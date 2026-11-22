"""The Campaigns console's reads (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 1): the grown
story_runner.status (the list and one run) and the new timeline, on runs of the bundled examples in a throwaway,
migrated database; and the same reads through the DM tool's route, with a DM and a viewer signed in. The game is
stood in for by answering dm.actions rows by hand, as tools/test_story_program.py does. Skipped without the local
PostgreSQL."""

import json
import secrets
import threading
import unittest
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer

import psycopg

import dungeon_master as D
import story_program as rules
import story_runner as runner
import world_db as W
from test_world_db import database_available, superuser

WOLVES = {'player-ash': 'Ash Greymantle', 'player-birch': 'Birch Oakenfall', 'player-elm': 'Elm Sandpaw'}
RESIDENTS = {'rm111': 'Ivo Tarmarsh', 'rm112': 'Wren the innkeeper', 'rm113': 'Tamsin', 'rm119': 'Old Hale'}


def admin(dbname):
    """The superuser on the throwaway database (it may take each role in turn, as the game and the DM tool would)."""
    s = W.settings()
    return psycopg.connect(host=s.get('RATW_DB_HOST', '127.0.0.1'), port=s.get('RATW_DB_PORT', '5433'), dbname=dbname,
                           user=s.get('POSTGRES_USER', 'postgres'), password=s.get('POSTGRES_PASSWORD', ''), connect_timeout=3)


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class ConsoleReads(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        W.ensure_roles()
        cls.name = f'ratw_test_campaigns_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor, ratw_publisher, ratw_game, ratw_dm')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner)
            cls.world = 'campaigns-fixture'
            owner.execute('INSERT INTO world.worlds(id,name) VALUES (%s,%s)', (cls.world, 'Test world'))
            owner.execute('INSERT INTO game.checkpoints(world_id,schema_version,revision,payload) VALUES (%s,1,1,%s)',
                          (cls.world, '{"calendarDays":10}'))
            for npc, name in RESIDENTS.items():
                owner.execute("""INSERT INTO live.npcs(world_id,id,name,role,age,voice,appearance,hours_start,hours_end,
                    home_x,home_y,work_x,work_y,evening_x,evening_y) VALUES (%s,%s,%s,'civilian',4,1,'{}',8,12,0,0,0,0,0,0)""",
                              (cls.world, npc, name))
            for wolf, name in WOLVES.items():
                owner.execute('INSERT INTO game.characters(world_id,key,position,data) VALUES (%s,%s,1,%s::jsonb)',
                              (cls.world, wolf, json.dumps({'name': name})))

    @classmethod
    def tearDownClass(cls):
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def setUp(self):
        # Every test works in one transaction it never commits (as tools/test_story_program.py's RuntimeTests).
        self.conn = admin(self.name)
        self.addCleanup(self.conn.close)
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        for wolf in WOLVES:
            self.fact('arrival', wolf)
        self.examples = runner.catalogue()['examples']

    def fact(self, kind, actor='', target='', day=10, **fields):
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute('SELECT game.record_events(%s,%s::jsonb)', (self.world, json.dumps(
            [{'kind': kind, 'actor': actor, 'target': target, 'day': day, 'item': '', 'quantity': 0, **fields}])))
        self.conn.execute('SET LOCAL ROLE ratw_dm')

    def launch(self, example):
        p = self.examples[example]
        ticket = runner.approve(self.conn, self.world, 'martin', p, rules.digest(p))['ticket']
        self.run_id = runner.publish(self.conn, self.world, ticket)['run']
        return self.run_id

    def answer(self, status='applied', result=''):
        """The game's answer to the newest order of the run."""
        order = runner.status(self.conn, self.world, self.run_id)['orders'][0]['id']
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute('UPDATE dm.actions SET status=%s,result=%s,done_at=now() WHERE id=%s', (status, result, order))
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        return order

    def tick(self):
        runner.tick(self.conn, self.world)

    def rewrite(self, sql):
        """Moves time back for what this test has written (as the superuser: no role may rewrite the event log)."""
        self.conn.execute('RESET ROLE')
        self.conn.execute(sql)
        self.conn.execute('SET LOCAL ROLE ratw_dm')

    def row(self):
        return next(r for r in runner.status(self.conn, self.world)['runs'] if r['run'] == self.run_id)

    def test_the_list_says_where_a_run_is_who_is_in_it_and_what_it_waits_for(self):
        self.launch('winter-ferries')
        row = self.row()
        self.assertEqual((row['status'], row['phase'], row['visit'], row['wolves']), ('running', 'enrol', 1, 3))
        self.assertEqual(sorted(row['cohort']), sorted(WOLVES))
        self.assertEqual(row['waiting'][0]['on'], 'game', 'the journal order is not answered yet')
        self.assertEqual(row['waiting'][0]['order']['kind'], 'story.phase')
        listing = runner.status(self.conn, self.world)
        self.assertEqual(listing['names']['player-ash'], 'Ash Greymantle')
        self.assertEqual(listing['now']['day'], 10)
        self.answer()
        self.fact('arrival', 'player-ash')
        self.tick()
        row = self.row()
        self.assertEqual((row['phase'], row['visit']), ('ferry-watch', 2))
        self.assertEqual(row['last']['type'], 'phase')
        self.assertEqual(row['waiting'][0]['on'], 'game')
        self.answer()
        self.tick()
        waiting = [w['on'] for w in self.row()['waiting']]
        self.assertEqual(waiting[:3], ['timer', 'timer', 'timer'], waiting)
        timers = [w['signal'] for w in self.row()['waiting'] if w['on'] == 'timer']
        self.assertEqual(timers, ['watch-round', 'first-freeze', 'spring-council'], 'soonest first')
        self.assertIn('wolves', waiting, 'offer-a-coil waits for a gift of rope')
        self.assertEqual(self.row()['alerts'], [])

    def test_one_run_carries_its_program_names_missions_and_only_what_is_new(self):
        self.launch('river-relief')
        self.answer()
        self.fact('conversation', 'player-ash', 'rm111')
        self.tick()
        one = runner.status(self.conn, self.world, self.run_id)
        self.assertEqual(one['program']['title'], self.examples['river-relief']['title'])
        self.assertEqual(one['names']['rm111'], 'Ivo Tarmarsh')
        self.assertEqual(one['names']['player-elm'], 'Elm Sandpaw')
        passengers = next(m for m in one['missionsNow'] if m['id'] == 'passengers')
        self.assertEqual(passengers['tracks']['player-ash'], [1, 0], 'Ash has done the first of two objectives')
        self.assertEqual([p['id'] for p in one['phases']], ['survey', 'rigging', 'accounts', 'reopened'])
        self.assertIn('rigging', one['phases'][0]['to'])
        self.assertNotIn('program', runner.status(self.conn, self.world, self.run_id, have=one['digest']))
        again = runner.status(self.conn, self.world, self.run_id, since={'visit': 1, 'trigger': 0}, have=one['digest'])
        self.assertEqual(again['history'], [], 'nothing new since visit 1')
        self.assertEqual([o['kind'] for o in one['orders']], ['story.missions', 'story.phase'], 'newest first')

    def test_the_timeline_merges_visits_firings_and_orders_newest_first(self):
        self.launch('winter-ferries')
        self.answer()
        # (One transaction shares one moment: what came before the arrival is put a minute back.)
        self.rewrite("UPDATE dm.actions SET requested_at=now()-interval '1 minute'")
        self.fact('arrival', 'player-birch', cell='ford', x=3, y=4)
        self.tick()
        self.answer()
        self.tick()
        page = runner.timeline(self.conn, self.world, self.run_id)
        kinds = [(i['type'], i.get('trigger') or i.get('kind') or i.get('phase')) for i in page['items']]
        self.assertEqual(kinds, [('order', 'story.phase'), ('phase', 'ferry-watch'), ('trigger', 'open-winter-ledger'),
                                 ('order', 'story.phase'), ('phase', 'enrol')], kinds)
        firing = page['items'][2]
        self.assertEqual((firing['actor'], firing['cell'], firing['x'], firing['y']), ('player-birch', 'ford', 3, 4))
        self.assertEqual(page['events'][str(firing['event'])]['kind'], 'arrival', 'the world event behind the firing')
        self.assertEqual(page['names']['player-birch'], 'Birch Oakenfall')
        self.assertEqual(page['items'][0]['status'], 'applied')
        self.assertIsNone(page['before'])
        # A poll since then returns nothing new (orders answered in the last few seconds come again, to be merged by
        # key); a new firing comes back alone.
        known = {i['key'] for i in page['items']}
        quiet = runner.timeline(self.conn, self.world, self.run_id, since=page['since'])
        self.assertEqual([i for i in quiet['items'] if i['key'] not in known], [])
        self.fact('gift', 'player-ash', 'rm111', item='rope', quantity=2)
        self.tick()
        fresh = runner.timeline(self.conn, self.world, self.run_id, since=page['since'])
        self.assertEqual([(i['type'], i.get('trigger')) for i in fresh['items'] if i['key'] not in known], [('trigger', 'offer-a-coil')])
        self.assertEqual(fresh['since']['trigger'], max(i['id'] for i in fresh['items'] if i['type'] == 'trigger'))
        # Paging back: two at a time, never a row twice, never one missed.
        seen, before = [], None
        while True:
            part = runner.timeline(self.conn, self.world, self.run_id, before=before, limit=2)
            seen += [i['key'] for i in part['items']]
            if not part['before']:
                break
            before = part['before']
        whole = [i['key'] for i in runner.timeline(self.conn, self.world, self.run_id)['items']]
        self.assertEqual(seen, whole)
        only = runner.timeline(self.conn, self.world, self.run_id, kinds=['trigger'], wolf='player-ash')
        self.assertEqual([i['trigger'] for i in only['items']], ['offer-a-coil'])
        with self.assertRaisesRegex(ValueError, 'Unknown timeline filter'):
            runner.timeline(self.conn, self.world, self.run_id, kinds=['weather'])

    def test_alerts_paused_the_game_silent_falling_behind_and_quiet(self):
        self.launch('winter-ferries')
        self.tick()   # (The runner's heartbeat: Phase 2's "runner" alert stands aside while it beats.)
        self.rewrite("UPDATE dm.actions SET requested_at=now()-interval '2 minutes'")
        self.assertEqual([a['kind'] for a in self.row()['alerts']], ['game'], 'the journal order has waited two minutes')
        self.answer('refused', 'that wolf is not in the world just now')
        self.tick()
        row = self.row()
        self.assertEqual(row['status'], 'paused')
        self.assertEqual([a['kind'] for a in row['alerts']], ['paused'])
        self.assertTrue(row['alerts'][0]['needsYou'])
        self.assertEqual(row['waiting'][0]['on'], 'admin')
        self.assertEqual(row['waiting'][0]['order']['status'], 'refused')
        runner.direct(self.conn, self.world, 'retry', {'run': self.run_id, 'operator': 'martin'})
        self.answer()
        self.tick()
        self.assertEqual(self.row()['alerts'], [])
        # Behind: an unread event a minute and a half old (the runner is not reading).
        self.fact('arrival', 'player-elm')
        self.rewrite("UPDATE game.events SET recorded_at=now()-interval '90 seconds' WHERE id=(SELECT max(id) FROM game.events)")
        self.assertEqual([a['kind'] for a in self.row()['alerts']], ['behind'])
        self.tick()
        self.answer()
        self.tick()
        self.assertEqual(self.row()['behind']['events'], 0)
        # Quiet: nothing for two days while a wolf of the story was about.
        self.rewrite("UPDATE dm.story_trigger_history SET recorded_at=now()-interval '2 days'")
        self.rewrite("UPDATE dm.actions SET requested_at=now()-interval '2 days'")
        self.rewrite("UPDATE game.events SET recorded_at=now()-interval '3 days'")
        self.assertEqual(self.row()['alerts'], [], 'nobody of the story has been about')
        self.fact('conversation', 'player-elm', 'rm119')
        self.tick()
        alerts = self.row()['alerts']
        self.assertEqual([a['kind'] for a in alerts], ['quiet'])
        self.assertGreater(alerts[0]['hours'], 47)

    def test_an_approved_run_waits_to_be_published_and_an_ended_one_keeps_its_counts(self):
        p = self.examples['wagon']
        runner.approve(self.conn, self.world, 'martin', p, rules.digest(p))
        approved = next(r for r in runner.status(self.conn, self.world)['runs'] if r['status'] == 'approved')
        self.assertEqual(approved['waiting'], [{'on': 'publish'}])
        self.launch('winter-ferries')
        runner.direct(self.conn, self.world, 'stop', {'run': self.run_id, 'operator': 'martin'})
        row = self.row()
        self.assertEqual((row['status'], row['visit'], row['wolves']), ('stopped', 1, 3))
        self.assertNotIn('waiting', row)
        self.assertEqual(runner.status(self.conn, self.world, self.run_id)['waiting'], [])


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class ConsoleRoute(unittest.TestCase):
    """The reads through POST /api/story-program (the DM tool's; Studio mounts it as /api/dm/story-program)."""

    @classmethod
    def setUpClass(cls):
        ConsoleReads.setUpClass.__func__(cls)
        with admin(cls.name) as owner:
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
        # Every world the tool is asked for is this throwaway database.
        dm = D.DungeonMaster(connect=lambda target: W.connect('dev', 'dm', dbname=cls.name), worlds=lambda: ['prod', 'dev'])
        cls.server = ThreadingHTTPServer(('127.0.0.1', 0), D.make_handler(dm, worlds=lambda: ['dev'], default=lambda: 'dev'))
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()
        cls.port = cls.server.server_port

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        ConsoleReads.tearDownClass.__func__(cls)

    def call(self, path, body, token=''):
        request = urllib.request.Request(f'http://127.0.0.1:{self.port}{path}', json.dumps(body).encode(), method='POST',
                                         headers={'Content-Type': 'application/json', **({'Authorization': 'Bearer ' + token} if token else {})})
        try:
            with urllib.request.urlopen(request, timeout=10) as reply:
                return reply.status, json.loads(reply.read())
        except urllib.error.HTTPError as error:
            return error.code, json.loads(error.read())

    def token(self, user, password):
        status, data = self.call('/api/login', {'username': user, 'password': password})
        self.assertEqual(status, 200, data)
        return data['token']

    def test_a_dm_reads_the_list_a_run_and_its_timeline_and_a_viewer_reads_none(self):
        dm = self.token('camp-dm', 'a long dm password')
        status, listing = self.call('/api/story-program', {'op': 'status', 'target': 'dev'}, dm)
        self.assertEqual(status, 200, listing)
        self.assertEqual([r['run'] for r in listing['runs']], [self.run_id])
        self.assertEqual(listing['runs'][0]['waiting'][0]['on'], 'game')
        status, one = self.call('/api/story-program', {'op': 'status', 'target': 'dev', 'run': self.run_id}, dm)
        self.assertEqual((status, one['program']['id']), (200, 'winter-ferries'))
        status, brief = self.call('/api/story-program', {'op': 'status', 'target': 'dev', 'run': self.run_id, 'have': one['digest'],
                                                         'since': {'visit': 1, 'trigger': 0}}, dm)
        self.assertEqual((status, 'program' in brief, brief['history']), (200, False, []))
        status, page = self.call('/api/story-program', {'op': 'timeline', 'target': 'dev', 'run': self.run_id}, dm)
        self.assertEqual(status, 200, page)
        self.assertEqual([i['type'] for i in page['items']], ['order', 'phase'])
        status, again = self.call('/api/story-program', {'op': 'timeline', 'target': 'dev', 'run': self.run_id, 'since': page['since']}, dm)
        self.assertEqual((status, [i['key'] for i in again['items']]), (200, [page['items'][0]['key']]),
                         'a queued order is sent again until it is answered')
        status, bad = self.call('/api/story-program', {'op': 'timeline', 'target': 'dev', 'run': self.run_id, 'limit': 0}, dm)
        self.assertEqual(status, 422, bad)
        viewer = self.token('camp-viewer', 'a long viewer password')
        for op in ('status', 'timeline'):
            status, data = self.call('/api/story-program', {'op': op, 'target': 'dev', 'run': self.run_id}, viewer)
            self.assertEqual(status, 403, data)


if __name__ == '__main__':
    unittest.main()
