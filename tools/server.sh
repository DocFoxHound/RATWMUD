#!/usr/bin/env bash
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
ratw_engine="${RATW_UNREAL_ROOT:-/home/martinb/Applications/UnrealEngine/5.8.2}"
ratw_bind="${RATW_BIND:-127.0.0.1}"
exec "$ratw_engine/Engine/Binaries/Linux/UnrealEditor" "$ratw_root/RATWMUD.uproject" /Engine/Maps/Entry -server -nullrhi -NoSound -NoSplash -Unattended -port=7787 -MULTIHOME="$ratw_bind" -log "$@"
