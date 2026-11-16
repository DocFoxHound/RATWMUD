#!/usr/bin/env python3
"""The truth lint (Docs/Design/84 §7.8, §15.8, Phase 1): every line of the talk library and the dialogue trees whose
words take something for granted (Data/Talk/truth.json) must have it behind them in its entry's criteria, its line's
tags or its say[...] (a fog bell needs fog; "my nets" a fisher), and no line's words may hold an identifier.

  python3 tools/talk_lint.py [--root Data/Talk] [--baseline Tests/fixtures/talk_lint_baseline.json] [--bless] [--list]

What it finds is counted against the committed baseline: a finding not in it fails (exit 1), so the count may only fall
(Phase 1 counted them and froze the count: "T2 counted and frozen as the baseline"); --bless writes the baseline (say
why in the commit); --list prints every finding. Only reads.
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import dialect_check  # noqa: E402
import talk_truth  # noqa: E402

ROOT = HERE.parent
BASELINE = ROOT / 'Tests' / 'fixtures' / 'talk_lint_baseline.json'
TOKEN = re.compile(r'^(!?)([a-z_]+(?:\.[a-z_.]+)?)(!=|<=|>=|=|<|>)?(.*)$')


def schema(root: Path) -> dict:
    return json.loads((root / 'facts.json').read_text())


def bare_word(word: str, facts: dict) -> tuple[str, set] | None:
    """A tag's bare word as the loader reads it (RatwTalk.cpp, bareWord): a sex, stage, role, job, band, time or weather."""
    person = facts.get('person', {})
    for name in ('sex', 'stage', 'role', 'job'):
        if isinstance(person.get(name), list) and word in person[name]:
            return f'self.{name}', {word}
    if word in facts.get('pair', {}).get('band', []):
        return 'pair.band', {word}
    for name in ('time', 'weather'):
        if word in facts.get('here', {}).get(name, []):
            return f'here.{name}', {word}
    return None


def criteria(tokens: list, who: str, scene: bool, facts: dict, tag: bool = False) -> list:
    """(key, op, values) for each criterion, the keys as the line's speaker sees them: self.*, other.*, here.*..."""
    person = set(facts.get('person', {}))
    out = []
    for raw in tokens:
        m = TOKEN.match(raw)
        if not m:
            continue
        neg, key, op, rest = m.groups()
        if not op:
            if tag and '.' not in key:
                found = bare_word(key, facts)
                if found:
                    out.append((found[0], '=', found[1]))
            elif not neg:
                out.append((key if '.' in key else key, 'has', set()))
            continue
        values = set(rest.split('|'))
        parts = key.split('.', 1)
        if len(parts) == 2 and parts[0] in ('a', 'b', 'self', 'other'):
            ns, name = parts
            if scene:
                me = ns == who or (ns == 'self' and who == 'a') or (ns == 'other' and who == 'b')
            else:
                me = ns in ('self', 'a')
            key = ('self.' if me else 'other.') + name
        elif len(parts) == 1 and parts[0] in person:
            key = 'self.' + parts[0] if (tag or not scene or who == 'a') else 'other.' + parts[0]
        elif len(parts) == 1:
            for ns in ('here', 'pair', 'world', 'topic'):
                if parts[0] in facts.get(ns, {}):
                    key = f'{ns}.{parts[0]}'
                    break
        out.append((key, op, values))
    return out


def held(when: list, implied: dict) -> list:
    """What the concepts an entry answers hold of themselves (truth.json `concepts`), as criteria: true of the moment
    whichever of them asked."""
    concepts = set()
    for token in when:
        if token.startswith('concept='):
            concepts |= set(token[8:].split('|'))
    if not concepts or not all(c in implied for c in concepts):
        return []
    return [(n.key, n.op, n.values) for c in sorted(concepts)[:1] for n in implied[c]]


def lines(root: Path, facts: dict):
    """Every line alternative with what stands behind it: (where, entry, text, [criteria])."""
    implied = talk_truth.concepts(root)
    for e in dialect_check.entries(root):
        for t in e['turns']:
            crit = criteria(e['when'], t['who'], e['scene'], facts) + criteria(t['tags'], t['who'], e['scene'], facts, tag=True)
            crit += held(e['when'], implied)
            for text in t['texts']:
                yield f"{e['file']}:{t['line']}", e['id'], text, crit
    # (A rule's say[...] variants: dialect_check reads only plain `say`.)
    for f in sorted(root.rglob('*.talk')):
        entry, when = None, []
        for number, raw in enumerate(f.read_text().splitlines(), 1):
            line = raw.strip()
            if line.startswith(('scene ', 'rule ')):
                entry, when = line.split(None, 1)[1].strip(), []
            elif line.startswith('when ') and entry:
                when += line[5:].split()
            elif line.startswith('say[') and entry:
                m = re.match(r'^say\[([^\]]*)\]\s*(.*)$', line)
                if m:
                    crit = criteria(when, 'a', False, facts) + criteria(m.group(1).split(), 'a', False, facts, tag=True)
                    crit += held(when, implied)
                    for text in dialect_check.split_alts(m.group(2)):
                        yield f'{f.relative_to(root)}:{number}', entry, text, crit
    for t in dialect_check.trees(root):
        if t['kind'] != 'say':
            continue
        crit = criteria(t['criteria'], 'a', False, facts, tag=True)
        for text in t['texts']:
            yield f"dialogue/{t['file']}:{t['line']}", f"{t['tree']}.{t['node']}", text, crit


def backed(rule: talk_truth.Rule, crit: list) -> bool:
    def one(need):
        return any(need.implied_by(k, op, v) for k, op, v in crit)
    if rule.needs and not all(one(n) for n in rule.needs):
        return False
    return not rule.needs_any or any(one(n) for n in rule.needs_any)


def findings(root: Path) -> list[dict]:
    facts = schema(root)
    rules, ids = talk_truth.load(root)
    out = []
    for where, entry, text, crit in lines(root, facts):
        words = talk_truth.SYMBOL.sub(' ', text)
        if ids.search(words):
            out.append({'rule': 'ids', 'entry': entry, 'where': where, 'text': text})
        for r in talk_truth.matches(rules, words):
            if not backed(r, crit):
                out.append({'rule': r.id, 'entry': entry, 'where': where, 'text': text})
    return out


def key(f: dict) -> str:
    return f"{f['rule']}|{f['entry']}|{f['text']}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--root', type=Path, default=talk_truth.TALK)
    ap.add_argument('--baseline', type=Path, default=BASELINE)
    ap.add_argument('--bless', action='store_true')
    ap.add_argument('--list', action='store_true')
    a = ap.parse_args()
    try:
        found = findings(a.root)
    except (ValueError, json.JSONDecodeError, re.error) as e:
        print(f'talk_lint: {e}', file=sys.stderr)
        return 1
    by_rule = collections.Counter(f['rule'] for f in found)
    if a.list:
        for f in sorted(found, key=lambda f: (f['rule'], f['where'])):
            print(f"{f['rule']:16} {f['where']:48} {f['entry']}: {f['text']}")
    if a.bless:
        a.baseline.write_text(json.dumps({'about': 'talk_lint.py findings frozen (Docs/Design/84 §7.8, Phase 1): the truth lexicon '
                                                   'counted over the library; a new one fails, the list may only shrink.',
                                          'count': len(found), 'by_rule': dict(sorted(by_rule.items())),
                                          'findings': sorted(key(f) for f in found)}, indent=1) + '\n')
        print(f'talk_lint: {len(found)} findings blessed into {a.baseline}')
        return 0
    known = set()
    if a.baseline.exists():
        known = set(json.loads(a.baseline.read_text()).get('findings', []))
    new = [f for f in found if key(f) not in known]
    gone = len(known - {key(f) for f in found})
    print(f"talk_lint: {len(found)} lines take for granted what nothing behind them holds ({', '.join(f'{k} {n}' for k, n in sorted(by_rule.items())) or 'none'}); "
          f"{len(new)} new, {gone} gone since the baseline")
    for f in new[:30]:
        print(f"  NEW {f['rule']}: {f['where']} {f['entry']}: {f['text']}")
    return 1 if new else 0


if __name__ == '__main__':
    sys.exit(main())
