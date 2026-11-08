# Hosting a Ridgemere world

Your own world of Ridgemere, on your own computer, listed on the game's world list.

**The full guide is `Docs/Hosting.md`**, in the game's repository: what you need, every
setting, the router's port, carrier-grade NAT, AI voices, updates, backups, the host's commands, your duties, and a
table of what each error means. This page is the short version.

## What you need

- A computer that stays on: Linux (x86_64) with Docker Engine and its Compose plugin, or Windows 10/11 with Docker
  Desktop. No Mac.
- About 250 MB to download, about 1.3 GB of disk to start with, and about 1 GB of memory with nobody playing (more
  with players: not measured yet).
- An account approved as a host. On the client's **Host** page, **MAKE A WORLD**: copy the **host token** (shown once)
  and the world's **id** (`world-...`, in the command shown under the token).
- Port **7788** (TCP) forwarded on your router to this computer.

## Start

1. Unpack this folder anywhere. Open a terminal in it.
2. Edit `world.toml`: the world's name, description, region and tags, and its **id**. Leave central's `url` as it
   is. An AI key is optional, and costs you money (the guide, section 10).
3. Make a file called `.env` beside `compose.yaml`, holding the token:

       RATW_HOST_TOKEN=rh_...

   (On Windows, save it as "All files", or Notepad calls it `.env.txt`.) Keep it secret. If it leaks, choose **NEW
   TOKEN** on the Host page.
4. Start it:

       docker compose up -d

5. Watch it: `docker compose logs -f world` (Ctrl+C stops watching, not the world). When the server says it is
   listening and the world agent says `listed on central at ...`, players can see it.

If something in `world.toml` is wrong, the log says what (a line starting `RATW_PACKAGE error:`), and the world doesn't
start until it is put right. Then `docker compose up -d` again.

## The port

Central checks from the internet that it can reach your world. If it can't, the log says `not listed on central:
Central could not reach this world ...`: forward TCP 7788 on your router to this computer, and allow it through the
firewall (the guide, section 8).

The `upnp` service tries to ask the router to do it for you (`[network] upnp` in `world.toml`; see what happened
with `docker compose logs upnp`). That works only on Linux with Docker Engine, and only if the router allows it.
Under Docker Desktop or rootless Docker, forward the port by hand.

If the log says `This world's address (IP) is not on the internet.`, or UPnP says the router's address is in
`100.64.0.0/10`, your connection is behind carrier-grade NAT, and no port can be opened at home. Ask your provider for
a public IPv4 address, or run the world on a rented server (the guide, section 9).

## Every day

- Stop: `docker compose stop` (the world saves first, and leaves the list). Start again: `docker compose start`.
- After editing `world.toml`: `docker compose restart world`.
- Logs: `docker compose logs world`. The NPC Mind's and the agent's: `docker compose exec world ls logs`.
- The host's commands in the game: press the ` key for the host's console, then `/help` (`/kick`, `/ban`,
  `/silence`, `/pending` and the rest). They need the host token: central tells the world who its host is.

## Update

    docker compose pull
    docker compose up -d

Your world is kept, with everything that has happened in it. A new package's world is only for new worlds. A world
whose game version is too old drops off the list, so update when a new version comes out. (Until the world image is
published, `pull` can't fetch it: the guide says what to do.)

## Backups

There is no backup command yet. The guide (section 12) gives the commands to copy the world's database and its
passwords to a file while the world is stopped, and to put them back.

## Settings beside compose.yaml (`.env`)

- `RATW_PORT`: the port (default 7788). It is the same inside the container and out.
- `RATW_HOST_TOKEN`: the host token (or a file of its own, named by `token_file` in `world.toml`).
- `RATW_IMAGE`: the world image, if you were given another one.
- `COMPOSE_PROJECT_NAME`: only for a second world on this computer (below).

## A second world on this computer

Make a second world on the Host page. Unpack the package again into another folder, give that folder's `world.toml`
the second id, and in its `.env` a name and a port of its own:

    COMPOSE_PROJECT_NAME=ratw-second
    RATW_PORT=7789
    RATW_HOST_TOKEN=rh_...

Without them it would take over the first world's containers and its database. (The first world needs nothing: its
name is `ratw-world` wherever its folder is, so moving or renaming the folder keeps the world.)

Every world signs its players in through central. A private world, or one for your own network alone, isn't offered
yet.

## Starting again from scratch

    docker compose down -v

deletes the world, its database and its passwords for good. Make a backup first. The next `up` makes the world afresh
from the package in the image you have now.

## Building the image (developers)

From the repository: `bash Package/stage.sh --world EXPORT_DIR --build`, where `EXPORT_DIR` is made by
`python3 tools/world_package.py export --world KEY --out EXPORT_DIR`. The script copies exactly what the image needs
into `build-package/context` (never build from the repository's root: it is about 90 GB) and tags `ratw-world:latest`
(`--tag NAME` for another name). `RATW_PACKAGE_IMPORT=skip` in `.env` starts an image built without a world export
(the migrations only; the server then says the database has no world build).
