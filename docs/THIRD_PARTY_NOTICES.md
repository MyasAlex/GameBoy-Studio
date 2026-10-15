# Third-party notices

GameBoy Studio's frontend, original legacy core, and original tools retain the project MIT license. The distribution also contains the separately licensed components below. All corresponding source and notices are included in this archive.

| Component | Pinned source | License / files |
|---|---|---|
| SameBoy accuracy core and replacement boot firmware | 1.0.4, `c4cf7abab79a4e22dc17e0704995976cb2751b2d` | Expat/MIT, `third_party/sameboy/LICENSE`, `UPSTREAM.md`, complete Core and BootROM sources |
| RetroAchievements rcheevos / rc_client | 12.5.0, `116d018470757b0d832698a5741f07b30a3d8e3c` | MIT, `third_party/rcheevos/LICENSE` |
| HQ2x algorithm / shader | Lior Halphon, mGBA shader | MIT notice in `third_party/hq2x/hq2x.glsl`, `src/hq2x_shader.h` |
| xBRZ image filter | `a2d8dce723e8fab548bf8460b46eeebfc64abca6` | GPLv3, `third_party/xbrz/License.txt`; **gb_pixel_filters.exe is a separate GPLv3 image-processing program**, complete source included. It is not linked into the frontend or core. |
| Miniz | 3.0.2 | MIT, `third_party/miniz/LICENSE` |
| Lua | 5.4.8, `6e22fedb74cf0c9b6656e9fce8b7331db847c605` | MIT notice in `third_party/lua-src/lua.h` |

Upstream repositories: https://github.com/LIJI32/SameBoy ; https://github.com/RetroAchievements/rcheevos ; https://github.com/mgba-emu/mgba ; https://sourceforge.net/projects/xbrz/ ; https://github.com/richgel999/miniz ; https://github.com/lua/lua .

Studio patches to SameBoy include deterministic host input, output channel gains, APU revision behavior, DMG tile-plane fetch timing, separate DMG/CGB object phases, boot handoff integration, and safe zero-length memory copies. `src/accuracy_bridge.c` implements the C ABI, MBC6 and Studio's BESS extension. The bundled firmware is **SameBoy replacement firmware**, not Nintendo Boot ROMs.

Protocol references: https://gbdev.io/pandocs/ ; https://github.com/LIJI32/SameBoy/blob/master/BESS.md ; https://github.com/RetroAchievements/rcheevos/wiki/rc_client-integration ; https://xiph.org/flac/format.html .

Hardware-test sources: https://github.com/LIJI32/SameSuite ; https://github.com/mattcurrie/mealybug-tearoom-tests ; https://github.com/c-sp/game-boy-test-roms/releases/tag/v7.0 . Reports include exact input hashes; external test ROMs, proprietary Nintendo firmware and commercial games are not bundled.
