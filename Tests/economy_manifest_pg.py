#!/usr/bin/env python3
"""A 0060 world's economy migrated to 0061 and loaded by Core (Docs/Design/79-a-staged-economy.md, 14; Phase 9).

    python3 Tests/economy_manifest_pg.py BUILD/authoring_tests        (ctest: economy_manifest_pg)

A throwaway database (ratw_test_s2stage_emf_<hex>, dropped afterwards) migrated to 0060, given a world with every old
economy setting (business_settings rows, a season in four parts, dead and nested rules, a full economy row) and two
residents, then migrated to 0061; live.people_manifest's records, after Greyfen's own map, are loaded by Core
(authoring_tests --economy): the economy read leniently, the overrides line taken, a band read, nothing refused. Skipped
(exit 77) without the local PostgreSQL's settings (set -a; . Database/.env; set +a in the main checkout).
"""
import json
import secrets
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
SKIPPED = 77
PEOPLE = ('economy ', 'route ', 'resident ', 'story ', 'faction ', 'joinable ', 'kin ', 'protected ', 'wander ', 'band ',
          'overrides ', 'loop ', 'season ', 'rule ', 'service ', 'store ', 'nostore ', 'herbs ')


def main(binary: str) -> int:
    try:
        import psycopg
        import world_db as W
    except ImportError as error:
        print(f'economy_manifest_pg skipped: {error}')
        return SKIPPED
    s = W.settings()
    if not s.get('POSTGRES_PASSWORD') or not s.get('RATW_OWNER_PASSWORD') or not s.get('RATW_GAME_PASSWORD'):
        print('economy_manifest_pg skipped: no PostgreSQL settings (set -a; . Database/.env; set +a)')
        return SKIPPED

    def superuser():
        return psycopg.connect(host=s.get('RATW_DB_HOST', '127.0.0.1'), port=s.get('RATW_DB_PORT', '5433'), dbname='postgres',
                               user=s.get('POSTGRES_USER', 'postgres'), password=s['POSTGRES_PASSWORD'], autocommit=True,
                               connect_timeout=3)
    try:
        superuser().close()
    except psycopg.Error as error:
        print(f'economy_manifest_pg skipped: local PostgreSQL not reachable ({str(error).strip()})')
        return SKIPPED
    name = f'ratw_test_s2stage_emf_{secrets.token_hex(4)}'
    assert 's2stage' in name
    with superuser() as su:
        su.execute(f'CREATE DATABASE {name} OWNER ratw_owner')
        su.execute(f'GRANT CONNECT ON DATABASE {name} TO ratw_game')
    try:
        with W.connect('dev', 'owner', dbname=name) as owner:
            assert owner.info.dbname == name
            W.migrate(owner, [f for f in W.migration_files() if int(f[0]) <= 60])
            owner.execute("INSERT INTO world.worlds (id, name) VALUES ('emf', 'economy_manifest_pg')")
            owner.execute(
                "INSERT INTO live.economy (world_id, treasury, store_herbs, store_meals, daily_herbs, daily_meals, internal_store, "
                "store_markup, needs_paused, country, country_off, subsidies, comforts_above, seasons, rules, services, farms) "
                "VALUES ('emf', 1234, 40, 30, 7, 9, false, 120, true, false, '{grain}', '{\"carriage\": 30}', 50, %s, %s, %s, %s)",
                (json.dumps({'bread': {'supply': [100, 120, 90, 80], 'price': [100, 95, 105, 120], 'shelf': [100, 110, 100, 90],
                                       'normal': 12},
                             'fish': {'price': [90, 90, 100, 110], 'shelf': [100, 100, 100, 100]}}),
                 json.dumps({'ripple': 40, 'steer': 1, 'steerLean': 150, 'prosperity': 37, 'prosperityCause': 'the good harvest',
                             'nested': {'a': [1, 2]}, 'neverImported': ['salt'], 'status': {'caps': {'rewardsADay': 200}},
                             'market': {'events': {'perWeek': 2}}}),
                 json.dumps({'groom_and_trim': {'on': False, 'price': 6, 'every': 20}}),
                 json.dumps({'start': 3, 'areas': {'orchards': {'on': False}}})))
            look = json.dumps({'species': 'timber', 'sex': 'male', 'stature': 'average', 'pattern': 'saddle',
                               'baseColor': 5, 'gradientColor': 2, 'markingColor': 1})
            for position, npc in enumerate(('bram', 'cora')):
                owner.execute(
                    "INSERT INTO live.npcs (world_id, id, position, name, role, work_label, age, appearance, voice, paid, hours_start, "
                    "hours_end, purse, herbs, meals, home_x, home_y, work_x, work_y, evening_x, evening_y) VALUES "
                    "('emf', %s, %s, %s, 'civilian', 'Baker', 30, %s, 3, true, 8, 18, 20, 1, 2, 0, 0, 0, 0, 0, 0)",
                    (npc, position, npc.title(), look))
            owner.execute("INSERT INTO live.business_settings (world_id, npc_id, internal_store) VALUES ('emf', 'bram', false)")
            W.migrate(owner)
            owner.execute("UPDATE live.npcs SET band_override = 'poor' WHERE id = 'cora'")
        with W.connect('dev', 'game', dbname=name) as game:
            assert game.info.dbname == name
            people = game.execute("SELECT live.people_manifest('emf')").fetchone()[0]
        with tempfile.TemporaryDirectory(prefix='ratw-s2stage-emf-') as tmp:
            world = Path(tmp) / 'Greyfen'
            shutil.copytree(ROOT / 'Data' / 'Worlds' / 'Greyfen', world)
            own = [l for l in (world / 'world.ratw').read_text(encoding='utf-8').splitlines() if not l.startswith(PEOPLE)]
            (world / 'world.ratw').write_text('\n'.join(own + people.split('\n')) + '\n', encoding='utf-8')
            run = subprocess.run([binary, '--economy', str(world / 'world.ratw')], capture_output=True, text=True)
            if run.returncode != 0:
                print('economy_manifest_pg failed: Core refused the migrated manifest:', run.stderr.strip())
                return 1
            got = json.loads(run.stdout.strip().splitlines()[-1])
        checks = [
            (got['treasury'] == 1234 and got['storeHerbs'] == 40 and got['storeMeals'] == 30, 'the economy line'),
            (got['needsPaused'] is True, 'needs paused'),
            (got['rules'] == {'ripple': 40, 'prosperity': 35}, f'the rules Core has, the slider rounded: {got["rules"]}'),
            (got['seasons'].get('bread', {}).get('price') == [100, 95, 105, 120] and 'fish' in got['seasons'], 'both seasons'),
            (got['services'] == 1, 'the service'),
            (got['cause'] == 'the good harvest' and got['neverImported'] == 1 and got['status'] and got['market'], 'the overrides line'),
            (got['bands'] == {'cora': 'poor'}, f'the band: {got["bands"]}'),
            (got['warnings'] >= 2, 'the old rules skipped with warnings'),
        ]
        failed = [why for ok, why in checks if not ok]
        if failed:
            print('economy_manifest_pg failed:', '; '.join(failed), json.dumps(got))
            return 1
        print(f'economy_manifest_pg: a 0060 world migrated to 0061 loads ({len(checks)} checks)')
        return 0
    finally:
        with superuser() as su:
            su.execute(f'DROP DATABASE IF EXISTS {name} WITH (FORCE)')


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
