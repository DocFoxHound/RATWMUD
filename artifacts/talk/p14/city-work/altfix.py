#!/usr/bin/env python3
"""Replace text inside one alternative of one line: altfix.py FILE LINE THREAD OLD NEW (THREAD 1-based, 0 = any)."""
import sys
f, no, k, old, new = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4], sys.argv[5]
lines = open(f).read().split('\n')
ln = lines[no - 1]
head, sep, body = ln.partition(' : ') if ' : ' in ln else ln.partition(': ')
alts = body.split(' | ')
idx = range(len(alts)) if k == 0 else [k - 1 if len(alts) > 1 else 0]
done = False
for i in idx:
    if old in alts[i]:
        alts[i] = alts[i].replace(old, new, 1); done = True; break
if not done:
    sys.exit(f'not found: {f} L{no} t{k}: {old!r} in {body[:120]!r}')
lines[no - 1] = head + sep + ' | '.join(alts)
open(f, 'w').write('\n'.join(lines))
