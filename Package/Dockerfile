# The world image (Docs/Design/77, section 8): ratw_server, the NPC Mind, the world agent and the first-start import in
# one container (the server talks to the Mind and the agent reads its /status over 127.0.0.1, so they share a network).
# Built from the staged context only (bash Package/stage.sh --build), never the repository: the context is
#   src/    the server's sources     app/    what runs (Data, migrations, tools, the entrypoint)
#   world/  the world export (tools/world_package.py export), imported into the world's database on first start

# ------------------------------------------------------------------------------------------------------------ build
FROM debian:trixie AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential cmake git libgoogle-perftools-dev \
 && rm -rf /var/lib/apt/lists/*
# The build id the server reports (/status, the listing): there is no .git here, so it is given (stage.sh).
ARG RATW_BUILD_ID=unknown
COPY src/ /src/
RUN cmake -S /src -B /build -DCMAKE_BUILD_TYPE=Release -DRATW_BUILD_ID="${RATW_BUILD_ID}" \
 && cmake --build /build --target ratw_server -j"$(nproc)" \
 && strip /build/ratw_server \
 && ldd /build/ratw_server | grep -q tcmalloc_minimal

# ---------------------------------------------------------------------------------------------------------- runtime
FROM python:3.14-slim-trixie
# libssl3t64 and zlib1g are loaded by the server at start (it refuses to start without them), libpq5 in database mode,
# tcmalloc is linked; CA certificates for the Mind's and the agent's HTTPS; tini as PID 1 (signals, zombies).
# Two users (Package/entrypoint.py): ratw (10001) runs the import, the NPC Mind and the world agent, each given only
# its secrets in its environment; ratw-server (10002) runs the server, which faces the internet, and can reach none of
# them. ratw-admin (10003, in no other group) runs the Editor's helper (tools/world_admin.py, Docs/Design/78, 6.5),
# with openssl to make and sign the Editor's certificate. The entrypoint runs as root to read the host's files
# (mounted in /etc/ratw) and the database passwords (compose.yaml), all root's alone.
RUN apt-get update \
 && apt-get install -y --no-install-recommends libpq5 libssl3t64 zlib1g libtcmalloc-minimal4t64 ca-certificates tini \
    openssl \
 && rm -rf /var/lib/apt/lists/* \
 && pip install --no-cache-dir "psycopg[binary]==3.2.13" \
 && groupadd --gid 10001 ratw \
 && useradd --uid 10001 --gid 10001 --home-dir /var/lib/ratw --no-create-home --shell /usr/sbin/nologin ratw \
 && groupadd --gid 10002 ratw-server \
 && useradd --uid 10002 --gid 10002 --home-dir /var/lib/ratw --no-create-home --shell /usr/sbin/nologin ratw-server \
 && groupadd --gid 10003 ratw-admin \
 && useradd --uid 10003 --gid 10003 --home-dir /var/lib/ratw/admin --no-create-home --shell /usr/sbin/nologin ratw-admin \
 && install -d -o root -g root -m 0755 /var/lib/ratw \
 && install -d -o ratw -g ratw-server -m 0770 /var/lib/ratw/logs \
 && install -d -o root -g root -m 0700 /etc/ratw /etc/ratw/host
COPY --from=build /build/ratw_server /opt/ratw/bin/ratw_server
COPY app/ /opt/ratw/
# The Editor's accounts command (Docs/Design/78, 3.6): `docker compose exec world ratw-editor ...`.
RUN install -m 0755 /opt/ratw/Package/ratw-editor /usr/local/bin/ratw-editor
COPY world/ /package/world/
# The server's Data (Core: RATW_DATA_DIR); the tools' registry and local choices in the container's own state, not the
# repository's (tools/world_db.py); Python writes no __pycache__ into the read-only app.
ENV RATW_DATA_DIR=/opt/ratw/Data \
    RATW_WORLDS_FILE=/var/lib/ratw/config/worlds.json \
    RATW_WORLDS_LOCAL=/var/lib/ratw/config/worlds.local.json \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1
WORKDIR /var/lib/ratw
EXPOSE 7788
ENTRYPOINT ["/usr/bin/tini", "--", "python3", "/opt/ratw/Package/entrypoint.py"]
