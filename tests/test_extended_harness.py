"""Regression tests for false success and hardware selection in ROM testing."""
from pathlib import Path, PureWindowsPath
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from PIL import Image
import run_extended_tests as runner


class ExtendedHarnessTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        root = Path(self.temporary.name)
        self.rom = root / 'ppu-test.gb'
        self.rom.write_bytes(bytes(32768))
        self.reference = root / 'ppu-test_dmg.png'
        Image.new('RGB', (160, 144), 'white').save(self.reference)
        self.args = SimpleNamespace(suite=root, output=root, emulator=root / 'emulator.exe', frames=120)
        self.job = ('mealybug-dmg', self.rom, self.reference)
        self.screenshot = root / 'mealybug-dmg-ppu-test.ppm'

    def process(self, marker='RESULT: PASS\n', returncode=0, create_image=True):
        if create_image:
            Image.new('RGB', (160, 144), 'white').save(self.screenshot)
        return subprocess.CompletedProcess([], returncode, marker, '')

    def test_success_requires_breakpoint_and_fresh_matching_image(self):
        with patch.object(runner.subprocess, 'run', side_effect=lambda *a, **k: self.process()):
            result = runner.run_job(self.job, self.args)
        self.assertEqual(result['status'], 'PASS')
        self.assertTrue(result['capture_completed'])
        self.assertEqual(result['differing_pixels'], 0)

    def test_matching_budget_screenshot_is_not_success(self):
        with patch.object(runner.subprocess, 'run', side_effect=lambda *a, **k: self.process('RESULT: TIMEOUT\n')):
            result = runner.run_job(self.job, self.args)
        self.assertEqual(result['status'], 'FAIL')
        self.assertFalse(result['capture_completed'])
        self.assertEqual(result['differing_pixels'], 0)

    def test_previous_screenshot_is_deleted_before_failed_launch(self):
        self.process()
        with patch.object(runner.subprocess, 'run', side_effect=OSError('launch failed')):
            result = runner.run_job(self.job, self.args)
        self.assertEqual(result['status'], 'ERROR')
        self.assertFalse(self.screenshot.exists())

    def test_success_marker_without_new_screenshot_fails(self):
        with patch.object(runner.subprocess, 'run', side_effect=lambda *a, **k: self.process(create_image=False)):
            result = runner.run_job(self.job, self.args)
        self.assertEqual(result['status'], 'FAIL')
        self.assertIsNone(result['differing_pixels'])

    def test_nonzero_exit_cannot_pass_with_matching_image(self):
        with patch.object(runner.subprocess, 'run', side_effect=lambda *a, **k: self.process(returncode=1)):
            result = runner.run_job(self.job, self.args)
        self.assertEqual(result['status'], 'FAIL')
        self.assertFalse(result['capture_completed'])

    def test_sgb_path_detection_is_portable(self):
        root = PureWindowsPath(r'C:\suite')
        rom = root / 'same-suite' / 'apu' / 'sgb' / 'channel_1.gb'
        self.assertEqual(runner.model_for('same-suite', rom, root), 'sgb')
        root = Path('/suite')
        rom = root / 'same-suite' / 'apu' / 'sgb' / 'channel_1.gb'
        self.assertEqual(runner.model_for('same-suite', rom, root), 'sgb')

    def test_revision_suffixes_preserve_hardware_selection(self):
        cases = {'align.gb': 'cgb', 'timing-cgb0BC.gb': 'cgbc',
                 'length-cgb0B.gb': 'cgbb', 'wave-cgbB.gb': 'cgbb',
                 'wave-cgb0.gb': 'cgb0', 'timing-A.gb': 'agb'}
        for name, expected in cases.items():
            with self.subTest(name=name):
                self.assertEqual(runner.model_for('same-suite', self.args.suite / name, self.args.suite), expected)

    def test_empty_suite_is_not_a_successful_run(self):
        with self.assertRaisesRegex(SystemExit, 'No tests found'):
            runner.main(['--emulator', str(self.args.emulator), '--suite', str(self.args.suite)])

    def test_repeatable_rom_filter_keeps_available_models_without_prefix_matches(self):
        ppu = self.args.suite / 'mealybug-tearoom-tests' / 'ppu'
        ppu.mkdir(parents=True)
        for stem in ('obj_size', 'obj_size_scx', 'wx_sprites'):
            (ppu / (stem + '.gb')).touch()
            for suffix in ('dmg_blob', 'cgb_c', 'cgb_d'):
                (ppu / (stem + '_' + suffix + '.png')).touch()
        args = runner.parse_args(['--emulator', str(self.args.emulator),
                                  '--suite', str(self.args.suite),
                                  '--rom', 'obj_size', '--rom', 'wx_sprites'])
        jobs = runner.discover_jobs(args.suite, args.only, args.rom)
        self.assertEqual(len(jobs), 6)
        self.assertEqual({job[1].stem for job in jobs}, {'obj_size', 'wx_sprites'})
        for stem in ('obj_size', 'wx_sprites'):
            self.assertEqual({job[0] for job in jobs if job[1].stem == stem},
                             {'mealybug-dmg', 'mealybug-cgb-c', 'mealybug-cgb-d'})
        cgb_c = runner.discover_jobs(args.suite, 'mealybug-cgb-c', args.rom)
        self.assertEqual(len(cgb_c), 2)

    def test_rom_filter_with_no_matches_rejects_empty_run(self):
        suite = self.args.suite / 'same-suite'
        suite.mkdir()
        (suite / 'available.gb').touch()
        with self.assertRaisesRegex(SystemExit, 'No tests found.*missing'):
            runner.main(['--emulator', str(self.args.emulator), '--suite', str(self.args.suite),
                         '--rom', 'missing'])

    def test_each_requested_rom_must_be_present(self):
        suite = self.args.suite / 'same-suite'
        suite.mkdir()
        (suite / 'available.gb').touch()
        with self.assertRaisesRegex(SystemExit, 'No tests found.*missing'):
            runner.main(['--emulator', str(self.args.emulator), '--suite', str(self.args.suite),
                         '--rom', 'available', '--rom', 'missing'])


if __name__ == '__main__':
    unittest.main()
