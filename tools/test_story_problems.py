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
import story_v2_samples
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


class ParticipationProblems(unittest.TestCase):
    """Doc 86, R1's checks: what a test for each wolf may not hold, and the floor above the cap."""

    def refused(self, condition, words):
        p = copy.deepcopy(story_v2_samples.READY)
        p['phases'][0]['triggers'][1]['if'] = condition
        found = [x for x in rules.problems(p) if x['level'] == 'error']
        self.assertTrue(any(words in x['text'] for x in found), found)
        self.assertRaisesRegex(ValueError, words, rules.validate, p)

    def test_a_fact_condition_inside_where_is_refused(self):
        self.refused({'test': 'participation', 'where': {'test': 'wolf indoors', 'who': '$each', 'is': True}, 'percent': 10},
                     "not the world's facts")

    def test_a_numbers_list_is_refused(self):
        p = copy.deepcopy(story_v2_samples.READY)
        p['variables'].append({'id': 'tally', 'type': 'numbers', 'scope': 'story'})
        p['phases'][0]['triggers'][1]['if'] = {'test': 'participation', 'list': 'tally', 'percent': 10}
        self.assertRaisesRegex(ValueError, 'list of wolves', rules.validate, p)

    def test_floor_above_cap_is_refused(self):
        self.refused({'test': 'participation', 'list': 'did', 'percent': 10, 'floor': 9, 'cap': 3}, 'The floor is above the cap')

    def test_a_wolf_value_read_without_each_is_refused(self):
        self.refused({'test': 'participation', 'where': {'test': 'compare', 'left': {'var': 'hauls'}, 'op': '>', 'right': 1},
                      'percent': 10}, '"hauls" of each wolf')


class ValuesProblems(unittest.TestCase):
    """Doc 86, Phase 2: what the Problems list says of values in effects, fills, filters and camps (no database)."""

    def test_a_wolf_variable_in_a_resident_parameter(self):
        import test_story_v2 as v2
        p = v2._values_program([{'do': 'effect', 'effect': {'kind': 'npc.walk', 'npc': {'var': 'finder'}, 'cell': 'grey', 'x': 3, 'y': 4,
                                                             'pace': 'walk', 'cue': 'c'}}])
        found = structural(rules.problems(p))
        self.assertTrue(found and found[0]['path'] == 'phases[0].triggers[0].actions[0]' and 'needs a resident' in found[0]['text'], found)

    def test_the_sample_has_none(self):
        self.assertEqual([x for x in rules.problems(story_v2_samples.VALUES) if x['level'] == 'error'], [])


try:
    import test_story_talk as talk
    from test_world_db import database_available
except ImportError:   # (pragma: no cover)
    talk, database_available = None, lambda: False


@unittest.skipUnless(talk is not None and database_available(), 'local PostgreSQL required')
class ValuesInTheWorld(unittest.TestCase):
    """Doc 86, Phase 2's approval checks in a throwaway world (the console's fixture): an objective's cells, a named story
    camp, a purse a value reads, and values in effects passed to the game to refuse."""
    setUpClass = classmethod(talk.console.ConsoleReads.setUpClass.__func__) if talk else None
    tearDownClass = classmethod(talk.console.ConsoleReads.tearDownClass.__func__) if talk else None
    fact = talk.console.ConsoleReads.fact if talk else None

    def setUp(self):
        talk.InTheWorld.setUp(self)

    def refs(self, p, chapters=False):
        return runner.references(self.conn, self.world, rules.validate(p), chapters=chapters)

    def quest(self, objective, **more):
        p = talk.program()
        p['quests'] = [{'id': 'q', 'title': 'Q', 'objectives': [{'id': 'a', 'label': 'Do it.', **objective}], **more}]
        return p

    def test_an_unknown_cell_in_where_is_refused(self):
        self.refs(self.quest({'on': 'hunted', 'where': {'cells': ['gate']}}))
        self.assertRaisesRegex(ValueError, 'Unknown cell in this world for "where": moon', self.refs,
                               self.quest({'on': 'hunted', 'where': {'cells': ['gate', 'moon']}}))
        found = runner.reference_problems(self.conn, self.world, self.quest({'on': 'hunted', 'where': {'cells': ['moon']}}))
        self.assertTrue(any('"where": moon' in x['text'] for x in found), found)

    def test_a_named_camp_and_its_objective(self):
        p = self.quest({'on': 'camp cleared', 'match': {'target': 'camp_story_sq_gate'}})
        self.assertRaisesRegex(ValueError, 'is spawned by this story', self.refs, p)
        p['prefix'] = 'sq'
        p['phases'][0]['triggers'][0]['actions'] = [talk.fx('bandits.call', player='$actor', count=2, camp='sq_gate')]
        self.refs(p)

    def test_each_name_of_a_one_of_filter_is_checked(self):
        # (Review of Phase 2: an objective's "one of" was passed over by the camp, resident and item checks.)
        self.assertRaisesRegex(ValueError, 'No camp "camp_nowhere"', self.refs,
                               self.quest({'on': 'camp cleared', 'match': {'target': {'in': ['camp_gate', 'camp_nowhere']}}}))
        self.assertRaisesRegex(ValueError, 'No item "moonstone"', self.refs,
                               self.quest({'on': 'gift', 'match': {'item': {'in': ['bread', 'moonstone']}}}))
        self.assertRaisesRegex(ValueError, 'rm_nobody', self.refs,
                               self.quest({'on': 'gift', 'match': {'target': {'in': ['rm111', 'rm_nobody']}}}))
        self.refs(self.quest({'on': 'camp cleared', 'match': {'target': {'starts': 'camp_'}}}))   # (A "begins with" names none.)

    def test_a_purse_a_value_reads(self):
        grant = lambda purse: talk.fx('character.grant', player='$actor', purse='town:greyfen', reason='r',
                                      coins={'min': [12, {'div': [{'purse': purse}, 20]}]})
        self.refs(talk.program([grant('till:job:rm111')]))
        self.assertRaisesRegex(ValueError, 'No purse till:job:nobody', self.refs, talk.program([grant('till:job:nobody')]))
        p = self.quest({'on': 'gift', 'goal': {'max': [2, {'div': [{'purse': 'town:atlantis'}, 50]}]}})
        self.assertRaisesRegex(ValueError, 'No town "atlantis"', self.refs, p)

    def test_values_in_effects_are_the_games_to_refuse(self):
        p = talk.program([talk.fx('npc.walk', npc={'field': 'target'}, cell='gate', x={'field': 'quantity'}, y=2, pace='walk', cue='c'),
                          talk.fx('item.give', player={'field': 'target'}, item='bread', quantity={'field': 'quantity'})])
        self.refs(p)


if __name__ == '__main__':
    unittest.main()
