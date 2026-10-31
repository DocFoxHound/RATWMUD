#!/usr/bin/env python3
"""World database: the world registry, new worlds, migrations, schema rules, role permissions and the publish password.

Database tests run against the local PostgreSQL (`python3 tools/world_db.py up`)
in a scratch database that is dropped afterwards; they are skipped when it is
not running. DEV and PROD are never touched, except to check who may connect.
"""
import json
import os
from pathlib import Path
import secrets
import stat
import tempfile
import unittest

import world_db as W

try:
    import psycopg
except ImportError:  # pragma: no cover
    psycopg = None


class PasswordTests(unittest.TestCase):
    def test_round_trip(self):
        record = W.hash_password('correct horse')
        self.assertTrue(W.password_matches(record, 'correct horse'))
        self.assertFalse(W.password_matches(record, 'correct horsE'))
        self.assertNotIn('correct horse', str(record))

    def test_salted(self):
        self.assertNotEqual(W.hash_password('same')['hash'], W.hash_password('same')['hash'])

    def test_malformed_records_never_match(self):
        for record in (None, {}, {'algorithm': 'md5'}, {'algorithm': 'scrypt', 'salt': '!!', 'hash': 'x', 'n': 2, 'r': 1, 'p': 1}):
            self.assertFalse(W.password_matches(record, 'password'))


class FakeConnection:
    """Enough of a psycopg connection for the pool: can close, break, and fail its liveness check."""
    opened = 0

    def __init__(self):
        FakeConnection.opened += 1
        self.closed = self.broken = self.dead = False
        self.autocommit = True
        self.checks = 0
        self.info = type('Info', (), {'transaction_status': psycopg.pq.TransactionStatus.IDLE})()

    def execute(self, sql):
        self.checks += 1
        if self.dead:
            raise OSError('server closed the connection')

    def close(self):
        self.closed = True


@unittest.skipIf(psycopg is None, 'psycopg is not installed')
class PoolTests(unittest.TestCase):
    def setUp(self):
        self.now = 0.0
        self.made = []

        def open_one():
            conn = FakeConnection()
            self.made.append(conn)
            return conn
        self.pool = W.Pool('dev', 'editor', size=2, connect_fn=open_one, idle_check=30, clock=lambda: self.now)

    def test_a_connection_is_reused(self):
        with self.pool.connection() as first:
            pass
        with self.pool.connection() as second:
            pass
        self.assertIs(first, second)
        self.assertEqual(1, len(self.made))
        self.assertFalse(first.closed)

    def test_concurrent_callers_get_their_own_and_extras_are_closed(self):
        leases = [self.pool.connection() for _ in range(3)]
        conns = [lease.__enter__() for lease in leases]
        self.assertEqual(3, len({id(c) for c in conns}))
        for lease in leases:
            lease.__exit__(None, None, None)
        self.assertEqual(1, sum(c.closed for c in conns))       # Only two are kept.

    def test_a_broken_connection_is_not_handed_out_again(self):
        with self.pool.connection() as conn:
            conn.broken = True
        with self.pool.connection() as fresh:
            pass
        self.assertIsNot(conn, fresh)

    def test_an_error_in_the_block_still_returns_the_connection(self):
        with self.assertRaises(ValueError):
            with self.pool.connection() as conn:
                raise ValueError('boom')
        with self.pool.connection() as again:
            pass
        self.assertIs(conn, again)

    def test_a_long_idle_connection_is_checked_and_replaced_if_dead(self):
        with self.pool.connection() as conn:
            pass
        self.now = 10
        with self.pool.connection():
            pass
        self.assertEqual(0, conn.checks)                         # Recently used: no check.
        conn.dead = True
        self.now = 100
        with self.pool.connection() as fresh:
            pass
        self.assertEqual(1, conn.checks)
        self.assertTrue(conn.closed)
        self.assertIsNot(conn, fresh)

    def test_an_unreachable_database_fails_when_asked_not_inside_the_block(self):
        pool = W.Pool('dev', 'editor', connect_fn=lambda: (_ for _ in ()).throw(W.DatabaseError('down')))
        with self.assertRaises(W.DatabaseError):
            pool.connection()


class SettingsTests(unittest.TestCase):
    def test_env_file_is_private_and_never_replaced(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / '.env'
            self.assertTrue(W.ensure_env(path))
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)
            values = W.read_env(path)
            for key in W.PASSWORD_KEYS:
                self.assertGreaterEqual(len(values[key]), 24)
            before = path.read_text()
            self.assertFalse(W.ensure_env(path))
            self.assertEqual(path.read_text(), before)

    def test_prod_can_live_elsewhere(self):
        saved = dict(os.environ)
        try:
            os.environ.update({'RATW_PROD_HOST': 'db.example.net', 'RATW_PROD_PORT': '6543', 'RATW_GAME_PASSWORD': 'x'})
            self.assertEqual((W.conninfo('prod', 'game')['host'], W.conninfo('prod', 'game')['port']), ('db.example.net', '6543'))
            self.assertNotEqual(W.conninfo('dev', 'game')['host'], 'db.example.net')
        finally:
            os.environ.clear(); os.environ.update(saved)

    def test_migrations_are_numbered_in_order(self):
        versions = [v for v, _, _ in W.migration_files()]
        self.assertEqual(versions, sorted(versions))
        self.assertEqual(versions[0], '0001')


def superuser():
    s = W.settings()
    return psycopg.connect(host=s.get('RATW_DB_HOST', '127.0.0.1'), port=s.get('RATW_DB_PORT', '5433'), dbname='postgres',
                           user=s.get('POSTGRES_USER', 'postgres'), password=s.get('POSTGRES_PASSWORD', ''),
                           autocommit=True, connect_timeout=3)


def database_available():
    if psycopg is None or not W.ENV_FILE.exists():
        return False
    try:
        superuser().close()
        return True
    except psycopg.Error:
        return False


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class DatabaseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.name = f'ratw_test_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'REVOKE ALL ON DATABASE {cls.name} FROM PUBLIC')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor, ratw_publisher, ratw_game')
        cls.owner = cls.as_role('owner')
        W.migrate(cls.owner)
        W.seed_publish_password(cls.owner)

    @classmethod
    def tearDownClass(cls):
        cls.owner.close()
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    @classmethod
    def as_role(cls, role):
        return W.connect('dev', role, dbname=cls.name)

    def setUp(self):
        with self.owner.transaction():
            self.owner.execute("DELETE FROM world.worlds")
            self.owner.execute("INSERT INTO world.worlds (id, name) VALUES ('w', 'Test world')")

    def add_area(self, conn, area, kind='cell', **fields):
        with conn.transaction():
            conn.execute("INSERT INTO world.areas (world_id, id, kind) VALUES ('w', %s, %s)", (area, kind))
            if kind == 'cell':
                conn.execute("""INSERT INTO world.cells (world_id, id, name, x, y, width, height)
                                VALUES ('w', %s, %s, %s, %s, %s, %s)""",
                             (area, area, fields.get('x', 0), fields.get('y', 0), fields.get('w', 32), fields.get('h', 24)))
            else:
                w, h = fields.get('w', 8), fields.get('h', 6)
                conn.execute("""INSERT INTO world.interiors (world_id, id, name, width, height, glyphs)
                                VALUES ('w', %s, %s, %s, %s, %s)""", (area, area, w, h, fields.get('glyphs', '.' * w * h)))

    def test_pooled_connections_are_reused_and_left_clean(self):
        pool = W.Pool('dev', 'editor', connect_fn=lambda: W.connect('dev', 'editor', dbname=self.name))
        try:
            with pool.connection() as conn:
                first = conn.execute('SELECT pg_backend_pid()').fetchone()[0]
                conn.execute('BEGIN')                      # Left open by a careless caller...
                conn.execute('SELECT 1')
            with pool.connection() as conn:
                self.assertEqual(first, conn.execute('SELECT pg_backend_pid()').fetchone()[0])
                self.assertEqual(psycopg.pq.TransactionStatus.IDLE, conn.info.transaction_status,
                                 '...and rolled back before reuse')  # (IDLE is reported after the SELECT completes.)
        finally:
            pool.close()

    def test_migrate_is_idempotent_and_refuses_edited_history(self):
        self.assertEqual(W.migrate(self.owner), [])
        edited = [(v, n, sql + '\n-- edited') if v == '0001' else (v, n, sql) for v, n, sql in W.migration_files()]
        with self.assertRaisesRegex(W.DatabaseError, 'changed after it was applied'):
            W.migrate(self.owner, edited)

    def test_cells_may_touch_but_not_overlap(self):
        self.add_area(self.owner, 'west', x=0, w=32)
        self.add_area(self.owner, 'east', x=32, w=32)
        with self.assertRaises(psycopg.errors.ExclusionViolation):
            self.add_area(self.owner, 'overlap', x=31, w=8)

    def test_the_world_can_grow_in_any_direction(self):
        self.add_area(self.owner, 'origin', x=0, y=0)
        self.add_area(self.owner, 'far_west', x=-320, y=-240)
        with self.owner.transaction():
            self.owner.execute("INSERT INTO world.terrain_chunks (world_id, cx, cy, size, glyphs) VALUES ('w', -10, -10, 128, %s)", ('.' * 128 * 128,))
        count = self.owner.execute("SELECT count(*) FROM world.cells WHERE world_id = 'w'").fetchone()[0]
        self.assertEqual(count, 2)

    def test_terrain_blocks_follow_their_worlds_size(self):
        with self.owner.transaction():
            self.owner.execute("UPDATE world.worlds SET chunk_size = 1024 WHERE id = 'w'")
            self.owner.execute("INSERT INTO world.terrain_chunks (world_id, cx, cy, size, glyphs) VALUES ('w', 0, 0, 1024, %s)",
                               ('.' * 1024 * 1024,))
        with self.assertRaises(psycopg.errors.ForeignKeyViolation), self.owner.transaction():
            self.owner.execute("INSERT INTO world.terrain_chunks (world_id, cx, cy, size, glyphs) VALUES ('w', 1, 0, 64, %s)",
                               ('.' * 64 * 64,))

    def test_tiles_must_be_known_glyphs_of_the_right_size(self):
        with self.assertRaises(psycopg.errors.CheckViolation):
            self.add_area(self.owner, 'short', kind='interior', w=8, h=6, glyphs='.' * 47)
        with self.assertRaises(psycopg.errors.CheckViolation):
            self.add_area(self.owner, 'odd', kind='interior', w=4, h=4, glyphs='.' * 15 + ' ')   # Not printable ASCII.
        with self.assertRaises(psycopg.errors.CheckViolation), self.owner.transaction():
            self.owner.execute("INSERT INTO world.terrain_chunks (world_id, cx, cy, size, glyphs) VALUES ('w', 0, 0, 128, %s)", ('.' * 1000,))

    def test_ids_are_checked(self):
        with self.assertRaises(psycopg.errors.CheckViolation), self.owner.transaction():
            self.owner.execute("INSERT INTO world.worlds (id, name) VALUES ('Bad ID', 'x')")

    def test_removing_an_area_that_live_data_uses_fails(self):
        self.add_area(self.owner, 'town')
        self.add_area(self.owner, 'shop', kind='interior')
        with self.owner.transaction():
            self.owner.execute("INSERT INTO live.patrol_routes (world_id, id, name) VALUES ('w', 'watch', 'Watch')")
            self.owner.execute("INSERT INTO live.patrol_posts (world_id, route_id, seq, area, x, y) VALUES ('w', 'watch', 0, 'shop', 1, 1)")
        with self.assertRaises(psycopg.errors.ForeignKeyViolation), self.owner.transaction():
            self.owner.execute("DELETE FROM world.areas WHERE world_id = 'w' AND id = 'shop'")
        # Replacing the area within one transaction (as Push to live does) is fine.
        with self.owner.transaction():
            self.owner.execute("DELETE FROM world.areas WHERE world_id = 'w' AND id = 'shop'")
            self.owner.execute("INSERT INTO world.areas (world_id, id, kind) VALUES ('w', 'shop', 'interior')")
            self.owner.execute("""INSERT INTO world.interiors (world_id, id, name, width, height, glyphs)
                                  VALUES ('w', 'shop', 'Shop', 4, 4, %s)""", ('.' * 16,))

    def test_links_join_two_different_areas(self):
        self.add_area(self.owner, 'town')
        self.add_area(self.owner, 'shop', kind='interior')
        with self.owner.transaction():
            self.owner.execute("""INSERT INTO world.links (world_id, id, name, kind, a_area, a_x, a_y, b_area, b_x, b_y)
                                  VALUES ('w', 'door', 'Door', 'door', 'town', 3, 3, 'shop', 1, 1)""")
        with self.assertRaises(psycopg.errors.CheckViolation), self.owner.transaction():
            self.owner.execute("""INSERT INTO world.links (world_id, id, name, kind, a_area, a_x, a_y, b_area, b_x, b_y)
                                  VALUES ('w', 'loop', 'Loop', 'door', 'town', 3, 3, 'town', 1, 1)""")

    def test_game_server_reads_the_world_but_cannot_change_it(self):
        self.add_area(self.owner, 'town')
        with self.as_role('game') as game:
            self.assertEqual(game.execute("SELECT count(*) FROM world.cells").fetchone()[0], 1)
            with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                game.execute("UPDATE world.cells SET name = 'Mine'")
            with game.transaction():
                game.execute("INSERT INTO live.economy (world_id, treasury) VALUES ('w', 50)")
                game.execute("INSERT INTO game.checkpoints (world_id, schema_version, revision, payload) VALUES ('w', 1, 1, '{}')")

    def test_the_event_log_is_written_once(self):
        import json
        events = [{'kind': 'economy', 'actor': 'wren', 'target': 'player-ada', 'cell': 'town', 'time': 12.5,
                   'day': 3.25, 'item': 'meal', 'quantity': 1, 'coins': 6, 'detail': 'resident food purchase'},
                  {'kind': 'death', 'actor': 'wren'}]
        with self.as_role('game') as game:
            self.assertEqual(2, game.execute('SELECT game.record_events(%s, %s::jsonb)', ('w', json.dumps(events))).fetchone()[0])
            row = game.execute("SELECT kind, actor, target, cell, game_time, game_day, item, quantity, coins, detail "
                               "FROM game.events WHERE world_id = 'w' ORDER BY id").fetchall()
            self.assertEqual(('economy', 'wren', 'player-ada', 'town', 12.5, 3.25, 'meal', 1, 6, 'resident food purchase'), row[0])
            self.assertEqual(('death', 'wren', '', '', 0.0, 0.0, '', 0, 0, ''), row[1], 'Missing fields are empty or zero')
            for change in ("UPDATE game.events SET detail = 'rewritten'", 'DELETE FROM game.events'):
                with self.assertRaises(psycopg.errors.InsufficientPrivilege, msg=change):
                    game.execute(change)
            with self.assertRaises(psycopg.errors.CheckViolation):
                game.execute('SELECT game.record_events(%s, %s::jsonb)', ('w', json.dumps([{'kind': ''}])))
        with self.as_role('editor') as editor:
            self.assertEqual(2, editor.execute('SELECT count(*) FROM game.events').fetchone()[0], 'Tools can read the log')
            with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                editor.execute('SELECT game.record_events(%s, %s::jsonb)', ('w', '[]'))

    def test_a_delta_checkpoint_stores_what_a_whole_one_would(self):
        import json

        def key(section, e):
            base = {'players': lambda: e.get('id', ''), 'npcs': lambda: e.get('id', ''),
                    'ledger': lambda: f"{e['event']}|{e['actor']}|{e['partner']}|{e['reason']}"}[section]()
            return base

        def delta(old, new):
            changes = {}
            for section in ('players', 'npcs', 'ledger'):
                before = {}
                counts = {}
                for position, e in enumerate(old.get(section, [])):
                    base = key(section, e); counts[base] = counts.get(base, 0) + 1
                    before[base + (f'#{counts[base]}' if counts[base] > 1 else '')] = (position, e)
                rows, seen, counts = [], set(), {}
                for position, e in enumerate(new.get(section, [])):
                    base = key(section, e); counts[base] = counts.get(base, 0) + 1
                    k = base + (f'#{counts[base]}' if counts[base] > 1 else '')
                    seen.add(k)
                    if before.get(k) != (position, e):
                        rows.append({'key': k, 'position': position, 'data': e})
                changes[section] = {'rows': rows, 'deleted': [k for k in before if k not in seen]}
            rest = {k: v for k, v in new.items() if k not in ('players', 'npcs', 'ledger')}
            return rest, changes

        first = {'schema': 1, 'time': 10, 'players': [{'id': 'player-ada', 'x': 1}],
                 'npcs': [{'id': 'wren', 'x': 1}, {'id': 'moss', 'x': 2}, {'id': 'rook', 'x': 3}],
                 'ledger': [{'event': 7, 'actor': 'a', 'partner': 'b', 'reason': 'r', 'amount': 1},
                            {'event': 7, 'actor': 'a', 'partner': 'b', 'reason': 'r', 'amount': 2}]}
        second = {'schema': 1, 'time': 25, 'players': [{'id': 'player-ada', 'x': 1}],
                  'npcs': [{'id': 'wren', 'x': 9}, {'id': 'rook', 'x': 3}, {'id': 'finch', 'x': 4}],
                  'ledger': [{'event': 7, 'actor': 'a', 'partner': 'b', 'reason': 'r', 'amount': 2}]}
        with self.as_role('game') as game:
            game.execute('SELECT game.save_checkpoint(%s, 1, %s)', ('w', json.dumps(first)))
            rest, changes = delta(first, second)
            self.assertEqual(['wren', 'rook', 'finch'], [r['key'] for r in changes['npcs']['rows']],
                             'Only the changed, the moved (rook is now second) and the new are sent')
            self.assertEqual(['moss'], changes['npcs']['deleted'])
            self.assertEqual([], changes['players']['rows'], 'An unchanged list sends nothing')
            game.execute('SELECT game.save_checkpoint_delta(%s, 2, %s, %s::jsonb)', ('w', json.dumps(rest), json.dumps(changes)))
            loaded = json.loads(game.execute('SELECT game.load_checkpoint(%s)', ('w',)).fetchone()[0])
            self.assertEqual(second, loaded, 'The save reads back as the new document')
            stored = lambda: {table: game.execute(f"SELECT key, position, data FROM game.{table} WHERE world_id = 'w' ORDER BY key").fetchall()
                              for table in ('characters', 'npcs', 'relationships')}
            after_delta = stored()
            game.execute('SELECT game.save_checkpoint(%s, 2, %s)', ('w', json.dumps(second)))    # The same, whole.
            self.assertEqual(stored(), after_delta, 'The tables hold what a whole save would')

    def test_bonds_are_stored_one_row_each(self):
        import json
        doc = {'schema': 1, 'bonds': [{'holder': 'wren', 'other': 'player-ada', 'affinity': 12.5, 'owed': 3},
                                      {'holder': 'player-ada', 'other': 'wren', 'affinity': 4}]}
        with self.as_role('game') as game:
            game.execute('SELECT game.save_checkpoint(%s, 1, %s)', ('w', json.dumps(doc)))
            rows = game.execute("SELECT key, holder, other, data->>'affinity' FROM game.bonds WHERE world_id = 'w' ORDER BY key").fetchall()
            self.assertEqual([('player-ada|wren', 'player-ada', 'wren', '4'), ('wren|player-ada', 'wren', 'player-ada', '12.5')], rows)
            self.assertEqual(doc, json.loads(game.execute('SELECT game.load_checkpoint(%s)', ('w',)).fetchone()[0]))
        with self.as_role('editor') as editor:
            self.assertEqual(2, editor.execute("SELECT count(*) FROM game.bonds").fetchone()[0], 'Tools can read bonds')

    def test_rumours_are_stored_one_row_each(self):
        import json
        doc = {'schema': 1, 'beliefs': [{'holder': 'wren', 'subject': 'player-ada', 'claim': 'breaks promises',
                                         'source': 'moss', 'confidence': .63, 'day': 4.5},
                                        {'holder': 'wren', 'subject': 'camp_x', 'claim': 'raids the road', 'source': 'the carters',
                                         'confidence': .9, 'day': 4}]}
        with self.as_role('game') as game:
            game.execute('SELECT game.save_checkpoint(%s, 1, %s)', ('w', json.dumps(doc)))
            rows = game.execute("SELECT key, holder, subject FROM game.beliefs WHERE world_id = 'w' ORDER BY key").fetchall()
            self.assertEqual([('wren|camp_x|raids the road', 'wren', 'camp_x'),
                              ('wren|player-ada|breaks promises', 'wren', 'player-ada')], rows)
            self.assertEqual(doc, json.loads(game.execute('SELECT game.load_checkpoint(%s)', ('w',)).fetchone()[0]))

    def test_chapters_camps_and_treaties_are_stored_one_row_each(self):
        import json
        doc = {'schema': 1,
               'chapters': {'next': 3, 'chapters': [{'id': 'ch-1', 'name': 'Ash Wardens', 'level': 3, 'members': [{'id': 'ada'}]},
                                                    {'id': 'ch-2', 'name': 'Reedfolk', 'level': 1, 'members': []}]},
               'camps': {'next': 4, 'sites': [{'id': 'site-1', 'chapter': 'ch-1', 'cell': 'moor_3'}],
                         'structures': [{'id': 'st-2', 'site': 'site-1', 'kind': 'tent', 'built': True}],
                         'staff': [{'npc': 'moss', 'site': 'site-1', 'role': 'hand', 'wage': 4}]},
               'factions': {'standings': [], 'lastDrift': 2,
                            'treaties': [{'id': 'tr-1', 'faction': 'wardens', 'chapter': 'ch-1', 'state': 'pending'}],
                            'levies': [],
                            'houses': [{'chapter': 'ch-1', 'faction': 'wardens', 'state': 'pending', 'day': 4}]}}
        with self.as_role('game') as game:
            game.execute('SELECT game.save_checkpoint(%s, 1, %s)', ('w', json.dumps(doc)))
            self.assertEqual([('ch-1', 'Ash Wardens', 3), ('ch-2', 'Reedfolk', 1)],
                             game.execute("SELECT key, name, level FROM game.chapters WHERE world_id = 'w' ORDER BY key").fetchall())
            self.assertEqual([('site-1', 'ch-1', 'moor_3')],
                             game.execute("SELECT key, chapter, cell FROM game.camp_sites WHERE world_id = 'w'").fetchall())
            self.assertEqual([('tr-1', 'ch-1', 'pending')],
                             game.execute("SELECT key, chapter, state FROM game.treaties WHERE world_id = 'w'").fetchall())
            self.assertEqual([('ch-1|wardens', 'pending')],
                             game.execute("SELECT key, state FROM game.house_requests WHERE world_id = 'w'").fetchall())
            payload = json.loads(game.execute("SELECT payload FROM game.checkpoints WHERE world_id = 'w'").fetchone()[0])
            self.assertEqual({'next': 3}, payload['chapters'], 'The checkpoint row keeps only what is not a list')
            self.assertEqual(doc, json.loads(game.execute('SELECT game.load_checkpoint(%s)', ('w',)).fetchone()[0]))
        with self.as_role('editor') as editor:
            self.assertEqual(1, editor.execute("SELECT count(*) FROM game.camp_structures").fetchone()[0], 'Tools can read camps')

    def test_editor_cannot_write_game_state_or_read_admin(self):
        with self.as_role('editor') as editor:
            with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                editor.execute("INSERT INTO game.checkpoints (world_id, schema_version, revision, payload) VALUES ('w', 1, 1, '{}')")
            with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                editor.execute("SELECT * FROM admin.settings")

    def test_publish_password(self):
        with self.as_role('publisher') as publisher:
            self.assertTrue(W.verify_publish_password(publisher, 'password'))
            self.assertFalse(W.verify_publish_password(publisher, 'Password'))
            with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                publisher.execute("UPDATE admin.settings SET value = '{}'")
        self.assertFalse(W.seed_publish_password(self.owner), 'seeding must never replace an existing password')
        with self.assertRaisesRegex(W.DatabaseError, 'at least 8'):
            W.set_publish_password(self.owner, 'short')
        W.set_publish_password(self.owner, 'a longer secret')
        with self.as_role('publisher') as publisher:
            self.assertTrue(W.verify_publish_password(publisher, 'a longer secret'))
            self.assertFalse(W.verify_publish_password(publisher, 'password'))
        W.set_publish_password(self.owner, 'password')  # Other tests expect the default.

    def test_editor_cannot_reach_prod(self):
        with self.assertRaisesRegex(W.DatabaseError, 'ratw_prod'):
            W.connect('prod', 'editor').close()


class ScratchRegistry:
    """A copy of Database/worlds.json and an empty local choice in a temporary folder, for one test."""

    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.registry = Path(self.folder.name) / 'worlds.json'
        self.registry.write_text(W.WORLDS_FILE.read_text(encoding='utf-8'), encoding='utf-8')
        self.local = Path(self.folder.name) / 'Saved' / 'worlds.local.json'
        self.saved_env = dict(os.environ)
        os.environ.update({'RATW_WORLDS_FILE': str(self.registry), 'RATW_WORLDS_LOCAL': str(self.local)})

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self.saved_env)
        self.folder.cleanup()
        W.worlds()                                   # (Back to the real registry for the next test.)

    def run_quietly(self, *argv):
        import contextlib
        import io
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = W.main(list(argv))
        return status, out.getvalue(), err.getvalue()


class RegistryTests(ScratchRegistry, unittest.TestCase):
    def test_the_registry_lists_the_three_worlds(self):
        # 2026-10-31: DEV is Ridgemere 2.0 (its database ratw_ridgemere2); the old DEV is the archive dev-old.
        self.assertEqual(W.keys(), ['prod', 'dev', 'dev-old'])
        self.assertEqual(W.keys('live'), ['prod'])
        self.assertEqual((W.kind('dev'), W.upstream('dev'), W.upstream('dev-old'), W.kind('dev-old')), ('dev', 'prod', None, 'dev'))
        self.assertEqual((W.world('dev')['name'], W.world('dev-old')['name']), ('Ridgemere 2.0 (dev)', 'Ridgemere (old map, archive)'))
        self.assertEqual(W.DATABASES, {'prod': 'ratw_prod', 'dev': 'ratw_ridgemere2', 'ridgemere2': 'ratw_ridgemere2',
                                       'dev-old': 'ratw_dev'})
        with self.assertRaisesRegex(W.DatabaseError, 'Unknown world'):
            W.world('atlantis')

    def test_ridgemere2_is_another_name_for_dev(self):
        self.assertEqual(W.world('ridgemere2')['key'], 'dev')
        self.assertEqual((W.resolve('ridgemere2'), W.resolve('ridgemere2', 'dev'), W.canonical('ridgemere2')), ('dev', 'dev', 'dev'))
        self.assertNotIn('ridgemere2', W.keys(), 'one world, listed once (the DM and migrate see it once)')
        self.assertEqual(self.run_quietly('resolve', 'ridgemere2', '--with-kind')[1].strip(), 'dev dev')
        W.set_local(atlas='ridgemere2')
        self.assertEqual((W.atlas_world(), json.loads(self.local.read_text())['atlas']), ('dev', 'dev'), 'chosen by its key')
        self.local.write_text('{"atlas": "ridgemere2"}')             # (A choice made before the switch still works.)
        self.assertEqual(W.atlas_world(), 'dev')
        with self.assertRaisesRegex(W.DatabaseError, 'Two worlds are called ridgemere2'):
            W.check_registry([{'key': 'dev', 'database': 'a', 'kind': 'dev', 'aliases': ['ridgemere2']},
                              {'key': 'ridgemere2', 'database': 'b', 'kind': 'dev'}])
        with self.assertRaisesRegex(W.DatabaseError, 'reserved'):
            W.check_registry([{'key': 'dev', 'database': 'a', 'kind': 'dev', 'aliases': ['atlas']}])
        with self.assertRaisesRegex(W.DatabaseError, 'already a world called ridgemere2'):
            W.new_world('ridgemere2', 'Again', log=lambda *_: None)

    def test_without_a_local_choice_atlas_is_dev_and_live_is_prod(self):
        self.assertFalse(self.local.exists())
        self.assertEqual((W.atlas_world(), W.live_world()), ('dev', 'prod'))
        self.assertEqual(W.WORKING, 'dev', 'the old name still answers')
        self.assertEqual((W.resolve('atlas'), W.resolve('live'), W.resolve('dev-old')), ('dev', 'prod', 'dev-old'))
        with self.assertRaisesRegex(W.DatabaseError, 'dev world'):
            W.resolve('prod', 'dev')

    def test_use_chooses_atlass_world_among_dev_worlds_only(self):
        self.assertEqual(self.run_quietly('use', 'dev')[0], 0)
        self.assertEqual(json.loads(self.local.read_text()), {'atlas': 'dev', 'live': 'prod'})
        self.assertEqual(W.atlas_world(), 'dev')
        status, _, err = self.run_quietly('use', 'prod')
        self.assertEqual(status, 1)
        self.assertIn('dev world', err)
        self.assertEqual(W.atlas_world(), 'dev', 'refused: nothing changed')
        self.assertEqual(self.run_quietly('use', 'nowhere')[0], 1)

    def test_serve_world_needs_yes_and_a_live_world(self):
        status, _, err = self.run_quietly('serve-world', 'prod')
        self.assertEqual(status, 1)
        self.assertIn('rarely wise', err)
        self.assertFalse(self.local.exists(), 'without --yes nothing is written')
        self.assertEqual(self.run_quietly('serve-world', 'dev', '--yes')[0], 1, 'a dev world is never the live one')
        self.assertFalse(self.local.exists())
        # (Switching, refused outside maintenance: NewWorldTests, on throwaway live worlds.)

    def test_a_chosen_world_that_is_gone_is_reported_not_swapped(self):
        self.local.parent.mkdir(parents=True)
        self.local.write_text('{"atlas": "vanished"}')
        with self.assertRaisesRegex(W.DatabaseError, 'vanished.*world_db.py use'):
            W.atlas_world()
        self.assertEqual(W.live_world(), 'prod')

    def test_registering_a_world_rewrites_the_file_and_the_map(self):
        W.register({'key': 'side', 'name': 'Side project', 'database': 'ratw_side', 'kind': 'dev'})
        self.assertIn('side', W.DATABASES)
        lines = self.registry.read_text().splitlines()
        self.assertEqual(lines[0], '{"worlds": [')
        self.assertEqual(len(lines), 6, 'one world a line')
        self.assertEqual(W.world('side')['database'], 'ratw_side')
        status, out, _ = self.run_quietly('list')
        self.assertEqual(status, 0)
        self.assertIn('Side project', out)
        self.assertIn('[atlas]', out)

    def test_the_registry_is_checked(self):
        good = W.worlds()
        bad = [
            good + [dict(good[0])],                                                          # A key twice.
            good + [{'key': 'x', 'name': 'X', 'database': 'ratw_prod', 'kind': 'dev'}],      # A database twice.
            good + [{'key': 'x', 'name': 'X', 'database': 'ratw_x', 'kind': 'staging'}],
            good + [{'key': 'x', 'name': 'X', 'database': 'ratw_x', 'kind': 'dev', 'upstream': 'dev'}],   # Not a live one.
            good + [{'key': 'x', 'name': 'X', 'database': 'ratw_x', 'kind': 'live', 'upstream': 'prod'}],
            good + [{'key': 'atlas', 'name': 'X', 'database': 'ratw_x', 'kind': 'dev'}],     # Reserved.
            good + [{'key': 'Big World', 'name': 'X', 'database': 'ratw_x', 'kind': 'dev'}],
            [],
        ]
        for entries in bad:
            with self.assertRaises(W.DatabaseError, msg=entries[-1:] if entries else 'empty'):
                W.check_registry(entries)

    def test_push_to_live_is_from_the_dev_world_with_an_upstream(self):
        self.assertEqual(W.push_worlds(), ('dev', 'prod'))
        W.register({'key': 'copy', 'name': 'Copy', 'database': 'ratw_copy_test', 'kind': 'dev', 'upstream': 'prod'})
        self.assertEqual(W.push_worlds(), ('dev', 'prod'), 'the first, while Atlas is on a side project')
        W.set_local(atlas='copy')
        self.assertEqual(W.push_worlds(), ('copy', 'prod'), "Atlas's own, when it has an upstream")

    def test_connection_settings_follow_the_registry(self):
        os.environ['RATW_GAME_PASSWORD'] = 'x'
        self.assertEqual(W.conninfo('ridgemere2', 'game')['dbname'], 'ratw_ridgemere2')
        self.assertIn("dbname='ratw_ridgemere2'", W.libpq_conninfo('dev', 'game'))
        self.assertIn("dbname='ratw_dev'", W.libpq_conninfo('dev-old', 'game'))
        os.environ['RATW_DEV_OLD_PORT'] = '6000'                     # (A '-' in a key is '_' in its settings' names.)
        self.assertEqual(W.conninfo('dev-old', 'game')['port'], '6000')
        os.environ['RATW_DEV_DBNAME'] = 'ratw_elsewhere'              # (An alias's settings are its world's.)
        self.assertEqual(W.conninfo('ridgemere2', 'game')['dbname'], 'ratw_elsewhere')
        del os.environ['RATW_DEV_DBNAME']
        self.assertEqual(self.run_quietly('conninfo', 'atlas')[0], 0)
        self.assertIn("dbname='ratw_ridgemere2'", self.run_quietly('conninfo', 'atlas')[1])
        with self.assertRaisesRegex(W.DatabaseError, 'Unknown world'):
            W.conninfo('nowhere', 'game')


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class NewWorldTests(ScratchRegistry, unittest.TestCase):
    """`new`, against throwaway databases (ratw_t...), dropped afterwards. DEV and PROD are never touched."""

    def setUp(self):
        super().setUp()
        self.tag = secrets.token_hex(3)
        self.made = []

    def tearDown(self):
        with superuser() as su:
            for database in self.made:
                su.execute(f'DROP DATABASE IF EXISTS {database} WITH (FORCE)')
        super().tearDown()

    def new(self, key, *args, **kwargs):
        self.made.append(f'ratw_{key}')
        return W.new_world(key, *args, log=lambda *_: None, **kwargs)

    def owner(self, key):
        return W.connect(key, 'owner')

    def can_connect(self, key, role):
        try:
            W.connect(key, role).close()
            return True
        except W.DatabaseError:
            return False

    def test_an_empty_world_has_every_migration_and_is_registered(self):
        key = f't{self.tag}_empty'
        entry = self.new(key, 'Empty', 'dev')
        self.assertEqual(entry['database'], f'ratw_{key}')
        self.assertEqual(W.world(key)['kind'], 'dev')
        self.assertIn(key, json.loads(self.registry.read_text())['worlds'][-1]['key'])
        with self.owner(key) as conn:
            self.assertEqual(set(W.applied_migrations(conn)), {v for v, _, _ in W.migration_files()})
            self.assertEqual(conn.execute('SELECT count(*) FROM world.worlds').fetchone()[0], 0)
        for role in ('editor', 'publisher', 'game', 'dm'):
            self.assertTrue(self.can_connect(key, role), role)
        with superuser() as su:
            acl = su.execute('SELECT datacl::text FROM pg_database WHERE datname = %s', (f'ratw_{key}',)).fetchone()[0]
        self.assertNotIn('=Tc/', acl, 'PUBLIC may not connect')
        with self.assertRaisesRegex(W.DatabaseError, 'already a world'):
            W.new_world(key, 'Again', log=lambda *_: None)

    def test_a_live_world_is_out_of_the_editors_reach_and_has_a_publish_password(self):
        key = f't{self.tag}_live'
        self.new(key, 'Live', 'live')
        self.assertFalse(self.can_connect(key, 'editor'))
        with W.connect(key, 'publisher') as conn:
            self.assertTrue(W.verify_publish_password(conn, W.DEFAULT_PUBLISH_PASSWORD))
        with self.assertRaisesRegex(W.DatabaseError, 'upstream'):
            W.new_world(f't{self.tag}_bad', 'Bad', 'dev', upstream_key='dev', log=lambda *_: None)

    def make_source(self):
        """A small world: a cell with a door into a house, a watch post and a claim on the cell, and two people."""
        key = f't{self.tag}_src'
        self.new(key, 'Source', 'dev')
        with self.owner(key) as conn, conn.transaction():
            conn.execute("INSERT INTO world.worlds (id, name) VALUES ('w', 'Test world')")
            conn.execute("INSERT INTO world.areas (world_id, id, kind) VALUES ('w', 'town', 'cell'), ('w', 'house', 'interior')")
            conn.execute("INSERT INTO world.cells (world_id, id, name, x, y, width, height) VALUES ('w', 'town', 'Town', 0, 0, 32, 24)")
            conn.execute("INSERT INTO world.interiors (world_id, id, name, width, height, glyphs) VALUES ('w', 'house', 'House', 4, 4, %s)",
                         ('.' * 16,))
            conn.execute("UPDATE world.worlds SET spawn_area = 'town', spawn_x = 2, spawn_y = 2, revision = 5")
            conn.execute("INSERT INTO world.terrain_chunks (world_id, cx, cy, size, glyphs) VALUES ('w', 0, 0, 128, %s)",
                         ('.' * 128 * 128,))
            conn.execute("""INSERT INTO world.links (world_id, id, name, kind, a_area, a_x, a_y, b_area, b_x, b_y)
                            VALUES ('w', 'door', 'Door', 'door', 'town', 3, 3, 'house', 1, 1)""")
            conn.execute("INSERT INTO live.patrol_routes (world_id, id, name) VALUES ('w', 'watch', 'Watch')")
            conn.execute("INSERT INTO live.patrol_posts (world_id, route_id, seq, area, x, y) VALUES ('w', 'watch', 0, 'town', 1, 1)")
            conn.execute("INSERT INTO live.factions (world_id, id, name) VALUES ('w', 'guild', 'Guild')")
            conn.execute("INSERT INTO live.faction_claims (world_id, faction_id, area) VALUES ('w', 'guild', 'town')")
            conn.execute("INSERT INTO live.npc_areas (world_id, id, name, kind, area) VALUES ('w', 'square', 'Square', 'wander', 'town')")
            for npc, work, wander in (('wren', 'town', 'square'), ('ada', 'house', None)):
                conn.execute("""INSERT INTO live.npcs (world_id, id, name, role, age, voice, appearance, hours_start, hours_end,
                                    home_area, home_x, home_y, work_area, work_x, work_y, evening_area, evening_x, evening_y, wander_area)
                                VALUES ('w', %s, %s, 'civilian', 30, 1, '{}', 8, 17, 'house', 1, 1, %s, 2, 2, 'town', 3, 3, %s)""",
                             (npc, npc.title(), work, wander))
            conn.execute("INSERT INTO live.economy (world_id, treasury) VALUES ('w', 50)")
        return key

    def test_a_copy_without_its_map_keeps_the_people_and_the_buildings(self):
        source = self.make_source()
        key = f't{self.tag}_bare'
        saved = Path(self.folder.name) / 'placements.json'
        self.new(key, 'Bare', 'dev', source=source, without_map=True, placements=saved)
        holding = f'{key}_unplaced'
        with self.owner(key) as conn:
            count = lambda sql: conn.execute(sql).fetchone()[0]
            self.assertEqual(count("SELECT count(*) FROM world.areas WHERE kind = 'cell'"), 0)
            self.assertEqual(count("SELECT count(*) FROM world.interiors WHERE id = 'house'"), 1, 'the buildings stay')
            for table in ('world.links', 'world.terrain_chunks', 'live.patrol_posts', 'live.faction_claims', 'live.economy',
                          'live.npc_areas'):
                self.assertEqual(count(f'SELECT count(*) FROM {table}'), 0, table)
            self.assertEqual(count('SELECT count(*) FROM live.patrol_routes'), 1, 'routes stay, without posts')
            self.assertEqual(conn.execute('SELECT spawn_area, revision FROM world.worlds').fetchone(), (None, 6))
            people = {r[0]: r[1:] for r in conn.execute(
                'SELECT id, home_area, work_area, evening_area, wander_area FROM live.npcs ORDER BY id').fetchall()}
            nullable = W._nullable(conn, 'live.npcs', 'home_area')
            unset = None if nullable else holding
            self.assertEqual(people, {'ada': ('house', 'house', unset, None), 'wren': ('house', unset, unset, None)})
            self.assertEqual(count(f"SELECT count(*) FROM world.interiors WHERE id = '{holding}'"), 0 if nullable else 1)
        with self.owner(source) as conn:
            self.assertEqual(conn.execute("SELECT count(*) FROM world.areas WHERE kind = 'cell'").fetchone()[0], 1,
                             'the world copied from is untouched')
        placements = json.loads(saved.read_text())
        self.assertEqual([c['id'] for c in placements['cells']], ['town'])
        self.assertEqual({n['id']: n['work_area'] for n in placements['npcs']}, {'ada': 'house', 'wren': 'town'})
        self.assertEqual([d['id'] for d in placements['doors']], ['door'])
        self.assertEqual(placements['patrol_posts'][0]['route'], 'watch')
        self.assertEqual(placements['faction_claims'][0]['faction'], 'guild')
        self.assertEqual(placements['spawn'], ['town', 2, 2])
        self.assertEqual([(a['id'], a['area']) for a in placements['npc_areas']], [('square', 'town')])
        self.assertEqual({n['id']: n['wander_area'] for n in placements['npcs']}, {'ada': None, 'wren': 'square'})
        import world_sync
        with self.owner(key) as conn, self.owner(source) as was:
            mine, theirs = world_sync.map_identity(conn, 'dev'), world_sync.map_identity(was, 'dev')
        self.assertEqual((mine['from'], mine['how']), ('stamp', 'without-map'), 'a map of its own: none')
        self.assertNotEqual(mine['id'], theirs['id'], 'not the map it was copied with')

    def test_a_plain_copy_is_the_same_world(self):
        source = self.make_source()
        key = f't{self.tag}_copy'
        self.new(key, 'Copy', 'dev', source=source)
        with self.owner(key) as conn:
            self.assertEqual(conn.execute("SELECT count(*) FROM world.areas").fetchone()[0], 2)
            self.assertEqual(conn.execute("SELECT count(*) FROM live.npcs").fetchone()[0], 2)
            import world_sync
            with self.owner(source) as was:
                self.assertEqual(world_sync.map_identity(conn, 'dev'), world_sync.map_identity(was, 'dev'), 'on the same map')

    def test_a_copy_is_refused_while_someone_uses_the_world(self):
        source = self.make_source()
        key = f't{self.tag}_busy'
        with self.owner(source):
            with self.assertRaisesRegex(W.DatabaseError, 'other session'):
                self.new(key, 'Busy', 'dev', source=source)
        self.assertNotIn(key, W.keys())
        with superuser() as su:
            self.assertIsNone(su.execute('SELECT 1 FROM pg_database WHERE datname = %s', (f'ratw_{key}',)).fetchone())

    def test_switching_the_live_world_needs_maintenance(self):
        import world_sync
        one, two = f't{self.tag}_one', f't{self.tag}_two'
        self.new(one, 'One', 'live')
        self.new(two, 'Two', 'live')
        W.set_local(live=one)
        status, _, err = self.run_quietly('serve-world', two, '--yes')
        self.assertEqual(status, 1)
        self.assertIn('not in maintenance', err)
        self.assertEqual(W.live_world(), one, 'refused: nothing changed')
        world_sync.maintenance(one, True, 'Ada')
        status, _, err = self.run_quietly('serve-world', two, '--yes')
        self.assertEqual(status, 0, err)
        self.assertIn('rarely wise', err, 'the warning is given even when it goes ahead')
        self.assertEqual(W.live_world(), two)
        self.assertEqual(world_sync.status(two)['server']['state'], 'maintenance', 'the new one waits until the DM opens it')
        self.assertEqual(W.atlas_world(), 'dev', 'the other choice is kept')

    def test_without_map_needs_a_world_to_copy(self):
        with self.assertRaisesRegex(W.DatabaseError, '--from'):
            W.new_world(f't{self.tag}_none', 'None', without_map=True, log=lambda *_: None)


if __name__ == '__main__':
    unittest.main()
