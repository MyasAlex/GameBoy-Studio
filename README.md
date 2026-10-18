# GameBoy Studio 1.0 preview

[Download Windows ZIP and EXE — v1.0.0-preview.4](https://github.com/MyasAlex/GameBoy-Studio/releases/tag/v1.0.0-preview.4). Extract the full Windows ZIP; the standalone EXE also needs the supplied DLL.

Stage 4 fixes the seven remaining Mealybug mismatches, three DMG/MGB boot checks, four Win32 focus/input defects, and loading of 16 KiB unbanked homebrew. Fresh runs cover all 71 Mealybug images, 20 published games across 28 DMG/CGB runs, and automated Win32 checks under Wine. See **docs/STAGE4_REPORT_RU.md** and **docs/stage4-validation.json** for evidence and limits.

Native snapshots now use version **18**. A genuine version-17 fixture imports through BESS without exact old cycle phases. Preserve the prior build for old deterministic replays. Bit-exact output PCM after loading a state remains unverified.

Windows x64 frontend and Studio tools, with the SameBoy 1.0.4 accuracy core and free replacement boot firmware. The original Studio core remains available with `gb_headless --core original`.

Extract the full archive and run **START_WINDOWS.bat** or **dist/GBStudio.exe**. Keep `gb_accuracy.dll` and `gb_pixel_filters.exe` beside the frontend. No Visual Studio, SDL or MinGW runtime installation is needed to run it. Only original homebrew demos are bundled.

New features include hardware revisions, CGB compatibility palettes, SGB borders, rare controllers, dual-console/TCP rollback and reconnect, BESS and paired snapshots, actual cheats/RAM search, RetroAchievements rc_client and local Hardcore restrictions, Direct3D/HQ2x/xBRZ, FLAC/PCM AVI, Lua plugins, native FIFO/APU inspectors, a TAS input grid, gamepad profiles and game-library tools.

**The full roadmap is not complete.** Full native Windows manual acceptance and commercial-library coverage remain open. Original Nintendo firmware, commercial games/Pokémon trades, physical devices, full Windows GUI/GPU behavior, external BESS roundtrips and official RetroAchievements approval remain unverified. Fresh CPU/GDI and bundled-demo checks passed under Wine; they do not establish native Windows GPU, audible audio or physical-device acceptance. This release is unsigned.

Read **README_RU.md**, **docs/FEATURE_STATUS_RU.md** and **docs/stage4-validation.json** for setup, verified results and precise limitations. Third-party licenses are included in **docs/THIRD_PARTY_NOTICES.md**. The xBRZ helper is a separate GPLv3 program with full source; the frontend and accuracy core use MIT/Expat licenses.

Build with CMake (C11/C++20). On Windows, **BUILD_WINDOWS.bat --no-launch** or **GBStudio.sln** runs the complete CMake build/test/install pipeline. MSVC uses the included GNU-C engine DLL; MinGW can rebuild it from source. No dependencies are downloaded during a normal build.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```
