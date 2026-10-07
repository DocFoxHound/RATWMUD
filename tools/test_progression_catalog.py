"""Tests for the progression catalog check (Docs/Design/49-characters-and-earned-gifts.md)."""
import copy
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import progression_catalog as P


class ProgressionCatalog(unittest.TestCase):
    def setUp(self):
        self.doc = json.loads(P.SKILLS.read_text(encoding='utf-8'))

    def test_the_catalog_is_whole(self):
        self.assertEqual(P.problems(self.doc), [])

    def test_a_cap_below_its_start_is_found(self):
        doc = copy.deepcopy(self.doc)
        doc['skills'][0]['cap'] = doc['skills'][0]['start']
        self.assertIn(f"{doc['skills'][0]['id']}: its cap is no higher than its start", P.problems(doc))

    def test_a_source_naming_a_missing_skill_is_found(self):
        doc = copy.deepcopy(self.doc)
        doc['sources'][0]['grows'] = {'charm': 0.5}
        self.assertTrue(any("unknown skill 'charm'" in p for p in P.problems(doc)))

    def test_an_unknown_field_is_found(self):
        doc = copy.deepcopy(self.doc)
        doc['attributes'][0]['field'] = 'muscle'
        self.assertTrue(any("unknown field 'muscle'" in p for p in P.problems(doc)))

    def test_a_rising_partner_decay_is_found(self):
        doc = copy.deepcopy(self.doc)
        doc['partnerDecay'] = [1, 0.5, 0.75]
        self.assertTrue(any(p.startswith('partnerDecay') for p in P.problems(doc)))

    def test_a_growth_line_needs_its_value(self):
        doc = copy.deepcopy(self.doc)
        doc['skills'][1]['line'] = 'You move more quietly.'
        self.assertTrue(any('growth line' in p for p in P.problems(doc)))

    def test_standing_is_whole(self):
        self.assertEqual(P.standing_problems(), [])

    def test_titles_must_start_at_level_one(self):
        doc = json.loads(P.STANDING.read_text(encoding='utf-8'))
        doc['titles'] = doc['titles'][1:]
        self.assertTrue(any(p.startswith('titles') for p in P.standing_problems(doc)))

    def test_creation_is_whole(self):
        self.assertEqual(P.creation_problems(), [])

    def test_a_preset_over_budget_is_found(self):
        doc = json.loads(P.CREATION.read_text(encoding='utf-8'))
        doc['presets'][0]['strong'] = ['strength', 'dexterity', 'wisdom']
        doc['presets'][0]['weak'] = []
        self.assertTrue(any('over the budget' in p for p in P.creation_problems(doc)))


if __name__ == '__main__':
    unittest.main()
