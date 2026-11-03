msr-native - Metroid: Samus Returns on PC, without an emulator
=================================================================

The game's code runs as native code that is built on your PC from your own copy of the
game. A small 3DS runtime of its own stands in for the console: no emulator is involved.
Nothing of the game comes with this program.

You need
--------
- A decrypted dump of Metroid: Samus Returns (USA): a .3ds, .cci or .cxi file.
- A graphics card with Vulkan 1.3.
- About 1 GB of free space and an internet connection for the first build (it downloads
  the build tools: Zig, capstone and, on Windows, Python; about 105 MB).

First start
-----------
1. Windows: unpack the zip anywhere you can write to and start msr-native.exe.
   Linux / Steam Deck: make the AppImage executable and start it.
2. Game tab, "Game file": choose your dump with Browse... or type its path.
3. "Build the native code". This takes a few minutes the first time, then it is done.
4. Play.

The same menu opens in the game with F1 or the controller's Select / View / Back button.
The launcher comes back when the game closes.

Your files
----------
Everything goes in the user folder ("Open the user folder" on the Game tab):
- Windows: user\ beside msr-native.exe
- Linux: ~/.local/share/msr-native/user, or a user/ folder beside the AppImage if you make one
It holds your saves (native-save), settings, the native code and its build tools, game mods
(mods/game) and texture packs (mods/textures). Saves from an emulator go in
native-save/savedata/profile0, profile1 and profile2.

Controls
--------
Keyboard and mouse (change them in the menu's Controls tab):
  Move WASD             Jump Space             Shoot left mouse button or V
  Aim at the pointer    hold the right mouse button
  Free Aim Q            Missiles E             Morph Ball Left Shift
  Change beam C or the mouse wheel             Aeion F    Melee counter R
  Aeion abilities 1 2 3 4                      Pause menu Tab
  Touch navigation T    Settings F1            Quick save F5, quick load F8
  Fullscreen F11
Controllers are read by button position. The button icons in the game follow the device you
used last (Xbox, PlayStation, Nintendo or the keyboard's keys).

Licences
--------
msr-native is free software under the GNU GPL version 3 or later (LICENSE-GPL-3.0.txt). The
parts from the emulator-based Samus Returns port are GPL 2.0 or later (LICENSE-GPL-2.0.txt).
Dear ImGui and AMD FidelityFX FSR 1 are MIT. Metroid: Samus Returns belongs to Nintendo; this
program contains none of it.
