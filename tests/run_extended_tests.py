#!/usr/bin/env python3
"""SameSuite register protocol and Mealybug screenshot comparisons.
Supply an extracted c-sp/game-boy-test-roms v7.0 release; ROMs are not bundled.
Revision-specific tests stay visible as failures; this runner never marks them
as expected-success by excluding them after seeing their result.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess
import time
from PIL import Image

SUITES = ('same-suite', 'mealybug-dmg', 'mealybug-cgb-c', 'mealybug-cgb-d')


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator', required=True, type=Path)
    parser.add_argument('--suite', required=True, type=Path)
    parser.add_argument('--report', type=Path, default=Path('docs/extended-results.json'))
    parser.add_argument('--output', type=Path, default=Path('build/extended-screenshots'))
    parser.add_argument('--workers', type=int, default=3)
    parser.add_argument('--only', choices=('all',) + SUITES, default='all')
    parser.add_argument('--rom', action='append', default=[], metavar='STEM',
                        help='Run only this exact ROM stem; repeat to select several ROMs across available models')
    parser.add_argument('--frames', type=int, default=120,
                        help='Mealybug frame budget before the software breakpoint (default: 120)')
    args = parser.parse_args(argv)
    if not 1 <= args.frames <= 100000:
        parser.error('--frames must be between 1 and 100000')
    return args


def discover_jobs(suite_root, only='all', roms=None):
    jobs = [('same-suite', rom, None)
            for rom in sorted((suite_root / 'same-suite').rglob('*.gb'))]
    for rom in sorted((suite_root / 'mealybug-tearoom-tests' / 'ppu').glob('*.gb')):
        references = [rom.with_name(rom.stem + '_dmg_blob.png'),
                      rom.with_name(rom.stem + '_dmg.png')]
        expected = next((path for path in references if path.is_file()), None)
        if expected:
            jobs.append(('mealybug-dmg', rom, expected))
        for revision in ('c', 'd'):
            expected = rom.with_name(rom.stem + '_cgb_' + revision + '.png')
            if expected.is_file():
                jobs.append(('mealybug-cgb-' + revision, rom, expected))
    selected = set(roms or [])
    return [job for job in jobs
            if (only == 'all' or job[0] == only)
            and (not selected or job[1].stem in selected)]


def model_for(suite, rom, suite_root):
    if suite != 'same-suite':
        return 'dmg' if suite == 'mealybug-dmg' else 'cgb' + suite[-1]
    # Path components work with native Windows separators as well as POSIX paths.
    model = 'sgb' if 'sgb' in rom.relative_to(suite_root).parts else 'cgb'
    # The ROM suffix specifies its actual hardware target. Do not compare a
    # revision-B expected table against revision E.
    if 'cgb0BC' in rom.stem:
        model = 'cgbc'
    elif 'cgb0B' in rom.stem or 'cgbB' in rom.stem:
        model = 'cgbb'
    elif 'cgb0' in rom.stem:
        model = 'cgb0'
    elif rom.stem.endswith('-A'):
        model = 'agb'
    return model


def run_job(job, args):
    suite, rom, expected = job
    model = model_for(suite, rom, args.suite)
    command = [str(args.emulator.resolve()), str(rom.resolve()), '--model', model]
    output = args.output / (suite + '-' + rom.stem + '.ppm')
    if suite == 'same-suite':
        command += ['--mooneye', '--frames', '1200']
    else:
        command += ['--ppu-test', '--frames', str(args.frames), '--output', str(output.resolve())]
        if model == 'dmg':
            command += ['--grey']
    start = time.monotonic()
    result = {'suite': suite, 'rom': rom.relative_to(args.suite).as_posix(),
              'sha256': hashlib.sha256(rom.read_bytes()).hexdigest(), 'model': model}
    try:
        if expected:
            # A failed launch must never reuse a screenshot from an earlier run.
            output.unlink(missing_ok=True)
        process = subprocess.run(command, capture_output=True, text=True, timeout=90)
        result['output'] = process.stdout[-1400:] + process.stderr[-500:]
        completed = process.returncode == 0 and 'RESULT: PASS' in process.stdout.splitlines()
        if expected:
            result.update(expected=expected.relative_to(args.suite).as_posix(),
                          expected_sha256=hashlib.sha256(expected.read_bytes()).hexdigest(),
                          capture_completed=completed, differing_pixels=None, status='FAIL')
            if output.is_file():
                with Image.open(output) as actual_image, Image.open(expected) as expected_image:
                    actual, reference = actual_image.convert('RGB'), expected_image.convert('RGB')
                    if actual.size == reference.size:
                        actual_raw, reference_raw = actual.tobytes(), reference.tobytes()
                        result['differing_pixels'] = sum(
                            actual_raw[i:i + 3] != reference_raw[i:i + 3]
                            for i in range(0, len(actual_raw), 3))
                    else:
                        result['error'] = 'Screenshot dimensions differ from the reference.'
                if completed and result['differing_pixels'] == 0:
                    result['status'] = 'PASS'
            else:
                result['error'] = 'Emulator did not produce a fresh screenshot.'
        else:
            result['status'] = 'PASS' if completed else 'FAIL'
    except (OSError, subprocess.TimeoutExpired) as error:
        result.update(status='ERROR', output=str(error))
    result['seconds'] = round(time.monotonic() - start, 3)
    return result


def main(argv=None):
    args = parse_args(argv)
    jobs = discover_jobs(args.suite, args.only, args.rom)
    missing = set(args.rom) - {job[1].stem for job in jobs}
    if missing:
        raise SystemExit('No tests found for selected ROM(s): ' + ', '.join(sorted(missing)))
    if not jobs:
        selection = ' (ROM stems: ' + ', '.join(args.rom) + ')' if args.rom else ''
        raise SystemExit('No tests found for ' + args.only + selection + ' in ' + str(args.suite))
    args.output.mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(max_workers=max(1, min(args.workers, 6))) as pool:
        results = list(pool.map(lambda job: run_job(job, args), jobs))
    report = {'source_release': 'https://github.com/c-sp/game-boy-test-roms/releases/tag/v7.0',
              'protocols': ['https://github.com/LIJI32/SameSuite',
                            'https://github.com/mattcurrie/mealybug-tearoom-tests'],
              'limitations': 'SameSuite targets each revision-specific suffix separately. Mealybug DMG-B compares blob references; original Boot ROM and physical devices are not part of this run.',
              'tests': results}
    report['summary'] = {
        suite: {status: sum(row['suite'] == suite and row['status'] == status for row in results)
                for status in ('PASS', 'FAIL', 'ERROR')}
        for suite in SUITES}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report['summary'], ensure_ascii=False))
    return 0 if all(row['status'] == 'PASS' for row in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
