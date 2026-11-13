#!/usr/bin/env python3
"""The host's world folder, as the game's team sends it (Package/README.md; Docs/Design/78, section 6.3):

    python3 Package/host_folder.py --out DIR [--zip FILE.zip]
    python3 Package/host_folder.py --sync-compose     (Package/ratw-backup.pl into compose.yaml's backup-run config)

DIR (missing or empty) gets compose.yaml, world.toml, README.md and Hosting.md (Docs/Hosting.md), the scripts a host
double-clicks (restore.bat and restore.sh, reset-editor-password.bat and .sh: Docs/Design/78, 3.6 and 12.2), and two
empty folders, manage/ (where the world writes first-editor.txt and editor-certificate.txt) and backups/. Shipping them
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
         'Hosting.md': ROOT / 'Docs' / 'Hosting.md', 'restore.bat': HERE / 'restore.bat', 'restore.sh': HERE / 'restore.sh',
         'reset-editor-password.bat': HERE / 'reset-editor-password.bat',
         'reset-editor-password.sh': HERE / 'reset-editor-password.sh'}
RUNNABLE = ('restore.sh', 'reset-editor-password.sh')
FOLDERS = ('manage', 'backups')
BACKUP = HERE / 'ratw-backup.pl'
BLOCK = '  backup-run:\n'                     # compose.yaml's last config: the backup service's program, inline.


def backup_block(program: str) -> str:
    """compose.yaml's backup-run config for `program`: every $ doubled (compose would read $NAME as its own setting)."""
    lines = ''.join(f'      {line}' if line.strip() else '\n' for line in program.replace('$', '$$').splitlines(True))
    return (BLOCK + '    # Generated from Package/ratw-backup.pl (python3 Package/host_folder.py --sync-compose): edit that '
            'file.\n    content: |\n' + lines)


def inline_program(compose: str) -> str:
    """The backup service's program as compose.yaml carries it, as compose hands it over ($$ read as $)."""
    block = compose[compose.index(BLOCK):]
    body = block.split('    content: |\n', 1)[1]
    return ''.join(line[6:] if line.strip() else '\n' for line in body.splitlines(True)).replace('$$', '$')


def sync_compose() -> bool:
    """Package/ratw-backup.pl into compose.yaml (the block it ends with). True if it changed."""
    path = HERE / 'compose.yaml'
    compose = path.read_text(encoding='utf-8')
    fresh = compose[:compose.index(BLOCK)] + backup_block(BACKUP.read_text(encoding='utf-8'))
    if fresh == compose:
        return False
    path.write_text(fresh, encoding='utf-8')
    return True


def make(out: Path) -> Path:
    if out.exists() and (not out.is_dir() or any(out.iterdir())):
        raise SystemExit(f'host_folder.py: {out} exists and isn\'t an empty folder: not writing into it')
    out.mkdir(parents=True, exist_ok=True)
    for name, source in FILES.items():
        shutil.copyfile(source, out / name)
        if name.endswith('.bat'):       # (cmd.exe wants CRLF, whatever a checkout gave: its labels go astray without.)
            text = (out / name).read_bytes().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
            (out / name).write_bytes(text)
        if name in RUNNABLE:
            (out / name).chmod(0o755)
    for name in FOLDERS:
        (out / name).mkdir(mode=0o755)
    return out


def zip_folder(folder: Path, target: Path) -> Path:
    top = folder.name
    with zipfile.ZipFile(target, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name in FILES:
            info = zipfile.ZipInfo.from_file(folder / name, f'{top}/{name}')
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = ((0o100755 if name in RUNNABLE else 0o100644) << 16)   # (unzip keeps .sh runnable.)
            archive.writestr(info, (folder / name).read_bytes())
        for name in FOLDERS:
            archive.writestr(zipfile.ZipInfo(f'{top}/{name}/'), b'')     # (A folder entry: unzipped, it is made empty.)
    return target


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--out', type=Path, help='the folder to make (missing or empty)')
    parser.add_argument('--zip', type=Path, help='also write it as this zip file')
    parser.add_argument('--sync-compose', action='store_true', help="Package/ratw-backup.pl into compose.yaml")
    args = parser.parse_args(argv)
    if args.sync_compose:
        print('host_folder.py: compose.yaml ' + ('updated' if sync_compose() else 'already up to date'), file=sys.stderr)
        return 0
    if not args.out:
        parser.error('--out DIR is needed')
    folder = make(args.out.resolve())
    if args.zip:
        zip_folder(folder, args.zip)
    print(f'host_folder.py: {folder}' + (f' and {args.zip}' if args.zip else ''), file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
