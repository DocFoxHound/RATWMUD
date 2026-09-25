#!/usr/bin/env python3
"""Real native account/creator flow, restart, two-client inspection and ownership probes.

Uses disposable test accounts/save. No dev-identity bypass and no credentials in
process arguments. --headless skips screenshots, not any authority assertions.
"""
import argparse
import errno
import json
import os
from pathlib import Path
import socket
import sqlite3
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--packaged', action='store_true')
    parser.add_argument('--headless', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    tests = root / 'Saved/Tests'
    tests.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='characters-', dir=tests))
    evidence = root / 'artifacts' / ('packaged-evidence' if args.packaged else 'screenshots')
    evidence.mkdir(parents=True, exist_ok=True)
    logs = root / 'artifacts/logs'
    logs.mkdir(parents=True, exist_ok=True)
    engine = Path(os.environ.get('RATW_UNREAL_ROOT', '/home/martinb/Applications/UnrealEngine/5.8.2'))
    base = ([str(root / 'artifacts/package/Linux/RATWMUD/Binaries/Linux/RATWMUD')]
            if args.packaged else [str(engine / 'Engine/Binaries/Linux/UnrealEditor'), str(root / 'RATWMUD.uproject')])
    kind = 'packaged' if args.packaged else 'native'
    common = ['-NoSound', '-NoSplash', '-Unattended', '-ForceLogFlush', '-noscreenmessages', '-NoSteam']
    save = run / 'world.sqlite'
    children, handles = [], []

    def start(label, flags):
        handle = (logs / f'characters-{kind}-{label}.log').open('w')
        handles.append(handle)
        child = subprocess.Popen(base + flags + common, cwd=root, stdout=handle, stderr=subprocess.STDOUT)
        children.append(child)
        return child

    def client(scenario, role='ash', address='/Engine/Maps/Entry', capture=False):
        flags = [address, '-game', f'-RatwScenario={scenario}', f'-RatwIdentity={role}',
                 f'-RatwSave={save}', f'-RatwCaptureDir={evidence}']
        flags += (['-windowed', '-ResX=1600', '-ResY=1000', '-ForceRes', '-RenderOffscreen', '-RatwCaptureCharacters']
                  if capture and not args.headless else ['-nullrhi'])
        return start(f'{scenario}-{role}', flags), time.time_ns()

    def result(child, stamp, scenario, role='ash'):
        code = child.wait(timeout=130)
        path = evidence / f'{scenario}-{role}.json'
        if not path.exists() or path.stat().st_mtime_ns < stamp:
            raise RuntimeError(f'Missing fresh {scenario}/{role} evidence; inspect characters-{kind} logs')
        report = json.loads(path.read_text())
        if code or not report['passed']:
            raise RuntimeError(f'{scenario}/{role} failed at {report.get("step")}: {report["detail"]}')
        print('PASS: ' + report['detail'], flush=True)
        return report

    try:
        process, stamp = client('characters-create', capture=True)
        created = result(process, stamp, 'characters-create')
        if not args.headless:
            for filename in ['33-character-login.png', '34-character-creator.png', '35-character-roster.png',
                             '36-player-character-card.png', '37-old-arctic-preview.png', '38-maned-wolf-preview.png']:
                shot = evidence / filename
                assert shot.exists() and shot.stat().st_mtime_ns >= stamp and shot.stat().st_size > 1024, filename
        with sqlite3.connect(save) as db:
            raw = db.execute('SELECT payload FROM world_state WHERE id=1').fetchone()[0]
            state = json.loads(raw)
            assert 'RATW-fixture-only-2026!' not in raw and 'Incorrect-fixture-2026!' not in raw
            assert len(state['players']) == 7, 'Six characters on A and one on B must survive checkpoint'
            assert state.get('accounts'), 'Private account store was not checkpointed'
        assert save.stat().st_mode & 0o077 == 0, 'Credential verifier store is not owner-only'
        print('PASS: atomic private checkpoint retains seven characters and no plaintext passwords', flush=True)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
            reservation.bind(('127.0.0.1', 0)); port = reservation.getsockname()[1]
        host = ['/Engine/Maps/Entry?listen', '-RatwHeadlessHost'] if args.packaged else ['/Engine/Maps/Entry', '-server']
        server = start('server', host + ['-nullrhi', '-MULTIHOME=127.0.0.1', f'-port={port}', f'-RatwSave={save}'])
        deadline = time.monotonic() + 75
        while time.monotonic() < deadline:
            if server.poll() is not None: raise RuntimeError('Authority exited before listening')
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
                try: probe.bind(('127.0.0.1', port))
                except OSError as error:
                    if error.errno == errno.EADDRINUSE: break
                    raise
            time.sleep(.2)
        else: raise RuntimeError('Authority did not listen')
        address = f'127.0.0.1:{port}'
        holder, hold_stamp = client('characters-hold', address=address)
        # Wait for persisted login/entry before attempting duplicate session.
        time.sleep(5)
        peer, peer_stamp = client('characters-inspect', role='birch', address=address, capture=True)
        inspected = result(peer, peer_stamp, 'characters-inspect', 'birch')
        duplicate, duplicate_stamp = client('characters-duplicate', address=address)
        result(duplicate, duplicate_stamp, 'characters-duplicate')
        result(holder, hold_stamp, 'characters-hold')
        if not args.headless:
            shot = evidence / '39-other-player-inspection.png'
            assert shot.exists() and shot.stat().st_mtime_ns >= peer_stamp and shot.stat().st_size > 1024
        report = {'passed': True, 'kind': kind, 'isolatedSave': str(save),
                  'creationChecks': created['checks'], 'networkChecks': inspected['checks'],
                  'noDevelopmentIdentityBypass': True, 'plaintextPasswordStored': False}
        (evidence / f'characters-{kind}-summary.json').write_text(json.dumps(report, indent=2))
        return 0
    finally:
        for child in reversed(children):
            if child.poll() is None:
                child.terminate()
                try: child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill(); child.wait(timeout=10)
        for handle in handles: handle.close()


if __name__ == '__main__':
    raise SystemExit(main())
