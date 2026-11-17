"""Phase 7's tools (Docs/Design/84 §18, P7): the marker conversion, the backchannel migration, the tagger and the
equivalence test, each on a small library of its own (ctest talk_p7_rules)."""
from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import talk_conv
import talk_equiv
import talk_migrate
import talk_tag


def library(files: dict[str, str]) -> Path:
    root = Path(tempfile.mkdtemp(prefix='ratw-p7-'))
    for name, text in files.items():
        (root / name).parent.mkdir(parents=True, exist_ok=True)
        (root / name).write_text(text)
    return root


class Conversion(unittest.TestCase):
    def test_a_short_pair_marker_becomes_a_tag_on_every_alternative_and_its_reads_conv(self):
        root = library({'rules/a.talk': 'rule t1\nwhen concept=talk.friends\nsay Nice hat. | {x=1} Bold coat.\n'
                                        'remember pair: jk=look 30m\nthen other: jest.answer\n\n'
                                        'rule t2\nwhen concept=jest.answer pair.mem.jk=look\nsay Ha.\n'})
        lib = talk_conv.Library(root)
        result = lib.convert()
        lib.write()
        text = (root / 'rules/a.talk').read_text()
        self.assertEqual(result['stats']['markers'], 1)
        self.assertIn('say {jk=look} Nice hat. | {x=1 jk=look} Bold coat.', text)
        self.assertIn('when concept=jest.answer conv.jk=look', text)
        self.assertNotIn('remember pair: jk', text)

    def test_a_key_also_kept_for_days_stays_memory(self):
        root = library({'rules/a.talk': 'rule t1\nwhen concept=talk.day\nsay Hi.\nremember pair: nb=ask 30m\n\n'
                                        'rule t2\nwhen concept=talk.day\nsay Again.\nremember pair: nb=ask 3d\n\n'
                                        'rule t3\nwhen concept=talk.day pair.mem.nb=ask\nsay So.\n'})
        lib = talk_conv.Library(root)
        result = lib.convert()
        self.assertEqual(result['stats']['markers'], 0)
        self.assertIn('nb', result['kept'])

    def test_remembered_of_the_other_is_read_as_self_and_becomes_conv(self):
        root = library({'rules/a.talk': 'rule t1\nwhen concept=talk.day\nsay How is he?\nremember other: asked=spouse 2m\n\n'
                                        'rule t2\nwhen concept=talk.day self.mem.asked=spouse\nsay Well.\n'})
        lib = talk_conv.Library(root)
        lib.convert()
        lib.write()
        self.assertIn('conv.asked=spouse', (root / 'rules/a.talk').read_text())


class Migration(unittest.TestCase):
    def test_only_whole_alternatives_are_replaced_and_shrugs_are_left(self):
        table = {'Good.': 'bc_good'}
        text = 'rule r\nwhen concept=x\nsay Good. | Good. Very good. | {k=v} Good.\n\nscene s\nwhen topic=day\na : Hm.\nb[stage=old] : Good. | Fine.\n'
        out, n = talk_migrate.rewrite(text, table)
        self.assertEqual(n, 3)
        self.assertIn('say #bc_good# | Good. Very good. | {k=v} #bc_good#', out)
        self.assertIn('b[stage=old] : #bc_good# | Fine.', out)

    def test_a_dialect_repeating_plain_words_or_naming_no_backchannel_fails_the_check(self):
        root = library({'grammar/plain.grammar': 'grammar plain\nbc_good = Good.\n',
                        'grammar/coldwater.grammar': 'grammar coldwater\nbc_good = Good.\nbc_odd = Odd.\n',
                        'settings.json': json.dumps({'ends': ['Mm.']}), 'rules/a.talk': 'rule r\nwhen concept=x\nsay #bc_good#\n'})
        problems = talk_migrate.check(root)
        self.assertTrue(any('repeats plain' in p for p in problems))
        self.assertTrue(any('bc_odd' in p for p in problems))


class Tagging(unittest.TestCase):
    def test_questions_ask_and_the_answers_answer(self):
        self.assertEqual(talk_tag.question("How are you keeping?"), 'ask.how')
        self.assertEqual(talk_tag.question("Can I borrow your saw?"), 'request')
        self.assertEqual(talk_tag.question("Will you dance with me?"), 'invite')
        self.assertEqual(talk_tag.question("Where's the miller?"), 'ask.whereabouts')
        self.assertEqual(talk_tag.answer(["Well enough."], 'ask.how'), ['answer.how'])

    def test_news_is_met_as_known_new_or_with_a_question(self):
        self.assertEqual(talk_tag.answer(["I heard."], 'tell.news'), ['react.news.known'])
        self.assertEqual(talk_tag.answer(["Never!"], 'tell.news'), ['react.news.new'])
        self.assertEqual(talk_tag.answer(["Who told you that?"], 'tell.news'), ['ask.detail'])
        self.assertEqual(talk_tag.answer(["Bright One. Who told you?"], 'tell.news'), ['react.news.new', 'ask.detail'])

    def test_every_answer_meets_what_was_owed_or_owes_nothing_of_it(self):
        for before, texts in (('tell.complaint', ['Aye, terrible.']), ('tease', ['Ha!']), ('tell.worry', ["It'll pass."]),
                              ('ask.detail', ['Since spring.']), ('greet', ['Morning.'])):
            moves = talk_tag.answer(texts, before)
            self.assertTrue(any(talk_tag.meets(moves[0], o) for o in talk_tag.expects(before)), (before, moves))

    def test_an_answer_concept_keeps_its_alias_first(self):
        root = library({'rules/a.talk': 'rule r\nwhen concept=jest.answer\nsay Ha! You wish.\n'})
        e = talk_tag.read(root)[0]
        moves, _ = talk_tag.tag(e)
        self.assertEqual(moves[0][0], talk_tag.ALIAS['jest.answer'])

    def test_a_death_is_grave_and_no_light_tone_follows_it_in_a_scene(self):
        root = library({'scenes/a.talk': 'scene s\nwhen topic=life\na : Old Bram died in the night.\nb : Ha. Bless him.\n'})
        e = talk_tag.read(root)[0]
        moves, tones = talk_tag.tag(e)
        self.assertEqual(tones[0], 'grave')
        self.assertNotEqual(tones[1] or talk_tag.default_tone(moves[1][0]), 'light')


class Equivalence(unittest.TestCase):
    def test_backchannels_and_discourse_words_expand_to_the_old_words(self):
        plain = {'bc_good': ['Good.'], 'vent': ['Truth be told', 'Between us']}
        self.assertEqual(talk_equiv.expansions('#bc_good#', plain), {'Good.'})
        self.assertIn('Truth be told, I am tired.', talk_equiv.expansions('#vent#, I am tired.', plain))
        self.assertEqual(talk_equiv.expansions('#greet#, friend.', plain), {'#greet#, friend.'})

    def test_a_converted_read_compares_equal(self):
        self.assertEqual(talk_equiv.norm_key('when concept=x pair.mem.jk=look', {'jk'}),
                         talk_equiv.norm_key('when concept=x conv.jk=look', {'jk'}))


if __name__ == '__main__':
    unittest.main()
