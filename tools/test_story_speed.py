"""Doc 85, Phase 7 (batched effects and a faster runner): a tick's effects sent to the game together and answered one by
one; what an effect says to do when refused (pause, skip, wait for its wolf) and the console's ways on from a stuck batch;
the runner reading until it has caught up; a direction landing on a busy world; old history pruned by the rule; the
runner's loop woken by the world's events; and, with the real runner process killed part-way through (mid-read and with
a batch waiting for the game) and started again, every effect given once: none twice, none lost. A throwaway, migrated
database each; the game stood in for by answering dm.actions. Skipped without the local PostgreSQL."""

import json
import os
import secrets
import signal
import subprocess
import sys
import threading
import time
import unittest
from pathlib import Path

import psycopg

import story_program as rules
import story_runner as runner
import test_story_console as console
import test_story_steering as steering
import world_db as W
from test_world_db import database_available, superuser

HERE = Path(__file__).resolve().parent
ASH, BIRCH = 'player-ash', 'player-birch'


def story(effects_on_trade, refusal=None, reward=False, trades='economy'):
    """A story that answers each trade (`economy`) with effects, and a quest whose reward is a deed."""
    effect = {'kind': 'deed.award', 'player': ASH, 'detail': 'A trade.'}
    if refusal:
        effect['ifRefused'] = refusal
    return {
        'format': 'ratw-story-program', 'version': 1, 'id': 'trades-' + secrets.token_hex(3), 'title': 'The trades',
        'audience': {'kind': 'players', 'players': [ASH, BIRCH, 'player-elm']}, 'start': 'market',
        'phases': [{'id': 'market', 'journal': 'Every trade is answered.', 'effects': [], 'exits': [], 'triggers': [
            {'id': 'answer', 'on': trades, 'actions': [{'do': 'add', 'scope': 'story', 'key': 'trades', 'value': 1},
                                                      *({'do': 'effect', 'effect': dict(effect, detail=f'Trade {n}.')}
                                                        for n in range(effects_on_trade))]}]}],
        'tests': [{'name': 'A trade', 'cohort': [ASH, BIRCH, 'player-elm'], 'expect': ['market'], 'events': [
            {'id': 'a', 'kind': trades, 'day': 10, 'actor': 'x', 'target': 'y'}]}],
    }


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class Speed(unittest.TestCase):
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    fact, answer, tick, rewrite = console.ConsoleReads.fact, console.ConsoleReads.answer, console.ConsoleReads.tick, console.ConsoleReads.rewrite
    one, send, taken, steer = steering.Steering.one, steering.Steering.send, steering.Steering.taken, steering.Steering.steer

    def setUp(self):
        console.ConsoleReads.setUp(self)

    def publish(self, p):
        ticket = runner.approve(self.conn, self.world, 'martin', p, rules.digest(p))['ticket']
        self.run_id = runner.publish(self.conn, self.world, ticket)['run']
        self.answer()
        self.tick()
        return self.run_id

    def last(self):
        return self.conn.execute('SELECT id,kind,payload,status FROM dm.actions ORDER BY id DESC LIMIT 1').fetchone()

    def respond(self, order, results):
        """The game's answer to a batch: a result for each item."""
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute("UPDATE dm.actions SET status='applied',result=%s,done_at=now() WHERE id=%s", (json.dumps(results), order))
        self.conn.execute('SET LOCAL ROLE ratw_dm')

    def trades(self, n, **fields):
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute('SELECT game.record_events(%s,%s::jsonb)', (self.world, json.dumps(
            [{'kind': 'economy', 'actor': 'till', 'target': 'buyer', 'day': 10, 'item': 'bread', 'quantity': 1, **fields}] * n)))
        self.conn.execute('SET LOCAL ROLE ratw_dm')

    def notes(self):
        return [n for r in self.conn.execute('SELECT entry FROM dm.story_trigger_history WHERE run=%s ORDER BY id', (self.run_id,))
                for n in r[0].get('notes', [])]

    # ---- batches
    def test_a_ticks_effects_go_together_in_order_and_the_runner_reads_until_caught_up(self):
        self.publish(story(3))
        self.trades(400)                               # (More than three pages of the old 128, and 1,200 effects.)
        self.tick()
        state = self.one()['state']
        cursor = self.conn.execute('SELECT cursor FROM dm.story_releases WHERE id=%s', (self.run_id,)).fetchone()[0]
        newest = self.conn.execute('SELECT max(id) FROM game.events').fetchone()[0]
        self.assertLess(cursor, newest, 'it stops reading once a batch is full')
        order = self.last()
        self.assertEqual(order[1], 'story.effects')
        self.assertEqual(len(order[2]['items']), runner.EFFECTS_A_BATCH)
        self.assertEqual([i['payload']['detail'] for i in order[2]['items'][:4]], ['Trade 0.', 'Trade 1.', 'Trade 2.', 'Trade 0.'], 'in order')
        self.assertEqual(state['counters']['story']['trades'], 21, 'it read on while the effects gathered (20 trades and the 21st)')
        batches = 0
        for _ in range(40):
            self.answer()
            self.tick()
            batches += 1
            if self.conn.execute('SELECT cursor FROM dm.story_releases WHERE id=%s', (self.run_id,)).fetchone()[0] >= newest and \
                    not self.one()['state'].get('triggerEffects') and 'batch' not in self.one()['state']:
                break
        sent = sum(len(p['items']) if k == 'story.effects' else 1 for k, p in self.conn.execute(
            "SELECT kind,payload FROM dm.actions WHERE kind IN ('story.effects','deed.award')"))
        self.assertEqual(sent, 1200, 'every effect once')
        self.assertLessEqual(batches, 21, '1,200 effects in batches of 61, not 1,200 round trips')
        self.assertEqual(self.one()['state']['counters']['story']['trades'], 400)

    def test_a_refusal_that_pauses_holds_the_rest_and_the_console_moves_it_on(self):
        self.publish(story(3))
        self.trades(1)
        self.tick()
        order = self.last()
        self.respond(order[0], [{'ok': True, 'text': 'ok'}, {'ok': False, 'text': 'That wolf is not in the world.'},
                                {'ok': False, 'held': True, 'text': 'Not applied.'}])
        self.tick()
        one = self.one()
        self.assertEqual(one['status'], 'paused')
        self.assertIn('Trade 1', json.dumps(one['state']['batch']['items'][1]))
        view = next(o for o in one['orders'] if o['id'] == order[0])
        self.assertEqual([i['status'] for i in view['items']], ['applied', 'refused', 'held'])
        # The dry run, then retry: the stuck one and the one after it, again; not the one applied.
        self.assertEqual(self.steer('order', {'receipt': order[0], 'do': 'retry'})[0], 'done')
        again = self.last()
        self.assertEqual([i['payload']['detail'] for i in again[2]['items']], ['Trade 1.', 'Trade 2.'])
        self.respond(again[0], [{'ok': False, 'text': 'Still away.'}, {'ok': False, 'held': True, 'text': 'Not applied.'}])
        self.tick()
        self.assertEqual(self.one()['status'], 'paused')
        # Held for its wolf: the rest goes now; the refused one waits for Ash to come back.
        status, result = self.steer('order', {'receipt': again[0], 'do': 'when-here'})
        self.assertEqual(status, 'done', result)
        rest = self.last()
        self.assertEqual((rest[1], rest[2]['detail']), ('deed.award', 'Trade 2.'))
        self.answer()
        self.tick()
        one = self.one()
        self.assertEqual(one['status'], 'running')
        self.assertEqual([(h['wolf'], h['kind']) for h in one['whenHere']], [(ASH, 'deed.award')])
        self.fact('arrival', ASH)
        self.tick()
        back = self.last()
        self.assertEqual((back[1], back[2]['detail']), ('deed.award', 'Trade 1.'), 'sent again when Ash came into the world')
        self.answer()
        self.tick()
        self.assertEqual(self.one()['whenHere'], [])

    def test_skip_and_when_here_as_the_effect_says_and_a_reward_waits_by_default(self):
        self.publish(story(1, refusal='skip'))
        self.trades(1)
        self.tick()
        self.answer('refused', 'That wolf is not in the world.')
        self.tick()
        self.assertEqual(self.one()['status'], 'running', 'skip: noted, and the story carries on')
        self.assertTrue(any('skipped' in n for n in self.notes()), self.notes())
        # "when here": held, with its wait; past it, dropped and noted.
        p = story(1, refusal='when here')
        p['phases'][0]['triggers'][0]['actions'][1]['effect']['waitDays'] = 2
        self.publish(p)
        self.trades(1)
        self.tick()
        self.answer('refused', 'That wolf is not in the world.')
        self.tick()
        held = self.one()['whenHere']
        self.assertEqual(len(held), 1)
        self.assertAlmostEqual(held[0]['until'] - time.time(), 2 * 86400, delta=60)
        state = self.one()['state']
        state['whenHere'][0]['until'] = time.time() - 1
        self.conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), self.run_id))
        self.tick()
        self.assertEqual(self.one()['whenHere'], [])
        self.assertTrue(any('Stopped waiting' in n for n in self.notes()))
        # A quest's reward is "when here" unless it says.
        self.assertEqual(runner.refusal({'kind': 'deed.award', 'player': ASH, 'reward': 'q'}), 'when here')
        self.assertEqual(runner.refusal({'kind': 'deed.award', 'player': ASH}), 'pause')
        self.assertEqual(runner.refusal({'kind': 'deed.award', 'player': ASH, 'reward': 'q', 'ifRefused': 'skip'}), 'skip')

    def test_if_refused_is_checked(self):
        p = story(1, refusal='when here')
        rules.validate(p)
        bad = story(1, refusal='later')
        with self.assertRaisesRegex(ValueError, 'ifRefused'):
            rules.validate(bad)
        world = story(0)
        world['phases'][0]['triggers'][0]['actions'].append({'do': 'effect', 'effect': {
            'kind': 'festival.call', 'town': 'greyfen', 'name': 'Fair', 'inDays': 1, 'ifRefused': 'when here'}})
        with self.assertRaisesRegex(ValueError, 'one wolf'):
            rules.validate(world)
        days = story(1, refusal='skip')
        days['phases'][0]['triggers'][0]['actions'][1]['effect']['waitDays'] = 3
        with self.assertRaisesRegex(ValueError, 'waitDays'):
            rules.validate(days)

    # ---- a direction on a busy world (known problem 2)
    def test_a_direction_lands_while_world_events_wait(self):
        p = self.examples['road-war']
        self.rewrite(f"""INSERT INTO live.factions(world_id,id,name) VALUES ('{self.world}','city_watch','Watch'),
                        ('{self.world}','road_company','Road company') ON CONFLICT DO NOTHING""")
        ticket = runner.approve(self.conn, self.world, 'martin', p, rules.digest(p))['ticket']
        self.run_id = runner.publish(self.conn, self.world, ticket)['run']
        self.answer()
        self.fact('conversation', ASH, 'rm112')
        self.tick()
        self.assertEqual(self.one()['state']['phase'], 'blockade')
        data = {'run': self.run_id, 'signal': 'accord signed', 'operator': 'director'}
        issued = runner.direct(self.conn, self.world, 'signal-ticket', data)['ticket']   # (before the phase's effects are answered)
        for _ in range(3):
            self.answer()
            self.tick()
        self.trades(50)                                # (A busy world: events wait past the cursor.)
        out = runner.direct(self.conn, self.world, 'signal', {'ticket': issued})
        self.assertEqual(out['status'], 'queued')
        self.tick()
        self.assertEqual(self.one()['state']['phase'], 'truce')
        cmd = self.conn.execute("SELECT status,payload FROM dm.story_commands WHERE run=%s", (self.run_id,)).fetchone()
        self.assertEqual((cmd[0], cmd[1]['direction']), ('done', True))

    # ---- the history's rule
    def test_old_history_is_pruned_by_the_rule(self):
        self.publish(story(0))
        self.trades(12)
        self.tick()
        rows = self.conn.execute('SELECT count(*) FROM dm.story_trigger_history WHERE run=%s', (self.run_id,)).fetchone()[0]
        self.assertEqual(rows, 12)
        self.rewrite(f"UPDATE dm.story_trigger_history SET recorded_at=now()-interval '100 days' WHERE run='{self.run_id}'")
        gone = self.conn.execute('SELECT dm.prune_story_history(%s,90,5)', (self.world,)).fetchone()[0]
        self.assertEqual(gone, 7, 'older than 90 days, but the newest 5 kept')
        self.assertEqual(self.conn.execute('SELECT dm.prune_story_history(%s,90,5)', (self.world,)).fetchone()[0], 0)
        self.rewrite("UPDATE dm.story_trigger_history SET recorded_at=now()")
        self.assertEqual(self.conn.execute('SELECT dm.prune_story_history(%s,90,1)', (self.world,)).fetchone()[0], 0, 'young: kept')
        self.assertEqual(runner.prune_history(self.conn, self.world), 0)


def dsn(name):
    s = W.settings()
    return dict(host=s.get('RATW_DB_HOST', '127.0.0.1'), port=s.get('RATW_DB_PORT', '5433'), dbname=name,
                user=s.get('POSTGRES_USER', 'postgres'), password=s.get('POSTGRES_PASSWORD', ''))


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class RealRunner(unittest.TestCase):
    """The runner as it runs: its loop, and its process killed and started again."""

    def setUp(self):
        W.ensure_roles()
        self.name = f'ratw_test_storyspeed_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {self.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {self.name} TO ratw_editor, ratw_publisher, ratw_game, ratw_dm')
        self.addCleanup(self.drop)
        with W.connect('dev', 'owner', dbname=self.name) as owner:
            W.migrate(owner)
            self.world = 'speed'
            owner.execute('INSERT INTO world.worlds(id,name) VALUES (%s,%s)', (self.world, 'Speed'))
            owner.execute('INSERT INTO game.checkpoints(world_id,schema_version,revision,payload) VALUES (%s,1,1,%s)',
                          (self.world, '{"calendarDays":10}'))
            for wolf in (ASH, BIRCH, 'player-elm'):
                owner.execute("INSERT INTO game.characters(world_id,key,position,data) VALUES (%s,%s,1,'{}')", (self.world, wolf))
        self.conn = psycopg.connect(**dsn(self.name), autocommit=True)
        self.addCleanup(self.conn.close)
        self.procs = []

    def drop(self):
        for p in self.procs:
            if p.poll() is None:
                p.kill()
                p.wait()
        with superuser() as su:
            su.execute(f'DROP DATABASE IF EXISTS {self.name} WITH (FORCE)')

    def events(self, batch):
        with self.conn.transaction():
            self.conn.execute('SET LOCAL ROLE ratw_game')
            self.conn.execute('SELECT game.record_events(%s,%s::jsonb)', (self.world, json.dumps(batch)))
            self.conn.execute("SELECT pg_notify('ratw_events',%s)", (self.world,))

    def publish(self, p):
        with self.conn.transaction():
            self.conn.execute('SET LOCAL ROLE ratw_dm')
            ticket = runner.approve(self.conn, self.world, 'martin', p, rules.digest(p))['ticket']
            run = runner.publish(self.conn, self.world, ticket)['run']
        return run

    def answer_all(self):
        with self.conn.transaction():
            self.conn.execute('SET LOCAL ROLE ratw_game')
            for rid, kind, payload in self.conn.execute("SELECT id,kind,payload FROM dm.actions WHERE status='queued' ORDER BY id").fetchall():
                result = json.dumps([{'ok': True, 'text': 'ok'}] * len(payload['items'])) if kind == 'story.effects' else 'ok'
                self.conn.execute("UPDATE dm.actions SET status='applied',result=%s,done_at=now() WHERE id=%s", (result, rid))

    def start(self):
        env = {**os.environ, **W.settings(), 'RATW_DEV_DBNAME': self.name, 'PYTHONUNBUFFERED': '1'}
        p = subprocess.Popen([sys.executable, str(HERE / 'story_runner.py'), '--world', 'dev'], env=env,
                             stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        self.procs.append(p)
        return p

    def cursor(self, run):
        return self.conn.execute('SELECT cursor,status,state FROM dm.story_releases WHERE id=%s', (run,)).fetchone()

    def test_the_loop_is_woken_by_the_worlds_events(self):
        run = self.publish(story(0))
        self.answer_all()
        with W.connect('dev', 'dm', dbname=self.name) as conn:
            self.assertEqual(conn.execute('SELECT current_database()').fetchone()[0], self.name)
            done = threading.Event()
            loop = threading.Thread(target=runner.serve, args=(conn, self.world), kwargs={'stop': done.is_set}, daemon=True)
            loop.start()
            time.sleep(runner.IDLE_SECONDS + 0.5)       # (Idle: waiting on its channels.)
            self.events([{'kind': 'economy', 'actor': 'till', 'target': 'b', 'day': 10}])
            began = time.monotonic()
            newest = self.conn.execute('SELECT max(id) FROM game.events').fetchone()[0]
            while self.cursor(run)[0] < newest and time.monotonic() - began < 5:
                time.sleep(0.02)
            took = time.monotonic() - began
            done.set()
            self.events([{'kind': 'clock', 'day': 10}])   # (Wakes it to see it should stop.)
            loop.join(5)
        self.assertGreaterEqual(self.cursor(run)[0], newest)
        self.assertLess(took, 1.0, f'woken by the event, not its {runner.IDLE_SECONDS} s timeout: {took:.2f} s')

    def test_killed_part_way_and_started_again_every_effect_is_given_once(self):
        run = self.publish(story(1, trades='economy'))
        self.answer_all()
        cheese = []
        batch = []
        for n in range(3000):
            item = 'cheese' if n % 15 == 0 else 'bread'
            batch.append({'kind': 'economy' if item == 'cheese' else 'gift', 'actor': 'till', 'target': 'b', 'day': 10, 'item': item, 'quantity': n})
            if item == 'cheese':
                cheese.append(n)
        self.events(batch)
        newest = self.conn.execute('SELECT max(id) FROM game.events').fetchone()[0]
        deadline = time.monotonic() + 120
        kills = 0
        in_flight_kills = 0
        while time.monotonic() < deadline:
            p = self.start()
            time.sleep(0.15 + 0.1 * (kills % 4))   # (Killed mid-read, mid-batch, or waiting for the game.)
            waiting = 'batchReceipt' in (self.cursor(run)[2] or {})
            p.send_signal(signal.SIGKILL)
            p.wait()
            kills += 1
            in_flight_kills += waiting
            if kills % 2:
                self.answer_all()                       # (The game answers some batches while the runner is down.)
            cursor, status, state = self.cursor(run)
            self.assertEqual(status, 'running', 'never paused by a kill')
            if cursor >= newest and not state.get('triggerEffects') and 'batch' not in state and kills >= 6:
                break
        # The last of it, to the end, with a runner left running.
        p = self.start()
        for _ in range(200):
            self.answer_all()
            cursor, status, state = self.cursor(run)
            if cursor >= newest and not state.get('triggerEffects') and 'batch' not in state and \
                    not self.conn.execute("SELECT 1 FROM dm.actions WHERE status='queued'").fetchone():
                break
            time.sleep(0.05)
        p.send_signal(signal.SIGTERM)
        p.wait(10)
        details = []
        for kind, payload in self.conn.execute("SELECT kind,payload FROM dm.actions WHERE kind IN ('story.effects','deed.award') ORDER BY id"):
            details.extend(i['payload']['detail'] for i in payload['items']) if kind == 'story.effects' else details.append(payload['detail'])
        firings = [r[0] for r in self.conn.execute(
            "SELECT (entry->>'event')::bigint FROM dm.story_trigger_history WHERE run=%s AND entry->>'trigger'='answer'", (run,))]
        self.assertGreaterEqual(kills, 6)
        self.assertGreater(in_flight_kills, 0, 'killed at least once with a batch waiting for the game')
        self.assertEqual(len(details), len(cheese), f'every effect sent once ({kills} kills, {in_flight_kills} with a batch waiting)')
        self.assertEqual(len(firings), len(set(firings)), 'no event answered twice')
        self.assertEqual(len(firings), len(cheese), 'none lost')
        self.assertEqual(self.cursor(run)[2]['counters']['story']['trades'], len(cheese))


if __name__ == '__main__':
    unittest.main()
