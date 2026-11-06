#!/usr/bin/env bash
# Steering the economy (Docs/Design/74-steering-the-economy.md, 14): the run variants (export70 with rule lines, its
# cells and seams hard-linked, nothing copied) and the 60-day smokes at -100, 0 and +100 and the centre with both floors,
# each on its own cores with stable10b's flags. Needs the user's go-ahead (no economy sims without it).
#   bash artifacts/economy/r2/steer-smokes.sh prepare       the four variant folders only
#   bash artifacts/economy/r2/steer-smokes.sh smokes        prepare, then the four 60-day smokes in parallel, then reports
#   bash artifacts/economy/r2/steer-smokes.sh years         prepare, then the four 3-year runs in parallel, then reports
#   STEER_ONLY="observe left" ...                           only those runs (their names as in `rules` below)
# The runs of 2026-11-06 (doc 74, Progress, "The slider's runs") add `observe` (the steering on at the centre with its band
# 0 wide: the push held at 0, so the economy is stable10b's and the measures are logged: the centre's calibration) and
# `sweep` (one run, the slider moved by --rule-at as the DM's economy.sync would: the centre for year 1, far left for
# year 2, far right for year 3), on cores 18-19 with two threads.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)"
tree="${STEER_TREE:-$root/.claude/worktrees/r2-steering}"
bin="${STEER_BIN:-$tree/build-steer/world_check}"
r2="$root/artifacts/economy/r2"
declare -A rules=(
  [centre]='rule "steer" 1|rule "steerLean" 100'
  [left]='rule "steer" 1|rule "steerLean" 0'
  [right]='rule "steer" 1|rule "steerLean" 200'
  [centre-floors]='rule "steer" 1|rule "steerLean" 100|rule "neverHungry" 1|rule "neverBroke" 1'
  [observe]='rule "steer" 1|rule "steerLean" 100|rule "steerWidthMid" 0'
  [sweep]='rule "steer" 1|rule "steerLean" 100'
)
declare -A cores=([centre]=2-5 [left]=6-9 [right]=10-13 [centre-floors]=14-17 [observe]=2-5 [sweep]=6-9)
declare -A threads=()
declare -A extra=([sweep]="--rule-at 365 steerLean 0 --rule-at 730 steerLean 200")
names="${STEER_ONLY:-centre left right centre-floors}"
prepare() {
  for name in $names; do
    dir="$r2/export70-steer-$name"
    mkdir -p "$dir"
    # (Hard links: world_check's walk follows no linked folder, and nothing is copied.)
    for part in cells seams; do
      [[ -L "$dir/$part" ]] && rm "$dir/$part"
      [[ -d "$dir/$part" ]] || cp -al "$r2/export70/$part" "$dir/$part"
    done
    # The rule lines after the store line (where the world's economy records are).
    IFS='|' read -ra lines <<< "${rules[$name]}"
    awk -v add="$(printf '%s\n' "${lines[@]}")" '{print} /^store /{print add}' "$r2/export70/world.ratw" > "$dir/world.ratw"
  done
}
run() {   # run HOURS SUFFIX
  local hours="$1" suffix="$2" pids=()
  for name in $names; do
    out="$r2/steer-$name$suffix"
    # shellcheck disable=SC2086
    ( cd "$root" && RATW_DATA_DIR="$tree/Data" taskset -c "${cores[$name]}" "$bin" "$r2/export70-steer-$name" --simulate 6 "$hours" \
        --no-check --deterministic --threads "${threads[$name]:-4}" --no-profile --economy "$out.jsonl" --loop-state "${out}_loop.jsonl" --farms 0 ${extra[$name]:-} \
        --economy-level full --economy-full "$out.full.jsonl.gz" > "$out.out" 2>&1; echo $? > "$out.exit" ) &
    pids+=($!)
  done
  wait "${pids[@]}"
  for name in $names; do
    out="$r2/steer-$name$suffix"
    python3 "$tree/tools/econ_report.py" "$out.jsonl" --full "$out.full.jsonl.gz" --out "$out-report.md"
  done
}
case "${1:-prepare}" in
  prepare) prepare ;;
  smokes) prepare; run 1446 -60 ;;
  years) prepare; run 26298 "" ;;
  *) sed -n 2,8p "$0"; exit 2 ;;
esac
