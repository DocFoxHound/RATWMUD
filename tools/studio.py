#!/usr/bin/env python3
"""RATW Studio: one tool, two suites (Docs/Design/66-worlds-atlas-and-dm.md, section 3).

  bash tools/studio.sh [--port N] [--suite atlas|dm]     build the UI when its sources changed, then serve it
  python3 tools/studio.py serve [--port N] [--suite atlas|dm] [--world KEY]
  bash tools/editor.sh / bash tools/dungeon-master.sh   the same, opening on the Atlas / DM suite

One host on one port (default 8765, on this computer and the local network: --host 0.0.0.0, the default; --host
127.0.0.1 for this computer only) serves one app (Editor/index.html) with a switch between:

  Atlas  builds worlds, on dev worlds (its world picker switches Atlas's world, as `world_db.py use KEY` does): the
         map and terrain, cells, interiors, doors, places, the roster and NPCs, their homes and jobs, and (moved from
         the DM) patrol routes, NPC areas, spawn rules, wander areas, factions and talk. Its routes:
         /api/atlas/...          tools/map_editor.py's Handler (its own session token still guards every write);
         /api/atlas/layers/...   the DM's NPC, layer, faction and talk methods (tools/dungeon_master.py), only on
                                 Atlas's world. No storylines, no Push to live (the DM's Import from dev).
         /api/atlas/sync/...     Update from live (tools/world_sync.py; doc 66, section 4): GET diff, POST pull
                                 {choices, expect}, on Atlas's world when it has an upstream.
         /api/atlas/economy...   the Economy page (Docs/Design/79-a-staged-economy.md, 12 and 14): GET economy (the
                                 EconomyView: the world's economy entity, the slider, the rules, services, season
                                 groups, goods and the data files' defaults beside them), and POST economy/<x>:
                                 steer {prosperity, cause?, now?} (the slider: clamped, never refused), needs
                                 {needsPaused}, rules {set: {rule: N or null}} (not prosperity), services {set: {id:
                                 {on?, price?, every?} or null}}, seasons {set: {group: {price?, shelf?} or null}};
                                 and with migration 0061 (409 before it) the host's overrides: imports {neverImported:
                                 [good] or null}, status {status: {part of status.json} or null}, market {market:
                                 {events?, wharf?} or null}. Each POST replies {changed, detail, sync, warnings,
                                 ...EconomyView}. Reads for anyone signed in, writes for dm and admin with Atlas's token.
         /api/dm/economy...      the same on the DM's world (GET ?world=KEY; POST {world, ...}): for dm and admin.
                                 Every change (Atlas's too) is audited as economy.<x> with its before and after, and
                                 queues economy.sync for the world's running game server, which reads the world's
                                 live.economy again and applies it at once (the slider's {now}: the times jump).
  DM     runs live worlds: the server, live NPC life (kill, revive, move, visit), players, reports, artwork, the
         calendar, prosperity, health, storytellers, chapters. Its routes: /api/dm/... (tools/dungeon_master.py's Handler)
         without the map and NPC authoring that moved to Atlas, and only on live worlds; and the server and imports
         (tools/world_sync.py; doc 66, section 4):
         GET  /api/dm/server/status?world=KEY     a live world's state (running or maintenance), its game server, release
         POST /api/dm/server/maintenance          {world, on, note}: close it (players get a minute) or turn it back on
         POST /api/dm/server/switch               {world, confirm: the world's key again}: the live world, in maintenance
                                                  (any registered world: a dev world is served as live)
         POST /api/dm/server/rebuild              {world}: a dev world served as live: a new build of Atlas's edits,
                                                  announced (its server restarts onto it once nobody is connected)
       The DM works on the live worlds and on the world the live server runs, which may be a dev world served as live
       (DEV while PROD is shut down: doc 66, Progress); there Import from dev and rollback are refused with why.
         GET  /api/dm/import/plan?dev=KEY         the dev worlds copied from the live world, and what importing one does
         POST /api/dm/import                      {dev, choices, password, note, expect}: a release, in maintenance
         GET  /api/dm/releases?world=KEY, POST /api/dm/rollback {number, password, note}

Errors from the engine keep their meaning: refused (422, with `errors`), a wrong publish password (401), locked out
(429), conflicts left to choose (409, with `conflicts`, the plan's items), not in maintenance or the worlds moved since
the plan (409). Every pull, import, maintenance change, switch and rollback is audited in that world's DM audit.

One sign-in for the whole tool: the DM accounts (dm.admins in PROD; tools/dungeon_master.py accounts ...). Atlas is
for dm and admin accounts; a viewer sees the DM suite only. The Storykeeper (tools/dm_service.py) stays its own
service, as before. Every request needs the local origin, as both hosts did.
"""
from __future__ import annotations

import argparse
import copy
import errno
import http.client
import json
import os
import socket
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

import dungeon_master as D
import economy_contract as C
import map_editor
import world_db
import world_store as S

PORT = 8765
SUITES = ('atlas', 'dm')
# What moved from the DM to Atlas (doc 66, section 3): making and changing NPCs and the layers they live by (patrol
# routes, NPC areas, spawn rules, wander areas), factions and talk. The DM suite doesn't offer them; Atlas's NPC
# pages reach them under /api/atlas/layers/, on Atlas's world only.
MOVED_TO_ATLAS = frozenset({
    '/api/npcs/save', '/api/npcs/delete', '/api/npcs/wander',
    '/api/routes/save', '/api/routes/delete', '/api/areas/save', '/api/areas/delete', '/api/spawns/save', '/api/spawns/delete',
    '/api/factions/save', '/api/factions/delete', '/api/factions/claim', '/api/factions/unclaim', '/api/factions/relation',
    '/api/factions/member',
    '/api/talk/voice', '/api/talk/kin', '/api/talk/dialect', '/api/talk/figure',
})
# What those pages read (and the queued syncs' answers, /api/actions/ID).
LAYER_READS = frozenset({'/api/npcs', '/api/world', '/api/ground', '/api/factions', '/api/factions/history', '/api/talk',
                         '/api/talk/dialects', '/api/talk/figures'})
# Push to live leaves Atlas: it becomes the DM's Import from dev (doc 66, Phase 4).
NOT_IN_ATLAS = frozenset({'/api/publish', '/api/publish/push', '/api/publish/rollback', '/api/publish/pull'})
PUSH_MOVED = ('Push to live has gone: Atlas updates its copy from live (Update from live), and the DM\'s Server & import '
              'tab does the Import from dev (doc 66, section 4).')
LIVE_SWITCH = 'Switching the live server\'s world is done in the DM\'s Server & import tab, during maintenance only.'
# The DM suite's server and import routes (doc 66, section 4), answered by the studio itself over tools/world_sync.py.
SERVER_ROUTES = frozenset({('GET', '/api/server/status'), ('POST', '/api/server/maintenance'), ('POST', '/api/server/switch'),
                           ('POST', '/api/server/rebuild'), ('POST', '/api/server/start'), ('GET', '/api/import/plan'),
                           ('POST', '/api/import'),
                           ('GET', '/api/releases'), ('POST', '/api/rollback')})
SYNC_ROUTES = frozenset({('GET', '/api/sync/diff'), ('POST', '/api/sync/pull')})
# Atlas's Play: the running game server of Atlas's world (its address), and starting one.
PLAY_ROUTES = frozenset({('GET', '/api/play/server'), ('POST', '/api/play/server/start')})
# The Economy controls (Docs/Design/79-a-staged-economy.md, 12 and 14; Phase 9): on Atlas's world (/api/atlas/economy...)
# and on the DM's (/api/dm/economy...), the same set. GET for any signed-in account; each POST for dm and admin, audited
# as economy.<x> with the before and after, and sent to the world's running server (economy.sync). The names are stable
# for doc 78 M3's allowlist.
ECONOMY_KINDS = ('steer', 'needs', 'rules', 'services', 'seasons', 'imports', 'status', 'market')
ECONOMY_ROUTES = frozenset({('GET', '/api/economy')} | {('POST', f'/api/economy/{k}') for k in ECONOMY_KINDS})
DM_ECONOMY_ROUTES = ECONOMY_ROUTES
SLIM_KINDS = ('imports', 'status', 'market')   # The host's overrides: migration 0061 only.
ECONOMY_NOTE = ('Per world (live.economy): never carried by Update from live or Import from dev, and kept by an update of '
                'the world (each world\'s own lever).')
TAKES_EFFECT = ('Applied to the world\'s running game server within a few seconds (economy.sync), and read by any server '
                'started on it later. Data/Economy/market.json and status.json themselves are the defaults, read when a '
                'server starts.')
NO_RULES_HERE = ('This world lacks migration 0058: its rules, seasons and services are as Data/Economy/market.json and '
                 'status.json have them and can\'t be changed here.')
NO_SLIM_HERE = 'This world needs migration 0061.'
STEER_NOTE = ('The slider moves the times toward its mark over a few days (or at once with "now"); 0 is ordinary times. '
              'Its cause is what the town says of it ("the Crown\'s war levy"); empty: a cause the world picks.')


def _number(v) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool)


class Economy:
    """A world's economy settings (doc 79, 12): the slider and its cause, needsPaused, the rules, services and seasons,
    and (migration 0061) the host's overrides of status.json and market.json and the goods ships never carry. The entity
    is world_store's (economy_value / save_economy / patch_economy_rules); a dev world's changes go through live_edit
    (logged, so open Atlas sessions follow them), a live world's straight to its row."""

    def __init__(self, connect=None, connect_live=None, connect_dm=None):
        self._connect = connect or (lambda key: world_db.connect(key, 'editor'))
        # A live world is the DM's: its own role, not Atlas's.
        self._connect_live = connect_live or (connect if connect else (lambda key: world_db.connect(key, 'dm')))
        # The DM's queue (dm.actions) on any world: economy.sync for its running server.
        self._connect_dm = connect_dm or (lambda key: world_db.connect(key, 'dm'))

    def queue_sync(self, key, who, payload=None) -> int:
        """economy.sync for the world's running game server: it reads live.economy again and applies it at once
        (`payload` {now: true}: the slider jumps to its mark). Expires unapplied after ten minutes when no server runs
        (a server started later reads the row)."""
        with self._connect_dm(key) as conn:
            action = conn.execute("INSERT INTO dm.actions (kind, target_id, requested_by, payload) "
                                  "VALUES ('economy.sync', 'economy', %s, %s) RETURNING id",
                                  (who, json.dumps(payload or {}))).fetchone()[0]
            conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action}),))
        return action

    @staticmethod
    def live(key) -> bool:
        try:
            return world_db.kind(key) == 'live'
        except world_db.DatabaseError:
            return False

    def connect(self, key):
        try:
            return (self._connect_live if self.live(key) else self._connect)(key)
        except world_db.DatabaseError as error:
            raise S.Unavailable(f'Cannot reach the {key} world\'s database: {error}') from error

    # -- the world's level ------------------------------------------------------------------------------------------
    @staticmethod
    def levels(conn) -> tuple[bool, bool]:
        """(supported, slim): the rules, seasons and services columns (migration 0058), and migration 0061."""
        columns = set(S.tables_for(conn)['live.economy'][1])
        supported = {'rules', 'seasons', 'services', 'needs_paused'} <= columns
        return supported, supported and S.slim_supported(conn)

    @staticmethod
    def entity(stored) -> dict:
        """The economy entity of contract 1.3 from world_store's (the integer rules apart from the overrides, whichever
        of the two shapes world_store gives; dailyHerbs and dailyMeals read past)."""
        e = stored or S.default_economy()
        raw = e.get('rules') or {}
        overrides = {k: copy.deepcopy(raw[k]) for k in C.OVERRIDE_KEYS if k in raw}
        overrides.update(copy.deepcopy(e.get('overrides') or {}))
        return {'treasury': int(e.get('treasury', 1000)), 'storeHerbs': int(e.get('storeHerbs', 100)),
                'storeMeals': int(e.get('storeMeals', 50)), 'needsPaused': bool(e.get('needsPaused', False)),
                'seasons': copy.deepcopy(e.get('seasons') or {}),
                'rules': {k: v for k, v in raw.items() if _number(v) and k not in C.OVERRIDE_KEYS},
                'services': copy.deepcopy(e.get('services') or {}), 'overrides': overrides}

    # -- the view ---------------------------------------------------------------------------------------------------
    @staticmethod
    def rule_list() -> list[dict]:
        """Core's rules (map_editor.RULES, Rules[]) less the slider's, with the data files' defaults."""
        defaults = map_editor.rule_defaults()
        return [{'name': name, 'low': low, 'high': high, 'fallback': defaults.get(name, fallback),
                 'switch': name in map_editor.RULE_SWITCHES}
                for name, (low, high, fallback) in map_editor.RULES.items() if name != 'prosperity']

    @staticmethod
    def season_groups() -> list[dict]:
        return [{'id': g['id'], 'name': g.get('name', g['id']), 'about': g.get('about', ''),
                 'goods': list(g.get('goods') or []), 'price': list(g.get('price') or [100] * 4),
                 'shelf': list(g.get('shelf') or [100] * 4)} for g in map_editor.season_groups()]

    @staticmethod
    def goods() -> list[dict]:
        """The catalogue's goods, for the goods ships never carry."""
        names = {i['id']: i.get('name', i['id']) for i in C.items_file().get('items') or [] if isinstance(i, dict)}
        return [{'id': g, 'name': names.get(g, g)} for g in C.base_goods()]

    @staticmethod
    def defaults() -> dict:
        d, market = C.load_defaults(), map_editor.market_file()
        events = (market.get('events') or {})
        kinds = [{'id': k['id'], 'words': k.get('words', ''), 'needs': [k['needs']] if k.get('needs') else []}
                 for k in events.get('list') or [] if isinstance(k, dict) and k.get('id')]
        wharf = market.get('wharf') or {}
        return {'status': d['status'],
                'market': {'events': {'perWeek': events.get('perWeek', 2), 'list': kinds,
                                      'causes': copy.deepcopy(events.get('causes') or {})},
                           'wharf': {'ships': list(wharf.get('ships') or []), 'never': list(wharf.get('never') or [])}}}

    def view(self, conn, w) -> dict:
        supported, slim = self.levels(conn)
        e = self.entity(S.economy_value(conn, w))
        return {'world': w, 'supported': supported, 'slim': slim,
                'problem': '' if supported else NO_RULES_HERE,
                'note': ECONOMY_NOTE, 'takesEffect': TAKES_EFFECT, 'steerNote': STEER_NOTE, 'economy': e,
                'prosperity': {'value': C.clamp_prosperity(e['rules'].get('prosperity', 0)),
                               'cause': e['overrides'].get('prosperityCause') or '', **C.PROSPERITY},
                'ruleList': self.rule_list(),
                'heldRules': sorted(map_editor.unknown_rules(e['rules'])),
                'serviceList': map_editor.service_list(), 'seasonNames': list(map_editor.SEASON_NAMES),
                'seasonGroups': self.season_groups(), 'goods': self.goods(), 'defaults': self.defaults()}

    def world_of(self, conn, key) -> str:
        world = conn.execute('SELECT id FROM world.worlds').fetchone()
        if not world:
            raise D.DMError(f'{key} has no world yet.', 409)
        return world[0]

    def read(self, key) -> dict:
        with self.connect(key) as conn:
            return self.view(conn, self.world_of(conn, key))

    # -- writing ----------------------------------------------------------------------------------------------------
    def _edit(self, key, editor, label, change, slim=False) -> dict:
        """One change of the economy entity: {changed, before, after, view}. `change(entity) -> entity`; `slim`: it needs
        migration 0061. A dev world's goes through live_edit (logged); a live world's is the row, as the DM writes it."""
        with self.connect(key) as conn:
            supported, slimmed = self.levels(conn)
            if not supported:
                raise D.DMError(f'{NO_RULES_HERE} (python3 tools/world_db.py migrate --db {key}).', 409)
            if slim and not slimmed:
                raise D.DMError(f'{NO_SLIM_HERE} (python3 tools/world_db.py migrate --db {key}).', 409)
            w = self.world_of(conn, key)
            stored = S.economy_value(conn, w)
            before = self.entity(stored)
            after = change(copy.deepcopy(before))
            if canonical(before) == canonical(after):
                return {'changed': False, 'before': before, 'after': after, 'view': self.view(conn, w)}
            # (Only what this change brings is refused, a bad override with it: doc 79, 14.)
            had = set(map_editor.check_economy(before, strict_overrides=True))
            problems = [p for p in map_editor.check_economy(after, strict_overrides=True) if p not in had]
            if problems:
                raise D.DMError(' '.join(problems), 400)
            # (The slider's cause is stored on a world before 0061 too: the rest of the overrides need it.)
            past_cause = lambda o: {k: v for k, v in (o or {}).items() if k != 'prosperityCause'}
            if not slimmed and canonical(past_cause(after['overrides'])) != canonical(past_cause(before['overrides'])):
                raise D.DMError(f'{NO_SLIM_HERE} (python3 tools/world_db.py migrate --db {key}).', 409)
            try:
                if self.live(key):
                    S.save_economy(conn, w, after)
                else:
                    import live_edit
                    old = stored if stored is not None else S.default_economy()
                    apply_edit(live_edit, conn, editor, label, [{'key': 'economy', 'before': old, 'after': after}])
            except S.EconomyRefused as error:
                raise D.DMError(str(error), 409) from error
            return {'changed': True, 'before': before, 'after': after, 'view': self.view(conn, w)}

    def set_steer(self, key, data, editor) -> dict:
        """The slider (doc 79, 12): {prosperity: anything (clamped, never refused: 9999 is 100, "abc" 0, 37 35), cause?:
        plain words of at most 80 bytes ("" or null: the world picks one), now?: true (the times jump to the mark)}.
        On a live world written through patch_economy_rules (the rule `prosperity` and `prosperityCause`), as the DM
        writes the row; on a dev world through live_edit, as every other setter, so open Atlas sessions follow it and its
        log holds it. A 0058-0060 world takes it too. `now` with the value and cause as they are is still a change: the
        times go to the mark at once (economy.sync {now}), a slider still easing included."""
        if not isinstance(data, dict) or 'prosperity' not in data or set(data) - {'prosperity', 'cause', 'now'}:
            raise D.DMError('Send {"prosperity": -100 to 100, "cause"?: words, "now"?: true or false}.', 400)
        value = C.clamp_prosperity(data['prosperity'])
        cause = data.get('cause', ...)
        if cause is not ... and cause is not None:
            words = C.plain_words(cause, C.CAUSE_MOST) if isinstance(cause, str) else None
            if words is None:
                raise D.DMError(f'The cause is plain words of at most {C.CAUSE_MOST} bytes (one line, never the forbidden '
                                'word).', 400)
            cause = words.strip()
        now = data.get('now') is True
        said = lambda v, c: f'{v:+d}' + (f' ({c})' if c else '')
        with self.connect(key) as conn:
            supported, _ = self.levels(conn)
            if not supported:
                raise D.DMError(f'{NO_RULES_HERE} (python3 tools/world_db.py migrate --db {key}).', 409)
            w = self.world_of(conn, key)
            before = self.entity(S.economy_value(conn, w))
        old = (C.clamp_prosperity(before['rules'].get('prosperity', 0)), before['overrides'].get('prosperityCause') or '')
        new = (value, old[1] if cause is ... else (cause or ''))
        if new != old and self.live(key):
            with self.connect(key) as conn:
                w = self.world_of(conn, key)
                drop = [] if new[1] else ['prosperityCause']
                wanted = {'prosperity': value, **({'prosperityCause': new[1]} if new[1] else {})}
                S.patch_economy_rules(conn, w, wanted, drop)
        elif new != old:
            def change(e):
                overrides = {k: v for k, v in e['overrides'].items() if k != 'prosperityCause'}
                if new[1]:
                    overrides['prosperityCause'] = new[1]
                return {**e, 'rules': {**e['rules'], 'prosperity': value}, 'overrides': overrides}
            self._edit(key, editor, f'Prosperity {said(*old)} -> {said(*new)}', change)
        with self.connect(key) as conn:
            view = self.view(conn, self.world_of(conn, key))
        return {'changed': new != old or now, 'before': {'prosperity': old[0], 'cause': old[1]},
                'after': {'prosperity': new[0], 'cause': new[1]}, 'view': view, 'now': now,
                'detail': f'Prosperity {said(*old)} -> {said(*new)}' + (', now' if now else '') + '.'}

    def set_needs(self, key, data, editor) -> dict:
        """{needsPaused: true|false}: residents' needs paused (maintenance and testing)."""
        if not isinstance(data, dict) or set(data) != {'needsPaused'} or type(data['needsPaused']) is not bool:
            raise D.DMError('Send {"needsPaused": true or false}.', 400)
        detail = f'Residents\' needs {"paused" if data["needsPaused"] else "on"}.'
        return {**self._edit(key, editor, detail, lambda e: {**e, 'needsPaused': data['needsPaused']}), 'detail': detail}

    def set_rules(self, key, data, editor) -> dict:
        """{set: {rule: whole number, or null for the data files'}}. Not `prosperity`: the slider's (steer)."""
        wanted = data.get('set') if isinstance(data, dict) else None
        names = [r['name'] for r in self.rule_list()]
        if not isinstance(wanted, dict) or not wanted:
            raise D.DMError('Send {"set": {rule: whole number or null}}; the rules: ' + ', '.join(names) + '.', 400)
        if 'prosperity' in wanted:
            raise D.DMError('prosperity is the slider\'s: use the slider (POST economy/steer).', 400)
        unknown = sorted(set(wanted) - set(names))
        if unknown:
            raise D.DMError(f'No such rule: {", ".join(unknown[:8])} (the rules: {", ".join(names)}).', 400)
        problems = map_editor.check_rules({k: v for k, v in wanted.items() if v is not None}, {})
        if problems:
            raise D.DMError(' '.join(problems), 400)

        def change(e):
            rules = dict(e['rules'])
            for name, value in wanted.items():
                if value is None:
                    rules.pop(name, None)
                else:
                    rules[name] = value
            return {**e, 'rules': rules}
        said = ', '.join(f'{k} {"as the data files have it" if v is None else v}' for k, v in wanted.items())
        return {**self._edit(key, editor, f'The rules: {said}', change), 'detail': f'The rules: {said}.'}

    def set_services(self, key, data, editor) -> dict:
        """{set: {service: {on?, price?, every?} or null}}: any of a service's three at a time (the rest as it is now), or
        null: back to market.json's (and items.json's price)."""
        wanted = data.get('set') if isinstance(data, dict) else None
        if not isinstance(wanted, dict) or not wanted:
            raise D.DMError('Send {"set": {service: {on?: true|false, price?: pennies, every?: days} or null}}.', 400)
        listed = {s['id']: s for s in map_editor.service_list()}
        unknown = sorted(set(wanted) - set(listed))
        if unknown:
            raise D.DMError(f'No such service in Data/Economy/market.json: {", ".join(unknown[:8])} (there are '
                            f'{", ".join(listed) or "none"}).', 400)
        for sid, values in wanted.items():
            if values is not None and (not isinstance(values, dict) or not values or set(values) - {'on', 'price', 'every'}):
                raise D.DMError(f'{sid}: send any of on, price and every, or null.', 400)

        def base(sid):
            return {'on': True, 'price': listed[sid]['price'], 'every': listed[sid]['every']}

        def change(e):
            services = copy.deepcopy(e['services'])
            for sid, values in wanted.items():
                merged = None if values is None else {**(services.get(sid) or base(sid)), **values}
                if merged is None or merged == base(sid):
                    services.pop(sid, None)        # (Back at market.json's: no entry kept for it.)
                else:
                    services[sid] = merged
            problems = map_editor.check_rules({}, services)
            if problems:
                raise D.DMError(' '.join(problems), 400)
            return {**e, 'services': services}

        def told(sid, values):
            if values is None:
                return f'{sid} as market.json has it'
            return f'{sid} ' + ', '.join(f'{k} {("on" if v else "off") if isinstance(v, bool) else v}' for k, v in values.items())
        said = '; '.join(told(s, v) for s, v in wanted.items())
        return {**self._edit(key, editor, f'The services: {said}', change), 'detail': f'The services: {said}.'}

    def set_seasons(self, key, data, editor) -> dict:
        """{set: {group: {price?, shelf?} or null}}: a group's price or shelf, four percents (Spring, Summer, Autumn,
        Winter), the other as it is now; null: back to market.json's. On a world before 0061 the record keeps its four
        parts (supply and normal as they were, read past), so 0060's season line still reads."""
        wanted = data.get('set') if isinstance(data, dict) else None
        if not isinstance(wanted, dict) or not wanted:
            raise D.DMError('Send {"set": {group: {price?, shelf?: [Spring, Summer, Autumn, Winter] percents} or null}}.', 400)
        groups = {g['id']: g for g in self.season_groups()}
        unknown = sorted(set(wanted) - set(groups))
        if unknown:
            raise D.DMError(f'No such season group in Data/Economy/market.json: {", ".join(unknown[:8])} (there are '
                            f'{", ".join(groups) or "none"}).', 400)
        for group, values in wanted.items():
            if values is not None and (not isinstance(values, dict) or not values or set(values) - {'price', 'shelf'}):
                raise D.DMError(f'{group}: send price or shelf (four percents each), or null.', 400)
        with self.connect(key) as conn:
            slim = self.levels(conn)[1]

        def change(e):
            seasons = copy.deepcopy(e['seasons'])
            for group, values in wanted.items():
                g = groups[group]
                file = {'price': g['price'], 'shelf': g['shelf']}
                had = seasons.get(group) or {}
                mine = {k: list(had.get(k) or file[k]) for k in ('price', 'shelf')}
                if values is not None:
                    mine.update(values)
                if values is None or mine == file:
                    seasons.pop(group, None)      # (Back at market.json's: no row kept for it.)
                elif slim:
                    seasons[group] = mine
                else:
                    seasons[group] = {'supply': list(had.get('supply') or [100] * 4), **mine,
                                      'normal': int(had.get('normal') or 0)}
            return {**e, 'seasons': seasons}

        def told(group, values):
            if values is None:
                return f'{group} as market.json has it'
            return f'{group} ' + ', '.join(f'{k} {"/".join(map(str, v)) if isinstance(v, list) else v}' for k, v in values.items())
        said = '; '.join(told(g, v) for g, v in wanted.items())
        return {**self._edit(key, editor, f'The seasons: {said}', change), 'detail': f'The seasons: {said}.'}

    def _override(self, key, editor, name, value, words) -> dict:
        """One override set whole (None: back to the data files'), checked as Core reads it (economy_contract)."""
        if value is not None:
            errors, warnings = C.check_overrides({name: value}, C.base_goods(), C.load_defaults())
            if errors:
                raise D.DMError(' '.join(errors[:12]), 400)
        else:
            warnings = []

        def change(e):
            overrides = dict(e['overrides'])
            if value is None:
                overrides.pop(name, None)
            else:
                overrides[name] = value
            return {**e, 'overrides': overrides}
        detail = f'{words}: ' + ('as the data files have it.' if value is None else "this world's own.")
        return {**self._edit(key, editor, detail, change, slim=True), 'detail': detail, 'warnings': warnings}

    def set_imports(self, key, data, editor) -> dict:
        """{neverImported: [good] or null}: the goods ships never carry here (in place of market.json's wharf.never)."""
        if not isinstance(data, dict) or set(data) != {'neverImported'}:
            raise D.DMError('Send {"neverImported": [good, ...] or null (market.json\'s)}.', 400)
        never = data['neverImported']
        if isinstance(never, list):
            never = sorted(set(never), key=never.index) if all(isinstance(g, str) for g in never) else never
        result = self._override(key, editor, 'neverImported', never, 'The goods ships never carry')
        if never is not None:
            result['detail'] = f'The goods ships never carry: {", ".join(never) or "none"}.'
        return result

    def set_status(self, key, data, editor) -> dict:
        """{status: {a part of status.json} or null}: the host's override of status.json, replaced whole (objects are
        merged over the file's by Core, arrays and numbers replace)."""
        if not isinstance(data, dict) or set(data) != {'status'}:
            raise D.DMError('Send {"status": {part of status.json} or null (the file\'s)}.', 400)
        status = data['status']
        if status == {}:
            status = None
        result = self._override(key, editor, 'status', status, 'The bands and street (status.json)')
        if status:
            result['detail'] = f'The bands and street (status.json): this world sets {", ".join(sorted(status))}.'
        return result

    def set_market(self, key, data, editor) -> dict:
        """{market: {events?: {perWeek?, list?, causes?}, wharf?: {ships?}} or null}: the host's override of
        market.json's staged events and ships, replaced whole. An event kind given by its id alone is market.json's own,
        stored whole (Core reads kinds whole)."""
        if not isinstance(data, dict) or set(data) != {'market'}:
            raise D.DMError('Send {"market": {events?: {perWeek?, list?, causes?}, wharf?: {ships?}} or null.', 400)
        market = copy.deepcopy(data['market'])
        if market == {}:
            market = None
        # (Any shape at all: one that isn't objects is check_overrides' to refuse with a 400, never a 500 here.)
        events = market.get('events') if isinstance(market, dict) else None
        kinds = events.get('list') if isinstance(events, dict) else None
        if isinstance(kinds, list):
            shipped = {k['id']: k for k in (map_editor.market_file().get('events') or {}).get('list') or []
                       if isinstance(k, dict) and k.get('id')}
            for i, k in enumerate(kinds):
                if isinstance(k, str):
                    if k not in shipped:
                        raise D.DMError(f'market.events.list[{i}]: market.json has no event kind {k!r} (it has '
                                        f'{", ".join(shipped)}).', 400)
                    kinds[i] = copy.deepcopy(shipped[k])
        result = self._override(key, editor, 'market', market, 'The staged events and ships (market.json)')
        if market:
            parts = [f'{a}.{b}' for a, b in C.MARKET_PATHS if b in (market.get(a) or {})]
            result['detail'] = f'The staged events and ships (market.json): this world sets {", ".join(parts)}.'
        return result


def canonical(value) -> str:
    return json.dumps(value, sort_keys=True, separators=(',', ':'))


def apply_edit(live_edit, conn, editor, label, ops):
    """live_edit's keyed edit, the studio's own: it queues economy.sync itself, so live_edit's own queuing (its
    queue_sync, doc 79 Phase 9's U9) is off where live_edit has it."""
    import inspect
    options = {'queue_sync': False} if 'queue_sync' in inspect.signature(live_edit.apply_edit).parameters else {}
    return live_edit.apply_edit(conn, 'studio-economy', editor, label[:200], ops, **options)


SWITCH_WARNING = 'Switching the live world is rarely wise: players\' characters and progress belong to each world.'


def dm_offers(method: str, path: str) -> bool:
    """The DM suite: everything the DM host had but the authoring that moved to Atlas."""
    return path not in MOVED_TO_ATLAS


def layer_offers(method: str, path: str) -> bool:
    """Atlas's NPC pages: the moved authoring and what it reads; no sign-in, storyline or live-life routes."""
    if method == 'POST':
        return path in MOVED_TO_ATLAS
    return path in LAYER_READS or (path.startswith('/api/actions/') and path.count('/') == 3)


def atlas_suite(key: str):
    """Atlas's Handler on a dev world's database (Push to live has gone: NOT_IN_ATLAS answers its old paths)."""
    import live_edit
    return map_editor.make_handler(world=live_edit.LiveWorld(key=key))


def shown(key: str) -> dict:
    """A registered world as the app sees it (no database name)."""
    return {k: v for k, v in world_db.world(key).items() if k != 'database'}


def dm_worlds() -> list[str]:
    """The worlds the DM suite may work on: every registered world (a live one, or a dev world run as a server: DEV
    while PROD is shut down, doc 66, Progress, "DEV as the server"), the DM's chosen world first."""
    out = world_db.keys()
    try:
        chosen = world_db.dm_world()
    except world_db.DatabaseError:
        return out
    return [chosen] + [k for k in out if k != chosen]


def dm_world() -> str:
    """The DM suite's world: its own choice (the DM's top bar, `world_db.py dm KEY`), else the live server's."""
    return world_db.dm_world()


SERVED_DEV = ('{name} ({key}) is a dev world: Atlas edits it directly, so there is nothing to import into it. Atlas\'s '
              'edits reach its running server with a new build (Rebuild, on this tab), which the server restarts onto once '
              'nobody is connected. Import from dev is for a live world (choose one in the DM\'s world picker).')
NO_RELEASES = ('{name} ({key}) is a dev world: it has builds, not releases, and nothing to roll back (Atlas\'s edits '
               'and undo are its history).')


class Builds:
    """A dev world's builds, for the DM when it serves one as the live server: the newest, whether it is current, and
    a new one made and announced (tools/world_build.py)."""

    def __init__(self, connect=None):
        self._connect = connect or (lambda key: world_db.connect(key, 'editor'))

    def connect(self, key):
        try:
            return self._connect(key)
        except world_db.DatabaseError as error:
            raise S.Unavailable(f'Cannot reach the {key} world\'s database: {error}') from error

    def status(self, key) -> dict:
        import world_build
        with self.connect(key) as conn:
            return world_build.status(conn)

    def rebuild(self, key, editor) -> dict:
        """A new build when the world changed since the newest (announced: its server restarts onto it once nobody is
        connected); else the newest announced again. {build, made, cells, ...status}."""
        import world_build
        with self.connect(key) as conn:
            kept = world_build.current(conn)
            if kept:
                world_build.announce(conn, kept[0], conn.execute('SELECT id FROM world.worlds').fetchone()[0])
                made, cells = False, kept[1]
            else:
                _, cells = world_build.build(conn, f'DM: {editor}'[:60])
                made = True
            return {**world_build.status(conn), 'made': made, 'cells': cells}


GAME_PORTS = range(7788, 7800)   # The ports a server the studio starts may take (7788 first, as tools/live.sh's).


def free_game_port() -> int:
    for port in GAME_PORTS:
        with socket.socket() as s:
            try:
                s.bind(('0.0.0.0', port))
                return port
            except OSError:
                continue
    raise D.DMError(f'Every game port ({GAME_PORTS.start}-{GAME_PORTS.stop - 1}) is taken on this computer.', 409)


class Servers:
    """Starting a world's game server from the studio (the DM's Server tab, Atlas's Play): `tools/live.sh server KEY`
    in the background, on the local network (tools/server.sh's default), its log in Saved/Logs/server-KEY.log. It runs
    as if started by hand: it restarts onto new builds and releases, closes for maintenance and opens again."""

    def __init__(self, spawn=None):
        self._spawn = spawn or self._launch
        self._started: dict[str, object] = {}       # {key: Popen} of launchers this studio started.
        self._lock = threading.Lock()

    @staticmethod
    def _launch(key, port):
        log = D.ROOT / 'Saved' / 'Logs' / f'server-{key}.log'
        log.parent.mkdir(parents=True, exist_ok=True)
        with open(log, 'ab') as out:
            return subprocess.Popen(['bash', str(D.ROOT / 'tools' / 'live.sh'), 'server', key, '--port', str(port)],
                                    cwd=D.ROOT, stdout=out, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                                    start_new_session=True)

    def starting(self, key) -> bool:
        """A launcher this studio started for the world is still up (building, or waiting for maintenance to end)."""
        process = self._started.get(key)
        return process is not None and getattr(process, 'poll', lambda: 0)() is None

    def start(self, key, holder) -> dict:
        with self._lock:
            if holder:
                named = holder.get('name') if isinstance(holder, dict) else ''
                raise D.DMError(f'A game server already runs {key} ({named or "another server"}).', 409)
            if self.starting(key):
                return {'world': key, 'started': False, 'starting': True}
            port = free_game_port()
            self._started[key] = self._spawn(key, port)
        return {'world': key, 'started': True, 'starting': True, 'port': port,
                'log': f'Saved/Logs/server-{key}.log'}


def play_address(holder, here=None) -> dict:
    """Where a browser reaches the running server's game: {port, host (None: the host the studio was reached by),
    local (it serves its own computer only)}; None without one."""
    if not holder or not holder.get('port'):
        return None
    here = here or socket.gethostname()
    other = holder.get('host') and holder['host'] not in (here, f'{here}.local')
    return {'port': holder['port'], 'host': holder['host'] if other else None, 'local': holder.get('local', False)}


class Studio:
    """The tool's state: the DM (accounts, the live worlds), Atlas's world and its Handler, the suite to open on."""

    def __init__(self, dm=None, atlas=atlas_suite, atlas_key=None, suite='atlas', choose=None, sync=None, economy=None,
                 builds=None, servers=None, choose_dm=None):
        if suite not in SUITES:
            raise ValueError(f'A suite is one of: {", ".join(SUITES)}.')
        self.dm = dm or D.DungeonMaster()
        if sync is None:
            import world_sync
            sync = world_sync.Sync()
        self.sync = sync                       # Pull, import, maintenance, the live switch (tools/world_sync.py).
        self.economy = economy or Economy()    # The Economy page's and tab's settings (doc 79, 12).
        self.builds = builds or Builds()       # A dev world's builds, when one runs as a server.
        self.servers = servers or Servers()    # Starting a world's game server (the DM's Server tab, Atlas's Play).
        self.suite = suite
        self._make_atlas = atlas
        self._choose = choose or (lambda key: world_db.set_local(atlas=key))   # Saved/Config/worlds.local.json
        self._choose_dm = choose_dm or (lambda key: world_db.set_local(dm=key))
        self._lock = threading.Lock()
        self.dm_suite = D.make_handler(self.dm, offers=dm_offers, allowed=dm_worlds, default=dm_world)
        self.layers = D.make_handler(self.dm, offers=layer_offers, world=lambda: self.atlas_key)
        self.atlas_key = world_db.canonical(atlas_key) if atlas_key else world_db.atlas_world()
        self.atlas = atlas(self.atlas_key)

    def use(self, key: str) -> str:
        """Switches Atlas's world (a dev world): this computer's choice, and the Atlas suite's database."""
        key = world_db.canonical(key)                     # (ridgemere2 is DEV.)
        if key not in world_db.keys('dev'):
            raise D.DMError(f'Atlas works on dev worlds: {", ".join(world_db.keys("dev")) or "none registered"}.', 400)
        with self._lock:
            handler = self._make_atlas(key)
            self._choose(key)
            self.atlas, self.atlas_key = handler, key
        return key

    def use_dm(self, key: str) -> str:
        """Chooses the DM suite's world (any registered world), apart from Atlas's: this computer's choice."""
        key = world_db.canonical(key)
        if key not in world_db.keys():
            raise D.DMError(f'Choose a world: {", ".join(world_db.keys())}.', 400)
        self._choose_dm(key)
        return key

    def session(self, who: dict) -> dict:
        """Who is signed in, the suite to open on, and each suite's worlds: Atlas's (a dev world) and the DM's (any),
        chosen apart; and the live server's (what `tools/live.sh server` runs by default)."""
        try:
            live = world_db.live_world()
        except world_db.DatabaseError:
            live = None
        try:
            dm = world_db.dm_world()
        except world_db.DatabaseError:
            dm = live
        return {'me': who, 'suite': self.suite,
                'atlas': {'available': D.RANK[who['role']] >= D.RANK['dm'], 'world': shown(self.atlas_key),
                          'worlds': [shown(k) for k in world_db.keys('dev')]},
                'dm': {'world': dm, 'live': live, 'worlds': [shown(k) for k in world_db.keys()], 'switch': LIVE_SWITCH}}

    def copies_of(self, live: str) -> list[str]:
        """The dev worlds whose upstream is this live world: what can be imported into it. Atlas's world first."""
        found = [k for k in world_db.keys('dev') if world_db.upstream(k) == live]
        return sorted(found, key=lambda k: k != self.atlas_key)


def make_handler(studio: Studio):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            pass     # Never log bodies, passwords or tokens.

        def as_(self, cls):
            """This request as one suite's Handler sees it: the same connection, headers and state."""
            view = cls.__new__(cls)
            view.__dict__ = self.__dict__
            return view

        def reply(self, status, data):
            return self.as_(studio.dm_suite).reply(status, data)

        def trusted(self):
            return self.as_(studio.dm_suite).trusted()

        def signed_in(self):
            return studio.dm.session(self.as_(studio.dm_suite).token())

        def builder(self):
            who = self.signed_in()
            if D.RANK[who['role']] < D.RANK['dm']:
                raise D.DMError('Atlas is for dm and admin accounts; a viewer has the DM suite.', 403)
            return who

        def do_GET(self):
            self.route('GET')

        def do_POST(self):
            self.route('POST')

        def route(self, method):
            if not self.trusted():
                return self.reply(403, {'error': 'Local studio origin required.'})
            parts = urlsplit(self.path)
            path = parts.path
            if not path.startswith('/api/'):
                # The built app and Atlas's portraits.
                return self.as_(studio.atlas).static(path) if method == 'GET' else self.reply(405, {'error': 'Method not allowed.'})
            try:
                for prefix, suite in (('/api/atlas/layers/', 'layers'), ('/api/atlas/', 'atlas'), ('/api/dm/', 'dm'),
                                      ('/api/studio/', 'studio')):
                    if path.startswith(prefix):
                        break
                else:
                    return self.reply(404, {'error': 'Not found: the studio\'s routes are /api/atlas/... and /api/dm/....'})
                inner = '/api/' + path[len(prefix):]
                if suite == 'studio':
                    return self.studio(method, inner)
                self.path = inner + ('?' + parts.query if parts.query else '')
                if suite == 'dm':
                    if (method, inner) in SERVER_ROUTES:
                        return self.server_routes(method, inner, parse_qs(parts.query))
                    if (method, inner) in DM_ECONOMY_ROUTES:
                        return self.dm_economy(method, inner, parse_qs(parts.query))
                    return self.as_(studio.dm_suite).dispatch(method)
                if suite == 'atlas' and (method, inner) in SYNC_ROUTES:
                    return self.update_from_live(method)
                if suite == 'atlas' and (method, inner) in PLAY_ROUTES:
                    return self.play(method)
                if suite == 'atlas' and (method, inner) in ECONOMY_ROUTES:
                    return self.economy_routes(method, inner)
                self.builder()
                if suite == 'layers':
                    return self.as_(studio.layers).dispatch(method)
                if inner in NOT_IN_ATLAS:
                    return self.reply(404, {'error': PUSH_MOVED})
                atlas = self.as_(studio.atlas)
                return atlas.do_GET() if method == 'GET' else atlas.do_POST()
            except D.DMError as error:
                return self.reply(error.status, {'error': str(error)})
            except map_editor.HostError as error:            # tools/world_sync.py's refusals (and world_store's).
                answer = {'error': str(error), 'errors': list(getattr(error, 'errors', []) or [])}
                if getattr(error, 'conflicts', None) is not None:
                    answer['conflicts'] = error.conflicts
                if getattr(error, 'maps', None) is not None:           # (Two maps: doc 66, "The same map".)
                    answer['maps'] = error.maps
                return self.reply(getattr(error, 'status', 422), answer)
            except world_db.DatabaseError as error:          # An unknown world, or the local choice unusable.
                return self.reply(400, {'error': str(error)})
            except (ValueError, TypeError, KeyError):
                return self.reply(400, {'error': 'Malformed request.'})
            except Exception as error:  # noqa: BLE001 - e.g. a world's database behind on its migrations: say so, don't hang up.
                first = (str(error).strip().splitlines() or [''])[0]
                return self.reply(500, {'error': f'The studio could not answer ({type(error).__name__}: {first[:300]}).'})

        def writer(self, who):
            if D.RANK[who['role']] < D.RANK['dm']:
                raise D.DMError(f'Your role ({who["role"]}) cannot do that: it needs a dm or admin account.', 403)
            return who

        def audited(self, who, world, action, target, detail, result):
            """Records it in that world's DM audit; the change is made by then, so a failure here is a warning."""
            try:
                studio.dm.log(who, world, action, target, detail)
            except Exception as error:  # noqa: BLE001
                result = {**result, 'warnings': [*result.get('warnings', []), f'Not audited: {error}']}
            return result

        def live_key(self, asked=''):
            """A world the DM works on: the one asked for (any registered world), else the DM's chosen world."""
            key = world_db.canonical(asked) if asked else world_db.dm_world()
            world_db.world(key)                        # (An unknown world: 400, naming the registered ones.)
            return key

        def copy_of(self, live, dev):
            copies = studio.copies_of(live)
            if dev not in copies:
                raise D.DMError(f'{dev or "(none)"} is not a working copy of {live}: import from '
                                f'{", ".join(copies) or "a dev world whose upstream is " + live}.', 400)
            return dev

        def update_from_live(self, method):
            """Atlas's Update from live: the plan, and the pull, on Atlas's world (it must have an upstream)."""
            who = self.builder()
            key = studio.atlas_key
            live = world_db.upstream(key)
            if not live:
                raise D.DMError(f'{world_db.world(key)["name"]} ({key}) has no upstream live world: it is a side project, with '
                                'nothing to update from. Only a dev world copied from a live one (world_db.py new KEY --upstream '
                                'LIVE --from LIVE) can be updated from live.', 409)
            if method == 'GET':
                plan = studio.sync.diff(key)
                try:
                    status = studio.sync.status(live)
                    server = {'state': status['server'], 'running': status['serverRunning'], 'release': status['release']}
                except S.StoreError:
                    server = None
                return self.reply(200, {**plan, 'devWorld': shown(key), 'liveWorld': shown(live), 'liveServer': server})
            if not self.as_(studio.atlas).editing():
                raise D.DMError('Local editor session required.', 403)
            data = self.as_(studio.dm_suite).body()
            expect = data.get('expect') or None
            result = studio.sync.pull(key, data.get('choices') or {}, who['username'], None if expect is None else str(expect))
            applied = sum(n for part in (result.get('applied') or {}).values() for c in part.values() for n in c.values())
            return self.reply(200, self.audited(who, key, 'world.pull', live,
                                                f'Updated from {live} (live release {result.get("release")}): {applied} rows '
                                                f'changed, {result.get("conflicts", 0)} conflicts chosen.', result))

        def economy_routes(self, method, path):
            """Atlas's Economy page on Atlas's world (doc 79, 12): read by anyone signed in; changed by dm and admin with
            Atlas's session token."""
            who = self.signed_in()
            key = studio.atlas_key
            if method == 'GET':
                return self.reply(200, {**studio.economy.read(key), 'worldInfo': shown(key)})
            self.writer(who)
            if not self.as_(studio.atlas).editing():
                raise D.DMError('Local editor session required.', 403)
            return self.reply(200, {**self.economy_change(key, path, self.as_(studio.dm_suite).body(), who), 'worldInfo': shown(key)})

        def economy_change(self, key, path, data, who):
            """One POST economy/<x> (both suites): the change, audited as economy.<x> with its before and after, and
            economy.sync queued for the world's running server. {changed, detail, sync, warnings, ...EconomyView}."""
            kind = path.rsplit('/', 1)[1]
            if kind not in ECONOMY_KINDS or not isinstance(data, dict):
                raise D.DMError('Not an economy change.', 404)
            result = getattr(studio.economy, f'set_{kind}')(key, data, who['username'])
            view = result.pop('view')
            before, after = result.pop('before', None), result.pop('after', None)
            out = {'changed': bool(result.get('changed')), 'detail': result.get('detail', ''), 'sync': None,
                   'warnings': list(result.get('warnings') or [])}
            if out['changed']:
                wanted = data.get('set') if isinstance(data.get('set'), dict) else {}
                target = next(iter(wanted)) if len(wanted) == 1 else kind
                said = f'{out["detail"]} Before: {self.short(before, kind)}; after: {self.short(after, kind)}.'
                out = self.audited(who, key, f'economy.{kind}', target, said, out)
                out = self.synced(who, key, out, {'now': bool(result.get('now'))} if kind == 'steer' else None)
            return {**view, **out}

        @staticmethod
        def short(entity, kind):
            """What an audit keeps of the entity before and after a change: the part the change is about."""
            if entity is None:
                return '-'
            part = {'steer': entity, 'needs': {'needsPaused': entity.get('needsPaused')}, 'rules': entity.get('rules'),
                    'services': entity.get('services'), 'seasons': entity.get('seasons'),
                    'imports': (entity.get('overrides') or {}).get('neverImported'),
                    'status': (entity.get('overrides') or {}).get('status'),
                    'market': (entity.get('overrides') or {}).get('market')}.get(kind)
            text = json.dumps(part, sort_keys=True, separators=(',', ':'))
            return text if len(text) <= 700 else text[:690] + '...'

        def synced(self, who, key, result, payload=None):
            """economy.sync queued for the world's running server; a failure here is a warning."""
            try:
                return {**result, 'sync': studio.economy.queue_sync(key, who['username'], payload)}
            except Exception as error:  # noqa: BLE001
                return {**result, 'warnings': [*result.get('warnings', []),
                                               f'Saved, but not sent to a running server (it reads it when it next starts): {error}']}

        def dm_economy(self, method, path, query):
            """The economy on the DM's world (any registered world; its own by default), as Atlas's: read by anyone
            signed in; changed by dm and admin, audited, and sent to the world's running server (economy.sync)."""
            who = self.signed_in()
            if method == 'GET':
                key = self.live_key(str((query.get('world') or [''])[0]))
                return self.reply(200, {**studio.economy.read(key), 'worldInfo': shown(key)})
            self.writer(who)
            data = self.as_(studio.dm_suite).body()
            key = self.live_key(str(data.pop('world', '') or ''))
            return self.reply(200, {**self.economy_change(key, path, data, who), 'worldInfo': shown(key)})

        def server_routes(self, method, path, query):
            """The DM's server and imports: maintenance, the live switch, Import from dev, releases and rollback."""
            who = self.signed_in()

            def asked(name):
                return str((query.get(name) or [''])[0])
            def is_dev(key):
                return world_db.kind(key) == 'dev'

            def said(text, key):
                return text.format(name=world_db.world(key)['name'], key=key)
            if method == 'GET':
                if path == '/api/server/status':
                    key = self.live_key(asked('world'))
                    answer = {**studio.sync.status(key), 'live': world_db.live_world(), 'warning': SWITCH_WARNING,
                              'kind': world_db.kind(key), 'switchTo': [shown(k) for k in world_db.keys() if k != world_db.live_world()],
                              'starting': studio.servers.starting(key)}
                    answer['play'] = play_address(answer.get('serverHolder'))
                    if is_dev(key):                       # A dev world run as a server: its builds, not releases.
                        try:
                            answer['build'] = studio.builds.status(key)
                        except S.StoreError as error:
                            answer['build'] = {'problem': str(error)}
                        answer['servedDev'] = said(SERVED_DEV, key)
                    return self.reply(200, answer)
                if path == '/api/releases':
                    key = self.live_key(asked('world'))
                    if is_dev(key):
                        return self.reply(200, {'world': key, 'releases': [], 'note': said(NO_RELEASES, key)})
                    return self.reply(200, {'world': key, 'releases': studio.sync.releases(key)})
                live = self.live_key(asked('world'))                           # /api/import/plan
                if is_dev(live):
                    return self.reply(200, {'live': shown(live), 'candidates': [], 'dev': None, 'plan': None,
                                            'problem': said(SERVED_DEV, live), 'errors': [], 'servedDev': True})
                copies = studio.copies_of(live)
                dev = asked('dev') or (copies[0] if copies else '')
                answer = {'live': shown(live), 'candidates': [shown(k) for k in copies], 'dev': dev or None, 'plan': None,
                          'problem': '', 'errors': []}
                if dev:
                    self.copy_of(live, dev)
                    try:
                        answer['plan'] = studio.sync.plan_import(dev)
                    except S.StoreError as error:      # E.g. no base yet: say why, with the worlds still listed.
                        answer['problem'], answer['errors'] = str(error), list(getattr(error, 'errors', []) or [])
                return self.reply(200, answer)
            self.writer(who)
            data = self.as_(studio.dm_suite).body()
            note = str(data.get('note') or '')[:400]
            if path == '/api/server/maintenance':
                key, on = self.live_key(str(data.get('world') or '')), data.get('on')
                if not isinstance(on, bool):
                    raise D.DMError('Say {"on": true} to start maintenance or {"on": false} to end it.', 400)
                result = studio.sync.maintenance(key, on, who['username'], note)
                return self.reply(200, self.audited(who, key, 'server.maintenance' if on else 'server.running', key,
                                                    ('Started maintenance' if on else 'Ended maintenance') + (f': {note}' if note else '.'),
                                                    result))
            if path == '/api/server/rebuild':
                # A dev world served as live: Atlas's edits to its server (a new build, announced: it restarts onto it
                # once nobody is connected, and tools/live.sh starts it again).
                key = self.live_key(str(data.get('world') or ''))
                if not is_dev(key):
                    raise D.DMError(f'{key} is a live world: its builds come from its imports (Import from dev).', 409)
                result = studio.builds.rebuild(key, who['username'])
                return self.reply(200, self.audited(who, key, 'server.rebuild', key,
                                                    (f'Made build {result["build"]} ({result["cells"]} cells)' if result['made']
                                                     else f'Announced build {result["build"]} again (nothing changed)')
                                                    + ': the server restarts onto it once nobody is connected.', result))
            if path == '/api/server/start':
                # Starts the world's game server (tools/live.sh server KEY, on the local network) when none holds it.
                key = self.live_key(str(data.get('world') or ''))
                status = studio.sync.status(key)
                result = studio.servers.start(key, status.get('serverHolder') or status.get('serverRunning'))
                if status['server']['state'] == 'maintenance':
                    result['note'] = 'It is in maintenance: its launcher waits, and starts the server when you end maintenance.'
                if result.get('started'):
                    result = self.audited(who, key, 'server.start', key, f'Started its game server (port {result["port"]}).', result)
                return self.reply(200, result)
            if path == '/api/server/switch':
                key = world_db.canonical(str(data.get('world') or ''))
                world_db.world(key)                    # (Any registered world: a live one, or a dev world served as live.)
                if str(data.get('confirm') or '') != key:
                    raise D.DMError(f'{SWITCH_WARNING} To switch anyway, type the world\'s key ({key}) to confirm.', 400)
                was = world_db.live_world()
                if key == was:
                    raise D.DMError(f'{key} is already the live world.', 400)
                result = studio.sync.switch_live(key, who['username'])
                result = self.audited(who, was, 'server.switch', key, f'Switched the live world from {was} to {key}.', result)
                return self.reply(200, self.audited(who, key, 'server.switch', key, f'Became the live world (was {was}).', result))
            served = self.live_key(str(data.get('world') or ''))
            if is_dev(served):                         # (A dev world: no imports, no releases.)
                raise D.DMError(said(SERVED_DEV if path == '/api/import' else NO_RELEASES, served), 409)
            password = data.get('password')
            if not isinstance(password, str) or not password:
                raise D.DMError('Enter the publish password.', 400)
            if path == '/api/import':
                live = served
                dev = self.copy_of(live, str(data.get('dev') or ''))
                expect = data.get('expect') or None
                replace = data.get('replaceMap') is True               # The replace-the-map confirmation (two maps).
                result = studio.sync.import_(dev, data.get('choices') or {}, password, who['username'], note,
                                             None if expect is None else str(expect), replace)
                return self.reply(200, self.audited(who, live, 'world.import', dev,
                                                    f'Release {result.get("release")} imported from {dev}'
                                                    + (', replacing the map' if result.get('replacedMap') else '')
                                                    + (f': {note}' if note else '.'), result))
            key = self.live_key(str(data.get('world') or ''))                 # /api/rollback
            number = data.get('number')
            if not isinstance(number, int) or isinstance(number, bool) or number < 1:
                raise D.DMError('Choose a release to roll back to.', 400)
            result = studio.sync.rollback(key, number, password, who['username'], note)
            return self.reply(200, self.audited(who, key, 'world.rollback', str(number),
                                                f'Rolled back to release {number} (now release {result.get("release")})'
                                                + (f': {note}' if note else '.'), result))

        def studio(self, method, path):
            if method == 'GET' and path == '/api/ping':
                return self.reply(200, {'app': 'ratw-studio', 'suite': studio.suite})
            if method == 'GET' and path == '/api/session':
                return self.reply(200, studio.session(self.signed_in()))
            if method == 'POST' and path == '/api/atlas-world':
                self.builder()
                data = self.as_(studio.dm_suite).body()
                return self.reply(200, {'world': shown(studio.use(str(data.get('key', ''))))})
            if method == 'POST' and path == '/api/dm-world':
                self.writer(self.signed_in())
                data = self.as_(studio.dm_suite).body()
                return self.reply(200, {'world': shown(studio.use_dm(str(data.get('key', ''))))})
            raise D.DMError('Not found.', 404)

        def play(self, method):
            """Atlas's Play: the game of the server running Atlas's world, at the address the browser reached the studio
            by (another computer on the network included); with none running, why, and (POST, dm and admin) starting
            one, as the DM's Server tab does."""
            who = self.builder()
            key = studio.atlas_key
            if method == 'POST':
                status = studio.sync.status(key)
                result = studio.servers.start(key, status.get('serverHolder') or status.get('serverRunning'))
                if result.get('started'):
                    result = self.audited(who, key, 'server.start', key, f'Started its game server from Atlas\'s Play (port {result["port"]}).', result)
                return self.reply(200, result)
            status = studio.sync.status(key)
            holder = status.get('serverHolder')
            address = play_address(holder)
            state = status['server']['state']
            why = ('' if address else
                   f'{world_db.world(key)["name"]}\'s game server is starting (building first if Atlas changed it).' if studio.servers.starting(key)
                   else f'A game server holds {key} but doesn\'t say its port (started before the studio could tell): open it at '
                        f'http://THIS-COMPUTER:7788/, or restart it.' if holder or status.get('serverRunning')
                   else f'No game server is running {world_db.world(key)["name"]} ({key})'
                        + (', and it is in maintenance (end it in the DM\'s Server tab).' if state == 'maintenance' else '.')
                        + f' Start it here, or on the studio\'s computer: bash tools/live.sh server {key}')
            build = None
            try:
                build = studio.builds.status(key)
            except S.StoreError:
                pass
            return self.reply(200, {'world': shown(key), 'play': address, 'why': why, 'state': state,
                                    'starting': studio.servers.starting(key), 'build': build,
                                    'canStart': D.RANK[who['role']] >= D.RANK['dm'] and not address and not holder})

    return Handler


def lan_names() -> list[str]:
    """This computer's names on the local network: its IPv4 addresses (not loopback), its host name and name.local."""
    try:
        found = subprocess.run(['hostname', '-I'], capture_output=True, text=True, timeout=5).stdout.split()
    except OSError:
        found = []
    host = socket.gethostname()
    return [a for a in found if '.' in a and not a.startswith('127.')] + [host, f'{host}.local']


def make_server(port=PORT, studio=None, host='127.0.0.1'):
    server = ThreadingHTTPServer((host, port), make_handler(studio or Studio()))
    server.daemon_threads = True
    return server


def running_here(port: int) -> bool:
    """Whether a studio already answers on this port."""
    try:
        conn = http.client.HTTPConnection('127.0.0.1', port, timeout=2)
        conn.request('GET', '/api/studio/ping')
        answer = conn.getresponse()
        return answer.status == 200 and json.loads(answer.read()).get('app') == 'ratw-studio'
    except (OSError, ValueError):
        return False


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('command', nargs='?', default='serve', choices=('serve',))
    parser.add_argument('--port', type=int, default=PORT)
    parser.add_argument('--suite', choices=SUITES, default='atlas', help='the suite the app opens on')
    parser.add_argument('--world', help="Atlas's world for this run (default: Atlas's, python3 tools/world_db.py use KEY)")
    parser.add_argument('--host', default='0.0.0.0',
                        help='the address to serve on: 0.0.0.0 (this computer and the local network, the default; '
                             'sign-in is still required, but the connection is not encrypted) or 127.0.0.1 (this '
                             'computer only)')
    args = parser.parse_args(argv)
    shared = args.host not in ('127.0.0.1', 'localhost')
    if shared:
        os.environ['RATW_SERVE_HOSTS'] = ','.join(lan_names() if args.host == '0.0.0.0' else [args.host])
    try:
        key = world_db.resolve(args.world or 'atlas', 'dev')
        studio = Studio(atlas_key=key, suite=args.suite)
    except world_db.DatabaseError as error:
        parser.error(str(error))
    url = f'http://127.0.0.1:{args.port}/?suite={args.suite}'
    try:
        server = make_server(args.port, studio, args.host)
    except OSError as error:
        if error.errno == errno.EADDRINUSE and running_here(args.port):
            print(f'RATW Studio is already running: {url}', flush=True)
            return 0
        print(f'Port {args.port} is taken ({error.strerror}); choose another with --port N.', file=sys.stderr)
        return 1
    try:
        served = world_db.live_world()
        live = (f'the DM on the live server\'s world, {served}'
                + (' (a dev world served as live)' if world_db.kind(served) == 'dev' else ''))
    except world_db.DatabaseError as error:
        live = f'the DM: {error}'
    print(f'RATW Studio: {url}\n'
          f'Atlas on {key} ("{world_db.world(key)["name"]}", {world_db.world(key)["database"]}); {live}. '
          f'Sign in with a DM account. '
          + (f'On the local network too: '
             + ', '.join(f'http://{h}:{args.port}/' for h in os.environ['RATW_SERVE_HOSTS'].split(','))
             if shared else 'Local-only.') + ' Ctrl+C to stop.', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
