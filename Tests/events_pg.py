#!/usr/bin/env python3
"""Staged events and causes through a crash, through Postgres (Docs/Design/79-a-staged-economy.md, 8.4-8.6; Phase 7).

    python3 Tests/events_pg.py BUILD/events_tests        (ctest: events_pg)

events_tests --write-crashes plays each of its event crashes (an event begins, one is over, a morning rolls the next
week's events, a ship is lost, the slider's cause is told on every board: each after a checkpoint, then a player's trade
journalled, then the game gone without saving; and each again with a snapshot that doesn't land and a purchase before
the crash) and writes the checkpoint and the journal it left. Each goes into a throwaway database (named
ratw_test_s2stage_<hex>, dropped afterwards) as the game server stores them (game.save_checkpoint; the journal's records
as rows of game.journal), is read back as the server reads them on start-up (game.load_checkpoint; the records after the
checkpoint's, in order), and events_tests --check-crashes restarts on them: the step has happened once, with its prices,
its notices and its Times. Skipped (exit 77, which ctest lists as Skipped) when the local PostgreSQL isn't reachable:
`set -a; . Database/.env; set +a` in the main checkout. It never opens a world's database: only the maintenance
database `postgres`, to create and drop its own.
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
        print(f'events_pg skipped: {error}')
        return SKIPPED
    s = W.settings()
    if not s.get('POSTGRES_PASSWORD') or not s.get('RATW_OWNER_PASSWORD') or not s.get('RATW_GAME_PASSWORD'):
        print('events_pg skipped: no PostgreSQL settings (set -a; . Database/.env; set +a)')
        return SKIPPED

    def superuser():
        return psycopg.connect(host=s.get('RATW_DB_HOST', '127.0.0.1'), port=s.get('RATW_DB_PORT', '5433'), dbname='postgres',
                               user=s.get('POSTGRES_USER', 'postgres'), password=s['POSTGRES_PASSWORD'], autocommit=True,
                               connect_timeout=3)
    try:
        superuser().close()
    except psycopg.Error as error:
        print(f'events_pg skipped: local PostgreSQL not reachable ({str(error).strip()})')
        return SKIPPED

    name = f'ratw_test_s2stage_{secrets.token_hex(4)}'
    assert 's2stage' in name and name.startswith('ratw_test_')
    with tempfile.TemporaryDirectory(prefix='ratw-s2stage-pg-') as tmp:
        crashed, restarted = Path(tmp) / 'crashed', Path(tmp) / 'restarted'
        restarted.mkdir()
        subprocess.run([binary, '--write-crashes', str(crashed)], check=True, stdout=subprocess.DEVNULL)
        cases = sorted(p.name[:-len('.was.json')] for p in crashed.glob('*.was.json'))
        if not cases:
            print('events_pg failed: no event crash written')
            return 1
        with superuser() as su:
            su.execute(f'CREATE DATABASE {name} OWNER ratw_owner')
            su.execute(f'REVOKE ALL ON DATABASE {name} FROM PUBLIC')
            su.execute(f'GRANT CONNECT ON DATABASE {name} TO ratw_game')
        try:
            with W.connect('dev', 'owner', dbname=name) as owner:
                assert owner.info.dbname == name
                W.migrate(owner)
                owner.execute("INSERT INTO world.worlds (id, name) VALUES ('w', 'events_pg')")
            with W.connect('dev', 'game', dbname=name) as game:
                assert game.info.dbname == name
                for revision, case in enumerate(cases, 1):  # (A database holds one world: each case in its turn.)
                    text = (crashed / f'{case}.json').read_text(encoding='utf-8')
                    doc = json.loads(text)
                    records = []
                    for line in (crashed / f'{case}.journal').read_text(encoding='utf-8').splitlines():
                        seq, record = line.split('\t', 1)
                        records.append((int(seq), record))
                    after = int(doc.get('journal', 0))
                    if not any(seq > after for seq, _ in records):
                        print(f'events_pg failed: {case}: no record after its checkpoint')
                        return 1
                    game.execute('DELETE FROM game.journal WHERE world_id = %s', ('w',))
                    game.execute('SELECT game.save_checkpoint(%s, %s, %s)', ('w', revision, text))
                    for seq, record in records:     # (As the server's journal thread writes them: RatwGame.cpp.)
                        game.execute('INSERT INTO game.journal (world_id, seq, record) VALUES (%s, %s, %s) ON CONFLICT DO NOTHING',
                                     ('w', seq, record))
                    loaded = json.loads(game.execute('SELECT game.load_checkpoint(%s)', ('w',)).fetchone()[0])
                    if loaded != doc:
                        differ = sorted(k for k in set(doc) | set(loaded) if doc.get(k) != loaded.get(k))
                        print(f'events_pg failed: {case}: the checkpoint differs through Postgres in {", ".join(differ[:12])}')
                        return 1
                    rows = game.execute('SELECT seq, record FROM game.journal WHERE world_id = %s AND seq > %s ORDER BY seq',
                                        ('w', after)).fetchall()
                    if [(seq, json.loads(record)) for seq, record in rows] != \
                            [(seq, json.loads(record)) for seq, record in records if seq > after]:
                        print(f'events_pg failed: {case}: the journal differs through Postgres')
                        return 1
                    (restarted / f'{case}.json').write_text(json.dumps(loaded), encoding='utf-8')
                    (restarted / f'{case}.journal').write_text(''.join(f'{seq}\t{record}\n' for seq, record in rows), encoding='utf-8')
                    print(f'  {case}: its checkpoint and {len(rows)} journal record(s) saved and read back the same')
        finally:
            with superuser() as su:
                su.execute(f'DROP DATABASE IF EXISTS {name} WITH (FORCE)')
        return subprocess.run([binary, '--check-crashes', str(restarted), str(crashed)]).returncode


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1]))
