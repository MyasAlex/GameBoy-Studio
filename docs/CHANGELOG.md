# 0.6 DMG compatibility

CPU memory accesses now advance the rest of the machine at each bus cycle. Interrupt dispatch, EI/HALT/STOP, timer reloads, serial edges and DMA startup/restart follow independently verified timing.

The renderer handles the window's internal line counter, object-to-object and object-to-background priority, ten-object selection, LCD startup, LY/STAT comparison and separate read/write memory-access windows. A completed frame is published at VBlank; Win32 uses a separate back buffer.

Audio generation moved from host callbacks into the emulated clock. Four channels now have length, envelope, sweep, DAC state, wave access/corruption behavior and stereo routing. The host consumes queued PCM without changing the emulated APU state.

Save states include all emulated timing and audio/pixel pipeline state and use ROM identification plus a checksum. State/cartridge writes commit through temporary files. Corrupt states leave the running machine intact.

The Windows app adds automatic demo loading, F11 fullscreen, XInput with throttled polling, focus pause, a shorter audio queue and explicit cartridge-load errors. The original demo now follows real VRAM/OAM access restrictions.

Added a Visual Studio solution, a one-click compiler script, repeatable pinned-ROM/image tests, per-test evidence and Windows/Linux CI builds. Color remains a separate future hardware model.
