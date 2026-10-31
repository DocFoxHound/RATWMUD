#!/usr/bin/env python3
"""tools/rename_house.py (doc 67, section 7): the rules, whole words, idempotence, the labels' limit, and the worlds
it refuses. No database: the dry run on Ridgemere 2.0 is reviewed as artifacts/ridgemere2/rename-report.txt."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import rename_house as R  # noqa: E402
import world_db  # noqa: E402

OLD = R.OLD


class Rules(unittest.TestCase):
    def test_the_house_by_whole_word(self):
        self.assertEqual(R.rewrite(f'Lady of House {OLD}.'), 'Lady of House Timberland.')
        self.assertEqual(R.rewrite(f"{OLD}'s own"), "Timberland's own")
        self.assertEqual(R.rewrite(f'The {OLD}s own the forges'), 'The Timberlands own the forges')
        self.assertEqual(R.rewrite(f'keeping {OLD} Court'), 'keeping Timberland Court')
        # Ids keep the name: it is part of a longer word there.
        for ident in ('rm_halvard_mossgrave', 'house_mossgrave', 'rm_mossgrave_court_up', 'mossgrave_court'):
            self.assertEqual(R.rewrite(ident), ident)

    def test_fell_alone_is_never_touched(self):
        for text in ('Fell once got lost in the deep wood.', 'Fellside Logging Camp', 'Oakfell', 'ruling House Fell',
                     'keeping the Fell accounts', 'This is Fell land.'):
            self.assertEqual(R.rewrite(text), text)

    def test_the_trades(self):
        self.assertEqual(R.rewrite('works at The Grayrock Ironworks'), 'works at The Timberland Ironworks')
        self.assertEqual(R.rewrite('a hand at the Fell Sawmill.'), 'a hand at the Timberland Sawmill.')
        self.assertEqual(R.rewrite('selling Fell cedar'), 'selling Fell wine')
        self.assertEqual(R.rewrite('buying Grayrock iron'), 'buying Timberland iron')
        self.assertEqual(R.rewrite(f'trading {OLD} grey'), 'trading Timberland iron')
        self.assertEqual(R.rewrite('smith at Nailers\' Forge'), "smith at Timberland's Nailers' Forge")
        self.assertEqual(R.rewrite('smith at The Anchor Forge'), "smith at Timberland's Anchor Forge")
        self.assertEqual(R.rewrite('helping at Southgate Farriers'), 'helping at Eastgate Farriers')
        self.assertEqual(R.rewrite("keeping the harbour at The Harbourmaster's Office"),
                         'keeping the harbour at The Grayrock Wharf Office')
        self.assertEqual(R.rewrite('Bo dreams of a berth on a Greatly ship bound somewhere dry.'),
                         'Bo dreams of a berth on a Grayrock ship bound somewhere dry.')
        self.assertEqual(R.rewrite('in every port the Greatly ships reach'), 'in every port the Grayrock ships reach')
        self.assertEqual(R.rewrite('Bo lost a brother to the quarry and does not forgive House Grayrock for it.'),
                         'Bo lost a brother on the wharf and does not forgive House Grayrock for it.')
        self.assertEqual(R.rewrite('Bo has a cousin in the Fell logging camps who writes when he can.'),
                         'Bo has a cousin in the Fell vineyards who writes when he can.')
        self.assertEqual(R.rewrite(f'Bo grew up on a {OLD} sheep run in the Folds and still misses the quiet.'),
                         'Bo grew up at a Timberland charcoal camp in the hills and still misses the quiet.')
        self.assertEqual(R.rewrite('burns charcoal at the Charcoal Store'), 'working the Timberland Charcoal Store')

    def test_wolves_not_dogs(self):
        self.assertEqual(R.rewrite('the smell of wet dogs.'), 'the smell of wet wolves.')
        self.assertEqual(R.rewrite('its roof green with moss and a dog asleep in the door.'),
                         'its roof green with moss and a pup asleep in the door.')
        self.assertEqual(R.rewrite('three dogs and a dog'), 'three wolves and a wolf')
        self.assertEqual(R.rewrite('Dogwood Lane, a hotdog'), 'Dogwood Lane, a hotdog')

    def test_every_rule_is_idempotent(self):
        samples = [r for _, r in R.PRE + R.NAMES + R.PHRASES] + list(R.INTERIOR_DESCRIPTIONS.values())
        samples += [after for edits in R.NPC_EDITS.values() for pairs in edits.values() for _, after in pairs]
        samples += ["helping at Timberland's Nailers' Forge", "Timberland's Anchor Forge, the flat above"]
        for text in samples:
            text = text.replace('\\1', 'T')
            self.assertEqual(R.rewrite(text), text, text)
            self.assertEqual(R.rewrite(R.rewrite(text)), R.rewrite(text))

    def test_written_labels_fit(self):
        for edits in R.NPC_EDITS.values():
            for old, new in edits.get('work_label', []):
                self.assertLessEqual(len(R.rewrite(new)), R.LABEL_MOST, new)
        for label in ('works at The Grayrock Bonded Warehouse', 'helping at Nailers\' Forge',
                      'works at The Charcoal Store', 'runs The Greatly Bonded Warehouse'):
            self.assertLessEqual(len(R.rewrite(label)), R.LABEL_MOST, R.rewrite(label))

    def test_no_new_label_breaks_a_label_rule(self):
        # Doc 67, section 4: the words that make a label resolve to the wrong business, or to a House.
        bad = ('keeps the ', 'feeding the ', 'cooking for', ' press', 'forge', 'armour', 'armor', 'bank', 'stall',
               'stables', ' pit', 'miner', 'the workings', 'burns charcoal', 'vats', 'holding court')
        for edits in R.NPC_EDITS.values():
            for _, new in edits.get('work_label', []):
                self.assertFalse([w for w in bad if w in new.lower()], new)


class Worlds(unittest.TestCase):
    def test_dev_and_prod_are_refused(self):
        for key in ('dev', 'prod'):
            with self.assertRaises(R.Refused):
                R.refuse_world(key)

    def test_a_live_world_is_refused(self):
        for key in world_db.keys('live'):
            with self.assertRaises(R.Refused):
                R.refuse_world(key)

    def test_the_command_refuses_dev_without_connecting(self):
        self.assertEqual(R.main(['--dry-run', '--world', 'dev']), 1)
        self.assertEqual(R.main(['--apply', '--world', 'prod']), 1)

    def test_ridgemere2_is_allowed(self):
        if 'ridgemere2' in world_db.keys():
            self.assertEqual(R.refuse_world('ridgemere2')['key'], 'ridgemere2')


if __name__ == '__main__':
    unittest.main()
