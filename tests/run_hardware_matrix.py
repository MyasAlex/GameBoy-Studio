#!/usr/bin/env python3
"""Measure additional documented ROM protocols from the cached c-sp v7.0 suite.

GBMicrotest: DMG-B/C, HRAM FF82 = 01 (pass) or FF (fail). 30 frames by
default includes the documented ~380 ms is_if_set_during_ime0 exception.
Mooneye: Fibonacci register breakpoint protocol, on a supported CGB/AGB
target selected from the author's filename suffixes. Boot tests explicitly
measure Studio's post-boot initialization unless an external image is supplied.

ROMs are copied into per-run temporary directories so the headless tool cannot
create battery files beside the cached public test ROMs. No ROMs are bundled.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite-root', type=Path, required=True,
                        help='Extracted pinned c-sp v7.0 suite directory')
    parser.add_argument('--emulator', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--only', choices=('all', 'gbmicrotest', 'mooneye-cgb'), default='all')
    parser.add_argument('--frames', type=int,
                        help='Override budget: defaults are 30 for GBMicrotest and 1200 for Mooneye')
    parser.add_argument('--workers', type=int, default=2)
    parser.add_argument('--timeout', type=float, default=60,
                        help='Per-ROM host timeout in seconds')
    parser.add_argument('--rom', action='append', default=[],
                        help='Select exact ROM stem; repeatable')
    parser.add_argument('--boot-rom', type=Path,
                        help='Optional CGB boot image for the Mooneye CGB/AGB cases')
    args = parser.parse_args(argv)
    if args.frames is not None and not 1 <= args.frames <= 100000:
        parser.error('--frames must be between 1 and 100000')
    if not 1 <= args.workers <= 8 or not 0 < args.timeout <= 600:
        parser.error('--workers must be 1..8 and --timeout must be >0 and <=600')
    args.emulator = args.emulator.resolve()
    args.suite_root = args.suite_root.resolve()
    args.report = args.report.resolve()
    if not args.emulator.is_file():
        parser.error('Emulator executable is missing')
    if not args.suite_root.is_dir():
        parser.error('Suite root is missing')
    if args.boot_rom and (not args.boot_rom.is_file() or args.boot_rom.stat().st_size not in (0x800, 0x900)):
        parser.error('CGB boot image must be a complete 0x800/0x900 byte file')
    protected = [args.emulator]
    if args.boot_rom:
        protected.append(args.boot_rom.resolve())
    if args.report in protected or args.report.with_suffix(args.report.suffix + '.tmp') in protected:
        parser.error('Report output must not replace an executable or boot image')
    return args


def discover_jobs(root, only='all', stems=()):
    jobs = []
    if only in ('all', 'gbmicrotest'):
        jobs.extend(('gbmicrotest', rom, 'dmg', 'hram_ff82')
                    for rom in sorted((root / 'gbmicrotest').glob('*.gb')))
    if only in ('all', 'mooneye-cgb'):
        suite = root / 'mooneye-test-suite'
        for family in ('acceptance', 'emulator-only', 'misc'):
            for rom in sorted((suite / family).rglob('*.gb')):
                # Unsuffixed acceptance/emulator-only tests are the author's
                # common tests. G/S and DMG/MGB-only variants must not run as CGB.
                if '-' not in rom.stem and family != 'misc':
                    model = 'cgbe'
                elif family == 'misc':
                    suffix = rom.stem.rsplit('-', 1)[-1]
                    model = {'C': 'cgbe', 'A': 'agb', 'cgb0': 'cgb0',
                             'cgbABCDE': 'cgbe', 'cgb': 'cgbe'}.get(suffix)
                else:
                    model = None
                if model:
                    jobs.append(('mooneye-cgb', rom, model, 'fibonacci_breakpoint'))
    chosen = set(stems)
    if chosen:
        jobs = [job for job in jobs if job[1].stem in chosen]
        missing = chosen - {job[1].stem for job in jobs}
        if missing:
            raise ValueError('No documented jobs for selected ROM(s): ' + ', '.join(sorted(missing)))
    if not jobs:
        raise ValueError('No documented test ROMs found for selected suite')
    return jobs


def run_job(job, args):
    family, source, model, protocol = job
    budget = args.frames or (30 if protocol == 'hram_ff82' else 1200)
    row = {'suite': family, 'rom': source.relative_to(args.suite_root).as_posix(),
           'rom_sha256': sha256(source), 'model': model, 'protocol': protocol,
           'frames': budget}
    start = time.monotonic()
    try:
        with tempfile.TemporaryDirectory(prefix='studio-hardware-matrix-') as folder:
            working = Path(folder)
            rom = working / source.name
            shutil.copyfile(source, rom)
            command = [str(args.emulator), str(rom), '--model', model, '--frames', str(budget)]
            flag = working / 'result.bin'
            if protocol == 'hram_ff82':
                command += ['--dump', 'FF80', '3', str(flag)]
            else:
                command += ['--mooneye']
                if args.boot_rom:
                    command += ['--boot-rom', str(args.boot_rom.resolve())]
            completed = subprocess.run(command, text=True, capture_output=True, timeout=args.timeout)
            output = completed.stdout + completed.stderr
            row['returncode'] = completed.returncode
            row['output'] = output[-2400:]
            if protocol == 'hram_ff82':
                data = flag.read_bytes() if flag.is_file() else b''
                row['hram_ff80_ff82'] = data.hex()
                if completed.returncode or len(data) != 3:
                    row['status'] = 'ERROR'
                elif data[2] == 1:
                    row['status'] = 'PASS'
                elif data[2] == 0xFF:
                    row['status'] = 'FAIL'
                else:
                    row['status'] = 'INCOMPLETE'
            elif completed.returncode == 0 and 'RESULT: PASS' in output.splitlines():
                row['status'] = 'PASS'
            elif 'RESULT: FAIL' in output.splitlines():
                row['status'] = 'FAIL'
            elif 'RESULT: TIMEOUT' in output.splitlines():
                row['status'] = 'INCOMPLETE'
            else:
                row['status'] = 'ERROR'
    except subprocess.TimeoutExpired:
        row.update(status='TIMEOUT', output=f'Host timeout after {args.timeout} seconds')
    except OSError as error:
        row.update(status='ERROR', output=str(error))
    row['seconds'] = round(time.monotonic() - start, 4)
    return row


def main(argv=None):
    args = parse_args(argv)
    try:
        jobs = discover_jobs(args.suite_root, args.only, args.rom)
    except ValueError as error:
        raise SystemExit(str(error))
    outputs = {args.report, args.report.with_suffix(args.report.suffix + '.tmp')}
    if any(job[1].resolve() in outputs for job in jobs):
        raise SystemExit('Report output must not replace a test ROM')
    initial_hash = sha256(args.emulator)
    results = []
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = [pool.submit(run_job, job, args) for job in jobs]
        for future in as_completed(futures):
            row = future.result()
            results.append(row)
            print(f"{len(results):4}/{len(jobs)} {row['status']:10} {row['model']:5} {row['rom']}", flush=True)
    assert sha256(args.emulator) == initial_hash, 'Emulator changed while tests were running'
    results.sort(key=lambda row: (row['suite'], row['rom'], row['model']))
    summary = {family: {status: sum(row['suite'] == family and row['status'] == status for row in results)
                        for status in ('PASS', 'FAIL', 'INCOMPLETE', 'TIMEOUT', 'ERROR')}
               for family in sorted({row['suite'] for row in results})}
    report = {'schema_version': 1, 'validation_utc': datetime.now(timezone.utc).isoformat(),
              'emulator_sha256': initial_hash,
              'core': 'Studio native core',
              'source_release': 'https://github.com/c-sp/game-boy-test-roms/releases/tag/v7.0',
              'archive_sha256': 'b9a9d7a1075aa35a3d07c07c34974048672d8520dca9e07a50178f5860c3832c',
              'protocol_documents': [
                  'https://github.com/c-sp/game-boy-test-roms/blob/v7.0/src/howto/gbmicrotest.md',
                  'https://github.com/Gekkio/mooneye-test-suite#test-naming',
                  'https://github.com/Gekkio/mooneye-test-suite#passfail-reporting'],
              'boot_condition': 'external_cgb_image_for_mooneye' if args.boot_rom else 'studio_native_post_boot_initialization',
              'external_boot_sha256': sha256(args.boot_rom) if args.boot_rom else None,
              'commercial_games_tested': [],
              'limitations': 'GBMicrotest targets DMG B/C only. Its directory also contains nonasserting manual probes/testbenches; missing terminal flags remain INCOMPLETE and do not prove a core failure. Mooneye common cases run on CGB E; model-specific misc suffixes select CGB0/E or AGB. Boot-state failures remain visible and depend on the selected initialization/firmware. No image-only or manual-only ROM is inferred to pass.',
              'summary': summary, 'results': results}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.report.with_suffix(args.report.suffix + '.tmp')
    temporary.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    temporary.replace(args.report)
    print(json.dumps(summary, ensure_ascii=False))
    return 0 if all(row['status'] == 'PASS' for row in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
