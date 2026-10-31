# The stability scorecard of an economy run (round 12 on, the user's goal 2026-10-29: "stable and resilient"): a row a
# quarter (91 days) and a verdict on each criterion. Usage: stability.py RUN_DIR [--json]
# Inequality is over the earning households (the user, 2026-10-30: "Don't count household dependents in the gini.
# Babies, children, or even retired folks. Their hunger matters, their income does not, and their families should be
# working enough to feed them."): econ_watch's households (a home's shared purse; in a barracks, a bunkhouse, quarters
# or lodgings, or a home of over eight grown, each its own), counting only those with a member 16 or over and under its
# own retiring age (Society::retireAge, by retire_age beside this script); a family's purse is still all of its members'.
# Rebuilt a day from residents.csv (it reproduces econ_watch's household_gini, poorer_half, households and elders exactly)
# and kept in the run's earning.json; the old measure, every household, stays in the table as "all".
import json,sys,csv,collections,os,subprocess
D=sys.argv[1]; Q=91
def communal(h): return any(k in h for k in ('barracks','bunkhouse','quarters','watch_house','lodging'))
def giniOf(money):
    money=sorted(max(0,m) for m in money); h=len(money); t=sum(money)
    if not h or t<=0: return 0.,0.
    return 2*sum((i+1)*m for i,m in enumerate(money))/(h*t)-(h+1)/h, sum(money[:h//2])/t
def earning(D):
    src=f'{D}/residents.csv'
    if not os.path.exists(src): return {}
    key=os.path.getsize(src); cache=f'{D}/earning.json'
    try:
        c=json.load(open(cache))
        if c['size']==key: return {int(k):v for k,v in c['days'].items()}
    except Exception: pass
    ids=set()
    with open(src) as f:
        r=csv.reader(f); next(r)
        for row in r: ids.add(row[1])
    helper=os.path.join(os.path.dirname(os.path.abspath(__file__)),'retire_age')
    out=subprocess.run([helper],input='\n'.join(sorted(ids))+'\n',capture_output=True,text=True,check=True).stdout
    retire={a:int(b) for a,b in (l.split(',') for l in out.split('\n') if l)}
    res={}
    def close(day,rows):
        grown=collections.Counter(h for _,h,a,_c in rows if a>=16 and h)
        homes=collections.defaultdict(int); earners=collections.defaultdict(int)
        for i,h,a,cash in rows:
            k=i if (not h or grown[h]>8 or communal(h)) else h
            homes[k]+=cash; earners[k]+=16<=a<retire[i]
        g,p=giniOf([m for k,m in homes.items() if earners[k]])
        res[day]=[g,p,sum(1 for k in homes if earners[k])]
    with open(src) as f:
        r=csv.reader(f); next(r); day=None; rows=[]
        for row in r:
            d=int(row[0])
            if d!=day:
                if rows: close(day,rows)
                day=d; rows=[]
            rows.append((row[1],row[2],int(row[5]),int(row[6])))
        if rows: close(day,rows)
    json.dump(dict(size=key,days=res),open(cache,'w'))
    return res
earn=earning(D)
days=[]
for l in open(f'{D}/days.jsonl'):
    try:
        o=json.loads(l)
    except Exception: continue
    if o.get('when')=='start' and o['day']>0: continue
    m=o.get('model') or {}; h=o.get('holders') or {}; t=o.get('towns') or {}
    ga=m.get('household_gini',o.get('gini')); pa=m.get('poorer_half',o.get('bottom_half_share')); e=earn.get(o['day'])
    days.append(dict(day=o['day'],res=o['residents'],gini=e[0] if e else ga,poor=e[1] if e else pa,gini_all=ga,poor_all=pa,
        hungry=o.get('hungry',0),starving=o.get('starving',0),broke=o.get('broke',0),going=(o.get('going') or {}).get('',0),
        capital=(h.get('capital treasury') or [0])[0],church=(h.get('town church') or [0])[0],towns=(h.get('town treasury') or [0])[0],
        houses=(h.get('great house') or [0])[0],producers=(h.get('producer') or [0])[0],paid=(h.get('paid civilian') or [0])[0],
        fh=(t.get('frosthollow') or {}).get('hungry',0),townheld={k:v.get('town_held',0) for k,v in t.items()}))
price=collections.defaultdict(lambda:[0,0])
try:
    for r in csv.DictReader(open(f'{D}/prices.csv')):
        if r['kind']=='resident food purchase': a=price[int(r['day'])//Q]; a[0]+=int(r['units']); a[1]+=int(r['coins'])
except FileNotFoundError: pass
stopped={}
try:
    for r in csv.DictReader(open(f'{D}/tills.csv')):
        d=int(r['day'])
        if d%Q==0 or d==days[-1]['day']: stopped.setdefault(d,[0,0]); stopped[d][0]+=1; stopped[d][1]+=r['stopped']!='-1'
except FileNotFoundError: pass
last=days[-1]['day']; rows=[]
for q in range(0,last//Q+1):
    ds=[d for d in days if q*Q<=d['day']<(q+1)*Q and d['day']>0]
    if not ds: continue
    e=ds[-1]; u,c=price[q]
    st=stopped.get(e['day']) or stopped.get((q+1)*Q) or stopped.get(q*Q) or [0,0]
    rows.append(dict(q=q,end=e['day'],res=e['res'],gini=e['gini'],poor=e['poor'],gini_all=e['gini_all'],poor_all=e['poor_all'],hungry_mean=sum(d['hungry'] for d in ds)/len(ds),hungry_max=max(d['hungry'] for d in ds),
        starving_max=max(d['starving'] for d in ds),broke=e['broke'],going=e['going'],food=c/u if u else 0,capital=e['capital'],church=e['church'],towns=e['towns'],
        town_min=min(e['townheld'].values()) if e['townheld'] else 0,houses=e['houses'],producers=e['producers'],paid=e['paid'],fh_max=max(d['fh'] for d in ds),
        stopped=st[1],tills=st[0]))
print(f"{D}  (to day {last}; COLLAPSED: {open(D+'/COLLAPSED').read().strip()})" if __import__('os').path.exists(D+'/COLLAPSED') else f"{D}  (to day {last})")
print(f"(inequality: {'earning households, dependents left out' if earn else 'every household (no residents.csv)'}; 'all': every household)")
print(" q  end  res  gini  poor½ hung~ hung^ starv^ broke going  food  capital  church   towns townmin  houses producers  paid  FH^ stopped  all: gini  poor½")
for r in rows:
    print(f"{r['q']:2} {r['end']:4} {r['res']:4} {r['gini']:.3f} {r['poor']:.3f} {r['hungry_mean']:5.1f} {r['hungry_max']:5} {r['starving_max']:6} {r['broke']:5} {r['going']:5.1f} {r['food']:5.1f} {r['capital']/1e3:7.0f}k {r['church']/1e3:6.0f}k {r['towns']/1e3:6.0f}k {r['town_min']/1e3:6.1f}k {r['houses']/1e3:6.0f}k {r['producers']/1e3:8.0f}k {r['paid']/1e3:5.0f}k {r['fh_max']:4} {r['stopped']:3}/{r['tills']}  {r['gini_all']:.3f} {r['poor_all']:.3f}")
# Criteria (after the first quarter, the founding's settling).
after=[d for d in days if d['day']>Q]; R=[r for r in rows if r['q']>=1]
res0=days[1]['res'] if len(days)>1 else days[0]['res']
v={}
if after:
    n=len(after)
    v['hunger']=(sum(d['hungry']<=0.01*d['res'] for d in after)/n>=.95 and sum(d['starving']<=0.005*d['res'] for d in after)/n>=.95,
                 f"days hungry<=1%: {sum(d['hungry']<=0.01*d['res'] for d in after)/n:.0%}, starving<=0.5%: {sum(d['starving']<=0.005*d['res'] for d in after)/n:.0%}")
    gmax=max(d['gini'] for d in after); pmin=min(d['poor'] for d in after)
    # The trend: the last four weeks' mean against the same four weeks a year before (the same season, so the harvest's
    # step cancels; and four weeks, so no one day's jitter decides it; the user left it to judgement, 2026-10-30: not a
    # line fitted over the year, which a season's swing tilts). A run under a year and four weeks: its first four weeks.
    endW=[d['gini'] for d in after if d['day']>last-28]
    startW=[d['gini'] for d in after if last-364-28<d['day']<=last-364] or [d['gini'] for d in after[:28]]
    span=364 if any(d['day']<=last-364 for d in after) else max(1,last-after[min(27,len(after)-1)]['day'])
    slope=(sum(endW)/len(endW)-sum(startW)/len(startW))*364/span if endW and startW else 0
    v['inequality']=(gmax<=0.60 and pmin>=0.10 and abs(slope)<=0.03, f"Gini max {gmax:.3f}, poorer half min {pmin:.3f}, last year's Gini trend {slope:+.3f}/yr (4-week means a year apart)")
    f0=rows[0]['food'] or 1; fs=[r['food'] for r in R if r['food']]
    rising=any(all(fs[i+k+1]>fs[i+k] for k in range(4)) for i in range(len(fs)-4)) if len(fs)>=5 else False
    v['prices']=(bool(fs) and min(fs)>=.67*f0 and max(fs)<=1.5*f0 and not rising, f"food a unit {f0:.1f}p in Q0, {min(fs) if fs else 0:.1f}-{max(fs) if fs else 0:.1f}p after; five quarters rising: {rising}")
    g0=60; gs=[d['going'] for d in after]
    v['wages']=(min(gs)>=.7*g0 and max(gs)<=1.6*g0, f"going rate {min(gs):.0f}-{max(gs):.0f}p (founding 60p)")
    y1=[d for d in after if d['day']<=364] or after
    cap1=sum(d['capital'] for d in y1)/len(y1); ch1=sum(d['church'] for d in y1)/len(y1)
    v['treasuries']=(min(d['capital'] for d in after)>=.2*cap1 and min(d['church'] for d in after)>=.2*ch1, f"capital min {min(d['capital'] for d in after)/1e3:.0f}k vs year-1 mean {cap1/1e3:.0f}k; church min {min(d['church'] for d in after)/1e3:.0f}k vs {ch1/1e3:.0f}k")
    sts=[r['stopped']/r['tills'] for r in R if r['tills']]
    # (Idle is allowed, the user's 2026-10-29: a business with no work goes idle and its owner comes back with demand. So
    # not a level but a bound and a trend: at most half stopped, and the last year's share no more than 10 points up.)
    yr=[r['stopped']/r['tills'] for r in R if r['tills'] and r['end']>last-364]
    drift=(yr[-1]-yr[0]) if len(yr)>1 else 0
    v['businesses']=(bool(sts) and max(sts)<=.50 and drift<=.10, f"tills stopped {min(sts) if sts else 0:.0%}-{max(sts) if sts else 0:.0%}, last year's change {drift:+.0%}")
    rs=[d['res'] for d in after]
    v['population']=(min(rs)>=.9*res0 and max(rs)<=1.1*res0, f"residents {min(rs)}-{max(rs)} (start {res0})")
ok=all(x[0] for x in v.values()) and not __import__('os').path.exists(D+'/COLLAPSED')
for k,(p,why) in v.items(): print(f"  {'PASS' if p else 'FAIL'} {k:11} {why}")
print(f"  VERDICT: {'STABLE' if ok else 'NOT STABLE'}")
if '--json' in sys.argv: print(json.dumps(dict(dir=D,last=last,ok=ok,criteria={k:dict(passed=p,detail=w) for k,(p,w) in v.items()},rows=rows)))
