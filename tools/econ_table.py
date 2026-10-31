# econ_table.py FIELD[,FIELD...] A.jsonl B.jsonl ...: a table of chosen fields by day, one column per run.
import json, os, sys
fields = sys.argv[1].split(",")
runs = [(os.path.basename(p).replace(".jsonl", ""), [json.loads(l) for l in open(p) if l.strip()]) for p in sys.argv[2:]]
days = min(len(r) for _, r in runs)
print("| day | field | " + " | ".join(n for n, _ in runs) + " |")
print("|---|---|" + "---|" * len(runs))
for d in range(days):
    for f in fields:
        vals = [r[d].get(f) for _, r in runs]
        fmt = lambda v: f"{v:.3f}" if isinstance(v, float) else str(v)
        print(f"| {runs[0][1][d]['day']} | {f} | " + " | ".join(fmt(v) for v in vals) + " |")
