#!/usr/bin/env python3
"""The Editor's helper: a host's way in to their own world's tools (Docs/Design/78-hosts-run-their-own-worlds.md,
sections 5, 6.3-6.5; Phase M2a, its security core). Standard library only (with the image's psycopg through
tools/world_db.py, and the image's `openssl` command to make and sign certificates).

  python3 tools/world_admin.py serve                the helper: TLS 1.3 on the Editor port (17789), until stopped
  ratw-editor list | add NAME --role R | reset NAME | invite NAME | disable NAME | enable NAME | remove NAME
  ratw-editor sign-out-all | fingerprint | new-certificate [--leaked]
  python3 tools/world_admin.py first-owner          (the entrypoint's) the first owner, or a renewed one-time password
  python3 tools/world_admin.py make-certificate     (the entrypoint's) the world's key and certificate, once

`ratw-editor` (Package/ratw-editor) runs this file. As root it reads only the dm password from the passwords file, then
runs again as ratw-admin; it is never limited by the Editor port, and prints Editor codes and one-time passwords only
to its terminal.

What the helper faces: the internet, once a host opens the port (`RATW_EDITOR_BIND=0.0.0.0`). So:
- TLS 1.3 only, with the world's own key (an ECDSA P-256 key made once, kept in the `admin` volume); nothing on the port
  is ever answered in plain HTTP. SIGHUP (from root, after `ratw-editor new-certificate`) reloads the key; a new key is
  announced in /editor/hello signed by the old one, so pinned clients move over without a warning.
- Authorization comes only from an Editor account's password and then a session (bearer token), never from the peer's
  address (I3). Addresses only slow guessing. Under Docker every connection from the world's own computer arrives as
  the gateway (M0a), and under Docker Desktop every peer may: such peers are "unidentified" and their per-address
  limits fold into the world-wide ones (RATW_EDITOR_PEERS=auto, the default; `real` or `folded` force either).
- Browsers can't talk to it (I9): any request with Origin, Cookie or Sec-Fetch-* is refused; no OPTIONS, no CORS; the
  Host header is ignored; every reply says nosniff.
- Two bounded pools of threads (sign-in work, signed-in work) and a third of two for scrypt; per-address (IPv6 per /64)
  and world-wide connection limits; handshakes within 10 s, idle connections closed after 60 s; bodies without a
  session at most 4 KB, Content-Length required, chunked refused; at most 2 bodies over 1 MB at once; every handler
  catches every exception.
- Sign-in: at most 2 password checks at once and 8 waiting, known devices and Editor codes first; unknown, disabled and
  admin names run the same scrypt against a dummy hash and get the same words; three lanes strangers can't fill (known
  devices, Editor codes, accounts on a one-time password); typed passwords from new devices counted per address, per
  account (in dm.locks, so a restart keeps it) and world-wide; fixed-size counter tables that fail closed;
  `[editor] new_devices = "approve"` asks new devices for an Editor code too.
- Sessions are dm.sessions rows (they survive a restart): 30 minutes without input (last_seen, renewed only by the
  keep-alive and the client's own screens, at most once a minute), 12 hours at most, 3 an account (its own oldest
  evicted), 10 a world (a new sign-in refused, nobody evicted).
- Logs (RATW_EDITOR lines) never hold a body, a query string, a token, a password or a traceback with request values
  (I15); failures above a threshold fold into one line and one audit row a minute.

The helper's files, all in its own directory (RATW_ADMIN_DIR, the `admin` volume: root and ratw-admin only):
  editor-key.pem, editor-cert.pem   the world's key and certificate; previous-key.pem after a signed rotation
  rotation.json                     the new key's SPKI signed by the old key (the hello carries it)
  world-id                          the world's id as the certificate names it
  settings.json                     the entrypoint's cleaned copy of world.toml (no token, no AI key)
  progress.json                     {code, percent} while the world sets up; "ready" when it is
  central.json, ever-official       central's kind for this world; "ever official" is never cleared
  requests/                         kind-only requests to the root entrypoint (first-editor-used, restart, ...)
  helper.pid                        so `ratw-editor new-certificate` can ask the helper to reload

The breached-password list (Package/breached-passwords.txt) is the UK NCSC's list of the 100,000 passwords most often
found in breaches (from Have I Been Pwned; as published in SecLists, MIT), lowercased, sorted, without its hex-escaped
and control-character entries.
"""
from __future__ import annotations

import argparse
import asyncio
import base64
import concurrent.futures
import contextlib
import gzip
import hashlib
import heapq
import hmac
import ipaddress
import itertools
import json
import os
from pathlib import Path
import re
import secrets
import signal
import ssl
import subprocess
import sys
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import world_db  # noqa: E402

ADMIN_DIR = Path(os.environ.get('RATW_ADMIN_DIR', '/var/lib/ratw/admin'))
SECRETS_FILE = Path(os.environ.get('RATW_SECRETS_FILE', '/run/ratw-secrets/passwords.env'))
BREACHED_FILE = Path(os.environ.get('RATW_BREACHED_PASSWORDS', ROOT / 'Package' / 'breached-passwords.txt'))
WORLD_KEY = os.environ.get('RATW_EDITOR_WORLD', 'world')      # The container's one world (Package/entrypoint.py: KEY).
ADMIN_USER = 'ratw-admin'                                     # (Package/Dockerfile: uid 10003, in no other group.)
PORT = 17789
EDITOR_API = 1
CODE_PREFIX = 'ratw-editor:'
USERNAME = re.compile(r'^[a-z][a-z0-9_-]{2,31}$')            # (The DM tool's rule, 0013_dungeon_master.sql.)
WORLD_ID = re.compile(r'^(world|local)-[0-9a-f]{32}$')
GRANTABLE = ('viewer', 'dm', 'builder')                      # What the Editor's own screen may give; owners: ratw-editor.
ROLES = ('viewer', 'dm', 'builder', 'owner')                 # What ratw-editor may give. Never admin.
PROGRESS_CODES = ('starting', 'database', 'importing', 'migrating', 'building', 'ready', 'stopped')
REQUEST_KINDS = ('restart', 'backup-before-publish', 'backup-by-hand', 'share', 'first-editor-used')

# Passwords and secrets.
MIN_PASSWORD, MAX_PASSWORD = 12, 256
MAX_TYPED = 1024                    # A password field longer than this isn't checked at all.
FIRST_OWNER_DAYS, CODE_HOURS = 7, 72
# Sessions.
SESSION_HOURS, IDLE_MINUTES, STEP_UP_MINUTES = 12, 30, 5
SESSIONS_PER_ACCOUNT, SESSIONS_PER_WORLD = 3, 10
DEVICES_PER_ACCOUNT = 5
STEP_UP_FAILURES = 5               # Wrong passwords typed in an open session (step-up, password change) end it.
# Sign-in limits.
FAILURES, WINDOW = 5, 15 * 60       # Per address, per account, per device, per invite: 5 failures (in 15 minutes).
ADDRESS_WAIT, ADDRESS_WAIT_MOST = 15 * 60, 60 * 60
ACCOUNT_WAIT, ACCOUNT_WAIT_MOST = 15 * 60, 4 * 60 * 60
DEVICE_WAIT = 15 * 60
WORLD_FAILURES = 20                 # A minute, from unknown devices; good sign-ins don't count.
SCRYPT_SLOTS, SCRYPT_WAITING = 2, 8
FOLD_ABOVE = 10                     # Failures a minute above which they fold into one log line and audit row.
TABLE_SIZE = 4096                   # Entries in each counter table (addresses, accounts, audit hours, sessions).
# Connections.
PER_ADDRESS_OPEN, WORLD_OPEN = 4, 32
HANDSHAKES = (6, 3.0)               # Per address: a burst of 6, then 3 a second.
CONNECTIONS = (40, 10.0)            # World-wide: a burst of 40, then 10 a second.
HANDSHAKE_SECONDS, FIRST_REQUEST_SECONDS, IDLE_SECONDS, BODY_SECONDS = 10, 10, 60, 120
HEAD_LIMIT, MAX_HEADERS = 8 * 1024, 64
PUBLIC_BODY, EDITOR_BODY = 4 * 1024, 4 * 1024
SIGNED_IN_BODY, LARGE_BODY, LARGE_AT_ONCE = 16 * 1024 * 1024, 1024 * 1024, 2
REQUESTS_PER_SECOND = (40, 20.0)    # A signed-in session: a burst of 40, then 20 a second.
DB_CONNECTIONS = 6
HELLO_MOST = 1024
GZIP_ABOVE = 16 * 1024              # Replies to the hosted studio this large go gzipped to a client that takes it.
PUBLIC_PATHS = ('/editor/hello', '/editor/login')
ONE_TIME_PATHS = ('/editor/password', '/editor/keepalive', '/editor/logout')
STATUS_WORDS = {200: 'OK', 400: 'Bad Request', 401: 'Unauthorized', 403: 'Forbidden', 404: 'Not Found',
                405: 'Method Not Allowed', 411: 'Length Required', 413: 'Content Too Large', 422: 'Unprocessable Content',
                429: 'Too Many Requests', 431: 'Request Header Fields Too Large', 500: 'Internal Server Error',
                503: 'Service Unavailable'}


class EditorError(Exception):
    """A refusal with a code the client words itself (doc 78, section 2) and a plain sentence for ratw-editor."""

    def __init__(self, status: int, code: str, message: str = '', **extra):
        super().__init__(message or code)
        self.status, self.code, self.message, self.extra = status, code, message or code, extra

    def payload(self) -> dict:
        return {'error': self.code, 'message': self.message, **self.extra}


def log(event: str, **fields) -> None:
    """One RATW_EDITOR line. Fields are only ever account names (checked), address keys, routes, statuses, lanes and
    counts: never a body, a query string, a token, a password or an exception's text (I15)."""
    parts = [f'{k}={_log_value(v)}' for k, v in fields.items() if v is not None]
    LOG(' '.join(['RATW_EDITOR', event, *parts]))


def _log_value(value) -> str:
    text = str(value)
    return text if re.fullmatch(r'[A-Za-z0-9_.:/#~*-]{1,80}', text) else '?'


def _print(line: str) -> None:
    print(line, flush=True)


LOG = _print


def token_hash(token: str) -> str:
    return hashlib.sha256(token.encode('utf-8')).hexdigest()


def b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b'=').decode('ascii')


def unb64url(text: str) -> bytes:
    return base64.urlsafe_b64decode(text + '=' * (-len(text) % 4))


# ------------------------------------------------------------------------------------------------------- passwords

_breached: frozenset[str] | None = None


def breached() -> frozenset[str]:
    global _breached
    if _breached is None:
        try:
            _breached = frozenset(line for line in BREACHED_FILE.read_text(encoding='utf-8').splitlines() if line)
        except OSError:
            log('breached-list.missing')
            _breached = frozenset()
    return _breached


def _squash(text: str) -> str:
    return re.sub(r'[\s_.-]+', '', text.lower())


def password_problem(password: str, username: str = '', world_name: str = '') -> str | None:
    """Why a new password is refused, as a code the client words (screen 4b), or None. At least 12 characters (at most
    256); not containing the username or the world's name; not one of the commonest breached passwords, nor one of them
    with digits or symbols before or after, nor one of them repeated; at least 5 different characters."""
    if not isinstance(password, str) or len(password) < MIN_PASSWORD:
        return 'short'
    if len(password) > MAX_PASSWORD:
        return 'long'
    lower, squashed = password.lower(), _squash(password)
    if username and (username.lower() in lower or _squash(username) in squashed):
        return 'username'
    world = _squash(world_name or '')
    if len(world) >= 3 and world in squashed:
        return 'world'
    if len(set(lower)) < 5:
        return 'simple'
    common = breached()
    core = lower.strip('0123456789!@#$%^&*()_+-=[]{};:\'",.<>/?\\|`~ ')
    if lower in common or (core and core in common):
        return 'common'
    for parts in (2, 3, 4):
        size = len(lower) // parts
        if size and len(lower) == size * parts and lower[:size] * parts == lower and lower[:size] in common:
            return 'common'
    return None


DUMMY_RECORD = world_db.hash_password(secrets.token_urlsafe(24))   # Made at start: unknown names check against it.


def _sane_record(record) -> bool:
    """A stored hash the helper will spend scrypt on: ours, with parameters no costlier than the tools' own."""
    try:
        return (isinstance(record, dict) and record.get('algorithm') == 'scrypt' and int(record['n']) <= 2 ** 16
                and int(record['r']) <= 16 and int(record['p']) <= 2)
    except (KeyError, TypeError, ValueError):
        return False


def check_password(record, password: str) -> bool:
    """scrypt, timing-safe (world_db.password_matches compares with hmac.compare_digest). A missing or odd record runs
    the same scrypt on the dummy hash and fails, so unknown and disabled names take the same time as real ones."""
    if isinstance(password, str) and len(password) <= MAX_TYPED and _sane_record(record):
        return world_db.password_matches(record, password)
    world_db.password_matches(DUMMY_RECORD, password if isinstance(password, str) else '')
    return False


# ---------------------------------------------------------------------------------------------------- certificates

def _der(data: bytes, at: int) -> tuple[int, int, int]:
    """(tag, where its value starts, where it ends) of the DER item at `at`."""
    tag, length, at = data[at], data[at + 1], at + 2
    if length & 0x80:
        count = length & 0x7f
        if not 1 <= count <= 4:
            raise ValueError('bad DER length')
        length, at = int.from_bytes(data[at:at + count], 'big'), at + count
    if at + length > len(data):
        raise ValueError('DER item runs past its end')
    return tag, at, at + length


def spki_of(cert_der: bytes) -> bytes:
    """A certificate's SubjectPublicKeyInfo, as DER: what the fingerprint and the client's pin hash."""
    tag, start, _ = _der(cert_der, 0)
    if tag != 0x30:
        raise ValueError('not a certificate')
    tag, at, end = _der(cert_der, start)            # tbsCertificate
    if tag != 0x30:
        raise ValueError('not a certificate')
    fields = []
    while at < end and len(fields) < 7:
        item_tag, value, value_end = _der(cert_der, at)
        fields.append((item_tag, at, value_end))
        at = value_end
    if fields and fields[0][0] == 0xa0:              # [0] version
        fields = fields[1:]
    if len(fields) < 6 or fields[5][0] != 0x30:
        raise ValueError('no public key in the certificate')
    return cert_der[fields[5][1]:fields[5][2]]


def cert_spki(path: Path) -> bytes:
    return spki_of(ssl.PEM_cert_to_DER_cert(path.read_text(encoding='ascii')))


def fingerprint(spki: bytes) -> str:
    """The SHA-256 of the key's SPKI, its first 128 bits as 8 groups of 4 hex characters: always all eight, in the same
    layout everywhere (the client, the files, the log; doc 78, section 3.4)."""
    digest = hashlib.sha256(spki).hexdigest().upper()[:32]
    return ' '.join(digest[i:i + 4] for i in range(0, 32, 4))


def pin(spki: bytes) -> str:
    """The full SHA-256 of the SPKI, as an Editor code carries it (base64url)."""
    return b64url(hashlib.sha256(spki).digest())


def _openssl(*args: str) -> None:
    subprocess.run(['openssl', *args], check=True, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                   stderr=subprocess.PIPE, timeout=60)


def _new_key_pair(folder: Path, world_id: str) -> tuple[Path, Path]:
    key, cert = folder / f'.new-key-{secrets.token_hex(4)}.pem', folder / f'.new-cert-{secrets.token_hex(4)}.pem'
    old_mask = os.umask(0o077)
    try:
        _openssl('req', '-x509', '-newkey', 'ec', '-pkeyopt', 'ec_paramgen_curve:prime256v1', '-nodes', '-days',
                 '36500', '-subj', f'/CN=RATW Editor {world_id}', '-keyout', str(key), '-out', str(cert))
    finally:
        os.umask(old_mask)
    os.chmod(key, 0o600)
    os.chmod(cert, 0o644)
    return key, cert


def world_id(folder: Path = None) -> str:
    folder = folder or ADMIN_DIR
    try:
        found = (folder / 'world-id').read_text(encoding='ascii').strip()
        if WORLD_ID.match(found):
            return found
    except OSError:
        pass
    return ''


def make_certificate(folder: Path = None, central_id: str = '') -> bool:
    """The world's key and certificate, made once (the entrypoint's first start; doc 78, 6.4): False if there is one
    already. The id is the world's central id when it has one, otherwise a local one; it never changes afterwards."""
    folder = folder or ADMIN_DIR
    if (folder / 'editor-key.pem').exists() and (folder / 'editor-cert.pem').exists():
        return False
    folder.mkdir(parents=True, exist_ok=True)
    identity = world_id(folder) or (central_id if WORLD_ID.match(central_id or '') and central_id.startswith('world-')
                                    else 'local-' + secrets.token_hex(16))
    _write_atomic(folder / 'world-id', identity + '\n', 0o644)
    key, cert = _new_key_pair(folder, identity)
    os.replace(cert, folder / 'editor-cert.pem')
    os.replace(key, folder / 'editor-key.pem')
    return True


ROTATION_TEXT = 'RATW Editor key rotation v1\n{world}\n{pin}\n'


def rotation_message(world: str, new_spki: bytes) -> bytes:
    """What the old key signs for a rotation: the world's id and the new key's pin. The client checks the signature
    with the old key it stored (ECDSA P-256, SHA-256, DER signature)."""
    return ROTATION_TEXT.format(world=world, pin=pin(new_spki)).encode('ascii')


def new_certificate(folder: Path = None, leaked: bool = False) -> dict:
    """`ratw-editor new-certificate`: a new key. Without --leaked the old key signs the new one (rotation.json, which
    the hello carries), so pinned clients move over quietly; with it there is no signature (every client warns), and
    the caller ends every session."""
    folder = folder or ADMIN_DIR
    current_key, current_cert = folder / 'editor-key.pem', folder / 'editor-cert.pem'
    if not (current_key.exists() and current_cert.exists()):
        raise EditorError(409, 'no-certificate', 'This world has no Editor certificate yet: start the world first.')
    identity = world_id(folder)
    old_spki = cert_spki(current_cert)
    key, cert = _new_key_pair(folder, identity)
    try:
        new_spki = cert_spki(cert)
        rotation = None
        if not leaked:
            with tempfile.TemporaryDirectory(dir=folder) as scratch:
                message, signature = Path(scratch) / 'message', Path(scratch) / 'signature'
                message.write_bytes(rotation_message(identity, new_spki))
                _openssl('dgst', '-sha256', '-sign', str(current_key), '-out', str(signature), str(message))
                rotation = {'spki': base64.b64encode(new_spki).decode(), 'previous': pin(old_spki),
                            'signature': base64.b64encode(signature.read_bytes()).decode(),
                            'algorithm': 'ecdsa-p256-sha256'}
        if rotation:
            os.replace(current_key, folder / 'previous-key.pem')
            os.replace(current_cert, folder / 'previous-cert.pem')
            _write_atomic(folder / 'rotation.json', json.dumps(rotation, separators=(',', ':')), 0o644)
        else:
            for name in ('rotation.json', 'previous-key.pem', 'previous-cert.pem'):
                with contextlib.suppress(FileNotFoundError):
                    (folder / name).unlink()
        os.replace(cert, folder / 'editor-cert.pem')
        os.replace(key, folder / 'editor-key.pem')
    finally:
        for leftover in (key, cert):
            with contextlib.suppress(FileNotFoundError):
                leftover.unlink()
    return {'fingerprint': fingerprint(new_spki), 'previous': fingerprint(old_spki), 'signed': bool(rotation)}


def signal_helper(folder: Path = None) -> bool:
    """Asks the running helper to reload its key (SIGHUP to helper.pid). False if none runs."""
    folder = folder or ADMIN_DIR
    try:
        pid = int((folder / 'helper.pid').read_text().strip())
        os.kill(pid, signal.SIGHUP)
        return True
    except (OSError, ValueError):
        return False


def server_context(folder: Path = None) -> ssl.SSLContext:
    """TLS 1.3 only, with the world's key (I2)."""
    folder = folder or ADMIN_DIR
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_3
    context.load_cert_chain(folder / 'editor-cert.pem', folder / 'editor-key.pem')
    return context


def certificate_text(folder: Path = None, port: int | None = None) -> str:
    """manage/editor-certificate.txt (the entrypoint writes it; `ratw-editor fingerprint` prints it)."""
    folder = folder or ADMIN_DIR
    return (f'Your world: {world_id(folder)}\n'
            f'Editor port: {port or os.environ.get("RATW_EDITOR_PORT", PORT)}\n'
            f'Editor certificate fingerprint: {fingerprint(cert_spki(folder / "editor-cert.pem"))}\n'
            'The game shows this fingerprint the first time you connect to this world from a new computer without an '
            'Editor code. If it is the same, trust it.\n')


def _write_atomic(path: Path, text: str, mode: int = 0o600) -> None:
    temporary = path.with_name(f'.{path.name}.{secrets.token_hex(4)}')
    fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, mode)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as out:
            out.write(text)
        os.chmod(temporary, mode)
        os.replace(temporary, path)
    except BaseException:
        with contextlib.suppress(FileNotFoundError):
            temporary.unlink()
        raise


# ----------------------------------------------------------------------------------------------------- Editor codes

def make_code(world: str, spki: bytes, username: str, password: str | None = None, invite: str | None = None,
              address: str | None = None) -> str:
    """An Editor code: `ratw-editor:` and base64url of a small record (doc 78, 6.3): the world id, the key's full pin,
    the username, the one-time password and/or the invite, and the address when the maker knows it. A secret: it
    works once and expires with what it carries."""
    record = {'v': 1, 'w': world, 'k': pin(spki), 'u': username}
    if password:
        record['p'] = password
    if invite:
        record['i'] = invite
    if address:
        record['a'] = address
    return CODE_PREFIX + b64url(json.dumps(record, separators=(',', ':'), sort_keys=True).encode('utf-8'))


def read_code(code: str) -> dict:
    """An Editor code's record, checked (what the client does with a pasted one)."""
    if not isinstance(code, str) or not code.startswith(CODE_PREFIX) or len(code) > 1024:
        raise ValueError('not an Editor code')
    record = json.loads(unb64url(code[len(CODE_PREFIX):]))
    if (not isinstance(record, dict) or record.get('v') != 1 or not WORLD_ID.match(str(record.get('w', '')))
            or not USERNAME.match(str(record.get('u', ''))) or len(unb64url(str(record.get('k', '')))) != 32
            or not (record.get('p') or record.get('i'))):
        raise ValueError('not an Editor code')
    return record


# ----------------------------------------------------------------------------------------------------- the settings

def _read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding='utf-8'))
        return value if isinstance(value, dict) else {}
    except (OSError, ValueError):
        return {}


class Settings:
    """What the helper knows of its world: the entrypoint's cleaned settings copy, the setup progress, central's kind.
    Read afresh when asked (the files are small), so the entrypoint can change them while the helper runs."""

    def __init__(self, folder: Path = None):
        self.folder = folder or ADMIN_DIR

    def values(self) -> dict:
        return _read_json(self.folder / 'settings.json')

    def editor(self) -> dict:
        editor = self.values().get('editor')
        return editor if isinstance(editor, dict) else {}

    def enabled(self) -> bool:
        return self.editor().get('enabled', True) is not False

    def new_devices(self) -> str:
        return 'approve' if self.editor().get('new_devices') == 'approve' else 'open'

    def owner_name(self) -> str | None:
        name = self.editor().get('owner')
        return name if isinstance(name, str) and USERNAME.match(name) else None

    def name(self) -> str:
        name = self.values().get('name')
        if isinstance(name, str) and name.strip():
            return name.strip()
        with contextlib.suppress(Exception):
            return world_db.world(WORLD_KEY)['name']
        return ''

    def package(self) -> str:
        package = self.values().get('package') or os.environ.get('RATW_PACKAGE_VERSION', '')
        return str(package)[:40]

    def progress(self) -> dict | None:
        """{code, percent} from the entrypoint, or None when it wrote none (the helper then asks the database)."""
        raw = _read_json(self.folder / 'progress.json')
        if not raw:
            return None
        code = raw.get('code') if raw.get('code') in PROGRESS_CODES else 'starting'
        percent = raw.get('percent')
        percent = max(0, min(100, int(percent))) if isinstance(percent, (int, float)) and not isinstance(percent, bool) else 0
        return {'code': code, 'percent': 100 if code == 'ready' else percent}

    def official(self) -> bool:
        """Central has ever named this world official (doc 78, 6.7): every sign-in is refused, for good."""
        return (self.folder / 'ever-official').exists() or _read_json(self.folder / 'central.json').get('kind') == 'official'


def request_root(kind: str, folder: Path = None) -> Path:
    """A request to the root entrypoint: a file in requests/ naming only a kind (doc 78, 6.5; I7)."""
    if kind not in REQUEST_KINDS:
        raise ValueError(f'unknown request {kind!r}')
    directory = (folder or ADMIN_DIR) / 'requests'
    directory.mkdir(mode=0o700, exist_ok=True)
    path = directory / f'{kind}.{secrets.token_hex(6)}'
    _write_atomic(path, kind + '\n', 0o600)
    return path


# ---------------------------------------------------------------------------------------------- counters and limits

class Bucket:
    """A token bucket: `capacity` tokens, `rate` more a second."""

    def __init__(self, capacity: float, rate: float, clock, start: float | None = None):
        self.capacity, self.rate, self.clock = capacity, rate, clock
        self.tokens, self.at = capacity if start is None else start, clock()

    def _fill(self) -> None:
        now = self.clock()
        self.tokens, self.at = min(self.capacity, self.tokens + (now - self.at) * self.rate), now

    def available(self) -> bool:
        self._fill()
        return self.tokens >= 1

    def take(self) -> bool:
        self._fill()
        if self.tokens < 1:
            return False
        self.tokens -= 1
        return True

    def spend(self) -> None:
        """Takes a token even if it goes below none (a failure has happened whatever the bucket says)."""
        self._fill()
        self.tokens = max(-self.capacity, self.tokens - 1)


class Table:
    """A dict of at most `size` entries. When it is full, stale entries go first; if none is stale, a new key gets
    None, and the caller fails closed (the world-wide limits apply to it)."""

    def __init__(self, size: int, make, stale):
        self.size, self.make, self.stale = size, make, stale
        self.items: dict = {}

    def get(self, key):
        return self.items.get(key)

    def entry(self, key):
        found = self.items.get(key)
        if found is not None:
            return found
        if len(self.items) >= self.size:
            self.prune()
            if len(self.items) >= self.size:
                return None
        found = self.items[key] = self.make()
        return found

    def prune(self) -> None:
        for key in [k for k, v in self.items.items() if self.stale(v)]:
            del self.items[key]

    def pop(self, key) -> None:
        self.items.pop(key, None)


def next_lock(state: dict, now: float, base: float, most: float) -> dict:
    """One more failure: 5 within 15 minutes make a wait of `base`, doubling (up to `most`) for each wait after it,
    forgotten after a day without one. state: {failures, first, level, until} (epoch seconds)."""
    if state.get('until') and now - state['until'] > 24 * 3600:
        state['level'] = 0
    if now - state.get('first', now) > WINDOW:
        state['failures'] = 0
    if not state.get('failures'):
        state['first'] = now
    state['failures'] = state.get('failures', 0) + 1
    if state['failures'] >= FAILURES:
        state['until'] = now + min(base * 2 ** state.get('level', 0), most)
        state['level'] = min(state.get('level', 0) + 1, 12)
        state['failures'] = 0
    return state


def _wait_minutes(until: float, now: float) -> int:
    return max(1, int((until - now + 59) // 60))


def default_gateway() -> str | None:
    """The container's default gateway (/proc/net/route): under Docker, connections from the world's own computer (and
    every IPv6 one, and every one under Docker Desktop if M0a's guess holds) arrive from it."""
    try:
        for line in Path('/proc/net/route').read_text().splitlines()[1:]:
            fields = line.split()
            if len(fields) > 2 and fields[1] == '00000000':
                return str(ipaddress.IPv4Address(int(fields[2], 16).to_bytes(4, 'little')))
    except (OSError, ValueError):
        pass
    return None


class Peers:
    """Which peers the per-address limits can tell apart (doc 78, D1). `auto`: all but the gateway and loopback (and
    RATW_EDITOR_UNIDENTIFIED's); `real`: all; `folded`: none. An unidentified peer's limits fold into the world-wide
    ones. Never a grant (I3): only how guessing is slowed."""

    def __init__(self, mode: str | None = None, unidentified=None):
        self.mode = mode or os.environ.get('RATW_EDITOR_PEERS', 'auto')
        if self.mode not in ('auto', 'real', 'folded'):
            self.mode = 'auto'
        listed = unidentified if unidentified is not None else [
            a for a in os.environ.get('RATW_EDITOR_UNIDENTIFIED', '').split(',') if a]
        gateway = default_gateway()
        self.unidentified = {*listed, *([gateway] if gateway else [])}

    def key(self, address: str | None) -> str | None:
        """The address's key for the per-address limits (IPv6 by /64), or None if it can't be told apart."""
        if self.mode == 'folded' or not address:
            return None
        try:
            ip = ipaddress.ip_address(address.split('%')[0])
        except ValueError:
            return None
        if isinstance(ip, ipaddress.IPv6Address) and ip.ipv4_mapped:
            ip = ip.ipv4_mapped
        if self.mode == 'auto' and (ip.is_loopback or str(ip) in self.unidentified or ip.is_unspecified):
            return None
        if isinstance(ip, ipaddress.IPv6Address):
            return str(ipaddress.IPv6Network((ip, 64), strict=False))
        return str(ip)


class ScryptGate:
    """At most `slots` password checks at once and `waiting` queued; known devices and Editor codes (priority 0) go
    first, and one arriving at a full queue takes the place of the newest stranger, who gets "busy"."""

    def __init__(self, executor, slots: int = SCRYPT_SLOTS, waiting: int = SCRYPT_WAITING):
        self.executor, self.slots, self.most = executor, slots, waiting
        self.active, self.queue, self.order = 0, [], itertools.count()

    def _queued(self):
        return [entry for entry in self.queue if not entry[2].done()]

    async def run(self, priority: int, work):
        loop = asyncio.get_running_loop()
        if self.active < self.slots and not self._queued():
            self.active += 1
        else:
            waiting = self._queued()
            if len(waiting) >= self.most:
                strangers = [entry for entry in waiting if entry[0] > priority]
                if not strangers:
                    raise EditorError(503, 'busy', 'The world is busy: try again in a minute.')
                newest = max(strangers, key=lambda entry: (entry[0], entry[1]))
                newest[2].set_exception(EditorError(503, 'busy', 'The world is busy: try again in a minute.'))
            future = loop.create_future()
            heapq.heappush(self.queue, (priority, next(self.order), future))
            try:
                await future                         # The slot is handed over by the one that leaves it.
            except asyncio.CancelledError:
                if future.done() and not future.cancelled() and future.exception() is None:
                    self._leave()
                raise
        try:
            return await loop.run_in_executor(self.executor, work)
        finally:
            self._leave()

    def _leave(self) -> None:
        while self.queue:
            _, _, future = heapq.heappop(self.queue)
            if not future.done():
                future.set_result(None)
                return
        self.active -= 1


# --------------------------------------------------------------------------------------------------------- accounts

def _secret() -> str:
    return secrets.token_urlsafe(15)          # 120 random bits (doc 78, section 5).


def owners_left(conn, without: str) -> int:
    return conn.execute("SELECT count(*) FROM dm.admins WHERE role = 'owner' AND NOT disabled AND username <> %s",
                        (without,)).fetchone()[0]


def audit(conn, who: str, action: str, target: str, detail: str) -> int:
    return conn.execute('INSERT INTO dm.audit (at, action, target, detail, who) VALUES (%s, %s, %s, %s, %s) RETURNING sequence',
                        (time.time(), action, target[:200], detail[:2000], who[:80])).fetchone()[0]


def _lock_accounts(conn) -> None:
    conn.execute("SELECT pg_advisory_xact_lock(hashtext('ratw-editor-accounts'))")


def _new_invite(conn, username: str, by: str, hours: float) -> str:
    invite = secrets.token_urlsafe(18)
    conn.execute("INSERT INTO dm.invites (invite_hash, username, expires_at, created_by) VALUES (%s, %s, now() + %s * interval '1 hour', %s)",
                 (token_hash(invite), username, hours, by[:80]))
    return invite


def add_account(conn, username: str, role: str, by: str, hours: float = CODE_HOURS, may_act_on_wolves: bool = False,
                roles=GRANTABLE) -> tuple[str, str]:
    """A new Editor account on a one-time password, with an invite: (password, invite) for its Editor code."""
    if not isinstance(username, str) or not USERNAME.match(username):
        raise EditorError(422, 'username', 'Usernames are 3-32 lowercase letters, digits, _ or -, starting with a letter.')
    if role not in roles:
        raise EditorError(422, 'role', f'The role must be one of: {", ".join(roles)}.')
    password = _secret()
    with conn.transaction():
        _lock_accounts(conn)
        if conn.execute('SELECT 1 FROM dm.admins WHERE username = %s', (username,)).fetchone():
            raise EditorError(409, 'exists', f'There is already an account called {username}.')
        conn.execute("""INSERT INTO dm.admins (username, role, password, must_change, must_change_until, may_act_on_wolves, created_by)
                        VALUES (%s, %s, %s, true, now() + %s * interval '1 hour', %s, %s)""",
                     (username, role, world_db.Jsonb(world_db.hash_password(password)), hours, bool(may_act_on_wolves), by[:80]))
        invite = _new_invite(conn, username, by, hours)
        audit(conn, by, 'account.add', username, f'Added {username} as {role}.')
    return password, invite


def reset_account(conn, username: str, by: str, hours: float = CODE_HOURS) -> tuple[str, str]:
    """A new one-time password and invite; unlocks; ends its sessions and forgets its devices."""
    password = _secret()
    with conn.transaction():
        _lock_accounts(conn)
        changed = conn.execute("""UPDATE dm.admins SET password = %s, must_change = true,
                                  must_change_until = now() + %s * interval '1 hour' WHERE username = %s AND role <> 'admin'""",
                               (world_db.Jsonb(world_db.hash_password(password)), hours, username)).rowcount
        if not changed:
            raise EditorError(404, 'no-account', f'No Editor account {username}.')
        _forget(conn, username)
        conn.execute('DELETE FROM dm.invites WHERE username = %s', (username,))
        invite = _new_invite(conn, username, by, hours)
        audit(conn, by, 'account.reset', username, f'Reset {username}: a new Editor code.')
    return password, invite


def invite_account(conn, username: str, by: str, hours: float = CODE_HOURS) -> str:
    """An Editor code's pass for a new device; the password is unchanged."""
    with conn.transaction():
        if not conn.execute("SELECT 1 FROM dm.admins WHERE username = %s AND role <> 'admin'", (username,)).fetchone():
            raise EditorError(404, 'no-account', f'No Editor account {username}.')
        invite = _new_invite(conn, username, by, hours)
        audit(conn, by, 'account.invite', username, f'An Editor code for a new device for {username}.')
    return invite


def _forget(conn, username: str) -> None:
    conn.execute('DELETE FROM dm.sessions WHERE username = %s', (username,))
    conn.execute('DELETE FROM dm.devices WHERE username = %s', (username,))
    conn.execute('DELETE FROM dm.locks WHERE username = %s', (username,))


def _target(conn, username: str) -> dict:
    row = conn.execute("SELECT role, disabled FROM dm.admins WHERE username = %s AND role <> 'admin' FOR UPDATE",
                       (username,)).fetchone()
    if not row:
        raise EditorError(404, 'no-account', f'No Editor account {username}.')
    return {'role': row[0], 'disabled': row[1]}


def set_disabled(conn, username: str, disabled: bool, by: str) -> None:
    with conn.transaction():
        _lock_accounts(conn)
        target = _target(conn, username)
        if disabled and target['role'] == 'owner' and not owners_left(conn, username):
            raise EditorError(409, 'last-owner', "The world's last owner can't be turned off.")
        conn.execute('UPDATE dm.admins SET disabled = %s WHERE username = %s', (disabled, username))
        if disabled:
            conn.execute('DELETE FROM dm.sessions WHERE username = %s', (username,))
        audit(conn, by, 'account.disable' if disabled else 'account.enable', username,
              f'Turned {username} {"off" if disabled else "on"}.')


def remove_account(conn, username: str, by: str) -> None:
    with conn.transaction():
        _lock_accounts(conn)
        target = _target(conn, username)
        if target['role'] == 'owner' and not owners_left(conn, username):
            raise EditorError(409, 'last-owner', "The world's last owner can't be removed.")
        conn.execute('DELETE FROM dm.admins WHERE username = %s', (username,))
        audit(conn, by, 'account.remove', username, f'Removed {username}.')


def set_role(conn, username: str, role: str, by: str, roles=GRANTABLE) -> None:
    if role not in roles:
        raise EditorError(422, 'role', f'The role must be one of: {", ".join(roles)}.')
    with conn.transaction():
        _lock_accounts(conn)
        target = _target(conn, username)
        if target['role'] == 'owner' and role != 'owner' and not owners_left(conn, username):
            raise EditorError(409, 'last-owner', "The world's last owner can't be given another role.")
        conn.execute('UPDATE dm.admins SET role = %s WHERE username = %s', (role, username))
        conn.execute('DELETE FROM dm.sessions WHERE username = %s', (username,))   # Signs in again under the new role.
        audit(conn, by, 'account.role', username, f'{username} is now {role} (was {target["role"]}).')


def set_wolves(conn, username: str, allowed: bool, by: str) -> None:
    with conn.transaction():
        _target(conn, username)
        conn.execute('UPDATE dm.admins SET may_act_on_wolves = %s WHERE username = %s', (bool(allowed), username))
        audit(conn, by, 'account.wolves', username,
              f'{username} {"may" if allowed else "may no longer"} act on players\' wolves.')


def end_all(conn, by: str) -> None:
    """End every Editor session and forget every account's devices."""
    with conn.transaction():
        conn.execute('DELETE FROM dm.sessions')
        conn.execute('DELETE FROM dm.devices')
        audit(conn, by, 'account.sign-out-all', '', 'Ended every Editor session and forgot every device.')


def list_accounts(conn) -> list[dict]:
    rows = conn.execute("""SELECT a.username, a.role, a.disabled, a.must_change, a.must_change_until < now(), a.created_at,
                                  a.last_login, a.claimed_from, a.may_act_on_wolves, a.created_by,
                                  (SELECT address FROM dm.sessions s WHERE s.username = a.username ORDER BY created_at DESC LIMIT 1)
                           FROM dm.admins a WHERE a.role <> 'admin' ORDER BY a.username""").fetchall()
    devices = {}
    for username, created, used in conn.execute('SELECT username, created_at, last_used FROM dm.devices ORDER BY created_at'):
        devices.setdefault(username, []).append({'created': created.timestamp(), 'lastUsed': used.timestamp()})
    return [{'username': r[0], 'role': r[1], 'disabled': r[2], 'mustChange': r[3], 'expired': bool(r[3] and r[4]),
             'unclaimed': bool(r[3] and r[7] is None), 'created': r[5].timestamp(),
             'lastLogin': r[6].timestamp() if r[6] else None, 'claimedFrom': r[7], 'lastFrom': r[10],
             'mayActOnWolves': r[8] or r[1] == 'owner', 'createdBy': r[9], 'devices': devices.get(r[0], [])} for r in rows]


def ensure_first_owner(conn, name: str | None = None) -> dict | None:
    """The world's first owner (doc 78, Q3), made only when it has no Editor accounts at all: [editor] owner's name, or
    `host-` and four random letters and digits, on a one-time password and an invite valid 7 days. A start after that,
    with the owner still unclaimed, renews them. None when there is nothing new to write."""
    with conn.transaction():
        _lock_accounts(conn)
        if not conn.execute("SELECT 1 FROM dm.admins WHERE role <> 'admin'").fetchone():
            username = name if name and USERNAME.match(name) else 'host-' + ''.join(
                secrets.choice('abcdefghijklmnopqrstuvwxyz0123456789') for _ in range(4))
            password, invite = add_account(conn, username, 'owner', 'ratw-editor', FIRST_OWNER_DAYS * 24, roles=ROLES)
            return {'username': username, 'password': password, 'invite': invite, 'renewed': False}
        stale = conn.execute("""SELECT username FROM dm.admins WHERE role = 'owner' AND must_change AND claimed_from IS NULL
                                AND must_change_until < now() AND NOT disabled ORDER BY created_at LIMIT 1""").fetchone()
        if not stale:
            return None
        password, invite = reset_account(conn, stale[0], 'ratw-editor', FIRST_OWNER_DAYS * 24)
        return {'username': stale[0], 'password': password, 'invite': invite, 'renewed': True}


# ------------------------------------------------------------------------------------------------------------- core

class Core:
    """The sign-in rules and the Editor's own routes, apart from the network (tests drive it with any address)."""

    def __init__(self, connect=None, settings: Settings | None = None, peers: Peers | None = None, clock=time.time,
                 folder: Path | None = None, table_size: int = TABLE_SIZE, sign_in_threads: int = 4,
                 signed_in_threads: int = 8):
        self.folder = folder or ADMIN_DIR
        self.settings = settings or Settings(self.folder)
        self.peers = peers or Peers()
        self.clock = clock
        self.pool = world_db.Pool(WORLD_KEY, 'dm', size=DB_CONNECTIONS, connect_fn=connect)
        self.db_slots = threading.BoundedSemaphore(DB_CONNECTIONS)
        self.sign_in_pool = concurrent.futures.ThreadPoolExecutor(sign_in_threads, thread_name_prefix='editor-sign-in')
        self.signed_in_pool = concurrent.futures.ThreadPoolExecutor(signed_in_threads, thread_name_prefix='editor-work')
        self.scrypt_pool = concurrent.futures.ThreadPoolExecutor(SCRYPT_SLOTS, thread_name_prefix='editor-scrypt')
        self.gate = ScryptGate(self.scrypt_pool)
        stale = lambda e: not e.get('failures') and (not e.get('until') or clock() - e['until'] > 24 * 3600)
        self.addresses = Table(table_size, dict, stale)
        self.names = Table(table_size, dict, stale)      # Unknown names' lock state (known ones: dm.locks).
        self.world = Bucket(WORLD_FAILURES, WORLD_FAILURES / 60, clock, start=WORLD_FAILURES / 2)
        self.audit_hours = Table(table_size, dict, lambda e: clock() - e.get('at', 0) > 3600)
        self.session_failures = Table(table_size, lambda: {'count': 0}, lambda e: False)
        self.minute = {'start': clock(), 'count': 0, 'folded': 0}
        self.today = {'day': time.strftime('%Y-%m-%d', time.gmtime(clock())), 'count': 0}
        self._ready = False
        self._hello = (0.0, None)
        self._tables = threading.Lock()          # The tables pool threads touch (audit hours, sessions' failures).
        self._pending: set = set()
        self.app = None                     # The hosted studio (serve: tools/studio.py HostedStudio, M3): (session, method, path, query, headers, body).

    def close(self) -> None:
        self.settle()
        for pool in (self.sign_in_pool, self.signed_in_pool, self.scrypt_pool):
            pool.shutdown(wait=False, cancel_futures=True)
        self.pool.close()

    @contextlib.contextmanager
    def db(self):
        if not self.db_slots.acquire(timeout=10):
            raise EditorError(503, 'busy', 'The world is busy: try again in a minute.')
        try:
            try:
                lease = self.pool.connection()
            except world_db.DatabaseError as error:
                raise EditorError(503, 'database', "The world's database isn't answering yet.") from error
            with lease as conn:
                yield conn
        finally:
            self.db_slots.release()

    async def _in(self, pool, work, *args):
        return await asyncio.get_running_loop().run_in_executor(pool, work, *args)

    # -- state ---------------------------------------------------------------------------------------------------
    def ready(self) -> bool:
        progress = self.settings.progress()
        if progress is not None:
            return progress['code'] == 'ready'
        if not self._ready:
            try:
                with self.db() as conn:
                    self._ready = bool(conn.execute("SELECT to_regclass('dm.devices') IS NOT NULL").fetchone()[0])
            except Exception:
                return False
        return self._ready

    def hello_now(self) -> dict:
        """/editor/hello: public, at most 1 KB, cached for 2 seconds."""
        now = self.clock()
        at, cached = self._hello
        if cached is not None and now - at < 2:
            return cached
        official = self.settings.official()
        progress = self.settings.progress()
        state, pending = 'unknown', False
        try:
            with self.db() as conn:
                row = conn.execute('SELECT state FROM admin.server_state LIMIT 1').fetchone()
                state = row[0] if row and row[0] in ('running', 'maintenance', 'host_only') else 'unknown'
                if conn.execute("SELECT to_regclass('dm.devices') IS NOT NULL").fetchone()[0]:
                    pending = bool(conn.execute("""SELECT 1 FROM dm.admins WHERE role = 'owner' AND must_change
                                                   AND NOT disabled LIMIT 1""").fetchone())
                    self._ready = True
        except Exception:
            if progress is None:
                progress = {'code': 'database', 'percent': 0}
        if progress is None:
            progress = {'code': 'ready' if self._ready else 'migrating', 'percent': 100 if self._ready else 90}
        name = re.sub(r'[\x00-\x1f\x7f-\x9f؜​-‏‪-‮⁦-⁩﻿]', '', self.settings.name())[:40]
        hello = {'world': world_id(self.folder), 'name': name, 'package': self.settings.package(), 'editorApi': EDITOR_API,
                 'kind': 'official' if official else 'community', 'serverState': state, 'progress': progress,
                 'setupPending': pending and not official}
        rotation = _read_json(self.folder / 'rotation.json')
        if rotation.get('spki') and rotation.get('signature'):
            hello['rotation'] = {k: str(rotation[k]) for k in ('spki', 'signature', 'previous', 'algorithm') if k in rotation}
        if len(json.dumps(hello)) > HELLO_MOST:
            hello.pop('rotation', None)
            hello['package'] = ''
        self._hello = (now, hello)
        return hello

    async def hello(self) -> dict:
        return await self._in(self.sign_in_pool, self.hello_now)

    def _open(self) -> None:
        if self.settings.official():
            raise EditorError(403, 'official', "Official worlds can't be edited.")

    # -- counters --------------------------------------------------------------------------------------------------
    def _count_failure(self, kind: str) -> None:
        """The day's count for the owner, and the folding of failure log lines (above FOLD_ABOVE a minute)."""
        now = self.clock()
        day = time.strftime('%Y-%m-%d', time.gmtime(now))
        if day != self.today['day']:
            self.today = {'day': day, 'count': 0}
        self.today['count'] += 1
        if now - self.minute['start'] >= 60:
            self.flush_minute()
        self.minute['count'] += 1
        if self.minute['count'] > FOLD_ABOVE:
            self.minute['folded'] += 1
            return
        log('login.failed', lane=kind)

    def flush_minute(self) -> None:
        """Writes the minute's folded failures as one line and one audit row (called every minute by the server)."""
        folded, self.minute = self.minute['folded'], {'start': self.clock(), 'count': 0, 'folded': 0}
        if folded:
            log('login.failed.folded', count=folded)
            with contextlib.suppress(Exception):
                with self.db() as conn:
                    audit(conn, 'editor', 'login.failed.folded', '', f'{folded} more failed sign-ins in a minute.')

    def _address_wait(self, key: str | None) -> int | None:
        """Minutes this address must wait, or None. An unidentified or overflowing address answers to the world-wide
        bucket alone."""
        now = self.clock()
        entry = self.addresses.get(key) if key else None
        if entry and entry.get('until', 0) > now:
            return _wait_minutes(entry['until'], now)
        return None

    def _address_failure(self, key: str | None) -> bool:
        """Counts a failure against an address; False if it couldn't be (unidentified, or the table is full)."""
        if not key:
            return False
        entry = self.addresses.entry(key)
        if entry is None:
            return False
        next_lock(entry, self.clock(), ADDRESS_WAIT, ADDRESS_WAIT_MOST)
        return True

    def _name_wait(self, username: str) -> int | None:
        entry = self.names.get(username)
        now = self.clock()
        if entry and entry.get('until', 0) > now:
            return _wait_minutes(entry['until'], now)
        return None

    # -- sign-in ---------------------------------------------------------------------------------------------------
    def _prepare(self, username: str, invite: str | None, device: str | None) -> dict:
        with self.db() as conn:
            now = conn.execute('SELECT now()').fetchone()[0].timestamp()
            row = conn.execute("""SELECT role, password, disabled, must_change, coalesce(must_change_until < now(), false)
                                  FROM dm.admins WHERE username = %s""", (username,)).fetchone()
            known = bool(row) and row[0] != 'admin'          # An admin row never signs in to a host's Editor.
            out = {'known': known, 'record': row[1] if known else None, 'disabled': bool(row and row[2]),
                   'role': row[0] if known else None, 'one_time': bool(known and row[3] and not row[2]),
                   'expired': bool(known and row[3] and row[4]), 'invite': None, 'device': None, 'lock': None,
                   'device_wait': None, 'now': now}
            if invite is not None:
                found = conn.execute("""SELECT 1 FROM dm.invites WHERE invite_hash = %s AND username = %s AND used_at IS NULL
                                        AND expires_at > now() AND failures < %s""",
                                     (token_hash(invite), username, FAILURES)).fetchone() if known else None
                out['invite'] = 'ok' if found else 'bad'
            if device is not None:
                found = conn.execute('SELECT wait_until FROM dm.devices WHERE token_hash = %s AND username = %s',
                                     (token_hash(device), username)).fetchone() if known else None
                # A token the world forgot (a reset, a password change, End all sessions, the five-device cap) is no
                # device at all: that client is a new device again (doc 78, 6.5; M4). A token issued to another account
                # is unknown and a failure.
                elsewhere = not found and conn.execute('SELECT 1 FROM dm.devices WHERE token_hash = %s',
                                                       (token_hash(device),)).fetchone()
                if not found and not elsewhere and known:
                    device = None
                elif not found:
                    out['device'] = 'other'
                elif found[0] and found[0].timestamp() > now:
                    out['device'], out['device_wait'] = 'waiting', found[0].timestamp()
                else:
                    out['device'] = 'ok'
            if known:
                lock = conn.execute('SELECT locked_until FROM dm.locks WHERE username = %s', (username,)).fetchone()
                if lock and lock[0] and lock[0].timestamp() > now:
                    out['lock'] = lock[0].timestamp()
        return out

    async def login(self, body: dict, address: str | None) -> dict:
        """POST /editor/login {username, password, invite?, device?} -> {token, username, role, mustChange, device}."""
        self._open()
        if not self.ready():
            raise EditorError(503, 'setup', 'Your world is still setting up.')
        username, password = body.get('username'), body.get('password')
        invite, device = body.get('invite'), body.get('device')
        if (not isinstance(username, str) or not isinstance(password, str) or not password
                or (invite is not None and not isinstance(invite, str)) or (device is not None and not isinstance(device, str))):
            raise EditorError(400, 'bad-request', 'A sign-in needs a username and a password.')
        username = username.strip().lower()
        if not USERNAME.match(username):
            username = '?'                               # Never looked up, never logged as typed (it may be a password).
        invite = invite[:200] if invite else None
        device = device[:200] if device else None
        key = self.peers.key(address)
        pre = await self._in(self.sign_in_pool, self._prepare, username, invite, device)
        now = self.clock()
        if pre['invite'] == 'ok':
            lane = 'invite'
        elif pre['device'] in ('ok', 'waiting'):
            lane = 'device'
        elif pre['one_time'] and pre['invite'] is None and pre['device'] is None:
            lane = 'one-time'
        else:
            lane = 'stranger'
        forced = pre['invite'] == 'bad' or pre['device'] == 'other'

        # Waits, before any scrypt.
        if lane == 'device' and pre['device'] == 'waiting':
            raise EditorError(429, 'wait', 'Too many tries from here.', minutes=_wait_minutes(pre['device_wait'], pre['now']))
        if lane in ('one-time', 'stranger'):
            if (self.settings.new_devices() == 'approve' and pre['invite'] is None and pre['device'] is None):
                raise EditorError(403, 'approve', 'This world needs an Editor code to sign in from a new computer.')
            waited = self._address_wait(key)
            if waited:
                raise EditorError(429, 'wait', 'Too many tries from here.', minutes=waited)
            # An identified address the full table can't hold fails closed, onto the world-wide count. (An unidentified
            # one, under Docker's gateway, isn't held back by strangers in the one-time lane: doc 78, 6.5.)
            overflow = key is not None and self.addresses.get(key) is None and len(self.addresses.items) >= self.addresses.size
            if (lane == 'stranger' or overflow) and not self.world.available():
                raise EditorError(429, 'wait', 'Too many tries from here.', minutes=1)
        if lane == 'stranger':
            locked = (pre['lock'] and _wait_minutes(pre['lock'], pre['now'])) or self._name_wait(username)
            if locked:
                raise EditorError(429, 'wait', 'Too many tries from here.', minutes=locked)

        record = pre['record']
        matched = await self.gate.run(0 if lane in ('invite', 'device') else 1, lambda: check_password(record, password))
        good = matched and pre['known'] and not forced
        if good and pre['disabled']:
            raise EditorError(403, 'disabled', 'This account is turned off: ask the world\'s owner.')
        if good and pre['expired']:
            raise EditorError(401, 'expired', 'That one-time password has expired.')
        if good:
            result = await self._in(self.sign_in_pool, self._signed_in, username, lane, invite, device, key or address)
            log('login', account=username, lane=lane, addr=key or '*')
            return result

        # A failure, counted in its lane.
        if lane in ('one-time', 'stranger'):
            counted = self._address_failure(key)
            if lane == 'stranger' or (key is not None and not counted):
                self.world.spend()
        if lane == 'stranger' and not pre['known']:
            entry = self.names.entry(username)
            if entry is None:
                self.world.spend()
            else:
                next_lock(entry, now, ACCOUNT_WAIT, ACCOUNT_WAIT_MOST)
        # Its rows are written after the reply, so a known name's failure takes no longer than an unknown one's.
        self._later(self._failed, username, lane, pre, invite, device, key)
        self._count_failure(lane)
        raise EditorError(401, 'wrong', 'Wrong name or password.')

    def _later(self, work, *args) -> None:
        def safely():
            try:
                work(*args)
            except Exception as error:
                log('bookkeeping.error', error=type(error).__name__)
        with self._tables:
            self._pending = {f for f in self._pending if not f.done()}
            self._pending.add(self.sign_in_pool.submit(safely))

    def settle(self, timeout: float = 10) -> None:
        """Waits for the failures' rows to be written (tests, and before closing)."""
        with self._tables:
            pending = list(self._pending)
        concurrent.futures.wait(pending, timeout)

    def _failed(self, username: str, lane: str, pre: dict, invite, device, key) -> None:
        if not pre['known']:
            return                                           # Unknown names get no row (doc 78, 6.5).
        with self.db() as conn, conn.transaction():
            if lane == 'device':
                # Five wrong passwords from a known device make that device wait (and nothing else).
                conn.execute("""UPDATE dm.devices SET wait_until = CASE WHEN failures + 1 >= %s THEN now() + %s * interval '1 second'
                                ELSE wait_until END, failures = CASE WHEN failures + 1 >= %s THEN 0 ELSE failures + 1 END
                                WHERE token_hash = %s AND username = %s""",
                             (FAILURES, DEVICE_WAIT, FAILURES, token_hash(device), username))
            elif lane == 'invite':
                conn.execute('UPDATE dm.invites SET failures = failures + 1 WHERE invite_hash = %s', (token_hash(invite),))
            elif lane == 'stranger':
                row = conn.execute("""SELECT failures, level, extract(epoch FROM first_at), extract(epoch FROM locked_until),
                                      extract(epoch FROM now()) FROM dm.locks WHERE username = %s FOR UPDATE""", (username,)).fetchone()
                now = conn.execute('SELECT extract(epoch FROM now())').fetchone()[0]
                state = {'failures': row[0], 'level': row[1], 'first': float(row[2]),
                         'until': float(row[3]) if row[3] else None} if row else {}
                next_lock(state, float(now), ACCOUNT_WAIT, ACCOUNT_WAIT_MOST)
                conn.execute("""INSERT INTO dm.locks (username, failures, level, first_at, locked_until, updated_at)
                                VALUES (%s, %s, %s, to_timestamp(%s), to_timestamp(%s), now())
                                ON CONFLICT (username) DO UPDATE SET failures = excluded.failures, level = excluded.level,
                                first_at = excluded.first_at, locked_until = excluded.locked_until, updated_at = now()""",
                             (username, state['failures'], state.get('level', 0), state['first'], state.get('until')))
            # One audit row per account and address an hour, with a count.
            hour = int(self.clock() // 3600)
            with self._tables:
                entry = self.audit_hours.entry((username, key or '*', hour))
                if entry is None:
                    return                                   # Full: the minute's fold counts it.
                entry['count'], entry['at'] = entry.get('count', 0) + 1, self.clock()
                count, sequence = entry['count'], entry.get('sequence')
            detail = f'{count} failed sign-in{"s" if count > 1 else ""} from {key or "an unidentified address"} this hour.'
            if sequence:
                conn.execute('UPDATE dm.audit SET detail = %s, at = %s WHERE sequence = %s', (detail, time.time(), sequence))
            else:
                made = audit(conn, 'editor', 'login.failed', username, detail)
                with self._tables:
                    entry['sequence'] = made

    def _signed_in(self, username: str, lane: str, invite, device, address) -> dict:
        token = secrets.token_urlsafe(32)
        issued = None
        with self.db() as conn, conn.transaction():
            conn.execute("SELECT pg_advisory_xact_lock(hashtext('ratw-editor-sessions'))")
            row = conn.execute("""SELECT role, must_change, last_login FROM dm.admins WHERE username = %s AND NOT disabled
                                  FOR UPDATE""", (username,)).fetchone()
            if not row:
                raise EditorError(401, 'wrong', 'Wrong name or password.')
            role, must_change, previous = row
            conn.execute(f"""DELETE FROM dm.sessions WHERE expires_at < now() OR last_seen < now() - interval '{IDLE_MINUTES} minutes'""")
            own = [r[0] for r in conn.execute('SELECT token_hash FROM dm.sessions WHERE username = %s ORDER BY created_at',
                                              (username,))]
            evict = own[:max(0, len(own) - (SESSIONS_PER_ACCOUNT - 1))]
            others = conn.execute('SELECT count(*) FROM dm.sessions WHERE username <> %s', (username,)).fetchone()[0]
            if others + len(own) - len(evict) + 1 > SESSIONS_PER_WORLD:
                raise EditorError(503, 'full', "Too many people signed in to this world's Editor.")
            for old in evict:
                conn.execute('DELETE FROM dm.sessions WHERE token_hash = %s', (old,))
            if lane == 'device':
                issued = device
                conn.execute('UPDATE dm.devices SET last_used = now(), failures = 0, wait_until = NULL WHERE token_hash = %s',
                             (token_hash(device),))
            elif not must_change:
                issued = self._issue_device(conn, username)
            conn.execute(f"""INSERT INTO dm.sessions (token_hash, username, expires_at, last_seen, device, address, since)
                             VALUES (%s, %s, now() + interval '{SESSION_HOURS} hours', now(), %s, %s, %s)""",
                         (token_hash(token), username, token_hash(issued) if issued else None, (address or '')[:64], previous))
            if lane == 'invite':
                conn.execute('UPDATE dm.invites SET used_at = now() WHERE invite_hash = %s', (token_hash(invite),))
            # (A good sign-in doesn't lift the account's lock: it runs out, or ratw-editor reset or a new password lifts it.)
            conn.execute('UPDATE dm.admins SET last_login = now(), claimed_from = coalesce(claimed_from, %s) WHERE username = %s',
                         ((address or '')[:64], username))
            audit(conn, f'{username}#{short_id(token_hash(token))}', 'login', username,
                  f'Signed in from {address or "an unidentified address"}' + (' with an Editor code.' if lane == 'invite' else '.'))
        return {'token': token, 'username': username, 'role': role, 'mustChange': bool(must_change), 'device': issued}

    @staticmethod
    def _issue_device(conn, username: str) -> str:
        device = secrets.token_urlsafe(32)
        conn.execute('INSERT INTO dm.devices (token_hash, username) VALUES (%s, %s)', (token_hash(device), username))
        conn.execute('''DELETE FROM dm.devices WHERE username = %s AND token_hash NOT IN
                        (SELECT token_hash FROM dm.devices WHERE username = %s ORDER BY last_used DESC, created_at DESC LIMIT %s)''',
                     (username, username, DEVICES_PER_ACCOUNT))
        return device

    # -- sessions --------------------------------------------------------------------------------------------------
    def session_now(self, token: str) -> dict | None:
        """The live session for a bearer token, or None: not expired, not idle 30 minutes, its account on and not
        admin. Reading it renews nothing (only the keep-alive and the client's own screens do)."""
        if not isinstance(token, str) or not 20 <= len(token) <= 200:
            return None
        hashed = token_hash(token)
        with self.db() as conn:
            row = conn.execute(f"""SELECT s.username, a.role, a.must_change, a.may_act_on_wolves,
                                          coalesce(s.step_up_until > now(), false), extract(epoch FROM s.since)
                                   FROM dm.sessions s JOIN dm.admins a USING (username)
                                   WHERE s.token_hash = %s AND s.expires_at > now()
                                     AND s.last_seen > now() - interval '{IDLE_MINUTES} minutes'
                                     AND NOT a.disabled AND a.role <> 'admin'""", (hashed,)).fetchone()
        if not row:
            return None
        sid = short_id(hashed)
        return {'hash': hashed, 'account': row[0], 'role': row[1], 'mustChange': row[2],
                'mayActOnWolves': bool(row[3]) or row[1] == 'owner', 'steppedUp': row[4],
                'since': float(row[5]) if row[5] is not None else 0.0, 'session': sid,
                # The DM tool's `who` (doc 78, 6.3): requested_by and the audit name the account and the session.
                'username': f'{row[0]}#{sid}'}

    async def session(self, token: str) -> dict | None:
        return await self._in(self.signed_in_pool, self.session_now, token)

    def _seen(self, conn, session: dict) -> None:
        conn.execute("UPDATE dm.sessions SET last_seen = now() WHERE token_hash = %s AND last_seen < now() - interval '1 minute'",
                     (session['hash'],))

    def keepalive(self, session: dict) -> dict:
        with self.db() as conn:
            self._seen(conn, session)
            row = conn.execute('SELECT extract(epoch FROM expires_at), extract(epoch FROM last_seen) FROM dm.sessions WHERE token_hash = %s',
                               (session['hash'],)).fetchone()
        return {'expiresAt': float(row[0]) if row else 0, 'lastSeen': float(row[1]) if row else 0}

    def logout(self, session: dict) -> dict:
        with self.db() as conn:
            conn.execute('DELETE FROM dm.sessions WHERE token_hash = %s', (session['hash'],))
        log('logout', account=session['account'])
        return {'ok': True}

    def _typed_wrong(self, session: dict) -> None:
        """A wrong password typed in an open session (step-up, password change): five end it."""
        with self._tables:
            entry = self.session_failures.entry(session['hash'])
            count = STEP_UP_FAILURES if entry is None else entry['count'] + 1   # (A full table fails closed.)
            if entry is not None:
                entry['count'] = count
        if count >= STEP_UP_FAILURES:
            with self.db() as conn:
                conn.execute('DELETE FROM dm.sessions WHERE token_hash = %s', (session['hash'],))
            with self._tables:
                self.session_failures.pop(session['hash'])
            log('session.ended', account=session['account'], reason='wrong-passwords')
            raise EditorError(401, 'ended', 'Your Editor session ended: sign in again.')

    def _record(self, account: str):
        with self.db() as conn:
            row = conn.execute("SELECT password, must_change, coalesce(must_change_until < now(), false) FROM dm.admins WHERE username = %s",
                               (account,)).fetchone()
        return row

    async def step_up(self, session: dict, body: dict) -> dict:
        """POST /editor/step-up {password}: 5 minutes for the account routes, bound to this session."""
        password = body.get('password')
        if not isinstance(password, str) or not password:
            raise EditorError(400, 'bad-request', 'Type your password.')
        row = await self._in(self.signed_in_pool, self._record, session['account'])
        matched = await self.gate.run(0, lambda: check_password(row[0] if row else None, password))
        if not matched or not row or row[1]:
            await self._in(self.signed_in_pool, self._typed_wrong, session)
            raise EditorError(403, 'wrong-password', 'That is not your password.')

        def grant():
            with self.db() as conn:
                conn.execute(f"""UPDATE dm.sessions SET step_up_until = now() + interval '{STEP_UP_MINUTES} minutes', last_seen = now()
                                 WHERE token_hash = %s""", (session['hash'],))
            with self._tables:
                self.session_failures.pop(session['hash'])
            return {'minutes': STEP_UP_MINUTES}
        return await self._in(self.signed_in_pool, grant)

    async def change_password(self, session: dict, body: dict) -> dict:
        """POST /editor/password {old, new}: the rules on screen 4b; clears must_change; ends the account's other
        sessions and forgets its devices; this client gets a new device token. For an owner leaving a one-time
        password, asks the root entrypoint to empty first-editor.txt."""
        old, new = body.get('old'), body.get('new')
        if not isinstance(old, str) or not isinstance(new, str):
            raise EditorError(400, 'bad-request', 'Type your old password and the new one.')
        row = await self._in(self.signed_in_pool, self._record, session['account'])
        matched = await self.gate.run(0, lambda: check_password(row[0] if row else None, old))
        if not matched or not row:
            await self._in(self.signed_in_pool, self._typed_wrong, session)
            raise EditorError(403, 'wrong-password', 'That is not your password.')
        if row[1] and row[2]:
            raise EditorError(401, 'expired', 'That one-time password has expired.')
        problem = password_problem(new, session['account'], self.settings.name())
        if problem is None and hmac.compare_digest(new.encode(), old.encode()):
            problem = 'same'
        if problem:
            raise EditorError(422, 'weak', 'Choose another password.', reason=problem)
        record = await self._in(self.scrypt_pool, world_db.hash_password, new)

        def save():
            with self.db() as conn, conn.transaction():
                was = conn.execute('SELECT must_change, role FROM dm.admins WHERE username = %s FOR UPDATE',
                                   (session['account'],)).fetchone()
                conn.execute('UPDATE dm.admins SET password = %s, must_change = false, must_change_until = NULL WHERE username = %s',
                             (world_db.Jsonb(record), session['account']))
                conn.execute('DELETE FROM dm.sessions WHERE username = %s AND token_hash <> %s', (session['account'], session['hash']))
                conn.execute('DELETE FROM dm.devices WHERE username = %s', (session['account'],))
                conn.execute('DELETE FROM dm.invites WHERE username = %s', (session['account'],))
                conn.execute('DELETE FROM dm.locks WHERE username = %s', (session['account'],))
                device = self._issue_device(conn, session['account'])
                conn.execute('UPDATE dm.sessions SET device = %s, last_seen = now() WHERE token_hash = %s',
                             (token_hash(device), session['hash']))
                audit(conn, session['username'], 'account.password', session['account'],
                      f'{session["account"]} chose a new password.')
            if was and was[0] and was[1] == 'owner':
                with contextlib.suppress(OSError):
                    request_root('first-editor-used', self.folder)
            with self._tables:
                self.session_failures.pop(session['hash'])
            log('password.changed', account=session['account'])
            return {'device': device, 'role': was[1] if was else session['role'], 'mustChange': False}
        return await self._in(self.signed_in_pool, save)

    # -- the account routes (owner, with a step-up) ------------------------------------------------------------------
    def accounts(self, session: dict, verb: str, body: dict, address_key: str | None) -> dict:
        if session['role'] != 'owner':
            raise EditorError(403, 'owner-only', "Only the world's owner manages Editor accounts.")
        if not session['steppedUp']:
            raise EditorError(403, 'step-up', 'Type your password again first.')
        who = session['username']
        with self.db() as conn:
            self._seen(conn, session)
            if verb == 'list':
                failed = conn.execute(r"""SELECT coalesce(sum((substring(detail from '^(\d+)'))::int), 0) FROM dm.audit
                                         WHERE action IN ('login.failed', 'login.failed.folded') AND at > %s""",
                                      (time.time() - 24 * 3600,)).fetchone()[0]
                banners = [{'at': r[0], 'action': r[1], 'target': r[2], 'detail': r[3], 'who': r[4]} for r in conn.execute(
                    """SELECT at, action, target, detail, who FROM dm.audit WHERE action LIKE 'account.%%' AND at > %s
                       ORDER BY sequence DESC LIMIT 50""", (session['since'],))]
                return {'accounts': list_accounts(conn), 'failedToday': max(int(failed), self.today['count']),
                        'banners': banners}
            username = body.get('username')
            if verb != 'end-sessions' and (not isinstance(username, str) or not USERNAME.match(username)):
                raise EditorError(422, 'username', 'Name an Editor account.')
            if verb == 'add':
                password, invite = add_account(conn, username, body.get('role', 'dm'), who,
                                               may_act_on_wolves=body.get('mayActOnWolves') is True)
                result = {'code': self._code(username, password, invite)}
            elif verb == 'reset':
                if _target_role(conn, username) == 'owner':
                    raise EditorError(403, 'owner-account', "An owner's account is reset only on the world's computer (reset-editor-password).")
                password, invite = reset_account(conn, username, who)
                result = {'code': self._code(username, password, invite)}
            elif verb == 'role':
                set_role(conn, username, body.get('role'), who)
                result = {'ok': True}
            elif verb == 'wolves':
                set_wolves(conn, username, body.get('allowed') is True, who)
                result = {'ok': True}
            elif verb in ('disable', 'enable'):
                set_disabled(conn, username, verb == 'disable', who)
                result = {'ok': True}
            elif verb == 'remove':
                remove_account(conn, username, who)
                result = {'ok': True}
            elif verb == 'end-sessions':
                end_all(conn, who)
                result = {'ok': True}
            else:
                raise EditorError(404, 'not-found', 'No such route.')
        log('account.' + verb, account=session['account'], target=username if verb != 'end-sessions' else None,
            addr=address_key or '*')
        return result

    def _code(self, username: str, password: str, invite: str) -> str:
        return make_code(world_id(self.folder), cert_spki(self.folder / 'editor-cert.pem'), username, password, invite)


def _target_role(conn, username: str) -> str | None:
    row = conn.execute("SELECT role FROM dm.admins WHERE username = %s AND role <> 'admin'", (username,)).fetchone()
    if not row:
        raise EditorError(404, 'no-account', f'No Editor account {username}.')
    return row[0]


def short_id(hashed: str) -> str:
    """A session's short id for the audit and requested_by: derived from its stored hash, never the token."""
    return hashlib.sha256(('sid:' + hashed).encode()).hexdigest()[:6]


# ---------------------------------------------------------------------------------------------------------- server

class Request:
    def __init__(self, method: str, path: str, query: str, headers: dict, version: str):
        self.method, self.path, self.query, self.headers, self.version = method, path, query, headers, version


class Reply(Exception):
    """A reply to send now; `close` when the connection can't be reused (a body left unread, a broken request)."""

    def __init__(self, status: int, payload=None, close: bool = False, body: bytes | None = None,
                 content_type: str = 'application/json', encoding: str | None = None):
        super().__init__(status)
        self.status, self.close, self.content_type, self.encoding = status, close, content_type, encoding
        self.body = body if body is not None else json.dumps(payload if payload is not None else {}).encode('utf-8')


def _refuse(error: EditorError, close: bool = False) -> Reply:
    return Reply(error.status, error.payload(), close)


class Connections:
    """Per-address and world-wide connection limits, checked before the handshake."""

    def __init__(self, peers: Peers, clock=time.monotonic, per_address: int = PER_ADDRESS_OPEN, world: int = WORLD_OPEN,
                 table_size: int = TABLE_SIZE):
        self.peers, self.clock, self.per_address, self.most = peers, clock, per_address, world
        self.open_total = 0
        self.open = {}
        self.world = Bucket(*CONNECTIONS, clock)
        self.handshakes = Table(table_size, lambda: {'bucket': Bucket(*HANDSHAKES, clock)},
                                lambda e: e['bucket'].available() and e['bucket'].tokens >= e['bucket'].capacity - 0.01)

    def admit(self, address: str | None) -> str | bool:
        """The address's key ('' when unidentified) if it may connect, else False."""
        if self.open_total >= self.most or not self.world.take():
            return False
        key = self.peers.key(address)
        if key is None:
            key = ''
        else:
            entry = self.handshakes.entry(key)
            if entry is None:
                key, entry = '*', self.handshakes.get('*') or self.handshakes.items.setdefault('*', {'bucket': Bucket(*HANDSHAKES, self.clock)})
            if self.open.get(key, 0) >= self.per_address or not entry['bucket'].take():
                return False
        self.open[key] = self.open.get(key, 0) + 1
        self.open_total += 1
        return key

    def release(self, key: str) -> None:
        self.open_total -= 1
        self.open[key] -= 1
        if not self.open[key]:
            del self.open[key]


class _Accepted(asyncio.Protocol):
    """The Editor port's TCP side: pauses at once, so no byte is read before the connection limits and the handshake."""

    def __init__(self, server):
        self.server = server

    def connection_made(self, transport) -> None:
        transport.pause_reading()
        self.server._accepted(transport)

    def data_received(self, data) -> None:      # (Never: reading is paused until the TLS protocol takes over.)
        pass


class EditorServer:
    """The Editor port: TLS 1.3, HTTP/1.1 with keep-alive, the browser rule, the limits, and the routes."""

    def __init__(self, core: Core, host: str = '0.0.0.0', port: int = PORT, context_factory=None,
                 handshake_seconds: float = HANDSHAKE_SECONDS, idle_seconds: float = IDLE_SECONDS,
                 connections: Connections | None = None):
        self.core, self.host, self.port = core, host, port
        self.context_factory = context_factory or (lambda: server_context(core.folder))
        self.context = self.context_factory()
        self.handshake_seconds, self.idle_seconds = handshake_seconds, idle_seconds
        self.connections = connections or Connections(core.peers)
        self.large = asyncio.Semaphore(LARGE_AT_ONCE)
        self.rates = Table(TABLE_SIZE, lambda: {'bucket': Bucket(*REQUESTS_PER_SECOND, time.monotonic)}, lambda e: True)
        self.server = None
        self._minute = None

    def reload(self) -> None:
        """SIGHUP: the key again (after `ratw-editor new-certificate`). New connections use it; a bad key keeps the old."""
        try:
            self.context = self.context_factory()
            self.core._hello = (0.0, None)
            log('certificate.reloaded', fingerprint=fingerprint(cert_spki(self.core.folder / 'editor-cert.pem')).replace(' ', ''))
        except Exception as error:
            log('certificate.reload-failed', error=type(error).__name__)

    async def start(self) -> int:
        loop = asyncio.get_running_loop()
        self.server = await loop.create_server(lambda: _Accepted(self), self.host, self.port, backlog=128)
        self.port = self.server.sockets[0].getsockname()[1]
        self._minute = loop.create_task(self._every_minute())
        return self.port

    async def _every_minute(self) -> None:
        while True:
            await asyncio.sleep(60)
            with contextlib.suppress(Exception):
                await self.core._in(self.core.sign_in_pool, self.core.flush_minute)

    async def stop(self) -> None:
        if self._minute:
            self._minute.cancel()
        if self.server:
            self.server.close()
            with contextlib.suppress(Exception):
                await asyncio.wait_for(self.server.wait_closed(), 5)

    def _accepted(self, transport) -> None:
        """A new TCP connection, its reading paused (nothing is read before the limits say yes)."""
        peer = transport.get_extra_info('peername')
        address = peer[0] if isinstance(peer, tuple) and peer else None
        key = self.connections.admit(address)
        if key is False:
            transport.abort()
            return
        asyncio.get_running_loop().create_task(self._connection(transport, address, key))

    async def _connection(self, transport, address, key) -> None:
        loop = asyncio.get_running_loop()
        tls = None
        try:
            # The TLS transport gets a reader and writer of its own (not StreamWriter.start_tls, whose reader keeps the
            # TCP transport for its flow control: two large bodies read at once then stalled for good).
            reader = asyncio.StreamReader(limit=HEAD_LIMIT, loop=loop)
            protocol = asyncio.StreamReaderProtocol(reader, loop=loop)
            try:
                tls = await loop.start_tls(transport, protocol, self.context, server_side=True,
                                           ssl_handshake_timeout=self.handshake_seconds)
            except Exception:
                return                   # Plain HTTP, TLS 1.2, a slow or broken handshake: no reply at all (I2).
            protocol.connection_made(tls)
            writer = asyncio.StreamWriter(tls, protocol, reader, loop)
            await self._serve(reader, writer, address)
        except Exception as error:      # Nothing a connection does can stop the helper.
            if not isinstance(error, (ConnectionError, asyncio.TimeoutError, asyncio.IncompleteReadError, ssl.SSLError)):
                log('connection.error', error=type(error).__name__)
        finally:
            self.connections.release(key)
            (tls or transport).abort()
            if tls is not None:
                transport.abort()

    async def _serve(self, reader, writer, address) -> None:
        first = True
        while True:
            try:
                head = await asyncio.wait_for(reader.readuntil(b'\r\n\r\n'),
                                              FIRST_REQUEST_SECONDS if first else self.idle_seconds)
            except asyncio.LimitOverrunError:
                await self._send(writer, Reply(431, {'error': 'too-large'}, close=True))
                return
            except (asyncio.IncompleteReadError, asyncio.TimeoutError, ConnectionError, ssl.SSLError):
                return
            first = False
            try:
                request = self._parse(head)
                reply = await self._handle(request, reader, address)
            except Reply as done:
                reply = done
            except EditorError as error:
                reply = _refuse(error)
            except Exception as error:
                log('handler.error', error=type(error).__name__)
                reply = Reply(500, {'error': 'internal', 'message': 'Something went wrong in the helper.'})
            await self._send(writer, reply)
            if reply.close:
                return

    @staticmethod
    def _parse(head: bytes) -> Request:
        lines = head.decode('latin-1').split('\r\n')
        parts = lines[0].split(' ')
        if len(parts) != 3 or parts[2] not in ('HTTP/1.1', 'HTTP/1.0') or not parts[1].startswith('/') or len(parts[1]) > 2048:
            raise Reply(400, {'error': 'bad-request'}, close=True)
        if not re.fullmatch(r'[\x21-\x7e]+', parts[1]) or not re.fullmatch(r'[A-Z]{1,10}', parts[0]):
            raise Reply(400, {'error': 'bad-request'}, close=True)
        headers = {}
        for line in lines[1:]:
            if not line:
                continue
            name, sep, value = line.partition(':')
            name = name.lower()
            if not sep or not re.fullmatch(r"[a-z0-9!#$%&'*+.^_`|~-]+", name) or len(headers) >= MAX_HEADERS:
                raise Reply(400, {'error': 'bad-request'}, close=True)
            if name in headers and name in ('content-length', 'authorization', 'transfer-encoding', 'host'):
                raise Reply(400, {'error': 'bad-request'}, close=True)
            headers[name] = value.strip()
        path, _, query = parts[1].partition('?')
        return Request(parts[0], path, query, headers, parts[2])

    async def _read_body(self, reader, length: int) -> bytes:
        if not length:
            return b''
        try:
            return await asyncio.wait_for(reader.readexactly(length), BODY_SECONDS)
        except (asyncio.IncompleteReadError, asyncio.TimeoutError):
            raise Reply(400, {'error': 'bad-request'}, close=True)

    @staticmethod
    def _json(body: bytes) -> dict:
        try:
            value = json.loads(body or b'{}')
        except ValueError:
            raise Reply(400, {'error': 'bad-request', 'message': 'The body must be JSON.'})
        if not isinstance(value, dict):
            raise Reply(400, {'error': 'bad-request', 'message': 'The body must be a JSON object.'})
        return value

    async def _handle(self, request: Request, reader, address) -> Reply:
        headers = request.headers
        has_body = 'content-length' in headers or 'transfer-encoding' in headers
        # I9: browsers can't talk to the helper.
        if 'origin' in headers or 'cookie' in headers or any(name.startswith('sec-fetch-') for name in headers):
            return Reply(403, {'error': 'browser', 'message': 'The Editor answers only the RATW client.'}, close=has_body)
        if request.method not in ('GET', 'POST'):
            return Reply(405, {'error': 'method'}, close=has_body)
        if 'transfer-encoding' in headers:
            return Reply(411, {'error': 'length-required'}, close=True)
        length_text = headers.get('content-length')
        if length_text is not None and not re.fullmatch(r'\d{1,10}', length_text):
            return Reply(400, {'error': 'bad-request'}, close=True)
        length = int(length_text) if length_text is not None else None
        if request.method == 'POST' and length is None:
            return Reply(411, {'error': 'length-required'}, close=True)
        length = length or 0
        close = request.version == 'HTTP/1.0' or headers.get('connection', '').lower() == 'close'
        core = self.core

        if request.path in PUBLIC_PATHS:
            if length > PUBLIC_BODY:
                return Reply(413, {'error': 'too-large'}, close=True)
            body = await self._read_body(reader, length)
            if request.path == '/editor/hello':
                if request.method != 'GET':
                    return Reply(405, {'error': 'method'}, close=close)
                return Reply(200, await core.hello(), close=close)
            if request.method != 'POST':
                return Reply(405, {'error': 'method'}, close=close)
            try:
                return Reply(200, await core.login(self._json(body), address), close=close)
            except EditorError as error:
                return _refuse(error, close)

        # Everything else needs a session, checked before any body is read.
        if core.settings.official():
            return Reply(403, {'error': 'official', 'message': "Official worlds can't be edited."}, close=True)
        authorization = headers.get('authorization', '')
        token = authorization[7:] if authorization.startswith('Bearer ') else ''
        session = await core.session(token) if token else None
        if not session:
            return Reply(401, {'error': 'signed-out', 'message': 'Your Editor session ended: sign in again.'},
                         close=close or bool(length))
        rate = self.rates.entry(session['hash'])
        if rate is not None and not rate['bucket'].take():
            return Reply(429, {'error': 'slow-down'}, close=close or bool(length))
        editor_route = request.path.startswith('/editor/')
        most = EDITOR_BODY if editor_route else SIGNED_IN_BODY if request.path.startswith('/api/') else 0
        if length > most:
            return Reply(413, {'error': 'too-large'}, close=True)
        if session['mustChange'] and request.path not in ONE_TIME_PATHS:
            return Reply(403, {'error': 'must-change', 'message': 'Choose your own password first.'}, close=close or bool(length))
        large = length > LARGE_BODY
        if large:
            if self.large.locked():
                return Reply(503, {'error': 'busy', 'message': 'The world is busy: try again in a minute.'}, close=True)
            await self.large.acquire()
        try:
            body = await self._read_body(reader, length)
            if editor_route:
                reply = await self._editor(session, request, body, address)
            else:
                reply = await self._app(session, request, body)
        finally:
            if large:
                self.large.release()
        reply.close = reply.close or close
        log('request', account=session['account'], route=request.path[:80], status=reply.status)
        return reply

    async def _editor(self, session: dict, request: Request, body: bytes, address) -> Reply:
        core, path = self.core, request.path
        try:
            if path == '/editor/keepalive' and request.method == 'POST':
                return Reply(200, await core._in(core.signed_in_pool, core.keepalive, session))
            if path == '/editor/logout' and request.method == 'POST':
                return Reply(200, await core._in(core.signed_in_pool, core.logout, session))
            if path == '/editor/password' and request.method == 'POST':
                return Reply(200, await core.change_password(session, self._json(body)))
            if path == '/editor/step-up' and request.method == 'POST':
                return Reply(200, await core.step_up(session, self._json(body)))
            if path == '/editor/accounts' and request.method == 'GET':
                return Reply(200, await core._in(core.signed_in_pool, core.accounts, session, 'list', {}, core.peers.key(address)))
            if path.startswith('/editor/accounts/') and request.method == 'POST':
                verb = path[len('/editor/accounts/'):]
                if verb not in ('add', 'reset', 'role', 'wolves', 'disable', 'enable', 'remove', 'end-sessions'):
                    return Reply(404, {'error': 'not-found'})
                return Reply(200, await core._in(core.signed_in_pool, core.accounts, session, verb, self._json(body),
                                                 core.peers.key(address)))
            return Reply(404, {'error': 'not-found'})
        except EditorError as error:
            return _refuse(error)

    async def _app(self, session: dict, request: Request, body: bytes) -> Reply:
        """/api/... and the page's files: the hosted studio (tools/studio.py's HostedStudio, doc 78 M3), given the session
        this checked. Large text replies go gzipped when the client takes it (the shell's proxy does: M0a)."""
        app = self.core.app
        if app is None:
            return Reply(404, {'error': 'not-found'})
        status, content_type, data = await self.core._in(self.core.signed_in_pool, app, session, request.method,
                                                         request.path, request.query, request.headers, body)
        if request.path.startswith('/api/') and content_type not in ('application/json', 'application/octet-stream'):
            content_type = 'application/json'      # No API reply can run as script.
        encoding = None
        if (len(data) >= GZIP_ABOVE and 'gzip' in request.headers.get('accept-encoding', '')
                and content_type in ('application/json', 'text/html', 'text/javascript', 'text/css', 'image/svg+xml')):
            data = await self.core._in(self.core.signed_in_pool, gzip.compress, data, 3)
            encoding = 'gzip'
        return Reply(status, body=data, content_type=content_type, encoding=encoding)

    @staticmethod
    async def _send(writer, reply: Reply) -> None:
        head = [f'HTTP/1.1 {reply.status} {STATUS_WORDS.get(reply.status, "Error")}',
                f'Content-Type: {reply.content_type}', f'Content-Length: {len(reply.body)}',
                'X-Content-Type-Options: nosniff', 'Cache-Control: no-store',
                f'Connection: {"close" if reply.close else "keep-alive"}']
        if reply.encoding:
            head += [f'Content-Encoding: {reply.encoding}', 'Vary: Accept-Encoding']
        writer.write(('\r\n'.join(head) + '\r\n\r\n').encode('latin-1') + reply.body)
        try:
            await asyncio.wait_for(writer.drain(), 60)
        except Exception:
            reply.close = True


def hosted_studio(folder: Path):
    """The hosted studio for this world (doc 78, 7; tools/studio.py): the DM tool, and Atlas from slice 2, behind the
    session. Its registry is the container's one-world copy in this folder, when the entrypoint wrote one."""
    if (folder / 'worlds.json').exists():
        os.environ.setdefault('RATW_WORLDS_FILE', str(folder / 'worlds.json'))
        os.environ.setdefault('RATW_WORLDS_LOCAL', str(folder / 'worlds.local.json'))
    import studio
    return studio.HostedStudio(studio.Hosted(WORLD_KEY, folder))


async def serve(folder: Path, host: str, port: int) -> int:
    settings = Settings(folder)
    if not settings.enabled():
        log('disabled')                                          # [editor] enabled = false: nothing listens.
        return 0
    core = Core(settings=settings, folder=folder)
    core.app = hosted_studio(folder)
    server = EditorServer(core, host, port)
    bound = await server.start()
    _write_atomic(folder / 'helper.pid', f'{os.getpid()}\n', 0o644)
    loop = asyncio.get_running_loop()
    stopped = asyncio.Event()
    loop.add_signal_handler(signal.SIGHUP, server.reload)
    for number in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(number, stopped.set)
    log('listening', port=bound, fingerprint=fingerprint(cert_spki(folder / 'editor-cert.pem')).replace(' ', ''),
        peers=core.peers.mode)
    await stopped.wait()
    await server.stop()
    core.close()
    with contextlib.suppress(FileNotFoundError):
        (folder / 'helper.pid').unlink()
    return 0


# -------------------------------------------------------------------------------------------------------- ratw-editor

def _read_dm_password() -> str | None:
    try:
        for line in SECRETS_FILE.read_text(encoding='utf-8').splitlines():
            name, sep, value = line.partition('=')
            if sep and name.strip() == 'RATW_DM_PASSWORD':
                return value.strip().strip('"\'')
    except OSError:
        return None
    return None


def _as_admin(argv: list[str]) -> int | None:
    """As root (`docker compose exec`): read the dm password, then run again as ratw-admin. None: already as it."""
    if os.geteuid() != 0 or os.environ.get('RATW_EDITOR_DROPPED') == '1':
        return None
    import pwd
    try:
        account = pwd.getpwnam(ADMIN_USER)
    except KeyError:
        print(f'ratw-editor: no {ADMIN_USER} user in this image; refusing to run as root.', file=sys.stderr)
        return 2
    password = _read_dm_password()
    if not password:
        print(f'ratw-editor: no dm password in {SECRETS_FILE}.', file=sys.stderr)
        return 2
    env = {k: v for k, v in os.environ.items() if not k.endswith('_PASSWORD') and k not in ('RATW_HOST_TOKEN', 'RATW_AI_KEY')}
    env.update(RATW_DM_PASSWORD=password, RATW_EDITOR_DROPPED='1', HOME=str(ADMIN_DIR))
    if (ADMIN_DIR / 'worlds.json').exists():
        env.update(RATW_WORLDS_FILE=str(ADMIN_DIR / 'worlds.json'), RATW_WORLDS_LOCAL=str(ADMIN_DIR / 'worlds.local.json'))
    return subprocess.run([sys.executable, str(Path(__file__).resolve()), *argv], env=env, user=account.pw_uid,
                          group=account.pw_gid, extra_groups=[]).returncode


def _connect_cli():
    return world_db.connect(WORLD_KEY, 'dm')


def _print_code(username: str, password: str | None, invite: str | None, folder: Path, address: str | None,
                hours: float) -> None:
    code = make_code(world_id(folder), cert_spki(folder / 'editor-cert.pem'), username, password, invite, address)
    print(f'Editor account: {username}')
    if password:
        print(f'One-time password: {password}')
    print(f'Editor code (a secret: works once, for {int(hours)} hours):\n{code}')


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    parser = argparse.ArgumentParser(prog='ratw-editor', description="Your world's Editor accounts and certificate.")
    parser.add_argument('--admin-dir', type=Path, default=None, help=argparse.SUPPRESS)
    sub = parser.add_subparsers(dest='command', required=True)
    serve_args = sub.add_parser('serve', help='run the helper')
    serve_args.add_argument('--host', default='0.0.0.0')
    serve_args.add_argument('--port', type=int, default=PORT)
    sub.add_parser('list', help='the Editor accounts')
    add = sub.add_parser('add', help='a new account (the only way to make an owner)')
    add.add_argument('name')
    add.add_argument('--role', choices=ROLES, default='dm')
    add.add_argument('--address', default=None, help="the world's address, put in the Editor code")
    for name, words in (('reset', 'a new Editor code; unlocks; ends its sessions and devices'),
                        ('invite', 'an Editor code for a new device, password unchanged')):
        command = sub.add_parser(name, help=words)
        command.add_argument('name')
        command.add_argument('--address', default=None)
    for name in ('disable', 'enable', 'remove'):
        sub.add_parser(name).add_argument('name')
    sub.add_parser('sign-out-all', help='end every Editor session and forget every device')
    sub.add_parser('fingerprint', help="the world's certificate fingerprint")
    certificate = sub.add_parser('new-certificate', help='a new key, signed by the old one')
    certificate.add_argument('--leaked', action='store_true', help='the old key leaked: no signature, every session ends')
    sub.add_parser('first-owner', help=argparse.SUPPRESS)
    made = sub.add_parser('make-certificate', help=argparse.SUPPRESS)
    made.add_argument('--world-id', default='')
    args = parser.parse_args(argv)
    folder = args.admin_dir or ADMIN_DIR

    if args.command == 'serve':
        return asyncio.run(serve(folder, args.host, args.port))
    if args.command == 'make-certificate':
        made_now = make_certificate(folder, args.world_id)
        print(json.dumps({'made': made_now, 'world': world_id(folder),
                          'fingerprint': fingerprint(cert_spki(folder / 'editor-cert.pem'))}))
        return 0
    if args.command == 'fingerprint':
        print(certificate_text(folder), end='')
        return 0
    dropped = _as_admin(argv)
    if dropped is not None:
        return dropped
    try:
        if args.command == 'new-certificate':
            done = new_certificate(folder, args.leaked)
            if args.leaked:
                with _connect_cli() as conn:
                    end_all(conn, 'ratw-editor')
            reloaded = signal_helper(folder)
            print(f'New Editor certificate: {done["fingerprint"]} (was {done["previous"]}).')
            if args.leaked:
                print('Every Editor session has ended and every client will warn. Send your co-builders new Editor codes '
                      '(ratw-editor reset NAME).')
            else:
                print('Signed by the old key: clients that knew the old one move over without a warning.')
            if not reloaded:
                print('The Editor will use it when the world next starts.')
            return 0
        settings = Settings(folder)
        with _connect_cli() as conn:
            if args.command == 'first-owner':
                if settings.official():
                    print(json.dumps(None))
                    return 0
                made_owner = ensure_first_owner(conn, settings.owner_name())
                if made_owner:
                    made_owner['code'] = make_code(world_id(folder), cert_spki(folder / 'editor-cert.pem'),
                                                   made_owner['username'], made_owner['password'], made_owner['invite'])
                    made_owner['days'] = FIRST_OWNER_DAYS
                print(json.dumps(made_owner))
                return 0
            if args.command == 'list':
                for account in list_accounts(conn):
                    state = ('off' if account['disabled'] else 'not yet claimed' if account['unclaimed']
                             else 'on a one-time password' if account['mustChange'] else 'on')
                    print(f'{account["username"]:<32} {account["role"]:<8} {state:<24} devices: {len(account["devices"])}')
                return 0
            if args.command == 'add':
                password, invite = add_account(conn, args.name, args.role, 'ratw-editor', roles=ROLES)
                _print_code(args.name, password, invite, folder, args.address, CODE_HOURS)
            elif args.command == 'reset':
                password, invite = reset_account(conn, args.name, 'ratw-editor')
                _print_code(args.name, password, invite, folder, args.address, CODE_HOURS)
            elif args.command == 'invite':
                invite = invite_account(conn, args.name, 'ratw-editor')
                _print_code(args.name, None, invite, folder, args.address, CODE_HOURS)
            elif args.command in ('disable', 'enable'):
                set_disabled(conn, args.name, args.command == 'disable', 'ratw-editor')
                print(f'{args.name} is turned {"off" if args.command == "disable" else "on"}.')
            elif args.command == 'remove':
                remove_account(conn, args.name, 'ratw-editor')
                print(f'{args.name} is removed.')
            elif args.command == 'sign-out-all':
                end_all(conn, 'ratw-editor')
                print('Every Editor session has ended, and every device is forgotten.')
        return 0
    except EditorError as error:
        print(f'ratw-editor: {error.message}', file=sys.stderr)
        return 1
    except world_db.DatabaseError as error:
        print(f'ratw-editor: the database: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
