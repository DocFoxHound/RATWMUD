"""Ridgemere and Ser Ferro: the building kit, the residents kit, who holds what, and (with RATW_SLOW=1, about five
minutes) the whole western world built with both cities and their people."""
import os
import random
import unittest

import terrain_catalog
from worldgen import buildings as B
from worldgen import citizens as C
from worldgen import western as W


class Kit(unittest.TestCase):
    def test_every_new_building_has_a_door_beds_and_work_where_it_should(self):
        rng = random.Random(1)
        for kind in B.WORKS:
            p = B.works(rng, 'works', kind, 4)
            self.assertTrue(p.work, kind)
            self.assertEqual(p.g[p.h - 1][p.w // 2], '+', kind)
        lower, upper, below, stairs = B.manor(rng, 'manor', 5, 4)
        self.assertEqual((len(upper.beds), len(below.beds)), (5, 4))
        self.assertEqual([pair[1][0] for pair in stairs], ['up', 'down'])
        court, royal, kitchens, stairs = B.palace(rng, 'palace', 6, 10)
        self.assertEqual((len(royal.beds), len(kitchens.beds)), (6, 10))
        self.assertEqual(sum(row.count('e') for row in court.g), 2, 'twin thrones')
        nave, crypt, stairs = B.cathedral(rng, 'palace')
        self.assertIn('a', ''.join(''.join(r) for r in nave.g), 'an altar')
        self.assertEqual(len(B.villa(rng, 'serferro', 4, 2)[1].beds), 4)
        self.assertEqual(len(B.tenement(rng, 'ridgemere', 6).beds), 6)

    def test_every_tile_the_kit_draws_is_in_the_catalog(self):
        rng = random.Random(2)
        plans = [B.works(rng, 'works', k, 4) for k in B.WORKS] + list(B.manor(rng, 'manor', 5, 4)[:3]) + \
            list(B.palace(rng, 'palace', 6, 10)[:3]) + list(B.cathedral(rng, 'palace')[:2]) + \
            list(B.villa(rng, 'serferro', 4, 2)[:2]) + [B.tenement(rng, 'ridgemere', 6)]
        for p in plans:
            for row in p.g:
                for ch in row:
                    self.assertIn(ch, terrain_catalog.GLYPHS)
        for style in ('ridgemere', 'works', 'manor', 'serferro', 'palace'):
            for key in ('wall', 'floor', 'home_floor', 'outer'):
                self.assertIn(B.STYLES[style][key], terrain_catalog.GLYPHS, style)
            self.assertIn(B.STYLES[style]['roof'][0], terrain_catalog.GLYPHS, style)


class Holdings(unittest.TestCase):
    def test_who_holds_what(self):
        claims = W.claims_for
        self.assertEqual(claims({'name': 'Grayrock Hall', 'district': 'estate_grayrock_seat', 'city': 'ridgemere',
                                 'kind': 'manor'}), ['house_grayrock'])
        self.assertEqual(claims({'name': 'The Brinewater Yard', 'district': 'quay_south', 'city': 'ridgemere',
                                 'kind': 'works'}), ['house_brinewater'])
        self.assertEqual(claims({'name': 'The Watch House', 'district': 'government', 'city': 'ridgemere',
                                 'kind': 'barracks'}), ['council_of_houses', 'ridgemere_watch'])
        self.assertEqual(claims({'name': 'The Cathedral of the Iron Saint', 'district': 'rise', 'city': 'ser_ferro',
                                 'kind': 'cathedral'}), ['church_iron_saint'])
        self.assertEqual(claims({'name': 'The Palace of Ser Ferro', 'district': 'palace', 'city': 'ser_ferro',
                                 'kind': 'palace'}), ['crown_of_ser_ferro'])
        known = {f['id'] for f in C.FACTIONS}
        self.assertTrue({'house_grayrock', 'house_fell', 'house_ashcombe', 'house_brinewater', 'house_vesk'} <= known)

    def test_family_names_come_from_the_house(self):
        self.assertEqual(C.family_of({'name': 'Bellandi House', 'kind': 'house'}), 'Bellandi')
        self.assertIsNone(C.family_of({'name': 'Soot Row', 'kind': 'tenement'}))


@unittest.skipUnless(os.environ.get('RATW_SLOW') and W.DRAWING.exists(), 'set RATW_SLOW=1 to build the whole world')
class WholeWorld(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import json
        blank = {'cells': [], 'rooms': [], 'links': [], 'people': [], 'routes': [], 'factions': [],
                 'format': 'ratw-atlas', 'version': 3, 'id': 'test', 'name': 'Test', 'spawn': None}
        cls.world = W.Western({'heights': __import__('numpy').zeros((W.UA[3], W.UA[2]), dtype='float32'),
                               'codes': __import__('numpy').full((W.UA[3], W.UA[2]), ord(','), dtype='uint8')})
        cls.world.build()
        cls.cells = cls.world.cells()
        cls.project = W.assemble(blank, cls.world, cls.cells)
        # A bare world has no spawn: put new characters on Ridgemere's market square.
        cls.project['spawn'] = next(p['work'] for p in cls.project['people']
                                    if p['id'].startswith('rm_') and p['work']['cell'].startswith('w_'))

    def test_the_project_is_valid(self):
        import map_editor
        map_editor.check_project(self.project, for_game=False)

    def test_each_city_has_its_people(self):
        people = self.project['people']
        self.assertGreaterEqual(sum(p['id'].startswith('rm_') for p in people), 120)
        self.assertGreaterEqual(sum(p['id'].startswith('sf_') for p in people), 120)
        by_name = {p['name']: p for p in people}
        self.assertEqual(by_name['Aldric Grayrock']['route'], 'rm_grayrock_journey', 'the lord is always travelling')
        self.assertEqual(by_name['Maren Grayrock']['route'], '', 'the lady stays at home')
        self.assertTrue(by_name['Maren Grayrock']['work']['cell'].startswith('rm_grayrock_hall'))
        self.assertIn('Aurelio di Castellane', by_name)

    def test_ridgemere_walls_hold_the_old_city_and_the_houses_hold_land_outside(self):
        import numpy as np
        places = self.world.places
        core = places['ridgemere']
        for record in self.world.city_site.manifest:
            if record.get('city') != 'ridgemere':
                continue
            x, y = record['door']['x'], record['door']['y']
            if record['district'] in ('government', 'sump', 'market'):
                self.assertTrue(core[y, x], record['name'])
            if record['district'].startswith('estate_'):
                estate = places['_'.join(record['district'].split('_')[:2])]
                self.assertTrue(estate[y, x] and not core[y, x], record['name'])

    def test_ser_ferro_climbs_a_step_at_a_time_to_the_palace(self):
        import numpy as np
        city = next(c for c in self.world.cities if c.region == 'ser_ferro')
        tier = city.tier
        inside = tier >= 0
        for dy, dx in ((1, 0), (0, 1)):
            a, b = tier[:-dy or None, :-dx or None], tier[dy:, dx:]
            both = (a >= 0) & (b >= 0)
            self.assertLessEqual(int(np.abs(a - b)[both].max()), 1)
        palace = next(r for r in self.world.city_site.manifest if r['kind'] == 'palace')
        self.assertEqual(tier[palace['door']['y'], palace['door']['x']], 4)
