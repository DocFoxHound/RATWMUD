#!/usr/bin/env python3
"""Run and manage the RATW PostgreSQL databases (DEV and PROD).

  python3 tools/world_db.py up                    start PostgreSQL (first run creates Database/.env)
  python3 tools/world_db.py migrate               apply Database/migrations to DEV and PROD
  python3 tools/world_db.py status                show connection, migrations and row counts
  python3 tools/world_db.py set-publish-password  change the Push to live password (PROD)
  python3 tools/world_db.py psql dev              open psql on DEV (--role editor|publisher|game|owner)
  python3 tools/world_db.py down                  stop PostgreSQL (data is kept)

Connection settings come from Database/.env, overridden by environment
variables of the same name. RATW_DEV_HOST/RATW_DEV_PORT and
RATW_PROD_HOST/RATW_PROD_PORT point either database elsewhere (e.g. a hosted
PROD); otherwise both use RATW_DB_HOST/RATW_DB_PORT (127.0.0.1:5433).
RATW_DEV_DBNAME/RATW_PROD_DBNAME rename a database (tests use scratch ones).
"""
from __future__ import annotations

import argparse
import base64
import contextlib
import getpass
import hashlib
import hmac
import os
from pathlib import Path
import secrets
import subprocess
import sys
import threading
import time

ROOT = Path(__file__).resolve().parent.parent
DATABASE_DIR = ROOT / 'Database'
ENV_FILE = DATABASE_DIR / '.env'
MIGRATIONS = DATABASE_DIR / 'migrations'
DATABASES = {'dev': 'ratw_dev', 'prod': 'ratw_prod'}
ROLES = ('owner', 'editor', 'publisher', 'game', 'dm')
PASSWORD_KEYS = ['POSTGRES_PASSWORD'] + [f'RATW_{r.upper()}_PASSWORD' for r in ROLES]
DEFAULT_PUBLISH_PASSWORD = 'password'
SCRYPT = {'n': 2 ** 15, 'r': 8, 'p': 1}


class DatabaseError(RuntimeError):
    pass


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
    import psycopg
    from psycopg import sql
    ensure_passwords()
    s = settings()
    created = []
    with psycopg.connect(host=s.get('RATW_DB_HOST', '127.0.0.1'), port=s.get('RATW_DB_PORT', '5433'), dbname='postgres',
                         user=s.get('POSTGRES_USER', 'postgres'), password=s['POSTGRES_PASSWORD'], autocommit=True) as su:
        for role in ROLES:
            name = f'ratw_{role}'
            if not su.execute('SELECT 1 FROM pg_roles WHERE rolname = %s', (name,)).fetchone():
                su.execute(sql.SQL('CREATE ROLE {} LOGIN PASSWORD {}').format(
                    sql.Identifier(name), sql.Literal(s[f'RATW_{role.upper()}_PASSWORD'])))
                created.append(name)
        # The Dungeon Master works on both databases.
        for database in DATABASES.values():
            if su.execute('SELECT 1 FROM pg_database WHERE datname = %s', (database,)).fetchone():
                su.execute(f'GRANT CONNECT ON DATABASE {database} TO ratw_dm')
    return created


def settings() -> dict[str, str]:
    values = read_env()
    values.update({k: v for k, v in os.environ.items() if k.startswith(('RATW_', 'POSTGRES_'))})
    return values


def conninfo(database: str, role: str) -> dict[str, str]:
    """Connection keywords for one of the two databases as one of the RATW roles."""
    if database not in DATABASES:
        raise DatabaseError(f'Unknown database {database!r}; use dev or prod.')
    if role not in ROLES:
        raise DatabaseError(f'Unknown role {role!r}; use one of {", ".join(ROLES)}.')
    s = settings()
    password = s.get(f'RATW_{role.upper()}_PASSWORD')
    if not password:
        raise DatabaseError(f'No password for ratw_{role}. Run `python3 tools/world_db.py up` first.')
    prefix = f'RATW_{database.upper()}_'
    return {'host': s.get(prefix + 'HOST', s.get('RATW_DB_HOST', '127.0.0.1')),
            'port': s.get(prefix + 'PORT', s.get('RATW_DB_PORT', '5433')),
            'dbname': s.get(prefix + 'DBNAME', DATABASES[database]), 'user': f'ratw_{role}', 'password': password,
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
    try:
        return psycopg.connect(autocommit=autocommit, **{**conninfo(database, role), **info})
    except psycopg.OperationalError as error:
        raise DatabaseError(f'Cannot connect to {DATABASES[database]} as ratw_{role}: {str(error).strip()}') from error


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
            connect('prod', 'owner').close()
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


def cmd_migrate(args) -> int:
    for role in ensure_roles():
        print(f'Created login role {role}.')
    for database in (['dev', 'prod'] if args.db == 'all' else [args.db]):
        with connect(database, 'owner') as conn:
            applied = migrate(conn)
            print(f'{DATABASES[database]}: ' + (f'applied {", ".join(applied)}' if applied else 'up to date'))
            if database == 'prod' and seed_publish_password(conn):
                print(f'{DATABASES[database]}: publish password set to "{DEFAULT_PUBLISH_PASSWORD}". '
                      'Change it with: python3 tools/world_db.py set-publish-password')
    return 0


def cmd_status(args) -> int:
    files = migration_files()
    for database in DATABASES:
        try:
            conn = connect(database, 'owner')
        except DatabaseError as error:
            print(f'{DATABASES[database]}: {error}')
            continue
        with conn:
            done = applied_migrations(conn)
            pending = [f'{v}_{n}' for v, n, _ in files if v not in done]
            print(f'{DATABASES[database]}: {len(done)} migrations applied' + (f', pending: {", ".join(pending)}' if pending else ''))
            if 'world' in {r[0] for r in conn.execute('SELECT nspname FROM pg_namespace').fetchall()}:
                for table in ('world.worlds', 'world.areas', 'world.terrain_chunks', 'live.npcs', 'live.characters', 'game.checkpoints'):
                    count = conn.execute(f'SELECT count(*) FROM {table}').fetchone()[0]
                    print(f'  {table:<22} {count}')
            if database == 'prod' and done:
                row = conn.execute("SELECT value FROM admin.settings WHERE key = 'publish_password'").fetchone()
                if not row:
                    print('  publish password: not set (run migrate)')
                elif row[0].get('default'):
                    print(f'  publish password: still the default "{DEFAULT_PUBLISH_PASSWORD}"')
                else:
                    print('  publish password: set')
    return 0


def cmd_set_publish_password(args) -> int:
    first = getpass.getpass('New publish password: ')
    if getpass.getpass('Repeat it: ') != first:
        print('The two passwords differ; nothing changed.', file=sys.stderr)
        return 1
    with connect('prod', 'owner') as conn:
        set_publish_password(conn, first)
    print('Publish password changed.')
    return 0


def cmd_conninfo(args) -> int:
    print(libpq_conninfo(args.database, args.role))
    return 0


def cmd_psql(args) -> int:
    info = conninfo(args.database, args.role)
    env = {**os.environ, 'PGPASSWORD': info['password']}
    return subprocess.call(['psql', '-h', info['host'], '-p', info['port'], '-U', info['user'], info['dbname']], env=env)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('up', help='start PostgreSQL in Docker').set_defaults(run=cmd_up)
    sub.add_parser('down', help='stop PostgreSQL (data is kept)').set_defaults(run=cmd_down)
    m = sub.add_parser('migrate', help='apply pending migrations')
    m.add_argument('--db', choices=('dev', 'prod', 'all'), default='all')
    m.set_defaults(run=cmd_migrate)
    sub.add_parser('status', help='show migrations and row counts').set_defaults(run=cmd_status)
    sub.add_parser('set-publish-password', help='change the Push to live password').set_defaults(run=cmd_set_publish_password)
    c = sub.add_parser('conninfo', help='print a libpq connection string (contains the password)')
    c.add_argument('database', choices=tuple(DATABASES))
    c.add_argument('--role', choices=ROLES, default='game')
    c.set_defaults(run=cmd_conninfo)
    p = sub.add_parser('psql', help='open psql on a database')
    p.add_argument('database', choices=tuple(DATABASES))
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
