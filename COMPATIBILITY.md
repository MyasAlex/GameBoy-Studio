# Compatibility — 1.0 preview

The default core is SameBoy 1.0.4 plus Studio changes, not the original 0.9 core. It supports DMG/MGB/CGB revisions/AGB/SGB HLE and the requested MBC6/MBC7/MMM01/HuC controllers. Actual frontend integration is covered by the native suites.

Current hardware collection: DMG 127/130, CGB 15/15, SameSuite 78/78, Mealybug DMG 23/24, CGB-C 21/27, CGB-D 16/20. Every failure and exact ROM/reference hash stays visible in docs/1.0-*-results.json. Three DMG failures concern initial values after proprietary boot firmware; eleven image failures remain open. No commercial game, Pokemon-trade or original Nintendo Boot-ROM run is claimed.

Native BESS roundtrips are exact for thirteen tested models; standard foreign BESS parsing is also exercised by the coverage fuzzer. Actual exchange with a different emulator has not been run. MSVC uses the included engine DLL; MinGW can compile its GNU-C source. Windows GUI, Direct3D SM4.0 and physical accessories remain untested in this environment.

See docs/FEATURE_STATUS_RU.md for the complete feature-by-feature limits and docs/validation-1.0.json for the final local build evidence. docs/compatibility.tsv powers the searchable database window and contains recorded tests, not invented commercial-game scores.
