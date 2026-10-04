#!/usr/bin/env bash
# A scratch game server: a server of your own to look at things, beside the real one (Docs/Design/20-world-database.md,
# "Scratch servers"). It reads the world and its save but writes nothing back, and stops by itself.
#
#   bash tools/scratch.sh dev        DEV's world, read only
#   bash tools/scratch.sh prod       PROD's world, read only
#   bash tools/scratch.sh town       Greyfen Crossing (its save read, never written)
#
# It takes any free port and prints it ("RATW server listening on port N"): open http://127.0.0.1:N/ in a browser.
# It prints its pid too: stop it when you're done (kill PID, or Ctrl-C). Left running, it stops after an hour (--for
# SECONDS to change it). Any number may run at once, beside the real server. NPCs use written lines, not the paid
# model, unless RATW_AI says otherwise. Extra arguments go to the server (e.g. --dev-identity, --for 7200).
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
world="${1:-}"
if [[ "$world" != dev && "$world" != prod && "$world" != town ]]; then
  sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'
  exit 2
fi
shift
export RATW_AI="${RATW_AI:-off}"
if [[ "$world" == town ]]; then
  exec bash "$ratw_root/tools/server.sh" --town --scratch --port 0 "$@"
fi
RATW_DATABASE_URL="$(python3 "$ratw_root/tools/world_db.py" conninfo "$world" --role game)"
export RATW_DATABASE_URL
exec bash "$ratw_root/tools/server.sh" --database "$world" --scratch --port 0 "$@"
