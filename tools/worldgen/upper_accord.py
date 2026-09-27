"""Upper Accord: the capital of the Warden Order, in a crater between three peaks.

Built from the user's drawing (upper_accord_map.png): the walled city fills the valley; Concord Hall sits on the
northern peak's shoulder above a grand stair from the city's north corner; the Warden Training Grounds lie in a
long terrace on the eastern peak's flank beside the city's east wall; the Warden Order holds the southern peak's
shoulder, reached by a switchback path from the south gate. Trails join Concord Hall and the Order to the Training
Grounds outside the walls, and the main road climbs in from the west to the Main Gate.

  cd tools && python3 -m worldgen.upper_accord [--out DIR] [--preview DIR]
  cd tools && python3 -m worldgen.upper_accord --import-dev [--replace] [--offset X Y]
  cd tools && python3 -m worldgen.upper_accord --import-dev --replace-world

--import-dev adds the region to the world in the DEV database (or creates it there) and makes it the spawn.
The region is shifted by --offset so it does not overlap existing cells. --replace swaps out a previous import of
Upper Accord; without it, an existing import is left alone, since Atlas edits made to it since would be lost.
--replace-world instead makes Upper Accord the whole DEV world: the world there now (its places, NPCs, routes and
economy) is saved to artifacts/backups/ and deleted, in the same transaction that creates Upper Accord.

World tiles run x east, y south, over a 768 x 768 frame split into cells (see CELLS).
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

from . import field
from .canvas import Canvas, half, reachable
from . import preview
from .city import build_city
from . import campuses as campus_plans
from .site import Site

SIZE = 768
SEED = 1717

# id, name, x, y, width, height, description. The city and each campus has a cell of its own.
CELLS = [
    ('upper_accord', 'Upper Accord', 256, 256, 256, 256,
     'The capital of the Wardens fills the crater between three peaks: old stone streets, crowded roofs and '
     'market noise inside a wall older than anyone can name.'),
    ('concord_hall', 'Concord Hall', 256, 128, 128, 128,
     'On the northern peak\'s shoulder the Concord Hall rises in carved pale stone, all arches, banners and '
     'long echoing chambers where the free packs meet to be heard.'),
    ('training_grounds', 'Warden Training Grounds', 512, 256, 128, 256,
     'A long terrace cut into the eastern peak: packed yards, rings, targets and boulder fields where pups '
     'and trainees are worked until the mountain air burns.'),
    ('warden_order', 'Warden Order', 256, 512, 128, 128,
     'The Warden Order keeps the southern peak\'s shoulder behind blunt old walls. Nothing here is decorated; '
     'the stone has simply been here longer than the Order itself.'),
    ('northwest_heights', 'Northwest Heights', 0, 0, 256, 256,
     'The northern peak climbs out of pine and scree into bare rock and old snow.'),
    ('northern_ridge', 'Northern Ridge', 256, 0, 256, 128,
     'A windy ridge above Concord Hall, stone and stunted pine.'),
    ('north_saddle', 'North Saddle', 384, 128, 128, 128,
     'A high saddle between the northern and eastern peaks; the trail from Concord Hall to the Training '
     'Grounds crosses it.'),
    ('northeast_heights', 'Northeast Heights', 512, 0, 256, 256,
     'Broken ground falls away north and east of the eastern peak.'),
    ('western_approach', 'Western Approach', 0, 256, 256, 256,
     'The main road climbs from the lowlands through a pass between the northern and southern peaks toward '
     'the Main Gate.'),
    ('eastern_peak', 'Eastern Peak', 640, 256, 128, 256,
     'The long spine of the eastern peak, cliffs stacked like shelves and snow on the heights.'),
    ('southwest_slopes', 'Southwest Slopes', 0, 512, 256, 256,
     'The southern peak\'s western slopes, pine-dark and steep.'),
    ('south_saddle', 'South Saddle', 384, 512, 128, 128,
     'Between the southern peak and the eastern, the trail from the Warden Order to the Training Grounds '
     'picks along a rocky shelf.'),
    ('southern_peak', 'Southern Peak', 256, 640, 256, 128,
     'The summit of the southern peak, bare stone above the Warden Order.'),
    ('southeast_slopes', 'Southeast Slopes', 512, 512, 256, 256,
     'Long slopes of scree and pine fall south and east from the peaks.'),
]

# The city wall, traced from the drawing (world tiles), clockwise from the north corner.
CITY_WALL = [(397, 266), (497, 302), (483, 370), (471, 406), (465, 454), (455, 482), (433, 502), (371, 478),
             (311, 462), (291, 422), (271, 384), (319, 326), (359, 290)]
PLAZA_SPAWN = (385, 400)   # South side of the plaza fountain.
MAIN_GATE = (291, 422)       # West wall, where the main road arrives.
SOUTH_GATE = (372, 480)      # South wall: the switchback path to the Warden Order.
STAIR_GATE = (368, 285)      # North corner: the grand stair up to Concord Hall.

# Campus terraces: (x, y, width, height, ground height).
CONCORD = (268, 140, 108, 100, 9.0)
TRAINING = (520, 272, 104, 224, 5.0)
ORDER = (266, 522, 108, 100, 7.0)
PEAKS = [  # centre, spread, angle, height
    ((205, 215), (125, 160), -.6, 18.0),   # North
    ((690, 405), (95, 210), 0.0, 18.0),    # East
    ((300, 675), (150, 115), .2, 18.0),    # South
]

MAIN_ROAD = [(0, 430), (40, 432), (120, 426), (190, 434), (250, 428), (280, 423), (296, 422)]
STAIR = [(368, 292), (368, 280), (362, 262), (356, 246), (352, 238)]
SWITCHBACK = [(372, 476), (372, 488), (392, 494), (350, 500), (394, 507), (346, 513), (336, 522), (330, 530)]
CONCORD_TRAIL = [(376, 188), (400, 196), (430, 205), (460, 222), (490, 245), (515, 268), (530, 282)]
ORDER_TRAIL = [(374, 570), (400, 566), (430, 556), (460, 540), (490, 520), (515, 500), (530, 488)]


def terrain(c: Canvas):
    xs, ys = field.grid(SIZE, SIZE)
    # Lowlands at the frame's edge rise to the massif; each peak is a broad cone standing on it.
    r = np.hypot(xs - 384, ys - 400)
    height = -12 + 13 * field.smoothstep(395, 230, r)
    for (px, py), (sx, sy), angle, h in PEAKS:
        height = np.maximum(height, h * field.bump(xs, ys, px, py, sx, sy, angle, power=1.15) - 2)
    rough = field.fbm(SIZE, SIZE, 48, SEED, 4) - .5
    height += rough * 6 + (field.fbm(SIZE, SIZE, 14, SEED + 3, 2) - .5) * 2

    # The crater floor and the campus terraces are levelled, and the land blends into them.
    city = c.polygon_mask(CITY_WALL)
    flat = np.zeros_like(height)
    weight = np.zeros_like(height)

    def level(mask, h, reach):
        d = field.distance_to_mask(mask, reach + 1)
        w = 1 - field.smoothstep(0, reach, d)
        np.copyto(flat, h, where=w > weight)
        np.copyto(weight, w, where=w > weight)
    level(city, 0.0, 26)
    for x, y, w, h, level_h in (CONCORD, TRAINING, ORDER):
        mask = np.zeros_like(city)
        mask[y:y + h, x:x + w] = True
        level(mask, level_h, 18)
    height = height * (1 - weight) + flat * weight

    # Upper slopes break into ledges: terraces 2½ high, faded in above the foothills.
    terraced = np.floor(height / 2.5) * 2.5 + field.smoothstep(.78, 1, (height / 2.5) % 1) * 2.5
    steep = field.smoothstep(2, 7, height) * (1 - weight) * field.smoothstep(.35, .55, field.fbm(SIZE, SIZE, 60, SEED + 5))
    height = height * (1 - steep) + terraced * steep
    c.heights[:] = half(height)
    return city


def road(c: Canvas, points, width, ch, start_h=None, end_h=None, shoulder=4):
    """A path whose ground climbs evenly between its ends, blended into the land beside it."""
    mask = c.line_mask(points, width)
    lengths = np.cumsum([0] + [np.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(points, points[1:])])
    total = lengths[-1]
    h0 = c.heights[int(points[0][1]), int(points[0][0])] if start_h is None else start_h
    h1 = c.heights[int(points[-1][1]) - 1, int(points[-1][0])] if end_h is None else end_h
    # Height at each tile: its nearest point along the polyline, as a fraction of the way.
    wide = c.line_mask(points, width + 2 * shoulder)
    along = np.full(c.heights.shape, -1.0, dtype=np.float32)
    best = np.full(c.heights.shape, 1e9, dtype=np.float32)
    ys, xs = np.nonzero(wide)
    px, py = xs + .5, ys + .5
    for i, ((x1, y1), (x2, y2)) in enumerate(zip(points, points[1:])):
        dx, dy = x2 - x1, y2 - y1
        l2 = dx * dx + dy * dy or 1e-9
        t = np.clip(((px - x1) * dx + (py - y1) * dy) / l2, 0, 1)
        d = np.hypot(px - (x1 + t * dx), py - (y1 + t * dy))
        better = d < best[ys, xs]
        best[ys[better], xs[better]] = d[better]
        along[ys[better], xs[better]] = (lengths[i] + t[better] * np.sqrt(l2)) / total
    line_h = h0 + (h1 - h0) * along
    blend = 1 - field.smoothstep(width / 2, width / 2 + shoulder, best)
    target = np.where(wide, c.heights * (1 - blend) + line_h * blend, c.heights)
    free = wide & ~c.locked
    c.heights[free] = half(target[free])
    c.heights[mask] = half(line_h[mask])
    c.paint(mask, ch)
    return mask


def city_wall(c: Canvas, city):
    """Old stone curtain wall three tiles thick with towers at its corners, and three gates."""
    inner = city.copy()
    for _ in range(3):
        shrunk = inner.copy()
        shrunk[1:, :] &= inner[:-1, :]
        shrunk[:-1, :] &= inner[1:, :]
        shrunk[:, 1:] &= inner[:, :-1]
        shrunk[:, :-1] &= inner[:, 1:]
        inner = shrunk
    c.paint(city & ~inner, '#', 0.0)
    c.paint(inner, '_', 0.0)
    for x, y in CITY_WALL:
        c.rect(x - 4, y - 4, 9, 9, '#', 0.0)
    for (gx, gy), (w, h) in ((MAIN_GATE, (12, 5)), (SOUTH_GATE, (5, 12)), (STAIR_GATE, (5, 12))):
        c.rect(gx - w // 2, gy - h // 2, w, h, 'G', 0.0)
    return inner


def campuses(c: Canvas):
    x, y, w, h, level = CONCORD
    c.rect(x, y, w, h, 'm', level)
    c.outline(x, y, w, h, 'M', level, thickness=2)
    c.rect(x + w // 2 + 32 - 3, y + h - 2, 6, 2, 'G', level)       # South gate, at the head of the stair.
    c.rect(x + w - 2, y + 44, 2, 6, 'G', level)                    # East gate to the trail.
    x, y, w, h, level = TRAINING
    c.rect(x, y, w, h, 'd', level)
    c.outline(x, y, w, h, '|', level)
    c.rect(x + 20, y, 6, 1, 'G', level)                            # North gate to the Concord trail.
    c.rect(x + 20, y + h - 1, 6, 1, 'G', level)                    # South gate to the Order trail.
    x, y, w, h, level = ORDER
    c.rect(x, y, w, h, 'f', level)
    c.outline(x, y, w, h, '#', level, thickness=2)
    c.rect(x + 60, y, 6, 2, 'G', level)                            # North gate: the switchback path.
    c.rect(x + w - 2, y + 44, 2, 6, 'G', level)                    # East gate to the trail.


def ground_cover(c: Canvas):
    """Grass, forest, rock, scree and snow by height and steepness; cliffs wherever a drop cannot be walked."""
    h = c.heights
    free = ~c.locked
    n1 = field.fbm(SIZE, SIZE, 24, SEED + 11, 3)
    n2 = field.fbm(SIZE, SIZE, 9, SEED + 12, 2)
    grain = np.random.default_rng(SEED).random(h.shape)
    rise = np.zeros_like(h)
    rise[:, :-1] = np.maximum(rise[:, :-1], np.abs(h[:, 1:] - h[:, :-1]))
    rise[:, 1:] = np.maximum(rise[:, 1:], np.abs(h[:, 1:] - h[:, :-1]))
    rise[:-1, :] = np.maximum(rise[:-1, :], np.abs(h[1:, :] - h[:-1, :]))
    rise[1:, :] = np.maximum(rise[1:, :], np.abs(h[1:, :] - h[:-1, :]))

    def put(mask, ch):
        c.codes[mask & free] = ord(ch)
    put(np.ones_like(free), ',')
    put((h < -3) & (n2 > .6), '"')
    put((h >= 4) & (n1 > .45), 'r')
    put(h >= 9, 'r')
    put((h >= 9) & (n2 > .62), 's')
    put((h >= 13) & (n1 > .4), '*')
    put(h >= 15, '*')
    put((h >= -3) & (h < 9) & (n1 < .42) & (grain < .55), 'P')            # Pine forest on the middle slopes.
    put((h < -3) & (n1 > .62) & (grain < .35), 'Y')                         # Broadleaf copses in the foothills.
    put((h >= -3) & (h < 9) & (grain > .985), 'B')
    put((h >= 2) & (grain < .012), 'o')
    put((h >= 6) & (rise >= 1) & (n2 > .45), 's')
    # A drop of more than one step is a cliff face on its high side.
    higher = np.zeros_like(free)
    higher[:, :-1] |= h[:, :-1] - h[:, 1:] > 1
    higher[:, 1:] |= h[:, 1:] - h[:, :-1] > 1
    higher[:-1, :] |= h[:-1, :] - h[1:, :] > 1
    higher[1:, :] |= h[1:, :] - h[:-1, :] > 1
    put(higher, '%')


def mountain_tarn(c: Canvas):
    xs, ys = field.grid(SIZE, SIZE)
    for cx, cy, r in ((150, 120, 9), (600, 180, 7)):
        mask = field.bump(xs, ys, cx, cy, r, r * .7, power=2) > .5
        level = float(np.median(c.heights[mask]))
        c.paint(mask, '~', level)
        rim = (field.bump(xs, ys, cx, cy, r + 5, r * .7 + 5, power=2) > .5) & ~mask & ~c.locked
        c.heights[rim] = level


def build():
    c = Canvas(SIZE, SIZE)
    city = terrain(c)
    mountain_tarn(c)
    campuses(c)
    inner = city_wall(c, city)
    road(c, MAIN_ROAD, 5, 'd', end_h=0.0)
    road(c, STAIR, 4, '^', start_h=0.0, end_h=CONCORD[4], shoulder=2)
    road(c, SWITCHBACK, 3, 'd', start_h=0.0, end_h=ORDER[4])
    road(c, CONCORD_TRAIL, 2, 'd', start_h=CONCORD[4], end_h=TRAINING[4])
    road(c, ORDER_TRAIL, 2, 'd', start_h=ORDER[4], end_h=TRAINING[4])
    ground_cover(c)
    site = Site(c, [(cid, x, y, w, h) for cid, _, x, y, w, h, _ in CELLS])
    placed, _ = build_city(site, inner, SEED)
    campus_plans.concord(site, SEED, CONCORD)
    campus_plans.training(site, SEED, TRAINING)
    campus_plans.order(site, SEED, ORDER)
    campus_plans.decorate(site, SEED, CONCORD, TRAINING, ORDER)
    return c, site, {'city': city, 'inner': inner, 'placed': placed}


def cells_json(c: Canvas):
    out = []
    for cid, name, x, y, w, h, description in CELLS:
        rows, heights = c.cell(x, y, w, h)
        region = 'upper_accord' if cid in ('upper_accord', 'concord_hall', 'training_grounds', 'warden_order') \
            else 'accord_peaks'
        out.append({'id': cid, 'name': name, 'description': description, 'x': x, 'y': y, 'width': w, 'height': h,
                    'z': 0, 'outdoors': True, 'weather': 'clear',
                    'territory': {'region': region, 'claims': [], 'chapter': ''}, 'terrain': rows, 'heights': heights})
    return out


class MergeError(ValueError):
    pass


def generated_project():
    c, site, parts = build()
    return c, site, parts, {
        'format': 'ratw-atlas', 'version': 3, 'id': 'upper_accord', 'name': 'Upper Accord',
        'cells': cells_json(c), 'rooms': site.rooms, 'links': site.links,
        'spawn': {'cell': 'upper_accord', 'x': PLAZA_SPAWN[0] - 256, 'y': PLAZA_SPAWN[1] - 256}}


def merge(existing: dict, region: dict, offset=(0, 0), replace=False) -> dict:
    """The existing world with the region added (shifted by `offset`) and its spawn moved into the region."""
    ours = {a['id'] for a in region['cells'] + region['rooms']}
    ours_links = {link['id'] for link in region['links']}
    theirs = existing['cells'] + existing['rooms']
    clash = sorted(a['id'] for a in theirs if a['id'] in ours)
    if clash and not replace:
        raise MergeError(f'The world already has Upper Accord places ({", ".join(clash[:5])}...); '
                         'use --replace to swap in a fresh generation (Atlas edits to them would be lost).')
    ox, oy = offset
    cells = [{**c, 'x': c['x'] + ox, 'y': c['y'] + oy} for c in region['cells']]
    rooms = [{**r, 'worldX': r['worldX'] + ox, 'worldY': r['worldY'] + oy} for r in region['rooms']]
    kept_cells = [c for c in existing['cells'] if c['id'] not in ours]
    for a in cells:
        for b in kept_cells:
            if a['x'] < b['x'] + b['width'] and b['x'] < a['x'] + a['width'] and \
                    a['y'] < b['y'] + b['height'] and b['y'] < a['y'] + a['height']:
                raise MergeError(f'{a["id"]} would overlap the existing cell {b["id"]}; choose another --offset.')
    merged = dict(existing)
    merged['cells'] = kept_cells + cells
    merged['rooms'] = [r for r in existing['rooms'] if r['id'] not in ours] + rooms
    merged['links'] = [link for link in existing['links']
                       if link['id'] not in ours_links and link['a']['cell'] not in ours and link['b']['cell'] not in ours] \
        + region['links']
    merged['spawn'] = region['spawn']
    return merged


def import_dev(project, offset, replace, conn=None, log=print, replace_world=False, backups=None):
    import world_db
    import world_store
    own = conn is None
    conn = conn or world_db.connect('dev', 'editor')
    try:
        worlds = world_store.list_worlds(conn)
        if worlds and replace_world:
            old = worlds[0]['id']
            existing, revision = world_store.load_world(conn, old)
            folder = backups or Path(__file__).resolve().parents[2] / 'artifacts/backups'
            folder.mkdir(parents=True, exist_ok=True)
            import time
            backup = folder / f'{old}_dev_r{revision}_{time.strftime("%Y%m%d-%H%M%S")}.atlas.json'
            backup.write_text(json.dumps(existing, ensure_ascii=False), encoding='utf-8')
            with conn.transaction():
                conn.execute('DELETE FROM world.worlds WHERE id = %s', (old,))
                new_revision = world_store.save_world(conn, project, None, create=True)
            log(f'Replaced {old} with {project["id"]} in DEV (revision {new_revision}); the old world is saved in '
                f'{backup}.')
            return project['id']
        if not worlds:
            revision = world_store.save_world(conn, project, None, create=True)
            log(f'Created the world {project["id"]} in DEV, revision {revision}.')
            return project['id']
        wid = worlds[0]['id']
        existing, revision = world_store.load_world(conn, wid)
        revision = world_store.save_world(conn, merge(existing, project, offset, replace), revision)
        log(f'Added Upper Accord to {wid} in DEV (revision {revision}); new characters start in its plaza.')
        return wid
    finally:
        if own:
            conn.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--preview', type=Path, help='write preview images here')
    parser.add_argument('--out', type=Path, help='write the atlas here')
    parser.add_argument('--import-dev', action='store_true', help='add the region to the DEV world')
    parser.add_argument('--replace', action='store_true', help='replace an earlier import of the region')
    parser.add_argument('--replace-world', action='store_true',
                        help='make the region the whole DEV world (backs up and deletes the world there)')
    parser.add_argument('--offset', type=int, nargs=2, default=(1024, 0), metavar=('X', 'Y'),
                        help='where the region goes in the world, in tiles (default 1024 0)')
    args = parser.parse_args(argv)
    c, site, parts, project = generated_project()
    walk = reachable(c.codes, c.heights, (MAIN_ROAD[0][0] + 1, MAIN_ROAD[0][1]))
    for label, (x, y) in {'Main Gate': MAIN_GATE, 'Concord Hall': (CONCORD[0] + 50, CONCORD[1] + 50),
                          'Training Grounds': (TRAINING[0] + 50, TRAINING[1] + 100),
                          'Warden Order': (ORDER[0] + 50, ORDER[1] + 50)}.items():
        print(f'{label:>18}: {"reachable" if walk[y, x] else "NOT REACHABLE"} from the western road')
    print(f'walkable share: {walk.mean():.0%}; heights {c.heights.min()}..{c.heights.max()}')
    from .site import STEP
    stranded = [r['id'] for r in site.manifest
                if not walk[r['door']['y'] + STEP[r['facing']][1], r['door']['x'] + STEP[r['facing']][0]]]
    print(f'doors reachable from the western road: {len(site.manifest) - len(stranded)}/{len(site.manifest)}'
          + (f'; stranded: {", ".join(stranded)}' if stranded else ''))
    kinds = {}
    for record in site.manifest:
        kinds[record['kind']] = kinds.get(record['kind'], 0) + 1
    print(f'{len(site.manifest)} buildings, {len(site.rooms)} interiors, {len(site.links)} links: '
          + ', '.join(f'{k} {n}' for k, n in sorted(kinds.items())))
    import map_editor
    try:
        map_editor.check_project(project, for_game=False)
        print('Atlas validation: OK')
    except map_editor.ValidationError as error:
        print('Atlas validation failed:')
        for e in error.errors[:40]:
            print('  ', e)
        return 1
    if args.preview:
        args.preview.mkdir(parents=True, exist_ok=True)
        preview.overview(c.codes, c.heights, [{'x': x, 'y': y, 'width': w, 'height': h, 'name': n}
                                              for _, n, x, y, w, h, _ in CELLS], args.preview / 'overview.png')
        reach = np.where(walk[..., None], [90, 200, 90], [200, 60, 60]).astype(np.uint8)
        from PIL import Image
        Image.fromarray(reach).save(args.preview / 'reachable.png')
    if args.out:
        args.out.mkdir(parents=True, exist_ok=True)
        (args.out / 'upper_accord.atlas.json').write_text(json.dumps(project, ensure_ascii=False), encoding='utf-8')
        (args.out / 'buildings.json').write_text(json.dumps(site.manifest, indent=1, ensure_ascii=False),
                                                 encoding='utf-8')
    if args.import_dev:
        try:
            import_dev(project, tuple(args.offset), args.replace, replace_world=args.replace_world)
        except MergeError as error:
            print(error)
            return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
