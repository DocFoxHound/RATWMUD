#!/usr/bin/env python3
"""World database: migrations, schema rules, role permissions and the publish password.

Database tests run against the local PostgreSQL (`python3 tools/world_db.py up`)
in a scratch database that is dropped afterwards; they are skipped when it is
not running. DEV and PROD are never touched, except to check who may connect.
"""
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


if __name__ == '__main__':
    unittest.main()
