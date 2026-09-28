#!/usr/bin/env bash
# Runs the game on the one world in the database.
#
#   bash tools/live.sh server prod   the server on PROD (what players join, at http://HOST:7788/)
#   bash tools/live.sh server dev    the server on DEV, after building DEV's current world
#   bash tools/live.sh play dev      DEV on this machine, opened in the browser
#
# Extra arguments go to the server (e.g. --port 7790). DEV's build is brought up to date
# first (skipped when nothing changed since the last one; RATW_NO_BUILD=1 skips the
# check too). A server restarts itself when Push to live publishes a new release (it
# exits with status 75 once nobody is connected); any other exit ends it.
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-}"; database="${2:-}"
if [[ "$mode" != server && "$mode" != play ]] || [[ "$database" != prod && "$database" != dev ]]; then
  sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//'
  exit 2
fi
shift 2
if [[ "$database" == dev && "${RATW_NO_BUILD:-0}" != 1 ]]; then
  python3 "$ratw_root/tools/world_build.py" dev
fi
RATW_DATABASE_URL="$(python3 "$ratw_root/tools/world_db.py" conninfo "$database" --role game)"
export RATW_DATABASE_URL
if [[ "$mode" == play ]]; then
  exec bash "$ratw_root/tools/play.sh" --database "$database" "$@"
fi
while true; do
  status=0
  bash "$ratw_root/tools/server.sh" --database "$database" "$@" || status=$?
  if [[ "$status" -ne 75 ]]; then
    exit "$status"
  fi
  echo "RATW: a new release was published; restarting on it." >&2
done
