#!/usr/bin/env python3
"""The .mantle syntax in the tools and in the game agree, and G2 refuses what it should (Docs/Design/84 §9.6, §11, §14.4,
§17; Phase 9). No database: the world's names and towns are given as mantle_check's world facts.

  * tools/mantle_text.py and the game's loader (talk_check --mantle --json) give every item of every fixture mantle the
    same key, in the draft as written and in its canonical form; the canonical form reads back to the same bytes;
  * tools/mantle_check.py finds the fixture clean, and refuses each thing it should, by its item's key.

talk_check must be built: RATW_TALK_CHECK names it, else the newest build*/talk_check.
"""
import json
import subprocess
import tempfile
import unittest
from pathlib import Path

import mantle_check as C
import mantle_store as S
import mantle_text as T

ROOT = Path(__file__).resolve().parent.parent
FIXTURE = ROOT / 'Tests' / 'fixtures' / 'mantles'


def game_keys(paths: list[Path]) -> dict:
    run = subprocess.run([str(S.talk_check()), '--json', '--mantle'] + [str(p) for p in paths], capture_output=True, text=True)
    return {Path(r['file']).name: r for r in json.loads(run.stdout)}


class Syntax(unittest.TestCase):
    def test_keys_agree_with_the_game(self):
        files = sorted(FIXTURE.rglob('*.mantle'))
        self.assertGreaterEqual(len(files), 15)
        game = game_keys(files)
        with tempfile.TemporaryDirectory() as tmp:
            canon = []
            for f in files:
                m = T.read(f.read_text(), str(f))
                self.assertEqual([i.key for i in m.items], game[f.name]['keys'], f.name)
                self.assertTrue(game[f.name]['ok'], f'{f.name}: {game[f.name]["problems"]}')
                text = T.write(m, 'canonical')
                self.assertEqual(T.write(T.read(text), 'canonical'), text, f'{f.name}: canonical is stable')
                p = Path(tmp) / f.name
                p.write_text(text)
                canon.append((p, m))
            again = game_keys([p for p, _ in canon])
            for p, m in canon:
                self.assertEqual(sorted(i.key for i in m.items), sorted(again[p.name]['keys']), f'{p.name} canonical')

    def test_reading(self):
        m = T.read((FIXTURE / 'coldwater' / 'cw017.mantle').read_text())
        keys = {i.key: i for i in m.items}
        self.assertEqual(keys['grown.request.carry'].move, 'request')
        self.assertEqual(keys['story.first_swim'].kind, 'story')
        self.assertIn('node swim_where', keys['graft.resident.hub'].body)
        self.assertEqual(m.columns()['card']['topics'][0], ['weather', 0.4])
        # A new unnamed item never takes a key the rows hold; a symbol's first line keeps its name.
        r = T.read('mantle cw018\nwords\noath! = By the deep\ngrown\nline greet : Hi.\n', reserved={'grown.greet.1', 'words.oath'})
        self.assertEqual([i.key for i in r.items], ['words.oath', 'grown.greet.2'])
        with self.assertRaises(T.MantleError) as e:
            T.read('mantle cw018\nline @a greet : Hi.\nline @a greet : Yo.\n')
        self.assertIn('there twice', str(e.exception))


class Rules(unittest.TestCase):
    def check(self, text: str, world=None, others=None):
        m = T.read(text)
        return [str(f) for f in C.check([m], world=world, others=others) if not f.warning]

    BASE = (FIXTURE / 'saltwell' / 'sw001.mantle').read_text()

    def with_line(self, words: str) -> str:
        return self.BASE.replace('line answer.how : Weighed and found fair, mostly.', f'line answer.how : {words}')

    def test_the_fixture_is_clean(self):
        mantles = [T.read(f.read_text(), str(f)) for f in sorted(FIXTURE.rglob('*.mantle'))]
        self.assertEqual([str(f) for f in C.check(mantles) if not f.warning], [])

    def test_refusals(self):
        world = {'names': {'Hollin', 'Ashby'}, 'npcs': {'tt_sa_storekeeper'}, 'town': {}, 'wearer': {}}
        cases = {
            'Hollin says the pans are full.': 'the name of someone in the world',
            'I saw Brannoc at the well.': 'capitalised mid-sentence',
            'A dog barked at the pans.': 'never the word dog',
            'By the gods, it is hot.': 'never gods',
            'Send a tell to the keeper.': 'never tells',
            '#other.name#? She owes me for salt.': 'beside a person named by a slot',
            'My nets are torn again.': 'takes for granted',
            "Fair catch, that one.": 'coldwater speech',
        }
        for words, why in cases.items():
            found = self.check(self.with_line(words), world=world)
            self.assertTrue(any(why in f for f in found), f'{words}: {found}')
            self.assertTrue(any('any.answer.how.1' in f for f in found), f'{words}: named by its key: {found}')
        stop = self.check(self.BASE.replace('oath! = By the pans', 'oath! = By the pans.'))
        self.assertTrue(any('ends its own sentence' in f and 'words.oath' in f for f in stop), stop)
        # A word of its own dialect written out, where its wearer may speak with none.
        none = self.check(self.BASE.replace('accent broad light', 'accent none light').replace(
            'Mind the salt on the step.', 'Sweet as well water, the step.'))
        self.assertTrue(any('written out' in f for f in none), none)
        # The tier's minimum, for a ready one; habits; 5-word runs shared in a dialect.
        short = self.check(self.BASE.split('\nscene weighing')[0] + '\n')
        self.assertTrue(any('minimum' in f for f in short), short)
        habits = self.check(self.BASE.replace('Weighed and found fair, mostly.', '#oath#. #saying#.').replace(
            'line farewell.return : #farewell#.', 'line farewell.return : #farewell#. #oath#.').replace(
            "line react.news.known : I'd heard it at the well.", "line react.news.known : #oath#, I'd heard."))
        self.assertTrue(any('its habits are in' in f for f in habits), habits)
        twin = T.read(self.BASE.replace('mantle sw001', 'mantle sw002').replace('written_for tt_sa_storekeeper', ''))
        shared = [str(f) for f in C.check([T.read(self.BASE)], others=[twin]) if not f.warning]
        self.assertTrue(any('word for word in sw002' in f for f in shared), shared)
        # In a town: one oath, one saying, a quirk at most twice.
        town = {'names': set(), 'npcs': {'a', 'b', 'c'}, 'town': {'a': 'saltwell', 'b': 'saltwell', 'c': 'saltwell'},
                'wearer': {'sw001': 'a', 'sw003': 'b', 'sw004': 'c'}}
        n1 = T.read(self.BASE.replace('mantle sw001', 'mantle sw003').replace('written_for tt_sa_storekeeper', 'written_for b')
                    .replace('Salt keeps, and so do debts | Weigh twice, sell once', 'Sand in the shoe, sand in the soup'))
        n2 = T.read(self.BASE.replace('mantle sw001', 'mantle sw004').replace('written_for tt_sa_storekeeper', 'written_for c')
                    .replace('By the pans | Salt and sun', 'Sun and stone'))
        found = [str(f) for f in C.check([T.read(self.BASE)], world=town, others=[n1, n2]) if not f.warning]
        self.assertTrue(any("sw003's oath too" in f for f in found), found)
        self.assertTrue(any('three in one town' in f for f in found), found)

    def test_slots_and_listeners(self):
        """Phase 11: the new slots name people (no he/she beside them) and back what they take for granted; a listener's
        bare line never writes the dialect out; with the wearer's facts, a line they can never say is warned."""
        world = {'names': set(), 'npcs': {'tt_sa_storekeeper'}, 'town': {}, 'wearer': {}}
        friend = self.check(self.with_line('#self.friend# came by. She owes me for salt.'), world=world)
        self.assertTrue(any('beside a person named by a slot' in f for f in friend), friend)
        # "my husband" is taken for granted by a line that names the husband by his slot (only the married have one).
        self.assertEqual(self.check(self.with_line('My husband #self.spouse# weighs it too.'), world=world), [])
        self.assertTrue(any('takes for granted' in f for f in self.check(self.with_line('My husband weighs it too.'), world=world)))
        scene = self.BASE.replace("b : There's always a thumb on it somewhere.", "b : Sweet as well water, that scale.")
        found = self.check(scene)
        self.assertTrue(any("listener's line" in f and 'any.scene.weighing' in f for f in found), found)
        gated = self.BASE.replace("b : There's always a thumb on it somewhere.",
                                  "b[from=saltwell] #sw : Sweet as well water, that scale.\nb : There's always a thumb on it somewhere.")
        self.assertFalse(any("listener's line" in f for f in self.check(gated)), self.check(gated))
        m = T.read(self.with_line('#self.friend# says the same.'))
        lacks = [str(f) for f in C.check([m], facts={'tt_sa_storekeeper': {'id': 'tt_sa_storekeeper'}}) if f.warning]
        self.assertTrue(any('never heard from them' in f and 'any.answer.how.1' in f for f in lacks), lacks)
        has = [str(f) for f in C.check([m], facts={'tt_sa_storekeeper': {'friend': 'Ash Sheafmeadow'}}) if f.warning]
        self.assertFalse(any('never heard' in f for f in has), has)
        one = self.check(self.BASE.replace('Salt keeps, and so do debts | Weigh twice, sell once', 'Weigh twice, sell once'))
        self.assertTrue(any("'saying' has 1 alternative" in f for f in one), one)


    def test_fix8(self):
        """Fix 8 (doc 84 §42): G2 reads tagged scene lines; the persona has no pronoun; a window line on a node whose tree
        reports facts says which case it is; the pools' health (--pools) year by year and the spares' honesty."""
        world = {'names': {'Hollin'}, 'npcs': {'tt_sa_storekeeper'}, 'town': {}, 'wearer': {}}
        for tagged in ("b[job=merchant] #fair : Ask Hollin, he weighs fair.", "a@fair : Ask Hollin, then."):
            text = self.BASE.replace("b : There's always a thumb on it somewhere.",
                                     "b : There's always a thumb on it somewhere.\n" + tagged)
            found = self.check(text, world=world)
            self.assertTrue(any('the name of someone' in f and 'any.scene.weighing' in f for f in found), f'{tagged}: {found}')
        she = self.check(self.BASE.replace('and weighs everything twice', 'and she weighs everything twice'))
        self.assertTrue(any('persona' in f and '"she"' in f for f in she), she)
        bare = self.check(self.BASE.replace('dlg shop.greet [here.shop!=bare] :', 'dlg shop.greet :'))
        self.assertTrue(any('dlg.shop.greet.1' in f and 'here.shop' in f for f in bare), bare)
        news = self.BASE + '\ndlg resident.news : Nothing worth the telling.\n'
        self.assertTrue(any('resident.news' in f and 'world.news' in f for f in self.check(news)))
        cased = news.replace('dlg resident.news :', 'dlg resident.news [' + C.PLAIN_CASE['resident.news'] + '] :')
        self.assertFalse(any('resident.news' in f for f in self.check(cased)), self.check(cased))
        self.assertIn('world.news', C.window_facts()['resident.news'])
        self.assertNotIn('topic.meeting', C.window_facts().get('shop.greet', set()))
        # The pools: 4 spares and 1 ready, the births drawn and the wearers' deaths coming back; the spares' honesty.
        report = {'pools': {'saltwell': {'spare': 4, 'ready': 1, 'resting': 1, 'birthsByYear': [2.0, 2.0, 4.0],
                                         'wearerDeathsByYear': [1.0, 1.0, 0.0], 'honesty': {'sly': 50.0, 'plain': 50.0},
                                         'livingHonesty': {'sly': 20.0, 'plain': 80.0}}}}
        h = C.pool_health(report)['saltwell']
        self.assertEqual([y['green'] for y in h['years']], [True, True, False], h)
        self.assertEqual(h['years'][1]['supply'], 4, '5 - 2 births + 1 rested')
        self.assertFalse(h['ok'])
        self.assertTrue(any('honesty' in w for w in h['why']), h['why'])
        spare = T.read(self.BASE.replace('mantle sw001', 'mantle sw901').replace('written_for tt_sa_storekeeper', '')
                       .replace('tier standard', 'tier spare'))
        spare.traits['honesty'] = 'plain'
        mixed = C.pool_health(report, [spare, spare])['saltwell']
        self.assertEqual(mixed['spares'], 6)
        self.assertEqual(mixed['honesty'][0], 33.3, 'two sly of six')


class Kits(unittest.TestCase):
    def test_kits_are_their_dialects_pups(self):
        """Phase 11: a stage kit (Data/Talk/kits/<dialect>.<stage>.talk, doc 84 §7.10) is the shared floor of one dialect's
        pups or adolescents: every entry is gated to speakers from that dialect's towns and of that stage."""
        towns = {'saltwell': {'saltwell'}, 'mirebridge': {'fenshore', 'mirebridge'}, 'plain': {'ridgemere'}}   # (plain: Ridgemere's speech)
        kits = sorted((ROOT / 'Data' / 'Talk' / 'kits').glob('*.talk'))
        self.assertGreaterEqual(len(kits), 4)
        for f in kits:
            dialect, stage = f.stem.split('.')
            entry, gates = None, []
            for line in f.read_text().splitlines() + ['rule end']:
                t = line.strip()
                if t.startswith(('rule ', 'scene ')):
                    if entry:
                        froms = {v for g in gates if g.split('=')[0] in ('self.from', 'a.from') for v in g.split('=')[1].split('|')}
                        stages = {v for g in gates if g.split('=')[0] in ('self.stage', 'a.stage') for v in g.split('=')[1].split('|')}
                        self.assertTrue(froms and froms <= towns.get(dialect, {dialect}), f'{f.name} {entry}: from {froms}')
                        self.assertEqual(stages, {stage}, f'{f.name} {entry}: stage {stages}')
                    entry, gates = t.split()[1], []
                elif t.startswith('when '):
                    gates += t[5:].split()


if __name__ == '__main__':
    unittest.main()
