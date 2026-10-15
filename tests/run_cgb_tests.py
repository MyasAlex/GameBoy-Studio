#!/usr/bin/env python3
"""Reproducible native CGB sound, acid2, and original demo image checks.

Public hardware-test ROMs are downloaded from their authors' pinned releases
or commits; no commercial game ROM or Nintendo boot ROM is downloaded.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess
import time
from urllib.parse import quote

from run_rom_tests import BLARGG_COMMIT, fetch

ROOT = Path(__file__).resolve().parent.parent
CGB_ACID_COMMIT = 'fa5b7f86d6fb599f79e55169494d981a7af75a31'
ACID_ROM_SHA256 = '197fb0bcec544f0400527fc707e0a94f55435974986e6986b424ace5de81720e'
ACID_IMAGE_SHA256 = '9ea9c262c5383353e77d715d021a0f7c5ccbe438f88082cb225756e50c4fdf01'
SOUND = ['01-registers', '02-len ctr', '03-trigger', '04-sweep', '05-sweep details',
         '06-overflow on trigger', '07-len sweep period sync', '08-len ctr during power',
         '09-wave read while on', '10-wave trigger while on', '11-regs after power', '12-wave']
SOUND_ROMS = [f'cgb_sound/rom_singles/{name}.gb' for name in SOUND] + ['cgb_sound/cgb_sound.gb']
SOUND_SHA256 = dict(zip(SOUND_ROMS, [
    '8415d6c69f7954e9365a586bac235f5ce39270ece32c09cb8ebbbff043b9852c',
    'a1ccf47375a2bede3077ac85a4903cb037dd8a4c7c85fc5d0c176af52915271f',
    '70173be396d14ac955c47aededcee90ba994206cc403273d0bb49ba163ae23ef',
    'abfd1f6fa9e6701e46135ac9f5bb5b10a46cde22d0fc34922636aa09a5647483',
    'f51a80c5538fe4b3afbc5c9af0e689a67869b1e2bbb73d4c60a8874826d056cb',
    '5701414d13ba55d25b59d13076e2e9cb59a9da6890595b41464f957605e317c0',
    '47d57ba87c3a93050ee340cf49b9d276e62e9573542b92b1de5fa2f17e3716c1',
    'be7948c70946e0b81c178b8778ef1907231e734650062f8fa8cdb1ceb5ac8c65',
    '9b101c68b2de1930df74c19aaf22cce74d37dd5aade93855c805ecb2e1e83fd3',
    'cbea1771a5384f4d8f8a249056224a750ad5ee4671297836e4ba87e689af2a23',
    'd347dd195a78c2d0f665b94da493a857218f032b014654b84ddb01f7b8e9befc',
    '7bbd2e78e159de4297f567fec7851f9b90c3076d18d1043af5f30e14bd66cd0a',
    'de2101eb718a2bf5f778df76e73a63f4598d9b471559f9a85adc26efa692053b',
]))


def prepare(cache):
    jobs = [(f'https://raw.githubusercontent.com/retrio/gb-test-roms/{BLARGG_COMMIT}/{quote(name)}',
             cache / 'blargg' / name, SOUND_SHA256[name]) for name in SOUND_ROMS]
    jobs += [
        ('https://github.com/mattcurrie/cgb-acid2/releases/download/v1.1/cgb-acid2.gbc',
         cache / 'cgb-acid2.gbc', ACID_ROM_SHA256),
        (f'https://raw.githubusercontent.com/mattcurrie/cgb-acid2/{CGB_ACID_COMMIT}/img/reference.png',
         cache / 'reference-cgb.png', ACID_IMAGE_SHA256),
    ]
    def download(job):
        try:
            fetch(*job)
        except (OSError, ValueError) as error:
            raise RuntimeError(f'{job[1].name}: {error}') from error
    with ThreadPoolExecutor(max_workers=6) as pool:
        list(pool.map(download, jobs))


def image_check(executable, rom, expected, screenshot, name):
    try:
        from PIL import Image, ImageChops
        result = subprocess.run([str(executable), str(rom.resolve()), '60', '--model', 'cgb',
                                 '--output', str(screenshot.resolve())],
                                capture_output=True, text=True, timeout=30)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        actual_image = Image.open(screenshot).convert('RGB')
        expected_image = Image.open(expected).convert('RGB')
        if actual_image.size != expected_image.size or actual_image.size != (160, 144):
            raise RuntimeError('Image dimensions are not 160 x 144')
        raw = ImageChops.difference(actual_image, expected_image).tobytes()
        different = sum(raw[i:i + 3] != b'\0\0\0' for i in range(0, len(raw), 3))
        return {'suite': 'CGB PPU image', 'rom': name,
                'status': 'PASS' if different == 0 else 'FAIL', 'different_pixels': different,
                'rom_sha256': hashlib.sha256(rom.read_bytes()).hexdigest()}
    except Exception as error:
        return {'suite': 'CGB PPU image', 'rom': name, 'status': 'ERROR', 'output': str(error)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator', type=Path, required=True)
    parser.add_argument('--cache', type=Path, default=ROOT / 'tests/roms/cgb')
    parser.add_argument('--download', action='store_true')
    parser.add_argument('--report', type=Path, default=ROOT / 'build/cgb-rom-tests.json')
    args = parser.parse_args()
    executable = args.emulator.resolve()
    if not executable.is_file():
        parser.error(f'Emulator executable not found: {executable}')
    if args.download:
        try:
            prepare(args.cache)
        except RuntimeError as error:
            parser.error(f'Could not download CGB tests: {error}')
    required = [(args.cache / 'blargg' / name, SOUND_SHA256[name]) for name in SOUND_ROMS]
    required += [(args.cache / 'cgb-acid2.gbc', ACID_ROM_SHA256),
                 (args.cache / 'reference-cgb.png', ACID_IMAGE_SHA256)]
    for file, digest in required:
        if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != digest:
            parser.error(f'Pinned {file.name} is missing or has wrong checksum; use --download')
    if not (ROOT / 'demo.gbc').is_file() or not (ROOT / 'tests/cgb-demo-reference.ppm').is_file():
        parser.error('Original demo/reference missing; run python tests/make_cgb_demo.py')

    def run(name):
        rom = args.cache / 'blargg' / name
        start = time.monotonic()
        try:
            result = subprocess.run([str(executable), str(rom.resolve()), '20000', '--test', '--model', 'cgb'],
                                    capture_output=True, text=True, timeout=90)
            output = result.stdout + result.stderr
            status = 'PASS' if result.returncode == 0 else 'FAIL'
            if 'RESULT: TIMEOUT' in output:
                status = 'TIMEOUT'
        except subprocess.TimeoutExpired:
            output, status = 'Host timeout after 90 seconds', 'TIMEOUT'
        return {'suite': 'Blargg CGB', 'rom': name, 'status': status,
                'seconds': round(time.monotonic() - start, 3), 'output': output,
                'rom_sha256': hashlib.sha256(rom.read_bytes()).hexdigest()}

    results = []
    with ThreadPoolExecutor(max_workers=4) as pool:
        for result in pool.map(run, SOUND_ROMS):
            results.append(result)
            print(f"{result['status']:7} {result['suite']}: {result['rom']}", flush=True)
    results.append(image_check(executable, args.cache / 'cgb-acid2.gbc', args.cache / 'reference-cgb.png',
                               args.cache / 'acid2-result.ppm', 'cgb-acid2 v1.1'))
    results.append(image_check(executable, ROOT / 'demo.gbc', ROOT / 'tests/cgb-demo-reference.ppm',
                               args.cache / 'demo-result.ppm', 'original CGB demo'))
    for result in results[-2:]:
        print(f"{result['status']:7} {result['suite']}: {result['rom']}: {result.get('different_pixels', '?')} different pixels")
    report = {'blargg_commit': BLARGG_COMMIT, 'cgb_acid_commit': CGB_ACID_COMMIT,
              'acid_rom_sha256': ACID_ROM_SHA256, 'acid_image_sha256': ACID_IMAGE_SHA256,
              'color_conversion': '(component << 3) | (component >> 2)',
              'commercial_games_tested': [], 'results': results}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    failures = [result for result in results if result['status'] != 'PASS']
    print(f"PASS={len(results) - len(failures)} FAIL={len(failures)}")
    for result in failures:
        print(result['rom'], result.get('output', 'Image differs'))
    return int(bool(failures))


if __name__ == '__main__':
    raise SystemExit(main())
