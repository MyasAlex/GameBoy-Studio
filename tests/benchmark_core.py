#!/usr/bin/env python3
"""Repeatable core benchmark; run this on the actual weak PC as well.
It does not measure Direct3D, audio-device latency, gamepad input or real games.
"""
import argparse
import json
import platform
from pathlib import Path
import statistics
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--emulator', required=True, type=Path)
p.add_argument('--rom', required=True, type=Path)
p.add_argument('--frames', type=int, default=600)
p.add_argument('--report', type=Path, default=Path('benchmark.json'))
args = p.parse_args()
seconds = []
for _ in range(5):
    start = time.perf_counter()
    subprocess.run([str(args.emulator.resolve()), str(args.rom.resolve()), '--frames', str(args.frames)],
                   check=True, capture_output=True, timeout=120)
    seconds.append(time.perf_counter()-start)
median = statistics.median(seconds)
report = {'platform': platform.platform(), 'machine': platform.machine(),
          'rom': args.rom.name, 'frames_per_run': args.frames, 'seconds': seconds,
          'median_frames_per_second': args.frames/median,
          'median_speed_multiple': args.frames/median/(4194304/70224),
          'scope': 'Headless core only; no claims about weak PCs or host-device latency.'}
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))

