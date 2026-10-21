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

/* An iOS app cannot re-execute itself; callers treat a negative result as
 * "restart failed" and tell the player to reopen the app. */
int Platform_RestartGame(void)
{
    return -1;
}
#endif
