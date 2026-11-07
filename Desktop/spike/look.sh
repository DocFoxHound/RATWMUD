#!/usr/bin/env bash
# Phase 0 spike: open the spike client with some env settings, screenshot its window after 8 s, close it.
#   bash spike/look.sh LABEL [VAR=value ...]
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="$here/out"; label="$1"; shift; mkdir -p "$out"
env "$@" SPIKE_REPORT="$out/$label.jsonl" "$here/../src-tauri/target/${SPIKE_PROFILE:-debug}/ratw-desktop" > "$out/$label.stdout" 2>&1 &
pid=$!
sleep 8
spectacle -b -n -a -o "$out/$label.png" >/dev/null 2>&1 || echo "no screenshot"
kill $pid; wait $pid 2>/dev/null
grep canvas_idle "$out/$label.jsonl"; grep -v '^SPIKE' "$out/$label.stdout" | head -5
