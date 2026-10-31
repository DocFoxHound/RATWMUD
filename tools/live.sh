#!/usr/bin/env bash
# Runs the game on a world in the database (the worlds: python3 tools/world_db.py list; Docs/Design/66-worlds-atlas-and-dm.md).
#
#   bash tools/live.sh server [KEY]   the server on a world (default the live world: what players join, at http://HOST:7788/)
#   bash tools/live.sh play [KEY]     a world on this machine, opened in the browser (default Atlas's world)
#
# KEY is a registered world (or atlas, live: the local choices). A dev world's build is brought up to date first
# (skipped when nothing changed since the last one; RATW_NO_BUILD=1 skips the check too); a live world's builds come
# from Push to live. Extra arguments go to the server (e.g. --port 7790). A server restarts itself when Push to live
# publishes a new release (it exits with status 75 once nobody is connected); any other exit ends it.
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
usage() { sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; }
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
if ! resolved="$(python3 "$ratw_root/tools/world_db.py" resolve "$choice" --with-kind)"; then
  usage
  exit 2
fi
database="${resolved% *}"; kind="${resolved#* }"
if [[ "$kind" == dev && "${RATW_NO_BUILD:-0}" != 1 ]]; then
  python3 "$ratw_root/tools/world_build.py" "$database"
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
