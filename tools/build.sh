#!/usr/bin/env bash
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
ratw_engine="${RATW_UNREAL_ROOT:-/home/martinb/Applications/UnrealEngine/5.8.2}"
ratw_target="${1:-RATWMUDEditor}"
"$ratw_engine/Engine/Build/BatchFiles/Linux/Build.sh" "$ratw_target" Linux Development "$ratw_root/RATWMUD.uproject" -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=6
