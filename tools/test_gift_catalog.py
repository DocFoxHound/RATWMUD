"""Tests for the Gift catalog check (Docs/Design/43-gifts.md)."""
import copy
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gift_catalog as G


class GiftCatalog(unittest.TestCase):
    def setUp(self):
        self.doc = json.loads(G.PATH.read_text(encoding='utf-8'))

    def test_the_catalog_is_whole(self):
        self.assertEqual(G.problems(self.doc), [])

    def test_a_missing_tell_or_a_bad_kind_is_found(self):
        doc = copy.deepcopy(self.doc)
        del doc['families'][0]['gifted']['tell']
        doc['families'][1]['quickened']['abilities'][0]['kind'] = 'spell'
        found = G.problems(doc)
        self.assertIn('fire gifted: no tell', found)
        self.assertTrue(any('unknown kind' in p for p in found))

    def test_a_gifted_ability_never_deals_damage(self):
        doc = copy.deepcopy(self.doc)
        doc['families'][0]['gifted']['abilities'][0]['kind'] = 'gathered'
        self.assertTrue(any('deal no damage' in p for p in G.problems(doc)))

    def test_death_walkers_stay_npcs(self):
        doc = copy.deepcopy(self.doc)
        for f in doc['families']:
            f.pop('npcOnly', None)
        found = G.problems(doc)
        self.assertTrue(any('playable families' in p for p in found))


if __name__ == '__main__':
    unittest.main()
