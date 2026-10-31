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

    def test_the_food_rebalance_labels(self):
        # Each market stall's label is its trade's, and the Carters' Yard is a business of its own.
        for labels in PP.OUTDOOR.values():
            for label in labels:
                key = label.split(' on ')[0].split(' in ')[0].split(',')[0]
                self.assertEqual(PP.business_for(label), PP.OUTDOOR_BUSINESS[key], label)
                self.assertEqual(PP.label_problems(label, PP.OUTDOOR_BUSINESS[key], food=True), [], label)
        for label, business in PP.HALL_KEEPERS:
            self.assertEqual(PP.business_for(label), business, label)
        self.assertEqual(PP.business_for("keeping the Carters' Yard"), 'carters')
        self.assertEqual(PP.business_for('driving for the carters'), 'carters')
        self.assertEqual(PP.business_for('cooking for Northgate'), 'inn')
        self.assertEqual(PP.label_problems('cooking for Northgate', 'inn', food=True), [])


class PeoplesText(unittest.TestCase):
    PERSON = {'id': 'rm_test_wolf', 'name': 'Hanne Testwick', 'age': 30,
              'description': 'A lean grey timber wolf with a missing claw, who keeps the egg stall on the Heights Market.',
              'greeting': 'Good day to you. Fine egg, fresh this morning.',
              'backstory': 'Hanne lost a brother on the wharf.'}

    def test_a_job_changed_here_gets_its_text(self):
        entry = {'post': 'keeper', 'business': 'bakery', 'record': 'rm_x'}
        out, why = PP.person_text(dict(self.PERSON), entry, 'baker at The Penny Loaf', 'tinker at Mend & Make Do',
                                  {'rm_x': 'The Penny Loaf'})
        self.assertEqual(why, 'job')
        self.assertTrue(out['description'].startswith('A lean grey timber wolf with a missing claw, '))
        self.assertIn('baker who keeps The Penny Loaf', out['description'])
        self.assertTrue(out['greeting'].startswith('Good day to you. '))
        self.assertTrue(out['backstory'].startswith('Hanne lost a brother on the wharf. Hanne '))
        self.assertNotIn('dog', ' '.join(out.values()).lower())

    def test_stale_text_is_fixed_and_fitting_text_left(self):
        entry = {'post': 'pool', 'record': 'pier'}
        same = 'unloading ships at the piers'
        out, why = PP.person_text(dict(self.PERSON), entry, same, same, {})
        self.assertEqual(why, 'stale')
        self.assertIn('a docker on the piers', out['description'])
        self.assertNotIn('backstory', out)                     # (Their job didn't change here.)
        fitting = dict(self.PERSON, description='A lean grey timber wolf with a missing claw, a docker on the piers.',
                       greeting='Good day to you. Mind your feet.')
        self.assertEqual(PP.person_text(fitting, entry, same, same, {}), ({}, None))
        broken = dict(fitting, greeting='Good day to you. I am carries loads for hire today.')
        out, why = PP.person_text(broken, entry, same, same, {})
        self.assertEqual(why, 'grammar')
        self.assertNotIn('I am carries', out['greeting'])

    def test_the_citys_figures_keep_their_own_text(self):
        lord = dict(self.PERSON, description='A lean grey timber wolf with a missing claw, Lord of House Testwick.')
        self.assertEqual(PP.person_text(lord, {'post': 'staff', 'record': 'r'}, 'clerking at the City Office', 'x', {}),
                         ({}, None))
        child = dict(self.PERSON, age=12)
        self.assertEqual(PP.person_text(child, {'post': 'help', 'record': 'r'}, 'helping at X', 'y', {}), ({}, None))


class Refit(unittest.TestCase):
    def test_a_shop_refitted_for_another_trade_keeps_its_walls_door_and_stairs(self):
        from worldgen import ridgemere2 as R
        p = B.shop(random.Random(7), 'ridgemere', 'tinker')
        p.g[3][1] = '^'
        room = {'terrain': [''.join(r) for r in p.g]}
        before = room['terrain']
        self.assertTrue(R.refit_shop(room, 'baker'))
        once = room['terrain']
        R.refit_shop(room, 'baker')
        self.assertEqual(room['terrain'], once)                # Twice is once.
        self.assertIn('v', once[1])                             # The ovens along the back.
        for y, (a, b) in enumerate(zip(before, once)):
            for x, (ca, cb) in enumerate(zip(a, b)):
                if ca in '#+^=':
                    self.assertEqual(ca, cb, (x, y))


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
        fixed = {pid for pid, _ in self.m['city'].text_fixed}
        for x in p['people']:
            b = before[x['id']]
            for key in ('name', 'description', 'backstory', 'personality', 'greeting', 'appearance', 'age', 'voice'):
                if key in PP.ROYAL_TEXT.get(x['id'], {}):
                    self.assertEqual(x[key], PP.ROYAL_TEXT[x['id']][key], (x['id'], key))   # The Crown's own text.
                elif x['id'] in fixed and key in ('description', 'greeting', 'backstory'):
                    # Written again for the post (the food rebalance): the looks and the old backstory stay.
                    if key == 'description':
                        self.assertEqual(PP.split_description(x[key])[0], PP.split_description(b[key])[0])
                    if key == 'backstory':
                        self.assertTrue(x[key].startswith(b[key].rstrip()), x['id'])
                else:
                    self.assertEqual(x[key], b[key], (x['id'], key))
        self.assertTrue(all(x['home'] for x in p['people'] if x['id'] in fixed))     # (Only the placed.)

    # -- The food rebalance (doc 67, Progress, "Food rebalance") --
    def test_the_food_trades_and_the_carters(self):
        from worldgen import ridgemere2 as R
        cap = self.m['capacity']['after']
        self.assertTrue(cap['ok'], cap['food'])
        self.assertLessEqual(cap['foodBusinesses'] - cap['businesses'].get('stall', 0), 56)
        self.assertGreaterEqual(cap['foodBusinesses'], 45)
        self.assertLessEqual(cap['householdBusinesses'], 15)
        people = self.m['project']['people']
        self.assertEqual(sum(1 for x in people if x['workLabel'] == 'driving for the carters'), PP.CARTERS)
        rooms = {r['id']: r for r in self.m['project']['rooms']}
        for rid, (trade, name, text) in R.REFIT.items():
            self.assertEqual(rooms[rid]['description'], text)
            if name:
                self.assertEqual(rooms[rid]['name'], name)

    def test_placed_text_fits(self):
        import re
        for x in self.m['project']['people']:
            if not x['home']:
                continue
            self.assertIsNone(re.search(r"I am [a-z]+s ", x['greeting']), (x['id'], x['greeting']))
            self.assertNotIn('Heights Market', x['description'], x['id'])
            if 'Records Office' in x['workLabel']:
                self.assertTrue(any(w in x['description'] for w in ('clerk', 'copyist', 'rolls', 'record')), x['id'])

    def test_the_king_reigns_at_the_great_palace(self):
        people = {x['id']: x for x in self.m['project']['people']}
        king = people['rm_gunnar_harrowgate']
        self.assertEqual(king['workLabel'], 'reigning at the Great Palace')
        self.assertEqual(king['work']['cell'], 'r2_the_antechamber')
        self.assertNotIn('merchant', king['description'])
        for pid in ('rm_oda_harrowgate', 'rm_ottilie_harrowgate'):
            self.assertEqual(people[pid]['work']['cell'], 'r2_the_king_s_residence_up')
            self.assertFalse(PP.label_problems(people[pid]['workLabel']), people[pid]['workLabel'])
            self.assertNotIn('Heights', people[pid]['greeting'])
        for pid in ('rm_gunnar_harrowgate', 'rm_oda_harrowgate', 'rm_ottilie_harrowgate'):
            self.assertTrue(people[pid]['home']['cell'].startswith('r2_the_king_s_residence'))

    def test_the_records_office_and_the_couriers_office(self):
        p = self.m['project']
        rooms = {r['id']: r for r in p['rooms']}
        self.assertEqual(rooms['rm_the_hall_of_records']['name'], "The Couriers' Office")
        door = next(l for l in p['links'] if l['id'] == 'rm_the_hall_of_records_door')
        self.assertEqual(door['name'], "The Couriers' Office door")
        at_hall = [x['workLabel'] for x in p['people'] if x['work'] and x['work']['cell'] == 'rm_the_hall_of_records']
        self.assertEqual(at_hall, ["carrying for the Couriers' Office"] * 3)
        records = [x for x in p['people'] if x['work'] and x['work']['cell'].startswith('r2_the_records_office')]
        self.assertEqual(len(records), 7)
        self.assertEqual({x['work']['cell'] for x in records if 'rolls' in x['workLabel']}, {'r2_the_records_office_top'})

    def test_six_stalls_in_the_rag_market(self):
        c = self.m['city'].c
        keepers = self.m['city'].posts['rag_market']
        self.assertEqual(len(keepers), 6)
        for x, y in keepers:
            self.assertTrue(any(c.at(x + dx, y + dy) == 'u' for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0))), (x, y))

    def test_the_houses_claim_their_forges_and_wharf(self):
        claims = {r['id']: r['territory']['claims'] for r in self.m['project']['rooms']}
        for rid in ('rm_the_anchor_forge', 'rm_nailers_forge', 'rm_the_rivet_and_rain', 'r2_timberland_s_forge_row'):
            self.assertEqual(claims[rid], ['house_mossgrave'], rid)
        for rid in ('rm_the_harbourmaster_s_office', 'rm_the_ropewalk', 'r2_the_grayrock_new_store',
                    'r2_grayrock_shipwrights'):
            self.assertEqual(claims[rid], ['house_grayrock'], rid)
        self.assertEqual(claims['r2_the_keep'], ['council_of_houses', 'ridgemere_watch'])

    def test_the_old_interiors_stay(self):
        ids = {r['id'] for r in self.m['project']['rooms']}
        self.assertTrue({r['id'] for r in self.m['base']['rooms']} <= ids)
        self.assertTrue(all(r['id'].startswith('r2_') for r in self.m['city'].site.rooms))

    def test_it_is_made_the_same_each_time(self):
        rec = self.m['city'].records
        self.assertGreater(len(rec), 200)

    # -- The dense city (doc 67, Progress, "Dense city") --
    def test_no_walls_between_the_districts(self):
        from worldgen import ridgemere2 as R
        city = self.m['city']
        d = R.district_metrics(city, self.m['walk'])
        self.assertEqual(d['wallsOnBorders'], 0)
        self.assertEqual(d['badSteps'], [])
        # Wall glyphs inside the walls are buildings' own walls or the walled pieces' (the Keep, the abbey, Greatly).
        inside = R.box(*R.INSIDE)
        wall = np.isin(city.c.codes, [ord(ch) for ch in '#H']) & inside
        self.assertFalse((wall & ~city.built & ~city.rings).any())
        self.assertFalse(city.edges.any())

    def test_districts_blend(self):
        from worldgen import ridgemere2 as R
        d = R.district_metrics(self.m['city'], self.m['walk'])
        self.assertGreaterEqual(d['crossings'].get('commons/sump', 0), 8)
        self.assertGreaterEqual(d['openBorder'].get('commons/sump', 0), 60)
        self.assertGreaterEqual(d['openBorder'].get('commons/rise', 0), 30)
        for name in ('sump', 'commons', 'rise', 'admin'):
            self.assertLess(d['districts'][name]['fill'], .9, name)

    def test_homes_three_at_most_and_the_sump_full(self):
        city, p = self.m['city'], self.m['project']
        self.assertLessEqual(max(r.get('homes', 0) for r in city.records), 3)
        room_struct = {room['id']: r for r in city.records for room in r['rooms']}
        lived = {}
        for x in p['people']:
            if x['home']:
                rec = room_struct.get(x['home']['cell'])
                if rec:
                    lived.setdefault(rec['id'], set()).add(x['home']['cell'])
        self.assertLessEqual(max(len(v) for v in lived.values()), 3)
        sump = [len(lived[r['id']]) for r in city.records if r.get('sump_home') and r['id'] in lived]
        self.assertGreaterEqual(sum(1 for n in sump if n == 3), .8 * len(sump))

    def test_the_city_is_compact(self):
        from worldgen import ridgemere2 as R
        d = R.district_metrics(self.m['city'], self.m['walk'])
        self.assertLess(d['insideArea'], .65 * 225456)
        self.assertGreater(d['builtInside'], .3)

    def test_the_landmarks(self):
        names = {lm['name'] for lm in self.m['city'].landmarks}
        for name in ('The Bay Light and the Mole', 'The Clock Tower', 'The Wall-Builders', 'Pillory Square', 'The Mercy',
                     'The Cisterns and the Runnel', 'The Hundred Steps', 'The Sunken Street', 'The Tallow Arcade'):
            self.assertIn(name, names)
        rooms = {r['id'] for r in self.m['project']['rooms']}
        for rid in ('r2_the_bay_light', 'r2_the_bay_light_up', 'r2_the_clock_tower_up', 'r2_the_sunken_street',
                    'r2_the_tallow_arcade', 'r2_the_knucklebones'):
            self.assertIn(rid, rooms)
        links = [l for l in self.m['project']['links'] if 'r2_the_sunken_street' in (l['a']['cell'], l['b']['cell'])]
        self.assertEqual(len(links), 2)                      # A stair-house at each end.


if __name__ == '__main__':
    unittest.main()
