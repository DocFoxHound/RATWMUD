# Hosting your own world of Run Against the World

Your own world of Ridgemere, on your own computer, listed on the game's world list, run with its own tools from the
game's EDITOR button.

**The full guide is `Hosting.md`**, beside this file (in the game's repository, `Docs/Hosting.md`). It has every step
in full, Windows first, and a table of what each message means (section 27). This page is the short version. Section
numbers below are the guide's.

This folder holds `compose.yaml`, `world.toml`, this file, `Hosting.md`, four scripts to double-click (`restore` and
`reset-editor-password`, each as `.bat` for Windows and `.sh` for Linux), and two empty folders: `manage` (where the
world puts your Editor account) and `backups` (where it puts its backups). Keep them all. The world image comes beside
it as a file (say `ratw-world.tar`).

## What you need (section 1)

- A computer that stays on: Windows 10 or 11 with Docker Desktop, or Linux (x86_64) with Docker Engine and its Compose
  plugin. No Mac.
- About 250 MB to download, about 1.5 GB of disk to start with (and 2 GB free beyond that for backups), and about
  1 GB of memory with nobody playing.
- A game account approved as a host, and the game's desktop client.
- Port **7788** (TCP) forwarded on your router to this computer.

## Start (sections 2-7)

1. In the game: **HOST A WORLD**, **MAKE A WORLD**. Copy the lines it shows: the `.env` line with your **host token**
   (shown once) and the `world.toml` lines with your world's **id** and central's address.
2. Unpack this folder somewhere of your own (say `C:\RATW\my-world`; not in OneDrive or another synced folder). Open
   a terminal in it (Windows 11: right-click inside the folder, **Open in Terminal**). Load the world image:

       docker load -i ratw-world.tar

3. Edit `world.toml` in Notepad: the world's name, description, region and tags, its `id` and central's `url`.
4. Make a file called `.env` beside `compose.yaml`, holding the token line:

       RATW_HOST_TOKEN=rh_...

   On Windows, save it as **Save as type: All files**, or Notepad calls it `.env.txt`. Keep the token secret.
5. Start the world:

       docker compose up -d

6. Watch it: `docker compose logs -f world` (Ctrl+C stops watching, not the world). When the server says
   `RATW_SERVER RATW server listening ...` and the agent says `RATW_AGENT world agent: listed on central at ...`,
   players can see it.

If something in `world.toml` is wrong, the log says what (a line starting `RATW_PACKAGE error:`), and the world waits
until it is put right: it reads `world.toml` again by itself, or at once with `docker compose restart world upnp`.

If `docker compose up -d` says **"ports are not available"** (or "port is already allocated", or "forbidden by its
access permissions"), another program, or Windows itself, holds one of the world's ports. If the message names 17789
(the Editor's), put `RATW_EDITOR_PORT=17800` in `.env`; if it names 7788, `RATW_PORT=7790`. Then type
`docker compose down` and `docker compose up -d` (your world is kept; a plain `up -d` after a refused one can leave the
world off its network, "waiting for the database"). This can happen on the update that adds the Editor, to a world
that ran the day before, often a second world without its own Editor port (section 27).

## The port (sections 8-9)

Central checks from the internet that it can reach your world. If it can't, the log says `not listed on central:
Central could not reach this world ...`: forward TCP 7788 on your router to this computer, and allow it through the
firewall. The `upnp` part tries to ask the router for you, but only on Linux with Docker Engine; under Docker Desktop
or rootless Docker, forward it by hand. If the log says `This world's address (IP) is not on the internet.`, your
connection is behind carrier-grade NAT: ask your provider for a public IPv4 address, or rent a server.

## Your world's Editor: the first sign-in (sections 10-11)

1. Wait until `manage/first-editor.txt` appears in this folder (a minute or two after the first start).
2. Open it. Copy the **Editor code**: the long line starting `ratw-editor:`. Keep the file to yourself.
3. In the game's main menu: **EDITOR**, paste the code into **HAVE AN EDITOR CODE? PASTE IT**, **USE THE CODE**, then
   **SIGN IN**.
4. Choose your own password: at least 12 characters, without your account's or your world's name, not a common one,
   and used for this world only. The file then says "Used".
5. The Editor menu: **DM** opens the Dungeon Master tool. Its **My world** tab closes, opens and restarts the world,
   and holds its backups.

The code works for 7 days; after that the world writes a new one the next time it starts.

## Managing from another computer (section 12)

The Editor port, 17789, answers only on this computer until you put this line in `.env`, then type
`docker compose up -d`:

    RATW_EDITOR_BIND=0.0.0.0

- **At home:** use this computer's home address (`ipconfig`, or `hostname -I` on Linux; say `192.168.1.20`) in the
  game's EDITOR box. On Windows, set your network to **Private** (Settings, Network & internet).
- **From outside your home:** only if you need it, forward **TCP 17789** on your router to this computer too, and test
  it from a phone's hotspot, not from inside your home.
- Use an Editor code for the first connection from a new computer:
  `docker compose exec world ratw-editor invite NAME --address 192.168.1.20:17789`.

**On a rented server (VPS):** Docker opens published ports past the firewall (`ufw` doesn't stop them). With
`RATW_EDITOR_BIND=0.0.0.0` the Editor port faces the internet at once: that is how you manage it from home. Use the
Editor code for your first connection, and set `new_devices = "approve"` under `[editor]` in `world.toml`.

To turn the Editor off: `[editor] enabled = false` in `world.toml`, then `docker compose restart world`. (Docker still
holds the port.)

## The fingerprint (section 13)

`manage/editor-certificate.txt` holds your Editor's fingerprint (eight groups of four characters). The game shows it
the first time you connect from a computer without an Editor code: compare them. If the game ever shows **"STOP: THE
CERTIFICATE HAS CHANGED"**, don't sign in: section 13 says when that is expected and what to do.

## Co-builders (sections 14-15)

On the Editor menu, **EDITOR ACCOUNTS** (your password is asked again), **ADD A CO-BUILDER**: a name and a role
(`viewer` watches, `dm` runs events and NPCs, `builder` also the map later). The game shows their **Editor code**
once: pass it on privately, never in a group chat. It works once, for 72 hours. They need the game's desktop client
and your Editor port open to them. Owners are made only here: `docker compose exec world ratw-editor add NAME --role
owner`.

## A lost password (section 16)

On the world's computer, while the world runs, double-click `reset-editor-password.bat` (Windows) or run
`./reset-editor-password.sh` in this folder. It lists your Editor accounts, asks which one, and shows a new one-time
password and Editor code (also written into `manage/first-editor.txt`). The same, typed:

    docker compose exec world ratw-editor list
    docker compose exec world ratw-editor reset NAME

## Backups and restore (section 19)

The world backs itself up into `backups/`: when it starts and every day, before an update, and when you ask (My world,
**Back up now**). It keeps the 7 newest daily ones, the 5 newest of the rest, and up to 5 you pin (90 days each). A
backup that would leave less than 2 GB free is refused (`RATW_BACKUP_MIN_FREE_MB` in `.env` changes that).
`docker compose run --rm backup list` lists them.

**To put the world back:** double-click `restore.bat` (Windows) or run `./restore.sh`. It lists the backups, asks
which one, says what you lose ("Everything players did since then is lost") and asks you to type RESTORE. It asks
whether to keep your Editor accounts as they are now (press Enter) or take the backup's (B). Then it stops the world,
makes a safety copy, puts the backup back and starts the world again.

**Keep `backups/` private:** the backups hold players' messages and the Editor accounts' password hashes.

## Every day (sections 21, 25)

- Stop: `docker compose stop` (it saves first, and leaves the list). Start again: `docker compose start`.
- After editing `world.toml`: `docker compose restart world upnp`, or My world's **Restart the world**. After editing
  `.env`: `docker compose up -d`.
- If the world's server stops on its own, the world starts it again, and the Editor stays up throughout.
- Logs: `docker compose logs world`, `docker compose logs backup`.
- The host's commands in the game: press the ` key, then `/help`.

## Updates (section 20)

Load the new image file, then start the world on it:

    docker load -i ratw-world.tar
    docker compose up -d

Your world is kept, with its Editor accounts and fingerprint; a backup is made first. If the update brings a new
`compose.yaml` or scripts, copy them over the old ones, keeping `world.toml`, `.env`, `manage` and `backups`. A world
from before the Editor also needs empty `manage` and `backups` folders, and a second world on this computer an Editor
port of its own (`RATW_EDITOR_PORT=17790` in its `.env`) before it updates (section 20).

## A second world on this computer (section 22)

Unpack the package again into another folder, give its `world.toml` the second world's id, and in its `.env` a name
and two ports of its own:

    COMPOSE_PROJECT_NAME=ratw-second
    RATW_PORT=7789
    RATW_EDITOR_PORT=17790
    RATW_HOST_TOKEN=rh_...

In the game's EDITOR box, the second world is `127.0.0.1:17790`.

## Never zip this folder to send it (section 24)

Once the world has run, this folder holds your host token (`.env`), an Editor code (`manage/first-editor.txt`) and
players' data (`backups/`). Send a friend the package from the game's team instead.

## Starting again from scratch (section 23)

    docker compose down -v

deletes the world, its database, its passwords and its Editor accounts and key for good. Make a backup first.

## Building the image (developers)

From the repository: `bash Package/stage.sh --world EXPORT_DIR --build`, where `EXPORT_DIR` comes from
`python3 tools/world_package.py export --world KEY --out EXPORT_DIR`. It stages into `build-package/context` (never
build from the repository's root) and tags `ratw-world:latest` (`--tag NAME` for another). The Editor bundle is signed
with the release key (`bash Package/stage.sh --make-release-key`, once; `--no-editor` stages without it). The folder to
send a host: `python3 Package/host_folder.py --out DIR [--zip DIR.zip]`; the image beside it from
`docker save ratw-world:latest -o ratw-world.tar`.
