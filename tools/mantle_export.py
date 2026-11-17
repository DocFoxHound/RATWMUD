#!/usr/bin/env python3
"""The export (Docs/Design/84-living-conversations.md §17.5; Phase 9): every mantle of a world written out of its rows as
.mantle text, one file a mantle at <out>/<dialect>/<id>.mantle, in canonical form (the header and card from the columns,
with its revision and state; then its words, then each stage section's items by position, each body as its row holds
it), and index.tsv (id, dialect, tier, state, revision, written_for, written_by). Wearers are not in it (Atlas shows
them). Loading an export and exporting it again gives the same bytes. No server reads it where there is a database: it is
for review (an edit made in Atlas or the DM shows as a git diff, committed at every batch and slice end), for ctest
(G1 and G2 over the whole committed export) and for worlds without a database (--mantles-from).

  python3 tools/mantle_export.py WORLD [--out Data/Talk/mantles-export] [--mantle ID]
  python3 tools/mantle_export.py WORLD --check [--out DIR]      every mantle and item where rows and files differ; exit 1

Reads the world's rows only (as Atlas's role).
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import mantle_check  # noqa: E402
import mantle_text as T  # noqa: E402

ROOT = HERE.parent
OUT = ROOT / 'Data' / 'Talk' / 'mantles-export'
INDEX = ('id', 'dialect', 'tier', 'state', 'revision', 'written_for', 'written_by')


def comment(m: T.Mantle, world: str) -> str:
    return (f'{m.id}{", " + m.title if m.title else ""} (Docs/Design/84). Exported from {world} at revision {m.revision}: '
            f'edit it in Atlas or the DM, or as a draft through tools/mantle_load.py.')


def text_of(m: T.Mantle, world: str) -> str:
    return T.write(m, comment(m, world))


def index_tsv(mantles: list[T.Mantle]) -> str:
    lines = ['\t'.join(INDEX)]
    for m in sorted(mantles, key=lambda m: m.id):
        lines.append('\t'.join(str(getattr(m, k) or '') for k in INDEX))
    return '\n'.join(lines) + '\n'


def path_of(out: Path, m: T.Mantle) -> Path:
    return out / m.dialect / f'{m.id}.mantle'


def export(conn, world: str, out: Path, only: str | None = None) -> list[Path]:
    """The world's mantles written over their files (and index.tsv, when all are). The paths written."""
    world_id = mantle_check.world_id_of(conn)
    mantles = mantle_check.world_mantles(conn, world_id, only)
    written = []
    for m in mantles:
        p = path_of(out, m)
        p.parent.mkdir(parents=True, exist_ok=True)
        # (A mantle whose dialect changed leaves its old file: removed.)
        for old in out.glob(f'*/{m.id}.mantle'):
            if old != p:
                old.unlink()
        p.write_text(text_of(m, world))
        written.append(p)
    if not only:
        out.mkdir(parents=True, exist_ok=True)
        (out / 'index.tsv').write_text(index_tsv(mantles))
    return written


def differences(conn, world: str, out: Path) -> list[str]:
    """Every mantle and item where the rows and the files differ (--check, G6)."""
    world_id = mantle_check.world_id_of(conn)
    found = []
    rows = {m.id: m for m in mantle_check.world_mantles(conn, world_id)}
    files = {}
    for p in sorted(out.rglob('*.mantle')) if out.exists() else []:
        try:
            m = T.read(p.read_text(), str(p))
            files[m.id] = (p, m)
        except T.MantleError as e:
            found.append(f'{p}: can\'t be read: {e.problems[0]}')
    for mid in sorted(set(rows) | set(files)):
        if mid not in files:
            found.append(f'{mid}: in the rows, not in the export')
            continue
        p, m = files[mid]
        if mid not in rows:
            found.append(f'{mid}: in the export ({p}), not in the rows')
            continue
        r = rows[mid]
        if p.read_text() == text_of(r, world):
            continue
        a, b = {i.key: i for i in r.items}, {i.key: i for i in m.items}
        for key in sorted(set(a) | set(b)):
            if key not in b:
                found.append(f'{mid} {key}: in the rows, not the file')
            elif key not in a:
                found.append(f'{mid} {key}: in the file, not the rows')
            elif (a[key].body, a[key].section, a[key].position) != (b[key].body, b[key].section, b[key].position):
                found.append(f'{mid} {key}: differs')
        if r.columns() != m.columns():
            found.append(f'{mid}: its card differs')
        if r.revision != m.revision:
            found.append(f'{mid}: revision {r.revision} in the rows, {m.revision} in the file')
    index = out / 'index.tsv'
    if rows and (not index.exists() or index.read_text() != index_tsv(list(rows.values()))):
        found.append('index.tsv differs from the rows')
    return found


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('world')
    ap.add_argument('--out', default=str(OUT))
    ap.add_argument('--mantle')
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()
    import world_db as W
    key = W.resolve(a.world)
    with W.connect(key, 'editor') as conn:
        if not conn.execute("SELECT to_regclass('live.mantles') IS NOT NULL").fetchone()[0]:
            print(f'{key} has no mantles yet (migration 0066)')
            return 1
        if a.check:
            found = differences(conn, key, Path(a.out))
            for f in found:
                print(f)
            print(f'{len(found)} differences between the rows and {a.out}' if found else f'the rows and {a.out} agree')
            return 1 if found else 0
        written = export(conn, key, Path(a.out), a.mantle)
        print(f'{len(written)} mantles exported to {a.out}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
