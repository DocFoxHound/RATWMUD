# 54. Gathering places

Drafted 2026-10-06 as an actionable plan for doc 48 (Part 5 except §5.3's training grounds, and Part 10). Nothing
built. Read doc 48 (Part 5, Part 10 and the Decisions) and docs 38 (rest), 44 (rested time), 42 (town purses, wages),
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

1. **Beds anywhere:** today any bed tile gives a full rest, a resident's own included. Should a full rest need a bed the
   wolf has a right to (its lodging, a paid inn bed, a camp), so that inns matter more?
2. **Resident stakes:** should residents gamble with players at all, or should stakes be between players only?
3. **Festival purses:** are the entrants' pots enough, or should towns fund prizes now (from their purses through the
   orchestrator, doc 46)?
