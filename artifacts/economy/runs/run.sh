#!/usr/bin/env bash
# run.sh NAME DAYS BINARY [econ_watch options]: an economy run (tools/econ_run.sh, 3 threads) with a watchdog that ends it
# on a sustained famine (collapse.py), noting why in its folder's COLLAPSED. Prints the folder.
set -u
root=/home/martinb/Documents/GitHub/MUDProject; here="$(cd "$(dirname "$0")" && pwd)"
name=$1; days=$2; bin=$3; shift 3
out="$root/artifacts/economy/$(date +%Y-%m-%d)-$name"
ECON_WATCH="$bin" bash "$root/tools/econ_run.sh" "$name" "$days" --threads 3 "$@" > "$here/$name.console" 2>&1 &
pid=$!
while kill -0 $pid 2>/dev/null; do
  if [[ -f "$out/days.jsonl" ]] && why=$(python3 -I "$here/collapse.py" "$out/days.jsonl"); then
    echo "$why" > "$out/COLLAPSED"
    pkill -f "econ_watch.*$out" ; break
  fi
  sleep 120
done
wait $pid
echo "$out $(cat "$out/COLLAPSED" 2>/dev/null)"
