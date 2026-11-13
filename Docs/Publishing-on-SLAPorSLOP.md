# Publishing on SLAPorSLOP

Run Against the World's store page on [SLAPorSLOP](https://slaporslop.games) (its guide: https://slaporslop.games/publishing.md):
the desktop client (`Desktop/`, Tauri 2 around `Client/`) for Windows and Linux, store address `run-against-the-world`,
stage Beta, MIT licence. Set up 2026-11-13.

## Done in the repository

- **Slop Build** (`.github/workflows/slop.yml`, the guide's Option B): on every push to `ridgemere2`, every `v*` tag and
  by hand (Actions, "Slop Build", Run workflow). It installs and builds the Editor's page and the game's page, then
  `tauri build --no-bundle` in `Desktop/`; Windows `.zip`, Linux `.tar.gz`, `.deb` and `.rpm`, each signed with a GitHub
  attestation, with Microsoft Defender, gitleaks and OSV-Scanner run on them. **It has `publish: false`**: it builds and
  checks only (no release, nothing sent to SLAPorSLOP; the files stay a day as the run's artifacts).
- **The rules for the code** (the guide's "Rules for the code"):
  - The Editor window no longer loads its page from the world: the client ships `Editor/dist` built in
    (`Desktop/src-tauri/build.rs`, `src/editor/pages.rs`) and only `/api/` data goes to the world (doc 78, 6.6's note).
    The signed-bundle check went with it.
  - No self-updater (SLAPorSLOP delivers updates); data from central and worlds is JSON; the shell starts no programs
    and opens no address a server names; the Editor window can't open windows or leave its page.
  - Central is reached over HTTPS. **Worlds are reached over plain `ws://` today** (the client also speaks `wss://`
    when a host sets up TLS); worlds are players' servers, not the maker's, and the Editor's own connection to a world
    is TLS 1.3 with a pinned key.
- **Saves** (the guide's "Saves and sandbox mode"): when `SLOP_SAVE_DIR` is set, everything the client keeps on the
  computer goes there: the Editor's pin store (`editor-worlds.json`), the sign-in and device token files (the file
  token store, not the OS keychain, since sandbox mode on Linux has none), the window's size and place, and the
  webviews' data (the page's localStorage and caches, `webview/`). Unset, as before.
- **Slop Pulse** (`Desktop/src-tauri/src/pulse.rs`): with `SLOP_PULSE_TOKEN`, `SLOP_PULSE_URL` and
  `SLOP_PULSE_INTERVAL` set, `start` on launch, `heartbeat` every interval, `end` on quit; only the token is sent.
- **Pictures** (`.slop/media.json`): the icon (the painted wolf face), a 920x430 thumbnail, nine screenshots with
  captions.

## The store page

Open this link (one use; ask for a new one if needed). It fills in **Publish -> New app**; nothing is sent until you
press **Create draft**:

https://slaporslop.games/publish/new#fill=%7B%22title%22%3A%22Run%20Against%20the%20World%22%2C%22slug%22%3A%22run-against-the-world%22%2C%22summary%22%3A%22An%20online%20role-playing%20game%20about%20wolves%20in%20a%20living%20world%3A%20residents%20with%20lives%20of%20their%20own%2C%20a%20working%20economy%2C%20turn-based%20fights%2C%20and%20worlds%20you%20can%20host%20yourself.%22%2C%22description_md%22%3A%22**Run%20Against%20the%20World**%20is%20an%20online%20role-playing%20game%20about%20wolves%20in%20a%20living%20world.%20You%20make%20a%20wolf%2C%20painted%20in%20your%20own%20colours%2C%20and%20join%20a%20world%20where%20hundreds%20of%20other%20wolves%20already%20live%20their%20lives%3A%20residents%20with%20homes%2C%20families%20and%20trades%2C%20who%20work%2C%20shop%2C%20talk%20among%20themselves%2C%20fall%20on%20hard%20times%20and%20are%20born%20and%20die%20whether%20or%20not%20a%20player%20is%20watching.%5Cn%5Cn%23%23%23%20A%20world%20that%20keeps%20going%5Cn-%20**The%20city%20of%20Ridgemere%20and%20its%20country**%3A%20districts%2C%20a%20wharf%2C%20markets%20on%20Marketday%2C%20farms%20and%20villages%2C%20roads%20through%20woods%20and%20hills%2C%20seasons%20and%20weather%2C%20day%20and%20night.%5Cn-%20**Residents%20with%20lives%20of%20their%20own**%3A%20each%20has%20a%20job%20and%20a%20place%20to%20sleep%3B%20they%20talk%20in%20their%20own%20dialects%2C%20gossip%2C%20pass%20on%20rumours%20and%20remember%20what%20you%20did.%5Cn-%20**A%20staged%20economy**%3A%20goods%20are%20grown%2C%20made%2C%20carried%20and%20sold.%20Shops%20keep%20their%20tills%2C%20markets%20have%20lean%20times%20and%20good%20ones%2C%20and%20prices%20move%20with%20what%20is%20short.%5Cn%5Cn%23%23%23%20Your%20own%20story%5Cn-%20**Play%20your%20part**%3A%20write%20what%20your%20wolf%20says%20and%20does%3B%20scenes%20with%20other%20players%20are%20kept%20in%20your%20journal.%5Cn-%20**Fights%20that%20count**%3A%20turn-based%20duels%20and%20brawls%20with%20bandits%20and%20other%20wolves%2C%20injuries%20that%20take%20time%20to%20heal%2C%20Gifts%20of%20fire%20and%20other%20powers%2C%20hunts%20with%20a%20pack.%5Cn-%20**Together**%3A%20parties%2C%20Chapters%20(the%20game's%20clans%2C%20with%20standing%20among%20the%20factions)%2C%20friends%2C%20letters%20and%20private%20messages.%5Cn%5Cn%23%23%23%20Host%20your%20own%20world%5CnAnyone%20can%20run%20a%20world%20on%20their%20own%20computer%20or%20a%20rented%20server%20with%20the%20world%20package%20(Docker).%20The%20client's%20**Editor**%20opens%20a%20host's%20tools%20for%20their%20world%20over%20a%20pinned%2C%20encrypted%20connection%3A%20the%20Dungeon%20Master's%20live%20map%2C%20NPC%20Life%2C%20prosperity%20and%20the%20economy%2C%20and%20**Atlas**%20for%20the%20map%20itself%2C%20with%20co-builders%20who%20get%20their%20own%20Editor%20accounts.%5Cn%5Cn%23%23%23%20Before%20you%20play%5Cn-%20You%20need%20a%20free%20account%20on%20the%20game's%20central%20server%20(made%20in%20the%20client)%2C%20and%20a%20world%20to%20join%3A%20the%20client%20lists%20the%20worlds%20that%20are%20open%2C%20or%20you%20can%20enter%20a%20host's%20address.%5Cn-%20Worlds%20are%20run%20by%20their%20hosts.%20The%20client%20keeps%20your%20password%20and%20sign-in%20to%20itself%3B%20a%20world%20sees%20your%20wolf%2C%20never%20your%20password.%5Cn-%20**Beta**%3A%20the%20game%20is%20still%20changing%2C%20and%20worlds%20may%20be%20reset.%5Cn%5CnSource%2C%20design%20notes%20and%20the%20world%20package%3A%20https%3A%2F%2Fgithub.com%2FDocFoxHound%2FRATWMUD%20(MIT%20licence).%22%2C%22visibility%22%3A%22public%22%2C%22kind%22%3A%22game%22%2C%22stage%22%3A%22beta%22%2C%22repo_url%22%3A%22https%3A%2F%2Fgithub.com%2FDocFoxHound%2FRATWMUD%22%2C%22license_spdx%22%3A%22MIT%22%2C%22min_age%22%3A12%2C%22content_labels%22%3A%5B%22violence%22%2C%22gambling_themes%22%5D%2C%22platforms%22%3A%5B%22windows%22%2C%22linux%22%2C%22linux-deb%22%2C%22linux-rpm%22%5D%2C%22tags%22%3A%5B%22rpg%22%2C%22open-world%22%2C%22simulation%22%2C%22multiplayer%22%2C%22exploration%22%2C%22pvp%22%2C%22co-op%22%2C%22stealth%22%2C%22turn-based%22%2C%22atmospheric%22%5D%2C%22online%22%3A%5B%22own_servers%22%2C%22player_servers%22%5D%2C%22online_hosts%22%3A%5B%22ratw.188-68-41-241.sslip.io%22%5D%2C%22sandbox_ready%22%3Atrue%2C%22bug_reports%22%3Atrue%7D

The fields it fills in:

| Field | Value |
|---|---|
| Name | Run Against the World |
| Store address | `run-against-the-world` |
| Summary | An online role-playing game about wolves in a living world: residents with lives of their own, a working economy, turn-based fights, and worlds you can host yourself. |
| Type, stage | Game, Beta |
| Repository, licence | https://github.com/DocFoxHound/RATWMUD, MIT |
| Runs on | Windows, Linux (`.deb` and `.rpm`) |
| Age rating | **12+** (see below) |
| Content labels | violence, gambling themes |
| Tags | rpg, open-world, simulation, multiplayer, exploration, pvp, co-op, stealth, turn-based, atmospheric |
| Online | the maker's servers (central: `ratw.188-68-41-241.sslip.io`) and servers players host (worlds) |
| Works in sandbox mode | yes |
| Bug reports | yes |

**Age rating.** 12+: fights between wolves, with bandits and with game are frequent, with bites, blades, injuries
that take time to heal and death, told in text and small pictures, never graphic; residents drink ale and mead,
and their talk has friendly penny wagers and dice (the "gambling themes" label; the game has no gambling). 16+ is the
choice if you want to allow for what players write to each other in free chat, or for the play-style consent flags
(romance, character death) players can set for their roleplay. Nothing sexual; the strongest word in the residents'
lines is "Damn".

The description (Markdown):

```markdown
**Run Against the World** is an online role-playing game about wolves in a living world. You make a wolf, painted in your own colours, and join a world where hundreds of other wolves already live their lives: residents with homes, families and trades, who work, shop, talk among themselves, fall on hard times and are born and die whether or not a player is watching.

### A world that keeps going
- **The city of Ridgemere and its country**: districts, a wharf, markets on Marketday, farms and villages, roads through woods and hills, seasons and weather, day and night.
- **Residents with lives of their own**: each has a job and a place to sleep; they talk in their own dialects, gossip, pass on rumours and remember what you did.
- **A staged economy**: goods are grown, made, carried and sold. Shops keep their tills, markets have lean times and good ones, and prices move with what is short.

### Your own story
- **Play your part**: write what your wolf says and does; scenes with other players are kept in your journal.
- **Fights that count**: turn-based duels and brawls with bandits and other wolves, injuries that take time to heal, Gifts of fire and other powers, hunts with a pack.
- **Together**: parties, Chapters (the game's clans, with standing among the factions), friends, letters and private messages.

### Host your own world
Anyone can run a world on their own computer or a rented server with the world package (Docker). The client's **Editor** opens a host's tools for their world over a pinned, encrypted connection: the Dungeon Master's live map, NPC Life, prosperity and the economy, and **Atlas** for the map itself, with co-builders who get their own Editor accounts.

### Before you play
- You need a free account on the game's central server (made in the client), and a world to join: the client lists the worlds that are open, or you can enter a host's address.
- Worlds are run by their hosts. The client keeps your password and sign-in to itself; a world sees your wolf, never your password.
- **Beta**: the game is still changing, and worlds may be reset.

Source, design notes and the world package: https://github.com/DocFoxHound/RATWMUD (MIT licence).
```

## What only you can do

1. **Sign in** on https://slaporslop.games with GitHub (Sign in -> Continue with GitHub).
2. **Open the link above**, check every field (the age rating, content labels, licence and repository especially) and
   press **Create draft**; accept the Publisher Agreement it shows.
3. **Turn publishing on**: change `publish: false` to `publish: true` in `.github/workflows/slop.yml` (or ask the agent
   to), push to `ridgemere2`, and the next build makes a release and sends it to SLAPorSLOP. Your Releases page
   follows it (Publish -> Releases & progress).
4. **The first release is reviewed by a person** at SLAPorSLOP; later ones that pass every check go out by themselves.
   Optional: install SLAPorSLOP's GitHub App from the Releases page's Updates box, so releases are picked up at once.
