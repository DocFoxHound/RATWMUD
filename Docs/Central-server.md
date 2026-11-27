# Central on its own server

Central (accounts, wolves, tickets and the list of worlds: Docs/Design/77-central-accounts-and-hosted-worlds.md) ran
on the dev server (188.68.41.241) from 2026-11-13 and runs on its own server, **37.27.253.109**, since 2026-11-27.

- **Address:** `https://ratw.37-27-253-109.sslip.io` (try `/health`). An sslip.io name until the game has a domain of
  its own.
- **Server:** 37.27.253.109 (Fedora 44, Hetzner Helsinki, 1 vCPU, 1 GB RAM, 19 GB disk), central's alone. Root logs in by
  key. Caddy's main file trusts 188.68.41.241 as a proxy (below).
- **The old address** `https://ratw.188-68-41-241.sslip.io` (the dev server, which hosts other projects: leave their
  accounts, containers and Caddy sites alone) forwards to the new one (its `/etc/caddy/Caddyfile.d/ratw.caddyfile`), so
  clients up to 0.1.11, whose built-in address it is, keep working. Its old central service and database container are
  stopped and disabled; the volume `ratw-central-pg-data` and `/root/central-final-*.sql` (the last dump) are kept
  there as a backup. Remove the forward once no client uses the old address.
- **Differences from the old setup:** Caddy here uses its default admin endpoint (no `admin unix//run/caddy/...` line).
- **Backups:** nightly, on the same server (section "Backups").
- **Not yet:** doc 77's "Going public" phase
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

## Backups

Nightly at 03:30 UTC (`ratw-central-backup.timer`, which runs `/usr/local/sbin/ratw-central-backup` as root) into
`/var/backups/ratw-central` (root only, 0700), kept 14 days:

- `ratw_central-STAMP.dump`: the database (`pg_dump -Fc`);
- `roles-STAMP.sql`: the database roles (`pg_dumpall --roles-only`);
- `keys-and-secrets-STAMP.tar.gz`: the ticket keys, `pg.env` and `central.env` (losing the keys signs every player out).

They are on the same server, so they cover a broken database or a bad deploy, not the loss of the server. Run one now:
`systemctl start ratw-central-backup`; see the last: `journalctl -u ratw-central-backup -n 5`. To restore (as root):

```
U="runuser -u ratwmud -- env XDG_RUNTIME_DIR=/run/user/1001 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1001/bus HOME=/srv/ratwmud"
systemctl stop ratw-central
cd /srv/ratwmud && $U podman exec -i ratw-central-pg pg_restore -U postgres -d ratw_central --clean --if-exists < /var/backups/ratw-central/ratw_central-STAMP.dump
systemctl start ratw-central
```

(First tested on 2026-11-27: the dump restored into a scratch database with every row.)

## Updating it

From a checkout on the laptop, with `ratw_identity` built (`cmake --build build-core --target ratw_identity`):

```
bash tools/deploy_central.sh            # this checkout's HEAD; or a revision
```

It copies the code, `Data/` and the identity checker, swaps them in (the old code stays as `/opt/ratwmud/app.old`),
restarts the service and prints `/health`. Central migrates its database when it starts.

## Approved worlds (Docs/Design/77, 7.1)

Central approves a community world by itself while its fingerprints match a release it knows. After deploying the
code that has it (migration `0007`, applied when central starts):

- **The release key's public half** goes in `Central/release-keys.json` (the line `bash Package/stage.sh
  --make-release-key` prints, or `key_id`/`key_public` of the existing key). Only public halves, ever.
- **Each world image's release:** `bash Package/stage.sh --build` writes `fingerprints-BUILD.json` (signed) beside its
  context. Commit it to `Central/releases/` and deploy (central imports every file there when it starts), or import it
  at once on the server:

  ```
  ... ratw_central.py releases add /path/to/fingerprints-BUILD.json
  ... ratw_central.py releases list            # and: releases withdraw BUILD --note "why"
  ```
- **Reviewing:** `worlds approval WORLD_ID` (approved, by which release, why not, its last fingerprints);
  `worlds block WORLD_ID --reason "..."` / `worlds unblock WORLD_ID`; `unlocks list [--flagged] [--world ID]
  [--username U]`; `unlocks revoke WORLD_ID USERNAME --note "..."` (kept, marked; the account loses Quickened unless
  another standing unlock gives it) and `unlocks clear WORLD_ID USERNAME` (drops a flag). Run them as the other
  central commands above.

## Using it

- **The desktop client:** this is its built-in address (`Desktop/src-tauri/src/central.rs`, `BUILT_IN`), so an
  installed client uses it with nothing to set. A central on your own computer, for development:
  `--central http://127.0.0.1:7790`.
- **A world:** register it on central (`worlds add`, then `worlds token`), then start the server in ticket mode
  (`--central https://ratw.37-27-253-109.sslip.io --world-id ID`) with the world agent holding the token. Central
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
