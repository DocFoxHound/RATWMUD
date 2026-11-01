#!/usr/bin/env python3
"""Ridgemere 2.0's realm (Docs/Design/70-the-realm.md): the generator's pieces, and (RATW_SLOW=1) the whole realm laid
round the city as DEV holds it, with every check of the write.

  python3 tools/test_realm2.py                 the pieces (seconds)
  RATW_SLOW=1 python3 tools/test_realm2.py     and the realm round DEV's city (about two minutes, 6 GB; read only;
                                               skipped without the database)
"""
from __future__ import annotations

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
            cls.project, _ = r2.load('dev')
        except Exception as error:                  # (no database here)
            raise unittest.SkipTest(f'DEV not readable: {error}')
        cls.realm = R.expand(cls.project, report=lambda *a: None)

    def test_every_check_passes(self):
        failed = [c for c in self.realm.result if not c['ok']]
        self.assertEqual(failed, [])

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
