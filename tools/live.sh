#!/usr/bin/env bash
# Runs the game on the one world in the database.
#
#   bash tools/live.sh server prod   dedicated server on PROD (what players join)
#   bash tools/live.sh server dev    dedicated server on DEV, after building DEV's current world
#   bash tools/live.sh play dev      a local game window on DEV (single process)
#
# Extra arguments go to the game (e.g. -port=7788). DEV's build is brought up to date
# first (skipped when nothing changed since the last one; RATW_NO_BUILD=1 skips the
# check too). RATW_PACKAGED=1 runs the server from the cooked package (tools/package.sh)
# instead of the editor: it starts in seconds, but runs the code the package was made
# from. A server restarts itself when Push to live publishes a new release (it exits
# with status 75 once nobody is connected); any other exit ends it.
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-}"; database="${2:-}"
if [[ "$mode" != server && "$mode" != play ]] || [[ "$database" != prod && "$database" != dev ]]; then
  sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'
  exit 2
fi
shift 2
if [[ "$database" == dev && "${RATW_NO_BUILD:-0}" != 1 ]]; then
  python3 "$ratw_root/tools/world_build.py" dev
fi
RATW_DATABASE_URL="$(python3 "$ratw_root/tools/world_db.py" conninfo "$database" --role game)"
export RATW_DATABASE_URL
if [[ "$mode" == play ]]; then
  exec bash "$ratw_root/tools/play.sh" "-RatwDatabase=$database" "$@"
fi
while true; do
  status=0
  if [[ "${RATW_PACKAGED:-0}" == 1 ]]; then
    bash "$ratw_root/tools/run-packaged.sh" server "-RatwDatabase=$database" "$@" || status=$?
  else
    bash "$ratw_root/tools/server.sh" "-RatwDatabase=$database" "$@" || status=$?
  fi
  if [[ "$status" -ne 75 ]]; then
    exit "$status"
  fi
  echo "RATW: a new release was published; restarting on it." >&2
done
