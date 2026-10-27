# iOS log file, HD read-ahead limit and touch controls

Three changes for devices with no Mac at hand. All logic that can run on Linux
was run there (`tests/pc/touch_pad_test.c`, a render preview of the buttons,
`-fsyntax-only` of the iOS-only code against stub headers); nothing has been
built with the iPhoneOS SDK or run on a device yet.

## 1. Log file in Documents (`src/pc/platform/ios_log.[ch]`)

* `main()` calls `IosLog_Start()` first. stdout and stderr are redirected to
  `Documents/logs/log.txt`; the previous launch's file is kept as
  `log-previous.txt`. The Files app shows both (On My iPhone > YFM Re-Decomp >
  logs). Crash and hang reports already went to `Documents/reports`.
* The file opens with device model, iOS version and build, RAM, the user folder
  and the contents of `settings.txt`.
* Every 10 s a line records the memory the app holds (`phys_footprint`, which is
  what the system counts), the memory left before it is ended
  (`os_proc_available_memory`) and the thermal state. A slowdown shows as
  `thermal fair/serious`; a kill for memory shows as the last lines before the
  file ends, with "left" near 0.
* Lifecycle lines: background, foreground, the system's low-memory warning and
  "terminating". A normal exit writes `exit`.
* `Documents/trace.txt` with channel names on its first line (`frames,window`,
  or `all`) turns on the existing trace channels (`MEMORIES_TRACE`) in the log.
  `frames` adds the present phases and texture pack timings every 120 frames.
* The file is cut and restarted past 4 MB. `MEMORIES_NO_LOG_FILE=1` keeps the
  output on the console (Xcode).

## 2. HD pack read-ahead by RAM (`texture_pack.c`)

iOS used the PC's 192 MB. Now it is a 64th of the device's RAM, between 16 and
192 MB: 32 MB on a 2 GB iPhone 6s, 48 on 3 GB, 64 on 4 GB, 96 on 6 GB,
128 on 8 GB (`MEMORIES_PACK_READ_AHEAD` still overrides). A low-memory warning
from the system (`SDL_EVENT_LOW_MEMORY`) frees what was read ahead and cuts the
limit to a quarter (`TexturePack_LowMemory`).

Limit of the change: it bounds only images read *ahead* of use. The images the
game is using, and the GL textures made from the pack (`entry_textures` in
`gl_picture.c`, uncompressed RGBA8, freed only when the pack reloads), are not
counted in it. If the 6s still runs out of memory with HD on, those are next
(see `ios-perf-iphone6s.md`).

## 3. Touch controls (`touch_pad.[ch]`, `touch_pad_art.[ch]`, `sdl.c`)

* On iOS the window is in points, one to the dp, but `touch_density()` knew
  only Android, so the buttons were 13% of the short side with no limit: about
  133 points on a 12.9" iPad. The pad now gets density 1.0 on iOS, so a button
  is 48 to 80 points, and on a tablet (short side of 600 points or more) at
  most 64 (`TouchPad_SetTabletLimit`), since the 4:3 picture fills an iPad's
  screen and the buttons sit over it. Only the pad uses this; the menu keeps its
  scale. Android is unchanged (no tablet limit set). The safe-area insets are the repository's own (SET_SAFE_AREA).
* The buttons are drawn as vectors (distance fields with a one-pixel soft
  edge) at the size they have, once per size and state, then blended. They no
  longer enlarge the game's 16 x 16 sprites with nearest-texel sampling, which
  was the coarse, stair-stepped look, and need nothing from the disc. Colors:
  Triangle green, Circle red, Cross blue, Square pink; held buttons fill in.
* The overlay is still the size of the window in points and is stretched by
  the GL quad (bilinear), so on a 2x screen edges are smooth but not as sharp as
  a pixel-resolution overlay would be. Making the overlay canvas pixel-sized
  would sharpen the menu and the pad together; it touches all menu code and is
  not done here.
