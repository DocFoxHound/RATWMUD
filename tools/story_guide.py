#!/usr/bin/env python3
"""The Story Maker's guide (Docs/Story-Maker-guide.md; Docs/Design/85-story-triggers-and-campaign-control.md, Phase 10): its
reference part is made from the trigger catalogue (Data/Storylines/trigger-catalogue.json), so every event, condition,
action and world effect is explained with its sentence, its fields or parameters, what it needs on a hosted world, what the
game undoes when the run ends, and an example. The written part above it is kept by hand. The Editor's Guide tab shows the
same file.

    python3 tools/story_guide.py --write    rewrite the reference between its markers
    python3 tools/story_guide.py --check    exit 1 when the reference is not the catalogue's (the test runs this)
"""
import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import story_program as rules  # noqa: E402

GUIDE = ROOT / 'Docs' / 'Story-Maker-guide.md'
BEGIN = '<!-- reference:begin (made by tools/story_guide.py from Data/Storylines/trigger-catalogue.json: do not edit by hand) -->'
END = '<!-- reference:end -->'

# Sample values for an example, by parameter type: ids of the Greyfen fixture where the examples are rehearsed.
SAMPLE = {
    'wolf': '$actor', 'resident': 'rm111', 'character': 'rm111', 'place': 'greyfen-square', 'cell': 'greyfen-square', 'item': 'rope',
    'faction': 'watch', 'town': 'greyfen', 'market': 'greyfen', 'ship': 'the-heron', 'area': 'greyfen-fields', 'template': 'the-debt',
    'project': 'granary', 'scene': 'the-oath', 'cue': 'begin', 'region': 'square', 'phase': 'muster', 'mission': 'gather-rope',
    'signal': 'gate-opens', 'counter': 'ropes', 'variable': 'ropes', 'trigger': 'count-rope', 'quest': 'axle', 'objective': 'ask',
    'purse': 'town:greyfen', 'offer': 'ask-about-the-ford', 'door': 'link_inn_door_a', 'mantle': 'ferryman', 'visitor': 'stranger-1',
    'story': 'the-crossing', 'notice': 'notice-1', 'contract': 'contract-1', 'letter': 'letter-1', 'run': 'world-…',
}
TEXT = {'text': 'The bells ring for the ford.', 'words': 'the ford is open again', 'reason': 'for the ford', 'detail': 'Kept the ford.',
        'label': 'The ford', 'name': 'The Pale Rider', 'title': 'The ford is open'}
# Examples that need more than one value to agree (the checks of story_program.validate_effect).
EXAMPLE = {
    'story.tell': {'to': 'participants', 'text': 'The bells ring for the ford.'},
    'weather.set': {'town': 'greyfen', 'weather': 'snow', 'hours': 6},
    'story.start': {'story': 'the-crossing', 'audience': 'same', 'carry': True, 'when': 'ended'},
    'scene.play': {'scene': 'the-oath', 'player': '$actor', 'cue': 'begin'},
    'character.grant': {'player': '$actor', 'coins': 12, 'purse': 'town:greyfen', 'reason': 'the carter thanks you'},
    'faction.relation': {'faction': 'watch', 'other': 'road-company', 'stance': 'tense', 'disposition': -30, 'reason': 'the blockade'},
    'story.tracker': {'shown': False, 'var': 'ropes'},
    'visitor.add': {'visitor': 'pale-rider', 'name': 'The Pale Rider', 'like': 'rm111', 'cell': 'greyfen-square', 'x': 12, 'y': 9, 'minutes': 60},
}
GROUP_NOTES = {
    'flow': 'Version 2 only. Nested lists of actions: up to 24 actions in a trigger counting nested ones, three deep.',
}


def _sample(p):
    if 'default' in p:
        return p['default']
    kind = p['type']
    if kind == 'int':
        low, high = p['min'], p['max']
        v = 1 if low <= 1 <= high else low
        return 2 if v == 0 and p.get('nonzero') and high >= 2 else v
    if kind == 'days':
        return 1
    if kind == 'choice':
        return p['values'][0]
    if kind == 'bool':
        return True
    if kind == 'text':
        return TEXT.get(p['key'], 'A few words.')[:p.get('max', 300)]
    if kind == 'operand':
        return 1
    if kind == 'whose':
        return p.get('values', ['$actor'])[0]
    return SAMPLE.get(kind, SAMPLE.get(p['key'], p['key']))


def example_effect(e):
    if e['id'] in EXAMPLE:
        return {'kind': e['id'], **EXAMPLE[e['id']]}
    out = {'kind': e['id']}
    for p in e.get('params', []):
        if not p.get('optional'):
            out[p['key']] = _sample(p)
    return out


def example_condition(c):
    if c.get('group'):
        return {'test': c['id'], **({'condition': {'test': 'switch', 'var': 'gate-open', 'is': True}} if c['id'] == 'not' else
                                     {'conditions': [{'test': 'switch', 'var': 'gate-open', 'is': True},
                                                     {'test': 'compare', 'left': {'var': 'ropes'}, 'op': '>=', 'right': 2}]})}
    out = {'test': c['id']}
    for p in c.get('params', []):
        if not p.get('optional'):
            out[p['key']] = _sample(p)
    if c['id'] == 'compare':
        out.update(left={'var': 'ropes'}, op='>=', right=2)
    return out


def example_action(a):
    do = a.get('do', a['id'])
    if do == 'if':
        return {'do': 'if', 'if': {'test': 'compare', 'left': {'var': 'ropes'}, 'op': '>=', 'right': 2},
                'then': [{'do': 'note', 'text': 'Enough rope.'}], 'else': [{'do': 'add', 'var': 'ropes', 'value': 1}]}
    if do == 'each':
        return {'do': 'each', 'over': 'participants', 'actions': [{'do': 'effect', 'effect': {'kind': 'deed.award', 'player': '$each',
                                                                                              'detail': 'Held the ford.'}}]}
    if do == 'random':
        return {'do': 'random', 'choices': [{'weight': 3, 'actions': [{'do': 'emit', 'signal': 'fair-weather'}]},
                                            {'weight': 1, 'actions': [{'do': 'emit', 'signal': 'storm'}]}]}
    if do == 'effect':
        return {'do': 'effect', 'effect': {'kind': 'deed.award', 'player': '$actor', 'detail': 'Kept the ford.'}}
    out = {'do': do}
    for p in a.get('params', []):
        if not p.get('optional'):
            out[p['key']] = _sample(p)
    return out


def example_event(e):
    t = {'id': 'when-' + re.sub(r'[^a-z0-9]+', '-', e['id']).strip('-'), 'on': e['id']}
    need = e.get('needsMatch') or []
    match = {}
    for k in need:
        f = e['fields'].get(k, {})
        match[k] = '500' if k == 'detail' else SAMPLE.get(f.get('type', ''), 'x')
    if not match:
        for k in ('target', 'item'):
            f = e.get('fields', {}).get(k)
            if f and f.get('type') in ('resident', 'item', 'region', 'place', 'faction', 'story'):
                match[k] = SAMPLE[f['type']]
                break
    if match:
        t['match'] = match
    if e['id'] == rules.triggers.SIGNAL:
        t['signal'] = 'gate-opens'
    t['actions'] = [{'do': 'note', 'text': e.get('title', e['id'])}]
    return t


def _sentence(text, params, ex, words_of=None):
    """A sentence with its slots filled from an example (the words a parameter has for its value, or the value)."""
    by_key = {p['key']: p for p in params}

    def fill(m):
        out = []
        for key in m.group(1).split(','):
            v = ex.get(key)
            if v is None:
                continue
            p = by_key.get(key, {})
            w = (p.get('words') or {}).get(str(v).lower() if isinstance(v, bool) else str(v))
            out.append(str(w) if w not in (None, '') else (json.dumps(v) if not isinstance(v, str) else v))
        return '**' + ', '.join(out) + '**' if out else ''
    said = re.sub(r'\s+', ' ', re.sub(r'\{([^}]+)\}', fill, text)).strip()
    said = re.sub(r'\s+([,.:;])', r'\1', said)
    return re.sub(r'[,:;](?=[,:;]|$)', '', said).strip()


def _params(params):
    rows = []
    for p in params:
        bits = [p['type']]
        if p['type'] == 'int':
            bits.append(f"{p['min']} to {p['max']}" + (', not 0' if p.get('nonzero') else ''))
        if p['type'] == 'text' and 'max' in p:
            bits.append(f"up to {p['max']} characters")
        if p.get('values'):
            bits.append('one of: ' + ', '.join(f'`{v}`' for v in p['values']))
        if p.get('of'):
            bits.append('a variable of type ' + ' or '.join(p['of']))
        if p.get('optional'):
            bits.append('optional')
        if 'default' in p:
            bits.append(f"default {json.dumps(p['default'])}")
        rows.append(f"| `{p['key']}` | {p.get('label', '')} | {'; '.join(bits)} |")
    return ['| Parameter | What | Kind |', '|---|---|---|', *rows] if rows else []


def _code(value):
    return ['```json', json.dumps(value, ensure_ascii=False), '```']


def _anchor(kind, ident):
    return f'<a id="{kind}-{re.sub(r"[^a-z0-9]+", "-", ident.lower()).strip("-")}"></a>'


def reference(cat=None):
    cat = cat or rules.CATALOGUE
    out = [BEGIN, '', '## Reference: every event, condition, action and effect', '',
           'Made from the trigger catalogue (`Data/Storylines/trigger-catalogue.json`), the same one the workbench draws its sentences '
           'and pickers from and the checks read their bounds from. **v2** marks what needs a version 2 story; **watched** an event '
           'the game writes only while a live story listens for it; **the story\'s own** what the story keeps itself rather than '
           'sending to the game; **acts on wolves** what needs the owner or "may act on players\' wolves" on a hosted world; **needs a '
           'builder** what needs a builder or the owner there; **undone at the end** what the game takes back when the run ends.', '']
    groups = {g['id']: g['label'] for g in cat['groups']}
    out += ['### Events', '', f"{len(cat['events'])} events. A trigger names one (`\"on\"`) and may filter its fields by exact text "
            '(`"match"`: actor, target, cell, item, detail). Fields not listed are empty for that event.', '']
    for gid, label in groups.items():
        evs = [e for e in cat['events'] if e.get('group') == gid]
        if not evs:
            continue
        out += [f'#### {label}', '']
        for e in evs:
            marks = [m for m, on in (('watched', e.get('watch')), ("the story's own", e.get('story')), ('mission evidence', e.get('mission')),
                                     ('has a tile', e.get('tile'))) if on]
            out += [f"{_anchor('event', e['id'])}**`{e['id']}`**: {e.get('title', '')}" + (f" ({', '.join(marks)})" if marks else ''), '']
            out += [e.get('help', ''), '']
            fields = e.get('fields', {})
            if fields:
                out += ['| Field | Holds | Left out means |', '|---|---|---|',
                        *[f"| `{k}` | {f.get('label', '')} ({f.get('type', '')}) | {f.get('any', '')} |" for k, f in fields.items()], '']
            if e.get('needsMatch'):
                out += ['A trigger on it must name ' + ' and '.join(f'`{k}`' for k in e['needsMatch']) + ' in its filters.', '']
            if e.get('notes'):
                out += [e['notes'], '']
            out += _code(example_event(e)) + ['']
    out += ['### Conditions', '', 'A condition is a tree: All, Any and Not hold others (8 under each, 5 deep). A condition over the '
            "world's facts reads the world as of the event; a fact that is missing (a wolf away) is unknown, and unknown never fires, "
            'not even under Not.', '']
    for c in cat['conditions']:
        marks = [m for m, on in (('v2', c.get('v2')), ("reads the world's facts", c.get('facts')), ('version 1', c.get('v1'))) if on]
        ex = example_condition(c)
        out += [f"{_anchor('condition', c['id'])}**`{c['id']}`**: {c.get('title', '')}" + (f" ({', '.join(marks)})" if marks else ''), '',
                c.get('help', ''), '']
        if c.get('params'):
            out += ['Reads: ' + _sentence(c.get('sentence', ''), c['params'], ex), '', *_params(c['params']), '']
        out += _code(ex) + ['']
    agroups = {g['id']: g['label'] for g in cat['actionGroups']}
    out += ['### Actions', '', 'A trigger runs its actions in order (12 in version 1; 24 counting nested ones in version 2). '
            'World effects are the action `effect`: they are listed under Effects below.', '']
    for gid, label in agroups.items():
        acts = [a for a in cat['actions'] if a.get('group') == gid]
        if not acts:
            continue
        out += [f'#### {label}', ''] + ([GROUP_NOTES[gid], ''] if gid in GROUP_NOTES else [])
        for a in acts:
            marks = [m for m, on in (('v2', a.get('v2')), ('version 1', a.get('v1')), ('must be last', a.get('last'))) if on]
            ex = example_action(a)
            out += [f"{_anchor('action', a['id'])}**`{a.get('do', a['id'])}`**" + (f" ({a['id']})" if a.get('do') and a['do'] != a['id'] else '')
                    + f": {a.get('title', '')}" + (f" ({', '.join(marks)})" if marks else ''), '', a.get('help', ''), '']
            if a.get('params'):
                out += ['Reads: ' + _sentence(a.get('sentence', ''), a['params'], ex), '', *_params(a['params']), '']
            out += _code(ex) + ['']
    rest = [a for a in cat['actions'] if a.get('group') not in agroups]
    for a in rest:
        out += [f"{_anchor('action', a['id'])}**`{a.get('do', a['id'])}`**: {a.get('title', '')}", '', a.get('help', ''), ''] + _code(example_action(a)) + ['']
    out += ['### Effects', '', f"{len(cat['effects'])} world effects, each one action `{{\"do\": \"effect\", \"effect\": {{...}}}}` (or, for a "
            "phase, one of what it sends as it opens). Each becomes an order the game applies and may refuse, with a receipt, except "
            "the story's own. On any effect the game applies, `ifRefused` says what the story does when it is refused: `pause` (the "
            'default), `skip`, or `when here` (an effect to one wolf, held until they are next in the world, `waitDays` 1 to 90).', '']
    undone = set(rules.UNDONE_AT_END)
    for gid, label in agroups.items():
        effs = [e for e in cat['effects'] if e.get('group') == gid]
        if not effs:
            continue
        out += [f'#### {label}', '']
        for e in effs:
            marks = [m for m, on in (('v2', e.get('v2')), ("the story's own", e.get('story')), ('acts on wolves', e.get('actsOnWolves')),
                                     ('needs a builder', e.get('needsBuilder')), ('undone at the end', e['id'] in undone),
                                     ('phase entry only', e.get('entryOnly'))) if on]
            ex = example_effect(e)
            out += [f"{_anchor('effect', e['id'])}**`{e['id']}`**: {e.get('title', '')}" + (f" ({', '.join(marks)})" if marks else ''), '',
                    e.get('help', ''), '']
            if e.get('params'):
                out += ['Reads: ' + _sentence(e.get('sentence', ''), e['params'], ex), '', *_params(e['params']), '']
            out += _code({'do': 'effect', 'effect': ex}) + ['']
    others = [e for e in cat['effects'] if e.get('group') not in agroups]
    for e in others:
        out += [f"{_anchor('effect', e['id'])}**`{e['id']}`**: {e.get('title', '')}", '', e.get('help', ''), ''] + _code({'do': 'effect', 'effect': example_effect(e)}) + ['']
    out += [END]
    return '\n'.join(out).replace('\n\n\n', '\n\n') + '\n'


def current(text):
    a, b = text.find(BEGIN), text.find(END)
    return text[a:b + len(END)] + '\n' if a >= 0 and b > a else None


def rewrite(text):
    a, b = text.find(BEGIN), text.find(END)
    ref = reference()
    if a < 0 or b < a:
        return text.rstrip('\n') + '\n\n' + ref
    return text[:a] + ref + text[b + len(END) + 1:]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--write', action='store_true')
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    if not (args.check or args.write):
        sys.stdout.write(reference())
        return 0
    text = GUIDE.read_text()
    if args.check:
        ok = current(text) == reference()
        print('the guide\'s reference is the catalogue\'s' if ok else 'the guide\'s reference is out of date: python3 tools/story_guide.py --write')
        return 0 if ok else 1
    if args.write:
        GUIDE.write_text(rewrite(text))
        print('wrote', GUIDE.relative_to(ROOT))
    return 0


if __name__ == '__main__':
    sys.exit(main())
