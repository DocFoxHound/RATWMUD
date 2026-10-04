#!/usr/bin/env python3
"""One game server per world (Core/RatwGameOwner.cpp): a second server on the same database is refused and told who
holds it; a server that is killed lets go at once. A scratch database with Greyfen, dropped afterwards; skipped without
the local PostgreSQL or a built build-core/ratw_server."""
import os
import secrets
import signal
import socket
import subprocess
import time
import unittest
from pathlib import Path

import world_build
import world_db as W
import world_store as S
from test_world_db import database_available, superuser
from test_world_store import greyfen

ROOT = Path(__file__).resolve().parent.parent
SERVER = ROOT / 'build-core' / 'ratw_server'


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


@unittest.skipUnless(database_available() and SERVER.exists(), 'local PostgreSQL not running, or ratw_server not built')
class OneServerPerWorld(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        W.ensure_roles()
        cls.name = f'ratw_test_one_{secrets.token_hex(4)}'
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


if __name__ == '__main__':
    unittest.main()
