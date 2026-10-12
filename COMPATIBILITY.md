# Verified DMG compatibility — 0.6

Validation date: 2026-10-12. The full, reproducible per-ROM output is in [docs/test-results.json](docs/test-results.json).

| Check | Result | Coverage |
|---|---:|---|
| Blargg | **35 / 35 PASS** | 11 individual CPU ROMs and the combined ROM; instruction timing; both memory-timing suites, singles and combined; 12 individual DMG sound ROMs and the combined ROM; HALT bug |
| Mooneye, DMG ABC selection | **94 / 94 PASS** | Applicable acceptance and emulator-only ROMs from the pinned suite, including CPU timing, interrupts, DMA, timers, boot registers/divider/serial phase, LCD/STAT, sprite timings, MBC1/MBC1M/MBC2/MBC5 |
| dmg-acid2 | **PASS: 0 different pixels** | Full 160×144 image compared byte for byte in canonical grayscale |
| Core regressions | **PASS** | Documented opcode execution, interrupt/EI/HALT/STOP behavior, memory restrictions, frame-clock debt, PCM sample production, RTC rollover, demo/input, deterministic state restore, corruption/wrong-ROM rejection and 128 KiB RAM states |
| AddressSanitizer + UndefinedBehaviorSanitizer | **PASS** | Core regressions; leak scanning disabled because this execution environment prevents the required process inspection |
| Windows x64 | **Cross-build PASS** | MinGW-w64 C++20; PE GUI binary, static C++ runtime, only Windows system DLL imports |

`130 PASS, 0 XFAIL, 0 FAIL` refers to the external ROM/image checks, not 130 commercial games. The runner has no enabled expected-failure exceptions.

## Reproduce

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python -m pip install Pillow
python tests/run_rom_tests.py --emulator build/gb_headless --download --mooneye
```

The checked-in report was produced by this runner with the same pinned test bytes. Windows uses `build/Release/gb_headless.exe` and `ctest -C Release`.

## Sources and pins

- [Blargg/retrio hardware tests](https://github.com/retrio/gb-test-roms), commit `c240dd7d700e5c0b00a7bbba52b53e4ee67b5f15`.
- [Mooneye Test Suite](https://github.com/Gekkio/mooneye-test-suite), commit `31510e12eea6286d36eea060a6adde755e1067aa`, prebuilt `mts-20260714-0944-31510e1`. Only DMG ABC-compatible acceptance and emulator-only files are selected; other model revisions, CGB/SGB, manual and madness tests are excluded.
- [dmg-acid2 v1.0](https://github.com/mattcurrie/dmg-acid2), reference image from commit `8a98ce731f96dde032ffb22ec36dc985d78fdb18`; ROM, reference image and Mooneye archive checksums are enforced by the downloader.
- [Pan Docs](https://gbdev.io/pandocs/) and the hardware behavior documented by the above test authors informed the implementation.

External test ROMs and Nintendo boot ROMs are not distributed in the project archive. The included demo is original homebrew and now uploads graphics with the LCD disabled, as required on hardware.

## What these results do not establish

Mario / Super Mario Land / Tetris ROMs were not supplied and have not been directly tested. Hardware tests provide substantial evidence for the core but do not certify every game or ROM revision. Commercial-game playthroughs, audible sound quality, physical controller input and native Windows GUI behavior still need real-device testing.

This is a dot-clocked renderer, not a transistor-level hardware simulation. The complete mealybug/SameSuite raster-effect suites and Blargg OAM-corruption suite have not been validated. Analog behavior and every obscure mid-scanline FIFO/OAM interaction are not guaranteed. Link-cable multiplayer, SGB, Game Boy Camera, HuC controllers, MBC7 and physical rumble are not implemented.

MBC3 battery RAM and RTC have local regression coverage; the pinned Mooneye emulator-only selection does not contain a full MBC3 commercial-game suite. RTC advances from emulated cycles during execution and from the saved wall timestamp when loading; turbo accelerates its running clock.

The app skips the Nintendo boot ROM using tested DMG post-boot registers and clock phases. State format `GBSTATE3` contains pipeline/audio/timing state and a checksum; old 0.5 state files are rejected. Battery `.sav` format remains compatible.

## Color follows DMG

CGB-only ROMs are rejected with an explanation. Dual GB/CGB cartridges run in DMG mode. The next stage must add banked VRAM/WRAM, tile attributes, color palettes, double-speed operation, HDMA/GDMA and CGB tests while retaining this DMG suite.
