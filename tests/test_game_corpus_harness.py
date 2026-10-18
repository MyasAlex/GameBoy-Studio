#!/usr/bin/env python3
"""Evidence-integrity checks: failed launches must not reuse earlier pass artifacts."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('game_harness', Path(__file__).with_name('run_game_compatibility.py'))
HARNESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HARNESS)


class EvidenceIntegrity(unittest.TestCase):
    def test_failed_launch_removes_stale_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus, output = root / 'corpus', root / 'out'
            corpus.mkdir()
            (corpus / 'fixture.gb').write_bytes(b'fixture')
            destination = output / 'fixture-dmg'
            destination.mkdir(parents=True)
            (destination / 'run.json').write_text(json.dumps({'save_state_continuation_exact': True}))
            (destination / 'frame-999.ppm').write_bytes(b'stale')
            (destination / 'gameplay-audio.wav').write_bytes(b'stale')
            game = dict(id='fixture', filename='fixture.gb', title='Fixture', license='test',
                        sha256=hashlib.sha256(b'fixture').hexdigest(), cartridge_type=0)
            # A nonexistent executable is portable and cannot emit fresh evidence.
            result = HARNESS.run_job((game, 'dmg'), root / 'missing-runner', corpus, output, 2400)
            self.assertEqual(result['exit_code'], 126)
            self.assertEqual(result['model'], 'dmg')
            self.assertNotIn('save_state_continuation_exact', result)
            self.assertNotIn('screenshots', result)
            self.assertNotIn('audio_wav_sha256', result)
            self.assertFalse((destination / 'frame-999.ppm').exists())
            self.assertFalse((destination / 'gameplay-audio.wav').exists())


if __name__ == '__main__':
    unittest.main()
