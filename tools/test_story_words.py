"""Doc 86, V2 (with T4): the story words check, tools/story_words.py: each rule, and every bundled program passes."""
import copy
import json
import unittest
from pathlib import Path

import story_library
import story_runner as runner
import story_v2_samples as samples
import story_words as W

WOLVES = ['player-ash', 'player-birch']


def program(effects=(), quests=(), scenes=(), entry=()):
    return {
        'format': 'ratw-story-program', 'version': 2, 'id': 'words', 'title': 'Words', 'start': 'one',
        'audience': {'kind': 'players', 'players': WOLVES},
        'variables': [{'id': 'hero', 'type': 'wolf', 'scope': 'story'}],
        'phases': [{'id': 'one', 'journal': 'The lamps are lit.', 'effects': list(entry), 'exits': [],
                    'triggers': [{'id': 't', 'on': 'arrival', 'audience': 'participant',
                                  'actions': [{'do': 'effect', 'effect': e} for e in effects] or [{'do': 'note', 'text': 'x'}]}]}],
        'quests': list(quests), 'scenes': list(scenes),
    }


def texts(p, voices=None, ids=None):
    return [(c['blocks'], c['text']) for c in W.check(p, voices, ids)]


def blocks(p, voices=None, ids=None):
    return [t for b, t in texts(p, voices, ids) if b]


def warns(p, voices=None, ids=None):
    return [t for b, t in texts(p, voices, ids) if not b]


class Rules(unittest.TestCase):
    def test_a_clean_story_has_no_problems(self):
        p = program([{'kind': 'npc.greet', 'npc': 'rm111', 'player': '$actor', 'text': '#greet# You kept the lamp lit.'},
                     {'kind': 'title.give', 'player': '$actor', 'title': 'the Lamp of {hero}'},
                     {'kind': 'story.tell', 'to': 'wolf', 'player': '$actor', 'text': 'The road is quiet tonight.'}])
        self.assertEqual(texts(p), [])

    def test_symbols_through_the_library(self):
        self.assertTrue(blocks(program([{'kind': 'npc.greet', 'npc': 'rm111', 'player': '$actor', 'text': '#no_such_symbol# Hello.'}])))
        titled = program([{'kind': 'npc.greet', 'npc': 'rm111', 'player': '$actor', 'text': '#greet# #player.title#.'}])
        self.assertEqual(blocks(titled), [])
        self.assertTrue(warns(titled), 'a fact not every wolf has here: a warning')
        self.assertTrue(blocks(program([{'kind': 'npc.greet', 'npc': 'rm111', 'player': '$actor', 'text': '#player.no_such# there.'}])))
        self.assertTrue(blocks(program([{'kind': 'npc.greet', 'npc': 'rm111', 'player': '$actor', 'text': '#greet Hello.'}])), 'an unclosed #')
        self.assertTrue(blocks(program([{'kind': 'npc.greet', 'npc': 'rm111', 'player': '$actor', 'text': 'Hello [friend].'}])), 'a bracket')
        # (Words no resident says fill nothing: a symbol there would be shown as it is.)
        self.assertTrue(blocks(program([{'kind': 'board.post', 'town': 'greyfen', 'text': '#greet# all.', 'days': 2}])))

    def test_words_said_as_written_fill_no_symbols(self):
        # (npc.speak is said aloud as written, and a scene's lines are shown as written: a #symbol# there would be seen as
        # it is. Review of Phase 3.)
        self.assertTrue(blocks(program(entry=[{'kind': 'npc.speak', 'npc': 'rm111', 'text': '#greet# all.'}])))
        scene = {'id': 's', 'cast': [{'id': 'c', 'npc': 'rm111'}],
                 'beats': [{'id': 'b', 'type': 'line', 'speaker': 'c', 'text': '#greet# friend', 'seconds': 3},
                           {'id': 'r', 'type': 'response', 'speaker': 'c', 'text': '#greet# again', 'seconds': 3}]}
        self.assertEqual(len(blocks(program(scenes=[scene]))), 2)
        # (Still a resident's words: their dialect is read.)
        lex = W.lexicon()
        dialect, phrase = next((d, p) for d, ps in lex.items() for p, saying in ps if not saying and ' ' in p)
        other = next(d for d in lex if d != dialect)
        self.assertTrue(warns(program(entry=[{'kind': 'npc.speak', 'npc': 'rm111', 'text': f'Well, {phrase} indeed.'}]), {'rm111': other}))

    def test_every_text_of_the_catalogue_is_read(self):
        for e in runner.rules.CATALOGUE['effects']:
            for param in e['params']:
                if param['type'] == 'text':
                    with self.subTest(effect=e['id'], param=param['key']):
                        self.assertTrue(param['key'] in [k for k, *_ in W.EFFECT_WORDS.get(e['id'], ())]
                                        or f"{e['id']}.{param['key']}" in W.EXEMPT)
        for effect in ({'kind': 'figure.set', 'role': 'villain', 'name': 'the Dog King', 'title': 'god of rot', 'about': 'A cur.'},
                       {'kind': 'times.nudge', 'words': 'The gods frown'}, {'kind': 'festival.call', 'name': 'the Dog Days'},
                       {'kind': 'warrant.issue', 'player': '$actor', 'town': 'greyfen', 'reason': 'kicked a dog'}):
            with self.subTest(effect=effect['kind']):
                self.assertTrue(blocks(program([effect])))

    def test_forbidden_words(self):
        for words in ('A dog barks.', 'The GODS are watching.', 'Puppies in the barn.', 'By the deities!', 'Send a tell to the warden.'):
            with self.subTest(words=words):
                self.assertTrue(blocks(program([{'kind': 'journal.note', 'text': words}])))
        # (Whole words only: "godwit" and "dogged" are other words; pups are what young wolves are.)
        self.assertEqual(blocks(program([{'kind': 'journal.note', 'text': 'A dogged godwit; the pups sleep. Tell her.'}])), [])
        # (A quest's words and a scene's are read too.)
        q = {'id': 'q', 'title': 'The lost dog', 'objectives': [{'id': 'o', 'label': 'Find it', 'on': 'arrival'}]}
        self.assertTrue(blocks(program(quests=[q])))

    def test_the_givers_dialect(self):
        lex = W.lexicon()
        dialect, phrase = next((d, p) for d, ps in lex.items() for p, saying in ps if not saying and ' ' in p)
        other = next(d for d in lex if d != dialect)
        p = program([{'kind': 'npc.greet', 'npc': 'rm111', 'player': '$actor', 'text': f'Well, {phrase} indeed.'}])
        self.assertTrue(warns(p, {'rm111': other}), 'another dialect in their mouth')
        self.assertTrue(warns(p, {'rm111': 'plain'}), 'a plain speaker')
        self.assertEqual(warns(p, {'rm111': dialect}), [], 'their own')
        self.assertEqual(warns(p), [], 'offline: no voices')
        # (An offer's words are the giver's.)
        q = {'id': 'q', 'title': 'Help', 'giver': 'rm111', 'offer': {'say': f'Well, {phrase} indeed.'},
             'objectives': [{'id': 'o', 'label': 'Help', 'on': 'arrival'}]}
        self.assertTrue(warns(program(quests=[q]), {'rm111': other}))

    def test_ids_in_speech(self):
        for words in ('Ask rm111 about it.', 'Ask rm_eleanor_saltcot.', 'Tell wolf-ash-12.', 'Ask rmx12 there.'):
            with self.subTest(words=words):
                self.assertTrue(blocks(program([{'kind': 'npc.greet', 'npc': 'rm111', 'player': '$actor', 'text': words}])))
        p = program([{'kind': 'npc.greet', 'npc': 'rm111', 'player': '$actor', 'text': 'Ask at greyfen-west about it.'}])
        self.assertEqual(blocks(p), [])
        self.assertTrue(blocks(p, ids={'greyfen-west'}), 'with the world, an id of a place')
        self.assertEqual(blocks(program([{'kind': 'npc.greet', 'npc': 'rm111', 'player': '$actor', 'text': 'Meet me at the gate.'}]), ids={'gate'}), [],
                         'a plain word that is also an id is a word')
        # (A word joined to "wolf-" is a word: a wolf's id has digits, "wolf-" and 32 hex.)
        q = {'id': 'q', 'title': 'The wolf-pack and the wolf-wide road', 'objectives': [{'id': 'o', 'label': 'Run', 'on': 'arrival'}]}
        self.assertEqual(blocks(program(quests=[q])), [])
        self.assertTrue(blocks(program([{'kind': 'journal.note', 'text': 'Ask wolf-' + '3f' * 16 + '.'}])))
        self.assertTrue(blocks(program([{'kind': 'journal.note', 'text': 'Ask wolf-abcdefabcdef.'}])))

    def test_lengths_with_fills(self):
        q = {'id': 'q', 'title': 'Help', 'giver': 'rm111', 'offer': {'say': 'x' * 290 + ' {hero}'},
             'objectives': [{'id': 'o', 'label': 'Help', 'on': 'arrival'}]}
        self.assertTrue(blocks(program(quests=[q])), '290 + a wolf\'s name at 24 > 300')
        q['offer']['say'] = 'x' * 270 + ' {hero}'
        self.assertEqual(blocks(program(quests=[q])), [])
        self.assertTrue(blocks(program(entry=[{'kind': 'npc.line', 'npc': 'rm111', 'text': 'y' * 201}])))

    def test_face_banners_and_speech_aloud(self):
        tell = {'kind': 'story.tell', 'to': 'wolf', 'player': '$actor', 'from': 'rm111', 'text': 'The lamps are out.'}
        self.assertTrue(warns(program([tell])) and not blocks(program([tell])))
        self.assertTrue(blocks(program([{**tell, 'text': 'She says: "Come quickly."'}])), 'quoted speech in a banner')
        self.assertTrue(blocks(program([{**tell, 'text': 'She says “Come quickly.”'}])))
        self.assertEqual(texts(program([{**tell, 'from': None, 'text': 'The lamps are out.'}])), [])
        self.assertTrue(warns(program([{'kind': 'npc.speak', 'npc': 'rm111', 'text': 'You came back!'}])))

    def test_a_clearings_words(self):
        plant = {'kind': 'belief.plant', 'subject': 'rm111', 'claim': 'never took the silver', 'town': 'greyfen', 'count': 3,
                 'confidence': 60, 'claimKind': 'story.cleared'}
        self.assertTrue(warns(program([plant])))
        self.assertEqual(warns(program([{**plant, 'claim': 'took the silver'}])), [])
        self.assertEqual(warns(program([{**plant, 'claimKind': 'story.rumour'}])), [])

    def test_a_scenes_lines(self):
        scene = {'id': 's', 'cast': [{'id': 'speaker', 'npc': 'rm111'}],
                 'beats': [{'id': 'b', 'type': 'line', 'speaker': 'speaker', 'text': 'The gods help us.', 'seconds': 3}]}
        self.assertTrue(blocks(program(scenes=[scene])))


class Bundled(unittest.TestCase):
    def test_every_bundled_program_passes(self):
        found = 0
        for f in sorted((runner.DATA / 'Storylines/programs').rglob('*.json')):
            found += 1
            self.assertEqual(W.check(json.loads(f.read_text())), [], f.name)
        for e in story_library.entries():
            found += 1
            self.assertEqual(W.check(story_library.draft(e['id'])), [], e['id'])
        for p in [*runner.catalogue()['examples'].values(), *samples.all_samples()]:
            self.assertEqual([c for c in W.check(p) if c['blocks']], [], p['id'])
        self.assertEqual(found, 108)

    def test_the_cli_over_a_directory(self):
        import subprocess
        import sys
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            good = program([{'kind': 'journal.note', 'text': 'Quiet.'}])
            bad = copy.deepcopy(good)
            bad['phases'][0]['triggers'][0]['actions'][0]['effect']['text'] = 'A dog barks.'
            Path(d, 'good.json').write_text(json.dumps(good))
            run = subprocess.run([sys.executable, str(Path(W.__file__)), '--stories', d], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            Path(d, 'bad.json').write_text(json.dumps(bad))
            run = subprocess.run([sys.executable, str(Path(W.__file__)), '--stories', d, '--json'], capture_output=True, text=True)
            self.assertEqual(run.returncode, 1)
            self.assertTrue(json.loads(run.stdout)[str(Path(d, 'bad.json'))])



class TitleLine(unittest.TestCase):
    def test_talk_check_validates_a_line_gated_on_a_title(self):
        # (Doc 86, C4: facts.json's player.titled and player.title; talk_check --validate takes a line that says the title.)
        import shutil
        import subprocess
        import tempfile
        built = sorted(runner.DATA.parent.glob('build*/talk_check'), key=lambda f: f.stat().st_mtime, reverse=True)
        if not built:
            self.skipTest('Build talk_check first.')
        with tempfile.TemporaryDirectory() as d:
            talk = Path(d, 'talk')
            shutil.copytree(runner.DATA / 'Talk', talk)
            tree = talk / 'dialogue' / 'resident.tree'
            text = tree.read_text()
            at = text.index('say[player.asked=person')
            end = text.index('\n', at) + 1
            tree.write_text(text[:end] + "say[player.titled=yes priority=16] They call you #player.title# now, don't they?\n" + text[end:])
            run = subprocess.run([str(built[0]), '--data', str(talk), '--validate', str(tree)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            tree.write_text(text[:end] + 'say[player.titled=maybe priority=16] Never.\n' + text[end:])
            run = subprocess.run([str(built[0]), '--data', str(talk), '--validate', str(tree)], capture_output=True, text=True)
            self.assertNotEqual(run.returncode, 0)

if __name__ == '__main__':
    unittest.main()
