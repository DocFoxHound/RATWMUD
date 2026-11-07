#!/usr/bin/env bash
# Phase 0 spike: sign tickets in Python, check them in C++ (libcrypto via dlopen). Outputs in Desktop/spike/out.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
out="$here/../out/ticket"
mkdir -p "$out"
g++ -std=c++20 -O2 -Wall -Wextra -I"$root/Core" "$here/verify_ticket.cpp" "$root/Core/RatwJsonDoc.cpp" -ldl -o "$out/verify_ticket"
world="world-$(python3 -c 'import secrets; print(secrets.token_hex(8))')"
started=$(( $(date +%s) - 3600 ))   # The world has been up an hour.
python3 "$here/sign_ticket.py" "$out" "$world" "$started"
"$out/verify_ticket" "$out/keys.json" "$world" "$started" "$out/cases.txt" "$out/bulk.txt"
