#!/usr/bin/env python3
"""Frame smoothness in the browser client while walking (Docs/Design/29-client-polish.md, phase 1).

Starts a server on a disposable copy of a world, walks a development wolf around in headless Chromium, and reports the
game's work per frame and any long frames (see tools/client/frames.mjs for the budget).

    python3 tools/client_frame_smoke.py                    the demo world
    python3 tools/client_frame_smoke.py --database dev     the DEV database world (the server saves into it: ask first)
    python3 tools/client_frame_smoke.py --seconds 40 --lenient
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

from game_run import GameServer, ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--database', help='Play a database world (dev) instead of the demo')
    parser.add_argument('--world', type=lambda p: Path(p).resolve(), help='A world manifest to play')
    parser.add_argument('--seconds', type=int, default=20)
    parser.add_argument('--lenient', action='store_true', help='Report only; never fail')
    args = parser.parse_args()
    run = Path(tempfile.mkdtemp(prefix='frames-', dir=ROOT / 'Saved'))
    logs = ROOT / 'artifacts' / 'logs'
    with GameServer(save=None if args.database else run / 'world.json', world=args.world, database=args.database,
                    flags=['--dev-tools'], log=logs / 'frames-server.log') as server:
        env = {**os.environ, **({'RATW_FRAMES_LENIENT': '1'} if args.lenient else {})}
        return subprocess.run(['node', str(ROOT / 'tools/client/frames.mjs'), server.url, str(args.seconds)], env=env).returncode


if __name__ == '__main__':
    raise SystemExit(main())
