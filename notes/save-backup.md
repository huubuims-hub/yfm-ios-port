# Backing up and restoring saves (iOS)

Deleting the app on iOS deletes its Documents folder, and the saves with it.
File > **Back up saves** and File > **Restore saves...** (iOS builds only) make
and use a copy.

* A backup is one file, `backups/yfm-backup-YYYYMMDD-HHMMSS.yfmbak`, in the
  Documents folder (Files > On My iPhone > YFM Re-Decomp > backups). It holds
  everything under `saves/` (slots, deck slots, what mods keep beside them) and
  `settings.txt` and `controls.txt`. The disc image, HD pack and mods are not in
  it. A few KB each; the newest 20 the game made are kept.
* The folder is inside the app, so it goes with the app too. After **Back up
  saves** the game offers **Save to Files...**: the system's own sheet
  (`UIDocumentPickerViewController`, `ios_files.c`), where the player picks
  iCloud Drive, Google Drive (if its app is installed), a folder on the phone
  or any other place the Files app reaches. A copy is saved there and stays
  when the app is deleted. No entitlement or paid account is needed: the system
  process does the picking.
* **Restore saves...** asks where from: "In the app" (the newest three in
  `backups/`) or "Files..." (the same sheet in the other direction: the system
  copies the chosen file into the app, it is checked and imported into
  `backups/`, `SaveBackup_Import`, and the restore goes on as below).
* Restore lists the newest three backups, asks to confirm and records the
  choice. The restore itself is done at the **next launch**, before the game
  reads anything (`SaveBackup_ApplyPending`, first thing in main after the log
  starts), because the running game would write its own copy over a restored
  file. The app cannot restart itself on iOS, so the player closes it and opens
  it again. A report (files restored, or why not) is shown as a notice then and
  is in `logs/log.txt`.
* Before anything is written the saves as they are get a backup of their own
  (`...-before-restore.yfmbak`), so a restore can be undone with another one.
  Files in the backup replace those of the same name; no other file is removed.
* A backup is checked completely first: a header, a CRC-32 per file, no cut
  off, sizes within limits, and every name a plain path inside `saves/` or one
  of the two settings files (`..`, absolute paths, other folders are refused).
  A damaged file is never partly applied.

Format: `YFMBAK01`, then per file `u32 name length, name, u64 size, data, u32
CRC-32`, ending with a `u32` zero; numbers little-endian.

Code: `src/pc/saves/save_backup.[ch]` (POSIX; built for iOS and by the unit test
`tests/pc/save_backup_test.c`), menu actions and the once-a-frame poll of the
sheet in `menu.c`, the sheet itself in `platform/ios_files.[ch]` (C against the
Objective-C runtime, like `ios_log.c`, so no Objective-C file is added to the
build; `Platform_UIKitWindow` in `sdl.c` gives it the UIWindow), the launch
hook in `guest/main.c`.

Untested on a device: the sheet is written from the documented UIKit API
(`initForExportingURLs:asCopy:`, `initForOpeningContentTypes:asCopy:`, the two
`UIDocumentPickerDelegate` methods) and has only been syntax-checked against
stub headers. If it fails to open, `IosFiles_Error` says why in the notice and
the backup stays in the app's `backups` folder. Google Drive appears in the
sheet only when its own app is installed and signed in.
