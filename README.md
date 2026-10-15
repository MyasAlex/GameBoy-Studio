# GameBoy Studio 1.0 preview

Stage 3 addresses reported acceleration: discard Turbo timing debt, bound catch-up frames, add Ctrl+0 for normal speed and display the measured speed percentage. Six actual Windows timer-loop probes passed with audio off/on; sustained acceleration was not reproduced with the available test ROMs. See **docs/STAGE3_REPORT_RU.md** and **docs/STAGES_RU.md**. Custom hotkeys take priority over Ctrl+0; the speed menu also selects 1x.

The stage 2 core is unchanged: four Mealybug differences were fixed; seven and the three replacement-firmware Boot checks remain open. See **docs/STAGE2_REPORT_RU.md**. Native core snapshots use version 17; older version-16 snapshots import through BESS without preserving exact previous cycle phases.

Windows x64 frontend and Studio tools, with the SameBoy 1.0.4 accuracy core and free replacement boot firmware. The original Studio core remains available with `gb_headless --core original`.

Extract the full archive and run **START_WINDOWS.bat** or **dist/GBStudio.exe**. Keep `gb_accuracy.dll` and `gb_pixel_filters.exe` beside the frontend. No Visual Studio, SDL or MinGW runtime installation is needed to run it. Only original homebrew demos are bundled.

New features include hardware revisions, CGB compatibility palettes, SGB borders, rare controllers, dual-console/TCP rollback and reconnect, BESS and paired snapshots, actual cheats/RAM search, RetroAchievements rc_client and local Hardcore restrictions, Direct3D/HQ2x/xBRZ, FLAC/PCM AVI, Lua plugins, native FIFO/APU inspectors, a TAS input grid, gamepad profiles and game-library tools.

**The full roadmap is not complete.** Mealybug still has hardware discrepancies; original Nintendo firmware, commercial games/Pokémon trades, physical devices, full Windows GUI/GPU behavior, external BESS roundtrips and official RetroAchievements approval remain unverified. Native CPU rendering and bundled demo execution have been checked in stage 1. This release is unsigned.

Read **README_RU.md**, **docs/FEATURE_STATUS_RU.md** and **docs/validation-1.0.json** for setup, verified results and precise limitations. Third-party licenses are included in **docs/THIRD_PARTY_NOTICES.md**. The xBRZ helper is a separate GPLv3 program with full source; the frontend and accuracy core use MIT/Expat licenses.

Build with CMake (C11/C++20). On Windows, **BUILD_WINDOWS.bat --no-launch** or **GBStudio.sln** runs the complete CMake build/test/install pipeline. MSVC uses the included GNU-C engine DLL; MinGW can rebuild it from source. No dependencies are downloaded during a normal build.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```
