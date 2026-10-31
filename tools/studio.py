#!/usr/bin/env python3
"""RATW Studio: one tool, two suites (Docs/Design/66-worlds-atlas-and-dm.md, section 3).

  bash tools/studio.sh [--port N] [--suite atlas|dm]     build the UI when its sources changed, then serve it
  python3 tools/studio.py serve [--port N] [--suite atlas|dm] [--world KEY]
  bash tools/editor.sh / bash tools/dungeon-master.sh   the same, opening on the Atlas / DM suite

One host on one port (default 8765, local only) serves one app (Editor/index.html) with a switch between:

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
                                 {set: {item: bool}} or {all: bool}; reads for anyone signed in, writes for dm and admin
                                 with Atlas's token.
         /api/dm/economy         the city's loop on a live world (GET ?world=KEY; POST economy/loop {world, ...}): the
                                 treasury's and the Country's levers, for dm and admin.
  DM     runs live worlds: the server, live NPC life (kill, revive, move, visit), players, reports, artwork, the
         calendar, money, health, storytellers, chapters. Its routes: /api/dm/... (tools/dungeon_master.py's Handler)
         without the map and NPC authoring that moved to Atlas, and only on live worlds; and the server and imports
         (tools/world_sync.py; doc 66, section 4):
         GET  /api/dm/server/status?world=KEY     a live world's state (running or maintenance), its game server, release
         POST /api/dm/server/maintenance          {world, on, note}: close it (players get a minute) or turn it back on
         POST /api/dm/server/switch               {world, confirm: the world's key again}: the live world, in maintenance
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
                           ('GET', '/api/import/plan'), ('POST', '/api/import'), ('GET', '/api/releases'),
                           ('POST', '/api/rollback')})
SYNC_ROUTES = frozenset({('GET', '/api/sync/diff'), ('POST', '/api/sync/pull')})
# Atlas's Economy controls (Docs/Design/67-ridgemere2-city.md, 6): the store room's switches, on Atlas's world. Reads for
# any signed-in account; writes for dm and admin with Atlas's session token, through live_edit's economy entity.
ECONOMY_ROUTES = frozenset({('GET', '/api/economy'), ('POST', '/api/economy/store'), ('POST', '/api/economy/businesses'),
                            ('POST', '/api/economy/loop'), ('POST', '/api/economy/goods')})
# The DM's view of the city's loop on a live world (Docs/Design/68-the-country-and-the-city-loop.md, 9): the treasury's
# levers (the gate toll, wages, the subsidies) and the Country's, read by anyone signed in, changed by dm and admin.
DM_ECONOMY_ROUTES = frozenset({('GET', '/api/economy'), ('POST', '/api/economy/loop')})
LOOP_KEYS = tuple(k for k in map_editor.LOOP_DEFAULTS if k != 'countryOff')
NO_RULE = 'The loop needs the restocking rule on, or shops sell out and stay empty.'
ECONOMY_NOTE = 'Per world; not carried by Update from live or Import from dev (each world\'s own lever).'
TAKES_EFFECT = 'Takes effect when a server next starts on this world.'


class Economy:
    """The store room's settings of a dev world (doc 67, 6): the world's switches and markups, and each placed
    merchant's business with what it would draw, as the game reckons it (tools/item_catalog.py mirrors Core)."""

    def __init__(self, connect=None, connect_live=None):
        self._connect = connect or (lambda key: world_db.connect(key, 'editor'))
        # A live world is the DM's (doc 68, 9: its loop's levers): its own role, not Atlas's.
        self._connect_live = connect_live or (connect if connect else (lambda key: world_db.connect(key, 'dm')))

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
                'basket': basket, 'goods': goods, 'ruleWarning': '' if economy['storeRule'] else NO_RULE}

    def _edit(self, key, editor, label, change, loop=False):
        """One keyed edit of the economy entity (live_edit: logged, so open Atlas sessions follow it). `loop`: the city
        loop's settings, which need migration 0056."""
        import live_edit
        with self.connect(key) as conn:
            if not S.store_supported(conn):
                raise D.DMError('This world lacks migration 0055: the store is on at its defaults and can\'t be changed here '
                                f'(python3 tools/world_db.py migrate --db {key}).', 409)
            if loop and not S.loop_supported(conn):
                raise D.DMError('This world lacks migration 0056: the city\'s loop runs at its defaults and can\'t be changed '
                                f'here (python3 tools/world_db.py migrate --db {key}).', 409)
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


class Studio:
    """The tool's state: the DM (accounts, the live worlds), Atlas's world and its Handler, the suite to open on."""

    def __init__(self, dm=None, atlas=atlas_suite, atlas_key=None, suite='atlas', choose=None, sync=None, economy=None):
        if suite not in SUITES:
            raise ValueError(f'A suite is one of: {", ".join(SUITES)}.')
        self.dm = dm or D.DungeonMaster()
        if sync is None:
            import world_sync
            sync = world_sync.Sync()
        self.sync = sync                       # Pull, import, maintenance, the live switch (tools/world_sync.py).
        self.economy = economy or Economy()    # Atlas's Economy controls: the store room (doc 67, 6).
        self.suite = suite
        self._make_atlas = atlas
        self._choose = choose or (lambda key: world_db.set_local(atlas=key))   # Saved/Config/worlds.local.json
        self._lock = threading.Lock()
        self.dm_suite = D.make_handler(self.dm, offers=dm_offers, kinds=('live',))
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

    def session(self, who: dict) -> dict:
        """Who is signed in, the suite to open on, and each suite's worlds."""
        try:
            live = world_db.live_world()
        except world_db.DatabaseError:
            live = None
        return {'me': who, 'suite': self.suite,
                'atlas': {'available': D.RANK[who['role']] >= D.RANK['dm'], 'world': shown(self.atlas_key),
                          'worlds': [shown(k) for k in world_db.keys('dev')]},
                'dm': {'live': live, 'worlds': [shown(k) for k in world_db.keys('live')], 'switch': LIVE_SWITCH}}

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
                        return self.dm_economy(method, parse_qs(parts.query))
                    return self.as_(studio.dm_suite).dispatch(method)
                if suite == 'atlas' and (method, inner) in SYNC_ROUTES:
                    return self.update_from_live(method)
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
            """A live world: the one asked for, else the live server's."""
            key = asked or world_db.live_world()
            if world_db.kind(key) != 'live':
                raise D.DMError(f'{key} is a dev world; the DM works on live worlds ({", ".join(world_db.keys("live"))}).', 400)
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
            else:
                result = studio.economy.set_businesses(key, data, who['username'])
                action, target = 'economy.businesses', next(iter(data.get('set') or {}), '') if isinstance(data.get('set'), dict) and len(data['set']) == 1 else 'businesses'
            if result.get('changed'):
                result = self.audited(who, key, action, target, result['detail'], result)
            return self.reply(200, {**result, **studio.economy.read(key)})

        def dm_economy(self, method, query):
            """The city's loop on a live world (doc 68, 9): read by anyone signed in, its levers changed by dm and admin
            (the gate toll, the wages, the subsidies, the Country's switch and reserve), audited; it takes effect when the
            game server next starts on the world."""
            who = self.signed_in()
            if method == 'GET':
                key = self.live_key(str((query.get('world') or [''])[0]))
                return self.reply(200, {**studio.economy.read(key), 'worldInfo': shown(key)})
            self.writer(who)
            data = self.as_(studio.dm_suite).body()
            key = self.live_key(str(data.pop('world', '') or ''))
            result = studio.economy.set_loop(key, data, who['username'])
            if result.get('changed'):
                result = self.audited(who, key, 'economy.loop', 'city loop', result['detail'], result)
            return self.reply(200, {**result, **studio.economy.read(key)})

        def server_routes(self, method, path, query):
            """The DM's server and imports: maintenance, the live switch, Import from dev, releases and rollback."""
            who = self.signed_in()

            def asked(name):
                return str((query.get(name) or [''])[0])
            if method == 'GET':
                if path == '/api/server/status':
                    key = self.live_key(asked('world'))
                    return self.reply(200, {**studio.sync.status(key), 'live': world_db.live_world(), 'warning': SWITCH_WARNING})
                if path == '/api/releases':
                    key = self.live_key(asked('world'))
                    return self.reply(200, {'world': key, 'releases': studio.sync.releases(key)})
                live = self.live_key()                                         # /api/import/plan
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
            if path == '/api/server/switch':
                key = str(data.get('world') or '')
                if world_db.kind(key) != 'live':
                    raise D.DMError(f'{key} is a dev world; the live server runs live worlds.', 400)
                if str(data.get('confirm') or '') != key:
                    raise D.DMError(f'{SWITCH_WARNING} To switch anyway, type the world\'s key ({key}) to confirm.', 400)
                was = world_db.live_world()
                if key == was:
                    raise D.DMError(f'{key} is already the live world.', 400)
                result = studio.sync.switch_live(key, who['username'])
                result = self.audited(who, was, 'server.switch', key, f'Switched the live world from {was} to {key}.', result)
                return self.reply(200, self.audited(who, key, 'server.switch', key, f'Became the live world (was {was}).', result))
            password = data.get('password')
            if not isinstance(password, str) or not password:
                raise D.DMError('Enter the publish password.', 400)
            if path == '/api/import':
                live = self.live_key()
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
            raise D.DMError('Not found.', 404)

    return Handler


def make_server(port=PORT, studio=None):
    server = ThreadingHTTPServer(('127.0.0.1', port), make_handler(studio or Studio()))
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
    args = parser.parse_args(argv)
    try:
        key = world_db.resolve(args.world or 'atlas', 'dev')
        studio = Studio(atlas_key=key, suite=args.suite)
    except world_db.DatabaseError as error:
        parser.error(str(error))
    url = f'http://127.0.0.1:{args.port}/?suite={args.suite}'
    try:
        server = make_server(args.port, studio)
    except OSError as error:
        if error.errno == errno.EADDRINUSE and running_here(args.port):
            print(f'RATW Studio is already running: {url}', flush=True)
            return 0
        print(f'Port {args.port} is taken ({error.strerror}); choose another with --port N.', file=sys.stderr)
        return 1
    try:
        live = f'the live server runs {world_db.live_world()}'
    except world_db.DatabaseError as error:
        live = str(error)
    print(f'RATW Studio: {url}\n'
          f'Atlas on {key} ("{world_db.world(key)["name"]}", {world_db.world(key)["database"]}); the DM on live worlds '
          f'({live}). Sign in with a DM account. Local-only. Ctrl+C to stop.', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
