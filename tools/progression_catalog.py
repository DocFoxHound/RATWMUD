#!/usr/bin/env python3
"""The progression catalog (Docs/Design/49-characters-and-earned-gifts.md): checks Data/Progression/.

    python3 tools/progression_catalog.py        # prints each problem; exits 1 if there is any

skills.json: every attribute and skill has an id (unique), a name, a cap above its start, a soft daily limit, a growth
line with {value}, and a field the server knows (or none, for the trade skills); every source grows known skills by a
positive amount; the rules' numbers are in range. Every number is a placeholder for the user's balance pass: this checks
that the file is whole, not that it is balanced.
"""
from __future__ import annotations

import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
SKILLS = ROOT / 'Data/Progression/skills.json'
STANDING = ROOT / 'Data/Progression/standing.json'
CREATION = ROOT / 'Data/Progression/creation.json'
# The Entity fields World::practiceSlot knows (Core/RatwProgress.cpp).
FIELDS = {'strength', 'dexterity', 'wisdom', 'endurance', 'hearing', 'vision', 'smell', 'fightingSkill', 'sneakSkill',
          'hearingSkill', 'scentSkill'}
ATTRIBUTES = ['strength', 'dexterity', 'wisdom', 'stamina', 'hearing', 'vision', 'smell']
PARTNERS = ('player', 'resident', 'animal', 'fierce', 'post')
TEACHERS = ('mentor', 'trainer', 'master')


def number(value) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def problems(doc: dict | None = None) -> list[str]:
    doc = doc if doc is not None else json.loads(SKILLS.read_text(encoding='utf-8'))
    found: list[str] = []
    if not doc.get('about'):
        found.append('skills.json: no "about" line')
    entries = [('attribute', e) for e in doc.get('attributes', [])] + [('skill', e) for e in doc.get('skills', [])]
    ids = [e.get('id') for _, e in entries]
    if len(set(ids)) != len(ids):
        found.append('attribute and skill ids repeat')
    attributes = [e.get('id') for kind, e in entries if kind == 'attribute']
    if attributes != ATTRIBUTES:
        found.append(f'the attributes are {ATTRIBUTES}, not {attributes}')
    for kind, e in entries:
        eid = e.get('id') or '?'
        if not e.get('name'):
            found.append(f'{eid}: no name')
        if kind == 'attribute' and not e.get('field'):
            found.append(f'{eid}: an attribute lives in a field')
        if e.get('field') and e['field'] not in FIELDS:
            found.append(f'{eid}: unknown field {e["field"]!r}')
        for key in ('start', 'cap', 'softPerDay', 'lineStep'):
            if not number(e.get(key)):
                found.append(f'{eid}: {key} must be a number')
        if number(e.get('start')) and number(e.get('cap')) and e['cap'] <= e['start']:
            found.append(f'{eid}: its cap is no higher than its start')
        if number(e.get('softPerDay')) and e['softPerDay'] <= 0:
            found.append(f'{eid}: softPerDay must be above 0')
        if number(e.get('lineStep')) and e['lineStep'] <= 0:
            found.append(f'{eid}: lineStep must be above 0')
        if 'quickenedCap' in e and (not number(e['quickenedCap']) or e['quickenedCap'] < e.get('cap', 0)):
            found.append(f'{eid}: a Quickened cap is at least the cap')
        if 'specialtyStart' in e and number(e.get('cap')) and (not number(e['specialtyStart']) or e['specialtyStart'] > e['cap']):
            found.append(f'{eid}: a specialty starts at most at the cap')
        if e.get('format', 'whole') not in ('whole', 'percent'):
            found.append(f'{eid}: format is whole or percent')
        if '{value}' not in e.get('line', ''):
            found.append(f'{eid}: a growth line with {{value}}')
    known = set(ids)
    sources = doc.get('sources', [])
    sids = [s.get('id') for s in sources]
    if len(set(sids)) != len(sids):
        found.append('source ids repeat')
    for s in sources:
        sid = s.get('id') or '?'
        grows = s.get('grows')
        if not isinstance(grows, dict) or not grows:
            found.append(f'{sid}: grows nothing')
            continue
        for skill, base in grows.items():
            if skill not in known:
                found.append(f'{sid}: grows unknown skill {skill!r}')
            if not number(base) or base <= 0:
                found.append(f'{sid}: {skill} must grow by a positive number')
    partners = doc.get('partners', {})
    for key in PARTNERS + ('weakerBy', 'weaker', 'betterBy', 'better'):
        if not number(partners.get(key)) or partners[key] < 0:
            found.append(f'partners: {key} must be a number, 0 or more')
    teachers = doc.get('teachers', {})
    for key in TEACHERS + ('reach', 'betterBy', 'nearby', 'cacheSeconds'):
        if not number(teachers.get(key)) or teachers[key] < 0:
            found.append(f'teachers: {key} must be a number, 0 or more')
    rules = {'room': ('floor',), 'soft': ('day', 'after'), 'rested': ('awayFor', 'perDayAway', 'most', 'rate'),
             'variety': ('occasion', 'window', 'repeats', 'factor', 'ring', 'block'), 'lines': ('throttle',)}
    for section, keys in rules.items():
        for key in keys:
            if not number(doc.get(section, {}).get(key)) or doc[section][key] < 0:
                found.append(f'{section}: {key} must be a number, 0 or more')
    for section, key in (('room', 'floor'), ('soft', 'after'), ('variety', 'factor')):
        if number(doc.get(section, {}).get(key)) and doc[section][key] > 1:
            found.append(f'{section}: {key} is a share, at most 1')
    decay = doc.get('partnerDecay', [])
    if not decay or not all(number(d) and 0 <= d <= 1 for d in decay) or decay != sorted(decay, reverse=True):
        found.append('partnerDecay: shares from 1 down, never rising')
    if '{name}' not in doc.get('lines', {}).get('cap', ''):
        found.append('lines: a cap line with {name}')
    bands = doc.get('bands', {})
    if not (number(bands.get('seasoned')) and number(bands.get('veteran')) and 0 < bands['seasoned'] < bands['veteran'] <= 1):
        found.append('bands: seasoned and veteran are shares of the way, 0 < seasoned < veteran <= 1')
    return found


SOCIAL = {'qualified_session_settlement', 'gold_star', 'story_star', 'story_closure'}


def standing_problems(doc: dict | None = None) -> list[str]:
    """standing.json: the social level curve, its titles (from level 1, rising), the social receipts, the day's cap."""
    doc = doc if doc is not None else json.loads(STANDING.read_text(encoding='utf-8'))
    found: list[str] = []
    if not doc.get('about'):
        found.append('standing.json: no "about" line')
    curve = doc.get('curve', {})
    if not number(curve.get('stepBase')) or curve['stepBase'] < 1:
        found.append('curve: stepBase must be a number, 1 or more')
    if not number(curve.get('stepGrowth')) or curve['stepGrowth'] < 0:
        found.append('curve: stepGrowth must be a number, 0 or more')
    levels = [t.get('level') for t in doc.get('titles', [])]
    if not levels or levels[0] != 1 or levels != sorted(set(levels)) or not all(t.get('title') for t in doc.get('titles', [])):
        found.append('titles: from level 1, each level once and rising, each with a title')
    reasons = doc.get('socialReasons', [])
    if not reasons or not set(reasons) <= SOCIAL:
        found.append(f'socialReasons: some of {sorted(SOCIAL)}')
    if not number(doc.get('dailyCap')) or doc['dailyCap'] < 1:
        found.append('dailyCap must be a number, 1 or more')
    unlocks = doc.get('unlocks', {})
    gifted, quickened = unlocks.get('gifted', {}), unlocks.get('quickened', {})
    for name, tier, keys in (('gifted', gifted, ('socialLevel', 'normalScenes')),
                             ('quickened', quickened, ('socialLevel', 'stars', 'starGivers', 'closedStories', 'reportDays'))):
        for key in keys:
            if not number(tier.get(key)) or tier[key] < 0:
                found.append(f'unlocks: {name}.{key} must be a number, 0 or more')
    if number(gifted.get('socialLevel')) and number(quickened.get('socialLevel')) and quickened['socialLevel'] < gifted['socialLevel']:
        found.append("unlocks: Quickened's social level is no lower than Gifted's")
    if number(quickened.get('starGivers')) and number(unlocks.get('givers')) and unlocks['givers'] < quickened['starGivers']:
        found.append('unlocks: givers (how many are counted) is at least starGivers')
    return found


def creation_problems(doc: dict | None = None, skills: dict | None = None) -> list[str]:
    """creation.json: every attribute's three grades (start below cap, weak below plain below strong; plain as skills.json
    has it), the budget and limits, specialties naming real skills, and presets within the budget."""
    doc = doc if doc is not None else json.loads(CREATION.read_text(encoding='utf-8'))
    skills = skills if skills is not None else json.loads(SKILLS.read_text(encoding='utf-8'))
    found: list[str] = []
    attributes = {a['id']: a for a in skills.get('attributes', []) if isinstance(a, dict) and 'id' in a}
    known = set(attributes) | {s.get('id') for s in skills.get('skills', [])}
    grades = doc.get('grades', {})
    if set(grades) != set(attributes):
        found.append(f'grades: one entry for each attribute ({sorted(attributes)})')
    for aid, g in grades.items():
        pairs = [g.get(k) for k in ('weak', 'plain', 'strong')]
        if not all(isinstance(p, list) and len(p) == 2 and all(number(x) for x in p) and p[0] < p[1] for p in pairs):
            found.append(f'{aid}: weak, plain and strong, each [start, cap] with start below cap')
            continue
        if not (pairs[0][0] <= pairs[1][0] <= pairs[2][0] and pairs[0][1] <= pairs[1][1] <= pairs[2][1]):
            found.append(f'{aid}: weak below plain below strong')
        if aid in attributes and (pairs[1][0] != attributes[aid].get('start') or pairs[1][1] != attributes[aid].get('cap')):
            found.append(f'{aid}: plain is skills.json\'s start and cap')
    for key in ('budget', 'strongCost', 'weakRefund', 'mostStrong', 'mostWeak'):
        if not number(doc.get(key)) or doc[key] < 0:
            found.append(f'{key} must be a number, 0 or more')
    specs = {s.get('id') for s in doc.get('specialties', [])}
    for s in doc.get('specialties', []):
        if s.get('skill') not in known or not s.get('name'):
            found.append(f'specialty {s.get("id")}: a name and a known skill')
    budget = doc.get('budget', 0)
    for p in doc.get('presets', []):
        strong, weak = p.get('strong', []), p.get('weak', [])
        if any(a not in grades for a in strong + weak) or set(strong) & set(weak):
            found.append(f'preset {p.get("id")}: known attributes, none both strong and weak')
        spent = len(strong) * doc.get('strongCost', 1) - len(weak) * doc.get('weakRefund', 1)
        if spent > budget or len(strong) > doc.get('mostStrong', 3) or len(weak) > doc.get('mostWeak', 3):
            found.append(f'preset {p.get("id")}: over the budget or the limits')
        if p.get('specialty') and p['specialty'] not in specs:
            found.append(f'preset {p.get("id")}: unknown specialty {p["specialty"]!r}')
    for tier in ('normal', 'gifted', 'quickened'):
        if not number(doc.get('tierCost', {}).get(tier)):
            found.append(f'tierCost: {tier} must be a number')
    return found


if __name__ == '__main__':
    found = problems() + standing_problems() + creation_problems()
    for p in found:
        print(p)
    print('Progression catalog: ' + ('OK' if not found else f'{len(found)} problem(s)'))
    sys.exit(1 if found else 0)
