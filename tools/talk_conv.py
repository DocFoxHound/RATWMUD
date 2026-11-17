#!/usr/bin/env python3
"""The library's short markers into conversation tags (Docs/Design/84 §8.7, Phase 7).

Before the turn engine a reply couldn't see which words its opener said, so a line remembered what it was about for the
pair, for a minute or an hour (`remember pair: jk=look 30m`), and the answer read it back (`pair.mem.jk=look`). The
marker outlived the talk: it leaked into the same two's next one, and it crowded the 32-entry talk memory. A talk has
facts of its own now (`conv.*`, §7.11), so each such marker becomes a tag on the line's words (`{jk=look} Have you...`),
set for the rest of the conversation when it is said and gone when it ends, and each read becomes `conv.jk=look`.

What is converted: every `remember pair:` (and `remember other:`) of an hour or less whose key is never remembered for
longer, in another scope, or read outside the talk library (the dialogue trees, the game). Kept as they are, in the talk
memory (§8.7's second tier): the long-lived keys (`served` 7 days, `grumble` 3, `nb` 3...), and any short one named in
KEEP below as meant to last past a talk (none is, today: every one of the 2,652 was read only within its exchange).

  python3 tools/talk_conv.py            report what would change (nothing written)
  python3 tools/talk_conv.py --write    convert the files in place
  python3 tools/talk_conv.py --check    exit 1 if any short marker that should be a tag is left (ctest talk_conv)
"""
from __future__ import annotations

import argparse
import collections
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TALK = ROOT / 'Data' / 'Talk'
SHORT_DAYS = 1 / 24                     # an hour or less: a talk's own (§8.7)
# Short markers meant to outlive a talk, kept as memory: {key: why}. (None: the long-lived keys, `served` and `grumble`
# among them, are remembered for days and were never markers.)
KEEP: dict[str, str] = {}

REMEMBER = re.compile(r'^remember (pair|other|self|town|x):\s*([a-z0-9_]+)(?:=(\S+))?(?:\s+(\S+))?\s*$')


def days(text: str | None) -> float:
    """A remembered fact's duration in days (no duration: until the game forgets it, as long as forever here)."""
    if not text or text == 'forever':
        return float('inf')
    m = re.match(r'^(\d+(?:\.\d+)?)([mhd])$', text)
    if not m:
        raise ValueError(f'not a duration: {text}')
    return float(m.group(1)) / {'m': 1440, 'h': 24, 'd': 1}[m.group(2)]


class Library:
    """Every .talk file's lines, the entries in them and their markers, read without the game (only to rewrite them)."""

    def __init__(self, root: Path = TALK):
        self.root = root
        self.files = {f: f.read_text().split('\n') for f in sorted(root.rglob('*.talk'))}
        self.markers = []                   # (file, line index, entry id, scene, scope, key, value, days)
        self.entry_of = {}                  # (file, line index) -> (entry id, scene)
        self.says = collections.defaultdict(list)     # (file, entry id) -> [line index of each say]
        for f, lines in self.files.items():
            entry, scene = None, False
            for i, raw in enumerate(lines):
                line = raw.strip()
                if line.startswith(('rule ', 'scene ')):
                    entry, scene = line.split()[1], line.startswith('scene ')
                    continue
                if entry is None or not line or line.startswith('#'):
                    continue
                self.entry_of[(f, i)] = (entry, scene)
                if line.startswith('say ') or line == 'say':
                    self.says[(f, entry)].append(i)
                m = REMEMBER.match(line)
                if m:
                    self.markers.append((f, i, entry, scene, m.group(1), m.group(2), m.group(3) or 'yes', days(m.group(4))))

    def plan(self) -> tuple[dict, dict]:
        """({key: scopes converted}, {key: why kept}) for every key a short pair/other marker sets."""
        by_key = collections.defaultdict(list)
        for m in self.markers:
            by_key[m[5]].append(m)
        outside = outside_reads()
        convert, kept = {}, {}
        for key, ms in sorted(by_key.items()):
            short = [m for m in ms if m[4] in ('pair', 'other') and m[7] <= SHORT_DAYS]
            if not short:
                continue
            if key in KEEP:
                kept[key] = KEEP[key]
            elif len(short) != len(ms):
                longest = max(m[7] for m in ms)
                kept[key] = (f'also remembered for {longest:g} days' if longest != float('inf') else 'also remembered for good') \
                    if any(m[7] > SHORT_DAYS for m in ms) else 'also remembered in another scope'
            elif any(m[3] for m in short):
                kept[key] = 'remembered by a scene'
            elif key in outside:
                kept[key] = 'read outside the talk library: ' + ', '.join(sorted(outside[key]))
            else:
                convert[key] = {m[4] for m in short}
        return convert, kept

    def reads(self, key: str, scopes: set) -> re.Pattern:
        """The reads of a converted key: pair.mem.K for a pair marker; self.mem.K and other.mem.K for one remembered of the
        one spoken to (`remember other:`, read by them as self.mem.K)."""
        names = (['pair'] if 'pair' in scopes else []) + (['self', 'other'] if 'other' in scopes else [])
        return re.compile(r'(?<![a-z0-9_.])(' + '|'.join(names) + r')\.mem\.' + re.escape(key) + r'(?![a-z0-9_])')

    def convert(self) -> dict:
        """Rewrite the files' lines in memory; what changed, counted."""
        convert, kept = self.plan()
        tags = collections.defaultdict(list)          # (file, entry) -> [(key, value)] in the order written
        drop = set()
        for f, i, entry, scene, scope, key, value, d in self.markers:
            if key in convert and scope in convert[key] and d <= SHORT_DAYS:
                tags[(f, entry)].append((key, value))
                drop.add((f, i))
        stats = collections.Counter()
        problems = []
        for (f, entry), kv in tags.items():
            says = self.says.get((f, entry), [])
            if not says:
                problems.append(f'{f.relative_to(self.root)}: {entry} remembers {kv} with no say to tag')
                continue
            for i in says:
                self.files[f][i] = tag_say(self.files[f][i], kv)
                stats['say lines tagged'] += 1
            stats['rules'] += 1
        for f, i in drop:
            self.files[f][i] = None
            stats['markers'] += 1
        patterns = {k: self.reads(k, s) for k, s in convert.items()}
        for f, lines in self.files.items():
            for i, line in enumerate(lines):
                if line is None:
                    continue
                new = line
                for k, p in patterns.items():
                    new, n = p.subn('conv.' + k, new)
                    if n:
                        stats['comments rewritten' if line.lstrip().startswith('#') else 'reads'] += n
                lines[i] = new
        stats['keys'] = len(convert)
        stats['keys kept'] = len(kept)
        return {'stats': stats, 'kept': kept, 'convert': convert, 'problems': problems}

    def write(self):
        for f, lines in self.files.items():
            text = '\n'.join(line for line in lines if line is not None)
            if text != f.read_text():
                f.write_text(text)


def tag_say(line: str, kv: list[tuple[str, str]]) -> str:
    """`say A | B` with each alternative led by the tags ({k=v k2=v2} A | ...), merged into a tag it already has."""
    indent = line[:len(line) - len(line.lstrip())]
    body = line.strip()[3:].strip()
    alts = []
    for alt in body.split('|'):
        alt = alt.strip()
        m = re.match(r'^\{([^}]*)\}\s*(.*)$', alt)
        have, words = (m.group(1).split(), m.group(2)) if m else ([], alt)
        keys = {w.split('=', 1)[0] for w in have if '=' in w}
        add = [f'{k}={v}' for k, v in kv if k not in keys]
        alts.append('{' + ' '.join(have + add) + '} ' + words)
    return indent + 'say ' + ' | '.join(alts)


def outside_reads() -> dict:
    """Keys read as remembered facts outside the .talk files: the dialogue trees and the game's own code."""
    out = collections.defaultdict(set)
    pattern = re.compile(r'(?:pair|self|other)\.mem\.([a-z0-9_]+)')
    for f in list((TALK / 'dialogue').glob('*.tree')) + list((ROOT / 'Core').glob('*.cpp')) + list((ROOT / 'Core').glob('*.h')):
        for k in pattern.findall(f.read_text(errors='replace')):
            out[k].add(f.name)
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--write', action='store_true', help='convert the files in place')
    ap.add_argument('--check', action='store_true', help='exit 1 if a short marker that should be a tag is left')
    ap.add_argument('--root', type=Path, default=TALK)
    a = ap.parse_args(argv)
    lib = Library(a.root)
    result = lib.convert()
    s = result['stats']
    if a.check:
        if s['markers']:
            print(f'talk_conv: {s["markers"]} short markers ({s["keys"]} keys) are still pair memory: run tools/talk_conv.py --write')
            for k in sorted(result['convert']):
                print('  ', k)
            return 1
        print(f'talk_conv: no short marker left to convert ({len(result["kept"])} short keys kept as memory, each for a reason)')
        return 0
    print(f'{s["markers"]} short markers in {s["rules"]} rules ({s["keys"]} keys) become tags on {s["say lines tagged"]} say lines; '
          f'{s["reads"]} reads become conv.*, and {s["comments rewritten"]} mentions in comments.')
    print(f'{len(result["kept"])} short keys stay in the talk memory:')
    for k, why in sorted(result['kept'].items()):
        print(f'  {k}: {why}')
    for p in result['problems']:
        print('problem:', p)
    if a.write:
        lib.write()
        print('written')
    return 1 if result['problems'] else 0


if __name__ == '__main__':
    sys.exit(main())
