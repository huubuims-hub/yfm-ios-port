"""Pinned static iOS ARM64 dependencies; no package manager required.

EXPERIMENTAL (notes/ios-port.md). The same archives and options as
macos_deps.py, configured for the iPhoneOS SDK. Needs an Apple host (Xcode's
command line tools): `xcrun --sdk iphoneos` must work.
"""

import hashlib
import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

from build_config import IOS_MINIMUM
from dependency_archives import ARCHIVES
from fetch_tools import ensure as ensure_tool

import macos_deps  # source() fetches and unpacks the pinned archives once

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tmp/pc/ios-deps"
INSTALL = OUT / "install"


def sdk_path():
    sdk = Path(
        subprocess.check_output(
            ["xcrun", "--sdk", "iphoneos", "--show-sdk-path"], text=True
        ).strip()
    )
    if not (sdk / "usr/include/stdlib.h").is_file():
        raise RuntimeError(f"xcrun returned an invalid iPhoneOS SDK: {sdk}")
    return sdk


OPTIONS = {
    "zstd": [
        "-DZSTD_BUILD_SHARED=OFF",
        "-DZSTD_BUILD_STATIC=ON",
        "-DZSTD_BUILD_PROGRAMS=OFF",
        "-DZSTD_BUILD_TESTS=OFF",
    ],
    "zlib": [
        "-DZLIB_BUILD_SHARED=OFF",
        "-DZLIB_BUILD_TESTING=OFF",
        "-DBUILD_SHARED_LIBS=OFF",
    ],
    "libpng": [
        "-DPNG_SHARED=OFF",
        "-DPNG_STATIC=ON",
        "-DPNG_TESTS=OFF",
        "-DPNG_TOOLS=OFF",
        "-DPNG_FRAMEWORK=OFF",
        "-DPNG_ARM_NEON=off",
        f"-DZLIB_LIBRARY={INSTALL}/lib/libz.a",
        f"-DZLIB_INCLUDE_DIR={INSTALL}/include",
    ],
    "freetype": [
        "-DBUILD_SHARED_LIBS=OFF",
        "-DFT_DISABLE_ZLIB=ON",
        "-DFT_DISABLE_BZIP2=ON",
        "-DFT_DISABLE_PNG=ON",
        "-DFT_DISABLE_HARFBUZZ=ON",
        "-DFT_DISABLE_BROTLI=ON",
    ],
    "sdl-source": [
        "-DSDL_STATIC=ON",
        "-DSDL_SHARED=OFF",
        "-DSDL_TESTS=OFF",
        "-DSDL_TEST_LIBRARY=OFF",
        "-DSDL_EXAMPLES=OFF",
        "-DSDL_INSTALL=ON",
    ],
}
LIBRARIES = dict(
    zip(OPTIONS, ("libzstd.a", "libz.a", "libpng16.a", "libfreetype.a", "libSDL3.a"))
)


def ensure(names=("zstd", "zlib", "libpng", "freetype", "sdl-source"), jobs=8):
    if platform.system() != "Darwin":
        raise RuntimeError("the iOS dependency builder requires a Mac (Xcode)")
    from llvm_guest import toolchain

    llvm = toolchain()
    sdk = sdk_path()
    cmake, ninja = ensure_tool("cmake", pinned=True), ensure_tool("ninja", pinned=True)

    def tool_identity(path, *version_args):
        executable = Path(path).resolve()
        version = subprocess.check_output(
            [str(executable), *version_args], text=True
        ).splitlines()[0]
        return {"path": str(executable), "version": version}

    compiler_identity = {
        "clang": tool_identity(llvm / "bin/clang", "--version"),
        "cmake": tool_identity(cmake, "--version"),
        "ninja": tool_identity(ninja, "--version"),
    }
    for name in names:
        directory = macos_deps.source(name)
        if name == "zstd":
            directory /= "build/cmake"
        build = OUT / "build" / name
        stamp = build / "complete.json"
        command = [
            str(cmake),
            "-S", str(directory),
            "-B", str(build),
            "-G", "Ninja",
            f"-DCMAKE_MAKE_PROGRAM={ninja}",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_C_COMPILER={llvm}/bin/clang",
            f"-DCMAKE_CXX_COMPILER={llvm}/bin/clang++",
            "-DCMAKE_SYSTEM_NAME=iOS",
            "-DCMAKE_OSX_ARCHITECTURES=arm64",
            f"-DCMAKE_OSX_DEPLOYMENT_TARGET={IOS_MINIMUM}",
            f"-DCMAKE_OSX_SYSROOT={sdk}",
            # The compiler cannot link an iOS executable during CMake's probes.
            "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
            f"-DCMAKE_INSTALL_PREFIX={INSTALL}",
            f"-DCMAKE_PREFIX_PATH={INSTALL}",
            f"-DCMAKE_FIND_ROOT_PATH={INSTALL};{sdk}",
            "-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY",
            "-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY",
            "-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY",
            "-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER",
            "-DCMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH=OFF",
            "-DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH=OFF",
            "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
            "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF",
            f"-DCMAKE_SYSTEM_PREFIX_PATH={INSTALL};/usr",
            "-DPKG_CONFIG_EXECUTABLE=/usr/bin/false",
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
            *OPTIONS[name],
        ]
        fingerprint = json.dumps(
            {
                "command": command,
                "archive": ARCHIVES[name][1],
                "builder": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                "compiler_identity": compiler_identity,
            },
            sort_keys=True,
        )
        if (
            stamp.is_file()
            and stamp.read_text() == fingerprint
            and (INSTALL / "lib" / LIBRARIES[name]).is_file()
        ):
            continue
        shutil.rmtree(build, ignore_errors=True)
        environment = {
            key: value
            for key, value in os.environ.items()
            if key
            not in (
                "CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "LIBRARY_PATH",
                "CFLAGS", "CXXFLAGS", "LDFLAGS", "SDKROOT", "IPHONEOS_DEPLOYMENT_TARGET",
                "MACOSX_DEPLOYMENT_TARGET",
            )
        }
        subprocess.run(command, check=True, env=environment)
        subprocess.run([str(cmake), "--build", str(build), "--parallel", str(jobs)],
                       check=True, env=environment)
        subprocess.run([str(cmake), "--install", str(build)], check=True, env=environment)
        if not (INSTALL / "lib" / LIBRARIES[name]).is_file():
            raise RuntimeError(f"{name} did not install {LIBRARIES[name]}")
        stamp.write_text(fingerprint)
    return INSTALL
