"""The western world generator (tools/worldgen/western.py): its plan and its pieces.

A whole generation takes about two and a half minutes, so these check the plan traced from the drawing and each
piece on its own; `python3 -m worldgen.western --preview DIR` renders the whole world to look at.
"""
import unittest

import numpy as np

import terrain_catalog
from worldgen import western as W


def overlaps(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah


def blank():
    world = W.Western({'heights': np.zeros((W.UA[3], W.UA[2]), dtype=np.float32)})
    world.biome = np.full((W.SIZE, W.SIZE), W.bid('temperate'), dtype=np.int8)
    for name in ('water', 'lake', 'ocean', 'water_tarn', 'streams'):
        setattr(world, name, np.zeros((W.SIZE, W.SIZE), dtype=bool))
    world.north_weight = np.zeros((W.SIZE, W.SIZE), dtype=np.float32)
    return world


class Plan(unittest.TestCase):
    def test_every_settlement_lies_within_one_cell_and_leaves_upper_accord_alone(self):
        boxes = []
        for sid, _, _, (x, y, w, h), _, _ in W.SETTLEMENTS:
            self.assertEqual((x // W.CELL, y // W.CELL), ((x + w - 1) // W.CELL, (y + h - 1) // W.CELL), sid)
            self.assertFalse(overlaps((x, y, w, h), W.UA), sid)
            boxes.append((x, y, w, h))
        for i, a in enumerate(boxes):
            for b in boxes[i + 1:]:
                self.assertFalse(overlaps(a, b), (a, b))

    def test_the_cities_are_where_the_author_put_them(self):
        where = {sid: (x // W.CELL, y // W.CELL) for sid, _, _, (x, y, _, _), _, _ in W.SETTLEMENTS}
        self.assertEqual(where['ridgemere'], (1, 0))        # Ridgemere Heights, North West
        self.assertEqual(where['ser_ferro'], (0, 9))        # The Ser Ferro Marches, South
        self.assertEqual(where['cinderbrook'], (0, 8))      # The Ser Ferro Marches, West
        self.assertEqual(where['lakeside'], (5, 5))         # The Mirrormere Shore, East
        for sid in ('ridgemere', 'ser_ferro'):              # Most of their cell: as large as Upper Accord's city.
            box = next(s[3] for s in W.SETTLEMENTS if s[0] == sid)
            self.assertGreaterEqual(min(box[2], box[3]), 220)

    def test_roads_join_real_gates_and_stay_on_the_map(self):
        gates = {(sid, side) for sid, _, _, _, sides, _ in W.SETTLEMENTS for side in sides}
        used = set()
        for name, stops, _ in W.ROADS:
            for stop in stops:
                if isinstance(stop, str):
                    self.assertIn(tuple(stop.split('.')), gates, name)
                    used.add(tuple(stop.split('.')))
                else:
                    self.assertTrue(0 <= stop[0] < W.SIZE and 0 <= stop[1] < W.SIZE, (name, stop))
        for sid, _, kind, _, sides, _ in W.SETTLEMENTS:
            if kind != 'ghost':
                self.assertTrue(any((sid, side) in used for side in sides), f'{sid} has no road')
        self.assertIn(W.UA_ENTRY, [stops[-1] for _, stops, _ in W.ROADS])

    def test_sub_biomes_use_catalog_tiles(self):
        for biome, subs in W.SUBBIOMES.items():
            for name, ground, trees, density, extras in subs:
                codes = list(ground or '') + list(trees or '') + [code for code, _ in extras]
                for code in codes:
                    self.assertIn(code, terrain_catalog.GLYPHS, f'{biome}/{name}: {code!r}')

    def test_the_springs_are_on_southern_peak(self):
        # Southern Peak is Upper Accord's cell at world x 256-511, y 640-767.
        for x, y in W.SPRINGS:
            wx, wy = x + W.OX, y + W.OY
            self.assertTrue(256 <= wx < 512 and 640 <= wy < 768, (wx, wy))


@unittest.skipUnless(W.DRAWING.exists(), 'the drawing is not on this machine')
class Drawing(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.label = W.read_drawing()
        cls.world = W.Western({'heights': np.zeros((W.UA[3], W.UA[2]), dtype=np.float32)})
        cls.world.lay_biomes()

    def at(self, plan_x, plan_y, grid=None):
        x, y = W.p(plan_x, plan_y)
        return W.BIOME_IDS[(self.label if grid is None else grid)[int(y), int(x)]]

    def test_the_drawings_regions_read_as_drawn(self):
        self.assertEqual(self.at(60, 330), 'water')          # The ocean, top left.
        self.assertEqual(self.at(970, 1150), 'water')        # The lake...
        self.assertNotEqual(self.at(1180, 1100), 'water')    # ...and its island.
        self.assertEqual(self.at(1100, 250), 'north')
        self.assertEqual(self.at(700, 1600), 'plains')
        self.assertEqual(self.at(1700, 1480), 'swamp')

    def test_the_regions_the_author_asked_for(self):
        label = self.world.label
        self.assertEqual(self.at(380, 150, label), 'wet')        # Ridgemere: rain, not snow.
        self.assertEqual(self.at(1100, 250, label), 'north')     # Snow further east.
        self.assertEqual(self.at(250, 1850, label), 'golden')    # Ser Ferro's grain country.
        self.assertEqual(self.at(1350, 1900, label), 'deep')     # The deep swamp south of the great river.
        # The rain coast is big enough to spend time in: several cells' worth.
        self.assertGreater(int((label == W.bid('wet')).sum()), 6 * W.CELL * W.CELL)

    def test_biomes_fade_into_each_other(self):
        # Going south from the cold north, the share of cold ground falls away over well over a hundred tiles.
        north = self.world.biome[:, 1100:1500] == W.bid('north')
        share = north.reshape(-1, 32 * 400).mean(axis=1)
        mixed = [i for i, s in enumerate(share) if .1 < s < .9]
        self.assertGreaterEqual(len(mixed) * 32, 125)

    def test_the_lake_leaves_a_shore_beside_upper_accord(self):
        x0, y0, w, h = W.UA
        self.assertFalse(self.world.lake[y0 + 500:y0 + h, x0 - 30:x0].any())

    def test_the_vale_river_rises_in_a_tarn_fed_by_streams(self):
        tx, ty, _, _ = W.TARN
        self.assertTrue(self.world.water_tarn[ty, tx])
        edge = W.UA[1] + W.UA[3] + 2
        self.assertFalse((self.world.water[edge, 2150:2345] & ~self.world.streams[edge, 2150:2345]).any(),
                         'no river springs full-width from the region edge')
        for mx, my in W.STREAM_MOUTHS:
            self.assertTrue(self.world.streams[my + 2, mx - 4:mx + 5].any())
        self.assertTrue(self.world.ua_streams)

    def test_road_ink_is_not_read_as_swamp(self):
        north_of_swamps = self.label[:1500, :1300]
        self.assertFalse(np.isin(north_of_swamps, [W.bid('swamp'), W.bid('deep')]).any())


class Pieces(unittest.TestCase):
    def test_components_finds_the_water_joined_to_each_seed(self):
        mask = np.zeros((50, 50), dtype=bool)
        mask[5:15, 5:15] = mask[30:40, 30:40] = True
        found = W.components(mask, [(8, 8), (35, 35), (25, 25)])
        self.assertEqual(int((found == 0).sum()), 100)
        self.assertEqual(int((found == 1).sum()), 100)
        self.assertEqual(int((found == -1).sum()), 50 * 50 - 200)

    def test_a_road_over_a_hill_climbs_at_most_half_a_step_a_tile_and_bridges_water(self):
        world = blank()
        xs, ys = np.meshgrid(np.arange(W.SIZE), np.arange(W.SIZE))
        world.c.heights[:] = W.half(np.clip(10 - np.hypot(xs - 300, ys - 300) / 6, 0, 10))
        world.water[295:305, 360:380] = True
        world.road('test', [(200, 300), (400, 300)])
        line = world.c.heights[300, 200:401]
        self.assertLessEqual(float(np.abs(np.diff(line)).max()), .5)
        self.assertTrue((world.c.codes[300, 362:378] == ord('8')).all(), 'a bridge where the road crosses water')
        self.assertTrue((world.c.codes[300, 200:360] == ord('d')).all())

    def test_roads_go_round_a_lake_but_bridge_a_river(self):
        world = blank()
        world.water[200:600, 400:700] = world.lake[200:600, 400:700] = True    # A lake in the way...
        path = world.route((300, 400), (800, 400), world.route_costs())
        self.assertFalse([q for q in path if world.water[int(q[1]), int(q[0])]], 'the road went into the lake')
        world = blank()
        world.water[:, 500:520] = True                                           # ...and a river across the map.
        path = world.route((300, 400), (800, 400), world.route_costs())
        self.assertLess(max(q[1] for q in path) - min(q[1] for q in path), 60, 'the road should just bridge it')

    def test_town_walls_follow_the_shore(self):
        world = blank()
        world.water[:, 150:] = True                                              # The sea east of x 150.
        world.settlement('t', 'Test', 'town', (100, 100, 100, 60), {'W': None}, True)
        codes = world.c.codes
        self.assertEqual(chr(codes[130, 170]), ',', 'nothing is built on the water')
        self.assertEqual(chr(codes[130, 144]), '#', 'the wall runs along the shore, back from the water')
        self.assertGreater(int((codes[100:160, 100:150] == ord('.')).sum()), 1000, 'the town fills the land')

    def test_settlements_are_walled_level_ground_with_open_gates(self):
        world = blank()
        world.settlement('t', 'Test', 'town', (100, 100, 70, 56), {'N': None, 'S': None}, False)
        codes = world.c.codes
        self.assertEqual(chr(codes[101, 101]), '#')
        self.assertEqual(chr(codes[128, 135]), '.', 'the inside is empty ground')
        self.assertEqual(chr(codes[100, 135]), 'G', 'a gate in the north wall')
        self.assertEqual(float(np.ptp(world.c.heights[100:156, 100:170])), 0)
        gx, gy = world.gates[('t', 'N')]
        self.assertLess(gy, 100, 'the road meets the gate from outside')

    def test_a_fortress_gate_opens_through_its_whole_wall(self):
        world = blank()
        world.settlement('f', 'Fort', 'fortress', (100, 100, 84, 84), {'S': None, 'E': None}, False)
        codes = world.c.codes
        self.assertTrue(all(chr(ch) in 'Gf' for ch in codes[176:184, 142]), 'south gate, all the way through')
        self.assertTrue(all(chr(ch) in 'Gf' for ch in codes[142, 176:184]), 'east gate, all the way through')

    def test_a_road_arrives_at_a_gate_at_the_settlements_level(self):
        world = blank()
        xs, ys = np.meshgrid(np.arange(W.SIZE), np.arange(W.SIZE))
        world.c.heights[:] = W.half(np.clip((ys - 100) / 40, 0, 4))           # Rising ground north of the town.
        world.settlement('t', 'Test', 'town', (100, 300, 70, 56), {'N': None}, False)
        gx, gy = world.gates[('t', 'N')]
        world.road('in', [(gx, 120), (gx, gy)], ends=(None, world.levels['t']))
        column = world.c.heights[120:302, int(gx)]
        self.assertLessEqual(float(np.abs(np.diff(column)).max()), .5, 'walkable all the way through the gate')

    def test_blended_biomes_interleave_across_the_border(self):
        world = blank()
        label = np.full((W.SIZE, W.SIZE), W.bid('temperate'), dtype=np.int8)
        label[:1280] = W.bid('north')
        share = (world.blend(label)[:, 500:1500] == W.bid('north')).mean(axis=1)
        self.assertTrue(.1 < share[1280] < .9)
        self.assertGreater(share[1150], .6)
        self.assertLess(share[1410], .4)
        self.assertGreater(share[1180], .05 + share[1380])
        self.assertEqual(share[200], 1.0)
        self.assertEqual(share[2400], 0.0)

    def test_streams_are_painted_into_upper_accord_keeping_its_heights(self):
        project = {'cells': [{'id': 'peak', 'x': 0, 'y': 0, 'width': 4, 'height': 2,
                              'terrain': ['r%,G', ',,,,'], 'heights': {'0,0': 7, '1,0': 9}}]}
        out, changed = W.with_streams(project, [(0, 0), (1, 0), (2, 0), (3, 0)])
        cell = out['cells'][0]
        self.assertEqual(changed, {'peak'})
        self.assertEqual(cell['terrain'][0], '~%~G', 'cliffs and gates stay as they are')
        self.assertEqual(cell['heights']['0,0'], 7)
        self.assertEqual(project['cells'][0]['terrain'][0], 'r%,G', 'the input is not changed')

    def test_cells_cover_the_world_outside_upper_accord_with_unique_names(self):
        world = blank()
        cells = world.cells()
        self.assertEqual(len(cells), (W.SIZE // W.CELL) ** 2 - 9)
        ua = (W.UA[0] + W.OX, W.UA[1] + W.OY, W.UA[2], W.UA[3])
        for c in cells:
            self.assertFalse(overlaps((c['x'], c['y'], W.CELL, W.CELL), ua), c['id'])
            self.assertEqual(len(c['terrain']), W.CELL)
        self.assertEqual(len({c['id'] for c in cells}), len(cells))
        self.assertEqual(len({c['name'] for c in cells}), len(cells))


if __name__ == '__main__':
    unittest.main()
