# Verified compatibility — 0.8

Validation date: 2026-10-13 (Asia/Irkutsk). Native Windows x64 MSVC release, static C++ runtime.

| Check | Result | Evidence |
|---|---:|---|
| Blargg DMG | 35/35 PASS | CPU, instruction/memory timing, HALT, DMG audio singles and combined |
| Mooneye DMG ABC selection | 94/94 PASS | Pinned applicable acceptance and emulator-only selection |
| dmg-acid2 | PASS, 0 differing pixels | Complete canonical grayscale RGB comparison |
| Blargg CGB audio | 13/13 PASS | Twelve singles and combined in native Color mode |
| cgb-acid2 v1.1 | PASS, 0 differing pixels | All 160×144 RGB pixels |
| Original Color demo | PASS, 0 differing pixels | Independent project reference image |
| Internal native tests | 10/10 suites PASS | Core, CGB memory/PPU/APU/integration, boot/state/bounds, rewind/movies, Studio, SPSC/WAV, IPS/BPS |
| GitHub Actions | PASS on Windows and Linux | Native build/tests, pinned hardware suites, Linux ASan/UBSan; run 38283863888 |
| Windows AddressSanitizer | 4/4 new suites PASS | Core extension, sessions, audio buffers, cartridge tools; extension repeated after final file-reader hardening |
| GUI hidden self-test | PASS on DMG and CGB demos | 12 frames and 48 offscreen configurations each; raw PPU invariant; bad-ROM exit2 |
| WASAPI silent lifecycle | PASS at 20/40/80 ms | Device creation, flush and close; waveOut fallback path also exercised in a scratch harness |
| Input recording review | PASS | Transient/held frame samples, non-default channel mute, exact serialized replay |
| Unicode CLI/exports | PASS | Cyrillic and emoji paths, JSON parsing, trace bounds, invalid arguments and collision rejection |

**144 external hardware/image checks** pass; with the original Color demo image this is **145 checks**. Internal suites are counted separately. Results describe tested scenarios, not the number of compatible games.

The Windows GUI self-test exercises application startup and renderer code without interactive visual inspection. Native-menu usability, subjective sound quality and physical gamepad operation have not been certified. Light/dark themes cover the client canvas and status area; native menus/Studio controls use the Windows theme. ASan/UBSan on Linux is configured in CI; no local Linux sanitizer result is claimed.

Machine-readable evidence: [DMG](docs/test-results.json), [CGB](docs/cgb-test-results.json), [summary](docs/validation-summary.json). The 0.7 Super Mario Land smoke remains historical evidence in the old release; it is not a fresh 0.8 full playthrough.

## New regression coverage

Synthetic DMG boot execution and FF50 unmapping, CGB segmented/packed images and cartridge-header hole, boot register locks and boot image identity; mapper reset and MBC1M; in-memory GBSTATE5/input resume and GBSTATE3/4 loading; canonical bool fields, finite APU filters, coordinate/counter/CPU-flag validation; checksum-valid malformed states, oversized files rejected before allocation, transactional rejection and battery backup.

Independent RLE packet round-trips and corruption bounds, time/memory-bounded rewind, full initial-state input replay including mixer settings; exhaustive SM83/CB instruction-size/legal-set checks, golden mnemonics, conditions/run-to/changed-value watches, interrupt/idle/HALT bug traces, bounds and CRC/header diagnostics; 500,000-frame concurrent SPSC ordering/wrap test, ramps, exclusive Unicode WAV files and RIFF caps; all BPS actions and overlapping copies, all three CRC checks, IPS literal/RLE/truncation, malformed prefixes and resource limits.

## Reproduce

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
py -m pip install Pillow
py tests/run_rom_tests.py --emulator build/Release/gb_headless.exe --download --mooneye
py tests/run_cgb_tests.py --emulator build/Release/gb_headless.exe --download
build/Release/GBStudio.exe --self-test demo.gbc
```

Linux uses `-DCMAKE_BUILD_TYPE=Release`, `build/gb_headless`, and no `-C Release`. Sanitizers: `-DCMAKE_BUILD_TYPE=Debug -DGB_ENABLE_SANITIZERS=ON`. External test ROMs are downloaded from their authors, not bundled. The legacy state fixture contains the project's original demo only.

## Primary references and pins

- [Blargg tests](https://github.com/retrio/gb-test-roms), commit `c240dd7d700e5c0b00a7bbba52b53e4ee67b5f15`.
- [Mooneye](https://github.com/Gekkio/mooneye-test-suite), `mts-20260714-0944-31510e1`, pinned archive SHA-256.
- [dmg-acid2 v1.0](https://github.com/mattcurrie/dmg-acid2), reference `8a98ce731f96dde032ffb22ec36dc985d78fdb18`.
- [cgb-acid2 v1.1](https://github.com/mattcurrie/cgb-acid2), reference `fa5b7f86d6fb599f79e55169494d981a7af75a31` with ROM/image SHA-256 checks.
- [Pan Docs](https://github.com/gbdev/pandocs): instruction set, power-up, CGB registers, audio, DMA and memory maps.
- [Microsoft WASAPI](https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendering-a-stream), [shared-mode conversion flags](https://learn.microsoft.com/en-us/windows/win32/coreaudio/audclnt-streamflags-xxx-constants).
- [BPS format](https://github.com/blakesmith/rombp/blob/master/docs/bps_spec.md), [Flips IPS](https://github.com/Alcaro/Flips/blob/master/libips.cpp), [SameBoy cheat decoding](https://github.com/LIJI32/SameBoy/blob/master/Core/cheats.c).

## Remaining limits

No perfect cycle-accuracy or hardware-revision claim. Mealybug/SameSuite raster/audio suites, OAM corruption and every FIFO edge case remain unvalidated; mode3 stalls retain approximations. The 8200-T-cycle CGB speed-switch locking behavior is approximate. Manufacturer Boot ROMs have not been tested; supplied assets only are supported, and no Nintendo images are distributed.

CGB compatibility mode for DMG-only cartridges and automatic boot colorization remain unsupported. Pocket/Light are display palette presets, not hardware revisions. Link peers/netplay, infrared communication, SGB, Camera/Printer, HuC/MMM01/MBC6/MBC7 and physical rumble/tilt remain pending. RTC advances with emulated base dots while running; turbo therefore accelerates its running clock.

GBSTATE5 continues the native-struct ABI of earlier states; cross-compiler/architecture portability is not certified, and BESS is pending. Watches detect value changes after a CPU step rather than bus access. CPU effects are not GPU shaders; WAV is 44.1 kHz PCM, not FLAC. Game Genie/GameShark are parsers only, not an active cheat manager. See [complete feature status](docs/FEATURE_STATUS_RU.md).