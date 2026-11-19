#!/usr/bin/env python3
"""The city editor's flags for plain mantles, beside review.py's: openers that ask, quarter or House claims with no
gate behind them, invented deeds of slot people, numbers that look like prices or dates in news, the brief's
backstory clichés, and the quirk/oath/saying/title against every other Ridgemere mantle.
Usage: cityflags.py FILE..."""
import glob, re, sys
sys.path.insert(0, 'tools')
import mantle_text as T  # noqa: E402

PLACE = r"(Northgate|the gate|the Sump|the Commons|the Rise|the Heights|the Wharf|the headland|the Works|the Office|the Estate|this quarter|our quarter|our street|our stair|our court|the tenement|this tenement)"
QUARTER = re.compile(r"\b(born|bore me|grew up|all my life|my whole life|never lived|I live|we live|I've lived|lived here|home is)\b[^.!?]*\b" + PLACE[1:-1].join("()") + r"|\b" + PLACE + r"\b[^.!?]*\b(bore me|born|all my life|my whole life|home)\b|\b([Dd]own here|[Uu]p here)\b")
GATES = re.compile(r"self\.(quarter|house|class|wealth|job|role|working|workplace|street|trade)|here\.(region|place)")
DEED = re.compile(r"#self\.(friend|neighbour|boss|spouse|child|mother|father|hand|priest|shopkeeper|leader|rival)#[^.!?|]*\b("
                  r"came|brought|said|told|gave|went|stayed|took|left|bought|sold|fixed|made|found|lost|caught|fell|broke|"
                  r"sang|ran|walked|swore|laughed|cried|won|danced|showed|asked|lent|borrowed|stole|dropped|burnt|burned|"
                  r"spilt|spilled|forgot|knitted|baked|cooked|carried)\b", re.I)
CLICHE = re.compile(r"charcoal camp|Chancellor|Lord Grayrock|a berth on|born in the Sump|\\bweep", re.I)
OPENERS = ('greet', 'again.open', 'hail', 'kin.open')

others = {}
for f in glob.glob('Data/Talk/mantles-export/*/*.mantle'):
    try:
        m = T.read(open(f).read(), f)
    except Exception:
        continue
    words = {i.key[6:]: i.body.split('=', 1)[1].strip() for i in m.items if i.kind == 'symbol'}
    others[m.id] = (m, words)

for path in sys.argv[1:]:
    m = T.read(open(path).read(), path)
    flags = []
    words = others.get(m.id, (m, {}))[1]
    for i in m.items:
        body = i.body
        crit = ' '.join(re.findall(r'\[([^\]]*)\]', body.split('\n')[0])) + ' ' + ' '.join(
            l for l in body.split('\n') if l.startswith('when ')) + ' ' + i.section
        for ln in body.split('\n'):
            s = ln.strip()
            mm = re.match(r'(?:line|dlg|opinion)\b[^:]*:\s*(.*)', s) or re.match(r'a(?:@\w+)?(?:\[[^\]]*\])?\s*:\s*(.*)', s) \
                or re.match(r'say\s+(.*)', s) or re.match(r':\s*(.*)', s)
            if not mm:
                continue
            for alt in mm.group(1).split(' | '):
                if i.kind == 'line' and i.move in OPENERS:
                    if alt.rstrip().endswith('?'):
                        flags.append(f'{i.key}: opener asks: {alt[:90]}')
                q = QUARTER.search(alt)
                if q and not GATES.search(crit) and re.search(r"\b(I|I'm|I've|my|our|we|us)\b", alt):
                    flags.append(f'{i.key}: "{q.group(0)}" claimed with no gate: {alt[:100]}')
                if DEED.search(alt):
                    flags.append(f'{i.key}: slot deed? {alt[:110]}')
                if CLICHE.search(alt):
                    flags.append(f'{i.key}: brief cliche? {alt[:100]}')
    me = {'quirk': m.quirk, 'title': m.title, 'oath': words.get('oath', ''), 'saying': words.get('saying', '')}
    for oid, (o, ow) in others.items():
        if oid == m.id:
            continue
        if o.title == m.title:
            flags.append(f'title "{m.title}" also {oid}')
        for k in ('oath', 'saying'):
            mine = {a.strip().lower() for a in me[k].split('|') if a.strip()}
            theirs = {a.strip().lower() for a in ow.get(k, '').split('|') if a.strip()}
            if mine & theirs:
                flags.append(f'{k} {sorted(mine & theirs)} also {oid}')
    print(f'=== {m.id} {m.written_for} "{m.title}" quirk {m.quirk}: {len(flags)} flags')
    for fl in flags:
        print('  ! ' + fl)
