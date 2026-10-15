"""On days the town goes short of bread, what each link of the chain held (doc 63): which one ran dry first."""
import sys
import model

start = sys.argv[1] if len(sys.argv) > 1 else "normal"
days = int(sys.argv[2]) if len(sys.argv) > 2 else 1092
money = int(sys.argv[3]) if len(sys.argv) > 3 else 1_000_000
w = model.World(start, 1, money=money)
prev = None
print(f"{'day':>5} {'sday':>4} {'hungry':>6} | {'bread':>6} {'bakers':>6} {'b.flour':>7} {'b.wood':>6} | {'flour':>6} {'millers':>7} "
      f"{'m.wheat':>7} | {'wood':>6} {'cutters':>7} {'tools':>5} | {'wheat':>7} | {'cart use':>8} {'carters':>7} | wage")
for d in range(days):
    w.step()
    hungry = w.log[-1]["hungry"]
    if hungry > 50 or (prev is not None and prev > 50 and hungry <= 50) or d % 91 == 0:
        F = w.firms
        bakers = F["bread"]; mills = F["flour"]; cutters = F["wood"]
        room = len(w.carter.workers) * model.HOURS / model.CARRY_PER_WEIGHT
        print(f"{d:5} {d % 364:4} {hungry:6} | {sum(f.stock for f in bakers):6.0f} {sum(len(f.workers) for f in bakers):6} "
              f"{sum(f.inputs['flour'] for f in bakers):7.0f} {sum(f.inputs['wood'] for f in bakers):6.0f} | "
              f"{sum(f.stock for f in mills):6.0f} {sum(len(f.workers) for f in mills):7} {sum(f.inputs['wheat'] for f in mills):7.0f} | "
              f"{sum(f.stock for f in cutters):6.0f} {sum(len(f.workers) for f in cutters):7} {sum(f.tools for f in cutters):5.1f} | "
              f"{sum(f.stock for f in F['wheat']):7.0f} | {w.carter.sales[-1] / max(room, 1):8.2f} {len(w.carter.workers):7} | "
              f"{w.market_wage():.2f}")
    prev = hungry
