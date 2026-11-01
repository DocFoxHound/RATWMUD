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
         /api/atlas/economy...   the Economy controls (Docs/Design/67-ridgemere2-city.md, 6): GET economy (the store
                                 room's switches and every placed merchant's business, what it would draw and whether
                                 it may), POST economy/store {internalStore?, storeMarkup?, storeResale?, resaleMarkup?,
                                 storeRule?}, POST economy/businesses {set: {npc: bool}} or {all: bool, business?,
                                 district?}; and the city's loop (Docs/Design/68-the-country-and-the-city-loop.md, 9):
                                 POST economy/loop {needsPaused?, country?, countryReserve?, gateToll?, carriage?,
                                 wageKeeper?, wageHand?, wagePublic?, wageCountry?, subsidies?}, POST economy/goods
                                 {set: {item: bool}} or {all: bool}; the seasons and the comforts (doc 68, Progress,
                                 "Seasons and the DM's rules"; migration 0057): POST economy/seasons {set: {group:
                                 {supply?, price?, shelf?, normal?} or null (back to country.json's)}}, POST
                                 economy/comforts {comfortsAbove?, comfortsShare?} (null: country.json's); the loop's
                                 rules and the city's services (doc 68, Progress, "First fixes"; migration 0058): POST
                                 economy/rules {set: {rule: N or null}}, POST economy/services {set: {service: {on?,
                                 price?, every?} or null}} (null: country.json's); reads for anyone signed in, writes
                                 for dm and admin with Atlas's token.
         /api/dm/economy         the economy's rules on the DM's world (GET ?world=KEY; POST economy/store, /loop,
                                 /goods, /seasons, /comforts, /rules, /services {world, ...}, as Atlas's): for dm and
                                 admin, audited. Every
                                 change (Atlas's too) queues economy.sync for the world's running game server, which
                                 reads the world's live.economy again and applies it at once.
  DM     runs live worlds: the server, live NPC life (kill, revive, move, visit), players, reports, artwork, the
         calendar, money, health, storytellers, chapters. Its routes: /api/dm/... (tools/dungeon_master.py's Handler)
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
# Atlas's Economy controls (Docs/Design/67-ridgemere2-city.md, 6): the store room's switches, on Atlas's world. Reads for
# any signed-in account; writes for dm and admin with Atlas's session token, through live_edit's economy entity.
ECONOMY_ROUTES = frozenset({('GET', '/api/economy'), ('POST', '/api/economy/store'), ('POST', '/api/economy/businesses'),
                            ('POST', '/api/economy/loop'), ('POST', '/api/economy/goods'), ('POST', '/api/economy/seasons'),
                            ('POST', '/api/economy/comforts'), ('POST', '/api/economy/rules'), ('POST', '/api/economy/services')})
# The DM's view of the city's loop on a live world (Docs/Design/68-the-country-and-the-city-loop.md, 9): the treasury's
# levers (the gate toll, wages, the subsidies) and the Country's, read by anyone signed in, changed by dm and admin.
# Since doc 68's seasons: every setting the loop and the store room have (the per-business switches stay Atlas's).
DM_ECONOMY_ROUTES = frozenset({('GET', '/api/economy'), ('POST', '/api/economy/loop'), ('POST', '/api/economy/store'),
                               ('POST', '/api/economy/goods'), ('POST', '/api/economy/seasons'), ('POST', '/api/economy/comforts'),
                               ('POST', '/api/economy/rules'), ('POST', '/api/economy/services')})
LOOP_KEYS = tuple(k for k in map_editor.LOOP_DEFAULTS if k != 'countryOff')
COMFORT_KEYS = ('comfortsAbove', 'comfortsShare')
NO_RULE = 'The loop needs the restocking rule on, or shops sell out and stay empty.'
ECONOMY_NOTE = 'Per world; not carried by Update from live or Import from dev (each world\'s own lever).'
TAKES_EFFECT = ('Applied to the world\'s running game server within a few seconds (economy.sync), and read by any server '
                'started on it later. A business\'s own switch, and Data/Items/country.json itself, take effect when a '
                'server next starts.')
NO_SEASONS_HERE = ('This world lacks migration 0057: its seasons and comforts are as Data/Items/country.json has them and '
                   'can\'t be changed here.')
NO_RULES_HERE = ('This world lacks migration 0058: its rules and services are as Data/Items/country.json has them and '
                 'can\'t be changed here.')


class Economy:
    """The store room's settings of a dev world (doc 67, 6): the world's switches and markups, and each placed
    merchant's business with what it would draw, as the game reckons it (tools/item_catalog.py mirrors Core)."""

    def __init__(self, connect=None, connect_live=None, connect_dm=None):
        self._connect = connect or (lambda key: world_db.connect(key, 'editor'))
        # A live world is the DM's (doc 68, 9: its loop's levers): its own role, not Atlas's.
        self._connect_live = connect_live or (connect if connect else (lambda key: world_db.connect(key, 'dm')))
        # The DM's queue (dm.actions) on any world: economy.sync for its running server.
        self._connect_dm = connect_dm or (lambda key: world_db.connect(key, 'dm'))

    def queue_sync(self, key, who) -> int:
        """economy.sync for the world's running game server (doc 68, Progress): it reads live.economy again and applies
        it at once. Expires unapplied after ten minutes when no server runs (a server started later reads the row)."""
        with self._connect_dm(key) as conn:
            action = conn.execute("INSERT INTO dm.actions (kind, target_id, requested_by) VALUES ('economy.sync', 'economy', %s) "
                                  'RETURNING id', (who,)).fetchone()[0]
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

    def districts(self, key) -> dict:
        """{interior id: district} from the world's building manifest (artifacts/<key>/buildings.json), if it has one."""
        import pathlib
        path = pathlib.Path(__file__).resolve().parent.parent / 'artifacts' / key / 'buildings.json'
        try:
            records = json.loads(path.read_text(encoding='utf-8'))
        except (OSError, ValueError):
            return {}
        records = records.get('buildings', []) if isinstance(records, dict) else records
        out = {}
        for b in records if isinstance(records, list) else []:
            if isinstance(b, dict):
                for room in b.get('rooms') or [{'id': b.get('id')}]:
                    if isinstance(room, dict) and room.get('id'):
                        out[room['id']] = b.get('district') or ''
        return out

    def read(self, key) -> dict:
        import item_catalog as C
        with self.connect(key) as conn:
            world = conn.execute('SELECT id FROM world.worlds').fetchone()
            if not world:
                raise D.DMError(f'{key} has no world yet.', 409)
            w = world[0]
            supported = S.store_supported(conn)
            loop_ok = supported and S.loop_supported(conn)
            seasons_ok = loop_ok and S.seasons_supported(conn)
            rules_ok = seasons_ok and S.rules_supported(conn)
            economy = S.economy_value(conn, w) or S.default_economy()
            rows = S.dict_rows(conn, '''
                SELECT n.id, n.name, n.work_label, n.work_area, coalesce(i.name, c.name, n.work_area) AS interior,
                       coalesce(i.region, c.region, '') AS region
                FROM live.npcs n
                LEFT JOIN world.interiors i ON i.world_id = n.world_id AND i.id = n.work_area
                LEFT JOIN world.cells c ON c.world_id = n.world_id AND c.id = n.work_area
                WHERE n.world_id = %s AND n.role = 'merchant' AND n.home_area IS NOT NULL AND n.spawn_id IS NULL
                ORDER BY n.position, n.id''', (w,))
        districts = self.districts(key)
        off = {npc for npc, s in economy['businesses'].items() if s.get('internalStore') is False}
        businesses = []
        for r in rows:
            business = C.business_for(r['work_label'])
            goods = C.store_goods(business, r['id'], r['work_label'])
            making = [{'item': k, 'price': C.store_price(k), 'cost': C.store_cost(k, 1, economy['storeMarkup'])} for k in goods['making']]
            resale = [{'item': k, 'price': C.store_price(k), 'cost': C.store_cost(k, 1, economy['resaleMarkup'])} for k in goods['resale']]
            drawn = (making if economy['internalStore'] else []) + (resale if economy['storeResale'] else [])
            own = r['id'] not in off
            if not making and not resale:
                effective, why = 'draws nothing', 'Its business has no crafts and nothing to resell.'
            elif not drawn:
                effective, why = 'off (whole world)', 'The world\'s switch for its kind of goods is off.'
            elif not own:
                effective, why = 'off (this business)', 'Switched off for this business.'
            else:
                effective, why = 'on', ''
            businesses.append({'npc': r['id'], 'name': r['name'], 'label': r['work_label'], 'business': business,
                               'district': districts.get(r['work_area']) or r['region'] or '', 'interior': r['interior'] or '',
                               'making': making, 'resale': resale, 'internalStore': own, 'effective': effective, 'why': why})
        # The city's loop (doc 68): its settings, the Country's basket (Data/Items/country.json) with its makers here, and
        # every good the listed businesses draw, with whether the Country supplies it.
        import pathlib
        country = json.loads((pathlib.Path(__file__).resolve().parent.parent / 'Data/Items/country.json').read_text(encoding='utf-8'))
        present = {b['business'] for b in businesses if b['business']}
        market = (country.get('markets') or [{}])[0]
        basket = []
        for item, share in (market.get('basket') or {}).items():
            makers = sorted({m for k in C.CRAFTS if item in k['out'] for m in k['makers'] if m in present})
            basket.append({'item': item, 'name': C.ITEMS.get(item, {}).get('name', item), 'share': share,
                           'price': C.store_price(item), 'makers': makers})
        total = sum(b['share'] for b in basket) or 1
        for b in basket:
            b['percent'] = round(100 * b['share'] / total, 1)
        drawn = {}
        for b in businesses:
            for g in b['making'] + b['resale']:
                drawn[g['item']] = drawn.get(g['item'], 0) + 1
        off = set(economy.get('countryOff') or [])
        goods = [{'item': g, 'name': C.ITEMS.get(g, {}).get('name', g), 'businesses': n, 'supplied': g not in off}
                 for g, n in sorted(drawn.items())]
        return {'world': w, 'supported': supported, **{k: economy[k] for k in S.STORE_DEFAULTS}, 'businesses': businesses,
                'note': ECONOMY_NOTE, 'takesEffect': TAKES_EFFECT,
                'problem': '' if supported else 'This world lacks migration 0055: the store is on at its defaults and can\'t be changed here.',
                'loop': {k: economy[k] for k in LOOP_KEYS}, 'countryOff': sorted(off), 'loopSupported': loop_ok,
                'loopProblem': '' if loop_ok else 'This world lacks migration 0056: the city\'s loop runs at its defaults and can\'t be changed here.',
                'market': {'id': market.get('id', 'country'), 'name': market.get('name', 'the Country'), 'people': market.get('people', '')},
                'basket': basket, 'goods': goods, 'ruleWarning': '' if economy['storeRule'] else NO_RULE,
                **self.seasons_view(country, economy, seasons_ok), **self.rules_view(economy, rules_ok)}

    @staticmethod
    def rules_view(economy, supported) -> dict:
        """The loop's rules and the city's services (doc 68, Progress, "First fixes"): this world's changes ({} for none),
        country.json's rules beside them, and each country.json service with its name, catalogue price and business."""
        return {'rulesSupported': supported, 'rulesProblem': '' if supported else NO_RULES_HERE,
                'rules': dict(economy.get('rules') or {}), 'services': copy.deepcopy(dict(economy.get('services') or {})),
                'ruleDefaults': map_editor.rule_defaults(), 'serviceList': map_editor.service_list()}

    @staticmethod
    def seasons_view(country, economy, supported) -> dict:
        """The seasons and the comforts (doc 68, Progress): country.json's groups with this world's changes, each
        with country.json's own numbers beside it, and the comforts (null: country.json's)."""
        own = economy.get('seasons') or {}
        groups = []
        for g in (country.get('seasons') or {}).get('groups') or []:
            if not isinstance(g, dict) or not g.get('id'):
                continue
            base = {k: list(g.get(k) or [100] * 4) for k in ('supply', 'price', 'shelf')}
            base['normal'] = int(g.get('normal') or 0)
            mine = own.get(g['id'])
            groups.append({'id': g['id'], 'name': g.get('name', g['id']), 'about': g.get('about', ''), 'goods': list(g.get('goods') or []),
                           **(mine or base), 'changed': mine is not None, 'file': base})
        comforts = country.get('comforts') or {}
        return {'seasonsSupported': supported, 'seasonsProblem': '' if supported else NO_SEASONS_HERE,
                'seasonNames': list(map_editor.SEASON_NAMES), 'seasons': groups,
                'comforts': {'comfortsAbove': economy.get('comfortsAbove'), 'comfortsShare': economy.get('comfortsShare'),
                             'fileAbove': comforts.get('above', 60), 'fileShare': round(100 * float(comforts.get('share', .1)))}}

    def _edit(self, key, editor, label, change, loop=False, seasons=False, rules=False):
        """One keyed edit of the economy entity (live_edit: logged, so open Atlas sessions follow it). `loop`: the city
        loop's settings, which need migration 0056; `seasons`: the seasons' and the comforts', which need 0057; `rules`:
        the rules' and the services', which need 0058."""
        import live_edit
        with self.connect(key) as conn:
            if not S.store_supported(conn):
                raise D.DMError('This world lacks migration 0055: the store is on at its defaults and can\'t be changed here '
                                f'(python3 tools/world_db.py migrate --db {key}).', 409)
            if (loop or seasons or rules) and not S.loop_supported(conn):
                raise D.DMError('This world lacks migration 0056: the city\'s loop runs at its defaults and can\'t be changed '
                                f'here (python3 tools/world_db.py migrate --db {key}).', 409)
            if (seasons or rules) and not S.seasons_supported(conn):
                raise D.DMError(f'{NO_SEASONS_HERE} (python3 tools/world_db.py migrate --db {key}).', 409)
            if rules and not S.rules_supported(conn):
                raise D.DMError(f'{NO_RULES_HERE} (python3 tools/world_db.py migrate --db {key}).', 409)
            world = conn.execute('SELECT id FROM world.worlds').fetchone()
            if not world:
                raise D.DMError(f'{key} has no world yet.', 409)
            before = S.economy_value(conn, world[0])
            after = change(copy.deepcopy(before) if before is not None else S.default_economy())
            if before is not None and live_edit.canonical(before) == live_edit.canonical(after):
                return {'changed': False}
            if self.live(key):
                # A live world has no Workshop sessions to keep current: the row itself, as the DM's role writes it.
                problems = map_editor.check_economy(after)
                if problems:
                    raise D.DMError(' '.join(problems), 400)
                tables = S.tables_for(conn)
                keys, columns = tables['live.economy']
                row = S.economy_row_for(world[0], after, tables)
                with conn.transaction():
                    conn.execute(f'INSERT INTO live.economy ({", ".join(columns)}) VALUES ({", ".join(["%s"] * len(columns))}) '
                                 'ON CONFLICT (world_id) DO UPDATE SET ' + ', '.join(f'{c} = excluded.{c}' for c in columns if c not in keys),
                                 [S.Jsonb(v) if c in S.JSON_COLUMNS else v for c, v in zip(columns, row)])
                return {'changed': True}
            seq = live_edit.apply_edit(conn, 'studio-economy', editor, label, [{'key': 'economy', 'before': before, 'after': after}])
        return {'changed': True, 'seq': seq}

    def set_store(self, key, data, editor) -> dict:
        """The world's switches (all at once) and markups: {internalStore?, storeMarkup?, storeResale?, resaleMarkup?, storeRule?}."""
        if not isinstance(data, dict) or not data or set(data) - set(S.STORE_DEFAULTS):
            raise D.DMError('Send any of: ' + ', '.join(S.STORE_DEFAULTS) + '.', 400)
        problems = map_editor.check_economy({**S.default_economy(), **data})
        if problems:
            raise D.DMError(' '.join(problems), 400)
        said = ', '.join(f'{k} {"on" if v is True else "off" if v is False else v}' for k, v in data.items())
        result = self._edit(key, editor, f'The store room: {said}', lambda e: {**e, **data})
        return {**result, 'detail': f'The store room: {said}.'}

    def set_loop(self, key, data, editor) -> dict:
        """The city's loop (doc 68, 9): {needsPaused?, country?, countryReserve?, gateToll?, carriage?, wageKeeper?,
        wageHand?, wagePublic?, wageCountry?, subsidies?: {service: percent}}."""
        if not isinstance(data, dict) or not data or set(data) - set(LOOP_KEYS):
            raise D.DMError('Send any of: ' + ', '.join(LOOP_KEYS) + '.', 400)
        if 'subsidies' in data and isinstance(data['subsidies'], dict):
            data = {**data, 'subsidies': {**map_editor.LOOP_DEFAULTS['subsidies'], **data['subsidies']}}
        problems = map_editor.check_economy({**S.default_economy(), **data})
        if problems:
            raise D.DMError(' '.join(problems), 400)
        def told(k, v):
            if isinstance(v, bool):
                return f'{k} {"on" if v else "off"}'
            if isinstance(v, dict):
                return f'{k} ' + ', '.join(f'{a} {b}%' for a, b in sorted(v.items()))
            return f'{k} {v}'
        said = ', '.join(told(k, v) for k, v in data.items())
        result = self._edit(key, editor, f'The city\'s loop: {said}', lambda e: {**e, **data}, loop=True)
        return {**result, 'detail': f'The city\'s loop: {said}.'}

    def set_goods(self, key, data, editor) -> dict:
        """Which goods the Country supplies (doc 68, 2): {set: {item: bool}} or {all: bool}; a good switched off is its
        real producers' (the store room refuses it)."""
        if not isinstance(data, dict):
            raise D.DMError('Send {"set": {item: true|false}} or {"all": true|false}.', 400)
        listed = {g['item'] for g in self.read(key)['goods']}
        if 'set' in data:
            wanted = data['set']
            if not isinstance(wanted, dict) or not wanted or not all(isinstance(v, bool) for v in wanted.values()):
                raise D.DMError('set is {item id: true or false}.', 400)
            unknown = sorted(set(wanted) - listed)
            if unknown:
                raise D.DMError(f'No business here draws: {", ".join(unknown[:8])}.', 400)
        elif isinstance(data.get('all'), bool):
            wanted = {g: data['all'] for g in listed}
        else:
            raise D.DMError('Send {"set": {item: true|false}} or {"all": true|false}.', 400)

        def change(e):
            off = set(e.get('countryOff') or [])
            for item, on in wanted.items():
                (off.discard if on else off.add)(item)
            return {**e, 'countryOff': sorted(off)}
        on = sum(1 for v in wanted.values() if v)
        detail = f'The Country supplies {on} of {len(wanted)} good{"s" if len(wanted) != 1 else ""} set.'
        result = self._edit(key, editor, detail, change, loop=True) if wanted else {'changed': False}
        return {**result, 'count': len(wanted), 'detail': detail}

    def set_seasons(self, key, data, editor) -> dict:
        """Season groups as this world has them (doc 68, Progress): {set: {group: {supply?, price?, shelf?, normal?}}}, any
        of a group's four numbers at a time (the rest as it is now), or a group null: back to country.json's."""
        wanted = data.get('set') if isinstance(data, dict) else None
        if not isinstance(wanted, dict) or not wanted:
            raise D.DMError('Send {"set": {group: {supply?, price?, shelf?: [Spring, Summer, Autumn, Winter] percents, '
                            'normal?: pieces a day} or null}}.', 400)
        groups = {g['id']: g for g in map_editor.season_groups()}
        unknown = sorted(set(wanted) - set(groups))
        if unknown:
            raise D.DMError(f'No such season group in Data/Items/country.json: {", ".join(unknown[:8])} (there are '
                            f'{", ".join(groups) or "none"}).', 400)
        for group, change in wanted.items():
            if change is not None and (not isinstance(change, dict) or not change or set(change) - {'supply', 'price', 'shelf', 'normal'}):
                raise D.DMError(f'{group}: send any of supply, price, shelf (four percents each) and normal, or null.', 400)

        def base(group):
            g = groups[group]
            return {**{k: list(g.get(k) or [100] * 4) for k in ('supply', 'price', 'shelf')}, 'normal': int(g.get('normal') or 0)}

        def change(e):
            seasons = copy.deepcopy(e.get('seasons') or {})
            for group, values in wanted.items():
                if values is None:
                    seasons.pop(group, None)
                    continue
                merged = {**(seasons.get(group) or base(group)), **values}
                seasons[group] = merged if merged != base(group) else None
                if seasons[group] is None:
                    del seasons[group]            # (Back at country.json's: no row kept for it.)
            problems = map_editor.check_economy({**S.default_economy(), 'seasons': seasons})
            if problems:
                raise D.DMError(' '.join(problems), 400)
            return {**e, 'seasons': seasons}

        def told(group, values):
            if values is None:
                return f'{group} as country.json has it'
            return f'{group} ' + ', '.join(f'{k} {"/".join(map(str, v)) if isinstance(v, list) else v}' for k, v in values.items())
        said = '; '.join(told(g, v) for g, v in wanted.items())
        result = self._edit(key, editor, f'The seasons: {said}', change, seasons=True)
        return {**result, 'detail': f'The seasons: {said}.'}

    def set_comforts(self, key, data, editor) -> dict:
        """The residents' comforts (doc 68, 3): {comfortsAbove?: pennies a head, comfortsShare?: percent}, null for
        country.json's."""
        if not isinstance(data, dict) or not data or set(data) - set(COMFORT_KEYS):
            raise D.DMError('Send any of: ' + ', '.join(COMFORT_KEYS) + ' (null: country.json\'s).', 400)
        problems = map_editor.check_economy({**S.default_economy(), **data})
        if problems:
            raise D.DMError(' '.join(problems), 400)
        said = ', '.join(f'{k} {"as country.json has it" if v is None else v}' for k, v in data.items())
        result = self._edit(key, editor, f'The comforts: {said}', lambda e: {**e, **data}, seasons=True)
        return {**result, 'detail': f'The comforts: {said}.'}

    def set_rules(self, key, data, editor) -> dict:
        """The loop's rules (doc 68, Progress, "First fixes"): {set: {rule: whole number, or null for country.json's}}."""
        wanted = data.get('set') if isinstance(data, dict) else None
        names = map_editor.RULE_LIMITS
        if not isinstance(wanted, dict) or not wanted:
            raise D.DMError('Send {"set": {rule: whole number or null}}; the rules: ' + ', '.join(names) + '.', 400)
        unknown = sorted(set(wanted) - set(names))
        if unknown:
            raise D.DMError(f'No such rule: {", ".join(unknown[:8])} (the rules: {", ".join(names)}).', 400)
        problems = map_editor.check_rules({k: v for k, v in wanted.items() if v is not None}, {})
        if problems:
            raise D.DMError(' '.join(problems), 400)

        def change(e):
            rules = dict(e.get('rules') or {})
            for name, value in wanted.items():
                if value is None:
                    rules.pop(name, None)
                else:
                    rules[name] = value
            return {**e, 'rules': rules}
        said = ', '.join(f'{k} {"as country.json has it" if v is None else v}' for k, v in wanted.items())
        result = self._edit(key, editor, f'The rules: {said}', change, rules=True)
        return {**result, 'detail': f'The rules: {said}.'}

    def set_services(self, key, data, editor) -> dict:
        """The city's services (doc 68, Progress, "First fixes"): {set: {service: {on?, price?, every?} or null}}, any of a
        service's three at a time (the rest as it is now), or null: back to country.json's (and items.json's price)."""
        wanted = data.get('set') if isinstance(data, dict) else None
        if not isinstance(wanted, dict) or not wanted:
            raise D.DMError('Send {"set": {service: {on?: true|false, price?: pennies, every?: days} or null}}.', 400)
        listed = {s['id']: s for s in map_editor.service_list()}
        unknown = sorted(set(wanted) - set(listed))
        if unknown:
            raise D.DMError(f'No such service in Data/Items/country.json: {", ".join(unknown[:8])} (there are '
                            f'{", ".join(listed) or "none"}).', 400)
        for sid, values in wanted.items():
            if values is not None and (not isinstance(values, dict) or not values or set(values) - {'on', 'price', 'every'}):
                raise D.DMError(f'{sid}: send any of on, price and every, or null.', 400)

        def base(sid):
            s = listed[sid]
            return {'on': True, 'price': s['price'], 'every': s['every']}

        def change(e):
            services = copy.deepcopy(e.get('services') or {})
            for sid, values in wanted.items():
                if values is None:
                    services.pop(sid, None)
                    continue
                merged = {**(services.get(sid) or base(sid)), **values}
                if merged == base(sid):
                    services.pop(sid, None)       # (Back at country.json's: no entry kept for it.)
                else:
                    services[sid] = merged
            problems = map_editor.check_rules({}, services)
            if problems:
                raise D.DMError(' '.join(problems), 400)
            return {**e, 'services': services}

        def told(sid, values):
            if values is None:
                return f'{sid} as country.json has it'
            return f'{sid} ' + ', '.join(f'{k} {("on" if v else "off") if isinstance(v, bool) else v}' for k, v in values.items())
        said = '; '.join(told(s, v) for s, v in wanted.items())
        result = self._edit(key, editor, f'The services: {said}', change, rules=True)
        return {**result, 'detail': f'The services: {said}.'}

    def set_businesses(self, key, data, editor) -> dict:
        """Per business ({set: {npc: bool}}), or every listed business at once ({all: bool, business?, district?})."""
        if not isinstance(data, dict):
            raise D.DMError('Send {"set": {npc: true|false}} or {"all": true|false}.', 400)
        listed = self.read(key)['businesses']
        if 'set' in data:
            wanted = data['set']
            if not isinstance(wanted, dict) or not wanted or not all(isinstance(v, bool) for v in wanted.values()):
                raise D.DMError('set is {npc id: true or false}.', 400)
            known = {b['npc'] for b in listed}
            unknown = sorted(set(wanted) - known)
            if unknown:
                raise D.DMError(f'Not a placed merchant\'s business here: {", ".join(unknown[:8])}.', 400)
        elif isinstance(data.get('all'), bool):
            chosen = [b for b in listed if (not data.get('business') or b['business'] == data['business'])
                      and (not data.get('district') or b['district'] == data['district'])]
            wanted = {b['npc']: data['all'] for b in chosen}
        else:
            raise D.DMError('Send {"set": {npc: true|false}} or {"all": true|false, "business"?, "district"?}.', 400)

        def change(e):
            shops = dict(e.get('businesses') or {})
            for npc, on in wanted.items():
                if on:
                    shops.pop(npc, None)                 # (Back at its default: no row kept.)
                else:
                    shops[npc] = {'internalStore': False}
            return {**e, 'businesses': shops}
        on = sum(1 for v in wanted.values() if v)
        detail = (f'The store room for {len(wanted)} business{"es" if len(wanted) != 1 else ""}: {on} on, {len(wanted) - on} off'
                  + (f' ({data.get("business") or "every business"}, {data.get("district") or "every district"})' if 'all' in data else '')
                  + '.')
        result = self._edit(key, editor, detail, change) if wanted else {'changed': False}
        return {**result, 'count': len(wanted), 'detail': detail}
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
        self.economy = economy or Economy()    # Atlas's Economy controls: the store room (doc 67, 6).
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
            """Atlas's Economy controls (doc 67, 6) on Atlas's world: read by anyone signed in; changed by dm and admin
            with Atlas's session token, as one keyed edit, audited in the world's DM audit."""
            who = self.signed_in()
            key = studio.atlas_key
            if method == 'GET':
                return self.reply(200, {**studio.economy.read(key), 'worldInfo': shown(key)})
            self.writer(who)
            if not self.as_(studio.atlas).editing():
                raise D.DMError('Local editor session required.', 403)
            data = self.as_(studio.dm_suite).body()
            if path == '/api/economy/store':
                result = studio.economy.set_store(key, data, who['username'])
                action, target = 'economy.store', 'store room'
            elif path == '/api/economy/loop':
                result = studio.economy.set_loop(key, data, who['username'])
                action, target = 'economy.loop', 'city loop'
            elif path == '/api/economy/goods':
                result = studio.economy.set_goods(key, data, who['username'])
                action, target = 'economy.goods', 'the Country\'s goods'
            elif path in ('/api/economy/seasons', '/api/economy/comforts', '/api/economy/rules', '/api/economy/services'):
                result, action, target = self.economy_change(key, path, data, who)
            else:
                result = studio.economy.set_businesses(key, data, who['username'])
                action, target = 'economy.businesses', next(iter(data.get('set') or {}), '') if isinstance(data.get('set'), dict) and len(data['set']) == 1 else 'businesses'
            if result.get('changed'):
                result = self.audited(who, key, action, target, result['detail'], result)
                result = self.synced(who, key, result)
            return self.reply(200, {**result, **studio.economy.read(key)})

        def economy_change(self, key, path, data, who):
            """An economy change both suites make: (the result, its audit action, its target)."""
            if path == '/api/economy/loop':
                return studio.economy.set_loop(key, data, who['username']), 'economy.loop', 'city loop'
            if path == '/api/economy/store':
                return studio.economy.set_store(key, data, who['username']), 'economy.store', 'store room'
            if path == '/api/economy/goods':
                return studio.economy.set_goods(key, data, who['username']), 'economy.goods', 'the Country\'s goods'
            if path == '/api/economy/seasons':
                wanted = data.get('set') if isinstance(data.get('set'), dict) else {}
                target = next(iter(wanted)) if len(wanted) == 1 else 'seasons'
                return studio.economy.set_seasons(key, data, who['username']), 'economy.seasons', target
            if path in ('/api/economy/rules', '/api/economy/services'):
                wanted = data.get('set') if isinstance(data.get('set'), dict) else {}
                kind = path.rsplit('/', 1)[1]
                target = next(iter(wanted)) if len(wanted) == 1 else kind
                setter = studio.economy.set_rules if kind == 'rules' else studio.economy.set_services
                return setter(key, data, who['username']), f'economy.{kind}', target
            return studio.economy.set_comforts(key, data, who['username']), 'economy.comforts', 'comforts'

        def synced(self, who, key, result):
            """economy.sync queued for the world's running server (doc 68, Progress); a failure here is a warning."""
            try:
                return {**result, 'sync': studio.economy.queue_sync(key, who['username'])}
            except Exception as error:  # noqa: BLE001
                return {**result, 'warnings': [*result.get('warnings', []),
                                               f'Saved, but not sent to a running server (it reads it when it next starts): {error}']}

        def dm_economy(self, method, path, query):
            """The economy's rules on the DM's world (doc 68, 9, and Progress, "Seasons and the DM's rules"): read by
            anyone signed in; changed by dm and admin (the store room's switches and markups, the loop, the Country's
            goods, the seasons, the comforts, the rules, the services), audited, and sent to the world's running server
            (economy.sync)."""
            who = self.signed_in()
            if method == 'GET':
                key = self.live_key(str((query.get('world') or [''])[0]))
                return self.reply(200, {**studio.economy.read(key), 'worldInfo': shown(key)})
            self.writer(who)
            data = self.as_(studio.dm_suite).body()
            key = self.live_key(str(data.pop('world', '') or ''))
            result, action, target = self.economy_change(key, path, data, who)
            if result.get('changed'):
                result = self.audited(who, key, action, target, result['detail'], result)
                result = self.synced(who, key, result)
            return self.reply(200, {**result, **studio.economy.read(key), 'worldInfo': shown(key)})

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
