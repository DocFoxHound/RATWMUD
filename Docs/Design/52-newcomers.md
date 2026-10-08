# 52. Newcomers

Drafted 2026-10-06 as an actionable plan for doc 48 (Part 7, §7.1–7.6, and §3.9's vouching). All five phases built
2026-10-07 (see "Built"). Read doc 48
(Principles, Part 7, §3.9, Plans and Decisions), doc 50 (accounts, known wolves, block) and docs 32 (§1.5 names and
introductions), 26 (the Mind, bonds, residents' jobs, crime), 23, 24 and 25 (the three start towns), 28 and 31 first.

## The ask

The user's words, from doc 48:

> "Let's prioritize Upper Accord for the default starting location."

> "The ties should be connected to other 'mentor' wolves nearby that aren't presently tasked out by another tie, but
> those should be timed so that mentors don't always get stuck with them. The tie's should be any number of story
> starters, and I think the suggested one should be randomized for every character. New accounts MUST have a tie.
> For additional characters it will be optional."

> "Now to your notes about what to add: using NPC's to matchmake is a fantastic idea. I love it. Note this
> specifically."

Agreed on 2026-10-06 (doc 48 decisions 15, 16, 18 and 20): Upper Accord as the default start unless Ser Ferro or
Ridgemere is busier, later characters choosing any of the three; a newcomer flag; mentors from social level 5; ties at
creation, mentors first and then residents, timed, randomised, required for a new account's first character;
**residents as matchmakers**.

## Where we stand (read from the code 2026-10-06)

| Area | State | What exists |
|---|---|---|
| Where new characters start | One spawn | `character_create` in `Game::accountCommand` calls `World::addPlayer`, which puts every new character at the world's one authored spawn (`World::spawnCell_`, `spawnPosition_`, read from the `spawn` record by `Core/RatwAuthoring.cpp`; `world.worlds.spawn_area/x/y`). In DEV it is Upper Accord's plaza (`tools/worldgen/upper_accord.py`, doc 23). The spawn also decides the **capital**: `World` makes the town holding the spawn the one whose store is the treasury (`Core/RatwRoads.cpp`). So the spawn must not move. |
| Towns | Built | `World::towns()` and `World::townOf(cell)` (a cell's town is its region; `Town` has the market merchant `market` and where they stand, `marketX/marketY`). Ridgemere and Ser Ferro are cities in their own cells (docs 24, 25). |
| The newcomer flag | Missing | Accounts have no played time and no flag (doc 50 Phase 1 adds played time and `Game::accountLevel`). Social level is per character (`SocialLedger::level`). |
| Mentors | Missing | Nothing. Doc 50 adds experience (Newcomer Guide is self-declared) and upheld reports. |
| Ties | Missing | The creator (`Client/src/ui/frontDoor.ts`: body, coat, eyes, Gift, name tabs) has no tie step. |
| Residents' jobs | Built | `Society::jobOf` (a `Position` with title and role), apprentices (`PositionState::apprentice`, `Society::apprentice` lets a player learn a trade), and `scenes::jobCategory(title, label, role, age)` (`Core/RatwScenes.h`), which already sorts residents into innkeeper, priest, merchant, guard, official and so on. **No world has a "market warden":** the nearest are the town's market merchant (`Town::market`) and its officials. |
| The Mind's briefing | Built | `Game::dialogueContext` builds the context for each reply; facts are appended to `context.activity` (`companionContext`, `factionContext`, `swornContext`). Residents learn and use names only by doc 32's rules (`Game::knowsName`, `Game::willName`, `Game::learnName`); `Game::veilFor` replaces names a viewer doesn't know with labels. The speech router (`Core/RatwVoice.*`, `Data/Voice/router.json`) answers simple requests without a model, with a normaliser this plan reuses. |
| Introductions | Built | Introductions in speech and the Introduce button (doc 32 Phase 2). Hearsay names (one wolf naming a third) are left out on purpose. |
| Bonds and crime | Built | `Bonds` (`change`, `mutual`, `addOwed`); crime incidents with offender, victim, town and witnesses (`Core/RatwCrime.h` `Incident`), which companions already follow with a cursor of seen incidents (`Core/RatwGameCompanions.cpp`). |
| The DM app | Built | Players tab (`Editor/src/dm/DmApp.tsx`); actions through `dm.actions` (`tools/dungeon_master.py` `ACTIONS`). |

## Scope

This plan builds:

- where a new character arrives: Upper Accord, or Ser Ferro or Ridgemere when busier, and a choice for later
  characters;
- the **newcomer flag**, account-based;
- **mentors**: opt-in, gated, available or busy, recognised;
- **ties** at creation: a story starter linking the new wolf to a mentor or a resident;
- **residents as matchmakers**;
- **innkeeper introductions** on a newcomer's first evening;
- **vouching**.

Left to other plans:

- Accounts, played time, the account's social level, known wolves, status, experience, block and reports: doc 50.
- The Welcoming star tag and stars on mentors' cards: doc 51 (it reads this plan's newcomer flag).
- Personal storylines that grow from a tie: doc 58 (§8.2).
- A letter carried for the tie partner as a real item: doc 55.
- Paying off a resident's debt as a lever: doc 57 (§1.3).
- Social level moving to the account: doc 49.

## Design

### 1. Where new characters start (agreed, doc 48 §7.1)

- **Three start towns**, named in `Data/Social/newcomers.json` by town id, Upper Accord first. Towns the world doesn't
  have are dropped with a warning, so test worlds with one town start at the spawn as today.
- **Where in each town a wolf arrives:** the world's spawn for the town that holds it (Upper Accord's plaza); otherwise
  the town's market stand (`Town::marketX/marketY`, in the market merchant's work place); otherwise, or when that spot
  isn't open ground, a cell and tile given for the town in the data file. The spawn itself never moves, so the capital
  stays where it is.
- **Active wolves nearby** (agreed placeholder: 30 minutes): once a minute, the game counts connected players with
  meaningful input in the last 5 minutes (`Game::operatorActivity_`) whose cell lies in each start town
  (`World::townOf`). Each town keeps its last 30 counts; its "active wolves" is their mean. One busy evening's spike moves
  the mean a little; a steady crowd moves it a lot.
- **A new account's first character** arrives in Upper Accord unless Ser Ferro or Ridgemere has a higher mean. Then it
  arrives in whichever of the three has the highest. A tie between them goes to Upper Accord, then the order in the
  data file.
- **The first character may choose too** (the user, 2026-10-07): the busiest town is preselected with the reason,
  and the player may pick either of the others, so no one is split from a friend on their first day.
- **Later characters** choose any of the three in the creator. Each town shows a line about it and "lately: about N
  wolves about" from its mean.
- The choice is made at creation (`character_create` gains `start`, part of the request's fingerprint), and the new
  character is placed there before it is saved (`World::addPlayer` takes an optional place).

### 2. The newcomer flag (agreed, doc 48 §7.2)

- **Account-based:** an account is a newcomer until it has 15 hours played or social level 3, whichever comes first
  *(agreed placeholders)*. Played time and `Game::accountLevel` come from doc 50. Once it ends it never comes back (a
  `graduated` mark on the account), even if thresholds change.
- **A veteran's new character is not a newcomer.**
- **Shown** as a small mark by the label (entity field `nc`) and on the card: "new to these parts".
- **Residents are told.** `Game::dialogueContext` adds: "This wolf is new to these parts. Be patient with them; if it
  fits, tell them where the inn and the notice board are." Nothing else changes in the rules.
- Doc 51's Welcoming tag reads `Game::isNewcomer(account)`.
- Doc 50's self-declared experience *Newcomer* is separate: a veteran may call themself new to roleplay.

### 3. Mentors (agreed: social level 5, doc 48 §7.3)

- **Who may opt in:** an account at social level 5 *(agreed placeholder)*, with no upheld report in the last 30 days
  (doc 50's `Game::upheldReports`; *placeholder*, proposed in doc 48), not a newcomer and not silenced.
- **Opting in** (People panel, "Mentor newcomers") sets the account's experience to Newcomer Guide (doc 50) and turns on
  the mentor mark. A mentor sets themself **available** or **busy**.
- **Where it shows:** the mentor mark on the card and the words "mentor" in In Sight. **Newcomers** also see available
  mentors marked by their label on the map *(placeholder)*, so the wolves who most need to find a mentor can, without
  adding a mark for everyone else.
- **Recognition only** (agreed): a "Mentor" line under the social title, and "guided 12 newcomers" on the card. A tie
  counts once it ends with at least one shared scene *(placeholder)*. Welcoming stars come from doc 51. No XP beyond what
  scenes already pay.
- **Losing it:** an upheld report turns mentoring off at once, and the mentor is told why. The DM can revoke and restore
  it (`mentor.revoke`, `mentor.restore`).
- Mentor fields live on doc 50's account record: opted in, available, resting until, current tie, guided count, last
  tie at.

### 4. Ties (agreed, doc 48 §7.4)

**At creation.** The creator gains a TIE step:

- One **story starter** is suggested at random (chosen by the page, checked by the server), with **Reroll** and the whole
  list to **pick** from.
- A new account's **first character must take one** (agreed). Later characters may choose **No tie**.
- `character_create` gains `tie` (a starter id or empty), in the fingerprint.

**Story starters** live in `Data/Social/ties.json`, so the list grows without code (doc 48's eight to begin with). Each
has:

- the newcomer's line ("They pulled you out of the river on the road here.") and the other side's ("You pulled a wolf
  out of the river on the road here; they've just arrived.");
- whether a mentor can take it (most can);
- what a **resident** must be to fit it (job categories from `scenes::jobCategory`, an age range, a need such as an
  apprentice place free (`PositionState::apprentice` empty) or a purse, or family);
- what it does with a resident: a bond to start from (familiarity, liking, trust), a small debt for "You owe them a
  small debt" (`Bonds::addOwed`, 4 pennies *(placeholder)*: a record of what is owed, no money moves), and whether the
  two learn each other's names (family starters only, `how: "tie"` *(placeholder)*: cousins would know each other's
  names; others meet as strangers and introduce themselves).

**Who the tie connects to.** When the character is created, the tie starts **seeking**:

1. **Mentors first** (agreed). Candidates: mentors who are online, available, eligible, in the start town (or at most
   two cells away by `World::routeBetween`), holding no tie, not resting, not Out of character, not on the newcomer's
   account, and not blocked either way (doc 50's `Game::blocked`). The one longest without a tie is asked first; ties
   are broken at random.
2. **The mentor agrees first** (agreed). They get an offer: the newcomer's look (the stranger label built from the
   appearance), the starter from their side, and the start town, with **Accept** and **Pass** and 3 minutes *(agreed
   placeholder)*. A pass or silence asks the next mentor.
3. **Then a resident** (agreed): one in the start town who fits the starter, awake and not travelling with a party,
   chosen at random among those that fit. If none fits, the starter falls back to "They've been asked to show you
   around", with the innkeeper nearest the arrival point.

**When the tie is made:**

- Both are told the starter and where to find each other: "Your tie: they pulled you out of the river on the road here.
  Look for a grey wolf with a torn ear near the fountain." A faint marker shows that spot on the map for 20 minutes
  *(placeholder)*: where they were when the tie was made, not where they go after, so no one is tracked.
- A newcomer still in the creator, or offline, is told on entering.
- The tie goes into both wolves' **known wolves** (doc 50) with the starter as its note. With a resident, the newcomer's
  entry is the resident's, and the resident's bond and briefing carry it ("You and this wolf share a tie: you've been
  asked to show them around.").

**Ties are timed** (agreed):

- A mentor holds **one tie at a time**.
- A tie **lapses** after 7 real days or once the two have shared 3 scenes *(agreed placeholders)*, whichever comes first.
  Shared scenes are counted from doc 50's known-wolves count since the tie began.
- The mentor then **rests a day** *(agreed placeholder)* before the next offer.
- The relationship stays: only the tie's claim on the mentor ends.
- A newcomer may end a tie early; the mentor doesn't rest then. A mentor may release one; the newcomer is offered a
  resident instead, and the mentor rests the day *(placeholders)*.

### 5. Residents as matchmakers (agreed: the user's favourite, doc 48 §7.5)

**This is the user's favourite idea, noted specifically.** A resident who knows two wolves points them at each other
when something links them. The server decides who and why; the Mind only says it, in the resident's voice.

**Who matchmakes:** innkeepers and priests (`scenes::jobCategory` gives `innkeeper` and `priest`), and each town's market
merchant (`Town::market`) in place of the "market wardens" doc 48 names, which no world has *(placeholder)*. The list of
categories is in `Data/Social/matchmaking.json`.

**When:** while a player (A) talks to a matchmaker. As `Game::dialogueContext` builds the reply's context, it may add one
pairing. At most once a game hour for A *(agreed placeholder)*, and at most 6 pairings a game hour for the matchmaker
*(placeholder)*.

**Whom it may point A at:**

- **Another player (B)** in the same town now (the town roster below), whom the resident knows: a bond from the
  resident to B with familiarity 10 or more *(placeholder)*, so it has dealt with B and isn't inventing an acquaintance.
- **A resident (C)** the matchmaker knows (a bond as above), for reason 4 below.
- **Never** (agreed): a wolf who is Out of character (doc 50's status), has matchmaking off, or is blocked either way
  (`Game::blocked`). Also never A's party mates, a wolf A already knows well (2 shared scenes or more *(placeholder)*), or
  a wolf pointed at 3 times this game hour already *(placeholder)*.

**The reasons** (doc 48's, proposed; the order is a placeholder, the first that fits wins):

1. **A newcomer and a helper:** A is a newcomer (§2) and B is an available mentor or a Newcomer Guide; or the reverse.
2. **A tie not yet met:** A and B are tied (§4) and haven't shared a scene.
3. **Both looking:** A and B are both *Looking for a scene*.
4. **A need and who meets it:** what A just said matches an ask in `matchmaking.json` ("looking for work", "need a
   teacher", "who can mend this", "anyone to hunt with"), matched with the speech router's normaliser. A resident C meets
   it when its position fits (an opening, an apprentice place free, the trade asked for); a player B when their status
   or profile says so now (Looking for a scene for company), and by their specialty once doc 49 gives players one.
5. **Shared roots:** both arrived in the same start town in the last 7 days *(placeholder)*.
6. **Two angles:** a contract or town project that needs two kinds of wolf (docs 53 and 57), once those exist.

**How it is said:**

- The briefing names the other wolf **as A knows them**: by name only if A already knows it (`Game::knowsName`),
  otherwise by their look. It says where they are in words ("by the fire", "often at the Wharf"), and the reason in
  words ("she came up from Ser Ferro last week and is glad to show newcomers about"). It tells the Mind to use no other
  name for them.
- **The server checks the reply.** Any registered name of the other wolf (`Game::namesOf`) that A doesn't know is
  replaced by their label before it is spoken, using the veiling `Game::veilFor` already does. So the resident never
  gives a name A shouldn't know (doc 32 §1.5), whatever the model writes.
- If the reply comes from the speech router or a written line (no model, or over budget), the server adds a written
  matchmaking line after it from `matchmaking.json` ("Try {who}, {where}. {reason}").
- B is told only what B could see: if B can see the resident, a quiet line, "The innkeeper nods your way while talking
  with a dun wolf." *(placeholder)*.

**The setting:** "Residents may point others to me, and me to others", per character, on by default *(placeholder)*, in
Settings. Off means neither direction.

**The town roster:** once a minute (the same pass as §1's counts), the game lists the active players in each town. The
matchmaker reads its own town's list and checks the chosen wolf is still there. No per-tick work.

### 6. Welcoming arrivals: the innkeeper's introductions (doc 48 §7.6)

- **When:** on a newcomer's first evening (game hours 17–23 *(placeholder)*), when one of their characters is in a place
  where an innkeeper is at work and awake, with at least one other player there who is not Out of character, not blocked
  either way and has matchmaking on. Once a character; if no one else is there, the next evening tries again, three
  evenings at most *(placeholder)*.
- **What the innkeeper says** (written lines from `matchmaking.json`, optionally polished by the light model, doc 28):
  a welcome to the newcomer, said aloud so the room hears it, then a line for each of up to 4 wolves present that the
  innkeeper knows, **by their look** and what the innkeeper knows of them: a regular or not (its familiarity), Looking for
  a scene ("looking for company tonight"), Newcomer Guide ("always glad to show a newcomer about").
- **Names stay with their owners.** Doc 48 §7.6 says "by the names it knows"; that would be hearsay, which §7.5 and doc
  32 §1.5 rule out. So the innkeeper points wolves out by their look, and each wolf pointed out gets a prompt with their
  Introduce button ("The innkeeper is pointing you out to a newcomer. Introduce yourself?"). The newcomer gets the same
  prompt. The names then come from the wolves themselves, as doc 32 intends.

### 7. Vouching (doc 48 §3.9)

A wolf with a good bond to a resident introduces a friend: "She's with me."

- **How:** from a resident's menu, "Vouch for…", choosing a wolf in earshot of both. The voucher says a stock line aloud
  ("They're with me. I'll vouch for them."), so it happens in the world and the resident hears it.
- **Who may vouch:** the resident's trust in the voucher at least 30 and liking at least 20 *(placeholders)*. The one
  vouched for is a player whom the resident doesn't already distrust (trust above −20), not blocked either way with the
  voucher. One vouch per resident and wolf; at most 5 a voucher at a time *(placeholders)*. Mentors are the natural
  vouchers, but anyone may.
- **A share of the voucher's trust carries over** (doc 48): the resident's trust in the newcomer rises by 30% of its
  trust in the voucher, at most 15; liking by 20% of its liking, at most 10; familiarity by 10 *(placeholders)*. The
  resident's briefing says who vouched for them.
- **The household gets half** (the user, 2026-10-07): each other member of the resident's household gets half of what
  the resident got (trust, liking and familiarity), and loses it again if the vouch goes bad.
- **The voucher's bond takes the hit** (doc 48): if, while the vouch lasts (30 game days *(placeholder)*), the one
  vouched for robs or assaults that resident, is named by it as a witness to a crime, or is charged in its town (doc 26
  Phase 7's incidents and warrants), the resident's trust in the voucher falls by twice what it gave and its liking by
  5, the vouched trust is taken back on top of the crime's own effect, and the vouch ends. The voucher is told when
  online, and the resident brings it up next time they talk (its briefing says so).
- Records: `Vouch` (resident, voucher, vouched, when, until, trust given), followed with a cursor over new incidents as
  companions already do. IDs and amounts only, never prose.

### 8. Data

| Where | What |
|---|---|
| `Data/Social/newcomers.json` | Start towns (id, name, line, optional arrival cell and tile), the newcomer thresholds, the sampling window, mentor rules (level, report days, offer seconds, tie days, scenes, rest), vouching numbers. |
| `Data/Social/ties.json` | The story starters (§4). |
| `Data/Social/matchmaking.json` | Matchmaker categories, asks and their patterns, reasons' words, written lines for matchmaking and the innkeeper, limits. |
| `game.account_profiles` (doc 50) | Gains: graduated, mentor (opted in, available, resting until, current tie, guided, last tie at), first evening done. |
| `game.ties` (new, through `game.sections`) | id, newcomer character and account, other (mentor character or resident), starter, state (seeking, offered, active, lapsed, ended), offered to and until, mentors asked, made, lapses at, scenes at start. |
| `game.vouches` (new, through `game.sections`) | id, resident, voucher, vouched, at, until, trust given, state. |

The town counts, town rosters and matchmaking timers are kept in memory only; a restart starts them again.

### 9. Wire

- `character_create` gains `start` and `tie`. The lobby (`Game::lobby`) gains `starts` (each town's name, line and
  recent mean), `ties` (the starter list) and `tieRequired` (true for a new account's first character).
- `{"type":"mentor","verb":"optin"|"optout"|"available"|"busy"|"accept"|"pass"|"release"}`; the offer arrives as
  `{"type":"tieOffer","tie":id,"look":"…","starter":"…","town":"…","seconds":180}`.
- `{"type":"tie","verb":"end"}`; the snapshot's `self.tie` (starter, the other as known, state, lapses, the marker while it
  lasts).
- `{"type":"vouch","resident":id,"for":id}`.
- Entities gain `nc` (a newcomer) and, for newcomers' views only, `mentor` (an available mentor).
- The card (`inspect`'s `profile`, doc 50) gains `newcomer`, `mentor` and `guided`.
- Settings: `{"type":"profile","verb":"settings","matchmaking":bool}` (doc 50's settings verb).

### 10. Client

- **Creator** (`Client/src/ui/frontDoor.ts`): an ARRIVAL tab for later characters (three towns, their lines and
  counts); for a first character, "You'll arrive in Upper Accord" (or the busier town) with the reason. A TIE tab: the
  suggestion, Reroll, the list, and No tie when allowed; Create stays disabled without a tie when one is required.
- **Mentor offer:** a toast with the look, the starter, the town, Accept and Pass and the seconds counting down
  (`Client/src/ui/hud/hud.ts`).
- **People panel** (doc 50's `Client/src/ui/hud/people.ts`): Mentor newcomers, Available or Busy, the current tie,
  guided count; for a newcomer, their tie and End tie.
- **Marks:** the newcomer mark by labels and the mentor mark for newcomers (`Client/src/game/paint.ts`, In Sight in
  `hud.ts`); the tie marker on the map and minimap (`Client/src/game/minimap.ts`).
- **Introduce prompts:** a toast with the Introduce button when an innkeeper points you out.
- **Vouch for…** in a resident's menu.

### 11. The DM app

- **Players tab** (`Editor/src/dm/DmApp.tsx`): newcomer or not, hours played, mentor (available, busy, resting), current
  tie, guided count; a Ties list (state, starter, partners, made, lapses); the three towns' recent means.
- **Actions** (`tools/dungeon_master.py` `ACTIONS`, role `dm`): `mentor.revoke`, `mentor.restore`, `tie.end`, applied by
  the game's `dm.actions` reader and audited.
- Numbers stay in the data files; the DM doesn't tune them live.

### 12. Cost at 1,000 players

- One pass a minute over connected players: the town counts, town rosters and played time (doc 50) together.
- Tie offers: a short queue checked once a second, holding only ties seeking a mentor.
- Lapses: checked when a scene settles (for the two in it) and once a game day over active ties.
- Matchmaking: only when a player talks to a matchmaker, at most once a game hour each, reading one town's roster (tens
  of players).
- Innkeeper introductions: checked when a newcomer enters a place, and only on their first three evenings.
- Vouches: a cursor over new crime incidents, looked up by offender.
- Nothing scans the world in the tick. Gate for every phase: `world_check --players 20` unchanged within noise.

## Phases

### Phase 1: the newcomer flag, and where new wolves arrive

- **Goal:** new accounts are marked as newcomers until 15 hours or social level 3; their first character arrives in the
  busiest of the three towns, Upper Accord by default; later characters choose.
- **Changes:**
  - Server: `Core/RatwNewcomers.h` and `.cpp` (new, pure, `ratw::newcomers`): the rolling town means, `chooseStart`,
    `isNewcomer`, the data file's reader. `Core/RatwGameNewcomers.cpp` (new): `Game::tendNewcomers(dt)` (the minute's
    pass), `Game::isNewcomer(account)`, the arrival point, the newcomer line in `Game::dialogueContext`.
    `character_create` reads `start`; `World::addPlayer` takes an optional place. `Game::lobby` sends `starts`.
    `Game::sendSnapshot` adds `nc`; doc 50's `Game::cardFor` adds `newcomer`.
  - Data: `Data/Social/newcomers.json`; `graduated` on doc 50's account record.
  - Client: the ARRIVAL tab and first-character line; the newcomer mark.
  - DM app: newcomer and hours in the Players tab; the towns' means.
- **Tests:** `Tests/newcomer_tests.cpp` (new): equal counts go to Upper Accord; a steady crowd in Ser Ferro wins; one
  minute's spike doesn't; a world with one town uses the spawn; the spawn and the capital don't move; the flag ends at
  15 hours or level 3 and stays ended; a veteran's new character isn't one; the Mind's context carries the line.
  Through the game: an account registered and its first character placed in the chosen town; a later character placed
  where it chose; a forged `start` refused. `Tests/checkpoint_tests.cpp` (graduated survives a restart). In a real page,
  `tools/client/newcomer.mjs` (new): register, create a first character, enter in Upper Accord with the mark; create a
  second and choose Ser Ferro.
- **Done when:** new accounts arrive where the rule says and carry the mark until they pass the thresholds.
- **Cost:** the minute's pass over connected players; a few bytes per entity.

### Phase 2: mentors

- **Goal:** eligible accounts can opt in as mentors, be available or busy, and be recognised.
- **Changes:**
  - Server: `Core/RatwNewcomers.*`: `mayMentor`. `Core/RatwGameNewcomers.cpp`: `mentorCommand`, the daily and
    report-time checks (an upheld report turns mentoring off), the mentor fields on the account, `mentor` for
    newcomers' views, `mentor` and `guided` on the card. `mentor.revoke` and `mentor.restore` in the `dm.actions` reader.
  - Client: the People panel's mentor controls; marks.
  - DM app: mentor columns and the two actions.
- **Tests:** `Tests/newcomer_tests.cpp`: level 4 can't opt in, level 5 can; an upheld report in 30 days stops it and an
  older one doesn't; opting in sets Newcomer Guide; only newcomers see the map mark; the DM's revoke holds through a
  restart. `Client/src/game/people.test.ts`.
- **Done when:** a mentor can opt in, show available or busy, and appears as such to newcomers and on cards.
- **Cost:** none in the tick; checks at opt-in, at reports and once a day.

### Phase 3: ties

- **Goal:** every new account's first character takes a tie to a mentor or a resident; later characters may.
- **Changes:**
  - Server: `Core/RatwNewcomers.*`: starters, the tie states, mentor order, resident fit, lapse rules.
    `Core/RatwGameNewcomers.cpp`: seeking at creation, the offer queue (`Game::tendTies`, once a second over seeking
    ties), Accept, Pass and time-outs, the resident fallback (bond seeds, `Bonds::addOwed`, `Game::learnName` for family
    starters), telling both, the marker, doc 50's `Game::meet(..., "tie")` with the starter as the note, lapses from doc
    50's shared-scene count, the rest day, End tie and release. The resident's briefing line. `character_create` reads
    `tie`; the lobby sends `ties` and `tieRequired`.
  - Data: `Data/Social/ties.json`; `game.ties` (sections) in a migration.
  - Client: the TIE tab; the offer toast; the tie in the People panel; the marker.
  - DM app: the Ties list and `tie.end`.
- **Tests:** `Tests/newcomer_tests.cpp`: a first character can't be made without a tie, a second can; the first mentor
  asked is the one longest without a tie; a pass and a time-out move to the next; a mentor in another town, resting,
  holding a tie, Out of character, on the same account or blocked is never asked; no mentor gives a fitting resident
  (the miller for the debt, a master with a free apprentice place for the apprentice starter); none fitting gives "show
  you around" with the innkeeper; both get known-wolf entries with the starter; 3 shared scenes or 7 days lapse it and
  the mentor rests a day; ties survive a restart. In a real page, `tools/client/newcomer.mjs` gains two browsers: Bo, a
  mentor, accepts the offer; Ash enters and is told where to find him.
- **Done when:** a new player arrives with a tie and someone to look for, and mentors are never held by more than one
  tie or asked while resting.
- **Cost:** the queue holds only seeking ties; the daily lapse check runs over active ties only.

### Phase 4: residents as matchmakers

- **Goal:** innkeepers, priests and market merchants point wolves at each other, by look or a known name, for a reason
  the server chose.
- **Changes:**
  - Server: `Core/RatwNewcomers.*`: the reasons and their order, `pickPairing` from the town roster and the limits.
    `Core/RatwGameNewcomers.cpp`: `Game::matchmakingContext(npc, player)` (added in `Game::dialogueContext`), the
    asks matched with the speech router's normaliser, the reply check that veils the other wolf's names, the written
    line after a router or written reply, B's quiet line, the setting. Nothing in `Core/RatwVoice.*` changes but a
    shared call to its normaliser.
  - Data: `Data/Social/matchmaking.json`.
  - Client: the setting in Settings (`Client/src/ui/hud/dialogs.ts`).
  - DM app: none needed; matchmakings are logged as events (IDs and reason only) for the DM's LIVE feed.
- **Tests:** `Tests/newcomer_tests.cpp`: a newcomer talking to the innkeeper is pointed at an available mentor in town,
  not at one Out of character, opted out, blocked, in their party or already well known; once a game hour; the
  matchmaker's 6 an hour; a reply naming the mentor to a newcomer who doesn't know the name comes out with the label;
  the written line without the Mind; "looking for work" points at a resident with an opening. `tools/test_npc_mind.py`
  is unchanged (the briefing is ordinary context). In a real page, `tools/client/newcomer.mjs` gains the innkeeper's
  pointer with the Mind off.
- **Done when:** a newcomer asking the innkeeper about anything may hear of a real wolf nearby who fits, by look, and
  never a name they haven't been given.
- **Cost:** on a matchmaker's reply only, at most once a game hour a player, reading one town's roster.

### Phase 5: innkeeper introductions and vouching

- **Goal:** a newcomer's first evening at an inn brings introductions; any wolf in good standing can vouch for another.
- **Changes:**
  - Server: `Core/RatwGameNewcomers.cpp`: the first-evening check on entering a place, the innkeeper's lines, the
    Introduce prompts; `vouchCommand`, the trust share, the incident cursor and the hit, the briefing line.
  - Data: the innkeeper's lines in `matchmaking.json`; vouching numbers in `newcomers.json`; `game.vouches`
    (sections).
  - Client: Introduce prompts; Vouch for… in a resident's menu.
- **Tests:** `Tests/newcomer_tests.cpp`: a newcomer entering the common room at evening with two other players hears
  the welcome and two lines by look, and both get prompts; no line for a wolf Out of character or blocked; once a
  character, three evenings at most; a vouch moves the resident's trust by the share; a theft from that resident by the
  one vouched for cuts the voucher's trust and ends it; a crime elsewhere doesn't; one vouch per pair; 5 at once. In a
  real page, the newcomer script gains the evening at the inn.
- **Done when:** a newcomer is welcomed and pointed to the wolves in the room, who can introduce themselves; vouching
  carries trust and its risk.
- **Cost:** checks on entering a place for newcomers' first evenings only; vouches follow new incidents only.

## Built

### Phase 1 (2026-10-07): the newcomer flag, and where new wolves arrive

- **The rules:**
  - `Core/RatwNewcomers.h/.cpp` (`ratw::newcomers`, pure) holds the rules, `Counts` (each start town's last 30 counts
    and their mean), `busiest` (a tie goes to the earlier town in the data file) and `graduates`.
  - `Data/Social/newcomers.json` holds the three towns (Upper Accord first), each with a line for the creator; new
    until 15 hours or social level 3; a count every 60 s, a mean over 30, active within 300 s.
  - In DEV the world's towns are `upper_accord`, `ser_ferro` and `ridgemere`, matching the data file. Cinderbrook is
    a town of its own, so Ser Ferro's count doesn't include it (the Risk noted below).
- **The game (`Core/RatwGameNewcomers.cpp`):**
  - `Game::tendNewcomers` runs once a minute from the tick, next to played time. It counts connected players at the
    keys in each start town (`World::townOf`), in memory only, and marks an account `graduated` once it passes either
    threshold (saved on the account record, `game.account_profiles`; never undone).
  - `Game::isNewcomer(account)` is true while the account isn't graduated and has under 15 hours. An account with
    nothing recorded yet counts as new.
  - `startTowns()` gives the data file's towns this world has. A world of one settlement has none, so everyone starts
    at its spawn as before.
- **Arriving:**
  - `character_create` takes `start`. A town this world doesn't offer is refused; with no `start`, the busiest town is
    used. `start` goes into the request's fingerprint only when named, so older clients' prints are unchanged.
  - `World::arrivalIn` places the wolf: the spawn for the town that holds it; else the first open tile beside the
    market merchant's stall; else the data file's cell and tile; else the spawn.
  - `World::addPlayer` gains a placed form. The spawn and the capital never move.
  - The lobby sends `starts` (name, line, recent wolves, `suggested`) and `firstCharacter`.
- **Shown:**
  - Entities carry `nc`, so In Sight shows a ✧ after the name ("New to these parts" on hover), and the look line says
    "new to these parts".
  - The card (`inspect`'s profile) has `newcomer`, shown as "new to these parts".
  - Residents' briefing adds: "This wolf is new to these parts. Be patient with them; if it fits, tell them where the
    inn and the notice board are."
  - **The creator** has an Arrival tab: three cards, each with its line and "Lately: about N wolves about", the busiest
    preselected and marked. A first wolf is told why ("the busiest of the start towns lately… You may choose another,
    say to join a friend": decision 19). The review says "Arrives in: …".
  - **The DM app:** the Players tab has a New column (`person.newcomer` from the account record).
- **The server:** `--world-export DIR` plays a world build exported as files, for tests and for trying a build
  offline.
- **Tests:**
  - `Tests/newcomer_tests.cpp` (423 checks). The rules and means: equal counts go to Upper Accord, a steady crowd in
    Ser Ferro wins, one busy minute doesn't, only the last 30 count. A one-town world: no towns offered, a named one
    refused, arrival at the spawn.
  - On a strip of three towns (as `world.ratw` files): the lobby's towns with Upper Accord preselected; a forged town
    refused; the first wolf at the spawn; another first wolf choosing Ridgemere and placed beside its merchant; the
    mark, the card and the briefing; a crowd in Ser Ferro making it the suggestion for the next account, who arrives
    at its market; the capital unmoved; graduating at social level 3 for good, kept across a restart.
  - `tools/test_dungeon_master.py` covers the New column's rule.
  - `tools/client/newcomer.mjs` runs on its own server on the same strip, with screenshots in
    `artifacts/screenshots/newcomers/`: the Arrival tab for a first wolf, choosing Ser Ferro, and the newcomer's ✧
    and card.
- **Cost:** a once-a-minute pass over connected players; two map lookups per player in view.
- **Not built (later):** the towns' recent means in the DM app. They live in memory only, so the DM host can't read
  them yet; the watch frame could carry them when the LIVE tab wants them.

### Phase 2 (2026-10-07): mentors

- **Who may mentor:** `newcomers::mayMentor` (pure) refuses, with the reason in words, an account that is:
  - revoked by a Dungeon Master;
  - silenced;
  - itself a newcomer;
  - under social level 5;
  - holding a report upheld within 30 days.

  The numbers are in `newcomers.json`'s `mentors`. The account's social level is its wolves' social XP together
  (`Game::accountSocialLevel`).
- **On the account record** (`AccountRecord::mentor`, saved in `game.account_profiles`): on, available, revoked,
  resting until, the current tie, guided, the last tie's time. The tie and resting fields wait for Phase 3.
- **Commands:**
  - `{"type": "mentor", "verb": "optin" | "optout" | "available" | "busy"}`.
  - Opting in sets the account's experience to Newcomer Guide.
  - The owner's account view (the profile panel) carries `mentor`: on, available, revoked, may, why, guided, and
    resting hours.
- **Turned off:**
  - An upheld report (`decideReport`) turns mentoring off at once; every wolf of the account in the world is told why
    (`Game::tellAccount`).
  - A daily pass turns off anyone who may no longer mentor (silenced, say).
  - `mentor.revoke` and `mentor.restore` (DM actions against a character, applied to its account) turn it off until
    restored, and back. The revoke is saved, so it holds through a restart.
- **Shown:**
  - Entities carry `mentor` for everyone, so In Sight says "mentor".
  - Only in a newcomer's view, `mentorFree` marks a mentor who is available, on no tie and not resting. The local map
    draws a small ✦ over their shoulder, In Sight says "mentor · free", and hovering says "a mentor, free to show you
    around".
  - The card carries `mentor` ("available" or "busy") and `guided`: "Mentor (busy) · guided 3 newcomers".
- **The client:**
  - The profile panel has a MENTORING row: Mentor newcomers (or the reason one may not); once on, Available, Busy and
    Stop mentoring, and "resting after a tie" when it applies.
  - `people.test.ts` covers the marks, the words and the command.
- **The DM app:** a Mentor column (available, busy or revoked, with the number guided), and Revoke mentoring and
  Restore mentoring buttons on a character's account.
- **Tests:** `Tests/newcomer_tests.cpp`, now 629 checks:
  - the rules in each case;
  - through the game, level 4 refused and level 5 accepted (as a Newcomer Guide);
  - only the newcomer sees the free mark, and busy takes it away;
  - the card says busy;
  - an upheld report turns mentoring off, tells her why and bars opting in again;
  - the DM's revoke holds through a restart and restore lifts it.

  A report older than 30 days isn't counted by `upheldReportsWithin`; the pure test covers that case, since the test
  can't age a report.
- **Cost:** nothing in the tick. Checks happen at opt-in, when a report is upheld and once a day; snapshots do one
  more map lookup per player in view.

### Phase 3 (2026-10-07): ties

- **The starters** (`Data/Social/ties.json`): doc 48's eight. For each, the newcomer's line and the other side's;
  whether a mentor may take it; what a resident must be (job categories, ages, a free apprentice place); a bond to
  start from; a debt (the debt starter: 4 pennies, recorded with `Bonds::addOwed`, no money moves); names for the
  cousins only. "Show you around" is the fallback.
  - The timings live there too: an offer stands 180 s; a tie lapses at 7 days or 3 scenes; the mentor rests a day; the
    marker lasts 20 minutes; mentors within 2 cells count as near.
  - The pure side (`Core/RatwNewcomers.*`): `Starter`, `TieRules`, `Tie` (saved and loaded), `mentorOrder` (the one
    longest without a tie first, never-tied first of all, ties broken by a seed), `residentFits` and `lapsed`.
- **At creation:**
  - `character_create` takes `tie` (a starter id, part of the fingerprint). A new account's first wolf is refused
    without one; later wolves may go without.
  - `Options::tiesOptional` lets the other suites' wolves be made without one; `newcomer_tests` exercises the
    requirement.
  - The lobby sends `ties` and `tieRequired`. The tie starts seeking at once, while the player is still on the
    character screen.
- **Seeking (`Game::tendTies`, once a second over open ties):**
  - Each mentor is offered it in turn, the one longest without a tie first. A mentor qualifies when they are:
    - online, available, eligible, holding no tie and not resting;
    - not Out of character, not on the newcomer's account, and not blocked either way;
    - in the start town or within 2 cells of the arrival (`World::routeBetween`).
  - The offer (`tieOffer`: the newcomer's look, the starter from the mentor's side, the town, the seconds) shows under
    the mentor's map with Accept and Pass. A pass or a time-out asks the next mentor.
  - Then a resident in town who fits, awake (one asleep only if none is awake), not following anyone, chosen at random.
  - If no resident fits, the starter becomes "show you around", with the innkeeper nearest the arrival, else the
    nearest grown resident.
- **Made (`Game::makeTie`):**
  - Both are told the starter and where to look ("Look for a dun wolf near Stretch 0. The spot is marked on your map
    for a while."). Anyone still on the character screen or offline is told on entering.
  - The starter goes on both known-wolves lists as the tie note (`KnownWolf::tie`, already in doc 50), even before the
    newcomer enters.
  - With a resident: the starter's bond, debt and names. The resident's briefing says "You and this wolf share a tie: …".
  - The snapshot's `self.tie` carries the starter from that side, the other as known, scenes shared and days left, and
    for the newcomer the marker (where the other was when it was made). The local map draws it as a dashed ring with
    "your tie", and the minimap as a sage ring.
- **Ending:**
  - A tie lapses at its time (checked every second) or at 3 shared scenes (once a minute, from the known-wolves
    count). The mentor's claim ends and they rest a day; a tie that ended with a scene shared counts once in "guided".
  - The newcomer may end it early (`{"type": "tie", "verb": "end"}`), and the mentor doesn't rest.
  - A mentor may release it (`mentor` verb `release`): they rest the day, and the newcomer is given a resident.
  - A Dungeon Master's `tie.end` ends it.
  - Closed ties are let go after 30 days. A mentor can still have its own newcomer's tie while holding one for someone
    else, so the two are indexed apart.
- **Saved:** the people root's `ties`, as `game.ties` through `game.sections` (migration `0042_ties.sql`).
- **The client:**
  - The creator's **Tie** tab: the suggestion (random for every wolf), Reroll, the eight to pick from, and No tie only
    when allowed. The review says "Tie: …".
  - The mentor's offer panel under the map (in sage, not the fight alert's red).
  - YOUR TIE in the profile panel, with End tie (a newcomer) or Release (a mentor), and the map and minimap markers.
  - `people.test.ts` covers the offer.
- **The DM app:** a **Ties** list on the Players tab (state, starter, the two, made, lapses; a click selects the
  newcomer) and **End tie** on a character.
- **Development:** `{"type": "socialLevel", "level": N}` (only with `--dev-tools`) sets one's account to a social level,
  to try mentoring offline.
- **A newcomer's level:** an account past social level 3 stops showing as new at once (`isNewcomer` checks the level
  itself), not only at the minute's count.
- **Tests:**
  - `Tests/newcomer_tests.cpp`, now 1,236 checks. The rules: the eight starters, the miller (a farmer) for the debt, a
    master with a free place, the order, the lapse rules, a tie saved and read back. On the strip, with four mentors:
    - the first wolf refused without a tie;
    - an offer to one of the two in town, with its look, starter, town and seconds;
    - a pass and a time-out each move on, then the fisher for the river (bond and known-wolf note, told on entering,
      the snapshot's marker, the briefing);
    - an accepted tie on both lists, and a mentor holding a tie never asked;
    - the one longest without a tie asked first;
    - a lapse resting the mentor, so the other is asked;
    - the far and Out of character mentors never asked, though Ridgemere's own mentor is asked for a Ridgemere newcomer;
    - Ser Ferro's innkeeper showing one around, a Ridgemere master taking an apprentice, a debt of 4 to someone with a
      trade;
    - ties kept across a restart.
  - Ties are timed by the real clock, so the test shortens the offer and the lapse (`Options::tieOfferSeconds` and
    `tieLapseSeconds`) and waits a few real seconds.
  - Not exercised through the game: a mentor on the newcomer's own account (a new account can't be a mentor), and a
    blocked mentor; both are single checks in `offerTie`. The 3-scene lapse is covered by the pure test.
  - `tools/client/newcomer.mjs` gains Bo, a mentor, who is offered Ash's tie under his map and accepts; Ash is told
    where to find him, with the marker. Screenshots 5–7: the Tie tab, the offer, and Ash told.
- **Cost:** offers look only at open ties, once a second; lapses compare a time each second and count scenes once a
  minute.
- **Applied** 2026-10-07: migration 0042, on DEV and PROD.

### Phase 4 (2026-10-07): residents as matchmakers

- **The rules** (`Data/Social/matchmaking.json`):
  - matchmakers are innkeepers and priests, and each town's market merchant (the one at the market's stall);
  - a matchmaker knows a wolf at familiarity 10;
  - "known well" is 2 shared scenes;
  - the limits: once a game hour a player, six an hour a matchmaker, three times an hour pointed at;
  - "the same start town" means within 7 days;
  - the reasons in order: a newcomer and a helper, a tie not yet met, both looking, a need and who meets it, the same
    start town;
  - the asks: work, a teacher, mending, company;
  - the words: the briefing, the written line ("Try {who}, {where}: {reason}.") and the quiet nod.

  The pure side is in `Core/RatwNewcomers.*`: `MatchRules`, `askIn` (on the speech router's normalised words,
  `voice::Rules::normalise`), `pickPairing` (the first reason someone fits, one of those by a seed) and `fill`.
- **Who may be pointed at** (`Game::matchmake`, called from `Game::talk` for an identified speaker): players in the
  town's roster (the active players at the minute's count, kept by `tendNewcomers`, and still in town) whom the
  matchmaker knows, and for an ask, residents it knows who meet it (a master with an apprentice place, or the trade
  asked for). Never anyone who is:
  - Out of character, or with the setting off (and a player with it off gets no pointers either);
  - blocked either way, or a party mate;
  - known well already;
  - pointed at three times this hour.
- **Helpers:** an available mentor, or a self-declared Newcomer Guide who isn't a mentor. A busy mentor isn't sent
  newcomers, as Busy promises.
- **Said:**
  - The briefing names the other as the player sees them in In Sight, lookalikes numbered ("a dun wolf (2)"), with
    where ("just over there", "over at …") and why. It tells the model to use no other name.
  - The reply is checked: any registered name of theirs the player doesn't know becomes that look (`Game::sayMatch`).
  - Without a model (the router, a written line, an unavailable Mind), the written line follows the reply.
  - The other wolf, if it can see the matchmaker, gets "The wolf who serves at the inn nods your way while talking
    with …".
  - Each pointer is logged as a `matchmaking` event (the matchmaker, the player, the reason and the other's id), for
    the DM's LIVE feed.
- **The setting:** "Residents may point others to me, and me to others" in Settings (on by default), on the account
  like the other settings.
- **Development:** `{"type": "acquaint", "npc": id, "familiarity": N}` (only with `--dev-tools`) makes a resident know
  one's wolf, for trying matchmaking offline.
- **Tests:**
  - `Tests/newcomer_tests.cpp`, now 2,327 checks. The rules: the ask, the picker in its order, exclusions, the blanks.
  - At Upper Accord's inn, with a stand-in Mind:
    - Nell (new) is pointed at Mo, an available mentor, by his numbered look, in the written line, and Mo sees the nod;
    - never Otto (Out of character), Opal (setting off), Bea (blocked by Nell; then busy);
    - not again within the game hour;
    - two more newcomers are pointed at Mo, but not a fourth that hour;
    - Vic's "looking for work" goes to the master next door;
    - with the model answering, the briefing names Mo by his look, and the "Mo" the model wrote comes out as his look.
  - `tools/client/newcomer.mjs`, with the Mind off: Ash, his tie ended, asks the innkeeper and is pointed at Bo by his
    look, and Bo sees the nod. Screenshot 8.
  - Not exercised through the game: the matchmaker's six-an-hour limit, party mates and known-well wolves (single
    checks in `matchmake`), and the same-start-town reason.
- **Cost:** only when a matchmaker answers a player, at most once a game hour each, reading one town's roster and the
  matchmaker's bonds.
- **Not built:** the sixth reason, "two angles", waits for docs 53 and 57.

### Phase 5 (2026-10-07): innkeeper introductions and vouching

- **First evenings** (`Game::tendEvenings`, every five seconds, only for connected newcomers):
  - A newcomer's first evening counts between 17:00 and 23:00, in a place where an innkeeper is at work (its post's
    place) and awake.
  - The others there must not be Out of character, must not be blocked either way, and must have matchmaking on.
  - Once a character. An evening with no one else there counts as tried, three at most (`newcomers.json`'s
    `evenings`).
  - Newcomers present together are welcomed together, so a room with several hears one welcome. An innkeeper welcomes
    no one again for ten minutes.
- **The welcome** (`Game::welcomeAtInn`; the lines in `matchmaking.json`):
  - The innkeeper says the welcome aloud to the newcomers, and the room hears it.
  - Then it gives a line for each of up to four wolves it knows: "That's {who} there: a regular here and looking for
    company tonight." The facts are a regular (familiarity 30) or in now and then, Looking for a scene, and a Newcomer
    Guide or available mentor.
  - **Each listener hears the wolves as it knows them.** The innkeeper speaks names, and a post marked `veilNames`
    (new on `ParsedPost`, set only by the game) is veiled for each listener. A name is never passed on as hearsay, and
    lookalikes keep each listener's own numbering. The matchmaker's written line now does the same when the wolf
    pointed at is in the player's place.
  - Each wolf pointed out, and each newcomer, gets an `introducePrompt` under the map ("The wolf who serves at the inn
    is pointing you out to a newcomer. Introduce yourself?"). Introduce yourself sends their introduction: to the
    newcomer, or to the room for the newcomer.
- **Vouching** (`{"type": "vouch", "resident": id, "for": id}`; the numbers in `newcomers.json`'s `vouching`):
  - Who may vouch: the resident's trust in the voucher must be at least 30 and its liking at least 20 (the resident's
    menu then offers **Vouch for…**, listing the wolves in sight).
  - The one vouched for must be in earshot of the resident, not distrusted (trust above −20), and not blocked either
    way. There is one vouch per pair, and a voucher may hold five at once.
  - The voucher says "They're with me. I'll vouch for them." aloud.
  - The resident gets 30% of its trust (at most 15), 20% of its liking (at most 10), and 10 familiarity. **Each of its
    household gets half** (a household is residents with the same home, at most 8; the user, decision 20).
  - It lasts 30 game days, and its share stays after that.
  - The resident's briefing says who vouched.
- **Gone bad** (`Game::tendVouches`, following new crime incidents): within the vouch, the one vouched for robs or
  assaults the resident, is seen at a crime by it, or commits one in its town (the incident's law town). Then:
  - the resident's trust in the voucher falls by twice the share, and its liking by 5;
  - the share is taken back from the resident and its household, and the vouch is broken;
  - the voucher is told, and the resident brings it up the next time they talk (once).

  A crime elsewhere, against someone else, changes nothing.
- **Saved:**
  - vouches as `game.vouches` through `game.sections` (migration `0043_vouches.sql`, not applied);
  - each character's evenings in the people root (`evenings`).
- **The client:**
  - the Introduce prompt under the map, beside a tie offer (`hud/tie.ts`);
  - "Vouch for…" in a resident's menu, then "Vouch for {wolf}";
  - `people.test.ts` covers both.
- **Tests:**
  - `Tests/newcomer_tests.cpp`, now 2,637 checks. The rules: the share and its limits, who may vouch, a vouch saved and
    read back.
  - At Upper Accord's inn at 19:00, Nell (new):
    - is welcomed aloud, and the room hears it;
    - Vic and Val are pointed out by her look for them (Val as a regular looking for company), and Val hears the same
      line with her own name for Vic;
    - Otto (Out of character) and Bea (blocked) are not;
    - Vic, Val and Nell get prompts, and the welcome comes once;
    - Pip, alone three evenings at Ser Ferro's inn, isn't welcomed on the fourth.
  - Vouching:
    - Vic isn't trusted enough and is told why;
    - the menu offers Vouch for… to Mo;
    - Mo's vouch is said aloud and moves the innkeeper's trust by the share, and a household member's by half;
    - one vouch a pair;
    - a theft in Ser Ferro by another wolf vouched for leaves that vouch standing;
    - Nell's theft from the innkeeper breaks hers: Mo's standing falls by twice the share, the share comes back from
      her and the household, Mo is told, and the innkeeper raises it when Mo next speaks.
  - `tools/client/newcomer.mjs` gains the evening: Cara arrives, dusk falls (`--dev-tools`' time), and the innkeeper
    welcomes her and the other newcomers together. It points out Bo by look; Cara and Bo are prompted, and Bo
    introduces himself. Screenshot 9.
  - Not exercised through the game: five vouches at once (the pure test covers it) and the 30-day end.
- **Cost:** a five-second look at connected newcomers in the evening; vouches follow only new incidents.
- **To note:** an innkeeper's post in the worlds as built is 8–17 by default; "at work" here means in its post's place
  and awake, whatever the hour, so a welcome happens wherever the innkeeper keeps the evening.

## Depends on and feeds

- **Depends on:**
  - doc 50 Phase 1 (played time, `Game::accountLevel`, the account record, the card, status, experience, settings),
    Phase 2 (block, upheld reports) and Phase 4 (known wolves and shared-scene counts, for ties);
  - doc 32's names, introductions and the Introduce button;
  - doc 26's bonds, positions and crime;
  - doc 28's speech router normaliser and light model.
- **Doc 51:** its Welcoming tag needs this plan's newcomer flag. Phase 1 here is small and can be built before doc 51
  if Welcoming should work from the start; otherwise doc 51 ships the tag and it comes alive with Phase 1.
- **Feeds:** doc 51 (Welcoming; stars on mentors' cards), doc 58 (a tie can start a personal story), doc 55 (the letter
  starter becomes a real letter), doc 57 (the debt starter becomes a debt to pay off), doc 53 (reason 6 once shared work
  exists), doc 49 (unlocks read the same account measures).

## Risks

- **Few mentors early.** With a small player base most ties go to residents. That is fine: residents are always there
  (Principle 3), and the resident fallback is built to be good, not a consolation.
- **Mentors stuck or swamped.** One tie at a time, the lapse and the rest day (agreed) bound it; the DM sees every tie.
- **The model saying a name anyway.** The reply check veils it; the briefing never contains the name.
- **A bad actor as mentor.** Gated by level and upheld reports; turned off at once by an upheld report; newcomers can
  block, and a blocked mentor is never tied to them.
- **Start towns and the capital.** Moving the world's spawn would move the treasury; this plan never moves it.
- **A town's bounds.** Counts use `World::townOf`, which goes by region. If a city's region also holds a neighbouring
  settlement (Cinderbrook lies in another cell of the Ser Ferro Marches), the count would include it; Phase 1 checks the
  three towns in DEV and narrows to the start cell and its interiors if needed.
- **Other sessions** are editing the economy files. This plan reads residents' positions through existing accessors
  (`Society::jobOf`, `PositionState`) and changes none of `Core/RatwOrchestrator.*`, `RatwDemand`, `RatwOddJobs`,
  `RatwResidents`, `RatwSociety.h` or `Data/Economy/`.

## Decisions

Agreed (doc 48):

1. Upper Accord is the default start unless Ser Ferro or Ridgemere has more active wolves nearby, smoothed over 30
   minutes; later characters choose any of the three (§7.1, decision 16).
2. The newcomer flag is account-based and lasts until 15 hours played or social level 3 (§7.2, decision 18).
3. Mentors from social level 5, opt-in, available or busy, rewarded by recognition only (§7.3, decision 18).
4. Ties at creation: required for a new account's first character, optional after; a random suggestion with reroll and
   pick; mentors first, who must accept within 3 minutes, then the next mentor, then a fitting resident; one tie per
   mentor; lapsing after 7 days or 3 shared scenes; the mentor rests a day (§7.4, decision 20).
5. **Residents as matchmakers**, the user's favourite: the server picks the pair and the reason, the Mind only speaks
   it, by look or a known name; never an Out of character, opted-out or blocked wolf; at most once a game hour (§7.5,
   decision 15).
6. Vouching: a share of trust carries over, and the voucher's bond takes the hit if the newcomer misbehaves (§3.9).

New placeholder choices in this plan:

7. "No upheld report in the last 30 days" for mentors (proposed in doc 48).
8. Matchmakers are innkeepers, priests and each town's market merchant, since no world has market wardens.
9. The innkeeper points wolves out by look and prompts them to introduce themselves, rather than naming them (doc 48
   §7.6's "by the names it knows" would be hearsay).
10. Arrival points: the spawn for its own town, else the market stand, else a tile in the data file; the spawn never
    moves.
11. Ties don't introduce the two, except family starters.
12. The tie's marker shows where the other was when the tie was made, for 20 minutes; nobody is tracked.
13. Newcomers see available mentors marked on the map; others see the mark only on the card and in In Sight.
14. A mentor's "guided" count counts ties that ended with a shared scene.
15. The matchmaking reasons' order, and their limits (6 an hour a matchmaker, 3 times an hour a wolf pointed at).
16. One matchmaking setting covers both directions, on by default.
17. Vouching numbers: trust 30 and liking 20 to vouch; 30% of trust (at most 15) and 20% of liking (at most 10) carried;
    30 game days; twice the trust given lost on misbehaviour.
18. The limits and numbers in §1–§7.

Answered by the user, 2026-10-07:

19. A first character may choose its start town too: the busiest is preselected, and any of the three may be picked.
20. Vouching carries to the resident's household at half the resident's share, and is taken back from them too if the
    vouch goes bad.

## Open questions

None: both answered 2026-10-07 (decisions 19 and 20).
