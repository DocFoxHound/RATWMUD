#!/usr/bin/env bash
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
ratw_engine="${RATW_UNREAL_ROOT:-/home/martinb/Applications/UnrealEngine/5.8.2}"
source "$ratw_root/tools/mind.sh"
ratw_start_mind "$@"          # A local game is its own authority: its NPCs talk through the Mind too.
status=0
"$ratw_engine/Engine/Binaries/Linux/UnrealEditor" "$ratw_root/RATWMUD.uproject" /Engine/Maps/Entry -game -windowed -ResX=1600 -ResY=1000 -ForceRes -NoSplash -NoSound -Unattended "${ratw_mind_args[@]}" "$@" || status=$?
exit "$status"
