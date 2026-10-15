"""Each trade's state after a run (doc 63's model): hands, sales, shelf, margin, wage against a labourer's, cost and
price in a labourer's day-wages against the ladder, money, days out of stock; and where households' money sits."""
import collections
import sys
import model

start = sys.argv[1] if len(sys.argv) > 1 else "normal"
days = int(sys.argv[2]) if len(sys.argv) > 2 else 840
if "--no-poach" in sys.argv:
    model.POACH["on"] = False
w = model.World(start, 1)
for _ in range(days):
    w.step()
mw = w.market_wage()
print(f"day {days}, a labourer's wage {mw:.2f}, idle {w.slack:.1%}")
print(f"{'good':10} {'hands':>5} {'vac':>3} {'sales':>7} {'shelf d':>7} {'margin':>6} {'wage/lab':>8} {'cost LD':>7} "
      f"{'price LD':>8} {'target':>6} {'money':>7} {'out':>4}")
for g, fs in w.firms.items():
    s = sum(f.daily_sales() for f in fs)
    st = sum(f.stock for f in fs)
    print(f"{g:10} {sum(len(f.workers) for f in fs):5} {sum(f.vacancies for f in fs):3} {s:7.1f} {st / max(s, 0.01):7.1f} "
          f"{sum(f.markup for f in fs) / len(fs):6.2f} {sum(f.wage for f in fs) / len(fs) / mw:8.2f} "
          f"{sum(f.unit_cost for f in fs) / len(fs) / mw:7.3f} {w.price_of(g) / mw:8.3f} {model.TARGETS[g]:6.3f} "
          f"{sum(w.L.cash[f.id] for f in fs):7.0f} {w.month_out.get(g, 0):4}")
keepers = {f.owner for f in w.all_firms()}
groups = collections.defaultdict(list)
for h in w.households:
    g = "keeper" if h.id in keepers else "no earner" if not h.workers else \
        "idle" if all(x.employer is None for x in h.workers) else "working"
    groups[g].append(h)
for g, hs in groups.items():
    cash = sorted(w.L.cash[h.id] for h in hs)
    print(f"{g:10} {len(hs):4} homes, money {sum(cash):8.0f}, median {cash[len(cash) // 2]:7.1f}, "
          f"a month's income {sum(h.month_income() for h in hs) / len(hs):7.1f}, hungry {sum(h.hungry for h in hs)}")
print("mended / bought new, last month (pennies):", {g: (round(w.month_flows.get(f"mends {g}", 0)),
      round(w.month_flows.get(f"buys {g}", 0))) for g in ("sword", "rapier", "cloth", "finery")})
owned = collections.Counter(p.good for h in w.households for p in h.owned)
worn = collections.Counter(p.good for h in w.households for p in h.owned if p.cond < model.MEND_AT)
print("households own:", dict(owned), "worn and waiting:", dict(worn),
      "clothes worn:", sum(1 for h in w.households if h.clothes.cond < model.MEND_AT))
print("month's flows:", {k: round(v) for k, v in sorted(w.month_flows.items(), key=lambda x: -x[1])})
