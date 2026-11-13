#!/usr/bin/env python3
"""The Editor's helper (tools/world_admin.py; Docs/Design/78-hosts-run-their-own-worlds.md, sections 5, 6 and 18;
Phase M2a): migration 0063 and rule I11, the password rules, the certificate and its signed rotation, Editor codes,
the TLS 1.3 port with the browser rule and the connection limits, signing in with its lanes and counters, sessions,
the account routes, the logs, and ratw-editor. The database parts use a throwaway database, dropped afterwards, and
are skipped without the local PostgreSQL; the certificate parts need `openssl`."""
import asyncio
import base64
import contextlib
import io
import json
import os
from pathlib import Path
import secrets
import shutil
import signal
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

import dungeon_master as D
import world_admin as A
import world_db as W
from test_world_db import database_available, superuser

# A throwaway database's name tag (RATW_TEST_DB_TAG, default ''): a run tagged s2stage_ makes ratw_test_s2stage_*.
DB_TAG = os.environ.get('RATW_TEST_DB_TAG', '')
TOOLS = Path(__file__).resolve().parent
OPENSSL = shutil.which('openssl')
DB = database_available()
SCRATCH = None
WORLD_NAME = 'Ashfall Vale'
GOOD = 'tallow-kettle-91-orchard'          # A password that passes the rules.

# The game server keeps its grant only on what it uses, and on the legacy Storykeeper's own tables (doc 78, I11).
GAME_TABLES = {'actions', 'health', 'watch', 'watchers', 'meta', 'documents', 'commands', 'activity', 'routes',
               'arrivals', 'storykeeper_audit'}
TRUSTED = ('admins', 'sessions', 'devices', 'invites', 'locks', 'ai_use', 'audit', 'stories')


def setUpModule():
    global SCRATCH
    if not DB:
        return
    W.ensure_roles()
    SCRATCH = f'ratw_test_{DB_TAG}editor_{secrets.token_hex(4)}'
    with superuser() as su:
        su.execute(f'CREATE DATABASE {SCRATCH} OWNER ratw_owner')
        su.execute(f'GRANT CONNECT ON DATABASE {SCRATCH} TO ratw_publisher, ratw_game, ratw_dm')
    with W.connect('dev', 'owner', dbname=SCRATCH) as owner:
        W.migrate(owner)


def tearDownModule():
    if SCRATCH:
        with superuser() as su:
            su.execute(f'DROP DATABASE IF EXISTS {SCRATCH} WITH (FORCE)')


def connect(role='dm'):
    return W.connect('dev', role, dbname=SCRATCH)


def owner_sql(sql, params=None):
    with connect('owner') as conn:
        cursor = conn.execute(sql, params)
        return cursor.fetchall() if cursor.description else None


def empty_accounts():
    owner_sql('TRUNCATE dm.admins, dm.sessions, dm.devices, dm.invites, dm.locks, dm.audit CASCADE')


def run(coroutine):
    return asyncio.run(coroutine)


class Folder:
    """An admin directory with a certificate and a settings copy."""

    def __init__(self, **editor):
        self.temp = tempfile.TemporaryDirectory(prefix='ratw-editor-test-')
        self.path = Path(self.temp.name)
        A.make_certificate(self.path)
        self.settings(**editor)

    def settings(self, **editor):
        (self.path / 'settings.json').write_text(json.dumps({'name': WORLD_NAME, 'package': 'test-1', 'editor': editor}))

    def cleanup(self):
        self.temp.cleanup()


class Captured:
    """The helper's log lines, while a test runs."""

    def __init__(self):
        self.lines = []

    def __enter__(self):
        self.patch = mock.patch.object(A, 'LOG', self.lines.append)
        self.patch.start()
        return self

    def __exit__(self, *exc):
        self.patch.stop()

    def text(self):
        return '\n'.join(self.lines)


# ------------------------------------------------------------------------------------------------------- no database

class PasswordRules(unittest.TestCase):
    def test_the_bundled_list_is_the_100000_and_under_2_mb(self):
        self.assertGreater(len(A.breached()), 90000)
        self.assertLess(A.BREACHED_FILE.stat().st_size, 2 * 1024 * 1024)
        self.assertIn('password1234', A.breached())

    def test_each_rule(self):
        cases = {'short-one': 'short', 'x' * 300: 'long', 'pell-likes-orchards-9': 'username',
                 'Pell_likes_orchards': 'username', 'my-ashfall-vale-rocks': 'world', 'xAshFallVale2024': 'world',
                 'password1234': 'common', 'Sunshine2024!!': 'common', '!!iloveyou12345': 'common',
                 'monkeymonkeymonkey': 'common', 'aaaaaaaaaaaaab': 'simple', '121212121212': 'simple'}
        for password, problem in cases.items():
            self.assertEqual(A.password_problem(password, 'pell', WORLD_NAME), problem, password)
        for password in (GOOD, 'correct horse battery staple', 'Grey wolves sing at 3am!'):
            self.assertIsNone(A.password_problem(password, 'pell', WORLD_NAME), password)

    def test_each_word_of_the_world_s_name_of_four_letters_or_more(self):
        """R1: the whole name squashed wasn't enough ("ashfall1234567" passed for "Ashfall Test")."""
        for password in ('ashfall1234567', 'my-TEST-orchard-91', 'kettle.Ashfall.91'):
            self.assertEqual(A.password_problem(password, 'pell', 'Ashfall Test'), 'world', password)
        self.assertIsNone(A.password_problem('ox-tallow-kettle-91', 'pell', 'Ox and Elm'), 'words under 4 letters are no rule')

    def test_a_short_world_name_is_not_a_rule(self):
        self.assertIsNone(A.password_problem('ox-tallow-kettle-91', 'pell', 'Ox'))

    def test_odd_records_still_run_scrypt_and_fail(self):
        calls = []
        real = W.password_matches
        with mock.patch.object(A.world_db, 'password_matches', side_effect=lambda r, p: calls.append(r) or real(r, p)):
            self.assertFalse(A.check_password(None, 'x'))
            self.assertFalse(A.check_password({'algorithm': 'scrypt', 'n': 2 ** 30, 'r': 8, 'p': 1, 'salt': '', 'hash': ''}, 'x'))
            self.assertFalse(A.check_password(W.hash_password('right-password-1'), 'x' * 2000))
        self.assertEqual(calls, [A.DUMMY_RECORD] * 3, 'each odd case checks the dummy hash once')


class Locks(unittest.TestCase):
    def test_five_failures_then_waits_that_double(self):
        state, now = {}, 1000.0
        for _ in range(4):
            A.next_lock(state, now, 900, 3600)
        self.assertFalse(state.get('until'))
        A.next_lock(state, now, 900, 3600)
        self.assertEqual(state['until'], now + 900)
        waits = []
        for round_ in range(4):
            now = state['until'] + 1
            for _ in range(5):
                A.next_lock(state, now, 900, 3600)
            waits.append(state['until'] - now)
        self.assertEqual(waits, [1800, 3600, 3600, 3600], 'doubling up to the most')
        account = {}
        for _ in range(30):
            A.next_lock(account, 0, 900, 4 * 3600)
        self.assertEqual(account['until'], 4 * 3600)

    def test_failures_outside_the_window_start_again_and_a_quiet_day_forgets(self):
        state = {}
        for at in (0, 100, 200, 300):
            A.next_lock(state, at, 900, 3600)
        A.next_lock(state, 300 + A.WINDOW + 1, 900, 3600)
        self.assertEqual(state['failures'], 1)
        self.assertFalse(state.get('until'))
        state = {'failures': 0, 'level': 3, 'until': 1000, 'first': 0}
        for _ in range(5):
            A.next_lock(state, 1000 + 25 * 3600, 900, 3600)
        self.assertEqual(state['until'] - (1000 + 25 * 3600), 900)


class PeerKeys(unittest.TestCase):
    def test_ipv6_by_64_and_mapped_ipv4(self):
        peers = A.Peers('real', unidentified=[])
        self.assertEqual(peers.key('2001:db8:1:2::5'), peers.key('2001:db8:1:2:ffff::9'))
        self.assertNotEqual(peers.key('2001:db8:1:2::5'), peers.key('2001:db8:1:3::5'))
        self.assertEqual(peers.key('::ffff:203.0.113.7'), '203.0.113.7')
        self.assertIsNone(peers.key('not an address'))

    def test_gateway_and_loopback_are_unidentified_in_auto_and_all_are_when_folded(self):
        auto = A.Peers('auto', unidentified=['172.17.0.1'])
        self.assertIsNone(auto.key('172.17.0.1'), "Docker's gateway: the world's own computer, or anyone (M0a)")
        self.assertIsNone(auto.key('127.0.0.1'))
        self.assertEqual(auto.key('192.168.1.20'), '192.168.1.20')
        self.assertIsNone(A.Peers('folded').key('192.168.1.20'))
        self.assertEqual(A.Peers('real', unidentified=[]).key('127.0.0.1'), '127.0.0.1')


class Gate(unittest.TestCase):
    def setUp(self):
        import concurrent.futures
        self.executor = concurrent.futures.ThreadPoolExecutor(2)
        self.release = threading.Event()
        self.order = []

    def tearDown(self):
        self.release.set()
        self.executor.shutdown()

    def work(self, name):
        def inner():
            self.release.wait(5)
            self.order.append(name)
            return name
        return inner

    def test_two_at_once_and_eight_waiting(self):
        async def scenario():
            gate = A.ScryptGate(self.executor, slots=2, waiting=8)
            running = [asyncio.create_task(gate.run(1, self.work(f's{i}'))) for i in range(2)]
            await asyncio.sleep(0.05)
            self.assertEqual(gate.active, 2)
            third = asyncio.create_task(gate.run(1, self.work('third')))
            await asyncio.sleep(0.05)
            self.assertFalse(third.done(), 'a third check waits')
            waiting = [asyncio.create_task(gate.run(1, self.work(f'w{i}'))) for i in range(7)]
            await asyncio.sleep(0.05)
            with self.assertRaises(A.EditorError) as busy:
                await gate.run(1, self.work('tenth'))
            self.assertEqual((busy.exception.status, busy.exception.code), (503, 'busy'))
            known = asyncio.create_task(gate.run(0, self.work('device')))   # Takes the newest stranger's place.
            await asyncio.sleep(0.05)
            self.release.set()
            results = await asyncio.gather(*running, third, *waiting, known, return_exceptions=True)
            self.assertIsInstance(results[9], A.EditorError, 'the newest stranger gave way to the known device')
            self.assertEqual(sum(isinstance(r, A.EditorError) for r in results), 1)
            self.assertEqual(results[10], 'device')
            self.assertEqual(gate.active, 0)
        asyncio.run(scenario())

    def test_known_devices_and_codes_go_first(self):
        async def scenario():
            gate = A.ScryptGate(self.executor, slots=1, waiting=8)
            holding = asyncio.create_task(gate.run(1, self.work('holding')))
            await asyncio.sleep(0.05)
            queued = [asyncio.create_task(gate.run(1, self.work(f'stranger{i}'))) for i in range(3)]
            await asyncio.sleep(0.02)
            queued.append(asyncio.create_task(gate.run(0, self.work('device'))))
            await asyncio.sleep(0.05)
            self.release.set()
            await asyncio.gather(holding, *queued)
            self.assertEqual(self.order, ['holding', 'device', 'stranger0', 'stranger1', 'stranger2'])
            self.assertEqual(gate.active, 0)
        asyncio.run(scenario())


class ConnectionLimits(unittest.TestCase):
    """Doc 78's R1 review, security 2 and 3: the limits before the handshake, on a clock of the test's own."""

    def setUp(self):
        self.now = [1000.0]
        self.connections = A.Connections(A.Peers('real', unidentified=[]), clock=lambda: self.now[0])

    def test_one_address_s_refused_connections_take_nothing_world_wide(self):
        admitted = [self.connections.admit('198.51.100.7') for _ in range(200)]
        self.assertEqual(sum(1 for a in admitted if a), 4, 'four at once from one address')
        others = [self.connections.admit(f'203.0.113.{i}') for i in range(1, 10)]
        self.assertTrue(all(others), 'nobody else is locked out by them')

    def test_at_the_cap_the_oldest_idle_connection_without_a_session_gives_way(self):
        aborted = []
        slots = []
        for i in range(8):
            for _ in range(4):
                slot = self.connections.admit(f'198.51.100.{i}')
                slot.abort = (lambda s: lambda: aborted.append(s))(slot)
                slots.append(slot)
                self.now[0] += 1
        self.assertEqual(self.connections.open_total, 32)
        slots[0].signed = True                # A session's connection never gives way;
        slots[1].busy = True                  # nor one with a request in hand.
        newcomer = self.connections.admit('203.0.113.50')
        self.assertTrue(newcomer, 'the owner gets in past 32 connections held without a session')
        self.assertEqual(aborted, [slots[2]], 'the oldest idle one without a session was closed')
        self.assertEqual(self.connections.open_total, 32)
        self.connections.release(slots[2])    # (Its own close, later: counted once.)
        self.assertEqual(self.connections.open_total, 32)
        for slot in slots + [newcomer]:
            slot.signed = True
        self.assertFalse(self.connections.admit('203.0.113.51'), 'all signed in: refused')
        self.assertEqual(self.connections.open_total, 32)


@unittest.skipUnless(OPENSSL, 'openssl missing')
class Certificates(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='ratw-editor-cert-')
        self.folder = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def openssl_spki(self, cert):
        pem = subprocess.run(['openssl', 'x509', '-in', str(cert), '-pubkey', '-noout'], capture_output=True, check=True).stdout
        return subprocess.run(['openssl', 'pkey', '-pubin', '-outform', 'DER'], input=pem, capture_output=True, check=True).stdout

    def test_made_once_with_a_p256_key_and_the_fingerprint_in_eight_groups(self):
        self.assertTrue(A.make_certificate(self.folder, 'world-' + '0' * 32))
        first = (self.folder / 'editor-cert.pem').read_bytes()
        self.assertFalse(A.make_certificate(self.folder), 'never made twice')
        self.assertEqual(first, (self.folder / 'editor-cert.pem').read_bytes())
        self.assertEqual(A.world_id(self.folder), 'world-' + '0' * 32)
        self.assertEqual(oct((self.folder / 'editor-key.pem').stat().st_mode & 0o777), '0o600')
        spki = A.cert_spki(self.folder / 'editor-cert.pem')
        self.assertEqual(spki, self.openssl_spki(self.folder / 'editor-cert.pem'))
        groups = A.fingerprint(spki).split(' ')
        self.assertEqual(len(groups), 8)
        self.assertTrue(all(len(g) == 4 and g == g.upper() for g in groups))
        text = subprocess.run(['openssl', 'x509', '-in', str(self.folder / 'editor-cert.pem'), '-noout', '-text'],
                              capture_output=True, text=True, check=True).stdout
        self.assertIn('prime256v1', text)
        self.assertIn('RATW Editor world-' + '0' * 32, text)
        self.assertIn(A.fingerprint(spki), A.certificate_text(self.folder, 17789))

    def test_a_world_without_a_central_id_gets_a_local_one(self):
        A.make_certificate(self.folder, 'not-an-id')
        self.assertRegex(A.world_id(self.folder), r'^local-[0-9a-f]{32}$')

    def test_a_new_key_is_signed_by_the_old_one(self):
        A.make_certificate(self.folder)
        old_cert = self.folder / 'old.pem'
        shutil.copy(self.folder / 'editor-cert.pem', old_cert)
        old_spki = A.cert_spki(old_cert)
        done = A.new_certificate(self.folder)
        self.assertTrue(done['signed'])
        new_spki = A.cert_spki(self.folder / 'editor-cert.pem')
        self.assertNotEqual(new_spki, old_spki)
        rotation = json.loads((self.folder / 'rotation.json').read_text())
        self.assertEqual(base64.b64decode(rotation['spki']), new_spki)
        self.assertEqual(rotation['previous'], A.pin(old_spki))
        scratch = self.folder / 'check'
        scratch.mkdir()
        (scratch / 'message').write_bytes(A.rotation_message(A.world_id(self.folder), new_spki))
        (scratch / 'signature').write_bytes(base64.b64decode(rotation['signature']))
        (scratch / 'old.pub').write_bytes(subprocess.run(['openssl', 'x509', '-in', str(old_cert), '-pubkey', '-noout'],
                                                         capture_output=True, check=True).stdout)
        verified = subprocess.run(['openssl', 'dgst', '-sha256', '-verify', str(scratch / 'old.pub'), '-signature',
                                   str(scratch / 'signature'), str(scratch / 'message')], capture_output=True, text=True)
        self.assertEqual(verified.returncode, 0, verified.stdout + verified.stderr)
        (scratch / 'message').write_bytes(A.rotation_message('world-' + 'f' * 32, new_spki))
        forged = subprocess.run(['openssl', 'dgst', '-sha256', '-verify', str(scratch / 'old.pub'), '-signature',
                                 str(scratch / 'signature'), str(scratch / 'message')], capture_output=True)
        self.assertNotEqual(forged.returncode, 0, 'the signature names this world')
        self.assertTrue((self.folder / 'previous-key.pem').exists())

    def test_leaked_means_no_signature(self):
        A.make_certificate(self.folder)
        A.new_certificate(self.folder)
        done = A.new_certificate(self.folder, leaked=True)
        self.assertFalse(done['signed'])
        self.assertFalse((self.folder / 'rotation.json').exists())
        self.assertFalse((self.folder / 'previous-key.pem').exists())

    def test_tls_13_only_with_the_world_key(self):
        A.make_certificate(self.folder)
        context = A.server_context(self.folder)
        self.assertEqual(context.minimum_version, ssl.TLSVersion.TLSv1_3)
        self.assertEqual(context.maximum_version, ssl.TLSVersion.TLSv1_3)


@unittest.skipUnless(OPENSSL, 'openssl missing')
class Codes(unittest.TestCase):
    def test_round_trip_and_refusals(self):
        spki = b'k' * 91
        world = 'world-' + 'a' * 32
        code = A.make_code(world, spki, 'pell', 'one-time-pass', 'invite-secret', '192.168.1.20:17789')
        self.assertTrue(code.startswith('ratw-editor:'))
        record = A.read_code(code)
        self.assertEqual(record, {'v': 1, 'w': world, 'k': A.pin(spki), 'u': 'pell', 'p': 'one-time-pass',
                                  'i': 'invite-secret', 'a': '192.168.1.20:17789'})
        self.assertLess(len(code), 400)
        for bad in ('ratw-editor:', 'pell', 'ratw-editor:' + A.b64url(b'[]'),
                    A.make_code('world-x', spki, 'pell', 'p'), A.make_code(world, spki, 'Pell!', 'p'),
                    A.make_code(world, spki, 'pell')):
            with self.assertRaises(ValueError):
                A.read_code(bad)


class Requests(unittest.TestCase):
    def test_only_known_kinds_and_only_the_kind(self):
        with tempfile.TemporaryDirectory() as temp:
            path = A.request_root('first-editor-used', Path(temp))
            self.assertEqual(path.read_text(), 'first-editor-used\n')
            self.assertEqual(oct(path.stat().st_mode & 0o777), '0o600')
            with self.assertRaises(ValueError):
                A.request_root('../../etc/passwd', Path(temp))
            # A pin names one backup by its time only (doc 78, 12.1; M5).
            self.assertEqual(A.request_root('pin-20261112T140300Z', Path(temp)).read_text(), 'pin-20261112T140300Z\n')
            for odd in ('pin-ratw-20261112T140300Z-daily', 'pin-20261112T140300Z/..', 'unpin-2026', 'pin-'):
                with self.assertRaises(ValueError):
                    A.request_root(odd, Path(temp))

    def test_the_backups_state_is_checked_again(self):
        with tempfile.TemporaryDirectory() as temp:
            folder, now = Path(temp), time.time()
            self.assertIsNone(A.read_backups(folder, now), 'no backup service: nothing said')
            (folder / 'backups.json').write_text(json.dumps({'v': 1, 'at': 5, 'working': 'evil', 'backups': [
                {'name': 'ratw-20261112T140300Z-by-hand', 'at': int(now) - 60, 'size': 9, 'package': '1.2<b>', 'migration': '0064',
                 'pinnedUntil': int(now) + 100},
                {'name': '../../etc/passwd', 'at': 1}, {'name': 'ratw-20261111T140300Z-daily', 'at': 'yesterday'},
                {'name': 'ratw-20261110T140300Z-daily', 'at': 3, 'pinnedUntil': 4}],
                'problem': {'code': '<script>', 'at': 3, 'free': -1}, 'disk': {'free': 10, 'used': 'x'}}))
            got = A.read_backups(folder, now)
            self.assertEqual([b['name'] for b in got['backups']], ['ratw-20261112T140300Z-by-hand', 'ratw-20261110T140300Z-daily'])
            self.assertEqual((got['last']['package'], got['pins'], got['backups'][1]['pinnedUntil'], got['working']),
                             ('1.2b', 1, None, None), 'a pin that ran out is no pin')
            self.assertEqual((got['problem'], got['disk']), ({'code': 'failed', 'at': 3, 'free': None},
                                                             {'free': 10, 'used': None, 'minFree': None}))


# ------------------------------------------------------------------------------------------- migration 0063 and I11

class Dropping(unittest.TestCase):
    def test_as_root_ratw_editor_always_drops_to_the_helpers_user(self):
        # R1 review: RATW_EDITOR_DROPPED=1 in the environment skipped the drop, so `exec -e` ran it as root.
        with mock.patch.object(A.os, 'geteuid', return_value=0), mock.patch.dict(os.environ, {'RATW_EDITOR_DROPPED': '1'}), \
                mock.patch('pwd.getpwnam', side_effect=KeyError('ratw-admin')), contextlib.redirect_stderr(io.StringIO()) as said:
            self.assertEqual(A._as_admin(['list']), 2, 'refused as root, never run as it')
        self.assertIn('refusing to run as root', said.getvalue())
        with mock.patch.object(A.os, 'geteuid', return_value=10003):
            self.assertIsNone(A._as_admin(['list']), 'already the helper: runs here')


@unittest.skipUnless(DB, 'local PostgreSQL not running')
class Migration(unittest.TestCase):
    def test_the_game_role_holds_only_what_the_server_uses(self):
        rows = owner_sql("""SELECT c.relname FROM pg_class c JOIN pg_namespace n ON n.oid = c.relnamespace
                            WHERE n.nspname = 'dm' AND c.relkind IN ('r', 'p', 'v')
                              AND has_table_privilege('ratw_game', c.oid, 'SELECT, INSERT, UPDATE, DELETE, TRUNCATE')""")
        self.assertEqual({r[0] for r in rows}, GAME_TABLES,
                         'a dm.* table open to ratw_game: revoke it in its migration, or name it here if the server uses it')

    def test_a_new_dm_table_starts_closed_to_the_game(self):
        with connect('owner') as conn:
            with self.assertRaises(RuntimeError), conn.transaction():
                conn.execute('CREATE TABLE dm.zz_probe (id int)')
                opened = conn.execute("SELECT has_table_privilege('ratw_game', 'dm.zz_probe', 'SELECT')").fetchone()[0]
                dm_may = conn.execute("SELECT has_table_privilege('ratw_dm', 'dm.zz_probe', 'SELECT, INSERT')").fetchone()[0]
                raise RuntimeError('roll back')
        self.assertFalse(opened)
        self.assertTrue(dm_may, 'the DM tool keeps its own schema')

    def test_the_game_server_cannot_read_or_forge_anything_the_helper_trusts(self):
        import psycopg
        with connect('game') as game:
            for table in TRUSTED:
                with self.assertRaises(psycopg.errors.InsufficientPrivilege, msg=table):
                    game.execute(f'SELECT 1 FROM dm.{table} LIMIT 1')
            with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                game.execute("INSERT INTO dm.sessions (token_hash, username, expires_at) VALUES ('x', 'host-k7q2', now())")
            with self.assertRaises(psycopg.errors.InsufficientPrivilege):
                game.execute("INSERT INTO dm.audit (at, action, target, detail) VALUES (0, 'forged', '', '')")
            game.execute("INSERT INTO dm.storykeeper_audit (at, action, target, detail) VALUES (0, 'event.x', '', '')")
            game.execute('SELECT 1 FROM dm.actions LIMIT 1')
        # Migration 0064 (R1 review): the game answers an act (status, result, done_at), never makes or rewrites one.
        with connect('owner') as owner:
            act = owner.execute("INSERT INTO dm.actions (kind, target_id, requested_by) VALUES ('npc.kill', 'sloe', 'pell#1a2b3c4d') "
                                'RETURNING id').fetchone()[0]
        try:
            with connect('game') as game:
                game.execute("UPDATE dm.actions SET status = 'applied', result = 'Done.', done_at = now() WHERE id = %s", (act,))
                for sql in ("INSERT INTO dm.actions (kind, target_id, requested_by) VALUES ('npc.kill', 'x', 'forged')",
                            "UPDATE dm.actions SET requested_by = 'someone-else' WHERE id = %s",
                            "UPDATE dm.actions SET kind = 'npc.revive', payload = '{}' WHERE id = %s",
                            'DELETE FROM dm.actions WHERE id = %s'):
                    with self.assertRaises(psycopg.errors.InsufficientPrivilege, msg=sql):
                        with game.transaction():
                            game.execute(sql, (act,) if '%s' in sql else None)
            self.assertEqual(owner_sql('SELECT status, requested_by FROM dm.actions WHERE id = %s', (act,)),
                             [('applied', 'pell#1a2b3c4d')])
        finally:
            owner_sql('DELETE FROM dm.actions WHERE id = %s', (act,))

    def test_roles_widened_and_the_new_columns(self):
        empty_accounts()
        with connect() as conn:
            for role in ('viewer', 'dm', 'builder', 'owner', 'admin'):
                D.create_account(conn, f'r-{role}', role, 'twelve-chars-ok')
            import psycopg
            with self.assertRaises(psycopg.errors.CheckViolation):
                conn.execute("INSERT INTO dm.admins (username, role, password) VALUES ('r-root', 'root', '{}')")
            row = conn.execute("SELECT must_change, must_change_until, may_act_on_wolves, claimed_from, created_by FROM dm.admins WHERE username = 'r-dm'").fetchone()
        self.assertEqual(row, (False, None, False, None, ''), 'old accounts are as they were')
        self.assertEqual(D.RANK, {'viewer': 0, 'dm': 1, 'builder': 2, 'owner': 3, 'admin': 4})
        self.assertLess(D.RANK['owner'], D.RANK[D.ACTIONS['character.dm']], 'only admin gets the Dev Console')


# --------------------------------------------------------------------------------------------------------- sign-in

@unittest.skipUnless(DB and OPENSSL, 'local PostgreSQL or openssl missing')
class SignInBase(unittest.TestCase):
    editor = {}

    def setUp(self):
        empty_accounts()
        self.folder = Folder(**self.editor)
        self.core = self.make_core()

    def make_core(self, **kw):
        return A.Core(connect=lambda: connect(), folder=self.folder.path, peers=A.Peers('real', unidentified=[]), **kw)

    def tearDown(self):
        self.core.close()
        self.folder.cleanup()

    def account(self, name, role='dm', password=GOOD):
        """An account that has chosen its password."""
        with connect() as conn:
            A.add_account(conn, name, role, 'test', roles=A.ROLES)
            conn.execute('UPDATE dm.admins SET password = %s, must_change = false, must_change_until = NULL WHERE username = %s',
                         (W.Jsonb(W.hash_password(password)), name))
            conn.execute('DELETE FROM dm.invites WHERE username = %s', (name,))

    def login(self, username, password=GOOD, address='203.0.113.9', **extra):
        try:
            return run(self.core.login({'username': username, 'password': password, **extra}, address))
        finally:
            self.core.settle()          # (A failure's rows are written after the reply.)

    def refused(self, code, username, password=GOOD, address='203.0.113.9', **extra):
        with self.assertRaises(A.EditorError) as caught:
            self.login(username, password, address, **extra)
        self.assertEqual(caught.exception.code, code, caught.exception.message)
        return caught.exception

    def session(self, token):
        return self.core.session_now(token)


class SigningIn(SignInBase):
    def test_wrong_passwords_get_the_same_words_and_the_same_scrypt_for_real_unknown_and_disabled_names(self):
        self.account('pell')
        self.account('mira')
        with connect() as conn:
            A.set_disabled(conn, 'mira', True, 'test')
        self.core.world = A.Bucket(1000, 1000, time.time)          # (Not the world-wide limit's test.)
        real = W.password_matches
        timings = {}
        for name in ('pell', 'nobody-here', 'mira', 'Not A Name!'):
            calls = []
            with mock.patch.object(A.world_db, 'password_matches', side_effect=lambda r, p: calls.append(1) or real(r, p)):
                times = []
                for i in range(3):
                    started = time.perf_counter()
                    error = self.refused('wrong', name, 'not-the-password-1', address=f'198.51.100.{len(timings) * 10 + i}')
                    times.append(time.perf_counter() - started)
            self.assertEqual(error.message, 'Wrong name or password.')
            self.assertEqual(len(calls), 3, f'{name}: one scrypt a try')
            timings[name] = sorted(times)[1]
        self.assertLess(max(timings.values()) / min(timings.values()), 2.5, timings)

    def test_a_disabled_account_with_its_right_password_is_told_and_an_admin_row_never_signs_in(self):
        self.account('mira')
        with connect() as conn:
            A.set_disabled(conn, 'mira', True, 'test')
            D.create_account(conn, 'dm-admin', 'admin', GOOD)
        self.refused('disabled', 'mira')
        self.refused('wrong', 'dm-admin')

    def test_the_host_token_opens_nothing(self):
        self.account('pell')
        self.refused('wrong', 'pell', 'rh_' + secrets.token_urlsafe(24))
        self.assertIsNone(self.session('rh_' + secrets.token_urlsafe(24)))

    def test_a_good_sign_in_gives_a_session_and_a_device(self):
        self.account('pell', 'builder')
        signed = self.login('pell')
        self.assertEqual((signed['username'], signed['role'], signed['mustChange']), ('pell', 'builder', False))
        self.assertTrue(signed['device'])
        session = self.session(signed['token'])
        self.assertEqual(session['account'], 'pell')
        self.assertRegex(session['username'], r'^pell#[0-9a-f]{6}$', 'requested_by and the audit name the session')
        stored = owner_sql('SELECT token_hash, device, address FROM dm.sessions')
        self.assertEqual(stored, [(A.token_hash(signed['token']), A.token_hash(signed['device']), '203.0.113.9')])
        who = owner_sql("SELECT who FROM dm.audit WHERE action = 'login'")[0][0]
        self.assertEqual(who, session['username'])

    def test_bad_bodies(self):
        for body in ({}, {'username': 'pell'}, {'username': 'pell', 'password': ''}, {'username': 3, 'password': 'x'},
                     {'username': 'pell', 'password': 'x', 'invite': 5}, {'username': 'pell', 'password': 'x', 'device': []}):
            with self.assertRaises(A.EditorError) as caught:
                run(self.core.login(body, '203.0.113.9'))
            self.assertEqual(caught.exception.status, 400)

    def test_before_the_database_answers_hello_says_setting_up_and_sign_in_waits(self):
        def down():
            raise W.DatabaseError('not yet')
        early = A.Core(connect=down, folder=self.folder.path, peers=A.Peers('real', unidentified=[]))
        try:
            hello = early.hello_now()
            self.assertEqual((hello['progress'], hello['serverState'], hello['setupPending']),
                             ({'code': 'database', 'percent': 0}, 'unknown', False))
            self.assertEqual(hello['world'], A.world_id(self.folder.path))
            with self.assertRaises(A.EditorError) as caught:
                run(early.login({'username': 'pell', 'password': GOOD}, '203.0.113.9'))
            self.assertEqual(caught.exception.code, 'setup')
        finally:
            early.close()

    def test_official_and_still_setting_up(self):
        self.account('pell')
        (self.folder.path / 'progress.json').write_text(json.dumps({'code': 'importing', 'percent': 40}))
        self.refused('setup', 'pell')
        self.assertEqual(self.core.hello_now()['progress'], {'code': 'importing', 'percent': 40})
        (self.folder.path / 'progress.json').write_text(json.dumps({'code': 'ready'}))
        self.core._hello = (0.0, None)
        self.login('pell')
        (self.folder.path / 'ever-official').write_text('')
        self.refused('official', 'pell')
        self.core._hello = (0.0, None)
        self.assertEqual(self.core.hello_now()['kind'], 'official')


@contextlib.contextmanager
def http_status(body: dict):
    """A stand-in game server's /status on loopback: its port."""
    import http.server

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            out = json.dumps(body).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(out)))
            self.end_headers()
            self.wfile.write(out)

        def log_message(self, *_):
            pass
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server.server_port
    finally:
        server.shutdown()
        server.server_close()


class HelloServer(SignInBase):
    """R1, H2 and M-a: the game server's state from the entrypoint's server.json, and a database that doesn't answer."""

    def write(self, **state):
        (self.folder.path / 'server.json').write_text(json.dumps(state))
        self.core._hello = (0.0, None)

    def test_the_server_s_state_checked_from_server_json(self):
        self.assertNotIn('server', self.core.hello_now(), 'none written: none said')
        now = time.time()
        self.write(up=False, reason='exit', code=-11, failures=3, retryAt=now + 120, at=now)
        self.assertEqual(self.core.hello_now()['server'],
                         {'up': False, 'reason': 'exit', 'code': 139, 'failures': 3, 'retryIn': 120})
        self.write(up=True, reason='running', code=None, failures=0, retryAt=None)
        self.assertEqual(self.core.hello_now()['server'],
                         {'up': True, 'reason': 'running', 'code': None, 'failures': 0, 'retryIn': None})
        self.write(up='yes', reason='world.toml', code='x', failures=500, retryAt=now + 10 ** 9)
        self.assertEqual(self.core.hello_now()['server'],
                         {'up': False, 'reason': 'world.toml', 'code': None, 'failures': 99, 'retryIn': 86400})
        self.write(up=True, reason='<b>exploded</b>')
        self.assertNotIn('server', self.core.hello_now())
        self.write(up=False, reason='closed', code=76, failures=0, retryAt=None)
        self.assertLessEqual(len(json.dumps(self.core.hello_now())), A.HELLO_MOST)

    def test_the_menu_s_wolves_and_last_backup(self):
        """Section 2's "Open · 7 wolves" and "Last backup 3 h ago" (M5): the game's own public count on loopback, and the
        newest backup's age; neither while unknown, and no wolves asked of a server that is down."""
        hello = self.core.hello_now()
        self.assertEqual((hello['wolves'], hello['lastBackup']), (None, None))
        (self.folder.path / 'backups.json').write_text(json.dumps({'v': 1, 'at': 1, 'backups': [
            {'name': 'ratw-20261112T140300Z-daily', 'at': int(time.time()) - 3 * 3600}]}))
        with http_status({'ratw': 'world', 'players': 7}) as port, mock.patch.dict(os.environ, {'RATW_PORT': str(port)}):
            A._WOLVES['at'] = float('-inf')
            self.write(up=True, reason='running', code=None, failures=0, retryAt=None)
            hello = self.core.hello_now()
            self.assertEqual(hello['wolves'], 7)
            self.assertAlmostEqual(hello['lastBackup'], 3 * 3600, delta=5)
            A._WOLVES['at'] = float('-inf')
            self.write(up=False, reason='exit', code=11, failures=1, retryAt=None)
            self.assertIsNone(self.core.hello_now()['wolves'], 'down: not asked')
        self.assertLessEqual(len(json.dumps(self.core.hello_now())), A.HELLO_MOST)

    def test_a_database_that_doesnt_answer_gets_a_hello_in_two_seconds(self):
        def stalled():
            time.sleep(3)
            raise W.DatabaseError('stalled')
        for progress, expected in ((None, {'code': 'database', 'percent': 0}),       # (Never set up: still its database.)
                                   ({'code': 'ready'}, {'code': 'slow', 'percent': 100})):
            if progress:
                (self.folder.path / 'progress.json').write_text(json.dumps(progress))
            slow = A.Core(connect=stalled, folder=self.folder.path, peers=A.Peers('real', unidentified=[]))
            try:
                started = time.monotonic()
                hello = run(slow.hello())
                self.assertLess(time.monotonic() - started, A.HELLO_WAIT + 0.8)
                self.assertEqual((hello['progress'], hello['serverState']), (expected, 'unknown'))
            finally:
                slow.close()

    def test_the_helper_s_connections_time_out(self):
        self.assertEqual(A.DB_OPTIONS['options'], '-c statement_timeout=5000')
        self.assertEqual(A.DB_OPTIONS['keepalives'], '1')
        with mock.patch.object(W, 'connect', side_effect=lambda *a, **k: k) as made:
            core = A.Core(folder=self.folder.path, peers=A.Peers('real', unidentified=[]))
            try:
                self.assertEqual(core.pool._open(), A.DB_OPTIONS)
            finally:
                core.close()
        self.assertEqual(made.call_args.args, (A.WORLD_KEY, 'dm'))


class OneTimePasswords(SignInBase):
    def test_the_first_owner_its_one_time_password_its_change_and_first_editor_used(self):
        with connect() as conn:
            made = A.ensure_first_owner(conn)
            self.assertIsNone(A.ensure_first_owner(conn), 'never a second owner')
        self.assertRegex(made['username'], r'^host-[a-z0-9]{4}$')
        days = owner_sql("SELECT extract(epoch FROM must_change_until - now()) / 86400 FROM dm.admins")[0][0]
        self.assertAlmostEqual(float(days), 7, delta=0.01)
        self.assertTrue(self.core.hello_now()['setupPending'])
        signed = self.login(made['username'], made['password'])
        self.assertTrue(signed['mustChange'])
        self.assertIsNone(signed['device'], 'a device comes with the chosen password')
        session = self.session(signed['token'])
        for new, reason in (('password1234', 'common'), ('ashfall-vale-forever', 'world'), ('short', 'short'),
                            (made['username'] + '-and-more', 'username')):
            with self.assertRaises(A.EditorError) as caught:
                run(self.core.change_password(session, {'old': made['password'], 'new': new}))
            self.assertEqual((caught.exception.code, caught.exception.extra), ('weak', {'reason': reason}))
        changed = run(self.core.change_password(session, {'old': made['password'], 'new': GOOD}))
        self.assertTrue(changed['device'])
        self.assertEqual([p.name.split('.')[0] for p in (self.folder.path / 'requests').iterdir()], ['first-editor-used'])
        self.refused('wrong', made['username'], made['password'], address='203.0.113.10')
        self.assertFalse(self.login(made['username'], GOOD, device=changed['device'])['mustChange'])
        self.core._hello = (0.0, None)
        self.assertFalse(self.core.hello_now()['setupPending'])

    def test_the_owner_name_from_world_toml(self):
        with connect() as conn:
            self.assertEqual(A.ensure_first_owner(conn, 'ferro')['username'], 'ferro')

    def test_an_expired_one_time_password_and_the_renewal_on_the_next_start(self):
        with connect() as conn:
            made = A.ensure_first_owner(conn)
        owner_sql("UPDATE dm.admins SET must_change_until = now() - interval '1 minute'")
        self.refused('expired', made['username'], made['password'])
        self.refused('wrong', made['username'], 'not-it-at-all-1', address='203.0.113.11')
        with connect() as conn:
            renewed = A.ensure_first_owner(conn)
        self.assertEqual((renewed['username'], renewed['renewed']), (made['username'], True))
        self.refused('wrong', made['username'], made['password'], address='203.0.113.12')
        self.assertTrue(self.login(made['username'], renewed['password'])['mustChange'])

    def test_a_one_time_session_may_only_choose_its_password(self):
        with connect() as conn:
            made = A.ensure_first_owner(conn)
        session = self.session(self.login(made['username'], made['password'])['token'])
        self.assertTrue(session['mustChange'])
        self.assertEqual(A.ONE_TIME_PATHS, ('/editor/password', '/editor/keepalive', '/editor/logout'))
        with self.assertRaises(A.EditorError):
            run(self.core.step_up(session, {'password': made['password']}))

    def test_a_password_change_ends_the_other_sessions_and_forgets_devices(self):
        self.account('pell')
        first = self.login('pell')
        second = self.login('pell', address='203.0.113.20')
        changed = run(self.core.change_password(self.session(second['token']), {'old': GOOD, 'new': 'a-whole-new-one-77'}))
        self.assertIsNone(self.session(first['token']))
        self.assertIsNotNone(self.session(second['token']))
        self.assertEqual(owner_sql('SELECT token_hash FROM dm.devices'), [(A.token_hash(changed['device']),)])
        # The forgotten device is a new device again: its old token buys nothing, and blocks nothing (M4: the client
        # would otherwise be refused for good with the right password).
        again = self.login('pell', 'a-whole-new-one-77', device=first['device'], address='203.0.113.21')
        self.assertNotEqual(again['device'], first['device'])
        self.refused('wrong', 'pell', GOOD, device=first['device'], address='203.0.113.22')


class Lanes(SignInBase):
    def setUp(self):
        super().setUp()
        self.core.world = A.Bucket(1000, 1000, time.time)          # (Only the world-wide test keeps the real one.)

    def fail_sign_in(self, username, address, times=1, **extra):
        for _ in range(times):
            with self.assertRaises(A.EditorError) as caught:
                self.login(username, 'wrong-password-123', address, **extra)
            if caught.exception.code != 'wrong':
                return caught.exception
        return None

    def test_five_failures_slow_that_address_only_ipv6_by_64(self):
        self.account('pell')
        for i in range(5):
            self.fail_sign_in(f'nobody-{i}', '203.0.113.5')
        error = self.refused('wait', 'pell', address='203.0.113.5')
        self.assertEqual(error.extra, {'minutes': 15})
        self.login('pell', address='203.0.113.99')
        for i in range(5):
            self.fail_sign_in(f'someone-{i}', f'2001:db8:5:5::{i + 1}')
        self.refused('wait', 'pell', address='2001:db8:5:5:abcd::9')
        self.login('pell', address='2001:db8:5:6::1')

    def test_an_account_refuses_unknown_devices_after_five_but_not_a_known_device_or_a_code(self):
        self.account('pell')
        device = self.login('pell', address='192.0.2.1')['device']
        for i in range(5):
            self.fail_sign_in('pell', f'198.51.100.{i}')
        error = self.refused('wait', 'pell', address='198.51.100.77')
        self.assertEqual(error.extra, {'minutes': 15})
        self.login('pell', address='198.51.100.78', device=device)
        with connect() as conn:
            invite = A.invite_account(conn, 'pell', 'test')
        self.login('pell', address='198.51.100.79', invite=invite)
        # The lock is in dm.locks: a restarted helper keeps it.
        restarted = self.make_core()
        try:
            with self.assertRaises(A.EditorError) as caught:
                run(restarted.login({'username': 'pell', 'password': GOOD}, '198.51.100.80'))
            self.assertEqual(caught.exception.code, 'wait')
        finally:
            restarted.close()

    def test_unknown_names_lock_the_same_way(self):
        for i in range(5):
            self.fail_sign_in('nobody-here', f'198.51.100.{i}')
        self.refused('wait', 'nobody-here', address='198.51.100.50')
        self.assertEqual(owner_sql("SELECT count(*) FROM dm.audit")[0][0], 0, 'no row for an unknown name')

    def test_an_account_on_a_one_time_password_waits_by_name_like_any_other_never_world_wide_and_its_code_passes(self):
        # R1 review, security 5: five failures, then a wait, the same as an unknown name's (so a pending account can't
        # be told from one), kept in memory (no dm.locks row); never on the strangers' world-wide count.
        with connect() as conn:
            password, invite = A.add_account(conn, 'newbie', 'dm', 'test')
        before = self.core.world.tokens
        seen = {}
        for name in ('newbie', 'nobody-at-all'):
            codes = []
            for i in range(7):
                with self.assertRaises(A.EditorError) as caught:
                    self.login(name, 'wrong-password-123', f'198.51.100.{i}')
                codes.append((caught.exception.status, caught.exception.code, caught.exception.extra))
            seen[name] = codes
        self.assertEqual(seen['newbie'], seen['nobody-at-all'], 'a pending account answers as an unknown name does')
        self.assertEqual(seen['newbie'][:5], [(401, 'wrong', {})] * 5)
        self.assertEqual(seen['newbie'][5], (429, 'wait', {'minutes': 15}))
        self.assertAlmostEqual(self.core.world.tokens, before, delta=0.5)
        self.refused('wait', 'newbie', password, address='198.51.100.200')
        self.assertEqual(owner_sql("SELECT count(*) FROM dm.locks"), [(0,)])
        self.assertTrue(self.login('newbie', password, address='198.51.100.201', invite=invite)['mustChange'],
                        'its Editor code still passes')

    def test_an_address_waits_by_itself_in_the_one_time_lane(self):
        with connect() as conn:
            password, _ = A.add_account(conn, 'newbie', 'dm', 'test')
        for i in range(5):
            self.fail_sign_in(f'nobody-{i}', '203.0.113.33')
        self.refused('wait', 'newbie', password, address='203.0.113.33')
        self.assertTrue(self.login('newbie', password, address='203.0.113.34')['mustChange'])

    def test_one_time_failures_from_an_unidentified_peer_count_world_wide_on_their_own(self):
        # R1 review, security 6: from the gateway (all peers, under Docker Desktop) they counted nowhere.
        self.core.close()
        self.core = A.Core(connect=lambda: connect(), folder=self.folder.path, peers=A.Peers('folded'))
        self.core.world = A.Bucket(1000, 1000, time.time)
        self.core.one_time_world = A.Bucket(5, 0.0001, time.time)
        names = []
        with connect() as conn:
            for i in range(3):
                A.add_account(conn, f'pending-{i}', 'dm', 'test')
                names.append(f'pending-{i}')
        codes = [self.fail_sign_in(name, None, 2) for name in names]
        self.assertEqual(codes[:2], [None, None], 'two a name (no name waits yet)')
        self.assertIsNotNone(codes[2], 'the sixth: the world-wide count of their own is spent')
        error = self.refused('wait', 'pending-0', 'another-wrong-one-1', address=None)
        self.assertEqual(error.extra, {'minutes': 1})

    def test_a_waiting_address_without_a_code_or_device_is_told_before_any_query(self):
        self.account('pell')
        device = self.login('pell', address='192.0.2.1')['device']
        for i in range(5):
            self.fail_sign_in(f'nobody-{i}', '203.0.113.60')
        prepared = []
        real = self.core._prepare
        self.core._prepare = lambda *args: prepared.append(args) or real(*args)
        self.refused('wait', 'pell', address='203.0.113.60')
        self.assertEqual(prepared, [], 'no query for it')
        self.login('pell', address='203.0.113.60', device=device)
        self.assertEqual(len(prepared), 1, 'a known device still signs in from there')

    def test_a_device_token_counts_only_for_its_own_account(self):
        self.account('pell')
        self.account('mira')
        device = self.login('pell', address='192.0.2.1')['device']
        self.refused('wrong', 'mira', GOOD, address='192.0.2.1', device=device)
        self.assertEqual(owner_sql("SELECT failures FROM dm.locks WHERE username = 'mira'"), [(1,)],
                         'counted as an unknown device, and as a failure')

    def test_five_devices_at_most_and_a_device_waits_after_five_failures(self):
        self.account('pell')
        devices = [self.login('pell', address=f'192.0.2.{i}')['device'] for i in range(6)]
        stored = {r[0] for r in owner_sql("SELECT token_hash FROM dm.devices WHERE username = 'pell'")}
        self.assertEqual(stored, {A.token_hash(d) for d in devices[1:]})
        self.fail_sign_in('pell', '192.0.2.50', 5, device=devices[5])
        error = self.refused('wait', 'pell', address='192.0.2.50', device=devices[5])
        self.assertEqual(error.extra, {'minutes': 15})
        self.login('pell', address='192.0.2.50', device=devices[4])
        self.login('pell', address='192.0.2.50')
        self.assertEqual(owner_sql("SELECT count(*) FROM dm.locks"), [(0,)], "a device's failures lock nothing else")

    def test_only_failures_count_world_wide_and_the_bucket_starts_half_full(self):
        self.core.close()
        self.core = self.make_core()
        self.core.world.rate = 0        # (It refills on the real clock: under load a slow run would get one back.)
        self.account('pell')
        for i in range(30):
            self.login('pell', address=f'192.0.2.{i}')
        self.assertGreaterEqual(self.core.world.tokens, 9)
        exhausted = None
        for i in range(12):
            exhausted = self.fail_sign_in(f'nobody-{i:02d}', f'198.51.100.{i}')
            if exhausted:
                break
        self.assertIsNotNone(exhausted, 'refused once the world-wide count is spent')
        self.assertEqual(exhausted.code, 'wait')
        self.assertEqual(i, 10, 'half of 20 at the start')
        self.refused('wait', 'pell', address='198.51.100.250')
        with connect() as conn:
            invite = A.invite_account(conn, 'pell', 'test')
            password, _ = A.add_account(conn, 'newbie', 'dm', 'test')
        self.login('pell', address='198.51.100.251', invite=invite)
        self.assertTrue(self.login('newbie', password, address='198.51.100.252')['mustChange'])

    def test_a_full_counter_table_fails_closed(self):
        self.core.close()
        self.core = self.make_core(table_size=2)
        self.core.world = A.Bucket(1000, 0.0001, time.time)
        with connect() as conn:
            password, _ = A.add_account(conn, 'newbie', 'dm', 'test')
        self.fail_sign_in('nobody-a', '198.51.100.1')
        self.fail_sign_in('nobody-b', '198.51.100.2')
        self.core.world.tokens = 0
        self.refused('wait', 'newbie', password, address='198.51.100.3')
        self.assertTrue(self.login('newbie', password, address='198.51.100.1')['mustChange'])

    def test_invites_work_once_expire_and_void_after_five_wrong_passwords(self):
        self.account('pell')
        with connect() as conn:
            invite = A.invite_account(conn, 'pell', 'test')
        self.login('pell', invite=invite)
        self.refused('wrong', 'pell', invite=invite, address='203.0.113.40')
        with connect() as conn:
            invite = A.invite_account(conn, 'pell', 'test')
        owner_sql("UPDATE dm.invites SET expires_at = now() - interval '1 second' WHERE used_at IS NULL")
        self.refused('wrong', 'pell', invite=invite, address='203.0.113.41')
        with connect() as conn:
            invite = A.invite_account(conn, 'pell', 'test')
        for _ in range(5):
            self.fail_sign_in('pell', '203.0.113.42', invite=invite)
        self.refused('wrong', 'pell', invite=invite, address='203.0.113.43')

    def test_an_invite_skips_every_count(self):
        self.core.world = A.Bucket(1000, 0.0001, time.time)
        self.account('pell')
        with connect() as conn:
            invite = A.invite_account(conn, 'pell', 'test')
        self.fail_sign_in('pell', '203.0.113.50', 5)
        self.core.world.tokens = 0
        self.login('pell', address='203.0.113.50', invite=invite)

    def test_approve_needs_a_code_from_a_new_device(self):
        self.folder.settings(new_devices='approve')
        self.account('pell')
        with connect() as conn:
            invite = A.invite_account(conn, 'pell', 'test')
        self.refused('approve', 'pell')
        device = self.login('pell', invite=invite)['device']
        self.login('pell', device=device)

    def test_an_account_change_or_reset_unlocks_by_ratw_editor(self):
        self.account('pell')
        for i in range(5):
            self.fail_sign_in('pell', f'198.51.100.{i}')
        with connect() as conn:
            password, invite = A.reset_account(conn, 'pell', 'ratw-editor')
        self.assertTrue(self.login('pell', password, address='198.51.100.9', invite=invite)['mustChange'])


class Sessions(SignInBase):
    def test_three_an_account_its_own_oldest_evicted_and_ten_a_world(self):
        self.account('pell')
        tokens = [self.login('pell', address=f'192.0.2.{i}')['token'] for i in range(4)]
        self.assertIsNone(self.session(tokens[0]))
        self.assertTrue(all(self.session(t) for t in tokens[1:]))
        for name in ('ana', 'bo', 'cy'):
            self.account(name + '-x')
        for name in ('ana-x', 'bo-x'):
            for i in range(3):
                self.login(name, address=f'192.0.3.{i}')
        # 3 + 3 + 3 = 9: the tenth is kept for an owner (R1 review), so co-builders can't shut the owner out.
        error = self.refused('full', 'cy-x', address='192.0.4.1')
        self.assertEqual(error.status, 503)
        self.account('olga', 'owner')
        self.login('olga', address='192.0.4.9')                  # 10.
        self.account('dee-x', 'owner')
        self.refused('full', 'dee-x', address='192.0.4.2')
        self.assertEqual(owner_sql('SELECT count(*) FROM dm.sessions')[0][0], 10, 'nobody evicted')
        self.login('pell', address='192.0.4.3')                  # Its own oldest goes: still 10.
        self.assertEqual(owner_sql('SELECT count(*) FROM dm.sessions')[0][0], 10)

    def test_idle_timeout_keepalive_once_a_minute_and_reading_renews_nothing(self):
        self.account('pell')
        token = self.login('pell')['token']
        owner_sql("UPDATE dm.sessions SET last_seen = now() - interval '10 minutes'")
        seen = owner_sql('SELECT last_seen FROM dm.sessions')[0][0]
        session = self.session(token)
        self.assertEqual(owner_sql('SELECT last_seen FROM dm.sessions')[0][0], seen, 'a page request renews nothing')
        self.core.keepalive(session)
        renewed = owner_sql('SELECT last_seen FROM dm.sessions')[0][0]
        self.assertGreater(renewed, seen)
        self.core.keepalive(session)
        self.assertEqual(owner_sql('SELECT last_seen FROM dm.sessions')[0][0], renewed, 'at most once a minute')
        owner_sql("UPDATE dm.sessions SET last_seen = now() - interval '31 minutes'")
        self.assertIsNone(self.session(token), '30 minutes without input')

    def test_twelve_hours_at_most_a_disabled_account_at_once_and_a_restart_keeps_sessions(self):
        self.account('pell')
        self.account('mira', 'owner')
        token = self.login('pell')['token']
        self.assertAlmostEqual(float(owner_sql("SELECT extract(epoch FROM expires_at - now()) / 3600 FROM dm.sessions")[0][0]), 12, delta=0.01)
        restarted = self.make_core()
        try:
            self.assertEqual(restarted.session_now(token)['account'], 'pell')
        finally:
            restarted.close()
        owner_sql("UPDATE dm.admins SET disabled = true WHERE username = 'pell'")
        self.assertIsNone(self.session(token))
        owner_sql("UPDATE dm.admins SET disabled = false WHERE username = 'pell'")
        token = self.login('pell')['token']
        owner_sql("UPDATE dm.sessions SET expires_at = now() - interval '1 second'")
        self.assertIsNone(self.session(token))

    def test_logout(self):
        self.account('pell')
        token = self.login('pell')['token']
        self.core.logout(self.session(token))
        self.assertIsNone(self.session(token))


class Accounts(SignInBase):
    def setUp(self):
        super().setUp()
        self.account('ferro', 'owner')
        self.token = self.login('ferro', address='192.168.1.20')['token']

    def owner(self):
        return self.session(self.token)

    def stepped(self):
        run(self.core.step_up(self.owner(), {'password': GOOD}))
        return self.owner()

    def call(self, verb, session=None, **body):
        return self.core.accounts(session or self.stepped(), verb, body, '192.168.1.20')

    def refused_call(self, code, verb, session=None, **body):
        with self.assertRaises(A.EditorError) as caught:
            self.call(verb, session, **body)
        self.assertEqual(caught.exception.code, code, caught.exception.message)

    def test_owner_only_and_a_step_up_first(self):
        self.account('pell', 'builder')
        builder = self.session(self.login('pell')['token'])
        run(self.core.step_up(builder, {'password': GOOD}))
        self.refused_call('owner-only', 'list', self.session(self.login('pell', address='192.0.2.8')['token']))
        self.refused_call('step-up', 'list', self.owner())
        self.assertIn('accounts', self.call('list'))
        owner_sql("UPDATE dm.sessions SET step_up_until = now() - interval '1 second'")
        self.refused_call('step-up', 'list', self.owner())

    def test_five_wrong_step_ups_end_the_session(self):
        for i in range(4):
            with self.assertRaises(A.EditorError) as caught:
                run(self.core.step_up(self.owner(), {'password': f'not-mine-{i}'}))
            self.assertEqual(caught.exception.code, 'wrong-password')
        with self.assertRaises(A.EditorError) as caught:
            run(self.core.step_up(self.owner(), {'password': 'not-mine-5'}))
        self.assertEqual(caught.exception.code, 'ended')
        self.assertIsNone(self.owner())

    def test_a_step_up_waits_behind_sign_ins_one_at_a_time_a_session_and_counts_before_scrypt(self):
        # R1 review, security 7: a viewer's wrong step-ups made the owner's known-device sign-in "busy".
        self.account('vic', 'viewer')
        viewer = self.session(self.login('vic', address='192.0.2.70')['token'])
        priorities = []
        release = threading.Event()
        real = self.core.gate.run

        async def gate(priority, work):
            priorities.append(priority)
            await asyncio.get_running_loop().run_in_executor(None, release.wait, 5)
            return await real(priority, work)
        self.core.gate.run = gate

        async def both():
            first = asyncio.create_task(self.core.step_up(viewer, {'password': 'not-mine-1'}))
            await asyncio.sleep(0.05)
            with self.assertRaises(A.EditorError) as caught:
                await self.core.step_up(viewer, {'password': 'not-mine-2'})
            self.assertEqual((caught.exception.status, caught.exception.code), (429, 'slow-down'))
            self.assertEqual(self.core.session_failures.get(viewer['hash'])['count'], 1, 'counted before scrypt')
            release.set()
            with self.assertRaises(A.EditorError) as caught:
                await first
            self.assertEqual(caught.exception.code, 'wrong-password')
        run(both())
        self.assertEqual(priorities, [1], 'behind known devices and codes (0), with strangers')
        run(self.core.step_up(viewer, {'password': GOOD}))
        self.assertIsNone(self.core.session_failures.get(viewer['hash']), 'a right one forgets them')

    def test_wrong_step_ups_are_forgotten_once_their_session_can_t_be_alive(self):
        # R1 review: the table never pruned, so once full every wrong step-up ended its session.
        self.core.close()
        self.core = self.make_core(table_size=2)
        self.token = self.login('ferro', address='192.168.1.20')['token']
        for i in range(2):
            other = self.session(self.login('ferro', address=f'192.168.1.{30 + i}')['token'])
            with self.assertRaises(A.EditorError):
                run(self.core.step_up(other, {'password': 'not-mine'}))
        for entry in self.core.session_failures.items.values():
            entry['at'] -= A.SESSION_HOURS * 3600 + 1
        with self.assertRaises(A.EditorError) as caught:
            run(self.core.step_up(self.owner(), {'password': 'not-mine'}))
        self.assertEqual(caught.exception.code, 'wrong-password', 'not ended: the stale entries made room')

    def test_add_gives_a_code_that_pins_the_key_and_signs_in_once(self):
        result = self.call('add', username='pell', role='dm')
        record = A.read_code(result['code'])
        self.assertEqual(record['k'], A.pin(A.cert_spki(self.folder.path / 'editor-cert.pem')))
        self.assertEqual((record['w'], record['u']), (A.world_id(self.folder.path), 'pell'))
        hours = owner_sql("SELECT extract(epoch FROM expires_at - now()) / 3600 FROM dm.invites")[0][0]
        self.assertAlmostEqual(float(hours), 72, delta=0.01)
        signed = self.login('pell', record['p'], invite=record['i'], address='203.0.113.70')
        self.assertTrue(signed['mustChange'])
        self.assertEqual(owner_sql("SELECT claimed_from FROM dm.admins WHERE username = 'pell'"), [('203.0.113.70',)])
        listed = {a['username']: a for a in self.call('list')['accounts']}
        self.assertEqual((listed['pell']['role'], listed['pell']['claimedFrom']), ('dm', '203.0.113.70'))

    def test_nobody_makes_or_promotes_an_owner_from_the_screen(self):
        self.refused_call('role', 'add', username='pell', role='owner')
        self.refused_call('role', 'add', username='pell', role='admin')
        self.call('add', username='pell', role='builder')
        self.refused_call('role', 'role', username='pell', role='owner')
        self.account('mira', 'owner')
        self.refused_call('owner-account', 'reset', username='mira')

    def test_the_last_owner_stays(self):
        self.refused_call('last-owner', 'remove', username='ferro')
        self.refused_call('last-owner', 'disable', username='ferro')
        self.refused_call('last-owner', 'role', username='ferro', role='builder')
        with connect() as conn:
            for refuse in (lambda: A.remove_account(conn, 'ferro', 'ratw-editor'),
                           lambda: A.set_disabled(conn, 'ferro', True, 'ratw-editor')):
                with self.assertRaises(A.EditorError):
                    refuse()

    def test_every_change_writes_a_log_line_an_audit_row_and_a_banner(self):
        with Captured() as captured:
            self.call('add', username='pell', role='dm')
            self.call('wolves', username='pell', allowed=True)
            self.call('role', username='pell', role='builder')
            self.call('reset', username='pell')
            self.call('disable', username='pell')
            self.call('enable', username='pell')
            self.call('remove', username='pell')
        actions = [r[0] for r in owner_sql("SELECT action FROM dm.audit WHERE action LIKE 'account.%' AND target = 'pell' ORDER BY sequence")]
        self.assertEqual(actions, ['account.add', 'account.wolves', 'account.role', 'account.reset', 'account.disable',
                                   'account.enable', 'account.remove'])
        whos = {r[0] for r in owner_sql("SELECT who FROM dm.audit WHERE action LIKE 'account.%' AND target = 'pell'")}
        self.assertEqual(whos, {self.owner()['username']})
        self.assertEqual(len([l for l in captured.lines if ' account.' in l]), 7)
        banners = [b for b in self.call('list')['banners'] if b['target'] == 'pell']
        self.assertEqual([b['action'] for b in reversed(banners)], actions)

    def test_wolves_flag_and_end_all_sessions_forgets_devices(self):
        self.account('pell')
        self.login('pell', address='192.0.2.3')
        self.call('wolves', username='pell', allowed=True)
        self.assertTrue(self.session(self.login('pell', address='192.0.2.4')['token'])['mayActOnWolves'])
        self.call('end-sessions')
        self.assertEqual(owner_sql('SELECT count(*) FROM dm.sessions')[0][0] + owner_sql('SELECT count(*) FROM dm.devices')[0][0], 0)
        self.assertIsNone(self.owner())

    def test_the_owner_s_sign_in_carries_counts_only(self):
        """R1, M-i: failed sign-ins today and account changes by others since the owner's last sign-in, before any
        step-up; nothing for anyone else."""
        first = self.login('ferro', address='192.168.1.21')
        self.assertEqual(first['notices'], {'failedToday': 0, 'changes': 0})
        self.call('add', username='pell', role='dm')                  # (The owner's own change: not counted.)
        with connect() as conn:
            A.add_account(conn, 'mira', 'dm', 'ratw-editor')
        with self.assertRaises(A.EditorError):
            self.login('ferro', 'not-the-one-1', address='198.51.100.7')
        again = self.login('ferro', address='192.168.1.22')
        self.assertEqual(again['notices'], {'failedToday': 1, 'changes': 1})
        self.assertEqual(set(again['notices']), {'failedToday', 'changes'}, 'counts only')
        self.account('kit', 'builder')
        self.assertNotIn('notices', self.login('kit'))

    def test_this_computer_s_addresses_read_local(self):
        self.core.peers.unidentified = {'172.20.0.1'}
        self.account('pell')
        self.account('mira')
        self.login('pell', address='172.20.0.1')
        self.login('mira', address='127.0.0.1')
        listed = {a['username']: a for a in self.call('list')['accounts']}
        self.assertEqual((listed['pell']['lastFrom'], listed['pell']['claimedFrom']), ('local', 'local'))
        self.assertEqual(listed['mira']['lastFrom'], 'local')
        self.assertEqual(listed['ferro']['claimedFrom'], '192.168.1.20', 'a real address stays')

    def test_failed_sign_ins_today_and_one_audit_row_per_account_and_address_an_hour(self):
        for _ in range(3):
            with self.assertRaises(A.EditorError):
                self.login('ferro', 'not-the-one-1', address='198.51.100.7')
        with self.assertRaises(A.EditorError):
            self.login('nobody-else', 'not-the-one-1', address='198.51.100.7')
        rows = owner_sql("SELECT target, detail FROM dm.audit WHERE action = 'login.failed'")
        self.assertEqual(rows, [('ferro', '3 failed sign-ins from 198.51.100.7 this hour.')])
        self.assertEqual(self.call('list')['failedToday'], 4)


class Logs(SignInBase):
    def test_folded_above_the_threshold(self):
        self.core.world = A.Bucket(1000, 1000, time.time)          # (Not the world-wide limit's test.)
        with Captured() as captured:
            for i in range(A.FOLD_ABOVE + 6):
                with self.assertRaises(A.EditorError):
                    self.login(f'nobody-{i}', 'not-the-one-1', address=f'198.51.100.{i}')
            self.core.flush_minute()
        failed = [l for l in captured.lines if l.startswith('RATW_EDITOR login.failed ')]
        self.assertEqual(len(failed), A.FOLD_ABOVE)
        self.assertIn('RATW_EDITOR login.failed.folded count=6', captured.lines)
        self.assertEqual(owner_sql("SELECT detail FROM dm.audit WHERE action = 'login.failed.folded'"),
                         [('6 more failed sign-ins in a minute.',)])

    def test_no_secret_ever_reaches_the_log(self):
        secrets_seen = []
        with Captured() as captured:
            with connect() as conn:
                made = A.ensure_first_owner(conn)
            secrets_seen += [made['password'], made['invite']]
            signed = self.login(made['username'], made['password'], invite=made['invite'])
            session = self.session(signed['token'])
            changed = run(self.core.change_password(session, {'old': made['password'], 'new': GOOD}))
            run(self.core.step_up(self.session(signed['token']), {'password': GOOD}))
            code = self.core.accounts(self.session(signed['token']), 'add', {'username': 'pell', 'role': 'dm'}, None)['code']
            typed_password_as_name = 'my-secret-passphrase-typed-in-the-wrong-box'
            for name, password in ((typed_password_as_name, 'x' * 12), ('pell', 'wrong-but-secret-99')):
                with self.assertRaises(A.EditorError):
                    self.login(name, password)
            with self.assertRaises(A.EditorError):
                run(self.core.change_password(self.session(signed['token']), {'old': 'secret-old-guess-1', 'new': GOOD}))
            secrets_seen += [signed['token'], changed['device'], code, GOOD, typed_password_as_name,
                             'wrong-but-secret-99', 'secret-old-guess-1', A.token_hash(signed['token'])]
        text = captured.text()
        self.assertTrue(captured.lines)
        for secret in secrets_seen:
            self.assertNotIn(secret, text)
        self.assertTrue(all(line.startswith('RATW_EDITOR ') for line in captured.lines))

    def test_log_values_are_checked(self):
        with Captured() as captured:
            A.log('x', account='pell', route='/editor/login?password=hunter2', other='a b')
        self.assertEqual(captured.lines, ['RATW_EDITOR x account=pell route=? other=?'])


# ------------------------------------------------------------------------------------------------------- the port

class Server:
    """An EditorServer on 127.0.0.1, in a thread of its own."""

    def __init__(self, core, **kw):
        self.loop = asyncio.new_event_loop()
        self.server = A.EditorServer(core, '127.0.0.1', 0, **kw)
        ready = threading.Event()

        def run_loop():
            asyncio.set_event_loop(self.loop)
            self.port = self.loop.run_until_complete(self.server.start())
            ready.set()
            self.loop.run_forever()
        self.thread = threading.Thread(target=run_loop, daemon=True)
        self.thread.start()
        ready.wait(10)

    def stop(self):
        asyncio.run_coroutine_threadsafe(self.server.stop(), self.loop).result(10)
        self.loop.call_soon_threadsafe(self.loop.stop)
        self.thread.join(10)
        self.loop.close()


def tls_socket(port, version=ssl.TLSVersion.TLSv1_3):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    context.minimum_version = min(version, ssl.TLSVersion.TLSv1_2)
    context.maximum_version = version
    return context.wrap_socket(socket.create_connection(('127.0.0.1', port), timeout=10))


def read_reply(sock):
    data = b''
    while b'\r\n\r\n' not in data:
        chunk = sock.recv(65536)
        if not chunk:
            raise ConnectionError('closed')
        data += chunk
    head, _, rest = data.partition(b'\r\n\r\n')
    lines = head.decode('latin-1').split('\r\n')
    status = int(lines[0].split(' ')[1])
    headers = {k.lower(): v.strip() for k, _, v in (l.partition(':') for l in lines[1:])}
    length = int(headers['content-length'])
    while len(rest) < length:
        chunk = sock.recv(65536)
        if not chunk:
            break
        rest += chunk
    body = rest[:length]
    try:
        return status, headers, json.loads(body)
    except ValueError:
        return status, headers, body


def call(port, method, path, body=None, token=None, headers=None, sock=None, raw_length=None):
    own = sock is None
    sock = sock or tls_socket(port)
    data = json.dumps(body).encode() if isinstance(body, (dict, list)) else (body or b'')
    lines = [f'{method} {path} HTTP/1.1', 'Host: 203.0.113.1:17789']
    if method == 'POST' or data or raw_length is not None:
        lines.append(f'Content-Length: {raw_length if raw_length is not None else len(data)}')
    if token:
        lines.append(f'Authorization: Bearer {token}')
    for name, value in (headers or {}).items():
        lines.append(f'{name}: {value}')
    sock.sendall(('\r\n'.join(lines) + '\r\n\r\n').encode() + (data if raw_length is None else b''))
    try:
        return read_reply(sock)
    finally:
        if own:
            sock.close()


def closed_without_reply(sock, seconds=5):
    sock.settimeout(seconds)
    try:
        return sock.recv(100) == b''
    except (ConnectionResetError, ssl.SSLError):
        return True
    except socket.timeout:
        return False


class PortBase(SignInBase):
    """A running port. Connections are limited world-wide only here (one test client opens many from 127.0.0.1);
    PortLimits tests the per-address ones."""

    def setUp(self):
        super().setUp()
        self.running = self.start()
        self.port = self.running.port

    def start(self):
        return Server(self.core, connections=A.Connections(A.Peers('folded')))

    def tearDown(self):
        self.running.stop()
        super().tearDown()

    def signed(self, name='pell', role='owner'):
        self.account(name, role)
        status, _, reply = call(self.port, 'POST', '/editor/login', {'username': name, 'password': GOOD})
        self.assertEqual(status, 200, reply)
        return reply


class Port(PortBase):

    def test_tls_13_only_and_the_world_key(self):
        with self.assertRaises((ssl.SSLError, ConnectionError)):
            tls_socket(self.port, ssl.TLSVersion.TLSv1_2).sendall(b'x')
        with tls_socket(self.port) as sock:
            self.assertEqual(sock.version(), 'TLSv1.3')
            spki = A.spki_of(sock.getpeercert(binary_form=True))
        self.assertEqual(A.fingerprint(spki), A.fingerprint(A.cert_spki(self.folder.path / 'editor-cert.pem')))

    def test_plain_http_gets_no_reply(self):
        with socket.create_connection(('127.0.0.1', self.port), timeout=10) as sock:
            sock.sendall(b'GET /editor/hello HTTP/1.1\r\nHost: x\r\n\r\n')
            sock.settimeout(15)
            received = b''
            with contextlib.suppress(ConnectionResetError):
                while chunk := sock.recv(4096):
                    received += chunk
        self.assertNotIn(b'HTTP/', received)

    def test_hello_is_public_small_and_says_no_secret(self):
        with connect() as conn:
            A.ensure_first_owner(conn)
        (self.folder.path / 'settings.json').write_text(json.dumps({'name': '‮evil\x07' + 'n' * 200, 'package': 'p'}))
        status, headers, hello = call(self.port, 'GET', '/editor/hello')
        self.assertEqual(status, 200)
        self.assertLessEqual(int(headers['content-length']), A.HELLO_MOST)
        self.assertEqual(set(hello), {'world', 'name', 'package', 'editorApi', 'kind', 'serverState', 'progress', 'setupPending',
                                     'wolves', 'lastBackup'})   # (M5: the menu's two numbers, public as /status is.)
        self.assertEqual((hello['kind'], hello['setupPending'], hello['editorApi']), ('community', True, 1))
        self.assertEqual(hello['name'], 'evil' + 'n' * 36)
        self.assertEqual(hello['progress'], {'code': 'ready', 'percent': 100})
        self.assertEqual(headers['x-content-type-options'], 'nosniff')

    def test_the_browser_rule(self):
        token = self.signed()['token']
        for header in ({'Origin': 'https://evil.example'}, {'Cookie': 'a=b'}, {'Sec-Fetch-Site': 'cross-site'},
                       {'sec-fetch-mode': 'cors'}):
            for method, path, auth in (('GET', '/editor/hello', None), ('GET', '/api/me', token)):
                status, headers, _ = call(self.port, method, path, token=auth, headers=header)
                self.assertEqual(status, 403, (header, path))
                self.assertFalse([h for h in headers if h.startswith('access-control')])
        status, headers, _ = call(self.port, 'OPTIONS', '/editor/login', headers={'Access-Control-Request-Method': 'POST'})
        self.assertEqual(status, 405)
        self.assertFalse([h for h in headers if h.startswith('access-control')])

    def test_bodies_before_sign_in(self):
        status, _, _ = call(self.port, 'POST', '/editor/login', b'{}' + b' ' * (4 * 1024))
        self.assertEqual(status, 413)
        with tls_socket(self.port) as sock:
            sock.sendall(b'POST /editor/login HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n')
            self.assertEqual(read_reply(sock)[0], 411)
        with tls_socket(self.port) as sock:
            sock.sendall(b'POST /editor/login HTTP/1.1\r\nHost: x\r\n\r\n')
            self.assertEqual(read_reply(sock)[0], 411)
        status, _, _ = call(self.port, 'POST', '/api/anything', raw_length=1000)
        self.assertEqual(status, 401, 'no body is read before the session is checked')

    def test_page_files_and_the_api_need_a_session(self):
        for path in ('/', '/index.html', '/bundle.json', '/api/me', '/api/dm/login', '/editor/accounts'):
            self.assertEqual(call(self.port, 'GET', path)[0], 401, path)
            self.assertEqual(call(self.port, 'GET', path, token='rh_' + 'x' * 30)[0], 401, path)
        token = self.signed()['token']
        self.assertEqual(call(self.port, 'GET', '/api/me', token=token)[0], 404, 'the hosted studio is M3')

    def test_signing_in_over_the_port_and_the_one_time_session(self):
        with connect() as conn:
            made = A.ensure_first_owner(conn)
        status, _, reply = call(self.port, 'POST', '/editor/login', {'username': made['username'], 'password': made['password']})
        self.assertEqual((status, reply['mustChange']), (200, True))
        token = reply['token']
        self.assertEqual(call(self.port, 'GET', '/editor/accounts', token=token)[2]['error'], 'must-change')
        self.assertEqual(call(self.port, 'GET', '/api/me', token=token)[2]['error'], 'must-change')
        self.assertEqual(call(self.port, 'POST', '/editor/keepalive', {}, token=token)[0], 200)
        status, _, changed = call(self.port, 'POST', '/editor/password', {'old': made['password'], 'new': GOOD}, token=token)
        self.assertEqual(status, 200, changed)
        self.assertEqual(call(self.port, 'POST', '/editor/step-up', {'password': GOOD}, token=token)[0], 200)
        status, _, listed = call(self.port, 'GET', '/editor/accounts', token=token)
        self.assertEqual((status, [a['username'] for a in listed['accounts']]), (200, [made['username']]))
        status, _, added = call(self.port, 'POST', '/editor/accounts/add', {'username': 'pell', 'role': 'dm'}, token=token)
        self.assertTrue(added['code'].startswith('ratw-editor:'))
        self.assertEqual(call(self.port, 'POST', '/editor/accounts/nope', {}, token=token)[0], 404)
        self.assertEqual(call(self.port, 'POST', '/editor/logout', {}, token=token)[0], 200)
        self.assertEqual(call(self.port, 'GET', '/editor/accounts', token=token)[0], 401)

    def test_wrong_sign_ins_are_401_with_the_same_words(self):
        self.account('pell')
        for name in ('pell', 'nobody'):
            status, _, reply = call(self.port, 'POST', '/editor/login', {'username': name, 'password': 'not-it-1234'})
            self.assertEqual((status, reply['error'], reply['message']), (401, 'wrong', 'Wrong name or password.'))
        self.assertEqual(call(self.port, 'POST', '/editor/login', b'not json')[0], 400)

    def test_keep_alive_connections_and_a_handler_that_raises(self):
        token = self.signed()['token']

        def app(session, method, path, query, headers, body):
            if path == '/api/boom':
                raise RuntimeError('a request value: ' + query)
            return 200, 'text/html', b'<script>alert(1)</script>'
        self.core.app = app
        with Captured() as captured, tls_socket(self.port) as sock:
            status, _, _ = call(self.port, 'GET', '/api/boom?secret=hunter2', token=token, sock=sock)
            self.assertEqual(status, 500)
            status, headers, body = call(self.port, 'GET', '/api/page', token=token, sock=sock)
            self.assertEqual((status, headers['content-type'], headers['x-content-type-options']),
                             (200, 'application/json', 'nosniff'), 'no API reply can run as script')
            self.assertEqual(body, b'<script>alert(1)</script>')
            self.assertEqual(call(self.port, 'GET', '/editor/hello', sock=sock)[0], 200)
        self.assertNotIn('hunter2', captured.text())
        self.assertIn('RATW_EDITOR handler.error error=RuntimeError', captured.lines)
        self.assertIn('RATW_EDITOR request account=pell route=/api/page status=200', captured.lines)

    def test_a_slow_signed_in_reply_does_not_delay_signing_in(self):
        token = self.signed()['token']
        self.account('mira')
        release = threading.Event()

        def app(session, method, path, query, headers, body):
            release.wait(10)
            return 200, 'application/json', b'{}'
        self.core.app = app
        threads = [threading.Thread(target=call, args=(self.port, 'GET', f'/api/live/world{i}'), kwargs={'token': token})
                   for i in range(3)]
        for thread in threads:
            thread.start()
        time.sleep(0.3)
        try:
            started = time.perf_counter()
            status, _, _ = call(self.port, 'POST', '/editor/login', {'username': 'mira', 'password': GOOD})
            self.assertEqual(status, 200)
            self.assertLess(time.perf_counter() - started, 2)
        finally:
            release.set()
            for thread in threads:
                thread.join(10)

    def test_bodies_once_signed_in(self):
        token = self.signed()['token']
        release = threading.Event()

        def app(session, method, path, query, headers, body):
            release.wait(10)
            return 200, 'application/json', json.dumps({'got': len(body)}).encode()
        self.core.app = app
        status, _, _ = call(self.port, 'POST', '/api/atlas/save', token=token, raw_length=A.SIGNED_IN_BODY + 1)
        self.assertEqual(status, 413)
        self.assertEqual(call(self.port, 'POST', '/editor/step-up', b' ' * 5000, token=token)[0], 413)
        big = b'{"x":"' + b'a' * (A.LARGE_BODY + 10) + b'"}'
        results = []
        threads = [threading.Thread(target=lambda: results.append(call(self.port, 'POST', '/api/atlas/save', big, token=token)[0]))
                   for _ in range(2)]
        for thread in threads:
            thread.start()
        time.sleep(1.0)
        third = call(self.port, 'POST', '/api/atlas/save', big, token=token)[0]
        release.set()
        for thread in threads:
            thread.join(20)
        self.assertEqual(third, 503, 'at most 2 bodies over 1 MB at once')
        self.assertEqual(sorted(results), [200, 200])

    def test_twenty_requests_a_second_a_session(self):
        token = self.signed()['token']
        with tls_socket(self.port) as sock:
            statuses = [call(self.port, 'GET', '/api/x', token=token, sock=sock)[0] for _ in range(60)]
        self.assertIn(429, statuses)
        self.assertEqual(statuses[0], 404)


class PortLimits(PortBase):
    def start(self):
        return Server(self.core, handshake_seconds=1.0, connections=A.Connections(A.Peers('real', unidentified=[])))

    def test_a_fifth_connection_from_one_address_is_refused(self):
        held = [socket.create_connection(('127.0.0.1', self.port), timeout=10) for _ in range(4)]
        time.sleep(0.2)
        try:
            with socket.create_connection(('127.0.0.1', self.port), timeout=10) as fifth:
                self.assertTrue(closed_without_reply(fifth, 2))
        finally:
            for sock in held:
                sock.close()
        time.sleep(0.2)
        self.assertEqual(call(self.port, 'GET', '/editor/hello')[0], 200, 'room again once they close')

    def test_a_few_handshakes_a_second_from_one_address(self):
        statuses = []
        for _ in range(15):
            try:
                statuses.append(call(self.port, 'GET', '/editor/hello')[0])
            except (ConnectionError, ssl.SSLError, OSError):
                statuses.append('refused')
        self.assertEqual(statuses[:6], [200] * 6, 'a burst of 6')
        self.assertIn('refused', statuses)

    def test_a_slow_handshake_is_dropped(self):
        with socket.create_connection(('127.0.0.1', self.port), timeout=10) as sock:
            started = time.perf_counter()
            self.assertTrue(closed_without_reply(sock, 5))
            self.assertLess(time.perf_counter() - started, 4)


class PortWorldWide(PortBase):
    def start(self):
        return Server(self.core, connections=A.Connections(A.Peers('folded')), anonymous_idle_seconds=1.0)

    def test_32_at_once_the_oldest_without_a_session_giving_way_and_never_a_signed_in_one(self):
        # R1 review, security 3: 32 connections held without a session (8 addresses, a hello every 55 s) kept the
        # owner out for good.
        token = self.signed()['token']
        signed = tls_socket(self.port)
        self.assertEqual(call(self.port, 'GET', '/api/x', token=token, sock=signed)[0], 404)
        held = [socket.create_connection(('127.0.0.1', self.port), timeout=10) for _ in range(31)]
        time.sleep(0.3)
        try:
            self.assertEqual(call(self.port, 'GET', '/editor/hello')[0], 200, 'the 33rd gets in')
            self.assertTrue(closed_without_reply(held[0], 2), 'the oldest without a session gave way')
            self.assertEqual(call(self.port, 'GET', '/api/x', token=token, sock=signed)[0], 404, 'the signed-in one stays')
        finally:
            signed.close()
            for sock in held:
                sock.close()

    def test_a_connection_without_a_session_waits_a_short_while_between_requests(self):
        token = self.signed()['token']
        with tls_socket(self.port) as anonymous, tls_socket(self.port) as signed:
            self.assertEqual(call(self.port, 'GET', '/editor/hello', sock=anonymous)[0], 200)
            self.assertEqual(call(self.port, 'GET', '/api/x', token=token, sock=signed)[0], 404)
            time.sleep(1.6)
            self.assertTrue(closed_without_reply(anonymous, 2), 'closed after its short wait')
            self.assertEqual(call(self.port, 'GET', '/api/x', token=token, sock=signed)[0], 404, 'a session\'s stays open')

    def test_bearer_tokens_that_are_no_session_are_slowed_before_the_lookup(self):
        # R1 review: each one was a database lookup in the signed-in pool, unlimited.
        token = self.signed()['token']
        looked = []
        real = self.core.session_now
        self.core.session_now = lambda t: looked.append(t) or real(t)
        statuses = []
        for i in range(30):
            try:
                statuses.append(call(self.port, 'GET', '/api/x', token=f'bogus-token-{i:04d}-xxxxxxxxxxxx')[0])
            except (ConnectionError, ssl.SSLError, OSError):
                statuses.append('closed')
        self.assertEqual(statuses[:20], [401] * 20)
        self.assertIn(429, statuses)
        self.assertLessEqual(len(looked), 21)
        self.assertEqual(call(self.port, 'GET', '/api/x', token=token)[0], 404, "a session's own token isn't slowed")


class HostedStudioPort(PortBase):
    """Doc 78, M3: the hosted studio behind the helper's port, on a throwaway world (the registry's `world`): the DM tool
    by the session, roles, Close and Open as ratw_dm (migration 0062), My world and its owner-only views, the AI's
    no-key state, caps and budget in dm.ai_use, and large replies gzipped."""
    HOST = 'acct-' + 'b' * 32

    def setUp(self):
        super().setUp()
        import studio
        self.studio_module = studio
        self.temp = tempfile.TemporaryDirectory(prefix='ratw-hosted-registry-')
        registry = Path(self.temp.name) / 'worlds.json'
        # (And `dev`, which this file's own connections name, with the throwaway database given to each.)
        registry.write_text(json.dumps({'worlds': [{'key': 'world', 'name': WORLD_NAME, 'database': SCRATCH, 'kind': 'live'},
                                                   {'key': 'dev', 'name': 'DEV', 'database': SCRATCH + '_unused', 'kind': 'dev'}]}))
        self.env = mock.patch.dict(os.environ, {'RATW_WORLDS_FILE': str(registry),
                                                'RATW_WORLDS_LOCAL': str(Path(self.temp.name) / 'worlds.local.json'),
                                                'RATW_AI_CONFIG': str(self.folder.path / 'npc-ai.json'), 'RATW_AI': 'on'})
        self.env.start()
        (self.folder.path / 'central.json').write_text(json.dumps({'kind': 'community', 'host': self.HOST}))
        self.write_settings()
        self.core.app = studio.HostedStudio(studio.Hosted('world', self.folder.path))
        owner_sql('TRUNCATE dm.ai_use')

    def tearDown(self):
        self.env.stop()
        owner_sql("UPDATE admin.server_state SET state = 'running', changed_by = '', note = ''")
        self.temp.cleanup()
        super().tearDown()

    def write_settings(self, **extra):
        (self.folder.path / 'settings.json').write_text(json.dumps(
            {'name': WORLD_NAME, 'package': 'test-1', 'cheats': 'off', 'player_limit': 40, 'workers': 2,
             'folder': 'C:\\Users\\pell\\worlds\\ashfall', 'editor': {}, **extra}))

    def test_the_dm_tool_behind_the_session_by_role(self):
        dm = self.signed('fern', 'dm')['token']
        self.assertEqual(call(self.port, 'GET', '/api/dm/me')[0], 401, 'nothing without a session')
        status, _, me = call(self.port, 'GET', '/api/dm/me', token=dm)
        self.assertEqual((status, me['role'], me['account'], me['hosted']), (200, 'dm', 'fern', True), me)
        self.assertRegex(me['username'], r'^fern#[0-9a-f]{6}$')
        status, _, session = call(self.port, 'GET', '/api/studio/session', token=dm)
        self.assertEqual((status, session['dm']['world'], session['atlas']['available'], session['myWorld']),
                         (200, 'world', False, False), session)
        for method, path in (('GET', '/api/dm/artwork'), ('POST', '/api/dm/login'), ('GET', '/api/atlas/session'),
                             ('GET', '/api/dm/import/plan'), ('POST', '/api/studio/dm-world')):
            self.assertEqual(call(self.port, method, path, {} if method == 'POST' else None, token=dm)[0], 404, path)
        self.assertEqual(call(self.port, 'POST', '/api/dm/server/maintenance', {'on': True}, token=dm)[0], 403)
        status, _, players = call(self.port, 'GET', '/api/dm/players', token=dm)
        self.assertEqual(status, 200, players)
        status, headers, page = call(self.port, 'GET', '/', token=dm)
        self.assertEqual((status, headers['content-type']), (200, 'text/html'))
        viewer = self.signed('wren', 'viewer')['token']
        self.assertEqual(call(self.port, 'GET', '/api/dm/reports', token=viewer)[0], 403)
        status, _, health = call(self.port, 'GET', '/api/dm/health', token=viewer)
        self.assertEqual((status, health['error']), (404, 'The WORLD database has no world yet.'),
                         'Server Health is everyone\'s (this throwaway world has none to show)')

    def test_close_and_open_by_the_owner_as_ratw_dm(self):
        owner = self.signed('pell', 'owner')['token']
        status, _, closed = call(self.port, 'POST', '/api/dm/server/maintenance', {'on': True, 'note': 'Festival prep.'}, token=owner)
        self.assertEqual((status, closed['server']['state'], closed['hosted']), (200, 'maintenance', True), closed)
        by = owner_sql('SELECT state, changed_by, note FROM admin.server_state')[0]
        self.assertEqual(by[0], 'maintenance')
        self.assertRegex(by[1], r'^pell#[0-9a-f]{6}$')
        audit = owner_sql("SELECT who, detail FROM dm.audit WHERE action = 'server.maintenance' ORDER BY sequence DESC LIMIT 1")[0]
        self.assertEqual(audit[0], by[1])
        status, _, opened = call(self.port, 'POST', '/api/dm/server/maintenance', {'on': False}, token=owner)
        self.assertEqual((status, opened['server']['state']), (200, 'running'))
        status, _, server = call(self.port, 'GET', '/api/dm/server/status', token=owner)
        self.assertEqual((status, server['key'], server['switchTo'], server['play']), (200, 'world', [], None))
        self.assertIsNone(server['hostServer'], 'no server.json: nothing said')
        (self.folder.path / 'server.json').write_text(json.dumps(
            {'up': False, 'reason': 'exit', 'code': 11, 'failures': 2, 'retryAt': time.time() + 30, 'at': time.time()}))
        server = call(self.port, 'GET', '/api/dm/server/status', token=owner)[2]
        self.assertEqual({k: server['hostServer'][k] for k in ('up', 'reason', 'code', 'failures')},
                         {'up': False, 'reason': 'exit', 'code': 11, 'failures': 2})
        self.assertAlmostEqual(server['hostServer']['retryIn'], 30, delta=2)
        status, _, restart = call(self.port, 'POST', '/api/world/restart', {}, token=owner)
        self.assertEqual(status, 200, restart)
        self.assertEqual([p.read_text() for p in (self.folder.path / 'requests').glob('restart.*')], ['restart\n'])

    def test_my_world_and_its_owner_only_views(self):
        (self.folder.path / 'exits.json').write_text(json.dumps({'exits': [{'at': 1, 'code': 139, 'lines': ['from 198.51.100.4 died']}]}))
        owner = self.signed('pell', 'owner')['token']
        fern = self.signed('fern', 'dm')['token']
        status, _, mine = call(self.port, 'GET', '/api/world', token=owner)
        self.assertEqual(status, 200, mine)
        self.assertEqual(mine['settings'], {'player_limit': 40, 'workers': 2, 'cheats': 'off', 'editor': {'enabled': True}})
        self.assertEqual((mine['owner'], mine['folder'], mine['exits'][0]['lines']),
                         (True, 'C:\\Users\\pell\\worlds\\ashfall', ['from [address] died']))
        self.assertEqual(mine['fingerprint'], A.fingerprint(A.cert_spki(self.folder.path / 'editor-cert.pem')))
        self.assertEqual({s['account'] for s in mine['signIns'] if s['detail'].startswith('Signed in')}, {'pell', 'fern'})
        self.assertIn('Signed in from this computer.', {s['detail'] for s in mine['signIns']}, 'loopback reads "this computer"')
        long_path = '/home/pell/Documents/' + 'worlds-of-the-long-valley-and-its-wolves-2026' + '/ashfall'
        self.write_settings(folder=long_path)
        self.assertEqual(call(self.port, 'GET', '/api/world', token=owner)[2]['folder'], long_path,
                         'the owner\'s own path is never scrubbed (R1, M-d)')
        status, _, theirs = call(self.port, 'GET', '/api/world', token=fern)
        self.assertEqual(status, 200, theirs)
        self.assertFalse(theirs['owner'])
        self.assertNotIn('folder', theirs)
        self.assertNotIn('exits', theirs)
        self.assertEqual({(s['account'], s['detail']) for s in theirs['signIns']}, {('fern', 'Signed in.')},
                         'only their own sign-ins, without addresses')
        for secret in ('rh_', 'npc-ai', 'api_key'):
            self.assertNotIn(secret, json.dumps(mine))
        # M5: the backups and the history (everyone; account changes the owner's alone, no address in others' details).
        self.assertIsNone(mine['backups'], 'no backup service: none')
        (self.folder.path / 'backups.json').write_text(json.dumps({'v': 1, 'at': 1, 'backups': [
            {'name': 'ratw-20261112T140300Z-daily', 'at': 100, 'size': 5}]}))
        owner_sql("""INSERT INTO dm.audit (at, action, target, detail, who) VALUES
                     (1, 'account.add', 'fern', 'Added from 192.0.2.7.', 'pell#abc123'),
                     (2, 'server.maintenance', 'world', 'Closed by pell from 192.0.2.7.', 'pell#abc123')""")
        mine = call(self.port, 'GET', '/api/world', token=owner)[2]
        theirs = call(self.port, 'GET', '/api/world', token=fern)[2]
        self.assertEqual((mine['backups']['last']['name'], theirs['backups']['last']['name']), ('ratw-20261112T140300Z-daily',) * 2)
        self.assertIn('account.add', {h['action'] for h in mine['history']})
        self.assertNotIn('account.add', {h['action'] for h in theirs['history']})
        self.assertFalse(any(h['action'].startswith('login') for h in mine['history']), 'sign-ins are their own list')
        self.assertIn('Closed by pell from [address].', {h['detail'] for h in theirs['history']})
        self.assertIn('wolves', mine)
        # M9: Share my world (the state for everyone; the request the owner's, kind-only, and in the history).
        self.assertIsNone(mine['share'], 'an older package: no share.json')
        (self.folder.path / 'share.json').write_text(json.dumps({'v': 1, 'state': 'ready', 'at': 5, 'recent': [],
                                                                 'file': 'ashfall-2026-11-12.ratwpkg', 'size': 7,
                                                                 'files': [{'name': 'ashfall-2026-11-12.ratwpkg', 'size': 7, 'at': 5}]}))
        self.assertEqual(call(self.port, 'GET', '/api/world', token=fern)[2]['share']['file'], 'ashfall-2026-11-12.ratwpkg')
        self.assertEqual(call(self.port, 'POST', '/api/world/share', {}, token=fern)[0], 403)
        status, _, asked = call(self.port, 'POST', '/api/world/share', {}, token=owner)
        self.assertEqual((status, asked), (200, {'ok': True, 'asked': True}))
        self.assertIn('share', {p.name.split('.')[0] for p in (self.folder.path / 'requests').iterdir()})
        self.assertEqual(call(self.port, 'POST', '/api/world/share', {}, token=owner)[0], 409, 'one at a time')
        self.assertIn('server.share', {h['action'] for h in call(self.port, 'GET', '/api/world', token=owner)[2]['history']})

    def test_the_ai_without_a_key_its_caps_and_the_month_s_budget(self):
        dm = self.signed('fern', 'dm')['token']
        status, _, ai = call(self.port, 'GET', '/api/studio/ai', token=dm)
        self.assertEqual((status, ai['available'], ai['reason']), (200, False, 'Needs an AI key in world.toml.'))
        status, _, refused = call(self.port, 'POST', '/api/dm/chronicle/story', {'id': 'npc_bram'}, token=dm)
        self.assertEqual((status, refused['error']), (503, 'Needs an AI key in world.toml.'))
        (self.folder.path / 'npc-ai.json').write_text('{"enabled": true, "api_key": "sk-test-not-real"}')
        status, _, ai = call(self.port, 'GET', '/api/studio/ai', token=dm)
        self.assertNotIn('sk-test', json.dumps(ai), 'no route returns the config')
        with mock.patch.dict(os.environ, {'RATW_AI': 'fixture'}):
            hosted = self.core.app.ai
            self.assertTrue(hosted.state()['available'])
            for _ in range(self.studio_module.AI_CAPS['story']):
                hosted.record('story', 'fern', 'fixture', 1, 10, 10)
            status, _, capped = call(self.port, 'POST', '/api/dm/chronicle/story', {'id': 'npc_bram'}, token=dm)
            self.assertEqual(status, 429, capped)
            self.assertEqual(hosted.state()['today'], {'story': 100, 'roster': 0})
            self.write_settings(ai={'monthly_tokens': 5000})
            self.assertTrue(hosted.state()['available'])
            (self.folder.path / 'mind-month.json').write_text(json.dumps({'month': time.strftime('%Y-%m', time.gmtime()), 'tokens': 3000}))
            state = hosted.state()
            self.assertEqual((state['available'], state['month']), (False, {'tools': 2000, 'mind': 3000, 'budget': 5000}))
            self.assertIn('budget', state['reason'])
            (self.folder.path / 'mind-month.json').write_text(json.dumps({'month': '2000-01', 'tokens': 99999}))
            self.assertTrue(hosted.state()['available'], 'another month\'s count is not this one\'s')
        self.assertEqual(owner_sql('SELECT count(*) FROM dm.ai_use')[0][0], 100)

    def test_large_replies_go_gzipped_to_a_client_that_takes_it(self):
        import gzip
        token = self.signed()['token']
        big = json.dumps({'cells': ['x' * 100] * 400}).encode()
        self.core.app = lambda *args: (200, 'application/json', big)
        status, headers, body = call(self.port, 'GET', '/api/dm/world', token=token, headers={'Accept-Encoding': 'gzip'})
        self.assertEqual((status, headers.get('content-encoding')), (200, 'gzip'))
        self.assertEqual(gzip.decompress(body), big)
        status, headers, body = call(self.port, 'GET', '/api/dm/world', token=token)
        self.assertEqual((headers.get('content-encoding'), body), (None, json.loads(big)))


@unittest.skipUnless(DB and OPENSSL, 'local PostgreSQL or openssl missing')
class Process(unittest.TestCase):
    """The real `serve` process: listens, reloads its key on SIGHUP, says so in the hello, and stops on SIGTERM."""

    def setUp(self):
        empty_accounts()
        self.folder = Folder()
        self.temp = tempfile.TemporaryDirectory(prefix='ratw-editor-registry-')
        registry = Path(self.temp.name) / 'worlds.json'
        registry.write_text(json.dumps({'worlds': [{'key': 'world', 'name': WORLD_NAME, 'database': SCRATCH, 'kind': 'live'}]}))
        self.env = {**os.environ, 'RATW_WORLDS_FILE': str(registry),
                    'RATW_WORLDS_LOCAL': str(Path(self.temp.name) / 'worlds.local.json'), 'RATW_EDITOR_PEERS': 'auto'}

    def tearDown(self):
        self.folder.cleanup()
        self.temp.cleanup()

    def cli(self, *args):
        return subprocess.run([sys.executable, str(TOOLS / 'world_admin.py'), '--admin-dir', str(self.folder.path), *args],
                              env=self.env, capture_output=True, text=True, timeout=60)

    def test_serve_reload_and_stop(self):
        with socket.socket() as probe:
            probe.bind(('127.0.0.1', 0))
            port = probe.getsockname()[1]
        process = subprocess.Popen([sys.executable, str(TOOLS / 'world_admin.py'), '--admin-dir', str(self.folder.path),
                                    'serve', '--host', '127.0.0.1', '--port', str(port)], env=self.env,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            for _ in range(100):
                if (self.folder.path / 'helper.pid').exists():
                    break
                time.sleep(0.1)
            old = A.cert_spki(self.folder.path / 'editor-cert.pem')
            with tls_socket(port) as sock:
                self.assertEqual(A.spki_of(sock.getpeercert(binary_form=True)), old)
            self.assertNotIn('rotation', call(port, 'GET', '/editor/hello')[2])
            done = self.cli('new-certificate')
            self.assertEqual(done.returncode, 0, done.stderr)
            self.assertIn('Signed by the old key', done.stdout)
            time.sleep(0.5)
            with tls_socket(port) as sock:
                new = A.spki_of(sock.getpeercert(binary_form=True))
            self.assertNotEqual(new, old)
            hello = call(port, 'GET', '/editor/hello')[2]
            self.assertEqual(base64.b64decode(hello['rotation']['spki']), new)
            self.assertEqual(hello['rotation']['previous'], A.pin(old))
            process.send_signal(signal.SIGTERM)
            self.assertEqual(process.wait(15), 0)
            output = process.stdout.read()
            self.assertIn('RATW_EDITOR listening', output)
            self.assertIn(f'port={port}', output)
            self.assertIn('RATW_EDITOR certificate.reloaded', output)
            self.assertFalse((self.folder.path / 'helper.pid').exists())
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            process.stdout.close()

    def test_with_the_editor_off_nothing_listens(self):
        self.folder.settings(enabled=False)
        done = self.cli('serve', '--host', '127.0.0.1', '--port', '0')
        self.assertEqual(done.returncode, 0)
        self.assertIn('RATW_EDITOR disabled', done.stdout)
        self.assertFalse((self.folder.path / 'helper.pid').exists())

    def test_ratw_editor(self):
        done = self.cli('add', 'ferro', '--role', 'owner', '--address', '192.168.1.20:17789')
        self.assertEqual(done.returncode, 0, done.stderr)
        code = done.stdout.strip().splitlines()[-1]
        record = A.read_code(code)
        self.assertEqual((record['u'], record['a']), ('ferro', '192.168.1.20:17789'))
        self.assertEqual(owner_sql("SELECT role, must_change FROM dm.admins WHERE username = 'ferro'"), [('owner', True)])
        self.assertIn('ferro', self.cli('list').stdout)
        removed = self.cli('remove', 'ferro')
        self.assertEqual(removed.returncode, 1)
        self.assertIn("last owner can't be removed", removed.stderr)
        self.assertEqual(self.cli('disable', 'ferro').returncode, 1)
        # Locked by strangers' failures: invite and reset still work, and reset unlocks.
        owner_sql("INSERT INTO dm.locks (username, failures, level, locked_until) VALUES ('ferro', 0, 3, now() + interval '4 hours')")
        invite = self.cli('invite', 'ferro')
        self.assertEqual(invite.returncode, 0, invite.stderr)
        self.assertNotIn('One-time password', invite.stdout)
        self.assertIn('i', A.read_code(invite.stdout.strip().splitlines()[-1]))
        self.assertEqual(self.cli('reset', 'ferro').returncode, 0)
        self.assertEqual(owner_sql('SELECT count(*) FROM dm.locks')[0][0], 0)
        # reset-editor-password's: the code shown, and written into manage/first-editor.txt with this computer's
        # address and the host's port (M5).
        manage = self.folder.path / 'manage'
        manage.mkdir()
        written = subprocess.run([sys.executable, str(TOOLS / 'world_admin.py'), '--admin-dir', str(self.folder.path), 'reset', 'ferro',
                                  '--write-file'], env={**self.env, 'RATW_MANAGE_DIR': str(manage), 'RATW_EDITOR_PORT': '17790'},
                                 capture_output=True, text=True, timeout=60)
        self.assertEqual(written.returncode, 0, written.stderr)
        text = (manage / 'first-editor.txt').read_text()
        shown = written.stdout.strip().splitlines()
        self.assertIn('Your Editor account: ferro', text)
        self.assertIn(next(l for l in shown if l.startswith('ratw-editor:')), text)
        self.assertEqual(A.read_code(next(l for l in shown if l.startswith('ratw-editor:')))['a'], '127.0.0.1:17790')
        self.assertIn('made by reset-editor-password', text)
        self.assertEqual(oct((manage / 'first-editor.txt').stat().st_mode & 0o777), '0o600')
        self.assertEqual(self.cli('reset', 'nobody', '--write-file').returncode, 1)
        self.assertEqual(self.cli('add', 'root', '--role', 'admin').returncode, 2, 'never admin')
        owner_sql("INSERT INTO dm.sessions (token_hash, username, expires_at) VALUES ('t', 'ferro', now() + interval '1 hour')")
        self.assertEqual(self.cli('sign-out-all').returncode, 0)
        self.assertEqual(owner_sql('SELECT count(*) FROM dm.sessions')[0][0], 0)
        fingerprint = self.cli('fingerprint').stdout
        self.assertIn(A.fingerprint(A.cert_spki(self.folder.path / 'editor-cert.pem')), fingerprint)
        owner_sql("INSERT INTO dm.sessions (token_hash, username, expires_at) VALUES ('t2', 'ferro', now() + interval '1 hour')")
        (manage / 'editor-certificate.txt').write_text('the old fingerprint')
        leaked = subprocess.run([sys.executable, str(TOOLS / 'world_admin.py'), '--admin-dir', str(self.folder.path),
                                 'new-certificate', '--leaked'], env={**self.env, 'RATW_MANAGE_DIR': str(manage)},
                                capture_output=True, text=True, timeout=60)
        self.assertEqual(leaked.returncode, 0, leaked.stderr)
        self.assertIn('Every Editor session has ended', leaked.stdout)
        self.assertEqual(owner_sql('SELECT count(*) FROM dm.sessions')[0][0], 0)
        # manage/editor-certificate.txt says the new fingerprint at once, not after the next start (M6's run).
        self.assertIn(A.fingerprint(A.cert_spki(self.folder.path / 'editor-cert.pem')), (manage / 'editor-certificate.txt').read_text())
        self.assertNotIn(fingerprint.split('fingerprint: ')[1].split('\n')[0], (manage / 'editor-certificate.txt').read_text())

    def test_first_owner_once_and_never_on_an_official_world(self):
        made = json.loads(self.cli('first-owner').stdout)
        self.assertRegex(made['username'], r'^host-[a-z0-9]{4}$')
        record = A.read_code(made['code'])
        self.assertEqual((record['p'], record['i'], made['days']), (made['password'], made['invite'], 7))
        self.assertEqual(record['a'], '127.0.0.1:17789', 'the code carries this computer\'s address (R1, M-c)')
        self.assertIsNone(json.loads(self.cli('first-owner').stdout))
        empty_accounts()
        self.env['RATW_EDITOR_PORT'] = '17811'                         # (A second world's port, from .env.)
        self.assertEqual(A.read_code(json.loads(self.cli('first-owner').stdout)['code'])['a'], '127.0.0.1:17811')
        del self.env['RATW_EDITOR_PORT']
        empty_accounts()
        (self.folder.path / 'central.json').write_text(json.dumps({'kind': 'official'}))
        self.assertIsNone(json.loads(self.cli('first-owner').stdout))
        self.assertEqual(owner_sql('SELECT count(*) FROM dm.admins')[0][0], 0)


if __name__ == '__main__':
    unittest.main()
