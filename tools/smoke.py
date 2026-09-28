#!/usr/bin/env python3
"""Runs the server and scripted browser-client players on a disposable save, and checks their evidence.

    python3 tools/smoke.py network|gallery|walkthrough|persistence|movement|scent [--headless]

--headless plays without a browser (the client's own code in Node) and takes no screenshots.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import time

from game_run import ROOT, GameServer, evidence, run_scenario, start_scenario


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('mode', choices=('network', 'gallery', 'walkthrough', 'persistence', 'movement', 'scent'))
    parser.add_argument('--headless', action='store_true', help='No browser and no screenshots')
    args = parser.parse_args()
    output = ROOT / 'artifacts' / 'screenshots'
    logs = ROOT / 'artifacts' / 'logs'
    output.mkdir(parents=True, exist_ok=True)
    logs.mkdir(parents=True, exist_ok=True)
    run_id = time.time_ns()
    save = ROOT / 'Saved' / 'Tests' / str(run_id) / 'world.json'
    save.parent.mkdir(parents=True, exist_ok=True)
    shots = not args.headless

    if args.mode == 'scent':
        # A disposable two-player fixture: a hidden, crouching source twelve tiles upwind.
        save.write_text(json.dumps({
            'schema': 1, 'revision': 0, 'sequence': 1, 'time': 0,
            'players': [
                {'id': 'player-ash', 'name': 'Ash', 'cell': 'exterior', 'x': 20.5, 'y': 12.5, 'posture': 'standing', 'color': 0},
                {'id': 'player-bracken', 'name': 'Bracken', 'cell': 'exterior', 'x': 8.5, 'y': 12.5, 'posture': 'crouching', 'color': 9},
            ],
            'weather': {'exterior': 0},
            'winds': {'exterior': {'direction': 0, 'strength': 0.5, 'variable': False}},
        }))

    if args.mode in ('network', 'scent'):
        with GameServer(save=save, flags=['--dev-tools'], log=logs / 'server.log') as server:
            players = (('bracken', 'scent-source'), ('ash', 'scent')) if args.mode == 'scent' else (('ash', 'network'), ('bracken', 'network'))
            children = [start_scenario(server.port, scenario, identity, capture=output, screenshots=shots and scenario != 'scent-source',
                                       log=logs / f'{identity}.log') for identity, scenario in players]
            if args.mode == 'scent':
                code = children[1].wait(timeout=180)
                children[0].kill()
                result = evidence(output, 'scent', 'ash', run_id)
                if code or not result['passed']:
                    raise RuntimeError(f"Scent scenario failed: {result['detail']}")
                if shots:
                    fresh(output / '12-upwind-scent.png', run_id)
                print(f"PASS: {result['detail']}", flush=True)
            else:
                codes = [child.wait(timeout=180) for child in children]
                results = [evidence(output, 'network', identity, run_id) for identity in ('ash', 'bracken')]
                if any(codes) or not all(r['passed'] for r in results):
                    raise RuntimeError(f"Network scenario failed: exits={codes}, details={[r['detail'] for r in results]}")
                print("PASS: two independent clients moved and received each other's IC and local OOC events.", flush=True)
    elif args.mode == 'persistence':
        # Three steps, each against a new server process on one save.
        for scenario in ('persist-write', 'persist-read', 'persist-aged'):
            if scenario == 'persist-aged':
                payload = json.loads(save.read_text())
                assert payload['activeMemory'], 'No NPC interaction was stored'
                for memory in payload['activeMemory']:
                    memory['lastActivity'] = time.time() - 3601
                save.write_text(json.dumps(payload))
            with GameServer(save=save, log=logs / f'server-{scenario}.log') as server:
                result = run_scenario(server.port, scenario, 'ash', capture=output, log=logs / f'{scenario}.log')
            print(f"PASS: {scenario}: {result['detail']}", flush=True)
        print('PASS: separate server restarts retained character, map memory and conversation; one hour inactive produced a permanent summary.', flush=True)
    else:
        with GameServer(save=save, flags=['--dev-tools'], log=logs / 'server.log') as server:
            if args.mode == 'movement':
                result = run_scenario(server.port, 'movement', 'ash', capture=output, screenshots=shots, log=logs / 'movement.log')
                print(f"PASS: {result['detail']}", flush=True)
            elif args.mode == 'walkthrough':
                result = run_scenario(server.port, 'walkthrough', 'ash', capture=output, screenshots=True, timeout=240, log=logs / 'walkthrough.log')
                for name in ('06-visible-vertical-world.png', '07-quiet-loft.png', '08-rain-in-juniper-yard.png'):
                    fresh(output / name, run_id)
                print(f"PASS: {result['detail']}", flush=True)
            else:
                run_scenario(server.port, 'gallery', 'ash', capture=output, screenshots=True, timeout=240, log=logs / 'gallery.log')
                for name in ('01-tavern-local.png', '02-character-sheet.png', '03-inventory.png', '04-world-map.png', '05-settings.png',
                             '09-text-first-layout.png'):
                    fresh(output / name, run_id)
                print('PASS: six browser-client screenshots captured.', flush=True)
    return 0


def fresh(path: Path, since_ns: int):
    if not path.exists() or path.stat().st_size < 1024 or path.stat().st_mtime_ns < since_ns:
        raise RuntimeError(f'Missing or stale screenshot: {path}')


if __name__ == '__main__':
    raise SystemExit(main())
