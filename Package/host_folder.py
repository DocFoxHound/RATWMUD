#!/usr/bin/env python3
"""The host's world folder, as the game's team sends it (Package/README.md; Docs/Design/78, section 6.3):

    python3 Package/host_folder.py --out DIR [--zip FILE.zip]

DIR (missing or empty) gets compose.yaml, world.toml, README.md and Hosting.md (Docs/Hosting.md), and two empty
folders, manage/ (where the world writes first-editor.txt and editor-certificate.txt) and backups/. Shipping them
empty means Docker doesn't make them itself, which on Linux would leave them root's and the files in them unreadable to
the host. --zip also writes the folder as a zip whose top folder is DIR's name, empty folders included. The world image
itself is sent beside it (`docker save`), not in it.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import sys
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
FILES = {'compose.yaml': HERE / 'compose.yaml', 'world.toml': HERE / 'world.toml', 'README.md': HERE / 'README.md',
         'Hosting.md': ROOT / 'Docs' / 'Hosting.md'}
FOLDERS = ('manage', 'backups')


def make(out: Path) -> Path:
    if out.exists() and (not out.is_dir() or any(out.iterdir())):
        raise SystemExit(f'host_folder.py: {out} exists and isn\'t an empty folder: not writing into it')
    out.mkdir(parents=True, exist_ok=True)
    for name, source in FILES.items():
        shutil.copyfile(source, out / name)
    for name in FOLDERS:
        (out / name).mkdir(mode=0o755)
    return out


def zip_folder(folder: Path, target: Path) -> Path:
    top = folder.name
    with zipfile.ZipFile(target, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name in FILES:
            archive.write(folder / name, f'{top}/{name}')
        for name in FOLDERS:
            archive.writestr(zipfile.ZipInfo(f'{top}/{name}/'), b'')     # (A folder entry: unzipped, it is made empty.)
    return target


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--out', type=Path, required=True, help='the folder to make (missing or empty)')
    parser.add_argument('--zip', type=Path, help='also write it as this zip file')
    args = parser.parse_args(argv)
    folder = make(args.out.resolve())
    if args.zip:
        zip_folder(folder, args.zip)
    print(f'host_folder.py: {folder}' + (f' and {args.zip}' if args.zip else ''), file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
