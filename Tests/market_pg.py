#!/usr/bin/env python3
"""The Market Board through Postgres (Docs/Design/79-a-staged-economy.md, 16.7; Phase 1).

    python3 Tests/market_pg.py BUILD/market_tests        (ctest: market_pg)

market_tests --write-board writes a checkpoint whose Board players' trades moved (pressure,
the budget and allowance drawn, the holder's push, the day's net and sell total, a Chapter's weekly discount), the
journal after the save replayed over it. It is saved with game.save_checkpoint into a throwaway database (named
ratw_test_p3stage_<hex>, dropped afterwards), loaded back with game.load_checkpoint, and checked by
market_tests --check-board: the same Board and the same checkpoint. Skipped (passing) when the local PostgreSQL isn't
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


def main(binary: str) -> int:
    try:
        import psycopg
        import world_db as W
    except ImportError as error:
        print(f'market_pg skipped: {error}')
        return 0
    s = W.settings()
    if not s.get('POSTGRES_PASSWORD') or not s.get('RATW_OWNER_PASSWORD') or not s.get('RATW_GAME_PASSWORD'):
        print('market_pg skipped: no PostgreSQL settings (set -a; . Database/.env; set +a)')
        return 0

    def superuser():
        return psycopg.connect(host=s.get('RATW_DB_HOST', '127.0.0.1'), port=s.get('RATW_DB_PORT', '5433'), dbname='postgres',
                               user=s.get('POSTGRES_USER', 'postgres'), password=s['POSTGRES_PASSWORD'], autocommit=True,
                               connect_timeout=3)
    try:
        superuser().close()
    except psycopg.Error as error:
        print(f'market_pg skipped: local PostgreSQL not reachable ({str(error).strip()})')
        return 0

    name = f'ratw_test_p3stage_{secrets.token_hex(4)}'
    assert 'p3stage' in name and name.startswith('ratw_test_')
    with tempfile.TemporaryDirectory(prefix='ratw-p3stage-pg-') as tmp:
        out, back = Path(tmp) / 'out', Path(tmp) / 'back'
        back.mkdir()
        subprocess.run([binary, '--write-board', str(out)], check=True)
        text = (out / 'board.json').read_text(encoding='utf-8')
        doc = json.loads(text)
        board = doc.get('society', {}).get('board', {})
        if not board.get('cell') or not board.get('pushed') or not board.get('budgetLeft'):
            print('market_pg failed: the checkpoint written holds no moved Board')
            return 1
        with superuser() as su:
            su.execute(f'CREATE DATABASE {name} OWNER ratw_owner')
            su.execute(f'REVOKE ALL ON DATABASE {name} FROM PUBLIC')
            su.execute(f'GRANT CONNECT ON DATABASE {name} TO ratw_game')
        try:
            with W.connect('dev', 'owner', dbname=name) as owner:
                assert owner.info.dbname == name
                W.migrate(owner)
                owner.execute("INSERT INTO world.worlds (id, name) VALUES ('w', 'market_pg')")
            with W.connect('dev', 'game', dbname=name) as game:
                assert game.info.dbname == name
                game.execute('SELECT game.save_checkpoint(%s, %s, %s)', ('w', 1, text))
                loaded = json.loads(game.execute('SELECT game.load_checkpoint(%s)', ('w',)).fetchone()[0])
                if loaded != doc:
                    differ = sorted(k for k in set(doc) | set(loaded) if doc.get(k) != loaded.get(k))
                    print(f'market_pg failed: the checkpoint differs through Postgres in {", ".join(differ[:12])}')
                    return 1
                (back / 'board.json').write_text(json.dumps(loaded), encoding='utf-8')
                print(f'  the Board ({len(json.dumps(board))} bytes): saved and loaded back the same')
        finally:
            with superuser() as su:
                su.execute(f'DROP DATABASE IF EXISTS {name} WITH (FORCE)')
        return subprocess.run([binary, '--check-board', str(back), str(out)]).returncode


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1]))
