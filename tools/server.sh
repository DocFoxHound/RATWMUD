#!/usr/bin/env bash
# The game server: ratw_server with the browser client and the NPC Mind. Players open http://HOST:PORT/ in a browser.
#
#   bash tools/server.sh                        the demo world, saved to Saved/ratw-world.json
#   bash tools/server.sh --town                 Greyfen Crossing (Data/Worlds/Greyfen), saved to Saved/ratw-town.json
#   bash tools/server.sh --world MANIFEST       an exported Atlas world, saved to Saved/Atlas/<its name>.json
#   bash tools/server.sh --database dev|prod    the world in the database (tools/live.sh sets RATW_DATABASE_URL)
#
# --save FILE picks another save. Everything else goes to ratw_server (see Server/ratw_server.cpp): --port N (default
# 7788), --dev-identity, --dev-tools, --dm-directory DIR... RATW_BIND chooses the interface (default 127.0.0.1: this
# machine only). RATW_AI=off|fixture: see tools/mind.sh. RATW_VOICE=off: no speech router or exchange library.
# The server and client are built first if their sources changed.
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ratw_root/tools/game.sh"
source "$ratw_root/tools/mind.sh"
args=() world="" save="" database="" port=7788
while [[ $# -gt 0 ]]; do
  case "$1" in
    --town) world="$ratw_root/Data/Worlds/Greyfen/world.ratw"; save="${save:-$ratw_root/Saved/ratw-town.json}"; shift ;;
    --world) world="$(realpath "$2")"; shift 2 ;;
    --save) save="$2"; shift 2 ;;
    --database) database="$2"; shift 2 ;;
    --port) port="$2"; shift 2 ;;
    *) args+=("$1"); shift ;;
  esac
done
ratw_build
command=("$ratw_server_binary" --port "$port" --bind "${RATW_BIND:-127.0.0.1}" --web "$ratw_root/Client/dist")
# Cheaper voices (Docs/Design/28-ai-cost.md): the game answers what it knows, and keeps a ledger of who answered each
# NPC line (no words in it) for tools/ai_cost.py. RATW_VOICE=off leaves everything to the NPC Mind.
if [[ "${RATW_VOICE:-on}" != off ]]; then
  mkdir -p "$ratw_root/Saved/Logs"
  command+=(--voice-data "$ratw_root/Data/Voice" --voice-log "$ratw_root/Saved/Logs/npc-voices.jsonl")
fi
if [[ -n "$database" ]]; then
  command+=(--database "$database")
else
  if [[ -z "$save" ]]; then
    if [[ -n "$world" ]]; then
      save="$ratw_root/Saved/Atlas/$(printf '%s' "$world" | md5sum | cut -c1-32).json"
    else
      save="$ratw_root/Saved/ratw-world.json"
    fi
  fi
  mkdir -p "$(dirname "$save")"
  command+=(--save "$save")
  [[ -n "$world" ]] && command+=(--world "$world")
fi
ratw_start_mind --database "$database"
if [[ "$port" == 0 ]]; then
  echo "RATW: the server takes a free port and prints it as it starts (\"listening on port N\")." >&2
else
  echo "RATW: players join at http://${RATW_BIND:-127.0.0.1}:$port/" >&2
fi
status=0
"${command[@]}" "${ratw_mind_args[@]}" "${args[@]}" || status=$?
exit "$status"
