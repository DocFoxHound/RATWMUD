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
from collections import Counter, defaultdict
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
SCENARIOS_FILE = ROOT / 'Data' / 'Economy' / 'scenarios.json'   # The orchestrator's scenarios (doc 46, Part 10).
DEFAULT_ACCOUNTS = (('dm-admin', 'admin'), ('dm-master', 'dm'), ('dm-viewer', 'viewer'))
SESSION_HOURS = 12
MAX_FAILURES, LOCK_MINUTES = 5, 15
MAX_BODY = 64 * 1024
TARGETS = ('prod', 'dev')


def practice_skills():
    """Every attribute and skill a wolf grows by practice, with its field and cap (Data/Progression/skills.json, doc 49):
    [(id, name, field or None for a trade skill, start, cap)]. Empty if the catalog can't be read."""
    try:
        doc = json.loads((ROOT / 'Data/Progression/skills.json').read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return []
    return [(e['id'], e.get('name', e['id']), e.get('field') or None, e.get('start', 0), e.get('cap', 100))
            for e in doc.get('attributes', []) + doc.get('skills', []) if isinstance(e, dict) and e.get('id')]


def practised(data, skills):
    """A saved character's attributes and skills with their caps (doc 49): {id: {name, value, cap}}. Trade skills come
    from its `skills` map, and only once begun."""
    trades = data.get('skills') if isinstance(data.get('skills'), dict) else {}
    out = {}
    for sid, name, field, start, cap in skills:
        value = data.get(field, start) if field else trades.get(sid)
        if isinstance(value, (int, float)) and not isinstance(value, bool) and (field or value > 0):
            out[sid] = {'name': name, 'value': value, 'cap': cap}
    return out


def newcomer_hours():
    """How many hours played end an account's newcomer days (Data/Social/newcomers.json, doc 52)."""
    try:
        return float(json.loads((ROOT / 'Data' / 'Social' / 'newcomers.json').read_text())['newcomer']['hours'])
    except (OSError, ValueError, KeyError, TypeError):
        return 15.0


def tie_view(row, names):
    """A tie for the Players tab's Ties list (doc 52): its state, starter, the two, when made and when it lapses."""
    return {'id': row.get('id', ''), 'state': row.get('state', ''), 'starter': row.get('starter', ''), 'town': row.get('town', ''),
            'newcomer': row.get('newcomer', ''), 'newcomerName': names.get(row.get('newcomer', ''), row.get('newcomer', '')),
            'other': row.get('other', ''), 'otherName': names.get(row.get('other', ''), row.get('other', '')),
            'resident': bool(row.get('resident')), 'made': row.get('made', 0) or 0, 'lapsesAt': row.get('lapsesAt', 0) or 0}


def person_view(row):
    """An account as a person for the Players tab (doc 50): its handle, experience, hours played and whether it is
    still new (doc 52: until it graduates, by hours played or social level); None if unknown."""
    if not isinstance(row, dict) or not row.get('account'):
        return None
    played = float(row.get('playedSeconds', 0))
    m = row.get('mentor') if isinstance(row.get('mentor'), dict) else {}
    mentor = 'revoked' if m.get('revoked') else ('available' if m.get('available', True) else 'busy') if m.get('on') else ''
    return {'handle': row.get('handle', ''), 'experience': row.get('experience', 'casual'),
            'playedHours': round(played / 3600, 1),
            'newcomer': not row.get('graduated', False) and played < newcomer_hours() * 3600,
            'mentor': mentor, 'guided': int(m.get('guided', 0) or 0)}


def account_view(row):
    """An account's standing for the Players tab (doc 49): its name, social level, each tier (who opened it, or None),
    the hold and its measures; None if unknown."""
    if not isinstance(row, dict) or not row.get('account'):
        return None
    measures = row.get('measures') if isinstance(row.get('measures'), dict) else {}
    return {'name': row['account'], 'socialLevel': measures.get('socialLevel'), 'measures': measures,
            'gifted': row.get('giftedBy') if row.get('giftedAt') is not None else None,
            'quickened': row.get('quickenedBy') if row.get('quickenedAt') is not None else None,
            'hold': bool(row.get('hold')), 'characters': [c for c in row.get('characters', []) if isinstance(c, str)]}


# Live actions this version knows, and who may request them.
def gift_families():
    """The Gift families a player may have (Data/Gifts/families.json, doc 43): all but the NPC-only ones."""
    families = json.loads((ROOT / 'Data/Gifts/families.json').read_text(encoding='utf-8'))['families']
    return [f['id'] for f in families if not f.get('npcOnly')]


ACTIONS = {'character.kill': 'dm', 'character.resurrect': 'dm', 'character.gift': 'dm', 'character.injury': 'dm',
           'account.unlock': 'dm',
           'bandits.call': 'dm', 'npc.sync': 'dm',
           'npc.kill': 'dm',
           'npc.revive': 'dm',
           'layers.sync': 'dm', 'factions.sync': 'dm', 'festival.call': 'dm',
           'artwork.review': 'dm', 'treaty.decide': 'dm', 'house.decide': 'dm', 'report.decide': 'dm',
           # Tying a player's Story book to a world storyline (Docs/Design/51-scenes-and-stars.md, Phase 7).
           'book.storyline': 'dm',
           # Turning an account's mentoring off until restored, and restoring it (Docs/Design/52-newcomers.md, Phase 2).
           'mentor.revoke': 'dm', 'mentor.restore': 'dm', 'tie.end': 'dm',
           'npc.move': 'dm', 'character.move': 'dm', 'visitor.add': 'dm', 'visitor.leave': 'dm',
           # Steering the economy orchestrator (Docs/Design/46-economy-orchestrator.md, Part 10).
           'economy.steer': 'dm', 'economy.unsteer': 'dm', 'economy.scenario': 'dm',
           # Notice boards and places to let (Docs/Design/54-gathering-places.md): a notice taken down; a place set to let,
           # or no longer (the game handled these already; the tool couldn't send them).
           'board.remove': 'dm', 'estate.set': 'dm', 'estate.clear': 'dm', 'estate.hold': 'dm', 'estate.release': 'dm',
           # Fame (Docs/Design/56-fame-and-memory.md): a deed awarded to a player character (target the doer; payload kind,
           # weight, cell, beneficiary, detail), or revoked (target the deed's id).
           'deed.award': 'dm', 'deed.revoke': 'dm', 'nickname.drop': 'dm', 'nickname.restore': 'dm',
           # Town projects (Docs/Design/57-changing-the-world.md, 7): one posted (target the town; payload kind, cell, x, y,
           # title), cancelled with its gifts returned, completed by hand, or taken down (target the project's id).
           'project.post': 'dm', 'project.cancel': 'dm', 'project.complete': 'dm', 'project.remove': 'dm',
           # Protected residents (doc 57, 6): marked live, or unmarked (target the resident).
           'npc.protect': 'dm', 'npc.unprotect': 'dm',
           # Storylines (Docs/Design/58-player-storytellers.md): one given to a character (payload template, cast), an
           # objective ticked by hand (target the storyline; payload step, objective).
           'storyline.give': 'dm', 'storyline.tick': 'dm',
           # Storytellers (doc 58, 11): an application decided or the standing revoked (target the account); a tale paused,
           # resumed or stopped (target its id); a milestone credited (payload built from the ledger here); the visitors'
           # list read again.
           'storyteller.decide': 'dm', 'storyteller.revoke': 'dm', 'tale.pause': 'dm', 'tale.resume': 'dm', 'tale.stop': 'dm',
           'milestone.credit': 'dm', 'visitors.sync': 'dm',
           # Marking a player a Dungeon Master in the game (the Dev Console) is for admins.
           'character.dm': 'admin'}
# Injuries a Dungeon Master may give (Docs/Design/38-injuries.md, phase 5; the game's Core/RatwInjury.cpp has the same).
INJURY_TYPES = ('torn_flank', 'bitten_foreleg', 'bitten_hindleg', 'torn_ear_acute', 'wrenched_neck', 'deep_gash', 'cut_foreleg',
                'cut_muzzle', 'cut_shoulder', 'bruised_ribs', 'cracked_rib', 'sprained_foreleg', 'knocked_senseless', 'burned_paws',
                'singed_coat', 'burned_muzzle', 'torn_ear', 'bent_tail', 'scarred_muzzle', 'scarred_flank', 'burn_scars',
                'notched_nose', 'clouded_eye', 'permanent_limp', 'bad_back', 'stiff_shoulder')
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
# The orchestrator's steers (doc 46, Part 10): each kind's strength range, and the channels it may close or favour.
STEER_KINDS = ('pressure', 'town', 'holder', 'channel', 'price')
STEER_CHANNELS = ('works', 'hires', 'commissions', 'food', 'trade', 'price support', 'wage support', 'rescue', 'opening')
STEER_DAYS = (1, 56)
STEER_ID = re.compile(r'^steer-[0-9]{1,18}$')
PLAIN_ID = re.compile(r'^[^\s\x00-\x1f\x7f]+$')       # An account or good id: no spaces or control characters.
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
            # Dungeon Masters in the game: as saved (game.characters.dungeon_master, migration 0033), or as the game server
            # has just made them, when it has applied a character.dm the save hasn't caught up with yet.
            made = {r[0]: (r[1], r[2]) for r in conn.execute('''
                SELECT DISTINCT ON (target_id) target_id, (payload->>'dungeonMaster')::boolean, done_at FROM dm.actions
                WHERE kind = 'character.dm' AND status = 'applied' ORDER BY target_id, id DESC''').fetchall()}
            characters = []
            skills = practice_skills()
            # Each account as a person and each character's profile (doc 50, migration 0035): read only here.
            persons, profiles = {}, {}
            if conn.execute("SELECT to_regclass('game.account_profiles')").fetchone()[0]:
                for (row,) in conn.execute('SELECT data FROM game.account_profiles WHERE world_id = %s', (world[0],)).fetchall():
                    for cid in row.get('characters', []) if isinstance(row, dict) else []:
                        persons[cid] = row
                for key_, row in conn.execute('SELECT key, data FROM game.profiles WHERE world_id = %s', (world[0],)).fetchall():
                    profiles[key_] = row
            # Circles (doc 50, migration 0039): each account's circles, with their members by handle; never their chat.
            circles = {}
            if persons and conn.execute("SELECT to_regclass('game.circles')").fetchone()[0]:
                handles = {row.get('account'): row.get('handle') or row.get('account') for row in persons.values()}
                for (row,) in conn.execute('SELECT data FROM game.circles WHERE world_id = %s', (world[0],)).fetchall():
                    members = [m for m in row.get('members', []) if isinstance(m, dict)] if isinstance(row, dict) else []
                    for m in members:
                        circles.setdefault(m.get('account'), []).append({
                            'name': row.get('name', ''), 'role': m.get('role', 'member'),
                            'members': sorted(handles.get(x.get('account'), x.get('account', '')) for x in members)})
            # Stars by account (doc 51, migration 0040): the counted total and how many accounts gave them.
            star_tallies = {}
            if conn.execute("SELECT to_regclass('game.star_tallies')").fetchone()[0]:
                for key_, row in conn.execute('SELECT key, data FROM game.star_tallies WHERE world_id = %s', (world[0],)).fetchall():
                    if isinstance(row, dict):
                        star_tallies[key_] = {'total': int(row.get('total', 0)), 'from': len(row.get('givers', []) or []),
                                              'kinds': row.get('kinds', {}) if isinstance(row.get('kinds'), dict) else {}}
            # Earned Gift tiers by account (doc 49, migration 0034): each character's account and its standing. (None
            # until the migration is applied and the game server has saved since.)
            standing = {}
            if conn.execute("SELECT to_regclass('game.account_standing')").fetchone()[0]:
                for (row,) in conn.execute('SELECT data FROM game.account_standing WHERE world_id = %s', (world[0],)).fetchall():
                    for cid in row.get('characters', []) if isinstance(row, dict) else []:
                        standing[cid] = row
            for key, data, saved, master in conn.execute('''SELECT key, data, updated_at, dungeon_master FROM game.characters
                                                            WHERE world_id = %s ORDER BY name''', (world[0],)).fetchall():
                latest = made.get(key)
                if latest and latest[1] and latest[1] > saved:
                    master = latest[0]
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
                    # A Gift (docs 33, 43): a family or "", Quickened or not.
                    'gift': data.get('gift', ''), 'quickened': bool(data.get('quickened')),
                    # Injuries that outlast a fight (doc 38): kind, type, side, how bad, rest left.
                    'injuries': [{k: i.get(k) for k in ('id', 'kind', 'type', 'side', 'severity', 'restLeft', 'restFull', 'from')}
                                 for i in data.get('injuries', []) if isinstance(i, dict)][:24],
                    # Marked a Dungeon Master in the game: they have the Dev Console.
                    'dungeonMaster': bool(master),
                    'stats': {k: data.get(k) for k in ('strength', 'dexterity', 'wisdom', 'stamina')},
                    'skills': {k: data.get(k) for k in ('sneakSkill', 'hearingSkill', 'scentSkill')},
                    # Each attribute and skill with its cap, and today's practice (doc 49).
                    'practice': practised(data, skills),
                    'account': account_view(standing.get(key)),
                    'person': person_view(persons.get(key)),
                    'circles': circles.get((persons.get(key) or {}).get('account'), []),
                    'stars': star_tallies.get((persons.get(key) or {}).get('account')),
                    'profile': profiles.get(key) if isinstance(profiles.get(key), dict) else None,
                    # How it was built (doc 49): grades that aren't plain, and its specialty.
                    'grades': {k: v for k, v in (data.get('grades') or {}).items() if v in ('weak', 'strong')}
                              if isinstance(data.get('grades'), dict) else {},
                    'specialty': data.get('specialty', '') if isinstance(data.get('specialty'), str) else '',
                    'senses': {k: data.get(k) for k in ('hearing', 'vision', 'smell')},
                    'saved': saved.isoformat()})
            actions = [{'id': r[0], 'kind': r[1], 'target': r[2], 'by': r[3], 'at': r[4].isoformat(), 'status': r[5], 'result': r[6]}
                       for r in conn.execute('''SELECT id, kind, target_id, requested_by, requested_at, status, result
                                                FROM dm.actions ORDER BY id DESC LIMIT 50''').fetchall()]
            # Ties (doc 52, migration 0042): newest first, with the two by name (a resident by its id).
            ties = []
            if conn.execute("SELECT to_regclass('game.ties')").fetchone()[0]:
                names = {c['id']: c['name'] for c in characters}
                rows = [row for (row,) in conn.execute('SELECT data FROM game.ties WHERE world_id = %s', (world[0],)).fetchall()]
                ties = [tie_view(row, names) for row in sorted((r for r in rows if isinstance(r, dict)),
                                                               key=lambda r: -float(r.get('created', 0) or 0))[:200]]
        return {'target': target, 'world': {'id': world[0], 'name': world[1]}, 'characters': characters, 'actions': actions,
                'ties': ties}

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
    def request(self, who, target, kind, character_id, reason='', payload=None):
        """Queues a live action for the game server. Returns its ID; the server writes back the outcome. A Gift
        (character.gift) carries {"gift": a playable family | "", "quickened": bool} (doc 43)."""
        needed = ACTIONS.get(kind)
        if not needed:
            raise DMError(f'Unknown action: {kind}.')
        detail = ''
        if kind == 'character.gift':
            gift = payload.get('gift', '') if isinstance(payload, dict) else None
            if (gift != '' and gift not in gift_families()) or not isinstance(payload.get('quickened', False), bool):
                raise DMError('A Gift is {"gift": one of ' + ', '.join(gift_families()) + ' or "", "quickened": true or false}.')
            payload = {'gift': gift, 'quickened': bool(payload.get('quickened')) and bool(gift)}
            detail = ' — ' + ('no Gift' if not gift else f'Quickened: {gift}' if payload['quickened'] else f'Gifted: {gift}')
        elif kind == 'character.injury':
            # An injury given ({"add": type, "severity": 1..3, "side": "left" | "right" | ""}) or taken away ({"remove": id}).
            p = payload if isinstance(payload, dict) else {}
            if p.get('add') in INJURY_TYPES:
                severity, side = p.get('severity', 2), p.get('side', '')
                if not isinstance(severity, int) or isinstance(severity, bool) or not 1 <= severity <= 3 or side not in ('', 'left', 'right'):
                    raise DMError('An injury is {"add": type, "severity": 1 to 3, "side": "left", "right" or ""}.')
                payload = {'add': p['add'], 'severity': severity, 'side': side}
                detail = f" — give {p['add'].replace('_', ' ')}" + (f' ({side})' if side else '')
            elif isinstance(p.get('remove'), str) and 0 < len(p['remove']) <= 96:
                payload = {'remove': p['remove']}
                detail = ' — take an injury away'
            else:
                raise DMError('Say {"add": a known injury} or {"remove": its id}.')
        elif kind == 'account.unlock':
            # An account's earned Gift tiers (doc 49, Phase 5), against one of its characters: grant or revoke a tier,
            # or hold and release new unlocks.
            p = payload if isinstance(payload, dict) else {}
            op, tier = p.get('op'), p.get('tier', '')
            if op not in ('grant', 'revoke', 'hold', 'release') or (op in ('grant', 'revoke') and tier not in ('gifted', 'quickened')):
                raise DMError('Say {"op": "grant" or "revoke", "tier": "gifted" or "quickened"}, or {"op": "hold" or "release"}.')
            payload = {'op': op, 'tier': tier if op in ('grant', 'revoke') else ''}
            detail = f' — {op}' + (f" {payload['tier']}" if payload['tier'] else '') + ' (their account)'
        elif kind == 'book.storyline':
            # A player's Story book tied to one of the world's storylines (doc 51, Phase 7: the World shelf), or untied.
            p = payload if isinstance(payload, dict) else {}
            book, storyline = p.get('book', character_id), p.get('storyline', '')
            if not isinstance(book, str) or not book.startswith('book-') or not isinstance(storyline, str) or len(storyline) > 120:
                raise DMError('Say {"book": "book-…", "storyline": "a storyline\'s name, or empty to untie it"}.')
            payload = {'book': book, 'storyline': storyline.strip()}
            detail = f" — tied to {payload['storyline']}" if payload['storyline'] else ' — untied from any storyline'
        elif kind == 'character.dm':
            on = payload.get('dungeonMaster') if isinstance(payload, dict) else None
            if not isinstance(on, bool):
                raise DMError('Say {"dungeonMaster": true or false}.')
            payload = {'dungeonMaster': on}
            detail = ' — ' + ('made a Dungeon Master in the game' if on else 'no longer a Dungeon Master in the game')
        elif kind == 'bandits.call':
            count = payload.get('count', 1) if isinstance(payload, dict) else 1
            if not isinstance(count, int) or isinstance(count, bool) or not 1 <= count <= 6:
                raise DMError('Call between one and six bandits.')
            payload = {'count': count}
            detail = f' — {count} bandit{"s" if count > 1 else ""} near them'
        else:
            payload = {}
        if RANK[who['role']] < RANK[needed]:
            raise DMError(f'Your role ({who["role"]}) cannot do that.', 403)
        with self.connect(target) as conn:
            with conn.transaction():
                exists = conn.execute('SELECT name FROM game.characters WHERE key = %s', (str(character_id),)).fetchone()
                if not exists:
                    raise DMError('No such character.', 404)
                action_id = conn.execute('''INSERT INTO dm.actions (kind, target_id, requested_by, payload) VALUES (%s, %s, %s, %s)
                                            RETURNING id''', (kind, character_id, who['username'], json.dumps(payload))).fetchone()[0]
                conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action_id}),))
                self.audit(conn, who['username'], kind, character_id,
                           f'{target.upper()}: {kind} {exists[0]}{detail}' + (f' — {reason[:500]}' if reason else ''))
        return {'id': action_id, 'status': 'queued'}

    def queue(self, conn, who, target, kind, target_id, detail):
        """Adds a live action for the game server and audits it (inside the caller's transaction)."""
        action_id = conn.execute('INSERT INTO dm.actions (kind, target_id, requested_by) VALUES (%s, %s, %s) RETURNING id',
                                 (kind, target_id, who['username'])).fetchone()[0]
        conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action_id}),))
        self.audit(conn, who['username'], kind, target_id, f'{target.upper()}: {detail}'[:2000])
        return action_id

    def live_action(self, who, target, kind, target_id, payload=None, reason=''):
        """A live action on something other than a character (town projects, deeds, nicknames, notices, places to let),
        queued for the game server with its payload checked, and audited."""
        if kind not in LIVE_KINDS:
            raise DMError('No such action.')
        self.allowed(who, kind)
        target_id = str(target_id or '').strip()
        if not target_id or len(target_id) > 80 or not PLAIN_ID.match(target_id):
            raise DMError('Say what it is for.')
        p = payload if isinstance(payload, dict) else {}
        clean = {}
        if kind == 'project.post':
            kinds = [k['id'] for k in project_rules().get('kinds', [])]
            if p.get('kind') not in kinds:
                raise DMError('No such kind of project.')
            clean = {'kind': p['kind'], 'title': str(p.get('title', ''))[:80]}
            if p.get('cell'):
                x, y = p.get('x'), p.get('y')
                if not isinstance(x, int) or not isinstance(y, int) or isinstance(x, bool) or isinstance(y, bool) or \
                        not 0 <= x < 4096 or not 0 <= y < 4096 or not PLAIN_ID.match(str(p['cell'])):
                    raise DMError('Give the tile as whole numbers.')
                clean.update({'cell': str(p['cell'])[:80], 'x': x, 'y': y})
        elif kind == 'deed.award':
            clean = {k: str(p.get(k, ''))[:160] for k in ('kind', 'weight', 'cell', 'beneficiary', 'detail') if p.get(k)}
        elif kind == 'storyline.give':
            ids = [t['id'] for t in storyline_templates()]
            cast = p.get('cast') if isinstance(p.get('cast'), dict) else {}
            if p.get('template') not in ids:
                raise DMError('No such story.')
            if len(cast) > 8 or not all(isinstance(k, str) and ID.match(k) and isinstance(v, str) and PLAIN_ID.match(v) and len(v) <= 80
                                        for k, v in cast.items()):
                raise DMError('The cast is roles and ids.')
            clean = {'template': p['template'], 'cast': cast}
        elif kind in ('storyteller.decide', 'storyteller.revoke'):
            reason = str(p.get('reason', '')).strip()
            if len(reason) > 400 or any(ord(c) < 32 for c in reason):
                raise DMError('A reason is at most 400 plain characters.')
            clean = {'reason': reason}
            if kind == 'storyteller.decide':
                if not isinstance(p.get('approve'), bool):
                    raise DMError('Approve or refuse.')
                clean['approve'] = p['approve']
        elif kind == 'storyline.tick':
            step, objective = p.get('step'), p.get('objective')
            if not all(isinstance(v, int) and not isinstance(v, bool) and 0 <= v < 16 for v in (step, objective)):
                raise DMError('Give the step and objective as numbers.')
            clean = {'step': step, 'objective': objective}
        with self.connect(target) as conn:
            with conn.transaction():
                action_id = conn.execute('''INSERT INTO dm.actions (kind, target_id, requested_by, payload) VALUES (%s, %s, %s, %s)
                                            RETURNING id''', (kind, target_id, who['username'], json.dumps(clean))).fetchone()[0]
                conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action_id}),))
                what = f" — {clean.get('kind', '')} {clean.get('title', '')}".rstrip() if clean else ''
                self.audit(conn, who['username'], kind, target_id,
                           f'{target.upper()}: {kind} {target_id}{what}' + (f' — {reason[:500]}' if reason else ''))
        return {'id': action_id, 'status': 'queued'}

    def storytellers(self, target):
        """Storytellers (doc 58, 11): applications with their notes, the approved and the rest; each storyteller's tales with
        their steps and participants; the kept log (narration and story characters' lines within 30 days); the visitors'
        list; the templates a DM may give."""
        with self.connect(target) as conn:
            if not conn.execute("SELECT to_regclass('game.storytellers') IS NOT NULL").fetchone()[0]:
                return {'target': target, 'ready': False, 'standing': [], 'tales': [], 'log': [], 'visitors': [], 'templates': []}
            world = C.world_of(conn)
            names = dict(conn.execute('SELECT key, name FROM game.characters WHERE world_id = %s', (world,)).fetchall())
            standing = [dict(r[0], name=names.get(r[0].get('character', ''), r[0].get('character', '')))
                        for r in conn.execute('SELECT data FROM game.storytellers WHERE world_id = %s', (world,)).fetchall()]
            tales = []
            for (d,) in conn.execute("SELECT data FROM game.storylines WHERE world_id = %s AND kind = 'tale' ORDER BY updated_at DESC LIMIT 100",
                                     (world,)).fetchall():
                tales.append({'id': d.get('id'), 'title': d.get('title'), 'premise': d.get('premise', ''), 'state': d.get('state'),
                              'author': d.get('author'), 'authorName': names.get(d.get('author', ''), d.get('author', '')),
                              'account': d.get('authorAccount'),
                              'participants': [{'id': k, 'name': names.get(k, k), 'left': v.get('left', -1) >= 0}
                                               for k, v in (d.get('participants') or {}).items()],
                              'steps': [{'title': s.get('title'), 'objectives': [{'line': o.get('line'), 'kind': o.get('kind'),
                                                                                    'by': names.get(o.get('doneBy', ''), o.get('doneBy', '')),
                                                                                    'byHand': o.get('byHand', False)}
                                                                                   for o in s.get('objectives') or []]}
                                        for s in d.get('steps') or []]})
            log = [r[0] for r in conn.execute('SELECT data FROM game.storyteller_log WHERE world_id = %s ORDER BY updated_at DESC LIMIT 300',
                                              (world,)).fetchall()]
            for e in log:
                e['name'] = names.get(e.get('character', ''), e.get('character', ''))
            visitors = [{'id': r[0], 'name': r[1], 'description': r[2], 'enabled': r[3], 'approvedBy': r[4] or ''}
                        for r in conn.execute('SELECT id, name, description, enabled, approved_by FROM live.story_visitors WHERE world_id = %s ORDER BY id',
                                              (world,)).fetchall()]
        log.sort(key=lambda e: -(e.get('at') or 0))
        return {'target': target, 'ready': True, 'standing': standing, 'tales': tales, 'log': log, 'visitors': visitors,
                'templates': [{'id': t['id'], 'title': t.get('title', t['id']), 'source': t.get('source', 'dm')} for t in storyline_templates()]}

    def save_story_visitor(self, who, target, visitor):
        """A visitor on the approved list (doc 58, 6), saved and approved by this DM; the game reads the list again."""
        self.allowed(who, 'visitors.sync')
        if not isinstance(visitor, dict):
            raise DMError('Describe the visitor.')
        vid, name = str(visitor.get('id', '')).strip(), str(visitor.get('name', '')).strip()
        description, enabled = str(visitor.get('description', '')).strip(), visitor.get('enabled', True)
        if not ID.match(vid) or not 1 <= len(name) <= 60 or len(description) > 400 or not isinstance(enabled, bool) or \
                any(ord(c) < 32 for c in name + description):
            raise DMError('A visitor needs an id (lower case), a name (60 letters) and a description (400).')
        like = str(visitor.get('like', '')).strip()
        with self.connect(target) as conn:
            with conn.transaction():
                world = C.world_of(conn)
                look = {}
                if like:
                    row = conn.execute('SELECT appearance FROM live.npcs WHERE world_id = %s AND id = %s', (world, like)).fetchone()
                    if not row:
                        raise DMError('No such resident to look like.')
                    look = row[0] or {}
                conn.execute('''INSERT INTO live.story_visitors (world_id, id, name, description, appearance, enabled, approved_by, approved_at)
                                VALUES (%s, %s, %s, %s, %s, %s, %s, now())
                                ON CONFLICT (world_id, id) DO UPDATE SET name = excluded.name, description = excluded.description,
                                    appearance = CASE WHEN %s THEN excluded.appearance ELSE live.story_visitors.appearance END,
                                    enabled = excluded.enabled, approved_by = excluded.approved_by, approved_at = now(), updated_at = now()''',
                             (world, vid, name, description, json.dumps(look), enabled, who['username'], bool(like)))
                action_id = self.queue(conn, who, target, 'visitors.sync', 'story-visitors', f'visitors.sync after saving {vid}')
        return {'id': action_id, 'status': 'queued'}

    def credit_milestone(self, who, target, story, milestone, weight='great', rule=None, people=None, main=None):
        """A world story's milestone credited (doc 58, 7): who took part, by a rule (a storyline's participants; everyone
        who acted at a place in a window), and each one's lines built from the ledger only (game.events: steps and parts
        done, fights, wolves tended, kills, contracts). The game checks every wolf, renders the lines per viewer, and keeps
        the screen 3 days to star."""
        self.allowed(who, 'milestone.credit')
        story, milestone = str(story or '').strip()[:80], str(milestone or '').strip()[:80]
        if not milestone or weight not in ('great', 'legendary'):
            raise DMError('Name the milestone, great or legendary.')
        rule = rule if isinstance(rule, dict) else {}
        lines = {}
        with self.connect(target) as conn:
            world = C.world_of(conn)
            chars = dict(conn.execute('SELECT key, name FROM game.characters WHERE world_id = %s', (world,)).fetchall())
            ids = [p for p in (people or []) if isinstance(p, str) and p in chars]
            if rule.get('kind') == 'storyline':
                row = conn.execute('SELECT data FROM game.storylines WHERE world_id = %s AND key = %s', (world, str(rule.get('storyline', '')))).fetchone()
                if not row:
                    raise DMError('No such storyline.')
                ids += [k for k in (row[0].get('participants') or {}) if k in chars]
                for actor, detail in conn.execute('''SELECT actor, detail FROM game.events WHERE world_id = %s AND target = %s
                                                     AND kind = 'storyline objective' ORDER BY id''', (world, row[0].get('id'))).fetchall():
                    lines.setdefault(actor, []).append((detail or 'did their part').split(' (by hand)')[0])
            elif rule.get('kind') == 'place':
                cell, since, until = str(rule.get('cell', '')), rule.get('from'), rule.get('to')
                if not cell or not isinstance(since, (int, float)) or not isinstance(until, (int, float)) or until < since:
                    raise DMError('A place, and a window in game days.')
                counts = {}
                for kind, actor, target_id in conn.execute('''SELECT kind, actor, target FROM game.events WHERE world_id = %s AND cell = %s
                                                                AND game_day BETWEEN %s AND %s''', (world, cell, since, until)).fetchall():
                    if actor in chars:
                        counts.setdefault(actor, {}).setdefault(kind, 0)
                        counts[actor][kind] += 1
                words = {'tended': 'tended {n} wolves', 'bandit falls': 'struck down {n} bandits', 'beaten down': 'beat down {n} foes',
                         'hunted': 'brought down {n} beasts', 'contract done': 'finished {n} jobs', 'conversation': 'talked with folk {n} times',
                         'assault': 'fought {n} times', 'storyline objective': 'did {n} parts of the story'}
                for actor, kinds in counts.items():
                    ids.append(actor)
                    lines[actor] = [words[k].format(n=n).replace(' 1 wolves', ' a wolf').replace(' 1 ', ' one ') for k, n in kinds.items() if k in words][:6]
            ids = list(dict.fromkeys(ids))[:40]
            if not ids:
                raise DMError('Nobody to credit.')
            payload = {'story': story, 'milestone': milestone, 'weight': weight,
                       'people': [{'id': i, 'lines': [l[:160] for l in lines.get(i, [])] or ['was there']} for i in ids],
                       'main': [m for m in (main or []) if m in ids][:3]}
            with conn.transaction():
                action_id = conn.execute('''INSERT INTO dm.actions (kind, target_id, requested_by, payload) VALUES ('milestone.credit', %s, %s, %s)
                                            RETURNING id''', (milestone[:80] or 'milestone', who['username'], json.dumps(payload))).fetchone()[0]
                conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action_id}),))
                self.audit(conn, who['username'], 'milestone.credit', milestone, f'{target.upper()}: credited {story} · {milestone} to {len(ids)} wolves')
        return {'id': action_id, 'status': 'queued', 'people': payload['people']}

    def projects(self, target):
        """Town projects (doc 57): each one open or standing, where, how far along, and its top givers by the names they
        chose."""
        with self.connect(target) as conn:
            rows = [r[0] for r in conn.execute(
                "SELECT data FROM game.projects WHERE data->>'state' IN ('open', 'built', 'worn', 'ruin')").fetchall()]
        out = []
        for d in rows:
            givers = {}
            for g in d.get('gifts') or []:
                givers[g.get('who')] = givers.get(g.get('who'), 0) + float(g.get('value') or 0)
            top = sorted(givers.items(), key=lambda kv: -kv[1])[:3]
            shown = d.get('shown') or {}
            out.append({'id': d.get('id'), 'kind': d.get('kind'), 'title': d.get('title'), 'town': d.get('town'),
                        'cell': d.get('cell'), 'x': d.get('x', 0), 'y': d.get('y', 0), 'state': d.get('state'),
                        'hours': d.get('hours', 0), 'worked': d.get('worked', 0), 'condition': d.get('condition', 100),
                        'needs': d.get('needs') or {}, 'namedFor': d.get('namedFor') or '',
                        'givers': [{'id': who, 'shown': shown.get(who) or 'a friend of the town', 'value': round(v)}
                                   for who, v in top]})
        return {'target': target, 'projects': out,
                'kinds': [{'id': k['id'], 'name': k['name']} for k in project_rules().get('kinds', [])]}

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

    # -- where the money is (Docs/Design/42-money-in-circulation.md, Phase 8) -------------
    MONEY_EVENTS = ('reckoning', 'surplus spent', 'trade caravan departs', 'trade caravan sells', 'sermon')

    def money(self, target):
        """The economy as last saved: each town's treasury, church and buyers; the great houses and their tills;
        residents' purses by kind of work, the poorest and richest tenth and how many are short of a day's food money;
        money on the road; and the latest reckonings, surplus spending, trade caravans and sermons."""
        with self.connect(target) as conn:
            world = C.world_of(conn)
            if not world:
                raise DMError(f'The {target.upper()} database has no world yet.', 404)
            project, _ = S.load_world(conn, world)
            row = conn.execute("SELECT payload::jsonb->'society', (payload::jsonb->>'calendarDays')::double precision "
                               'FROM game.checkpoints WHERE world_id = %s', (world,)).fetchone()
            events = [{'kind': r[0], 'actor': r[1], 'target': r[2], 'day': r[3], 'detail': r[4], 'at': r[5].isoformat()}
                      for r in conn.execute('SELECT kind, actor, target, game_day, detail, recorded_at FROM game.events '
                                            'WHERE world_id = %s AND kind = ANY(%s) ORDER BY id DESC LIMIT 60',
                                            (world, list(self.MONEY_EVENTS))).fetchall()]
        society = row[0] if row and row[0] else {}
        if isinstance(society, str):
            society = json.loads(society)
        accounts = {k: int(v.get('cash', 0)) for k, v in (society.get('accounts') or {}).items()}
        region = {a['id']: a.get('territory', {}).get('region', '') for a in project['cells'] + project['rooms']}
        people = {p['id']: p for p in project.get('people', [])}
        living = Counter(region.get(p['home']['cell'], '') for p in people.values())
        towns = {}
        for town, n in sorted(living.items(), key=lambda kv: -kv[1]):
            if not town or n < 5:
                continue
            buyers = {k.split(':')[-1]: v for k, v in accounts.items()
                      if k.startswith(f'town:{town}:') and not k.endswith((':church', ':granary'))}
            # Every town's church keeps one purse, the land's (doc 42, "One church"): 'church' is that purse, the same for all.
            towns[town] = {'residents': n, 'treasury': accounts.get(f'stores:{town}'), 'church': accounts.get('town:all:church', accounts.get(f'town:{town}:church')),
                           'buyers': buyers, 'condition': ((society.get('memory') or {}).get('condition') or {}).get(town)}
        # The orchestrator's funds, each town's by channel (fund:<town>:<channel>, '_' for spaces) and the land's
        # (fund:land), and the towns' granaries (doc 46, Phases 5 and 7).
        funds = defaultdict(dict)
        for k, v in accounts.items():
            if k.startswith('fund:') and k != 'fund:land' and ':' in k[5:]:
                town, channel = k[5:].rsplit(':', 1)
                funds[town][channel.replace('_', ' ')] = v
        granaries = {}
        for k, a in (society.get('accounts') or {}).items():
            if k.startswith('town:') and k.endswith(':granary') and len(k) > len('town::granary'):
                stock = a.get('stock') if isinstance(a.get('stock'), dict) else {}
                granaries[k[5:-8]] = {'cash': int(a.get('cash', 0)),
                                      'goods': sum(int(n) for n in stock.values() if isinstance(n, (int, float)))}
        houses = [{'id': k, 'cash': v} for k, v in sorted(accounts.items()) if k.startswith('house:')]
        tills = {k: v for k, v in accounts.items() if k.startswith('till:')}
        purses = sorted(v for k, v in accounts.items() if k in people)
        by_role = defaultdict(list)
        for k, v in accounts.items():
            if k in people:
                by_role[people[k]['role']].append(v)
        tenth = max(1, len(purses) // 10)

        def mean(xs):
            return round(sum(xs) / len(xs), 1) if xs else None
        residents = {'count': len(purses), 'total': sum(purses), 'median': purses[len(purses) // 2] if purses else None,
                     'poorestTenth': mean(purses[:tenth]), 'richestTenth': mean(purses[-tenth:]),
                     'shortOfFood': sum(1 for v in purses if v < 6),
                     'byRole': {r: {'count': len(v), 'total': sum(v), 'median': sorted(v)[len(v) // 2]} for r, v in by_role.items()}}
        road = {'caravans': sum(v for k, v in accounts.items() if k.startswith('caravan:')),
                'contracts': sum(v for k, v in accounts.items() if k.startswith('contract:')),
                'bandits': sum(v for k, v in accounts.items() if k.startswith('bandits:'))}
        return {'target': target, 'day': row[1] if row else None, 'total': sum(accounts.values()),
                'capital': accounts.get('treasury'), 'towns': towns, 'houses': houses,
                'tills': {'count': len(tills), 'total': sum(tills.values())}, 'residents': residents, 'road': road,
                'players': sum(v for k, v in accounts.items() if k.startswith(('wolf-', 'player-'))),
                'month': (society.get('books') or {}).get('month'), 'events': events,
                'funds': {t: dict(sorted(c.items())) for t, c in sorted(funds.items())}, 'landFund': accounts.get('fund:land'),
                'granaries': dict(sorted(granaries.items())),
                # The economy orchestrator's last plan (doc 46, Part 9); older saves have none.
                'orchestrator': society.get('orchestrator') if isinstance(society.get('orchestrator'), dict) else None}

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

    # -- steering the economy orchestrator (Docs/Design/46-economy-orchestrator.md, Part 10) -----------
    @staticmethod
    def steer_range(kind, strength):
        """Whether a steer's strength is within its kind's range: a holder is spared (0) or squeezed (1.5 to 4)."""
        if kind == 'holder':
            return strength == 0 or 1.5 <= strength <= 4
        return (0.5 if kind in ('pressure', 'price') else 0) <= strength <= 3

    def steer_towns(self, target):
        """The towns a steer may name: the save's towns and the ones only the orchestrator's brief knows."""
        money = self.money(target)
        return set(money['towns']) | {t.get('id') for t in (((money.get('orchestrator') or {}).get('brief') or {}).get('towns') or [])
                                      if isinstance(t, dict)}

    def check_steer(self, target, kind, target_id='', item='', strength=1.0, days=7, note='', towns=None):
        """Checks a steer and returns its payload and how the audit says it. `towns` (the towns a steer may name) is
        looked up when needed and not given."""
        if kind not in STEER_KINDS:
            raise DMError('A steer is pressure, town, holder, channel or price.')
        target_id, item = ('' if target_id is None else target_id), ('' if item is None else item)
        if not isinstance(target_id, str) or not isinstance(item, str) or not isinstance(note, str):
            raise DMError('Malformed steer.')
        target_id, item, note = target_id.strip(), item.strip(), note.strip()
        if isinstance(strength, bool) or not isinstance(strength, (int, float)) or strength != strength or strength in (float('inf'), float('-inf')):
            raise DMError('A steer\'s strength is a number.')
        strength = float(strength)
        if not self.steer_range(kind, strength):
            raise DMError({'pressure': 'Pressure on the land is from 0.5 (gentle) to 3 (hard).',
                           'town': 'A town is weighed from 0 to 3.',
                           'holder': 'A holder is spared (0) or squeezed (1.5 to 4).',
                           'channel': 'A channel is weighed from 0 (closed) to 3.',
                           'price': 'A price shock is from 0.5 (cheaper) to 3 (dearer).'}[kind])
        if not isinstance(days, int) or isinstance(days, bool) or not STEER_DAYS[0] <= days <= STEER_DAYS[1]:
            raise DMError(f'A steer lasts {STEER_DAYS[0]} to {STEER_DAYS[1]} days.')
        if len(note) > 200 or any(ord(c) < 32 or ord(c) == 127 for c in note):
            raise DMError('A note is at most 200 plain characters.')
        if kind != 'price' and item:
            raise DMError('Only a price shock names a good.')
        if kind == 'pressure':
            if target_id not in ('', 'land'):
                raise DMError('Pressure is on the whole land.')
            target_id = ''
        elif kind == 'holder':
            if not target_id or len(target_id) > 120 or not PLAIN_ID.fullmatch(target_id):
                raise DMError('Name the holder: an account such as house:fell or stores:ridgemere.')
        elif kind == 'channel':
            if target_id not in STEER_CHANNELS:
                raise DMError('A channel is one of ' + ', '.join(STEER_CHANNELS) + '.')
        else:                                     # town, price: a known town (or every town, for a price).
            if kind == 'price' and (not item or len(item) > 60 or not PLAIN_ID.fullmatch(item)):
                raise DMError('Name the good whose price is shocked.')
            if not (kind == 'price' and target_id == '*'):
                if towns is None:
                    towns = self.steer_towns(target)
                if not target_id or target_id not in towns:
                    raise DMError(f'No town called {target_id or "(none)"} in {target.upper()}.', 404)
        payload = {'kind': kind, 'target': target_id, 'item': item, 'strength': strength, 'days': days, 'note': note}
        what = {'pressure': 'pressure on the land', 'town': f'weigh {target_id}', 'holder': f'{"spare" if strength == 0 else "squeeze"} {target_id}',
                'channel': f'channel {target_id}', 'price': f'price of {item} in {"every town" if target_id == "*" else target_id}'}[kind]
        return payload, what

    def queue_steer(self, conn, who, target, payload, what):
        """Queues a checked steer (inside the caller's transaction) and audits it."""
        target_id, strength, days, note = payload['target'], payload['strength'], payload['days'], payload['note']
        action = conn.execute('''INSERT INTO dm.actions (kind, target_id, payload, requested_by)
                                 VALUES ('economy.steer', %s, %s, %s) RETURNING id''',
                              (target_id or 'land', json.dumps(payload), who['username'])).fetchone()[0]
        conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action}),))
        self.audit(conn, who['username'], 'economy.steer', target_id or 'land',
                   f'{target.upper()}: steer the economy, {what} x{strength:g} for {days} day{"s" if days != 1 else ""}'
                   + (f' — {note}' if note else ''))
        return {'id': action, 'status': 'queued', 'steer': f'steer-{action}'}

    def steer_economy(self, who, target, kind, target_id='', item='', strength=1.0, days=7, note=''):
        """Queues a steer for the economy orchestrator: it changes what the orchestrator weighs from the next day's plan
        for some days, and the orchestrator still decides. The game names it steer-<action id>."""
        self.allowed(who, 'economy.steer')
        payload, what = self.check_steer(target, kind, target_id, item, strength, days, note)
        with self.connect(target) as conn:
            with conn.transaction():
                return self.queue_steer(conn, who, target, payload, what)

    # -- scenarios: named bundles of steers (doc 46, Part 10 and Phase 8) --------------------------
    STAPLES_MAX = 12          # A scenario's "staples" names at most this many goods, the cheapest.

    @staticmethod
    def scenarios():
        """The scenarios a Dungeon Master can start (Data/Economy/scenarios.json)."""
        data = json.loads(SCENARIOS_FILE.read_text(encoding='utf-8'))
        return [s for s in data.get('scenarios', []) if isinstance(s, dict) and s.get('id')]

    @classmethod
    def staples(cls):
        """The goods a scenario's "staples" means: every plain food (catalogue price 3p or less, not a drink) and
        firewood, the cheapest STAPLES_MAX of them (by price, then id)."""
        items = json.loads((ROOT / 'Data/Items/items.json').read_text(encoding='utf-8'))['items']
        plain = [(i.get('price', 0), i['id']) for i in items
                 if isinstance(i.get('food'), dict) and i.get('drink') is not True and not i['food'].get('drink')
                 and i['food'].get('nourish', 0) > 0 and (i.get('price') or 0) <= 3]
        plain += [(i.get('price', 0), i['id']) for i in items if i.get('id') == 'firewood']
        return [item for _, item in sorted(set(plain))[:cls.STAPLES_MAX]]

    def start_scenario(self, who, target, scenario_id, town='', holder=''):
        """Starts a scenario: expands its steers ($town and $holder named; "staples" one price shock a good) and queues
        them all, or none if any is wrong."""
        self.allowed(who, 'economy.scenario')
        scenario = next((s for s in self.scenarios() if s.get('id') == scenario_id), None)
        if scenario is None or not isinstance(scenario_id, str):
            raise DMError(f'No scenario called {scenario_id}.', 404)
        needs = scenario.get('needs') or []
        town = (town or '').strip() if isinstance(town, str) else ''
        holder = (holder or '').strip() if isinstance(holder, str) else ''
        towns = None
        if 'town' in needs:
            towns = self.steer_towns(target)
            if not town or town not in towns:
                raise DMError(f'{scenario.get("name", scenario_id)} needs a town: no town called {town or "(none)"} in {target.upper()}.', 404 if town else 422)
        if 'holder' in needs and (not holder or len(holder) > 120 or not PLAIN_ID.fullmatch(holder)):
            raise DMError(f'{scenario.get("name", scenario_id)} needs a holder: an account such as house:fell.')
        note = f'Scenario: {scenario.get("name", scenario_id)}' + (f' ({town})' if 'town' in needs else '') + (f' ({holder})' if 'holder' in needs else '')
        checked = []
        for steer in scenario.get('steers') or []:
            target_id = str(steer.get('target', '')).replace('$town', town).replace('$holder', holder)
            item = steer.get('item', '') or ''
            for good in (self.staples() if item == 'staples' else [item]):
                checked.append(self.check_steer(target, steer.get('kind'), target_id, good, steer.get('strength', 1.0),
                                                steer.get('days', 7), note, towns))
        with self.connect(target) as conn:
            with conn.transaction():
                queued = [self.queue_steer(conn, who, target, payload, what) for payload, what in checked]
        return {'scenario': scenario_id, 'queued': queued}

    def end_steer(self, who, target, steer_id):
        """Queues the end of a steer before its time."""
        self.allowed(who, 'economy.unsteer')
        if not isinstance(steer_id, str) or not STEER_ID.fullmatch(steer_id):
            raise DMError('Name the steer to end (steer-<number>).')
        with self.connect(target) as conn:
            with conn.transaction():
                action = conn.execute('''INSERT INTO dm.actions (kind, target_id, payload, requested_by)
                                         VALUES ('economy.unsteer', %s, '{}', %s) RETURNING id''',
                                      (steer_id, who['username'])).fetchone()[0]
                conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action}),))
                self.audit(conn, who['username'], 'economy.unsteer', steer_id, f'{target.upper()}: end {steer_id}')
        return {'id': action, 'status': 'queued'}

    # -- uploaded portraits (Docs/Design/29-client-polish.md, phase 9) -------------
    def artwork(self, target):
        """Portraits waiting for a decision (new or reported), with the image, and the latest decisions."""
        with self.connect(target) as conn:
            world = C.world_of(conn)
            if not world:
                raise DMError(f'The {target.upper()} database has no world yet.', 404)
            if not conn.execute("SELECT to_regclass('game.artwork') IS NOT NULL").fetchone()[0]:
                return {'target': target, 'ready': False, 'pending': [], 'recent': [], 'actions': []}
            names = dict(conn.execute('SELECT key, name FROM game.characters WHERE world_id = %s', (world,)).fetchall())

            def row(r):
                out = {'id': r[0], 'account': r[1], 'character': r[2], 'name': names.get(r[2], r[2]), 'status': r[3],
                       'reason': r[4], 'reported': r[5], 'at': r[6].isoformat()}
                if len(r) > 7:
                    out['png'] = r[7]
                return out
            pending = [row(r) for r in conn.execute('''
                SELECT id, account, character_id, status, reason, reported, created_at, png_base64 FROM game.artwork
                WHERE world_id = %s AND status = 'pending' ORDER BY reported DESC, created_at LIMIT 50''', (world,)).fetchall()]
            recent = [row(r) for r in conn.execute('''
                SELECT id, account, character_id, status, reason, reported, coalesce(reviewed_at, created_at) FROM game.artwork
                WHERE world_id = %s AND status <> 'pending' ORDER BY coalesce(reviewed_at, created_at) DESC LIMIT 20''', (world,)).fetchall()]
            actions = [{'id': r[0], 'target': r[1], 'payload': r[2], 'by': r[3], 'at': r[4].isoformat(), 'status': r[5], 'result': r[6]}
                       for r in conn.execute('''SELECT id, target_id, payload, requested_by, requested_at, status, result
                                                FROM dm.actions WHERE kind = 'artwork.review' ORDER BY id DESC LIMIT 20''').fetchall()]
        return {'target': target, 'ready': True, 'pending': pending, 'recent': recent, 'actions': actions}

    def review_artwork(self, who, target, art_id, decision, reason=''):
        """Asks the game server to approve or reject a portrait; it tells the owner and stores the decision."""
        self.allowed(who, 'artwork.review')
        art_id, decision, reason = str(art_id), str(decision), str(reason or '').strip()
        if decision not in ('approve', 'reject'):
            raise DMError('Approve or reject.')
        if len(reason) > 400 or any(ord(c) < 32 for c in reason):
            raise DMError('A reason is at most 400 plain characters.')
        with self.connect(target) as conn:
            with conn.transaction():
                world = C.world_of(conn)
                found = conn.execute('SELECT character_id FROM game.artwork WHERE world_id = %s AND id = %s',
                                     (world, art_id)).fetchone() if world else None
                if not found:
                    raise DMError('No such portrait.', 404)
                action = conn.execute('''INSERT INTO dm.actions (kind, target_id, payload, requested_by)
                                         VALUES ('artwork.review', %s, %s, %s) RETURNING id''',
                                      (art_id, json.dumps({'decision': decision, 'reason': reason}), who['username'])).fetchone()[0]
                conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action}),))
                self.audit(conn, who['username'], 'artwork.review', art_id,
                           f'{target.upper()}: {decision} the portrait of {found[0]}' + (f' — {reason}' if reason else ''))
        return {'id': action, 'status': 'queued'}

    # -- players' reports (Docs/Design/50-player-card-friends-safety.md, Phase 2) ---
    def reports(self, target):
        """Reports, open ones first, each with its evidence, and each reported account's earlier reports and the decisions
        on them; plus how many accounts block each reported account (a count, never who)."""
        with self.connect(target) as conn:
            world = C.world_of(conn)
            if not world:
                raise DMError(f'The {target.upper()} database has no world yet.', 404)
            if not conn.execute("SELECT to_regclass('game.reports') IS NOT NULL").fetchone()[0]:
                return {'target': target, 'ready': False, 'reports': [], 'actions': []}
            names = dict(conn.execute('SELECT key, name FROM game.characters WHERE world_id = %s', (world,)).fetchall())
            rows = conn.execute('''SELECT id, created_at, reporter_account, reporter_character, reported_account, reported_character, kind,
                                          category, note, evidence, status, decided_by, decided_at, outcome, silence_hours
                                   FROM game.reports WHERE world_id = %s
                                   ORDER BY (status = 'open') DESC, created_at DESC LIMIT 200''', (world,)).fetchall()
            earlier = {}
            for r in rows:
                earlier.setdefault(r[4], []).append({'id': r[0], 'status': r[10], 'category': r[7], 'outcome': r[13] or ''})
            blocked_by = {}
            if conn.execute("SELECT to_regclass('game.safety_marks') IS NOT NULL").fetchone()[0]:
                blocked_by = dict(conn.execute('''SELECT target, count(*) FROM game.safety_marks WHERE world_id = %s AND kind = 'block'
                                                  GROUP BY target''', (world,)).fetchall())
            reports = [{'id': r[0], 'at': r[1].isoformat(), 'reporter': r[2], 'reporterName': names.get(r[3], r[3]),
                        'reported': r[4], 'reportedName': names.get(r[5], r[5]), 'reportedCharacter': r[5], 'kind': r[6],
                        'category': r[7], 'note': r[8], 'evidence': r[9] if isinstance(r[9], list) else [], 'status': r[10],
                        'decidedBy': r[11] or '', 'decidedAt': r[12].isoformat() if r[12] else None, 'outcome': r[13] or '',
                        'silenceHours': r[14], 'earlier': [e for e in earlier.get(r[4], []) if e['id'] != r[0]],
                        'blockedBy': blocked_by.get(r[4], 0)} for r in rows]
            actions = [{'id': a[0], 'target': a[1], 'payload': a[2], 'by': a[3], 'at': a[4].isoformat(), 'status': a[5], 'result': a[6]}
                       for a in conn.execute('''SELECT id, target_id, payload, requested_by, requested_at, status, result
                                                FROM dm.actions WHERE kind = 'report.decide' ORDER BY id DESC LIMIT 20''').fetchall()]
        return {'target': target, 'ready': True, 'reports': reports, 'actions': actions}

    def decide_report(self, who, target, report_id, decision, outcome='', hours=0, reason=''):
        """Asks the game server to uphold or dismiss a report: an upheld one with a note, a warning, or a silence of 1, 6,
        24 or 72 hours, which the game applies and confirms. Audited."""
        self.allowed(who, 'report.decide')
        report_id, decision, outcome, reason = str(report_id), str(decision), str(outcome or ''), str(reason or '').strip()
        if decision not in ('uphold', 'dismiss'):
            raise DMError('Uphold or dismiss.')
        if decision == 'uphold' and outcome not in ('note', 'warning', 'silence'):
            raise DMError('An upheld report ends in a note, a warning or a silence.')
        if decision == 'uphold' and outcome == 'silence' and hours not in (1, 6, 24, 72):
            raise DMError('A silence is 1, 6, 24 or 72 hours.')
        if len(reason) > 400 or any(ord(c) < 32 for c in reason):
            raise DMError('A reason is at most 400 plain characters.')
        with self.connect(target) as conn:
            with conn.transaction():
                world = C.world_of(conn)
                found = conn.execute('SELECT reported_character, reported_account FROM game.reports WHERE world_id = %s AND id = %s',
                                     (world, report_id)).fetchone() if world else None
                if not found:
                    raise DMError('No such report.', 404)
                payload = {'report': report_id, 'decision': decision, 'outcome': outcome if decision == 'uphold' else '',
                           'hours': hours if decision == 'uphold' and outcome == 'silence' else 0}
                action = conn.execute('''INSERT INTO dm.actions (kind, target_id, payload, requested_by)
                                         VALUES ('report.decide', %s, %s, %s) RETURNING id''',
                                      (found[0], json.dumps(payload), who['username'])).fetchone()[0]
                conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action}),))
                self.audit(conn, who['username'], 'report.decide', report_id,
                           f'{target.upper()}: {decision} the report against {found[1]}' +
                           (f' ({outcome}{f", {hours} h" if outcome == "silence" else ""})' if decision == 'uphold' else '') +
                           (f' — {reason}' if reason else ''))
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

    # -- Chapters (Docs/Design/32-parties-chapters-factions.md): camps, Holds, treaties and House requests -------------
    def chapters(self, target):
        """Every Chapter as last saved, its camps and their buildings (placed on the world map), treaties, levies and
        House requests, and the recent decisions sent to the game server. Pending ones wait for a Dungeon Master for
        a game day; then the faction's own rule decides."""
        with self.connect(target) as conn:
            world = C.world_of(conn)
            if not world:
                raise DMError(f'The {target.upper()} database has no world yet.', 404)
            if not conn.execute("SELECT to_regclass('game.chapters') IS NOT NULL").fetchone()[0]:
                raise DMError(f'The {target.upper()} database needs migrating (python3 tools/world_db.py migrate).', 409)
            rows = lambda table: [r[0] for r in conn.execute(f'SELECT data FROM game.{table} WHERE world_id = %s ORDER BY position',
                                                               (world,)).fetchall()]
            places = {r['id']: r for r in S.dict_rows(conn, 'SELECT id, name, x, y FROM world.cells WHERE world_id = %s', (world,))}
            names = {r[0]: r[1] for r in conn.execute(
                'SELECT id, name FROM live.factions WHERE world_id = %s UNION ALL SELECT key, name FROM game.characters WHERE world_id = %s '
                'UNION ALL SELECT id, name FROM live.npcs WHERE world_id = %s', (world, world, world)).fetchall()}
            chapters = [{'id': c.get('id', ''), 'name': c.get('name', ''), 'colour': c.get('colour', ''), 'level': c.get('level', 1),
                         'renown': c.get('renown', 0), 'charter': c.get('charter', ''),
                         'members': [{'id': m.get('id', ''), 'name': names.get(m.get('id'), m.get('id', '')), 'rank': m.get('rank', 3)}
                                     for m in c.get('members', [])],
                         'hold': c.get('claimCell', ''), 'holdName': places.get(c.get('claimCell'), {}).get('name', ''),
                         'houseOf': c.get('houseOf', ''), 'toll': c.get('toll', 0), 'sworn': len(c.get('sworn', []))}
                        for c in rows('chapters')]
            structures, staff = {}, {}
            for st in rows('camp_structures'):
                structures.setdefault(st.get('site'), []).append(
                    {'id': st.get('id', ''), 'kind': st.get('kind', ''), 'x': st.get('x', 0), 'y': st.get('y', 0),
                     'built': bool(st.get('built')), 'condition': st.get('condition', 100)})
            for h in rows('camp_staff'):
                staff.setdefault(h.get('site'), []).append({'npc': h.get('npc', ''), 'name': names.get(h.get('npc'), h.get('npc', '')),
                                                            'role': h.get('role', ''), 'wage': h.get('wage', 0)})
            sites = []
            for s in rows('camp_sites'):
                place = places.get(s.get('cell'))
                sites.append({'id': s.get('id', ''), 'chapter': s.get('chapter', ''), 'name': s.get('name', ''), 'cell': s.get('cell', ''),
                              'place': place['name'] if place else s.get('cell', ''), 'state': s.get('state', 'standing'),
                              'x': s.get('x', 0), 'y': s.get('y', 0),
                              # Buildings sit at cellX + x, cellY + y on the world map.
                              'cellX': place['x'] if place else None, 'cellY': place['y'] if place else None,
                              'structures': structures.get(s.get('id'), []), 'staff': staff.get(s.get('id'), [])})
            named = lambda d: dict(d, factionName=names.get(d.get('faction'), d.get('faction', '')))
            treaties = [named(t) for t in rows('treaties')]
            houses = [named(h) for h in rows('house_requests')]
            levies = [named(l) for l in rows('levies')]
            actions = [{'id': r[0], 'kind': r[1], 'target': r[2], 'by': r[3], 'at': r[4].isoformat(), 'status': r[5], 'result': r[6],
                        'payload': r[7]}
                       for r in conn.execute("SELECT id, kind, target_id, requested_by, requested_at, status, result, payload "
                                             "FROM dm.actions WHERE kind IN ('treaty.decide', 'house.decide') ORDER BY id DESC LIMIT 40").fetchall()]
        return {'target': target, 'world': world, 'chapters': chapters, 'sites': sites, 'treaties': treaties, 'houses': houses,
                'levies': levies, 'actions': actions}

    def decide(self, who, target, what, ident, approve, faction='', reason=''):
        """Approves or refuses a pending treaty (by its ID) or House request (a Chapter's, to a faction). The game
        server checks it is still pending and tells the Chapter."""
        kind = {'treaty': 'treaty.decide', 'house': 'house.decide'}.get(str(what))
        if not kind:
            raise DMError('Decide a treaty or a House request.')
        self.allowed(who, kind)
        ident, faction, reason = str(ident), str(faction or ''), str(reason or '').strip()
        if not isinstance(approve, bool):
            raise DMError('Approve is true or false.')
        if len(reason) > 400 or any(ord(c) < 32 for c in reason):
            raise DMError('A reason is at most 400 plain characters.')
        with self.connect(target) as conn:
            with conn.transaction():
                world = C.world_of(conn)
                if kind == 'treaty.decide':
                    found = conn.execute("SELECT 1 FROM game.treaties WHERE world_id = %s AND key = %s AND state = 'pending'",
                                         (world, ident)).fetchone() if world else None
                    words = f"{'approve' if approve else 'refuse'} treaty {ident}"
                    payload = {'approve': approve}
                else:
                    found = conn.execute("SELECT 1 FROM game.house_requests WHERE world_id = %s AND chapter = %s "
                                         "AND data->>'faction' = %s AND state = 'pending'", (world, ident, faction)).fetchone() if world else None
                    words = f"{'approve' if approve else 'refuse'} {ident} as a House of {faction}"
                    payload = {'approve': approve, 'faction': faction}
                if not found:
                    raise DMError('Nothing pending by that name (it may have been decided already).', 404)
                action = conn.execute('INSERT INTO dm.actions (kind, target_id, payload, requested_by) VALUES (%s, %s, %s, %s) RETURNING id',
                                      (kind, ident, json.dumps(payload), who['username'])).fetchone()[0]
                conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action}),))
                self.audit(conn, who['username'], kind, ident, f'{target.upper()}: {words}' + (f' — {reason}' if reason else ''))
        return {'id': action, 'status': 'queued'}

    # -- server health (Docs/Design/31-responsiveness.md, the health tracker) ----------------
    def health(self, target, hours):
        """How the game server has run: each minute's window as a compact series, the slowest ticks, and the players
        with the worst ping (dm.health, migration 0032; written by the game server, tools/perf_report.py reads it too)."""
        hours = max(0.25, min(float(hours), 24 * 14))
        with self.connect(target) as conn:
            world = conn.execute('SELECT id FROM world.worlds').fetchone()
            if not world:
                raise DMError(f'The {target.upper()} database has no world yet.', 404)
            if not conn.execute("SELECT to_regclass('dm.health') IS NOT NULL").fetchone()[0]:
                return {'target': target, 'hours': hours, 'missing': True, 'windows': [], 'spikes': [], 'players': []}
            rows = conn.execute('SELECT at, kind, body FROM dm.health WHERE world_id = %s AND at > now() - make_interval(secs => %s) '
                                'ORDER BY at', (world[0], hours * 3600)).fetchall()
        windows, spikes, worst = [], [], {}
        for at, kind, body in rows:
            b = body if isinstance(body, dict) else json.loads(body)
            if kind == 'window':
                tick, ping, backlog = b.get('tick', {}), b.get('ping', {}), b.get('backlog', {})
                reporting = bool(ping.get('reporting'))
                windows.append({'at': at.isoformat(), 'clients': b.get('clients', 0), 'mean': tick.get('mean', 0),
                                'p99': tick.get('p99', 0), 'max': tick.get('max', 0), 'over50': tick.get('over50', 0),
                                'ping': ping.get('p50', 0) if reporting else None,
                                'ping95': ping.get('p95', 0) if reporting else None,
                                'slow': backlog.get('slow', 0), 'outMbps': b.get('traffic', {}).get('outMbps', 0),
                                'corrections': b.get('corrections', 0)})
                who = ping.get('worstWho')
                if who and ping.get('worst', 0) > worst.get(who, {}).get('ms', 0):
                    worst[who] = {'name': who, 'ms': ping['worst'], 'at': at.isoformat()}
            elif kind == 'spike':
                parts = sorted(((ms, name) for name, ms in b.get('parts', {}).items() if ms >= 1), reverse=True)
                spikes.append({'at': at.isoformat(), 'ms': b.get('ms', 0), 'clients': b.get('clients', 0),
                               'parts': [[name, ms] for ms, name in parts[:4]], 'note': b.get('note', '')})
        spikes.sort(key=lambda s: -s['ms'])
        return {'target': target, 'hours': hours, 'missing': False, 'windows': windows, 'spikes': spikes[:25],
                'players': sorted(worst.values(), key=lambda w: -w['ms'])[:10]}

    # -- the LIVE map (Docs/Design/34-dungeon-master-refresh.md, 1.1) ----------------
    WATCH_SECONDS = 60
    QUIET_EVENTS = ('economy', 'operator', 'conversation')

    def live(self, who, target):
        """The newest frame of everyone's position, how old it is, notable recent events and the day's moves and NPC saves. Asking
        keeps this account watching for a minute: the game server writes frames only while someone is."""
        with self.connect(target) as conn:
            world = conn.execute('SELECT id FROM world.worlds').fetchone()
            if not world:
                raise DMError(f'The {target.upper()} database has no world yet.', 404)
            with conn.transaction():
                conn.execute('''INSERT INTO dm.watchers (username, until) VALUES (%s, now() + make_interval(secs => %s))
                                ON CONFLICT (username) DO UPDATE SET until = excluded.until''', (who['username'], self.WATCH_SECONDS))
            row = conn.execute('''SELECT frame, extract(epoch FROM now() - written_at) FROM dm.watch WHERE world_id = %s''',
                               (world[0],)).fetchone()
            frame = None
            if row:
                try:
                    frame = json.loads(row[0])
                except ValueError:
                    frame = None
            events = [{'id': r[0], 'kind': r[1], 'actor': r[2], 'target': r[3], 'cell': r[4], 'day': r[5], 'detail': r[6],
                       'at': r[7].isoformat()}
                      for r in conn.execute('''SELECT id, kind, actor, target, cell, game_day, detail, recorded_at FROM game.events
                                               WHERE world_id = %s AND cell <> '' AND kind <> ALL(%s)
                                               ORDER BY id DESC LIMIT 80''', (world[0], list(self.QUIET_EVENTS))).fetchall()]
            actions = [{'id': r[0], 'kind': r[1], 'target': r[2], 'by': r[3], 'at': r[4].isoformat(), 'status': r[5], 'result': r[6]}
                       for r in conn.execute('''SELECT id, kind, target_id, requested_by, requested_at, status, result FROM dm.actions
                                                WHERE kind IN ('npc.move', 'character.move', 'npc.sync', 'visitor.add',
                                                               'visitor.leave', 'npc.revive', 'character.resurrect')
                                                      AND requested_at > now() - interval '1 day'
                                                ORDER BY id DESC LIMIT 30''').fetchall()]
        return {'target': target, 'frame': frame, 'age': round(float(row[1]), 1) if row else None, 'events': events,
                'actions': actions}

    def move(self, who, target, kind, someone, cell, x, y, reason=''):
        """Puts an NPC (npc.move) or a player character (character.move) on a tile; the game server checks the tile."""
        if kind not in ('npc.move', 'character.move'):
            raise DMError('Move an NPC (npc.move) or a character (character.move).')
        self.allowed(who, kind)
        if not someone or not cell or not all(isinstance(v, int) and not isinstance(v, bool) and 0 <= v < 4096 for v in (x, y)):
            raise DMError('Say who, and a tile: a place and whole-number x and y.')
        with self.connect(target) as conn:
            place = conn.execute('''SELECT name FROM world.cells WHERE id = %s UNION ALL SELECT name FROM world.interiors WHERE id = %s
                                    LIMIT 1''', (cell, cell)).fetchone()
            if not place:
                raise DMError('No such place.', 404)
            with conn.transaction():
                action_id = conn.execute('''INSERT INTO dm.actions (kind, target_id, requested_by, payload) VALUES (%s, %s, %s, %s)
                                            RETURNING id''', (kind, someone, who['username'],
                                                               json.dumps({'cell': cell, 'x': x, 'y': y}))).fetchone()[0]
                conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action_id}),))
                self.audit(conn, who['username'], kind, someone,
                           f'{target.upper()}: moved {someone} to {place[0]} {x}, {y}' + (f' — {reason[:500]}' if reason else ''))
        return {'id': action_id, 'status': 'queued'}

    def visit(self, who, target, name, cell, x, y, minutes, like='', description=''):
        """Brings a temporary visitor onto a tile for a while (visitor.add): never saved, gone when their time is up.
        They may look like a resident (`like`), and take that resident's description when given none."""
        self.allowed(who, 'visitor.add')
        name = ' '.join(str(name).split())
        if not 1 <= len(name) <= 60:
            raise DMError('Give them a name (up to 60 letters).')
        if not isinstance(minutes, int) or isinstance(minutes, bool) or not 1 <= minutes <= 24 * 60:
            raise DMError('They stay between a minute and a day (whole minutes).')
        if not cell or not all(isinstance(v, int) and not isinstance(v, bool) and 0 <= v < 4096 for v in (x, y)):
            raise DMError('Say where: a place and whole-number x and y.')
        visitor = f'visitor_{int(self.clock() * 1000):x}{secrets.token_hex(2)}'
        payload = {'name': name, 'cell': cell, 'x': x, 'y': y, 'minutes': minutes, 'like': str(like)[:80]}
        if description:
            payload['description'] = str(description)[:400]
        with self.connect(target) as conn:
            place = conn.execute('''SELECT name FROM world.cells WHERE id = %s UNION ALL SELECT name FROM world.interiors WHERE id = %s
                                    LIMIT 1''', (cell, cell)).fetchone()
            if not place:
                raise DMError('No such place.', 404)
            with conn.transaction():
                action_id = conn.execute('''INSERT INTO dm.actions (kind, target_id, requested_by, payload) VALUES ('visitor.add', %s, %s, %s)
                                            RETURNING id''', (visitor, who['username'], json.dumps(payload))).fetchone()[0]
                conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action_id}),))
                self.audit(conn, who['username'], 'visitor.add', visitor,
                           f'{target.upper()}: {name} visits {place[0]} {x}, {y} for {minutes} min')
        return {'id': action_id, 'visitor': visitor, 'status': 'queued'}

    def leave(self, who, target, visitor):
        """Sends a temporary visitor away before their time (visitor.leave)."""
        self.allowed(who, 'visitor.leave')
        if not str(visitor).startswith('visitor_'):
            raise DMError('Only a temporary visitor can be sent away.')
        with self.connect(target) as conn:
            with conn.transaction():
                return {'id': self.queue(conn, who, target, 'visitor.leave', str(visitor), f'sent {visitor} away'), 'status': 'queued'}

    def rumours(self, target):
        """What is going round: each rumour (who it is about, and what is said), with everyone who has heard it, most
        widely heard first (game.beliefs, as last saved). A deed (doc 56) is told in its own words."""
        with self.connect(target) as conn:
            rows = conn.execute('''SELECT subject, data->>'claim', array_agg(holder ORDER BY holder), avg((data->>'confidence')::float)
                                     FROM game.beliefs WHERE subject IS NOT NULL GROUP BY 1, 2
                                     ORDER BY count(*) DESC LIMIT 60''').fetchall()
            words = deed_words(conn, [r[1][5:] for r in rows if (r[1] or '').startswith('deed:')])
        claim = lambda c: words.get(c[5:], 'did a good deed') if (c or '').startswith('deed:') else (c or '')
        return {'target': target, 'rumours': [{'subject': r[0], 'claim': claim(r[1]), 'holders': r[2], 'sure': round(r[3] or 0, 2)}
                                              for r in rows]}

    def fame(self, target):
        """Good deeds (doc 56): each live deed, its doers and witnesses, and how far word of it has got round each town
        (the game's own rule, from Data/Fame/deeds.json), heaviest and newest first."""
        with self.connect(target) as conn:
            rows = [r[0] for r in conn.execute("SELECT data FROM game.deeds WHERE NOT coalesce((data->>'revoked')::boolean, false)").fetchall()]
            now = conn.execute('SELECT max(game_day) FROM game.events').fetchone()[0] or 0
            words = deed_words(conn, [d.get('id', '') for d in rows])
        out = []
        for d in rows:
            weight = int(d.get('weight') or 0)
            out.append({'id': d.get('id'), 'kind': d.get('kind'), 'weight': WEIGHTS[min(max(weight, 0), 3)],
                        'phrase': words.get(d.get('id', ''), d.get('kind', '')), 'doers': d.get('doers') or [],
                        'beneficiary': d.get('beneficiary') or '', 'town': d.get('town') or '', 'cell': d.get('cell') or '',
                        'day': d.get('day') or 0, 'names': d.get('names') or {},
                        'witnesses': [{'id': w.get('id'), 'as': w.get('as') or {}} for w in d.get('witnesses') or []],
                        'towns': [{'town': t.get('town'), 'carrier': t.get('carrier'), 'reach': round(deed_reach(t, weight, now), 2)}
                                  for t in d.get('towns') or []]})
        out.sort(key=lambda d: (-WEIGHTS.index(d['weight']), -d['day']))
        return {'target': target, 'day': now, 'deeds': out}

    def action(self, target, action_id):
        with self.connect(target) as conn:
            row = conn.execute('SELECT status, result, done_at FROM dm.actions WHERE id = %s', (int(action_id),)).fetchone()
        if not row:
            raise DMError('No such action.', 404)
        return {'id': int(action_id), 'status': row[0], 'result': row[1], 'doneAt': row[2].isoformat() if row[2] else None}


# --------------------------------------------------------------------------- Town projects (doc 57)

# Live actions on things other than characters, sent through /api/live/action.
LIVE_KINDS = ('project.post', 'project.cancel', 'project.complete', 'project.remove', 'npc.protect', 'npc.unprotect',
              'storyline.give', 'storyline.tick', 'storyteller.decide', 'storyteller.revoke', 'tale.pause', 'tale.resume',
              'tale.stop', 'visitors.sync',
              'deed.award', 'deed.revoke', 'nickname.drop', 'nickname.restore', 'board.remove', 'estate.clear', 'estate.release')


def storyline_templates() -> list:
    try:
        return json.loads((ROOT / 'Data/Storylines/templates.json').read_text(encoding='utf-8')).get('templates', [])
    except (OSError, ValueError):
        return []


def project_rules() -> dict:
    try:
        return json.loads((ROOT / 'Data/Town/projects.json').read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return {}


# --------------------------------------------------------------------------- Fame (doc 56)

WEIGHTS = ['small', 'notable', 'great', 'legendary']


def fame_rules() -> dict:
    try:
        return json.loads((ROOT / 'Data/Fame/deeds.json').read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return {}


def deed_reach(word: dict, weight: int, now: float, rules: dict | None = None) -> float:
    """How far word of a deed has got round a town (0..1): the game's RatwFame.cpp `reach`, from the same data."""
    w = (rules or fame_rules()).get('word', {})
    carrier = word.get('carrier') or ''
    caravan, legend = carrier.startswith('caravan'), carrier == 'legend'
    start = w.get('legendStart', .5) if legend else w.get('caravanStart', .15) if caravan else w.get('start', .25)
    days = max(.01, w.get('days', 3)) * (2 if caravan else 1)
    age = max(0.0, now - float(word.get('since') or 0))
    grown = min(1.0, start + (1 - start) * age / days)
    if weight >= 3:
        return grown
    name = WEIGHTS[min(max(weight, 0), 3)]
    fresh, fade = w.get('fresh', {}).get(name, 92), max(.01, w.get('fade', {}).get(name, 92))
    return grown * max(0.0, 1 - (age - fresh) / fade) if age > fresh else grown


def deed_words(conn, ids: list[str]) -> dict:
    """Each deed's phrase, as its game.events row tells it ("drove the bandits off the road at the ford (deed-3)")."""
    if not ids:
        return {}
    rows = conn.execute("SELECT detail FROM game.events WHERE kind = 'deed' AND detail LIKE ANY(%s)",
                        ([f'% ({i})' for i in ids],)).fetchall()
    out = {}
    for (detail,) in rows:
        phrase, _, rest = (detail or '').rpartition(' (')
        out[rest.rstrip(')')] = phrase
    return out


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
            if method == 'GET' and path == '/api/health':
                try:
                    hours = float((query.get('hours') or ['24'])[0])
                except ValueError:
                    hours = 24
                return self.reply(200, dm.health(self.target(query), hours))
            if method == 'GET' and path == '/api/world':
                return self.reply(200, dm.world_map(self.target(query), lean=query.get('lean') == ['1']))
            if method == 'GET' and path == '/api/ground':
                return self.reply(200, dm.ground(self.target(query), [i for i in ','.join(query.get('cells', [])).split(',') if i]))
            if method == 'POST' and path == '/api/actions':
                data = self.body()
                return self.reply(200, dm.request(who, str(data.get('target', 'prod')), str(data.get('kind', '')),
                                                  str(data.get('characterId', '')), str(data.get('reason', '')),
                                                  data.get('payload')))
            if method == 'GET' and path == '/api/live':
                return self.reply(200, dm.live(who, self.target(query)))
            if method == 'POST' and path == '/api/live/move':
                data = self.body()
                return self.reply(200, dm.move(who, str(data.get('target', 'prod')), str(data.get('kind', '')), str(data.get('id', '')),
                                               str(data.get('cell', '')), data.get('x'), data.get('y'), str(data.get('reason', ''))))
            if method == 'GET' and path == '/api/live/rumours':
                return self.reply(200, dm.rumours(self.target(query)))
            if method == 'GET' and path == '/api/live/fame':
                return self.reply(200, dm.fame(self.target(query)))
            if method == 'GET' and path == '/api/storytellers':
                return self.reply(200, dm.storytellers(self.target(query)))
            if method == 'POST' and path == '/api/storytellers/visitor':
                data = self.body()
                return self.reply(200, dm.save_story_visitor(who, str(data.get('target', 'prod')), data.get('visitor')))
            if method == 'POST' and path == '/api/milestone':
                data = self.body()
                return self.reply(200, dm.credit_milestone(who, str(data.get('target', 'prod')), data.get('story'), data.get('milestone'),
                                                           data.get('weight', 'great'), data.get('rule'), data.get('people'), data.get('main')))
            if method == 'GET' and path == '/api/live/projects':
                return self.reply(200, dm.projects(self.target(query)))
            if method == 'POST' and path == '/api/live/action':
                data = self.body()
                return self.reply(200, dm.live_action(who, str(data.get('target', 'prod')), str(data.get('kind', '')),
                                                      data.get('id', ''), data.get('payload'), str(data.get('reason', ''))))
            if method == 'POST' and path == '/api/live/visit':
                data = self.body()
                return self.reply(200, dm.visit(who, str(data.get('target', 'prod')), data.get('name', ''), str(data.get('cell', '')),
                                                data.get('x'), data.get('y'), data.get('minutes'), str(data.get('like', '')),
                                                str(data.get('description', ''))))
            if method == 'POST' and path == '/api/live/leave':
                data = self.body()
                return self.reply(200, dm.leave(who, str(data.get('target', 'prod')), str(data.get('id', ''))))
            if method == 'GET' and path == '/api/money':
                return self.reply(200, dm.money(self.target(query)))
            if method == 'GET' and path == '/api/calendar':
                return self.reply(200, dm.calendar(self.target(query)))
            if method == 'POST' and path == '/api/festivals/call':
                data = self.body()
                return self.reply(200, dm.call_festival(who, str(data.get('target', 'prod')), data.get('community', ''),
                                                        data.get('name', ''), data.get('inDays', 0)))
            if method == 'POST' and path == '/api/economy/steer':
                data = self.body()
                return self.reply(200, dm.steer_economy(who, str(data.get('target', 'prod')), data.get('kind'), data.get('targetId', ''),
                                                        data.get('item', ''), data.get('strength'), data.get('days'),
                                                        data.get('note', '')))
            if method == 'POST' and path == '/api/economy/unsteer':
                data = self.body()
                return self.reply(200, dm.end_steer(who, str(data.get('target', 'prod')), data.get('id')))
            if method == 'GET' and path == '/api/economy/scenarios':
                return self.reply(200, {'scenarios': dm.scenarios()})
            if method == 'POST' and path == '/api/economy/scenario':
                data = self.body()
                return self.reply(200, dm.start_scenario(who, str(data.get('target', 'prod')), data.get('scenario'),
                                                         data.get('town', ''), data.get('holder', '')))
            if method == 'GET' and path == '/api/artwork':
                return self.reply(200, dm.artwork(self.target(query)))
            if method == 'POST' and path == '/api/artwork/review':
                data = self.body()
                return self.reply(200, dm.review_artwork(who, str(data.get('target', 'prod')), str(data.get('id', '')),
                                                         str(data.get('decision', '')), str(data.get('reason', ''))))
            if method == 'GET' and path == '/api/reports':
                return self.reply(200, dm.reports(self.target(query)))
            if method == 'POST' and path == '/api/reports/decide':
                data = self.body()
                hours = data.get('hours', 0)
                return self.reply(200, dm.decide_report(who, str(data.get('target', 'prod')), str(data.get('id', '')),
                                                        str(data.get('decision', '')), str(data.get('outcome', '')),
                                                        hours if isinstance(hours, int) and not isinstance(hours, bool) else 0,
                                                        str(data.get('reason', ''))))
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
            if method == 'GET' and path == '/api/chapters':
                return self.reply(200, dm.chapters(self.target(query)))
            if method == 'POST' and path == '/api/chapters/decide':
                data = self.body()
                return self.reply(200, dm.decide(who, target(data), data.get('what'), data.get('id', ''), data.get('approve'),
                                                 data.get('faction', ''), data.get('reason', '')))
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
