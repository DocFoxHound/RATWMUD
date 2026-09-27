#!/usr/bin/env python3
"""Compile the world in a database into a build the game server loads.

  python3 tools/world_build.py dev      build DEV's current world (for a local DEV server)
  python3 tools/world_build.py latest   show the newest build in DEV and PROD

PROD builds are made by Push to live (tools/publish.py), one per release.
A build is what the game server loads, produced by the same exporter as
Play/Export, so seams, door arrivals and profession-slot residents are resolved
exactly once, in one place. It is streamed: world.builds holds the manifest
("RATW_WORLD 3", every cell named by an "area" record) and world.build_cells one
row per cell (header, whole file, seams), which the server reads as players and
NPCs come near, so a world of any size loads. Filling profession slots from the
roster is permanent, so the assignments are saved in that database's roster.
"""
from __future__ import annotations

import argparse
import sys

import map_editor
import world_db
import world_store as S

KEEP_CELLS = 3          # Builds whose cell rows are kept (the server loads the newest; a restart mid-push the one before).


def build(conn, created_by='', release=None):
    """Exports the database's world, records roster assignments, stores the build. Returns its ID and file count."""
    with conn.transaction():
        world_id = conn.execute('SELECT id FROM world.worlds').fetchone()
        if not world_id:
            raise S.StoreError('There is no world in this database to build.')
        project, _ = S.load_world(conn, world_id[0])
        roster = S.load_roster(conn)
        files, updated, _ = map_editor.export_files(project, roster, with_roster=True, stream=True)
        if updated != roster:
            S.save_roster(conn, updated)
        build_id = conn.execute('''INSERT INTO world.builds (world_id, release, created_by, files) VALUES (%s, %s, %s, %s)
                                   RETURNING id''', (world_id[0], release, created_by[:60],
                                                       S.Jsonb({'world.ratw': files['world.ratw']}))).fetchone()[0]
        rows = [(build_id, name[6:-5], text[:text.index('\ngrid:\n') + 7], text, files.get(f'seams/{name[6:-5]}', ''))
                for name, text in files.items() if name.startswith('cells/')]
        with conn.cursor() as cur:
            cur.executemany('INSERT INTO world.build_cells (build_id, cell_id, header, body, seams) VALUES (%s, %s, %s, %s, %s)', rows)
        conn.execute('''DELETE FROM world.build_cells WHERE build_id NOT IN
                        (SELECT id FROM world.builds ORDER BY id DESC LIMIT %s)''', (KEEP_CELLS,))
    return build_id, len(rows)


def game_files(conn, build_id) -> dict[str, str]:
    """A build as the files the game loads: its manifest, and each cell's file and seams (for tests and tools)."""
    manifest = conn.execute('SELECT files FROM world.builds WHERE id = %s', (build_id,)).fetchone()[0]
    out = dict(manifest)
    for cell_id, body, seams in conn.execute('SELECT cell_id, body, seams FROM world.build_cells WHERE build_id = %s ORDER BY cell_id',
                                             (build_id,)).fetchall():
        out[f'cells/{cell_id}.cell'] = body
        out[f'seams/{cell_id}'] = seams
    return out


def latest(conn):
    row = conn.execute('''SELECT id, world_id, release, created_at, created_by, (SELECT count(*) FROM jsonb_object_keys(files))
                          FROM world.builds ORDER BY id DESC LIMIT 1''').fetchone()
    return None if not row else {'id': row[0], 'world': row[1], 'release': row[2], 'at': row[3].isoformat(),
                                 'by': row[4], 'files': row[5]}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('command', choices=('dev', 'latest'))
    args = parser.parse_args(argv)
    try:
        if args.command == 'dev':
            with world_db.connect('dev', 'editor') as conn:
                build_id, count = build(conn, 'world_build.py')
            print(f'DEV build {build_id}: {count} cells.')
        else:
            for database in ('dev', 'prod'):
                with world_db.connect(database, 'publisher') as conn:
                    info = latest(conn)
                print(f'{database.upper()}: ' + (f'build {info["id"]} of {info["world"]}, {info["files"]} files, '
                      + (f'release {info["release"]}, ' if info['release'] else '') + f'{info["at"][:19]}' if info else 'no builds yet'))
    except map_editor.ValidationError as error:
        print('The world cannot be built:\n' + '\n'.join(f'  {e}' for e in error.errors), file=sys.stderr)
        return 1
    except (world_db.DatabaseError, S.StoreError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
