"""The world's facts and a watched event through a real game server and the real runner (Docs/Design/85, Phase 8), on a
disposable Greyfen: a story watching the square (`region occupied`) with conditions on the world's facts (the wolf stands
in the square, is in the world, the hour), real clients signed in and moved by the DM's orders. The game writes the
watched event, and with every write of its events (between saves, while the story is live: Phase 7's early writes) the
world's facts in the same transaction, so every event has facts covering it and they agree with it (the wolf the event
puts in the square is in the square in its facts). The runner reads them and the trigger fires once, for the first wolf in;
the second makes no `region occupied`; the deed it gives is applied by the game. Skipped without the local PostgreSQL, a
built ratw_server and Node."""

import threading
import time
import unittest

import story_program
import story_runner
import test_dm_live as live
import world_db
from test_story_scene_live import SceneClients

SQUARE = {'id': 'square', 'cell': 'town', 'x1': 30, 'y1': 18, 'x2': 36, 'y2': 24}


def program(players):
    return {
        'format': 'ratw-story-program', 'version': 2, 'id': 'square-watch', 'title': 'Watch the square',
        'audience': {'kind': 'players', 'players': players}, 'start': 'watch', 'regions': [SQUARE],
        'variables': [{'id': 'first', 'type': 'wolf', 'scope': 'story'}],
        'phases': [{'id': 'watch', 'journal': 'Somebody should keep an eye on the square.', 'effects': [], 'exits': [], 'triggers': [
            {'id': 'first-in', 'on': 'region occupied', 'audience': 'participant', 'match': {'target': 'square'},
             'if': {'test': 'all', 'conditions': [{'test': 'wolf in region', 'region': 'square'}, {'test': 'wolf here'},
                                                   {'test': 'hour between', 'from': 0, 'to': 23},
                                                   {'test': 'not', 'condition': {'test': 'wolf is', 'state': 'wanted', 'is': True}}]},
             'actions': [{'do': 'remember', 'var': 'first', 'from': 'actor'},
                         {'do': 'effect', 'effect': {'kind': 'deed.award', 'player': '$actor', 'detail': 'First to watch the square.'}}]},
        ]}],
        'tests': [{'name': 'Ash first', 'cohort': players, 'facts': {'world': {'hour': 9}, 'wolves': {
            players[0]: {'cell': 'town', 'x': 33, 'y': 21}}}, 'events': [
            {'id': 'e1', 'kind': 'region occupied', 'day': 1, 'actor': players[0], 'target': 'square', 'detail': 'run', 'quantity': 1}],
            'expect': ['watch'], 'expectVariables': {'story': {'first': players[0]}}}],
    }


@unittest.skipUnless(live.database_available() and live.SERVER.exists() and live.NODE, 'local PostgreSQL, server and Node required')
class FactsLive(unittest.TestCase):
    def setUp(self):
        self.fixture = live.DungeonMasterLive()
        self.fixture.setUpClass()
        self.addCleanup(self.fixture.tearDownClass)
        self.server, port = self.fixture.start(240)
        self.addCleanup(self.stop_server)
        self.clients = SceneClients(port)
        self.addCleanup(self.clients.close)
        self.players = {}
        for name in ('ash', 'fern'):
            self.clients.request('connect', name)
            self.players[name] = self.until(lambda name=name: self.clients.request('read', name).get('id'), 10, 'signs in')

    def stop_server(self):
        if self.server.poll() is None:
            self.server.terminate()
        self.server.communicate(timeout=40)

    def until(self, fn, seconds, label):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            result = fn()
            if result:
                return result
            time.sleep(0.15)
        self.fail('Timed out: ' + label)

    def db(self):
        return world_db.connect('dev', 'dm', dbname=self.fixture.name)

    def move(self, name, x, y):
        action = self.fixture.queue('character.move', self.players[name], {'cell': 'town', 'x': x, 'y': y})
        self.assertEqual(self.fixture.outcome(action)[0], 'applied')

    def test_a_watched_event_and_its_facts(self):
        ids = list(self.players.values())
        self.move('ash', 20, 21)
        self.move('fern', 21, 21)

        def characters():
            with self.db() as db:
                return db.execute('SELECT count(*) FROM game.characters WHERE key=ANY(%s)', (ids,)).fetchone()[0] == 2
        self.until(characters, 50, 'the wolves in the checkpoint')
        p = program(ids)
        self.assertTrue(story_program.review(p)['passed'])
        with self.db() as db:
            world = story_runner.world_id(db)
            approval = story_runner.approve(db, world, 'test-host', p, story_program.digest(p))
            run = story_runner.publish(db, world, approval['ticket'])['run']
            published = db.execute('SELECT coalesce(max(id),0) FROM game.events WHERE world_id=%s', (world,)).fetchone()[0]
        done, errors = threading.Event(), []

        saves = set()   # (Every checkpoint the game writes meanwhile: its time, which a write of facts with it shares.)

        def runner_loop():
            while not done.is_set():
                try:
                    with self.db() as db:
                        story_runner.tick_run(db, world, run, lambda: story_runner.game_day(db, world))
                        saves.add(db.execute('SELECT saved_at FROM game.checkpoints WHERE world_id=%s', (world,)).fetchone()[0])
                except Exception as exc:  # noqa: BLE001 - shown in the test thread
                    errors.append(exc)
                    break
                done.wait(0.1)
        worker = threading.Thread(target=runner_loop, daemon=True)
        worker.start()
        self.addCleanup(worker.join, 5)
        self.addCleanup(done.set)

        def phase_applied():
            with self.db() as db:
                return db.execute("SELECT status FROM dm.actions WHERE kind='story.phase' AND target_id=%s", (run,)).fetchone() == ('applied',)
        self.until(phase_applied, 20, 'the phase, with its watch list, applied by the game')
        # The wolves walk in (their clients ask the server to walk them there: no order, no save asked for), Ash first.
        self.clients.request('command', 'ash', command={'type': 'path', 'x': 33.5, 'y': 21.5})
        time.sleep(1.5)
        self.clients.request('command', 'fern', command={'type': 'path', 'x': 34.5, 'y': 22.5})

        def deed():
            with self.db() as db:
                row = db.execute("SELECT status FROM dm.actions WHERE kind='deed.award' AND target_id=%s", (ids[0],)).fetchone()
                return row if row and row[0] != 'queued' else None
        self.assertEqual(self.until(deed, 30, 'the deed for the first wolf in'), ('applied',))
        self.assertFalse(errors, errors)
        time.sleep(2)   # (Fern's way in, after.)
        with self.db() as db:
            events = db.execute("""SELECT id,kind,actor,target,detail,quantity FROM game.events WHERE world_id=%s AND id>%s
                                   AND kind IN ('region entered','region occupied') ORDER BY id""", (world, published)).fetchall()
            occupied = [e for e in events if e[1] == 'region occupied']
            self.assertEqual([(e[2], e[3], e[4], e[5]) for e in occupied], [(ids[0], 'square', run, 1)], 'occupied once, by Ash')
            self.assertEqual(sorted(e[2] for e in events if e[1] == 'region entered'), sorted(ids))
            # Every event since the story went live has the world's facts written with it, and Ash's in the square agree.
            newest = db.execute('SELECT max(id) FROM game.events WHERE world_id=%s', (world,)).fetchone()[0]
            facts = db.execute('SELECT as_of_event,facts,written_at FROM game.story_facts WHERE world_id=%s ORDER BY as_of_event',
                               (world,)).fetchall()
            self.assertTrue(facts and facts[-1][0] == newest, 'the newest event has facts')
            first = db.execute("SELECT min(id) FROM game.events WHERE world_id=%s AND id>%s AND kind='region entered'", (world, published)).fetchone()[0]
            for (eid,) in db.execute('SELECT id FROM game.events WHERE world_id=%s AND id>=%s', (world, first)):
                self.assertTrue(any(at >= eid for at, _, _ in facts), f'event {eid} has facts')
            covering = next(f for at, f, _ in facts if at >= occupied[0][0])
            ash = covering['wolves'][ids[0]]
            self.assertTrue(ash['cell'] == 'town' and 30 <= ash['x'] <= 36 and 18 <= ash['y'] <= 24, f'Ash is in the square in its facts: {ash}')
            self.assertIn('hour', covering['world'])
            self.assertEqual(set(covering), {'wolves', 'world'}, 'only what the story watches')
            saves.add(db.execute('SELECT saved_at FROM game.checkpoints WHERE world_id=%s', (world,)).fetchone()[0])
            mine = next(w for at, _, w in facts if at >= occupied[0][0])
            self.assertNotIn(mine, saves, 'the facts of Ash\'s way in were written between saves (the early writes of a live story)')
            fired = [r[0] for r in db.execute("SELECT entry->>'trigger' FROM dm.story_trigger_history WHERE run=%s", (run,))]
            self.assertEqual(fired, ['first-in'])
            state = db.execute('SELECT state FROM dm.story_releases WHERE id=%s', (run,)).fetchone()[0]
            self.assertEqual(state['counters']['story']['first'], ids[0])
            why = story_runner.why_not(db, world, run, {'trigger': 'first-in'})
            self.assertEqual(why['events'][0]['verdict'], 'fired')

    def test_a_residents_list_is_watched_and_sent_again(self):
        """Doc 86, J6: a story counting the residents of a list in a region: the phase's watch names the list's first
        members; when a trigger adds one, the runner sends the watch again (story.watch), the game takes it, and the facts it
        writes then have the new resident too."""
        ids = list(self.players.values())
        self.move('ash', 20, 21)
        self.move('fern', 21, 21)

        def characters():
            with self.db() as db:
                return db.execute('SELECT count(*) FROM game.characters WHERE key=ANY(%s)', (ids,)).fetchone()[0] == 2
        self.until(characters, 50, 'the wolves in the checkpoint')
        with self.db() as db:
            world = story_runner.world_id(db)
            first, second = [r[0] for r in db.execute('SELECT id FROM dm.story_residents WHERE world_id=%s ORDER BY id LIMIT 2', (world,))]
        at = {'test': 'count in region', 'region': 'square', 'op': '>=', 'value': 1, 'who': 'residents', 'list': 'crew'}
        p = {'format': 'ratw-story-program', 'version': 2, 'id': 'square-crew', 'title': 'The crew in the square',
             'audience': {'kind': 'players', 'players': ids}, 'start': 'watch', 'regions': [SQUARE],
             'variables': [{'id': 'crew', 'type': 'residents', 'scope': 'story', 'initial': [first]},
                           {'id': 'seen', 'type': 'number', 'scope': 'story'}],
             'phases': [{'id': 'watch', 'journal': 'Who of the crew is in the square?', 'effects': [], 'exits': [], 'triggers': [
                 {'id': 'more', 'on': 'region occupied', 'audience': 'participant', 'match': {'target': 'square'},
                  'actions': [{'do': 'list add', 'var': 'crew', 'value': second}]},
                 {'id': 'count', 'on': 'region entered', 'audience': 'participant', 'match': {'target': 'square'}, 'if': at,
                  'actions': [{'do': 'add', 'var': 'seen', 'value': 1}]}]}],
             'tests': [{'name': 'none', 'cohort': ids, 'events': [], 'expect': ['watch']}]}
        self.assertTrue(story_program.review(p)['passed'])
        with self.db() as db:
            approval = story_runner.approve(db, world, 'test-host', p, story_program.digest(p))
            run = story_runner.publish(db, world, approval['ticket'])['run']
            phase = db.execute("SELECT payload FROM dm.actions WHERE kind='story.phase' AND target_id=%s", (run,)).fetchone()[0]
        self.assertEqual(phase['watch']['residents'], [first], 'the list\'s members, as it starts')
        self.assertNotIn('residentLists', phase['watch'], 'the game is sent residents, never lists')
        done, errors = threading.Event(), []

        def runner_loop():
            while not done.is_set():
                try:
                    with self.db() as db:
                        story_runner.tick_run(db, world, run, lambda: story_runner.game_day(db, world))
                except Exception as exc:  # noqa: BLE001 - shown in the test thread
                    errors.append(exc)
                    break
                done.wait(0.1)
        worker = threading.Thread(target=runner_loop, daemon=True)
        worker.start()
        self.addCleanup(worker.join, 5)
        self.addCleanup(done.set)

        def applied(kind):
            def check():
                with self.db() as db:
                    row = db.execute('SELECT status,payload FROM dm.actions WHERE kind=%s AND target_id=%s ORDER BY id DESC LIMIT 1',
                                     (kind, run)).fetchone()
                    return row if row and row[0] != 'queued' else None
            return check
        self.assertEqual(self.until(applied('story.phase'), 20, 'the phase applied')[0], 'applied')
        with self.db() as db:
            self.assertFalse(db.execute("SELECT 1 FROM dm.actions WHERE kind='story.watch' AND target_id=%s", (run,)).fetchone(),
                             'nothing sent again while the list is as it was')
        self.clients.request('command', 'ash', command={'type': 'path', 'x': 33.5, 'y': 21.5})
        status, payload = self.until(applied('story.watch'), 30, 'the watch sent again once the list grew')
        self.assertEqual(status, 'applied', payload)
        self.assertEqual(payload['watch']['residents'], sorted([first, second]))
        self.assertEqual(payload['visit'], 1)
        time.sleep(1.5)
        self.clients.request('command', 'fern', command={'type': 'path', 'x': 34.5, 'y': 22.5})

        def facts_with_both():
            with self.db() as db:
                row = db.execute('SELECT facts FROM game.story_facts WHERE world_id=%s ORDER BY as_of_event DESC LIMIT 1', (world,)).fetchone()
                return row and set(row[0].get('residents', {})) >= {first, second}
        self.until(facts_with_both, 40, 'the facts written with both of the crew')
        self.assertFalse(errors, errors)
        with self.db() as db:
            sent = db.execute("SELECT count(*) FROM dm.actions WHERE kind='story.watch' AND target_id=%s", (run,)).fetchone()[0]
        self.assertEqual(sent, 1, 'sent again once, not every tick')


if __name__ == '__main__':
    unittest.main()
