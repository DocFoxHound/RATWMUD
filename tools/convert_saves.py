#!/usr/bin/env python3
"""Converts the Unreal server's SQLite saves under Saved/ into the save files the game server reads (Docs/Design/
27-browser-client.md). One-off: run it once after moving off Unreal. Each converted save sits beside the old one, which
is left untouched; an existing new save is never overwritten.

    python3 tools/convert_saves.py              convert every save found
    python3 tools/convert_saves.py --dry-run    only list what would be converted

  Saved/ratw-world.sqlite                -> Saved/ratw-world.json   (the demo world)
  Saved/ratw-town.sqlite                 -> Saved/ratw-town.json    (Greyfen Crossing, --town)
  Saved/Atlas/<hash>/ratw-world.sqlite   -> Saved/Atlas/<hash>.json (an exported Atlas world, --world)

The database worlds (DEV, PROD) are not files and need nothing.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sqlite3

ROOT = Path(__file__).resolve().parent.parent


def targets(saved: Path) -> list[tuple[Path, Path]]:
    found = []
    for name in ('ratw-world', 'ratw-town'):
        old = saved / f'{name}.sqlite'
        if old.exists():
            found.append((old, saved / f'{name}.json'))
    atlas = saved / 'Atlas'
    if atlas.is_dir():
        for folder in sorted(atlas.iterdir()):
            old = folder / 'ratw-world.sqlite'
            if folder.is_dir() and old.exists():
                found.append((old, atlas / f'{folder.name}.json'))
    return found


def read_payload(path: Path) -> str | None:
    """The save's JSON, or None if the file holds no save (a fresh or damaged database)."""
    with sqlite3.connect(f'file:{path}?mode=ro', uri=True) as database:
        row = database.execute('SELECT schema_version, payload FROM world_state WHERE id=1').fetchone()
    if not row or row[0] != 1 or not row[1]:
        return None
    json.loads(row[1])                       # It must be JSON to be worth keeping.
    return row[1]


def write_private(path: Path, text: str):
    """Owner-only, as the game writes it (account verifiers are inside)."""
    temp = path.with_name(path.name + '.tmp')
    descriptor = os.open(temp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(descriptor, 'w', encoding='utf-8') as out:
        out.write(text)
    os.replace(temp, path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--saved', type=Path, default=ROOT / 'Saved', help=argparse.SUPPRESS)
    args = parser.parse_args()
    found = targets(args.saved)
    if not found:
        print('No Unreal saves to convert.')
        return 0
    failures = 0
    for old, new in found:
        if new.exists():
            print(f'kept    {new.relative_to(args.saved)} (already there; {old.relative_to(args.saved)} not converted)')
            continue
        try:
            payload = read_payload(old)
        except (sqlite3.Error, ValueError) as error:
            print(f'FAILED  {old.relative_to(args.saved)}: {error}')
            failures += 1
            continue
        if payload is None:
            print(f'skipped {old.relative_to(args.saved)}: no save in it')
            continue
        if args.dry_run:
            print(f'would   {old.relative_to(args.saved)} -> {new.relative_to(args.saved)}')
            continue
        write_private(new, payload)
        print(f'made    {new.relative_to(args.saved)} from {old.relative_to(args.saved)}')
    return 1 if failures else 0


if __name__ == '__main__':
    raise SystemExit(main())
