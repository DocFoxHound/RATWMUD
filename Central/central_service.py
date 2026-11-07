"""What central does (Docs/Design/77-central-accounts-and-hosted-worlds.md, sections 4 and 5), apart from HTTP: each
call takes the caller's address, its bearer token (if any) and the JSON body, and returns (HTTP status, JSON reply).
ratw_central.py puts it on the network; the tests call it directly as well.

Rules kept from the world's accounts (Core/RatwAccountsCore): usernames 3-32 (a letter first; letters, digits, _ or
-; compared in lower case), passwords 12-128 bytes without control characters (at most 128 UTF-16 units), PBKDF2-HMAC-
SHA256 at 600,000 rounds with a 32-byte salt, and a missing user costs the same hash as a real one. The hashing runs on
a few threads of its own: sign-ins past that wait their turn (doc 77, 4.3: queue, don't fail), and a refresh skips it.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
import datetime as dt
import hashlib
import hmac
import os
import re
import secrets
import threading
import time
import unicodedata

import central_store
from central_store import Conflict
import central_tickets
from central_identity import IdentityError

PASSWORD_ITERATIONS = 600_000
WOLF_SLOTS = 6                      # As the world's accounts (accounts::CharacterSlots).
MIN_AGE = 16                        # Doc 77, Q8.
ACCESS_LIFETIME = 15 * 60
REFRESH_LIFETIME = 30 * 86400       # "Remember me".
SESSION_LIFETIME = 12 * 3600        # Otherwise.
NAME_RESERVED = 30 * 86400          # A deleted account's names, held back.
USERNAME = re.compile(r'^[a-z][a-z0-9_-]{2,31}$')
WORLD_ID = re.compile(r'^world-[0-9a-f]{32}$')
WOLF_ID = re.compile(r'^wolf-[0-9a-f]{32}$')
BAD_SIGN_IN = 'That username and password do not match an account.'


# --------------------------------------------------------------------------- Rules

def normalize_username(given) -> str | None:
    if not isinstance(given, str) or not 3 <= len(given) <= 32 or not given.isascii():
        return None
    lower = given.lower()
    return lower if USERNAME.match(lower) else None


def valid_password(password) -> bool:
    if not isinstance(password, str):
        return False
    try:
        raw = password.encode('utf-8')
    except UnicodeEncodeError:              # (A lone surrogate from JSON's \ud800.)
        return False
    units = sum(2 if ord(c) > 0xFFFF else 1 for c in password)
    return 12 <= len(raw) <= 128 and units <= 128 and not any(b < 32 or b == 127 for b in raw)


def name_key(name: str) -> str:
    """Names are unique by this: compatibility-normalised, case-folded, runs of spaces as one."""
    return ' '.join(unicodedata.normalize('NFKC', name).casefold().split())


def old_enough(birth_year, birth_month, today: dt.date) -> bool | None:
    """16 or older by the birth year (and, only in the year they turn 16, the month). None if the answer is needed or
    malformed."""
    if not isinstance(birth_year, int) or isinstance(birth_year, bool) or not 1900 <= birth_year <= today.year:
        return None
    age = today.year - birth_year
    if age != MIN_AGE:
        return age > MIN_AGE
    if not isinstance(birth_month, int) or isinstance(birth_month, bool) or not 1 <= birth_month <= 12:
        return None
    return birth_month <= today.month          # (Turned 16 by this month: the conservative reading of the month.)


def hash_password(password: str, iterations: int = PASSWORD_ITERATIONS) -> dict:
    salt = secrets.token_bytes(32)
    digest = hashlib.pbkdf2_hmac('sha256', password.encode('utf-8'), salt, iterations, 32)
    return {'algorithm': 'pbkdf2-sha256', 'iterations': iterations, 'salt': salt.hex(), 'hash': digest.hex()}


def password_matches(record: dict | None, password: str, iterations: int = PASSWORD_ITERATIONS) -> bool:
    """Always one PBKDF2 at the record's rounds (or the default for no record), so a missing user costs the same."""
    known = isinstance(record, dict) and record.get('algorithm') == 'pbkdf2-sha256'
    try:
        salt = bytes.fromhex(record['salt']) if known else bytes(32)
        expected = bytes.fromhex(record['hash']) if known else bytes(32)
        rounds = int(record['iterations']) if known else iterations
    except (KeyError, ValueError, TypeError):
        known, salt, expected, rounds = False, bytes(32), bytes(32), iterations
    digest = hashlib.pbkdf2_hmac('sha256', password.encode('utf-8', 'replace'), salt, rounds, 32)
    return known and hmac.compare_digest(digest, expected)


def token_hash(token: str) -> str:
    return hashlib.sha256(token.encode('utf-8')).hexdigest()


# --------------------------------------------------------------------------- Rate limits

@dataclass
class Limits:
    """(count, seconds) per key. Sign-ins and registrations per address; failed sign-ins per account; tickets per
    account and per world; new wolves per account; and every call per address."""
    any_per_address: tuple = (300, 60)
    register_per_address: tuple = (5, 3600)
    login_per_address: tuple = (30, 600)
    login_failures_per_account: tuple = (10, 900)
    refresh_per_address: tuple = (60, 600)
    wolves_per_account: tuple = (10, 3600)
    changes_per_account: tuple = (60, 3600)
    tickets_per_account: tuple = (20, 60)
    tickets_per_world: tuple = (600, 60)
    delete_per_address: tuple = (10, 3600)


class RateLimiter:
    """Sliding windows in memory (one central process). `hit` counts and answers whether the call may go on; `peek`
    only answers; `retry_after` says how long until there is room."""

    def __init__(self, clock):
        self.clock = clock
        self._events: dict[tuple, deque] = {}
        self._lock = threading.Lock()

    def _window(self, key, seconds, now):
        events = self._events.setdefault(key, deque())
        while events and events[0] <= now - seconds:
            events.popleft()
        return events

    def peek(self, key, limit: tuple) -> float:
        """0 if there is room, else the seconds until there is."""
        count, seconds = limit
        now = self.clock()
        with self._lock:
            events = self._window(key, seconds, now)
            return 0.0 if len(events) < count else max(0.001, events[0] + seconds - now)

    def hit(self, key, limit: tuple) -> float:
        count, seconds = limit
        now = self.clock()
        with self._lock:
            events = self._window(key, seconds, now)
            if len(events) >= count:
                return max(0.001, events[0] + seconds - now)
            events.append(now)
            return 0.0

    def sweep(self, longest: float = 86400):
        now = self.clock()
        with self._lock:
            for key in [k for k, e in self._events.items() if not e or e[-1] <= now - longest]:
                del self._events[key]


# --------------------------------------------------------------------------- The service

class Reply(Exception):
    def __init__(self, status: int, error: str, code: str, retry_after: float | None = None):
        super().__init__(error)
        self.status, self.error, self.code, self.retry_after = status, error, code, retry_after


def ok(**fields) -> tuple[int, dict]:
    return 200, {'ok': True, **fields}


def refused(status: int, error: str, code: str) -> Reply:
    return Reply(status, error, code)


@dataclass
class Settings:
    iterations: int = PASSWORD_ITERATIONS
    hash_threads: int = max(1, min(4, (os.cpu_count() or 2) // 2))
    hash_wait: float = 30.0                 # a sign-in waits this long for a hashing thread, then 503
    limits: Limits = field(default_factory=Limits)


class Central:
    def __init__(self, store: central_store.Store, keys: central_tickets.KeyRing, identity, clock=time.time,
                 settings: Settings | None = None):
        self.store, self.keys, self.identity, self.clock = store, keys, identity, clock
        self.settings = settings or Settings()
        self.limiter = RateLimiter(clock)
        self._hashers = threading.BoundedSemaphore(self.settings.hash_threads)

    # ---------------------------------------------------------------- Plumbing

    def _limit(self, key, limit: tuple, what: str):
        wait = self.limiter.hit(key, limit)
        if wait:
            raise Reply(429, f'Too many {what}. Try again in {int(wait) + 1} seconds.', 'rate_limited', wait)

    def _hash(self, work):
        """Runs password work on one of the hashing threads' places, waiting for one if all are busy."""
        if not self._hashers.acquire(timeout=self.settings.hash_wait):
            raise Reply(503, 'Central is busy signing others in. Try again in a moment.', 'busy', 5)
        try:
            return work()
        finally:
            self._hashers.release()

    def _session(self, token: str | None) -> dict:
        if not token:
            raise refused(401, 'Sign in first.', 'unauthenticated')
        session = self.store.session_by_access(token_hash(token))
        if session is None or session['access_expires'] <= self.clock():
            raise refused(401, 'Your sign-in has expired. Sign in again.', 'unauthenticated')
        return session

    def _account(self, token: str | None) -> tuple[dict, dict]:
        session = self._session(token)
        account = self.store.account(session['account_id'])
        if account is None:
            raise refused(401, 'Sign in first.', 'unauthenticated')
        if account['banned']:
            raise refused(403, 'This account is banned.', 'banned')
        return session, account

    def _tokens(self, account_id: str, remember: bool, now: float) -> dict:
        access, refresh = secrets.token_urlsafe(32), secrets.token_urlsafe(32)
        lifetime = REFRESH_LIFETIME if remember else SESSION_LIFETIME
        self.store.create_session('sess-' + secrets.token_hex(16), account_id, token_hash(access),
                                  now + ACCESS_LIFETIME, token_hash(refresh), now + lifetime, remember, now)
        return {'access_token': access, 'access_expires': now + ACCESS_LIFETIME, 'refresh_token': refresh,
                'refresh_expires': now + lifetime}

    @staticmethod
    def _public_account(account: dict) -> dict:
        return {'id': account['id'], 'username': account['username'], 'host': account['host'],
                'admin': account['admin']}

    @staticmethod
    def _public_wolf(wolf: dict) -> dict:
        return {'id': wolf['id'], 'name': wolf['name'], 'sex': wolf['sex'], 'age': wolf['age'],
                'appearance': wolf['appearance'], 'family': wolf['family'], 'profile': wolf['profile'],
                'revision': wolf['revision'], 'created': wolf['created_at'], 'updated': wolf['updated_at']}

    def call(self, method, *args) -> tuple[int, dict]:
        """Runs a call, turning a Reply into its answer."""
        try:
            return method(*args)
        except Reply as reply:
            body = {'ok': False, 'error': reply.error, 'code': reply.code}
            if reply.retry_after is not None:
                body['retry_after'] = round(reply.retry_after, 3)
            return reply.status, body
        except IdentityError:
            return 503, {'ok': False, 'error': 'Wolves cannot be checked just now. Try again later.',
                         'code': 'identity_unavailable'}

    # ---------------------------------------------------------------- Accounts

    def register(self, address: str, token, body: dict) -> tuple[int, dict]:
        """{"username", "password", "birth_year", "birth_month"?, "remember"?} -> the account and its tokens."""
        self._limit(('register', address), self.settings.limits.register_per_address, 'new accounts from here')
        username = normalize_username(body.get('username'))
        password = body.get('password')
        if username is None or not valid_password(password):
            raise refused(400, 'Use a 3–32 character username starting with a letter (letters, digits, _ or -), '
                               'and a 12–128 byte password without control characters.', 'invalid')
        now = self.clock()
        old = old_enough(body.get('birth_year'), body.get('birth_month'), dt.datetime.fromtimestamp(now, dt.timezone.utc).date())
        if old is None:
            raise refused(400, 'Give your year of birth (and, if you turn 16 this year, the month).', 'birth_year')
        if not old:
            raise refused(403, 'You must be 16 or older to play.', 'too_young')
        record = self._hash(lambda: hash_password(password, self.settings.iterations))
        account_id = 'acct-' + secrets.token_hex(16)
        try:
            self.store.create_account(account_id, username, record, self.clock())
        except Conflict:
            raise refused(409, 'This account cannot be registered. Try another username, or sign in to an '
                               'existing account.', 'taken') from None
        account = self.store.account(account_id)
        return ok(account=self._public_account(account), **self._tokens(account_id, bool(body.get('remember')), now))

    def login(self, address: str, token, body: dict) -> tuple[int, dict]:
        """{"username", "password", "remember"?} -> the account and its tokens."""
        limits = self.settings.limits
        self._limit(('login', address), limits.login_per_address, 'sign-in attempts from here')
        username = normalize_username(body.get('username'))
        password = body.get('password')
        if username is None or not isinstance(password, str) or len(password) > 256:
            raise refused(401, BAD_SIGN_IN, 'bad_sign_in')
        if self.limiter.peek(('login-fail', username), limits.login_failures_per_account):
            wait = self.limiter.peek(('login-fail', username), limits.login_failures_per_account)
            raise Reply(429, f'Too many failed sign-ins for this account. Try again in {int(wait) + 1} seconds.',
                        'rate_limited', wait)
        account = self.store.account_by_username(username)
        matched = self._hash(lambda: password_matches(account['password'] if account else None, password,
                                                      self.settings.iterations))
        if not matched:
            self.limiter.hit(('login-fail', username), limits.login_failures_per_account)
            raise refused(401, BAD_SIGN_IN, 'bad_sign_in')
        if account['banned']:
            raise refused(403, 'This account is banned.', 'banned')
        now = self.clock()
        return ok(account=self._public_account(account), **self._tokens(account['id'], bool(body.get('remember')), now))

    def refresh(self, address: str, token, body: dict) -> tuple[int, dict]:
        """{"refresh_token"} -> new tokens (both change; the old refresh token is spent)."""
        self._limit(('refresh', address), self.settings.limits.refresh_per_address, 'refreshes from here')
        given = body.get('refresh_token')
        if not isinstance(given, str) or not given or len(given) > 256:
            raise refused(401, 'Sign in again.', 'unauthenticated')
        now = self.clock()
        access, refresh = secrets.token_urlsafe(32), secrets.token_urlsafe(32)
        # "Remember me" rolls on for 30 days from each refresh; otherwise the session ends 12 hours after sign-in.
        status, session = self.store.refresh(
            token_hash(given), now, token_hash(access), now + ACCESS_LIFETIME, token_hash(refresh),
            lambda s: now + REFRESH_LIFETIME if s['remember'] else s['created_at'] + SESSION_LIFETIME)
        if status != 'ok':
            raise refused(401, 'Sign in again.', 'refresh_reused' if status == 'reused' else 'unauthenticated')
        account = self.store.account(session['account_id'])
        if account is None or account['banned']:
            self.store.end_session(session['id'])
            raise refused(403 if account else 401, 'This account is banned.' if account else 'Sign in again.',
                          'banned' if account else 'unauthenticated')
        return ok(account=self._public_account(account), access_token=access, access_expires=now + ACCESS_LIFETIME,
                  refresh_token=refresh, refresh_expires=session['refresh_expires'])

    def logout(self, address: str, token, body: dict) -> tuple[int, dict]:
        """Ends this session ({"everywhere": true}: every session of the account)."""
        session = self._session(token)
        if body.get('everywhere'):
            self.store.end_sessions(session['account_id'])
        else:
            self.store.end_session(session['id'])
        return ok()

    def delete_account(self, address: str, token, body: dict) -> tuple[int, dict]:
        """{"password"}: the account, its sessions and its wolves go now; the username and the wolves' names are held
        back for 30 days; the wolves are listed for the worlds to forget."""
        self._limit(('delete', address), self.settings.limits.delete_per_address, 'deletions from here')
        session = self._session(token)
        account = self.store.account(session['account_id'])
        password = body.get('password')
        if account is None or not isinstance(password, str) or len(password) > 256:
            raise refused(401, 'Give your password to delete your account.', 'bad_sign_in')
        if self.limiter.peek(('login-fail', account['username']), self.settings.limits.login_failures_per_account):
            raise Reply(429, 'Too many failed attempts for this account. Try again later.', 'rate_limited', 60)
        if not self._hash(lambda: password_matches(account['password'], password, self.settings.iterations)):
            self.limiter.hit(('login-fail', account['username']), self.settings.limits.login_failures_per_account)
            raise refused(401, 'Give your password to delete your account.', 'bad_sign_in')
        now = self.clock()
        gone = self.store.delete_account(account['id'], now, now + NAME_RESERVED)
        return ok(deleted=account['id'], wolves=gone['wolves'] if gone else [])

    def me(self, address: str, token, body: dict) -> tuple[int, dict]:
        _, account = self._account(token)
        return ok(account=self._public_account(account))

    # ---------------------------------------------------------------- Wolves

    def list_wolves(self, address: str, token, body: dict) -> tuple[int, dict]:
        _, account = self._account(token)
        return ok(wolves=[self._public_wolf(w) for w in self.store.wolves(account['id'])], slots=WOLF_SLOTS)

    def create_wolf(self, address: str, token, body: dict) -> tuple[int, dict]:
        """{"name", "age", "appearance", "family"?, "profile"?: fields} -> the wolf, as ratw_identity made it."""
        _, account = self._account(token)
        self._limit(('changes', account['id']), self.settings.limits.changes_per_account, 'attempts')
        unknown = set(body) - {'name', 'age', 'appearance', 'family', 'profile'}
        if unknown:
            raise refused(400, 'A wolf is a name, an age, a look, a Gift family and a profile; not ' +
                          ', '.join(sorted(unknown)) + '.', 'invalid')
        wolf_in = {k: body[k] for k in ('name', 'age', 'appearance', 'family') if k in body}
        checked = self.identity.wolf(wolf_in)
        if not checked.get('ok'):
            raise refused(400, checked.get('error', 'That wolf cannot be made.'), 'invalid')
        wolf = checked['wolf']
        profile = {}
        if body.get('profile') is not None:
            made = self.identity.profile(None, body['profile'])
            if not made.get('ok'):
                raise refused(400, made.get('error', 'That profile cannot be saved.'), 'invalid_profile')
            profile = made['profile']
        self._limit(('wolves', account['id']), self.settings.limits.wolves_per_account, 'new wolves')
        record = {'id': 'wolf-' + secrets.token_hex(16), 'account_id': account['id'], 'name': wolf['name'],
                  'name_key': name_key(wolf['name']), 'sex': wolf['sex'], 'age': int(wolf['age']),
                  'appearance': wolf['appearance'], 'family': wolf['family'], 'profile': profile}
        try:
            full = self.store.create_wolf(record, WOLF_SLOTS, self.clock())
        except Conflict:
            raise refused(409, 'That name is taken. Choose another.', 'name_taken') from None
        if full:
            raise refused(409, f'An account has at most {WOLF_SLOTS} wolves.', 'full')
        return ok(wolf=self._public_wolf(self.store.wolf(record['id'])))

    def name_free(self, address: str, token, body: dict) -> tuple[int, dict]:
        """{"name"} -> whether a new wolf could take it (the creator's hint; the create is what decides)."""
        _, account = self._account(token)
        self._limit(('changes', account['id']), self.settings.limits.changes_per_account, 'checks')
        name = body.get('name')
        if not isinstance(name, str) or len(name) > 256:
            raise refused(400, 'Give a name.', 'invalid')
        return ok(free=self.store.name_free(name_key(name.strip()), self.clock()))

    def patch_wolf(self, address: str, token, body: dict, wolf_id: str) -> tuple[int, dict]:
        """{"revision", "appearance"?, "profile"?: fields}: the look (never the sex) and the roleplay profile may
        change; the name, sex, age and family never do."""
        _, account = self._account(token)
        self._limit(('changes', account['id']), self.settings.limits.changes_per_account, 'changes')
        wolf = self.store.wolf(wolf_id) if WOLF_ID.match(wolf_id or '') else None
        if wolf is None or wolf['account_id'] != account['id']:
            raise refused(404, 'That wolf is not on this account.', 'not_found')
        unknown = set(body) - {'revision', 'appearance', 'profile'}
        if unknown:
            raise refused(400, 'Only the look and the profile can change: ' + ', '.join(sorted(unknown)) + ' cannot.',
                          'immutable')
        revision = body.get('revision')
        if not isinstance(revision, int) or isinstance(revision, bool):
            raise refused(400, 'Say which revision of the wolf you are changing.', 'invalid')
        appearance = profile = None
        if 'appearance' in body:
            looked = self.identity.look(body['appearance'])
            if not looked.get('ok'):
                raise refused(400, looked.get('error', 'That look cannot be saved.'), 'invalid')
            if looked['sex'] != wolf['sex']:
                raise refused(400, "A wolf's sex never changes.", 'immutable')
            appearance = looked['appearance']
        if 'profile' in body:
            made = self.identity.profile(wolf['profile'] or None, body['profile'])
            if not made.get('ok'):
                raise refused(400, made.get('error', 'That profile cannot be saved.'), 'invalid_profile')
            profile = made['profile']
        if appearance is None and profile is None:
            return ok(wolf=self._public_wolf(wolf))
        changed = self.store.update_wolf(wolf_id, revision, self.clock(), appearance, profile)
        if changed is None:
            raise refused(409, 'The wolf was changed elsewhere. Reload it and try again.', 'stale')
        return ok(wolf=self._public_wolf(changed))

    # ---------------------------------------------------------------- Tickets and keys

    def ticket(self, address: str, token, body: dict) -> tuple[int, dict]:
        """{"world", "wolf"} -> a signed ticket for that wolf on that world (doc 77, 5): about 60 s, used once."""
        _, account = self._account(token)
        world_id, wolf_id = body.get('world'), body.get('wolf')
        if not isinstance(world_id, str) or not WORLD_ID.match(world_id) or not isinstance(wolf_id, str) or \
                not WOLF_ID.match(wolf_id):
            raise refused(400, 'Name a world and one of your wolves.', 'invalid')
        self._limit(('tickets', account['id']), self.settings.limits.tickets_per_account, 'joins')
        world = self.store.world(world_id)
        if world is None or world['delisted']:
            raise refused(404, 'That world is not open.', 'no_world')
        wolf = self.store.wolf(wolf_id)
        if wolf is None or wolf['account_id'] != account['id']:
            raise refused(404, 'That wolf is not on this account.', 'not_found')
        self._limit(('tickets-world', world_id), self.settings.limits.tickets_per_world, 'joins to this world')
        now = int(self.clock())
        sub = account['id'] if world['kind'] == 'official' else \
            central_tickets.pairwise_id(self.keys.pairwise_secret(), account['id'], world_id)
        claims = {'sub': sub, 'wolf': wolf['id'], 'name': wolf['name'], 'sex': wolf['sex'], 'age': wolf['age'],
                  'look': wolf['appearance'], 'gift': wolf['family'], 'aud': world_id, 'kind': world['kind'],
                  'jti': secrets.token_hex(16), 'iat': now, 'exp': now + central_tickets.LIFETIME}
        ticket, kid = self.keys.sign(claims, now)
        self.store.record_ticket(claims['jti'], world_id, wolf['id'], account['id'], kid, now, claims['exp'])
        return ok(ticket=ticket, expires=claims['exp'], world=world_id, kind=world['kind'])

    def jwks(self, address: str, token, body: dict) -> tuple[int, dict]:
        return 200, self.keys.jwks(self.clock())

    # ---------------------------------------------------------------- Housekeeping

    def maintain(self) -> list[str]:
        now = self.clock()
        done = self.keys.maintain(now)
        pruned = self.store.prune(now)
        self.limiter.sweep()
        return done + [f'pruned {k}: {v}' for k, v in pruned.items() if v]
