/* iOS-only platform services (notes/ios-port.md). EXPERIMENTAL.
 *
 * Stage 1 provides only what main.c leaves out on iOS. The rest of the
 * platform layer (sdl.c's touch controls, the disc picker, orientation,
 * the HD-pack download) is Android-specific today and is ported in later
 * stages, guided by the compile log of tools/pc/build.py --target
 * ios-arm64-translated.
 */
typedef int memories_ios_translation_unit_is_not_empty;

#ifdef MEMORIES_IOS
#include "platform.h"
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>

/* clang lowers @available / __builtin_available to __isPlatformVersionAtLeast,
 * which lives in compiler-rt's libclang_rt.ios.a. The pinned LLVM is a macOS
 * release with no iOS runtime, and libSDL3.a's UIKit code calls this (first
 * link on a macOS runner: "Undefined symbols ... ___isPlatformVersionAtLeast").
 * This is compiler-rt's os_version_check.c in short: dyld answers. */
typedef struct {
    uint32_t platform;
    uint32_t version;
} ios_build_version_t;

int32_t __isPlatformVersionAtLeast(uint32_t platform, uint32_t major, uint32_t minor, uint32_t subminor)
{
    typedef bool (*check_t)(uint32_t, ios_build_version_t[]);
    static check_t check;
    static int looked_up;
    ios_build_version_t versions[1];
    if (!looked_up) {
        check = (check_t)dlsym(RTLD_DEFAULT, "_availability_version_check");
        looked_up = 1;
    }
    if (!check) return 1; /* dyld older than iOS 12: the app's minimum is 15 */
    versions[0].platform = platform;
    versions[0].version = ((major & 0xffff) << 16) | ((minor & 0xff) << 8) | (subminor & 0xff);
    return check(1, versions);
}

/* OpenGL ES only (UIKit has no desktop OpenGL): sdl.c then shows the game
 * through SDL's renderer in an ES 3.0 context, as on Android. */
int Platform_HasDesktopGL(void)
{
    return 0;
}

/* An iOS app cannot re-execute itself; callers treat a negative result as
 * "restart failed" and tell the player to reopen the app. */
int Platform_RestartGame(void)
{
    return -1;
}
#endif
