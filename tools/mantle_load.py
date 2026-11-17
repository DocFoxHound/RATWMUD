#!/usr/bin/env python3
"""How agents' drafts reach a dev world's rows (Docs/Design/84-living-conversations.md §17.4; Phase 9): a committed,
re-runnable tool, so a world's mantle rows are only ever written by one that checks, backs up and records, never by
hand-made SQL. Never for a live world (PROD's mantles come by import, §17.9): it refuses one.

  python3 tools/mantle_load.py WORLD FILE... --batch NAME [--bind] [--dry-run] [--prune] [--take-theirs IDS] [--take-mine IDS]
                                                       [--no-export]
      1. Every file read and checked (G1, the game's loader, with the world's dialects; G2, mantle_check with the world's
         names, towns and other mantles): any problem refuses the whole batch, listed by file, mantle and item.
      2. Revisions compared: a file's `revision` is the one it was exported from (none: a new mantle). A mantle whose rows
         have moved on since (an edit in Atlas or the DM) is refused, unless --take-theirs (keep the rows, skip the file)
         or --take-mine (overwrite; the history keeps the other) names it (IDS: comma-separated, or "all").
      3. Every row the batch will change (mantles, items, the voice rows it binds) dumped to
         Saved/Backups/mantles-<world>-<batch>-<stamp>.jsonl (`restore` puts it back).
      4. Written in one transaction as Atlas's role (ratw_editor), the history's source `mantle_load:<batch>`: new mantles
         and items inserted, changed items updated, unchanged ones left alone (their revision doesn't move), items absent
         from a file deleted only with --prune. written_by is claude-<batch>. A new item with no @name takes the next
         free number of its kind, never a key the rows have.
      5. --bind: each mantle to the resident it was written for (bound_how authored, bound_day the world's day), unless
         they are dead or wear another.
      6. The world's server told: one voice.sync.
      7. The touched mantles exported over their files (unless --no-export), so the batch's commit carries the keys and
         revisions it was given.
      Loading the same files twice changes nothing.
  python3 tools/mantle_load.py check WORLD
      Placed residents with no mantle, mantles bound twice, written_for ids that aren't NPCs, ready mantles that fail
      today's checks (a dialect or a resident changed), and rows that differ from the committed export.
  python3 tools/mantle_load.py backup WORLD [--out FILE]
      Every mantle, item and history row and every binding to Saved/Backups/ (before the rollback of 0066, or any
      repair).
  python3 tools/mantle_load.py clear-own WORLD NPC... --batch NAME
      Phase 11 (§9.7): residents' own words, lines and persona (live.npc_voice.grammar, rules, persona: the luna tables)
      emptied once mantles carry their voices, after a dump to Saved/Backups/ (`restore` puts them back); voice.sync each.
  Loading is G2 --strict (Phase 11 on: the tier's size and the dialect density refused); --lenient warns them only.
  python3 tools/mantle_load.py restore FILE [--world WORLD]
      A backup's mantles, items and bindings put back (as they were; the history notes the restore).
"""
from __future__ import annotations

import argparse
import datetime
import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import mantle_check  # noqa: E402
import mantle_export  # noqa: E402
import mantle_store as S  # noqa: E402
import mantle_text as T  # noqa: E402

ROOT = HERE.parent
BACKUPS = ROOT / 'Saved' / 'Backups'


def _world_db():
    import world_db
    return world_db


def connect(world: str, role: str = 'editor'):
    W = _world_db()
    key = W.resolve(world)
    if W.kind(key) != 'dev':
        raise SystemExit(f'{key} is a {W.kind(key)} world: mantle_load writes dev worlds only (a live world takes its '
                         f'mantles by import, doc 84 §17.9)')
    return key, W.connect(key, role)


def ids(text: str | None) -> set:
    return set() if not text else {'all'} if text == 'all' else set(text.split(','))


def jsonable(v):
    if isinstance(v, (datetime.datetime, datetime.date)):
        return v.isoformat()
    return v


def dump(conn, world_id: str, mantle_ids: set | None, npcs: set | None, path: Path) -> int:
    """Rows to a .jsonl backup: mantles, items, history and bindings (all, or those named). The rows written."""
    path.parent.mkdir(parents=True, exist_ok=True)
    n = 0
    with path.open('w') as f:
        def out(table, cols, rows_):
            nonlocal n
            for r in rows_:
                f.write(json.dumps({'table': table, 'row': {c: jsonable(v) for c, v in zip(cols, r)}}, sort_keys=True) + '\n')
                n += 1
        mcols = ('world_id', 'id', 'dialect', 'tier', 'state', 'format', 'accents', 'title', 'written_for', 'fits', 'prefers',
                 'traits', 'quirk', 'card', 'persona', 'revision', 'written_by', 'created_at', 'updated_at')
        lcols = ('world_id', 'mantle_id', 'key', 'section', 'kind', 'move', 'position', 'body', 'revision', 'updated_at')
        hcols = ('seq', 'world_id', 'mantle_id', 'key', 'revision', 'action', 'before', 'after', 'source', 'account', 'note', 'at')
        vcols = ('world_id', 'npc_id', 'mantle_id', 'bound_day', 'bound_how', 'released_day')
        where = ' AND id = ANY(%s)' if mantle_ids is not None else ''
        args = (world_id, list(mantle_ids)) if mantle_ids is not None else (world_id,)
        out('live.mantles', mcols, conn.execute(f'SELECT {", ".join(mcols)} FROM live.mantles WHERE world_id = %s{where} ORDER BY id', args))
        where = ' AND mantle_id = ANY(%s)' if mantle_ids is not None else ''
        out('live.mantle_lines', lcols, conn.execute(f'SELECT {", ".join(lcols)} FROM live.mantle_lines WHERE world_id = %s{where} '
                                                     'ORDER BY mantle_id, key', args))
        if mantle_ids is None:
            out('live.mantle_history', hcols, conn.execute(f'SELECT {", ".join(hcols)} FROM live.mantle_history WHERE world_id = %s '
                                                           'ORDER BY seq', (world_id,)))
        vwhere = (' AND (npc_id = ANY(%s) OR mantle_id = ANY(%s))' if npcs is not None else ' AND mantle_id IS NOT NULL')
        vargs = (world_id, list(npcs or ()), list(mantle_ids or ())) if npcs is not None else (world_id,)
        out('live.npc_voice', vcols, conn.execute(f'SELECT {", ".join(vcols)} FROM live.npc_voice WHERE world_id = %s{vwhere} '
                                                  'ORDER BY npc_id', vargs))
    return n


def backup_path(world: str, batch: str) -> Path:
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    return BACKUPS / f'mantles-{world}-{batch}-{stamp}.jsonl'


def read_files(paths: list[str], reserved_of) -> tuple[list[tuple[Path, T.Mantle]], list[str]]:
    out, problems = [], []
    for p in mantle_check.files(paths):
        text = p.read_text()
        try:
            first = T.read(text, str(p))
            reserved = reserved_of(first.id)
            m = T.read(text, str(p), reserved=reserved) if reserved else first
            out.append((p, m))
        except T.MantleError as e:
            problems += e.problems
    seen = {}
    for p, m in out:
        if m.id in seen:
            problems.append(f'{p}: {m.id} is in {seen[m.id]} too')
        seen[m.id] = p
    return out, problems


def load(world: str, paths: list[str], batch: str, bind: bool = False, dry_run: bool = False, prune: bool = False,
         take_theirs: set = frozenset(), take_mine: set = frozenset(), export: bool = True, account: str = '',
         echo=print, strict: bool = True) -> dict:
    """The batch (steps 1-7 above). What was done: {loaded: {id: changes}, skipped, bound, backup, problems}."""
    key, conn = connect(world)
    with conn:
        return load_into(conn, key, paths, batch, bind, dry_run, prune, take_theirs, take_mine, export, account, strict)


def load_into(conn, key: str, paths: list[str], batch: str, bind: bool = False, dry_run: bool = False, prune: bool = False,
              take_theirs: set = frozenset(), take_mine: set = frozenset(), export: bool = True, account: str = '',
              strict: bool = True) -> dict:
    """load() on an open connection (Atlas's role) to the world `key`."""
    report = {'loaded': {}, 'skipped': [], 'bound': [], 'backup': None, 'problems': []}
    world_id = S.world_id_of(conn)

    def reserved_of(mid):
        return {r[0] for r in conn.execute('SELECT key FROM live.mantle_lines WHERE world_id = %s AND mantle_id = %s',
                                           (world_id, mid)).fetchall()}

    found, problems = read_files(paths, reserved_of)
    if problems:
        report['problems'] = problems
        return report
    mantles = [m for _, m in found]
    # 2. Revisions.
    go = []
    for p, m in found:
        current = conn.execute('SELECT revision FROM live.mantles WHERE world_id = %s AND id = %s', (world_id, m.id)).fetchone()
        theirs = 'all' in take_theirs or m.id in take_theirs
        mine = 'all' in take_mine or m.id in take_mine
        if current and m.revision and current[0] != m.revision and not mine:
            if theirs:
                report['skipped'].append(m.id)
                continue
            problems.append(f'{p}: {m.id} has moved on to revision {current[0]} {S.last_change(conn, world_id, m.id)} '
                            f'(the file is revision {m.revision}): --take-theirs {m.id} or --take-mine {m.id}')
        elif current and not m.revision and not mine:
            problems.append(f'{p}: {m.id} is in the rows already (revision {current[0]}) and the file names no revision: '
                            f'export it first, or --take-mine {m.id}')
        go.append((p, m, current[0] if current else None))
    # 1. The checks, on the batch as a whole.
    if not problems:
        for mid, found_ in S.check([m for _, m, _ in go], conn, world_id, strict=strict).items():
            problems += found_
    if problems:
        report['problems'] = problems
        return report
    if dry_run:
        for _, m, current in go:
            report['loaded'][m.id] = {'revision': current, 'dry_run': True, 'items': len(m.items)}
        return report
    # 3. The backup.
    npcs = {m.written_for for _, m, _ in go if bind and m.written_for}
    path = backup_path(key, batch)
    dump(conn, world_id, {m.id for _, m, _ in go}, npcs, path)
    report['backup'] = str(path)
    # 4-5. One transaction.
    with conn.transaction():
        for p, m, current in go:
            m.written_by = f'claude-{batch}'[:80]
            report['loaded'][m.id] = S.save(conn, world_id, m, base_revision=current, source=f'mantle_load:{batch}',
                                            account=account or os.environ.get('USER', ''), note=f'{p.name}',
                                            prune=prune, take_mine=True, checked=True)
            if bind and m.written_for:
                why = S.bind(conn, world_id, m.id, m.written_for)
                if why:
                    report['problems'].append(f'{m.id} not bound: {why}')
                else:
                    report['bound'].append(f'{m.id} -> {m.written_for}')
        # 6. The server.
        S.queue(conn, 'voice.sync', '', f'mantle_load:{batch}')
    # 7. The files written back from the rows.
    if export:
        for p, m, _ in go:
            rows = mantle_check.world_mantles(conn, world_id, m.id)
            if rows:
                p.write_text(mantle_export.text_of(rows[0], key))
    return report


def check(world: str) -> list[str]:
    """What's out of joint in a world's mantles and bindings (`check`)."""
    key, conn = connect(world)
    with conn:
        return check_conn(conn, key)


def check_conn(conn, key: str, export_dir: Path | None = None) -> list[str]:
    out = []
    world_id = S.world_id_of(conn)
    mantles = mantle_check.world_mantles(conn, world_id)
    npcs = {r[0] for r in conn.execute('SELECT id FROM live.npcs WHERE world_id = %s', (world_id,)).fetchall()}
    placed = {r[0] for r in conn.execute("SELECT id FROM live.npcs WHERE world_id = %s AND home_area IS NOT NULL", (world_id,)).fetchall()}
    dead = {r[0] for r in conn.execute("SELECT key FROM game.npcs WHERE world_id = %s AND data->>'dead' = 'true'", (world_id,)).fetchall()}
    bound = conn.execute('SELECT npc_id, mantle_id FROM live.npc_voice WHERE world_id = %s AND mantle_id IS NOT NULL AND '
                         'released_day IS NULL', (world_id,)).fetchall()
    wearers = {n for n, _ in bound}
    out.append(f'{len(placed - dead - wearers)} placed residents wear no mantle (of {len(placed - dead)}); '
               f'{len(mantles)} mantles, {len(bound)} worn')
    seen = {}
    for npc, mid in bound:
        if mid in seen:
            out.append(f'{mid} is bound to {seen[mid]} and {npc}')
        seen[mid] = npc
        if npc in dead:
            out.append(f'{mid} is bound to {npc}, who is dead and not released')
    for m in mantles:
        if m.written_for and m.written_for not in npcs:
            out.append(f'{m.id} is written for {m.written_for}, who is not an NPC of the world')
    ready = [m for m in mantles if m.state == 'ready']
    if ready:
        for mid, problems in S.check(ready, conn, world_id).items():
            out += [f'{mid}: {p}' for p in problems]
    out += mantle_export.differences(conn, key, export_dir or mantle_export.OUT)
    return out


OWN = ('world_id', 'npc_id', 'grammar', 'rules', 'persona', 'written_by')


def clear_own(world: str, npcs: list[str], batch: str) -> tuple[str, list[str]]:
    """Phase 11 (§9.7, §17.7.2): residents' own words, lines and persona in live.npc_voice (the ten luna tables) emptied
    once mantles carry their voices, after a dump to Saved/Backups/ that `restore` puts back. The voice rows' other
    columns (origin, accent, quarter, traits, quirk, the binding) are untouched; the server is told (voice.sync <npc>)."""
    key, conn = connect(world)
    with conn:
        return clear_own_conn(conn, key, npcs, batch)


def clear_own_conn(conn, key: str, npcs: list[str], batch: str) -> tuple[str, list[str]]:
    """clear_own on an open connection (Atlas's role) to the world `key`."""
    done = []
    if True:
        world_id = S.world_id_of(conn)
        path = backup_path(key, batch)
        path.parent.mkdir(parents=True, exist_ok=True)
        rows = conn.execute(f'SELECT {", ".join(OWN)} FROM live.npc_voice WHERE world_id = %s AND npc_id = ANY(%s) ORDER BY npc_id',
                            (world_id, npcs)).fetchall()
        with path.open('w') as f:
            for r in rows:
                f.write(json.dumps({'table': 'live.npc_voice.own', 'row': dict(zip(OWN, r))}, sort_keys=True) + '\n')
        with conn.transaction():
            for r in rows:
                row = dict(zip(OWN, r))
                if row['grammar'] or row['rules'] or row['persona']:
                    conn.execute("UPDATE live.npc_voice SET grammar = '', rules = '', persona = '', updated_at = now() WHERE "
                                 'world_id = %s AND npc_id = %s', (world_id, row['npc_id']))
                    S.queue(conn, 'voice.sync', row['npc_id'], f'mantle_load:{batch}')
                    done.append(row['npc_id'])
    return str(path), done


def restore(path: str, world: str | None = None) -> int:
    lines = [json.loads(l) for l in Path(path).read_text().splitlines() if l.strip()]
    worlds = {l['row']['world_id'] for l in lines}
    if not world:
        name = Path(path).name
        world = name.split('-')[1] if name.startswith('mantles-') else None
    if not world:
        raise SystemExit('restore: name the world (--world)')
    key, conn = connect(world)
    with conn:
        n = restore_conn(conn, path)
    print(f'{n} rows put back into {key} from {path} ({", ".join(sorted(worlds))})')
    return 0


def restore_conn(conn, path: str) -> int:
    """A backup's mantles, items and bindings put back, in one transaction; the rows put back."""
    lines = [json.loads(l) for l in Path(path).read_text().splitlines() if l.strip()]
    n = 0
    with conn.transaction():
        conn.execute("SELECT set_config('ratw.edit_source', 'restore', true), set_config('ratw.edit_note', %s, true)", (Path(path).name,))
        for l in lines:
            t, r = l['table'], l['row']
            if t == 'live.mantles':
                cols = [c for c in r if c not in ('created_at', 'updated_at')]
                vals = [json.dumps(r[c]) if c in ('traits', 'card') else r[c] for c in cols]
                conn.execute(f'INSERT INTO live.mantles ({", ".join(cols)}) VALUES ({", ".join(["%s"] * len(cols))}) ON CONFLICT '
                             f'(world_id, id) DO UPDATE SET {", ".join(f"{c} = EXCLUDED.{c}" for c in cols if c not in ("world_id", "id"))}',
                             vals)
            elif t == 'live.mantle_lines':
                cols = [c for c in r if c != 'updated_at']
                conn.execute(f'INSERT INTO live.mantle_lines ({", ".join(cols)}) VALUES ({", ".join(["%s"] * len(cols))}) ON CONFLICT '
                             f'(world_id, mantle_id, key) DO UPDATE SET {", ".join(f"{c} = EXCLUDED.{c}" for c in cols if c not in ("world_id", "mantle_id", "key"))}',
                             [r[c] for c in cols])
            elif t == 'live.npc_voice.own':
                conn.execute('UPDATE live.npc_voice SET grammar = %s, rules = %s, persona = %s WHERE world_id = %s AND npc_id = %s',
                             (r['grammar'], r['rules'], r['persona'], r['world_id'], r['npc_id']))
            elif t == 'live.npc_voice':
                conn.execute('UPDATE live.npc_voice SET mantle_id = %s, bound_day = %s, bound_how = %s, released_day = %s WHERE '
                             'world_id = %s AND npc_id = %s', (r['mantle_id'], r['bound_day'], r['bound_how'], r['released_day'],
                                                               r['world_id'], r['npc_id']))
            else:
                continue                            # (The history stays where it is: the restore is its newest entry.)
            n += 1
    return n


def main() -> int:
    argv = sys.argv[1:]
    if argv[:1] == ['check']:
        ap = argparse.ArgumentParser(prog='mantle_load.py check')
        ap.add_argument('world')
        a = ap.parse_args(argv[1:])
        found = check(a.world)
        for f in found:
            print(f)
        return 1 if len(found) > 1 else 0
    if argv[:1] == ['backup']:
        ap = argparse.ArgumentParser(prog='mantle_load.py backup')
        ap.add_argument('world')
        ap.add_argument('--out')
        a = ap.parse_args(argv[1:])
        key, conn = connect(a.world)
        with conn:
            path = Path(a.out) if a.out else backup_path(key, 'backup')
            n = dump(conn, S.world_id_of(conn), None, None, path)
        print(f'{n} rows of {key}\'s mantles, items, history and bindings in {path}')
        return 0
    if argv[:1] == ['clear-own']:
        ap = argparse.ArgumentParser(prog='mantle_load.py clear-own')
        ap.add_argument('world')
        ap.add_argument('npcs', nargs='+')
        ap.add_argument('--batch', required=True)
        a = ap.parse_args(argv[1:])
        path, done = clear_own(a.world, a.npcs, a.batch)
        print(f'backup: {path}')
        print(f'emptied the own words, lines and persona of {len(done)}: {", ".join(done)}')
        return 0
    if argv[:1] == ['restore']:
        ap = argparse.ArgumentParser(prog='mantle_load.py restore')
        ap.add_argument('file')
        ap.add_argument('--world')
        a = ap.parse_args(argv[1:])
        return restore(a.file, a.world)
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('world')
    ap.add_argument('files', nargs='+')
    ap.add_argument('--batch', required=True)
    ap.add_argument('--bind', action='store_true')
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--prune', action='store_true')
    ap.add_argument('--take-theirs')
    ap.add_argument('--take-mine')
    ap.add_argument('--no-export', action='store_true')
    ap.add_argument('--lenient', action='store_true', help="G2 without --strict (the tier's size and the density warned)")
    a = ap.parse_args(argv)
    report = load(a.world, a.files, a.batch, a.bind, a.dry_run, a.prune, ids(a.take_theirs), ids(a.take_mine), not a.no_export,
                  strict=not a.lenient)
    for p in report['problems']:
        print(p)
    for mid, done in sorted(report['loaded'].items()):
        print(f'{mid}: {done}')
    for b in report['bound']:
        print(f'bound {b}')
    if report['skipped']:
        print('kept the rows of ' + ', '.join(report['skipped']))
    if report['backup']:
        print(f'backup: {report["backup"]}')
    return 1 if report['problems'] and not report['loaded'] else 0


if __name__ == '__main__':
    sys.exit(main())
