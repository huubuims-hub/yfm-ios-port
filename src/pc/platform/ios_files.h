#ifndef MEMORIES_PC_IOS_FILES_H
#define MEMORIES_PC_IOS_FILES_H
/* The system's file sheets on iOS: "Save to Files" for a backup (so it can go to
 * iCloud Drive, Google Drive, a folder on the phone, anywhere the Files app
 * reaches, and outlive the app) and "Open from Files" to bring one back.
 * They are UIDocumentPickerViewController, which needs no entitlement and no
 * paid account: the system does the picking and takes (or hands over) a copy.
 *
 * IosFiles_Save and IosFiles_Open show the sheet and return at once. The
 * answer comes later, on the main thread, while the event pump runs; the
 * caller asks for it once a frame with IosFiles_State until it is no longer
 * BUSY, reads it (IosFiles_Path, IosFiles_Error) and then calls
 * IosFiles_Done, which makes the next sheet possible. One sheet at a time.
 *
 * Written in C against the Objective-C runtime, as ios_log.c is, so the build
 * needs no Objective-C file. Main thread only. Elsewhere both fail. */

typedef enum {
    IOS_FILES_IDLE,      /* no sheet, nothing waiting to be read */
    IOS_FILES_BUSY,      /* the sheet is up */
    IOS_FILES_PICKED,    /* the player chose: IosFiles_Path */
    IOS_FILES_CANCELLED, /* the player closed the sheet */
    IOS_FILES_FAILED     /* IosFiles_Error says why */
} IosFilesState;

/* Save the file at `path` through the sheet (a copy is made where the
 * player chooses; the file stays). 0 when the sheet is up, else -1. */
int IosFiles_Save(const char *path);
/* Pick a file: the system copies it into the app's temporary folder first,
 * IosFiles_Path is that copy, which the caller deletes after using it. 0 or -1. */
int IosFiles_Open(void);
IosFilesState IosFiles_State(void);
/* The chosen file (Save: where the copy went; Open: the temporary copy). */
const char *IosFiles_Path(void);
const char *IosFiles_Error(void);
/* Read: back to IDLE. */
void IosFiles_Done(void);

#endif
