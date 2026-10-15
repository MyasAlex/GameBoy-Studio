"""Regression checks for firmware forwarding and honest boot-test results."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tempfile
import types
import unittest
from unittest.mock import patch

import run_rom_tests as runner


BOOT_FAILURES = [
    'acceptance/boot_div-dmgABCmgb.gb',
    'acceptance/boot_hwio-dmgABCmgb.gb',
    'acceptance/serial/boot_sclk_align-dmgABCmgb.gb',
]


def image_module():
    # The renderer is irrelevant to this runner test; simulate equal images.
    image = types.SimpleNamespace(convert=lambda mode: None)
    return types.SimpleNamespace(
        Image=types.SimpleNamespace(open=lambda path: image),
        ImageChops=types.SimpleNamespace(
            difference=lambda actual, expected: types.SimpleNamespace(tobytes=lambda: b'\0\0\0')))


class FirmwareRunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.emulator = self.directory/'emulator.exe'
        self.emulator.touch()
        self.cache = self.directory/'roms'
        self.report = self.directory/'report.json'
        self.arguments = ['--emulator', str(self.emulator), '--cache', str(self.cache),
                          '--report', str(self.report)]

    def run_suite(self, extra_arguments, blargg, execute):
        with patch.object(runner, 'BLARGG', blargg), \
                patch.object(runner.subprocess, 'run', side_effect=execute), \
                patch.dict('sys.modules', {'PIL': image_module()}), \
                contextlib.redirect_stdout(io.StringIO()):
            result = runner.main(self.arguments + extra_arguments)
        return result, json.loads(self.report.read_text(encoding='utf-8'))

    def test_external_firmware_reaches_every_suite_and_boot_failures_stay_open(self):
        boot = self.directory/'DMG boot image.bin'
        boot.write_bytes(bytes(range(256)))
        for name in BOOT_FAILURES:
            rom = self.cache/'mooneye'/runner.MOONEYE_VERSION/name
            rom.parent.mkdir(parents=True, exist_ok=True)
            rom.touch()
        commands = []

        def execute(command, **kwargs):
            commands.append(command)
            failed = Path(command[1]).name in {Path(name).name for name in BOOT_FAILURES}
            return subprocess.CompletedProcess(command, int(failed),
                                               'RESULT: FAIL\n' if failed else 'RESULT: PASS\n', '')

        result, report = self.run_suite(
            ['--mooneye', '--allow-known-failures', '--boot-rom', str(boot)],
            ['cpu_instrs/individual/01-special.gb'], execute)
        self.assertEqual(result, 1)
        self.assertEqual(len(commands), 5)  # Blargg, three boot checks, acid2.
        for command in commands:
            self.assertEqual(command.count('--boot-rom'), 1)
            self.assertEqual(command[command.index('--boot-rom')+1], str(boot.resolve()))
            self.assertEqual(command[command.index('--model')+1], 'dmg')
        self.assertTrue(any('--test' in command for command in commands))
        self.assertTrue(any('--mooneye' in command for command in commands))
        self.assertTrue(any('--output' in command for command in commands))
        self.assertEqual(report['boot_firmware']['source'], 'external')
        self.assertEqual(report['boot_firmware']['sha256'], hashlib.sha256(boot.read_bytes()).hexdigest())
        self.assertEqual(report['boot_firmware']['bytes'], 256)
        self.assertEqual({row['rom'] for row in report['results'] if row['status'] == 'FAIL'},
                         set(BOOT_FAILURES))
        self.assertFalse(any(row['status'] == 'XFAIL' for row in report['results']))

    def test_default_records_replacement_policy_without_inventing_embedded_hash(self):
        commands = []

        def execute(command, **kwargs):
            commands.append(command)
            return subprocess.CompletedProcess(command, 0, '', '')

        result, report = self.run_suite([], [], execute)
        self.assertEqual(result, 0)
        self.assertEqual(len(commands), 1)
        self.assertNotIn('--boot-rom', commands[0])
        self.assertEqual(report['boot_firmware']['source'], 'bundled_sameboy_replacement')
        self.assertIsNone(report['boot_firmware']['sha256'])

    def test_missing_or_non_dmg_boot_is_rejected_before_execution_or_download(self):
        for size in (None, 0, 255, 2048, 2304):
            with self.subTest(size=size):
                boot = self.directory/('missing.bin' if size is None else f'boot-{size}.bin')
                if size is not None:
                    boot.write_bytes(bytes(size))
                with patch.object(runner, 'prepare') as download, \
                        patch.object(runner.subprocess, 'run') as execute, \
                        contextlib.redirect_stderr(io.StringIO()), \
                        self.assertRaises(SystemExit) as exit_status:
                    runner.main(self.arguments + ['--download', '--boot-rom', str(boot)])
                self.assertEqual(exit_status.exception.code, 2)
                download.assert_not_called()
                execute.assert_not_called()


if __name__ == '__main__':
    unittest.main()
