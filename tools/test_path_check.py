"""tools/path_check.py: what closes a settlement's paths and doors, and fixing it (tools/refresh_ridgemere3_map.py)."""
import collections
import unittest

import path_check as PC
import refresh_ridgemere3_map as RM


def project(rows, heights=None, doors=()):
    """One outdoor cell of these rows (a list of strings), with doors to an interior at the given tiles."""
    w, h = len(rows[0]), len(rows)
    cell = {'id': 'w_test', 'name': 'Test', 'x': 0, 'y': 0, 'width': w, 'height': h, 'z': 0, 'outdoors': True,
            'terrain': list(rows), 'heights': dict(heights or {})}
    room = {'id': 'rm_test', 'name': 'Room', 'x': 0, 'y': 0, 'width': 4, 'height': 4, 'terrain': ['....'] * 4,
            'heights': {}}
    links = [{'id': f'door_{i}', 'kind': 'door', 'a': {'cell': 'w_test', 'x': x, 'y': y},
              'b': {'cell': 'rm_test', 'x': 1, 'y': 1}} for i, (x, y) in enumerate(doors)]
    return {'cells': [cell], 'rooms': [room], 'links': links}


def grid(p):
    c = p['cells'][0]
    return PC.Grid(p, (0, 0, c['width'], c['height']))


# Two streets joined by a one-tile alley between buildings, and by a lane at the west end: the way round is long.
ALLEY = [
    '_____________________',
    '_____________________',
    '_##########.########_',
    '_#ZZZZZZZZZ.ZZZZZZZ#_',
    '_#ZZZZZZZZZ.ZZZZZZZ#_',
    '_#ZZZZZZZZZ.ZZZZZZZ#_',
    '_##########.########_',
    '_____________________',
    '_____________________',
]


def put(rows, x, y, code):
    rows = list(rows)
    rows[y] = rows[y][:x] + code + rows[y][x + 1:]
    return rows


class PathCheckTest(unittest.TestCase):
    def kinds(self, problems):
        return collections.Counter(p['kind'] for p in PC.failures(problems))

    def test_open_alley_passes(self):
        self.assertEqual(PC.failures(PC.check(grid(project(ALLEY)))), [])

    def test_barrel_in_alley_is_a_closed_path_and_is_moved_aside(self):
        p = project(put(ALLEY, 11, 4, 'O'))
        g = grid(p)
        problems = PC.check(g)
        self.assertEqual(self.kinds(problems), {'closed path': 1})
        self.assertEqual(problems[0]['blockers'], [(11, 4)])
        moves = []
        self.assertEqual(PC.failures(PC.fix(g, moves=moves)), [])
        g.commit()
        self.assertEqual(p['cells'][0]['terrain'][4][11], '.')
        self.assertEqual(sum(r.count('O') for r in p['cells'][0]['terrain']), 1)   # (Carried aside, not thrown away.)
        self.assertEqual(PC.failures(PC.check(grid(p))), [])

    def test_barrel_shutting_in_a_back_door_is_moved_aside(self):
        # A house between two streets joined by a lane: its front door on the north street, its back door onto a strip
        # of grass behind it, which a barrel closes off from the south street. Reached through the house, the strip
        # isn't cut off; but whoever comes out by the back door finds no way on (Ridgemere 3, 2026-11-20).
        rows = ['__________',
                '_###+#####',
                '_ZZZZZZZZ#',
                '_###+#####',
                '_###,,####',
                '_###O#####',
                '__________']
        p = project(rows, doors=[(4, 1), (4, 3)])
        g = grid(p)
        problems = PC.failures(PC.check(g))
        self.assertEqual(self.kinds(problems), {'shut in': 1}, problems)
        self.assertEqual(problems[0]['blockers'], [(4, 5)])
        self.assertEqual(PC.failures(PC.fix(g)), [])
        g.commit()
        self.assertNotEqual(p['cells'][0]['terrain'][5][4], 'O')
        self.assertEqual(PC.failures(PC.check(grid(p))), [])

    def test_back_door_onto_its_own_walled_yard_is_fine(self):
        # A back door onto a yard walled round, with nothing in the way to clear: the house's own, not shut in.
        rows = ['__________',
                '_###+#####',
                '_ZZZZZZZZ#',
                '_###+#####',
                '_###,,####',
                '_#########',
                '__________']
        self.assertEqual(PC.failures(PC.check(grid(project(rows, doors=[(4, 1), (4, 3)])))), [])

    def test_barrel_in_a_wide_street_is_fine(self):
        self.assertEqual(PC.failures(PC.check(grid(project(put(ALLEY, 5, 0, 'O'))))), [])

    def test_fence_closing_off_a_yard_with_a_path_is_cleared(self):
        rows = ['__________',
                '__________',
                '####|#####',
                '#........#',
                '##########']
        g = grid(project(rows))
        problems = [p for p in PC.check(g) if p['kind'] == 'cut off']
        self.assertEqual(len(problems), 1)
        self.assertEqual(problems[0]['blockers'], [(4, 2)])
        self.assertEqual(PC.failures(PC.fix(g)), [])
        self.assertNotEqual(g.at(4, 2), '|')

    def test_door_whose_approach_is_closed_by_crates(self):
        rows = ['__________',
                '__________',
                '###x######',
                '#..+ZZZZZ#',
                '##########']
        g = grid(project(rows, doors=[(3, 3)]))
        problems = PC.check(g)
        self.assertTrue(any(p['kind'] == 'cut off' and 'door' in p['what'] for p in problems))
        self.assertEqual(PC.failures(PC.fix(g)), [])

    def test_steep_step_on_a_road_is_graded(self):
        heights = {f'{x},{y}': (2 if y < 5 else .5) for x in range(10) for y in range(10)}
        rows = [''.join('d' if x == 4 else ',' for x in range(10)) for _ in range(10)]
        g = grid(project(rows, heights))
        self.assertEqual(self.kinds(PC.check(g)), {'steep step': 1})
        self.assertEqual(PC.failures(PC.fix(g)), [])
        self.assertEqual([g.h_at(4, y) for y in range(4, 8)], [2, 1.5, 1, .5])

    def test_walled_yard_with_no_door_is_only_noted(self):
        rows = ['__________',
                '##########',
                '#........#',
                '##########']
        problems = PC.check(grid(project(rows)))
        self.assertEqual([p['kind'] for p in problems], [PC.SEALED])
        self.assertEqual(PC.failures(problems), [])

    def test_unreachable_door_behind_a_wall_is_a_failure(self):
        rows = ['__________',
                '##########',
                '#..+ZZZZZ#',
                '##########']
        problems = PC.check(grid(project(rows, doors=[(3, 2)])))
        self.assertEqual(self.kinds(problems), {'cut off': 1})
        self.assertEqual(problems[0]['blockers'], [])


# A cobbled street four tiles wide between two rows of buildings, a two-tile lane north of them, a one-tile alley
# through the northern row, and a dirt road with grass beside it.
STREETS = [
    '########################',
    '#______________________#',
    '#______________________#',
    '####_###################',
    '#ZZZ_ZZZZZZZZZZZZZZZZZZ#',
    '########################',
    '________________________',
    '________________________',
    '________________________',
    '________________________',
    '########################',
    ',,,,,,,,,,,,,,,,,,,,,,,,',
    'dddddddddddddddddddddddd',
    'dddddddddddddddddddddddd',
    ',,,,,,,,,,,,,,,,,,,,,,,,',
    ',,,,,,,,,,,,,,,,,,,,,,,,',
]


class LampTest(unittest.TestCase):
    def lamp_problems(self, rows):
        return [p for p in PC.check(grid(project(rows))) if p['kind'] == PC.LAMP_IN_THE_WAY]

    def test_lamp_at_a_wide_streets_edge_passes(self):
        self.assertEqual(self.lamp_problems(put(STREETS, 10, 6, "'")), [])
        self.assertEqual(self.lamp_problems(put(STREETS, 10, 9, "'")), [])

    def test_lamp_on_a_wide_streets_walking_line_fails(self):
        # One tile in from the edge: an open tile beside it, so no closed path, but it stands in the way of a straight
        # walk along the street.
        rows = put(STREETS, 10, 7, "'")
        self.assertEqual([p['kind'] for p in PC.failures(PC.check(grid(project(rows))))], [PC.LAMP_IN_THE_WAY])
        self.assertIn('middle', self.lamp_problems(rows)[0]['what'])

    def test_lamp_across_a_streets_line_north_south_fails(self):
        rows = [r[:12] for r in STREETS[5:11]]          # The wide street, cut to twelve tiles.
        rows = [''.join(r[x] if x < 12 else '#' for x in range(12)) for r in rows]
        rows = [''.join(row[y] for row in rows) for y in range(12)]   # Turned: now it runs north-south.
        self.assertTrue(self.lamp_problems(put(rows, 2, 6, "'")))
        self.assertEqual(self.lamp_problems(put(rows, 1, 6, "'")), [])

    def test_lamp_in_a_two_tile_lane_fails_even_at_its_edge(self):
        problems = self.lamp_problems(put(STREETS, 10, 1, "'"))
        self.assertEqual(len(problems), 1)
        self.assertIn('only 1 tile wide', problems[0]['what'])

    def test_lamp_in_a_one_tile_alley_fails(self):
        problems = self.lamp_problems(put(STREETS, 4, 3, "'"))
        self.assertEqual(len(problems), 1)
        self.assertIn('one tile wide', problems[0]['what'])

    def test_lamp_on_the_grass_beside_a_road_passes_and_on_the_road_fails(self):
        self.assertEqual(self.lamp_problems(put(STREETS, 10, 11, "'")), [])
        self.assertTrue(self.lamp_problems(put(STREETS, 10, 13, "'")))

    def test_props_are_never_carried_onto_a_walking_line(self):
        g = grid(project(STREETS))
        self.assertFalse(PC.harmless(g, 10, 7, set()))
        self.assertTrue(PC.harmless(g, 10, 6, set()))

    def test_lamps_in_the_way_are_carried_to_the_edge_once(self):
        rows = put(put(STREETS, 3, 7, "'"), 22, 13, "'")
        p = project(rows)
        g = grid(p)
        counts, moves = collections.Counter(), []
        self.assertEqual(RM.move_lamps(g, {'_', 'd'}, frozenset(',_d'), counts, moves, 'test'), 2)
        g.commit()
        terrain = p['cells'][0]['terrain']
        self.assertEqual((terrain[7][3], terrain[13][22]), ('_', 'd'))
        lamps = {(x, y) for y, r in enumerate(terrain) for x, c in enumerate(r) if c == "'"}
        self.assertEqual(lamps, {(3, 6), (22, 14)})          # (The street's edge row; the grass beside the road.)
        self.assertEqual(self.lamp_problems(terrain), [])
        self.assertEqual(RM.move_lamps(grid(p), {'_', 'd'}, frozenset(',_d'), counts, moves, 'test'), 0)

    def test_new_lamps_stand_only_at_edges(self):
        p = project(STREETS)
        g = grid(p)
        RM.place_lamps(g, {'_', 'd'}, frozenset(',_d'), collections.Counter(), 'test')
        g.commit()
        terrain = p['cells'][0]['terrain']
        self.assertTrue(any("'" in r for r in terrain))
        self.assertEqual(PC.failures(PC.check(grid(p))), [])


if __name__ == '__main__':
    unittest.main()
