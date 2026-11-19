"""The authoring fallback and compiled interpreter must agree on every shipped rehearsal."""

import unittest
from unittest.mock import patch

import story_library
import story_native
import story_program
import story_runner


@unittest.skipUnless(story_native.interpreter(), 'Build ratw_server or set RATW_STORY_LIBRARY first.')
class NativeAuthoringParity(unittest.TestCase):
    def test_all_shipped_paths_match_including_traces_and_recipients(self):
        examples = list(story_runner.catalogue()['examples'].values())
        examples.extend(story_library.draft(entry['id']) for entry in story_library.entries())
        for program in examples:
            with self.subTest(program=program['id']):
                actual = story_program.review(program)
                with patch('story_native.interpreter', return_value=None):
                    expected = story_program.review(program)
                self.assertTrue(actual['passed'])
                self.assertEqual(actual, expected)


if __name__ == '__main__':
    unittest.main()
