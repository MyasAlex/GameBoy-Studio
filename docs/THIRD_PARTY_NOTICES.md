# Third-party notices

GameBoy Studio's frontend, original Studio emulation core, and original tools retain the project MIT license in `LICENSE`. This native-core edition does not contain the SameBoy engine, its replacement firmware, or its prebuilt engine DLL. Historical reports may refer to that previous integration; they are not executable components of this edition.

Other components retain their individual licenses. Their corresponding source and notices are included.

| Component | Pinned source | License / files |
|---|---|---|
| RetroAchievements rcheevos / rc_client | 12.5.0, `116d018470757b0d832698a5741f07b30a3d8e3c` | MIT, `third_party/rcheevos/LICENSE` |
| HQ2x algorithm / shader | Lior Halphon, mGBA shader | MIT notice in `third_party/hq2x/hq2x.glsl`, `src/hq2x_shader.h` |
| xBRZ image filter | `a2d8dce723e8fab548bf8460b46eeebfc64abca6` | GPLv3, `third_party/xbrz/License.txt`; `gb_pixel_filters.exe` is a separate program, not linked into the frontend or emulation core. Its full source is included. |
| Miniz | 3.0.2 | MIT, `third_party/miniz/LICENSE` |
| Lua | 5.4.8, `6e22fedb74cf0c9b6656e9fce8b7331db847c605` | MIT notice in `third_party/lua-src/lua.h` |

Upstream repositories: https://github.com/RetroAchievements/rcheevos ; https://github.com/mgba-emu/mgba ; https://sourceforge.net/projects/xbrz/ ; https://github.com/richgel999/miniz ; https://github.com/lua/lua .

Protocol references: https://gbdev.io/pandocs/ ; https://github.com/LIJI32/SameBoy/blob/master/BESS.md (BESS interchange specification) ; https://github.com/RetroAchievements/rcheevos/wiki/rc_client-integration ; https://xiph.org/flac/format.html . A specification or source reference does not mean that another emulator's engine is included.

Hardware-test sources used by the optional harnesses: https://github.com/LIJI32/SameSuite ; https://github.com/mattcurrie/mealybug-tearoom-tests ; https://github.com/c-sp/game-boy-test-roms/releases/tag/v7.0 . External test ROMs, proprietary Nintendo firmware and commercial games are not bundled.

The accuracy update also consulted https://github.com/aappleby/GBMicrotest and https://github.com/Gekkio/mooneye-test-suite . SameSuite is MIT, copyright 2018 Lior Halphon; Mealybug Tearoom Tests is MIT, copyright 2018 Matt Currie. Pinned versions and the scope of the measurements are recorded in [NATIVE_ACCURACY.md](NATIVE_ACCURACY.md). Test expectations describe hardware behavior; the Studio implementation remains part of this project.

Developing the included Studio core does not transfer the copyright of existing code or third-party libraries. Their license notices remain applicable.
