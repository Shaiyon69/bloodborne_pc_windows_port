Bloodborne PC (Native Windows Port) - v0.2.1

What's New & Fixed in v0.2.1

Map Loading & Shader Table Crash Fix
- Fixed a fatal access violation (0xc0000005) when loading into maps caused by unbound extended user-data tables (ReadUdReg reading address 0x8). The recompiler now returns a value-initialized descriptor when table pointers are null, allowing vertex specialization to safely fall back to its empty-sharp handler (PR #36).

High-Resolution Direct Memory Stability (1440p / 4K / Ultrawide)
- Fixed startup crash at guest offset 0x443025 when overriding resolutions above 1080p directly from the launcher.
- run.bat now automatically provisions a 9152 MiB direct memory budget (BB_DMEM_MB) when render pixel counts exceed 1920x1080, matching the Increased Graphics Heap Sizes patch requirements (PR #37).

Windows Host Read-Probe & Hook Compatibility
- Fixed exit code 139 crash on startup caused by third-party background software and window hooks (e.g. DRM overlays, screen utilities) calling IsBadReadPtr during CreateWindowExW.
- The Vectored Exception Handler (VEH) now verifies whether an access violation originates from KERNEL32!IsBadReadPtr unwind metadata and allows Windows SEH to handle the probe natively without terminating the process (PR #35).

Packaging & Runtime Dependency Fixes
- Added all transitive FFmpeg, AV1, and image decoder DLLs (libaom.dll, libjxl.dll, libjxl_threads.dll, liblc3-1.dll, libhwy.dll, libcrypto) to the packaged runtime, resolving "DLL not found" errors when launching bbport.exe directly or via the GUI.
- Upgraded stage_dlls.py to recursively resolve and stage PE imports automatically from MinGW/MSYS2 toolchains.
- Updated launcher defaults with recommended baseline settings (60 FPS, Default 1080p, Skip Intro enabled, FSR upscaler off by default for maximum compatibility).

Build System Enhancements
- Fixed ar archiver wildcard expansion in build.bat on Windows cmd.exe, ensuring libatrac9.a builds with all audio decoder symbols.
- Added automated submodule checking and git longpaths configuration.
- Added automatic virtual drive (subst X:) detection in build.bat to bypass the Windows 260-character MAX_PATH limit when compiling deep directory structures.

Requirements
- 64-bit Windows 10 or Windows 11
- Vulkan 1.3 capable GPU (NVIDIA GeForce GTX 10-series+, AMD Radeon RX 400-series+, Intel Arc)
- Python 3.10+ (for GUI launcher and patch compiler)
- Decrypted Bloodborne PS4 dump (CUSA03173, version 1.09)

How to Run
1. Download and extract Bloodborne-Windows-Port-Release-v0.2.1.zip.
2. Double-click launch_gui.bat.
3. Browse to your eboot.bin file (or game folder).
4. Click Launch Bloodborne.
