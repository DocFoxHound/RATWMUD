#!/usr/bin/env python3
"""The item catalogue's Python side (tools/item_catalog.py): the catalogue is sound; business_for agrees with Core's
items::businessFor (Docs/Design/67-ridgemere2-city.md, 4) on Tests/fixtures/business_labels.json's labels, which
Tests/shops_tests.cpp reads too, so the two cannot drift. (The store room's store_goods, store_price and store_cost went
with the studio's store-room view in doc 79's Phase 9.)"""
import json
import unittest
from pathlib import Path

import item_catalog as C

FIXTURE = Path(__file__).resolve().parent.parent / 'Tests' / 'fixtures' / 'business_labels.json'


class CatalogueTests(unittest.TestCase):
    def test_the_catalogue_is_sound(self):
        problems, made = C.check()
        self.assertEqual(problems, [])
        for item in ('brandy', 'apple_brandy', 'berry_cordial', 'fruit_preserves', 'dried_apples', 'aqua_vitae'):
            self.assertTrue(made.get(item), f'{item}: something makes it')

    def test_the_new_trades(self):
        for business in ('builders_works', 'distillery', 'preserver', 'vintner', 'shipwright', 'laundry', 'bathhouse', 'barber',
                         'pawnbroker', 'rag_and_bone'):
            self.assertIn(business, C.BUSINESSES)
        order = list(C.BUSINESSES)
        self.assertLess(order.index('builders_works'), order.index('smithy'))
        for business in ('distillery', 'preserver', 'vintner', 'shipwright', 'laundry', 'bathhouse', 'barber', 'pawnbroker', 'rag_and_bone'):
            self.assertLess(order.index(business), order.index('inn'), business)
        self.assertTrue(C.BUYS['pawnbroker'])
        for craft in C.CRAFTS:
            self.assertLessEqual(len(craft['in']), 2, craft['id'])
        brandy, aqua = C.ITEMS['brandy']['price'], C.ITEMS['aqua_vitae']['price']
        self.assertGreater(brandy, 4 * aqua, 'two wine make one brandy, dearer than the four aqua vitae they would make')


class SharedFixtureTests(unittest.TestCase):
    def setUp(self):
        self.fixture = json.loads(FIXTURE.read_text(encoding='utf-8'))

    def test_labels_resolve_as_the_game_resolves_them(self):
        self.assertGreater(len(self.fixture['labels']), 20)
        for row in self.fixture['labels']:
            self.assertEqual(C.business_for(row['label']), row['business'], row['label'])

    def test_the_store_room_s_helpers_are_gone(self):
        # Doc 79, 12 and Phase 9: restocking is free, so nothing reckons a draw.
        for name in ('store_goods', 'store_price', 'store_cost'):
            self.assertFalse(hasattr(C, name), name)
        self.assertNotIn('keepers', self.fixture)


if __name__ == '__main__':
    unittest.main()
