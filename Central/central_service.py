"""What central does (Docs/Design/77-central-accounts-and-hosted-worlds.md, sections 4 and 5), apart from HTTP: each
call takes the caller's address, its bearer token (if any) and the JSON body, and returns (HTTP status, JSON reply).
ratw_central.py puts it on the network; the tests call it directly as well.

Rules kept from the world's accounts (Core/RatwAccountsCore): usernames 3-32 (a letter first; letters, digits, _ or
-; compared in lower case), passwords 12-128 bytes without control characters (at most 128 UTF-16 units), PBKDF2-HMAC-
SHA256 at 600,000 rounds with a 32-byte salt, and a missing user costs the same hash as a real one. The hashing runs on
a few threads of its own: sign-ins past that wait their turn (doc 77, 4.3: queue, don't fail), and a refresh skips it.

The server list (doc 77, 7; Phase 4): host tokens, the world agent's heartbeat and leave, central's call-back to a
world's /status, the honest player count, the bans each world is told, the Host page's calls, and the word filter
(central_words) on usernames, wolf names and world listings.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
import datetime as dt
import hashlib
import hmac
import ipaddress
import json
import os
import re
import secrets
import socket
import ssl
import threading
import time
import unicodedata

import central_store
from central_store import Conflict
import central_tickets
import central_words
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

# The server list (doc 77, 7).
HOST_TOKEN_PREFIX = 'rh_'
TOKEN_UNUSED = 90 * 86400           # A host token unused this long is refused, and its world drops off.
HEARTBEAT_EVERY = 60                # What the world agent is told.
HEARTBEAT_LASTS = 180               # A world with no heartbeat for this long is off the list.
CHECK_EVERY = 600                   # Central's call-back to a listed world.
CHECK_TIMEOUT = 5.0
STATUS_MOST = 2048                  # The most of a /status reply's body central reads (the world caps it at 2 KB).
STATUS_HEAD_MOST = 4096             # And of its status line and headers.
TICKETS_COUNTED = 86400             # A wolf online counts if its latest ticket (to any world) was for this world, this
                                    # recently, and it and its account are in good standing.
HOST_WORLDS = 3                     # Worlds an account may host (not counting removed ones).
ONLINE_MOST = 10000
HOST_NAME = re.compile(r'^(?=.{1,253}$)([A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?)(\.[A-Za-z0-9]([A-Za-z0-9-]{0,61}'
                       r'[A-Za-z0-9])?)*\.?$')
CGNAT = ipaddress.ip_network('100.64.0.0/10')
# Names that are never on the public internet's DNS, refused before central looks them up (and single-label names).
LOCAL_NAMES = ('.internal', '.local', '.localhost', '.lan', '.home.arpa')
GAME_NAMES = ('official', 'runagainsttheworld', 'runsagainsttheworld')
_SIGNS = central_words.WORD_SIGNS
# R-A-T-W as a word of its own (stops and spaces between its letters, digits after it, or "RATWorld").
GAME_INITIALS = re.compile(f'(?<![{_SIGNS}])' + f'[^{_SIGNS}]*'.join(central_words.look_alike(c) for c in 'ratw') +
                           f'(?:(?![{_SIGNS}])|{central_words.look_alike("orld")})')
NOT_ON_THE_INTERNET = "This world's address (IP) is not on the internet."


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


def invisible(c: str) -> bool:
    """A character that shows nothing of its own in a name: a format character (zero-width spaces and joiners, soft
    hyphens, direction marks), an enclosing mark, a variation selector or the grapheme joiner."""
    return unicodedata.category(c) in ('Cf', 'Me') or '\ufe00' <= c <= '\ufe0f' or \
        '\U000e0100' <= c <= '\U000e01ef' or c in '\u034f\u180b\u180c\u180d\u180f'


def name_key(name: str) -> str:
    """Names are unique by this: compatibility-normalised, case-folded, invisible characters taken out, runs of spaces
    as one."""
    folded = unicodedata.normalize('NFKC', name).casefold()
    return ' '.join(''.join(c for c in folded if not invisible(c)).split())


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


def new_host_token() -> str:
    """A host token (doc 77, 7): shown once, kept only as its hash."""
    return HOST_TOKEN_PREFIX + secrets.token_urlsafe(32)


# --------------------------------------------------------------------------- The server list's fields

def plain_text(value, most: int, least: int = 0) -> str | None:
    """A string of `least` to `most` characters once trimmed, with no control or format characters (the joiner
    emoji use apart); None if not."""
    if not isinstance(value, str):
        return None
    value = value.strip()
    if not least <= len(value) <= most:
        return None
    if any(unicodedata.category(c).startswith('C') and c != '\u200d' for c in value):
        return None
    return value


def whole_number(value, least: int, most: int) -> bool:
    return isinstance(value, int) and not isinstance(value, bool) and least <= value <= most


def is_ip_literal(text: str) -> bool:
    try:
        ipaddress.ip_address(text.strip('[]'))
        return True
    except ValueError:
        return bool(re.fullmatch(r'[0-9.]+', text))      # (Not a name: "10.1" or "1.2.3.4.5".)


def plain_ip(address: str):
    """An address as one form: an IPv4 address mapped into IPv6 is the IPv4 address, and a zone is dropped."""
    ip = ipaddress.ip_address(address.split('%', 1)[0])
    return ip.ipv4_mapped if ip.version == 6 and ip.ipv4_mapped else ip


def address_block(address: str) -> str:
    """The block one sender is counted by: an IPv4 address itself; an IPv6 address's /64 (one home or server has a
    whole /64 to pick from)."""
    try:
        ip = plain_ip(address)
    except (ValueError, AttributeError):
        return str(address)
    return str(ipaddress.ip_network(f'{ip}/64', strict=False)) if ip.version == 6 else str(ip)


def bracketed(address: str) -> str:
    return f'[{address}]' if ':' in address else address


def whole_seconds(value: float | None) -> int | None:
    return None if value is None else int(value)


def listing_refused(fields: dict, kind: str) -> str:
    """'' if a world's name and listing text may be shown; else why not. The word filter for every world; and a
    community world can't call itself official or use the game's name (doc 77, 7: the badge is central's), in its
    name, description, region or any tag, read as central_words reads text (look-alike letters of other scripts, 0 as
    o, 1 and | as i or l, stops and spaces between letters)."""
    texts = [fields.get('name') or '', fields.get('description') or '', fields.get('region') or '',
             *(fields.get('tags') or [])]
    if any(central_words.refused(t) for t in texts):
        return 'That name or description uses a word central does not allow.'
    if kind != 'official':
        for text in texts:
            if central_words.holds(text, GAME_NAMES) or \
                    any(GAME_INITIALS.search(form) for form in central_words.forms(text)):
                return ('A community world cannot call itself official or use the game\'s name (Run Against the '
                        'World, RATW).')
    return ''


def read_status(sock, deadline: float) -> bytes | str:
    """Reads a world's answer to GET /status: its body (at most 2 KB), or why it can't be used. Only a 200 is taken;
    a redirect is never followed. The whole answer must arrive by the deadline: a body of known length, else what
    comes before the world closes the connection (central asked it to), chunked or not."""
    data, head, length, chunked = b'', None, None, False
    while True:
        left = deadline - time.monotonic()
        if left <= 0:
            raise TimeoutError
        sock.settimeout(left)
        piece = sock.recv(4096)
        if not piece:
            break
        data += piece
        if head is None:
            end = data.find(b'\r\n\r\n')
            if end < 0:
                if len(data) > STATUS_HEAD_MOST:
                    return 'its reply\'s headers are too long'
                continue
            head, data = data[:end].decode('latin-1'), data[end + 4:]
            lines = head.split('\r\n')
            match = re.match(r'HTTP/1\.[01] (\d{3})', lines[0])
            if not match:
                return 'it does not answer HTTP'
            code = int(match.group(1))
            if 300 <= code < 400:
                return f'it answered with a redirect (HTTP {code}), which central does not follow'
            if code != 200:
                return f'its /status answered HTTP {code}'
            for line in lines[1:]:
                key, _, value = line.partition(':')
                key, value = key.strip().lower(), value.strip()
                if key == 'content-length':
                    if not value.isdigit():
                        return 'its reply has a bad Content-Length'
                    length = int(value)
                    if length > STATUS_MOST:
                        return 'its /status is over 2 KB'
                elif key == 'transfer-encoding' and value.lower() != 'identity':
                    chunked = True
        if head is not None:
            if len(data) > (STATUS_MOST + 512 if chunked else STATUS_MOST):
                return 'its /status is over 2 KB'
            if length is not None and not chunked and len(data) >= length:
                return data[:length]
    if head is None:
        return 'it closed the connection without answering'
    if chunked:
        body, rest = b'', data
        while True:
            size, _, rest = rest.partition(b'\r\n')
            try:
                size = int(size.split(b';')[0], 16)
            except ValueError:
                return 'its reply is badly chunked'
            if size == 0:
                break
            body, rest = body + rest[:size], rest[size + 2:]
        return body if len(body) <= STATUS_MOST else 'its /status is over 2 KB'
    if length is not None and len(data) < length:
        return 'its reply was cut short'
    return data


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
    heartbeats_per_token: tuple = (10, 60)          # (per world: the token's world, whichever token it has)
    bad_tokens_per_address: tuple = (30, 60)        # unknown host tokens, per address (per /64 for IPv6)
    host_changes_per_account: tuple = (30, 3600)


class RateLimiter:
    """Sliding windows in memory (one central process). `hit` counts and answers whether the call may go on; `peek`
    only answers; `retry_after` says how long until there is room."""

    def __init__(self, clock):
        self.clock = clock
        self._events: dict[tuple, deque] = {}
        self._spans: dict[tuple, float] = {}     # each key's window, so a sweep knows when its events are all old
        self._lock = threading.Lock()

    def _window(self, key, seconds, now, make: bool):
        events = self._events.get(key)
        if events is None:
            if not make:
                return ()
            events = self._events[key] = deque()
        self._spans[key] = max(seconds, self._spans.get(key, 0))
        while events and events[0] <= now - seconds:
            events.popleft()
        return events

    def peek(self, key, limit: tuple) -> float:
        """0 if there is room, else the seconds until there is. (A key never hit is not kept.)"""
        count, seconds = limit
        now = self.clock()
        with self._lock:
            events = self._window(key, seconds, now, False)
            return 0.0 if len(events) < count else max(0.001, events[0] + seconds - now)

    def hit(self, key, limit: tuple) -> float:
        count, seconds = limit
        now = self.clock()
        with self._lock:
            events = self._window(key, seconds, now, True)
            if len(events) >= count:
                return max(0.001, events[0] + seconds - now)
            events.append(now)
            return 0.0

    def sweep(self, longest: float = 86400):
        """Drops every key whose events have all left its window (or are older than `longest`)."""
        now = self.clock()
        with self._lock:
            for key in [k for k, e in self._events.items()
                        if not e or e[-1] <= now - min(self._spans.get(k, longest), longest)]:
                del self._events[key]
                self._spans.pop(key, None)

    def __len__(self) -> int:
        return len(self._events)


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
    local_worlds: bool = False              # development: list worlds on loopback and private addresses (serve
                                            # --local-worlds); link-local, unspecified and multicast never
    check_timeout: float = CHECK_TIMEOUT    # central's call-back to a world, all of it


class Central:
    def __init__(self, store: central_store.Store, keys: central_tickets.KeyRing, identity, clock=time.time,
                 settings: Settings | None = None):
        self.store, self.keys, self.identity, self.clock = store, keys, identity, clock
        self.settings = settings or Settings()
        self.limiter = RateLimiter(clock)
        self._hashers = threading.BoundedSemaphore(self.settings.hash_threads)
        self._rules: dict | None = None

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
        if central_words.refused(username):
            raise refused(400, 'That username is not allowed. Choose another.', 'name_refused')
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
        name_refused = refused(400, 'That name is not allowed. Choose another.', 'name_refused')
        hidden = refused(400, "A wolf's name cannot hold invisible or decorative characters (zero-width spaces, "
                              'joiners, soft hyphens, variation selectors, enclosing marks).', 'invalid')
        if isinstance(wolf_in.get('name'), str):
            if any(invisible(c) for c in wolf_in['name']):
                raise hidden
            if central_words.refused(wolf_in['name']):
                raise name_refused
        checked = self.identity.wolf(wolf_in)
        if not checked.get('ok'):
            raise refused(400, checked.get('error', 'That wolf cannot be made.'), 'invalid')
        wolf = checked['wolf']
        if any(invisible(c) for c in wolf['name']):
            raise hidden
        if central_words.refused(wolf['name']):
            raise name_refused
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

    def delete_wolf(self, address: str, token, body: dict, wolf_id: str) -> tuple[int, dict]:
        """{"password"}: one wolf goes now, the account stays (the user, 2026-11-07). Its name is held back for 30 days,
        as an account's are, and its id is listed for the worlds to forget. The password is asked again, as for
        deleting the account: a stolen access token alone can't take a wolf."""
        self._limit(('delete', address), self.settings.limits.delete_per_address, 'deletions from here')
        _, account = self._account(token)
        password = body.get('password')
        if set(body) - {'password'} or not isinstance(password, str) or len(password) > 256:
            raise refused(401, 'Give your password to delete a wolf.', 'bad_sign_in')
        limit = self.settings.limits.login_failures_per_account
        if self.limiter.peek(('login-fail', account['username']), limit):
            raise Reply(429, 'Too many failed attempts for this account. Try again later.', 'rate_limited', 60)
        if not self._hash(lambda: password_matches(account['password'], password, self.settings.iterations)):
            self.limiter.hit(('login-fail', account['username']), limit)
            raise refused(401, 'Give your password to delete a wolf.', 'bad_sign_in')
        now = self.clock()
        if not WOLF_ID.match(wolf_id or '') or not self.store.delete_wolf(account['id'], wolf_id, now, now + NAME_RESERVED):
            raise refused(404, 'That wolf is not on this account.', 'not_found')
        return ok(deleted=wolf_id)

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
        if world is None or world['delisted'] or world['removed']:
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
        # Signed at central's own clock, not the whole second the claims carry: in the second a key was made its
        # activation is still ahead of int(now), and the ring would make a second, unpublished key for this ticket
        # (found by Phase 2's join test, doc 77).
        ticket, kid = self.keys.sign(claims, self.clock())
        self.store.record_ticket(claims['jti'], world_id, wolf['id'], account['id'], kid, now, claims['exp'])
        return ok(ticket=ticket, expires=claims['exp'], world=world_id, kind=world['kind'])

    # ---------------------------------------------------------------- What the client shows

    def rules(self, address: str, token, body: dict) -> tuple[int, dict]:
        """The creator's identity rules (doc 77, 3.1): the Gift families, from the game's own Data/ through
        ratw_identity, as a world's lobby sends them; the slots and the ages. Public; asked once and kept."""
        if self._rules is None:
            got = self.identity.rules()
            if not got.get('ok') or not isinstance(got.get('gifts'), dict):
                raise refused(503, 'The creator\'s rules cannot be read just now.', 'identity_unavailable')
            self._rules = {'gifts': got['gifts'], 'slots': WOLF_SLOTS, 'ages': [6, 99]}
        return ok(**self._rules)

    def list_worlds(self, address: str, token, body: dict) -> tuple[int, dict]:
        """The world list (doc 77, 3.1 and 7). A world is in it while its heartbeat arrives (within 3 minutes) with a
        live host token, from an address central could call back, and it isn't delisted or removed. Each is {id, name
        (the heartbeat's), kind (central's), address (the one to connect to), host (a name to show, or null), secure
        (wss://), protocol, description, players (the honest count), limit, region, tags, voices, cheats}: official
        worlds first, then the most players. A ws:// world's address is the "ip:port" central called back, never its
        name (nothing binds a name to the checked address there, so the name is for showing only); a wss:// world's is
        "host:port" (its certificate binds the name), with the called-back address as "ip" too. Public."""
        out = []
        for w in self.store.listed_worlds(self.clock(), HEARTBEAT_LASTS, TOKEN_UNUSED):
            listing = w['listing'] if isinstance(w['listing'], dict) else {}
            secure = listing.get('secure') is True and bool(w['host_name'])
            entry = {'id': w['id'], 'name': listing.get('name') or w['name'], 'kind': w['kind'],
                     'address': f'{w["host_name"] if secure else bracketed(w["address"])}:{w["port"]}',
                     'host': w['host_name'] or None, 'secure': secure, 'protocol': listing.get('protocol'),
                     'description': listing.get('description', ''), 'players': w['players'],
                     'limit': listing.get('limit'), 'region': listing.get('region', ''),
                     'tags': listing.get('tags', []), 'voices': listing.get('voices') is True,
                     'cheats': listing.get('cheats') is True}
            if secure:
                entry['ip'] = w['address']
            out.append(entry)
        return ok(worlds=out)

    # ---------------------------------------------------------------- The world's own calls (doc 77, 7)

    def _world_token(self, token: str | None, address: str) -> dict:
        """The world a host token names (its 'token_hash' added), or the refusal: 401 for an unknown, revoked or
        expired token; 403 for a delisted or removed world, or a banned host. At most 10 calls a minute a world; and
        unknown tokens are counted per address (per /64 for IPv6), so made-up ones can't grow the limiter's keys."""
        bad = refused(401, 'Central does not know this host token (it was revoked, replaced or never made). Make a '
                           'new one on the Host page.', 'bad_token')
        guesses = ('bad_token', address_block(address))
        wait = self.limiter.peek(guesses, self.settings.limits.bad_tokens_per_address)
        if wait:
            raise Reply(429, f'Too many unknown host tokens from here. Try again in {int(wait) + 1} seconds.',
                        'rate_limited', wait)
        world = None
        if token and token.startswith(HOST_TOKEN_PREFIX) and len(token) <= 128:
            hashed = token_hash(token)
            world = self.store.world_by_token(hashed)
        if world is None:
            self.limiter.hit(guesses, self.settings.limits.bad_tokens_per_address)
            raise bad
        world['token_hash'] = hashed
        self._limit(('heartbeat', world['id']), self.settings.limits.heartbeats_per_token, 'heartbeats')
        if world['removed'] or world['delisted']:
            raise refused(403, 'This world is delisted.' if not world['removed'] else 'This world was removed by its '
                          'host.', 'delisted')
        used = world['token_used'] or world['token_created']
        if used is None or used <= self.clock() - TOKEN_UNUSED:
            raise refused(401, 'This host token went unused for 90 days and has expired. Make a new one on the Host '
                               'page.', 'bad_token')
        if world['host_banned']:
            raise refused(403, "This world's host account is banned.", 'banned')
        return world

    def _listing(self, body: dict, kind: str) -> dict:
        """The heartbeat's fields, checked (unknown ones ignored): 400 'invalid' or 'name_refused'."""
        def bad(what):
            raise refused(400, f"The heartbeat's {what} is not right.", 'invalid')

        port = body.get('port')
        if not whole_number(port, 1, 65535):
            bad('port (1-65535)')
        name = plain_text(body.get('name'), 48, 1)
        if name is None:
            bad('name (1-48 characters)')
        out = {'port': port, 'name': name}
        for key, most in (('description', 300), ('region', 32), ('build', 64), ('clientData', 64)):
            value = plain_text(body.get(key, ''), most) if body.get(key) is not None else ''
            if value is None:
                bad(f'{key} (at most {most} characters)')
            out[key] = value
        tags = body.get('tags', [])
        if tags is None:
            tags = []
        if not isinstance(tags, list) or len(tags) > 5:
            bad('tags (at most 5)')
        out['tags'] = []
        for tag in tags:
            tag = plain_text(tag, 16, 1)
            if tag is None:
                bad('tags (1-16 characters each)')
            if tag not in out['tags']:
                out['tags'].append(tag)
        limit = body.get('limit')
        if limit is not None and not whole_number(limit, 1, 10000):
            bad('limit (1-10000)')
        out['limit'] = limit
        for key in ('private', 'voices', 'cheats', 'secure'):
            value = body.get(key, False)
            if value is None:
                value = False
            if not isinstance(value, bool):
                bad(f'{key} (true or false)')
            out[key] = value
        protocol = body.get('protocol')
        if not whole_number(protocol, 0, 2 ** 31 - 1):
            bad('protocol')
        out['protocol'] = protocol
        host = body.get('host')
        if host in (None, ''):
            host = None
        elif not isinstance(host, str) or not HOST_NAME.match(host) or is_ip_literal(host):
            bad('host (a DNS name)')
        else:
            host = host.rstrip('.').lower()
        out['host'] = host
        online = body.get('online', [])
        if online is None:
            online = []
        if not isinstance(online, list) or len(online) > ONLINE_MOST:
            bad(f'online (a list of at most {ONLINE_MOST} wolf ids)')
        out['online'] = list(dict.fromkeys(w for w in online if isinstance(w, str) and WOLF_ID.match(w)))
        why = listing_refused(out, kind)
        if why:
            raise refused(400, why, 'name_refused')
        return out

    def _address_refused(self, ip) -> str:
        """'' if a world at this address may be listed; else why not."""
        if ip.is_unspecified or ip.is_multicast or ip.is_link_local:
            return NOT_ON_THE_INTERNET
        if self.settings.local_worlds and (ip.is_loopback or ip.is_private or (ip.version == 4 and ip in CGNAT)):
            return ''
        if not ip.is_global or ip.is_private or ip.is_loopback or ip.is_reserved or \
                (ip.version == 4 and ip in CGNAT):
            return NOT_ON_THE_INTERNET
        return ''

    def resolve(self, host: str, port: int) -> set:
        """The addresses a host name resolves to (a test may replace it). Raises OSError when it doesn't."""
        return {plain_ip(info[4][0]) for info in socket.getaddrinfo(host, port, type=socket.SOCK_STREAM)}

    def _host_refused(self, host: str, ip, port: int) -> str:
        """'' if the host name may stand for this world: it resolves to exactly the address the heartbeat came from
        (no other A or AAAA record, which clients might pick instead). Single-label names and local-only suffixes are
        refused without a lookup, and central's own DNS answers are never shown (they could map its own network)."""
        if '.' not in host or any(host == s[1:] or host.endswith(s) for s in LOCAL_NAMES):
            return (f'The host name {host} is not a public DNS name (one label, or a local-only name such as .local or '
                    '.internal). Give a name on the public DNS, or leave the host out.')
        try:
            found = self.resolve(host, port)
        except (OSError, UnicodeError, ValueError):
            return f'The host name {host} does not resolve (DNS). Point it at {ip}, or leave the host out.'
        if found != {ip}:
            return (f"The host name {host} does not point at this world's address ({ip}) alone: give it just this "
                    'one address (no other A or AAAA record), or leave the host out.')
        return ''

    def heartbeat(self, address: str, token, body: dict) -> tuple[int, dict]:
        """POST /worlds/heartbeat (doc 77, 7), from the world agent every 60 s with the world's host token. The address
        is where the request came from, never a field. Central calls the world back (GET /status) before first
        listing, every 10 minutes, and when its address changes. The reply says whether it is listed (and why not),
        the honest count, central's bans as this world sees them (each banned account's ticket `sub` here), and
        central's clock."""
        world = self._world_token(token, address)
        fields = self._listing(body, world['kind'])
        now = self.clock()
        ip = plain_ip(address)
        port, host, secure = fields['port'], fields['host'], fields['secure']
        seen = f'{bracketed(str(ip))}:{port}'
        why = self._address_refused(ip)
        if not why and host:
            why = self._host_refused(host, ip, port)
        if not why and secure and not host:
            why = 'A secure (wss://) world needs a host name (the name on its certificate): give the host.'
        check = None
        if why:                                         # (Refused without calling: central never connects there.)
            check = {'reachable': False, 'note': why, 'address': seen}
        else:
            listing = world['listing'] if isinstance(world['listing'], dict) else {}
            due = not world['reachable'] or world['checked_at'] is None or \
                now - world['checked_at'] >= CHECK_EVERY or world['check_address'] != seen or \
                world['host_name'] != host or (listing.get('secure') is True) != secure
            if due:
                why = self.call_back(ip, port, host, secure, world['id'])
                check = {'reachable': not why, 'note': why, 'address': seen}
        online = fields.pop('online')
        counted = self.store.ticketed_wolves(world['id'], online, now - TICKETS_COUNTED)
        online = [w for w in online if w in counted][:fields['limit'] or ONLINE_MOST]
        listing = {k: fields[k] for k in ('name', 'description', 'region', 'tags', 'limit', 'private', 'voices',
                                          'cheats', 'secure', 'protocol', 'build', 'clientData')}
        if not self.store.heartbeat(world['id'], world['token_hash'], now, str(ip), port, host, listing, online,
                                    check):
            # (Its token was replaced or revoked, or the world delisted, while central called it back.)
            raise refused(401, 'Central does not know this host token (it was revoked, replaced or never made). Make a '
                               'new one on the Host page.', 'bad_token')
        banned = self.store.banned_accounts()
        if world['kind'] != 'official' and banned:
            secret = self.keys.pairwise_secret()
            banned = [central_tickets.pairwise_id(secret, a, world['id']) for a in banned]
        # The world's host (doc 77, 3.2): a community world is told its host account's ticket sub there, so the host's
        # wolves get the host commands. An official world has no host (central's DM tool decides there): None.
        host = central_tickets.pairwise_id(self.keys.pairwise_secret(), world['host_account'], world['id']) \
            if world['kind'] != 'official' and world['host_account'] else None
        return ok(world=world['id'], kind=world['kind'], listed=not why, reachable=not why, why=why, address=seen,
                  players=len(online), bans=banned, host=host, time=now, every=HEARTBEAT_EVERY)

    def leave(self, address: str, token, body: dict) -> tuple[int, dict]:
        """POST /worlds/leave: a world's clean shutdown takes it off the list at once."""
        world = self._world_token(token, address)
        if not self.store.leave(world['id'], world['token_hash']):
            raise refused(401, 'Central does not know this host token (it was revoked, replaced or never made). Make a '
                               'new one on the Host page.', 'bad_token')
        return ok()

    def call_back(self, ip, port: int, host: str | None, secure: bool, world_id: str) -> str:
        """Central's call-back (doc 77, 7): GET /status from the address the heartbeat came from (no second name
        lookup), wss:// worlds over TLS checked against the host name. 5 seconds in all, at most 2 KB of reply, no
        redirects. '' if it answers as this world; else why not, for the host to read."""
        seen = f'{bracketed(str(ip))}:{port}'

        def unreachable(reason):
            return f'Central could not reach this world at {seen}: {reason}. Open port {port} to the internet (TCP).'

        timeout = self.settings.check_timeout
        deadline = time.monotonic() + timeout
        late = f'it did not answer within {timeout:g} seconds'
        try:
            sock = socket.create_connection((str(ip), port), timeout=timeout)
        except (socket.timeout, TimeoutError):
            return unreachable(late)
        except ConnectionRefusedError:
            return unreachable('the connection was refused')
        except OSError as error:
            return unreachable(f'the connection failed ({error.strerror or type(error).__name__})')
        try:
            if secure:
                sock.settimeout(max(0.05, deadline - time.monotonic()))
                sock = ssl.create_default_context().wrap_socket(sock, server_hostname=host)
            name = host or bracketed(str(ip))
            header = name if port == (443 if secure else 80) else f'{name}:{port}'
            sock.settimeout(max(0.05, deadline - time.monotonic()))
            sock.sendall(f'GET /status HTTP/1.1\r\nHost: {header}\r\nUser-Agent: ratw-central/1\r\n'
                         'Accept: application/json\r\nConnection: close\r\n\r\n'.encode('ascii', 'replace'))
            answer = read_status(sock, deadline)
        except (socket.timeout, TimeoutError):
            return unreachable(late)
        except ssl.SSLCertVerificationError as error:
            return unreachable(f'its certificate was refused ({error.verify_message})')
        except ssl.SSLError as error:
            return unreachable(f'the secure connection failed ({error.reason or "TLS"})')
        except OSError as error:
            return unreachable(f'the connection failed ({error.strerror or type(error).__name__})')
        finally:
            sock.close()
        if isinstance(answer, str):
            return unreachable(answer)
        try:
            status = json.loads(answer.decode('utf-8'))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return unreachable('its /status is not JSON')
        if not isinstance(status, dict) or status.get('ratw') != 'world':
            return unreachable('what answers there is not a RATW world (its /status)')
        if status.get('id') != world_id:
            other = status.get('id') if isinstance(status.get('id'), str) and WORLD_ID.match(status['id']) else \
                'another id'
            return (f'The world at {seen} is another world (its /status names {other}), not the one this host token '
                    'is for. Give this world its own token.')
        return ''

    # ---------------------------------------------------------------- The Host page (doc 77, 3.2)

    def _host(self, token) -> dict:
        _, account = self._account(token)
        if not account['host']:
            raise refused(403, 'This account cannot host worlds. In the closed beta, hosts are approved by hand.',
                          'not_host')
        return account

    @staticmethod
    def why_not_listed(w: dict, now: float) -> str:
        """Why a world is not listed ('' when it is): the list's own rules, in the host's words."""
        if w['removed']:
            return 'This world was removed.'
        if w['delisted']:
            return 'Central has delisted this world.'
        if w['host_banned']:
            return "This world's host account is banned."
        if not w['has_token']:
            return 'It has no host token: make a new one.'
        used = w['token_used'] or w['token_created']
        if used is None or used <= now - TOKEN_UNUSED:
            return 'Its host token went unused for 90 days and has expired: make a new one.'
        if w['last_heartbeat'] is None or not w['address'] or not w['port']:
            return 'No heartbeat from it: start the world and its world agent (with the host token).'
        if w['last_heartbeat'] <= now - HEARTBEAT_LASTS:
            minutes = int((now - w['last_heartbeat']) // 60)
            return f'No heartbeat for {minutes} minutes: is its world agent running?'
        if not w['reachable']:
            return w['check_note'] or 'Central has not reached it yet.'
        return ''

    def _host_view(self, w: dict, now: float) -> dict:
        listing = w['listing'] if isinstance(w['listing'], dict) else {}
        why = self.why_not_listed(w, now)
        return {'id': w['id'], 'name': listing.get('name') or w['name'], 'kind': w['kind'], 'listed': not why,
                'reachable': bool(w['reachable']), 'why': why,
                'address': f'{bracketed(w["address"])}:{w["port"]}' if w['address'] and w['port'] else None,
                'players': w['players'], 'last_heartbeat': whole_seconds(w['last_heartbeat']),
                'token_created': whole_seconds(w['token_created'])}

    def _hosted(self, account: dict, world_id: str) -> dict:
        world = self.store.world(world_id) if WORLD_ID.match(world_id or '') else None
        if world is None or world['host_account'] != account['id'] or world['removed']:
            raise refused(404, 'That world is not one of yours.', 'not_found')
        return world

    def host_worlds(self, address: str, token, body: dict) -> tuple[int, dict]:
        """GET /host/worlds: the account's worlds, listed or not and why."""
        account = self._host(token)
        now = self.clock()
        return ok(worlds=[self._host_view(w, now) for w in self.store.host_worlds(account['id'])], most=HOST_WORLDS)

    def host_make_world(self, address: str, token, body: dict) -> tuple[int, dict]:
        """POST /host/worlds {name}: a new community world and its host token, shown this once."""
        account = self._host(token)
        self._limit(('host', account['id']), self.settings.limits.host_changes_per_account, 'changes to your worlds')
        name = plain_text(body.get('name'), 48, 1)
        if name is None:
            raise refused(400, "A world's name is 1-48 characters.", 'invalid')
        why = listing_refused({'name': name}, 'community')
        if why:
            raise refused(400, why, 'name_refused')
        now = self.clock()
        world_id, made = 'world-' + secrets.token_hex(16), new_host_token()
        if not self.store.create_host_world(world_id, name, account['id'], token_hash(made), now, HOST_WORLDS):
            raise refused(403, f'An account hosts at most {HOST_WORLDS} worlds. Remove one first.', 'too_many')
        return ok(world=self._host_view(self.store.world(world_id), now), token=made)

    def host_new_token(self, address: str, token, body: dict, world_id: str) -> tuple[int, dict]:
        """POST /host/worlds/ID/token: a new host token, shown this once; the old one stops working at once."""
        account = self._host(token)
        self._limit(('host', account['id']), self.settings.limits.host_changes_per_account, 'changes to your worlds')
        world = self._hosted(account, world_id)
        made = new_host_token()
        self.store.set_world_token(world['id'], token_hash(made), self.clock())
        return ok(token=made)

    def host_remove_world(self, address: str, token, body: dict, world_id: str) -> tuple[int, dict]:
        """DELETE /host/worlds/ID: removed for good (off the list at once, its token dropped)."""
        account = self._host(token)
        self._limit(('host', account['id']), self.settings.limits.host_changes_per_account, 'changes to your worlds')
        world = self._hosted(account, world_id)
        self.store.remove_world(world['id'])
        return ok(removed=world['id'])

    def jwks(self, address: str, token, body: dict) -> tuple[int, dict]:
        return 200, self.keys.jwks(self.clock())

    # ---------------------------------------------------------------- Housekeeping

    def maintain(self) -> list[str]:
        now = self.clock()
        done = self.keys.maintain(now)
        pruned = self.store.prune(now)
        self.limiter.sweep()
        return done + [f'pruned {k}: {v}' for k, v in pruned.items() if v]
