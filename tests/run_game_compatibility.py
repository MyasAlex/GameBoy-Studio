#!/usr/bin/env python3
"""Reproducible gameplay smoke runs; a successful run is not a full-game compatibility rating."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

MANIFEST = Path(__file__).with_name('game_corpus') / 'manifest.json'


def events(game):
    # Each bit matches the Studio bridge: R,L,U,D,A,B,Select,Start.
    result = {0: 0, 200: 128, 212: 0, 240: 16, 252: 0, 280: 16, 292: 0,
              320: 128, 332: 0, 350: 16, 362: 0}
    # Repeated presses let puzzle games consume individual steps; action games
    # get held movement and jumps. No writes to game RAM or cartridge patches.
    puzzle = game['id'] in {'2048gb','adjustris','geometrix','libbet','petris','postbot',
                             'snake','ucity','gb-wordyl','big2small','gb-corp','sushi-nights','porklike-gb'}
    for frame in range(390, 2400, 30 if puzzle else 60):
        index = (frame - 390) // (30 if puzzle else 60)
        key = [1, 8, 2, 4][index % 4] if puzzle else [1 | 16, 1, 2 | 16, 2][index % 4]
        result[frame] = key
        result[frame + (8 if puzzle else 40)] = 0
        if puzzle and index % 4 == 0:
            result[frame + 12] = 16
            result[frame + 20] = 0
    if game['id'] == 'ucity':
        # Main menu defaults to LOAD; choose Random Map and name the new city.
        result = {f: b for f, b in result.items() if f >= 900}
        result.update({0: 0, 200: 128, 212: 0, 320: 8, 332: 0, 360: 16, 372: 0,
                       450: 16, 462: 0, 520: 128, 532: 0})
    if game['id'] == 'porklike-gb':
        # A opens inventory; close it with B and move without reopening it.
        result = {f: b for f, b in result.items() if f < 390}
        result.update({370: 32, 382: 0})
        for frame in range(420, 2400, 30):
            result[frame] = [1, 8, 2, 4][((frame - 420)//30)//8 % 4]
            result[frame + 8] = 0
    if game['id'] == 'petris':
        result = {f: b for f, b in result.items() if not 1490 <= f <= 1720}
        result.update({650: 128, 662: 0})
    if game['id'] in {'adjustris', 'snake'}:
        result.pop(320, None)
        result.pop(332, None)
    if game['id'] in {'tobutobugirl', 'tobutobugirldeluxe'}:
        result = {f: b for f, b in result.items() if f >= 1020 or f == 0}
        result = {f: b for f, b in result.items() if not 1490 <= f <= 1720}
        result.update({650: 128, 662: 0, 800: 128, 812: 0, 900: 16, 912: 0, 960: 16, 972: 0,
                       1500: 128, 1512: 0, 1620: 16, 1632: 0, 1700: 16, 1712: 0})
    return sorted(result.items())


def run_job(job, executable, corpus, output, frames):
    game, model = job
    source = corpus / game['filename']
    if hashlib.sha256(source.read_bytes()).hexdigest() != game['sha256']:
        raise ValueError(f"Hash mismatch: {source}")
    destination = output / f"{game['id']}-{model}"
    destination.mkdir(parents=True, exist_ok=True)
    for pattern in ('frame-*.ppm', 'run.json', 'cartridge.json', 'gameplay-audio.wav', 'stdout.txt'):
        for stale in destination.glob(pattern):
            stale.unlink()
    rom = destination / game['filename']
    shutil.copyfile(source, rom)
    # Isolate every run from prior batteries; games may mutate their own saves.
    for suffix in ('.sav', '.sbbattery'):
        for path in (rom.with_suffix(suffix), Path(str(rom) + suffix)):
            path.unlink(missing_ok=True)
    script = destination / 'input.txt'
    script.write_text(''.join(f'{f} {b}\n' for f, b in events(game)))
    command = [str(executable), str(rom), str(destination), model, str(script), str(frames)]
    (destination / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
    try:
        result = subprocess.run(command, text=True, capture_output=True, timeout=180)
        output_log = result.stdout + result.stderr
    except subprocess.TimeoutExpired as error:
        result = subprocess.CompletedProcess(command, 124)
        output_log = f"Run timed out after {error.timeout} seconds\n"
    except OSError as error:
        result = subprocess.CompletedProcess(command, 126)
        output_log = f"Could not execute runner: {error}\n"
    (destination / 'stdout.txt').write_text(output_log)
    data = json.loads((destination / 'run.json').read_text()) if (destination / 'run.json').exists() else {}
    if data and (data.get('schema') != 'gameboy-studio-game-smoke-v1' or data.get('model') != model
                 or data.get('frames') != frames or data.get('rom') != game['filename']):
        raise ValueError(f"Run report identity mismatch: {destination}")
    data.update(id=game['id'], model=model, requested_frames=frames, title=game['title'], sha256=game['sha256'],
                cartridge_type=game['cartridge_type'], license=game['license'], command=command,
                exit_status_scope='state/animation smoke only; inspect all independent metrics',
                exit_code=result.returncode, input_sha256=hashlib.sha256(script.read_bytes()).hexdigest())
    for path in destination.glob('*.ppm'):
        data.setdefault('screenshots', {})[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    wave = destination / 'gameplay-audio.wav'
    if wave.exists():
        data['audio_wav_sha256'] = hashlib.sha256(wave.read_bytes()).hexdigest()
    rom.unlink(missing_ok=True)
    for path in (rom.with_suffix('.sav'), Path(str(rom) + '.sbbattery')):
        path.unlink(missing_ok=True)
    print(f"{game['id']}/{model}: exit={result.returncode}, frames={data.get('frames')}, exact={data.get('save_state_continuation_exact')}", flush=True)
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--corpus', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--frames', type=int, default=2400)
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--game', action='append', default=[])
    args = parser.parse_args()
    executable_hash = hashlib.sha256(args.executable.read_bytes()).hexdigest()
    manifest = json.loads(MANIFEST.read_text())
    games = [g for g in manifest['games'] if not args.game or g['id'] in args.game]
    args.output.mkdir(parents=True, exist_ok=True)
    jobs = [(game, model) for game in games for model in
            (['cgb'] if game['cgb_flag'] == 0xc0 else ['dmg', 'cgb'] if game['cgb_flag'] & 0x80 else ['dmg'])]
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(lambda job: run_job(job, args.executable.resolve(), args.corpus.resolve(), args.output.resolve(), args.frames), jobs))
    if hashlib.sha256(args.executable.read_bytes()).hexdigest() != executable_hash:
        raise RuntimeError('Runner binary changed during the corpus run; evidence cannot identify one build')
    report = dict(schema='gameboy-studio-game-corpus-v1', generated_utc=datetime.now(timezone.utc).isoformat(),
                  corpus_revision=manifest['revision'], runner_sha256=executable_hash,
                  runner_source_sha256=hashlib.sha256(Path(__file__).with_name('game_compatibility_runner.cpp').read_bytes()).hexdigest(),
                  input_generator_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  games=len(games), runs=len(results), frames_per_run=args.frames, results=results,
                  scope='Automated boot/input/frame/audio/save smoke checks on published homebrew games. No full-game completion, commercial-ROM, physical-device or Windows GUI compatibility claim.')
    (args.output / 'results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    return int(any(result['exit_code'] for result in results))


if __name__ == '__main__':
    raise SystemExit(main())
