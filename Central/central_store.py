"""Central's database (Docs/Design/77-central-accounts-and-hosted-worlds.md, 4.1): its own Postgres database, never a
world's, reached the way every RATW tool reaches Postgres (tools/world_db.py: Database/.env for the server and the
passwords, psycopg 3, world_db.Pool, world_db.migrate). Central is not a world, so it is not in Database/worlds.json.

  RATW_CENTRAL_DBNAME  the database (default ratw_central)
  RATW_CENTRAL_ROLE    the RATW role it connects as (default owner: on this laptop, for development; the box gets a
                       role of its own in Phase 7)
  RATW_CENTRAL_HOST/RATW_CENTRAL_PORT  another server (default RATW_DB_HOST/RATW_DB_PORT, 127.0.0.1:5433)

Every time is written from central's own clock (an epoch float, turned into timestamptz here), so tests can move it.
"""
from __future__ import annotations

import datetime as dt
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'tools'))
import world_db  # noqa: E402

MIGRATIONS = HERE / 'migrations'
WORLD_DATABASES = frozenset({'ratw_prod', 'ratw_ridgemere2', 'ratw_dev'})


class Conflict(Exception):
    """A unique name or username already taken (or reserved)."""


def ts(epoch: float) -> dt.datetime:
    return dt.datetime.fromtimestamp(epoch, dt.timezone.utc)


def epoch(value: dt.datetime | None) -> float | None:
    return None if value is None else value.timestamp()


def conninfo(dbname: str | None = None, role: str | None = None) -> dict:
    s = world_db.settings()
    dbname = dbname or s.get('RATW_CENTRAL_DBNAME', 'ratw_central')
    role = role or s.get('RATW_CENTRAL_ROLE', 'owner')
    if dbname in WORLD_DATABASES or dbname in {w['database'] for w in world_db.worlds()}:
        raise world_db.DatabaseError(f'{dbname} is a world database; central keeps its own (ratw_central).')
    password = s.get(f'RATW_{role.upper()}_PASSWORD')
    if not password:
        raise world_db.DatabaseError(f'No password for ratw_{role} in Database/.env (in a git worktree, which has no '
                                     ".env of its own, export the main checkout's: set -a; . MAIN/Database/.env; set +a).")
    return {'host': s.get('RATW_CENTRAL_HOST', s.get('RATW_DB_HOST', '127.0.0.1')),
            'port': s.get('RATW_CENTRAL_PORT', s.get('RATW_DB_PORT', '5433')), 'dbname': dbname,
            'user': f'ratw_{role}', 'password': password, 'application_name': 'ratw-central', 'connect_timeout': '5'}


def migration_files() -> list[tuple[str, str, str]]:
    found = []
    for path in sorted(MIGRATIONS.glob('*.sql')):
        version, _, name = path.stem.partition('_')
        found.append((f'central-{version}', name, path.read_text(encoding='utf-8')))
    return found


def create_database(dbname: str, owner: str = 'ratw_owner') -> bool:
    """Creates central's database with the Postgres superuser from Database/.env (no role is made or changed). False if
    it exists already."""
    from psycopg import sql
    conninfo(dbname)                                  # (Refuses a world's database.)
    with world_db.superuser() as su:
        if su.execute('SELECT 1 FROM pg_database WHERE datname = %s', (dbname,)).fetchone():
            return False
        su.execute(sql.SQL('CREATE DATABASE {} OWNER {}').format(sql.Identifier(dbname), sql.Identifier(owner)))
        su.execute(sql.SQL('REVOKE ALL ON DATABASE {} FROM PUBLIC').format(sql.Identifier(dbname)))
    return True


def drop_database(dbname: str) -> None:
    from psycopg import sql
    conninfo(dbname)
    with world_db.superuser() as su:
        su.execute(sql.SQL('DROP DATABASE IF EXISTS {} WITH (FORCE)').format(sql.Identifier(dbname)))


ACCOUNT = 'id, username, password, created_at, banned, host, admin'
WOLF = 'id, account_id, name, name_key, sex, age, appearance, family, profile, revision, created_at, updated_at'
SESSION = 'id, account_id, access_expires, refresh_expires, remember, created_at, refreshed_at'


def _account(row) -> dict | None:
    if row is None:
        return None
    keys = ACCOUNT.split(', ')
    out = dict(zip(keys, row))
    out['created_at'] = epoch(out['created_at'])
    return out


def _wolf(row) -> dict | None:
    if row is None:
        return None
    out = dict(zip(WOLF.split(', '), row))
    out['created_at'], out['updated_at'] = epoch(out['created_at']), epoch(out['updated_at'])
    return out


def _session(row) -> dict | None:
    if row is None:
        return None
    out = dict(zip(SESSION.split(', '), row))
    for key in ('access_expires', 'refresh_expires', 'created_at', 'refreshed_at'):
        out[key] = epoch(out[key])
    return out


class Store:
    def __init__(self, dbname: str | None = None, role: str | None = None, pool_size: int = 8):
        self.info = conninfo(dbname, role)
        import psycopg
        self._pool = world_db.Pool(self.info['dbname'], role or 'owner', size=pool_size,
                                   connect_fn=lambda: psycopg.connect(autocommit=True, **self.info))

    def connection(self):
        return self._pool.connection()

    def close(self):
        self._pool.close()

    def migrate(self) -> list[str]:
        with self.connection() as conn:
            return world_db.migrate(conn, migration_files())

    # ---------------------------------------------------------------- Accounts

    def reserved(self, conn, kind: str, key: str, now: float) -> bool:
        return conn.execute('SELECT 1 FROM central.reserved_names WHERE kind = %s AND name_key = %s AND until > %s',
                            (kind, key, ts(now))).fetchone() is not None

    def create_account(self, account_id: str, username: str, password: dict, now: float) -> None:
        import psycopg
        with self.connection() as conn, conn.transaction():
            if self.reserved(conn, 'username', username, now):
                raise Conflict('username')
            try:
                conn.execute('INSERT INTO central.accounts (id, username, password, created_at, age_checked_at) '
                             'VALUES (%s, %s, %s, %s, %s)',
                             (account_id, username, world_db.Jsonb(password), ts(now), ts(now)))
            except psycopg.errors.UniqueViolation as error:
                raise Conflict('username') from error

    def account_by_username(self, username: str) -> dict | None:
        with self.connection() as conn:
            return _account(conn.execute(f'SELECT {ACCOUNT} FROM central.accounts WHERE username = %s',
                                         (username,)).fetchone())

    def account(self, account_id: str) -> dict | None:
        with self.connection() as conn:
            return _account(conn.execute(f'SELECT {ACCOUNT} FROM central.accounts WHERE id = %s',
                                         (account_id,)).fetchone())

    def set_flags(self, account_id: str, **flags: bool) -> bool:
        allowed = {k: bool(v) for k, v in flags.items() if k in ('banned', 'host', 'admin')}
        if not allowed:
            return False
        with self.connection() as conn:
            sets = ', '.join(f'{k} = %s' for k in allowed)
            return conn.execute(f'UPDATE central.accounts SET {sets} WHERE id = %s',
                                (*allowed.values(), account_id)).rowcount == 1

    def set_password(self, account_id: str, password: dict) -> bool:
        with self.connection() as conn:
            return conn.execute('UPDATE central.accounts SET password = %s WHERE id = %s',
                                (world_db.Jsonb(password), account_id)).rowcount == 1

    def delete_account(self, account_id: str, now: float, reserve_until: float) -> dict | None:
        """Removes the account, its sessions and wolves; reserves its username and wolves' names until then; lists
        the wolves for the worlds to forget. Returns {"username", "wolves": [ids]} or None if there was none."""
        with self.connection() as conn, conn.transaction():
            row = conn.execute('SELECT username FROM central.accounts WHERE id = %s FOR UPDATE',
                               (account_id,)).fetchone()
            if row is None:
                return None
            wolves = conn.execute('SELECT id, name_key FROM central.wolves WHERE account_id = %s',
                                  (account_id,)).fetchall()
            upsert = ('INSERT INTO central.reserved_names (kind, name_key, until) VALUES (%s, %s, %s) '
                      'ON CONFLICT (kind, name_key) DO UPDATE SET until = GREATEST(central.reserved_names.until, '
                      'EXCLUDED.until)')
            conn.execute(upsert, ('username', row[0], ts(reserve_until)))
            for wolf_id, key in wolves:
                conn.execute(upsert, ('wolf', key, ts(reserve_until)))
                conn.execute('INSERT INTO central.deleted_wolves (wolf_id, deleted_at) VALUES (%s, %s) '
                             'ON CONFLICT DO NOTHING', (wolf_id, ts(now)))
            conn.execute('DELETE FROM central.accounts WHERE id = %s', (account_id,))
            return {'username': row[0], 'wolves': [w for w, _ in wolves]}

    def delete_wolf(self, account_id: str, wolf_id: str, now: float, reserve_until: float) -> bool:
        """Removes one wolf of the account (the user, 2026-11-07: a wolf may go without its account); reserves its name
        until then and lists it for the worlds to forget. False if the account has no such wolf."""
        with self.connection() as conn, conn.transaction():
            row = conn.execute('SELECT name_key FROM central.wolves WHERE id = %s AND account_id = %s FOR UPDATE',
                               (wolf_id, account_id)).fetchone()
            if row is None:
                return False
            conn.execute('INSERT INTO central.reserved_names (kind, name_key, until) VALUES (%s, %s, %s) '
                         'ON CONFLICT (kind, name_key) DO UPDATE SET until = GREATEST(central.reserved_names.until, '
                         'EXCLUDED.until)', ('wolf', row[0], ts(reserve_until)))
            conn.execute('INSERT INTO central.deleted_wolves (wolf_id, deleted_at) VALUES (%s, %s) ON CONFLICT DO NOTHING',
                         (wolf_id, ts(now)))
            conn.execute('DELETE FROM central.wolves WHERE id = %s', (wolf_id,))
            return True

    def deleted_wolves(self, since: float = 0) -> list[str]:
        with self.connection() as conn:
            return [r[0] for r in conn.execute('SELECT wolf_id FROM central.deleted_wolves WHERE deleted_at >= %s '
                                               'ORDER BY deleted_at, wolf_id', (ts(since),)).fetchall()]

    # ---------------------------------------------------------------- Sessions

    def create_session(self, session_id: str, account_id: str, access_hash: str, access_expires: float,
                       refresh_hash: str, refresh_expires: float, remember: bool, now: float) -> None:
        with self.connection() as conn:
            conn.execute('INSERT INTO central.sessions (id, account_id, access_hash, access_expires, refresh_hash, '
                         'refresh_expires, remember, created_at, refreshed_at) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s)',
                         (session_id, account_id, access_hash, ts(access_expires), refresh_hash,
                          ts(refresh_expires), remember, ts(now), ts(now)))

    def session_by_access(self, access_hash: str) -> dict | None:
        with self.connection() as conn:
            return _session(conn.execute(f'SELECT {SESSION} FROM central.sessions WHERE access_hash = %s',
                                         (access_hash,)).fetchone())

    def refresh(self, refresh_hash: str, now: float, access_hash: str, access_expires: float, new_refresh: str,
                expires_for) -> tuple[str, dict | None]:
        """Swaps a session's tokens for new ones. ("ok", session) on success; ("reused", None) if the token was
        already spent (the session is ended); ("expired" | "unknown", None) otherwise."""
        with self.connection() as conn, conn.transaction():
            row = conn.execute(f'SELECT {SESSION} FROM central.sessions WHERE refresh_hash = %s FOR UPDATE',
                               (refresh_hash,)).fetchone()
            if row is None:
                spent = conn.execute('SELECT session_id FROM central.spent_refresh WHERE refresh_hash = %s',
                                     (refresh_hash,)).fetchone()
                if spent is None:
                    return 'unknown', None
                conn.execute('DELETE FROM central.sessions WHERE id = %s', (spent[0],))
                return 'reused', None
            session = _session(row)
            if session['refresh_expires'] <= now:
                conn.execute('DELETE FROM central.sessions WHERE id = %s', (session['id'],))
                return 'expired', None
            expires = expires_for(session)          # (The new refresh token's end: the session's own rule.)
            conn.execute('INSERT INTO central.spent_refresh (refresh_hash, session_id, expires_at) VALUES (%s,%s,%s)',
                         (refresh_hash, session['id'], ts(session['refresh_expires'])))
            conn.execute('UPDATE central.sessions SET access_hash = %s, access_expires = %s, refresh_hash = %s, '
                         'refresh_expires = %s, refreshed_at = %s WHERE id = %s',
                         (access_hash, ts(access_expires), new_refresh, ts(expires), ts(now), session['id']))
            session.update(access_expires=access_expires, refresh_expires=expires, refreshed_at=now)
            return 'ok', session

    def end_session(self, session_id: str) -> None:
        with self.connection() as conn:
            conn.execute('DELETE FROM central.sessions WHERE id = %s', (session_id,))

    def end_sessions(self, account_id: str) -> int:
        with self.connection() as conn:
            return conn.execute('DELETE FROM central.sessions WHERE account_id = %s', (account_id,)).rowcount

    # ---------------------------------------------------------------- Wolves

    def wolves(self, account_id: str) -> list[dict]:
        with self.connection() as conn:
            return [_wolf(r) for r in conn.execute(f'SELECT {WOLF} FROM central.wolves WHERE account_id = %s '
                                                   'ORDER BY created_at, id', (account_id,)).fetchall()]

    def wolf(self, wolf_id: str) -> dict | None:
        with self.connection() as conn:
            return _wolf(conn.execute(f'SELECT {WOLF} FROM central.wolves WHERE id = %s', (wolf_id,)).fetchone())

    def name_free(self, name_key: str, now: float) -> bool:
        with self.connection() as conn:
            return not self.reserved(conn, 'wolf', name_key, now) and conn.execute(
                'SELECT 1 FROM central.wolves WHERE name_key = %s', (name_key,)).fetchone() is None

    def create_wolf(self, wolf: dict, limit: int, now: float) -> str | None:
        """Adds the wolf unless the account has `limit` already ("full") or the name is taken or reserved
        (Conflict). None on success."""
        import psycopg
        with self.connection() as conn, conn.transaction():
            conn.execute('SELECT 1 FROM central.accounts WHERE id = %s FOR UPDATE', (wolf['account_id'],))
            have = conn.execute('SELECT count(*) FROM central.wolves WHERE account_id = %s',
                                (wolf['account_id'],)).fetchone()[0]
            if have >= limit:
                return 'full'
            if self.reserved(conn, 'wolf', wolf['name_key'], now):
                raise Conflict('name')
            try:
                conn.execute('INSERT INTO central.wolves (id, account_id, name, name_key, sex, age, appearance, '
                             'family, profile, revision, created_at, updated_at) '
                             'VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,1,%s,%s)',
                             (wolf['id'], wolf['account_id'], wolf['name'], wolf['name_key'], wolf['sex'],
                              wolf['age'], world_db.Jsonb(wolf['appearance']), wolf['family'],
                              world_db.Jsonb(wolf['profile']), ts(now), ts(now)))
            except psycopg.errors.UniqueViolation as error:
                raise Conflict('name') from error
        return None

    def update_wolf(self, wolf_id: str, revision: int, now: float, appearance=None, profile=None) -> dict | None:
        """Changes the look and/or profile if the wolf is still at `revision`; the wolf afterwards, or None if it
        changed meanwhile (or is gone)."""
        sets, values = ['revision = revision + 1', 'updated_at = %s'], [ts(now)]
        if appearance is not None:
            sets.append('appearance = %s')
            values.append(world_db.Jsonb(appearance))
        if profile is not None:
            sets.append('profile = %s')
            values.append(world_db.Jsonb(profile))
        with self.connection() as conn:
            row = conn.execute(f'UPDATE central.wolves SET {", ".join(sets)} WHERE id = %s AND revision = %s '
                               f'RETURNING {WOLF}', (*values, wolf_id, revision)).fetchone()
            return _wolf(row)

    # ---------------------------------------------------------------- Worlds (a stub until Phase 4) and tickets

    def add_world(self, world_id: str, name: str, kind: str, now: float, host_account: str | None = None,
                  address: str | None = None, port: int | None = None, listing: dict | None = None) -> None:
        with self.connection() as conn:
            conn.execute('INSERT INTO central.worlds (id, name, kind, host_account, created_at, address, port, listing) '
                         'VALUES (%s, %s, %s, %s, %s, %s, %s, %s)',
                         (world_id, name, kind, host_account, ts(now), address, port, world_db.Jsonb(listing or {})))

    def set_world(self, world_id: str, address: str | None = None, port: int | None = None,
                  listing: dict | None = None, delisted: bool | None = None) -> bool:
        """Changes a world's address, listing fields (merged) or delisting, by hand (the heartbeat does it in Phase 4)."""
        sets, values = [], []
        if address is not None:
            sets += ['address = %s', 'port = %s']
            values += [address, port]
        if listing:
            sets.append('listing = listing || %s')
            values.append(world_db.Jsonb(listing))
        if delisted is not None:
            sets.append('delisted = %s')
            values.append(delisted)
        if not sets:
            return self.world(world_id) is not None
        with self.connection() as conn:
            return conn.execute(f'UPDATE central.worlds SET {", ".join(sets)} WHERE id = %s',
                                (*values, world_id)).rowcount == 1

    WORLD_FIELDS = ('id', 'name', 'kind', 'host_account', 'delisted', 'address', 'port', 'listing')

    def world(self, world_id: str) -> dict | None:
        with self.connection() as conn:
            row = conn.execute(f'SELECT {", ".join(self.WORLD_FIELDS)} FROM central.worlds WHERE id = %s',
                               (world_id,)).fetchone()
            return None if row is None else dict(zip(self.WORLD_FIELDS, row))

    def worlds(self) -> list[dict]:
        with self.connection() as conn:
            rows = conn.execute(f'SELECT {", ".join(self.WORLD_FIELDS)} FROM central.worlds '
                                'ORDER BY created_at, id').fetchall()
            return [dict(zip(self.WORLD_FIELDS, r)) for r in rows]

    def record_ticket(self, jti: str, world_id: str, wolf_id: str, account_id: str, kid: str, now: float,
                      expires: float) -> None:
        with self.connection() as conn:
            conn.execute('INSERT INTO central.tickets (jti, world_id, wolf_id, account_id, kid, issued_at, expires_at) '
                         'VALUES (%s,%s,%s,%s,%s,%s,%s)', (jti, world_id, wolf_id, account_id, kid, ts(now), ts(expires)))

    def tickets_issued(self, world_id: str, since: float) -> int:
        with self.connection() as conn:
            return conn.execute('SELECT count(*) FROM central.tickets WHERE world_id = %s AND issued_at >= %s',
                                (world_id, ts(since))).fetchone()[0]

    # ---------------------------------------------------------------- Housekeeping

    def prune(self, now: float, tickets_kept: float = 7 * 86400) -> dict:
        with self.connection() as conn, conn.transaction():
            return {
                'sessions': conn.execute('DELETE FROM central.sessions WHERE refresh_expires <= %s',
                                         (ts(now),)).rowcount,
                'spent': conn.execute('DELETE FROM central.spent_refresh WHERE expires_at <= %s', (ts(now),)).rowcount,
                'reserved': conn.execute('DELETE FROM central.reserved_names WHERE until <= %s', (ts(now),)).rowcount,
                'tickets': conn.execute('DELETE FROM central.tickets WHERE issued_at <= %s',
                                        (ts(now - tickets_kept),)).rowcount,
            }
