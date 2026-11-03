# Working on msr-native

- Build: from Git Bash, `export PATH=/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH && ninja -C build -j 8`
  (first time: `cmake -S native -B build -G Ninja`). Never more than `-j 8`: the PC's i9-14900K resets under
  all-core builds until its BIOS is updated.
- Never run git in a shell that has the MSYS2 PATH exported. MSYS2's git has no autocrlf and no identity: run git in
  a separate command (Git for Windows).
- Commit as `fhclimited <347583180+fhclimited@users.noreply.github.com>` (GIT_AUTHOR_*/GIT_COMMITTER_* variables;
  the global git config has a personal address). Push only when asked.
- `native/` sources are CRLF: keep them CRLF when editing with scripts. Write Python edit scripts to a file (a Bash
  heredoc turns `\n` in C strings into real newlines).
- `run/` is the user's play folder (their saves and settings): never use it for tests. Tests run from `test/` with
  `tools/dev/shot.ps1`. It keeps the window in the background (no foreground, no console) and stops only the process
  it started. Never stop msr-native processes by name: the user may be playing (from `run/` here, or from the
  emulator-based build's play copy, `samus-recomps/build-jit/fresh2`).
- Nothing made from the game goes into git: the dump, `msr_aot.dll` and the generated code, saves, texture packs,
  shader caches, texture or frame dumps.
- Test keys (`shot.ps1`, VK hex): F 46 / Enter 0D = A, Space 20 = B, Tab 09 = Start, WASD 57 41 53 44 (arrows 25-28
  too), Q 51 = L, E 45 = R, Shift 10 = Morph Ball, C 43 = next beam, F1 70. From boot, to play on test file B:
  `46:200,wait6000,28:150,wait800,0D:200,wait2500,0D:200,wait16000,shot`.
- The launcher (no options) in tests: `shot.ps1 -GameArgs '' -Keys '0D:150,...'` (Enter is Play; its user folder is
  the test folder via MSR_NATIVE_HOME; the game it starts is a child process, found and stopped by the script). A first
  build from nothing: `-Folder <repo>\test-build` with only `user\msr\game-path.txt`, and `MSR_BUILD_JOBS=8` (the
  launcher otherwise uses every thread).
- Releases: README.md (Releases). The Linux build runs in WSL Ubuntu, launched from PowerShell (Git Bash mangles
  /mnt paths); WSL has no SDL2 headers and sudo needs a password, so SDL2 is built from
  ~/samus-build/src/externals/sdl2/SDL (`MSR_SDL2_SOURCE`).
- `native/README.md` documents the runtime, its Samus Returns parts and the debugging switches (`PWR_*`).
- The emulator-based build (Azahar fork, Qt launcher) is the separate `samus-recomps` repository. It shares `src/msr`
  with this one, and changes there are not synced automatically.
