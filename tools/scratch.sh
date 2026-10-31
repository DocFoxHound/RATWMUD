#!/usr/bin/env bash
# A scratch game server: a server of your own to look at things, beside the real one (Docs/Design/20-world-database.md,
# "Scratch servers"). It reads the world and its save but writes nothing back, and stops by itself.
#
#   bash tools/scratch.sh            Atlas's world (python3 tools/world_db.py list), read only
#   bash tools/scratch.sh KEY        any registered world (dev, prod, ridgemere2...; atlas, live: the local choices)
#   bash tools/scratch.sh town       Greyfen Crossing (its save read, never written)
#
# It takes any free port and prints it ("RATW server listening on port N"): open http://127.0.0.1:N/ in a browser.
# It prints its pid too: stop it when you're done (kill PID, or Ctrl-C). Left running, it stops after an hour (--for
# SECONDS to change it). Any number may run at once, beside the real server. NPCs use written lines, not the paid
# model, unless RATW_AI says otherwise. A dev world plays its newest build (python3 tools/world_build.py KEY makes
# one). Extra arguments go to the server (e.g. --dev-identity, --for 7200).
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
usage() { sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'; }
world="${1:-}"
case "$world" in
  -h|--help) usage; exit 0 ;;
  ""|-*) world=atlas ;;
  *) shift ;;
esac
export RATW_AI="${RATW_AI:-off}"
if [[ "$world" == town ]]; then
  exec bash "$ratw_root/tools/server.sh" --town --scratch --port 0 "$@"
fi
if ! world="$(python3 "$ratw_root/tools/world_db.py" resolve "$world")"; then
  usage
  exit 2
fi
RATW_DATABASE_URL="$(python3 "$ratw_root/tools/world_db.py" conninfo "$world" --role game)"
export RATW_DATABASE_URL
exec bash "$ratw_root/tools/server.sh" --database "$world" --scratch --port 0 "$@"
