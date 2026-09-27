#!/usr/bin/env bash
# Atlas Workshop: builds the editor UI when its sources changed, then serves it
# on http://127.0.0.1:8765 (local only). Extra arguments go to map_editor.py,
# e.g. --port 8800. For UI development with hot reload, run the server and
# `npm --prefix Editor run dev` side by side instead.
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
editor="$ratw_root/Editor"
if [ ! -d "$editor/node_modules" ]; then
  echo "Installing editor dependencies (first run)…"
  npm --prefix "$editor" install --no-audit --no-fund
fi
if [ ! -f "$editor/dist/index.html" ] || [ -n "$(find "$editor/src" "$editor/index.html" -newer "$editor/dist/index.html" -print -quit)" ]; then
  echo "Building the editor UI…"
  npm --prefix "$editor" run build --silent
fi
exec python3 "$ratw_root/tools/map_editor.py" serve "$@"
