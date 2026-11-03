import json, sys
for path in sys.argv[1:]:
    a = [json.loads(l) for l in open(path)]
    seas = {}
    for d in a[1:]:
        seas.setdefault(d['season'], []).append(d)
    print(path.split('/')[-1])
    for s, L in seas.items():
        n = len(L)
        m = lambda k: sum(x[k] for x in L) / n
        print(f'  {s:7} hungry {m("hungry_mean"):5.2f} grain sold {m("grain_sold"):4.0f} (drawn {m("grain_drawn"):3.0f}/{m("grain_cap"):3.0f}) '
              f'fish sold {m("fish_sold"):4.0f} @ {m("fish_paid"):.2f} fruit drawn {m("fruit_drawn"):4.0f}/{m("fruit_cap"):4.0f} '
              f'preserves sold {m("preserved_sold"):4.0f} stock {m("preserved_stock"):4.0f} (most {max(x["preserved_stock"] for x in L)}) '
              f'meat {m("meat_sold"):3.0f} dairy {m("dairy_sold"):3.0f} spent {m("residents_spent"):5.0f} needs {m("needs_bought"):3.0f}')
