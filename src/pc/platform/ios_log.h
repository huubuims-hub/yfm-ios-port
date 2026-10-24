#ifndef MEMORIES_PC_IOS_LOG_H
#define MEMORIES_PC_IOS_LOG_H
/* A log file an iPhone's owner can read without a Mac (notes/ios-log.md).
 *
 * From the first line of main, the game's stdout and stderr go to
 * Documents/logs/log.txt, which the Files app shows (On My iPhone > YFM
 * Re-Decomp > logs). The log of the launch before is kept beside it as
 * log-previous.txt, so a hang, a kill by the system or a crash that ended
 * the app leaves its last lines to be read at the next launch. Crash and hang
 * reports (debug/crash.c) already go to Documents/reports.
 *
 * It opens with the device, the system, the memory and the settings file,
 * and a line every 10 seconds says how much memory the app holds and has
 * left and how hot the phone is: a slowdown or a kill shows which it was.
 *
 * Documents/trace.txt, if it exists, holds trace channels (the names
 * MEMORIES_TRACE takes, e.g. "frames,window" or "all") the log then also
 * records. MEMORIES_NO_LOG_FILE=1 keeps the output on the console, as in
 * Xcode.
 *
 * Only on iOS: elsewhere these do nothing. */

/* First thing in main, before Log_Init. */
void IosLog_Start(void);
/* One line in the log, with the time since launch. */
void IosLog_Note(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif
