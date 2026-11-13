#!/usr/bin/env bash
# Puts this checkout's central on the server that hosts it (Docs/Central-server.md): the code and Data/ of a git
# revision, and the identity checker, then restarts the service. The account, the database, the secrets and Caddy's
# site are set up once by hand (the doc says how); this only updates.
#
#   bash tools/deploy_central.sh [REVISION]        (default: HEAD)
#
# RATW_CENTRAL_SSH   who to log in as (default root@188.68.41.241)
# RATW_IDENTITY_BIN  a built ratw_identity (default: build-core/ratw_identity)
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
revision="${1:-HEAD}"
target="${RATW_CENTRAL_SSH:-root@188.68.41.241}"
identity="${RATW_IDENTITY_BIN:-$root/build-core/ratw_identity}"
[[ -x "$identity" ]] || { echo "No ratw_identity at $identity: cmake --build build-core --target ratw_identity, or set RATW_IDENTITY_BIN." >&2; exit 2; }
short="$(git -C "$root" rev-parse --short "$revision")"
git -C "$root" archive "$revision" Central tools/world_db.py tools/world_sync.py Database/worlds.json Data |
  ssh -o BatchMode=yes "$target" "set -e; rm -rf /opt/ratwmud/app.new; install -d -m 755 /opt/ratwmud/app.new /opt/ratwmud/bin; tar -x -C /opt/ratwmud/app.new; echo $short > /opt/ratwmud/app.new/REVISION; chown -R root:root /opt/ratwmud/app.new; chmod -R go-w /opt/ratwmud/app.new"
scp -q -o BatchMode=yes "$identity" "$target:/opt/ratwmud/bin/ratw_identity.new"
ssh -o BatchMode=yes "$target" 'set -e
  chown root:root /opt/ratwmud/bin/ratw_identity.new; chmod 755 /opt/ratwmud/bin/ratw_identity.new
  mv /opt/ratwmud/bin/ratw_identity.new /opt/ratwmud/bin/ratw_identity
  rm -rf /opt/ratwmud/app.old; [ -d /opt/ratwmud/app ] && mv /opt/ratwmud/app /opt/ratwmud/app.old; mv /opt/ratwmud/app.new /opt/ratwmud/app
  restorecon -R /opt/ratwmud >/dev/null 2>&1 || true
  systemctl restart ratw-central.service; sleep 3; systemctl is-active ratw-central.service
  printf "central at %s: " "$(cat /opt/ratwmud/app/REVISION)"; curl -s -m 5 http://127.0.0.1:7790/health; echo'
