#!/usr/bin/env bash
# Atlas: the one tool (tools/studio.sh, doc 66 section 3) opening on its Atlas suite, http://127.0.0.1:8765
# (local only). Extra arguments go to studio.py, e.g. --port 8800 or --world KEY.
set -euo pipefail
exec bash "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/studio.sh" --suite atlas "$@"
