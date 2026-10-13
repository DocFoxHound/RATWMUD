#!/usr/bin/env python3
"""Put a regenerated world's terrain into DEV: only the cells whose ground changed, nothing else.

  python3 tools/apply_terrain.py NEW_PROJECT.json [--base OLD_PROJECT.json] [--revision N] [--dry-run]

NEW is the generator's output (python3 -m worldgen.realm --out DIR writes DIR/project.json); BASE is what DEV was made
from (default: artifacts/realm/project.json, the generator's own copy of the world in DEV). Each cell whose terrain
differs between them gets NEW's terrain in DEV. It refuses unless every one of those cells in DEV is still exactly as
BASE has it (no one has edited them since), and, with --revision, unless DEV is at that revision; people, buildings,
doors and everything else in DEV are left as they are. The cells' terrain in DEV before is saved under
artifacts/backups/ first. Afterwards the generator's own copy (artifacts/realm) is the NEW world's, and DEV needs a
build: python3 tools/world_build.py dev.

Used for the farm tracks (Docs/Design/60-the-new-map.md, "Roads and weather"), when only ground changed.
"""
from __future__ import annotations

import argparse
import json
import shutil
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))


def changed_cells(base: dict, new: dict) -> dict[str, list[str]]:
    """The cells whose terrain differs: id -> NEW's terrain. Refuses if anything but terrain differs."""
    old = {c['id']: c for c in base['cells']}
    out = {}
    for c in new['cells']:
        before = old.get(c['id'])
        if before is None:
            sys.exit(f'{c["id"]} is new: this tool only changes the ground of cells DEV already has.')
        if any(before.get(k) != c.get(k) for k in c if k != 'terrain'):
            sys.exit(f'{c["id"]} differs in more than its terrain: not a terrain-only change.')
        if before['terrain'] != c['terrain']:
            out[c['id']] = c['terrain']
    for key in new:
        if key != 'cells' and base.get(key) != new[key]:
            sys.exit(f'The worlds differ in their {key}, not only in terrain: not a terrain-only change.')
    return out


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('new', type=Path, help='The regenerated project.json')
    parser.add_argument('--base', type=Path, default=ROOT / 'artifacts/realm/project.json', help='What DEV was made from')
    parser.add_argument('--revision', type=int, help='Refuse unless DEV is at this revision')
    parser.add_argument('--dry-run', action='store_true', help='Check and say what would change; write nothing')
    args = parser.parse_args(argv)
    import world_db
    import world_store

    base = json.loads(args.base.read_text(encoding='utf-8'))
    new = json.loads(args.new.read_text(encoding='utf-8'))
    changes = changed_cells(base, new)
    if not changes:
        print('No cell\'s terrain differs: nothing to do.')
        return 0
    tiles = sum(a != b for cid, rows in changes.items() for r1, r2 in zip(rows, next(c for c in base['cells'] if c['id'] == cid)['terrain'])
                for a, b in zip(r1, r2))
    with world_db.connect('dev', 'editor') as conn:
        world_id = world_store.list_worlds(conn)[0]['id']
        dev, revision = world_store.load_world(conn, world_id)
        if args.revision is not None and revision != args.revision:
            sys.exit(f'DEV is at revision {revision}, not {args.revision}: not touching it.')
        old = {c['id']: c for c in base['cells']}
        for c in dev['cells']:
            if c['id'] in changes and (c['terrain'] != old[c['id']]['terrain'] or c.get('heights') != old[c['id']].get('heights')):
                sys.exit(f'{c["id"]} was edited in DEV since it was made: not touching it.')
        missing = set(changes) - {c['id'] for c in dev['cells']}
        if missing:
            sys.exit(f'DEV has no cell {sorted(missing)[0]}: not touching it.')
        print(f'DEV {world_id} revision {revision}: {len(changes)} cells, {tiles} tiles to change.')
        if args.dry_run:
            return 0
        folder = ROOT / 'artifacts/backups'
        folder.mkdir(parents=True, exist_ok=True)
        backup = folder / f'{world_id}_dev_r{revision}_terrain_before_{time.strftime("%Y%m%d-%H%M%S")}.json'
        backup.write_text(json.dumps({c['id']: c['terrain'] for c in dev['cells'] if c['id'] in changes}), encoding='utf-8')
        for c in dev['cells']:
            if c['id'] in changes:
                c['terrain'] = changes[c['id']]
        saved = world_store.save_world(conn, dev, revision)
    print(f'DEV revision {saved}; the cells\' old terrain saved to {backup.relative_to(ROOT)}.')
    if args.base.resolve() == (ROOT / 'artifacts/realm/project.json').resolve():
        for name in ('project.json', 'roads.json', 'buildings.json'):
            if (args.new.parent / name).exists():
                shutil.copyfile(args.new.parent / name, ROOT / 'artifacts/realm' / name)
        print('artifacts/realm is the new world now.')
    print('Next: python3 tools/world_build.py dev')
    return 0


if __name__ == '__main__':
    sys.exit(main())
