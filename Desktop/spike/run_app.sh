#!/usr/bin/env bash
# Phase 0 spike: run the spike client once against spike/ws_server.mjs (start that first), screenshot the window after
# 10 s (KDE's spectacle, the active window), and wait for the page's tests to finish. Extra args are env settings:
#   bash spike/run_app.sh LABEL [VAR=value ...]
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="$here/out"
label="$1"; shift
mkdir -p "$out"
rm -f "$out/$label.jsonl"
env "$@" SPIKE_REPORT="$out/$label.jsonl" SPIKE_QUIT=1 SPIKE_CA="$out/ca.pem" SPIKE_CERT="$out/server.pem" \
  timeout 600 "$here/../src-tauri/target/${SPIKE_PROFILE:-debug}/ratw-desktop" > "$out/$label.stdout" 2>&1 &
pid=$!
sleep 10
spectacle -b -n -a -o "$out/$label.png" >/dev/null 2>&1 || echo "no screenshot"
wait $pid
echo "exit $?"
grep -c . "$out/$label.jsonl" 2>/dev/null
