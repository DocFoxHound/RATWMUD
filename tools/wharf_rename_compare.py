#!/usr/bin/env python3
"""Phase 0's compare (doc 73, 2.3): two world_check --economy logs, ridgemere2's and the Wharf's with wharfValve 0, day by
day: every number equal once the renamed keys are mapped (outside:country -> outside:wharf...). Prints the differences."""
import json, sys

RENAME = {'country': 'wharf', 'country_spent': 'basket_spent', 'country_unspent': 'basket_unspent',
          'country_sales': 'wharf_sales', 'country_rents': 'estate_rents', 'country_rents_by_house': 'estate_rents_by_house',
          'country_land_rents': 'estate_land_rents', 'country_land_rents_by_house': 'estate_land_rents_by_house',
          'country_posts_owed': 'estate_posts_owed', 'country_posts_paid': 'estate_posts_paid',
          'country_posts_by_rule': 'estate_posts_by_rule', 'country_posts_held': 'estate_posts_held',
          'patrons_country': 'patrons_estates', 'country_basket_shift': 'wharf_basket_shift', 'outside:country': 'outside:wharf'}
NEW_ONLY = {'wharf_day'}


def norm(v):
    if isinstance(v, dict):
        return {RENAME.get(k, k): norm(x) for k, x in v.items() if k not in NEW_ONLY}
    if isinstance(v, list):
        return [norm(x) for x in v]
    if isinstance(v, str):
        return {'to the Country': 'to the Wharf', 'country': 'wharf'}.get(v, v)
    return v


def diff(a, b, path, out):
    if isinstance(a, dict) and isinstance(b, dict):
        for k in sorted(set(a) | set(b)):
            if k not in a or k not in b:
                out.append(f'{path}.{k}: only in {"base" if k in a else "wharf"}')
            else:
                diff(a[k], b[k], f'{path}.{k}', out)
    elif a != b:
        out.append(f'{path}: {a!r} != {b!r}')


base = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
mine = [json.loads(l) for l in open(sys.argv[2]) if l.strip()]
print(f'{len(base)} and {len(mine)} lines')
problems = []
for i, (a, b) in enumerate(zip(base, mine)):
    out = []
    diff(norm(a), {k: v for k, v in b.items() if k not in NEW_ONLY}, f'line{i}', out)
    problems += out
for p in problems[:60]:
    print(p)
print(f'{len(problems)} differences')
