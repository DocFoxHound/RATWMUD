"""Every write to a world's mantle rows (Docs/Design/84-living-conversations.md §17.3; Phase 9): one module, so Atlas, the
Dungeon Master and tools/mantle_load.py can't disagree. Before anything is written, the whole mantle as it would be after
the save goes through the game's own loader (G1: `talk_check --mantle`, with the world's dialects over the files') and
tools/mantle_check.py (G2, reading the world's own rows); a `ready` mantle needs a clean G2 and its tier's minimum; a
retired one is refused while worn; a save names the revision it was edited from, and a mantle that has moved on since is
refused with who changed it and when. Then one transaction (as the caller's role: Atlas's ratw_editor), with
ratw.edit_source, ratw.edit_by and ratw.edit_note set for live.mantle_history; and the world's server told (mantle.sync
<id>, or one voice.sync for a batch) through dm.queue_talk_sync. A refused save writes nothing and returns the problems by
item key.

Phase 10: a binding checks its wearer against the mantle's `fits` (fits(), as Core/RatwHandoff.cpp's
Society::fitsMantle: a stage or an age holds if it is the wearer's now or one they will grow to), and retire() refuses a
mantle while it is worn. The Mantles panel's endpoints (P10's table) are still to come on top of save() and retire().
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import mantle_check  # noqa: E402
import mantle_text as T  # noqa: E402

ROOT = HERE.parent
TALK = ROOT / 'Data' / 'Talk'
SECONDS_PER_DAY = 14400.0                            # (Core/RatwCalendar.h: a game day is four real hours.)


class StoreError(Exception):
    """A refused save: the problems, one a line, each naming its mantle and item."""

    def __init__(self, problems: list[str]):
        super().__init__('\n'.join(problems))
        self.problems = problems


def talk_check() -> Path:
    found = os.environ.get('RATW_TALK_CHECK')
    if found:
        return Path(found)
    built = sorted(ROOT.glob('build*/talk_check'), key=lambda p: p.stat().st_mtime, reverse=True)
    if not built:
        raise StoreError(['talk_check is not built (cmake --build build-core --target talk_check): the loader checks every save'])
    return built[0]


def world_id_of(conn) -> str:
    return mantle_check.world_id_of(conn)


def dialects(conn, world_id: str) -> dict:
    return {n: g for n, g in conn.execute('SELECT name, grammar FROM live.dialects WHERE world_id = %s', (world_id,)).fetchall()}


def g1(mantles: list[T.Mantle], world_dialects: dict | None = None) -> dict:
    """The game's own loader on each mantle as its canonical text: {id: [problems]} (empty: loads)."""
    if not mantles:
        return {}
    with tempfile.TemporaryDirectory(prefix='ratw-mantles-') as tmp:
        tmp = Path(tmp)
        paths = {}
        for m in mantles:
            p = tmp / f'{m.id}.mantle'
            p.write_text(T.write(m))
            paths[str(p)] = m.id
        args = [str(talk_check()), '--data', str(TALK)]
        if world_dialects:
            d = tmp / 'dialects'
            d.mkdir()
            for name, grammar in world_dialects.items():
                (d / f'{name}.grammar').write_text(grammar)
            args += ['--dialects', str(d)]
        run = subprocess.run(args + ['--json', '--mantle'] + list(paths), capture_output=True, text=True)
        try:
            found = json.loads(run.stdout)
        except json.JSONDecodeError:
            raise StoreError([f'talk_check failed: {run.stderr.strip() or run.stdout.strip()}'])
        out = {}
        for r in found:
            out[paths.get(r['file'], r['id'])] = r['problems']
        return out


def check(mantles: list[T.Mantle], conn=None, world_id: str | None = None, strict: bool = False) -> dict:
    """G1 and G2 for these mantles (with a world: its dialects, names, towns and other mantles): {id: [problems]}."""
    world = others = None
    world_dialects = None
    if conn is not None:
        world_id = world_id or world_id_of(conn)
        world = mantle_check.world_facts(conn, world_id)
        others = mantle_check.world_mantles(conn, world_id)
        world_dialects = dialects(conn, world_id)
    out = {m.id: [] for m in mantles}
    for mid, problems in g1(mantles, world_dialects).items():
        out.setdefault(mid, []).extend(problems)
    for f in mantle_check.check(mantles, world=world, strict=strict, others=others):
        if not f.warning:
            out.setdefault(f.mantle, []).append(str(f))
    return out


def world_day(conn, world_id: str) -> float | None:
    row = conn.execute("SELECT substring(left(payload, 200) from '\"time\": *([0-9.]+)') FROM game.checkpoints WHERE world_id = %s",
                       (world_id,)).fetchone()
    try:
        return round(float(row[0]) / SECONDS_PER_DAY, 4) if row and row[0] else None
    except (TypeError, ValueError):
        return None


def rows(conn, world_id: str, mantle_id: str) -> tuple[dict | None, dict]:
    """A mantle's live.mantles row (or None) and its items by key."""
    cols = ('id', 'dialect', 'tier', 'state', 'format', 'accents', 'title', 'written_for', 'fits', 'prefers', 'traits', 'quirk',
            'card', 'persona', 'revision', 'written_by', 'updated_at')
    row = conn.execute(f"SELECT {', '.join(cols)} FROM live.mantles WHERE world_id = %s AND id = %s", (world_id, mantle_id)).fetchone()
    items = {r[0]: dict(zip(('key', 'section', 'kind', 'move', 'position', 'body', 'revision'), r)) for r in conn.execute(
        'SELECT key, section, kind, move, position, body, revision FROM live.mantle_lines WHERE world_id = %s AND mantle_id = %s',
        (world_id, mantle_id)).fetchall()}
    return (dict(zip(cols, row)) if row else None), items


def last_change(conn, world_id: str, mantle_id: str) -> str:
    row = conn.execute('SELECT source, account, at FROM live.mantle_history WHERE world_id = %s AND mantle_id = %s ORDER BY seq DESC '
                       'LIMIT 1', (world_id, mantle_id)).fetchone()
    return f'by {row[1] or row[0] or "someone"} at {row[2]:%Y-%m-%d %H:%M}' if row else 'since'


def worn_by(conn, world_id: str, mantle_id: str) -> str | None:
    row = conn.execute('SELECT npc_id FROM live.npc_voice WHERE world_id = %s AND mantle_id = %s AND released_day IS NULL',
                       (world_id, mantle_id)).fetchone()
    return row[0] if row else None


def save(conn, world_id: str, m: T.Mantle, *, base_revision: int | None, source: str, account: str = '', note: str = '',
         prune: bool = False, take_mine: bool = False, checked: bool = False) -> dict:
    """One mantle written, inside the caller's transaction (conn.transaction()): its card, new items inserted, changed
    ones updated (their revision raised by the trigger), unchanged ones left alone, items it no longer has deleted only
    with `prune`. `base_revision`: the revision it was edited from (None: a new mantle, refused if one exists unless
    `take_mine`). Raises StoreError, having written nothing. What changed: {id, revision, inserted, updated, deleted,
    unchanged, kept}."""
    m = T.canonical(m)
    if not checked:
        problems = check([m], conn, world_id).get(m.id, [])
        if problems:
            raise StoreError(problems)
    current, items = rows(conn, world_id, m.id)
    if current is not None and not take_mine:
        if base_revision is None:
            raise StoreError([f'{m.id}: there is a mantle {m.id} already (revision {current["revision"]}): edit that one'])
        if base_revision != current['revision']:
            raise StoreError([f'{m.id}: it has moved on to revision {current["revision"]} {last_change(conn, world_id, m.id)}, '
                              f'from the revision {base_revision} this was edited from: merge, or take one side'])
    if m.state == 'retired' and worn_by(conn, world_id, m.id):
        raise StoreError([f'{m.id}: retired while {worn_by(conn, world_id, m.id)} wears it: release or reassign it first'])
    conn.execute("SELECT set_config('ratw.edit_source', %s, true), set_config('ratw.edit_by', %s, true), "
                 "set_config('ratw.edit_note', %s, true)", (source[:120], account[:120], note[:400]))
    col = m.columns()
    values = (col['dialect'], col['tier'], col['state'], col['format'], col['accents'], col['title'], col['written_for'],
              col['fits'], col['prefers'], json.dumps(col['traits']), col['quirk'], json.dumps(col['card']), col['persona'],
              col['written_by'])
    # Nothing changed (the card, every item, nothing to prune): nothing written, its revision and written_by as they were.
    if current is not None:
        same_card = all(current[k] == v for k, v in (('dialect', col['dialect']), ('tier', col['tier']), ('state', col['state']),
                                                     ('format', col['format']), ('accents', col['accents']), ('title', col['title']),
                                                     ('written_for', col['written_for']), ('fits', col['fits']),
                                                     ('prefers', col['prefers']), ('traits', col['traits']), ('quirk', col['quirk']),
                                                     ('card', col['card']), ('persona', col['persona'])))
        same_items = {k: (v['section'], v['kind'], v['move'], v['position'], v['body']) for k, v in items.items()} == {
            it.key: (it.section, it.kind, it.move, it.position, it.body) for it in m.items} or (
            not prune and all(items.get(it.key) and (items[it.key]['section'], items[it.key]['kind'], items[it.key]['move'],
                                                      items[it.key]['position'], items[it.key]['body']) ==
                              (it.section, it.kind, it.move, it.position, it.body) for it in m.items))
        if same_card and same_items:
            return {'id': m.id, 'inserted': 0, 'updated': 0, 'deleted': 0, 'unchanged': len(m.items),
                    'kept': len(set(items) - {it.key for it in m.items}), 'revision': current['revision']}
    if current is None:
        conn.execute('INSERT INTO live.mantles (world_id, id, dialect, tier, state, format, accents, title, written_for, fits, '
                     'prefers, traits, quirk, card, persona, written_by) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, '
                     '%s, %s, %s, %s)', (world_id, m.id) + values)
    else:
        conn.execute('UPDATE live.mantles SET dialect = %s, tier = %s, state = %s, format = %s, accents = %s, title = %s, '
                     'written_for = %s, fits = %s, prefers = %s, traits = %s, quirk = %s, card = %s, persona = %s, written_by = %s '
                     'WHERE world_id = %s AND id = %s', values + (world_id, m.id))
    done = {'id': m.id, 'inserted': 0, 'updated': 0, 'deleted': 0, 'unchanged': 0, 'kept': 0}
    for it in m.items:
        was = items.get(it.key)
        r = it.row()
        if was is None:
            conn.execute('INSERT INTO live.mantle_lines (world_id, mantle_id, key, section, kind, move, position, body) VALUES '
                         '(%s, %s, %s, %s, %s, %s, %s, %s)', (world_id, m.id, r['key'], r['section'], r['kind'], r['move'],
                                                             r['position'], r['body']))
            done['inserted'] += 1
        elif any(was[k] != r[k] for k in ('section', 'kind', 'move', 'position', 'body')):
            conn.execute('UPDATE live.mantle_lines SET section = %s, kind = %s, move = %s, position = %s, body = %s WHERE '
                         'world_id = %s AND mantle_id = %s AND key = %s', (r['section'], r['kind'], r['move'], r['position'],
                                                                           r['body'], world_id, m.id, r['key']))
            done['updated'] += 1
        else:
            done['unchanged'] += 1
    for key in set(items) - {it.key for it in m.items}:
        if prune:
            conn.execute('DELETE FROM live.mantle_lines WHERE world_id = %s AND mantle_id = %s AND key = %s', (world_id, m.id, key))
            done['deleted'] += 1
        else:
            done['kept'] += 1
    done['revision'] = conn.execute('SELECT revision FROM live.mantles WHERE world_id = %s AND id = %s', (world_id, m.id)).fetchone()[0]
    return done


STAGES = ('young', 'adolescent', 'adult', 'old')


def stage_of(age: int) -> int:
    """Core/RatwAppearance.cpp's lifeStage: young under 13, adolescent to 17, adult to 64, old after."""
    return 0 if age < 13 else 1 if age < 18 else 2 if age < 65 else 3


def fits(text: str, facts: dict, age: int) -> bool:
    """Whether a wolf fits a mantle's hard criteria (§7.9 `fits`), as the game's Society::fitsMantle decides."""
    now = stage_of(age)
    for token in (text or '').split():
        token = token[5:] if token.startswith('self.') else token
        op = next((o for o in ('!=', '>=', '<=', '=', '>', '<') if o in token), None)
        if op is None:
            if token in STAGES:
                name, op, value = 'stage', '=', token
            elif token in ('male', 'female'):
                name, op, value = 'sex', '=', token
            else:
                return False
        else:
            name, value = token.split(op, 1)
        values = value.split('|')
        if name == 'stage':
            reachable = [STAGES[i] for i in range(now, 4)]
            if op == '=' and not any(v in reachable for v in values):
                return False
            if op == '!=' and all(r in values for r in reachable):
                return False
            if op not in ('=', '!='):
                return False
            continue
        if name == 'age':
            try:
                n = float(value)
            except ValueError:
                return False
            ok = {'>=': True, '>': True, '<': age < n, '<=': age <= n, '=': age <= n, '!=': age != n}[op]
            if not ok:
                return False
            continue
        have = facts.get(name)
        listed = have is not None and have in values
        if (op == '=' and not listed) or (op == '!=' and listed) or op not in ('=', '!='):
            return False
    return True


def wearer_facts(conn, world_id: str, npc: str) -> tuple[dict, int] | None:
    """A resident's facts for `fits` (sex, role, job) and age, from live.npcs (the game's age where it has saved one)."""
    row = conn.execute("SELECT age, role, work_label, appearance->>'sex' FROM live.npcs WHERE world_id = %s AND id = %s",
                       (world_id, npc)).fetchone()
    if not row:
        return None
    age = row[0] or 0
    saved = conn.execute("SELECT data->>'age' FROM game.npcs WHERE world_id = %s AND key = %s", (world_id, npc)).fetchone()
    if saved and saved[0] and str(saved[0]).isdigit():
        age = int(saved[0])
    return {'sex': row[3] or '', 'role': row[1] or '', 'job': row[2] or ''}, int(age)


def retire(conn, world_id: str, mantle_id: str, *, source: str, account: str = '', note: str = '') -> dict:
    """A mantle taken out of use (§9.2): refused while worn (release or reassign it first). Inside the caller's
    transaction; the server is told by the caller (queue mantle.sync)."""
    current, _ = rows(conn, world_id, mantle_id)
    if current is None:
        raise StoreError([f'{mantle_id}: no such mantle'])
    wearer = worn_by(conn, world_id, mantle_id)
    if wearer:
        raise StoreError([f'{mantle_id}: retired while {wearer} wears it: release or reassign it first'])
    if current['state'] == 'retired':
        return {'id': mantle_id, 'revision': current['revision'], 'state': 'retired'}
    conn.execute("SELECT set_config('ratw.edit_source', %s, true), set_config('ratw.edit_by', %s, true), "
                 "set_config('ratw.edit_note', %s, true)", (source[:120], account[:120], note[:400]))
    conn.execute("UPDATE live.mantles SET state = 'retired' WHERE world_id = %s AND id = %s", (world_id, mantle_id))
    revision = conn.execute('SELECT revision FROM live.mantles WHERE world_id = %s AND id = %s', (world_id, mantle_id)).fetchone()[0]
    return {'id': mantle_id, 'revision': revision, 'state': 'retired'}


def bind(conn, world_id: str, mantle_id: str, npc: str, how: str = 'authored') -> str:
    """The mantle bound to a resident (their voice row): '' done, else why not (they're dead, they wear another, it's
    worn by someone else, no such resident)."""
    if not conn.execute('SELECT 1 FROM live.npcs WHERE world_id = %s AND id = %s', (world_id, npc)).fetchone():
        return f'{npc} is not an NPC of the world'
    dead = conn.execute("SELECT data->>'dead' FROM game.npcs WHERE world_id = %s AND key = %s", (world_id, npc)).fetchone()
    if dead and dead[0] == 'true':
        return f'{npc} is dead'
    other = worn_by(conn, world_id, mantle_id)
    if other and other != npc:
        return f'{mantle_id} is worn by {other}'
    voice = conn.execute('SELECT mantle_id, released_day FROM live.npc_voice WHERE world_id = %s AND npc_id = %s',
                         (world_id, npc)).fetchone()
    if voice and voice[0] and voice[0] != mantle_id and voice[1] is None:
        return f'{npc} wears {voice[0]}'
    if voice and voice[0] == mantle_id and voice[1] is None:
        return ''
    card = conn.execute('SELECT fits, state FROM live.mantles WHERE world_id = %s AND id = %s', (world_id, mantle_id)).fetchone()
    if card and card[1] == 'retired':
        return f'{mantle_id} is retired'
    wearer = wearer_facts(conn, world_id, npc)
    if card and wearer and not fits(card[0], *wearer):
        return f'{npc} doesn\'t fit {mantle_id} (fits {card[0]})'
    day = world_day(conn, world_id)
    if voice:
        conn.execute('UPDATE live.npc_voice SET mantle_id = %s, bound_how = %s, bound_day = %s, released_day = NULL WHERE '
                     'world_id = %s AND npc_id = %s', (mantle_id, how, day, world_id, npc))
    else:
        conn.execute('INSERT INTO live.npc_voice (world_id, npc_id, mantle_id, bound_how, bound_day) VALUES (%s, %s, %s, %s, %s)',
                     (world_id, npc, mantle_id, how, day))
    return ''


def queue(conn, kind: str, target: str, who: str) -> int | None:
    """mantle.sync <id> or voice.sync for the world's running server (an action nobody takes expires: no error)."""
    try:
        with conn.transaction():                        # (A savepoint: a refusal leaves the caller's transaction whole.)
            return conn.execute('SELECT dm.queue_talk_sync(%s, %s, %s)', (who[:60], kind, target)).fetchone()[0]
    except Exception:                                   # (A world before 0066, or a role without the function.)
        return None
