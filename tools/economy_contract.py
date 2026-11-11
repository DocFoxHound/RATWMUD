#!/usr/bin/env python3
"""The staged economy's DM and Atlas contract (Docs/Design/79-a-staged-economy.md, 12 and 14; Phase 9).

A leaf module: the standard library and Data/Economy/*.json only, never map_editor or world_store, so the studio, the
Dungeon Master tool, world_store, live_edit and map_editor can all use it. What a world keeps in live.economy.rules
beside its integer rules (the "overrides"), the prosperity slider's range, the bands, and the checks of each.

    rules = {"<items::Rules name>": int, ..., "prosperity": -100..100 (step 5), "harvestBias": -20..20,
             "prosperityCause": str (<= 80 bytes, plain words), "neverImported": [base good],
             "status": {partial status.json}, "market": {"events": {...}, "wharf": {"ships": [...]}}}
"""
from __future__ import annotations

import json
import math
import re
from pathlib import Path

ECONOMY_DATA = Path(__file__).resolve().parent.parent / 'Data' / 'Economy'

BANDS = ('destitute', 'poor', 'comfortable', 'rich')
PROSPERITY = {'low': -100, 'high': 100, 'step': 5}
CAUSE_MOST = 80
OVERRIDE_KEYS = ('prosperityCause', 'neverImported', 'status', 'market')
OVERRIDE_BYTES_MOST = 12000                     # Each override object, serialized.
STATUS_KEYS = ('shares', 'rows', 'caps', 'pay', 'street', 'score', 'hold', 'leanDays', 'timesShift', 'smoothing',
               'hysteresis', 'shortHard', 'shortBoom', 'shortPressureMost', 'shortPressureAt')
MARKET_PATHS = (('events', 'perWeek'), ('events', 'list'), ('events', 'causes'), ('wharf', 'ships'))
HARVEST_WORDS = ('poor', 'fair', 'good')
GRANT_MOST = 2500                               # The most a host may raise caps.grantsADay to (and one grant's most).
CAP_CEILING = 5                                 # A host's faucet cap: at most 5x the shipped default.

_FORBIDDEN = re.compile(r'(?<![A-Za-z])dogs?(?![A-Za-z])', re.IGNORECASE)


def forbidden_word(text: str) -> bool:
    """The word world text never has (market::forbiddenWord): "dog" or "dogs" as a word, in any case."""
    return bool(_FORBIDDEN.search(text or ''))


def clamp_prosperity(v) -> int:
    """The slider's target: a number clamped to -100..100 and rounded to the nearest 5 (37 is 35, 9999 is 100); anything
    else is 0, the rule's fallback (doc 79, 12: clamped, never refused)."""
    if isinstance(v, bool):
        return 0
    if isinstance(v, str):
        try:
            v = float(v.strip())
        except ValueError:
            return 0
    if not isinstance(v, (int, float)) or not math.isfinite(v):
        return 0
    v = max(PROSPERITY['low'], min(PROSPERITY['high'], v))
    step = PROSPERITY['step']
    return int(math.floor(v / step + .5)) * step


def plain_words(s, most: int = CAUSE_MOST) -> str | None:
    """market::plainWords: the text stripped, at most `most` bytes (UTF-8), no control character and never the
    forbidden word; else None."""
    if not isinstance(s, str):
        return None
    s = s.strip()
    if len(s.encode('utf-8')) > most or forbidden_word(s) or any(ord(c) < 32 or ord(c) == 127 for c in s):
        return None
    return s


def check_band(v) -> str | None:
    """An authored band's word, or None (from rank)."""
    return v if isinstance(v, str) and v in BANDS else None


def load_defaults() -> dict:
    """The shipped defaults the overrides are over: status.json's STATUS_KEYS part, market.json's events and wharf."""
    status = json.loads((ECONOMY_DATA / 'status.json').read_text(encoding='utf-8'))
    market = json.loads((ECONOMY_DATA / 'market.json').read_text(encoding='utf-8'))
    return {'status': {k: status[k] for k in STATUS_KEYS if k in status},
            'market': {'events': market.get('events', {}), 'wharf': market.get('wharf', {})}}


def check_overrides(o, goods, defaults) -> tuple[list[str], list[str]]:
    """The overrides' errors (refused) and warnings (kept, ignored where Core would skip them). `goods`: the known base
    goods; `defaults`: load_defaults()'s shape. Phase 9's editor lane fills this in (doc 79, Phase 9 plan, E2)."""
    return [], []
