#!/usr/bin/env python3
"""talk_coverage (Docs/Design/84 §15.4, a ctest): `talk_check --coverage` over the library and Data/Talk/profiles.json,
its holes compared with Tests/fixtures/talk_coverage.json: no move may have more holes than it had (the hole rate may
only fall), and a core move with none may not gain one. Phase 7 lists the holes; Phase 8 writes the lines that fill
them (its gate: no holes in the core moves).

  python3 Tests/talk_coverage_check.py BUILD_DIR            the check (exit 1 if a move has more holes)
  python3 Tests/talk_coverage_check.py BUILD_DIR --bless    write the baseline (say why in the commit)
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = ROOT / 'Tests' / 'fixtures' / 'talk_coverage.json'


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    run = subprocess.run([str(Path(sys.argv[1]) / 'talk_check'), '--coverage', '--json'], capture_output=True, text=True)
    if run.returncode != 0:
        print(run.stdout + run.stderr, file=sys.stderr)
        return 1
    now = json.loads(run.stdout)
    keep = {k: now[k] for k in ('moves_owed', 'topics', 'kinds', 'bands', 'cells', 'holes', 'hole_rate', 'core_holes')}
    keep['by_move'] = {m: {'core': r['core'], 'lines': r['lines'], 'holes': r['holes'], 'cells': r['cells']}
                       for m, r in now['by_move'].items()}
    if '--bless' in sys.argv[2:]:
        BASELINE.write_text(json.dumps(keep, indent=1, sort_keys=True) + '\n')
        print(f'talk_coverage: baseline written, {keep["holes"]} holes of {keep["cells"]} cells ({keep["core_holes"]} core)')
        return 0
    base = json.loads(BASELINE.read_text())
    problems = []
    for m, r in keep['by_move'].items():
        b = base['by_move'].get(m)
        if b is None:
            continue                                # (A move newly owed: listed, not judged until blessed.)
        scale = r['cells'] / b['cells'] if b['cells'] else 1
        if r['holes'] > b['holes'] * scale:
            problems.append(f'{m}: {r["holes"]} holes of {r["cells"]} cells, more than the baseline\'s {b["holes"]} of {b["cells"]}')
    for p in problems:
        print('talk_coverage:', p)
    print(f'talk_coverage: {keep["holes"]} holes of {keep["cells"]} cells ({100 * keep["hole_rate"]:.1f}%), '
          f'{keep["core_holes"]} in the core moves; baseline {base["holes"]} ({base["core_holes"]} core)')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
