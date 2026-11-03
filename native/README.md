# msr-native: Samus Returns without an emulator

`msr-native` runs Metroid: Samus Returns the way
[Woohoo! Flight School](https://github.com/thefixinhixon/woohoo-flight-school) runs Pilotwings Resort: the game's
ARM code is native code recompiled from the player's own dump, and everything around it (the 3DS kernel, the system
services, the PICA200 GPU, the DSP) is a small hand-written runtime instead of an emulator. Azahar is not part of it.

```
your dump ──> tools/msr/recomp/build_module.py ──> user/msr/recomp/msr_aot.dll   (the same library the launcher's
                                                                                  "Native code" Build makes)
msr-native.exe ── loads the dump's .code, ExHeader and RomFS straight from the dump file
               ── loads msr_aot.dll and checks it (module version, sizeof(Ctx), SHA-256 of aot_runtime.h)
               ── runtime/dispatch.cpp: one host thread per guest thread, function to function through the module
               ── runtime/kernel.c, services.c, fs.c, gsp.c, dsp.c, pica*.c, video.c, input.c (from Woohoo! Flight School)
```

## Status (2026-11-02)

**Works:** boot, the game's own start-up (kernel objects, threads, timers, srv/APT/fs/cfg/hid/gsp/dsp/ir/nfc),
loading from the RomFS inside the dump, the title screen, the file select with the player's saves, and gameplay
(walking, jumping, room transitions, the pause map) at 30, 60, 90, 120 or 144 FPS (`--fps`, else the settings'). Audio runs (no underruns in
tests). Tested on Windows with an RTX 5090 at 1x-10x internal resolution (4K = 9x), and on a 100% save.

**Samus Returns' own features** (`runtime/msr/`, C++ sharing `src/msr`'s headers with the emulator-based build):
- The player's settings: `user/msr/settings.ini`, as the launcher writes it (`--settings`; by default
  `user/msr/settings.ini`, or `../msr/settings.ini` from `user/native`). Its frame rate applies unless `--fps` says.
- The single 16:9 screen (`--layout=single`, F9 cycles to it): `compositor.cpp` ports the emulator build's compositor
  (renderer_vulkan.cpp DrawMsrScreens): the bottom screen is classified each frame (`bottom_classifier.h`) and its HUD
  pieces (energy, beam slots, the minimap following Samus, the counters), menus (a panel, the title menus without
  their backdrop), prompts and the gallery layout are drawn over the view by `msr.vert`/`msr.frag`. `pica_vk.c`
  squeezes the top screen's world draws to 16:9 and applies the camera zoom in clip space, and leaves out the title
  menus' backdrop quads. The mouse touches the panel while it shows.
- Input: RT taps the next beam slot, LT the minimap (Morph Ball), as injected touches while the HUD shows (C and
  Left Shift too, and the mouse wheel: down the next beam, up the one before); the pad's Back/Select opens the settings
  panel (`select_layout`: `settings`, or true for the next layout, false for 3DS Select). Every pad SDL can map is
  open; the one used last is read.
- The buttons as the player's device names them (`msr/glyphs.cpp`, `src/msr/button_glyphs.h`), from the last input
  (a pad's button or a stick pushed past half way, a key, a mouse button) and SDL's controller type: an Xbox pad (and
  Steam's virtual pads, and pads SDL cannot name) shows the game's letter icons as Xbox labels those buttons, a
  PlayStation pad its symbols (drawn into the icons as the game cuts its letters), a Nintendo pad the game's own, the
  keyboard the letter or digit of the key bound to each (the space bar's sign for Space, the game's icon where the
  key's name is longer). The icons are redrawn in the game's two icon textures as `pica_vk.c` uploads them (a texture
  pack's too; uploaded again when the device changes). The control reminders beside the menus, the Controls tab's
  reference and the settings' footer name the same buttons (the keys by name, the circle pad's and touch navigation's
  from the bindings). `MSR_BUTTON_ICONS=xbox|playstation|nintendo|keyboard` forces one.
- Mouse aiming (`video.c`, `input.c`): in play the left button shoots (3DS Y; on the bottom screen it still touches)
  and the right one held is Free Aim (3DS L) with the circle pad all the way toward the pointer from Samus: her
  entity's position (Samus's handle at the game +0xFC, then +0xFC..+0x104, about her arm cannon's height) seen by the
  game's camera (`msr_samus_view`), then the zoom and the 16:9 squeeze, then the top screen's place in the window. The
  pointer is a crosshair over the top screen in play and hides while a pad is in use until the mouse moves.
- Touch navigation (`src/msr/touch_nav.cpp`, compiled in as it is): T, or the pad's L3 while a menu shows, puts
  a highlight on the touch screen's buttons (a free cursor where none are known); the D-pad, the circle pad's keys,
  arrows or left stick move it, the right face button, A's key or Return touches (hold to drag), the bottom face
  button, B's key or Escape leaves.
  The panel shows meanwhile and the game gets no other input. The classifier's page is published only once it has
  held for 3 frames and 80 ms, like its state: menus sliding in read as no page for a few frames.
- The settings menu (F1 or Select) is the emulator build's own: `src/msr/overlay.cpp` (Dear ImGui) compiled in as it is,
  drawn last in the presentation pass with dynamic rendering (`msr/overlay_host.cpp`), so its tabs and look are the same.
  The player's options are its `PlayerSettings` in the same `settings.ini`; display and sound changes apply at once
  and go to `settings.ini` `[native]`, which the launcher takes over when msr-native exits. The resolution goes up to
  4x. Restart exits with code 75 and the launcher starts it again. msr-native's own rows: Bloom (the game's glow,
  0-100%, default 75%: `pica_vk.c bloom_scale` scales the combiner constant of the pass that adds the blurred,
  downscaled scene) and HDR (below). The Controls tab rebinds a key and a pad button per 3DS button (`input.c`, `settings.ini`
  `[native_controls]`); Select's binding opens the menu, ZL and ZR are the triggers' taps.
- Save states (`state.c`, `state.h`): the menu's Quick save and Quick load, F5 and F8. The guest's memory (the
  reservation's committed pages, without the game's code) and every part that keeps state, in sections, zlib-compressed
  on a worker thread into `<user>/states/native-quick.state` (~13 MiB); a state of other game files or game mods is
  refused. Saving stops the game's threads where their registers are their whole state (`kernel.c k_freeze`: at a
  scheduling point, or blocked in a wait or sleep; never inside a nested call) and lets the GX worker finish. Loading
  reads the file first, then every guest thread leaves what it was doing (`k_abandon_all`) and parks in `thread_body`;
  memory, objects, threads, services, GSP, PICA, DSP and open files are put back, a wait is issued again, threads the
  state lacks end and missing ones start. Render targets are made again from guest memory.
- HDR (`video.c`, `shaders/hdr.frag`, `win/hdr_win.c`): with the player's HDR on and the display in HDR mode (DXGI's
  colour space; its peak and Windows' SDR white from DXGI and the display configuration), an HDR10 (or scRGB)
  swapchain. The game is drawn as before; hdr.frag expands its highlights towards the peak (default at most 1000 nits)
  and lays the interface (HUD pieces, menus, the settings: their own layer) over it at paper white.
- The amiibo extras (galleries, Fusion Mode, the Fusion suit) and the cheats (energy, missiles, Aeion kept full):
  `cheats.h`'s patches follow the settings each VBlank; `cheats.cpp` refills as the emulator build's
  ApplyMsrRefills, calling the components' setters on the game thread from a trap at the frame limiter.
- The Fusion suit switched in play (`cheats.cpp`, the emulator build's ApplyMsrSuitSwap): the same ARM stub
  (`src/msr/suit_swap.h`) at the same place loads the other suit family, rebuilds Samus's model components and
  releases the first family, one command per frame on the game thread, called from the frame limiter's trap with
  its last instruction turned into a return; the fallback interpreter runs it (silently: `rt_quiet_range`), and
  the effects hanging from her bones are moved to the bones of the same names.
- Texture packs: `textures.cpp` reads the packs the launcher passes (`--texture-pack <folder>`, the Textures tab's
  enabled ones, highest priority first) in Azahar's format and matches textures as the emulator build does: by default
  the legacy hash, CityHash64 (`src/common/cityhash.cpp`) of the texture decoded exactly as Azahar's rasterizer cache
  decodes it (bottom-up rows, colour formats as stored, the others as RGBA8, Azahar's ETC1); `use_new_hash` in a
  pack.json hashes the guest bytes. PNGs (libpng; without it packs are left out) load on worker threads, flipped as
  `flip_png_files` says, and `pica_vk.c` gives each replacement mipmaps down past the game's own levels by its scale.
- Game mods (`msr/mods.cpp`, `msr/layered_fs.cpp`): the launcher's Mods list (`<user>/mods/game`, `settings.ini`
  `[game_mods]`, read and written as QSettings does by `msr/ini.cpp`) and the Textures list. The game parses its
  RomFS image itself, so with mods the image is rebuilt as the emulator's LayeredFS does: each mod's `romfs/` replaces
  or adds files, `romfs_ext/` removes (`.stub`) or patches (`.ips`, `.bps`) them, the first mod listed winning. A
  mod's `exheader.bin` and `code.bin` replace the dump's, and its code patches apply (BPS, then IPS from the lowest
  priority up). The recompiled functions covering a word a mod changed are turned off and run on the fallback
  interpreter (`dispatch.cpp` `rt_code_changed`, from the function ranges `msr_aot_region_spans` the module exports
  since 2026-11-02, one entry per run of a function's code: an older module asks for a rebuild; `PWR_CODE_MOD_TRACE=1`
  lists the functions each changed word turns off); code a mod adds runs there too, its threads included. A Randovania
  seed is such a mod (41 changed words, 14 functions interpreted, 60 FPS); its loader's OpenProcess and
  ControlProcessMemory succeed; with one the Fusion suit setting stays off, as in the emulator build.
- Code the module lacks (reached only through pointers computed at run time): `interp_full.c` interprets it (ARMv6K
  ARM and Thumb, VFPv2) until the module's code is reached, where the emulator build uses its JIT. Each such address
  is logged once and added to `user/msr/recomp-jit-entries-core-native.txt`, which the launcher's next build of the
  native code takes.
- Command lists are copied as the game queues them: the runtime's hook (from Woohoo! Flight School) on Samus Returns'
  GX enqueue (0x1AE170), called by the dispatcher on entry. Run late from guest memory at a higher internal resolution,
  the little set-up lists the game builds in a scratch buffer had been rewritten: the bottom screen drew into the wrong
  target or not at all, and the file select flashed black or shifted in about one frame of four.
- Shaders preloaded (`pica_vk.c`, the emulator build's shader seed): `pwr_shaders.bin` keeps every generated vertex
  shader (key and SPIR-V) and pipeline key the game has used, and at start-up they are all built again on four threads
  (with `pwr_pipelines.bin`, the driver's cache, about 15 ms), so an effect seen in an earlier session no longer waits
  for its shaders. A file from another build of the shader generators is dropped and written again;
  `--preload-shaders=off` (the launcher's "Preload shaders") leaves it alone.

**The internal resolution** (1x-10x; 9x = 2160p/4K) reaches the screen: the game renders its scene into a 240x400
target and TextureCopies it into a 256x512 texture its final composite samples. `pica_vk_texture_copy` makes that copy
from the target's image at the internal resolution (an alias), which `tex_get` uses while the texture's bytes are
still that copy's. Until 2026-11-02 the whole picture came out at 1x whatever `--resolution` said.
The bottom screen too (2026-11-02, the compositor's minimap, icons, counters and pause map; they were blurry and
texture packs looked squeezed): its DisplayTransfer copies the first 320 of 400 lines, and the scan-out copy now takes
those from the 240x320 target the game drew its bottom screen into. Its guest bytes are still made for the compositor's
page classification.

**The renderer never waits for the GPU in a normal frame** (2026-11-02): batches are submitted without waiting, six
batch sets (command buffers, rings, fence) taking turns (a frame is about 10 batches). A target's download reaches
guest memory only when something reads those bytes (a GX command over them, a texture there, a save state), when its
set comes round again, or at the VBlank tick if its batch is done. The TextureCopy above and the bloom/light-shaft
chain's 2x2 downscales of it (`pica_vk_transfer_gpu`) run between aliases on the GPU; their guest bytes are made from
the image (`alias_writeback`) only if something reads them, which nothing does in a usual frame. The scan-out
DisplayTransfer copies the target's image into the image video.c shows, skipping the CPU copy. All of it at every
internal resolution, 1x included. `PWR_VK_NO_GPUCOPY=1` goes back to the CPU copies (A/B), `PWR_VK_SYNC=1` waits for
every batch.

**Known issues:**
- 120 and 144 FPS: the frame limit becomes 8 or 6 ms and main() runs VBlank at that rate, presenting through a
  mailbox swapchain; a VBlank held back while the GX worker lags is delivered as soon as it catches up. The game sends
  ~30 GX commands a frame one after the other, so a frame is their work plus the hand-offs between its threads and the
  worker (2026-11-02: a blocked guest thread is woken alone, a higher-priority one preempts at the next loop head,
  small commands run on the calling thread, sleeps are exact to the microsecond). Measured on an RTX 5090 in gameplay
  with the GPU never waited for (above): 144 holds 142.5-143 at 4x and at 9x (4K) once a room is loaded, 60 holds
  59.5 at 9x, 1x about 131 (it waited for the GPU before: about 94). A busy PC (a video playing) costs 10-15 FPS at
  144. `PWR_GX_STATS=1` prints the worker's time per command type, the hand-off latencies, token waits, where a
  list's time goes, the GPU's busy time per frame (timestamps) and its waits by cause; `PWR_EVTRACE=<s>` records
  256 Ki events (svcs, token, GX) to evtrace.txt.
- No stutter (2026-11-02, the user: "SERIOUSLY bad jitter and stutter" when firing): vertex shaders compile on two
  background threads, unoptimized (11-22 ms instead of 250-360 ms on the GX thread), draws using the interpreter shader
  until theirs and its pipeline are ready; `pwr_shaders.bin` is stamped with a hash of `pica_vsgen.c` (CMake), so a
  rebuild no longer throws it away; the log (stderr) is buffered and written with the 2 s stats (unbuffered, those
  writes cost ~19 ms on the VBlank thread every 2 s); the shader/pipeline cache saves and the infinite energy/Aeion heap
  scan run on threads of their own. The 2 s line shows `frame-max` and the frames over 10/20/50 ms; `PWR_HITCH=<ms>`
  logs each longer frame with the GX worker's and the main loop's share. At 9x/144 while firing: 144.0 FPS, longest
  frame 7.1 ms; from an empty cache no frame over 20 ms. `PWR_LOG_UNBUFFERED=1`, `PWR_VSGEN_SYNC=1`,
  `PWR_VSGEN_OPT=<0-2>` go back to the old behaviour for comparison.
- 90, 120 and 144 FPS hold steady (2026-11-02): the GX worker no longer waits for the GPU in a frame. The bottom
  screen's pixels for the compositor come from a read-back of its scan-out copy (no CPU copy waiting for the target),
  twelve batch sets, and VBlank follows the display's own vblanks (DWM) when the frame rate is its refresh. At 9x
  while firing: 144.0 FPS, longest frame 7.1 ms; 120.0, 8.5 ms.
- Camera zoom without objects popping in at the edges (msr/camera_fov.cpp): the scene's render camera is given a field
  of view wide enough for the zoomed (and 16:9) view (a call hook on its set-up, 0x173A70 called by the camera's
  manager, widens the field of view it is handed), and the renderer scales the world draws back, so the picture is the
  same and the game culls for all of it. The game's camera (vtable 0x6D1FD4) keeps its own field of view, and for its
  update (0x33715C, which keeps the view inside the room's camera limits from the render camera's last field of view)
  the render camera gets the game's values back: the camera stays where it is at zoom 1 (2026-11-03, the user: when
  it moved between a large room's areas at a zoom under 1 the focus drifted; the first version widened the game
  camera's own field of view, and the limits moved it).
- The GX queue is never full for the game (gsp.c's enqueue hook waits for the worker): with two threads queueing, a
  failed enqueue let the other thread's command in first and the file select's bottom screen jittered at 144 FPS.
- Not here: texture dumping, the randomizer's remote connection (Randovania's tracker: no `soc:U`).
- Linux: the public AppImage ships it (`tools/package/appimage.sh`, when Vulkan's headers and glslc or
  glslangValidator are there). Tested on a Steam Deck OLED (RADV, 2026-11-03), at the landing site with the 4x texture
  pack, `PWR_GX_STATS=1` and `--fps=144` for the most it does:
  - The GPU's vertex work had been most of its frame (`PWR_VK_CPU_VS=1` took it off: 11 ms of GPU a frame at 2x became
    3.8). The dispatcher form of the translated shaders compiled to 20-46k instructions with all 256 VGPRs, 100 of them
    spilled: 55 FPS at 4x, 74 at 2x, and 20-38 in play at 4x. The structured form (`pica_vsgen.c`
    generate_structured: 40 VGPRs, ~2.5k instructions, no spills) gives 77-80 at 4x, 98-104 at 3x, 95-111 at 2x, and 60
    held at 4x in play; the picture is the dispatcher's (0.01-0.04% of pixels differ, by the animation's timing, one
    second after a quick load). `PWR_VSGEN_FLAT=1` keeps the dispatcher (A/B). The interpreter shader (`pica_vs.vert`,
    a new program's draws until its translation is built) is as heavy, for those moments only.
  - The game never exited: once its window closed (or Restart, or SIGTERM) `exit()`'s static destructors waited in
    glibc's `pthread_cond_destroy` for the texture packs' PNG workers, which never leave their condition variable. On
    Linux main() now ends with `_exit` after the shutdown.
  - "Match window" (`--resolution=0`, which the launcher had turned into 2x as the option refused 0) follows the top
    screen as shown, the nearest 240-line multiple: 3x on the Deck's 1280x800 (it was 4x by the window's height).

**From the launcher:** once the native code is built, the Game tab's "Without the emulator (experimental)" makes Play
start `msr-native` (beside the exe) with `--layout=single --scale=3 --resolution=<the launcher's> --filter=fsr`, plus
`--fullscreen=borderless`, `--fps=60`, `--preload-shaders=off`, `--vsync=off` and `--volume=<0-100>` as set there; the
game mods and texture packs it finds itself (`--texture-pack <folder>` instead, for tests). It runs in
`user/native/` (log: `msr-native.log`) with saves in `user/native-save/`, a copy of the emulator's on the first start.

## Build and run

Windows (MSYS2 UCRT64: gcc, cmake, ninja, SDL2, Vulkan headers and loader, shaderc):

```sh
cmake -S native -B build -G Ninja && cmake --build build -j 8
cd <a folder with user/msr/recomp/msr_aot.dll and user/msr/game-path.txt>   # run/ (../run.ps1 does this), test/
PATH=/c/msys64/ucrt64/bin:$PATH <repo>/build/msr-native.exe [--dump <dump.cci>] [--mute]
```

Keyboard, mouse and gamepad (`runtime/input.c`, rebindable in the menu's Controls tab; a PC layout for the mouse since
2026-11-03): WASD = circle pad (arrows too), Space = B (jump), F = A (Aeion, confirm; Enter too), R = X (melee
counter), V = Y (shoot), Q/E = L/R, Left Shift = ZL (Morph Ball), C = ZR (the next beam), Tab = Start, Backspace =
Select (the menu), 1/2/3/4 = D-pad up/left/right/down (the Aeion abilities), T = touch navigation; in play the mouse's
left button shoots, the right one held aims at the pointer and the wheel changes beams; the pad by position. F1 opens
the menu, F5 quick saves, F8 quick loads, F9 switches the screen layout, F11 fullscreen.

Debugging switches (environment, as in Woohoo! Flight School): `PWR_SKIP=<first>-<last>` (drop those draws of every
frame, counted from each top-screen swap: bisects an artifact to its draws; with `PWR_SKIP_DUMP=<frame>` it prints their
state in that frame instead), `PWR_GX_COPYLOG=1` (each distinct TextureCopy once), `PWR_GXDUMP=<frame>` (VRAM after
every GX command of that game frame, `gxdump_<n>_<cmd>.bin`: decode the tiled surfaces to see where an artifact appears), `PWR_TRACE=1` (kernel calls and IPC),
`PWR_HANG_AT=<s>` (every thread and what it waits on), `PWR_RENDERER=sw` (the software rasterizer),
`PWR_DRAWDBG=<target>,<shader>,<after s>,<count>` (draw state), `PWR_PEEK=<addr>,<words>` (guest memory).
`MSR_NATIVE_ENTRIES_OUT=<file>` collects code the module lacks, for `build_module.py --extra`. `PWR_TEX_TRACE=1` lists
each texture sampled into each target once, with the targets it lies in (how render-to-texture flows);
`PWR_TEXPACK_TRACE=1` lists each texture a pack replaces. `PWR_SEQ=<presented frame>,<count>` writes both screens of
those frames as PPM files (seq_*): one-frame glitches such as the file select's flashes show up there.

## Adapting Woohoo! Flight School's runtime

- `cpu.h` mirrors `MsrAot::Ctx` field for field (`dispatch.cpp` checks every offset at compile time), so the C runtime
  reads guest registers directly. Memory is Woohoo's 4 GiB reservation; every page of `Ctx::pages` points into it.
- Guest threads run `rt_run` (the dispatcher) instead of a recompiled C function: system calls come back as
  `Exit::Svc`, time slices end at loop heads (`kSlice`, about 1 ms) and become scheduling points (`k_slice_end`).
- Samus Returns needed beyond Pilotwings: `svcQueryMemory` (a region map), timers (one host thread), thread priority,
  `CreateMemoryBlock`, `GetSystemInfo`, `GetProcessInfo` (type 20 is the linear heap's physical offset: without it the
  game hands the GPU virtual addresses), `OutputDebugString`; services `APT:A`, `ir:USER`, `nfc:u` and aliases.
- The GPU's vertex and index buffers are offsets from the attribute base in physical address space. Samus Returns sets
  that base to VRAM and keeps its buffers in FCRAM, so the sum is converted to a virtual address, not the base
  (`pica_sw.c` `vtx_va`).
- The RomFS is read in place from the dump (`fs.c`), not extracted.
- The TEV combiner buffer lags one stage, as the hardware (Citra's model): Samus Returns' post-processing composite
  adds the scene (stage 0) and the bloom (stage 1) through it.
- DisplayTransfer bit 5 (no swizzle conversion, tiled to tiled) for the bloom and light-shaft downscales, and with
  scaling the output size register is the size before it (halved for the image written, as Citra reads it): taken as
  the real size, the downscaled textures had twice their tile-row stride and the landing site showed bright bars.
- Depth targets that a GX copy reads are written back to guest memory after each list (D16/D24/D24S8).
- `interp.c` runs the single instruction the module hands back (patched words: the frame limit, the interface's
  time steps): data processing, B/BL, VMOV.F32. `--fps=60` writes the same patches as the main build.

## Licence

The files in this folder that come from Woohoo! Flight School keep its licence, GPL-3.0-or-later (`LICENSE`); the
rest of `native/` is offered under the same terms. The files under `src/` and `tools/` come from the Azahar-based
Samus Returns port and stay GPL-2.0-or-later (`../license.txt`); Dear ImGui (`externals/msr-imgui`) is MIT. Third-party parts: AMD FidelityFX FSR 1 (MIT, `runtime/shaders/fsr/`), the overlay font from DejaVu
Sans Mono (Bitstream Vera licence, `runtime/font_ui.h`). No game code or data is in this folder.
