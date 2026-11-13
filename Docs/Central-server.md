# Central on the dev server

Central (accounts, wolves, tickets and the list of worlds: Docs/Design/77-central-accounts-and-hosted-worlds.md) runs
on the dev server since 2026-11-13.

- **Address:** `https://ratw.188-68-41-241.sslip.io` (try `/health`). An sslip.io name, as the server's other sites
  use, until the game has a domain of its own.
- **Server:** 188.68.41.241 (Fedora 44). It hosts other projects too: leave their accounts, containers and Caddy
  sites alone.
- **Not yet:** backups of central's database, the address built into the client, and doc 77's "Going public" phase
  (the legal pages, the age gate's page, the official world beside it).

## What is where

| What | Where | Notes |
|---|---|---|
| The account it runs as | `ratwmud` (home `/srv/ratwmud`, no login shell, lingering on) | never root |
| The service | `ratw-central.service` (`/etc/systemd/system/`) | `User=ratwmud`, hardened; answers on `127.0.0.1:7790` only |
| The code | `/opt/ratwmud/app` (root-owned; `REVISION` says which commit) | `Central/`, `tools/world_db.py`, `tools/world_sync.py`, `Database/worlds.json`, `Data/` |
| The identity checker | `/opt/ratwmud/bin/ratw_identity` | built on the laptop; needs only libc and libstdc++ |
| Python | `/opt/ratwmud/venv` | `psycopg[binary]`, `cryptography` |
| Secrets | `/etc/ratwmud/central.env` (root:ratwmud, 0640) | the database passwords, generated on the server; never in git |
| Ticket keys | `/srv/ratwmud/central/keys` (0700) | made at the first start; losing them signs every player out |
| The database | Postgres 18 in a rootless podman container of `ratwmud`: `ratw-central-pg`, `127.0.0.1:5435`, volume `ratw-central-pg-data` | `~ratwmud/.config/containers/systemd/ratw-central-pg.container`; its password in `/srv/ratwmud/central/pg.env` |
| HTTPS | Caddy, `/etc/caddy/Caddyfile.d/ratw.caddyfile` | a reverse proxy to `127.0.0.1:7790`; central runs with `--trust-proxy` |

## Everyday commands (as root on the server)

```
systemctl status ratw-central          # is it up
journalctl -u ratw-central -f          # its log
systemctl restart ratw-central
curl -s http://127.0.0.1:7790/health
```

Central's own commands (worlds, host tokens, account flags, keys) run as `ratwmud` with its settings:

```
cd /srv/ratwmud && runuser -u ratwmud -- env $(grep -v '^#' /etc/ratwmud/central.env | xargs) HOME=/srv/ratwmud \
  /opt/ratwmud/venv/bin/python /opt/ratwmud/app/Central/ratw_central.py worlds list
```

The database container belongs to `ratwmud`'s own service manager (the account has no login shell, so `systemctl
--user -M ratwmud@` doesn't work; `podman exec` from `runuser` doesn't either):

```
cd /srv/ratwmud && runuser -u ratwmud -- env XDG_RUNTIME_DIR=/run/user/1001 \
  DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1001/bus HOME=/srv/ratwmud systemctl --user status ratw-central-pg
```

## Updating it

From a checkout on the laptop, with `ratw_identity` built (`cmake --build build-core --target ratw_identity`):

```
bash tools/deploy_central.sh            # this checkout's HEAD; or a revision
```

It copies the code, `Data/` and the identity checker, swaps them in (the old code stays as `/opt/ratwmud/app.old`),
restarts the service and prints `/health`. Central migrates its database when it starts.

## Using it

- **The desktop client:** start it with `--central https://ratw.188-68-41-241.sslip.io` (its built-in address is
  still `http://127.0.0.1:7790`, `Desktop/src-tauri/src/central.rs`, `BUILT_IN`).
- **A world:** register it on central (`worlds add`, then `worlds token`), then start the server in ticket mode
  (`--central https://ratw.188-68-41-241.sslip.io --world-id ID`) with the world agent holding the token. Central
  lists a world only when it can call the world back on a public address.
- **Anyone who has the address can register an account.** Nothing else limits sign-ups yet.

## How it was set up (once, by hand, as root)

1. `useradd --create-home --home-dir /srv/ratwmud --shell /sbin/nologin ratwmud`; `loginctl enable-linger ratwmud`;
   folders `/opt/ratwmud`, `/etc/ratwmud` (root:ratwmud 0750), `/srv/ratwmud/central/keys` (0700).
2. `/etc/ratwmud/central.env` with generated `POSTGRES_PASSWORD` and `RATW_OWNER_PASSWORD`, `RATW_DB_HOST=127.0.0.1`,
   `RATW_DB_PORT=5435`, `RATW_CENTRAL_DBNAME=ratw_central`, `RATW_CENTRAL_ROLE=owner`,
   `RATW_IDENTITY=/opt/ratwmud/bin/ratw_identity`, `RATW_DATA_DIR=/opt/ratwmud/app/Data`.
3. The Postgres container's unit and `pg.env`, started through `ratwmud`'s service manager.
4. The code, the checker and the venv; the role `ratw_owner` made over the local connection; then
   `ratw_central.py db-create` and `migrate`.
5. `ratw-central.service`, enabled and started; Caddy's site file, `caddy validate`, `systemctl reload caddy`.
