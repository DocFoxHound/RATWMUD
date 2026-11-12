#!/usr/bin/env python3
"""Migration 0061 (Docs/Design/79-a-staged-economy.md, 14; Phase 9): schema only, on every path a world takes to it.

(a) 0001 to 0061 on an empty database; (b) 0001 to 0054, then 0055 to 0061 one at a time (PROD's path); (c) 0001 to 0060
with business_settings rows, a season in 4 parts, dead rules, nested rules values and a full economy row, then 0061: every
live.economy row byte-identical, business_settings gone, people_manifest working for a world and for one NPC, and its
lines what tools/manifest_lines.py writes for the same entity.

Each case in its own throwaway database, `ratw_test_s2stage_mig_<hex>`, dropped after; skipped without PostgreSQL.

(d) Opt in: RATW_MIGRATE_COPIES names real world databases (e.g. "ratw_ridgemere2,ratw_prod": DEV at 0060, PROD at
0054). Each is copied read only (pg_dump) into a throwaway `ratw_test_s2stage_copy_<hex>`, migrated to 0061 there, its
live.economy rows checked byte-identical and its manifest read; then, with RATW_TEST_SERVER, a scratch game server (it
writes nothing) is started on the copy and must load the world and listen. The real databases are only read.
"""
import json
import os
import secrets
import signal
import subprocess
import unittest
from pathlib import Path

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
            self.assertEqual(lines[0], 'economy 1234 40 30')
            self.assertIn('loop needs off', lines)
            for gone in ('store ', 'nostore ', 'countryoff ', 'subsidy ', 'comforts ', 'farms ', 'farmarea ', 'farm '):
                self.assertFalse([l for l in lines if l.startswith(gone)], gone)
            self.assertIn('season "bread" price 100 95 105 120 shelf 100 110 100 90', lines)
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
    """0061 on copies of the real worlds (DEV at 0060, PROD at 0054 as their migrations stand), never on them."""

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
                    self.assertEqual(len(W.migrate(db.owner)), 1, f'{source}: 0061 alone after 0060')
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
