#!/usr/bin/env python3
"""The batch report's table and totals, counted from the loaded files. Usage: report_table.py BATCH"""
import json, re, sys
sys.path.insert(0, 'tools')
import mantle_text as T  # noqa: E402

batch = sys.argv[1]
rows = [l.rstrip('\n').split('\t') for l in open(f'Saved/Mantles/{batch}/batch.tsv')][1:]
facts = {}
for ln in open('Saved/Mantles/residents.jsonl'):
    d = json.loads(ln); facts[d['id']] = d
out = ['| id | resident | name | dialect | accents | tier | stage | title | items | scenes | stories | opinions | lines | barks | dlg | grafts | symbols | alternatives |',
       '|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|']
tot = dict(items=0, alts=0, words=0, full=0, standard=0, pups=0)
for r in rows:
    mid, npc, name, dialect, tier, accents, stage, path = r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[11]
    text = open(path).read()
    m = T.read(text, path)
    kinds = {}
    for it in m.items:
        k = it.kind if it.kind != 'rule' else ('bark' if 'concept=bark' in it.body else 'rule')
        kinds[k] = kinds.get(k, 0) + 1
    sym = kinds.get('symbol', 0) + kinds.get('word', 0)
    items = sum(v for k, v in kinds.items() if k not in ('symbol', 'word'))
    body = text.split('\nwords\n', 1)[-1]
    alts = 0
    words = 0
    for ln in body.split('\n'):
        s = ln.strip()
        if re.match(r'^(line|opinion|dlg|a|b|a@|say)\b|^(a|b)(\[|\?| #)|^a@|^:|^ask :', s) and ':' in s or s.startswith('say '):
            seg = s.split(':', 1)[1] if ':' in s and not s.startswith('say ') else s[4:]
            alts += len(seg.split(' | '))
            words += len(re.findall(r"[A-Za-z']+", re.sub(r'#[^#\s]+#', 'x', seg)))
    tot['items'] += items; tot['alts'] += alts; tot['words'] += words
    tot[tier] = tot.get(tier, 0) + 1
    if stage in ('young', 'adolescent'): tot['pups'] += 1
    out.append(f'| {mid} | {npc} | {name} | {dialect} | {accents} | {tier} | {stage} | {m.title} | {items} | {kinds.get("scene", 0)} | '
               f'{kinds.get("story", 0)} | {kinds.get("opinion", 0)} | {kinds.get("line", 0)} | {kinds.get("bark", 0)} | '
               f'{kinds.get("dlg", 0)} | {kinds.get("graft", 0)} | {sym} | {alts} |')
print('\n'.join(out))
print(f'\nTotals: {len(rows)} mantles ({tot.get("full", 0)} full, {tot.get("standard", 0)} standard, {tot["pups"]} pups or adolescents); '
      f'{tot["items"]} items, {tot["alts"]} alternatives, about {tot["words"]:,} words.')
