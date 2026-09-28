#!/usr/bin/env python3
"""An isolated game server, a scripted browser-client player and the Storykeeper's HTTP service, together.

Needs build-core/ratw_server and the browser client (Client/). No production save, player account, dialogue provider, or external service is used.
The private session token is read in memory and never printed or copied to evidence.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import errno
import http.client
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time
from urllib.parse import parse_qs, urlsplit
import uuid

import dm_service
from game_run import ensure_client, ensure_server, start_scenario


class Smoke:
    def __init__(self, restart=True):
        self.root = Path(__file__).resolve().parent.parent
        tests = self.root / 'Saved/Tests'
        tests.mkdir(parents=True, exist_ok=True)
        self.run = Path(tempfile.mkdtemp(prefix='dm-integration-', dir=tests))
        self.evidence = self.run / 'evidence'
        self.evidence.mkdir(mode=0o700)
        self.exchange = self.run / 'bridge'
        self.state_dir = self.run / 'storykeeper'
        self.logs = self.root / 'artifacts/logs'
        self.logs.mkdir(parents=True, exist_ok=True)
        self.kind = 'native'
        self.result_path = self.logs / 'dm-native-smoke-result.json'
        self.restart = restart
        self.game_port = self.port(socket.SOCK_STREAM)
        self.http_port = self.port(socket.SOCK_STREAM)
        self.origin = f'http://127.0.0.1:{self.http_port}'
        self.server = self.client = self.service = None
        self.streams = []
        self.token = ''
        self.checks = []
        self.started = time.time()
        self.client_started_ns = 0
        self.report = {'passed': False, 'mode': self.kind, 'runDirectory': str(self.run), 'checks': self.checks}

    @staticmethod
    def port(kind):
        with socket.socket(socket.AF_INET, kind) as reservation:
            reservation.bind(('127.0.0.1', 0))
            return reservation.getsockname()[1]

    def check(self, condition, detail):
        if not condition:
            raise RuntimeError(detail)
        self.checks.append(detail)
        print('PASS: ' + detail, flush=True)

    def process(self, command, label, env=None):
        log = self.logs / f'dm-{self.kind}-{label}.log'
        stream = log.open('w')
        self.streams.append(stream)
        return subprocess.Popen(command, cwd=self.root, stdout=stream, stderr=subprocess.STDOUT, env=env)

    def scratch_database(self):
        """The Storykeeper keeps its state in PostgreSQL; this run gets its own database, dropped afterwards."""
        import world_db
        from test_world_db import superuser
        self.database = f'ratw_dm_smoke_{os.getpid()}'
        with superuser() as su:
            su.execute(f'DROP DATABASE IF EXISTS {self.database} WITH (FORCE)')
            su.execute(f'CREATE DATABASE {self.database} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {self.database} TO ratw_game')
        with world_db.connect('dev', 'owner', dbname=self.database) as owner:
            world_db.migrate(owner)

    def drop_database(self):
        from test_world_db import superuser
        if getattr(self, 'database', None):
            with superuser() as su:
                su.execute(f'DROP DATABASE IF EXISTS {self.database} WITH (FORCE)')

    @staticmethod
    def stop(process):
        if process and process.poll() is None:
            # Always resume a test-owned process before requesting shutdown.
            if hasattr(signal, 'SIGCONT'):
                process.send_signal(signal.SIGCONT)
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=10)

    def running(self):
        for label, child in [('authority', self.server), ('Storykeeper', self.service)]:
            if child and child.poll() is not None:
                raise RuntimeError(f'{label} exited with status {child.returncode}; inspect dm-{self.kind} logs.')
        if self.client and self.client.poll() not in (None, 0):
            raise RuntimeError(f'Observer exited with status {self.client.returncode}; inspect dm-{self.kind}-observer.log.')

    def wait(self, predicate, detail, timeout=30):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.running()
            value = predicate()
            if value:
                return value
            time.sleep(.15)
        raise RuntimeError(f'Timed out after {timeout}s: {detail}')

    def start_server(self, label='server'):
        started_ns = time.time_ns()
        self.server = self.process([str(ensure_server()), '--save', str(self.run / 'world.json'), '--port', str(self.game_port),
                                    '--web', str(ensure_client()), '--dev-identity', '--dm-directory', str(self.exchange)], label)

        def ready():
            path = self.exchange / 'snapshot.json'
            if not path.exists() or path.stat().st_mtime_ns < started_ns:
                return False
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
                try:
                    probe.bind(('127.0.0.1', self.game_port))
                except OSError as error:
                    if error.errno == errno.EADDRINUSE:
                        return True
                    raise
            return False

        self.wait(ready, 'native authority to listen and publish a fresh private snapshot', 90)
        self.check(True, 'Native authority publishes its separate owner-private operator snapshot.')

    def start_service(self):
        if not getattr(self, 'database', None):
            self.scratch_database()
        self.service = self.process([sys.executable, str(self.root / 'tools/dm_service.py'), '--exchange', str(self.exchange),
                                     '--state-dir', str(self.state_dir), '--port', str(self.http_port)], 'http',
                                    env={**os.environ, 'RATW_DEV_DBNAME': self.database})
        session = self.state_dir / 'session.json'
        self.wait(session.exists, 'Storykeeper private session file', 20)
        parts = urlsplit(json.loads(session.read_text())['url'])
        self.token = parse_qs(parts.fragment, strict_parsing=True)['token'][0]
        if f'{parts.scheme}://{parts.netloc}' != self.origin:
            raise RuntimeError('Storykeeper session points at an unexpected host/port.')

    def api(self, method, path, body=None, *, authenticated=True, origin=None, expected=200):
        headers = {'Origin': self.origin if origin is None else origin}
        if authenticated:
            headers['Authorization'] = 'Bearer ' + self.token
        data = None
        if body is not None:
            data = dm_service.encode(body).encode()
            headers['Content-Type'] = 'application/json'
        connection = http.client.HTTPConnection('127.0.0.1', self.http_port, timeout=10)
        try:
            connection.request(method, path, body=data, headers=headers)
            response = connection.getresponse()
            raw = response.read(9 * 1024 * 1024)
            result = json.loads(raw)
            if response.status != expected:
                raise RuntimeError(f'HTTP {method} {path}: expected {expected}, got {response.status}: {result.get("error", "unexpected response")}')
            if expected != 200 and result.get('ok') is not False:
                raise RuntimeError('Rejected API request did not return a failure response.')
            return result
        finally:
            connection.close()

    def state(self):
        return self.api('GET', '/api/state')

    def command(self, action, payload, *, identifier=None, expected=200, full=False):
        request = {'id': identifier or uuid.uuid4().hex, 'action': action, 'payload': payload}
        response = self.api('POST', '/api/command', request, expected=expected)
        if expected != 200:
            return response
        if response.get('ok') is not True:
            raise RuntimeError(f'{action} did not succeed.')
        return (request, response) if full else response['result']

    def event(self, kind, payload, title, **extra):
        return self.command('event.create', {'title': title, 'kind': kind, 'payload': payload, **extra})

    def event_applied(self, identifier, timeout=30):
        def applied():
            state = self.state()
            event = next((event for event in state['events'] if event['id'] == identifier), None)
            if not event:
                raise RuntimeError('Expected event disappeared from Storykeeper.')
            if event['status'] in ('failed', 'blocked', 'cancelled'):
                raise RuntimeError(f'Event {event["title"]} became {event["status"]}: {event["detail"]}')
            return (state, event) if event['status'] == 'applied' else None
        return self.wait(applied, f'authority confirmation for event {identifier}', timeout)

    @staticmethod
    def actor(state, identifier):
        return next((actor for actor in (state.get('snapshot') or {}).get('characters', []) if actor['id'] == identifier), None)

    def fresh_condition(self, predicate, detail, timeout=30):
        def inspect():
            state = self.state()
            return state if state['bridge']['live'] and predicate(state) else None
        return self.wait(inspect, detail, timeout)

    def replay_native(self, envelope, expected_receipt):
        path = self.exchange / 'outbox' / (envelope['id'] + '.json')
        dm_service.atomic_json(path, envelope)
        self.wait(lambda: not path.exists(), 'native replay receipt', 20)
        receipt = dm_service.decode(dm_service.read_file(self.exchange / 'inbox' / path.name, dm_service.MAX_BODY))
        self.check(receipt == expected_receipt, 'Native retry returns the original persisted receipt without a second effect.')
        # The inbox is published just before the snapshot. Require a later
        # whole-second snapshot instead of accidentally accepting stale cash.
        after_reply = int(time.time()) + 1
        state = self.fresh_condition(lambda state: state['snapshot']['generatedAtUnix'] >= after_reply and
                                     self.actor(state, 'player-ash') and self.actor(state, 'player-ash')['cash'] == 23,
                                     'replayed transfer still leaves exactly 23 pennies')
        entries = [entry for entry in state['snapshot']['economy']['ledger'] if entry['kind'] == 'operator transfer' and
                   entry['from'] == 'treasury' and entry['to'] == 'player-ash' and entry['coins'] == 3]
        self.check(len(entries) == 1, 'The authority ledger contains one 3-penny transfer, not duplicated retries.')

    def exercise(self):
        self.start_server()
        self.start_service()
        self.api('GET', '/api/state', authenticated=False, expected=401)
        self.api('GET', '/api/state', origin='https://untrusted.invalid', expected=403)
        self.api('POST', '/api/command', {'id': 'bad-origin', 'action': 'faction.upsert', 'payload': {'name': 'Must not exist'}},
                 origin='https://untrusted.invalid', expected=403)
        self.check(True, 'Operator HTTP reads/writes require bearer authorization and same-origin access.')
        state = self.fresh_condition(lambda state: len(state['snapshot']['cells']) == 3, 'live full-world operator state')
        self.check(set(state['snapshot']['capabilities']) == dm_service.NATIVE and
                   len([actor for actor in state['snapshot']['characters'] if actor['npc']]) == 6,
                   'Live schema exposes all three cells and six real NPCs through advertised native capabilities.')
        world_id = state['snapshot']['worldId']
        self.report['worldId'] = world_id

        self.client_started_ns = time.time_ns()
        self.client = start_scenario(self.game_port, 'dm-observer', 'ash', capture=self.evidence,
                                     log=self.logs / 'dm-native-observer.log')
        state = self.fresh_condition(lambda state: self.actor(state, 'player-ash') and self.actor(state, 'player-ash')['online'] and
                                     self.actor(state, 'player-ash')['active'], 'Ash connected and issued look', 45)
        self.check(self.actor(state, 'player-ash')['cash'] == 20, 'A connected player cannot forge the public operator transfer command.')

        campaign = self.command('campaign.upsert', {'id': 'smoke_campaign', 'name': 'Smoke: changing roads',
                                  'description': 'Isolated integration fixture, not a live campaign.'})
        beat = self.command('beat.upsert', {'campaignId': campaign['id'], 'title': 'A new hearth', 'kind': 'settlement'})
        faction = self.command('faction.upsert', {'id': 'smoke_wardens', 'name': 'Smoke Wardens'})
        chapter = self.command('chapter.upsert', {'id': 'smoke_hearth', 'name': 'Smoke Hearth', 'memberIds': ['player-ash'],
            'cellIds': ['exterior'], 'housing': 8, 'jobCapacity': 8, 'attraction': 80, 'treatyModifier': .25})
        self.command('opinion.set', {'factionId': faction['id'], 'targetType': 'chapter', 'targetId': chapter['id'],
            'score': -15, 'reason': 'Isolated diplomatic adjustment.'})
        self.command('opinion.set', {'factionId': faction['id'], 'targetType': 'player', 'targetId': 'player-ash',
            'score': 10, 'reason': 'Isolated individual standing.'})
        self.check(len(self.state()['opinions']) == 2, 'Campaigns, story beats, Chapters, factions and manual Chapter/player opinions persist through HTTP.')

        weather = self.event('weather', {'cell': 'exterior', 'preset': 'fog'}, 'Smoke fog', campaignId=campaign['id'], beatId=beat['id'])
        self.command('event.approve', {'eventId': weather['id']})
        self.event_applied(weather['id'])
        self.fresh_condition(lambda state: next(cell for cell in state['snapshot']['cells'] if cell['id'] == 'exterior')['weather'] == 'fog',
                             'actual authority weather changed to fog')
        self.check(True, 'An approved weather event changes the native world to fog, not just an operator draft.')

        transfer = self.event('economy_transfer', {'from': 'treasury', 'to': 'player-ash', 'item': '', 'quantity': 0, 'coins': 3},
                              'Smoke finite grant')
        # A sub-second pause of this isolated test-owned authority prevents it from
        # consuming the envelope before we capture its exact persisted replay input.
        self.server.send_signal(signal.SIGSTOP)
        try:
            request, response = self.command('event.approve', {'eventId': transfer['id']}, full=True)
            envelope_path = self.exchange / 'outbox' / (transfer['id'] + '.json')
            raw = dm_service.read_file(envelope_path, dm_service.MAX_BODY)
            envelope = dm_service.decode(raw)
            if dm_service.encode(envelope).encode() != raw:
                raise RuntimeError('Captured envelope cannot be replayed byte-for-byte canonically.')
        finally:
            self.server.send_signal(signal.SIGCONT)
        state, _ = self.event_applied(transfer['id'])
        state = self.fresh_condition(lambda state: self.actor(state, 'player-ash')['cash'] == 23, 'finite grant visible in native purse')
        treasury = next(account for account in state['snapshot']['accounts'] if account['id'] == 'treasury')
        self.check(treasury['cash'] == 977, 'Three pennies move from the finite treasury to Ash: 23 held, 977 remain.')
        self.check(self.api('POST', '/api/command', request) == response, 'Repeating the same authenticated command ID returns its original response.')
        self.command('event.approve', {'eventId': weather['id']}, identifier=request['id'], expected=409)
        original_receipt = dm_service.decode(dm_service.read_file(self.exchange / 'inbox' / envelope_path.name, dm_service.MAX_BODY))
        self.replay_native(envelope, original_receipt)

        war = self.event('war', {'targetChapter': chapter['id']}, 'Smoke unavailable army', campaignId=campaign['id'])
        self.check(war['status'] == 'blocked', 'Unavailable armies remain explicitly blocked story plans, not reported native effects.')
        self.command('event.approve', {'eventId': war['id']}, expected=409)

        due = int(time.time()) + 5
        timed = self.event('notice', {'scope': 'chapter', 'target': chapter['id'], 'text': 'DM_SMOKE_TIMED_NOTICE'},
                           'Smoke timed Chapter notice', scheduledAt=datetime.fromtimestamp(due, timezone.utc).isoformat().replace('+00:00', 'Z'))
        scheduled = self.command('event.approve', {'eventId': timed['id']})
        self.check(scheduled['status'] == 'scheduled' and not (self.exchange / 'outbox' / (timed['id'] + '.json')).exists(),
                   'A future Chapter notice remains scheduled instead of firing immediately.')
        _, timed_event = self.event_applied(timed['id'], 20)
        receipt = dm_service.decode(dm_service.read_file(self.exchange / 'inbox' / (timed['id'] + '.json'), dm_service.MAX_BODY))
        self.check(receipt['appliedAtUnix'] >= due and timed_event['status'] == 'applied', 'Scheduled Chapter notice executes at or after its fixed UTC time.')
        peak = self.command('chapter.peak', {'chapterId': chapter['id']})
        self.check(peak['suggestion'] and peak['suggestion']['observedMinutes'] > 0, 'Chapter peak-time advice derives from observed player activity, not invented history.')

        preview = self.command('migration.preview', {'chapterId': chapter['id']})
        self.check(preview['capacity']['available'] > 0 and bool(preview['candidates']), 'Same-region migration preview uses real residents and declared housing/job capacity.')
        state = self.state()
        candidates = sorted(preview['candidates'], key=lambda candidate: (self.actor(state, candidate['npcId'])['cell'] == candidate['destinationCell'],
                            candidate['npcId'] != 'npc_scout', candidate['npcId']))
        selected = candidates[0]
        self.check(all(candidate['npcId'] not in ('npc_keeper', 'npc_cook', 'npc_porter') for candidate in candidates),
                   'Migration preview protects essential merchant, cooking and gathering jobs.')
        source_home = self.actor(state, selected['npcId'])['homeCell']
        migration = self.command('migration.approve', {'previewId': preview['id'], 'npcId': selected['npcId']})
        self.check(migration['status'] == 'queued', 'Migration approval queues physical navigation rather than declaring instant arrival.')
        state, arrived = self.event_applied(migration['id'], 70)
        resident = self.actor(state, selected['npcId'])
        self.check(source_home != resident['homeCell'] and resident['homeCell'] == selected['destinationCell'] and not resident['relocating'] and
                   abs(resident['homeX'] - selected['x']) <= .3 and abs(resident['homeY'] - selected['y']) <= .3,
                   'Migration becomes applied only after the native home record confirms physical arrival.')
        self.report['migration'] = {'npc': selected['npcId'], 'from': source_home, 'to': resident['homeCell'], 'eventId': arrived['id']}

        final_notice = self.event('notice', {'scope': 'player', 'target': 'player-ash', 'text': 'DM_SMOKE_DONE'}, 'Smoke observer completion')
        self.command('event.approve', {'eventId': final_notice['id']})
        self.event_applied(final_notice['id'])
        code = self.client.wait(timeout=20)
        observer_path = self.evidence / 'dm-observer-ash.json'
        if not observer_path.exists() or observer_path.stat().st_mtime_ns < self.client_started_ns:
            raise RuntimeError('No fresh dm-observer evidence; inspect the client log.')
        observer = json.loads(observer_path.read_text())
        self.check(code == 0 and observer.get('passed') is True, 'Real client verifies 23 pennies and no omniscient director data in its player snapshots.')
        self.report['observer'] = {'passed': observer['passed'], 'detail': observer.get('detail', '')}

        if self.restart:
            self.stop(self.server)
            self.start_server('server-restart')
            state = self.fresh_condition(lambda state: state['snapshot']['worldId'] == world_id and self.actor(state, 'player-ash') and
                                         not self.actor(state, 'player-ash')['online'], 'same-world restart and saved offline player', 30)
            resident = self.actor(state, selected['npcId'])
            self.check(self.actor(state, 'player-ash')['cash'] == 23 and resident['homeCell'] == selected['destinationCell'] and not resident['relocating'] and
                       next(cell for cell in state['snapshot']['cells'] if cell['id'] == 'exterior')['weather'] == 'fog',
                       'Native restart preserves world identity, offline purse, relocated home and weather intervention.')
            self.replay_native(envelope, original_receipt)
        self.report['passed'] = True

    def execute(self):
        try:
            self.exercise()
        except Exception as error:
            self.report['error'] = str(error)
            raise
        finally:
            for process in (self.client, self.server, self.service):
                self.stop(process)
            self.drop_database()
            for stream in self.streams:
                stream.close()
            self.report['elapsedSeconds'] = round(time.time() - self.started, 3)
            self.report['checkCount'] = len(self.checks)
            self.result_path.write_text(json.dumps(self.report, indent=2) + '\n')
            print(f'Evidence: {self.result_path}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--no-restart', action='store_true', help='Skip the final separate-authority restart/replay check.')
    args = parser.parse_args()
    Smoke(restart=not args.no_restart).execute()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
