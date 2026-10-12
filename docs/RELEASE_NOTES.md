# GameBoy Studio 0.8 — native Studio and session tools

The verified DMG/CGB core now has external boot configuration, hardened state/file readers, rewind, named/quick saves and reproducible frame-input movies. The Windows interface adds RU/EN menus, display-only palettes/LCD/CRT/ghosting, comparison, speed/frame controls, recent games and session restore.

WASAPI runs on an event worker using a lock-free SPSC buffer, with waveOut fallback and raw PCM WAV recording. Studio provides CPU stepping/disassembly/registers/PC stops/value-change watches/trace and ROM JSON diagnostics. IPS/BPS patches load as separate copies of the original cartridge.

Validation: ten native suites, four new-suite AddressSanitizer runs, 145 pinned ROM/image checks, and hidden DMG/CGB GUI renderer checks. See COMPATIBILITY.md and docs/validation-summary.json for exact scope. This is the first major stage of the requested roadmap; netplay, SGB/accessories, uncommon mappers, RetroAchievements, GPU shaders and other pending features are recorded in docs/FEATURE_STATUS_RU.md.

Extract the Windows ZIP completely and run GBStudio.exe. SHA256SUMS.txt verifies downloaded archives. Save states retain native ABI constraints; named exports protect source ROM/boot paths. Commercial games and manufacturer boot ROMs are not included.