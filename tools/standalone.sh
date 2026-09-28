#!/usr/bin/env bash
# The standalone headless world server (Server/ratw_server.cpp): the game's authority with no Unreal, started in
# seconds. Builds it first if its sources changed (build-core, CMake Release), starts the NPC Mind as server.sh does
# (RATW_AI=off|fixture), and passes everything else on:
#
#   bash tools/standalone.sh --database dev        DEV's world (RATW_DATABASE_URL; tools/live.sh sets it)
#   bash tools/standalone.sh --save FILE           the demo world (or --world MANIFEST), saved to FILE
#
# Players connect with the Unreal game: RATW_STANDALONE=1 bash tools/connect.sh 127.0.0.1:7788 (or the game started
# with -RatwServer=HOST:PORT). Options: see Server/ratw_server.cpp (--port, --bind, --dev-identity, ...).
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build="$ratw_root/build-core"
if [[ ! -x "$build/ratw_server" ]] || [[ -n "$(find "$ratw_root/Server" "$ratw_root/Source/RATWMUD/Core" -newer "$build/ratw_server" -print -quit)" ]]; then
  echo "RATW: building the standalone server..." >&2
  [[ -f "$build/CMakeCache.txt" ]] || cmake -S "$ratw_root" -B "$build" -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build "$build" --target ratw_server -j"$(nproc)" >/dev/null
fi
source "$ratw_root/tools/mind.sh"
database_args=()
for arg in "$@"; do [[ "$arg" == dev || "$arg" == prod ]] && database_args=("-RatwDatabase=$arg"); done
ratw_start_mind "${database_args[@]}"
dialogue=()
for arg in "${ratw_mind_args[@]}"; do dialogue=(--dialogue "${arg#-RatwDialogueEndpoint=}"); done
status=0
"$build/ratw_server" "${dialogue[@]}" "$@" || status=$?
exit "$status"
