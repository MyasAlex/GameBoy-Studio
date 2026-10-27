# GameBoy Studio 1.1 — native core edition

[Download Studio Core 1.1 for Windows](https://github.com/MyasAlex/GameBoy-Studio/releases/tag/v1.1.0). Choose **GameBoy-Studio-1.1.0.zip** under Assets; GitHub’s automatic source archives contain source only.

This edition runs the project's original C++ DMG/CGB engine, developed as the Studio core. CPU and memory live in `src/gameboy.hpp`; the LCD and audio implementations are in `src/native_ppu.hpp` and `src/native_apu.hpp`. The frontend and headless runner compile these sources directly. SameBoy, its replacement boot firmware, and `gb_accuracy.dll` are not part of this edition. The original project MIT license remains in place.

The archive contains source, a portable Windows x64 build in `dist`, and the project's two homebrew demos. Extract it completely and run `START_WINDOWS.bat` or `dist\GBStudio.exe`; no compiler is needed to launch the included build. To rebuild on Windows, install Visual Studio with **Desktop development with C++** and **CMake tools**, then run:

```bat
BUILD_WINDOWS.bat --no-launch
START_WINDOWS.bat
```

`BUILD_WINDOWS.bat` builds, runs tests, and installs into `dist`. The native core is compiled into the executables. `gb_pixel_filters.exe` is a separate optional xBRZ filter helper, not an emulation core. Normal builds use the included dependency sources and do not download libraries.

For a native Linux build and tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
build/gb_headless demo.gb --frames 120 --output demo.ppm
```

The engine implements DMG/CGB CPU, timers, memory, graphics and audio; ROM/MBC1/MBC2/MBC3/MBC5/MBC7/HuC1/Camera cartridges; serial Link and native snapshots. The Studio tools remain available. Exact hardware revisions, SGB command/border emulation, MMM01, MBC6 and HuC3 are not implemented by this core. See [COMPATIBILITY.md](COMPATIBILITY.md) for limits and [README_RU.md](README_RU.md) for usage.

The accuracy update improves interrupt/DMA timing, audio phases, the LCD fetch pipeline and post-boot CGB/AGB state. See [docs/NATIVE_ACCURACY.md](docs/NATIVE_ACCURACY.md) for scope, measurements and references. No universal or 100% compatibility claim is made. Current validation is recorded in `docs/native-validation.json`; previous native and SameBoy reports are historical.

Keep backups before opening existing games or saves. This core uses `.sav` and `.rtc` battery files. New snapshots use GBSTATE8 or BESS with Studio GBSP3 and optional GBAU tool progress. GBSTATE3–7 and GBSP2 remain readable, but they omit the newly modelled timing phases; those phases are reconstructed during import. Modern exact states require the matching hardware profile and compatible build. Old SameBoy exact snapshots (`GBSACC1`, `GSTU`/`GBSX`) and TAS recordings are not exact native-core snapshots. Standard BESS import may lose internal timing phases and is not a promise of interchange with every emulator.

No commercial games, Nintendo boot firmware, private saves or account credentials are bundled. User-provided boot images remain optional; the default is native post-boot initialization. Third-party components used by the tools keep their licenses in [docs/THIRD_PARTY_NOTICES.md](docs/THIRD_PARTY_NOTICES.md).
