# econ_diff.py A.jsonl B.jsonl [C.jsonl ...]: day by day, the economy line's fields that differ between the runs.
import json, sys
runs = [[json.loads(l) for l in open(p) if l.strip()] for p in sys.argv[1:]]
days = min(len(r) for r in runs)
for d in range(days):
    rows = [r[d] for r in runs]
    diff = {k: [row.get(k) for row in rows] for k in rows[0] if len(set(json.dumps(row.get(k)) for row in rows)) > 1}
    print(f"day {rows[0]['day']}: {len(diff)} of {len(rows[0])} fields differ")
    for k, v in diff.items():
        if k != "hungry_who":
            print(f"   {k}: {v}")
