#!/usr/bin/env python3
"""Three real client connections verify aging while logged out of a live authority."""
import argparse
import errno
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--packaged', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    tests = root / 'Saved/Tests'
    tests.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='offline-age-', dir=tests))
    evidence = root / 'artifacts' / ('packaged-evidence' if args.packaged else 'screenshots')
    evidence.mkdir(parents=True, exist_ok=True)
    logs = root / 'artifacts/logs'
    logs.mkdir(parents=True, exist_ok=True)
    engine = Path(os.environ.get('RATW_UNREAL_ROOT', '/home/martinb/Applications/UnrealEngine/5.8.2'))
    base = ([str(root / 'artifacts/package/Linux/RATWMUD/Binaries/Linux/RATWMUD')]
            if args.packaged else [str(engine / 'Engine/Binaries/Linux/UnrealEditor'), str(root / 'RATWMUD.uproject')])
    common = ['-nullrhi', '-NoSound', '-NoSplash', '-Unattended', '-ForceLogFlush', '-RatwDevIdentity']
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    kind = 'packaged' if args.packaged else 'native'
    server_log = logs / f'offline-age-{kind}-server.log'
    server = client = None
    try:
        with server_log.open('w') as output:
            host = ['/Engine/Maps/Entry?listen', '-RatwHeadlessHost'] if args.packaged else ['/Engine/Maps/Entry', '-server']
            server = subprocess.Popen(base + host + common + ['-RatwDevTools', '-MULTIHOME=127.0.0.1',
                                      f'-port={port}', f'-RatwSave={run / "world.sqlite"}'], cwd=root,
                                      stdout=output, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline:
                if server.poll() is not None:
                    raise RuntimeError(f'Authority exited. See {server_log}')
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
                    try:
                        probe.bind(('127.0.0.1', port))
                    except OSError as error:
                        if error.errno == errno.EADDRINUSE:
                            break
                        raise
                time.sleep(.25)
            else:
                raise RuntimeError('Authority did not begin listening')
            for identity, scenario in [('ash', 'age-register'), ('birch', 'age-advance'), ('ash', 'age-return')]:
                log = logs / f'{scenario}-{kind}.log'
                started = time.time_ns()
                with log.open('w') as client_output:
                    client = subprocess.Popen(base + [f'127.0.0.1:{port}', '-game'] + common +
                              [f'-RatwIdentity={identity}', f'-RatwName={identity.title()}', f'-RatwScenario={scenario}',
                               f'-RatwCaptureDir={evidence}'], cwd=root, stdout=client_output, stderr=subprocess.STDOUT)
                    code = client.wait(timeout=150)
                result = evidence / f'{scenario}-{identity}.json'
                if not result.exists() or result.stat().st_mtime_ns < started:
                    raise RuntimeError(f'No fresh {scenario} evidence. See {log}')
                report = json.loads(result.read_text())
                if code or not report['passed']:
                    raise RuntimeError(f'{scenario}: {report["detail"]}. See {log}')
                print('PASS: ' + report['detail'], flush=True)
    finally:
        for process in (client, server):
            if process and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=10)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
