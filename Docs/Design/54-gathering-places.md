# 54. Gathering places

Drafted 2026-10-06 as an actionable plan for doc 48 (Part 5 except §5.3's training grounds, and Part 10). Open
questions answered 2026-10-08 (see "Open questions"); all seven phases built the same day (see "Built"; not yet committed). Read doc 48 (Part 5, Part 10 and the Decisions) and docs 38 (rest), 44 (rested time), 42 (town purses, wages),
26 (festivals, contracts, residents), 32 (Part 5 and Phase 7: renting), 39 (shops and stalls), 36 (homes), 35 (items),
30 (towns and talk), 14 (the calendar) and 31 (cost) first. Doc 55 comes before this one: its document store holds
the notices.

Marks: **(agreed)** is the user's decision, with a pointer to doc 48; *(placeholder)* is one constant to change.

## The ask

> "I think we need to take a look at ways we can make centers of activity and social meeting in the game. My thought
> on this centers around markets (buy/sell/trade), taverns (rest and healing), training grounds (so players can
> practice fighting), quests (storyline and DM events), and 'activities' [...] healing at the cantina being a good
> example." (doc 48 Part 5)

> "It should be where players can go to find work and storylines, but there should be a 'public forum' one, too,
> where players can post 'letters' or 'signs' on the board." (§5.4, agreed)

> "Players should be able to rent out anywhere that isn't a leadership center (kings, great house bedrooms, etc etc)
> but I don't know how this could impact stories." (§5.7, agreed in spirit)

> "Players want different things: we should probably try to figure out exploration or other sorts of games within
> this game. Minigames may help, such as a library sorting minigame that allows players to explore lore while they
> work?" (Part 10; tavern games agreed, decision 32)

The rule doc 48 takes from Star Wars Galaxies' cantinas: **a place becomes a centre when something you need can only
be had there, and it gets better with company.**

## Where we stand (read from the code 2026-10-06)

**Inns and rest**
- **The rules don't know an inn.** Healing reads only bed tiles (`World::inBed`, `b` and `z`, `RatwBattle.cpp:3284`),
  with its rates as literals in `World::restPlayers` (3243): bed 1.5, still elsewhere 0.75, about 0.25.
- An inn is found three ways today: its keeper's work label matching the `inn` business (`items::businessFor`,
  match words such as "keeps the ", "taproom"), its cell name (`placeKind` in `RatwAmbient.cpp`, for ambient talk), and
  a cell named "<inn>, upstairs" (`Game::refreshEstates`).
- **What an inn is on the ground** (`tools/worldgen/buildings.py` `tavern`, `towns.py`): a common room of tables (`T`),
  chairs and a counter, and for an inn an upstairs of four small rooms with two beds each. About six in ten of a town's
  residents spend their evenings in its common room (`populate`'s `evening_room`), so the room already fills at night.
- **Rested time** (`SocialLedger::restedLeft`, `RatwSocialCore.cpp:740`) comes from real-time gaps between earnings.
  Where a wolf was plays no part; a logout records only `awaySince` and `awayInBed`.
- **No performers.** Instruments (mouth pipe, paw drum, hurdy-gurdy, handbell) are in the catalog and residents buy
  them as a pastime.

**Boards and work**
- **Only Chapter-rented places have a board:** `Lease::notices` (20 notices of 200 letters; `notice` and `unnotice` in
  `Game::estateCommand`, `RatwGameEstates.cpp:224`), shown in the place panel (`Client/src/ui/hud/place.ts`), saved in
  the checkpoint row. No scent, no expiry, no fee.
- **Contracts reach players only through a merchant's actions** ("ask for work", "take k<N>", `RatwGame.cpp:2741`;
  `contractsNear`, `RatwRoads.cpp:238`). There is no contracts message. Contracts exist only in a world of two or more
  towns, and escorts and bounties are listed in the town they go to.
- **Faction missions come from officials**, not boards (`missionBoard`, `RatwGameFactions.cpp:198`; the
  `{"type":"missions"}` message).
- **No quests** (doc 34 Part 3 isn't built), **no town projects** (doc 57), **no storytellers** (doc 58).
- **Towns** have no settlement records: `World::towns()` comes from cell regions, and `World::square(community)`
  (`RatwSchedules.cpp:57`) works out each day the market point (`Town::market`), the stall spots beside built stalls
  (`u`) and up to 64 crowd spots.
- **Fees players pay go to the capital's `treasury`**, not their town's (the founding fee, `RatwGameChapters.cpp:107`;
  rent to "the town", `RatwGameEstates.cpp:69`), though doc 42 gave every town its own purse.

**Markets**
- The three cities have built stalls (Ridgemere's Market Square, Ser Ferro's Mercato, Upper Accord's plaza), kept
  every day from 6 to 4 by stallholders. Towns have none and no market day (`World::dayPlan`, doc 39).
- On Marketday from 7 to 2, **every merchant of the community** is sent to a stall spot chosen by hashing its id
  (`tradingAt`, `RatwResidents.cpp:292–298`), stallholders included, and two may land on one spot.
- **Players can't trade with each other** at all; only with a merchant (`World::trade`, `RatwWorld.cpp:4147`).

**Festivals**
- The 46th day of each season, or called by a DM (`World::callFestival`). From noon residents gather at the square
  and the town's store gives each a meal (`RatwResidents.cpp:1116–1118`). **Players get no meal**, and there are no
  contests, goods or programme. Festival names are hard-coded (`FestivalNames`).

**Renting**
- `estate::Estates` (`Core/RatwEstates.*`): places with a landlord, a faction, a weekly rent and a Chapter level;
  leases **by Chapters only**, rent weekly from the treasury, a week's grace, eviction; `Game::mayEnterPlace` locks the
  door.
- `Game::refreshEstates` (every 5 minutes) lets every inn's upstairs (30p a week, to the keeper), every warehouse (50p,
  to the town), Atlas's `let` records and a DM's `estate.set` places.
- **Rent above an inn goes to the keeper's own purse**, not the inn's till (doc 46 gave every business a till,
  `Society::tillOf`).
- **Warehouses have a storekeeper working in them**, so doc 48 §5.7's "never a place a resident works from" holds only
  if the town counts as letting a spare part.
- `estate.set` and `estate.clear` work in the game (`Game::applyDmActions`, `RatwGame.cpp:793`), but the DM app can't
  send them: they are missing from `ACTIONS` (`tools/dungeon_master.py`) and its screens.

**Games, the library, exploration:** none. The catalog has bone dice, bones, the `map` ("shows the cells it covers on
the world map", unread by the game), manuscripts, and doc 42's hall of records in big towns. Discovery XP counts each
new cell (doc 44).

**The wire:** owner state in `self` is resent five times a second; a board, a stall or a table is sent as an event when
opened, as faction missions are.

## Scope

This plan builds:
- **taverns:** rest in the common room, company and performers, rested time at inns;
- **notice boards** on every town square, with the work-and-stories side and the public side;
- **renting by individuals** and venues: inn beds, lodgers, whole places, nights; holds for stories; player-run nights;
- **market stalls** for players on Marketday;
- **tavern games:** Knucklebones, Wolves and Deer, Liar's Bones;
- **festivals that draw players:** contests, goods, the feast, rested time;
- **the library and archive**, and light exploration.

It leaves to other plans:
- the innkeeper's **introductions** and residents as matchmakers (doc 52); **training grounds and sparring** (doc 53);
- the **crier** who recounts deeds at festivals (doc 56; this plan leaves the slot);
- **town projects** (doc 57), **Storykeeper quests** (doc 34 Phase 5) and **storytellers' calls** (doc 58), which
  post on the board's work side through this plan's sources;
- **stars** (doc 51), which judge the storytelling contest, and **open scenes on the map** (doc 51);
- **circles** (doc 50), whose lists carry player-run nights; **block and report** (doc 50);
- **letters and the document store** (doc 55).

## Design

### 1. Taverns (agreed: rest and healing, doc 48 §5.1)

**The common room.** An inn's or tavern's ground-floor interior where its keeper works (a merchant post whose label
matches the `inn` business). `refreshEstates` lists them every 5 minutes (`commonRooms_`: cell to community). A venue
rented for a night (§4) counts as one while its night lasts.

**Rest there** (doc 48 §5.1's placeholders): sitting or lying still in a common room, out of a fight, heals
**1.25** rest hours an hour, against doc 38's bed at 1.5 and 0.75 elsewhere.
- It is a partial rest. A full rest still needs six hours in a bed (doc 38), so the common room heals but doesn't
  reset downings.
- **Company:** **+10% for each other player** in the room, awake and moved or spoken in the last 10 minutes, **up to
  +30%**. Residents don't count: the room always has some in the evening, so counting them would make company free.
- **A performer** in the room gives everyone resting there the full +30% (agreed, §5.1 and §5.7).
- At best that is 1.25 × 1.3 = 1.625, more than a bed. That is the cantina's lesson, by design.
- Company is counted once every 5 s per occupied common room from the cell index (`World::entitiesIn`), and
  `restPlayers` reads the room's factor.

**Performing.** PERFORM in a common room or a venue on its night: sing, tell a tale, or play an instrument carried (a
mouth pipe takes the mouth, so no speech while it plays; the others take the forepaws).
- It lasts while the performer stays in the room and says or acts something (speech or an `/action`) at least every 2
  real minutes, up to 30 minutes, then rests 10 *(placeholders)*. One performer counts per room.
- It earns no XP (doc 48's principle 2). Listeners can give (doc 55) and star (doc 51). Residents in the room hush
  (an ambient bark), and the performance is a line for the chronicle (doc 56).

**Rested time at an inn** (agreed: "builds faster at an inn").
- A wolf who logs out in an inn (its common room, an inn bed, or a bed or place rented there) builds the rested pool
  for that absence at **×1.5** (75 a day, not 50), within the same 300 *(placeholders)*.
- Festivals add more (§6).
- **How:** `Entity::awayAtInn`, set beside `awaySince` in `Game::leaveCharacter` and `releaseLingering`; the ledger
  gains **rested grants** (`SocialLedger::restedGrants`: actor to `{at, amount, source}`, saved with it), counted
  after the latest return, less what has been paid, within `RestedMost`. Doc 49 turns rested time into faster
  practice and keeps the grants.

**The innkeeper** is already a resident. This plan gives them the beds by the night (§4), letters waiting (doc 55),
the tavern games (§5) and a briefing line about the room (below). Introductions are doc 52's.

### 2. Notice boards (agreed, doc 48 §5.4)

**One per town square.** `World::placeBoard(community)` puts it on open ground 2 to 4 tiles from the square's market
point (`square(community).at`), clear of doorways and stall spots, chosen the same way every time, as home stores are
(`placeHomeStores`). It is sent in the snapshot like them (`boardsView`, a glyph and an id) and read from within 2
tiles: READ THE BOARD. Every town with a square has one (the 14 towns and 3 cities on DEV). A Chapter's place keeps its
own board.

**The work-and-stories side** is built when opened (`{"type":"board"}`), from **sources** (`Core/RatwBoards.cpp`, a
`BoardSource` with `entries(community, viewer)` and `take(entry, viewer)`):
- **Contracts** (doc 26, doc 42): the town's open ones by `contractsNear`'s rule, with TAKE IT ON (`takeContract`);
  for goods, what is wanted and where to hand it in.
- **Faction missions** (doc 32): for each faction claiming the town, what its officials would offer (`missionBoard`),
  shown to members of a Chapter that qualifies; TAKE IT ON uses the existing `take` verb, and delivering stays with the
  official.
- **Town projects** (doc 57), **Storykeeper quests** (doc 34 Phase 5: quests whose giver is a board) and
  **storytellers' calls** (doc 58): sources those plans register. Nothing shows for them until then.
- **The festival programme** from three days before (§6).

**The public side: players' notices** (agreed).
- A notice has a **kind** (seeking, offering, event, lost and found, other), a **text** (1 to 280 letters
  *(placeholder)*), and for seeking or offering an optional **what** from a fixed list: an item from the catalog, a
  trade, a room, an apprenticeship, a hunting or work partner. An event has a day, an hour and a place.
- It is a document (doc 55, kind `notice`, `board` the board's id), **carries its writer's scent** as doc 55's table
  reads it, and is unsigned unless signed. Masking oil makes it anonymous to players.
- It **costs a penny** to the board's own town (`Society::treasuryOf(community)`), and **expires after 7 game days**
  (28 real hours) (agreed placeholders). The writer may take it down, without a refund.
- A writer has at most 3 notices on a board, and a board holds 30 *(placeholders)*. A full board refuses: "Every pin is
  taken. Try tomorrow."
- Reports (doc 50) carry its text; a DM can take any down, audited. A blocked wolf's notices are hidden from the one
  who blocked them (doc 50).

**Residents answer "seeking" notices** (agreed), from the notice's structured *what*, never its prose:

| Seeking | Who answers |
|---|---|
| an item | the nearest shop in town that sells it or holds it (`Society::wares`, its stock) |
| an apprenticeship in a trade | a master of that trade in town who could take one (doc 26's rule); the master's trust (30+) still decides |
| a room | a household with a spare bed (§4) |

- Offering notices are answered the same way by a shop that buys the good (`buys` in `crafts.json`).
- The answer is pinned under the notice in the resident's words from a template ("The tanner at La Concia has fine
  hides."), naming them as they would name themselves (`Game::willName`).
- Checked when posted and once a game day; at most one answer a notice. The resident's briefing knows it ("You
  answered the notice seeking fine hides.").
- **No notice text ever goes into a prompt.** Notices are players' prose, and the Mind gets only the answer's facts.

**Chapter boards** move into the document store (kind `chapter_notice`, board `lease:<cell>`): free, no expiry, 20 kept,
with scent. The place panel shows them as now.

### 3. Market stalls (agreed, doc 48 §5.2)

- **Where and when:** the cities' squares with built stalls, on Marketday from 7 to 2 in fair weather, as for the
  merchants. In foul weather there are none, and a fee paid is returned.
- **Renting:** at a free stall spot, RENT THIS STALL FOR TODAY · 3p, paid to the town's treasury. At most 6 player
  stalls a square, one a wolf *(placeholders)*.
- **Free spots** are those no merchant is sent to that day. `tradingAt` needs one check (skip a spot let to a player),
  agreed with the economy session that owns `RatwResidents.cpp`. The phase can't ship without it.
- **Wares:** goods from the purse, each with a price (1 to 999p). They stay in the purse, reserved by the listing
  (`Stall{spot, keeper, wares[{item, quantity, price}]}`): a reserved good can't be sold elsewhere, given or eaten
  without taking it off.
- **Buying:** a player within 2 tiles opens the stall (`{"type":"stall"}`): goods, prices, marks and scent (doc 55). BUY
  moves goods and coins at once (`Society::transfer`, kind `stall sale`, with the 64-kinds check), **only while the
  keeper stands within 3 tiles** and has moved or spoken in the last 10 minutes: face to face (agreed). Haggling is
  roleplay; the keeper can change a price at any time.
- **Residents buying from players' stalls**, in their hour at market (doc 26), when a good their household wants is no
  dearer than in the town's shops. This hooks into the market visit in `RatwDemand.cpp` and `RatwResidents.cpp`, so it
  is this phase's second part, built with the economy session.
- At 2 the listings clear.

### 4. Renting by individuals, and venues (agreed in spirit, doc 48 §5.7)

| What | Period and rent *(placeholders)* | Landlord |
|---|---|---|
| **A bed at an inn:** a bed upstairs, with a small chest | a night (until noon) 2p, or a week 10p | the inn's till (`Society::tillOf`) |
| **A lodger's bed:** a spare bed in a resident's home, with a small chest | a week, 6p | the household's head, to their own purse |
| **The upstairs for a night:** the inn's whole upstairs, locked to the renter's guests | 18:00 to 06:00, 8p | the inn's till |
| **A whole place** on the to-let list marked for individuals: an empty shop, a barn, a cottage (Atlas `let`, DM `estate.set`) | a night or a week, as set | as set |

- Chapters keep everything doc 32 gave them. The whole upstairs can't be let while one of its beds is, and the other
  way round.
- One bed and one whole place a wolf at a time *(placeholder)*. Rent above an inn now goes to the inn's till, the
  Chapters' included.

**Never** (agreed): seats of power (where a ruler, council or great house's head works or sleeps: `Society::houseHead`
and those posts' titles), churches (`World::chapel`), guardhouses and barracks (where the watch works or sleeps), and
any place a resident lives in or works from (`townCells_`), **unless the owner lets a spare part**:
- a bed nobody sleeps in, upstairs at an inn or in a home (doc 36's bed places); the town's warehouse, as now.
- **Letting is income for the resident** (agreed). The head lets only to a wolf they don't dislike (affinity and trust
  0 or more) *(placeholder)*.
- **Nobody is evicted for a renter:** if the household grows (a birth, a marriage), the lodger gets a week's notice and
  the rest of the rent back.

**What a renter gets:** a bed to sleep and log out in (a full rest, doc 38); at an inn, rested time ×1.5 (§1); a small
chest (`let:<cell>:<unit>`, 10 kinds and 20 lb *(placeholders)*, a new facility prefix); letters delivered to their
lodgings (doc 55); a residence the profile can name (doc 50); up to 3 guests in a whole place (doc 55's room lending).
- When a lease ends, the chest's goods go to the renter's purse if there is room; otherwise they wait with the landlord
  ("left with the innkeeper") for 28 game days, then are the landlord's.

**Held for a story** (agreed in spirit):
- A DM (`estate.hold`: place, until, reason) or a story (the same action in doc 34's list) marks a place held.
- New leases are refused ("The landlord has promised it to someone.").
- A lease running gets a week's notice ("The landlord needs the room back."), then ends. Rent paid past the end comes
  back from the landlord's purse; what they can't pay is owed (`Bonds::addOwed`). `estate.release` ends the hold.
- Renters can be part of a story (the watch searches a room; a stranger asks to lodge) through doc 34's actions.
  Leases are never a reason a story can't happen.

**Player-run nights** (agreed in spirit):
- A renter of a whole place, or of the upstairs for a night, posts an **event notice** (§2) and, through doc 50, a
  line on their circles' lists.
- At its hour, OPEN THE DOORS lets anyone in for the night (`mayEnterPlace` treats it as open).
- A performer there gives the company bonus, and rest there heals as in a common room (§1).

**Data:** `estate::Property` gains `individuals`, `night`, `beds` (unit spots), `heldBy`, `heldUntil`, `heldReason`;
`estate::Lease` gains `holder` (`chapter:<id>` or a player's id), `unit`, `period` (`week` or `night`) and
`noticeUntil`, keyed `cell#unit`. Leases and the DM's places move to their own tables (`game.leases`,
`game.properties`; migration `00NN_leases.sql`, sections like doc 32's Chapter tables). The due check walks leases in
order of `paidTo`.

**The page:** the place panel (`place.ts`) shows a bed's terms (TAKE A BED FOR THE NIGHT, FOR A WEEK), HIRE THE
UPSTAIRS FOR TONIGHT, and one's own lodging (days paid, the chest, guests, GIVE IT UP). A lodger asks from the
resident's menu ("ask to lodge"). The character sheet has a LODGINGS line.

### 5. Tavern games (agreed, doc 48 Part 10, decision 32)

- **Where:** at a table (`T`) in a common room or a venue, seated within 1.5 tiles. PLAY on the table: choose a game,
  invite the seated, or ask the room.
- **Resident opponents** (agreed): when no player is free, a resident in the room who is awake, idle and 14 or older
  (16 for stakes) takes a seat. Each plays at its own skill (from age and its id: some are sharp). The ambient director
  leaves them be while they play.

| Game | Players | How it plays *(placeholders)* |
|---|---|---|
| **Knucklebones** | 2 to 4 | Five bones. Rounds "ones" to "fives": toss one, sweep up N, catch it. Each try's chance falls from 90% at ones to 50% at fives, plus DEX ÷ 400 and skill ÷ 400. After a catch, go on or bank; a miss passes the turn and keeps what was banked. First through fives wins. |
| **Wolves and Deer** | 2 (or 2 against 2) | A cross-shaped board of 33 points: two wolves against 13 deer. Deer step forward or sideways; wolves step any way along the lines or take a deer by jumping it, in chains. The pack wins at 7 deer taken; the herd by penning both wolves. No dice. The numbers are tuned by a simulation of residents' play. |
| **Liar's Bones** | 2 to 4 | Five hidden bones each; bid how many show a face across the table; raise or call "liar"; the loser of a call loses a bone; the last with bones takes the pot. |

- **Stakes** (Liar's Bones only, agreed): 0 to 5p each, the same for everyone at the table, held in a table account
  (`table:<id>`) and paid to the winner less one penny to the house (the inn's till). Doc 48's "a penny a round" is
  read as a round of the table: one game.
- **Residents staking** do so only from purses of 30p or more, at most 3 staked games a game day, never over a
  twentieth of their purse *(placeholders)*.
- **Talk counts toward scenes** like any talk (agreed). Moves show to the room as short world lines, not speech.
- **Practice** (agreed): `Entity::gameSkills` (0 to 100 a game) grows by playing, faster against a better player: "Your
  knucklebones sharpened (31)." Against residents, Liar's Bones skill shows an occasional tell ("the miller's ear
  twitches"); against players, skill changes nothing but Knucklebones' catches. Doc 49 folds it into practice.
- **Festival tournaments** (§6), and a mentor teaching a newcomer a game is a line in doc 52's mentor record.

### 6. Festivals that draw players (agreed, doc 48 §5.6)

**The programme** (`Core/RatwFestivals.cpp`), for each town's festival *(placeholder hours)*:

| Hour | What |
|---|---|
| 12:00 | **The feast:** a player at the square gets the meal residents get, from the town's store, once; eaten together it is a shared meal (doc 55). Festival goods on sale. The hunting contest opens. |
| 13:00 | Races |
| 14:00 | Tug-of-war |
| 15:00 | Howling |
| 16:00 | The sparring tourney |
| 17:00 | The hunting contest is judged |
| 19:00 | The storytelling contest |
| 20:00 | The crier (doc 56's slot: `Game::festivalCrier(community)`, empty until then) |
| 21:00 | The tavern-game tournament, at the inn |

- Shown on the board and in the calendar line from three days before. Residents already talk of festivals (the scenes'
  festival topic).
- **Signing up** at the board or with the steward (the innkeeper) until a contest starts. Entry 1p *(placeholder)*
  into the pot. Two or three residents enter each contest so a field is never empty, paying from their own purses.
- **Prizes** are the pot: two thirds to the winner, a third to the second. Conserved and self-funded. If the economy
  orchestrator later gives towns a `festival` channel (doc 46), it adds to the pot through one call.
- **Winners** are announced aloud by the steward, recorded as a `festival won` event (a small deed for doc 56), and
  known to residents' briefings ("Kestrel won the race at Midsummer").

**The contests:**
- **Races:** a loop of marks on open, reachable ground around the square (worked out as the crowd spots are). The
  server times entrants from their positions, which its walking rules already check. Residents run it too, timed from
  their DEX and age.
- **Tug-of-war** (a team haul, doc 48 Part 6): two teams of up to 6 at a rope across the square. PULL on the beat (every
  1.2 s, shown); a pull on the beat counts STR × the stamina left. The marker moves by the difference; 3 tiles or 60 s
  decides it *(placeholders)*. Residents fill the teams.
- **Howling:** entrants howl in turn (doc 51's howl when built; a contest HOWL until then). Each howl's carry comes from
  stamina and a howling skill, times a roll, plus the crowd: each player present may cheer one entrant, +5% each, at
  most +30% *(placeholders)*.
- **The sparring tourney:** a bracket of up to 8 in a ring at the square, bouts until one yields (doc 38's yield duels;
  doc 53's sparring once built), so nothing lasting. Entering agrees to the tourney's bouts, auto-decline or not (doc
  40). Off-duty guards and doc 53's trainer fill it.
- **The hunting contest:** hunts from 12:00 to 17:00 (doc 41; pairs share credit as doc 53 shares kills). The best
  single kill counts: the species' health × quality (crude 0.6, common 1, fine 1.5, masterwork 2.5)
  *(placeholders)*, read from a hook in `World::huntKill`. The goods stay the hunter's.
- **The storytelling contest** (agreed: judged by the audience's stars, doc 51): entrants take turns at the square's
  middle, 5 minutes each, in an open scene. Then each player present gives one star to one entrant other than
  themself (a doc 51 star, counting as stars do). Most different givers wins; a tie goes to the Storyteller tag. With no
  audience there is no winner ("Too quiet a crowd this year.").
- **The tavern-game tournament:** a Knucklebones or Liar's Bones bracket at the inn.

**Festival goods** (agreed): honey cakes, mead, the embroidered festival shawl and ribbon collars, all in the catalog,
made by their makers in the three days before (a `festival` flag in `crafts.json` that only starts those batches then)
and sold only on the day. This needs the crafting owners' agreement (`RatwCrafting.cpp`, `crafts.json`).

**Chapter displays:** an Officer may raise the Chapter's banner at a free stall spot for the day, drawn in its colours;
scenes there count as Chapter scenes, as at a meeting place (doc 32 §5.1).

**Rested time builds all festival day** (agreed): each game hour a player spends at the square from 12 to 23 adds a
rested grant of 15 *(placeholder)*, within the cap.

**The festival quest** (agreed): the board shows quests whose giver is the festival (doc 34 Phase 5, doc 58). A hook
until then.

### 7. The library, the archive and exploration (doc 48 Part 10)

**Where:** places with a scholar's or clerk's post: halls of records, archives, scriptoria, Upper Accord's campuses
(by the post's label, from a list in `Data/Lore/archives.json`). The keeper gives work: "ask for archive work".

**The work, paid like work:**
- **Sorting:** six records to put in order (by date, place or house) from clues on each (a seal, a year, a name). The
  server keeps the answer and checks the order; the page never has it. About 3 minutes.
- **Copying:** 5 real minutes sitting at a desk; a scholarship check decides the errors. Nothing sellable is made: the
  copy stays in the archive.
- 2p a task, at most 4 a game day *(placeholders)*, from the town's treasury (`archive work`), as the Works pays its
  labour. It grows a scholarship skill (doc 49).

**Lore:** each finished task shows the next fragment of that archive's region the wolf hasn't read
(`Data/Lore/fragments.json`: id, region, topic, text up to 400 letters, an earlier fragment it may need), written by
hand or drafted offline and reviewed like the scene library (`tools/lore_library.py`).
- Read fragments are kept on the character (`Entity::lore`), shown on a LORE page in the journal, and go to the
  chronicle (doc 56). An archive with nothing new still pays.
- **Scholars:** a wolf who has read 10 of a region's fragments *(placeholder)* gets one line in its residents'
  briefings ("They have read the old records of Ridgemere: the founding, the flood of year 2."), so residents may ask
  them about history.

**Exploration, kept light:**
- **Bestiary:** each species seen in a hunt or brought down, with the first day and the count (`World::huntKill`).
- **Herbarium:** each forage good found (`World::forage`), with its season and ground.
- **Places:** the cells discovered, by region (doc 44 already counts them).
- **Maps:** a `map` bought from a cartographer, read, adds its region's cells to the travel map. Drawing maps waits
  for player crafting (doc 35).
- Kept as small id sets with first days (`Entity::bestiary`, `herbarium`), saved, on journal pages.

### Wire

| Direction | Message |
|---|---|
| Commands | `perform` (`start`, `stop`); `board` (`read`, `post` with kind, text, what, sign; `unpost`; `take` an entry); `stall` (`rent`, `list`, `unlist`, `price`, `buy`); `estate` verbs `bed`, `night`, `lodge`, `open`, `endlease`, `guest`; `table` (`start`, `invite`, `join`, `move`, `bid`, `call`, `bank`, `leave`); `festival` (`enter`, `pull`, `howl`, `cheer`); `archive` (`ask`, `sort`, `copy`) |
| Events | `board`, `stall`, `table` (to the seated and to watchers within 4 tiles), `festival` (the programme, a contest's state), `archive` (a sorting task) |
| Snapshot | `boards` per cell (a glyph and an id, like `stores`); `self.place` gains bed and lodging terms; `self.company` (the room's factor while resting) |
| `game.events` kinds | `notice posted`, `notice answered`, `stall rented`, `stall sale`, `lease` (with holder kind), `held for a story`, `table game` (stakes, no words), `festival entered`, `festival won`, `archive work` |

### Client

- `Client/src/ui/hud/board.ts` (both sides, posting with the kind and *what*), `stall.ts`, `table.ts` with
  `Client/src/game/tableGames.ts` (drawing the three boards), `festival.ts` (the programme and contests), and an
  archive sorting sheet.
- `place.ts` for beds, nights and lodgings; PERFORM in the actions row in a common room; the rest label shows the
  room's factor ("resting in the common room · 1.5 with company").

### The Mind

The server decides; the model only speaks. New lines in `Game::dialogueContext`'s activity:
- the innkeeper: who lodges at the inn (as the keeper knows them), tonight's player-run night, the festival programme;
- a landlord: "Your lodger, the grey wolf, has paid to Riverday.";
- a resident who answered a notice; residents on festival days: the winners so far;
- for a scholar: the one line above.

### The DM app

- **A Places panel** (the Chapters tab): every place to let, its leases (Chapter or wolf), holds; `estate.set` (with
  `individuals` and `night`), `estate.clear`, `estate.hold` and `estate.release` added to `ACTIONS` and handled in
  `Game::applyDmActions`.
- **Boards:** each town's notices (with scent and author) and TAKE DOWN (`board.remove`, audited).
- **Money tab:** stakes on tables and stall takings, beside contracts and caravans.
- **Calendar panel:** a festival's programme, entrants and winners.

## Phases

### Phase 1: taverns

- **Goal:** the common room heals almost like a bed, better in company or with a performer, and rest at an inn builds
  rested time faster.
- **Changes:**
  - `Core/RatwGameEstates.cpp`: `commonRooms_` in `refreshEstates` (doc 55's Phase 1 builds the list if it comes
    first); company per room every 5 s.
  - `Core/RatwBattle.cpp`: `restPlayers` reads the room's rate and factor (`World::restRate(e)` replaces the literals).
  - `Core/RatwGameSocial.cpp` or `Core/RatwTaverns.cpp` (new): `perform`, its upkeep and its end.
  - `Core/RatwSocialCore.*`: `restedGrants`, `restedLeft` counting them, saved; `Entity::awayAtInn` (`RatwWorld.h`,
    `RatwWire.cpp`), set in `Game::leaveCharacter` and `releaseLingering`, granted on return (`Game::enterCharacter`).
  - `self.company`; the rest label in `Client/src/game/labels.ts`; PERFORM in the actions row.
- **Tests:** `battle_tests` `restAndRepeatedDowns` grows a common-room case (1.25; +10% a player to +30%; residents not
  counted; a performer gives +30%; no full rest there); `Tests/tavern_tests.cpp` (new: performing starts, lapses after
  2 quiet minutes, rests); `level_tests` `rested` (×1.5 for a logout at the inn, the cap, grants paid once);
  `Client/src/game/game.test.ts` (the rest label); `tools/client/tavern.mjs` (two players sit in an inn and see
  "with company").
- **Done when:** in a game test a wolf heals 1.25 alone and 1.625 with three others in the common room, and returns from
  a day away at the inn with 75 rested.
- **Cost:** the company count is per occupied common room every 5 s, from the cell index; a city inn full of players is
  one small loop. `world_check --players 20` unchanged.

### Phase 2: notice boards

- **Goal:** every square has a board: work and stories on one side, players' notices on the other.
- **Changes:** `World::placeBoard` and `boardsView` (`RatwWorld.cpp`, `RatwGame.cpp`); `Core/RatwBoards.{h,cpp}` (new:
  sources for contracts, faction missions and the festival programme; registration for docs 34, 57, 58); notices in
  doc 55's store with the penny to `treasuryOf(community)`, expiry from a queue, limits; residents' answers
  (`Data/Voice/notices.json` templates); Chapter notices moved into the store (a one-time move on load); the DM's
  board view and `board.remove`; `Client/src/ui/hud/board.ts`.
- **Tests:** `Tests/board_tests.cpp` (new): the board placed the same way every time and off doorways; contracts and a
  qualifying Chapter's missions listed and taken; a notice posted (a penny to the town, not the capital), scented,
  masked, expired at 7 days, refused when full; answers to an item, an apprenticeship and a room, once; old Chapter
  notices carried over. `estate_tests` (the Chapter board through the store); `tools/test_dungeon_master.py`
  (`board.remove`); `tools/client/board.mjs`.
- **Done when:** a player in Saltreach reads the town's contracts, takes one, and pins a notice a resident answers.
- **Cost:** the board is built only when someone opens it; expiry from a queue; answers checked on posting and once a
  game day. Nothing in the tick.

### Phase 3: renting by individuals, and venues

- **Goal:** any wolf can take a bed at an inn, lodge with a resident, hire a place for a night or a week, and hold a
  player-run night, within the agreed limits; stories can take a place back.
- **Changes:** `Core/RatwEstates.*` (holders, units, periods, holds, notice); `Game::refreshEstates` (beds upstairs,
  spare beds in homes, the never-list, rent to tills); `estateCommand` verbs; the `let:` facility prefix
  (`RatwCareers.cpp`); `mayEnterPlace` for players' leases and open nights; event notices with OPEN THE DOORS;
  `Database/migrations/00NN_leases.sql`; the DM's Places panel and actions (`tools/dungeon_master.py`,
  `Editor/src/dm/ChaptersTab.tsx`); `place.ts`; landlords' briefing lines.
- **Tests:** `estate_tests` grows: a bed by the night (rent to the till), lapsing at noon; a lodger's bed only where a
  bed is spare and only from a head who doesn't dislike the wolf; refused in a church, a guardhouse, a lord's hall and
  a lived-in home; the upstairs not let whole while a bed is let; a hold with a week's notice and the rest refunded or
  owed; the chest's goods home at the end; a night opened to all; saving. `pg_tests` (the leases tables);
  `tools/test_dungeon_master.py` (the actions); `tools/client/lodging.mjs`.
- **Done when:** a player rents a bed, logs out in it, and comes back fully rested with ×1.5 rested time; a DM's hold
  ends a lease with notice; money is conserved.
- **Cost:** leases are checked in order of when they fall due, so a thousand leases cost nothing until one is due.
  `refreshEstates` stays a 5-minute pass, now also counting beds in homes once.

### Phase 4: market stalls

- **Goal:** players sell their own goods face to face from the cities' stalls on Marketday.
- **Changes:** `Core/RatwStalls.cpp` (new): renting, listings, reservations, buying with the keeper present, the end of
  the day; the one check in `tradingAt` (with the economy session); `Client/src/ui/hud/stall.ts`; then, with the economy
  session, residents buying from stalls in their market hour.
- **Tests:** `Tests/stall_tests.cpp` (new): rented only on Marketday, in fair weather, at a free spot, at most six; a
  merchant never sent to a let spot; a sale with the keeper present, refused with them away; reserved goods can't be
  given or eaten; the 64-kinds refusal; money conserved; cleared at 2. `tools/client/stall.mjs`.
- **Done when:** on a Marketday in Ridgemere one player sells a fine hide to another at their stall.
- **Cost:** commands only; the day's end clears at most 18 stalls.

### Phase 5: tavern games

- **Goal:** Knucklebones, Wolves and Deer and Liar's Bones at any inn table, against players or residents.
- **Changes:** `Core/RatwTavernGames.{h,cpp}` (new: the three games' rules, pure); `Core/RatwGameTables.cpp` (new:
  tables, seats, residents' seats and turns, stakes in `table:<id>`, the house penny); `Entity::gameSkills`;
  `Data/Games/tavern_games.json`; `Client/src/ui/hud/table.ts`, `Client/src/game/tableGames.ts`.
- **Tests:** `Tests/tavern_tests.cpp`: each game's rules (Knucklebones' banking, Wolves and Deer's jumps and pens,
  Liar's Bones' bids and calls); a simulation of residents' Wolves and Deer that neither side wins more than 60%;
  stakes capped at 5p, the pot paid less the house penny, money conserved; residents staking only by the rule;
  `Client/src/game/tableGames.test.ts`; `tools/client/tavern.mjs` (a game of Knucklebones against a resident).
- **Done when:** a lone player can always find a game in an inn in the evening, and stakes balance to the penny.
- **Cost:** a table's work is done on its moves; a resident's move is computed on its turn (a small board).

### Phase 6: festivals that draw players

- **Goal:** festivals with a programme, contests, the feast for players, festival goods and rested time.
- **Changes:** `Core/RatwFestivals.cpp` (new): the programme, sign-up, pots, each contest, residents entering, the feast
  for players, rested grants, the crier's empty slot; a hook in `World::huntKill`; the tourney's consent beside
  `noPvp`'s check; the festival source on the board; the `festival` flag in `crafts.json` (with the crafting owners);
  `Client/src/ui/hud/festival.ts`; the DM's Calendar panel.
- **Tests:** `Tests/festival_tests.cpp` (new): the programme on a festival day and a called one; the feast once; each
  contest decided by its rule (a race timed, a tug won, a howl with cheers, a bracket of yields with no lasting
  injury, the best kill, the storytelling winner by distinct stars, no winner without an audience); pots paid and
  conserved; rested grants. `schedules_tests` still pass. `tools/client/festival.mjs`.
- **Done when:** on a festival day a player eats at the feast, enters the race and the storytelling, and the winners
  are announced and known to residents.
- **Cost:** a contest runs one at a time per town and looks only at its entrants; nothing happens on other days.

### Phase 7: the library, the archive and exploration

- **Goal:** work that teaches the world, and journal pages that fill as a wolf finds things.
- **Changes:** `Core/RatwArchive.cpp` (new): archive posts, sorting and copying, pay, fragments, scholars;
  `Data/Lore/archives.json`, `Data/Lore/fragments.json`, `tools/lore_library.py`; `Entity::lore`, `bestiary`,
  `herbarium` from `huntKill` and `forage`; maps read into the travel map; journal pages in `dialogs.ts`.
- **Tests:** `Tests/archive_tests.cpp` (new): work only where a keeper is; a sorting order checked by the server; pay
  and the daily limit; fragments in order, never twice; the scholar's line; bestiary and herbarium entries; a map
  revealing its cells. `tools/client/archive.mjs`.
- **Done when:** a player sorts records in Upper Accord, is paid, reads a fragment, and finds it in their journal.
- **Cost:** commands only.

## Built

### Phase 1 (2026-10-08): taverns

- **Common rooms** are the inns' cells: where a post of the `inn` business works, the same list doc 55's letters use
  (`Game::innCells_`, refreshed every five minutes).
- **Company:** every 5 s (`Game::tendTaverns`, new `Core/RatwTaverns.cpp`) the game tells the world
  (`World::setCommonRooms`) which common rooms have players in them, which of those were active in the last 10 real
  minutes (any command; entering counts), and whether a performer is there. Residents never count.
- **Rest** (`World::restRate`, now read by `restPlayers` in place of the literals):
  - a bed the wolf has a right to: 1.5;
  - a common room: 1.25 × `companyFactor` (+10% for each other active player, up to +30%; a performer gives the full
    +30%), so at best 1.625;
  - anywhere else still, another's bed included: 0.75.
  - The common room is a partial rest: no full rest there.
- **A full rest needs a bed the wolf has a right to** (the user): `World::setBedRight`, wired to `Game::hasBedRight`.
  For now that is its Chapter's own rented place; Phase 3 adds lodgings and paid inn beds. Without a game (World
  alone), any bed counts, so the world's own tests stand. Away time counts as a bed rest only in such a bed
  (`awayInBed` is now `bedIsTheirs`).
- **Performing** (`perform` with `start` and `stop`; a Perform / Stop performing button shown in a common room):
  - Sing, tell a tale, or play an instrument carried (mouth pipe, paw drum, hurdy-gurdy, handbell).
  - One performer a room; the room is told ("Bo begins to sing. The room settles to listen.").
  - The performer keeps it up by saying or doing anything at least every 2 real minutes, up to 30, then rests 10
    (`Options::performQuietSeconds` and `performLongestSeconds`, so tests can shorten them). It earns nothing, and a
    `performance` event is recorded.
- **Rested time at an inn:**
  - A wolf leaving the world in a common room is marked `awayAtInn` (saved). On its return (`returnFromAway`) its
    rested practice gains half again what the time away gave (`0.5 × restedPerDay × time away`), within `restedMost`.
  - This is doc 49's practice pool (5 a day, at most 30), which replaced doc 44's 50-a-day pool the plan was written
    against. So ×1.5 means 7.5 a day.
- **Shown:** the rest label reads "resting in the common room · 1.25", "· 1.63 with company", or "resting (not your
  bed: a partial rest)" (`self.rest` gains `rate`, `room`, `notYours`). `self.commonRoom`, `company` and `performing`
  are sent too.
- **Tests:**
  - `Tests/gathering_tests.cpp` (new, 15 checks; the three-town fixture now shared with `letters_tests` through
    `Tests/town_fixture.h`):
    - 1.25 × 1.2 with two others active, and 1.625 with three; the status shows it;
    - 0.75 in the street; 1.25 with only residents about; a performer's +30%;
    - one performer a room, and not in the street; a performer quiet too long trails off;
    - another's bed in the room gives the room's rest and no full rest, while a world with no game takes any bed;
    - away a day at the inn: half again the rested practice.
  - `Client/src/game/taverns.test.ts`: the rest labels.
  - `tools/client/tavern.mjs` (new): alone 1.25; Perform shows; with Bo, 1.38 with company; Bo sings and the room is
    told; 1.63 with company. Screenshots in `artifacts/screenshots/tavern/`.

### Phase 2 (2026-10-08): notice boards

- **One by each square** (`World::boardSpot`, new `Core/RatwBoards.cpp`): the nearest open tile 2 to 4 tiles from the
  square's market point, off its stall spots and doorways, chosen the same way every time and kept. It is sent in the
  snapshot like home stores (`boards`: id, place, town, how many notices) and drawn as a `¶`. Pointing at it says how
  many notices are up.
- **Reading** within 2 tiles (`self.nearBoard`, a Read the board button; the `board` command with `read`, `take`,
  `post` and `unpost`; a `board` event, shown in `Client/src/ui/hud/board.ts`):
  - **The work side:** the town's open contracts by `contractsNear`'s rule, with TAKE IT ON (`World::takeContract`).
    Contracts offered by letter to someone else (doc 55) are left off until their days are out.
  - **The public side:** players' notices.
- **Notices** (documents of kind `notice` in doc 55's store, `board` "board:<community>", indexed by board):
  - The kind (seeking, offering, event, lost and found, other); the text (1 to 280 letters); signed with one's own
    names or not.
  - For seeking or offering, a **what** from a fixed list: a good from the catalog (by name or id; `items::allGoods`,
    new), an apprenticeship in a trade (`items::businesses`, `items::business`, new), a room, or a partner.
  - A penny to the board's own town (`treasuryOf(community)`, not the capital); 7 game days, taken down once a game
    hour; at most 3 a writer and 30 a board ("Every pin is taken. Try tomorrow.").
  - The writer's scent, read as a letter's (doc 55); masked, none. The writer may take it down. A blocked wolf's notices
    are hidden from the one who blocked them.
- **Residents answer** the structured *what*, never the text, on posting and again once a game day; one answer a
  notice; a `notice answered` event.
  - A good sought: a shop in town whose till has it ("The stall keeper has honey (pot).").
  - A good offered: a maker in town who uses it.
  - An apprenticeship: a master of that trade in town who trusts the writer (30) ("… would take on an apprentice; ask
    at the workshop.").
  - The answer names the resident as each reader knows it.
- **The DM:** `board.remove` (a notice taken down, recorded as `notice removed`). `estate.set` and `estate.clear` were
  handled by the game but missing from the DM tool's `ACTIONS`, the epic's logged bug; all three are now in it.
- **Not done:**
  - faction missions on the board (they stay with officials);
  - rooms answered by households (renting is Phase 3), and the festival programme (Phase 6);
  - Chapter boards moving into the store (they stay as they were);
  - a DM screen for boards (the action exists; the app has no panel yet).
- **Tests:**
  - `gathering_tests` `boards` (now 31 checks): Ser Ferro's board placed, the same every time; not from across town; the
    status says it's near; the town's contract listed and taken from the board; a notice pinned with a penny to Ser
    Ferro, not the capital; read with an unknown scent and "honey"; the stall answers a second notice; the innkeeper
    would take on an apprentice who has its trust; three a writer; a masked writer leaves no scent; seven days on, the
    board is bare; money conserved.
  - `tools/test_dungeon_master.py`: the three actions are for DMs, not viewers.
  - `tools/client/board.mjs` (new): Ash walks to Upper Accord's board and pins a notice seeking honey; Bo reads it, with
    its scent line. Screenshots in `artifacts/screenshots/board/`.

### Phase 3 (2026-10-08): renting by individuals, and venues

- **Lodgings** (`Core/RatwLodgings.cpp`, new; the `lodge` command; a LODGINGS / TO LET panel, `lodging.ts`; "Ask to
  lodge" in a resident's menu). Chapters keep doc 32's leases as they were; individuals' lodgings are their own list
  (`Game::Lodging`), and the two never share a place.
  - **A bed upstairs at an inn** (a cell named "<inn>, upstairs", whose landlord is a keeper): a night (to noon) 2p, or a
    week 10p, to the inn's till; the nearest bed nobody has (`World::bedTiles`, new).
  - **The whole upstairs for a night** (to 06:00), 8p, to the till: not while a bed there is let, nor the other way round.
  - **A lodger's bed:** a spare bed in a resident's home, one the household doesn't sleep on (`World::spareBeds`, new).
    A week, 6p, to the head's own purse. Refused by a head with liking or trust below 0.
  - **A place listed for individuals:** `estate::Property` gains `individuals` and `night` (saved, and set through
    `estate.set`'s payload), taken for a night or a week.
  - One lodging a wolf. Nothing else is offered, so seats of power, churches, guardhouses and lived-in places never are.
  - Rent is a `Society::shift` (kind "rent"). A week renews from the renter's purse at its end, or ends; a night ends at
    its hour.
  - **The chest** (`let:<id>`, a new facility prefix): 10 kinds and 20 lb. At the end its goods go to the renter, or to
    the landlord if they won't fit (the plan's 28 days' wait wasn't built).
- **Rights:** a lodging's bed (or any bed in a whole place) is a bed the wolf has a right to (`hasBedRight`), so it gives
  a full rest, and leaving the world in an inn's bed counts as away at the inn (rested ×1.5). Letters go to the
  lodging's town first (doc 55's post town).
- **Venues:** a renter of a whole place or the upstairs may OPEN THE DOORS for the night (`mayEnterPlace` lets anyone
  in) and keep 3 guests. An opened place counts as a common room (company, and performing there).
- **Held for a story** (DM actions `estate.hold` with days and a reason, and `estate.release`; `Game::holdForStory` for
  tools and tests):
  - New lodgings there are refused ("The landlord has promised it to someone.").
  - Running ones get a week's notice, then end with the rest of the rent returned from the landlord, or owed
    (`Bonds::addOwed`) if it can't pay.
- **Saved** in the people root (`lodgings`, `holds`). Migration `0045_lodgings.sql` gives lodgings their own table; it is
  written, not applied.
- **Tests:** `gathering_tests` `lodgings` (now 46 checks; the fixture gains an optional upstairs room with beds):
  - three offers upstairs; a bed for the night for 2p; the whole upstairs refused while a bed is let;
  - her bed rests her fully (1.5) and isn't Bo's;
  - a meal in her chest; back from a night away in her own bed, fully rested;
  - the night over, the lodging ends and the meal comes home;
  - a resident lets its spare bed for 6p to its own purse; a head who dislikes Bo won't;
  - held for a story: a week's notice, ended, no new lodging there; money conserved.
- **Not done:**
  - a lodger's notice when the household grows (no birth event yet);
  - the profile's residence line, and landlords' briefing lines;
  - the DM app's Places panel (the actions exist);
  - a browser check (the test world's upstairs room isn't walkable).

### Phase 4 (2026-10-08): market stalls

- **Renting** (`Core/RatwStalls.cpp`, new; the `stall` command; a MARKET STALL panel, `stall.ts`; a pennant over a let
  stall on the map):
  - On Marketday from 7 to 2, in fair weather, a wolf standing at a stall spot on a city's square (`World::stallSpots`,
    the built stalls' spots) sees RENT THIS STALL FOR TODAY · 3p, paid to the town's treasury.
  - At most 6 a square (half its spots, if it has fewer than 12), one a wolf, one wolf a spot.
- **Merchants and let spots:** a let spot is left out of Marketday's plan (`World::setLetStalls`, filtered in
  `World::dayPlan`), so merchants set up at the others. Two may share a spot, as before.
  - `RatwResidents.cpp` needed no change: `tradingAt` already picks only from the plan's spots.
  - This differs from the plan's rule ("free spots are those no merchant is sent to"). A player may take any spot no
    player has, and a merchant whose spot it was picks another. Under the plan's rule, a busy square would have had
    almost no free spots.
- **Wares:**
  - Goods from the purse (not worn, nor lent: `spareOf`), up to 10 kinds, each with a price apiece from 1 to 999p.
  - They move into the stall (`stall:<id>`, a new facility prefix) rather than being "reserved" in the purse. The
    effect is the same: what is listed can't be sold to a merchant, given or eaten until it's taken off.
  - The keeper may change a price, or take wares off, while within 3 tiles; PACK UP ends the day early (no refund).
- **Buying:**
  - A wolf within 2 tiles sees the wares, prices and scent (doc 55's records stay with the keeper and pass to the
    buyer). BUY works only while the keeper stands within 3 tiles and has stirred in the last 10 minutes.
  - Goods and coins move at once (`Society::shift`, "stall sale", refused past 64 kinds; undone if the coins fail).
  - The keeper is told who bought what.
- **Residents buying** (the plan's second part; built in `RatwStalls.cpp` alone, with no change to the economy
  session's files): once a game hour, grown residents of the stall's own town within 6 tiles of a kept stall look its
  wares over, once a market day each, and about one in three buy one thing. They buy only:
  - a treat (food at 3p or more), a household need, finery, care, a pastime or something for the home;
  - at no more than the town's price (`Society::townPrice`);
  - within a tenth of what they hold beyond a week's food.

  Three buyers a stall an hour at most. Food goes in the purse, to be eaten as residents eat what they carry; anything
  else is used.
- **The end:** at 2 the stall clears and its goods go home. Foul weather clears it too, and the fee comes back from the
  town. A purse too full to take everything back leaves the rest waiting in the stall until there's room.
- **Saved** in the people root (`stalls`), with the let spots restored on load. No migration: a stall lasts a morning.
- **Tests:**
  - `gathering_tests` `stalls` (now 71 checks):
    - no stall on another day;
    - the offer at a free spot; rented for 3p to the town; the spot out of the merchants' plan, and the merchant not
      sent there; one wolf a spot;
    - three meals listed, none left to give or eat; Bo sees them with Ash present and buys one; refused with her away;
    - the 64-kinds refusal; townsfolk buying a household need at 1p;
    - cleared at 2 with the goods home and the spot back in the plan;
    - a storm clears the next week's stall with the fee returned, and no stall in a storm; money conserved.
  - `tools/client/stall.mjs` (the real page, Marketday noon):
    - Ash rents a stall from the panel and lays out a meal at 4p;
    - Bo comes up, sees BUY · 4p and buys it; Ash is told.
- **Not done:** stall takings in the DM's Money tab.

### Phase 5 (2026-10-08): tavern games

- **The rules** (`Core/RatwTavernGames.{h,cpp}`, new, pure, with a seeded random source):
  - **Knucklebones** as the plan wrote it: 90% at ones to 50% at fives, plus DEX ÷ 400 and skill ÷ 400, capped at
    98%. Bank or go on; a miss loses what wasn't banked; the first through fives wins.
  - **Wolves and Deer** on the 33-point cross: two wolves at the top, 13 deer on the bottom three rows, the deer
    first. Moves go across and down only; the plan's "along the lines" diagonals weren't built. Jumps chain, with
    STOP JUMPING to end one early. The pack wins at 7 deer taken or when the deer can't move. The herd wins by penning
    both wolves, or by holding out 200 moves.
  - **Liar's Bones:** five bones each; bids raise the count, or the face at the same count; a call shows all the bones,
    and whoever was wrong loses a bone and starts the next round; the last with bones wins. No wild ones.
  - **Residents' play:** Knucklebones banks by how much is at risk. Wolves and Deer weighs every move: the pack by
    jumps, threats and room to move, the herd by never leaving a deer to be taken and by closing in. Liar's Bones calls
    a bid well past what the resident believes, else raises on its best face.
  - **Skill:** a resident's comes from its age and id (`residentSkill`), so some are sharp. A duller player's
    reckoning wanders more, rather than making random moves. Tuned by simulation, the pack wins 45 to 53% at any equal
    skill (53% in the test's 400 games).
- **The tables** (`Core/RatwGameTables.cpp`, new; the `table` command; an AT THE TABLE panel, `table.ts` and
  `tableGames.ts`):
  - **Where:** a `T` tile in a common room (`innCells_`) or an opened venue, within 1.5 tiles.
  - **Seating:** SET OUT a game; others JOIN (4 seats; Wolves and Deer 2), or are asked by a private message
    (`invite`). ASK THE ROOM seats the nearest resident who is awake, not at work, not in a fight, and 14 or more
    (16 for stakes). BEGIN starts the game with two or more.
  - **A seated resident keeps its seat:** a new errand before its day's plan (`World::seatResident`, read in
    `World::errand`), let go at the end. A hungry or sleepy resident still goes; gone two minutes, it forfeits.
  - **Turns:** a resident takes its turn after 2 world seconds.
  - **What the room sees:** every move goes to the table and to watchers within 4 tiles, and into the table's log,
    each wolf named as the viewer knows it. Talk is ordinary talk, so it counts toward scenes.
- **Stakes** (Liar's Bones, 0 to 5p each):
  - Held in `table:<id>` (a new facility prefix), and paid to the winner less a penny to the house: the inn's till, a
    venue's landlord, else the town.
  - Residents stake only from purses of 30p or more, at most 3 staked games a game day, never more than a twentieth
    of the purse.
  - **Forfeits:** a wolf gone from the table a minute, or idle ten on its turn, forfeits its stake. With one player
    left, that one wins; with more, the game breaks up and the pot goes back to those who stayed.
- **Practice:** `Entity::gameSkills` (0 to 100 a game, saved) grows by 1 each game, or 2 against a better player,
  slowing near the top: "Your knucklebones sharpened (31)."
  - Skill affects Knucklebones' catches only.
  - Against residents, a sharp Liar's Bones player now and then sees a bluffer's tell ("…'s ear twitches."), with a
    chance of the player's skill ÷ 200 on a bluff.
- **Tests:**
  - `tavern_games_tests` (new, 26 checks): the catch chances; banking and misses; the board, deer never stepping back;
    a chain of jumps; a pen; the balance simulation; Liar's Bones' raises, a call and the last with bones.
  - `gathering_tests` `tables` (now 88 checks):
    - Knucklebones between two wolves, played to a winner, with practice;
    - Liar's Bones for 3p: a resident with no money refused, another seated; 6p in the pot; the winner up 2p and the
      loser down 3p; the house's penny to the inn's till; the resident kept to its seat, then let go; money conserved;
    - a wolf leaving Wolves and Deer hands the other the win.
  - `Client/src/game/tableGames.test.ts`.
  - `tools/client/tables.mjs` (the real page): at dusk Ash sets out Knucklebones, asks the room, a regular takes a
    seat, and they play it out to a winner.
- **Not done:**
  - Wolves and Deer for 2 against 2;
  - the ambient director leaving players alone (a seated resident may still talk to others; it doesn't leave);
  - stakes in the DM's Money tab;
  - mentors teaching a game (doc 52), and festival tournaments (Phase 6 uses the race, tug, howl and the rest instead).

### Phase 6 (2026-10-08): festivals that draw players

- **The programme** (`Core/RatwFestivals.cpp`, new; the `festival` command; a FESTIVAL panel, `festival.ts`) runs on a
  town's festival day: the season's 46th, or one a DM called (`Game::festivalOn`, World's own rule). A fair is made when
  a wolf is in the town that day (`Game::fairs_`).
  - The panel shows a festival 1 to 3 days ahead, then the day's programme: each slot's state and winner, ENTER · 1p
    while a contest is open, and what to do now.
  - The board carries the programme from three days before (`festivalBoard`).
- **The feast:** from noon, a wolf at the square (within 12 tiles of its middle) is given a meal from the town's store,
  once.
- **Rested time:** each game hour at the square from 12 to 23 adds half a day's rested practice (2.5), within its most
  (30). The plan's "15 an hour" was in doc 44's old units, and would fill the pool in two hours.
- **Signing up and pots:**
  - Sign up at the square, the board or the inn until the contest begins: 1p into its pot (`fest:<town>:<day>:<contest>`,
    a new facility prefix).
  - Two or three residents of the town enter each contest except the storytelling: grown, awake, not on watch, from
    purses of 10p or more. Off-duty guards go first into the tourney.
  - **The town adds to the pot** (the user's answer): 2p an entrant, up to 10p, from its own treasury.
  - Two thirds of the pot to the winner, a third to the second; a tug's teams share their parts.
  - With fewer than two entrants, no winner, or the day ending first, every penny goes back to whoever put it in
    (the town's share to the town).
- **Winners:** called by "the steward" to everyone at the square and every entrant, recorded as `festival won`, shown in
  the programme, and known to the town's residents that day (`festivalBriefing`, in the Mind's activity: "At today's
  Midsummer: a grey wolf won the race.").
- **The contests:**
  - **Races (13:00):** four marks, the farthest crowd spot in each quarter round the square, then back to the middle.
    Racers are timed from where the server has them; residents are timed from DEX and age. Three minutes. The next
    mark is drawn on the map (⚐).
  - **Tug-of-war (14:00):** two teams, players spread between them. PULL counts on the beat (every 1.2 s, ±0.35 s) as
    STR × stamina; each resident pulls on three beats in four. The marker moves by the difference; 3 tiles or a minute
    decides it.
  - **Howling (15:00):** in turn, 15 s each. A howl's carry comes from stamina and howling skill (`gameSkills["howl"]`,
    grown by howling), times a roll. The crowd's cheers add 5% each, at most 30%, one cheer a wolf, never for itself.
  - **The sparring tourney (16:00):** a bracket of up to 8, bouts one at a time. Each bout is a real spar to a yield
    (doc 53's spar terms, bruises at worst): the two step into the ring at the square's middle, side by side
    (`World::tourneyBout`), and whoever yields, goes down or flees loses (the user, 2026-10-08: "keep it to yield").
    Entering agrees to the bouts, even for a wolf who declines challenges. A wolf not at the square within a minute of
    its bout's call forfeits it; a resident entrant is fetched to the ring.
  - **The hunting contest (12:00 to 17:00):** an entrant's best kill, read from a new hook in `World::huntKill`
    (`World::takeHunted`). Its worth is the species' health × how clean it was: clean 1.5, good 1, rough 0.8, ragged
    0.6. The plan's quality grades (crude to masterwork) aren't what a kill carries. Residents' kills are rolled.
  - **Storytelling (19:00):** entrants take the middle in turn, 5 minutes each, then two minutes for stars.
    - Each wolf at the square gives one star to one teller other than itself: the festival's STAR, a doc 51 star of
      the kind "festival" counting as stars do. A star given the ordinary way during the contest counts too.
    - The most different givers wins. With none: "Too quiet a crowd this year."
  - **20:00 the crier:** doc 56's slot (`festivalCrier`), empty.
  - **21:00 games at the inn:** the tables (Phase 5), with no tournament bracket.
- **Tests:**
  - `gathering_tests` `festivals` (now 127 checks with Phase 7), on a festival called for today:
    - the programme; sign-ups, once each; the feast once; rested time;
    - Ash runs the race round its marks home and the winner is called; the tug decided on the beat; the howling with
      cheers;
    - the tourney: Ash spars a resident in the ring to a yield, then residents spar it out; the hunt among residents;
    - the storytelling: Cy and Bo star Ash, one star a wolf, not one's own; Ash wins and is paid, with the town's share;
    - money conserved throughout.
  - `Client/src/game/festival.test.ts`.
  - `tools/client/festival.mjs` (the real page): the calendar moved to the festival day at noon; Ash walks to the
    square, is fed, sees the programme and enters the race from the panel.
- **Not done:**
  - **festival goods:** the crafting session's `crafts.json` flag;
  - **Chapter banners** at a stall spot;
  - **the festival quest** (docs 34 and 58);
  - **a tavern-game bracket;**
  - **the DM's Calendar panel;**
  - (Decided by the user, 2026-10-08: contests run whatever the weather, though residents keep indoors on a foul
    festival.)

### Phase 7 (2026-10-08): the library, the archive and exploration

- **Archive work** (`Core/RatwArchive.cpp`, new; the `archive` command; ARCHIVE WORK and COPY A PAGE in the actions row
  where a keeper is; a sorting sheet, `archive.ts`):
  - **Where:** wherever a resident holding a records post is there and awake. The posts are in
    `Data/Lore/archives.json` (new): the Hall of Records' "copying the city rolls", the Concord Annex's four keepers,
    the Warden crypt's librarian, and any label with "archive" or "record clerk".
  - **Sorting:** six records with clues, to be put in order by one of three rules: a year of the old count ("the
    123rd winter"), a roll's number in old numerals ("Roll XIV"), or the time of year ("taken in late autumn"). The
    server keeps the order and checks it ("4 of 6 in the right place"); three tries, then the keeper takes them back.
    The page never has the answer.
  - **Copying:** five minutes sitting at a desk in the archive; the clock stops when the copier stands. A scholarship
    check counts the slips: none or one, full pay; more, half.
  - **Pay:** 2p a task from the town's treasury (`archive work`), 4 a game day. Leaving the archive drops the work.
    Each finished task grows scholarship (`gameSkills["scholarship"]`).
- **Lore:**
  - `Data/Lore/fragments.json` (new): 36 hand-written fragments, a first set for review. Ten are Upper Accord's (the
    First Oath, the Hall of Concord, the hearings, the Warden Order, the Annex, the Watch, the fountain), eight each
    are Ser Ferro's and Ridgemere's, and ten are shared by every town. They build only on what worldgen already says,
    and their dates are "the old count", never the game's calendar.
  - A finished task shows the next fragment of the archive's own town not yet read (its `after` read first), else a
    shared one. An archive with nothing new still pays.
  - Fragments read are kept on the character (`Entity::lore`, saved).
  - The plan's drafting tool (`tools/lore_library.py`) wasn't built; the fragments were written by hand.
- **Scholars:** a wolf who has read 10 of a town's fragments (or all it has) gets a line in that town's residents'
  briefings ("This wolf has read the old records of Ridgemere (the lake, the ice and the pier); you might ask them about
  the town's history.").
- **The journal** (a JOURNAL button on the character page; the `journal` command and event):
  - **Lore:** what has been read, with its topic and town.
  - **Bestiary:** each species brought down, with the first day and a count (from `World::huntKill`).
  - **Herbarium:** each forage good the first time it is found, with the season and ground (`World::noteFound`, from
    `World::forage`).
  - **Places:** the cells a wolf has been in, counted by town (`Entity::places`, new: doc 44's discovery count went
    with the levels).
  - All of these are saved on the character.
- **Not done:**
  - **Maps** revealing their region on the travel map (the travel map doesn't read `places`);
  - **the bestiary's "seen":** only kills are counted;
  - **the chronicle** (doc 56).
- **Tests:**
  - `gathering_tests` `archive` (now 124 checks):
    - no work away from a keeper; six records, and no answer sent;
    - the test solves them from the words alone; a wrong order counted; the right one paid 2p by the town, with
      Upper Accord's first fragment;
    - four tasks, then "enough for one day"; copying, sitting for five minutes, paid; leaving drops the work;
    - the journal's lore, herbarium and places; money conserved.
  - `Client/src/game/archive.test.ts`.
  - `tools/client/archive.mjs` (the real page): Ash goes to the clerk, asks for work, sorts the records in the sheet
    with ▲, hands them in, is paid, reads the fragment, and opens the journal.

## Depends on and feeds

- **Depends on:** doc 55's document store (notices) and scent table (Phases 2 to 4); doc 51's stars and howl
  (Phase 6's storytelling contest and howling; both have stand-ins); doc 50's block, report and circles (Phases 2 and
  3 work without them); doc 53's sparring and trainer (Phase 6 uses yield duels until then).
- **Feeds:** doc 52 (the innkeeper's common room and board for introductions); doc 56 (festival winners, performances,
  lore and the crier's slot); doc 57 (projects post on the board); doc 58 and doc 34 (quests and calls post on the
  board); doc 49 (game, howling and scholarship skills as practice; rested grants).

## Risks

- **Economy files:** the stall check in `tradingAt`, residents buying from stalls, festival goods and a festival
  channel all touch code the economy session owns. Each is named in its phase and built with that session.
- **The common room beats a bed for healing** in company (1.625 against 1.5). Intended, but beds still give the full
  rest; if inns empty the rooms upstairs, lower the cap.
- **Crowds:** a full common room or festival square is the crowding doc 31 warns of (separation). The perf gate and
  `game_load` with a crowded inn check it.
- **Gambling with residents** moves residents' money to players. The stake cap, the 30p floor and 3 games a day keep it
  small, and the Money tab shows it.
- **Moderation:** public notices are players' prose in public. Reports, DM removal, blocks, the penny and the cap cover
  it; notices never reach a prompt.
- **Races** rely on the server's checks of client walking. A race that looks wrong in tests should run on server
  movement for its entrants.
- **Lore is writing work.** The archive pays without new fragments, so it can open with few.

## Decisions

Agreed (doc 48):
1. Taverns as the place for rest and healing (§5.1, decision 4), with doc 48's placeholders: common-room rest at
   1.25 (a bed 1.5), +10% per other wolf present up to +30%, or a performer; rested time faster at an inn; a resident
   innkeeper.
2. Players rent a stall by the day on Marketday, at the cities' stalls (§5.2).
3. One board per town square with two sides: work and stories; public notices with scent, a penny to the town,
   expiring after 7 game days, answered by residents when seeking (§5.4, decision 21).
4. Festivals draw players: contests (hunting, howling, tug-of-war, races, a sparring tourney, a storytelling contest
   judged by stars), festival goods, rested time, a festival quest (§5.6, decision 22).
5. Renting by individuals, agreed in spirit (§5.7, decision 22), with doc 48's proposed details taken as written:
   anywhere but seats of power, churches and guardhouses; never where a resident lives or works unless the owner lets
   a spare part, which is income for them; held-for-a-story marks with a week's notice and the rest returned;
   player-run nights; performers give the company bonus.
6. Tavern games: Knucklebones, Wolves and Deer, Liar's Bones (capped at 5p, a penny to the house); resident opponents;
   talk counts toward scenes; practice; tournaments (Part 10, decision 32).

New placeholder choices in this plan:
7. Only other players count as company; residents don't.
8. The common room is a partial rest: no full rest without a bed.
9. Rested grants (inns ×1.5 for time away; festivals 15 an hour) beside doc 44's pool.
10. Board sources as a small interface other plans plug into; the board is sent only when read.
11. Residents answer only a notice's structured *what*, never its text, and no notice reaches a prompt.
12. Player stalls stay face to face: goods stay in the purse, and sales need the keeper at the stall.
13. Individuals rent beds (inns, lodgers), the upstairs for a night, and listed places; inn rent goes to the till.
14. Festival prizes are the entrants' pot until the orchestrator gives a festival channel.
15. "A penny a round" in Liar's Bones is a penny a game.

## Open questions

All answered by the user, 2026-10-08:
1. **Beds:** yes. A full rest needs a bed the wolf has a right to: its lodging, a paid inn bed, its Chapter's place, or
   a camp. Others' beds give a partial rest only.
2. **Resident stakes:** yes, within the caps (a purse of 30p or more, 3 staked games a game day, a twentieth of the
   purse at most).
3. **Festival purses:** towns add to the pot from their own purses, beside the entrants' pennies.
