"""A version 2 story run by the world's story runner (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 5), in a
throwaway, migrated database: declared variables in the run's state and the world's (`dm.story_world_vars`, migration
0077), a wait that resumes when the world's clock reaches it, notes in the history, a trigger that starts off and is
turned on from the console, typed values set from the console, the dry run and "why not" over the new conditions, and the
story finishing itself (the game told with `story.finish`, then complete). The game is stood in for by answering
dm.actions rows by hand. Skipped without the local PostgreSQL."""

import json
import unittest

import story_program as rules
import story_runner as runner
import test_story_console as console
import test_story_steering as steering
from test_story_console import WOLVES
from test_world_db import database_available

ASH, BIRCH, ELM = sorted(WOLVES)

PROGRAM = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-runner', 'title': 'Two words with Ivo',
    'audience': {'kind': 'players', 'players': sorted(WOLVES)}, 'start': 'open',
    'variables': [
        {'id': 'tally', 'type': 'number', 'scope': 'story', 'show': 'Words had'},
        {'id': 'best', 'type': 'number', 'scope': 'world'},
        {'id': 'seen', 'type': 'wolves', 'scope': 'story'},
        {'id': 'mood', 'type': 'text', 'scope': 'story', 'initial': 'calm'},
        {'id': 'asked', 'type': 'switch', 'scope': 'player'},
    ],
    'phases': [{'id': 'open', 'journal': 'Talk with Ivo twice.', 'effects': [], 'exits': [], 'triggers': [
        {'id': 'count', 'on': 'conversation', 'audience': 'participant', 'match': {'target': 'rm111'}, 'actions': [
            {'do': 'add', 'var': 'tally', 'value': 1},
            {'do': 'turn on', 'var': 'asked'},
            {'do': 'list add', 'var': 'seen', 'value': {'field': 'actor'}},
            {'do': 'set', 'var': 'best', 'value': {'max': [{'var': 'best'}, {'var': 'tally'}]}},
            {'do': 'if', 'if': {'test': 'compare', 'left': {'var': 'tally'}, 'op': '>=', 'right': 2},
             'then': [{'do': 'note', 'text': 'Two words had.'}, {'do': 'wait', 'days': 0.5},
                      {'do': 'effect', 'effect': {'kind': 'deed.award', 'player': '$actor', 'detail': 'Heard Ivo out.'}},
                      {'do': 'finish', 'outcome': 'success', 'text': 'Ivo was heard.'}]},
        ]},
        {'id': 'late', 'on': 'arrival', 'enabled': False, 'if': {'test': 'switch', 'var': 'asked', 'is': True},
         'actions': [{'do': 'note', 'text': 'A late arrival.'}, {'do': 'set', 'var': 'mood', 'value': 'busy'}]},
    ]}],
    'tests': [{'name': 'Two words', 'cohort': sorted(WOLVES),
               'events': [{'id': 'a', 'kind': 'conversation', 'day': 1, 'actor': ASH, 'target': 'rm111'},
                          {'id': 'b', 'kind': 'conversation', 'day': 1.1, 'actor': BIRCH, 'target': 'rm111'},
                          {'id': 'c', 'kind': 'arrival', 'day': 2, 'actor': ELM}],
               'expect': ['open'], 'expectOutcome': 'success',
               'expectVariables': {'story': {'tally': 2, 'seen': [ASH, BIRCH]}, 'world': {'best': 2}}}],
}


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class VersionTwoRuns(unittest.TestCase):
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    fact, launch, answer, tick, rewrite, row = (console.ConsoleReads.fact, console.ConsoleReads.launch, console.ConsoleReads.answer,
                                                console.ConsoleReads.tick, console.ConsoleReads.rewrite, console.ConsoleReads.row)
    one, send, taken, steer, answer_all, settle = (steering.Steering.one, steering.Steering.send, steering.Steering.taken,
                                                   steering.Steering.steer, steering.Steering.answer_all, steering.Steering.settle)

    def setUp(self):
        console.ConsoleReads.setUp(self)
        self.examples = {**self.examples, 'v2-runner': json.loads(json.dumps(PROGRAM))}

    def day(self, n):
        self.rewrite(f"UPDATE game.checkpoints SET payload='{{\"calendarDays\":{n}}}' WHERE world_id='{self.world}'")

    def state(self):
        return self.one()['state']

    def world_vars(self):
        return dict(self.conn.execute('SELECT name,value FROM dm.story_world_vars WHERE world_id=%s', (self.world,)).fetchall())

    def test_the_rehearsal_passes(self):
        self.assertTrue(rules.review(PROGRAM)['passed'])

    def test_a_version_2_story_waits_notes_keeps_a_world_value_and_finishes(self):
        self.launch('v2-runner')
        self.assertEqual(self.state()['disabled'], ['open/late'], 'it starts with "late" off')
        self.assertEqual(self.state()['seed'], self.run_id, "the run's id seeds its rolls")
        self.answer()
        self.fact('conversation', ASH, 'rm111')
        self.tick()
        self.assertEqual(self.state()['counters']['story'], {'tally': 1, 'seen': [ASH]})
        self.assertEqual(self.world_vars(), {'best': 1}, 'the world keeps its value')
        self.fact('conversation', BIRCH, 'rm111')
        self.tick()
        state = self.state()
        self.assertEqual([w['trigger'] for w in state['waiting']], ['count'])
        self.assertEqual(state['waiting'][0]['at'], 10.5)
        history = self.conn.execute('SELECT entry FROM dm.story_trigger_history WHERE run=%s ORDER BY id', (self.run_id,)).fetchall()
        self.assertEqual(history[-1][0]['notes'], ['Two words had.'])
        changes = {(c['key'], json.dumps(c['after'])) for c in history[-1][0]['changes']}
        self.assertIn(('tally', '2'), changes)
        self.assertIn(('best', '2'), changes)
        self.assertEqual(self.world_vars(), {'best': 2})
        timeline = runner.timeline(self.conn, self.world, self.run_id)
        self.assertEqual([i['notes'] for i in timeline['items'] if i.get('notes')], [['Two words had.']])
        # Half a day later the rest of its actions run: the deed, then the story ends itself.
        self.tick()
        self.assertIn('waiting', self.state(), 'not due yet')
        self.day(10.6)
        self.tick()
        state = self.state()
        self.assertNotIn('waiting', state)
        resumed = self.conn.execute('SELECT entry FROM dm.story_trigger_history WHERE run=%s ORDER BY id DESC LIMIT 1', (self.run_id,)).fetchone()[0]
        self.assertEqual((resumed['trigger'], resumed['via']), ('count', 'wait'))
        self.assertEqual(state['outcome']['outcome'], 'success')
        self.settle()
        deed = self.conn.execute("SELECT id,target_id,payload FROM dm.actions WHERE kind='deed.award'").fetchall()
        self.assertEqual([d[1:] for d in deed], [(BIRCH, {'detail': 'Heard Ivo out.'})], 'for the wolf who waited, with the event it waited with')
        self.assertEqual(self.one()['status'], 'complete')
        finish = self.conn.execute("SELECT id,payload FROM dm.actions WHERE kind='story.finish' AND target_id=%s", (self.run_id,)).fetchall()
        self.assertEqual([f[1] for f in finish], [{'outcome': 'success', 'text': 'Ivo was heard.'}])
        self.assertLess(deed[0][0], finish[0][0], 'the deed goes before the story is told it is over')
        # Another run of a story declaring "best" reads the world's value.
        other = json.loads(json.dumps(PROGRAM))
        state = rules.begin(other, sorted(WOLVES), 0)
        runner.world_load(self.conn, self.world, other, state)
        self.assertEqual(state['counters']['world'], {'best': 2})
        other['variables'][1]['type'] = 'text'
        state = rules.begin(other, sorted(WOLVES), 0)
        runner.world_load(self.conn, self.world, other, state)
        self.assertNotIn('counters', state, 'a value kept under another type is not read')

    def test_the_console_turns_on_a_trigger_that_starts_off_and_sets_typed_values(self):
        self.launch('v2-runner')
        self.answer()
        controls = self.one()['controls'] if 'controls' in self.one() else runner.controls(self.examples['v2-runner'], self.state())
        late = next(t for t in controls['triggers'] if t['id'] == 'late')
        self.assertTrue(late['off'])
        variables = {v['key']: v for v in controls['counters']}
        self.assertEqual(variables['mood'], {'key': 'mood', 'scope': 'story', 'type': 'text', 'initial': 'calm'})
        self.assertEqual(variables['tally']['show'], 'Words had')
        with self.assertRaisesRegex(ValueError, r'Choose a value of its type \(text\)'):
            self.send('set', {'key': 'mood', 'scope': 'story', 'value': 3})
        status, result = self.steer('set', {'key': 'mood', 'scope': 'story', 'value': 'wary'})
        self.assertEqual((status, result['before'], result['after']['value']), ('done', {'value': 'calm'}, 'wary'))
        status, result = self.steer('set', {'key': 'asked', 'scope': 'player', 'player': ELM, 'value': True})
        self.assertEqual(status, 'done')
        status, result = self.steer('set', {'key': 'best', 'scope': 'world', 'value': 7})
        self.assertEqual(self.world_vars(), {'best': 7}, 'set from the console, kept for the world')
        # Why not: "late" is off; then turned on, its condition is judged for the wolf.
        why = runner.explain(self.conn, self.world, self.run_id, {'trigger': 'late'})
        self.assertTrue(why)
        self.assertEqual(self.steer('enable', {'trigger': 'late'})[0], 'done')
        self.assertNotIn('disabled', self.state())
        judged = next(i for i in runner.explain(self.conn, self.world, self.run_id)['listeners'] if i.get('id') == 'late')
        self.assertTrue(judged['startsOff'])
        self.assertEqual(judged['forWolves'], [ELM], 'only the wolf whose own switch is on')
        self.fact('arrival', ELM)
        self.tick()
        self.assertEqual(self.state()['counters']['story']['mood'], 'busy')

    def test_a_dry_run_of_fire_on_a_version_2_trigger(self):
        self.launch('v2-runner')
        self.answer()
        out = runner.explain(self.conn, self.world, self.run_id, {'command': {'kind': 'fire', 'payload': {'trigger': 'count', 'actor': ASH,
                                                                                                         'check': False}}}, 'martin')
        self.assertEqual(out['outcome'], 'done', out)
        self.assertEqual(self.state().get('counters', {}).get('story', {}).get('tally'), None, 'a dry run changes nothing')
        status, result = self.steer('fire', {'trigger': 'count', 'actor': ASH, 'check': False})
        self.assertEqual(status, 'done', result)
        self.assertEqual(self.state()['counters']['story']['tally'], 1)
        self.assertEqual(self.state()['counts'], {'visit': {'open/count': 1}, 'run': {'open/count': 1}})


if __name__ == '__main__':
    unittest.main()
