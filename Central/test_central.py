#!/usr/bin/env python3
"""Central's tests (Docs/Design/77-central-accounts-and-hosted-worlds.md, Phase 1; section 11).

    python3 Central/test_central.py            (or: python3 -m unittest discover -s Central)

They run against a throwaway database, `ratw_central_test`, made on the local Postgres (Database/compose.yaml) with the
superuser from Database/.env and dropped at the end; no world's database is touched, and no role is made or changed.
They need a built ratw_identity (RATW_IDENTITY, else build-core/, build-central/ or Build/). Time is a fake clock, and
passwords use 1,000 PBKDF2 rounds (one test checks the real 600,000).
"""
from __future__ import annotations

import base64
import datetime as dt
import hashlib
import hmac
import json
from pathlib import Path
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
import ratw_central  # noqa: E402

TEST_DB = 'ratw_central_test'
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
        self.assertEqual(self.http('POST', '/account/logout', {}, token)[0], 200)
        self.assertEqual(self.http('GET', '/account', token=token)[0], 401)
        self.assertEqual(self.http('GET', '/health')[0], 200)

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
