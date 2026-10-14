# Bloodborne PC (native Windows port)

Native 64-bit Windows port of the PS4 Bloodborne executable (CUSA03173 v1.09). Game x86-64 code runs natively; a Win32 HLE runtime replaces PS4 system libs; GPU commands go to Vulkan (shadPS4 video core, FSR 3.1/4 upscaling).

Based on the Linux port bbport by deadinside28 and the Windows port by Mrsuss60 (https://github.com/Mrsuss60/bloodborne_pc_windows_port).

## Rules
- Never modify `CUSA03173/` (read-only game dump, 30 GB). Never commit it, `mods/`, or `out/`.
- Mods go in `mods/` and are applied via an overlay in `out/`.
- Per-change history and root-cause notes live in `CHANGES.md`; read it before touching memory, audio, threading, or texture-cache code.

## Layout
- `src/` Win32 loader (`probe.c`), memory (`win32_memory.c`, `runtime_memory.c`), HLE runtime (`runtime_*.c`)
- `gpu/` Vulkan video core (`shadps4/`), shims (`shim/`), submodules in `gpu/third_party/`
- `scripts/` eboot prep, patch compiler (`patches.py`), `stage_dlls.py`, `game_check.py`
- `patches/Bloodborne.xml` community patches; `launcher.py` + `launcher/` Tk GUI
- `tests/` Python unittest + C/C++ tests; `docs/` architecture notes

## Build / run
- Build: `build.bat` (MinGW-w64 GCC, CMake 3.25+, Ninja; third-party libs are vendored in `gpu/third_party/` and `third_party/`)
- Run: `run.bat` (CLI) or `launch_gui.bat` (GUI)
- Tests: `python -m unittest discover -s tests -p "test_*.py"` (no pytest). Some tests are Linux-era (run.sh, symlinks, bash, nix) and fail on Windows without admin/bash; C/C++ tests need the MinGW build.

## Env vars
`BB_GC_TRIGGER/PRESSURE/CRITICAL`, `BB_GC_IDLE_SECONDS`, `BB_VMA_BLOCK_MB`, `BB_VMA_DEDICATED_MB`, `BB_NCPU`, `BB_AUDIO_PREFILL`, `BB_FRAMES_AHEAD`, `BB_DMEM_MB`, `BB_ANISO`, `BB_INTEL_TONEMAP_FIX`, `BB_SKIP_GAME_CHECK`, `BB_LANG`.

## Gotchas
- `bbport.exe` is linked without DynamicBase/high-entropy VA; `runtime_memory_reserve_space()` must run first in `main()` (1 TB guest reservation).
- Do not reintroduce silent null-skips in the VEH (`probe.c`); crashes write `out/crash_<ts>.log`.
