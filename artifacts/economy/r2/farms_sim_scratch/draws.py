import json, collections, sys
lo, hi = 30, 60
def base_draws(path):
    ev = json.load(open(path))['events']
    c = collections.Counter(); made = collections.Counter(); days = set()
    for e in ev:
        if not (lo <= e['day'] < hi) or not e['item'] or e['quantity'] <= 0:
            continue
        days.add(int(e['day']))
        d = e['detail']
        if e['actor'] == 'made' and (d == 'restocked from the store room' or d.startswith('drawn for ')):
            c[e['item']] += e['quantity']
        elif e['actor'] == 'made' and d == 'made from the store room':
            made[e['item']] += e['quantity']
    return c, made, len(days)
def farm_draws(path):
    ev = json.load(open(path))['events']
    c = collections.Counter(); c2 = collections.Counter(); made = collections.Counter(); days = set()
    for e in ev:
        if not (lo <= e['day'] < hi) or not e['item'] or e['quantity'] <= 0:
            continue
        days.add(int(e['day']))
        d = e['detail']
        if e['actor'].startswith('farm:') and e['target'].startswith('till:') and (d == 'restocked from the store room' or d.startswith('drawn for ')):
            c[e['item']] += e['quantity']
        elif e['actor'] == 'made' and (d == 'restocked from the store room' or d.startswith('drawn for ')):
            c2[e['item']] += e['quantity']
        elif e['actor'] == 'made' and d == 'made from the store room':
            made[e['item']] += e['quantity']
    return c, c2, made, len(days)
b, bm, nb = base_draws(sys.argv[1])
f, fc, fm, nf = farm_draws(sys.argv[2])
goods = sys.argv[3].split(',')
for g in goods:
    print(f'{g:16} base drawn {b[g]/nb:7.1f} made {bm[g]/nb:7.1f} | farms: from farms {f[g]/nf:7.1f} from Country {fc[g]/nf:6.1f} made {fm[g]/nf:7.1f}')
