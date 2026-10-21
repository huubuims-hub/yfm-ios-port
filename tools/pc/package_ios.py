#!/usr/bin/env python3
"""Stage an unsigned-for-distribution iOS .app and .ipa (EXPERIMENTAL).

notes/ios-port.md. The .ipa is ad-hoc signed only so that it is a well-formed
bundle; install it with a sideloading tool (AltStore, SideStore, Sideloadly)
that re-signs it with your own Apple ID. It holds no game disc.
"""

import argparse
import hashlib
import json
import plistlib
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

from build_config import IOS_MINIMUM

ROOT = Path(__file__).resolve().parents[2]
APP = "YFM Re-Decomp.app"
EXECUTABLE = "memories-ios"
BUNDLE_ID = "org.yfmredecomp.ios"


def inspect_binary(path):
    """Fail early unless this is an arm64 Mach-O executable built for iOS."""
    data = path.read_bytes()
    magic, cpu, _, kind, count, commands, _, _ = struct.unpack_from("<8I", data)
    if (magic, cpu, kind) != (0xFEEDFACF, 0x0100000C, 2):
        raise ValueError("expected a native ARM64 Mach-O executable")
    offset, platform = 32, None
    for _ in range(count):
        command, size = struct.unpack_from("<II", data, offset)
        if command == 0x32:  # LC_BUILD_VERSION
            platform = struct.unpack_from("<I", data, offset + 8)[0]
        offset += size
    if platform != 2:  # PLATFORM_IOS
        raise ValueError(f"expected an iOS executable (LC_BUILD_VERSION platform 2), got {platform}")
    return data


def stage_mods(app, game):
    """Copy the repository's own mods into the bundle. Each code mod's dylib is
    built for iOS and goes to Frameworks/<id>.dylib, where mods.c loads it
    (ios_library_path) and where sideloading tools sign it with the app.
    Mods a player adds later can only be data mods: iOS runs no unsigned code."""
    from build_mod_arm64 import build

    tracked = subprocess.check_output(
        ["git", "ls-files", "-z", "mods"], cwd=ROOT, text=True
    ).split("\0")
    files = [Path(n) for n in tracked if n and Path(n).suffix not in (".c", ".h", ".o", ".dylib")]
    for relative in files:
        destination = app / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / relative, destination)
    frameworks = app / "Frameworks"
    libraries = []
    for relative in files:
        if relative.name != "mod.json":
            continue
        manifest = json.loads((ROOT / relative).read_text(encoding="utf-8-sig"))
        if not manifest.get("library"):
            continue
        scratch = ROOT / "tmp/pc/ios-mods" / manifest["id"]
        shutil.rmtree(scratch, ignore_errors=True)
        built = build(ROOT / relative.parent, scratch, game=game, ios=True)
        frameworks.mkdir(exist_ok=True)
        target = frameworks / f"{manifest['id']}.dylib"
        shutil.copyfile(built, target)
        target.chmod(0o755)
        subprocess.run(["codesign", "--force", "--sign", "-", str(target)], check=True)
        libraries.append(target)
    return files, libraries


def package(binary, out, label):
    if sys.platform != "darwin":
        raise ValueError("iOS packaging requires codesign and zip on a Mac")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", label):
        raise ValueError("version must be a filename-safe release label")
    inspect_binary(binary)
    stage = out / "stage"
    shutil.rmtree(stage, ignore_errors=True)
    app = stage / "Payload" / APP
    app.mkdir(parents=True)
    shutil.copy2(binary, app / EXECUTABLE)
    (app / EXECUTABLE).chmod(0o755)
    # The app looks for its read-only files next to the executable (SDL's
    # base path on iOS is the bundle folder).
    languages = app / "languages"
    languages.mkdir()
    for name in ("en-eu", "fr", "de", "it", "es"):
        shutil.copyfile(ROOT / f"languages/{name}.txt", languages / f"{name}.txt")
    mod_files, mod_libraries = stage_mods(app, binary.parent)
    shutil.copyfile(ROOT / "LICENSE", app / "LICENSE.txt")
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    release = re.match(r"^v?(\d+\.\d+\.\d+)(?:[-.].*)?$", label)
    info = dict(
        CFBundleName="YFM Re-Decomp",
        CFBundleDisplayName="YFM Re-Decomp",
        CFBundleIdentifier=BUNDLE_ID,
        CFBundleExecutable=EXECUTABLE,
        CFBundlePackageType="APPL",
        CFBundleVersion="1",
        CFBundleShortVersionString=release.group(1) if release else "0.0.0",
        CFBundleSupportedPlatforms=["iPhoneOS"],
        MinimumOSVersion=IOS_MINIMUM,
        UIDeviceFamily=[1, 2],
        UIRequiredDeviceCapabilities=["arm64"],
        UISupportedInterfaceOrientations=[
            "UIInterfaceOrientationLandscapeLeft",
            "UIInterfaceOrientationLandscapeRight",
        ],
        UIRequiresFullScreen=True,
        UIStatusBarHidden=True,
        UIViewControllerBasedStatusBarAppearance=False,
        UILaunchScreen={},
        # Documents shows up in the Files app under "On My iPhone", so a
        # disc image (.bin) can be copied in without a picker in the game.
        UIFileSharingEnabled=True,
        LSSupportsOpeningDocumentsInPlace=True,
        PackageVersion=label,
        SourceRevision=revision,
    )
    (app / "Info.plist").write_bytes(plistlib.dumps(info))
    (app / "PkgInfo").write_bytes(b"APPL????")
    manifest = dict(
        version=label,
        revision=revision,
        architecture="arm64",
        minimum_ios=IOS_MINIMUM,
        signing="ad-hoc (re-sign when sideloading)",
        mod_files=sorted(str(p) for p in mod_files),
        code_mod_libraries=sorted(str(p.relative_to(app)) for p in mod_libraries),
        binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
    )
    (app / "build.json").write_text(json.dumps(manifest, indent=2) + "\n")
    subprocess.run(["codesign", "--force", "--sign", "-", str(app)], check=True)
    subprocess.run(["codesign", "--verify", "--strict", str(app)], check=True)
    ipa = out / f"yfm-redecomp-{label}-ios-arm64.ipa"
    ipa.unlink(missing_ok=True)
    subprocess.run(["zip", "-qry", str(ipa.resolve()), "Payload"], cwd=stage, check=True)
    (out / (ipa.name + ".sha256")).write_text(
        f"{hashlib.sha256(ipa.read_bytes()).hexdigest()}  {ipa.name}\n"
    )
    print(ipa)
    return ipa


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "tmp/pc/ios-arm64-translated/memories-ios")
    parser.add_argument("--out", type=Path, default=ROOT / "tmp/pc-release/ios-arm64")
    parser.add_argument("--version", default=None)
    args = parser.parse_args()
    if Path.cwd().resolve() != ROOT:
        parser.error("run from the repository root")
    label = args.version or "dev-" + subprocess.check_output(
        ["git", "rev-parse", "--short=12", "HEAD"], cwd=ROOT, text=True
    ).strip()
    args.out.mkdir(parents=True, exist_ok=True)
    package(args.binary.resolve(), args.out.resolve(), label)


if __name__ == "__main__":
    main()
