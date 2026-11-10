#!/usr/bin/env python3
"""The ships' timetable, and ship day through a crash, through Postgres (Docs/Design/79-a-staged-economy.md, 8.3;
Phase 5, and Phases 4 and 5 merged).

    python3 Tests/port_pg.py BUILD/port_tests        (ctest: port_pg)

port_tests --write-timetable writes a checkpoint of a world with a port: its ships posted a week ahead, one of them in
(it docked after the save: the journal replayed over the checkpoint carries it). It is saved with game.save_checkpoint
into a throwaway database (named ratw_test_s2stage_<hex>, dropped afterwards), loaded back with game.load_checkpoint,
and checked by port_tests --check-timetable: the same ships, the one in still in and never docked twice, the same words
on the Wharf office's notice.

port_tests --write-crashes plays each of its ship crashes (a ship docks, sails, is kept out by a storm, a morning rolls
a new week of ships: each after a checkpoint, then a player's trade journalled, then the game gone without saving; and
each again with a snapshot that doesn't land and a purchase before the crash) and writes the checkpoint and the journal
it left. Each goes into the same throwaway database as the game server stores them (game.save_checkpoint; the journal's
records as rows of game.journal), is read back as the server reads them on start-up (game.load_checkpoint; the records
after the checkpoint's, in order), and port_tests --check-crashes restarts on them: the ship's step has happened once,
with its prices, its shelves and its back rooms. Skipped (exit 77, which ctest lists as Skipped) when the local PostgreSQL isn't
reachable: `set -a; . Database/.env; set +a` in the main checkout. It never opens a world's database: only the
maintenance database `postgres`, to create and drop its own.
"""
import json
import secrets
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'tools'))

SKIPPED = 77                                        # (ctest's SKIP_RETURN_CODE: listed as Skipped, never Passed.)


def main(binary: str) -> int:
    try:
        import psycopg
        import world_db as W
    except ImportError as error:
        print(f'port_pg skipped: {error}')
        return SKIPPED
    s = W.settings()
    if not s.get('POSTGRES_PASSWORD') or not s.get('RATW_OWNER_PASSWORD') or not s.get('RATW_GAME_PASSWORD'):
        print('port_pg skipped: no PostgreSQL settings (set -a; . Database/.env; set +a)')
        return SKIPPED

    def superuser():
        return psycopg.connect(host=s.get('RATW_DB_HOST', '127.0.0.1'), port=s.get('RATW_DB_PORT', '5433'), dbname='postgres',
                               user=s.get('POSTGRES_USER', 'postgres'), password=s['POSTGRES_PASSWORD'], autocommit=True,
                               connect_timeout=3)
    try:
        superuser().close()
    except psycopg.Error as error:
        print(f'port_pg skipped: local PostgreSQL not reachable ({str(error).strip()})')
        return SKIPPED

    name = f'ratw_test_s2stage_{secrets.token_hex(4)}'
    assert 's2stage' in name and name.startswith('ratw_test_')
    with tempfile.TemporaryDirectory(prefix='ratw-s2stage-pg-') as tmp:
        out, back = Path(tmp) / 'out', Path(tmp) / 'back'
        crashed, restarted = Path(tmp) / 'crashed', Path(tmp) / 'restarted'
        back.mkdir()
        restarted.mkdir()
        subprocess.run([binary, '--write-timetable', str(out)], check=True, stdout=subprocess.DEVNULL)
        subprocess.run([binary, '--write-crashes', str(crashed)], check=True, stdout=subprocess.DEVNULL)
        cases = sorted(p.name[:-len('.was.json')] for p in crashed.glob('*.was.json'))
        if not cases:
            print('port_pg failed: no ship crash written')
            return 1
        text = (out / 'timetable.json').read_text(encoding='utf-8')
        doc = json.loads(text)
        expected = json.loads((out / 'timetable.expected.json').read_text(encoding='utf-8'))
        board = doc.get('society', {}).get('board', {})
        ships = board.get('ships') or []
        if len(ships) < 2 or not board.get('shipsThrough') or not any(x.get('status') == 'in' for x in ships):
            print('port_pg failed: the checkpoint written holds no timetable with a ship in')
            return 1
        with superuser() as su:
            su.execute(f'CREATE DATABASE {name} OWNER ratw_owner')
            su.execute(f'REVOKE ALL ON DATABASE {name} FROM PUBLIC')
            su.execute(f'GRANT CONNECT ON DATABASE {name} TO ratw_game')
        try:
            with W.connect('dev', 'owner', dbname=name) as owner:
                assert owner.info.dbname == name
                W.migrate(owner)
                owner.execute("INSERT INTO world.worlds (id, name) VALUES ('w', 'port_pg')")
            with W.connect('dev', 'game', dbname=name) as game:
                assert game.info.dbname == name
                game.execute('SELECT game.save_checkpoint(%s, %s, %s)', ('w', 1, text))
                loaded = json.loads(game.execute('SELECT game.load_checkpoint(%s)', ('w',)).fetchone()[0])
                if loaded != doc:
                    differ = sorted(k for k in set(doc) | set(loaded) if doc.get(k) != loaded.get(k))
                    print(f'port_pg failed: the checkpoint differs through Postgres in {", ".join(differ[:12])}')
                    return 1
                after = loaded.get('society', {}).get('board', {})
                if after.get('ships') != ships or after.get('shipsThrough') != board.get('shipsThrough') or \
                        after.get('nextShip') != board.get('nextShip'):
                    print('port_pg failed: the timetable differs through Postgres')
                    return 1
                (back / 'timetable.json').write_text(json.dumps(loaded), encoding='utf-8')
                print(f'  the timetable ({len(ships)} ships, {expected.get("in")} in): saved and loaded back the same')
                for revision, case in enumerate(cases, 2):  # (A database holds one world: each case in its turn.)
                    text = (crashed / f'{case}.json').read_text(encoding='utf-8')
                    doc = json.loads(text)
                    records = []
                    for line in (crashed / f'{case}.journal').read_text(encoding='utf-8').splitlines():
                        seq, record = line.split('\t', 1)
                        records.append((int(seq), record))
                    after = int(doc.get('journal', 0))
                    if not any(seq > after for seq, _ in records):
                        print(f'port_pg failed: {case}: no record after its checkpoint')
                        return 1
                    game.execute('DELETE FROM game.journal WHERE world_id = %s', ('w',))
                    game.execute('SELECT game.save_checkpoint(%s, %s, %s)', ('w', revision, text))
                    for seq, record in records:     # (As the server's journal thread writes them: RatwGame.cpp.)
                        game.execute('INSERT INTO game.journal (world_id, seq, record) VALUES (%s, %s, %s) ON CONFLICT DO NOTHING',
                                     ('w', seq, record))
                    loaded = json.loads(game.execute('SELECT game.load_checkpoint(%s)', ('w',)).fetchone()[0])
                    if loaded != doc:
                        differ = sorted(k for k in set(doc) | set(loaded) if doc.get(k) != loaded.get(k))
                        print(f'port_pg failed: {case}: the checkpoint differs through Postgres in {", ".join(differ[:12])}')
                        return 1
                    rows = game.execute('SELECT seq, record FROM game.journal WHERE world_id = %s AND seq > %s ORDER BY seq',
                                        ('w', after)).fetchall()
                    if [(seq, json.loads(record)) for seq, record in rows] != \
                            [(seq, json.loads(record)) for seq, record in records if seq > after]:
                        print(f'port_pg failed: {case}: the journal differs through Postgres')
                        return 1
                    (restarted / f'{case}.json').write_text(json.dumps(loaded), encoding='utf-8')
                    (restarted / f'{case}.journal').write_text(''.join(f'{seq}\t{record}\n' for seq, record in rows), encoding='utf-8')
                    print(f'  {case}: its checkpoint and {len(rows)} journal record(s) saved and read back the same')
        finally:
            with superuser() as su:
                su.execute(f'DROP DATABASE IF EXISTS {name} WITH (FORCE)')
        timetable = subprocess.run([binary, '--check-timetable', str(back), str(out)]).returncode
        return timetable or subprocess.run([binary, '--check-crashes', str(restarted), str(crashed)]).returncode


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1]))
