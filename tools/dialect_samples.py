#!/usr/bin/env python3
"""A listening sample of the dialogue window (Docs/Design/83-dialogue-trees.md, Phase 4): for each place, a few of its
residents as the world has them now (where they grew up, their accent, their work), and what they say through the
shop, resident and work trees, rendered offline by the game's own talk library (talk_check, no model).

    python3 tools/dialect_samples.py --world ridgemere3 --out FILE.md [--residents FILE.jsonl] [--per 3] [--seed 7]
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

import dialect_check as DC

ORDER = ['ridgemere', 'coldwater', 'larkfield', 'southmeadow', 'oakhaven', 'frosthollow', 'fenshore', 'saltwell']
# (tree node, a label, the moment's facts for it)
NODES = {
    'shop': [('dlg.shop.greet', 'Trade', {}), ('dlg.shop.prices', "What's dear today?", {}), ('dlg.shop.bye', 'Goodbye.', {})],
    'resident': [('dlg.resident.hub', 'Talk', {}), ('dlg.resident.job', 'What do you do?', {}),
                 ('dlg.resident.news', 'Heard any news?', {}), ('dlg.resident.trouble', 'Is something troubling you?', {}),
                 ('dlg.resident.way', 'Could you tell me the way?', {}), ('dlg.resident.bye', 'Goodbye.', {})],
    'work': [('dlg.work.list', 'Any work going?', {'here.work': '0'}), ('dlg.work.taken', "I'll take it on.", {}),
             ('dlg.work.declined', 'Not for me, thanks.', {}), ('dlg.work.given', "I can't do it after all.", {})],
}
ROLE = {'innkeeper': ('inn', [('dlg.inn.greet', 'Talk', {}), ('dlg.inn.rest', "I'll just rest by the fire a while.", {}),
                              ('dlg.inn.roomed', 'A bed for the night (3p).', {})]),
        'guard': ('watch', [('dlg.watch.hub', 'Talk', {}), ('dlg.watch.reported', 'I saw it with my own eyes.', {'topic.reported': 'yes'}),
                            ('dlg.watch.bye', 'Goodbye.', {})]),
        'priest': ('church', [('dlg.church.greet', 'Talk', {}), ('dlg.church.bye', 'Goodbye.', {})])}
MOMENT = {'here.time': 'day', 'here.shop': 'open', 'here.giver': 'yes', 'here.places': '3', 'here.troubles': '0',
          'topic.meeting': 'stranger', 'here.wanted': 'no', 'here.inn': 'yes', 'here.bed_ok': 'yes', 'here.inn_helper': 'no',
          'here.day': 'work', 'here.weather': 'clear', 'here.season': 'autumn', 'world.times': 'steady',
          'here.folk': '40', 'here.shops': '4', 'here.inns': '1', 'here.service': 'from nine in the morning until eleven'}
TOWNS = {'ridgemere': 'Ridgemere', 'coldwater': 'Coldwater', 'larkfield': 'Larkfield', 'southmeadow': 'Southmeadow',
         'oakhaven': 'Oakhaven', 'frosthollow': 'Frosthollow', 'fenshore': 'Fenshore', 'saltwell': 'Saltwell'}


def say(talk_check: Path, concept: str, person: dict, facts: dict, dialect_of: dict, seed: int) -> str:
    args = [str(talk_check), concept, '--seed', str(seed)]
    for k, v in person.items():
        if not k.startswith('_') and k != 'grammar' and v:
            args.append(f'a.{k}={v}')
    if person.get('from'):
        args.append('a.grammar=' + dialect_of.get(person['from'], person['from']))
    args += ['b.name=Ash', 'b.sex=male', 'b.stage=adult', 'b.age=30']
    args += [f'{k}={v}' for k, v in {**MOMENT, 'here.town': TOWNS.get(person.get('_home', ''), 'Ridgemere'),
                                     'here.region': person.get('_home', 'ridgemere'), **facts}.items()]
    run = subprocess.run(args, capture_output=True, text=True, timeout=60)
    lines = [l[5:] for l in run.stdout.splitlines() if l.startswith('  a: ')]
    return lines[-1] if lines else '(nothing)'


def pick(people: list[dict], town: str, per: int) -> list[dict]:
    """A keeper, a local, and one from elsewhere where there is one (the broadest first), up to `per`."""
    here = [p for p in people if p.get('_home') == town and p.get('stage') in ('adult', 'old')]
    rank = {'broad': 0, 'light': 1, 'none': 2}
    here.sort(key=lambda p: (rank.get(p.get('accent', ''), 3), p['id']))
    chosen = []
    for want in (lambda p: p.get('role') == 'merchant' and p.get('from') == town,
                 lambda p: p.get('job') in ROLE and p.get('role') != 'merchant' or p.get('job') in ('innkeeper',),
                 lambda p: p.get('from') == town and p.get('role') != 'merchant',
                 lambda p: p.get('from') not in (town, '', None)):
        for p in here:
            if want(p) and p not in chosen:
                chosen.append(p)
                break
        if len(chosen) >= per:
            break
    return chosen[:per]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--world', default='ridgemere3')
    parser.add_argument('--residents', default='', help='a talk_check --residents --json file (else it is run)')
    parser.add_argument('--out', required=True)
    parser.add_argument('--per', type=int, default=3)
    parser.add_argument('--seed', type=int, default=7)
    args = parser.parse_args()
    talk_check = DC.newest_talk_check()
    if not talk_check:
        sys.exit('No talk_check built')
    if args.residents:
        people = [json.loads(l) for l in Path(args.residents).read_text().splitlines() if l.startswith('{')]
    else:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import world_db
        people = DC.residents(world_db.resolve(args.world, 'dev'))
    dialect_of = DC.settings().get('dialect_of', {})
    out = ['# How Ridgemere 3 sounds now: a listening sample', '',
           'Doc 83, Phase 4. A few residents of each place as the world has them after the origins were worked out again '
           '(where they grew up, their accent: none, light or broad), and what they say in the dialogue window through the '
           'shop, resident and work trees (and their role\'s own, where they have one). Rendered offline by the game\'s talk '
           'library (`tools/dialect_samples.py`, no model). A light accent uses its dialect\'s words about half the time, so '
           'the same wolf sounds plainer some days. Fenshore speaks as the fens (Mirebridge) did, Saltwell as Coldwater. '
           'The player is "Ash", a stranger; the moment is an autumn day.', '']
    seed = args.seed
    for town in ORDER:
        chosen = pick(people, town, args.per)
        if not chosen:
            continue
        local = sum(1 for p in people if p.get('_home') == town and dialect_of.get(p.get('from', ''), p.get('from')) ==
                    dialect_of.get(town, town))
        total = sum(1 for p in people if p.get('_home') == town)
        speech = dialect_of.get(town, town)
        out += [f'## {TOWNS[town]}', '',
                f'{total} residents; {local} speak its speech ({"plain, the city\'s" if speech == "ridgemere" else speech}).', '']
        for p in chosen:
            work = p.get('_post_title') or p.get('_work_label') or p.get('job', '')
            out.append(f"### {p['name']} ({p.get('sex', '')}, {p.get('stage', '')}), {work}")
            out.append(f"Grew up in {TOWNS.get(p.get('from', ''), (p.get('from') or 'nowhere').capitalize())}, "
                       f"accent {p.get('accent') or 'none'}; {p.get('class', '')} class, {p.get('wealth', '')}.")
            out.append('')
            trees = (['shop'] if p.get('role') == 'merchant' else []) + ['resident', 'work']
            sections = [(t, NODES[t]) for t in trees]
            if p.get('job') in ROLE:
                tree, nodes = ROLE[p['job']]
                if p['job'] == 'innkeeper' and p.get('role') != 'merchant':
                    # A helper at the inn: the keeper lets the beds (here.inn_helper, as Game::dialogueRoleFacts has it).
                    keeper = next((q['name'] for q in people if q.get('job') == 'innkeeper' and q.get('role') == 'merchant'
                                   and q.get('_home') == p.get('_home')), 'the keeper')
                    helper = {'here.inn_helper': 'yes', 'here.innkeeper': keeper, 'here.inn': 'no', 'here.bed_ok': 'no'}
                    nodes = [(c, label, {**facts, **helper}) for c, label, facts in nodes if c != 'dlg.inn.roomed']
                sections.insert(0, (tree, nodes))
            for tree, nodes in sections:
                out.append(f'- **{tree}**')
                for concept, label, facts in nodes:
                    seed += 1
                    line = say(talk_check, concept, p, facts, dialect_of, seed)
                    out.append(f'  - *{label}* "{line}"')
            out.append('')
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out).write_text('\n'.join(out) + '\n')
    print(f'Wrote {args.out}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
