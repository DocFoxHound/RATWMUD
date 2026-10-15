"""Who holds the households' money after a run, and who has none (doc 63): by decile, and the poorer half's makeup."""
import collections, sys
import model
start = sys.argv[1] if len(sys.argv) > 1 else "normal"
days = int(sys.argv[2]) if len(sys.argv) > 2 else 1820
w = model.World(start, 1)
for _ in range(days):
    w.step()
mw = w.market_wage()
keepers = {f.owner for f in w.all_firms()}
def kind(h):
    if h.id in keepers: return "keeper"
    if not h.workers: return "no earner"
    if all(x.employer is None for x in h.workers): return "idle"
    return "working"
hs = sorted(w.households, key=lambda h: w.L.cash[h.id])
n = len(hs)
print(f"day {days}: a labourer's wage {mw:.2f}; households' money by decile (in days of a labourer's wage):")
for d in range(10):
    part = hs[d * n // 10:(d + 1) * n // 10]
    kinds = collections.Counter(kind(h) for h in part)
    inc = sum(h.month_income() for h in part) / len(part) / mw
    earners = sum(len(h.workers) for h in part) / len(part)
    heads = sum(h.persons for h in part) / len(part)
    print(f"  {d+1:2}: money {sum(w.L.cash[h.id] for h in part) / len(part) / mw:7.1f} d, month's income {inc:5.1f} d, "
          f"heads {heads:.1f}, earners {earners:.1f}, hungry {sum(h.hungry for h in part):3}, {dict(kinds)}")
poor = hs[: n // 2]
emp = collections.Counter(type(x.employer).__name__ if x.employer is None else x.employer.id.split(':')[0] + ':' + x.employer.id.split(':')[1]
                          for h in poor for x in h.workers)
print("the poorer half's earners work at:", dict(emp.most_common(12)))
wages = collections.defaultdict(list)
for x in w.workers:
    if x.employer is not None:
        wages[x.employer.id.split(':')[0] + ':' + x.employer.id.split(':')[1]].append(x.employer.wage / mw)
print("wage by employer (x a labourer's):", {k: round(sum(v) / len(v), 2) for k, v in sorted(wages.items())})
print("month's flows:", {k: round(v) for k, v in sorted(w.month_flows.items(), key=lambda x: -x[1])[:14]})
