#!/usr/bin/env bash
# Runs the game on a world in the database (the worlds: python3 tools/world_db.py list; Docs/Design/66-worlds-atlas-and-dm.md).
#
#   bash tools/live.sh server [KEY]   the server on a world (default the live world: what players join, at http://HOST:7788/)
#   bash tools/live.sh play [KEY]     a world on this machine, opened in the browser (default Atlas's world)
#
# KEY is a registered world (or atlas, live: the local choices; the live one may be a dev world, served as live). A dev
# world's build is brought up to date first (skipped when nothing changed since the last one; RATW_NO_BUILD=1 skips the
# check too); a live world's builds come from its imports. Extra arguments go to the server (e.g. --port 7790). A
# server restarts itself when a new release, or a dev world's new build, is published (it exits with status 75 once
# nobody is connected). When its world closes for maintenance (status 76, also when it was closed already) it waits,
# checking every few seconds (RATW_MAINTENANCE_POLL), until the world is open again, then starts it; on the live world
# it starts whichever world is the live one by then (the DM may have switched it). Any other exit ends it.
# The server is on the local network too (players on another computer open http://THIS-COMPUTER:7788/); RATW_BIND=127.0.0.1: this computer only.
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
usage() { sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'; }
mode="${1:-}"
if [[ "$mode" != server && "$mode" != play ]]; then
  usage
  exit 2
fi
shift
choice=""
if [[ $# -gt 0 && "$1" != -* ]]; then
  choice="$1"
  shift
fi
[[ -z "$choice" ]] && { [[ "$mode" == server ]] && choice=live || choice=atlas; }
# The world for `choice` now: its key and kind, its build brought up to date, its connection.
prepare() {
  local resolved
  resolved="$(python3 "$ratw_root/tools/world_db.py" resolve "$choice" --with-kind)" || return 1
  database="${resolved% *}"; kind="${resolved#* }"
  if [[ "$kind" == dev && "${RATW_NO_BUILD:-0}" != 1 ]]; then
    python3 "$ratw_root/tools/world_build.py" "$database"
  fi
  RATW_DATABASE_URL="$(python3 "$ratw_root/tools/world_db.py" conninfo "$database" --role game)"
  export RATW_DATABASE_URL
}
if ! prepare; then
  usage
  exit 2
fi
if [[ "$mode" == play ]]; then
  exec bash "$ratw_root/tools/play.sh" --database "$database" "$@"
fi
while true; do
  status=0
  bash "$ratw_root/tools/server.sh" --database "$database" "$@" || status=$?
  if [[ "$status" -eq 75 ]]; then
    # A release (a live world), or a new build of a dev world (Atlas's edits); a dev world is brought up to date again.
    echo "RATW: a new version of $database was published; restarting on it." >&2
    prepare || exit 2
    continue
  fi
  if [[ "$status" -ne 76 ]]; then
    exit "$status"
  fi
  echo "RATW: $database is closed for maintenance; waiting until it is opened again (the DM, or" \
       "python3 tools/world_sync.py maintenance $database off)." >&2
  while true; do
    next="$(python3 "$ratw_root/tools/world_db.py" resolve "$choice" 2>/dev/null)" || next="$database"
    state="$(python3 "$ratw_root/tools/world_sync.py" status "$next" --state 2>/dev/null)" || state=unreachable
    [[ "$state" == running ]] && break
    sleep "${RATW_MAINTENANCE_POLL:-5}"
  done
  was="$database"
  prepare || exit 2
  [[ "$database" != "$was" ]] && echo "RATW: the live world is now $database (was $was)." >&2
  echo "RATW: $database is open again; starting its server." >&2
done
