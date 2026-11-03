import json, collections, sys
SEASONS=['spring','summer','autumn','winter']
rows=[json.loads(l) for l in open(sys.argv[1])]
skip=int(sys.argv[2]) if len(sys.argv)>2 else 14
sold=collections.defaultdict(lambda: collections.defaultdict(list)); spo=collections.defaultdict(lambda: collections.defaultdict(list)); out=collections.defaultdict(lambda: collections.defaultdict(int))
n=collections.Counter()
for r in rows[skip:]:
    s=r['season']; n[s]+=1
for r in rows[skip:]:
    s=r['season']
    for g,v in r.get('farms_sold_goods',{}).items(): sold[g][s].append(v)
    for g,v in r.get('farms_spoiled_goods',{}).items(): spo[g][s].append(v)
    for g,v in r.get('farms_out_goods',{}).items():
        if v>0: out[g][s]+=1
res={}
print(f'{"good":16}' + ''.join(f'{s:>22}' for s in SEASONS))
for g in sorted(sold, key=lambda g: -sum(sum(v) for v in sold[g].values())):
    res[g]=[sum(sold[g][s])/n[s] if n[s] else 0 for s in SEASONS]
    print(f'{g:16}' + ''.join(f'{sum(sold[g][s])/n[s] if n[s] else 0:9.1f} sp{sum(spo[g][s])/n[s] if n[s] else 0:6.1f} o{out[g][s]:3}' for s in SEASONS))
json.dump(res, open(sys.argv[1].replace('.jsonl','_demand.json'),'w'))
