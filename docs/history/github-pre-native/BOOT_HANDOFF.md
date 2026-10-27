# Default DMG/MGB cartridge handoff

The accurate backend runs the bundled MIT-licensed SameBoy replacement firmware
before entering a cartridge. Its animation has a different duration from the
manufacturer firmware. Previously its animation's exit phase leaked into game
execution: on DMG-B the divider was `$BD1C`, and the LCD was on line 144 with
`STAT=$81`. This failed all three DMG ABC/MGB Mooneye boot timing checks.

The default DMG-B and MGB paths now finish the replacement firmware by advancing
peripherals with the CPU held at `$0100` to physical line 153, dot 396. The running
LCD state machine sets `LY=0` and `STAT=$85` coherently. The initial divider phase
is then installed as `$ABCC`, with the serial master clock at the corresponding
bit-8 phase. This is initial-state installation, so it does not synthesize a CPU
write to DIV or clock TIMA/APU/serial during the phase change. The firmware's
initialized VRAM, registers, and audio state remain in use.

This profile applies to default DMG-B, Pocket and Light (the latter two share the
MGB model). It does not examine ROM names, bytes, checksums, or test identities.
It does not apply to supplied external boot firmware, CGB/AGB or SGB firmware.
External firmware continues to execute from `$0000` with its own timing.

References used for the model state and validation:

- [Pan Docs, Power-Up Sequence](https://gbdev.io/pandocs/Power_Up_Sequence.html):
  monochrome handoff `DIV=$AB`, `LY=0`, `STAT=$85`.
- [Gambatte initial LCD phase](https://github.com/EmulatorJS/gambatte-libretro/blob/master/libgambatte/src/initstate.cpp):
  monochrome physical line 153, dot 396. Only this numeric phase fact is used;
  Gambatte implementation code and state dumps are not included.
- [Mooneye boot_div-dmgABCmgb](https://github.com/Gekkio/mooneye-test-suite/blob/main/acceptance/boot_div-dmgABCmgb.s):
  verified hardware DIV value and phase at cartridge entry; its six reads cover
  both sides of divider edges.
- [Mooneye boot_sclk_align-dmgABCmgb](https://github.com/Gekkio/mooneye-test-suite/blob/main/acceptance/serial/boot_sclk_align-dmgABCmgb.s):
  independently checks the initial serial clock phase by the interrupt timing.
- [Mooneye boot_hwio-dmgABCmgb](https://github.com/Gekkio/mooneye-test-suite/blob/main/acceptance/boot_hwio-dmgABCmgb.s):
  checks hardware IO through timed reads after entry, including the LCD phase.

The native `boot_handoff` regression test also verifies reset, serial save/load
continuation at the handoff boundary, and a synthetic external firmware whose
handed-off DIV/LCD state must remain its own. Public Mooneye ROM results are
recorded separately. Passing these checks validates the tested handoff profile;
it does not certify the manufacturer boot ROM, exact startup audio internals,
other hardware revisions, or every power-on RAM pattern. No manufacturer boot
ROM was downloaded, bundled or executed for this change.

## MBC6 boot mapping regression

The stricter handoff check exposed a pre-existing MBC6 bridge error: its custom
cartridge read callback replaced the core's already-mapped boot-ROM bytes. This
could execute cartridge data in place of firmware and leave the LCD disabled at
entry. MBC6 now observes the same boot-overlay priority as other cartridges:
`$0000–$00FF`, plus `$0200–$08FF` on CGB, come from the internal boot mapping until
FF50 unmaps it. The CGB header hole `$0100–$01FF` remains on the cartridge bus.
The native regression covers default MBC6 startup and synthetic supplied DMG/CGB
firmware, including the second CGB segment, header hole, and unmapping.
