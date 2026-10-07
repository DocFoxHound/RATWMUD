# 51. Scenes and stars: being thanked, being seen, and the gathering howl

Drafted 2026-10-06 as an actionable plan for doc 48 (§3.6, Part 4, §8.1 and §3.5's gathering howl). Open questions
answered 2026-10-07 (below); Phase 1 under way.
Read doc 48 (Principles, §3.5, §3.6, Part 4, §8.1, Decisions) and docs 08 (scenes and "The scene line"), 32 (§1.1–1.2,
§1.5 and Phase 4), 33 and 37 (the fight's end), 44 (XP awards), 29 (the minimap) and 31 (cost) first, with
Docs/References/SOCIAL_PROGRESSION_ROLEPLAY_TRACKING.md §7.

Marks: **(agreed)** is the user's decision, with the doc 48 section it comes from; *(placeholder)* is one constant to
change; **(new)** is a choice this plan makes that the user hasn't seen.

## The ask

From doc 48:

> "Reduce the XP bonus while roleplaying during a fight, instead lets lean into stars. Maybe we can show player's
> total earned stars somewhere on their character sheet? [...] This would be account-bound, not character-bound."

> "I think it'd be good for all players to see what quality of roleplayer they're getting involved with."

> "Groups roleplaying should be noticeable. [...] I want to make it easy for other characters to hop in if they want
> to."

> "Ending a fight or a roleplay session brings up the star screen, what if we also had a stats screen that showed
> involvement in both?"

And §3.5's gathering howl: a wolf howls to say "come and find me", and others can join it.

## Where we stand (read from the code 2026-10-06)

**Scenes** (`Core/RatwSocialCore.h`, `.cpp`; doc 08):

- `SocialLedger::record` routes each line. It takes the **first** scene in the cell with the same `party` tag (a party
  ID, a fight tag, or empty for the room), so there is one room scene per cell. Party mates in earshot open the
  party's scene at once; an outsider who answers a party member (heard them in the last 30 s, and is heard by them)
  joins it; anyone a room scene's member hears joins the room scene; otherwise an A–B–A exchange forms a new one.
- `settle`, `leave`, `payMember` (20/12/5, repeated-partner decay), `joinFight`, `settleFight` and `tick` (30 quiet
  minutes) do the rest. `SocialSession` has no openness, no admitted list and no record of what happened beyond counts.
- **Nothing is pruned.** Ended sessions, ledger entries and stars stay in memory and the checkpoint for good;
  `payMember`, `star` and `usedToday` scan all of `entries`, and `payMember` every session too.

**The scene line** (doc 08, built 2026-10-04): `Client/src/ui/hud/story.ts` (`StoryPanel.updateScene`) draws
`self.social.scenes` and `self.social.ended`. `Game::refreshSocialViews` (`Core/RatwGameSocial.cpp`) builds them every
2 s by looping over **every session for every client**: O(clients × all sessions ever).

**Stars:** `SocialLedger::star` gives one Gold Star per recipient per scene, within a day of its end, between two who
qualified, worth min(2, their pay) × pair decay (1, ½, ¼, then nothing over a rolling day), the giver's first 10 a
day paying. `storyStar` gives one per giver per closed Story, worth 4 × decay. `Game::socialCommand` (verbs `star`,
`storystar`) also warms the bond. `SocialStar` holds **character IDs only**, saved as `socialStars` in the
checkpoint row. Nothing totals, shows, tags or links stars to an account, and `giver != recipient` is checked by
character, so one account's two characters could star each other today.

**Accounts:** `accounts::Accounts` (`Core/RatwAccountsCore.h`) maps a username to up to 6 characters. There is **no
reverse lookup** from a character to its account (a connection knows its `accountUsername`; an offline character's
owner must be searched for). Every social figure (`points`, `level`) is per character.

**Fights:** `settleFight` pays `FightXP` (10) to those who took two turns and `FightTalkFactor` (2) × a scene's pay to
those who talked it through (`Tests/social_game_tests.cpp` `fightScenes`). The result card (`Client/src/ui/hud/
combat.ts`, `summarise` and `buildResult`) sums "dealt" and "taken" from the log's figures, but the log keeps 60 lines
(`battle::BattleLogKept`), so long fights are under-counted. `BattleFighter` (`Core/RatwBattle.h`) counts only
`turnsTaken`. The review says "roleplay in a fight pays twice".

**Speech:** `Game::publish` (`Core/RatwGame.cpp`) sends each listener a `roleplay` event with `party` or `chapter`
flags, no author ID, and the speaker as that listener knows them (`Game::labelFor`). The client (`state.ts`) fades only
"words too far off to make out". Nothing marks a line as part of a scene.

**Sound:** `World::hearingClarity`: whisper 2 tiles, speech 16, yell 32, times hearing and weather; a yell crosses
doors. Nothing carries across open country. No howl exists (only the Quickened Gift `shatterhowl`, in fights).

**Minimap** (`Client/src/game/minimap.ts`, doc 29 Phase 8): known outdoor places within 512 tiles at
`Cell::worldX/worldY`, with doors, visible wolves, the Chapter's marks and party mates. Interiors aren't drawn.

**Name clash:** `Core/RatwScenes.*` is doc 30's library of *written ambient scenes*, not social scenes.

**Missing:** account totals, tags, bands, openness, join and knock, scene marks in the log and on the map, end-screen
moments and fight figures, the howl.

## Scope

This plan builds stars as an account-bound record (totals, bands, spread, tags, Known for, an optional rate) and the
fight roleplay cut; scene openness with joining and knocking and several room scenes in one place; scene bars in the
log, a "my scene only" filter and open scenes on the minimap; end screens for fights and scenes; the gathering howl
and chorus.

It leaves to other plans:

- **Doc 50:** the player card, roleplay status, friends (and whether a friend may see which character is theirs),
  block and mute, circles, known wolves and scene summaries. This plan supplies the card's `stars` block and uses
  doc 50's `friendSeesCharacter`, `blocks` and `statusOf`.
- **Doc 52:** the newcomer flag (Welcoming, the "new" mark on joining) and mentor standing.
- **Doc 49:** account social level, Gift unlocks (it reads this plan's totals), and what `FightXP` becomes without
  levels.
- **Docs 55 and 56:** gifts, grooming and deeds as scene moments. **Doc 53:** whether game animals react to a howl.
- **Doc 58:** milestone stars (credits screens) and tale stars (a storyteller's story), which go into this plan's
  star book.

## Design

### 1. The star book

A new pure class, `StarBook` (`Core/RatwStars.h`, `.cpp`), keyed by **account**. `SocialLedger` keeps deciding who may
star whom and the XP a star pays; the star book keeps the account-bound record.

- **A star:** `id`; `kind` (`gold`, `story`, `milestone` and `tale` from doc 58); `source` (scene, Story, milestone or
  tale); giver and recipient, each as account and character; `tag`; `at`; XP paid; `counted`.
- **An account's tally** (`StarTally`): `total` (counted stars), counts by kind and tag, the distinct giver accounts,
  the last 30 days of stars from each giver, and `chances` and `goldReceived` for the rate.
- **Every Gold Star, Story Star and milestone star counts** (agreed, doc 48 §3.6), and tale stars with them, each as
  one whatever XP it paid.
- **Not from oneself:** a star between two characters of one account is refused **(new)**.
- **What counts toward the total (new).** Doc 48 says pair decay keeps two friends from starring each other forever,
  but decay resets every rolling day. So a star counts only if it is among the giver's first 10 of the rolling day,
  at most the 3rd from that giver account to that recipient account in a rolling day, and at most the 10th between
  that pair of accounts in 30 days *(placeholders)*. A star that doesn't count is still given, thanked and warms the
  bond; only the total ignores it.
- **The spread** counts distinct giver **accounts**, so alts can't widen it **(new)**. Doc 49's Quickened gate ("100
  stars from at least 30 different wolves", agreed) reads it.
- **Account lookup:** `Accounts::ownerOf(character)`, a reverse index built in `restore` and `addCharacter` (doc 50
  may add it first, for friends).

**Who sees what** (agreed, doc 48 §3.6):

| Viewer | Total | From N wolves | Tags |
|---|---|---|---|
| The player (on any of their characters) | Exact | Exact | Counts per tag |
| A friend who can see this character is theirs (doc 50) | Exact | Exact | Counts per tag |
| Everyone else | Band | Band | Known for, and shares in words |

- **Bands** (agreed): 10+, 25+, 50+, 100+, 250+, 500+, 1,000+ *(placeholder)*; under 10 reads "a few". **Giver
  bands:** 5+, 10+, 30+, 60+, 100+, 250+ *(placeholder)*.
- **Friends (new):** doc 48 §3.2 lets a friend hide which character they're on. An exact count on a stranger's card
  would undo that (a friend with 437 stars is the only 437), so exact counts go only to friends who can already see
  the character is theirs. For the same reason strangers see tag shares in words ("mostly Storyteller; often
  Packmate"), never counts **(new)**.
- **Where it shows:** the character sheet (`dialogs.ts` `character`: "★ 437 stars from 61 wolves · Known for:
  Storyteller", kinds and tags beneath), and every character's card (agreed): doc 50's card, or until then the Look
  panel (the `inspect` event in `RatwGame.cpp` gains `stars`).
- **The Mind is never told about stars.** They are out-of-character thanks; residents know deeds (doc 56).

### 2. Tags and Known for (agreed, doc 48 §3.6)

- **Tags:** Storyteller, Packmate, Good fun, and Welcoming, which only a newcomer (doc 52's flag when tagging) may give.
  Until doc 52 is built, Welcoming isn't offered.
- **Giving:** one click gives the star, as now. The ★ button then shows tag chips for that star for 10 minutes
  *(placeholder)* or until the card closes; one more click tags it, for good. Wire: social verb `startag {star, tag}`;
  `star` returns the star's ID.
- **Known for** shows once the account has 50 counted stars (agreed), naming the most-given tag (ties show both), with
  the breakdown on hover. Welcoming counts toward mentor standing (doc 52 reads `tags.welcoming`).

### 3. The star rate (open for the user: designed as optional)

`chances` grows at each settlement by the number of other qualified members (those who could have starred this wolf);
`goldReceived` counts every Gold Star received. Rate = `goldReceived / chances`, in words after 20 chances
*(placeholder)*: ≥ 0.6 "most wolves who play with them leave a star", ≥ 0.3 "many", ≥ 0.1 "some", else nothing. It is
always counted, and shown only when `stars.showRate` in `Data/Social/social.json` is true: false until the user
decides (doc 48, Open 1).

### 4. Less XP for roleplay in a fight (agreed, doc 48 §3.6, Decision 1)

`SocialLedger::FightTalkFactor` (twice a scene's pay) becomes `FightTalkShare`, **half a scene's pay**
*(placeholder)*: 10 for the first four who talk it through, instead of 40. Scenes outside fights pay as before.
`FightXP` (10 for taking part) stays as it is here; doc 49 decides what it becomes without levels. The review reads
"Stars are how a fight's roleplay is thanked" (`combat.ts`, `story.ts`); docs 08 and 33 are updated when built.

### 5. Scene openness (agreed, doc 48 Part 4)

`SocialSession` gains `openness` (`open`, `knock`, `private`) and when it last changed, `admitted` (actors let in,
with an expiry), `knocks`, `moments` (§8) and `place` (the cell's name). All of it is saved in `socialSessions`.

- **Defaults:** Open in public places (agreed); Private for a party's scene (agreed as a placeholder, doc 48 §4.1);
  Private in a resident's home, a rented place or a Chapter's site *(placeholder)*, found from `Society` homes,
  `estate::Estates::lease` and the camp sites. A fight's scene has no openness: joining a fight is doc 33's.
- **Changing it:** any member, from the scene line (agreed): OPEN · KNOCK · PRIVATE, the current one lit. At most one
  change per scene per 30 s *(placeholder)*; members are told ("The grey wolf made the scene private"). Social verb
  `openness {session, value}`.

| Openness | Who can join | Its speech, to others nearby |
|---|---|---|
| Open | Anyone in earshot, by speaking or with Join | A coloured bar and the tag "scene at the pier · open" |
| Knock | Anyone who knocks and is let in | Normal, with the tag "· knock to join" |
| Private | Members only | Normal talk between wolves: no tag, no Join, **never faded** (agreed) |

### 6. Joining and knocking (agreed, doc 48 §4.2)

**Routing.** `SocialLedger::record` is rewritten around an index from each actor to their open scenes
(`SocialLedger::sceneOf_`), which also settles doc 08's open refinement: several room scenes can run in one place. A
line goes:

1. to the actor's own scene in that lane (party or room), if they are a member;
2. else to a scene they were **admitted** to or pressed **Join** on in the last 2 minutes *(placeholder)*: no A–B–A,
   and the line counts at once (agreed);
3. else to an **Open** scene in the cell where a member hears them and they heard a member in the last 30 s (today's
   automatic joining, now for Open scenes only, party scenes included);
4. else into the A–B–A candidates, so two strangers can start their own scene beside a Private one.

**Two behaviour changes (new):** a stranger who answers a party member no longer joins a Private party scene (their
words form or join a room scene instead), and Knock or Private scenes never take outsiders by themselves.

- **Join** (Open scenes): social verb `join {session}`, from the same cell, able to hear a member speaking
  (`World::perceive` at `Voice::Speak`). Refused, without saying who, if any member has blocked the joiner (doc 50).
  The next line counts; doc 08's reply rule for pay still applies.
- **Knock:** social verb `knock {session}`. Each online member gets `{"type": "knock", session, from, newcomer,
  status}` (`from` as that member knows the knocker; `newcomer` from doc 52, `status` from doc 50). The scene line
  shows "A grey wolf is knocking · LET IN · NOT NOW", never a modal. The first `admit {session, who}` lets them in.
  Knocks lapse after 2 minutes, and a wolf turned away waits 5 before knocking again *(placeholders)*. A knocker
  blocked by any member gets "No answer."
- **A newcomer who joins** carries a small "new" mark on the members' scene line (agreed, doc 48 §4.2).
- **Nearby scenes:** `self.social.nearby` lists Open and Knock scenes in the viewer's cell that they could hear: place,
  number of wolves, openness, colour. **Counts, never names.** The scene line offers JOIN or KNOCK.

### 7. Seeing scenes: the log, the filter and the map (agreed, doc 48 §4.2)

- **Colours:** each scene's ID hashed into a small palette in `Data/Social/social.json` *(placeholder: teal, violet,
  sky, moss, rose, slate)*: no red (hostile) and no party gold (doc 32 §2.4).
- **Lines carry their scene.** `Game::publish` takes the speaker's routed scene, found before publishing by
  `SocialLedger::routeFor(actor, cell, lane, now)` (the same rules as `record`). Each `roleplay` event gains `scene`:
  `{mine: true, colour}` for members; `{open | knock, place, colour}` for others when the scene is Open or Knock;
  nothing for a Private scene. No IDs or names, so an anonymous voice stays anonymous. The lines that form a scene by
  A–B–A aren't tagged (it doesn't exist until the third).
- **In the log** (`story.ts` `updateFeed`): one's own scene's lines carry its bar; open scenes nearby their bar and
  tag; Knock scenes their tag; everything else, Private scenes included, shows normally. **Nothing is faded** (agreed).
- **"My scene only":** a toggle on the IN WORLD tab, kept in the browser (`ratw.feed.myScene`). It shows one's own
  scenes' lines, one's own lines, system lines, and anything addressed to the player ("→ you"), so nothing meant for
  them is lost.
- **Open scenes on the map:** `self.social.openNear` lists Open scenes, not one's own, within 60 tiles *(placeholder,
  doc 48 §4.2)*, placed at the members' centre or, for a scene **inside an interior** (interiors aren't drawn), at the
  door into it from outdoors. `minimap.ts` draws a small speech mark with the count; hover: "3 wolves at the Wharf
  tavern · open". Only Open scenes are marked (agreed).

### 8. End screens (agreed in spirit, doc 48 §8.1)

**Moments, not word counts** (agreed). Each scene records up to 24 moments *(placeholder)*: a kind, an actor, a target
and a time, **IDs only** (doc 08). Text is built for each viewer when shown, with the names *that viewer* knows (doc 32
§1.5).

| Kind | Recorded when | Shown as |
|---|---|---|
| `joined`, `admitted` | Join, or a knock let in | "A grey wolf joined the scene." / "Wren let Ash in." |
| `introduced` | `Game::noticeIntroduction` between two members | "Kestrel told Ash and Wren their name." |
| `newcomer` | A newcomer took part (doc 52) | "Ash was new to these parts." |
| `first` | Two members' first shared scene | "You and Wren shared a scene for the first time." |
| `bond` | Settlement moved the viewer's bond across a word (`Game::regardWords` before and after, in `afterSocial`) | "Wren knows you a little now." |
| `story` | The scene began or carried on a Story | "It carried on the Story 'The Drowned Bell'." |
| `chorus` | Members howled together (§9) | "You howled together." |
| `step` | A storyline step was done (doc 58) | "You found the miller (The debt, 3 of 5)." |
| `gift`, `groomed`, `deed` | Added by docs 55 and 56 | |

**Two changes to doc 48's example (new):** "Ash and Wren's bond grew" would show other wolves' bonds, which are private
records (doc 32 §1.4), so a viewer sees only changes to **their own** bonds; and lines use names and "their" until doc
50's pronouns exist.

- **The scene's card:** the ended block in the story column grows into a card (never a modal): "THE SCENE AT THE PIER ·
  40 MIN · +20 SOCIAL", who took part (by label), up to 8 moments, the ★ buttons with tag chips, MAKE IT A STORY and
  ADD TO. Wire: `self.social.ended` gains `place`, `minutes` and `moments: [{kind, text}]`, rendered by the server.
- **A fight's figures:** `BattleFighter` gains a `Tally`, counted where things happen in `Core/RatwBattle.cpp` and
  `Core/RatwMagic.cpp`: `landed` (blows that hit), `dealt`, `taken`, `raised` (allies got up from Downed: a successful
  `tend` or a Gift's "rise"), `covered` (Interposes, and blows taken on guard beside a Downed ally) and `guarded`
  (turns ended on guard) *(definitions are placeholders)*. `tendFightScenes` copies each player's tally into their
  `Contribution` at settlement, so it is saved and outlives the battle. `ended.tally` replaces the client's summing of
  log figures (kept as a fallback). The result card (`buildResult`) shows the figures ("12 landed · 2 raised · 1
  covered"), then moments ("You got Wren up from Downed"), then the review with stars and tags. Doc 48's "ground held"
  is read here as turns on guard; a truer measure can come later.

### 9. The gathering howl and chorus (proposed in doc 48 §3.5; planned here)

**Howling:** `{"type": "howl"}`, a HOWL button or `/howl`, handled by `Game::howl` in a new `Core/RatwGameHowl.cpp`.
Not in a fight or hunt, Downed, dead, held by the watch, or with something in the mouth. Cooldown 10 minutes a
character (agreed placeholder, doc 48 §3.5), in `howledAt_`, not saved. It is a real sound: a sneaking wolf who howls
gives itself away to residents within 32 tiles (`World::heardVoice`), as a yell does.

**Who hears it:**

- Players within 150 tiles *(placeholder)* in world tiles (`Cell::worldX/worldY` plus position), not across a large
  height difference (`worldZ`). The range scales with the listener's hearing (as in `World::hearingClarity`) and the
  weather, and indoors cuts it: ×0.4 for a howler inside, ×0.5 for a listener inside *(placeholders)*.
- Each listener gets `{"type": "howl", id, bearing, band, status, wolves, canJoin, until}` and a line: "A howl rises
  to the north-east, far off."
- **Direction, never position (new):** `bearing` is jittered ±12° *(placeholder)* and `band` is near, far or very far,
  so a hiding wolf can't be pinpointed (doc 48 §3.1a's reason for not scanning roleplayers). **The howler is never
  named.** A listener who has blocked them (doc 50) gets nothing.
- **The mark** lasts 60 s (agreed): a faint arrow at the minimap's edge toward the bearing, a line from the wolf on the
  World Map, labelled with the howler's status (doc 50): "a howl · Looking for a scene · far". The sound plays through
  `Client/src/ui/sound.ts`, louder when nearer, panned by bearing. "Show howl marks" is a client setting (on); the
  sound itself stays, being in the world.

**The chorus** (doc 48 §3.5): a howl within 8 s and 40 tiles of another joins its chorus *(placeholders)*; each joiner
extends the window by 4 s, to 20 s at most. Listeners within 40 tiles see JOIN THE HOWL while it is open (`canJoin`).
A chorus carries further, ×(1 + 0.25 per extra wolf) to ×2 at most, and listeners get **one** mark for it, updated
with the count. When it closes, each pair of players in it gains bond (+1 familiarity, +1 affinity *(placeholder)*),
once a pair a game day, and it becomes a `chorus` moment in any scene they share.

**Residents react** (doc 48 §3.5): within 60 tiles *(placeholder)*, found through the cells in range and
`World::entitiesIn`, up to 6 turn toward the sound and up to 2 say a short authored line from `Data/Social/social.json`
(curious by day, grumbling at night in town, cheering at a festival; no model call). `Game::recentHowls_` keeps each
community's latest chorus for a game hour, with where it came from in words ("from the hill north of town"), and
`Game::dialogueContext` adds it to residents' briefing ("Wolves howled together north of town a little while ago").
The server decides what happened; the Mind only mentions it. The ring lives in the game layer, not `RatwSociety`
(other sessions are editing the economy). **Animals:** a hook only; doc 53 decides.

### 10. Data, wire and the DM's view

**`Data/Social/social.json`** (new) holds every placeholder: star and giver bands, counting limits, tag names, the tag
window, the rate switch, openness defaults, scene colours, ranges, howl numbers and resident lines.

**Postgres** (one migration, the next free number: 0034 at the time of writing):

- **`game.stars`**, append-only like `game.events`: `id`, `world_id`, `recorded_at`, `at`, `kind`, `source`,
  `giver_account`, `giver_character`, `recipient_account`, `recipient_character`, `tag`, `xp`, `counted`; indexed by
  (`world_id`, `recipient_account`, `id`) and (`world_id`, `giver_account`, `id`). Written by
  `game.record_stars(world, jsonb)` in the checkpoint's transaction (`Core/RatwDbStore.cpp`, beside `record_events`),
  once a star's tag window has closed, so rows are never updated.
- **`game.star_tallies`**, a checkpoint section (`starTallies`, keyed by account, in `game.sections`; written by
  `save_checkpoint_delta` like the Chapter tables of migration 0030). The DM can read it.
- **`socialStars`** stays in the checkpoint row, pruned to 48 hours: what `SocialLedger::star` needs for duplicates,
  decay and the daily limit. Stars already saved are folded into the star book on the first load (each recipient's
  account from `Accounts::ownerOf`).
- Stars aren't valuables and aren't journalled (doc 31); a crash may lose those since the last snapshot.

**Wire:**

- Social verbs: `star` (now returning the star ID), `storystar`, `startag {star, tag}`, `openness {session, value}`,
  `join`, `knock`, `admit {session, who}`, `refuse {session, who}`. A new command `{"type": "howl"}`; new events
  `knock` and `howl`; `roleplay` events gain `scene`.
- Snapshot: `self.stars` (the player's own tally); `self.social.scenes[]` gains `openness`, `colour`, `place` and
  `knocks`; `self.social.nearby`, `openNear`, `starsGiven` (stars still open to a tag); `self.social.ended` gains
  `place`, `minutes`, `moments` and `tally`; `self.howlReady`. The card's (or `inspect`'s) `stars`:
  `{shown, exact, from, knownFor, tags, rate?}`.

**The Dungeon Master:** the Players tab (`Editor/src/dm/DmApp.tsx`; `tools/dungeon_master.py` `/api/players`) shows
each character's account stars, givers and Known for, and a Stars drawer from `game.stars` (who gave what, when, the
tag, whether it counted), with a hint when one giver account supplies more than 25% of an account's counted stars
*(placeholder)*, which points at alts. **`stars.void {star}`** (role dm, audited) takes a star out of a total. Later
and optional: a LIVE Scenes layer with places, counts and openness, never words (doc 34 §1.1).

## Phases

Each phase passes `world_check --players 20` within noise before the next. None needs a 1,000-player run.

### Phase 1: The star book, bands and the fight cut

- **Goal:** every star counts toward its account; everyone sees a total, in bands; fight roleplay pays less.
- **Changes:**
  - **Server:** `Core/RatwStars.h`, `.cpp` (`StarBook`: record, counting rules, tallies, bands, `view(viewerAccount,
    ownerAccount, friendSees)`); `Accounts::ownerOf` if doc 50 hasn't added it; `Game::socialCommand`'s `star` and
    `storystar` record into the book and refuse same-account stars; `SocialLedger::stars` pruned to 48 h in `tick`;
    `FightTalkShare`; `inspect.stars` and `self.stars` in `sendSnapshot`.
  - **Data:** the migration (`game.stars`, `record_stars`, the `starTallies` section); `Data/Social/social.json`; old
    stars folded in on load.
  - **Client and DM app:** the stars line on the sheet (`dialogs.ts` `character`) and Look panel; the review copy in
    `combat.ts` and `story.ts`; the Players tab's stars column.
- **Tests:** `Tests/stars_tests.cpp` (new: counting limits, same-account refusal, distinct givers by account, bands, the
  view for self, a friend who sees and a stranger); `social_game_tests` (`fightScenes` with the new pay;
  `starsAndStories` reaching the tally); `checkpoint_tests` (tallies and the window survive a restart; old stars fold
  in); `tools/test_game_tables.py` (the section round-trips, `record_stars` appends); `social.test.ts` (band words).
- **Done when:** two players on separate accounts star each other after a scene and each sheet shows the exact total;
  a third player's Look panel shows "a few", later "10+"; a second character of the same account can't star the
  first; a fight talked through pays 10 + 10, not 10 + 40.
- **Cost:** O(1) per star (lookups by account); tallies written only when changed; nothing in the tick.

### Phase 2: Tags, Known for, the spread and the rate

- **Goal:** a star can say what was valued, and a card says what a wolf is known for.
- **Changes:** `StarBook::tag` and the `startag` verb; `self.social.starsGiven`; chances counted at settlement in
  `payMember` and `settleFight` (through a callback, so `SocialLedger` stays pure); Known for, shares in words, the
  rate; Welcoming gated on doc 52's `isNewcomer(account)` and hidden until it exists. Client: tag chips after a ★ in
  `story.ts`, `combat.ts` and the Stories list (`dialogs.ts` `stories`); Known for with a hover breakdown.
- **Tests:** `stars_tests` (tag once, in the window, never changed; Welcoming refused for a non-newcomer; Known for at
  49 and 50; ties; words for strangers, counts for self; the rate's words and its floor); `social.test.ts`;
  `tools/client/stars.mjs` (new: give, tag, see the sheet change; screenshots in `artifacts/screenshots/stars/`).
- **Done when:** a tagged star shows in the recipient's breakdown; at 50 stars a stranger sees "Known for: Packmate" and
  words, not numbers, on hover; with `showRate` on, the rate appears after 20 chances.
- **Cost:** as Phase 1, plus one counter per qualified member at settlement.

### Phase 3: Openness, joining and knocking

- **Goal:** every scene is Open, Knock or Private; strangers join open scenes and knock on others; private scenes stay
  private, and several conversations share one place.
- **Changes:**
  - **Server:** `SocialLedger` gains `sceneOf_` (kept by `record`, `leave`, `settle`, `joinFight`), `openness`,
    `admitted`, `knocks`, the routing of §6 and `routeFor`. Ended scenes are pruned after 3 days *(placeholder)*, and
    `propose` and `extend` take only scenes ended within 24 h (the UI already offers an hour). `socialCommand` gains
    `openness`, `join`, `knock`, `admit`, `refuse`, with doc 50's `blocks` (a stub answering no until then).
    `refreshSocialViews` walks the client's own scenes through `sceneOf_` instead of every session, and adds `nearby`
    and `knocks`.
  - **Client:** the OPEN · KNOCK · PRIVATE toggle, nearby scenes with JOIN or KNOCK, LET IN and NOT NOW, the "new" mark.
- **Tests:** `social_game_tests` new cases: a Private party scene ignores a stranger who answers, an Open one takes
  them; Join counts the next line without A–B–A; a knock let in, turned away, lapsed, and the wait; two room scenes in
  one cell (one Private, one formed beside it); openness changes and their limit; a blocked wolf can't join. The
  existing cases (`actionsAndPartyScenes`, `leavingAScene`, `aSceneSeenAndStarred`) still pass, changed only where
  the Private default means to change them. `checkpoint_tests`: openness, admitted and knocks survive a restart.
  `tools/client/scenes.mjs` grows: Knock, a third wolf knocks and is let in, JOIN on an Open scene.
- **Done when:** in the real page two party mates talk privately while a stranger in the same tavern starts their own
  scene; the stranger can't join the private one and joins an open one with one click.
- **Cost:** routing becomes O(1) a line through `sceneOf_` instead of O(sessions); `refreshSocialViews` becomes
  O(clients × own scenes) instead of O(clients × all sessions), a saving that grows with uptime; `nearby` is O(scenes
  in the viewer's cell).

### Phase 4: Seeing scenes in the log and on the map

- **Goal:** a player tells their own scene's lines at a glance, sees open scenes beside them, filters to their own, and
  finds open scenes on the minimap.
- **Changes:** `Game::publish` takes the routed scene and adds `scene` to each `roleplay` event (membership through
  `sceneOf_`, O(1) a listener); `self.social.openNear` from an index of Open scenes (centre in world tiles, or the door
  into an interior). Client: `Post.scene`; bars, tags and the filter in `story.ts`; speech marks in `minimap.ts`.
- **Tests:** `social_game_tests` (members get `mine`; an onlooker gets `open` and the place; a Private scene's lines
  carry nothing; an anonymous voice carries no ID); `wire_tests`; `game.test.ts` (the filter keeps own lines, system
  lines and "→ you"); `scenes.mjs` screenshots of bars, filter and minimap mark.
- **Done when:** from across town a player sees "3 wolves at the Wharf tavern · open", walks in and sees that scene's
  lines barred; with the filter on, a private conversation nearby leaves the log while a line addressed to them stays.
- **Cost:** `publish` already loops over every client per line; this adds one lookup per listener. `openNear` is
  O(clients × open scenes) every 2 s, and open scenes are few (about one per busy place): thousands of distance checks,
  not millions.

### Phase 5: End screens

- **Goal:** a fight ends with what each wolf did, a scene with what happened between them, stars and tags on both.
- **Changes:** `BattleFighter::Tally`, counted in `RatwBattle.cpp` and `RatwMagic.cpp` (blow resolution, `tend`, Gifts'
  "rise", `interpose`, `guard`); the tally copied into `Contribution` by `tendFightScenes`; `SocialSession::moments`
  recorded at `join`, `admit`, `noticeIntroduction`, `afterSocial` (`first`, `bond`), `propose`, `extend` and the
  chorus; `ended.moments` rendered per viewer with `labelFor`; hooks for docs 55, 56 and 58. Client: the scene card in
  `story.ts`; figures and moments on `combat.ts`'s result card.
- **Tests:** `battle_tests` (blows, damage, a tend that raises, an Interpose, guard turns; a fight past 60 log lines
  still counts every blow); `social_game_tests` (moments recorded and capped; an introduction named for a viewer who
  heard it and described for one who didn't; a bond moment only for the viewer's own bond); `checkpoint_tests`;
  `tools/client/fight.mjs` and `scenes.mjs` screenshots of both cards.
- **Done when:** after a team fight the card says "9 landed · 1 raised" and "You got Wren up from Downed"; after a scene
  it says who took part, how long it ran, that Kestrel told the others her name and that Wren knows the viewer a
  little now, with no word or turn count anywhere.
- **Cost:** a few integers per fighter; moments recorded at events, rendered only for each client's last-ended scene
  during the hour it shows.

### Phase 6: The gathering howl and chorus

- **Goal:** a wolf can call others from across the country, others can join the howl, and the town notices.
- **Changes:** `Core/RatwGameHowl.cpp` (`Game::howl`, `choruses_` closed by time in the game tick, the listener pass,
  residents' reactions, `recentHowls_`); `dialogueContext` gains the howl fact; `self.howlReady`; howl numbers and
  lines in `Data/Social/social.json`. Client: HOWL and `/howl`, the `howl` event, marks on `minimap.ts` and the World
  Map, JOIN THE HOWL, the sound cue, the setting.
- **Tests:** `Tests/howl_tests.cpp` (new: range by world distance across places, indoors, hearing and weather; bearing
  within the jitter; band; cooldown; a chorus formed, extended, capped and carrying further; one mark per chorus; bond
  once a pair a day; a blocked listener gets nothing; the sneak given away; residents turn and at most two speak; the
  Mind fact lasts an hour); `tools/client/howl.mjs` (new: one page howls, the other sees the mark and joins).
- **Done when:** a wolf howls at the edge of Upper Accord; a player across town sees a faint arrow and "a howl ·
  Looking for a scene · far" and joins within seconds; a third, further off, sees one chorus mark of "2 wolves"; a
  resident in the square, asked, mentions the howling to the north.
- **Cost:** one howl is O(clients) distance checks plus O(cells in range) for residents, at most once per wolf per 10
  minutes: under two a second even with 1,000 players all howling. Per tick, only closing open choruses.

## Depends on and feeds

- **Depends on doc 50** (friends and `friendSeesCharacter`, `blocks`, `statusOf`, the card), **doc 52**
  (`isNewcomer`) and **doc 49** (account social level on the card; `FightXP`). Every phase runs with stubs until they
  land: exact counts for the player only, no blocks, no status, no Welcoming.
- **Feeds doc 49** (stars and distinct givers for Gift unlocks), **doc 52** (Welcoming for mentor standing; the
  newcomer mark on joining), **doc 54** (open scenes make taverns look alive; the storytelling contest's audience
  stars), **docs 55 and 56** (moments) and **doc 58** (milestone and tale stars through `StarBook`; tale scenes use
  openness, and narration follows it).

## Risks

- **The routing rewrite** touches every scene. The social tests and `scenes.mjs` guard it, and Open scenes keep today's
  behaviour.
- **Linking characters:** an account-bound total on every card links characters weakly even in bands (two wolves both
  "250+, Known for: Storyteller"). Bands, words and the friend rule limit it; players should know stars are shared.
- **Alt accounts farming stars:** the counting limits, the spread, the DM's concentration hint and `stars.void`.
  Quickened's 30 distinct givers is the real gate.
- **Openness wars** between members: the 30-second limit and the announcement make it visible; leaving is the answer.
- **Howl spam in towns at night:** the cooldown and residents grumbling; a town rule can come later if needed.
- **Unbounded social memory:** pruning scenes (Phase 3) and stars (Phase 1) helps, but `SocialLedger::entries` still
  grows for good and is scanned by `usedToday`, `paidFor` and `restedLeft`. That is doc 49's to fix with the XP rework
  (an index by actor and a rolling window); noted here because these phases call those functions more.
- **Other sessions:** nothing here touches `RatwOrchestrator`, `RatwDemand`, `RatwOddJobs`, `RatwResidents`,
  `RatwSociety.h` or `Data/Economy`; homes and leases are read through existing accessors.

## Decisions

**Agreed (doc 48):**

1. Cut XP for roleplay during fights; lean on stars (§3.6, Decision 1).
2. An account-bound total of every Gold Star, Story Star and milestone star, on every character's card (§3.6,
   Decision 2).
3. Everyone sees it: bands for strangers (10+ … 1,000+), exact for the player and their friends (Decision 27).
4. Tags Storyteller, Packmate, Good fun, and Welcoming from newcomers only; Known for at 50 stars; the tag a second,
   optional click (§3.6, Decisions 3 and 29).
5. Openness Open / Knock / Private, set by any member and shown on the scene line; private scenes shown normally,
   never faded (Part 4, Decision 30).
6. Join counts the next line without A–B–A; Knock asks the members; open scenes on the minimap; one's own scene picked
   out, with a "my scene only" filter (§4.2).
7. End screens: figures for fights, moments from the ledger for scenes, not word counts (§8.1, Decision 7).
8. The howl's 10-minute cooldown and one-minute mark (§3.5, placeholders).

**New placeholder choices (this plan):**

9. Same-account stars are refused.
10. A star counts toward the total within the giver's 10 a day, 3 a pair a day and 10 a pair in 30 days.
11. "From N wolves" counts giver accounts, in bands 5+ … 250+.
12. Exact counts only for friends who can see the character is theirs; strangers see tag shares in words.
13. Fight roleplay pays half a scene's pay (10), not twice (40).
14. Party scenes, and scenes in homes, rented places and Chapter sites, start Private.
15. Strangers no longer join a Private party scene by answering; several room scenes may share a place.
16. Knocks lapse after 2 minutes, with 5 minutes before knocking again; openness changes at most every 30 s.
17. The end screen shows only the viewer's own bond changes.
18. Howl marks give a jittered direction and a distance band, never a position or the howler's name.
19. Howl numbers: range 150 tiles; chorus within 8 s and 40 tiles, up to ×2; bond once a pair a game day.
20. Every number lives in `Data/Social/social.json`.

## Answered (the user, 2026-10-07)

1. **The star rate is shown** ("players should see their star rate"): `stars.showRate` is on.
2. **Talking a fight through pays a scene's ordinary pay** ("I think it should pay normally, why wouldn't it?"): not
   half, as planned, and not twice, as before. This replaces §4 and decision 13. `FightTalkFactor` is 1.
3. **Scenes on the minimap are off by default; each player may turn them on** in Settings. With it on, Open scenes
   show, and **a Knock scene shows only when one of the player's friends is in it** (doc 50's friends); Private
   scenes never. This changes §7's map rule and Phase 4.

Also since this plan was drafted, plans 49 and 50 built `Accounts::ownerOf`, `Game::blocked`, `Game::areFriends`,
the status and the card, so the stubs this plan expected aren't needed: "a friend who can see this character is
theirs" is a friend who shares their wolf with the viewer (`Game::sharedHandle`).

## Open questions

None.

## Built

### Phase 1: the star book, bands and fight pay (built 2026-10-07, not committed)

- **Data:** `Data/Social/social.json` (new) has the `stars` section, all placeholders:
  - the bands (10+ … 1000+) and giver bands (5+ … 250+);
  - the counting limits: the giver's 10 a day, a pair's 3 a day, a pair's 10 in 30 days;
  - 30 days of stars kept, the rate on (the user's answer), shown after 20 chances, with its words;
  - the four tags, a 10-minute tag window, and Known for at 50.
- **The book:** `Core/RatwStars.h/.cpp` (new, pure, `ratw::stars`). `Book::record` decides whether a star counts:
  - it must be within all three limits, counting either way round in time;
  - it never counts from the recipient's own account.

  Each account's `Tally` holds counted stars by kind and tag, the giver accounts, and Gold Stars received. `view`
  gives the exact count or the bands; `band` puts a number in words; `prune` drops stars past 30 days, leaving the
  tallies alone; and there's save and load.
- **The game:**
  - `recordStar` (in `RatwGameSocial.cpp`) records each Gold and Story Star against the receiving account, then looks
    at the account's Gift tiers again.
  - A star between one account's own wolves is refused ("Not one of your own wolves.").
  - `starsFor(viewer, target)` is exact for the player themselves and for a friend they share their wolf with
    (`sharedHandle`), and in bands for everyone else.
  - `self.social.stars` carries the player's own count; a closer look at a player carries `stars`.
  - Doc 49's Quickened gate now reads the book: counted stars, and the accounts that gave them.
  - `SocialLedger::stars` is pruned to two days in its tick. The record that lasts is the book's.
- **Fight pay:** talking a fight through pays a scene's ordinary pay (`FightTalkFactor` 1, the user's answer: 20 for
  contributors 1–4, where it was 40). The result card and the scene line now say so, and doc 08 is updated.
- **Saving:** `people.starTallies` and `people.stars`. `Database/migrations/0040_stars.sql` adds
  `game.star_tallies` and `game.stars`, readable by the DM and not by the editor or publisher. **Not applied yet.**
  - This differs from §10: `game.stars` holds the last 30 days, kept by the checkpoint like the other lists, not an
    append-only history written by a `record_stars` function. The DM's Stars drawer and `stars.void` can read it
    when they're built.
  - Older stars aren't folded in, by the no-migration rule.
- **DM app:** a Stars column on the Players tab (the counted total), from `game.star_tallies` through
  `tools/dungeon_master.py`.
- **Client:**
  - The sheet shows "★ 437 stars from 61 wolves" (exact), beside the social line and in STANDING.
  - A card shows the same, or in bands ("★ 250+ stars from 30+ wolves", "★ A few stars"), with a tooltip saying what
    stars are and who sees the count.
  - `starsLine` is exported for tests.
- **Tests:**
  - `Tests/stars_tests.cpp` (679 checks, most of them snapshot fills), covering:
    - the book: a pair's 3 a day and 10 in 30 days, the giver's 10 a day, never one's own, a Story Star counting as
      one, givers counted by account, the bands, exact against banded views, a round trip, the pruning;
    - in the game: after a party scene, Bo stars Ada and her sheet shows one star from one wolf; Cy, a stranger,
      sees "a few"; as a friend she shares her wolf with, Cy sees the count, and when she stops sharing, the band
      again; Quickened counts from the book; Bo's second wolf can't star his first;
    - the book across a restart.
  - `social_game_tests` (fight pay: 20); `game_tests` (Quickened from book stars); `tools/test_game_tables.py`;
    `tools/test_dungeon_master.py`; `Client/src/game/people.test.ts` (the star line's words).
  - `ctest` 53 of 53; client tests 106; `card`, `scenes`, `safety` and `friends` pass in a real page (the card's
    screenshot shows "★ A few stars").
- **Cost:**
  - A star: one pass over the last 30 days' stars to count it. Stars are rare.
  - A card or the sheet: one map lookup.
  - Nothing runs in the world's tick.

### Phase 2: tags, Known for, the spread and the rate (built 2026-10-07, not committed)

- **The book** (`Core/RatwStars.*`):
  - `Book::tag`: only the star's giver may tag it, once, within the 10-minute window, with one of the tags. Welcoming
    needs a newcomer. A tag on a counted star goes into the tally.
  - `openToTag`: the giver's untagged stars still in the window, found by a short walk back from the newest.
  - `chances`: chances to have been starred, for the rate.
  - `view` now adds the tags (counts for the exact view; for everyone else each tag's share in words: "mostly",
    "often", "sometimes", "now and then"), `knownFor` (from 50 counted stars, the most-given tag, both when tied),
    and, with `showRate` on (the user's answer), the rate in words after 20 chances, with the share for the exact
    view only.
- **The game:**
  - The social verb `startag {star, tag}`.
  - `self.social.starsGiven` lists the stars this wolf may still tag, each with whom it went to, the seconds left,
    and the tags it may give. Welcoming stays hidden until doc 52 says who's a newcomer.
  - Chances are counted at each settlement in `afterSocial`, one for each other who qualified with them (the
    entry's partners), so `SocialLedger` stays pure.
- **Client:**
  - After a star, the scene bar asks "★ Your star to … : what was it for?" with a chip for each tag; one click tags
    it, and the chips go.
  - This covers stars from a scene, a fight or a Story, since they all show in the story column.
  - The sheet and a card show what the stars say beneath the count: "Known for: Storyteller · Storyteller 30,
    Packmate 15 · Most wolves who play with them leave a star", or in words for strangers.
  - The scene bar and the fight result card no longer say fight roleplay pays twice.
- **Tests:**
  - `Tests/stars_tests.cpp` (777 checks), covering:
    - tags: only by the giver, real tags, Welcoming only from a newcomer, open for ten minutes, once, too late after;
    - Known for: none at 49, Storyteller at 50, both on a tie;
    - counts for the player and words for a stranger;
    - the rate: none at 19 chances, "most…" at 12 of 20 (the share for the player only), "some…" at 12 of 50;
    - in the game: Bo's star open to a tag, tagged Storyteller into Ada's count, and one chance each from their
      scene.
  - `Client/src/game/people.test.ts` (the words).
  - `tools/client/stars.mjs` (new, 9 checks; screenshots in `artifacts/screenshots/stars/`): a scene ends; Bo stars
    Ash from its card; the tag chips follow, he tags Storyteller, and they go; Ash's sheet shows "★ 1 star from 1
    wolf" and "Storyteller 1"; Cy, a stranger, sees "★ A few stars".
  - `ctest` 53 of 53; client tests 107; `card`, `scenes` and `friends` pass in a real page.
- **Cost:**
  - A tag: one walk back through the stars of the last ten minutes.
  - The open stars, in each social view every two seconds: the same short walk.
  - Chances: one counter per settlement.
  - No new saving: tags and chances go in the tallies.

### Phase 3: openness, joining and knocking (built 2026-10-07, not committed)

- **Scenes** (`Core/RatwSocialCore.h`): `SocialSession` gains `openness` ("open", "knock", "private", or "" for a
  fight's), `opennessAt` (the last change), and `admitted`, `knocks` and `refused` (who, until when). All are saved
  in `socialSessions` (`Core/RatwCheckpoint.cpp`). Older scenes load Open; that table needed no migration.
- **Routing** (`SocialLedger::record`, rewritten):
  1. The actor's own scene in that lane (their party's or the room's) takes the line, and only when another of its
     wolves hears them (or a party mate is near). Words said to no one stay out, as before.
  2. A scene they joined or were let in to in the last two minutes takes the next line at once, heard or not, with no
     A–B–A.
  3. A party mate's line goes to their party's scene here, or opens one, Private.
  4. An Open room scene takes one a member hears. An Open party scene takes one who answers a member.
  5. Otherwise the A–B–A candidates, so two strangers make their own scene beside a Private one, and several scenes
     share a place.

  Knock and Private scenes never take an outsider by themselves. As planned, a stranger who answers a party member no
  longer joins its (Private) scene.
- **Indexes:** `scenesOf(actor)`, `openIn(cell)` and `lastEnded(actor)` are kept by `record`, `leave`, `settle` and
  the fight's scenes. `reindexScenes()` rebuilds them after a load.
  - Scenes that ended more than three days ago are let go in `tick`; Stories and stars look back a day at most.
  - Knocks lapse after two minutes and refusals after five.
- **Ledger rules:**
  - `setOpenness`: members only; the first change any time, then at most one every 30 seconds.
  - `join`: Open scenes only.
  - `knock`: Knock scenes only; once; not again within five minutes of being turned away.
  - `admit` and `refuse`: members, while a knock is under two minutes old.
- **Where a scene starts Private:** a party's scene, and a room scene in a rented place (`Game` sets
  `SocialLedger::privatePlace` from `estates_.lease`).
  - This differs from §5: residents' homes are left out for now. The world can't tell a home from an inn or a shop
    whose keeper lives there, and counting those would have made tavern scenes private.
- **The game** (`socialCommand`): the verbs `openness`, `join`, `knock`, `admit` and `refuse`.
  - Joining and knocking need the wolf to be in the scene's place and able to hear one of its wolves speaking.
  - Anyone a member has blocked is refused: "You can't join that scene." or "No answer.", never who.
  - Members are told of a change ("… made the scene private."), of a knock (a `knock` event and a line where they
    write) and of who was let in. The knocker is told whether they were let in.
- **The scene views** (`refreshSocialViews`) now walk each player's own scenes through the index, find their last
  ended scene through `lastEnded`, and read receipts by actor. Before, they scanned every scene and every receipt
  ever, for every player.
  - Each scene carries `openness` and `knocks`, named as this wolf knows the knockers.
  - `nearby` lists the Open and Knock scenes here that the player isn't in, could hear, and has no blocks with: how
    many wolves and the openness, never names.
- **Client** (`story.ts`):
  - Each of one's scenes shows OPEN · KNOCK · PRIVATE, with the current one lit.
  - A knock shows as "… is knocking · LET IN · NOT NOW".
  - A scene here shows as "A SCENE HERE · OPEN / KNOCK TO JOIN · 2 wolves", with JOIN or KNOCK, or "knocked".
  - The scene bar now also shows when one is in no scene but one is nearby, or a star is waiting for its tag. Before,
    both stayed hidden.
  - Doc 52's "new" mark waits for doc 52.
- **Tests:**
  - `Tests/social_game_tests.cpp` (584 checks):
    - a party's scene starts Private and a stranger answering stays out; opened, one who answers joins;
    - Join makes the next line count, heard or not;
    - the 30-second limit and its first-change exception, bad values, non-members;
    - Knock: no Join; heard but not in; knock once; turned away for five minutes; let in; a knock lapsing;
    - Private: no Join, no knock;
    - two scenes in one tavern; indexes rebuilt from a copy;
    - an ended scene out of the indexes, kept three days, then let go.
  - `Tests/scene_doors_tests.cpp` (new, 984 checks, most of them snapshot fills): through the game, a party's scene
    unseen by Cy until Ada opens it; Cy sees "2 wolves · open" with no names, joins, and her line counts; a Knock scene
    refuses Join and ignores her talk; her knock reaches both its wolves with a line, shows in Ada's scene line and as
    "knocked" in hers; let in, her line counts; one who blocked her keeps the scene out of her list and refuses her
    join and her knock, without saying who.
    - While writing this file I first saved it over the existing `Tests/scenes_tests.cpp` (doc 30's ambient scenes,
      which had no local changes). I restored it from git; it still passes its 260 checks.
  - `tools/client/scenes.mjs` grows a knock: Bo makes his scene KNOCK; Cy sees "A SCENE HERE · KNOCK TO JOIN · 1 wolf"
    and knocks; Bo sees it with LET IN and lets her in; her line counts. Screenshots 8 and 9.
  - `ctest` 54 of 54; client tests 107; `card`, `stars`, `friends`, `safety`, `party` and `names` pass in a real page.
- **Cost:**
  - Routing is a lookup into the actor's own scenes, then the open scenes in that place, instead of every scene ever.
  - The scene views are O(players × their own scenes), plus the open scenes in each player's place for `nearby`.
  - Scenes are let go three days after ending.
  - Nothing new runs in the world's tick.
