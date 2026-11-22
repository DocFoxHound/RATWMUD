"""The authoring fallback and compiled interpreter must agree on every shipped rehearsal: the 108 programs as they are
(version 1), the same 108 made version 2 (story_program.upgrade), and the version 2 samples that use every new condition
and action (tools/story_v2_samples.py), with their rolls hashed under several seeds."""

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


if __name__ == '__main__':
    unittest.main()
