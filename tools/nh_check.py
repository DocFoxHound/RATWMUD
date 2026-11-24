"""The Ninth Howl content checks (Docs/Design/86 9.3; artifacts/quests/design/decisions.md 1.2.7, 3 and P2-22, P2-24).

  python3 tools/nh_check.py FILE...            check these story programs (.json) and talk files (.talk/.tree/.grammar,
                                               tools/dialect_allow.json)
  python3 tools/nh_check.py --all              every program under Data/Storylines/ridgemere3, and the talk checks
  python3 tools/nh_check.py --talk             the talk checks alone
  options: --world KEY (default ridgemere3), --no-db (skip the world's checks), --json

For each story program:
  1. story_program.validate, problems(p) == [] and review(p): passed, no phase and no trigger uncovered;
  2. story_runner.references(conn, <world>, p) on the world, read only (the dm role, a READ ONLY transaction, rolled back);
  3. every resident it names placed, alive and reachable (decisions.md 1's query), each giver's dialect shown;
  4. the giver rule (givers.tsv): a pack quest's giver and npc.offer holders are that quest's registered residents; a
     campaign program's are campaign cast or Keepers; never an innkeeper or a look-alike; the registry's own check (1.2.7);
  5. every purse it pays from or watches is an account in the latest checkpoint's society.accounts, never
     town:<id>:works|watch|church; every character.grant follows the counter rule (decisions 3.2: inside an If on
     coin_<purse>_ok and coin_<purse>_day, beside an Add to coin_<purse>_day; never a quest reward or an entry effect);
  6. the words: no forbidden word (the d-word, gods, "tells" for private messages, ...), no ids or keys in what is said;
     no npc.speak, no story.tell carrying a resident (from), no story wave matched on camp_w_<...>;
  7. its world variables are in world-vars.json with the same type, and carry a content prefix (nh_ sq_ mq_ lq_ st_ coin_);
  8. it lives under Data/Storylines/ridgemere3/, never Data/Storylines/programs/;
  9. the doors (doc 86, "Doors a story unlocks"): a door.unlock or door.open never on a door into anyone's home (a
     resident's or a post's home_area, the room itself or one its stairs and passages reach); a door into a room
     nobody works in is warned of, and so is door.lock (the map locks no door, so it locks the residents out too, until
     the run ends; door.close ends a window instead).
For talk files (or --talk): dialect_check --world <world>; talk_check --validate (the files given, else the story talk
files) and --render-trees; talk_lint.

Prints PASS or FAIL per file (with each error), and exits 1 if anything failed. Nothing is written anywhere.
"""

import argparse
import csv
import collections
import json
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
import nh_world_vars  # noqa: E402
import story_program  # noqa: E402
import story_runner  # noqa: E402
import world_db  # noqa: E402

STORIES = ROOT / 'Data' / 'Storylines' / 'ridgemere3'
GIVERS = ROOT / 'artifacts' / 'quests' / 'design' / 'givers.tsv'
WORLD_VARS = STORIES / 'ninth-howl' / 'world-vars.json'
TALK = ROOT / 'Data' / 'Talk'
TALK_CHECK = ROOT / 'build-core' / 'talk_check'
PREFIXES = ('nh_', 'sq_', 'mq_', 'lq_', 'st_', 'coin_')
PACK_TIERS = ('small', 'medium', 'large', 'stage')
# Wave J: each folder's programs and their prefix (D2: camps, foes and ids begin with it).
FOLDER_PREFIX = {'packs': {'small': 'sq', 'medium': 'mq'}, 'quests-large': 'lq', 'quests-stage': 'st',
                 'ninth-howl': {'nh-purses': 'coin', 'ninth-howl.': 'nh'}}
TALK_GATE = re.compile(r'\b(world|self|pair)\.mem\.([a-z0-9_]+)(!?=)([^\s\]]+)')
SCOPE_NS = {'realm': 'world', 'town': 'world', 'npc': 'self', 'pair': 'pair'}

# decisions.md 1: placed, alive, reachable.
REACH_SQL = """
with reach as (
  select a_area as area from world.links where world_id=%(w)s
  union select b_area from world.links where world_id=%(w)s
  union select id from world.cells where world_id=%(w)s)
select n.id, n.name, n.age, s.alive,
       n.home_area in (select area from reach) as home_ok,
       (n.work_area is null or n.work_area = '' or n.work_area in (select area from reach)) as work_ok,
       v.home_region, v.accent
from live.npcs n
left join live.npc_state s on s.world_id=n.world_id and s.npc_id=n.id
left join live.npc_voice v on v.world_id=n.world_id and v.npc_id=n.id
where n.world_id=%(w)s and n.id = any(%(ids)s)"""

# The words (doc 86 6; campaign 2.10): errors, then warnings.
FORBIDDEN = [
    (re.compile(r'\bdog(s|gy|gie|gies)?\b', re.I), 'wolves, never the d-word (pups for the young)'),
    (re.compile(r'\b(gods?|goddess(es)?|deit(y|ies))\b', re.I), 'only the Bright One and the Dark One'),
    (re.compile(r'\b(a|the|your|my|his|her|their|private|send|sent|sending|got)\s+tells?\b', re.I), '"private messages", never "tells"'),
    (re.compile(r'\b(non-?binary|Mx\.?)\b'), 'male or female only'),
]
WARNED = [
    (re.compile(r'\bpupp(y|ies)\b', re.I), '"pups" for young wolves'),
    (re.compile(r'\blongest night\b', re.I), 'decisions 6.3: the Moot is not "the longest night"'),
    (re.compile(r'\bMidwinter\b'), 'decisions 6.4: "Midwinter" names the Moot only under nh_moot=midwinter'),
]
# What a wolf reads or a resident says (an effect's prose, a quest's words, a journal line).
PROSE = {'say', 'thanks', 'accept', 'decline', 'handIn', 'ask', 'text', 'journal', 'reply', 'words', 'label', 'summary',
         'title', 'detail', 'reason', 'line', 'during', 'greet', 'notice'}
RESIDENT_KEYS = {'npc', 'giver', 'from', 'like', 'resident'}
RESIDENT_TARGET_EVENTS = {'conversation', 'gift', 'gift again', 'trade done', 'near resident', 'dialogue opened',
                          'dialogue node', 'dialogue closed', 'dialogue choice', 'death', 'old age'}
SYMBOL = re.compile(r'#[^#\s]+#')
KEYLIKE = re.compile(r'\b[a-z][a-z0-9]*_[a-z0-9_]+\b')


class Report:
    def __init__(self, name):
        self.name, self.errors, self.warnings, self.info = name, [], [], []

    def error(self, text):
        self.errors.append(text)

    def warn(self, text):
        self.warnings.append(text)

    def note(self, text):
        self.info.append(text)

    def passed(self):
        return not self.errors

    def show(self, verbose=True):
        out = [f'{"PASS" if self.passed() else "FAIL"}  {self.name}']
        out += [f'    error: {e}' for e in self.errors]
        out += [f'    warning: {w}' for w in self.warnings]
        if verbose:
            out += [f'    {i}' for i in self.info]
        return '\n'.join(out)

    def json(self):
        return {'file': self.name, 'passed': self.passed(), 'errors': self.errors, 'warnings': self.warnings, 'info': self.info}


# ----------------------------------------------------------------------------------------------- walking a program

def walk(value, path=()):
    """Every (path, key, value) of a JSON document, leaves only."""
    if isinstance(value, dict):
        for k, v in value.items():
            yield from walk(v, path + (k,))
    elif isinstance(value, list):
        for i, v in enumerate(value):
            yield from walk(v, path + (i,))
    else:
        yield path, (path[-1] if path else None), value


def effects_in(p):
    """Every effect dict of a program with where it sits: ('entry'|'trigger'|'reward', path, effect, ancestors)."""
    out = []

    def actions(lst, path, conds):
        for i, a in enumerate(lst or []):
            if not isinstance(a, dict):
                continue
            here = path + (i,)
            if a.get('do') == 'effect' and isinstance(a.get('effect'), dict):
                out.append(('trigger', here, a['effect'], conds, lst))
            if a.get('do') == 'if':
                actions(a.get('then'), here + ('then',), conds + [a.get('if')])
                for j, e in enumerate(a.get('elif') or []):
                    actions(e.get('then'), here + ('elif', j), conds + [e.get('if')])
                actions(a.get('else'), here + ('else',), conds + [{'not': a.get('if')}])
            for k in ('actions',):
                if isinstance(a.get(k), list):
                    actions(a[k], here + (k,), conds)
            if a.get('do') == 'random':
                for j, c in enumerate(a.get('choices') or []):
                    actions(c.get('actions'), here + ('choices', j), conds)

    def triggers(ts, path):
        for i, t in enumerate(ts or []):
            actions(t.get('actions'), path + (i, 'actions'), [t['if']] if t.get('if') else [])

    triggers(p.get('triggers'), ('triggers',))
    for pi, ph in enumerate(p.get('phases', [])):
        for ei, e in enumerate(ph.get('effects', [])):
            out.append(('entry', ('phases', pi, 'effects', ei), e, [], None))
        triggers(ph.get('triggers'), ('phases', pi, 'triggers'))
    for qi, q in enumerate(p.get('quests', [])):
        for key in ('rewards', 'bonus'):
            for ri, e in enumerate(q.get(key, [])):
                out.append(('reward', ('quests', qi, key, ri), e, [], None))
    return out


def all_triggers(p):
    yield from p.get('triggers', [])
    for ph in p.get('phases', []):
        yield from ph.get('triggers', [])


def where(path):
    return '.'.join(str(x) for x in path)


def mentions(cond, name):
    return name in json.dumps(cond)


# ------------------------------------------------------------------------------------------------------ the checks

DOOR_EFFECTS = ('door.unlock', 'door.open')


class DoorMap:
    """The world's doors, rooms, stairs and homes, read once (read only)."""

    def __init__(self, conn, world):
        rooms = conn.execute('SELECT id, name, outdoors FROM world.cells WHERE world_id=%s UNION ALL '
                             'SELECT id, name, outdoors FROM world.interiors WHERE world_id=%s', (world, world)).fetchall()
        self.name = {r[0]: r[1] for r in rooms}
        self.outdoors = {r[0]: bool(r[2]) for r in rooms}
        self.doors = {}
        self.inner = collections.defaultdict(set)
        for lid, name, kind, a, b in conn.execute('SELECT id, name, kind, a_area, b_area FROM world.links WHERE world_id=%s', (world,)):
            if kind == 'door':
                for key in ('link_' + lid, 'link_' + lid + '_a', 'link_' + lid + '_b'):
                    self.doors[key] = (name, a, b)
            elif not self.outdoors.get(a, True) and not self.outdoors.get(b, True):
                # (Stairs and passages inside are never locked to wolves: a room is open with every room they reach.)
                self.inner[a].add(b)
                self.inner[b].add(a)
        self.homes = collections.defaultdict(list)
        for name, home in conn.execute('SELECT name, home_area FROM live.npcs WHERE world_id=%s', (world,)):
            self.homes[home].append(name)
        for label, home in conn.execute('SELECT work_label, home_area FROM live.profession_slots WHERE world_id=%s', (world,)):
            self.homes[home].append(f'a post ({label})')
        self.workers = collections.defaultdict(list)
        for name, work in conn.execute('SELECT name, work_area FROM live.npcs WHERE world_id=%s', (world,)):
            self.workers[work].append(name)

    def reached(self, room):
        seen, todo = {room}, [room]
        while todo:
            for y in self.inner[todo.pop()]:
                if y not in seen:
                    seen.add(y)
                    todo.append(y)
        return seen


def check_doors(p, r, doors):
    """Doc 86 (the DECISION of 2026-11-22): a door a live story unlocks is open to wolves while the story holds it, so a
    story never unlocks or opens a door into anyone's home."""
    found = []
    for _, path, e, _, _ in effects_in(p):
        if isinstance(e, dict) and str(e.get('kind', '')).startswith('door.'):
            found.append((where(path), e))
    # (And any other effect list a program carries, a scene's or a quest's: every dict with a door and a door kind.)
    def scan(v, path):
        if isinstance(v, dict):
            if str(v.get('kind', '')).startswith('door.') and 'door' in v and not any(v is f[1] for f in found):
                found.append((where(path), v))
            for k, x in v.items():
                scan(x, path + (k,))
        elif isinstance(v, list):
            for i, x in enumerate(v):
                scan(x, path + (i,))
    scan({k: x for k, x in p.items() if k != 'tests'}, ())
    for at, e in found:
        kind, door = e.get('kind'), e.get('door', '')
        if kind == 'door.lock':
            r.warn(f'{at}: door.lock {door}: the map locks no door, so this locks the residents out too, until the run ends '
                   '(door.close ends a window and leaves the residents their door)')
        if kind not in DOOR_EFFECTS:
            continue
        if door not in doors.doors:
            r.error(f'{at}: {kind} {door}: no such door in this world')
            continue
        name, a, b = doors.doors[door]
        inside = [x for x in (a, b) if not doors.outdoors.get(x, True)]
        homes, workers = [], []
        for room in inside:
            for x in sorted(doors.reached(room)):
                if doors.homes.get(x):
                    how = 'is' if x == room else f'reaches {x} ({doors.name.get(x, x)}) by its stairs and passages, which is'
                    who = doors.homes[x]
                    homes.append(f'{room} {how} the home of {", ".join(who[:3])}' + (f' and {len(who) - 3} more' if len(who) > 3 else ''))
                workers += doors.workers.get(x, [])
        if homes:
            r.error(f'{at}: {kind} {door} ("{name}"): a story never opens a home to wolves: ' + '; '.join(homes))
        elif inside and not workers:
            r.warn(f'{at}: {kind} {door} ("{name}"): nobody works behind it (a story opens a workplace, never a home)')
        else:
            r.note(f'doors: {kind} {door} ("{name}"), a workplace of {len(set(workers))}, no home behind it')


def check_engine(p, r):
    try:
        story_program.validate(p)
    except Exception as error:   # noqa: BLE001 (any refusal is the finding)
        r.error(f'validate: {error}')
        return False
    probs = story_program.problems(p)
    for x in probs:
        (r.error if x['level'] == 'error' else r.warn)(f'problems: {x["text"]} ({x["path"] or "program"})')
    if probs:
        r.error(f'problems(p) is not empty ({len(probs)})')
    rev = story_program.review(p)
    if not rev['passed']:
        for t in rev['tests']:
            if not t['passed']:
                r.error(f'rehearsal "{t["name"]}" fails: ' + json.dumps({k: t[k] for k in t if k not in ('trace', 'effects', 'triggers')})[:600])
    if rev['uncovered']:
        r.error('phases no rehearsal reaches: ' + ', '.join(rev['uncovered']))
    if rev['uncoveredTriggers']:
        r.error('triggers no rehearsal fires: ' + ', '.join(rev['uncoveredTriggers']))
    r.note(f'review: {len(rev["tests"])} rehearsal(s) passed={rev["passed"]}; digest {rev["digest"][:12]}')
    return True


def check_place(path, r):
    try:
        rel = path.resolve().relative_to(ROOT)
    except ValueError:
        return
    if rel.parts[:3] == ('Data', 'Storylines', 'programs'):
        r.error('lives in Data/Storylines/programs/ (three tests count the files there): use Data/Storylines/ridgemere3/')
    elif rel.parts[:3] != ('Data', 'Storylines', 'ridgemere3'):
        r.warn(f'not under Data/Storylines/ridgemere3/ ({rel})')


def residents_named(p, known):
    """(keyed, anywhere): resident ids under a resident key or an event's target, and any string that is a resident id."""
    keyed, anywhere = set(), set()
    for path, key, v in walk(p):
        if not isinstance(v, str):
            continue
        if 'tests' in path:
            continue
        if key in RESIDENT_KEYS and v not in ('$actor', '$each', 'world', ''):
            # ("from" is also a Remember action's event field and a Days since condition's start: not a resident.)
            if not (key == 'from' and v in ('actor', 'target', 'cell', 'item', 'detail', 'each', 'var', 'phase', 'run')):
                keyed.add(v)
        if v in known:
            anywhere.add(v)
    for t in all_triggers(p):
        if t.get('on') in RESIDENT_TARGET_EVENTS and t.get('match', {}).get('target'):
            keyed.add(t['match']['target'])
    for q in p.get('quests', []):
        for o in q.get('objectives', []):
            if o.get('on') in RESIDENT_TARGET_EVENTS and o.get('match', {}).get('target'):
                keyed.add(o['match']['target'])
        for f in q.get('fail', []):
            if f.get('on') in RESIDENT_TARGET_EVENTS and f.get('match', {}).get('actor'):
                keyed.add(f['match']['actor'])
    return keyed, anywhere


def check_residents(p, r, conn, world):
    known = {row[0] for row in conn.execute('SELECT id FROM live.npcs WHERE world_id=%s', (world,))}
    keyed, anywhere = residents_named(p, known)
    for x in sorted(keyed - known):
        # (A wolf or a stranger may stand where a resident does: only an unknown id under a resident key is an error.)
        if not conn.execute('SELECT 1 FROM game.characters WHERE world_id=%s AND key=%s', (world, x)).fetchone():
            r.error(f'unknown resident {x}')
    ids = sorted((keyed | anywhere) & known)
    rows = conn.execute(REACH_SQL, {'w': world, 'ids': ids}).fetchall()
    voices = {}
    for rid, name, age, alive, home_ok, work_ok, region, accent in rows:
        voices[rid] = (name, age, region, accent)
        if alive is not True:
            r.error(f'{rid} ({name}) is not alive')
        if not home_ok:
            r.error(f'{rid} ({name}) has no reachable home')
        if not work_ok:
            r.error(f'{rid} ({name}) has no reachable work place')
    if ids:
        r.note(f'residents: {len(ids)} named, all checked placed/alive/reachable')
    return voices


def givers_registry(path=GIVERS):
    rows = collections.defaultdict(list)
    with open(path, newline='') as f:
        for x in csv.DictReader(f, delimiter='\t'):
            rows[x['resident_id']].append(x)
    return rows


def registry_problems(rows):
    """decisions.md 1.2.7: one pack quest per resident, and none on campaign cast, a Keeper, an innkeeper or a look-alike."""
    out = []
    for k, v in rows.items():
        roles = [y['role_in_content'] for y in v]
        packs = [y for y in roles if y.split(':')[0] in PACK_TIERS]
        if len(set(packs)) > 1:
            out.append(f'{k} has {len(set(packs))} pack quests: {sorted(set(packs))}')
        if packs and any(y.startswith(('campaign cast', 'keeper', 'innkeeper', 'look-alike')) for y in roles):
            out.append(f'{k} carries a pack quest beside {roles}')
    return out


def hub_holders(p):
    """(resident, what): each quest's giver and every npc.offer's resident."""
    out = []
    for q in p.get('quests', []):
        if q.get('giver'):
            out.append((q['giver'], f'giver of {q["id"]}', q['id']))
    for _, path, e, _, _ in effects_in(p):
        if e.get('kind') == 'npc.offer':
            out.append((e.get('npc'), f'npc.offer {e.get("id", "")} at {where(path)}', None))
    return out


def check_givers(p, r, rows):
    pid = p['id']
    campaign = pid.startswith('ninth-howl')
    for x in registry_problems(rows):
        r.error(f'givers.tsv: {x}')
    quest_ids = {q['id'] for q in p.get('quests', [])}
    for npc, what, quest in hub_holders(p):
        mine = rows.get(npc, [])
        roles = [y['role_in_content'] for y in mine]
        if any(y.startswith(('innkeeper', 'look-alike')) for y in roles):
            r.error(f'{npc} ({what}) is an innkeeper or a look-alike: they give nothing (decisions 1.2.4, 1.6)')
            continue
        if campaign:
            if not any(y.startswith(('campaign cast', 'keeper')) for y in roles):
                r.error(f'{npc} ({what}) is not campaign cast or a Keeper in givers.tsv')
            continue
        packs = [y for y in roles if y.split(':')[0] in PACK_TIERS]
        names = {y.split(':', 1)[1] for y in packs if ':' in y}
        if not packs and any(y.startswith(('campaign cast', 'keeper')) for y in roles):
            r.error(f'{npc} ({what}) is campaign cast or a Keeper: they carry no pack quests (decisions 1.2.2)')
        elif not packs:
            r.error(f'{npc} ({what}) has no pack row in givers.tsv (decisions 1.2)')
        elif not (names & ({pid} | ({quest} if quest else quest_ids))):
            r.error(f'{npc} ({what}) is registered for {sorted(names)}, not for this quest or program')


def purse_account(purse):
    """The account a story's purse names (Game::storyPurseAccount): town:<first town> the treasury, town:<id> its store,
    anything else by its own name."""
    if purse.startswith('town:') and purse.count(':') == 1:
        return 'treasury' if purse == 'town:ridgemere' else 'stores:' + purse[5:]
    return purse


def latest_accounts(conn, world):
    row = conn.execute('SELECT payload::text, saved_at FROM game.checkpoints WHERE world_id=%s ORDER BY saved_at DESC LIMIT 1',
                       (world,)).fetchone()
    if not row:
        return {}, None
    doc = json.loads(row[0])
    return doc.get('society', {}).get('accounts', {}), row[1]


def counter_name(purse):
    if purse.startswith('town:') and purse.count(':') == 1:
        return purse[5:]
    if purse.startswith('till:job:'):
        return 'till_' + purse[len('till:job:'):].removeprefix('rm_').removeprefix('rmx_')
    if purse.startswith('till:'):
        return 'till_' + purse[5:].removeprefix('rm_').removeprefix('rmx_')
    return re.sub(r'[^a-z0-9]+', '_', purse.lower())


def check_purses(p, r, accounts=None, saved=None):
    named = []
    for kind, path, e, conds, siblings in effects_in(p):
        if e.get('kind') != 'character.grant':
            continue
        purse = e.get('purse', '')
        named.append((purse, where(path)))
        n = counter_name(purse)
        day, ok = f'coin_{n}_day', f'coin_{n}_ok'
        if kind != 'trigger':
            r.error(f'{where(path)}: a coin grant as a {kind} effect cannot follow the counter rule (decisions 3.2): pay it '
                    'from a trigger, inside an If on ' + f'{ok} and {day}')
            continue
        if not any(mentions(c, ok) for c in conds) or not any(mentions(c, day) for c in conds):
            r.error(f'{where(path)}: the grant from {purse} is not inside an If on {ok} and {day} (decisions 3.2.1)')
        if not any(isinstance(a, dict) and a.get('do') == 'add' and a.get('var') == day for a in siblings or []):
            r.error(f'{where(path)}: the grant from {purse} has no Add to {day} beside it (decisions 3.2.1)')
        if e.get('ifRefused') != 'skip':
            r.error(f'{where(path)}: a story coin grant is ifRefused "skip" (decisions 3.2.1)')
    for t in all_triggers(p):
        if t.get('on') == 'purse crossed' and t.get('match', {}).get('target'):
            named.append((t['match']['target'], f'trigger {t["id"]}'))
    seen = set()
    for purse, at in named:
        if re.fullmatch(r'town:[a-z0-9_]+:(works|watch|church)', purse):
            r.error(f'{at}: {purse} does not exist in this world (decisions 3.1)')
            continue
        if accounts is None:
            continue
        acc = purse_account(purse)
        if acc not in accounts:
            r.error(f'{at}: {purse} (account {acc}) is not in the latest checkpoint\'s society.accounts')
        elif purse not in seen:
            seen.add(purse)
            cash = accounts[acc].get('cash') if isinstance(accounts[acc], dict) else None
            r.note(f'purse {purse} = {acc}: {cash} pennies at {saved:%Y-%m-%d %H:%M} UTC')
    if accounts is not None and not named:
        r.note('purses: none named')


def check_words(p, r):
    in_tests = lambda path: path and path[0] == 'tests'   # noqa: E731
    for path, key, v in walk(p):
        if not isinstance(v, str) or in_tests(path):
            continue
        for rx, why in FORBIDDEN:
            if rx.search(v):
                r.error(f'{where(path)}: "{rx.search(v).group(0)}": {why}')
        if 'match' in path or key in ('note',) or 'variables' in path:
            continue
        if key in PROSE:
            for rx, why in WARNED:
                if rx.search(v):
                    r.warn(f'{where(path)}: "{rx.search(v).group(0)}": {why}')
            spoken = SYMBOL.sub('', v)
            ids = KEYLIKE.findall(spoken)
            if ids:
                r.error(f'{where(path)}: ids or keys in what is read or said: {", ".join(sorted(set(ids)))}')
    for _, path, e, _, _ in effects_in(p):
        if e.get('kind') == 'npc.speak':
            r.error(f'{where(path)}: npc.speak (NPC speech goes in the dialogue window only)')
        if e.get('kind') == 'story.tell' and e.get('from'):
            r.error(f'{where(path)}: story.tell from {e["from"]}: a banner with a face is not the dialogue window (P2-22)')
    for path, key, v in walk(p):
        if isinstance(v, str) and 'match' in path and v.startswith('camp_w_'):
            r.error(f'{where(path)}: a story wave matched on {v} (decisions 4.1: cell plus a spawned_ switch)')


def check_variables(p, r, declarations):
    by = {d['name']: d for d in declarations['variables']} if declarations else {}
    for v in p.get('variables', []):
        if v.get('scope') == 'world':
            if not v['id'].startswith(PREFIXES):
                r.error(f'world variable {v["id"]} has no content prefix ({", ".join(PREFIXES)})')
            d = by.get(v['id'])
            if declarations is None:
                continue
            if d is None:
                r.error(f'world variable {v["id"]} is not in {WORLD_VARS.relative_to(ROOT)}')
            elif d['type'] != v['type']:
                r.error(f'world variable {v["id"]} is a {v["type"]} here, a {d["type"]} in world-vars.json')
            elif 'initial' in v and v['initial'] != d['value']:
                r.warn(f'world variable {v["id"]} starts at {v["initial"]!r} here, {d["value"]!r} in world-vars.json')


def talk_reads(talk=TALK):
    """(namespace, key) -> the values the talk library's lines test (a | b both; '*' for a != test, any value)."""
    out = collections.defaultdict(set)
    for f in list((talk / 'dialogue').glob('*.tree')) + list(talk.rglob('*.talk')):
        for m in TALK_GATE.finditer(f.read_text()):
            out[(m.group(1), m.group(2))].update(['*'] if m.group(3) == '!=' else m.group(4).split('|'))
    return out


def expected_prefix(path):
    rel = path.resolve().relative_to(STORIES.resolve()) if path.resolve().is_relative_to(STORIES.resolve()) else None
    if rel is None or len(rel.parts) < 2:
        return None
    want = FOLDER_PREFIX.get(rel.parts[0])
    if isinstance(want, dict):
        return next((v for k, v in want.items() if k in rel.name), None)
    return want


def talk_writes(p):
    """(namespace, key, value, where) for every talk.remember in the program."""
    out = []
    for kind, path, e, conds, siblings in effects_in(p):
        if e.get('kind') == 'talk.remember':
            out.append((SCOPE_NS.get(e.get('scope'), 'world'), e.get('key'), e.get('value'), where(path)))
    for qi, q in enumerate(p.get('quests', []) or []):
        for k in ('rewards', 'bonus'):
            for ri, e in enumerate(q.get(k, []) or []):
                if isinstance(e, dict) and e.get('kind') == 'talk.remember':
                    out.append((SCOPE_NS.get(e.get('scope'), 'world'), e.get('key'), e.get('value'), f'quests[{qi}].{k}[{ri}]'))
    return out


def check_wave_j(path, p, r, reads):
    """Doc 86, wave J: the prefix, the purse guard (M2) and the talk keys (C8)."""
    want = expected_prefix(path)
    if want and p.get('prefix') != want:
        r.error(f'the program declares no prefix "{want}" (its folder\'s; doc 86 D2, wave J)')
    for kind, ep, e, conds, siblings in effects_in(p):
        if e.get('kind') == 'character.grant' and kind == 'trigger':
            if not any(mentions(c, json.dumps({'purse': e.get('purse', '')})) for c in conds):
                r.error(f'{where(ep)}: the grant from {e.get("purse")} does not read the purse itself (M2: {{"purse": ...}} at '
                        'least 60 and the amount, beside the floor switch)')
    if reads is None:
        return
    for ns, key, value, at in talk_writes(p):
        got = reads.get((ns, key))
        if not got:
            r.warn(f'{at}: nothing in Data/Talk reads {ns}.mem.{key}: the memory says nothing')
        elif value not in got and '*' not in got:
            r.warn(f'{at}: Data/Talk reads {ns}.mem.{key} as {"|".join(sorted(got))}, never {value}')
    n = len(talk_writes(p))
    if n:
        r.note(f'talk: {n} memories written, each read by the library')


def check_program(path, conn, world, rows, declarations, accounts, saved, doors=None, reads=None):
    r = Report(str(path.resolve().relative_to(ROOT)) if path.resolve().is_relative_to(ROOT) else str(path))
    try:
        p = json.loads(path.read_text())
    except (OSError, ValueError) as error:
        r.error(f'not readable JSON: {error}')
        return r
    check_place(path, r)
    if check_engine(p, r) is False:
        return r
    check_words(p, r)
    check_variables(p, r, declarations)
    check_purses(p, r, accounts, saved)
    check_wave_j(path, p, r, reads)
    if rows is not None:
        check_givers(p, r, rows)
    if conn is not None:
        chapters = p['id'].startswith('ninth-howl.') and bool(story_runner.chapter_starts(p))
        try:
            story_runner.references(conn, world, p, chapters=chapters)
            r.note('references: passed on the world (read only)')
        except Exception as error:   # noqa: BLE001
            r.error(f'references: {error}')
            conn.rollback()
            conn.execute('SET TRANSACTION READ ONLY')
        voices = check_residents(p, r, conn, world)
        if doors is not None:
            check_doors(p, r, doors)
        for npc, what, _ in hub_holders(p):
            if npc in voices:
                name, age, region, accent = voices[npc]
                r.note(f'voice: {npc} ({name}, {age}) {region or "?"}/{accent or "?"} for {what}')
    return r


# ---------------------------------------------------------------------------------------------------- the talk checks

def run_tool(r, title, cmd):
    proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    tail = (proc.stdout.strip().splitlines() or [''])[-1] if proc.stdout.strip() else proc.stderr.strip()[-300:]
    if proc.returncode != 0:
        r.error(f'{title} exit {proc.returncode}: ' + (proc.stdout + proc.stderr).strip()[-1500:])
    else:
        r.note(f'{title}: {tail}')


def talk_files_default():
    """The story talk the Ninth Howl writes (doc 86 6.5), those that exist, and the trees it adds lines to."""
    names = ['rules/story_ninth_howl.talk', 'scenes/common/story_ninth_howl.talk', 'rules/story_mq.talk',
             'scenes/common/story_mq.talk', 'dialogue/resident.tree', 'dialogue/watch.tree', 'dialogue/church.tree',
             'dialogue/official.tree', 'dialogue/inn.tree']
    return [TALK / n for n in names if (TALK / n).exists()]


def check_talk(files, world):
    r = Report('talk: ' + ', '.join(str(f.relative_to(ROOT)) if f.is_relative_to(ROOT) else str(f) for f in files)
               if files else 'talk: the library')
    if not TALK_CHECK.exists():
        r.error(f'{TALK_CHECK.relative_to(ROOT)} is missing: cmake --build build-core --target talk_check')
        return r
    nh_world_vars.credentials()
    validate = [f for f in files if f.suffix in ('.talk', '.tree', '.grammar')] or talk_files_default()
    run_tool(r, 'dialect_check', [sys.executable, str(HERE / 'dialect_check.py')] + (['--world', world] if world else []))
    run_tool(r, 'talk_check --validate', [str(TALK_CHECK), '--data', str(TALK), '--validate', *map(str, validate)])
    run_tool(r, 'talk_check --render-trees', [str(TALK_CHECK), '--data', str(TALK), '--render-trees'])
    run_tool(r, 'talk_lint', [sys.executable, str(HERE / 'talk_lint.py')])
    return r


# ----------------------------------------------------------------------------------------------------------- main

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('files', nargs='*')
    ap.add_argument('--all', action='store_true', help='every program under Data/Storylines/ridgemere3, and the talk checks')
    ap.add_argument('--talk', action='store_true', help='run the talk checks')
    ap.add_argument('--world', default='ridgemere3')
    ap.add_argument('--no-db', action='store_true', help="skip the world's checks (references, residents, purses)")
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--quiet', action='store_true', help='errors and warnings only')
    a = ap.parse_args(argv)
    paths = [Path(f) for f in a.files]
    if a.all:
        paths += [path for path, _ in nh_world_vars.programs(STORIES)]
    programs = []
    talk = list(dict.fromkeys(x for x in paths if x.suffix in ('.talk', '.tree', '.grammar') or x.name == 'dialect_allow.json'))
    for x in dict.fromkeys(paths):
        if x.suffix == '.json' and x.name not in ('dialect_allow.json', 'world-vars.json'):
            programs.append(x)
    reports = []
    # The declarations file itself, whenever any program is checked or it is named.
    declarations = json.loads(WORLD_VARS.read_text()) if WORLD_VARS.exists() else None
    if declarations is not None and (programs or any(x.name == 'world-vars.json' for x in paths)):
        r = Report(str(WORLD_VARS.relative_to(ROOT)))
        for e in nh_world_vars.check(declarations):
            r.error(e)
        errors, warnings = nh_world_vars.against_programs(declarations, STORIES)
        for e in errors:
            r.error(e)
        for w in warnings:
            r.warn(w)
        r.note(f'{len(declarations["variables"])} world variables declared')
        reports.append(r)
    conn = world = None
    accounts = saved = None
    rows = givers_registry() if GIVERS.exists() else None
    if programs and not a.no_db:
        nh_world_vars.credentials()
        key = world_db.resolve(a.world)
        conn = world_db.connect(key, 'dm', autocommit=False)
        conn.execute('SET TRANSACTION READ ONLY')
        world = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
        accounts, saved = latest_accounts(conn, world)
    reads = talk_reads() if TALK.exists() else None
    try:
        doors = DoorMap(conn, world) if conn is not None else None
        for x in programs:
            reports.append(check_program(x, conn, world, rows, declarations, accounts, saved, doors, reads))
    finally:
        if conn is not None:
            conn.rollback()
            conn.close()
    if a.all and reads is not None:
        # (Wave J, C8: the pack keys the library reads that no program writes stay silent; nh_ keys are the chapters'.)
        r = Report('talk keys: the library against the programs')
        written = set()
        for x in programs:
            try:
                written |= {(ns, k, v) for ns, k, v, _ in talk_writes(json.loads(x.read_text()))}
            except ValueError:
                pass
        silent = []
        for (ns, key), values in sorted(reads.items()):
            if not key.startswith(('sq_', 'mq_', 'lq_', 'st_', 'nh_coin')):
                continue
            for v in sorted(values - {'*'}):
                if (ns, key, v) not in written:
                    silent.append(f'{ns}.mem.{key}={v}')
        for x in silent:
            r.warn(f'no program writes {x}: its lines stay silent until the DM seeds it')
        r.note(f'{len(written)} keys and values written by the programs; {len(silent)} the library reads that none writes')
        reports.append(r)
    if talk or a.talk or a.all:
        reports.append(check_talk([x.resolve() for x in talk], '' if a.no_db else a.world))
    if a.json:
        print(json.dumps([r.json() for r in reports], indent=1))
    else:
        for r in reports:
            print(r.show(verbose=not a.quiet))
        bad = [r for r in reports if not r.passed()]
        print(f'\n{len(reports) - len(bad)} of {len(reports)} passed' + (': FAIL' if bad else ': PASS'))
    return 1 if any(not r.passed() for r in reports) else 0


if __name__ == '__main__':
    sys.exit(main())
