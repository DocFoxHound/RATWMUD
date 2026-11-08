#!/usr/bin/env python3
"""Central's tests (Docs/Design/77-central-accounts-and-hosted-worlds.md, Phases 1-4; section 11).

    python3 Central/test_central.py            (or: python3 -m unittest discover -s Central)

They run against a throwaway database, `ratw_central_test` (or RATW_CENTRAL_TEST_DB), made on the local Postgres
(Database/compose.yaml) with the superuser from Database/.env and dropped at the end; no world's database is touched,
and no role is made or changed. They need a built ratw_identity (RATW_IDENTITY, else build-core/, build-central/ or
Build/). Time is a fake clock, and passwords use 1,000 PBKDF2 rounds (one test checks the real 600,000). The server
list's tests run stand-in worlds' /status on loopback, with central listing local worlds (serve --local-worlds).
"""
from __future__ import annotations

import base64
import datetime as dt
import hashlib
import hmac
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ipaddress
import io
import json
import os
from pathlib import Path
import secrets
import socket
import stat
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import central_identity  # noqa: E402
import central_service as cs  # noqa: E402
import central_store  # noqa: E402
import central_tickets as ct  # noqa: E402
import central_words  # noqa: E402
import ratw_central  # noqa: E402

TEST_DB = os.environ.get('RATW_CENTRAL_TEST_DB', 'ratw_central_test')
START = dt.datetime(2026, 11, 7, 12, 0, tzinfo=dt.timezone.utc).timestamp()
DAY = 86400
LOOK = {'species': 'timber', 'sex': 'female', 'stature': 'average', 'pattern': 'solid', 'baseColor': 3,
        'gradientColor': 1, 'markingColor': 2, 'gradientAmount': 0.4, 'patternAmount': 0.2}
PASSWORD = 'correct horse battery'


class Clock:
    def __init__(self, now=START):
        self.now = now

    def __call__(self):
        return self.now

    def advance(self, seconds):
        self.now += seconds


STORE = IDENTITY = None


def setUpModule():
    global STORE, IDENTITY
    central_store.drop_database(TEST_DB)
    central_store.create_database(TEST_DB)
    STORE = central_store.Store(TEST_DB)
    STORE.migrate()
    IDENTITY = central_identity.Identity()


def tearDownModule():
    if IDENTITY:
        IDENTITY.close()
    if STORE:
        STORE.close()
    central_store.drop_database(TEST_DB)


def wipe():
    with STORE.connection() as conn:
        conn.execute('TRUNCATE central.accounts, central.sessions, central.spent_refresh, central.wolves, '
                     'central.reserved_names, central.deleted_wolves, central.worlds, central.tickets CASCADE')


class Base(unittest.TestCase):
    limits = None

    def setUp(self):
        wipe()
        self.clock = Clock()
        self.keydir = tempfile.TemporaryDirectory()
        self.keys = ct.KeyRing(Path(self.keydir.name))
        self.keys.maintain(self.clock())
        self.central = cs.Central(STORE, self.keys, IDENTITY, clock=self.clock,
                                  settings=cs.Settings(iterations=1000, limits=self.limits or cs.Limits()))
        self.address = 0

    def tearDown(self):
        self.keydir.cleanup()

    def ip(self):
        self.address += 1
        return f'198.51.100.{self.address}'

    def call(self, method, body=None, token=None, address=None, *extra):
        return self.central.call(getattr(self.central, method), address or self.ip(), token, body or {}, *extra)

    def register(self, username='ash', password=PASSWORD, **more):
        status, reply = self.call('register', {'username': username, 'password': password, 'birth_year': 1990, **more})
        self.assertEqual(status, 200, reply)
        return reply

    def wolf(self, token, name='Ash Tallow', **more):
        status, reply = self.call('create_wolf', {'name': name, 'age': 7, 'appearance': LOOK, **more}, token)
        self.assertEqual(status, 200, reply)
        return reply['wolf']

    def world(self, kind='community', name='Greyfen'):
        import secrets
        world_id = 'world-' + secrets.token_hex(16)
        STORE.add_world(world_id, name, kind, self.clock())
        return world_id


# --------------------------------------------------------------------------- The rules alone

class Rules(unittest.TestCase):
    def test_usernames_match_the_worlds_accounts(self):
        self.assertEqual(cs.normalize_username('Ash_Tallow-2'), 'ash_tallow-2')
        for bad in ('ab', 'a' * 33, '2ash', '_ash', 'ash tallow', 'äsh', 'ash!', '', None, 7):
            self.assertIsNone(cs.normalize_username(bad), bad)
        self.assertEqual(cs.normalize_username('abc'), 'abc')
        self.assertEqual(cs.normalize_username('a' * 32), 'a' * 32)

    def test_passwords_match_the_worlds_accounts(self):
        self.assertTrue(cs.valid_password('x' * 12))
        self.assertTrue(cs.valid_password('x' * 128))
        self.assertFalse(cs.valid_password('x' * 11))
        self.assertFalse(cs.valid_password('x' * 129))
        self.assertFalse(cs.valid_password('twelve chars\n'))
        self.assertFalse(cs.valid_password('twelve chars\x7f'))
        self.assertTrue(cs.valid_password('ü' * 6))            # 12 bytes
        self.assertFalse(cs.valid_password('ü' * 65))          # 130 bytes
        self.assertFalse(cs.valid_password('\ud800' * 12))     # a lone surrogate
        self.assertFalse(cs.valid_password(None))

    def test_names_compare_folded(self):
        self.assertEqual(cs.name_key('Ash  Tallow'), cs.name_key('ash tallow'))
        self.assertEqual(cs.name_key('ＡＳＨ'), cs.name_key('ash'))   # full-width letters
        self.assertNotEqual(cs.name_key('Ash'), cs.name_key('Asha'))
        for hidden in ('A\u200bsh', 'A\u00adsh', 'As\ufe0fh', 'A\u2060sh', 'Ash\u200d', 'A\u034fsh'):
            self.assertEqual(cs.name_key(hidden), cs.name_key('Ash'), repr(hidden))

    def test_the_age_gate(self):
        today = dt.date(2026, 11, 7)
        self.assertTrue(cs.old_enough(1990, None, today))
        self.assertTrue(cs.old_enough(2009, None, today))          # 17 this year
        self.assertFalse(cs.old_enough(2011, None, today))         # 15 this year
        self.assertFalse(cs.old_enough(2026, None, today))
        self.assertIsNone(cs.old_enough(2010, None, today))        # 16 this year: the month decides
        self.assertTrue(cs.old_enough(2010, 11, today))
        self.assertFalse(cs.old_enough(2010, 12, today))
        for bad in (1899, 2027, '1990', 1990.0, True, None):
            self.assertIsNone(cs.old_enough(bad, None, today), bad)
        self.assertIsNone(cs.old_enough(2010, 13, today))

    def test_password_hashing_is_todays_pbkdf2(self):
        record = cs.hash_password(PASSWORD)
        self.assertEqual((record['algorithm'], record['iterations']), ('pbkdf2-sha256', 600_000))
        self.assertEqual((len(bytes.fromhex(record['salt'])), len(bytes.fromhex(record['hash']))), (32, 32))
        expected = hashlib.pbkdf2_hmac('sha256', PASSWORD.encode(), bytes.fromhex(record['salt']), 600_000, 32)
        self.assertEqual(record['hash'], expected.hex())
        self.assertTrue(cs.password_matches(record, PASSWORD))
        self.assertFalse(cs.password_matches(record, PASSWORD + '!'))
        self.assertFalse(cs.password_matches(None, PASSWORD, 1000))
        self.assertFalse(cs.password_matches({'algorithm': 'pbkdf2-sha256', 'salt': 'zz'}, PASSWORD, 1000))


# --------------------------------------------------------------------------- Accounts

class Accounts(Base):
    def test_register_gives_tokens_and_the_account(self):
        reply = self.register('Ash', remember=True)
        self.assertEqual(reply['account']['username'], 'ash')
        self.assertRegex(reply['account']['id'], r'^acct-[0-9a-f]{32}$')
        self.assertEqual(reply['access_expires'], self.clock() + 15 * 60)
        self.assertEqual(reply['refresh_expires'], self.clock() + 30 * DAY)
        status, me = self.call('me', token=reply['access_token'])
        self.assertEqual((status, me['account']['username']), (200, 'ash'))
        with STORE.connection() as conn:
            stored = conn.execute('SELECT access_hash, refresh_hash FROM central.sessions').fetchone()
            password = conn.execute('SELECT password FROM central.accounts').fetchone()[0]
        self.assertEqual(stored, (cs.token_hash(reply['access_token']), cs.token_hash(reply['refresh_token'])),
                         'tokens are kept only as hashes')
        self.assertNotIn(PASSWORD, json.dumps(password))
        self.assertTrue(cs.password_matches(password, PASSWORD))

    def test_register_refusals(self):
        self.register('ash')
        cases = [({'username': 'ASH', 'password': PASSWORD, 'birth_year': 1990}, 409, 'taken'),
                 ({'username': 'a!', 'password': PASSWORD, 'birth_year': 1990}, 400, 'invalid'),
                 ({'username': 'birch', 'password': 'short', 'birth_year': 1990}, 400, 'invalid'),
                 ({'username': 'birch', 'password': PASSWORD}, 400, 'birth_year'),
                 ({'username': 'birch', 'password': PASSWORD, 'birth_year': 2012}, 403, 'too_young'),
                 ({'username': 'birch', 'password': PASSWORD, 'birth_year': 2010}, 400, 'birth_year'),
                 ({'username': 'birch', 'password': PASSWORD, 'birth_year': 2010, 'birth_month': 12}, 403,
                  'too_young')]
        for body, status, code in cases:
            got, reply = self.call('register', body)
            self.assertEqual((got, reply['code']), (status, code), body)
        got, reply = self.call('register', {'username': 'birch', 'password': PASSWORD, 'birth_year': 2010,
                                            'birth_month': 11})
        self.assertEqual(got, 200, 'sixteen this month')
        with STORE.connection() as conn:
            columns = [r[0] for r in conn.execute("SELECT column_name FROM information_schema.columns "
                                                  "WHERE table_schema = 'central' AND table_name = 'accounts'")]
        self.assertFalse([c for c in columns if 'birth' in c], 'the birth year is never kept')

    def test_login(self):
        self.register('ash')
        status, reply = self.call('login', {'username': 'Ash', 'password': PASSWORD})
        self.assertEqual(status, 200, reply)
        self.assertEqual(reply['refresh_expires'], self.clock() + 12 * 3600, 'without remember me: 12 hours')
        wrong = self.call('login', {'username': 'ash', 'password': PASSWORD + 'x'})
        nobody = self.call('login', {'username': 'nobody', 'password': PASSWORD})
        self.assertEqual(wrong, nobody, 'a wrong password and a missing user look the same')
        self.assertEqual(wrong[0], 401)

    def test_a_banned_account_cannot_sign_in_or_use_its_tokens(self):
        reply = self.register('ash')
        STORE.set_flags(reply['account']['id'], banned=True)
        self.assertEqual(self.call('login', {'username': 'ash', 'password': PASSWORD})[1]['code'], 'banned')
        self.assertEqual(self.call('list_wolves', token=reply['access_token'])[0], 403)
        self.assertEqual(self.call('refresh', {'refresh_token': reply['refresh_token']})[0], 403)

    def test_access_tokens_last_fifteen_minutes(self):
        token = self.register('ash')['access_token']
        self.clock.advance(15 * 60 - 1)
        self.assertEqual(self.call('me', token=token)[0], 200)
        self.clock.advance(1)
        self.assertEqual(self.call('me', token=token)[1]['code'], 'unauthenticated')
        self.assertEqual(self.call('me', token=None)[0], 401)
        self.assertEqual(self.call('me', token='made-up')[0], 401)

    def test_refresh_rotates_and_a_reused_token_ends_the_session(self):
        first = self.register('ash', remember=True)
        self.clock.advance(20 * 60)
        status, second = self.call('refresh', {'refresh_token': first['refresh_token']})
        self.assertEqual(status, 200, second)
        self.assertNotEqual(second['refresh_token'], first['refresh_token'])
        self.assertEqual(second['refresh_expires'], self.clock() + 30 * DAY, 'remember me rolls on')
        self.assertEqual(self.call('me', token=second['access_token'])[0], 200)
        self.assertEqual(self.call('me', token=first['access_token'])[0], 401, 'the old access token is gone')
        # The first refresh token again: someone copied it. The whole session ends.
        status, reply = self.call('refresh', {'refresh_token': first['refresh_token']})
        self.assertEqual((status, reply['code']), (401, 'refresh_reused'))
        self.assertEqual(self.call('refresh', {'refresh_token': second['refresh_token']})[0], 401)
        self.assertEqual(self.call('me', token=second['access_token'])[0], 401)

    def test_refresh_tokens_expire(self):
        remembered = self.register('ash', remember=True)
        plain = self.call('login', {'username': 'ash', 'password': PASSWORD})[1]
        self.clock.advance(11 * 3600)
        status, plain = self.call('refresh', {'refresh_token': plain['refresh_token']})
        self.assertEqual(status, 200)
        self.assertEqual(plain['refresh_expires'], START + 12 * 3600, "a plain session doesn't roll on")
        self.clock.advance(3600)
        self.assertEqual(self.call('refresh', {'refresh_token': plain['refresh_token']})[0], 401)
        self.clock.advance(30 * DAY)
        self.assertEqual(self.call('refresh', {'refresh_token': remembered['refresh_token']})[0], 401)
        self.assertEqual(self.call('refresh', {'refresh_token': 'nonsense'})[0], 401)

    def test_logout(self):
        a = self.register('ash')
        b = self.call('login', {'username': 'ash', 'password': PASSWORD})[1]
        c = self.call('login', {'username': 'ash', 'password': PASSWORD})[1]
        self.assertEqual(self.call('logout', {}, a['access_token'])[0], 200)
        self.assertEqual(self.call('me', token=a['access_token'])[0], 401)
        self.assertEqual(self.call('refresh', {'refresh_token': a['refresh_token']})[0], 401)
        self.assertEqual(self.call('me', token=b['access_token'])[0], 200, 'other sessions stay')
        self.call('logout', {'everywhere': True}, b['access_token'])
        self.assertEqual(self.call('me', token=c['access_token'])[0], 401)


# --------------------------------------------------------------------------- Rate limits

class RateLimits(Base):
    limits = cs.Limits(register_per_address=(3, 3600), login_per_address=(5, 600),
                       login_failures_per_account=(3, 900), tickets_per_account=(4, 60), tickets_per_world=(6, 60),
                       wolves_per_account=(2, 3600))

    def test_registrations_per_address(self):
        for i in range(3):
            self.assertEqual(self.call('register', {'username': f'ash{i}', 'password': PASSWORD, 'birth_year': 1990},
                                       address='203.0.113.9')[0], 200)
        status, reply = self.call('register', {'username': 'ash9', 'password': PASSWORD, 'birth_year': 1990},
                                  address='203.0.113.9')
        self.assertEqual((status, reply['code']), (429, 'rate_limited'))
        self.assertGreater(reply['retry_after'], 3500)
        self.assertEqual(self.call('register', {'username': 'ash9', 'password': PASSWORD, 'birth_year': 1990},
                                   address='203.0.113.10')[0], 200, 'another address')
        self.clock.advance(3600)
        self.assertEqual(self.call('register', {'username': 'ash10', 'password': PASSWORD, 'birth_year': 1990},
                                   address='203.0.113.9')[0], 200, 'the window passed')

    def test_sign_ins_per_address(self):
        self.register('ash')
        for _ in range(5):
            self.call('login', {'username': 'ash', 'password': PASSWORD}, address='203.0.113.9')
        self.assertEqual(self.call('login', {'username': 'ash', 'password': PASSWORD}, address='203.0.113.9')[0], 429)

    def test_failed_sign_ins_per_account(self):
        self.register('ash')
        for _ in range(3):
            self.assertEqual(self.call('login', {'username': 'ash', 'password': 'wrong password!'})[0], 401)
        status, reply = self.call('login', {'username': 'ash', 'password': PASSWORD})
        self.assertEqual(status, 429, 'locked for a while, from any address, even with the right password')
        self.assertEqual(self.call('login', {'username': 'other', 'password': PASSWORD})[0], 401,
                         'other accounts are not affected')
        self.clock.advance(900)
        self.assertEqual(self.call('login', {'username': 'ash', 'password': PASSWORD})[0], 200)

    def test_tickets_per_account_and_per_world(self):
        world = self.world()
        tokens = []
        for name in ('ash', 'birch'):
            reply = self.register(name)
            tokens.append((reply['access_token'], self.wolf(reply['access_token'], name.title())['id']))
        token, wolf = tokens[0]
        for _ in range(4):
            self.assertEqual(self.call('ticket', {'world': world, 'wolf': wolf}, token)[0], 200)
        self.assertEqual(self.call('ticket', {'world': world, 'wolf': wolf}, token)[1]['code'], 'rate_limited')
        token, wolf = tokens[1]
        self.assertEqual(self.call('ticket', {'world': world, 'wolf': wolf}, token)[0], 200)
        self.assertEqual(self.call('ticket', {'world': world, 'wolf': wolf}, token)[0], 200)
        status, reply = self.call('ticket', {'world': world, 'wolf': wolf}, token)
        self.assertEqual((status, reply['code']), (429, 'rate_limited'), 'six a minute for this world')
        self.assertEqual(self.call('ticket', {'world': self.world(), 'wolf': wolf}, token)[0], 200, 'another world')
        self.assertEqual(STORE.tickets_issued(world, START), 6, 'central counts tickets per world')

    def test_new_wolves_per_account(self):
        token = self.register('ash')['access_token']
        self.wolf(token, 'Ash')
        self.wolf(token, 'Birch')
        self.assertEqual(self.call('create_wolf', {'name': 'Cedar', 'age': 7, 'appearance': LOOK}, token)[0], 429)

    def test_sign_ins_queue_for_the_hashing_threads(self):
        central = cs.Central(STORE, self.keys, IDENTITY, clock=self.clock,
                             settings=cs.Settings(iterations=1000, hash_threads=1, hash_wait=0.05))
        central._hashers.acquire()          # (All hashing threads busy.)
        status, reply = central.call(central.login, '203.0.113.1', None, {'username': 'ash', 'password': PASSWORD})
        self.assertEqual((status, reply['code']), (503, 'busy'))
        threading.Timer(0.02, central._hashers.release).start()
        central.settings.hash_wait = 5
        status, _ = central.call(central.login, '203.0.113.2', None, {'username': 'ash', 'password': PASSWORD})
        self.assertEqual(status, 401, 'it waited its turn and was answered')


# --------------------------------------------------------------------------- Wolves

class Wolves(Base):
    def setUp(self):
        super().setUp()
        self.token = self.register('ash')['access_token']

    def test_a_wolf_is_made_by_the_games_rules(self):
        wolf = self.wolf(self.token, '  ash tallow ', family='fire', profile={'motto': 'Onward'})
        self.assertEqual(wolf['name'], 'Ash tallow', "ratw_identity's form: trimmed, a capital first")
        self.assertEqual((wolf['sex'], wolf['age'], wolf['family']), ('female', 7, 'fire'))
        self.assertEqual(wolf['appearance']['species'], 'timber')
        self.assertEqual(wolf['profile']['motto'], 'Onward')
        self.assertRegex(wolf['id'], r'^wolf-[0-9a-f]{32}$')
        status, reply = self.call('list_wolves', token=self.token)
        self.assertEqual(([w['id'] for w in reply['wolves']], reply['slots']), ([wolf['id']], 6))
        plain = self.wolf(self.token, 'Birch')
        self.assertEqual(plain['family'], '')

    def test_refused_wolves(self):
        cases = [{'name': 'A', 'age': 7, 'appearance': LOOK},
                 {'name': 'Ash', 'age': 5, 'appearance': LOOK},
                 {'name': 'Ash', 'age': 7.5, 'appearance': LOOK},
                 {'name': 'Ash', 'age': 7, 'appearance': {**LOOK, 'sex': 'other'}},
                 {'name': 'Ash', 'age': 7, 'appearance': {**LOOK, 'species': 'domestic'}},
                 {'name': 'Ash', 'age': 7, 'appearance': {**LOOK, 'extra': 1}},
                 {'name': 'Ash', 'age': 7, 'appearance': LOOK, 'family': 'death_walker'},
                 {'name': 'Ash', 'age': 7, 'appearance': LOOK, 'tier': 'quickened'},
                 {'name': 'Ash', 'age': 7, 'appearance': LOOK, 'profile': {'glances': 'no'}},
                 {'name': 7, 'age': 7, 'appearance': LOOK}]
        for body in cases:
            status, reply = self.call('create_wolf', body, self.token)
            self.assertEqual(status, 400, (body, reply))
            self.assertTrue(reply['error'])
        self.assertEqual(self.call('list_wolves', token=self.token)[1]['wolves'], [])

    def test_names_are_unique_across_central(self):
        self.wolf(self.token, 'Ash Tallow')
        other = self.register('birch')['access_token']
        for name in ('ash tallow', 'ASH  TALLOW', 'Ash Tallow'):
            status, reply = self.call('create_wolf', {'name': name, 'age': 7, 'appearance': LOOK}, other)
            self.assertEqual((status, reply['code']), (409, 'name_taken'), name)
        self.assertFalse(self.call('name_free', {'name': 'ash tallow'}, other)[1]['free'])
        self.assertTrue(self.call('name_free', {'name': 'Ash Other'}, other)[1]['free'])

    def test_six_wolves_an_account(self):
        for i in range(6):
            self.wolf(self.token, f'Wolf {chr(65 + i)}')
        status, reply = self.call('create_wolf', {'name': 'Wolf G', 'age': 7, 'appearance': LOOK}, self.token)
        self.assertEqual((status, reply['code']), (409, 'full'))

    def test_the_look_and_profile_change_nothing_else(self):
        wolf = self.wolf(self.token)
        status, reply = self.call('patch_wolf', {'revision': 1, 'appearance': {**LOOK, 'baseColor': 5},
                                                 'profile': {'description': 'Grey and quiet.'}}, self.token, None,
                                  wolf['id'])
        self.assertEqual(status, 200, reply)
        changed = reply['wolf']
        self.assertEqual((changed['appearance']['baseColor'], changed['revision']), (5, 2))
        self.assertEqual(changed['profile']['description'], 'Grey and quiet.')
        refusals = [({'revision': 2, 'name': 'Other'}, 400, 'immutable'),
                    ({'revision': 2, 'family': 'fire'}, 400, 'immutable'),
                    ({'revision': 2, 'appearance': {**LOOK, 'sex': 'male'}}, 400, 'immutable'),
                    ({'revision': 2, 'appearance': {**LOOK, 'stature': 'giant'}}, 400, 'invalid'),
                    ({'revision': 1, 'appearance': LOOK}, 409, 'stale'),
                    ({'appearance': LOOK}, 400, 'invalid')]
        for body, status, code in refusals:
            got, reply = self.call('patch_wolf', body, self.token, None, wolf['id'])
            self.assertEqual((got, reply['code']), (status, code), body)
        other = self.register('birch')['access_token']
        self.assertEqual(self.call('patch_wolf', {'revision': 2, 'appearance': LOOK}, other, None, wolf['id'])[0], 404)
        self.assertEqual(self.call('patch_wolf', {'revision': 2}, self.token, None, 'wolf-nope')[0], 404)


# --------------------------------------------------------------------------- Tickets and keys

class Tickets(Base):
    def setUp(self):
        super().setUp()
        reply = self.register('ash')
        self.token, self.account = reply['access_token'], reply['account']['id']
        self.wolf_id = self.wolf(self.token, 'Ash Tallow', family='seer')['id']
        self.community, self.official = self.world('community'), self.world('official', 'Ridgemere')
        self.world_start = self.clock() - 3600

    def ticket(self, world=None, token=None, wolf=None):
        if token is None and self.call('me', token=self.token)[0] != 200:      # (The clock moved on: sign in again.)
            self.token = self.call('login', {'username': 'ash', 'password': PASSWORD})[1]['access_token']
        status, reply = self.call('ticket', {'world': world or self.community, 'wolf': wolf or self.wolf_id},
                                  token or self.token)
        self.assertEqual(status, 200, reply)
        return reply['ticket']

    def keyset(self):
        return ct.public_keys(self.call('jwks')[1])

    def check(self, ticket, world=None, now=None, seen=None, keys=None, start=None):
        return ct.verify(ticket, keys or self.keyset(), world or self.community, self.clock() if now is None else now,
                         self.world_start if start is None else start, {} if seen is None else seen)

    def test_a_ticket_carries_the_wolf_for_one_world(self):
        ticket = self.ticket()
        head = json.loads(base64.urlsafe_b64decode(ticket.split('.')[0] + '=='))
        self.assertEqual(set(head), {'alg', 'typ', 'kid'})
        self.assertEqual(head['alg'], 'Ed25519')
        claims = self.check(ticket)
        self.assertEqual((claims['wolf'], claims['name'], claims['sex'], claims['age'], claims['gift'], claims['kind']),
                         (self.wolf_id, 'Ash Tallow', 'female', 7, 'seer', 'community'))
        self.assertEqual(claims['look']['species'], 'timber')
        self.assertEqual(claims['exp'] - claims['iat'], 60)
        self.assertEqual(claims['aud'], self.community)
        self.assertNotIn('build', claims, 'the strengths are chosen at each first arrival (Q12 (a))')

    def test_a_ticket_in_the_second_a_key_is_made_uses_that_key(self):
        # Doc 77, Phase 2's review: signing at int(now) found no key yet in the second the first key was made, and made
        # a second one that no world had fetched.
        keydir = tempfile.TemporaryDirectory()
        self.addCleanup(keydir.cleanup)
        clock = Clock(START + 0.6)
        ring = ct.KeyRing(Path(keydir.name))
        ring.maintain(clock())
        published = ct.public_keys(ring.jwks(clock()))
        central = cs.Central(STORE, ring, IDENTITY, clock=clock, settings=cs.Settings(iterations=1000))
        clock.advance(0.2)
        status, reply = central.call(central.ticket, '198.51.100.9', self.token, {'world': self.community,
                                                                                  'wolf': self.wolf_id})
        self.assertEqual(status, 200, reply)
        self.assertEqual(len(ring.describe(clock())), 1, 'no second key was made')
        ct.verify(reply['ticket'], published, self.community, clock(), clock() - 10, {})

    def test_community_worlds_get_pairwise_account_ids(self):
        a1 = self.check(self.ticket(self.community))['sub']
        a2 = self.check(self.ticket(self.community))['sub']
        other = self.world('community', 'Other')
        b = self.check(self.ticket(other), world=other)['sub']
        official = self.check(self.ticket(self.official), world=self.official)
        self.assertEqual(a1, a2, 'stable in one world')
        self.assertNotEqual(a1, b, 'unlinkable across worlds')
        self.assertNotEqual(a1, self.account)
        self.assertRegex(a1, r'^acct-[0-9a-f]{32}$')
        self.assertEqual((official['sub'], official['kind']), (self.account, 'official'))

    def test_a_world_refuses_bad_tickets(self):
        ticket = self.ticket()
        seen = {}
        self.check(ticket, seen=seen)
        with self.assertRaisesRegex(ct.TicketError, 'already used'):
            self.check(ticket, seen=seen)
        with self.assertRaisesRegex(ct.TicketError, 'another world'):
            self.check(self.ticket(), world=self.official)
        late = self.ticket()
        self.check(late, now=self.clock() + 60 + 120)                   # (inside the 2 minutes' leeway)
        with self.assertRaisesRegex(ct.TicketError, 'expired'):
            self.check(late, now=self.clock() + 60 + 121)
        with self.assertRaisesRegex(ct.TicketError, 'before this world started'):
            self.check(self.ticket(), start=self.clock() + 121)
        self.check(self.ticket(), now=self.clock() - 90, start=self.clock() + 90)   # a world clock 90 s off
        head, body, sig = self.ticket().split('.')
        claims = json.loads(base64.urlsafe_b64decode(body + '=='))
        forged = ct.b64u(ct.compact({**claims, 'name': 'Someone Else'}))
        with self.assertRaisesRegex(ct.TicketError, 'bad signature'):
            self.check(f'{head}.{forged}.{sig}')
        kid = json.loads(base64.urlsafe_b64decode(head + '=='))['kid']
        for alg in ('none', 'EdDSA', 'ed25519'):
            other = ct.b64u(ct.compact({'alg': alg, 'typ': 'JWT', 'kid': kid}))
            with self.assertRaisesRegex(ct.TicketError, 'wrong algorithm'):
                self.check(f'{other}.{body}.{sig if alg != "none" else ""}')
        hs = ct.b64u(ct.compact({'alg': 'HS256', 'typ': 'JWT', 'kid': kid}))
        mac = hmac.new(self.keyset()[kid], f'{hs}.{body}'.encode(), hashlib.sha256).digest()
        with self.assertRaisesRegex(ct.TicketError, 'wrong algorithm'):
            self.check(f'{hs}.{body}.{ct.b64u(mac)}')
        doubled = ct.b64u(b'{"alg":"Ed25519","alg":"none","typ":"JWT","kid":"' + kid.encode() + b'"}')
        with self.assertRaisesRegex(ct.TicketError, 'repeated'):
            self.check(f'{doubled}.{body}.{sig}')
        for broken in (f'{head}.{body}', f'{head}.{body}.{sig}.AAAA', f'{head}=.{body}.{sig}', 'nonsense'):
            with self.assertRaises(ct.TicketError):
                self.check(broken)

    def test_tickets_only_for_open_worlds_own_wolves_and_good_accounts(self):
        import secrets
        cases = [({'world': 'world-' + secrets.token_hex(16), 'wolf': self.wolf_id}, 404, 'no_world'),
                 ({'world': 'greyfen', 'wolf': self.wolf_id}, 400, 'invalid'),
                 ({'world': self.community, 'wolf': 'wolf-' + secrets.token_hex(16)}, 404, 'not_found')]
        for body, status, code in cases:
            got, reply = self.call('ticket', body, self.token)
            self.assertEqual((got, reply['code']), (status, code), body)
        other = self.register('birch')['access_token']
        self.assertEqual(self.call('ticket', {'world': self.community, 'wolf': self.wolf_id}, other)[0], 404,
                         "another account's wolf")
        with STORE.connection() as conn:
            conn.execute('UPDATE central.worlds SET delisted = true WHERE id = %s', (self.community,))
        self.assertEqual(self.call('ticket', {'world': self.community, 'wolf': self.wolf_id}, self.token)[0], 404)
        STORE.set_flags(self.account, banned=True)
        self.assertEqual(self.call('ticket', {'world': self.official, 'wolf': self.wolf_id}, self.token)[0], 403)
        self.assertEqual(self.call('ticket', {'world': self.official, 'wolf': self.wolf_id}, None)[0], 401)

    def test_key_rotation(self):
        old_kid = self.keys.describe(self.clock())[0]['kid']
        world_keys = self.keyset()                       # (What a world fetched at its start.)
        old_ticket = self.ticket()
        self.clock.advance(28 * DAY)
        self.central.maintain()
        self.assertEqual(len(self.keyset()), 1, 'not yet')
        self.clock.advance(DAY)
        self.central.maintain()
        published = self.keyset()
        self.assertEqual(len(published), 2, 'the next key is published a day before it signs')
        self.assertEqual(self.check(self.ticket(), keys=world_keys)['wolf'], self.wolf_id, 'still the old key')
        self.clock.advance(DAY)
        self.central.maintain()
        new_ticket = self.ticket()
        new_kid = json.loads(base64.urlsafe_b64decode(new_ticket.split('.')[0] + '=='))['kid']
        self.assertNotEqual(new_kid, old_kid)
        self.assertIn(new_kid, published, 'a world that fetched /keys the day before knows it')
        # A world meeting a key id it doesn't know fetches /keys again, and then admits it.
        with self.assertRaisesRegex(ct.TicketError, 'unknown key'):
            self.check(new_ticket, keys=world_keys)
        self.check(new_ticket, keys=self.keyset())
        self.clock.advance(30)
        self.assertIn(old_kid, self.keyset(), 'the old key stays published for its grace')
        self.clock.advance(ct.GRACE)
        self.central.maintain()
        self.assertEqual(list(self.keyset()), [new_kid])
        with self.assertRaisesRegex(ct.TicketError, 'unknown key|expired'):
            self.check(old_ticket)
        self.assertFalse((Path(self.keydir.name) / f'key-{old_kid}.json').exists(), 'its private half is gone')

    def test_keys_survive_a_restart_and_are_private(self):
        ticket = self.ticket()
        again = ct.KeyRing(Path(self.keydir.name))
        self.assertEqual(again.jwks(self.clock()), self.keys.jwks(self.clock()))
        self.check(ticket, keys=ct.public_keys(again.jwks(self.clock())))
        self.assertEqual(again.pairwise_secret(), self.keys.pairwise_secret())
        folder = Path(self.keydir.name)
        self.assertEqual(stat.S_IMODE(folder.stat().st_mode), 0o700)
        for path in folder.iterdir():
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600, path.name)
        self.assertNotIn('private', json.dumps(self.call('jwks')[1]))
        self.assertNotIn('"d"', json.dumps(self.call('jwks')[1]))

    def test_an_emergency_rotation_revokes_every_ticket_out(self):
        ticket = self.ticket()
        kid = self.keys.rotate(self.clock(), immediate=True)
        self.assertEqual(list(self.keyset()), [kid])
        with self.assertRaisesRegex(ct.TicketError, 'unknown key'):
            self.check(ticket)
        self.check(self.ticket())


# --------------------------------------------------------------------------- Deletion

class Deletion(Base):
    def test_deletion_removes_the_account_and_reserves_its_names(self):
        reply = self.register('ash', remember=True)
        token, account = reply['access_token'], reply['account']['id']
        wolves = [self.wolf(token, 'Ash Tallow')['id'], self.wolf(token, 'Birch Fen')['id']]
        self.assertEqual(self.call('delete_account', {'password': 'wrong password!'}, token)[0], 401)
        status, gone = self.call('delete_account', {'password': PASSWORD}, token)
        self.assertEqual(status, 200, gone)
        self.assertEqual(sorted(gone['wolves']), sorted(wolves))
        self.assertIsNone(STORE.account(account))
        self.assertTrue(all(STORE.wolf(w) is None for w in wolves))
        self.assertEqual(self.call('me', token=token)[0], 401)
        self.assertEqual(self.call('refresh', {'refresh_token': reply['refresh_token']})[0], 401)
        self.assertEqual(self.call('login', {'username': 'ash', 'password': PASSWORD})[0], 401)
        self.assertEqual(sorted(STORE.deleted_wolves()), sorted(wolves), 'for the worlds to forget')
        # Held back for 30 days: the username, and the wolves' names.
        self.assertEqual(self.call('register', {'username': 'ash', 'password': PASSWORD, 'birth_year': 1990})[0], 409)
        other = self.register('cedar')['access_token']
        self.assertEqual(self.call('create_wolf', {'name': 'ash tallow', 'age': 7, 'appearance': LOOK}, other)[1]['code'],
                         'name_taken')
        self.assertFalse(self.call('name_free', {'name': 'Birch Fen'}, other)[1]['free'])
        self.clock.advance(30 * DAY)
        self.central.maintain()
        other = self.call('login', {'username': 'cedar', 'password': PASSWORD})[1]['access_token']
        self.wolf(other, 'Ash Tallow')
        self.register('ash')
        with STORE.connection() as conn:
            self.assertEqual(conn.execute('SELECT count(*) FROM central.reserved_names').fetchone()[0], 0)

    def test_one_wolf_goes_and_the_account_stays(self):
        # The user, 2026-11-07: a wolf may be deleted without deleting the account.
        token = self.register('ash')['access_token']
        ash, birch = self.wolf(token, 'Ash Tallow')['id'], self.wolf(token, 'Birch Fen')['id']
        other = self.register('cedar')['access_token']
        cedars = self.wolf(other, 'Cedar')['id']
        self.assertEqual(self.call('delete_wolf', {'password': 'wrong password!'}, token, None, ash)[0], 401)
        self.assertEqual(self.call('delete_wolf', {}, token, None, ash)[0], 401, 'the password is asked')
        self.assertEqual(self.call('delete_wolf', {'password': PASSWORD}, None, None, ash)[0], 401, 'signed in')
        self.assertEqual(self.call('delete_wolf', {'password': PASSWORD}, token, None, cedars)[0], 404,
                         "nobody else's wolf")
        self.assertEqual(self.call('delete_wolf', {'password': PASSWORD}, token, None, 'wolf-x')[0], 404)
        status, reply = self.call('delete_wolf', {'password': PASSWORD}, token, None, ash)
        self.assertEqual((status, reply['deleted']), (200, ash), reply)
        self.assertIsNone(STORE.wolf(ash))
        self.assertEqual([w['id'] for w in self.call('list_wolves', token=token)[1]['wolves']], [birch],
                         'the account and its other wolves stay')
        self.assertIsNotNone(STORE.wolf(cedars))
        self.assertEqual(STORE.deleted_wolves(), [ash], 'listed for the worlds to forget')
        self.assertEqual(self.call('delete_wolf', {'password': PASSWORD}, token, None, ash)[0], 404, 'once')
        self.assertEqual(self.call('create_wolf', {'name': 'ash tallow', 'age': 7, 'appearance': LOOK}, other)[1]['code'],
                         'name_taken', 'its name is held back for 30 days')
        self.assertEqual(self.call('create_wolf', {'name': 'Ash Tallow', 'age': 7, 'appearance': LOOK}, token)[1]['code'],
                         'name_taken', 'from its own account too')
        world = self.world()
        self.assertEqual(self.call('ticket', {'world': world, 'wolf': ash}, token)[0], 404, 'no ticket for it')
        self.wolf(token, 'Ash Again')                 # (Its slot is free again.)
        self.clock.advance(30 * DAY)
        self.central.maintain()
        token = self.call('login', {'username': 'ash', 'password': PASSWORD})[1]['access_token']
        self.wolf(token, 'Ash Tallow')

    def test_failed_wolf_deletions_count_as_failed_sign_ins(self):
        token = self.register('ash')['access_token']
        ash = self.wolf(token, 'Ash Tallow')['id']
        for _ in range(10):
            self.assertEqual(self.call('delete_wolf', {'password': 'wrong password!'}, token, None, ash)[0], 401)
        self.assertEqual(self.call('delete_wolf', {'password': PASSWORD}, token, None, ash)[0], 429)
        self.assertIsNotNone(STORE.wolf(ash))

    def test_deletion_needs_a_signed_in_account(self):
        self.assertEqual(self.call('delete_account', {'password': PASSWORD}, None)[0], 401)


# --------------------------------------------------------------------------- Over HTTP

class Http(Base):
    def setUp(self):
        super().setUp()
        self.server = ratw_central.Server(self.central, '127.0.0.1', 0, trust_proxy=True, every=3600).start()
        self.base = f'http://127.0.0.1:{self.server.port}'

    def tearDown(self):
        self.server.stop()
        super().tearDown()

    def http(self, method, path, body=None, token=None, headers=None, raw=None):
        data = raw if raw is not None else (json.dumps(body).encode() if body is not None else None)
        request = urllib.request.Request(self.base + path, data=data, method=method)
        if data is not None:
            request.add_header('Content-Type', 'application/json')
        if token:
            request.add_header('Authorization', f'Bearer {token}')
        for key, value in (headers or {}).items():
            request.add_header(key, value)
        try:
            with urllib.request.urlopen(request, timeout=10) as response:
                return response.status, json.loads(response.read()), dict(response.headers)
        except urllib.error.HTTPError as error:
            with error:
                return error.code, json.loads(error.read() or b'{}'), dict(error.headers)

    def test_the_whole_way_through(self):
        status, reply, headers = self.http('POST', '/account/register', {'username': 'ash', 'password': PASSWORD,
                                                                        'birth_year': 1990})
        self.assertEqual(status, 200, reply)
        self.assertEqual(headers['Cache-Control'], 'no-store')
        token = reply['access_token']
        status, reply, _ = self.http('POST', '/account/login', {'username': 'ash', 'password': PASSWORD})
        self.assertEqual(status, 200)
        status, wolf, _ = self.http('POST', '/wolves', {'name': 'Ash', 'age': 9, 'appearance': LOOK}, token)
        self.assertEqual(status, 200, wolf)
        wolf_id = wolf['wolf']['id']
        status, patched, _ = self.http('PATCH', f'/wolves/{wolf_id}', {'revision': 1, 'profile': {'motto': 'Hm'}},
                                       token)
        self.assertEqual((status, patched['wolf']['revision']), (200, 2))
        self.assertEqual(self.http('GET', '/wolves', token=token)[1]['wolves'][0]['id'], wolf_id)
        world = self.world()
        status, ticket, _ = self.http('POST', '/tickets', {'world': world, 'wolf': wolf_id}, token)
        self.assertEqual(status, 200, ticket)
        status, keys, headers = self.http('GET', '/keys')
        self.assertEqual((status, headers['Cache-Control']), (200, 'public, max-age=300'))
        claims = ct.verify(ticket['ticket'], ct.public_keys(keys), world, self.clock(), self.clock() - 10, {})
        self.assertEqual(claims['name'], 'Ash')
        status, second, _ = self.http('POST', '/wolves', {'name': 'Bramble', 'age': 9, 'appearance': LOOK}, token)
        status, gone, _ = self.http('DELETE', f'/wolves/{second["wolf"]["id"]}', {'password': PASSWORD}, token)
        self.assertEqual((status, gone['deleted']), (200, second['wolf']['id']), gone)
        self.assertEqual([w['id'] for w in self.http('GET', '/wolves', token=token)[1]['wolves']], [wolf_id])
        self.assertEqual(self.http('POST', '/account/logout', {}, token)[0], 200)
        self.assertEqual(self.http('GET', '/account', token=token)[0], 401)
        self.assertEqual(self.http('GET', '/health')[0], 200)
        status, rules, headers = self.http('GET', '/rules')
        self.assertEqual((status, len(rules['gifts']['families']), headers['Cache-Control']), (200, 8, 'public, max-age=300'))
        status, listed, _ = self.http('GET', '/worlds')
        self.assertEqual((status, listed['worlds']), (200, []), 'no world has an address yet')

    def test_bad_requests(self):
        self.assertEqual(self.http('GET', '/nowhere')[0], 404)
        self.assertEqual(self.http('GET', '/account/login')[0], 405)
        self.assertEqual(self.http('POST', '/account/login', raw=b'{not json')[0], 400)
        self.assertEqual(self.http('POST', '/account/login', raw=b'[1, 2]')[0], 400)
        self.assertEqual(self.http('POST', '/account/login', raw=b'{}', headers={'Content-Type': 'text/plain'})[0],
                         415)
        self.assertEqual(self.http('POST', '/account/login', raw=b'x' * (17 * 1024))[0], 413)
        self.assertEqual(self.http('GET', '/wolves', token='nonsense')[0], 401)

    def test_limits_by_the_forwarded_address_behind_a_proxy(self):
        self.central.settings.limits = cs.Limits(register_per_address=(1, 3600))
        first = {'X-Forwarded-For': '203.0.113.7'}
        body = {'username': 'ash', 'password': PASSWORD, 'birth_year': 1990}
        self.assertEqual(self.http('POST', '/account/register', body, headers=first)[0], 200)
        status, reply, headers = self.http('POST', '/account/register', {**body, 'username': 'birch'}, headers=first)
        self.assertEqual(status, 429)
        self.assertGreater(int(headers['Retry-After']), 3500)
        self.assertEqual(self.http('POST', '/account/register', {**body, 'username': 'birch'},
                                   headers={'X-Forwarded-For': '203.0.113.8'})[0], 200)


# --------------------------------------------------------------------------- What the client shows (doc 77, Phase 3)

class ClientViews(Base):
    def test_the_creators_rules_come_from_the_game(self):
        status, reply = self.call('rules')
        self.assertEqual(status, 200, reply)
        families = reply['gifts']['families']
        self.assertEqual(len(families), 8)
        self.assertIn('fire', [f['id'] for f in families])
        self.assertTrue(all('gifted' in f and 'quickened' in f for f in families))
        self.assertEqual((reply['slots'], reply['ages']), (6, [6, 99]))


# --------------------------------------------------------------------------- The word filter (doc 77, 7; Phase 4)

class Words(unittest.TestCase):
    def test_whole_words_and_inside_words(self):
        self.assertTrue(central_words.refused('Shit Hollow'))
        self.assertTrue(central_words.refused('the shits'), 'with a plural s')
        self.assertTrue(central_words.refused('BITCHES'), 'with a plural es, in capitals')
        self.assertFalse(central_words.refused('Shitake'), 'a whole-word entry inside a longer word passes')
        self.assertTrue(central_words.refused('Fuckwit Fen'), "a '*' entry inside a longer word")
        self.assertTrue(central_words.refused('f.u.c.k'), "a '*' entry with stops between its letters")
        self.assertFalse(central_words.refused(''))
        self.assertFalse(central_words.refused(None))

    def test_look_alikes_and_accents(self):
        self.assertTrue(central_words.refused('sh1t'))
        self.assertTrue(central_words.refused('5h1t'))
        self.assertTrue(central_words.refused('b1tch'))
        self.assertTrue(central_words.refused('fück'))
        self.assertTrue(central_words.refused('@ssh0le'))
        for dodged in ('bitch1', 'shit0', 'slut4', 's1ut', 'bu11shit', 'sh|t', 'Ѕhit', 'ѕhit', 'shіt', 'βitch'):
            self.assertTrue(central_words.refused(dodged), dodged)

    def test_invisible_characters_and_repeated_letters(self):
        for dodged in ('Sh\u200bit', 'Sh\u00adit', 'Sh\ufe0fit', 'S\u20ddh\u20ddi\u20ddt\u20dd', 'Sh\u200dit', 'Sh\u034fit',
                       'shiiit', 'biiitch', 'cuuunt', 'SHHHIIIT', 'f\u200bu\u200bc\u200bk'):
            self.assertTrue(central_words.refused(dodged), repr(dodged))

    def test_ordinary_words_pass(self):
        for fine in ('Scunthorpe', 'Essex', 'Penistone', 'Cockburn', 'Ash Tallow', 'Greyfen Crossing',
                     'Middlesex Wolves', 'Assassin', 'Grasshopper', 'Shiitake', 'Dickens', 'Spice Road',
                     'Cocktail Hour', 'Bass Lake', 'Sussex Pack', 'Arsenal', 'Niger Delta', 'Therapist',
                     'Traders selling Spices', 'Spices', 'Sniggering Crows', 'snigger', 'a chink in the armour',
                     'Spick and span', "Offa's Dyke", 'Van Dyke', 'bastard sword', 'pin-prick', 'Pussy willow',
                     'Coon Rapids', 'fire-retarded', 'Glass Hill 2', 'Bull Hill', 'Mississippi', 'Ill Wind'):
            self.assertFalse(central_words.refused(fine), fine)

    def test_the_list_is_read_from_its_file(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'words.txt'
            path.write_text('# a comment\nbramble  # after the word\n*thorn\n\n', encoding='utf-8')
            self.assertTrue(central_words.refused('Bramble', path))
            self.assertTrue(central_words.refused('Blackthorns', path))
            self.assertFalse(central_words.refused('Bramblewood', path))
            self.assertFalse(central_words.refused('comment', path))
            path.write_text('*thorn\n!thornton\nfox\nmoss\n', encoding='utf-8')
            self.assertFalse(central_words.refused('Thornton Wolves', path), "'!' lets an ordinary word through")
            self.assertTrue(central_words.refused('Thornton Thornpack', path), 'but not the rest')
            self.assertTrue(central_words.refused('Foxes', path))
            self.assertTrue(central_words.refused('Mosses', path))
            self.assertFalse(central_words.refused('Foxe', path))
        self.assertGreaterEqual(len(central_words._words()[3]) + len(central_words._words()[4]), 30)


# --------------------------------------------------------------------------- The server list (doc 77, 7; Phase 4)

class StandIn:
    """A stand-in world's /status on loopback. `mode` picks its answer: world (its id), wrong (another id), redirect,
    big (over 2 KB), notjson, missing (404); or silent (a socket that takes the connection and never answers)."""

    def __init__(self, world_id: str, mode: str = 'world'):
        self.world_id, self.mode, self.seen = world_id, mode, []
        if mode == 'silent':
            self.sock = socket.socket()
            self.sock.bind(('127.0.0.1', 0))
            self.sock.listen(8)
            self.port, self.http = self.sock.getsockname()[1], None
            return
        outer = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_GET(self):
                outer.seen.append((self.path, self.headers.get('Host')))
                if outer.mode == 'redirect':
                    self.send_response(302)
                    self.send_header('Location', 'http://169.254.169.254/latest/meta-data/')
                    self.send_header('Content-Length', '0')
                    self.end_headers()
                    return
                if outer.mode == 'missing':
                    self.send_response(404)
                    self.send_header('Content-Length', '0')
                    self.end_headers()
                    return
                status = {'ratw': 'world', 'id': outer.world_id if outer.mode != 'wrong' else 'world-' + 'e' * 32,
                          'name': 'Stand-in', 'protocol': 1, 'players': 0}
                if outer.mode == 'big':
                    status['padding'] = 'x' * 3000
                data = b'<html>hello</html>' if outer.mode == 'notjson' else json.dumps(status).encode()
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)

        self.http = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.http.daemon_threads = True
        self.port = self.http.server_address[1]
        threading.Thread(target=self.http.serve_forever, args=(0.05,), daemon=True).start()

    def close(self):
        if self.http:
            self.http.shutdown()
            self.http.server_close()
        else:
            self.sock.close()


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


class ServerList(Base):
    local_worlds = True

    def setUp(self):
        super().setUp()
        self.central.settings.local_worlds = self.local_worlds
        self.central.settings.check_timeout = 0.5
        self.stand_ins = []
        self.host = self.register('hazel')
        STORE.set_flags(self.host['account']['id'], host=True)
        self.token = self.host['access_token']

    def tearDown(self):
        for stand_in in self.stand_ins:
            stand_in.close()
        super().tearDown()

    def make(self, name='Greyfen Hollow', token=None):
        status, reply = self.call('host_make_world', {'name': name}, token or self.token)
        self.assertEqual(status, 200, reply)
        return reply['world']['id'], reply['token']

    def stand_in(self, world_id, mode='world'):
        made = StandIn(world_id, mode)
        self.stand_ins.append(made)
        return made

    def beat(self, host_token, world_port, address='127.0.0.1', **more):
        body = {'port': world_port, 'name': 'Greyfen Hollow', 'protocol': 1, **more}
        return self.call('heartbeat', body, host_token, address)

    def view(self, world):
        """The Host page's view of one world (signing in again: the fake clock outruns access tokens)."""
        token = self.call('login', {'username': 'hazel', 'password': PASSWORD})[1]['access_token']
        return [w for w in self.call('host_worlds', {}, token)[1]['worlds'] if w['id'] == world][0]

    def listed(self):
        return [w['id'] for w in self.call('list_worlds')[1]['worlds']]

    def test_a_heartbeat_lists_the_world(self):
        world, host_token = self.make()
        self.assertRegex(host_token, r'^rh_[A-Za-z0-9_-]{43}$')
        with STORE.connection() as conn:
            stored = conn.execute('SELECT token_hash FROM central.worlds WHERE id = %s', (world,)).fetchone()[0]
        self.assertEqual(stored, cs.token_hash(host_token), 'kept only as its hash')
        self.assertEqual(self.listed(), [], 'not before its first heartbeat')
        stand_in = self.stand_in(world)
        status, reply = self.beat(host_token, stand_in.port, description='A quiet valley.', region='EU',
                                  tags=['rp', 'slow', 'rp'], limit=40, voices=True, build='abc', clientData='f00d')
        self.assertEqual(status, 200, reply)
        self.assertEqual(reply, {'ok': True, 'world': world, 'kind': 'community', 'listed': True, 'reachable': True,
                                 'why': '', 'address': f'127.0.0.1:{stand_in.port}', 'players': 0, 'bans': [],
                                 'host': ct.pairwise_id(self.keys.pairwise_secret(), self.host['account']['id'], world),
                                 'time': self.clock(), 'every': 60})
        self.assertEqual(stand_in.seen, [('/status', f'127.0.0.1:{stand_in.port}')])
        status, reply = self.call('list_worlds')
        self.assertEqual(reply['worlds'], [{'id': world, 'name': 'Greyfen Hollow', 'kind': 'community',
                                            'address': f'127.0.0.1:{stand_in.port}', 'host': None, 'secure': False,
                                            'protocol': 1,
                                            'description': 'A quiet valley.', 'players': 0, 'limit': 40,
                                            'region': 'EU', 'tags': ['rp', 'slow'], 'voices': True, 'cheats': False}])
        self.clock.advance(60)
        self.assertTrue(self.beat(host_token, stand_in.port, name='Greyfen Vale')[1]['listed'])
        self.assertEqual(len(stand_in.seen), 1, 'checked again only after 10 minutes')
        self.assertEqual(self.call('list_worlds')[1]['worlds'][0]['name'], 'Greyfen Vale', "the heartbeat's name")
        self.clock.advance(600)
        self.assertTrue(self.beat(host_token, stand_in.port)[1]['listed'])
        self.assertEqual(len(stand_in.seen), 2, 'every 10 minutes')
        other = self.stand_in(world)
        self.clock.advance(60)
        self.assertTrue(self.beat(host_token, other.port)[1]['listed'])
        self.assertEqual(len(other.seen), 1, 'and at once when the address changes')

    def test_host_tokens_good_bad_rotated_and_expired(self):
        world, host_token = self.make()
        stand_in = self.stand_in(world)
        for bad in (None, '', 'nonsense', 'rh_' + 'x' * 43, host_token + 'x'):
            status, reply = self.beat(bad, stand_in.port)
            self.assertEqual((status, reply['code']), (401, 'bad_token'), bad)
        self.assertEqual(self.beat(host_token, stand_in.port)[0], 200)
        status, reply = self.call('host_new_token', {}, self.token, None, world)
        self.assertEqual(status, 200, reply)
        rotated = reply['token']
        self.assertEqual(self.listed(), [], 'a new token delists the world at once')
        self.assertFalse(self.view(world)['listed'])
        self.assertEqual(self.beat(host_token, stand_in.port)[1]['code'], 'bad_token', 'the old one stops at once')
        self.assertTrue(self.beat(rotated, stand_in.port)[1]['listed'])
        # A token used keeps going; unused for 90 days it is refused, and the world is off the list.
        self.clock.advance(89 * DAY)
        self.assertEqual(self.beat(rotated, stand_in.port)[0], 200)
        self.clock.advance(89 * DAY)
        self.assertEqual(self.beat(rotated, stand_in.port)[0], 200)
        self.assertEqual(self.listed(), [world])
        with STORE.connection() as conn:     # (Its heartbeat kept arriving, but from a world whose token went unused.)
            conn.execute('UPDATE central.worlds SET token_used = %s WHERE id = %s',
                         (central_store.ts(self.clock() - 91 * DAY), world))
        self.assertEqual(self.listed(), [], 'an expired token drops the world off at once')
        status, reply = self.beat(rotated, stand_in.port)
        self.assertEqual((status, reply['code']), (401, 'bad_token'))
        self.assertIn('90 days', reply['error'])
        view = self.view(world)
        self.assertFalse(view['listed'])
        self.assertIn('90 days', view['why'])
        self.token = self.call('login', {'username': 'hazel', 'password': PASSWORD})[1]['access_token']
        fresh = self.call('host_new_token', {}, self.token, None, world)[1]['token']
        self.assertTrue(self.beat(fresh, stand_in.port)[1]['listed'])
        # Revoked by hand (ratw_central.py worlds revoke): refused, and off the list.
        STORE.set_world_token(world, None, self.clock())
        self.assertEqual(self.beat(fresh, stand_in.port)[1]['code'], 'bad_token')
        self.assertEqual(self.listed(), [])

    def test_heartbeats_are_limited_per_token(self):
        world, host_token = self.make()
        stand_in = self.stand_in(world)
        for _ in range(10):
            self.assertEqual(self.beat(host_token, stand_in.port)[0], 200)
        status, reply = self.beat(host_token, stand_in.port)
        self.assertEqual((status, reply['code']), (429, 'rate_limited'))
        self.clock.advance(61)
        self.assertEqual(self.beat(host_token, stand_in.port)[0], 200)

    def test_unknown_host_tokens_are_counted_per_address_and_grow_nothing(self):
        world, host_token = self.make()
        stand_in = self.stand_in(world)
        before = len(self.central.limiter)
        for _ in range(30):
            status, reply = self.beat('rh_' + secrets.token_urlsafe(32), stand_in.port, address='2001:db8:1::5')
            self.assertEqual((status, reply['code']), (401, 'bad_token'))
        self.assertLessEqual(len(self.central.limiter), before + 1, 'one key for the address, not one a token')
        status, reply = self.beat('rh_' + secrets.token_urlsafe(32), stand_in.port, address='2001:db8:1::77')
        self.assertEqual((status, reply['code']), (429, 'rate_limited'), 'the same /64')
        self.assertEqual(self.beat('rh_x', stand_in.port, address='2001:db8:2::5')[0], 401, 'another /64')
        self.assertEqual(self.beat(host_token, stand_in.port)[0], 200, 'a real token from elsewhere is fine')
        self.clock.advance(61)
        self.assertEqual(self.beat('rh_x', stand_in.port, address='2001:db8:1::5')[0], 401)
        # A key whose events have all left its window is swept, not kept for a day.
        self.clock.advance(3601)
        self.central.limiter.sweep()
        self.assertEqual(len(self.central.limiter), 0)

    def test_a_token_replaced_during_the_call_back_writes_nothing(self):
        world, host_token = self.make()
        stand_in = self.stand_in(world)
        real = self.central.call_back

        def rotate_meanwhile(*args):
            why = real(*args)
            STORE.set_world_token(world, cs.token_hash(cs.new_host_token()), self.clock())
            return why
        self.central.call_back = rotate_meanwhile
        status, reply = self.beat(host_token, stand_in.port)
        self.assertEqual((status, reply['code']), (401, 'bad_token'))
        self.assertEqual(self.listed(), [])
        self.assertIsNone(STORE.world(world)['last_heartbeat'], 'nothing written')
        # And one revoked by hand meanwhile, or the world delisted meanwhile.
        fresh = cs.new_host_token()
        STORE.set_world_token(world, cs.token_hash(fresh), self.clock())
        self.central.call_back = lambda *args: real(*args) if STORE.set_world_token(world, None, self.clock()) else ''
        self.assertEqual(self.beat(fresh, stand_in.port)[1]['code'], 'bad_token')
        self.assertEqual(self.listed(), [])
        fresh = cs.new_host_token()
        STORE.set_world_token(world, cs.token_hash(fresh), self.clock())
        self.central.call_back = lambda *args: real(*args) if STORE.set_world(world, delisted=True) else ''
        self.assertEqual(self.beat(fresh, stand_in.port)[1]['code'], 'bad_token')
        self.assertEqual(self.listed(), [])
        # A leave with a token replaced since it was checked does nothing either.
        STORE.set_world(world, delisted=False)
        self.central.call_back = real
        self.clock.advance(61)
        self.assertTrue(self.beat(fresh, stand_in.port)[1]['listed'])
        hashed = STORE.world(world)['last_heartbeat']
        self.assertFalse(STORE.leave(world, cs.token_hash(cs.new_host_token())))
        self.assertEqual(STORE.world(world)['last_heartbeat'], hashed)

    def test_the_address_is_where_the_heartbeat_came_from(self):
        world, host_token = self.make()
        stand_in = self.stand_in(world)
        status, reply = self.beat(host_token, stand_in.port, address='127.0.0.1', ip='8.8.8.8',
                                  addressOverride='10.0.0.1')
        self.assertEqual((status, reply['address']), (200, f'127.0.0.1:{stand_in.port}'), 'unknown fields ignored')
        self.assertEqual(self.call('list_worlds')[1]['worlds'][0]['address'], f'127.0.0.1:{stand_in.port}')
        status, reply = self.beat(host_token, stand_in.port, address='::ffff:127.0.0.1')
        self.assertEqual(reply['address'], f'127.0.0.1:{stand_in.port}', 'IPv4 mapped into IPv6 is IPv4')

    def test_private_and_loopback_are_not_listed_without_local_worlds(self):
        self.central.settings.local_worlds = False
        world, host_token = self.make()
        stand_in = self.stand_in(world)
        for address in ('127.0.0.1', '10.0.0.5', '192.168.1.2', '172.16.0.9', '100.64.1.1', '169.254.169.254',
                        '0.0.0.0', '224.0.0.1', '240.0.0.1', '::1', 'fc00::1', 'fe80::1', '::', '192.0.2.4'):
            status, reply = self.beat(host_token, stand_in.port, address=address)
            self.assertEqual(status, 200, address)
            self.assertEqual((reply['listed'], reply['why']), (False, cs.NOT_ON_THE_INTERNET), address)
            self.clock.advance(7)
        self.assertEqual(stand_in.seen, [], 'central never called them')
        self.assertEqual(self.listed(), [])
        # A public address is called back (here a stand-in call-back: no packet leaves this computer).
        called = []
        self.central.call_back = lambda *args: called.append(args) or ''
        status, reply = self.beat(host_token, 7788, address='8.8.8.8')
        self.assertTrue(reply['listed'], reply)
        self.assertEqual(called, [(ipaddress.ip_address('8.8.8.8'), 7788, None, False, world)])
        self.assertEqual(self.call('list_worlds')[1]['worlds'][0]['address'], '8.8.8.8:7788')
        self.clock.advance(7)
        reply = self.beat(host_token, 7788, address='2001:4860:4860::8888')[1]
        self.assertEqual((reply['listed'], reply['address']), (True, '[2001:4860:4860::8888]:7788'))
        self.assertEqual(self.call('list_worlds')[1]['worlds'][0]['address'], '[2001:4860:4860::8888]:7788')

    def test_local_worlds_lists_loopback_and_private_but_never_link_local(self):
        world, host_token = self.make()
        stand_in = self.stand_in(world)
        self.assertTrue(self.beat(host_token, stand_in.port, address='127.0.0.1')[1]['listed'])
        called = []
        self.central.call_back = lambda *args: called.append(args) or ''
        for address in ('10.0.0.5', '192.168.1.2', '::1', 'fd00::7'):
            self.clock.advance(7)
            self.assertTrue(self.beat(host_token, 7788, address=address)[1]['listed'], address)
        for address in ('169.254.169.254', 'fe80::1', '0.0.0.0', '::', '224.0.0.1', 'ff02::1'):
            self.clock.advance(7)
            reply = self.beat(host_token, 7788, address=address)[1]
            self.assertEqual((reply['listed'], reply['why']), (False, cs.NOT_ON_THE_INTERNET), address)
        self.assertEqual(len(called), 4, 'never called back at those')

    def test_a_host_name_must_resolve_to_the_source(self):
        world, host_token = self.make()
        stand_in = self.stand_in(world)
        ip = ipaddress.ip_address
        names = {'play.example.net': {ip('203.0.113.9')},
                 'home.example.net': {ip('127.0.0.1')},
                 'both.example.net': {ip('127.0.0.1'), ip('::1')},
                 'stranger.example.net': {ip('127.0.0.1'), ip('198.51.100.7')},
                 'router.example.net': {ip('127.0.0.1'), ip('192.168.1.1')}}
        looked_up = []

        def resolve(host, port):
            looked_up.append(host)
            if host not in names:
                raise socket.gaierror(-2, 'Name or service not known')
            return names[host]
        self.central.resolve = resolve
        reply = self.beat(host_token, stand_in.port, host='play.example.net')[1]
        self.assertFalse(reply['listed'])
        self.assertIn('does not point at', reply['why'])
        self.assertNotIn('203.0.113.9', reply['why'], "central never shows its own DNS answers")
        for other in ('stranger.example.net', 'router.example.net', 'both.example.net'):
            self.clock.advance(7)
            reply = self.beat(host_token, stand_in.port, host=other)[1]
            self.assertFalse(reply['listed'], f'{other}: the source and another address')
            self.assertIn('does not point at', reply['why'])
            self.assertNotIn('198.51.100.7', reply['why'])
            self.assertNotIn('192.168.1.1', reply['why'])
        self.clock.advance(7)
        reply = self.beat(host_token, stand_in.port, host='nowhere.example.net')[1]
        self.assertFalse(reply['listed'])
        self.assertIn('does not resolve', reply['why'])
        self.assertEqual(stand_in.seen, [], 'not called back')
        # Names that are never on the public DNS are refused before central looks them up.
        looked_up.clear()
        for local in ('postgres', 'caddy', 'localhost', 'metadata.google.internal', 'db.internal', 'printer.local',
                      'web.localhost', 'nas.lan', 'box.home.arpa', 'home.arpa'):
            self.clock.advance(7)
            status, reply = self.beat(host_token, stand_in.port, host=local)
            self.assertEqual(status, 200, local)
            self.assertFalse(reply['listed'], local)
            self.assertIn('not a public DNS name', reply['why'], local)
        self.assertEqual(looked_up, [], 'without a lookup')
        self.clock.advance(7)
        reply = self.beat(host_token, stand_in.port, host='Home.Example.Net.')[1]
        self.assertTrue(reply['listed'], reply)
        self.assertEqual(stand_in.seen, [('/status', f'home.example.net:{stand_in.port}')], 'Host is the name')
        # A ws:// world is listed at the address central called back; the name is only to show.
        entry = self.call('list_worlds')[1]['worlds'][0]
        self.assertEqual((entry['address'], entry['host'], entry['secure'], 'ip' in entry),
                         (f'127.0.0.1:{stand_in.port}', 'home.example.net', False, False))
        self.clock.advance(7)
        reply = self.beat(host_token, stand_in.port, secure=True)[1]
        self.assertFalse(reply['listed'])
        self.assertIn('needs a host name', reply['why'])
        for bad in ('127.0.0.1', '[::1]', 'a b', 'x' * 254, '-bad.example.net', 7):
            self.clock.advance(7)
            status, reply = self.beat(host_token, stand_in.port, host=bad)
            self.assertEqual((status, reply['code']), (400, 'invalid'), bad)
        # A wss:// world is listed by its name (its certificate binds it), with the called-back address beside it.
        self.central.call_back = lambda *args: ''
        self.clock.advance(7)
        self.assertTrue(self.beat(host_token, 7788, host='home.example.net', secure=True)[1]['listed'])
        entry = self.call('list_worlds')[1]['worlds'][0]
        self.assertEqual((entry['address'], entry['host'], entry['secure'], entry['ip']),
                         ('home.example.net:7788', 'home.example.net', True, '127.0.0.1'))

    def test_the_call_back(self):
        world, host_token = self.make()
        cases = [('wrong', 'is another world (its /status names world-' + 'e' * 32),
                 ('redirect', 'redirect (HTTP 302), which central does not follow'),
                 ('big', 'over 2 KB'), ('notjson', 'not JSON'), ('missing', 'HTTP 404'),
                 ('silent', 'did not answer within 0.5 seconds')]
        for mode, expected in cases:
            stand_in = self.stand_in(world, mode)
            status, reply = self.beat(host_token, stand_in.port)
            self.assertEqual(status, 200, mode)
            self.assertFalse(reply['listed'], mode)
            self.assertFalse(reply['reachable'], mode)
            self.assertIn(expected, reply['why'], mode)
            if mode != 'wrong':
                self.assertTrue(reply['why'].startswith(f'Central could not reach this world at 127.0.0.1:'
                                                        f'{stand_in.port}: '), reply['why'])
                self.assertTrue(reply['why'].endswith(f'Open port {stand_in.port} to the internet (TCP).'))
            self.assertEqual(self.listed(), [], mode)
            self.assertEqual(self.view(world)['why'], reply['why'], 'the Host page says why')
            self.clock.advance(7)
        closed = free_port()
        reply = self.beat(host_token, closed)[1]
        self.assertIn('the connection was refused', reply['why'])
        self.assertIn(f'Open port {closed}', reply['why'])
        # Not reachable yet: every heartbeat calls again, so the host's fix shows within a minute.
        stand_in = self.stand_in(world, 'missing')
        self.beat(host_token, stand_in.port)
        self.clock.advance(7)
        stand_in.mode = 'world'
        self.assertTrue(self.beat(host_token, stand_in.port)[1]['listed'])
        self.assertEqual(len(stand_in.seen), 2)
        self.assertEqual(self.listed(), [world])
        # Listed, then it goes wrong: the 10-minute check takes it off.
        self.clock.advance(601)
        stand_in.mode = 'wrong'
        self.assertFalse(self.beat(host_token, stand_in.port)[1]['listed'])
        self.assertEqual(self.listed(), [])

    def test_off_the_list_after_180_seconds_or_at_once_on_leaving(self):
        world, host_token = self.make()
        stand_in = self.stand_in(world)
        self.beat(host_token, stand_in.port)
        self.clock.advance(179)
        self.assertEqual(self.listed(), [world])
        self.clock.advance(2)
        self.assertEqual(self.listed(), [])
        view = self.view(world)
        self.assertEqual((view['listed'], view['why']), (False, 'No heartbeat for 3 minutes: is its world agent running?'))
        self.beat(host_token, stand_in.port)
        self.assertEqual(self.listed(), [world])
        status, reply = self.call('leave', {}, host_token)
        self.assertEqual((status, reply), (200, {'ok': True}))
        self.assertEqual(self.listed(), [], 'leaving takes it off at once')
        self.assertEqual(self.call('leave', {}, 'rh_nonsense')[1]['code'], 'bad_token')
        self.assertTrue(self.beat(host_token, stand_in.port)[1]['listed'], 'and the next heartbeat brings it back')

    def test_honest_counts(self):
        world, host_token = self.make()
        other_world, _ = self.make('Other Vale')
        stand_in = self.stand_in(world)
        a = self.register('ash')['access_token']
        b = self.register('birch')['access_token']
        ash, bramble = self.wolf(a, 'Ash Tallow')['id'], self.wolf(b, 'Bramble')['id']
        thorn = self.wolf(b, 'Thorn')['id']
        self.assertEqual(self.call('ticket', {'world': world, 'wolf': ash}, a)[0], 200)
        self.assertEqual(self.call('ticket', {'world': other_world, 'wolf': bramble}, b)[0], 200)
        made_up = 'wolf-' + secrets.token_hex(16)
        online = [ash, bramble, made_up, 'wolf-NOTHEX', 7, ash, thorn]
        reply = self.beat(host_token, stand_in.port, online=online)[1]
        self.assertEqual(reply['players'], 1, 'only a wolf central gave a ticket for this world counts')
        self.assertEqual(self.call('list_worlds')[1]['worlds'][0]['players'], 1)
        self.assertEqual(STORE.world(world)['online'], [ash])
        self.assertEqual(self.view(world)['players'], 1)
        # A past visitor now on another world doesn't count here (its latest ticket is for the other world).
        self.assertEqual(self.call('ticket', {'world': world, 'wolf': thorn}, b)[0], 200)
        self.clock.advance(7)
        self.assertEqual(self.beat(host_token, stand_in.port, online=online)[1]['players'], 2)
        self.assertEqual(self.call('ticket', {'world': other_world, 'wolf': thorn}, b)[0], 200)
        self.clock.advance(7)
        self.assertEqual(self.beat(host_token, stand_in.port, online=online)[1]['players'], 1, 'gone to the other')
        self.assertEqual(self.call('ticket', {'world': world, 'wolf': thorn}, b)[0], 200)
        self.clock.advance(7)
        self.assertEqual(self.beat(host_token, stand_in.port, online=online)[1]['players'], 2, 'and back')
        # Capped at the listing's limit.
        self.clock.advance(7)
        self.assertEqual(self.beat(host_token, stand_in.port, online=online, limit=1)[1]['players'], 1)
        # A banned account's wolves, and a deleted wolf, don't count.
        STORE.set_flags(STORE.account_by_username('birch')['id'], banned=True)
        self.clock.advance(7)
        self.assertEqual(self.beat(host_token, stand_in.port, online=online)[1]['players'], 1, 'banned')
        STORE.set_flags(STORE.account_by_username('birch')['id'], banned=False)
        self.assertEqual(STORE.delete_wolf(STORE.account_by_username('birch')['id'], thorn, self.clock(),
                                           self.clock() + DAY), True)
        self.clock.advance(7)
        self.assertEqual(self.beat(host_token, stand_in.port, online=online)[1]['players'], 1, 'deleted')
        # Tickets count for a day.
        with STORE.connection() as conn:
            conn.execute('UPDATE central.tickets SET issued_at = %s', (central_store.ts(self.clock() - DAY - 1),))
        self.clock.advance(7)
        self.assertEqual(self.beat(host_token, stand_in.port, online=online)[1]['players'], 0, 'tickets of a day')
        status, reply = self.beat(host_token, stand_in.port, online=['wolf-' + 'a' * 32] * 10001)
        self.assertEqual((status, reply['code']), (400, 'invalid'))

    def test_the_list_puts_official_worlds_first_then_the_most_players(self):
        quiet, quiet_token = self.make('Quiet Vale')
        busy, busy_token = self.make('Busy Vale')
        official = 'world-' + secrets.token_hex(16)
        STORE.add_world(official, 'Ridgemere', 'official', self.clock())
        official_token = cs.new_host_token()
        STORE.set_world_token(official, cs.token_hash(official_token), self.clock())
        a = self.register('ash')['access_token']
        ash = self.wolf(a, 'Ash Tallow')['id']
        self.call('ticket', {'world': busy, 'wolf': ash}, a)
        for world, host_token, online in ((quiet, quiet_token, []), (busy, busy_token, [ash]),
                                          (official, official_token, [])):
            stand_in = self.stand_in(world)
            self.assertTrue(self.beat(host_token, stand_in.port, online=online,
                                      name='Run Against the World' if world == official else 'Vale')[1]['listed'])
        self.assertEqual(self.listed(), [official, busy, quiet])
        self.assertEqual(self.call('list_worlds')[1]['worlds'][0]['kind'], 'official')

    def test_bans_as_each_world_sees_them(self):
        community, community_token = self.make()
        official = 'world-' + secrets.token_hex(16)
        STORE.add_world(official, 'Ridgemere', 'official', self.clock())
        official_token = cs.new_host_token()
        STORE.set_world_token(official, cs.token_hash(official_token), self.clock())
        reply = self.register('ash')
        account, a = reply['account']['id'], reply['access_token']
        ash = self.wolf(a, 'Ash Tallow')['id']
        ticket = self.call('ticket', {'world': community, 'wolf': ash}, a)[1]['ticket']
        sub = ct.verify(ticket, ct.public_keys(self.keys.jwks(self.clock())), community, self.clock(),
                        self.clock() - 10, {})['sub']
        STORE.set_flags(account, banned=True)
        stand_in = self.stand_in(community)
        bans = self.beat(community_token, stand_in.port)[1]['bans']
        self.assertEqual(bans, [sub], "a community world gets the banned account's ticket sub there")
        self.assertNotEqual(sub, account)
        self.assertEqual(bans, [ct.pairwise_id(self.keys.pairwise_secret(), account, community)])
        official_stand_in = self.stand_in(official)
        self.assertEqual(self.beat(official_token, official_stand_in.port)[1]['bans'], [account],
                         'an official world gets the account id')

    def test_the_heartbeat_names_a_community_worlds_host_and_no_host_for_an_official_world(self):
        community, community_token = self.make()
        hazel = STORE.world(community)['host_account']
        ticket = self.call('ticket', {'world': community, 'wolf': self.wolf(self.token, 'Hazel Brook')['id']},
                           self.token)[1]['ticket']
        sub = ct.verify(ticket, ct.public_keys(self.keys.jwks(self.clock())), community, self.clock(),
                        self.clock() - 10, {})['sub']
        stand_in = self.stand_in(community)
        host = self.beat(community_token, stand_in.port)[1]['host']
        self.assertEqual(host, sub, "the host is the host account's ticket sub on that world")
        self.assertEqual(host, ct.pairwise_id(self.keys.pairwise_secret(), hazel, community))
        self.assertNotEqual(host, hazel)
        official = 'world-' + secrets.token_hex(16)
        STORE.add_world(official, 'Ridgemere', 'official', self.clock(), host_account=hazel)
        official_token = cs.new_host_token()
        STORE.set_world_token(official, cs.token_hash(official_token), self.clock())
        official_stand_in = self.stand_in(official)
        self.assertIsNone(self.beat(official_token, official_stand_in.port, name='Run Against the World')[1]['host'],
                          'an official world has no host, whoever made it')

    def test_names_and_fields(self):
        world, host_token = self.make()
        stand_in = self.stand_in(world)
        for name in ('Official Greyfen', 'The 0fficial Server', 'Run Against The World II', 'runs against the world',
                     'R.A.T.W. fans', 'ratw', 'Sh1t Hollow', 'Fuckwit Fen', 'Оfficial Ridgemere', 'Ridgemere Οfficial',
                     'Run Аgainst the World', 'RΑTW', 'Officia1', 'Run Against the Wor1d', 'Run Against the Wor|d',
                     'RATW2', 'RATW1', 'RATWorld', 'Of\u200dficial', 'Of\ufe0fficial', 'Of\u034fficial'):
            self.clock.advance(7)
            status, reply = self.beat(host_token, stand_in.port, name=name)
            self.assertEqual((status, reply['code']), (400, 'name_refused'), name)
        self.clock.advance(7)
        self.assertEqual(self.beat(host_token, stand_in.port, description='Not an official world.')[1]['code'],
                         'name_refused')
        self.clock.advance(7)
        self.assertEqual(self.beat(host_token, stand_in.port, tags=['shit'])[1]['code'], 'name_refused')
        for fields in ({'region': 'Official EU'}, {'region': 'RATW'}, {'tags': ['rp', 'official']}, {'tags': ['ratw']}):
            self.clock.advance(7)
            self.assertEqual(self.beat(host_token, stand_in.port, **fields)[1]['code'], 'name_refused', fields)
        for name in ('Ratwell Crossing', 'Scunthorpe Wolves', 'Against the Wind', 'Rat Warren', 'Art Walk'):
            self.clock.advance(7)
            self.assertEqual(self.beat(host_token, stand_in.port, name=name)[0], 200, name)
        self.assertEqual(self.call('host_make_world', {'name': 'RATW Official'}, self.token)[1]['code'],
                         'name_refused')
        official = 'world-' + secrets.token_hex(16)
        STORE.add_world(official, 'Ridgemere', 'official', self.clock())
        official_token = cs.new_host_token()
        STORE.set_world_token(official, cs.token_hash(official_token), self.clock())
        self.assertEqual(self.beat(official_token, stand_in.port, name='Run Against the World (official)')[0], 200,
                         'an official world may')
        self.assertEqual(self.beat(official_token, stand_in.port, name='Shit Town')[1]['code'], 'name_refused')
        bad = [{'port': 0}, {'port': 70000}, {'port': '7788'}, {'port': None}, {'name': ''}, {'name': 'x' * 49},
               {'name': 'Tab\there'}, {'name': None}, {'description': 'x' * 301}, {'region': 'x' * 33},
               {'tags': ['a'] * 6}, {'tags': ['x' * 17]}, {'tags': 'rp'}, {'limit': 0}, {'limit': 10001},
               {'limit': True}, {'private': 'yes'}, {'voices': 1}, {'protocol': None}, {'protocol': '1'},
               {'build': 'x' * 65}, {'clientData': 'x' * 65}, {'online': 'wolf'}, {'name': 'A‮B'}]
        for fields in bad:
            self.clock.advance(7)
            status, reply = self.beat(host_token, stand_in.port, **fields)
            self.assertEqual((status, reply['code']), (400, 'invalid'), fields)

    def test_delisted_removed_and_banned_hosts(self):
        world, host_token = self.make()
        stand_in = self.stand_in(world)
        self.assertTrue(self.beat(host_token, stand_in.port)[1]['listed'])
        STORE.set_world(world, delisted=True)
        self.assertEqual(self.listed(), [])
        status, reply = self.beat(host_token, stand_in.port)
        self.assertEqual((status, reply['code']), (403, 'delisted'))
        STORE.set_world(world, delisted=False)
        self.assertEqual(self.beat(host_token, stand_in.port)[0], 200)
        STORE.set_flags(self.host['account']['id'], banned=True)
        self.assertEqual(self.listed(), [], "a banned host's worlds drop off at once")
        status, reply = self.beat(host_token, stand_in.port)
        self.assertEqual((status, reply['code']), (403, 'banned'))
        status, reply = self.call('host_worlds', {}, self.token)
        self.assertEqual((status, reply['code']), (403, 'banned'))

    def test_the_host_calls(self):
        guest = self.register('rowan')['access_token']
        for method, extra in (('host_worlds', ()), ('host_make_world', ())):
            status, reply = self.call(method, {'name': 'Mine'}, guest, None, *extra)
            self.assertEqual((status, reply['code']), (403, 'not_host'), method)
        self.assertEqual(self.call('host_worlds', {}, None)[0], 401)
        first, first_token = self.make('First Vale')
        status, listing = self.call('host_worlds', {}, self.token)
        self.assertEqual(status, 200)
        self.assertEqual(listing['most'], 3)
        view = self.view(first)
        self.assertEqual(set(view), {'id', 'name', 'kind', 'listed', 'reachable', 'why', 'address', 'players',
                                     'last_heartbeat', 'token_created'})
        self.assertEqual((view['id'], view['name'], view['kind'], view['listed'], view['address'], view['last_heartbeat'],
                          view['token_created']), (first, 'First Vale', 'community', False, None, None, int(START)))
        self.assertIn('No heartbeat', view['why'])
        self.make('Second Vale')
        third, _ = self.make('Third Vale')
        status, reply = self.call('host_make_world', {'name': 'Fourth Vale'}, self.token)
        self.assertEqual((status, reply['code']), (403, 'too_many'))
        for bad in ('', 'x' * 49, None, 7):
            self.assertEqual(self.call('host_make_world', {'name': bad}, self.token)[1]['code'], 'invalid', bad)
        # Someone else's world: not found, for the token and for removing.
        willow = self.register('willow')
        STORE.set_flags(willow['account']['id'], host=True)
        willow = willow['access_token']
        for method in ('host_new_token', 'host_remove_world'):
            status, reply = self.call(method, {}, willow, None, first)
            self.assertEqual((status, reply['code']), (404, 'not_found'), method)
            self.assertEqual(self.call(method, {}, self.token, None, 'world-' + 'f' * 32)[1]['code'], 'not_found')
        # A new token revokes the old.
        stand_in = self.stand_in(first)
        self.assertEqual(self.beat(first_token, stand_in.port)[0], 200)
        self.clock.advance(30)
        renewed = self.call('host_new_token', {}, self.token, None, first)[1]['token']
        self.assertNotEqual(renewed, first_token)
        self.assertEqual(self.beat(first_token, stand_in.port)[0], 401)
        self.assertTrue(self.beat(renewed, stand_in.port)[1]['listed'])
        view = self.view(first)
        self.assertEqual((view['listed'], view['reachable'], view['why'], view['address'], view['last_heartbeat'],
                          view['token_created']),
                         (True, True, '', f'127.0.0.1:{stand_in.port}', int(self.clock()), int(START + 30)))
        # Removed: off the list at once, its token dropped, gone from the page, and its place free.
        status, reply = self.call('host_remove_world', {}, self.token, None, first)
        self.assertEqual((status, reply), (200, {'ok': True, 'removed': first}))
        self.assertEqual(self.listed(), [])
        status, reply = self.beat(renewed, stand_in.port)
        self.assertEqual((status, reply['code']), (403, 'delisted'), 'its agent is told it was removed')
        self.assertIn('removed', reply['error'])
        self.assertNotIn(first, [w['id'] for w in self.call('host_worlds', {}, self.token)[1]['worlds']])
        self.assertEqual(self.call('host_new_token', {}, self.token, None, first)[1]['code'], 'not_found')
        self.assertEqual(self.call('host_remove_world', {}, self.token, None, first)[1]['code'], 'not_found')
        self.assertFalse(STORE.set_world_token(first, cs.token_hash(cs.new_host_token()), self.clock()))
        self.assertEqual(self.call('host_make_world', {'name': 'Fourth Vale'}, self.token)[0], 200)
        a = self.register('ash')['access_token']
        ash = self.wolf(a, 'Ash Tallow')['id']
        self.assertEqual(self.call('ticket', {'world': first, 'wolf': ash}, a)[1]['code'], 'no_world')
        self.assertEqual(self.call('ticket', {'world': third, 'wolf': ash}, a)[0], 200)

    def test_usernames_and_wolf_names_go_through_the_filter(self):
        status, reply = self.call('register', {'username': 'sh1thead', 'password': PASSWORD, 'birth_year': 1990})
        self.assertEqual((status, reply['code']), (400, 'name_refused'))
        status, reply = self.call('register', {'username': 'big_fuck', 'password': PASSWORD, 'birth_year': 1990})
        self.assertEqual((status, reply['code']), (400, 'name_refused'))
        self.register('scunthorpe')
        status, reply = self.call('create_wolf', {'name': 'Bitch', 'age': 7, 'appearance': LOOK}, self.token)
        self.assertEqual((status, reply['code']), (400, 'name_refused'))
        self.wolf(self.token, 'Penistone')
        for hidden in ('Sh\u200bit', 'Ash\u200d', 'As\ufe0fh', 'A\u00adsh', 'A\u20ddsh'):
            status, reply = self.call('create_wolf', {'name': hidden, 'age': 7, 'appearance': LOOK}, self.token)
            self.assertEqual(status, 400, repr(hidden))
            self.assertIn(reply['code'], ('invalid', 'name_refused'), repr(hidden))

    def test_the_command_line(self):
        out = io.StringIO()
        os.environ['RATW_CENTRAL_DBNAME'] = TEST_DB
        try:
            def run(*argv):
                out.seek(0)
                out.truncate()
                with __import__('contextlib').redirect_stdout(out), \
                        __import__('contextlib').redirect_stderr(io.StringIO()):
                    code = ratw_central.main(list(argv))
                return code, out.getvalue().strip().splitlines()
            code, lines = run('worlds', 'add', '--name', 'Laptop', '--kind', 'community', '--host', 'Hazel')
            self.assertEqual(code, 0)
            world = lines[-1]
            self.assertRegex(world, r'^world-[0-9a-f]{32}$')
            self.assertEqual(STORE.world(world)['host_account'], self.host['account']['id'])
            self.assertEqual(run('worlds', 'add', '--name', 'X', '--host', 'nobody')[0], 1)
            code, lines = run('worlds', 'token', world)
            host_token = lines[-1]
            self.assertEqual((code, len(lines)), (0, 1), 'only the token on stdout')
            self.assertRegex(host_token, r'^rh_')
            stand_in = self.stand_in(world)
            self.assertTrue(self.beat(host_token, stand_in.port)[1]['listed'])
            code, lines = run('worlds', 'list')
            row = json.loads(lines[0])
            self.assertEqual((row['id'], row['reachable'], row['players'], row['check_note'], row['last_heartbeat']),
                             (world, True, 0, '', self.clock()), '(listed by the real clock, so not asked)')
            code, lines = run('worlds', 'token', world)
            self.assertEqual(self.beat(host_token, stand_in.port)[0], 401, 'a new token revokes the old')
            renewed = lines[-1]
            self.assertEqual(self.beat(renewed, stand_in.port)[0], 200)
            self.assertEqual(run('worlds', 'set', world, '--delisted')[0], 0)
            self.assertEqual(self.beat(renewed, stand_in.port)[1]['code'], 'delisted')
            self.assertEqual(run('worlds', 'set', world, '--listed')[0], 0)
            self.assertEqual(self.beat(renewed, stand_in.port)[0], 200)
            self.assertEqual(run('worlds', 'revoke', world)[0], 0)
            self.assertEqual(self.beat(renewed, stand_in.port)[1]['code'], 'bad_token')
            self.assertEqual(run('worlds', 'token', 'world-' + 'f' * 32)[0], 1)
            with self.assertRaises(SystemExit), __import__('contextlib').redirect_stderr(io.StringIO()):
                ratw_central.main(['worlds', 'add', '--name', 'X', '--address', '127.0.0.1:7788'])
        finally:
            os.environ.pop('RATW_CENTRAL_DBNAME', None)

    def test_over_http(self):
        server = ratw_central.Server(self.central, '127.0.0.1', 0, every=3600).start()
        try:
            base = f'http://127.0.0.1:{server.port}'

            def http(method, path, body=None, token=None):
                data = json.dumps(body).encode() if body is not None else None
                request = urllib.request.Request(base + path, data=data, method=method)
                if data is not None:
                    request.add_header('Content-Type', 'application/json')
                if token:
                    request.add_header('Authorization', f'Bearer {token}')
                try:
                    with urllib.request.urlopen(request, timeout=10) as response:
                        return response.status, json.loads(response.read())
                except urllib.error.HTTPError as error:
                    with error:
                        return error.code, json.loads(error.read() or b'{}')
            status, made = http('POST', '/host/worlds', {'name': 'Greyfen Hollow'}, self.token)
            self.assertEqual(status, 200, made)
            world, host_token = made['world']['id'], made['token']
            self.assertEqual(http('GET', '/host/worlds', token=self.token)[1]['worlds'][0]['id'], world)
            stand_in = self.stand_in(world)
            a = self.register('ash')['access_token']
            ash = self.wolf(a, 'Ash Tallow')['id']
            self.call('ticket', {'world': world, 'wolf': ash}, a)
            online = [ash] + ['wolf-' + secrets.token_hex(16) for _ in range(2000)]     # (About 80 KB.)
            status, beat = http('POST', '/worlds/heartbeat', {'port': stand_in.port, 'name': 'Greyfen Hollow',
                                                              'protocol': 1, 'online': online}, host_token)
            self.assertEqual(status, 200, beat)
            self.assertEqual((beat['listed'], beat['address'], beat['players']),
                             (True, f'127.0.0.1:{stand_in.port}', 1), 'the address is the connection itself')
            self.assertEqual([w['id'] for w in http('GET', '/worlds')[1]['worlds']], [world])
            self.assertEqual(http('POST', '/worlds/heartbeat', {'port': 1}, 'rh_nope')[1]['code'], 'bad_token')
            self.assertEqual(http('POST', '/worlds/leave', {}, host_token), (200, {'ok': True}))
            self.assertEqual(http('GET', '/worlds')[1]['worlds'], [])
            status, renewed = http('POST', f'/host/worlds/{world}/token', {}, self.token)
            self.assertEqual(status, 200, renewed)
            self.assertEqual(http('POST', '/worlds/leave', {}, host_token)[0], 401)
            self.assertEqual(http('DELETE', f'/host/worlds/{world}', None, self.token), (200, {'ok': True,
                                                                                               'removed': world}))
            self.assertEqual(http('POST', '/worlds/heartbeat', {}, renewed['token'])[1]['code'], 'delisted')
            self.assertEqual(http('GET', f'/host/worlds/{world}', None, self.token)[0], 405)
            self.assertEqual(http('POST', '/wolves', {'name': 'x'}, None)[0], 401)
        finally:
            server.stop()

# --------------------------------------------------------------------------- The identity check

class IdentityProcess(unittest.TestCase):
    def test_it_comes_back_after_a_crash(self):
        identity = central_identity.Identity()
        try:
            self.assertTrue(identity.check({'check': 'version'})['ok'])
            identity._process.kill()
            identity._process.wait()
            self.assertTrue(identity.look(LOOK)['ok'])
            self.assertEqual(identity.look({**LOOK, 'sex': 'other'})['ok'], False)
        finally:
            identity.close()

    def test_a_missing_binary_is_reported(self):
        identity = central_identity.Identity(Path('/nonexistent/ratw_identity'))
        with self.assertRaises(central_identity.IdentityError):
            identity.check({'check': 'version'})

    def test_central_says_so_when_it_cannot_check(self):
        broken = central_identity.Identity(Path('/nonexistent/ratw_identity'))
        wipe()
        keydir = tempfile.TemporaryDirectory()
        try:
            central = cs.Central(STORE, ct.KeyRing(Path(keydir.name)), broken, clock=Clock(),
                                 settings=cs.Settings(iterations=1000))
            token = central.call(central.register, '203.0.113.1', None,
                                 {'username': 'ash', 'password': PASSWORD, 'birth_year': 1990})[1]['access_token']
            status, reply = central.call(central.create_wolf, '203.0.113.1', token,
                                         {'name': 'Ash', 'age': 7, 'appearance': LOOK})
            self.assertEqual((status, reply['code']), (503, 'identity_unavailable'))
        finally:
            keydir.cleanup()

    def test_central_refuses_a_world_database(self):
        for name in ('ratw_prod', 'ratw_ridgemere2', 'ratw_dev'):
            with self.assertRaises(central_store.world_db.DatabaseError):
                central_store.conninfo(name)


if __name__ == '__main__':
    unittest.main()
