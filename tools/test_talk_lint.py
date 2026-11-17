"""The truth lint's own tests (tools/talk_lint.py, tools/talk_truth.py; Docs/Design/84 §7.8)."""
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import talk_lint  # noqa: E402
import talk_truth  # noqa: E402

TALK = Path(__file__).resolve().parent.parent / 'Data' / 'Talk'

LIBRARY = """
scene t_fog_bare
when topic=lore here.region=ridgemere
a : Fog bell rang all night.
b : I counted the strokes.

scene t_fog_backed
when topic=lore here.region=ridgemere here.outdoors=fog
a : Fog bell rang all night.
b : I counted the strokes.

scene t_nets
when topic=work
a : My nets are tangled past saving.
b[fisher] : My nets too.

rule t_rain_react
when concept=react.rain
say It's raining. Of course it is.

rule t_id
when concept=talk.day
say Word is camp_w_ridgemere_6_8 is back.

rule t_they
when concept=talk.gossip
say #topic.victim# was robbed. Have you seen their face? | #topic.victim# was robbed. Did you see #topic.victim_their# face? | They say #topic.subject# left in a hurry.

rule t_kin_they
when concept=talk.family
say Ask #self.spouse#. They do the dreaming. | #self.spouse#'s family are coming, the two of us and #self.child# and them.

scene t_gos_pity
when topic=gossip
a : They're saying #topic.subject# #topic.claim#, poor thing.
b : Poor thing indeed.

scene t_gos_pity_bad
when topic=gossip topic.claim_kind=death|victim
a : They're saying #topic.subject# #topic.claim#, poor thing.
b : Poor thing indeed.

scene t_gos_glad
when topic=gossip topic.claim_kind=marriage
a : #topic.subject# #topic.claim#. Good for #topic.them#.
b : That's lovely news.

scene t_cr_rain
when topic=crime here.region=rain_coast
a : Rain covers footsteps. #topic.victim# heard nothing.
b : It covers everything.

scene t_cr_rain_gated
when topic=crime here.region=rain_coast here.outdoors=rain|storm
a : Rain covers footsteps. #topic.victim# heard nothing.
b : It covers everything.

scene t_food_pity
when topic=food
a : Poor thing, she's not eaten all day.
b : I'll take her some bread.

scene t_stranger
when topic=player topic.known=no
a : Look at #topic.subject#. | See #topic.subject# by the well?
b : Ask them, then. | Don't bother #topic.them#. | Travellers keep their own counsel.
a : They look tired. | They say #topic.subject# came over the Pass. | #topic.they.capitalize# looks tired.
"""


class Lint(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        for name in ('facts.json', 'truth.json', 'groups.talk', 'settings.json'):
            shutil.copy(TALK / name, self.root / name)
        (self.root / 'dialogue').mkdir()
        (self.root / 't.talk').write_text(LIBRARY)

    def tearDown(self):
        shutil.rmtree(self.root)

    def test_findings(self):
        found = {(f['rule'], f['entry'], f['text']) for f in talk_lint.findings(self.root)}
        self.assertIn(('fog_now', 't_fog_bare', 'Fog bell rang all night.'), found)
        self.assertNotIn(('fog_now', 't_fog_backed', 'Fog bell rang all night.'), found)
        self.assertIn(('fisher_self', 't_nets', 'My nets are tangled past saving.'), found)      # (a: anyone)
        self.assertNotIn(('fisher_self', 't_nets', 'My nets too.'), found)                       # (b[fisher])
        self.assertNotIn(('rain_now', 't_rain_react', "It's raining. Of course it is."), found)  # (react.rain holds rain)
        self.assertIn(('ids', 't_id', 'Word is camp_w_ridgemere_6_8 is back.'), found)

    def test_phase6_pronouns_news_kinds_and_weather(self):
        found = {(f['rule'], f['entry'], f['text']) for f in talk_lint.findings(self.root)}
        # "they" of one known wolf: the victim named, then "their"; never the symbol, nor the town's "they say".
        self.assertIn(('they_named', 't_they', '#topic.victim# was robbed. Have you seen their face?'), found)
        self.assertNotIn(('they_named', 't_they', '#topic.victim# was robbed. Did you see #topic.victim_their# face?'), found)
        self.assertNotIn(('they_named', 't_they', 'They say #topic.subject# left in a hurry.'), found)
        self.assertIn(('they_named', 't_kin_they', 'Ask #self.spouse#. They do the dreaming.'), found)
        self.assertNotIn(('they_named', 't_kin_they', "#self.spouse#'s family are coming, the two of us and #self.child# and them."), found)
        # Pity on gossip only for bad news; gladness only for glad news (a wedding is never "poor thing").
        self.assertIn(('gossip_kind', 't_gos_pity', "They're saying #topic.subject# #topic.claim#, poor thing."), found)
        self.assertIn(('pity', 't_gos_pity', 'Poor thing indeed.'), found)
        self.assertNotIn(('gossip_kind', 't_gos_pity_bad', "They're saying #topic.subject# #topic.claim#, poor thing."), found)
        self.assertNotIn(('pity', 't_gos_pity_bad', 'Poor thing indeed.'), found)
        self.assertNotIn(('glad', 't_gos_glad', "That's lovely news."), found)
        self.assertNotIn(('pity', 't_food_pity', "Poor thing, she's not eaten all day."), found)   # (Not a news topic.)
        # A traveller near is one wolf (Slice 2's review): never "them" for the one the two can see; travellers in
        # general, the town's "they say" and the symbols are fine.
        self.assertIn(('they_stranger', 't_stranger', 'Ask them, then.'), found)
        self.assertIn(('they_stranger', 't_stranger', 'They look tired.'), found)
        for fine in ("Don't bother #topic.them#.", 'Travellers keep their own counsel.',
                     'They say #topic.subject# came over the Pass.', '#topic.they.capitalize# looks tired.'):
            self.assertNotIn(('they_stranger', 't_stranger', fine), found)
        # A claim about the weather needs the weather.
        self.assertIn(('rain_claim', 't_cr_rain', 'Rain covers footsteps. #topic.victim# heard nothing.'), found)
        self.assertNotIn(('rain_claim', 't_cr_rain_gated', 'Rain covers footsteps. #topic.victim# heard nothing.'), found)

    def test_runtime_scoped(self):
        rules, _ = talk_truth.load(TALK)
        pity = next(r for r in rules if r.id == 'pity')
        self.assertIs(pity.judge({'topic.claim_kind': 'marriage'}), False)
        self.assertIs(pity.judge({'topic.claim_kind': 'death'}), True)
        self.assertTrue(pity.scoped('gossip') and not pity.scoped('food') and pity.scoped(None))
        rain = next(r for r in rules if r.id == 'rain_claim')
        self.assertIs(rain.judge({'here.weather': 'clear', 'here.outdoors': 'clear'}), False)

    def test_runtime(self):
        rules, _ = talk_truth.load(TALK)
        fog = next(r for r in rules if r.id == 'fog_now')
        self.assertIs(fog.judge({'here.weather': 'indoors', 'here.outdoors': 'clear'}), False)
        self.assertIs(fog.judge({'here.weather': 'indoors', 'here.outdoors': 'fog'}), True)
        self.assertIsNone(fog.judge({}))
        fish = next(r for r in rules if r.id == 'fishing_town')
        self.assertIs(fish.judge({'self.job': 'merchant', 'here.fishers': '1'}), False)
        self.assertIs(fish.judge({'self.job': 'merchant', 'here.fishers': '7'}), True)


if __name__ == '__main__':
    unittest.main()
