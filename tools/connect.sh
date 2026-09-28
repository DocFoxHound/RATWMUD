#!/usr/bin/env bash
# Joins a game server from this machine: opens its page in the browser.
#
#   bash tools/connect.sh [HOST:PORT]           default 127.0.0.1:7788
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ratw_root/tools/game.sh"
ratw_open "http://${1:-127.0.0.1:7788}/"
