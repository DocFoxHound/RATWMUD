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
  number of wolves, openness, colour. The scene line offers JOIN or KNOCK.
  - **Who is in it, on hover** (the user, 2026-10-07, replacing "counts, never names"): pointing at A SCENE HERE opens a
    small window listing each wolf in it with its portrait and its name **as the viewer knows it** (a stranger by their
    look, as everywhere: doc 32 §1.5), as a scrolling list when there are many.
  - Private scenes are still never listed, and a scene with anyone who has blocked the viewer isn't offered at all.

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

### Phase 7: Story books and the bookshelf (added by the user, 2026-10-07)

The user: "scenes should be linkable. Like if players start a new scene they should be able to link it (after the fact,
before, or during) to other scenes in a player-driven Story. These don't need to be official stories, they can be
shared through Circles and Chapters and Friends." Their Stories sit as books on a bookshelf, newest at the top.

**A book** is a player-driven Story: an ordered list of scenes (its chapters), with a title, a summary, its wolves, who
it's shared with, and whether it's finished.

- **The one kind of Story** (answered below): doc 32's official Stories become books. A book needs no agreement to
  exist; one agreed by two thirds of its wolves is **official** and pays as a Story does today when it is finished.
- **Linking a scene, any time:**
  - **Before:** "my next scene goes into …" from the book's page. The next scene the wolf is in joins it.
  - **During:** ADD TO A STORY on the scene line.
  - **After:** from the ended scene's card, or the book's page: a scene the wolf was in, for **seven days** (what
    players are told; scenes are kept eight, the user's rule, so the last day is grace).
  - Any wolf who was in a scene may link it to a book they're in. A wolf who was in a linked scene but not the book
    becomes one of its wolves.
- **Where it goes:** linking a scene asks where it falls in the book's timeline (after which chapter), newest last by
  default. The book's wolves may reorder its chapters later.
- **A chapter** keeps what the book needs even after the scene itself is let go, eight days on:
  - when and where it was, and who was in it;
  - a title (player-written, 60 letters *(placeholder)*);
  - a summary, either player-written (600 letters *(placeholder)*) or written by the small model from the linking
    wolf's own recap of it (doc 50 §5: only what that wolf perceived). It is marked as the model's, and any of the
    book's wolves may rewrite it.
- **The book's own summary:** player-written, or the model's from the chapters' titles and summaries.
- **Finishing a book** (agreed, the user, 2026-10-07): its keeper (who started it) proposes FINISH. It is finished once
  a majority of its recently active wolves (in a chapter in the last 30 days *(placeholder)*) agree, or, with no
  activity, after three days without an objection *(placeholder)*. Finished, the model writes a line or two of
  flavour text from its chapters. The book shows as complete on every shelf, a bound, gilded spine, and takes no more
  chapters.
- **Private scenes** (agreed): any of a scene's wolves may link a Private scene into a book, whoever the book is shared
  with.
  - Until the book is finished, a Private scene's chapter shows its readers only its title and date. Its summary is
    for that scene's own wolves.
  - Proposing FINISH warns, on the proposal and on every agreement: "This book has private scenes. Once it is
    finished, anyone who can read the book can read their summaries."
  - Once finished, anyone who can read the book can read every chapter's summary, private scenes' included. The scene's
    lines never are: the summary is the linker's own words, or the model's from their own recap.
- **Volumes** (agreed): a finished book can be linked to another as **related**, **sequel** or **prequel**, by its
  keeper or any of its wolves. A reader opening one sees the whole volume at once: the books in order (prequels before,
  sequels after, related beside), each with its flavour text, and can read straight through.
- **Sharing:** each book is shared with Members only, Friends, a Circle (which), a Chapter, or Everyone. Its keeper
  chooses, and each wolf may hide themselves from it. Names are always shown as each viewer knows the wolf.

**The bookshelf** (a STORIES view, from the character sheet and the FRIENDS sheet):

- **Books as spines** on shelves, coloured by kind. The view opens at the top shelf with the newest books and scrolls
  all the way down through every book the player's wolves have been in.
- **Filters** (the user's), worked out for each viewer:
  - **World:** the Dungeon Master's world storylines (the user, 2026-10-07; doc 34's story planner, doc 58's
    storytellers): books the DM has tied to a world storyline, and the storylines as volumes;
  - **Chapter:** shared with, or mostly made of, the viewer's Chapter;
  - **Circle:** shared with one of their circles;
  - **Friend:** with one of their friends in it;
  - **Other:** books they were in with no official tie to any of the other wolves.
- **Unaffiliated** (a separate tab): books they **aren't** in that their friends, circles or Chapter are in and have
  shared with them, books shared with Everyone, and the world's storylines they haven't joined: stories to find, and
  perhaps join.
- **A book, opened:** its title, summary (or flavour text, once finished), its wolves, its volume (related, prequels,
  sequels), then a timeline of its chapters: title, date and place, summary, and who wrote it (or "the model"). Its
  wolves get Add a scene, Reorder, Edit, Finish and, once finished, Link to another book.

**Changes:**

- **Server:** `Core/RatwBooks.h/.cpp` (pure: books, chapters, ordering, sharing, finishing, the shelf's categories for a
  viewer) and `Core/RatwGameBooks.cpp` (the `book` command, linking from scenes, the views, `self.social` hooks for
  "next scene into …").
- **The Mind:** `/chapter` (a scene summary from a recap) and `/book` (a book's summary, and its flavour text when
  finished), on the small model, kind `book` in the call ledger, a fixture for tests. These cost money; their budgets
  sit beside recaps' (10 a day a character *(placeholder)*), and the player-written way always works.
- **Data:** a migration for `game.story_books` (sections; the DM reads them); `Data/Social/social.json` gains the
  books' numbers.
- **Client:** the STORIES bookshelf (spines, filters, the Unaffiliated tab, infinite scroll), the book page and
  timeline, ADD TO A STORY on the scene line and the ended card, and the placement picker.

**Tests:** `Tests/books_tests.cpp`:

- linking before, during and after, and the placement;
- a chapter outliving its scene;
- reordering; summaries written, by the model (fixture), and rewritten;
- finishing: a majority of the recently active, three quiet days, an objection, the private-scene warning, flavour
  text;
- a private scene's summary hidden from readers until the book is finished, then shown;
- volumes: related, sequel, prequel, read in order;
- sharing and each filter for a viewer; Unaffiliated;
- names as each viewer knows them; a hidden wolf;
- across a restart.

`tools/client/books.mjs`: two wolves make a book from a scene, link a second one before it starts, finish it, and see
it gilded at the top of the shelf, while a friend sees it under Unaffiliated.

**Done when:** players can link scenes into a Story at any point, find every Story they've been part of on a shelf
newest first, filter it, read a book's chapters as a timeline, finish it with flavour text, and discover their
friends', circles' and Chapter's Stories.

**Cost:** a book is read when the shelf or the book is opened, never per tick. Model calls are capped per character per
day.

**Answered (the user, 2026-10-07):** finishing by a majority of the recently active, or unopposed after no activity;
Private scenes may be linked, with a warning on finishing, and their summaries readable by all the book's readers once
it is finished; finished books link into volumes (related, sequel, prequel); "World" means the DM's world storylines.

**Books are the one kind of Story** (the user, 2026-10-07: "make books one kind"). Doc 32's official Stories become
books:

- **"Official"** is a book that two thirds of its wolves agreed to, as today's proposal is agreed. An official book pays
  as a closed Story does now when it is finished: social XP for its tellers, Story Stars, Chapter renown for a Chapter
  Story, and the closed Stories that doc 49's Quickened gate counts.
- **An unofficial book** works the same in every other way: linking, chapters, sharing, the shelf, volumes. It pays
  nothing; it is for telling and sharing.
- **What moves:** `SocialLedger`'s Stories (`propose`, `approve`, `extend`, `close`, `storyStar`, `SocialStory`) and
  the character sheet's STORIES list move into the book code and the bookshelf. MAKE IT A STORY and ADD TO become
  "Start a book" and ADD TO A STORY. Making a book official is one more step, AGREE, with today's rules (two thirds,
  within a day).
- **Today's limits carry over** to official books: 32 scenes and 48 wolves, 8 open at once. Telling pays as now:
  members paid in two or more of its scenes get a quarter of what those scenes paid, plus one per scene past the first,
  at most 5. Unofficial books get their own, roomier limits *(placeholders in `Data/Social/social.json`)*.
- **Nothing to migrate:** the test world's Stories can be dropped, by the no-migration rule.
- **Tests** gain: an unofficial book made official by agreement; a finished official book paying its tellers and
  Story Stars, and counting for Quickened and Chapter renown; an unofficial one paying nothing.

**Open questions for Phase 7:** none.

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
  - Scenes that ended more than eight days ago are let go in `tick` (the user, 2026-10-07: "Make scenes live for 8
    days before being deleted, but tell the users it only lasts for 7"; `KeepEndedDaysTold` is the seven players
    see). Stories and stars look back a day at most.
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
    many wolves, the openness, and (added the same day, the user's request) who is in them, for a window on hover.
    Each wolf comes named as the viewer knows them, with its look and any portrait it may see, at most 48.
- **Client** (`story.ts`):
  - Each of one's scenes shows OPEN · KNOCK · PRIVATE, with the current one lit.
  - A knock shows as "… is knocking · LET IN · NOT NOW".
  - A scene here shows as "A SCENE HERE · OPEN / KNOCK TO JOIN · 2 wolves", with JOIN or KNOCK, or "knocked".
    Pointing at it opens IN THIS SCENE: each wolf's portrait and name, scrolling past about five.
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
    - an ended scene out of the indexes, kept eight days, then let go.
  - `Tests/scene_doors_tests.cpp` (new, 984 checks, most of them snapshot fills): through the game, a party's scene
    unseen by Cy until Ada opens it; Cy sees "2 wolves · open", and who is in it by the names she knows; she joins,
    and her line counts; a Knock scene
    refuses Join and ignores her talk; her knock reaches both its wolves with a line, shows in Ada's scene line and as
    "knocked" in hers; let in, her line counts; one who blocked her keeps the scene out of her list and refuses her
    join and her knock, without saying who.
    - While writing this file I first saved it over the existing `Tests/scenes_tests.cpp` (doc 30's ambient scenes,
      which had no local changes). I restored it from git; it still passes its 260 checks.
  - `tools/client/scenes.mjs` grows a knock: Bo makes his scene KNOCK; Cy sees "A SCENE HERE · KNOCK TO JOIN · 1 wolf"
    and points at it, which shows him in a window with his portrait; she knocks; Bo sees it with LET IN and lets her in;
    her line counts. Screenshots 8 and 9.
  - `ctest` 54 of 54; client tests 107; `card`, `stars`, `friends`, `safety`, `party` and `names` pass in a real page.
- **Cost:**
  - Routing is a lookup into the actor's own scenes, then the open scenes in that place, instead of every scene ever.
  - The scene views are O(players × their own scenes), plus the open scenes in each player's place for `nearby`.
  - Scenes are let go eight days after ending.
  - Nothing new runs in the world's tick.

### Phase 4: seeing scenes in the log and on the map (built 2026-10-07, not committed)

- **Colours:** `Data/Social/social.json` gains `scenes`: a palette of six (teal, violet, sky, moss, rose, slate, with no
  red and no party gold) and the map's reach, 60 world tiles. `stars::sceneColour(id)` hashes a scene's id into the
  palette, the same colour for everyone.
- **Lines carry their scene:**
  - `SocialLedger::routeFor(actor, cell, party, now)` finds the scene a line will go to before it is said: the
    speaker's own scene in their party's or fight's lane, else the room's, else one they joined or were let in to.
  - `Game::publish` adds `scene` to each listener's copy:
    - `{mine, colour}` for the scene's wolves, and for the speaker's own copy (so a wolf just let in sees their first
      line as their scene's);
    - `{openness, place, colour}` for anyone else, when the scene is Open or Knock;
    - nothing for a Private scene's or a fight's lines, which read as ordinary talk.
  - Never an id, so an anonymous voice stays anonymous.
  - Lines that join an Open scene by being heard, or begin one by A–B–A, aren't tagged (§7 allowed that). Residents'
    lines aren't tagged either.
- **Open scenes on the map:** `self.social.openNear` lists, within 60 world tiles:
  - Open scenes;
  - Knock scenes only when one of the player's friends is in them (the user's answer);
  - never Private scenes, fights, scenes the player is or was in, or scenes with someone who blocked them.

  Each comes with its wolves' middle, or for a scene indoors, the door into it from outside (found once a refresh,
  not once a player), plus how many wolves, its openness, the place's name and its colour. The server always sends
  the list (counts and places, never names); the page shows it only when the player turns it on.
- **Client:**
  - **Feed:** a line from one's own scene has its left edge in the scene's colour (party lines keep their gold). An
    open or knock scene's line nearby has a dashed edge and the tag "scene at The Bent Bough · open" or "· knock to
    join". Nothing is faded.
  - **MY SCENE ONLY** beside the IN WORLD feed's label keeps one's own scenes' lines, one's own words, the world's
    lines and anything said to the player (`keptByMyScene`). It's kept in the browser (`ratw.feed.myScene`), off by
    default.
  - **Settings:** "Scenes on the map", off by default (the user's answer), kept in the browser (`ratw.map.scenes`).
    When on, the minimap and the World Map show a small speech mark in the scene's colour with how many wolves.
    Pointing at one on the minimap says "3 wolves at the Wharf tavern · open", or "· knock to join (a friend is in
    it)".
- **Tests:**
  - `Tests/scene_doors_tests.cpp` (1539 checks), covering:
    - a Private scene's line is "mine" with its colour for its wolves, and plain for an onlooker; it isn't on the map;
    - opened, an onlooker's copy says open and where, never the id, in the same colour as the wolves' copies; it is on
      Cy's map with how many wolves, not who, and never on a member's;
    - a Knock scene isn't on Cy's map until her friend Ada is in it;
    - a wolf let in sees her own first line as her scene's.
  - `Client/src/game/people.test.ts`: a line keeps its scene; `keptByMyScene` keeps one's own scene, one's own words,
    the world and "→ you", and drops the rest; the toggle.
  - `tools/client/scenes.mjs` (23 checks; screenshots 10–12): Bo's line barred in their scene's colour for Cy; Di, a
    stranger, hears it, and MY SCENE ONLY takes it out of her log and puts it back; Bo opens the scene; it's in Di's
    open scenes, and with "Scenes on the map" on, its mark shows at the tavern's door with "2".
  - `ctest` 54 of 54; client tests 108; `stars`, `friends`, `card` and `safety` pass in a real page.
- **Cost:**
  - A line: one `routeFor` (lookups into the speaker's own scenes and the scenes here), then a membership check per
    listener inside the loop `publish` already runs.
  - Every two seconds: `openNear` for each player is O(open scenes × their wolves), with the doors into indoor scenes
    found once.
  - Nothing runs in the world's tick.

### Phase 5: end screens, and fights as part of their scenes (built 2026-10-07, not committed)

- **Fights are part of scenes** (the user, 2026-10-07: "make sure that fights become part of Scenes, or at least the
  action-by-action log of the fight"):
  - A fight's scene finds the scene its players were in at that place (`SocialSession::parent`) and becomes one of its
    moments.
  - While the fight lasts, that scene stays alive, so it can't go quiet and end under them.
  - The fight's action-by-action log (`Battle::told`: every line's words, up to 200, beyond the 60 the page keeps) is
    kept with the fight's scene (`SocialSession::log`), saved, and outlives the battle.
  - The scene's card links to it. A member's recap (doc 50) includes it, as that wolf would read the names.
  - `fightlog {session}` sends it, veiled for the reader, to one who was in the fight or in the scene it broke out in.
- **A fight's figures:** `BattleFighter::Tally`:
  - `landed` and `guarded` are counted in `World::fightLine` by line kind ("hit", "graze" and "slash"; "guard");
  - `raised` counts a tend or a Gift's rise;
  - `dealt` and `taken` are counted in `World::hurtFighter`, where all damage lands;
  - `covered` is an Interpose, counted where it happens.
  - At settlement each player's tally is copied into their part of the fight's scene, so it's saved.
  - This narrows §8: "covered" counts Interposes only, not blows taken on guard beside a Downed ally.
- **Moments** (`SocialSession::moments`, at most 24, ids only, saved):
  - "joined" (Join), "admitted" (a knock let in), "introduced" (an introduction between two wolves in one scene),
    "story" (began or carried on a Story), "fight".
  - `Game::sceneMoments(viewer, scene)` puts them in words for the viewer, at most eight, in the names it knows ("you"
    for itself; introductions gathered: "Cy told you and Bo One their name.").
  - Then what the wolf noticed for itself when the scene ended (`endedNotes_`): "You and Wren shared a scene for the
    first time." (from Known wolves) and "How Wren regards you now: they know you a little." (only when it changed
    since it last saw it).
  - The `bond` wording differs from §8's example, so it reads right for any name. The chorus, storyline steps,
    gifts, grooming and deeds are hooks for Phase 6 and docs 55, 56 and 58.
- **The card** (`self.social.ended`) gains `place`, `minutes`, `with` and `moments`; a fight's adds its `tally` and
  `logLines`.
  - A wolf who steps out of a scene now gets its card at once (`Contribution::leftAt`, saved), marked "goingOn", with
    stars once the scene really ends. Before, it showed only when the whole scene ended.
  - No word or turn counts anywhere.
- **Client:**
  - The scene's card reads "THE SCENE AT THE BENT BOUGH · 40 MIN · +20 SOCIAL", then "With …", a fight's figures
    ("9 landed · 1 raised · 2 on guard · 40 dealt · 22 taken"), the moments, and THE FIGHT, BLOW BY BLOW (the log,
    fetched when first opened, in a scrolling box).
  - The fight's result card (`combat.ts`) uses the server's tally once the fight's scene settles (landed, dealt, taken,
    raised, covered, on guard), falling back on summing the page's log until then, with the moments beneath.
- **Tests:**
  - `Tests/battle_tests.cpp` (`fightTally`): a fight past 90 lines keeps its whole log beside the last 60; every blow
    that landed is counted; what one dealt is what the other took; turns on guard.
  - `Tests/scene_doors_tests.cpp` (`endCards`, 2002 checks in all):
    - Cy joins Ada and Bo's open scene and tells them her name;
    - Ada and Bo fight: the fight's scene belongs to theirs and is one of its moments;
    - a truce ends it, its log is kept, and their scene goes on;
    - Cy reads the log;
    - the scene ends, and Ada's card has the place and who was in it, "Cy joined the scene.", "Cy told you … their
      name.", "A fight broke out" (linked to its log), "You and Cy shared a scene for the first time.", and no word or
      turn counts.
  - `Client/src/game/people.test.ts`: the fight log kept.
  - `tools/client/scenes.mjs` (25 checks): when Ash steps out, her card shows the place, its pay, who was in it and a
    first scene together, with no counts (screenshot 4b).
  - `ctest` 54 of 54; client tests 109; `stars`, `friends`, `card`, `safety` and `party` pass in a real page.
  - `tools/client/fight.mjs` is still the stale one noted earlier, so the fight result card has no real-page check.
- **Cost:**
  - A few integers per fighter, and a fight's lines kept once.
  - Each fight's scene finds its parent once; while the fight lasts, one lookup keeps the parent alive.
  - Moments are recorded at events, and put in words only for each player's last scene during the hour its card
    shows.
  - A fight's log goes only when asked for.

### Phase 6: the gathering howl and chorus (built 2026-10-07, not committed)

- **Data:** `Data/Social/social.json` gains `howl` (all placeholders, read into `stars::rules().howl`):
  - a range of 150 world tiles, a height limit of 30, and ×0.4 for a howler indoors, ×0.5 for a listener indoors;
  - a ten-minute cooldown, a one-minute mark, and ±12° of jitter;
  - the chorus: within 40 tiles and 8 s, +4 s for each joiner, 20 s at most, carrying +25% a wolf up to ×2;
  - residents: a sneak given away within 32 tiles; up to 6 near (60 tiles) turn toward it and 2 speak; a town
    remembers it for 600 s;
  - residents' lines by "day", "night" and "nightTown".
- **The game** (`Core/RatwGameHowl.cpp`, new; the `howl` command):
  - **Refused** in a fight or a hunt, Downed or dead, held by the watch, with something in the mouth, or within ten
    minutes of the last howl. `howledAt_` uses world time, so fast-forward shortens it, and isn't saved.
  - **A chorus:** a howl within 40 tiles and 8 s of an open chorus joins it ("You join the howl.") and keeps it open
    longer, to 20 s at most.
  - **A real sound:** residents within 32 tiles of a sneaking howler hear it (`World::heardVoice`), as with a yell.
  - **Who hears it** (`sendHowl`): players within range, scaled by the listener's hearing (`World::hearingSensitivity`,
    taken out of `hearingClarity` so both use it), the weather where they are, indoors on either side, and the
    chorus's carry. Not across 30 of height, not the howlers themselves, and nobody who muted or blocked a howler.
    - Each gets a `howl` event: the chorus's id, a jittered bearing, a band (near, far, very far), the first howler's
      status ("Looking for a scene", "Out of character"), how many howl, `canJoin` (within 40 tiles, rested, still
      open) and how long the mark lasts.
    - Never a name or a place.
    - The first time, a line: "A howl rises to the north-east, far off. You could join it."; later, "More wolves join
      the howl to the north-east: 3 now.".
  - **Residents** (`residentsHear`, at a chorus's first howl): up to six nearest within 60 tiles turn toward it, and up
    to two say an authored line: curious by day, grumbling at night in a town. No model call. Festivals' cheering
    waits for doc 54.
  - **When a chorus closes** (`tendHowls`, five times a second):
    - each pair of players in it gains a little familiarity and affinity, once a pair a game day;
    - it becomes a "chorus" moment of every scene its howlers are in ("You howled together, 2 of you.");
    - the town where it rose remembers it for a game hour. `dialogueContext` tells residents there "A little while
      ago, 2 wolves howled together near the Bent Bough."; the game says what happened, and the Mind only mentions it.
  - Choruses and towns' memories are in memory only. Animals reacting is doc 53's.
  - `self.social.howlIn` gives the seconds until the wolf may howl again.
- **Client:**
  - **Howl** among the actions (resting, with the minutes in its tooltip), and `/howl` in the composer.
  - The `howl` event plays a howl, quieter from far off, the first time a chorus is heard.
  - **The mark:** a faint arrow at the edge of the minimap (and the World Map, which shares its drawing) for a minute,
    the way the sound came. Pointing at it says "a howl of 2 wolves · Looking for a scene · far".
  - **JOIN THE HOWL** on a row in the scene bar ("A HOWL TO THE SOUTH-EAST · NEAR") while one may.
  - **Settings:** "Howl marks: On / Off". The sound always plays, being in the world.
  - This narrows §9: no line drawn from the wolf on the World Map, and no panning of the sound by bearing.
- **Tests:**
  - `Tests/howl_tests.cpp` (new, 750 checks, most of them snapshot fills), covering:
    - Bo hears Ada's howl near, could join, from the west within the jitter, with no name or place, and a line;
    - deaf Cy hears nothing;
    - a resident near says one of the lines;
    - a second howl must wait;
    - not with a sword in the jaws;
    - a chorus: Bo joins, Cy has one mark now of two wolves and is told; when it closes Ada and Bo are closer, and it
      is a moment of their scene;
    - Cy, who blocked Ada, hears nothing of her howl, while Bo does.
  - `Client/src/game/people.test.ts`: one mark per chorus, its count updated, the sound once, `/howl` sent as a
    command and never said.
  - `tools/client/howl.mjs` (new, 10 checks; screenshots in `artifacts/screenshots/howl/`): Ash howls from the button;
    Bo hears it with a line and JOIN THE HOWL, and joins; Cy sees one mark of two wolves; Ash's Howl button rests.
  - `ctest` 55 of 55; client tests 110; `scenes`, `stars`, `friends`, `card` and `safety` pass in a real page.
- **Cost:**
  - A howl: one pass over the players to decide who hears it, and one over the places (by world position) for
    residents near. At most one a wolf every ten minutes.
  - Five times a second: a check of the open choruses, which last 20 s at most.
  - Nothing else in the tick.

### Phase 7: Story books and the bookshelf (built 2026-10-07, not committed)

- **Data:** `Data/Social/social.json` gains `books`, all placeholders:
  - lengths: title 80, chapter title 60, a chapter summary 600, a book's 1200, flavour text 300;
  - time: scenes may be added for 7 days (what players are told; scenes are kept 8); 30 days counts as "recently
    active"; 3 quiet days finish a book;
  - limits: 10 model calls a day, 20 open books a keeper, 200 chapters and 100 wolves a book.
- **The rules:** `Core/RatwBooks.h/.cpp` (new, pure, `ratw::books`) define `Book`, `Chapter` and `Link`, and provide:
  - `recentlyActive` and `finishes`: a majority of the wolves in a chapter in the last 30 days, or three days after
    the proposal with no objection;
  - `inverse` (sequel ↔ prequel), sharing and link checks, and save and load.
- **The game:** `Core/RatwGameBooks.cpp` (new; the `book` command):
  - **Linking a scene, any time** (`linkScene`):
    - *during*: an open scene;
    - *after*: one ended within 7 days, from the book or ADD TO A STORY on its card;
    - *before*: "next", whose next scene goes in when it ends (`booksSceneEnded`, from `endScene`).

    Only a wolf who was in the scene may link it. It goes after the chapter chosen (or at the start, or at the end),
    and its wolves join the book.
  - **A chapter keeps** its scene's place, time and wolves, and whether it was Private, so it outlives the scene. Its
    end and wolves are filled in when the scene ends.
  - **Titles and summaries:** any of the book's wolves may write them. FROM MY RECAP uses the wolf's own recap of the
    scene (doc 50), marked the model's when the recap was. *(Since 2026-10-07: with chapters before it, `/book` mode
    `chapter` tells it on from them; see "Scenes told as stories".)*
  - The book's summary is written by hand, or by WRITE IT FOR ME: the small model through the Mind's new `/book`
    ("summary"), within 10 a day, else written from the chapters' titles.
  - **Reorder** (any wolf); **take out** (the keeper or whoever added it, before finishing).
  - **Sharing** (the keeper): its wolves only, friends, one of the keeper's circles, the keeper's Chapter, or everyone.
    **Hide me from its readers** (each wolf).
  - **Finishing** (the user's rule):
    - the keeper proposes, with the warning about private scenes on the proposal and on every agreement;
    - it's finished once a majority of the recently active wolves agree, or after three days with no objection
      (`tendBooks`, with the friends' ten-minute upkeep);
    - finished, it gets flavour text: written at once, and the model's (`/book` "flavour") when it comes;
    - its private scenes' summaries open to every reader; before that, readers see only those chapters' titles and
      dates.
  - **Volumes:** two finished books link as related, sequel or prequel, both ways. A reader sees the whole volume,
    as far as they may read it.
  - **Official** (books are the one kind of Story):
    - MAKE IT OFFICIAL proposes the ledger's Story from the book's first ended scene;
    - AGREE approves it;
    - once it's agreed, the book carries its Story on with every ended chapter (`syncStory`, as far as the ledger's
      rules allow: an ended scene that paid two of them);
    - finishing tells it, which pays as doc 32's Stories do (social XP, Chapter renown, Quickened's count);
    - Story Stars are given from the finished book.
  - **The shelf** (`sendShelf`, forty spines at a time, newest first):
    - YOUR SHELF is the books the wolf is in, filtered World / Chapter / Circle / Friend / Other;
    - UNAFFILIATED is books it isn't in that its friends, circles or Chapter share with it, books shared with
      everyone, and the world's storylines, filtered the same way, with Everyone in place of Other.
  - **World** is the DM's world storylines (the user's answer). `tieBookToStoryline`, the DM action `book.storyline`
    (validated and queued by `tools/dungeon_master.py`), ties a book to one. The DM app has no button for it yet.
  - **A book's view** (`bookView`) gives names as each viewer knows them and leaves hidden wolves out. For its wolves
    it adds the scenes they may add (the last 7 days) and, once it's finished, their other finished books to link it
    to.
- **Saving:** `people.books`. `Database/migrations/0041_story_books.sql` adds `game.story_books`, which the DM may read
  and the editor and publisher may not. **Not applied yet.**
- **Client:**
  - **STORIES** (from the character sheet, in place of the old Stories list) is the bookshelf:
    - books as spines on wooden shelves, coloured by shelf (World, Chapter, Circle, Friend, Other), gilded when
      finished, ★ when official;
    - the newest shelf at the top, scrolling down, more loaded at the bottom;
    - the two tabs and the filters, and START A BOOK.
  - **A book**:
    - its flavour (finished) and summary, its wolves, its volume, and the private-scene warning;
    - finishing progress with AGREE and OBJECT, AGREE TO MAKE IT OFFICIAL, and ★ Story Stars;
    - the chapters as a timeline: number, title, place, date, length, private, wolves, and summary or sealed;
    - for its wolves: edit, FROM MY RECAP, ↑ ↓, TAKE OUT, ADD THIS SCENE (with "at the end", "at the start" or
      "after 2. …"), "My next scene goes into this book", the summary with WRITE IT FOR ME, sharing, FINISH, MAKE IT
      OFFICIAL, "Hide me from its readers", and LINK INTO A VOLUME once finished.
  - **The scene's card**: ADD TO A STORY (any scene, not only paid ones) offers the open books ("INTO …") or START A
    BOOK, in place of MAKE IT A STORY and ADD TO.
- **Tests:**
  - `Tests/books_tests.cpp` (new, 715 checks, most of them snapshot fills), covering:
    - the pure rules: a majority, three quiet days, an objection, both ways for volumes, a round trip;
    - through the game:
      - a book begun from a scene under way (private, both wolves); Bo's next scene going into it;
      - a chapter summarised from Ada's recap, one titled by hand, reordering;
      - members only, then shared with friends: Cy finds it under Unaffiliated (a friend's) and reads it with its
        private scene sealed; it's on Ada's shelf under Other;
      - made official and agreed;
      - after a restart: finishing with the warning and Bo asked, a majority, flavour, an official book paying, Cy
        reading the private scene; no more chapters;
      - a second book finished only with both its recent wolves; linked as a sequel, its volume showing the first as
        its prequel;
      - tied to a world storyline and on the World shelf.
  - `tools/test_npc_mind.py`: `/book` (summary and flavour, bounds, the small model, nothing of the chapters in the
    log, over HTTP).
  - `tools/test_game_tables.py`: the books become rows.
  - `tools/test_dungeon_master.py`: `book.storyline` checked and queued.
  - `Client/src/game/people.test.ts`: the shelf appended forty at a time, a book opened, the commands.
  - `tools/client/books.mjs` (new, 14 checks; screenshots in `artifacts/screenshots/books/`):
    - Ash begins a book from the scene's card;
    - its spine is on her shelf; opened, it has one chapter;
    - Bo's next scene goes into it;
    - shared with friends, proposed with the warning, Bo agrees from the book, and it's finished;
    - gilded at the top of Ash's shelf;
    - Cy finds it under Unaffiliated and reads it with its private scenes open.
  - `ctest` 56 of 56; client tests 111; `scenes`, `stars`, `friends`, `card`, `safety` and `howl` pass in a real page.
  - The real model wasn't called: that costs money.
- **Cost:**
  - Books are read only when a shelf or a book is opened.
  - A scene ending checks the books for its chapters and "next" flags. That is a pass over the books; an index by
    scene would be the next step if books number in the thousands.
  - A model call for a book's summary or flavour, at most 10 a character a day.
  - Ten-minutely: a check of the books being finished.
  - Nothing runs in the world's tick.

#### The real model, tried (2026-10-07, with the user's leave)

Five calls through the Mind's `/recap` and `/book` on the small model (`gpt-5.4-nano`):

- **Two scene recaps:**
  - the bandit tale at the Bent Bough, with a fight's log;
  - Bo and Wren at the pier.

  Both are in the second person, with every wolf by its label and no invented names. Claims are reported as claims
  ("claimed they'd seen bandits", "the ferryman said the cable was frayed").
  - The first try gave the grey wolf "she", which the lines never said. `RECAP_RULES` and `BOOK_RULES` now say to use
    "they" unless the lines do; the retry used "they".
  - One slip remains: the small model read "Forgive me" as "you said you'd forgiven them". Recaps are marked as the
    model's, and each player may delete theirs.
- **A book's summary** (590 characters) and **its flavour text** (260 characters, "At dawn, they rang the drowned bell
  from the water's edge…"): both use only the chapters, within their limits.
- **Cost, in tokens:**
  - a recap: about 500 in and 120 out;
  - a book's summary: about 380 in and 130 out;
  - flavour text: about 380 in and 70 out.

  The plan's estimate for recaps was 1,250 in and 150 out, so they come in well under. Each call took 2–3 seconds.
- The Mind's ready line now lists `/recap` and `/book` too.

#### Scenes told as stories (2026-10-07, the user)

The user asked for the model to see the wolves' profiles instead of guessing pronouns, for a scene to be written up as
a story of up to about 1,000 words, and for a book's later chapters to carry on from the earlier ones instead of
describing the same wolves again. They also asked for a better model than mini for stories.

- **What the model is given:**
  - *The lines as the wolf perceived them, with their markup.* Every in-character post that reached the wolf, whole:
    what was done in `*asterisks*` and what was said in `"quotes"`. Before this the game joined a post's pieces as
    bare text, so the model couldn't tell action from speech. The wolf's own lines go by its name, not "You". A
    fight's log follows as "The fight".
  - *The wolves, as this wolf could see them.* Each one's label, pronouns and description, with "Currently:" added,
    from its card as this wolf may see it (`Game::cardFor`). A mature description folded for this viewer stays out, and
    so does the out-of-character tab. Pronouns follow the character's sex (see below). Up to 12 wolves, the player's own
    first.
  - *In a book, what came before.* The book's title and its latest eight chapters before this one (each cut to 1,500
    characters), leaving out a private chapter this wolf wasn't in until the book is finished. The Mind marks any wolf
    those chapters already name as `met` and leaves out its description, so it isn't described all over again.
- **What it writes (`RECAP_RULES`):**
  - fiction, not a summary: third person, past tense, close to the player's wolf, in paragraphs;
  - it opens on the place and the wolves (only those not met), then tells the scene beat by beat, quoting the lines
    that matter;
  - its length follows the scene, at most 1,000 words (the Mind cuts at 1,000 words or 7,000 characters);
  - claims stay claims, nothing is invented, and labels are kept.

  A later chapter is told just as fully; it only leaves out what the earlier chapters described.
- **Into the book:**
  - *A scene linked before or during it.* When it ends, its story is written with the book's earlier chapters, and
    fills that chapter's summary if no one has written one.
  - *A scene linked after it.* FROM MY RECAP takes the wolf's story at once. If there are chapters before it and the
    story was the model's, `/book` mode `chapter` then tells it again as the next chapter. That is within the book's
    daily limit, and only if no one has edited the chapter meanwhile.
- **Male or female (the user, 2026-10-07):** characters are created male or female, with no other options offered.
  Pronouns are no longer a profile setting: `Game::pronounsOf` gives she/her or he/him from the character's sex for
  its card, a resident's context and a story. The profile editor's PRONOUNS box and the "they/them" choice are gone; a
  profile change naming pronouns is refused, and a saved one is skipped when a save loads. The model uses "they" only
  for a voice no one saw.
- **The story model:** a new config setting, `story_model` (the main voice when unset), writes scene stories and every
  `/book` call. It is set to `gpt-5.6-luna` (doc 28). Story calls have their own two slots and a 50-second time limit
  (the game waits 60), so a long story never holds up a resident's answer.
- **Limits raised:**
  - a recap (`recaps.most`) and a chapter's summary (`books.summary`) are 7,000 characters;
  - the chapter box is a text area;
  - stories show in paragraphs and scroll past 260 pixels.
- **Tried on the real model** (`gpt-5.6-luna`, two scenes, about $0.001 each):
  - *Chapter one.* A 24-line scene at the Lantern between Ada (she/her) and Bram (he/him), with a scarred brown wolf
    and a short fight. It came out as 495 words in 6.6 seconds, 1,458 tokens in and 738 out. It opens on both
    wolves from their descriptions, quotes the dialogue, and tells the fight's log as prose.
  - *A first draft of the rules gave a 125-word report.* The rules now insist on fiction.
  - *Chapter two.* The next day at the weir, in the book "The Salt Road", with chapter one before it. It came out as
    300 words in 5.1 seconds. Ada and Bram are not described again, the new red wolf is, and every line is there. A
    middle draft left the descriptions out but squeezed the scene into a summary; the rules now say a later chapter is
    told in full.
