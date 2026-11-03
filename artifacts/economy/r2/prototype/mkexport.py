#!/usr/bin/env python3
"""mkexport.py NAME [LINE ...]: a what-if copy of export50 (cells and seams linked, world.ratw copied) with the given
records added after its "store" line (e.g. "loop needs on country on reserve 200 toll 27 carriage 6 wages 14 8 8 8")."""
import os
import shutil
import sys

SRC = '/home/martinb/Documents/GitHub/MUDProject/artifacts/economy/r2/export50'
DST = '/tmp/claude-1000/-home-martinb-Documents-GitHub-MUDProject/da6a1181-05f7-491a-a334-c3e7343d5540/scratchpad/exports'
name, lines = sys.argv[1], sys.argv[2:]
out = os.path.join(DST, name)
os.makedirs(out, exist_ok=True)
for d in ('cells', 'seams'):
    link = os.path.join(out, d)
    if not os.path.exists(link):
        shutil.copytree(os.path.join(SRC, d), link)
text = open(os.path.join(SRC, 'world.ratw')).read().split('\n')
i = next(k for k, l in enumerate(text) if l.startswith('store '))
text[i + 1:i + 1] = lines
open(os.path.join(out, 'world.ratw'), 'w').write('\n'.join(text))
print(out)
