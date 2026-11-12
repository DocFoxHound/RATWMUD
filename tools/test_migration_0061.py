#!/usr/bin/env python3
"""Migration 0061 (Docs/Design/79-a-staged-economy.md, 14; Phase 9): schema only, on every path a world takes to it.

(a) 0001 to 0061 on an empty database; (b) 0001 to 0054, then 0055 to 0061 one at a time (PROD's path); (c) 0001 to 0060
with business_settings rows, a season in 4 parts, dead rules, nested rules values and a full economy row, then 0061: every
live.economy row byte-identical, business_settings gone, people_manifest working for a world and for one NPC, and its
lines what tools/manifest_lines.py writes for the same entity.

Each case in its own throwaway database, `ratw_test_s2stage_mig_<hex>`, dropped after; skipped without PostgreSQL.

(d) Opt in: RATW_MIGRATE_COPIES names real world databases (e.g. "ratw_ridgemere2,ratw_prod": DEV at 0061 since
Phase 9, PROD at 0054). Each is copied read only (pg_dump) into a throwaway `ratw_test_s2stage_copy_<hex>`, migrated to
0061 there (nothing more for DEV's), its live.economy rows checked byte-identical and its manifest read; then, with
RATW_TEST_SERVER, a scratch game server (it writes nothing) is started on the copy and must load the world and listen.
The real databases are only read.
"""
import json
import os
import secrets
import signal
import subprocess
import unittest
from pathlib import Path

import economy_contract as C
import manifest_lines as M
import world_db as W
from test_world_db import database_available, superuser


def files_upto(last):
    return [f for f in W.migration_files() if int(f[0]) <= last]


class Scratch:
    """A throwaway database for one case."""

    def __init__(self):
        self.name = f'ratw_test_s2stage_mig_{secrets.token_hex(4)}'

    def __enter__(self):
        with superuser() as su:
            su.execute(f'CREATE DATABASE {self.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {self.name} TO ratw_editor, ratw_game')
        self.owner = W.connect('dev', 'owner', dbname=self.name)
        return self

    def __exit__(self, *_):
        self.owner.close()
        with superuser() as su:
            su.execute(f'DROP DATABASE {self.name} WITH (FORCE)')


def tables(conn):
    return {r[0] for r in conn.execute("SELECT table_name FROM information_schema.tables WHERE table_schema = 'live'")}


def columns(conn, table):
    return {r[0] for r in conn.execute("SELECT column_name FROM information_schema.columns WHERE table_schema = 'live' "
                                       "AND table_name = %s", (table,))}


def seed_world(conn, world='mig'):
    """A world, an economy row with every dead column set, two residents (one with a business switch)."""
    conn.execute("INSERT INTO world.worlds (id, name) VALUES (%s, 'Migration world')", (world,))
    conn.execute(
        "INSERT INTO live.economy (world_id, treasury, store_herbs, store_meals, daily_herbs, daily_meals, internal_store, "
        "store_markup, store_resale, resale_markup, store_rule, needs_paused, country, country_reserve, gate_toll, carriage, "
        "wage_keeper, wage_hand, wage_public, wage_country, country_off, subsidies, comforts_above, comforts_share, seasons, "
        "rules, services, farms) VALUES (%s, 1234, 40, 30, 7, 9, false, 120, false, 60, true, true, false, 300, 20, 9, 15, 9, "
        "9, 9, '{grain,ale}', '{\"carriage\": 30}', 50, 20, %s, %s, %s, %s)",
        (world,
         json.dumps({'bread': {'supply': [100, 120, 90, 80], 'price': [100, 95, 105, 120], 'shelf': [100, 110, 100, 90],
                               'normal': 12},
                     'fish': {'price': [90, 90, 100, 110], 'shelf': [100, 100, 100, 100]},
                     'odd': {'price': [100, 100]}}),
         json.dumps({'ripple': 40, 'steer': 1, 'almsADay': 12, 'prosperity': 35, 'prosperityCause': 'the good harvest',
                     'nested': {'a': [1, 2]}, 'neverImported': ['salt'],
                     'status': {'caps': {'rewardsADay': 200}}, 'market': {'events': {'perWeek': 2}}}),
         json.dumps({'groom_and_trim': {'on': False, 'price': 6, 'every': 20}}),
         json.dumps({'start': 3, 'areas': {'orchards': {'on': False, 'yieldScale': 80}}})))
    return world


def seed_people(conn, world, settings=True):
    for position, npc in enumerate(('bram', 'cora')):
        conn.execute(
            "INSERT INTO live.npcs (world_id, id, position, name, role, work_label, age, appearance, voice, paid, hours_start, "
            "hours_end, purse, herbs, meals, home_x, home_y, work_x, work_y, evening_x, evening_y) "
            "VALUES (%s, %s, %s, %s, 'civilian', 'Baker', 30, %s, 3, true, 8, 18, 20, 1, 2, 0, 0, 0, 0, 0, 0)",
            (world, npc, position, npc.title(),
             json.dumps({'species': 'wolf', 'sex': 'male', 'stature': 'average', 'pattern': 'plain',
                         'baseColor': '#776655', 'gradientColor': '#665544', 'markingColor': '#554433'})))
    if settings:
        conn.execute("INSERT INTO live.business_settings (world_id, npc_id, internal_store) VALUES (%s, 'bram', false)", (world,))


def economy_rows(conn):
    return conn.execute("SELECT world_id, to_jsonb(e) - 'updated_at' FROM live.economy e ORDER BY world_id").fetchall()


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class Migration0061Tests(unittest.TestCase):

    def check_slim(self, conn):
        self.assertNotIn('business_settings', tables(conn))
        self.assertIn('band_override', columns(conn, 'npcs'))
        # The schema keeps every dead column (D20): nothing dropped from live.economy.
        for dead in ('internal_store', 'store_markup', 'daily_herbs', 'country', 'subsidies', 'comforts_above', 'farms'):
            self.assertIn(dead, columns(conn, 'economy'), dead)

    def test_a_fresh_database_runs_every_migration(self):
        with Scratch() as db:
            W.migrate(db.owner)
            self.assertIn('0061', W.applied_migrations(db.owner))
            self.check_slim(db.owner)
            seed_world(db.owner, 'fresh')
            seed_people(db.owner, 'fresh', settings=False)
            db.owner.execute("UPDATE live.npcs SET band_override = 'rich' WHERE id = 'bram'")
            with self.assertRaises(Exception):     # The band's check holds.
                db.owner.execute("UPDATE live.npcs SET band_override = 'wealthy' WHERE id = 'bram'")

    def test_prods_path_one_at_a_time_from_0054(self):
        with Scratch() as db:
            W.migrate(db.owner, files_upto(54))
            for version in range(55, 62):
                applied = W.migrate(db.owner, files_upto(version))
                self.assertEqual(len(applied), 1, version)
            self.check_slim(db.owner)

    def test_0060_rows_are_kept_byte_for_byte(self):
        with Scratch() as db:
            W.migrate(db.owner, files_upto(60))
            world = seed_world(db.owner)
            seed_people(db.owner, world)
            before = economy_rows(db.owner)
            old = db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0]
            self.assertIn('store off 120 resale off 60 rule on', old)
            self.assertEqual(db.owner.execute('SELECT count(*) FROM live.business_settings').fetchone()[0], 1)
            self.assertIn('rule "prosperityCause" the good harvest', old)   # (0060 prints every rule as text.)
            W.migrate(db.owner)
            self.check_slim(db.owner)
            self.assertEqual(economy_rows(db.owner), before)
            npcs = db.owner.execute("SELECT id, purse, herbs, meals, band_override FROM live.npcs ORDER BY id").fetchall()
            self.assertEqual(npcs, [('bram', 20, 1, 2, None), ('cora', 20, 1, 2, None)])
            text = db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0]
            lines = text.split('\n')
            self.assertEqual(lines[0], 'economy 1234 40 30 10 12')
            self.assertIn('loop needs off', lines)
            for gone in ('store ', 'nostore ', 'countryoff ', 'subsidy ', 'comforts ', 'farms ', 'farmarea ', 'farm '):
                self.assertFalse([l for l in lines if l.startswith(gone)], gone)
            # (A group stored with its supply and normal keeps 0060's line, which a server built before Phase 9 reads.)
            self.assertIn('season "bread" supply 100 120 90 80 price 100 95 105 120 shelf 100 110 100 90 normal 12', lines)
            self.assertIn(next(l for l in old.split('\n') if l.startswith('season "bread"')), lines, '0060 wrote it so')
            self.assertIn('season "fish" price 90 90 100 110 shelf 100 100 100 100', lines)
            self.assertFalse([l for l in lines if l.startswith('season "odd"')])
            rules = [l for l in lines if l.startswith('rule ')]
            self.assertEqual(rules, ['rule "almsADay" 12', 'rule "prosperity" 35', 'rule "ripple" 40', 'rule "steer" 1'])
            overrides = [l for l in lines if l.startswith('overrides ')]
            self.assertEqual(len(overrides), 1)
            self.assertEqual(json.loads(overrides[0][len('overrides '):]),
                             {'prosperityCause': 'the good harvest', 'neverImported': ['salt'],
                              'status': {'caps': {'rewardsADay': 200}}, 'market': {'events': {'perWeek': 2}}})
            # One NPC alone: only its own lines.
            db.owner.execute("UPDATE live.npcs SET band_override = 'poor' WHERE id = 'cora'")
            one = db.owner.execute("SELECT live.people_manifest(%s, 'cora')", (world,)).fetchone()[0].split('\n')
            self.assertTrue(one[0].startswith('resident "cora"'), one)
            self.assertEqual(one[1:], ['band "cora" poor'])
            # The same entity through tools/manifest_lines.py gives the same economy lines (the overrides as JSON).
            row = db.owner.execute("SELECT treasury, store_herbs, store_meals, needs_paused, seasons, rules, services "
                                   "FROM live.economy WHERE world_id = %s", (world,)).fetchone()
            rules_ = {k: v for k, v in row[5].items() if isinstance(v, int) and not isinstance(v, bool)}
            entity = {'treasury': row[0], 'storeHerbs': row[1], 'storeMeals': row[2], 'needsPaused': row[3],
                      'seasons': row[4], 'rules': rules_, 'services': row[6],
                      'overrides': {k: row[5][k] for k in M.OVERRIDE_KEYS if k in row[5]}}
            written = M.economy_lines(entity)
            db_economy = [l for l in db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0].split('\n')
                          if l.split(' ', 1)[0] in ('economy', 'loop', 'season', 'rule', 'service', 'overrides')]
            self.assertEqual(M.comparable(written), M.comparable(db_economy))

    def test_an_overrides_line_too_long_is_written_for_core_to_skip(self):
        # Phase 9's first repair: an overrides object past Core's 65,536 bytes is written whatever its length (Core skips
        # it with a warning, RATW_ECONOMY), never left out of the manifest unsaid.
        with Scratch() as db:
            W.migrate(db.owner)
            world = seed_world(db.owner)
            big = {'market': {'wharf': {'ships': ['the Gull ' + 'x' * 70000]}}}
            db.owner.execute('UPDATE live.economy SET rules = rules || %s::jsonb WHERE world_id = %s', (json.dumps(big), world))
            lines = db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0].split('\n')
            overrides = [l for l in lines if l.startswith('overrides ')]
            self.assertEqual(len(overrides), 1)
            self.assertGreater(len(overrides[0].encode()), M.OVERRIDES_MOST)
            self.assertTrue(lines[0].startswith('economy ') and lines[0].endswith(' 10 12'), 'the old dailies kept for older servers')

    def test_malformed_stored_values_never_stop_the_manifest(self):
        # Phase 9's second repair: one bad host value (a service switched "sometimes", a price in words, a season's
        # number in words, a rule of 40.5) made live.people_manifest itself fail, so the world could not load. Now each
        # is written as a word Core skips with a warning (or, a rule that isn't a whole number, left out: the entity
        # world_store reads holds whole numbers only), and the rest of the economy reads.
        with Scratch() as db:
            W.migrate(db.owner)
            world = seed_world(db.owner)
            db.owner.execute('UPDATE live.economy SET services = %s, seasons = seasons || %s, rules = rules || %s WHERE world_id = %s',
                             (json.dumps({'groom_and_trim': {'on': 'sometimes', 'price': 'six', 'every': 20}, 'odd': 7}),
                              json.dumps({'eel': {'price': [100, 'dear', 100, 100], 'shelf': [100] * 4}}),
                              json.dumps({'ripple': 40.5}), world))
            lines = db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0].split('\n')
            self.assertIn('service "groom_and_trim" unset price unset every 20', lines)
            self.assertFalse([l for l in lines if l.startswith('service "odd"')])
            self.assertIn('season "eel" price 100 unset 100 100 shelf 100 100 100 100', lines)
            self.assertFalse([l for l in lines if l.startswith('rule "ripple"')], 'a rule of 40.5 is not written')
            # The same through manifest_lines (the exporter), line for line.
            row = db.owner.execute('SELECT seasons, services FROM live.economy WHERE world_id = %s', (world,)).fetchone()
            written = M.economy_lines({'seasons': row[0], 'services': row[1], 'rules': {'ripple': 40.5}})
            for line in ('service "groom_and_trim" unset price unset every 20', 'season "eel" price 100 unset 100 100 shelf 100 100 100 100'):
                self.assertIn(line, written)
            self.assertFalse([l for l in written if l.startswith('rule ')])

    def test_a_name_that_would_split_a_line_is_never_written(self):
        # Phase 9's last repair: a rule's (a service's, a season group's) name with a line break in it, stored past the
        # doors, was printed as two manifest lines, and the game refused the whole world for the second ("Unknown manifest
        # record"). The manifest prints only a name that is one plain word; manifest_lines the same.
        with Scratch() as db:
            W.migrate(db.owner)
            world = seed_world(db.owner)
            bad = {'zz\nbogus': 5, 'a b': 6, 'q"uote': 7, 'steerLean': 150}
            db.owner.execute('UPDATE live.economy SET rules = rules || %s, services = services || %s, seasons = seasons || %s WHERE world_id = %s',
                             (json.dumps(bad), json.dumps({'x\ny': {'on': True, 'price': 1, 'every': 2}}),
                              json.dumps({'ee\nl': {'price': [100] * 4, 'shelf': [100] * 4}}), world))
            for manifest in ('live.people_manifest', 'live.economy_manifest'):
                lines = db.owner.execute(f'SELECT {manifest}(%s)', (world,)).fetchone()[0].split('\n')
                self.assertIn('rule "steerLean" 150', lines, 'a plain name Core no longer has is still written (Core skips it)')
                self.assertFalse([l for l in lines if l.startswith(('rule "zz', 'rule "a b', 'rule "q', 'service "x', 'season "ee'))],
                                 (manifest, lines[:12]))
                # (No tail of a split line either: every line begins with a record's kind.)
                kinds = {l.split(' ', 1)[0] for l in lines}
                self.assertLessEqual(kinds, {'economy', 'loop', 'season', 'rule', 'service', 'overrides', 'resident', 'story', 'route',
                                             'joinable', 'protected', 'band', 'wander', 'faction', 'claims', 'kin'}, kinds)
            row = db.owner.execute('SELECT seasons, services, rules FROM live.economy WHERE world_id = %s', (world,)).fetchone()
            written = M.economy_lines({'seasons': row[0], 'services': row[1], 'rules': {k: v for k, v in row[2].items() if isinstance(v, int)}})
            economy = db.owner.execute('SELECT live.economy_manifest(%s)', (world,)).fetchone()[0].split('\n')
            self.assertEqual(sorted(l for l in written if l.split(' ', 1)[0] in ('rule', 'service', 'season')),
                             sorted(l for l in economy if l.split(' ', 1)[0] in ('rule', 'service', 'season')), 'manifest_lines writes the same')
            for name, fits in (('ripple', True), ('wall_limewashed', True), ('a-b', True), ('zz\nbogus', False), ('zz\n', False),
                               ('', False), ('9lives', False), ('a b', False), ('x' * 65, False)):
                self.assertIs(db.owner.execute('SELECT live.economy_name(%s)', (name,)).fetchone()[0], fits, repr(name))
                self.assertIs(C.name_fits(name), fits, repr(name))

    def test_the_sync_function_holds_only_what_the_game_reads(self):
        # Phase 9's last repair: dm.queue_economy_sync stored any JSON of any size from the editor's role (5 MB was
        # taken); it keeps {"now": true} or nothing. The publisher has no USAGE on schema dm: its dead EXECUTE grant is gone.
        with Scratch() as db:
            W.migrate(db.owner)
            with W.connect('dev', 'editor', dbname=db.name) as editor:
                big = editor.execute('SELECT dm.queue_economy_sync(%s, %s::jsonb)', ('Ada', json.dumps({'now': True, 'junk': 'x' * 200000}))).fetchone()[0]
                none = editor.execute('SELECT dm.queue_economy_sync(%s, %s::jsonb)', ('Ada', json.dumps({'now': 'yes', 'junk': [1] * 1000}))).fetchone()[0]
            rows = dict(db.owner.execute('SELECT id, payload FROM dm.actions WHERE id = ANY(%s)', ([big, none],)).fetchall())
            self.assertEqual((rows[big], rows[none]), ({'now': True}, {}))
            grants = {r[0] for r in db.owner.execute(
                "SELECT grantee FROM information_schema.routine_privileges WHERE routine_schema = 'dm' AND routine_name = 'queue_economy_sync'")}
            self.assertIn('ratw_editor', grants)
            self.assertIn('ratw_dm', grants)
            self.assertNotIn('ratw_publisher', grants)

    def test_the_economy_alone_is_the_manifests_first_lines(self):
        # Phase 9's second repair: economy.sync reads live.economy_manifest (the economy's records only), not every
        # resident; it is exactly the head of live.people_manifest, and the game may read it.
        with Scratch() as db:
            W.migrate(db.owner)
            world = seed_world(db.owner)
            seed_people(db.owner, world, settings=False)
            whole = db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0]
            with W.connect('dev', 'game', dbname=db.name) as game:
                economy = game.execute('SELECT live.economy_manifest(%s)', (world,)).fetchone()[0]
            self.assertTrue(economy.startswith('economy 1234 40 30 10 12\n'))
            self.assertTrue(whole.startswith(economy + '\n'), 'the people follow the economy')
            self.assertFalse([l for l in economy.split('\n') if l.startswith('resident ')])
            self.assertIsNone(db.owner.execute("SELECT live.economy_manifest('nowhere')").fetchone()[0])

    def test_rolled_back_by_hand_then_applied_again(self):
        # Phase 9's second repair: Database/rollback/0061_staged_economy_down.sql, as its header says to run it. After
        # it the world is 0060's (the table back with 0055's grants, 0060's manifest, 0061 not listed), the bands kept;
        # world_db.migrate then applies 0061 again (band_override already there).
        import world_store as S
        down = (Path(__file__).resolve().parent.parent / 'Database' / 'rollback' / '0061_staged_economy_down.sql').read_text()
        with Scratch() as db:
            W.migrate(db.owner, files_upto(60))
            world = seed_world(db.owner)
            seed_people(db.owner, world)
            old = db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0]
            W.migrate(db.owner)
            db.owner.execute("UPDATE live.npcs SET band_override = 'rich' WHERE id = 'bram'")
            with db.owner.transaction():
                db.owner.execute(down)
            self.assertNotIn('0061', W.applied_migrations(db.owner))
            # (Last repair: 0061's USAGE on schema dm is taken back from the editor, as 0060 had it.)
            self.assertFalse(db.owner.execute("SELECT has_schema_privilege('ratw_editor', 'dm', 'USAGE')").fetchone()[0])
            self.assertIsNone(db.owner.execute("SELECT to_regprocedure('live.economy_name(text)')").fetchone()[0])
            self.assertIn('business_settings', tables(db.owner))
            self.assertFalse(S.slim_supported(db.owner), 'never half migrated')
            # (0060's manifest again, its store line back: these NPCs have no home, so no nostore line either way. But
            # for a season stored without its supply and normal, the last repair: 0060 printed "season g supply price ...
            # normal" for it, a line a server before Phase 9 refuses the world for; the rolled-back manifest leaves it out.)
            kept = [l for l in old.split('\n') if not (l.startswith('season ') and ' supply price ' in l)]
            self.assertEqual(db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0].split('\n'), kept)
            with W.connect('dev', 'editor', dbname=db.name) as editor:
                editor.execute("INSERT INTO live.business_settings (world_id, npc_id, internal_store) VALUES (%s, 'cora', false)", (world,))
            with W.connect('dev', 'game', dbname=db.name) as game:
                with self.assertRaises(Exception):
                    game.execute("DELETE FROM live.business_settings")
            db.owner.execute('DELETE FROM live.business_settings')
            # (Last repair: a season the slim studio wrote, {price, shelf} alone, is left out of the rolled-back manifest:
            # 0060's line for it, "season g supply price ... normal", is one a server before Phase 9 refuses the world
            # for; one stored with its supply and normal is printed as 0060 printed it.)
            db.owner.execute('UPDATE live.economy SET seasons = %s WHERE world_id = %s',
                             (json.dumps({'fish': {'price': [90, 90, 100, 110], 'shelf': [100] * 4},
                                          'grain': {'supply': [100, 120, 90, 80], 'price': [100, 95, 105, 120], 'shelf': [100] * 4, 'normal': 12}}), world))
            rolled = db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0].split('\n')
            self.assertEqual([l for l in rolled if l.startswith('season ')],
                             ['season "grain" supply 100 120 90 80 price 100 95 105 120 shelf 100 100 100 100 normal 12'])
            db.owner.execute("UPDATE live.economy SET seasons = '{}' WHERE world_id = %s", (world,))
            self.assertEqual(W.migrate(db.owner), ['0061_staged_economy'])
            self.assertTrue(db.owner.execute("SELECT has_schema_privilege('ratw_editor', 'dm', 'USAGE')").fetchone()[0])
            self.check_slim(db.owner)
            self.assertEqual(db.owner.execute("SELECT band_override FROM live.npcs WHERE id = 'bram'").fetchone()[0], 'rich')
            self.assertIn('band "bram" rich', db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0])

    def test_the_game_may_still_read_the_manifest(self):
        with Scratch() as db:
            W.migrate(db.owner)
            world = seed_world(db.owner, 'game')
            seed_people(db.owner, world, settings=False)
            with W.connect('dev', 'game', dbname=db.name) as game:
                self.assertTrue(game.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0])


COPIES = [d for d in os.environ.get('RATW_MIGRATE_COPIES', '').split(',') if d]
SERVER = os.environ.get('RATW_TEST_SERVER')


@unittest.skipUnless(database_available() and COPIES, 'RATW_MIGRATE_COPIES names no database to copy')
class CopiesOfRealWorlds(unittest.TestCase):
    """0061 on copies of the real worlds (PROD at 0054; DEV at 0061 since Phase 9), never on them."""

    def copy(self, source):
        s = W.settings()
        env = {**os.environ, 'PGPASSWORD': s.get('POSTGRES_PASSWORD', '')}
        where = ['-h', s.get('RATW_DB_HOST', '127.0.0.1'), '-p', str(s.get('RATW_DB_PORT', '5433')), '-U', s.get('POSTGRES_USER', 'postgres')]
        db = Scratch()
        db.name = f'ratw_test_s2stage_copy_{secrets.token_hex(4)}'
        self.assertIn('s2stage', db.name)          # (Only ever restored into a throwaway database.)
        dump = subprocess.Popen(['pg_dump', *where, '-d', source, '-Fc'], stdout=subprocess.PIPE, env=env)
        db.__enter__()
        try:
            # (Into the empty copy; "schema public already exists" is its only expected complaint.)
            subprocess.run(['pg_restore', *where, '-d', db.name], stdin=dump.stdout, env=env, check=False, stderr=subprocess.PIPE)
            dump.stdout.close()
            self.assertEqual(dump.wait(), 0, f'pg_dump {source}')
            with superuser() as su:                 # (A database's own CONNECT grants are not in its dump.)
                su.execute(f'GRANT CONNECT ON DATABASE {db.name} TO ratw_editor, ratw_publisher, ratw_game, ratw_dm')
        except BaseException:
            db.__exit__()
            raise
        return db

    def test_each_copy_migrates_and_a_server_opens_it(self):
        for source in COPIES:
            with self.subTest(source=source):
                db = self.copy(source)
                try:
                    before_version = max(W.applied_migrations(db.owner))
                    W.migrate(db.owner, files_upto(60))    # (PROD's 0055-0060 add columns; 0061 rewrites nothing.)
                    before = economy_rows(db.owner)
                    worlds = [r[0] for r in db.owner.execute('SELECT id FROM world.worlds ORDER BY id')]
                    # (DEV has had 0061 since Phase 9: a copy of it then takes nothing more; PROD's takes 0061 alone.)
                    self.assertEqual(len(W.migrate(db.owner)), 0 if before_version >= '0061' else 1,
                                     f'{source} at {before_version}: 0061 alone after 0060')
                    self.assertIn('0061', W.applied_migrations(db.owner), f'{source} at {before_version} migrated')
                    self.assertNotIn('business_settings', tables(db.owner))
                    self.assertEqual(economy_rows(db.owner), before, f'{source}: live.economy byte for byte')
                    for world in worlds:
                        text = db.owner.execute('SELECT live.people_manifest(%s)', (world,)).fetchone()[0]
                        self.assertTrue(text.startswith('economy ') or not before, f'{source} {world}: ' + text[:80])
                    staged = db.owner.execute("SELECT coalesce(bool_or(payload::jsonb->'society' ?| array['board', 'staged']), false) "
                                              "FROM game.checkpoints").fetchone()[0]
                    if SERVER and Path(SERVER).exists():
                        self.open_with_a_server(db.name, source, staged)
                        self.storm_a_ship(db.name, source)
                finally:
                    db.__exit__()

    def open_with_a_server(self, name, source, staged):
        info = {**W.conninfo('dev', 'game'), 'dbname': name}
        url = ' '.join(f'{k}={v}' for k, v in info.items())
        server = subprocess.Popen([SERVER, '--database', 'dev', '--port', '0', '--scratch', '--for', '240'],
                                  env={**os.environ, 'RATW_DATABASE_URL': url, 'RATW_AI': 'off'},
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        seen = []
        try:
            for line in server.stdout:
                seen.append(line.rstrip())
                if 'listening on port' in line:
                    break
        finally:
            server.send_signal(signal.SIGTERM)
            out, _ = server.communicate(timeout=120)
            seen += out.splitlines()
        text = '\n'.join(seen)
        self.assertTrue(any('listening on port' in l for l in seen), f'{source}: the server never listened:\n' + text[-3000:])
        self.assertNotIn('Unknown manifest record', text, source)
        # (Phase 9's first repair: the migrated manifest reads clean, nothing of it skipped.)
        self.assertFalse([l for l in seen if 'RATW_ECONOMY skipped' in l or 'more economy records skipped' in l],
                         f'{source}: no economy record skipped')
        # A save that never ran staged (both, today) says so once (doc 79, 14).
        self.assertEqual('economy staged (doc 79)' in text, not staged, f'{source}: the first-staged line')
        loaded = [l for l in seen if 'RATW_ECONOMY' in l or 'economy' in l.lower() or 'loaded' in l.lower()]
        print(f'\n[{source}] ' + f'\n[{source}] '.join(loaded[:20]))

    def storm_a_ship(self, name, source):
        """The DM tool's storm on a world with a port, end to end on the copy: a real server (it saves to the copy), the
        Board's ships in its checkpoint, a due ship stormed through DungeonMaster.act (ship.storm), applied and journalled,
        the server killed before its next checkpoint (SIGKILL: nothing saved), and the next one holding the storm."""
        import time
        import dungeon_master as D
        info = {**W.conninfo('dev', 'game'), 'dbname': name}
        env = {**os.environ, 'RATW_DATABASE_URL': ' '.join(f'{k}={v}' for k, v in info.items()), 'RATW_AI': 'off'}
        dm = D.DungeonMaster(lambda target: W.connect('dev', 'dm', dbname=name), clock=time.time)

        def start():
            server = subprocess.Popen([SERVER, '--database', 'dev', '--port', '0', '--for', '240'], env=env,
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            seen = []
            for line in server.stdout:
                if 'listening on port' in line:
                    return server
                seen.append(line)
            server.wait(timeout=60)
            if any('closed for maintenance' in l for l in seen):
                return None                             # (The copy keeps the real world's flag: PROD's is closed.)
            self.fail(f'{source}: the server never listened: ' + ''.join(seen[-5:]))

        def checkpoint():
            with W.connect('dev', 'owner', dbname=name) as conn:
                return conn.execute("SELECT max(saved_at) FROM game.checkpoints").fetchone()[0]

        server = start()
        if server is None:
            print(f'\n[{source}] closed for maintenance (its own flag, copied): a live server refuses it, the storm not tried')
            return
        was, deadline = checkpoint(), time.time() + 45  # (A save after a roads pass: the ports' weeks rolled, the Board's
        while checkpoint() == was and time.time() < deadline:   # ships in the checkpoint.)
            time.sleep(.2)
        server.send_signal(signal.SIGTERM)
        server.communicate(timeout=120)
        due = [s for s in dm.prosperity('dev')['week']['ships'] if s['state'] == 'due' and s['id']]
        if not due:
            print(f'\n[{source}] no ship due this week (no port): the storm not tried')
            return
        ship = due[0]['id']
        server = start()
        try:
            was, deadline = checkpoint(), time.time() + 45
            while checkpoint() == was and time.time() < deadline:
                time.sleep(.2)
            taken = checkpoint()
            action = dm.act({'username': 'test', 'role': 'dm'}, 'dev', 'ship.storm', ship, {})['id']
            deadline = time.time() + 15
            while time.time() < deadline:
                with W.connect('dev', 'owner', dbname=name) as conn:
                    status, result = conn.execute('SELECT status, result FROM dm.actions WHERE id = %s', (action,)).fetchone()
                if status != 'queued':
                    break
                time.sleep(.25)
            self.assertEqual(status, 'applied', f'{source}: {result}')
            self.assertEqual(checkpoint(), taken, f'{source}: no checkpoint since the storm (the crash needs the journal alone)')
        finally:
            server.kill()                               # (SIGKILL: no save.)
            server.communicate(timeout=60)
        server = start()
        time.sleep(1)
        server.send_signal(signal.SIGTERM)
        server.communicate(timeout=120)
        states = {s['id']: s['state'] for s in dm.prosperity('dev')['week']['ships']}
        self.assertEqual(states.get(ship), 'storm', f'{source}: the storm kept across the crash: {states}')
        print(f'\n[{source}] {ship} stormed, kept across a crash ({result})')


if __name__ == '__main__':
    unittest.main()
