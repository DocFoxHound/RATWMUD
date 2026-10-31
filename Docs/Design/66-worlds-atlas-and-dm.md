# 66. Worlds, Atlas and the DM: many worlds, NPCs without a map, one tool, safe pushes (2026-10-30)

The user, 2026-10-30:

- "We need to be able to switch servers easily, and we need the NPC'S to be unset to a business and home and
  essentially be able to exist in a state that isn't tied to a map."
- "Merge both Atlas and DM into one tool, but keep them as separate suites. Atlas is made for building and editing
  maps and all static components. It can also build and assign NPC's to buildings and jobs."
- "In some cases you may have 3 or 4 different worlds that are side projects, while 1 of them may be a static copy of
  a live server that you want to edit. ... we need to make sure there is some robust conflict management when the user
  decides to push the changes from the Dev world to the Prod world. That switch and conflict resolution should not be
  managed in Atlas but in DM. Atlas should have a way to pull the diff from the live server and choose which conflicts
  to retain."
- "If you wanted to add a few buildings to Ridgemere you would close the game server down for a little while for
  maintenance, tell Atlas to update the dev server from the prod server, manage conflicts, make your changes, and then
  save. Then in DM ... you would import the Dev world to Prod, resolve any remaining conflicts, and then turn the
  server back on."
- "The prod server should be able to switch worlds using DM. I would heavily suggest people don't switch worlds but it
  needs to be possible. ... Dev server can also switch between many worlds, but that is done in Atlas."
- "DM is the live game management tool without map editor tools, and Atlas is the dev management/development tool
  without storyline tools and mainly map/building/NPC/faction/etc tools."
- And: erase all the economy rules made so far; keep items, buildings and shop types.

Builds on doc 20 (the world database, Push to live), doc 21 and 34 (the Dungeon Master), doc 64 (the economy).

## Where it starts

- Two databases, `ratw_dev` and `ratw_prod`, one world each. "dev" and "prod" are written into some forty places:
  `tools/world_db.py`, the launchers (`live.sh`, `scratch.sh`, `mind.sh`), `world_build.py`, `publish.py`,
  `dungeon_master.py` (`TARGETS`), `map_editor.py`, `dm_service.py`, and the game server (`Game::loadFromDatabase`).
- 2026-10-30, by hand: a third database `ratw_ridgemere2`, "Ridgemere 2.0", copied from DEV with its map taken away
  (its 1,083 NPCs, their talk and relations, the 626 buildings kept; the old placements in
  `artifacts/ridgemere2/old-map-placements.json`). The tools take it as the working world (`world_db.WORKING`).
- An NPC's home, work and evening areas are required (`live.npcs`, NOT NULL), so 556 work and 128 evening areas
  point at a holding room, `r2_unplaced`, for now.
- Atlas (`tools/map_editor.py serve`, `Editor/index.html`) and the DM (`tools/dungeon_master.py serve`,
  `Editor/dm.html`; the Storykeeper `tools/dm_service.py`) are two hosts on two ports over one front-end codebase.
- Push to live (`tools/publish.py`) makes PROD's authored content equal DEV's, and live data add-only: an NPC that
  exists in PROD is never updated. No three-way comparison, no conflicts to choose.

## 1. Worlds: a registry, and switching

**The registry**, `Database/worlds.json` (in git), lists every world:

    {"worlds": [
      {"key": "prod",       "name": "Ridgemere (live)", "database": "ratw_prod",       "kind": "live"},
      {"key": "dev",        "name": "Ridgemere (dev)",  "database": "ratw_dev",        "kind": "dev", "upstream": "prod"},
      {"key": "ridgemere2", "name": "Ridgemere 2.0",    "database": "ratw_ridgemere2", "kind": "dev"}
    ]}

- `kind`: **dev** worlds are edited in Atlas and run on local and scratch servers; **live** worlds are what players
  join, run and managed from the DM.
- `upstream` (dev worlds only): the live world it is a working copy of. Only a dev world with an upstream can pull
  from live or be imported into it. Side projects have none.
- Each database holds one world, as now. Schemas never drift: `world_db.py migrate` covers every registered world.

**Local choices**, `Saved/Config/worlds.local.json` (not in git): `{"atlas": "ridgemere2", "live": "prod"}`.

- `atlas`: the dev world Atlas, the world builder, local and scratch servers and the talk/voice tools use by default.
  Switched in Atlas (or `world_db.py use KEY`).
- `live`: the world the live game server runs. Switched in the DM only, only during maintenance, with a warning.

**Commands** (`tools/world_db.py`):

- `list`: the worlds, their kind, upstream, database, which is Atlas's and which is live.
- `use KEY`: Atlas's world.
- `new KEY --name NAME [--kind dev|live] [--upstream KEY] [--from KEY [--without-map]]`: a new database, empty with
  every migration or copied from another world (`--without-map` does what was done by hand for Ridgemere 2.0:
  NPCs, talk and buildings kept, their map places left unset), registered, roles granted.
- `serve-world KEY` (what the DM calls): the live server's world.

Every launcher and tool takes a world key and defaults to the local choice; the game server takes any registered key
(the launcher checks it against the registry; the server no longer hard-codes prod and dev).

## 2. NPCs without a map

`live.npcs.home_area`, `work_area` and `evening_area` (and `live.profession_slots`' three) become optional
(migration 0052, on every world).

| An NPC with | Is | In the world |
|---|---|---|
| home, work | placed | as now |
| home, no work | placed, without work | spawns at home; idle (no job, no wage) |
| no home | **unplaced** | no body: it exists (name, talk, relations, memories, roster entry, job if it has one) but stands nowhere, isn't simulated on the map and can't be met |

- Unplaced is a state, not a place: no holding room. Ridgemere 2.0's `r2_unplaced` goes and its NPCs' areas are
  unset.
- The build/export (`world_store`, `map_editor` exporter, `world_build.py`) writes unplaced NPCs into the roster
  without a body; the game server loads them as off-map residents and spawns only the placed. Placing an NPC later
  (Atlas, a push) spawns it at its home.
- Atlas lists unplaced and jobless NPCs and assigns a home, a business (a work area and post) and an evening place;
  the DM views tolerate NPCs with none.
- The simulation counts an unplaced NPC as a resident who is away: no needs, no work, no money moving.

## 3. One tool, two suites

One host, `tools/studio.py serve` (`bash tools/studio.sh`), on one port, serves one app with two suites and a switch
between them. One sign-in (the DM accounts, `dm.admins`, kept in PROD as now).

| | Atlas | DM |
|---|---|---|
| For | building worlds | running a live world |
| Works on | dev worlds (its world switcher) | live worlds |
| Has | map and terrain, cells, buildings and interiors, doors, places, resources, patrol routes and NPC areas, NPCs and their homes, jobs and businesses, professions and the roster, factions and their claims, talk and voice tables, playtests, **Update from live** (with conflicts) | the server (start, stop, maintenance, the live world switch), **Import from dev** (with conflicts), releases and rollback, live NPC life (alive/dead, moves), players, reports and artwork review, the calendar and festivals, money and the levers, health, storytellers, chapters and storylines (the Storykeeper) |
| Hasn't | storylines, the live server's controls | map editing tools |

- The two old hosts become modules of the new one; their routes keep their paths under `/api/atlas/...` and
  `/api/dm/...`. `editor.sh` and `dungeon-master.sh` start the one tool on its suite.
- Route and NPC-area editing and NPC creation move from the DM to Atlas; the DM keeps live life (kill, revive, move,
  visit) and the storyline tools.

## 4. Pull and import, with conflicts

**The base.** A dev world with an upstream keeps the snapshot of its upstream as of its last pull or import (table
`admin.sync_base` in the dev world: upstream key, the live release it saw, when, and the content: every synced row by
its key, as JSON). A world copied from live starts with that copy as its base.

**What is synced**, row by row, each by its key: authored content (cells, terrain chunks, interiors, links, places,
resources, chapters, the world row) and live data (NPCs, profession slots, professions, roster characters, patrol
routes with their posts, NPC areas, spawns, factions with their claims and relations, NPC voices, kin, dialects,
figures, story visitors). Never synced, and never touched by either: the game's player data and history
(`game.*`: accounts, characters, memories, the event log, the save), and the economy's state.

**Three-way**, for each key: base B, dev D, live L.

| | Result |
|---|---|
| D = L | nothing to do |
| D = B, L ≠ B | live changed: take live's |
| L = B, D ≠ B | dev changed: keep dev's |
| both changed, differently | **conflict**: shown field by field; the user keeps dev's or live's (or, for an NPC, field by field) |
| removed on one side, changed on the other | conflict |

**Atlas: Update from live** (a dev world with an upstream). Reads live, compares, applies every live-only change to
the dev world, lists the conflicts for the user to choose, applies the choices, and makes live's state the new base.
Warns if the live server is running (live data may still move; the DM's import will catch it).

**DM: Import from dev.** Only during maintenance. Compares again (live may have moved since the pull) and lists what
the import will change and the remaining conflicts; the user resolves them; one transaction writes live, records a
release (number, who, note, summary, the authored content for rollback, as now), and becomes the dev world's new base.
References are checked as now: nothing live may be left in a removed area. Players' data is never written.

**Maintenance and the switch.** The DM sets the live world to maintenance: the game server tells players, saves and
exits (status 76), and its launcher waits. The DM imports, or switches the live world (refused outside maintenance;
warns and asks again), then turns it back on: the launcher starts the server on the chosen world.

Push to live's add-only rules and its "copy NPCs back" are replaced by these.

## 5. The economy rules erased

On Ridgemere 2.0 (branch `ridgemere2`): the economy's rules come out of the code: docs 42, 61 and 63's mechanisms,
doc 64's thirteen phases and every balancing round (the WageRule switches). Kept: money (accounts and coins), the
item, recipe, craft, building and shop-type catalogues, shops holding stock at their catalogue prices, buying and
selling. NPCs' lives (schedules, talk, relations) stay; what their needs bought (food) is paused, so nobody starves,
until the user's own rules say how they are met. `econ_watch` and the economy tests that test removed rules go with
them.

## Phases

1. Worlds: the registry, the commands, every launcher and tool, the server.
2. NPCs without a map: the migration, the build, the server, Atlas's list and assignment; Ridgemere 2.0's holding
   room removed.
3. One tool, two suites.
4. Pull and import with conflicts; maintenance and the live world switch.
5. The economy rules erased.

Phases 1, 2 and 5 can be built side by side; 3 needs 1; 4 needs 1 and 3.

## Progress

### Phase 5: built (2026-10-30)

Branch `r2-p5-no-economy`, from `ridgemere2` (6c4a1b5). About 40,000 lines out, 1,200 in: Core −18,400, Tests −21,800.

**Removed.** The simulated economy's mechanisms, whole:

- Doc 64's phases 0 to 13 and every balancing round: work by the hours and the realm's pace, the books, the worth
  ladder's pricing (costed prices, margins, markdowns, carriage), production by hours (makers' batches, producers'
  yields, seasonal fields, tools' wear, upkeep, laying in), businesses' tills as owned businesses (owners topping up,
  stopping, idling and returning, credit, rent, working money), the labour market (employers' wages, going rates,
  hands by sales, hires, poaching), odd jobs, crews and farmhands, households' purses, keepers, wants, pieces and works,
  children's stipends, institutions' budgets, relief and alms, taxes, the holdings tax, the mint, wages of every kind,
  kept hands' meals, NPCs' belongings and mending, the economy's monitor, and the WageRule and WageRule2 switches with
  every rule behind them. Files: `Core/RatwDemand`, `RatwPrices`, `RatwLabour`, `RatwInstitutions`, `RatwProcure`,
  `RatwReckoning`, `RatwWages`, `RatwOutwork`, `RatwHouses`, `RatwTills`, `RatwFounding`, `RatwOddJobs`,
  `RatwFarmhands`, `RatwBelongings`, `RatwMint`, `RatwHouseholds`, `RatwSurplus`, `RatwOrchestrator(.h, Json.h)`,
  `RatwOrchestrate`, `RatwTrade`, `RatwCrafting`; `RatwSociety.h` from 2,721 lines to 646, `RatwResidents.cpp`'s day
  from 1,500 lines to 450.
- Doc 42's: town purses (the treasury's share at founding), the weekly reckoning, the church's Restday plate and its
  care, rents, living costs, the rule against hoarding, the Town Works and the towns' repair, outwork, the daily
  carter's and the towns' caravans of food, supply runs, contracts for goods (procurement), store-priced meals and
  herbs.
- Doc 61's: Houses owning trades, the towns' specialties, standing orders and their porters (`Data/Economy/houses.json`
  and `specialties.json` deleted). Doc 63's model is gone from the game (`tools/econ_model` untouched).
- Doc 57, 5's trade that moves prices (price notes, "fed the town"), the town talking of prices, residents buying at
  players' stalls, town projects' hired hands and contracts for materials, the town's mending project.
- The DM's levers: `tools/dungeon_master.py`'s tax rate, holdings tax, mint and relief grant (and their routes), the
  money view's month, levers and monitor; `Editor/src/dm/OrchestratorPanel.tsx`; the game's `economy.*` actions and the
  Dev Console's `/reckon`; `world_check --reckon`.
- `Tests/econ_watch.cpp`, `econ_players*`, `econ_watch_stamp.cmake`, `crafting_tests.cpp`, `orchestrator_tests.cpp`,
  `founded_fixture.h`, `tools/econ_run.sh`, `econ_compare.py`; the economy's tests in `roads_tests` (4,774 lines to 830)
  and elsewhere.
- Saved state: the society's `books`, `houses`, `memory`, `orchestrator` and `craftingStocked`; the roads' standing
  orders, `sentEarly`, `failed`, `refused`, `stocked`, `purses`, a caravan's trader and orders, a contract's goods. An
  older save still loads: those fields are read past (`wire::readSociety`, `checkpoint`), and a procurement contract in
  it simply runs out.

**Kept.** Money (accounts in whole pennies, `shift`, `sale`, `create`, `consume`, the ledger and the journal,
conservation); players paying and being paid (contracts, couriers, bounties, escorts, lodgings, stalls between players,
tables, festivals' entries, letters, fines, the Hold's staff at `camp::HoldWage`, companions' hire); the catalogue
(`Data/Items`: items, qualities, recipes, crafts, producers, businesses, stations, regions; `Data/Economy/ladder.json`
for the items' worth and the catalogue's day); buildings; players' crafting and foraging; shops: each merchant post's
till (`till:<position>`, founded once with its keeper's wares and its founder's authored purse's worth), its wares
(`Society::wares`), at the catalogue's price (`Society::catalogPrice`; a tenth off at a stall), buying from players at
`SellBackShare` (.55) up to a store of a good (`SuppliesKept`, `GoodsKept`); NPCs' lives: schedules, posts as titles,
careers, skills and apprentices, succession, families, births, deaths, ageing, retirement, newcomers (with
`NewcomerPurse`), estates inherited, households splitting, Restday's service (the plate gone), festivals and market
days, patrols, travellers, wander areas, children's friend groups, talk, relations, troubles (the shop's loan gone), the
Mind. The Great Houses as people (a head, its family, a leader in talk), not owners. The demo roster's own little
economy (doc 26's six) is untouched. The caravan machinery is kept but nothing sends one (`World::sendCaravan`, public).
The DM's money view stays, as a read of where the money is.

Kept with a change: a player's farm or bench work beside a resident (doc 53) still practises and lends the paw, but
pays nothing and brings nothing in; the Hold's workshops and fields are built but make nothing; a talk speaker's
`on_dole` is "no" and `business` "none"; a trouble's household is reckoned in days of `troubles::PlainFoodADay` (5p) at
catalogue prices; the hungry thief marks a shop whose till holds meals. A town project's placement now loads its cell
first (no caravan walks a road to load it any more).

**Paused.** Needs that buying met: `Society::NeedsPaused` (true). A resident's hunger neither grows nor sends it
anywhere (no shopping, larders, alms, rations or kitchens); one hungry from a save still eats what it carries. Home
stores are opened empty. Nobody restocks a shop: what it sells stays sold until rules say how goods come in.

**Where the user's rules plug in.**

- Needs: `Society::NeedsPaused` and the line in `Society::decideAuthored` (`RatwResidents.cpp`, the first half) where
  hunger would grow; the "eat" branch below it.
- A day's rules: the day's turn in `Society::decide` (`RatwSociety.cpp`, where `budgetDay` moves on), before
  `decideAuthored`.
- Work: `atWork` in `decideAuthored`'s second half: a resident at its post in its hours (trade, watch, its title).
  Making and paying go there; `Society::giftLiftOf` and `pawRateOf` keep the Gifts and paws players lend a maker.
- Prices and shops: `Society::catalogPrice`, `quote`/`trade`, `tillOf`, `foundShops`, `wares`.
- Goods between towns: `World::sendCaravan` (`RatwRoads.cpp`); a caravan's arrival unloads into the town's stores.
- Players' work for residents: `World::workBeat` (`RatwTogether.cpp`); a Hold's work: `Game::campTick`
  (`RatwGameCamps.cpp`); town projects: `Game::projectTick` (`RatwGameProjects.cpp`).
- The primitives every rule uses: `Society::shift`, `sale`, `create`, `consume`, `openAccount`, `cash`.

Tests: 66 of 66 (`shops_tests` new: the catalogue, a shop's till, a player's trade at catalogue prices, a day and a half
with nothing bought, made or paid and hunger held, an older save read). A scratch server on the town world started and
ran.
