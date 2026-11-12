#!/usr/bin/env python3
"""The NPC records' economy lines and a resident's band line, as live.people_manifest writes them since migration 0061
(Docs/Design/79-a-staged-economy.md, 14; Phase 9). One owner for both halves of the grammar: the SQL function and the
exporter (tools/map_editor.py's export_files) write the same lines from the same entity, and Core's Authoring reads
both (leniently: RatwAuthoring.cpp).

    economy T SH SM 10 12                            once (10 12: the old daily herbs and meals, for older servers)
    loop needs off                                   when needs are paused
    season "<group>" price P P P P shelf S S S S      a group the world changed (by text); one stored with its supply
                                                     and normal keeps 0060's line: supply ... price ... shelf ... normal N
    rule "<name>" N                                  an integer rule the world changed (by text)
    service "<id>" on|off price N every D            a service the world changed (by text); a switch that is not
                                                     true or false, or a price or days not a number: "unset" (Core skips it)
    overrides {json}                                 prosperityCause, neverImported, status, market (when any)
    band "<npc>" destitute|poor|comfortable|rich     after its resident's records (person_lines)

The economy entity is world_store's: {treasury, storeHerbs, storeMeals, needsPaused, seasons: {g: {price, shelf}},
rules: {name: int}, services: {id: {on, price, every}}, overrides: {...}}.
"""
from __future__ import annotations

import json

from economy_contract import BANDS, OVERRIDE_KEYS

OVERRIDES_MOST = 65536                          # The overrides line's JSON Core reads at most (longer: skipped, warned).
# The economy line's last two numbers: the old daily herbs and meals at their defaults, read past since Phase 9, written
# so a server built before it still reads a world with no overrides or band lines (migration 0061's own).
OLD_DAILY = '10 12'
ECONOMY_KINDS = ('economy', 'loop', 'season', 'rule', 'service', 'overrides')


def quote(value) -> str:
    """live.q (std::quoted's escaping): UTF-8 literal, no JSON escapes."""
    return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"') + '"'


def _whole(v) -> bool:
    return isinstance(v, int) and not isinstance(v, bool)


def _four(v) -> bool:
    return isinstance(v, list) and len(v) == 4


def _number(v) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def _numbers(v) -> str:
    return ' '.join(str(x) if _number(x) else 'unset' for x in v)


def economy_lines(entity) -> list[str]:
    """The economy's lines for an entity, in live.people_manifest's order (economy, loop, then seasons, rules and services
    each sorted by text, then the overrides)."""
    e = entity or {}
    out = [f'economy {e.get("treasury", 1000)} {e.get("storeHerbs", 100)} {e.get("storeMeals", 50)} {OLD_DAILY}']
    if e.get('needsPaused'):
        out.append('loop needs off')
    seasons = []
    for group, g in (e.get('seasons') or {}).items():
        if isinstance(g, dict) and _four(g.get('price')) and _four(g.get('shelf')):
            old = _four(g.get('supply')) and _number(g.get('normal'))     # (0060's line: older servers read it.)
            seasons.append(f'season {quote(group)} ' + (f'supply {_numbers(g["supply"])} ' if old else '') +
                           f'price {_numbers(g["price"])} shelf {_numbers(g["shelf"])}' + (f' normal {g["normal"]}' if old else ''))
    out += sorted(seasons)
    out += sorted(f'rule {quote(name)} {value}' for name, value in (e.get('rules') or {}).items() if _whole(value))
    word = lambda on: 'on' if on is True else 'off' if on is False else 'unset'
    num = lambda v: str(v) if _number(v) else 'unset'
    out += sorted(f'service {quote(sid)} {word(s.get("on"))} price {num(s.get("price"))} every {num(s.get("every"))}'
                  for sid, s in (e.get('services') or {}).items() if isinstance(s, dict))
    overrides = {k: v for k, v in (e.get('overrides') or {}).items() if k in OVERRIDE_KEYS}
    if overrides:
        # (Whatever its length, as the SQL: one too long is Core's to skip with a warning.)
        out.append('overrides ' + json.dumps(overrides, ensure_ascii=False))
    return out


def person_lines(person) -> list[str]:
    """A resident's economy follow-lines, after its protected record: its authored band (none: from rank)."""
    band = (person or {}).get('band')
    return [f'band {quote(person["id"])} {band}'] if band in BANDS else []


def comparable(lines) -> list:
    """Lines as a parity test compares them: sorted, the overrides line as parsed JSON (key order aside)."""
    out = []
    for line in lines:
        if line.startswith('overrides '):
            out.append(('overrides', json.dumps(json.loads(line[len('overrides '):]), sort_keys=True)))
        else:
            out.append((line, ''))
    return sorted(out)
