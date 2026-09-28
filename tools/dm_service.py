#!/usr/bin/env python3
"""Storykeeper: trusted-local DM planning, audit, and opt-in authority bridge.

Keeps its state in the dm schema of the world database (DEV unless --database
prod; needs psycopg). Never touches the game's own save tables. HTTP APIs are
loopback-only and bearer authenticated; native effects cross a private file bridge.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import hmac
import ipaddress
import json
import math
import os
from pathlib import Path
import re
import secrets
import stat
import sys
import threading
import time
from datetime import datetime, timedelta, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit, unquote
import uuid

import http_body

ROOT = Path(__file__).resolve().parent.parent
ACTIVITY = 'chapter, weekday, hour, activeminutes AS "activeMinutes", memberminutes AS "memberMinutes", samples'


class PgState:
    """The Storykeeper's tables in PostgreSQL (schema dm, migration 0012), used the way it used SQLite:
    execute() with ? placeholders returning dict rows, and `with db:` for one transaction (nesting is fine)."""

    def __init__(self, conn):
        from psycopg.rows import dict_row
        self.conn = conn
        self.conn.row_factory = dict_row
        self._open = []

    def execute(self, sql, params=()):
        return self.conn.execute(sql.replace('?', '%s'), params)

    def __enter__(self):
        transaction = self.conn.transaction()
        transaction.__enter__()
        self._open.append(transaction)
        return self

    def __exit__(self, *exc):
        return self._open.pop().__exit__(*exc)

    def close(self):
        self.conn.close()


def database_errors():
    import psycopg
    return psycopg.Error


# Where the Storykeeper keeps its state: the dm schema of the DEV database unless told otherwise
# (--database prod). Tests point this at a scratch database.
DATABASE = 'dev'


def default_connect():
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import world_db
    return world_db.connect(DATABASE, 'game', options='-c search_path=dm')


DEFAULT_CONNECT = default_connect
WEATHERS = ('clear', 'overcast', 'rain', 'storm', 'fog', 'snow', 'sandstorm')
MAX_BODY = 65536
MAX_SNAPSHOT = 8 * 1024 * 1024
STALE_SECONDS = 10
REQUEST_TTL = 120
PREVIEW_TTL = 300
ARRIVAL_TIMEOUT = 600
MIGRATION_COOLDOWN = 86400
ID = re.compile(r'[A-Za-z0-9][A-Za-z0-9_-]{0,79}\Z')
NATIVE = {'notice', 'weather', 'npc_relocate', 'economy_transfer'}
STORY_ONLY = {'brigands', 'assassins', 'war', 'faction-collapse', 'combat', 'route-robbery'}
TERMINAL = {'applied', 'failed', 'cancelled', 'blocked'}


class DMError(ValueError):
    def __init__(self, message, status=422):
        super().__init__(message)
        self.status = status


def checked(condition, message, status=422):
    if not condition:
        raise DMError(message, status)


def text(value, maximum=240, *, empty=False, multiline=False):
    checked(isinstance(value, str) and len(value) <= maximum and (empty or bool(value.strip())) and
            all((ord(c) >= 32 and ord(c) != 127 and not 0xD800 <= ord(c) <= 0xDFFF) or
                (multiline and c == '\n') for c in value), 'Invalid or overlong text.')
    return value


def ident(value):
    checked(isinstance(value, str) and ID.fullmatch(value), 'Invalid identifier.')
    return value


def number(value, low, high, *, integer=False):
    checked(type(value) in (int, float) and math.isfinite(value) and low <= value <= high and
            (not integer or value == int(value)), 'Invalid numeric value.')
    return int(value) if integer else value


def shape(value, required=(), optional=()):
    checked(isinstance(value, dict) and set(required) <= value.keys() and
            value.keys() <= set(required) | set(optional), 'Unexpected or missing fields.')
    return value


def id_list(value, maximum=256):
    checked(isinstance(value, list) and len(value) <= maximum, 'Invalid identifier list.')
    result = [ident(item) for item in value]
    checked(len(set(result)) == len(result), 'Duplicate identifiers are not allowed.')
    return sorted(result)


def encode(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(',', ':'), allow_nan=False)


def decode(data):
    def pairs(items):
        result = {}
        for key, value in items:
            checked(key not in result, 'Duplicate JSON fields are not allowed.')
            result[key] = value
        return result
    try:
        return json.loads(data, object_pairs_hook=pairs,
                          parse_constant=lambda value: (_ for _ in ()).throw(DMError('Nonfinite JSON number.')))
    except (ValueError, UnicodeError, RecursionError) as error:
        if isinstance(error, DMError):
            raise
        raise DMError('Malformed JSON.') from error


def utc(timestamp):
    return datetime.fromtimestamp(timestamp, timezone.utc).isoformat(timespec='seconds').replace('+00:00', 'Z')


def parse_utc(value):
    text(value, 40)
    checked(value.endswith('Z') or value.endswith('+00:00'), 'Schedule must be an explicit UTC timestamp.')
    try:
        result = datetime.fromisoformat(value.replace('Z', '+00:00'))
        checked(result.utcoffset() == timedelta(0), 'Schedule must be UTC.')
        return result.timestamp()
    except (ValueError, OverflowError) as error:
        if isinstance(error, DMError):
            raise
        raise DMError('Invalid UTC timestamp.') from error


def private_dir(path):
    path = Path(path).absolute()
    checked(not path.is_symlink(), 'Private directory cannot be a symlink.')
    path.mkdir(parents=True, exist_ok=True, mode=0o700)
    info = path.stat()
    checked(info.st_uid == os.getuid() and stat.S_ISDIR(info.st_mode) and not info.st_mode & 0o077,
            'Private directory must be owned by this user with mode 0700.')
    return path


def read_file(path, limit):
    flags = os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0)
    descriptor = os.open(path, flags)
    try:
        info = os.fstat(descriptor)
        checked(stat.S_ISREG(info.st_mode) and info.st_size <= limit, 'Bridge file is invalid or too large.')
        with os.fdopen(descriptor, 'rb', closefd=False) as source:
            data = source.read(limit + 1)
        checked(len(data) <= limit, 'Bridge file exceeds its size limit.')
        return data
    finally:
        os.close(descriptor)


def atomic_json(path, value):
    path = Path(path)
    temporary = path.parent / ('.' + path.name + '.' + secrets.token_hex(8))
    descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(descriptor, 'w', encoding='utf-8') as target:
            target.write(encode(value))
            target.flush()
            os.fsync(target.fileno())
        os.replace(temporary, path)
        directory = os.open(path.parent, os.O_RDONLY | getattr(os, 'O_DIRECTORY', 0))
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        if temporary.exists():
            temporary.unlink()


def normalize_snapshot(raw, now):
    checked(isinstance(raw, dict) and type(raw.get('version')) is int and raw['version'] == 1,
            'Unsupported authority snapshot.')
    result = {'version': 1, 'worldId': ident(raw.get('worldId')),
              'sequence': number(raw.get('sequence'), 0, 2**53, integer=True),
              'generatedAtUnix': number(raw.get('generatedAtUnix'), 0, now),
              'calendarDays': number(raw.get('calendarDays'), 0, 4e6)}
    checked(isinstance(raw.get('capabilities'), list) and len(raw['capabilities']) <= 32, 'Invalid capabilities.')
    result['capabilities'] = sorted({item for item in raw['capabilities'] if isinstance(item, str) and item in NATIVE})
    for key, maximum in [('cells', 256), ('characters', 4096), ('accounts', 8192), ('factions', 64), ('chapters', 128)]:
        checked(isinstance(raw.get(key), list) and len(raw[key]) <= maximum, f'Invalid {key} catalog.')
        result[key] = []
        seen = set()
        for entry in raw[key]:
            checked(isinstance(entry, dict), f'Invalid {key} entry.')
            entry_id = ident(entry.get('id'))
            checked(entry_id not in seen, f'Duplicate {key} identifier.')
            seen.add(entry_id)
            item = {'id': entry_id}
            if key != 'accounts':
                item['name'] = text(entry.get('name'), 120)
            if key == 'cells':
                for coordinate in ('x', 'y', 'z'):
                    item[coordinate] = number(entry.get(coordinate), -1e6, 1e6)
                item['width'] = number(entry.get('width'), 1, 256, integer=True)
                item['height'] = number(entry.get('height'), 1, 256, integer=True)
                checked(type(entry.get('outdoors')) is bool, 'Invalid shelter flag.')
                item['outdoors'] = entry['outdoors']
                weather = entry.get('weather', 'clear')
                checked(weather in WEATHERS, 'Invalid cell weather.')
                item['weather'] = weather
                rows = entry.get('terrain')
                checked(isinstance(rows, list) and len(rows) == item['height'] and
                        all(isinstance(row, str) and len(row) == item['width'] and
                            all(32 <= ord(char) < 127 for char in row) for row in rows), 'Invalid terrain grid.')
                item['terrain'] = rows
                territory = entry.get('territory')
                checked(isinstance(territory, dict), 'Missing cell territory metadata.')
                item['territory'] = {'region': ident(territory.get('region')),
                                     'claims': id_list(territory.get('claims'), 64),
                                     'chapter': ident(territory['chapter']) if territory.get('chapter') else ''}
            elif key == 'characters':
                for flag in ('npc', 'online', 'active', 'relocating'):
                    checked(type(entry.get(flag)) is bool, f'Invalid character {flag}.')
                    item[flag] = entry[flag]
                for field in ('cell', 'homeCell'):
                    item[field] = ident(entry[field]) if entry.get(field) else ''
                for field in ('x', 'y', 'homeX', 'homeY'):
                    item[field] = number(entry.get(field), -1e6, 1e6)
                item['age'] = number(entry.get('age'), 0, 10000, integer=True)
                item['activity'] = text(entry.get('activity', ''), 512, empty=True)
                item['role'] = text(entry.get('role', ''), 64, empty=True)
                item['relocationTarget'] = ident(entry['relocationTarget']) if entry.get('relocationTarget') else ''
                item['lastActiveAtUnix'] = number(entry.get('lastActiveAtUnix', 0), 0, now)
                item['leaderId'] = ident(entry['leaderId']) if entry.get('leaderId') else ''
                checked(type(entry.get('recruited', False)) is bool, 'Invalid recruitment flag.')
                item['recruited'] = entry.get('recruited', False)
            if key in ('characters', 'accounts'):
                item['cash'] = number(entry.get('cash'), 0, 1e12, integer=True)
                stock = entry.get('stock')
                checked(isinstance(stock, dict), 'Invalid inventory stock.')
                item['stock'] = {good: number(stock.get(good, 0), 0, 1e9, integer=True) for good in ('herbs', 'meal')}
            if key == 'factions':
                checked(isinstance(entry.get('color'), str) and re.fullmatch(r'#[0-9a-fA-F]{6}', entry['color']),
                        'Invalid faction color.')
                item['color'] = entry['color']
            result[key].append(item)
    cells = {cell['id'] for cell in result['cells']}
    factions = {faction['id'] for faction in result['factions']}
    chapters = {chapter['id'] for chapter in result['chapters']}
    checked(sum(cell['width'] * cell['height'] for cell in result['cells']) <= 262144, 'Terrain budget exceeded.')
    for cell in result['cells']:
        checked(set(cell['territory']['claims']) <= factions and
                (not cell['territory']['chapter'] or cell['territory']['chapter'] in chapters), 'Unknown territory references.')
    for actor in result['characters']:
        checked(actor['cell'] in cells and (not actor['homeCell'] or actor['homeCell'] in cells), 'Unknown character cell.')
        checked(not actor['active'] or actor['online'] and not actor['npc'], 'Invalid active-player flag.')
    economy = raw.get('economy', {})
    checked(isinstance(economy, dict), 'Invalid economy metadata.')
    result['economy'] = {key: number(economy.get(key, 0), 0, 1e15, integer=True) for key in ('minted', 'sunk')}
    ledger = economy.get('ledger', [])
    checked(isinstance(ledger, list) and len(ledger) <= 128, 'Invalid bounded economy ledger.')
    result['economy']['ledger'] = []
    for entry in ledger:
        checked(isinstance(entry, dict), 'Invalid economy ledger entry.')
        record = {key: number(entry.get(key), 0, 1e12, integer=True) for key in ('sequence', 'day', 'coins', 'quantity')}
        record.update({key: text(entry.get(key), 120, empty=True) for key in ('kind', 'from', 'to', 'item')})
        result['economy']['ledger'].append(record)
    # Deliberate allowlist: no chat, drafts, model keys, memories, or arbitrary nested data.
    return result


class DMService:
    def __init__(self, state_dir, exchange_dir, *, clock=time.time, connect=None):
        self.state_dir = private_dir(state_dir)
        self.exchange_dir = private_dir(exchange_dir)
        self.outbox = private_dir(self.exchange_dir / 'outbox')
        self.inbox = private_dir(self.exchange_dir / 'inbox')
        self.clock = clock
        self.lock = threading.RLock()
        # State lives in the database's dm schema; the state directory keeps only the private session file.
        self.db = PgState((connect or DEFAULT_CONNECT)())
        self.snapshot = self._meta('snapshot')
        self.bridge_error = 'Authority snapshot has not been received.'
        self.world_mismatch = False

    def close(self):
        with self.lock:
            self.db.close()

    def _meta(self, key):
        row = self.db.execute('SELECT value FROM meta WHERE key=?', (key,)).fetchone()
        return decode(row['value']) if row else None

    def _set_meta(self, key, value):
        self.db.execute('INSERT INTO meta(key,value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value',
                        (key, encode(value)))

    def _get(self, kind, identifier):
        row = self.db.execute('SELECT data FROM documents WHERE kind=? AND id=?', (kind, identifier)).fetchone()
        return decode(row['data']) if row else None

    def _all(self, kind):
        return [decode(row['data']) for row in self.db.execute('SELECT data FROM documents WHERE kind=? ORDER BY id', (kind,))]

    def _put(self, kind, value):
        self.db.execute('INSERT INTO documents(kind,id,data) VALUES(?,?,?) ON CONFLICT(kind,id) DO UPDATE SET data=excluded.data',
                        (kind, value['id'], encode(value)))

    def _audit(self, action, target, detail):
        self.db.execute('INSERT INTO audit(at,action,target,detail) VALUES(?,?,?,?)',
                        (self.clock(), action, target, text(detail, 2000, empty=True, multiline=True)))

    def bridge(self):
        age = max(0, self.clock() - self.snapshot['generatedAtUnix']) if self.snapshot else None
        live = self.snapshot is not None and age <= STALE_SECONDS and not self.bridge_error and not self.world_mismatch
        return {'live': live, 'stale': not live, 'ageSeconds': age,
                'worldId': self._meta('worldId'),
                'detail': self.bridge_error or ('Authority connected.' if live else 'Authority is stale; live effects are disabled.')}

    def _require_live(self):
        checked(self.bridge()['live'], 'Live authority unavailable; this operation is read-only until a fresh snapshot arrives.', 409)

    def _ingest(self):
        try:
            incoming = normalize_snapshot(decode(read_file(self.exchange_dir / 'snapshot.json', MAX_SNAPSHOT)), self.clock())
            pinned = self._meta('worldId')
            checked(not pinned or incoming['worldId'] == pinned,
                    'Authority worldId changed. Use a separate Storykeeper state directory for another world.', 409)
            old = self.snapshot
            if old:
                checked(incoming['sequence'] >= old['sequence'], 'Authority sequence moved backwards; refusing stale or rolled-back state.', 409)
                checked(incoming['generatedAtUnix'] >= old['generatedAtUnix'], 'Authority timestamp moved backwards.', 409)
            with self.db:
                if not pinned:
                    self._set_meta('worldId', incoming['worldId'])
                if old and incoming['generatedAtUnix'] > old['generatedAtUnix']:
                    self._observe(old, incoming)
                self._set_meta('snapshot', incoming)
            self.snapshot = incoming
            self.bridge_error = ''
            self.world_mismatch = False
        except (OSError, DMError) as error:
            self.bridge_error = f'Authority snapshot unavailable or rejected: {error}'
            self.world_mismatch = 'worldId changed' in str(error)

    def _observe(self, old, new):
        start, end = old['generatedAtUnix'], new['generatedAtUnix']
        delta = end - start
        # Never fill service downtime or sparse data gaps with invented active minutes/routes.
        if delta <= 0 or delta > STALE_SECONDS or self.clock() - end > STALE_SECONDS:
            return
        previous = {actor['id']: actor for actor in old['characters']}
        current = {actor['id']: actor for actor in new['characters']}
        for chapter in self._all('chapter'):
            active = [actor for identifier, actor in current.items() if identifier in chapter['memberIds'] and
                      actor['active'] and identifier in previous and previous[identifier]['active']]
            cursor = start
            while active and cursor < end:
                dt = datetime.fromtimestamp(cursor, timezone.utc)
                boundary = (dt.replace(minute=0, second=0, microsecond=0) + timedelta(hours=1)).timestamp()
                seconds = min(end, boundary) - cursor
                self.db.execute('''INSERT INTO activity VALUES(?,?,?,?,?,1) ON CONFLICT(chapter,weekday,hour)
                    DO UPDATE SET activeMinutes=activity.activeMinutes+excluded.activeMinutes,
                    memberMinutes=activity.memberMinutes+excluded.memberMinutes,samples=activity.samples+1''',
                    (chapter['id'], dt.weekday(), dt.hour, seconds / 60, seconds * len(active) / 60))
                cursor += seconds
            for identifier in chapter['memberIds']:
                before, after = previous.get(identifier), current.get(identifier)
                if before and after and before['online'] and after['online'] and before['cell'] != after['cell']:
                    self.db.execute('''INSERT INTO routes VALUES(?,?,?,1) ON CONFLICT(chapter,source,destination)
                        DO UPDATE SET transitions=routes.transitions+1''', (chapter['id'], before['cell'], after['cell']))

    def state(self):
        with self.lock:
            self.tick()
            result = {'version': 1, 'serverTime': utc(self.clock()), 'bridge': self.bridge(),
                      'snapshot': copy.deepcopy(self.snapshot),
                      'activity': [dict(row) for row in self.db.execute(f'SELECT {ACTIVITY} FROM activity ORDER BY chapter,weekday,hour')],
                      'routes': [dict(row) for row in self.db.execute('SELECT * FROM routes ORDER BY chapter,source,destination')],
                      'audit': [dict(row) for row in self.db.execute('SELECT * FROM audit ORDER BY sequence DESC LIMIT 200')]}
            for singular, plural in [('campaign', 'campaigns'), ('beat', 'beats'), ('event', 'events'),
                                     ('chapter', 'chapters'), ('faction', 'factions'), ('opinion', 'opinions'),
                                     ('migration', 'migrationPreviews')]:
                result[plural] = self._all(singular)
            # Never expose internal execution envelopes or command receipts to frontend editing.
            for event in result['events']:
                event.pop('envelope', None)
            return result

    def command(self, request):
        shape(request, ('id', 'action', 'payload'))
        command_id = ident(request['id'])
        action = text(request['action'], 64)
        checked(isinstance(request['payload'], dict), 'Command payload must be an object.')
        fingerprint = hashlib.sha256(encode(request).encode()).hexdigest()
        with self.lock:
            self.tick()
            old = self.db.execute('SELECT fingerprint,response FROM commands WHERE id=?', (command_id,)).fetchone()
            if old:
                checked(hmac.compare_digest(old['fingerprint'], fingerprint), 'Command ID was already used for another request.', 409)
                return decode(old['response'])
            try:
                with self.db:
                    result = self._action(action, request['payload'])
                    response = {'ok': True, 'id': command_id, 'result': result}
                    self.db.execute('INSERT INTO commands VALUES(?,?,?)', (command_id, fingerprint, encode(response)))
                    self._audit(action, result.get('id', result.get('chapterId', '')) if isinstance(result, dict) else '',
                                'Operator command accepted.')
            except DMError as error:
                with self.db:
                    self._audit('rejected:' + action, command_id, str(error))
                raise
            self._publish()
            return response

    def _action(self, action, payload):
        if action != 'chapter.peak':
            self._require_live()
        if action in ('campaign.upsert', 'beat.upsert', 'chapter.upsert', 'faction.upsert'):
            return self._upsert(action.split('.')[0], payload)
        if action in ('campaign.delete', 'beat.delete'):
            kind = action.split('.')[0]
            shape(payload, (kind + 'Id',))
            identifier = ident(payload[kind + 'Id'])
            checked(self._get(kind, identifier), 'Record does not exist.', 404)
            children = self._all('event') + (self._all('beat') if kind == 'campaign' else [])
            checked(not any(child.get(kind + 'Id') == identifier for child in children),
                    'Remove or reassign referenced records before deleting this record.', 409)
            self.db.execute('DELETE FROM documents WHERE kind=? AND id=?', (kind, identifier))
            return {'id': identifier, 'deleted': True}
        if action == 'opinion.set':
            shape(payload, ('factionId', 'targetType', 'targetId', 'score', 'reason'))
            faction = ident(payload['factionId'])
            checked(faction in self._factions(), 'Unknown faction.')
            checked(payload['targetType'] in ('chapter', 'player'), 'Opinion target must be a Chapter or player.')
            target = ident(payload['targetId'])
            checked(self._get('chapter', target) if payload['targetType'] == 'chapter' else
                    any(actor['id'] == target and not actor['npc'] for actor in (self.snapshot or {}).get('characters', [])),
                    'Unknown opinion target.')
            return self._opinion(faction, payload['targetType'], target, number(payload['score'], -100, 100, integer=True),
                                 text(payload['reason'], 1000, multiline=True))
        if action == 'event.create':
            return self._create_event(payload)
        if action in ('event.approve', 'event.cancel'):
            shape(payload, ('eventId',))
            event = self._get('event', ident(payload['eventId']))
            checked(event, 'Event not found.', 404)
            if action == 'event.cancel':
                checked(event['status'] in ('draft', 'scheduled', 'blocked'),
                        'Dispatched effects cannot be cancelled safely; wait for the authority result.', 409)
                event.update(status='cancelled', detail='Cancelled before dispatch.', updatedAt=utc(self.clock()))
            else:
                checked(event['status'] == 'draft', 'Only draft events can be approved.', 409)
                self._require_live()
                self._validate_live_effect(event['kind'], event['payload'])
                event.update(status='scheduled', approvedAt=utc(self.clock()), worldId=self.snapshot['worldId'],
                             detail='Approved; fixed UTC schedule will not move automatically.')
                if parse_utc(event['scheduledAt']) <= self.clock():
                    self._prepare(event)
            self._put('event', event)
            return self._public_event(event)
        if action == 'chapter.peak':
            shape(payload, ('chapterId',))
            return self._peak(ident(payload['chapterId']))
        if action == 'migration.preview':
            shape(payload, ('chapterId',))
            self._require_live()
            preview = self._migration_preview(ident(payload['chapterId']))
            self._put('migration', preview)
            return preview
        if action == 'migration.approve':
            return self._approve_migration(payload)
        raise DMError('Unsupported command action.')

    def _upsert(self, kind, payload):
        fields = {'campaign': (('name',), ('description',)),
                  'beat': (('campaignId', 'title'), ('description', 'kind')),
                  'chapter': (('name', 'memberIds', 'cellIds', 'housing', 'jobCapacity', 'attraction'),
                              ('description', 'treatyModifier')),
                  'faction': (('name',), ('description',))}
        required, optional = fields[kind]
        shape(payload, required, ('id',) + optional)
        identifier = ident(payload['id']) if 'id' in payload else uuid.uuid4().hex
        old = self._get(kind, identifier)
        checked(old is not None or len(self._all(kind)) < (128 if kind == 'chapter' else 2000), 'Record limit reached.', 409)
        result = {'id': identifier, 'createdAt': old['createdAt'] if old else utc(self.clock()), 'updatedAt': utc(self.clock()),
                  'description': text(payload.get('description', ''), 6000, empty=True, multiline=True)}
        if kind == 'beat':
            result['campaignId'] = ident(payload['campaignId'])
            checked(self._get('campaign', result['campaignId']), 'Unknown campaign.')
            result['title'] = text(payload['title'], 160)
            result['kind'] = text(payload.get('kind', 'story'), 64)
        else:
            result['name'] = text(payload['name'], 120)
        if kind == 'chapter':
            result.update(memberIds=id_list(payload['memberIds'], 128), cellIds=id_list(payload['cellIds']))
            native_cells = {cell['id']: cell for cell in (self.snapshot or {}).get('cells', [])}
            for cell_id in result['cellIds']:
                checked(cell_id in native_cells, 'Chapter sites must reference a known authority cell.')
                authored = native_cells[cell_id]['territory']['chapter']
                checked(not authored or authored == identifier, 'Chapter site conflicts with authored ownership.', 409)
                checked(not any(other['id'] != identifier and cell_id in other['cellIds'] for other in self._all('chapter')),
                        'Chapter sites cannot overlap another operator-declared Chapter.', 409)
            for field in ('housing', 'jobCapacity', 'attraction'):
                result[field] = number(payload[field], 0, 100 if field == 'attraction' else 10000, integer=True)
            result['treatyModifier'] = number(payload.get('treatyModifier', 0), 0, 1)
            result['siteAuthority'] = 'Operator-declared planning overlay; native authored ownership takes precedence.'
        self._put(kind, result)
        return result

    def _factions(self):
        return {item['id']: item for item in (self.snapshot or {}).get('factions', [])} | {
            item['id']: item for item in self._all('faction')}

    def _opinion(self, faction, target_type, target, score, reason):
        identifier = hashlib.sha256(f'{faction}:{target_type}:{target}'.encode()).hexdigest()
        result = {'id': identifier, 'factionId': faction, 'targetType': target_type, 'targetId': target,
                  'score': score, 'reason': reason, 'updatedAt': utc(self.clock())}
        self._put('opinion', result)
        return result

    def _effect_shape(self, kind, payload):
        if kind == 'notice':
            shape(payload, ('scope', 'text'), ('target',))
            checked(payload['scope'] in ('world', 'cell', 'player', 'chapter'), 'Invalid notice scope.')
            text(payload['text'], 2000, multiline=True)
            if payload['scope'] != 'world':
                ident(payload.get('target'))
            else:
                checked('target' not in payload or payload['target'] == '', 'World notice cannot have an individual target.')
        elif kind == 'weather':
            shape(payload, ('cell', 'preset'))
            ident(payload['cell'])
            checked(payload['preset'] in (*WEATHERS, 'seasonal'), 'Unsupported weather preset.')
        elif kind == 'npc_relocate':
            shape(payload, ('npc', 'cell', 'x', 'y'))
            ident(payload['npc']); ident(payload['cell'])
            number(payload['x'], 0, 256); number(payload['y'], 0, 256)
        elif kind == 'economy_transfer':
            shape(payload, ('from', 'to', 'item', 'quantity', 'coins'))
            ident(payload['from']); ident(payload['to'])
            checked(payload['from'] != payload['to'], 'Transfer requires distinct accounts.')
            checked(payload['item'] in ('', 'herbs', 'meal'), 'Unsupported good.')
            quantity = number(payload['quantity'], 0, 99, integer=True)
            coins = number(payload['coins'], 0, 1000000, integer=True)
            checked((quantity > 0 or coins > 0) and bool(payload['item']) == (quantity > 0), 'Empty or inconsistent transfer.')
        else:
            checked(kind in STORY_ONLY and isinstance(payload, dict) and len(encode(payload)) <= 16000,
                    'Unsupported event kind or overlong story-only payload.')

    def _create_event(self, payload):
        shape(payload, ('title', 'kind', 'payload'), ('scheduledAt', 'campaignId', 'beatId', 'chapterId'))
        title = text(payload['title'], 160)
        kind = text(payload['kind'], 64)
        self._effect_shape(kind, payload['payload'])
        checked(len(self._all('event')) < 10000, 'Event record limit reached.', 409)
        schedule = parse_utc(payload['scheduledAt']) if payload.get('scheduledAt') else self.clock()
        checked(schedule <= self.clock() + 366 * 86400, 'Schedule must be within the next year.')
        event = {'id': uuid.uuid4().hex, 'title': title, 'kind': kind, 'payload': copy.deepcopy(payload['payload']),
                 'createdAt': utc(self.clock()), 'updatedAt': utc(self.clock()), 'scheduledAt': utc(schedule),
                 'status': 'draft' if kind in NATIVE else 'blocked',
                 'detail': 'Awaiting explicit approval.' if kind in NATIVE else 'Story plan only: this executor is not implemented.'}
        for field, table in [('campaignId', 'campaign'), ('beatId', 'beat'), ('chapterId', 'chapter')]:
            if field in payload:
                event[field] = ident(payload[field])
                checked(self._get(table, event[field]), f'Unknown {table}.')
        if event.get('beatId') and event.get('campaignId'):
            checked(self._get('beat', event['beatId'])['campaignId'] == event['campaignId'], 'Beat belongs to another campaign.')
        self._put('event', event)
        return event

    def _validate_live_effect(self, kind, payload):
        self._require_live()
        checked(kind in self.snapshot['capabilities'], 'Authority does not advertise this executor.', 409)
        self._effect_shape(kind, payload)
        cells = {cell['id']: cell for cell in self.snapshot['cells']}
        actors = {actor['id']: actor for actor in self.snapshot['characters']}
        if kind == 'notice':
            target = payload.get('target')
            online = [actor for actor in actors.values() if not actor['npc'] and actor['online']]
            if payload['scope'] == 'world':
                checked(online, 'No connected players can receive this notice.', 409)
            if payload['scope'] == 'cell':
                checked(target in cells, 'Notice cell is unknown.')
                checked(any(actor['cell'] == target for actor in online), 'No connected players are in the notice cell.', 409)
            elif payload['scope'] == 'player':
                checked(target in actors and not actors[target]['npc'] and actors[target]['online'],
                        'Notice player is unknown or offline.', 409)
            elif payload['scope'] == 'chapter':
                chapter = self._get('chapter', target)
                checked(chapter and chapter['memberIds'] and all(member in actors and not actors[member]['npc']
                        for member in chapter['memberIds']), 'Chapter notice requires known player members.')
                checked(any(actor['id'] in chapter['memberIds'] for actor in online),
                        'No connected Chapter members can receive this notice.', 409)
        elif kind == 'weather':
            checked(payload['cell'] in cells, 'Weather cell is unknown.')
        elif kind == 'npc_relocate':
            npc, cell = actors.get(payload['npc']), cells.get(payload['cell'])
            checked(npc and npc['npc'] and npc['role'] == 'resident' and not npc['leaderId'] and not npc['recruited'] and not npc['relocating'],
                    'Only an available, non-recruited resident NPC may relocate.', 409)
            checked(cell and self._walkable(cell, payload['x'], payload['y']), 'Destination is not a known traversable point.')
        elif kind == 'economy_transfer':
            accounts = {actor['id']: actor for actor in self.snapshot['characters']} | {
                account['id']: account for account in self.snapshot['accounts']}
            source, destination = accounts.get(payload['from']), accounts.get(payload['to'])
            checked(source and destination, 'Transfer account is unknown.')
            checked(source['cash'] >= payload['coins'] and
                    (not payload['quantity'] or source['stock'][payload['item']] >= payload['quantity']),
                    'Source lacks the existing money or goods.', 409)

    @staticmethod
    def _walkable(cell, x, y):
        return 0 <= x < cell['width'] and 0 <= y < cell['height'] and cell['terrain'][int(y)][int(x)] not in '#T=~ '

    def _prepare(self, event):
        self._validate_live_effect(event['kind'], event['payload'])
        checked(event.get('worldId', self.snapshot['worldId']) == self.snapshot['worldId'], 'Event belongs to another authority world.', 409)
        payload = copy.deepcopy(event['payload'])
        if event['kind'] == 'notice' and payload['scope'] == 'chapter':
            payload = {'scope': 'players', 'targets': self._get('chapter', payload['target'])['memberIds'], 'text': payload['text']}
        now = self.clock()
        envelope = {'version': 1, 'id': event['id'], 'worldId': self.snapshot['worldId'], 'createdAtUnix': now,
                    'expiresAtUnix': now + REQUEST_TTL, 'kind': event['kind'], 'payload': payload}
        event.update(status='queued', worldId=self.snapshot['worldId'], requestId=event['id'], envelope=envelope,
                     queuedAt=utc(now), queuedSequence=self.snapshot['sequence'], updatedAt=utc(now),
                     detail='Queued for the authority; no effect is confirmed yet.')

    @staticmethod
    def _public_event(event):
        return {key: value for key, value in event.items() if key != 'envelope'}

    def _publish(self):
        if not self.bridge()['live']:
            return
        for event in self._all('event'):
            if event['status'] != 'queued' or event.get('nativeAcceptedAt') or self.clock() > event['envelope']['expiresAtUnix']:
                continue
            path = self.outbox / (event['id'] + '.json')
            try:
                if path.exists() or path.is_symlink():
                    checked(decode(read_file(path, MAX_BODY)) == event['envelope'], 'Outbox envelope conflicts with its persisted request.')
                else:
                    atomic_json(path, event['envelope'])
            except (OSError, DMError) as error:
                with self.db:
                    detail = f'Outbox write pending: {error}'[:1000]
                    if event.get('detail') != detail:
                        event['detail'] = detail
                        self._put('event', event)
                        self._audit('bridge.write_failed', event['id'], detail)

    def tick(self):
        with self.lock:
            self._ingest()
            with self.db:
                for event in self._all('event'):
                    if event['status'] == 'scheduled' and parse_utc(event['scheduledAt']) <= self.clock():
                        if self.clock() - parse_utc(event['scheduledAt']) > REQUEST_TTL:
                            event.update(status='blocked', detail='Scheduled window missed. Draft a newly approved event; no late surprise was dispatched.')
                            self._put('event', event)
                            self._audit('event.blocked', event['id'], event['detail'])
                            continue
                        if not self.bridge()['live']:
                            continue
                        try:
                            self._prepare(event)
                        except DMError as error:
                            event.update(status='blocked', detail=str(error))
                        self._put('event', event)
                        self._audit('event.' + event['status'], event['id'], event['detail'])
                    if event['status'] == 'queued':
                        self._reconcile(event)
            self._publish()

    def _reconcile(self, event):
        if not event.get('nativeAcceptedAt'):
            path = self.inbox / (event['id'] + '.json')
            try:
                receipt = decode(read_file(path, MAX_BODY))
                shape(receipt, ('version', 'id', 'worldId', 'ok', 'detail', 'appliedAtUnix'))
                checked(type(receipt['version']) is int and receipt['version'] == 1 and receipt['id'] == event['id'] and
                        receipt['worldId'] == event['worldId'] and type(receipt['ok']) is bool, 'Invalid or cross-world receipt.')
                number(receipt['appliedAtUnix'], event['envelope']['createdAtUnix'] - 1, self.clock())
                checked(not receipt['ok'] or receipt['appliedAtUnix'] <= event['envelope']['expiresAtUnix'],
                        'Successful receipt claims application after request expiry.')
                detail = text(receipt['detail'], 2000, empty=True, multiline=True)
                if not receipt['ok']:
                    event.update(status='failed', detail=detail, updatedAt=utc(self.clock()))
                elif event['kind'] == 'npc_relocate':
                    event.update(nativeAcceptedAt=receipt['appliedAtUnix'], verificationDeadline=self.clock() + ARRIVAL_TIMEOUT,
                                 detail='Authority accepted navigation. Waiting for a fresh snapshot to verify arrival.')
                else:
                    event.update(status='applied', detail=detail, appliedAt=utc(receipt['appliedAtUnix']), updatedAt=utc(self.clock()))
                self._put('event', event)
                self._audit('event.' + event['status'], event['id'], event['detail'])
            except FileNotFoundError:
                pass
            except (OSError, DMError) as error:
                detail = f'Untrusted or unreadable result: {error}'[:1000]
                if event['detail'] != detail:
                    event['detail'] = detail
                    self._put('event', event)
                    self._audit('bridge.result_rejected', event['id'], detail)
        if event['status'] != 'queued':
            return
        if event.get('nativeAcceptedAt'):
            actor = next((actor for actor in (self.snapshot or {}).get('characters', []) if actor['id'] == event['payload']['npc']), None)
            if (self.bridge()['live'] and self.snapshot['generatedAtUnix'] >= event['nativeAcceptedAt'] and actor and
                    actor['homeCell'] == event['payload']['cell'] and not actor['relocating'] and not actor['relocationTarget'] and
                    math.hypot(actor['homeX'] - event['payload']['x'], actor['homeY'] - event['payload']['y']) <= .3):
                event.update(status='applied', appliedAt=utc(self.clock()), updatedAt=utc(self.clock()),
                             detail='Arrival verified in the authoritative home record.')
                if event.get('migration'):
                    self._arrival(event)
            elif self.clock() > event['verificationDeadline']:
                event.update(status='failed', updatedAt=utc(self.clock()),
                             detail='Arrival could not be verified within ten minutes. The native move may still be in progress; no resentment was applied.')
        elif self.clock() > event['envelope']['expiresAtUnix']:
            event.update(status='failed', updatedAt=utc(self.clock()),
                         detail='Authority result was not received before expiry. Outcome is unconfirmed; do not assume the effect was undone.')
        if event['status'] != 'queued':
            self._put('event', event)
            self._audit('event.' + event['status'], event['id'], event['detail'])

    def _peak(self, chapter_id):
        checked(self._get('chapter', chapter_id), 'Unknown Chapter.')
        rows = list(self.db.execute(f'SELECT {ACTIVITY} FROM activity WHERE chapter=? AND activeMinutes>0 ORDER BY activeMinutes DESC,weekday,hour', (chapter_id,)))
        if not rows:
            return {'chapterId': chapter_id, 'suggestion': None, 'reason': 'No observed active Chapter minutes yet. No history was fabricated.'}
        best = rows[0]
        now = datetime.fromtimestamp(self.clock(), timezone.utc)
        target = now.replace(hour=best['hour'], minute=0, second=0, microsecond=0)
        target += timedelta(days=(best['weekday'] - now.weekday()) % 7)
        if target.timestamp() <= self.clock():
            target += timedelta(days=7)
        return {'chapterId': chapter_id, 'suggestion': {'weekday': best['weekday'], 'hourUtc': best['hour'],
                'scheduledAt': utc(target.timestamp()), 'observedMinutes': round(best['activeMinutes'], 3), 'samples': best['samples']},
                'reason': 'Suggestion from observed activity only. Explicitly choose this UTC time; existing schedules never move.'}

    def _migration_preview(self, chapter_id):
        chapter = self._get('chapter', chapter_id)
        checked(chapter, 'Unknown Chapter.')
        cells = {cell['id']: cell for cell in self.snapshot['cells']}
        destinations = sorted((cells[identifier] for identifier in chapter['cellIds'] if identifier in cells and
                               cells[identifier]['territory']['chapter'] in ('', chapter_id)), key=lambda cell: cell['id'])
        residents = [actor for actor in self.snapshot['characters'] if actor['npc']]
        occupied = sum(actor['homeCell'] in chapter['cellIds'] for actor in residents)
        pending = [event for event in self._all('event') if event['status'] in ('scheduled', 'queued') and
                   event.get('migration', {}).get('chapterId') == chapter_id]
        reserved = len(pending)
        available = max(0, min(chapter['housing'], chapter['jobCapacity']) - occupied - reserved)
        preview = {'id': uuid.uuid4().hex, 'chapterId': chapter_id, 'worldId': self.snapshot['worldId'],
                   'createdAt': utc(self.clock()), 'expiresAt': utc(self.clock() + PREVIEW_TTL),
                   'snapshotSequence': self.snapshot['sequence'], 'capacity': {'housing': chapter['housing'],
                   'jobs': chapter['jobCapacity'], 'occupied': occupied, 'reserved': reserved, 'available': available},
                   'candidates': [], 'blockedReason': ''}
        if not destinations or not available:
            preview['blockedReason'] = 'No non-conflicting declared Chapter destination or no declared housing/job capacity remains.'
            return preview
        destination = destinations[0]
        points = [(x + .5, y + .5) for y in range(destination['height']) for x in range(destination['width'])
                  if self._walkable(destination, x + .5, y + .5)]
        if not points:
            preview['blockedReason'] = 'No safe destination tile is available.'
            return preview
        points.sort(key=lambda point: (abs(point[0] - destination['width'] / 2) + abs(point[1] - destination['height'] / 2), point[1], point[0]))
        active = sum(actor['active'] and actor['id'] in chapter['memberIds'] for actor in self.snapshot['characters'])
        in_flight = {event['payload'].get('npc') for event in self._all('event') if event['kind'] == 'npc_relocate' and event['status'] in ('scheduled', 'queued')}
        for npc in sorted(residents, key=lambda actor: actor['id']):
            home = cells.get(npc['homeCell'])
            prior = self.db.execute('SELECT at FROM arrivals WHERE npc=?', (npc['id'],)).fetchone()
            if not home or npc['homeCell'] in chapter['cellIds'] or npc['role'] != 'resident' or npc['leaderId'] or npc['recruited'] or npc['relocating'] or npc['id'] in in_flight:
                continue
            if home['territory']['region'] != destination['territory']['region'] or home['territory']['region'] == 'unassigned':
                continue
            if prior and self.clock() - prior['at'] < MIGRATION_COOLDOWN:
                continue
            need = (10 if npc['cash'] < 12 else 0) + (8 if npc['stock']['meal'] == 0 else 0)
            score = min(100, chapter['attraction'] + min(active * 3, 12) + need)
            loss = min(10, 2 + need // 6)
            resentment = max(0, round(loss * (1 - chapter['treatyModifier'])))
            preview['candidates'].append({'npcId': npc['id'], 'name': npc['name'], 'sourceCell': home['id'],
                'destinationCell': destination['id'], 'x': points[0][0], 'y': points[0][1], 'score': score,
                'reasons': [f'Declared attraction {chapter["attraction"]}/100.',
                            f'{active} Chapter members currently active.',
                            f'Observed purse {npc["cash"]}; carried meals {npc["stock"]["meal"]}.'],
                'sourceFactions': home['territory']['claims'], 'resentment': resentment})
        preview['candidates'].sort(key=lambda candidate: (-candidate['score'], candidate['npcId']))
        preview['candidates'] = preview['candidates'][:min(available, 32)]
        if not preview['candidates']:
            preview['blockedReason'] = 'No eligible same-region resident candidates outside cooldown. No residents will be invented.'
        return preview

    def _approve_migration(self, payload):
        shape(payload, ('previewId', 'npcId'))
        preview = self._get('migration', ident(payload['previewId']))
        npc_id = ident(payload['npcId'])
        self._require_live()
        checked(preview and preview['worldId'] == self.snapshot['worldId'] and parse_utc(preview['expiresAt']) >= self.clock(),
                'Migration preview expired or belongs to another world; generate a new preview.', 409)
        selected = next((candidate for candidate in preview['candidates'] if candidate['npcId'] == npc_id), None)
        refreshed = self._migration_preview(preview['chapterId'])
        current = next((candidate for candidate in refreshed['candidates'] if candidate['npcId'] == npc_id), None)
        checked(selected and current and all(selected[key] == current[key] for key in
                ('sourceCell', 'destinationCell', 'sourceFactions', 'resentment', 'x', 'y')),
                'Candidate, capacity, or terms changed. Review a new preview before approval.', 409)
        event = self._create_event({'title': f'Relocation: {selected["name"]}', 'kind': 'npc_relocate',
                'chapterId': preview['chapterId'], 'payload': {'npc': npc_id, 'cell': selected['destinationCell'],
                                                             'x': selected['x'], 'y': selected['y']}})
        event['migration'] = {'chapterId': preview['chapterId'], 'previewId': preview['id'], 'sourceCell': selected['sourceCell'],
                              'sourceFactions': selected['sourceFactions'], 'resentment': selected['resentment']}
        event['approvedAt'] = utc(self.clock())
        self._prepare(event)
        self._put('event', event)
        return self._public_event(event)

    def _arrival(self, event):
        migration = event['migration']
        self.db.execute('INSERT INTO arrivals VALUES(?,?,?) ON CONFLICT(npc) DO UPDATE SET at=excluded.at,event=excluded.event',
                        (event['payload']['npc'], self.clock(), event['id']))
        for faction in migration['sourceFactions']:
            if migration['resentment'] <= 0:
                continue
            identifier = hashlib.sha256(f'{faction}:chapter:{migration["chapterId"]}'.encode()).hexdigest()
            previous = self._get('opinion', identifier)
            score = max(-100, (previous['score'] if previous else 0) - migration['resentment'])
            self._opinion(faction, 'chapter', migration['chapterId'], score,
                          f'Verified arrival of {event["payload"]["npc"]} from {migration["sourceCell"]}; bounded loss after treaty modifier.')
        self._audit('migration.arrived', event['id'], 'Verified authority home change; cooldown and source-claim opinions updated once.')


def handler_class(service, token, frontend):
    frontend = Path(frontend).resolve()

    class Handler(BaseHTTPRequestHandler):
        server_version = 'Storykeeper/1'

        def log_message(self, *_):
            pass  # URLs, tokens, narrative plans, and authorization headers never enter logs.

        def _send(self, code, body, content_type='application/json; charset=utf-8'):
            data = encode(body).encode() if isinstance(body, dict) else body
            self.send_response(code)
            self.send_header('Content-Type', content_type)
            data = http_body.send(self, data, content_type)
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Referrer-Policy', 'no-referrer')
            self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'")
            self.end_headers()
            self.wfile.write(data)

        def _guard(self, api=False):
            port = self.server.server_address[1]
            allowed = {f'127.0.0.1:{port}', f'localhost:{port}'}
            checked(ipaddress.ip_address(self.client_address[0]).is_loopback, 'Loopback clients only.', 403)
            checked(len(self.headers.get_all('Host', [])) == 1 and self.headers.get('Host') in allowed, 'Invalid Host.', 403)
            origins = self.headers.get_all('Origin', [])
            checked(len(origins) <= 1 and (not origins or origins[0] == 'http://' + self.headers['Host']), 'Invalid Origin.', 403)
            checked(not urlsplit(self.path).query and '#' not in self.path, 'Query parameters and fragment tokens are not accepted by the server.', 400)
            if api:
                values = self.headers.get_all('Authorization', [])
                checked(len(values) == 1 and values[0].isascii() and
                        hmac.compare_digest(values[0], 'Bearer ' + token), 'Authentication required.', 401)

        def do_GET(self):
            try:
                path = urlsplit(self.path).path
                self._guard(path.startswith('/api'))
                if path == '/api/state':
                    self._send(200, service.state())
                    return
                checked(not path.startswith('/api'), 'API route not found.', 404)
                decoded = unquote(path)
                checked(decoded in ('/', '/index.html', '/app.js', '/app.mjs', '/model.mjs', '/terrain.generated.mjs', '/style.css'),
                        'Static route not found.', 404)
                file = frontend / ('index.html' if decoded == '/' else decoded.lstrip('/'))
                checked(file.parent == frontend and not file.is_symlink(), 'Invalid static file.', 404)
                types = {'.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
                         '.mjs': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8'}
                self._send(200, read_file(file, 2 * 1024 * 1024), types[file.suffix])
            except DMError as error:
                self._send(error.status, {'ok': False, 'error': str(error)})
            except OSError:
                self._send(404, {'ok': False, 'error': 'Frontend file unavailable.'})
            except database_errors():
                self._send(503, {'ok': False, 'error': 'Local state is unavailable.'})

        def do_POST(self):
            try:
                self._guard(True)
                checked(urlsplit(self.path).path == '/api/command', 'API route not found.', 404)
                checked(not self.headers.get('Transfer-Encoding'), 'Transfer encoding is unsupported.', 400)
                lengths = self.headers.get_all('Content-Length', [])
                checked(len(lengths) == 1 and lengths[0].isdigit(), 'Content-Length is required.', 411)
                size = int(lengths[0])
                checked(0 < size <= MAX_BODY, 'Request body exceeds its limit.', 413)
                checked(self.headers.get('Content-Type', '').split(';')[0].strip().lower() == 'application/json', 'JSON content type required.', 415)
                self.connection.settimeout(5)
                raw = self.rfile.read(size)
                checked(len(raw) == size, 'Truncated request body.', 400)
                self._send(200, service.command(decode(raw)))
            except DMError as error:
                self._send(error.status, {'ok': False, 'error': str(error)})
            except (OSError, database_errors()):
                self._send(503, {'ok': False, 'error': 'Local operation failed; no completion is implied. Inspect the local service.'})

        def do_OPTIONS(self):
            try:
                self._guard(urlsplit(self.path).path.startswith('/api'))
                self._send(405, {'ok': False, 'error': 'Cross-origin access is unsupported.'})
            except DMError as error:
                self._send(error.status, {'ok': False, 'error': str(error)})

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--state-dir', type=Path, default=ROOT / 'Saved/DM')
    parser.add_argument('--exchange', type=Path, default=ROOT / 'Saved/DMBridge')
    parser.add_argument('--port', type=int, default=8780)
    parser.add_argument('--frontend', type=Path, default=ROOT / 'DM')
    parser.add_argument('--database', choices=('dev', 'prod'), default='dev',
                        help='whose dm schema holds the Storykeeper state (default dev)')
    args = parser.parse_args()
    global DATABASE
    DATABASE = args.database
    checked(1 <= args.port <= 65535, 'Invalid local port.')
    service = DMService(args.state_dir, args.exchange)
    token = secrets.token_urlsafe(32)
    server = ThreadingHTTPServer(('127.0.0.1', args.port), handler_class(service, token, args.frontend))
    server.daemon_threads = True
    atomic_json(service.state_dir / 'session.json', {'url': f'http://127.0.0.1:{args.port}/#token={token}'})
    stopping = threading.Event()

    def scheduler():
        while not stopping.wait(1):
            try:
                service.tick()
            except (OSError, database_errors(), DMError):
                # Failure never changes a queued effect into an applied one.
                service.bridge_error = 'Local service processing failed; inspect the private state directory.'

    worker = threading.Thread(target=scheduler, daemon=True)
    worker.start()
    print(f'Storykeeper listening on loopback port {args.port}. Open the URL in private {service.state_dir / "session.json"}.', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        stopping.set()
        worker.join(5)
        server.server_close()
        service.close()


if __name__ == '__main__':
    main()
