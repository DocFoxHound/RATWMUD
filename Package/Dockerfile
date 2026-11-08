# The world image (Docs/Design/77, section 8): ratw_server, the NPC Mind, the world agent and the first-start import in
# one container (the server talks to the Mind and the agent reads its /status over 127.0.0.1, so they share a network).
# Built from the staged context only (bash Package/stage.sh --build), never the repository: the context is
#   src/    the server's sources     app/    what runs (Data, migrations, tools, the entrypoint, the LAN page)
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
RUN apt-get update \
 && apt-get install -y --no-install-recommends libpq5 libssl3t64 zlib1g libtcmalloc-minimal4t64 ca-certificates tini \
 && rm -rf /var/lib/apt/lists/* \
 && pip install --no-cache-dir "psycopg[binary]==3.2.13" \
 && groupadd --gid 10001 ratw \
 && useradd --uid 10001 --gid 10001 --home-dir /var/lib/ratw --no-create-home --shell /usr/sbin/nologin ratw \
 && install -d -o ratw -g ratw -m 0700 /var/lib/ratw /var/lib/ratw/logs
COPY --from=build /build/ratw_server /opt/ratw/bin/ratw_server
COPY app/ /opt/ratw/
COPY world/ /package/world/
# The server's Data (Core: RATW_DATA_DIR); the tools' registry and local choices in the container's own state, not the
# repository's (tools/world_db.py); Python writes no __pycache__ into the read-only app.
ENV RATW_DATA_DIR=/opt/ratw/Data \
    RATW_WORLDS_FILE=/var/lib/ratw/config/worlds.json \
    RATW_WORLDS_LOCAL=/var/lib/ratw/config/worlds.local.json \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1
USER ratw
WORKDIR /var/lib/ratw
EXPOSE 7788
ENTRYPOINT ["/usr/bin/tini", "--", "python3", "/opt/ratw/Package/entrypoint.py"]
