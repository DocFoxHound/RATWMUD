#!/usr/bin/env python3
"""A listener's (or the wearer's next) line that says they/them/their right after a line naming one wolf by a slot:
"#self.child# makes me proud." / "You should tell them." (R8: one known wolf is never "they"). By thread.
Usage: slotthey.py FILE..."""
import re, sys
SLOT = re.compile(r"#(self\.(spouse|child|mother|father|neighbour|shopkeeper|priest|leader|friend|rival|boss|hand|late)|topic\.subject|topic\.victim)[.#]")
THEY = re.compile(r"\b(they|them|their|theirs|themselves|they're|they've|they'll|they'd)\b", re.I)
n = 0
for f in [a for a in sys.argv[1:] if not a.startswith("--")]:
    lines = open(f).read().split('\n')
    turns = []
    for no, ln in enumerate(lines, 1):
        s = ln.strip()
        if s.startswith(('scene ', 'story ')) or not s:
            turns = []
            continue
        m = re.match(r'(a|b)(\[[^\]]*\])?(\s+#\w+)?\??\s*:\s*(.*)|a@\w+\s*:\s*(.*)|:\s*(.*)', s)
        if not m:
            continue
        body = m.group(4) or m.group(5) or m.group(6) or ''
        alts = body.split(' | ')
        if turns:
            prev = [max((t[k] if k < len(t) else t[0]) for t in turns if True) for k in range(len(alts))] if False else turns[-1]
            if "--deep" in sys.argv:
                prev = [" ".join((t[k] if k < len(t) else t[0]) for t in turns) for k in range(len(alts))]
            for k, alt in enumerate(alts):
                p = prev[k] if k < len(prev) else prev[0]
                if SLOT.search(p) and not SLOT.search(alt) and THEY.search(re.sub(r'#[^#]+#', '', alt)):
                    n += 1
                    print(f'{f.rsplit("/",1)[1][:-7]} L{no} t{k+1}: "{p[:70]}" / "{alt[:80]}"')
        turns.append(alts)
print(f'{n} found', file=sys.stderr)
