# 78. Hosts run their own worlds: the Editor (2026-11-08)

> **Parts superseded by doc 79 (2026-11-08):** its economy (Docs/Design/79-a-staged-economy.md, section 21, says
> which parts). The rest stands.

Drafted 2026-11-08 and revised the same day after four reviews; rewritten the same evening around the user's Editor
design, then revised again that night after three reviews of the rewrite (security, the code, the host's side). See
"Review" at the end. **Agreed by the user on 2026-11-08** ("Go with the recommendations"): every decision in section 17
takes its recommendation.

The user, 2026-11-08 (verbatim):

> yes so the whole point of someone being able to run their own server is so that they can edit and run their own
> world/server

And later the same day, on how hosts get in (verbatim):

> In the main menu before they connect to a server they have an "editor" button that prompts for an IP address (to a
> server) and then, if there is a connection, it prompts for a username and password, and then it brings them to
> another menu for Atlas and DM

What that means here:
- A community host gets **the DM tool** (`tools/dungeon_master.py` and its tabs in `Editor/src/dm`) and **the map
  editor** (Atlas, in the same studio app) for **their own world**.
- They reach both **from the game client's main menu**, before joining any world: **EDITOR**, then the world's address,
  then an Editor username and password, then a menu with **DM** and **ATLAS**.
- It works **from anywhere the address reaches**: the same PC, another PC at home, or across the internet to a spare
  computer or a VPS. The same PC works out of the box; anywhere else needs one line in `.env` (section 3.2). It
  doesn't need central or a listing.
- **Official worlds stay locked.** They refuse every Editor sign-in. Their progress is shared through central (doc 77,
  section 6), so nobody but the user changes them, and the user keeps the local studio as now.

This reverses doc 77's "No DM suite for hosts in v1" and its "Out: Atlas source" (doc 77, section 8). Section 15 lists
every change to docs 76 and 77.

This doc is a plan only. No code was changed, nothing was built, no server was started and no database was touched.
The code is the `r2-desktop` worktree at 029d050 (doc 77's Phase 5 and its review's fixes, committed). Other sessions
are still changing `Package/`, `Core/`, `Server/`, `Tests/` and `tools/world_package.py`, so line numbers there drift;
the ones here were right when the reviews ran.

Words used the same way throughout (doc 77's words still hold: central, a world, a wolf, a ticket, a host):
- **the Editor:** the client's way into a world's tools: the EDITOR button, its screens, and the Editor window.
- **the Editor window:** the client window that shows the studio app (DM or Atlas) for one world.
- **an Editor account:** a username and password that belongs to **one world**, kept in that world's database. It is
  not a central account and never reaches central.
- **the owner:** the Editor account the world makes at first start. It can add other Editor accounts (co-builders).
- **the Editor certificate:** the TLS certificate the world makes for itself at first start. The client pins its key,
  from an Editor code or on first use, as SSH does.
- **an Editor code:** one string to paste that carries a world's key, a username and a one-time sign-in. The world
  writes one for the owner, and makes one for each co-builder. With it there is nothing to compare by eye.
- **the Editor port:** the world's second port, 17789 by default, separate from the game port (7788). Open only to the
  world's own computer until the host opens it (section 3.2).
- **the studio:** `tools/studio.py`, which serves the DM suite and Atlas from one app (`Editor/`).
- **the helper:** the new process inside the world's container that holds the host's tools (`tools/world_admin.py`).
- **Publish:** make a new build from the host's map edits, then restart the world onto it.
- **a slice:** a set of phases that ships something whole. Slice 1 is "run my world", slice 2 is "edit my map".

Standing rules, kept:
- Characters are male or female only.
- Never the word for the domestic animal in anything players read (wolves; pups for young).
- Nothing writes PROD.
- No data migrations: migrations change schema only (section 12.5 makes this a firm rule for community worlds).
- Ask before simulations and paid model runs.

## Headline

- **Hosts get the whole toolset for their own world,** in two slices:
  - **Slice 1, "Run my world":** the Editor's way in (button, address, certificate, sign-in, menu), the DM suite
    (LIVE, NPC life, Chapters, Players, Money, Economy, Health) with live changes only, a My world tab (open and
    close, restart, backups), an Editor accounts screen in the client's own menu, and restore and password-reset
    scripts in the world folder.
  - **Slice 2, "Edit my map":** Atlas (terrain, cells, interiors, doors, places, NPC layers, talk) behind the menu's
    ATLAS button, with Publish, the map source in every package, preview before opening, and a way back from a bad
    map.
- **Most of it works already.** The DM tool writes rows into the world's own `dm.actions` table, and the running server
  applies them within about a second. Atlas edits source tables, `world_build` makes a build and sends
  `ratw_release`, the server exits 75, and the package's entrypoint restarts it. What is missing is the way in,
  one-world mode, safe recovery, and the map source in the package.
- **The way in is an address, an Editor account and a pinned key.**
  - A new helper in the world's container listens on the **Editor port**, 17789 by default, separate from the game
    port. It speaks **TLS 1.3 only**: the world makes its own self-signed certificate at first start, and keeps it in
    a Docker volume of its own so updates don't change it.
  - **The port is open only to the world's own computer until the host opens it** with one line in `.env` (Q1). A
    fresh world is never reachable from the network while its first one-time password is waiting.
  - The client's shell (Rust, not a page) connects and **pins the world's key**. An **Editor code** (one string the
    world writes for the owner and makes for each co-builder) pins it ahead of time, so nobody compares anything by
    eye. A bare address is trusted on first use, as SSH does, showing a fingerprint the host can compare with the one
    in the world folder. **A changed key is refused loudly**; an honest new key is signed by the old one, so clients
    move over without a red screen. No password ever crosses plain `ws://` or `http://`.
  - **Editor accounts are the world's own**, reusing the DM tool's accounts and sessions (`dm.admins`, `dm.sessions`)
    in the world's database. The world makes the first one, the **owner**, at first start, under a name it generates,
    with a one-time password and an Editor code written into the world folder. The owner adds co-builders, each with
    a role (DM only, DM and Atlas, or watch only). **A new owner is made only on the world's computer.**
  - Sign-in has **rate limits that can't lock the owner out**: known devices, Editor codes and accounts still on an
    unguessable one-time password each have their own lane. A dummy hash stops username probing, password checks and
    connections are capped per address and world-wide, and a breached-password list refuses weak passwords. Sessions
    expire after 30 minutes without input or 12 hours.
  - It works when central is down, before the world is listed, on a delisted world, on a LAN world, and from another
    computer: the cases the earlier draft needed a key file and an SSH tunnel for.
- **The password and the session never reach a page the world serves.** The host types the password in the client's
  own main window. The shell signs in, keeps the session token in memory, and **proxies** the Editor window's requests
  to the world through a custom URI scheme, adding the token itself. The Editor window has no shell permissions, is
  bound to one world, one account and one session, and is destroyed with its storage when any of them changes.
  Adding accounts and ending sessions happen in the client's own screen, with the password typed again.
- **The page comes from the world's image, but only code the user signed runs** (Q6). The package's Editor bundle is
  signed with a release key; the shell checks every file the world serves against the signed list and refuses
  anything else. So client and world versions can't drift apart, and a hostile world can't run its own script in the
  client.
- **Official worlds get none of it.** The package can't run an official world at all: it never passes `--official` and
  holds no shared-progress credential. The helper also refuses every sign-in if central ever named the world official.
- **The listing stays honest.** Cheats come only from `world.toml`'s `cheats`, enforced in the server at the point of
  use, and a world with any cheats is listed "cheats on". `character.dm` is never available to hosts.
- **Players are told the truth.** The host of a community world can read everything said there and sees players'
  addresses. The filters in the tools stop casual harvesting and limit a stolen session; they don't stop the host.
- **A bad change can always be undone without the command line,** except a full restore, which is a script in the
  world folder. If a new map won't start, the world goes back to the previous map by itself.
- **Nothing a host does reaches central or another world.** Community progress stays in its world (doc 77, section 6).
- **Size:** slice 1 about 35-45 plan hours (about 15-20 expected); slice 2 about 19-26 plan hours (about 8-11
  expected). Both, with two review rounds: **about 54-71 plan hours, about 24-30 expected.** That is more than the
  rewrite's first count (45-61, 19-24): an internet-facing sign-in needs the Editor code, signed bundles, per-session
  windows, separate lanes and pools for the limits, a volume of its own for the key, and recovery scripts. The
  expected column now uses about 0.45 times the plan for the three phases that are new kinds of work (the helper's
  security core, the hosted studio, the client) and about 0.3 for the rest, as doc 77 ran. The stop-and-ask rule uses
  the expected column.

## 1. What it is, and why

Doc 77 shipped a world package so players can run worlds. It gave hosts only in-game moderation (`/pending`,
`/treaty`, `/house`, `/storyteller`, `/kick`, `/ban`, `/silence`) and kept the studio for the user's own worlds.

The user now says the point of hosting is to make the world your own. That means two things:
1. **Running it:** the DM's levers (moving NPCs, calling festivals, steering the economy, deciding storytellers,
   treaties and Houses, watching health).
2. **Editing it:** Atlas, for the map and its people.

Both already exist for the user. The work is to make them safe and simple for a stranger, who:
- runs Windows with Docker Desktop, unzipped the package, edited `world.toml` and ran `docker compose up -d`;
- usually plays on the same PC, but may run the world on a spare computer in a cupboard or a rented VPS, and may want
  a friend to help build;
- doesn't read container logs and can't find files inside a Docker volume.

So everything starts from one button on the client's main menu, needs no new setup beyond opening one file the world
writes, says plainly when each change takes effect, makes a backup before every risky step, and has a way back from
every mistake.

## 2. The Editor, screen by screen

All of these screens are the client's own bundled pages in the main window (`Client/src/ui/frontDoor.ts`), except the
tools themselves, which open in the Editor window. EDITOR shows only in the desktop client (`shell.native`,
`Client/src/shell.ts:79`): a browser page can't pin a key. **On macOS EDITOR stays hidden until it has been tried
there** (Q26): WKWebView's custom-scheme handler has its own quirks with request bodies and downloads, and no Mac has
been tried.

Every piece of text a world sends that these screens show (its name, its progress, its errors) is checked by the shell
first: the name is cut to 40 characters and loses control and bidirectional-text characters, progress is a code and
a percent the client words itself, and unknown fields are dropped (section 6.6).

**Screen 1. The main menu, before connecting.**
- The sign-in page (`centralLoginPage()`, `frontDoor.ts:432-499`, and the password-mode `loginPage()`, `:389-426`)
  gains an **EDITOR** button under the pitch card: "Run or build your own world." It needs no central sign-in, so it
  works when central is down (the page still shows then, with its error, `client.ts:83-84`).
- The roster page (`rosterPage`, `:503-575`), the first menu after signing in, shows EDITOR too, beside HOST A WORLD.
- Each COMMUNITY row on the Host page (`hostPage`, `:731-800`) gets an **EDITOR** button. It first tries
  `127.0.0.1:17789` and uses it if that world's id matches the row (a host at home often can't reach their own public
  address); otherwise it fills screen 2 with the listed host name and the Editor port. OFFICIAL rows never do.

**Screen 2. "Edit a world": the address, or an Editor code.**
- **"Have an Editor code? Paste it."** One box for the string from `manage/first-editor.txt` or from the owner. It
  fills the address (when the code carries one) and the username, and pins the world's key straight from the code, so
  screen 3 has nothing to compare. Codes start `ratw-editor:` so the box can tell them from addresses.
- Or one box: "Your world's address", placeholder `e.g. 127.0.0.1, 192.168.1.20 or myworld.example.net:17789`, at most
  260 characters (as `worldsPage()`'s `world-direct` form, `frontDoor.ts:676-687`). The port defaults to **17789**,
  not the game's 7788. `localhost` is tried as 127.0.0.1, since it can resolve to `::1`, which a loopback-only Editor
  port doesn't answer.
- Under it: "On the computer that runs your world, use 127.0.0.1." And: "Don't have a world yet? HOST A WORLD."
- **Recent worlds:** the worlds this client has signed in to, each with its name, address and last username (never a
  password), and FORGET.
- **CONNECT.**
- If the client has central's listing and the address is an OFFICIAL world's, it says "Official worlds can't be
  edited" before connecting at all.

**Screen 3. Connecting, and the key check.** The shell connects over TLS and asks the world for `/editor/hello`.
- **Still setting up.** On a first start the import and migrations take minutes. The helper answers from the start
  with a "setting up" code and a percent, and the screen says "Your world is still setting up (40%). This page waits
  for it." Sign-in is refused until it is done.
- **Can't connect.** The message depends on the address, in this order:
  - a loopback address: "Is Docker Desktop running, and is the world started (`docker compose up -d` in its
    folder)?";
  - a home address: the same, then "Your world's Editor answers only its own computer until you allow others: add
    `RATW_EDITOR_BIND=0.0.0.0` to `.env` in the world folder, then run `docker compose up -d`." (Section 3.2.);
  - any address: "Is the Editor port right? It is 17789 unless your `.env` sets `RATW_EDITOR_PORT`. The Editor may
    also be turned off (`[editor] enabled` in `world.toml`).";
  - a public address: "From outside your home, your router must forward TCP 17789 to the computer that runs the world.
    The hosting guide shows how. From inside your own home, use the home address instead: some routers can't reach
    their own public address." (Section 3.3.)
- **The wrong port.** If the port answers plain HTTP with a world's `/status`: "That's your world's game port. The
  Editor uses port 17789." That fallback check stores nothing.
- **Not an Editor.** "This address isn't a RATW world's Editor." An older package with no helper says "Your world's
  package is older than the Editor: update it (keep `world.toml`, `.env` and `backups`)."
- **Official.** "Official worlds can't be edited." Nothing more happens.
- **A key this client already trusts** (the same world, at this address or a new one): no screen. A new address says
  "*Ashfall*, now at 192.168.1.21" and reuses the world's pin and device token.
- **A new key, signed by the old one** (the host ran `ratw-editor new-certificate`): no red screen. A notice says
  "*Ashfall* has a new certificate, signed by its old one" and the pin moves over.
- **First connection, from an Editor code:** no screen; the key was pinned from the code.
- **First connection, from a bare address:** "First connection to *Ashfall* at 203.0.113.7:17789. Its certificate
  fingerprint is `3F2A 9C10 5B7E 0D41 A6C3 22F9 8E05 77D1`. Compare it with the one in your world folder,
  `manage/editor-certificate.txt`, or in the world's log. If they match, trust it." All eight groups, never cut short,
  in the same layout as the file. Buttons: **TRUST AND CONTINUE** (labelled **I COMPARED IT** on a public address) and
  CANCEL. On 127.0.0.1 the screen adds "This is the computer you're using" and the comparison is optional. Below:
  "Using an Editor code skips this."
- **A different key at an address this client trusts,** whatever world id or name the reply claims, and **a known
  world id under a different key:** a full red screen, and **no way to sign in from it**: "This world's certificate
  has changed since you last connected. Someone may be intercepting your connection. Don't sign in." It shows both
  fingerprints. Only if the host knows why (a fresh world, `ratw-editor new-certificate --leaked`, a lost volume):
  "Compare the new fingerprint with `manage/editor-certificate.txt`", then **FORGET THE OLD CERTIFICATE**, which needs
  the world's name typed. Then screen 3 starts again as a first connection. A new Editor code from the owner also
  clears it.

**Screen 4. Sign in.**
- The world's name and address at the top, drawn by the client, so the host knows where the password is going.
- Username and password (both filled from an Editor code). "This is your world's Editor account, not your RATW
  account. Use a password only for this world. RATW never asks for your RATW password here or in the Editor window."
- While the owner hasn't yet chosen a password: "First time? Your username and one-time password are in
  `manage/first-editor.txt` in your world folder. Pasting the Editor code from that file is easiest."
- **SIGN IN.** Errors: "Wrong name or password." (the same for an unknown name); "Too many tries from here: wait N
  minutes."; "This account is turned off: ask the world's owner."; "That one-time password has expired: ask the
  world's owner for a new code, or on the world's computer run `reset-editor-password` in the world folder."; "Lost
  your password? On the world's computer, run `reset-editor-password` in the world folder."
- The password box is cleared on every page change (`show()` already does `this.password = null`, `frontDoor.ts:368`).

**Screen 4b. Choose your password** (only on an account's first sign-in, or after a reset). New password twice, at
least 12 characters (the DM tool's rule, `dungeon_master.py:228, 235`), not containing the username or the world's
name, and not on the bundled list of breached passwords (section 6.3). The one-time password stops working at once,
and the world empties `first-editor.txt` within seconds.

**Screen 5. The Editor menu: DM and ATLAS.**
- At the top: the world's name, address and a status pill (**Open · 7 wolves**, **Open only to you**, **Closed for
  changes**, **Restarting…**, **Publishing map 63%**, **Keeps stopping**), "Last backup 3 h ago", "Signed in as
  *host-k7q2* (owner)", "Certificate checked". For the owner: "12 failed sign-ins today" when there were any, and a
  banner for every account change since their last visit ("Account *pell* added by *host-k7q2* from 192.168.1.20").
- Two big buttons:
  - **DM:** the Dungeon Master tool, with a **My world** tab.
  - **ATLAS:** the map editor. In slice 1 it is shown greyed: "Map editing comes in a later update." A `dm` account
    sees it greyed with "Your account is for the DM tool only."
- **Editor accounts** (owner only), on this screen and not in the Editor window, so a page can never drive it. The
  list of accounts, each with its role, "not yet claimed" or its last sign-in (when, and from which address), and its
  devices. **Add a co-builder:** a name and a role (default `dm`), each role in one line ("Can run events and NPCs;
  can't touch the map or accounts"). Before the code is shown: "They need the RATW desktop client, and your Editor
  port open to them (section 3.3)." Then the Editor code, once, to pass on privately. Also: reset (a new code), change
  role, turn off, remove, "may act on players' wolves" (Q24), and **End all Editor sessions**. Every one of these asks
  for the owner's password again, in this screen.
- Small links: Change my password, Sign out, Back to the main menu.

**Screen 6. The tools.**
- DM or ATLAS opens the **Editor window** (one window, labelled `editor`) on that tool (`editor://localhost/?suite=dm`
  or `?suite=atlas`, as `Studio.tsx:45, 57-58` picks its suite); choosing the other from the menu switches it. Its
  title, set by the shell, is "RATW Editor: *Ashfall* (203.0.113.7)": the address from the shell, the name cut short.
- **The window belongs to one world, one account and one session.** Signing out, signing in to another world, or
  signing in as another username closes it and wipes its storage. A new window opens for the new session. The shell
  refuses any request from a window whose session isn't the current one.
- The window keeps working if the host goes back to the main menu and plays.
- When the container restarts the window shows "Restarting…" and reconnects by itself, keeping its state. Sessions are
  database rows, so they survive a helper restart.
- **When the session ends** (30 minutes without the host's input, the 12-hour limit, the owner's "End all Editor
  sessions", a password change elsewhere, the account turned off): the window keeps its queued saves and says "Your
  Editor session ended: sign in again from the Editor menu." The menu shows screen 4 with the username filled in.
  Signing in again **as the same account on the same world** hands the window a new session in place, and the queued
  saves retry. Anything else drops them, with a notice saying how many. Closing the window with saves pending asks
  first.
- Quitting the client signs out (the token is only in memory).

**The tabs in the Editor window.** Slice 1 shows today's DM tabs (`Editor/src/dm/DmApp.tsx:27-39`) plus My world;
slice 2 adds Atlas behind ATLAS. The regrouping into the host's words is slice 3.

| Tab | What | Takes effect | Roles |
|---|---|---|---|
| **My world** (new) | Status, Close and Open, Restart, backups and restore steps, a few settings (read-only: `player_limit`, `cheats`, `workers`, `[editor] enabled`), AI use, version, history, the certificate's fingerprint; owner only: the world folder's path, "keeps stopping" log lines (addresses and tokens removed), failed sign-ins; in slice 2, Publish, "can this computer publish", previous map, discard, share | — | owner (others see status, settings and history read-only, and only their own sign-ins) |
| **Live** | The DM's LIVE tab | Now | dm and up |
| **Residents** | NPC Life plus, in slice 2, Atlas's NPC layers (NPCs, routes, areas, spawns, factions, talk) | Now | dm; layers builder and up |
| **Map** (slice 2) | Atlas: terrain, cells, interiors, doors, places | After Publish | builder and up |
| **Economy** | Edicts, the steering slider, rules, store, seasons, farms | Now | dm and up |
| **Money & calendar** | Festivals, Wharf credit | Now | dm and up |
| **Players & decisions** | Chapters, storytellers, reports, the filtered players list | Now | dm and up |
| **Health** | Server Health | — | everyone |

## 3. Setting it up: the host's side

### 3.1 The first time, on the same PC

1. Start the world as today (`docker compose up -d`).
2. Open the world folder and wait until `manage/first-editor.txt` appears (a minute or two on a first start; the
   Editor's screen 3 says "still setting up" meanwhile). The world has written two files into `manage/`:
   - `first-editor.txt`: "Your Editor account: `host-k7q2`. One-time password: `…` (20 random characters). Easiest:
     open the game, press EDITOR, and paste this Editor code: `ratw-editor:…`. Or use the address 127.0.0.1 with the
     name and password above. You'll choose your own password next. If you've already chosen your own password, this
     one no longer works."
   - `editor-certificate.txt`: the world's id, the Editor port and the certificate's fingerprint, in the same eight
     groups the client shows.
3. In the game: EDITOR, paste the code (or `127.0.0.1`, TRUST AND CONTINUE, the name and one-time password), choose a
   new password, then the menu.

**The owner's name** is `[editor] owner = "name"` in `world.toml` if the host sets one (the DM tool's username rule,
`^[a-z][a-z0-9_-]{2,31}$`, `0013_dungeon_master.sql:10-17`). Otherwise the world makes one, `host-` and four random
letters and digits, so a stranger can't aim at a name they know. The host never needs to remember it: the file, the
code and Recent worlds carry it. **A password never goes in `world.toml`.**

The world makes the owner only when it has no Editor accounts at all, so a restart or an update never makes another.
The owner's one-time password lasts 7 days; a start after that, with the owner still unclaimed, writes a fresh one into
the file, so a host who waits a fortnight isn't stuck. As soon as the owner chooses a password, the helper asks the
root entrypoint to empty the file to "Used. Lost your password? Double-click `reset-editor-password` in this folder."

An old `compose.yaml` has no writable `./manage` mount (section 6.3), so no file appears. The log then says to take the
new `compose.yaml`, or to run `reset-editor-password` (which prints the code) instead. An existing doc 77 world that
updates to the Editor package gets its owner on that first start, because it has no Editor accounts yet.

### 3.2 The Editor port

- **17789 by default,** published by compose as `"${RATW_EDITOR_BIND:-127.0.0.1}:${RATW_EDITOR_PORT:-17789}:17789"`.
  It is clear of the README's second-world game port (7789, `README.md:73`). The second-world recipe sets both:
  `RATW_PORT=7789` and `RATW_EDITOR_PORT=17790`.
- **Open only to the world's own computer by default** (Q1). That covers the commonest case, a host who plays on the
  PC that runs the world, with no setup at all. It also means a fresh world is never reachable from the network while
  its one-time password waits, a laptop host on café Wi-Fi exposes nothing, and an IPv6 home network (where a router
  may let incoming traffic straight through) exposes nothing either.
- **To manage from another computer** (at home, or from outside): `RATW_EDITOR_BIND=0.0.0.0` in `.env`, then
  `docker compose up -d`. Screen 3's "can't connect" text and the guide both give that line. M0a checks whether a
  `0.0.0.0` publish also answers on IPv6 under Docker Engine and Docker Desktop, and the guide says which; if it
  doesn't, `RATW_EDITOR_BIND=::` covers both where Docker supports it.
- **To turn it off:** `[editor] enabled = false` in `world.toml`, then Restart. The helper then **doesn't listen at
  all**, so the client can only say "can't connect", and its message mentions that the Editor may be turned off.
  Compose still publishes the port, so a port collision (below) remains; the guide says so.
- **On a VPS** the guide says plainly, and the README's first-start section repeats: "Docker publishes ports past
  `ufw`. With `RATW_EDITOR_BIND=0.0.0.0` the Editor port faces the internet at once. That is how you manage from home.
  Use the Editor code for your first connection, and consider `[editor] new_devices = "approve"` (Q23)."
- **When the port is taken.** If 17789 falls in a Hyper-V or WSL reserved range, or another program holds it, `docker
  compose up -d` fails for the whole world, game port included, with "ports are not available … forbidden by its
  access permissions". This can happen on the update that adds the Editor, to a world that ran fine the day before.
  The README and the guide quote that error and the fix: `RATW_EDITOR_PORT=17800` (or another) in `.env`, then
  `docker compose up -d`.
- **Under Docker Desktop**, the container may see every peer as the gateway (M0a checks). Then the per-address limits
  can't tell one stranger from another, so one stranger can keep the port busy and keep **new devices without an
  Editor code** waiting. Known devices and Editor codes still get through (section 6.5). The fix, if it happens, is
  not to open the port beyond the world's computer, or to use Editor codes.

### 3.3 Managing from another computer

First `RATW_EDITOR_BIND=0.0.0.0` (section 3.2). Then:
- **At home:** use the world's computer's home address (for example `192.168.1.20`). Windows' firewall rules belong to
  programs, not ports: if friends can already join the game from another computer, Docker Desktop is allowed and
  nothing more is needed. If Windows marked the network as **Public**, other computers can't reach either port: set it
  to Private. The guide shows where to find the address and the setting.
- **From outside:** forward **TCP 17789** on the router to the world's computer, the same steps as the game port's.
  Then use the home's public address, or a dynamic DNS name. The guide says: "Only forward 17789 if you'll manage from
  outside your home. Test it from a phone's hotspot, not from inside your home: some routers can't reach their own
  public address." Nothing checks the Editor port from outside, as central checks the game port; that is deliberate
  (central shouldn't learn where the Editor listens).
- **The first connection from outside is the one that matters** for the key. An Editor code carries the key, so there
  is nothing to compare; with a bare address, compare the fingerprint with `manage/editor-certificate.txt` (screen 3).
  A host who connected once at home already has the pin, and the same key at a new address is recognised.

### 3.4 The fingerprint

The certificate's fingerprint is the SHA-256 of its public key. The client and the files show its first 128 bits as 8
groups of 4 hex characters, always all eight, in the same layout everywhere: no key-making attack can match 128 bits,
and 8 groups is short enough that people really compare it. The client pins the full SHA-256 and keeps the public key
itself, for signed rotation (section 6.4). The host finds it:
- in `manage/editor-certificate.txt` in the world folder;
- in the world's log at every start ("Editor certificate: 3F2A 9C10 5B7E 0D41 A6C3 22F9 8E05 77D1");
- with `docker compose exec world ratw-editor fingerprint`;
- in My world and on screen 5, once signed in.

Most hosts never compare it: the Editor code pins it for them.

### 3.5 Co-builders

- **The owner adds them** on screen 5's Editor accounts (section 2): a name and a role, with the owner's password typed
  again. The client shows an **Editor code** once, to pass on privately. The co-builder pastes it into EDITOR and
  chooses their own password (screen 4b). The code carries the world's key, so the co-builder compares nothing.
- **The code is a secret** (it holds a one-time password) and **expires after 72 hours** if unused. The owner can make
  a new one. The accounts list shows "not yet claimed" until then, and afterwards from which address it was claimed.
- **Roles** (Q2):

| Role | DM tool | Atlas layers (live) | Atlas map and Publish | My world's actions, backups | Editor accounts |
|---|---|---|---|---|---|
| `viewer` | Watch only | No | No | Read only | No |
| `dm` | Yes | No | No | Read only | No |
| `builder` | Yes | Yes | Yes | Read only | No |
| `owner` | Yes | Yes | Yes | Yes | Yes |

- **Acting on players' wolves** (kill, injury, move, resurrect) is for the owner, and for accounts the owner marks "may
  act on players' wolves" (Q24). Everything else in the DM tool follows the role.
- **A new owner, or promotion to owner, happens only with `ratw-editor` on the world's computer.** The screen can add
  `viewer`, `dm` and `builder` accounts. So a stolen owner session can't make itself a lasting owner account.
- The owner can turn an account off, reset it (a new Editor code), change its role (up to builder), remove it, see its
  devices and last sign-in, and **End all Editor sessions**. A reset, a password change and ending all sessions also
  forget that account's devices. **The last owner can't be removed or turned off.**
- Every account change writes a log line and an audit row, and the owner sees a banner for it on screen 5.
- The guide tells co-builders: "The world's host keeps your account in their database and sees your address. Use a
  password only for this world. Only accept Editor codes from people you trust: signing in to a stranger's world shows
  you their tools, never yours."

### 3.6 Lost password, or locked out

**The double-click way:** `reset-editor-password.bat` (Windows) and `reset-editor-password.sh` ship in the world folder.
Each lists the accounts, asks which one, resets it, and shows the new Editor code. It also writes the code into
`manage/first-editor.txt`, so the host isn't relying on a console window, and ends with `pause` so a double-clicked
window doesn't close before the host reads it.

**The commands,** on the world's computer (anyone who can run `docker` there already owns the world):

    docker compose exec world ratw-editor list
    docker compose exec world ratw-editor reset NAME        # a new Editor code; unlocks; ends its sessions and devices
    docker compose exec world ratw-editor invite NAME       # an Editor code for a new device, password unchanged
    docker compose exec world ratw-editor add NAME --role builder
    docker compose exec world ratw-editor add NAME --role owner    # the only way to make an owner
    docker compose exec world ratw-editor disable NAME
    docker compose exec world ratw-editor sign-out-all
    docker compose exec world ratw-editor fingerprint
    docker compose exec world ratw-editor new-certificate   # signed by the old key: clients move over quietly
    docker compose exec world ratw-editor new-certificate --leaked   # every client shows the red screen

`docker compose exec` runs as root (the image sets no `USER`). The command reads the dm password from
`/run/ratw-secrets/passwords.env` as root, then drops to `ratw-admin`, and wraps the DM tool's `create_account` and
`set_password` (`dungeon_master.py:223-240`). No limit on the Editor port ever blocks it. It prints Editor codes and
one-time passwords to the terminal (and, through the reset script, to `first-editor.txt`), never to the log.
`invite` is the way back in for an owner on a new computer while unknown devices are held off (section 6.5).

### 3.7 Official worlds

Nothing here exists on an official world (section 6.7): no helper, no Editor port reply beyond "official", no Editor
accounts. The user keeps his local studio.

## 4. What the plan rests on (today's code)

**The DM tool** (`tools/dungeon_master.py`, 2,377 lines; served by `tools/studio.py`, 1,718 lines)
- Every write goes into the world's own database. Most become a `dm.actions` row with `pg_notify('ratw_dm')`, in one
  transaction with an audit row (`dungeon_master.py:452-599`).
- `Game::applyDmActions` (`Core/RatwGame.cpp:898-1500`) applies up to 50 rows a pass, at most once a second after a
  notify, and also once a minute without one. It writes back `applied` or `refused`. It handles 52 kinds by name;
  the DM tool's `ACTIONS` lists 51 (`dungeon_master.py:150-187`). Queued actions expire after 10 minutes. Scratch
  servers skip it.
- Layer edits write the `live.*` row first, then queue a `*.sync` (`npc.sync`, `layers.sync`, `factions.sync`,
  `economy.sync`, `voice.sync`, `visitors.sync`). The server adopts that part of the world from them.
- NPC Life already goes through actions: `npc_life` queues `npc.kill` or `npc.revive` in the same transaction as its
  `live.npc_state` write (`dungeon_master.py:1374-1391`). The row is what survives a restart: the server reads
  `npc_state` rows newer than its checkpoint at start (`externalNpcStates`, `Core/RatwDbStore.cpp:385-392`).
- Economy writes and `economy.sync` live in `studio.py` (the `Economy` class from `:323`, `queue_sync` at `:333-341`,
  routes at `:1393-1480`), not in the DM tool. On a dev world they go through `live_edit` as the editor role
  (`studio.py:350-353, 739-754`); on a live world, a direct upsert as dm.
- The `ratw_dm` role reads `world.*` and `game.*` (except `game.accounts` and three revoked tables) and writes `live.*`
  and `dm.*` (migrations 0013, 0014, 0038). It may also insert into `world.edits` and bump `world.worlds`' revision
  and `updated_at` (`0014_people_from_live.sql:71-72`).

**The DM tool's accounts and sign-in** (what the Editor reuses)
- `dm.admins` (`0013_dungeon_master.sql:10-17`): `username` (the CHECK above), `role` (CHECK `IN
  ('viewer','dm','admin')`), `password jsonb`, `disabled`, `created_at`, `last_login`. `dm.sessions` (`:20-25`):
  `token_hash` primary key, `username` (cascading), `created_at`, `expires_at`. **No `last_seen`.**
- `ratw_dm` has full read and write on schema `dm` (`:49, :54`). `REVOKE ALL ON dm.admins, dm.sessions FROM
  ratw_game` (`:59`): a compromised game server can't read password hashes or sessions.
- **But every other `dm.*` table is the game server's to read and write.** `0012_game_tables.sql:138` sets `ALTER
  DEFAULT PRIVILEGES ... IN SCHEMA dm GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO ratw_game`. So `dm.audit`
  (`0012:142`) can be forged or deleted by `ratw_game` today, though nothing in `Core/` or `Server/` touches it, and any
  new `dm.*` table would be open to it too unless its migration revokes that.
- Passwords: `world_db.hash_password` and `password_matches` (`tools/world_db.py:593-609`): scrypt with
  `n = 2**15, r = 8, p = 1` (`:79`), a 16-byte salt, a 32-byte key, about 32 MB of memory per check, compared with
  `hmac.compare_digest`. At least 12 characters.
- Accounts: `create_account` and `set_password` (`dungeon_master.py:223-240`; `set_password` deletes the user's
  sessions); `create_defaults` and `write_accounts_file` (`:243-252, 2324-2332`, written once, mode 0600). The command
  line has `accounts create-defaults|list|set-password USER` (`:2341-2365`). **Nothing adds one named account,
  disables one or deletes one;** the `disabled` column exists but nothing sets it.
- Ranks: `RANK = {'viewer': 0, 'dm': 1, 'admin': 2}` (`:202`); `character.dm` needs admin (`:187`); Atlas needs dm
  (`studio.py:1269-1273`). So today **dm means DM and Atlas**: "DM only" can't be said.
- Accounts live in PROD: `ACCOUNTS = 'prod'` (`:62`), used by login, the session check and logout (`:289, 310, 318`) and by the command line (`:2343`).
  A container has no `prod`.
- Login (`:288-319`): 5 failures in 15 minutes (`MAX_FAILURES, LOCK_MINUTES`, `:59`), counted from `login.failed`
  audit rows **per username**, returns 429. Its gaps on an open port:
  - **anyone can lock the owner out** by spraying wrong passwords at the name;
  - **no per-address or world-wide limit**, and `ThreadingHTTPServer` makes a thread per connection, so a flood of
    wrong passwords costs 32 MB and some tens of milliseconds of scrypt each (an estimate, not measured);
  - **unknown and disabled usernames skip scrypt** (`if not row or row[2] or not password_matches(...)`, `:296`), so
    timing tells which names exist;
  - every failure is an audit row, without limit.
- Sessions: `secrets.token_urlsafe(32)`, only its SHA-256 stored (`:217-218`); 12 hours absolute (`SESSION_HOURS`,
  `:58`), **no idle timeout**; expired rows swept at each login (`:303`); `session()` refuses disabled accounts
  (`:311-312`). `requested_by` and the audit take `who['username']` (`:591-597`).
- The page keeps the token in sessionStorage `ratw-dm-session` (`Editor/src/dm/api.ts:88-89, 131-134`) and adds it as
  a bearer header (`authHeaders()`, `:93`); its sign-in form is `SignIn` (`Editor/src/dm/DmApp.tsx:43-62`); `Studio`
  boots with `/api/me` and `/api/studio/session` (`Editor/src/studio/Studio.tsx:31-38, 52-54`).

**What else ties the studio to this laptop**
- The studio needs a dev world to start (`world_db.resolve(..., 'dev')`), and the package registers its one world as
  `live`. The studio would refuse to start there. `Studio.session()` (`studio.py:1227-1241`) assumes a dev-world
  registry.
- Its only network guard is a Host/Origin check (`trusted()`, `dungeon_master.py:2068-2073`,
  `map_editor.py:1487-1493`), which any non-browser client can forge. It binds `0.0.0.0` by default
  (`studio.py:1674-1681`), over plain HTTP.
- **Every studio request passes `trusted()` first** (`studio.py:1277-1278`), and Atlas's `editing()` checks it again
  (`map_editor.py:1566`). It needs `Host` to be `127.0.0.1` or `localhost` on the studio's port (or a name in
  `RATW_SERVE_HOSTS`) and a matching `Origin`. Through the helper `Host` is the world's address, so every request
  would get 403 unless hosted mode replaces that check.
- Its body readers differ: Atlas's refuses chunked bodies and needs `Content-Length` (`map_editor.py:1524-1528`); the
  DM suite's `MAX_BODY` is 64 KB (`dungeon_master.py:60`), so a 16 MB limit matters only for Atlas.
- Its page server serves only real bundle files, with no fallback to `index.html` (`map_editor.py:1515-1522`); the app
  picks its suite with `?suite=dm|atlas` (`Studio.tsx:45, 57-58`).
- Replies are gzipped only when the request's `Accept-Encoding` asks (`tools/http_body.py`).
- **Atlas's write token:** `map_editor` alone hands it to any `trusted()` GET of `/api/session`
  (`map_editor.py:1456, 1541-1543`). Under the studio, every `/api/atlas/...` route first calls `builder()`
  (`studio.py:1312`), so there it needs a dm sign-in. Either way the page fetches it (`sessionInfo()`,
  `Editor/src/lib/api.ts:19-24`) and sends it as `X-RATW-Editor` on every write (`:27-29`) and on the economy calls
  (`:126-154`); the server requires it (`map_editor.py:1566-1572`). If `/api/atlas/session` is refused, `sessionInfo()`
  throws and every Atlas post and `atlasWorld()` fail.
- It starts servers with `tools/live.sh` (`studio.py:1147-1153`), runs playtests with `play.sh`
  (`map_editor.py:1394-1430`), writes `Saved/Config/worlds.local.json`, and reads `Saved/`, `artifacts/` and
  `Database/dm-accounts.txt`.
- The page starts by calling `/api/studio/session`, `/api/studio/atlas-world` and `/api/studio/dm-world`
  (`Editor/src/dm/api.ts:351-355`).
- Close and Open go through `admin.server_state`, which **only `ratw_publisher` may update**; editor, game and dm may
  only read it (`0054_sync_base.sql:62-63`). The studio's `/api/dm/server/status` and `/maintenance` routes go through
  `studio.sync`, which connects as publisher (`studio.py:1489, 1529`, `world_sync.py:1467`). `/rebuild` doesn't: it
  calls `studio.builds.rebuild` (`studio.py:1533-1539`), which connects as **editor** (`:1095`). The state is
  `running` or `maintenance` (a CHECK, `0054:34`).

**Atlas and builds**
- Atlas edits `world.*` source tables through keyed, logged batches (`tools/live_edit.py`, the `editor` role, every
  batch in `world.edits` with before and after values). Every `apply_edit` bumps `world.worlds.revision`
  (`live_edit.py:530, 563`), the DM tool's NPC and layer edits and a dev world's economy edits included.
- `world_build.build(conn, created_by='', release=None)` (`tools/world_build.py:79-109`) needs an editor connection,
  writes `world.builds` and `world.build_cells`, keeps 3 builds' cells (`KEEP_CELLS`, `:35`), and sends
  `ratw_release`. `created_by` is at most 60 characters (`0010_game_builds.sql:13`). `world.builds` records no source
  revision; its `fingerprint` hashes `revision`/`updated_at`, the `LIVE_LAYERS` rows, the roster and the tools'
  source (`world_build.py:37-59`).
- The studio already has `Builds.rebuild` and `POST /api/dm/server/rebuild` (`studio.py:1084-1119, 1533-1543`).
- The server takes any newer build (`RatwGame.cpp:776-787`). With nobody on, it saves and exits 75. With players on,
  it tells them once and waits **with no limit** (`RatwGame.cpp:865-896`).
- The entrypoint restarts the server on 75, and on 76 waits a minute and tries again. **On any other exit it stops
  the loop and the container exits** (`entrypoint.py`, `main()`); `restart: unless-stopped` then starts it again.
- `world_package.py` hardcodes `live` in five places (`_write_registry`, `register` and its refusal, `new_world(...,
  'live')`, `W.grant_connect(..., 'live')`). That `grant_connect` runs only on a first import, as the superuser.
  `W.ensure_roles` runs on every start as the superuser.
- The package leaves Atlas's source out and `dm.*` out of exports (the module's "Out:" docstring,
  `world_package.py:30-31`; `dm_usernames` `:818-823`); `import_build` writes the build rows directly "because
  world_build.build needs Atlas's tables". `world_package export` connects as `owner` (`:1044`).
- A live world gives `ratw_editor` no CONNECT (`CONNECT_ROLES['live']`, `world_db.py:76`; `compose.yaml:114`), so in
  slice 1 the editor role and its password do nothing.
- Measured on this laptop when one build was 119 MB (doc 70, section 7): a build takes **40 s and 3.9 GB of RAM**.
  Today's build is about 307 MB by doc 77's draft figure, not rechecked.

**The package**
- `Package/stage.sh` copies every top-level `tools/*.py` except tests (`:93-96`). So `dungeon_master.py`, `studio.py`,
  `map_editor.py`, `world_build.py`, `dm_service.py` and `apply_terrain.py` are all in the image today. `Data/Portraits`
  is not staged.
- The built image already has the `openssl` command (OpenSSL 3.5.7, checked in `ratw-world:latest`), pulled in because
  `ca-certificates` depends on it; it isn't named in `Package/Dockerfile:29`. Python's `cryptography` is absent, and
  the standard `ssl` can serve TLS but can't make a certificate.
- The world folder is mounted read-only at `/etc/ratw/host` (`compose.yaml:44`), root-only with mode 0700
  (`Dockerfile:38`). Nothing in it is writable from the container, and no child can read it. The game port is
  published on every interface (`compose.yaml:31`).
- **The world service mounts only `./` (read-only), `secrets` (read-only) and `logs`** (`compose.yaml:42-46`).
  `/var/lib/ratw` itself is the container's writable layer (`Dockerfile:36`), so anything kept there (the registry
  copy today; a key tomorrow) is lost on every `docker compose up -d` that recreates the container (an image update,
  an `.env` change) and on every `down` and `up`. No service sets log limits, so Docker's json-file logs grow
  without end.
- `passwords.env` is root-only (`chown 0:0`, mode 0600, in `compose.yaml`'s `db-start`, committed in 029d050); the
  root entrypoint hands each child only what it needs. `docker compose exec` runs as root, since the image sets no
  `USER`.
- The registry (`worlds.json`, `worlds.local.json`) sits in `STATE/config`, 0700 and owned by `ratw`; `world_db`
  finds it through `RATW_WORLDS_FILE` and `RATW_WORLDS_LOCAL` (`Dockerfile:45-46`). The Mind's `npc-ai.json` is 0600
  and owned by `ratw`. The agent's directory is `ratw:ratw-server`, mode 0770 (`entrypoint.py:547`), so the server's
  user can write files there, `host.json` included. `LOGS` is `ratw:ratw-server`, 0770 (`:308`), and holds the Mind's
  call ledger, `npc-mind-calls.jsonl` (`:416`), which the server's user can therefore rewrite. `run_as` starts every
  child with no extra groups. `logs/npc-voices.jsonl`, which holds players' lines to NPCs, is readable by group
  `ratw-server`.
- `server_command` never passes `--official`, so every package world runs as community. It still passes
  `--dm-directory`, where the server writes `snapshot.json` and inbox replies (`RatwDirector.cpp:501, 517-519`);
  nothing in the container reads them.
- `read_settings()` reads `world.toml` once, in `main()`.
- **The loop doesn't keep helpers up today.** `Helper.check()` runs only inside the server-wait loop
  (`entrypoint.py:558-562`), and every helper is stopped after any server exit other than 75 or 76 (`:577-580`).
- `compose.yaml:20` and `README.md:73` tell hosts that a second world uses `RATW_PORT=7789`. The README's "every day"
  section suggests `docker compose restart world` after editing `world.toml`.

**The host today**
- Central's heartbeat reply carries the host's pairwise id, `None` on official worlds, and the world's `kind`
  (`Central/central_service.py:911-916`), and the listing's address is the one central saw (`:915`, `address=seen`):
  the home's public address. The world agent writes only the host to `host.json` (`tools/world_agent.py:368-380`),
  and `null` on bad_token, delisted or banned.
- `isHost` = community world, ticket mode, the session's sub equals `host.json`'s, **and a wolf in the world**
  (`Core/RatwGameTickets.cpp:681`). An official world ignores `host.json` (`:695-700`).
- Every player ticket carries the wolf's global id and its name, which is unique across central
  (`central_service.py:696`). The heartbeat sends the online wolves' ids.

**The client and its shell**
- The front door's pages are `'login' | 'register' | 'roster' | 'creator' | 'review' | 'worlds' | 'arrive' | 'host'`
  (`frontDoor.ts:47`), switched in `show()` (`:363-379`). Commands go through `client.ts command()` (`:186` on) as
  `if (type === ...)` branches; `world_join` parses an address with `parseAddress` (`Client/src/net/worlds.ts:16-29`,
  default port 7788) and asks the shell's `world_status`.
- The shell (`Desktop/src-tauri`): `ureq 3.5.1` with rustls, `rustls 0.23.46` with ring (installed in `run()`,
  `lib.rs:175`), `tokio-tungstenite 0.29`. Commands are registered at `lib.rs:236-239`; the capability
  (`capabilities/default.json`) names only window `main`, so **a new window gets no IPC unless a capability names
  it**. The main window's CSP allows no `http:` or `https:` fetch (`tauri.conf.json`, `connect-src 'self' ipc:
  http://ipc.localhost ws: wss:`). The navigation rule `page_origin` (`lib.rs:69-84`, plugin `page-only`,
  `:190-199`) keeps windows on the app's own origin; its hook receives the asking `&Webview`
  (`tauri-2.12.5/src/plugin.rs:459-461`), which `lib.rs:188` ignores as `_` today. No dialog plugin.
  `withGlobalTauri: true` (`tauri.conf.json:11`) injects `window.__TAURI__` into **every** webview.
- `tokens.rs` keeps central's refresh token in the OS keychain first, and in a 0600 file only as a fallback
  (`tokens.rs:1-5, 44-56, 113`).
- rustls is built with `tls12` (`Cargo.toml:29`); ureq is built without gzip (`Cargo.toml:31`, `default-features =
  false`, `["rustls", "json"]`); ureq's `Connector` lives in `ureq::unversioned` (ureq `lib.rs:609`), which promises
  no semver stability.
- `world_status` (`central.rs:321-355`) refuses link-local, unspecified, multicast and broadcast addresses
  (`pingable`, `:357-370`), caps replies at 2 KB, follows no redirects and uses no proxy (`UreqHttp`, `:100-117`).
- **ureq can't pin a certificate by itself.** Its `TlsConfig` offers only `root_certs`, `disable_verification` and
  `use_sni` (`ureq-3.5.1/src/tls/mod.rs:197-307`), and it builds its own rustls config (`tls/rustls.rs:137-190`).
  Root certificates fail on the name (the world can't know its public address or DNS name), and turning verification
  off is not acceptable. What works: rustls's `ClientConfig::builder().dangerous().with_custom_certificate_verifier`
  (the API ureq itself uses at `rustls.rs:166-170`) inside a copy of ureq's `RustlsConnector` (`rustls.rs:25-95`),
  given to `Agent::with_parts` (`agent.rs:130`, documented as low-level).
- **A custom URI scheme:** `register_asynchronous_uri_scheme_protocol` (`tauri-2.12.5/src/app.rs:2290-2335`). Its
  origin is `editor://localhost` on Linux and macOS and `http://editor.localhost` on Windows, and its handler is told
  which webview asked (`UriSchemeContext::webview_label()`, `app.rs:2648`).
- **A webview can't simply trust the self-signed certificate on every platform:** WebKitGTK has
  `allow_tls_certificate_for_host` (per host, not per port; not tried), WebView2 has a certificate-error event (not
  checked here), and wry exposes no hook on macOS.
- `Editor/` is React 19 and Vite 8, about 1.1 MB built, with `base: './'` already set (`Editor/vite.config`). All its
  network use goes through three fetch sites, all relative paths (`Editor/src/dm/api.ts:116`, `Editor/src/lib/api.ts:18`
  and `:27`), so it works unchanged behind a same-origin proxy. It has data-derived links: `href={gameUrl(...)}`
  (`dm/ServerTab.tsx:84`) and `window.open(gameUrl(server.play))` (`components/Dialogs.tsx:217`). It keeps state in
  localStorage under fixed keys (`ratw-editor-identity`, `lib/live.ts:268`; `EconomyControls.tsx:22`, which spreads the
  parsed value into a filter; `MapView.tsx:51`; `Chrome.tsx:286`; `TerrainPalette.tsx:10`), so one origin shared by
  every world would carry values from one world's page to the next.
- The studio serves over 100 `/api/...` routes across the three files (109 quoted paths by grep; more are built).

**Gaps found on the way**
- **The cheats aren't tied to the listing.** The Dev Console's cheats check only `Entity::dungeonMaster`
  (`RatwGameDev.cpp:113, 132, 147`; `RatwGame.cpp:3203`), and that flag is saved with the character
  (`RatwWire.cpp:372, 563`). The listing's `cheats` comes only from `--dev-tools` (`RatwGameTickets.cpp:151-152`).
- **`--dev-tools` hands cheats to every player.** The socket messages `gift`/`grant` (Gifts, and goods from nothing),
  `practice`, `socialLevel`, `deed`, `acquaint`, the weather's `front` and `wind`, and the storyline `approve`,
  `give`, `post` and `complete` verbs check only `options_.devTools` (`RatwGame.cpp:4899, 4981, 4999, 5155-5184,
  5285-5330`).
- **A server crash takes everything in the container down with it,** in a loop if the cause is the build.

## 5. Threat model

A host already owns their Postgres volume, `passwords.env` and every file in the world folder. The tools give them no
data they can't already read. What is new is **a port that may face the internet**, **accounts that aren't only the
host's**, and **a client that connects to worlds other people run**. So the design's job is to make that port safe
against guessing, flooding and interception; keep passwords and sessions out of any page a world serves; run only code
the user signed in the client; keep one world's page from reaching another world; keep a compromised game server from
climbing into the host's tools; keep host powers off official worlds and central; keep the listing honest about cheats;
and tell players and co-builders the truth about what a host can see.

The boundary is the Editor account's password and the session it opens, carried only over TLS to a pinned key, in a
window bound to that one session. Binding the port to loopback (`RATW_EDITOR_BIND`, the default) or not forwarding it
is defence in depth, not what grants anything.

| Who | Wants | Must not get |
|---|---|---|
| Anyone on the internet who finds the Editor port | To guess a password | A session. Passwords are at least 12 characters, checked against a breached-password list and the world's name, and hashed with scrypt; one-time passwords are 120 random bits and expire; failures are limited per address (IPv6 per /64), per account and world-wide (section 6.5). The port answers only the world's computer until the host opens it. |
| The same | To wear the world out | Its CPU, memory, disk or the owner's way in. At most 2 password checks at once; separate pools for signed-in and not signed-in work; 4 connections and a few handshakes a second per address; a token bucket on new connections; fixed-size counters that fail closed; Docker log limits; folded audit rows. **Residual:** under Docker Desktop, if the container sees every peer as the gateway, one stranger can keep the port busy for everyone without a known device or an Editor code. The fix is not opening the port, stated in section 3.2. |
| The same, aiming at the owner | To lock the owner out by spraying wrong passwords at their name | That. The owner's name isn't predictable (`host-` and four random characters). Known devices, Editor codes and accounts still on a one-time password each have their own lane, which failures elsewhere never fill. `ratw-editor invite` and `reset` on the world's computer are never limited. |
| The same, probing names | Which usernames exist | That. Unknown and disabled names run the same scrypt against a dummy hash, and every failure has the same words. No audit row is written per unknown name. |
| Someone on the network path (plain `ws://` is normal on community worlds, doc 77 section 5, point 6) | To read or take over an Editor session | Anything. The Editor port is TLS 1.3 only; the shell pins the world's key; no password or token ever crosses `ws://` or `http://`. The game socket is unchanged. |
| The same, at the very first connection | To present their own key and catch the password (trust on first use) | With an Editor code, nothing: the code carries the key. With a bare address (only the owner typing one), hard, not impossible: screen 3 shows all eight groups and where to compare them, and from a public address the button reads "I COMPARED IT". This is SSH's trade-off, stated in the guide. |
| The same, later | To swap in their key, perhaps claiming to be a new world | Anything. Pins are kept by key as well as by address: any new key at a trusted address is red, whatever world id or name the reply claims, and a known world id under a new key is red. An honest new key comes signed by the old one. |
| A host or co-builder who reuses a password | — | Their other accounts' safety. The screens say "a password only for this world"; Editor passwords are never central's, and a stolen central password opens no Editor. |
| Someone holding a leaked host token (`rh_…`, in `world.toml`, the environment, screenshots) | Admin | Anything beyond "list a world". The helper never accepts the host token, and never shows it: the settings view is an allowlist of fields. |
| Someone with a stolen central session for the host | DM powers | Nothing. Editor accounts are the world's own. |
| A co-builder | The owner's password, or more power than their role | Neither. Device tokens count only for the account they were issued to, so a co-builder's tokens buy no guesses at the owner. Roles are checked on every route, and accounts, owners and ending sessions are only on screen 5 behind the owner's password. Non-owners see only their own sign-ins and no log lines, folder path or addresses. |
| Someone who steals an owner session | To stay in after it ends | That. Making or promoting an owner needs `ratw-editor` on the world's computer; adding any account, changing a role and ending sessions need the password typed again in the client's own screen; every account change is logged and shown to the owner. |
| A compromised `ratw_server` (large, internet-facing C++) | To climb into the helper, forge its history or switch it off | None of these. The key and the helper's files live in their own volume, which only root and `ratw-admin` can read. `ratw_game` has no grant on any `dm.*` table that matters: `dm.admins`, `dm.sessions`, `dm.devices`, `dm.invites`, `dm.locks`, `dm.audit` and `dm.ai_use` are all revoked from it. It can't fake the AI budget (the Mind's ledger moves out of its reach) or mark the world official (the agent writes `kind` where only `ratw` can). What it **can** do: write hostile strings (names, notices, health reports) that the Editor page shows, so the page treats every string as text. |
| A hostile host | Official progress, central's keys, code in other players' clients, a way to hang their clients | All of these. Their power stays inside their own database, and Publish's validator caps what reaches a client. |
| A hostile host | Players' words, addresses, and which wolves are the same player across worlds | **They get the words and addresses** (they own the database and the socket). Wolf names are unique across central, so wolves are recognisable on every world. Players are told both plainly (section 9). Pairwise ids hide which **account** a wolf belongs to, not the wolf. |
| A hostile host, against a co-builder ("join my build team") | The co-builder's RATW password, files on their disk, or a way into their client | None of them. The Editor window runs only the signed bundle (section 6.6), so the world can't serve its own script: no fake "RATW sign-in" form, no script aimed at a WebKitGTK or WebView2 bug. Downloads go through the shell's save dialog and an extension allowlist. The client and guide say RATW never asks for the RATW password in the Editor. The world gets only a slow scrypt hash of a password used nowhere else. |
| A hostile world, against another world the same client manages | To drive the host's real world from a window left open | That. Each Editor window is bound to one world, account and session; a switch, a sign-out or another username destroys it and its storage; the proxy refuses a window whose session isn't current; queued saves retry only for the same world and account. |
| A hostile world, against the client's main window | To spoof the client's own screens with text it controls | That. The shell validates the hello (a 40-character name without control or bidirectional characters, progress as a code), the window title's address comes from the shell, and the text-only test covers the Editor screens in `frontDoor.ts`. |
| A web page in the host's own browser | CSRF or DNS rebinding against the Editor port | Anything. Browsers can't finish the TLS handshake without the host clicking through a warning; even then the helper refuses any request carrying `Origin`, `Cookie` or `Sec-Fetch-*` (the shell sends none), and sessions are bearer tokens the browser never has. |
| Someone who gets the world folder or a backup | Editor sign-in | Little. The key isn't in the folder (only its fingerprint); an unused `first-editor.txt` expires in 7 days and is emptied the moment it is used; backups hold scrypt hashes and SHA-256 session hashes, and a restore keeps today's accounts and ends every session. "Share my world" strips `dm.*`. |
| Someone who can write the world folder from inside the container | To replace `editor-certificate.txt` so a host compares against a fake | That. Only root inside the container writes `./manage`; a test tries as `ratw-server` and `ratw`. |
| Another program on the computer | The host's tools | Out of scope: anything that can read the world's volume or run `docker` already owns the world. The client keeps the session token in memory only, device tokens in the OS keychain, and pins in a 0600 file. |

## 6. The way in

### 6.1 The rules (each one gets a test)

- **I1.** Admin is never carried over the game socket or the game port. The game port stays `/ws` and `/status`, and
  ticket mode serves no page (doc 77, section 5.1).
- **I2.** The Editor port speaks TLS 1.3 only, with the world's own key. The client trusts it only through the pin the
  shell holds. Nothing on the Editor port is ever answered in plain HTTP.
- **I3.** Authorization comes from an Editor account's password and then a world-local session, **never from the peer
  address.** Under Docker Desktop every peer may look like the gateway, and through a tunnel every peer is loopback
  (doc 77, section 8; the same reason ticket mode refuses `--dev-identity`). The address is used only to slow guessing.
- **I4.** The password and the session token never reach a page the world serves. The password is typed only in the
  client's bundled main window and handed to the shell once; only the shell holds the token, in memory.
- **I5.** Nothing admin runs on an official world (section 6.7).
- **I6.** Only code the user signed runs in the client. The Editor page is the image's own files, signed at staging
  with the release key; the shell checks every page file against the signed list and refuses anything else. The
  helper never serves page code from the database or the world folder. The shell gives the Editor window no
  capability and sets its CSP itself.
- **I7.** No request to the helper names a process, file, path or command. The only process it starts is the fixed
  build child. Restoring a backup and deleting the world are never reachable through the port.
- **I8.** The game server's user can't read or write anything the helper trusts.
- **I9.** Browsers can't talk to the helper: a request with `Origin`, `Cookie` or any `Sec-Fetch-*` header is refused;
  there are no `OPTIONS` answers and no `Access-Control-*` headers.
- **I10.** Each Editor window is bound to one (pin, world id, username, session) when it is made. It is destroyed, with
  its storage, when any of them changes, and the proxy refuses a window whose binding isn't the current session.
- **I11.** No `dm.*` table the helper trusts is readable or writable by `ratw_game`. Only the tables the server
  already uses (`dm.actions` among them; M2a lists them by grep) keep its grant. Every migration that adds a `dm.*` table revokes `ratw_game` on it, and a test lists the grants.
- **I12.** Text a world controls never reaches the main window unchecked: the shell validates and trims the hello, and
  the client words every message itself.
- **I13.** Owners are made only on the world's computer. Account changes and ending sessions need the password typed
  again, in the client's own screen, never in the Editor window.
- **I14.** No request value ever becomes SQL text or an identifier. Today's f-string SQL takes identifiers only from
  fixed maps or the catalogue (`studio.py:750`, `live_edit.py:227`, `dungeon_master.py:1738`); a grep test keeps it so.
- **I15.** Logs never hold a sign-in body, a query string, a token, a password or a traceback with request values.

### 6.2 Why not the alternatives

| Option | Why not |
|---|---|
| A "manage" message on the game socket | Breaks I1. Plain `ws://` on most worlds, readable and takeable on the path. Messages are capped at 64 KB (`Core/RatwLink.h:26`); Atlas sends megabytes. The work would run on the game thread. `isHost` needs a wolf. And it goes dark whenever the server restarts or closes, exactly when the host needs it. The in-game host commands stay on the socket as they are. |
| An HTTP route on the game port | New proxy C++ in the internet-facing process, on an internet-facing port, mostly without TLS. |
| Publishing the studio's port 8765, or Postgres | The studio's only guard is a forgeable header check; it binds `0.0.0.0`; it speaks plain HTTP. |
| Hanging admin on `isHost` and the player session | Every join mints a player ticket with no step-up, so a stolen central session becomes a DM, over a socket anyone on the path can read. |
| Accepting the host token | It sits in screenshots and the environment. Doc 77 promises it "can only list a world". |
| Central accounts, through a central admin ticket | Needs central up, the host flag (approved by hand in the closed beta, `central_service.py:986`), ticket mode and a listed world, so a host building before listing, a LAN host or a delisted host gets nothing. Safe, it needs its own signing keys, a challenge nonce, a revocation epoch and clock-skew handling. Co-builders would need central accounts tied to the world. The user chose the world's own accounts. |
| A manage key in the world folder (this doc's earlier draft) | Only works on the computer that holds the folder; another computer needs an SSH tunnel. The user wants to manage by address, from anywhere. |
| A CA-signed certificate (Let's Encrypt) | Needs a domain name and port 80 or a DNS provider's API; most hosts have a bare home address. Pinning needs neither. |
| The Editor window loading the world's page itself over the self-signed certificate | macOS has no hook without patching wry; WebKitGTK's is per host, not port; WebView2's is unverified. And the password would be typed into a page the world serves. |
| A loopback HTTP proxy in the shell (as `relay.rs` does for the game) | Any local program or browser tab could reach it, guarded only by a path token. A custom URI scheme is reachable only from the client's own webviews, and the handler checks which one. |
| Plain HTTP on the home network | Puts the password on the network in clear text. Never. |
| A pairing code from `docker compose logs` | Hosts don't read logs. The Editor code in `manage/first-editor.txt` does the same job once, then the host's own password. |
| Comparing a fingerprint by eye as the only first-connection check | Nobody really compares 64 hex characters, and co-builders would need the owner to pass on four separate things. The Editor code carries the key, the name and the one-time password in one string; bare trust on first use is left only for an owner typing an address. |
| The world's own Editor bundle, unsigned (the rewrite's shape (a)) | A hostile world serves its own JavaScript, and the shell's `script-src 'self'` lets it run, because 'self' is the world's origin. A signed bundle keeps the no-drift benefit without that. |
| Evicting the oldest session when a world's session limit is full | Any co-builder could sign the owner out by signing in ten times. A full world refuses the new sign-in instead. |
| The Editor port open on every interface by default (the rewrite's Q1 (a)) | It assumed IPv4 NAT. An IPv6 router may let traffic straight in, a laptop on café Wi-Fi is on someone else's network, and a fresh world's one-time password would wait on an open port. Loopback by default costs a remote host one line in `.env`, which they are editing anyway to forward the port. |

### 6.3 Editor accounts and signing in

**Where they live.** In hosted mode `ACCOUNTS` becomes the container's world key, `world`, so `dm.admins` and
`dm.sessions` live in the world's own database. The migrations make the tables; the package carries no rows (`dm.*`
stays out of exports, `world_package.py:30-31`).

**A schema-only migration:**
- the role CHECK widens to `viewer`, `dm`, `builder`, `owner`, `admin`, and `RANK` becomes viewer 0, dm 1, builder 2,
  owner 3, admin 4. In the user's studio nothing changes: Atlas still needs dm there, and only admin gets
  `character.dm`. In hosted mode Atlas needs builder, My world's actions need owner, and **an admin row can't sign in
  at all**;
- `dm.admins` gains `must_change boolean` (a one-time password), `must_change_until` (its expiry), `may_act_on_wolves
  boolean` (Q24), `claimed_from` and `created_by`; `dm.sessions` gains `last_seen` (the idle timeout) and `device`;
- new tables: `dm.devices` (`token_hash`, `username`, `created_at`, `last_used`; at most 5 an account, the oldest
  dropped), `dm.invites` (`invite_hash`, `username`, `expires_at`, `used_at`: the sign-in pass inside an Editor code),
  `dm.locks` (per-account lock state, so a helper restart keeps it) and `dm.ai_use` (the tools' AI calls, section 7);
- `REVOKE ALL ON dm.devices, dm.invites, dm.locks, dm.ai_use, dm.audit FROM ratw_game`, and the I11 rule for every
  later `dm.*` table.

**The first account (Q3).**
- On a start that finds no Editor accounts, the entrypoint asks the helper's code to make the owner: `[editor] owner`
  if set, otherwise `host-` and four random characters, at role owner, with `secrets.token_urlsafe(15)` as a one-time
  password (as `create_defaults` does, `dungeon_master.py:243-252`), `must_change` set, valid 7 days, and an invite.
- The root entrypoint writes it, with the Editor code, to `manage/first-editor.txt` (section 3.1). The log says only
  "Your first Editor account is in manage/first-editor.txt", never the password.
- Compose gains a writable bind mount, `./manage:/var/ratw/manage`, which only root inside the container writes, for
  this file and `editor-certificate.txt`. The package's zip ships empty `manage/` and `backups/` folders, so Docker
  doesn't make them root-owned on Linux. On Docker Desktop these are Windows folders, where `chown` and `chmod` may
  fail or do nothing (M0a checks); such errors are logged and never stop the start. On NTFS anyone signed in to that
  Windows account can read the file, the same as `world.toml`'s host token.

**Editor codes.** One string, `ratw-editor:` and base64url of a small record: the world id, the full
SHA-256 of the key's SPKI, the username, the one-time password or an invite secret, and, when the maker knows it, the
address. `first-editor.txt`, the reset script, `ratw-editor reset|invite|add` and screen 5's add and reset all make
one. The client pins from it and signs in with it. It is a secret: it expires with its one-time password (7 days for
the first owner, 72 hours otherwise) and works once.

**`ratw-editor`,** a command in the image (section 3.6): `list`, `add NAME --role R` (the only way to make an owner),
`reset NAME`, `invite NAME`, `disable NAME`, `enable NAME`, `remove NAME`, `sign-out-all`, `fingerprint`,
`new-certificate [--leaked]`. It runs as root only to read the dm password, then as `ratw-admin`, refuses to remove or
disable the last owner, and prints codes only to its terminal. After `new-certificate` it signals the running helper
(root can) to reload its key.

**Signing in** (the helper's routes; section 6.5 has the limits):
- `POST /editor/login {username, password | invite, device?}` → `{token, username, role, mustChange, device}`. A good
  one-time password returns a session that may only call `/editor/password`.
- `POST /editor/password {old, new}`: the rules on screen 4b; clears `must_change`; ends the account's other sessions
  and forgets its devices; drops a request for the entrypoint to empty `first-editor.txt` if it was the owner's.
- `POST /editor/step-up {password}`: a 5-minute grant, bound to the session, for the account routes. The shell calls it
  from screen 5, never from the Editor window.
- `GET|POST /editor/accounts...` (owner, with a step-up): list, add (`viewer`, `dm` or `builder`, returns an Editor
  code once), reset, role (up to builder), "may act on players' wolves", disable, enable, remove, end all sessions.
  These are the helper's own routes, not the studio's, and the proxy refuses `/editor/*` from the Editor window.
- `POST /editor/keepalive`, `POST /editor/logout`.
- **`/api/dm/login` is refused in hosted mode**, and the page never signs in: in hosted mode its `SignIn` form isn't
  shown, `authHeaders()` adds nothing, and the shell adds the bearer header.

**The password rules:** at least 12 characters, not containing the username or the world's name, and not on a list of
the 100,000 commonest breached passwords bundled in the image (as a Bloom filter or a sorted list, under 2 MB). The
helper checks; the client words the reason.

**Sessions** reuse `dm.sessions` rows, so they survive a helper restart:
- 30 minutes idle, through `last_seen`, written only when it is over a minute old (not on every request). The shell's
  keep-alive renews it only after the host's input in the Editor window in the last 30 minutes, so an open window left
  alone does time out. 12 hours at most.
- 3 live sessions an account, the account's oldest evicted. When the world's 10 are full, the **new** sign-in is
  refused ("Too many people signed in to this world's Editor"); another account's session is never evicted.
- The owner's "End all Editor sessions" and `ratw-editor sign-out-all` delete them all, and every account's devices.
- The audit and `requested_by` name the account plus the session's short id (a mapping from `who['username']`), and
  each sign-in records its address for the accounts list (Editor accounts only; players' addresses stay out of the
  tools, section 9). Only the owner sees other accounts' addresses.

### 6.4 The Editor certificate

- **Made once.** On a first start with none, the root entrypoint makes an ECDSA P-256 key and a self-signed
  certificate with the image's `openssl` (named in `Package/Dockerfile:29` from now on, so it stays even if
  `ca-certificates` drops it): `openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -days 36500
  -subj "/CN=RATW Editor <world id>"`.
- **It lives in a named volume of its own,** `admin:/var/lib/ratw/admin` (owned by `ratw-admin`, mode 0700), with the
  rest of the helper's state (section 6.5). Today `/var/lib/ratw` is the container's writable layer, which every image
  update and `down`/`up` throws away; the key there would change on every update, every client would show the red
  screen, and hosts would learn to press FORGET. "No certificate" means "the volume holds none". It is lost only with
  the volume (`down -v`, a new world): then every client warns, as it should.
- **The fingerprint is the SHA-256 of the public key** (the SPKI), not of the whole certificate, so a renewed
  certificate on the same key needs no new trust. It is printed in the log at every start, written to
  `manage/editor-certificate.txt` with the world id and the host-side port (compose passes `RATW_EDITOR_PORT` in), and
  shown in My world.
- **`ratw-editor new-certificate`** makes a new key and keeps the old one. `/editor/hello` then carries the new key's
  SPKI signed by the old key. A client pinned to the old key that meets the new one checks that signature against the
  old public key it stored, and moves its pin over with a notice. The red screen stays only for `new-certificate
  --leaked` (no signature: every session ends, every client warns, and the command says to send co-builders new codes)
  and for a lost volume.
- **The helper serves it** with Python's `ssl.SSLContext(PROTOCOL_TLS_SERVER)`, TLS 1.3 only, and reloads it on a
  signal from root.

**In the shell (`Desktop/src-tauri/src/editor.rs`, new).**
- A rustls `ServerCertVerifier`, `PinnedVerifier`: `verify_server_cert` compares the SHA-256 of the end entity's SPKI
  with the pin and ignores the name and the dates; `verify_tls13_signature` delegates to
  `rustls::crypto::verify_tls13_signature` with ring's `signature_verification_algorithms`, so the server must hold the
  key (without that, a pin would mean nothing). The config is built with
  `ClientConfig::builder_with_protocol_versions(&[&rustls::version::TLS13])`, so TLS 1.2, compiled in for central,
  is never offered here.
- For a first connection from a bare address, the same verifier in "record" mode: it accepts any key, records its
  SPKI, and the connection is used only for `/editor/hello`. Nothing secret is ever sent on it: no username, no device
  token, no Editor code.
- An HTTP client built from it: ureq's `Agent::with_parts` with a copy of its `RustlsConnector` holding this config
  (section 4). ureq is pinned to `=3.5.1`, because `Connector` sits in `ureq::unversioned`. No redirects, no proxy,
  15 s to connect, the `pingable` filter on the address, `Accept-Encoding: gzip` with ureq's `gzip` feature. M0a checks
  that `with_parts` is usable; if not, a small hyper-rustls client is the fallback.
- **The pin store:** `editor-worlds.json` in the app data dir, 0600, **keyed by the key**, with the addresses that key
  was met at as aliases: the SPKI and its SHA-256, the world id and name, the last username. Never a password, a
  session token or a device token: **device tokens go in the OS keychain**, through the same `TokenStore` pattern
  `tokens.rs` uses (keychain first, a 0600 file only as fallback), because they are credentials.

### 6.5 The helper: `tools/world_admin.py` (new, in the world's container)

**Process and user**
- One Python process, started by the entrypoint as a new user `ratw-admin` (10003), in neither the `ratw` nor the
  `ratw-server` group. So it can't read `npc-voices.jsonl`, and the server can't read anything of its.
- It gets the `dm` database password from the entrypoint's environment, and from slice 2 the `editor` password (a live
  world gives `ratw_editor` no CONNECT, so in slice 1 it would do nothing). Nothing else: no owner, superuser,
  publisher, game role or host token.
- **Its own files, all in the `admin` volume** (root and `ratw-admin` only): the certificate and keys; its own copy of
  the registry (it starts with `RATW_WORLDS_FILE` and `RATW_WORLDS_LOCAL` pointing there); its own copy of the AI
  config (`RATW_AI_CONFIG`, mode 0600); a cleaned copy of the settings (no token, no AI key) that the entrypoint
  writes before each server start, since the helper can't read `/etc/ratw/host`; `central.json`, the host id and
  central's `kind`; the "ever official" record; and `requests/`, where the helper drops kind-only request files.
  It uses `central.json` only for display, the official refusal and host-only admission, never to grant anything.
- **Requests to root.** The helper can't signal the server, run `pg_dump` or read the world folder, so it asks: a file
  in `requests/` naming only a kind (`restart`, `backup-before-publish`, `backup-by-hand`, `share`,
  `first-editor-used`). The root entrypoint watches the directory, acts, and removes the file. Backups go one step
  further, to the backup container, through a small `requests` volume both services mount.
- It listens on `0.0.0.0:17789` inside the container, published by compose as section 3.2 says. With `[editor]
  enabled = false` it doesn't listen at all. The entrypoint restarts it when the `[editor]` settings change.
- **It starts first,** before the import and the migrations, and answers `/editor/hello` with "setting up" and a
  percent from a progress file the entrypoint writes, refusing sign-ins until the world is ready.
- **It stays up whatever the server does.** The entrypoint's loop is restructured so it checks helpers while the
  server is down too (today `Helper.check()` runs only while the server runs, and helpers stop after any exit but 75
  and 76). So Close, Open, Restart, Publish and "back to the previous map" keep working through crashes, restarts and
  maintenance.
- **Restart** is a `restart` request file: the root entrypoint stops the server (SIGTERM, it saves) and starts it
  again, re-reading `world.toml` first.

**Routes**
- `GET /editor/hello`: `{world, name, package, editorApi, kind: "community", serverState, progress: {code, percent},
  setupPending, rotation?}`. Public, at most 1 KB. `setupPending` says the owner still has a one-time password (it
  can't be guessed, so telling costs nothing). `rotation` is the new key's SPKI signed by the old one.
- `POST /editor/login`, `/editor/password`, `/editor/step-up`, `/editor/keepalive`, `/editor/logout` and the account
  routes (section 6.3). Bodies on routes without a session are capped at 4 KB, `Content-Length` is checked before
  reading, and chunked bodies are refused.
- **Login and hello are the only routes without a session.** The page's files, `GET /` and below, from the image, and
  the signed bundle list need a session too (section 6.6).
- Everything under `/api/` needs `Authorization: Bearer` and is dispatched, in the same process, into the hosted
  studio (section 7). There is no second listener: one on the container's loopback would be reachable by the server's
  user, which is the loopback trust I3 forbids.

**The browser rule (I9).** Bearer tokens only, never cookies; a request carrying `Origin`, `Cookie` or `Sec-Fetch-*`
is refused; the `Host` header is ignored (the world can't know its own public address); no `OPTIONS` and no CORS;
replies are `application/json` or raw bytes for ground, except the page's files, all with `X-Content-Type-Options:
nosniff`.

**Limits on connections**
- Two bounded pools of worker threads replace one thread per connection: a small one for work without a session
  (hello, login) and one for signed-in work, so a big `/api/live/world` reply or the 11 s validator can't starve
  sign-in.
- Per address (IPv6 counted per /64): at most 4 open connections and a few new handshakes a second. World-wide: a
  token bucket on new connections, and at most 32 open. The TLS handshake within 10 s; idle connections closed after
  60 s.
- At most 2 large bodies (over 1 MB) at once; a cap on the helper's database connections.
- Every handler catches every exception, so no request can crash the helper.

**Limits on signing in**
- **Password checks:** at most 2 scrypt checks at once, at most 8 waiting. Requests from a known device or with an
  invite go to the front of the queue. Beyond that "The world is busy: try again in a minute." (503).
- **Unknown and disabled usernames** run scrypt against a dummy hash made at start, so their timing matches. The reply
  is always "Wrong name or password."
- **Device tokens.** A good sign-in returns a random device token (stored hashed in `dm.devices` with its account),
  which the shell keeps in the keychain. A token counts as known **only for the account it was issued to**; shown
  with another username it counts as unknown, and as a failure. A sign-in from a known device is counted only against
  that device: 5 failures, then 15 minutes' wait for that device.
- **Lanes that strangers can't fill:**
  - known devices (above);
  - **invites:** a sign-in carrying a valid invite from an Editor code is looked up cheaply before any scrypt and
    skips every failure count (only the connection limits and the scrypt queue apply). An invalid invite is a
    failure like any other;
  - **accounts still on a one-time password:** 120 random bits can't be guessed, so failures against them never lock
    the account and don't count world-wide. They still count per address and wait for the scrypt cap, so they can't
    be used to burn the world's CPU. Under Docker Desktop, where addresses fold, the Editor code is the way that
    always gets through, which is why `first-editor.txt` leads with it.
- **Everything else** (a typed password from a new device) is counted three ways:
  - per address: 5 failures in 15 minutes, then waits that double up to an hour;
  - per account: after 5 failures from unknown devices, that account refuses unknown devices without an invite for 15
    minutes, doubling up to 4 hours, kept in `dm.locks`. **Known devices and invites are never blocked by this.** The
    owner on a new computer runs `ratw-editor invite` (or the reset script) for a code;
  - world-wide: at most 20 **failures** a minute from unknown devices; good sign-ins don't count.
- **With `[editor] new_devices = "approve"`** (Q23), a typed password from a new device isn't enough at all: it needs
  an Editor code too. A stolen password is then useless from a new computer.
- Under Docker Desktop the container may see every peer as the gateway (M0a checks); then the per-address limits fold
  into the world-wide ones. Known devices, invites and one-time accounts still get through; everyone else may wait
  (section 3.2's residual).
- **The counters** are in fixed-size tables keyed by address and by account; when a table is full, everything counts
  against the world-wide bucket (failing closed). Per-account lock state is in `dm.locks`, so a restart keeps it.
  After a restart the world-wide bucket starts half full.
- **The audit and the log:** a failure for a known account is one audit row per account and address per hour, with a
  count. Unknown usernames get no row. Above a threshold, failures without a session fold into one log line and one
  audit row a minute, world-wide. The owner sees "N failed sign-ins today" on screen 5.

**Limits once signed in:** 20 requests a second a session; bodies up to 16 MB (Atlas only; the DM suite's own limit is
64 KB); one build at a time; Publish at most 6 an hour an account, so a hostile builder can't loop the fallback. Each
signed-in request writes one `RATW_EDITOR` log line (account, route, status; never a body, query string or token).
Every service in compose gets `logging: {driver: json-file, options: {max-size: 10m, max-file: "5"}}`, so logs can't
fill the disk.

### 6.6 The shell and the page

**The commands, granted to the main window only** (registered beside today's five, `lib.rs:236-239`):
- `editor_connect(address)` → `{state: "new" | "trusted" | "moved" | "rotated" | "changed", world, name,
  fingerprint, previous?, hello}`. Parses with its own default port (17789), tries 127.0.0.1 for `localhost`,
  filters the address (`pingable`), resolves names, and connects in record mode. On a TLS failure it tries the game's
  `/status` once, for screen 3's "that's your game port"; that check stores nothing. **It validates the hello**:
  unknown fields dropped, the name cut to 40 characters without control or bidirectional characters, progress a known
  code and a percent, the whole reply at most 1 KB.
- `editor_use_code(code)`: decodes an Editor code, stores the pin it carries, and returns the address and username.
- `editor_trust(address, fingerprint)`: stores the pin. `editor_forget(address, worldName)`: removes a changed pin,
  only if the typed name matches the stored one.
- `editor_sign_in(address, username, password | code)`: connects with the strict verifier, posts `/editor/login`
  with that account's device token, keeps `{address, pin, world, token, username, role}` in memory. Returns the role
  and `mustChange`. One Editor session at a time: signing in to another world, or as another username, signs out of
  the first and destroys its Editor window (I10).
- `editor_change_password(old, new)`, `editor_sign_out()`, `editor_step_up(password)` and `editor_accounts(...)` for
  screen 5's accounts panel.
- `editor_open(suite)`: opens or focuses the window labelled `editor` at `editor://localhost/?suite=dm` or
  `?suite=atlas` (`WebviewUrl::CustomProtocol`; the Windows origin differs, so never a literal URL), with the shell's
  title. The window is **incognito** (`WebviewWindowBuilder::incognito`), so its storage dies with it, and is made
  anew for every session.
- A keep-alive every 5 minutes, sent only if the Editor window had the host's input in the last 30 minutes.

**The proxy: the `editor` URI scheme.** `register_asynchronous_uri_scheme_protocol("editor", ...)`:
- refuses any request whose `webview_label()` isn't `editor`, so the main window can't use it, and any request from
  an Editor window whose binding (pin, world id, username, session) isn't the current session's;
- forwards `GET` and `POST` only, for the page's files and `/api/...`, to `https://ADDRESS/...` over the pinned
  client, with the session's `Authorization: Bearer` added in Rust. It refuses `/editor/*` outright;
- sends on only `Content-Type`, `Accept`, `Content-Length` (Atlas's reader needs it) and the body, so the page can add
  no `Cookie`, `Origin`, `Authorization` or `Host`; requests up to 16 MB, replies up to 32 MB **after**
  decompression (and the compressed size counted too), no redirects, a 60 s timeout;
- **checks every page file against the signed bundle list** (below) before handing it over, and refuses any file not
  on it;
- passes back only the status, `Content-Type` and body. Replies to `/api/...` are forced to `application/json` (or
  `application/octet-stream` for ground) with `nosniff`, so no API reply can run as script. It sets the page's CSP
  itself: `default-src 'self'; script-src 'self'; connect-src 'self'; img-src 'self' blob: data:; form-action 'none';
  base-uri 'none'; object-src 'none'; frame-src 'none'; frame-ancestors 'none'`, no inline script or eval, whatever
  the world sent;
- on a 401 tells the main window, which shows "Your Editor session ended" on screen 5.

**The signed bundle (Q6).** `Package/stage.sh` builds `Editor/` into the image, root-owned and read-only, and writes
`bundle.json`: every file's path and SHA-256, the bundle's version and `editorApi`, signed with the user's release key
(ed25519, through `ring`, already a dependency). The shell holds the public key. On a new session it fetches
`bundle.json`, checks the signature and that the version is no older than the oldest the shell accepts (so a world
can't serve an old bundle with a known bug), and then checks each page file's hash as it passes. An unsigned or
unknown file is refused, and the window says "This world's Editor isn't an official RATW build." A host running a
modified image can still run the game; their Editor window won't open. The release key's private half stays off the
repository, beside central's signing keys.

**The Editor window gets no capability at all.** Tauri 2 grants IPC to a window only when a capability names it, and
none does. `withGlobalTauri: true` still injects `window.__TAURI__` into it, so a test calls the built-in commands
from that window (`plugin:window|set_title`, `plugin:event|emit`, `set_fullscreen`, the window-state and page-only
plugins' commands, `central_call`) and expects every one refused. The navigation rule (`page_origin_on`,
`lib.rs:76-84`) learns one case, using the `&Webview` its hook already receives: the window labelled `editor` may show
only the `editor` scheme's origin and may open no new windows, and the `main` window keeps refusing that origin.

**Downloads.** An `on_download` handler refuses any download outside an allowlist of extensions (`.json`, `.png`,
`.csv`) and always goes through the shell's save dialog. Exports to files are removed in hosted mode anyway
(section 7); this catches the rest.

**What this keeps from the rewrite's shape (a):** the page and its API ship together in the world's image, so a DM
change needs no client release, and the page talks plain relative `fetch` to its own origin with no transport swap.
Doc 77 section 9 ("a world's machine can't serve pages into the client") is restated as "only code the user signed
runs in the client"; a world's machine may carry that code, through a pinned key after an Editor sign-in, in a window
with no shell access, with the CSP the shell sets. Section 15 records it.

**The fallback, shape (b), bundled in the client:** `Editor/` built into `Client/dist/manage/`, a window at
`WebviewUrl::App("manage/")` with its own capability granting only `world_manage(method, path, body)`, which sends
through the same pinned session. The page needs a transport chosen at start at its three fetch sites (as
`CentralTransport`/`ShellCentral` in `net/central.ts`); the helper accepts the current and the previous `editorApi`;
every DM change needs a client release. About 4-5 plan hours more than the signed bundle. D1 takes it if the scheme
can't carry the studio.

**Either way**
- Everything a world sends is shown as text. A test bans `innerHTML`, `dangerouslySetInnerHTML`, data-derived `href`,
  `src` and `style`, and `window.open` in hosted builds of `Editor/src`, **and in `Client/src/ui/frontDoor.ts`'s
  Editor screens** (`ServerTab.tsx:84` and `Dialogs.tsx:217` are hidden in hosted builds).
- `location.reload`/`assign` (`studio/Studio.tsx:45, 73, 80`, `dm/ServerTab.tsx:269`) become in-app reloads.
- Portraits bundled (or drawn as `ui/wolfArt.ts` does); Play hidden.
- In hosted mode the page skips `sessionInfo()` and gets its world from `/api/studio/session`; Atlas's writes carry
  no `X-RATW-Editor`, because the hosted `editing()` takes the session the shell adds instead (section 7).
- The main window's `central_call` allowlist (`central.rs:48`) gains nothing, and its CSP gains nothing: Editor
  traffic goes through commands, never through a fetch from a page.

### 6.7 Official worlds stay locked

- **The package can't run an official world.** `server_command` never passes `--official`, and the package holds no
  credential that writes shared progress (doc 77, section 6). The official world runs on the user's box from his own
  launchers, and **that box publishes neither 8765 (the studio) nor 17789**.
- **The entrypoint doesn't make an owner, a certificate or start the helper** if the shared-progress credential is
  present, or if central has ever named the world official. The agent writes central's `kind` and the host id into a
  directory only `ratw` can write (not the shared agent directory, which the server's user can write, so a
  compromised server could otherwise mark the world official and lock the Editor for good). Root mirrors from there
  into the `admin` volume, records "ever official" there once, and never clears it on a later `host.json`.
- **The helper refuses every sign-in** while the record says official, and `/editor/hello` says `kind: "official"`,
  which the client shows as "Official worlds can't be edited."
- **The client refuses** EDITOR for an address central lists as official, when it has the listing, before
  connecting.
- **The server ignores `host.json` on official worlds** (`RatwGameTickets.cpp:695-700`).

The user's own DM tool for the official world stays the local studio, unchanged except that it binds 127.0.0.1 by
default (Q25). Later, the user will want remote DM for the official box behind Caddy (doc 77, Phase 7). Nothing here
rules that out: the same Editor flow with a real certificate and operator accounts could be added then. v1 doesn't
build it.

## 7. The hosted studio

The helper runs the studio in a new **hosted mode**. What changes:

**One world.** `targets()`, `dm_worlds()`, `atlas_key` and `live_key()` all return `world`. No dev-world requirement.

**The Host check.** `trusted()` would refuse every request through the helper, because `Host` is the world's address
(section 4). Hosted mode replaces it in all three handlers (`dungeon_master.py:2068`, `studio.py:1277`,
`map_editor.py:1487`) and in Atlas's `editing()` (`map_editor.py:1566`) with the I9 rule and the session.

**Routes: deny by default.** An explicit list of allowed (method, path) pairs, each with the lowest role that may use
it, a new constant beside `dm_offers` (`studio.py:1043`) and `MOVED_TO_ATLAS` (`:93`). A test walks every route the
studio serves and fails if any is neither allowed nor denied by name. Each of the 100-plus routes needs a decision, so
this is its own phase (M3).

**Removed in hosted mode:**
- server switch, import, import plan, releases, rollback, `server/start` (`live.sh`);
- `/api/studio/atlas-world`, `/api/studio/dm-world` (they write `worlds.local.json`) and `lan_names` (`hostname -I`);
- Atlas's Update from live, Play and playtests (`play.sh`), and exports to files;
- whole-world project save and import (256 MB bodies, `map_editor.py:49`);
- `/api/dm/login` (the Editor signs in through `/editor/login`);
- `/api/atlas/session`: the hosted `editing()` accepts the session in place of Atlas's editor token, and the page in
  hosted mode skips `sessionInfo()` and takes its world from `/api/studio/session`.

**Kept, through the helper.** `/api/dm/server/status` stays. `/maintenance` (Close and Open) stops going through
`studio.sync` as publisher: a migration grants `UPDATE (state, changed_by, changed_at, note) ON admin.server_state TO
ratw_dm`, narrower than handing the helper the publisher password, which can also write builds and run the push. That
grant is for Close and Open only. `/rebuild` (Publish's build step) already connects as editor
(`studio.builds.rebuild`, `studio.py:1095`), which a live world can't (section 4), so it is slice 2's, once the world
is a dev world. Publish reuses `Builds.rebuild`'s steps in a child process (section 10.3). Restart is a request file
(section 6.5).

**Not in the studio:** the Editor accounts routes are the helper's own (`/editor/accounts...`, section 6.3), reached
only from screen 5 with a step-up, never from the Editor window.

**What My world shows** (section 2's table): the settings view is an allowlist of fields from the entrypoint's cleaned
copy (`player_limit`, `cheats`, `workers`, `[editor] enabled`), never the host token or the AI key. The folder's path,
the "keeps stopping" log lines (with addresses and tokens removed) and other accounts' sign-ins are owner-only.

**Players' wolves.** `character.kill`, `character.injury`, `character.move` and `character.resurrect` on a player's
wolf need the owner, or an account the owner marked "may act on players' wolves" (Q24). The allowlist checks the flag.

**The DM tool on central ids** (a doc 77 Phase 7 item, needed here). In ticket mode accounts are pairwise ids. The
Players, Reports and chronicle views assume usernames; they show wolf names and short pairwise ids instead.

**The model, on the host's key.** `RATW_AI_CONFIG` (read by `dungeon_master.py:1276` and `map_editor.py:1473`) points
at the helper's own copy in the `admin` volume.
- `write_story`: the existing 30 an hour (`dungeon_master.py:195`), plus 100 a day.
- Atlas's roster generation: 8 a call today and no hourly cap; add 20 a day.
- A monthly token budget in `world.toml`'s `[ai]`. The helper counts the tools' own calls in `dm.ai_use`, which
  `ratw_game` can't touch. The Mind's calls come from its ledger, which moves out of `LOGS` (where the server's user
  can rewrite it) into a directory only `ratw` can write; the root entrypoint copies a monthly count from it into the
  `admin` volume. M3 checks nothing else reads the ledger where it is now, and sets the default from its real costs.
- No route returns or edits the AI config, so a stolen session can't point the key at another endpoint.
- With no key, life stories and roster generation are greyed out: "Needs an AI key in `world.toml`." With a key, the
  button shows the day's count against the caps and a rough cost a story.

**Database roles:** `dm` and `editor` only. The server keeps `game`.

**Fixed in every mode, not only hosted:**
- the big-world playtest path builds `PLAYTESTS / (id + '-' + stamp)` from an unchecked `project.id`
  (`map_editor.py:973-978, 1632-1641`). That is a path traversal. Validate the id against its pattern before the
  folder is made;
- the studio binds `127.0.0.1` by default instead of `0.0.0.0` (`studio.py:1674-1681`, over plain HTTP and guarded
  only by a forgeable header check); the user's launchers pass `--host 0.0.0.0` where he wants it on his LAN (Q25);
- the I14 grep test over the three files' SQL.

**The legacy Storykeeper** (`tools/dm_service.py`, `DM/`). The brief named it as part of the DM tool, but it is the
pre-doc-34 planner whose work now runs inside the server. It is in the image already (stage.sh copies every tool) and
nothing starts it. Recommended: leave it unstarted, and drop `--dm-directory` from the package's server command, since
nothing in the container reads what the server writes there (Q18).

## 8. What hosts get, and what stays off

| Feature | Community host | Enforced by |
|---|---|---|
| LIVE map and watching; NPC moves, visitors, rumours, fame, projects, deeds, nicknames, notices, estates | Yes (dm and up) | allowlist |
| Festivals, bandits, Wharf credit (within `wharfActionMost`) | Yes (dm and up) | allowlist |
| NPC Life: kill, revive, protect, move (through `dm.actions`, with the `live.npc_state` row that survives a restart) | Yes (dm and up) | allowlist |
| Chapters: treaties and House requests | Yes (dm and up) | allowlist |
| Money, calendar, Economy: edicts, steering, rules, store, seasons, farms (docs 74, 75) | Yes (dm and up). The controller's floors are the safety net. | allowlist |
| Server Health | Yes (everyone) | allowlist |
| Storytellers, tales, storylines, milestones | Yes (dm and up) | allowlist |
| Reports: decide, with evidence | Yes, filtered; never reports about the host's own wolves (section 9) | hosted filter |
| Artwork review | Not needed: portrait uploads are off on community worlds | allowlist |
| Life stories and roster generation | Yes, on the host's key, capped | helper |
| Atlas layers: NPCs, routes, areas, spawns, factions, talk tables | Yes, live (slice 2; builder and up) | allowlist |
| Atlas map: terrain, cells, interiors, doors, places, resources, roster | Yes, after Publish (slice 2; builder and up) | allowlist and the editor role |
| Close and open the world; restart; Publish; backups | Yes (My world; owner; Publish builder and up, at most 6 an hour) | helper; restart and backups through request files to root |
| Editor accounts and sessions | Yes (owner, on screen 5, with the password again); owners only through `ratw-editor` | helper |
| `character.kill`, `character.injury`, `character.move`, `character.resurrect` on players' wolves | The owner, and accounts the owner marks "may act on players' wolves" (Q24): storytelling and moderation, world-local. A confirmation names the wolf (kill needs the name typed), and the player is told in game ("The host moved you"). | allowlist and the account's flag; the server's notice |
| Mentor and tie actions on players' wolves | Yes (dm and up), with the same confirmation and notice | allowlist; the server's notice |
| Rewards on the host's **own** wolves (`character.resurrect`, `deed.award`, `milestone.credit`, `estate.set`, `wharf.credit`, `storyline.give`, `mentor.restore`) | Only with cheats on | **the server:** refused when the target's account is `host.json`'s host and cheats are off |
| `character.gift`, `account.unlock` | Only with `cheats = "host"` or `"everyone"` | **the server** |
| `character.dm` (the full Dev Console) | **Never.** It needs rank admin, and no hosted account can be admin. | the DM tool's ranks; the server ignores the flag on community worlds without `--dev-tools` |
| A username lookup from central | **Never.** It would tie wolves to accounts. | central |
| Switch, import, rollback, `server/start`, world pickers, playtests, file exports | No | allowlist; code removed in hosted mode |
| Restore a backup; delete the world | No: the scripts in the world folder (I7) | not routed |

Rewards to "the host's own wolves" mean the wolves of the central account in `host.json`, whichever Editor account
acts. A co-builder's own wolves aren't known to the world as theirs (Editor accounts aren't central accounts), which
the guide states: on a world with cheats off, the owner trusts co-builders not to reward their own wolves.

**Cheats, three settings** in `world.toml` (Q7), each listed "cheats on" except `off`:
- `cheats = "off"` (the default): no cheat kinds; the `dungeonMaster` flag is ignored and cleared on load, so a flag
  granted earlier does nothing; the host's rewards to their own wolves are refused.
- `cheats = "host"`: a new narrow server flag. The DM tool's `character.gift` and `account.unlock` work, and the host
  can reward their own wolves. Players get no socket cheats.
- `cheats = "everyone"`: `--dev-tools`, as on DEV. **Every player** can give themselves Gifts and goods through the
  socket messages listed in section 4. The guide says so in those words.

Disabled cheat actions show in the tool with the reason ("Set `cheats` in `world.toml`; the world will be listed
'cheats on'"), not as errors. A host can always run a modified image, so "cheats on" only keeps honest hosts honest,
and the hosting guide says so. What is guaranteed: nothing on a community world reaches central or official progress.

**To check (M0a):** whether a ticket's `gift` claim overwrites a Gift the world set, on the wolf's next `hello`. If it
does, `character.gift` is pointless on community worlds and the table should say so.

## 9. Other players' privacy

**What players are told (the honest part).** The host owns the database and the socket. Filters can't change that.
- The privacy page and the community join screen say: "The host runs this world. They can read everything you say and
  do there, and they see your internet address." Co-builders the host adds can see what the host's tools show.
- They also say wolves are recognisable on every world: wolf names are unique across central. Pairwise ids keep the
  host from learning which account a wolf belongs to, or which other wolves share it.
- The client never asks for the central password inside a game, and the guide warns players about NPCs (which hosts
  now author) that ask for passwords.
- "Voices use AI" on the listing becomes "AI used", covering the host's tools too.

**What the filters are for.** Hosted mode filters by default (Q8). They make harvesting inconvenient and limit what a
stolen session, a co-builder or an injected page can see. They are not a protection against the host.
- **Accounts** appear only as short pairwise ids and wolf names.
- **Reports:** the reported wolf, the reason and the evidence lines (the lines the reporter received from that wolf,
  `Core/RatwGameSafety.cpp:222-227`), marked "supplied by the world". **The reporter is hidden from the host** by
  default, against retaliation. Reports about the host's own wolves never appear in the Editor.
- **Chronicles and NPC conversations** (`dungeon_master.py:1233-1253`): the NPC's side and summaries only, never the
  player's typed words.
- **Circles:** members only (already "never their chat", `:358`).
- **No players' IP addresses** appear in the tools. (Editor accounts' sign-in addresses do, to the owner: section 3.5.)

**Reports (Q10).** Doc 77 section 3.2 says reports on community worlds go to central (`POST /reports`, Phase 7, not
built). The game tells reporters "Reported. The host will look at it." (`RatwGameSafety.cpp:235`). Recommended:
- The host decides reports about other wolves on their world.
- A report about the host's own wolves goes to central **from the reporter's client**, signed with the reporter's
  central session, so the host can neither drop nor forge it. Central marks world-supplied evidence as untrusted.
- Until Phase 7 builds central's route, such a report is kept on the reporter's client and the reply says "Central
  can't take reports yet; this one is kept until it can." The reply never promises more than happens.

**Backups and deletion.** Deleted wolves stay in daily dumps for up to 7 days and in pinned dumps for up to 90 days
(section 12.1). The privacy page says so. Deleting a wolf inside a world doesn't exist yet (doc 77, Phase 7); when it
does, a restore re-applies every deletion made since the dump, from a dated list the agent keeps.

**For the host terms (doc 77, Phase 7):** the host is responsible for their world's data, and for the co-builders they
add; the tools send story inputs (a chronicle and a description) to the host's AI provider; deleted wolves must also
be forgotten in `dm.stories` and `dm.audit`.

## 10. Map editing (slice 2)

### 10.1 What ships

Every package carries the `world_sync.AUTHORED` tables as Atlas holds them:
- `world.cells`, `terrain_chunks`, `interiors`, `links`, `resources`, `places`, `chapters`, `minimaps`;
- the full `world.areas` rows (today only id and kind ship);
- `world.worlds` as it is (revision, chunk size, bounds, spawn).

Rules:
- They go through the same strip and scan as the rest (`wolf-`, `player-`, `acct-`, `dev:`, usernames). Players'
  changes live in `live.game_changes`, which stays out, so the source should scan clean; a test proves it.
- **The scan also covers every author column** (`world.builds.created_by`, migration 0010; `live.factions.updated_by`,
  0016; and the rest) and checks for names from `dm.admins`, `world.editors` and `Database/dm-accounts.txt`.
- **Unreleased content** (hidden or unfinished places) has to be removed before export. The export lists places no
  route or door reaches, for the user to check.
- `world.edits` and `world.editors` start empty. `admin.*` stays out; the map stamp falls back to
  `original:<world id>`, harmless with no sync.
- The package still ships a ready build, so a first start doesn't pay for one.
- At first import the world keeps a copy of its original `live.economy`, so "Reset economy rules" has something fixed
  to go back to after updates (section 12.3).

**Why every package, not an "editable" one:** a host who never edits still needs the source if a later server can't
load their old build (section 12.5).

**Size, not measured.** `terrain_chunks` alone was 964 rows and 65 MB at doc 70's revision, 56 MB of it heights as
jsonb. Guess: the package grows from about 24 MB to 40-70 MB compressed. M0b measures it; doc 70's dense heights
encoding (doc 70, lines 276-279) is the fix if heights dominate.

**Imports are untrusted.** Hosts will pass worlds around. Import keeps to parameterised SQL and in-memory extraction
(as today), and adds column allowlists for the AUTHORED tables, size caps, and the Atlas validator with the client
limits (section 10.3) before anything is written. No SQL text is ever read from a package.

### 10.2 World kind and roles

The hosted world becomes **a dev world served as live** in slice 2: doc 66's model, which is how DEV runs on this
laptop today. Slice 1 leaves it live.
- `world_package.py`'s five hardcoded `live`s change (`_write_registry`, `register` and its refusal, `new_world`,
  `grant_connect`). The entrypoint's registry, and the helper's copy, say `'kind': 'dev'`.
- **`--world-kind live` stays.** The server's kind switches only the `/store` console, which is for dev worlds only
  (`RatwGameDev.cpp:232`). Following the registry would turn it on.
- `CONNECT_ROLES['dev']` already includes `editor`. The `ratw_editor` CONNECT grant moves into `W.ensure_roles`, which
  runs as the superuser on every start, so existing volumes get it too (today's `grant_connect` runs only on a first
  import). `world_db.grant_connect` (committed in 029d050) is what this builds on. The helper gets the editor
  password from here on.
- On a dev world the economy writes take the editor role through `live_edit`, not a direct upsert as dm. Hosted mode
  keeps that path; the allowlist covers both.
- `world_build` already accepts dev worlds.

The allowlist, not the kind, is the boundary. Dev-only flows (pull, `new --from`, `sync_base`) are simply not routed.

### 10.3 Publish

**"N map changes not published."** A migration adds `edits_through` (the newest `world.edits.id` at build time) to
`world.builds`. The count is the `world.edits` rows with map keys (`cell:`, `room:`, terrain and the other AUTHORED
keys) after it. Revision and fingerprint are both wrong for this: every NPC, layer and economy edit bumps the
revision, and the fingerprint also hashes the tools' source and the live layers.

**Shown before the host starts.** The Map tab and My world say up front either "This computer can publish map changes"
or "This computer can't publish yet: it needs N GB, Docker has M", with the fix (below). Nobody spends hours in Atlas
to learn that at the end.

**The Publish button** (builder and up):
1. **Validate.** The Atlas validator (about 11 s at doc 70's size); limits that match the client's (sizes, counts,
   string lengths, Tracery depth and expansion size); `world_sync.orphans` (`world_sync.py:317-344`) and
   `SET CONSTRAINTS ALL IMMEDIATE`, which the laptop's import path runs and hosted Atlas would skip; the `live.*`
   references against the new build (homes, workplaces, spawns, routes, economy places); and the game state in the
   checkpoint (leased flats, stalls, homes, items on the ground, wolves inside a removed interior or cell). Each error
   is in plain words with a "Show me" link that jumps to the spot in Atlas.
2. **Back up.** If no backup can be made (an old `compose.yaml` with no backup service, a full disk), Publish refuses.
3. **Check memory:** the cgroup's `memory.max` and `memory.current` and `MemAvailable`. Below N GB, refuse with
   "Publishing needs about N GB free; this world has M." N is set in M0b.
4. **Build in a child process** with `RLIMIT_AS`, reusing `Builds.rebuild`'s steps, so a failed build dies alone
   instead of the kernel killing `ratw_server`. Killed after a time limit M0b sets on modest hardware (4 cores,
   8 GB, a slow disk), not this laptop. Progress ticks come from `export_files` (new; today it is one long call).
5. `world_build.build(editor_conn, created_by=<the Editor account>)` stores the build and sends `ratw_release`.
6. **The host chooses how the world moves onto it:**
   - **Open only to me** (recommended for a first Publish): a new `host_only` server state. The server restarts onto
     the new map and admits only tickets whose sub is the host's. Two buttons follow: "Open to everyone" and "Go back
     to the previous map". Ticket-mode worlds only, since a password-mode world names no host. It also needs a
     valid host token: `host.json`'s host is nulled on `bad_token`, `delisted` or `banned` (`world_agent.py:420`),
     and then this choice is greyed with the reason. Everything else in the Editor works without a token.
   - **Now, with a minute's warning:** maintenance (exit 76), and the helper sets the state back to running.
   - **When everyone has left:** today's behaviour; queue nothing.

The `host_only` state needs a migration (the CHECK on `admin.server_state.state`) and a small Core change at admission
(M1). The entrypoint's 76 wait drops from a minute to a 5 s poll (as `live.sh` does with `RATW_MAINTENANCE_POLL`), so
Close and Open feel immediate.

**Live edits can't run ahead of the map.** A Residents edit (an NPC, route, spawn or home) that names an interior,
place or cell the running build lacks is refused: "Publish the map first." The reverse (Publish removing something
live rows use) is caught by step 1.

**The memory guide.** Measured 3.9 GB at a third of today's build size. Docker Desktop's WSL2 VM gets half the PC's
RAM by default, so a 16 GB PC has 8 GB for the server, Postgres, the save's fork and a build. Docker Desktop's
Resources slider doesn't set memory on the WSL2 backend; the guide gives the real steps: make
`%UserProfile%\.wslconfig` holding `[wsl2]` and `memory=12GB`, run `wsl --shutdown`, restart Docker Desktop. It says
"2 GB to play; N GB to Publish". Building in a separate container started on demand is Q12's alternative.

### 10.4 Map edits never take a world off the list

The client-data hash covers only the terrain catalogue, the walking rules and the packed formats
(`CMakeLists.txt:12-13`, `RATW_CLIENT_DATA_FILES`), not the map. Atlas paints only catalogue terrain, and
`Data/Terrain` is read-only in the image. So an edited world stays listed and players' walking matches. **Hosts can't
add terrain kinds**, and the guide says so. M0b checks that Atlas can't write a ground kind the catalogue lacks.

### 10.5 Sharing a world

Hosts will pass worlds around. A zipped world folder leaks the host token, `first-editor.txt` if unused, and every
player's data (and the Editor accounts' password hashes) in `backups/`. So My world gets **"Share my world"** (owner):
a `share` request file (section 6.5), since `world_package export` connects as `owner` (`world_package.py:1044`),
which the helper doesn't hold. The root entrypoint runs the export into `manage/shared/`, which strips players, the
token and `dm.*`, and runs the same scan. The certificate's key is never in the folder. The guide says never to zip
the world folder.

## 11. When each change takes effect

In slice 2 every Save button in the Editor window carries a badge: **Live now** or **Waits for Publish**. Nothing a
host can touch is "restart only".

| Takes effect | What | Path |
|---|---|---|
| Live now (about 1 s) | Everything through `dm.actions`: NPCs, routes, areas, spawns, factions and claims, economy and steering, talk tables, visitors, festivals, storylines, character actions, decisions | `live.*` write, `*.sync`, `applyDmActions`. Receipts in `dm.actions.status`, polled by the page. Refused if it names an unpublished map item. |
| At once | Editor accounts, roles, ending sessions | `dm.*` rows the helper reads on every request |
| After Publish | Terrain, cells, interiors, doors, places, resources, roster and profession-slot fills | Publish (10.3) |
| On the next server start (My world's Restart) | Server settings: `player_limit`, `cheats`, `workers`; `[editor]` | `world.toml`, edited on the host's disk. The entrypoint re-reads `[world]`, `[server]` and `[editor]` before every server start, so Restart applies them; it writes the helper's cleaned copy then, and restarts the helper when `[editor]` changed. My world shows the allowed settings read-only, with "`world.toml` changed since start: Restart to apply". |
| On `docker compose up -d` | `.env` (ports, `RATW_EDITOR_BIND`, the token file, the image) | `restart` doesn't re-read `.env`; the README's "every day" section changes to say so. |
| Never through the Editor | `Data/` catalogues: items, Talk, Voice, Terrain | Read-only in the image; they change only with the package |

## 12. Backups, undo, updates and recovery

### 12.1 Backups

Today nothing can roll a world back: `game.save_checkpoint` overwrites the one checkpoint row per world
(`RatwDbStore.cpp:234-236`).
- **A `backup` compose service** on the `postgres:18` image already pulled (its `pg_dump` matches the server; trixie's
  client is 17 and can't dump 18). It runs `pg_dump -Fc` as `ratw_owner`, owns `./backups`, and makes the folder if
  it is missing.
- **Build cells once each, not every day** (Q15). `world.build_cells` is most of the database (three builds of about
  307 MB). Builds never change, so each build's cells are dumped once, to `backups/build-<id>.dump`, when the build is
  made; daily dumps skip `world.build_cells`' data. A restore loads the daily dump and its build's file. M0a measures
  real sizes.
- **When:** on start and then every 24 hours whenever the last dump is over 24 hours old (a home PC that is off at a
  fixed hour would otherwise never get one); before every Publish; and before migrations whenever the package version
  changed (the entrypoint records the version it last ran in the database).
- **Asked for by the helper** through a request file (section 6.5) that carries only a kind ("before-publish", "by
  hand"), never a path or a name. The root entrypoint passes it to the backup container through the small `requests`
  volume both services mount.
- **Kept:** 7 dailies, the last 5 automatic ones, and up to 5 the owner pins, each pin for at most 90 days. Build files
  are kept while a kept dump needs them.
- **Disk:** free space is checked before each dump, and a dump that would leave under 2 GB free is refused with a
  plain message. The status pill shows "Last backup 3 h ago" and the folder's disk use.
- Each dump gets a manifest: the world id, the package version, the migration level, the build id and its SHA-256.
- **The dumps hold players' data and the Editor accounts' password hashes.** On an NTFS mount mode 0600 means nothing:
  anyone signed in to that Windows account can read them. The guide warns against keeping the world folder under
  OneDrive or another synced folder, and My world shows the owner the folder's path so they can check. The host terms
  cover the rest.

### 12.2 Restore

Restore is never reachable through the Editor port (I7), so a stolen Editor session can't wipe a world. But hosts
don't type `docker compose run` commands in a crisis, so:
- **`restore.bat` and `restore.sh`** ship in the world folder. They list the dumps (date, package version, why it was
  made), ask which one, say plainly "This puts your world back to <date>. Everything players did since then is lost",
  and then stop the world, restore and start it.
- **Restore refuses** while the world is running (it checks for live `ratw_game` connections and prints the stop
  command); a dump from another world (the manifest's world id and SHA-256); and a dump from a newer package or a
  newer migration level (a downgrade).
- **It takes a safety dump first.**
- **It keeps today's Editor accounts by default.** The script asks: "Keep your Editor accounts as they are now
  (recommended), or take the ones in the backup?" Keeping them means a removed co-builder, an old password or a
  forgotten device doesn't come back. Either way it deletes every `dm.sessions` row and prints which accounts the world
  now has.
- A dump from before an update is migrated forward when the world next starts.
- My world's backup list has **"Copy restore steps"** for each dump, in PowerShell and in bash.
- Restore from inside the Editor, behind the owner's password asked again and a safety dump, is Q14's option (b).

### 12.3 Undo

- **Map, within a session:** Atlas's undo already sends inverse edits (`Editor/src/lib/store.ts`).
- **Discard unpublished map changes** (slice 2, not optional): rewinds the map source to the newest build's
  `edits_through`, from the before-values in `world.edits`, refusing keys that changed since. Without it, "back to the
  previous map" leaves the bad edit in the source and the next Publish brings it back.
- **Back to the previous map:** copy build N-1's manifest and cells as a new build id (the server takes only newer
  builds), then restart onto it. It offers to discard the unpublished changes with it, and runs Publish's step 1
  checks the other way (NPCs and leases placed in interiors the old map lacks).
- **DM changes:** show the opposite action where one exists (kill and revive, protect and unprotect, hold and release,
  post and cancel a project). Each confirmation says whether there is one. `character.resurrect` exists, but a
  player's lost time doesn't come back.
- **Economy:** **Reset economy rules** goes back to the copy of `live.economy` made at first import, not to whatever
  package the image holds now. The doc 75 ledger shows what changed.
- **History:** the My world tab lists `dm.audit`, with the Editor account that made each change.
- **Cross-session Atlas undo** ("undo my last batch" from another day) is slice 3 (Q20). Discard covers the case that
  traps hosts.

### 12.4 When the server won't start

- **The entrypoint keeps the helper up on any server exit** and waits, instead of exiting the container. Restarts back
  off: 5 s, 30 s, 2 minutes, then every 10 minutes.
- **A new map that won't start falls back by itself.** If the server fails to start twice within about 5 minutes of a
  new build, the entrypoint (through the helper's code, as the editor role) publishes build N-1 as a new id and starts
  the server on it. The Editor then says "Your last map didn't start; the world went back to the previous map. Your
  edits are kept." The failing build stays for the bug report.
- **Any other crash loop** shows on the Editor menu and in My world as "The world keeps stopping (exit N)", with
  "Copy restore steps". The owner also sees the last lines of the server's log, with addresses and tokens removed.

### 12.5 Package updates against host edits

Phase 5's rule holds: an update keeps the world, and a newer package's world is never applied to an existing one
(import does nothing on an existing database). So host edits survive updates, and so do Editor accounts and the
certificate.
- **Migrations change schema only and never rewrite authored content** on a community world. A test imports a package,
  edits a cell, runs the migrations and checks the cell.
- **New seed rows (Q17).** New features sometimes need new rows (`live.economy` keys, the voice and talk tables,
  `live.business_settings`). Recommended: an add-only seed step on update that inserts missing keys and never touches
  an existing one, the same rule as Atlas's add-only NPC push. It is not a migration: it writes only rows that don't
  exist.
- **A build-format number** goes into the build manifest and the server's loader (none exists today, by grep). The
  server reads the current format **and the previous one**, so a format bump doesn't force a rebuild. A rebuild is the
  last resort.
- **When a rebuild is unavoidable,** the entrypoint runs the memory check **before** migrating. If memory is short it
  says so plainly in `/editor/hello` and stops, rather than restarting in a loop. If there are unpublished changes, it
  doesn't rebuild on its own: the Editor asks "This update needs your map rebuilt. Your unpublished changes would go
  live: rebuild with them, or discard them first?" Progress shows in `/editor/hello` and the status pill.
- If the host's source uses a terrain kind a later catalogue dropped, the rebuild fails before it starts, with the
  cells named.
- **Official map changes don't reach hosted worlds.** A host who wants the new map starts a second world folder
  (`COMPOSE_PROJECT_NAME`) or a fresh one after a backup. A three-way pull with the package as upstream (`world_sync`,
  doc 66 section 4) is later.

### 12.6 Something went wrong

The guide and My world's help carry this table.

| What went wrong | What fixes it | What it costs |
|---|---|---|
| A new map looks wrong | Back to the previous map; then Discard unpublished map changes | Nothing players did |
| A new map won't start | Nothing: the world goes back by itself (12.4) | Nothing players did |
| The economy went bad | Reset economy rules; the controller's floors meanwhile | The rules you changed |
| An NPC action was a mistake | Its opposite action, where one exists | Nothing |
| A player's wolf was hurt by mistake | `character.resurrect` or moving it back | Their lost time |
| Everything is broken | `restore.bat` or `restore.sh` with a dump | **Everything every player did since that dump** |
| The Editor can't connect | Screen 3's messages, in order; section 3.3 for forwarding | Nothing |
| "The certificate has changed" | Don't sign in. If you made a new world, lost the world's volumes or ran `new-certificate --leaked` yourself, compare `manage/editor-certificate.txt`, then forget the old one, or paste a new Editor code | Nothing |
| A lost password, or "too many tries" | Double-click `reset-editor-password` in the world folder on the world's computer; or the owner resets a co-builder on screen 5 | Nothing |
| The owner on a new computer can't get in | `ratw-editor invite NAME` (or the reset script) for an Editor code | Nothing |
| Someone you don't know signed in | Screen 5: End all Editor sessions, then reset or remove the account; check the account banners; `ratw-editor new-certificate --leaked` if the world folder leaked | Nothing |
| `docker compose up -d` says "ports are not available" | `RATW_EDITOR_PORT=` another port in `.env`, then `up -d` (section 3.2) | Nothing |

## 13. Not in v1

The user's words are "edit and run their own world". Hosts may expect more than v1 gives. Said once, with reasons:
- **Items, Talk and Voice catalogues, and terrain kinds** (`Data/`): read-only, because the client-data hash covers
  them and players' clients would stop matching.
- **New terrain kinds:** the same reason.
- **Official map updates merged into an edited world:** a three-way merge is later (12.5).
- **Editor sign-in with central accounts:** not planned; the user chose the world's own accounts.
- **The Editor in a web browser:** never; only the desktop client can pin the certificate.
- **Restore from inside the Editor:** the scripts instead (Q14).
- **Cross-session Atlas undo:** slice 3.
- **Two-factor sign-in for Editor accounts:** later, if hosts ask; device tokens, Editor codes, the limits and
  `new_devices = "approve"` cover v1.
- **The Editor on macOS:** hidden until it has been tried on WKWebView (Q26).
- **"Stay signed in on this computer":** not in v1 (Q22).
- **A check of the Editor port from outside,** as central checks the game port: not planned, so central never learns
  where a world's Editor listens. The guide's phone-hotspot test does the job.
- **The helper in a container of its own** (so a port collision would break only the Editor): not in v1. Compose
  can't publish one of a container's ports from a profile, and a separate container needs shared volumes for every
  request; the README's collision text covers it.
- **The full Dev Console (`character.dm`)** for hosts: never; `cheats = "everyone"` is the sandbox instead.
- **A DM suite on official worlds:** never through this package.

## 14. Phases

**Gates before M0:**
- doc 77's Phase 5 is finished: its end-to-end run of the bundle, its review, and the merge. Until then two sessions
  would be editing `world_package.py` and `Package/` at once;
- the desktop client has been tried on Windows (doc 76's open Windows item). The Editor is desktop-only and the
  target host is on Windows, so M6's and M10's Windows runs depend on it. That run is user-bound.

"Plan" is the careful estimate. "Expected" is about 0.3 times it for familiar work, from doc 77's record (Phase 0: 3-4
h planned, about 25 minutes taken; Phase 1: 8-10 h, about 2 h; Phase 2: 10-13 h, about 3 h; Phase 3: 12-15 h, about
3½ h). Those phases were mostly familiar Python. **M2a, M3 and M4 use about 0.45 instead:** a new TLS server with
bounded pools and lanes, a custom rustls verifier inside ureq's low-level API with a URI-scheme proxy and signed
bundles, and 100-plus route decisions with privacy filters. Reviews are counted at face value: doc 77's found 25-30
problems each.

**Slice 1: Run my world**

| Phase | Work | Plan | Expected |
|---|---|---|---|
| M0a. Measure (read-only) | The `editor` URI scheme proxying the built studio app on WebKitGTK and WebView2 (16 MB POST bodies with `Content-Length`, 32 MB raw and gzipped replies, `webview_label()` in the handler, incognito windows, `on_download`); ureq's `Agent::with_parts` with a pinned TLS 1.3 rustls verifier against a self-signed P-256 server; whether the container sees real peer addresses under Docker Engine and Docker Desktop; whether a `0.0.0.0` publish also answers on IPv6; Hyper-V and WSL reserved port ranges against 17789 (`netsh int ipv4 show excludedportrange`); `chown` and `chmod` from the root entrypoint into a Windows-folder bind mount on Docker Desktop; the studio starting with no `Saved/` or `artifacts/`; a dump's real size, with and without `build_cells`; the largest DM and Atlas reply (`/api/live/world`, `map_editor.py:1546`, is likely largest) gzipped and not; a ticket's `gift` claim against a world-set Gift. Ends with D1 | 1.5-2 | 0.5 |
| M1. Guards in the server | The three `cheats` settings and the narrow flag; `dungeonMaster` ignored and cleared on community worlds without `--dev-tools`; host's own-wolf rewards refused with cheats off; the in-game notice to a player when the host acts on their wolf; the `admin.server_state` UPDATE grant to `ratw_dm` (migration); `--dm-directory` off the package; the agent writing `kind` and the host id into a `ratw`-only directory; the playtest path fix; the studio's 127.0.0.1 default (if Q25 says yes); ctest | 2-3 | 1 |
| M2a. The helper's security core | `world_admin.py`: TLS 1.3 with the world's key and reload on a signal; hello (with progress, `setupPending`, rotation) from before the import; `/editor/login`, password, step-up, keep-alive, logout and the account routes over the DM tool's accounts in the world's database; the migration (roles, `must_change` and its expiry, the wolves flag, `last_seen`, `dm.devices`, `dm.invites`, `dm.locks`, `dm.ai_use`, the revokes and the I11 test); Editor codes and invites; the breached-password list and the world-name rule; the browser rule; the two pools, per-address and world-wide connection limits, large-body cap, catch-all handlers; the scrypt cap and its priority, the dummy hash, device tokens scoped to their account and capped, the three lanes, per-address, per-account and world-wide failure counts, fixed-size counters, `new_devices = "approve"`; sessions (idle on input, caps that refuse rather than evict) and the audit mapping; folded log lines and audit rows; `ratw-editor` with `invite`, owner-only `add --role owner` and signed `new-certificate` | 6-8 | 3-3.5 |
| M2b. The entrypoint, compose and README | Entrypoint: `openssl` named in the Dockerfile, the `admin` volume, the certificate and fingerprint, the first owner (generated name, 7-day renewal) and `first-editor.txt`, emptied on use; user 10003 in no group; the helper's environment (registry, AI config, settings copy, `central.json`, ever-official); the request directory and its kinds (restart, backup, share, first-editor-used); the loop restructured to keep helpers up on any server exit, with backoff; `world.toml` re-read before each server start and the helper restarted on `[editor]` changes; the 5 s maintenance poll; the official refusal; the Mind's ledger moved. Compose: the Editor publish with `RATW_EDITOR_BIND` (default 127.0.0.1) and `RATW_EDITOR_PORT`, the `admin` and `requests` volumes, the `./manage` mount, log limits on every service. README: the first sign-in, opening the port, the VPS warning, the "ports are not available" fix, both ports in the second-world recipe; empty `manage/` and `backups/` in the zip | 3-4 | 1-1.5 |
| M3. The hosted studio (DM tool) | One world; the Host check replaced in all three handlers and `editing()`; the allowlist with each route's lowest role, and its every-route test; removed routes; Atlas needing builder and My world needing owner in hosted mode; admin refused; Close and Open through the new grant; the wolves flag; the DM tool on central ids; privacy filters; reports filtered and the reporter hidden; the settings allowlist and owner-only views; AI config, caps, `dm.ai_use`, the monthly budget and the no-key state; the SQL grep test; serving the page's files only with a session | 6.5-8 | 3-3.5 |
| M4. The client | The shell's `editor.rs`: the pinned TLS 1.3 verifier and its record mode, the pin store keyed by key with address aliases, signed rotation, device tokens in the keychain, `editor_connect` (with hello validation) / `use_code` / `trust` / `forget` / `sign_in` / `change_password` / `step_up` / `accounts` / `sign_out` / `open`, the input-gated keep-alive; the `editor` scheme proxy with its binding check, header rules, gzip, size caps, forced content types, CSP and the signed-bundle check; the incognito, per-session Editor window, its navigation case, `on_download`. The signed bundle in `Package/stage.sh` and the release key. The front door: EDITOR on the sign-in, roster and Host pages (with the 127.0.0.1 match); screens 2-5 and every message, the Editor code box, the accounts panel with step-up, banners and failed sign-ins; NEW TOKEN's dialog; EDITOR hidden on macOS. `Editor/` staged into the image; its hosted mode (no `SignIn`, no token, no `sessionInfo()`, the 401 message, in-app reloads); the widened text-only test. (Shape (b) instead: +4-5 plan, +2 expected) | 6.5-8 | 2.5-3.5 |
| R1. Review | An adversarial review of M1-M4, the security core (now facing the internet), and its fixes | 2.5-3 | 2-2.5 |
| M5. Host basics and backups | A bare My world tab (status pill, Close and Open, Restart, the settings allowlist, AI use, history, the fingerprint, owner-only views); confirmations naming the wolf; opposite actions; disabled-with-reason states; the backup service (on start and every 24 h, pre-migration, manifests, pins capped, free-disk check, build files, the `requests` volume); `restore.sh` and `restore.bat` with their refusals, the safety dump and the keep-accounts question; `reset-editor-password.bat` and `.sh`; "Copy restore steps" | 4.5-5.5 | 1.5-2 |
| M6. Guide and end to end | The hosting guide (the first sign-in with the Editor code, opening the port, at home and from outside, forwarding and the hotspot test, Private networks, the VPS warning, the fingerprint, co-builders and roles, Editor codes as secrets, `ratw-editor` and the reset script, port collisions, cheats in plain words, privacy duties, AI use, the "something went wrong" table, never zip the folder); the privacy page and join screen lines; host-terms additions for doc 77 Phase 7; a full run on this laptop (first sign-in from the code, a co-builder from a second client with a code, a live NPC edit, an economy lever, close and open, a signed rotation moving quietly, a changed key refused, a password spray that doesn't lock out a known device or a code, a backup and a restore keeping accounts, a server crash with the helper still up, an image update keeping the fingerprint, an existing doc 77 world updated to the Editor package, the second-world recipe with both ports); then the user's Windows PC, including from a second computer at home (user-bound) | 2.5-3.5 | 1-1.5 |
| **Slice 1** | | **about 35-45 h** | **about 15-20 h** |

**Slice 2: Edit my map**

| Phase | Work | Plan | Expected |
|---|---|---|---|
| M0b. Measure | Package size with the AUTHORED tables; Publish's RAM and whole time (backup, validate, build, restart) at today's build size on a throwaway copy (**user-bound: the user's go-ahead**), and on modest hardware; what the server does when the checkpoint names an interior, place or cell the new build lacks; that Atlas can't write a ground kind the catalogue lacks. Ends with D2 | 1.5-2 | 1 |
| M7. Map source in the package | Export, scan (author columns, the user's names) and import of the AUTHORED tables; import's allowlists and caps; the registry's `kind: dev` with `--world-kind live` (the five places); the editor CONNECT grant in `ensure_roles` and the editor password to the helper; the economy write path on dev; the original `live.economy` copy; the add-only seed step (if Q17 says yes); tests (scan clean; edit, build, NOTIFY, exit 75 in a real server) | 4-6 | 1.5-2 |
| M8. Atlas and Publish | ATLAS on the menu for builder and up; Atlas's routes on the allowlist; Atlas writes taking the session; `edits_through` and the unpublished count; the build-format number (current and previous read); Publish as a job (validation with client limits, orphans, constraints, live and checkpoint references; backup or refuse; memory check, shown up front; the child build; 6 an hour an account); the `host_only` state (migration and Core admission, greyed without a valid token); the three restart choices; the automatic fallback to the previous map; previous-map republish; Discard unpublished changes; live edits refused ahead of the map; Live now / Waits for Publish badges; the update rebuild's questions | 9.5-12.5 | 3.5-4 |
| M9. Share my world | The My world button, a `share` request, and the root entrypoint's `world_package export` into `manage/shared/` | 1-1.5 | 0.5 |
| R2. Review | An adversarial review of M7-M9 and its fixes | 1-2 | 1-2 |
| M10. Guide and end to end | The guide's map sections (Publish memory with the `.wslconfig` steps, preview, going back); a run on this laptop (a cell edit, a bad edit caught by validation, Publish open only to me, open to everyone, a build that crashes on load falling back, discard); then the Windows PC (user-bound) | 1.5-2 | 0.5-1 |
| **Slice 2** | | **about 19-26 h** | **about 8-11 h** |

**Both slices: about 54-71 plan hours, about 24-30 expected,** reviews included.

**What the reviews of the rewrite changed in the hours.** Slice 1 grows by about 8-10 plan hours (4-7 expected):
- M2 is split into M2a (the helper's security core) and M2b (entrypoint, compose, README), because it was too big to
  stop-and-ask on;
- new work: the Editor code and invites, signed bundles, the per-session window and its binding, signed key rotation,
  the key-indexed pin store, separate pools and lanes, the `admin` and `requests` volumes, request files to root, the
  reset scripts, one-time password expiry and renewal, the breached-password list, the step-up and the accounts panel
  on screen 5, owner-only views, the AI ledger's move, log limits, and the "still setting up" hello;
- the 0.45 factor on M2a, M3 and M4 (without it, slice 1 would read about 13-16 expected, and the stop-and-ask rule
  would fire on nearly every phase or be ignored).
Slice 2 grows by about half an hour, for Atlas's session and Publish's rate limit.

**Slice 3 (later, not planned in detail):** the host's-words layer (tabs regrouped, progress polish), 3-5 plan hours;
cross-session Atlas undo, 2-3.

If a phase runs past its **expected** hours, stop and ask (memory `session-budget`). Against the plan column the rule
would never trigger.

### Decision points

- **D1 (after M0a):** if the `editor` scheme can't carry the studio on either platform (bodies, raw replies, the
  label check, downloads), take shape (b) and add its hours. If ureq's `with_parts` can't take the pinned connector,
  use a small hyper-rustls client (+0.5 plan). If Docker Desktop hides peer addresses, the per-address limits fold
  into the world-wide ones: known devices, Editor codes and one-time accounts still get through, but a typed password
  from a new device can be held off by one stranger, so the guide recommends Editor codes for every first sign-in
  and not opening the port wider than needed. If 17789 falls in a common reserved range, pick another default.
- **D2 (after M0b):** if Publish at today's size needs more than about 6 GB, choose between stating the requirement, a
  leaner streaming build first, or a separate build container (Q12). If the Atlas source adds more than about 50 MB to
  the package, do dense heights first.
- **Between slices:** slice 1 ships and the user tries it before slice 2 starts (Q21).

## 15. What changes in docs 76 and 77

**Doc 77**
- **Headline**, the package bullet: the export also carries the map's source (Atlas tables), and hosts edit and run
  their world through the client's Editor (doc 78).
- **Section 3.2, point 4:** hosts moderate in game **and** run and edit their world from the Editor (doc 78), with
  co-builders if they want. Reports: Q10 here; the "reports go to central" line becomes "reports about the host go to
  central from the reporter's client".
- **Section 4 (central):** no new route. Editor accounts are the world's own and never reach central.
- **Section 5 (the ticket):** unchanged.
- **Section 6 (official worlds):** the package can't run one, and no helper, Editor port or Editor account exists on
  one.
- **Section 7 (the list):** "cheats on" now follows `world.toml`'s `cheats` (`host` or `everyone`), which the server
  enforces. "Voices use AI" becomes "AI used".
- **Section 8 (the package):**
  - strike "Out: Atlas source (`world.terrain_chunks`, cells, areas and the rest)"; the AUTHORED tables are in, through
    the same scan, widened to author columns;
  - strike "**No DM suite for hosts in v1.** …" and point to doc 78;
  - Ports: the game's 7788, and the Editor's 17789 (TLS only), published on 127.0.0.1 unless `RATW_EDITOR_BIND`
    opens it; the second-world recipe sets both;
  - Volumes: `admin` (the helper's key and state, kept across updates) and `requests` (shared with the backup
    service); log limits on every service;
  - the world folder gains writable `manage/` (`first-editor.txt`, `editor-certificate.txt`, `shared/`) and
    `backups/`, and `restore.sh`/`restore.bat` and `reset-editor-password.sh`/`.bat`; `world.toml` gains `[editor]`
    (`enabled`, `owner`, `new_devices`);
  - Updates: a backup before migrations when the version changes; the server reads the current and previous build
    formats; a rebuild only as a last resort, asked about when there are unpublished changes; the add-only seed step;
    Editor accounts and the certificate survive;
  - the world is registered as dev, served with `--world-kind live`, from slice 2;
  - the export test allows the AUTHORED tables and still fails on any player or admin row, and on any `dm.*` row.
- **Section 9 (the client):** an Editor flow on the main menu and a second window, `editor`, showing the signed
  Editor bundle the world's helper carries, through the shell's pinned `editor` scheme, with no shell permissions.
  Section 9's "a world's machine can't serve pages into the client" becomes "only code the user signed runs in the
  client" (doc 78, section 6.6).
- **Section 10 (phases):** doc 78's slices follow Phase 5's end-to-end run and review; Phase 7's legal pages take
  section 9's lines; Phase 7's "DM tool on central ids" moves into doc 78's M3.
- **Section 11 (tests):** add section 18's tests by reference.
- **Section 12 (risks):** the "host harvests chat" row says plainly that a host can, and points to section 9 here; a
  new row for the internet-facing Editor port points to section 5 here.
- **Progress, "Phase 5's plan":** "The other Atlas tables stay out" is reversed by doc 78.
- **`tools/world_package.py:30-31`**, the module docstring: "Atlas's source" is no longer left out.
- **`Package/entrypoint.py`:** drop `--dm-directory`; the helper, the `admin` volume, the certificate and the first
  owner; the request directory; keep the helper up on any server exit; re-read `world.toml` before each start and
  write the helper's settings copy; the 5 s maintenance poll; the fallback to the previous map; the Mind's ledger out
  of `LOGS`. **`Package/Dockerfile`:** `openssl` named, the `ratw-admin` user, `ratw-editor`, the built and signed
  `Editor/`, the breached-password list. **`Package/stage.sh`:** the signed `bundle.json`. **`Package/compose.yaml`:**
  the Editor publish (127.0.0.1 by default), `RATW_EDITOR_PORT` and `RATW_EDITOR_BIND`, the `admin` and `requests`
  volumes, the `./manage` mount, the backup service and `./backups`, log limits. **`Package/README.md`:** the
  second-world recipe, the Editor's first sign-in, opening the Editor port, the VPS warning, the "ports are not
  available" fix, and `up -d` after `.env` changes.

**Doc 76** (as amended by doc 77)
- **"Not in v1":** strike "several windows". The Editor adds a second window.
- **Section 4.5 (the window):** a second window, `editor`, opened by the shell, incognito, made anew for each Editor
  session; the window-state plugin keeps its size and place separately.
- **Section 4.6 (what the page may use):** add: the main window may call the `editor_*` commands; the Editor window gets
  no commands at all (shape a) or only `world_manage` (shape b), never `central_call`; the `editor` URI scheme answers
  only that window.
- **Section 4.7 (signing in):** the Editor needs no central sign-in. Its password is typed in the main window, sent only
  by the shell over a pinned TLS connection, and never stored; the session token stays in the shell's memory; device
  tokens go in the OS keychain beside central's refresh token; the pin store keeps keys, addresses and names only.
- **Section 6 (where the code lives):** the shell gains `editor.rs` (the verifier, the pin store, the commands, the
  scheme proxy, the bundle check) and the release key's public half; ureq is pinned to `=3.5.1` with its `gzip`
  feature; `Editor/` is staged into the world image and signed (shape a) or built into `Client/dist/manage/`
  (shape b). No dialog plugin.
- **Section 8 (tests):** the Editor window can't reach `central_call` or any built-in Tauri command; the `editor`
  scheme refuses other windows and stale sessions; an unsigned page file is refused; a changed key is refused and a
  signed rotation accepted; no password or token ever appears in a page's memory or in the pin store.

## 16. Risks

| Risk | What we do |
|---|---|
| Password guessing against an internet-facing Editor port | The port is loopback until the host opens it; 12-character minimum, a breached-password list and the world-name rule; random one-time passwords that expire; scrypt; per-address (per /64), per-account and world-wide limits; at most 2 checks at once; connection caps; the dummy hash; `new_devices = "approve"` for hosts who want more. |
| A stranger locks the owner out, or blocks a friend's first sign-in | An unguessable owner name; lanes for known devices, Editor codes and one-time accounts that strangers' failures never fill; only failures count world-wide; `ratw-editor invite` and the reset script are never limited. |
| One stranger keeps the port busy under Docker Desktop, where addresses fold | Residual, stated (section 3.2): codes and known devices still get through; the fix is not opening the port. |
| A man in the middle at the first connection | Editor codes carry the key, for every co-builder and the owner's first sign-in; a bare address shows all eight groups with where to compare them; "I compared it" from public addresses. Residual for bare addresses, stated. |
| Hosts click through a changed-key warning | There is nothing to click through: the red screen has no sign-in; forgetting the old pin needs the world's name typed. Honest rotations are signed and never show it, and the key survives updates in its own volume, so the red screen stays rare enough to mean something. |
| The key is lost on every update, training hosts to press FORGET | The `admin` volume (section 6.4); a test that an image update keeps the fingerprint. |
| One world's page drives another world from a window left open | I10: each window bound to one session, destroyed on any change; the proxy's binding check; queued saves only for the same world and account. |
| A hostile world runs its own script in a co-builder's client | Only the signed bundle runs (I6); the CSP and downloads rules; "RATW never asks for your RATW password here". |
| The release key leaks | It signs only the Editor bundle; a new shell release rotates the public key and raises the oldest accepted version. Kept off the repository, beside central's keys. |
| A co-builder climbs to owner, or a stolen owner session makes itself permanent | Device tokens scoped to their account; owners only through `ratw-editor`; step-up on account changes in the client's own screen; banners and log lines for every change; ending sessions forgets devices. |
| The world's session limit lets a co-builder sign the owner out | A full world refuses the new sign-in; other accounts' sessions are never evicted. |
| A host reuses their central password, or a co-builder theirs | The screens and guide say "a password only for this world"; the host's database holds only a slow hash. |
| A VPS or café-Wi-Fi host exposes the Editor port without knowing | Loopback by default; opening it is a deliberate line in `.env`, with the VPS warning in the README's first-start section and the guide; the safeguards above whatever they choose. |
| The first-editor file is left in a shared or synced folder | It works once, is emptied within seconds of use, and expires after 7 days; the guide's never-zip and synced-folder warnings. |
| Logs or audit rows fill the disk | Log limits on every service; folded failure lines and rows; no row per unknown name. |
| `chown`/`chmod` do nothing on a Windows bind mount | M0a checks; errors are logged and never stop the start; the guide's warning about who can read the folder. |
| The Editor misbehaves on macOS | Hidden there until tried (Q26). |
| A host forgets their password and can't use a terminal | `reset-editor-password` in the world folder, by double-click. |
| A new map crashes the server on load and locks the host out | The helper stays up on any exit; restarts back off; two failed starts after a new build fall back to the previous map by themselves (12.4). |
| Publish runs a 16 GB Windows PC out of memory and the kernel kills the world | "Can this computer publish" shown up front; the memory check; the build in a child with `RLIMIT_AS`; one build at a time; the `.wslconfig` steps (D2). |
| A host or co-builder breaks the economy or map | Backups before every Publish and update; preview open only to the host; previous map; discard; Reset economy rules; the controller's floors (doc 75); the audit list names the account; roles keep DM-only helpers out of Atlas and My world. |
| A live edit points at map that isn't published, or Publish removes something in use | Live edits refused ahead of the map; Publish's reference and checkpoint checks (10.3). |
| A compromised game server climbs into the helper, forges its history, fakes the AI budget or marks the world official | The `admin` volume, closed to `ratw-server`; every trusted `dm.*` table revoked from `ratw_game` (I11), `dm.audit` included; the Mind's ledger and the agent's `kind` moved where only `ratw` writes; the helper in no shared group; tests as `ratw-server`. |
| An injected string or a hostile page in the Editor window | Text only, the widened test (Editor screens in `frontDoor.ts` too), the signed bundle, the shell's CSP, forced content types, and a window with no shell commands; the password and token never enter a page; hello validated in the shell. |
| The `editor` scheme behaves differently on WebKitGTK and WebView2 | M0a tests both; shape (b) is the fallback (D1). |
| Hosts harvest players' words, or send them to an AI provider | Players are told plainly; the filters make it inconvenient; host terms, report and delist. |
| Players expect privacy across worlds | They are told wolves are recognisable everywhere and hosts see addresses (Q9). |
| A world looks clean but hands out cheats | The server enforces the `cheats` settings at the point of use; a modified image can still lie, and the guide says so. |
| A package update pushes unpublished map edits live, or can't start on a small PC | Two build formats read; rebuild only as a last resort, with the memory check first and the host asked about unpublished changes (12.5). |
| A migration rewrites a host's edits | Schema-only rule and its test. |
| Features that need new seed rows never reach existing worlds | The add-only seed step (Q17), or the release notes say so. |
| Backups fill the disk, or keep deleted wolves | Build cells dumped once each; caps on pins; the free-disk check; the privacy page says how long. |
| A shared world leaks the token, accounts and players' data | "Share my world" strips them; the certificate's key is never in the folder; the guide says never to zip the folder; import treats packages as untrusted. |
| Hostile map data hangs players' clients | Publish's validator with the client's limits; doc 77's client tests add a fuzz test. |
| An old `compose.yaml` has no `./manage` mount | The log points to the reset script (which prints the Editor code); the guide says to take the new `compose.yaml`; Publish refuses without a backup. |
| The Editor port collides with a second world or a Windows reserved range, **and the whole world won't start** | 17789, `RATW_EDITOR_PORT`, the README's recipe, the quoted error and its fix, M0a's check (D1 picks another default if needed). |
| The AI key is abused through a stolen session | No route edits the AI config; daily caps and a monthly budget counted where the server can't write; AI use shown in My world. |
| Restoring a backup brings back a removed co-builder | Restore keeps today's accounts by default and ends every session. |
| The DM API changes often (doc 75 added edicts last week) | The every-route test fails on any new route until it is allowed or denied by name, with its role. With the served page, page and API ship together. |
| `world.edits` grows without limit | Not a v1 problem; pruning later. |

## 17. Decisions for the user

**Settled by the user, 2026-11-08** (the earlier draft's Q1 and Q2): hosts reach their tools from an EDITOR button on
the main menu, by address, with a username and password, from anywhere; not through a key file on the world's
computer, an SSH tunnel or a central admin ticket. The rest of the safety design (TLS with a pinned key, world-local
accounts, the limits, official worlds refused) follows from that and is in section 6. Q1 decides only where the port
answers out of the box, not whether managing from anywhere works.

1. **The Editor port by default.** (a) On, on every interface; `RATW_EDITOR_BIND=127.0.0.1` keeps it to the world's
   computer; (b) on, but only on the world's computer until the host sets `RATW_EDITOR_BIND=0.0.0.0` in `.env`;
   (c) off until the host turns it on. *Recommended: (b)* (changed from the rewrite's (a)). The commonest host plays
   on the PC that runs the world and needs nothing more. A fresh world's one-time password never waits on an open
   port; a laptop on café Wi-Fi and an IPv6 home network expose nothing. A host who manages from elsewhere is already
   editing their router; one line in `.env` is small beside that, and screen 3 and the guide give it. (a)'s argument
   assumed IPv4 NAT. Either way `[editor] enabled = false` turns the Editor off and the helper then doesn't listen.
2. **Roles for Editor accounts.** (a) `viewer`, `dm` (DM only), `builder` (DM and Atlas, Publish) and `owner` (also
   My world's actions and Editor accounts); (b) only `dm` and `owner`; (c) one role, everyone is owner.
   *Recommended: (a)*, with owners made only by `ratw-editor` on the world's computer. It is a schema-only CHECK
   change and a role column on the allowlist, and it gives the user's "DM only vs Atlas and DM".
3. **The first account.** (a) Made at first start, with a one-time password and an Editor code in
   `manage/first-editor.txt` and a forced change at first sign-in; (b) only by command (`docker compose exec world
   ratw-editor add`); (c) both: (a), plus the command and the reset script for recovery and co-builders.
   *Recommended: (c)*, with the owner's name generated (`host-` and four random characters) unless `[editor] owner`
   names one, and the one-time password renewed on a start after 7 days unclaimed. Hosts open a file instead of
   typing a command, and a stranger can't aim at a known name. Never a password in `world.toml`.
4. **The first connection's key check.** (a) A bare address, the fingerprint shown in all eight groups, trusting one
   button ("I COMPARED IT" on a public address); (b) the host types the fingerprint's first 8 characters from the
   file; (c) an **Editor code** that carries the key, the username and the one-time password, made by
   `first-editor.txt`, the reset script, `ratw-editor` and screen 5, with (a) left for an owner typing a bare address.
   *Recommended: (c).* Co-builders and the owner's first sign-in then compare nothing and pass on one string instead
   of four things; about 1 plan hour (0.5 expected).
5. **Device tokens against lockout.** (a) Yes: a client that signed in before can't be locked out by strangers'
   failures, with tokens scoped to their account, at most 5 an account, kept in the keychain, and forgotten on reset;
   (b) no: per-account lockout as the DM tool does today, with `ratw-editor reset` to unlock. *Recommended: (a).*
   About an hour of work; without it anyone who learns the owner's name can keep them out.
6. **How the page reaches the Editor window.** (a) Carried by the world's helper through the shell's pinned `editor`
   scheme, **signed with the user's release key and checked file by file by the shell**, into a window with no shell
   permissions; (b) bundled in the client with a `world_manage` transport. *Recommended: (a), if M0a shows the
   scheme works.* No version drift, no transport swap, about 4-5 plan hours less than (b), and a hostile world can't
   run its own script. It needs the user to keep a release key beside central's keys.
7. **Cheats.** `cheats = "off" | "host" | "everyone"`: `host` unlocks only the DM tool's `character.gift` and
   `account.unlock` (and rewards to the host's own wolves); `everyone` is `--dev-tools`, socket cheats for all players.
   Both are listed "cheats on". `character.dm` is never available to hosts. *Recommended: all three, default `off`.*
8. **The filters in the host's tools.** (a) Section 9's filters, with players told plainly that the host can read
   everything; (b) everything the DM tool shows today, with the same disclosure. *Recommended: (a).* It limits a stolen
   session, a co-builder or an injected page.
9. **Wolves recognisable across worlds.** (a) Per-world wolf ids in community tickets and heartbeats; (b) tell players
   plainly. *Recommended: (b).* Wolf names are unique across central and shown in game, so per-world ids would hide
   little.
10. **Reports on community worlds.** (a) The host decides reports about other wolves; reports about the host go to
    central from the reporter's client once Phase 7 builds the route, kept on the client until then; the reporter is
    hidden from the host; (b) all reports go to central (doc 77 section 3.2 as written). *Recommended: (a).*
11. **The hosted world's kind, from slice 2.** (a) A dev world served as live (doc 66, how DEV runs); (b) a new
    `hosted` kind. *Recommended: (a).* The allowlist is the boundary.
12. **Where Publish builds.** (a) In the world's container, refused below N GB free and shown up front; (b) a separate
    build container started on demand; (c) a leaner streaming build first. *Recommended: (a)*, with N from M0b.
13. **Atlas source in every package**, or only in an "editable" one. *Recommended: every package*, also so updates can
    rebuild.
14. **Restore.** (a) Scripts in the world folder plus "Copy restore steps"; (b) also from inside the Editor, with the
    owner's password asked again and a safety dump first. *Recommended: (a) for v1.* Now that the Editor port can face
    the internet, a stolen owner session shouldn't be able to roll a world back.
15. **Backups' build cells.** (a) Each build dumped once, daily dumps without build cells; (b) keep 2 builds in hosted
    mode and dump everything; (c) skip build cells and rebuild on restore. *Recommended: (a)*, with M0a's sizes. (c)
    would need Publish's memory to restore.
16. **AI limits for the host's tools:** 100 life stories and 20 roster generations a day, and a monthly token budget
    in `world.toml`, counted where the game server can't write. *Recommended: yes.*
17. **New seed rows on update.** (a) An add-only step that inserts missing keys and never changes existing ones;
    (b) none: features that need new rows don't reach existing worlds. *Recommended: (a).*
18. **The legacy Storykeeper** (`dm_service.py`, `DM/`, `--dm-directory`). *Recommended: leave it unstarted and drop
    `--dm-directory`.* Its work runs inside the server now.
19. **Official map changes on hosted worlds:** never in v1 (a fresh world for a new map); a merge tool later if wanted.
    *Recommended: never in v1.*
20. **Cross-session Atlas undo.** *Recommended: slice 3.* Discard unpublished changes, in slice 2, covers the trap.
21. **Order.** Slice 1, then the user tries it, then slice 2. *Recommended: yes.* "Run my world", with the Editor's
    way in, lands in about 15-20 expected hours, and map editing builds on a tested helper.
22. **"Stay signed in on this computer."** (a) A revocable, device-bound credential kept in the keychain, so a host
    signs in once a week, listed under each account and revocable by the owner; (b) sign in each time the client
    starts, as now: the session lives in memory, 30 minutes without input, 12 hours at most. *Recommended: (b) for
    v1.* An Editor session can kill players' wolves and change the economy, game PCs are often shared in a family, and
    a long-lived credential on disk would skip the password entirely. Revisit after slice 1 has been tried (the
    reviewer's worry, hosts retyping and then reusing passwords, is real).
23. **New-device approval.** `[editor] new_devices = "open" | "approve"`: with `approve`, a new computer signs in only
    with an Editor code (from `ratw-editor invite` or the owner's screen 5) as well as the password. *Recommended:
    offer it, default `open`,* and the guide recommends `approve` to anyone who forwards the Editor port to the
    internet. It reuses the Editor code, so it costs about half an hour.
24. **Acting on players' wolves.** (a) Only the owner, and accounts the owner marks "may act on players' wolves", may
    kill, injure, move or resurrect a player's wolf; (b) every `dm` and up. *Recommended: (a).* These hurt real
    players; a co-builder helping with events rarely needs them, and the flag is a column and a checkbox.
25. **The user's own studio binds 127.0.0.1 by default.** Today it binds `0.0.0.0` over plain HTTP, guarded only by a
    forgeable header check, with PROD's DM accounts. *Recommended: yes*, with `--host 0.0.0.0` for the user's own LAN
    use, passed by his launchers where wanted.
26. **macOS.** (a) Hide EDITOR on macOS until it has been tried on WKWebView; (b) ship it untried. *Recommended:
    (a).* There is no Mac to try it on yet, and WKWebView's custom schemes differ on bodies and downloads.

## 18. Tests and checks

- **The certificate:** made once, kept across restarts, `down`/`up` and an image update that recreates the container
  (the `admin` volume); the log's, the file's and `ratw-editor fingerprint`'s fingerprints match the one the shell
  computes, in the same eight groups; `new-certificate` changes it, the running helper reloads it, and a pinned
  client moves over through the signed rotation; `new-certificate --leaked` ends every session and gives the red
  screen.
- **The shell's verifier:** the pinned key passes whatever the name and dates; a different key is refused; a server
  that presents the pinned certificate without its key fails the handshake; TLS 1.2 is never offered; record mode
  sends nothing but `/editor/hello`, with no username, device token or code.
- **The pin store:** keyed by key; the same key at a new address is "moved" and reuses its device token; a new key at
  a pinned address is red **even when the hello claims a new world id or name**; a known world id under a new key is
  red; an Editor code pins without a prompt; the game-port `/status` fallback stores nothing; device tokens are in the
  keychain, never in `editor-worlds.json`.
- **The client's screens:** forgetting needs the typed name; the game port gives "that's your game port"; an official
  world gives "can't be edited", and so does a listed official address before connecting; a world still setting up
  gives "still setting up"; `localhost` reaches a loopback-only port; EDITOR is hidden in a browser and on macOS. A
  hello with a 200-character name, bidirectional characters, free-text progress or unknown fields is trimmed or
  refused by the shell.
- **Signing in:** a wrong password refused with the same words and about the same time for a real, an unknown and a
  disabled name; the one-time password works once, forces a change, and empties `first-editor.txt`; an expired one is
  refused, and the owner's is renewed on the next start; a breached password, or one containing the world's name, is
  refused at screen 4b; a password change ends the account's other sessions and forgets its devices; the host token
  refused everywhere; an admin row can't sign in in hosted mode; `/api/dm/login` returns 404 in hosted mode.
- **The limits:** 5 failures from an address slow that address (IPv6 per /64); failures from unknown devices block
  unknown devices on that account but not a known device, an Editor code or an account on a one-time password; a
  device token shown with another username counts as unknown and as a failure; an account keeps at most 5 devices;
  only failures count toward the world-wide cap; a third concurrent password check waits, known devices first; more
  than 8 waiting get 503; a fifth connection from one address and the 33rd overall are refused; a slow handshake is
  dropped; a large `/api/live/world` reply and the validator running don't delay a sign-in; a full counter table fails
  closed; a helper restart keeps account locks; chunked bodies and bodies without `Content-Length` are refused before
  sign-in; a handler that raises doesn't stop the helper; `ratw-editor invite` and `reset` work while locked; with
  `new_devices = "approve"` a correct password from a new device without a code is refused.
- **Sessions:** idle timeout, and an open window with no input still times out; `last_seen` written at most once a
  minute; the 12-hour limit; the fourth session of an account evicts its own oldest; with the world's 10 full, an
  eleventh sign-in is refused and nobody is evicted; End all Editor sessions also forgets devices; sessions survive a
  helper restart; a disabled account's session stops at once.
- **Roles and accounts:** a `dm` account can't reach Atlas, Publish, My world's actions or Editor accounts; a
  `builder` can't reach My world's actions or accounts; nobody can make or promote an owner except `ratw-editor`;
  every account route needs a step-up and is refused through the `editor` scheme; every account change writes a log
  line, an audit row and a banner; a `dm` without the flag can't kill, injure, move or resurrect a player's wolf; the
  last owner can't be removed or disabled; every allowlisted route names its role; Publish refuses a seventh in an
  hour.
- **The game server can't climb:** as `ratw-server`, try to read and replace everything in the `admin` volume (the
  key included), to replace `manage/editor-certificate.txt` and `first-editor.txt`, and to write `kind: official`
  where the agent records it; as `ratw`, try the two `manage/` files; as `ratw_game`, read and write `dm.admins`,
  `dm.sessions`, `dm.devices`, `dm.invites`, `dm.locks`, `dm.ai_use` and `dm.audit`; forge a session. All fail. A
  forged Mind ledger changes nothing in the budget. As `ratw-admin`, `npc-voices.jsonl` can't be read. A grant
  listing fails on any new `dm.*` table open to `ratw_game`.
- **The browser rule:** a request with `Origin`, `Cookie` or `Sec-Fetch-Site` refused; no `OPTIONS` answer and no CORS
  headers; plain HTTP on the Editor port gets no reply; unauthenticated bodies over 4 KB refused; page files refused
  without a session; with `enabled = false` nothing listens.
- **The proxy and the window:** the `editor` scheme refuses the main window and any other label; after a switch of
  world or username, the old window is gone, its storage is empty, and a request with its binding is refused; queued
  saves retry for the same world and account only; the page can't set `Authorization`, `Cookie` or `Host`;
  `Content-Length` is passed on; `/editor/*` is refused; redirects aren't followed; the shell's CSP replaces the
  world's; a page file not in the signed list, a bad signature and an old bundle version are refused; an `/api/` reply
  that holds script is served as JSON with `nosniff` and doesn't run; gzipped replies decompress and the 32 MB cap
  holds after decompression; 16 MB requests pass and larger are refused; a 401 reaches the menu; downloads outside the
  allowlist are refused and the rest go through the save dialog; the Editor window has no commands (shape a) or only
  `world_manage` (shape b): `plugin:window|set_title`, `plugin:event|emit`, `set_fullscreen`, the window-state and
  page-only plugins' commands and `central_call` all fail; it can't navigate off its origin or open windows.
- **Secrets stay in the shell:** no password or token in the pin store, the log, or any page's memory (a test page
  probes `window`, storage and the DOM after sign-in).
- **Logs:** no sign-in body, query string, token, password or request-valued traceback in any log (I15); every
  compose service has log limits; above the threshold, failures fold into one line a minute.
- **SQL:** a grep test fails on any f-string SQL in the studio's three files whose identifier isn't from a fixed map or
  the catalogue (I14).
- **Official worlds:** the entrypoint makes no owner or certificate and doesn't start the helper with the
  shared-progress credential or after central said official; a hand-started helper refuses every sign-in.
- **The allowlist** names every route; the denied ones (switch, import, rollback, `server/start`,
  `/api/studio/atlas-world`, `/api/studio/dm-world`, playtest, export, `/api/atlas/session`, `/api/dm/login`) return
  404 in hosted mode. No route reads or edits the AI config.
- **Cheats:** with `off`, `character.gift` and `account.unlock` refused, a saved `dungeonMaster` flag cleared on load,
  and the host's rewards to their own wolves refused; with `host`, they work and the socket cheats still don't; with
  `everyone`, `/status` says `cheats: true`. A player is told when the host acts on their wolf.
- **Crash recovery:** a build that crashes on load falls back to the previous map after two failed starts; the helper
  answers throughout; restarts back off.
- **Publish:** an NPC or economy edit counts no map changes and a cell edit counts one; a package update with no edit
  counts none; removing an interior that holds a leased flat and a wolf is caught by validation, and if forced through,
  the server starts and moves the wolf somewhere safe; a live edit naming an unpublished interior is refused; host-only
  admits the host and refuses another wolf; Publish refuses with no backup.
- **Discard:** after a bad edit, discard and Publish give back the old map.
- **Package:** the export with source scans clean, author columns included, and carries no `dm.*` row; migrations keep
  a host's edited cell and the Editor accounts; the server loads the previous build format; an update with
  unpublished changes asks before rebuilding; the add-only seed step inserts a new key and leaves an edited one alone;
  import refuses unknown columns and oversized rows.
- **Backups and restore:** a dump has its manifest; restore refuses a running world, another world's dump and a newer
  one; it takes a safety dump first; by default it keeps today's Editor accounts and devices, ends every session and
  lists the accounts; pins over the cap are refused; a dump refused on a nearly full disk.
- **The reset script:** on Linux and Windows it lists accounts, resets one, shows the Editor code, writes it into
  `first-editor.txt`, and waits before closing.
- **Memory:** a build that exceeds `RLIMIT_AS` fails alone; the server keeps running.
- **Client, the rest:** the Host page's EDITOR tries 127.0.0.1 first and matches by world id, otherwise fills the
  listed address; the widened text-only test passes on `Editor/src` and the Editor screens in `frontDoor.ts`; a 401
  keeps queued saves and signing in again as the same account retries them; doc 77's client tests gain a fuzz test
  against hostile world data.
- **The user's studio is unchanged:** `test_studio.py` and `test_dungeon_master.py` stay green without hosted mode,
  with the widened role CHECK.
- **By hand:** M6's and M10's runs on this laptop, then on the Windows PC and from a second computer.

**Still to check (not verified):**
- That the `editor` URI scheme can carry the studio app on WebKitGTK and WebView2, incognito, with `on_download`
  (M0a). (The navigation hook does receive the asking webview: `plugin.rs:459-461`.)
- Whether a `0.0.0.0` publish also answers on IPv6 under Docker Engine and Docker Desktop (M0a).
- Whether `chown` and `chmod` work on a Windows-folder bind mount under Docker Desktop (M0a).
- That nothing but the Mind writes or reads `npc-mind-calls.jsonl` where it is now, before moving it (M3).
- The Editor on WKWebView (macOS): not tried; hidden until it is.
- That ureq's `Agent::with_parts` takes a copied `RustlsConnector` with a custom verifier (M0a).
- Whether the container sees real peer addresses under Docker Engine and Docker Desktop (M0a).
- Publish's RAM and time at today's build size, and the source's size (M0b).
- Real dump sizes with and without build cells (M0a).
- Whether builds carry a format number (none found by grep).
- What the server does when the checkpoint names a place the build lacks (M0b).
- Whether a ticket's `gift` claim overwrites a world-set Gift on the next `hello`.
- Whether anything a community world sends central (the agent's heartbeat) carries Gift or tier changes. By doc 77
  section 6 it shouldn't.

## Progress

M0a measured, M1, M2a and M4 built (2026-11-12, below); the rest not yet.

- **2026-11-08: the user's decisions.** "Go with the recommendations": all 21 decisions in section 17 take their
  recommendation (among them: the Editor answers only on the world's own computer until the host opens it; roles
  viewer, dm, builder and owner; the first account in a file with a reset script; Editor codes, the fingerprint as
  the fallback; slice 1 first, the user tries it, then slice 2).

- **2026-11-08: the user's Editor design.** The user, verbatim: "In the main menu before they connect to a server they
  have an "editor" button that prompts for an IP address (to a server) and then, if there is a connection, it prompts
  for a username and password, and then it brings them to another menu for Atlas and DM." The doc was rewritten around
  it the same day (see the Review's second part). Plan only; nothing built.
- **2026-11-08, night: three reviews of the rewrite.** Security (24 findings), the code at 029d050 and the built image
  (26), and a non-technical Windows host's side with scope and hours (22). Most were taken; the Review's first part
  lists what changed and what was rejected. Plan only; nothing built.

- **2026-11-12: M1 built (Guards in the server),** on `r2-editor` (cut from ridgemere2 at 65a9f0e, with doc 79's staged
  economy and migration 0061). Every item of section 14's M1 row:
  - **The three `cheats` settings.** `world.toml`'s `[world] cheats = "off" | "host" | "everyone"` (default `off`; any
    other value stops the start). The entrypoint passes nothing, the new narrow `--host-cheats`
    (`Options::hostCheats`) or `--dev-tools`. `/status` says `cheats: true` for either; an official world refuses
    `--host-cheats` as it refuses `--dev-tools`. `--host-cheats` gives players no socket cheats (those still check
    `--dev-tools` alone) and nobody the Dev Console.
  - **The server's guards, at the point of use** (`Game::dmActRefused`; community worlds only, so the user's own
    worlds and their DMs are as they were). With cheats off: `character.gift` and `account.unlock` refused, saying
    which setting allows them; `character.dm` refused without `--dev-tools`. The host's own wolves (the account in
    `host.json`, whichever Editor account asks) get no rewards with cheats off: `character.resurrect`, `deed.award`,
    `storyline.give`, `mentor.restore`, `milestone.credit` naming one of them, `estate.set` on a place their Chapter
    leases or they lodge in whole, a grant (`character.grant` with coins above 0, doc 79's DM economy act, guarded in
    `applyEconomyCommand`) and the Storykeeper's `player_grant` (a new `director::Bridge::grantRefused` hook, so the
    guard holds even if a host runs the legacy bridge). Fines are not rewards and pass. `wharf.credit` (section 8's
    table) no longer exists since doc 79 (Core answers "Unknown action"); `character.grant` is its successor.
    `band.override` and the Storykeeper's `npc_band` act on residents' households only, never a wolf: nothing to guard.
  - **`dungeonMaster` ignored and cleared.** `Game::devConsoleAllowed()` (not a community world, or `--dev-tools`)
    gates the console, its command list and `snapshot.self.dungeonMaster`; a saved mark is cleared at the restore and
    as the wolf comes in, each logged `RATW_CHEATS`.
  - **The in-game notice.** On a community world a player is told when the host's tools act on their wolf, in the
    host's name: killed, brought back, injured or healed, moved ("The host moved you: you find yourself somewhere
    else."), given a Gift, granted or fined (the Storykeeper's too, through `Bridge::granted`), their tie ended, their
    mentoring turned off or on. A wolf away is told when it next comes in ("While you were away: ..."), from
    `Entity::hostNotes` (saved with the character, at most six, then cleared). The user's own worlds keep their words
    ("A Dungeon Master grants you ...") and tell only a wolf in the world, as before.
  - The per-kind body of `applyDmActions` moved, unchanged, into `Game::applyDmAct(row)` (public: the C++ tests have
    no `dm.actions` table); the guards are its first step.
  - **Migration 0062** (`0062_dm_server_state.sql`): `GRANT UPDATE (state, changed_by, changed_at, note) ON
    admin.server_state TO ratw_dm`; no INSERT, DELETE or `singleton`. M3 moves Close and Open onto it.
  - **`--dm-directory` off the package** (`server_command`; no `STATE/dm` made). The legacy Storykeeper stays
    unstarted (Q18).
  - **The agent's `kind` and host id** go to `central.json` (`{at, kind, host}`) in `--central-dir`, which the
    entrypoint makes as `STATE/central`, owned by `ratw`, mode 0700 (the shared agent directory stays as it was for
    `host.json`, which the server reads). Written from each heartbeat; a refused token, a delisted world or a banned
    host clears the host and keeps the kind; a restarted agent keeps the kind it last wrote; the keys-only mode (no
    token) removes it. Root's copy into the `admin` volume and the "ever official" record are M2b's.
  - **The playtest path fix** (`map_editor.py`, `/api/playtest`): the project's id is checked against Atlas's id
    pattern before the folder is named, on the big world's path too, and the folder must sit directly in
    `Saved/Playtests`; otherwise 422.
  - **The studio's 127.0.0.1 default** (Q25, recommendation taken): `studio.py serve` binds 127.0.0.1 unless given
    `--host`; `tools/studio.sh` passes `--host 0.0.0.0` unless its arguments name a `--host`, so the user's usual
    launcher keeps the local network; `editor.sh` and `dungeon-master.sh` pass `--host 127.0.0.1`, as their headers
    always said ("local only"). **Changed for the user:** those two used to answer on the local network as well;
    `--host 0.0.0.0` brings that back.
  - **Tests** (one run): a fresh Release build (`build-m1`, from empty; 128 warnings, one of them a shadowed name in
    the new Storykeeper hook, renamed after the run and rebuilt: 127 as before) and the full ctest once with the
    database (`RATW_TEST_DB_TAG=s2stage_`, `RATW_TEST_KEY_PREFIX=s2stage`; `pg_client` on a throwaway
    `ratw_test_s2stage_m1`, dropped after): **89 of 89 passed**, none skipped. `ticket_tests` 1,012 checks (new:
    `theHostsToolsKeepTheCommunityGuards`: everyone, off and host on one save, the mark cleared, every guarded kind,
    the notices online and away, an official world refusing `--host-cheats`); `server_parts_tests` 267 (the
    Storykeeper's hooks). Tools: `Package/test_package.py` 20, `test_world_agent` 20, `test_map_editor` 52,
    `test_studio` 54, `test_world_package` 48 (1 skipped: the pre-79 package check, DEV's build has moved on),
    `test_dm_live` 10 (new: the DM role closes and opens its world, refused INSERT, DELETE and `singleton`), and
    `test_dungeon_master` 54: all OK. No `s2stage` database or `/tmp` file left;
    `artifacts/screenshots/dev-console-fight.png` untouched.
  - **DEV migrated:** `pg_dump -Fc` first (`Saved/Backups/ratw_ridgemere2-before-0062.dump`, 303 MB), then
    `world_db.py migrate --db dev`: 0062 applied to `ratw_ridgemere2`. PROD and old DEV untouched.
  - Not in M1: the "ticket's `gift` claim against a world-set Gift" question (M0a); Disabled-with-reason states in the
    DM tool (M3/M5); the I11 revokes and `dm.audit` (M2a).

- **2026-11-12: M2a built (The helper's security core),** on `r2-editor` after M1. `tools/world_admin.py` (standard
  library, the image's psycopg and `openssl`; no new Python dependency) and `tools/test_world_admin.py`. Every item of
  section 14's M2a row:
  - **TLS 1.3 with the world's key, reloaded on a signal.** `ssl.SSLContext(PROTOCOL_TLS_SERVER)`, minimum and maximum
    TLS 1.3. `make_certificate` (the entrypoint's, M2b) makes an ECDSA P-256 key and a 100-year self-signed certificate
    with `openssl req` once, with the world's id (its central id, or `local-` and 32 hex when it has none) in
    `world-id`; the fingerprint is the SPKI's SHA-256 (the SPKI read from the certificate's DER by a small parser), its
    first 128 bits in 8 groups of 4. SIGHUP builds a new context for new connections (a bad key keeps the old one).
    Nothing on the port is ever answered in plain HTTP: the TCP side pauses reading at once, the limits run, then
    `loop.start_tls` (10 s handshake); a failed handshake closes without a byte. (`StreamWriter.start_tls` was tried
    first: its reader kept the TCP transport for flow control, and two large bodies read at once stalled for good; the
    TLS transport now gets a reader and writer of its own.)
  - **The hello** (`GET /editor/hello`, public, at most 1 KB, cached 2 s): `{world, name, package, editorApi: 1, kind,
    serverState, progress {code, percent}, setupPending, rotation?}`. It answers before the database does (progress from
    `progress.json`, which the entrypoint will write; without one, `database` until it answers); the name loses control
    and bidirectional characters and is cut to 40; `setupPending` while an owner is still on a one-time password; `kind:
    "official"` once `ever-official` exists or `central.json` says so, and then every sign-in and every signed-in route
    is refused.
  - **Signed rotation.** `ratw-editor new-certificate` makes a new key, signs `"RATW Editor key rotation v1\n<world
    id>\n<new key's pin>\n"` with the old key (ECDSA P-256, SHA-256, DER: what ring's `ECDSA_P256_SHA256_ASN1` checks),
    keeps the old key as `previous-key.pem`, writes `rotation.json` (the new SPKI, the signature, the old pin) for the
    hello, and signals the helper through `helper.pid`. `--leaked`: no signature, the previous key dropped, every
    session and device ended.
  - **The routes:** `/editor/login`, `/editor/password`, `/editor/step-up` (5 minutes, bound to the session; five wrong
    passwords typed in an open session end it), `/editor/keepalive`, `/editor/logout`, and `GET /editor/accounts` with
    `POST /editor/accounts/{add,reset,role,wolves,disable,enable,remove,end-sessions}` (owner, with a step-up). A
    one-time session may only call password, keep-alive and logout. Everything else (`/api/...`, the page's files) needs
    a bearer session and goes to `Core.app`, the hosted studio's hook (M3); until then it is 404 behind the session.
    Replies to `/api/` are forced to JSON or octet-stream; every reply has `nosniff` and `no-store`.
  - **Accounts** stay the DM tool's (`dm.admins`, `dm.sessions`) in the world's database. Screen 5's routes add
    `viewer`, `dm` or `builder` only, change roles up to builder, and can't reset an owner (a stolen owner session would
    get that owner's code: owners are reset by `reset-editor-password`/`ratw-editor`); the last owner can't be removed,
    turned off or demoted, by the screen or by `ratw-editor`. Every change writes a log line and an audit row
    (`account.*`, who = the owner's session); the list returns the banners since the owner's previous sign-in, "failed
    sign-ins today", each account's role, claim address, last address and devices.
  - **Migration 0063** (`0063_editor_accounts.sql`, schema only): the role CHECK widened to viewer, dm, builder, owner,
    admin (`RANK` 0-4 in `dungeon_master.py`: in the user's studio nothing changes; an admin row never signs in to a
    host's Editor); `dm.admins` gains `must_change`, `must_change_until`, `may_act_on_wolves`, `claimed_from`,
    `created_by`; `dm.sessions` gains `last_seen`, `device`, `step_up_until`, `address` and `since`; new `dm.devices`
    (with each device's failures and wait), `dm.invites` (with failures), `dm.locks` and `dm.ai_use`. **I11:** found by
    grep, the server uses `dm.actions`, `dm.health`, `dm.watch` and `dm.watchers`; it keeps those, and the legacy
    Storykeeper (`dm_service.py`, which connects as `ratw_game`) keeps its own tables. Its log moved from `dm.audit` to
    a new `dm.storykeeper_audit` (two lines in `dm_service.py`), so `dm.audit`, `dm.stories`, the four new tables and
    their sequences are revoked from `ratw_game`, and `dm`'s default privileges for `ratw_game` are revoked too: every
    later `dm.*` table starts closed to the game. The I11 test lists every `dm.*` table `ratw_game` holds any privilege
    on and fails on one not named; another creates a table and checks it is closed; another, as `ratw_game`, fails to
    read any trusted table or forge a session or audit row.
  - **Editor codes and invites.** `ratw-editor:` and base64url of `{v: 1, w: world id, k: the SPKI's full SHA-256, u,
    p?: one-time password, i?: invite, a?: address}`. A new account's code carries both (`add`, `reset`, the first
    owner, screen 5); `ratw-editor invite` only an invite (the password stays). Sign-in always takes the password; an
    invite (SHA-256 in `dm.invites`) is looked up before any scrypt, works once, expires (72 hours; 7 days for the first
    owner), and five wrong passwords with it void it. `read_code` checks a pasted one, for M4.
  - **Passwords:** at least 12 characters (256 at most); not containing the username or the world's name (spaces, `-`,
    `_` and case ignored; names under 3 letters skipped); not one of the NCSC's 100,000 commonest breached passwords
    (`Package/breached-passwords.txt`, 800 KB, lowercased, from SecLists), nor one of them with digits or symbols around
    it, nor one repeated; at least 5 different characters. The reply is a code (`short`, `long`, `username`, `world`,
    `common`, `simple`, `same`) the client words.
  - **The browser rule:** `Origin`, `Cookie` or any `Sec-Fetch-*` refused (403); no `OPTIONS` (405) and no
    `Access-Control-*`; `Host` ignored.
  - **Connections:** a sign-in pool (4 threads), a signed-in pool (8) and a scrypt pool (2); per address (IPv6 by /64)
    at most 4 open and a burst of 6 handshakes then 3 a second; world-wide at most 32 open and a burst of 40 then 10 a
    second; 10 s for the handshake and the first request, 60 s idle; heads at most 8 KB; bodies without a session at
    most 4 KB, `Content-Length` required, chunked refused, nothing read before the session is checked; `/editor/*` 4 KB,
    `/api/` 16 MB, at most 2 over 1 MB at once (503 beyond); 20 requests a second a session; at most 6 database
    connections; every handler and connection catches everything (the log names only the exception's type).
  - **Peers that can't be told apart (D1, built both ways).** `RATW_EDITOR_PEERS=auto` (default): the container's
    default gateway (from `/proc/net/route`; M0a: connections from the world's own computer, and IPv6 through a bare
    publish, arrive as it) and loopback are unidentified; `real` and `folded` force either. An unidentified peer has no
    per-address limits: its connections count only world-wide and its failures only in the world-wide and per-account
    counts. It is never trusted (I3): the host's own lanes are known devices, codes and the one-time password.
  - **Signing in:** at most 2 scrypt checks at once and 8 waiting; known devices and codes first, and one arriving at a
    full queue takes the newest stranger's place. Unknown, disabled and admin names (and odd stored hashes) run the same
    scrypt on a dummy hash made at start and get "Wrong name or password."; a failure's rows are written after the
    reply, so a real name takes no longer than an unknown one; a disabled account or an expired one-time password is
    told only with the right password. Lanes: **known devices** (a token for its own account only, at most 5 an account,
    5 failures make that device wait 15 minutes), **codes** (no counts), **one-time accounts** (per address only; never
    locked; not world-wide), **everyone else** (per address: 5 in 15 minutes, then 15, 30, 60 minutes; per account: 5
    from unknown devices, then 15 minutes doubling to 4 hours, in `dm.locks`, kept across a restart and not lifted by a
    good sign-in; world-wide: 20 failures a minute, starting half full). A device token shown with another username
    counts as unknown and as a failure. Unknown names lock the same way in memory, so a lock says nothing about whether
    a name exists. `new_devices = "approve"`: a typed password from a new device is refused without a code. Counter
    tables are fixed (4,096): when one is full a new address fails closed onto the world-wide count.
  - **Sessions:** `dm.sessions` rows (they survive a restart); 30 minutes without input (`last_seen`, renewed only by
    the keep-alive and the client's own screens' routes, at most once a minute; a page's requests renew nothing), 12
    hours at most; 3 an account (its own oldest evicted); 10 a world (a new sign-in refused, "Too many people signed
    in", nobody evicted). A good sign-in gives a device token (stored hashed; none on a one-time password: the password
    change gives it). A password change ends the account's other sessions, forgets its devices, invites and lock, and,
    for an owner leaving a one-time password, drops a `first-editor-used` request (`requests/`, kind-only files) for the
    root entrypoint. **The audit mapping:** a session's `who['username']` is `account#short` (6 hex derived from the
    stored hash, never the token) for `requested_by` and the audit.
  - **Logs and audit, folded:** `RATW_EDITOR` lines whose values are checked (a name, an address key, a route, a status,
    a count: anything else prints `?`); a name typed that isn't a username is never looked up or logged (it may be a
    password typed in the wrong box). One audit row per known account and address an hour, with a count; none for
    unknown names; above 10 failures a minute the rest fold into one line and one audit row a minute. Each signed-in
    request writes one line (account, route, status).
  - **`ratw-editor`** (`Package/ratw-editor`, installed in `/usr/local/bin`): `list`, `add NAME --role R` (the only way
    to an owner; never admin), `reset`, `invite`, `disable`, `enable`, `remove`, `sign-out-all`, `fingerprint`,
    `new-certificate [--leaked]`, and the entrypoint's `first-owner` (JSON: the generated `host-xxxx` or `[editor]
    owner`, its one-time password, invite and code; renewed when 7 days pass unclaimed; nothing on an official world)
    and `make-certificate`. As root it reads only the dm password from `passwords.env`, then runs itself again as
    `ratw-admin` with no other groups. Codes go only to its terminal.
  - **The image:** `Package/Dockerfile` names `openssl`, adds `ratw-admin` (10003, in no other group) and installs
    `ratw-editor`; `stage.sh` stages it and the breached-password list. (Those three were M2b's list; done here because
    `ratw-editor` needs them. The rest of M2b stands: the `admin` volume, the entrypoint's certificate, first owner,
    settings copy, progress file, request handling, restarts, compose and README.)
  - **Tests** (one run): `test_world_admin` **80** (I11 and the migration; every password rule; the lock arithmetic;
    peer keys; the scrypt gate's cap, queue and priority; the certificate, fingerprint and a rotation checked with
    `openssl dgst -verify` and refused for another world; codes; requests; signing in: same words and one scrypt for
    real, unknown, disabled and odd names with a timing check, admin refused, the host token, bad bodies, the database
    down, setting up, official; the first owner, its forced change, `first-editor-used`, expiry and renewal; every lane
    and count, devices scoped and capped, invites once/expired/voided, approve, a full table failing closed, locks
    across a restart; sessions: caps, idle, keep-alive once a minute, 12 hours, disabled, restart, logout; every account
    route and refusal, banners, audit rows; folded logs and no secret in any line; over a real TLS port: TLS 1.2
    refused, the key's fingerprint, plain HTTP unanswered, the hello, the browser rule, body caps, sessions on every
    non-public route, a one-time session's limits, a raising handler, a slow signed-in reply not delaying a sign-in, 2
    large bodies at once, the request rate, a fifth connection from one address, the handshake rate, a slow handshake,
    the 33rd connection; the real `serve` process reloading on SIGHUP with the rotation in its hello and stopping on
    SIGTERM; with the Editor off nothing listens; `ratw-editor` end to end). Also once: ctest 89 of 89 (on M1's Release
    build in `build-m1`: no C++ changed; `pg_client` on a throwaway `ratw_test_s2stage_m2a` with all 63 migrations,
    dropped after), `test_dungeon_master` 54, `test_studio` 54, `test_dm_live` 10, `test_world_package` 48 (1 skipped,
    as at M1: the pre-79 package check; its end-to-end tests need `RATW_TEST_SERVER`, given on a second invocation after
    the first skipped them), `test_dm_service` 40 (the Storykeeper's new log table), `Package/test_package.py` 20: all
    OK. No `s2stage` database, `/tmp` file or process left.
  - **DEV migrated:** `pg_dump -Fc` first (`Saved/Backups/ratw_ridgemere2-before-0063.dump`, 303 MB), then `world_db.py
    migrate --db dev`: 0063 applied to `ratw_ridgemere2`; `ratw_game` there now holds exactly the eleven tables named
    above. PROD and old DEV untouched. Until `r2-editor` is merged, the main checkout's legacy Storykeeper
    (`dm_service.py`, never started) would fail writing `dm.audit` on DEV.
  - **Time:** about an hour and a quarter of session time, against 6-8 plan hours and 3-3.5 expected (0.45): the
    0.45 factor was generous for this phase, which needed no client or Core work.
  - **For later phases.** M2b: write `settings.json`, `progress.json`, `central.json`/`ever-official`, the registry copy
    (`worlds.json`) into the `admin` volume; start `world_admin.py serve` as `ratw-admin` with `RATW_ADMIN_DIR` and the
    dm password; run `make-certificate` and `first-owner` and act on `requests/`. M3: `Core.app` is where the hosted
    studio plugs in, with `session['username']` as its `who`. M4: the hello's fields, `rotation_message` and `read_code`
    are the client's contract.
- **2026-11-12: M0a measured** (branch `r2-editor-m0a`, from `ridgemere2` at 65a9f0e). Read-only: no game server, no
  PROD, no paid calls; DEV (`ratw_ridgemere2`) only read. The spike is kept in `Desktop/spike-editor/` (a throwaway
  Tauri app, the stand-in world `world_stub.py`, `docker_peer.sh`, `windows_facts.ps1`, `measure_replies.py`); the
  Windows half ran on a GitHub runner through `.github/workflows/editor-spike.yml` (run 43289848875: Windows Server
  2025, WebView2/Edge 156; and ubuntu-latest's rootful Docker Engine 29.4.2). This laptop: Fedora 43, WebKitGTK
  2.50.5, rootless Docker Engine 29.3.1 (pasta networking). Tauri 2.12.5, wry 0.57.1, ureq 3.5.1, rustls 0.23.46.

  The `editor` scheme test: an Editor window (label `editor`) loads the built studio app (`Editor/dist`, 1.1 MB)
  through the scheme, whose handler forwards to the stand-in over the pinned client (TLS 1.3, the doc's own `openssl`
  P-256 command), adds the bearer, caps requests at 16 MB and replies at 32 MB on the wire and after gunzip, and sets
  section 6.6's CSP; a script evaluated in the window then posts and fetches.

  | Item | Result | How measured | For D1 |
  |---|---|---|---|
  | Studio app through the scheme | **Yes on both.** Page renders (the sign-in screen) on `editor://localhost` (WebKitGTK) and `http://editor.localhost` (WebView2), a secure context, `crypto.subtle` present; **no CSP violation** under section 6.6's CSP (no `style-src` needed) | Spike, `page` lines; a `securitypolicyviolation` listener injected before the page | Scheme stays |
  | 16 MB POST bodies | **Intact on both** (CRC-32 equal at the server; 16 MB binary and a 16 MB JSON string). **Neither webview passes `Content-Length` to the handler** (absent from the request's headers), so the shell must set it; ureq does from the body. 17 MB refused (413) by the cap. Linux 150 ms, Windows 370 ms | Spike, `post-*` and `scheme-request-big` lines | Scheme stays; M4: the proxy sets `Content-Length` itself, never copies it |
  | 32 MB replies, raw and gzipped | **Both work on both;** 33 MB refused by each cap (wire, and after gunzip). 32 MB raw: Linux 250 ms, Windows 600 ms in the page; 32 MB from 2.7 MB of gzip: 410 ms / 1.2 s (debug build) | Spike, `get-*` and `proxy-big` lines | Scheme stays |
  | `webview_label()` in the handler | **Works on both:** the main window's request arrives labelled `main` and is refused (403); the Editor window's arrive labelled `editor` | Spike, `scheme-refused-label` | Scheme stays |
  | The Editor window's IPC | `window.__TAURI__` and `__TAURI_INTERNALS__` **exist** in it (`withGlobalTauri`), but an app command and `plugin:window\|set_title` are **refused** by the ACL on both. Tauri files the scheme's origin under "URL: local", so a capability naming window `editor` would grant it: none may | Spike, `tauri-*-command` lines | As planned |
  | Incognito windows | **WebView2: work** (storage gone in the next window; the persistent control keeps it). **WebKitGTK: the scheme never answers an incognito window** (navigation starts, no request reaches the handler, both orders). wry builds an incognito view on a fresh ephemeral context and registers schemes on it after making the view (`wry webkitgtk/mod.rs:259-381`), the likely cause. **A window with its own `data_directory`, new per session, works on both and is isolated** | Spike rounds `iipp`, `ppii`, `ddp` | M4: per-session data directory (deleted after the window is destroyed) instead of `incognito`, at least on Linux |
  | `on_download` | **WebView2:** `Requested` fires and refusing it saves nothing; two more downloads straight after raised no event at all (likely WebView2's "multiple automatic downloads" block; unconfirmed). **WebKitGTK:** in an own-data-directory window `Requested` fires and refusing works; in the shared default context it **did not fire** and a `blob:` download was saved to `~/Downloads` (deleted). Scheme-served attachments fail on WebKitGTK either way | Spike, `on-download-*` lines | M4: the per-session data directory (above), and the navigation rule refuses `blob:`/`data:` and every navigation but the page, which WebKitGTK does report (`editor-navigation`) |
  | ureq `Agent::with_parts` + pinned TLS 1.3 verifier | **Works on both.** A copy of `RustlsConnector` (about 80 lines) builds against `ureq::unversioned::transport`; only `TransportAdapter::set_timeout_at` is crate-private (`set_timeout` used). Right pin: 200 (first request 44-94 ms with the handshake, then about 1 ms on the pooled connection). Wrong pin: `invalid peer certificate: ApplicationVerificationFailure`. TLS 1.2-only port: `ProtocolVersion` alert. Record mode's SPKI SHA-256 equals `openssl`'s | Spike, `pinned-*` and `record-mode` lines | **ureq stays;** no hyper-rustls |
  | Peer addresses, Docker Engine | **IPv4 from another host: real address** (rootful: 10.77.0.2 seen as 10.77.0.2). **From the world's own computer over 127.0.0.1: the bridge gateway** 172.17.0.1 (rootful and rootless). Rootless (pasta) keeps the computer's own LAN address too; another host wasn't available here | `docker_peer.sh`: a container echoing its peer; on the runner a network namespace on a veth pair stands in for another computer | Per-address limits stay for IPv4; local clients share one bucket |
  | IPv6 through a publish, Docker Engine | **A `0.0.0.0` publish doesn't answer IPv6** (rootful and rootless). A bare `P:P` publish answers IPv6 on rootful Engine (not rootless here), but **every IPv6 peer arrives as 172.17.0.1** (docker-proxy into an IPv4-only container), so per-address limits fold for IPv6 | Same | `RATW_EDITOR_BIND=0.0.0.0` (IPv4) is the "open" value; section 3.2's `::` fallback is dropped (it would fold every IPv6 stranger into one address) |
  | Reserved ports vs 17789 | Runner: reserved only 80, 5985-5986, 47001; dynamic range 49152-65535; **17789, 7788 and 8765 free and bindable.** Hyper-V, VirtualMachinePlatform and WSL are enabled there, but no WSL VM or Docker Desktop ran, so Hyper-V's own reservations (taken from the dynamic range) never appeared | `windows_facts.ps1` (`netsh int ipv4/ipv6 show excludedportrange`, `show dynamicport`, a bind test) | 17789 stays (below the dynamic range); the user's PC with Docker Desktop running is the real check |
  | Docker Desktop: peers, IPv6, bind-mount `chown`/`chmod` | **Not measurable on the runner:** windows-latest has the Docker CLI and no running engine, and no Docker Desktop | `windows_facts.ps1` | User's PC (below) |
  | Studio with no `Saved/` or `artifacts/` | **Starts, serves the page and answers** (`/api/studio/ping` 200, the page 200, signed-out routes 401); creates neither directory. It still needs a dev world in the registry (section 4, M3's hosted mode) | `git archive` of 65a9f0e without `artifacts/`, `Editor/dist` added, `studio.py --host 127.0.0.1` on DEV | M3 as planned |
  | A dump's size (`pg_dump -Fc`, postgres:18's 18.6) | **DEV:** 304 MB whole (25 s); **51 MB without `world.build_cells`' data** (4 s); the build cells alone 252 MB for 3 builds (about 84 MB a build; 1,212 rows and 120 MB of row data each); without build cells and `world.terrain_chunks` (Atlas's source, not in a slice-1 package) 7.2 MB. **A packaged world** (doc 77's rev5 test world, one older build, few players): 85 MB whole, **0.8 MB without build cells** | `pg_dump` piped to `wc -c` | Q15's "build cells once each" holds: a daily dump is 1-2% of a whole one |
  | Largest DM and Atlas replies (DEV) | What the page asks for: `/api/live/world?lean=1` 3.06 MB raw / 0.61 MB gzipped (level 3, as `http_body.py`); a ground batch of the 32 largest cells (the route's maximum) 4.8 MB / 0.77 MB; `/api/dm/npcs` 1.0 MB / 0.14 MB; the rest under 0.2 MB. **But the full (non-lean) `/api/live/world` and `/api/world` are 235 MB raw / 45 MB gzipped (40 MB at level 9), over both 32 MB caps;** the page never asks for them | `measure_replies.py`: the methods behind each GET route, encoded as the handlers do | 32 MB caps stay; M3's allowlist must refuse the non-lean world routes (or force `lean=1`) |
  | A ticket's `gift` claim vs a world-set Gift | **Not overwritten.** The claim (`family`) is read only on the first arrival (`character_arrive`, `RatwGameTickets.cpp:401-481`) and the lobby; a returning wolf's hello refreshes only the look (`:336-360`). `character.gift` writes the wolf's own Gift (`RatwGame.cpp:1422-1452`) | Code read, not run | `character.gift` is meaningful on community worlds; section 8's table stands |

  **D1, decided:**
  - **The `editor` scheme, not shape (b).** It carries the studio on both platforms: bodies, raw and gzipped replies,
    the label check, the CSP and refused IPC. M4 takes three changes: the proxy sets `Content-Length` from the body;
    the Editor window gets a **new data directory per session** (deleted after the window is destroyed) instead of
    `incognito`, which WebKitGTK can't use with a custom scheme; and the navigation rule refuses `blob:`, `data:` and
    any navigation but the page, behind `on_download`, which didn't fire in WebKitGTK's shared context.
  - **ureq `with_parts`, pinned to `=3.5.1`;** no hyper-rustls client.
  - **Per-address limits stay, for IPv4.** Docker Engine shows other hosts' real IPv4 addresses. IPv6 peers through a
    publish all look like the gateway, so the Editor is opened with `RATW_EDITOR_BIND=0.0.0.0` (IPv4 only) and the
    guide doesn't offer `::`. Local clients share the gateway's bucket. Docker Desktop is unmeasured; the design
    already copes if it folds (section 3.2's residual).
  - **Port 17789 stays** (outside every reserved range on the runner and below the dynamic range Hyper-V reserves
    from), pending the user's PC.

  **Still needs the user's Windows PC** (Docker Desktop running, then `pwsh Desktop/spike-editor/windows_facts.ps1
  -Keep` in a checkout of `r2-editor-m0a`, and `http://<the PC's LAN address>:47789/` from a phone or a second
  computer): the peer address Docker Desktop shows for another computer, and for the PC itself; whether a `0.0.0.0`
  publish answers IPv6 there; `chown` and `chmod` from a root container into a bind-mounted Windows folder (the
  script writes, chowns, chmods and reads back, and prints the NTFS ACL); `netsh int ipv4 show excludedportrange
  protocol=tcp` with WSL2 and Docker Desktop up; and WebView2's second and third downloads with a real signed-in
  session.

- **2026-11-12: M4 built (The client),** on `r2-editor-m4` (cut from `r2-editor` at 3037b72, beside M2b and M3). Every
  item of section 14's M4 row, with M0a's D1 changes:
  - **The shell's Editor** (`Desktop/src-tauri/src/editor.rs` and `editor/`: `tls.rs`, `wire.rs`, `pins.rs`,
    `devices.rs`, `bundle.rs`, `proxy.rs`). **The pinned client:** `PinnedVerifier` compares the end entity's SPKI
    SHA-256 with the pin and ignores name and dates, checks the handshake signature (`verify_tls13_signature`), refuses
    TLS 1.2 outright, and records the key it saw in both modes; TLS 1.3 only, no session resumption; ureq pinned to
    `=3.5.1` with `Agent::with_parts` and the copied `RustlsConnector` (M0a's spike), no redirects or proxy, 15 s to
    connect. A name is resolved once in the shell and the checked address is the one connected to (central's
    `pingable` filter). **Record mode** sends only `GET /editor/hello`. **The hello** is checked (1 KB, a world id, a
    known kind; the name loses control, bidirectional and invisible characters and is cut to 40; progress is a known
    code and a percent, an unknown code reads "starting"; every other field dropped). **The pin store**
    (`editor-worlds.json`, 0600): entries keyed by the key's pin, with its SPKI, world id, name, last username and up to
    8 addresses as aliases; never a password, session or device token. Verdicts: trusted, moved (the same key at a new
    address), rotated (the hello's rotation names the old pin and this world, carries exactly the key shown, and the
    stored old SPKI's ECDSA P-256 signature over `rotation_message` checks), new, and changed (any other key at a known
    address whatever the hello claims, a known world id under a new key, or a known key claiming another world). An
    Editor code pins its key ahead of time (and replaces any other key kept for that world: a new code clears a red
    screen); its one-time password and invite stay in the shell, never the page. Forgetting a changed key needs the
    world's name typed. **Device tokens** in the OS keychain (`editor-device@WORLD/USERNAME`, `tokens.rs`'s pattern:
    a 0600 file where there is no keychain, memory with `--token-store memory`).
  - **The commands** (main window only): `editor_state`, `editor_connect` (screen 3's facts: the verdict, both
    fingerprints, where the address is (loopback, home or public), the checked hello; on failure `unreachable` (and
    whether the game's 7788 answers there), `game-port` (plain HTTP with a world's `/status`: stores nothing),
    `not-editor`, `package-old`/`client-old` by `editorApi`, `official`), `editor_use_code`, `editor_trust` (only the
    key the last connection showed, only with its fingerprint), `editor_forget` (a changed key by name, or a Recent
    world), `editor_sign_in` (strict pin, the account's device token, the code's secrets; one session at a time: another
    world or account signs the old one out and destroys its window; the same account on the same world rebinds the
    open window in place), `editor_change_password` (screen 4b sends only the new password: the shell keeps the
    one-time one while it must change), `editor_step_up`, `editor_accounts` (list and the eight verbs, only the fields
    each needs; a code the world makes comes back with this world's address added unless it is loopback),
    `editor_sign_out`, `editor_status` (the pill's hello over the pin), `editor_open`. A world's refusal reaches the page
    as its code, minutes and reason only, never its sentence. **The keep-alive** every 5 minutes, only with an Editor
    window open and the host's input in it in the last 30 minutes (an init script the shell injects pings
    `/.ratw-shell/input`, which never leaves the shell). Quitting signs out. A 401 anywhere ends the session in the
    shell (the token goes, the window stays bound) and the main window is told (`editor-session` event).
  - **The `editor` scheme** (`proxy.rs`): only the window labelled `editor`, only while bound to the current session
    (403 for another world's or account's window; 401, keeping the page's state, for an ended one); GET and POST;
    never `/editor/*`; paths without `..`, `//` or backslashes; on only the body (16 MB; Content-Length set by ureq from
    the body, D1), Content-Type and Accept (printable, one line), with the bearer and `Accept-Encoding: gzip` added;
    replies 32 MB on the wire and after gunzip (other encodings refused); `/api/` replies forced to JSON or
    octet-stream; page files must be on the signed list and hash to it, their type from their name; nosniff, no-store,
    no referrer and section 6.6's CSP on every reply. **The signed bundle** (`bundle.rs`): `bundle.json` = `{v, key,
    manifest (base64), signature (Ed25519 over the manifest)}`, manifest `{v, version (the commit's time), editorApi,
    files: {path: sha256}}`; checked once a session against `RELEASE_KEYS`, `OLDEST` (1,794,000,000) and the API.
  - **The Editor window:** label `editor`, opened at `editor://localhost/?suite=dm` (`http://editor.localhost` on
    Windows) through `WebviewUrl::CustomProtocol`, titled "RATW Editor: NAME (ADDRESS)" by the shell. **A new data
    directory per window** (D1, instead of incognito), removed after it is destroyed (WebKitGTK writes a few files again
    as it closes, so it is removed until it stays gone), and every leftover at start and at exit. **The navigation
    rule** (`page_origin_for`): main keeps to its page and never the Editor's origin; `editor` only to the scheme's
    origin (never blob:, data:, another host or port, D1); any other window nowhere; the window opens no other
    window (`on_new_window` denies). **Downloads:** `.json`, `.png` and `.csv` only, always through the shell's save
    dialog (`rfd`, GTK on Linux; no dialog plugin). **No capability names the window** (a test reads every capability
    file).
  - **The signed bundle in `Package/stage.sh`:** it builds `Editor/` (or takes `--editor-dist`), stages it into
    `app/Editor/dist` (the studio's `DIST` in the image) with `Data/Portraits` beside it and in the list as
    `portraits/`, and signs every file with the release key through `openssl` (Ed25519, `pkeyutl -rawin`); without a
    key it refuses (`--no-editor` stages an image whose Editor window never opens). `--sign-bundle DIR` signs a built
    Editor; `--make-release-key [PEM]` makes the key (0600, refused inside the repository or over an existing one) and
    prints the line for `RELEASE_KEYS`. **The release key: not made.** `RELEASE_KEYS` is empty, so a release build
    opens no world's Editor until the user runs `bash Package/stage.sh --make-release-key` (default
    `~/.config/ratw/editor-release-key.pem`, or `$RATW_EDITOR_RELEASE_KEY`), keeps a copy of the file beside central's
    keys, pastes the printed line into `Desktop/src-tauri/src/editor/bundle.rs` and releases the client. A debug build
    also trusts `RATW_EDITOR_DEV_KEY` ("ID:base64"), which the tests make per run; no private key is in git (the
    fixtures in `editor/testdata` are throwaway test keys trusted by nothing).
  - **The front door** (`Client/src/ui/editorDoor.ts`, `Client/src/net/editor.ts`, wired into `frontDoor.ts`):
    EDITOR under the pitch card on the sign-in page ("Run or build your own world.", central not needed), beside HOST A
    WORLD on the roster, and on each COMMUNITY row of the Host page (127.0.0.1 first, used when its hello's world id is
    the row's; otherwise screen 2 filled with the listed host and 17789; never on OFFICIAL rows). Screens 2-5 with
    section 2's words: the Editor code box (a code in the address box counts too), the address, "use 127.0.0.1",
    HOST A WORLD, Recent worlds with FORGET, an address central lists as official refused before connecting; "still
    setting up (N%)", polled every 5 s; can't connect by place (loopback, home with the `RATW_EDITOR_BIND` line, public
    with the router line), the game port, not an Editor, older package, official; moved and rotated notices; the first
    connection with all eight groups (I COMPARED IT from a public address, "this is the computer you're using" on
    loopback); the red screen with both fingerprints and no sign-in, FORGET THE OLD CERTIFICATE behind the typed name;
    screen 4 with the world drawn by the client, its account and password lines, the one-time-password hint while
    `setupPending`, every refusal; 4b (two boxes, checked before sending, the world's reason worded); screen 5 with the
    status pill, "Signed in as", "Certificate checked" and the fingerprint, DM (opens the window) and ATLAS greyed with
    its reason, Change my password, Sign out, Back. **Editor accounts** (owner): every action asks the password again
    in the page's own dialog (a step-up, then the change); the list (role, state, last sign-in and address, devices,
    claim address), add (name, role with its one line, "may act on players' wolves", the "they need the client and
    your port open" question), the code shown once with COPY, reset (not an owner's), role, wolves, turn off or on,
    remove, End all Editor sessions; "N failed sign-ins today" and the account banners, worded by the client. The
    session-ended event brings screen 4 with the account filled and "Your Editor session ended". The password boxes
    are emptied on every redraw. **NEW TOKEN's dialog** now says the world leaves the list until it restarts with the
    new token in `.env`. **EDITOR is hidden on macOS** (the page, and `AVAILABLE` in the shell) and in a browser.
  - **The Editor's hosted mode** (`Editor/src/lib/hosted.ts`: on the scheme's origin): no SignIn, no token
    (`authHeaders()` adds nothing, nothing in sessionStorage), the account from `/api/studio/session`, Atlas's world
    from it too and no `X-RATW-Editor`, a 401 shows "Your Editor session ended: sign in again from the Editor menu"
    over the page and keeps it, suite switches and reloads inside the page, the world pickers and Sign out hidden, the
    two game-opening links hidden.
  - **The widened text-only test:** `Editor/src/lib/textOnly.test.ts` (no markup sinks, eval or `cssText` anywhere;
    `window.open` and data-made links or sources only where hosted mode hides them, one listed exception: the
    artwork panel's `data:image/png` portrait) and `Client/src/ui/textOnly.test.ts` (the Editor screens and their
    words). **Not banned: data-derived `style`** (React sets it through the CSSOM and the shell's CSP blocks every load
    from elsewhere); section 6.6 asked for it, R1 may want it.
  - **A fix in M2a's helper** (`tools/world_admin.py`, `_prepare`): a device token the world had forgotten (a reset, a
    password change, End all sessions, the five-device cap) counted as a forced failure, so the client that kept it
    was refused for good with the right password. It is now no device at all (that client is a new device again); a
    token issued to another account is still unknown and a failure. `test_world_admin`'s password-change test says so.
  - **Tests** (one run at the end): `cargo test` **57** (the verifier against real rustls servers: the pin passes with
    any name and an expired certificate, a wrong key is refused before a byte, the pinned certificate without its key
    fails, TLS 1.2 is refused, record mode records; the hello and codes; the pin store's verdicts, rotation refusals,
    codes, forgetting, the file's fields and mode; device tokens; the bundle, an `openssl`-signed fixture from
    stage.sh's commands; every proxy rule; the commands against a stand-in world; the keep-alive's gate; the origins
    and downloads; no capability names the window); Client **158** and its typecheck (new: `net/editor.test.ts`,
    `ui/textOnly.test.ts`); Editor 78 + **70** and its typecheck and build; `Package/test_package.py` **23** (new: the
    bundle staged, every file listed with its hash, the signature checked with `openssl`, refused without a key, the key
    never in the repository; the stage tests pass `--no-editor`); `tools/test_world_admin.py` **80** (throwaway
    `ratw_test_s2stage_*`, dropped). **The end-to-end run** (`Desktop/tests/editor.test.mjs`, `npm run test:editor`;
    the real e2e build against `tools/world_admin.py`'s port on a throwaway database with every migration, through
    `Desktop/tests/editor_world.py`, whose stand-in app serves the signed bundle until M3's hosted studio): EDITOR with
    central down; the first owner's code, the address, no key screen; signed in with the code; 4b refusing a short and
    a breached password, then the new one; the menu "Open"; the pin store by key with the address, no secret in it;
    the device token 0600 in its own file; the accounts panel refusing a wrong password, then the list, a co-builder
    added and its code shown (no loopback address in it), the banner; DM opening the window with `/bundle.json`, `/`
    and `/api/studio/session` asked for under the owner's session; from the window `editor_state`, `central_call`,
    `world_status`, `plugin:window|set_title`, `plugin:window|set_fullscreen`, `plugin:event|emit` and the window-state
    plugin all refused; no bearer, device token or password anywhere in its page or storage; `/editor/accounts` 403,
    an unsigned file 404, `window.open` null, a navigation away stays home; the session ended under it (End all), its
    next request 401, the menu back on screen 4 with the account and the notice, signed in again and the same window
    carrying on (200); sign out, the window and its data directory gone; `new-certificate` signed: the pin moved
    quietly with the notice; `--leaked`: the red screen with both fingerprints and no sign-in (the shell refuses too),
    a wrong name refused, the right one forgets, the first-connection screen, trusted, signed in; no page error, no token
    in any reply the page got, no data directory left. No `s2stage` database, `/tmp` file or process left.
  - **Windows** (`desktop-windows.yml` on `r2-editor-m4`): run 43305720830 failed before any test
    (`STATUS_ENTRYPOINT_NOT_FOUND`: the test harness had no Common Controls v6 manifest, which only the app got, and the
    Editor's code links an entry point only v6 has); `build.rs` now embeds `windows-app-manifest.xml` into every
    binary through the linker (`WindowsAttributes::new_without_app_manifest`). Run 43306703409: 56 of 57, the keep-alive
    test set an `Instant` before the runner's boot; fixed. **Run 43307200798: success**, `cargo test` 57 of 57, the
    release build and its NSIS installer.
  - **Not done here:** "closing the window with saves pending asks first" and queued saves retrying (the page's own
    queue: Atlas, slice 2); the menu's "Open · N wolves" count and "Last backup" (the hello has neither: My world, M5);
    the owner's banners and failed sign-ins appear once the accounts panel is opened (the helper gives them only behind
    the step-up); doc 76's and 77's text (section 15). **For the user:** make the release key (above); the Windows run
    (`desktop-windows.yml` on `r2-editor-m4`) builds and tests the shell, but opening the Editor window on WebView2
    against a real world is the user's PC test, with a dev build and `RATW_EDITOR_DEV_KEY`, or after the release key.
    **For M3:** the image's Editor is `/opt/ratw/Editor/dist` with `bundle.json` and `portraits/`; the hosted studio
    serves those files (with a session), `/bundle.json` included, and `/api/studio/session` with `me` and
    `atlas.world`.

## Review (2026-11-08)

### The three reviews of the Editor rewrite (2026-11-08, night)

Three reviews of the rewrite: one on security (24 findings, most serious first), one checking every claim against the
`r2-desktop` worktree at 029d050 and the built `ratw-world:latest` image (26 findings), and one from a non-technical
Windows host's side, with scope and hours (22 findings). Some facts were rechecked before writing: the world service's
three mounts and no `admin` volume (`compose.yaml:42-46`), the `dm` schema's default grant to `ratw_game` (`0012:138`),
`trusted()` on every studio request, `/rebuild` connecting as editor, `withGlobalTauri: true`, and `openssl` present
in the built image. No code was changed and no world was touched.

**Changed: the shape**
- **The Editor port answers only the world's own computer by default** (Q1 now recommends (b), was (a)). The safety
  argument for (a) assumed IPv4 NAT; an IPv6 router, a laptop on café Wi-Fi and a fresh world's waiting one-time
  password all argued the other way. Managing from anywhere is still in v1; it takes one line in `.env`.
- **Editor codes** (both the security and host reviews): one string carrying the key, the username and a one-time
  sign-in, from `first-editor.txt`, the reset script, `ratw-editor` and screen 5. The owner's first sign-in and every
  co-builder's pin the key from it, so nobody compares hex by eye. Bare trust on first use is left only for an owner
  typing an address (Q4 now recommends (c)).
- **Only signed code runs in the client** (Q6). A hostile world could serve its own script under the rewrite's shape
  (a), because `script-src 'self'` is the world's origin. The bundle is now signed at staging and checked file by file
  by the shell, which keeps shape (a)'s no-drift benefit. Doc 77 section 9's bend is restated to match.
- **Each Editor window is bound to one world, account and session** (I10), incognito, and destroyed on any change, so
  a hostile world's page left open can't drive another world, and saves queued as one account aren't sent as another.
- **Account management moved out of the Editor window** to screen 5, the client's own, behind the password typed
  again. Owners are made only with `ratw-editor` on the world's computer (I13), so a stolen owner session can't make
  itself permanent. Every account change is logged and shown as a banner.
- **The key lives in a Docker volume of its own** (`admin`). The rewrite said the key "lives in the Docker volume";
  in fact `/var/lib/ratw` is the container's writable layer, so the key would have changed on every update and trained
  hosts to press FORGET. The same volume holds the helper's registry copy, settings copy, `central.json` and the
  "ever official" record.
- **Signed key rotation:** `new-certificate` keeps the old key and signs the new one with it, so clients move over
  quietly; the red screen stays for `--leaked` and lost volumes. Pins are kept by key, with addresses as aliases: the
  same world at a new address is recognised, and any new key at a pinned address is red whatever world it claims.
- **Lanes that strangers can't fill:** known devices, Editor codes and accounts on a one-time password; device tokens
  count only for their own account and are capped at 5; only failures count world-wide; the owner's name is
  generated. So a stranger can't block the owner's first sign-in, and a co-builder can't mint tokens to guess the
  owner's password.
- **Sessions:** a full world refuses the new sign-in instead of evicting someone else's (a co-builder could have
  signed the owner out); the keep-alive needs recent input, so the idle timeout means something; `last_seen` is
  written once a minute, not on every request.
- **Recovery for hosts without a terminal:** `reset-editor-password.bat` and `.sh` in the world folder; screen 4 says
  where the first password is; a "still setting up" hello; `first-editor.txt` emptied within seconds of use.
- **Restore keeps today's Editor accounts** by default and always ends every session.
- **The hours:** M2 split into M2a and M2b; about 0.45 times the plan for M2a, M3 and M4; slice 1 now about 35-45
  plan hours and 15-20 expected (was 27-35 and 11-13); both slices about 54-71 and 24-30.
- **Gates:** the desktop client tried on Windows joins doc 77's Phase 5 as a gate. EDITOR is hidden on macOS until
  tried (Q26).

**Changed: wrong or missing facts (the code review)**
- `dm.audit`, and any new `dm.*` table, were the game server's to read and write through `0012:138`'s default grant;
  the migration revokes them all and I11 makes it a rule.
- `trusted()` would have refused every request through the helper; hosted mode replaces it in all three handlers and
  Atlas's `editing()`.
- The proxy would have stripped Atlas's `X-RATW-Editor`, and refusing `/api/atlas/session` would have broken
  `sessionInfo()`; hosted `editing()` takes the session and the page skips `sessionInfo()`.
- `/rebuild` connects as editor, not through `studio.sync` as publisher; it is slice 2's, and the `ratw_dm` grant is
  for Close and Open only. The editor password does nothing in slice 1 and reaches the helper from slice 2.
- "Share my world" needs the owner role: a request file to the root entrypoint. Restart, backups and emptying
  `first-editor.txt` are request files too; backups reach the backup container through a shared `requests` volume.
- The helper can't read `world.toml`: the entrypoint writes it a cleaned copy and restarts it on `[editor]` changes.
- The agent's directory is writable by the server's user, so `kind` is now written where only `ratw` can write, and
  root records "ever official" once.
- The image already has `openssl`; it is now named in the Dockerfile so it stays.
- The window URL is `?suite=dm|atlas`, not `/dm/`; the page server has no `index.html` fallback.
- Replies are gzipped only on request: the shell asks and decompresses, and the cap applies after decompression.
- The proxy passes `Content-Length`; the DM suite's own limit is 64 KB.
- The navigation hook already receives the webview: that M0a item is dropped and the rule is specified.
- The pinned config uses `builder_with_protocol_versions` for TLS 1.3 only; ureq is pinned to `=3.5.1` because
  `Connector` is unversioned.
- The shell's token store is keychain-first: device tokens, which are credentials, go there.
- The baseline is 029d050, clean but for this doc; line numbers updated (`world_package.py:30-31` and `:818-823`,
  `studio.py:323`, `RatwGameTickets.cpp:681` and `:695-700`, `Central/central_service.py`).
- The Host page's shortcut tries 127.0.0.1 first and matches by world id, because the listed address is the public
  one and many routers can't reach it from inside.
- Preview ("open only to me") needs a valid host token; the rest of the Editor doesn't.
- `ratw-editor` reads the dm password as root, then drops to `ratw-admin`.
- The entrypoint loop is restructured to keep helpers up while the server is down.
- The helper's environment points `RATW_WORLDS_FILE`, `RATW_WORLDS_LOCAL` and `RATW_AI_CONFIG` at its volume.
- The Mind's ledger was in `LOGS`, unreadable to the helper and writable by the server: the tools count in
  `dm.ai_use`, and the ledger moves where only `ratw` writes.

**Changed: added (security and host reviews)**
- The fingerprint is shown in all eight groups, never cut short, in the same layout everywhere.
- The shell validates the hello; the title's address comes from the shell; the text-only test covers the Editor
  screens in `frontDoor.ts`.
- My world's settings view is an allowlist; log lines, the folder path and other accounts' addresses are owner-only.
- Separate pools for signed-in and not signed-in work; per-address connection caps; a token bucket; a large-body cap;
  catch-all handlers; fixed-size counters that fail closed; locks in `dm.locks`; IPv6 per /64; the Docker Desktop
  residual stated.
- Log limits on every compose service; folded failure lines; no audit row per unknown username.
- Incognito Editor windows, a downloads allowlist through the save dialog, a stricter CSP, forced content types for
  `/api/` replies, `/editor/*` refused by the proxy, and tests of Tauri's built-in commands from the Editor window.
- One-time passwords expire (72 hours for co-builders; 7 days for the owner, renewed on a start).
- A breached-password list, the world-name rule, "N failed sign-ins today", and `[editor] new_devices = "approve"`
  (Q23).
- With the Editor off, nothing listens; page files need a session.
- The official box publishes neither 8765 nor 17789; the user's studio binds 127.0.0.1 by default (Q25); the client
  refuses listed official addresses.
- I14 (no request value becomes SQL) and I15 (what logs never hold), with tests; record mode sends nothing secret;
  the `/status` fallback stores nothing; acting on players' wolves needs the owner or a flag (Q24); Publish limited to
  6 an hour an account.
- Host's side: the port-collision error quoted in the README with its fix; the firewall is per program, and Public
  networks block both ports; the phone-hotspot test; `localhost` tried as 127.0.0.1; the add dialog's plain role
  lines, default `dm` and "they need the client and your port open"; the "Don't have a world yet?" link; M6 also
  updates an existing doc 77 world and runs the second-world recipe; doc 76's "several windows" struck.

**Rejected, and why**
- **Showing all 16 groups of the fingerprint** (security review): eight groups are 128 bits, beyond any key-making
  attack, and short enough that hosts really compare them; the full hash is still what the client pins. The review's
  real point, never cutting it short and the same layout everywhere, is taken.
- **"Stay signed in on this computer" in v1** (host review): offered as Q22 and not recommended. An Editor session can
  hurt players, family PCs are shared, and a credential on disk would skip the password. Revisit after slice 1.
- **A compose profile so a port collision breaks only the Editor** (host review): compose can't publish one of a
  container's ports from a profile; it would need the helper in its own container with shared volumes for every
  request. The README's quoted error and fix cover it in v1 (section 13).
- **Central probing the Editor port** (host review, optional): it would tell central where each Editor listens. The
  phone-hotspot test is enough (section 13).
- **The 12-hour cap counting only while the Editor window is closed** (host review's fallback): a window left open
  for days would then never expire; the input-gated idle timeout and the 12-hour cap stay.
- **Recommending the bundled shape (b)** as the way to stop a hostile world's script (one of the security review's two
  options): the signed bundle does the same and keeps the no-drift benefit; (b) stays the fallback at D1.
- **Exempting one-time accounts from every limit** (a reading of both reviews): they skip the per-account lock and the
  world-wide failure count, but still count per address and still wait for the scrypt cap, so they can't be used to
  burn the world's CPU.

### The rewrite for the user's Editor design (2026-11-08, evening)

The user's design (quoted in Progress) replaced the way in. Mapped against the code at c495841 before rewriting; no
code was run.

**Replaced**
- **The manage key, its file in `manage/manage.key`, the folder dialog and the one-time code** (old sections 4.3 and
  4.5) are gone. Hosts sign in with an Editor account's username and password, typed in the client's main window.
- **"This computer or an SSH tunnel only"** (old Q1 and rule I2) is gone. The Editor port can be reached from anywhere
  the host lets it, and it is TLS only with a certificate the client pins on first use (section 6.4).
- **The central admin ticket** (old section 4.7, slice 3) is dropped: Editor accounts are the world's own, so the
  Editor works with central down, unlisted, delisted and on a LAN, and co-builders need no central account.
- **The fixed `host` account at role dm, and refusing the accounts command line** (old 4.4): reversed. The world makes
  an owner, the owner adds co-builders with roles, and `ratw-editor` manages accounts on the world's computer.
- **"Host must be 127.0.0.1"** (old browser rule): replaced by refusing any browser-shaped request (I9); the Host
  header is ignored.
- **The page in a window at `http://127.0.0.1:PORT/manage/`** (old shape (a)): the page still comes from the world's
  image, but through the shell's pinned `editor` URI scheme, which adds the session itself, so neither the password nor
  the token reaches the page. The page loading the world's address directly was rejected: macOS has no hook to trust
  the certificate, and the password would be typed into a world-served page.
- **MANAGE on the Host page and "Manage a world on this computer"** (old section 11): replaced by EDITOR on the main
  menu, with a shortcut on the Host page's rows.

**Added**
- Screen-by-screen flow (section 2) and the host's setup (section 3): the first owner, the port, forwarding, the VPS
  warning, the fingerprint, co-builders, `ratw-editor`.
- An internet-facing threat model (section 5): guessing, lockout, username probing, floods, first-use interception, a
  changed certificate, password reuse, a hostile world's page, a leaked folder.
- The limits today's DM login lacks: per-address and world-wide counts, a cap on concurrent scrypt, a dummy hash,
  device tokens so the owner can't be locked out, folded audit rows, an idle timeout (`last_seen`).
- Roles: `dm`, `builder`, `owner` (and today's `viewer`), never admin in hosted mode.
- New decisions Q1-Q5 (the port's default, roles, the first account, the fingerprint check, device tokens).

**Corrected fact:** the earlier draft said the studio "hands Atlas's write token to any GET of `/api/atlas/session`".
Under the studio every `/api/atlas/...` route first calls `builder()` (`studio.py:1312`), so it needs a dm sign-in;
only `map_editor` on its own hands the token to any trusted GET. Hosted mode still makes the token inside the session.

**Kept from the earlier draft:** the two slices; live changes (section 11); the My world tab; backups and restore
scripts; the map source in every package; Publish with preview and the fallback to the previous map; the cheats
settings; the privacy notices and filters; reports; AI limits; updates keep the world; official worlds locked.

### The four reviews of the first draft (2026-11-08)

Four reviews of the first draft: one checking every claim against the code (c495841 plus other sessions' uncommitted
changes), one on security, one from a non-technical Windows host's side, and one on scope and hours. No code was run
and no database was queried. Items marked *(superseded)* were overtaken by the Editor rewrite above.

**Changed: the shape**
- *(superseded)* Sign-in became a manage key in the world folder, not a central admin ticket. The user's design now
  replaces both with world-local Editor accounts.
- The page is served by the helper (now Q6), because a bundled page drifts from the world's API and needs a client
  release for every DM change. The bundled shape stays as the fallback, with its fixes.
- **The work is in slices.** "Run my world" ships first; "Edit my map" follows. Hours have plan and expected columns,
  two review rounds and a gate on doc 77's Phase 5. The stop-and-ask rule uses the expected column.
- **Recovery is designed, not assumed:** the helper stays up on any server exit; a new map that won't start falls back
  by itself; Discard unpublished map changes is core, not optional; restore scripts ship in the folder; a "something
  went wrong" table.
- **Preview before opening:** a `host_only` state lets the host walk a new map before players can.
- **The `server.restart {grace}` kind and the 1/5/15-minute choice are gone.** "When everyone has left", "now with a
  minute's warning" and "open only to me" cover it with less Core work.
- **Cheats are three settings,** because `--dev-tools` hands socket cheats to every player and `character.dm` needs a
  rank hosts never get. The flag is gated at the point of use, and rewards to the host's own wolves need cheats on.

**Changed: wrong facts**
- Close and Open need UPDATE on `admin.server_state`, which only publisher had: a narrow grant to `ratw_dm` instead.
- The helper couldn't read the registry, the AI config or the agent's files: it gets its own copies in `STATE/admin`
  and joins no group (which would also have opened players' voice lines).
- The agent's directory is writable by the server's user, so the helper no longer trusts anything in it.
- The unpublished count would have counted NPC and economy edits: it now counts map-key edits after `edits_through`.
- NPC Life already goes through actions; its row is what survives a restart. The "misleads" claim and its table row are
  gone.
- Every tool is already in the image; dropping one would need a stage.sh exclusion.
- The entrypoint can't tell official from community: the package simply can't run an official world, and the agent now
  records central's `kind`.
- Port 7789 was the README's second-world game port: the helper's default is 17789.
- `world_build.build` takes an editor connection, and the studio's rebuild route is reused.
- Pairwise ids don't stop players being linked across worlds (wolf names are unique), and hosts see addresses: the
  threat model and section 9 now say so, and players are told.
- Smaller fixes: the action kinds (about 50, not 70), the extra one-minute poll, where economy writes live, `ratw_dm`'s
  extra writes, `trusted()`'s lines, the full route paths, `/api/studio/session` at page start, the Windows origin,
  `--dm-directory` (the server writes there; nothing reads it), the session code that is new, the checkpoint citation,
  Vite's `base` already set, and where the client's size was measured.

**Changed: added**
- The world folder's `manage/` and `backups/`; backups on start and every 24 h, build cells dumped once each, capped
  pins, manifests, a free-disk check, the synced-folder warning; restore's refusals and safety dump.
- Publish's validation now covers client limits, orphans, constraints, live references and the checkpoint, and is shown
  up front as "can this computer publish", with the `.wslconfig` steps.
- Updates: the previous build format read, the memory check before migrating, the host asked before unpublished
  changes go live, an add-only seed step (Q17), and "Reset economy rules" to the world's own original.
- Host-side fixes: `world.toml` re-read before each start, NEW TOKEN's warning, message order, confirmations on
  players' wolves, the no-key AI state and a monthly budget, a 401 that keeps queued saves.
- Privacy: plain disclosures, the reporter hidden, reports about the host from the reporter's client, author columns
  and the user's names in the export scan, unreleased content checked, deletions against backups.
- "Share my world", untrusted imports, the "Not in v1" list, the official box's later remote DM left open, and the DM
  tool on central ids moved here from doc 77 Phase 7.

**Rejected, and why**
- **Using the package's new build after an update when the host never edited.** The new package's build is a different
  map from the world's live rows and checkpoint, and updates never apply a newer package's world. Reading two build
  formats does the job safely.
- **A public "host actions this week" count on `/status` and the listing.** It needs central and listing work, and a
  modified image would lie. The in-game notice to the player, and the cheats rules, cover it in v1.
- **Per-world wolf ids** (offered as Q9 (a), not recommended): wolf names are unique and shown, so they would hide
  little.
- **Giving the helper the publisher password** for Close and Open: publisher also writes builds and runs the push; a
  column grant to `ratw_dm` is narrower.
- **Skipping build cells and rebuilding on restore:** a restore would then need Publish's memory, which a small PC may
  lack. Dumping each build once does better (Q15).
- *(superseded)* **Remote Manage over TLS in v1** was rejected then; the user's design puts managing by address in v1,
  over TLS with a pinned certificate and world-local accounts instead of the central ticket.
- *(superseded)* **Refusing to start the helper under host networking:** moot now that the port isn't loopback-only.
- **Cutting the unpublished counter and the Live now / Waits for Publish badges outright:** they move to slice 2,
  where Publish exists, instead.

**Already true in today's code:** `passwords.env` is now root-only (`compose.yaml:93`, `chown 0:0`, mode 0600), so the
security review's point about group `ratw` reading every password is fixed by another session's change. A test keeps
it so.
