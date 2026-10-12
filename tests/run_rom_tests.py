#!/usr/bin/env python3
"""Reproducible hardware ROM tests. Downloads test ROMs, never commercial games."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
BLARGG_COMMIT = 'c240dd7d700e5c0b00a7bbba52b53e4ee67b5f15'
ACID_COMMIT = '8a98ce731f96dde032ffb22ec36dc985d78fdb18'
MOONEYE_VERSION = 'mts-20260714-0944-31510e1'
MOONEYE_SHA256 = '5f0694e4e9e58441db23ff057a5ab681870d4cfaed91267e99885936db8e6f1d'
CPU = ['01-special','02-interrupts','03-op sp,hl','04-op r,imm','05-op rp','06-ld r,r',
       '07-jr,jp,call,ret,rst','08-misc instrs','09-op r,r','10-bit ops','11-op a,(hl)']
SOUND = ['01-registers','02-len ctr','03-trigger','04-sweep','05-sweep details',
         '06-overflow on trigger','07-len sweep period sync','08-len ctr during power',
         '09-wave read while on','10-wave trigger while on','11-regs after power','12-wave write while on']
BLARGG = [f'cpu_instrs/individual/{n}.gb' for n in CPU]
BLARGG += ['cpu_instrs/cpu_instrs.gb','instr_timing/instr_timing.gb','halt_bug.gb']
for folder in ['mem_timing','mem_timing-2']:
    sub = 'individual' if folder == 'mem_timing' else 'rom_singles'
    BLARGG += [f'{folder}/{sub}/{n}.gb' for n in ['01-read_timing','02-write_timing','03-modify_timing']]
    BLARGG += [f'{folder}/mem_timing.gb']
BLARGG += [f'dmg_sound/rom_singles/{n}.gb' for n in SOUND] + ['dmg_sound/dmg_sound.gb']
# Keep known failures explicit. Never add failures just to make CI green.
KNOWN_FAILURES = set()


def fetch(url, destination, sha256=None):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() and (not sha256 or hashlib.sha256(destination.read_bytes()).hexdigest() == sha256):
        return
    data = None
    for attempt in range(3):
        try:
            with urllib.request.urlopen(url, timeout=40) as response:
                data = response.read()
            break
        except OSError:
            if attempt == 2:
                raise
            time.sleep(0.5)
    if sha256 and hashlib.sha256(data).hexdigest() != sha256:
        raise ValueError(f'Checksum mismatch: {destination.name}')
    destination.write_bytes(data)


def prepare(cache, mooneye):
    from urllib.parse import quote
    jobs = [(f'https://raw.githubusercontent.com/retrio/gb-test-roms/{BLARGG_COMMIT}/{quote(n)}', cache/'blargg'/n, None) for n in BLARGG]
    jobs += [('https://github.com/mattcurrie/dmg-acid2/releases/download/v1.0/dmg-acid2.gb', cache/'dmg-acid2.gb',
              '464e14b7d42e7feea0b7ede42be7071dc88913f75b9ffa444299424b63d1dff1'),
             (f'https://raw.githubusercontent.com/mattcurrie/dmg-acid2/{ACID_COMMIT}/img/reference-dmg.png', cache/'reference-dmg.png',
              'ca966d50895c7efef05838590d148c2cbfd7fba57dab986f25b35b4da71abb57')]
    with ThreadPoolExecutor(max_workers=6) as pool:
        list(pool.map(lambda job: fetch(*job), jobs))
    if mooneye:
        archive = cache/'mooneye.tar.gz'
        fetch(f'https://gekkio.fi/files/mooneye-test-suite/{MOONEYE_VERSION}/{MOONEYE_VERSION}.tar.gz', archive, MOONEYE_SHA256)
        dest = cache/'mooneye'
        if not (dest/MOONEYE_VERSION).exists():
            dest.mkdir(parents=True, exist_ok=True)
            with tarfile.open(archive, 'r:gz') as bundle:
                # Supported by Python 3.12+; explicit containment checks support older Python.
                for item in bundle.getmembers():
                    target = (dest/item.name).resolve()
                    if not target.is_relative_to(dest.resolve()) or item.issym() or item.islnk():
                        raise ValueError('Unsafe archive entry')
                if hasattr(tarfile, 'data_filter'):
                    bundle.extractall(dest, filter='data')
                else:
                    bundle.extractall(dest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator', required=True, type=Path)
    parser.add_argument('--cache', type=Path, default=ROOT/'tests/roms')
    parser.add_argument('--download', action='store_true')
    parser.add_argument('--mooneye', action='store_true')
    parser.add_argument('--allow-known-failures', action='store_true')
    parser.add_argument('--report', type=Path, default=ROOT/'build/rom-tests.json')
    args = parser.parse_args()
    executable = args.emulator.resolve()
    if not executable.is_file():
        parser.error(f'Emulator executable not found: {executable}')
    if args.download:
        prepare(args.cache, args.mooneye)
    tests = [('Blargg', n, args.cache/'blargg'/n, '--test') for n in BLARGG]
    if args.mooneye:
        suite = args.cache/'mooneye'/MOONEYE_VERSION
        paths = sorted((suite/'acceptance').rglob('*.gb')) + sorted((suite/'emulator-only').rglob('*.gb'))
        if not paths:
            parser.error('Mooneye ROMs are missing. Use --download --mooneye.')
        paths = [p for p in paths if not any(tag in p.stem for tag in ['-S','-C','-A','-dmg0','-mgb','-sgb','-cgb','-agb'])]
        tests += [('Mooneye DMG ABC', p.relative_to(suite).as_posix(), p, '--mooneye') for p in paths]

    def run(test):
        suite, name, rom, mode = test
        start = time.monotonic()
        try:
            result = subprocess.run([str(executable), str(rom.resolve()), '20000', mode], capture_output=True, text=True, timeout=90)
            output = result.stdout + result.stderr
            status = 'PASS' if result.returncode == 0 else 'FAIL'
            if 'RESULT: TIMEOUT' in output:
                status = 'TIMEOUT'
            if args.allow_known_failures and suite.startswith('Mooneye') and name in KNOWN_FAILURES and status == 'FAIL' and result.returncode == 1:
                status = 'XFAIL'
        except subprocess.TimeoutExpired:
            output = 'Host timeout after 90 seconds'; status = 'TIMEOUT'
        return {'suite': suite, 'rom': name, 'status': status, 'seconds': round(time.monotonic()-start, 3), 'output': output}

    results = []
    with ThreadPoolExecutor(max_workers=4) as pool:
        for item in pool.map(run, tests):
            results.append(item)
            print(f"{item['status']:7} {item['suite']}: {item['rom']}", flush=True)
    try:
        from PIL import Image, ImageChops
        image_path = args.cache/'acid2-result.ppm'
        result = subprocess.run([str(executable), str((args.cache/'dmg-acid2.gb').resolve()), '60', '--grey', '--output', str(image_path.resolve())], capture_output=True, text=True, timeout=30)
        if result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        actual = Image.open(image_path).convert('RGB')
        expected = Image.open(args.cache/'reference-dmg.png').convert('RGB')
        difference = ImageChops.difference(actual, expected)
        raw = difference.tobytes()
        count = sum(raw[i:i+3] != b'\0\0\0' for i in range(0,len(raw),3))
        results.append({'suite': 'PPU image', 'rom': 'dmg-acid2', 'status': 'PASS' if count == 0 else 'FAIL', 'different_pixels': count})
    except Exception as error:
        results.append({'suite': 'PPU image', 'rom': 'dmg-acid2', 'status': 'ERROR', 'output': str(error)})
    args.report.parent.mkdir(parents=True, exist_ok=True)
    report = {'blargg_commit': BLARGG_COMMIT, 'mooneye_version': MOONEYE_VERSION,
              'commercial_games_tested': [], 'results': results}
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    failures = [r for r in results if r['status'] not in {'PASS','XFAIL'}]
    print(f"PASS={sum(r['status']=='PASS' for r in results)} XFAIL={sum(r['status']=='XFAIL' for r in results)} FAIL={len(failures)}")
    for item in failures:
        print(item['rom'], item.get('output', 'Image differs'))
    return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
