#!/usr/bin/env python3
"""The ships' timetable through Postgres (Docs/Design/79-a-staged-economy.md, 8.3; Phase 5).

    python3 Tests/port_pg.py BUILD/port_tests        (ctest: port_pg)

port_tests --write-timetable writes a checkpoint of a world with a port: its ships posted a week ahead, one of them in
(it docked after the save: the journal replayed over the checkpoint carries it). It is saved with game.save_checkpoint
into a throwaway database (named ratw_test_s2stage_<hex>, dropped afterwards), loaded back with game.load_checkpoint,
and checked by port_tests --check-timetable: the same ships, the one in still in and never docked twice, the same words
on the Wharf office's notice. Skipped (exit 77, which ctest lists as Skipped) when the local PostgreSQL isn't
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
        back.mkdir()
        subprocess.run([binary, '--write-timetable', str(out)], check=True, stdout=subprocess.DEVNULL)
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
        finally:
            with superuser() as su:
                su.execute(f'DROP DATABASE IF EXISTS {name} WITH (FORCE)')
        return subprocess.run([binary, '--check-timetable', str(back), str(out)]).returncode


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1]))
