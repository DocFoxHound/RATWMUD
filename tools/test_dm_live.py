#!/usr/bin/env python3
"""A Dungeon Master's queued action, end to end (Docs/Design/38-injuries.md, phase 5): a real game server on a scratch
database with Greyfen, a player signed in (tools/client/hold.ts), and dm.actions rows the server applies. An injury
given to the player online reaches their page and is told to them; once they have left, one is taken away and another
given offline; and the saved character holds what is left. Skipped without the local PostgreSQL, a built server or
Node. RATW_SERVER picks the server binary (build-core/ratw_server by default)."""
import json
import os
import secrets
import shutil
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

# A throwaway database's name tag (RATW_TEST_DB_TAG, default ''): a run tagged s2stage_ makes ratw_test_s2stage_*.
DB_TAG = os.environ.get('RATW_TEST_DB_TAG', '')

ROOT = Path(__file__).resolve().parent.parent
SERVER = Path(os.environ.get('RATW_SERVER', ROOT / 'build-core' / 'ratw_server'))
NODE = shutil.which('node')


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


@unittest.skipUnless(database_available() and SERVER.exists() and NODE, 'local PostgreSQL, a built ratw_server or Node missing')
class DungeonMasterLive(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        W.ensure_roles()
        cls.name = f'ratw_test_{DB_TAG}dmlive_{secrets.token_hex(4)}'
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

    def queue(self, kind, target, payload):
        with W.connect('dev', 'owner', dbname=self.name) as conn:
            action = conn.execute('INSERT INTO dm.actions (kind, target_id, requested_by, payload) VALUES (%s, %s, %s, %s) RETURNING id',
                                  (kind, target, 'test', json.dumps(payload))).fetchone()[0]
            conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action}),))   # (As the DM tool does.)
            return action

    def outcome(self, action, seconds=15):
        """The action's status and result, once the server has dealt with it."""
        deadline = time.time() + seconds
        while time.time() < deadline:
            with W.connect('dev', 'owner', dbname=self.name) as conn:
                status, result = conn.execute('SELECT status, result FROM dm.actions WHERE id = %s', (action,)).fetchone()
            if status != 'queued':
                return status, result
            time.sleep(.25)
        self.fail(f'action {action} was never applied')

    def test_the_dm_role_closes_and_opens_its_world(self):
        """Migration 0062 (Docs/Design/78, 7; Phase M1): ratw_dm may update admin.server_state's four columns (Close and
        Open without the publisher's password), the server told as before; never insert, delete or touch `singleton`."""
        import psycopg
        import world_sync as WS
        with W.connect('dev', 'game', dbname=self.name) as listener:
            listener.execute('LISTEN ratw_maintenance')
            try:
                with W.connect('dev', 'dm', dbname=self.name) as dm:
                    self.assertEqual(WS.set_state(dm, 'maintenance', 'host-ab12', 'Closed for the festival.')['state'], 'maintenance')
                    for refused in ("INSERT INTO admin.server_state (singleton) VALUES (true)", 'DELETE FROM admin.server_state',
                                    'UPDATE admin.server_state SET singleton = true'):
                        with self.assertRaises(psycopg.errors.InsufficientPrivilege, msg=refused):
                            with dm.transaction():
                                dm.execute(refused)
                    self.assertEqual(WS.set_state(dm, 'running', 'host-ab12')['state'], 'running')
                said = [json.loads(n.payload) for n in listener.notifies(timeout=2, stop_after=2)]
                self.assertEqual([(n['state'], n['by']) for n in said], [('maintenance', 'host-ab12'), ('running', 'host-ab12')])
            finally:
                with W.connect('dev', 'owner', dbname=self.name) as owner:
                    WS.set_state(owner, 'running')

    def test_an_injury_given_online_and_offline(self):
        port = free_port()
        server = subprocess.Popen([str(SERVER), '--database', 'dev', '--port', str(port), '--for', '120', '--dev-identity'],
                                  env={**os.environ, 'RATW_DATABASE_URL': self.url, 'RATW_AI': 'off'},
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        player = None
        try:
            for line in server.stdout:
                if 'listening on port' in line:
                    break
            player = subprocess.Popen([NODE, '--experimental-strip-types', '--no-warnings', str(ROOT / 'tools' / 'client' / 'hold.ts'),
                                       '--port', str(port), '--identity', 'ada', '--seconds', '60'],
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=ROOT)
            seen = []

            def until(test, what, seconds=20):
                for earlier in seen:                # (Lines may come in either order.)
                    if test(earlier):
                        return earlier
                deadline = time.time() + seconds
                while time.time() < deadline:
                    line = player.stdout.readline()
                    if not line:
                        break
                    try:
                        seen.append(json.loads(line))
                    except ValueError:
                        continue
                    if test(seen[-1]):
                        return seen[-1]
                self.fail(f'{what} never came: {seen[-6:]}')

            who = until(lambda o: 'in' in o, 'the player in the world')['in']
            # Online: given, told, and on the page.
            given = self.queue('character.injury', who, {'add': 'cracked_rib', 'severity': 3})
            listed = until(lambda o: any(i.get('name') == 'Cracked rib' for i in o.get('injuries', [])), 'the injury on the page')
            rib = next(i for i in listed['injuries'] if i['name'] == 'Cracked rib')
            self.assertEqual((rib['kind'], rib['severity']), ('acute', 'severe'), 'a severe cracked rib, as asked')
            self.assertIn('rest', rib['line'], 'told with the rest it needs: ' + rib['line'])
            until(lambda o: 'You find you have an injury: Cracked rib' in o.get('said', ''), 'the player told')
            status, result = self.outcome(given)
            self.assertEqual(status, 'applied', result)
            self.assertIn('Cracked rib given', result)
            # Offline: the player leaves; the rib is taken away and a bent tail given.
            player.send_signal(signal.SIGTERM)
            player.wait(timeout=10)
            time.sleep(3)
            taken = self.queue('character.injury', who, {'remove': rib['id']})
            status, result = self.outcome(taken)
            self.assertEqual(status, 'applied', result)
            self.assertIn('taken away', result)
            tail = self.queue('character.injury', who, {'add': 'bent_tail'})
            status, result = self.outcome(tail)
            self.assertEqual(status, 'applied', result)
            self.assertIn('Bent tail given', result)
            bad = self.queue('character.injury', who, {'add': 'broken_heart'})
            self.assertEqual(self.outcome(bad)[0], 'refused', 'an unknown injury is refused by the server too')
        finally:
            if player and player.poll() is None:
                player.kill()
            if player:
                player.wait(timeout=10)
                player.stdout.close()
            server.send_signal(signal.SIGTERM)
            out, _ = server.communicate(timeout=60)
        # Saved: the character holds the tail, not the rib.
        with W.connect('dev', 'owner', dbname=self.name) as conn:
            row = conn.execute('SELECT data FROM game.characters WHERE key = %s', (who,)).fetchone()
        self.assertIsNotNone(row, 'the character is saved')
        kept = [i['type'] for i in (row[0].get('injuries') or [])]
        self.assertEqual(kept, ['bent_tail'], 'saved with the bent tail only: ' + json.dumps(row[0].get('injuries')))

    def start(self, seconds=120, extra=()):
        """A game server on this database (the class's), once it is listening."""
        server = subprocess.Popen([str(SERVER), '--database', 'dev', '--port', str(free_port()), '--for', str(seconds), '--dev-identity',
                                   *extra],
                                  env={**os.environ, 'RATW_DATABASE_URL': self.url, 'RATW_AI': 'off'},
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in server.stdout:
            if 'listening on port' in line:
                return server, int(line.split('listening on port')[1].split()[0])
        server.wait(timeout=30)
        self.fail('the server never listened')

    def checkpoint(self):
        """(saved_at, the journal seq it covers) of the world's checkpoint."""
        with W.connect('dev', 'owner', dbname=self.name) as conn:
            row = conn.execute("SELECT saved_at, coalesce((payload::jsonb->>'journal')::bigint, 0) FROM game.checkpoints").fetchone()
        return row if row else (None, 0)

    def saved_cash(self, pc):
        """A player's cash in the world's checkpoint (None: not saved)."""
        with W.connect('dev', 'owner', dbname=self.name) as conn:
            row = conn.execute("SELECT payload::jsonb->'society'->'accounts'->%s->>'cash' FROM game.checkpoints", (pc,)).fetchone()
        return int(row[0]) if row and row[0] is not None else None

    def end_the_dead_servers_sessions(self):
        """A killed server's database sessions, ended as a real crash leaves them (a statement it was waiting on goes)."""
        with superuser() as su:
            su.execute("SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname = %s AND usename = 'ratw_game'",
                       (self.name,))

    def test_a_grant_through_the_journals_windows(self):
        """Doc 79, Phase 9's first repair: a DM's act is told (to the DM's dm.actions row and to the player) only once its
        journal record is committed, and is applied once whatever a crash interrupts. (1) The journal held (game.journal
        locked): the grant is applied in the game, but the row stays queued and the player hears nothing; the server is
        killed; the next one applies the still-queued row once. (2) The row held (FOR UPDATE) after the record commits, so
        the server can't mark it: killed there, the next one finds the act in the journal's record (dmActs) and answers the
        row from it, never paying twice."""
        import threading
        import dungeon_master as D
        dm = D.DungeonMaster(lambda target: W.connect('dev', 'dm', dbname=self.name), clock=time.time)
        who = {'username': 'test', 'role': 'dm'}
        server, port = self.start(240)
        player, heard = None, []
        try:
            player = subprocess.Popen([NODE, '--experimental-strip-types', '--no-warnings', str(ROOT / 'tools' / 'client' / 'hold.ts'),
                                       '--port', str(port), '--identity', 'cid', '--seconds', '90'],
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=ROOT)
            pc = None
            for line in player.stdout:
                try:
                    pc = json.loads(line).get('in') or pc
                except ValueError:
                    continue
                if pc:
                    break
            self.assertTrue(pc, 'the player came in')
            threading.Thread(target=lambda: [heard.append(l) for l in player.stdout], daemon=True).start()
            deadline = time.time() + 45
            while self.saved_cash(pc) is None and time.time() < deadline:
                time.sleep(.5)
            had = self.saved_cash(pc)
            self.assertIsNotNone(had, 'the player\'s purse in a checkpoint')
            # (1) The journal held.
            with W.connect('dev', 'owner', dbname=self.name) as lock:
                with lock.transaction():
                    lock.execute('LOCK TABLE game.journal IN EXCLUSIVE MODE')
                    first = dm.act(who, 'dev', 'character.grant', pc, {'coins': 23, 'reason': 'for the lost ewe'})['id']
                    # (Second repair: two economy.sync rows in one poll, the slider moved "now": the second, answered
                    # with the first's outcome, waits for the first's record too.)
                    with W.connect('dev', 'owner', dbname=self.name) as conn:
                        S.patch_economy_rules(conn, 'greyfen', {'prosperity': -50})
                        syncs = [conn.execute("INSERT INTO dm.actions (kind, target_id, requested_by, payload) VALUES "
                                              "('economy.sync', 'economy', 'test', %s) RETURNING id",
                                              (json.dumps({'now': True}),)).fetchone()[0] for _ in range(2)]
                        conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': syncs[0]}),))
                    time.sleep(5)
                    with W.connect('dev', 'owner', dbname=self.name) as look:
                        status = look.execute('SELECT status FROM dm.actions WHERE id = %s', (first,)).fetchone()[0]
                        synced = [look.execute('SELECT status FROM dm.actions WHERE id = %s', (a,)).fetchone()[0] for a in syncs]
                    self.assertEqual(status, 'queued', 'not "applied" while its record isn\'t written')
                    self.assertEqual(synced, ['queued', 'queued'], 'neither economy.sync "applied" before the record')
                    self.assertFalse([l for l in heard if 'grants you' in l], 'the player not told either: ' + ''.join(heard[-5:]))
                    server.kill()
                    server.communicate(timeout=60)
                    self.end_the_dead_servers_sessions()
        finally:
            if player and player.poll() is None:
                player.kill()
            if player:
                player.wait(timeout=10)
            if server.poll() is None:
                server.kill()
                server.communicate(timeout=60)
        server, _ = self.start(240)
        try:
            status, result = self.outcome(first, 30)
            self.assertEqual((status, result), ('applied', 'Granted 23p: for the lost ewe.'), 'the next server applies the queued row')
            for a in syncs:
                self.assertEqual(self.outcome(a, 30)[0], 'applied', 'the queued syncs applied by the next server')
            # (2) The rows held after their records commit. Second repair: the next server comes back more than ten
            # minutes after the first was asked (its requested_at set back), and the expiry caught the other meanwhile:
            # both answered from the records, never "expired" (a DM told that would pay again).
            grants = []
            with W.connect('dev', 'owner', dbname=self.name) as conn:
                for reason in ('for the second ewe', 'for the third ewe'):
                    grants.append(conn.execute('INSERT INTO dm.actions (kind, target_id, requested_by, payload) VALUES (%s, %s, %s, %s) RETURNING id',
                                               ('character.grant', pc, 'test', json.dumps({'coins': 23, 'reason': reason}))).fetchone()[0])
            second, third = grants
            with W.connect('dev', 'owner', dbname=self.name) as row_lock:
                with row_lock.transaction():
                    row_lock.execute('SELECT 1 FROM dm.actions WHERE id = ANY(%s) FOR UPDATE', (grants,))
                    with W.connect('dev', 'owner', dbname=self.name) as conn:
                        conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': second}),))
                    deadline, found = time.time() + 20, 0
                    while time.time() < deadline and found < 2:
                        with W.connect('dev', 'owner', dbname=self.name) as conn:
                            found = sum(conn.execute("SELECT count(*) FROM game.journal WHERE record LIKE '%%\"dmActs\"%%' "
                                                     'AND record LIKE %s', (f'%"{g}"%',)).fetchone()[0] > 0 for g in grants)
                        time.sleep(.25)
                    self.assertEqual(found, 2, 'the grants\' records, with their dm.actions ids, committed')
                    time.sleep(1)
                    server.kill()
                    server.communicate(timeout=60)
                    self.end_the_dead_servers_sessions()
            with W.connect('dev', 'owner', dbname=self.name) as conn:
                self.assertEqual([conn.execute('SELECT status FROM dm.actions WHERE id = %s', (g,)).fetchone()[0] for g in grants],
                                 ['queued', 'queued'])
                conn.execute("UPDATE dm.actions SET requested_at = now() - interval '11 minutes' WHERE id = %s", (second,))
                conn.execute("UPDATE dm.actions SET status = 'expired', result = 'Not applied within ten minutes.', done_at = now(), "
                             "requested_at = now() - interval '11 minutes' WHERE id = %s", (third,))
        finally:
            if server.poll() is None:
                server.kill()
                server.communicate(timeout=60)
        server, _ = self.start(240)
        try:
            status, result = self.outcome(second, 30)
            self.assertEqual((status, result), ('applied', 'Granted 23p: for the second ewe.'), 'answered from the record, not expired')
            deadline = time.time() + 30
            while time.time() < deadline:
                with W.connect('dev', 'owner', dbname=self.name) as conn:
                    status, result = conn.execute('SELECT status, result FROM dm.actions WHERE id = %s', (third,)).fetchone()
                if status == 'applied':
                    break
                time.sleep(.25)
            self.assertEqual((status, result), ('applied', 'Granted 23p: for the third ewe.'), 'an expired row the record holds set right')
        finally:
            server.send_signal(signal.SIGTERM)
            out, _ = server.communicate(timeout=60)
        self.assertIn('already applied before the restart', out)
        self.assertEqual(self.saved_cash(pc), had + 69, 'each grant paid once: 23, 23 and 23, never 92')

    def test_a_clean_stop_answers_the_acts_still_waiting(self):
        """Doc 79, Phase 9's last repair: an act applied whose journal record wasn't written yet when the server was
        stopped cleanly stayed "queued" in the DM's tool until some later server started. The stop now waits for the
        journal and answers it. The journal is held (game.journal locked) while a Times nudge is applied; the server is
        told to stop; the journal is let go; the row reads "applied" once the server has gone, with no other server."""
        import dungeon_master as D
        dm = D.DungeonMaster(lambda target: W.connect('dev', 'dm', dbname=self.name), clock=time.time)
        server, _ = self.start(240)
        try:
            report, deadline = None, time.time() + 60
            while not report and time.time() < deadline:
                report = dm.prosperity('dev')['report']
                if not report:
                    time.sleep(.5)
            self.assertTrue(report, 'a checkpoint with the report')
            market = report['markets'][0]['id']
            with W.connect('dev', 'owner', dbname=self.name) as lock:
                with lock.transaction():
                    lock.execute('LOCK TABLE game.journal IN EXCLUSIVE MODE')
                    action = self.queue('times.nudge', market, {'points': 15, 'days': 2, 'words': 'a fair wind'})
                    time.sleep(5)
                    with W.connect('dev', 'owner', dbname=self.name) as look:
                        status = look.execute('SELECT status FROM dm.actions WHERE id = %s', (action,)).fetchone()[0]
                    self.assertEqual(status, 'queued', 'applied in the game, its record not yet written')
                    server.send_signal(signal.SIGTERM)
                    time.sleep(3)                       # (The stop waits on the journal.)
            server.communicate(timeout=90)
        finally:
            if server.poll() is None:
                server.kill()
                server.communicate(timeout=60)
        with W.connect('dev', 'owner', dbname=self.name) as conn:
            status, result = conn.execute('SELECT status, result FROM dm.actions WHERE id = %s', (action,)).fetchone()
        self.assertEqual(status, 'applied', f'answered at the clean stop, with no later server: {result}')
        self.assertIn('nudged +15 for 2 days: a fair wind', result)

    def test_another_kinds_act_is_answered_from_its_record_after_a_crash(self):
        """Doc 79, Phase 9's last repair: the other dm.actions kinds that journal what they do (here npc.sync, an NPC's
        authored band taken up) carry their dm.actions id in the record too. The row is held (FOR UPDATE) once the record
        is committed, so the server can't mark it; killed there, the next server answers the row from the record and
        never applies the act again."""
        with W.connect('dev', 'owner', dbname=self.name) as conn:
            npc = conn.execute("SELECT id FROM live.npcs WHERE band_override IS NULL ORDER BY id DESC LIMIT 1").fetchone()[0]
        server, _ = self.start(240)
        try:
            with W.connect('dev', 'owner', dbname=self.name) as conn:
                conn.execute("UPDATE live.npcs SET band_override = 'comfortable' WHERE id = %s", (npc,))
                action = conn.execute("INSERT INTO dm.actions (kind, target_id, requested_by, payload) VALUES ('npc.sync', %s, 'test', '{}') "
                                      'RETURNING id', (npc,)).fetchone()[0]
            with W.connect('dev', 'owner', dbname=self.name) as row_lock:
                with row_lock.transaction():
                    row_lock.execute('SELECT 1 FROM dm.actions WHERE id = %s FOR UPDATE', (action,))
                    with W.connect('dev', 'owner', dbname=self.name) as conn:
                        conn.execute("SELECT pg_notify('ratw_dm', %s)", (json.dumps({'action': action}),))
                    deadline, found = time.time() + 30, False
                    while time.time() < deadline and not found:
                        with W.connect('dev', 'owner', dbname=self.name) as conn:
                            found = conn.execute("SELECT count(*) FROM game.journal WHERE record LIKE '%%\"dmActs\"%%' AND record LIKE %s",
                                                 (f'%"{action}"%',)).fetchone()[0] > 0
                        time.sleep(.25)
                    self.assertTrue(found, 'the act\'s record, with its dm.actions id, committed')
                    time.sleep(1)
                    server.kill()
                    server.communicate(timeout=60)
                    self.end_the_dead_servers_sessions()
            with W.connect('dev', 'owner', dbname=self.name) as conn:
                self.assertEqual(conn.execute('SELECT status FROM dm.actions WHERE id = %s', (action,)).fetchone()[0], 'queued')
        finally:
            if server.poll() is None:
                server.kill()
                server.communicate(timeout=60)
        server, _ = self.start(240)
        try:
            status, result = self.outcome(action, 30)
        finally:
            server.send_signal(signal.SIGTERM)
            out, _ = server.communicate(timeout=60)
        self.assertEqual(status, 'applied', result)
        self.assertIn(f'RATW_DM_ACTION npc.sync {npc}: already applied before the restart', out)

    def test_the_storykeepers_band_on_a_live_server(self):
        """Phase 9's second repair (a missing item): the Storykeeper's npc_band, through the operator bridge of a real
        server on this database, holds a resident's household to a band: its receipt, the report's band for a while."""
        import tempfile
        import uuid
        bridge = Path(tempfile.mkdtemp(prefix='ratw-s2stage-bridge-'))
        try:
            server, _ = self.start(120, ('--dm-directory', str(bridge)))
            try:
                snapshot = bridge / 'snapshot.json'
                deadline, npc = time.time() + 30, None
                while time.time() < deadline and not npc:   # (Once the first bands are worked out.)
                    try:
                        view = json.loads(snapshot.read_text())
                        npc = next((c['id'] for c in view['characters'] if c.get('kind') == 'staged' and c.get('band')), None)
                    except (OSError, ValueError):
                        pass
                    time.sleep(.25)
                self.assertTrue(npc, 'a resident with a band in the bridge\'s snapshot')
                now = int(time.time())
                rid = uuid.uuid4().hex
                request = {'version': 1, 'id': rid, 'worldId': view['worldId'], 'createdAtUnix': now, 'expiresAtUnix': now + 120,
                           'kind': 'npc_band', 'payload': {'npc': npc, 'band': 'rich', 'days': 3}}
                (bridge / 'outbox' / f'{rid}.tmp').write_text(json.dumps(request))
                (bridge / 'outbox' / f'{rid}.tmp').rename(bridge / 'outbox' / f'{rid}.json')
                reply_at = bridge / 'inbox' / f'{rid}.json'
                deadline = time.time() + 30
                while time.time() < deadline and not reply_at.exists():
                    time.sleep(.25)
                reply = json.loads(reply_at.read_text())
                self.assertTrue(reply['ok'], reply)
                self.assertIn('Band rich for', reply['detail'])
                held = []
                deadline = time.time() + 60
                while time.time() < deadline and not held:
                    with W.connect('dev', 'owner', dbname=self.name) as conn:
                        row = conn.execute("SELECT payload::jsonb->'society'->'report'->'bandOverrides' FROM game.checkpoints").fetchone()
                    held = [o for o in (row[0] if row and row[0] else []) if npc in (o.get('members') or []) and o.get('band') == 'rich']
                    time.sleep(.5)
                self.assertTrue(held, 'the report holds the household rich for a while')
            finally:
                server.send_signal(signal.SIGTERM)
                server.communicate(timeout=60)
        finally:
            shutil.rmtree(bridge, ignore_errors=True)

    def test_economy_sync_reads_the_economy_alone(self):
        """Phase 9's second repair: economy.sync reads live.economy_manifest (the economy's records), never the whole world
        again (on Ridgemere 2.0 that stopped the game for seconds on each slider move): with live.people_manifest out of
        the way, a slider move still reaches the running server."""
        server, _ = self.start(120)
        try:
            with W.connect('dev', 'owner', dbname=self.name) as conn:
                conn.execute('ALTER FUNCTION live.people_manifest(ratw_id, text) RENAME TO people_manifest_away')
            try:
                with W.connect('dev', 'owner', dbname=self.name) as conn:
                    S.patch_economy_rules(conn, 'greyfen', {'prosperity': 40, 'prosperityCause': 'the silver strike'})
                sync = self.queue('economy.sync', 'economy', {'now': True})
                status, result = self.outcome(sync, 30)
                self.assertEqual(status, 'applied', result)
                deadline = time.time() + 60          # (The report reaches the checkpoint with the next snapshot.)
                report = {}
                while time.time() < deadline:
                    with W.connect('dev', 'owner', dbname=self.name) as conn:
                        row = conn.execute("SELECT payload::jsonb->'society'->'report'->'prosperity' FROM game.checkpoints").fetchone()
                    report = (row[0] if row else None) or {}
                    if report.get('target') == 40:
                        break
                    time.sleep(.5)
            finally:
                with W.connect('dev', 'owner', dbname=self.name) as conn:
                    conn.execute('ALTER FUNCTION live.people_manifest_away(ratw_id, text) RENAME TO people_manifest')
                    S.patch_economy_rules(conn, 'greyfen', drop=('prosperity', 'prosperityCause'))
            self.assertEqual((report.get('target'), report.get('cause')), (40, 'the silver strike'), 'the slider in force: ' + json.dumps(report))
        finally:
            server.send_signal(signal.SIGTERM)
            server.communicate(timeout=60)

    def test_the_acts_survive_a_crash(self):
        """Doc 79, Phase 9, integrated: every path from the studio and the DM tool to the running server, then a crash.
        The studio's steer (the slider with its cause, economy.sync {now}), the Band picker (save_npc: band_override,
        npc.sync), and the DM tool's live acts (a Times nudge, a harvest called, an event added and another cancelled,
        a household's band for a while, prices reset everywhere, a player's grant and fine), each applied by the server
        and journalled; the server is then killed before any checkpoint (SIGKILL: nothing saved), and the next one,
        restored from the checkpoint and the journal, holds every change exactly once."""
        import dungeon_master as D
        import studio
        dm = D.DungeonMaster(lambda target: W.connect('dev', 'dm', dbname=self.name), clock=time.time)
        who = {'username': 'test', 'role': 'dm'}
        economy = studio.Economy(connect=lambda key: W.connect('dev', 'editor', dbname=self.name),
                                 connect_dm=lambda key: W.connect('dev', 'dm', dbname=self.name))
        # A first server's save: the checkpoint the crash goes back to, with the report naming the targets.
        server, _ = self.start()
        server.send_signal(signal.SIGTERM)
        server.communicate(timeout=60)
        report = dm.prosperity('dev')['report']
        market = report['markets'][0]['id']
        area = next(a['id'] for a in report['areas'] if not a['override'])
        kind = next(k['id'] for k in report['eventKinds'] if not k['needs'])
        with W.connect('dev', 'owner', dbname=self.name) as conn:
            npcs = [r[0] for r in conn.execute('SELECT id FROM live.npcs ORDER BY id').fetchall()]
        server, port = self.start()
        player = None
        try:
            player = subprocess.Popen([NODE, '--experimental-strip-types', '--no-warnings', str(ROOT / 'tools' / 'client' / 'hold.ts'),
                                       '--port', str(port), '--identity', 'bea', '--seconds', '60'],
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=ROOT)
            pc = None
            for line in player.stdout:
                try:
                    pc = json.loads(line).get('in') or pc
                except ValueError:
                    continue
                if pc:
                    break
            self.assertTrue(pc, 'the player came in')
            # The server takes a checkpoint every 30 s: the acts go in just after one, so the crash comes before the next.
            snapshot = lambda: self.checkpoint()[0]
            was, deadline = snapshot(), time.time() + 45
            while snapshot() == was and time.time() < deadline:
                time.sleep(.2)
            taken = snapshot()
            self.assertNotEqual(taken, was, 'the server took a checkpoint')
            started = time.time()
            # Everything queued at once (the server applies a poll's rows in order), so the crash comes well before the
            # next checkpoint. The studio's slider: clamped (-43 is -45), its cause, and the times at once.
            steered = economy.set_steer('dev', {'prosperity': -43, 'cause': 'the long frost', 'now': True}, 'test')
            self.assertEqual(steered['after'], {'prosperity': -45, 'cause': 'the long frost'})
            queued = {'steer': economy.queue_sync('dev', 'test', {'now': True})}
            # The Band picker (Atlas's Inspector and the DM's NPC tab save person.band): band_override, then npc.sync.
            with W.connect('dev', 'editor', dbname=self.name) as conn:
                person = next(p for p in S.load_world(conn, 'greyfen')[0]['people'] if p['id'] == npcs[0])
            queued['band picker'] = dm.save_npc(who, 'dev', {**person, 'band': 'rich'})['action']
            acts = {'nudge': ('times.nudge', market, {'points': -60, 'days': 9, 'words': 'the bridge is out'}),
                    'harvest': ('harvest.override', area, {'word': 'good', 'days': 20, 'words': 'a kind summer'}),
                    'event 1': ('event.add', market, {'kind': kind, 'day': 3, 'hour': 10}),
                    'event 2': ('event.add', market, {'kind': kind, 'day': 5, 'hour': 12}),
                    'band': ('band.override', npcs[1], {'band': 'poor', 'days': 14}),
                    'reset': ('prices.reset', 'all', {'words': 'fresh stock at last'})}
            for what, (kind_, target, payload) in acts.items():
                queued[what] = dm.act(who, 'dev', kind_, target, payload)['id']
            queued['grant'] = dm.act(who, 'dev', 'character.grant', pc, {'coins': 23, 'reason': 'for finding the lost ewe'})['id']
            queued['fine'] = dm.act(who, 'dev', 'character.grant', pc, {'coins': -20, 'reason': 'for the broken fence'})['id']
            queued['over the cap'] = dm.act(who, 'dev', 'character.grant', pc, {'coins': 2000, 'reason': 'too much at once'})['id']
            applied = {what: self.outcome(action) for what, action in queued.items()}
            first = applied['event 1'][1].split(':')[0]
            applied['event.cancel'] = self.outcome(dm.act(who, 'dev', 'event.cancel', first, {})['id'])
            for what, (status, result) in applied.items():
                self.assertEqual(status, 'refused' if what == 'over the cap' else 'applied', f'{what}: {result}')
            # Every act journalled after the checkpoint, then the crash: nothing saved.
            deadline = time.time() + 10
            while time.time() < deadline:
                at, seq = self.checkpoint()
                with W.connect('dev', 'owner', dbname=self.name) as conn:
                    journalled = conn.execute('SELECT count(*) FROM game.journal WHERE seq > %s', (seq,)).fetchone()[0]
                if journalled >= 10:
                    break
                time.sleep(.25)
            self.assertEqual(at, taken, f'no checkpoint since the acts began ({time.time() - started:.1f} s ago): the crash '
                                        'test needs them in the journal alone')
            self.assertGreaterEqual(journalled, 10, 'each act journalled')
        finally:
            if player and player.poll() is None:
                player.kill()
            if player:
                player.wait(timeout=10)
                player.stdout.close()
            server.kill()                                # (SIGKILL: no save, the journal is all there is.)
            server.communicate(timeout=60)
        with W.connect('dev', 'owner', dbname=self.name) as conn:
            saved = conn.execute('SELECT saved_at, payload::jsonb->\'society\'->\'report\'->\'prosperity\' FROM game.checkpoints').fetchone()
        self.assertEqual((saved[0], saved[1]['target']), (taken, 0), 'the checkpoint is from before the acts: nothing was saved')
        # The next server: restored from the checkpoint and the journal, then saved as it stops.
        server, _ = self.start()
        time.sleep(2)
        server.send_signal(signal.SIGTERM)
        out, _ = server.communicate(timeout=60)
        view = dm.prosperity('dev')
        report = view['report']
        self.assertEqual((report['prosperity']['slider'], report['prosperity']['target'], report['prosperity']['cause'],
                          report['prosperity']['by']), (-45, -45, 'the long frost', 'dm'), 'the slider kept across the crash')
        m = next(x for x in report['markets'] if x['id'] == market)
        self.assertEqual((m['nudge']['points'], m['nudge']['words']), (-60, 'the bridge is out'), 'the nudge kept')
        self.assertTrue(m['resetting'], 'the prices still easing back')
        a = next(x for x in report['areas'] if x['id'] == area)
        self.assertEqual((a['harvest'], a['override']['word'], a['override']['words']), ('good', 'good', 'a kind summer'))
        events = [e for e in view['week']['events'] if e['by'] != 'seeded']
        self.assertEqual(sorted((e['day'], e['state']) for e in events), [(3, 'cancelled'), (5, 'due')],
                         'one event added and kept, one cancelled: each once: ' + json.dumps(events))
        self.assertGreaterEqual(sum(x['overridden'] for x in report['markets']), 1, 'the household held poor')
        holder = next((h for h in view['players']['holders'] if pc in (h['holder'], h['name']) or h['holder'].endswith(pc)), None)
        self.assertIsNotNone(holder, json.dumps(view['players']))
        self.assertEqual((holder['byReason'].get('dm grant: for finding the lost ewe'), holder['byReason'].get('dm fine: for the broken fence')),
                         ([23, 0, 0, 0], [0, 20, 0, 0]), 'granted 23 and fined 20, once each (the one over the cap refused): ' + json.dumps(holder))
        with W.connect('dev', 'owner', dbname=self.name) as conn:
            band = conn.execute('SELECT band_override FROM live.npcs WHERE id = %s', (npcs[0],)).fetchone()[0]
        self.assertEqual(band, 'rich', 'the Band picker\'s band stored')
        logged = [e['kind'] for e in view['log']]
        for kind_ in ('times.nudge', 'harvest.override', 'event.add', 'event.cancel', 'band.override', 'prices.reset',
                      'character.grant'):
            self.assertIn(kind_, logged, f'{kind_} in the Prosperity log')

    def test_the_hosts_overrides_reach_the_server(self):
        """Doc 79, 12 and 14 (Phase 9), integrated: the studio's Market, Status and Imports panels write the host's
        overrides into live.economy.rules; economy.sync has the running server read them through the manifest's
        `overrides` line and layer them over market.json and status.json (the report's event kinds are the host's); a
        hand-made status override that fails the loader's checks (shares summing to 140) and an unknown never-imported
        good are skipped with a warning, never refused, and the rest stays in force."""
        import dungeon_master as D
        import studio
        dm = D.DungeonMaster(lambda target: W.connect('dev', 'dm', dbname=self.name), clock=time.time)
        economy = studio.Economy(connect=lambda key: W.connect('dev', 'editor', dbname=self.name),
                                 connect_dm=lambda key: W.connect('dev', 'dm', dbname=self.name))
        self.assertTrue(economy.read('dev')['slim'], 'the throwaway world is at 0061')
        server, _ = self.start()
        try:
            economy.set_market('dev', {'market': {'events': {'perWeek': 1, 'list': ['works_fire', 'good_catch']}}}, 'test')
            economy.set_imports('dev', {'neverImported': ['salt']}, 'test')
            economy.set_status('dev', {'status': {'leanDays': 4}}, 'test')
            economy.set_rules('dev', {'set': {'neverHungry': 1}}, 'test')          # (The Rules panel's, a clamp rule.)
            status, result = self.outcome(economy.queue_sync('dev', 'test'))
            self.assertEqual(status, 'applied', result)
            # A host's hand edit past the studio: shares that sum to 140 and a good that is no good.
            with W.connect('dev', 'editor', dbname=self.name) as conn:
                S.patch_economy_rules(conn, 'greyfen', {'status': {'leanDays': 4, 'shares': [
                    {'at': -100, 'shares': [40, 40, 40, 20]}, {'at': 100, 'shares': [10, 30, 60, 40]}]},
                    'neverImported': ['salt', 'unobtainium']}, ())
            status, result = self.outcome(economy.queue_sync('dev', 'test'))
            self.assertEqual(status, 'applied', 'skipped, never refused: ' + result)
        finally:
            server.send_signal(signal.SIGTERM)
            out, _ = server.communicate(timeout=60)
        warned = [l for l in out.splitlines() if 'RATW_ECONOMY override' in l]
        self.assertTrue(any('shares' in l for l in warned), 'the bad shares warned of: ' + '\n'.join(warned))
        self.assertTrue(any('unobtainium' in l for l in warned), 'the unknown good warned of: ' + '\n'.join(warned))
        report = dm.prosperity('dev')['report']
        self.assertEqual(sorted(k['id'] for k in report['eventKinds']), ['good_catch', 'works_fire'],
                         'the host\'s event kinds in force, in place of market.json\'s twelve')
        self.assertTrue(report['prosperity']['neverHungry'], 'the Rules panel\'s Never Hungry in force')
        with W.connect('dev', 'editor', dbname=self.name) as conn:      # (Back to the files', for the other tests.)
            S.patch_economy_rules(conn, 'greyfen', {}, ['status', 'market', 'neverImported', 'neverHungry'])

    def test_the_slider_and_an_act_end_to_end(self):
        """Doc 79, 12 (Phase 9): the slider written into live.economy's rules with its cause, economy.sync {now} applied by
        the running server, and a DM act (prices reset everywhere) queued through the DM tool; once the server has saved,
        the Prosperity view's report holds both."""
        import dungeon_master as D
        try:                                            # (The core lane's world_store, C8: the studio's steer writes through it.)
            with W.connect('dev', 'editor', dbname=self.name) as conn:
                S.patch_economy_rules(conn, 'greyfen', {}, ())
        except NotImplementedError as missing:
            self.skipTest(f'Phase 9\'s core lane is not merged yet: {missing}')
        dm = D.DungeonMaster(lambda target: W.connect('dev', 'dm', dbname=self.name), clock=time.time)
        who = {'username': 'test', 'role': 'dm'}
        port = free_port()
        server = subprocess.Popen([str(SERVER), '--database', 'dev', '--port', str(port), '--for', '120', '--dev-identity'],
                                  env={**os.environ, 'RATW_DATABASE_URL': self.url, 'RATW_AI': 'off'},
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            for line in server.stdout:
                if 'listening on port' in line:
                    break
            # The slider, as the studio's steer writes it (one statement over the rules), then its economy.sync.
            with W.connect('dev', 'editor', dbname=self.name) as conn:
                S.patch_economy_rules(conn, 'greyfen', {'prosperity': -40, 'prosperityCause': 'the long frost'}, ())
            synced = self.queue('economy.sync', 'economy', {'now': True})
            status, result = self.outcome(synced)
            self.assertEqual(status, 'applied', result)
            reset = dm.act(who, 'dev', 'prices.reset', 'all', {'words': 'fresh stock at last'})
            self.assertEqual(reset['payload'], {'words': 'fresh stock at last'})
            status, result = self.outcome(reset['id'])
            self.assertEqual(status, 'applied', result)
        finally:
            server.send_signal(signal.SIGTERM)          # (It saves as it stops: the checkpoint carries the report.)
            server.communicate(timeout=60)
        view = dm.prosperity('dev')
        self.assertIsNotNone(view['report'], 'the server wrote its report with the checkpoint')
        prosperity = view['report']['prosperity']
        self.assertEqual((prosperity['target'], prosperity['slider']), (-40, -40), 'now: the slider at its target at once')
        self.assertEqual((prosperity['cause'], prosperity['by']), ('the long frost', 'dm'))
        self.assertTrue(all(m['resetting'] for m in view['report']['markets']), 'every market resetting its prices')
        self.assertIn('prices.reset', [e['kind'] for e in view['log']], 'the act in the log')


if __name__ == '__main__':
    unittest.main()
