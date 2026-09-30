#!/usr/bin/env python3
"""Dungeon Master: run the living world (Docs/Design/21-dungeon-master.md).

  bash tools/dungeon-master.sh                          build the UI and serve it on http://127.0.0.1:8766
  python3 tools/dungeon_master.py accounts create-defaults   the three DM accounts (passwords written once
                                                             to Database/dm-accounts.txt, owner-only)
  python3 tools/dungeon_master.py accounts list
  python3 tools/dungeon_master.py accounts set-password USER

DM accounts live in PROD (dm.admins) and sign in to both PROD and DEV. Their
roles: viewer sees everything and changes nothing; dm runs the live world;
admin also manages DM accounts. Live changes are queued in the target
database (dm.actions); the game server applies each one within a second and
writes back what happened. Every request to change something is audited.
Local-only: the host listens on this computer.
"""
from __future__ import annotations

import argparse
import getpass
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

import chronicle as C
import http_body
import live_edit as L
import map_editor
import world_db
import world_store as S

ROOT = Path(__file__).resolve().parent.parent
DIST = ROOT / 'Editor' / 'dist'
ACCOUNTS_FILE = ROOT / 'Database' / 'dm-accounts.txt'
DEFAULT_ACCOUNTS = (('dm-admin', 'admin'), ('dm-master', 'dm'), ('dm-viewer', 'viewer'))
SESSION_HOURS = 12
MAX_FAILURES, LOCK_MINUTES = 5, 15
MAX_BODY = 64 * 1024
TARGETS = ('prod', 'dev')
# Live actions this version knows, and who may request them.
ACTIONS = {'character.kill': 'dm', 'character.resurrect': 'dm', 'npc.sync': 'dm', 'npc.kill': 'dm', 'npc.revive': 'dm',
           'layers.sync': 'dm', 'factions.sync': 'dm', 'festival.call': 'dm'}
# What else a role may do here (not live actions for the game server).
WRITES = {'story.write': 'dm'}
STORIES_PER_HOUR = 30
AI_CONFIG = ROOT / 'Saved' / 'Config' / 'RATWNPCAI.local.json'
AREA_KINDS = ('wander', 'spawn', 'plan')
FACTION_KINDS = ('npc', 'city', 'guild', 'clan', 'other')
STANCES = ('allied', 'friendly', 'neutral', 'tense', 'hostile', 'war')
MAX_FACTIONS = 64                                          # What the game's world loader accepts.
ID = re.compile(r'^[a-z][a-z0-9_]{0,39}$')          # Within ratw_id (48), leaving room for suffixes.
RANK = {'viewer': 0, 'dm': 1, 'admin': 2}
MIME = {'.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.svg': 'image/svg+xml', '.png': 'image/png',
        '.woff2': 'font/woff2', '.ico': 'image/x-icon'}


class DMError(RuntimeError):
    def __init__(self, message, status=422):
        super().__init__(message)
        self.status = status


def token_hash(token: str) -> str:
    return hashlib.sha256(token.encode('utf-8')).hexdigest()


# --------------------------------------------------------------------------- Accounts

def create_account(conn, username, role, password):
    if not re.fullmatch(r'[a-z][a-z0-9_-]{2,31}', username or ''):
        raise DMError('Usernames are 3-32 lowercase letters, digits, _ or -.')
    if role not in RANK:
        raise DMError('Role must be viewer, dm or admin.')
    if len(password) < 12:
        raise DMError('DM passwords must be at least 12 characters.')
    conn.execute('INSERT INTO dm.admins (username, role, password) VALUES (%s, %s, %s)',
                 (username, role, S.Jsonb(world_db.hash_password(password))))


def set_password(conn, username, password):
    if len(password) < 12:
        raise DMError('DM passwords must be at least 12 characters.')
    changed = conn.execute('UPDATE dm.admins SET password = %s WHERE username = %s', (S.Jsonb(world_db.hash_password(password)), username)).rowcount
    if not changed:
        raise DMError(f'No DM account {username}.', 404)
    conn.execute('DELETE FROM dm.sessions WHERE username = %s', (username,))     # Signed out everywhere.


def create_defaults(conn):
    """The three DM accounts, each with its own generated password. Returns [(username, role, password)] made."""
    made = []
    for username, role in DEFAULT_ACCOUNTS:
        if conn.execute('SELECT 1 FROM dm.admins WHERE username = %s', (username,)).fetchone():
            continue
        password = secrets.token_urlsafe(15)
        create_account(conn, username, role, password)
        made.append((username, role, password))
    return made


# --------------------------------------------------------------------------- The service

class DungeonMaster:
    """Everything the UI does, over connections to PROD (accounts, live world) and DEV (rehearsal)."""

    def __init__(self, connect=None, clock=time.time, writer=None):
        if connect is None:
            pools = {target: world_db.Pool(target, 'dm') for target in TARGETS}    # Reused, not one per request.
            connect = lambda target: pools[target].connection()
        self._connect = connect
        self.clock = clock
        self.lock = threading.Lock()
        self._writer = writer                  # (Mind, model): writes life stories; made when first needed.

    def connect(self, target):
        if target not in TARGETS:
            raise DMError('Choose PROD or DEV.')
        try:
            return self._connect(target)
        except world_db.DatabaseError as error:
            raise DMError(f'The {target.upper()} database is not reachable: {error}', 503) from error

    # -- sign-in -----------------------------------------------------------------
    def login(self, username, password):
        with self.connect('prod') as conn:
            failures = conn.execute(f'''SELECT count(*) FROM dm.audit WHERE action = 'login.failed' AND target = %s
                AND at > %s AND at > coalesce((SELECT max(at) FROM dm.audit WHERE action = 'login' AND target = %s), 0)''',
                                    (username, self.clock() - LOCK_MINUTES * 60, username)).fetchone()[0]
            if failures >= MAX_FAILURES:
                raise DMError(f'Too many wrong passwords for {username}. Try again in {LOCK_MINUTES} minutes.', 429)
            row = conn.execute('SELECT role, password, disabled FROM dm.admins WHERE username = %s', (str(username),)).fetchone()
            if not row or row[2] or not world_db.password_matches(row[1], str(password)):
                self.audit(conn, str(username)[:60], 'login.failed', str(username)[:60], 'Wrong username or password.')
                raise DMError('Wrong username or password.', 401)
            token = secrets.token_urlsafe(32)
            conn.execute("INSERT INTO dm.sessions (token_hash, username, expires_at) VALUES (%s, %s, now() + %s * interval '1 hour')",
                         (token_hash(token), username, SESSION_HOURS))
            conn.execute('UPDATE dm.admins SET last_login = now() WHERE username = %s', (username,))
            conn.execute('DELETE FROM dm.sessions WHERE expires_at < now()')
            self.audit(conn, username, 'login', username, 'Signed in.')
        return {'token': token, 'username': username, 'role': row[0]}

    def session(self, token):
        if not token:
            raise DMError('Sign in first.', 401)
        with self.connect('prod') as conn:
            row = conn.execute('''SELECT a.username, a.role FROM dm.sessions s JOIN dm.admins a USING (username)
                                  WHERE s.token_hash = %s AND s.expires_at > now() AND NOT a.disabled''', (token_hash(token),)).fetchone()
        if not row:
            raise DMError('Your session has ended; sign in again.', 401)
        return {'username': row[0], 'role': row[1]}

    def logout(self, token):
        with self.connect('prod') as conn:
            conn.execute('DELETE FROM dm.sessions WHERE token_hash = %s', (token_hash(token),))

    def audit(self, conn, who, action, target, detail):
        conn.execute('INSERT INTO dm.audit (at, action, target, detail, who) VALUES (%s, %s, %s, %s, %s)',
                     (self.clock(), action, target, detail[:2000], who))

    # -- players ---------------------------------------------------------------
    def players(self, target):
        """Every player character as last saved, with world-map positions, and recent actions on them."""
        with self.connect(target) as conn:
            world = conn.execute('SELECT id, name FROM world.worlds').fetchone()
            if not world:
                return {'target': target, 'world': None, 'characters': [], 'actions': []}
            places = {r['id']: r for r in S.dict_rows(conn, '''
                SELECT c.id, c.name, c.x, c.y, 'cell' AS kind FROM world.cells c WHERE c.world_id = %s
                UNION ALL SELECT i.id, i.name, i.overview_x, i.overview_y, 'interior' FROM world.interiors i WHERE i.world_id = %s''',
                (world[0], world[0]))}
            characters = []
            for key, data, saved in conn.execute('SELECT key, data, updated_at FROM game.characters WHERE world_id = %s ORDER BY name',
                                                 (world[0],)).fetchall():
                place = places.get(data.get('cell'))
                x, y = float(data.get('x', 0)), float(data.get('y', 0))
                characters.append({
                    'id': key, 'name': data.get('name', key), 'age': data.get('age'), 'dead': bool(data.get('dead')),
                    'cell': data.get('cell', ''), 'place': place['name'] if place else data.get('cell', ''),
                    'indoors': bool(place and place['kind'] == 'interior'), 'x': x, 'y': y,
                    # Interiors sit at their overview position on the world map.
                    'worldX': (place['x'] + (x if place['kind'] == 'cell' else 0)) if place else None,
                    'worldY': (place['y'] + (y if place['kind'] == 'cell' else 0)) if place else None,
                    'posture': data.get('posture', ''), 'activity': data.get('activity', ''),
                    'stats': {k: data.get(k) for k in ('strength', 'dexterity', 'wisdom', 'stamina')},
                    'skills': {k: data.get(k) for k in ('sneakSkill', 'hearingSkill', 'scentSkill')},
                    'senses': {k: data.get(k) for k in ('hearing', 'vision', 'smell')},
                    'saved': saved.isoformat()})
            actions = [{'id': r[0], 'kind': r[1], 'target': r[2], 'by': r[3], 'at': r[4].isoformat(), 'status': r[5], 'result': r[6]}
                       for r in conn.execute('''SELECT id, kind, target_id, requested_by, requested_at, status, result
                                                FROM dm.actions ORDER BY id DESC LIMIT 50''').fetchall()]
        return {'target': target, 'world': {'id': world[0], 'name': world[1]}, 'characters': characters, 'actions': actions}

    def world_map(self, target, lean=False):
        """The world for plotting positions: the editor's project (terrain, cells, interiors). Lean, each world cell
        comes as its outline and a preview, and its ground is asked for as it comes into view (ground())."""
        with self.connect(target) as conn:
            world = conn.execute('SELECT id FROM world.worlds').fetchone()
            if not world:
                raise DMError(f'The {target.upper()} database has no world yet.', 404)
            project, _ = S.load_world(conn, world[0], ground=not lean)
        return project

    def ground(self, target, ids):
        """These world cells' ground, heights as compact rows (live_edit.ground)."""
        try:
            with self.connect(target) as conn:
                cells, _ = L.ground(conn, ids)
        except ValueError as error:
            raise DMError(str(error)) from error
        return {'cells': cells}

    # -- live actions -----------------------------------------------------------
    def request(self, who, target, kind, character_id, reason=''):
        """Queues a live action for the game server. Returns its ID; the server writes back the outcome."""
        needed = ACTIONS.get(kind)
        if not needed:
            raise DMError(f'Unknown action: {kind}.')
        if RANK[who['role']] < RANK[needed]:
            raise DMError(f'Your role ({who["role"]}) cannot do that.', 403)
        with self.connect(target) as conn:
            with conn.transaction():
                exists = conn.execute('SELECT name FROM game.characters WHERE key = %s', (str(character_id),)).fetchone()
                if not exists:
                    raise DMError('No such character.', 404)
                action_id = conn.execute('''INSERT INTO dm.actions (kind, target_id, requested_by) VALUES (%s, %s, %s)
                                            RETURNING id''', (kind, character_id, who['username'])).fetchone()[0]
                conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action_id}),))
                self.audit(conn, who['username'], kind, character_id,
                           f'{target.upper()}: {kind} {exists[0]}' + (f' — {reason[:500]}' if reason else ''))
        return {'id': action_id, 'status': 'queued'}

    def queue(self, conn, who, target, kind, target_id, detail):
        """Adds a live action for the game server and audits it (inside the caller's transaction)."""
        action_id = conn.execute('INSERT INTO dm.actions (kind, target_id, requested_by) VALUES (%s, %s, %s) RETURNING id',
                                 (kind, target_id, who['username'])).fetchone()[0]
        conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action_id}),))
        self.audit(conn, who['username'], kind, target_id, f'{target.upper()}: {detail}'[:2000])
        return action_id

    def allowed(self, who, kind):
        if RANK[who['role']] < RANK[ACTIONS[kind]]:
            raise DMError(f'Your role ({who["role"]}) cannot do that.', 403)

    # -- NPCs --------------------------------------------------------------------
    def npcs(self, target):
        """Named NPCs (editable), job holders (from profession slots), routes, who is dead, and recent NPC actions."""
        with self.connect(target) as conn:
            world = conn.execute('SELECT id FROM world.worlds').fetchone()
            if not world:
                raise DMError(f'The {target.upper()} database has no world yet.', 404)
            project, _ = S.load_world(conn, world[0])
            holders = [{'id': r[0], 'name': r[1], 'role': r[2], 'slot': r[3], 'work': {'cell': r[4], 'x': r[5], 'y': r[6]},
                        'home': {'cell': r[7], 'x': r[8], 'y': r[9]}}
                       for r in conn.execute('''SELECT c.id, c.name, p.behavior, s.name, s.work_area, s.work_x, s.work_y,
                                                       s.home_area, s.home_x, s.home_y
                                                FROM live.profession_slots s JOIN live.professions p ON p.id = s.profession
                                                JOIN live.characters c ON c.status = 'active'
                                                     AND c.assignment = jsonb_build_object('world', s.world_id, 'slot', s.id)
                                                WHERE s.world_id = %s ORDER BY s.position''', (world[0],)).fetchall()]
            dead = [r[0] for r in conn.execute('SELECT npc_id FROM live.npc_state WHERE world_id = %s AND NOT alive', (world[0],))]
            actions = [{'id': r[0], 'kind': r[1], 'target': r[2], 'by': r[3], 'at': r[4].isoformat(), 'status': r[5], 'result': r[6]}
                       for r in conn.execute('''SELECT id, kind, target_id, requested_by, requested_at, status, result FROM dm.actions
                                                WHERE kind LIKE 'npc.%%' OR kind = 'layers.sync'
                                                ORDER BY id DESC LIMIT 60''').fetchall()]
            wanders = dict(conn.execute('SELECT id, wander_area FROM live.npcs WHERE world_id = %s AND wander_area IS NOT NULL',
                                        (world[0],)).fetchall())
            spawned = dict(conn.execute('SELECT id, spawn_id FROM live.npcs WHERE world_id = %s AND spawn_id IS NOT NULL',
                                        (world[0],)).fetchall())
            areas = [{'id': r[0], 'name': r[1], 'kind': r[2], 'cell': r[3], 'tiles': r[4]}
                     for r in conn.execute('''SELECT id, name, kind, area, tiles FROM live.npc_areas WHERE world_id = %s
                                              ORDER BY position, id''', (world[0],)).fetchall()]
            spawns = []
            for r in conn.execute('''SELECT id, name, area_id, template_id, count, respawn_minutes, enabled FROM live.spawns
                                     WHERE world_id = %s ORDER BY position, id''', (world[0],)).fetchall():
                made = [npc for npc, rule in spawned.items() if rule == r[0]]
                spawns.append({'id': r[0], 'name': r[1], 'area': r[2], 'template': r[3], 'count': r[4], 'respawnMinutes': r[5],
                               'enabled': r[6], 'alive': sum(1 for n in made if n not in dead),
                               'dead': sum(1 for n in made if n in dead)})
        return {'target': target, 'world': world[0], 'people': project['people'], 'holders': holders, 'dead': dead,
                'routes': project['routes'], 'areas': areas, 'spawns': spawns, 'wanders': wanders, 'spawned': spawned,
                'actions': actions}

    # -- the calendar (Docs/Design/26-living-npcs.md, Phase 9) ----------------------
    def calendar(self, target):
        """The world's date as last saved, its week, the season's festival, the communities that could hold one, and
        festivals called lately."""
        with self.connect(target) as conn:
            world = C.world_of(conn)
            if not world:
                raise DMError(f'The {target.upper()} database has no world yet.', 404)
            row = conn.execute("SELECT (payload::jsonb->>'calendarDays')::double precision FROM game.checkpoints WHERE world_id = %s",
                               (world,)).fetchone()
            day = row[0] if row and row[0] is not None and row[0] >= 0 else None
            communities = [{'id': r[0], 'residents': r[1]} for r in conn.execute('''
                SELECT p.region, count(*) FROM (
                    SELECT home_area AS place FROM live.npcs WHERE world_id = %(w)s
                    UNION ALL SELECT home_area FROM live.profession_slots WHERE world_id = %(w)s) h
                JOIN (SELECT id, region FROM world.cells WHERE world_id = %(w)s
                      UNION ALL SELECT id, region FROM world.interiors WHERE world_id = %(w)s) p ON p.id = h.place
                WHERE p.region <> 'unassigned' GROUP BY p.region HAVING count(*) >= 5 ORDER BY count(*) DESC, p.region''',
                {'w': world}).fetchall()]
            actions = [{'id': r[0], 'target': r[1], 'payload': r[2], 'by': r[3], 'at': r[4].isoformat(), 'status': r[5], 'result': r[6]}
                       for r in conn.execute('''SELECT id, target_id, payload, requested_by, requested_at, status, result
                                                FROM dm.actions WHERE kind = 'festival.call' ORDER BY id DESC LIMIT 20''').fetchall()]
        today = None
        if day is not None:
            year, season, of_season = C.season_of(day)
            start = int(day) - of_season + 1
            festival = start + C.FESTIVAL_DAY - 1
            if festival < int(day):
                festival += C.season_length(int(day))
            today = {'day': day, 'date': C.date_label(day), 'weekday': C.weekday_name(day), 'season': season,
                     'hour': round((day - int(day)) * 24, 2),
                     'nextMarket': (C.MARKETDAY - int(day) % 7) % 7, 'nextRest': (C.RESTDAY - int(day) % 7) % 7,
                     'nextFestival': festival - int(day), 'festivalDate': C.date_label(festival)}
        return {'target': target, 'today': today, 'communities': communities, 'actions': actions,
                'weekdays': list(C.WEEKDAYS)}

    def call_festival(self, who, target, community, name='', in_days=0):
        """Asks the game server to hold a festival in a community today (from noon) or some days ahead."""
        self.allowed(who, 'festival.call')
        community, name = str(community), str(name or '').strip()
        if not community or len(community) > 80:
            raise DMError('Choose a town.')
        if len(name) > 60 or any(ord(c) < 32 for c in name):
            raise DMError('A festival name is at most 60 plain characters.')
        if not isinstance(in_days, int) or isinstance(in_days, bool) or not 0 <= in_days <= 30:
            raise DMError('Call it for today or up to thirty days ahead.')
        if community not in {c['id'] for c in self.calendar(target)['communities']}:
            raise DMError(f'No town called {community} in {target.upper()}.', 404)
        with self.connect(target) as conn:
            with conn.transaction():
                action = conn.execute('''INSERT INTO dm.actions (kind, target_id, payload, requested_by)
                                         VALUES ('festival.call', %s, %s, %s) RETURNING id''',
                                      (community, json.dumps({'name': name, 'inDays': in_days}), who['username'])).fetchone()[0]
                conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action}),))
                when = 'today' if not in_days else f'in {in_days} day{"s" if in_days != 1 else ""}'
                self.audit(conn, who['username'], 'festival.call', community,
                           f'{target.upper()}: a festival{" (" + name + ")" if name else ""} in {community}, {when}')
        return {'id': action, 'status': 'queued'}

    # -- chronicles (Docs/Design/26-living-npcs.md, Phase 8) ----------------------
    def chronicle(self, target, subject):
        """Someone's life from the event log, the story written from it (if any), and what they carry in mind:
        memories, open conversations, bonds and what they have heard. For NPCs and player characters alike."""
        subject = str(subject)
        if not subject or len(subject) > 80:
            raise DMError('Choose someone.')
        with self.connect(target) as conn:
            world = C.world_of(conn)
            if not world:
                raise DMError(f'The {target.upper()} database has no world yet.', 404)
            life = C.load(conn, subject)
            life['story'] = self.stored_story(conn, world, subject, life)
            life['mind'] = self.mind_of(conn, world, subject)
        return life

    @staticmethod
    def stored_story(conn, world, subject, life):
        try:
            row = conn.execute('''SELECT story, through_event, entries, model, written_by, written_at FROM dm.stories
                                  WHERE world_id = %s AND subject = %s''', (world, subject)).fetchone()
        except Exception as error:             # A database without migration 0026: no stories yet.
            if 'stories' not in str(error):
                raise
            return None
        if not row:
            return None
        newer = sum(1 for e in life['entries'] if (e['id'] or 0) > row[1])
        return {'text': row[0], 'throughEvent': row[1], 'entries': row[2], 'model': row[3], 'by': row[4],
                'at': row[5].isoformat(), 'newer': newer}

    @staticmethod
    def mind_of(conn, world, subject):
        def rows(sql, *args):
            try:
                return conn.execute(sql, (world, *args)).fetchall()
            except Exception as error:         # A table this database doesn't have yet (bonds 0023, beliefs 0024).
                if 'does not exist' not in str(error):
                    raise
                return []
        memories = [{'npc': d.get('npc', ''), 'subject': d.get('subject', ''), 'text': d.get('text', ''),
                     'started': d.get('started'), 'consolidated': d.get('consolidated')}
                    for (d,) in rows('''SELECT data FROM game.npc_memories WHERE world_id = %s AND (npc = %s OR data->>'subject' = %s)
                                        ORDER BY (data->>'consolidated')::double precision DESC NULLS LAST LIMIT 40''',
                                     subject, subject)]
        talks = [{'npc': d.get('npc', ''), 'subject': d.get('subject', ''), 'started': d.get('started'),
                  'lastActivity': d.get('lastActivity'),
                  'turns': [{'who': t.get('who', ''), 'text': t.get('text', '')} for t in (d.get('turns') or [])[-12:]]}
                 for (d,) in rows('''SELECT data FROM game.conversations WHERE world_id = %s
                                     AND (data->>'npc' = %s OR data->>'subject' = %s) LIMIT 20''', subject, subject)]

        def bond(d, key):
            return {'who': d.get(key, ''), **{k: d.get(k, 0) for k in ('affinity', 'trust', 'familiarity', 'fear', 'respect',
                                                                       'owed', 'lastContact')}}
        order = "ORDER BY (data->>'familiarity')::double precision + abs((data->>'affinity')::double precision) DESC LIMIT 30"
        bonds = [bond(d, 'other') for (d,) in rows(f'SELECT data FROM game.bonds WHERE world_id = %s AND holder = %s {order}', subject)]
        regard = [bond(d, 'holder') for (d,) in rows(f'SELECT data FROM game.bonds WHERE world_id = %s AND other = %s {order}', subject)]

        def belief(d):
            return {k: d.get(k, '') for k in ('holder', 'subject', 'claim', 'source', 'confidence', 'day', 'incident')}
        heard = [belief(d) for (d,) in rows('''SELECT data FROM game.beliefs WHERE world_id = %s AND holder = %s
                                               ORDER BY (data->>'confidence')::double precision DESC LIMIT 40''', subject)]
        said = [belief(d) for (d,) in rows('''SELECT data FROM game.beliefs WHERE world_id = %s AND subject = %s
                                              ORDER BY (data->>'confidence')::double precision DESC LIMIT 40''', subject)]
        keys = {subject} | {m['npc'] for m in memories} | {m['subject'] for m in memories} | {t['npc'] for t in talks} \
            | {t['subject'] for t in talks} | {b['who'] for b in bonds + regard} \
            | {b[k] for b in heard + said for k in ('holder', 'subject', 'source')}
        keys = {k for k in keys if k}
        names = C.fetch_names(conn, world, keys)
        return {'memories': memories, 'conversations': talks, 'bonds': bonds, 'regard': regard, 'heard': heard,
                'said': said, 'names': {k: names.get(k) or C.plain_name(k) for k in keys}}

    def writer(self):
        """The NPC Mind's story writer, with the same model and key as live NPCs (RATW_AI=fixture: offline)."""
        with self.lock:
            if self._writer is None:
                import npc_bridge
                import npc_mind
                mode = os.environ.get('RATW_AI', 'on')
                if mode == 'off':
                    raise DMError('Life stories are off (RATW_AI=off).', 503)
                if mode == 'fixture':
                    provider, model = npc_mind.FixtureProvider(), 'fixture'
                else:
                    try:
                        config = npc_bridge.load_config(Path(os.environ.get('RATW_AI_CONFIG') or AI_CONFIG))
                    except npc_bridge.BridgeError as error:
                        raise DMError(f'No story writer: the NPC model is not configured ({error.code}).', 503) from None
                    provider, model = npc_mind.OpenAIProvider(config), config.model
                budget = npc_mind.Budget(STORIES_PER_HOUR, STORIES_PER_HOUR)
                self._writer = (npc_mind.Mind(provider, budget=budget, concurrency=2, timeout=40, wait=5), model)
            return self._writer

    def write_story(self, who, target, subject):
        """Has the story of someone's life written from their chronicle, and keeps it (paid for once)."""
        if RANK[who['role']] < RANK[WRITES['story.write']]:
            raise DMError(f'Your role ({who["role"]}) cannot do that.', 403)
        life = self.chronicle(target, subject)
        lines = C.story_lines(life)
        if not lines:
            raise DMError(f"Nothing is recorded of {life['name']}'s life yet.")
        with self.connect(target) as conn:
            world = C.world_of(conn)
            try:
                conn.execute('SELECT 1 FROM dm.stories LIMIT 0')
            except Exception:
                raise DMError('Apply migration 0026 first (python3 tools/world_db.py migrate).', 503) from None
            row = conn.execute('''SELECT data->>'description' FROM game.npcs WHERE world_id = %s AND key = %s
                                  UNION ALL SELECT data->>'description' FROM game.characters WHERE world_id = %s AND key = %s''',
                               (world, subject, world, subject)).fetchone()
        mind, model = self.writer()
        import npc_bridge
        try:
            story = mind.story({'name': life['name'], 'description': (row and row[0]) or '', 'lines': lines})['story']
        except npc_bridge.BridgeError as error:
            raise DMError(f'The story could not be written just now ({error.code}).', 503) from None
        with self.connect(target) as conn:
            with conn.transaction():
                conn.execute('''INSERT INTO dm.stories (world_id, subject, story, through_event, entries, model, written_by)
                                VALUES (%s, %s, %s, %s, %s, %s, %s)
                                ON CONFLICT (world_id, subject) DO UPDATE SET story = excluded.story,
                                    through_event = excluded.through_event, entries = excluded.entries,
                                    model = excluded.model, written_by = excluded.written_by, written_at = now()''',
                             (world, subject, story[:4000], life['lastEvent'], len(life['entries']), model[:80],
                              who['username']))
                self.audit(conn, who['username'], 'story.write', subject, f'{target.upper()}: the story of {life["name"]}')
        return self.chronicle(target, subject)

    def save_npc(self, who, target, person):
        """Creates or changes a named NPC: checked against the whole world, written through the live-edit log,
        then synced into a running server."""
        self.allowed(who, 'npc.sync')
        if not isinstance(person, dict) or not isinstance(person.get('id'), str):
            raise DMError('Send the NPC as an object with an id.')
        with self.connect(target) as conn:
            world = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
            project, _ = S.load_world(conn, world)
            before = next((p for p in project['people'] if p['id'] == person['id']), None)
            project['people'] = [p for p in project['people'] if p['id'] != person['id']] + [person]
            try:
                map_editor.check_project(project, for_game=False)
            except map_editor.ValidationError as error:
                raise DMError(' '.join(error.errors[:6])) from error
            try:
                L.apply_edit(conn, 'dungeon-master', who['username'], f'{"Changed" if before else "Created"} {person["name"]}',
                             [{'key': f'person:{person["id"]}', 'before': before, 'after': person}])
            except L.Conflict as conflict:
                raise DMError(f'{conflict} Reload and try again.', 409) from conflict
            except L.Rejected as rejected:
                raise DMError(str(rejected)) from rejected
            with conn.transaction():
                action = self.queue(conn, who, target, 'npc.sync', person['id'], f'{"changed" if before else "created"} NPC {person["name"]}')
        return {'id': person['id'], 'action': action}

    def delete_npc(self, who, target, npc_id):
        self.allowed(who, 'npc.sync')
        with self.connect(target) as conn:
            world = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
            project, _ = S.load_world(conn, world)
            before = next((p for p in project['people'] if p['id'] == npc_id), None)
            if not before:
                raise DMError('No such named NPC.', 404)
            try:
                L.apply_edit(conn, 'dungeon-master', who['username'], f'Deleted {before["name"]}',
                             [{'key': f'person:{npc_id}', 'before': before, 'after': None}])
            except (L.Conflict, L.Rejected) as error:
                raise DMError(str(error), error.status) from error
            with conn.transaction():
                conn.execute('DELETE FROM live.npc_state WHERE world_id = %s AND npc_id = %s', (world, npc_id))
                action = self.queue(conn, who, target, 'npc.sync', npc_id, f'deleted NPC {before["name"]}')
        return {'id': npc_id, 'action': action}

    def npc_life(self, who, target, npc_id, dead):
        """Kills or revives an NPC now in a running server, and in the saved state for the next start."""
        kind = 'npc.kill' if dead else 'npc.revive'
        self.allowed(who, kind)
        listing = self.npcs(target)
        name = next((p['name'] for p in listing['people'] + listing['holders'] if p['id'] == npc_id), None)
        if not name:
            raise DMError('No such NPC.', 404)
        with self.connect(target) as conn:
            with conn.transaction():
                conn.execute('''INSERT INTO live.npc_state (world_id, npc_id, alive, state, updated_at)
                                VALUES (%s, %s, %s, jsonb_build_object('dead', %s), now())
                                ON CONFLICT (world_id, npc_id) DO UPDATE SET alive = excluded.alive,
                                state = live.npc_state.state || jsonb_build_object('dead', %s), updated_at = now()''',
                             (listing['world'], npc_id, not dead, dead, dead))
                action = self.queue(conn, who, target, kind, npc_id, f'{"killed" if dead else "revived"} NPC {name}')
        return {'id': npc_id, 'action': action}

    # -- NPC layers: patrol routes, painted areas, spawn rules ----------------------
    def edit_world(self, conn, who, summary, key, before, after, check=None):
        """One change through the live-edit log, after checking the whole world with it applied."""
        if check is not None:
            try:
                map_editor.check_project(check, for_game=False)
            except map_editor.ValidationError as error:
                raise DMError(' '.join(error.errors[:6])) from error
        try:
            L.apply_edit(conn, 'dungeon-master', who['username'], summary, [{'key': key, 'before': before, 'after': after}])
        except L.Conflict as conflict:
            raise DMError(f'{conflict} Reload and try again.', 409) from conflict
        except L.Rejected as rejected:
            raise DMError(str(rejected)) from rejected

    def save_route(self, who, target, route):
        """Creates or changes a patrol route (posts in order), then has a running server take it."""
        self.allowed(who, 'layers.sync')
        if not isinstance(route, dict) or not isinstance(route.get('id'), str) or not ID.match(route['id']):
            raise DMError('A route needs an id of lowercase letters, digits and underscores.')
        with self.connect(target) as conn:
            world = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
            project, _ = S.load_world(conn, world)
            before = next((r for r in project['routes'] if r['id'] == route['id']), None)
            project['routes'] = [r for r in project['routes'] if r['id'] != route['id']] + [route]
            self.edit_world(conn, who, f'{"Changed" if before else "Created"} patrol route {route.get("name", "")}',
                            f'route:{route["id"]}', before, route, project)
            with conn.transaction():
                action = self.queue(conn, who, target, 'layers.sync', route['id'],
                                    f'{"changed" if before else "created"} route {route.get("name")} ({len(route.get("posts", []))} posts)')
        return {'id': route['id'], 'action': action}

    def delete_route(self, who, target, route_id):
        self.allowed(who, 'layers.sync')
        with self.connect(target) as conn:
            world = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
            project, _ = S.load_world(conn, world)
            before = next((r for r in project['routes'] if r['id'] == route_id), None)
            if not before:
                raise DMError('No such patrol route.', 404)
            users = [p['name'] for p in project['people'] + project.get('slots', []) if p.get('route') == route_id]
            if users:
                raise DMError(f'{", ".join(users[:5])} still walk this route; give them another first.')
            self.edit_world(conn, who, f'Deleted patrol route {before["name"]}', f'route:{route_id}', before, None)
            with conn.transaction():
                action = self.queue(conn, who, target, 'layers.sync', route_id, f'deleted route {before["name"]}')
        return {'id': route_id, 'action': action}

    def save_area(self, who, target, area):
        """Creates or changes a painted NPC area: tiles of one cell or interior."""
        self.allowed(who, 'layers.sync')
        if not isinstance(area, dict) or not isinstance(area.get('id'), str) or not ID.match(area['id']):
            raise DMError('An area needs an id of lowercase letters, digits and underscores.')
        name, kind, cell, tiles = str(area.get('name', '')).strip(), area.get('kind'), area.get('cell'), area.get('tiles')
        if not 1 <= len(name) <= 120:
            raise DMError('Give the area a name (up to 120 characters).')
        if kind not in AREA_KINDS:
            raise DMError(f'An area is one of: {", ".join(AREA_KINDS)}.')
        if not isinstance(tiles, list) or len(tiles) > 4096:
            raise DMError('An area has at most 4096 tiles.')
        if not all(isinstance(t, list) and len(t) == 2 and all(type(v) is int for v in t) for t in tiles):
            raise DMError('Tiles are [x, y] pairs of whole numbers.')
        clean = sorted({(t[0], t[1]) for t in tiles})
        with self.connect(target) as conn:
            world = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
            project, _ = S.load_world(conn, world)
            place = next((c for c in project['cells'] + project['rooms'] if c['id'] == cell), None)
            if not place:
                raise DMError('Paint the area in a cell or interior of this world.')
            if any(not (0 <= x < place['width'] and 0 <= y < place['height']) for x, y in clean):
                raise DMError(f'Some tiles lie outside {place["name"]}.')
            with conn.transaction():
                exists = conn.execute('SELECT 1 FROM live.npc_areas WHERE world_id = %s AND id = %s', (world, area['id'])).fetchone()
                conn.execute("""INSERT INTO live.npc_areas (world_id, id, name, kind, area, tiles, position, origin)
                                VALUES (%s, %s, %s, %s, %s, %s,
                                        (SELECT coalesce(max(position) + 1, 0) FROM live.npc_areas WHERE world_id = %s), 'authored')
                                ON CONFLICT (world_id, id) DO UPDATE SET name = excluded.name, kind = excluded.kind,
                                    area = excluded.area, tiles = excluded.tiles, updated_at = now()""",
                             (world, area['id'], name, kind, cell, S.Jsonb([list(t) for t in clean]), world))
                action = self.queue(conn, who, target, 'layers.sync', area['id'],
                                    f'{"changed" if exists else "created"} {kind} area {name} ({len(clean)} tiles in {place["name"]})')
        return {'id': area['id'], 'action': action}

    def delete_area(self, who, target, area_id):
        """Removes an area; its spawn rules go with it, and NPCs who wandered it keep to their schedules."""
        self.allowed(who, 'layers.sync')
        with self.connect(target) as conn:
            with conn.transaction():
                row = conn.execute('DELETE FROM live.npc_areas WHERE id = %s RETURNING name', (area_id,)).fetchone()
                if not row:
                    raise DMError('No such area.', 404)
                action = self.queue(conn, who, target, 'layers.sync', area_id, f'deleted area {row[0]}')
        return {'id': area_id, 'action': action}

    def save_spawn(self, who, target, rule):
        """Creates or changes a spawn rule. The game server reads its rules every few seconds, so no sync is queued."""
        self.allowed(who, 'layers.sync')
        if not isinstance(rule, dict) or not isinstance(rule.get('id'), str) or not ID.match(rule['id']) or len(rule['id']) > 32:
            raise DMError('A spawn rule needs an id of up to 32 lowercase letters, digits and underscores.')
        name = str(rule.get('name', '')).strip()
        count, respawn, enabled = rule.get('count'), rule.get('respawnMinutes'), bool(rule.get('enabled', True))
        if not 1 <= len(name) <= 80:
            raise DMError('Give the rule a name (up to 80 characters).')
        if type(count) is not int or not 1 <= count <= 50:
            raise DMError('A rule keeps 1 to 50 NPCs alive.')
        if type(respawn) is not int or not 0 <= respawn <= 10080:
            raise DMError('Respawn time is 0 to 10080 minutes (a week).')
        with self.connect(target) as conn:
            world = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
            if not conn.execute('SELECT 1 FROM live.npc_areas WHERE world_id = %s AND id = %s', (world, rule.get('area'))).fetchone():
                raise DMError('Choose the area the NPCs appear in.')
            template = conn.execute('SELECT name FROM live.npcs WHERE world_id = %s AND id = %s AND spawn_id IS NULL',
                                    (world, rule.get('template'))).fetchone()
            if not template:
                raise DMError('Choose a named NPC as the template.')
            with conn.transaction():
                exists = conn.execute('SELECT 1 FROM live.spawns WHERE world_id = %s AND id = %s', (world, rule['id'])).fetchone()
                conn.execute("""INSERT INTO live.spawns (world_id, id, name, area_id, template_id, count, respawn_minutes, enabled,
                                                         position, origin)
                                VALUES (%s, %s, %s, %s, %s, %s, %s, %s,
                                        (SELECT coalesce(max(position) + 1, 0) FROM live.spawns WHERE world_id = %s), 'authored')
                                ON CONFLICT (world_id, id) DO UPDATE SET name = excluded.name, area_id = excluded.area_id,
                                    template_id = excluded.template_id, count = excluded.count,
                                    respawn_minutes = excluded.respawn_minutes, enabled = excluded.enabled, updated_at = now()""",
                             (world, rule['id'], name, rule['area'], rule['template'], count, respawn, enabled, world))
                self.audit(conn, who['username'], 'spawn.save', rule['id'],
                           f'{target.upper()}: {"changed" if exists else "created"} spawn rule {name}: {count} × {template[0]}, '
                           f'respawn {respawn} min, {"on" if enabled else "off"}')
        return {'id': rule['id']}

    def delete_spawn(self, who, target, rule_id):
        """Removes a rule. NPCs it made stay in the world as ordinary NPCs."""
        self.allowed(who, 'layers.sync')
        with self.connect(target) as conn:
            with conn.transaction():
                row = conn.execute('DELETE FROM live.spawns WHERE id = %s RETURNING name', (rule_id,)).fetchone()
                if not row:
                    raise DMError('No such spawn rule.', 404)
                self.audit(conn, who['username'], 'spawn.delete', rule_id, f'{target.upper()}: deleted spawn rule {row[0]}')
        return {'id': rule_id}

    def set_wander(self, who, target, npc_id, area_id):
        """Gives a named NPC an area to roam in their free time, or takes it away (area_id None)."""
        self.allowed(who, 'layers.sync')
        with self.connect(target) as conn:
            world = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
            with conn.transaction():
                if area_id and not conn.execute('SELECT 1 FROM live.npc_areas WHERE world_id = %s AND id = %s',
                                                (world, area_id)).fetchone():
                    raise DMError('No such area.', 404)
                row = conn.execute('UPDATE live.npcs SET wander_area = %s, updated_at = now() WHERE world_id = %s AND id = %s RETURNING name',
                                   (area_id or None, world, npc_id)).fetchone()
                if not row:
                    raise DMError('No such named NPC.', 404)
                action = self.queue(conn, who, target, 'layers.sync', npc_id,
                                    f'{row[0]} now wanders {area_id}' if area_id else f'{row[0]} no longer wanders')
        return {'id': npc_id, 'action': action}

    # -- Factions: factions, territory claims, relations and members -------------------------------------------
    def factions(self, target):
        """Every faction with its claims, members and relations, the named NPCs who could join, and recent syncs."""
        with self.connect(target) as conn:
            world = conn.execute('SELECT id FROM world.worlds').fetchone()
            if not world:
                raise DMError(f'The {target.upper()} database has no world yet.', 404)
            w = world[0]
            rows = lambda sql: S.dict_rows(conn, sql, (w,))
            factions = [{'id': r['id'], 'name': r['name'], 'color': r['color'], 'kind': r['kind'], 'description': r['description']}
                        for r in rows('SELECT * FROM live.factions WHERE world_id = %s ORDER BY position, id')]
            claims = [{'faction': r['faction_id'], 'area': r['area'], 'tiles': r['tiles']}
                      for r in rows('SELECT faction_id, area, tiles FROM live.faction_claims WHERE world_id = %s ORDER BY faction_id, area')]
            relations = [{'faction': r['faction_id'], 'other': r['other_id'], 'disposition': r['disposition'], 'stance': r['stance'],
                          'reason': r['reason'], 'by': r['updated_by'], 'at': r['updated_at'].isoformat()}
                         for r in rows('SELECT * FROM live.faction_relations WHERE world_id = %s')]
            members = [{'faction': r['faction_id'], 'npc': r['npc_id'], 'rank': r['rank']}
                       for r in rows('SELECT faction_id, npc_id, rank FROM live.faction_members WHERE world_id = %s ORDER BY faction_id, npc_id')]
            people = [{'id': r['id'], 'name': r['name'], 'role': r['role']}
                      for r in rows('SELECT id, name, role FROM live.npcs WHERE world_id = %s ORDER BY position')]
            actions = [{'id': r[0], 'kind': r[1], 'target': r[2], 'by': r[3], 'at': r[4].isoformat(), 'status': r[5], 'result': r[6]}
                       for r in conn.execute('''SELECT id, kind, target_id, requested_by, requested_at, status, result FROM dm.actions
                                                WHERE kind = 'factions.sync' ORDER BY id DESC LIMIT 60''').fetchall()]
        return {'target': target, 'world': w, 'factions': factions, 'claims': claims, 'relations': relations,
                'members': members, 'people': people, 'actions': actions}

    def relation_history(self, target, faction, other):
        with self.connect(target) as conn:
            return [{'disposition': r[0], 'stance': r[1], 'reason': r[2], 'by': r[3], 'at': r[4].isoformat()}
                    for r in conn.execute('''SELECT disposition, stance, reason, by, at FROM live.faction_relation_log
                                             WHERE faction_id = %s AND other_id = %s ORDER BY id DESC LIMIT 50''',
                                          (faction, other)).fetchall()]

    def world_id(self, conn):
        return conn.execute('SELECT id FROM world.worlds').fetchone()[0]

    def save_faction(self, who, target, faction):
        """Creates or changes a faction. Its name and color go through the live-edit log (Atlas shows them); a running
        server takes the change at once."""
        self.allowed(who, 'factions.sync')
        if not isinstance(faction, dict) or not isinstance(faction.get('id'), str) or not ID.match(faction['id']):
            raise DMError('A faction needs an id of lowercase letters, digits and underscores.')
        name, color = str(faction.get('name', '')).strip(), faction.get('color')
        kind, description = faction.get('kind', 'npc'), str(faction.get('description', ''))
        if not 1 <= len(name) <= 120:
            raise DMError('Give the faction a name (up to 120 characters).')
        if not isinstance(color, str) or not re.fullmatch(r'#[0-9a-fA-F]{6}', color):
            raise DMError('A faction color is #rrggbb.')
        if kind not in FACTION_KINDS:
            raise DMError(f'A faction is one of: {", ".join(FACTION_KINDS)}.')
        if len(description) > 4000:
            raise DMError('Keep the description under 4000 characters.')
        with self.connect(target) as conn:
            w = self.world_id(conn)
            found = S.dict_rows(conn, 'SELECT id, name, color FROM live.factions WHERE world_id = %s AND id = %s', (w, faction['id']))
            before = found[0] if found else None
            if not before and conn.execute('SELECT count(*) FROM live.factions WHERE world_id = %s', (w,)).fetchone()[0] >= MAX_FACTIONS:
                raise DMError(f'The world has the most factions it can hold ({MAX_FACTIONS}).')
            after = {'id': faction['id'], 'name': name, 'color': color}
            if before != after:
                self.edit_world(conn, who, f'{"Changed" if before else "Created"} faction {name}', f'faction:{faction["id"]}', before, after)
            with conn.transaction():
                conn.execute('UPDATE live.factions SET kind = %s, description = %s, updated_at = now() WHERE world_id = %s AND id = %s',
                             (kind, description, w, faction['id']))
                action = self.queue(conn, who, target, 'factions.sync', faction['id'],
                                    f'{"changed" if before else "created"} {kind} faction {name}')
        return {'id': faction['id'], 'action': action}

    def delete_faction(self, who, target, faction_id):
        """Removes a faction with its claims, members and relations (their history stays)."""
        self.allowed(who, 'factions.sync')
        with self.connect(target) as conn:
            w = self.world_id(conn)
            found = S.dict_rows(conn, 'SELECT id, name, color FROM live.factions WHERE world_id = %s AND id = %s', (w, faction_id))
            if not found:
                raise DMError('No such faction.', 404)
            L.change_claims(conn, 'dungeon-master', who['username'], f'Removed the claims of {found[0]["name"]}',
                            lambda c: c.execute('DELETE FROM live.faction_claims WHERE world_id = %s AND faction_id = %s', (w, faction_id)))
            self.edit_world(conn, who, f'Deleted faction {found[0]["name"]}', f'faction:{faction_id}', found[0], None)
            with conn.transaction():
                action = self.queue(conn, who, target, 'factions.sync', faction_id, f'deleted faction {found[0]["name"]}')
        return {'id': faction_id, 'action': action}

    def save_claim(self, who, target, faction_id, area, tiles):
        """A faction's claim on one cell or interior: painted tiles, or the whole place when `tiles` is empty."""
        self.allowed(who, 'factions.sync')
        if not isinstance(tiles, list) or len(tiles) > 4096:
            raise DMError('A claim has at most 4096 tiles.')
        if not all(isinstance(t, list) and len(t) == 2 and all(type(v) is int for v in t) for t in tiles):
            raise DMError('Tiles are [x, y] pairs of whole numbers.')
        clean = [list(t) for t in sorted({(t[0], t[1]) for t in tiles})]
        with self.connect(target) as conn:
            w = self.world_id(conn)
            project, _ = S.load_world(conn, w)
            place = next((c for c in project['cells'] + project['rooms'] if c['id'] == area), None)
            faction = conn.execute('SELECT name FROM live.factions WHERE world_id = %s AND id = %s', (w, faction_id)).fetchone()
            if not place or not faction:
                raise DMError('Choose a faction and a cell or interior of this world.', 404)
            if any(not (0 <= x < place['width'] and 0 <= y < place['height']) for x, y in clean):
                raise DMError(f'Some tiles lie outside {place["name"]}.')
            L.change_claims(conn, 'dungeon-master', who['username'], f'{faction[0]} claims {place["name"]}', lambda c: c.execute(
                '''INSERT INTO live.faction_claims (world_id, faction_id, area, tiles) VALUES (%s, %s, %s, %s)
                   ON CONFLICT (world_id, faction_id, area) DO UPDATE SET tiles = excluded.tiles, updated_at = now()''',
                (w, faction_id, area, S.Jsonb(clean))))
            with conn.transaction():
                action = self.queue(conn, who, target, 'factions.sync', faction_id,
                                    f'{faction[0]} claims {len(clean) or "all"} tiles of {place["name"]}')
        return {'faction': faction_id, 'area': area, 'action': action}

    def delete_claim(self, who, target, faction_id, area):
        self.allowed(who, 'factions.sync')
        with self.connect(target) as conn:
            w = self.world_id(conn)
            gone = L.change_claims(conn, 'dungeon-master', who['username'], f'{faction_id} gives up {area}', lambda c: c.execute(
                'DELETE FROM live.faction_claims WHERE world_id = %s AND faction_id = %s AND area = %s RETURNING 1',
                (w, faction_id, area)).fetchone())
            if not gone:
                raise DMError('No such claim.', 404)
            with conn.transaction():
                action = self.queue(conn, who, target, 'factions.sync', faction_id, f'{faction_id} gave up its claim on {area}')
        return {'faction': faction_id, 'area': area, 'action': action}

    def save_relation(self, who, target, faction_id, other_id, disposition, stance, reason=''):
        """How one faction regards another: a disposition (-100 to 100) and a stance, with the reason. Logged."""
        self.allowed(who, 'factions.sync')
        if faction_id == other_id:
            raise DMError('A faction cannot have a relation with itself.')
        if type(disposition) is not int or not -100 <= disposition <= 100:
            raise DMError('Disposition is a whole number from -100 to 100.')
        if stance not in STANCES:
            raise DMError(f'A stance is one of: {", ".join(STANCES)}.')
        reason = str(reason or '').strip()
        if len(reason) > 1000:
            raise DMError('Keep the reason under 1000 characters.')
        with self.connect(target) as conn:
            w = self.world_id(conn)
            names = dict(conn.execute('SELECT id, name FROM live.factions WHERE world_id = %s AND id IN (%s, %s)',
                                      (w, faction_id, other_id)).fetchall())
            if len(names) != 2:
                raise DMError('No such faction.', 404)
            with conn.transaction():
                conn.execute('''INSERT INTO live.faction_relations (world_id, faction_id, other_id, disposition, stance, reason, updated_by)
                                VALUES (%s, %s, %s, %s, %s, %s, %s)
                                ON CONFLICT (world_id, faction_id, other_id) DO UPDATE SET disposition = excluded.disposition,
                                    stance = excluded.stance, reason = excluded.reason, updated_by = excluded.updated_by, updated_at = now()''',
                             (w, faction_id, other_id, disposition, stance, reason, who['username']))
                conn.execute('''INSERT INTO live.faction_relation_log (world_id, faction_id, other_id, disposition, stance, reason, by)
                                VALUES (%s, %s, %s, %s, %s, %s, %s)''', (w, faction_id, other_id, disposition, stance, reason, who['username']))
                self.audit(conn, who['username'], 'faction.relation', f'{faction_id}>{other_id}',
                           f'{target.upper()}: {names[faction_id]} regards {names[other_id]} as {stance} ({disposition:+d})'
                           + (f' — {reason[:300]}' if reason else ''))
        return {'faction': faction_id, 'other': other_id}

    def set_member(self, who, target, faction_id, npc_id, rank=None):
        """Adds a named NPC to a faction (or changes their rank); rank None removes them."""
        self.allowed(who, 'factions.sync')
        with self.connect(target) as conn:
            w = self.world_id(conn)
            with conn.transaction():
                if rank is None:
                    gone = conn.execute('DELETE FROM live.faction_members WHERE world_id = %s AND faction_id = %s AND npc_id = %s RETURNING 1',
                                        (w, faction_id, npc_id)).fetchone()
                    if not gone:
                        raise DMError('They are not a member.', 404)
                else:
                    rank = str(rank).strip()[:60]
                    names = conn.execute('''SELECT f.name, n.name FROM live.factions f, live.npcs n WHERE f.world_id = %s AND f.id = %s
                                            AND n.world_id = f.world_id AND n.id = %s''', (w, faction_id, npc_id)).fetchone()
                    if not names:
                        raise DMError('Choose a faction and a named NPC.', 404)
                    conn.execute('''INSERT INTO live.faction_members (world_id, faction_id, npc_id, rank) VALUES (%s, %s, %s, %s)
                                    ON CONFLICT (world_id, faction_id, npc_id) DO UPDATE SET rank = excluded.rank, updated_at = now()''',
                                 (w, faction_id, npc_id, rank))
                self.audit(conn, who['username'], 'faction.member', f'{faction_id}:{npc_id}',
                           f'{target.upper()}: {npc_id} ' + ('left ' + faction_id if rank is None else f'is in {faction_id}' + (f' as {rank}' if rank else '')))
        return {'faction': faction_id, 'npc': npc_id}

    def action(self, target, action_id):
        with self.connect(target) as conn:
            row = conn.execute('SELECT status, result, done_at FROM dm.actions WHERE id = %s', (int(action_id),)).fetchone()
        if not row:
            raise DMError('No such action.', 404)
        return {'id': int(action_id), 'status': row[0], 'result': row[1], 'doneAt': row[2].isoformat() if row[2] else None}


# --------------------------------------------------------------------------- HTTP

def make_server(port=8766, dm=None):
    dm = dm or DungeonMaster()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            pass     # Never log bodies, passwords or tokens.

        def trusted(self):
            hosts = (f'127.0.0.1:{self.server.server_port}', f'localhost:{self.server.server_port}')
            dev = ('127.0.0.1:5173', 'localhost:5173')                        # Vite dev server.
            return (self.headers.get('Host') in hosts + dev
                    and self.headers.get('Origin', 'http://' + hosts[0]) in tuple(f'http://{h}' for h in hosts + dev))

        def reply(self, status, data, mime='application/json'):
            body = data if isinstance(data, bytes) else json.dumps(data, ensure_ascii=True, default=str).encode('utf-8')
            self.send_response(status)
            self.send_header('Content-Type', mime + ('; charset=utf-8' if mime.startswith(('text/', 'application/json')) else ''))
            body = http_body.send(self, body, mime)
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Referrer-Policy', 'no-referrer')
            self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; "
                             "img-src 'self' data: blob:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; object-src 'none'")
            self.end_headers()
            self.wfile.write(body)

        def token(self):
            value = self.headers.get('Authorization', '')
            return value[7:] if value.startswith('Bearer ') else ''

        def who(self):
            return dm.session(self.token())

        def target(self, query):
            return (query.get('target') or ['prod'])[0]

        def body(self):
            size = int(self.headers.get('Content-Length', '-1'))
            if self.headers.get_content_type() != 'application/json' or not 0 < size <= MAX_BODY:
                raise DMError('A small JSON request is required.', 415)
            self.connection.settimeout(10)
            data = json.loads(self.rfile.read(size))
            if not isinstance(data, dict):
                raise DMError('Bad request.', 400)
            return data

        def static(self, path):
            relative = 'dm.html' if path in ('/', '/dm.html') else path.lstrip('/')
            file = (DIST / relative).resolve()
            if DIST.resolve() not in file.parents or not file.is_file() or file.suffix not in MIME:
                if relative == 'dm.html':
                    return self.reply(200, b'<!doctype html><title>Dungeon Master</title><p>Not built yet: run bash tools/dungeon-master.sh',
                                      'text/html')
                return self.reply(404, {'error': 'Not found.'})
            return self.reply(200, file.read_bytes(), MIME[file.suffix])

        def handle_api(self, method):
            parts = urlsplit(self.path)
            path, query = parts.path, parse_qs(parts.query)
            if method == 'POST' and path == '/api/login':
                data = self.body()
                return self.reply(200, dm.login(str(data.get('username', '')), str(data.get('password', ''))))
            who = self.who()
            if method == 'GET' and path == '/api/me':
                return self.reply(200, who)
            if method == 'POST' and path == '/api/logout':
                dm.logout(self.token())
                return self.reply(200, {'ok': True})
            if method == 'GET' and path == '/api/players':
                return self.reply(200, dm.players(self.target(query)))
            if method == 'GET' and path == '/api/world':
                return self.reply(200, dm.world_map(self.target(query), lean=query.get('lean') == ['1']))
            if method == 'GET' and path == '/api/ground':
                return self.reply(200, dm.ground(self.target(query), [i for i in ','.join(query.get('cells', [])).split(',') if i]))
            if method == 'POST' and path == '/api/actions':
                data = self.body()
                return self.reply(200, dm.request(who, str(data.get('target', 'prod')), str(data.get('kind', '')),
                                                  str(data.get('characterId', '')), str(data.get('reason', ''))))
            if method == 'GET' and path == '/api/calendar':
                return self.reply(200, dm.calendar(self.target(query)))
            if method == 'POST' and path == '/api/festivals/call':
                data = self.body()
                return self.reply(200, dm.call_festival(who, str(data.get('target', 'prod')), data.get('community', ''),
                                                        data.get('name', ''), data.get('inDays', 0)))
            if method == 'GET' and path == '/api/chronicle':
                return self.reply(200, dm.chronicle(self.target(query), query.get('id', [''])[0]))
            if method == 'POST' and path == '/api/chronicle/story':
                data = self.body()
                return self.reply(200, dm.write_story(who, str(data.get('target', 'prod')), str(data.get('id', ''))))
            if method == 'GET' and path == '/api/npcs':
                return self.reply(200, dm.npcs(self.target(query)))
            if method == 'POST' and path == '/api/npcs/save':
                data = self.body()
                return self.reply(200, dm.save_npc(who, str(data.get('target', 'prod')), data.get('person')))
            if method == 'POST' and path == '/api/npcs/delete':
                data = self.body()
                return self.reply(200, dm.delete_npc(who, str(data.get('target', 'prod')), str(data.get('id', ''))))
            if method == 'POST' and path == '/api/npcs/life':
                data = self.body()
                return self.reply(200, dm.npc_life(who, str(data.get('target', 'prod')), str(data.get('id', '')), bool(data.get('dead'))))
            layer_posts = {
                '/api/routes/save': lambda d: dm.save_route(who, str(d.get('target', 'prod')), d.get('route')),
                '/api/routes/delete': lambda d: dm.delete_route(who, str(d.get('target', 'prod')), str(d.get('id', ''))),
                '/api/areas/save': lambda d: dm.save_area(who, str(d.get('target', 'prod')), d.get('area')),
                '/api/areas/delete': lambda d: dm.delete_area(who, str(d.get('target', 'prod')), str(d.get('id', ''))),
                '/api/spawns/save': lambda d: dm.save_spawn(who, str(d.get('target', 'prod')), d.get('rule')),
                '/api/spawns/delete': lambda d: dm.delete_spawn(who, str(d.get('target', 'prod')), str(d.get('id', ''))),
                '/api/npcs/wander': lambda d: dm.set_wander(who, str(d.get('target', 'prod')), str(d.get('id', '')),
                                                            str(d['area']) if d.get('area') else None),
            }
            target = lambda d: str(d.get('target', 'prod'))
            layer_posts.update({
                '/api/factions/save': lambda d: dm.save_faction(who, target(d), d.get('faction')),
                '/api/factions/delete': lambda d: dm.delete_faction(who, target(d), str(d.get('id', ''))),
                '/api/factions/claim': lambda d: dm.save_claim(who, target(d), str(d.get('faction', '')), str(d.get('area', '')), d.get('tiles')),
                '/api/factions/unclaim': lambda d: dm.delete_claim(who, target(d), str(d.get('faction', '')), str(d.get('area', ''))),
                '/api/factions/relation': lambda d: dm.save_relation(who, target(d), str(d.get('faction', '')), str(d.get('other', '')),
                                                                     d.get('disposition'), d.get('stance'), d.get('reason', '')),
                '/api/factions/member': lambda d: dm.set_member(who, target(d), str(d.get('faction', '')), str(d.get('npc', '')),
                                                                None if d.get('rank') is None else str(d['rank'])),
            })
            if method == 'GET' and path == '/api/factions':
                return self.reply(200, dm.factions(self.target(query)))
            if method == 'GET' and path == '/api/factions/history':
                return self.reply(200, dm.relation_history(self.target(query), query.get('faction', [''])[0], query.get('other', [''])[0]))
            if method == 'POST' and path in layer_posts:
                return self.reply(200, layer_posts[path](self.body()))
            if method == 'GET' and path.startswith('/api/actions/'):
                return self.reply(200, dm.action(self.target(query), path.rsplit('/', 1)[1]))
            raise DMError('Not found.', 404)

        def dispatch(self, method):
            if not self.trusted():
                return self.reply(403, {'error': 'Local Dungeon Master origin required.'})
            path = urlsplit(self.path).path
            if not path.startswith('/api/'):
                return self.static(path) if method == 'GET' else self.reply(405, {'error': 'Method not allowed.'})
            try:
                return self.handle_api(method)
            except DMError as error:
                return self.reply(error.status, {'error': str(error)})
            except (ValueError, TypeError, KeyError):
                return self.reply(400, {'error': 'Malformed request.'})

        def do_GET(self):
            self.dispatch('GET')

        def do_POST(self):
            self.dispatch('POST')

    server = ThreadingHTTPServer(('127.0.0.1', port), Handler)
    server.daemon_threads = True
    return server


# --------------------------------------------------------------------------- Command line

def write_accounts_file(made):
    lines = ['# Dungeon Master accounts. Each password is shown here once; change them with',
             '#   python3 tools/dungeon_master.py accounts set-password USER',
             '# and delete this file when you have stored them safely. Never commit it.', '']
    lines += [f'{u:<12} {r:<7} {p}' for u, r, p in made]
    fd = os.open(ACCOUNTS_FILE, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, 'w', encoding='utf-8') as out:
        out.write('\n'.join(lines) + '\n')


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command')
    serve = sub.add_parser('serve')
    serve.add_argument('--port', type=int, default=8766)
    accounts = sub.add_parser('accounts')
    accounts.add_argument('action', choices=('create-defaults', 'list', 'set-password'))
    accounts.add_argument('username', nargs='?')
    args = parser.parse_args(argv)
    try:
        if args.command == 'accounts':
            with world_db.connect('prod', 'dm') as conn:
                if args.action == 'create-defaults':
                    made = create_defaults(conn)
                    if made:
                        write_accounts_file(made)
                        print(f'Created {", ".join(u for u, _, _ in made)}. Passwords: {ACCOUNTS_FILE.relative_to(ROOT)} (owner-only).')
                    else:
                        print('The DM accounts already exist.')
                elif args.action == 'list':
                    for u, r, d, last in conn.execute('SELECT username, role, disabled, last_login FROM dm.admins ORDER BY username'):
                        print(f'{u:<12} {r:<7} {"disabled" if d else "active":<9} last sign-in {last or "never"}')
                else:
                    if not args.username:
                        parser.error('set-password needs a username.')
                    first = getpass.getpass(f'New password for {args.username}: ')
                    if getpass.getpass('Repeat it: ') != first:
                        print('The two passwords differ; nothing changed.', file=sys.stderr)
                        return 1
                    set_password(conn, args.username, first)
                    print(f'Password changed; {args.username} is signed out everywhere.')
            return 0
        server = make_server(getattr(args, 'port', 8766))
        print(f'Dungeon Master: http://127.0.0.1:{server.server_port}  (local only; Ctrl+C to stop)', flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            server.server_close()
        return 0
    except (DMError, world_db.DatabaseError) as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
