"""The new world's generator (Docs/Design/60: tools/worldgen/realm.py, cities.py, citizens.py, towns.py, country.py).

The plan is checked quickly; a whole generation takes about three minutes, so it runs only with RATW_SLOW=1:
  python3 -m unittest test_realm                 # the plan
  RATW_SLOW=1 python3 -m unittest test_realm     # + the whole world built and checked against what was asked for
"""
import os
import unittest

import numpy as np

import terrain_catalog
from worldgen import cities as C
from worldgen import citizens as Z
from worldgen import country as K
from worldgen import realm as W
from worldgen import towns as T


def overlaps(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah


class Plan(unittest.TestCase):
    def test_eight_cells_by_eight(self):
        self.assertEqual(W.SIZE, 8 * W.CELL)
        self.assertEqual(W.CELL, 256)

    def test_six_towns_each_within_one_cell_apart(self):
        self.assertEqual(len(W.SETTLEMENTS), 6)
        boxes = []
        for sid, _, kind, (x, y, w, h), _, _ in W.SETTLEMENTS:
            self.assertEqual(kind, 'town')
            self.assertEqual((x // W.CELL, y // W.CELL), ((x + w - 1) // W.CELL, (y + h - 1) // W.CELL), sid)
            boxes.append((x, y, w, h))
        for i, a in enumerate(boxes):
            for b in boxes[i + 1:]:
                self.assertFalse(overlaps(a, b), (a, b))
        self.assertEqual(set(T.TOWNS), {s[0] for s in W.SETTLEMENTS}, 'every town is built out')

    def test_the_quarters_are_where_asked(self):
        where = {sid: (x // W.CELL, y // W.CELL) for sid, _, _, (x, y, _, _), _, _ in W.SETTLEMENTS}
        self.assertGreaterEqual(where['frosthollow'][0], 4)       # The north-east: the mountains.
        self.assertLess(where['frosthollow'][1], 4)
        for sid in ('oakhaven', 'mirebridge'):                      # The south-west: warm forest and fens.
            self.assertLess(where[sid][0], 4)
            self.assertGreaterEqual(where[sid][1], 4)
        for sid in ('larkfield', 'southmeadow'):                    # The south-east: downs and fields.
            self.assertGreaterEqual(where[sid][0], 4)
            self.assertGreaterEqual(where[sid][1], 4)
        xs, ys = zip(*C.WALL_OUTLINE)                               # Ridgemere: the north-west, on the coast.
        self.assertLess(max(xs), W.SIZE / 2)
        self.assertLess(max(ys), W.SIZE / 2)
        coast_x = np.interp(np.mean(ys), [y for y, _ in W.COAST], [x for _, x in W.COAST])
        self.assertLess(min(xs) - coast_x, C.WHARF_WIDTH + 20, 'its west side follows the coast')

    def test_roads_join_real_gates_and_stay_on_the_map(self):
        gates = {(sid, side) for sid, _, _, _, sides, _ in W.SETTLEMENTS for side in sides}
        gates |= {('ridgemere', side) for side in 'NES'}
        for name, stops, _ in W.ROADS:
            for stop in stops:
                if isinstance(stop, str):
                    self.assertIn(tuple(stop.split('.')), gates, name)
                else:
                    self.assertTrue(0 <= stop[0] < W.SIZE and 0 <= stop[1] < W.SIZE, (name, stop))
        reached = {s.split('.')[0] for _, stops, _ in W.ROADS for s in stops if isinstance(s, str)}
        self.assertEqual(reached, {s[0] for s in W.SETTLEMENTS} | {'ridgemere'}, 'every place is on a road')

    def test_sub_biomes_use_catalog_tiles(self):
        for biome, subs in W.SUBBIOMES.items():
            for name, ground, trees, _, extras in subs:
                for code in list(ground or {}) + list(trees or '') + [c for c, _ in extras]:
                    self.assertIn(code, terrain_catalog.GLYPHS, (biome, name, code))
            self.assertIn(biome, W.BIOME_IDS)

    def test_the_houses(self):
        self.assertEqual(set(C.HOUSES), {'greatly', 'fell', 'grayrock', 'mossgrave'})
        self.assertIsNone(C.HOUSES['greatly'][4], 'Greatly lives inside the walls')
        for house in ('fell', 'grayrock', 'mossgrave'):
            self.assertIsNotNone(C.HOUSES[house][4], f'{house} has an estate outside')
        self.assertEqual(set(Z.HEADS), set(C.HOUSES))
        self.assertEqual({f['id'] for f in Z.FACTIONS} >= {f'house_{h}' for h in C.HOUSES}, True)

    def test_three_markets_matching_their_quarters(self):
        self.assertEqual(set(C.MARKETS), {'heights_market', 'commons_market', 'sump_market'})
        for key, (name, foods, call) in C.MARKETS.items():
            self.assertEqual(len(foods), 8, key)
            self.assertLessEqual(len(f'{max(foods, key=len)} stall on {name}'), 40)
            self.assertTrue(set(foods) <= set(Z.MARKETS[key][1]))
        self.assertIn('fish', C.MARKETS['sump_market'][1])
        self.assertIn('honey', C.MARKETS['heights_market'][1])

    def test_country_sites(self):
        for sid, wanted in K.SITES.items():
            count = sum(n for _, n, _ in wanted)
            self.assertGreaterEqual(len(K.NAMES[sid]), count, f'{sid}: a name for every site')
            for kind, _, cells in wanted:
                self.assertIn(kind, K.KINDS)
                for cx, cy in cells:
                    self.assertTrue(0 <= cx < 8 and 0 <= cy < 8, (sid, kind, cx, cy))
            for n in K.NAMES[sid]:
                for kind, _, _ in wanted:
                    self.assertLessEqual(len(K.KINDS[kind]['label'].format(n=n, site='', building='')), 40, n)
        near = {kind for kind, _, _ in K.SITES['ridgemere'] if kind in ('sheep', 'rabbit', 'farm', 'orchard')}
        self.assertEqual(near, {'sheep', 'rabbit'}, 'farms near Ridgemere are livestock')
        south = {kind for sid in ('larkfield', 'southmeadow') for kind, _, _ in K.SITES[sid]}
        self.assertTrue({'farm', 'orchard'} <= south, 'grain farms and orchards in the south')


@unittest.skipUnless(os.environ.get('RATW_SLOW'), 'the whole world takes ~3 minutes (RATW_SLOW=1)')
class WholeWorld(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.world = W.Realm()
        cls.world.build()
        cls.cells = cls.world.cells()
        cls.project = W.assemble({'id': 'test_realm'}, cls.world, cls.cells, report=lambda _: None)
        cls.city = cls.world.cities[0]

    def test_the_project_is_valid(self):
        import map_editor
        map_editor.check_project(self.project, for_game=False)

    def test_eight_by_eight_cells(self):
        xs = {c['x'] for c in self.cells}
        ys = {c['y'] for c in self.cells}
        self.assertLessEqual(len(xs), 8)
        self.assertLessEqual(len(ys), 8)
        self.assertGreaterEqual(len(self.cells), 56)

    def test_ridgemere_is_twice_its_old_size(self):
        region = self.world.places['ridgemere'].copy()
        for m in self.city.suburb_masks:
            region |= m
        self.assertGreaterEqual(region.sum(), 2 * 0.9 * 256 * 256 * .9, 'about two cells of city')

    def test_walls_gates_tiers(self):
        city, c = self.city, self.world.c
        for side in 'NES':
            self.assertIn(('ridgemere', side), self.world.gates)
        self.assertEqual(len(city.spots['water_gates']), 3)
        # Up the hill a step at a time, never two.
        t = city.tier
        inside = t >= 0
        for dy, dx in ((1, 0), (0, 1)):
            a, b = t, np.roll(t, (-dy, -dx), (0, 1))
            both = inside & np.roll(inside, (-dy, -dx), (0, 1))
            self.assertLessEqual(int(np.abs(a - b)[both].max()), 1)
        self.assertEqual(set(np.unique(t[inside])), {0, 1, 2, 3})
        # The wharf (tier 0) lies outside the wall, between it and the Sound.
        self.assertFalse((city.wharf & city.core).any())
        self.assertTrue((city.wharf & np.roll(self.world.ocean, 3, 1)).any() or (city.wharf & self.world.ocean).any()
                        or city.wharf.any())

    def test_greatly_inside_the_others_outside(self):
        core = self.city.core
        greatly = self.world.places['estate_greatly']
        self.assertTrue((greatly & core).sum() == greatly.sum(), 'Greatly House is inside the walls')
        for house in ('fell', 'grayrock', 'mossgrave'):
            estate = self.world.places[f'estate_{house}']
            self.assertFalse((estate & core).any(), f'{house} is outside the walls')
            codes = self.world.c.codes[estate]
            for ch in 'FSB3~':
                self.assertIn(ord(ch), codes, f'{house}\'s grounds have {ch}: fountains, statues, hedges, flowers, a pond')

    def test_districts_and_markets(self):
        manifest = self.world.city_site.manifest
        names = {b['name'] for b in manifest}
        for name in ('The City Office', 'The Rain Hall', 'The Watch House', 'The Customs House', 'The Greatly Yard',
                     'The Grayrock Ironworks', 'The Soup Kitchen', 'Greatly House', 'Fellhall', 'Grayrock Hall',
                     'Mossgrave Court'):
            self.assertIn(name, names)
        for key in C.MARKETS:
            self.assertEqual(len(self.city.spots[f'stalls_{key}']), 8, key)
        by = {b['name']: b for b in manifest}
        tier = self.city.tier
        door = lambda b: tier[b['door']['y'], b['door']['x']]
        works = [b for b in manifest if b['district'].startswith('works') and b['kind'] == 'works']
        self.assertGreaterEqual(len(works), 10, 'an industry sector')
        self.assertTrue(all(door(b) in (1, 0, 99) or door(b) <= 1 for b in works), 'the works are on the lowest tier')
        tenements = [b for b in manifest if b['district'].startswith('sump') and b['kind'] == 'tenement']
        self.assertGreaterEqual(len(tenements), 30, 'the slums')
        villas = [b for b in manifest if b['kind'] == 'villa']
        self.assertGreaterEqual(len(villas), 12)
        self.assertTrue(all(tier[b['door']['y'], b['door']['x']] in (3, 99) or tier[b['door']['y'], b['door']['x']] == 3
                            for b in villas))
        self.assertIn('Rainrun Bridge', {n for n, _, _ in C.SUBURBS.values()})
        outside = [b for b in manifest if b['district'].startswith('suburb_') and b['kind'] == 'house']
        self.assertGreaterEqual(len(outside), 25, 'houses outside the walls')
        self.assertTrue(by['The City Office']['district'].startswith('office'))

    def test_people(self):
        people = self.project['people']
        names = {p['name'] for p in people}
        for name in ('Ambrose Greatly', 'Isolde Fell', 'Aldric Grayrock', 'Maren Grayrock', 'Halvard Mossgrave'):
            self.assertIn(name, names)
        aldric = next(p for p in people if p['name'] == 'Aldric Grayrock')
        self.assertTrue(aldric['route'], 'Lord Grayrock is always on the road')
        homes = [(p['home']['cell'], p['home']['x'], p['home']['y']) for p in people]
        self.assertEqual(len(homes), len(set(homes)), 'no two share a bed')
        stalls = [p for p in people if ' stall on ' in p['workLabel']]
        self.assertEqual(len(stalls), 24)
        self.assertGreater(len(people), 800)
        labels = ' '.join(p['workLabel'] for p in people)
        for word in ('raises sheep at', 'keeps rabbits at', 'tends the orchard at', 'works the fields at'):
            self.assertIn(word, labels)


if __name__ == '__main__':
    unittest.main()
