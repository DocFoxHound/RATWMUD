#!/usr/bin/env python3
"""Flatten walled Ridgemere, open every path and door approach, and light the streets, in Ridgemere 3 (doc 82).

  python3 tools/refresh_ridgemere3_map.py [--world ridgemere3]           what it would change (nothing is written)
  python3 tools/refresh_ridgemere3_map.py --apply                         change it (a revision-checked save)
  python3 tools/refresh_ridgemere3_map.py --file IN.json[.gz] --out OUT   the same on a saved project, for a look

1. The city inside its walls has one ground height (STREET): every tile but water, so no edge of the old Rise, the
   Palace's terrace or a street's steps is left too high to step over, and the stairs that crossed them become paving.
   Interiors and their upper and lower floors are rooms, and are not touched.
2. tools/path_check.py walks the city and the six towns: whatever closes a path or a door's approach (a barrel, crates, a
   stall, a fence, a tree...) is moved aside, or cleared where it isn't a thing one carries.
3. Street lamps (the `'` tile): beside Ridgemere's cobbled streets and each town's roads about every LAMP_SPACING tiles,
   where a lamp closes nothing. The lamps already standing count.

Running it again changes nothing once the world is as it leaves it.
"""
from __future__ import annotations

import argparse
import collections
import gzip
import json
import math
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import path_check as PC

STREET = 2                                       # The city's one ground height (what its streets already were).
WALLS = (168, 150, 472, 456)                     # The city wall's outer edge, world tiles [x0, x1) x [y0, y1).
WALL = 4                                         # Its depth.
LAMP = "'"
LAMP_SPACING = 16                                # Tiles between lamps along a street (a light pool is about 8 across).
TOWN_REACH = 40                                  # A town's lit roads: within this of its doors' middle.
VERGE = frozenset(',";35.!-0')                   # Ground a town lamp may stand on beside its road.


def flatten(project, counts):
    """One ground height inside the city's walls (water keeps its own); stairs there become paving. Each gate through
    the wall is graded in half steps between the street and the ground outside it."""
    x0, y0, x1, y1 = WALLS
    grid = PC.Grid(project, (x0 - 1, y0 - 1, x1 + 1, y1 + 1), 'Ridgemere')

    # A door onto the water (the Sunken Street's, onto the canal) keeps the water's level round it.
    keep = set()
    for (x, y) in grid.doors:
        if any(grid.at(x + dx, y + dy) in PC.WATER for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            keep.update((x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1))

    def level(x, y, height):
        g = grid.at(x, y)
        if g is None or g in PC.WATER or (x, y) in keep:
            return
        if g in PC.RAMPS:
            grid.set(x, y, ground_beside(grid, x, y))
            counts['city stairs and slopes paved'] += 1
        if grid.h_at(x, y) != height:
            grid.set_height(x, y, height)
            counts['city tiles levelled'] += 1

    for y in range(y0 + WALL, y1 - WALL):
        for x in range(x0 + WALL, x1 - WALL):
            level(x, y, STREET)
    # The wall's depth, a line at a time from outside inwards: (outside tile, the wall's tiles in order).
    lines = []
    for y in range(y0 + WALL, y1 - WALL):
        lines.append(((x0 - 1, y), [(x0 + k, y) for k in range(WALL)]))
        lines.append(((x1, y), [(x1 - 1 - k, y) for k in range(WALL)]))
    for x in range(x0 + WALL, x1 - WALL):
        lines.append(((x, y0 - 1), [(x, y0 + k) for k in range(WALL)]))
        lines.append(((x, y1), [(x, y1 - 1 - k) for k in range(WALL)]))
    for outside, band in lines:
        if all(grid.at(*t) in PC.SOLID for t in band):
            continue                           # (The wall itself.)
        g = grid.at(*outside)
        low = STREET if g is None or g in PC.SOLID or g in PC.WATER else grid.h_at(*outside)
        before = counts['city tiles levelled'] + counts['city stairs and slopes paved']
        for k, t in enumerate(band):
            level(*t, round((low + (STREET - low) * (k + 1) / (WALL + 1)) * 2) / 2)
        if counts['city tiles levelled'] + counts['city stairs and slopes paved'] > before:
            counts['gate lines graded'] += 1
    for x, y in [(x, y) for x in range(x0, x1) for y in range(y0, y1)
                 if (x < x0 + WALL or x >= x1 - WALL) and (y < y0 + WALL or y >= y1 - WALL)]:
        level(x, y, STREET)                    # (The corners.)
    grid.commit()


def ground_beside(grid, x, y):
    around = [grid.at(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))]
    paving = [g for g in around if g and g in '_f']
    if paving:
        return collections.Counter(paving).most_common(1)[0][0]
    return PC.ground_for(grid, x, y)


def lamp_spots(grid, streets, verge, reach=None):
    """Where lamps may stand beside these street tiles: [(x, y)] in a steady order."""
    spots = []
    for y in range(grid.y0 + 1, grid.y1 - 1):
        for x in range(grid.x0 + 1, grid.x1 - 1):
            g = grid.at(x, y)
            if g not in verge or (reach and not reach(x, y)):
                continue
            beside = [grid.at(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))]
            if not any(b in streets for b in beside):
                continue
            if any(abs(x - dx) + abs(y - dy) <= 2 for dx, dy in grid.doors):
                continue
            spots.append((x, y))
    return spots


def lamp_ok(grid, x, y):
    """A lamp may stand here: open ground with open ground on at least three sides, closing no path."""
    if grid.at(x, y) in PC.SOLID or grid.at(x, y) in '+G' or grid.at(x, y) in PC.RAMPS or grid.near_reserved(x, y):
        return False
    ring = [(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy]
    if any(grid.at(*p) in PC.OBSTACLE for p in ring):
        return False                           # (Never against another prop: no new wall of things.)
    sides = [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    if sum(grid.walkable(*p) for p in sides) < 3:
        return False
    open_sides = [p for p in sides if grid.walkable(*p)]
    for p in open_sides[1:]:
        if PC.detour(grid, open_sides[0], p, frozenset({(x, y)}), 6) is None:
            return False
    return True


def place_lamps(grid, streets, verge, counts, label, reach=None):
    lamps = [(x, y) for y in range(grid.y0, grid.y1) for x in range(grid.x0, grid.x1) if grid.at(x, y) == LAMP]
    near = LAMP_SPACING * LAMP_SPACING
    placed = 0
    for x, y in lamp_spots(grid, streets, verge, reach):
        if any((x - lx) ** 2 + (y - ly) ** 2 < near for lx, ly in lamps):
            continue
        if not lamp_ok(grid, x, y):
            continue
        grid.set(x, y, LAMP)
        lamps.append((x, y))
        placed += 1
    if placed:
        counts[f'street lamps placed ({label})'] += placed
    return placed


def refresh(project, log=print):
    counts = collections.Counter()
    flatten(project, counts)
    report = {}
    for name, grid in PC.settlements(project).items():
        moves = []
        before = PC.check(grid)
        remaining = PC.fix(grid, counts, moves)
        if name == 'Ridgemere':
            x0, y0, x1, y1 = WALLS
            place_lamps(grid, {'_', 'd'}, frozenset('f_.d'), counts, name,
                        reach=lambda x, y: x0 + 4 <= x < x1 - 4 and y0 + 4 <= y < y1 - 4)
        else:
            doors = [p for p, _ in PC.door_tiles(grid)]
            if doors:
                mx, my = statistics.median(p[0] for p in doors), statistics.median(p[1] for p in doors)
                place_lamps(grid, {'d', '_'}, VERGE, counts, name,
                            reach=lambda x, y: math.hypot(x - mx, y - my) <= TOWN_REACH)
        remaining = PC.failures(PC.fix(grid, counts, moves))
        notes = [p for p in PC.check(grid) if p['kind'] == PC.SEALED]
        grid.commit()
        report[name] = {'found': before, 'moves': moves, 'remaining': remaining}
        log(f'{name}: {len(PC.failures(before))} problem(s) found, {len(moves)} obstacle(s) moved or cleared, '
            f'{len(remaining)} left')
        for p in PC.failures(before):
            log(f'    found {p["kind"]} at {p["cell"]} {tuple(p["local"])}: {p["what"]}')
        for g, a, b in moves:
            log(f'    {g!r} at {PC.local(grid, *a)[0]} {PC.local(grid, *a)[1:]} ' +
                (f'moved to {PC.local(grid, *b)[1:]}' if b else 'cleared'))
        if notes:
            log(f'    ({len(notes)} sealed yard(s) noted: walled round, with no door)')
        for p in remaining:
            log(f'    LEFT {p["kind"]} at {p["cell"]} {tuple(p["local"])}: {p["what"]}')
    return counts, report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--world', default='ridgemere3')
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--file', type=Path, help='a saved project (JSON, or .json.gz) instead of the database')
    parser.add_argument('--out', type=Path, help='with --file: where to write the result')
    args = parser.parse_args(argv)
    if args.file:
        opener = gzip.open if args.file.suffix == '.gz' else open
        with opener(args.file, 'rt') as f:
            project = json.load(f)
        counts, report = refresh(project)
        print(dict(counts))
        if args.out:
            with (gzip.open if args.out.suffix == '.gz' else open)(args.out, 'wt') as f:
                json.dump(project, f, separators=(',', ':'))
        return 1 if any(r['remaining'] for r in report.values()) else 0
    import world_db
    import world_store
    with world_db.connect(args.world, 'editor') as conn:
        project, revision, seen = world_store.load_world(conn, 'upper_accord', with_seen=True)
        if args.apply:
            backup = Path('/tmp') / f'{args.world}-before-map-rev{revision}.json.gz'
            with gzip.open(backup, 'wt') as out:
                json.dump(project, out, separators=(',', ':'))
            print(f'Backup: {backup}')
        counts, report = refresh(project)
        print('Revision:', revision, dict(counts))
        if args.apply and +counts:
            print('Saved revision:', world_store.save_world(conn, project, revision, seen=seen))
    return 1 if any(r['remaining'] for r in report.values()) else 0


if __name__ == '__main__':
    sys.exit(main())
