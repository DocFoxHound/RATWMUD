#!/usr/bin/env python3
"""Editor's digest of mantle drafts: the card, the habits, sample threads, and heuristic flags for what no gate sees.
Usage: review.py [--threads N] [--flags-only] FILE..."""
import json, random, re, sys

args = sys.argv[1:]
nthreads = 2
flags_only = False
if '--threads' in args:
    i = args.index('--threads'); nthreads = int(args[i + 1]); del args[i:i + 2]
if '--flags-only' in args:
    args.remove('--flags-only'); flags_only = True

facts = {}
try:
    for ln in open('Saved/Mantles/residents.jsonl'):
        d = json.loads(ln); facts[d.get('id') or d.get('npc')] = d
except Exception:
    pass

SEXED = re.compile(r"\b(crone|hag|wench|lad|lass|laddie|lassie|my boy|my girl|old man|old woman|spinster|bachelor|"
                   r"widower|widow|mother of|father of|grandmother|grandfather|matron|maiden|sir|madam|ma'am|"
                   r"husband|wife|missus|mister)\b", re.I)
DEED = re.compile(r"#self\.(friend|neighbour|boss|spouse|child|mother|father|hand|priest|shopkeeper|leader)#[^.!?]*\b("
                  r"came|brought|said|told|gave|went|stayed|took|left|bought|sold|fixed|made|found|lost|caught|"
                  r"fell|broke|sang|ran|walked|swore|laughed|cried|won|danced|showed|asked|lent|borrowed|stole)\b", re.I)
PRICE = re.compile(r"\b\d+\s*(p|pence|pennies|copper|coppers|silver|crowns?|shillings?)\b", re.I)
NETS = re.compile(r"\b(my nets|my boat|the catch|my catch)\b", re.I)


def items(path):
    lines = open(path).read().split('\n')
    head, words, blocks = {}, {}, []
    sect = None
    cur = None
    for ln in lines:
        if not ln.strip():
            if cur: blocks.append(cur); cur = None
            continue
        if ln.startswith('#') and sect is None:
            continue
        w = ln.split(' ', 1)
        if sect is None and w[0] not in ('words', 'any', 'grown', 'young', 'adolescent', 'old'):
            head[w[0]] = w[1] if len(w) > 1 else ''
            continue
        if ln.strip() in ('words', 'any', 'grown', 'young', 'adolescent', 'old'):
            if cur: blocks.append(cur); cur = None
            sect = ln.strip(); continue
        if sect == 'words':
            m = re.match(r'(\w+)(!*)\s*=\s*(.*)', ln)
            if m: words[m.group(1)] = m.group(3)
            continue
        kw = w[0]
        if kw in ('scene', 'story', 'rule', 'graft'):
            if cur: blocks.append(cur)
            cur = {'kind': kw, 'name': (w[1] if len(w) > 1 else ''), 'sect': sect, 'body': []}
            continue
        if kw in ('line', 'opinion', 'dlg'):
            if cur: blocks.append(cur); cur = None
            blocks.append({'kind': kw, 'name': ln, 'sect': sect, 'body': []})
            continue
        if cur is not None:
            cur['body'].append(ln)
    if cur: blocks.append(cur)
    return head, words, blocks


def split_alts(text):
    return [a.strip() for a in text.split(' | ')]


for path in args:
    head, words, blocks = items(path)
    mid = head.get('mantle', path)
    who = head.get('written_for', '')
    f = facts.get(who, {})
    kinds = {}
    for b in blocks:
        kinds[b['kind']] = kinds.get(b['kind'], 0) + 1
    flags = []
    alltext = open(path).read()
    for i, ln in enumerate(alltext.split('\n'), 1):
        if ln.startswith(('persona', 'title', 'quirk', 'likes', 'dislikes', 'fears', 'hopes', 'mantle', 'written_for')):
            continue
        s = re.sub(r'#[^#\s]+#', '', ln)
        m = SEXED.search(s)
        if m and 'spouse_word' not in ln:
            flags.append(f'L{i} sexed? "{m.group(0)}": {ln.strip()[:110]}')
        m = DEED.search(ln)
        if m:
            flags.append(f'L{i} slot deed? {ln.strip()[:120]}')
        m = PRICE.search(ln)
        if m and not ln.startswith('dlg'):
            flags.append(f'L{i} price? "{m.group(0)}": {ln.strip()[:100]}')
        m = NETS.search(ln)
        if m:
            flags.append(f'L{i} nets? {ln.strip()[:100]}')
        if re.match(r'line(\s+@\w+)?\s+(greet|again\.open|hail|kin\.open)\b', ln):
            body = ln.split(' : ', 1)[-1]
            for a in split_alts(body):
                if a.rstrip().endswith('?'):
                    flags.append(f'L{i} opener asks: {a[:90]}')
    wp = (f.get('slots') or {}).get('self.workplace') or f.get('workplace') or ''
    if isinstance(wp, str) and wp.startswith(('Ridgemere,', 'The Rainwood,')) and '#self.workplace#' in alltext:
        flags.append(f'workplace is a district ({wp}) and #self.workplace# is used {alltext.count("#self.workplace#")}x')
    print(f'=== {mid} {who} [{head.get("tier")} {head.get("dialect")} {head.get("accent")}] "{head.get("title")}"  quirk {head.get("quirk")}')
    if not flags_only:
        print(f'  persona: {head.get("persona")}')
        print(f'  traits: {head.get("traits")}  topics: {head.get("topics")}')
        print(f'  oath: {words.get("oath")}   saying: {words.get("saying")}')
        print('  counts: ' + ', '.join(f'{k} {v}' for k, v in sorted(kinds.items())) + f', words {len(words)}')
        scenes = [b for b in blocks if b['kind'] == 'scene']
        rnd = random.Random(mid)
        for b in rnd.sample(scenes, min(nthreads, len(scenes))):
            turns = [l for l in b['body'] if re.match(r'(a|b)(\[[^\]]*\])?(\s+#\w+)?\??\s*:|a@\w+\s*:', l)]
            when = [l for l in b['body'] if l.startswith('when')]
            n = max(len(split_alts(t.split(':', 1)[1])) for t in turns) if turns else 0
            t = rnd.randrange(n) if n else 0
            print(f'  -- scene {b["name"]} ({when[0] if when else ""}) thread {t + 1}/{n}')
            for tl in turns:
                spk, body = tl.split(':', 1)
                alts = split_alts(body)
                print(f'     {spk.strip()}: {alts[t] if t < len(alts) else alts[0]}')
        stories = [b for b in blocks if b['kind'] == 'story']
        if stories:
            s = rnd.choice(stories)
            print(f'  -- story {s["name"]}')
            for l in s['body']:
                print(f'     {l.strip()}')
    for fl in flags:
        print('  ! ' + fl)
