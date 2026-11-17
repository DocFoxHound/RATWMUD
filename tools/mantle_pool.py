#!/usr/bin/env python3
"""The mantle pools (Docs/Design/84-living-conversations.md §12.1, Phase 10): read-only.

    python3 tools/mantle_pool.py forecast ridgemere3 [--years 5] [--json]
    python3 tools/mantle_pool.py names [--json]

`forecast` reads a world's rows (never writes; a read-only transaction) and projects, dialect by dialect, the births the
pools must give: births follow deaths one for one, realm-wide (the user's Q5), so the deaths expected each year (doc 64's
Gompertz, Core/RatwSociety.h's DeathA, DeathB and InfantDeath, on the living residents' ages, a year older each year) are
shared among the dialects as the fertile couples' mothers are (a married couple at home together, the mother 18 to 45,
fewer than three children). Beside them, each pool as the rows have it (spare: ready and never bound; resting: its last
wearer died within mantle_rest_days; ready: rested), the game's health line (low when spare + ready falls under 1.5 x the
births expected in the next 180 days, plus 2: §12.1) and each pool's honesty mix against its dialect's living residents'
(§9.5: P15 writes the spares to it; mantle_check --pools gates it within 5 points).

`names` checks the name pools (Data/Talk/names/<dialect>.names, §7.10) as the game reads them: one `names <dialect>`
line, female and male lists of one-word names, no name twice or in both lists, named_for a share, a pool for every
dialect, about 70 a sex (fewer is a warning), and no word the world's text never says.
"""
from __future__ import annotations

import argparse
import json
import math
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
TALK = ROOT / 'Data' / 'Talk'
NAMES = TALK / 'names'
# Core/RatwSociety.h (Society::DeathA, DeathB, InfantDeath) and Core/RatwCalendar.h (DaysPerYear).
DEATH_A, DEATH_B, INFANT = .0004, .085, .03
DAYS_PER_YEAR = 365
DIALECTS = ('plain', 'coldwater', 'frosthollow', 'larkfield', 'mirebridge', 'oakhaven', 'southmeadow', 'saltwell')
PLACES = ('ridgemere', 'frosthollow', 'coldwater', 'oakhaven', 'mirebridge', 'larkfield', 'southmeadow', 'saltwell')
HONESTY = ('frank', 'plain', 'guarded', 'sly')
NAME = re.compile(r"[A-Z][a-z']+")


def settings() -> dict:
    return json.loads((TALK / 'settings.json').read_text())


def pool_of(town: str, dialect_of: dict) -> str:
    """World::dialectPoolOf: a town's grammar, plain for Ridgemere, '' for none of the realm's."""
    if town not in dialect_of and town not in PLACES:
        return ''
    d = dialect_of.get(town, town)
    return 'plain' if d == 'ridgemere' else d


def yearly_death(age: float) -> float:
    return min(1.0, DEATH_A * math.exp(DEATH_B * age) + (INFANT if age < 5 else 0.0))


def deaths_within(ages, days: float) -> float:
    """Deaths expected among wolves of these ages within `days` (each its own chance, as World::tendOldAge draws it)."""
    return sum(1 - (1 - yearly_death(a)) ** (days / DAYS_PER_YEAR) for a in ages)


def project(residents: list[dict], couples: list[dict], years: int = 5) -> dict:
    """Births by dialect: {'next180': {d: n}, 'years': [{d: n} for each year]}. `residents`: {age, dialect}; `couples`:
    {mother_age, dialect, children}. Pure, for the tests."""
    def share(year: int) -> dict:
        mothers = Counter(c['dialect'] for c in couples if 18 <= c['mother_age'] + year <= 45 and c['children'] < 3)
        total = sum(mothers.values())
        return {d: n / total for d, n in mothers.items()} if total else {}

    out = {'next180': {}, 'years': []}
    now = deaths_within([r['age'] for r in residents], 180)
    out['next180'] = {d: round(now * s, 2) for d, s in share(0).items()}
    for y in range(years):
        deaths = sum(yearly_death(r['age'] + y) for r in residents)
        out['years'].append({d: round(deaths * s, 2) for d, s in share(y).items()})
    return out


def pools(cards: list[dict], bindings: list[dict], today: float, rest_days: float) -> dict:
    """Each dialect's pool from the rows (§9.2: worked out from the bindings and state, never stored): {d: {spare,
    resting, ready, worn, honesty}}. `cards`: ready mantles {id, dialect, honesty}; `bindings`: {mantle, alive, released}."""
    status = {}
    for b in bindings:
        s = status.setdefault(b['mantle'], {'worn': False, 'released': -1.0})
        if b['released'] is None and b['alive']:
            s['worn'] = True
        let = b['released'] if b['released'] is not None else (None if b['alive'] else b.get('bound') or 0.0)
        if let is not None:
            s['released'] = max(s['released'], let)
    out = defaultdict(lambda: {'spare': 0, 'resting': 0, 'ready': 0, 'worn': 0, 'honesty': Counter()})
    for c in cards:
        p = out[c['dialect']]
        s = status.get(c['id'])
        if s and s['worn']:
            p['worn'] += 1
            continue
        p['honesty'][c.get('honesty') or 'plain'] += 1
        if not s or s['released'] < 0:
            p['spare'] += 1
        elif today - s['released'] >= rest_days:
            p['ready'] += 1
        else:
            p['resting'] += 1
    return dict(out)


def mix(counter: Counter) -> dict:
    total = sum(counter.values())
    return {h: round(100 * counter.get(h, 0) / total, 1) for h in HONESTY} if total else {}


def read_world(conn) -> dict:
    """What the forecast needs, read in one read-only transaction."""
    import mantle_check
    world = mantle_check.world_id_of(conn)
    conn.execute('SET TRANSACTION READ ONLY')
    region = dict(conn.execute('SELECT id, region FROM world.cells WHERE world_id = %s UNION ALL '
                               'SELECT id, region FROM world.interiors WHERE world_id = %s', (world, world)).fetchall())
    dead = {r[0] for r in conn.execute('SELECT npc_id FROM live.npc_state WHERE world_id = %s AND NOT alive', (world,)).fetchall()}
    saved = {r[0]: r[1] for r in conn.execute("SELECT key, data->>'age' FROM game.npcs WHERE world_id = %s", (world,)).fetchall()}
    for k, v in list(saved.items()):
        try:
            saved[k] = int(v)
        except (TypeError, ValueError):
            del saved[k]
    npcs = {}
    for npc, age, home, sex in conn.execute("SELECT id, age, home_area, appearance->>'sex' FROM live.npcs WHERE world_id = %s AND "
                                            "home_area IS NOT NULL AND spawn_id IS NULL", (world,)).fetchall():
        if npc not in dead:
            npcs[npc] = {'age': saved.get(npc, age or 30), 'home': home, 'region': region.get(home, ''), 'sex': sex or ''}
    voice = {r[0]: r[1] for r in conn.execute("SELECT npc_id, traits->>'honesty' FROM live.npc_voice WHERE world_id = %s", (world,))}
    relations = conn.execute('SELECT npc_id, other_id, kind FROM live.npc_relations WHERE world_id = %s', (world,)).fetchall()
    # The checkpoint's families too: marriages made in play and the children born, as the game's society has them.
    saved_kin = conn.execute("SELECT payload::jsonb->'society'->'careers'->'spouses', payload::jsonb->'society'->'careers'->'parents', "
                             "payload::jsonb->'society'->'careers'->'births' FROM game.checkpoints WHERE world_id = %s", (world,)).fetchone()
    births = {}
    if saved_kin:
        spouses, parents, births = (v if isinstance(v, dict) else {} for v in saved_kin)
        relations = list(relations) + [(a, b, 'spouse') for a, b in spouses.items() if isinstance(b, str)]
        relations += [(p, child, 'parent') for child, ps in parents.items() if isinstance(ps, list) for p in ps if isinstance(p, str)]
    has_mantles = conn.execute("SELECT to_regclass('live.mantles') IS NOT NULL").fetchone()[0]
    cards, bindings = [], []
    if has_mantles:
        cards = [{'id': r[0], 'dialect': r[1], 'honesty': r[2]} for r in conn.execute(
            "SELECT id, dialect, traits->>'honesty' FROM live.mantles WHERE world_id = %s AND state = 'ready'", (world,))]
        bindings = [{'npc': r[0], 'mantle': r[1], 'released': r[2], 'bound': r[3], 'alive': r[0] in npcs} for r in conn.execute(
            'SELECT npc_id, mantle_id, released_day, bound_day FROM live.npc_voice WHERE world_id = %s AND mantle_id IS NOT NULL',
            (world,))]
    import mantle_store
    return {'world': world, 'npcs': npcs, 'voice': voice, 'relations': relations, 'births': births, 'cards': cards, 'bindings': bindings,
            'day': mantle_store.world_day(conn, world) or 0.0, 'mantles': bool(has_mantles)}


def forecast(data: dict, years: int = 5) -> dict:
    conf = settings()
    dialect_of = conf.get('dialect_of', {})
    rest = float(conf.get('mantle_rest_days', 365))
    npcs = data['npcs']
    dialect = {n: pool_of(v['region'], dialect_of) for n, v in npcs.items()}
    spouses = set()
    for a, b, kind in data['relations']:
        if kind == 'spouse':
            spouses.add(tuple(sorted((a, b))))
    couples = []
    for a, b in spouses:
        if a not in npcs or b not in npcs or npcs[a]['home'] != npcs[b]['home']:
            continue
        mother = next((m for m in (a, b) if npcs[m]['sex'] == 'female'), None)
        if mother:
            # (Society::tendBirths counts a couple's own births in play, "a|b", three at most.)
            couples.append({'mother_age': npcs[mother]['age'], 'dialect': dialect[mother],
                            'children': int((data.get('births') or {}).get(f'{a}|{b}', 0) or 0)})
    births = project([{'age': v['age'], 'dialect': dialect[n]} for n, v in npcs.items()], couples, years)
    have = pools(data['cards'], data['bindings'], data['day'], rest)
    living = defaultdict(Counter)
    for n in npcs:
        living[dialect[n]][data['voice'].get(n) or 'plain'] += 1
    report = {'world': data['world'], 'day': data['day'], 'restDays': rest, 'years': years, 'mantles': data['mantles'],
              'residents': len(npcs), 'couples': len(couples), 'pools': {}}
    for d in DIALECTS:
        p = have.get(d, {'spare': 0, 'resting': 0, 'ready': 0, 'worn': 0, 'honesty': Counter()})
        next180 = births['next180'].get(d, 0.0)
        by_year = [y.get(d, 0.0) for y in births['years']]
        report['pools'][d] = {
            'spare': p['spare'], 'resting': p['resting'], 'ready': p['ready'], 'worn': p['worn'],
            'births180': next180, 'birthsByYear': by_year, 'births': round(sum(by_year), 1),
            'low': p['spare'] + p['ready'] < 1.5 * next180 + 2,
            'couples': sum(1 for c in couples if c['dialect'] == d),
            'honesty': mix(p['honesty']), 'livingHonesty': mix(living.get(d, Counter())),
        }
    return report


def show(report: dict) -> str:
    lines = [f"{report['world']}: day {report['day']:.1f}, {report['residents']} living residents, {report['couples']} couples of an "
             f"age for pups; mantles rest {report['restDays']:.0f} days" + ('' if report['mantles'] else ' (no live.mantles: before 0066)'),
             f"{'pool':<12} {'spare':>5} {'rest':>5} {'ready':>5} {'worn':>5} {'180d':>6} " +
             ' '.join(f'y{i + 1:<4}' for i in range(report['years'])) + f" {'all':>6}  health  honesty (pool / living, % sly+guarded)"]
    for d, p in report['pools'].items():
        sly = lambda m: round(m.get('sly', 0) + m.get('guarded', 0), 1) if m else '-'
        lines.append(f"{d:<12} {p['spare']:>5} {p['resting']:>5} {p['ready']:>5} {p['worn']:>5} {p['births180']:>6.2f} " +
                     ' '.join(f'{y:<5.1f}' for y in p['birthsByYear']) + f" {p['births']:>6.1f}  {'LOW' if p['low'] else 'ok':<6}  "
                     f"{sly(p['honesty'])} / {sly(p['livingHonesty'])}")
    total = sum(p['births'] for p in report['pools'].values())
    lines.append(f"{report['years']} years: about {total:.0f} births in all, as many as deaths (births one for one, realm-wide: Q5).")
    return '\n'.join(lines)


def check_names(folder: Path = NAMES) -> tuple[list[str], list[str], dict]:
    """(problems, warnings, counts) of the name pools, as Core/RatwHandoff.cpp's readNamePool reads them."""
    problems, warnings, counts = [], [], {}
    banned = re.compile(r'dog', re.I)
    seen = {}
    for path in sorted(folder.glob('*.names')):
        dialect, pool = None, {'female': [], 'male': []}
        for at, line in enumerate(path.read_text().splitlines(), 1):
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            if line.startswith('names '):
                dialect = line[6:].strip()
                continue
            key, _, value = line.partition('=')
            key, value = key.strip(), value.strip()
            if key == 'named_for':
                try:
                    if not 0 <= float(value) <= 1:
                        raise ValueError
                except ValueError:
                    problems.append(f'{path.name}:{at}: named_for is a share from 0 to 1')
                continue
            if key not in pool:
                problems.append(f'{path.name}:{at}: {key} is not female, male or named_for (male or female only)')
                continue
            for name in (n.strip() for n in value.split('|')):
                if not NAME.fullmatch(name):
                    problems.append(f'{path.name}:{at}: {name!r} is not one word, a capital and small letters')
                elif name in pool[key]:
                    problems.append(f'{path.name}:{at}: {name} twice')
                elif banned.search(name):
                    problems.append(f'{path.name}:{at}: {name}: a word the world never says')
                else:
                    pool[key].append(name)
        if dialect is None or dialect != path.stem:
            problems.append(f'{path.name}: its names line must name {path.stem}')
        for name in set(pool['female']) & set(pool['male']):
            problems.append(f'{path.name}: {name} is both a female and a male name')
        for sex in ('female', 'male'):
            if len(pool[sex]) < 60:
                warnings.append(f'{path.name}: {len(pool[sex])} {sex} names (about 70 wanted, §7.10)')
        counts[path.stem] = {sex: len(v) for sex, v in pool.items()}
        seen[path.stem] = pool
    for d in DIALECTS:
        if d not in seen:
            problems.append(f'{d}: no name pool (Data/Talk/names/{d}.names)')
    return problems, warnings, counts


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='what', required=True)
    f = sub.add_parser('forecast')
    f.add_argument('world')
    f.add_argument('--years', type=int, default=5)
    f.add_argument('--json', action='store_true')
    n = sub.add_parser('names')
    n.add_argument('--json', action='store_true')
    a = ap.parse_args(argv)
    if a.what == 'names':
        problems, warnings, counts = check_names()
        if a.json:
            print(json.dumps({'problems': problems, 'warnings': warnings, 'counts': counts}, indent=2))
        else:
            for d, c in counts.items():
                print(f"{d:<12} {c['female']:>3} female {c['male']:>3} male")
            print('\n'.join(problems + [f'warning: {w}' for w in warnings]) or 'the name pools read cleanly')
        return 1 if problems else 0
    sys.path.insert(0, str(HERE))
    import world_db as W
    key = W.resolve(a.world)
    with W.connect(key, 'editor') as conn:
        with conn.transaction():
            data = read_world(conn)
    report = forecast(data, a.years)
    print(json.dumps(report, indent=2) if a.json else show(report))
    return 0


if __name__ == '__main__':
    sys.exit(main())
