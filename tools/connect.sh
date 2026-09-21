#!/usr/bin/env bash
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
ratw_engine="${RATW_UNREAL_ROOT:-/home/martinb/Applications/UnrealEngine/5.8.2}"
ratw_address="${1:-127.0.0.1:7787}"
if [ "$#" -gt 0 ]; then shift; fi
exec "$ratw_engine/Engine/Binaries/Linux/UnrealEditor" "$ratw_root/RATWMUD.uproject" "$ratw_address" -game -windowed -ResX=1600 -ResY=1000 -ForceRes -NoSplash -NoSound -Unattended "$@"
