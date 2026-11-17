#!/usr/bin/env python3
"""A writer's brief for one resident's mantle (Docs/Design/84-living-conversations.md §14.2, Phase 11): everything a
writer needs to give a wolf a voice of their own, read from the world and the library, and nothing written anywhere.

  python3 tools/mantle_brief.py WORLD NPC... [--facts RESIDENTS.jsonl] [--json] [--examples 20]

For each resident: who they are (sex, age, stage, post and work, class and wealth, home, street and town), where they
grew up and how they sound (origin, accent, the dialect a mantle for them is written in), the traits a mantle keeps
(honesty and quarter, never changed: they drive thieves and the Market Board, §9.5) and the talk-only ones it may refine,
their quirk, likes and dislikes, the stances the game works out for them and why, their household, kin and the people
the slots name (spouse, child, mother, father, neighbour, friend, rival, boss, hand, priest, shopkeeper), which §11
slots are filled for them (a line using an empty one doesn't fit them), their personality and backstory (to honour, never
to copy: they name the wolf), the old luna voice row and its fixture port where there is one, the town's lore (its scene
file's head), the dialect's words, and example lines in that dialect. The facts are the game's own
(`talk_check --world WORLD --residents --json`, a scratch world: nothing is written); --facts reads a saved dump of
them instead. The rest is read from the world's rows in one read-only transaction.

Names appear in the brief so the writer knows who is who; a mantle never says one (mantle_check refuses it): every
name, kin, trade and place goes through a slot (§11).
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
TALK = ROOT / 'Data' / 'Talk'
sys.path.insert(0, str(HERE))

# A region's scene file: its lore and its example lines.
REGION_FILE = {'saltwell': 'saltwell.talk', 'fenshore': 'mirebridge.talk', 'coldwater': 'coldwater.talk',
               'larkfield': 'larkfield.talk', 'southmeadow': 'southmeadow.talk', 'frosthollow': 'frosthollow.talk',
               'oakhaven': 'oakhaven.talk', 'ridgemere': 'ridgemere.talk', 'mirebridge': 'mirebridge.talk'}
# What a region is called in the talk (here.region): the Waste is Saltwell's, the fens Fenshore's.
HERE_REGION = {'saltwell': 'waste', 'fenshore': 'fens'}
# §11's slots, and the fact behind each (a line saying one fits only a wearer who has it).
SLOTS = ('first', 'surname', 'spouse', 'spouse_word', 'child', 'mother', 'father', 'late', 'neighbour', 'friend', 'rival',
         'boss', 'hand', 'priest', 'shopkeeper', 'leader', 'workplace', 'trade_words', 'street', 'from_town', 'away', 'quarter')
TRAITS = ('temper', 'talk', 'warmth', 'humour', 'honesty', 'piety', 'drive')


def talk_check() -> Path:
    built = sorted(ROOT.glob('build*/talk_check'), key=lambda p: p.stat().st_mtime, reverse=True)
    if not built:
        raise SystemExit('mantle_brief: talk_check is not built (cmake --build build-core --target talk_check)')
    return built[0]


def residents(world: str, facts_file: str | None) -> dict[str, dict]:
    if facts_file:
        lines = Path(facts_file).read_text().splitlines()
    else:
        run = subprocess.run([str(talk_check()), '--world', world, '--residents', '--json'], capture_output=True, text=True,
                             timeout=900)
        if run.returncode != 0:
            raise SystemExit('mantle_brief: talk_check failed:\n' + run.stderr[-2000:])
        lines = run.stdout.splitlines()
    out = {}
    for line in lines:
        if line.startswith('{'):
            r = json.loads(line)
            out[r['id']] = r
    return out


def dialect_of(origin: str) -> str:
    mapped = json.loads((TALK / 'settings.json').read_text()).get('dialect_of', {})
    d = mapped.get(origin, origin)
    return 'plain' if d in ('ridgemere', '', None) or not (TALK / 'grammar' / f'{d}.grammar').is_file() else d


def world_rows(world: str, npcs: list[str]) -> dict:
    """The rows a brief reads: each NPC, their voice, kin, household, whether they live, and their home's region."""
    import world_db as W
    with W.connect(W.resolve(world), 'editor', autocommit=False) as conn:
        conn.execute('SET TRANSACTION READ ONLY')
        world_id = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
        region = dict(conn.execute('SELECT id, region FROM world.cells WHERE world_id = %s UNION ALL '
                                   'SELECT id, region FROM world.interiors WHERE world_id = %s', (world_id, world_id)).fetchall())
        cols = ('id', 'name', 'role', 'age', 'description', 'greeting', 'personality', 'backstory', 'work_label', 'home_area',
                'work_area')
        allnpcs = {r[0]: dict(zip(cols, r)) for r in conn.execute(
            f"SELECT {', '.join(cols)} FROM live.npcs WHERE world_id = %s", (world_id,)).fetchall()}
        alive = dict(conn.execute('SELECT npc_id, alive FROM live.npc_state WHERE world_id = %s', (world_id,)).fetchall())
        vcols = ('npc_id', 'home_region', 'accent', 'quarter', 'traits', 'quirk', 'persona', 'grammar', 'rules', 'written_by',
                 'mantle_id', 'bound_how', 'released_day')
        voice = {r[0]: dict(zip(vcols, r)) for r in conn.execute(
            f"SELECT {', '.join(vcols)} FROM live.npc_voice WHERE world_id = %s", (world_id,)).fetchall()}
        rel = conn.execute('SELECT npc_id, other_id, kind FROM live.npc_relations WHERE world_id = %s', (world_id,)).fetchall()
    out = {}
    for npc in npcs:
        n = allnpcs.get(npc)
        if not n:
            out[npc] = None
            continue
        kin = []
        for a, b, kind in rel:
            if a == npc and b in allnpcs:
                kin.append((kind if kind == 'spouse' else 'child', b))
            elif b == npc and a in allnpcs:
                kin.append((kind if kind == 'spouse' else 'parent', a))
        home = n['home_area']
        household = [o for o, r in allnpcs.items() if o != npc and home and r['home_area'] == home and alive.get(o, True)]
        out[npc] = {'npc': n, 'voice': voice.get(npc) or {}, 'alive': alive.get(npc, True),
                    'region': region.get(home or '', ''), 'kin': [(k, o, allnpcs[o]['name'], alive.get(o, True), bool(allnpcs[o]['home_area'])) for k, o in kin],
                    'household': [(o, allnpcs[o]['name'], allnpcs[o]['work_label'] or allnpcs[o]['role']) for o in household],
                    'names': {o: r['name'] for o, r in allnpcs.items()}}
    return out


def lore(region: str) -> tuple[str, list[str], list[str]]:
    """A region's scene file: its head (the lore), its scenes' names and topics, and every first-alternative line."""
    f = TALK / 'scenes' / 'regions' / REGION_FILE.get(region, 'towns.talk')
    if not f.is_file():
        return '', [], []
    head, scenes, lines = [], [], []
    started = False
    for raw in f.read_text().splitlines():
        t = raw.strip()
        if not started and t.startswith('#'):
            head.append(t.lstrip('# ').rstrip())
            continue
        started = True
        if t.startswith('scene '):
            scenes.append(t[6:].strip())
        m = re.match(r'^[ab]\??(?:\[[^\]]*\])?(?:\s*#\w+)?(?:@\w+)?\s*:\s*(.*)$', t)
        if m:
            first = m.group(1).split(' | ')[0].strip()
            if first and len(first.split()) >= 5:
                lines.append(first)
    return ' '.join(head), scenes, lines


def grammar(dialect: str) -> list[tuple[str, str]]:
    f = TALK / 'grammar' / f'{dialect}.grammar'
    out = []
    for raw in f.read_text().splitlines() if f.is_file() else []:
        m = re.match(r'^([a-z0-9_]+(?:\.broad)?!{0,2}(?:\[[^\]]*\])?)\s*\+?=\s*(.*)$', raw.strip())
        if m:
            out.append((m.group(1), m.group(2)))
    return out


def fixture_port(npc: str) -> str:
    for f in sorted((ROOT / 'Tests' / 'fixtures' / 'mantles').rglob('*.mantle')):
        text = f.read_text()
        if re.search(rf'^written_for {re.escape(npc)}$', text, re.M):
            return f'{f.relative_to(ROOT)}\n{text}'
    return ''


def brief(world: str, npc: str, facts: dict, rows: dict, examples: int) -> dict:
    r = facts.get(npc) or {}
    w = rows.get(npc)
    if w is None:
        return {'npc': npc, 'error': f'{npc} is not an NPC of {world}'}
    n, v = w['npc'], w['voice']
    origin = r.get('from') or v.get('home_region') or w['region']
    dialect = r.get('dialect') or dialect_of(origin)
    accent = r.get('accent') or v.get('accent') or 'none'
    traits = dict(v.get('traits') or {})
    for t in TRAITS:
        if r.get(t):
            traits[t] = r[t]
    head, scenes, lines = lore(w['region'] or origin)
    step = max(1, len(lines) // max(1, examples))
    tier = 'full' if (r.get('role') or n['role']) == 'merchant' or r.get('job') in ('innkeeper', 'priest', 'guard') or \
        r.get('master') == 'yes' or r.get('house') else 'standard'
    slots = {s: r[s] for s in SLOTS if r.get(s) not in (None, '', 'no', 'none')}
    out = {
        'npc': npc, 'name': n['name'], 'alive': w['alive'], 'sex': r.get('sex', ''), 'age': r.get('age', n['age']),
        'stage': r.get('stage', ''), 'role': r.get('role', n['role']), 'job': r.get('job', ''), 'trade': r.get('trade', ''),
        'work_label': n['work_label'], 'post': r.get('_post_title', ''), 'workplace': r.get('workplace', ''),
        'class': r.get('class', ''), 'wealth': r.get('wealth', ''), 'house': r.get('house', ''),
        'home': n['home_area'], 'town': w['region'], 'here_region': HERE_REGION.get(w['region'], w['region']),
        'street': r.get('street', ''), 'quarter': v.get('quarter', ''),
        'origin': origin, 'from_town': r.get('from_town', ''), 'away': r.get('away', ''), 'accent': accent,
        'dialect': dialect, 'mantle_accents': ('none' if accent == 'none' else f'{accent} {"light" if accent == "broad" else "broad"}'),
        'tier': tier,
        'kept': {'honesty': traits.get('honesty', 'plain'), 'quarter': v.get('quarter', '')},
        'traits': traits, 'quirk': r.get('quirk') or v.get('quirk', ''),
        'likes': r.get('likes', ''), 'dislikes': r.get('dislikes', ''), 'mood': r.get('mood', ''),
        'words': {k: r[k] for k in ('food_words', 'pastime_words', 'thing_words', 'weather_words', 'dislike_words') if r.get(k)},
        'stances': {k[3:]: f"{r[k]} {r.get('op_strength.' + k[3:], '')}" + (f" — {r['op_why.' + k[3:]]}" if r.get('op_why.' + k[3:]) else '')
                    for k in sorted(r) if k.startswith('op.')},
        'married': r.get('married', ''), 'widowed': r.get('widowed', ''), 'parent': r.get('parent', ''),
        'mourning': r.get('mourning', ''), 'master': r.get('master', ''), 'apprentice': r.get('apprentice', ''),
        'slots': slots, 'kin': [f'{k}: {name}{"" if a else " (dead)"}{"" if placed else " (in the Country, not in a town: no slot names them)"}'
                                for k, _, name, a, placed in w['kin']],
        'household': [f'{name} ({work})' for _, name, work in w['household']],
        'personality': n['personality'], 'backstory': n['backstory'], 'description': n['description'],
        'greeting': n['greeting'],
        'luna': {k: v.get(k, '') for k in ('persona', 'grammar', 'rules')} if v.get('written_by') not in ('', 'seed', None) else {},
        'fixture_port': fixture_port(npc),
        'mantle': {'id': v.get('mantle_id') or '', 'how': v.get('bound_how') or ''},
        'lore': head, 'scenes': scenes,
        'examples': lines[::step][:examples],
        'grammar': grammar(dialect) if dialect != 'plain' else [],
    }
    return out


def text(b: dict) -> str:
    if b.get('error'):
        return b['error'] + '\n'
    o = []
    say = o.append
    say(f"# {b['npc']}: {b['name']}{'' if b['alive'] else ' (DEAD: no mantle is bound to the dead)'}")
    say(f"{b['sex']}, {b['age']} ({b['stage']}); {b['role']} / job {b['job']} / trade words \"{b.get('slots', {}).get('trade_words', '')}\"")
    say(f"work: {b['work_label'] or '-'}; post {b['post'] or '-'}; workplace slot \"{b['workplace'] or '-'}\"; class {b['class']}, {b['wealth']}"
        f"{'; House ' + b['house'] if b['house'] else ''}")
    say(f"home {b['home']} in {b['town']} (here.region={b['here_region']}); street \"{b['street']}\"; quarter {b['quarter'] or '-'}")
    say(f"grew up: {b['origin']} ({b['from_town']}); away={b['away'] or 'no'}; accent {b['accent']} -> mantle dialect "
        f"**{b['dialect']}**, accents `{b['mantle_accents']}`; tier **{b['tier']}**")
    say(f"kept (never changed): honesty {b['kept']['honesty']}, quarter {b['kept']['quarter'] or '-'}")
    say('traits: ' + ' '.join(f'{k}={v}' for k, v in b['traits'].items()) + f"; quirk {b['quirk'] or '-'}; mood {b['mood']}")
    say(f"likes {b['likes'] or '-'}; dislikes {b['dislikes'] or '-'}")
    if b['words']:
        say('their words for things: ' + '; '.join(f'{k} "{v}"' for k, v in b['words'].items()))
    say(f"married {b['married'] or '-'}, widowed {b['widowed'] or '-'}, parent {b['parent'] or '-'}, mourning {b['mourning'] or '-'}, "
        f"master {b['master'] or '-'}, apprentice {b['apprentice'] or '-'}")
    say('\n## Slots filled for this wearer (a line using another does not fit them)')
    for k, v in b['slots'].items():
        say(f'- #self.{k}# = {v}')
    say('\n## Kin, household')
    for k in b['kin'] or ['(no kin rows)']:
        say(f'- {k}')
    for h in b['household'] or ['(lives alone)']:
        say(f'- same home: {h}')
    say('\n## Stances the game works out (a mantle may override with its own opinions and reasons)')
    for k, v in b['stances'].items():
        say(f'- {k}: {v}')
    say('\n## Personality and backstory (honour them; never copy: they name the wolf)')
    say(f"personality: {b['personality']}")
    say(f"description: {b['description']}")
    say(f"backstory: {b['backstory']}")
    if b['greeting']:
        say(f"old greeting: {b['greeting']}")
    if b['luna']:
        say('\n## The old luna voice row (emptied once the mantle lands; its facts may be stale)')
        for k, v in b['luna'].items():
            say(f'{k}:\n{v}')
    if b['fixture_port']:
        say('\n## Its port in the test fixture (Phase 9; content to honour, not to copy word for word)')
        say(b['fixture_port'])
    say(f"\n## The town's lore ({b['town']})\n{b['lore']}")
    say('scenes already in the town file: ' + ', '.join(b['scenes'][:60]))
    say('\n## Example lines of the place')
    for e in b['examples']:
        say(f'- {e}')
    if b['grammar']:
        say(f"\n## The {b['dialect']} grammar's words (#symbol#; the dialect's own speech)")
        say('; '.join(f'{k} = {v[:60]}' for k, v in b['grammar'] if not k.startswith('bc_')))
        say('backchannels: ' + ' '.join('#' + k + '#' for k, _ in b['grammar'] if k.startswith('bc_')))
    return '\n'.join(o) + '\n'


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('world')
    ap.add_argument('npcs', nargs='+')
    ap.add_argument('--facts', help='a saved dump of talk_check --world WORLD --residents --json')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--examples', type=int, default=20)
    a = ap.parse_args()
    facts = residents(a.world, a.facts)
    rows = world_rows(a.world, a.npcs)
    briefs = [brief(a.world, npc, facts, rows, a.examples) for npc in a.npcs]
    if a.json:
        print(json.dumps(briefs, indent=1, default=str))
    else:
        for b in briefs:
            sys.stdout.write(text(b) + '\n')
    return 1 if any(b.get('error') for b in briefs) else 0


if __name__ == '__main__':
    sys.exit(main())
