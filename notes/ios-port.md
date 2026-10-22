# iOS port (EXPERIMENTAL, stages 1 and 2)

Status: **untested**. Nothing here has been built on a Mac yet. Stage 1 only
adds the build plumbing so that a macOS CI runner can build the iOS
dependencies, compile every game unit against the iPhoneOS SDK and report
every error in one run. Expect the first runs to fail; the logs say what
stage 2 has to fix.

## What stage 1 adds

| File | Change |
| --- | --- |
| `tools/pc/build_config.py` | `IOS_MINIMUM` (15.0) and the `ios-arm64-translated` target (translated 32-bit guest memory, as macOS and Android arm64: no fixed host address). |
| `tools/pc/guest_build.py` | `IosSdk` marker type; `darwin_flags()` returns `-target arm64-apple-ios15.0 -isysroot <iPhoneOS SDK>` for it; `guest_frontend_flags()` defines `MEMORIES_IOS=1`. |
| `tools/pc/ios_deps.py` | zstd, zlib, libpng, FreeType and SDL3 (static) for iOS, from the same pinned archives as `macos_deps.py`. |
| `tools/pc/build_arm64.py` | `--target ios-arm64-translated`: GLES presenter stub in place of desktop GL (`gl_desktop_none.c`), UIKit/OpenGLES frameworks, output `memories-ios`. |
| `tools/pc/package_ios.py` | Builds `YFM Re-Decomp.app` (Info.plist with landscape + Files-app sharing of Documents) and `yfm-redecomp-<v>-ios-arm64.ipa`. Code mods are left out (iOS forbids loading unsigned native code). |
| `src/pc/platform/ios.c` | `Platform_RestartGame()` stub (an iOS app cannot re-exec itself). |
| `src/pc/guest/main.c` | iOS: SDL's `SDL_main` entry, no crash-monitor child process (`MEMORIES_NO_MONITOR`), no re-exec restart. |
| `tools/pc/build_mod_arm64.py` | `build(..., ios=True)`: builds a code mod's dylib against the iPhoneOS SDK (`darwin_flags` now shared with the game's flags; the macOS flags are unchanged). |
| `src/pc/mods/mods.c` | iOS: a bundled code mod's dylib is loaded from `<app>/Frameworks/<mod id>.dylib` (`ios_library_path`), the one place sideloading tools sign. Mods a player adds can only be data mods. |
| `src/pc/platform/paths.c` | iOS: the user folder is the app's `Documents` itself (shown by the Files app); `mods/` and `game/` are created on first launch. Read-only resources (languages, shipped mods) stay in the bundle root, which `Paths_ProgramDir()` already returns for a flat `.app`. |
| `src/pc/platform/game_files.c` | iOS: the disc image is also looked for in `Documents/game` and `Documents`; the "not found" message says to use the Files app. |
| `src/pc/render/gl_headers.h` | SDL's `SDL_opengl.h` is empty on iOS ("No OpenGL on iOS"); this includes it as though the platform were not iOS so the GL names and function-pointer types the ES 3 path uses (as on Android) exist. Found by the first CI compile (`unknown type name 'GLuint'`). |
| `src/pc/render/gl_picture.c`, `sdl.c`, `platform_common.c`, `ios.c` | iOS takes the ES 3 / SDL-renderer path like Android: `Platform_HasDesktopGL()` is 0, the macOS core-profile attributes, `#version 150` shaders (the ES rewrite needs `#version 130`) and `PresentPass_Quad` calls are for macOS only; touch-style pointer and overlay panels as on Android. |
| `src/pc/platform/recovery.c` (+ `settings.c` `Settings_Default`, `menu.c`, `sdl.c`) | iOS startup recovery: a marker file `startup-check.txt` is written when the settings are applied (and when a graphics setting changes) and removed after 20 s / 120 frames on screen or a normal exit. A marker found at launch (hang, iOS kill, force-quit) asks: reset the graphics settings, also turn the mods off, or keep. The old `settings.txt` is kept as `settings.txt.bak`. Logic tested on Linux with stubs; the dialog itself (SDL_ShowMessageBox on UIKit before SDL_Init) is untested on a device. |
| `.github/workflows/ios-build.yml` | Manual workflow, stages `deps-only` / `compile-only` / `full`; uploads every log. |

## Mods and the HD pack on iOS

* The bundled code mods (3D Monsters, Hand Camera, AI Hard Mode, Yamyi Mods)
  are built as dylibs by `package_ios.py`, ad-hoc signed and put in
  `Frameworks/`. **Unverified**: that dyld accepts them once a sideloading
  tool has re-signed the app, and that the flat-namespace lookup of the game's
  exports (`-undefined dynamic_lookup`, `-Wl,-export_dynamic`) works on iOS.
  If it does not, the fallback is to link the four mods into the executable.
* The HD pack is a data mod (`forbidden-memories-hd`). The in-app download is
  Android-only (JNI). On iOS: download `yfm-redecomp-hd-mod-<tag>.zip` from
  the project's Releases, tap it in the Files app to uncompress, and move the
  folder into `On My iPhone > YFM Re-Decomp > mods`. `mod.json` must sit
  directly inside that folder.

## Known gaps

These are the Android-only code paths (`SDL_PLATFORM_ANDROID` in `sdl.c`,
`android.c`) that need an iOS counterpart. Which of them break the compile
is exactly what stage 1's log should show.

1. **Disc image selection UI.** Android copies the picked `.bin` into the app.
   On iOS the game finds a `.bin` put in the app's folder with the Files app;
   an in-app picker (`UIDocumentPicker`) is not done.
2. **Touch controls, orientation, safe areas** (`sdl.c` Android blocks).
3. **GLES context**: `gl_picture.c` already has an OpenGL ES 3 path; confirm
   SDL's EAGL context on iOS provides it, and that `Platform_HasDesktopGL()`
   returns 0 there.
4. **In-app HD pack download and "Import mod..."** (Android: JNI and the
   system picker).
5. **Distribution**: sideload only (AltStore, SideStore, Sideloadly). The
   game needs the user's own disc image; an App Store release is not
   realistic.
