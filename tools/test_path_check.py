"""tools/path_check.py: what closes a settlement's paths and doors, and fixing it (tools/refresh_ridgemere3_map.py)."""
import collections
import unittest

import path_check as PC


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


if __name__ == '__main__':
    unittest.main()
