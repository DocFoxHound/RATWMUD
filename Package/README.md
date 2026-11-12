# Hosting a Ridgemere world

Your own world of Ridgemere, on your own computer, listed on the game's world list.

**The full guide is `Hosting.md`**, beside this file (the game's team sends it with the package; in the game's
repository it is `Docs/Hosting.md`): what you need, every setting, the router's port, carrier-grade NAT, AI voices,
updates, backups, the host's commands, your duties, and a table of what each error means. This page is the short
version.

During the closed beta the game's team sends you this folder, the world image as a file (say `ratw-world.tar`), and
central's address. The folder holds `compose.yaml`, `world.toml`, this file, `Hosting.md`, and two empty folders,
`manage` (where the world puts your Editor account, below) and `backups`. Keep them.

## What you need

- A computer that stays on: Linux (x86_64) with Docker Engine and its Compose plugin, or Windows 10/11 with Docker
  Desktop. No Mac.
- About 250 MB to download, about 1.3 GB of disk to start with, and about 1 GB of memory with nobody playing (more
  with players: not measured yet).
- An account approved as a host. On the client's **Host** page, **MAKE A WORLD**: copy the **host token** (shown once)
  and the world's **id** (`world-...`, in the command shown under the token).
- Port **7788** (TCP) forwarded on your router to this computer.

## Start

1. Unpack this folder anywhere. Open a terminal in it. Load the world image (put its file here first):

       docker load -i ratw-world.tar

2. Edit `world.toml`: the world's name, description, region and tags, and its **id**. Central's `url` holds an
   example address (`https://central.example.org`), which isn't a real central: put in the address the game's team
   gave you. An AI key is optional, and costs you money (the guide, section 10).
3. Make a file called `.env` beside `compose.yaml`, holding the token:

       RATW_HOST_TOKEN=rh_...

   (On Windows, save it as "All files", or Notepad calls it `.env.txt`; the same for any file you make here.) Keep it
   secret. If it leaks, choose **NEW TOKEN** on the Host page, put the new one in `.env`, and type
   `docker compose up -d`.
4. Start it:

       docker compose up -d

5. Watch it: `docker compose logs -f world` (Ctrl+C stops watching, not the world). When the server says it is
   listening and the world agent says `listed on central at ...`, players can see it.

If something in `world.toml` is wrong, the log says what (a line starting `RATW_PACKAGE error:`), and the world doesn't
start until it is put right. Then `docker compose restart world upnp`.

If `docker compose up -d` says **"ports are not available"** (or "address already in use", or "forbidden by its access
permissions"), another program, or Windows itself, holds one of the world's ports. If the message names 17789 (the
Editor's), put another Editor port in `.env`, say `RATW_EDITOR_PORT=17800`; if it names 7788, `RATW_PORT=7790`. Then
`docker compose up -d` again. This can happen on the update that adds the Editor, to a world that ran the day before.

## The Editor: your world's own tools

The game has an **EDITOR** button on its main menu, before you join any world. Through it you run your world: the
DM tool (residents, chapters, players, money, the economy, closing and opening the world). It is yours alone: your
world keeps its own Editor accounts, and nobody else's computer reaches it until you open its port.

**The first time,** on the computer that runs the world:
1. Wait until `manage/first-editor.txt` appears in this folder (a minute or two after the first `up`). It holds your
   Editor account's name (made up, say `host-k7q2`, unless `[editor] owner` in `world.toml` names one), a one-time
   password, and an **Editor code** (`ratw-editor:...`). Keep it to yourself.
2. In the game: **EDITOR**, paste the Editor code. (Or type the address `127.0.0.1`, then the name and the one-time
   password.) Choose your own password. The file then says "Used".
3. The one-time password works for 7 days. If it wasn't used by then, the world writes a new one the next time it
   starts.

Lost your password? On the world's computer, in this folder:

    docker compose exec world ratw-editor list
    docker compose exec world ratw-editor reset NAME

`reset` shows a new Editor code. `manage/editor-certificate.txt` holds the Editor's fingerprint, which the game shows
the first time you connect from a computer without an Editor code: compare them.

**From another computer** (at home, or from outside): the Editor port, 17789, answers only on this computer until you
put this line in `.env`, then type `docker compose up -d`:

    RATW_EDITOR_BIND=0.0.0.0

Then use this computer's address on your home network (say `192.168.1.20`) in the game's EDITOR box. From outside your
home, forward **TCP 17789** on your router to this computer too, as for the game's port, and test it from a phone's
hotspot. Use an Editor code for the first connection from a new computer (`ratw-editor invite NAME` makes one).

**On a rented server (VPS):** Docker opens published ports past the firewall (`ufw` doesn't stop them). With
`RATW_EDITOR_BIND=0.0.0.0` the Editor port faces the internet at once: that is how you manage from home. Use the Editor
code for your first connection, and consider `[editor] new_devices = "approve"` in `world.toml`, so a new computer
needs an Editor code as well as the password.

To turn the Editor off: `[editor] enabled = false` in `world.toml`, then `docker compose restart world`. Nothing then
answers on its port.

## The port

Central checks from the internet that it can reach your world. If it can't, the log says `not listed on central:
Central could not reach this world ...`: forward TCP 7788 on your router to this computer, and allow it through the
firewall (the guide, section 8).

The `upnp` service tries to ask the router to do it for you (`[network] upnp` in `world.toml`; see what happened
with `docker compose logs upnp`). That works only on Linux with Docker Engine, and only if the router allows it.
Under Docker Desktop or rootless Docker, forward the port by hand.

If the log says `This world's address (IP) is not on the internet.`, or UPnP says the router's address is in
`100.64.0.0/10`, your connection is behind carrier-grade NAT, and no port can be opened at home. Ask your provider for
a public IPv4 address, run the world on a rented server, or use a tunnel with port forwarding (the guide, section 9).

## Every day

- Stop: `docker compose stop` (the world saves first, and leaves the list). Start again: `docker compose start`.
- After editing `world.toml` (or a token or key file it names): `docker compose restart world upnp`, or Restart in
  the Editor (the world reads `world.toml` before each start). After editing `.env`: `docker compose up -d` (a restart
  doesn't read `.env` again).
- If the world's server stops on its own, the world starts it again (after 5 seconds, then waiting longer each time),
  and the Editor stays up throughout.
- Logs: `docker compose logs world`. The NPC Mind's and the agent's: `docker compose exec world ls logs`.
- The host's commands in the game: press the ` key for the host's console, then `/help` (`/kick`, `/ban`,
  `/silence`, `/pending` and the rest). They need the host token: central tells the world who its host is.

## Update

Load the new image file from the game's team, then start the world on it:

    docker load -i ratw-world.tar
    docker compose up -d

(Once the image is published, `docker compose pull` then `docker compose up -d`.) Your world is kept, with everything
that has happened in it. A new package's world is only for new worlds. A world on an older game version stays on the
list, but players with a newer client see "This world is older than your client: its host needs to update it" and
can't join, so update when a new version comes out.

## Backups

There is no backup command yet. The guide (section 12) gives the commands to copy the world's database and its
passwords to a file while the world is stopped, and to put them back.

## Settings beside compose.yaml (`.env`)

- `RATW_PORT`: the port (default 7788), a number from 1 to 65535. It is the same inside the container and out.
- `RATW_HOST_TOKEN`: the host token (or a file of its own, named by `token_file` in `world.toml`).
- `RATW_IMAGE`: the world image, if you were given another one.
- `COMPOSE_PROJECT_NAME`: only for a second world on this computer (below).
- `RATW_EDITOR_PORT`: the Editor port (default 17789).
- `RATW_EDITOR_BIND`: where the Editor answers: `127.0.0.1` (the default: this computer only) or `0.0.0.0` (every
  address of this computer, so other computers can reach it: above).

## A second world on this computer

Make a second world on the Host page. Unpack the package again into another folder, give that folder's `world.toml`
the second id, and in its `.env` a name and two ports of its own (the game's and the Editor's):

    COMPOSE_PROJECT_NAME=ratw-second
    RATW_PORT=7789
    RATW_EDITOR_PORT=17790
    RATW_HOST_TOKEN=rh_...

Without them it would take over the first world's containers and its database, or fail with "ports are not
available". (The first world needs nothing: its
name is `ratw-world` wherever its folder is, so moving or renaming the folder keeps the world.)

Every world signs its players in through central. A private world, or one for your own network alone, isn't offered
yet.

## Starting again from scratch

    docker compose down -v

deletes the world, its database, its passwords and its Editor accounts and certificate for good (every computer that
signed in to its Editor then warns that its key changed). Make a backup first. The next `up` makes the world afresh
from the package in the image you have now.

## Building the image (developers)

From the repository: `bash Package/stage.sh --world EXPORT_DIR --build`, where `EXPORT_DIR` is made by
`python3 tools/world_package.py export --world KEY --out EXPORT_DIR`. The script copies exactly what the image needs
into `build-package/context` (never build from the repository's root: it is about 90 GB) and tags `ratw-world:latest`
(`--tag NAME` for another name). `RATW_PACKAGE_IMPORT=skip` in `.env` starts an image built without a world export
(the migrations only; the server then says the database has no world build).

The folder to send a host: `python3 Package/host_folder.py --out DIR [--zip DIR.zip]` (this file, `compose.yaml`,
`world.toml`, `Hosting.md`, and the empty `manage` and `backups` folders, which a zip keeps); the image beside it, from
`docker save ratw-world:latest -o ratw-world.tar`.
