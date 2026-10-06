# 31. Responsiveness and scale: one server, a thousand wolves

Planned 2026-10-02. **Agreed. Phases 1–4 built 2026-10-02; Phases 5–6 not started.**

The plan is agreed (see "The decisions"). Phases 1 (measuring), 2 (saves), 3 (movement) and 4 (per-player work) are built; the rest has not started. The target is **1,000 players on one server**, with
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
| Saves | **A journal for valuables, and a `fork()` snapshot of everything else every 30 s** (changed from "the full redesign" once the fork cost was measured, see Phase 2; from every 5 s to every 30 s in Phase 6). A crash loses no confirmed valuable and at most 30 s of the rest. | Full dirty rows and binary rows (held back); fixing only the stalls. |
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
4. **The snapshot by `fork()`, every 5 s** (every 30 s since Phase 6; and 3 s after other changes worth keeping):
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

**Built 2026-10-02**, each step measured with `game_load`. The profiling was done with callgrind; gprof's numbers
misled at first.

1. **Compression** (4.9). Messages under 1 KB (most motion frames) go uncompressed: the raw length's top bit is
   `link::Stored`, and both decoders read it. The rest go at zlib level 1, not 6. Compression went from 8.9 to 3.4 ms
   a tick at 100 players, for 14% more bandwidth.
2. **Section keys hashed straight from the value**, eight bytes at a time, not by writing JSON first. A part sent
   again as the very same value is not hashed again: its copy-on-write storage (`json::Value::storage`) is kept per
   client.
3. **Wolves' details sent when they change** (4.7). `entities` is a delta section:
   - each wolf's details go once, then as `{"$held": key}`
   - its pose and timers (x, y, facing, turning, moving, posture and speech timers) travel with every reference
4. **The cell's ground and heights as row edits.** Unseen tiles are blank, so every step that shows new ground
   changed them. They went whole, 67 KB a snapshot for a walking wolf in a city.
   - Snapshots fell from about 16 KB to 5 KB raw.
   - The rows themselves are reused while what the wolf sees and remembers is unchanged.
   - `World::snapshot` can leave out the tiles, and the game reads the live cell and the memory directly, so a
     city's 65,000 tiles are no longer copied for every view.
5. **Sight** (4.4):
   - **The range once per view and motion frame** (`visionClarity(observer, source, range)`). It was worked out up to
     three times for every wolf and every door, each sampling the weather field.
   - **Views keyed by the tile**, as agreed, not by the position to a fiftieth of a tile. A walking wolf's thousands
     of sight rays had been cast again for nearly every snapshot. Sight fell from 6.1 to 0.5 ms a tick at 100
     players.
6. **The spatial index** (4.1). `World::idsIn(cell)` holds ids, never pointers, and is rebuilt each tick and on
   entries, removals and doors. Views, motion frames, pawsteps, the hostile check and the trader search look at their
   own cell, not all 2,000 entities.
7. **The pool** (4.10, `Core/RatwPool.h`):
   - **What runs on it:** every due player's whole snapshot, the motion frames, sight (`prepareViews`), memory
     (`World::observeAll`) and pawsteps.
   - **On the game thread first:** what a view would change (memory, the indexes, each client's kept rows,
     notices). The views then only read the world.
   - **Checks:** ThreadSanitizer is clean at 30, 60 and 80 players, the last walking freely. The meter counts only on
     the game thread, and traffic atomically.
   - **Size:** the server uses the cores less two (`--workers N`); tests use none.
8. **Binary poses**, for input. A pose is 18 bytes (`link::Pose`), not a JSON command: 5,000 a second at 250 players
   cost 5 ms of command parsing.
9. **`epoll`** (4.11) in place of `poll()`. Only sockets with something to do are looked at; output waits are watched
   only while some is pending. The network work is still on the game thread: with `epoll` and the pool, a thread of
   its own wasn't needed yet.
10. **Measured** (`game_load`, cities, 30 s, file saves; Phase 1's baseline in brackets):

| Players | Mean / p99 per tick | World | Views + sight | Motion | Snapshot raw | kbit/s per player |
| --- | --- | --- | --- | --- | --- | --- |
| 20 | **11.1 / 39** (23 / 52) | 6.6 | 3.4 | 0.7 | 5.0 KB (17 KB) | 173 (220) |
| 100 | **20.5 / 51** (79 / 136) | 11.5 | 6.6 | 1.1 | 4.9 KB (16 KB) | 174 (218) |
| 250 | **37.8 / 95** (262 / 432) | 18.8 | 13.7 | 1.8 | 5.2 KB (15 KB) | 181 (223) |
| 100 spread | 31.8 / 92 (125 / 176) | 17.4 | 11.1 | 1.4 | | |
| 250, the page walking | 34.9 / 82 | 14.2 | 12.7 | 1.8 | | |

**Gate 1** (100 players, under 25 ms mean and 50 ms p99): **the mean is met. The p99 (51 ms) is just over.** Its spikes
are the residents' schedules and route planning (`RATW_PERF_WORLD` worst), which Phase 5 is for.

**Gate 2** (250) is not met: 37.8 ms mean. **What is left is mostly the world's own tick**, which runs on one thread:
residents walking, separation, schedules and route planning. Players' work now spreads over the cores.

**Then the rest, built the same day** (the parts first left out, done at the user's asking):

- **Entity pointers in the cell index** (4.1, in place of integer handles throughout). `World::entitiesIn(cell)` holds
  pointers:
  - every erase marks the index stale
  - a copy of the world starts with none
  - views, motion frames, pawsteps and the trader search no longer look ids up
- **Far wolves less often** (4.3):
  - Wolves more than 24 tiles away go in one motion frame of four, at a phase of each client's own.
  - The frame says it is partial. On such a frame the client drops only a missing wolf near enough that it would
    have been in it, so a wolf gone from sight close by still goes at once.
  - The saving is small in a city, where almost everything a wolf sees is within 24 tiles.
- **Binary snapshots** (4.8, `Core/RatwPack.h`, `Client/src/net/pack.ts`):
  - The same value tree as the JSON, packed: whole numbers as varints, others as float32 where that is exact, and
    strings and keys numbered the second time they appear.
  - The client reads it into the very same objects, so nothing that uses a snapshot changed.
  - The server sends `link::PackedSnapshot`; tests and other hosts still get JSON (`Connection::snapshotValue`).
  - A golden file holds both sides to the same bytes.
  - Snapshots fell from 5.0 to 3.7 KB raw.
- **A network thread** (4.11). `ratw_server`'s sockets are now on a thread of their own:
  - accepting, reading, WebSocket and HTTP parsing, the client's files, writing
  - the latency ping is answered there at once
  - The game thread takes each client's messages from a queue (woken by an `eventfd`) and writes replies into each
    client's buffer, under its lock. The network thread is woken only when a buffer goes from empty to non-empty.
  - Clients are shared pointers, so one gone is never freed under the game.
  - ThreadSanitizer is clean over the whole server smoke (735 checks); the browser and Dungeon Master smokes pass.
- **Terrain shading on the page** (4.6):
  - **The shared rule:** line of sight is now one template (`Core/RatwSight.h`) that `World::lineOfSight` and the
    WebAssembly module both run.
  - **What the page does:** it asks (`"sight": "client"`), works out which tiles its wolf sees when it reaches a new
    tile, and marks seen, remembered and unknown itself. The server leaves its visibility rows out and sends
    `self.sightRange`.
  - **What stays with the server:** which tiles the page knows at all (the glyphs it is sent), and which wolves it is
    shown.
  - **Checks:** a golden of the demo exterior (two heights, a door, tiles hidden behind others) holds the server's
    sight and the page's to the same tiles, with none different.

### Phase 5. Smooth the spikes

1. **Route planning and the schedules pass** run under a per-tick time budget (about 4 ms). Unfinished work carries
   over to the next tick. This replaces the per-tick count.
2. **The stranded residents** (commit 8ec3451): find why residents end up stranded and fix the cause, so the cost of
   a tick stays flat over days of play.
3. **A 30 Hz tick** stays possible once p99 is comfortably under 33 ms. Movement no longer needs it.

Gate: p99.9 under 50 ms at 250 players. Over a simulated week, the mean tick on the last day is within 10% of the
first day's.

**Built 2026-10-02.**

1. **The schedules pass in stages** (`World::continueSchedules`, 3 ms a tick):
   - **The stages:** the society's half-second (one piece), bonds, roads, crime, then the residents' errands.
   - **How it carries over:** the errands go a resident at a time from a cursor. The time is checked every 32
     residents, and what is left waits for the next tick.
   - **Before:** the whole pass ran in one tick, 10–12 ms every second, worst 43 ms.
2. **Route planning by time** (`planWantedRoutes`): as many searches as fit in 3 ms. A long search runs alone; cheap
   ones go several to a tick. At most 8 a tick.
3. **Routes searched on a thread of their own** (`World::setRoutesOffThread`, on wherever the game has its pool):
   - **What changes for a resident:** a route missing from the path cache is handed to the route thread, and the
     resident waits in `routeWanted_` (skipped until it comes back). At the start of a tick the found routes go into
     the path cache, where the resident's next try finds them. The rest of `moveTo` and `headFor` is unchanged.
   - **What the thread reads:** a copy of the cell's ground, kept while its checksum holds, and the closed doors'
     tiles as they were when asked. The search itself (`World::astar`) only reads what it is given.
   - **Why it matters:** single searches of 35–160 ms (Ridgemere's heights expand 90,000 nodes) no longer happen on
     the game thread. The schedules' worst tick at 100 players fell from 43 to 14 ms.
   - **Copies of the world** start with it off, and tests and tools search at once as before. One world test runs a
     resident's day with it on.
4. **A data race fixed:** the society's careers and specs are built lazily, and snapshots on the pool built them at
   once from several threads (`free(): invalid pointer`, found with ThreadSanitizer). `Society::prepareReading` builds
   them before the views, from `World::prepareReading`.
5. **Snapshots on a change of cell** go out together on the pool (`Game::sendSnapshots`), as the regular ones do. They
   were sent one after another on the game thread.
6. **tcmalloc** for the server and `game_load` (CMake finds it; `dnf install gperftools-libs`). Snapshots on 18
   threads were waiting on glibc's allocator: at 100 players the mean fell from 20.8 to 18.2 ms and p99 from 54 to
   44 ms. Sanitizer builds don't use it.
7. **The slowest ticks, broken down:** `RATW_PERF_SLOWEST` lists the five slowest passes of a window. Each shows its
   parts and the world's own (streaming, schedules, movement, separation, views).
8. **The stranded residents** were already fixed (doc 26, commit f1c1eaf).
9. **Measured** (`game_load`, cities, 30 s; Phase 4's numbers in brackets):

| Players | Mean / p99 per tick | World | Views + sight |
| --- | --- | --- | --- |
| 100 | **18.2 / 43** (20.5 / 51) | 10.5 | 6.1 |
| 250 | **33.5 / 77** (37.8 / 95) | 17.3 | 12.4 |

**What the numbers mean on this machine.** The development laptop (i9-13900H) runs the "quiet" power profile with the
powersave governor. Under load its cores run at 0.4–1.7 GHz. A snapshot taking 5 ms alone takes 10–30 ms when all
cores are busy, and that is CPU time, not waiting: the cores slow down together. Spikes at 250 players are now mostly
that, not the game. For numbers that compare with a server, measure on the "performance" profile
(`powerprofilesctl set performance`).

**Gate 1** (100 players, under 25 ms mean and 50 ms p99) **is met.** **The Phase 5 gate** (p99.9 under 50 ms at 250)
**is not met** on this machine: p99 is 77 ms. What remains is the world's own tick (17 ms at 250: residents walking,
separation and the players' memory as they cross tiles) and every core slowing at once.

**The week check** (`world_check --simulate 6 174 --no-check`, DEV's export, 965 residents and one player, 2,016,000
ticks, 1 h 42 min; 2026-10-03). **Not met:** the mean tick on day 7 is 2.7 times day 1's.

| Day | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Mean ms a tick | 1.34 | 1.27 | 2.37 | 2.43 | 2.51 | 3.48 | 3.56 |
| of it, schedules | 0.69 | 0.75 | 1.30 | 1.32 | 1.35 | 1.81 | 2.08 |
| of it, separation | 0.25 | 0.20 | 0.60 | 0.63 | 0.67 | 1.01 | 0.89 |

- **It grows in steps**, on days 3 and 6, not steadily. The schedules and separation (keeping crowds apart) grow
  most.
- **Residents keep asking for routes:** 12.97 million route searches, 99.6% answered from the path cache. That is
  6.4 a tick, all week. At the end 561 residents were on their way somewhere and 379 at their place.
- **In absolute terms it is still small:** 3.6 ms a tick on day 7, p99.9 23 ms, worst 153 ms (a schedules step
  with four route searches; this tool searches on the game thread).
- **Found by the first attempt:** that run hung on day 3. A guard catching a wanted resident ended the pursuit while
  the crime pass was walking the pursuits (`tendCrime`); it now goes on from the guard's place.
  `crime_tests` covers it.
- **Why** (found 2026-10-03 with `world_check`'s new per-day breakdown: the schedules' stages, route searches, and
  the most wolves on one tile): residents stuck for good, piling up, and asking for routes every half second.
  - **A corner trap in the walking rules.** Where flat tiles of 4, 4.5 and 5 meet at a corner, a footprint reaching
    from the 4.5 onto both others was allowed. With its centre on the 4, no step from there was. 21 residents stood
    on one spot at Warden Order, each having asked for a route 7,000–38,000 times. **Now** `step::passable` also
    asks that a wolf could stand where it is from the ground under its own centre, and the route search agrees
    (`World::astar`). A wolf already in such a spot (an older save) is put back in the middle of its tile when
    blocked. `world_tests` (`cornerTrap`) holds it; `walk.wasm` was rebuilt.
  - **Offstage hops into pockets.** Residents travelling offstage took the nearest way into the next cell wherever it
    landed. 31 of Fenhollow's stood on one tile at South Saddle, a pocket hemmed in by trees, with no way on. **Now**
    offstage hops prefer ways that don't land in a pocket (where the cell beyond is in memory to tell).
  - **No way out of a pair of pockets.** A resident in a pocket now clambers to the nearest open ground of the
    cell's body over open ground and features (trees, rocks), up to six tiles, never through walls or over cliffs.
    With nowhere to clamber, they leave by any way at all rather than wait for good.
- **Again with the fixes** (the same run, 2026-10-03):

| Day | 1 | 2 | 3 | 4 | 5 | 6 | 7 | (8, part) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Mean ms a tick | 0.90 | 0.70 | 1.08 | 1.10 | 1.18 | 1.77 | 1.03 | 0.83 |
| Route searches a tick | 0.03 | 0.03 | 0.06 | 0.36 | 0.46 | 0.22 | 0.11 | 0.20 |
| Most on one tile | 19 | 7 | 22 | 25 | 39 | 50 | 92 | 12 |

  - The week's mean tick halved (2.99 → 1.53 ms), and route searches fell from 13 million to 374,000.
  - **The cost no longer climbs:** the days rise and fall. Day 7 is 15% above day 1, so the gate's 10% is just
    missed, but day 8 is below it.
  - **Still to look at:** the crowds of days 5–7 (50 and 92 on one tile, gone by day 8), which may be a gathering
    (a market or festival), and p99.9 (45 ms), now mostly real route searches, which this tool runs on the game
    thread and the server on its route thread.

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

**Measured 2026-10-02** (the development laptop on its "performance" power profile from here on; `game_load`, 30 s,
file saves). Gate 4 (1,000 players) was not run: it takes too long to run every phase, and is for the server hardware.

| Run | Mean / p99 per tick | World | Views + sight | Gate 3 (under 30 / 50 ms) |
| --- | --- | --- | --- | --- |
| 500, cities | **28.4 / 60.5** | 13.1 | 11.2 | mean met; p99 not |
| 500, spread | 41.2 / 87.6 | 17.1 | 18.7 | neither |

What was found and done:

- **`game_load`'s players moved in lockstep.** All of them turned on the same tick every two seconds, so they sent
  their keys and crossed tile edges together, and the memory pass spiked (25 ms). Each now turns at a moment of its
  own (real players don't all turn on one tick). The world's worst views pass fell to 12 ms.
- **The p99 at 500 is the snapshot ticks.** Every 5 s (1% of ticks) a tick takes 60–67 ms:
  - `fork()` itself is 17 ms. It copies the page tables, about 17 ms per GB, and the server holds 1.07 GB at 500
    players: 340 MB of world, then about 1.5 MB a player.
  - Then 15–20 ms more in the same tick. Every page the game writes to after the fork is copied once (copy on write).
  - **Huge pages don't help much:** glibc's `hugetlb` tunable took the fork from 20 to 15 ms, and tcmalloc is faster
    overall.
  - **The journal can't replace the snapshot:** it holds only the valuables. Positions, needs and the rest are only
    in snapshots.
  - At 1,000 players the fork alone would be about 32 ms.
- **Spread out**, each player's view and sight cost about 60% more than in the cities. Their ground differs, so less
  of it is shared or reused.

**Gate 4 on the development laptop** (1,000 players, cities, 60 s, snapshots every 30 s; run once at the user's
asking). Phase 1 measured about 1,400 ms a tick at 1,000 players.

| Mean / p99 / worst | World | Views + sight | Motion | Memory | Per player | Full save |
| --- | --- | --- | --- | --- | --- | --- |
| **50.3 / 77.7 / 109 ms** | 19.2 | 22.1 | 2.5 | 1.78 GB | 197 kbit/s (167 Mbit/s in all) | 350–395 ms, 4.7 MB |

- **Not met** (under 35 ms mean, 50 ms p99). The mean is at the 50 ms budget: on this laptop the server just keeps 20
  ticks a second at 1,000. The gate is meant for the server hardware.
- **The crash from Phase 2 did not come back.** It was most likely the careers race fixed in Phase 5.
- **The snapshot ticks** are 103–106 ms (the fork is 31–33 ms at 1.78 GB, as expected), but there are only two a
  minute now, outside p99.
- **What sets p99 now:** snapshot building on the pool (views 58–59 ms in the worst ticks), the world's own thread
  (19 ms: movement 6.6, its views 8.8), and the schedules (up to 22 ms).
- A 1,000-player run now takes about 80 s, not 15–45 minutes.

**Decided with the user:** keep the journal and the fork, not the full redesign (dirty rows), for now.

- **Snapshots every 30 s, not 5** (`Game::SnapshotSeconds`). It takes the snapshot ticks out of p99, though not out of
  p99.9. A crash loses up to 30 s of positions and needs; valuables are still never lost.
- **If the snapshot ticks still break gate 4 on the server hardware,** the full redesign is the fix. Its save costs
  what changed, not the server's size. It gets its own design doc first.

**What is open:**

1. **Making the snapshot tick cheaper.** Each player's kept state is about 1.5 MB, and a city cell's rows are about
   250 KB of it. Halving it would take a few ms off the fork.
2. **Spread players' views.**
3. **The fallback (splitting by space)** only once these are done, and only if gate 4 fails on the server hardware.

## The health tracker (built 2026-10-04)

A server left running keeps a record of how it ran, so lag can be traced after the fact: when, for whom, and why.

- **Each minute** (each `--perf-log` window) the server records:
  - tick mean, p99, worst, and how many ticks went over 50 ms;
  - each part's mean and worst (commands, world, views, sight, motion, the rest, saves, compression...);
  - the world's own parts (streaming, schedules, movement, separation, views) and its route searches;
  - traffic out and in;
  - players' ping, as their pages report it: how many reported, the middle player's middle ping, the 95th percentile
    of their 95th percentiles, the worst and whose;
  - corrections sent in the minute;
  - slow connections: the most waiting to be sent to any one client, how many have more than 256 KB waiting, frames
    dropped for clients too far behind;
  - the five slowest ticks, with their parts and the world's own.
- **Each tick over 100 ms** is recorded on its own, with where its time went (at most 30 a minute).
- **The page** sends `{"type":"net"}` every 30 s: its ping's middle, 95th percentile and worst over the last minute,
  and the corrections it has had. The network thread answers pings itself, so ping measures the line, not the game's
  queue; the minute's tick times and slow connections show the game's side.
- **Where it goes** (`Core/RatwHealth.h`, on its own thread): `dm.health` for a world in the database (migration 0032,
  kept 14 days), else `<save>.health.jsonl` beside the save. A world in memory keeps nothing.
- **Reading it:**
  - `python3 tools/perf_report.py --database dev [--hours N]` (or `--file PATH`) prints the span in blocks (players,
    tick mean, p99 and worst, ticks over budget, ping, slow connections), the slowest ticks and the worst pings by player.
  - The Dungeon Master tool's **Server Health** tab charts tick times against the budget, players online and ping
    over the last hour, six hours, day or week, and lists the slowest ticks and the worst pings by player.
- **Checks:** `game_tests` (`healthIsKept`): a page's report, a minute's window and a long tick reach the file.
  `test_dungeon_master` (`test_the_servers_health`): the game role writes to `dm.health`, the DM reads it back, PROD
  and DEV apart.

## Fast-forward (built 2026-10-05)

The user wants to put the game into fast-forward, to get through tests that take days or months of game time.

- **`ratw_server --speed N`**, or `/speed N` in the Dev Console (a Dungeon Master's), runs the world N times as fast
  (1 to 1000). `tools/scratch.sh dev --speed 16` passes it on.
- **Nothing is skipped.** Every tick is still the world's 0.05 s; the server ticks N times as often, and as fast as the
  machine can if it can't keep up.
- **What belongs to real time keeps to it** (`Game::tick`'s `real`):
  - motion frames and snapshots to the clients (a frame every N ticks);
  - saves;
  - the database polls;
  - the Dungeon Master's director;
  - the NPC Mind's ambient voices, so a fast world makes no more model calls an hour.
- **`World::setDeterministic`** (`--deterministic` in `world_check` and `econ_watch`) has the schedules finish in the
  step that begins them, and route planning stop by its counts rather than the clock. A run then repeats exactly. Before
  this, the 3 ms budgets made three identical runs end three different ways.
- Two savings found on the way, with the digest unchanged:
  - `spotNear` asks whether anyone stands there before it rebuilds the region map;
  - a resident's station key is built only when the station is its alone.

**Measured** on DEV build 24 with nobody connected: 320 ticks a second at 16x (0.9 ms each). Flat out, about 1,600 a
second, roughly 80x real time:

- a game day in about 3 minutes;
- a 28-day month in about 1.4 hours;
- a year in about 18 hours.

With a player connected a tick costs about 3 ms, so about 17x at most. Going faster takes cheaper ticks: the profile is
now spread thin over residents' decisions (string keys, map lookups), or the work could be split over threads.

### Cheaper ticks and threads (2026-10-05)

The user's aim: a game year (105 million ticks) in one to two hours. It took about 24 hours (0.85 ms a tick, DEV build
24's export, 1,350 residents, nobody connected). Two steps.

**First, cheaper, with every result the same** (the digest and every `econ_watch` file unchanged):

- What a work label or title says is worked out once per thread: `items::producerFor`, `businessFor`, `idlePost`,
  `clergy`, `houseHead`, `smith`, and what pays a post (`payerOf`'s words).
- `items::good` no longer copies the id to look it up. `Society::hasFood` answers "has it anything to eat?" without
  building the name of the best.
- The nearest open shop is found from an index (by cell, by town), not by walking every open shop for every resident.
- A pass works out each cell's community and plan, each home's larder, once.
- Maps keyed by resident are walked alongside the residents (both in ID order), not searched.
- A work or rest place's key is numbers, not text.
- The bodies the society reads are made only on the passes it decides.
- The flags every tick's passes read (`npc`, `dead`, `offstage`...) are first in `Entity`.
- `World::townOf` is indexed.
- That made 0.55 ms a tick.

**Then the user's choice: the same rules, results that may differ but still repeat exactly.**

- **Deciding and doing.** `Society::decideAuthored` is two halves.
  - Every resident decides at once, from where things stood as the second began, spread over the threads
    (`Society::setParallel`, given the world's runner by `World::setParallel`).
  - Then, in ID order, each does what it decided: eats, buys, works, is paid.
  - One whose deciding would change something others see decides in its turn instead, as before: it would claim an odd
    job, take the day's ground out of town, put food away, draw rations, or give up waiting on wages.
  - The results don't depend on the number of threads.
- **The ordered entity list.** `EntityMap` (the world's entities) keeps a list of them in ID order, made again after any
  is added or taken away. The passes every tick (movement, bodies apart, streaming, sight, the Downed, rest, awareness)
  walk it, not the tree. With everyone offstage each of these passes went from about 0.03 ms to 0.003.
- **Errands.**
  - The stage is settled first, and then which cells anyone on it stands in. Elsewhere a goal is nobody else's, and
    `spotNear` isn't asked.
  - A resident's plan is read where it is, not copied.
  - The "task — reason" line is built only when it changes.
- **Loading keeps the threads.** A loaded world (`loadWorldFiles`, a restore) keeps its runner. Before, a world loaded
  after `setParallel` lost it.
- **Threads.** `econ_watch` and `world_check` take `--threads N` (8 unless asked; 1 is the main thread alone). The
  server uses its pool (`--workers`, as before).

**Measured** (`econ_watch` on DEV build 24's export, a quarter day from 06:00, `--deterministic`):

| Threads | 1 | 4 | 8 | 12 |
| --- | --- | --- | --- | --- |
| ms a tick | 0.33 | 0.21 | 0.20 | 0.20 |

- That is about **4.3x** the 0.85 ms before, or **340x real time**:
  - a game day in under a minute (about 56 s);
  - a 28-day month in about 26 minutes;
  - a year in about 5.7 hours.
- More than eight threads gains nothing: what is left mostly runs in order.
  - Doing what was decided: about 0.03 ms a tick.
  - The errands: about 0.04.
  - Finding the open shops, and the bodies: about 0.03.
  - Roads, crime and the rest of the tick: about 0.04.
- Waking sleeping threads costs 50–100 µs a round. Threads that spin while waiting would hide that, at the cost of
  keeping the cores hot, so the pool's threads still sleep.
- **Checked:**
  - The quarter-day digest is unchanged by every step, on 1 to 16 threads.
  - A week against the code before: every figure is the same for five days. From day 6 they differ by a few pennies
    and one or two events. On day 7 there are 121,290 pennies in residents' purses against 121,326, and wages are the
    same.
  - ThreadSanitizer on eight threads (half a day, evening and night): nothing.
  - All 45 tests pass.
- **What would come next**, toward an hour or two a year:
  - Residents nobody can see could decide and act in coarser steps: a tier of their own, as offstage walking already
    is. Each step does several seconds' work; tasks take the same game time.
  - The text keys under it all (residents, accounts, cells) could become numbers.
  - The user chose both (below).

### Unseen residents (2026-10-05)

The user: nobody would notice residents out of sight taking a moment longer to decide. Residents a player may see
(onstage) are as before. One out of every player's sight (offstage, `LifeBody::unseen`) lives in steps of
`Society::UnseenStep` (5) seconds.

- **A step is a step's worth.** Its decision stands for every second since its last (`ResidentLife::decidedAt`, not
  saved): at most UnseenStep, at least one. A step's seconds are counted:
  - in hunger and tiredness;
  - in work done (`progress`; a step past a task's end carries on into the next, so work takes the game time it
    always did);
  - in skill practised, an odd job's progress, and sleep's rest.
- **On the seconds between, nothing is decided for the unseen.** With nobody onstage the world doesn't even make the
  bodies (`Society::needsBodies`); on a new day, or the society's first decisions, it does.
- **Their errands keep the same beat** (`World::unseenErrands_`), except for those keeping up with something that
  moves, every pass, seen or not (`following_`):
  - a caravan's escorts;
  - the watch after someone;
  - a thief after its mark.
- **An offstage walk loses no time.** The next leg sets off from when the last one ended, not from when it is next
  looked at (`moveOffstage`).
- **Measured** (`econ_watch`, DEV build 24, eight threads):
  - 0.060 ms a tick over two game days, 14 times the 0.85 before;
  - a game day in about 17 s, a 28-day month in about 8 minutes, a year in about 1.8 hours;
  - a week took 2.2 minutes against 28.5.
- **The economy over that week, against the code before every change here:**
  - The same within a hair: median purses the same; the Gini within 0.002; wages within about 1% (day 6: 3.7% more).
  - Three more residents of 1,350 broke by day 7 (17 against 14).
  - Balancing that is left for a session on the economy.
  - A run still repeats exactly, on any number of threads.

### Residents by number, and the rest (2026-10-05)

The user asked for the text keys to be replaced by numbers too, "even if the improvements are marginal". Where the
deciding reaches a resident's things it now goes by number. The saved state keeps its names, so nothing outside the
society changes.

- **Residents by number** (`Society::ResidentRecord`, `refreshRecords`).
  - Each resident's place in ID order is its number. Its record holds its life, purse, spec, post, apprenticeship,
    bed, its home's larder and community, and its skill at its post.
  - The records are made again only when something they rest on changes (`rosterRevision_`):
    - the careers or specs reindexed;
    - the positions rebuilt;
    - an account closed, or a resident come or gone;
    - a reset or restore, new beds, or the towns rebuilt (`Society::forgetPlaces`);
    - or a copy of the society (it makes its own).
  - A home's larder and community are looked at again when the home changes.
- **By number in the pass.**
  - Bodies are matched to residents by walking both in ID order.
  - Shops are kept with their keepers' numbers.
  - Practice goes straight to the skill.
- **Fixed words** (`task == "sleep"sv`) are compared knowing their length, in the deciding, the errands and placing
  on the stage. Most comparisons fail at once, on the length.
- **The passes every tick go only over those they may have something to do for** (`World::gatherAwake`): anyone dead,
  onstage, a player, with a Gift, tending someone or Downed. They are gathered after the schedules, the only thing in
  a tick step that changes who:
  - the movement, bodies apart, the Downed and rest walk that list;
  - the crime pass and the stage's cells walk the ordered list.
- **Measured** (two game days, eight threads): **0.042 ms a tick**, 20 times the 0.85 before.
  - A game day in about 12 s, a 28-day month in about 6 minutes, a year in about 1.2 hours.
  - A week in 1.5 minutes.
  - The economy and digest are as with the unseen step alone. In the week's events, two caravans' arrivals come in
    another order: escorts keep up every pass.
- **Checked:** all 45 tests; ThreadSanitizer over a game day on eight threads.
- **Fights** (`level_sim`) were already quick: about 1,000 duels a second over the machine's threads, each fight its
  own small world. Nothing here changes them.

## A year still in an hour and a half, after the economy orchestrator (2026-10-06)

The user: after doc 46's day of work, make sure a year still runs in one to two hours. A year is about 365 days of
288,000 ticks: 0.034 to 0.069 ms a tick on average, the whole year through.

**What was found** (`econ_watch` now prints each day's cost by part, and what the society holds):
- **The cost grew with the days.** It rose from 0.04 ms a tick to 0.075 by day 45, about 0.0008 more each day; a year
  would have taken some five hours. The game before doc 46 grew too (0.043 to 0.064 by day 15), so the hour-and-a-quarter
  year measured on two days was optimistic.
- **Stock entries at nought.** `consume` and `shift` left a good's entry in place when the last of it went, so every
  account kept a nought for everything it ever held: 7,600 entries on day 1, 25,000 by day 14, and every look for food
  (`hasFood`, `bestFood`) scanned them all. They now go as `transfer`'s do (meals, herbs and swords keep their place). This
  also fixed two old bugs that leaned on the noughts: a town posted repair jobs when its works held only noughts, and the
  job then took a nought and mended nothing; and spoilage kept a good's batches while its nought stood.
- **Odd jobs scanned whole.** Every resident deciding looked through all the land's odd jobs up to three times, once
  calling `communityOfResident` for each. The orchestrator's works post up to 60 a day in each town. They are indexed now
  (`Society::indexOddJobs`: who holds one, each town's, the farms' hires), remade when the list changes, before residents
  decide; results unchanged (the same digest).
- **Errands scanned every contract.** `World::errand` looked through every contract and standing order (about 900) for each
  resident on each errand pass. A pass now indexes who has taken what (`World::indexErrands`); results unchanged.

**Measured** (DEV build 26 export, eight threads, a quiet machine): from day 9 the cost holds at about 0.048 to 0.052 ms a
tick (0.039 to 0.043 on Restdays) through day 54. The roads stage grows with the contracts kept (done and expired ones for
30 days) and levels off with them, at about 800, by day 35.

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
