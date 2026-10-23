# iOS app icon

`AppIcon-1024.png` is the master (1024x1024, RGB, no transparency: iOS rounds the
corners itself). `icons/` holds it scaled to the sizes an iPhone and an iPad ask
for; `tools/pc/package_ios.py` copies them into the app and lists them in
Info.plist (`CFBundleIcons`).

`make_icon.py` draws the master (`python make_icon.py violet AppIcon-1024.png`;
a `red` palette is there too). It needs Pillow and numpy and two fonts, both
SIL Open Font License 1.1, which are not kept in the repository: Orbitron
(variable weight, `Orbitron.ttf`) for "YFM1" and Rajdhani Bold
(`Rajdhani-Bold.ttf`) for "iOSRecompile", from github.com/google/fonts (ofl/).
The icon holds no artwork from the game.
