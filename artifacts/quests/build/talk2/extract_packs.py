# The quest packs' talk lines, read from the design files (quests-small.md, quests-medium.md, quests-large.md) into
# packs_raw.json. build_packs.py then applies the fixes and additions and writes packs.json, which gen.py reads.
import re, sys, json
from pathlib import Path
HERE = Path(__file__).resolve().parent
D = str(HERE.parent.parent / 'design') + '/'
TOWNS={'ridgemere','coldwater','larkfield','southmeadow','fenshore','saltwell','oakhaven','frosthollow'}
DIAL={'plain','coldwater','larkfield','southmeadow','mirebridge','saltwell','oakhaven','frosthollow'}
TOWN_DIAL={'ridgemere':'plain','coldwater':'coldwater','larkfield':'larkfield','southmeadow':'southmeadow','fenshore':'mirebridge','saltwell':'saltwell','oakhaven':'oakhaven','frosthollow':'frosthollow'}
out=[]
def rec(src,quest,node,gates,line,ln):
    out.append(dict(src=src,quest=quest,node=node,gates=gates,line=line.strip(),ln=ln))
# small
quest=None
for i,l in enumerate(open(D+'quests-small.md'),1):
    m=re.match(r'#### (?:S?\d+)\. `([a-z0-9-]+)`',l)
    if m: quest=m.group(1)
    m=re.match(r'\| (news|gossip|hub|greet) · (.*?) \| (.*) \|\s*$',l)
    if m:
        node,g,line=m.groups()
        gates=[]
        for part in re.findall(r'`([^`]*)`',g): gates+=part.split()
        rest=re.sub(r'`[^`]*`','',g)
        for t in re.split(r'[·\s]+',rest):
            if t=='on': gates.append('world.mem.sq_on=yes')
            elif t.startswith('last='): gates.append('world.mem.sq_last='+t[5:])
        rec('small',quest,node,[x.replace('\\|','|') for x in gates],line.replace('\\|','|'),i)
# medium
quest=None; town=None; dial=None
for i,l in enumerate(open(D+'quests-medium.md'),1):
    m=re.match(r'### (?:Q|S)\d+\. `([a-z0-9-]+)`',l)
    if m: quest=m.group(1); town=dial=None
    m=re.match(r'- \*\*Gossip and helpful knowledge\*\*(.*)',l)
    if m:
        h=m.group(1).lower(); town=dial=None
        for t in TOWNS:
            if t in h: town=t
        for d in DIAL:
            if re.search(r'\b'+d+r'\b',h.replace(town or '#','')) or ('dialect='+d) in h: dial=d
        if town and not dial: dial=TOWN_DIAL[town]
    m=re.match(r'\s*- (\(H\) )?`\[(.*?)\]` "(.*)"\s*$',l)
    if m:
        h,b,line=m.groups()
        parts=[p.strip() for p in b.split('·')]
        node=None; gates=[]; t=town; d=dial
        for p in parts:
            pl=p.lower()
            if p in('resident.gossip','resident.news','inn.greet'): node=p
            elif pl in TOWNS and p!=pl: t=pl
            elif pl in DIAL: d=pl
            else: gates+=p.split()
        if node is None: node='resident.gossip'
        gd=[x for x in gates if x.startswith('self.dialect=')]
        if gd: d=gd[0].split('=')[1]; gates=[x for x in gates if not x.startswith('self.dialect=')]
        g=[]
        if t and not any(x.startswith('here.region') for x in gates): g.append('here.region='+t)
        if d and not any(x.startswith('self.id') for x in gates): g.append('self.dialect='+d)
        rec('medium',quest,node,g+gates,line,i)
# large
quest=None; intable=False
for i,l in enumerate(open(D+'quests-large.md'),1):
    m=re.match(r'## \d+a?\. .*\(`([a-z0-9-]+)`\)',l)
    if m: quest=m.group(1)
    m=re.match(r'### S\d+\. `([a-z0-9-]+)`',l)
    if m: quest=m.group(1)
    if l.startswith('**Gossip and helpful knowledge**'): intable=True; continue
    if intable and l.startswith('| ') and not l.startswith('| Town') and not l.startswith('|---'):
        cells=[c.strip() for c in l.strip().strip('|').split(' | ')]
        if len(cells)==4:
            tc,dc,kc,line=cells
            g=[]
            tl=tc.split()[0].lower()
            if tl in TOWNS: g.append('here.region='+tl)
            g+=[x for p in re.findall(r'`([^`]*)`',tc) for x in p.split() if not x.startswith('here.region')]
            g.append('self.dialect='+dc.strip('`').split()[0])
            keys=re.findall(r'`([^`]*)`',kc)
            g+=['world.mem.'+k if not k.startswith('world.mem') else k for k in keys]
            node='news' if 'helpful' in kc else 'gossip'
            rec('large',quest,node,g,line.strip('"'),i)
        continue
    if intable and not l.strip(): continue
    if intable and not l.startswith('|'): intable=False
    m=re.match(r'Recap \((.*?)\): "(.*)"',l)
    if m:
        g=[]
        for part in [x.strip() for x in m.group(1).split(',')]:
            q=part.strip('`')
            if q=='inn.greet': continue
            if q.lower() in TOWNS: g.append('here.region='+q.lower())
            elif '=' in q: g.append(q if q.startswith('world.mem.') else 'world.mem.'+q)
        OPEN={'lq-old-brown':'lq_brown=rustled','lq-runnel-fever':'lq_runnel=fever','lq-wreckers-light':'lq_wreck=lamp','lq-eel-famine':'lq_eels=weir','lq-vanishing-thief':'lq_thief=walking'}
        if not any(x.startswith('world.mem') for x in g): g.append('world.mem.'+OPEN[quest])
        rec('large',quest,'inn.greet',g,m.group(2),i)
for r in out:
    r['gates']=list(dict.fromkeys(x.replace('\\|','|') for x in r['gates']))
    r['node']={'resident.gossip':'gossip','resident.news':'news','inn.greet':'inn.greet'}.get(r['node'],r['node'])
json.dump(out,open(HERE / 'packs_raw.json','w'),indent=0)
from collections import Counter
print(Counter((r['src'],r['node']) for r in out))
