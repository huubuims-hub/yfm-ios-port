#ifndef MEMORIES_PC_SAVE_BACKUP_H
#define MEMORIES_PC_SAVE_BACKUP_H
/* Backups of the player's saves (notes/save-backup.md).
 *
 * A backup is one file, root/backups/yfm-backup-YYYYMMDD-HHMMSS.yfmbak, with
 * everything under saves/ (the slots and what mods keep beside them) and
 * settings.txt and controls.txt. The disc image, the HD pack and the mods are
 * not in it. On iOS deleting the app deletes its Documents folder with the
 * saves, so the backups folder is the one to copy out with the Files app
 * (to iCloud Drive or anywhere else) and to put back after a fresh install.
 *
 * Restoring is done at the next launch, before the game reads anything:
 * SaveBackup_Stage checks the file and records it, SaveBackup_ApplyPending
 * (first thing in main) writes it over the saves. The saves as they were are
 * backed up first ("-before-restore"), files in the backup replace those of
 * the same name and no other file is removed.
 *
 * A file is checked completely (its checksums, and that every name is a
 * plain path inside saves/ or one of the two settings files) before a byte
 * of the live folder changes. `root` is the user folder (Paths_UserDir).
 * Nothing here needs the game: it can be tested on its own. Main thread. */
#include <stddef.h>

#define SAVE_BACKUP_EXT ".yfmbak"
#define SAVE_BACKUP_NAME 96

typedef struct SaveBackupInfo {
    char name[SAVE_BACKUP_NAME]; /* the file's name in the backups folder */
    char shown[24];              /* "2026-10-24 21:05", from the name, else the file's date */
    long long size;
} SaveBackupInfo;

/* Makes a backup, naming the new file in `name` (label, if not NULL, goes at the
 * end of its name) and the number of files in `files`. 0 on success, else -1
 * with the reason in `why`. Keeps the newest 20 files the game named. */
int SaveBackup_Create(const char *root, const char *label, char *name, size_t name_size, int *files, char *why,
                      size_t why_size);
/* Takes a backup file from anywhere (the system's Open from Files hands the
 * app a copy in its temporary folder) into the backups folder, after
 * checking it completely: under its own name when that is a plain backup
 * name not taken yet, else as yfm-backup-imported[-n].yfmbak. `info` (may be
 * NULL) describes the new file. 0, or -1 with the reason and nothing added. */
int SaveBackup_Import(const char *root, const char *source, SaveBackupInfo *info, char *why, size_t why_size);
/* The backups there are, newest first; the number found (at most `max`). */
int SaveBackup_List(const char *root, SaveBackupInfo *list, int max);
/* 0 when the file is whole and safe to restore, else -1 and why. */
int SaveBackup_Verify(const char *root, const char *name, int *files, char *why, size_t why_size);
/* Verifies, then records that the next launch restores it. 0 or -1. */
int SaveBackup_Stage(const char *root, const char *name, char *why, size_t why_size);
/* First thing at launch: 1 when a staged restore was done, -1 when it failed
 * (nothing was changed, or only part; `report` says), 0 when none was staged. */
int SaveBackup_ApplyPending(const char *root, char *report, size_t report_size);
/* The report of the restore ApplyPending did, once: 1 and the text, else 0. */
int SaveBackup_TakeResult(char *text, size_t size);

#endif
