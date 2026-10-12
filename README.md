# GameBoy Studio 0.8

Native Windows 10/11 x64 Game Boy / Game Boy Color emulator and Studio tools, written in C++20. This release extends the verified 0.7 core with usable session, display, audio, and debugging tools.

Extract the entire Windows archive and run **GBStudio.exe**. Open `.gb` / `.gbc` with Ctrl+O or drag a ROM into the window. The included demo cartridges are original project assets. Commercial games and manufacturer boot images are not included.

## Added in 0.8

- English/Russian menus, client light/dark themes, recent games, configurable game keys, saved window position and automatic session restore.
- Display-only DMG palettes and custom colors, LCD grid, CRT scanlines, LCD ghosting, integer scaling and raw/processed comparison. Effects preserve the emulated framebuffer; these are CPU/GDI effects.
- Named save files and nine quick slots; in-memory compressed rewind (off/30s/2min/5min with a 64 MiB limit), 0.25x–16x speeds and frame stepping.
- Frame input movies containing the initial validated state, mixer settings and sampled joypad input; deterministic replay for the same ROM/state ABI.
- External DMG/CGB boot images, GBSTATE5 boot/input snapshots, GBSTATE3/4 loading, battery backups and bounded file readers.
- WASAPI event-driven audio worker, lock-free SPSC PCM transport, waveOut fallback, master gain/channel mute and raw PCM WAV recording.
- Studio instruction stepping, full SM83/CB disassembly, registers, PC breakpoints, value-change watches and recent trace; JSON ROM/header/CRC32 reports. Reusable C++ APIs also provide conditional breakpoints and run-to.
- IPS/BPS patching into a separate ROM copy. BPS verifies all three CRC32 values. Original ROM and saves remain separate from patched copies. Cheat parsing APIs are provided; a runtime cheat manager is still pending.
- Strict Unicode-aware CLI, ten test suites, sanitizer configuration, Windows/source archives, SHA-256 and a tag-triggered GitHub Release workflow.

## Controls

| Action | Key |
|---|---|
| D-pad / A / B | Arrows / Z / X |
| Start / Select | Enter / Shift |
| Open / reset | Ctrl+O / Ctrl+R |
| Pause / frame step | P / F6 |
| Quick save / load | F5 / F9 |
| Select quick slot | 1–9 |
| Named save / load | Ctrl+Shift+S / Ctrl+Shift+O |
| Rewind / turbo | Hold Backspace / hold Tab (8x) |
| Fullscreen / raw BMP | F11 / F12 |
| Studio debugger | Ctrl+D |

XInput D-pad/left stick, A/B, Back/Start are supported. Audio playback is enabled at 1x; other speeds, pause and rewind are muted. WAV records raw emulated PCM rather than device output, at stereo PCM16 44.1 kHz. Channel selection is fixed during movies and restored from the recording for repeatable APU state. Input movies sample buttons at frame boundaries; taps completed between two recorded frames are not recorded.

Quick states use `game.gb.state` or `game.gbc.state`; slots 2–9 append `.slotN.state`. Slot 1 can read the old `game.state` when the new filename is absent. `.sav` / `.rtc` battery files remain beside the ROM; the previous battery generation is retained as `.bak`. Session states and patched ROM copies are stored beside the configuration under `Sessions/`.

Settings use an atomic UTF-16 `GameBoy-Studio.ini` beside the executable; when that directory is not writable, `%LOCALAPPDATA%/GameBoy-Studio/config.ini` is used. The GUI has no telemetry or network activity.

## Build and verify

Install Visual Studio 2022/2026 with Desktop development with C++. Open `GBStudio.sln` (Release/x64), or run `BUILD_WINDOWS.bat --no-launch`. CMake is also supported:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
```

The command-line tool is `tools/gb_headless.exe` in the Windows archive. Use `--help`; examples:

```powershell
tools/gb_headless.exe demo.gbc 60 --report report.json --output frame.ppm
tools/gb_headless.exe demo.gb --instructions 100 --trace trace.txt --trace-limit 100
tools/gb_headless.exe demo.gb --boot-rom my-dmg-boot.bin --model dmg
GBStudio.exe --self-test demo.gbc
```

The hidden GUI self-test checks startup, 12 emulated frames, 48 offscreen rendering combinations and framebuffer immutability. It does not certify visible interaction, audio quality or controller hardware.

For pinned public hardware ROM suites, install Pillow and run `tests/run_rom_tests.py --emulator <headless> --download --mooneye` and `tests/run_cgb_tests.py --emulator <headless> --download`. On GCC/Clang, configure Debug with `-DGB_ENABLE_SANITIZERS=ON` for ASan/UBSan.

## Scope and evidence

See [COMPATIBILITY.md](COMPATIBILITY.md), [validation summary](docs/validation-summary.json), and [full requested feature status](docs/FEATURE_STATUS_RU.md). 0.8 is a substantial first stage of the full requested platform, not completion of every feature. SGB, uncommon mappers/accessories, CGB compatibility mode for DMG games, netplay, RetroAchievements, game-library covers, GPU shaders, ZIP/GZIP and BESS remain pending. Native-layout save states are not certified portable across arbitrary compilers/architectures. Nintendo boot images were not exercised; synthetic tests verify mapping, execution and handoff.

The GitHub workflow publishes release assets for a pushed `v*` tag only after Linux compatibility, sanitizers and Windows tests all succeed. Signing requires the project owner's certificate.