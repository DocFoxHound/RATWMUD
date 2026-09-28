#!/usr/bin/env python3
"""Three browser-client connections to one running server verify aging while logged out."""
from pathlib import Path
import tempfile

from game_run import ROOT, GameServer, run_scenario


def main():
    tests = ROOT / 'Saved/Tests'
    tests.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='offline-age-', dir=tests))
    evidence = ROOT / 'artifacts' / 'screenshots'
    logs = ROOT / 'artifacts' / 'logs'
    with GameServer(save=run / 'world.json', flags=['--dev-tools'], log=logs / 'offline-age-server.log') as server:
        for identity, scenario in [('ash', 'age-register'), ('birch', 'age-advance'), ('ash', 'age-return')]:
            result = run_scenario(server.port, scenario, identity, capture=evidence, log=logs / f'{scenario}.log')
            print('PASS: ' + result['detail'], flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
