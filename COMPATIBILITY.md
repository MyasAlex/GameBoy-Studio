# Compatibility — 1.0 preview, stage 4

The default core is SameBoy 1.0.4 plus Studio changes. The original 0.9 core remains available for legacy snapshots/testing.

The hardware runs and exact counts are recorded in docs/stage4-validation.json: DMG, CGB, SameSuite, and all 71 Mealybug reference images. The seven former image mismatches now match unchanged ROM/reference hashes. The DMG LCD profile follows the existing DMG-blob references; matching every physical DMG LCD variant is not claimed.

The three DMG/MGB boot checks pass after the bundled replacement firmware installs the documented cartridge-entry peripheral phase. No original Nintendo Boot ROM was downloaded or executed. Exact initial APU internals remain unverified.

A pinned corpus of 20 published open games has reproducible input/frame/audio/save smoke runs, with per-run metrics and visual evidence. See docs/STAGE4_REPORT_RU.md and docs/stage4-game-results.json. These runs do not establish full-game completion or commercial-library compatibility. PCM after loading a state can differ because output resampling/filter history is not preserved exactly; this metric remains visible.

The native snapshot layout is 18. A real version-17 fixture imports through BESS; old snapshots without BESS are rejected. Importing an old layout does not preserve exact prior PPU/DMA/APU phases or deterministic replay.

The Windows binary has automated visible-window focus/input/menu checks under Wine. Full native Windows GUI/Direct3D, audible audio, physical accessories, long gameplay, cross-emulator BESS exchange, and signing remain open. This build remains a preview. Historical stage reports are retained.
