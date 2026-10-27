# 1.0 preview

Stage 3 Windows timing update: discard Turbo time debt, cancel the Turbo override when selecting normal speed, bound catch-up batches, add Ctrl+0 and show measured speed. Six hidden Win32 timer-loop probes passed with sound off/on. Persistent acceleration in a user game remains unconfirmed. The stage 2 accuracy core is unchanged; see STAGE3_REPORT_RU.md for scope and measurements.

This is a major implementation update to 0.9. The default engine is now SameBoy 1.0.4, integrated through a versioned C ABI and patched for Studio. The original core is retained for legacy snapshots/testing.

Added: hardware revisions and replacement boot palettes; SGB/SGB2/PAL frames and commands; MBC6/MMM01/HuC3; full native RTC/flash batteries; exact dual-console snapshots; TCP heartbeat/reconnect and rollback; official rc_client runtime with progress snapshots and Hardcore restrictions; HQ2x and separate xBRZ process; GPU scalers/color/ghosting; PCM audio in AVI; PNG/JPEG covers; independent Lua plugins; actual FIFO/APU state and PPU bus watches; TAS grid with Camera/motion recording; hardware comparison, branch graph, virtual controls, gamepad profiles, homebrew/compatibility windows, Discord and signing/uninstall tooling.

All native test suites and sanitizer results are recorded in validation-1.0.json. Every ROM failure remains visible in 1.0-*-results.json. The full requested roadmap still has limits detailed in FEATURE_STATUS_RU.md: native Windows/device QA, real games/boot assets, fine PPU effects, external services and cross-emulator state checks. This is a preview, not a compatibility or signed-release guarantee.
