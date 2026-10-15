#!/usr/bin/env python3
"""Measure 1x speed through the actual Windows timer loop and accuracy core.

Three original ROM probes exercise DMG, CGB and KEY1/STOP CGB double speed.
Each probe runs separately so simultaneous emulators cannot distort pacing.
This tests a hidden CPU-rendered window; GPU effects, commercial ROMs and
audible quality are outside its scope. --audio requires a real audio device.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import subprocess
import time

CLOCK_HZ = 4194304
DOTS_PER_FRAME = 70224
EXPECTED_FPS = CLOCK_HZ / DOTS_PER_FRAME
PROBES = (('dmg-normal', False, False), ('cgb-normal', True, False), ('cgb-double', True, True))


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def probe_paths(output, probe):
    name, cgb, _ = probe
    return {'rom': output / (name + ('.gbc' if cgb else '.gb')),
            'capture': output / (name + '.timing.json'),
            'stdout': output / (name + '.stdout.txt'),
            'stderr': output / (name + '.stderr.txt')}


def same_file_or_path(left, right):
    # resolve handles relative paths and links; samefile also catches existing
    # hard links whose resolved names differ. Windows Path equality folds case.
    left, right = left.resolve(), right.resolve()
    return left == right or (left.exists() and right.exists() and left.samefile(right))


def build_probe(cgb, double_speed):
    """Original ROM: INC HL/JR loop; the double case executes KEY1 + STOP."""
    rom = bytearray(32768)
    rom[0x100:0x104] = bytes((0, 0xC3, 0x50, 1))
    rom[0x134:0x13F] = b'SPEEDPROBE!'
    rom[0x143] = 0x80 if cgb else 0
    rom[0x144:0x146] = b'00'
    rom[0x14A], rom[0x14B] = 1, 0x33
    code = bytes((0xF3,))  # DI
    if double_speed:
        code += bytes((0x3E, 1, 0xE0, 0x4D, 0x10, 0))  # KEY1 = 1; STOP 0
    code += bytes((0x21, 0, 0, 0x23, 0x18, 0xFD))  # HL=0; INC HL; JR loop
    rom[0x150:0x150 + len(code)] = code
    checksum = 0
    for value in rom[0x134:0x14D]:
        checksum = (checksum - value - 1) & 255
    rom[0x14D] = checksum
    checksum = sum(rom) & 0xFFFF
    rom[0x14E:0x150] = bytes((checksum >> 8, checksum & 255))
    return bytes(rom)


def validate_measurement(measurement, double_speed, audio, seconds):
    errors = []
    if measurement.get('status') != 'PASS':
        errors.append('Application timing status is not PASS.')
    if measurement.get('clock_rate') != CLOCK_HZ:
        errors.append('Frame clock is not 4194304 Hz.')
    if measurement.get('cpu_double_speed') is not double_speed:
        errors.append('CGB CPU speed does not match the probe.')
    for name, lower, upper in (('speed_ratio', 0.97, 1.03),
                               ('ppu_fps', EXPECTED_FPS - 2, EXPECTED_FPS + 2),
                               ('maximum_frame_batch', 1, 2),
                               ('wall_seconds', seconds, seconds + 5)):
        value = measurement.get(name)
        if (type(value) not in (int, float) or not math.isfinite(value)
                or value < lower or value > upper):
            errors.append(f'{name} must be within {lower:g}..{upper:g}.')
    if measurement.get('speed_setting') != 1 or measurement.get('turbo') is not False:
        errors.append('Timing run was not at normal speed with turbo released.')
    if measurement.get('sound_requested') is not audio:
        errors.append('Audio request does not match the selected test mode.')
    if audio and measurement.get('audio_opened') is not True:
        errors.append('Requested audio device did not open.')
    return errors


def run_probe(probe, args):
    name, cgb, double_speed = probe
    paths = probe_paths(args.output, probe)
    rom = paths['rom']
    rom.write_bytes(build_probe(cgb, double_speed))
    capture = paths['capture']
    # A failed process must not inherit a previous successful measurement.
    capture.unlink(missing_ok=True)
    command = [str(args.emulator), '--timing-test', str(capture.resolve()),
               '--timing-seconds', str(args.seconds)]
    if args.audio:
        command += ['--timing-audio']
    command += [str(rom.resolve())]
    row = {'probe': name, 'rom_sha256': sha256(rom), 'cgb': cgb,
           'expected_cpu_double_speed': double_speed, 'status': 'FAIL',
           'command': command, 'measurement': None, 'errors': []}
    started = time.monotonic()
    try:
        process = subprocess.run(command, capture_output=True, text=True,
                                 timeout=args.seconds + 20)
        row['returncode'] = process.returncode
        paths['stdout'].write_text(process.stdout, encoding='utf-8')
        paths['stderr'].write_text(process.stderr, encoding='utf-8')
        if process.returncode != 0:
            row['errors'].append('Application exited with status ' + str(process.returncode) + '.')
        if not capture.is_file():
            row['errors'].append('Application did not produce a fresh timing report.')
        else:
            row['measurement'] = json.loads(capture.read_text(encoding='utf-8'))
            row['errors'].extend(validate_measurement(row['measurement'], double_speed,
                                                      args.audio, args.seconds))
            row['measurement_sha256'] = sha256(capture)
        if not row['errors']:
            row['status'] = 'PASS'
    except (OSError, subprocess.TimeoutExpired, json.JSONDecodeError, TypeError, AttributeError) as error:
        row['status'] = 'ERROR'
        row['errors'].append(str(error))
    row['host_seconds'] = round(time.monotonic() - started, 6)
    return row


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator', type=Path, required=True, help='Windows GBStudio.exe')
    parser.add_argument('--output', type=Path, required=True, help='Probe ROMs, measurements and process logs')
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--seconds', type=float, default=3, help='Measured seconds per probe, 2..30 (default 3)')
    parser.add_argument('--audio', action='store_true', help='Open and pump a real audio device')
    args = parser.parse_args(argv)
    if not math.isfinite(args.seconds) or not 2 <= args.seconds <= 30:
        parser.error('--seconds must be between 2 and 30.')
    args.emulator = args.emulator.resolve()
    if not args.emulator.is_file():
        parser.error('Emulator executable is missing.')
    args.output, args.report = args.output.resolve(), args.report.resolve()
    dll = args.emulator.with_name('gb_accuracy.dll')
    protected = [('emulator executable', args.emulator), ('accuracy DLL', dll)]
    for probe in PROBES:
        protected.extend((probe[0] + ' ' + kind, path)
                         for kind, path in probe_paths(args.output, probe).items())
    # Validate all destinations before creating directories, generating ROMs,
    # deleting stale captures or starting a process.
    try:
        for name, path in protected:
            if same_file_or_path(args.report, path):
                parser.error('--report must not overwrite the ' + name + ': ' + str(path))
    except OSError as error:
        parser.error('Could not validate report paths: ' + str(error))
    args.output.mkdir(parents=True, exist_ok=True)
    identities = {'executable_sha256': sha256(args.emulator),
                  'accuracy_dll_sha256': sha256(dll) if dll.is_file() else None}
    tests = [run_probe(probe, args) for probe in PROBES]
    summary = {status: sum(row['status'] == status for row in tests)
               for status in ('PASS', 'FAIL', 'ERROR')}
    report = {'created_utc': datetime.now(timezone.utc).isoformat(), **identities,
              'audio_requested': args.audio, 'seconds_per_probe': args.seconds,
              'expected_clock_hz': CLOCK_HZ, 'expected_dots_per_frame': DOTS_PER_FRAME,
              'expected_ppu_fps': EXPECTED_FPS, 'speed_ratio_tolerance': [0.97, 1.03],
              'scope': 'Actual hidden Win32 timer loop and accuracy core. Original probes; no commercial ROMs. GPU rendering and audible quality are outside this check.',
              'summary': summary, 'tests': tests}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(summary))
    for row in tests:
        print(row['status'], row['probe'], row['errors'])
    return 0 if summary['FAIL'] == summary['ERROR'] == 0 else 1


if __name__ == '__main__':
    raise SystemExit(main())
