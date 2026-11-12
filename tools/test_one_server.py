#!/usr/bin/env python3
"""One game server per world (Core/RatwGameOwner.cpp): a second server on the same database is refused and told who
holds it; a server that is killed lets go at once. A scratch database with Greyfen, dropped afterwards; skipped without
the local PostgreSQL or a built build-core/ratw_server.

And the farms' people on a server of a world in the database (Docs/Design/79-a-staged-economy.md, Progress, "Wagons and
farms on DEV"): a real server places the farm's keeper from the unplaced and writes the row; a scratch server places
them for itself and writes nothing."""
import copy
import json
import os
import secrets
import shutil
import signal
import socket
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

import world_build
import world_db as W
import world_store as S
from test_world_db import database_available, superuser
from test_world_store import greyfen

# A throwaway database's name tag (RATW_TEST_DB_TAG, default ''): a run tagged s2stage_ makes ratw_test_s2stage_*.
DB_TAG = os.environ.get('RATW_TEST_DB_TAG', '')

ROOT = Path(__file__).resolve().parent.parent
# (RATW_TEST_SERVER: another build's, such as a worktree's, as test_world_package takes it.)
SERVER = Path(os.environ.get('RATW_TEST_SERVER') or ROOT / 'build-core' / 'ratw_server')


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


@unittest.skipUnless(database_available() and SERVER.exists(), 'local PostgreSQL not running, or ratw_server not built')
class OneServerPerWorld(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        W.ensure_roles()
        cls.name = f'ratw_test_{DB_TAG}one_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor, ratw_publisher, ratw_game, ratw_dm')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner)
        with W.connect('dev', 'editor', dbname=cls.name) as editor:
            S.save_roster(editor, S.load_roster(editor))
            S.save_world(editor, greyfen(), None, create=True)
            world_build.build(editor, 'test')
        info = {**W.conninfo('dev', 'game'), 'dbname': cls.name}
        cls.url = ' '.join(f'{k}={v}' for k, v in info.items())

    @classmethod
    def tearDownClass(cls):
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def start(self, seconds=30, *extra):
        return subprocess.Popen([str(SERVER), '--database', 'dev', '--port', str(free_port()), '--for', str(seconds), *extra],
                                env={**os.environ, 'RATW_DATABASE_URL': self.url, 'RATW_AI': 'off'},
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    def wait_until_ready(self, server):
        for line in server.stdout:
            if 'listening on port' in line:
                return
        self.fail('the first server never became ready')

    def test_a_second_server_is_refused_and_a_killed_one_lets_go(self):
        first = self.start()
        try:
            self.wait_until_ready(first)
            second = self.start(5)
            out, _ = second.communicate(timeout=60)
            self.assertNotEqual(second.returncode, 0, 'the second server does not run')
            self.assertIn('already being run by ratw_server pid', out)
            self.assertIn(str(first.pid), out, 'and is told which process holds the world')
        finally:
            first.send_signal(signal.SIGKILL)       # No clean stop: the lock must still go with the process.
            first.wait()
            first.stdout.close()
        third = self.start(3)
        try:
            self.wait_until_ready(third)            # The world is free again at once.
        finally:
            third.send_signal(signal.SIGTERM)
            third.wait(timeout=60)
            third.stdout.close()

    def what_the_database_holds(self):
        with W.connect('dev', 'owner', dbname=self.name) as owner:
            return owner.execute(
                "SELECT (SELECT count(*) FROM game.checkpoints), (SELECT max(saved_at)::text || max(revision)::text FROM game.checkpoints), "
                "(SELECT count(*) FROM game.journal), (SELECT count(*) FROM game.events), "
                "(SELECT count(*) FROM dm.actions WHERE status = 'queued'), (SELECT count(*) FROM dm.health)").fetchone()

    def test_a_scratch_server_runs_beside_the_owner_and_writes_nothing(self):
        owner = self.start(60)
        try:
            self.wait_until_ready(owner)
            owner.send_signal(signal.SIGSTOP)       # (Holding still, so only the scratch server could change anything.)
            with W.connect('dev', 'owner', dbname=self.name) as conn:
                conn.execute("INSERT INTO dm.actions (kind, target_id, requested_by, status) "
                             "VALUES ('npc.kill', 'sloe', 'test', 'queued')")
            before = self.what_the_database_holds()
            scratch = self.start(60, '--scratch', '--idle-exit', '3')
            out, _ = scratch.communicate(timeout=120)
            self.assertEqual(scratch.returncode, 0, out)
            self.assertIn('RATW_SCRATCH a scratch server', out, 'it runs, though another server owns the world')
            self.assertIn('nobody connected for 3 s: stopping', out, 'and stops after --idle-exit with nobody connected')
            self.assertEqual(self.what_the_database_holds(), before,
                             'no save, journal, event, health row, and the DM\'s queued action left for the real server')
        finally:
            owner.send_signal(signal.SIGCONT)
            owner.send_signal(signal.SIGTERM)
            owner.wait(timeout=60)
            owner.stdout.close()


@unittest.skipUnless(database_available() and SERVER.exists(), 'local PostgreSQL not running, or ratw_server not built')
class FarmPeopleOnAServer(unittest.TestCase):
    """Greyfen with one unplaced NPC of the fields, and a catalogue (RATW_DATA_DIR: a copy of Data) whose one farm place
    is Greyfen's longhouse (its work room) and cottage (its farmhouse): the farms' rule sends for the NPC as the farm's
    keeper (Core/RatwFarmPeople.cpp), and the server carries it out (Game::makeResidents)."""
    WHO = 'hob_wheatley'

    @classmethod
    def setUpClass(cls):
        W.ensure_roles()
        cls.name = f'ratw_test_{DB_TAG}farm_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor, ratw_publisher, ratw_game, ratw_dm')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner)
        world = greyfen()
        hob = copy.deepcopy(next(p for p in world['people'] if p['role'] != 'merchant' and p['role'] != 'guard'))
        hob.update(id=cls.WHO, name='Hob Wheatley', role='civilian', workLabel='works the fields at Wheatley', age=41,
                   home=None, work=None, evening=None, route='', paid=False)
        world['people'].append(hob)
        with W.connect('dev', 'editor', dbname=cls.name) as editor:
            S.save_roster(editor, S.load_roster(editor))
            S.save_world(editor, world, None, create=True)
            world_build.build(editor, 'test')
        info = {**W.conninfo('dev', 'game'), 'dbname': cls.name}
        cls.url = ' '.join(f'{k}={v}' for k, v in info.items())
        # The catalogue: the game's own, its farm places only Longmead's, at Greyfen's longhouse and cottage.
        cls.tmp = tempfile.mkdtemp(prefix='ratw-farm-people-')
        cls.data = Path(cls.tmp) / 'Data'
        shutil.copytree(ROOT / 'Data', cls.data)
        path = cls.data / 'Economy' / 'communities.json'
        places = json.loads(path.read_text(encoding='utf-8'))
        farm = next(f for f in places['farms'] if f['farm'] == 'grain_farm_1')
        farm.update(community='ridgemere', workplace='longhouse',
                    workplaces=[{'area': 'longhouse', 'name': 'the longhouse', 'work': [{'x': 4, 'y': 4}], 'stations': []}],
                    home={'area': 'cottage', 'name': 'the cottage', 'capacity': 6, 'beds': [{'x': 3, 'y': 3}, {'x': 5, 'y': 3}]})
        places.update(farms=[farm], works=[], communities=[])
        path.write_text(json.dumps(places), encoding='utf-8')

    @classmethod
    def tearDownClass(cls):
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def start(self, seconds, *extra):
        return subprocess.Popen([str(SERVER), '--database', 'dev', '--port', str(free_port()), '--for', str(seconds), *extra],
                                env={**os.environ, 'RATW_DATABASE_URL': self.url, 'RATW_AI': 'off', 'RATW_DATA_DIR': str(self.data)},
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    def row(self):
        with W.connect('dev', 'owner', dbname=self.name) as owner:
            return owner.execute("SELECT home_area, work_area, work_label, role, paid FROM live.npcs WHERE id = %s", (self.WHO,)).fetchone()

    def test_1_a_scratch_server_places_them_for_itself_and_writes_no_row(self):
        self.assertEqual(self.row()[:2], (None, None), 'unplaced in the database')
        scratch = self.start(8, '--scratch')
        out, _ = scratch.communicate(timeout=120)
        self.assertEqual(scratch.returncode, 0, out)
        self.assertIn('residents=11', out, 'the world loads with its eleven placed residents')
        self.assertIn('RATW_RESIDENT 1 made for this scratch server only (no row written)', out,
                      'the farm\'s keeper arrives on the scratch server')
        self.assertNotIn('could not be placed at a farm', out)
        self.assertNotIn('read-only transaction', out)
        self.assertEqual(self.row()[:2], (None, None), 'and stays unplaced in the database')

    def test_2_a_real_server_places_them_and_a_restart_finds_them_there(self):
        first = self.start(8)
        out, _ = first.communicate(timeout=120)
        self.assertEqual(first.returncode, 0, out)
        self.assertIn('RATW_RESIDENT farm: Hob Wheatley (hob_wheatley)', out, 'the farm\'s keeper arrives')
        home, work, label, role, paid = self.row()
        self.assertEqual((home, work, role, paid), ('cottage', 'longhouse', 'civilian', True), 'the row has the home and the post')
        self.assertTrue(label and label != 'works the fields at Wheatley', f'and the farm\'s label: {label}')
        again = self.start(8)
        out, _ = again.communicate(timeout=120)
        self.assertEqual(again.returncode, 0, out)
        self.assertIn('residents=12', out, 'a restart loads them as a resident')
        self.assertNotIn('RATW_RESIDENT', out, 'and sends for nobody')
        self.assertEqual(self.row()[:2], ('cottage', 'longhouse'))


if __name__ == '__main__':
    unittest.main()
