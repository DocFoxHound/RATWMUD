#!/usr/bin/env python3
"""What a town's mantles have already taken (Docs/Design/84-living-conversations.md §14.2, Phase 11), so a writer's next
mantle comes out distinct from its neighbours': their titles, quirks, oaths and sayings (no two in a town may share
one), their own words, opinions (issue, side and the reason given), stories, scenes and the topics they're on, the
moves their lines make, and the distinctive 4-word runs they say (one shared is a warning sign; mantle_check refuses a
shared 5-word run within a dialect). Read-only.

  python3 tools/mantle_index.py TOWN [--world WORLD] [--dir DIR]... [--json]

TOWN is a town (saltwell, fenshore, coldwater, ridgemere...) or a dialect (`--dialect`). Mantles are read from the
export (Data/Talk/mantles-export, and any --dir: a writer's drafts) and, with --world, from the world's rows too (rows
win where both hold a mantle). A mantle is the town's when the resident it is worn by (with --world) or written for
lives there; without --world, when its dialect is the town's grammar.
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
import mantle_check as C  # noqa: E402
import mantle_text as T  # noqa: E402

EXPORT = ROOT / 'Data' / 'Talk' / 'mantles-export'
TOWN_DIALECT = {'fenshore': 'mirebridge', 'ridgemere': 'plain'}


def read_dirs(dirs: list[Path]) -> dict[str, T.Mantle]:
    out = {}
    for d in dirs:
        for f in sorted(d.rglob('*.mantle')) if d.is_dir() else [d]:
            try:
                m = T.read(f.read_text(), str(f))
            except T.MantleError as e:
                print(f'(unreadable, skipped: {f}: {e.problems[0]})', file=sys.stderr)
                continue
            out[m.id] = m
    return out


def towns_of(world: str | None, mantles: dict[str, T.Mantle]) -> dict[str, str]:
    """mantle id -> the town its wearer (or the one it was written for) lives in."""
    if not world:
        return {}
    import world_db as W
    with W.connect(W.resolve(world), 'editor', autocommit=False) as conn:
        conn.execute('SET TRANSACTION READ ONLY')
        wid = C.world_id_of(conn)
        facts = C.world_facts(conn, wid)
        region = dict(conn.execute('SELECT id, region FROM world.cells WHERE world_id = %s UNION ALL SELECT id, region FROM '
                                   'world.interiors WHERE world_id = %s', (wid, wid)).fetchall())
        home = dict(conn.execute('SELECT id, home_area FROM live.npcs WHERE world_id = %s', (wid,)).fetchall())
        for m in C.world_mantles(conn, wid):
            mantles[m.id] = m
    out = {}
    for mid, m in mantles.items():
        npc = facts['wearer'].get(mid) or m.written_for
        if npc and home.get(npc):
            out[mid] = region.get(home[npc], '')
    return out


def index(mantles: list[T.Mantle]) -> dict:
    said_ = collections.Counter()
    by_run = collections.defaultdict(set)
    out = {'mantles': [], 'quirks': {}, 'oaths': {}, 'sayings': {}, 'words': {}, 'opinions': {}, 'stories': {}, 'scenes': {},
           'moves': collections.Counter(), 'topics': collections.Counter()}
    for m in mantles:
        out['mantles'].append(f'{m.id} ({m.state}, {m.tier}, {m.dialect}, {" ".join(m.accents)}): "{m.title}" '
                              f'written for {m.written_for or "-"}')
        if m.quirk:
            out['quirks'].setdefault(m.quirk, []).append(m.id)
        for it in m.items:
            if it.kind == 'symbol':
                values = C.said(it)[0][0] if C.said(it) else []
                name = it.symbol.split('.')[0]
                if name in ('oath', 'saying'):
                    for v in values:
                        out['oaths' if name == 'oath' else 'sayings'].setdefault(v, []).append(m.id)
                else:
                    out['words'].setdefault(name, []).append(f'{m.id}: {" | ".join(values)[:90]}')
            elif it.kind == 'opinion':
                first = it.body.split('\n')[0]
                _, head, crit, words, _, _ = T.split_item(first[len('opinion'):])
                out['opinions'].setdefault(head.split()[0] if head else '?', []).append(
                    f'{m.id} {" ".join(head.split()[1:])}{" [" + crit + "]" if crit else ""}: {words[:100]}')
            elif it.kind == 'story':
                first = it.body.split('\n')[0]
                beat = next((l.strip()[1:].strip() for l in it.body.split('\n')[1:] if l.strip().startswith(':')), '')
                out['stories'].setdefault(m.id, []).append(f'{it.key}: {beat[:90]}')
            elif it.kind == 'scene':
                when = ' '.join(l.strip()[5:] for l in it.body.split('\n') if l.strip().startswith('when '))
                tops = re.findall(r'(?:topic|concept)=([a-z_.|]+)', when)
                for t in tops:
                    for x in t.replace('talk.', '').split('|'):
                        out['topics'][x] += 1
                first = next((l.split(':', 1)[1].strip() for l in it.body.split('\n')[1:] if re.match(r'^\s*a\b[^:]*:', l)), '')
                out['scenes'].setdefault(m.id, []).append(f'{it.key} [{" ".join(tops)}]: {first.split(" | ")[0][:80]}')
            elif it.kind == 'line':
                first = it.body.split('\n')[0]
                _, head, _, _, _, _ = T.split_item(first[len('line'):])
                for mv in head.split():
                    out['moves'][mv] += 1
            for texts, _, who in C.said(it):
                if who == 'p':
                    continue
                for t in texts:
                    words = re.findall(r"[a-z']+", C.plain_words(t).lower())
                    for i in range(len(words) - 3):
                        run = tuple(words[i:i + 4])
                        by_run[run].add(m.id)
                        said_[run] += 1
    out['shared_4grams'] = sorted(((' '.join(r), sorted(ids)) for r, ids in by_run.items() if len(ids) > 1),
                                  key=lambda x: -len(x[1]))[:80]
    out['moves'] = dict(out['moves'].most_common())
    out['topics'] = dict(out['topics'].most_common())
    return out


def show(town: str, ix: dict) -> str:
    o = [f'# What {town}\'s {len(ix["mantles"])} mantles have taken']
    o += ['', '## Mantles'] + [f'- {x}' for x in ix['mantles']]
    for key, title in (('quirks', 'Quirks (at most two in a town)'), ('oaths', 'Oaths (none twice in a town)'),
                       ('sayings', 'Sayings (none twice in a town)')):
        o += ['', f'## {title}'] + [f'- {k}: {", ".join(v)}' for k, v in sorted(ix[key].items())]
    o += ['', '## Their own words'] + [f'- {k}: ' + '; '.join(v) for k, v in sorted(ix['words'].items())]
    o += ['', '## Opinions'] + [f'- {k}: ' + ' || '.join(v) for k, v in sorted(ix['opinions'].items())]
    o += ['', '## Stories'] + [f'- {k}: ' + ' || '.join(v) for k, v in sorted(ix['stories'].items())]
    o += ['', '## Scenes'] + [f'- {k}: ' + ' || '.join(v) for k, v in sorted(ix['scenes'].items())]
    o += ['', '## Scene topics (how many scenes on each)', ', '.join(f'{k} {v}' for k, v in ix['topics'].items())]
    o += ['', '## Line moves', ', '.join(f'{k} {v}' for k, v in ix['moves'].items())]
    o += ['', '## 4-word runs said by more than one (change yours if it is one of them)']
    o += [f'- "{r}": {", ".join(ids)}' for r, ids in ix['shared_4grams']]
    return '\n'.join(o) + '\n'


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('town')
    ap.add_argument('--world')
    ap.add_argument('--dir', action='append', default=[])
    ap.add_argument('--dialect', action='store_true', help='TOWN is a dialect: every mantle of it')
    ap.add_argument('--json', action='store_true')
    a = ap.parse_args()
    mantles = read_dirs([EXPORT] + [Path(d) for d in a.dir])
    towns = towns_of(a.world, mantles)
    town = a.town.lower()
    if a.dialect:
        chosen = [m for m in mantles.values() if m.dialect == town]
    elif towns:
        chosen = [m for mid, m in mantles.items() if towns.get(mid) == town]
    else:
        d = TOWN_DIALECT.get(town, town)
        chosen = [m for m in mantles.values() if m.dialect == d]
    chosen.sort(key=lambda m: m.id)
    ix = index(chosen)
    print(json.dumps(ix, indent=1) if a.json else show(town, ix))
    return 0


if __name__ == '__main__':
    sys.exit(main())
