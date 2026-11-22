"""Explaining a running campaign (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 3, "Explain it"), on runs of
the bundled examples in a throwaway, migrated database: what a run waits for (each trigger and exit judged, conditions
true, false or unknown with their values, for which wolves), why a trigger has not fired (its last events and where each
stopped), what a made-up event would do, and what a console command would do (the runner's own code, rolled back:
nothing saved, queued or audited); the new alerts (cannot move on, a timer overdue, the audience nearly full) with each
world's rules and a program's quiet hours; the summary for the tab bar and LIVE; the alerts the runner leaves for the game
(and what the game may read of them); every problem of a draft with the world's references; and the route's permissions.
The game is stood in for by answering dm.actions rows by hand. Skipped without the local PostgreSQL."""

import copy
import json
import secrets
import threading
import unittest
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer

import dungeon_master as D
import story_program as rules
import story_runner as runner
import story_triggers
import world_db as W
import test_story_console as console
from test_story_console import WOLVES
from test_world_db import database_available

ASH, BIRCH, ELM = sorted(WOLVES)


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class Explaining(unittest.TestCase):
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    setUp = console.ConsoleReads.setUp
    fact, launch, answer, tick, rewrite, row = (console.ConsoleReads.fact, console.ConsoleReads.launch, console.ConsoleReads.answer,
                                                console.ConsoleReads.tick, console.ConsoleReads.rewrite, console.ConsoleReads.row)

    def one(self):
        return runner.status(self.conn, self.world, self.run_id)

    def answer_all(self):
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute("UPDATE dm.actions SET status='applied',result='ok',done_at=now() WHERE status='queued'")
        self.conn.execute('SET LOCAL ROLE ratw_dm')

    def settle(self, rounds=6):
        for _ in range(rounds):
            self.answer_all()
            self.tick()

    def ferries_in_watch(self):
        self.launch('winter-ferries')
        self.answer()
        self.fact('arrival', ASH)
        self.tick()
        self.settle()
        self.assertEqual(self.one()['phase'], 'ferry-watch')

    def explain(self, **data):
        return runner.explain(self.conn, self.world, self.run_id, data, 'martin')

    def counts(self):
        return {t: self.conn.execute(f'SELECT count(*) FROM {t}').fetchone()[0]
                for t in ('dm.actions', 'dm.story_trigger_history', 'dm.story_transitions', 'dm.audit')}

    # ---- what it waits for
    def test_every_trigger_judged_with_its_conditions_values_and_wolves(self):
        self.ferries_in_watch()
        out = self.explain()
        by = {i['id']: i for i in out['listeners'] if i['source'] == 'trigger'}
        self.assertEqual(set(by), {'open-winter-ledger', 'offer-a-coil', 'log-watch-round', 'stocked-at-thaw', 'short-at-thaw',
                                   'begin-deep-freeze'})
        self.assertTrue(by['open-winter-ledger']['used'], 'once per campaign, and it fired')
        self.assertFalse(by['open-winter-ledger']['can'])
        self.assertEqual(by['open-winter-ledger']['cannot'], 'used')
        coil = by['offer-a-coil']
        self.assertEqual((coil['on'], coil['match']), ('gift', {'item': 'rope', 'target': 'rm111'}))
        self.assertEqual(sorted(coil['forWolves']), sorted(WOLVES), 'a participant trigger: any wolf of the story')
        stocked = by['stocked-at-thaw']['conditions']
        self.assertEqual((stocked['test'], stocked['key'], stocked['value'], stocked['expected'], stocked['verdict']),
                         ('counter', 'stores', 0, 5, False), 'stores is 0, it needs 5')
        self.assertEqual([t['signal'] for t in by['log-watch-round']['timers']], ['watch-round'])
        self.assertTrue(by['log-watch-round']['can'], 'a timer runs for its signal')
        self.assertEqual([t['signal'] for t in out['timers']], ['watch-round', 'first-freeze', 'spring-council'], 'soonest first')
        self.assertIsNone(out['stall'])
        wolves = next(w for w in self.row()['waiting'] if w['on'] == 'wolves')
        self.assertNotIn('open-winter-ledger', [w.get('id') for w in wolves['what']], 'a used-up trigger waits for nothing')
        # A counter set by a gift changes the verdict.
        self.fact('gift', BIRCH, 'rm111', item='rope', quantity=5)
        self.tick()
        stocked = next(i for i in self.explain()['listeners'] if i.get('id') == 'stocked-at-thaw')['conditions']
        self.assertEqual((stocked['value'], stocked['verdict']), (5, True))

    def test_judge_agrees_with_satisfied(self):
        """The recording form gives satisfied's verdict on every node it records, for real events (known=None)."""
        state = {'cohort': [ASH], 'counters': {'story': {'a': 3}, 'player': {ASH: {'b': 1}}}}
        regions = {'r': {'cell': 'c', 'x1': 0, 'y1': 0, 'x2': 3, 'y2': 3}}
        leaves = [{'test': 'counter', 'key': 'a', 'scope': 'story', 'op': '>=', 'value': 3},
                  {'test': 'counter', 'key': 'b', 'scope': 'player', 'op': '=', 'value': 2},
                  {'test': 'field', 'field': 'item', 'op': '=', 'value': 'rope'},
                  {'test': 'field', 'field': 'coins', 'op': '>', 'value': 4},
                  {'test': 'region', 'region': 'r'}, {'test': 'mission', 'id': 'm'}]
        events = [{'actor': ASH, 'item': 'rope', 'coins': 5, 'cell': 'c', 'x': 1, 'y': 1}, {'actor': BIRCH, 'item': 'nail'},
                  {'actor': ASH, 'coins': 1}]
        import itertools
        trees = list(leaves) + [{'test': 'not', 'condition': leaf} for leaf in leaves]
        for a, b in itertools.combinations(leaves, 2):
            trees += [{'test': 'all', 'conditions': [a, b]}, {'test': 'any', 'conditions': [a, {'test': 'not', 'condition': b}]}]
        for tree in trees:
            for ev in events:
                for ready in (set(), {'m'}):
                    want = story_triggers.satisfied(tree, state, ev, lambda: ready, regions)
                    got = story_triggers.judge(tree, state, ev, lambda: ready, regions)['verdict']
                    self.assertIs(got, want, (tree, ev))

    # ---- why not
    def test_why_a_trigger_has_not_fired(self):
        self.ferries_in_watch()
        self.fact('gift', BIRCH, 'rm112', item='rope', quantity=1)          # the wrong resident
        self.fact('gift', BIRCH, 'rm111', item='nail', quantity=1)          # the wrong thing
        self.fact('gift', 'player-stranger', 'rm111', item='rope', quantity=1)   # not in the story
        self.fact('gift', ELM, 'rm111', item='rope~fine', quantity=2)       # a fine rope counts as rope: it fires
        self.tick()
        self.answer_all()
        self.disable('offer-a-coil')
        self.fact('gift', ASH, 'rm111', item='rope', quantity=1)            # turned off now
        self.tick()
        out = self.explain(trigger='offer-a-coil')
        self.assertTrue(out['listener']['off'])
        self.assertIsInstance(self.one()['state']['visitCursor'], int, 'a visit keeps the cursor it opened at')
        verdicts = [(e['event']['actor'], e['verdict']) for e in out['events']]
        self.assertEqual(verdicts[:5], [(ASH, 'off'), (ELM, 'fired'), ('player-stranger', 'outsider'), (BIRCH, 'filter'), (BIRCH, 'filter')])
        self.assertEqual((out['events'][3]['field'], out['events'][3]['expected'], out['events'][3]['actual']), ('item', 'rope', 'nail'))
        self.assertEqual((out['events'][4]['field'], out['events'][4]['actual']), ('target', 'rm112'))
        # A signal trigger is not in the world's log: what can send it is said instead.
        out = self.explain(trigger='begin-deep-freeze')
        self.assertEqual(out['notLogged'], 'story signal')
        self.assertEqual([t['signal'] for t in out['listener']['timers']], ['first-freeze'])
        # A condition false, with its value.
        out = self.explain(trigger='stocked-at-thaw')
        self.assertEqual(out['listener']['conditions']['value'], 2)
        with self.assertRaisesRegex(ValueError, 'not in phase'):
            self.explain(trigger='no-such-trigger')

    def disable(self, trigger):
        one = self.one()
        sent = runner.command(self.conn, self.world, 'martin', {'run': self.run_id, 'kind': 'disable', 'payload': {'trigger': trigger},
                                                               'reason': 'testing why not', 'key': secrets.token_hex(8),
                                                               'visit': one['visit'], 'digest': one['digest']})
        self.tick()
        self.assertEqual(self.conn.execute('SELECT status FROM dm.story_commands WHERE id=%s', (sent['id'],)).fetchone()[0], 'done')

    def test_an_exit_why_not_and_its_gate(self):
        self.launch('river-relief')
        self.answer()
        self.tick()
        phase = rules.phase_of(self.one()['program'], self.one()['state'])
        exits = self.explain()['listeners']
        gates = [i for i in exits if i['source'] == 'exit']
        self.assertTrue(gates, phase['exits'])
        first = gates[0]
        out = self.explain(exit=first['index'])
        self.assertEqual(out['listener']['index'], first['index'])
        with self.assertRaisesRegex(ValueError, 'No such exit'):
            self.explain(exit=99)

    # ---- what if
    def test_what_if_says_what_would_happen_and_saves_nothing(self):
        self.ferries_in_watch()
        before, state = self.counts(), json.dumps(self.one()['state'], sort_keys=True)
        out = self.explain(fact={'kind': 'gift', 'actor': ASH, 'target': 'rm111', 'item': 'rope', 'quantity': 4})
        self.assertEqual([f['trigger'] for f in out['fired']], ['offer-a-coil'])
        self.assertEqual(out['changes'], [{'what': 'counter', 'scope': 'story', 'key': 'stores', 'before': 0, 'after': 4}])
        self.assertFalse(out['moved'])
        self.assertEqual(self.counts(), before)
        self.assertEqual(json.dumps(self.one()['state'], sort_keys=True), state, 'nothing saved')
        # A signal that moves the story: the phase it lands in, and that phase's entry effects to follow.
        out = self.explain(fact={'kind': 'story signal', 'signal': 'first-freeze'})
        self.assertEqual((out['moved'], out['phase']), (True, 'deep-freeze'))
        self.assertEqual([e['kind'] for e in out['entryEffects']],
                         [e['kind'] for e in runner.entry_effects(rules.phase_of(self.one()['program'], {'phase': 'deep-freeze'}), {})])
        # Who did not fire, and why.
        out = self.explain(fact={'kind': 'gift', 'actor': ASH, 'target': 'rm111', 'item': 'nail'})
        self.assertEqual(out['fired'], [])
        self.assertEqual([(n['trigger'], n['verdict'], n['field']) for n in out['notFired']], [('offer-a-coil', 'filter', 'item')])
        # A made-up scene or region event is this story's, this visit's (not "another story's").
        out = self.explain(fact={'kind': 'region entered', 'actor': ASH, 'target': 'nowhere'})
        self.assertEqual(out['fact']['detail'], self.run_id)
        with self.assertRaisesRegex(ValueError, 'world event'):
            self.explain(fact={'kind': 'clock'})
        with self.assertRaisesRegex(ValueError, 'Unknown field'):
            self.explain(fact={'kind': 'gift', 'run': 'x'})

    # ---- what a command would do
    def test_a_command_dry_run_tells_what_it_would_do_and_does_nothing(self):
        self.ferries_in_watch()
        before, state = self.counts(), json.dumps(self.one()['state'], sort_keys=True)
        out = self.explain(command={'kind': 'fire', 'payload': {'trigger': 'offer-a-coil', 'actor': BIRCH}})
        self.assertEqual(out['outcome'], 'done', out)
        self.assertEqual(out['changes'], [], 'by hand, the gift\'s quantity is none: stores stays 0')
        self.assertEqual([f['trigger'] for f in out['fired']], ['offer-a-coil'])
        out = self.explain(command={'kind': 'enter', 'payload': {'phase': 'deep-freeze', 'effects': True}})
        self.assertEqual((out['outcome'], out['moved'], out['phase']), ('done', True, 'deep-freeze'))
        self.assertEqual([o['kind'] for o in out['orders']], ['story.phase'], 'its journal at once')
        landing = rules.phase_of(self.one()['program'], {'phase': 'deep-freeze'})
        self.assertEqual(out['next'], runner.entry_effects(landing, {}), 'then its entry effects')
        out = self.explain(command={'kind': 'set', 'payload': {'key': 'stores', 'scope': 'story', 'value': 9}})
        self.assertEqual((out['outcome'], out['after']['value']), ('done', 9))
        self.assertEqual(out['changes'], [{'what': 'counter', 'scope': 'story', 'key': 'stores', 'before': 0, 'after': 9}])
        out = self.explain(command={'kind': 'fire', 'payload': {'trigger': 'stocked-at-thaw', 'check': True}})
        self.assertEqual(out['outcome'], 'refused')
        self.assertIn('false', out['text'])
        out = self.explain(command={'kind': 'resume', 'payload': {}})
        self.assertEqual(out['outcome'], 'refused')
        self.assertEqual(self.counts(), before, 'nothing queued, recorded or audited')
        self.assertEqual(json.dumps(self.one()['state'], sort_keys=True), state, 'nothing saved')
        with self.assertRaisesRegex(ValueError, 'no such phase'):
            self.explain(command={'kind': 'enter', 'payload': {'phase': 'nowhere'}})

    def test_a_flow_command_dry_run_waits_while_orders_are_unanswered(self):
        self.launch('winter-ferries')
        out = self.explain(command={'kind': 'signal', 'payload': {'name': 'watch-round'}})
        self.assertEqual(out['outcome'], 'waits', out)

    # ---- the new alerts
    def test_cannot_move_on_when_nothing_left_can_fire(self):
        self.ferries_in_watch()
        self.assertNotIn('stuck', [a['kind'] for a in self.row()['alerts']])
        state = self.one()['state']
        state['timers'] = {}                     # (No timer left to send the signals its triggers wait for.)
        self.conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), self.run_id))
        # offer-a-coil (a participant gift) can still fire while a wolf of the story is about.
        self.assertNotIn('stuck', [a['kind'] for a in self.row()['alerts']])
        self.rewrite(f"UPDATE game.events SET recorded_at = now() - interval '20 days' WHERE world_id='{self.world}'")
        alerts = {a['kind']: a for a in self.row()['alerts']}
        self.assertIn('stuck', alerts)
        self.assertTrue(alerts['stuck']['needsYou'])
        self.assertEqual(alerts['stuck']['phase'], 'ferry-watch')
        self.assertIn('gone', alerts['stuck']['reasons'])
        self.assertIn('unsent', alerts['stuck']['reasons'])
        self.assertIsNotNone(self.explain()['stall'])
        # The owner's rule: gone for 30 days, not 14 -> it can move again.
        runner.set_alert_rules(self.conn, self.world, 'martin', {'rules': {'goneDays': 30}})
        self.assertNotIn('stuck', [a['kind'] for a in self.row()['alerts']])
        summary = runner.alert_summary(self.conn, self.world)
        self.assertEqual(summary['needsYou'], 0)

    def test_a_timer_feeding_a_participant_signal_is_not_stuck(self):
        """A running timer sends its signal as the wolf who started it, gone or not: the phase can still move."""
        p = {'format': 'ratw-story-program', 'version': 1, 'id': 'x', 'title': 'X', 'start': 'a', 'audience': {'kind': 'players', 'players': [ASH]},
             'phases': [{'id': 'a', 'journal': 'Wait.', 'effects': [], 'exits': [], 'triggers': [
                 {'id': 'ring', 'on': 'story signal', 'signal': 'bell', 'audience': 'participant', 'actions': [{'do': 'enter', 'phase': 'b'}]}]},
                 {'id': 'b', 'journal': 'Done.', 'effects': [], 'exits': []}], 'tests': []}
        state = {'phase': 'a', 'entered': 1, 'cohort': [ASH], 'votes': {}, 'visits': 1, 'actor': '',
                 'timers': {'bell': {'signal': 'bell', 'at': 5, 'clock': 'game', 'actor': ASH, 'interval': 0, 'scope': 'visit'}}}
        ring = runner.listeners(p, state, per_wolf=False, recent=set())[0]
        self.assertTrue(ring['can'], ring)
        state['timers'] = {}
        ring = runner.listeners(p, state, per_wolf=False, recent=set())[0]
        self.assertEqual((ring['can'], ring['cannot']), (False, 'gone'))

    def test_overdue_timer_and_nearly_full_audience(self):
        self.ferries_in_watch()
        # A minute of real time is a game day here; a timer due 0.5 game days ago is 30 s late (under 300 s).
        self.rewrite(f"""UPDATE game.events SET recorded_at = now() - (10 - game_day) * interval '1 minute' WHERE world_id='{self.world}'""")
        self.fact('arrival', ASH, day=10)
        self.rewrite(f"""UPDATE game.events SET game_day = 8, recorded_at = now() - interval '2 minutes 30 seconds'
                         WHERE world_id='{self.world}' AND id=(SELECT min(id) FROM game.events WHERE world_id='{self.world}')""")
        speed = runner.world_speed(self.conn, self.world)
        self.assertAlmostEqual(speed, 2 / 150, places=3)
        state = self.one()['state']
        state['timers']['watch-round']['at'] = 9.0          # a game day ago: 75 s late at this speed
        self.conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), self.run_id))
        self.assertNotIn('overdue', [a['kind'] for a in self.row()['alerts']])
        state['timers']['watch-round']['at'] = 4.0          # six game days ago: 450 s late
        self.conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), self.run_id))
        overdue = next(a for a in self.row()['alerts'] if a['kind'] == 'overdue')
        self.assertEqual((overdue['timer'], overdue['needsYou']), ('watch-round', False))
        self.assertGreater(overdue['seconds'], 300)
        # The timers ahead carry an estimate in real time.
        timers = self.explain()['timers']
        self.assertIsNotNone(timers[-1]['seconds'])
        # Nearly full: 450 of the 500.
        state['cohort'] = sorted(set(state['cohort']) | {f'player-x{i}' for i in range(450)})
        self.conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), self.run_id))
        full = next(a for a in self.row()['alerts'] if a['kind'] == 'full')
        self.assertEqual((full['wolves'], full['cap']), (453, 500))

    def test_alert_rules_are_checked_and_a_program_has_its_own_quiet_hours(self):
        with self.assertRaisesRegex(ValueError, 'Unknown alert rule'):
            runner.set_alert_rules(self.conn, self.world, 'martin', {'rules': {'nonsense': 1}})
        with self.assertRaisesRegex(ValueError, 'quietHours'):
            runner.set_alert_rules(self.conn, self.world, 'martin', {'rules': {'quietHours': -1}})
        out = runner.set_alert_rules(self.conn, self.world, 'martin', {'rules': {'quietHours': 48, 'runnerSeconds': 60}})
        self.assertEqual((out['rules']['quietHours'], out['rules']['runnerSeconds'], out['rules']['gameSeconds']), (48, 60, 60))
        self.assertEqual(runner.overview(self.conn, self.world)['thresholds']['quietHours'], 48)
        out = runner.set_alert_rules(self.conn, self.world, 'martin', {'rules': {'quietHours': None}})
        self.assertEqual(out['rules']['quietHours'], 24, 'null: back to the first value')
        audit = self.conn.execute("SELECT count(*) FROM dm.audit WHERE action='story.alert-rules'").fetchone()[0]
        self.assertEqual(audit, 2, 'the two changes made are audited (not the refused)')
        # quietAfterHours: 0 is "slow on purpose".
        p = copy.deepcopy(self.examples['winter-ferries'])
        p['quietAfterHours'] = 0
        self.assertEqual(rules.problems(p), [])
        p['quietAfterHours'] = -2
        self.assertIn('Quiet after', rules.problems(p)[0]['text'])

    # ---- outside the tab: the tab bar, LIVE, the Dev Console
    def test_the_summary_and_the_alerts_left_for_the_game(self):
        self.ferries_in_watch()
        first = self.run_id
        self.launch('river-relief')
        self.answer('refused', 'that wolf is not in the world')
        self.tick()
        self.assertEqual(self.one()['status'], 'paused')
        summary = runner.alert_summary(self.conn, self.world)
        self.assertEqual(summary['needsYou'], 1)
        self.assertEqual([(r['run'], r['kinds']) for r in summary['runs']], [(self.run_id, ['paused'])])
        self.assertIn('paused', summary['runs'][0]['text'])
        self.assertFalse(summary['runnerDown'], 'the tick just beat')
        runner._game_alerts_at.clear()
        self.tick()
        stored = self.conn.execute('SELECT alerts,live FROM dm.story_runner WHERE world_id=%s', (self.world,)).fetchone()
        self.assertEqual([a['kind'] for a in stored[0]], ['paused'])
        self.assertEqual(stored[1], 1, 'one running (the other paused)')
        # What the game may read: only this function, nothing of the story tables.
        self.conn.execute('SET LOCAL ROLE ratw_game')
        told = self.conn.execute('SELECT dm.story_alerts_for_game(%s)', (self.world,)).fetchone()[0]
        self.assertEqual((told['running'], told['runnerSeconds'], [a['kind'] for a in told['alerts']]), (1, 30, ['paused']))
        self.assertLess(told['beatAge'], 30)
        import psycopg
        with self.assertRaises(psycopg.errors.InsufficientPrivilege):
            with self.conn.transaction():
                self.conn.execute('SELECT 1 FROM dm.story_runner')
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        # The runner silent: the summary says so (the game works it out from beatAge itself).
        self.rewrite(f"UPDATE dm.story_runner SET beat_at = now() - interval '2 minutes' WHERE world_id='{self.world}'")
        summary = runner.alert_summary(self.conn, self.world)
        self.assertTrue(summary['runnerDown'])
        self.assertEqual(summary['needsYou'], 2)
        self.assertIn(first, [r['run'] for r in runner.overview(self.conn, self.world)['runs']])

    # ---- every problem of a draft, with the world's
    def test_reference_problems_name_every_unknown_with_where_it_is(self):
        p = copy.deepcopy(self.examples['winter-ferries'])
        self.assertEqual([x for x in runner.reference_problems(self.conn, self.world, p) if x['blocks']], [], 'approval passes')
        coil = p['triggers'][1]
        coil['match']['target'] = 'rm999'
        coil['match']['actor'] = 'player-nobody'
        p['phases'][2]['effects'].append({'kind': 'npc.move', 'npc': 'rm404', 'cell': 'nowhere', 'x': 1, 'y': 1})
        p['phases'][1]['effects'].append({'kind': 'times.nudge', 'market': 'atlantis', 'points': 5, 'days': 3, 'words': 'news'})
        found = runner.reference_problems(self.conn, self.world, p)
        texts = {(x['path'], x['text'], x['blocks']) for x in found}
        self.assertIn(('triggers[1]', 'Unknown mission character in this world: rm999', True), texts)
        self.assertIn(('triggers[1]', 'No wolf or resident "player-nobody" in this world: this never matches.', False), texts)
        moved = [x for x in found if x['path'].startswith('phases[2].effects[')]
        self.assertEqual({x['text'] for x in moved}, {'Unknown npc in this world: rm404', 'Unknown cell in this world: nowhere',
                                                     'Tile (1,1) lies outside nowhere.'})
        self.assertEqual(moved[0]['at']['phaseId'], 'deep-freeze')
        with self.assertRaisesRegex(ValueError, 'Unknown'):
            runner.references(self.conn, self.world, p)
        out = runner.program_problems(self.conn, self.world, p)
        self.assertEqual(out['errors'], 0)
        self.assertTrue(out['world'])
        p['phases'][0]['journal'] = ''
        out = runner.program_problems(self.conn, self.world, p)
        self.assertEqual((out['errors'], out['world']), (1, False), 'the world is asked once the draft has no error')



@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class ExplainRoute(unittest.TestCase):
    """The ops `explain`, `problems`, `alerts` and `alert-rules` through POST /api/story-program: a DM and a viewer in the
    user's own studio; on a hosted world, the owner and a DM for the alert rules."""

    setUpClass = classmethod(lambda cls: _route_setup(cls))
    tearDownClass = classmethod(lambda cls: _route_teardown(cls))

    def call(self, body, token='', session=None, port=None):
        headers = {'Content-Type': 'application/json', **({'Authorization': 'Bearer ' + token} if token else {}),
                   **({'X-Session': session} if session else {})}
        request = urllib.request.Request(f'http://127.0.0.1:{port or self.port}/api/story-program', json.dumps({'target': 'dev', **body}).encode(),
                                         method='POST', headers=headers)
        try:
            with urllib.request.urlopen(request, timeout=20) as reply:
                return reply.status, json.loads(reply.read())
        except urllib.error.HTTPError as error:
            return error.code, json.loads(error.read())

    def token(self, user, password):
        request = urllib.request.Request(f'http://127.0.0.1:{self.port}/api/login', json.dumps({'username': user, 'password': password}).encode(),
                                         method='POST', headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(request, timeout=10) as reply:
            return json.loads(reply.read())['token']

    def test_explain_problems_alerts_and_the_alert_rules(self):
        dm, viewer = self.token('camp-dm', 'a long dm password'), self.token('camp-viewer', 'a long viewer password')
        status, out = self.call({'op': 'explain', 'run': self.run_id}, dm)
        self.assertEqual(status, 200, out)
        self.assertTrue(out['listeners'])
        status, out = self.call({'op': 'explain', 'run': self.run_id, 'command': {'kind': 'hold', 'payload': {}}}, dm)
        self.assertEqual((status, out['outcome']), (200, 'done'), out)
        status, out = self.call({'op': 'status', 'run': self.run_id}, dm)
        self.assertEqual(out['status'], 'running', 'the hold was a dry run')
        self.assertEqual(self.call({'op': 'explain', 'run': self.run_id}, viewer)[0], 403)
        status, out = self.call({'op': 'alerts'}, dm)
        self.assertEqual((status, out['live']), (200, 1))
        self.assertEqual(self.call({'op': 'alerts'}, viewer)[0], 403)
        wagon = runner.catalogue()['examples']['wagon']
        status, out = self.call({'op': 'problems', 'program': wagon}, viewer)
        self.assertEqual((status, out['errors'], out['world']), (200, 0, False), 'a viewer: the program only')
        status, out = self.call({'op': 'problems', 'program': wagon}, dm)
        self.assertEqual((status, out['world']), (200, True), out)
        status, out = self.call({'op': 'problems', 'program': {'format': 'nonsense'}}, viewer)
        self.assertEqual(status, 200)
        self.assertGreater(out['errors'], 0)
        status, out = self.call({'op': 'alert-rules', 'rules': {'quietHours': 12}}, dm)
        self.assertEqual((status, out['rules']['quietHours']), (200, 12), out)
        self.assertEqual(self.call({'op': 'alert-rules', 'rules': {'quietHours': 12}}, viewer)[0], 403)
        # On a hosted world, the alert rules are the owner's.
        status, out = self.call({'op': 'alert-rules', 'rules': {'quietHours': 6}}, session='dm', port=self.hosted_port)
        self.assertEqual(status, 403, out)
        status, out = self.call({'op': 'alert-rules', 'rules': {'quietHours': 6}}, session='owner', port=self.hosted_port)
        self.assertEqual((status, out['rules']['quietHours']), (200, 6), out)
        status, out = self.call({'op': 'explain', 'run': self.run_id}, session='dm', port=self.hosted_port)
        self.assertEqual(status, 200, 'any DM sees the console')


def _route_setup(cls):
    console.ConsoleReads.setUpClass.__func__(cls)
    with console.admin(cls.name) as owner:
        owner.execute('SET ROLE ratw_dm')
        D.create_account(owner, 'camp-dm', 'dm', 'a long dm password')
        D.create_account(owner, 'camp-viewer', 'viewer', 'a long viewer password')
        for wolf in WOLVES:
            owner.execute('SET ROLE ratw_game')
            owner.execute('SELECT game.record_events(%s,%s::jsonb)', (cls.world, json.dumps([{'kind': 'arrival', 'actor': wolf, 'day': 10}])))
        owner.execute('SET ROLE ratw_dm')
        p = runner.catalogue()['examples']['winter-ferries']
        ticket = runner.approve(owner, cls.world, 'martin', p, rules.digest(p))['ticket']
        cls.run_id = runner.publish(owner, cls.world, ticket)['run']
        owner.execute('SET ROLE ratw_game')
        owner.execute("UPDATE dm.actions SET status='applied',result='ok',done_at=now() WHERE status='queued'")
        owner.execute('SET ROLE ratw_dm')
    dm = D.DungeonMaster(connect=lambda target: W.connect('dev', 'dm', dbname=cls.name), worlds=lambda: ['prod', 'dev'])
    cls.servers = []

    def serve(handler):
        server = ThreadingHTTPServer(('127.0.0.1', 0), handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        cls.servers.append(server)
        return server.server_port

    cls.port = serve(D.make_handler(dm, worlds=lambda: ['dev'], default=lambda: 'dev'))
    hosted = D.make_handler(dm, worlds=lambda: ['dev'], default=lambda: 'dev', hosted=True)
    sessions = {'owner': {'username': 'pell#a1', 'role': 'owner', 'mayActOnWolves': True}, 'dm': {'username': 'wren#b2', 'role': 'dm'}}

    class Hosted(hosted):
        def trusted(self):
            return True

        def handle_api(self, method):
            self.editor_session = sessions[self.headers.get('X-Session')]
            return super().handle_api(method)

    cls.hosted_port = serve(Hosted)


def _route_teardown(cls):
    for server in cls.servers:
        server.shutdown()
        server.server_close()
    console.ConsoleReads.tearDownClass.__func__(cls)


if __name__ == '__main__':
    unittest.main()
