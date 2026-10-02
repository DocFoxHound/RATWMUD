#!/usr/bin/env bash
# Builds the browser's walking (Client/wasm/walk.cpp over Core/RatwStep.cpp) into Client/src/wasm/walk.wasm, the
# server's own movement rules for the page (Docs/Design/31-responsiveness.md, Phase 3). Needs Emscripten: the emsdk in
# ~/emsdk (or $EMSDK). The .wasm is committed, so building the client never needs it; rebuild after changing
# Core/RatwStep.* and commit the result.
set -euo pipefail
ratw_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
emsdk="${EMSDK:-$HOME/emsdk}"
if ! command -v em++ > /dev/null; then
  [[ -f "$emsdk/emsdk_env.sh" ]] || { echo "Emscripten not found: install the emsdk in ~/emsdk (or set EMSDK)." >&2; exit 1; }
  # shellcheck disable=SC1091
  source "$emsdk/emsdk_env.sh" > /dev/null 2>&1
fi
out="$ratw_root/Client/src/wasm/walk.wasm"
em++ -O3 -std=c++17 -fno-exceptions -fno-rtti -I "$ratw_root/Core" \
  "$ratw_root/Client/wasm/walk.cpp" "$ratw_root/Core/RatwStep.cpp" \
  -s STANDALONE_WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=16MB -s FILESYSTEM=0 --no-entry \
  -o "$out"
echo "Built $out ($(stat -c %s "$out") bytes)"
