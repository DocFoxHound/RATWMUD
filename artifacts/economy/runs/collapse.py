# Exit 0 (collapse) if the run's last days show a sustained famine: starving >= 25 for 14 days running, or hungry >= 60
# for 28 days running. Reads only the last lines of days.jsonl.
import json,sys,subprocess
lines=subprocess.run(['tail','-n','40',sys.argv[1]],capture_output=True,text=True).stdout.splitlines()
recs=[]
for l in lines:
    try:
        o=json.loads(l)
        if o.get('when')!='start': recs.append(o)
    except Exception: pass
s=[r.get('starving',0) for r in recs]; h=[r.get('hungry',0) for r in recs]
if len(s)>=14 and min(s[-14:])>=25: print(f"collapse: starving >= 25 for 14 days (day {recs[-1]['day']}: {s[-1]})"); sys.exit(0)
if len(h)>=28 and min(h[-28:])>=60: print(f"collapse: hungry >= 60 for 28 days (day {recs[-1]['day']}: {h[-1]})"); sys.exit(0)
sys.exit(1)
