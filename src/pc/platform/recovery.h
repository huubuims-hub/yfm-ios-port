#ifndef MEMORIES_PC_RECOVERY_H
#define MEMORIES_PC_RECOVERY_H

/* Startup recovery (every system: iOS, Android, Windows, Linux, macOS).
 *
 * A graphics setting too demanding for the device (the internal resolution
 * above all) can leave the game hung, or killed by the system for memory, and every
 * launch then loads the same settings. A hung app cannot show a dialog, so
 * the check is made at the next launch: a marker file (startup-check.txt in
 * the user folder) is written when the settings are applied, or when a
 * graphics setting is changed, and removed once the game has run on screen
 * for a while or the app exits normally. A marker found at launch means the
 * last start never got there, and the player is asked whether to reset the
 * graphics settings (and optionally turn the mods off). MEMORIES_NO_RECOVERY=1,
 * or MEMORIES_HEADLESS, skips the check.
 */

/* Right after Settings_Load(), before any setting is applied. */
void Recovery_AtStart(void);
/* Once per game frame. */
void Recovery_Frame(void);

#endif
