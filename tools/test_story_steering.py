"""Steering a running campaign (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 2, "Steer it"): every command
of the inbox on runs of the bundled examples in a throwaway, migrated database (hold, resume, hold all, stop, signal,
fire, enable and disable, enter, set, timers, missions, orders, tell, scene), with the rules every command keeps: made
for an earlier visit, made against another version, sent twice, arriving with world events waiting, waiting behind the
game; the audit rows, the timeline's fourth source and the values before and after; the runner's heartbeat; and the
route's permissions (a DM in the user's own studio, a viewer, a hosted DM with and without "may steer campaigns", the
owner, and "may act on players' wolves"). The game is stood in for by answering dm.actions rows by hand. Skipped
without the local PostgreSQL."""

import json
import secrets
import threading
import unittest
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer
from unittest.mock import patch

import dungeon_master as D
import story_native
import story_program as rules
import story_runner as runner
import story_triggers
import world_db as W
import test_story_console as console
from test_story_console import WOLVES, admin
from test_world_db import database_available

ASH, BIRCH, ELM = sorted(WOLVES)


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class Steering(unittest.TestCase):
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    setUp = console.ConsoleReads.setUp
    fact, launch, answer, tick, rewrite, row = (console.ConsoleReads.fact, console.ConsoleReads.launch, console.ConsoleReads.answer, console.ConsoleReads.tick,
                                                console.ConsoleReads.rewrite, console.ConsoleReads.row)

    # ---- helpers
    def one(self):
        return runner.status(self.conn, self.world, self.run_id)

    def send(self, kind, payload=None, reason='the DM is testing this', key=None, visit=None, digest=None, may=None, who='martin'):
        one = self.one()
        return runner.command(self.conn, self.world, who, {
            'run': self.run_id, 'kind': kind, 'payload': payload or {}, 'reason': reason, 'key': key or secrets.token_hex(8),
            'visit': one['visit'] if visit is None else visit, 'digest': one['digest'] if digest is None else digest}, may)

    def taken(self, sent):
        row = self.conn.execute('SELECT status,result FROM dm.story_commands WHERE id=%s', (sent['id'],)).fetchone()
        return row[0], row[1]

    def steer(self, kind, payload=None, **kw):
        """Send, tick, and the command's status and result."""
        sent = self.send(kind, payload, **kw)
        self.tick()
        return self.taken(sent)

    def answer_all(self):
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute("UPDATE dm.actions SET status='applied',result='ok',done_at=now() WHERE status='queued'")
        self.conn.execute('SET LOCAL ROLE ratw_dm')

    def settle(self, rounds=6):
        for _ in range(rounds):
            self.answer_all()
            self.tick()

    def ferries_in_watch(self):
        """winter-ferries, published, its journal answered, a wolf arrived: in ferry-watch with its three timers."""
        self.launch('winter-ferries')
        self.answer()
        self.fact('arrival', ASH)
        self.tick()
        self.settle()
        self.assertEqual(self.one()['phase'], 'ferry-watch')

    def state(self):
        return self.one()['state']

    def counters(self):
        return self.state().get('counters', {}).get('story', {})

    def queued_orders(self, kind):
        return self.conn.execute("SELECT id,target_id,payload FROM dm.actions WHERE kind=%s ORDER BY id", (kind,)).fetchall()

    # ---- hold, resume, hold all, stop
    def test_hold_reads_nothing_and_resume_counts_or_skips_what_happened_meanwhile(self):
        self.ferries_in_watch()
        self.assertEqual(self.steer('hold')[0], 'done')
        self.assertEqual(self.one()['status'], 'held')
        cursor = self.one()['cursor']
        self.fact('gift', BIRCH, 'rm111', item='rope', quantity=3)
        self.tick()
        self.assertEqual(self.one()['cursor'], cursor, 'a held run reads no events')
        self.assertNotIn('stores', self.counters())
        row = self.row()
        self.assertEqual(row['waiting'][0]['on'], 'admin')
        self.assertEqual(row['waiting'][0]['held']['by'], 'martin')
        self.assertEqual([a['kind'] for a in row['alerts']], ['held'])
        self.assertEqual(row['alerts'][0]['events'], 1, 'one world event waits')
        status, result = self.steer('resume')
        self.assertEqual(status, 'done')
        self.tick()
        self.assertEqual(self.counters()['stores'], 3, 'what happened while held counts')
        # Held again; resumed "from now": the gift meanwhile is skipped.
        self.steer('hold')
        self.fact('gift', BIRCH, 'rm111', item='rope', quantity=2)
        status, result = self.steer('resume', {'catchUp': False})
        self.assertEqual((status, result['after']['skipped']), ('done', 1))
        self.tick()
        self.assertEqual(self.counters()['stores'], 3)
        self.assertEqual(self.steer('resume')[0], 'refused', 'only a held story is resumed')

    def test_hold_all_holds_every_running_story_with_one_reason(self):
        self.ferries_in_watch()
        first = self.run_id
        self.launch('river-relief')
        out = runner.hold_all(self.conn, self.world, 'martin', {'reason': 'the server restarts soon', 'key': 'hold-all-1'})
        self.assertEqual(len(out['commands']), 2)
        self.tick()
        for run in (first, self.run_id):
            self.assertEqual(runner.status(self.conn, self.world, run)['status'], 'held')
        audit = self.conn.execute("SELECT count(*) FROM dm.audit WHERE action='story.hold' AND detail LIKE '%%server restarts%%'").fetchone()[0]
        self.assertEqual(audit, 2, 'one audit row each')
        again = runner.hold_all(self.conn, self.world, 'martin', {'reason': 'the server restarts soon', 'key': 'hold-all-1'})
        self.assertEqual(again['commands'], [], 'nothing is running now')

    def test_stop_waits_for_the_game_to_acknowledge(self):
        self.ferries_in_watch()
        status, result = self.steer('stop')
        self.assertEqual((status, self.one()['status']), ('done', 'stopping'))
        stop = self.queued_orders('story.stop')[-1][0]
        self.assertEqual(result['after']['order'], stop)
        self.assertEqual(self.row()['waiting'][0]['order']['id'], stop)
        self.tick()
        self.assertEqual(self.one()['status'], 'stopping', 'not stopped until the game says so')
        self.answer_all()
        self.tick()
        self.assertEqual(self.one()['status'], 'stopped')
        self.assertIn(stop, [o['id'] for o in self.one()['orders']], 'the stop order is one of the run\'s')

    # ---- signal, fire, enable and disable
    def test_a_signal_fires_its_triggers_and_moves_the_story_with_values_before_and_after(self):
        self.ferries_in_watch()
        status, result = self.steer('signal', {'name': 'watch-round'})
        self.assertEqual(status, 'done', result)
        self.assertEqual(result['after']['fired'], ['log-watch-round'])
        self.assertEqual(self.counters()['watches'], 1)
        entry = self.conn.execute('SELECT entry FROM dm.story_trigger_history WHERE run=%s ORDER BY id DESC LIMIT 1', (self.run_id,)).fetchone()[0]
        self.assertEqual((entry['trigger'], entry['by']), ('log-watch-round', 'martin'))
        self.assertEqual(entry['changes'], [{'what': 'counter', 'scope': 'story', 'key': 'watches', 'before': 0, 'after': 1}])
        status, result = self.steer('signal', {'name': 'first-freeze'})
        self.assertEqual((status, result['after']['phase']), ('done', 'deep-freeze'))
        cause = self.conn.execute('SELECT cause FROM dm.story_transitions WHERE run=%s ORDER BY visit DESC LIMIT 1', (self.run_id,)).fetchone()[0]
        self.assertEqual(cause, 'admin martin: the DM is testing this')
        with self.assertRaisesRegex(ValueError, 'listens for that signal'):
            self.send('signal', {'name': 'no-such-signal'})

    def test_firings_by_the_world_keep_their_values_before_and_after(self):
        self.ferries_in_watch()
        self.fact('gift', BIRCH, 'rm111', item='rope', quantity=2)
        self.tick()
        entry = self.conn.execute('SELECT entry FROM dm.story_trigger_history WHERE run=%s ORDER BY id DESC LIMIT 1', (self.run_id,)).fetchone()[0]
        self.assertEqual(entry['trigger'], 'offer-a-coil')
        self.assertEqual(entry['changes'], [{'what': 'counter', 'scope': 'story', 'key': 'stores', 'before': 0, 'after': 2}])

    def test_fire_now_checks_conditions_unless_told_not_to(self):
        self.ferries_in_watch()
        with self.assertRaisesRegex(ValueError, 'no such trigger'):
            self.send('fire', {'trigger': 'made-up'})
        status, result = self.steer('fire', {'trigger': 'stocked-at-thaw'})
        self.assertEqual(status, 'refused')
        self.assertIn('conditions are false', result['text'])
        status, result = self.steer('fire', {'trigger': 'offer-a-coil'})
        self.assertEqual(status, 'refused', 'a participant trigger needs a wolf')
        status, result = self.steer('fire', {'trigger': 'offer-a-coil', 'actor': BIRCH})
        self.assertEqual(status, 'done', result)
        self.assertEqual(self.counters().get('stores', 0), 0, 'the stand-in event has no quantity')
        status, result = self.steer('fire', {'trigger': 'stocked-at-thaw', 'check': False})
        self.assertEqual((status, result['after']['phase']), ('done', 'ferries-reopen'))
        entry = self.conn.execute("SELECT entry FROM dm.story_trigger_history WHERE run=%s AND entry->>'trigger'='stocked-at-thaw'",
                                  (self.run_id,)).fetchone()[0]
        self.assertEqual((entry['byHand'], entry['checked']), (True, False))
        self.settle()
        self.assertTrue(self.queued_orders('times.nudge'), 'the phase\'s approved entry effect was sent')

    def test_a_trigger_turned_off_does_not_fire_in_either_engine(self):
        self.ferries_in_watch()
        self.assertEqual(self.steer('disable', {'trigger': 'log-watch-round'})[0], 'done')
        self.assertEqual(self.state()['disabled'], ['log-watch-round'], 'a campaign trigger by its id')
        controls = self.one()['controls']
        self.assertTrue(next(t for t in controls['triggers'] if t['id'] == 'log-watch-round')['off'])
        self.assertEqual(self.steer('signal', {'name': 'watch-round'})[1]['after']['fired'], [])
        self.assertNotIn('watches', self.counters())
        with patch('story_native.interpreter', return_value=None):
            self.assertEqual(self.steer('signal', {'name': 'watch-round'})[1]['after']['fired'], [], 'the Python twin too')
        self.assertEqual(self.steer('fire', {'trigger': 'log-watch-round'})[0], 'refused', 'turned off: only unchecked')
        status, result = self.steer('enable', {'trigger': 'log-watch-round'})
        self.assertEqual((status, result['before'], result['after']), ('done', {'on': False}, {'on': True}))
        self.assertNotIn('disabled', self.state())
        self.assertEqual(self.steer('signal', {'name': 'watch-round'})[1]['after']['fired'], ['log-watch-round'])
        # A phase's own trigger is named phase/id, and stays off on a later visit of that phase.
        self.steer('disable', {'trigger': 'begin-deep-freeze'})
        self.assertEqual(self.state()['disabled'], ['ferry-watch/begin-deep-freeze'])
        self.assertEqual(self.steer('signal', {'name': 'first-freeze'})[1]['after']['phase'], 'ferry-watch')

    def test_disabled_rules_match_in_the_native_engine_and_the_python_twin(self):
        if not story_native.interpreter():
            self.skipTest('Build ratw_story first.')
        program = self.examples['winter-ferries']
        phase = rules.phase_of(program, {'phase': 'ferry-watch'})
        for disabled in ([], ['log-watch-round'], ['ferry-watch/begin-deep-freeze'], ['log-watch-round', 'ferry-watch/begin-deep-freeze']):
            for signal in ('watch-round', 'first-freeze'):
                outcomes = []
                for native in (True, False):
                    state = {'phase': 'ferry-watch', 'entered': 0, 'cohort': [ASH], 'votes': {}, 'visits': 2, 'actor': '',
                             'disabled': disabled}
                    trace = []
                    with patch('story_native.interpreter', return_value=story_native.interpreter() if native else None):
                        moved = story_triggers.fire(phase, state, {'kind': story_triggers.SIGNAL, 'signal': signal, 'actor': ''}, 1, 0,
                                                    lambda: set(), rules.matches, lambda *a: None, trace)
                    outcomes.append((moved, state.get('counters'), [t['trigger'] for t in trace]))
                with self.subTest(disabled=disabled, signal=signal):
                    self.assertEqual(outcomes[0], outcomes[1])

    # ---- enter
    def test_enter_a_phase_with_or_without_its_entry_effects(self):
        self.ferries_in_watch()
        status, result = self.steer('enter', {'phase': 'deep-freeze', 'effects': False})
        self.assertEqual((status, result['after']['way']), ('done', 'on'))
        self.settle()
        self.assertEqual(self.queued_orders('band.override'), [], 'without its entry effects')
        self.assertEqual(self.one()['phase'], 'deep-freeze')
        status, result = self.steer('enter', {'phase': 'ferry-watch'})
        self.assertEqual((status, result['after']['way']), ('done', 'back'))
        self.assertIn('Nothing the world was given is taken back', result['text'])
        self.settle()
        self.steer('enter', {'phase': 'deep-freeze'})
        self.settle()
        self.assertEqual(len(self.queued_orders('band.override')), 1, 'with them')
        self.assertEqual(self.one()['visit'], 5)
        self.assertIn('watch-round', self.state()['timers'], 'campaign timers live on')

    # ---- set, timers
    def test_set_a_counter_and_its_bounds(self):
        self.ferries_in_watch()
        status, result = self.steer('set', {'key': 'stores', 'scope': 'story', 'value': 7})
        self.assertEqual((status, result['before'], result['after']['value']), ('done', {'value': 0}, 7))
        self.assertEqual(self.counters()['stores'], 7)
        with self.assertRaisesRegex(ValueError, 'names no such counter'):
            self.send('set', {'key': 'stors', 'scope': 'story', 'value': 1})
        with self.assertRaisesRegex(ValueError, 'whole number'):
            self.send('set', {'key': 'stores', 'scope': 'story', 'value': 2.5})
        # Now the thaw goes the stocked way.
        self.assertEqual(self.steer('fire', {'trigger': 'stocked-at-thaw'})[1]['after']['phase'], 'ferries-reopen')

    def test_timers_fire_now_shift_cancel_and_start(self):
        self.ferries_in_watch()
        timers = self.state()['timers']
        at = timers['watch-round']['at']
        status, result = self.steer('timer', {'do': 'shift', 'name': 'watch-round', 'days': 2})
        self.assertEqual((status, result['before']['at'], result['after']['at']), ('done', at, at + 2))
        status, result = self.steer('timer', {'do': 'fire', 'name': 'watch-round'})
        self.assertEqual((status, result['after']['fired']), ('done', ['log-watch-round']))
        self.assertEqual(self.counters()['watches'], 1)
        self.assertIn('watch-round', self.state()['timers'], 'a repeating timer goes round again')
        self.assertEqual(self.steer('timer', {'do': 'cancel', 'name': 'spring-council'})[0], 'done')
        self.assertNotIn('spring-council', self.state()['timers'])
        status, result = self.steer('timer', {'do': 'start', 'signal': 'spring-council', 'days': 3})
        self.assertEqual(status, 'done', result)
        self.assertEqual(self.state()['timers']['spring-council']['scope'], 'campaign', 'as the program would start it')
        self.assertEqual(self.steer('timer', {'do': 'cancel', 'name': 'nothing'})[0], 'refused')
        with self.assertRaisesRegex(ValueError, 'never starts a timer'):
            self.send('timer', {'do': 'start', 'signal': 'made-up'})
        status, result = self.steer('timer', {'do': 'fire', 'name': 'first-freeze'})
        self.assertEqual(result['after']['phase'], 'deep-freeze')

    # ---- missions
    def test_complete_and_reset_a_mission_for_a_wolf(self):
        self.launch('river-relief')
        self.settle()
        status, result = self.steer('mission', {'mission': 'passengers', 'do': 'complete', 'player': ASH})
        self.assertEqual((status, result['before']['step'], result['after']['step']), ('done', 0, 2), result)
        passengers = next(m for m in self.one()['missionsNow'] if m['id'] == 'passengers')
        self.assertEqual(passengers['tracks'][ASH], [2, 0])
        journal = self.queued_orders('story.missions')[-1]
        self.assertEqual(journal[2]['missions'][0]['tracks'][ASH], [2, 0], 'the journal is told')
        self.settle()
        self.assertEqual(self.steer('mission', {'mission': 'passengers', 'do': 'complete'})[0], 'refused', 'each wolf\'s mission needs a wolf')
        status, result = self.steer('mission', {'mission': 'passengers', 'do': 'reset', 'player': ASH})
        self.assertEqual((status, result['after']['step']), ('done', 0))
        self.settle()
        # Completing every gate by hand moves it on at the next thing a wolf of the story does.
        for wolf in WOLVES:
            self.steer('mission', {'mission': 'passengers', 'do': 'complete', 'player': wolf})
            self.settle()
        self.steer('mission', {'mission': 'dispatch', 'do': 'complete'})
        self.settle()
        self.fact('arrival', ELM)
        self.tick()
        self.settle()
        self.assertEqual(self.one()['phase'], 'rigging')

    # ---- orders
    def test_a_refused_order_is_retried_or_skipped(self):
        self.ferries_in_watch()
        self.steer('enter', {'phase': 'deep-freeze'})
        self.answer()          # the journal
        self.tick()
        self.answer('refused', 'no such household')   # band.override
        self.tick()
        self.assertEqual(self.one()['status'], 'paused')
        stuck = self.row()['waiting'][0]['order']['id']
        self.assertEqual(self.steer('order', {'receipt': stuck + 999, 'do': 'skip'})[0], 'refused', 'not the stuck one')
        status, result = self.steer('order', {'receipt': stuck, 'do': 'retry'})
        self.assertEqual(status, 'done')
        again = result['after']['order']
        self.assertEqual(self.one()['status'], 'running')
        self.answer('refused', 'still no such household')
        self.tick()
        self.assertEqual(self.one()['status'], 'paused')
        status, result = self.steer('order', {'receipt': again, 'do': 'skip'})
        self.assertEqual((status, self.one()['status']), ('done', 'running'))
        self.tick()
        self.assertEqual(self.one()['status'], 'running', 'it carries on without it')
        self.assertEqual(self.one()['alerts'], [])

    # ---- tell, scene
    def test_tell_the_wolves_and_a_scene_that_does_not_exist(self):
        self.ferries_in_watch()
        status, result = self.steer('tell', {'to': 'participants', 'text': 'The river has closed.'})
        self.assertEqual(status, 'done')
        receipt, target, payload = self.queued_orders('story.tell')[-1]
        self.assertEqual((target, payload['text'], payload['to'], payload['visit']), (self.run_id, 'The river has closed.', 'participants', 2))
        self.steer('tell', {'to': 'wolf', 'player': BIRCH, 'text': 'Bring rope.'})
        self.assertEqual(self.queued_orders('story.tell')[-1][2]['player'], BIRCH)
        self.assertEqual(self.steer('tell', {'to': 'wolf', 'player': 'player-nobody', 'text': 'Hello.'})[0], 'refused')
        self.assertIn(receipt, [o['id'] for o in self.one()['orders']])
        self.assertEqual(self.one()['status'], 'running', 'a message never holds the story up')
        with self.assertRaisesRegex(ValueError, 'One line'):
            self.send('tell', {'to': 'participants', 'text': 'two\nlines'})
        with self.assertRaisesRegex(ValueError, 'no such scene'):
            self.send('scene', {'scene': 'none', 'player': 'world'})

    # ---- the rules every command keeps
    def test_made_for_an_earlier_visit_against_another_version_or_sent_twice(self):
        self.ferries_in_watch()
        stale = self.send('set', {'key': 'stores', 'scope': 'story', 'value': 1}, visit=1)
        forced = self.send('set', {'key': 'stores', 'scope': 'story', 'value': 4, 'force': True}, visit=1)
        self.tick()
        self.assertEqual(self.taken(stale)[0], 'refused')
        self.assertIn('moved on to visit 2', self.taken(stale)[1]['text'])
        self.assertEqual((self.taken(forced)[0], self.counters()['stores']), ('done', 4))
        with self.assertRaisesRegex(ValueError, 'another version'):
            self.send('hold', digest='0' * 64)
        first = self.send('signal', {'name': 'watch-round'}, key='same-key-1')
        second = self.send('signal', {'name': 'watch-round'}, key='same-key-1')
        self.assertEqual((first['id'], second.get('again')), (second['id'], True))
        self.tick()
        self.tick()
        self.assertEqual(self.counters()['watches'], 1, 'sent twice, done once')
        with self.assertRaisesRegex(ValueError, 'Say why'):
            self.send('hold', reason='ok')

    def test_a_command_with_world_events_waiting_lands_between_them(self):
        self.ferries_in_watch()
        self.fact('gift', BIRCH, 'rm111', item='rope', quantity=2)
        sent = self.send('set', {'key': 'stores', 'scope': 'story', 'value': 10})
        cursor = self.one()['cursor']
        self.tick()
        status, result = self.taken(sent)
        self.assertEqual((status, result['atEvent']), ('done', cursor), 'taken before the waiting gift (known problem 2 is gone)')
        self.assertEqual(self.counters()['stores'], 12, 'then the gift is read on top')

    def test_a_flow_command_waits_for_the_game_and_hold_goes_past_it(self):
        self.ferries_in_watch()
        self.steer('fire', {'trigger': 'begin-deep-freeze'})        # the journal and band.override still to go
        fire = self.send('signal', {'name': 'watch-round'})
        hold = self.send('hold')
        self.tick()
        self.assertEqual(self.taken(fire)[0], 'queued', 'waits while the visit\'s orders are being sent')
        self.assertEqual(self.taken(hold)[0], 'done', 'hold goes past it')
        self.assertEqual(self.taken(fire)[0], 'queued')
        self.tick()
        self.assertEqual(self.taken(fire)[0], 'refused', 'held part-way through sending: resume first')
        self.steer('resume')
        late = self.send('signal', {'name': 'watch-round'})
        self.rewrite(f"UPDATE dm.story_commands SET requested_at=now()-interval '2 minutes' WHERE id={late['id']}")
        self.tick()
        self.assertIn('did not answer', self.taken(late)[1]['text'])
        self.settle()
        self.assertEqual(self.steer('signal', {'name': 'watch-round'})[0], 'done', 'once the orders are answered')

    # ---- audit, timeline, heartbeat
    def test_every_command_is_audited_and_on_the_timeline(self):
        self.ferries_in_watch()
        self.steer('set', {'key': 'stores', 'scope': 'story', 'value': 5}, reason='two coils were handed over unseen')
        self.steer('timer', {'do': 'cancel', 'name': 'nothing-here'})
        audit = self.conn.execute("SELECT action,target,detail,who FROM dm.audit WHERE target=%s ORDER BY sequence", (self.run_id,)).fetchall()
        self.assertEqual([a[0] for a in audit], ['story.set', 'story.timer'])
        self.assertIn('stores set 0 → 5', audit[0][2])
        self.assertIn('two coils were handed over unseen', audit[0][2])
        self.assertIn('refused', audit[1][2])
        self.assertEqual(audit[0][3], 'martin')
        page = runner.timeline(self.conn, self.world, self.run_id)
        commands = [i for i in page['items'] if i['type'] == 'command']
        self.assertEqual([(c['command'], c['status']) for c in commands], [('timer', 'refused'), ('set', 'done')])
        self.assertEqual(commands[1]['key'], f'c:{commands[1]["id"]}')
        self.assertEqual(page['items'][0]['type'], 'command', 'a command ranks after the orders of its moment')
        self.assertEqual(page['since']['command'], commands[0]['id'])
        quiet = runner.timeline(self.conn, self.world, self.run_id, since=page['since'])
        self.assertEqual([i['key'] for i in quiet['items'] if i['type'] == 'command' and i['key'] not in {c['key'] for c in commands}], [])
        only = runner.timeline(self.conn, self.world, self.run_id, kinds=['command'])
        self.assertEqual(len(only['items']), 2)
        self.assertEqual([c['kind'] for c in self.one()['commands']], ['timer', 'set'])

    def test_the_heartbeat_and_the_runner_down_alert(self):
        self.ferries_in_watch()
        beat = runner.status(self.conn, self.world)['runner']
        self.assertEqual(beat['version'], runner.RUNNER_VERSION)
        self.assertLess(beat['age'], 5)
        self.assertEqual(self.row()['alerts'], [])
        self.rewrite("UPDATE dm.story_runner SET beat_at=now()-interval '2 minutes'")
        self.assertEqual([a['kind'] for a in self.row()['alerts']], ['runner'])
        self.assertTrue(self.row()['alerts'][0]['needsYou'])


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class SteeringRoute(unittest.TestCase):
    """The `command` and `hold-all` ops through POST /api/story-program: in the user's own studio (a DM may steer, a viewer
    may not), and on a hosted world (the owner; a DM only with "may steer campaigns"; "may act on wolves")."""

    @classmethod
    def setUpClass(cls):
        console.ConsoleReads.setUpClass.__func__(cls)
        with admin(cls.name) as owner:
            owner.execute('SET ROLE ratw_dm')
            D.create_account(owner, 'camp-dm', 'dm', 'a long dm password')
            D.create_account(owner, 'camp-viewer', 'viewer', 'a long viewer password')
            for wolf in WOLVES:
                owner.execute('SET ROLE ratw_game')
                owner.execute('SELECT game.record_events(%s,%s::jsonb)', (cls.world, json.dumps([{'kind': 'arrival', 'actor': wolf, 'day': 10}])))
            owner.execute('SET ROLE ratw_dm')
            cls.runs = {}
            for example in ('winter-ferries', 'river-relief'):
                p = runner.catalogue()['examples'][example]
                ticket = runner.approve(owner, cls.world, 'martin', p, rules.digest(p))['ticket']
                cls.runs[example] = runner.publish(owner, cls.world, ticket)['run']
        dm = D.DungeonMaster(connect=lambda target: W.connect('dev', 'dm', dbname=cls.name), worlds=lambda: ['prod', 'dev'])
        cls.servers = []
        cls.port = cls.serve(D.make_handler(dm, worlds=lambda: ['dev'], default=lambda: 'dev'))
        hosted = D.make_handler(dm, worlds=lambda: ['dev'], default=lambda: 'dev', hosted=True)
        sessions = {
            'owner': {'username': 'pell#a1', 'role': 'owner', 'mayActOnWolves': True},
            'dm': {'username': 'wren#b2', 'role': 'dm'},
            'steerer': {'username': 'tam#c3', 'role': 'dm', 'maySteerCampaigns': True},
            'steerer-wolves': {'username': 'tam#c4', 'role': 'builder', 'maySteerCampaigns': True, 'mayActOnWolves': True},
        }

        class Hosted(hosted):
            def trusted(self):
                return True

            def handle_api(self, method):
                self.editor_session = sessions[self.headers.get('X-Session')]
                return super().handle_api(method)

        cls.hosted_port = cls.serve(Hosted)

    @classmethod
    def serve(cls, handler):
        server = ThreadingHTTPServer(('127.0.0.1', 0), handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        cls.servers.append(server)
        return server.server_port

    @classmethod
    def tearDownClass(cls):
        for server in cls.servers:
            server.shutdown()
            server.server_close()
        console.ConsoleReads.tearDownClass.__func__(cls)

    def call(self, body, token='', session=None, port=None):
        headers = {'Content-Type': 'application/json', **({'Authorization': 'Bearer ' + token} if token else {}),
                   **({'X-Session': session} if session else {})}
        request = urllib.request.Request(f'http://127.0.0.1:{port or self.port}/api/story-program', json.dumps(body).encode(),
                                         method='POST', headers=headers)
        try:
            with urllib.request.urlopen(request, timeout=10) as reply:
                return reply.status, json.loads(reply.read())
        except urllib.error.HTTPError as error:
            return error.code, json.loads(error.read())

    def token(self, user, password):
        request = urllib.request.Request(f'http://127.0.0.1:{self.port}/api/login', json.dumps({'username': user, 'password': password}).encode(),
                                         method='POST', headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(request, timeout=10) as reply:
            return json.loads(reply.read())['token']

    def body(self, example='winter-ferries', kind='set', payload=None, **extra):
        with W.connect('dev', 'dm', dbname=self.name) as conn:
            visit, digest = conn.execute("SELECT (state->>'visits')::int,digest FROM dm.story_releases WHERE id=%s",
                                         (self.runs[example],)).fetchone()
        return {'op': 'command', 'target': 'dev', 'run': self.runs[example], 'kind': kind,
                'payload': payload if payload is not None else {'key': 'stores', 'scope': 'story', 'value': 3},
                'reason': 'checking who may steer', 'key': secrets.token_hex(8), 'visit': visit, 'digest': digest, **extra}

    def program_with(self, kind):
        p = json.loads(json.dumps(runner.catalogue()['examples']['winter-ferries']))
        p['phases'][2]['effects'].append({'kind': kind})
        return p

    def test_in_the_users_own_studio_a_dm_steers_and_a_viewer_does_not(self):
        dm = self.token('camp-dm', 'a long dm password')
        status, sent = self.call(self.body(), dm)
        self.assertEqual((status, sent['status']), (200, 'queued'), sent)
        self.assertEqual(sent['by'], 'camp-dm')
        viewer = self.token('camp-viewer', 'a long viewer password')
        status, refused = self.call(self.body(), viewer)
        self.assertEqual(status, 403, refused)
        status, bad = self.call(self.body(kind='launch'), dm)
        self.assertEqual(status, 422, bad)

    def test_on_a_hosted_world_the_owner_or_may_steer_campaigns(self):
        port = self.hosted_port
        status, data = self.call(self.body(), session='dm', port=port)
        self.assertEqual(status, 403, data)
        self.assertIn('may steer campaigns', data['error'])
        status, data = self.call(self.body(), session='steerer', port=port)
        self.assertEqual(status, 200, data)
        status, data = self.call(self.body(), session='owner', port=port)
        self.assertEqual(status, 200, data)
        status, data = self.call({'op': 'hold-all', 'target': 'dev', 'reason': 'everyone hold on', 'key': secrets.token_hex(8)},
                                 session='steerer', port=port)
        self.assertEqual(status, 403, 'hold all is the owner\'s')
        # A story that can hurt wolves (here: band.override stands in for character.injury) needs "may act on players'
        # wolves" too, to be moved on; holding it releases nothing.
        with patch.object(runner, 'WOLF_EFFECTS', frozenset({'band.override'})):
            status, data = self.call(self.body('winter-ferries', 'enter', {'phase': 'deep-freeze'}), session='steerer', port=port)
            self.assertEqual(status, 422, data)
            self.assertIn("may act on players", data['error'])
            status, data = self.call(self.body('winter-ferries', 'hold', {}), session='steerer', port=port)
            self.assertEqual(status, 200, data)
            status, data = self.call(self.body('winter-ferries', 'enter', {'phase': 'deep-freeze'}), session='steerer-wolves', port=port)
            self.assertEqual(status, 200, data)
        self.assertRaisesRegex(ValueError, 'faction relations', runner._may_release, self.program_with('faction.relation'), 'fire', {},
                               {'relations': False, 'wolves': True})
        runner._may_release(self.program_with('faction.relation'), 'tell', {}, {'relations': False, 'wolves': False})
        status, data = self.call({'op': 'hold-all', 'target': 'dev', 'reason': 'everyone hold on', 'key': secrets.token_hex(8)},
                                 session='owner', port=port)
        self.assertEqual(status, 200, data)


if __name__ == '__main__':
    unittest.main()
