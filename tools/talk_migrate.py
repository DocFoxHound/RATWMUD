#!/usr/bin/env python3
"""Short stock lines into backchannel symbols (Docs/Design/84 §10.2, Phase 7).

The library says "Good." 201 times, "Always." 162 and "It is." 106, word for word in every voice, so a Coldwater wolf
answers exactly as a Ridgemere one. Each such line becomes a symbol (`#bc_good#`) whose value in plain speech is exactly
the old words, so plain speakers sound as they did (checked: --check), while each dialect gives its own ("Fair catch.",
"Sound.", "Full weight.").

The symbols are plain.grammar's `bc_*` lines, one alternative each (the old words); a dialect's `bc_*` are its own way of
saying them. This tool only replaces whole alternatives in the .talk files (after a conversation tag, if one leads);
it never changes a word inside a longer line, and leaves the shrugs that end a talk (settings.json `ends`) alone.

  python3 tools/talk_migrate.py --candidates [N]  the most repeated short lines not yet symbols (to choose more)
  python3 tools/talk_migrate.py                   report what would change
  python3 tools/talk_migrate.py --write           replace them in place
  python3 tools/talk_migrate.py --check           exit 1 unless: every bc_ symbol's plain value is one line, every
                                                  dialect's bc_ is one plain has, and no line the library says is
                                                  still one of them word for word (ctest talk_migrate)
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TALK = ROOT / 'Data' / 'Talk'
SYMBOL = re.compile(r'^(bc_[a-z0-9_]+)\s*=\s*(.*)$')
SCENE_LINE = re.compile(r'^(\s*[ab]\??[^:]*:\s*)(.*)$')


def grammar_symbols(path: Path) -> dict[str, list[str]]:
    """A grammar file's bc_* symbols: {name: [alternatives]}."""
    out = {}
    for line in path.read_text().splitlines():
        m = SYMBOL.match(line.strip())
        if m:
            out.setdefault(m.group(1), []).extend(a.strip() for a in m.group(2).split('|') if a.strip())
    return out


def backchannels(root: Path = TALK) -> dict[str, str]:
    """{old words: symbol} from plain.grammar."""
    return {alts[0]: name for name, alts in grammar_symbols(root / 'grammar' / 'plain.grammar').items() if len(alts) == 1}


def shrugs(root: Path = TALK) -> set[str]:
    return set(json.loads((root / 'settings.json').read_text()).get('ends', []))


def split_alt(alt: str) -> tuple[str, str, str]:
    """(leading space and tag, words, trailing space) of one alternative."""
    m = re.match(r'^(\s*(?:\{[^}]*\}\s*)?)(.*?)(\s*)$', alt)
    return m.group(1), m.group(2), m.group(3)


def rewrite(text: str, table: dict[str, str]) -> tuple[str, int]:
    """One .talk file's text with each whole alternative that is a backchannel's old words replaced by its symbol."""
    out, n = [], 0
    for line in text.split('\n'):
        s = line.lstrip()
        head = None
        if s.startswith('say ') and not s.startswith('#'):
            i = line.index('say ') + 4
            head, body = line[:i], line[i:]
        elif not s.startswith('#'):
            m = SCENE_LINE.match(line)
            if m and re.match(r'^\s*[ab]\??(?:@[a-z0-9_]+)*(?:\[[^\]]*\])?(?:\s+#[a-z0-9_]+)*\s*:', line):
                head, body = m.group(1), m.group(2)
        if head is None:
            out.append(line)
            continue
        alts = []
        for alt in body.split('|'):
            lead, words, trail = split_alt(alt)
            if words in table:
                words = '#' + table[words] + '#'
                n += 1
            alts.append(lead + words + trail)
        out.append(head + '|'.join(alts))
    return '\n'.join(out), n


def candidates(root: Path, limit: int) -> list[tuple[str, int]]:
    sys.path.insert(0, str(ROOT / 'tools'))
    import talk_files
    lib = talk_files.read(root)
    count = collections.Counter()
    for e in lib['entries']:
        for _, _, _, alts in e['lines']:
            for a in alts:
                if len(a.split()) <= 4 and '#' not in a:
                    count[a] += 1
    taken, ends = backchannels(root), shrugs(root)
    return [(t, n) for t, n in count.most_common() if t not in taken and t not in ends][:limit]


def check(root: Path) -> list[str]:
    problems = []
    plain = grammar_symbols(root / 'grammar' / 'plain.grammar')
    for name, alts in plain.items():
        if len(alts) != 1:
            problems.append(f'plain.grammar: {name} has {len(alts)} values; a backchannel is exactly the old words')
    for g in sorted((root / 'grammar').glob('*.grammar')):
        if g.stem == 'plain':
            continue
        for name, alts in grammar_symbols(g).items():
            if name not in plain:
                problems.append(f'{g.name}: {name} is no backchannel plain has')
            elif plain[name][0] in alts:
                problems.append(f'{g.name}: {name} repeats plain\'s words ("{plain[name][0]}"): leave it out instead')
    table = backchannels(root)
    for f in sorted(root.rglob('*.talk')):
        _, n = rewrite(f.read_text(), table)
        if n:
            problems.append(f'{f.relative_to(root)}: {n} lines are still a backchannel word for word (run --write)')
    clash = set(table) & shrugs(root)
    if clash:
        problems.append(f'settings.json ends: {sorted(clash)} are shrugs, matched by their words: never a symbol')
    return problems


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--write', action='store_true')
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--candidates', nargs='?', type=int, const=40)
    ap.add_argument('--root', type=Path, default=TALK)
    a = ap.parse_args(argv)
    if a.candidates:
        for t, n in candidates(a.root, a.candidates):
            print(f'{n:5}  {t}')
        return 0
    if a.check:
        problems = check(a.root)
        for p in problems:
            print('talk_migrate:', p)
        if not problems:
            print(f'talk_migrate: {len(backchannels(a.root))} backchannels, plain identical, none left unconverted')
        return 1 if problems else 0
    table = backchannels(a.root)
    total = files = 0
    for f in sorted(a.root.rglob('*.talk')):
        text = f.read_text()
        new, n = rewrite(text, table)
        if n:
            total += n
            files += 1
            if a.write:
                f.write_text(new)
    print(f'{total} lines in {files} files are backchannels ({len(table)} symbols){": replaced" if a.write else ""}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
