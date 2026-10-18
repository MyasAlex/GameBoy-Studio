#!/usr/bin/env python3
"""Fetch only the lawful, pinned homebrew corpus; never search for commercial ROMs."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import urllib.request

MANIFEST = Path(__file__).with_name('game_corpus') / 'manifest.json'


def fetch(game, output):
    path = output / game['filename']
    if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest() != game['sha256']:
        with urllib.request.urlopen(game['url'], timeout=60) as response:
            data = response.read(8 * 1024 * 1024 + 1)
        if len(data) != game['bytes'] or hashlib.sha256(data).hexdigest() != game['sha256']:
            raise ValueError(f"Unexpected content for {game['id']}; pinned SHA-256 mismatch")
        temporary = path.with_suffix(path.suffix + '.tmp')
        temporary.write_bytes(data)
        temporary.replace(path)
    return f"{game['id']}: {game['license']} SHA-256 verified"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path, help='External corpus/cache directory (ROMs are not bundled)')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    games = json.loads(MANIFEST.read_text())['games']
    with ThreadPoolExecutor(max_workers=6) as pool:
        for result in pool.map(lambda game: fetch(game, args.output), games):
            print(result)


if __name__ == '__main__':
    main()
