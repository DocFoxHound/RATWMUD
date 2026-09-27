# Sourced by the launch scripts (server.sh, play.sh, run-packaged.sh): live NPC conversation by default.
#
#   ratw_start_mind "$@"      starts the NPC Mind (tools/npc_mind.py) unless one is already listening, and sets
#                             ratw_mind_args to the game flag that uses it. Stops it again when the script exits.
#
# The key and model come from Saved/Config/RATWNPCAI.local.json (gitignored, owner-only), or RATW_AI_CONFIG.
# RATW_AI=off plays with the authored lines only; RATW_AI=fixture uses the Mind's offline replies (no cost).
# A game started with -RatwDatabase=dev|prod gives the Mind that database's event log for NPCs' history.
# RATW_MIND_PORT picks the port (default 18766). The Mind's log: Saved/Logs/npc-mind.log (no dialogue in it).
ratw_mind_args=()
ratw_mind_pid=""

ratw_stop_mind() {
  if [[ -n "$ratw_mind_pid" ]] && kill -0 "$ratw_mind_pid" 2>/dev/null; then
    kill "$ratw_mind_pid" 2>/dev/null || true
    wait "$ratw_mind_pid" 2>/dev/null || true
  fi
}

ratw_start_mind() {
  local mode="${RATW_AI:-on}" port="${RATW_MIND_PORT:-18766}" database="" arg
  [[ "$mode" == off ]] && { echo "RATW: NPC conversation uses authored lines (RATW_AI=off)." >&2; return 0; }
  for arg in "$@"; do
    case "$arg" in -RatwDatabase=dev|-RatwDatabase=prod) database="${arg#-RatwDatabase=}" ;; esac
  done
  local endpoint="http://127.0.0.1:$port/dialogue"
  if python3 -c "import socket,sys; s=socket.socket(); s.settimeout(.3); sys.exit(s.connect_ex(('127.0.0.1', $port)))" 2>/dev/null; then
    echo "RATW: using the NPC Mind already listening on port $port." >&2
    ratw_mind_args=("-RatwDialogueEndpoint=$endpoint")
    return 0
  fi
  local source=()
  if [[ "$mode" == fixture ]]; then
    source=(--fixture)
  else
    local config="${RATW_AI_CONFIG:-$ratw_root/Saved/Config/RATWNPCAI.local.json}"
    if [[ ! -f "$config" ]]; then
      echo "RATW: no AI config at $config; NPCs use authored lines. (See tools/mind.sh.)" >&2
      return 0
    fi
    source=(--config "$config")
  fi
  local history=()
  [[ -n "$database" ]] && history=(--database "$database")
  mkdir -p "$ratw_root/Saved/Logs"
  python3 "$ratw_root/tools/npc_mind.py" "${source[@]}" "${history[@]}" --port "$port" \
    >"$ratw_root/Saved/Logs/npc-mind.log" 2>&1 &
  ratw_mind_pid=$!
  trap ratw_stop_mind EXIT
  local waited
  for waited in $(seq 1 50); do
    if grep -q '"event": "ready"' "$ratw_root/Saved/Logs/npc-mind.log" 2>/dev/null; then
      echo "RATW: NPC Mind ready on port $port ($([[ $mode == fixture ]] && echo "offline replies" || echo "live model")${database:+, history from $database})." >&2
      ratw_mind_args=("-RatwDialogueEndpoint=$endpoint")
      return 0
    fi
    kill -0 "$ratw_mind_pid" 2>/dev/null || break
    sleep 0.1
  done
  echo "RATW: the NPC Mind did not start (see Saved/Logs/npc-mind.log); NPCs use authored lines." >&2
  ratw_stop_mind
  ratw_mind_pid=""
}
