#!/usr/bin/env python3
"""Finite trade and the calendar in the browser client, then a separate-process restart."""
import argparse
from pathlib import Path
import tempfile

from game_run import series


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true', help='No browser and no screenshots')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    tests = root / 'Saved/Tests'
    tests.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='society-', dir=tests))
    series(('society', 'society-restore'), run / 'world.json', headless=args.headless,
           screenshots=('29-finite-merchant.png', '30-birthday-character.png', '31-nightly-rest.png', '32-birthday-notice.png'))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
