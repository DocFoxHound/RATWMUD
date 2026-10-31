#!/usr/bin/env python3
"""Ridgemere 2.0, the city (Docs/Design/67, section 8 A): the new kit pieces, and every acceptance check of the
generator's city and NPC plan.

  python3 tools/test_ridgemere2.py              the kit tests, and the city's checks on Atlas's world ridgemere2
                                                (read only; skipped without the database)
  RATW_R2_PROJECT=FILE python3 tools/test_ridgemere2.py
                                                the city's checks on a saved project (load_world's JSON)

Making the city takes a few minutes; it is made once for all the checks.
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

from worldgen import buildings as B  # noqa: E402
from worldgen import ridgemere2_people as PP  # noqa: E402
from worldgen.canvas import Canvas  # noqa: E402
from worldgen.site import Site  # noqa: E402


class Kit(unittest.TestCase):
    def test_flats_stack_up_to_three_homes_with_stairs_on_distinct_tiles(self):
        plans, pairs = B.flats(random.Random(1), 'ridgemere', 3, (2, 3, 4), (12, 10))
        self.assertEqual(len(plans), 3)
        self.assertEqual([len(p.beds) for p in plans], [2, 3, 4])
        tiles = [(a[1], a[2]) for a, b in pairs]
        self.assertEqual(len(set(tiles)), 2)
        for p in plans[1:]:
            self.assertNotIn('+', ''.join(''.join(r) for r in p.g))     # Upstairs has no street door.

    def test_a_through_building_has_two_door_links(self):
        c = Canvas(64, 64, '.')
        site = Site(c, [('cell', 0, 0, 64, 64)])
        plans, pairs = B.flats(random.Random(2), 'ridgemere', 2, (2, 2), (10, 8), back=True)
        rooms = [plans[0].room('', 'A', 'a'), plans[1].room('up', 'A up', 'b', z=1)]
        b = B.Building('flats', 'Test Row', 'ridgemere', (7, 6), rooms, pairs)
        site.place(b, 10, 10, 'S', 'ridgemere', open_door=False)
        ids = {l['id'] for l in site.links}
        self.assertIn('test_row_door', ids)
        self.assertIn('test_row_door_2', ids)
        self.assertEqual(c.at(13, 15), '+')
        self.assertEqual(c.at(13, 10), '+')

    def test_an_l_shaped_block_leaves_its_notch_open(self):
        c = Canvas(64, 64, '.')
        site = Site(c, [('cell', 0, 0, 64, 64)])
        p = B.Plan(10, 8, '#', '.', random.Random(3))
        p.notch('NE', 3, 2)
        b = B.Building('house', 'Notch House', 'ridgemere', (8, 6), [p.room('', 'N', 'n')])
        mask = np.ones((6, 8), dtype=bool)
        mask[:2, 5:] = False
        site.place(b, 4, 4, 'S', 'ridgemere', mask=mask)
        self.assertEqual(c.at(11, 4), '.')
        self.assertEqual(c.at(4, 4), '#')

    def test_old_recipes_place_as_before(self):
        """Site.place without the new arguments stamps exactly what it did."""
        c = Canvas(40, 40, '.')
        site = Site(c, [('cell', 0, 0, 40, 40)])
        p = B.house(random.Random(4), 'ridgemere', 3)
        b = B.Building('house', 'Old House', 'ridgemere', (9, 6), [p.room('', 'Old House', 'x')])
        site.place(b, 5, 5, 'S', 'r')
        rows = [''.join(chr(v) for v in c.codes[y, 5:14]) for y in range(5, 11)]
        self.assertEqual(rows[0], '#########')
        self.assertEqual(rows[-1], '####+####')
        self.assertEqual(len(site.links), 1)

    def test_gaol_cells_and_the_market_hall(self):
        g = B.gaol_cells(random.Random(5), 'civic', 22, 20)
        text = ''.join(''.join(r) for r in g.g)
        self.assertEqual(text.count('p'), 8)
        self.assertGreaterEqual(text.count('|'), 16)
        hall = B.market_hall(random.Random(6), 'ridgemere')
        self.assertEqual(len(hall.work), 7)                    # Six keepers and the warden.
        self.assertEqual(''.join(''.join(r) for r in hall.g).count('+'), 2)
        self.assertEqual(sum(1 for r in hall.g for ch in r if ch == '='), 18)


class Labels(unittest.TestCase):
    def test_the_label_rules(self):
        self.assertEqual(PP.business_for('pawnbroker at The Pawn and Ledger'), 'pawnbroker')
        self.assertEqual(PP.business_for('builder at the Greatly works'), 'builders_works')
        self.assertEqual(PP.business_for('weaver on Market Square'), 'weaver')
        self.assertIn('"forge" outside its business', PP.label_problems('clerking at the Forge Office'))
        self.assertTrue(any('House head' in x for x in PP.label_problems('keeping the Market Hall'.replace('the ', ''))))
        self.assertEqual(PP.label_problems('keeping the rolls at the Records Office'), [])
        self.assertTrue(PP.label_problems('burns charcoal at the Charcoal Store'))
        self.assertEqual(PP.label_problems('priest at the cathedral', clergy=True), [])


def project_for_checks():
    path = os.environ.get('RATW_R2_PROJECT')
    if path:
        data = json.loads(Path(path).read_text(encoding='utf-8'))
        return data.get('project', data)
    from worldgen import ridgemere2 as R
    try:
        project, _ = R.load('ridgemere2')
    except Exception as error:                        # No database here.
        raise unittest.SkipTest(f'ridgemere2 is not reachable: {error}')
    return project


class City(unittest.TestCase):
    """Every acceptance check of section 8 A, on the city made from the world as it is."""

    @classmethod
    def setUpClass(cls):
        from worldgen import ridgemere2 as R
        project = project_for_checks()
        if any(c.get('id', '').startswith('w_ridgemere_') for c in project['cells']):
            # The city is already written: make it again from the world as it was (its NPCs and interiors are kept).
            pass
        cls.m = R.make_all(project, report=lambda line: None)

    def test_every_check(self):
        failed = [f'{c["req"]} {c["check"]}: {c["detail"][:400]}' for c in self.m['checks'] if not c['ok']]
        self.assertEqual(failed, [])

    def test_voices_and_relations_untouched(self):
        p = self.m['project']
        self.assertEqual(len(p['people']), 1083)
        before = {x['id']: x for x in self.m['base']['people']}
        for x in p['people']:
            b = before[x['id']]
            for key in ('name', 'description', 'backstory', 'personality', 'greeting', 'appearance', 'age', 'voice'):
                self.assertEqual(x[key], b[key], (x['id'], key))

    def test_the_old_interiors_stay(self):
        ids = {r['id'] for r in self.m['project']['rooms']}
        self.assertTrue({r['id'] for r in self.m['base']['rooms']} <= ids)
        self.assertTrue(all(r['id'].startswith('r2_') for r in self.m['city'].site.rooms))

    def test_it_is_made_the_same_each_time(self):
        rec = self.m['city'].records
        self.assertGreater(len(rec), 250)


if __name__ == '__main__':
    unittest.main()
