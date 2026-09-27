#!/usr/bin/env python3
"""Regenerate the game files for worlds bundled with the project.

Each Data/Worlds/<Name>/ folder holds one editable `*.atlas.json` (the source,
opened in Atlas Workshop) next to its generated `world.ratw` and `cells/`.
Run after editing a bundled atlas:   python3 tools/bundle_worlds.py
Verify nothing is stale (tests, CI):  python3 tools/bundle_worlds.py --check
"""
from __future__ import annotations

import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import map_editor  # noqa: E402

WORLDS = map_editor.ROOT / 'Data' / 'Worlds'


def generated(folder: Path, assign: bool = False) -> dict[str, str]:
    sources = sorted(folder.glob('*.atlas.json'))
    if len(sources) != 1:
        raise SystemExit(f'{folder}: expected exactly one *.atlas.json, found {len(sources)}.')
    project = map_editor.read_json(sources[0].read_text(encoding='utf-8'))
    # Regenerating records roster assignments; --check only compares.
    files = map_editor.export_and_assign(project)[0] if assign else map_editor.export_files(project)
    return {name: data for name, data in files.items() if name == 'world.ratw' or name.startswith('cells/')}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--check', action='store_true', help='fail if any bundled world is stale')
    args = parser.parse_args()
    stale = []
    for folder in sorted(p for p in WORLDS.iterdir() if p.is_dir()):
        files = generated(folder, assign=not args.check)
        existing = {str(p.relative_to(folder)): p for p in (folder / 'cells').glob('*.cell')}
        existing['world.ratw'] = folder / 'world.ratw'
        changed = [name for name, data in files.items()
                   if not existing.get(name, Path('/nonexistent')).is_file()
                   or existing[name].read_text(encoding='utf-8') != data]
        removed = [name for name in existing if name not in files and existing[name].exists()]
        if args.check:
            stale += [f'{folder.name}/{name}' for name in changed + removed]
            continue
        for name in removed:
            existing[name].unlink()
        for name in changed:
            (folder / name).parent.mkdir(parents=True, exist_ok=True)
            (folder / name).write_text(files[name], encoding='utf-8')
        print(f'{folder.name}: {len(changed)} updated, {len(removed)} removed, {len(files)} files.')
    if stale:
        print('Stale bundled world files (run python3 tools/bundle_worlds.py):\n  ' + '\n  '.join(stale), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
