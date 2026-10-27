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

Picker types: "Open from Files" accepts `public.item` and `public.data`. With
`public.data` alone a `.yfmbak` kept in iCloud Drive showed greyed out (not
selectable) in one report, while the same file kept On My iPhone could be
chosen; a service may describe a file whose extension the system does not
know as a plain item. If a file is still greyed out, it is usually one iCloud
has not finished uploading ("Upload Error" in the Files app), which no app can
read.

## Other systems (Android, Linux, macOS; not Windows)

File > Back up saves and Restore saves... are in the File menu everywhere but
Windows (`MEMORIES_SAVE_BACKUP_UI` in `menu.c`). `save_backup.c` is POSIX
(lstat, mkdir with a mode, rename over a file) and is not built on Windows,
whose paths, UTF-8 file names and rename would each need their own code and a
Windows machine to test; the saves there are plain files in the user folder.

Where iOS has UIDocumentPickerViewController, the others use SDL's file dialogs
(`SDL_ShowSaveFileDialog` / `SDL_ShowOpenFileDialog`) behind the same
`IosFiles_*` calls (`ios_files.c`): on Android the system's document picker, so
Google Drive and any other provider are in it, and the answer is a content://
URI that is copied with `SDL_IOFromFile`, once a frame, when the menu asks
for the state. Android deletes the app's folder on uninstall too, and Android
11 and later hide `Android/data` from file managers, so this dialog is the way
a backup leaves the phone. `SDL_ShowSaveFileDialog` is added to the JNI guard
list (`jni_guard.h`).

The restore at launch (`SaveBackup_ApplyPending`) runs in `main()` after the
crash monitor has become the game's process on a desktop, and first thing on
iOS. Its report is also kept in `restore-report.txt`, as the process that shows it
is not always the one that applied it.

Untested on a device or a desktop dialog; only the shared logic has a test.

# Startup recovery on every system

`recovery.c` (reset the graphics settings after a start that never finished,
`startup-check.txt`, `settings.txt.bak`) was iOS-only and now runs on Android
and the desktops as well. `MEMORIES_NO_RECOVERY=1` or `MEMORIES_HEADLESS` turns
it off (an automated run has no one to ask).

# Android launcher icon

`tools/pc/android_assets/res/mipmap-*/ic_launcher.png` (48 to 192 px, from
`tools/pc/ios_assets/AppIcon-1024.png`); `package_android.py` compiles the folder
with `aapt2 compile` and links it, and the manifest names it
(`android:icon="@mipmap/ic_launcher"`). Windows' executable and macOS' app have no icon from here:
`package_macos.py` takes an .icns when one is given, and a Windows icon would need a resource file.
