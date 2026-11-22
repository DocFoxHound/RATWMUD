"""Every problem of a story draft at once (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 3; 4.2's Problems
list): story_program.problems, where validate stops at the first. Every bundled example and library draft has none; a
draft broken in several places lists each, with its path and where an editor goes; problems and validate agree on every
draft (thousands of seeded mutations of the examples); and the warnings the program alone can tell. No database."""

import copy
import json
import random
import unittest
from unittest.mock import patch

import story_library
import story_program as rules
import story_runner as runner

EXAMPLES = runner.catalogue()['examples']


def structural(found):
    return [p for p in found if p['level'] == 'error' and not p['path'].startswith('tests[')]


class Problems(unittest.TestCase):
    def test_the_bundled_examples_and_the_library_have_none(self):
        for name, p in EXAMPLES.items():
            self.assertEqual([x for x in rules.problems(p) if x['level'] == 'error'], [], name)
        for entry in story_library.entries():
            p = story_library.draft(entry['id'])
            self.assertEqual([x for x in rules.problems(p) if x['level'] == 'error'], [], entry['id'])
        self.assertEqual(rules.problems(runner.catalogue()['blank']), [])

    def test_a_draft_broken_in_several_places_lists_each_with_where_it_is(self):
        p = copy.deepcopy(EXAMPLES['winter-ferries'])
        p['regions'] = [{'id': 'gap', 'cell': 'c', 'x1': 5, 'y1': 0, 'x2': 1, 'y2': 1}]
        p['triggers'][1]['if'] = {'test': 'counter', 'key': 'k', 'scope': 'nobody', 'op': '>', 'value': 1}
        p['phases'][1]['triggers'][0]['actions'].insert(0, {'do': 'add', 'key': 'bad key', 'scope': 'story', 'value': 1})
        p['phases'][1]['triggers'][0]['actions'].insert(0, {'do': 'enter', 'phase': 'deep-freeze'})
        p['phases'][2]['effects'].append({'kind': 'deed.award', 'player': 'player-ash'})
        p['phases'][3]['journal'] = ''
        p['tests'][0]['events'][0]['day'] = 'soon'
        found = rules.problems(p)
        paths = {(x['path'], x['text']) for x in found}
        self.assertEqual(paths, {
            ('regions[0]', 'Choose ordered, inclusive tile bounds for the region.'),
            ('triggers[1].if', 'Choose a counter name and scope.'),
            ('phases[1].triggers[0].actions[0]', 'Enter phase must be the last action.'),
            ('phases[1].triggers[0].actions[1]', 'Choose a counter name and scope.'),
            ('phases[2].effects[1]', "Missing fields ['detail'], unexpected fields []."),
            ('phases[3]', 'Each phase needs a unique ID and journal entry.'),
            ('tests[0].events[0]', 'Invalid rehearsal fact.'),
        }, found)
        where = {x['path']: x['at'] for x in found}
        self.assertEqual(where['triggers[1].if'], {'phase': None, 'kind': 'trigger', 'index': 1, 'id': 'offer-a-coil', 'part': 'if'})
        self.assertEqual(where['phases[1].triggers[0].actions[1]'],
                         {'phase': 1, 'phaseId': 'ferry-watch', 'kind': 'trigger', 'index': 0, 'id': 'begin-deep-freeze', 'part': 'actions[1]'})
        self.assertEqual(where['phases[2].effects[1]']['kind'], 'effect')
        with self.assertRaises(ValueError):
            rules.validate(p)

    def test_duplicates_missions_exits_and_the_graph(self):
        p = copy.deepcopy(EXAMPLES['river-relief'])
        phase = p['phases'][0]
        phase['missions'].append(copy.deepcopy(phase['missions'][0]))
        phase['exits'].append({'on': 'clock', 'to': phase['id'], 'afterDays': -1})
        found = rules.problems(p)
        self.assertIn(('phases[0].missions[%d]' % (len(phase['missions']) - 1), 'Each mission needs a unique ID and title.'),
                      {(x['path'], x['text']) for x in found})
        self.assertIn('phases[0].exits[%d]' % (len(phase['exits']) - 1), {x['path'] for x in found})
        # An unreachable phase is told once every phase is sound.
        q = copy.deepcopy(EXAMPLES['wagon'])
        q['phases'].append({'id': 'island', 'journal': 'No way here.', 'effects': [], 'exits': []})
        self.assertEqual([(x['path'], x['text']) for x in rules.problems(q)], [('phases', 'Unreachable phases: island')])

    def test_not_an_object_and_missing_fields(self):
        self.assertEqual(rules.problems([])[0]['text'], 'A story program is a JSON object.')
        found = rules.problems({'format': 'ratw-story-program'})
        self.assertTrue(found and found[0]['path'] == '')
        self.assertIn('Missing fields', found[0]['text'])

    def test_problems_and_validate_agree_on_every_draft(self):
        """Seeded mutations of every example: validate refuses a draft exactly when problems lists a structural error."""
        rng = random.Random(85)
        junk = [None, '', 'bad id!', -1, 10**12, [], {}, {'do': 'nonsense'}, {'test': 'nonsense'}, 'deep-freeze', True, 1.5]
        checked = 0
        for name, base in EXAMPLES.items():
            for _ in range(400):
                p = copy.deepcopy(base)
                for _ in range(rng.choice((1, 1, 2, 3))):
                    mutate(p, rng, junk)
                try:
                    rules.validate(copy.deepcopy(p))
                    valid = True
                except (ValueError, KeyError, TypeError, AttributeError, IndexError, StopIteration):
                    valid = False
                found = structural(rules.problems(p))
                self.assertEqual(valid, not found, (name, json.dumps(p)[:400], found[:3]))
                # And the walk on its own, without validate's verdict as its safety net, agrees too.
                with patch.object(rules, 'validate', side_effect=ValueError('NET')):
                    walked = [x for x in structural(rules._problems(p)) if x['text'] != 'NET']
                self.assertEqual(valid, not walked, (name, json.dumps(p)[:400], walked[:3]))
                checked += 1
        self.assertEqual(checked, 3200)

    def test_warnings_the_program_alone_can_tell(self):
        p = copy.deepcopy(EXAMPLES['winter-ferries'])
        p['triggers'][1]['actions'].append({'do': 'emit', 'signal': 'releif'})
        p['triggers'][2]['if'] = {'test': 'counter', 'key': 'stroes', 'scope': 'story', 'op': '>', 'value': 1}
        p['triggers'][2]['actions'].append({'do': 'cancel', 'signal': 'never-started'})
        found = [x for x in rules.problems(p) if x['level'] == 'warning']
        texts = {(x['path'], x['text']) for x in found}
        self.assertIn(('triggers[1].actions[1]', 'Signal "releif" is sent but nothing listens for it.'), texts)
        self.assertIn(('triggers[2].if', 'Counter "stroes" (story) is read but nothing sets it: it stays 0 (a misspelt name?).'), texts)
        self.assertIn(('triggers[2].actions[1]', 'Timer "never-started" is cancelled but nothing starts it.'), texts)
        q = copy.deepcopy(EXAMPLES['winter-ferries'])
        q['triggers'][2]['signal'] = 'nobody-sends-this'
        self.assertIn(('triggers[2]', 'Nothing in the story sends signal "nobody-sends-this": only the Campaigns console could.'),
                      {(x['path'], x['text']) for x in rules.problems(q)})


def mutate(p, rng, junk):
    """One random change somewhere in the draft: a value replaced, a key removed, a list item duplicated or dropped."""
    nodes = []

    def walk(node, depth):
        if depth > 7:
            return
        if isinstance(node, dict):
            nodes.append(node)
            for v in node.values():
                walk(v, depth + 1)
        elif isinstance(node, list):
            nodes.append(node)
            for v in node:
                walk(v, depth + 1)

    walk(p, 0)
    node = rng.choice(nodes)
    if isinstance(node, dict) and node:
        key = rng.choice(sorted(node))
        what = rng.random()
        if what < 0.25:
            del node[key]
        elif what < 0.9:
            node[key] = copy.deepcopy(rng.choice(junk))
        else:
            node['unexpected'] = 1
    elif isinstance(node, list) and node:
        i = rng.randrange(len(node))
        if rng.random() < 0.5:
            node.insert(i, copy.deepcopy(node[i]))
        else:
            del node[i]


if __name__ == '__main__':
    unittest.main()
