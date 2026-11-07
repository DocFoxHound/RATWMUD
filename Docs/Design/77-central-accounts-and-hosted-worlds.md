# 77. Central accounts and hosted worlds (2026-11-07)

Drafted 2026-11-07, revised the same day after a review (see "Review" at the end). **Agreed 2026-11-07** with every
recommendation in section 14, except Q10: central runs on its own server, which the user sets up later (see Progress).

The user, 2026-11-07 (verbatim):

> I want to ship it as a dedicated server that players can run on their own machines with minimal setup, but there's
> one major difference because I want to split player characters away from the database and to a new one. The database
> I want to ship needs to have NPC's and the map, and of course things that go with that like the stories and whatnot.
> What I want to separate into a new, centralized server are player characters and account information. Essentially my
> setup will be this:
>
> 1. A game client
> 2. A dedicated server client so players can create/host their own worlds
> 3. A centralized database that retains the truth of player characters and their accounts.
>
> The game client will essentially launch to a character select/customizer/creator, and then have a menu select for
> whatever dedicated servers are presently live and connected to it. So on my centralized server I need to have a way
> for servers to reach out and tell it that a dedicated server is live and to offer that as an option for players to
> connect to.

Decided by the user on 2026-11-07, before this draft:
- **Two kinds of server.**
  - **Official servers**, which the user runs, share a character's progress through central.
  - **Community servers**, hosted by players, keep progress per world. Central holds only who the wolf is and how they
    look.
- **NPC voices on a community server** use the host's own AI key, or the offline lines when there is none.
- **The client** is "Run Against the World". It is tested on a Windows PC and this Fedora laptop, with no Mac.

This doc is a plan only. No code was changed. The code is `ridgemere2` at 0af9bc0. It builds on doc 76 (the desktop
client) and changes one of its decisions (section 9). The findings below come from a read of the code and from
research into how other games do this (Minecraft, Steam, Factorio, Space Station 14, Veloren, Valheim, Rust, ARK,
Diablo II). The sources are listed in section 13.

Words used the same way throughout:
- **central:** the new service and its database. It holds accounts, wolves, official progress and the server list.
- **a world:** one running dedicated server and its own database (map, NPCs, stories, economy, and per-world progress).
- **a wolf:** a player character's identity, held by central: name, sex, look, Gift, build and roleplay profile.
- **a ticket:** a short-lived pass from central that lets one wolf join one world.
- **a host:** a player running a community world.

Standing rules, kept:
- Characters are male or female only.
- Never the word for the domestic animal in anything players read.
- Nothing writes PROD.
- No data migrations, because the worlds are test worlds. DEV has 0 accounts and 0 characters today.

## Headline

- **Three parts:** the client, the world server (today's `ratw_server`, shipped to hosts), and central (new). The
  client signs in to central only. It never gives a world its password.
- **Joining works like Minecraft and Space Station 14.**
  1. The client asks central for a ticket for one wolf on one world.
  2. Central signs the ticket (Ed25519) with that world's id inside it. The ticket lives about 60 s and can be used
     once.
  3. The world checks the ticket itself with central's public key.
  4. The wolf's name and look come from the ticket, so a host can't change them.
- **The server list works like Factorio and Space Station 14.**
  - A host makes a host token on central and puts it in the server's config.
  - The world sends central a heartbeat every 60 s and drops off the list after 3 minutes of silence.
  - Central calls the world back to check it can be reached, before listing it.
  - Only central can mark a world "official".
- **Progress lives where the trust is.**
  - A community world keeps everything about a wolf (skills, purse, home, standing, crimes) in its own database, keyed
    by central's wolf id. It starts fresh on the wolf's first arrival.
  - Official worlds keep what Q1 chooses as shared progress on central, behind a lease, so only one official world can
    write a wolf at a time.
- **The client carries the game page itself.** This changes doc 76's thin shell, because a host's machine can't be
  trusted to serve the page (section 9). The client checks each world's protocol version and hides worlds it can't
  talk to.
- **The world package is a Docker Compose bundle for v1.** It holds `ratw_server`, Postgres, the NPC Mind and a world
  export with all player data stripped: map, NPCs, factions, voices, stories, economy seeds and `Data/`.
  - The server is Linux-only today (epoll, `fork`, `flock`). Windows hosts run it through Docker Desktop.
  - A native Windows server is a later, larger step (Q2).
- **Size:** about 65-85 hours over nine phases, with a spike first (revised up by the review; see section 10). The
  smallest version that plays end to end is Phases 0-3 plus a bare list (4a), about 36-46 hours.
  - That version is: sign in to a central running on this laptop, make a wolf, see a list holding a world marked
    official, join it, play. A second world on this laptop is listed beside it. Nothing faces the internet until
    Phase 7.
- **Today's servers keep working unchanged.** Ticket mode is opt-in (`--central URL` plus a host token). Without it,
  DEV, PROD, scratch servers, ctest and the client tools keep today's loopback accounts.

## 1. What it is, and why

Today one `ratw_server` holds a whole world, and that includes its accounts.
- Passwords, characters, purses, homes, friends and stars all sit in the world's own Postgres database (`game.*`).
- Passwords are taken only from this computer (`Server/ratw_server.cpp:422`, `Core/RatwGame.cpp:2343-2347`).

So nobody can play from elsewhere, and no one else can run a world.

The user wants three things:
- players can run their own worlds;
- a player's wolf is the same wolf everywhere;
- a world can be found from inside the client.

That needs the accounts and the wolves out of the world and into one central service, and a way for a world to say
"I'm up".

Other games split it the same way. Central holds who you are, and at most your cosmetics. The world holds what you do
there.
- Every game that shares progress across servers either keeps that progress on its own servers (Diablo II's closed
  realms, ARK's official clusters, Rust's cosmetics) or accepts cheating (Valheim and Diablo II's open play, where
  characters live on the player's machine).
- The user's "two kinds of server" is the Diablo II and ARK pattern. A community world can never write to a wolf's
  shared progress.

## 2. What the plan rests on (today's code)

**Accounts**
- `accounts::Accounts` (`Core/RatwAccountsCore.h:52-97`): salt, verifier and up to 6 character ids per username.
- Hashing is PBKDF2-HMAC-SHA256 at 600,000 rounds on hasher threads, with OpenSSL's libcrypto loaded at run time.
- They are saved in the world's checkpoint and in `game.accounts`.
- There are no session tokens. The account is bound to the WebSocket, and a disconnect signs out
  (`RatwGame.cpp:1991-2006`).

**Account-level tables, per world** (keyed by username)
- `account_profiles` (the handle), `account_standing` (earned Gift tiers, doc 49), `friendships`, `friend_requests`,
  `private_inbox`, `circles`, `star_tallies`, `stars`, `safety_marks`, `storytellers`, `storyteller_log`,
  `story_books` (the author), `vouches`, `reports`, `artwork`.
- Most are `key, position, data jsonb` mirrors of checkpoint sections. The username sits inside `data` (generated
  columns such as `stars.giver_account` read it out). So the change is in the C++ that writes those sections, not in
  the tables.

**Characters**
- `Entity` (`Core/RatwWorld.h:236-382`), saved by `wire::persistEntity` into the checkpoint and `game.characters`.
- Ids are `wolf-<32 hex>`. `playerAccountId` (`Core/RatwSociety.cpp:18-24`) enforces that shape (and also accepts
  `player-<id>` for `--dev-identity` wolves), so central keeps it.
- The purse and goods are not on `Entity`. They are `society.accounts[charId]` in the world's conserved economy
  (`Core/RatwSociety.h:31-35`, `Society::conserved`).
- About 45 `game.*` tables and many checkpoint sections name a character or account by text id. There are no foreign
  keys, so a missed one fails silently. Because central keeps the `wolf-` id shape, everything keyed by character id
  needs no change; only what is keyed by username does.
- A world can't delete a character today ("Deletion is not available", `RatwGame.cpp:2561`). Central's account
  deletion needs one (section 12).
- A world holds at most 8,192 player economy accounts (`MaxPlayerAccounts`, `RatwSociety.h:50`), and one is made for
  every wolf that ever arrives. After that, every new arrival fails.

**Creation is tied to the world** (`RatwGame.cpp:2437-2612`). It does all of the following:
- checks the build (`practice::checkBuild`), the appearance (`wire::readAppearance`), the Gift family and the name,
  all in C++;
- picks a start town;
- makes a tie to a local mentor;
- gives the welcome (`Society::addPlayer`, `RatwSociety.cpp:368-382`): 20 pennies and 2 herbs moved from the world's
  treasury, and 3 meals made new;
- gates a Gifted or Quickened tier on the account's earned standing in that world (`RatwGame.cpp:2467-2479`).

**The client** connects to `ws(s)://<page host>/ws` (`Client/src/net/connection.ts:27-30`). The link has no protocol or
version field (`Core/RatwLink.h`).
- The page bakes in parts of `Data/` at build time: the terrain catalogue (`Client/src/game/terrain.generated.mjs`,
  from `Data/Terrain/terrain.json`), the walking rules (`walk.wasm`, from `Core/RatwStep`), the portrait sheets and
  the fonts (`Client/vite.config.ts:5-18`). A world whose own copy differs refuses the page's moves.
- The server accepts a WebSocket only when `Origin` is absent or matches its own `Host` (`Core/RatwWeb.cpp:121-133`).
  A page bundled in the client (`tauri://localhost`, `http://tauri.localhost`) is refused with 403 today.

**The server**
- It is Linux-only: epoll and eventfd (`ratw_server.cpp:56, 66, 148-150`), `fork` snapshots
  (`RatwGameJournal.cpp:373`), and `flock` (`RatwGameOwner.cpp:78`).
- It listens on IPv4 only (`sockaddr_in`, `ratw_server.cpp:409-419`). Many hosts behind carrier-grade NAT have
  reachable IPv6.
- It has no HTTP client. The only outgoing call is to the Mind on literal 127.0.0.1 (`Core/RatwMind.cpp:71-98`).
  libcrypto is loaded at run time for PBKDF2 and SHA-1 only (`Core/RatwSystemLibs.cpp:48-58`).
- It already has an operator bridge, a private directory of requests and receipts plus a `snapshot.json` of the world
  (`--dm-directory`, `Core/RatwDirector.h`).
- Postgres 18 runs in Docker (`Database/compose.yaml`). A file mode (`--world-export DIR --save F`) needs no Postgres,
  but it drops voices, dialects, spawns, story visitors, the event log, artwork and reports.
- Migrations, connection strings and the database roles come from `tools/world_db.py` (Python, psycopg).

**What waits on a DM.** Several game features wait for a decision made in the DM tool: house requests, treaties,
storyteller applications, artwork review, report decisions and account unlocks (`ACTIONS`,
`tools/dungeon_master.py:150-180`). A community world with no DM tool would leave them waiting for ever.

**What uses passwords.** ctest (`Tests/game_tests.cpp`, `friends_tests.cpp` and about ten more), the client's tests and
`tools/client/scenario.ts` and `newcomer.mjs` all register and sign in with passwords.

**The world as data**
- DEV is 674 MB in all.
- An exported build is about 307 MB (284 MB of it cells) and compresses to about 24 MB with zstd.
- The NPC tables (`live.*`) and `Data/` (13 MB) come on top.

**NPC voices**
- The server talks only to the local Mind (`tools/npc_mind.py`, `Core/RatwMind.cpp:71-98`), and only at the literal
  address 127.0.0.1: another container's address is refused.
- The Mind reads the key from the config file it is given (`--config`; today `Saved/Config/RATWNPCAI.local.json`). With
  `--database` it reads the event log through `tools/world_db.py`, so it needs psycopg and the registry.
- With no key, the server falls back to the Tracery talk library and authored lines. Community hosts already get the
  "host's own key" choice for free.

## 3. The shape

```
                 HTTPS (sign in, wolves, list, tickets)
   Client  ─────────────────────────────────────────────►  Central (one box, behind Caddy)
  (Tauri,                                                    ratw_central + Postgres "ratw_central"
   page bundled)                                             accounts · wolves · tickets · server list
      │                                                      official progress (leases) · bans · keys
      │  WebSocket: hello{ticket, protocol}                          ▲
      ▼                                                              │ HTTPS heartbeat (host token)
   World (ratw_server + Postgres + Mind)  ──────────────────────────┘ official worlds: checkout / save / checkin
   map · NPCs · stories · economy · per-world progress
```

### 3.1 What a player sees

1. **Sign in.** The client opens on central's sign-in page, which is bundled in the client, never served by a world.
   - A new player registers there.
   - "Remember me" keeps a refresh token in the shell (the OS's keychain), never in the page's own storage, so a
     script slipped into the page by a hostile world can't take it. The password is never stored.
2. **Your wolves.** The roster comes from central: up to 6 wolves, as today.
   - **New wolf** opens the creator, on its identity tabs: Body, Coat, Markings, Eyes, Gift, Strengths, Name & age.
   - Today's Arrival and Tie tabs move to the world (step 5). If Q12 goes as recommended, so do the Gift tier and
     Strengths, and the Gift tab on central keeps only the family.
   - Today the creator's rules (the strengths' grades, budget and presets, the start towns, the ties) come from the
     world's lobby (`frontDoor.ts:91`). Central must serve the identity rules instead, from the same `Data/`.
3. **Worlds.** A list from central. Each world shows:
   - its name and description;
   - official or community;
   - players online and their limit;
   - region;
   - ping, which the client measures itself;
   - whether NPC voices use AI;
   - a lock if it is private.

   Worlds on a version this client can't talk to are greyed out with "needs an update", or hidden. A direct-address
   box is there for LAN or private worlds.
4. **Join.** The client gets a ticket for that wolf and that world, and connects.
5. **First arrival in a world.** The world asks where the wolf arrives and who they know (today's Arrival and Tie
   tabs, and under Q12 the tier and strengths), opens their purse with the welcome, and places them. Next time, they
   go straight in.
6. **Leaving** goes back to the world list, without signing in again.

### 3.2 What a host does

1. Signs in to central (the client's Host page, or a small web page on central) and makes a **host token** for one
   world. They can have up to 3 worlds per account.
2. Downloads the world package, unzips it and edits `world.toml`:
   - the world's name, description, public or private, and player limit (a private world's password is set on
     central, Q9);
   - the host token;
   - an optional AI key.
3. Runs `docker compose up` (Linux, or Windows with Docker Desktop).
   - The world loads its package, checks it can reach central, and starts its heartbeat.
   - If central can't call it back, the host sees "Your world can't be reached from the internet: open port 7788
     (guide)".
4. Moderates their own world: a host-only kick, a world ban by wolf id, and a world-local silence. Central's
   account bans apply everywhere.
   - What waits on a DM today (section 2) needs an answer on a community world. v1: house requests, treaties and
     storyteller applications are decided by the host with in-game commands; portrait uploads are off (the painted
     portraits stay); reports go to central (`POST /reports`), not to `game.reports`.

## 4. Central

### 4.1 What it holds

| Kind | What | Notes |
|---|---|---|
| Accounts | Account id, username (unique), password hash, optional email (for recovery only), created, birth-year check passed, flags (banned, host, admin) | Reuses today's rules (usernames 3-32, passwords 12-128). Central's store is new, so the hash needn't be the world's PBKDF2: `world_db.hash_password` (scrypt, already used for the DM tool's accounts) is ready in Python. Run off the request thread. |
| Sessions | Refresh tokens (hashed), 30 days, revocable | Access tokens last 15 minutes. |
| Wolves | `wolf-<32 hex>` id, account, name (unique across central, never changed), sex and appearance, starting age, Gift family, build grades and specialty (with the tier: Q12), roleplay profile (`game.profiles` today) | The wolf's identity. Signed into every ticket. Uploaded portraits stay per world in v1 (`game.artwork`, reviewed in the DM tool), so central needs no image moderation yet. |
| Official progress | Per wolf: the shared progress Q1 picks, a version number, saved history (last 20), and the lease (world id, since) | Only official worlds can read or write it. |
| Worlds | World id, host account, host token (hashed), kind (official/community, set by central), listing fields, last heartbeat, reachable, online wolf ids | Listed while heartbeats arrive. |
| Bans and reports | Account bans, world delistings, reports from players | One place for the user to act. |
| Keys | Ed25519 ticket keys with key ids, rotated; the public set at `/keys` | The private key never leaves central. |

These stay **per world**, as today, keyed by central's ids: everything in the "tied to this world" column of the code
read. That is position, purse and goods, home and lodgings, contracts, quests and storylines, chapters, parties,
letters, favours, NPC regard and memories, rumours, fame, crimes and warrants, map memory, known wolves, ties and
companions.

The account-level social layer (friends, circles, stars, private messages, safety marks, the handle) belongs on
central eventually, because a friend is a friend in every world. v1 keeps it per world (Q5).

**The Gift tier at creation.** Today a Gifted or Quickened wolf needs standing earned in that world (doc 49), and the
tier is paid for out of the same points as the strengths. With creation on central, central has no standing to check:
it is per world, and only official worlds would share it (Q1). So the tier can't be part of a central identity as
things stand (Q12):
- (a) the family is identity (central); the tier and the strengths are chosen at first arrival in each world, gated
  by that world's standing as today;
- (b) every tier open at creation (today's `--open-tiers`), which drops doc 49's gate.

**Checking an identity.** The rules for a valid name, appearance, build and Gift are C++ (section 2). Central must not
copy them into Python, or a wolf central accepts could be refused by a world. A small `ratw_identity` command built
from Core checks a wolf for central, which runs it as a subprocess. The world checks the ticket's identity again with
the same code, in case its `Data/` differs.

### 4.2 Its interface

All calls are JSON over HTTPS.

| Call | Who | What |
|---|---|---|
| `POST /account/register`, `/account/login`, `/account/refresh`, `/account/logout` | client | Sign-in. Rate-limited per address and per account. |
| `DELETE /account` | client, or a web page | Deletion within 30 days. GDPR needs a way that doesn't require the game. |
| `GET/POST/PATCH /wolves` | client | List, create, and change the parts allowed to change (profile, look). |
| `GET /worlds` | client | The list, filtered to listed and reachable worlds. |
| `POST /tickets {world, wolf}` | client | Returns a signed ticket. Central counts tickets issued per world, and limits them per account and per world. |
| `GET /keys` | worlds | Ticket public keys by key id. Fetched at start, daily, and when a ticket names a key id the world doesn't know (at most once a minute). The next key is published a day before it is used. |
| `POST /worlds/heartbeat` | worlds (host token) | Listing fields plus online wolf ids. Every 60 s. The reply carries central's ban list changes, the wolves deleted on central (section 12), and central's clock. |
| `POST /worlds/leave` | worlds | On a clean shutdown. |
| `POST /progress/checkout`, `/save`, `/checkin` | official worlds only (world credential) | Section 6. |
| `POST /reports` | client, worlds | A player reporting a wolf or a world. |

### 4.3 What it's built in

- **Recommended: Python**, beside `tools/studio.py` and `tools/dungeon_master.py`, which reach Postgres through
  `tools/world_db.py` (psycopg). It runs as one process behind Caddy on the official box, with its own database
  `ratw_central`. Identity checks go through `ratw_identity` (4.1), so the rules stay in one place.
- **Load:** 1,000 players is a few sign-ins a second at worst, so speed doesn't decide this. Each sign-in is a slow
  hash on purpose (about 0.2 s for today's PBKDF2), so a thousand at once after an outage is minutes of CPU: rate
  limits must queue sign-ins, not fail them, and "remember me" refreshes skip the hash.
- **The other choice is C++,** sharing `Core/RatwAccountsCore` (Q4). The ticket check inside the world is C++
  whichever is chosen. OpenSSL's libcrypto verifies Ed25519, so no new library is needed, but the run-time loader
  (`Core/RatwSystemLibs.cpp`) has to bind about six more functions (`EVP_PKEY_new_raw_public_key`,
  `EVP_DigestVerifyInit`, `EVP_DigestVerify` and their frees), and the world needs base64url and a strict parser.

## 5. Joining a world: the ticket

A ticket is a compact signed token: JWT with the `Ed25519` algorithm (RFC 9864), or PASETO v4.public. Its fields:

- `sub`: the account's id for this world; `wolf`: the wolf id;
- `name`, `sex`, `look`, `gift`, `build`: the wolf's identity;
- `aud`: the world id; `kind`: official or community;
- `jti`: a random id; `iat`: when it was issued; `exp`: about 60 s from now; `kid`: the key id.

`sub` on a community world is a per-world id (a keyed hash of the account id and the world id, as OpenID's "pairwise"
ids), so hosts can't link one player's wolves across worlds. Official worlds get the real account id, which they need
to share standing.

1. The client connects to the world and sends `hello{ticket, protocol}` as its first message.
2. The world checks the ticket:
   - the format and algorithm are exactly the one expected: Ed25519 only, and any other `alg` (`none` included) is
     refused before anything else is read;
   - the signature, against the key named by `kid`;
   - `aud` is its own id, and `exp` hasn't passed, with 2 minutes' leeway for a wrong clock. Home machines drift,
     Docker Desktop's virtual machine especially after sleep. The world logs its offset from central's clock, which
     each heartbeat reply carries;
   - `iat` is after the world started (allowing for the clock offset), so a ticket seen before a restart can't be
     used again after it;
   - the `jti` hasn't been seen (it remembers each until `exp` plus the leeway);
   - the identity passes the world's own checks (4.1).

   If it passes, the wolf is in. No password, and no call to central.
3. **Ticket mode is opt-in.** A world is in ticket mode only when started with `--central URL` and a host token.
   Without them it keeps today's username-and-password path from loopback, unchanged, so DEV, PROD, scratch servers,
   ctest and the client tools go on as now. There is no `--local-accounts` flag, because that is the default.
   - In ticket mode the server refuses to start with `--dev-identity` (anyone reaching it through a tunnel or a
     same-box proxy arrives from loopback, doc 76 section 5), and with `--dev-tools` or `--open-tiers` on an official
     world. A community world with dev tools is listed with a "cheats on" tag.
   - In ticket mode the loopback rule grants nothing: no passwords at all.
4. **Why offline checking:** a world keeps admitting new tickets central issued during a short outage of the world's
   own link to central, and central isn't in every join's path. If central itself is down, no tickets are issued, so
   nobody can join any world until it is back. The cost of offline checking is that a ban takes up to one ticket
   lifetime (60 s) plus the next heartbeat reply to reach a world. That's acceptable.
5. **Why the world id is inside the ticket:** a dishonest host can't take a ticket handed to their world and use it to
   join an official one as that player.
6. **Over plain `ws://`** (section 9) anyone on the network path (the same café Wi-Fi, say) can read the ticket and the
   whole session, and can take the session over: act as that wolf in that world. They can't use the ticket anywhere
   else. That's the real meaning of "not encrypted" in the list.
7. **A dropped connection** today takes the wolf out of the world. A reconnect needs a new ticket, and so central. A
   later step: the world gives the client a short resume token of its own (a few minutes, world-local), so a blip
   doesn't need central.

### 5.1 What changes in `ratw_server`

- `hello{ticket}` replaces `auth_register` / `auth_login` / `character_create` in ticket mode.
  - An account entry is then `{central id, wolves seen here}`, with no salt or verifier. `Accounts` holds one kind or
    the other, by mode; a world never mixes them.
  - `Accounts::referencesOnly` and `restore` check against that.
- **Creation splits in two.**
  - The identity comes from the ticket.
  - A new `character_arrive{start, tie}` (plus the tier and build under Q12) runs the rest of today's
    `character_create` once per world: the start town, the tie, `Society::addPlayer` (the welcome) and the creation
    receipt.
  - Each first arrival costs the world an economy account and a welcome (section 2's 8,192 limit). So arrivals are
    rate-limited per account and per world, and a wolf that never comes back has its account folded back into the
    treasury after a season (conserved: its money and goods return, they aren't deleted).
- **Identity is refreshed on every join.** If a wolf's look or profile changed on central, the world takes the
  ticket's version. The name never changes (residents remember it, `known_wolves`, nicknames, veiling).
- **The account-level tables** key on central's account id instead of the username. This needs no data migration
  (no accounts exist in DEV), only the C++ that writes those sections' `data` (section 2).
- **The Origin check** (`Core/RatwWeb.cpp:121-133`) stays in password mode, where it stops a web page in the player's
  browser from using a loopback sign-in. In ticket mode any origin is accepted: nothing ambient rides on the socket,
  and the bundled page's origin (`tauri://localhost`, `http://tauri.localhost`) would otherwise get 403.
- **No page is served in ticket mode** (`--web` refused). Only `/ws` and `/status` answer. A host can't serve a page
  that looks like the game.
- **A protocol number** goes in `hello`, in the lobby, and in a small `GET /status`:
  - world id, name, protocol, build, the client-data hash, players, limit, voices on/off, private flag;
  - capped at 2 KB, answered by the network thread from counters the game thread updates (no game-thread work),
    rate-limited per address, with `Access-Control-Allow-Origin: *` (it is public);
  - served on the game port for central's call-back and the client's ping.
  - **The client-data hash** is made at build time from what the page bakes in (section 2: terrain catalogue, walking
    rules, packed formats). The client hides a world whose hash differs from its own, as for the protocol. A host who
    edits `Data/Terrain` drops off the list rather than breaking every player's walking.
- **The heartbeat, `/keys` and the ban list** run outside the C++ server, because it has no HTTPS client (section 2).
  Recommended: a small Python "world agent" in the server's container that posts the heartbeat, fetches `/keys` into a
  file the server reads, and passes ban lists and deleted wolves to the server through the operator bridge
  (`--dm-directory`). The other choice, an HTTPS client in C++ (libssl bound at run time, certificates checked), is
  more code in the game process and in its `fork` snapshots.
- **Bans:** central's account bans from the heartbeat reply, plus the host's own world bans, are refused at `hello`.
  A wolf already in the world when a ban arrives is sent out.

## 6. Official worlds: shared progress

Official worlds hold a **world credential**: a host token marked official by central, kept only on the user's boxes.

1. **Checkout.** When a wolf joins an official world, the world calls `checkout{wolf}`.
   - Central grants a **lease** if no other official world holds one, and hands back the shared progress and its
     version.
   - If another world holds a lease, central refuses: "this wolf is in another official world". A lease older than
     10 minutes with no save is taken as abandoned.
2. **Save.** While the wolf is in the world, it calls `save{wolf, version, progress}` on the world's own save rhythm,
   and when the wolf leaves.
   - Central keeps the last 20 saves.
   - It refuses a save whose version isn't the latest.
   - It applies **sanity limits**: skill gains per hour played, and the most a stat can change. A save over a limit is
     stored but flagged for the user, not applied.
3. **Checkin** frees the lease when the wolf leaves.
4. **A world that can't save for 10 minutes sends the wolf out.** Otherwise a world cut off from central keeps playing
   a wolf whose lease another official world has since taken, and that play is lost when its stale save is refused.

The world credential can write any wolf's shared progress, so it is bound on central to the official boxes' own
addresses: used from anywhere else, it is refused and the user is told. "Official" in the list follows the credential
and the address, never the listing's own fields.

**What counts as shared progress is Q1.**
- **Recommended:** skills and practice, stats, injuries, the bestiary and herbarium, and earned Gift standing.
- **Not shared:** money, goods and homes stay in the world where they were earned, like a bank in each realm.
  - Moving a purse between worlds would break each world's conserved economy and the steering controller (doc 75). It
    could only work by minting at one border and sinking at the other.
  - Gear could travel as catalog items if every official world runs the same `Data/`. That is offered as a later
    option.

Today there will be **one official world** (the Hetzner box), so Phase 6 can wait until a second official world
exists. Until then, the official world's own database is the truth. Central's checkout is only needed once two
official worlds could both write one wolf.

## 7. The server list

- **Host tokens.** One per world, made on central, shown once, stored hashed. Revoking it delists the world. Tokens
  unused for 90 days expire.
- **The heartbeat**, every 60 s, carries:
  - the listing fields: name (48 characters at most), description (300), port, region, tags, player limit, private
    flag, voices on/off;
  - protocol, build, the client-data hash, and the world package's id and version;
  - the online wolf ids.

  It expires after 3 minutes.
- **The address is where the heartbeat came from**, never a field the host fills in. Otherwise a listing could point
  every client's ping, and central's own call-back, at someone else's machine or at addresses inside central's box
  (its database, the DM tool, the cloud's metadata address). An optional host name is accepted only if it resolves to
  that same address. Private, loopback, link-local and reserved ranges are never listed.
- **Call-back.** Before first listing, and every 10 minutes after, central fetches `http://address:port/status`, with
  a 5-second timeout, a 2 KB cap, no redirects, and the connection made to the address already checked (no second
  name lookup).
  - The reply must name the same world id as the token. If it doesn't, or the call fails, the world stays off the list.
  - The host's server log and the client's Host page say why. This doubles as the host's port-forwarding test.
- **The client's ping** is made by the shell, not the page: to listed addresses and the direct-address box only, one
  request per world, a 2-second timeout and the same 2 KB cap. Everything a world sends (its `/status`, its listing
  text, and everything over the game socket) is shown as text, never as markup. The page has no `innerHTML` today;
  a test keeps it that way.
- **Honest counts.** Players online come from the heartbeat's wolf ids, checked against tickets central issued for
  that world. A host can't invent players, but can still pad the count with real accounts of their own; the account
  limits and reports are the answer to that.
- **Privacy.** Central learns which wolves are on which community world. The privacy page says so.
- **Names.** A word filter at registration, a report button on every listing, and delisting by hand (by world, token
  or host account).
- **Official badge:** set by central only. Community worlds can't use "official" or the game's name in their own.
- **Version filter:** the client greys out worlds on a protocol it can't speak, with "needs an update" or "this world
  is older".

## 8. The world package (the dedicated server)

**What's in it:** a `docker-compose.yaml` with two services, plus three more files.
- The services:
  - `ratw_server`, built for Linux x86_64 (and arm64 later). Its image also holds Python with psycopg, for the
    migrations and import (`tools/world_db.py`), the world agent (5.1) and the NPC Mind, which stays off without a
    key. The Mind must be in this container, or share its network (`network_mode: "service:ratw_server"`), because
    the server sends dialogue only to literal 127.0.0.1 (section 2);
  - Postgres 18, on a volume.
- Expect an image download of several hundred MB (Postgres alone is roughly 150 MB compressed; not checked), not the
  24 MB of world.
- What a world needs to run (memory, cores, disk) hasn't been measured; the hosting guide needs those numbers before
  D2.
- `world.toml`, the host's settings.
- `world/`, the world export.
- `README` and a hosting guide.

**The world export** is made by a new `tools/world_package.py export`. It is about 24 MB compressed and about 300-570
MB unpacked.
- **In:**
  - the newest build's manifest and cells (`world.builds`, `world.build_cells`);
  - the NPC and economy seed tables: `live.npcs`, `live.characters` (the NPCs' profiles), `live.professions`,
    `profession_slots`, patrols, factions, claims, relations, members, `npc_relations`, economy, `business_settings`,
    spawns, `npc_areas`, `npc_voice`, dialects, figures, `story_visitors`;
  - `Data/`;
  - optionally a running-state snapshot of the NPC world, with every player record removed, so a new world doesn't
    start its economy cold.
    - "Removed" has to be conserving: a player's money and goods go back to the treasury, or `Society::conserved`
      fails on the first load.
    - Players also live inside NPC-side sections: memories, regard, beliefs, rumours, crimes, acquaintances, ties,
      scene and talk memory (the checkpoint's `regardLedger`, `talkMemory`, `beliefs`, `crime`, `acquaintances` and
      more). The strip covers those, and the test below scans the whole snapshot.
- **Out:**
  - all `game.*` player data;
  - all `dm.*` and `admin.*`;
  - Atlas source (`world.terrain_chunks`, cells, areas and the rest);
  - old builds;
  - `live.game_changes`;
  - every key and password.

  An automated test fails the export if any player or admin table has a row in it, or if the snapshot anywhere holds
  a `wolf-` or `player-` id (the second from `--dev-identity` sessions) or a username.

**First start** creates the world's database, runs the migrations, imports the package, and makes a fresh world id.
The id comes from central's registration if there's a token, or a random id for a private LAN world.

**Without Docker,** a plain Linux tarball with a `start.sh` runs against a local Postgres. The file mode stays for
tests; it isn't the shipping path, because it is lossy.

**Ports:** one, 7788. UPnP is tried once (it fails behind carrier-grade NAT, as many home connections are), then the
call-back says whether it worked.
- The server is IPv4 only (section 2). IPv6 would reach many hosts behind carrier-grade NAT; it is a later step.
- Under Docker Desktop every player seems to come from one gateway address. Limits by address are therefore loose on
  those worlds; the ticket limits on central are the ones that hold.
- A tunnel (recommended in section 12 for hiding a home address) makes every player arrive from loopback. That is
  safe only because ticket mode grants loopback nothing (5, point 3).

**Updates:** a new package version is a new image tag. `docker compose pull && docker compose up -d` updates the
server and keeps the world's volume. A world whose protocol is too old drops off the list, so hosts have a reason to
update.

**No DM suite for hosts in v1.** Hosts get the in-game host commands (section 3.2). The studio, Atlas and DM tool stay
with the user's own worlds.

**The DM tool on the official world** keeps working: it reads `game.characters` and the account sections as now. But
its Players tab shows accounts by username (`tools/dungeon_master.py`, around line 118), which become central ids.
It needs the usernames from central (a read-only admin call), and an account ban is made on central, not in the world.
Counted in Phase 7.

## 9. What changes in doc 76 (the client)

- **Doc 76's section 4.2, "the page comes from the server", is reversed.** The page is bundled in the client.
  - A host's machine can't be trusted to serve the page. It could show a fake central sign-in and harvest passwords,
    and it would get whatever the shell allows its address.
  - With the page bundled, central's sign-in only ever happens inside the client's own code.
  - The costs:
    - every page change becomes a client release (Tauri's signed updater, which doc 76 already plans for later);
    - the client and worlds must agree a protocol number and the client-data hash (section 5.1), so a client release
      and a world package release go out together;
    - doc 76's other reasons for a thin shell go too: its address rule and phishing argument (4.3), the `remote.urls`
      capability (4.6) and "no commands of our own". The bundled page is the app's own content. Shell commands are
      now cheap, because page and shell always ship together: the refresh token (3.1) and the ping (section 7) live
      in the shell.
    - the page now talks to worlds it can't trust. It already caps message sizes (`MaxRaw`, `Client/src/net/wire.ts`)
      and writes no markup; keep both, and give the page a strict content security policy.
- **Browser play.** Today the world serves the page to any browser. In ticket mode it serves none (5.1), and central
  would need CORS and a sign-in page of its own to support browsers. Recommended: browser play stays for password-mode
  worlds (this laptop, DEV, scratch servers) and the client is the only way into ticket worlds (Q13).
- **The connection** goes to a chosen world's address, not the page's own host (`connection.ts:27-30`).
  - **Official worlds use `wss://`** through Caddy, as doc 76 planned.
  - **A world on this computer** (`ws://127.0.0.1:PORT`) should not count as mixed content: loopback is treated as
    trustworthy by Chromium, so WebView2 allows it; WebKitGTK is checked in the spike. So the milestone (section 10)
    doesn't need (a) below.
  - **Community worlds** mostly won't have certificates. A secure bundled page can't open plain `ws://` to another
    machine (it's blocked as mixed content). So either:
    - (a) the shell carries the socket in Rust (Tauri's websocket plugin) and hands frames to the page; or
    - (b) every community world needs TLS (Let's Encrypt has issued IP-address certificates since January 2026, but
      only if port 80 or 443 is reachable, and they last about 6 days, so renewals must be automatic).

    The spike (Phase 0) tests (a), and measures it: every frame crosses from Rust to the page, at 20 motion frames a
    second plus snapshots up to megabytes. If the plugin hands binary frames over as JSON arrays of numbers, that may
    be too slow, and a raw-bytes channel of our own is needed. Q3 asks what is acceptable.
  - Over plain `ws://`, see section 5, point 6: the session can be read and taken over on the path, in that world
    only. The list marks those worlds "not encrypted".
- **Doc 76's Phases 0-2 still stand:** the window, the keys, the close question, Reconnect. Its "v1 local" scope
  becomes this doc's Phase 3.
- **Doc 76's `--server URL` override** becomes the list's direct-address box plus a `--central URL` for development
  (a local central on this laptop).

## 10. Phases

| Phase | Work | Time |
|---|---|---|
Everything up to the milestone runs on this laptop: a local central, and worlds marked official or community by hand.
The real official world on the box comes in Phase 7.

| Phase | Work | Time |
|---|---|---|
| 0. Spike | A Tauri window with a bundled page reaching `wss://` and a loopback `ws://` world on Linux; the shell's socket to a plain `ws://` world, with its frame throughput measured on a full snapshot; an Ed25519 ticket signed in Python and checked in C++ through the run-time libcrypto loader; the world agent's heartbeat against a stub. Ends with D1 (Q3's transport) | 3-4 h |
| 1. Central core | `ratw_central`: its database and migrations; register, login, logout, delete; wolves, checked by a new `ratw_identity`; tickets (pairwise ids); keys and rotation; rate limits; tests. Refresh tokens ("remember me") are the first thing to cut if it runs long | 8-10 h |
| 2. Worlds join by ticket | Ticket mode (`--central`) beside the unchanged password mode; the ticket check; the account entry; `character_arrive` with arrival limits; the account sections on central ids (the sweep, with its test); Origin and `--web` in ticket mode; protocol number and client-data hash; `/status`; ctest plus a two-wolf join test with a local central | 10-13 h |
| 3. The client | Doc 76 Phases 0-2 (window, keys, Reconnect: 5-7 h on their own), with the page bundled; sign-in, wolves and the creator on central, the Arrival and Tie tabs moved to the world; the refresh token in the shell; the world list (no ping yet); join; first arrival; the direct-address box | 12-15 h |
| 4a. A bare list | Host tokens made with a command on central (no Host page); the world agent's heartbeat; expiry; the official flag set by hand; worlds on another protocol hidden | 3-4 h |
| **Plays end to end** | | **about 36-46 h** |
| 4b. The list, finished | The address rules and call-back; the ping in the shell; honest counts; word filter; greyed "needs an update"; the client's Host page | 4-6 h |
| 5. The world package | `world_package.py` export with the conserving strip and the whole-snapshot scan; first-start import; the compose bundle (two services, the Mind inside the server's container); `world.toml`; host commands for what waits on a DM; the community transport chosen at D1; UPnP attempt; hosting guide; tried on the Windows PC with Docker Desktop | 10-14 h |
| 6. Official shared progress | World credential bound to its address, checkout, save, checkin, leases and the 10-minute rule, history, sanity limits. Only when a second official world exists | 5-7 h |
| 7. Going public | Central and the official world on the box behind Caddy, isolated from each other (Q10); backups; the legal pages (privacy, host terms, contact point); age gate; deletion page, and deleting a wolf inside a world, which doesn't exist today; the DM tool on central ids; reports and delisting tools for the user; first signed Windows build (doc 76's release step) | 9-12 h |
| **All** | | **about 65-85 h** (60-78 h without Phase 6) |

The first draft's 28-36 h for the milestone counted doc 76's 5-7 h inside an 8-10 h client phase, leaving about 3 h
for sign-in, the wolf roster, splitting the creator between central and the world, the list and first arrival. It
also left out the ticket-mode switch, identity checking, the HTTPS question and the sweep of the account sections.
If a phase runs past its estimate, stop and ask (memory `session-budget`).

### Decision points
- **D1 (after Phase 0):** the community transport. If the shell's socket works on Windows WebView2 and Linux
  WebKitGTK, and is fast enough, Q3's plain option is possible. If not, community worlds need TLS. The milestone
  doesn't wait on it.
- **D2 (after Phase 5):** whether to ship the package to a few trusted hosts (closed beta) before Phase 7.

## 11. Tests and checks

- **Central:** unit tests for every call. Plus:
  - rate limits;
  - an expired ticket refused, a replayed `jti` refused, a ticket for world A refused by world B;
  - a ticket with another `alg`, or `none`, refused; a ticket issued before the world started refused; a clock 90 s
    off still admits;
  - key rotation, including a world meeting a new key id;
  - a wolf central accepts is accepted by a world (both through `ratw_identity`);
  - deletion removes the account and its wolves, and their names are reserved for 30 days.
- **World:** ctest stays green, unchanged, in password mode. A join test: two wolves join by ticket, arrive, play,
  leave and rejoin straight in. In ticket mode: a password sign-in from loopback is refused, `--dev-identity` won't
  start, and any Origin is accepted.
- **Economy:** `Society::conserved` holds after a thousand arrivals and after folding back the dormant ones. The
  steering and conservation tests are unaffected: sims have no players (memory `births-match-deaths`).
- **Package:** the export fails if any `game.*`, `dm.*` or `admin.*` row is present, or if any player id or username
  appears anywhere in the snapshot. A fresh `docker compose up` on this laptop and on the Windows PC lists the world,
  and its NPCs talk through the Mind with a key.
- **List:** a world that stops heartbeating drops off in 3 minutes, and an unreachable world is never listed. A
  listing can't name an address other than its own, or a private one. A padded player count shows the ticket count
  instead.
- **Official (Phase 6):** two official worlds can't both hold one wolf, a stale-version save is refused, and an
  over-limit save is flagged and not applied.
- **By hand, on Windows and Fedora:** register, make a wolf, join the official world, join a community world on the
  LAN, see each world's progress stay separate.

## 12. Risks

| Risk | What we do |
|---|---|
| Central is down: nobody can sign in or join anywhere | Keep it simple and backed up nightly. Players in a world stay in, but a dropped connection can't come back until central does (5, point 7). Worlds keep their last keys and ban list on disk. A status line in the client. A LAN party without central uses a password-mode world. |
| The box is broken into through the world server, and with it central's signing key and password hashes | The world server is a large network-facing C++ program on the same box as central (Q10). Separate system users and containers, central's key file readable only by central, separate database roles; rotating the key revokes every outstanding ticket. |
| A host token leaks (pasted `world.toml`, a screenshot) | It can only list a world under that host: it can't mint tickets or read anything. Shown once, revocable, and the listing's address is fixed to where the heartbeat comes from. |
| A griefer fills a world with throwaway wolves (each first arrival makes an economy account; 8,192 at most) | Account and ticket limits on central, arrival limits per world, dormant wolves folded back (5.1). |
| An account is deleted on central, but community worlds keep that wolf's data (position, chat in the event log, NPC memories, letters) | The heartbeat reply lists deleted wolves; honest worlds forget them; the host terms require it. Official worlds must too, and a world can't delete a character today (section 2): Phase 7 adds it. |
| A host harvests chat or sends it to an AI provider | Host terms make the host responsible for their world's data. The list shows whether voices use AI. Report and delist. |
| A cheated wolf on a community world | Nothing a community world does reaches central, by design (section 6). |
| Two official worlds duplicate a wolf's progress | Leases plus version checks (Phase 6). |
| Hosting friction: carrier-grade NAT, Docker on Windows, antivirus | A clear call-back message, a guide, and Docker so the server isn't an unsigned `.exe`. Expect support questions. |
| A host's home address is public in the list | Say so plainly when they make a token, and recommend a cheap VPS or a tunnel. |
| Version drift between the client and worlds | The protocol number, the client-data hash and the list's filter; a world too old drops off. Client and package releases go out together. |
| Offensive or fake listings | Word filter, call-back, honest counts, report button, delisting. |
| Legal: EU hosting (GDPR), ages, the EU Digital Services Act for listings | Hetzner's data processing agreement, minimum age 16, deletion without the game, a contact point and notice-and-action for listings, host terms. A short legal review before charging money. |
| About 45 tables and many checkpoint sections key on usernames and character ids, with no foreign keys | Character ids keep their shape, so only the username-keyed ones change. Phase 2 changes them in one sweep, with a test that walks every table and section after a join test and finds no username left. |

## 13. Sources

- Minecraft session server and `hasJoined`: https://minecraft.wiki/w/Java_Edition_protocol/Encryption
- Mojang's server blocklist: https://github.com/sudofox/mojang-blocklist/wiki
- Steam authentication: https://partner.steamgames.com/doc/features/auth
- Steam game server login tokens (GSLT): https://unturned-docs.readthedocs.io/en/latest/servers/game-server-login-tokens.html
- Factorio:
  - authentication: https://wiki.factorio.com/Multiplayer_authentication_API
  - matchmaking: https://wiki.factorio.com/Matchmaking_API
- Space Station 14:
  - the server hub and sessions: https://github.com/space-wizards/SS14.Web
  - launcher manifests: https://github.com/space-wizards/docs
- Veloren:
  - its auth server: https://gitlab.com/veloren/auth
  - hosting: https://book.veloren.net/players/server-hosting/introduction.html
- Diablo II realms: https://diablo2.diablowiki.net/Archive:_Realm_FAQ
- Rust community server rules: https://facepunch.com/legal/servers
- Tokens:
  - JWT: RFC 7519
  - JWT best practice: RFC 8725
  - the `Ed25519` algorithm: RFC 9864
  - PASETO: https://github.com/paseto-standard/paseto-spec
- Tauri websocket plugin: https://v2.tauri.app/plugin/websocket
- Mixed-content issue: https://github.com/tauri-apps/tauri/pull/4131
- Let's Encrypt IP certificates: https://letsencrypt.org/2026/01/15/6day-and-ip-general-availability
- Password storage:
  - OWASP: https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html
  - NIST SP 800-63B-4: https://pages.nist.gov/800-63-4/sp800-63b.html
- Data protection:
  - Hetzner: https://docs.hetzner.com/general/company-and-policy/data-protection-at-hetzner
  - EU ages of consent: https://childreninthedigitalworld.twobirds.com/home/eu
  - deletion (QuizDuel decision): https://www.edpb.europa.eu/system/files/2023-07/se_2023-05decisionpublic.pdf
- EU Digital Services Act, Art. 19: https://www.springlex.eu/en/packages/dsa/dsa-regulation/article-19/

## 14. Decisions for the user

1. **What is shared progress on official worlds?**
   - (a) Skills, stats, injuries, knowledge and earned Gift standing; money, goods and homes stay in each world.
   - (b) As (a), plus worn gear as catalog items.
   - (c) Everything, including the purse, minted and sunk at each world's border.

   *Recommended: (a).* It keeps every world's economy and the steering controller sound. (b) can come later. Not
   needed until Phase 6. Earned Gift standing is per account, not per wolf, and it also decides Q12.
2. **Windows hosts in v1.**
   - (a) Docker Desktop on Windows, with a native Linux tarball as well. Docker Desktop needs WSL 2 and
     virtualisation turned on; the tarball also runs inside WSL 2 without Docker.
   - (b) Port the server to native Windows first. That means a new network loop and a replacement for `fork`
     snapshots: a large job, perhaps 20+ hours.

   *Recommended: (a).*
3. **Community worlds without certificates.**
   - (a) Allowed, through the shell's own socket, marked "not encrypted" in the list. On the path, such a session can
     be read and taken over, in that world only (section 5, point 6).
   - (b) TLS required for any listed world.

   *Recommended: (a), if the spike passes, including its speed.* Most hosts can't get a certificate. Not needed for
   the milestone.
4. **What central is built in.** (a) Python, beside the studio and the DM tool. (b) C++, sharing the accounts code.
   *Recommended: (a)*, on one condition: identity rules aren't copied into Python, but checked by a small C++
   `ratw_identity` command (4.1). Without that, (b) is the safer choice.
5. **Friends, circles, stars and private messages.**
   - (a) Per world for now, moved to central later.
   - (b) Central from the start, adding about 6-8 hours.

   *Recommended: (a).*
6. **Wolf names:** unique across central, or only per world? *Recommended: across central*, since the name is part of
   the identity in every ticket. Names can't be changed. With unique names, squatting is possible: the account limits
   hold it down, and names of accounts that never played are freed after a while.
7. **Email at sign-up:** optional (only for password recovery), required, or none. Optional email only helps with a
   recovery flow and a mail provider to send it, neither counted in the phases. *Recommended: none during the closed
   beta* (the user resets a password by hand), *then optional, with recovery, before open sign-ups* (about 3-4 h
   more).
8. **Minimum age:** 16, with a birth-year question at sign-up. *Recommended: 16*, since central would be in Germany.
9. **Private worlds:** a world password and an allow-list of wolves. A password sent to the world in `hello` would
   cross plain `ws://` and reach the host. *Recommended: a password set on central for the world and checked by
   central before it issues a ticket*, so the ticket itself is the pass; the allow-list later.
10. **Where central runs:** on the same Hetzner box as the official world, or its own small box.
    *Recommended: the same box*, behind Caddy, but isolated from the world server (own system user and container,
    the signing key readable only by central, own database role). Move it only if it competes. If that isolation
    isn't done, use its own small box: a break-in through the world server would otherwise give the ticket key.
11. **Who may host:** any account, or hosts must be approved by the user first. *Recommended: approved hosts during a
    closed beta (D2), then any account.*
12. **The Gift tier with central creation** (4.1). The tier also costs build points: tier and strengths share one
    budget (`practice::checkBuild`, `Core/RatwPractice.cpp:577-581`). (a) The family is identity; the tier and the
    strengths are chosen at first arrival in each world, gated by that world's standing as today. (b) Every tier open
    at creation, dropping doc 49's gate. *Recommended: (a)*, since it keeps doc 49 and needs no shared standing. It
    moves the Gift tier and Strengths tabs to the arrival step (3.1).
13. **Browser play** (section 9). (a) Only on password-mode worlds (this laptop, DEV, scratch servers); the client is
    the only way into ticket worlds. (b) Also on the official world, which needs central's CORS and a sign-in page on
    central (about 3-5 h). *Recommended: (a)* for v1.

## Progress

**2026-11-07: the user's decisions.** "Go with your suggestions for the decisions. Caveat being that the main server
may not be on the same one as Hetzner. I'll setup the main server separately later."
- Q1-Q9 and Q11-Q13 take the recommendation: (a) shared progress without money, goods or homes; (a) Docker Desktop
  for Windows hosts; (a) unencrypted community worlds allowed and marked, if the spike passes; (a) Python central with
  a C++ `ratw_identity` check; (a) social layer per world for now; names unique across central; no email in the closed
  beta, optional with recovery before open sign-ups; minimum age 16; private-world passwords checked by central;
  approved hosts in the closed beta; the Gift tier and strengths chosen at first arrival; browser play only on
  password-mode worlds.
- **Q10: central gets its own server**, separate from the official world's box; the user sets it up later. Until
  then central runs on this laptop for development (`--central http://127.0.0.1:...`), and Phase 7 deploys it to the
  user's box. Nothing in Phases 0-6 depends on where it runs.

**2026-11-07: Phase 0, the spike (branch `r2-desktop`, worktree `.claude/worktrees/r2-desktop`).** About 25 minutes of
wall time on this laptop. Nothing in `Core/`, `Server/` or `Client/` changed; no game server was started.

What was built (all spike code, under `Desktop/`, plus one workflow file):
- `Desktop/src-tauri`: a Tauri **2.12.5** app ("Run Against the World", identifier `net.ratw.client`) with a bundled
  test page (`Desktop/page`). Tauri 2.12's MSRV is Rust 1.90, so **Fedora's Rust 1.94 is enough; no rustup**. Debug
  build about 3 minutes cold; release binary 5.9 MB. `.gitignore` gains `Desktop/node_modules`, `src-tauri/target`,
  `src-tauri/gen` and `spike/out`.
- Three ways for the shell to carry a socket, beside the page's own `WebSocket`:
  - `tauri-plugin-websocket` 2.5.0. Confirmed by reading it: binary frames cross as **JSON arrays of numbers** both
    ways.
  - **IPC relay** (ours): tokio-tungstenite in Rust; binary frames go to the page as raw bytes on a Tauri `Channel`
    (an `ArrayBuffer` in the page) and come back as a raw `invoke` body.
  - **Loopback relay** (ours, about 60 lines): a `ws://127.0.0.1:<random port>/<random token>?to=<world>` listener in
    Rust. The page opens it with its own `WebSocket`, and Rust pipes frames to the world unchanged.
- `Desktop/spike/ws_server.mjs`: a stand-in world in Node (`ws`). It echoes, sends paced streams and floods. It listens
  on `ws://127.0.0.1:47811`, on `ws://192.168.122.1:47811` (the libvirt bridge: a plain `ws://` world that isn't
  loopback, and not on the LAN), and on `wss://127.0.0.1:47812` with a throwaway CA from `spike/make_certs.sh`.
- `Desktop/spike/run_app.sh` runs the client once and collects the page's results (`spike/out/*.jsonl`).
  `spike/look.sh` opens it and takes a screenshot.
- `Desktop/spike/ticket/`: `sign_ticket.py` (central's side) and `verify_ticket.cpp` (a world's side), run by `run.sh`.
- `Desktop/spike/heartbeat_stub.py`: a stub central and the world agent (`selftest`).
- `.github/workflows/desktop-windows.yml`: a draft, **never run**. It only starts by hand (`workflow_dispatch`), so a
  push runs nothing.

**1. The window (Linux, KDE Plasma on Wayland, Intel + RTX 4070).**
- The window opens, the page's script runs, and every test completes. The page's origin is `tauri://localhost`, and
  `isSecureContext` is true. The data folder is `~/.local/share/net.ratw.client`.
- **The rendering could not be judged.** The session was **locked** for the whole spike (`LockedHint=yes`), so the
  compositor drew nothing new. Every screenshot shows only the empty window background, including Fedora's own
  WebKitGTK `MiniBrowser` on the same page.
- By default (DMA-BUF renderer), `requestAnimationFrame` ran at **0 fps** while locked; that is normal throttling of a
  hidden view. With `WEBKIT_DISABLE_DMABUF_RENDERER=1` it ran at 60-62 fps. `GDK_BACKEND=x11`,
  `WEBKIT_DISABLE_COMPOSITING_MODE=1`, Mesa's EGL and software GL all behaved the same.
- **Still to check, by the user, unlocked:** the moving squares show and move smoothly. Run
  `cd Desktop/src-tauri && cargo build --release`, then `SPIKE_PROFILE=release bash Desktop/spike/look.sh mine`, and
  look at the window or at `spike/out/mine.png`. If it is blank, try again with `WEBKIT_DISABLE_DMABUF_RENDERER=1`.
  Doc 76's other Phase 0 checks (fullscreen, pickers, sound, ten minutes minimised) wait for an unlocked session too.

**2. Sockets from the bundled page (Linux, WebKitGTK 2.50.5).**
- **The page's own `WebSocket` opens plain `ws://` to a non-loopback address** from `tauri://localhost`: no
  mixed-content block (doc 77 had left this open). It opens loopback `ws://` too, and `wss://` with the spike's
  certificate. A self-signed certificate needs the shell to allow it (`allow_tls_certificate_for_host`, done in
  `lib.rs`); a real certificate needs nothing.
- **Origins.** The world sees `Origin: tauri://localhost` from the page's own socket, which today's password-mode
  `originAllowed` refuses with 403, as section 5.1 expects. Both relays send **no** `Origin`, which today's server
  accepts. A relay must therefore be closed to anyone but the page (the token does this). Ticket mode doesn't rely on
  `Origin`.
- **Both relays work for `ws://` and for `wss://`.** They trust the spike's CA through `SPIKE_CA`; real worlds use
  webpki's built-in roots. The plugin refuses the spike's CA (`UnknownIssuer`): it would need its own `TlsConnector`
  state or the native-roots feature. That is not a problem for real certificates.
- **Measured.** Release build, loopback, default renderer; the second figure in a cell is from the run with
  `WEBKIT_DISABLE_DMABUF_RENDERER=1` (where rAF runs, so "worst frame" shows how long the page was blocked).
  - **At game rates every transport is fine.** One player's view (20 motion frames of 450 B a second plus 5 snapshots
    of 3 KB) and a crowd (1,000 frames a second for 4 s): every frame arrived, in order. Delay above the floor was
    0.6-0.8 ms at the median and at most about 2 ms at p99 (about 4-5 ms in the second run), at 60 fps. The plugin
    had one outlier (17 ms at p99).
  - **Ceilings differ by 10-50 times:**

| Transport | Echo of 450 B, mean | 20,000 × 450 B flood | 500 × 64 KB | 64 × 1 MB | Page blocked during the 450 B flood |
|---|---|---|---|---|---|
| Page's own `WebSocket` | 0.05 ms | 357k frames/s | 2.3 GB/s | 1.8 GB/s | 24 ms |
| Loopback relay (ours) | 0.08 ms | 455k frames/s | 1.8 GB/s | 1.9 GB/s | 17 ms |
| IPC relay (ours, raw channel) | 0.18 ms | 27k frames/s | 460 MB/s | 600 MB/s | **738 ms** |
| `tauri-plugin-websocket` (JSON arrays) | 0.25 ms | 27k frames/s | 53 MB/s | **40 MB/s (26 ms a frame)** | 319 ms |

  - The plugin takes about 1.5 ms per 64 KB frame (a large first snapshot) and 26 ms per 1 MB, all on the page's
    thread.
  - The IPC relay costs about 36 µs a frame, so a flood of small frames stalls the page.
  - The loopback relay is as fast as the page's own socket, and the page keeps its ordinary `WebSocket` code.

**3. The Ed25519 ticket.**
- RFC 9864 (October 2025) registers `"alg": "Ed25519"` and deprecates the polymorphic `EdDSA`. That is checked.
- `sign_ticket.py` signs with the system Python's `cryptography` 46.0.5; nothing was installed.
- `verify_ticket.cpp` loads `libcrypto.so.3` with `dlopen`, as `RatwSystemLibs` does, and binds six functions:
  `EVP_PKEY_new_raw_public_key`, `EVP_PKEY_free`, `EVP_MD_CTX_new`, `EVP_MD_CTX_free`, `EVP_DigestVerifyInit` and
  `EVP_DigestVerify`. It parses with `Core/RatwJsonDoc`, and the keys come as a JWK set (what `/keys` would serve).
- **Results: 25 of 25 cases as expected.**
  - Admitted: a good ticket, an official one, one expired 90 s ago (inside the 2-minute leeway), and one from a clock
    90 s fast.
  - Refused: wrong `aud`, expired, a replayed `jti`, issued before the world started, a flipped signature bit, a changed
    payload, another key, an unknown or missing `kid`, `alg` `none`, `EdDSA`, `HS256` with the public key as the
    secret, a lowercase `ed25519`, padding characters, two or four parts, a missing `wolf`, `exp` as a string, and a
    malformed wolf id.
- A ticket with sample look and build is **658 bytes**. A check takes **81.5 µs**: parse, verify and claims. Phase 2
  can keep one `EVP_PKEY` per `kid`.
- **Finding for Phase 2:** `RatwJsonDoc` keeps the *last* copy of a repeated key, as Python's `json` does. A doubled
  `alg` or `aud` is therefore read the same way by central and the world, and the cases above were refused. The
  world's ticket parser should still refuse repeated keys outright (a strict mode).

**4. The world agent.** `heartbeat_stub.py selftest`: 7 of 7. It covers these:
- a wrong host token is refused (401);
- one beat lists the world with 2 players;
- the address comes from the connection;
- the keys file is written atomically;
- central's bans are left as a file for the operator bridge;
- the clock offset is logged (under 50 ms);
- the world drops off after silence.

One beat (heartbeat, `/keys`, two files) takes 1.6 ms. Standard library only.

**D1, Linux: plain `ws://` community worlds are possible (Q3 (a)), with no relay at all.** The bundled page reaches
them itself.
- **Recommendation for Phase 3:** the page uses its own `WebSocket` wherever the engine allows it.
- If WebView2 blocks plain `ws://` to another machine, use **the loopback relay**. It is as fast as the page's own
  socket, needs no change to the page's socket code, and is guarded by a random token.
- Don't use `tauri-plugin-websocket`: big frames are slow and the page's thread does the work. The IPC relay is the
  second choice.

**Still to check on the user's Windows PC (D1 is open until then):**
1. **Get a build.** Either:
   - push `r2-desktop` and start "desktop-windows (spike, untested)" from the Actions tab (it builds, runs the spike on
     a GitHub runner and uploads the `.exe`, the installer and `windows.jsonl`); or
   - build on the PC: rustup (MSVC) plus the Visual Studio C++ build tools, Node 22, then in `Desktop/`: `npm ci` and
     `npx tauri build --bundles nsis`.
2. **Run the spike on the PC:**
   - in `Desktop/`: `npm ci`, then `bash spike/make_certs.sh` (Git Bash);
   - `node spike/ws_server.mjs --lan <the PC's LAN IPv4>` (allow it through the firewall on private networks);
   - in PowerShell: `$env:SPIKE_LAN="ws://<that IPv4>:47811"`,
     `$env:SPIKE_REPORT="$PWD\spike\out\windows.jsonl"`, `$env:SPIKE_CA="$PWD\spike\out\ca.pem"`, `$env:SPIKE_QUIT=1`;
   - start `src-tauri\target\release\ratw-desktop.exe`, and send back `spike\out\windows.jsonl`.
3. **What answers D1:**
   - the `env` line's origin (expected `http://tauri.localhost`);
   - whether `quick direct ws://<LAN>` connects or errors (WebView2's mixed-content rule);
   - whether the relays connect;
   - whether the window draws the moving squares.

   Expected failures, not problems: the direct `wss://` (no certificate hook on WebView2) and the plugin's `wss://`
   (`UnknownIssuer`).

**2026-11-07: Phase 1, central core (branch `r2-desktop`).** About 2 hours. No game server, no simulation; no world's
database was touched (tests use a throwaway `ratw_central_test`, made and dropped with the Postgres superuser from
`Database/.env`; no role was made or changed).

What was built:
- **`ratw_identity`** (`Server/ratw_identity.cpp`, in CMake): a small command built from Core. One JSON request per line
  on stdin, one reply per line on stdout; central keeps one running. Checks: `wolf` (name, age, appearance, family),
  `look` (an appearance, for a change), `profile` (the roleplay profile's fields, `people::applyFields`), `build` (a
  tier and strengths, for Phase 2's first arrival) and `version`. It replies with the game's own form of each (the
  name trimmed and capitalised, the appearance as `wire::appearance` writes it), which is what central stores.
  - The rules are not copied: the name, age and Gift checks moved out of `character_create` into
    `Core/RatwIdentity.{h,cpp}`, and **creation now calls them too** (`RatwGame.cpp`, same messages). The look is
    `wire::readAppearance` (male or female only), the build `practice::checkBuild`, the profile `people::applyFields`.
  - `Tests/identity_tests.cpp` (ctest `identity_behaviors`): the rules alone, then the command driven as central drives
    it. 74 checks.
- **`ratw_central`** (`Central/`, a new top-level folder; Python, psycopg 3 through `tools/world_db.py`):
  - `Central/migrations/0001_central.sql`, schema `central`: accounts, sessions, spent refresh tokens, wolves, reserved
    names, deleted wolves, issued tickets, and a **stub** `worlds` table (added by hand; host tokens, heartbeat and the
    list are Phase 4). Applied with `world_db.migrate`, recorded as `central-0001`. Its own database, default
    `ratw_central`; it refuses a world's database by name. It isn't in `Database/worlds.json`, so the world tools never
    touch it.
  - `central_store.py` (the database), `central_service.py` (the calls, apart from HTTP), `central_tickets.py` (the
    signing, the key ring and the reference ticket check), `central_identity.py` (the `ratw_identity` process),
    `ratw_central.py` (the HTTP server and the command line), `test_central.py`.
- **The calls:** `POST /account/register`, `/account/login`, `/account/refresh`, `/account/logout`, `GET` and
  `DELETE /account`, `GET`/`POST /wolves`, `PATCH /wolves/ID`, `POST /wolves/name` (is a name free), `POST /tickets`,
  `GET /keys`, `GET /health`. JSON in and out; `{"ok": false, "error", "code"}` with the HTTP status on a refusal.
- **Accounts.** Usernames 3-32 and passwords 12-128 exactly as `Core/RatwAccountsCore` (lower-cased; a letter first;
  bytes, UTF-16 units and control characters counted the same way). Passwords: PBKDF2-HMAC-SHA256, 600,000 rounds, a
  32-byte salt, as the world's accounts (not `world_db`'s scrypt, which section 4.1 allowed). A missing user costs the
  same hash. Hashing runs in at most a few places at once (half the cores, up to 4); a sign-in past that waits up to
  30 s for its turn, then gets 503. Access tokens last 15 minutes; refresh tokens 30 days with "remember me" (rolling
  from each refresh), else the session ends 12 hours after sign-in. Both are random and kept only as SHA-256 hashes. A
  refresh replaces both; a spent refresh token used again ends the whole session (it was copied). Logout ends this
  session, or every one (`everywhere`).
- **Age gate:** a birth year at registration; 16 or older (Q8). Only in the year someone turns 16 is the month asked
  too (turned 16 by this month). The birth year is never stored, only that the check passed.
- **Wolves:** up to 6 an account; checked by `ratw_identity`. **Names unique across central** (Q6), compared folded
  (NFKC, case-folded, spaces collapsed). A change may touch only the look (never the sex) and the profile, with a
  revision number against lost updates; the name, sex, age and family never change. The Gift is the **family only**
  (Q12 (a)): the tier and the strengths are a world's first-arrival choice, so the ticket carries no `build`.
- **Tickets:** JWT, `alg` `Ed25519` (RFC 9864), `typ` `JWT`, `kid` in the header. Claims: `sub`, `wolf`, `name`, `sex`,
  `age`, `look`, `gift` (the family, `""` for none), `aud` (the world id), `kind`, `jti` (128 random bits), `iat`,
  `exp` = `iat` + 60. `sub` is the account id on an official world and a pairwise id elsewhere (HMAC-SHA256 of the
  account and world ids with a secret of central's, in the `acct-<32 hex>` shape). Every ticket is recorded
  (`central.tickets`, a week) and counted per world. Only for a world in the registry and not delisted, a wolf of the
  caller's, and an account that isn't banned.
  - `central_tickets.verify` is the reference check for Phase 2: the algorithm pinned before anything else is read,
    repeated JSON keys refused, the header's fields fixed, the claims' types strict, `aud`, `exp` with 2 minutes'
    leeway, `iat` after the world's start (with the leeway), and each `jti` once.
- **Keys:** in a directory of their own (`Saved/Central/keys` by default; mode 0700, files 0600), never in the database.
  A key signs for 30 days. The next one is made and published at `/keys` a day before it signs; when it starts, the old
  one stops signing and stays published for an hour's grace, then its file is deleted. `keys rotate --now` is the
  emergency rotation: a new key signs at once and every other key leaves `/keys`, revoking every ticket out.
  Housekeeping (keys, pruning expired sessions, spent tokens, reservations and old tickets) runs at start and every
  5 minutes.
- **Deletion** (`DELETE /account` with the password): the account, its sessions and its wolves go at once. The username
  and the wolves' names are reserved for 30 days. The wolves' ids go to `central.deleted_wolves` (no personal data),
  for the heartbeat reply to pass to worlds (Phase 4).
- **Rate limits,** in memory, as sliding windows (one central process). Per address: every call 300 a minute, 5
  registrations an hour, 30 sign-ins in 10 minutes, 60 refreshes in 10 minutes, 10 deletions an hour. Per account: 10
  failed sign-ins in 15 minutes locks that account's sign-in for the rest of the window (from any address), 10 new
  wolves an hour, 60 changes or checks an hour, 20 tickets a minute. Per world: 600 tickets a minute. A refusal is 429
  with `Retry-After`. Behind Caddy, `--trust-proxy` takes the address from `X-Forwarded-For` (only from loopback).
- **By hand:** `worlds add --name N --kind official|community` (prints the world id) and `worlds list`;
  `accounts flag USERNAME --banned|--unbanned|--host|--admin...` (a ban also ends its sessions); `accounts
  set-password USERNAME` (Q7: no email in the closed beta, the user resets a password by hand; it ends every session).

How to run it on this laptop (in a git worktree, which has no `Database/.env` of its own, first
`set -a; . /home/martinb/Documents/GitHub/MUDProject/Database/.env; set +a`):

    cmake --build build-core --target ratw_identity        # or RATW_IDENTITY=path/to/ratw_identity
    python3 Central/ratw_central.py db-create              # ratw_central, owned by ratw_owner
    python3 Central/ratw_central.py worlds add --name "Laptop world" --kind official
    python3 Central/ratw_central.py serve                  # http://127.0.0.1:7790/ (migrates first)
    python3 Central/test_central.py                        # makes and drops ratw_central_test

`RATW_CENTRAL_DBNAME` picks another database (the smoke run used `ratw_central_smoke`, dropped after);
`RATW_CENTRAL_ROLE` another RATW role (default `owner`, for development; the box gets a role of its own in Phase 7).

Tests:
- `Central/test_central.py`: **40 tests, all pass** (about 2.5 s). The rules alone (usernames, passwords, names, the
  age gate, today's PBKDF2 at the real 600,000 rounds); every call; refresh rotation, a reused refresh token, expiry of
  access and refresh tokens, logout; the rate limits (registrations and sign-ins per address, failed sign-ins per
  account, tickets per account and per world, new wolves, sign-ins queuing for the hashing threads); wolves refused by
  the game's rules (bad name, age, look, sex `other`, NPC-only family, unknown fields, bad profile), names unique across
  central, six an account, changes; tickets (the claims, pairwise ids, an expired ticket, a replayed `jti`, the wrong
  `aud`, a ticket from before the world's start, a world clock 90 s off, a changed payload, `alg` `none`, `EdDSA`,
  lower-case `ed25519` and HS256 with the public key, a doubled `alg`, malformed tickets; unknown worlds, delisted
  worlds, others' wolves, banned accounts); key rotation (published a day ahead, a world meeting a new `kid` and
  fetching `/keys` again, the old key's grace and removal), keys surviving a restart with private files, the emergency
  rotation; deletion and its 30-day name and username reservation, then the names free again; over HTTP (the whole way
  through, bad requests, limits by the forwarded address); `ratw_identity` restarting after a crash, a missing binary
  (503 `identity_unavailable`), and central refusing the world databases.
- ctest: **81 of 81 pass** (with the new `identity_behaviors`, and `character_create` on the shared rules).
- A smoke run of `serve`: register, make a wolf, ticket for an official world, `/keys`, against a database made for it
  and dropped after.

Left for later phases, on purpose: the world list, host tokens' use and the heartbeat (Phase 4); private-world
passwords (Q9); reports; the word filter for names (4b); serving the creator's identity rules from `Data/` (section
3.1, needed by the client in Phase 3: `ratw_identity` is the natural place); the admin call for the DM tool (Phase 7);
a role of central's own and CORS (`--cors ORIGIN` exists, unused until Phase 3 decides whether the page or the shell
calls central).

For the user to decide (none blocks Phase 2):
1. **A wolf with a family and a Normal tier.** Under Q12 (a) the family is chosen at central and the tier at each
   first arrival. A wolf may also have no family, which then can only arrive as Normal. Is a family required, so every
   wolf can choose a tier in any world? Built: optional.
2. **Can a wolf be deleted on its own** (not the whole account)? Doc 4.2 lists no such call, and a world can't delete
   a character today. Built: only with the account.
3. **The limits' numbers** above are first guesses.

## Review (2026-11-07)

One adversarial review of the first draft, checked against the code at 0af9bc0. No database was queried and nothing
was run, so the size figures in section 2 (674 MB, 307 MB, 24 MB) are the draft's, not rechecked. The web sources in
section 13 were not re-read.

Fixed (wrong about today's code):
- `Entity` ends at `RatwWorld.h:382`, not 401. epoll and eventfd are at `ratw_server.cpp:56, 66, 148-150`; `flock` at
  `RatwGameOwner.cpp:78`.
- "About 30" `game.*` tables: about 45. Most keep the username inside `data`, not in a column.
- `playerAccountId` also accepts `player-` ids from `--dev-identity`.
- The welcome is 20 pennies and 2 herbs from the treasury, and 3 meals made new; not "20 coins and meals" from the
  treasury. It is conserving, so the steering and conservation tests are unaffected (sims have no players).
- Creation gates Gifted as well as Quickened, and the tier shares the strengths' points budget.
- studio.py and dungeon_master.py reach psycopg through `world_db.py`. Python's ready password hash there is scrypt,
  not PBKDF2.
- The Mind key path is whatever `--config` names.

Added (missing from the draft):
- The Origin check refuses a bundled page; ticket mode accepts any origin and serves no page.
- The page bakes in `Data/Terrain`, `walk.wasm`, portraits and fonts: a client-data hash beside the protocol number.
- Ticket mode is opt-in, so DEV, PROD, scratch servers, ctest and the client tools keep working unchanged; it refuses
  `--dev-identity`, and dev tools on official worlds; loopback grants nothing in it.
- The ticket check: algorithm pinned, clock leeway, `iat` after the world's start, unknown key ids refetched, the
  identity checked again; pairwise account ids on community worlds; what plain `ws://` really exposes.
- The listing address comes from the heartbeat's source, with private ranges refused, no redirects and the world id
  checked in `/status`: otherwise central's call-back and every client's ping could be aimed anywhere, including
  inside central's own box.
- The server has no HTTPS client: a Python world agent through the operator bridge, recommended.
- The Mind only answers on literal 127.0.0.1, so it can't be its own compose service.
- The export missed `live.characters` and `live.professions`; stripping players must return their money to the
  treasury and reach NPC-side sections; the test scans the whole snapshot.
- Economy accounts are capped at 8,192 per world and made on every first arrival: limits and folding back dormant
  wolves.
- The Gift tier can't be checked on central (Q12). Identity rules are C++, so `ratw_identity` rather than a Python
  copy (Q4).
- What waits on a DM (house requests, treaties, storytellers, artwork, reports) on community worlds; portrait uploads
  off there.
- The DM tool's Players tab on central ids; deleting a wolf in a world (today impossible) for central's deletion;
  deleted wolves sent to worlds.
- The signing key on the same box as the world server (Q10), host-token leaks, a stolen official credential (bound to
  its address), the 10-minute rule for leases.
- Browser play (Q13). The refresh token in the shell, not the page. Loopback `ws://` needs no relay, so the milestone
  doesn't wait on D1; the relay's frame speed is measured in the spike.
- Q7 (email) and Q9 (private-world password) recommendations changed.
- Hours: 65-85 in all and 36-46 to the milestone, instead of 45-60 and 28-36 (section 10 says why). The milestone was
  cut to a bare list (4a); the call-back, ping, honest counts, word filter and Host page moved after it. It runs on a
  local central, because the box comes only in Phase 7: the draft's milestone named "the official world" without
  deploying one.

Rejected, and why:
- **Moving friends and messages to central now:** Q5's (a) stands; nothing in v1 needs it.
- **Signing `/keys` with a key shipped in the package:** HTTPS to central's fixed address is enough, and a host who
  points their own world elsewhere only fools themselves.
- **Remembering every `jti` across restarts:** refusing tickets issued before the world started does the same job.
- **A challenge-and-answer call-back:** the source-address rule and the world id in `/status` cover it; a host who
  proxies their own world harms nobody.
- **Central-run names and certificates for hosts** (as Plex does for its servers): it would make TLS easy for every
  host, but it is a large piece of work; a later answer to Q3.
- **Dropping `kind` from the ticket:** harmless, and lets a misconfigured world refuse a ticket of the wrong kind.
- **The file mode as the shipping path:** still lossy, as the draft said.

Not checked, left for the spike or for later: whether WebKitGTK treats loopback `ws://` as trustworthy from the
bundled page; how Tauri's websocket plugin carries binary frames; RFC 9864's algorithm name; Let's Encrypt's IP
certificate terms; Docker image sizes.
