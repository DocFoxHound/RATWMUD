#!/usr/bin/env python3
"""Run and manage the RATW PostgreSQL databases: one world in each, every world in the registry.

  python3 tools/world_db.py up                    start PostgreSQL (first run creates Database/.env)
  python3 tools/world_db.py list                  the worlds: kind, upstream, database, Atlas's and the live one
  python3 tools/world_db.py use KEY               Atlas's world (a dev world): Atlas, builds, local servers, tools
  python3 tools/world_db.py serve-world KEY --yes the live server's world (a live world; rarely wise)
  python3 tools/world_db.py new KEY --name NAME [--kind dev|live] [--upstream KEY] [--from KEY [--without-map]]
                                                  a new world: an empty database with every migration, or a copy
  python3 tools/world_db.py migrate               apply Database/migrations to every world (--db KEY: one)
  python3 tools/world_db.py status                show connection, migrations and row counts of every world
  python3 tools/world_db.py set-publish-password  change the Push to live password (PROD)
  python3 tools/world_db.py psql KEY              open psql on a world (--role editor|publisher|game|owner)
  python3 tools/world_db.py resolve atlas|live|KEY  the world key (atlas, live: the local choices); exit 1 if unknown
  python3 tools/world_db.py down                  stop PostgreSQL (data is kept)

The worlds (Docs/Design/66-worlds-atlas-and-dm.md, section 1) are listed in Database/worlds.json (in git). Which one
is Atlas's and which one the live server runs is this computer's choice, in Saved/Config/worlds.local.json (not in
git; {"atlas": "ridgemere2", "live": "prod"} while it is missing). RATW_WORLDS_FILE and RATW_WORLDS_LOCAL point
elsewhere (tests use scratch ones).

Connection settings come from Database/.env, overridden by environment
variables of the same name. RATW_<KEY>_HOST/RATW_<KEY>_PORT (RATW_PROD_HOST,
RATW_PROD_PORT...) point one world's database elsewhere (e.g. a hosted PROD);
otherwise all use RATW_DB_HOST/RATW_DB_PORT (127.0.0.1:5433).
RATW_<KEY>_DBNAME renames a database (tests use scratch ones).
"""
from __future__ import annotations

import argparse
import base64
import contextlib
import getpass
import hashlib
import hmac
import json
import os
from pathlib import Path
import re
import secrets
import subprocess
import sys
import threading
import time

ROOT = Path(__file__).resolve().parent.parent
DATABASE_DIR = ROOT / 'Database'
ENV_FILE = DATABASE_DIR / '.env'
MIGRATIONS = DATABASE_DIR / 'migrations'
ARTIFACTS = ROOT / 'artifacts'
# The worlds (doc 66, section 1): the registry in git, and this computer's choice of Atlas's world and the live one.
WORLDS_FILE = DATABASE_DIR / 'worlds.json'
LOCAL_FILE = ROOT / 'Saved' / 'Config' / 'worlds.local.json'
# With no local choice: Atlas, the builds, the local and scratch servers and the talk and voice tools on Ridgemere 2.0
# (the user, 2026-10-30: "Switch them all to the new 2.0"); the live server on PROD.
DEFAULT_LOCAL = {'atlas': 'ridgemere2', 'live': 'prod'}
KINDS = ('dev', 'live')
# Words the tools take in place of a world key (or beside one), so no world may be called them.
RESERVED_KEYS = frozenset({'all', 'atlas', 'live', 'town', 'latest', 'export', 'demo', 'none'})
KEY_PATTERN = re.compile(r'^[a-z][a-z0-9_]{0,30}$')
# {key: database name} of every registered world. Kept up to date in place (worlds()), so callers may hold on to it.
DATABASES: dict[str, str] = {}
ROLES = ('owner', 'editor', 'publisher', 'game', 'dm')
# Who may connect to a world's database: a live world's isn't Atlas's to edit (doc 20: the editor never reaches PROD).
CONNECT_ROLES = {'dev': ('editor', 'publisher', 'game', 'dm'), 'live': ('publisher', 'game', 'dm')}
PASSWORD_KEYS = ['POSTGRES_PASSWORD'] + [f'RATW_{r.upper()}_PASSWORD' for r in ROLES]
DEFAULT_PUBLISH_PASSWORD = 'password'
SCRYPT = {'n': 2 ** 15, 'r': 8, 'p': 1}


class DatabaseError(RuntimeError):
    pass


# --------------------------------------------------------------------------- Worlds

def registry_file() -> Path:
    return Path(os.environ.get('RATW_WORLDS_FILE') or WORLDS_FILE)


def local_file() -> Path:
    return Path(os.environ.get('RATW_WORLDS_LOCAL') or LOCAL_FILE)


def check_registry(entries) -> list[dict]:
    """The registry's worlds, checked: unique keys and databases, a kind each, upstreams only on dev worlds and only
    to live ones."""
    if not isinstance(entries, list) or not entries:
        raise DatabaseError('The world registry lists no worlds.')
    seen, databases, out = set(), set(), []
    for entry in entries:
        if not isinstance(entry, dict):
            raise DatabaseError('Every world in the registry must be an object.')
        key, database, kind = entry.get('key'), entry.get('database'), entry.get('kind')
        if not isinstance(key, str) or not KEY_PATTERN.match(key) or key in RESERVED_KEYS:
            raise DatabaseError(f'World key {key!r}: a lowercase name (letters, digits, _), not one of the reserved words.')
        if key in seen:
            raise DatabaseError(f'Two worlds are called {key}.')
        if not isinstance(database, str) or not re.match(r'^[a-z][a-z0-9_]{0,62}$', database) or database in databases:
            raise DatabaseError(f'World {key}: database {database!r} must be a lowercase name used by no other world.')
        if kind not in KINDS:
            raise DatabaseError(f'World {key}: kind must be dev or live, not {kind!r}.')
        seen.add(key)
        databases.add(database)
        out.append({'key': key, 'name': str(entry.get('name') or key), 'database': database, 'kind': kind,
                    **({'upstream': entry['upstream']} if entry.get('upstream') else {})})
    kinds = {w['key']: w['kind'] for w in out}
    for w in out:
        if 'upstream' in w and (w['kind'] != 'dev' or kinds.get(w['upstream']) != 'live'):
            raise DatabaseError(f'World {w["key"]}: only a dev world has an upstream, and it must be a registered live world.')
    return out


_registry_cache: dict = {}


def worlds() -> list[dict]:
    """Every registered world, in the registry's order: [{key, name, database, kind[, upstream]}]. Read again when
    the file changes, so a long-running host sees a world added by `new`."""
    path = registry_file()
    try:
        stamp = (str(path), path.stat().st_mtime_ns, path.stat().st_size)
    except OSError as error:
        raise DatabaseError(f'No world registry at {path}.') from error
    if _registry_cache.get('stamp') != stamp:
        try:
            data = json.loads(path.read_text(encoding='utf-8'))
        except ValueError as error:
            raise DatabaseError(f'The world registry {path} is not valid JSON: {error}') from error
        entries = check_registry(data.get('worlds') if isinstance(data, dict) else None)
        _registry_cache.update(stamp=stamp, worlds=entries)
        DATABASES.clear()
        DATABASES.update({w['key']: w['database'] for w in entries})
    return [dict(w) for w in _registry_cache['worlds']]


def world(key: str) -> dict:
    for w in worlds():
        if w['key'] == key:
            return w
    raise DatabaseError(f'Unknown world {key!r}; the worlds are {", ".join(w["key"] for w in worlds())} '
                        '(python3 tools/world_db.py list).')


def kind(key: str) -> str:
    return world(key)['kind']


def upstream(key: str) -> str | None:
    return world(key).get('upstream')


def keys(of_kind: str | None = None) -> list[str]:
    """The registered worlds' keys, or only the dev or the live ones."""
    return [w['key'] for w in worlds() if of_kind in (None, w['kind'])]


def local_choice() -> dict[str, str]:
    """This computer's choice, {'atlas': KEY, 'live': KEY}: the defaults for whatever the local file doesn't say."""
    choice = dict(DEFAULT_LOCAL)
    path = local_file()
    if path.is_file():
        try:
            data = json.loads(path.read_text(encoding='utf-8'))
        except ValueError as error:
            raise DatabaseError(f'{path} is not valid JSON: {error}') from error
        if isinstance(data, dict):
            choice.update({k: v for k, v in data.items() if k in DEFAULT_LOCAL and isinstance(v, str) and v})
    return choice


def _chosen(which: str, of_kind: str, command: str) -> str:
    key = local_choice()[which]
    if key not in keys(of_kind):
        raise DatabaseError(f'The {which} world is {key}, which is not a registered {of_kind} world. Choose one: '
                            f'python3 tools/world_db.py {command} KEY')
    return key


def atlas_world() -> str:
    """Atlas's world (a dev world): Atlas, the world builder, local and scratch servers and the talk and voice tools
    use it unless told otherwise."""
    return _chosen('atlas', 'dev', 'use')


def live_world() -> str:
    """The world the live game server runs (a live world). Switched only from the DM, rarely."""
    return _chosen('live', 'live', 'serve-world --yes')


def resolve(name: str, of_kind: str | None = None) -> str:
    """A world key from what a user typed: `atlas` and `live` are the local choices, anything else a registered key
    (of that kind, if given)."""
    key = atlas_world() if name == 'atlas' else live_world() if name == 'live' else world(name)['key']
    if of_kind and kind(key) != of_kind:
        raise DatabaseError(f'{key} is a {kind(key)} world; this needs a {of_kind} world '
                            f'({", ".join(keys(of_kind)) or "none registered"}).')
    return key


def push_worlds() -> tuple[str, str]:
    """Push to live's two worlds (tools/publish.py, until doc 66 Phase 4): a dev world and the live world it is a
    working copy of. Atlas's world if it has an upstream, else the first registered dev world that has one."""
    candidates = [w for w in worlds() if w.get('upstream')]
    if not candidates:
        raise DatabaseError('No dev world has an upstream live world to push to.')
    try:
        atlas = atlas_world()
    except DatabaseError:
        atlas = None
    chosen = next((w for w in candidates if w['key'] == atlas), candidates[0])
    return chosen['key'], chosen['upstream']


def _write_json(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(f'.{path.name}.{os.getpid()}.tmp')
    temp.write_text(text, encoding='utf-8')
    os.replace(temp, path)


def set_local(**choice: str) -> dict[str, str]:
    """Changes this computer's choice (atlas=KEY, live=KEY), keeping the rest."""
    current = local_choice()
    current.update(choice)
    _write_json(local_file(), json.dumps(current, indent=2) + '\n')
    return current


def register(entry: dict) -> None:
    """Adds a world to the registry, one world a line as the file is written by hand."""
    entries = check_registry(worlds() + [entry])
    lines = ',\n'.join('  ' + json.dumps(w) for w in entries)
    _write_json(registry_file(), '{"worlds": [\n' + lines + '\n]}\n')
    worlds()


def __getattr__(name):
    # (WORKING was the one working world before the registry: Atlas's world now.)
    if name == 'WORKING':
        return atlas_world()
    raise AttributeError(name)


try:
    worlds()
except DatabaseError:
    pass        # (Reported by whatever asks for a world.)


# --------------------------------------------------------------------------- Settings

def read_env(path: Path = ENV_FILE) -> dict[str, str]:
    values = {}
    if path.is_file():
        for line in path.read_text(encoding='utf-8').splitlines():
            line = line.strip()
            if line and not line.startswith('#') and '=' in line:
                key, value = line.split('=', 1)
                values[key.strip()] = value.strip()
    return values


def ensure_env(path: Path = ENV_FILE) -> bool:
    """Creates the settings file with generated passwords once. Returns True if it was created."""
    if path.exists():
        return False
    lines = ['# RATW PostgreSQL settings and generated passwords. Never commit this file.',
             'RATW_DB_HOST=127.0.0.1', 'RATW_DB_PORT=5433', 'POSTGRES_USER=postgres']
    lines += [f'{key}={secrets.token_urlsafe(24)}' for key in PASSWORD_KEYS]
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'w', encoding='utf-8') as out:
        out.write('\n'.join(lines) + '\n')
    return True


def ensure_passwords(path: Path = ENV_FILE) -> list[str]:
    """Adds generated passwords for login roles added since the settings file was made. Returns the keys added."""
    have = read_env(path)
    missing = [key for key in PASSWORD_KEYS if key not in have]
    if missing:
        with path.open('a', encoding='utf-8') as out:
            out.write(''.join(f'{key}={secrets.token_urlsafe(24)}\n' for key in missing))
    return missing


def ensure_roles() -> list[str]:
    """Creates login roles that do not exist yet (roles added after the database was first set up). Needs the
    superuser from Database/.env. Returns the roles created."""
    from psycopg import sql
    ensure_passwords()
    s = settings()
    created = []
    with superuser() as su:
        for role in ROLES:
            name = f'ratw_{role}'
            if not su.execute('SELECT 1 FROM pg_roles WHERE rolname = %s', (name,)).fetchone():
                su.execute(sql.SQL('CREATE ROLE {} LOGIN PASSWORD {}').format(
                    sql.Identifier(name), sql.Literal(s[f'RATW_{role.upper()}_PASSWORD'])))
                created.append(name)
        # The Dungeon Master works on every world.
        for database in [w['database'] for w in worlds()]:
            if su.execute('SELECT 1 FROM pg_database WHERE datname = %s', (database,)).fetchone():
                su.execute(f'GRANT CONNECT ON DATABASE {database} TO ratw_dm')
    return created


def superuser(dbname: str = 'postgres'):
    """The PostgreSQL superuser from Database/.env (autocommit), for creating roles and databases."""
    try:
        import psycopg
    except ImportError as error:
        raise DatabaseError('Python package psycopg (v3) is required: `pip install "psycopg[binary]"`.') from error
    s = settings()
    if not s.get('POSTGRES_PASSWORD'):
        raise DatabaseError('No POSTGRES_PASSWORD in Database/.env. Run `python3 tools/world_db.py up` first.')
    try:
        return psycopg.connect(host=s.get('RATW_DB_HOST', '127.0.0.1'), port=s.get('RATW_DB_PORT', '5433'), dbname=dbname,
                               user=s.get('POSTGRES_USER', 'postgres'), password=s['POSTGRES_PASSWORD'], autocommit=True,
                               connect_timeout=5)
    except psycopg.OperationalError as error:
        raise DatabaseError(f'Cannot connect to PostgreSQL as its superuser: {str(error).strip()}') from error


def settings() -> dict[str, str]:
    values = read_env()
    values.update({k: v for k, v in os.environ.items() if k.startswith(('RATW_', 'POSTGRES_'))})
    return values


def conninfo(database: str, role: str) -> dict[str, str]:
    """Connection keywords for a registered world's database (`database` is its key) as one of the RATW roles."""
    entry = world(database)
    if role not in ROLES:
        raise DatabaseError(f'Unknown role {role!r}; use one of {", ".join(ROLES)}.')
    s = settings()
    password = s.get(f'RATW_{role.upper()}_PASSWORD')
    if not password:
        raise DatabaseError(f'No password for ratw_{role}. Run `python3 tools/world_db.py up` first.')
    prefix = f'RATW_{database.upper()}_'
    return {'host': s.get(prefix + 'HOST', s.get('RATW_DB_HOST', '127.0.0.1')),
            'port': s.get(prefix + 'PORT', s.get('RATW_DB_PORT', '5433')),
            'dbname': s.get(prefix + 'DBNAME', entry['database']), 'user': f'ratw_{role}', 'password': password,
            'application_name': 'ratw-tools', 'connect_timeout': '5'}


def libpq_conninfo(database: str, role: str) -> str:
    """The same settings as one libpq connection string (for the game server), values quoted."""
    quote = lambda v: "'" + str(v).replace('\\', '\\\\').replace("'", "\\'") + "'"
    return ' '.join(f'{k}={quote(v)}' for k, v in conninfo(database, role).items())


def connect(database: str, role: str, autocommit: bool = True, **info):
    """Autocommit by default: callers group statements with `conn.transaction()`."""
    try:
        import psycopg
    except ImportError as error:
        raise DatabaseError('Python package psycopg (v3) is required: `pip install "psycopg[binary]"`.') from error
    settings_ = {**conninfo(database, role), **info}
    try:
        return psycopg.connect(autocommit=autocommit, **settings_)
    except psycopg.OperationalError as error:
        raise DatabaseError(f'Cannot connect to {settings_["dbname"]} as ratw_{role}: {str(error).strip()}') from error


class Pool:
    """Connections kept open and reused, instead of one opened (and its login checked) for every request: the editor
    and Dungeon Master hosts talk to the database several times a second. `with pool.connection() as conn:` works like
    `with connect(...) as conn:`, except that the connection goes back to the pool afterwards. A connection that broke,
    or was left in a transaction, is rolled back or dropped rather than handed to the next caller; one idle for a while
    is checked before reuse, so a database restart costs no failed request."""

    def __init__(self, database: str, role: str, size: int = 4, connect_fn=None, idle_check: float = 30.0,
                 clock=time.monotonic):
        self._open = connect_fn or (lambda: connect(database, role))
        self.size, self.idle_check, self.clock = size, idle_check, clock
        self._idle: list[tuple[object, float]] = []
        self._lock = threading.Lock()

    def _usable(self, conn, idle_since: float) -> bool:
        if conn.closed or getattr(conn, 'broken', False):
            return False
        if self.clock() - idle_since < self.idle_check:
            return True
        try:
            conn.execute('SELECT 1')
            return True
        except Exception:
            return False

    def _take(self):
        while True:
            with self._lock:
                if not self._idle:
                    break
                conn, since = self._idle.pop()
            if self._usable(conn, since):
                return conn
            self._discard(conn)
        return self._open()

    @staticmethod
    def _discard(conn):
        try:
            conn.close()
        except Exception:
            pass

    def _give_back(self, conn, failed: bool):
        try:
            import psycopg
            if not conn.closed and not getattr(conn, 'broken', False):
                if not conn.autocommit:
                    (conn.rollback if failed else conn.commit)()
                elif conn.info.transaction_status != psycopg.pq.TransactionStatus.IDLE:
                    conn.rollback()
        except Exception:
            self._discard(conn)
            return
        if conn.closed or getattr(conn, 'broken', False):
            return
        with self._lock:
            if len(self._idle) < self.size:
                self._idle.append((conn, self.clock()))
                return
        self._discard(conn)

    def connection(self):
        """A connection for one `with` block. It is taken now, so a database that can't be reached fails here (as
        connect() does), not later on entering the block."""
        return _Lease(self, self._take())

    def close(self):
        with self._lock:
            idle, self._idle = self._idle, []
        for conn, _ in idle:
            self._discard(conn)


class _Lease(contextlib.AbstractContextManager):
    def __init__(self, pool: Pool, conn):
        self.pool, self.conn = pool, conn

    def __enter__(self):
        return self.conn

    def __exit__(self, kind, *_):
        self.pool._give_back(self.conn, failed=kind is not None)
        return False


def pooled(database: str, role: str, size: int = 4):
    """A `connect()` for a long-running host: each call gives a pooled connection for one `with` block."""
    return Pool(database, role, size).connection


# --------------------------------------------------------------------------- Docker

def compose(*args: str, check: bool = True) -> subprocess.CompletedProcess:
    command = ['docker', 'compose', '--project-directory', str(DATABASE_DIR), '-f', str(DATABASE_DIR / 'compose.yaml'), *args]
    return subprocess.run(command, check=check)


def wait_until_ready(timeout: float = 60) -> None:
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        try:
            superuser().close()
            return
        except DatabaseError as error:
            last = error
            time.sleep(1)
    raise DatabaseError(f'PostgreSQL did not become ready: {last}')


# --------------------------------------------------------------------------- Migrations

def migration_files() -> list[tuple[str, str, str]]:
    """(version, name, sql) for every migration, in order."""
    found = []
    for path in sorted(MIGRATIONS.glob('*.sql')):
        version, _, name = path.stem.partition('_')
        if not version.isdigit():
            raise DatabaseError(f'Migration {path.name} must start with a number, e.g. 0005_npc_memories.sql.')
        found.append((version, name, path.read_text(encoding='utf-8')))
    if len({v for v, _, _ in found}) != len(found):
        raise DatabaseError('Two migrations share a number.')
    return found


def checksum(sql: str) -> str:
    return hashlib.sha256(sql.encode('utf-8')).hexdigest()


def applied_migrations(conn) -> dict[str, str]:
    conn.execute('''CREATE TABLE IF NOT EXISTS public.schema_migrations (
        version text PRIMARY KEY, name text NOT NULL, checksum text NOT NULL,
        applied_at timestamptz NOT NULL DEFAULT now())''')
    return dict(conn.execute('SELECT version, checksum FROM public.schema_migrations').fetchall())


def migrate(conn, files=None) -> list[str]:
    """Applies pending migrations, each in its own transaction. Refuses if an applied one was edited."""
    files = migration_files() if files is None else files
    done = applied_migrations(conn)
    for version, name, sql in files:
        if version in done and done[version] != checksum(sql):
            raise DatabaseError(f'Migration {version}_{name} was changed after it was applied. '
                                'Add a new migration instead of editing an old one.')
    applied = []
    for version, name, sql in files:
        if version in done:
            continue
        with conn.transaction():
            conn.execute(sql)
            conn.execute('INSERT INTO public.schema_migrations (version, name, checksum) VALUES (%s, %s, %s)',
                         (version, name, checksum(sql)))
        applied.append(f'{version}_{name}')
    return applied


# --------------------------------------------------------------------------- Publish password

def hash_password(password: str) -> dict:
    salt = secrets.token_bytes(16)
    digest = hashlib.scrypt(password.encode('utf-8'), salt=salt, dklen=32, maxmem=64 * 1024 * 1024, **SCRYPT)
    return {'algorithm': 'scrypt', **SCRYPT, 'salt': base64.b64encode(salt).decode(),
            'hash': base64.b64encode(digest).decode()}


def password_matches(record: dict, password: str) -> bool:
    if not isinstance(record, dict) or record.get('algorithm') != 'scrypt':
        return False
    try:
        salt, expected = base64.b64decode(record['salt']), base64.b64decode(record['hash'])
        digest = hashlib.scrypt(password.encode('utf-8'), salt=salt, dklen=len(expected), maxmem=64 * 1024 * 1024,
                                n=int(record['n']), r=int(record['r']), p=int(record['p']))
    except (KeyError, ValueError, TypeError):
        return False
    return hmac.compare_digest(digest, expected)


def Jsonb(value):
    from psycopg.types.json import Jsonb as wrap
    return wrap(value)


def seed_publish_password(conn) -> bool:
    """Sets the initial publish password if none exists. Returns True if it did."""
    with conn.transaction():
        row = conn.execute("SELECT 1 FROM admin.settings WHERE key = 'publish_password'").fetchone()
        if row:
            return False
        record = {**hash_password(DEFAULT_PUBLISH_PASSWORD), 'default': True}
        conn.execute("INSERT INTO admin.settings (key, value) VALUES ('publish_password', %s)", (Jsonb(record),))
    return True


def set_publish_password(conn, password: str) -> None:
    if len(password) < 8:
        raise DatabaseError('The publish password must be at least 8 characters.')
    with conn.transaction():
        conn.execute("""INSERT INTO admin.settings (key, value, updated_at) VALUES ('publish_password', %s, now())
                        ON CONFLICT (key) DO UPDATE SET value = excluded.value, updated_at = now()""",
                     (Jsonb(hash_password(password)),))


def verify_publish_password(conn, password: str) -> bool:
    """For Push to live: checks a password against PROD's stored hash."""
    row = conn.execute("SELECT value FROM admin.settings WHERE key = 'publish_password'").fetchone()
    return bool(row) and password_matches(row[0], password)



# --------------------------------------------------------------------------- New worlds

def other_sessions(su, database: str) -> list[str]:
    """Who else is connected to a database (a copy needs it to itself): 'user (application)' each."""
    rows = su.execute('''SELECT usename, application_name FROM pg_stat_activity
                         WHERE datname = %s AND pid <> pg_backend_pid()''', (database,)).fetchall()
    return [f'{u or "?"}' + (f' ({a})' if a else '') for u, a in rows]


def _owner_info(database: str) -> dict:
    """ratw_owner on a database of the local server (where the superuser makes new ones)."""
    s = settings()
    if not s.get('RATW_OWNER_PASSWORD'):
        raise DatabaseError('No password for ratw_owner. Run `python3 tools/world_db.py up` first.')
    return {'host': s.get('RATW_DB_HOST', '127.0.0.1'), 'port': s.get('RATW_DB_PORT', '5433'), 'dbname': database,
            'user': 'ratw_owner', 'password': s['RATW_OWNER_PASSWORD'], 'application_name': 'ratw-tools', 'connect_timeout': '5'}


def new_world(key: str, name: str, of_kind: str = 'dev', upstream_key: str | None = None, source: str | None = None,
              without_map: bool = False, database: str | None = None, placements: Path | None = None, log=print) -> dict:
    """Makes a world: its database (owned by ratw_owner, the roles' CONNECT granted), either empty with every migration
    applied or a copy of `source`'s (refused while anyone else is connected to it; `without_map` then takes the map
    away), and registers it. Returns the registry entry."""
    from psycopg import sql
    database = database or f'ratw_{key}'
    entry = {'key': key, 'name': name, 'database': database, 'kind': of_kind, **({'upstream': upstream_key} if upstream_key else {})}
    if key in keys():
        raise DatabaseError(f'There is already a world called {key}.')
    check_registry(worlds() + [entry])
    if without_map and not source:
        raise DatabaseError('--without-map takes the map away from a copy: name the world to copy with --from.')
    source_db = world(source)['database'] if source else None
    with superuser() as su:
        if su.execute('SELECT 1 FROM pg_database WHERE datname = %s', (database,)).fetchone():
            raise DatabaseError(f'A database called {database} already exists; choose another key (or drop it first).')
        if source_db:
            busy = other_sessions(su, source_db)
            if busy:
                raise DatabaseError(f'{source_db} has {len(busy)} other session(s) ({", ".join(sorted(set(busy))[:6])}); '
                                    'a copy needs it to itself. Stop its servers, Atlas and the DM first.')
        import psycopg
        try:
            if source_db:
                su.execute(sql.SQL('CREATE DATABASE {} TEMPLATE {} OWNER ratw_owner').format(
                    sql.Identifier(database), sql.Identifier(source_db)))
            else:
                su.execute(sql.SQL('CREATE DATABASE {} OWNER ratw_owner').format(sql.Identifier(database)))
        except psycopg.Error as error:
            raise DatabaseError(f'Cannot create {database}: {str(error).strip()}') from error
        log(f'Created {database}' + (f' as a copy of {source_db}.' if source_db else '.'))
        try:
            su.execute(sql.SQL('REVOKE ALL ON DATABASE {} FROM PUBLIC').format(sql.Identifier(database)))
            su.execute(sql.SQL('GRANT CONNECT ON DATABASE {} TO {}').format(
                sql.Identifier(database), sql.SQL(', ').join(sql.Identifier(f'ratw_{r}') for r in CONNECT_ROLES[of_kind])))
            with psycopg.connect(autocommit=True, **_owner_info(database)) as conn:
                if not source_db:
                    applied = migrate(conn)
                    log(f'{database}: applied {len(applied)} migrations.')
                if of_kind == 'live' and seed_publish_password(conn):
                    log(f'{database}: publish password set to "{DEFAULT_PUBLISH_PASSWORD}".')
                if without_map:
                    took = take_map_away(conn, key, placements)
                    log(f'{database}: the map taken away ({took["cells"]} cells); what stood on it saved in '
                        f'{took["saved"]}.')
            register(entry)
        except BaseException as error:
            su.execute(sql.SQL('DROP DATABASE IF EXISTS {} WITH (FORCE)').format(sql.Identifier(database)))
            log(f'Dropped {database} again.')
            if isinstance(error, psycopg.Error):
                raise DatabaseError(f'Making {key} failed, nothing kept: {str(error).strip()}') from error
            raise
    log(f'Registered {key} ("{name}", {of_kind}' + (f', upstream {upstream_key}' if upstream_key else '') + ').')
    return entry


def _rows(conn, query: str, params=()) -> list:
    return conn.execute(f'SELECT coalesce(json_agg(row_to_json(t)), \'[]\') FROM ({query}) t', params).fetchone()[0]


def _nullable(conn, table: str, column: str) -> bool:
    schema, name = table.split('.')
    row = conn.execute('''SELECT is_nullable FROM information_schema.columns
                          WHERE table_schema = %s AND table_name = %s AND column_name = %s''', (schema, name, column)).fetchone()
    return bool(row) and row[0] == 'YES'


HOLDING_GLYPHS = '#' * 8 + '#......#' * 4 + '#' * 8


def take_map_away(conn, key: str, saved: Path | None = None) -> dict:
    """What was done by hand for Ridgemere 2.0 (2026-10-30), in one transaction: the map's cells, its terrain and its
    builds go, and with them what stood on them (doors to cells, patrol posts, faction claims, resources, places, NPC
    areas on cells and the spawn rules in them, the world's spawn); the people, their talk and relations and the buildings stay.
    An NPC (or profession slot) whose home, work or evening place was a cell has it unset where the schema allows
    (doc 66 Phase 2, migration 0052), else points at a holding room `<key>_unplaced`; an NPC that wandered an NPC area
    on a cell wanders nowhere. The game's save, event log and map memories and the economy's state go too. What pointed at the cells is
    saved first to artifacts/<key>/old-map-placements.json (or `saved`)."""
    saved = saved or ARTIFACTS / key / 'old-map-placements.json'
    with conn.transaction():
        row = conn.execute('SELECT id, spawn_area, spawn_x, spawn_y FROM world.worlds').fetchall()
        if len(row) != 1:
            raise DatabaseError(f'Expected one world in the copy, found {len(row)}.')
        wid, spawn_area, spawn_x, spawn_y = row[0]
        cells = [r[0] for r in conn.execute("SELECT id FROM world.areas WHERE world_id = %s AND kind = 'cell' ORDER BY id",
                                             (wid,)).fetchall()]
        on = 'world_id = %s AND {} = ANY(%s)'
        people = ('home_area', 'work_area', 'evening_area')
        # NPC areas (wander areas, spawn rules' areas) on cells: their own ids, which NPCs and spawn rules point at.
        wanders = [r[0] for r in conn.execute('SELECT id FROM live.npc_areas WHERE ' + on.format('area'), (wid, cells)).fetchall()]
        placements = {
            'note': f'{key} ({time.strftime("%Y-%m-%d")}): what pointed at the map cells before they were taken away '
                    '(tools/world_db.py new --without-map).',
            'cells': _rows(conn, 'SELECT id, name, x, y, width, height, region FROM world.cells WHERE world_id = %s ORDER BY id', (wid,)),
            'npcs': _rows(conn, f'''SELECT id, home_area, work_area, evening_area, wander_area FROM live.npcs WHERE world_id = %s
                                    AND (home_area = ANY(%s) OR work_area = ANY(%s) OR evening_area = ANY(%s) OR wander_area = ANY(%s))
                                    ORDER BY id''', (wid, cells, cells, cells, wanders)),
            'profession_slots': _rows(conn, '''SELECT id, home_area, work_area, evening_area FROM live.profession_slots
                                               WHERE world_id = %s AND (home_area = ANY(%s) OR work_area = ANY(%s)
                                               OR evening_area = ANY(%s)) ORDER BY id''', (wid, cells, cells, cells)),
            'doors': _rows(conn, '''SELECT * FROM world.links WHERE world_id = %s AND (a_area = ANY(%s) OR b_area = ANY(%s))
                                    ORDER BY id''', (wid, cells, cells)),
            'patrol_posts': _rows(conn, 'SELECT route_id AS route, seq, area, x, y FROM live.patrol_posts WHERE '
                                  + on.format('area') + ' ORDER BY route_id, seq', (wid, cells)),
            'faction_claims': _rows(conn, 'SELECT area, faction_id AS faction, tiles FROM live.faction_claims WHERE '
                                    + on.format('area') + ' ORDER BY area, faction_id', (wid, cells)),
            'npc_areas': _rows(conn, 'SELECT * FROM live.npc_areas WHERE ' + on.format('area') + ' ORDER BY id', (wid, cells)),
            'spawn_rules': _rows(conn, 'SELECT * FROM live.spawns WHERE ' + on.format('area_id') + ' ORDER BY id', (wid, wanders)),
            'places': _rows(conn, 'SELECT * FROM world.places WHERE ' + on.format('area') + ' ORDER BY id', (wid, cells)),
            'resources': _rows(conn, 'SELECT * FROM world.resources WHERE ' + on.format('area') + ' ORDER BY id', (wid, cells)),
            'spawn': [spawn_area, spawn_x, spawn_y],
        }
        holding = f'{key}_unplaced'
        unset = {table: _nullable(conn, table, 'home_area') for table in ('live.npcs', 'live.profession_slots')}
        held = []
        for table, nullable in unset.items():
            if not nullable and not held and conn.execute(
                    f'SELECT 1 FROM {table} WHERE world_id = %s AND (home_area = ANY(%s) OR work_area = ANY(%s) '
                    'OR evening_area = ANY(%s)) LIMIT 1', (wid, cells, cells, cells)).fetchone():
                conn.execute("INSERT INTO world.areas (world_id, id, kind) VALUES (%s, %s, 'interior')", (wid, holding))
                conn.execute("INSERT INTO world.interiors (world_id, id, name, width, height, glyphs) VALUES (%s, %s, 'Unplaced', 8, 6, %s)",
                             (wid, holding, HOLDING_GLYPHS))
                held.append(holding)
            for column in people:
                conn.execute(f'UPDATE {table} SET {column} = %s WHERE ' + on.format(column),
                             (None if nullable else holding, wid, cells))
        conn.execute('UPDATE live.npcs SET wander_area = NULL WHERE ' + on.format('wander_area'), (wid, wanders))
        # (Spawn rules in NPC areas on cells go with their areas.)
        for table, column in (('live.patrol_posts', 'area'), ('live.faction_claims', 'area'), ('world.resources', 'area'),
                              ('world.places', 'area'), ('live.npc_areas', 'area')):
            conn.execute(f'DELETE FROM {table} WHERE ' + on.format(column), (wid, cells))
        conn.execute('UPDATE world.worlds SET spawn_area = NULL, spawn_x = NULL, spawn_y = NULL, revision = revision + 1, '
                     'updated_at = now() WHERE id = %s', (wid,))
        conn.execute('DELETE FROM world.build_cells WHERE build_id IN (SELECT id FROM world.builds WHERE world_id = %s)', (wid,))
        for table in ('world.terrain_chunks', 'world.builds'):
            conn.execute(f'DELETE FROM {table} WHERE world_id = %s', (wid,))
        conn.execute("DELETE FROM world.areas WHERE world_id = %s AND kind = 'cell'", (wid,))   # (Cells, links, minimaps.)
        for table in ('game.checkpoints', 'game.events', 'game.map_memories', 'live.economy'):
            conn.execute(f'DELETE FROM {table} WHERE world_id = %s', (wid,))
        if held:
            placements['note'] += f' Home, work and evening places on cells now point at the holding room {holding}.'
        saved.parent.mkdir(parents=True, exist_ok=True)
        saved.write_text(json.dumps(placements, indent=1) + '\n', encoding='utf-8')
    shown = saved.relative_to(ROOT) if ROOT in saved.parents else saved
    return {'cells': len(cells), 'npcs': len(placements['npcs']), 'holding': held[0] if held else None, 'saved': str(shown)}


# --------------------------------------------------------------------------- Commands

def cmd_up(args) -> int:
    if ensure_env():
        print(f'Created {ENV_FILE.relative_to(ROOT)} with generated passwords (owner-only permissions).')
    compose('up', '-d')
    wait_until_ready()
    print('PostgreSQL is ready. Next: python3 tools/world_db.py migrate')
    return 0


def cmd_down(args) -> int:
    compose('down')
    return 0


def cmd_list(args) -> int:
    choice = local_choice()
    rows = [(w['key'], w['name'], w['kind'], w.get('upstream', ''), w['database'],
             ' '.join(f'[{which}]' for which in ('atlas', 'live') if choice[which] == w['key'])) for w in worlds()]
    widths = [max(len(str(r[i])) for r in rows + [('KEY', 'NAME', 'KIND', 'UPSTREAM', 'DATABASE', '')]) for i in range(5)]
    for r in [('KEY', 'NAME', 'KIND', 'UPSTREAM', 'DATABASE', '')] + rows:
        print('  '.join(str(v).ljust(widths[i]) for i, v in enumerate(r[:5])) + ('  ' + r[5] if r[5] else ''))
    for which in ('atlas', 'live'):
        if choice[which] not in keys():
            print(f'(The {which} choice, {choice[which]}, is not a registered world.)')
    return 0


def cmd_use(args) -> int:
    key = resolve(args.key, 'dev')
    set_local(atlas=key)
    print(f'Atlas\'s world is now {key} ("{world(key)["name"]}"). Atlas, the builds, local and scratch servers and the '
          'talk tools use it unless told otherwise; a running Atlas picks it up when restarted.')
    return 0


def cmd_serve_world(args) -> int:
    key = resolve(args.key, 'live')
    print('Warning: switching the live world is rarely wise. Players join whatever world the live server runs; their '
          'characters, saves and the world they knew are in the old one. Switch only during maintenance, with the '
          'live server stopped.', file=sys.stderr)
    if not args.yes:
        print(f'Nothing changed. To make {key} the live world: python3 tools/world_db.py serve-world {key} --yes',
              file=sys.stderr)
        return 1
    before = local_choice()['live']
    set_local(live=key)
    print(f'The live world is now {key} ("{world(key)["name"]}"), was {before}. The live server starts on it next time '
          '(bash tools/live.sh server).')
    return 0


def cmd_new(args) -> int:
    new_world(args.key, args.name, args.kind, args.upstream, args.source, args.without_map)
    return 0


def cmd_resolve(args) -> int:
    key = resolve(args.key, args.kind)
    print(f'{key} {kind(key)}' if args.with_kind else key)
    return 0


def cmd_migrate(args) -> int:
    for role in ensure_roles():
        print(f'Created login role {role}.')
    for database in (keys() if args.db == 'all' else [resolve(args.db)]):   # (Every world: their schemas never drift.)
        name = world(database)['database']
        with connect(database, 'owner') as conn:
            applied = migrate(conn)
            print(f'{name}: ' + (f'applied {", ".join(applied)}' if applied else 'up to date'))
            if kind(database) == 'live' and seed_publish_password(conn):
                print(f'{name}: publish password set to "{DEFAULT_PUBLISH_PASSWORD}". '
                      'Change it with: python3 tools/world_db.py set-publish-password')
    return 0


def cmd_status(args) -> int:
    files = migration_files()
    choice = local_choice()
    for database in keys():
        w = world(database)
        label = f'{w["database"]} ({database}, a {w["kind"]} world' + ''.join(
            f', {what}' for which, what in (('atlas', "Atlas's"), ('live', 'the live one')) if choice[which] == database) + ')'
        try:
            conn = connect(database, 'owner')
        except DatabaseError as error:
            print(f'{label}: {error}')
            continue
        with conn:
            done = applied_migrations(conn)
            pending = [f'{v}_{n}' for v, n, _ in files if v not in done]
            print(f'{label}: {len(done)} migrations applied' + (f', pending: {", ".join(pending)}' if pending else ''))
            if 'world' in {r[0] for r in conn.execute('SELECT nspname FROM pg_namespace').fetchall()}:
                for table in ('world.worlds', 'world.areas', 'world.terrain_chunks', 'live.npcs', 'live.characters', 'game.checkpoints'):
                    count = conn.execute(f'SELECT count(*) FROM {table}').fetchone()[0]
                    print(f'  {table:<22} {count}')
            if w['kind'] == 'live' and done:
                row = conn.execute("SELECT value FROM admin.settings WHERE key = 'publish_password'").fetchone()
                if not row:
                    print('  publish password: not set (run migrate)')
                elif row[0].get('default'):
                    print(f'  publish password: still the default "{DEFAULT_PUBLISH_PASSWORD}"')
                else:
                    print('  publish password: set')
    return 0


def cmd_set_publish_password(args) -> int:
    target = resolve(args.db, 'live') if args.db else push_worlds()[1]
    first = getpass.getpass(f'New publish password for {target}: ')
    if getpass.getpass('Repeat it: ') != first:
        print('The two passwords differ; nothing changed.', file=sys.stderr)
        return 1
    with connect(target, 'owner') as conn:
        set_publish_password(conn, first)
    print('Publish password changed.')
    return 0


def cmd_conninfo(args) -> int:
    print(libpq_conninfo(resolve(args.database), args.role))
    return 0


def cmd_psql(args) -> int:
    info = conninfo(resolve(args.database), args.role)
    env = {**os.environ, 'PGPASSWORD': info['password']}
    return subprocess.call(['psql', '-h', info['host'], '-p', info['port'], '-U', info['user'], info['dbname']], env=env)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('up', help='start PostgreSQL in Docker').set_defaults(run=cmd_up)
    sub.add_parser('down', help='stop PostgreSQL (data is kept)').set_defaults(run=cmd_down)
    sub.add_parser('list', help='the registered worlds').set_defaults(run=cmd_list)
    u = sub.add_parser('use', help="choose Atlas's world (a dev world)")
    u.add_argument('key')
    u.set_defaults(run=cmd_use)
    s = sub.add_parser('serve-world', help="choose the live server's world (a live world; rarely wise)")
    s.add_argument('key')
    s.add_argument('--yes', action='store_true', help='really switch it')
    s.set_defaults(run=cmd_serve_world)
    n = sub.add_parser('new', help='a new world: an empty database, or a copy of another world')
    n.add_argument('key', help='its key: lowercase letters, digits and _ (its database is ratw_KEY)')
    n.add_argument('--name', required=True, help='its name, as Atlas and the DM show it')
    n.add_argument('--kind', choices=KINDS, default='dev')
    n.add_argument('--upstream', help='a dev world: the live world it is a working copy of')
    n.add_argument('--from', dest='source', help="copy this world's database (nobody else may be connected to it)")
    n.add_argument('--without-map', action='store_true', help='with --from: take the map away, keep people and buildings')
    n.set_defaults(run=cmd_new)
    r = sub.add_parser('resolve', help='print a world key: atlas, live (the local choices) or a registered key')
    r.add_argument('key')
    r.add_argument('--kind', choices=KINDS, help='refuse a world of the other kind')
    r.add_argument('--with-kind', action='store_true', help="print the world's kind after its key")
    r.set_defaults(run=cmd_resolve)
    m = sub.add_parser('migrate', help='apply pending migrations')
    m.add_argument('--db', default='all', help='a world key, or all (the default)')
    m.set_defaults(run=cmd_migrate)
    sub.add_parser('status', help='show migrations and row counts').set_defaults(run=cmd_status)
    pw = sub.add_parser('set-publish-password', help='change the Push to live password')
    pw.add_argument('--db', help="the live world (default Push to live's)")
    pw.set_defaults(run=cmd_set_publish_password)
    c = sub.add_parser('conninfo', help='print a libpq connection string (contains the password)')
    c.add_argument('database', help='a world key (atlas, live: the local choices)')
    c.add_argument('--role', choices=ROLES, default='game')
    c.set_defaults(run=cmd_conninfo)
    p = sub.add_parser('psql', help='open psql on a world')
    p.add_argument('database', help='a world key (atlas, live: the local choices)')
    p.add_argument('--role', choices=ROLES, default='owner')
    p.set_defaults(run=cmd_psql)
    args = parser.parse_args(argv)
    try:
        return args.run(args)
    except DatabaseError as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
