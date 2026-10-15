"""Exercise PPU completion and budget handling against a real headless build."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest


class HeadlessProtocols(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def rom(self, name, program):
        image = bytearray(DEMO.read_bytes())
        image[0x100:0x104] = b'\xc3\x50\x01\x00'
        image[0x150:0x150+len(program)] = program
        result = self.directory/name
        result.write_bytes(image)
        return result

    def execute(self, rom, *options):
        return subprocess.run([str(EMULATOR), str(rom), *options],
                              capture_output=True, text=True, timeout=20)

    def test_capture_requires_software_breakpoint(self):
        ready = self.rom('ready.gb', b'\x40\x18\xfe')
        for core in ('accurate', 'original'):
            with self.subTest(core=core):
                result = self.execute(ready, '--core', core, '--ppu-test')
                self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
                self.assertIn('RESULT: PASS', result.stdout)

    def test_budget_exhaustion_cannot_pass_even_if_image_export_succeeds(self):
        pending = self.rom('pending.gb', b'\x18\xfe')
        screenshot = self.directory/'pending.ppm'
        result = self.execute(pending, '--ppu-test', '--frames', '1',
                              '--output', str(screenshot))
        self.assertEqual(result.returncode, 1, result.stdout+result.stderr)
        self.assertIn('RESULT: TIMEOUT', result.stdout)
        self.assertNotIn('RESULT: PASS', result.stdout)
        self.assertEqual(len(screenshot.read_bytes()), len(b'P6\n160 144\n255\n')+160*144*3)

    def test_debugger_stop_cannot_claim_capture(self):
        pending = self.rom('stopped.gb', b'\x18\xfe')
        result = self.execute(pending, '--ppu-test', '--breakpoint', '100')
        self.assertEqual(result.returncode, 1)
        self.assertIn('RESULT: TIMEOUT', result.stdout)

    def test_conflicting_modes_are_rejected(self):
        ready = self.rom('invalid.gb', b'\x40')
        for options in (('--ppu-test', '--test'), ('--ppu-test', '--mooneye'),
                        ('--ppu-test', '--report-only')):
            with self.subTest(options=options):
                self.assertEqual(self.execute(ready, *options).returncode, 2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--emulator', type=Path, required=True)
    parser.add_argument('--demo', type=Path, required=True)
    args, remaining = parser.parse_known_args()
    EMULATOR, DEMO = args.emulator.resolve(), args.demo.resolve()
    unittest.main(argv=[__file__, *remaining])
