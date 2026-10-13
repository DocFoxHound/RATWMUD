"""Reading the talk library's files (Docs/Design/62-talk.md) for the tools: rules and scenes in Data/Talk/**/*.talk,
grammars in Data/Talk/grammar/*.grammar. Only reads and counts; the game's own loader (Core/RatwTalk.cpp, and
`talk_check --validate`) is the judge of whether a file is valid."""
from __future__ import annotations

import re
from pathlib import Path

TALK = Path(__file__).resolve().parent.parent / 'Data' / 'Talk'

# The exchange library's blanks (doc 28's, as the small model writes them) and the facts they are in the talk library.
LIBRARY_BLANKS = {'teller': 'a.name', 'listener': 'b.name', 'subject': 'topic.subject', 'claim': 'topic.claim',
                  'news': 'topic.news', 'day': 'topic.remark'}


def read(root: Path = TALK) -> dict:
    """{'groups': {name: set}, 'entries': [...], 'grammars': {name: {symbol: [alternatives...]}}}. An entry is
    {'id', 'file', 'scene': bool, 'concepts': set, 'when': [token...], 'regions': set or None, 'priority', 'weight',
    'lines': [(who, optional, tags, [alternatives])]} (a rule's `say` is one line for a)."""
    groups, entries, grammars = {}, [], {}
    for f in sorted(root.rglob('*.grammar')):
        current = None
        for line in f.read_text().splitlines():
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith('grammar '):
                current = grammars.setdefault(line[8:].strip(), {})
                continue
            m = re.match(r'^([a-z0-9_]+)(\.broad)?!{0,2}(\[[^\]]*\])?\s*\+?=\s*(.*)$', line)
            if m and current is not None:
                current.setdefault(m.group(1) + (m.group(2) or ''), []).extend(a.strip() for a in m.group(4).split('|') if a.strip())
    files = sorted(root.rglob('*.talk'), key=lambda p: (p.name != 'groups.talk', str(p)))
    for f in files:
        entry = None
        for line in f.read_text().splitlines():
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith('group '):
                name, members = line[6:].split('=', 1)
                out = set()
                for m in members.split():
                    out |= groups.get(m, {m})
                groups[name.strip()] = out
            elif line.startswith(('scene ', 'rule ')):
                scene = line.startswith('scene ')
                entry = {'id': line.split(None, 1)[1].strip(), 'file': str(f.relative_to(root)), 'scene': scene,
                         'concepts': set(), 'when': [], 'regions': None, 'priority': 0, 'weight': 1.0, 'lines': []}
                entries.append(entry)
            elif entry is None:
                continue
            elif line.startswith('when '):
                for token in line[5:].split():
                    key, _, values = token.partition('=')
                    values = set(values.split('|'))
                    if key == 'concept':
                        entry['concepts'] |= values
                    elif key == 'topic':
                        entry['concepts'] |= {'talk.' + v for v in values}
                    else:
                        if key in ('here.region', 'region'):
                            entry['regions'] = set().union(*(groups.get(v, {v}) for v in values))
                        entry['when'].append(token)
            elif line.startswith('priority '):
                entry['priority'] = int(line.split()[1])
            elif line.startswith('weight '):
                entry['weight'] = float(line.split()[1])
            elif line.startswith('say'):
                entry['lines'].append(('a', False, '', [t.strip() for t in line[3:].split('|') if t.strip()]))
            elif re.match(r'^[ab]\??(\[[^\]]*\])?\s*:', line):
                head, _, text = line.partition(':')
                tags = re.findall(r'\[([^\]]*)\]', head)
                entry['lines'].append((head[0], '?' in head, tags[0] if tags else '',
                                       [t.strip() for t in text.split('|') if t.strip()]))
    return {'groups': groups, 'entries': entries, 'grammars': grammars}


def library_exchanges(path: Path = TALK / 'library.talk') -> list[dict]:
    """The exchange library's scenes as doc 28 wrote them: {'id', 'kind', 'band', 'lines': [[who, text]]}, with the
    facts written as {blanks} again."""
    if not path.is_file():
        return []
    back = {v: k for k, v in LIBRARY_BLANKS.items()}
    out = []
    for e in read(path.parent)['entries']:
        if e['file'] != path.name:
            continue
        band = 'any'
        for token in e['when']:
            if token == 'pair.band!=friends|rivals':
                band = 'acquaintances'
            elif token.startswith('pair.band='):
                band = token.split('=', 1)[1]
        lines = [[who, re.sub(r'#([a-z.]+)#', lambda m: '{' + back.get(m.group(1), m.group(1)) + '}', alts[0])]
                 for who, _, _, alts in e['lines']]
        out.append({'id': e['id'], 'kind': next(iter(e['concepts'])).split('.', 1)[1], 'band': band, 'lines': lines})
    return out


def library_scene(id: str, kind: str, band: str, lines: list, note: str = '') -> str:
    """One exchange (doc 28's form, with {blanks}) as a scene for library.talk."""
    when = f'topic={kind}' + {'any': '', 'friends': ' pair.band=friends', 'rivals': ' pair.band=rivals',
                              'acquaintances': ' pair.band!=friends|rivals'}[band]
    out = ['', f'scene {id}'] + ([f'# {note}'] if note else []) + ['when ' + when, 'priority -1']
    for who, text in lines:
        out.append(f'{who} : ' + re.sub(r'\{(\w+)\}', lambda m: '#' + LIBRARY_BLANKS[m.group(1)] + '#', text))
    return '\n'.join(out) + '\n'
