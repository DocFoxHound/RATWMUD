"""The Story Maker's guide (Docs/Story-Maker-guide.md; Docs/Design/85-story-triggers-and-campaign-control.md, Phase 10): its
reference is the trigger catalogue's (made by tools/story_guide.py, checked here as the Editor shows it), every event,
condition, action and effect is in it with its own anchor and an example, every example effect passes the checks, and the
written part covers what an admin needs (a small quest, a campaign, flow, variables and scopes, quests and rewards, the
console and steering, a new version, permissions, what is undone, troubleshooting). The world's words: the Bright One and
the Dark One only, wolves, private messages."""

import re
import unittest

import story_guide as G
import story_program as rules

TEXT = G.GUIDE.read_text()
REGIONS = {'square': {'id': 'square', 'cell': 'greyfen-square', 'x1': 0, 'y1': 0, 'x2': 3, 'y2': 3}}
SCENES = {'the-oath': {'visibility': 'private'}}


class Guide(unittest.TestCase):
    def test_the_reference_is_the_catalogues(self):
        self.assertEqual(G.current(TEXT), G.reference(), 'run python3 tools/story_guide.py --write')

    def test_every_entry_has_its_anchor_and_an_example(self):
        cat = rules.CATALOGUE
        for kind, items in (('event', cat['events']), ('condition', cat['conditions']), ('action', cat['actions']), ('effect', cat['effects'])):
            for item in items:
                with self.subTest(kind=kind, id=item['id']):
                    anchor = G._anchor(kind, item['id'])
                    self.assertIn(anchor, TEXT)
                    after = TEXT[TEXT.index(anchor):]
                    self.assertIn('```json', after[:6000], 'an example follows')

    def test_every_example_effect_passes_the_checks(self):
        for e in rules.CATALOGUE['effects']:
            with self.subTest(effect=e['id']):
                rules.validate_effect(G.example_effect(e), 'guide', REGIONS, SCENES)

    def test_the_written_part_covers_what_an_admin_needs(self):
        written = TEXT[:TEXT.index(G.BEGIN)]
        for heading in ('A small quest in five minutes', 'Building a campaign', 'Flow: if, wait, for each, random', 'Variables and scopes',
                        'Quests and rewards', 'The Campaigns console', 'Moving a run onto a new version', 'Who may do what',
                        'What the game undoes when a run ends', 'Troubleshooting: Explain, Why not?, What if'):
            self.assertRegex(written, r'\n## \d+\. ' + re.escape(heading), heading)
        for words in ('story.start', 'World variables', 'needs a builder', 'acts on wolves', 'may steer campaigns', 'Why not?', 'What if',
                      'prices.reset', 'when here', 'expectChapters', 'story ended'):
            self.assertIn(words, written)
        # (Every effect the game undoes at the end is in the table of section 14.)
        table = written[written.index('## 14.'):written.index('## 15.')]
        for kind in rules.UNDONE_AT_END:
            self.assertIn(f'`{kind}`', table)

    def test_the_worlds_words(self):
        for word in (r'\bdogs?\b', r'\bgods?\b', r'\bGods?\b', r'\btells\b(?= (to|from|in) )'):
            self.assertIsNone(re.search(word, TEXT), word)


if __name__ == '__main__':
    unittest.main()
