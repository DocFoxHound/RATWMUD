import json, sys, collections
sys.path.insert(0, '/home/martinb/Documents/GitHub/MUDProject/.claude/worktrees/agent-afce1bb4c5ae0bf2f/tools')
import econ_detail as E
path, start, end = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
rooms = E.class_map('/home/martinb/Documents/GitHub/MUDProject/artifacts/ridgemere2/buildings.json')
WAGE = {'keeper': 14, 'hand': 8, 'public': 8, 'watch': 8, 'country': 8}
agg = collections.defaultdict(collections.Counter)
for line in open(path):
    d = json.loads(line)
    if not (start <= d['day'] <= end):
        continue
    for key, h in d['households'].items():
        cls = E.household_class(key, h['ids'].split(','), rooms)
        posts = h['posts'].split(',')
        c = agg[cls]
        c['hh'] += 1
        c['n'] += h['n']
        c['adults'] += h['adults']
        c['earners'] += sum(p in WAGE for p in posts)
        c['posted'] += sum(WAGE.get(p, 0) for p in posts)
        c['wages'] += sum(v for k, v in h['in'].items() if k.startswith('wages'))
        c['profit'] += h['in'].get('profit', 0)
        c['alms'] += h['in'].get('alms', 0)
        c['food'] += h['out'].get('food', 0)
        c['goods'] += h['out'].get('goods', 0)
        c['cash'] += h['cash']
        c['hungry_hours'] += h['hungry_hours']
        c['hungry_hh'] += h['hungry_hours'] > 0
        c['nowage_hh'] += not any(p in WAGE for p in posts)
print(f'{"class":7} {"hh":>4} {"size":>5} {"earn":>5} {"posted":>7} {"paid":>6} {"profit":>6} {"alms":>5} {"food":>6} {"f/head":>6} {"needs+":>6} {"cash":>6} {"hungry%":>7} {"hh hungry":>9}')
for cls, c in sorted(agg.items()):
    hh = c['hh']
    days = end - start + 1
    print(f'{cls:7} {hh / days:4.0f} {c["n"] / hh:5.2f} {c["earners"] / hh:5.2f} {c["posted"] / hh:7.1f} {c["wages"] / hh:6.1f} '
          f'{c["profit"] / hh:6.1f} {c["alms"] / hh:5.1f} {c["food"] / hh:6.1f} {c["food"] / c["n"]:6.2f} {c["goods"] / hh:6.1f} '
          f'{c["cash"] / hh:6.0f} {100 * c["hungry_hours"] / (24 * c["n"]):7.1f} {c["hungry_hh"] / days:9.1f}')
