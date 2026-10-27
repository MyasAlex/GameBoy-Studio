# Compatibility — native Studio core

Both the Windows frontend and `gb_headless` compile the Studio C++ engine from `src/gameboy.hpp`, `src/native_ppu.hpp` and `src/native_apu.hpp`. This edition does not contain or load SameBoy or `gb_accuracy.dll`.

| Area | Scope |
|---|---|
| Hardware | DMG and CGB families, DMG compatibility mode on CGB, and selected documented CGB/AGB startup and revision differences; exact coverage of every revision is incomplete |
| Cartridges | ROM-only, MBC1/2/3/5/7, HuC1, Game Boy Camera |
| Unimplemented | MMM01, MBC6, HuC3, SGB command HLE/borders |
| Battery data | Native `.sav` RAM and `.rtc` MBC3 clock files; no automatic import of old `.sbbattery` clock data |
| New snapshots | GBSTATE8; BESS with Studio GBSP3 for timing phases and optional GBAU tool progress; exact states require a compatible build and matching hardware profile |
| Legacy native snapshots | GBSTATE3–7 and GBSP2 import remains available; missing new timing phases are reconstructed |
| Old accuracy-engine data | SameBoy `GBSACC1`, `GSTU`/`GBSX` exact continuation and TAS replay are not supported as native snapshots |
| Boot | Native post-boot initialization by default; optional user-provided external firmware; skip-boot timing approximates a canonical startup rather than executing firmware |

The current measurements and comparison with the previous native release are recorded in [docs/NATIVE_ACCURACY.md](docs/NATIVE_ACCURACY.md). Detailed evidence is in `docs/native-validation.json`, `docs/native-dmg-results.json`, `docs/native-cgb-results.json` and `docs/native-extended-results.json`. Each report applies to its recorded executable and source manifest. The old native v1 reports are preserved under `docs/history/native-v1`.

Basic tests, SameSuite audio/revision tests, and Mealybug LCD image tests cover different behavior and are reported separately. A passed test does not establish universal game compatibility. Extended failures remain visible. The additional GBMicrotest/Mooneye matrix is exploratory: some files are manual probes without a terminal pass/fail assertion, and some assertions require further interpretation against the test source. Do not count an unfinished manual probe as either a pass or a core failure.

Windows x64 binaries are included in `dist`. The scope of their checks is recorded in `docs/windows-native-validation.json`. Wine checks do not validate the complete Windows GUI, Direct3D, physical audio devices or accessories. The frontend and headless runner need no separate emulation-engine DLL.

Commercial-game coverage, Pokémon trades, original Nintendo firmware, exchange with other BESS implementations, Windows GUI/GPU/audio, real networks and physical accessories require separate checks. This edition does not claim 100% compatibility or official RetroAchievements approval.

See [README_RU.md](README_RU.md) for setup, [docs/NATIVE_CORE_RU.md](docs/NATIVE_CORE_RU.md) for the native-core structure, and [docs/THIRD_PARTY_NOTICES.md](docs/THIRD_PARTY_NOTICES.md) for licenses. Keep copies of battery files and states when changing emulator versions.

Studio Core 1.1 also accepts unbanked 16 KiB ROM-only images (direct/ZIP/GZIP), and has measured startup-register fixes for Pocket/Light/SGB profiles. SGB commands and borders remain unsupported. See docs/NATIVE_ACCURACY.md for measured scope and remaining failures.
