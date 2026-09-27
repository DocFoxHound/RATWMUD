#!/usr/bin/env bash
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
ratw_engine="${RATW_UNREAL_ROOT:-/home/martinb/Applications/UnrealEngine/5.8.2}"
"$ratw_engine/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  -project="$ratw_root/RATWMUD.uproject" -noP4 -platform=Linux \
  -clientconfig=Development -build -cook -map=/Engine/Maps/Entry \
  -stage -pak -archive -archivedirectory="$ratw_root/artifacts/package" \
  -utf8output -unattended -ubtargs="-MaxParallelActions=6 -NoUBA"
