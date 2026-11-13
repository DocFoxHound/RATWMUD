# Hosting your own world of Run Against the World

This guide is for anyone who wants to run their own world of Ridgemere, the world of Run Against the World, on their
own computer. Other players join it from the game's world list. You run it with your world's own tools, the
**Editor**, from the game's main menu.

You don't need to be technical. You will type a few commands. Each one is given in full. Windows comes first in every
step; Linux is beside it.

A few words this guide uses:
- **Your world** runs on your computer: the map, the residents, their stories and their economy. It is yours. What
  happens in it stays in it.
- **Central** is the game's own service. It holds players' accounts and wolves, signs them in, and keeps the world
  list. Your world tells central every minute that it is up, and central lists it.
- **A wolf** is a player's character. Its name and look come from central. What it does in your world (its money,
  goods, home, friends) stays in your world.
- **The world folder** is the folder you unpack the package into (section 4). Every command in this guide is typed in
  it.
- **The Editor** is your world's own tools: the Dungeon Master tool (the DM tool) now, and a map editor later. You
  reach it with the **EDITOR** button on the game's main menu (section 10).

Where things stand:
- This is the first version. During the closed beta, an account can host only once the game's team has approved it,
  and the team sends you the package and the world image (section 4).
- It has been tested on a Fedora Linux laptop with rootless Docker. **It hasn't been tested on Windows with Docker
  Desktop yet.** If a step here doesn't match what you see, tell the game's team.
- No Mac: the world needs a 64-bit Intel or AMD processor (x86_64), and the game's EDITOR button is hidden on a Mac
  for now.
- Under Docker Desktop and rootless Docker, every player may seem to come from one address. The world answers at most
  30 status requests a minute from one address, so a busy world there can be marked not reachable (section 25, "HTTP
  429"). Linux with Docker Engine (not rootless) keeps each player's own address.

Contents:
0. The first day, quickly
1. What you need
2. Your world's id and host token
3. Installing Docker
4. Getting and unpacking the package
5. Your settings: `world.toml` and `.env`
6. Starting the world
7. Checking it worked
8. Letting players in: your router's port
9. Carrier-grade NAT: when no port can be opened
10. Your world's Editor: the first sign-in
11. The Editor menu, the DM tool and My world
12. Managing from another computer
13. The fingerprint, and "the certificate has changed"
14. Co-builders and their roles
15. Editor codes are secrets
16. A lost password, and the `ratw-editor` commands
17. Cheats
18. NPC voices, the Editor and your AI key
19. Backups, and putting the world back
20. Updates
21. Stopping, starting, and after a restart
22. A second world on one computer
23. Moving to another computer, or starting again
24. Never zip your world folder to send it
25. Being the host in the game
26. What you can see, and your duties
27. Something went wrong

---

## 0. The first day, quickly

For the impatient, on Windows. Each step has its own section below if something doesn't go as written.

1. Install **Docker Desktop**, start it, and let it finish setting up (section 3).
2. In the game, sign in, choose **HOST A WORLD**, then **MAKE A WORLD**. Copy the lines it shows (the `.env` line
   with your token, and the `world.toml` lines with your world's id and central's address) into a note for now
   (section 2).
3. Unpack the package from the game's team into `C:\RATW\my-world`. Put the world image file (`ratw-world.tar`) in
   that folder too (section 4).
4. In File Explorer, right-click inside the folder and choose **Open in Terminal**. Type
   `docker load -i ratw-world.tar` (section 4).
5. Open `world.toml` in Notepad. Give the world its name, and put in its `id` and central's `url` from your note.
   Save (section 5).
6. In Notepad, make a new file holding the `.env` line from your note, `RATW_HOST_TOKEN=rh_...`. Save it in the world
   folder as `.env`, with **Save as type: All files** (section 5).
7. In the terminal, type `docker compose up -d` (section 6).
8. Forward **TCP 7788** on your router to this computer (section 8). Wait a minute: the Host page should say **LISTED**.
9. Open `manage\first-editor.txt` in the world folder. Copy the Editor code in it (the long line starting
   `ratw-editor:`) (section 10).
10. In the game's main menu: **EDITOR**, paste the code, **USE THE CODE**, **SIGN IN**, choose your own password, then
    **DM**. You are running your world (sections 10 and 11).

---

## 1. What you need

**A computer that stays on**, and doesn't go to sleep, while people play (on Windows, section 3 says how):
- **Windows 10 or 11** (64-bit) with **Docker Desktop**. It needs WSL 2 and virtualisation turned on in the computer's
  firmware (BIOS or UEFI). Docker Desktop's installer says if it isn't.
- Or **Linux** on x86_64, with Docker Engine and its Compose plugin.

**What it takes.** Measured on the test laptop, with nobody playing:

| What | Measured |
|---|---|
| First download | about **250 MB** (PostgreSQL, the database, is most of it; the world image comes as a file from the game's team) |
| Disk, images | about 350 MB for the world image and about 650 MB for PostgreSQL's, once unpacked |
| Disk, the world's database | about 340 MB for a new world |
| Disk, backups | about 80 MB for the map (saved once), then about 1-2 MB a day for a quiet world |
| Memory, idle | the world about **760 MiB**, the database about **215 MiB**: about 1 GB in all |
| First start | under a minute (it makes the database and imports the world) |
| Later starts | a few seconds |

**Not measured yet:** memory and processor use with players in the world; how many players a home computer can hold;
how fast the database grows as the world is played; how much of your internet connection each player uses.

So keep room to spare. The world saves itself by copying itself in memory for a moment, which can briefly need up to
twice the world's memory. A computer with 4 GB of memory free for the world should be comfortable, but that is a guess.
On Windows, Docker runs inside a small Linux machine (WSL 2), which may use at most half of your computer's memory
(section 3 says how to give it more).

**At least 2 GB free on the disk** after all that: a backup that would leave less is refused (section 19).

**An internet connection where you can open a port** on your router (section 8). Some home connections can't
(section 9). A rented server (a "VPS") always can.

**A game account approved as a host**, and the game's desktop client: the Editor works only in the desktop client,
not in a web browser. During the closed beta, ask the game's team to approve your account; until they do, the game
says "Hosting a world needs a host's account".

---

## 2. Your world's id and host token

Your world needs two things from central: its **id** and its **host token**. You get both from the game.

1. Open the game and sign in.
2. On the screen with your wolves, choose **HOST A WORLD**. (You only see it once your account may host.)
3. Under "A new world", give it a name of 1 to 48 characters, and choose **MAKE A WORLD**.
4. The **token** is shown **once**, in a box at the top, with a **COPY** button. It starts `rh_`. Copy it into a note
   for now (section 5 says where it goes).
5. Below the token, the Host page shows the lines to put in your world's files, each with its own **COPY**:
   - the line for `.env`, `RATW_HOST_TOKEN=rh_...`;
   - the lines for `world.toml`: your world's **id** (`id = "world-..."`: `world-` and 32 letters and digits) and
     central's address (`url = "https://..."`).
   Copy them into your note too. Section 5 says where each goes.

Keep the token secret.
- Anyone with it can put a world on the list as yours. It can't sign anyone in, and it can't read anything.
- The token is shown only this once. If you lose it, or it leaks, choose **NEW TOKEN** beside your world. The old one
  stops working at once, and your world leaves the list until it has the new one. Put the new one where the old one
  was (section 5), then type `docker compose up -d` if it is in `.env`, or `docker compose restart world` if it is in
  a token file.
- A token that isn't used for 90 days expires. Make a new one the same way.

An account can have up to 3 worlds. **REMOVE** takes a world off central for good. Its save, on your computer, is
kept.

The Host page also shows each world as **LISTED** or **NOT LISTED**, and why not (section 7).

---

## 3. Installing Docker

The world comes as a Docker package: four small parts that Docker runs for you (the world, its database, its backups,
and a small part that asks your router to open the port). You don't need to know how Docker works.

**On Windows:**
1. Install Docker Desktop: https://docs.docker.com/desktop/setup/install/windows-install/
2. Start it, and let it finish setting up. Accept the WSL 2 option if it asks.
3. So the world comes back after a restart: in Docker Desktop's **Settings**, **General**, turn on **Start Docker
   Desktop when you sign in to your computer**.
4. So the computer doesn't sleep: in Windows **Settings**, **System**, **Power** (or **Power & battery**), **Screen
   and sleep**, set "When plugged in, put my device to sleep after" to **Never**. On a laptop, also set closing the lid
   to **Do nothing** (Control Panel, **Power Options**, **Choose what closing the lid does**). While the computer
   sleeps, the world is off the list and its players are cut off.
5. Docker Desktop starts only when someone signs in to Windows. After Windows restarts (an update, a power cut), the
   world stays down until you sign in. To keep it up unattended, turn on automatic sign-in for your account (search
   for "Windows automatic sign-in"), or set Windows Update's **active hours** and pause its restarts while you host.
   A rented Linux server (a VPS) is the way to run fully unattended.
6. Memory: Docker's Linux machine (WSL 2) may use at most half of the computer's memory, and Docker Desktop's
   settings have no slider for it. If the computer has less than about 12 GB, give it more: make a plain-text file
   called `.wslconfig` (not `.wslconfig.txt`) in your user folder (`%UserProfile%`, for example `C:\Users\you`),
   holding

       [wsl2]
       memory=6GB

   then in PowerShell type `wsl --shutdown`, and start Docker Desktop again.

**On Linux:**
1. Install Docker Engine and its Compose plugin: https://docs.docker.com/engine/install/ (choose your distribution).
2. So the world comes back after a restart: `sudo systemctl enable --now docker`.
3. Rootless Docker works too (it is what the world was tested on). Its start-at-boot steps are on Docker's own pages.

**Check it:** open a terminal (on Windows, PowerShell or Terminal) and type:

    docker compose version

It should print a version. If it says "not recognized" or "command not found", Docker isn't installed yet, or the
terminal was open before it was: open a new one.

---

## 4. Getting and unpacking the package

During the closed beta, the game's team sends you three things:
- **the package**: a `.zip` holding the world folder;
- **the world image**, as a file (for example `ratw-world.tar`);
- **central's address**, for `world.toml` (section 5).

1. Unpack the package into a folder of its own. For example:
   - Windows: `C:\RATW\my-world`. In File Explorer, right-click the `.zip`, **Extract All...**, and choose `C:\RATW`.
     Keep the folder inside your own disk, **not** in OneDrive, Dropbox or another folder a cloud service copies
     (section 19 says why).
   - Linux: `~/ratw/my-world`
2. The folder holds:
   - `compose.yaml`, `world.toml`, `README.md` and this guide, `Hosting.md`;
   - four scripts: `restore.bat` and `reset-editor-password.bat` for Windows, `restore.sh` and
     `reset-editor-password.sh` for Linux;
   - two empty folders, `manage` and `backups`. Keep them. The world writes your Editor account into `manage`
     (section 10) and its backups into `backups` (section 19). Without them Docker would make them itself, and on
     Linux only root could open them.
3. Open a terminal in that folder.
   - Windows 11: in File Explorer, right-click inside the folder and choose **Open in Terminal**. Or in PowerShell:
     `cd C:\RATW\my-world`
   - Linux: `cd ~/ratw/my-world`
4. Load the world image into Docker. Put the image file in that folder, and type:

       docker load -i ratw-world.tar

   It ends `Loaded image: ratw-world:latest`. If it names another image, put that name in `.env` as
   `RATW_IMAGE=...` (section 5), for example `RATW_IMAGE=ratw-world-beta:latest`. You can delete the file afterwards:
   Docker keeps the image.

**Every command in this guide is typed in that folder.** If a command says `no configuration file provided`, the
terminal isn't in the world folder.

The folder's name and place don't matter to the world: it keeps its name inside Docker (`ratw-world`), so moving or
renaming the folder later keeps the world. But keep the folder: it holds your settings, your Editor account and your
backups.

---

## 5. Your settings: `world.toml` and `.env`

### Editing them

Use any plain-text editor: Notepad on Windows, or gedit, nano or Kate on Linux.
- Keep the quotes around text: `name = "My Ridgemere"`.
- `true` and `false` and numbers have no quotes.
- A line starting with `#` is a note, and is ignored. To turn on a setting shown as a note, remove the `#` in front of
  it.
- Save as plain text, UTF-8. (Notepad's "UTF-8 with BOM" is fine too.)
- On Windows, Notepad adds `.txt` to every file it saves unless told not to: `.env`, `host-token` and `ai-key` alike.
  In Notepad's "Save as" window, choose **Save as type: All files**, and type the whole name. To see files' real names
  in File Explorer: **View**, **Show**, **File name extensions**.

Each setting is read when its part of the world starts. After a change:

| You changed | Then |
|---|---|
| `world.toml`, or a token or key file it names | `docker compose restart world upnp`, or the Editor's **My world** tab, **⟳ Restart the world** (section 11). (The Restart button doesn't restart the `upnp` part, so a changed `[network]` setting waits for `docker compose restart world upnp`.) |
| `.env` | `docker compose up -d` (a restart doesn't read `.env` again) |

If something in `world.toml` is wrong, the world doesn't start. The log says what is wrong (section 7), and the world
waits: it reads `world.toml` again after 5 seconds, 30 seconds, 2 minutes and then every 2 minutes, and the log says
when it tries next. Put it right and the world starts at its next try, or at once with `docker compose restart world
upnp`. If the world has started before, the Editor stays up meanwhile, and its status says "Stopped: world.toml has a
mistake (see the log)".

### `[world]`: your world

| Setting | What it is | Rules |
|---|---|---|
| `name` | The world's name, on the list and in the game. | **Needed.** At most 48 bytes: a plain letter is one byte, an accented one two or more. No control characters. Central refuses rude words, and a community world can't call itself "official" or use the game's name. |
| `description` | A line or two for the list. | At most 300 characters. Central's word filter applies. |
| `region` | Roughly where the world is, so players can pick a near one. | At most 32 characters. |
| `tags` | Words to find it by: `["friendly", "roleplay"]`. | Up to 5, at most 16 characters each. |
| `player_limit` | How many wolves can be in the world at once. | A whole number from 1 to 10000. If left out: 40. |
| `arrivals_per_hour` | At most this many new wolves may arrive for the first time each hour. | A whole number from 1 to 100000. If left out, the server's own: 600. Each account may also bring at most 3 new wolves an hour. |
| `private` | A private world, with a password kept on central. | **Not built yet.** Leave it `false`. If set to `true`, the log warns and the world is listed as public. |
| `cheats` | `"off"`, `"host"` or `"everyone"`. | If left out: `"off"`. Section 17 says what each means. Anything but `"off"` lists your world "cheats on". |
| `id` | Your world's id, from the Host page (section 2). | **Needed.** `"world-"` and 32 letters and digits. |

### `[central]`: the game's service

| Setting | What it is | Rules |
|---|---|---|
| `url` | Central's address. | **Needed.** The package holds an example address here (`https://central.example.org`), which isn't a real central: put in the address the game's team gave you (section 4). `https://name`, with no path after it. |
| `token_file` | The name of a file holding your host token (another way to give the token). | Optional. A file in the world folder, beside `world.toml`, given by its name alone (a full path doesn't work: the world sees only its own folder). See "The host token" below. |
| `host` | A DNS name for your computer, listed in place of its number. | Optional, at most 253 characters. It must point at your internet address and nothing else. Local names (`.local`, `.lan`, `.home.arpa`, one-word names) are refused. If you don't know what this is, leave it out. |
| `secure` | `true` only if players reach the world through `wss://`: a proxy in front of it with a certificate for `host`. | `false` unless you have set that up. `true` needs `host`. For experienced hosts only. |

Without a host token the world still starts, and players who know its address can join it with the game's **JOIN BY
ADDRESS** box. But it isn't on the list, nobody has the host's commands (section 25), and central's ban list doesn't
reach it.

### `[ai]`: the residents' voices

All optional. Without a key, the residents speak their written lines, which work well. See section 18 first: a key
costs you money.

| Setting | What it is | Rules |
|---|---|---|
| `key` | Your OpenAI API key. | Optional. No spaces or unusual characters. It stays on your computer: it is never sent to central. |
| `key_file` | The name of a file holding the key, instead of `key`. | Optional. A file in the world folder, beside `world.toml`, given by its name alone. Used only when `key` is empty. |
| `model` | The residents' main voice. | If left out: `gpt-5.4-mini`. |
| `light_model` | Short replies. | If left out: `gpt-5.4-nano`. |
| `fallback_model` | Used when the main one fails. | If left out: `gpt-5.4-mini`. |
| `story_model` | The stories. | If left out: `gpt-5.6-luna`. |
| `cost_mode` | How much talk to buy: `generous`, `balanced` or `frugal`. | If left out: `balanced`. |
| `monthly_tokens` | The most your key is used for in a month, by the Editor's tools and the residents together, in tokens. | A whole number. If left out: 20,000,000. |

The model defaults are what the official world uses. Leave them unless you know why.

### `[server]`

| Setting | What it is | Rules |
|---|---|---|
| `workers` | The game's worker threads. | A whole number from 0 to 16. If left out: the processors Docker gives the world, less two. Leave it out unless the world competes with other programs. |

### `[network]`: asking the router to open the port (UPnP)

| Setting | What it is | Rules |
|---|---|---|
| `upnp` | Whether to ask your router, when the world starts, to forward its port to your computer (section 8). | `true` or `false`. The package has `true`; if left out, also `true`. `false` turns the attempt off. After a change, `docker compose restart world upnp`. |

### `[editor]`: your world's Editor

| Setting | What it is | Rules |
|---|---|---|
| `enabled` | Whether the Editor answers at all. | `true` or `false`. If left out: `true`. With `false`, nothing answers on the Editor port (section 12). |
| `owner` | The name of your first Editor account. | Optional: 3 to 32 small letters, digits, `-` or `_`, starting with a letter. If left out, the world makes one up (`host-` and four letters and digits). Used only the first time the world starts. **Never put a password here.** |
| `new_devices` | `"open"`: a name and password sign in from any computer. `"approve"`: a computer that hasn't signed in before also needs an Editor code. | If left out: `"open"`. Choose `"approve"` if you open the Editor port to the internet (section 12). |

### `.env`: settings beside `compose.yaml`

A few settings live in a file called `.env`, in the same folder. It doesn't exist until you make it. Each line is
`NAME=value`, with no spaces and no quotes.

| Setting | What it is |
|---|---|
| `RATW_HOST_TOKEN` | Your host token (see below). |
| `RATW_PORT` | The world's game port. If left out: 7788. Players and central reach the world on it, and it is the one to open on your router. |
| `RATW_EDITOR_PORT` | The Editor port. If left out: 17789. Change it only if something else holds 17789 (section 27, "ports are not available"), or for a second world (section 22). |
| `RATW_EDITOR_BIND` | Where the Editor answers. If left out: `127.0.0.1`, this computer only. `0.0.0.0`: every address of this computer, so you can manage your world from another computer (section 12). |
| `RATW_IMAGE` | The world image to run. Leave it out unless `docker load` named another one (section 4). |
| `RATW_BACKUP_MIN_FREE_MB` | A backup is refused if it would leave less than this free on the disk, in MB. If left out: 2048 (section 19). |
| `COMPOSE_PROJECT_NAME` | Only for a second world on the same computer (section 22). |

### The host token

Give it one of two ways:
- In `.env`:

      RATW_HOST_TOKEN=rh_...

- Or in a file of its own beside `world.toml` (say `host-token`, holding just the token), named in `world.toml`:

      [central]
      token_file = "host-token"

Paste the whole token, with nothing else. If you give both, the token file is the one used. On Linux you may make
these files private to you (`chmod 600 .env host-token world.toml`): the world still reads them.

---

## 6. Starting the world

In the world folder:

    docker compose up -d

The first time, Docker downloads the database's image (PostgreSQL), makes the database and imports the world. That
takes under a minute once the download is done. `-d` means it runs in the background: you can close the terminal.

---

## 7. Checking it worked

### Is it running?

    docker compose ps

You should see four lines, `world`, `db`, `backup` and `upnp`, all **Up**. (`db` also says "healthy".) If `world`
says **Restarting**, something is wrong: read the log. (`upnp` stays up even when UPnP didn't work: section 8.)

### The log

    docker compose logs -f world

This shows the world's log and keeps following it. Press **Ctrl+C** to stop following: the world goes on running.
Leave out `-f` to see it once. Every line starts with `world-1  |`.

Lines from the package itself start `RATW_PACKAGE`; the world server's start `RATW_SERVER` and the world agent's
`RATW_AGENT` (so neither can print a line that passes for the package's). These are the lines that matter, in about the
order they come:

| A line like this | What it means |
|---|---|
| `RATW_PACKAGE "My Ridgemere": on central https://... as world-..., port 7788, 2 worker thread(s)` | `world.toml` was read and is fine. |
| `RATW_PACKAGE Editor certificate: 3F2A 9C10 ...` | Your Editor's fingerprint (section 13). |
| `world: imported package ...: world ..., 1212 areas, ...` | First start only: your world was made. |
| `world already holds package ...; nothing to do.` | A later start: your world is kept as it was. |
| `RATW_PACKAGE the world database is ready (6.4 s)` | The database is ready. |
| `RATW_PACKAGE Your first Editor account is in manage/first-editor.txt` | First start only: your Editor account is ready (section 10). |
| `RATW_PACKAGE no AI key: NPCs use their authored lines` | No AI key: the residents use their written lines. |
| `RATW_PACKAGE the NPC Mind is ready (live model)` | Your AI key is in use (section 18). |
| `RATW_AGENT world agent: the world on port 7788 does not answer (... Connection refused>): no heartbeat until it does` | **Normal on every start**: the agent starts a few seconds before the world server. It tries again every minute. |
| `RATW_SERVER warning: RATW_TICKETS no keys from central yet (...)` | Normal for the first seconds of a first start. Nobody can join until the world has central's keys. |
| `RATW_SERVER RATW server listening on port 7788 (0.0.0.0); ready in 3.2 s` | **The world is up.** |
| `RATW_AGENT world agent: listed on central at 203.0.113.7:7788` | **The world is on the list**, at that address. Done. It comes up to a minute after the `listening` line. |
| `RATW_AGENT world agent: not listed on central: ...` | Not on the list, and why. Most often the port: see section 8. |
| `RATW_SERVER RATW_CONNECT ...` and `RATW_SERVER RATW_DISCONNECT ... (1 connected)` | A player came or went. |
| `RATW_SERVER RATW_HOST /kick ... : done` | One of the host's commands was used (section 25). |
| `RATW_AGENT world agent: left central's list`, then `RATW_PACKAGE stopped (the server's last exit: 0)` | The world stopped cleanly and left the list. |

A line starting `RATW_PACKAGE error:` stops the start. Section 27 says what each one means. Other lines (`RATW_TALK`,
`RATW_VOICE`, `RATW_DATABASE_WORLD_LOADED` and the like) are the world loading its parts. They are fine.

The backups have a log of their own: `docker compose logs backup`.

### Being listed

Once the log says "listed on central", the game's Host page shows your world as **LISTED** (choose **Refresh** to ask
again), and your world is on the world list for everyone. If it says **NOT LISTED**, the reason is beside it, the same
as in the log.

### Joining it yourself

You can join with any of your own wolves, from the list. If your router can't reach your own internet address from
inside your home (many can't), join from the same computer with **JOIN BY ADDRESS** and `127.0.0.1:7788`. From another
computer at home, use the world computer's home address, for example `192.168.1.20:7788`.

---

## 8. Letting players in: your router's port

Players, and central, reach your world on **one port: TCP 7788** (or your `RATW_PORT`). Your router must send that
port to the computer that runs the world. This is called **port forwarding**.

(The Editor has a second port, 17789. It stays closed to everyone but this computer unless you open it, and players
never need it: section 12.)

Central checks the game port for you: before it lists your world, and every 10 minutes after, it tries to reach the
world from the internet. If it can't, you see:

    not listed on central: Central could not reach this world at 203.0.113.7:7788: it did not answer within 5 seconds. Open port 7788 to the internet (TCP).

### Maybe: the world asks the router itself (UPnP)

Many home routers let programs ask them to open a port (UPnP). The package has a small part, `upnp`, that tries this
when the world starts. While the world runs it keeps the forward open; when the world stops cleanly it removes it.
To see what happened:

    docker compose logs upnp

It says one line, starting `UPnP:`:
- `UPnP: the router (...) now forwards port 7788 (TCP) to this computer (...)`: it worked.
- `UPnP: port 7788 is already forwarded to this computer on the router ...: left as it is.`: you (or something else)
  forwarded it already. Fine.
- A line saying the router's internet address is in `100.64.0.0/10` or is private: carrier-grade NAT, or a second
  router (section 9).
- Anything else says why it didn't: see "By hand" below.

Don't count on it:
- It works only on **Linux with Docker Engine** (not rootless). Under **Docker Desktop** (Windows) and **rootless
  Docker**, the world can't reach the router's UPnP at all: forward the port by hand.
- Many routers have UPnP turned off. It never works behind carrier-grade NAT (section 9).
- It never changes a forward you made yourself, or one for another computer.
- A few routers keep only permanent forwards. There, a world that doesn't stop cleanly (killed, or a power cut) leaves
  its forward on the router until it next starts on this computer. If you stop hosting, remove it on the router.

To turn it off: in `world.toml`, `[network]`, set `upnp = false`, then type `docker compose restart world upnp`.

### By hand

**1. Find your computer's home address.**
- Windows: in PowerShell, type `ipconfig`. Look for **IPv4 Address** under your network adapter (Wi-Fi or
  Ethernet), for example `192.168.1.20`.
- Linux: type `hostname -I`. The first address is usually the one, for example `192.168.1.20`.

**2. Keep that address from changing.** In your router's settings, look for "DHCP reservation", "static lease" or
"address reservation", and reserve this address for this computer. Otherwise the router may give it another one some
day, and the forward stops working.

**3. Open your router's page.** Type its address in a web browser. It is often `192.168.1.1` or `192.168.0.1`, or it is
on a sticker on the router. Sign in with the router's admin password (also often on that sticker).

**4. Add the forward.** Look for **Port forwarding**, **Virtual server**, **NAT** or **Applications**. Add one:
- protocol: **TCP**
- outside (external, public) port: **7788**
- inside (internal) port: **7788**
- to (device, internal IP): your computer's home address from step 1

Every router's page looks different. Your router's maker has a guide: search for "port forwarding" and your router's
model.

**5. Let it through your computer's firewall.**
- **Windows:** if Windows asks whether to allow Docker Desktop through the firewall, allow it. If players still can't
  reach it (these steps aren't tested on Windows yet):
  1. If you chose **Cancel** in that window, or closed it, Windows may have made rules that *block* Docker Desktop,
     and a block rule wins over any allow rule. Open PowerShell **as administrator**, and list them:

         Get-NetFirewallRule -Direction Inbound -Action Block | Where-Object DisplayName -like '*Docker*'

     If it lists any, remove them: type the same line again with `| Remove-NetFirewallRule` at the end. (Or in
     **Windows Defender Firewall**, **Allow an app or feature through Windows Defender Firewall**, tick Docker
     Desktop's backend, `com.docker.backend`, for your network.)
  2. Then let the port in (still as administrator). The rule covers every network profile, Private and Public:

         New-NetFirewallRule -DisplayName "RATW world" -Direction Inbound -Protocol TCP -LocalPort 7788 -Action Allow

- **Linux with firewalld** (Fedora and others):

      sudo firewall-cmd --permanent --add-port=7788/tcp
      sudo firewall-cmd --reload

- **Linux with ufw** (Ubuntu and others): `sudo ufw allow 7788/tcp`

**6. Wait a minute.** The world agent tells central every minute, and while the world isn't reachable central checks
again each time. The log then says `listed on central at ...`.

### Your address is public

The list shows your world's internet address to every player. That is your home's address. If you'd rather not, run
the world on a rented server (a VPS) instead: the steps are the same on any Linux server.

---

## 9. Carrier-grade NAT: when no port can be opened

Some internet providers share one internet address between many homes. This is **carrier-grade NAT** (CGNAT). It is
common on mobile, satellite and fibre connections. Behind it, no port can be opened to your home from the internet:
forwarding on your own router doesn't help.

**How to tell:**
- The log or Host page says: `This world's address (IP) is not on the internet.`
- Or the UPnP line says the router's internet address "is in 100.64.0.0/10, the providers' shared range", or "is
  private".
- Or: your router's page shows its internet (WAN) address, and it isn't the address a site like
  https://ifconfig.me shows you. If they differ, or the router's starts with `100.64` to `100.127`, `10.`, `172.16` to
  `172.31` or `192.168.`, you are behind someone else's NAT.

**What to do:**
1. **Ask your provider for a public IPv4 address.** Many give one for free or a small fee, if asked. This is the
   simplest fix.
2. **Run the world on a rented server (a VPS)** instead of at home. A small one from any hosting company will do.
3. **A tunnel or VPN with port forwarding.** Central lists the address your world's heartbeat comes from, so the
   world's outgoing traffic must leave from the same public address that players connect to. This hasn't been tested.

The world speaks IPv4 only for now.

Behind CGNAT you can still manage your world from the computer it runs on, and from other computers at home
(section 12). Only managing from outside your home needs a port.

---

## 10. Your world's Editor: the first sign-in

The game's main menu has an **EDITOR** button, before you join any world. Through it you run your world with its own
tools. You need the game's **desktop client** (the Editor doesn't work in a web browser).

Your world keeps its own **Editor accounts**. They aren't your game account: an Editor account belongs to this world
only, and its password is one you choose for this world alone. The world makes the first one for you, the **owner**,
the first time it starts.

**The first time, on the computer that runs the world:**

1. Start the world (section 6) and wait a minute or two.
2. Open the world folder, then the `manage` folder in it. Wait until **`first-editor.txt`** appears there. (Beside it,
   `editor-certificate.txt` appears too: section 13.)
3. Open `first-editor.txt` in Notepad (Linux: any text editor). It says:

       Your Editor account: host-k7q2
       One-time password: ...

       Easiest: open the game, press EDITOR, and paste this Editor code:

       ratw-editor:...

   The name is made up for you (`host-` and four letters and digits) unless you chose one with `[editor] owner` in
   `world.toml`. You don't need to remember it: the game remembers it for you.
4. Copy the whole Editor code: the long line starting `ratw-editor:`, from the first letter to the last. (In Notepad,
   click at its start, hold Shift and press End, then Ctrl+C.)
5. Open the game. On the main menu, before signing in or after, choose **EDITOR**.
6. In the box **HAVE AN EDITOR CODE? PASTE IT**, paste the code (Ctrl+V) and choose **USE THE CODE**.
   If the world is still starting, the game says "Your world is still setting up (…%). This page waits for it." Wait:
   it goes on by itself.
7. The sign-in screen shows your world's name and `127.0.0.1`, your account's name, and "Your Editor code carries the
   one-time password: just sign in." Choose **SIGN IN**.
8. **CHOOSE YOUR PASSWORD.** Type a new password in **NEW PASSWORD** and again in **NEW PASSWORD AGAIN**, then choose
   **CHOOSE THIS PASSWORD**. The rules:
   - at least 12 characters;
   - not containing your account's name, or your world's name (or any word of 4 letters or more from it);
   - not one of the passwords most often found in breaches (`password123456` and the like are refused);
   - use it for this world only, never your game account's password or any other.

   Write it down somewhere safe, or keep it in a password manager.
9. The game says "Your password is saved." and shows **the Editor menu** (section 11). The one-time password has
   stopped working, and within seconds `first-editor.txt` says "Used".
10. Choose **DM** to open the Dungeon Master tool in its own window.

**Without the code:** on the same computer you can also type the address `127.0.0.1` in **YOUR WORLD'S ADDRESS**,
choose **CONNECT**, then **TRUST AND CONTINUE** (section 13 says what that screen is), then the name and one-time
password from the file. The code is easier: it carries everything, and the game checks the world's key from it.

**Keep `first-editor.txt` to yourself until it says "Used".** Whoever has the code can take the account once.

**If you wait too long:** the one-time password lasts 7 days. If it wasn't used by then, the world writes a new one
into the file the next time it starts ("A new one-time password: the last one wasn't used within 7 days").

**If `first-editor.txt` never appears:** read the log (`docker compose logs world`). If it says the `compose.yaml` has
no manage folder, your `compose.yaml` is from before the Editor: section 20, "A world from before the Editor". You can
also make a new code with the reset script (section 16).

**Each time after that**, on this computer: **EDITOR**, then your world under **RECENT WORLDS**, **CONNECT**, your
password, **SIGN IN**. The game never stores your password: you type it each time you start the game. A session ends
after 30 minutes without you touching the Editor, after 12 hours in any case, and when you quit the game.

---

## 11. The Editor menu, the DM tool and My world

### The Editor menu

After signing in you see your world's name and address, and:
- **a status**: "Open · 7 wolves", "Open only to you", "Closed for changes", "Restarting…", "Stopped (exit N): starts
  again in 5 s", "Keeps stopping (exit N): …", "Setting up 40%", "Database slow", or "Not answering";
- "Last backup 3 h ago" (or "No backup yet");
- "Signed in as host-k7q2 (owner) · Certificate checked ·" and the fingerprint (section 13);
- for the owner, "N failed sign-ins today" when there were any, and a line for every Editor account change since your
  last visit;
- **DM**: opens the Dungeon Master tool in the Editor window;
- **ATLAS**: the map editor, greyed: "Map editing comes in a later update.";
- **EDITOR ACCOUNTS** (the owner only: section 14), **Change my password**, **Sign out**, **Back to the main menu**.

The Editor window keeps working while you go back to the main menu and play. If the world restarts, the window says
"Your world is restarting: the Editor reconnects by itself." If your session ends, it says "Your Editor session ended:
sign in again from the Editor menu." Sign in again as the same account and the window carries on.

### The DM tool's tabs

| Tab | What it does | When it takes effect |
|---|---|---|
| **LIVE** | The world as it is now: who is where, visitors, rumours, notices, festivals, the latest events and the calendar. (The map's picture itself comes with map editing, in a later update: until then the map area says so, and Find someone and the side panel do the work.) | At once |
| **NPC Life** | Each resident's life: kill, revive, protect, move, their story | At once |
| **Chapters** | Players' Chapters: treaties, House requests | At once |
| **Players** | The wolves in your world: storytellers, reports, and (for the owner and those allowed) acting on a wolf | At once |
| **Prosperity** | The economy as players see it: prices, takings, farms, carts | At once |
| **Economy** | The economy's levers: the steering slider, edicts, rules, the store, seasons | At once |
| **Server Health** | How the world's server is doing | (Read only) |
| **My world** | Your world itself (below) | (Below) |

Each action that harms a player's wolf names the wolf in its question, says "its player is told in the game", and
says whether it has an opposite (kill and resurrect, hold and release, and so on). A greyed button says why it is
greyed when you point at it.

### My world

The **My world** tab is about the world itself. At the top: how many wolves are in the world, and the last backup.
- **The world's state**, with **⏸ Close the world** (players get a 60-second warning, then it saves and stops; it
  stays closed until you open it), **▶ Open the world**, and **⟳ Restart the world** (it saves, stops and starts
  again, reading `world.toml` first; the Editor stays open). The owner's only: others see them greyed.
- **Backups**: the newest, one being made now, the last refusal in plain words, the space the backups use and the
  disk's free space; **⤓ Back up now**; each backup with **Pin** (kept 90 days; at most 5 pinned) and **Copy restore
  steps** (section 19).
- **Settings**: players at once, worker threads, cheats and the Editor, as `world.toml` says. "Change them there, then
  Restart."
- **AI use**: today's life stories against the day's limit, and the month's tokens against `monthly_tokens`
  (section 18).
- **Version and certificate**: the package's version and the Editor's fingerprint.
- **History**: every change made with the Editor, newest first, with the account that made it; **Sign-ins**.
- For the owner only: **the world folder**'s path (with a warning if it looks like a OneDrive, Dropbox, Google Drive or
  iCloud folder), and, if the world's server keeps stopping, its last lines.
- **Something went wrong?**: a short table of what to do (section 27).

Others with an Editor account see My world read-only, and only their own sign-ins.

---

## 12. Managing from another computer

The Editor port, **17789**, answers **only on the world's own computer** until you open it. That is on purpose: a new
world's one-time password never waits on an open port. If you play on the computer that runs the world, you never need
this section.

### Opening it at home

1. In the world folder, open `.env` (or make it: section 5) and add this line:

       RATW_EDITOR_BIND=0.0.0.0

2. Save it, then type:

       docker compose up -d

3. Find the world's computer's home address: on Windows, `ipconfig` in PowerShell (**IPv4 Address**, for example
   `192.168.1.20`); on Linux, `hostname -I`.
4. On the other computer, in the game: **EDITOR**, then in **YOUR WORLD'S ADDRESS** type that address, for example
   `192.168.1.20`, and **CONNECT**. (The port is 17789 unless your `.env` sets `RATW_EDITOR_PORT`; then type
   `192.168.1.20:17800`, say.)
5. The first time from that computer, the game shows **FIRST CONNECTION** and your world's fingerprint. Compare it
   with `manage\editor-certificate.txt` on the world's computer (section 13). If they match, choose **TRUST AND
   CONTINUE**. Or skip this with an Editor code made for that computer:

       docker compose exec world ratw-editor invite NAME --address 192.168.1.20:17789

   (NAME is your Editor account; `docker compose exec world ratw-editor list` shows it.) Paste the code it prints
   into EDITOR on the other computer. Your password stays the same.
6. Sign in with your name and password.

If the other computer says it "can't reach an Editor":
- **Windows marks some networks Public**, and then other computers can't reach either port. On the world's computer:
  **Settings**, **Network & internet**, **Wi-Fi** (or **Ethernet**), your network's name (on Wi-Fi, its
  **Properties**), then **Network profile type: Private network**. (Windows 10: **Settings**, **Network & Internet**,
  **Status**, **Properties**, **Private**.) Only do this on your own home network.
- **The firewall:** if friends can already join the game from another computer, Docker Desktop is allowed and nothing
  more is needed. Otherwise follow section 8, step 5, with 17789 in place of 7788:

      New-NetFirewallRule -DisplayName "RATW Editor" -Direction Inbound -Protocol TCP -LocalPort 17789 -Action Allow

  (Linux: `sudo firewall-cmd --permanent --add-port=17789/tcp` then `sudo firewall-cmd --reload`, or
  `sudo ufw allow 17789/tcp`.)

### From outside your home

**Only do this if you will manage your world from outside your home.** Everything else works without it.

1. Open the Editor port at home first (above).
2. On your router, forward **TCP 17789** to the world's computer, the same way as the game port (section 8, "By
   hand", steps 3 and 4, with 17789 in place of 7788). Players never need it.
3. Find your home's public address: the address central lists your world at (the log's `listed on central at ...`),
   or a site like https://ifconfig.me opened on the world's computer. If your provider changes it now and then, a
   free "dynamic DNS" name (your router's settings often have one) keeps a name pointing at it.
4. **Test it from a phone's hotspot, not from inside your home.** Many routers can't reach their own public address
   from inside, so a test from home fails even when the forward works. Turn on your phone's hotspot, join it with a
   laptop, and in the game: **EDITOR**, `203.0.113.7` (your public address), **CONNECT**.
5. The first connection from outside is the one that matters. Use an Editor code (step 5 above, with
   `--address 203.0.113.7:17789`), so there is nothing to compare. With a bare address the button reads **I COMPARED
   IT**: compare all eight groups with `manage\editor-certificate.txt` first (section 13).
6. Consider `new_devices = "approve"` under `[editor]` in `world.toml` (then **⟳ Restart the world**): a computer that
   hasn't signed in before then needs an Editor code as well as the password, so a guessed or stolen password alone
   isn't enough.

Nothing checks the Editor port from outside the way central checks the game port. The hotspot test is your check.

### On a rented server (VPS)

**Warning: Docker opens published ports past the server's firewall** (`ufw` doesn't stop them). With
`RATW_EDITOR_BIND=0.0.0.0` the Editor port faces the internet at once. That is how you manage a VPS from home, and it
is fine, if you:
- make your first connection with the Editor code from `manage/first-editor.txt` (copy the file down to your own
  computer over the same way you reach the server, then delete the copy), or with `ratw-editor invite NAME --address
  your.server.address:17789`, never by trusting a bare address;
- set `new_devices = "approve"` under `[editor]` in `world.toml`;
- choose a long password.

### Turning the Editor off

`[editor] enabled = false` in `world.toml`, then `docker compose restart world` (or **⟳ Restart the world**). Nothing
then answers on the Editor port, and the game can only say it can't connect. Docker still holds the port (17789, or
your `RATW_EDITOR_PORT`) for the world, so turning the Editor off doesn't fix a port collision: section 27 does.

---

## 13. The fingerprint, and "the certificate has changed"

Your world's Editor talks to the game over an encrypted connection with a key of its own, made the first time the world
starts. The **fingerprint** is that key's short name: eight groups of four characters, like
`3F2A 9C10 5B7E 0D41 A6C3 22F9 8E05 77D1`. The game remembers it after your first connection, the way your phone
remembers a Wi-Fi network, and warns if it ever changes. That is how the game knows it is talking to your world and
not to someone in between.

**Most hosts never compare it:** an Editor code carries the key, so the game checks it for you.

**Where to find it:**
- in `manage/editor-certificate.txt` in the world folder (with your world's id and its Editor port);
- by typing `docker compose exec world ratw-editor fingerprint`;
- on the Editor menu ("Certificate checked ·" and the fingerprint) and on My world, once signed in;
- in the log at every start (`RATW_PACKAGE Editor certificate: ...`). Compare with the file or the command, though:
  those come from the world itself.

**FIRST CONNECTION.** Connecting from a computer by a bare address (no code) the first time, the game shows "First
connection to *your world* at *address*. Its certificate fingerprint is:" and all eight groups. Compare every group
with the file. If they match, choose **TRUST AND CONTINUE** (**I COMPARED IT** for an address on the internet). On the
world's own computer (`127.0.0.1`) it adds "This is the computer you're using: comparing is up to you." If they don't
match, choose **Cancel**.

**"STOP: THE CERTIFICATE HAS CHANGED".** A full red screen: "Don't sign in." It shows what this computer trusted and
what the address shows now. **Don't sign in**, and there is no way to sign in from that screen. Someone may be
pretending to be your world. It is expected only if *you* did one of these:
- made a new world (`docker compose down -v`), or lost Docker's storage for the world;
- ran `ratw-editor new-certificate --leaked` (below);
- put back a copy of the world without its Editor key.

Then, and only then: compare the new fingerprint with `manage/editor-certificate.txt` (or `docker compose exec world
ratw-editor fingerprint`) on the world's computer, type the world's name in **THE WORLD'S NAME** (as this computer knew it), and choose **FORGET THE
OLD CERTIFICATE**. The game then shows FIRST CONNECTION again. A new Editor code from the world's owner also clears
the red screen.

**A new key, signed by the old one.** If you think the key may have been copied (an old backup of Docker's storage
went missing, say), make a new one:

    docker compose exec world ratw-editor new-certificate

It prints "New Editor certificate: ... (was ...). Signed by the old key: clients that knew the old one move over
without a warning." Every computer that knew the old key moves over by itself; the game just says "*your world* has a
new certificate, signed by its old one."

**If the key leaked** (someone else had a copy of Docker's storage, or the world's computer was broken into):

    docker compose exec world ratw-editor new-certificate --leaked

Every Editor session ends, and every computer that used the Editor shows the red screen once. Send your co-builders new
Editor codes (section 14). `manage/editor-certificate.txt` and `ratw-editor fingerprint` show the new fingerprint at
once.

---

## 14. Co-builders and their roles

A co-builder is someone you trust to help run your world: an event-runner, a friend who builds. Each gets an Editor
account of their own, with a **role**:

| Role | What they can do |
|---|---|
| `viewer` | Watch the DM tool. Can't change anything. |
| `dm` | Run events and NPCs, the economy, Chapters and storytellers. Can't touch the map, My world's actions or accounts. |
| `builder` | All a `dm` can, and the map once map editing comes. Can't touch My world's actions or accounts. |
| `owner` | Everything, including Editor accounts and My world (closing, restarting, backups). |

**Acting on players' wolves** (killing, injuring, moving or bringing one back) is for the owner, and for accounts you
tick **MAY ACT ON PLAYERS' WOLVES**. These hurt real players, so leave it off unless they need it.

**Owners are made only on the world's computer**, with `docker compose exec world ratw-editor add NAME --role owner`.
The Editor accounts screen can add `viewer`, `dm` and `builder` accounts, so a stolen owner session can't make itself a
lasting owner. **The last owner can't be removed or turned off.**

Remember: your world doesn't know which wolves are a co-builder's. With cheats off (section 17) the world refuses
rewards to *your* wolves; it trusts you and your co-builders not to reward theirs.

### Adding a co-builder

1. On the Editor menu, choose **EDITOR ACCOUNTS**, and type your password again when asked ("Type your Editor password
   to go on"). Every change here asks for it again.
2. Under **ADD A CO-BUILDER**, type their account's name (3 to 32 small letters, digits, `-` or `_`, starting with a
   letter: say `pell`). Choose the **ROLE** (`dm` unless you mean otherwise; the line below it says what the role can
   do), tick **MAY ACT ON PLAYERS' WOLVES** only if needed, then **ADD**.
3. The game asks: "Add pell with the role dm? They need the RATW desktop client, and your Editor port open to them ..." Type
   your password and go on.
4. **THE EDITOR CODE FOR PELL · SHOWN THIS ONCE.** Choose **COPY**, and pass it on privately (section 15). Under it,
   the game says whether the code holds your world's address. A code made on the world's own computer doesn't: "It
   doesn't include your world's address: tell them that too (your home or public address, port 17789)."
5. Tell them:
   - they need the game's **desktop client**;
   - your Editor port must be open to them: at home, section 12's first part; from outside, its second part too;
   - your world's address, if the code doesn't carry it;
   - to choose **EDITOR**, paste the code into **HAVE AN EDITOR CODE? PASTE IT**, **USE THE CODE**, type the address if
     asked, **SIGN IN**, and choose their own password.

The accounts list says "not yet claimed" until they do, then their last sign-in and from which address.

### Tell your co-builders

"The world's host keeps your Editor account in their world and sees your address. Use a password only for this world.
Only accept Editor codes from people you trust: signing in to a stranger's world shows you their tools, never yours."

### Changing an account

On **EDITOR ACCOUNTS**, beside each account (each change asks for your password again):
- the role list (`viewer`, `dm`, `builder`);
- **MAY ACT ON PLAYERS' WOLVES**;
- **NEW CODE**: a new Editor code for them, shown once; their old password, sessions and computers stop working (for
  a lost password, or a code that leaked);
- **TURN OFF** (their sessions end at once) and **TURN ON**;
- **REMOVE**: for good.

At the bottom, **END ALL EDITOR SESSIONS** ends every session, yours too, and forgets every computer, so everyone signs
in again (and, with `new_devices = "approve"`, needs a new code). Use it if someone you don't know signed in.

Every account change shows on the owner's Editor menu next time ("Account pell added by host-k7q2"), and in My
world's history.

---

## 15. Editor codes are secrets

An Editor code (`ratw-editor:...`) carries your world's address and key, an account's name, and a one-time password or
pass. **Whoever has it can sign in as that account once.**
- A co-builder's code works **once, for 72 hours**. The first owner's, in `first-editor.txt`, for **7 days**. A code
  from `ratw-editor invite` or the reset script, for 72 hours.
- Pass it **privately**: a direct message to that one person, or read it out. **Never in a group chat**, a forum, a
  screenshot or a shared document.
- Once used, it does nothing. The account then has its own password.
- **If one leaks before it was used:** on **EDITOR ACCOUNTS** choose **NEW CODE** for that account (the old code stops
  working), or on the world's computer `docker compose exec world ratw-editor reset NAME`. If it was already used by
  someone else, do that too, then **END ALL EDITOR SESSIONS**.
- `manage/first-editor.txt` holds a code until you sign in, and after a reset. Keep the folder to yourself
  (section 24).

---

## 16. A lost password, and the `ratw-editor` commands

### The double-click way

On the world's computer, while the world runs:
- **Windows:** double-click **`reset-editor-password.bat`** in the world folder.
- **Linux:** in the world folder, type `./reset-editor-password.sh`.

It lists your world's Editor accounts, asks "Which account? Type its name (or just press Enter to stop):", resets that
one, and shows its new one-time password and Editor code. The code is also written into `manage/first-editor.txt`,
with the address `127.0.0.1`, so you can paste it from there. The account's sessions end and its computers are
forgotten; at the next sign-in it chooses a new password. The window waits for Enter before it closes.

Use it for a lost password, for "Too many tries from here", and for an owner's account (the Editor accounts screen
can't reset an owner).

### The commands

Typed in the world folder, on the world's computer. Anyone who can type these already owns the world, so no sign-in
limit ever blocks them.

| Command | What it does, and what it prints |
|---|---|
| `docker compose exec world ratw-editor list` | Every Editor account: its name, role, state ("on", "off", "not yet claimed", "on a one-time password") and how many computers it knows. |
| `docker compose exec world ratw-editor reset NAME` | A new one-time password and Editor code for NAME (72 hours). Unlocks it; ends its sessions; forgets its computers. Prints "Editor account:", "One-time password:" and the code. |
| `docker compose exec world ratw-editor invite NAME` | An Editor code for a new computer; the password stays the same. Add `--address 192.168.1.20:17789` to put the world's address in the code. |
| `docker compose exec world ratw-editor add NAME --role dm` | A new account (`viewer`, `dm`, `builder` or `owner`), with its one-time password and code. The only way to make an owner. |
| `docker compose exec world ratw-editor disable NAME` / `enable NAME` | Turns an account off ("NAME is turned off.") or on. |
| `docker compose exec world ratw-editor remove NAME` | Removes it ("NAME is removed."). Not the last owner. |
| `docker compose exec world ratw-editor sign-out-all` | "Every Editor session has ended, and every device is forgotten." |
| `docker compose exec world ratw-editor fingerprint` | Your world's id, its Editor port and the fingerprint (section 13). |
| `docker compose exec world ratw-editor new-certificate` | A new key, signed by the old one (section 13). Add `--leaked` if the old one leaked. |

The codes and passwords are printed only in your terminal, never in the log.

---

## 17. Cheats

`cheats` in `world.toml`'s `[world]` decides whether anyone in your world can get things the game doesn't normally
give. After a change, **⟳ Restart the world**.

| Setting | What it means |
|---|---|
| `cheats = "off"` | The default. Nobody has any cheats, you included. The Editor can't give Gifts or unlock Gift tiers, and it refuses rewards to your own wolves (bringing one back, awarding a deed, giving money or a place, approving your own requests). |
| `cheats = "host"` | Your Editor's DM tool may give Gifts and Gift tiers by hand, and reward your own wolves. Players get no cheats. |
| `cheats = "everyone"` | **Every player** can give themselves Gifts and goods from nothing, as on a test world. Only for a world where that is the point. |

Either of the last two lists your world **"cheats on"** for everyone to see, and the log says so at each start. The
full in-game Dev Console is never available on a community world.

"Cheats on" keeps honest hosts honest; it isn't a lock. What *is* certain: nothing on your world reaches central or
any other world, so cheats here never touch a player's progress elsewhere.

---

## 18. NPC voices, the Editor and your AI key

Without an AI key, the residents speak their own written lines. They work well, and cost nothing. This is the default.

With an OpenAI API key in `world.toml`, the residents talk with the model's words. **You pay OpenAI for every word
they say**, on your own OpenAI account.
- The cost hasn't been measured for a community world. It depends on how many players talk to residents, and for how
  long.
- **Set a monthly spending limit on your OpenAI account** before you add the key.
- `cost_mode` sets how freely the main voice is used. Past these, the small model (`light_model`) answers, which costs
  less, but still costs:

  | `cost_mode` | Main-voice replies an hour, whole world | ... to any one player | Overheard talk written live, an hour |
  |---|---|---|---|
  | `frugal` | 200 | 20 | 4 |
  | `balanced` | 600 | 60 | 10 |
  | `generous` | 1200 | 120 | 20 |

**The Editor uses your key too.** In the DM tool, writing a resident's life story calls the model, on your key. Map
editing's roster generation will too, when it comes. So:
- at most **100 life stories** and **20 roster generations** a day;
- `monthly_tokens` in `[ai]` (20 million if left out) caps the Editor's tools and the residents **together** for the
  month. Past it, the Editor's AI buttons stop until next month ("This month's AI budget is spent").
- My world's **AI use** shows today's count against the limits and the month's tokens against the budget.
- Without a key, those buttons are greyed and say "Needs an AI key in world.toml."

What players say to residents, and what the Editor sends for a life story (a resident's chronicle and description), go
to OpenAI under your key. The world list shows players that your world uses AI. The key never leaves your computer for
central. If the part that talks to OpenAI can't start, the log says so and the residents use their written lines.

---

## 19. Backups, and putting the world back

### The world backs itself up

A part of the package, `backup`, saves your world into the `backups` folder in the world folder:
- when it starts, and whenever the newest daily backup is over a day old (a computer that is off at night still gets
  one each day it runs);
- before an update changes the world's database layout (section 20);
- when you ask: My world, **⤓ Back up now** (the owner).

It keeps the 7 newest daily backups, the 5 newest of the others (by hand, before an update, before a restore), and up
to 5 you **pin** in My world, each for 90 days. Each backup is a `ratw-DATE-WHY.dump` file with a `.json` beside it
saying which world, package and map it came from. The map itself, most of the database, is saved once per map
(`build-N.cells.gz`), not in every backup: a daily backup of a quiet world is a megabyte or two.

Before each one it checks the disk: a backup that would leave less than 2 GB free is refused, and My world says so in
plain words. Free some space (or delete old files from `backups`); `RATW_BACKUP_MIN_FREE_MB` in `.env` changes the
limit. `docker compose logs backup` says what it did. `docker compose run --rm backup list` lists the backups.

**Keep backups private.** They hold everything in your world, players' messages too, and the Editor accounts'
password hashes. On Windows, keep the world folder inside your own user folder or on your own disk, and never in
OneDrive, Dropbox or another folder a cloud service copies. My world shows the owner the folder's path, and warns when
it looks like one.

### Putting the world back to a backup

This puts your whole world back to how it was when the backup was made. **Everything players did since then is lost.**
Use it only when something is badly broken (section 27).

On the world's computer:
- **Windows:** double-click **`restore.bat`** in the world folder.
- **Linux:** in the world folder, type `./restore.sh`.

It:
1. lists the backups, newest first, each with its date, how long ago, why it was made and its size, and asks "Which
   backup? Type its number (or just press Enter to stop):";
2. says plainly what it means: "This puts your world back to DATE (3 hours ago, by hand). Everything players did since
   then is lost.", and asks "Type RESTORE to go on (anything else stops):". Type `RESTORE` in capitals;
3. asks "Keep your Editor accounts as they are now (recommended), or take the ones in the backup?". Press Enter (or
   type `K`) to keep today's: a co-builder you removed, an old password or a forgotten computer doesn't come back.
   Type `B` to take the backup's;
4. stops the world (it saves first: up to a minute and a half), makes a **safety copy** of the world as it is now (in
   `backups`, "before a restore"), puts the backup back, ends every Editor session, prints the Editor accounts the
   world now has, and starts the world again;
5. says "Your world is back. Sign in to its Editor again (every Editor session ended)." Press Enter to close.

It refuses, changing nothing: a backup from another world, a damaged one, one whose map file is missing, and one made
by a newer package than the world's (update the world first). A backup from an older package is brought up to date
when the world starts again; if you kept your Editor accounts, it says "your world brings it up to date when it starts,
and then puts your Editor accounts back as they were", and the log then says `put back the N Editor account(s) the
restore kept`. Changed your mind? The safety copy is a backup like the others: restore it the same way.

If you took the backup's Editor accounts, `manage/first-editor.txt` says which accounts the world has now; a lost
password is then the reset script (section 16).

**Copy restore steps.** In My world each backup has **Copy restore steps**: the lines for PowerShell and for a Linux
terminal that restore that one backup (`.\restore.bat ratw-...`, run in the world folder). A restore never starts
from the Editor itself: only on the world's computer, so a stolen Editor session can't wipe your world.

---

## 20. Updates

When a new version of the game comes out, update your world. During the closed beta the game's team sends the new
world image as a file. In the world folder, load it, then start the world on it:

    docker load -i ratw-world.tar
    docker compose up -d

**Your world is kept**, with everything that has happened in it, and so are its Editor accounts, passwords and
fingerprint: nobody sees a warning. The update changes the server, and the database's layout if it needs to. Before it
changes the layout, the world makes a backup ("before an update"), and the log says:

    RATW_PACKAGE made a backup before the update: backups/ratw-...-before-update.dump

It doesn't change your world's map or residents: a new package's world is only for new worlds. The log then says:

    world: this world was started from package A (...) and keeps its own state; the image carries package B (...), which is not applied to an existing world; ...

A world on an older game version stays on the list, but players whose game is newer see "This world is older than
your client: its host needs to update it", and can't join it. So update when a new version comes out.

**If the update comes with a new folder** (a new `compose.yaml` or new scripts): copy `compose.yaml` and the four
scripts over the old ones. **Keep** your `world.toml`, `.env`, `manage` and `backups`. Then `docker compose up -d`.

### A world from before the Editor

A world started before the Editor came has no Editor account, no `manage` folder and no backups. To update it:
1. Copy the new `compose.yaml` and the four scripts (`restore.bat`, `restore.sh`, `reset-editor-password.bat`,
   `reset-editor-password.sh`) into the world folder, over the old `compose.yaml`. Keep `world.toml` and `.env`.
2. Make two empty folders beside them: `manage` and `backups`.
3. Load the new image and start the world, as above.
4. The world makes a backup before the update, brings its database up to date, then makes your first Editor account:
   `manage/first-editor.txt` appears. Carry on from section 10, step 3.

If the log says `this compose.yaml has no admin volume` or `has no manage folder`, step 1 or 2 didn't take: do it
again, then `docker compose up -d`.

**A second world on the same computer** (its `.env` has `COMPOSE_PROJECT_NAME`) also needs an Editor port of its own
before it updates: add `RATW_EDITOR_PORT=17790` to its `.env`. Otherwise the two worlds both want 17789, and whichever
starts second stays down with "port is already allocated" (section 27).

---

## 21. Stopping, starting, and after a restart

| To | Type |
|---|---|
| Stop the world (it saves first, and leaves the list at once) | `docker compose stop` |
| Start it again | `docker compose start` |
| Restart it (after changing `world.toml`, or a token or key file) | `docker compose restart world upnp`, or My world's **⟳ Restart the world** |
| Close it to players for a while, keeping the Editor | My world's **⏸ Close the world**; then **▶ Open the world** |
| Use a changed `.env` | `docker compose up -d` |
| Take its parts away, but keep the world | `docker compose down` |
| Bring them back | `docker compose up -d` |
| Delete the world for good | `docker compose down -v` (section 23) |

Stopping can take up to a minute and a half while the world saves. Let it finish.

**If the world's server stops on its own** (a crash), the world starts it again: after 5 seconds, then 30 seconds, 2
minutes, and every 10 minutes after that while it keeps stopping. The Editor stays up throughout, so you can still
reach your world's tools: its status says "Keeps stopping", with the exit code and when it starts again, and My world
shows the owner its last lines.

**After the computer restarts,** the world starts again by itself once Docker does (section 3), unless you stopped it
with `docker compose stop`. Then it stays stopped until you start it. On Windows, Docker Desktop starts only once
someone signs in: until then the world is down.

**If the computer loses power** or Docker is killed, the world comes back from its last save. It saves often, so little
is lost.

---

## 22. A second world on one computer

1. On the Host page, make a second world: it has its own id and token.
2. Unpack the package again, into another folder (say `C:\RATW\second-world`).
3. In that folder, edit `world.toml` with the second world's name and id, and make a `.env` with a name for it, two
   ports of its own and its token:

       COMPOSE_PROJECT_NAME=ratw-second
       RATW_PORT=7789
       RATW_EDITOR_PORT=17790
       RATW_HOST_TOKEN=rh_...

4. Forward the second game port, 7789, on your router too (section 8), and allow it through the firewall. Forward
   17790 only if you will manage the second world from outside your home.
5. `docker compose up -d` in that folder.
6. Its own `manage/first-editor.txt` appears, with its own code. In the game's EDITOR box the second world is
   `127.0.0.1:17790` (the code already says so).

Without `COMPOSE_PROJECT_NAME` and its own two ports, the second folder would take over the first world, or stop with
"ports are not available" (section 27). The first world needs no change.

Each world takes its own memory and disk (section 1).

---

## 23. Moving to another computer, or starting again

### Moving to another computer

The backups in section 19 hold the world's database. To move a world to another computer, copy everything: the world
folder (with `world.toml`, `.env`, `manage` and `backups`) and three of Docker's storage areas: the database
(`pgdata`), its passwords (`secrets`) and the Editor's key (`admin`). With the third, computers that used your Editor
don't warn after the move.

The commands below use the names for a world in its usual folder: `ratw-world_pgdata`, `ratw-world_secrets` and
`ratw-world_admin`. A second world (section 22) has its own names: `docker volume ls` lists them.

**On the old computer**, in the world folder:

1. Stop the world: `docker compose stop`
2. Copy Docker's storage into one file, `backups/world-copy.tgz` (one line):

   - Windows (PowerShell):

         mkdir -Force backups | Out-Null; docker run --rm -v ratw-world_pgdata:/data:ro -v ratw-world_secrets:/secrets:ro -v ratw-world_admin:/admin:ro -v "${PWD}\backups:/backup" postgres:18 tar czf /backup/world-copy.tgz -C / data secrets admin

   - Linux:

         mkdir -p backups && docker run --rm -v ratw-world_pgdata:/data:ro -v ratw-world_secrets:/secrets:ro -v ratw-world_admin:/admin:ro -v "$PWD/backups":/backup postgres:18 sh -c 'umask 077 && tar czf /backup/world-copy.tgz -C / data secrets admin'

3. Copy the whole world folder to the new computer (a USB stick, or your own network; never a cloud folder or a
   message: section 24).

**On the new computer**, after installing Docker (section 3) and loading the world image (section 4), in the copied
world folder:

1. Make empty storage for the world:

       docker compose down -v
       docker compose create

2. Put the copy back (one line):

   - Windows (PowerShell):

         docker run --rm -v ratw-world_pgdata:/data -v ratw-world_secrets:/secrets -v ratw-world_admin:/admin -v "${PWD}\backups:/backup:ro" postgres:18 tar xzf /backup/world-copy.tgz -C /

   - Linux:

         docker run --rm -v ratw-world_pgdata:/data -v ratw-world_secrets:/secrets -v ratw-world_admin:/admin -v "$PWD/backups":/backup:ro postgres:18 tar xzf /backup/world-copy.tgz -C /

3. Start it:

       docker compose up -d --force-recreate

   (Not `docker compose start`: the database wouldn't come up.)

The log then says `world already holds package ...; nothing to do.`: your world is there, with its Editor accounts
and the same fingerprint. Forward the ports to the new computer (section 8). On the Editor's EDITOR box, use the new
computer's address: the game recognises the same key ("*your world*, now at ...").

Then delete `world-copy.tgz` from both computers, or keep it somewhere private: it holds everything.

(Tested on Linux with rootless Docker; not yet on Windows.)

### Starting again from scratch

    docker compose down -v

This **deletes your world for good**: its database, everything that happened in it, its passwords, and its Editor
accounts and key (every computer that used its Editor then shows "the certificate has changed": section 13). Make a
backup first. The next `docker compose up -d` makes a new world from the package you have now, with a new
`first-editor.txt`. Your world id and token stay the same. The `backups` folder isn't deleted, but a new world can't
take the old world's backups.

---

## 24. Never zip your world folder to send it

Your world folder isn't the package any more. Once the world has run, it holds:
- `.env`, with your **host token**;
- `manage/first-editor.txt`, with an **Editor code** (until it says "Used");
- `backups`, with **everything players did and said** in your world, and your Editor accounts' password hashes;
- perhaps your **AI key** in `world.toml` or a key file.

So **never zip it, upload it, or put it in a shared or synced folder** to send someone. To help a friend host, send
them the package from the game's team instead. To get help, send the log (section 27) after reading it through. A way
to share your world's map with someone ("Share my world") comes in a later update.

If you sent it by mistake: make a new host token (section 2, **NEW TOKEN**), reset every Editor account that hadn't
chosen a password (section 16), and change your AI key on OpenAI's site.

---

## 25. Being the host in the game

Your game account is your world's host. Any of your wolves has the host's commands there, once central has accepted the
world's heartbeat with your token, even before it is listed (joining by `127.0.0.1:7788`, say). Without a token,
nobody does.

**The host's console:** in the game, press the **`** key (the key under Esc, left of 1). It opens over the map, headed
"HOST'S CONSOLE". Type a command and press Enter. Tab completes a command; Esc or ` closes it.

A wolf is named by its name, or by its `wolf-` id if two wolves share a name. You can't use these on your own
account.

| Command | What it does |
|---|---|
| `/help` | Lists these commands. |
| `/pending` | What waits on you: treaties, House requests and storyteller applications. |
| `/treaty <treaty-N> yes\|no` | Agrees to a treaty, or turns it down. One not decided in a game day settles by the faction's own rule. |
| `/house <chapter-N> <faction> yes\|no` | Recognises a Chapter as a minor House, or not. Likewise settles itself after a game day. |
| `/storyteller <wolf> yes\|no\|revoke [reason]` | Decides an application to tell stories, or takes the standing back. You may approve yourself. |
| `/kick <wolf>` | Sends the wolf out of your world. It may come back. |
| `/ban <wolf> [reason]` | Bans the wolf's account from your world (all of its wolves), and sends it out. |
| `/unban <wolf>` | Lifts your world's ban on that account. |
| `/silence <wolf> 1\|6\|24\|72` | That account can't speak in world chat, or send private messages or letters, in your world for that many hours. |
| `/unsilence <wolf>` | Lifts the silence. |

Good to know:
- Every use is written in the world's log (`RATW_HOST ...`) and its event log.
- Your bans and silences are for your world only. Central's own bans (of an account, by the game's team) reach every
  world, yours too.
- Only storyteller applications wait for you for ever. Check `/pending` now and then (the Editor's Players and
  Chapters tabs show them too).
- When you, or a co-builder allowed to, act on a player's wolf in the Editor, the player is told in the game.
- Portrait uploads are off on community worlds. The painted portraits stay.
- Where the game would say "the DM", your world says "the host".

---

## 26. What you can see, and your duties

Players are told, on the world list and when they first come to a community world: "A community world is run by its
host. The host can read everything you say and do there, and sees your internet address; co-builders they add see what
the host's tools show. Your wolf's name is the same on every world." The list also marks a world that uses AI ("AI
used") or cheats ("cheats on").

**As the host you can see, in your world's database and logs:**
- every wolf that has been in your world: its name and look;
- what wolves say in world chat, and their private messages and letters to one another in your world;
- where they go and what they do there, what the residents remember of them, and the world's event log;
- the internet address each player connects from (the `RATW_CONNECT` lines in the log);
- with an AI key, what players say to residents goes to OpenAI under your account.

**Your co-builders** see what the Editor shows their role (section 14).

**What the Editor hides on purpose.** The tools don't show everything the database holds. They make harvesting
players' data inconvenient, and limit what a stolen session or a co-builder can see. They don't stop you, the host.
- Players' accounts appear only as short ids and wolf names.
- **Reports:** the reported wolf, the reason and the lines the reporter received from it, marked "supplied by the
  world". **The reporter is hidden**, so nobody can get back at them. Reports about your own wolves never appear in
  your Editor.
- **Conversations with residents:** the resident's side and summaries only, never the player's typed words.
- **Circles:** their members only, never their chat.
- **No players' internet addresses** appear in the Editor. (Editor accounts' sign-in addresses do, to the owner.)

**You can't see:** players' account names, email addresses or passwords, or their wolves on other worlds. Each world
knows an account by an id of its own. Wolf names are the same on every world, though, so a wolf is recognisable
wherever it goes.

**Your duties.** Players trust you with what they do and say in your world.
- Don't publish or pass on players' messages, letters or addresses, and don't use the Editor to dig into them.
- You answer for the co-builders you add. Give each the smallest role that does the job.
- Keep your world folder, your backups, your host token and your Editor codes private (sections 15 and 24).
- Tell your players if your world uses AI: their words to residents go to OpenAI. (The list shows it too.)
- Never ask a player for their game password, and don't make residents that do. The game never asks for it inside a
  world.
- Players must be 16 or older to have an account.
- **Backups keep what players did,** deleted wolves too: a daily backup for about a week (7 are kept), a pinned one
  for up to 90 days, the 5 newest of the others until newer ones replace them, and none past 90 days unless pinned (the
  newest one always stays). Don't pin backups you don't need.
- Host terms, which make the host responsible for their world's data, are still being written. So is a way for central
  to tell your world when a player deletes a wolf. When they come, this guide will say what they ask of you.

Remember also that your home's internet address is on the list (section 8).

---

## 27. Something went wrong

First look at the log: `docker compose logs world`. Find the message below (the start of it is enough).

### The short table

| What went wrong | What fixes it | What it costs |
|---|---|---|
| An NPC action in the DM tool was a mistake | Its opposite action, where one exists (each question says whether there is one) | Nothing |
| A player's wolf was hurt by mistake | Bring it back, or move it back (Players, or LIVE) | The player's lost time |
| The economy went wrong | The Economy tab: move the steering slider back, or undo the edict or rule you changed | The rules you changed |
| The world keeps stopping | My world's last lines; `docker compose logs world`; a restore if nothing else helps (section 19) | Depends |
| Everything is broken | `restore.bat` or `restore.sh` with a backup (section 19) | **Everything every player did since that backup** |
| The Editor can't connect | The next table | Nothing |
| "STOP: THE CERTIFICATE HAS CHANGED" | Don't sign in. If you made a new world, lost Docker's storage or ran `new-certificate --leaked` yourself, compare the fingerprint, then forget the old one, or paste a new Editor code (section 13). Two worlds on one computer? A code made before the second world had its own Editor port points at the first world's: make a new one for it with its reset script (section 16) | Nothing |
| A lost password, or "too many tries" | Double-click `reset-editor-password.bat` in the world folder on the world's computer (section 16); or, for a co-builder, NEW CODE on EDITOR ACCOUNTS | Nothing |
| The owner on a new computer can't get in | `docker compose exec world ratw-editor invite NAME` for an Editor code (section 12) | Nothing |
| Someone you don't know signed in | EDITOR ACCOUNTS: END ALL EDITOR SESSIONS, then NEW CODE or REMOVE for that account; check the account lines on the menu; `ratw-editor new-certificate --leaked` if the world folder or Docker's storage leaked | Nothing |
| `docker compose up -d` says "ports are not available" or "port is already allocated" | `RATW_EDITOR_PORT=17800` (or another port) in `.env`, then `docker compose down` and `docker compose up -d` (below) | Nothing |

### The Editor can't connect, or won't sign in

The game says why, in this order:

| The game says | What it means, and what to do |
|---|---|
| "Is Docker Desktop running, and is the world started (docker compose up -d in its folder)? ..." | On the world's own computer (`127.0.0.1`): start Docker Desktop, then `docker compose up -d` in the world folder. If it is running, `docker compose logs world` says what's wrong (a line starting `RATW_PACKAGE error:`). |
| "Your world's Editor answers only its own computer until you allow others: add RATW_EDITOR_BIND=0.0.0.0 ..." | From another computer: open the Editor port (section 12). Then check the network is Private and the firewall (section 12). |
| "Is the Editor port right? It is 17789 unless your .env sets RATW_EDITOR_PORT. The Editor may also be turned off ..." | Type the address with your port (`192.168.1.20:17800`), or check `[editor] enabled` in `world.toml`. |
| "From outside your home, your router must forward TCP 17789 ..." | From the internet: forward the Editor port (section 12), and test from a phone's hotspot. From inside your home, use the home address instead. |
| "The world's game answers at this address, but its Editor doesn't. ..." | The world runs but no Editor answers: your package is older than the Editor (section 20), or the Editor is turned off, or the port isn't open to this computer. |
| "That's your world's game port. The Editor uses port 17789 ..." | You typed the game's port (7788). Leave the port out, or use 17789 (or your `RATW_EDITOR_PORT`). |
| "This address isn't a RATW world's Editor." | Something else answers there. Check the address. |
| "Your world's package is older than the Editor: update it ..." | Section 20. |
| "This world's Editor is newer than your game: update the game." | Update the game. |
| "This world's Editor isn't an official RATW build." | The world's Editor wasn't signed by the game's team: the game won't open it. Use the world image the team sent. |
| "Official worlds can't be edited." | Only community worlds have an Editor. |
| "Your world is still setting up (…%). This page waits for it." | Normal on a first start or an update: wait. |
| "Your world answered, but its database is slow: try again in a minute." | Wait a minute. If it lasts, `docker compose logs db`; a full disk is a common cause. |
| "Wrong name or password." | Check both. The name is in `manage/first-editor.txt` or under RECENT WORLDS. |
| "Too many tries from here: wait N minutes." | Someone (perhaps you) typed wrong passwords from here. A computer that has signed in before, and an Editor code, still get through: on the world's computer, `docker compose exec world ratw-editor invite NAME` makes a code and keeps your password (section 16). Or wait. |
| "That one-time password has expired: ..." | Ask the owner for a new code, or on the world's computer run the reset script (section 16). |
| "This world asks for an Editor code to sign in from a new computer ..." | The world has `new_devices = "approve"`: get a code from `ratw-editor invite NAME` (or the owner's NEW CODE). |
| "This account is turned off: ask the world's owner." | The owner turned it off. |
| "Too many people signed in to this world's Editor: try again later." | At most 10 sessions at once. |
| "Your Editor session ended: sign in again ..." | 30 minutes without use, 12 hours in all, a password change, or END ALL EDITOR SESSIONS. Sign in again. |

### The world doesn't start (`RATW_PACKAGE error: ...`)

A mistake in `world.toml` keeps the world waiting: it reads the file again after 5 seconds, 30 seconds, 2 minutes and
then every 2 minutes (the log says `next try at ...`), and starts once it is right. Put it right, then `docker compose
restart world upnp` (or `docker compose up -d`, if what you changed is `.env`).

| Message | What it means, and what to do |
|---|---|
| `no /etc/ratw/host/world.toml: copy world.toml beside compose.yaml` | There is no `world.toml` in the folder (or it is called `world.toml.txt`, or there is a folder of that name). Delete any folder called `world.toml` there, put the file beside `compose.yaml`, then `docker compose up -d`. |
| `world.toml is not valid TOML: ...` | A typing slip in `world.toml`: often a missing quote, or a word without quotes. The message gives the line. |
| `world.toml: [world], [central], [ai], [server], [network] and [editor] are tables.` | One of those names was given a value (`world = "..."`) instead of being a heading, `[world]`. Put the heading back, with its settings under it. (A misspelt heading, such as `[World]`, gives no message of its own: its settings are ignored, so the error is that the name, the id or central's url is missing.) |
| `world.toml: give the world a name ([world] name = "...").` | Give the world a name. |
| `world.toml: the name is N bytes; at most 48 ...` | The name is too long. Accented letters count two or more. Make it shorter. |
| `world.toml: the name has a control character in it.` | Retype the name. |
| `world.toml: X is N characters; at most M.` | That setting is too long. Shorten it. |
| `world.toml: X must be text ("...").` | Put that setting in quotes. |
| `world.toml: X must be true or false.` | Write `true` or `false`, without quotes. |
| `world.toml: X must be a whole number from A to B.` | Write a number in that range, without quotes. |
| `world.toml: tags is a list of words, ["a", "b"].` | Write tags as `["a", "b"]`. |
| `world.toml: up to 5 tags, 16 characters each at most.` | Fewer tags, or shorter ones. |
| `world.toml: cheats is "off", "host" or "everyone".` | Write one of those three, in quotes (section 17). |
| `world.toml: [central] url is needed ...` | Central's address is missing (or its `[central]` heading is). Put in the address the game's team gave you. |
| `world.toml: central's url is https://name (no path).` | Central's address has something after the name. Put it in as the game's team gave it. |
| `world.toml: a world on central needs its id ([world] id = "world-...") ...` | The world id is missing or mistyped. Copy it from the Host page (section 2). |
| `world.toml: secure = true needs host = "a.dns.name" ...` | Set `secure = false`, unless you have a proxy with a certificate. |
| `world.toml: cost_mode is generous, balanced or frugal.` | Write one of those three. |
| `world.toml: [editor] owner is 3 to 32 small letters, digits, - and _ ...` | Change the name, or leave `owner` out. Never put a password there. |
| `world.toml: [editor] new_devices is "open" or "approve".` | Write one of those two, in quotes. |
| `RATW_EDITOR_PORT is a port number` | `RATW_EDITOR_PORT` in `.env` must be a number from 1 to 65535, with nothing else on the line. |
| `world.toml: the AI key has a space or an unusual character in it.` | Copy the key again, whole, with nothing else. |
| `world.toml names a file for the host token: there is no ...` (or `for the AI key`) | The file named by `token_file` (or `key_file`) isn't there. Check its name, and that it is in the world folder, beside `world.toml`. On Windows, check it isn't called `host-token.txt` (section 5). |
| `... can't be read (permission denied). Make it a file of the user that runs Docker.` | On Linux, the file belongs to another user. `sudo chown $USER FILE`. |
| `... is not UTF-8 text.` | Save the file again as UTF-8 plain text. |
| `... is empty: put the host token from the Host page in it.` | The token file is empty. Paste the token in it. |
| `...: the host token has spaces or odd characters in it: copy it again from the Host page.` | The token was pasted with something else. If it is lost, make a new one (section 2). |
| `world.toml: central's url is http:// but ...: the host token would go in clear.` | Central's address must start `https://`. |
| `the database passwords can't be read from ...; is the db service running?` | The database didn't start. See `docker compose logs db`. |
| `the database never answered: ...` | The database didn't come up within three minutes. See `docker compose logs db`. A full disk is a common cause. |
| `this image holds no world package ...` | The world image is a developer's build without a world. Use the image that came with the package. |
| `the world package could not be imported (exit N); see above.` | The lines just above say why. Ask the game's team for help, with the log. |

### Docker's own messages: "ports are not available" and others

| Message | What it means, and what to do |
|---|---|
| Windows: `Ports are not available: exposing port TCP 127.0.0.1:17789 ... bind: An attempt was made to access a socket in a way forbidden by its access permissions` | Windows has set the Editor's port aside (Hyper-V and WSL reserve ranges of ports), or another program holds it. The whole world then stays down, game port included. This can happen on the update that adds the Editor, to a world that ran the day before. Put another Editor port in `.env`: `RATW_EDITOR_PORT=17800`, then `docker compose up -d`. To see Windows' reserved ranges, in PowerShell: `netsh interface ipv4 show excludedportrange protocol=tcp`; choose a port outside all of them. |
| Windows: `Ports are not available: ... 17789 ... bind: Only one usage of each socket address ... is normally permitted` | Another program, or another world, has the Editor's port: the same fix. |
| Linux: `Bind for 127.0.0.1:17789 failed: port is already allocated` (or `0.0.0.0:17789`, or `address already in use`) | The same: `RATW_EDITOR_PORT=17800` in `.env`. Often it is a second world without its own Editor port (section 22): then give *that* world `RATW_EDITOR_PORT=17790` instead, and `docker compose up -d` in its folder. |
| Any of these naming **7788** (or your `RATW_PORT`), the game's port | Another program, or another world, has the game's port. Stop it, or give this world another port in `.env` (`RATW_PORT=7790`), and forward that one instead (section 8). |
| `invalid containerPort: ...`, or `... is missing a target port` | `RATW_PORT` or `RATW_EDITOR_PORT` in `.env` must be a number from 1 to 65535, with nothing else on the line (no spaces, no quotes). |
| `pull access denied for ratw-world, repository does not exist ...` | The world image isn't on this computer, and isn't published yet. Load the image file the game's team sent: `docker load -i ratw-world.tar` (section 4). |
| `bind source path does not exist: .../world.toml` | There is no `world.toml` in the folder: it is missing, or called `world.toml.txt` (section 5). |
| `docker: command not found`, or `'docker' is not recognized` | Docker isn't installed, or the terminal was open before it was. Open a new terminal. |
| `Cannot connect to the Docker daemon ...`, or `error during connect ...` | Docker isn't running. Start Docker Desktop (Windows), or `sudo systemctl start docker` (Linux). |
| `no configuration file provided: not found` | The terminal isn't in the world folder. `cd` into it. |

**After any of these, type `docker compose down`, then `docker compose up -d`** (your world is kept). A plain `up -d`
after a refused one can start the world without its network: the log then says `waiting for the database (... failed to
resolve host 'db' ...). This part of the world isn't on its network`, and it waits for ever. `down` and `up -d` mend it.

Turning the Editor off (`[editor] enabled = false`) doesn't free its port: Docker still holds it. Change the port
instead.

### The world runs, but isn't listed

Read the `RATW_AGENT world agent:` lines in the log, or the reason on the Host page.

| Message | What it means, and what to do |
|---|---|
| `RATW_PACKAGE warning: no host token ...` | No token was given. Put it in `.env`, then `docker compose up -d`; or in a token file (section 5), then `docker compose restart world`. |
| `not listed on central: Central could not reach this world at ADDRESS: it did not answer within 5 seconds. Open port 7788 to the internet (TCP).` | The port isn't open. Forward it on your router and allow it through the firewall (section 8). |
| `... the connection was refused. Open port 7788 ...` | The forward reaches a computer that isn't running the world on that port. Check the forward's address and port, and that the world is up. |
| `... its /status answered HTTP 429. Open port 7788 ...` | The port is open, but the world turned central's check away: one address asked for its status more than 30 times in a minute. Under Docker Desktop and rootless Docker every player can seem to come from one address, so players' games looking at the world list use up that allowance. A known limit for now. It clears when fewer people look. |
| `This world's address (IP) is not on the internet.` | You are behind carrier-grade NAT, or another router (section 9). |
| `The world at ADDRESS is another world (its /status names ...), not the one this host token is for.` | Something else at that address answers, often another world of yours. Give each world its own port and its own token. |
| `THE HOST TOKEN IS FOR WORLD ..., BUT THE SERVER ON PORT 7788 IS WORLD ...` | The token and the `id` in `world.toml` are for different worlds. Copy the id that goes with this token, then `docker compose restart world`. |
| `central refused the heartbeat (bad_token): ...` | The token was replaced, removed or never made, or expired after 90 days unused. Make a new one (section 2), put it in place of the old one, then `docker compose up -d` (token in `.env`) or `docker compose restart world` (token in a file). |
| `central refused the heartbeat (delisted): This world is delisted.` | The game's team has taken your world off the list. |
| `central refused the heartbeat (delisted): This world was removed by its host.` | You removed this world on the Host page. It can't come back: make a new world there, and put its id and token in. |
| `central refused the heartbeat (banned) ...` | Your account is banned. |
| `central refused the heartbeat (name_refused): ...` | Change the name, description, region or tags in `world.toml`, then `docker compose restart world upnp`. |
| `... A community world cannot call itself official or use the game's name (Run Against the World, RATW).` | Likewise: change the name or text. |
| `The host name ... does not resolve (DNS) ...`, `... is not a public DNS name ...` or `... does not point at this world's address ... alone ...` | Your `host` setting is wrong. Point that name at your internet address alone, or leave `host` out. |
| `could not reach central: ...` or `central did not answer in time ...` | Your computer can't reach central: your internet connection, a firewall blocking outgoing traffic, or central is down. The agent keeps trying every minute. |
| `could not fetch central's keys: ...` | The same: central can't be reached. Until the keys come, nobody can join. |
| `the world on port 7788 does not answer (...): no heartbeat until it does` | Normal for the first seconds of every start (section 7). If it is the last `world agent:` line well after that, the world server isn't up, or stopped: read the lines above. |
| `this machine's clock is N s from central's: tickets allow two minutes; set the clock right` | Your computer's clock is wrong. Turn on automatic time. On Windows, Docker's clock can drift after the computer sleeps: restart Docker Desktop. |
| `RATW_PACKAGE warning: private worlds (a password kept on central) aren't built yet ...` | Set `private = false`. The world is listed as public. |

### Players can't get in, and other trouble

| Message, or what happens | What it means, and what to do |
|---|---|
| `RATW_SERVER warning: RATW_TICKETS no keys from central yet ...`, and it stays | The agent can't fetch central's keys. See "could not fetch central's keys" above. |
| `RATW_SERVER RATW_ARRIVAL refused: this world holds N player economy accounts, its most` | The world is full of wolves that have ever arrived. Ask the game's team for help. |
| `RATW_PACKAGE warning: the NPC Mind did not start (see logs/npc-mind.log) ...` | Something is wrong with the AI key or OpenAI. Read the Mind's log: `docker compose exec world cat logs/npc-mind.log`. The residents use their written lines meanwhile. |
| `RATW_PACKAGE the world is closed for maintenance: it starts again once it is opened ...` | The world was closed (My world's **⏸ Close the world**). It starts as soon as it is opened again (**▶ Open the world**). |
| `RATW_PACKAGE the world server stopped (exit N); starting it again in ...` | The world server stopped by itself. The lines above say why. The world starts it again, waiting longer each time (section 21); the Editor stays up. If it keeps stopping, ask for help, with the log. |
| `RATW_PACKAGE the world waits until world.toml is put right ...` | `world.toml` has a mistake: the line above says what. Put it right, then wait for the next try, or use **⟳ Restart the world**, or `docker compose restart world`. |
| `RATW_PACKAGE warning: this compose.yaml has no admin volume ...`, or `... has no manage folder ...` | Your `compose.yaml` is from before the Editor (section 20, "A world from before the Editor"). |
| `RATW_PACKAGE warning: no backup was made before the update ...` | The backup part didn't answer (an old `compose.yaml`: section 20) or the disk is nearly full (`docker compose logs backup`). The update went on anyway. |
| `RATW_BACKUP refused: ... at least 2048 MB must stay free ...` (in `docker compose logs backup`) | The disk is nearly full. Free some space (section 19). |
| `UPnP: no router answered the search from inside this container ...` (in `docker compose logs upnp`) | Normal on Windows (Docker Desktop) and with rootless Docker: forward the port by hand (section 8). |
| `UPnP: port 7788 is already forwarded on the router to another computer ...` | Another computer has that port. Change the forward on the router, or give this world another port. |
| `UPnP: the router doesn't let programs open ports ...` | Forward the port by hand. |
| `UPnP: the router's internet address, ..., is in 100.64.0.0/10 ...` or `... is private ...` | Carrier-grade NAT, or a second router in front of yours (section 9). |
| You can't join your own world from the list | Your router may not reach your own address from inside your home. Use **JOIN BY ADDRESS** with `127.0.0.1:7788` (section 7). |
| The list says "This world is older than your client: its host needs to update it" | A new game version is out, and players with it can't join your world. Update (section 20). |

### Asking for help

Save the log to a file: `docker compose logs world > world-log.txt`. Read it through first: it holds players'
internet addresses. Send the log, never the world folder (section 24).
