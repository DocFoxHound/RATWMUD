# The month's economy, from econ_watch's output: money, collectors, tills, wages, materials, notable events.
import csv, json, statistics as st, sys
from collections import defaultdict
D = sys.argv[1]
days = [json.loads(l) for l in open(f"{D}/days.jsonl")]
ends = [d for d in days if d["when"] != "start"]
s0, s1 = days[0], days[-1]
print("== Money ==")
print(f"supply {s0['supply']} -> {s1['supply']}, minted {s0['minted']}->{s1['minted']}, sunk {s1['sunk']}, conserved every day: {all(d['conserved'] for d in days)}")
print("day  median poor10 rich10 richest gini  top10% bot50% grownshort short broke hungry starving open/taken/done/expired contracts")
for d in days:
    c = d["contracts"]
    print(f"{d['day']:>3} {d['median']:>6} {d['poorest_tenth']:>6} {d['richest_tenth']:>6} {d['richest_resident']:>7} {d['gini']:.3f} {d['top_tenth_share']:.3f} {d['bottom_half_share']:.3f} {d.get('grown_short',0):>5} {d['short']:>5} {d['broke']:>5} {d['hungry']:>6} {d['starving']:>4}  {c['open']}/{c['taken']}/{c['done']}/{c['expired']}")
print("\n== By holder (total p / count), start, day 7, 14, 21, end ==")
picks = [days[0]] + [d for d in days if d['day'] in (7, 14, 21)] + [days[-1]]
picks = list({id(p): p for p in picks}.values())
kinds = sorted({k for d in picks for k in d["holders"]})
for k in kinds:
    print(f"  {k:24}" + "".join(f"{d['holders'].get(k,[0,0])[0]:>10}" for d in picks))
print("\n== Collectors (each day's purse) ==")
coll = sorted({k for d in days for k in d["purses"] if k == "treasury" or k.startswith(("stores:", "house:")) or (k.startswith("town:"))})
for k in coll:
    series = [d["purses"].get(k) for d in days]
    vals = [v for v in series if v is not None]
    if not vals: continue
    print(f"  {k:34} start {vals[0]:>6} min {min(vals):>6} max {max(vals):>6} end {vals[-1]:>6}  | " + " ".join(str(v) if v is not None else '-' for v in series[::3]))
print("\n== Shop tills ==")
tillkeys = sorted({k for d in days for k in d["purses"] if k.startswith(("till:", "shop:"))})
for d in days:
    t = [d["purses"][k] for k in tillkeys if k in d["purses"]]
    if t:
        t.sort()
        print(f"  day {d['day']:>2}: {len(t)} tills, total {sum(t)}, median {t[len(t)//2]}, under 5p {sum(v<5 for v in t)}, under 20p {sum(v<20 for v in t)}, over 1000p {sum(v>1000 for v in t)}, largest {t[-1]}")
low_days = defaultdict(int)
for d in ends:
    for k in tillkeys:
        if d["purses"].get(k, 99) < 5: low_days[k] += 1
chronic = sorted(((n, k) for k, n in low_days.items() if n >= 7), reverse=True)
print(f"  tills under 5p at 7+ day ends: {len(chronic)}: " + ", ".join(f"{k}({n})" for n, k in chronic[:25]))
grow = sorted(((days[-1]['purses'].get(k,0) - days[1]['purses'].get(k,0), k) for k in tillkeys if k in days[1]['purses']), reverse=True)
print("  tills that grew most since day 1: " + ", ".join(f"{k} +{g}" for g, k in grow[:10]))
print("\n== Top accounts at the end ==")
for k, v in s1["top_accounts"]:
    print(f"  {k:40} {v:>7}  ({100*v/s1['supply']:.1f}%)")
print("\n== Flows a day by kind (coins) ==")
flows = defaultdict(lambda: defaultdict(int)); goods = defaultdict(lambda: defaultdict(int))
paths = defaultdict(int)
for r in csv.DictReader(open(f"{D}/flows.csv")):
    flows[r["kind"]][int(r["day"])] += int(r["coins"]); goods[r["kind"]][int(r["day"])] += int(r["goods"])
    paths[(r["kind"], r["from"], r["to"])] += int(r["coins"])
ndays = sorted({dd for k in flows for dd in flows[k]})
for k in sorted(flows, key=lambda k: -sum(flows[k].values())):
    tot = sum(flows[k].values())
    if tot == 0 and sum(goods[k].values()) == 0: continue
    print(f"  {k:32} coins {tot:>8} goods {sum(goods[k].values()):>8} | by day: " + " ".join(str(flows[k].get(dd, 0)) for dd in ndays))
print("\n== Biggest coin paths (kind, from, to) over the month ==")
for (k, f, t), c in sorted(paths.items(), key=lambda x: -x[1])[:40]:
    print(f"  {c:>8}  {k} : {f} -> {t}")
print("\n== Wages ==")
w = defaultdict(lambda: defaultdict(int)); titles = {}
for r in csv.DictReader(open(f"{D}/wages.csv")):
    w[r["resident"]][int(r["day"])] += int(r["coins"]); titles[r["resident"]] = r["title"]
for dd in ndays:
    xs = sorted(w[r][dd] for r in w if dd in w[r])
    if xs: print(f"  day {dd:>2}: {len(xs)} paid, total {sum(xs)}, median {xs[len(xs)//2]}, max {xs[-1]}")
# Stability: each worker's daily wage over the last two weeks (working days only), coefficient of variation.
late = [dd for dd in ndays if dd >= max(ndays) - 14]
cvs = []
for r, byd in w.items():
    xs = [byd.get(dd, 0) for dd in late if byd.get(dd, 0) > 0]
    if len(xs) >= 6 and st.mean(xs) > 0: cvs.append(st.pstdev(xs) / st.mean(xs))
if cvs: print(f"  last two weeks, each worker's day-to-day wage variation (cv): median {st.median(cvs):.2f}, 90th pct {sorted(cvs)[int(.9*len(cvs))]:.2f}, n {len(cvs)}")
wk = defaultdict(list)
for dd in ndays: wk[dd // 7].append(sum(w[r].get(dd, 0) for r in w))
print("  wages a day, mean by week: " + ", ".join(f"wk{k+1} {st.mean(v):.0f}" for k, v in sorted(wk.items())))
by_title = defaultdict(list)
for r, byd in w.items():
    by_title[titles[r]].append(sum(byd.get(dd, 0) for dd in late) / max(1, len(late)))
print("  wage a day by title (last two weeks, mean over holders; top 25 by count):")
for t, xs in sorted(by_title.items(), key=lambda x: -len(x[1]))[:25]:
    print(f"    {t[:48]:48} {len(xs):>4} holders, {st.mean(xs):.1f}p a day")
print("\n== Materials (sums over the month; maker/supplier hours short) ==")
m = defaultdict(lambda: defaultdict(int)); held = defaultdict(dict)
for r in csv.DictReader(open(f"{D}/materials.csv")):
    for f in ("maker_hours_short", "supplier_hours_short", "brought_in", "crafted", "used", "bought", "carted_in"):
        m[r["item"]][f] += int(r[f])
    held[r["item"]][int(r["day"])] = int(r["held"])
print(f"  {'item':22} {'mk-short':>8} {'sup-short':>9} {'in':>7} {'crafted':>7} {'used':>7} {'bought':>7} {'carted':>6}  held: start..end")
for it in sorted(m, key=lambda i: -(m[i]['maker_hours_short'] + m[i]['supplier_hours_short'])):
    x = m[it]; h = held[it]; hs = [h[k] for k in sorted(h)]
    print(f"  {it:22} {x['maker_hours_short']:>8} {x['supplier_hours_short']:>9} {x['brought_in']:>7} {x['crafted']:>7} {x['used']:>7} {x['bought']:>7} {x['carted_in']:>6}  {hs[0] if hs else '-'} {hs[len(hs)//2] if hs else '-'} {hs[-1] if hs else '-'}")
print("\n== Events (not ledger) by kind ==")
ev = defaultdict(int); notable = []
for r in csv.DictReader(open(f"{D}/events.csv")):
    ev[r["kind"]] += 1
    if any(w in r["kind"] for w in ("reckon", "sold", "disrepair", "mended", "business", "bankrupt", "starv", "rent")):
        notable.append(r)
for k, n in sorted(ev.items(), key=lambda x: -x[1]): print(f"  {n:>6} {k}")
print("\n== Notable events ==")
for r in notable[:80]: print(f"  day {r['day']} {r['kind']}: {r['actor']} {r['target']} {r['coins']} {r['detail'][:160]}")
