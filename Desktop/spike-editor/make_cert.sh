#!/usr/bin/env bash
# Doc 78 section 6.4's certificate, made the way the entrypoint will make it, and its pin (the SHA-256 of the SPKI).
# Usage: make_cert.sh OUT_DIR [WORLD_ID]. Writes key.pem, cert.pem, pin.txt; and other-key.pem/other-cert.pem/other-pin.txt
# (a second key, for the "changed key" check).
set -euo pipefail
out=${1:?out dir}; id=${2:-spike-world}
mkdir -p "$out"
for name in "" other-; do
  MSYS_NO_PATHCONV=1 openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -days 36500 \
    -subj "/CN=RATW Editor $id" -keyout "$out/${name}key.pem" -out "$out/${name}cert.pem" 2>/dev/null
  openssl x509 -in "$out/${name}cert.pem" -pubkey -noout | openssl pkey -pubin -outform der | openssl dgst -sha256 -r \
    | cut -d' ' -f1 > "$out/${name}pin.txt"
done
echo "pin $(cat "$out/pin.txt")"
