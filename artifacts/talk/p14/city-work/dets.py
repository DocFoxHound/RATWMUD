#!/usr/bin/env python3
"""Doubled determiners (a possessive value after the/a/that, "the storm's rumble", is fine): "my #pet#" where pet's values start with "my" ("my my pearl"), "the #x#" where x starts
"the"/"a"..., "a #x#" where x starts with a determiner. Reads the mantle's own words, its dialect's grammar and plain's.
Usage: dets.py FILE..."""
import re, sys

DET = r"(my|the|a|an|your|our|his|her|their|this|that|some)"


def grammar(path):
    out = {}
    try:
        for ln in open(path):
            m = re.match(r'\s*([a-z0-9_]+)(!*)\s*=\s*(.*)', ln)
            if m:
                out[m.group(1)] = [a.strip() for a in m.group(3).split('|')]
    except FileNotFoundError:
        pass
    return out


plain = grammar('Data/Talk/grammar/plain.grammar')
FIX = '--fix' in sys.argv
if FIX: sys.argv.remove('--fix')
AUTO = {'my', 'the', 'a', 'an', 'some', 'that', 'this'}
cache = {}
n = 0
for f in sys.argv[1:]:
    text = open(f).read()
    dia = re.search(r'^dialect (\w+)', text, re.M).group(1)
    if dia not in cache:
        g = dict(plain); g.update(grammar(f'Data/Talk/grammar/{dia}.grammar')); cache[dia] = g
    syms = dict(cache[dia])
    in_words = False
    for ln in text.split('\n'):
        s = ln.strip()
        if s == 'words': in_words = True; continue
        if s in ('any', 'grown', 'young', 'adolescent', 'old'): in_words = False
        if in_words:
            m = re.match(r'([a-z0-9_]+)(!*)\s*=\s*(.*)', s)
            if m: syms[m.group(1)] = [a.strip() for a in m.group(3).split('|')]
    lines = text.split('\n')
    for no, ln in enumerate(lines, 1):
        def sub(m):
            vals = syms.get(m.group(2), [])
            bad = [v for v in vals if re.match(DET + r"\b", v, re.I) or re.match(r"[a-z]+'s\b", v, re.I)]
            if not bad or m.group(1).lower() not in AUTO or '.' in m.group(0).split('#')[1]:
                return m.group(0)
            return '#' + m.group(2) + ('.capitalize#' if m.group(1)[0].isupper() else '#')
        if FIX and not re.match(r'\s*[a-z0-9_]+!*\s*=', ln):
            lines[no - 1] = re.sub(r'\b' + DET + r"\s+#([a-z0-9_]+)(?:\.[a-z]+)?#", sub, ln, flags=re.I)
            ln = lines[no - 1]
        for m in re.finditer(r'\b' + DET + r"\s+#([a-z0-9_]+)(?:\.[a-z]+)?#", ln, re.I):
            vals = syms.get(m.group(2), [])
            pos_ok = m.group(1).lower() in ('the', 'a', 'an', 'that', 'this')   # ("the storm's rumble" reads right)
            bad = [v for v in vals if re.match(DET + r"\b", v, re.I) or (not pos_ok and re.match(r"[a-z]+'s\b", v, re.I))]
            if bad:
                n += 1
                print(f'{f.rsplit("/",1)[1][:-7]} L{no}: "{m.group(0)}" but #{m.group(2)}# = {bad[:2]}')
    if FIX: open(f, 'w').write('\n'.join(lines))
print(f'{n} doubled determiners' + (' left (fix the rest by hand)' if FIX else ''), file=sys.stderr)
