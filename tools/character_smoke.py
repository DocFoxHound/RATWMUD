#!/usr/bin/env python3
"""The account and creator flow in the browser client, a restart, two-client inspection and ownership probes.

Uses disposable test accounts and a disposable save. No development-identity bypass: the server runs without
--dev-identity, and no credentials appear in process arguments. --headless skips the browser and screenshots, not any
authority assertion.
"""
import argparse
import json
from pathlib import Path
import tempfile
import time

from game_run import ROOT, GameServer, evidence, fresh_screenshots, run_scenario, start_scenario


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--headless', action='store_true')
    args = parser.parse_args()
    tests = ROOT / 'Saved/Tests'
    tests.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='characters-', dir=tests))
    capture = ROOT / 'artifacts' / 'screenshots'
    logs = ROOT / 'artifacts' / 'logs'
    save = run / 'world.json'
    shots = not args.headless

    stamp = time.time_ns()
    with GameServer(save=save, dev_identity=False, log=logs / 'characters-server-1.log') as server:
        created = run_scenario(server.port, 'characters-create', 'ash', capture=capture, dev=False, screenshots=shots,
                               log=logs / 'characters-create.log')
    print('PASS: ' + created['detail'], flush=True)
    if shots:
        fresh_screenshots(capture, ['33-character-login.png', '34-character-creator.png', '35-character-roster.png',
                                    '36-player-character-card.png', '37-old-arctic-preview.png', '38-maned-wolf-preview.png'], stamp)
    raw = save.read_text()
    state = json.loads(raw)
    assert 'RATW-fixture-only-2026!' not in raw and 'Incorrect-fixture-2026!' not in raw, 'A plaintext password was saved'
    assert len(state['players']) == 7, 'Six characters on A and one on B must survive the checkpoint'
    assert state.get('accounts'), 'The private account store was not checkpointed'
    assert save.stat().st_mode & 0o077 == 0, 'The credential verifier store is not owner-only'
    print('PASS: the private checkpoint keeps seven characters and no plaintext passwords', flush=True)

    with GameServer(save=save, dev_identity=False, log=logs / 'characters-server-2.log') as server:
        hold_stamp = time.time_ns()
        holder = start_scenario(server.port, 'characters-hold', 'ash', capture=capture, dev=False, log=logs / 'characters-hold.log')
        time.sleep(5)                     # The holder signs in and enters before the duplicate session tries.
        inspected = run_scenario(server.port, 'characters-inspect', 'birch', capture=capture, dev=False, screenshots=shots,
                                 log=logs / 'characters-inspect.log')
        print('PASS: ' + inspected['detail'], flush=True)
        duplicate = run_scenario(server.port, 'characters-duplicate', 'ash', capture=capture, dev=False, log=logs / 'characters-duplicate.log')
        print('PASS: ' + duplicate['detail'], flush=True)
        code = holder.wait(timeout=130)
        held = evidence(capture, 'characters-hold', 'ash', hold_stamp)
        if code or not held['passed']:
            raise RuntimeError(f'characters-hold failed: {held["detail"]}')
        print('PASS: ' + held['detail'], flush=True)
    if shots:
        fresh_screenshots(capture, ['39-other-player-inspection.png'], hold_stamp)
    report = {'passed': True, 'isolatedSave': str(save), 'creationChecks': created['checks'], 'networkChecks': inspected['checks'],
              'noDevelopmentIdentityBypass': True, 'plaintextPasswordStored': False}
    (capture / 'characters-summary.json').write_text(json.dumps(report, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
