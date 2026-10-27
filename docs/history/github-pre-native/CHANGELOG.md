# 1.0 preview — 2026-10-13

## Stage 4 — 2026-10-18

- Corrected CGB TILE_SEL bus retention/addressing and DMG window sampling against unchanged Mealybug references; snapshot layout is now 18 with verified BESS import for layout 17.
- Installed the documented DMG/MGB cartridge-entry peripheral phase after replacement firmware and preserved MBC6 boot overlays.
- Fixed four Win32 focus/capture/tilt/rumble defects; added visible-window regression checks and a manual acceptance script.
- Added a pinned published-game corpus and input/frame/audio/save smoke tooling; fixed loading of 16 KiB unbanked homebrew in raw, ZIP and GZIP form. Native Windows hardware acceptance remains open; see STAGE4_REPORT_RU.md.


Default SameBoy core, revisions/SGB/rare controllers, exact linked states and reconnect, achievements, GPU filters/media, expanded Studio/TAS/library/Windows tools. See FEATURE_STATUS_RU.md and RELEASE_NOTES.md for verified results and remaining work.

# 0.9 beta

See FEATURE_STATUS_RU.md and RELEASE_NOTES.md for the new implementation and explicit limitations.

# 0.8 — 2026-10-13

- External DMG/CGB boot assets, GBSTATE5 boot/input snapshots and legacy v3/v4 reading; bounded file/state loading and battery backups.
- In-memory RLE rewind, input movies with frame-sampled deterministic input, named states, nine isolated quick slots, autosession and speed/frame controls.
- RU/EN Win32 menus, configurable game keys, recent ROMs, client themes, palettes/LCD/CRT/ghosting and raw/processed comparison.
- Event-driven WASAPI worker, lock-free SPSC, waveOut fallback, master gain/channel mute and raw WAV recording.
- SM83/CB disassembler, Studio stepping/stops/trace/dumps, strict Unicode CLI and ROM/header/CRC32 diagnostics.
- Transactional IPS/BPS copies, pure cheat decoding APIs, resource-bound/corruption and multithreaded tests; ten suites and sanitizer/release CI.
- Full requested roadmap retained with explicit pending feature status.

# Changelog

## 0.7 — Game Boy Color (2026-10-12)

- Automatic cartridge-based DMG/CGB selection and manual frontend/headless model selection.
- Native RGB555, BG/OBJ palettes, banked VRAM/WRAM, tile banks/attributes/flips and CGB priorities.
- KEY1/STOP double-speed CPU, stable display/audio/RTC clock, GDMA/HDMA bus stalls and independent OAM DMA buses.
- CGB fast serial, timer/STAT differences, wave RAM/retrigger/power behavior and PCM registers.
- GBSTATE4 color/clock/DMA state, original GBSTATE3 DMG loading and invalid-state checks.
- Original CGB demo and independent image reference; four new internal suites and pinned CGB hardware runner.
- Native Windows binaries, both demos in build/install paths, updated batch/Visual Studio/CI checks.
- 144 external checks, 5 internal suites and original demo image pass. Provided Super Mario Land V1.1 passes a 600-frame scripted smoke, identical to 0.6 in the recorded CPU/memory/video/audio traces.

The full evidence and remaining timing limitations are in COMPATIBILITY.md.
