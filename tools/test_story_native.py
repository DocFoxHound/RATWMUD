"""The authoring fallback and compiled interpreter must agree on every shipped rehearsal: the 108 programs as they are
(version 1), the same 108 made version 2 (story_program.upgrade), and the version 2 samples that use every new condition
and action (tools/story_v2_samples.py), with their rolls hashed under several seeds; and the quest samples of
tools/story_v2_samples.py (Phase 6); and the night watch (Phase 8: every condition over the world's facts, the new events),
whose facts are also made up at random by tools/test_story_facts.py."""

import copy
import unittest
from unittest.mock import patch

import story_library
import story_native
import story_program
import story_runner
import story_v2_samples


def shipped():
    examples = list(story_runner.catalogue()['examples'].values())
    examples.extend(story_library.draft(entry['id']) for entry in story_library.entries())
    return examples


@unittest.skipUnless(story_native.interpreter(), 'Build ratw_server or set RATW_STORY_LIBRARY first.')
class NativeAuthoringParity(unittest.TestCase):
    def same(self, program, passed=True):
        actual = story_program.review(program)
        with patch('story_native.interpreter', return_value=None):
            expected = story_program.review(program)
        if passed is not None:
            self.assertEqual(actual['passed'], passed)
        self.assertEqual(actual, expected)

    def test_all_shipped_paths_match_including_traces_and_recipients(self):
        programs = shipped()
        self.assertEqual(len(programs), 108)
        for program in programs:
            with self.subTest(program=program['id']):
                self.same(program)

    def test_the_shipped_programs_as_version_2_match_too(self):
        for program in shipped():
            with self.subTest(program=program['id']):
                upgraded = story_program.upgrade(program)
                self.assertEqual(upgraded['version'], 2)
                self.same(upgraded)

    def test_every_version_2_construct_matches(self):
        for program in story_v2_samples.all_samples():
            with self.subTest(program=program['id']):
                self.same(program)
                # The same cases under other seeds and with no pinned rolls: whatever path they take, the twins agree.
                for seed in ('a', 'b', 'c', 'winter', 'x' * 80):
                    other = copy.deepcopy(program)
                    for case in other['tests']:
                        case.pop('rolls', None)
                        case['seed'] = seed
                        for key in ('expect', 'expectVariables', 'expectOutcome'):
                            case.pop(key, None)
                        case['expect'] = []
                    self.same(other, passed=None)

    def test_quests_match(self):
        """Doc 85, Phase 6: a story's quests run the same whichever twin fires its triggers (the quest effects are carried
        by both as any effect, and kept by the story after the firing)."""
        for program in (story_v2_samples.QUEST_SMALL, story_v2_samples.QUEST_CAMPAIGN):
            with self.subTest(program=program['id']):
                self.same(program)

    def test_world_facts_match(self):
        """Doc 85, Phase 8: conditions over the world's facts as of the event, and over quests, the same in both twins (the
        native one sent only the wolves its conditions read)."""
        self.same(story_v2_samples.FACTS_WATCH)
        other = copy.deepcopy(story_v2_samples.FACTS_WATCH)
        for case in other['tests']:
            case.pop('expectVariables', None)
            case['cohort'] = [story_v2_samples.ASH]   # (Birch out of the story: counts and loops change, the twins still agree.)
        other['audience']['players'] = [story_v2_samples.ASH]
        self.same(other, passed=None)


if __name__ == '__main__':
    unittest.main()
