#!/usr/bin/env python3
"""The staged economy's DM and Atlas contract (Docs/Design/79-a-staged-economy.md, 12 and 14; Phase 9).

A leaf module: the standard library and the data files only (Data/Economy/status.json and market.json, and
Data/Items/items.json for the goods), never map_editor or world_store, so the studio, the Dungeon Master tool,
world_store, live_edit and map_editor can all use it. What a world keeps in live.economy.rules
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

ROOT = Path(__file__).resolve().parent.parent
ECONOMY_DATA = ROOT / 'Data' / 'Economy'

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

# A rule's, a service's or a season group's name as the manifest can carry it (migration 0061's live.economy_name, the same
# pattern): one plain word, so it never splits a manifest line or opens a new record (doc 79, Phase 9's last repair).
NAME = re.compile(r'[A-Za-z][A-Za-z0-9_-]{0,63}\Z')

_FORBIDDEN = re.compile(r'(?<![A-Za-z])dogs?(?![A-Za-z])', re.IGNORECASE)


def forbidden_word(text: str) -> bool:
    """The word world text never has (market::forbiddenWord): "dog" or "dogs" as a word, in any case."""
    return bool(_FORBIDDEN.search(text or ''))


def name_fits(name) -> bool:
    """Whether a rule's (or a service's, a season group's) name is one plain word: a letter, then letters, digits, _ or -,
    64 at most. Anything else (a space, a quote, a newline) is refused at every door and never written to the manifest."""
    return isinstance(name, str) and NAME.match(name) is not None


def shown_name(name) -> str:
    """A refused name as an error message shows it: its first 40 characters, escaped."""
    return repr(str(name)[:40])


def rule_name_problems(names) -> list[str]:
    """The names among `names` that are no plain word, each as a refusal."""
    return [f'economy.rules: {shown_name(n)} is not a rule\'s name (one plain word: letters, digits, _ and -).'
            for n in names if not name_fits(n)]


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


MIN_SHARE = 2                               # Core's MinShare (RatwStatus.cpp): a band's share, at least.
EVENT_KINDS_MOST = 64                       # Core's eventKindsOf.
CAUSES_MOST = 32                            # Core's causesOf.
SHIPS_MOST = 64                             # Core's shipNamesOf.
PER_WEEK_MOST = 3
EVENT_ID = re.compile(r'[a-z0-9_]{1,32}\Z')
EVENT_NEEDS = ('', 'city', 'port', 'cart', 'festival')
EVENT_SHIP = ('', 'docks', 'lost')
CAUSE_PARTS = ('down', 'up', 'noticeDown', 'noticeUp', 'cryDown', 'cryUp')


def _data(name: str) -> dict:
    try:
        value = json.loads((ECONOMY_DATA / name).read_text(encoding='utf-8'))
        return value if isinstance(value, dict) else {}
    except (OSError, ValueError):
        return {}


def market_file() -> dict:
    """Data/Economy/market.json ({} if it can't be read)."""
    return _data('market.json')


def items_file() -> dict:
    """Data/Items/items.json, the catalogue ({} if it can't be read)."""
    try:
        value = json.loads((ROOT / 'Data' / 'Items' / 'items.json').read_text(encoding='utf-8'))
        return value if isinstance(value, dict) else {}
    except (OSError, ValueError):
        return {}


def base_goods(items: dict | None = None) -> list[str]:
    """The catalogue's goods (Data/Items/items.json's ids, in file order): Core's items::good, without a maker's mark;
    what neverImported and an event kind's goods may name."""
    items = items_file() if items is None else items
    return [i['id'] for i in items.get('items') or [] if isinstance(i, dict) and isinstance(i.get('id'), str)]


def _number(v) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)


def forbidden_in_name(text: str) -> bool:
    """Core's market::forbiddenInName: d-o-g anywhere in an id or a name."""
    return 'dog' in text.lower()


def ship_name_fits(name) -> bool:
    """Core's market::shipNameFits: letters, spaces, ' and -, at most 40, never the forbidden word inside."""
    return (isinstance(name, str) and 0 < len(name.encode('utf-8')) <= 40 and not forbidden_in_name(name)
            and all(c.isalpha() or c in " '-" for c in name) and name.isascii())


def _bytes(value) -> int:
    return len(json.dumps(value, separators=(',', ':'), ensure_ascii=False).encode('utf-8'))


def _same_shape(value, default, path: str, errors: list[str], extra=()):
    """A status part as its default has it: the same JSON types, objects naming only the default's keys (merged over it),
    lists of the default's kind of element, numbers finite."""
    if isinstance(default, dict):
        if not isinstance(value, dict):
            errors.append(f'{path} must be an object, as status.json has it.')
            return
        for k, v in value.items():
            if k not in default and k not in extra:
                errors.append(f'{path} has no {k!r} (status.json\'s: {", ".join(default) or "none"}).')
            elif k in default:
                _same_shape(v, default[k], f'{path}.{k}', errors)
    elif isinstance(default, list):
        if not isinstance(value, list):
            errors.append(f'{path} must be a list, as status.json has it.')
        elif default:
            for i, v in enumerate(value):
                _same_shape(v, default[0], f'{path}[{i}]', errors)
    elif isinstance(default, bool):
        if not isinstance(value, bool):
            errors.append(f'{path} must be true or false.')
    elif _number(default):
        if not _number(value):
            errors.append(f'{path} must be a number.')
    elif isinstance(default, str):
        if plain_words(value, 200) is None:
            errors.append(f'{path} must be plain words (at most 200 bytes, never the forbidden word).')


def _check_shares(rows, errors: list[str]):
    """status.json's shares as Core's sharesValid has them: 2-21 rows {at, shares: [4]}, from -100 to 100 in rising
    order, each row summing to 100 with every share at least 2, destitute never rising and rich never falling."""
    if not isinstance(rows, list) or not 2 <= len(rows) <= 21:
        errors.append('status.shares must be 2 to 21 rows {at, shares: [destitute, poor, comfortable, rich]}.')
        return
    last = None
    for i, row in enumerate(rows):
        shares = row.get('shares') if isinstance(row, dict) else None
        if (not isinstance(row, dict) or set(row) - {'at', 'shares'} or not _number(row.get('at')) or not isinstance(shares, list)
                or len(shares) != 4 or not all(_number(s) for s in shares)):
            errors.append(f'status.shares[{i}] must be {{at: -100..100, shares: four percents}}.')
            return
        if any(s < MIN_SHARE for s in shares):
            errors.append(f'status.shares[{i}]: every band\'s share is at least {MIN_SHARE}%.')
        if abs(sum(shares) - 100) > 1e-6:
            errors.append(f'status.shares[{i}] sums to {sum(shares):g}, not 100.')
        if last is not None:
            if row['at'] <= last['at']:
                errors.append('status.shares: the rows run in rising order of the slider.')
            if shares[0] > last['shares'][0] + 1e-9:
                errors.append(f'status.shares[{i}]: the destitute share never rises as the slider rises.')
            if shares[3] < last['shares'][3] - 1e-9:
                errors.append(f'status.shares[{i}]: the rich share never falls as the slider rises.')
        last = row
    if rows[0].get('at') != -100 or rows[-1].get('at') != 100:
        errors.append('status.shares run from -100 to 100.')


def _check_caps(caps, default: dict, errors: list[str]):
    if not isinstance(caps, dict):
        errors.append('status.caps must be an object of daily caps.')
        return
    for k, v in caps.items():
        if k == 'grantsADay':
            high = GRANT_MOST
        elif k in default and _number(default[k]):
            high = CAP_CEILING * default[k]
        else:
            errors.append(f'status.caps has no {k!r} (status.json\'s: {", ".join([*default, "grantsADay"])}).')
            continue
        # (welcomeDays is a period: shorter is the generous way, so it is at least the shipped default, as Core holds it.)
        low = default[k] if k == 'welcomeDays' and k in default and _number(default[k]) else 0
        if not _number(v) or v < low or v > high:
            errors.append(f'status.caps.{k} must be a number from {low:g} to {high:g}'
                          + (' (5 times the shipped default).' if k != 'grantsADay' else '.')
                          + (' A welcome no oftener than the shipped week.' if low else ''))


def _check_status(status, default: dict, errors: list[str], warnings: list[str]):
    if not isinstance(status, dict):
        errors.append('status must be an object: a part of status.json.')
        return
    for k, v in status.items():
        if k not in STATUS_KEYS:
            errors.append(f'status has no {k!r} a host may change (they are: {", ".join(STATUS_KEYS)}).')
        elif k == 'shares':
            _check_shares(v, errors)
        elif k == 'caps':
            _check_caps(v, default.get('caps') or {}, errors)
        elif k == 'rows':
            if not isinstance(v, dict):
                errors.append('status.rows must be {band: {setting: value}}.')
                continue
            for band, row in v.items():
                if band not in BANDS:
                    errors.append(f'status.rows names no band: {band!r} (the bands: {", ".join(BANDS)}).')
                elif band in (default.get('rows') or {}):
                    _same_shape(row, default['rows'][band], f'status.rows.{band}', errors)
        elif k in default:
            _same_shape(v, default[k], f'status.{k}', errors)
            if _number(v) and v < 0 and _number(default[k]) and default[k] >= 0:
                errors.append(f'status.{k} must not be below 0.')
        else:
            warnings.append(f'status.{k}: this build\'s status.json has none; kept, and Core reads it if it knows it.')


def _check_kind(kind, i: int, goods: set[str], errors: list[str], warnings: list[str]) -> str | None:
    """One staged event kind as Core's eventKindsOf takes it; its id, or None when it would be skipped."""
    where = f'market.events.list[{i}]'
    if not isinstance(kind, dict):
        errors.append(f'{where} must be an event kind {{id, words, notice, cry, goods, ...}}.')
        return None
    kid = kind.get('id')
    if not isinstance(kid, str) or not EVENT_ID.match(kid) or forbidden_in_name(kid):
        errors.append(f'{where}: its id is 1-32 lower-case letters, digits or _ (never the forbidden word).')
        return None
    # (Every text of a kind Core holds to plain words: eventKindsOf. `noticeAfter`, the notice once the event's ship has
    # sailed, since the decisions of 2026-11-12. A kind's old `times` is read past, as Core reads it: events move no Times.)
    for key, most, needed in (('words', 80, True), ('notice', 280, False), ('cry', 200, False), ('noticeOn', 280, False),
                              ('noticeAfter', 280, False), ('sign', 40, False)):
        v = kind.get(key, '' if not needed else None)
        if (needed and not v) or plain_words(v if v is not None else '', most) is None:
            errors.append(f'{where} ({kid}).{key} must be plain words of at most {most} bytes'
                          + (' (it is needed).' if needed else '.'))
    if kind.get('needs', '') not in EVENT_NEEDS:
        errors.append(f'{where} ({kid}).needs is one of: {", ".join(n or "(none)" for n in EVENT_NEEDS)}.')
    if kind.get('ship', '') not in EVENT_SHIP:
        errors.append(f'{where} ({kid}).ship is one of: {", ".join(n or "(none)" for n in EVENT_SHIP)}.')
    pressed = kind.get('goods') or {}
    if not isinstance(pressed, dict):
        errors.append(f'{where} ({kid}).goods must be {{good: pressure}}.')
        pressed = {}
    unknown = sorted(g for g in pressed if g not in goods)
    if unknown:
        warnings.append(f'{where} ({kid}): no such good, skipped: {", ".join(unknown[:8])}.')
    known = [g for g, p in pressed.items() if g in goods and _number(p) and abs(p) >= .01]
    if not known and not any(_number(kind.get(k)) and abs(kind[k]) >= .01 for k in ('yields', 'brings')):
        errors.append(f'{where} ({kid}) presses no good (goods, yields or brings): Core would skip it.')
    return kid


def _check_causes(causes, errors: list[str]):
    if not isinstance(causes, dict):
        errors.append('market.events.causes must be {down, up, noticeDown, noticeUp, cryDown, cryUp}.')
        return
    for k, v in causes.items():
        if k not in CAUSE_PARTS:
            errors.append(f'market.events.causes has no {k!r} (they are: {", ".join(CAUSE_PARTS)}).')
        elif k in ('down', 'up'):
            if not isinstance(v, list) or not 0 < len(v) <= CAUSES_MOST:
                errors.append(f'market.events.causes.{k} must be 1 to {CAUSES_MOST} causes.')
                continue
            seen = set()
            for i, c in enumerate(v):
                ok = (isinstance(c, dict) and isinstance(c.get('id'), str) and 0 < len(c['id']) <= 32
                      and not forbidden_in_name(c['id']) and c['id'] not in seen and bool(c.get('words'))
                      and plain_words(c.get('words'), 80) is not None and plain_words(c.get('notice', ''), 280) is not None
                      and plain_words(c.get('cry', ''), 200) is not None)
                if not ok:
                    errors.append(f'market.events.causes.{k}[{i}] must be {{id (distinct, at most 32), words (at most 80), '
                                  'notice (280), cry (200)}, plain words.')
                else:
                    seen.add(c['id'])
        elif plain_words(v, 200) is None or ('{cause}' not in v and '{Cause}' not in v):
            errors.append(f'market.events.causes.{k} must be plain words of at most 200 bytes holding {{cause}}.')


def _check_market(market, goods: set[str], errors: list[str], warnings: list[str]):
    if not isinstance(market, dict):
        errors.append('market must be {events?: {perWeek?, list?, causes?}, wharf?: {ships?}}.')
        return
    allowed = {}
    for a, b in MARKET_PATHS:
        allowed.setdefault(a, set()).add(b)
    for part, value in market.items():
        if part not in allowed or not isinstance(value, dict) or set(value) - allowed[part]:
            errors.append(f'market may set only: {", ".join(f"{a}.{b}" for a, b in MARKET_PATHS)}.')
            return
    events = market.get('events') or {}
    if 'perWeek' in events and (not _number(events['perWeek']) or not 0 <= events['perWeek'] <= PER_WEEK_MOST):
        errors.append(f'market.events.perWeek must be a number from 0 to {PER_WEEK_MOST}.')
    if 'list' in events:
        kinds = events['list']
        if not isinstance(kinds, list) or len(kinds) > EVENT_KINDS_MOST:
            errors.append(f'market.events.list must be at most {EVENT_KINDS_MOST} event kinds.')
        else:
            ids = [_check_kind(k, i, goods, errors, warnings) for i, k in enumerate(kinds)]
            named = [i for i in ids if i]
            if len(set(named)) != len(named):
                errors.append('market.events.list names a kind twice.')
            if not kinds:
                warnings.append('market.events.list is empty: no staged events in this world.')
    if 'causes' in events:
        _check_causes(events['causes'], errors)
    ships = (market.get('wharf') or {}).get('ships')
    if ships is not None:
        if (not isinstance(ships, list) or not 0 < len(ships) <= SHIPS_MOST or not all(ship_name_fits(n) for n in ships)
                or len(set(ships)) != len(ships)):
            errors.append(f'market.wharf.ships must be 1 to {SHIPS_MOST} distinct names: letters, spaces, \' and -, '
                          'at most 40 each (never the forbidden word).')


def check_overrides(o, goods=None, defaults=None) -> tuple[list[str], list[str]]:
    """(errors, warnings) of a world's overrides ({subset of OVERRIDE_KEYS}), before the studio writes them: errors refuse
    the change (Core would skip that part, warned of), warnings are kept and said. `goods`: the goods neverImported
    and the event kinds may name (None: the catalogue's, base_goods); `defaults`: load_defaults()'s shape (None: the data files')."""
    errors, warnings = [], []
    if not isinstance(o, dict):
        return ['The overrides must be an object of ' + ', '.join(OVERRIDE_KEYS) + '.'], []
    goods = set(base_goods() if goods is None else goods)
    d = load_defaults() if defaults is None else defaults
    for k in o:
        if k not in OVERRIDE_KEYS:
            errors.append(f'No such override: {k!r} (they are: {", ".join(OVERRIDE_KEYS)}).')
    for k in OVERRIDE_KEYS:
        if k in o and _bytes(o[k]) > OVERRIDE_BYTES_MOST:
            errors.append(f'{k} is {_bytes(o[k])} bytes: at most {OVERRIDE_BYTES_MOST}.')
    if 'prosperityCause' in o and plain_words(o['prosperityCause'], CAUSE_MOST) is None:
        errors.append(f'prosperityCause must be plain words of at most {CAUSE_MOST} bytes (never the forbidden word).')
    if 'neverImported' in o:
        never = o['neverImported']
        if not isinstance(never, list) or not all(isinstance(g, str) for g in never) or len(set(never)) != len(never):
            errors.append('neverImported must be a list of distinct base goods.')
        else:
            unknown = sorted(g for g in never if g not in goods)
            if unknown:
                errors.append(f'neverImported names no good of the catalogue: {", ".join(unknown[:8])}.')
    if 'status' in o:
        _check_status(o['status'], d.get('status') or {}, errors, warnings)
    if 'market' in o:
        _check_market(o['market'], goods, errors, warnings)
    return errors, warnings
