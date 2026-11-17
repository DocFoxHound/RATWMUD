#!/usr/bin/env python3
"""The equivalence test for Phase 7's conversion (Docs/Design/84 §18, P7: "the same possible lines, old against new").

Phase 7 changes how the library is written, not what it can say: short markers became conversation tags
(tools/talk_conv.py), stock short lines became backchannel symbols whose plain value is the old words
(tools/talk_migrate.py), and every entry gained a `does` line and, where it isn't its move's, a `tone` (talk_tag.py).
This reads the library at a git revision (the phase before) and the working tree, entry by entry, and checks:

- every entry is still there, with the same criteria (a converted marker's read `pair.mem.k` is now `conv.k`), the
  same hand-on, limits, priority and weight, and the same remembering less the converted markers;
- every line a plain speaker could say before, they can say still: each alternative of each line, its symbols
  backchannels and discourse words expanded to plain's values, is one the old library had, and each old one is still
  there; a converted marker's tag leads every alternative of its rule;
- an alternative that is new says a discourse word (§10.3: a vent line led in by `#vent#`), and an entry that is new
  is listed; a line changed by hand is named in EDITED below with why.

  python3 tools/talk_equiv.py [--against REV]   (default: origin/r3-talk-p6) exit 1 on any difference
"""
from __future__ import annotations

import argparse
import itertools
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TALK = ROOT / 'Data' / 'Talk'
DEFAULT = 'origin/r3-talk-p6'
# Lines changed by hand in Phase 7, each with why (the only differences allowed beyond the conversion).
EDITED = {
    'e1_food_rm_fog_chowder': 'said "in this rain" in fog (its criteria allow rain, fog or storm): "and no wonder" now',
    'ag_again_any': '"Back again?" once a day (cooldown 1d): every opener now says what it does, so the again_today frame '
                    'put it before 83% of a busy place\'s talks',
    'ag_again_any2': 'the same',
    'w7s_shore_work': '"My nets are tangled past saving." from a weaver at the shore (talk_lint\'s fisher_self, frozen since '
                      'Phase 1): "Look at those nets. Tangled past saving." now, true for anyone there',
    # Phase 8, session 1 (doc 84 §33.2): the town's gossip answers, met by the card answering a death.
    'n7_ga2_merry': 'laughed at a death ("Ha! I\'ll never look at Gunnar Harrowgate the same.", C4): not for a death, a '
                    'hurt or a victim now (topic.claim_kind!=death|violence|victim)',
    'n7_ga2_dry': 'the same ("And the moon\'s made of cheese" to a death)',
    'n7_ga2_sly': 'the same ("Interesting. Who else knows?" to a death)',
    'n7_ga2_chatty': 'the same ("I knew it! Didn\'t I say?" to a death)',
    'n7_ga2_young': 'the same ("I\'m telling everyone!" to a death)',
    'n7_ga2_old': 'the same ("It\'ll be forgotten by the next festival" to a death)',
    'n7_ga2_nervy': 'the same ("If they hear we said it..." to a death)',
    'n7_ga2_cold': '"Doesn\'t surprise me." to a death: not for a death now (n7_ga2_death_cold is the cold wolf\'s)',
    'n7_ga2_devout': '"The gods hear tattle too" to a death: not for a death now (n7_ga2_death_devout is the devout\'s)',
    'n7_ga2_priest': '"I\'ll not hear it" to a death: the same',
    'n7_ga2_warm': '"I hope they\'re all right" of one wolf: #topic.they# (he or she) now',
    'n7_ga2_death': '"I saw them only the other day": "they" for one wolf, and an eyewitness who saw nothing; "No. Not '
                    '#topic.them#. Surely not." now',
    'n7_ga2_death_devout': '"May they find their rest" and "keep them" of one wolf: #topic.they#, #topic.them# now',
    'n7_ga2_death_cold': '"I hardly knew them" of one wolf: #topic.them# now',
}
DISCOURSE = {'hedge', 'account', 'ack', 'pivot', 'anyway', 'later', 'resume', 'preclose', 'excuse', 'knew', 'sympathy',
             'concede', 'insist', 'warn', 'welcome_back', 'welcome', 'sorry', 'forgive', 'deflect', 'no_matter',
             'go_ahead', 'put_off', 'vent', 'reassure', 'congrats', 'demur', 'doubt', 'listen'}
HEAD = re.compile(r'^(\s*[ab]\??(?:@[a-z0-9_]+)*(?:\[[^\]]*\])?(?:\s+#[a-z0-9_]+)*\s*):(.*)$')


def git_files(rev: str) -> dict[str, str]:
    names = subprocess.run(['git', '-C', str(ROOT), 'ls-tree', '-r', '--name-only', rev, 'Data/Talk'], capture_output=True,
                           text=True, check=True).stdout.split()
    out = {}
    for n in names:
        if n.endswith(('.talk', '.grammar')):
            out[n[len('Data/Talk/'):]] = subprocess.run(['git', '-C', str(ROOT), 'show', f'{rev}:{n}'], capture_output=True,
                                                        text=True, check=True).stdout
    return out


def tree_files() -> dict[str, str]:
    return {str(f.relative_to(TALK)): f.read_text() for f in sorted(TALK.rglob('*')) if f.suffix in ('.talk', '.grammar')}


def plain_symbols(files: dict[str, str]) -> dict[str, list[str]]:
    out = {}
    for line in files.get('grammar/plain.grammar', '').splitlines():
        m = re.match(r'^([a-z0-9_]+)\s*=\s*(.*)$', line.strip())
        if m:
            out.setdefault(m.group(1), []).extend(a.strip() for a in m.group(2).split('|') if a.strip())
    return out


def entries(files: dict[str, str]) -> dict[str, dict]:
    out = {}
    for name, text in files.items():
        if not name.endswith('.talk'):
            continue
        e = None
        for raw in text.split('\n'):
            line = raw.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith(('rule ', 'scene ')):
                e = {'file': name, 'kind': line.split()[0], 'keys': [], 'lines': [], 'remember': []}
                out[line.split()[1]] = e
                continue
            if e is None:
                continue
            if line.startswith(('does ', 'tone ')) or line in ('does', 'tone'):
                continue
            if line.startswith('remember '):
                e['remember'].append(line)
            elif line.startswith('say ') or line == 'say':
                e['lines'].append(('say', [a.strip() for a in line[3:].split('|')]))
            else:
                m = HEAD.match(line)
                if m and e['kind'] == 'scene':
                    e['lines'].append((re.sub(r'\s+', ' ', m.group(1).strip()), [a.strip() for a in m.group(2).split('|')]))
                else:
                    e['keys'].append(line)
    return out


def converted_keys(old: dict, new: dict) -> set[str]:
    keys = set()
    for id, o in old.items():
        n = new.get(id)
        if not n:
            continue
        for r in set(o['remember']) - set(n['remember']):
            m = re.match(r'^remember (pair|other): ([a-z0-9_]+)', r)
            if m:
                keys.add(m.group(2))
    return keys


def norm_key(line: str, keys: set[str]) -> str:
    return re.sub(r'(?<![a-z0-9_.])(?:pair|self|other)\.mem\.(' + '|'.join(map(re.escape, sorted(keys))) + r')(?![a-z0-9_])',
                  r'conv.\1', line) if keys else line


def strip_tag(alt: str) -> tuple[str, str]:
    m = re.match(r'^\{([^}]*)\}\s*(.*)$', alt)
    return (m.group(1), m.group(2)) if m else ('', alt)


def expansions(text: str, plain: dict[str, list[str]], limit: int = 400) -> set[str]:
    """The text with each symbol Phase 7 added (a backchannel, a discourse word: `plain` holds only those) replaced by
    each of plain's values; the rest left as written."""
    parts = re.split(r'(#[a-z0-9_]+(?:\.[a-z]+)?#)', text)
    choices = []
    for p in parts:
        m = re.match(r'^#([a-z0-9_]+)(\.[a-z]+)?#$', p)
        if m and m.group(1) in plain:
            vals = plain[m.group(1)]
            if m.group(2) == '.capitalize':
                vals = [v[:1].upper() + v[1:] for v in vals]
            choices.append(vals)
        else:
            choices.append([p])
    out = set()
    for combo in itertools.product(*choices):
        out.add(''.join(combo))
        if len(out) >= limit:
            break
    return out


def compare(rev: str) -> list[str]:
    old_files, new_files = git_files(rev), tree_files()
    old, new = entries(old_files), entries(new_files)
    before = plain_symbols(old_files)
    plain = {k: v for k, v in plain_symbols(new_files).items() if k not in before}    # (Phase 7's own symbols.)
    keys = converted_keys(old, new)
    problems = []
    added = sorted(set(new) - set(old))
    for id in sorted(set(old) - set(new)):
        problems.append(f'{id}: gone')
    for id in sorted(set(old) & set(new)):
        o, n = old[id], new[id]
        where = f'{n["file"]}: {id}'
        if id not in EDITED and [norm_key(k, keys) for k in o['keys']] != [norm_key(k, keys) for k in n['keys']]:
            problems.append(f'{where}: its criteria or keywords changed beyond the conversion')
        gone = [r for r in o['remember'] if r not in n['remember']]
        tags = set()
        for r in gone:
            m = re.match(r'^remember (pair|other): ([a-z0-9_]+)(?:=(\S+))?\s+(\d+[mh])$', r)
            if not m or (m.group(4).endswith('h') and int(m.group(4)[:-1]) > 1):
                problems.append(f'{where}: "{r}" is gone but wasn\'t a short marker')
            else:
                tags.add(f'{m.group(2)}={m.group(3) or "yes"}')
        if [r for r in n['remember'] if r not in o['remember']]:
            problems.append(f'{where}: remembers something new')
        if len(o['lines']) != len(n['lines']):
            problems.append(f'{where}: {len(o["lines"])} lines before, {len(n["lines"])} now')
            continue
        for (oh, oalts), (nh, nalts) in zip(o['lines'], n['lines']):
            if norm_key(oh, keys) != norm_key(nh, keys):
                problems.append(f'{where}: a line\'s speaker or tags changed ({oh} / {nh})')
                continue
            olds = [strip_tag(a)[1] for a in oalts]
            possible = set()
            for a in nalts:
                tag, words = strip_tag(a)
                have = set(tag.split())
                if tags - have:
                    problems.append(f'{where}: "{words[:50]}" lacks the tags {sorted(tags - have)}')
                exp = expansions(words, plain)
                possible |= exp
                if not (exp & set(olds)) and id not in EDITED and not re.search(r'#(' + '|'.join(DISCOURSE) + r')(\.[a-z]+)?#', words):
                    problems.append(f'{where}: "{words[:60]}" is a new line with no discourse word')
            for a in olds:
                if a not in possible and id not in EDITED:
                    problems.append(f'{where}: "{a[:60]}" can no longer be said')
    for id in added:
        print(f'talk_equiv: new entry {new[id]["file"]}: {id}')
    return problems


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--against', default=DEFAULT)
    a = ap.parse_args(argv)
    try:
        problems = compare(a.against)
    except subprocess.CalledProcessError as e:
        print(f'talk_equiv: no revision {a.against} to compare with ({e.stderr.strip()[:200]})')
        return 1
    for p in problems:
        print('talk_equiv:', p)
    if not problems:
        print(f'talk_equiv: every line said before {a.against} can be said now, word for word for a plain speaker; '
              f'nothing else changed but tags, moves and tones ({len(EDITED)} entries changed by hand, each said why)')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
