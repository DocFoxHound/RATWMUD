#!/usr/bin/env bash
# Doc 78, M0a: the peer address a container sees through a published port, from this computer and from another host
# (a network namespace on a veth pair stands in for a computer on the LAN; needs sudo, so the runner, not the laptop),
# and whether each kind of publish answers on IPv6. Publishes: 0.0.0.0:47789, bare 47790 (compose's "P:P"),
# 127.0.0.1:47791 (RATW_EDITOR_BIND's default).
set -u
img=${IMG:-python:3.12-slim}
here=$(cd "$(dirname "$0")" && pwd)
docker version --format 'docker {{.Server.Version}} ({{.Server.Os}})'
docker info --format 'rootless: {{.SecurityOptions}}'
docker pull -q "$img" >/dev/null
docker run -d --name m0a-a -p 0.0.0.0:47789:17789 -v "$here":/p:ro --entrypoint python3 "$img" /p/peer_server.py >/dev/null
docker run -d --name m0a-b -p 47790:17789 -v "$here":/p:ro --entrypoint python3 "$img" /p/peer_server.py >/dev/null
docker run -d --name m0a-c -p 127.0.0.1:47791:17789 -v "$here":/p:ro --entrypoint python3 "$img" /p/peer_server.py >/dev/null
sleep 3
for c in m0a-a m0a-b m0a-c; do echo "$c publishes: $(docker port $c | tr '\n' ' ')"; done
echo "docker-proxy processes: $(pgrep -c docker-proxy || true)"
hostip=$(ip -4 route get 1.1.1.1 | awk '{for (i = 1; i <= NF; i++) if ($i == "src") print $(i + 1)}')
hostip6=$(ip -6 addr show scope global 2>/dev/null | awk '/inet6/ {print $2}' | cut -d/ -f1 | head -1)
ext=0
if sudo -n true 2>/dev/null; then
  sudo ip netns add m0aext && sudo ip link add m0a-h type veth peer name m0a-e && sudo ip link set m0a-e netns m0aext \
  && sudo ip addr add 10.77.0.1/24 dev m0a-h && sudo ip -6 addr add fd77::1/64 dev m0a-h nodad && sudo ip link set m0a-h up \
  && sudo ip netns exec m0aext ip addr add 10.77.0.2/24 dev m0a-e \
  && sudo ip netns exec m0aext ip -6 addr add fd77::2/64 dev m0a-e nodad \
  && sudo ip netns exec m0aext ip link set m0a-e up && sudo ip netns exec m0aext ip link set lo up && ext=1
fi
probe() { local label=$1; shift; local out; if out=$("$@" 2>/dev/null) && [ -n "$out" ]; then echo "$label: $out"; else echo "$label: no answer"; fi; }
for p in 47789 47790 47791; do
  probe "$p from this computer, 127.0.0.1" curl -s -m 3 "http://127.0.0.1:$p/"
  probe "$p from this computer, [::1]" curl -s -m 3 -g "http://[::1]:$p/"
  probe "$p from this computer, its own $hostip" curl -s -m 3 "http://$hostip:$p/"
  [ -n "$hostip6" ] && probe "$p from this computer, its own [$hostip6]" curl -s -m 3 -g "http://[$hostip6]:$p/"
  if [ $ext = 1 ]; then
    probe "$p from another host 10.77.0.2 to 10.77.0.1" sudo ip netns exec m0aext curl -s -m 3 "http://10.77.0.1:$p/"
    probe "$p from another host fd77::2 to [fd77::1]" sudo ip netns exec m0aext curl -s -m 3 -g "http://[fd77::1]:$p/"
  fi
done
docker rm -f m0a-a m0a-b m0a-c >/dev/null
if [ $ext = 1 ]; then sudo ip netns del m0aext; sudo ip link del m0a-h 2>/dev/null; fi
true
