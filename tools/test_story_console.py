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


# ----- Doc 86, Phase 1a: a chapter paced by readiness (R2), for the runner's, steering's, explain's, the campaigns' and the
# console's tests (tools/test_story_v2_runner.py and the others), on the throwaway database.
MUSTER = [f'player-m{i:02d}' for i in range(16)]


def ready_program(auto=False, then=None, slow=None, after=None, minimum=2, story_id='v2-ready', ending=None):
    """A chapter of 16 wolves: it is ready when 35% of the active ones (at least 3) brought rope, and `minimum` days have
    passed in the phase."""
    block = {'all': [{'participation': {'list': 'did'}, 'of': 'active', 'percent': 35, 'floor': 3, 'cap': 150,
                      'label': 'Wolves who brought rope'},
                     {'minDays': minimum, 'label': 'Days of the muster'}], 'auto': auto}
    for key, value in (('then', then), ('slowAfterDays', slow), ('autoAfterDays', after)):
        if value is not None:
            block[key] = value
    return {
        'format': 'ratw-story-program', 'version': 2, 'id': story_id, 'title': 'The muster ' + story_id,
        'audience': {'kind': 'players', 'players': MUSTER}, 'start': 'work',
        'variables': [{'id': 'did', 'type': 'wolves', 'scope': 'story'}],
        'phases': [{'id': 'work', 'journal': 'Bring rope to the muster.', 'effects': [], 'ready': block,
                    'exits': [{'on': 'ready', 'to': 'done'}], 'triggers': [
                        {'id': 'rope', 'on': 'gift', 'audience': 'participant', 'actions': [
                            {'do': 'list add', 'var': 'did', 'value': {'field': 'actor'}}]}]},
                   {'id': 'done', 'journal': 'The muster is done.', 'effects': [*(ending or [])], 'exits': []}],
        'tests': [{'name': 'Ready, then Advance', 'cohort': MUSTER, 'events': [
            *({'id': f'a{i}', 'kind': 'arrival', 'day': 0.1, 'actor': w} for i, w in enumerate(MUSTER[:4])),
            *({'id': f'g{i}', 'kind': 'gift', 'day': 0.5, 'actor': w, 'item': 'rope'} for i, w in enumerate(MUSTER[:4])),
            {'id': 'c', 'kind': 'clock', 'day': max(minimum, 0) + 0.5},
            *([{'id': 'c2', 'kind': 'clock', 'day': minimum + after + 0.7}] if after else []),
            *([{'advance': 'now'}] if not auto and not after else []),
            *([{'id': 'd', 'kind': then['on'], 'day': minimum + (after or 0) + 1, 'detail': 'Restday'}] if then else []),
        ], 'expect': ['work', 'done']}],
    }


class ReadyFixture:
    """The muster's wolves in the throwaway world, and its runs."""

    def add_wolves(self):
        self.rewrite('SELECT 1')
        self.conn.execute('RESET ROLE')
        for w in MUSTER:
            self.conn.execute('INSERT INTO game.characters(world_id,key,position,data) VALUES (%s,%s,1,%s::jsonb) ON CONFLICT DO NOTHING',
                              (self.world, w, json.dumps({'name': 'Wolf ' + w[-2:]})))
        self.conn.execute('SET LOCAL ROLE ratw_dm')

    def ready_launch(self, program, active=10, day=1):
        self.add_wolves()
        self.day(day)
        for w in MUSTER[:active]:
            self.fact('arrival', w, day=day)
        self.examples = {**self.examples, program['id']: program}
        self.launch(program['id'])
        self.answer()
        return self.run_id

    def gifts(self, wolves, day):
        for w in wolves:
            self.fact('gift', w, 'rm111', day=day, item='rope', quantity=1)

    def ready(self):
        return self.state().get('ready') or {}

    def alert_kinds(self):
        return [a['kind'] for a in self.row()['alerts']]

    def day(self, n):
        self.rewrite(f"UPDATE game.checkpoints SET payload='{{\"calendarDays\":{n}}}' WHERE world_id='{self.world}'")

    def state(self):
        return runner.status(self.conn, self.world, self.run_id)['state']


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
            # (Placed, with a home: doc 86's V1 refuses a resident with no place in the world.)
            owner.execute("INSERT INTO world.areas(world_id,id,kind) VALUES (%s,'fixture-homes','cell')", (cls.world,))
            for npc, name in RESIDENTS.items():
                owner.execute("""INSERT INTO live.npcs(world_id,id,name,role,age,voice,appearance,hours_start,hours_end,
                    home_area,home_x,home_y,work_x,work_y,evening_x,evening_y) VALUES (%s,%s,%s,'civilian',4,1,'{}',8,12,'fixture-homes',0,0,0,0,0,0)""",
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


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class ReadinessInTheConsole(ReadyFixture, unittest.TestCase):
    """Doc 86, R3's server side: the overview and the run's detail carry its readiness; Advance through the route's
    command lands in the audit."""
    setUpClass = classmethod(ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(ConsoleReads.tearDownClass.__func__)
    setUp = ConsoleReads.setUp
    fact, launch, answer, tick, rewrite, row = (ConsoleReads.fact, ConsoleReads.launch, ConsoleReads.answer, ConsoleReads.tick,
                                                ConsoleReads.rewrite, ConsoleReads.row)

    def test_overview_and_detail_carry_ready_and_advance_is_audited(self):
        self.ready_launch(ready_program(slow=6))
        self.gifts(MUSTER[:4], 1)
        self.day(3.1)
        self.tick()
        row = self.row()
        self.assertEqual((row['ready']['met'], row['ready']['to'], row['ready']['activeDays']), (True, 'done', 18))
        self.assertEqual([p['kind'] for p in row['ready']['parts']], ['participation', 'minDays'])
        self.assertEqual(row['ready']['parts'][0]['audience'], {'all': 16, 'active': 10, 'online': None})
        one = runner.status(self.conn, self.world, self.run_id)
        self.assertEqual(one['ready']['metAt'], 3.1)
        self.assertEqual([e['kind'] for e in one['readyLog']], ['ready'])
        self.assertEqual(one['controls']['ready'], {'to': 'done', 'auto': False, 'then': None, 'thenWords': None, 'met': True})
        sent = runner.command(self.conn, self.world, 'martin', {
            'run': self.run_id, 'kind': 'advance', 'payload': {'when': 'now'}, 'reason': 'ready, and the admin agrees',
            'key': secrets.token_hex(8), 'visit': one['visit'], 'digest': one['digest']})
        self.tick()
        self.assertEqual(self.conn.execute('SELECT status FROM dm.story_commands WHERE id=%s', (sent['id'],)).fetchone()[0], 'done')
        audit = self.conn.execute("SELECT detail FROM dm.audit WHERE action='story.advance'").fetchone()[0]
        self.assertIn('Advanced to done now (ready)', audit)
        self.assertIn('ready, and the admin agrees', audit)


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class HousePursesRoute(unittest.TestCase):
    """Doc 86, M3 (review of Phase 9): the world switch for House purses through POST /api/story-program: read by a Dungeon
    Master, turned on and off by the owner (or the studio's admin) alone, audited, and sent to the game as an order
    dm.actions takes (story.house_purses: its kinds have no hyphen)."""

    @classmethod
    def setUpClass(cls):
        ConsoleRoute.setUpClass.__func__(cls)
        with admin(cls.name) as owner:
            owner.execute('SET ROLE ratw_dm')
            D.create_account(owner, 'camp-owner', 'owner', 'a long owner password')
            D.create_account(owner, 'camp-admin', 'admin', 'a long admin password')
            owner.execute('RESET ROLE')
            owner.execute("INSERT INTO live.factions(world_id,id,name) VALUES (%s,'house_greatly','House Greatly'),(%s,'council','The Council')",
                          (cls.world, cls.world))

    tearDownClass = classmethod(ConsoleRoute.tearDownClass.__func__)
    call, token = ConsoleRoute.call, ConsoleRoute.token

    def test_the_owner_turns_house_purses_on_and_off(self):
        dm = self.token('camp-dm', 'a long dm password')
        status, state = self.call('/api/story-program', {'op': 'house-purses-state', 'target': 'dev'}, dm)
        self.assertEqual(status, 200, state)
        self.assertEqual((state['on'], state['houses']), (False, [{'id': 'house_greatly', 'name': 'House Greatly', 'purse': None}]))
        status, refused = self.call('/api/story-program', {'op': 'house-purses', 'target': 'dev', 'on': True, 'reason': 'the check passed'}, dm)
        self.assertEqual(status, 403, refused)
        owner = self.token('camp-owner', 'a long owner password')
        status, bad = self.call('/api/story-program', {'op': 'house-purses', 'target': 'dev', 'on': 'yes', 'reason': 'the check passed'}, owner)
        self.assertEqual(status, 422, bad)
        status, out = self.call('/api/story-program', {'op': 'house-purses', 'target': 'dev', 'on': True, 'reason': 'the check passed'}, owner)
        self.assertEqual(status, 200, out)
        self.assertEqual((out['on'], out['houses'], out['saved']), (True, ['house_greatly'], False))
        with admin(self.name) as conn:
            order = conn.execute('SELECT kind,target_id,requested_by FROM dm.actions WHERE id=%s', (out['receipt'],)).fetchone()
            self.assertEqual(order, ('story.house_purses', 'on', 'camp-owner'))
            audit = conn.execute("SELECT target,detail,who FROM dm.audit WHERE action='story.house-purses'").fetchall()
            self.assertEqual(audit, [('on', 'House purses on. Reason: "the check passed"', 'camp-owner')])
        admin_ = self.token('camp-admin', 'a long admin password')
        status, out = self.call('/api/story-program', {'op': 'house-purses', 'target': 'dev', 'on': False, 'reason': 'not yet after all'}, admin_)
        self.assertEqual(status, 200, out)
        status, state = self.call('/api/story-program', {'op': 'house-purses-state', 'target': 'dev'}, dm)
        self.assertEqual([a['on'] for a in state['audit']], [False, True], 'newest first')


if __name__ == '__main__':
    unittest.main()
