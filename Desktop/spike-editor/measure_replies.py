#!/usr/bin/env python3
"""Doc 78, Phase M0a: the size of the DM tool's and Atlas's GET replies on a world, raw and gzipped (read-only).

Calls the methods behind each GET route directly (no studio, no sign-in: the DM accounts live in PROD, which this
never touches) and encodes each answer as the handlers do (json.dumps(..., ensure_ascii=True), tools/http_body.py's
gzip level 3). Usage: python3 Desktop/spike-editor/measure_replies.py [WORLD_KEY] (default dev). One JSON line a route.
"""
import gzip
import json
import sys
import time
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[2] / 'tools'
sys.path.insert(0, str(TOOLS))

import dungeon_master as D  # noqa: E402
import live_edit  # noqa: E402

key = sys.argv[1] if len(sys.argv) > 1 else 'dev'
dm = D.DungeonMaster()
atlas = live_edit.LiveWorld(key=key)
who = {'username': 'm0a-measure', 'role': 'admin'}


def measure(route, fn):
    t = time.perf_counter()
    try:
        data = fn()
    except Exception as error:  # noqa: BLE001 - a route that can't answer here is reported, not fatal.
        print(json.dumps({'route': route, 'error': f'{type(error).__name__}: {str(error)[:200]}'}), flush=True)
        return None
    ms = round((time.perf_counter() - t) * 1000)
    body = json.dumps(data, ensure_ascii=True, default=str).encode('utf-8')
    t = time.perf_counter()
    gz3 = gzip.compress(body, compresslevel=3)
    gz_ms = round((time.perf_counter() - t) * 1000)
    print(json.dumps({'route': route, 'bytes': len(body), 'gzip3': len(gz3), 'gzip9': len(gzip.compress(body, 9)),
                      'build_ms': ms, 'gzip3_ms': gz_ms}), flush=True)
    return data


lean = measure('/api/atlas/live/world?lean=1', lambda: atlas.load(lean=True))
measure('/api/atlas/live/world (full)', lambda: atlas.load(lean=False))
cells = []
if lean:
    project = lean.get('project') or {}
    cells = [c.get('id') for c in (project.get('cells') or []) if isinstance(c, dict) and c.get('id')]
measure(f'/api/atlas/live/ground (all {len(cells)} cells at once)', lambda: atlas.ground(cells))
measure('/api/atlas/live/ground (16 cells)', lambda: atlas.ground(cells[:16]))
measure('/api/atlas/live/camps', atlas.camps)
measure('/api/atlas/roster', atlas.roster.load)
measure('/api/dm/world?lean=1', lambda: dm.world_map(key, lean=True))
measure('/api/dm/world (full)', lambda: dm.world_map(key, lean=False))
measure('/api/dm/ground (16 cells)', lambda: dm.ground(key, cells[:16]))
measure('/api/dm/live', lambda: dm.live(who, key))
measure('/api/dm/players', lambda: dm.players(key))
measure('/api/dm/health (24 h)', lambda: dm.health(key, 24))
measure('/api/dm/npcs', lambda: dm.npcs(key))
for name in ('rumours', 'fame', 'storytellers', 'dialects', 'figures', 'projects', 'prosperity', 'calendar', 'artwork',
             'reports', 'chapters', 'factions'):
    measure(f'/api/dm/{name}', lambda name=name: getattr(dm, name)(key))
measure('/api/dm/talk', lambda: dm.talk(key, ''))
measure('/api/dm/chronicle', lambda: dm.chronicle(key, ''))
