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
