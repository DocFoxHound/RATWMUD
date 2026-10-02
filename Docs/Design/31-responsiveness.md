# 31. Responsiveness and scale: one server, a thousand wolves

Planned 2026-10-02. **Agreed. Phases 1–3 built 2026-10-02; Phases 4–6 not started.**

The plan is agreed (see "The decisions"). Phases 1 (measuring), 2 (saves) and 3 (movement) are built; the rest has not started. The target is **1,000 players on one server**, with
the world's roughly 1,000 residents. Hosting is local development now, dedicated servers later. We fix lag rather than
split the game into services by system. The Unreal cleanup waits until the character-creator work is done; it is
listed at the end.

## Where we stand (measured 2026-10-02)

All numbers are from this machine (20 threads), using DEV build 12 (966 residents, 1,289 places).

**`world_check --simulate 7 7.2 --players N`** covers the world tick plus each player's view and motion frame. It
leaves out JSON, compression and sending.

| Players | Mean tick | p99 | One player's view | One motion frame |
| --- | --- | --- | --- | --- |
| 20 | 21.6 ms | 54 ms | 1.5 ms | 0.08 ms |
| 100 | 86 ms | 159 ms | 2.1 ms | 0.09 ms |
| 250 | 224 ms | 394 ms | 2.4 ms | 0.14 ms |
| 500 | 354 ms | 659 ms | 1.9 ms | 0.13 ms |
| 1,000 | **893 ms** | 1,444 ms | 2.4 ms | 0.20 ms |

**A whole-game run** (scratch benchmark) covers everything the server does in a tick except writing to sockets:

- `Game::tick`, which sends a real snapshot and motion frame to each fake client
- zlib compression of every message, as the server does it
- acknowledgements, so snapshots are deltas
- a move command from every player every 2 seconds
- three full saves at the end

| Players | Mean loop | p99 | Compression | Snapshot (raw → packed) | Outgoing | One full save |
| --- | --- | --- | --- | --- | --- | --- |
| 5 | 13 ms | 37 ms | 0.8 ms | 24.8 KB → 3.0 KB | 0.8 Mbit/s | 60 ms |
| 20 | **41 ms** | **87 ms** | 3.2 ms | 34 KB → 3.0 KB | 3.1 Mbit/s | 77 ms |
| 100 | 209 ms | 357 ms | 19 ms | 35 KB → 3.0 KB | 16 Mbit/s | 215 ms |
| 250 | 396 ms | 628 ms | 37 ms | 35 KB → 3.1 KB | 41 Mbit/s | 383 ms |
| 500 | 692 ms | 1,124 ms | 65 ms | 34 KB → 3.0 KB | 81 Mbit/s | 668 ms |
| 1,000 | **1,397 ms** | 1,985 ms | 121 ms | 37 KB → 3.2 KB | 173 Mbit/s | **about 1 s** |

The whole-game run at 1,000 players is about **28× over budget**.

**What fills a snapshot.** This was measured after the client had acknowledged earlier snapshots:

- Visible wolves' details are about **70%** (11.5 KB of 16 KB). They are sent again in full every time.
- The terrain is already held by the client and is not resent.

The game has **50 ms per tick**. Reading the two tables together:

- **Today one server holds about 20–25 players**, and its p99 is already over budget at 20.
- **1,000 players is about 18× over budget on the world side alone, and about 28× over in the whole game.**
- **Every full save stops the game for 60 ms to about 1 s**, and the cost grows with the number of players. About a dozen
  ordinary commands trigger one: trade, eat, contracts, entering the world and others.

## What breaks at 1,000 players, worst first

1. **Views are rebuilt from scratch.** Each player gets five a second, at about 2.4 ms each. For 1,000 players that is
   250 views a tick, about **600 ms**. Each view recomputes sight and builds every tile row, entity and section, and
   only afterwards are unchanged parts trimmed away. A snapshot is still **about 35 KB of JSON raw** even after the
   trim, which means most of that work is thrown away.
2. **Motion frames scan the whole world.** `motion::frame` loops over *every* entity for every player, every tick,
   keeping only the ones in the same cell (`RatwMotionCore.cpp:135`).
   - With 2,000 entities that is 2 million checks a tick, about **200 ms**.
   - Each frame is built as a JSON tree, then packed to binary, then zlib-compressed. Compression saves only about
     35% on a 450-byte frame.
3. **`movementSounds` does the same scan.** Every 0.2 s it loops over all entities for each player, with a sight
   check for each (`RatwGame.cpp:1434`).
4. **Compression runs per client on the game thread.** At level 6 this costs about 0.15 ms per player per tick: about
   37 ms at 250 players, and about **150 ms** at 1,000. Chat is compressed once for each listener.
5. **Movement and separation grow with crowds**: 4× and 2× from 20 to 1,000 players. Separation compares each
   wolf with its neighbours in the cell, so a festival square with 300 wolves would be far worse.
6. **Saves**, as above. They also deep-copy the whole world on the game thread.
7. **One thread for everything.** Simulation, views, compression, sockets and saves share one core. The other 19
   threads on this machine sit idle.
8. **Smaller items:**
   - `poll()` over 1,000 sockets on every pass.
   - Entities live in a `std::map<std::string, Entity>`, so every lookup is a string comparison through a tree.
   - Route-search spikes: the p99.9 is 52 ms with 20 players.
   - Residents stranded on DEV make each day's ticks slower (commit 8ec3451).
   - Bandwidth is about 160 kbit/s per player, about 160 Mbit/s for 1,000. A dedicated server can carry that, but it
     should come down.

**Can one machine do 1,000?** Probably yes, but only if it does both of these:

- **Per-player work drops about 10×.** Change-driven views, frames built once per cell, and binary snapshots.
- **That work spreads over cores.** Everything per player becomes read-only work between ticks.

The rough budget:

| Work | Volume | Cost each | Cost per tick | With 16 cores |
| --- | --- | --- | --- | --- |
| Snapshots | 5,000 a second (250 a tick) | under 0.2 ms | 50 ms | 3–4 ms |
| Motion frames | 1,000 a tick | 0.01 ms | 10 ms | under 1 ms |

That leaves the world simulation 30–40 ms on its own thread. If the gates in Phase 6 show this can't be reached,
the fallback is splitting **by space** (see Phase 6).

## The decisions (2026-10-02)

| Problem | Chosen | Not chosen, and why |
| --- | --- | --- |
| The client | **Stay in the browser.** A desktop wrapper (Tauri or Electron) is possible later, with the same code. | A native client would remove none of the server's costs. |
| Movement feel | **Hybrid:** the client is in charge of its own wolf, and the server checks it. During combat and chases, the server is in charge and the client predicts. | Server-only with prediction everywhere would cost the server more for players' movement. |
| Scanning every wolf | **A spatial index first**, then motion built once per cell, slower updates for far wolves, and pawsteps folded into motion. | None of it can move to the client (see sight). |
| Who sees whom | **The server decides, recomputing only on change.** The client works out terrain-only lighting and line-of-sight shading for the picture. | All of sight on the client would let a modified client see hidden wolves. |
| Snapshots | **Wolves' details sent only when they change, in a binary format.** Snapshots stay on their five-a-second timer. | Sending on change instead of on a timer was not chosen. |
| Compression and cores | **A compression policy, a thread pool for per-player work, and a network thread with `epoll`.** | Region processes stay a fallback (Phase 6). |
| Saves | **A journal for valuables, and a `fork()` snapshot of everything else every 5 s** (changed from "the full redesign" once the fork cost was measured; see Phase 2). A crash loses no confirmed valuable and at most 5 s of the rest. | Full dirty rows and binary rows (held back); fixing only the stalls. |
| Slow ticks | **Time budgets for route planning and schedules, and fix the stranded residents.** | |

## How other games save, and what we take from each

| Method | Who uses it | Taken for |
| --- | --- | --- |
| Dirty tracking, a row per record | Most MMOs | Players, residents, memories, bonds, beliefs |
| Write-behind persistence thread | EVE, most MMO servers | Everything: the game thread never waits |
| Small transactions for valuables, with a callback | Item and currency systems | Accounts, trades, money, items, contracts |
| Append-only journal + snapshots | Redis AOF, databases | Crash recovery to within a fraction of a second |
| Copy-on-write snapshot via `fork()` | Redis BGSAVE | The bulk world: society, doors, roads, weather |
| Binary serialization | Nearly every engine | Rows the game reads back. JSON stays for tools and the DM. |

## The plan

Six phases, in this order. Each phase is measured with `game_load` (Phase 1) against the one before, and each is
useful by itself.

**Shared code.** Phases 2–4 change `RatwGame.cpp`, `RatwWorld.cpp` and the wire, and other work goes on in those
files too: the character creator and NPC conversations. Agree who owns which files before each phase starts.

### Phase 1. Measure

1. **`Tests/game_load.cpp`**, promoted from the scratch benchmark:
   - the whole `Game` with N fake clients that acknowledge snapshots and send movement
   - a world export
   - two layouts: spread over the country, and packed into one town
   - output: mean and p99 loop time, split by part, bytes per client, and the cost of a save
   It becomes the gate for every later phase. It is not a ctest: a run is minutes long.
2. **The server's own timing:**
   - every loop pass split into commands, world tick, views and snapshots, motion, compression, saves and socket
     writes
   - a minute's histogram of how long the game thread was blocked, by cause
   - `TickProfile`, logged once a minute
3. **A client overlay** (a settings toggle): ping, time from input to motion, frame time, and bytes per second.

Gate: the baseline numbers recorded here, with the same machine and the same world build as the table above.

**Built 2026-10-02.**

- **`Tests/game_load.cpp`** (`build-core/game_load EXPORT_DIR --players N --layout cities|spread`).
  - The whole `Game`, with fake players that enter, walk (a new direction every 2 s) and acknowledge every snapshot.
  - Every message is compressed exactly as the server does it.
  - It prints the server's own log lines for the window, the cost per player, and a full save.
  - `cities` packs players beside residents in the three most peopled regions. Today those are Upper Accord,
    Ser Ferro and Ridgemere.
- **The world for it.** `python3 tools/world_build.py export DIR` writes DEV's newest build as files, for
  `game_load` and `world_check`. `game::Options::worldExport` plays such a folder offline, with its residents as
  built.
- **`Core/RatwPerf.h`.** A meter on the game thread. Each part is timed exclusive of the parts inside it: compressing a
  snapshot counts as compression, and a save made by a command counts as saves.
  - The parts are commands, world, views, motion, the rest of the tick, saves, database statements (on the game's own
    connection), compression and sockets.
  - `ratw_server` logs it every minute (`--perf-log SECONDS`, 0 for never):
    - `RATW_PERF`: busy time per tick, as mean, p99 and max, and each part's mean per tick, plus traffic.
    - `RATW_PERF_BLOCKED`: for each part with a call of 5 ms or more, a histogram of those calls.
    - `RATW_PERF_WORLD`: `World::TickProfile` and route searches.
- **Ping.** `link::Ping` / `link::Pong` are answered by the server itself, uncompressed, outside the game's ordered
  messages.
- **The client's performance overlay** (Settings → Performance overlay, or `?perf`; remembered in the browser) shows:
  - frame and draw times
  - ping, latest and median
  - received KB/s
  - **input→motion**: from the key that sets a standing wolf walking to the first motion frame that shows it moved
- **Tests:**
  - client: the ping framing, and input-to-motion timing
  - `server_smoke`: the game's ping answered, and a wrong-sized one not
  - `tools/test_world_build.py`: the export
  - the movement and network smokes

**The baseline** (DEV build 13, this machine, `game_load`, 30 s measured after a warm-up, file-mode saves):

| Players | Layout | Busy per tick: mean / p99 | Views | Motion | Rest of tick | Compression | World | Full save | Save size |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 20 | cities | **23 / 52 ms** | 13.2 | 1.3 | 0.2 | 1.9 | 6.3 | 61–75 ms | 4.8 MB |
| 100 | cities | 79 / 136 ms | 51 | 6.4 | 1.9 | 9.0 | 10.4 | 133–156 ms | 16 MB |
| 250 | cities | 262 / 432 ms | 169 | 26 | 13.5 | 30 | 22 | 351–534 ms | 37 MB |
| 500 | cities | 619 / 1,137 ms | 387 | 68 | 57 | 68 | 37 | 558–625 ms | 71 MB |
| 1,000 | cities | **929 / 1,474 ms** | 536 | 129 | 128 | 89 | 43 | 814–896 ms | **140 MB** |
| 100 | spread | 125 / 176 ms | 87 | 9.7 | 2.2 | 12 | 13.6 | 135–147 ms | 20 MB |

The middle columns are ms per tick. Bandwidth was about 220–260 kbit/s per player, 152 Mbit/s at 1,000.

**What the baseline adds to the picture:**

- **Views are 55–60% of everything** at every size, so Phase 4.3–4.7 matters most. Players in the cities are
  *cheaper* per player than spread ones (51 against 87 ms at 100), because their views share loaded places.
  Crowding mostly costs motion and the rest of the tick.
- **"Rest of the tick" grows with the square of the players**: 0.2 → 128 ms. It is most likely `movementSounds`
  (every player against every entity, five times a second) and the other per-player loops in `Game::tick`. This is not
  yet measured part by part. Phase 4.1 and
  4.5 remove it.
- **The save grows by about 140 KB per player**: 140 MB of JSON at 1,000. Even the 15 s autosave stopped the game for
  77 ms with 20 players. Phase 2 must find what each player adds before choosing row layouts.
- **The world itself is not the problem**: 43 ms at 1,000 players. Movement and separation are its growing parts.

### Phase 2. Saves that never stop the game

**Changed 2026-10-02, before building.**

- **Two facts changed the plan:**
  - `fork()` measured 6–13 ms at 385–519 MB of memory.
  - 99% of a player's 140 KB in the save was their map memories.
- **What was chosen:** **valuables in a journal, everything else in a `fork()` snapshot every 5 s.**
- **Held back:** dirty rows and binary rows. Dirty rows would mean marking every place the world changes.
- **The gate became:** no storage stall over 15 ms, and none inside any command.

1. **Compact map memories.**
   - A remembered cell's glyphs, and its seen tiles packed eight to a byte, are each deflated and stored in base64
     (`glyphsZ`, `observedZ`).
   - A barely-seen 256×256 cell was 128 KB of spaces and '0's. It is now a few hundred bytes.
   - Old saves still read. A damaged memory is refused, not read as blank.
2. **The journal** (`Core/RatwJournal.h`; `game.journal`, migration 0028; `<save>.journal` for a file world).
   - **What is a valuable command:**
     - registering, creating a character, entering and leaving
     - trading, gathering, eating
     - theft, reports, fines, attacks and paying bandits
     - taking a contract, apprenticing, recruiting a companion
   - **Each is applied at once, then `Game::record()` appends what changed as one record:** a list of changes to the
     checkpoint document, each saying what something *now is*, so a record applied twice is harmless. It compares
     against what it last recorded:
     - purses and stock, account by account
     - the economy's counters and ledger
     - career positions, one by one
     - roads, crime, companions and sign-in accounts, compared whole
     - the character itself
   - **The writer is a thread with its own connection.** It commits in batches: each batch forms while the last is
     being written, so a quiet server commits at once.
   - **A failed batch is retried every 0.5 s until written.** Nothing is undone. The replies simply wait, and the
     server logs `RATW_JOURNAL cannot write`.
3. **Replies wait for the record.**
   - Every reply a command sends is held until the command ends.
   - If it made a journal record, the replies go when that record is written.
   - A client's later replies queue behind its earlier ones, so they keep their order.
   - In a test the journal writes inline. Tools can call `Game::settle()`.
4. **The snapshot by `fork()`, every 5 s** (and 3 s after other changes worth keeping):
   - The child is a frozen copy of the game. It captures and encodes the checkpoint and writes it:
     - **a file world:** its save, in place, by rename
     - **a database:** a private file, which the store's worker thread reads and stores as before, as deltas
   - The child uses only plain system calls and leaves with `_exit()`.
   - One snapshot at a time. Three failures in a row fall back to saving in place.
   - The document says how far into the journal it reaches (`"journal"`). Once it is stored, the records it covers
     are trimmed.
5. **Start-up.** The checkpoint loads, and the journal records after its `"journal"` are replayed over it before it
   is decoded. A world never saved is saved at once, so the journal always has something to replay over.
6. **What still waits for a whole save:**
   - stopping the server
   - a release restart
   - the operator bridge's start and commits (rare, and operator-only)
   - a store without a journal (a database not yet migrated): valuables save whole, as before
   DM actions, spawns and development weather now ask for a snapshot soon instead.
7. **Tests:**
   - `journal_tests`: changes, replay, the file journal (a torn last line ignored, trimming), and a failed batch kept
     until written
   - `checkpoint_tests`: compact memories, old form, damage
   - `game_tests`: herbs gathered, a crash without saving, the journal replayed, and a forked snapshot replacing the save
     and trimming the journal
   - `server_smoke`: an account registered, the server **killed with SIGKILL** right after the reply, and the account
     signing in after a restart
   - **By hand, against a copy of DEV with migration 0028:**
     - database snapshots every ~5 s, with the journal trimmed
     - an account only in the journal, the server killed, replayed on restart, and signing in

**Measured** (`game_load`, cities layout, file-mode saves, DEV build 13; Phase 1's baseline in brackets):

| Players | Storage on the game thread, worst | Save size | Mean / p99 per tick |
| --- | --- | --- | --- |
| 20 | **8.0 ms** (77 ms) | 2.0 MB (4.8 MB) | 23.5 / 59.5 ms (23.1 / 52.3) |
| 100 | **9.4 ms** (142 ms) | 2.2 MB (16 MB) | 77.5 / 111.6 ms (79.2 / 135.5) |
| 250 | **15.5 ms** (517 ms) | 2.6 MB (37 MB) | 193 / 260 ms (262 / 432) |

That worst case is the fork, once every 5 s. A full save is still 60–130 ms, but it now happens only when the server
stops.

**Open: one crash at 1,000 players.**

- **What happened:** the one 1,000-player run crashed after its 6-minute warm-up. The heap was found corrupted in
  `malloc`, during `Game::sendSnapshot`.
- **What doesn't reproduce it**, all under AddressSanitizer:
  - `game_tests` (1,210 checks) and `journal_tests`
  - a 100-player `game_load`: 60 s of play, a snapshot every 5 s, the journal writing throughout
- **What isn't known:** whether it is new in Phase 2 or older and only showing at that size. Phase 1's single
  1,000-player run did not crash.
- **When to chase it:** with the 1,000-player gate (Phase 6). Runs of that size are not made for every phase: they take
  15–45 minutes.

**Found along the way (not storage, for later phases):**

- **Password hashing ran on the game thread.** Registering or signing in held every player for about 140–230 ms.
  **Fixed 2026-10-02:**
  - Passwords are worked on by two hasher threads (`accounts::Hasher`). The command returns at once and the answer
    comes in a later tick.
  - A second request waits for the first. A client gone before its answer is forgotten. Missing users still cost the
    same.
- **Entering the world copied the whole `World`** and every character (`enterCharacter`, `character_create`), to roll
  back if something failed: 47–54 ms with DEV's world. **Fixed 2026-10-02:** only that character's entity and record
  are undone. Entering now costs 7–9 ms.
- **In-memory map memories are still 72 KB per remembered cell** (`std::vector<char>` and `std::vector<bool>`). This
  is most of the server's memory growth with players, and so most of the fork's cost.
- **`tools/smoke.py persistence` fails on `HEAD` before Phase 2 too:** the NPC conversation memory isn't kept
  (`active=0`). It is from other work, not from saves.

### Phase 3. Movement: the client is in charge, the server in chases

**Amended 2026-10-02 for combat (doc 33).**

- **Fights take place in turn-based instanced arenas**, where a move is a turn command on a grid, chosen and checked
  by the server. Real-time movement plays no part in a fight.
- **So there are three modes, not two:**
  - **free**: the client is in charge; the server checks it
  - **held**: the server is in charge; the client predicts. For the real-time moments around a fight: pursued by a
    guard, a crime just committed, a standing hostile close by.
  - **fighting**: in an arena. Real-time movement is off; the arena's own turn moves come with the combat work.
- **Phase 3 builds the switch into and out of fighting**, with no client or wire for arenas yet. Doc 33's rule that
  held keys are let go when a fight ends (`transitioned`) still applies.

1. **The movement module.** Movement is extracted from `World` into a small self-contained C++ module:
   - integration, pace and stamina
   - collision with tiles, heights, doors and seams
   - rise and turn timing
   It has no dependencies. The server uses it for NPCs and for checking players. Emscripten builds it to
   WebAssembly for the client. It is one source of truth.
2. **Free mode** (normally): the client is in charge.
   - The client moves its own wolf with the module, at once, and sends its pose 10–20 times a second, numbered: cell,
     position, facing, pace, posture.
   - The server doesn't integrate the player's movement. It **checks** each pose against the last accepted one:
     - Distance: within what pace, stamina and the time since allow, with a tolerance for jitter.
     - The path between the two poses: no solid tile, closed door or impossible height. The segment is sampled every
       half tile.
     - A change of cell only through a seam or door the wolf is standing at.
     - Stamina charged on the server from the distance accepted.
     - Posture and rise timing respected.
   - **When a check fails**, the server answers with a correction to the last good pose, and the client snaps back
     with easing. Failures are counted per character and logged. Repeated ones are flagged for the DM.
   - Collisions with other wolves (separation) stay with the server, applied as a gentle push the client takes as a
     correction.
3. **Held mode** (combat and chases): the server is in charge, and the client predicts.
   - **Held mode starts** when combat is detected (see "Settled"). That is when the character:
     - strikes or is struck (an entry in `fights_`)
     - is pursued by a guard (`pursuits_`)
     - comes within a set distance of a standing hostile
     - has committed a crime in the last N seconds
     - is in any future PvP encounter
   - **Held mode ends** after a quiet spell (about 10 s) with none of those.
   - The client sends inputs (keys, numbered), as now. The server moves the wolf and returns, in that player's own
     motion frame, the number of the last input it applied.
   - The client predicts with the same module. When a frame arrives, it replays its unconfirmed inputs from the
     server's pose and eases out any difference.
   - **The mode** travels in the player's own motion frame. The client switches on the next frame. Switching to held
     mode starts from the last accepted pose, so the switch makes no jump.
4. **The wire.** The client sends its pose (free mode) or its numbered inputs (held mode). The player's own motion
   frame gains the mode, the last applied input number, and corrections.

Gate:

- Input-to-motion under one frame in both modes, with 150 ms of simulated latency.
- Scripted cheat attempts are all corrected: speed, walls, teleports, crossing a cell without a seam.
- The server's movement time for players drops in `game_load`.

**Built 2026-10-02.**

- **The shared rules: `Core/RatwStep.{h,cpp}`.** How fast a wolf goes, where it may stand, how it slides along what
  blocks it, and what moving costs in stamina, on nothing but a grid of ground.
  - `World::integrate`, `passable` and stamina now call it, unchanged in behaviour (the world and movement tests
    pass).
  - Door and edge crossing moved into `World::throughDoor`, used by both kinds of walking.
- **The page runs the same code.** `Client/wasm/walk.cpp` is built to `Client/src/wasm/walk.wasm` (20 KB) by
  `tools/build_wasm.sh` (Emscripten, in `~/emsdk`). The .wasm is committed. `Client/src/game/walker.ts` feeds it the
  tiles, heights and closed doors the client holds.
- **Free movement:**
  - The page asks (`{"type":"walking","mode":"client"}`) and then walks its own wolf on the frame a key is pressed.
  - It sends `pose` twenty times a second (position, facing, and heading, so pushing into a door still crosses).
  - `World::placeByClient` checks each pose:
    - a distance budget refilled at the wolf's real speed (15% margin, half a second's worth at most, so jitter costs
      nothing and a burst of poses gains nothing)
    - a place it may stand
    - a clear way there (straight, or along one axis then the other, as walking slides)
  - A refused pose is answered with `correction`, and the page eases back. Refusals are counted, and 20 or 200 in a row
    are logged and recorded as an event for the DM.
  - The world no longer walks that wolf. Stamina is charged from the distance accepted.
  - The snapshot's `self` carries `walkSpeed` and `moveFactor`, so the page walks at the server's speed.
- **The server walks the wolf** in held mode:
  - **When:** pursued by a guard, a blow or an offence within 10 s, a standing hostile within 8 tiles, along a
    clicked route, on a journey, or after keys rather than poses (rising from a sit, or a script driving by keys).
  - **How the page predicts it:** with the same rules, from the server's newest pose, so it never draws the wolf
    through a wall.
  - Moves are numbered.
- **Fighting** (`Game::setFighting`, for doc 33's arenas): no walking of any kind. Leaving it returns to free.
- **The motion frame** ends with the observer's mode, the last input applied and the last pose accepted.
- **Tests:**
  - `game_tests`: a walk taken as sent; refused for five tiles at once, a burst over the budget, a pose in a wall, and
    a leap across the cell; a door crossed by heading into it; an offence holds the wolf; a fight stops even the keys;
    and free again after
  - client: the walker's speed, walls, sliding, closed doors, ledges and heavy ground; the page walking at once,
    twenty poses a second, corrections; held prediction stopping at a wall
  - `tools/walking_smoke.py`: real key presses in headless Chromium; the wolf moves on the page, the server follows to
    0.000 tiles, nothing is corrected
  - the movement and network smokes
- **Measured** (`game_load --players 100 --walking client|server`, cities): the world's movement went from 2.02 to
  1.18 ms a tick, and the world from 9.35 to 7.09 ms. Reading the poses added 1.05 ms of commands, which Phase 4's
  binary wire will cut. The tick mean went from 77 to 72 ms.

### Phase 4. Per-player work

1. **Spatial index (first).**
   - Entities get integer handles. String IDs are used only at the edges (wire, saves, tools).
   - Entities are kept in buckets per cell, with a grid of about 8 tiles inside each cell.
   - Everything that looks for neighbours uses it: motion, sounds, separation, sight, barks, ambient picks.
2. **Motion built once per cell per tick**, written straight to binary with no JSON tree.
   - Each observer gets the cell's frame filtered by their sight list.
   - The sight list is kept from Phase 4.4, so filtering is a lookup, not a sight check.
   - Viewers who see the same wolves share one encoded frame.
3. **Far wolves less often.**
   - Wolves within about 24 tiles of the observer: every tick (20 Hz).
   - Farther away in the same cell: every fourth tick (5 Hz).
   - The client already interpolates.
4. **Sight recomputed on change.** A player's sight list (whom they see and how clearly) is recomputed only when:
   - the observer moves to a new tile
   - a wolf near them moves to a new tile
   - a door, light, weather or time-of-day change nearby alters it
   Everything else reuses the list.
5. **Pawsteps in the motion frame.** "Heard, not seen" becomes a flag (and a direction) in the frame. It is derived from
   the same neighbour search, with no second scan. The client writes the line in the story pane.
6. **Terrain shading on the client.**
   - The client computes lighting and line-of-sight shading of the *terrain* from the tiles it holds, the light
     sources and the time.
   - The server's per-tile visibility rows leave the snapshot.
   - Which wolves are shown remains the server's sight list, so nothing hidden is sent.
7. **Wolves' details on change.**
   - A visible wolf's details (name as known, appearance, actions, injuries, carried, posture text) are sent when they
     change, or when the wolf first comes into view. Otherwise the snapshot refers to what the client holds, as
     terrain does today.
   - Change marks on the entity tell the snapshot builder; it doesn't build and compare.
8. **Binary snapshots.**
   - A compact, versioned binary layout replaces JSON for snapshots and motion. It is decoded in the client's worker.
   - A JSON debug form is kept for tools, the headless client and smokes.
   - Snapshots stay at five a second.
9. **Compression policy.**
   - No compression for messages under about 1 KB, which covers most motion frames.
   - Larger messages use a deflate stream per connection, so repeated structure compresses away, at level 1–3.
   - Broadcasts (chat, weather, announcements) are encoded and compressed once and shared.
10. **The thread pool.** After `World::tick`, the world is read-only until the next tick.
    - A fixed pool (one thread per core, less two) builds, encodes and compresses each due player's snapshot and
      motion in parallel.
    - Results go to each connection's outgoing queue.
    - The simulation itself stays on one thread and deterministic.
11. **The network thread.**
    - It runs `epoll`: it accepts connections, reads, parses WebSocket frames and writes queued output.
    - Commands reach the game thread through a lock-free queue, and are drained at the start of each tick.
    - The game thread never touches a socket.
12. **Separation** uses the spatial index, so a crowded square costs neighbours, not everyone.

Gate: gates 1–3 below.

### Phase 5. Smooth the spikes

1. **Route planning and the schedules pass** run under a per-tick time budget (about 4 ms). Unfinished work carries
   over to the next tick. This replaces the per-tick count.
2. **The stranded residents** (commit 8ec3451): find why residents end up stranded and fix the cause, so the cost of
   a tick stays flat over days of play.
3. **A 30 Hz tick** stays possible once p99 is comfortably under 33 ms. Movement no longer needs it.

Gate: p99.9 under 50 ms at 250 players. Over a simulated week, the mean tick on the last day is within 10% of the
first day's.

### Phase 6. Scale gates, and the fallback

Each gate is run with `game_load` on the development machine. It must pass with players packed around the cities, and
is checked with players spread out. Gate 4 is also
run on the planned server hardware.

| Gate | Players | Mean tick | p99 | Expected after |
| --- | --- | --- | --- | --- |
| 1 | 100 | under 25 ms | under 50 ms | Phases 2–3, and Phase 4.1–4.2 |
| 2 | 250 | under 25 ms | under 50 ms | Phase 4 complete |
| 3 | 500 | under 30 ms | under 50 ms | Phase 4's pool and network thread, and Phase 5 |
| 4 | 1,000 | under 35 ms | under 50 ms | the server hardware |

**If gate 3 or 4 can't be met**, the fallback is splitting by space. Several world processes each own a set of regions,
with handover at the borders and a small relay for chat. It gets its own design doc if it comes to that.

## Hardware notes for the dedicated server

- The simulation thread wants **high single-core speed**. The pool wants **many cores**. A current 16–32 core part
  with high boost clocks suits both.
- Memory: the world is in the hundreds of MB. The `fork()` snapshot briefly needs room for pages changed while the
  child writes, which is small.
- **Postgres** on the same machine, on fast NVMe. The journal is small sequential writes. Move Postgres to its own
  machine only if its disk competes with the game.
- About 1 Gbit/s of network. Phase 4 should bring 1,000 players well under the 173 Mbit/s measured today.

## Settled (2026-10-02)

- **Free-mode tolerance:** 15% over the allowed distance, then tuned from Phase 1's data.
- **Held-mode triggers:** the list in Phase 3.3 stands.
  - The game has no general combat yet (doc 18 is design only), and no lawless or PvP zones. So held mode is
    triggered by **combat detected as it happens**, not by place.
  - Phase 3 builds the switch and its hooks: the existing assault, pursuit and crime paths, and a single
    "combat began / ended" call for the combat system to use.
  - Each trigger is wired up as the system behind it is built.
- **The journal batch window:** 100–250 ms.
- **Where players stand:** mostly around cities, where most stories and quests are. The **packed layout** (several
  hundred players in and around the great cities) is the one the gates must pass. The spread layout is checked too.

## Later: the Unreal cleanup (deferred)

Nothing in the tracked code depends on Unreal. Left over:

- **About 2.2 GB of untracked engine output.** Remove it:
  - `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Content/`, `Build/`
  - in `Saved/`: `StagedBuilds`, `Cooked`, `Shaders`, `MaterialStats`, `ShaderDebugInfo`, `UnrealBuildTool`,
    `Autosaves`, `Crashes`, `Temp`
- **Keep the rest of `Saved/`.** `Config/RATWNPCAI.local.json` (the AI key), the demo save, playtests, tests, logs
  and DM review are all in use.
- **Fix the stale paths:**
  - the `Source/RATWMUD` fallback in `tools/game.sh:12`
  - two migration comments
  - the `.gitignore` entries for `Content/` and `Build/`
- Mark `PLAN.md`'s Unreal milestones as historical.
- Retire `tools/convert_saves.py` once no SQLite saves remain.
- **Unreal-shaped formats that still work:**
  - motion strings written as `FArchive` writes an `FString`
  - Unreal-style GUIDs
  - text limits counted in UTF-16 units
  They go when Phase 4.8 replaces the wire format.
