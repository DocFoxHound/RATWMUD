#!/usr/bin/env bash
# RATW Studio, one tool with two suites, Atlas and the DM (Docs/Design/66-worlds-atlas-and-dm.md, section 3):
# builds the UI when its sources changed, then serves it on http://127.0.0.1:8765 and the local network (another
# computer opens http://THIS-COMPUTER:8765/; the addresses are printed). Extra arguments go to studio.py: --port N,
# --suite atlas|dm (the suite it opens on), --world KEY (Atlas's world for this run), --host 127.0.0.1 (this computer
# only). Sign in with a DM account (python3 tools/dungeon_master.py accounts ...). For UI development with hot reload,
# run the studio and `npm --prefix Editor run dev` side by side instead.
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
editor="$ratw_root/Editor"
if [ ! -d "$editor/node_modules" ]; then
  echo "Installing the studio's dependencies (first run)…"
  npm --prefix "$editor" install --no-audit --no-fund
fi
if [ ! -f "$editor/dist/index.html" ] || [ -n "$(find "$editor/src" "$editor/index.html" -newer "$editor/dist/index.html" -print -quit)" ]; then
  echo "Building the studio UI…"
  npm --prefix "$editor" run build --silent
fi
exec python3 "$ratw_root/tools/studio.py" serve "$@"
