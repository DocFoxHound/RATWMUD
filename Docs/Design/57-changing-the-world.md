# 57. Changing the world: residents' troubles, town projects, and an economy players can see

Drafted 2026-10-06 as an actionable plan for doc 48 (Part 1: §§1.1–1.6). Nothing built. Read doc 48 (Principles,
Part 1 and the Decisions) and docs 56 (deeds), 53 (cooperative work), 46 (the economy orchestrator), 26 (Phases 4, 5
and 7), 32 (Part 5), 35 (Part 7), 42 and 15 first.

Marks: **(agreed)** is the user's decision in doc 48; *(placeholder)* is a number or name that is one constant to change;
**(new)** is a choice this plan makes, listed under Decisions for the user to confirm. **⚑ economy** marks a change in
files another session owns (the orchestrator and the society's economy code); every one is gathered under
"Changes in the economy session's files".

## The ask

> "We need to look into making autonomy matter more (player's choices mattering in the game world [...] I have a
> feeling it leans towards NPC's, but maybe even economical impact and impact on cities/towns/farms/wolves)."
> (doc 48, The ask)

Doc 48 agreed "Players improving residents' lives, farms and towns, remembered fondly" (decision 9). Its Part 1 holds
the shape, marked *proposed*: four tests for a choice that matters, levers at each scale, residents' troubles drawn
from real state, town projects as the main new system, an economy whose effects players can see, and bounds so players
can't break the world. What RATW lacks is mostly the *seeing*: "players rarely find out what their actions changed"
(§1).

## Where we stand (read from the code 2026-10-06)

**What a trouble could be read from.** Doc 48 §1.3 names six troubles. Checked against the code, three of its sources
don't exist:

| Doc 48 says | What the code has |
|---|---|
| A debt they can't pay (purse) | **Residents have no debts.** `Bond::owed` exists (`Core/RatwBonds.h:20`) but nothing in Core ever sets it. The only loans are business rescues: `EconomyMemory::loans` (till → lent, day; `RatwSociety.h:214`), lent by the orchestrator's rescue channel and repaid from what a business holds above two floats, or written off after a month (`RatwChannels.cpp:289–323`). Households pool their money daily (`tendHouseholds`, `RatwHouseholds.cpp:17`). |
| Lost or broken tools (doc 35 durability) | **Residents' tools don't wear.** Durability is for players' gear only (`World::wearGear`, `RatwDurability.cpp:45`). A producer buys its tool every few days and uses it up at once (`Society::tradeUpkeep`, `RatwDemand.cpp:1001`). If no shop has it, a missed sale is counted and work goes on as before. |
| A sick or injured family member (doc 38) | **Residents get no injuries and there is no sickness.** `Entity::injuries` is for players only (doc 38, "As built"). `RatwHealth.*` is the server's performance recorder. |
| A feud (bonds) | Exists: one-sided bonds −100..100. Affinity −25 or worse fires ambient quarrels that sour both further (`RatwAmbient.cpp:253`, `:561`). |
| No work | Exists: a resident with no post is "growing up", "retired", in an outwork trade, or "labouring for the Town Works", paid by the town (`decideAuthored`, `RatwResidents.cpp:508–549`). The orchestrator counts idleness (`TownReading::idle`, `TownSnap::unfilled`). |
| A child of age with no apprenticeship | Exists: apprentices are 12–25, matched once a week per post (`tendCareers`, `RatwCareers.cpp:583–620`). Masters are 35+ or skilled 70+, and never the watch. Players can already apprentice (`Society::apprentice`, `:683`). |

**Settlements.** Town buildings aren't runtime objects: they are terrain and interiors from the generators (doc 39).
- Each community has one repair value, `EconomyMemory::condition` (0–100, `Society::condition`, `RatwSociety.h:923`).
  It wears a point a day (two in winter) and is mended by the Town Works' used materials (`townBuyers`,
  `RatwDemand.cpp:794–915`) and "repairs" odd jobs (+1.5 each, `RatwOddJobs.cpp:101`). Crossing 50 makes a "disrepair"
  or "mended" event.
- There is no road condition; bridges are only a terrain type. There are no water chores and no raids on towns:
  bandits rob caravans and stop travellers (`caravanEntered`, `RatwRoads.cpp:629`; raid odds
  `bold / (bold + guards × 8 + 20)`, `:654`).

**The structure layer exists, for Chapters only** (`Core/RatwCamps.h`): `Kind`, `Structure {site, kind, x, y, work,
condition, built}`, `Site`, `Camps::work` (build or mend) and `Camps::wear`; 17 kinds (`RatwCamps.cpp:14`); members
working together at one structure (`Game::campTick`, `RatwGameCamps.cpp:276`; a work-hour is a real minute of one
wolf). Walking and weather read built structures (`Game::refreshGround`; `World::obstacles`, `shelters`). They are
saved in `game.camp_structures` (migration 0030) and drawn on players' maps, in Atlas and in the DM's Chapters layer.
Camps can't be made in towns.

**Money.** Every purse is a `Society` account; `Society::shift` moves coin and goods, `consume` removes goods, `record`
books everything, and `conserved()` checks the supply against what was minted and sunk (doc 15). The journal saves
valuables at once (`Game::record`, doc 31). Contracts escrow their reward (`World::postContract`, `RatwRoads.cpp:202`);
residents fill "procure" contracts too (`World::residentsFillContracts`, `RatwProcure.cpp`). Odd jobs
(`Society::OddJob`, 1–5 hands, paid by the poster) are for residents only.

**The orchestrator** (doc 46, owned by another session) measures each day from a `Snapshot` and decides once a week
(`Core/RatwOrchestrator.h`).
- Readable from outside: `Society::orchestrator()` (the last `Brief`, with each town's `TownReading {distress, kind,
  idle, short_, poor, foodCost, shopFoodDays, …}`), `Society::townPrice`, `dayWage`, and the granaries
  (`town:<t>:granary`, `Dials::granaryDays`).
- `Society::noteForOrchestra` (`RatwOrchestrate.cpp:159`) counts flows between towns and residents' earnings. Player
  accounts belong to no town, so **nothing counts what players bring**.
- Holders are recognised by account prefix (`stores:`, `town:`, `house:`, `till:`; `RatwOrchestrate.cpp:~367`).
- Its decision 4: **invisible in the world**, no writs or news of its orders.

**Prices players can move.** Meals, herbs and swords follow each town's stores hourly (`World::tendPrices`,
`RatwRoads.cpp:421`, filling `priceSeen_`), and ambient talk mentions a move of a tenth or more (doc 30). Everything
else is the orchestrator's weekly town price. Caravans carry up to 8 rumours to the next town (`World::caravanArrived`,
`RatwRoads.cpp:768`); escorts count only while present (`:745`).

**The DM.** Actions go through `dm.actions`, audited in `dm.audit` (`tools/dungeon_master.py`). The LIVE map has a
Structures layer (world rooms) and a Chapters layer (camps). **There is no undo** for live actions, only inverses
(revive, unsteer); doc 34 Part 8 plans *Reverse*.

**Protected residents: none.** No flag exists. Relocation alone protects essential jobs (`World::relocateResident`,
`RatwWorld.cpp:3030`). Doc 26 left "which NPCs are protected, and who decides" open.

## Scope

**This plan builds** the four tests as a checklist; the levers it owns; **residents' troubles**, from what the
simulation really holds; **town projects** on the structure layer; the **economy's visibility** (prices moved by
players, guarded caravans carrying news, players' flows in the orchestrator's books as an interface the economy session
builds); and **bounds** (protected residents, wear, the DM's reversal, no terrain).

**It leaves to other plans:**

| Doc | What |
|---|---|
| 56 | Deeds, nicknames, the chronicle, welcome back. This plan *makes* deeds through `Game::recordDeed`. |
| 53 | How much faster wolves work together (the cooperation rate, two angles). Project labour uses it. |
| 54 | The notice board that shows projects. This plan supplies the projects' view. |
| 55 | Residents' thank-you letters after a trouble is solved, from doc 56's deeds. |
| 52 | Vouching for a *player*. This plan's "speak for them" vouches for a *resident* to an employer. |
| 58 | Personal storylines, which may start from a trouble. |
| 46 | The orchestrator itself. This plan names what it would read and expose, and changes nothing in it. |

## Design

### 1. The four tests (doc 48 §1.1)

Every lever in this plan is checked against them. The table says what makes each test pass:

| Test | Troubles | Town projects | The economy |
|---|---|---|---|
| **Seen** | The resident's state changes in front of you ("the shop is out of debt"), and doc 56's welcome card later says what became of them | The structure rises on the map; the board shows progress; a plaque names the givers | Town talk says *why* a price moved, naming who brought the goods as the town knows them |
| **Lasting** | Simulation state: a post, an apprenticeship, a business that survives, a feud ended | A structure that keeps working until it wears out | Prices and stocks that hold until trade moves them again |
| **Spoken of** | A deed (doc 56): the resident, their household, then gossip | A notable or great deed for the chief givers; criers at the festival | Ambient talk of prices with their cause; news riding guarded caravans |
| **Costly** | Coin, a fee, trust earned first, time spent bringing two residents together | Materials, labour and coin, all real (doc 15) | Goods carried, roads guarded |

### 2. Levers at each scale (doc 48 §1.2)

| Scale | Lever | Where it is built |
|---|---|---|
| A wolf | Solve a resident's trouble; speak for them to an employer; pay an apprenticeship fee; pay off a debt | This plan, §3 (the troubles) |
| | Testify or post bail; escort a resident to a new town; introduce two residents who may marry | **Later.** They need the crime code, doc 16's relocation and the marriage rules to take a player's part (Open question 4). |
| A farm or business | Work the harvest; lend a paw | Doc 53 |
| | Rescue a failing shop | This plan (the shop's rescue loan) |
| | Bring supplies | Already built (supply and procure contracts); this plan makes it seen (§5) |
| | Mend tools; invest in a business | **Later:** residents' tools don't wear yet; a share in a till is a new kind of holder (the economy session) |
| A town | A town project | This plan, §4 |
| | Sponsor a festival | Doc 54 |
| | Guard caravans; clear bandits off a road | Already built (escort and bounty contracts, camps). This plan and doc 56 make them spoken of (§5). |
| A region or faction | Chapter standing, missions, sides | Doc 32, built |

### 3. Residents' troubles (doc 48 §1.3)

A resident may carry **one current trouble**, read from their real state, never invented.

**The troubles this plan builds** (data in `Data/Town/troubles.json` **(new)**: kinds, order, thresholds, lines):

| Trouble | Who has it | Read from | How a player solves it | What changes | Deed (doc 56) |
|---|---|---|---|---|---|
| **Short of coin** | The head of a household | The household's purses and larder hold under 7 days of plain food at the town's price (`TownReading::foodCost`) *(placeholder)* | Give coin or food (a gift, doc 55) enough to bring it to 14 days | Their purse or larder | Small |
| **The shop is in debt** | A keeper whose till holds a rescue loan | `EconomyMemory::loans` | Pay into the till enough to clear the loan; the business repays it by its own rule at the next daily pass | The loan is gone, so it is not written off | Small; notable at 100p or more *(placeholder)* |
| **A feud** | Either of two residents of one community | Affinity −25 or worse both ways, or −40 one way *(placeholders)* | *Make peace*: a wolf trusted by both (trust 30+) brings them within 4 tiles of each other and of the wolf; or carries a gift from one to the other (doc 55) | Affinity both ways +25, trust +10, and the ambient quarrels stop | Small; notable between two families' heads; great between two great houses' heads (`Society::houseHead`) |
| **No steady work** | A grown wolf 16–64 with no post: labouring for the Town Works, or idle 3 days | `Society::jobOf`, the work title, the earnings the society keeps for the orchestrator | *Speak for them* to a keeper or farmer with a vacant post they can fill, who trusts the wolf (30+) | They take the post | Small; notable if idle a game week or more |
| **A child with no trade** | A parent of a youth 12–25 with no apprenticeship or post | `CareerState` and the careers' own rules | *Sponsor an apprenticeship*: the wolf pays a fee (*placeholder: 20p*) to a master who would take one under doc 26's rules and trusts the wolf (20+) | The apprenticeship begins now, not at the weekly chance | Notable |

**Not built, because the state isn't there** (doc 48 needs changing here):
- **Broken tools:** residents' tools don't wear, and a missing tool changes nothing about their work. A trouble the
  player "solves" without changing anything fails the four tests. It waits until the economy session gives tools an
  effect.
- **Sick or injured kin:** residents have no injuries or sickness. It waits for a resident ailment (Open questions).

**When a resident speaks of it** (agreed: "once they trust the player a little"):
- **Worked out lazily.** A resident's trouble is worked out when a player talks to them or looks at them, and kept for
  a game day (`Game::troubles_`: resident → {kind, details, day}). Nothing scans the population.
- **The Mind's briefing:** a new optional field `trouble` (300 characters) on `mind::Context`, filled only when the
  resident's trust in the speaker is 10+ and familiarity 20+ *(placeholders)*. For example: "Your trouble (true): your
  shop borrowed 40 pennies to stay open and takings are thin. Speak of it if the talk turns that way or they ask how
  you are. Never ask outright for money." `tools/npc_mind.py` gets the limit and a rule line.
- **Knowing it was said:** the Mind's structured reply gains `mentions_trouble` (true or false), checked by the server
  like the other fields. When true, the player is marked as having heard it.
- **The game's own answer:** a router intent, `trouble` ("what's wrong", "is something troubling you", "can I help
  you"), answered from `troubles.json`'s lines when the trust rule holds, and "Oh, nothing worth your time." when it
  doesn't. No model.
- **Once heard,** it shows in doc 56's unfinished business ("The baker's shop is in debt"), and doc 58 may offer it as
  a personal story's start.

**Solving it:**
- The resident's menu (Look, the talk target) offers what fits: *Pay into their till*, *Give…* (doc 55), *Make peace
  with Wren*, *Speak for them to the cooper*, *Sponsor an apprenticeship with the smith*. The server checks every rule.
  The Mind never grants anything; its words may only lead the player there.
- **Each success:**
  - changes the simulation as the table says;
  - moves bonds through `Bonds::change`: the resident +8 affinity, +8 trust, +5 respect; their household +3 affinity
    and +3 trust *(placeholders)*;
  - logs a `trouble solved` event (actor the wolf, target the resident, item the kind, coins);
  - records a deed through `Game::recordDeed` (doc 56), which makes the rumour ("the grey wolf paid off the baker's
    debt").
- **Seen:** the player is told what changed in plain words ("The baker's loan is paid. The shop stays open."), and the
  resident's next briefing knows it.

**Limits** *(placeholders)*: one trouble solved per resident per season by the same wolf; a solved trouble of a kind
doesn't come back for that resident for 14 game days; money given to a resident with no trouble is a gift, and makes no
deed (doc 55 handles thanks).

### 4. Town projects (doc 48 §1.4, the main new system)

A **town project** is a public work the town needs. It is built from materials, labour and coin, it stands as a
structure, and it changes the simulation while it stands.

**The catalogue** (`Data/Town/projects.json` **(new)**; every number a placeholder):

| Kind | Needs | What it does while standing | Wears |
|---|---|---|---|
| **Mend the town** ("re-roof the chapel", "mend the walls") | Stone, timber, limestone, cord, by the town's size; 20 work-hours | Its materials go to the Town Works' stock (`town:<t>:works`), which mends the town by its own rule as it uses them. Its hired hands do "repairs" odd jobs (+1.5 each, existing). Players' hours count as repairs through one accessor, `Society::mendTown` **⚑ economy**. Named for the building it stands for. | (a mending, not a structure) |
| **Watch post** (on a road out of town) | 10 timber, 4 stone; 20 work-hours | Caravans in its cell and the next count +2 guards against a raid; no bandit camp gathers within 2 cells | 1 a game day |
| **Market cover** (a city square) | 12 timber, 6 cord, 4 linen cloth; 20 work-hours | Stalls set up under it in snow and storms, which keep a square's stalls in today (`World::dayPlan`'s `foul`); it shelters those beneath it | 2 a game day |
| **Waystation** (a road between towns) | 8 timber, 4 stone; 30 work-hours | A shelter for travellers (the layer's shelter); caravans on that road wait out storms under it and go a tenth faster | 1 a game day |
| **Granary** | 20 timber, 10 stone; 40 work-hours | The town's granary keeps more in store for the winter (`granaryDays` +3 for the town) **⚑ economy** | 0.5 a game day |
| **Bridge** | — | A shorter road. **Later:** nothing today makes water walkable without editing terrain. It needs a new "deck" structure the world's walking reads (Risks). | — |

**Not in the catalogue:** a **well**, since there are no water chores to save; a **wall** against raids, since bandits
never raid towns. Doc 48's examples change here: the watch post stands in for the wall, against the bandits that do
exist.

**Where projects come from:**
- **Real needs** (agreed). Once a game day at dawn, the game looks at one town in turn (round robin, so a single town
  a call) and proposes a project when a need holds and the town has no open project of that kind. It only reads:

  | Need | Read from | Project |
  |---|---|---|
  | The town's condition under 50 | `Society::condition` | Mend the town |
  | A caravan robbed on one of the town's roads in the last 14 game days | the roads' `raid` events, `RoadsState::camps` | Watch post on the worst cell |
  | A city whose stalls stayed in for foul weather on 2+ market days this season | a small counter at `World::dayPlan`'s foul check (new) | Market cover |
  | A road where caravans waited out storms on 3+ days this season | the caravans' waits (new counter) | Waystation |
  | The town's distress "empty shelves" in two decisions running, or its granary empty at the turn of autumn | `Society::orchestrator()`'s `TownReading`, `TownSnap::granary` | Granary **⚑ economy** (only once its effect exists) |

- **A DM** can post any project anywhere suitable (`project.post`, §7).
- **At most** one open project a town, two in a city *(placeholders)*.

**The ledger** (`Core/RatwProjects.{h,cpp}` **(new)**, pure rules like `RatwCamps`):

```text
Project {
  id "proj-<n>", kind, town, cell, x, y
  title          from the template ("the Fenhollow granary"), or named for its chief giver (below)
  state          proposed, open, built, worn, ruin, cancelled
  needs          {materials: item → n, hours}
  have           {materials: item → n, hours}
  purse          "project:<id>"        a real account
  structure      the structure it became (on the layer), once built
  posted, by     calendar day; "town" or a DM's action
  plaque[]       up to 3 {who, name shown}
  chief          who gave the most, and whether it was named for them
}
Gift { project, who, kind (coin | goods | labour), item, amount, value, day, nameShown }
```

Kept in `game.projects` and `game.project_gifts` **(new tables)**, written by checkpoints as `game.sections` lists.
Each gift is a `game.events` row (`project gift`), and so is a finish (`project built`). IDs and amounts only, no
prose (agreed).

**Giving:**
- **Coin** (agreed: conserved). *Give to the project* at the board (doc 54) or the site moves coin from the wolf to
  `project:<id>` (`Society::shift`, saved through the journal, `Game::record`). The coin does work:
  - it posts procure contracts for materials still missing (`World::postContract`, poster `project:<id>`, as the Town
    Works' buyers do), filled by residents and players alike;
  - it hires hands for the hours still missing (below).

  So coin is a third way to bring the other two, never a bar of its own.
- **Goods:** *Hand in* at the site, as a procure contract's goods are handed in, any quality.
- **Labour:**
  - **Players** *Work on it* within 2 tiles, counted as camps are (`camp::SecondsPerWorkHour`), at doc 53's
    `together::rate` (`Core/RatwTogether.h`), summed over the work as doc 53's `World::workLabour`: alone ×1, two in
    different roles ×1.8, a resident hand ×1.4 (agreed in doc 48 §6.3). Each kind
    names its two roles: timber framing is holder and striker; a cover, raiser and lasher.
  - **Residents** are hired as hands with the project's coin, as odd jobs of a new kind, `project`, paid from its purse
    at the town's labourer's pay (`Society::dayWage`). The jobless take them first, as with any odd job **⚑ economy**
    (one entry point to post an odd job from outside the society).
- **Value of a gift** (for the plaque): coin at face; goods at the town's price (`Society::townPrice`); a work-hour at
  an eighth of the town's labourer's day *(placeholder)*.

**Finishing:**
- When materials and hours are met, the materials are used up (`Society::consume`, "used in the granary": goods only,
  not money). The structure stands on the layer (below).
- **Coin left in the purse stays there** as the project's upkeep. It buys materials and hires hands to mend the
  structure when its condition falls under 60 *(placeholder)*.
- **If the project is cancelled** (by the DM, or unfinished after 60 game days *(placeholder)*): its goods go back to
  their givers in the shares they gave, and its coin to the coin givers pro rata, all as moves.
- **Never minted, never lost** (doc 15): every penny ends in a resident's wages, a shop's till, back with its giver, or
  in the upkeep purse.

**The structure** (agreed: never a terrain edit):
- Town projects use the same layer as Chapters' camps, so walking, shelter, saving, maps, Atlas and the DM all see them
  with no new path. A project's site is a `camp::Site` owned by `town:<community>` instead of a Chapter; its kinds join
  `camp::catalogue()` (granary, watch post, market cover, waystation); the camps' "not in a town" rule applies to
  Chapters only.
- **Where:** proposed by the game on open ground next to the town's square or road, never on a road's tiles, a
  doorway, water or anyone's home, by the camps' own checks (`Game::whyNotGround`). A DM can choose the tile.
- **Built** while open, it shows dim and framed as a camp's does. When built, it is drawn in a town colour (a stone
  grey, not a Chapter's), and Look shows its plaque.

**Plaques and naming** (agreed):
- **The plaque** lists up to 3 givers by value. Each is shown by the name *they chose* at their first gift: any of their
  own registered names, or "a friend of the town" (the default). Reading a plaque teaches a name, not a face, so it
  introduces nobody (doc 32 §1.5).
- **Naming:** if the chief giver gave 40%+ of the value and the whole is worth 300p+ *(placeholders)*, the project is
  named for them with the name they chose ("Kestrel's Waystation").
- **Remembered fondly** (agreed), through doc 56: the finish is a deed for its givers (the chief giver's notable, or
  great for a project worth 1,000p+; others who gave a tenth or more, small *(placeholders)*). The town's word carries
  it, and residents warm to the giver when they first connect it (doc 56, open question 2).

**Wear** (agreed: like doc 32 §5.6's buildings):
- Built project structures wear by `Camps::wear` at their kind's rate. A town's site is never "abandoned" (no ×4).
- Under 50 its effect is halved; at 0 it is a **ruin**, standing as scenery, doing nothing, rebuildable as a new
  project.
- Anyone can *Work on it* to mend it, and the upkeep purse buys materials and hires hands when it has coin. A worn
  project the town still needs is proposed again as a mending ("mend the watch post"), which keeps a town in good repair
  as a reason to come back.

**On the board** (doc 54 shows it): `Game::projectsView(town)` gives each open project's title, needs and progress
(materials, hours, coin in the purse and what it will buy), the top givers as the viewer knows them, and the actions
*Give*, *Hand in* and *Go to the site*. Until doc 54 lands, the site panel (as the camp panel,
`Client/src/ui/hud/camp.ts`) and Look on the site show the same.

### 5. The economy as a lever (doc 48 §1.5)

**Trade between towns moves prices, and the town says so.**
- When a player sells or hands in goods a town is short of (under two days' stock: `World::tendPrices`' store
  reading for meals and herbs, `GoodSnap` for the rest), the world keeps a **trade note**: `{town, item, who,
  quantity, day}`, at most 32 a town *(placeholder)*.
- When that good's price in that town falls by a tenth or more within a week (hourly for meals and herbs,
  `priceSeen_`; at the next weekly decision for the rest, read with `Society::townPrice`), the note becomes town news,
  `prices` with a cause.
- It is voiced from new written scenes, `Data/Voice/scenes/prices_why.scene` ("Bread's cheaper since that grey wolf
  brought a cartload up from Ser Ferro"). The wolf is named only as the town knows them: the doc 56 public name, or
  their look.
- If the town was in "empty shelves" distress and the wolf brought a day's food for a tenth of its people, it is also a
  deed (*fed the town*, notable).
- This only reads prices. The orchestrator still sets them.

**Guarding caravans carries news further.**
- A caravan that arrives with a player escort present (`Caravan::escorts`, counted at arrival) carries 16 rumours, not
  8, at ×0.75, not ×0.6 *(placeholders)* (`World::caravanArrived`, `RatwRoads.cpp:768`).
- It also carries doc 56's notable deeds as well as great ones.
- The escort's own deed (*saw the caravan through*) is doc 56's small deed.

**Player help counts as a real flow** (doc 48: "This touches the orchestrator's design, which another thread owns").
What the orchestrator would need, as an interface for the economy session to build or refuse **⚑ economy**:
1. **Count it.** In `Society::noteForOrchestra`, a flow where one side is a player's account counts toward the other
   side's town as `playerIn` (coins players spend there: purchases, project gifts) or `playerOut` (coins shops pay
   players). Goods players sell or hand in count as `playerGoods` at the town's price. These are separate from
   `townFlow_`, so the economy session chooses whether they feed the *draining* sensor.
2. **Show it.** `TownSnap` gains `playerIn`, `playerOut` and `playerGoods`. `orchestrator_towns.csv` and the DM's
   Orchestrator panel show them.
3. **Leave projects alone.** `project:` accounts match none of the holder prefixes in `orchestraSnapshot`, so they are
   never banded or squeezed. That must stay so (a test), since their coin is spoken for.
4. **Optional, the economy session's call:** the *works* channel's materials go into an open project of its town rather
   than being used up, so orchestrated works build what players can see.
   - This brushes doc 46's decision 4 ("no writs or news of its orders"). The board would show "the Town Works gave 40
     stone".
   - *Recommendation:* show it as the Town Works' gift, as it is a town buyer, without naming the orchestrator.
     Open question 1.

Food players bring into a hungry town already shows in its shops' stock, which the orchestrator reads, so "players
feeding a hungry town means the orchestrator sees less need" (doc 48) holds today for goods. What is missing is coin
and the record of who helped. The orchestrator's own response stays invisible (its decision 4); only players' acts are
spoken of.

### 6. Bounds (doc 48 §1.6)

**Protected residents** (doc 48: "can't be killed or ruined by players").
- **Who:** residents marked protected in Atlas (a new `live.npcs.protected` field, exported as `protected "id"` records
  the way `joinable` is: migration `0029_joinable.sql`, read by `RatwAuthoring.cpp`); heads of great houses
  (`Society::houseHead`); faction members with a rank (`live.faction_members`); and any resident a DM marks
  (`npc.protect`, `npc.unprotect`). The set is kept by the game (`Game::protected_`), not in the society's records, so
  the economy's files don't change.
- **What it means:** players can't attack them ("Their guards close in; you think better of it."); a theft from them
  takes a meal or herbs but never coin; only the DM kills, as now.
- This answers doc 26's open question: authors in Atlas, the DM live, and the two derived rules.

**Wear and regrowth** (agreed): projects wear (§4); troubles come back as life goes on (a household falls short again,
a business into debt); hunting and foraging already regrow (doc 41). Nothing a player does is permanent but memory:
deeds, the chronicle, the plaque on a ruin.

**The DM can see and reverse any player-made change** (agreed). Each is an action with its inverse, for doc 34 Part 8's
*Reverse*:

| Change | See it | Reverse it |
|---|---|---|
| A project (open or built) | LIVE **Projects** layer: sites, progress, givers, condition; the Life panel's gifts | `project.cancel` (gifts returned pro rata), `project.remove` (the structure goes and its effect ends), `project.complete` (finish it by hand) |
| A trouble solved | The event, in the resident's Life panel | The ordinary inverse action: `npc.job` (doc 34), a bond edit (`bond.set`), an apprenticeship ended. The deed through doc 56's `deed.revoke`. |
| A protected flag | The NPC inspector | `npc.unprotect` |

Every action is audited, with its before and after (doc 34 decision 7).

**Nothing edits terrain** (agreed): projects are structures. The bridge waits for a "deck" structure (Risks).

**Against farming:** troubles are limited per resident and per wolf; a gift counts toward the plaque only while it
stays in the project (refunds come off); names on plaques are registered names only (the name filter).

### 7. The Dungeon Master

- **LIVE:** a **Projects** layer (`Editor/src/dm/LiveTab.tsx`, `GET /api/live/projects`, read from `game.projects` and
  `game.camp_structures`): open and built projects, progress, condition, top givers.
- **Actions** (`dm.actions`, audited): `project.post` (kind, town, tile, optional title); `project.cancel`,
  `project.complete`, `project.remove`; `npc.protect`, `npc.unprotect`.
- **Settings** are data: `Data/Town/troubles.json` and `Data/Town/projects.json`. The trouble and project kinds can be
  switched off one by one.

## Phases

### Phase 1: troubles, seen

**Goal:** residents know their troubles and speak of them to wolves they trust; the game knows when a player has heard
one. Nothing is solved yet.

**Changes:**
- `Core/RatwTroubles.{h,cpp}` (new), pure: the kinds and their order from `Data/Town/troubles.json` (new), and
  `troubleOf(resident, reads)`, using only public reads (`Society::account`, `household`, `jobOf`, `state().careers`,
  `state().memory.loans`, `orchestrator()`, `World::bonds`).
- `Core/RatwGame.cpp`: `troubles_` (kept a game day); the `trouble` field filled in `Game::dialogueContext` by the trust
  rule; `heed` reads `mentions_trouble`. `Core/RatwMind.h/.cpp` and `tools/npc_mind.py`: the field, its limit, the
  reply flag and the rule.
- `Data/Voice/router.json` and `Game::gameAnswer`: the `trouble` intent and lines. Doc 56's `self.unfinished` gains
  troubles heard.

**Tests:**
- `Tests/troubles_tests.cpp` (new), on a made-up society and through the game (the `social_game_tests` harness): each
  of the five kinds found from its state, none from an untroubled resident; the order when two hold; the day's cache;
  briefed only past the trust rule; `mentions_trouble` marks it heard; the router's answer and its refusal.
- `voice_tests` (the corpus routes "what's wrong" to `trouble`), `tools/test_npc_mind.py` (the field and the flag).

**Done when:** a player whom the baker trusts asks "is something troubling you?" and hears about the shop's debt, from
the game's own lines or from the Mind with the flag set, and it appears in their unfinished business.

**Cost:** worked out per conversation from a handful of lookups, cached a game day. No scan; nothing per tick.

### Phase 2: troubles, solved

**Goal:** a player can solve each trouble; the simulation, the bonds and the town's talk change.

**Changes:**
- `Core/RatwGameTroubles.cpp` (new): the menu entries and `trouble` verbs (pay into the till, make peace, speak for,
  sponsor); the checks, bond changes (`Bonds::change`), the `trouble solved` event and `Game::recordDeed` (doc 56); the
  plain words of what changed. Gifts for "short of coin" come through doc 55's give.
- Two small entry points in the society's careers **⚑ economy**: `Society::appoint(position, resident)` (a vacant post
  given now, as succession would) and `Society::apprenticeTo(master, youth)` (an apprenticeship begun now, under doc
  26's rules).
- The shop's debt needs no economy change: the coin goes into the till, and the business repays the loan by its own
  rule at the next daily pass. The trouble shows solved when `loans` no longer holds it.

**Tests:**
- `troubles_tests`, through the game: the baker's loan cleared by a player's coin at the next pass, money conserved;
  two feuding residents brought together, both affinities up and no quarrel between them after; an idle resident
  spoken for gets the cooper's vacant post; a youth sponsored is apprenticed to the smith, and the fee is in the
  smith's purse; each makes a deed and a rumour; the limits; a restart.
- `careers_tests` (the two entry points), `town_tests`' conservation run.

**Done when:** each of the four solutions works in play, the resident's life visibly changes, and residents who weren't
there hear of it within the deed's reach.

**Cost:** at a command, never in the tick.

### Phase 3: town projects

**Goal:** towns post projects; wolves give coin, goods and labour; residents fill contracts and take the hands' jobs;
the structure rises, with a plaque and perhaps a name.

**Changes:**
- `Core/RatwProjects.{h,cpp}` (new): `Project`, `Gift`, needs and progress, value, the plaque and naming, cancel and
  refund, save and load. `Data/Town/projects.json` (new).
- `Core/RatwCamps.{h,cpp}`: town-owned sites, the town kinds, the town colour. `Core/RatwGameCamps.cpp`: the "not in a
  town" rule for Chapters only.
- `Core/RatwGameProjects.cpp` (new): `project` verbs (give, hand in, work, the name shown); work at `together::rate`
  (doc 53); procure contracts from the purse; hands hired as `project` odd jobs **⚑ economy** (one entry point,
  `Society::postOddJob(payer, town, kind, slots, pay, site)`); finishing; `projectsView(town)` for doc 54; the site
  panel's view.
- A migration (next free number): `game.projects` and `game.project_gifts` as `game.sections` lists.
- `Client/src/ui/hud/camp.ts` (or `project.ts`, new): the site panel. DM: the `project.*` actions and the LIVE
  Projects layer.

**Tests:**
- `Tests/project_tests.cpp` (new), pure rules: progress, value, the plaque's order and names, naming at 40% and 300p,
  refunds pro rata. Through the game: a DM-posted market cover; two players at ×1.8 in two roles against one at ×1
  (`together::rate`); coin buying materials through a procure contract a resident fills; a hired resident hand paid from
  the purse; the structure standing with its plaque; "Kestrel's Waystation"; a restart. `conserved()` at every step.
- `Client/src/game/projects.test.ts` (new); `tools/client/projects.mjs` (new), in a real browser: give, hand in, work
  beside another wolf, see it rise; `tools/test_dungeon_master.py` (the actions).

**Done when:** in a test world, a project posted by the DM is finished by two players and a hired resident, stands on
the map in the town's colour with a plaque naming its givers as they chose, and every penny is accounted for.

**Cost:** work at a site is counted like camp work (a few workers, only while working); procure contracts and odd jobs
go through the existing daily machinery; snapshots carry a site's structures only in its cell, as camps do.

### Phase 4: projects that change the world

**Goal:** finished projects do what their kind says, wear, are mended, and are proposed from the town's real needs.

**Changes:**
- **Effects:** watch post, in `World::caravanEntered`'s raid odds (+2 guards near it) and `roadsDaily`'s camp gathering
  (`RatwRoads.cpp`); market cover, in `World::dayPlan`'s foul-weather check (`RatwSchedules.cpp`); waystation, as
  shelter and in caravans' pace and storm waits on its road (`RatwRoads.cpp`); mend the town, as materials into
  `town:<t>:works`'s stock and hands' labour as "repairs" odd jobs (existing rules), and players' hours through
  `Society::mendTown` **⚑ economy**. Halved under 50, gone at 0. The
  game hands the world the standing projects' effects when they change, as it hands it obstacles
  (`Game::refreshGround`).
- **Wear and mending:** `Camps::wear` at town rates; the upkeep purse mending under 60; "mend the …" proposed when worn.
- **The proposer:** `Game::proposeProjects`, one town a day at dawn, with the needs table's reads and two new counters
  (market days lost to foul weather, caravans waiting out storms).
- **The granary** only once the economy session gives the orchestrator a per-town granary allowance **⚑ economy**.

**Tests:**
- `project_tests`: a watch post cuts a strong camp's raid odds and no camp gathers within 2 cells; a market cover keeps
  stalls out in a storm; a waystation shelters; mending raises the town's condition through the Town
  Works' own rule; wear halves then ends an effect, and mending brings it back; the proposer posts a watch post after a
  robbery and a mending under 50, one town a day, never two of a kind.
- `roads_tests` and `schedules_tests` unchanged with no projects. Gate: `world_check --simulate 7 7.2 --players 20`
  within noise; `econ_watch` 14 days with and without projects, money conserved.

**Done when:** a watch post on a robbed road makes the next raids rarer in a run; the chapel's mending lifts the town's
condition over 50 (and its "mended" news); a neglected cover stops working until mended.

**Cost:** the proposer reads one town a day. Effects are flags looked up where the rule already runs (a raid, a day's
plan, a caravan's step). Nothing scans.

### Phase 5: the economy made visible

**Goal:** players see their trade move prices and hear why; guarded roads carry news further; the orchestrator's books
count what players bring.

**Changes:**
- Trade notes (`World::noteTrade`, at a player's sale, hand-in or supply delivery: `RatwRoads.cpp`, `RatwProcure.cpp`);
  the `prices` news with a cause (`RatwAmbient.cpp`) and `Data/Voice/scenes/prices_why.scene` (new); *fed the town* as
  a doc 56 deed.
- Guarded caravans: `World::caravanArrived`'s rumour count and factor with a player escort present, and doc 56's
  notable deeds across.
- **⚑ economy**, built by or with the economy session: `noteForOrchestra`'s player flows; `TownSnap::playerIn`,
  `playerOut`, `playerGoods`; the CSV and panel columns; a test that `project:` accounts are never holders; optionally,
  the works channel's materials going to an open project.

**Tests:**
- `roads_tests`: a player's grain sold into a town short of it, its price falls, and the town's talk names the cause,
  by look for a stranger; an escorted caravan carries more rumours and a notable deed, an unescorted one doesn't.
- `ambient_tests` (the cause in the scene, never a true name); `orchestrator_tests` with the economy session (the
  player counters; `project:` never banded).

**Done when:** a player who carries grain into a hungry town hears within a day that bread is cheaper because of the
wolf who brought it, and the DM's Orchestrator panel shows the town's player inflow.

**Cost:** a note at a sale (bounded per town); the price check reuses `priceSeen_` and the weekly prices; the counters
cost what `noteForOrchestra` costs now.

### Phase 6: bounds and the Dungeon Master's hand

**Goal:** protected residents can't be ruined by players; the DM sees and can reverse every player-made change.

**Changes:**
- **Protected residents:** `live.npcs.protected` (migration); Atlas's field (`Editor`, `tools/map_editor.py` checks);
  the export record `protected "id"`, read in `RatwAuthoring.cpp`; `Game::protected_` with the derived rules; the
  checks in `World::attack` (`RatwBattle.cpp:296`) and `World::steal` (`RatwCrime.cpp:281`) through a callback the game
  sets; `npc.protect` and `npc.unprotect`.
- **The DM:** inverses for doc 34's *Reverse* (`project.*`, the troubles' ordinary inverses); the Projects layer's
  inspector with *Cancel*, *Remove* and *Complete*; the Life panel's troubles solved and gifts.
- **Anti-farming checks** (§6).

**Tests:**
- `crime_tests`: a protected resident can't be attacked, and a theft from them takes no coin; an ordinary resident is
  as before. `town_tests`: the Atlas record read, unknown IDs refused.
- `tools/test_dungeon_master.py` and `tools/test_dm_live.py`: a project removed live and its effect gone; a protected
  flag set live.

**Done when:** a ruler can't be beaten down by a player, and a DM can take any project out of the world from the LIVE
map, with an audit line that says what it was.

**Cost:** a set lookup at an attack or theft.

## Changes in the economy session's files

Every hunk this plan would make in code another session owns (doc 46's orchestrator and the society's economy code).
Each is small, and each is for that session to build or agree before the phase that needs it.

| Phase | File | Change | Why | Without it |
|---|---|---|---|---|
| 2 | `Core/RatwCareers.cpp`, `Core/RatwSociety.h` (declarations) | `Society::appoint(position, resident)`; `Society::apprenticeTo(master, youth)` | "Speak for them" and "sponsor an apprenticeship" change real careers | Those two troubles can be seen but not solved |
| 3 | `Core/RatwOddJobs.cpp`, `Core/RatwSociety.h` | `Society::postOddJob(payer, town, kind, slots, pay, site)`, and the kind `project` (hands paid from the poster's purse, at the site) | Residents' labour on projects, paid from the project's coin | Coin buys only materials; labour comes from players only |
| 4 | `Core/RatwDemand.cpp`, `Core/RatwSociety.h` | `Society::mendTown(community, points)`, the same mending a "repairs" odd job does | Players' own work on a "mend the town" project | Only its materials and hired hands mend the town |
| 4 | `Core/RatwOrchestrator.h/.cpp`, `Core/RatwOrchestrate.cpp`, `Data/Economy/orchestrator.json` | A per-town granary allowance (days more), read by `plan` | The granary project's effect | No granary in the catalogue |
| 5 | `Core/RatwOrchestrate.cpp` (`noteForOrchestra`, `orchestraSnapshot`), `Core/RatwOrchestrator.h` (`TownSnap`), `econ_watch`, `Editor/src/dm/OrchestratorPanel.tsx` | Player flows counted by town; `playerIn`, `playerOut`, `playerGoods`; shown | "Player help counts as a real flow" (doc 48 §1.5) | Goods players bring still show in stock; coin and who helped don't |
| 5 | `Core/RatwOrchestrate.cpp` | A test that `project:` accounts are never holders | Projects' coin is spoken for | Today's prefixes already leave them out; the test only keeps it so |
| 5 (optional) | `Core/RatwChannels.cpp` | The works channel's materials go to an open project of its town | Orchestrated works build something visible | Works materials are used up, as now |
| Later | `Core/RatwDemand.cpp` (`tradeUpkeep`) | A missing tool slows a producer's work, and is recorded | The "broken tools" trouble | No tool trouble |
| Later | `Core/RatwResidents.cpp` | A resident ailment that keeps them home | The "sick or injured kin" trouble | No such trouble |

Reading only, no change: `Society::condition`, `orchestrator()`, `townPrice`, `dayWage`, `state().memory.loans`,
`jobOf`, `household`, `account`. Nothing in this plan writes to the society's memory directly.

## Depends on and feeds

**Depends on:**

| Doc | For | Until it lands |
|---|---|---|
| 56 | `Game::recordDeed`, the town's word, public names, unfinished business, the welcome card | Phases 2–5 need its Phase 1; the talk needs its Phase 2 |
| 53 | The cooperation rate and roles, for project labour (Phase 3) | Workers add up plainly |
| 55 | The gift action, for "short of coin" and peace by a gift (Phase 2) | Coin can still be paid |
| 54 | The board, to show projects | The site panel |
| 46 | The hunks above, from the economy session | See each row's "Without it" |
| 34 | *Reverse* in Audit | The inverse actions, sent by hand |

**Feeds:** doc 56 (deeds from troubles and projects; rows for the chronicle and the welcome card); doc 55 (thank-you
letters to wolves who solved a trouble or gave to a project); doc 58 (troubles as starts for personal storylines,
projects as goals for storytellers' calls); doc 54 (projects on the board's work side; festivals may open a finished
project); the DM (the Projects layer and the protected flag).

## Risks

- **Doc 48's troubles assumed state that isn't there** (debts, tools, sickness). This plan builds the troubles that
  are real and defers the rest. The user may want the missing state built first. Each is economy code (Open question
  2).
- **The bridge.** Doc 48's favourite example can't be built without making water walkable, and nothing may edit
  terrain. A "deck" structure that the world's walking treats as ground would do it within the rules, but it touches
  walking, routes, the client's walker and Atlas. It is its own decision (Open question 3).
- **Projects and the orchestrator pulling against each other.** A project hiring the idle and buying materials is the
  same kind of spending as the works channel. Mitigations: the purse is never a holder; money moves only by the
  existing rules; `econ_watch` runs with and without projects in Phase 4.
- **Resident hands double-paid.** A resident "labouring for the Town Works" who takes a project's odd job must not be
  paid by both. The odd-job machinery already handles the jobless first; the economy session confirms it in Phase 3.
- **Visible orders.** Showing the Town Works' gift on a board comes close to doc 46's decision 4 (Open question 1).
- **Farming troubles and plaques.** Covered by the limits in §3 and §6. Watch the first weeks' events.
- **Proposals nobody takes up** clutter the board. Mitigations: one project a town, a 60-day lapse with refunds, and
  only real needs.

## Decisions

### Agreed (doc 48)

1. Choices that matter pass four tests: seen, lasting, spoken of, costly (§1.1). Players improving residents' lives,
   farms and towns are remembered fondly (decision 9).
2. Residents' troubles come from real state, are mentioned once the resident trusts the player a little (a Mind
   briefing), and when solved change the simulation and the bond, and become a rumour (§1.3).
3. Town projects come from real needs or a DM; need materials, labour (cooperative work) and coin (conserved); show on
   the board; change the simulation as structures, never terrain; record givers as IDs and amounts; carry plaques and
   names; and wear like Chapters' buildings (§1.4).
4. Trade moves prices and the town news says so; guarded caravans carry rumours further; player help is a real flow
   in the orchestrator's books (§1.5, *proposed* there; the orchestrator part is the economy session's).
5. Bounds: protected residents, wear and regrowth, DM reversal, no terrain edits (§1.6).

### New in this plan (placeholders for the user to confirm)

6. **Troubles built:** short of coin, a shop in debt (the rescue loan), a feud, no steady work, a child with no trade.
   Broken tools and sick kin wait for state that doesn't exist yet.
7. **"Debt" means a business's rescue loan**, paid off by coin into its till, since residents have no personal debts.
8. **The trust rule:** a trouble is spoken of at trust 10+ and familiarity 20+.
9. **Projects built first:** mend the town, watch post, market cover, waystation; the granary when the orchestrator can
   hold more for it; the bridge later; no well and no wall, since there are no water chores or town raids.
10. **Coin is a third way to bring materials and labour,** never a bar of its own; what's left stays as the project's
    upkeep.
11. **Town projects share the Chapters' structure layer,** with town-owned sites.
12. **Plaques show the name each giver chooses** (any registered name, or "a friend of the town"); naming at 40% of a
    project worth 300p+.
13. **Protected residents:** an Atlas flag, house heads, ranked faction members and DM marks; they can't be attacked or
    robbed of coin by players.
14. **One open project a town** (two in a city); unfinished projects lapse after 60 game days with refunds.

## Open questions

1. **Should orchestrated works build open projects,** with the board showing "the Town Works gave 40 stone"? It makes
   the economy visible, but brushes doc 46's decision 4. *Recommendation:* yes, credited to the Town Works, never to the
   orchestrator.
2. **Should residents get the missing state** (tools that matter to work, ailments that keep them home), so the last two
   troubles can exist? Both are economy-side work. *Recommendation:* ailments first: they suit roleplay best, and doc
   55's healers and gifts would have something to do.
3. **The bridge:** build a "deck" structure that makes water walkable without editing terrain, or leave bridges to
   authors and the DM's world edits? *Recommendation:* a deck structure, as its own small plan, after Phase 4.
4. **The later levers** (testify or post bail, escort a resident to a new town, introduce two residents who may marry):
   which first? *Recommendation:* introducing two residents. It fits matchmaking (doc 52) and needs only the marriage
   rules to count a player's introduction.
