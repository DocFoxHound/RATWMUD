#!/usr/bin/env python3
"""Build and distinguish Ridgemere3's seven active community towns.

The older ``tn_`` rooms are historical remnants.  This pass only extends the
``cn_`` settlements, preserving their existing buildings, residents and doors.
Run without --apply to inspect proposed additions.  --apply saves a backed up,
revision-checked world edit and updates its community contract.  The same pass
runs when a fresh realm is generated.
"""
from __future__ import annotations

import argparse
import collections
import gzip
import heapq
import json
import math
import random
from pathlib import Path

import numpy as np

import path_check as PC
from . import realm2_places as P
from .buildings import Building
from .canvas import Canvas
from .cities import grow
from .site import Lots, Site, door_tile

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = ROOT / 'Data/Economy/communities.json'
RADIUS = 94
# The built market already includes a store and an alehouse or inn.  These
# additions make a walk through each place feel like a town while retaining
# clear ground around its working square and its farms' roads.
HOMES = {'coldwater': 2, 'larkfield': 2, 'southmeadow': 3, 'fenshore': 3,
         'saltwell': 3, 'oakhaven': 3, 'frosthollow': 3}
INNS = {'larkfield': 'The Long River Inn', 'southmeadow': 'The Sunfield Inn',
        'fenshore': 'The Reed and Ferry', 'oakhaven': 'The Sound Road Inn',
        'frosthollow': 'The Pass House'}
# Relative to each green.  Each route starts and ends on an existing street;
# its intermediate points give the town a recognisable street plan.  All the
# materials are existing printable terrain codes, never image assets.
STREET_PLANS = {
    'coldwater': {'surface': '_', 'lane': 'f', 'width': 2, 'shape': 'crossroads and market blocks',
                  'paths': [[(-45, -18), (-30, -24), (0, -24), (30, -24), (45, -18)],
                            [(-45, 18), (-30, 24), (0, 24), (30, 24), (45, 18)]]},
    'larkfield': {'surface': 'd', 'lane': 'f', 'width': 2, 'shape': 'long market street and grain lanes',
                  'paths': [[(-50, -9), (-25, -9), (0, -9), (25, -9), (50, -9)],
                            [(-13, -48), (-13, -24), (-13, 0), (-13, 30), (-13, 48)]]},
    'southmeadow': {'surface': 'f', 'lane': '/', 'width': 2, 'shape': 'diagonal vine lanes',
                    'paths': [[(-48, 30), (-25, 12), (2, -12), (42, -40)],
                              [(-42, -30), (-20, -12), (12, 18), (40, 35)]]},
    'fenshore': {'surface': '8', 'lane': '8', 'width': 1, 'shape': 'branching fen boardwalks',
                 'paths': [[(-32, -12), (-14, -20), (10, -18), (32, -8)],
                           [(-28, 20), (-8, 27), (15, 25), (32, 16)]]},
    'saltwell': {'surface': 'f', 'lane': 'd', 'width': 2, 'shape': 'oasis crescent and caravan arc',
                 'paths': [[(-34, -26), (-42, -6), (-38, 20), (-21, 38)],
                           [(12, -40), (35, -20), (40, 12), (13, 39)]]},
    'oakhaven': {'surface': '/', 'lane': 'd', 'width': 2, 'shape': 'staggered woodland lanes',
                 'paths': [[(-32, -18), (-18, -29), (0, -20), (17, -30), (33, -17)],
                           [(-31, 17), (-14, 29), (5, 19), (30, 28)]]},
    'frosthollow': {'surface': '_', 'lane': 'f', 'width': 2, 'shape': 'pass road and miners’ spurs',
                    'paths': [[(-52, -8), (-26, -8), (0, -8), (28, -8), (52, -8)],
                              [(-8, -50), (-8, -25), (-8, 2), (-8, 37)]]},
}


def nearest(mask, point, reach):
    x, y = point
    H, W = mask.shape
    options = [(math.hypot(xx - x, yy - y), xx, yy)
               for yy in range(max(1, y - reach), min(H - 1, y + reach + 1))
               for xx in range(max(1, x - reach), min(W - 1, x + reach + 1)) if mask[yy, xx]]
    if not options:
        return None
    _, xx, yy = min(options)
    return xx, yy


def route(mask, heights, codes, start, end):
    """A short, height-safe lane around existing buildings and garden plots."""
    if start is None or end is None:
        return None
    best = {start: 0.0}
    back = {}
    queue = [(math.dist(start, end), 0.0, start)]
    while queue:
        _, cost, at = heapq.heappop(queue)
        if cost > best.get(at, float('inf')) + 1e-6:
            continue
        if at == end:
            path = [at]
            while path[-1] in back:
                path.append(back[path[-1]])
            path.reverse()
            return path if len(path) <= 5 * math.dist(start, end) + 40 else None
        x, y = at
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if ny < 0 or nx < 0 or ny >= mask.shape[0] or nx >= mask.shape[1] or not mask[ny, nx] or \
                    not PC.step_allowed(float(heights[y, x]), chr(codes[y, x]) in PC.RAMPS,
                                        float(heights[ny, nx]), chr(codes[ny, nx]) in PC.RAMPS):
                continue
            # Keep the lane near its intended line without tunnelling through
            # walls or tracing the entire old oval street instead.
            length = math.dist(start, end) or 1
            offset = abs((end[0] - start[0]) * (start[1] - ny) -
                         (start[0] - nx) * (end[1] - start[1])) / length
            next_cost = cost + 1 + .035 * offset
            if next_cost < best.get((nx, ny), float('inf')):
                best[nx, ny] = next_cost
                back[nx, ny] = at
                heapq.heappush(queue, (next_cost + math.dist((nx, ny), end), next_cost, (nx, ny)))
    return None


def decorate_town(canvas, cid, counts):
    """Give one already built town its own street plan and nearby ground texture."""
    plan = STREET_PLANS[cid]
    c = canvas.codes
    H, W = c.shape
    yy, xx = np.indices(c.shape)
    rx, ry = xx - W // 2, yy - H // 2
    dist = np.hypot(rx, ry)
    old_street = np.isin(c, [ord(ch) for ch in 'd/8_f'])
    core = old_street & (dist < 43) & (c != ord('8'))
    changed = core & (c != ord(plan['surface']))
    c[changed] = ord(plan['surface'])
    counts[f'{cid} street texture'] += int(changed.sum())
    if cid == 'frosthollow':
        roof = (c == ord('L')) & (dist < 68)
        c[roof] = ord('Z')
        counts[f'{cid} slate roofs'] += int(roof.sum())

    # These ground codes are passable at their existing heights; crops,
    # gardens, trees, doors, wells and water are never paved through.
    ground = np.isin(c, [ord(ch) for ch in ',;".0D!-*sr(_fd/8']) & (dist < 70)
    travel = ground | (np.isin(c, [ord(ch) for ch in ':EB']) & (dist < 70))
    for points in plan['paths']:
        stops = []
        for i, (dx, dy) in enumerate(points):
            target = (W // 2 + dx, H // 2 + dy)
            anchor = old_street & travel if i in (0, len(points) - 1) else travel
            stops.append(nearest(anchor, target, 16 if i in (0, len(points) - 1) else 8))
        pieces = [route(travel, canvas.heights, c, a, b) for a, b in zip(stops, stops[1:])]
        if any(piece is None for piece in pieces):
            continue
        points_on_lane = {point for piece in pieces for point in piece}
        for x, y in points_on_lane:
            for nx, ny in ((x, y), (x + 1, y))[:plan['width']]:
                if 0 <= nx < W and 0 <= ny < H and ground[ny, nx] and \
                        abs(float(canvas.heights[ny, nx]) - float(canvas.heights[y, x])) <= .5 and \
                        c[ny, nx] != ord('8') and c[ny, nx] != ord(plan['lane']):
                    c[ny, nx] = ord(plan['lane'])
                    counts[f'{cid} lane tiles'] += 1

    street = np.isin(c, [ord(ch) for ch in 'd/8_f'])
    verge = grow(street, 3) & (dist > 10) & (dist < 58)
    signature = {
        'coldwater': ((rx + 2 * ry) % 11 == 0, '3', ',"'),
        'larkfield': ((ry % 7 <= 1) & (dist > 23), '4', ',;'),
        'southmeadow': ((rx + ry) % 7 == 0, '3', ',;'),
        'fenshore': (((2 * rx + ry) % 9 <= 1) & grow(np.isin(c, [ord(ch) for ch in 'wW~']), 2), 'E', ',D'),
        'saltwell': ((rx + 2 * ry) % 6 == 0, '(', '0D'),
        'oakhaven': ((rx - 2 * ry) % 7 <= 2, '!', ',"'),
        'frosthollow': ((rx + ry) % 8 <= 1, 's', ',-'),
    }
    pattern, tile, source = signature[cid]
    accent = verge & pattern & np.isin(c, [ord(ch) for ch in source])
    c[accent] = ord(tile)
    counts[f'{cid} verge texture'] += int(accent.sum())



class TownSite(Site):
    def __init__(self, canvas, cells, prefix):
        super().__init__(canvas, cells)
        self.prefix = prefix
        self.key = ''

    def unique(self, name):
        result = self.prefix + self.key
        if result in self.ids or len(result) > 36:
            raise ValueError(f'duplicate or overlong room id: {result}')
        self.ids.add(result)
        return result

    def put(self, key, building, x, y, facing, region, ground):
        self.key = key
        self.place(building, x, y, facing, region, open_door=building.kind != 'house', ground=ground)
        return self.manifest[-1]


def room_record(record, room):
    return {'area': room['id'], 'name': room['name'], 'capacity': len(room['beds']), 'beds': room['beds']}


def small_inn(rng, style, title):
    """A compact roadside inn, including actual guest beds upstairs."""
    look = P.STYLES[style]
    down = P.Plan(15, 11, look['wall'], look['floor'], rng)
    down.run(2, 2, 1, 0, 5, '=')
    down.worker(4, 1)
    for x in (2, 6, 10):
        down.run(x, 5, 1, 0, 2, 'T')
        down.put(x, 6, 'c')
    down.put(12, 1, 'h')
    up = P.Plan(15, 11, look['wall'], look['home_floor'], rng)
    up.g[up.h - 1][up.w // 2] = look['wall']
    up.keep = set()
    for x, y in ((2, 2), (2, 3), (6, 2), (6, 3), (10, 2), (10, 3)):
        up.bed(x, y)
    P.stairs_between(down, up, 12, 7)
    rooms = [down.room('', title, f'{title}: a roadside taproom and a place for travellers to rest.'),
             up.room('up', f'{title}, upstairs',
                     'Guest rooms under the roof, with pallets, blankets and water jugs.', z=1)]
    return Building('inn', title, style, P.fp(down.w, down.h), rooms,
                    [(('', 12, 7), ('up', 12, 7))])


def expand(project, contract, realm=None):
    existing = {r['id'] for r in project['rooms']}
    counts = collections.Counter()
    for com in contract['communities']:
        cid = com['id']
        cx, cy = com['centre']['x'], com['centre']['y']
        x0, y0 = cx - RADIUS, cy - RADIUS
        grid = PC.Grid(project, (x0, y0, cx + RADIUS + 1, cy + RADIUS + 1), com['name'])
        c = Canvas(grid.w, grid.h)
        c.codes[:] = np.array([[ord(ch) for ch in row] for row in grid.code], dtype=np.uint8)
        c.heights[:] = np.asarray(grid.height, dtype=np.float32)
        cells = [(cell['id'], cell['x'] - x0, cell['y'] - y0, cell['width'], cell['height'])
                 for cell in grid.cells]
        site = TownSite(c, cells, f'cn_{cid}_')
        site.ids = set(existing)
        rng = random.Random(f'ridgemere3:town:{cid}')
        street = np.isin(c.codes, [ord(ch) for ch in 'd/8_f'])
        solid = np.isin(c.codes, [ord(ch) for ch in PC.SOLID])
        water = np.isin(c.codes, [ord(ch) for ch in PC.WATER])
        reserved = np.zeros(c.codes.shape, dtype=bool)
        for wx, wy in grid.reserved:
            if grid.inside(wx, wy):
                reserved[wy - y0, wx - x0] = True
        yy, xx = np.indices(c.codes.shape)
        near = np.hypot(xx - RADIUS, yy - RADIUS) < 76
        # A building gets a two-tile gap from existing solid footprints,
        # gardens, doors and named spots.  The facing side must meet a road.
        open_ground = near & ~street & ~water & ~grow(solid | reserved, 1)
        open_ground &= ~np.isin(c.codes, [ord(ch) for ch in PC.PATH])
        lots = Lots(open_ground, street, gap=1)
        additions = []
        if cid in INNS:
            title = INNS[cid]
            additions.append(('guest_inn', small_inn(rng, com['style'], title), 'inn'))
        for n in range(1, HOMES[cid] + 1):
            title = f'{com["name"]} Town House {n}'
            plan = P.house(rng, com['style'], 3)
            building = P.one('house', title, com['style'], plan,
                             f'{title}: a small family home beside the town streets.')
            additions.append((f'town_house_{n}', building, 'home'))

        new_lots = 0
        for key, building, kind in additions:
            if site.prefix + key in existing:
                counts['already built'] += 1
                continue
            fw, fh = building.footprint
            spot = None
            for _, bx, by, facing, w, h in lots.candidates(fw, fh, (RADIUS, RADIUS), limit=5000):
                if not lots.clear(bx, by, w, h):
                    continue
                _, outside = door_tile(bx, by, w, h, facing)
                ox, oy = outside
                if not (0 <= ox < grid.w and 0 <= oy < grid.h) or not street[oy, ox]:
                    continue
                level = float(c.heights[oy, ox])
                if np.max(np.abs(c.heights[by:by + h, bx:bx + w] - level)) > 1:
                    continue
                spot = bx, by, facing, w, h, level
                break
            if spot is None:
                raise RuntimeError(f'{com["name"]}: no clear street-front lot for {building.name}')
            bx, by, facing, w, h, level = spot
            record = site.put(key, building, bx, by, facing, cid, level)
            lots.take(bx, by, w, h)
            door = record['door']
            cell_id, lx, ly = site.cell_at(door['x'], door['y'])
            address = {'cell': cell_id, 'x': lx, 'y': ly, 'worldX': door['x'] + x0, 'worldY': door['y'] + y0}
            if kind == 'home':
                com['homes'].append({**room_record(record, record['rooms'][0]), 'door': address})
                com['beds'] += len(record['rooms'][0]['beds'])
            else:
                downstairs, upstairs = record['rooms']
                com['market'].append({'area': downstairs['id'], 'name': record['name'], 'business': 'inn',
                                      'posts': [], 'work': downstairs['work'], 'stations': downstairs['stations'],
                                      'door': address, 'otherRooms': [upstairs['id']],
                                      'guestBeds': len(upstairs['beds'])})
            counts[f'{cid} {kind}'] += 1
            new_lots += 1
            existing.update(r['id'] for r in site.rooms)

        if 'layout' not in com:
            decorate_town(c, cid, counts)
            com['layout'] = {'shape': STREET_PLANS[cid]['shape'], 'surface': STREET_PLANS[cid]['surface'],
                             'lane': STREET_PLANS[cid]['lane']}
        for y in range(grid.h):
            for x in range(grid.w):
                ch = chr(c.codes[y, x])
                if ch != grid.code[y][x] or abs(float(c.heights[y, x]) - grid.height[y][x]) > .01:
                    grid.set(x + x0, y + y0, ch, float(c.heights[y, x]))
                    if realm is not None:
                        realm.codes[y + y0, x + x0] = ord(ch)
                        realm.h[y + y0, x + x0] = c.heights[y, x]
        # A new lane can put an existing street lamp in the walking line.
        # Move those lamps to the verge with the same rule as the city pass.
        from refresh_ridgemere3_map import VERGE, move_lamps
        moved = []
        move_lamps(grid, set('d/8_f'), VERGE, counts, moved, com['name'])
        if realm is not None:
            for _, old, new in moved:
                for pos in (old, new):
                    if pos is not None:
                        wx, wy = pos
                        realm.codes[wy, wx] = ord(grid.at(wx, wy))
                        realm.h[wy, wx] = grid.h_at(wx, wy)
        grid.commit()
        for room in site.rooms:
            room['worldX'] += x0
            room['worldY'] += y0
        project['rooms'].extend(site.rooms)
        project['links'].extend(site.links)
        com['town'] = True
        com['roomToGrow']['cottageLots'] = max(0, com['roomToGrow']['cottageLots'] - new_lots)
    contract['about'] = contract['about'].replace("A community's `town` (false) keeps it a plain place on a restart, never a town of its own (World::setupTowns), until a later step makes it one.",
        "A community's `town` (true) makes it a town on restart (World::setupTowns).")
    return counts


def verify_contract(project, contract):
    """The live contract's town beds and doors must point into this same map."""
    rooms = {r['id']: r for r in project['rooms']}
    links = {link['id']: link for link in project['links']}
    cells = {cell['id']: cell for cell in project['cells']}
    for com in contract['communities']:
        if not com['town']:
            raise ValueError(f'{com["name"]} is still marked as a plain region')
        for entry in com['homes'] + com['market']:
            area = entry['area']
            room = rooms.get(area)
            link = links.get(f'{area}_door')
            door = entry['door']
            cell = cells.get(door['cell'])
            if room is None or link is None or cell is None or link['a'] != {k: door[k] for k in ('cell', 'x', 'y')} or \
                    link['b']['cell'] != area or (cell['x'] + door['x'], cell['y'] + door['y']) != \
                    (door['worldX'], door['worldY']):
                raise ValueError(f'{com["name"]}: stale door or room for {area}')
            if 'capacity' in entry:
                if entry['capacity'] != len(entry['beds']) or any(
                        room['terrain'][bed['y']][bed['x']] != 'b' for bed in entry['beds']):
                    raise ValueError(f'{com["name"]}: stale beds for {area}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--world', default='ridgemere3')
    args = parser.parse_args()
    if args.world != 'ridgemere3':
        parser.error('This town pass is scoped to Ridgemere3')
    import world_db
    import world_store
    with world_db.connect(args.world, 'editor') as conn:
        project, revision, seen = world_store.load_world(conn, 'upper_accord', with_seen=True)
        contract = json.loads(CONTRACT.read_text())
        counts = expand(project, contract)
        verify_contract(project, contract)
        print(f'Ridgemere3 revision {revision}: {dict(counts)}')
        if args.apply and sum(v for k, v in counts.items() if k != 'already built'):
            import map_editor
            map_editor.check_project(project, for_game=False)
            for com in contract['communities']:
                cx, cy = com['centre']['x'], com['centre']['y']
                box = (cx - RADIUS, cy - RADIUS, cx + RADIUS + 1, cy + RADIUS + 1)
                failures = PC.failures(PC.check(PC.Grid(project, box, com['name'])))
                if failures:
                    raise RuntimeError(f'{com["name"]}: {len(failures)} blocked paths or doors: {failures[:2]}')
            backup = ROOT / 'Saved/Backups' / f'ridgemere3-before-towns-rev{revision}.json.gz'
            backup.parent.mkdir(parents=True, exist_ok=True)
            original, _, _ = world_store.load_world(conn, 'upper_accord', with_seen=True)
            with gzip.open(backup, 'wt') as out:
                json.dump(original, out, separators=(',', ':'))
            print('Backup:', backup)
            new_revision = world_store.save_world(conn, project, revision, seen=seen)
            CONTRACT.write_text(json.dumps(contract, indent=1, ensure_ascii=False) + '\n')
            print('Saved revision:', new_revision)


if __name__ == '__main__':
    main()
