#!/usr/bin/env python3
"""Download and safely extract the pinned c-sp hardware-test ROM release.

The release contains public emulator test ROMs, not commercial games. A cached
archive must pass the same pinned SHA256 check as a new download before any
member is extracted. The --output directory is the suite root expected by
run_extended_tests.py; --cache optionally supplies an existing archive.
"""
import argparse
import hashlib
from pathlib import Path, PurePosixPath, PureWindowsPath
import shutil
import stat
import tempfile
import urllib.request
import zipfile

RELEASE = 'v7.0'
ARCHIVE_NAME = 'game-boy-test-roms-v7.0.zip'
URL = 'https://github.com/c-sp/game-boy-test-roms/releases/download/v7.0/' + ARCHIVE_NAME
SHA256 = 'b9a9d7a1075aa35a3d07c07c34974048672d8520dca9e07a50178f5860c3832c'


def verify_archive(archive, expected_sha256=SHA256):
    digest = hashlib.sha256()
    with archive.open('rb') as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b''):
            digest.update(chunk)
    actual = digest.hexdigest()
    if actual != expected_sha256:
        raise ValueError('Archive SHA256 mismatch: ' + str(archive))
    return actual


def download_archive(archive):
    if archive.is_file():
        verify_archive(archive)
        return
    archive.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(prefix=ARCHIVE_NAME + '.', suffix='.part',
                                         dir=archive.parent, delete=False) as target:
            temporary = Path(target.name)
            request = urllib.request.Request(URL, headers={'User-Agent': 'GameBoy-Studio-ROM-tests'})
            with urllib.request.urlopen(request, timeout=60) as response:
                shutil.copyfileobj(response, target)
        verify_archive(temporary)
        temporary.replace(archive)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def extract_archive(archive, output):
    root = output.resolve()
    with zipfile.ZipFile(archive) as bundle:
        members = []
        # Validate the whole list first, so malformed archives cannot leave a
        # partly extracted suite or write through an existing directory link.
        for entry in bundle.infolist():
            name = entry.filename.replace('\\', '/')
            relative = PurePosixPath(name)
            windows = PureWindowsPath(name)
            mode = entry.external_attr >> 16
            if (not relative.parts or relative.is_absolute() or windows.drive
                    or '..' in relative.parts or ':' in name or stat.S_ISLNK(mode)):
                raise ValueError('Unsafe ZIP member: ' + entry.filename)
            target = root.joinpath(*relative.parts)
            if not target.resolve().is_relative_to(root):
                raise ValueError('ZIP member escapes output: ' + entry.filename)
            for ancestor in (target, *target.parents):
                if ancestor == root:
                    break
                if ancestor.is_symlink():
                    raise ValueError('ZIP member crosses a symbolic link: ' + entry.filename)
            members.append((entry, target))
        root.mkdir(parents=True, exist_ok=True)
        for entry, target in members:
            if entry.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with bundle.open(entry) as source, target.open('wb') as destination:
                    shutil.copyfileobj(source, destination)
    return len(members)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path, help='Extracted test-suite directory')
    parser.add_argument('--cache', type=Path, help='Cached ZIP; default: archive beside --output')
    args = parser.parse_args(argv)
    archive = (args.cache or args.output.parent / ARCHIVE_NAME).resolve()
    try:
        download_archive(archive)
        count = extract_archive(archive, args.output)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        raise SystemExit('Could not prepare extended suite: ' + str(error)) from error
    print(f'{RELEASE}: verified SHA256 {SHA256}; extracted {count} entries to {args.output.resolve()}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
