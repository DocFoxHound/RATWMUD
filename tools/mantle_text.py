"""The .mantle syntax for the tools (Docs/Design/84-living-conversations.md §7.9, §17; Phase 9): a mantle's text read into
its card and its items, with the keys the game gives them (Core/RatwMantle.cpp's readMantle, word for word in its
rules), and written back in canonical form: what tools/mantle_export.py writes, so loading an export and exporting it
again gives the same bytes. Only reads and writes text; the database is tools/mantle_store.py's.

A canonical item's body carries its own name (`line @2 request ...`, `@2 oath! = ...` for a symbol's second line), so the
game reading the rows (Game::loadMantles assembles the text from them) gives every item the key its row has.
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass, field

HEADER = ('mantle', 'revision', 'state', 'dialect', 'accent', 'accents', 'tier', 'written_for', 'written_by', 'title', 'fits',
          'prefers', 'traits', 'quirk', 'likes', 'dislikes', 'fears', 'hopes', 'topics', 'styles', 'marks', 'persona', 'format')
SECTIONS = ('words', 'any', 'grown', 'young', 'adolescent', 'old')
STAGES = ('any', 'grown', 'young', 'adolescent', 'old')
ITEMS = ('line', 'rule', 'scene', 'story', 'opinion', 'dlg', 'graft')
BLOCKS = ('rule', 'scene', 'story', 'graft')
KINDS = ('symbol', 'opinion', 'line', 'rule', 'scene', 'story', 'dlg', 'graft')
AXES = ('temper', 'talk', 'warmth', 'humour', 'honesty', 'piety', 'drive')
ID = re.compile(r'^[a-z]{2}[0-9]{3,4}$')
NAME = re.compile(r'^[a-z0-9_]+$')
KEY = re.compile(r'^[a-z0-9_.:]{1,120}$')
SYMBOL = re.compile(r'^(?:@([a-z0-9_]+)\s+)?([a-z0-9_]+(?:\.broad)?)(!{0,2})(\[[^\]]*\])?\s*(\+?=)')


class MantleError(ValueError):
    """The text can't be read: the problems, one a line."""

    def __init__(self, problems: list[str]):
        super().__init__('\n'.join(problems))
        self.problems = problems


@dataclass
class Item:
    key: str = ''
    section: str = 'any'
    kind: str = ''
    move: str = ''
    body: str = ''
    name: str = ''
    position: int = 0
    line: int = 0
    prefix: str = ''
    symbol: str = ''
    fixed: bool = False

    def row(self) -> dict:
        return {'key': self.key, 'section': self.section, 'kind': self.kind, 'move': self.move, 'position': self.position,
                'body': self.body}


@dataclass
class Mantle:
    id: str = ''
    revision: int = 0
    state: str = 'draft'
    dialect: str = 'plain'
    accents: list = field(default_factory=lambda: ['light'])
    tier: str = 'standard'
    written_for: str = ''
    written_by: str = ''
    title: str = ''
    fits: str = ''
    prefers: str = ''
    traits: dict = field(default_factory=dict)
    quirk: str = ''
    likes: list = field(default_factory=list)
    dislikes: list = field(default_factory=list)
    fears: list = field(default_factory=list)
    hopes: list = field(default_factory=list)
    topics: list = field(default_factory=list)       # [(topic, weight)]
    styles: dict = field(default_factory=dict)
    marks: list = field(default_factory=list)
    persona: str = ''
    format: int = 1
    items: list = field(default_factory=list)

    # ---- the card as live.mantles holds it
    def card(self) -> dict:
        return {'likes': self.likes, 'dislikes': self.dislikes, 'fears': self.fears, 'hopes': self.hopes,
                'topics': [[t, w] for t, w in self.topics], 'styles': self.styles, 'marks': self.marks}

    def columns(self) -> dict:
        """live.mantles' columns (but world_id, revision and the times)."""
        return {'id': self.id, 'dialect': self.dialect, 'tier': self.tier, 'state': self.state, 'format': self.format,
                'accents': list(self.accents), 'title': self.title, 'written_for': self.written_for or None, 'fits': self.fits,
                'prefers': self.prefers, 'traits': self.traits, 'quirk': self.quirk, 'card': self.card(), 'persona': self.persona,
                'written_by': self.written_by}

    @staticmethod
    def from_columns(row: dict, items: list[dict]) -> 'Mantle':
        card = row.get('card') or {}
        m = Mantle(id=row['id'], revision=int(row.get('revision') or 0), state=row['state'], dialect=row['dialect'],
                   accents=list(row['accents']), tier=row['tier'], written_for=row.get('written_for') or '',
                   written_by=row.get('written_by') or '', title=row.get('title') or '', fits=row.get('fits') or '',
                   prefers=row.get('prefers') or '', traits=dict(row.get('traits') or {}), quirk=row.get('quirk') or '',
                   likes=list(card.get('likes', [])), dislikes=list(card.get('dislikes', [])), fears=list(card.get('fears', [])),
                   hopes=list(card.get('hopes', [])), topics=[(t, float(w)) for t, w in card.get('topics', [])],
                   styles=dict(card.get('styles', {})), marks=list(card.get('marks', [])), persona=row.get('persona') or '',
                   format=int(row.get('format') or 1))
        for r in items:
            m.items.append(Item(key=r['key'], section=r['section'], kind=r['kind'], move=r.get('move') or '', body=r['body'],
                                position=int(r['position'])))
        return m


def _first(t: str) -> str:
    return t.split(None, 1)[0] if t.strip() else ''


def _at_name(rest: str) -> tuple[str, str, bool]:
    """'@name rest' -> (name, rest, ok)."""
    rest = rest.strip()
    if not rest.startswith('@'):
        return '', rest, True
    w = _first(rest)
    return w[1:], rest[len(w):].strip(), bool(NAME.match(w[1:]))


def split_item(after: str) -> tuple[str, str, str, str, bool, bool]:
    """An item's line after its keyword: [@name] head [criteria] : words -> (name, head, criteria, text, colon, ok)."""
    name, rest, ok = _at_name(after)
    if not ok:
        return '', '', '', '', False, False
    at = 0
    while at < len(rest) and rest[at] not in '[:':
        at += 1
    head = rest[:at].strip()
    crit, text, colon = '', '', False
    if at < len(rest) and rest[at] == '[':
        end = rest.find(']', at)
        if end < 0:
            return name, head, '', '', False, False
        crit = rest[at + 1:end].strip()
        at = end + 1
        while at < len(rest) and rest[at] == ' ':
            at += 1
    if at < len(rest) and rest[at] == ':':
        colon, text = True, rest[at + 1:].strip()
    elif at < len(rest):
        return name, head, crit, '', False, False
    return name, head, crit, text, colon, True


def read(text: str, where: str = 'mantle', reserved: set | None = None) -> Mantle:
    """The text read, its items keyed as the game keys them (a named item by its name, the rest the next free number;
    `reserved`: keys taken already, a mantle's rows in the database, so a new unnamed item never takes one). Raises
    MantleError with the problems."""
    m = Mantle()
    problems: list[str] = []
    section, in_words, saw_id, block = 'any', False, False, None
    number = 0

    def fail(msg: str):
        problems.append(f'{where}:{number}: {msg}')

    def add(kind, sec, body, name, prefix, move, fixed=False) -> Item:
        it = Item(kind=kind, section=sec, body=body, name=name, prefix=prefix, move=move, line=number, fixed=fixed)
        m.items.append(it)
        return it

    for raw in text.split('\n'):
        number += 1
        raw = raw.rstrip()
        t = raw.strip()
        fw = _first(t)
        if block is not None:
            ends = not t or fw in HEADER or (fw in SECTIONS and t == fw) or fw in ITEMS
            if not ends:
                block.body += '\n' + raw
                if block.kind == 'rule' and not block.move and fw == 'does':
                    moves = t[4:].split()
                    if moves:
                        block.move = moves[0]
                continue
            block = None
        if not t or t.startswith('#'):
            continue
        if not saw_id:
            saw_id = True
            if fw != 'mantle':
                fail("a mantle's text starts 'mantle <id>'")
        sym = SYMBOL.match(t) if in_words else None
        if sym:
            it = add('symbol', 'any', t, sym.group(1) or '', 'words.' + sym.group(2), '')
            it.symbol = sym.group(2)
            continue
        if fw in SECTIONS and t == fw:
            in_words = fw == 'words'
            if not in_words:
                section = fw
            continue
        rest = t[len(fw):].strip()
        if fw in HEADER:
            words = rest.split()

            def one():
                if len(words) != 1:
                    fail(f'{fw} is one word')
                    return ''
                return words[0]

            if fw == 'mantle':
                m.id = one()
                if not ID.match(m.id):
                    fail(f"a mantle's id is two letters and three or four digits (cw017), not '{m.id}'")
            elif fw in ('revision', 'format'):
                if not rest.isdigit():
                    fail(f'{fw} is a whole number')
                else:
                    setattr(m, fw, int(rest))
            elif fw in ('state', 'dialect', 'tier', 'quirk', 'written_for', 'written_by'):
                setattr(m, fw, one())
            elif fw in ('accent', 'accents'):
                m.accents = words
            elif fw in ('title', 'fits', 'prefers', 'persona'):
                setattr(m, fw, rest)
            elif fw in ('traits', 'styles'):
                into = {}
                for w in words:
                    k, eq, v = w.partition('=')
                    if not eq or not k or not v:
                        fail(f"{fw} is key=value ...: '{w}'")
                    else:
                        into[k] = v
                setattr(m, fw, into)
            elif fw in ('likes', 'dislikes', 'fears', 'hopes', 'marks'):
                setattr(m, fw, words)
            elif fw == 'topics':
                for i in range(0, len(words), 2):
                    try:
                        w = float(words[i + 1])
                        if not NAME.match(words[i]) or not -1 <= w <= 1:
                            raise ValueError
                        m.topics.append((words[i], w))
                    except (IndexError, ValueError):
                        fail("topics is 'topic weight ...' (weather +.4 lore -.3)")
            continue
        if fw in ITEMS:
            in_words = False
            if fw in BLOCKS:
                named, after, ok = _at_name(rest)
                if not ok:
                    fail('@ names an item: lower-case letters, digits and _')
                    continue
                if fw == 'rule':
                    if after:
                        fail("a rule block is 'rule [@name]', its lines below it (when, does, say...)")
                    block = add('rule', section, t, named, '', '')
                elif fw == 'scene':
                    n = named or after
                    if not NAME.match(n or '-') or (named and after):
                        fail("a scene is 'scene <name>', its lines below it")
                    block = add('scene', section, t, n, f'{section}.scene.{n}', '', True)
                elif fw == 'story':
                    _, head, _, _, colon, ok2 = split_item(after)
                    if not ok2 or colon or len(head.split()) != 1 or not NAME.match(head):
                        fail("a story is 'story <name> [criteria]', its lines below it")
                    block = add('story', section, t, head, 'story.' + head, '', True)
                else:
                    w = after.split()
                    if len(w) != 1 or '.' not in w[0] or not re.match(r'^[a-z0-9_.]+$', w[0]):
                        fail("a graft is 'graft [@name] <tree>.<node>', its choices and nodes below it")
                    at = w[0] if w else ''
                    block = add('graft', section, t, named, 'graft.' + at, at, not named)
                continue
            name, head, crit, words_, colon, ok = split_item(rest)
            if not ok or not colon or not words_:
                shape = {'line': '<move>', 'opinion': '<issue> <side> <strength>'}.get(fw, '<tree>.<node>')
                fail(f"{fw} is '{fw} [@name] {shape} [criteria] : words'")
                continue
            heads = head.split()
            if fw == 'line':
                if not heads or not all(re.match(r'^[a-z0-9_.]+$', h) for h in heads):
                    fail('a line names the move it makes: line [@name] <move> [criteria] : words')
                    continue
                add('line', section, t, name, f'{section}.{heads[0]}', heads[0])
            elif fw == 'opinion':
                if len(heads) != 3 or not NAME.match(heads[0]):
                    fail("an opinion is 'opinion <issue> <for|against|mixed> <0-3> [criteria] : words'")
                    continue
                add('opinion', section, t, name, 'opinion.' + heads[0], '')
            else:
                if len(heads) != 1 or '.' not in heads[0] or not re.match(r'^[a-z0-9_.]+$', heads[0]):
                    fail("a window line is 'dlg [@name] <tree>.<node> [criteria] : words'")
                    continue
                add('dlg', section, t, name, 'dlg.' + heads[0], heads[0])
            continue
        fail(f"expected a header line, a section (words, grown, young, adolescent, old) or an item (line, rule, scene, story, "
             f"opinion, dlg, graft), not '{fw}'" + ('' if in_words else " (a symbol's line belongs under words)"))
    if not saw_id or not m.id:
        problems.append(f"{where}: no 'mantle <id>' line")
    if m.state not in ('draft', 'ready', 'retired'):
        problems.append(f"{where}: state is draft, ready or retired, not '{m.state}'")
    if m.tier not in ('full', 'standard', 'spare'):
        problems.append(f"{where}: tier is full, standard or spare, not '{m.tier}'")
    if not m.accents or len(m.accents) > 3 or any(a not in ('none', 'light', 'broad') for a in m.accents):
        problems.append(f'{where}: accent is one to three of none, light and broad (the first as written)')
    # The keys.
    taken = set(reserved or ())
    own = set()
    for it in m.items:
        if it.kind == 'rule' and not it.prefix:
            it.prefix = f"{it.section}.{it.move or 'rule'}"
        if it.fixed:
            it.key = it.prefix
        elif it.name:
            it.key = f'{it.prefix}.{it.name}'
        else:
            continue
        if it.key in own:
            problems.append(f'{where}:{it.line}: {it.key} is there twice (give one an @name)')
        own.add(it.key)
        taken.add(it.key)
    for it in m.items:
        if it.key:
            continue
        # (A symbol's first line is words.<symbol> whatever the rows hold: that is its name, not a number.)
        if it.kind == 'symbol' and it.prefix not in own:
            it.key = it.prefix
            own.add(it.key)
            taken.add(it.key)
            continue
        n = 2 if it.kind == 'symbol' else 1
        while f'{it.prefix}.{n}' in taken:
            n += 1
        it.key = f'{it.prefix}.{n}'
        taken.add(it.key)
        own.add(it.key)
    at: dict[str, int] = {}
    for it in m.items:
        it.position = at.get(it.section, 0)
        at[it.section] = it.position + 1
        if not KEY.match(it.key):
            problems.append(f'{where}:{it.line}: the key {it.key} is lower-case letters, digits, _ . and :, at most 120')
    if problems:
        raise MantleError(problems)
    return m


def named_body(it: Item) -> str:
    """The item's body carrying its own name, so its key is the same wherever it is read (the canonical form)."""
    lines = it.body.split('\n')
    first = lines[0].strip()
    if it.kind == 'symbol':
        _, rest, _ = _at_name(first)
        base = f'words.{SYMBOL.match(rest).group(2)}' if SYMBOL.match(rest) else ''
        name = it.key[len(base) + 1:] if it.key.startswith(base + '.') else ''
        lines[0] = (f'@{name} ' if name else '') + rest
    elif it.kind in ('line', 'opinion', 'dlg', 'rule', 'graft'):
        kw = _first(first)
        _, rest, _ = _at_name(first[len(kw):])
        prefix = (f'{it.section}.{it.move or "rule"}' if it.kind == 'rule' else
                  f'{it.section}.{it.move}' if it.kind == 'line' else
                  'opinion.' + rest.split()[0] if it.kind == 'opinion' else
                  'dlg.' + rest.split()[0] if it.kind == 'dlg' else 'graft.' + it.move)
        name = it.key[len(prefix) + 1:] if it.key.startswith(prefix + '.') else ''
        lines[0] = kw + (f' @{name}' if name else '') + (f' {rest}' if rest else '')
    elif it.kind == 'scene':
        lines[0] = 'scene ' + it.key.rsplit('.', 1)[-1]
    else:
        lines[0] = first
    return '\n'.join(lines)


def _number(w: float) -> str:
    s = f'{w:+g}'
    return s


def write(m: Mantle, comment: str = '') -> str:
    """The canonical text: a comment, the header in its order, the words, then each stage section's items by position
    (a block followed by a blank line)."""
    out = []
    if comment:
        out += [f'# {line}' if line else '#' for line in comment.split('\n')]
    out.append(f'mantle {m.id}')
    if m.revision:
        out.append(f'revision {m.revision}')
    out += [f'state {m.state}', f'dialect {m.dialect}', 'accent ' + ' '.join(m.accents), f'tier {m.tier}']

    def line(key, value):
        if value:
            out.append(f'{key} {value}')

    line('written_for', m.written_for)
    line('written_by', m.written_by)
    line('title', m.title)
    line('fits', m.fits)
    line('prefers', m.prefers)
    traits = sorted(m.traits.items(), key=lambda kv: (AXES.index(kv[0]) if kv[0] in AXES else len(AXES), kv[0]))
    line('traits', ' '.join(f'{k}={v}' for k, v in traits))
    line('quirk', m.quirk)
    line('likes', ' '.join(m.likes))
    line('dislikes', ' '.join(m.dislikes))
    line('fears', ' '.join(m.fears))
    line('hopes', ' '.join(m.hopes))
    line('topics', ' '.join(f'{t} {_number(w)}' for t, w in m.topics))
    line('styles', ' '.join(f'{k}={v}' for k, v in sorted(m.styles.items())))
    line('marks', ' '.join(m.marks))
    line('persona', m.persona)
    if m.format != 1:
        out.append(f'format {m.format}')
    symbols = sorted((i for i in m.items if i.kind == 'symbol'), key=lambda i: i.position)
    if symbols:
        out += ['', 'words'] + [named_body(i) for i in symbols]
    for section in STAGES:
        items = sorted((i for i in m.items if i.kind != 'symbol' and i.section == section), key=lambda i: i.position)
        if not items:
            continue
        out += ['', section]
        for i in items:
            body = named_body(i)
            out.append(body)
            if i.kind in BLOCKS:
                out.append('')
    while out and out[-1] == '':
        out.pop()
    return '\n'.join(out) + '\n'


def canonical(m: Mantle) -> Mantle:
    """The mantle with its symbols first (positions in order) and its bodies named: as it is once written and read back."""
    return read(write(m), m.id)


def card_json(m: Mantle) -> str:
    return json.dumps(m.card(), sort_keys=True)
