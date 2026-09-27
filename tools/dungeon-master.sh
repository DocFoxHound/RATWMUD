#!/usr/bin/env bash
# Dungeon Master: builds the UI when its sources changed, then serves it
# on http://127.0.0.1:8766 (local only). Extra arguments go to dungeon_master.py,
# e.g. --port 8800. Sign in with a DM account (see dungeon_master.py accounts).
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
editor="$ratw_root/Editor"
if [ ! -d "$editor/node_modules" ]; then
  echo "Installing editor dependencies (first run)…"
  npm --prefix "$editor" install --no-audit --no-fund
fi
if [ ! -f "$editor/dist/dm.html" ] || [ -n "$(find "$editor/src" "$editor/dm.html" -newer "$editor/dist/dm.html" -print -quit)" ]; then
  echo "Building the Dungeon Master UI…"
  npm --prefix "$editor" run build --silent
fi
exec python3 "$ratw_root/tools/dungeon_master.py" serve "$@"
