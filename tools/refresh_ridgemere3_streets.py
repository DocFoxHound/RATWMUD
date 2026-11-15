#!/usr/bin/env python3
"""Flatten Ridgemere3 streets, clear Sump fences, and place text-glyph street lamps.

Run with --apply after reviewing the dry-run counts. Uses the editor's revision-checked save.
"""
from __future__ import annotations

import argparse
import collections
import gzip
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import world_db
import world_store

CITY = {
    'w_ridgemere_0_0': (176, 256, 144, 256),
    'w_ridgemere_0_1': (176, 256, 0, 208),
    'w_ridgemere_1_0': (0, 160, 144, 256),
    'w_ridgemere_1_1': (0, 160, 0, 196),
}
WATER = {'W', 'w', '~', '8'}  # The quay, river and bridges keep their own levels.
ROOF = {'Z', 'L', 'Q', '['}
FRONTAGE = chr(96)  # A solid, character-drawn private entrance.


def palace(x: int, y: int) -> bool:
    return 36 <= x <= 126 and 146 <= y <= 178


def refresh(project: dict) -> collections.Counter:
    counts: collections.Counter = collections.Counter()
    by_id = {cell['id']: cell for cell in project['cells'] if cell['id'] in CITY}
    protected = collections.defaultdict(set)
    for link in project['links']:
        for side in ('a', 'b'):
            at = link[side]
            cell = by_id.get(at['cell'])
            if not cell or link['kind'] != 'door':
                continue
            x, y = int(at['x']), int(at['y'])
            if any(0 <= nx < 256 and 0 <= ny < 256 and cell['terrain'][ny][nx] in WATER
                   for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1))):
                protected[cell['id']].update((nx, ny) for ny in range(max(0, y - 1), min(256, y + 2))
                                             for nx in range(max(0, x - 1), min(256, x + 2)))
    for cell in project['cells']:
        bounds = CITY.get(cell['id'])
        if not bounds:
            continue
        x0, x1, y0, y1 = bounds
        rows = [list(row) for row in cell['terrain']]
        heights = cell['heights']
        for y in range(y0, y1):
            for x in range(x0, x1):
                if (x, y) in protected[cell['id']] or (
                    cell['id'] == 'w_ridgemere_1_1' and x in (51, 80, 109) and y in (144, 145)
                ):
                    continue
                code = rows[y][x]
                if code in WATER:
                    continue
                if code == '^':
                    # Road steps become street/flagstones; the Palace gets a single clear approach below.
                    around = [rows[ny][nx] for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1))
                              if 0 <= nx < 256 and 0 <= ny < 256]
                    rows[y][x] = '_' if around.count('_') > around.count('f') else 'f'
                    counts['stairs removed'] += 1
                if cell['id'] == 'w_ridgemere_0_1' and code == '|':
                    rows[y][x] = 'd'
                    counts['Sump fences removed'] += 1
                if cell['id'] == 'w_ridgemere_1_1' and x < 56 and y < 112 and code == '|':
                    rows[y][x] = 'd'
                    counts['Sump fringe fences removed'] += 1
                if cell['id'] == 'w_ridgemere_1_1' and palace(x, y):
                    continue
                key = f'{x},{y}'
                if heights.get(key) != 2:
                    heights[key] = 2
                    counts['tiles leveled'] += 1
        if cell['id'] == 'w_ridgemere_1_1':
            for x in (51, 80, 109):
                for y in (144, 145):
                    if rows[y][x] != '^':
                        rows[y][x] = '^'
                        counts['Palace approach stairs'] += 1
                    heights[f'{x},{y}'] = 2.5 if y == 145 else 2
        # A lamp is a single character tile on a wide sidewalk beside a cobbled street.
        # Re-running the script keeps existing lamps at the same sites.
        for target_y in range(y0 + 10, y1 - 8, 24):
            for target_x in range(x0 + 10, x1 - 8, 24):
                if cell['id'] == 'w_ridgemere_1_1' and palace(target_x, target_y):
                    continue
                nearby = [(x, y) for y in range(max(y0, target_y - 6), min(y1, target_y + 7))
                          for x in range(max(x0, target_x - 6), min(x1, target_x + 7))]
                if any(rows[y][x] == "'" for x, y in nearby):
                    continue
                candidates = []
                for x, y in nearby:
                    if rows[y][x] != 'f' or (cell['id'] == 'w_ridgemere_1_1' and palace(x, y)):
                        continue
                    adjacent = [rows[ny][nx] for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1))
                                if 0 <= nx < 256 and 0 <= ny < 256]
                    if adjacent.count('_') == 0 or sum(g in 'f_d./' for g in adjacent) < 3:
                        continue
                    candidates.append((abs(x - target_x) + abs(y - target_y), y, x))
                if candidates:
                    _, y, x = min(candidates)
                    rows[y][x] = "'"
                    counts['street lamps placed'] += 1
        if cell['id'] == 'w_ridgemere_1_1':
            # Market Square is a broad paved plaza; its lamps sit at the edges, away from the stalls.
            for x, y in ((91, 40), (117, 40), (91, 56), (117, 56)):
                if rows[y][x] == '_':
                    rows[y][x] = "'"
                    counts['Market Square lamps placed'] += 1
        cell['terrain'] = [''.join(row) for row in rows]
    mark_private_frontages(project, counts)
    return counts


def mark_private_frontages(project: dict, counts: collections.Counter) -> None:
    """Give sizeable doorless buildings a sealed facade; clear tiny roofed debris/sheds."""
    cells = {cell['id']: cell for cell in project['cells'] if cell['id'] in CITY}
    grid = {}
    for cell in cells.values():
        for y, row in enumerate(cell['terrain']):
            for x, code in enumerate(row):
                if code in ROOF or code in '#H]' or code == FRONTAGE:
                    grid[(cell['x'] + x, cell['y'] + y)] = (cell, x, y, code)
    frontages = {point for point, (_, _, _, code) in grid.items() if code == FRONTAGE}
    entrances = set()
    for link in project['links']:
        for side in ('a', 'b'):
            at = link[side]
            cell = cells.get(at['cell'])
            if cell and link['kind'] == 'door':
                entrances.add((cell['x'] + int(at['x']), cell['y'] + int(at['y'])))
    changed = {cid: [list(row) for row in cell['terrain']] for cid, cell in cells.items()}
    seen = set()
    for start, (_, _, _, code) in grid.items():
        if code not in ROOF or start in seen:
            continue
        stack = [start]
        seen.add(start)
        component = []
        while stack:
            point = stack.pop()
            component.append(point)
            x, y = point
            for next_point in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if next_point not in seen and next_point in grid and grid[next_point][3] in ROOF:
                    seen.add(next_point)
                    stack.append(next_point)
        if len(component) < 4 or not any(
            (lambda c, x, y: c['id'] in CITY and CITY[c['id']][0] <= x < CITY[c['id']][1] and
             CITY[c['id']][2] <= y < CITY[c['id']][3])(*grid[point][:3]) for point in component
        ):
            continue
        if any(abs(x - dx) + abs(y - dy) <= 3 for x, y in component for dx, dy in entrances):
            continue
        if any(abs(x - fx) + abs(y - fy) <= 2 for x, y in component for fx, fy in frontages):
            continue
        if len(component) <= 8:
            for point in component:
                cell, x, y, _ = grid[point]
                changed[cell['id']][y][x] = 'X'
            counts['tiny doorless roofs cleared'] += 1
            continue
        candidates = []
        for wx, wy in component:
            for nx, ny in ((wx - 1, wy), (wx + 1, wy), (wx, wy - 1), (wx, wy + 1)):
                if (nx, ny) not in grid:
                    continue
                cell, x, y, code = grid[(nx, ny)]
                if code == FRONTAGE or (nx, ny) in frontages or any(
                    abs(nx - dx) + abs(ny - dy) <= 2 for dx, dy in entrances
                ):
                    continue
                for tx, ty in ((nx - 1, ny), (nx + 1, ny), (nx, ny - 1), (nx, ny + 1)):
                    local_x, local_y = tx - cell['x'], ty - cell['y']
                    if 0 <= local_x < 256 and 0 <= local_y < 256 and cell['terrain'][local_y][local_x] in 'f_d./,;':
                        candidates.append((0 if code in ROOF else 1, wy, wx, cell, x, y))
                        break
        if candidates:
            _, _, _, cell, x, y = min(candidates, key=lambda item: item[:3])
            changed[cell['id']][y][x] = FRONTAGE
            frontages.add((cell['x'] + x, cell['y'] + y))
            counts['private frontages marked'] += 1
    for cid, rows in changed.items():
        cells[cid]['terrain'] = [''.join(row) for row in rows]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    with world_db.connect('ridgemere3', 'editor') as conn:
        project, revision, seen = world_store.load_world(conn, 'upper_accord', with_seen=True)
        if args.apply:
            backup = Path('/tmp') / f'ridgemere3-before-streets-rev{revision}.json.gz'
            with gzip.open(backup, 'wt') as out:
                json.dump(project, out, separators=(',', ':'))
            print(f'Backup: {backup}')
        counts = refresh(project)
        print('Revision:', revision, dict(counts))
        if args.apply and counts:
            print('Saved revision:', world_store.save_world(conn, project, revision, seen=seen))


if __name__ == '__main__':
    main()
