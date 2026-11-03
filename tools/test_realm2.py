#!/usr/bin/env python3
"""Ridgemere 2.0's realm (Docs/Design/70-the-realm.md): the generator's pieces, and (RATW_SLOW=1) the whole realm laid
round the city as DEV holds it, with every check of the write.

  python3 tools/test_realm2.py                 the pieces (seconds)
  RATW_SLOW=1 python3 tools/test_realm2.py     and the realm's sites round the kept city snapshot (two minutes, no
                                               database), and the realm round DEV's city (about two and a half minutes,
                                               8 GB; read only; skipped without the database)
  RATW_REALM_PROJECT=FILE                      the world from an Atlas export (a write's backup) instead of DEV
"""
from __future__ import annotations

import json
import os
import random
import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))

from worldgen import realm2 as R  # noqa: E402
from worldgen import realm2_places as P  # noqa: E402
from worldgen.canvas import walkable_steps, reachable  # noqa: E402

import terrain_catalog  # noqa: E402


def flood_labels(mask):
    """Plain 4-way flood fill labels (the slow, obvious way), for comparison."""
    H, W = mask.shape
    lab = np.full((H, W), -1, dtype=np.int64)
    n = 0
    for y0 in range(H):
        for x0 in range(W):
            if mask[y0, x0] and lab[y0, x0] < 0:
                todo = [(x0, y0)]
                lab[y0, x0] = n
                while todo:
                    x, y = todo.pop()
                    for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                        if 0 <= nx < W and 0 <= ny < H and mask[ny, nx] and lab[ny, nx] < 0:
                            lab[ny, nx] = n
                            todo.append((nx, ny))
                n += 1
    return lab


def same_partition(a, b):
    pairs = set(zip(a.ravel().tolist(), b.ravel().tolist()))
    return len(pairs) == len(set(a.ravel().tolist())) == len(set(b.ravel().tolist()))


class Pieces(unittest.TestCase):
    def test_components_match_a_flood_fill(self):
        rng = np.random.default_rng(1)
        for density in (.45, .55, .62, .75):
            mask = rng.random((60, 70)) < density
            self.assertTrue(same_partition(R.components4(mask), flood_labels(mask)))

    def test_label_runs_follows_the_games_step_rule(self):
        rng = np.random.default_rng(2)
        codes = np.where(rng.random((40, 40)) < .2, ord('#'), ord(',')).astype(np.uint8)
        heights = np.round(np.cumsum(rng.normal(0, .35, (40, 40)), axis=1) * 2) / 2
        east, south = walkable_steps(codes, heights)
        lab = R.label_runs(~R.SOLID[codes], east, south)
        start = tuple(int(v) for v in np.argwhere(~R.SOLID[codes])[0][::-1])
        seen = reachable(codes, heights, start)
        self.assertTrue(np.array_equal(seen, lab == lab[start[1], start[0]]))

    def test_the_flood_drains_every_node_downhill_to_an_outlet(self):
        rng = np.random.default_rng(3)
        e = (rng.random((48, 48)) * 5).astype(np.float32)
        seeds = np.zeros(e.shape, dtype=bool)
        seeds[:, 0] = True
        filled, _ = R.priority_flood(e, seeds)
        self.assertTrue((filled >= e - 1e-6).all())
        down = R.steepest(filled, seeds)
        for i in range(e.size):
            steps, j = 0, i
            while down.ravel()[j] >= 0:
                k = down.ravel()[j]
                self.assertLess(filled.ravel()[k], filled.ravel()[j])
                j = k
                steps += 1
                self.assertLess(steps, e.size)
            self.assertTrue(seeds.ravel()[j])

    def test_limit_slope_leaves_no_steep_step_and_keeps_fixed_tiles(self):
        rng = np.random.default_rng(4)
        h = (rng.random((50, 50)) * 8).astype(np.float32)
        fixed = np.zeros(h.shape, dtype=bool)
        fixed[20:25, 20:25] = True
        h[fixed] = 3.0
        before = h.copy()
        R.limit_slope(h, np.full(h.shape, .45, dtype=np.float32), fixed, rounds=4)
        self.assertTrue(np.array_equal(h[fixed], before[fixed]))
        self.assertLessEqual(float(np.abs(np.diff(h, axis=0)).max()), .45 + 1e-5)
        self.assertLessEqual(float(np.abs(np.diff(h, axis=1)).max()), .45 + 1e-5)

    def test_up_and_down_keep_a_field(self):
        a = np.arange(64, dtype=np.float32).reshape(8, 8) / 8
        inner = np.s_[1:-1, 1:-1]                       # (at the edges bilinear holds the last value)
        self.assertTrue(np.allclose(R.down(R.up(a))[inner], a[inner], atol=.05))

    def test_the_new_tiles_are_in_the_catalog(self):
        for code, name in (('(', 'Sandstone'), (')', 'Salt flat'), ('?', 'Cactus'), ('@', 'Willow')):
            self.assertEqual(terrain_catalog.BY_CODE[code]['name'], name)
        self.assertTrue(terrain_catalog.BY_CODE['?']['solid'] and terrain_catalog.BY_CODE['@']['solid'])
        self.assertFalse(terrain_catalog.BY_CODE['('].get('solid') or terrain_catalog.BY_CODE[')'].get('solid'))

    def test_the_new_tiles_are_in_the_wild_tables(self):
        # Hunting's ground kinds and foraging's grounds are keyed by tile (Data/Wild): the realm's new tiles are in them,
        # willow as woods, cactus as scrub, sandstone as rock, a salt flat as barren as sand.
        wild = Path(__file__).resolve().parents[1] / 'Data' / 'Wild'
        ground = json.loads((wild / 'animals.json').read_text(encoding='utf-8'))['ground']
        kind = {c: k for k, tiles in ground.items() for c in tiles}
        self.assertEqual((kind.get('@'), kind.get('?'), kind.get('('), kind.get(')')), ('forest', 'shrub', 'rock', 'sand'))
        forage = {c: g['id'] for g in json.loads((wild / 'forage.json').read_text(encoding='utf-8'))['ground'] for c in g['tiles']}
        self.assertEqual((forage.get('@'), forage.get('?'), forage.get(')')), ('willow', 'shrub', 'salt'))
        # Every tile the realm paints that is natural ground (not water, a road or the city's) has its hunting kind.
        painted = set(''.join(''.join(p[0]) + p[1] for p in R.PALETTES.values()))
        for a, b, _, trees, _, extras in R.CALM.values():
            painted |= set(a) | set(b) | set(trees) | {c for c, _ in extras}
        missing = sorted(c for c in painted if c not in kind and c not in '.')
        self.assertEqual(missing, [])

    def test_ridged_noise_and_the_range_carver(self):
        ys, xs = np.mgrid[0:64, 0:64].astype(np.float32)
        a = R.ridges_at(xs * 8, ys * 8, 120, 5, 4)
        self.assertTrue(np.array_equal(a, R.ridges_at(xs * 8, ys * 8, 120, 5, 4)))       # (deterministic)
        self.assertTrue(0 <= float(a.min()) and float(a.max()) <= 1)
        # A cone of a range on flat ground: carved, it keeps its foot, rises highest inside, and is cut into valleys
        # (more relief than the smooth cone's) without any node falling below the land it stands on.
        plan = np.clip(10 - np.hypot(xs - 32, ys - 32) / 2.5, 0, None).astype(np.float32)
        foot = np.full(plan.shape, 2.0, dtype=np.float32)
        sea = np.zeros(plan.shape, dtype=bool)
        carved = R.carve_range(foot, plan, sea, xs * 8, ys * 8, 7)
        self.assertTrue((carved >= foot - 1e-5).all())
        self.assertTrue(np.allclose(carved[plan == 0], 2.0))
        self.assertGreater(float(carved[24:40, 24:40].mean()), float(carved[plan < .5].mean()) + 3)
        rough = lambda h: float(np.abs(np.diff(h[16:48, 16:48], axis=1)).mean())
        self.assertGreater(rough(carved), rough(foot + plan))

    def test_the_plan(self):
        ids = [s[0] for s in R.SITES]
        self.assertEqual(len(ids), len(set(ids)))
        towns = [s for s in R.SITES if s[1] == 'town']
        self.assertTrue(5 <= len(towns) <= 7)
        for s in R.SITES:
            self.assertIn(s[1], R.GROUND_OF)
            if isinstance(s[3], str):
                self.assertLess(ids.index(s[3]), ids.index(s[0]))      # (placed after the site it is near)
        for name, a, b, _ in R.ROADS:
            for end in (a, b):
                self.assertTrue(isinstance(end, tuple) or end in R.GATE_ROADS or end in ids, (name, end))
        text = ' '.join(' '.join(v) for v in R.REGION_TEXT.values()) + ' '.join(s[7] + s[2] for s in R.SITES)
        self.assertNotRegex(text.lower(), r'\bdogs?\b')


class Places(unittest.TestCase):
    """The farms' steadings and the communities (realm2_places): the plan, the interiors, the compounds' layout."""

    def test_every_farm_of_doc_71_has_a_site_and_a_steading(self):
        cf = json.loads((Path(__file__).resolve().parent.parent / 'Data/Economy/country_farms.json').read_text())
        farms = [f['id'] for a in cf['areas'] for f in a['farms']]
        self.assertEqual(sorted(farms), sorted(P.FARMS))
        sites = {s[0] for s in R.SITES}
        self.assertTrue(set(P.FARMS.values()) <= sites)
        self.assertEqual(len(set(P.FARMS.values())), len(P.FARMS))
        self.assertEqual(set(P.STEADINGS), set(P.FARMS.values()))
        self.assertTrue(set(P.MILLS.values()) <= sites)
        for stem, kind, style, community in P.STEADINGS.values():
            self.assertIn(kind, P.STEADING_KINDS)
            self.assertIn(style, ('country', 'southern', 'adobe', 'fen'))
            self.assertIn(community, {c['id'] for c in P.COMMUNITIES} | {'ridgemere'})

    def test_the_hands_live_where_the_user_put_them(self):
        cf = json.loads((Path(__file__).resolve().parent.parent / 'Data/Economy/country_farms.json').read_text())
        hands = {}
        for a in cf['areas']:
            for f in a['farms']:
                com = P.STEADINGS[P.FARMS[f['id']]][3]
                hands[com] = hands.get(com, 0) + f['crew']['hands']
        self.assertEqual(hands, {'coldwater': 21, 'larkfield': 11, 'southmeadow': 9, 'fenshore': 8, 'saltwell': 6,
                                 'ridgemere': 23})

    def test_the_interiors(self):
        rng = random.Random(5)
        for style in ('country', 'southern', 'adobe', 'fen'):
            home = P.farmhouse(rng, style, 'A Farmhouse')
            self.assertEqual(len(home.rooms[0].beds), P.HOUSEHOLD_MOST)
            self.assertEqual(len(P.cottage(rng, style, 'A Cottage').rooms[0].beds), P.HOUSEHOLD_MOST)
            for kind, items in P.STEADING_KINDS.items():
                for key, fn, role, word in items:
                    b = fn(rng, style, f'A {word}')
                    self.assertTrue(b.rooms[0].work, (kind, key))
                    P.more_work(b.rooms[0], 6)
                    work = b.rooms[0].work
                    self.assertGreaterEqual(len(work), 5, (kind, key))
                    rows = b.rooms[0].rows
                    self.assertTrue(all(rows[y][x] in '.f' for x, y in work), (kind, key))
                    self.assertEqual(len(set(map(tuple, work))), len(work))
        press = P.press_house(rng, 'adobe', 'A Press House')
        self.assertEqual([r.z for r in press.rooms], [0, -1])
        self.assertEqual(len(press.stairs), 1)

    def test_each_compound_opens_every_door_on_its_yard_however_it_is_turned(self):
        from worldgen.site import door_tile
        rng = random.Random(1)
        for kind, items in P.STEADING_KINDS.items():
            builds = [P.farmhouse(rng, 'country', 'X House')] + [fn(rng, 'country', f'X {w}') for _, fn, _, w in items]
            layout = P.steading_layout(builds)
            for k in range(4):
                (W, H), parts = P.turned_layout(layout, k)
                _, yx, yy, yw, yh, _ = next(p for p in parts if p[0] == 'yard')
                rects = []
                for name, x, y, w, h, f in parts:
                    self.assertTrue(0 <= x and x + w <= W and 0 <= y and y + h <= H, (kind, k, name))
                    if isinstance(name, int):
                        _, (ox, oy) = door_tile(x, y, w, h, f)
                        self.assertTrue(yx <= ox < yx + yw and yy <= oy < yy + yh, (kind, k, name))
                    if name != 'gate':
                        rects.append((x, y, w, h))
                for i, a in enumerate(rects):
                    for b in rects[i + 1:]:
                        self.assertFalse(a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and
                                         b[1] < a[1] + a[3], (kind, k, a, b))
            self.assertEqual(P.turned_layout(layout, 4), P.turned_layout(layout, 0))

    def test_the_words(self):
        text = json.dumps([P.COMMUNITIES, P.FENSHORE, P.COTTAGE_PROSE, P.FARMHOUSE_PROSE, P.MAP_WINS])
        self.assertNotRegex(text.lower(), r'\bdogs?\b')


@unittest.skipUnless(os.environ.get('RATW_SLOW'), 'the realm\'s sites: RATW_SLOW=1')
class RealmSites(unittest.TestCase):
    """The realm laid round the kept city snapshot as far as its sites (no database: about two minutes), each site's
    preparing watched: what it changed of the ground and heights."""
    @classmethod
    def setUpClass(cls):
        import gzip
        if not R.SNAPSHOT.exists():
            raise unittest.SkipTest(f'no city snapshot at {R.SNAPSHOT}')
        cells = json.loads(gzip.decompress(R.SNAPSHOT.read_bytes()).decode('utf-8'))
        codes, heights = R.city_canvas(cells)
        realm = R.Realm2(codes, heights, R.SEED, report=lambda *a: None)
        cls.changed = {}
        prepare = R.Realm2.prepare

        def watched(self, site):
            codes, h = self.codes.copy(), self.h.copy()
            prepare(self, site)
            cls.changed[site['id']] = np.flatnonzero((codes != self.codes) | (h != self.h))
        R.Realm2.prepare = watched
        try:
            for step in ('lay_city', 'lay_sea', 'lay_elevation', 'lay_climate', 'lay_drainage', 'lay_heights',
                         'lay_water', 'settle', 'lay_ground', 'lay_sites'):
                getattr(realm, step)()
        finally:
            R.Realm2.prepare = prepare
        cls.realm = realm

    @classmethod
    def tearDownClass(cls):
        del cls.realm, cls.changed                  # (before the whole realm is laid)

    def test_every_site_finds_its_ground(self):
        placed = {s['id'] for s in self.realm.sites}
        self.assertEqual([s[0] for s in R.SITES if s[0] not in placed], [])

    def test_no_site_is_laid_over_another(self):
        # Each site's ground is its own (the review, doc 70): a grain farm laid over Larkfield drew hedges across the
        # town's levelled core, and both counted the tiles they shared.
        by = np.zeros(R.SIZE * R.SIZE, dtype=np.int16)
        shared = []
        for i, site in enumerate(self.realm.sites, 1):
            tiles = self.changed[site['id']]
            for j in np.unique(by[tiles]):
                if j:
                    shared.append((self.realm.sites[j - 1]['id'], site['id'], int((by[tiles] == j).sum())))
            by[tiles] = i
        self.assertEqual(shared, [])
        self.assertEqual([s['id'] for s in self.realm.sites if s.get('shared')], [])
        for i, site in enumerate(self.realm.sites, 1):
            self.assertEqual(site['tiles'], int((self.realm.ground_of == i).sum()), site['id'])

    def test_the_growing_sites_are_on_growing_ground(self):
        # A farm, orchard, vineyard or dairy grows on soil: the Ochre vineyard was laid on two-thirds sand, sandstone
        # and rubble at the desert's edge. Through the middle of each (its own ground for sure), barely any.
        bare = np.isin(self.realm.codes, [ord(c) for c in '(0X)rs'])     # (sandstone, sand, rubble, salt, rock, scree)
        for s in self.realm.sites:
            if s['kind'] not in ('farmland', 'orchard', 'vineyard', 'dairy'):
                continue
            x, y, size = s['x'], s['y'], s['size']
            box = np.s_[max(0, y - size):y + size, max(0, x - size):x + size]
            core = np.hypot((self.realm.X[box] - x) / size, (self.realm.Y[box] - y) / (size * .75)) < .8
            core &= ~self.realm.water[box]
            share = float(bare[box][core].mean())
            self.assertLess(share, .02, (s['id'], round(share, 3)))


@unittest.skipUnless(os.environ.get('RATW_SLOW'), 'the whole realm: RATW_SLOW=1')
class WholeRealm(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            if os.environ.get('RATW_REALM_PROJECT'):
                cls.project = json.loads(Path(os.environ['RATW_REALM_PROJECT']).read_text(encoding='utf-8'))
            else:
                from worldgen import ridgemere2 as r2
                cls.project, _, _ = r2.load('dev')
        except Exception as error:                  # (no database here)
            raise unittest.SkipTest(f'DEV not readable: {error}')
        # Laying the realm (as a preview or a dry run does) never touches the city's snapshot: only a write that saved.
        cls.snapshots = []
        keep, R.keep_snapshot = R.keep_snapshot, lambda cells: cls.snapshots.append(cells)
        try:
            cls.realm = R.expand(cls.project, report=lambda *a: None)
        finally:
            R.keep_snapshot = keep

    def test_every_check_passes(self):
        failed = [c for c in self.realm.result if not c['ok']]
        self.assertEqual(failed, [])
        checks = {c['check']: c for c in self.realm.result}
        self.assertTrue(checks['no river runs uphill']['detail'].startswith('0 rises'))
        self.assertTrue(checks['every kept tile of the city as it was (code and height)']['detail'].startswith('0 codes, 0 heights'))

    def test_laying_the_realm_keeps_no_snapshot(self):
        self.assertEqual(self.snapshots, [])

    def test_every_river_runs_down_through_its_lakes(self):
        # The strict check, again here: tile by tile from source to mouth, water and lakes, never a rise.
        r = self.realm
        water = (r.river | r.lake | r.sea | r.creek | r.pool) & ~r.keep & (r.codes != ord('8'))
        for line in r.river_lines:
            if line['dry']:
                continue
            last = None
            for x, y in line['course']:
                xi, yi = min(R.SIZE - 1, int(x)), min(R.SIZE - 1, int(y))
                if not water[yi, xi]:
                    continue
                v = float(r.h[yi, xi])
                self.assertFalse(last is not None and v > last, (xi, yi, v, last))
                last = v if last is None else min(last, v)
                if r.sea[yi, xi]:
                    break

    def test_the_farms_and_works_doc_71_asked_for(self):
        sites = R.report_of(self.realm, self.project)['siteList']
        grain = [s for s in sites if s['kind'] == 'farmland']
        self.assertEqual(len(grain), 2)
        for s in grain:                              # (about 363 x 363 tiles, the user's choice; less any water in it)
            self.assertTrue(110_000 <= s['tiles'] <= 135_000, s)
            self.assertGreaterEqual(s.get('fields', 0), 6)
        for kind in ('vineyard', 'salt|saltcoast'):
            real = [s for s in sites if s['kind'] in kind.split('|') and not s['for'].startswith('spare')]
            self.assertEqual(len(real), 2, kind)

    def test_the_desert_edge_is_dry_country(self):
        r = self.realm
        dry = r.dry_edge > .4
        for c in '15P$*J-':                          # (no heather, pine, cedar or snow at the desert's edge)
            self.assertEqual(int((r.codes[dry] == ord(c)).sum()), 0, c)
        self.assertGreater(int((r.codes[dry] == ord('(')).sum()), int(dry.sum()) // 10)   # (red rock showing)
        cells = {c['id']: c for c in self.project['cells']}
        ochre = [c for c in cells.values() if c['territory']['region'] == 'ochre_hills']
        self.assertGreater(len(ochre), 3)
        self.assertTrue(all(c['weather'] == 'sandstorm' for c in ochre))
        for c in ochre:
            self.assertNotRegex(c['description'].lower(), r'heather|pine|rain(?! seldom)')

    def test_round_the_city_the_rain_forest(self):
        r = self.realm
        # Outside the walls, beyond the cleared ground by the city, the woods are fir and cedar.
        box = np.s_[:R.CITY, :R.CITY]
        wild = r.city_wild & (r.from_city[box] > 40)
        trees = r.codes[box][wild]
        conifer = int(np.isin(trees, [ord('$'), ord('P')]).sum())
        self.assertGreater(conifer, int(wild.sum()) // 5)
        self.assertGreater(conifer, 3 * int(np.isin(trees, [ord('Y'), ord('B')]).sum()))

    def test_the_regions_the_user_asked_for(self):
        rep = R.report_of(self.realm, self.project)
        for region in ('whitefangs', 'greyteeth', 'greenholt', 'fens', 'waste', 'rainwood'):
            self.assertGreater(rep['regions'].get(region, 0), 3, region)
        self.assertGreater(len([l for l in rep['lakes'] if l['tiles'] > 20000]), 1)
        self.assertTrue(any(r['name'] == 'the Long River' for r in rep['rivers']))
        # About 14 cells of fields and range (doc 71's food plan, the user's "somewhere in the middle").
        res = rep['reserved']
        self.assertTrue(5.5 <= res['ownFields'] / res['cellTiles'] <= 8)
        self.assertTrue(6.5 <= res['range'] / res['cellTiles'] <= 9)

    def test_the_steadings_and_communities(self):
        c = self.realm.contract
        rooms = {r['id']: r for r in self.project['rooms']}
        links = {l['id'] for l in self.project['links']}
        self.assertEqual(len(c['farms']), 31)
        for f in c['farms']:
            self.assertIn(f['workplace'], rooms, f['farm'])
            self.assertIn(f['home']['area'], rooms, f['farm'])
            self.assertIn(f'{f["home"]["area"]}_door', links)
            self.assertEqual(rooms[f['workplace']]['territory']['region'], f['community'])
            self.assertGreaterEqual(sum(len(w['work']) for w in f['workplaces']), f['keepers'] + f['hands'], f['farm'])
            for bed in f['home']['beds']:
                self.assertEqual(rooms[f['home']['area']]['terrain'][bed['y']][bed['x']], 'b')
        beds = {com['id']: com['beds'] for com in c['communities']}
        self.assertTrue(55 <= beds['coldwater'] <= 65, beds)
        self.assertTrue(28 <= beds['larkfield'] <= 34, beds)
        for k in ('southmeadow', 'fenshore', 'saltwell'):
            self.assertTrue(20 <= beds[k] <= 25, beds)
        for com in c['communities']:
            self.assertGreaterEqual(com['roomToGrow']['cottageLots'], 3, com['id'])
            self.assertTrue(any(g['kind'] == 'kitchen garden' for g in com['gardens']))
            self.assertEqual({g['kind'] for g in com['gardens']}, {'kitchen garden', 'hen run', 'goat paddock'})
            self.assertIn('general', [m['business'] for m in com['market']])
            self.assertIn('inn', [m['business'] for m in com['market']])
            for m in com['market']:
                self.assertGreaterEqual(len(m['work']), sum(p['count'] for p in m['posts']), m['area'])
        # Oakhaven, Frosthollow and Mirebridge stay reserved, unbuilt.
        built = {r['id'].split('_')[1] for r in self.project['rooms'] if r['id'].startswith('cn_')}
        self.assertEqual(built, {'coldwater', 'larkfield', 'southmeadow', 'fenshore', 'saltwell'})

    def test_the_contract_written_is_the_one_made(self):
        # Data/Economy/communities.json is what the last write made; the generator makes the same again.
        path = P.CONTRACT
        if not path.exists():
            self.skipTest('no contract written yet')
        self.assertEqual(json.loads(path.read_text(encoding='utf-8')), json.loads(json.dumps(self.realm.contract)))

    def test_ridgemere_is_the_north_west_corner(self):
        xs = [c['x'] for c in self.project['cells']]
        ys = [c['y'] for c in self.project['cells']]
        self.assertEqual((min(xs), min(ys)), (0, 0))
        city = [c for c in self.project['cells'] if c['territory']['region'] == 'ridgemere']
        self.assertEqual(sorted((c['x'], c['y']) for c in city), [(x, y) for x in (0, 256, 512) for y in (0, 256, 512)])


if __name__ == '__main__':
    unittest.main()
