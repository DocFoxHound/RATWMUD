# 49. Characters without levels, and earned Gifts

Drafted 2026-10-06 as an actionable plan for doc 48 (§§2.1, 2.2, including "Gift tiers are earned, not chosen").
Nothing built. Read doc 48 (Principles, §2.1, §2.2, Plans, Decisions) and docs 44 (levelling), 45 (Gift balance),
43 (Gifts), 08 (social progression), 19 (character creation), 40 (sneaking), 41 (hunting and the nose), 26
(apprentices) and 47 (gear tiers) first.

Marks: **(agreed)** is the user's decision, with a pointer to doc 48; **(proposed)** is this plan's recommendation;
*(placeholder)* is a number or name that is one entry in a data file. **Balance is the user's, later** (agreed, doc 48
§2.2): this plan makes every number a data entry and builds the simulators for that pass. It does no balancing.

## The ask

> "What if we didn't let characters level up? What if it was just simply... you chose what your character's
> strengths/weaknesses were at character creation? And you can IMPROVE a character's skill through practice?"

> "No character levels are a yes, but don't worry about balancing it. I will do that later. For now, let's just make
> sure its documented."

> "Quickened wolves wouldn't be selectable to new players. Right now my schema for that is a player needs to spend
> time roleplaying as a normal wolf before they can make a gifted wolf, and then achieve a roleplay level and/or star
> amount (or some sort of way to measure that they're good roleplayers) before they can unlock Quickened wolves."

The user set the unlock measures (doc 48 Decisions 36): Gifted needs social level 3 and 10 rewarded scenes on Normal
characters, with no count of different wolves; Quickened needs Gifted, social level 8, 100 stars from at least 30
different wolves, 2 closed Stories and no upheld reports in the last 30 days.

## Where we stand (read from the code 2026-10-06)

### Levels and XP

- **One XP pool, keyed by character.** `SocialLedger` (`Core/RatwSocialCore.*`) keeps `entries` (receipts) and
  `points` (totals) by actor, and the actor is the character's ID (`wolf-…`). `SocialLedger::level` applies doc 44's
  curve (`Core/RatwLevels.h`: `levels::stepCost`, `xpFor`, `levelFor`).
- **Everything pays into it:** scenes (`payMember`, reason `qualified_session_settlement`); fights (`settleFight`:
  `FightXP` 10 for taking turns plus twice the scene pay for talking, on the same receipt reason); Gold and Story Stars
  (`star`, `storyStar`); Story closure (`close`); and doc 44's typed awards through `SocialLedger::award` (`work`,
  `practice`, `milestone`, `discovery`, `story`). `pay` adds rested XP (`restedLeft`, a `rested_bonus` receipt) and
  holds the 150-a-day `DailyCap`.
- **Awards** are queued by `World::award` (`Core/RatwProgress.cpp`) and paid each tick in `Core/RatwGame.cpp` ("+N
  experience (…)"). Sources: `World::tendProgress` (places first visited, an apprentice beside their master,
  sneak/listening/tracking at 25/50/75), forage and hunt kills (`RatwHunt.cpp`), repairs (`RatwDurability.cpp`),
  contracts (`RatwRoads.cpp`), Gifts lent (`RatwMagic.cpp`).
- **A level does one thing:** `World::temperamentOf` (`RatwBattle.cpp`) sets a player's fighting skill to
  `levels::fightingSkill(levelOf(id), quickened)` through the `World::levelOf` hook, which `RatwGame.cpp` points at
  `social_.level`. The self view sends the same as `fightingSkill`.
- **Totals are rebuilt at load** (`RatwCheckpoint.cpp`: `points[actor] += amount` for every receipt). The ledger is one
  row per receipt in `game.relationships` (migration 0012).
- **Social level gates** Chapter founding (`options_.chapterFoundingLevel`), friendship companions at 3
  (`RatwGameCompanions.cpp`) and the self-only title (`socialTitle`, `RatwGameSocial.cpp`).
- **Cost note:** `SocialLedger::award`, `usedToday`, `restedLeft` and `payMember` each walk the whole ledger, so every
  award and settlement costs more as receipts pile up.

### Attributes and skills

- `Entity` (`Core/RatwWorld.h`): `strength` 50, `dexterity` 50, `wisdom` 30; `hearing`, `vision`, `smell` as
  multipliers (1.0); `sneakSkill`, `hearingSkill` (Listening) and `scentSkill` (Tracking), 0–100.
- **`stamina` is not an attribute.** It is the current 0–100 bar (`step::StaminaRecovery`, 5 a second, `RatwStep.h`;
  `battle::staminaPerTurn(hurt, strength)` in a fight). Doc 48's stamina choice needs a new attribute.
- **Growth by use is scattered**, each place its own `min(100, x + k × (1 − x/100))`: sneaking unnoticed and ambushes
  (`battle::SneakUnnoticedWorld`, `SneakUnnoticed`, `SneakPerAmbush`), noticing a stalker (`NoticeTeaches`), finding
  trails (`World::smellTracks`, +0.3), the nose (`World::trainNose`, `RatwMarks.cpp`: `smell` toward `SmellMost` 1.6,
  tracking +0.1). All silent. Hearing and vision never grow.
- **Fighting skill doesn't grow.** `Entity::fightingSkill` is still saved (`wire::entity`, `readEntity`) but unread for
  players; `World::growSkill` is an empty stub, still called on every blow and at a fight's end.
- **Players have no trade skills.** Residents do (`CareerState::skill`, "resident|position"; `Society::practise`;
  `skillFamily`: craft, trade, labour, watch, service, travel, general). A player can be apprenticed
  (`Society::apprentice`) but learns nothing beyond "work" XP once a game day.

### Creation and accounts

- **Accounts exist:** `accounts::Accounts` (`Core/RatwAccountsCore.h`) maps a username to up to six characters, with
  creation receipts, saved in `game.accounts` (verifiers inside, so the tools can't read it). No reverse lookup from
  character to account, and nothing of progression on the account. Doc 08's "one character per development identity"
  is out of date; only the `--dev-identity` test path still works that way (`player-<id>`, no account).
- **One account can have two wolves in the world at once:** `Game::enterCharacter` refuses only the same character on
  two clients.
- **The creator** (`character_create` in `RatwGame.cpp`; `Client/src/ui/frontDoor.ts`): name, age, appearance and an
  optional `gift` `{tier, family}`, any tier free (doc 43). No attributes or specialty: every wolf gets the defaults.
  The fingerprint includes the Gift only when it isn't Normal, so old receipts still match.
- **The sheet** (`Client/src/ui/hud/dialogs.ts`): STR, DEX, WIS, FIGHTING, senses, the three skills out of 100, and
  "Level N · Title" with a bar that fills at 100 XP whatever the level.

### Gifts, stars, the DM, the simulators

- Stars and Stories exist in the ledger (`SocialStar`: giver and recipient character IDs; `SocialStory` states).
  Milestone stars (doc 48 §8.3), tags and an account total don't (doc 51). Reports don't exist (doc 50).
  `Entity::wardenAttention` is recorded (doc 43), as doc 48 says.
- **The DM app** (`Editor/src/dm/DmApp.tsx`, `tools/dungeon_master.py`): the Players tab lists STR, DEX, WIS, STA and
  the three skills; Make Gifted, Make Quickened and Take Gift away queue `character.gift` in `dm.actions`, which
  `RatwGame.cpp` applies.
- `Tests/level_sim.cpp` gives each wolf a `level` and answers `World::levelOf` from it (`fireq@20`; suites `levels`,
  `core`, `trance`, `tiers`: docs 44, 45, 47). `battle_tests` sets `levelOf` to 25; `level_tests` checks the curve,
  each award kind and rested XP. `game_tests` (`accountsAndARestart`) and `Client/tests/frontDoor.browser.test.mjs`
  create Quickened wolves freely. There is no practice or pace simulator.

**Missing:** a practice engine with caps, soft limits and anti-macro rules; growth lines; strengths and weaknesses; a
stamina attribute; player trade skills; fighting that grows; social level on the account from social XP alone; account
counters for the unlock measures; locks in the creator; DM controls for unlocks; simulators at skill bands.

## Scope

**This plan does:** the practice engine and its data; fighting by practice; strengths, weaknesses, a specialty and
presets at creation; the stamina attribute; player trade skills (growth only); doc 44's awards turned into practice;
social level from social XP alone, on the account; the migration of existing characters; earned Gift tiers per account,
with the creator's locks and the DM's controls; the simulators the balance pass needs.

**It leaves:** star totals, tags, milestone stars, the star display and less XP for roleplay in fights to **doc 51**
(this plan counts what exists and gives doc 51 one place to feed); reports, block, friends and the account handle to
**doc 50**; sparring, training grounds, trainers, practice posts, lend a paw, lead and hand, and hunting's use of skills
to **doc 53**; mentors and the newcomer flag to **doc 52**; rested time at inns and festivals, tavern-game skills, the
scholar's library work and exploration's reward to **doc 54**; growth and unlocks in the chronicle to **doc 56**;
storyteller gates to **doc 58**.

## Design

### 1. The data: `Data/Progression/`

Three JSON files, each with an `"about"` line, loaded like the Gift catalog (`RATW_DATA_DIR`, else the working tree:
`giftsFile` in `Core/RatwGifts.cpp`). `tools/progression_catalog.py` (with `tools/test_progression_catalog.py`) checks
them as `tools/gift_catalog.py` does: no unknown field or skill, no cap below its start, no preset over budget, no
Quickened threshold below Gifted's, no source naming a missing skill.

| File | Holds |
|---|---|
| `skills.json` | Attributes and skills (field, name, caps, soft daily limit, growth line and step); growth sources and base rates; partner and teacher multipliers; rested practice; anti-macro limits; the simulators' skill bands. |
| `creation.json` | The grades (weak, plain, strong) with each attribute's start and cap; budget and costs; specialties; presets; a point cost per Gift tier (0 for now). |
| `standing.json` | The social level curve (`stepBase` 100, `stepGrowth` 50: doc 44's), titles, and the Gift unlock thresholds. |

### 2. Attributes and grades

Seven attributes (agreed, doc 48 §2.2), each **weak**, **plain** or **strong**. Strengths start and cap higher;
weaknesses start and cap lower.

| Attribute | Field | Weak: start / cap | Plain | Strong |
|---|---|---|---|---|
| Strength | `strength` | 40 / 60 | 50 / 70 | 60 / 85 |
| Dexterity | `dexterity` | 40 / 60 | 50 / 70 | 60 / 85 |
| Wisdom | `wisdom` | 20 / 45 | 30 / 55 | 40 / 70 |
| Stamina | `endurance` (new) | 40 / 60 | 50 / 70 | 60 / 85 |
| Hearing | `hearing` | 0.85 / 1.15 | 1.00 / 1.30 | 1.15 / 1.50 |
| Vision | `vision` | 0.85 / 1.15 | 1.00 / 1.30 | 1.15 / 1.50 |
| Smell | `smell` | 0.85 / 1.30 | 1.00 / 1.45 | 1.15 / 1.60 |

All *(placeholders)*. Plain starts are today's defaults, so a plain wolf fights as one does now; smell's strong cap is
today's `SmellMost`.

- **The budget** *(placeholders)*: 2 points; a strength costs 1, a weakness gives 1 back; at most 3 of each; unspent
  points allowed. `tierCost` is 0 for Gifted and Quickened, ready for the user ("Whether a Gift tier also costs
  creation points is part of the user's balance pass", doc 48).
- **Stamina, the attribute** (`Entity::endurance`, saved, 50 by default) sets how fast the bar comes back and how
  slowly running drains it *(placeholders)*: out of a fight `StaminaRecovery × (0.8 + endurance/250)` and the sprint
  drain `× (1.2 − endurance/250)`; in a fight `staminaPerTurn` gains `endurance/25`. The bar stays 0–100.
  `wire::privatePace` sends both rates so the client's walking prediction and `World::placeByClient`'s check agree
  (doc 31).
- **The user's examples** as grades: a Normal wolf with weak Strength and strong Dexterity; a Quickened wolf with strong
  Strength and strong Wisdom.
- **Presets** (agreed names, doc 48 §2.2; contents *(placeholders)*), each spending the budget exactly:

| Preset | Strong | Weak | Specialty |
|---|---|---|---|
| Hunter | Smell, Hearing, Dexterity | Wisdom | Tracker |
| Scholar | Wisdom, Hearing, Vision | Strength | Trade: service (scribes, clerks) |
| Smith's hand | Strength, Stamina, Dexterity | Smell | Trade: craft |
| Brawler | Strength, Dexterity, Stamina | Wisdom | Fighter |

### 3. Skills and specialties

| Skill | Field | Start | Specialty start | Cap | Soft a day |
|---|---|---|---|---|---|
| Fighting | `fightingSkill` | 50 | 60 | 86 (Quickened 100) | 3 |
| Sneak | `sneakSkill` | 0 | 25 | 100 | 2 |
| Listening | `hearingSkill` | 0 | 25 | 100 | 2 |
| Tracking | `scentSkill` | 0 | 25 | 100 | 2 |
| Craft, labour, commerce, service, travel, watch | `skills[…]` (new map) | 0 | 25 | 100 | 3 |

All *(placeholders)*. Fighting runs over today's level 1 to level 25 (50 to 86; a Quickened wolf's 100, the user's call
in doc 45), so practice covers the range a level did.

- **Trade skills** are new for players and use `skillFamily`'s families (its "trade" is called commerce here). They
  grow now; what they do comes with doc 53 (the lead's skill sets quality, the hand's speed) and player crafting
  (doc 35).
- **One specialty** (agreed): Fighter, Sneak, Tracker, or one trade. It starts the skill higher and is starred on the
  sheet. A `capBonus` per specialty exists in data, 0 for now.
- **One fighting skill** (proposed). Styles (jaws, blade) can split it when more weapons arrive (doc 37's next step).
- **Other plans add skills in data** (tavern games, the scholar's work): an entry in `skills.json` with its sources.
  Code names a skill only where a rule reads it.

### 4. Practice: how anything grows

`World::practise(who, source, context)` is the one way an attribute or skill grows. `context` names the partner (a
wolf's ID, or a kind: `post`, `animal`), the place (cell, tile) and a variety key.

```text
gain = base × room × partner × teacher × variety × soft     (rested then doubles it, from the pool)
room = (cap − value) / cap, at least 0.1 below the cap, 0 at it      (today's (1 − x/100), with a real cap)
soft = 1 until today's gains in this skill reach its soft limit, then 0.2
```

- **Faster beside a better wolf** (agreed): ×1.5 when a player within 6 tiles is 10 or more points better; ×2 for a
  mentor teaching (doc 52) or doc 53's trainer; ×3 for an apprentice at their master's side (doc 26's rate). Only
  players and the resident actually teaching count: a skilled resident standing by isn't teaching, and judging every
  resident would mean a scan. Characters of the same account never count. The learner isn't told who it was (names
  stay hidden, doc 32 §1.5).
- **Faster against a live partner** (agreed): ×1 a player, ×0.8 a resident, ×0.6 a fierce animal, ×0.25 a practice post
  (doc 53). A foe 15 or more points weaker teaches ×0.5; one 10 or more better, ×1.5.
- **Slower near the cap, a daily soft limit** (agreed): `room` and `soft`. The day is a rolling real day, kept per
  skill on the character (`practiceDay`, saved, so a restart doesn't reset it).
- **Rested practice** (agreed: rested time "becomes faster practice after a break"): after a day or more without
  gains, a pool of 5 points a day away, 30 at most *(placeholders)*; each gain is doubled from it, outside the soft
  limit. `restedRate` is 1 for now; doc 54 raises it for time away in an inn bed (`Entity::awayInBed`) or a festival.
- **Anti-macro** (agreed: practice counts only on meaningful use):
  1. **Real use only:** each source has a condition (below). Nothing grows from standing, chatting or the Mind's words.
  2. **Variety:** a key (skill, source, partner or 4×4 tile block) seen more than 3 times in 10 minutes counts ×0.25
     *(placeholders)*; a ring of the last 16 keys per character, not saved.
  3. **The same partner again** in a day: ×1, ×½, ×¼, then nothing, as scene pay does (doc 08).
  4. **Passive sources need a player at the keys:** an apprentice's time counts only with input in the last 5 minutes.
- **Growth lines** (agreed, doc 48 §2.1): a system line when a skill passes a whole number: "Your tracking sharpened
  (31).", "Your fighting has improved (64).", "You move more quietly (22).". Attributes speak at their step
  *(placeholder)*: "You feel stronger (STR 53).", "Your nose grows keener (112%).". At most one line a skill each 2
  minutes, carrying the latest number; once at the cap ("Your tracking is as sharp as it will get."). Doc 44's
  milestones become lines too ("Your tracking reaches 50."), without XP.

**Growth sources** (`skills.json`; today's constants where one exists):

| Source | Grows | Base | Counts when |
|---|---|---|---|
| `fight.blow` | Fighting | 0.2 | A bite, cut or fire lands on a foe still able to fight (not Downed, not yielded). |
| `fight.end` | Fighting | 0.5 | A fight ends with the wolf in it, after two turns or more. |
| `fight.dodge` / `fight.shove` | Dexterity / Strength | 0.02 | A blow at the wolf misses / a shove lands. |
| `sneak.world` / `.arena` / `.ambush` | Sneak, a tenth to Dexterity | 0.01 / 0.05 / 0.5 | As doc 40 §6 today. |
| `notice.sound` / `.scent` / `.sight` | Listening and Hearing / Tracking and Smell / Vision | 0.3 (skills) | Catching a stalker by that sense (doc 40). |
| `track.found` | Tracking | 0.3 | Smell finds a trail (`World::smellTracks`). |
| `nose.use` | Smell, Tracking | 0.004 share, 0.1 | `World::trainNose`'s uses (the user's rule: the nose is a physical stat that grows with use). |
| `forage.pick` | Labour | 0.2 | Every picking that yields. |
| `apprentice.work` | The position's trade family | 0.006 a game minute | At the master's work, in its hours, the master there (×3 teacher). |
| `gift.mana` / `gift.lend` | Wisdom | 0.02 per 10 mana / 0.05 | A Gift used on a real foe or ally / lent and helping a batch (doc 43). |
| `contract.done` | Commerce | 0.5 | A contract fulfilled (`settleContract`). |
| `load.carry` / `run.far` | Strength / Stamina | 0.02 per 100 / 200 tiles | Walking under a heavy load (doc 35 §1.2) / running or sprinting. |

### 5. Fighting skill

- **It grows by fighting and sparring again** (agreed; this reverses doc 44's decision 2). `World::growSkill` becomes
  `growSkill(Battle&, const BattleFighter& learner, const BattleFighter* foe, source)` over `World::practise`; its four
  callers in `RatwBattle.cpp` (bite, sword, fire, a fight's end) pass the foe.
- `World::temperamentOf` reads `Entity::fightingSkill` for players; `World::levelOf` goes.
- Sparring itself (yield-ending bouts, trainers, posts) is doc 53's; duels that end at a yield already teach.

### 6. Doc 44's awards, mapped

| Doc 44 award | Today | Becomes |
|---|---|---|
| Work: a Gift lent | 10 XP | `gift.lend` (Wisdom) |
| Work: apprentice beside master | 10 XP a game day | `apprentice.work`, while it lasts |
| Practice: first forage, hunt kill, repair of the day | 5 XP each | `forage.pick` each picking; the kill teaches through the fight; a repair bought from a merchant teaches nothing (not the wolf's work) |
| Milestone: 25/50/75 in sneak, listening, tracking | 10 XP | A growth line (and a chronicle line, doc 56) |
| Discovery: a place first visited | 5 XP | Nothing; doc 54's exploration gives its own reward |
| Story: a contract fulfilled | 25 XP | `contract.done` (Commerce); the pay is unchanged |

`SocialLedger::award`, `World::award`, `World::takeAwards` and the game's "+N experience" loop go. Doc 44's limits per
kind become the soft limits per skill; its rested XP becomes rested practice.

### 7. Social level: social XP alone, on the account

- **Character level goes away; social level stays** (agreed, doc 48 §2.2), fed by social XP alone: scene pay, Gold and
  Story Stars, Story closures, and what doc 51 adds. Receipts stay by character (the chronicle needs that); the
  **account's** social XP is the sum over its characters.
- **Social receipts** are `qualified_session_settlement`, `gold_star`, `story_star`, `story_closure`. A fight keeps its
  receipt, so stars and Stories still treat it as a scene, but `FightXP` leaves it: a silent fight writes a zero
  receipt and teaches fighting. Doc 51 owns what talking a fight through pays.
- **Rested XP leaves social** (§4). `DailyCap` 150 stays as the social day's cap *(placeholder)*; doc 08's own limits
  (8 scenes, 100 a day of scene pay) are untouched.
- `Accounts::ownerOf(characterId)` (new reverse map, built at restore and in `addCharacter`); a dev identity is its own
  account, `dev:<id>`. `Game::socialLevel(characterId)` and `Game::socialXp` sum `social_.points` over the account's
  characters (six at most) on `standing.json`'s curve. Every `social_.level` caller moves to them (Chapter founding,
  companions, the title, the self view); docs 52 and 58 use them for mentors and storytellers.
- **Recomputed from the ledger** at load: the restore adds only social reasons to `points`, plus a `rested_bonus`
  whose sibling (same actor, source and event) is social. Old work, practice, discovery and contract receipts stay as
  history and count for nothing.
- **A per-actor index** (proposed, while the code is open): `SocialLedger` keeps each actor's receipt positions, so
  `usedToday`, `payMember` and `star` read one actor's day, not the whole ledger.
- **Who sees it:** the player (the title is self-only today). Shown to strangers, an account-wide level would link a
  player's characters as an exact star count would (doc 48 §3.6); doc 50 decides what the card shows.

### 8. Migration for existing characters

Each character carries `progressVersion` (saved); each phase that changes characters moves it on once, at load.

- **Fighting (1):** `fightingSkill = max(saved, levels::fightingSkill(level from every receipt, quickened))`: a level 10
  wolf keeps 63.5, a level 25 keeps 86. Nobody fights worse the day it ships. A value past the cap stays, unrising.
- **Social level (2):** recomputed from social receipts, so most accounts drop (work, places and contracts no longer
  count). What a level already won stays won (Chapter ranks, offices, companions). A one-time line: "Standing now counts
  roleplay alone: your account is at social level 4 (Known). Your skills keep what you earned."
- **Attributes (3):** all grades plain; values kept, even above the plain cap (they don't grow past it); no specialty.
  See Open questions.
- **Apprentices:** each old `apprentice:<day>` receipt counts as a day's `apprentice.work` toward the family of the
  position they are apprenticed to now, if any *(placeholder)*.
- **Accounts** (Phase 5): standing built from the ledger; tiers already met unlock.

### 9. Earned Gift tiers, per account (agreed, doc 48 §2.2, Decisions 26 and 36)

| Tier | Needs *(agreed placeholders)* | Measured from, today | Until the source exists |
|---|---|---|---|
| Normal | Always; a new account's first character is Normal | — | — |
| Gifted | Account social level 3, and 10 rewarded scenes on Normal characters (any wolves) | §7's level; positive settlement receipts by characters with no Gift | — |
| Quickened | Gifted, social level 8, 100 stars from 30 different wolves, 2 closed Stories, no upheld reports in 30 days | Gold and Story Star receipts (`partner` is the giver); `story_closure` receipts | Doc 51 adds milestone stars to the same counters. Reports count 0 until doc 50; the DM's **hold** stands in |

- **The record** (`Core/RatwStanding.h/.cpp`, `struct AccountStanding`): `normalScenes`, `stars`, `starGivers` (giver
  *accounts*, up to 64 kept *(placeholder)*), `closedStories` (Story IDs), `giftedAt`/`quickenedAt`,
  `giftedBy`/`quickenedBy` ("earned", "dm:<name>", "grandfathered"), `hold`. No prose.
- **Counted as it happens:** `Game::afterSocial` already walks each new receipt once. It adds to the recipient account's
  counters (a positive settlement by a Normal character; a star from another account; a closure, once per Story per
  account), then runs `standing::check`. A newly met tier is stamped, kept and told: "Wolves have noticed your
  roleplay. You may now create Gifted wolves."
- **"Different wolves" means different accounts** (proposed); stars between one account's characters don't count.
- **One wolf per account in the world at a time** (proposed): `Game::enterCharacter` refuses a second ("Another of
  your wolves is in the world. Leave them first."). Otherwise a player could put two wolves in one scene and pay and
  star themselves. `options_.oneWolfPerAccount` (default on) lets tests turn it off.
- **Kept once earned**, whatever thresholds do later; **existing characters unchanged** (agreed): an unlock is for new
  characters, and the DM still gives, changes or takes any Gift (doc 43). A wolf the DM makes Gifted stops adding
  Normal scenes.
- **The check** `standing::check(standing, socialLevel, upheldReports, thresholds)` is pure. Creation refuses a locked
  tier with its progress: "Quickened isn't open to your account yet: 62 of 100 stars." `options_.openTiers` (off by
  default) opens every tier for tests, scratch servers and the Dev Console.
- **Saved** as a save-document list (`accountStanding`) stored one row per account in `game.account_standing` by a new
  migration (the next free number; 0030's pattern, a `game.sections` row keyed by `e->>'account'`). It holds the
  account's character IDs and no verifiers, so the DM tools may read it, unlike `game.accounts`.
- **Quickened keeps the setting's cost** (agreed): rare, feared, watched by Wardens (`wardenAttention`).

### 10. Wire messages

- **Lobby** (`Game::lobby`), beside `gifts`: `creation` (attributes with each grade's start and cap, budget, costs,
  limits, specialties, presets, tier costs) and `tiers` (for gifted and quickened: `open` and `progress`, a list of
  `{measure, have, need, label}` such as "Social level 2 of 3"; the reports line only once doc 50 exists).
- **`character_create`** gains an optional `build`: `{"grades": {"str": "strong", "wis": "weak", …}, "specialty":
  "fighter" | "sneak" | "tracker" | "trade:craft" | …}`. Missing grades are plain; no `build` is all plain with no
  specialty, so old clients work. Refused: unknown attribute, grade or specialty; over budget or over the limits; a
  locked tier. `build` joins the fingerprint only when it isn't the default (as the Gift did).
- **Self view**, beside today's flat fields (kept for older readers and the DM): `attributes` (id, name, value, cap,
  grade); `skills` (id, name, value, cap, specialty, `easing` past today's soft limit); `restedPractice`; `socialLevel`
  and `socialXp` (now the account's) with `socialXpLevel` and `socialXpNext` for the bar; `tiers`.
- **Growth** is a system line; the sheet reads the self view. No new event type.
- **Dev Console** (`options_.devTools`): `{"type": "practice", "skill": "tracking", "value": 60}`, for tests.

### 11. Client

- **The creator** (`frontDoor.ts`): a **Strengths** tab between Gift and Name & age. Preset chips first ("Hunter",
  "Scholar", "Smith's hand", "Brawler", "Build my own"); seven rows, each a weak · plain · strong switch over a small
  start-and-cap bar; "Points left: 1", with an overspending switch disabled and the reason on hover; the specialty
  picker (Fighter, Sneak, Tracker, a trade). Randomise leaves it alone, as it does the Gift; the review lists it.
- **The Gift tab:** a locked card shows a lock and its progress ("Quickened · 62 of 100 stars · 1 of 2 Stories
  closed") and can still be opened to read its families; the review sends a locked choice back with what it takes.
  (The creator is a fixed layout scaled to the window, doc 43, so there is no separate phone layout.)
- **Sheet and status screen** (`dialogs.ts`): grade marks (▲ strong, ▼ weak) with caps on hover; STAMINA beside STR,
  DEX, WIS; each skill as value and cap, the specialty starred, "easing off for today", "rested". STANDING reads
  "Social level 4 · Known" with a bar to the next level, then the next tier's progress. "Level" alone leaves the header.

### 12. The Dungeon Master app

- **Players tab** (`DmApp.tsx` `COLUMNS`; `dungeon_master.py` `players()`): grade marks on STR, DEX, WIS; new columns
  Fighting, Account, Social level. The character panel shows the build, every skill with its cap and today's practice,
  and an **Account** box: social level, the measures, each tier's state (since when, by whom), the other characters.
- **`account.unlock`** `{tier: "gifted" | "quickened", op: "grant" | "revoke" | "hold" | "release"}`: queued in
  `dm.actions` against one of the account's characters (the server finds the account), audited with a reason,
  validated in `request()` like `character.gift`. Revoke is the one exception to "kept once earned" (proposed, for
  abuse); hold stops new unlocks until released, standing in for doc 50's reports.
- **Settings:** thresholds, curve and budget show read-only (read from `Data/Progression/` as `gift_families()` reads
  the Gift catalog). The user edits the files in the balance pass.

### 13. Tools for the balance pass

Each phase builds its part and records one untuned run in a "Built" section here.

- **`Tests/practice_sim.cpp`** (new, Phase 1): drives the pure `practice::gain` over simulated days for activity
  profiles (a fighter's evening, duels with a better partner, a tracker, an apprentice; four sessions a week or daily)
  and prints days to **seasoned** and **veteran** per skill and the share lost to soft limits. `SIM_PROFILE`, `SIM_DAYS`.
- **`level_sim` at bands** (Phase 2): `Wolf.level` becomes `Wolf.skill`: `@new`, `@seasoned`, `@veteran` or `@<skill>`,
  bands from `skills.json` (starts, halfway, caps *(placeholders)*). `@L20` still means `levels::fightingSkill(20)`, so
  docs 44, 45 and 47's tables re-run side by side. `levels` becomes `bands`; `trance`, `ladder` and `crowd` take "a band
  up" for "ten levels up".
- **`level_sim` grades** (Phase 4): `plain^str_wis@seasoned` (`^` strong, `_` weak); a `grades` suite sets each strength
  against plain at each band, bare and armed, and against a band gap, a gear tier and a Gifted partner.
- **Unlock pace** (Phase 5): `tools/progression_catalog.py --pace` prints what each threshold takes at doc 08's pay.
  Today: social level 3 is 250 social XP (about 13 fully paid scenes), 5 is 700 (35), 8 is 1,750 (about 88, at least
  18 days at the 100-a-day cap). Doc 44's curve was sized for XP from everything; social XP alone is slower.

## Phases

### Phase 1: The practice engine

**Goal:** one data-driven way to grow, with caps, soft limits, rested practice, anti-macro rules and growth lines, for
the skills that grow today. Levels still exist.

**Changes:**
- Data: `Data/Progression/skills.json`; `tools/progression_catalog.py` and its test.
- Server: `Core/RatwPractice.h/.cpp` (new): the loader (the `RatwGifts.cpp` pattern) and pure functions
  (`practice::gain`, room, soft limit, rested, variety). `Entity` gains `grades`, `specialty`, `skills`, `practiceDay`,
  `restedPractice`, `lastPracticeAt`, `progressVersion`, `endurance` (inert until Phase 4) and an unsaved key ring;
  `wire::entity`/`readEntity` save them, omitting defaults. `World::practise` (in `RatwProgress.cpp`) applies a source
  and queues the line through `World::notice`. `Accounts::ownerOf` and a `World::accountOf` hook (set by the game) for
  the same-account rule.
- The sneak, notice, ambush and tracking growth in `RatwBattle.cpp`, `World::smellTracks` and `World::trainNose` call
  `World::practise`. Fighting stays level-based until Phase 2.
- Wire: the self view's `attributes`, `skills`, `restedPractice`; the Dev Console's `practice`.
- Client: SENSES & SKILLS show value and cap, "easing off", "rested". DM app: skills with caps.

**Tests:**
- `Tests/practice_tests.cpp` (new, `level_tests`' pattern): slowing and stopping at a cap; the soft limit; rested
  doubling until spent, outside the soft limit; a better player within 6 tiles speeds it, a same-account one doesn't;
  partner kinds; the variety ring; same-partner decay; a line at each whole number, none inside the throttle; `wire`
  round trips.
- `battle_tests`, `hunt_tests`: sneaking, noticing and trails grow as before, now through data.
- `tools/client/practice.mjs` (new, scratch server): Smell finds trails and "Your tracking sharpened" appears; the Dev
  Console sets tracking at its soft limit and the status screen says "easing off for today". Screenshots.

**Done when:** growth lines appear in play; the status screen shows caps; the catalog check passes; `practice_sim`'s
first table is recorded here.

**Cost:** a few map lookups per gain. The better-wolf check walks `World::entitiesIn(cell)` only on a gain, cached 30
seconds per player and skill: at 1,000 players, a few dozen cell walks a second. About 1 KB per character. No tick
pass. Perf gate: `world_check --players 20` unchanged.

### Phase 2: Fighting by practice, and the simulators at bands

**Goal:** a player's fighting skill is their own and grows by fighting; nothing reads a level in a fight.

**Changes:**
- Server: `temperamentOf` reads `Entity::fightingSkill`; `World::levelOf` and its line in `RatwGame.cpp` removed;
  `growSkill` over `World::practise` at its four callers; the self view's `fightingSkill` from the entity; migration 1
  (§8) over `Game::characters_` at load.
- `Core/RatwLevels.h`: `fightingSkill(level)` stays only for the migration and `level_sim`'s `@L`, marked so.
- Simulators: `level_sim` at bands (§13); `battle_tests` stops setting `levelOf`. DM app: a Fighting column.

**Tests:**
- `battle_tests`: a blow on a live foe and a fight's end teach; a Downed or yielded foe doesn't; a resident teaches less
  than a player; the caps hold (86; a Quickened wolf's 100).
- `checkpoint_tests`: an old save's level 10 character returns with 63.5 and `progressVersion` 1; a higher saved skill
  is kept.
- `level_sim 300 bands` and `core` at `@seasoned` run; `@L` rows match doc 47's last tables within noise.

**Done when:** no fight reads a level; doc 45's yardsticks at new, seasoned and veteran are recorded here, untuned.

**Cost:** cheaper: `temperamentOf` reads a field instead of calling into the ledger. Growth is per blow. Gate
unchanged.

### Phase 3: Social level from social XP alone, on the account

**Goal:** doc 44's awards become practice; social level counts roleplay only, for the whole account.

**Changes:**
- Server: `SocialLedger::award` and social rested XP removed; `settleFight` drops `FightXP` from the receipt; the
  per-actor index. `World::award`, `takeAwards` and the game's award loop removed; each source calls `World::practise`
  (§6) or nothing. `World::tendProgress` keeps only the apprentice's practice and walks players only (a small set kept
  by `addPlayer`/`removePlayer`).
- `Game::socialLevel`/`socialXp`; every `social_.level` caller moved; `standing.json`'s curve and titles; the load
  filter (§7); migration 2 and its line.
- Client: STANDING and the header show the account's level with a true bar; "+N experience" lines go. DM app: Account
  and Social level columns.

**Tests:**
- `level_tests` reworked: only social reasons count; `rested_bonus` only with a social sibling; the account sums its
  characters; the curve from data; nothing paid for places, work or contracts.
- `social_game_tests`: scenes pay and stars give as before; a silent fight writes a zero receipt and still takes stars;
  Chapter founding reads the account's level.
- `checkpoint_tests`: an old ledger loads to social-only totals; the line shows once.
- `practice_tests`: foraging grows labour; an active apprentice at the master's side grows that trade ×3, an idle one
  doesn't; a contract grows commerce.

**Done when:** XP is paid only for scenes, stars and Stories; a new character on an established account shows the
account's level; the ledger no longer grows with every cell visited.

**Cost:** fewer ledger rows (discovery wrote one per player per new cell); settlement and stars read one actor's day,
not the ledger. Account level is six lookups. `tendProgress` walks players, not every resident. Load is one filtered
pass. Gate unchanged or better.

### Phase 4: Strengths, weaknesses and a specialty at creation

**Goal:** a new wolf is built from a small budget, from scratch or a preset; attributes grow toward their caps.

**Changes:**
- Data: `Data/Progression/creation.json`; attribute sources in `skills.json`.
- Server: `character_create` checks `build` (`practice::checkBuild`), sets starts, grades and specialty before
  `characters_` is written, and fingerprints it; the lobby sends `creation`. `endurance` takes effect in `RatwStep.cpp`,
  `battle::staminaPerTurn`, `wire::privatePace` and `World::placeByClient`'s check. Attribute sources hooked (dodge,
  shove, carry, run, notice by sight, Gift mana). Migration 3.
- Client: the Strengths tab, the review, grade marks and STAMINA. Simulators: `grades`. DM app: grades and build.

**Tests:**
- `game_tests` (`accountsAndARestart`): a preset is accepted and saved; over budget, four strengths, an unknown
  attribute or specialty refused; no `build` makes a plain wolf with the old fingerprint; a reused request ID with
  another build refused; the build survives a restart.
- `practice_tests`: weak stops at the weak cap, strong goes on; old values above a cap kept.
- `pace_tests`: endurance changes recovery and drain, and the client's prediction matches the server's.
- `Client/tests/frontDoor.browser.test.mjs`: the tab, a preset filling it, the budget blocking, the review, `build`
  sent.

**Done when:** a player can make the user's two example wolves and the four presets; the `grades` table is recorded.

**Cost:** creation and lobby only, plus two numbers in the own-wolf frame; growth per event. Gate unchanged.

### Phase 5: Earned Gift tiers

**Goal:** Gifted and Quickened open per account by the agreed measures, shown in the creator, kept once earned; the DM
can grant, revoke and hold.

**Changes:**
- Data: `standing.json`'s thresholds.
- Server: `Core/RatwStanding.h/.cpp` (record, pure `check`, progress lines); `Game::standing_` by account, counted in
  `afterSocial`; locked tiers refused at creation; `tiers` in the lobby and self view; `options_.openTiers`
  (`--open-tiers` for `tools/scratch.sh` and tests) and `options_.oneWolfPerAccount`; `account.unlock` in the DM
  action handler; an `upheldReports(account, days)` hook returning 0 until doc 50.
- Database: the `game.account_standing` migration and `game.sections` row, saved and loaded with the checkpoint.
- Migration: standing built from the ledger; tiers met unlock as earned; accounts holding a Gifted or Quickened wolf
  made at creation per the open question.
- Client: locked cards and progress; the sheet's next-tier line. DM app: the Account box, unlock buttons, thresholds.

**Tests:**
- `Tests/standing_tests.cpp` (new): each threshold short then met; Quickened needs Gifted; unlocks stay when thresholds
  rise; one account's characters count once as givers and same-account stars not at all; a Story once per account;
  hold, release and revoke; a report from the hook blocks Quickened.
- `game_tests`: a fresh account sees both tiers locked with progress and is refused Quickened; with the measures met
  through the ledger it may create one; the unlock survives a restart; a second wolf of the account can't enter;
  `openTiers` lets the old Quickened Blinker test through.
- `checkpoint_tests`: the section's round trip; standing rebuilt from an old ledger.
- `tools/test_dungeon_master.py`: `account.unlock` validated and queued; `players()` returns account and standing.
- `Client/tests/frontDoor.browser.test.mjs`: locked cards, progress text, the review refusing a locked choice.

**Done when:** a new account makes only Normal wolves and sees what each tier takes; the measures open them in play;
the DM can see and change unlocks; the `--pace` output is recorded here.

**Cost:** counters move in `afterSocial`, which already walks each new receipt; `check` runs only on a change. One
small row per account, written when it changes (section deltas). Giver sets capped. Nothing in the tick. Gate
unchanged.

**Order:** 1, 2, 3 in turn (fighting must leave the level before non-social XP stops feeding it); then 4 and 5 in
either order. Doc 48's Plans say unlocks "need only stars and social level, so they can come right after 51", but the
social level they need is this plan's (Phase 3: on the account, from social XP alone). Phase 5 doesn't need doc 51: it
counts today's stars, and doc 51 later feeds the same counters.

## Depends on and feeds

- **Replaces** doc 44's level and typed awards. When built, update doc 43's "free for now" row, doc 32 §1.3's "earned
  tiers stay out", and doc 08's account note.
- **Reads** docs 08 (scenes, stars, Stories), 26 (apprentices), 40 and 41 (senses, skills), 43 (the Gift tab,
  `character.gift`). **Re-runs** docs 45 and 47's yardsticks at bands, for the user.
- **Waits on** doc 50 for upheld reports (the hold stands in) and doc 51 for milestone stars and the star display (both
  feed `AccountStanding`).
- **Feeds** `World::practise` and the skill catalog to doc 53 (sparring, partner kinds, lead and hand, hunting), doc 54
  (rested rate, game and scholar skills, exploration instead of discovery XP) and doc 52 (mentors as teachers);
  `Game::socialLevel` to docs 52 (newcomer flag, mentors at 5) and 58 (storytellers at 5); growth and unlocks to doc
  56's chronicle.

## Risks

- **Social levels drop at the switch.** What a level won stays; a one-time line; Open question 3.
- **Fights shift once skill grows by fighting:** a wolf that fights a lot outgrows a high-level wolf that never fought.
  The migration keeps today's skill; band tables before and after.
- **Gift balance was tuned at levels** (doc 45's Trance). `@L` reproduces the old tables; the user re-tunes at bands.
- **Macroing.** Soft limits, variety, partner decay and real-use conditions make it pointless, not impossible;
  `practice_sim` shows what a scripted grinder gains.
- **Min-maxing at creation:** presets, modest caps and the `grades` suite. **Alts farming unlocks:** one wolf per
  account in the world; givers counted by account.
- **Two places counting stars:** one counter set, in `AccountStanding`; doc 51 writes to it.
- **Hot shared files** (`RatwWorld.h`, `RatwGame.cpp`, `RatwBattle.cpp`, `RatwWire.cpp`): small hunks, commit only your
  own (doc 45's traps). Nothing here edits the economy session's files (`RatwOrchestrator.*`, `RatwDemand`,
  `RatwOddJobs`, `RatwResidents`, `RatwSociety.h`, `Data/Economy/`); it only reads `Society::apprenticedTo`.
- **Tests that assume free tiers** (`accountsAndARestart`, the front-door test, scratch runs of `gifts.mjs`) get
  `openTiers` or the dev `gift` command in Phase 5's commit. **The walking prediction** (doc 31) gets the stamina rates
  in the same commit as `endurance`.

## Decisions

### Agreed (doc 48)

1. No character levels; social level stays, fed by social XP alone: scenes, stars, Stories (§2.2; Decisions 25).
2. Strengths and weaknesses at creation over STR, DEX, WIS, stamina, hearing, vision and smell, plus one specialty;
   presets "Hunter", "Scholar", "Smith's hand", "Brawler"; strengths start and cap higher, weaknesses lower (§2.2).
3. Practice: faster beside a better wolf and against a live partner, slower near the cap, a daily soft limit,
   meaningful use only; typed awards and rested time carry over as practice; gains shown as they happen (§2.1, §2.2).
4. Fighting skill grows by fighting and sparring again (§2.2).
5. Gift tiers earned per account: Normal always; Gifted at social level 3 and 10 rewarded scenes on Normal characters,
   no count of different wolves; Quickened with Gifted, social level 8, 100 stars from 30 different wolves, 2 closed
   Stories, no upheld reports in 30 days; kept once earned; existing characters unchanged; locks and progress in the
   creator; the DM still gives and takes Gifts (§2.2; Decisions 26, 36).
6. Every number is a placeholder; the user balances later (§2.2).

### Placeholder choices made here (proposed)

7. Numbers live in `Data/Progression/` (skills, creation, standing), checked by `tools/progression_catalog.py`.
8. Stamina is a new attribute, `endurance`, setting the bar's recovery and drain; the bar stays 0–100.
9. One fighting skill (the Fighter specialty), over today's level range: 50 to 86, a Quickened wolf's to 100.
10. Player trade skills use the residents' families (craft, labour, commerce, service, travel, watch).
11. Only players and the resident teaching count as a better wolf; never the same account's characters.
12. Bought repairs and places first visited teach nothing; contracts teach commerce.
13. A silent fight's 10 XP leaves social and teaches fighting (the zero receipt stays); rested XP becomes practice.
14. Migration keeps the fighting skill a level gave, recomputes social level, and leaves won things won.
15. "Different wolves" counts accounts; one wolf per account in the world at a time.
16. The DM can grant, revoke and hold an account's unlocks; holds stand in for reports until doc 50.

## Answered (the user, 2026-10-06)

There are no real players yet, only the user and a test account, so nothing needs carrying over:
1. **Existing characters** get no one-time choice of strengths and weaknesses. They stay plain.
2. **No grandfathering:** accounts that made a Gifted or Quickened wolf while it was free don't keep that tier open.
   Unlocks are earned like anyone's.
3. **Social level at the switch:** no carry-over. It is recomputed from social receipts with no special handling, and
   the one-time line in §8 is dropped.

4. **One wolf per account in the world at a time:** yes.
5. **Quickened opens before reports exist,** with the DM's hold standing in: yes ("it's just me and this is a test
   environment").

And: "Don't bother preserving anything, I'll just remake a character." So no migration of any kind (§8): Phase 2
doesn't carry a level's fighting skill over either. Existing characters keep whatever their save says, and the user
remakes one.

## Open questions

1. **Existing characters:** offer each a one-time choice of strengths, weaknesses and a specialty, changing caps and
   raising (never lowering) starts? *Recommended: yes, once.*
2. **Existing accounts** that made a Gifted or Quickened wolf while it was free: keep that tier open for them?
   *Recommended: yes (it was allowed when they chose it).*
3. **Social level at the switch:** recompute from roleplay alone (most levels drop, nothing won is lost), or count
   the XP earned so far as social, once? *Recommended: recompute.*
4. **One wolf per account in the world at a time**, to keep scenes, stars and unlocks honest? *Recommended: yes.*
5. **Quickened before reports exist** (doc 50): open it with the DM's hold standing in, or wait for reports?
   *Recommended: open it, with the hold.*

## Built

### Phase 1: the practice engine (built 2026-10-06, not committed)

- **Data:** `Data/Progression/skills.json` holds:
  - 7 attributes and 10 skills, each with its field, start, cap, soft daily limit and growth line;
  - 7 sources: `sneak.world`, `sneak.arena`, `sneak.ambush`, `notice.sound`, `notice.scent`, `track.found` and
    `nose.use`;
  - the partner, teacher, rested, variety, decay and line rules;
  - the simulators' bands (seasoned 50%, veteran 90% of the way from a skill's start to its cap).

  `tools/progression_catalog.py` checks the file, with `tools/test_progression_catalog.py` (6 tests).
- **The engine:** `Core/RatwPractice.h/.cpp` holds the catalog and the pure arithmetic: `room`, `soft`,
  `partnerFactor`, `variety`, `partnerDecay`, `rested`, `lineDue` and `lineText`. One *occasion* is either everything
  within a minute of the last practice of the same key, or everything under one name (a fight's ID). So practice
  that comes every few tenths of a second, like creeping past a resident, counts as one occasion, not hundreds.
- **The world:**
  - `World::practise(who, source, context)` lives in `RatwProgress.cpp`, with `practiceSlot`, `practiceValue`,
    `practiceCap`, `setPractice` and `teacherNear`. `teacherNear` looks for a better player within 6 tiles and caches
    the answer 30 seconds per player and skill.
  - Two hooks are set by the game: `realClock` (`Game::now`) and `accountOf`.
  - `Accounts::ownerOf` is backed by an owners index that `restore`, `addCharacter` and `removeAccount` keep.
  - `Entity` gains `grades`, `specialty`, `skills`, `endurance`, `progressVersion` and `practice`. All are saved
    except the occasion ring and the line throttle; defaults are left out of saves.
- **Growth moved onto practice:**

  | Source | Where | Context |
  |---|---|---|
  | `sneak.world` | `World::senseInWorld` | the resident as partner |
  | `notice.sound`, `notice.scent`, `sneak.arena` | `World::senseOne` | the fight's ID as the occasion |
  | `sneak.ambush` | `World::sprungOn` | — |
  | `nose.use` | `World::trainNose` | — |
  | `track.found` | `World::smellTracks` | — |

  The old constants are gone: `SneakPerAmbush`, `SneakUnnoticed`, `SneakUnnoticedWorld`, `NoticeTeaches`,
  `SmellMost` and `SmellGrowth`.
- **Wire:**
  - `persistEntity` and `readEntity` save the new fields; unknown grades and skills are dropped.
  - `wire::practiceView` gives the self view `attributes`, `skills` (with `easing`) and `restedPractice`.
  - The Dev Console takes `{"type": "practice", "skill", "value", "today"}`.
- **Client:**
  - The Status window lists each skill out of its cap, with an "EASING OFF TODAY · …" line and a "RESTED" line.
  - The character sheet shows skills out of their caps. Older servers still work.
- **DM app:** a Practice line under each character, with each value out of its cap (`practice_skills` and
  `practised` in `dungeon_master.py`).
- **Tests:**
  - `Tests/practice_tests.cpp`: 135 checks.
  - `hunt_tests`: the nose test now grows to the catalog's cap.
  - `tools/client/practice.mjs`: 8 checks, with screenshots in `artifacts/screenshots/practice/`.
  - `ctest`: 47 of 47 pass. The client's tests and `tools/test_dungeon_master.py` (42 tests) pass.

**Cost:**
- Practice adds no pass to the tick: it runs only when a wolf practises.
- Perf gate on DEV build 26: `world_check --simulate 12 13 --players 20 --no-check --deterministic` gave a mean tick
  of 11.8 ms, p99 25.9 ms and p99.9 44.7 ms (8 threads), inside the 50 ms budget.
- There was no clean before-and-after: the only other build to hand, `build-core`'s, is from 2026-10-04 and too old
  for `--deterministic`.

**What plays differently now** (all placeholders):
- A plain wolf's nose stops at 145%; before, every wolf's grew to 160%.
- Tracking from sniffing and from trails slows near the cap; before, it rose by a flat 0.1 and 0.3.
- Each skill has a daily soft limit, and the same practice repeated in ten minutes counts for less.

**Not yet, by design:**
- Growth from noticing by sight (Vision) and the tenth of sneaking that goes to Dexterity come in Phase 4, with the
  other attribute sources.
- Partner kinds change only sources marked `byPartner` (the fight sources of Phase 2), so sneaking and noticing grow
  at today's base rates, apart from room, the soft limit and variety.
- Fighting still comes from the level until Phase 2. Its row is in the catalog but not on the client's skill list.

**`practice_sim`, first run (untuned; 365 days, 4 evenings a week, no teacher):**

```text
tracker: an evening in the wild nosing for game: 20 sniffs a minute apart, 6 trails found
  skill          start       cap   seasoned    veteran     soft held   rested
  smell           1.00      1.45     day 11     day 23           11%      39%
  tracking           0       100     day 23     day 59            8%      45%
stalker: five hunts stalking game: unnoticed close by every 10 s for 2 minutes, then an ambush
  sneak              0       100     day 22     day 45           18%      45%
prowler: ten minutes creeping about town past residents (a check every 0.4 s, a new resident every 2 minutes)
  sneak              0       100     day 15     day 29           44%      41%
watcher: a stalker caught twice by ear and once by nose
  listening          0       100    day 102    day 338            0%      49%
  tracking           0       100    day 205      never            0%      49%
```

What it shows, for the balance pass:
- **Rested practice is large:** about 45% of all gains for a four-evenings-a-week player. A pool of 5 a day away is
  big next to a few points of growth a day.
- **Dedicated practice is quick:** a dedicated tracker or stalker reaches "veteran" in one to two months.
- **The soft limits barely bite** except on prowling (44%).
- **Noticing alone is slow:** a year to veteran Listening.
- Played every day, rested practice drops to nothing, and the times barely change.

### Phase 2: fighting by practice (built 2026-10-06, not committed)

- **No fight reads a level.** `World::temperamentOf` reads a player's own `Entity::fightingSkill`, and the self view
  sends the same. `World::levelOf` and the game's hook for it are gone.
- **Fighting grows by fighting:** `World::growSkill(Battle&, learner, foe, source)` sits over `World::practise`. There
  are two new sources in `skills.json`:
  - `fight.blow` (0.2): a bite, cut or fire that lands. It teaches only on a foe still able to fight. The foe is the
    partner, scaled by kind: a player ×1, a resident ×0.8, a fierce animal ×0.6, game ×0.3. A foe 15 weaker teaches
    ×0.5; one 10 better, ×1.5. The fight is the occasion, so the same foe in another fight the same day teaches half,
    then a quarter, then nothing.
  - `fight.end` (0.5): being in a fight of two turns or more to its end, whether standing, yielded or Downed. The
    user: "Let learning happen even if the wolf isn't conscious at the end." A wolf that fled learns nothing from it.

  `battle::SkillPerHit` and `SkillPerFight` are gone. The cap is 86, or 100 for a Quickened wolf.
- **No migration**, at the user's word: a character's saved fighting skill is what it fights with. The user will make
  a new character.
- **Simulator:** `level_sim` no longer touches levels in the world. Each wolf's `fightingSkill` is set directly, and
  `World::practising = false` keeps any wolf from growing during simulated fights. The syntax:
  - `@20` (or `@L20`) still means the skill level 20 gave, so docs 44, 45 and 47's tables re-run unchanged. The
    `levels` suite reproduces doc 44's table: L5, L10 and L25 against L1 win 55%, 64% and 90%, against 55%, 64% and
    88%.
  - New: `@new`, `@seasoned` and `@veteran` (the bands in `skills.json`), and `@s70` for a set skill.
  - New suite: `bands`.
- **Client and DM app:**
  - The Status window's FIGHTING shows its cap ("of 86").
  - The DM app gets a Fight column, and fighting joins the Practice line.
- **Tests:**
  - `practice_tests`: 152 checks, including how a blow teaches by foe, the same foe again, the fight's end, both caps
    and practice switched off.
  - `battle_tests`: fighting now raises the player's own skill, and that is what a fight reads.
  - `ctest`: 47 of 47 pass. The client's tests (98) and `tools/test_dungeon_master.py` pass.
  - `tools/client/gifts.mjs`: eight families' duels in the real page, with no page errors.
- **Found, not caused by this phase:** `tools/client/fight.mjs` is out of date. It predates the 30-second positioning
  phase (doc 40) and never presses Ready, so it times out waiting for the fight. `gifts.mjs` handles positioning.

**`level_sim 300 bands`, first run (untuned; bands from `skills.json`: new 50, seasoned 68, veteran 82):**

```text
  new vs new                                                  47.7%
  seasoned vs new                                             68.7%
  veteran vs new                                              85.7%
  veteran vs seasoned                                         64.0%
  new vs seasoned, new strikes first                          29.3%
  new vs veteran, new strikes first                           12.7%
  new armed vs veteran bare                                   61.0%
  new armed vs veteran armed                                  11.7%
  two new vs one veteran                                     100.0%
  two new vs one seasoned                                    100.0%
  Q-fire @new vs plain @new                                   62.0%
  Q-earth @new vs plain @new                                  77.3%
  Q-fire @veteran vs plain @veteran                           79.0%
  Q-earth @veteran vs plain @veteran                          89.7%
```

**For the balance pass:**
- A veteran is worth about what a level 25 was against a level 1: 86%, against 88–90%.
- Gear still beats a skill gap: an armed newcomer beats a bare veteran 61% of the time.
- Numbers decide outright.
- Striking first barely helps the weaker wolf.

### Phase 3: social level from roleplay alone, on the account (built 2026-10-06, not committed)

- **Social XP is scenes, stars and Stories alone.** The receipts that count are listed in
  `Data/Progression/standing.json`'s `socialReasons`: settlements, Gold Stars, Story Stars and Story closures.
  - `SocialLedger::award`, `restedLeft` and rested XP are gone.
  - A fight pays no social XP for fighting (`FightXP` is gone). Fighters who stay silent get a zero receipt, so stars
    and Stories still take the fight as a scene. Talking a fight through pays as before (doc 51 owns that rate).
  - The social day's cap comes from `standing.json` (150).
- **The curve and titles are in `standing.json`:** `practice::xpFor`, `levelFor`, `titleFor` and `socialReason`.
  `socialTitle` reads it, and `levels::fightingSkill` survives only for `level_sim`'s `@20` rows.
- **The account's standing:** `Game::socialXp` and `Game::socialLevel` sum the social XP of every character on the
  account (a development identity's wolf counts alone). Chapter founding, friendship companions, the title and the
  self view all read it. The self view adds `socialXpLevel` and `socialXpNext` for the bar.
- **At load,** only social receipts count toward a character's total. Old work, practice, place, contract and rested
  receipts stay in the ledger as history. As the user asked, nothing is carried over and no one-time line is shown.
- **The ledger's per-actor index:** `SocialLedger::reindex`, `receiptsOf` and a private `add`. A scene's pay, the
  day's total, a star's checks and a Story Star's checks now read one actor's receipts, not the whole ledger.
  `paidIn(session)` still walks it; doc 51 reworks scenes.
- **Doc 44's awards became practice:**
  - `World::award`, `takeAwards` and the game's "+N experience" loop are gone.
  - Foraging grows labour (`forage.pick`), a contract commerce (`contract.done`), and a Gift lent wisdom
    (`gift.lend`).
  - An apprentice beside their master grows the master's trade family (`apprentice.<family>`, ×3), but only while the
    player is at the keys: an action, a chat or a step in the last five minutes, through the new
    `World::playerActive` hook.
  - Places first visited, skill milestones, hunt kills and bought repairs pay nothing. Growth lines tell of skills
    instead.
- **Client:**
  - The sheet and Status window read "Social level N · Title", with a bar from the level's start to the next.
  - The line reads "N social experience · M to the next level", and a hover explains that it comes from roleplay and
    is the account's.
- **Tests:**
  - `level_tests` was rewritten (57 checks): the curve and titles from data, social receipts alone, the index and the
    cap.
  - `practice_tests` (157): the trade sources and the master's ×3.
  - `game_tests`: 300 social XP on one of an account's wolves puts all of them at level 3; a wolf with no account
    stands alone.
  - `social_game_tests`: a silent fighter pays nothing but keeps a receipt; Bo's total no longer includes the 5 for a
    place.
  - `checkpoint_tests`: old XP receipts load as history.
  - `progression_catalog.py` checks `standing.json` (8 tests).
  - `tools/client/practice.mjs` (10 checks): the sheet's social level, and no "+N experience" lines.
  - `ctest`: 46 of 47. The one failure, `roads_tests` ("A penniless one doesn't"), is the economy session's work in
    progress: it edited `RatwDemand.cpp`, `RatwSociety.h` and that test minutes before the run.
- **Moved to Phase 5:** the DM app's Account and Social level columns. The tools can't read `game.accounts` (it holds
  password verifiers), so they wait for Phase 5's `game.account_standing`, which the tools may read.

### Phase 4: strengths, weaknesses and a specialty at creation (built 2026-10-06, not committed)

- **Data:** `Data/Progression/creation.json` holds:
  - each attribute's weak, plain and strong start and cap (plain matches `skills.json`);
  - the budget (2), costs and limits (at most 3 of each);
  - nine specialties: Fighter, Sneak, Tracker and six trades;
  - the four presets (Hunter, Scholar, Smith's hand, Brawler);
  - a cost per Gift tier (0 for now);
  - what Stamina does.

  `tools/progression_catalog.py` checks it: grades in order, presets within the budget, specialties naming real
  skills.
- **The engine:**
  - `practice::creation`, `specialty` and the stamina effects (`staminaRecovery`, `staminaDrain`,
    `staminaPerTurnExtra`, each unchanged at 50).
  - `capFor` takes a grade.
  - `Core/RatwCreation.h` holds the JSON-facing parts (`checkBuild`, `creationCatalog`). They are kept out of
    `RatwPractice.h` so that `RatwWorld.h` doesn't bring the JSON types into every file (they clashed with the economy
    session's `Tests/econ_watch.cpp`).
  - `World::applyBuild` sets each grade's start and the specialty's starting skill.
  - `World::practiceCap` reads the wolf's grade.
- **Creation:**
  - `character_create` takes an optional `build`, which is checked and refused with a reason: over the budget, too
    many of a kind, an unknown attribute, grade or specialty, or an unknown field.
  - A build that isn't plain joins the request's fingerprint, so a reused request ID with another build is refused.
    A plain wolf's fingerprint is unchanged.
  - The build is applied before the Gift, so mana follows Wisdom.
  - The lobby sends `creation`.
- **Stamina, the attribute (`Entity::endurance`):**
  - The bar comes back ×(0.8 + 0.004 × stamina), and running drains it ×(1.2 − 0.004 × stamina). Both are 1 at 50.
  - A fight turn gives back 0.04 × (stamina − 50) more.
  - The self view sends `staminaRecovery`.
  - `step::updateStamina` now lets a drain below 1 and a recovery above 1 through. The client doesn't predict stamina,
    so the walking WebAssembly needs no rebuild.
- **Attributes grow by practice** (sources in `skills.json`):
  - Dexterity: a dodged blow (`fight.dodge`), and a tenth of sneaking.
  - Strength: a shove that lands (`fight.shove`), and each 100 tiles under a heavy load (`load.carry`).
  - Stamina: each 200 tiles run (`run.far`).
  - Hearing, Smell and Vision: noticing a stalker by ear, nose or eye.
  - Wisdom: each 10 mana a fight Gift spends (`gift.mana`, through a wrapper round `useGift`).

  `Context.amount` scales a source.
- **Client:**
  - The creator has a **Strengths** tab between Gift and Name & age. It offers the preset chips and "Build my own";
    seven weak · plain · strong switches, each showing start → cap; the points left, with any overspending choice
    disabled and the reason on hover; and the specialty chips.
  - The review lists the build, which is sent only when it isn't plain.
  - The Status window marks grades (▲ strong, ▼ weak), shows each attribute's cap, and adds a STAMINA box.
  - Screenshot: `artifacts/screenshots/practice/3-creator-strengths.png`.
- **DM app:** the STR, DEX and WIS columns carry grade marks, and the character panel gets a Build line.
- **Simulator:** `level_sim` reads grades after the kind ("plain^str_wis@seasoned") and has a `grades` suite.
- **Tests:**
  - `practice_tests`: 188 checks, covering `checkBuild` (the presets, the user's two examples, over-budget and limit
    refusals), `applyBuild` starts and caps, weak caps holding, old values kept, the stamina effects and the creator's
    copy.
  - `game_tests`: a Brawler made and saved with its starts; over the budget and an unknown specialty refused; the same
    request ID with another build refused; the plain first wolf unchanged.
  - The front-door browser test: the Strengths tab, a preset, the budget, a weakness giving a point back, the review,
    and the build sent or not.
  - `ctest`: 47 of 47. The client's tests (98), browser tests (6), DM-host tests and catalog checks pass.
- **No migration**, as the user asked: existing characters stay plain.

**`level_sim 300 grades`, first run (untuned; the same at every band, since only the gap in skill matters):**

```text
  plain^str vs plain        62.3%      plain_str vs plain         35.0%
  plain^dex vs plain        80.0%      plain_dex vs plain         17.0%
  plain^sta vs plain        47.0%      plain^wis vs plain         47.0%
  armed ^str vs armed       66.7%
  ^str @new vs @seasoned    36.3%      ^dex @new vs @seasoned     55.3%
  ^str @seasoned vs armed   12.0%      ^str vs a plain + Seer pair 0.0%
```

**For the balance pass:**
- Dexterity is worth far more than anything else in a fight: strong 80%, weak 17%. A strong-Dexterity newcomer
  beats a seasoned wolf.
- Strength is moderate.
- Stamina and Wisdom do nothing in a bare duel. Wisdom matters only through Gift mana.
- Gear and numbers still dominate.

### Phase 5: earned Gift tiers (built 2026-10-06, not committed)

- **Thresholds** are in `Data/Progression/standing.json`'s `unlocks`:
  - Gifted: social level 3 and 10 scenes on Normal wolves.
  - Quickened: social level 8, 100 stars from 30 different wolves, 2 closed Stories, and no upheld reports in 30 days.
  - At most 64 givers are counted.

  `tools/progression_catalog.py` checks them.
- **The rules:** `Core/RatwStanding.h/.cpp` is the pure part: `Measures`, `Thresholds`, `Record`, `meetsGifted`,
  `meetsQuickened`, `open`, `progress` and `lockedMessage` (for example, "Quickened isn't open to your account yet:
  Stars: 62 of 100; Stories closed: 1 of 2.").
- **The game** (in `RatwGameSocial.cpp`):
  - `Game::measuresOf(account)` counts from the ledger: the account's social level; paid scenes on its wolves that
    have no Gift; stars its wolves received from other accounts' wolves, and how many accounts gave them (a wolf with
    no account counts as its own); and the Stories its wolves saw closed.
  - `checkUnlocks` opens a tier that is met, stamps it "earned", tells the account's online wolf, and saves soon,
    unless the account is held. It runs after each social receipt (`afterSocial`) and for every account at load.
    Unlocks are kept once earned.
  - `upheldReports` returns 0 until doc 50 builds reports; the DM's hold stands in.
- **Creation:**
  - A tier the account hasn't opened is refused with `lockedMessage`. Quickened needs Gifted open too.
  - The lobby sends `tiers`: each tier open or not, who opened it, the progress lines and the message.
  - The self view sends the account's `tiers`.
- **One wolf per account in the world** (agreed): `enterCharacter` refuses an account's second wolf ("Another of your
  wolves is in the world. Leave them first."). `Options::oneWolfPerAccount` is on by default.
- **Opening every tier:** `Options::openTiers`, through the server's `--open-tiers` (pass it to `tools/scratch.sh`),
  opens every tier for tests and scratch servers.
- **The DM:**
  - `account.unlock` (`{"op": "grant" | "revoke" | "hold" | "release", "tier": "gifted" | "quickened"}`), queued
    against one of the account's characters. It goes through `Game::unlockTier`, which tests and tools can call too.
  - A revoke stands while the account is held; on release, an earned tier opens again.
  - The DM host validates it (`tools/dungeon_master.py`).
  - The Players tab has Account and Social columns, and the character panel shows the account's tiers, measures and
    hold, with Grant Gifted, Grant Quickened, Revoke Quickened and Hold/Release buttons.
- **Saving:**
  - The save document gets `standing.accounts`: one entry per account, with its character IDs, when each tier opened
    and by whom, the hold, and the last measures. It holds no verifiers.
  - `Database/migrations/0034_account_standing.sql` gives it its own table, `game.account_standing`, through
    `game.sections`, so the tools and the DM may read it.
  - **The migration isn't applied to DEV or PROD yet** (`python3 tools/world_db.py migrate`). Until it is, the list
    rides in the checkpoint row, and the DM's account columns stay empty.
- **Client:**
  - Locked Gift cards show a lock and their progress lines, and can still be opened to read their families. The
    review refuses a locked tier with the server's message.
  - The sheet's Standing shows "NEXT · GIFTED WOLVES: …".
  - Screenshot: `artifacts/screenshots/practice/4-creator-locked-tiers.png`, from a fresh account on a real server.
- **Tests:**
  - `level_tests` (65): the thresholds, `meetsGifted` and `meetsQuickened` (a report blocks it), and the locked
    message.
  - `game_tests` (2,760):
    - a fresh account sees both tiers locked with their progress and is refused them;
    - Quickened needs Gifted;
    - DM grants open them, a hold and a held revoke work, and an unknown tier or account is refused;
    - an unlock survives a restart;
    - ten paid Normal scenes at level 3 earn Gifted, and level 8, 100 stars from 30 wolves and two Stories earn
      Quickened;
    - an account's second wolf can't enter while the first is in the world.
  - `tools/test_dungeon_master.py` (43): `account.unlock` validated and queued, and the account's standing on the
    Players tab. The test databases apply migration 0034.
  - The front-door browser test (7): locked cards, their progress, families still readable, and a locked choice
    refused at review.
  - `ctest`: 47 of 47. The client tests (98) and catalog checks pass.

**Plan 49 is built.** Every number is a placeholder in `Data/Progression/` for the user's balance pass, with
`practice_sim`, `level_sim`'s `bands` and `grades` suites, and the catalog checker to help with it.
