#!/usr/bin/env python3
"""Package actual corpus evidence, retaining original frames/PCM and their SHA-256 hashes."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile
from PIL import Image, ImageDraw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runs', type=Path)
    parser.add_argument('docs', type=Path)
    args = parser.parse_args()
    args.docs.mkdir(parents=True, exist_ok=True)
    report = json.loads((args.runs / 'results.json').read_text())
    manifest = json.loads(Path(__file__).with_name('game_corpus').joinpath('manifest.json').read_text())
    titles = {game['id']: game['title'] for game in manifest['games']}
    rows = report['results']
    sheet = Image.new('RGB', (4 * 240, ((len(rows) + 3) // 4) * 260), (24, 28, 34))
    draw = ImageDraw.Draw(sheet)
    hashes = {}
    # Choose legible gameplay samples; every original capture remains in the ZIP.
    representative = {'libbet-dmg': 720, 'libbet-cgb': 1200, 'snake-dmg': 1200, 'snake-cgb': 1200,
                      'sushi-nights-dmg': 1800, 'tobutobugirldeluxe-dmg': 1800,
                      'ucity-cgb': 720, 'shock-lobster-dmg': 1200}
    with zipfile.ZipFile(args.docs / 'stage4-game-evidence.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.write(args.runs / 'results.json', 'results.json')
        for index, row in enumerate(rows):
            folder = args.runs / f"{row['id']}-{row['model']}"
            x, y = index % 4 * 240, index // 4 * 260
            label = f"{titles[row['id']]} / {row['model'].upper()}"
            draw.text((x + 8, y + 7), label[:35], fill=(235, 239, 245))
            frame_number = representative.get(folder.name, row.get('frames', report['frames_per_run']))
            final = folder / f'frame-{frame_number}.ppm'
            if not final.exists():
                frame_number = row.get('frames', report['frames_per_run'])
                final = folder / f'frame-{frame_number}.ppm'
            if final.exists():
                frame = Image.open(final).resize((240, 216), Image.Resampling.NEAREST)
                sheet.paste(frame, (x, y + 27))
            draw.text((x + 8, y + 245), f"capture {frame_number} | state exact {row.get('save_state_continuation_exact')}", fill=(164, 185, 209))
            for path in sorted(folder.iterdir()):
                if path.name in {'input.txt', 'command.json', 'run.json', 'cartridge.json', 'stdout.txt', 'gameplay-audio.wav'} or path.suffix == '.ppm':
                    relative = f'{folder.name}/{path.name}'
                    hashes[relative] = hashlib.sha256(path.read_bytes()).hexdigest()
                    archive.write(path, relative)
        archive.writestr('sha256.json', json.dumps(hashes, indent=2) + '\n')
    sheet.save(args.docs / 'stage4-game-contact-sheet.png')
    shutil.copyfile(args.runs / 'results.json', args.docs / 'stage4-game-results.json')
    print(f"Packaged {len(rows)} runs, {len(hashes)} evidence files")


if __name__ == '__main__':
    main()
