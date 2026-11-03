# msr-native

Metroid: Samus Returns on PC without an emulator, for private testing. The game's ARM code is recompiled from your
own dump into a library (`msr_aot.dll`), and a small runtime stands in for the 3DS: kernel, system services, the
PICA200 GPU (Vulkan), the DSP, input and the window. The runtime comes from
[Woohoo! Flight School](https://github.com/thefixinhixon/woohoo-flight-school). Samus Returns' own features are on
top: the 16:9 single screen with the HUD, 30-144 FPS, up to 10x internal resolution, HDR, texture packs, game mods,
the randomizer, save states, cheats, mouse aiming and per-device button icons. `native/README.md` has the details.

**No game code or data is in this repository.** The dump, the library made from it, saves and texture packs live in
`run/` and `test/`, which git ignores.

## Layout

| Folder | What |
| --- | --- |
| `native/` | msr-native: the runtime (`runtime/`), Samus Returns' features (`runtime/msr/`), shaders, CMake |
| `src/msr/` | Samus Returns code shared with the emulator-based build: settings menu, touch navigation, HUD layout, button icons, cheats, frame-rate patches |
| `src/common/`, `src/core/arm/` | the few helpers those need, the recompiled module's interface (`msr_aot/aot_runtime.h`) and the ARM11 timing table |
| `externals/msr-imgui/` | Dear ImGui (MIT), unmodified |
| `tools/msr/recomp/` | the recompiler: your dump in, `msr_aot.dll` out |
| `tools/dev/shot.ps1` | test driver: starts the game from `test/` in the background, sends keys and mouse input, takes screenshots |
| `run/` | your play folder (not in git) |
| `test/` | the test folder (not in git) |

## Build (Windows)

You need MSYS2 UCRT64 with these packages: `mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,SDL2,libpng,zlib,shaderc,vulkan-headers,vulkan-loader}`.
The Vulkan SDK also works for the Vulkan headers and glslc.

```sh
# in an MSYS2 UCRT64 shell, or with C:\msys64\ucrt64\bin first in PATH
cmake -S native -B build -G Ninja
cmake --build build -j 8
```

Keep builds at `-j 8` or less on this PC: the i9-14900K resets under all-core loads until its BIOS is updated.

## The game's code

`tools/msr/recomp/build_module.py` checks the dump, recompiles its code and writes the library. Supported dump:
Samus Returns (USA), decrypted. The `--kit` folder holds the compiler tools (Zig, Python with capstone), as the
emulator-based build's launcher downloads them. `run/user/msr/recomp-kit` is a copy of yours. Use
`--cxx g++` to build with MSYS2's compiler instead.

```sh
python tools/msr/recomp/build_module.py --dump "<dump.cci>" --user run/user --kit run/user/msr/recomp-kit --jobs 8
```

The library lands in `run/user/msr/recomp/`. Rebuild it whenever `src/core/arm/msr_aot/aot_runtime.h` or the
recompiler changes. msr-native refuses a library built against another `aot_runtime.h`.

## Play

```powershell
.\run.ps1                       # 9x internal resolution, the settings' frame rate
.\run.ps1 -Resolution 4 -Fps 60 -Fullscreen
```

`run.ps1` starts `build\msr-native.exe` in `run\` and restarts it when its menu asks to. The log is
`run\msr-native.log`. The play folder holds:

- `user\msr\game-path.txt`: the dump's path
- `user\msr\recomp\`: the library
- `user\msr\settings.ini`: settings, controls, texture-pack order
- `user\native-save\`: saves
- `user\mods\textures\`, `user\mods\game\`: texture packs, game mods
- `pwr_shaders.bin`, `pwr_pipelines.bin`: the shader cache

## Controls

Keyboard and mouse (rebind in F1 → Controls):

| Key | Does |
| --- | --- |
| WASD | Move |
| Space | Jump |
| Mouse left / V | Shoot |
| Mouse right (held) | Free Aim at the pointer |
| Q / E | Free Aim / ready missiles (L / R) |
| Left Shift | Morph Ball |
| C, mouse wheel | Change beam |
| F, Enter | Aeion, confirm |
| R | Melee counter |
| 1 2 3 4 | Aeion abilities |
| Tab | Pause menu |
| T | Touch navigation |
| F1, Backspace | Settings |
| F5 / F8 | Quick save / load |
| F11 | Fullscreen |

Gamepads are read by button position. The game's button icons and the hints beside the menus follow the device you
used last: Xbox, PlayStation, Nintendo or the keyboard's keys.

## Testing

`tools\dev\shot.ps1` runs the game from `test\` without taking the foreground. It posts keys (VK codes in hex) and
mouse input and saves screenshots to `test\shots\`:

```powershell
.\tools\dev\shot.ps1 -Seconds 14 -Shot title -Keys '46:200,wait6000,shot'
```

`test\` is set up like `run\`, with test saves; profile B is a 100% file. The script stops only the process it
started, so a game running from `run\` is safe. `native/README.md` lists the debugging switches (`PWR_*`).

## Licences

- `native/`: GPL-3.0-or-later (`native/LICENSE`), as Woohoo! Flight School.
- `src/` and `tools/`: GPL-2.0-or-later (`license.txt`), from the emulator-based Samus Returns port.
- Dear ImGui and AMD FidelityFX FSR 1: MIT.
- The UI font: DejaVu Sans Mono.

The program as built is GPL-3.0-or-later.
