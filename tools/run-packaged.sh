#!/usr/bin/env bash
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
ratw_binary="$ratw_root/artifacts/package/Linux/RATWMUD/Binaries/Linux/RATWMUD"
if [ ! -x "$ratw_binary" ]; then
  echo "No Linux package found. Run: bash tools/package.sh" >&2
  exit 1
fi
ratw_mode="${1:-play}"
if [ "$#" -gt 0 ]; then shift; fi
case "$ratw_mode" in
  play)
    exec "$ratw_binary" /Engine/Maps/Entry -windowed -ResX=1600 -ResY=1000 -ForceRes -NoSplash -NoSound "$@"
    ;;
  server)
    exec "$ratw_binary" '/Engine/Maps/Entry?listen' -RatwHeadlessHost -nullrhi -NoSound -NoSplash -Unattended -port=7787 -MULTIHOME="${RATW_BIND:-127.0.0.1}" -log "$@"
    ;;
  connect)
    ratw_address="${1:-127.0.0.1:7787}"
    if [ "$#" -gt 0 ]; then shift; fi
    exec "$ratw_binary" "$ratw_address" -windowed -ResX=1600 -ResY=1000 -ForceRes -NoSplash -NoSound "$@"
    ;;
  *) echo "Usage: bash tools/run-packaged.sh [play|server|connect [address]] [Unreal flags]" >&2; exit 2 ;;
esac
