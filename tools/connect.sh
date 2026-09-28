#!/usr/bin/env bash
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
ratw_engine="${RATW_UNREAL_ROOT:-/home/martinb/Applications/UnrealEngine/5.8.2}"
# RATW_STANDALONE=1: a client of the standalone server (tools/standalone.sh; default port 7788) rather than of an
# Unreal server.
if [[ "${RATW_STANDALONE:-0}" == 1 ]]; then
  ratw_address="${1:-127.0.0.1:7788}"
  if [ "$#" -gt 0 ]; then shift; fi
  exec "$ratw_engine/Engine/Binaries/Linux/UnrealEditor" "$ratw_root/RATWMUD.uproject" /Engine/Maps/Entry "-RatwServer=$ratw_address" -game -windowed -ResX=1600 -ResY=1000 -ForceRes -NoSplash -NoSound -Unattended "$@"
fi
ratw_address="${1:-127.0.0.1:7787}"
if [ "$#" -gt 0 ]; then shift; fi
exec "$ratw_engine/Engine/Binaries/Linux/UnrealEditor" "$ratw_root/RATWMUD.uproject" "$ratw_address" -game -windowed -ResX=1600 -ResY=1000 -ForceRes -NoSplash -NoSound -Unattended "$@"
