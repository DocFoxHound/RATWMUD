#!/usr/bin/env python3
"""NPC chronicles (Docs/Design/26-living-npcs.md, Phase 8): the story of someone's life, compiled from the event log.

Everything that happened to someone is already in `game.events` (Phase 2). A chronicle reads it and keeps what matters
to a life: arrivals, apprenticeships, positions taken and lost, marriages, mourning, deaths, crimes suffered and done,
promises made and broken. The daily round (trade, wages, conversations, comings and goings) is folded into one line a
season. Nothing is invented and nothing is stored: the log stays the one record, and a chronicle is a view of it.

    compile_chronicle(subject, rows, names)   pure: event rows in, a chronicle out (tested offline)
    load(conn, subject)                        reads one person's rows and the names they need from a world database
    milestones(chronicle, subject)             a few lines of "your life so far", for the NPC Mind
    story_lines(chronicle)                     the chronicle as plain lines, for writing a life story from

    python3 tools/chronicle.py dev wren        prints Wren's chronicle from DEV (read only)
    python3 tools/chronicle.py EVENTS [ID]     the same from `world_check ... --events EVENTS` (no ID: the fullest lives)
"""
from __future__ import annotations

import argparse
import re
import sys

SEASONS = (('Spring', 1), ('Summer', 93), ('Autumn', 185), ('Winter', 276))
DAYS_PER_YEAR = 365
LIFE, NOTABLE, ROUTINE = 3, 2, 1
# Kinds folded into the season's round rather than told one by one.
ROUTINE_KINDS = {'economy', 'conversation', 'arrival', 'departure'}
# Kinds about roads, camps and caravans that name no person: kept out of a person's chronicle.
IMPERSONAL = {'caravan departs', 'caravan arrives', 'caravan home', 'caravan passes', 'caravan lost', 'raid',
              'bandits gather', 'bandits scattered', 'patrol'}
# Ledger entries that move money and goods the same way (Society::shift); in a trade the goods go the other way.
TRANSFERS = {'caravan delivered', 'caravan loaded', 'caravan turned back', 'caravan unloaded', 'inheritance',
             'reward set aside', 'stores stocked', 'town tithe', 'town purse', 'church foundation', 'gather', 'settlement welcome grant', 'initial funding'}
# Ledger entries a crime or a road event already tells.
TOLD_ELSEWHERE = {'stolen', 'fine', 'restitution', 'robbed by bandits', 'paid to bandits'}
MAX_ROWS = 4000          # The latest events told one by one (the round is counted in the database, in full).
MAX_SEASONS = 40


# --------------------------------------------------------------------------- Dates and names

def season_of(day: float) -> tuple[int, str, int]:
    """(year, season, day of the season) for a calendar day, as the game's calendar counts them (RatwCalendar.cpp)."""
    absolute = max(0, int(day))
    year, of_year = absolute // DAYS_PER_YEAR + 1, absolute % DAYS_PER_YEAR + 1
    name, start = next((n, s) for n, s in reversed(SEASONS) if of_year >= s)
    return year, name, of_year - start + 1


# The week (RatwCalendar.h, Phase 9): seven days, Year 1 beginning on a Dawnday.
WEEKDAYS = ('Dawnday', 'Hearthday', 'Stoneday', 'Riverday', 'Emberday', 'Marketday', 'Restday')
MARKETDAY, RESTDAY = 5, 6
FESTIVAL_DAY = 46                                   # Of each season.


def weekday_name(day: float) -> str:
    return WEEKDAYS[max(0, int(day)) % 7]


def season_length(day: int) -> int:
    """How many days the season holding this day has (92, 92, 91, 90)."""
    _, name, _ = season_of(day)
    starts = [s for _, s in SEASONS] + [DAYS_PER_YEAR + 1]
    i = [n for n, _ in SEASONS].index(name)
    return starts[i + 1] - starts[i]


def date_label(day: float) -> str:
    year, season, of_season = season_of(day)
    return f'{season} {of_season}, Year {year}'


def season_label(day: float) -> str:
    year, season, _ = season_of(day)
    return f'{season}, Year {year}'


def plain_name(key: str) -> str:
    """A readable name for an ID nobody has named: 'camp_greyfen_road' -> 'Greyfen Road'."""
    text = re.sub(r'^(player-|npc_|camp_|road:|holder-)', '', key or '')
    text = re.sub(r'[_\-:]+', ' ', text).strip()
    return text.title() if text else 'someone'


class Voice:
    """How a line names people: by name, or, told to the subject themselves, as "you"."""

    def __init__(self, subject: str, names: dict[str, str], second_person: bool = False):
        self.subject, self.names, self.second = subject, names, second_person

    def who(self, key: str) -> str:
        if self.second and key == self.subject:
            return 'you'
        return self.names.get(key) or plain_name(key)

    def whose(self, key: str) -> str:
        return 'your' if self.second and key == self.subject else self.who(key) + "'s"

    def was(self, key: str) -> str:
        return 'were' if self.second and key == self.subject else 'was'


def pennies(n: int) -> str:
    return f"{n} penn{'y' if n == 1 else 'ies'}"


def sentence(text: str) -> str:
    text = text.strip()
    if not text:
        return text
    text = text[0].upper() + text[1:]
    return text if text.endswith(('.', '!', '?')) else text + '.'


# --------------------------------------------------------------------------- One event, told

def title_of(detail: str) -> str:
    return detail.split(':', 1)[0].strip() or 'their work'


def work(detail: str, titled: str, doing: str) -> str:
    """A post as the log names it: a title ("Baker") or what the work is ("keeping the moneychanger shop")."""
    title = title_of(detail)
    if not detail:
        return titled.format('their work')
    return titled.format(title) if title[:1].isupper() else doing.format(title)


def describe(row: dict, v: Voice) -> tuple[int, str] | None:
    """(importance, sentence) for one event about the subject, or None when it isn't theirs to tell."""
    kind, a, t, d = row['kind'], row['actor'], row['target'], row.get('detail', '')
    item, quantity, coins = row.get('item', ''), int(row.get('quantity') or 0), int(row.get('coins') or 0)
    A, T = v.who(a), v.who(t)
    took = pennies(coins) if coins else (goods_of(quantity, item) if item else 'something')
    lines = {
        'character created': (LIFE, f'{A} came into the world'),
        'spawn': (LIFE, f'{A} first appeared'),
        'death': (LIFE, f'{A} died'),
        'revival': (LIFE, f'{A} {v.was(a)} brought back to life'),
        'marriage': (LIFE, f'{A} married {T}'),
        'mourning': (LIFE, f'{A} mourned {T}, ' + ('one of the family' if d == 'family' else 'a friend')),
        'apprenticeship': (LIFE, f'{A} became apprentice to {T}' + work(d, ' ({})', ', {}')),
        'apprenticeship completed': (LIFE, f'{A} finished an apprenticeship with {T}' + work(d, ' ({})', ', {}')),
        'succession': (LIFE, f'{A} ' + work(d, 'took up the post of {}', 'took over {}') + (f', after {T}' if t else '')
                       + (f' ({d.split(":", 1)[1].strip()})' if ':' in d else '')),
        'vacancy': (NOTABLE, f'{A} ' + work(d, 'left the post of {}', 'gave up {}')),
        'returned to work': (NOTABLE, f'{A} ' + work(d, 'went back to work as {}', 'went back to {}')),
        'estate settled': (NOTABLE, f'{v.whose(a)} estate was settled' + (f', {d}' if d else '')
                           + (f'; {T} inherited' if t and t != 'treasury' else '')),
        'newcomer sent for': (NOTABLE, f'a newcomer was sent for to take {v.whose(t)} place as {d or "worker"}'),
        'relocation': (NOTABLE, f'{A} moved to a new home' + (f' in {plain_place(d)}' if d else '')),
        'cleared': (NOTABLE, f'{v.whose(a)} body was cleared away'),
        'promise': (NOTABLE, f'{A} promised {T}: "{d}"' if d else f'{A} made {T} a promise'),
        'promise broken': (NOTABLE, f'{A} broke a promise to {T}' + (f': "{d}"' if d else '')),
        'contract posted': (NOTABLE, f'{A} offered work' + (f' ({d})' if d else '') + (f' for {pennies(coins)}' if coins else '')),
        'contract taken': (NOTABLE, f'{A} took on work from {T}' + (f' ({d})' if d else '')),
        'theft': (NOTABLE, f'{A} stole {took} from {T}'),
        'attempted theft': (NOTABLE, f'{A} tried to steal from {T}'),
        'assault': (NOTABLE, f'{A} attacked {T}'),
        'beaten down': (NOTABLE, f'{A} beat {T} down'),
        'warrant': (NOTABLE, f'the watch wanted {A} for a crime against {T}'),
        'reported': (NOTABLE, f'{A} told the watch ({T}) of a {d.split(" (", 1)[0] or "crime"}'),
        'stopped by the watch': (NOTABLE, f'{A} of the watch stopped {T}' + (f' for {d}' if d else '')),
        'fine paid': (NOTABLE, f'{A} paid the watch {pennies(coins)}'),
        'arrest': (NOTABLE, f'{A} of the watch took {T} to the gaol'),
        'released': (NOTABLE, f'{A} {v.was(a)} let out of the gaol'),
        'bandits demand': (NOTABLE, f'bandits demanded {pennies(coins)} of {T} on the road'),
        'fight': (NOTABLE, f'bandits attacked {T} on the road'),
        'robbed': (NOTABLE, f'bandits beat and robbed {T} of {pennies(coins)}'),
        'paid off bandits': (NOTABLE, f'{A} paid bandits {pennies(coins)} to pass'),
        'bandit falls': (NOTABLE, f'{A} struck down a bandit' + (f', {d}' if d else '')),
        'bandits flee': (NOTABLE, f'bandits fled from {T}'),
        'camp cleared': (NOTABLE, f'{A} cleared a bandit camp'),
        # Deeds (doc 56): the detail is the deed's phrase, then its id in brackets.
        'deed': (NOTABLE, f'{A} {d.rsplit(" (", 1)[0] or "did a good turn"}'),
        'deed revoked': (NOTABLE, f'the Dungeon Master struck a deed ({d}) from the record'),
        'nickname': (NOTABLE, f'{T} first called {A} "{d.rsplit(" (", 1)[0]}"'),
        'nickname dropped': (NOTABLE, f'{A} asked folk not to call them "{d.rsplit(" (", 1)[0]}"'),
        'operator': (NOTABLE, f'the Dungeon Master acted on {T}' + (f' ({d})' if d else '')),
        'quarrel': (NOTABLE, f'{A} quarrelled with {T}'),
    }
    if kind.startswith('contract ') and kind not in lines:
        return NOTABLE, sentence(f'work for {T}' + (f' ({d})' if d else '') + f' was {kind[9:]}')
    found = lines.get(kind)
    if not found:
        return (NOTABLE, sentence(f'{kind}: {A}' + (f', {T}' if t else '') + (f' ({d})' if d else ''))) if kind else None
    return found[0], sentence(found[1])


def plain_place(detail: str) -> str:
    return detail.replace('new home in ', '').strip() or 'a new place'


# --------------------------------------------------------------------------- The round of a season

def fold_round(rows: list[dict], v: Voice) -> list[dict]:
    """One line a season for the routine: what they earned and spent, what changed hands, whom they talked with."""
    seasons: dict[tuple[int, str], dict] = {}
    for row in rows:
        year, name, _ = season_of(row['day'])
        s = seasons.setdefault((year, name), {'day': row['day'], 'earned': 0, 'spent': 0, 'talks': 0, 'people': set(),
                                              'visits': 0, 'wages': 0, 'goods': {}})
        s['day'] = min(s['day'], row['day'])
        count = int(row.get('count') or 1)
        kind = row['kind']
        if kind == 'conversation':
            s['talks'] += count
            other = row['target'] if row['actor'] == v.subject else row['actor']
            if other:
                s['people'].add(other)
        elif kind == 'arrival':
            s['visits'] += count
        elif kind == 'economy':
            detail = row.get('detail') or ''
            if detail in TOLD_ELSEWHERE or row['actor'] == row['target']:
                if detail == 'cook':
                    book(s, 'cooked', row)
                continue
            coins, item = int(row.get('coins') or 0), row.get('item') or ''
            paying = row['actor'] == v.subject              # Money always goes from the actor to the target.
            if coins:
                s['spent' if paying else 'earned'] += coins
                if not paying and 'wage' in detail:
                    s['wages'] += coins
            if item and int(row.get('quantity') or 0):
                trade = coins > 0 and detail not in TRANSFERS
                receiving = paying if trade else not paying
                verb = ('gathered' if detail == 'gather' else 'ate' if detail == 'eat' else
                        ('bought' if receiving else 'sold') if trade else ('received' if receiving else 'gave'))
                book(s, verb, row)
    out = []
    for (year, name), s in sorted(seasons.items(), key=lambda kv: kv[1]['day']):
        parts = []
        money = []
        if s['earned']:
            money.append(f"earned {pennies(s['earned'])}" + (f" ({pennies(s['wages'])} in wages)" if s['wages'] else ''))
        if s['spent']:
            money.append(f"spent {pennies(s['spent'])}")
        if money:
            parts.append(', '.join(money))
        for verb in ('bought', 'sold', 'gathered', 'cooked', 'ate', 'received', 'gave'):
            if goods := s['goods'].get(verb):
                parts.append(f'{verb} ' + ', '.join(goods_of(q, i) for i, q in sorted(goods.items(), key=lambda x: -x[1])[:4]))
        if s['talks']:
            people = sorted(v.who(p) for p in s['people'])
            parts.append(f"talked {s['talks']} time{'s' if s['talks'] != 1 else ''}"
                         + (f" with {', '.join(people[:4])}" + (f' and {len(people) - 4} more' if len(people) > 4 else '')
                            if people else ''))
        if s['visits']:
            parts.append(f"came into the world {s['visits']} time{'s' if s['visits'] != 1 else ''}")
        if parts:
            out.append({'day': s['day'], 'label': f'{name}, Year {year}', 'text': sentence('; '.join(parts))})
    return out[-MAX_SEASONS:]


GOODS = {'herbs': ('bundle of herbs', 'bundles of herbs')}


def goods_of(quantity: int, item: str) -> str:
    one, many = GOODS.get(item, (item, item if item.endswith('s') else item + 's'))
    return f"{quantity} {one if quantity == 1 else many}"


def book(season: dict, verb: str, row: dict) -> None:
    goods = season['goods'].setdefault(verb, {})
    goods[row['item']] = goods.get(row['item'], 0) + int(row.get('quantity') or 0)


# --------------------------------------------------------------------------- The whole chronicle

def compile_chronicle(subject: str, rows: list[dict], names: dict[str, str], places: dict[str, str] | None = None,
                      second_person: bool = False) -> dict:
    """Rows (oldest first) are {id, day, kind, actor, target, cell, item, quantity, coins, detail[, count]}; routine
    rows may come already counted (`count`). Returns the life events told one by one, and the round season by season."""
    v = Voice(subject, names, second_person)
    places = places or {}
    entries, routine, met = [], [], set()
    for row in rows:
        if subject not in (row.get('actor'), row.get('target')):
            continue
        kind = row.get('kind', '')
        if kind in IMPERSONAL:
            continue
        if kind in ROUTINE_KINDS:
            routine.append(row)
            if kind == 'conversation':
                other = row['target'] if row['actor'] == subject else row['actor']
                if other and other not in met:
                    met.add(other)
                    entries.append(entry(row, NOTABLE - 1, sentence(f'{v.who(subject)} first spoke with {v.who(other)}'),
                                         [other], places, kind='first meeting'))
            continue
        told = describe(row, v)
        if told:
            others = [k for k in (row.get('actor'), row.get('target')) if k and k != subject]
            entries.append(entry(row, told[0], told[1], others, places))
    days = [r['day'] for r in rows if subject in (r.get('actor'), r.get('target'))]
    return {'subject': subject, 'name': v.who(subject) if not second_person else names.get(subject, plain_name(subject)),
            'entries': entries, 'seasons': fold_round(routine, v),
            'firstDay': min(days) if days else None, 'lastDay': max(days) if days else None,
            'firstDate': date_label(min(days)) if days else '', 'lastDate': date_label(max(days)) if days else '',
            'events': sum(int(r.get('count') or 1) for r in rows if subject in (r.get('actor'), r.get('target'))),
            'lastEvent': max((int(r['id']) for r in rows if r.get('id') is not None), default=0)}


def entry(row: dict, importance: int, text: str, people: list[str], places: dict[str, str], kind: str = '') -> dict:
    cell = row.get('cell') or ''
    return {'id': row.get('id'), 'day': row['day'], 'date': date_label(row['day']), 'kind': kind or row['kind'],
            'importance': importance, 'text': text, 'people': people, 'cell': cell, 'place': places.get(cell, '')}


def milestones(chronicle: dict, limit: int = 8) -> str:
    """The life events of a chronicle compiled in the second person, newest last, as a few dated lines."""
    picked = [e for e in chronicle['entries'] if e['importance'] >= NOTABLE and e['kind'] != 'operator']
    lives = [e for e in picked if e['importance'] >= LIFE]
    recent = [e for e in picked if e['importance'] < LIFE][-max(2, limit - len(lives[-limit:])):]
    chosen = sorted({id(e): e for e in lives[-limit:] + recent}.values(), key=lambda e: (e['day'], e['id'] or 0))[-limit:]
    return '\n'.join(f"{e['date']}: {e['text']}" for e in chosen)


def season_end(day: float) -> float:
    """The last moment of the season a day falls in: a season's round is told after what happened in it."""
    year, name, _ = season_of(day)
    starts = [start for _, start in SEASONS] + [DAYS_PER_YEAR + 1]
    following = starts[[n for n, _ in SEASONS].index(name) + 1]
    return (year - 1) * DAYS_PER_YEAR + following - 1 - 1e-6


def story_lines(chronicle: dict, limit: int = 80) -> list[str]:
    """Everything a story may draw on: the life events and the seasons' round, in order, dated."""
    items = [(e['day'], f"{e['date']}: {e['text']}") for e in chronicle['entries'] if e['kind'] != 'operator']
    items += [(season_end(s['day']), f"{s['label']} (the round): {s['text']}") for s in chronicle['seasons']]
    return [text for _, text in sorted(items, key=lambda x: x[0])][-limit:]


# --------------------------------------------------------------------------- From the database

def world_of(conn) -> str | None:
    row = conn.execute('SELECT id FROM world.worlds ORDER BY id LIMIT 1').fetchone()
    return row[0] if row else None


def fetch_rows(conn, world: str, subject: str, limit: int = MAX_ROWS, routine: bool = True) -> list[dict]:
    """One person's events: the latest `limit` told one by one, and the whole routine counted a day at a time."""
    columns = ('id', 'day', 'kind', 'actor', 'target', 'cell', 'item', 'quantity', 'coins', 'detail')
    told = conn.execute(
        """SELECT id, game_day, kind, actor, target, cell, item, quantity, coins, detail FROM (
               SELECT * FROM game.events WHERE world_id = %(w)s AND actor = %(s)s AND kind <> ALL(%(r)s)
               UNION SELECT * FROM game.events WHERE world_id = %(w)s AND target = %(s)s AND kind <> ALL(%(r)s)
               ORDER BY id DESC LIMIT %(n)s) e ORDER BY id""",
        {'w': world, 's': subject, 'r': sorted(ROUTINE_KINDS), 'n': limit}).fetchall()
    rows = [dict(zip(columns, r)) for r in told]
    if not routine:
        return rows
    counted = conn.execute(
        """SELECT max(id), floor(game_day), kind, actor, target, item, sum(quantity), sum(coins), detail, count(*)
           FROM game.events WHERE world_id = %(w)s AND (actor = %(s)s OR target = %(s)s) AND kind = ANY(%(r)s)
           GROUP BY floor(game_day), kind, actor, target, item, detail""",
        {'w': world, 's': subject, 'r': sorted(ROUTINE_KINDS)}).fetchall()
    for r in counted:
        rows.append({'id': r[0], 'day': float(r[1]), 'kind': r[2], 'actor': r[3], 'target': r[4], 'cell': '',
                     'item': r[5], 'quantity': int(r[6] or 0), 'coins': int(r[7] or 0), 'detail': r[8], 'count': int(r[9])})
    rows.sort(key=lambda r: (r['day'], r['id'] or 0))
    return rows


def fetch_names(conn, world: str, keys: set[str]) -> dict[str, str]:
    """Names for these IDs: NPC bodies and player characters as saved, then the NPCs and job holders as authored."""
    keys = sorted(k for k in keys if k)
    names: dict[str, str] = {}
    if not keys:
        return names
    for sql in ('SELECT key, name FROM game.characters WHERE world_id = %s AND key = ANY(%s)',
                'SELECT key, name FROM game.npcs WHERE world_id = %s AND key = ANY(%s)',
                'SELECT id, name FROM live.npcs WHERE world_id = %s AND id = ANY(%s)'):
        try:
            for key, name in conn.execute(sql, (world, keys)).fetchall():
                if name and key not in names:
                    names[key] = name
        except Exception:                        # A table this database doesn't have (or can't show this role).
            if not conn.autocommit:
                raise
    missing = [k for k in keys if k not in names]
    if missing:
        try:
            for key, name in conn.execute('SELECT id, name FROM live.characters WHERE id = ANY(%s)', (missing,)).fetchall():
                names.setdefault(key, name)
        except Exception:
            if not conn.autocommit:
                raise
    return names


def fetch_places(conn, world: str, cells: set[str]) -> dict[str, str]:
    cells = sorted(c for c in cells if c)
    if not cells:
        return {}
    return dict(conn.execute('''SELECT id, name FROM world.cells WHERE world_id = %(w)s AND id = ANY(%(c)s)
                                UNION ALL SELECT id, name FROM world.interiors WHERE world_id = %(w)s AND id = ANY(%(c)s)''',
                             {'w': world, 'c': cells}).fetchall())


def load(conn, subject: str, second_person: bool = False, limit: int = MAX_ROWS, routine: bool = True) -> dict:
    """A person's chronicle from a world database (read only; any role that may read game.events). Without the
    routine (`routine=False`), only the life events: quicker, and all the NPC Mind needs."""
    world = world_of(conn)
    if not world:
        return compile_chronicle(subject, [], {subject: plain_name(subject)})
    rows = fetch_rows(conn, world, subject, limit, routine)
    keys = {subject} | {r['actor'] for r in rows} | {r['target'] for r in rows}
    names = fetch_names(conn, world, keys)
    places = fetch_places(conn, world, {r['cell'] for r in rows})
    return compile_chronicle(subject, rows, names, places, second_person)


def from_export(path: str) -> tuple[list[dict], dict[str, str]]:
    """Events and names from `world_check ... --events FILE`, as rows compile_chronicle takes."""
    import json
    data = json.loads(open(path, encoding='utf-8').read())
    rows = [{'id': i + 1, 'day': float(e.get('day', 0)), 'kind': e.get('kind', ''), 'actor': e.get('actor', ''),
             'target': e.get('target', ''), 'cell': e.get('cell', ''), 'item': e.get('item', ''),
             'quantity': int(e.get('quantity', 0)), 'coins': int(e.get('coins', 0)), 'detail': e.get('detail', '')}
            for i, e in enumerate(data['events'])]
    return rows, data.get('names', {})


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('source', help="dev or prod (a world database), or a world_check --events file")
    parser.add_argument('subject', nargs='?', help="an NPC's or a character's ID (without it: the fullest lives)")
    parser.add_argument('--top', type=int, default=10, help='how many of the fullest lives to list')
    args = parser.parse_args()
    if args.source in ('dev', 'prod'):
        if not args.subject:
            parser.error('name someone to read from a database')
        import world_db
        with world_db.connect(args.source, 'dm') as conn:
            chronicle = load(conn, args.subject)
    else:
        rows, names = from_export(args.source)
        if not args.subject:
            lives: dict[str, int] = {}
            for r in rows:
                if r['kind'] not in ROUTINE_KINDS and r['kind'] not in IMPERSONAL:
                    for key in {r['actor'], r['target']} - {''}:
                        lives[key] = lives.get(key, 0) + 1
            for key, n in sorted(lives.items(), key=lambda kv: -kv[1])[:args.top]:
                print(f'{n:5}  {key}  {names.get(key, "")}')
            return 0
        chronicle = compile_chronicle(args.subject, rows, names)
    print(f"{chronicle['name']}: {chronicle['events']} events, {chronicle['firstDate']} to {chronicle['lastDate']}")
    for line in story_lines(chronicle, limit=10_000):
        print('  ' + line)
    return 0


if __name__ == '__main__':
    sys.exit(main())
