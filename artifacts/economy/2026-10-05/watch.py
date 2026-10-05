# Follows the month run: one line a game day, with alarms for broke collectors, hoarding, hunger and lost money.
import json, os, sys, time
S = sys.argv[1]; pid = int(sys.argv[2]); seen = int(sys.argv[3]) if len(sys.argv) > 3 else 0
run = sys.argv[4] if len(sys.argv) > 4 else "month"
path = f"{S}/{run}/days.jsonl"
first = None
def alive():
    try: os.kill(pid, 0); return True
    except OSError: return False
while True:
    lines = open(path).read().splitlines() if os.path.exists(path) else []
    if lines and first is None: first = json.loads(lines[0])
    for line in lines[seen:]:
        d = json.loads(line); seen += 1
        if d["when"] == "start": continue
        p = d["purses"]; alarms = []
        if not d["conserved"]: alarms.append("MONEY NOT CONSERVED")
        coll = {k: v for k, v in p.items() if k == "treasury" or k.startswith(("stores:", "house:")) or k.endswith(":church")}
        broke = [k for k, v in coll.items() if v < 20]
        if broke: alarms.append("collectors under 20p: " + ", ".join(sorted(broke))[:200])
        tills = {k: v for k, v in p.items() if k.startswith(("till:", "shop:"))}
        empty = sum(1 for v in tills.values() if v < 5)
        if empty > len(tills) * 0.1: alarms.append(f"{empty}/{len(tills)} shop tills under 5p")
        top = d["top_accounts"][0]
        if top[1] > d["supply"] * 0.15: alarms.append(f"{top[0]} holds {top[1]}p ({100*top[1]/d['supply']:.0f}% of all)")
        if d.get("grown_short", d["short"]) > d["residents"] / 15: alarms.append(f"{d.get('grown_short')} grown wolves short of food money (>7%)")
        if d["starving"]: alarms.append(f"{d['starving']} starving")
        h = d["holders"]
        print(f"day {d['day']}: median {d['median']}p gini {d['gini']:.3f} grown short {d.get('grown_short')} hungry {d['hungry']} starving {d['starving']}; "
              f"treasuries {h.get('town treasury',[0])[0]+h.get('capital treasury',[0])[0]}p, houses {h.get('great house',[0])[0]}p, "
              f"tills {sum(tills.values())}p; top {top[0]} {top[1]}p" + ("; ALARM: " + "; ".join(alarms) if alarms else ""), flush=True)
    if not alive():
        tail = open(f"{S}/{run}.log").read().splitlines()[-2:]
        print("RUN ENDED: " + " | ".join(tail), flush=True); break
    time.sleep(20)
