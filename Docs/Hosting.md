# Hosting a Ridgemere world

This guide is for anyone who wants to run their own world of Ridgemere on their own computer, for other players to
join from the game's world list. You don't need to be technical. You will type a few commands, and each one is given
in full.

What a hosted world is (Docs/Design/77, sections 3.2 and 8):
- **Your world** runs on your computer: the map, the residents, their stories and their economy. It is yours. What
  happens in it stays in it.
- **Central** is the game's own service. It holds players' accounts and wolves, signs them in, and keeps the world
  list. Your world tells central every minute that it is up, and central lists it.
- **A wolf** is a player's character. Its name and look come from central. What it does in your world (its money,
  goods, home, friends) stays in your world.

Where things stand (2026-11-08; the Editor's first parts 2026-11-12):
- This is the first version (v1). During the closed beta, an account can host only once the game's team has approved
  it.
- During the closed beta, the game's team sends you the package, the world image as a file, and central's address
  (section 4). The image isn't on a public registry yet, so `docker compose pull` can't fetch it.
- Tested so far on a Fedora Linux laptop with rootless Docker. Not yet tested on Windows with Docker Desktop.
- No Mac. The world is built for 64-bit Intel and AMD processors (x86_64) only.
- Under Docker Desktop and rootless Docker, every player may seem to come from one address. The world answers at most
  30 status requests a minute from one address, so there a busy world can be marked not reachable (section 17,
  "HTTP 429"). Linux with Docker Engine (not rootless) keeps each player's own address.

Contents:
1. What you need
2. Your world's id and host token
3. Installing Docker
4. Getting and unpacking the package
5. Your settings: `world.toml` and `.env`
6. Starting the world
7. Checking it worked
8. Letting players in: your router's port
9. Carrier-grade NAT: when no port can be opened
10. NPC voices and your own AI key
11. Updates
12. Backups
13. Stopping, starting, and after a restart
14. A second world on one computer
15. Being the host in the game
16. What you can see, and your duties
17. Something went wrong

---

## 1. What you need

**A computer that stays on**, and doesn't go to sleep, while people play (on Windows, section 3 says how):
- **Linux** on x86_64, with Docker Engine and its Compose plugin; or
- **Windows 10 or 11** (64-bit) with **Docker Desktop**. It needs WSL 2 and virtualisation turned on in the computer's
  firmware (BIOS or UEFI). Docker Desktop's installer says if it isn't.

**What it takes.** These were measured on 2026-11-08 on the test laptop, with nobody playing:

| What | Measured |
|---|---|
| First download | about **250 MB** (the world image is about 82 MB of it; PostgreSQL, the database, most of the rest) |
| Disk, images | about 310 MB for the world image and about 650 MB for PostgreSQL's, once unpacked |
| Disk, the world's database | about 340 MB for a new world |
| Memory, idle | the world about **760 MiB**, the database about **215 MiB**: about 1 GB in all |
| First start | about **18 seconds** (it makes the database and imports the world) |
| Later starts | a few seconds (the world is already there) |

**Not measured yet:**
- memory and processor use with players in the world;
- how many players a home computer can hold;
- how fast the database grows as the world is played;
- how much of your internet connection each player uses.

So keep some room to spare. The world saves itself by copying itself in memory for a moment, which can briefly need
up to twice the world's memory. A computer with 4 GB of memory free for the world should be comfortable, but that is
a guess, not a measurement. On Windows, Docker runs inside a small Linux machine (WSL 2), which by default may use up
to half of your computer's memory (section 3 says how to give it more).

**An internet connection where you can open a port** on your router (section 8). Some home connections can't
(section 9). A rented server (a "VPS") always can.

**A game account approved as a host**, and the game's client to make your world's token with (section 2).

---

## 2. Your world's id and host token

Your world needs two things from central: its **id** and its **host token**. You get both from the client.

1. Open the game's client and sign in.
2. On the screen with your wolves, choose **HOST A WORLD**. (You only see it once your account may host.)
3. Under "A new world", give it a name of 1 to 48 characters, and choose **MAKE A WORLD**.
4. The **token** is shown **once**, in a box at the top, with a **COPY** button. It starts `rh_`. Copy it into a file
   for now (section 5 says where it goes).
5. Under the token is the command to run a world by hand. Your world's **id** is in it, after `--world-id`. It starts
   `world-` and has 32 letters and digits after that, for example `world-0123456789abcdef0123456789abcdef`. Copy it
   too.

Keep the token secret.
- Anyone with it can put a world on the list as yours. It can't sign anyone in, and it can't read anything.
- The token is shown only this once. If you lose it, or it leaks, choose **NEW TOKEN** beside your world. The old one
  stops working at once, and the new one is shown with the id again. Put it where the old one was (section 5), then
  type `docker compose up -d` if it is in `.env`, or `docker compose restart world` if it is in a token file.
- A token that isn't used for 90 days expires. Make a new one the same way.

An account can have up to 3 worlds. **REMOVE** takes a world off central for good. Its save, on your computer, is
kept.

The Host page also shows each world as **LISTED** or **NOT LISTED**, and why not (section 7).

---

## 3. Installing Docker

The world comes as a Docker package: three small "containers" that Docker runs for you (the world, its database, and
a small helper that asks your router to open the port). You don't need to know how Docker works.

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

**Check it:** open a terminal (on Windows, PowerShell) and type:

    docker compose version

It should print a version. If it says "command not found", Docker isn't installed yet, or the terminal was open
before it was: open a new one.

---

## 4. Getting and unpacking the package

During the closed beta, the game's team sends you three things:
- **the package**: a folder, or a `.zip` of one, holding `compose.yaml`, `world.toml`, `README.md` and this guide
  (`Hosting.md`);
- **the world image**, as a file (for example `ratw-world.tar`);
- **central's address**, for `world.toml` (section 5).

1. Unpack the package into a folder of its own. For example:
   - Windows: `C:\RATW\my-world`
   - Linux: `~/ratw/my-world`
2. The folder holds `compose.yaml`, `world.toml`, `README.md` and `Hosting.md`, and two empty folders, `manage` and
   `backups`. Keep them: the world writes your Editor account into `manage` (section 15), and without the folder
   Docker would make one itself, which on Linux only root could open.
3. Open a terminal in that folder.
   - Windows 11: in File Explorer, right-click inside the folder and choose **Open in Terminal**. Or in PowerShell:
     `cd C:\RATW\my-world`
   - Linux: `cd ~/ratw/my-world`
4. Load the world image into Docker. Put the image file in that folder, and type:

       docker load -i ratw-world.tar

   It ends `Loaded image: ratw-world:latest`. If it names another image, put that name in `.env` as
   `RATW_IMAGE=...` (section 5). You can delete the file afterwards: Docker keeps the image.

Every command in this guide is typed in that folder.

Once the image is on a public registry, step 4 goes away: `docker compose up -d` fetches it by itself.

The folder's name and place don't matter. Your world keeps its name inside Docker (`ratw-world`), so moving or
renaming the folder later keeps the world.

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

Each setting is read when its part of the world starts: `world.toml` whenever the world's server starts, which
the **Restart** button on the Editor window's **Server** tab (section 15) does too. After a change:

| You changed | Then type |
|---|---|
| `world.toml`, or a token or key file it names | `docker compose restart world upnp` (both read `world.toml`), or the **Restart** button on the Editor window's **Server** tab (the `upnp` helper then keeps the old `[network]` setting) |
| `.env` | `docker compose up -d` (a restart doesn't read `.env` again) |

If something in `world.toml` is wrong, the world doesn't start. The log says what is wrong (section 7), and the world
waits: it reads `world.toml` again after 5 seconds, 30 seconds, 2 minutes and then every 2 minutes, and the log says
when it tries next. Put it right and the world starts at its next try, or at once with `docker compose restart world
upnp`. If the world has started before, the Editor stays up meanwhile, and its status says "world.toml has a
mistake".

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
| `id` | Your world's id, from the Host page (section 2). | **Needed.** `"world-"` and 32 letters and digits. |

### `[central]`: the game's service

| Setting | What it is | Rules |
|---|---|---|
| `url` | Central's address. | **Needed.** During the closed beta the package holds an example address here (`https://central.example.org`), which isn't a real central: put in the address the game's team gave you (section 4). `https://name`, with no path after it. |
| `token_file` | The name of a file holding your host token (another way to give the token). | Optional. A file in the world's folder, beside `world.toml`, given by its name alone (a full path doesn't work: the world sees only its own folder). See "The host token" below. |
| `host` | A DNS name for your computer, listed in place of its number. | Optional, at most 253 characters. It must point at your internet address and nothing else. Local names (`.local`, `.lan`, `.home.arpa`, one-word names) are refused. If you don't know what this is, leave it out. |
| `secure` | `true` only if players reach the world through `wss://`: a proxy in front of it with a certificate for `host`. | `false` unless you have set that up. `true` needs `host`. For experienced hosts only. |

Without a host token the world still starts, and players who know its address can join it with the client's **JOIN
BY ADDRESS** box. But it isn't on the list, nobody has the host's commands (section 15), and central's ban list doesn't
reach it.

### `[ai]`: the residents' voices

All optional. Without a key, the residents speak their written lines, which work well. See section 10 first: a key
costs you money.

| Setting | What it is | Rules |
|---|---|---|
| `key` | Your OpenAI API key. | Optional. No spaces or unusual characters. It stays on your computer: it is never sent to central. |
| `key_file` | The name of a file holding the key, instead of `key`. | Optional. A file in the world's folder, beside `world.toml`, given by its name alone. Used only when `key` is empty. |
| `model` | The residents' main voice. | If left out: `gpt-5.4-mini`. |
| `light_model` | Short replies. | If left out: `gpt-5.4-nano`. |
| `fallback_model` | Used when the main one fails. | If left out: `gpt-5.4-mini`. |
| `story_model` | The stories. | If left out: `gpt-5.6-luna`. |
| `cost_mode` | How much talk to buy: `generous`, `balanced` or `frugal`. | If left out: `balanced`. |
| `monthly_tokens` | The most your key is used for in a month, by the Editor's tools (life stories) and the residents together, in tokens. | A whole number. If left out: 20,000,000. |

The model defaults are what the official world uses (Docs/Design/28). Leave them unless you know why.

### `[server]`

| Setting | What it is | Rules |
|---|---|---|
| `workers` | The game's worker threads. | A whole number from 0 to 16. If left out: the processors Docker gives the world, less two. Leave it out unless the world competes with other programs. |

### `[network]`: asking the router to open the port (UPnP)

| Setting | What it is | Rules |
|---|---|---|
| `upnp` | Whether to ask your router, when the world starts, to forward its port to your computer (section 8). | `true` or `false`. The package has `true`; if left out, also `true`. `false` turns the attempt off. The `upnp` helper reads it when it starts: after a change, `docker compose restart world upnp`. |

### `[editor]`: your world's Editor

The Editor is your world's own tools, reached from the game's EDITOR button (section 15).

| Setting | What it is | Rules |
|---|---|---|
| `enabled` | Whether the Editor answers at all. | `true` or `false`. If left out: `true`. With `false`, nothing answers on the Editor port. |
| `owner` | The name of your first Editor account. | Optional: 3 to 32 small letters, digits, `-` or `_`, starting with a letter. If left out, the world makes one up (`host-` and four letters and digits). Used only the first time the world starts. **Never put a password here.** |
| `new_devices` | `"open"`: a name and password sign in from any computer. `"approve"`: a computer that hasn't signed in before also needs an Editor code. | If left out: `"open"`. Choose `"approve"` if you open the Editor port to the internet. |

### `.env`: settings beside `compose.yaml`

A few settings live in a file called `.env`, in the same folder. It doesn't exist until you make it. Each line is
`NAME=value`, with no spaces and no quotes.

| Setting | What it is |
|---|---|
| `RATW_HOST_TOKEN` | Your host token (see below). |
| `RATW_PORT` | The world's port. If left out: 7788. It is the same port inside Docker and on your computer, and the one to open on your router. |
| `RATW_IMAGE` | The world image to run. Leave it out unless you were given another one. |
| `COMPOSE_PROJECT_NAME` | Only for a second world on the same computer (section 14). |
| `RATW_EDITOR_PORT` | The Editor port. If left out: 17789. Change it only if another program holds 17789 (section 17), or for a second world. |
| `RATW_EDITOR_BIND` | Where the Editor answers. If left out: `127.0.0.1`, this computer only. `0.0.0.0`: every address of this computer, so you can manage your world from another computer (section 15). |

On Windows, Notepad likes to save `.env` as `.env.txt`, which doesn't work: see "Editing them" above.

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

In the world's folder:

    docker compose up -d

The first time, Docker downloads the database's image (PostgreSQL: most of the 250 MB in section 1; the world's
image is the one you loaded), makes the database and imports the world. That takes about 18 seconds once the download
is done. `-d` means it runs in the background: you can close the terminal.

---

## 7. Checking it worked

### Is it running?

    docker compose ps

You should see three lines, `world`, `db` and `upnp`, all **Up**. (`db` also says "healthy".) If `world` says
**Restarting**, something is wrong: read the log. (`upnp` stays up even when UPnP didn't work: section 8.)

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
| `RATW_PACKAGE UPnP: the upnp service asks the router to forward port 7788 (TCP) here; ...` | A reminder that UPnP's own log is separate: `docker compose logs upnp` (section 8). |
| `world: imported package ...: world upper_accord, 1212 areas, ...` | First start only: your world was made. |
| `world already holds package ...; nothing to do.` | A later start: your world is kept as it was. |
| `world: this world was started from package ... and keeps its own state; the image carries package ...` | After an update: your world is kept (section 11). |
| `RATW_PACKAGE the world database is ready (6.4 s)` | The database is ready. |
| `RATW_PACKAGE no AI key: NPCs use their authored lines` | No AI key: the residents use their written lines. |
| `RATW_PACKAGE the NPC Mind is ready (live model)` | Your AI key is in use (section 10). |
| `RATW_AGENT world agent: heartbeats every 60 s for the world on port 7788, to https://...` | The world agent will tell central every minute that the world is up. |
| `RATW_AGENT world agent: 1 key(s): ...; clock offset +0.2 s` | The world agent has central's keys. |
| `RATW_AGENT world agent: the world on port 7788 does not answer (... Connection refused>): no heartbeat until it does` | **Normal on every start**: the agent starts a few seconds before the world server. It tries again every minute. |
| `RATW_SERVER warning: RATW_TICKETS no keys from central yet (...)` | Normal for the first seconds of a first start. The world agent is fetching central's keys. Nobody can join until it has them. |
| `RATW_SERVER RATW_KEYS central's keys: ...; this machine's clock is 0 s from central's` | The world has central's keys: players can join. |
| `RATW_SERVER RATW server listening on port 7788 (0.0.0.0); ready in 3.2 s` | **The world is up.** |
| `RATW_AGENT world agent: listed on central at 203.0.113.7:7788` | **The world is on the list**, at that address. Done. It comes with the agent's next heartbeat, up to a minute after the `listening` line. |
| `RATW_AGENT world agent: not listed on central: ...` | Not on the list, and why. Most often the port: see section 8. |
| `RATW_SERVER RATW_CONNECT ...` and `RATW_SERVER RATW_DISCONNECT ... (1 connected)` | A player came or went. |
| `RATW_SERVER RATW_HOST /kick ... : done` | One of the host's commands was used (section 15). |
| `RATW_AGENT world agent: left central's list`, then `RATW_PACKAGE stopped (the server's last exit: 0)` | The world stopped cleanly and left the list. |

A line starting `RATW_PACKAGE error:` stops the start. Section 17 says what each one means.

The other lines (`RATW_SERVER RATW_TALK`, `RATW_SERVER RATW_VOICE`, `RATW_SERVER RATW_DATABASE_WORLD_LOADED` and the
like) are the world loading its parts. They are fine.

More logs are kept inside the world: the NPC Mind's (`npc-mind.log`) and the world agent's (`world-agent.log`). To list
them: `docker compose exec world ls logs`.

### Being listed

Once the log says "listed on central":
- In the client, the Host page shows your world as **LISTED**. Choose **Refresh** to ask again.
- Your world is on the world list for everyone, with its players online and its limit.

If it says **NOT LISTED**, the reason is beside it, the same as in the log.

### Joining it yourself

You can join with any of your own wolves, from the list. If your router can't reach your own internet address from
inside your home (many can't), join from the same computer with **JOIN BY ADDRESS** and `127.0.0.1:7788`. From another
computer at home, use the world computer's home address, for example `192.168.1.20:7788`.

---

## 8. Letting players in: your router's port

Players, and central, reach your world on **one port: TCP 7788** (or your `RATW_PORT`). Your router must send that
port to the computer that runs the world. This is called **port forwarding**.

Central checks it for you: before it lists your world, and every 10 minutes after, it tries to reach the world from
the internet. If it can't, you see:

    not listed on central: Central could not reach this world at 203.0.113.7:7788: it did not answer within 5 seconds. Open port 7788 to the internet (TCP).

### Maybe: the world asks the router itself (UPnP)

Many home routers let programs ask them to open a port (UPnP). The package has a small third part, `upnp`, that tries
this once, when the world starts. While the world runs it keeps the forward open; when the world stops cleanly it
removes it. To see what happened:

    docker compose logs upnp

It says one line, starting `UPnP:`:
- `UPnP: the router (...) now forwards port 7788 (TCP) to this computer (...)`: it worked. Central's check says
  whether players can really reach it.
- `UPnP: port 7788 is already forwarded to this computer on the router ...: left as it is.`: you (or something else)
  forwarded it already. Fine.
- A line saying the router's internet address is in `100.64.0.0/10` or is private: carrier-grade NAT, or a second
  router (section 9).
- Anything else says why it didn't, and ends "Forward port 7788 (TCP) on your router to this computer by hand
  (README.md)": see "By hand" below.

Don't count on it:
- It works only on **Linux with Docker Engine** (not rootless). Under **Docker Desktop** (Windows) and **rootless
  Docker**, the world can't reach the router's UPnP at all: forward the port by hand.
- Many routers have UPnP turned off.
- It never works behind carrier-grade NAT (section 9).
- It never changes a forward you made yourself, or one for another computer. (It replaces only its own old forward,
  to the address this computer had before, when nothing answers there any more.)
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

  (Docker Engine opens its own published ports past ufw, but rootless Docker doesn't, so the rule does no harm either
  way.)

**6. Wait a minute.** The world agent tells central every minute, and while the world isn't reachable central checks
again each time. The log then says `listed on central at ...`.

### Your address is public

The list shows your world's internet address to every player. That is your home's address. If you'd rather not, run
the world on a rented server (a VPS) instead: the steps are the same on any Linux server.

### Later: a second port

The world uses one port today. A later version plans a second one, for managing your world from the client. When it
comes, this guide will say whether and how to open it.

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
2. **Run the world on a rented server (a VPS)** instead of at home. A small one from any hosting company will do; any
   Linux server works with the steps above.
3. **A tunnel or VPN with port forwarding.** Central lists the address your world's heartbeat comes from. So the
   world's outgoing traffic must leave from the same public address that players connect to. A tunnel that only
   carries incoming connections isn't enough on its own. This hasn't been tested with the world yet.

The world speaks IPv4 only for now. IPv6, which would get round CGNAT for many homes, is a later step.

---

## 10. NPC voices and your own AI key

Without an AI key, the residents speak their own written lines. They work well, and they cost nothing. This is the
default.

With an OpenAI API key in `world.toml`, the residents talk with the model's words. **You pay OpenAI for every word
they say**, on your own OpenAI account.
- The cost hasn't been measured for a community world. It depends on how many players talk to residents, and for how
  long.
- **Set a monthly spending limit** on your OpenAI account before you add the key.
- `cost_mode` sets how freely the main voice is used. Past these, the small model (`light_model`) answers, which costs
  less, but still costs:

  | `cost_mode` | Main-voice replies an hour, whole world | ... to any one player | Overheard talk written live, an hour |
  |---|---|---|---|
  | `frugal` | 200 | 20 | 4 |
  | `balanced` | 600 | 60 | 10 |
  | `generous` | 1200 | 120 | 20 |

- The world keeps a ledger of every model call, with no words in it, in `logs/npc-mind-calls.jsonl`.

What players say to residents is sent to OpenAI, under your key. The world list shows players whether a world's voices
use AI. See section 16.

The key never leaves your computer for central. If the NPC Mind (the part that talks to OpenAI) can't start, the log
says so and the residents use their written lines.

---

## 11. Updates

When a new version of the game comes out, update your world. During the closed beta the game's team sends the new
world image as a file: load it, then start the world on it:

    docker load -i ratw-world.tar
    docker compose up -d

(Once the image is on a public registry, `docker compose pull` then `docker compose up -d` does the same.)

**Your world is kept**, with everything that has happened in it. The update changes the server, and the database's
layout if it needs to. It doesn't change your world's map or residents: a new package's world is only for new worlds.
The log then says:

    world: this world was started from package A (...) and keeps its own state; the image carries package B (...), which is not applied to an existing world; to start fresh from the new package run docker compose down -v (this deletes the world).

A world on an older game version stays on the list, but players whose client is newer see "This world is older than
your client: its host needs to update it", and can't join it. So update when a new version comes out.

If a new version comes with a new `compose.yaml`, copy it over the old one. Keep your `world.toml` and `.env`.

### Starting again from scratch

    docker compose down -v

This **deletes your world for good**: its database, everything that happened in it, its passwords, and its Editor
accounts and certificate (every computer that used its Editor then warns that the key changed). Make a backup first
(section 12). The next `docker compose up -d` makes a new world from the package you have now. Your world id and
token stay the same.

---

## 12. Backups

**There is no backup command yet.** A later version plans one. Until then, you can copy the world's data by hand, as
below. Do it before an update, and before anything you're unsure of.

This copies three of Docker's storage areas ("volumes"): the database (`pgdata`), its passwords (`secrets`) and the
Editor's key and certificate (`admin`). You need the first two: a database without its own passwords can't be opened
by the world. The third keeps the Editor's key, so computers that used your Editor don't warn after a restore.

This was tested on 2026-11-08 on Linux with rootless Docker: a new world's backup was about 106 MB and took about 7
seconds, and a restore brought the world back as it was. It hasn't been tested on Windows yet.

The commands below use the names for a world in its usual folder: `ratw-world_pgdata`, `ratw-world_secrets` and
`ratw-world_admin`. A second world (section 14) has its own names: `docker volume ls` lists them.

### Making a backup

In the world's folder:

1. Stop the world, so the database is still while it is copied:

       docker compose stop

2. Copy the volumes into one file, `backups/world-backup.tgz` (one line):

   - Linux:

         mkdir -p backups && docker run --rm -v ratw-world_pgdata:/data:ro -v ratw-world_secrets:/secrets:ro -v ratw-world_admin:/admin:ro -v "$PWD/backups":/backup postgres:18 sh -c 'umask 077 && tar czf /backup/world-backup.tgz -C / data secrets admin'

   - Windows (PowerShell):

         mkdir -Force backups | Out-Null; docker run --rm -v ratw-world_pgdata:/data:ro -v ratw-world_secrets:/secrets:ro -v ratw-world_admin:/admin:ro -v "${PWD}\backups:/backup" postgres:18 tar czf /backup/world-backup.tgz -C / data secrets admin

3. Start the world again:

       docker compose start

Give each backup its own name (rename the file, say `world-2026-11-08.tgz`), or the next one replaces it.

**Keep backups private.** They hold everything in your world, players' messages too, and the database passwords.
On Linux the command above makes the file readable by you alone. (With Docker Engine, not rootless, the file belongs
to root: use `sudo` to copy it elsewhere.) On Windows, keep the world's folder, or at least its backups, inside your
own user folder, where other accounts on the computer can't open them.

### Restoring a backup

This replaces your world with the one in the backup.

1. Delete the world as it is now, and make empty storage for it:

       docker compose down -v
       docker compose create

2. Put the backup back (one line):

   - Linux:

         docker run --rm -v ratw-world_pgdata:/data -v ratw-world_secrets:/secrets -v ratw-world_admin:/admin -v "$PWD/backups":/backup:ro postgres:18 tar xzf /backup/world-backup.tgz -C /

   - Windows (PowerShell):

         docker run --rm -v ratw-world_pgdata:/data -v ratw-world_secrets:/secrets -v ratw-world_admin:/admin -v "${PWD}\backups:/backup:ro" postgres:18 tar xzf /backup/world-backup.tgz -C /

   (A backup made before the Editor came has no `admin` in it: that is fine. The world makes a new Editor key, and
   computers that used the Editor warn once.)

3. Start it:

       docker compose up -d --force-recreate

   (Not `docker compose start`: with Docker Compose 5, containers made by `create` lack the database's start-up
   script, and the database then never comes up. Checked 2026-11-12, Compose 5.1.1.)

The log then says `world already holds package ...; nothing to do.`: your world is back, with its Editor accounts and,
if the backup holds `admin`, the same Editor fingerprint. `manage/first-editor.txt` may still hold an account from the
world you replaced: it no longer works.

---

## 13. Stopping, starting, and after a restart

| To | Type |
|---|---|
| Stop the world (it saves first, and leaves the list at once) | `docker compose stop` |
| Start it again | `docker compose start` |
| Restart it (after changing `world.toml`, or a token or key file) | `docker compose restart world upnp`, or the **Restart** button on the Editor window's **Server** tab |
| Use a changed `.env` | `docker compose up -d` |
| Take the containers away, but keep the world | `docker compose down` |
| Bring them back | `docker compose up -d` |
| Delete the world for good | `docker compose down -v` (section 11) |

Stopping can take up to a minute and a half while the world saves. Let it finish.

**If the world's server stops on its own** (a crash), the world starts it again: after 5 seconds, then 30 seconds, 2
minutes, and every 10 minutes after that while it keeps stopping. The Editor stays up throughout, so you can still
reach your world's tools: its status says "Keeps stopping", with the exit code and when it starts again.

**After the computer restarts,** the world starts again by itself once Docker does (section 3), unless you stopped it
with `docker compose stop`. Then it stays stopped until you start it. On Windows, Docker Desktop starts only once
someone signs in: until then the world is down (section 3).

**If the computer loses power** or Docker is killed, the world comes back from its last save. It saves often, so little
is lost.

---

## 14. A second world on one computer

1. On the Host page, make a second world: it has its own id and token.
2. Unpack the package again, into another folder.
3. In that folder, edit `world.toml` with the second world's id, and make a `.env` with its token, a name for it, and
   a port of its own:

       COMPOSE_PROJECT_NAME=ratw-second
       RATW_PORT=7789
       RATW_EDITOR_PORT=17790
       RATW_HOST_TOKEN=rh_...

4. Forward the second port on your router too (section 8), and allow it through the firewall. (Forward 17790 only if
   you will manage the second world from outside your home.)
5. `docker compose up -d` in that folder.

Without `COMPOSE_PROJECT_NAME` and its own ports, the second folder would take over the first world's containers and
its database, or stop with "ports are not available". The first world needs no change. In the game's EDITOR box, the
second world is `127.0.0.1:17790`.

Each world takes its own memory and disk (section 1).

---

## 15. Being the host in the game

Your account is your world's host. Any of your wolves has the host's commands there, once central has accepted the
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
- Only storyteller applications wait for you for ever. Check `/pending` now and then.
- Portrait uploads are off on community worlds. The painted portraits stay.
- Where the game would say "the DM", your world says "the host".

### Your world's Editor

The game's main menu has an **EDITOR** button, before you join any world. Through it you run your world with its own
tools: the DM tool (residents, chapters, players, money, the economy, closing and opening the world). Your world keeps
its own Editor accounts; they aren't your central account.

**The first time,** on the computer that runs the world:
1. Wait until `manage/first-editor.txt` appears in the world's folder (a minute or two after the first start). It
   holds your Editor account's name, a one-time password and an **Editor code** (`ratw-editor:...`). Keep it to
   yourself: whoever has the code can take the account.
2. In the game: **EDITOR**, then paste the Editor code. (Or type the address `127.0.0.1`, then the name and the
   one-time password.) Choose your own password: at least 12 characters, not a common one, and not containing the
   account's or the world's name. The file then says "Used".
3. The one-time password lasts 7 days. If it wasn't used by then, the world writes a new one into the file the next
   time it starts.

`manage/editor-certificate.txt` holds your world's id, its Editor port and the Editor's **fingerprint** (eight groups
of four characters). The game shows the same fingerprint the first time you connect from a computer without an Editor
code: if they match, trust it. `docker compose exec world ratw-editor fingerprint` shows it too. The log does at every
start (`RATW_PACKAGE Editor certificate: ...`), but compare with the file or `ratw-editor fingerprint`: those come from
the world itself, not from a line any part of it could print.

**Lost your password, or "too many tries":** on the world's computer, in its folder:

    docker compose exec world ratw-editor list
    docker compose exec world ratw-editor reset NAME     # a new one-time password and Editor code
    docker compose exec world ratw-editor invite NAME    # an Editor code for a new computer, password unchanged

**From another computer.** The Editor port (17789) answers only on the world's own computer until you open it. Put
this line in `.env`, then `docker compose up -d`:

    RATW_EDITOR_BIND=0.0.0.0

- **At home:** in the game's EDITOR box, use the world's computer's home address (say `192.168.1.20`). On Windows, if
  the network is marked **Public**, other computers can't reach it: set it to Private.
- **From outside your home:** forward **TCP 17789** on your router to the world's computer as well, as for the game's
  port (section 8). Only do this if you will manage from outside. Test it from a phone's hotspot, not from inside your
  home.
- Use an Editor code for the first connection from a new computer (`ratw-editor invite NAME` makes one): it carries
  the world's key, so there is nothing to compare.

**On a rented server (VPS):** Docker opens published ports past the firewall (`ufw` doesn't stop them). With
`RATW_EDITOR_BIND=0.0.0.0` the Editor port faces the internet at once: that is how you manage from home. Use the
Editor code for your first connection, and set `[editor] new_devices = "approve"` in `world.toml` (section 5).

**To turn the Editor off:** `[editor] enabled = false` in `world.toml`, then `docker compose restart world`. Nothing then
answers on the Editor port.

---

## 16. What you can see, and your duties

**As the host you can see, in your world's database and logs:**
- every wolf that has been in your world: its name and look;
- what wolves say in world chat, and their private messages and letters to one another in your world (these are kept
  by each world);
- where they go and what they do there, what the residents remember of them, and the world's event log;
- the internet address each player connects from (the `RATW_CONNECT` lines). Under Docker Desktop and rootless
  Docker, every player may seem to come from the same address. That is also why a busy world there can be marked not
  reachable (section 17, "HTTP 429"): a known limit for now;
- with an AI key, what players say to residents goes to OpenAI under your account.

**You can't see:** players' account names, email addresses or passwords; or their wolves on other worlds. Each world
knows an account by an id of its own, so worlds can't link a player's wolves across worlds.

**Your duties.** Players trust you with what they do and say in your world.
- Don't publish or pass on players' messages, letters or addresses.
- Keep your world's folder, your backups and your host token private.
- Tell your players if the residents' voices use AI: their words go to OpenAI. (The list shows it too.)
- Players must be 16 or older to have an account.
- Host terms, which make the host responsible for their world's data, are still being written (Docs/Design/77,
  Phase 7). So is a way for central to tell your world when a player deletes a wolf. When they come, this guide will
  say what they ask of you.

Remember also that your home's internet address is on the list (section 8).

---

## 17. Something went wrong

First look at the log: `docker compose logs world`. Find the message below (the start of it is enough).

### The world doesn't start (`RATW_PACKAGE error: ...`)

A mistake in `world.toml` keeps the world waiting: it reads the file again after 5 seconds, 30 seconds, 2 minutes and
then every 2 minutes (the log says `next try at ...`), and starts once it is right. Any other error stops the world,
and Docker tries again every few seconds, so `docker compose ps` shows **Restarting** and the log repeats the line.
Put it right, then `docker compose restart world upnp` (or `docker compose up -d`, if what you changed is `.env`).

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
| `world.toml: [central] url is needed ...` | Central's address is missing (or its `[central]` heading is). Put in the address the game's team gave you (section 4). |
| `world.toml: central's url is https://name (no path).` | Central's address has something after the name. Put it in as the game's team gave it. |
| `world.toml: a world on central needs its id ([world] id = "world-...") ...` | The world id is missing or mistyped. Copy it from the Host page (section 2). |
| `world.toml: secure = true needs host = "a.dns.name" ...` | Set `secure = false`, unless you have a proxy with a certificate. |
| `world.toml: cost_mode is generous, balanced or frugal.` | Write one of those three. |
| `world.toml: [editor] owner is 3 to 32 small letters, digits, - and _ ...` | Change the name, or leave `owner` out. Never put a password there. |
| `world.toml: [editor] new_devices is "open" or "approve".` | Write one of those two, in quotes. |
| `RATW_EDITOR_PORT is a port number` | `RATW_EDITOR_PORT` in `.env` must be a number from 1 to 65535, with nothing else on the line. |
| `world.toml: the AI key has a space or an unusual character in it.` | Copy the key again, whole, with nothing else. |
| `world.toml names a file for the host token: there is no ...` (or `for the AI key`) | The file named by `token_file` (or `key_file`) isn't there. Check its name, and that it is in the world's folder, beside `world.toml` (a full path doesn't work). On Windows, check it isn't called `host-token.txt` (section 5). |
| `... can't be read (permission denied). Make it a file of the user that runs Docker.` | On Linux, the file belongs to another user. `sudo chown $USER FILE`. |
| `... is not UTF-8 text.` | Save the file again as UTF-8 plain text. |
| `... is empty: put the host token from the Host page in it.` | The token file is empty. Paste the token in it. |
| `...: the host token has spaces or odd characters in it: copy it again from the Host page.` | The token was pasted with something else. If it is lost, make a new one (section 2). |
| `world.toml: central's url is http:// but ...: the host token would go in clear.` | Central's address must start `https://`. Put it in as the game's team gave it. |
| `the database passwords can't be read from ...; is the db service running?` | The database didn't start. See `docker compose logs db`. |
| `the database never answered: ...` | The database didn't come up within three minutes. See `docker compose logs db`. A full disk is a common cause. |
| `this image holds no world package ...` | The world image is a developer's build without a world. Use the image that came with the package. |
| `the world package could not be imported (exit N); see above.` | The lines just above say why. If they say the database already holds a world "that no package made", this database isn't a package world. Ask for help. |

### Docker's own messages

| Message | What it means, and what to do |
|---|---|
| `pull access denied for ratw-world, repository does not exist ...` | The world image isn't on this computer, and isn't published yet. Load the image file the game's team sent: `docker load -i ratw-world.tar` (section 4). |
| `bind source path does not exist: .../world.toml` | There is no `world.toml` in the folder: it is missing, or called `world.toml.txt` (section 5). Put it back with that name, then `docker compose up -d`. |
| `failed to set up container networking: ... Bind for 0.0.0.0:7788 failed: port is already allocated` (or `Bind for :::7788 failed`) | Another program, or another world, has that port. Stop it, or give this world another port in `.env` (`RATW_PORT=7789`), and forward that one instead. |
| Windows: `Ports are not available: exposing port TCP 0.0.0.0:7788 ... bind: Only one usage of each socket address ... is normally permitted` | The same: something else on the computer has that port. |
| Windows: `Ports are not available: ... bind: An attempt was made to access a socket in a way forbidden by its access permissions` | Windows has set that port aside (Hyper-V and WSL reserve ranges of ports). To see the ranges, in PowerShell: `netsh interface ipv4 show excludedportrange protocol=tcp`. Choose a `RATW_PORT` outside all of them, and forward that one instead. |
| Any of the three above naming **17789** (`127.0.0.1:17789`), the Editor's port | Another program, or Windows, holds the Editor's port. This can happen on the update that adds the Editor, to a world that ran the day before; the whole world then stays down. Put another Editor port in `.env` (`RATW_EDITOR_PORT=17800`, or one outside the ranges above), then `docker compose up -d`. |
| `invalid containerPort: ...`, or `... is missing a target port` | `RATW_PORT` in `.env` must be a number from 1 to 65535, with nothing else on the line (no spaces, no quotes). |
| `docker: command not found`, or `'docker' is not recognized` | Docker isn't installed, or the terminal was open before it was. Open a new terminal. |
| `Cannot connect to the Docker daemon ...`, or `error during connect ...` | Docker isn't running. Start Docker Desktop (Windows), or `sudo systemctl start docker` (Linux). |
| `no configuration file provided: not found` | The terminal isn't in the world's folder. `cd` into it. |

### The world runs, but isn't listed

Read the `world agent:` lines in the log, or the reason on the Host page.

| Message | What it means, and what to do |
|---|---|
| `RATW_PACKAGE warning: no host token ...` | No token was given. Put it in `.env`, then `docker compose up -d`; or in a token file (section 5), then `docker compose restart world`. |
| `not listed on central: Central could not reach this world at ADDRESS: it did not answer within 5 seconds. Open port 7788 to the internet (TCP).` | The port isn't open. Forward it on your router and allow it through the firewall (section 8). |
| `... the connection was refused. Open port 7788 ...` | The forward reaches a computer that isn't running the world on that port. Check the forward's address and port, and that the world is up. |
| `... its /status answered HTTP 429. Open port 7788 ...` | The port is open, but the world turned central's check away: one address asked for its status more than 30 times in a minute. Under Docker Desktop and rootless Docker every player can seem to come from one address, so players' clients looking at the world list use up that allowance, and central's check is refused with them. A known limit for now. It clears when fewer people look; Linux with Docker Engine (not rootless) doesn't have it. |
| `This world's address (IP) is not on the internet.` | You are behind carrier-grade NAT, or another router (section 9). |
| `The world at ADDRESS is another world (its /status names ...), not the one this host token is for.` | Something else at that address answers, often another world of yours. Give each world its own port and its own token. |
| `THE HOST TOKEN IS FOR WORLD ..., BUT THE SERVER ON PORT 7788 IS WORLD ...` | The token and the `id` in `world.toml` are for different worlds. Copy the id that goes with this token, then `docker compose restart world`. |
| `central refused the heartbeat (bad_token): Central does not know this host token ...` or `... went unused for 90 days and has expired ...` | The token was replaced, removed or never made, or expired after 90 days unused. Make a new one (section 2), put it in place of the old one, then `docker compose up -d` (token in `.env`) or `docker compose restart world` (token in a file). |
| `central refused the heartbeat (delisted): This world is delisted.` | The game's team has taken your world off the list. |
| `central refused the heartbeat (delisted): This world was removed by its host.` | You removed this world on the Host page. It can't come back: make a new world there, and put its id and token in. |
| `central refused the heartbeat (banned) ...` | Your account is banned. |
| `central refused the heartbeat (name_refused): That name or description uses a word central does not allow.` | Change the name, description, region or tags in `world.toml`, then `docker compose restart world upnp`. |
| `... A community world cannot call itself official or use the game's name (Run Against the World, RATW).` | Likewise: change the name or text. |
| `The host name ... does not resolve (DNS) ...`, `... is not a public DNS name ...` or `... does not point at this world's address ... alone ...` | Your `host` setting is wrong. Point that name at your internet address alone, or leave `host` out. |
| `could not reach central: ...` or `central did not answer in time ...` | Your computer can't reach central: your internet connection, a firewall blocking outgoing traffic, or central is down. The agent keeps trying every minute. |
| `could not fetch central's keys: ...` | The same: central can't be reached. Until the keys come, nobody can join. |
| `the world on port 7788 does not answer (...): no heartbeat until it does` | Normal for the first seconds of every start (section 7). If it is the last `world agent:` line well after that, the world server isn't up, or stopped: read the lines above. |
| `this machine's clock is N s from central's: tickets allow two minutes; set the clock right` | Your computer's clock is wrong. Turn on automatic time. On Windows, Docker's clock can drift after the computer sleeps: restart Docker Desktop. |
| `RATW_PACKAGE warning: private worlds (a password kept on central) aren't built yet ...` | Set `private = false`. The world is listed as public. |

### Players can't get in, or other trouble

| Message, or what happens | What it means, and what to do |
|---|---|
| `RATW_SERVER warning: RATW_TICKETS no keys from central yet ...`, and it stays | The agent can't fetch central's keys. See "could not fetch central's keys" above. |
| `RATW_SERVER RATW_ARRIVAL refused: this world holds N player economy accounts, its most` | The world is full of wolves that have ever arrived. Ask for help. |
| `RATW_PACKAGE warning: the NPC Mind did not start (see logs/npc-mind.log) ...` | Something is wrong with the AI key or OpenAI. Read the Mind's log: `docker compose exec world cat logs/npc-mind.log`. The residents use their written lines meanwhile. |
| `RATW_PACKAGE the world is closed for maintenance: it starts again once it is opened ...` | The world was closed for maintenance (in the Editor, or by the DM). It checks every 5 seconds, and starts as soon as it is opened again. |
| `RATW_PACKAGE the world server stopped (exit N); starting it again in ...` | The world server stopped by itself. The lines above say why. The world starts it again, waiting longer each time it stops (section 13); the Editor stays up. If it keeps stopping, ask for help, with the log. |
| `RATW_PACKAGE the world waits until world.toml is put right ...` | `world.toml` has a mistake: the line above says what. The world reads it again on its own (`RATW_PACKAGE world.toml still has a mistake; next try at ...`). Put it right, then wait for that try, or use the **Restart** button on the Editor window's **Server** tab, or `docker compose restart world`. |
| `RATW_PACKAGE warning: this compose.yaml has no admin volume ...`, or `... has no manage folder ...` | Your `compose.yaml` is from before the Editor. Copy the new one from the package over it (keep `world.toml` and `.env`), make an empty folder called `manage` beside it, then `docker compose up -d`. |
| `UPnP: no router answered the search from inside this container: expected under Docker Desktop and rootless Docker ...` (in `docker compose logs upnp`) | Normal on Windows (Docker Desktop) and with rootless Docker: forward the port by hand (section 8). On Linux with Docker Engine, it means UPnP is off on the router: turn it on in the router's settings, or forward the port by hand. |
| `UPnP: port 7788 is already forwarded on the router to another computer ...` | Another computer has that port. Change the forward on the router, or give this world another port. |
| `UPnP: the router doesn't let programs open ports ...` | Forward the port by hand. |
| `UPnP: the router's internet address, ..., is in 100.64.0.0/10 ...` or `UPnP: the router's own internet address, ..., is private ...` | Carrier-grade NAT, or a second router in front of yours (section 9). |
| You can't join your own world from the list | Your router may not reach your own address from inside your home. Use **JOIN BY ADDRESS** with `127.0.0.1:7788` (section 7). |
| The list says "This world is older than your client: its host needs to update it" | A new game version is out, and players with it can't join your world. Update (section 11). |

To ask for help, include the log: `docker compose logs world > world-log.txt` saves it to a file. Look through it
first: it holds players' internet addresses.
