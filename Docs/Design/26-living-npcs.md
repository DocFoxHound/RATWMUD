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

**Relationships** (`Source/RATWMUD/Core/RatwBonds.h`, `World::bonds()`).
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
- **What the game server does with a reply** (`FRatwDialogueProvider::Converse`, `Heed`): it checks every field again.
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

**Positions** (`Position`, `Society::positions()`, `Source/RATWMUD/Core/RatwCareers.cpp`).
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

`Source/RATWMUD/Core/RatwRoads.h`, `RatwRoads.cpp`; `World::roads()`.

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
  - It crosses a cell every two and a half minutes along the road (the cell graph), offstage like a resident.
  - On arrival its load goes into the town's stores, and the carters tell the town's merchants what they heard in the
    capital's market; that is how rumours cross between towns. Then it goes home empty.
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
  - **Escort:** its taker joins the next caravan to that town (+2 guards) and is paid on a safe arrival.
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

**Not yet:**
- **Players and residents on the road in person.** Caravans and couriers are offstage hops; a player sees a caravan
  only as events and its effects.
- **Encounters with bandits for a player,** and so bounties done by players.
- **Residents taking contracts themselves** (beyond carters carrying letters).
- **Trade between the non-capital towns.**
- **Prices that follow scarcity town by town.**

## Phase 6: the backend at scale

- **Networking.** Delta snapshots: cell tiles once on entry, then entity changes; world-map reveals as increments;
  binary motion frames.
- **Tools.**
  - Atlas and the Dungeon Master load only what is in view (doc 20 step 4).
  - Heights use a compact encoding.
  - Responses are compressed.
  - The roster preview sends only what changed.
- **Launch.** Don't re-export the world on every `live.sh` launch when nothing changed. Use a cooked dedicated server
  instead of `UnrealEditor -server`.
- **A standalone headless world server.**
  - `Source/RATWMUD/Core` is already portable C++, and the Unreal runtime mostly moves compressed JSON strings over RPCs.
  - Moving the authority into its own process would give second-scale start-up, ordinary profilers and sanitizers, a
    separate thread or process for the T2 world, and room for region processes later.
  - Unreal would become the client.
  - Best done after Phase 1 shows what the server core needs.

## More ideas to fold in

- Schedules with market days, festivals, a day of rest, and weather changing plans.
- A local **ambient director** that picks one or two NPC-to-NPC exchanges worth voicing where players are: gossip about
  a recent event, rivals arguing.
- A **chronicle** of each NPC's life, compiled from events, for the Dungeon Master and for storytelling.
- **Crime and law**: theft and violence create witnesses and beliefs. The Watch investigates from what it knows, not
  from omniscience.

## Open questions

- How long a game year is in play time, which sets apprenticeship terms and succession pace (a game day is four real
  hours).
- Which NPCs are protected, and who decides: authors in Atlas, or the Dungeon Master live.
- Hosting and cost ceilings for the language models.
