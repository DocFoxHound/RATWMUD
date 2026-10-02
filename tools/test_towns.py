"""The town builder (worldgen/towns.py) on a placeholder like the western generator leaves: built, peopled, every
door walkable from the gate; refused where the ground has been edited; and nothing outside the place touched."""
import copy
import unittest

from worldgen import towns as T
from worldgen.western import OX, OY, SETTLEMENTS


def placeholder(sid='westmarch'):
    """A project with one outdoor cell holding the settlement's walled box of empty ground, gates N and E."""
    _, name, kind, (x, y, w, h), _, _ = next(s for s in SETTLEMENTS if s[0] == sid)
    wx, wy = x + OX, y + OY
    cx, cy = wx - (wx - OX) % 256, wy - (wy - OY) % 256
    rows = [[','] * 256 for _ in range(256)]
    x0, y0 = wx - cx, wy - cy
    for yy in range(y0, y0 + h):
        for xx in range(x0, x0 + w):
            edge = min(xx - x0, x0 + w - 1 - xx, yy - y0, y0 + h - 1 - yy)
            rows[yy][xx] = '#' if edge < 2 else '.'
    for d in range(-2, 3):                      # Gates through the wall's depth.
        for t in range(2):
            rows[y0 + t][x0 + w // 2 + d] = 'G'
            rows[y0 + h // 2 + d][x0 + w - 1 - t] = 'G'
    cell = {'id': 'w_test', 'name': 'Test downs', 'description': '', 'x': cx, 'y': cy, 'width': 256, 'height': 256, 'z': 0,
            'outdoors': True, 'weather': 'clear', 'lighting': {'artificial': 1, 'daylightAccess': 1, 'tone': 'warm'},
            'territory': {'region': 'test_downs', 'claims': [], 'chapter': ''}, 'terrain': [''.join(r) for r in rows], 'heights': {}}
    return {'format': 'atlas', 'version': 3, 'id': 'test', 'name': 'Test', 'cells': [cell], 'rooms': [], 'links': [],
            'people': [], 'routes': [], 'factions': [], 'chapters': []}, (x0, y0, w, h)


class TownsTest(unittest.TestCase):
    def test_a_placeholder_is_built_and_peopled(self):
        project, (x0, y0, w, h) = placeholder()
        before = copy.deepcopy(project)
        out, results = T.build_all(project, only={'westmarch'}, report=lambda _: None)
        r = results['westmarch']
        self.assertGreaterEqual(len(r['manifest']), 15, 'the inn, shops, chapel, watch house and houses all fit')
        self.assertGreaterEqual(len(r['people']), 30)
        self.assertTrue(all(p['id'].startswith('tn_wm_') for p in r['people']), 'every resident carries the prefix')
        self.assertTrue(all(room['id'].startswith('tn_wm_') for room in r['rooms']))
        roles = {p['role'] for p in r['people']}
        self.assertTrue({'merchant', 'guard', 'civilian'} <= roles, 'keepers, a watch and townsfolk')
        self.assertTrue(r['routes'] and all(p['route'] == r['routes'][0]['id'] for p in r['people'] if p['role'] == 'guard'
                                            and p['workLabel'] == 'stands watch'), 'the watch walks a route')
        homes = [(p['home']['cell'], p['home']['x'], p['home']['y']) for p in r['people']]
        self.assertEqual(len(homes), len(set(homes)), 'no two share a bed')
        rooms = {room['id']: room for room in out['rooms']}
        for p in r['people']:
            room = rooms.get(p['home']['cell'])
            self.assertIsNotNone(room, f'{p["id"]} lives indoors')
            self.assertIn(room['terrain'][p['home']['y']][p['home']['x']], 'bz', 'in a bed')
        self.assertTrue(all(room['territory']['region'] == 'westmarch' for room in r['rooms']), 'the town is its own region')
        # Outside the box and the farms, nothing changed; the project passed in is untouched.
        self.assertEqual(project, before)
        cell = out['cells'][0]
        self.assertEqual(cell['terrain'][0], before['cells'][0]['terrain'][0])
        self.assertTrue(any('+' in row for row in cell['terrain']), 'doors on the street')
        again, results = T.build_all(out, only={'westmarch'}, report=lambda _: None)
        self.assertEqual(results, {}, 'a place already built is skipped')

    def test_edited_ground_is_refused(self):
        project, (x0, y0, w, h) = placeholder()
        rows = [list(r) for r in project['cells'][0]['terrain']]
        rows[y0 + 10][x0 + 10] = 'T'                # Someone put a table in the square.
        project['cells'][0]['terrain'] = [''.join(r) for r in rows]
        notes = []
        out, results = T.build_all(project, only={'westmarch'}, report=notes.append)
        self.assertEqual(results, {})
        self.assertTrue(any('REFUSED' in n and 'edited' in n for n in notes), notes)
        self.assertEqual(out['cells'][0]['terrain'], project['cells'][0]['terrain'])


if __name__ == '__main__':
    unittest.main()
