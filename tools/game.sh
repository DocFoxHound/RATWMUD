# Sourced by the launch scripts (server.sh, play.sh, live.sh, connect.sh).
#
#   ratw_build            builds the server (build-core, CMake Release) and the browser client (Client/dist) when their
#                         sources are newer than the builds
#   ratw_open URL         opens the game in a browser (RATW_BROWSER=off prints the address instead)
#   ratw_free_port        a free TCP port on this machine
ratw_server_binary="$ratw_root/build-core/ratw_server"

ratw_build() {
  local build="$ratw_root/build-core" sources=()
  local dir
  for dir in "$ratw_root/Server" "$ratw_root/Core" "$ratw_root/Source/RATWMUD/Core"; do [[ -d "$dir" ]] && sources+=("$dir"); done
  if [[ ! -x "$ratw_server_binary" ]] || [[ -n "$(find "${sources[@]}" "$ratw_root/CMakeLists.txt" -newer "$ratw_server_binary" -print -quit)" ]]; then
    echo "RATW: building the server..." >&2
    [[ -f "$build/CMakeCache.txt" ]] || cmake -S "$ratw_root" -B "$build" -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build "$build" --target ratw_server -j"$(nproc)" >/dev/null
  fi
  local client="$ratw_root/Client"
  if [[ ! -d "$client/node_modules" ]]; then
    echo "RATW: installing the browser client's tools (first run)..." >&2
    npm --prefix "$client" install --no-audit --no-fund >/dev/null
  fi
  if [[ ! -f "$client/dist/index.html" ]] || [[ -n "$(find "$client/src" "$client/index.html" "$client/vite.config.ts" "$ratw_root/Data/Portraits" "$ratw_root/Data/Fonts" -newer "$client/dist/index.html" -print -quit)" ]]; then
    echo "RATW: building the browser client..." >&2
    npm --prefix "$client" run build --silent >/dev/null
  fi
}

ratw_open() {
  local url="$1"
  if [[ "${RATW_BROWSER:-on}" == off ]] || ! command -v xdg-open >/dev/null || [[ -z "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then
    echo "RATW: open $url in a browser." >&2
    return 0
  fi
  xdg-open "$url" >/dev/null 2>&1 &
}

ratw_free_port() {
  python3 -c "import socket; s=socket.socket(); s.bind(('127.0.0.1', 0)); print(s.getsockname()[1])"
}
