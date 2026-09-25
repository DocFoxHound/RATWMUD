#!/usr/bin/env python3
"""Real Unreal client finite-trade/calendar scenario and separate-process restart."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true')
    parser.add_argument('--packaged', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    tests = root / 'Saved/Tests'
    tests.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='society-', dir=tests))
    evidence = root / 'artifacts' / ('packaged-evidence' if args.packaged else 'screenshots')
    evidence.mkdir(parents=True, exist_ok=True)
    logs = root / 'artifacts/logs'
    logs.mkdir(parents=True, exist_ok=True)
    engine = Path(os.environ.get('RATW_UNREAL_ROOT', '/home/martinb/Applications/UnrealEngine/5.8.2'))
    base = ([str(root / 'artifacts/package/Linux/RATWMUD/Binaries/Linux/RATWMUD')]
            if args.packaged else [str(engine / 'Engine/Binaries/Linux/UnrealEditor'), str(root / 'RATWMUD.uproject')])
    for scenario in ('society', 'society-restore'):
        capture = scenario == 'society' and not args.headless
        command = base + ['/Engine/Maps/Entry', '-game', '-NoSplash', '-NoSound', '-Unattended', '-noscreenmessages',
                          '-ForceLogFlush', '-RatwIdentity=ash', '-RatwName=Ash', '-RatwDevTools', '-RatwDevIdentity', f'-RatwScenario={scenario}',
                          f'-RatwSave={run / "world.sqlite"}', f'-RatwCaptureDir={evidence}']
        command += (['-windowed', '-ResX=1600', '-ResY=1000', '-ForceRes', '-RenderOffscreen', '-RatwCaptureSociety']
                    if capture else ['-nullrhi'])
        kind = 'packaged' if args.packaged else 'native'
        log = logs / f'{scenario}-{kind}-smoke.log'
        started = time.time_ns()
        child = None
        try:
            with log.open('w') as output:
                child = subprocess.Popen(command, cwd=root, stdout=output, stderr=subprocess.STDOUT)
                code = child.wait(timeout=170)
            result = evidence / f'{scenario}-ash.json'
            if not result.exists() or result.stat().st_mtime_ns < started:
                raise RuntimeError(f'No fresh {scenario} evidence. See {log}')
            report = json.loads(result.read_text())
            if code or not report['passed']:
                raise RuntimeError(f'{scenario} failed: {report["detail"]}. See {log}')
            if capture:
                for name in ('29-finite-merchant.png', '30-birthday-character.png', '31-nightly-rest.png', '32-birthday-notice.png'):
                    shot = evidence / name
                    if not shot.exists() or shot.stat().st_mtime_ns < started or shot.stat().st_size < 1024:
                        raise RuntimeError(f'No fresh screenshot: {name}')
            print('PASS: ' + report['detail'], flush=True)
        finally:
            if child and child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait(timeout=10)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
