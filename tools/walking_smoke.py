#!/usr/bin/env python3
"""The page walks its own wolf (Docs/Design/31-responsiveness.md, Phase 3): a server on a disposable save, the real
client in headless Chromium, a key held; the wolf moves on the page and the server takes its poses.

    python3 tools/walking_smoke.py
"""
from __future__ import annotations

import subprocess
import sys
import time

from game_run import ROOT, GameServer


def main() -> int:
    save = ROOT / 'Saved' / 'Tests' / str(time.time_ns()) / 'world.json'
    save.parent.mkdir(parents=True, exist_ok=True)
    log = ROOT / 'artifacts' / 'logs'
    log.mkdir(parents=True, exist_ok=True)
    with GameServer(save=save, log=log / 'walking-server.log') as server:
        result = subprocess.run(['node', str(ROOT / 'tools' / 'client' / 'walking.mjs'), str(server.port)], cwd=ROOT,
                                capture_output=True, text=True, timeout=120)
    print(result.stdout.strip() or result.stderr.strip())
    return result.returncode


if __name__ == '__main__':
    sys.exit(main())
