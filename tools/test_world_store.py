#!/usr/bin/env python3
"""Worlds and the roster in PostgreSQL: exact round trips, minimal writes, conflicts.

Runs in a scratch database dropped afterwards; skipped when the local
PostgreSQL is not running (python3 tools/world_db.py up).
"""
import copy
import secrets
import unittest

import psycopg

import map_editor as E
import roster as R
import world_db as W
import world_store as S
from test_world_db import database_available, superuser


def greyfen():
    world = next(w for w in E.bundled_worlds() if w['id'] == 'Greyfen')
    return E.read_json(world['source'].read_text(encoding='utf-8'))


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class StoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.name = f'ratw_test_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner)
        cls.conn = W.connect('dev', 'editor', dbname=cls.name)
        S.save_roster(cls.conn, R.load())

    @classmethod
    def tearDownClass(cls):
        cls.conn.close()
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def setUp(self):
        self.conn.execute('DELETE FROM world.worlds')

    def create(self, project):
        return S.save_world(self.conn, project, None, create=True)

    def test_greyfen_round_trip_produces_identical_game_files(self):
        source = greyfen()
        self.create(source)
        loaded, revision = S.load_world(self.conn, 'greyfen')
        self.assertEqual(revision, 1)
        roster = R.load()
        before, after = E.export_files(source, roster), E.export_files(loaded, roster)
        del before['atlas.json'], after['atlas.json']     # The embedded source spells out default lighting.
        self.assertEqual(before, after)

    def test_resaving_unchanged_world_writes_nothing(self):
        self.create(greyfen())
        loaded, _ = S.load_world(self.conn, 'greyfen')
        p, _ = E.check_project(loaded)
        with self.conn.transaction():
            self.assertEqual(S.write_rows(self.conn, S.project_rows(p, 128), S.stored_rows(self.conn, 'greyfen')), 0)

    def test_one_tile_edit_writes_one_chunk(self):
        self.create(greyfen())
        loaded, revision = S.load_world(self.conn, 'greyfen')
        town = loaded['cells'][0]['terrain']
        town[3] = town[3][:5] + '~' + town[3][6:]
        p, _ = E.check_project(loaded)
        with self.conn.transaction():
            self.assertEqual(S.write_rows(self.conn, S.project_rows(p, 128), S.stored_rows(self.conn, 'greyfen')), 1)
            raise psycopg.Rollback
        self.assertEqual(S.save_world(self.conn, loaded, revision), revision + 1)
        self.assertEqual(S.load_world(self.conn, 'greyfen')[0]['cells'][0]['terrain'][3][5], '~')

    def test_a_whole_world_save_keeps_npcs_the_game_server_made_since_it_was_read(self):
        # (A dev world run as the server, doc 66: births and newcomers land in live.npcs without raising the world's
        # revision, so a project read before them, e.g. the generator's or apply_terrain's, must not delete them.)
        self.create(greyfen())
        loaded, revision = S.load_world(self.conn, 'greyfen')
        columns = [c for c in S.TABLES['live.npcs'][1] if c not in ('id',)]
        self.conn.execute(f"INSERT INTO live.npcs (id, origin, {', '.join(columns)}) SELECT 'born_x', 'runtime', "
                          f"{', '.join(columns)} FROM live.npcs WHERE id = 'wren'")
        gone = next(p['id'] for p in loaded['people'] if p['id'] != 'wren')
        loaded['people'] = [p for p in loaded['people'] if p['id'] != gone]       # An authored NPC removed on purpose.
        S.save_world(self.conn, loaded, revision)
        ids = {r[0] for r in self.conn.execute('SELECT id FROM live.npcs').fetchall()}
        self.assertIn('born_x', ids, 'born while the project was open: kept')
        self.assertNotIn(gone, ids, 'an authored NPC the save leaves out still goes')

    def test_loads_after_the_first_reuse_the_ground_but_never_a_stale_one(self):
        self.create(greyfen())
        first, revision = S.load_world(self.conn, 'greyfen')
        again, _ = S.load_world(self.conn, 'greyfen')
        self.assertEqual(first, again)
        again['cells'][0]['terrain'][0] = 'changed by a caller'
        self.assertEqual(S.load_world(self.conn, 'greyfen')[0], first)          # Each load gets its own copy.
        # Made again from scratch with other ground, at the same revision number: the new ground, not the kept one.
        self.conn.execute('DELETE FROM world.worlds')
        other = copy.deepcopy(first)
        row = other['cells'][0]['terrain'][2]
        other['cells'][0]['terrain'][2] = '~' + row[1:]
        self.create(other)
        remade, remade_revision = S.load_world(self.conn, 'greyfen')
        self.assertEqual(remade_revision, revision)
        self.assertEqual(remade['cells'][0]['terrain'][2][0], '~')

    def test_stale_saves_and_duplicate_creates_are_refused(self):
        self.create(greyfen())
        loaded, revision = S.load_world(self.conn, 'greyfen')
        S.save_world(self.conn, loaded, revision)
        with self.assertRaises(S.ConflictError):
            S.save_world(self.conn, loaded, revision)     # Someone else saved since this copy was opened.
        with self.assertRaises(S.ConflictError):
            self.create(greyfen())

    def test_recutting_cells_moves_edges_in_one_save(self):
        demo = E.demo_project()
        demo['id'] = 'juniper'
        self.create(demo)
        loaded, revision = S.load_world(self.conn, 'juniper')
        # Widen the first cell and shrink its neighbour (the ground moves with the edge): overlapping only part-way
        # through the save.
        a, b = loaded['cells'][0], loaded['cells'][1]
        a['terrain'] = [ra + rb[:8] for ra, rb in zip(a['terrain'], b['terrain'])]
        b['terrain'] = [rb[8:] for rb in b['terrain']]
        a['width'] += 8
        b['x'] += 8
        b['width'] -= 8
        S.save_world(self.conn, loaded, revision)
        cells = S.load_world(self.conn, 'juniper')[0]['cells']
        self.assertEqual((cells[0]['width'], cells[1]['x']), (40, 40))

    def test_deleting_an_interior_removes_its_doors(self):
        self.create(greyfen())
        loaded, revision = S.load_world(self.conn, 'greyfen')
        room = loaded['rooms'][0]
        for person in loaded['people']:                    # Anyone living or working there moves out first.
            for k in ('home', 'work', 'evening'):
                if person[k]['cell'] == room['id']:
                    person[k] = dict(loaded['spawn'])
        loaded['rooms'] = [r for r in loaded['rooms'] if r['id'] != room['id']]
        loaded['links'] = [l for l in loaded['links'] if room['id'] not in (l['a']['cell'], l['b']['cell'])]
        S.save_world(self.conn, loaded, revision)
        again = S.load_world(self.conn, 'greyfen')[0]
        self.assertNotIn(room['id'], [r['id'] for r in again['rooms']])
        self.assertEqual(len(again['links']), len(loaded['links']))

    def test_npcs_without_a_map_round_trip_and_export(self):
        # Doc 66, 2: no home (unplaced), a home and no work (jobless), a job without a workplace, in rows and the manifest.
        p = greyfen()
        people = {x['id']: x for x in p['people']}
        people['rook'].update(home=None, work=None, evening=None)
        people['linden'].update(work=None, evening=None)
        self.create(p)
        loaded, revision = S.load_world(self.conn, 'greyfen')
        again = {x['id']: x for x in loaded['people']}
        self.assertEqual((again['rook']['home'], again['rook']['work'], again['rook']['evening']), (None, None, None))
        self.assertEqual((again['linden']['work'], again['linden']['evening']), (None, None))
        self.assertEqual(again['linden']['home'], people['linden']['home'])
        rows = dict(self.conn.execute("SELECT id, home_area FROM live.npcs WHERE id IN ('rook', 'linden')").fetchall())
        self.assertEqual(rows, {'rook': None, 'linden': 'cottage'})
        manifest = E.export_files(loaded, R.load())['world.ratw']
        rook = next(l for l in manifest.split('\n') if l.startswith('resident "rook"'))
        self.assertTrue(rook.endswith(' "-" 0 0 "-" 0 0 "-" 0 0'), rook)
        linden = next(l for l in manifest.split('\n') if l.startswith('resident "linden"'))
        self.assertTrue(linden.endswith(' "cottage" 9.5 7.5 "-" 0 0 "-" 0 0'), linden)
        # Given a home again.
        again['rook']['home'] = dict(people['linden']['home'])
        S.save_world(self.conn, loaded, revision)
        self.assertEqual(self.conn.execute("SELECT home_area FROM live.npcs WHERE id = 'rook'").fetchone()[0], 'cottage')

    def test_a_world_of_interiors_without_a_spawn_saves_but_does_not_export(self):
        # Ridgemere 2.0 (doc 66): every building kept, no map and no spawn yet.
        p = greyfen()
        for person in p['people']:
            for k in ('home', 'work', 'evening'):
                if person[k]['cell'] == 'town':
                    person[k] = None
        p['herbPatch'] = None
        p['routes'] = []
        for person in p['people']:
            person['route'] = ''
        p['links'] = [l for l in p['links'] if 'town' not in (l['a']['cell'], l['b']['cell'])]
        p['cells'], p['spawn'] = [], None
        _, warnings = E.check_project(p, for_game=False, playable=False)
        self.assertTrue(any('no player spawn' in w for w in warnings), warnings)
        self.create(p)
        loaded, _ = S.load_world(self.conn, 'greyfen')
        self.assertEqual((loaded['cells'], loaded['spawn']), ([], None))
        with self.assertRaisesRegex(E.ValidationError, 'spawn'):
            E.export_files(loaded, R.load(), stream=True)
        loaded['spawn'] = {'cell': 'longhouse', 'x': 2, 'y': 9}       # A spawn indoors is enough: no outdoor cell needed.
        files = E.export_files(loaded, R.load(), stream=True)
        self.assertIn('spawn "longhouse" 2.5 9.5', files['world.ratw'])

    def test_larger_terrain_blocks(self):
        self.conn.execute("INSERT INTO world.worlds (id, name, chunk_size) VALUES ('big', 'Big', 512)")
        demo = {**E.demo_project(), 'id': 'big'}
        S.save_world(self.conn, demo, 0)
        rows = self.conn.execute("SELECT size, char_length(glyphs) FROM world.terrain_chunks WHERE world_id = 'big'").fetchall()
        self.assertEqual(rows, [(512, 512 * 512)])
        self.assertEqual([c['terrain'] for c in S.load_world(self.conn, 'big')[0]['cells']],
                         [c['terrain'] for c in E.to_v3(copy.deepcopy(demo))['cells']])

    def test_slots_need_professions_from_the_roster(self):
        p = greyfen()
        p['slots'] = [{'id': 'odd_job', 'name': 'Odd job', 'profession': 'astronomer', 'workLabel': '', 'route': '', 'paid': True,
                       'purse': 0, 'herbs': 0, 'meals': 0, 'hours': {'start': 8, 'end': 17},
                       'home': p['spawn'], 'work': p['spawn'], 'evening': p['spawn']}]
        with self.assertRaisesRegex(S.StoreError, 'astronomer'):
            self.create(p)

    def test_store_room_settings_round_trip(self):
        # Doc 67, 6: the world's switches and markups in live.economy, a business switched off in live.business_settings,
        # all in the export's records; a business back on keeps no row.
        p = greyfen()
        p['economy'] = {**p['economy'], 'internalStore': False, 'storeMarkup': 120, 'storeResale': True, 'resaleMarkup': 60,
                        'storeRule': True, 'businesses': {'brann': {'internalStore': False}}}
        self.create(p)
        loaded, revision = S.load_world(self.conn, 'greyfen')
        self.assertEqual(loaded['economy'], {**p['economy'], **S.LOOP_DEFAULTS, **S.SEASON_DEFAULTS})
        self.assertEqual(self.conn.execute('SELECT npc_id, internal_store FROM live.business_settings').fetchall(), [('brann', False)])
        lines = E.export_files(loaded, R.load())['world.ratw'].split('\n')
        self.assertIn('store off 120 resale on 60 rule on', lines)
        self.assertEqual(lines[lines.index('store off 120 resale on 60 rule on') - 1].split(' ')[0], 'economy')
        brann = next(i for i, l in enumerate(lines) if l.startswith('resident "brann"'))
        self.assertIn('nostore "brann"', lines[brann + 1:brann + 5])
        with self.conn.transaction():
            self.assertEqual(S.write_rows(self.conn, S.project_rows(E.check_project(loaded)[0], 128), S.stored_rows(self.conn, 'greyfen')), 0)
            raise psycopg.Rollback
        loaded['economy'] = {**loaded['economy'], **S.STORE_DEFAULTS, 'businesses': {'brann': {'internalStore': True}}}
        S.save_world(self.conn, loaded, revision)
        self.assertEqual(self.conn.execute('SELECT count(*) FROM live.business_settings').fetchone()[0], 0)
        again, _ = S.load_world(self.conn, 'greyfen')
        self.assertEqual(again['economy']['businesses'], {})
        lines = E.export_files(again, R.load())['world.ratw'].split('\n')
        self.assertFalse(any(l.startswith(('store ', 'nostore ')) for l in lines), 'at the defaults: no store records')
        # A business key must name an NPC, a markup must be 0 to 1000.
        for bad in ({'businesses': {'nobody': {'internalStore': False}}}, {'storeMarkup': 1001}, {'resaleMarkup': -1},
                    {'storeRule': 'yes'}, {'businesses': {'brann': {'internalStore': 0}}}):
            with self.assertRaises(E.ValidationError, msg=str(bad)):
                E.check_project({**again, 'economy': {**again['economy'], **bad}}, for_game=False, playable=False)

    def test_city_loop_settings_round_trip(self):
        # Doc 68, 9: the loop's settings in live.economy and in the export's records ("loop", "countryoff", "subsidy"
        # after the "store" line); back at the defaults, no records.
        p = greyfen()
        p['economy'] = {**p['economy'], 'gateToll': 30, 'carriage': 9, 'wageHand': 7, 'countryOff': ['fish', 'flour'],
                        'subsidies': {'carriage': 40}}
        self.create(p)
        loaded, revision = S.load_world(self.conn, 'greyfen')
        self.assertEqual(loaded['economy'], {**S.default_economy(), **p['economy'], 'businesses': {}})
        lines = E.export_files(loaded, R.load())['world.ratw'].split('\n')
        at = lines.index('loop needs on country on reserve 200 toll 30 carriage 9 wages 14 7 8 8')
        self.assertEqual(lines[at - 1].split(' ')[0], 'economy')
        self.assertEqual(lines[at + 1:at + 4], ['countryoff "fish"', 'countryoff "flour"', 'subsidy "carriage" 40'])
        # The database's own manifest writes the same lines.
        manifest = self.conn.execute("SELECT live.people_manifest('greyfen')").fetchone()[0].split('\n')
        self.assertEqual(manifest[manifest.index(lines[at]):manifest.index(lines[at]) + 4], lines[at:at + 4])
        with self.conn.transaction():
            self.assertEqual(S.write_rows(self.conn, S.project_rows(E.check_project(loaded)[0], 128), S.stored_rows(self.conn, 'greyfen')), 0)
            raise psycopg.Rollback
        loaded['economy'] = {**loaded['economy'], **E.loop_defaults()}
        S.save_world(self.conn, loaded, revision)
        again, _ = S.load_world(self.conn, 'greyfen')
        lines = E.export_files(again, R.load())['world.ratw'].split('\n')
        self.assertFalse(any(l.startswith(('loop ', 'countryoff ', 'subsidy ')) for l in lines), 'at the defaults: no loop records')
        for bad in ({'gateToll': 91}, {'carriage': -1}, {'needsPaused': 'no'}, {'countryOff': ['fish', 'fish']},
                    {'subsidies': {'roads': 10}}, {'subsidies': {'carriage': 101}}, {'wageKeeper': 1001}):
            with self.assertRaises(E.ValidationError, msg=str(bad)):
                E.check_project({**again, 'economy': {**again['economy'], **bad}}, for_game=False, playable=False)

    def test_seasons_and_comforts_round_trip(self):
        # Doc 68, Progress, "Seasons and the DM's rules" (migration 0057): the seasons and the comforts in live.economy
        # and in the export's records ("comforts", "season" after the loop's); back at country.json's, no records.
        p = greyfen()
        grain = {'supply': [50, 100, 150, 40], 'price': [120, 100, 80, 130], 'shelf': [100, 100, 100, 100], 'normal': 90}
        p['economy'] = {**p['economy'], 'comfortsAbove': 40, 'comfortsShare': None, 'seasons': {'grain': grain}}
        self.create(p)
        loaded, revision = S.load_world(self.conn, 'greyfen')
        self.assertEqual(loaded['economy'], {**S.default_economy(), **p['economy'], 'businesses': {}})
        lines = E.export_files(loaded, R.load())['world.ratw'].split('\n')
        wanted = ['comforts 40 -', 'season "grain" supply 50 100 150 40 price 120 100 80 130 shelf 100 100 100 100 normal 90']
        at = lines.index(wanted[0])
        self.assertEqual(lines[at:at + 2], wanted)
        manifest = self.conn.execute("SELECT live.people_manifest('greyfen')").fetchone()[0].split('\n')
        self.assertEqual(manifest[manifest.index(wanted[0]):manifest.index(wanted[0]) + 2], wanted, 'the database writes the same')
        with self.conn.transaction():
            self.assertEqual(S.write_rows(self.conn, S.project_rows(E.check_project(loaded)[0], 128), S.stored_rows(self.conn, 'greyfen')), 0)
            raise psycopg.Rollback
        loaded['economy'] = {**loaded['economy'], **copy.deepcopy(S.SEASON_DEFAULTS)}
        S.save_world(self.conn, loaded, revision)
        again, _ = S.load_world(self.conn, 'greyfen')
        lines = E.export_files(again, R.load())['world.ratw'].split('\n')
        self.assertFalse(any(l.startswith(('comforts ', 'season ')) for l in lines), 'as country.json has them: no records')
        for bad in ({'comfortsAbove': -1}, {'comfortsShare': 101}, {'seasons': {'grain': {**grain, 'supply': [1, 2, 3]}}},
                    {'seasons': {'grain': {**grain, 'price': [9, 100, 100, 100]}}}, {'seasons': {'Grain!': grain}},
                    {'seasons': {'grain': {**grain, 'colour': 1}}}, {'seasons': []}):
            with self.assertRaises(E.ValidationError, msg=str(bad)):
                E.check_project({**again, 'economy': {**again['economy'], **bad}}, for_game=False, playable=False)

    def test_a_missing_economy_row_is_made_from_the_defaults(self):
        # Ridgemere 2.0 has no live.economy row: its first save makes one from DEFAULT_ECONOMY (1000p), never the
        # table's column defaults (a treasury of 0).
        self.create(greyfen())
        self.conn.execute('DELETE FROM live.economy')
        loaded, revision = S.load_world(self.conn, 'greyfen')
        self.assertEqual(loaded['economy'], S.DEFAULT_ECONOMY)
        loaded['economy']['businesses']['wren'] = {'internalStore': False}
        self.assertEqual(S.DEFAULT_ECONOMY['businesses'], {}, 'the defaults are never shared')
        S.save_world(self.conn, loaded, revision)
        row = S.dict_rows(self.conn, 'SELECT * FROM live.economy', ())[0]
        self.assertEqual((row['treasury'], row['store_herbs'], row['store_meals'], row['internal_store'], row['store_markup'],
                          row['store_resale'], row['resale_markup'], row['store_rule']), (1000, 100, 50, True, 100, True, 75, False))

    def test_roster_round_trip_and_order(self):
        roster = R.load()
        self.assertEqual(S.load_roster(self.conn), roster)
        changed = copy.deepcopy(roster)
        changed['characters'].reverse()
        changed['characters'][0]['name'] = 'Renamed'
        self.assertEqual(S.save_roster(self.conn, changed), R.check_roster(changed))
        S.save_roster(self.conn, roster)
        self.assertEqual(S.load_roster(self.conn), roster)


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class WithoutMigration55Tests(unittest.TestCase):
    """A world without migration 0055 (DEV and PROD until the user migrates them) loads, saves and builds as before;
    only a store room setting away from its default is refused."""

    @classmethod
    def setUpClass(cls):
        cls.name = f'ratw_test_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner, [m for m in W.migration_files() if m[0] < '0055'])
        cls.conn = W.connect('dev', 'editor', dbname=cls.name)
        S.save_roster(cls.conn, R.load())

    @classmethod
    def tearDownClass(cls):
        cls.conn.close()
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def test_loads_and_saves_unchanged_and_refuses_a_store_setting(self):
        self.assertFalse(S.store_supported(self.conn))
        source = greyfen()
        S.save_world(self.conn, source, None, create=True)
        loaded, revision = S.load_world(self.conn, 'greyfen')
        self.assertEqual(loaded['economy'], {**source['economy'], **S.STORE_DEFAULTS, 'businesses': {}, **S.LOOP_DEFAULTS, **S.SEASON_DEFAULTS}, 'the defaults, read')
        roster = R.load()
        before, after = E.export_files(source, roster), E.export_files(loaded, roster)
        del before['atlas.json'], after['atlas.json']
        self.assertEqual(before, after)
        p, _ = E.check_project(loaded)
        with self.conn.transaction():
            self.assertEqual(S.write_rows(self.conn, S.project_rows(p, 128, S.LEGACY_TABLES), S.stored_rows(self.conn, 'greyfen')), 0)
            raise psycopg.Rollback
        self.assertEqual(S.save_world(self.conn, loaded, revision), revision + 1)
        for change in ({'storeMarkup': 50}, {'businesses': {'brann': {'internalStore': False}}}):
            with self.assertRaisesRegex(S.StoreError, '0055'):
                S.save_world(self.conn, {**loaded, 'economy': {**loaded['economy'], **change}}, revision + 1)
        import world_build
        self.assertTrue(world_build.fingerprint(self.conn, 'greyfen', S.load_roster(self.conn)))


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class WithoutMigration56Tests(unittest.TestCase):
    """A world with 0055 but not 0056 loads and saves as before, reading the loop's defaults; a loop setting away from
    its default is refused."""

    @classmethod
    def setUpClass(cls):
        cls.name = f'ratw_test_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner, [m for m in W.migration_files() if m[0] < '0056'])
        cls.conn = W.connect('dev', 'editor', dbname=cls.name)
        S.save_roster(cls.conn, R.load())

    @classmethod
    def tearDownClass(cls):
        cls.conn.close()
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def test_loads_and_saves_and_refuses_a_loop_setting(self):
        self.assertTrue(S.store_supported(self.conn))
        self.assertFalse(S.loop_supported(self.conn))
        self.assertIs(S.tables_for(self.conn), S.STORE_TABLES)
        source = greyfen()
        S.save_world(self.conn, source, None, create=True)
        loaded, revision = S.load_world(self.conn, 'greyfen')
        self.assertEqual(loaded['economy'], {**S.default_economy(), **source['economy']})
        p, _ = E.check_project(loaded)
        with self.conn.transaction():
            self.assertEqual(S.write_rows(self.conn, S.project_rows(p, 128, S.STORE_TABLES), S.stored_rows(self.conn, 'greyfen')), 0)
            raise psycopg.Rollback
        self.assertEqual(S.save_world(self.conn, {**loaded, 'economy': {**loaded['economy'], 'storeRule': True}}, revision), revision + 1)
        for change in ({'gateToll': 10}, {'countryOff': ['fish']}, {'subsidies': {'carriage': 0}}):
            with self.assertRaisesRegex(S.StoreError, '0056'):
                S.save_world(self.conn, {**loaded, 'economy': {**loaded['economy'], **change}}, revision + 1)


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class WithoutMigration57Tests(unittest.TestCase):
    """A world with 0056 but not 0057 loads and saves as before, its loop's settings too, reading country.json's seasons
    and comforts; a season or comforts setting is refused, naming 0057."""

    @classmethod
    def setUpClass(cls):
        cls.name = f'ratw_test_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner, [m for m in W.migration_files() if m[0] < '0057'])
        cls.conn = W.connect('dev', 'editor', dbname=cls.name)
        S.save_roster(cls.conn, R.load())

    @classmethod
    def tearDownClass(cls):
        cls.conn.close()
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def test_loads_and_saves_and_refuses_a_season_setting(self):
        self.assertTrue(S.loop_supported(self.conn))
        self.assertFalse(S.seasons_supported(self.conn))
        self.assertIs(S.tables_for(self.conn), S.LOOP_TABLES)
        source = greyfen()
        S.save_world(self.conn, source, None, create=True)
        loaded, revision = S.load_world(self.conn, 'greyfen')
        self.assertEqual(loaded['economy'], {**S.default_economy(), **source['economy']})
        p, _ = E.check_project(loaded)
        with self.conn.transaction():
            self.assertEqual(S.write_rows(self.conn, S.project_rows(p, 128, S.LOOP_TABLES), S.stored_rows(self.conn, 'greyfen')), 0)
            raise psycopg.Rollback
        self.assertEqual(S.save_world(self.conn, {**loaded, 'economy': {**loaded['economy'], 'gateToll': 12}}, revision), revision + 1)
        grain = {'supply': [50, 100, 150, 40], 'price': [120, 100, 80, 130], 'shelf': [100, 100, 100, 100], 'normal': 90}
        for change in ({'comfortsAbove': 30}, {'comfortsShare': 5}, {'seasons': {'grain': grain}}):
            with self.assertRaisesRegex(S.StoreError, '0057'):
                S.save_world(self.conn, {**loaded, 'economy': {**loaded['economy'], **change}}, revision + 1)


if __name__ == '__main__':
    unittest.main()
