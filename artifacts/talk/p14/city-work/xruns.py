#!/usr/bin/env python3
"""5-word runs (and whole habits) that the given mantles share with ANY other mantle, across dialects.
G2 checks runs within a dialect only; this is the editor's cross-dialect check.
Usage: xruns.py [--n 5] [--habits-only] FILE...   (compares against every Data/Talk/mantles-export/*/*.mantle)"""
import glob, re, sys
from collections import defaultdict

args = sys.argv[1:]
N = 5
habits_only = False
if '--n' in args:
    i = args.index('--n'); N = int(args[i + 1]); del args[i:i + 2]
if '--habits-only' in args:
    args.remove('--habits-only'); habits_only = True
mine = set(args)
allf = sorted(glob.glob('Data/Talk/mantles-export/*/*.mantle'))


def said(path):
    out = []  # (lineno, kind, text)
    sect = None
    for no, ln in enumerate(open(path), 1):
        ln = ln.rstrip('\n')
        s = ln.strip()
        if s in ('words', 'any', 'grown', 'young', 'adolescent', 'old'):
            sect = s; continue
        if sect is None:
            continue
        if sect == 'words':
            m = re.match(r'(\w+)(!*)\s*=\s*(.*)', s)
            if m:
                kind = 'habit' if m.group(2) else 'word'
                for a in m.group(3).split('|'):
                    out.append((no, kind + ':' + m.group(1), a.strip()))
            continue
        m = re.match(r'(?:line|opinion|dlg)\b[^:]*:\s*(.*)', s) or re.match(r'(?:a|b)(?:\[[^\]]*\])?(?:\s+#\w+)?\??\s*:\s*(.*)', s) \
            or re.match(r'a@\w+\s*:\s*(.*)', s) or re.match(r'(?:say|ask)\s*:?\s*(.*)', s) or re.match(r':\s*(.*)', s) \
            or re.match(r'b(?:\[[^\]]*\])?\s*:\s*(.*)', s)
        if m:
            for a in m.group(1).split(' | '):
                out.append((no, 'line', a.strip()))
    return out


def norm(t):
    t = re.sub(r'\{[^}]*\}', ' ', t)
    t = re.sub(r'#[^#\s]*#', ' ', t)
    return re.findall(r"[a-z']+", t.lower())


index = defaultdict(set)
habit_index = defaultdict(set)
for f in allf:
    mid = f.rsplit('/', 1)[1][:-7]
    for no, kind, text in said(f):
        if kind.startswith('habit'):
            habit_index[' '.join(norm(text))].add(mid)
        w = norm(text)
        for i in range(len(w) - N + 1):
            index[' '.join(w[i:i + N])].add(mid)

total = 0
for f in args:
    mid = f.rsplit('/', 1)[1][:-7]
    seen = set()
    for no, kind, text in said(f):
        if kind.startswith('habit'):
            others = habit_index[' '.join(norm(text))] - {mid}
            if others:
                print(f'{mid} L{no} HABIT "{text}" also in {sorted(others)}'); total += 1
        if habits_only:
            continue
        w = norm(text)
        for i in range(len(w) - N + 1):
            run = ' '.join(w[i:i + N])
            others = index[run] - {mid}
            if others and (no, run) not in seen:
                seen.add((no, run))
                print(f'{mid} L{no} "{run}" also in {sorted(others)[:4]}{"..." if len(others) > 4 else ""}'); total += 1
print(f'{total} shared', file=sys.stderr)
