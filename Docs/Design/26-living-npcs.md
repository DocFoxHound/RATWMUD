# 26 — Living NPCs at scale

Status: plan agreed 2026-09-27. Phase 0 (below) is built and measured; everything after it is planned.

## What we want

NPCs that hold AI conversations, remember the players they meet, form relationships with players and with each other,
and live as convincingly as the world allows. Some can be killed; a death leaves a gap in the market and in the lives
around it, which other NPCs fill slowly, through relationships and opportunity, never by instantly inheriting a job,
home and family. New NPCs arrive when nobody can fill a gap, and they arrive in the world (by road, at a gate), not by
appearing in a shop. Between the towns there are caravans, couriers (on contracts from NPCs or players), bandits and
military patrols.

All of this has to fit in the server's 50 ms tick, with many players, and the tools must stay quick to open.

## Principles

- **Fidelity follows attention.** Full physical simulation only where a player could see it. Everywhere else the same
  rules run coarser, and the world is still consistent when a player arrives.
- **Rules decide, language performs.** Money, goods, jobs, relationships and deaths change only through validated
  game rules. A language model writes speech and may *propose* bounded intents (a promise, an offer, a small change of
  opinion) that the server checks. It never mints money, moves goods or changes state by itself.
- **Everything meaningful is an event.** Trades, deaths, promises, conversations and changes of relationship are
  recorded once, and memory, rumour, chronicles, the Dungeon Master's audit and debugging all read from that record.
- **Measure before and after.** Every performance change is checked with `world_check` (below). An optimisation that
  should change nothing must leave the *people* digest identical.

## Phase 0: stop the tick from falling over (built 2026-09-27)

Profiling the DEV world (434 residents, 503 places) showed the NPC simulation was not the main cost: **players** were.
With 20 walking players the tick averaged 57 ms, over budget on every tick. Each walking player's view (thousands of
sight rays) was recomputed every tick for map memory, and all players' snapshots were built in the same tick.

| 434 residents, 7:00–7:12 | Before | After |
|---|---|---|
| 1 player: mean tick | 10.6 ms | 5.0 ms |
| 20 players: mean tick | 56.7 ms | 19.1 ms |
| 20 players: p99 / worst after the first minute | ~89 / ~140 ms | 30 / 37 ms |
| People digest (positions, activities, tasks) | — | identical |

The first minute still includes one slow tick (about 0.75 s) while every occupied cell loads and every route and
region cache is built. That is start-up, not play.

What changed:

- **Sight rays** (`World::lineOfSight`) check each tile a ray crosses once instead of every 0.12-tile sample, with
  exactly the same result. A randomized test compares 40,000 rays against the per-sample version
  (`sightRaysMatchSampling` in `Tests/world_tests.cpp`). A view looks fixtures up in a flat mask.
- **Map memory while walking**: `World::tick` takes a player's view when the player enters a new tile or cell, not every
  tick. The server's five-a-second snapshots still observe from the exact position. Over 12 minutes with 20 players,
  memory holds 99.8% of the tiles it held before.
- **Snapshots are staggered**: each client still gets five a second, but a quarter of the clients in each tick.
- **Pathfinding**: closed doors come from a flat mask, and each node's footprint is worked out once per search (exact).
- **Separation** compares characters only with others in the same cell (exact: same pairs, same order).
- **Resident specs** are found by hash instead of a linear scan per resident per decision.
- **Saves**: chat, NPC conversation turns, doors and colour changes no longer each make the game wait on a whole-world
  save (a chat line used to cause two). They ask for a background save within three seconds (`SaveSoon()`). Economic
  and account changes still save before replying. `live.npc_state` rows are rewritten only when they changed.
- **Dialogue**:
  - The language model now receives both sides of the current conversation and the ends of up to three earlier ones
    (`MemoryStore::recallForDialogue`). Before, it got only the player's previous line, cut to 160 characters.
  - Lines said to a busy NPC are queued instead of dropped.
  - Names match whole words.
  - The bridge answers up to three conversations at once.
- **Measuring**: `world_check` gained `--players N` (walking players spread over the world), `--no-check`, a per-part
  breakdown of `World::tick` (`World::tickProfile()`), the steady state after the first minute, and a digest.

```
build-core/world_check EXPORT_DIR --simulate 7 7.2 --players 20 --no-check
```

An export of a build is what `world_build.game_files(conn, build_id)` returns, written out as files.

Finished afterwards (same day):

- **Saves are built off the game thread.** `CaptureState()` copies what a save needs; the persistence worker turns the
  copy into the document (`BuildState`, `BuildNpcStates`) and writes it (`FRatwPersistence::SaveInBackground(FBuild)`).
- **The Storykeeper snapshot** keeps each cell's terrain rows until its ground changes (a fingerprint of its glyphs)
  instead of rebuilding millions of characters every two seconds, and is encoded and written on a worker, one at a time.
- **Loading a place** takes 2.2 ms on average instead of 8.3 (worst 22 ms instead of 95), without the database:
  - Height overrides, tens of thousands in an outdoor cell, are read by a small exact parser instead of a string stream
    each, with a bitmap for duplicates.
  - A load or unload updates only that cell's door list instead of rebuilding the index of all 72,000 doors and seams
    and dropping every route cache.
  - Routes between cells use the manifest's exits for a streamed cell's seams, so no route depends on which cells
    happen to be in memory. (This changed the tie-break between equally short routes; everyone still has one.)
- **Cells are fetched ahead.** `FRatwCellPrefetch`, on its own thread and connection, fetches the ring of cells beyond
  those in memory (`World::cellsSoonNeeded()`), so a load rarely waits on the database. It logs how many loads found
  their cell ready.
- **The Python hosts pool their database connections** (`world_db.Pool` / `pooled`). Atlas's 1.5-second heartbeat and
  the Dungeon Master no longer open a connection, and log in, for every request.

## Phase 1: simulation tiers (built 2026-09-27)

**Stage and offstage** (`World::setTiered`, on by default for streamed worlds, which is the live server):

- **The stage** is where full simulation runs, as before: A* paths, collisions, doors, perception, 20 Hz. It is each
  player's surroundings (`World::nearCells`):
  - their cell
  - the cells across its open edges
  - the buildings whose doors are within 24 tiles
  A city square's hundred shop doors are not all "next to" someone at one end of it.
- **Offstage** is everyone else. A resident's needs, work, trade, wages, sleep and schedule are decided exactly as
  before; the society runs the same rules for every resident. Only *travel* changes:
  - it goes in timed hops between known-good places: to the nearest door or edge crossing towards the goal, then its
    arrival point on the far side, and finally onto the goal spot
  - each hop takes the straight-line distance × 1.3 at the resident's walking speed
  - no path search, no collisions, no posture timing, and the cell need not be in memory
  - a cell's edge crossings are remembered from the first time it loaded (`seamAnchors_`), so an offstage resident
    crosses unloaded ground freely
- **Switching tiers.** A resident joins or leaves the stage at its schedule update (twice a second) wherever it
  stands. That is always a place it could stand, because walking only reaches such places and hops only end on them.
  It then plans again from there.
  - **Companions** follow their leader and are always onstage.
  - **Saves** hold only valid places. A saved NPC whose footprint is no longer clear (a rebuilt street) now takes its
    authored body instead of failing the whole checkpoint.
- **Streaming follows the stage.** Cells stay in memory around players and onstage residents, not around every
  resident. Crossing into anything else still loads it on demand, and the prefetcher (Phase 0) makes that cheap.
- **Views are prepared together.** `World::prepareViews` works out the sight of the players about to be observed or
  snapshotted on several threads at once. Each view only reads the world. ThreadSanitizer is clean, and a test shows
  the prepared sight and map memory equal the plain ones.
- **Bigger populations.** A world may now author 16,384 residents (was 1,024), with room for 8,192 player accounts. The
  loader checks duplicate IDs with a set instead of comparing every pair. Atlas's limit matches.

Also found and fixed while measuring Phase 1 (each exact, with the people digest unchanged):

- **Separation.** In large crowds it compares only pairs in neighbouring buckets. Its "is either at a doorway?" check
  looks only at portals within reach (`nearPortal`) instead of every seam record in the cell.
- **Scent cues** in a snapshot looked through all 72,000 doors of the world for the cell's closed ones, and built the
  air map even with nobody unseen to smell. Now it uses only the cell's doors, and only when needed.
- **Path smoothing** checks the tiny footprint's single tile when all five points share it.
- **Route budget.** Route searches share a budget of 100,000 expanded nodes per schedule update, as well as six
  searches. A resident over it sets off half a second later.

**Measured** (`world_check`, 7:00–7:36, i.e. six real minutes; "steady" means after the first minute; 20 players each
stand beside a different resident, which spreads the stage over the whole world, the worst case):

| World | Players | Full simulation | Tiers | Residents offstage | Places in memory |
|---|---|---|---|---|---|
| DEV, 434 residents | 1 | 9.2 ms mean | **2.0 ms** mean, steady p99 5.6 ms | 396 | 91 (was 418) |
| DEV, 434 residents | 20 | 20.5 ms mean, steady p99 50 ms | **13.6 ms** mean, steady p99 22 ms, worst 34 ms | 313 | 190 |
| 3× population, 1,302 | 1 | 14.7 ms mean | **4.3 ms** mean, steady p99 15 ms | 1,188 | 125 |
| 3× population, 1,302 | 20 | 30.5 ms mean | **20.5 ms** mean, steady p99 42 ms, worst 119 ms | 939 | 266 |

In every run money was conserved and every resident had a route. More residents reached their scheduled place with
tiers (430 of 434, against 408 with full simulation over twelve minutes), since offstage hops never get stuck.

The 3× world copies each resident twice onto the same home and job, so its crowds and bed contention are harsher
than a real one. Its remaining spikes are single large route searches in Ridgemere (up to about 100,000 nodes and
70 ms).

**Paths are remembered** (`World::findPath`). A route search is keyed by everything it depends on: the cell, its
ground (the region checksum), which of its doors are closed, the exact start and goal, and whether closed doors may be
passed. Residents repeat their ways every day, so a repeat is a lookup, and the answer is always the path a search would
give (the people digest is unchanged).
- Up to 16,384 paths are kept.
- Within the benchmarks' few game hours, 15% of searches were answered this way (45% in the 3× world). The share grows
  on a live server, since a game day is four real hours.
- The 3× world's slowest steady tick fell from 119 ms to 67 ms.

Still open:
- a coarse route over each large cell before the fine search, for the rare first search that is huge
- a persistent thread pool for views and snapshots (today's helpers are started per tick)

The planned event queue for offstage residents turned out unnecessary. Checking a hop's arrival time is a single
comparison per resident per update, cheap next to the society's own decisions, which run for everyone anyway. The
integer-ID and string-enum data layout is deferred: after the changes above, profiles show no cost in string keys.

Tests (`Tests/stream_tests.cpp`):
- An offstage resident walks home across 18 unloaded cells, through each in order, in about walking time, fetching
  each cell at most once and leaving nothing in memory.
- A player arriving next door brings her onstage onto open ground, where she walks a planned path; she goes back
  offstage when the player leaves.
- A save made mid-journey restores.

## Phase 2: the event log and delta saves (built 2026-09-27)

**The event log** (`game.events`, migration 0021) is one row per thing that happened, never changed afterwards. Each row
records the kind, actor, target, cell, world time and calendar day, item, quantity, coins and a short detail.
- **What is logged:**
  - Every economy ledger entry: sales, wages, restocking, dues. The society keeps a journal of them; its saved ledger
    keeps only the latest 128.
  - Deaths, revivals, relocations.
  - Arrivals, departures and new characters.
  - Spawns and clearances.
  - Operator actions.
  - Conversations with NPCs. The log says *that* two characters talked, never what was said; the words stay in the
    NPC's memory, as before. An unrecognised voice stays anonymous.
- **How it gets there:**
  - The world collects events (`World::recordEvent`, `takeEvents()`, at most 50,000 uncollected).
  - The server hands them to the persistence worker at each save, and they are written in the same transaction as
    that checkpoint (`game.record_events`).
  - Text is cut to the table's limits and anything that isn't valid UTF-8 is dropped (`ratw::eventsJson`), so no
    event can make a save fail.
- **Who may do what:** the game server may add rows but not change or delete them; tools may read.
- **Older databases:** a database without the migration keeps working, logging nothing and saying so at start.
- **Next:** NPC memory, rumours, chronicles and the Dungeon Master's audit will read from here.

**Delta saves** (`game.save_checkpoint_delta`, migration 0022):
- **What gets sent.** The persistence worker keeps, for every list in the save (characters, NPC bodies, map memories,
  conversations, NPC memories, the social ledger...), a fingerprint of each entry and its place. It sends only entries
  that are new, changed or moved, plus the keys of removed ones. The keys are exactly those of `game.sections`,
  including numbered repeats.
- **When a save is whole instead:** the first save after start, after any failure, every 20th save, and after any save
  handed over as finished text. Anything changed behind the server's back is therefore soon put right.
- **Synchronous saves** (trades, logins...) are built on the worker too, with the game waiting as before, so the
  worker's record of the database stays exact.
- **Older databases:** a database without the migration gets whole saves.
- **Checking it:** `RATW_VERIFY_SAVES=1` makes the worker check after every delta that `game.load_checkpoint` reads
  back exactly the whole document (jsonb equality), and log `RATW_SAVE_VERIFY ... matches`.

**Verified on a copy of DEV** (the live dedicated server on a scratch database with both migrations, four minutes, a
restart, and another run):
- 23 delta saves across three runs, every one matching the whole document.
- The restart restored cleanly.
- 312 economy events logged.
- Each delta sent about 345 KB of a 911 KB save. Most of what remains is the checkpoint row itself (the society's
  per-resident needs, door states, weather), which is one document; splitting the society's residents into rows would
  be the next saving.
- With nobody connected, every resident was offstage and the cells in memory fell to none.

**The social tables** (relationships, beliefs, households, positions with apprentice places, contracts, caravans) come
with the phases that give them rules, so none is created before there is something to keep in it.

## Phase 3: relationships, memory and the NPC Mind (built 2026-09-27)

**Relationships** (`Core/RatwBonds.h`, `World::bonds()`).
- **What a bond holds.** Every character's regard for another is one-sided, with affinity, trust and respect (-100..100),
  familiarity and fear (0..100), what is owed, and the day of last contact.
- **Only rules move them.** The events the world records drive them (`World::bondsFromEvent`):
  - A sale or a wage honestly paid: a little familiarity, trust and liking, both ways.
  - A conversation: familiarity and a touch of liking.
  - Help or a gift warms the one helped.
  - Harm breeds dislike, distrust and fear in the one harmed.

  Only characters have bonds; the treasury does not.
- **Time together.** Every game hour, residents at home with their household or at work with those working there grow
  more familiar. In a crowded bunkhouse each comes to know a few neighbours, not everyone.
- **Growth and fading.** Growth slows near a limit, so the tenth kind word matters less than the first. Each game day
  acquaintance fades without contact, strong feelings cool and fear passes; trust, respect and debts stay. An empty bond
  is forgotten.
- **Limits.** Each character keeps at most 150 others, and the faintest go first. A resident who leaves the world is
  forgotten by everyone.
- **Saving.** Bonds are saved as the save's `bonds` list: table `game.bonds` (migration 0023), one row per holder and
  other, written as deltas like the rest. A database without the table keeps them in the checkpoint row.
- **Measured.** 1,302 residents formed about 3,000 bonds in 36 minutes, at no measurable cost to the tick.

**Conversations know the relationship.** The dialogue context now carries:
- the speaker's ID when the NPC recognises them
- how the NPC regards them, in words (`Bonds::describe`: "You know Ash well, like them and trust them a little. Ash
  owes you 3 pennies.")
- the NPC's mood after its last reply

**The NPC Mind** (`tools/npc_mind.py`, replacing the test bridge for real play):
- **Persona first.** The NPC's lasting identity (name, description, personality, backstory) is the system message, so
  a provider can cache it. The scene, what was heard, memory, the relationship and history follow.
- **History.** With `--database dev|prod` the pair's recent dealings come from the event log: "Day 3: Ash paid you 6
  pennies for 1 meal (resident food purchase)."
- **Structured, checked replies.** A reply is the words, an emotion (one of nine), how the exchange moves the NPC's
  liking and trust (-3..3), a short private note, and a promise if one was made. Every field is checked and cut or
  refused. Speech is cut at a sentence.
- **Load control.** Budgets limit replies per hour overall and per minute per speaker; over budget the game uses its
  authored line. A request waits briefly for one of a few slots, else it is refused as busy.
- **Summaries.** `POST /summarize` summarises a finished conversation from the NPC's point of view, reporting claims as
  claims.
- **Rehearsal.** `--fixture` gives offline replies with no model and no cost.
- **What the game server does with a reply** (`ratw::mind::Client::converse`, `Game::heed`): it checks every field again.
  - The emotion becomes the NPC's mood.
  - Nudges move a recognised speaker's bond by at most 3 a reply and 6 an hour in each, so flattery can't buy
    adoration.
  - A note is kept as "(your note)" in the conversation's memory.
  - A promise is kept as "(their promise)" or "(your promise)" in memory and logged as a `promise` event.
  - Nothing a reply says moves money, goods or anything else in the world.
- **Summaries in the game** (`Consolidate`). When a conversation closes after an hour's quiet, its extractive summary is
  written at once as before, and the Mind's summary replaces it when it arrives (`MemoryStore::rewrite`).
- **Older setups.** A text-only provider (the old bridge, the test fixture) still works; its replies simply carry no
  more than their text.

**Verified:**
- `tools/test_npc_mind.py`: 18 tests, including history read from a real scratch event log.
- `Tests/bonds_tests.cpp`: 29 checks.
- `tools/mind_smoke.py`: the real game with the Mind in fixture mode.
  - Rowan's reply is exactly the Mind's line.
  - His trust in the player rises by the promise's nudge.
  - The promise and a note are in his memory.
  - The bond survives a restart.
  - A conversation closed after an hour is summarised by the Mind.

**Not yet** (and where it fits):
- **Rumours**: beliefs that spread between residents. They travel with the road network in Phase 5.
- **Embedding-ranked memory** (pgvector): today recall is by subject and recency.
- **Streaming replies.**
- **Promises the world tracks**: kept or broken, and debts from them. These come with households and positions in
  Phase 4, where there is something to keep a promise about.

## Phase 4: households, positions, apprentices and succession (built 2026-09-27)

**Positions** (`Position`, `Society::positions()`, `Core/RatwCareers.cpp`).
- **What a position is.** Every authored resident's job becomes a position the town has: its title, role (merchant,
  guard, civilian), workplace, counter, hours, route and pay. It outlives whoever holds it.
- **Who holds it.** Each founder starts in their own. The daily routine reads a resident's job from the position they
  hold.
- **Apprentices and the unemployed.** An apprentice with no position of their own works beside their master in the
  master's hours, unpaid. Anyone with neither looks for work.
- **Proof it changed nothing.** With nobody dead this is exactly the old behaviour: the people digest is unchanged.

**Households and family.** A household is those sharing a home. Family is the household members who share a surname
(Holly and Laurel Ashwalker at the Gatehouse Inn).

**Skill** (0..100 per resident and position):
- It grows with hours worked at the post, more slowly near mastery.
- An apprentice beside a master who is also there learns three times as fast.
- Founders start as skilled as their years suggest.

**Apprentices.**
- **Who takes one on.** A master at least 35 years old, or skilled 70 or more, takes on one apprentice. Not at once:
  about once a week the chance comes.
- **Who is chosen.** First the master's own family (12–25 years old), then the household, then a youth the master
  knows and trusts who works alongside them, then an unemployed youth of the same town. Guards don't take
  apprentices.
- **When it ends.** At skill 70 the apprenticeship is complete.
- **Players.** A player can ask an NPC close by through the new *Apprentice* action. The master accepts only someone
  they know and trust (regard 30 or more from their bond), and a player learns one trade at a time.

**When someone dies** (`Society::died`, `tendCareers`, once a game day):
1. **At once.**
   - Their position stands empty; a shop is shut, since only its holder keeps it open.
   - Anyone close grieves: family for five days, friends (liking 20 or more, or familiarity 40 or more) for two. A
     grieving NPC talks as one: "Grieving for Holly Ashwalker, who died recently", and sad by default.
2. **After a day** the estate is settled, split evenly among the living family and otherwise given to the town. It is
   existing money and goods only, recorded as `inheritance`, and the books still balance.
3. **Filling the post:**
   - From the second day, the apprentice steps up if skilled 30 or more.
   - From the third, family carry the trade on, if it is a step up for them (keeping a shop, then the watch, then paid
     work, then unpaid). The son takes over the stall; the stallkeeper doesn't leave it to run errands.
   - From the fifth, someone local who is out of work takes it.

   Whoever moves up leaves their own job empty in turn, so a death ripples slowly through a town.
4. **If they are brought back first** (the Dungeon Master), their job and estate are still theirs.

Every step is an event in the log: `vacancy`, `mourning`, `estate settled`, `succession`, `apprenticeship`,
`apprenticeship completed`, `returned to work`.

**Saving.** Careers are saved with the society (positions, skill, mourning, estates waiting). A save from before careers
starts everyone in their own job; an unreadable careers section never makes a save unreadable.

**Verified:**
- `Tests/careers_tests.cpp`, 34 checks: positions and family; Hale Brook takes his son Tam as apprentice, Tam learns,
  Hale dies, the estate is split between Tam and Ivy to the penny, and on the second day Tam keeps the stall while his
  errand job falls empty; revival before replacement; no family, so the estate goes to the town; players as
  apprentices; saving and restoring, including old saves; grief in the world.
- **On a copy of DEV** with the live server:
  - On the first career day five apprenticeships formed across Ridgemere and Ser Ferro.
  - A Dungeon Master kill of Holly Ashwalker left "keeping the inn" vacant, with her sister Laurel mourning.
  - Everything was logged and saved.

**Added the same day** (first versions with placeholder numbers):

- **Newcomers.** A post nobody here has taken in eight days sends for a stranger (`ResidentRequest`).
  - A live world adds them to `live.npcs` as a runtime resident: cloned from the last holder's record, with a fresh
    name and age, a bed found near the old home or the work, and no job of their own (work label "-").
  - The server takes them in like any spawn and gives them the post (`World::welcomeResident`), knowing something of
    the work (skill 35).
  - A world from files can't add people, so it only notes the want.
  - Verified on a copy of DEV: Sedge Thistledown came to take up "helping at The Gatehouse Inn" and was still there
    after a restart.
- **Marriage.**
  - About once a week, two unmarried adults (18–60) who each like the other 50 or more and know them 60 or more may
    marry.
  - The one from the smaller household moves in (`Society::moveHome`). A save may carry an essential worker's move
    only if it is to their spouse's home, so operators still cannot move essential workers.
  - Spouses and parents count as family.
- **Births.**
  - A married couple at home together, the mother 18–45, may have a child: about one a season, a year apart at least,
    three at most.
  - The child is made like a newcomer (age 0, a parent's surname), grows up without working until 16, and is counted
    in the family.
- **Promises the world keeps track of** (`World::promise`, `promisesBetween`). A promise from the NPC Mind becomes a
  record due in three days.
  - It is kept if the two trade, pay, give or help while it is open: trust grows, and "promise kept" is logged.
  - It is broken if it falls due first: trust falls further than keeping raised it.
  - Open promises are part of the NPC's context in conversation.
- **Skill that matters.**
  - Paid work finishes faster for the skilled (`workPace`: 1 at skill 50, 0.75–1.25).
  - A placeholder table of skill families (craft, trade, labour, watch, service, travel, general; `skillFamily`)
    carries half of one's best skill in a family to a new job in it.
  - The NPC's standing at their trade ("a master of it", "skilled", "capable", "still learning") is part of the
    conversation.

## Phase 5: the roads (built 2026-09-27; placeholder numbers throughout)

`Core/RatwRoads.h`, `RatwRoads.cpp`; `World::roads()`.

- **Towns.** A town is a region (the cells' territory) where at least five people live and there is a market. On DEV
  there are three: Upper Accord, Ridgemere and Ser Ferro.
  - The capital is where people arrive (the spawn's region), and its store is the treasury.
  - Every other town has its own store (`stores:<town>`), given its share of the treasury's goods once, by
    population.
  - Merchants restock from their own town's store and pay into it. Once a week the stores send what they took in
    back to the treasury, which pays the wages.
  - A world with one settlement keeps the treasury as its one store, exactly as before.
- **Caravans.** Every morning a caravan leaves the capital's market for each other town, carrying that town's share
  of the day's goods.
  - Its load is its own account, so goods really move.
  - Its wagon is in the world (see "In person" below), and goes along the road (the cell graph) at walking pace.
  - On arrival its load goes into the town's stores, and the carters tell the town's merchants what they heard in the
    market they came from; that is how rumours cross between towns. Then it goes home empty.
- **Bandits.** Camps are placed in wild cells along the roads (one in six to begin with).
  - **Odds.** A loaded caravan passing a camp is robbed with odds of boldness (strength × hunger) against its guards
    and the carters' caution.
  - **A raid.** The load is taken and eaten, which feeds the camp. The town posts a bounty on the camp and asks for an
    escort for the next caravan. Its merchants hear of it.
  - **Lying low.** A camp that has just raided lies low for two days.
  - **Hunger.** Hunger grows daily; a starving camp dwindles and scatters, and where a road has no camp one may
    gather.
  - **The watch.** A town's watch goes after camps with a price on them, with better chances the more guards it has,
    and may clear them.
- **Contracts**, the reward set aside at once from the poster's purse (or the treasury) and paid or returned:
  - **Bounty:** done when the camp is gone (the watch; the Dungeon Master, or later combat, through
    `completeContract`).
  - **Escort:** its taker goes with the next caravan to that town, adding two to its guards while there, and is paid
    on a safe arrival (see "In person" below).
  - **Supply:** posted when a town's store runs low on food; done by selling goods to that town's merchant.
  - **Courier:** a resident who cares for someone in another town pays to have a letter carried. It is done when its
    taker stands before the recipient; untaken for a week, it goes with the carters.

  Unfinished contracts expire and their rewards go back. Players see the work near a merchant (*Ask for work*) and
  take it (*Take k12*).
- **Rumours** (`World::believe`, `rumoursAbout`; table `game.beliefs`, migration 0024). What a character has heard: a
  claim about someone or something, who told them, and how sure they are.
  - **Where they start:** witnesses (a death, harm, a marriage, a newcomer), those it happened to (a broken or kept
    promise), mourners, and the markets told of a raid.
  - **How they spread:** each day everyone tells the few they know best what they are surest of, at 70% of their
    certainty. Rumours fade by 3% a day, and each character keeps at most 30.
  - **In conversation:** "You have heard that Ash breaks promises (from Moss; fairly sure)."

**Verified:**
- `Tests/roads_tests.cpp`, 41 checks, with money conserved throughout:
  - A caravan delivers its whole load, and the carters bring the capital's news to the west market.
  - A strong, starving camp robs a caravan: loot eaten, bounty and escort posted, the market told.
  - A player escorts the next caravan and is paid; a letter delivered in person pays and earns trust; a supply run is
    done by selling to the town's merchant.
  - Rumours pass from one to another less surely, and survive a restart.
- **On DEV** (`world_check`, across midnight): three towns and three camps. Each day's caravans set out, some get
  through and some are robbed, and money stays conserved.
- **The live server on a copy of DEV:**
  - The towns' stores were stocked.
  - Two loaded caravans set out and bandits gathered on a road.
  - After a restart the same caravans were still on the road and none were sent twice.

### Second pass (built 2026-09-28): the road in person

What the first version left for later, built on the same day's numbers (all placeholders).

- **In person.** The road's folk are characters in the world (`Entity::transient`, IDs `road:...`), made from the
  roads' own state as needed and never saved themselves. They are left out of saves, bonds, rumours, careers and the
  NPC Mind: players can see them and inspect them, but not talk to them.
  - **The wagon.** A caravan's wagon walks the road where someone is near, and goes in timed hops elsewhere, like any
    resident offstage (the same `headFor` as residents' schedules). It stands in the market while it waits for its
    escorts, up to two game hours.
    - A wagon stuck ten minutes in a cell is moved on to the next.
    - A wagon with no road on turns back, and what it carries goes back to its town's stores.
  - **Escorts.** Escorts must be there, and are counted only while they are: in the wagon's cell or next to it.
    - Each is paid on arrival only if there at the end and for at least half the cells. Otherwise the reward goes
      back, and a player is told so.
    - Players travelling with a caravan see what happens to it: bandits who weigh up its guards and let it pass, or
      fall on it.
- **Bandits in person** (spawned only when needed).
  - **Only near.** A camp's bandits are in the world only while someone is in or next to its cell: one for every
    three points of strength, up to six. The first of them is their leader.
  - **Stopped.** A player in their cell with a purse is stopped when the camp is bold (hungry or strong, and not lying
    low), and asked for part of it. The player can pay (*Pay*), get clear, or fight (*Attack*).
  - **Patience.** After twenty seconds the bandits lose patience.
  - **Fights** (`World::attack`; a placeholder until there is a combat design).
    - Stamina is what a fight wears down; blows and swings cost it. Strength and dexterity set the odds and the
      damage.
    - A player beaten to the ground is robbed of half their purse (or what was asked, if more), not killed, and left
      alone for a game hour.
    - Felling the leader, or every bandit, breaks the camp. The camp's takings and any bounty on it go to whoever did
      it, and every market hears who drove the bandits off.
    - With half of them down, the rest run and lie low.
  - **The takings.** What bandits take from players is held in the camp's account (`bandits:<camp>`), so money is
    still conserved.
- **Residents take work** that players have left for two days.
  - Someone out of work in the writer's town carries a letter, walking it there (their errand overrides the day's
    plan, except sleep).
  - One of the town guard takes an escort when the caravan sets out, and walks the road with it.
  - Errands are derived from the contracts, so nothing new is saved.
- **Trade between towns.** Each day a town other than the capital with plenty of food or herbs for its people sends a
  caravan to one with little.
  - It carries up to twelve of each.
  - On arrival the buying town's stores pay the selling town's a wholesale price (three a meal, one for herbs), as far
    as they can.
  - Bandits camp along every road between two towns, not only the capital's.
- **Prices follow scarcity town by town.**
  - Once a game hour, each store's price factor is worked out from what it holds for its people: half a meal and a
    quarter of a bundle of herbs each is enough.
  - The factor runs from 0.85 (plenty) to 1.6 (none), and multiplies what the trader's own stock already does.
  - A merchant's prices follow the store they restock from.

**Verified** (`Tests/roads_tests.cpp`, now 118 checks, money conserved throughout):
- **Escorts in person.** A player who walks beside the wagon is paid on arrival. One who never comes is waited for,
  then left behind, and their pay goes back.
- **Bandits.** They appear only when someone is near, and are never saved. They stop a traveller, are paid off and
  hold the coin, and let them be. They beat and rob one who neither pays nor leaves, without killing them.
- **A fight.** A fight with the leader breaks the camp and pays the bounty, and the west market hears of it.
- **Residents.** A resident out of work carries a letter to another town in person and is paid. A town guard walks
  with a caravan and is paid.
- **Trade and prices.** With three towns, the one with plenty sends food to the one with none, and is paid for it.
  Food costs more where the stores are empty.
- **On DEV** (`world_check`, 20 players across midnight): 7.7 ms mean tick (p99 21.5 ms), money conserved. There
  are four caravans on the road: two from the capital, and two trading between Ridgemere and Ser Ferro.

**Not yet:** residents taking bounties or supply runs themselves (the watch still does the bounties); a player
following a wagon by command rather than by walking; bandits other than on the roads; a combat design to replace the
placeholder fight.

## Phase 6: the backend at scale (built 2026-09-28, except a stripped server binary)

### Built

- **Delta snapshots** (`Source/RATWMUD/Runtime/RatwSnapshotSections.h`).
  - **What changes seldom.** A snapshot's big parts change far less often than the five times a second it is sent:
    the cell's ground and heights, what the wolf can see, the world and travel maps, the doors, the satchel.
  - **Keys.** Each part goes with a key, a hash of its content. The client acknowledges each snapshot it applies
    (`ServerSnapshotAck`, unreliable).
  - **Leaving out.** The server then leaves out every part whose key the client is known to hold, and the client puts
    its kept copy back (the newest six of each part) before anything uses the snapshot. The widget, the tools and the
    tests see whole snapshots, as before.
  - **Unreliable delivery.** Snapshots travel unreliably, so the server relies only on what an acknowledgement proves
    arrived. A client missing a part asks for everything again, and a new session starts from nothing.
  - **Turning it off.** `-RatwFullSnapshots` sends every snapshot whole.
- **Binary motion frames** (`ratwmotion::Pack`, `Unpack`). The twenty-a-second pose frames are binary on the wire:
  stamps, then each pose's ID, x, y, facing and whether it's moving (single precision). Malformed frames are refused
  whole.
- **Tools.**
  - **Cached ground.** Atlas and the Dungeon Master keep each cell's ground (terrain rows and heights, the slow part of
    loading a world) between loads.
    - It is checked each time against the revision, the time of change, and the row versions and counts of the
      terrain chunks and cells, so an edit, a publish or a world made again is never served stale.
    - Each caller gets its own copy.
    - DEV's world loads in 0.9 s after the first time (7.8 s before).
  - **Compressed responses.** Large text and JSON responses are gzipped for a browser that accepts it and isn't on
    this machine (`tools/http_body.py`). A local one gets them as they are, which is faster there.
- **Launch.**
  - **No needless builds.** `tools/world_build.py dev` (and so `live.sh ... dev`) builds DEV only when it has changed
    since the newest build. It compares a fingerprint of the world's revision, its live layers, the roster and the
    exporter's code (migration 0025); `--force` builds anyway. Unchanged, a launch skips the half-minute export.
  - **The cooked package.** `RATW_PACKAGED=1 bash tools/live.sh server ...` runs the cooked package's headless host
    (`tools/run-packaged.sh server`) instead of the editor.

Verified:
- **Engine tests.** `RATW.Network.DeltaSnapshots` covers:
  - parts left out only after an acknowledgement, and put back exactly;
  - a changed part sent again;
  - a client that lost its copies noticing.

  `RATW.Network.BinaryMotionFrames` covers the round trip, the size against JSON, and refusal of cut, foreign or
  padded frames.
- **Smokes.** The two-client network smoke and the persistence smoke pass over real connections.
- **Tools.** `tools/test_world_store.py` covers stale ground. `tools/test_publish.py` covers build reuse against
  edits, live layers and the roster. `tools/test_http_body.py` covers compression.

### Built after (2026-09-28)

- **Atlas and the Dungeon Master load only what is in view**, heights in a compact encoding: see doc 20, "Massive
  worlds", step 4.
- **The roster preview sends only what it depends on**, and only when that has changed: the slots, who is already
  named, and the tiles beside each work spot (`/api/roster/plan`; a few kilobytes, however large the world), where it
  used to send the whole world after every edit.
- **World-map reveals as increments.** Each map entry (a cell and everything remembered of it) is held on its own: an
  entry the client holds goes as `{"$held": key}`, so a reveal costs one cell, not the whole map. Tested in
  `RATW.Network.DeltaSnapshots`.

- **A standalone headless world server, with Unreal as the client** (`Server/ratw_server.cpp`).
  - **One game, two hosts.** The whole server side is portable C++ in `Core`: `ratw::game::Game`
    (`RatwGame.h`) holds the world, accounts and characters, commands, snapshots and events, the NPC Mind client
    (`RatwMind.h`), the Dungeon Master's bridge (`RatwDirector.h`), spawns, saves and releases. The Unreal server
    (`RatwGameMode.cpp`) is now a thin adapter that hosts it behind Unreal's networking; the standalone server hosts
    it over TCP. The two can no longer disagree.
  - **Portable pieces under it.** The checkpoint codec (`RatwCheckpoint.h`, the save's JSON without Unreal types),
    the database store (`RatwDbStore.h`), accounts (`RatwAccountsCore.h`), delta sections (`RatwSections.h`),
    binary motion (`RatwMotionCore.h`), and zlib and OpenSSL loaded at run time (`RatwSystemLibs.h`).
  - **The wire** (`RatwLink.h`): length-prefixed frames (command, snapshot acknowledgement, event, snapshot,
    motion), each server payload zlib-compressed. Unreal connects with `-RatwServer=host:port`
    (`RatwRemoteLink.h`); `RATW_STANDALONE=1` in `tools/connect.sh` and `tools/live.sh` picks it.
  - **Numbers.** Ready in 2.4 s on DEV's database world (under 0.01 s on a test world). On DEV a walking player got snapshots of about 4.8 KB and motion frames of about
    280 bytes. Ticks averaged 0.1 to 0.3 ms on test worlds and about 10 ms on DEV.
  - **Verified.** The network, persistence, Dungeon Master (23 checks) and Mind smokes pass against both servers
    with real Unreal clients (`--standalone build-core/ratw_server`), and a save moves between the two servers
    both ways on a DEV copy. `Tests/server_smoke.cpp` (340 checks) and `Tests/game_tests.cpp` (94) drive the game
    headless; `Tests/server_parts_tests.cpp` covers accounts, sections, the Mind client and the bridge's request
    contract (moved there from the engine tests with the Unreal-only copies they tested).

### Not yet

- **A stripped dedicated-server binary.** This installed engine refuses Server targets (see the README). The
  standalone server now fills that role: no Unreal on the server at all.

## Next phases (order agreed 2026-09-28)

Phase 6's stripped server binary comes first; then:

- **Phase 7: crime and law.** Theft and violence create witnesses and beliefs. The Watch investigates from what it
  knows, not from omniscience.
- **Phase 8: NPC chronicles.** A chronicle of each NPC's life, compiled from events, for the Dungeon Master and for
  storytelling.
- **Phase 9: schedules.** Market days, festivals, a day of rest, and weather changing plans.
- **Phase 10: the ambient director.** A local director that picks one or two NPC-to-NPC exchanges worth voicing where
  players are: gossip about a recent event, rivals arguing.

## Open questions

- How long a game year is in play time, which sets apprenticeship terms and succession pace (a game day is four real
  hours).
- Which NPCs are protected, and who decides: authors in Atlas, or the Dungeon Master live.
- Hosting and cost ceilings for the language models.
