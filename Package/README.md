# Hosting a Ridgemere world

Your own world of Ridgemere, on your own computer, listed on the game's server list (Docs/Design/77, section 8).

## What you need

- A computer that stays on: Linux (x86_64), or Windows with Docker Desktop.
- **Docker** with Compose: on Linux, Docker Engine and its compose plugin (https://docs.docker.com/engine/install/);
  on Windows, Docker Desktop (https://docs.docker.com/desktop/setup/install/windows-install/).
- Port **7788** (TCP) open on your router to this computer, so players and central can reach the world.
- A host token: sign in on the game's **Host** page, make a world, and copy its **id** and its **host token**.

What a world needs to run (memory, cores, disk) hasn't been measured yet. The world saves itself by forking, which can
briefly need up to twice its memory.

## Start

1. Unpack this folder anywhere. It holds `compose.yaml`, `world.toml` and this file.
2. Edit `world.toml`: the world's name, description, region and tags, its **id** from the Host page, and (optionally)
   an AI key.
3. Put the host token in a file called `.env` beside `compose.yaml`:

       RATW_HOST_TOKEN=rh_...

   (Or in a file of its own, named by `token_file` in `world.toml`.) Keep it secret: anyone with it can list a world
   as yours. If it leaks, make a new one on the Host page. The files in this folder may be private to you
   (`chmod 600`): the container reads them as root, and the world server itself can't read them.
4. Start it:

       docker compose up -d

   The first start downloads the images (a few hundred MB), makes the database and imports the world, which takes a
   minute or two. See how it is going with `docker compose logs -f world`. When the server says it is listening and
   the world agent says "listed on central", players can see it. If something in `world.toml` is wrong, the log says
   what, and the world doesn't start until it is put right (then `docker compose up -d` again).

If central says "Your world can't be reached from the internet", open port 7788 on your router (TCP, to this
computer's address on your network). Behind some home connections (carrier-grade NAT) that isn't possible; a tunnel
is the way then.

## Every day

- Stop: `docker compose stop` (the world saves first). Start again: `docker compose start`.
- After editing `world.toml`: `docker compose restart world`.
- Logs: `docker compose logs world`; the Mind's and the agent's logs are in the `logs` volume
  (`docker compose exec world ls logs`).
- Moderating: the host's commands in the game (`/kick`, `/ban`, `/silence`, `/pending` and the rest). They need the
  host token: central tells the world who its host is.

## Update

    docker compose pull
    docker compose up -d

Your world (the database volume) is kept, with everything that has happened in it: the new image updates the server,
and the database's layout if it needs to, but its starting world (its package) is only for a new world. The log then
says which package your world was started from and which the image carries. To start afresh from the new one, see
"Starting again from scratch" below. A world whose game version is too old drops off the server list, so update when a
new version comes out.

## Settings beside compose.yaml (`.env`)

- `RATW_PORT`: the port (default 7788). It is the same inside the container and out.
- `RATW_HOST_TOKEN`: the host token.
- `RATW_IMAGE`: the world image, if you were given another one.
- `COMPOSE_PROJECT_NAME`: only for a second world on this computer (below).

## A second world on this computer

Unpack the package again into another folder, and in that folder's `.env` give it a name and a port of its own:

    COMPOSE_PROJECT_NAME=ratw-second
    RATW_PORT=7789

Without them it would take over the first world's containers and its database. (The first world needs nothing: its
name is `ratw-world` wherever its folder is, so moving or renaming the folder keeps the world.)

Every world signs its players in through central; a private world or one for your own network alone isn't offered
yet.

## Starting again from scratch

    docker compose down -v

deletes the world, its database and its passwords for good. The next `up` imports the world afresh, from the
package in the image you have now.

## Building the image (developers)

From the repository: `bash Package/stage.sh --world EXPORT_DIR --build`, where `EXPORT_DIR` is made by
`python3 tools/world_package.py export --world KEY --out EXPORT_DIR`. The script copies exactly what the image needs
into `build-package/context` (never build from the repository's root: it is about 90 GB) and tags `ratw-world:latest`.
`RATW_PACKAGE_IMPORT=skip` in `.env` starts an image built without a world export (the migrations only; the server
then says the database has no world build).
