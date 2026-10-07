# 56. Fame and memory: deeds, nicknames, the chronicle, and being welcomed back

Drafted 2026-10-06 as an actionable plan for doc 48 (§3.7, §5.6's festival criers, §8.4, §8.5, §8.6). Nothing built.
Read doc 48 (Principles, §3.7, Part 8 and the Decisions) and docs 26 (Phases 3, 5, 7, 8 and 10), 32 (§1.4, §1.5,
§4.2b), 30 (Phase 6), 28 and 09 first.

Marks: **(agreed)** is the user's decision in doc 48; *(placeholder)* is a number or name that is one constant to change;
**(new)** is a choice this plan makes, listed under Decisions for the user to confirm.

## The ask

> "NPC's should DEFINITELY come up with nicknames for wolves they recognize as having done something good and also
> mention your deeds and achievements. These should spread by rumor to other towns, too, but it should depend on the
> severity/immensity of the event, and whether NPC's recognize you." (doc 48, §3.7)

Doc 48 agreed the rest in its first round (decisions 23 and 24): deeds weighted by how far they travel, recognition
that follows the name rule, nicknames, deeds mentioned in greetings and introductions, festival criers, a chronicle
from the ledgers, unfinished business, and a welcome after a long break. The principles that bind it: recognition over
pay (2), the world remembers and says so (6), never punish absence (7), and the existing rules (8): no prose in
ledgers, the Mind never grants, names stay hidden until given.

## Where we stand (read from the code 2026-10-06)

**Rumours exist, and spread cheaply.**
- `Belief {holder, subject, claim, source, confidence, day, incident}` (`Core/RatwRoads.h:75`) lives in
  `World::beliefs_`, saved to `game.beliefs` (migration 0024), at most 30 a holder. `World::believe`
  (`RatwRoads.cpp:286`) adds one; `World::rumoursAbout` (`:310`) phrases them for the Mind.
- `World::rumoursFromEvent` (`:324`) plants them from deaths, harm, promises, marriages and newcomers; crime witnesses
  believe through `World::witness` (`RatwCrime.cpp:145`).
- `World::gossip` (`RatwRoads.cpp:1534`), once a game day: each resident tells its two surest rumours to its three
  closest at ×0.7; rumours fade ×0.97 a day. `World::caravanArrived` (`:705`) passes up to 8 to the next town's
  merchants at ×0.6. Ambient talk (`World::ambientTopic`, `RatwAmbient.cpp:169`) voices gossip a listener lacks.

**One good deed already travels, crudely.** `World::clearCamp` (`RatwRoads.cpp:~1500`) makes every merchant in every
town believe the player "drove the bandits off the road at …", ignoring witnesses and the name rule.

**What is missing.** A `Belief` has no weight and no record of whether the holder knew the subject's name. Its subject
is an entity ID, so a resident who heard of a player recognises them on sight, by name or not. There is no deed record,
no nickname, and no way for "the grey wolf with a torn ear" to become "Kestrel" later.

**Names** live in the game, not the world: `names::Acquaintances` (`Core/RatwNames.h:46`), held by `Game` (`known_`) in
the checkpoint (doc 32's planned `game.acquaintances` table doesn't exist). `Game::labelFor` (`RatwGameNames.cpp:89`)
gives a viewer's name for a wolf, else their look; `Game::noticeIntroduction` (`:190`) is where a name is learned.

**A name leak, found while reading.** Ambient gossip about a player speaks their *true* name: the facts use
`name(belief.subject)` (`RatwAmbient.cpp:~232`), `Game::ambient` sets `context.subjectName = subject->name`
(`RatwGameAmbient.cpp:~85`), and speech isn't veiled (only `system()` text is). Deeds would make it worse; Phase 2
fixes it.

**The Mind's briefing.** `Game::dialogueContext` (`RatwGame.cpp:3336`) fills `mind::Context` (`Core/RatwMind.h:18`).
Its `relationship` field carries `Bonds::describe`, `promisesBetween` and `rumoursAbout`, cut to 1,000 characters
(`RatwMind.cpp:193`). `tools/npc_mind.py` refuses a request with an over-long field (`OPTIONAL_LIMITS`), and the NPC
falls back to a written line. Greetings are often answered by the game itself (`Game::gameAnswer`,
`RatwGameVoice.cpp:69`, `Data/Voice/router.json`) and never reach a model.

**Festivals** fall on the 46th day of each season and gather the town at its square from noon (`World::dayPlan`,
`RatwSchedules.cpp:197`). There is no crier. The nearest mechanism is `Game::sermons` (`RatwGameAmbient.cpp:297`): a
preacher yells a line a minute from `Data/Voice/sermons.json`, only where a player is in the cell.

**Chronicles** exist for the Dungeon Master: `tools/chronicle.py` compiles a life from `game.events` (indexed by actor,
target, kind and cell; migration 0021) and stores nothing. Players have no chronicle page. `RatwJournal.*` is the save
journal (doc 31), not a character journal.

**Unfinished business** is scattered: `Promise {by, to, what, made, due, status}` (`RatwWorld.h:494`, due in three game
days), Stories (`SocialStory`, `RatwSocialCore.h`), taken contracts with `quantity` and `delivered` (`RatwRoads.h:57`).
Nothing gathers them. **A bug:** "promise kept" is pushed into `events_` without `World::recordEvent`, so the "keeps
promises" rumour never forms.

**Absence.** `Game::leaveCharacter` (`RatwGame.cpp:1684`) sets `Entity::awaySince` (a calendar day). On entering,
`World::returnFromAway` (`RatwBattle.cpp:3306`, called at `RatwGame.cpp:1634`) rests the wolf and resets it to −1, so
nothing later knows how long they were gone. `Bonds::fade` (`RatwBonds.cpp:107`) takes 0.5 familiarity a day after
three days without contact: **a wolf away a real week (42 game days) loses about 20 familiarity with every resident**,
which breaks Principle 7.

**Game time.** A game day is four real hours (`calendar::SecondsPerDay`), a season about 15 real days. "Three days
away" below means real days.

## Scope

**This plan builds** the deed ledger; how deeds travel, and recognition by name or look; nicknames; mentions in the
Mind's briefing, the game's greetings and ambient talk; festival criers' content; the chronicle; unfinished business;
welcome back, including bonds that don't fade while a wolf is away; and the DM's view and actions.

**It leaves to other plans:**

| Doc | What |
|---|---|
| 50 | The player card and its journal tab. Until then these sections sit in the character sheet. |
| 54 | Festivals themselves: when, contests, the crowd. This plan supplies what the crier says. |
| 55 | Residents' thank-you letters after a deed, read from this plan's deeds. |
| 57 | Residents' troubles and town projects, which *make* deeds through `Game::recordDeed`. |
| 52 | Introducing wolves to each other, which uses `fameLine`. |
| 58 | Big stories' credits, which record great and legendary deeds here. |
| 26, 32 | Bad deeds: crime rumours stay as they are (agreed: "This plan adds only the good side"). |

## Design

### 1. Deeds

A **deed** is a ledger event: IDs, a kind, a weight, a place and its witnesses, never prose (doc 08). Lines are written
from templates when someone reads or hears of it.

| Weight (agreed) | First sources | How far it travels |
|---|---|---|
| **Small** | Kept a promise to a resident; tended a downed wolf; carried a courier letter; saw an escorted caravan through; a small trouble solved (doc 57); a festival contest won (doc 54's `festival won`) | The witnesses and the beneficiary's household, then doc 26's ordinary gossip |
| **Notable** | Broke a bandit camp (today's `clearCamp`); a report that got a thief caught; a notable trouble solved, or chief giver to a finished project (doc 57) | The whole town within *(placeholder: 3)* game days |
| **Great** | Chief giver to a great project, or peace between great houses (doc 57); a story milestone's main contributors (doc 58); DM award | Its town, then neighbouring towns as caravans arrive |
| **Legendary** | A world story's climax (doc 58, §8.3); DM award only | Every town within *(placeholder: 2)* days, called by criers |

- **Kinds** live in `Data/Fame/deeds.json` **(new)**: id, weight, family (for nicknames), and phrase templates ("drove
  the bandits off the road at {place}"). A kind may cap itself: the third courier letter to one resident in a season
  makes no deed.
- **Paid work isn't a deed by itself** **(new)**: a contract makes a deed only where the kinds table says so (an escort
  through bandits, a bounty). Recognition is for what goes beyond the pay.
- **Deeds pay no XP** (Principle 2; doc 49 removes levels).

**The record** (`fame::Deed`, `Core/RatwFame.h` **(new)**):

```text
Deed {
  id            "deed-<n>"
  kind, weight  from Data/Fame/deeds.json
  doers[]       player characters (a party's fighters may share one deed), at most 6
  beneficiary   a resident, a town ("town:<id>"), or empty
  town, cell, x, y, day
  witnesses[]   {id, as{doer → name heard, or ""}}   at most 8, residents and players
  looks{doer → stranger label then}                  names::describe of the appearance at the time
  names{doer → names it travels under}               from witnesses who knew a name; grows when joined (§3)
  source        an event, a contract, a trouble, a project, a story step, or "dm"
  towns[]       {town, since, carrier}               the towns whose word holds it (§2)
  revoked       DM only
}
```

- **Witnesses** are found as crime witnesses are (`World::witness`: onstage by sight, offstage by nearness), plus the
  beneficiary always. `as` holds the name each witness knew the doer by (`known_.nameFor`), or nothing.
- **Kept in** `game.deeds` **(new table)**, one jsonb row per deed, written by checkpoints like the other
  `game.sections` lists (as migration 0030 does). Each deed is also a `game.events` row (kind `deed`; actor, target,
  item the kind, quantity the weight, detail the ID), so the chronicle and the DM read it from the one record.
- **Live** while any town's word or resident's belief holds it: about a month of game days for a small deed, two
  seasons for a notable one, two years for a great one, always for a legendary one *(placeholders)*. After that it
  lives on in `game.events`.
- **Made by** `World::deed(kind, doers, beneficiary, cell, source)`, which queues a seed as `World::award` queues XP
  (`RatwProgress.cpp`). Each tick the game takes the seeds (`World::takeDeeds`), fills in names from `known_` (the
  world doesn't know names) and records the deed in `Game::fame_`. Other plans call `Game::recordDeed`.

### 2. How deeds travel

Three carriers, by weight. None scans every resident.

1. **Personal beliefs** (every weight).
   - The witnesses, the beneficiary and the beneficiary's household believe it: claim `deed:<id>`, confidence 0.9 for
     witnesses and 0.8 for the household *(placeholders)*. Doc 26's daily gossip carries it on.
   - `Belief` gains one field, `as` (the name the holder knows the subject by, or empty), saved in `game.beliefs`'
     jsonb.
2. **The town's word** (notable and up).
   - The deed's town holds it: `{town, since}`. There are no rows per resident.
   - A resident of that community *has heard* it when `hash(resident, deed) mod 1000 < 1000 × reach × ear`.
   - `reach` grows from 0.25 at once to 1 over 3 game days. `ear` is 1.5 for innkeepers, merchants, priests and the
     watch, 0.6 for children, else 1 *(placeholders; from `jobCategory`)*.
   - So a resident always gives the same answer, and more of them have heard as days pass. Confidence is 0.6.
3. **Between towns.**
   - **Great deeds** join a neighbouring town's word when the next caravan from the deed's town arrives
     (`World::caravanArrived`), at reach 0.15, growing at half speed.
   - **Notable deeds** cross only with a caravan a player escorted (doc 48 §1.5; doc 57 builds the rest of that lever).
   - **Legendary deeds** join every town's word at reach 0.5 at once, and criers call them at noon the next day (§7).

**Fading** *(placeholders)*:
- A town's word holds a notable deed fresh for a season, then lets it fall to nothing over the next.
- A great deed holds a year, then fades over another. A legendary one never fades.
- A town's word holds at most 40 deeds; the lightest and oldest go first.
- The daily fame pass, run with the world's other daily work, updates reach and fading: O(live deeds).

**`clearCamp`'s beliefs are replaced** by a notable deed, so breaking a camp no longer tells every merchant at once.

### 3. Recognition: by name, by look, and joining up (agreed, doc 48 §3.7)

| The resident | Knows the deed | And says |
|---|---|---|
| knows the wolf by a name the deed travels under | by name | "You're Kestrel, who broke the camp on the east road." |
| doesn't, but the wolf's look today matches the deed's look, and the look is distinctive | by look | "You're not the grey wolf with the torn ear who broke the camp?" (unsure) |
| neither | not at all | Nothing |

- **Distinctive** means the look names a marking ("with a torn ear"). "A grey wolf" matches too many to be sure of. A
  look changed since (a new lasting injury, doc 38) no longer matches, as a future disguise wouldn't.
- **Joining up.** When a resident who knows a deed by look (from a belief or the town's word) learns the wolf's name
  (`Game::noticeIntroduction`, after `learnName`):
  - their belief gets `as`, and the name joins the deed's `names` (at most 4);
  - their next reply is briefed once: "You have just realised this is the wolf who …" ("So *you're* the one who…");
  - from then on the town's word carries that name, so everyone who knows the wolf by it can connect.
- **Aliases work as names** (doc 32 §1.5). A deed done as "Ash" travels as Ash; a resident who knows the same wolf as
  Kestrel can't connect it until they hear "Ash" from them.
- **Players** learn only what they perceived. A crier or gossip tells them that "Kestrel" did something, never who
  Kestrel is.
- **Names in public lines.** Anything a player can hear (criers, ambient talk) names:
  - a wolf only by the deed's `names` (names witnesses were given) or its look;
  - a resident by their public role ("the miller"), never by an entity's true name.

  The Mind's briefing may name residents to the NPC, who knows its neighbours, as today.

### 4. Nicknames (agreed, doc 48 §3.7)

- **When:** at a notable or greater deed, or at the third small deed of one family by the same wolf in the same town
  within a season *(placeholders)*.
- **Who coins it:** of the residents who can tie the deed to the wolf, the witness or beneficiary who likes them best
  (affinity 20+, familiarity 15+). Failing that, the town's innkeeper once the word reaches half the town
  *(placeholders)*.
- **The first coiner is remembered** (agreed). Their Mind is told "You were the first to call them 'the Ferryman'."
- **From a template plus the deed's details** (agreed). `Data/Fame/nicknames.json` **(new)** holds forms per family, one
  chosen by hash:
  - an epithet ("the Ferryman");
  - a possessive ("Hale's Luck");
  - a deed phrase ("the wolf who pulled the miller's pup from the river");
  - a look form for a wolf known only by look ("the torn-eared grey").

  A nickname already worn by another wolf in the town isn't used.
- **It travels with the deed:** whoever can tie the deed to the wolf knows the nickname. No separate spread.
- **At most 3 a wolf** *(placeholder)*: residents use the one from the heaviest, newest deed; the chronicle lists all.
- **Players can't pick their own** (agreed): there is no command for it.
- **Asking residents to drop one** (proposed in doc 48): the character sheet's nickname row has *Ask folk not to use
  it*.
  - Residents stop at once, and it can't be coined again from that deed.
  - The player is told "You let it be known you'd rather not be called that."
  - It stays in the record, marked dropped, for the chronicle and the DM.
- **Stored** in `game.nicknames` **(new table)**: `{id, wolf, text, family, deed, coinedBy, town, day, dropped}`. A name
  from a template, not chat prose. Each coining is a `game.events` row (`nickname`).

### 5. Residents mention deeds

**The Mind's briefing:** two new optional fields on `mind::Context`, apart from `relationship`, which is already full.
The server writes both, and only for an identified speaker.

| Field | Limit | What goes in |
|---|---|---|
| `fame` | 400 characters | Up to two deeds this NPC can tie to the speaker (heaviest, newest), how they know and how sure; the nickname they'd use; whether they coined it; "(you spoke of it on Summer 12)" if they have |
| `away` | 200 characters | §10 |

- Example of `fame`: "You have heard (the town's talk; fairly sure) that this wolf broke the bandit camp on the east
  road. Folk call them 'the Lantern'. You think it might be them: they look like the one described."
- `tools/npc_mind.py`'s rules gain a line: *speak of it when it fits, at a greeting or when introducing them; never
  every time; never invent deeds.*
- After a reply briefed with a deed, the server notes it in the memory ("(you spoke of …)").

**The game's own greetings.**
- `router.json`'s `greet` gains a band, `famous`, used at most once a game day per resident and wolf *(placeholder)*
  when the resident can tie a notable deed or a nickname to the speaker.
- Examples: "Well, if it isn't the Lantern." "Morning. Heard about the east road."
- `{nickname}` and `{deed}` come from the same lookup.

**Introductions.** `Game::fameLine(knower, wolf)` returns the phrase a resident would use ("the one who broke the camp
on the east road"), by name or look, or nothing. Doc 52's matchmakers and innkeepers use it.

**Ambient talk.**
- A new topic, `deed`, in `World::ambientTopic` (score 3.2 *(placeholder)*): one of the pair has heard a deed the other
  hasn't.
- Voiced from new written scenes, `Data/Voice/scenes/deeds.scene`, with blanks `{subject}` (the deed's public name or
  look), `{deed}` and `{nickname}`.
- The listener comes to believe it, as with gossip.

**Bad deeds** keep their own path (agreed). A wolf can be "the Lantern" and wanted for theft; the Mind sees both.

### 6. Fixing the gossip name leak

In the same phase as the `deed` topic, gossip about a player uses the name *the teller* holds, or the look:
- The world asks the game through a namer callback (`World::setNamer`, set to `labelFor`) instead of `Entity::name`.
- `context.subjectName` in `Game::ambient` uses the same.

This touches `RatwAmbient.cpp` and `RatwGameAmbient.cpp`, which no other session is editing now.

### 7. Festival criers (agreed, doc 48 §5.6; the crier's content only)

- **Who:** in each town, a resident whose post is a crier or official (`jobCategory` "official"), else the innkeeper
  nearest the square, else the priest *(placeholder order)*.
- **When:** doc 54's festival programme calls `Game::festivalCrier(community)` at its slot (20:00 in doc 54's draft).
  Until doc 54 is built, the crier calls at 13:00 on the festival day *(placeholder)*, at the square.
- **What:** a line a minute, yelled (`Voice::Yell`), heard through perception as the sermons are:
  - an opening;
  - up to 6 deeds from the town's word since the last festival, heaviest first;
  - the nicknames coined in town that season;
  - a closing.
- **Lines** come from `Data/Fame/criers.json` **(new)**, with these blanks:
  - `{name}`: the deed's public name, or its look ("a grey wolf with a torn ear, whose name nobody knows");
  - `{beneficiary}`: a role;
  - `{deed}`, `{nickname}`, `{place}`, `{festival}`, `{town}`.

  No model.
- **What it does:**
  - Each deed called is at full reach in that town, and residents at the square believe it.
  - A wolf whose deed was called by look can claim it by introducing themself there. Anyone who heard the call in the
    last game hour joins it to the name (§3). No new command is needed.
- **Only where heard:** lines are spoken only if a player is in the square's cell. The reach applies either way.
- **Legendary deeds:** every town's crier calls them at noon the next day, festival or not.

### 8. The chronicle (agreed, doc 48 §8.4)

A page of the character's life, from the ledgers, as template lines, never stored prose.

- **Told one by one:** first arrival; first meetings (as the owner knows those wolves now); ties (doc 52); Stories told;
  deeds and nicknames; Chapter moments; town projects given to and troubles solved (doc 57); skill milestones; lasting
  injuries (doc 38); festivals where their deed was called.
- **Folded** into one line a season, as `tools/chronicle.py` does for residents: scenes shared, places first visited,
  hunts, trades.
- **Read from** `game.events` rows where the character is actor or target (both indexed), the deed and nickname
  tables, and the social ledger's Stories.
- **New event kinds where missing:** `introduced` (actor, listener, the registered name given: a name, not chat),
  `story told`, `deed`, `nickname`, and doc 57's kinds.
- **Built on request,** by the `social` verb `chronicle`, as `reputation` is today:
  - `Core/RatwChronicle.{h,cpp}` **(new)** has `compile(rows, deeds, nicknames, stories, lens)`, which is pure. `lens`
    names everyone only as the owner knows them (`labelFor`).
  - Rows are read on a worker with the server's own read connection (as the cell prefetcher has), newest 2,000.
  - The result is sent as an event, `chronicle`, with dated lines ("Summer 9, Year 1: You broke the bandit camp on the
    east road. The carters spoke of it in Ridgemere.").
  - At most one request per character per 30 s *(placeholder)*.
- **Needs a grant:** `ratw_game` needs `SELECT` on `game.events` (today only the tools read it).
- **Without a database** (tests, file worlds) it is built from the in-memory deeds, nicknames and Stories, and says it
  is partial.
- **The DM and residents:** `tools/chronicle.py` learns the new kinds. The DM's Life panel shows deeds, and a resident's
  milestones for the Mind include deeds done for them ("Summer 9: Kestrel paid your debt.").

### 9. Unfinished business (agreed, doc 48 §8.5)

A short list of open threads. **Nothing nags:** no toasts, no reminders.

| Thread | From |
|---|---|
| Promises they made to residents, and when due | `World::promise` (`by` the wolf, open), with the stored `what` |
| Promises residents made them | the same, `to` the wolf |
| A Story one scene from being told, or waiting for their agreement | `SocialLedger` stories |
| Contracts taken and not done ("3 of 5 smoked fish handed in") | `RoadsState::contracts` |
| Letters received and not answered | doc 55's letters |
| Troubles heard of; projects pledged to | doc 57 |
| A personal story's next marker | doc 58 |

- **Shown** as a section of the character sheet (the card's journal tab once doc 50 builds it), soonest due first, at
  most 8 *(placeholder)*.
- **On Log out,** the page shows a "Before you go" card from the same list, with *Log out* beside it. The client builds
  it from the snapshot, so logging out never waits.
- **The data:** an owner-only `self.unfinished`, built in `Game::refreshSocialViews` (every 2 s, only when dirty).
- **The "promise kept" bug** is fixed here, through `World::recordEvent`.

### 10. Welcome back (agreed, doc 48 §8.6)

- **How long they were away:** `Game::leaveCharacter` stores `Entity::leftAt` (Unix seconds) beside `awaySince`.
  `Game::enterCharacter` (`RatwGame.cpp:1595`) reads both *before* `returnFromAway` resets `awaySince`, and keeps
  `{realDays, leftDay}` for the session. **The break** is 3 real days or more *(agreed placeholder)*.
- **Residents notice:**
  - A resident whose bond's `lastContact` is older than `leftDay` hasn't seen the wolf since the break. Their first
    briefing for that wolf gets `away`: "You last saw this wolf about three weeks ago, in late Spring. Greet them as
    someone back after a long while." The Mind is told how long in game time, since residents live in it.
  - `router.json` gains a `returning` band ("Haven't seen you since the thaw!"), with `{since}` from the season or
    festival of `leftDay`.
  - Once they have spoken, `lastContact` moves on and the mention stops. There is no list to keep.
- **Never punished for absence** (Principle 7): `Bonds::fade` skips residents' bonds toward a player character while
  they are away, from a set of characters away that the game updates at login and logout. Bonds pick up where they
  left off. This is in `RatwBonds.cpp`.
- **The "while you were away" card**, sent once on entering after a break as an event, `welcome`:
  - letters waiting (doc 55);
  - from towns they know (a deed there, or 3+ residents who know them): deeds in the town's word since `leftDay`,
    projects finished (doc 57), and marriages, deaths, apprenticeships and new posts among the 12 residents they know
    best;
  - what became of residents they helped (doc 57, and deeds' beneficiaries), Chapter news, and rested time waiting
    (doc 44; doc 49's faster practice).

  It is built from one worker query on `game.events` since `leftAt`, plus what is in memory. The page shows a card
  that can be closed, and keeps it in the sheet until the next logout.

### 11. The Mind's cost (docs 28 and 09)

- **No new model calls.** `fame` and `away` ride on replies that happen anyway. Nicknames, criers, greetings and the
  chronicle are templates.
- **`fame`** adds about 60–100 input tokens, only on replies to a wolf the NPC knows a deed about. If a tenth of replies
  carry it, that's about 2% more input tokens. `away` adds about 40, once per resident per return.
- **Caching:** both fields come after the persona and rules, so the cached opening is unchanged (doc 28 step 5).
- **Measuring:** prices aren't in the config yet (doc 28), so `tools/ai_cost.py` measures the effect after Phase 2.
- **Limits:** `OPTIONAL_LIMITS` gets `fame: 400` and `away: 200`. The server cuts each field first, so the Mind never
  refuses a request.

### 12. The Dungeon Master

- **Players tab, Life panel:** a character's deeds (kind, weight, towns and reach, witnesses by name) and nicknames (who
  coined them, dropped or not).
- **LIVE, a Fame layer:** each town's word with reach. Clicking a deed shows its witnesses and where it has travelled.
- **Actions** (`dm.actions`, audited, as doc 34 Part 2): `deed.award` (doer, kind, weight, town, beneficiary);
  `deed.revoke` (out of every town's word, residents' beliefs dropped, kept in the log as revoked); `nickname.drop` and
  `nickname.restore`.
- **Settings** are data read at start: `Data/Fame/deeds.json`, `nicknames.json`, `criers.json`, `chronicle.json`.
- **Storytellers:** doc 58's player storytellers can't award deeds (doc 48 Part 9). DM-run world stories can.

## Phases

### Phase 1: the deed ledger

**Goal:** good deeds are recorded with their witnesses and the names those witnesses knew, saved, and visible to the
DM. They travel only as personal beliefs.

**Changes:**
- `Core/RatwFame.{h,cpp}` (new): `Deed`; `fame::Ledger` (record, revoke, the `byDoer` index, save and load); kinds from
  `Data/Fame/deeds.json` (new).
- `Core/RatwWorld.h/.cpp`: `World::deed`, `takeDeeds`, and a witnesses helper drawn from `World::witness`.
- The sources: `World::clearCamp` (the merchants' beliefs become a notable deed); "tended" (`RatwBattle.cpp:3212`);
  "promise kept", through `recordEvent`; `settleContract` for courier and escort (`RatwRoads.cpp:225`); a player's
  report that leads to a warrant (`RatwCrime.cpp`, `weigh`).
- `Core/RatwRoads.h`: `Belief::as`.
- `Core/RatwGame.cpp`: `fame_`, `Game::recordDeed`, taking the seeds each tick and filling in names.
- `Core/RatwCheckpoint.cpp` and a migration (next free number): `game.deeds` and `game.nicknames` as `game.sections`
  lists.
- `tools/dungeon_master.py`: `deed.award`, `deed.revoke` and the Life panel's deeds.
- `tools/chronicle.py`: the `deed` kind.

**Tests:**
- `Tests/fame_tests.cpp` (new), with the `expect` and through-the-game patterns of `social_game_tests.cpp`: a camp
  cleared makes one notable deed; witnesses who knew the name hold `as`, others hold it by look; a promise kept makes a
  small deed and a "keeps promises" rumour; a DM award and revoke; all of it after a restart.
- `pg_tests` (the new tables), `tools/test_dungeon_master.py`, `tools/test_chronicle.py`.

**Done when:** breaking a camp no longer tells every merchant in the land, but its witnesses and the beneficiaries hold
the deed; the DM's Life panel lists it with witnesses; it survives a restart.

**Cost:** made at an event, never in the tick; witnesses found within one cell; a few thousand rows.

### Phase 2: deeds travel and are recognised

**Goal:** deeds reach as far as their weight says. Residents tie them to a wolf only as far as they know them, and
speak of them.

**Changes:**
- `RatwFame.cpp`: the town's word (reach, ear, fading, the cap); `knownTo(resident, wolf)` (by name, by look, or not);
  the daily fame pass.
- `World::caravanArrived`: great deeds cross with any caravan, notable ones with a player escort; legendary ones go
  everywhere. `Game::noticeIntroduction`: joining up.
- The `fame` field in `Core/RatwMind.h/.cpp`, `Game::dialogueContext` (filled, mentions noted) and `tools/npc_mind.py`
  (limit and rule). `Game::gameAnswer` and `router.json`: the `famous` band. `Game::fameLine` for doc 52.
- `RatwAmbient.cpp` and `RatwGameAmbient.cpp`: the `deed` topic with `Data/Voice/scenes/deeds.scene` (new); the name
  leak fixed with a namer callback (§6).
- DM: the LIVE Fame layer (`Editor/src/dm/LiveTab.tsx`, `GET /api/live/fame`).

**Tests:**
- `fame_tests`: a notable deed's share of a town grows from about a quarter to all over 3 game days, and each resident
  answers the same every time; a great deed reaches the next town only with a caravan, a notable one only with an
  escort; recognised by name, and by a distinctive look but not a plain one; a look changed by a lasting injury no
  longer matches; joining up at an introduction; an alias.
- `ambient_tests` (the deed topic; gossip never speaks a name the teller wasn't given), `scenes_tests` (`deeds.scene`
  loads), `voice_tests` (the famous greeting), `tools/test_npc_mind.py` (`fame` cut at 400, never refused).

**Done when:**
- within 3 game days, two residents of the deed's town who never saw it greet the doer by deed or nickname, one by name
  and one, unsure, by look;
- a resident in the next town knows of a great deed only after a caravan;
- no ambient line in the test world speaks an unknown player's true name.

**Cost:** the daily pass is O(live deeds); a caravan's arrival O(≤ 40); a lookup when an NPC replies or greets is
O(the wolf's live deeds) plus their ≤ 30 beliefs; nothing per tick. Gate: `world_check --simulate 7 7.2 --players 20`
within noise of the commit before.

### Phase 3: nicknames

**Goal:** residents coin nicknames, remember who coined them, use them, and stop when asked.

**Changes:**
- `RatwFame.cpp`: coining, the coiner, three a wolf, dropping. `Data/Fame/nicknames.json` (new).
- `Game::dialogueContext` (the nickname and "you coined it"), and `router.json`'s `{nickname}`.
- The `social` verb `dropnickname`, and `self.social.nicknames` in the snapshot.
- `Client/src/ui/hud/dialogs.ts`: the sheet's NICKNAMES row with *Ask folk not to use it*.
- DM: `nickname.drop` and `nickname.restore`.
- `tools/chronicle.py`: the `nickname` kind.

**Tests:**
- `fame_tests`: a notable deed coins one, by the witness who likes the doer best; three small deeds of a family coin
  one, two don't; no two wolves in a town share one; the look form; dropping ends it in briefings and greetings and
  prevents a re-coin; a restart.
- `Client/src/game/fame.test.ts` (new); `tools/client/fame.mjs` (new), in a real browser: a deed through the dev
  console, a greeting by nickname, the sheet's row, the drop.

**Done when:** a wolf who breaks a camp is greeted by a nickname in that town; the sheet shows it with its coiner (as
the player knows them, or by role); dropping ends it.

**Cost:** at a deed or in the daily pass.

### Phase 4: festival criers

**Goal:** at each town's festival, a crier calls the season's deeds and nicknames, and the town learns who's who.

**Changes:**
- `Core/RatwGameAmbient.cpp`: `Game::festivalCrier(community)` and a `criers` round beside `sermons` (the crier chosen
  once per town per festival and walked to the square; a line a minute, only where a player is in the cell).
- `Data/Fame/criers.json` (new).
- `RatwFame.cpp`: full reach for called deeds, and "heard the call" kept a game hour for joining.
- Legendary deeds' calls at noon the next day.

**Tests:** `fame_tests`, through the game: a festival with two notable deeds and a nickname, called by weight, by name
and by look; residents at the square believe them; the unnamed doer introduces themself and those present join the
deed; no lines with no player there, but reach still set; a legendary deed called everywhere the next noon.
`schedules_tests` passes unchanged.

**Done when:** a player at the square hears their deed and nickname called, and a resident who hadn't heard greets
them by it afterwards.

**Cost:** one choice per town per festival; a line a minute where a player stands.

### Phase 5: the chronicle

**Goal:** every character has a chronicle page, written from the ledgers.

**Changes:**
- `Core/RatwChronicle.{h,cpp}` (new): `compile` (pure), lines from `Data/Fame/chronicle.json` (new), the season fold.
- The worker read, with its own connection, and a migration granting `SELECT` on `game.events` to `ratw_game`.
- Event kinds `introduced` (`RatwGameNames.cpp`) and `story told` (`RatwGameSocial.cpp`).
- The `social` verb `chronicle` (rate-limited) and the event `chronicle`.
- `Client/src/game/state.ts` and `dialogs.ts`: a CHRONICLE section.

**Tests:**
- `Tests/chronicle_tests.cpp` (new), pure, on rows: first arrival; meetings by the owner's names, so a wolf never
  introduced shows by look; deeds, nicknames, Stories; the season fold; no line names anyone the owner wasn't told.
- `pg_tests` (the query on a scratch database), `tools/test_chronicle.py`, `tools/client/fame.mjs` (the page draws).

**Done when:** a player's chronicle shows their arrival, the wolves they met as they know them, their deeds, nicknames
and Stories, dated by the game's calendar, and the DM's Life panel shows the same deeds.

**Cost:** on request, one indexed query on a worker, at most every 30 s a character.

### Phase 6: unfinished business, and welcome back

**Goal:** open threads are easy to see; a wolf back from a break is welcomed and has lost nothing.

**Changes:**
- `Game::refreshSocialViews`: `self.unfinished`.
- `dialogs.ts`: the UNFINISHED BUSINESS section. `Client/src/ui/hud/hud.ts`: the "Before you go" card.
- `Entity::leftAt` (`RatwWorld.h`, saved in `RatwWire.cpp`).
- `Game::enterCharacter` and `leaveCharacter`: the absence, read before `returnFromAway`.
- The `away` field (Mind and `npc_mind.py`), and `router.json`'s `returning` band.
- `Bonds::fade` (`RatwBonds.cpp:107`): skip bonds toward characters away.
- The `welcome` event (in-memory part plus one worker query), and the client's card.

**Tests:**
- `fame_tests`, through the game: an open promise, a one-scene Story and a half-done procure contract are listed, and
  each leaves when settled; a player away 40 game days keeps their bonds' familiarity; a resident who last saw them
  before the break is briefed with `away` once and greets with `returning`, and one who met them since isn't.
- `bonds_tests` (fade skips the away set), `tools/client/fame.mjs` (both cards).

**Done when:** a player logging out sees their open threads without nagging; one back after three real days is greeted
as someone long gone, sees what happened while away, and finds every resident's regard where they left it.

**Cost:** `self.unfinished` is rebuilt only when dirty, from per-player lists; one query per long-absent login; `fade`
gains a set lookup per bond, once a day.

## Depends on and feeds

**Depends on:**

| Doc | For | Until it lands |
|---|---|---|
| 50 | The journal tab | These sections sit in the character sheet |
| 54 | The festival programme (the crier's slot, 20:00 in its draft) | The crier calls at 13:00 |
| 55 | Letters, in unfinished business and the welcome card | Those rows are left out |
| 26, 30, 32 | Rumours, caravans, ambient talk and names | (Built) |

**Feeds:** doc 52 (`fameLine`); doc 55 (deeds with a resident beneficiary, for thanks); doc 57 (`Game::recordDeed`,
guarded caravans carrying fame, chronicle and welcome rows); doc 58 (great and legendary deeds from milestones); doc 51
(deeds on end screens); the DM (the Fame layer).

## Risks

- **Deeds feeling cheap.** If small deeds are everywhere, greetings become a list. Mitigations: the `famous` greeting at
  most once a game day per pair; the Mind told what was already said; small deeds never reach the town's word; courier
  and contract deeds capped per resident.
- **Recognition by look leaks identity.** Only the true doer is ever "recognised", which tells a resident more than
  they could really know. Mitigations: distinctive looks only, worded as unsure, and later disguises break it. It is
  accepted, as crime witnesses already identify by sight.
- **Names in public speech.** Criers and ambient talk speak the deeds' names, which is intended (word gets round). Only
  names witnesses were really given may appear, and tests guard it (Phases 2, 4, 5).
- **`Belief::as`** touches `RatwRoads.h`, which doc 46's code also uses. It is one field, saved in the jsonb; agree it
  with the economy session first.
- **The chronicle query** needs a grant and a connection off the game thread. A missing grant fails safe: the page says
  it is partial.
- **Pausing bond fading** changes the people digest only of runs where players log out. `world_check`'s doesn't change.
- **"Within days"** is read as game days (a notable deed is all over town in about half a real day). It is one number if
  the user meant real days.

## Decisions

### Agreed (doc 48)

1. Deeds are ledger events with a weight (small, notable, great, legendary) that sets how far they travel (§3.7;
   decision 23).
2. A deed ties to a wolf only as far as witnesses knew them, by name or by description, and joins up when a name is
   learned (§3.7).
3. Residents coin nicknames from templates and the deed's details; the first coiner is remembered; players can't pick
   their own (§3.7).
4. Residents mention deeds in greetings, introductions and at festivals (§3.7).
5. Bad deeds keep the existing crime rumours (§3.7).
6. Festival criers recount the season's deeds and nicknames (§5.6).
7. The chronicle comes from the ledgers as template lines, never stored prose (§8.4; decision 24).
8. Unfinished business lists open threads and never nags (§8.5).
9. Welcome back after 3 days or more, with the Mind told how long, and a "while you were away" card (§8.6).

### New in this plan (placeholders for the user to confirm)

10. **Asking residents to drop a nickname** works at once, from the sheet, and that deed can't coin it again (doc 48
    left it proposed).
11. **Paid work isn't a deed** unless the kinds table says so; deeds pay no XP.
12. **Notable deeds** reach the whole town in 3 game days. Great ones cross by caravan, notable ones only by a
    player-guarded caravan. Legendary ones reach everywhere in 2 days.
13. **The town's word** is one record per deed per town, with a deterministic draw for "has heard", instead of a belief
    per resident.
14. **Recognition by look** needs a distinctive look.
15. **At most 3 nicknames a wolf**; residents use the newest from the heaviest deed.
16. **The crier** is the town's official, else the innkeeper, else the priest, at doc 54's slot (13:00 until then).
17. **Bonds don't fade toward a wolf who is away** (Principle 7).
18. **Welcome back** starts at 3 real days; the Mind is told in game time.
19. **The gossip name leak** is fixed in Phase 2 for all gossip about players.

## Open questions

1. **Should criers call deeds by look** ("a grey wolf with a torn ear, whose name nobody knows")? It makes a moment when
   the wolf steps forward, but points at them. *Recommendation:* yes, notable and up only.
2. **Should a deed warm a resident to the doer** the first time they connect it (say +5 liking, once)? It makes fame
   count in trade and first meetings. *Recommendation:* yes, notable and up, once per resident per deed.
