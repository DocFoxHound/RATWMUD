"""tools/dialect_check.py (Docs/Design/83-dialogue-trees.md, Phase 4): each rule on a small library of its own, and the
real library passing it.

    cd tools && python3 -m unittest test_dialect_check
"""
import json
import shutil
import tempfile
import unittest
from pathlib import Path

import dialect_check as DC

PLAIN = """grammar plain
greet = Good day | Hello
farewell = Farewell
address = friend | neighbour
shop_offer = What'll it be?
"""
FENS = """grammar mirebridge
greet = Dry feet | Dry feet to you
farewell = Dry feet
address = fen-friend
shop_offer = What'll it be, fen-friend?
"""
LAKE = """grammar coldwater
greet = Still water
address = shore-friend
"""
SETTINGS = {"dialect_of": {"fenshore": "mirebridge", "saltwell": "coldwater"}}
GROUPS = "group towns = coldwater mirebridge fenshore saltwell\ngroup fens = mirebridge fenshore\n"


class Library:
    """A small Data/Talk of its own, in a temporary folder."""

    def __init__(self, talk: str = '', tree: str = '', grammars=(PLAIN, FENS, LAKE)):
        self.root = Path(tempfile.mkdtemp(prefix='dialect-check-'))
        (self.root / 'grammar').mkdir()
        (self.root / 'dialogue').mkdir()
        for text in grammars:
            name = text.split('\n', 1)[0].split()[1]
            (self.root / 'grammar' / f'{name}.grammar').write_text(text)
        (self.root / 'settings.json').write_text(json.dumps(SETTINGS))
        (self.root / 'groups.talk').write_text(GROUPS)
        (self.root / 'rules').mkdir()
        (self.root / 'rules' / 'lines.talk').write_text(talk)
        if tree:
            (self.root / 'dialogue' / 'test.tree').write_text(tree)
        self.allow = self.root / 'allow.json'
        self.allow.write_text(json.dumps({'erased': ['mirebridge'], 'ordinary': [], 'marked': {}, 'lines': []}))

    def run(self, people=None):
        report, summary = DC.run(root=self.root, allow_path=self.allow, people=people, render=False)
        return report

    def __del__(self):
        shutil.rmtree(self.root, ignore_errors=True)


class Lexicon(unittest.TestCase):
    def setUp(self):
        self.lib = Library()

    def test_distinctive_phrases(self):
        lex = DC.lexicon(DC.grammars(self.lib.root), {})
        fens = dict(lex['mirebridge'])
        self.assertIn('fen-friend', fens)                     # (Coined, with a hyphen.)
        self.assertTrue(fens['dry feet to you'])              # (A greeting: a saying.)
        self.assertNotIn("what'll it be", fens)              # (Plain says it too.)

    def test_a_dialects_word_in_anyones_mouth_fails(self):
        lib = Library('rule a\nwhen concept=bark\nsay Dry feet to you, neighbour.\n')
        self.assertTrue(lib.run().failures['lexicon'])

    def test_gated_to_its_speakers_passes(self):
        lib = Library('rule a\nwhen concept=bark from=mirebridge|fenshore\nsay Dry feet to you, neighbour.\n'
                      'scene b\nwhen topic=lore b.from=fenshore\na : Hello.\nb : Dry feet, fen-friend.\n'
                      'scene c\nwhen topic=lore\na : Hello.\nb[from=mirebridge] : Dry feet.\n')
        self.assertFalse(lib.run().failures.get('lexicon'))

    def test_a_gate_with_others_in_it_fails(self):
        lib = Library('rule a\nwhen concept=bark from=mirebridge|coldwater\nsay Dry feet.\n')
        self.assertTrue(lib.run().failures['lexicon'])

    def test_a_saying_counts_only_standing_alone(self):
        lex = DC.lexicon(DC.grammars(self.lib.root), {})
        self.assertEqual(DC.hits('Dry feet, friend.', lex), [('mirebridge', 'dry feet')])
        self.assertEqual(DC.hits('Keep your dry feet.', lex), [])
        self.assertEqual(DC.hits('Still water hides things.', lex), [])
        self.assertEqual(DC.hits('#greet#, #address#.', lex), [])        # (Symbols are the grammar's business.)

    def test_a_players_choice_never_has_a_dialect(self):
        tree = 'tree t\nentry talk a\nnode a\nsay #greet#.\nchoice Dry feet to you. -> end\n'
        self.assertTrue(Library(tree=tree).run().failures['lexicon'])

    def test_the_allowlist(self):
        lib = Library('rule a\nwhen concept=bark\nsay Dry feet, they say there.\n')
        lib.allow.write_text(json.dumps({'erased': [], 'lines': [{'phrase': 'dry feet', 'file': 'rules/lines.talk',
                                                                     'text': 'they say there', 'why': 'quoted'}]}))
        self.assertFalse(lib.run().failures.get('lexicon'))


class Trees(unittest.TestCase):
    def test_a_trees_symbol_missing_from_a_dialect(self):
        tree = 'tree t\nentry talk a\nnode a\nsay #greet#. #shop_offer#\nchoice Bye. -> end\n'
        failures = Library(tree=tree).run().failures['symbols']
        self.assertEqual(len(failures), 1)
        self.assertIn('#shop_offer#', failures[0])
        self.assertIn('coldwater', failures[0])


class Reach(unittest.TestCase):
    def test_lines_for_an_erased_town_never_play(self):
        lib = Library('scene a\nwhen topic=lore here.region=mirebridge\na : Hm.\n'
                      'scene b\nwhen topic=lore here.region=fens\na : Hm.\n'
                      'scene c\nwhen topic=lore here.region=wild\na : Hm.\n')
        failures = lib.run(people=[{'_home': 'fenshore', 'from': 'fenshore'}]).failures['reach']
        self.assertEqual(len(failures), 1)
        self.assertIn(' a is for mirebridge', failures[0])


class Origins(unittest.TestCase):
    def people(self, town, local, others, other_from='coldwater'):
        return [{'_home': town, 'from': town}] * local + [{'_home': town, 'from': other_from}] * others

    def test_shares(self):
        report = DC.Report()
        out = DC.check_origins(report, self.people('fenshore', 7, 3) + self.people('ridgemere', 6, 4), SETTINGS['dialect_of'])
        self.assertFalse(report.failures)
        self.assertEqual(out['fenshore']['share'], .7)
        report = DC.Report()
        DC.check_origins(report, self.people('coldwater', 6, 4, 'larkfield'), {})
        self.assertIn('coldwater: 6 of 10', report.failures['origins'][0])

    def test_a_town_that_speaks_anothers(self):
        report = DC.Report()
        out = DC.check_origins(report, self.people('saltwell', 5, 5, 'coldwater'), SETTINGS['dialect_of'])
        self.assertEqual(out['saltwell']['speak_its_dialect'], 10)     # (Coldwater folk speak Saltwell's speech.)

    def test_a_town_with_no_dialect(self):
        report = DC.Report()
        DC.check_origins(report, self.people('fenshore', 9, 1), {})
        self.assertIn('no dialect of its own', report.failures['origins'][0])


class Tables(unittest.TestCase):
    def setUp(self):
        self.lib = Library()

    def test_a_character_saying_anothers_words(self):
        lex = DC.lexicon(DC.grammars(self.lib.root), {})
        report = DC.Report()
        DC.check_tables(report, [('rm_a', 'coldwater', 'greet = Dry feet to you', ''),
                                 ('rm_b', 'fenshore', 'greet = Dry feet to you', '')], lex, SETTINGS['dialect_of'])
        self.assertEqual(len(report.failures['tables']), 1)
        self.assertIn('rm_a', report.failures['tables'][0])


class TheRealLibrary(unittest.TestCase):
    def test_passes(self):
        report, summary = DC.run(render=False)
        self.assertFalse(report.failures, json.dumps(report.failures, indent=1)[:3000])
        self.assertGreater(summary['lines_read'], 50000)


if __name__ == '__main__':
    unittest.main()
