# 40. Sneaking: against players, NPCs and animals

Planned 2026-10-04 for the combat session to build. Hunting (doc 41) is built around it and ships first with a
placeholder for noticing (*The seam hunting leaves*, below). All numbers are *placeholders* for play-testing.

The user's brief: hunting means "sneaking up on the target and attacking it". Sneaking isn't in combat yet. This doc
says what it needs everywhere a wolf can be noticed: in the open world and inside a fight's arena, against players,
residents and animals.

## What exists (read from the code 2026-10-04)

- **Crouching** (posture `crouching`; moving while lying becomes a crouch): 0.30× speed, walking pace only
  (`Core/RatwStep.cpp`).
- **Being seen** (`World::visionClarity`, `Core/RatwWorld.cpp`): sight range is 27 tiles × vision × age × eye health ×
  the light and weather (`environment.sight`). A crouching wolf can't be seen beyond `(7 − 4·sneakSkill/100) ×
  range/27`. Line of sight is shared with the browser (`Core/RatwSight.h`). **No field of view:** facing doesn't
  matter, and there's no cover.
- **Being heard** (`World::movementAudibility`): a moving wolf's pawsteps carry 6 tiles, a crouching one's
  `2.5 − 1.7·sneakSkill/100`. A still wolf is silent. **Pace makes no difference.**
- **Being smelt** (`World::scentClarity`, `windAt`): scent carries downwind, gets around corners but not through walls,
  and is weaker in rain and storms. Sneaking does nothing to it (doc 02 says so on purpose).
- **Skills:** `sneakSkill`, `hearingSkill` and `scentSkill` exist (0–100) but nothing trains them.
- **NPCs noticing:** only at events: crime witnesses (`World::witness`), guards joining a fight they see, companions
  spotting hostiles, an NPC hearing a player speak. Nothing tracks an NPC being "alert" to someone.
- **Stealing** already uses crouching and `sneakSkill` in its odds (`Core/RatwCrime.cpp`).
- **Fights** (doc 33, `Core/RatwBattle.cpp`):
  - Both starters get a full initiative bar.
  - Everyone is turned to face their nearest foe.
  - The starter goes first, and NPCs wait for them.
  - A blow from the side gets +0.1 to hit, from behind +0.2, with hit zones by side.
  - Inside the arena every fighter sees everything, and there's no crouch.
- **Players can't be attacked without consent:** a fight between players is a challenge the other must accept
  (doc 33).

## What sneaking needs

### 1. Noticing, as one model

One function decides whether an observer has noticed a wolf. Everything below uses it: the open world, residents,
animals, and fighters in an arena.

`notice(observer, target)` gives 0..1 from:

| Factor | Effect |
|---|---|
| Sight | `visionClarity`, plus a **field of view**: full in front (±60°), half to the sides (±60–120°), none behind. Asleep or eyes shut: none. |
| Light and weather | already in `environment.sight`. Night and fog matter most. |
| Cover | the target standing in tall grass `"`, ferns `&`, reeds `E`, a shrub `B` or heather `5`, or behind a tree, log or boulder next to it: ×0.5 while crouching, ×0.8 standing. |
| Posture | crouching: today's short sight limit. Lying still: half that again. |
| Pace and noise | a still wolf is silent; walking is quiet (crouching: today's crouch range); trotting is today's 6 tiles; sprinting is 12. Noise is heard through `hearingClarity`'s rules (walls, weather, wind). |
| Scent | downwind of the observer (the air carrying the target's scent to them): `scentClarity` counts as noticing, faint at first. Upwind: none. A wolf's own nose works the same way. **Sneaking still does nothing to scent:** a good stalker keeps the wind in their face. |
| Skill | the target's `sneakSkill` against the observer's `hearingSkill` and `scentSkill`. |
| Attention | an observer already alert keeps noticing for a while (below). |

**Awareness states**, kept per observer per target (only for observers that need them: residents near players, and
animals in a hunt):

- **Unaware:** going about its business.
- **Suspicious** (noticed a little, under a threshold): turns towards it, stops, sniffs. Residents say so ("Who's
  there?"), animals freeze and raise their heads. Each further notice adds to it. With no more notice it calms back
  to unaware.
- **Alert:** knows where the wolf is. Residents react by their role (guards come to look, others back away), and
  animals flee or fight (doc 41). It fades to suspicious after a while unseen.

The state must be cheap: computed on a slow tick (a few times a second near players, never far away), and kept only
for pairs that are near each other.

### 2. Sneaking in the open world

**Against residents (NPCs):**

- Residents get the awareness states above, for players within their sight or hearing.
- Uses:
  - getting past a guard;
  - stealing (replacing the `.3 × visionClarity` term with the victim's awareness);
  - a crime's witnesses (`witness`): only those aware of the offender name them. A suspicious witness saw
    "someone they couldn't make out".
  - an ambush (§3).
- Guards: suspicious makes them come and look; alert on a wanted wolf starts the pursuit (it needs sight now).

**Against players:**

- A player sees what their character notices: an unnoticed wolf isn't drawn, as today.
- New: a *suspicious* player gets today's anonymous cues ("unseen pawsteps", "scent: a wolf, roughly NE") instead of
  nothing, using the same sector arrows.
- Fights between players still need consent (a challenge), so sneaking on players is about getting close unseen, not
  a free first blow.
- *Open question:* may a challenge be issued from hiding, and if accepted, does the challenger keep an ambush (§3)?
  *Recommendation:* no ambush between players; hiding only buys position.

**Against animals:** animals exist only inside a hunt (doc 41), so their sneaking is all in the arena (§4).

### 3. An ambush: starting a fight unnoticed

When a fight starts and a target is **unaware** of its attacker:

- The target's bar starts empty, not full, and it keeps the facing it had. It isn't turned to meet the attacker, so a
  blow from behind is possible. Others who join keep today's rules.
- The attacker's first blow on an unaware target gets the "behind" bonus whatever the facing, plus an **ambush
  bonus**: +0.15 to hit and ×1.5 damage *(placeholders)*. It also aims, so hit zones favour the throat, head and body
  over legs.
- After that first blow the target is alert, and so is everyone in the fight who saw or heard it.
- NPC allies of the target who are unaware come in late: their bars start empty.

### 4. Sneaking inside an arena

Fights today assume everyone sees everything. Sneaking needs that to change, but only where it matters:

- **Stalking**: a fight move made crouched. Steps take twice as long *(placeholder)* and the move is half as long,
  but noise is crouching noise. It's a toggle on the move, like the pace wheel, and an NPC can choose it too.
- **Being out of sight**: a fighter its foes haven't noticed (or have lost) is **hidden** from them.
  - NPC turns don't target it.
  - Players on the other side don't see its marker, only a "?" where it was last seen, fading.
  - Its own side sees it.
  - Breaking line of sight (trees, boulders, smoke from fire) and cover can make a fighter hidden again, if it is
    stalking and the others' notice falls low.
- **Noticing each turn**: at the end of each fighter's turn, every foe that isn't alert to it rolls notice (§1). Then
  each NPC's turn and each player's view use the result.
- **The first strike from hidden** works as an ambush (§3).
- **Animals** (doc 41) have their own senses in the data (`sight`, `hearing`, `smell`: a deer's nose is better than
  its eyes, a pheasant's eyes better than its nose). The same model uses them. Wind in the arena is the cell's wind
  (`windAt`), so stalking downwind matters.

### 5. What the client needs

- A **Stalk** toggle in the fight's move controls (and the pace wheel greyed while stalking).
- Over each animal or NPC in a fight, its awareness *of you*: nothing (unaware), an "?" (suspicious) or an "!"
  (alert). This tells you whether it's working. *Open question:* should the other side's awareness be shown, or left
  for the player to read from behaviour? *Recommendation:* show it in hunts, not in fights against residents.
- The wind's direction on the arena (an arrow at its edge), and the cover tiles shaded.
- In the world: today's "crouching · sneaking" label, plus a small notice meter while a resident is suspicious of you.

### 6. Skills

- `sneakSkill` grows a little each time one stays unnoticed close to an observer (within half its sight range), and
  with each ambush.
- `hearingSkill` and `scentSkill` grow with noticing a sneaking wolf.
- Slower as they climb, like fighting skill.

### 7. Performance

- Awareness is kept only for near pairs and recomputed on a slow tick.
- Arena notice rolls happen once per turn, per foe, per fighter.
- The perf gate (`world_check --players 20`, doc 31 at up to 250) must not move.

## The seam hunting leaves (doc 41, built 2026-10-04)

Hunting ships before sneaking with a simple placeholder for noticing, in one place, for this work to replace:

- `World::animalNotices(const Battle&, const BattleFighter& animal)` (`Core/RatwHunt.cpp`) decides, at the start of an
  animal's turn, whether it has noticed a hunter. The placeholder:
  - a hunter within the species' `alert` tiles is noticed (twice that while the hunter moves at a sprint);
  - so is any hunter adjacent;
  - being hurt alerts it.
  - Replace it with §1 and §4: the species' `sight`, `hearing` and `smell` are already in `Data/Animals/animals.json`.
- `World::animalUnaware(id)` says whether an animal hasn't noticed anyone yet. §3's ambush bonus should use the general
  "unaware of its attacker" test, with this as the animals' case.
- Hunts start with the animals far off, unaware and facing away or grazing (they never auto-face the hunter). So when
  §3 and §4 arrive, the ambush and stalking apply to hunts without any change to hunting.
- A hunt's quality rule (doc 41: the hardest single blow decides the yield) rewards an ambush's ×1.5 automatically.

## Open questions

1. A challenge from hiding: any ambush between players? (Recommended: no.)
2. Show other fighters' awareness of you: in hunts only? (Recommended: yes, hunts only.)
3. Do residents remember being sneaked up on (a bond change, a rumour of "someone prowling")? (Recommended: guards and
   the victim of a theft only.)
4. Should a sneaking wolf's voice be quieter (doc 02's open decision)? (Recommended: a whisper stays a whisper; normal
   speech gives a hidden wolf away within its range.)

## Decided (2026-10-04)

The open questions above, as recommended: no ambush between players (hiding buys them position only); others'
awareness of you is shown in hunts only; only guards and the victim of a theft remember being sneaked up on; a whisper
stays a whisper, and normal speech gives a hidden wolf away within its range.

## Built: phase 1, fights and hunts (2026-10-04)

In `Core/RatwBattle.cpp` ("Sneaking"), with one change each to hunting's `animalNotices` and the unaware animal's turn
(`Core/RatwHunt.cpp`).

- **Noticing in an arena** (`World::arenaNotice`, 0..1), from fighters' tiles:
  - **Sight:** the observer's sight range (× an animal's `sight`), full in its field of view (its facing ±1 eighth),
    half to the sides, none behind; line of sight; nothing through smoke. A stalker is seen only within
    `(7 − 4·sneak/100) × range/27`, as a crouch in the world. **Cover:** a stalker on tall grass `"`, ferns `&`, reeds
    `E`, a shrub `B` or heather `5`, or with a tree or boulder beside it toward the observer, counts ×0.5 (standing
    ×0.8). The stalker's skill takes up to 30% off.
  - **Noise**, only for a move this turn: a stalk carries `2.5 − 1.7·sneak/100` tiles, a walk 3, a trot 6, a sprint
    12, by the observer's hearing (ears, `hearingSkill`, an animal's `hearing`), the weather, and ×0.38 without line of
    sight.
  - **Scent:** down the wind (the cell's `windAt`), `(4 + 14 × strength)` tiles × the nose (`smell`, `scentSkill`, an
    animal's `smell`, the weather), weaker the more across the wind; in still air or upwind only a tile or so. It
    counts ×0.6: noticed faintly at first. Stalking does nothing for scent: keep the wind in your face.
  - Senses fade as the world's do: full within their range, nothing at twice it.
- **Awareness** (`Battle::aware`, observer to target, for NPC and animal observers only): each check adds notice ×
  1.2; one with next to nothing to notice calms it by 0.25. From 0.3 suspicious, from 1 alert (then held at 1.5, so a
  few calm checks lose it). Checked at the end of each fighter's turn by every NPC and animal on the other side, and
  by each NPC or animal at the start of its own turn. One already alert to a wolf that isn't stalking doesn't check.
  Everyone starts alert to everyone, except game in a hunt (unaware) and those an ambush takes unawares.
  - An NPC only goes for foes it is alert to; one it is only suspicious of, it turns toward and waits. A resident
    says "looks round, uneasy", "spots Ash"; game "lifts its head" and, suspicious, freezes and faces it instead of
    grazing; alert, it startles as before (doc 41).
- **Stalking** (C, `stalk`/`rise`, at any time but mid-move; a player crouched as a fight starts begins it): a move half
  as long, each step twice as slow; quiet; harder to see; cover counts. The token's rim is broken; the card says so.
  NPCs don't stalk yet.
- **An ambush** (§3):
  - Starting a fight: each NPC on the other side who wouldn't have noticed the attacker as it came in (its facing and
    the attacker's noise and scent as they stood) keeps its facing, starts with an empty bar, and is unaware of it.
    "The wolf drawing water at the fountain never saw Ash coming."
  - A blow on one unaware of its striker (`World::ambushing`; never between players) is struck as from behind, +0.15
    likelier still, guard or no guard, for ×1.5 damage, aimed (hit zones: the throat 40%, the head 20%, the shoulder
    35%, a leg 5%). "Ash strikes from hiding." After it the one struck and its side are alert to the striker. Game in
    a hunt is unaware until it notices, so the first blow of a good stalk is an ambush (and doc 41's hardest-blow rule
    rewards it).
  - The odds on the card and the preview say so ("95% · ambush", "~18").
- **The page:** a Stalk button (C) with its tooltip; a broken rim on a stalker's token; over game in a hunt, "?" once it
  has half noticed you and "!" once alert; the wind as an arrow in the arena's corner ("still air" without); cover tiles
  faintly green; words under the wolves ("?", "!", "from hiding"); C in the help line.
- **Skill** (§6): an ambush adds 0.5 to `sneakSkill`, and each check a player stalks unnoticed within half an
  observer's sight adds 0.05, both slower as it climbs.
- **Tested:** `Tests/battle_tests.cpp` `sneak::noticing` (in a hunt: behind and downwind unnoticed, upwind smelt, in
  front seen, the side half, a crouch seen closer, a sprint heard and a stalk not, tall grass hiding a crouch, the move
  halved, an ambush likelier and told, then alert) and `sneak::ambushAResident` (from behind in still air, crouched:
  her bar empty, still facing away, the first blow an ambush; from in front, none). `Client/src/game/battle.test.ts`.
  In the browser (Greyfen): set on from behind with the wind blowing toward her, a resident smelt Ash and turned to
  meet her; with the wind the other way, the same approach was an ambush (95%, ~18; "never saw ash coming", "strikes
  from hiding", a bite on the shoulder of 19).
- `hunt_tests` pass with animals noticing by these senses instead of by distance.

## Built: phase 2, the open world and the rest (2026-10-04)

- **One notice core** (`World::noticeSenses`) from any two places, by sense (sight, noise, scent); the arena
  (`arenaSenses`/`arenaNotice`) and the open world both use it. A wolf with masking oil on (doc 41's marks,
  `World::scentMasked`) gives no scent.
- **Residents notice players out of a fight** (`World::tendAwareness`, every 0.4 s): each resident awake and on its
  feet within 30 tiles of a player checks, from where it stands and faces; a crouch counts as a stalk and moving makes
  noise by the pace. Pairs drifting apart are let go. Awareness grows and calms as in a fight, and **alert fades** with
  nothing to notice (fixed: it used to be held for good, in fights too, so a stalker could never be lost).
  - A wolf creeping (crouched) half noticed: the resident turns its way, "Who's there?" (a guard: "Who goes there?"),
    and a guard on duty **goes to look** (an errand to where it was, for 20 s: "looking into a noise").
  - Spotted creeping: a guard says "You there! What's all this creeping about?", goes to look, and **remembers** it
    ("was prowling about": doc 40's decision, guards and the victims of theft only); anyone else keeps a wary eye.
  - **Guards** go after a wanted player only once alert to them, so a sneak can slip past the watch.
  - **Stealing:** a resident's watchfulness is its awareness of the thief, checked at the moment, not its sight of
    them. From behind and unheard, it is 0.
  - **Witnesses:** a resident names a player only if alert to them; only suspicious, it saw "someone they couldn't make
    out"; unaware, at most it heard the struggle. (Players witness by sight, as before.)
  - Creeping unnoticed within half a resident's sight teaches `sneakSkill` a little.
- **A voice gives a sneak away** (`World::heardVoice`): a resident who hears a player speak or yell (not whisper) is
  alert to them, in the world and in a fight.
- **Players lose sight of stalkers in a fight:** a player keeps track of a foe only while it stalks (anyone else is plain
  to see). One it has lost is sent as `hidden`, at the tile where it was last seen and how long ago: drawn only as a
  red "?" fading over ten seconds, its card greyed ("Lost from sight"), no odds, and actions don't aim at it.
- **NPCs stalk:** an aggressive one coming at a foe's back from more than two tiles off stalks (half the move, quiet),
  so a player who turns their back may lose it and be struck from hiding.
- **Ears and noses learn:** a player catching a stalker again learns `hearingSkill` (if the noise gave it away) or
  `scentSkill` (the scent), 0.3 a time, slower as it climbs.
- **Players' cues** for what they half notice were already there: "You hear pawsteps nearby" and the scent sectors.
- **Tested:** `Tests/battle_tests.cpp` `sneak::inTheWorld` (crouched behind a resident, still: never noticed, and a
  theft from there names no one; speaking aloud gives her away; creeping up in front: "Who's there?", then seen) and
  `sneak::aBanditStalks` (a bandit coming at a player's turned back stalks, and she loses sight of it). All 43 suites
  pass; a clang ASan/UBSan build of the fight and crime suites finds nothing. The perf gate (`world_check --simulate
  7 7.2 --players 20` on DEV build 22) is unchanged: mean 12.6–12.8 ms against HEAD's 12.6, p99 after the first minute
  21.4 ms against 21.3–21.8.

## Built: phase 3, the last of it (2026-10-04)

- **Bandits creep up in the open world** (`World::tendCamp`, `Core/RatwRoads.cpp`). A bold camp that sees a
  traveller worth robbing who hasn't noticed them (the traveller's own notice of the bandits, by the same senses, under
  suspicious) creeps up instead of stepping out: crouched (a sixth of a walk for an NPC), to a stride behind them.
  - The traveller isn't asked for anything (`banditDemand` is 0 while they creep) and isn't told they got clear.
  - What the traveller notices of them builds as in a fight: half noticed, "Something rustles low in the grass behind
    you"; noticed, "Bandits rise from the grass around you and rush you!" and the fight comes as before.
  - Unnoticed within reach, a bandit stops (no last pawstep to give it away) and springs: the fight begins with the
    traveller taken unawares (startBattle's ambush now covers an NPC setting on a player: never player on player),
    their bar empty, still facing away, and the first blow struck from hiding. "Ada never saw a gaunt highwayman coming."
  - A traveller who has noticed the camp (in sight, or downwind of it) sees them step out and ask, as before.
  - After 90 s at it unready, they give up and leave the traveller be for a while.
  - NPCs crouched as a fight starts begin it stalking.
- **Residents back away from a prowler:** one (not a guard) that spots a creeping wolf "backs away, watching you",
  given an errand of 8 s to a spot four strides off from the prowler ("backing away", "someone creeping about"), the
  same way a guard is sent to look.
- **Aiming for a hit zone** (Z, battle verb `aim` with `target` "throat", "head", "body", "legs" or "" for wherever;
  free, at any time, kept until changed):
  - a blow aimed lands on that zone if the side it comes at allows it (no head or face from behind); otherwise where it
    may;
  - it is 15% less likely to land (`battle::AimPenalty`), but not on one taken unawares (an ambush is aimed anyway);
  - the odds on a foe's card give the blow through the armour on that zone; the Aim button steps through the zones
    and says the one chosen ("Aim: throat", −15%).
- **The action bar:** with Aim, the move/action/facing marks moved into End turn (down its right edge, "✓M ✓A ·F"),
  so the bar stays one row.
- **Tested:** `Tests/roads_tests.cpp` `banditsCreepUp` (her back to them in still air: no demand, they creep, and the
  fight opens with her taken unawares; downwind of them she smells them and they step out and ask instead);
  `Tests/battle_tests.cpp` `aiming` (the throat, every time it lands; less likely; never the head from behind; no
  aiming for a tail) and `sneak::inTheWorld` (a resident backs away). `Client/src/game/battle.test.ts`. In the browser:
  an ambush in Greyfen, and the bar in one row.

**Still open, for play:** the numbers (the notice gain, the calm, cover, scent's weight, the aim penalty, how long a
camp creeps); whether players should notice an NPC sneaking in the open world (only bandits creep, and a player gets
the rustle and the anonymous pawstep and scent cues); and doc 35's other leads on zones (a bite going for the throat,
leg hits slowing a wolf).
