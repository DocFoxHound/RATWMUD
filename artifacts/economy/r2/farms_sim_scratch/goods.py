import json, collections, sys
ev = json.load(open(sys.argv[1]))['events']
lo = float(sys.argv[2]) if len(sys.argv) > 2 else 6
days = set()
g = collections.defaultdict(collections.Counter)
stock = collections.Counter()
for e in ev:
    a, t, it, q, det = e['actor'], e['target'], e['item'], e['quantity'], e['detail']
    if not it or q == 0:
        continue
    farmA, farmT = a.startswith('farm:'), t.startswith('farm:')
    # stock tracking across the whole run
    if farmT and det.startswith('grown'):
        stock[(t, it)] += q
    if farmA:
        stock[(a, it)] -= q
    if e['day'] < lo:
        continue
    days.add(int(e['day']))
    if farmT and det.startswith('grown'):
        g[it]['grown'] += q
    elif farmA and t.startswith('till:'):
        g[it]['drawn'] += q
    elif farmA and det == 'spoiled':
        g[it]['spoiled'] += q
    elif farmA and det.startswith('eaten in the Country'):
        g[it]['table'] += q
    elif farmA and det.startswith('eaten by'):
        g[it]['crew'] += q
    elif farmA:
        g[it]['other:' + det[:20]] += q
n = len(days)
held = collections.Counter()
for (f, it), q in stock.items():
    held[it] += q
print(f'{n} days')
for it, c in sorted(g.items(), key=lambda kv: -kv[1]['drawn']):
    print(f'{it:16} grown {c["grown"]/n:7.1f} drawn {c["drawn"]/n:7.1f} spoiled {c["spoiled"]/n:6.1f} table {c["table"]/n:5.1f} held_end {held[it]:6}  ' +
          ' '.join(f'{k}={v/n:.1f}' for k, v in c.items() if k.startswith('other')))
