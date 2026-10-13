#!/usr/bin/env python3
"""Writes each resident's personality and character table (Docs/Design/62-talk.md, decisions 6 and 10).

From everything the world knows of a resident (as the game works it out: talk_check --residents --json, after a game
day so families are taken in) and worldgen's description, personality, backstory and greeting, the story model writes:

  persona   who they are, for the model whenever it voices them (live.npc_voice.persona)
  words     their own words: a saying, what they call their spouse and friends, their oath... (their grammar layer)
  lines     a few lines of their own: at work, to a traveller they know, a remark, at home (their rules)

Each table is checked as the game loads it (talk_check --character) before it is kept; one refused is asked for again
with the reasons, then kept as the personality alone. Every draft is also written to artifacts/voices/<id>.json.

Paid: one call a resident. --dry-run prints the prompt and inputs and calls nothing.

  python3 tools/voice_tables.py --sample --dry-run         ten residents of different kinds: what would be asked
  python3 tools/voice_tables.py --sample --write           ...written (paid: ten calls)
  python3 tools/voice_tables.py --ids a,b --write
  python3 tools/voice_tables.py --all --missing --write --yes     everyone without one yet (paid: a call each)
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

import npc_bridge as bridge
import npc_mind as mind
import talk_files
import world_db

ROOT = Path(__file__).resolve().parent.parent
CONFIG = ROOT / 'Saved' / 'Config' / 'RATWNPCAI.local.json'
DRAFTS = ROOT / 'artifacts' / 'voices'
GRAMMARS = ROOT / 'Data' / 'Talk' / 'grammar'
WORDS = ('saying', 'dear', 'pet', 'oath', 'greet', 'farewell', 'agree', 'refuse', 'surprise', 'annoyed')
HABITS = {'saying': '!', 'dear': '!', 'oath': '!', 'pet': '!!'}
MOMENTS = {
    'work': 'rule work\nwhen concept=bark working=yes\ncooldown 6h',
    'greet_known': 'rule greet_known\nwhen concept=player.greet topic.meeting=known\ncooldown 1d',
    'remark': 'rule remark\nwhen concept=talk.smalltalk\ncooldown 1d',
    'home': 'rule home\nwhen concept=bark working=no\ncooldown 8h',
}
FILLS = {'{saying}': '#saying#', '{oath}': '#oath#', '{dear}': '#dear#', '{pet}': '#pet#', '{player}': '#topic.player#'}

RULES = """You write the people of Runs Against the World, a text-first roleplaying world of quadrupedal wolves in a realm
of seven regions: Ridgemere, the rain city on its hills above Ridgemere Sound, its Heights for the rich and great Houses,
its Commons, its Wharf and Docks, and the Sump at the bottom where the poor live; Frosthollow, mining in a high snowy
valley; Coldwater, fishers on the lake Glasswater; Oakhaven in the Warmwood; Mirebridge in the Sallow Fens; Larkfield,
market town of the downs; Southmeadow, grain fields and orchards in the far south. Four great Houses (Greatly, Fell,
Grayrock, Mossgrave) own most trades.

The user JSON is everything the world knows of one resident: background, not a checklist. Weave in only what shapes
them, in their own world's terms (never "standing" or "civilian", never counts of days). Write who they are, consistent
with every fact in it:
their age, family, standing (wealth, House, trade, quarter), where they grew up and how broadly they still speak like it,
their traits and quirk, worldgen's personality (its two adjectives and quirk) and backstory, which you deepen, never
contradict. Never invent named people, places or events beyond those given. They are wolves: no hands, nobody stands
upright; paws, tails, ears, fur.

persona: 80 to 160 words, present tense, third person: temperament and manner; how they speak (pace, words, what they
won't say); what they care about and fear; their family and standing as they feel it; one contradiction in them.
words: 2 to 5 of their own, each a symbol from: saying (a stock phrase of their own, a whole saying), dear (what they
call their spouse: only if married), pet (what they call friends: a name), oath (their own exclamation), greet,
farewell, agree, refuse, surprise, annoyed (whole sayings). 1 to 3 short alternatives each, in the spirit of their
dialect's words given, never the dialect's own words again.
lines: 2 to 4, each for one moment: work (a remark at their work: only if they work), greet_known (to a traveller they
know: write {player} where the name goes), remark (in passing talk, about what matters to them), home (at home or off
work). 1 or 2 alternatives each, at most 140 characters. Use {saying}, {oath}, {dear}, {pet} where natural. Plain
speech: no narration, no action tags, no quotation marks, no # or [ ] characters."""


def schema() -> dict:
    words = {'type': 'object', 'additionalProperties': False, 'required': ['symbol', 'words'],
             'properties': {'symbol': {'type': 'string', 'enum': list(WORDS)},
                            'words': {'type': 'array', 'items': {'type': 'string'}}}}
    line = {'type': 'object', 'additionalProperties': False, 'required': ['moment', 'say'],
            'properties': {'moment': {'type': 'string', 'enum': list(MOMENTS)},
                           'say': {'type': 'array', 'items': {'type': 'string'}}}}
    return {'type': 'object', 'additionalProperties': False, 'required': ['persona', 'words', 'lines'],
            'properties': {'persona': {'type': 'string'}, 'words': {'type': 'array', 'items': words},
                           'lines': {'type': 'array', 'items': line}}}


def talk_check() -> Path:
    return talk_files.talk_check()


def residents() -> dict[str, dict]:
    """Every resident's facts, as the game works them out, after a game day (families taken in at its turn)."""
    run = subprocess.run([str(talk_check()), '--world', 'dev', '--run', '24', '--residents', '--json'], capture_output=True,
                         text=True, timeout=900, check=True)
    return {f['id']: f for f in (json.loads(l) for l in run.stdout.splitlines() if l.startswith('{'))}


def dialect_words(region: str) -> str:
    f = GRAMMARS / f'{region}.grammar'
    if not f.exists():
        return 'the realm\'s plain common speech'
    lines = [l for l in f.read_text().splitlines() if l and not l.startswith('#') and not l.startswith('grammar')]
    return '; '.join(lines[:14])


def sample(people: dict[str, dict]) -> list[str]:
    """Ten of different kinds, one each, where there is one."""
    kinds = [
        lambda f: f.get('quarter') == 'heights' and f.get('stage') == 'adult',
        lambda f: f.get('quarter') == 'sump' and f.get('stage') == 'adult',
        lambda f: f.get('from') == 'frosthollow' and f.get('job') == 'miner',
        lambda f: f.get('from') == 'coldwater' and f.get('job') == 'fisher',
        lambda f: f.get('job') == 'merchant' and f.get('from') == 'larkfield',
        lambda f: f.get('job') == 'farmer' and f.get('from') == 'southmeadow',
        lambda f: f.get('from') == 'mirebridge' and f.get('accent') == 'broad',
        lambda f: f.get('stage') == 'young' and (f.get('mother') or f.get('father')),
        lambda f: f.get('stage') == 'old',
        lambda f: f.get('job') in ('guard', 'priest', 'innkeeper'),
    ]
    chosen = []
    for kind in kinds:
        for id_, f in sorted(people.items()):
            if id_ not in chosen and kind(f):
                chosen.append(id_)
                break
    return chosen


# The game's own bookkeeping, not who they are: left out of what the model is told.
INTERNAL = {'id', 'wealth_days', 'role', 'shift', 'tone', 'working', 'errand', 'travelling', 'on_watch', 'fighting', 'hungry',
            'tired', 'injured', 'fought', 'robbed', 'owes', 'on_dole', 'homeless', 'trade', 'stage', 'business'}


def dialect_alternatives(region: str) -> set[str]:
    """Every word the region's dialect (and plain) already has: a person's own words are their own."""
    out = set()
    for f in (GRAMMARS / f'{region}.grammar', GRAMMARS / 'plain.grammar'):
        if f.exists():
            for line in f.read_text().splitlines():
                if '=' in line and not line.startswith('#'):
                    out |= {w.strip().rstrip('.').lower() for w in line.split('=', 1)[1].split('|')}
    return out


def ask(person: dict, row: dict) -> dict:
    keep = {k: v for k, v in person.items() if k not in INTERNAL and v not in ('', 'no', 'none')}
    return {'resident': keep, 'description': row['description'], 'personality': row['personality'],
            'backstory': row['backstory'], 'greeting': row['greeting'], 'work': row['work_label'],
            'dialect_words': dialect_words(person.get('from', ''))}


def table(draft: dict, region: str = '') -> tuple[str, str]:
    """(grammar, rules) in the library's syntax from a draft: at most five of their own words, none their dialect has
    already (a person's layer is always used, whatever their accent: a dialect's word there would make them broad)."""
    clean = lambda t: ' '.join(re.sub(r'[#\[\]|"]', '', t).split())[:140]
    known = dialect_alternatives(region)
    grammar = []
    for w in draft.get('words', []):
        words = [clean(x) for x in w.get('words', []) if clean(x) and clean(x).rstrip('.').lower() not in known][:3]
        if words and w.get('symbol') in WORDS and len(grammar) < 5:
            grammar.append(f"{w['symbol']}{HABITS.get(w['symbol'], '')} = " + ' | '.join(words))
    rules = []
    for line in draft.get('lines', []):
        say = []
        for x in line.get('say', [])[:2]:
            x = re.sub(r'[#\[\]|"]', '', x)
            for k, v in FILLS.items():
                x = x.replace(k.strip('{}').join('{}'), v)
            x = ' '.join(x.split())[:140]
            if x and '{' not in x:
                say.append(x)
        if say and line.get('moment') in MOMENTS:
            rules.append(MOMENTS[line['moment']] + '\nsay ' + ' | '.join(say))
    return '\n'.join(grammar) + ('\n' if grammar else ''), '\n\n'.join(rules) + ('\n' if rules else '')


def check(npc: str, grammar: str, rules: str) -> str:
    """'' if the game would load it; else why not."""
    return talk_files.check_character(npc, grammar, rules)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    pick = parser.add_mutually_exclusive_group(required=True)
    pick.add_argument('--sample', action='store_true', help='ten residents of different kinds')
    pick.add_argument('--ids', help='comma-separated resident IDs')
    pick.add_argument('--all', action='store_true', help='everyone (with --yes)')
    parser.add_argument('--missing', action='store_true', help='only those without a personality yet')
    parser.add_argument('--dry-run', action='store_true', help='print what would be asked; call nothing')
    parser.add_argument('--write', action='store_true', help='keep the tables in DEV (live.npc_voice)')
    parser.add_argument('--yes', action='store_true', help='needed with --all: a paid call for every resident')
    parser.add_argument('--model', choices=('story', 'light', 'voice'), default='story')
    args = parser.parse_args()
    if args.all and not (args.yes or args.dry_run):
        parser.error('--all makes a paid call for every resident: add --yes')
    people = residents()
    with world_db.connect('dev', 'editor') as conn:
        world = conn.execute('SELECT id FROM world.worlds').fetchone()[0]
        rows = {r[0]: dict(zip(('id', 'description', 'personality', 'backstory', 'greeting', 'work_label', 'persona'), r))
                for r in conn.execute('SELECT n.id, n.description, n.personality, n.backstory, n.greeting, n.work_label, '
                                      "coalesce(v.persona, '') FROM live.npcs n LEFT JOIN live.npc_voice v "
                                      'ON v.world_id = n.world_id AND v.npc_id = n.id WHERE n.world_id = %s', (world,)).fetchall()}
        ids = sample(people) if args.sample else args.ids.split(',') if args.ids else sorted(people)
        ids = [i for i in ids if i in people and i in rows and not (args.missing and rows[i]['persona'])]
        print(f'{len(ids)} residents: ' + ', '.join(ids[:12]) + (' ...' if len(ids) > 12 else ''))
        if args.dry_run:
            print('\n--- system\n' + RULES + '\n\n--- user (the first)\n' + json.dumps(ask(people[ids[0]], rows[ids[0]]), indent=2,
                                                                                   ensure_ascii=False) if ids else '')
            return 0
        config = bridge.load_config(CONFIG)
        provider = mind.OpenAIProvider(config)
        model = {'story': config.story_model or config.light, 'light': config.light, 'voice': config.model}[args.model]
        DRAFTS.mkdir(parents=True, exist_ok=True)
        kept = refused = 0
        for npc in ids:
            user = ask(people[npc], rows[npc])
            draft, _ = provider.complete(RULES, json.dumps(user, ensure_ascii=False), 'npc_character', schema(), 2000, 120, model=model)
            grammar, rules = table(draft, people[npc].get('from', ''))
            why = check(npc, grammar, rules)
            if why:
                user['refused'] = why[:600]
                draft, _ = provider.complete(RULES, json.dumps(user, ensure_ascii=False), 'npc_character', schema(), 2000, 120, model=model)
                grammar, rules = table(draft, people[npc].get('from', ''))
                why = check(npc, grammar, rules)
            if why:
                grammar = rules = ''
                refused += 1
            (DRAFTS / f'{npc}.json').write_text(json.dumps({'input': user, 'draft': draft, 'grammar': grammar, 'rules': rules,
                                                           'refused': why}, indent=2, ensure_ascii=False) + '\n')
            persona = ' '.join(draft.get('persona', '').split())[:4000]
            if args.write and persona:
                conn.execute("INSERT INTO live.npc_voice (world_id, npc_id, persona, grammar, rules, written_by) "
                             'VALUES (%s, %s, %s, %s, %s, %s) ON CONFLICT (world_id, npc_id) DO UPDATE SET persona = excluded.persona, '
                             'grammar = excluded.grammar, rules = excluded.rules, written_by = excluded.written_by, updated_at = now()',
                             (world, npc, persona, grammar, rules, model))
                kept += 1
            print(f'  {npc}: {len(persona)} characters, {grammar.count(chr(10))} words, {rules.count("rule ")} lines'
                  + (f' (table refused: {why.splitlines()[-1][:120]})' if why else ''))
        print(f'{kept} kept in DEV, {refused} tables refused (personality kept alone); drafts in {DRAFTS}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
