#!/usr/bin/env python3
"""#self.trade_words# is a gerund ("helping at X"), a third-person verb ("works at X", "runs X") or a bare noun
("butcher at X") by wearer, so only a fragment of its own reads right for every later wearer:
"I'm #self.trade_words#" -> "#self.trade_words.capitalize#". Usage: tradefix.py [--fix] FILE..."""
import re, sys
fix = '--fix' in sys.argv
files = [a for a in sys.argv[1:] if a != '--fix']
PRE = r"(^|(?<=: )|(?<=\| )|(?<=[.!?] ))"
pats = [
    (re.compile(PRE + r"I'm still learning #self\.trade_words#"), "Still learning. #self.trade_words.capitalize#"),
    (re.compile(PRE + r"I'm (?:the one who |the |just |only )?#self\.trade_words#"), "#self.trade_words.capitalize#"),
    (re.compile(PRE + r"I (?:am )?#self\.trade_words#"), "#self.trade_words.capitalize#"),
]
left = 0
for f in files:
    t = open(f).read(); out = []
    for no, ln in enumerate(t.split('\n'), 1):
        new = ln
        for p, r in pats:
            new = p.sub(r, new)
        out.append(new)
        for m in re.finditer(r"[^|:.!?]{0,30}#self\.trade_words(?:\.[a-z]+)?#", new):
            pre = m.group(0)[:-len('#self.trade_words#')].strip()
            pre = re.sub(r'#self\.trade_words.*', '', m.group(0)).strip()
            if pre and not pre.endswith((',', '—')):
                left += 1; print(f'{f.rsplit("/",1)[1][:-7]} L{no}: ...{m.group(0)[-70:]}')
    if fix: open(f, 'w').write('\n'.join(out))
print(f'{left} mid-sentence uses left', file=sys.stderr)
