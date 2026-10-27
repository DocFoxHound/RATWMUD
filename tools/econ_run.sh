#!/usr/bin/env bash
# An economy run kept out of /tmp (Docs/Design/64-economy-migration.md, Phase 13, round 10, lane 0): econ_watch over a
# world export for DAYS days, into artifacts/economy/<date>-<NAME>/, its log beside its files (run.log), and its report
# (tools/econ_model/report.py) printed at the end and kept as report.txt.
#
#   bash tools/econ_run.sh NAME DAYS [econ_watch options...]
#
# The export is $RATW_EXPORT (else artifacts/economy/export; make one with python3 tools/world_build.py export DIR).
# The binary is $ECON_WATCH (else build-econ/econ_watch). Unless asked otherwise it runs --deterministic --hours
# --threads 6; any options given go after those (e.g. --without crews, --stop-on-sentinel all, --players hoard).
# A name already used today is refused, so no run is written over. The CSVs and days.jsonl are not committed
# (.gitignore); run.txt and report.txt may be.
set -euo pipefail
ratw_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ $# -lt 2 || ! "$2" =~ ^[0-9]+([.][0-9]+)?$ || ! "$1" =~ ^[A-Za-z0-9._-]+$ ]]; then
  sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
  exit 2
fi
name="$1"
days="$2"
shift 2
export_dir="${RATW_EXPORT:-$ratw_root/artifacts/economy/export}"
binary="${ECON_WATCH:-$ratw_root/build-econ/econ_watch}"
if [[ ! -d "$export_dir" ]]; then
  echo "no world export at $export_dir: set RATW_EXPORT, or make one (python3 tools/world_build.py export DIR)" >&2
  exit 2
fi
if [[ ! -x "$binary" ]]; then
  echo "no econ_watch at $binary: build it (cmake --build build-econ --target econ_watch) or set ECON_WATCH" >&2
  exit 2
fi
out="$ratw_root/artifacts/economy/$(date +%Y-%m-%d)-$name"
if [[ -e "$out" ]]; then
  echo "$out is there already: pick another name" >&2
  exit 2
fi
mkdir -p "$out"
echo "econ_watch: $days days into $out (log: $out/run.log)"
status=0
"$binary" "$export_dir" "$days" "$out" --deterministic --hours --threads 6 "$@" > "$out/run.log" 2>&1 || status=$?
tail -n 4 "$out/run.log"
python3 "$ratw_root/tools/econ_model/report.py" "$out" | tee "$out/report.txt"
exit "$status"
