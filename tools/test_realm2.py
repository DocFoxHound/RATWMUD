#!/usr/bin/env python3
"""Ridgemere 2.0's realm (Docs/Design/70-the-realm.md): the generator's pieces, and (RATW_SLOW=1) the whole realm laid
round the city as DEV holds it, with every check of the write.

  python3 tools/test_realm2.py                 the pieces (seconds)
  RATW_SLOW=1 python3 tools/test_realm2.py     and the realm round DEV's city (about two and a half minutes, 8 GB; read only;
                                               skipped without the database)
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


@unittest.skipUnless(os.environ.get('RATW_SLOW'), 'the whole realm: RATW_SLOW=1')
class WholeRealm(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
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

    def test_ridgemere_is_the_north_west_corner(self):
        xs = [c['x'] for c in self.project['cells']]
        ys = [c['y'] for c in self.project['cells']]
        self.assertEqual((min(xs), min(ys)), (0, 0))
        city = [c for c in self.project['cells'] if c['territory']['region'] == 'ridgemere']
        self.assertEqual(sorted((c['x'], c['y']) for c in city), [(x, y) for x in (0, 256, 512) for y in (0, 256, 512)])


if __name__ == '__main__':
    unittest.main()
