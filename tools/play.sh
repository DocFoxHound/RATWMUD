#!/usr/bin/env bash
# Plays on this machine: starts a game server (tools/server.sh) on a free port and opens it in the browser. Stops the
# server when this script stops (Ctrl+C).
#
#   bash tools/play.sh                          the demo world (--town, --world MANIFEST, --database dev: as server.sh)
#   bash tools/play.sh --identity tester        signs straight in as a development identity (implies --dev-identity)
#
# Other options go to server.sh.
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ratw_root/tools/game.sh"
args=() identity="" name="" port=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --identity) identity="$2"; shift 2 ;;
    --name) name="$2"; shift 2 ;;
    --port) port="$2"; shift 2 ;;
    *) args+=("$1"); shift ;;
  esac
done
port="${port:-$(ratw_free_port)}"
query=""
if [[ -n "$identity" ]]; then
  args+=(--dev-identity)
  query="?identity=$identity&name=${name:-$identity}"
fi
url="http://127.0.0.1:$port/$query"
(
  # Open the page once the server listens.
  for _ in $(seq 1 600); do
    if python3 -c "import socket,sys; s=socket.socket(); s.settimeout(.2); sys.exit(s.connect_ex(('127.0.0.1', $port)))" 2>/dev/null; then
      ratw_open "$url"
      exit 0
    fi
    sleep 0.1
  done
) &
exec bash "$ratw_root/tools/server.sh" --port "$port" "${args[@]}"
