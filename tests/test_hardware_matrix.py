"""Check hardware-ROM result protocols and isolation without downloading ROMs."""
import contextlib
import io
from pathlib import Path
import subprocess
import tempfile
import types
import unittest
from unittest.mock import patch

import run_hardware_matrix as runner


class HardwareMatrixTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.suite = self.directory / 'suite'
        self.source = self.suite / 'gbmicrotest' / 'sample.gb'
        self.source.parent.mkdir(parents=True)
        self.source.write_bytes(b'original test ROM')
        self.emulator = self.directory / 'headless'
        self.emulator.write_bytes(b'placeholder executable')
        self.arguments = types.SimpleNamespace(
            suite_root=self.suite, emulator=self.emulator, frames=None,
            timeout=5, boot_rom=None)

    def execute(self, protocol, output='', code=0, dump=b'\x00\x00\x01'):
        command_seen = []

        def fake_process(command, **unused):
            command_seen.append(command)
            cartridge = Path(command[1])
            self.assertNotEqual(cartridge.parent, self.source.parent)
            self.assertEqual(cartridge.read_bytes(), self.source.read_bytes())
            if '--dump' in command and dump is not None:
                Path(command[-1]).write_bytes(dump)
            return subprocess.CompletedProcess(command, code, output, '')

        job = ('gbmicrotest' if protocol == 'hram_ff82' else 'mooneye-cgb',
               self.source, 'dmg' if protocol == 'hram_ff82' else 'cgbe', protocol)
        with patch.object(runner.subprocess, 'run', side_effect=fake_process):
            row = runner.run_job(job, self.arguments)
        return row, command_seen[0]

    def test_hram_requires_exact_complete_dump_and_terminal_flag(self):
        for flag, status in ((1, 'PASS'), (255, 'FAIL'), (0, 'INCOMPLETE'),
                             (2, 'INCOMPLETE')):
            with self.subTest(flag=flag):
                row, command = self.execute('hram_ff82', dump=bytes((3, 4, flag)))
                self.assertEqual(row['status'], status)
                self.assertEqual(command[command.index('--dump') + 1:-1], ['FF80', '3'])
        for data in (None, b'', b'\x01', b'\0\0\x01\0'):
            with self.subTest(data=data):
                row, unused = self.execute('hram_ff82', dump=data)
                self.assertEqual(row['status'], 'ERROR')

    def test_nonzero_process_exit_cannot_claim_pass(self):
        for protocol in ('hram_ff82', 'fibonacci_breakpoint'):
            with self.subTest(protocol=protocol):
                row, unused = self.execute(protocol, output='RESULT: PASS\n', code=2)
                self.assertEqual(row['status'], 'ERROR')

    def test_fibonacci_requires_an_exact_terminal_line(self):
        cases = [('RESULT: PASS\n', 0, 'PASS'),
                 ('RESULT: FAIL\n', 1, 'FAIL'),
                 ('RESULT: TIMEOUT\n', 1, 'INCOMPLETE'),
                 ('log mentions RESULT: PASS\n', 0, 'ERROR'),
                 ('RESULT: PASS eventually\n', 0, 'ERROR'),
                 ('', 0, 'ERROR')]
        for output, code, expected in cases:
            with self.subTest(output=output):
                row, command = self.execute('fibonacci_breakpoint', output, code)
                self.assertEqual(row['status'], expected)
                self.assertIn('--mooneye', command)
                self.assertEqual(command[command.index('--frames') + 1], '1200')

    def test_battery_writes_cannot_modify_cached_test_files(self):
        battery = self.source.with_suffix('.sav')
        battery.write_bytes(b'existing battery data')
        written = []

        def fake_process(command, **unused):
            cartridge = Path(command[1])
            generated = cartridge.with_suffix('.sav')
            generated.write_bytes(b'new battery data')
            written.append(generated)
            Path(command[-1]).write_bytes(b'\0\0\1')
            return subprocess.CompletedProcess(command, 0, '', '')

        with patch.object(runner.subprocess, 'run', side_effect=fake_process):
            result = runner.run_job(('gbmicrotest', self.source, 'dmg', 'hram_ff82'),
                                    self.arguments)
        self.assertEqual(result['status'], 'PASS')
        self.assertEqual(battery.read_bytes(), b'existing battery data')
        self.assertEqual(self.source.read_bytes(), b'original test ROM')
        self.assertFalse(written[0].exists())

    def test_host_timeout_stays_visible(self):
        with patch.object(runner.subprocess, 'run',
                          side_effect=subprocess.TimeoutExpired('headless', 5)):
            row = runner.run_job(('gbmicrotest', self.source, 'dmg', 'hram_ff82'),
                                 self.arguments)
        self.assertEqual(row['status'], 'TIMEOUT')

    def test_mooneye_model_suffixes_do_not_run_on_incompatible_hardware(self):
        for relative in ('acceptance/common.gb', 'acceptance/dmg-dmgABC.gb',
                         'acceptance/sgb-GS.gb', 'misc/boot-C.gb',
                         'misc/boot-A.gb', 'misc/boot-cgb0.gb'):
            path = self.suite / 'mooneye-test-suite' / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        jobs = runner.discover_jobs(self.suite, 'mooneye-cgb')
        self.assertEqual({job[1].name: job[2] for job in jobs},
                         {'common.gb': 'cgbe', 'boot-C.gb': 'cgbe',
                          'boot-A.gb': 'agb', 'boot-cgb0.gb': 'cgb0'})

    def test_report_cannot_replace_emulator_or_test_rom(self):
        base = ['--suite-root', str(self.suite), '--emulator', str(self.emulator)]
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            runner.parse_args(base + ['--report', str(self.emulator)])
        with self.assertRaisesRegex(SystemExit, 'must not replace a test ROM'):
            runner.main(base + ['--only', 'gbmicrotest', '--report', str(self.source)])


if __name__ == '__main__':
    unittest.main()
