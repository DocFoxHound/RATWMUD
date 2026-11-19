"""Portable scene authoring, embedded references, and playback receipt scoping."""

import copy
import unittest

import story_author
import story_program as rules
import story_scenes as scenes


class SceneAuthorTests(unittest.TestCase):
    def setUp(self):
        self.scene = scenes.examples()['greyfen-oath']

    def test_standalone_graph_and_all_authored_endings(self):
        for choices, responses, ending in [
            ([], {}, 'bridge-watch'),
            (['carters'], {}, 'silent-watch'),
            (['carters'], {'name-the-watch': 'submitted'}, 'road-watch'),
        ]:
            report = story_author.request(
                {'op': 'scene.rehearse', 'scene': self.scene, 'choices': choices, 'responses': responses}, {}
            )
            self.assertEqual(report['ending'], ending)
            self.assertEqual(report['digest'], rules.digest(self.scene))
        with self.assertRaisesRegex(ValueError, 'Unused'):
            scenes.rehearse(self.scene, [], {'name-the-watch': 'submitted'})
        with self.assertRaisesRegex(ValueError, 'response beat'):
            scenes.rehearse(self.scene, [], {'name-the-watch': 'my private text'})

    def test_import_and_document_validation(self):
        p = story_author.request({'op': 'draft', 'blank': True}, {})['program']
        imported = story_author.request({'op': 'scene.import', 'scene': self.scene, 'program': p}, {})['program']
        self.assertEqual(imported['scenes'], [self.scene])
        with self.assertRaisesRegex(ValueError, 'already exists'):
            story_author.request({'op': 'scene.import', 'scene': self.scene, 'program': imported}, {})
        isolated = copy.deepcopy(self.scene)
        isolated['beats'][0]['next'] = isolated['start']
        with self.assertRaisesRegex(ValueError, 'cycle'):
            scenes.validate(isolated)
        bad = copy.deepcopy(self.scene)
        del bad['beats'][-1]['text']
        with self.assertRaisesRegex(ValueError, 'Missing fields'):
            scenes.validate(bad)

    def test_visibility_and_initiating_character_when_standalone(self):
        for visibility in ('public', 'party', 'individual'):
            doc = {**self.scene, 'visibility': visibility}
            for specific in ('player-ash', None):
                p = scenes.wrap(doc, specific)
                report = rules.review(p)
                self.assertTrue(report['passed'])
                self.assertEqual(report['uncovered'], [])
                on_arrival = not specific and visibility != 'public'
                self.assertEqual(p['start'], 'arrival' if on_arrival else 'playing')
                if on_arrival:
                    self.assertNotIn(doc['title'], [p['title'], *(ph['journal'] for ph in p['phases'])])
                play = report['tests'][0]['effects'][0]
                self.assertEqual(play['player'], 'world' if visibility == 'public' else 'player-ash')

    def test_embedded_refs_and_private_audience(self):
        doc = {**self.scene, 'visibility': 'party'}
        p = scenes.wrap(doc, 'player-ash')
        effect = p['phases'][0]['effects'][0]
        effect['player'] = 'world'
        with self.assertRaisesRegex(ValueError, 'private scene'):
            rules.validate(p)
        effect['player'] = 'player-ash'
        effect['scene'] = 'missing'
        with self.assertRaisesRegex(ValueError, 'embedded scene'):
            rules.validate(p)
