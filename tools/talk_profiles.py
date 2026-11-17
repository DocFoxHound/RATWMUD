#!/usr/bin/env python3
"""The replier kinds for the coverage check (Docs/Design/84 §15.4, Phase 7): Data/Talk/profiles.json.

`talk_check --coverage` asks, for every move a line can owe, every topic and every kind of resident who might owe the
answer, whether some line answers and holds. The kinds are Ridgemere 3's real residents, clustered on the axes lines
test: stage, job, wealth and dialect (the cluster), each with the temperament most of its members have (temper,
warmth, humour, talk, honesty, piety, drive) and the facts of the member nearest to it, names left out. Read-only: it
asks the game for the residents (`talk_check --world KEY --residents --json`, a scratch world written nowhere).

  python3 tools/talk_profiles.py ridgemere3                 write Data/Talk/profiles.json (RATW_DATABASE_URL, or the
                                                            registry's game connection)
  python3 tools/talk_profiles.py --from RESIDENTS.jsonl     from a saved dump
  options: --kinds N (150), --out FILE
"""
from __future__ import annotations

import argparse
import collections
import datetime
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TALK = ROOT / 'Data' / 'Talk'
CLUSTER = ('stage', 'job', 'wealth', 'dialect')
TEMPERAMENT = ('temper', 'warmth', 'humour', 'talk', 'honesty', 'piety', 'drive')
# Facts that name somebody: kept as present (a line may need one to name), never as the name itself.
NAMES = {'name', 'first', 'surname', 'mother', 'father', 'neighbour', 'shopkeeper', 'spouse', 'child', 'priest', 'leader',
         'friend', 'rival', 'boss', 'hand', 'late', 'master_name', 'called'}


def residents(world: str) -> list[dict]:
    built = sorted(ROOT.glob('build*/talk_check'), key=lambda p: p.stat().st_mtime, reverse=True)
    if not built:
        raise SystemExit('talk_profiles: no talk_check built (cmake --build <build> --target talk_check)')
    run = subprocess.run([str(built[0]), '--world', world, '--residents', '--json'], capture_output=True, text=True, timeout=600)
    if run.returncode != 0:
        raise SystemExit('talk_profiles: talk_check failed:\n' + run.stderr[-2000:])
    return [json.loads(l) for l in run.stdout.splitlines() if l.startswith('{')]


def dialect_of(origin: str) -> str:
    mapped = json.loads((TALK / 'settings.json').read_text()).get('dialect_of', {})
    d = mapped.get(origin, origin)
    return 'plain' if d in ('ridgemere', '', None) or not (TALK / 'grammar' / f'{d}.grammar').is_file() else d


def facts_of(r: dict) -> dict:
    out = {}
    for k, v in sorted(r.items()):
        if k.startswith('_') or k == 'id' or k.startswith(('op_why.', 'op_basis.')):
            continue
        out[k] = 'Name' if k in NAMES or k.endswith('_name') else v
    return out


def profile(rs: list[dict], kinds: int) -> dict:
    for r in rs:
        r['dialect'] = dialect_of(r.get('from', ''))
    groups = collections.defaultdict(list)
    for r in rs:
        groups[tuple(r.get(a, '') for a in CLUSTER)].append(r)
    ranked = sorted(groups.items(), key=lambda kv: (-len(kv[1]), kv[0]))[:kinds]
    out = []
    for i, (key, members) in enumerate(ranked, 1):
        modal = {a: collections.Counter(m.get(a, '') for m in members).most_common(1)[0][0] for a in TEMPERAMENT}
        region = collections.Counter(m.get('_home', '') for m in members).most_common(1)[0][0]
        nearest = max(members, key=lambda m: (sum(m.get(a, '') == modal[a] for a in TEMPERAMENT), m.get('id', '')))
        facts = facts_of(nearest)
        facts.update(modal)
        out.append({'id': f'k{i:03}', 'count': len(members), 'axes': dict(zip(CLUSTER, key)), 'temperament': modal,
                    'region': region or 'ridgemere', 'facts': facts})
    covered = sum(k['count'] for k in out)
    return {'about': 'Replier kinds for talk_check --coverage (Docs/Design/84 §15.4): Ridgemere 3\'s residents clustered by '
                     'stage, job, wealth and dialect, each with its members\' commonest temperament and the facts of the '
                     'member nearest to it (names left out). Regenerate with tools/talk_profiles.py ridgemere3.',
            'generated': datetime.date.today().isoformat(), 'residents': len(rs), 'covered': covered,
            'axes': list(CLUSTER), 'temperament': list(TEMPERAMENT),
            'bands': ['strangers', 'acquaintances', 'friends', 'family'], 'kinds': out}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('world', nargs='?')
    ap.add_argument('--from', dest='source', type=Path)
    ap.add_argument('--kinds', type=int, default=150)
    ap.add_argument('--out', type=Path, default=TALK / 'profiles.json')
    a = ap.parse_args(argv)
    if not a.world and not a.source:
        ap.error('a world key or --from RESIDENTS.jsonl')
    rs = [json.loads(l) for l in a.source.read_text().splitlines() if l.startswith('{')] if a.source else residents(a.world)
    p = profile(rs, a.kinds)
    a.out.write_text(json.dumps(p, indent=1, ensure_ascii=False) + '\n')
    print(f'talk_profiles: {len(p["kinds"])} kinds cover {p["covered"]} of {p["residents"]} residents -> {a.out}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
