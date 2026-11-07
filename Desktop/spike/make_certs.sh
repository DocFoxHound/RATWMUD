#!/usr/bin/env bash
# Phase 0 spike: a throwaway CA and a certificate for 127.0.0.1/localhost, in Desktop/spike/out (gitignored).
set -euo pipefail
out="$(cd "$(dirname "$0")" && pwd)/out"
mkdir -p "$out"
cd "$out"
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -days 7 -subj "/CN=RATW spike CA" \
  -keyout ca.key -out ca.pem -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign" 2>/dev/null
openssl req -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -subj "/CN=localhost" -keyout server.key -out server.csr 2>/dev/null
printf 'subjectAltName=IP:127.0.0.1,DNS:localhost\nbasicConstraints=CA:FALSE\nextendedKeyUsage=serverAuth\n' > ext.cnf
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca.key -CAcreateserial -days 7 -extfile ext.cnf -out server.pem 2>/dev/null
echo "made $out/ca.pem and $out/server.pem"
