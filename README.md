# Game Boy Studio 0.6 — DMG

An original C++20 Game Boy emulator with a native Windows frontend. Development prioritizes a verified monochrome Game Boy core; Game Boy Color is the next stage.

**[Русская инструкция](README_RU.md) · [Compatibility and test evidence](COMPATIBILITY.md)**

## Run on Windows

Download `releases/GBStudio-0.6-DMG-Windows-x64.zip`, extract it, and run **GBStudio.exe**. The original homebrew demo opens automatically. Open your `.gb` file with **Ctrl+O**, or drag it onto the window. Games and Nintendo boot ROMs are not bundled.

In the complete project archive, run `START_WINDOWS.bat` or `dist/GBStudio.exe`. To build the sources, open `GBStudio.sln` in Visual Studio 2022/2026, select **Release / x64**, and press **F5**. `BUILD_WINDOWS.bat` also builds and checks the core automatically using the installed C++ compiler; it does not require CMake.

## Implemented

- All documented SM83 base and CB instructions, machine-cycle bus accesses, EI delay, HALT bug and interrupt-dispatch cancellation.
- Divider/TIMA falling edges and delayed reload/write behavior; serial transfer timing.
- DMG 160×144 dot renderer, signed tile addressing, scrolling, window line counter, first ten OAM objects, DMG object priority, palettes and LCD/STAT timings.
- OAM DMA startup/restart, source mirrors and video/main-bus restrictions.
- Four clock-driven audio channels: pulse, sweep, wave and noise; length/envelope counters, stereo routing and a high-pass filter.
- ROM-only, MBC1/MBC1M, MBC2, MBC3/RTC and MBC5/rumble-bank handling.
- Atomic cartridge saves, checked save states containing CPU/PPU/APU/DMA/timer state, screenshots, four palettes, integer scaling, fullscreen, turbo and XInput controllers.
- Completed-frame presentation with a Win32 back buffer, at the actual 4,194,304 Hz / 70,224 clocks per frame.

The project does not claim perfect compatibility with every commercial game. See the verified results and remaining limits in [COMPATIBILITY.md](COMPATIBILITY.md).

## Controls

| Action | Keyboard | XInput controller |
|---|---|---|
| Directions | Arrow keys | D-pad / left stick |
| A / B | Z / X | A / B |
| Start / Select | Enter / Shift | Start / Back |
| Pause | P | — |
| Save / load state | F5 / F9 | — |
| Fullscreen | F11; Esc to leave | — |
| Palette / screenshot | F1 / F12 | — |
| Turbo | Hold Tab | — |

The app pauses when it loses focus. Battery saves (`.sav`, `.rtc`) and states (`.state`) are written next to the ROM. F12 writes a BMP screenshot there.

## CMake and tests

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python -m pip install Pillow
python tests/run_rom_tests.py --emulator build/gb_headless --download --mooneye
```

On Windows use `--config Release`, `ctest -C Release`, and `build/Release/gb_headless.exe`. The test runner downloads pinned public hardware test ROMs and compares dmg-acid2 to its reference image. Its JSON report records each result and explicitly lists commercial games as untested.

## Color roadmap

Add the CGB hardware alongside this verified DMG path: VRAM/WRAM banks and attributes, BG/OBJ color palettes, KEY1 double speed, HDMA/GDMA, CGB interrupt/bus timing and cgb-acid2/CGB sound tests. CGB-only cartridges currently produce an explanatory error; dual-compatible cartridges run in DMG mode.
