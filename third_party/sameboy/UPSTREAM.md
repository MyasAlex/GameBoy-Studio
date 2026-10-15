SameBoy 1.0.4, source commit c4cf7abab79a4e22dc17e0704995976cb2751b2d
https://github.com/LIJI32/SameBoy
Core and replacement firmware: Expat license, see LICENSE.
Boot binaries from official sameboy_winsdl_v1.0.4.zip. No Nintendo firmware included.

Local changes: channel output gains; wave/noise revision behavior; DMG fetch-plane and BG enable timing; separate DMG/CGB object-read phases; CGB low-plane address latch one dot before data access; WX reactivation zero-pixel insertion also on CGB; zero-length copy guards and defined SUB/SBC shifts; optional observational PPU VRAM/OAM callbacks that disable line batching while watched. STRUCT_VERSION is 17 for the new OBJ continuation state and latch semantics. Older version-16 snapshots use the embedded BESS import path; exact old sub-cycle continuation is not preserved.

Studio C ABI v3 is implemented by src/accuracy_bridge.c and declared in src/accuracy_api.h. It integrates native batteries, free boot handoff, cameras, Printer, serial/IR/rumble, direct memory editing and actual FIFO/APU diagnostics. MBC6 is a separate bridge controller in src/mbc6.h. Studio's GBSX block saves key/serial/settings/camera/frame buffers and MBC6 state. The wrapper's GSTU block adds ROM/model identity, checksum, timing, input and achievement progress.

MSVC cannot compile the GNU-C core; prebuilt/gb_accuracy.dll is the exact DLL rebuilt from this source with MinGW GCC 16.2.0 (w64devkit 2.10.1), Release. MinGW CMake builds regenerate it. MSVC builds use the included binary through the same verified C ABI v3. _GNU_SOURCE exposes vasprintf on modern MinGW/GCC. The DLL imports only KERNEL32.dll and msvcrt.dll. See docs/stage2-validation.json for its hash and unresolved hardware tests; docs/validation-1.0.json is historical.
