# Combat feel: a fight screen that reads at a glance

Status: **plan, drafted October 4, 2026**, from a playtest of the built combat (doc 33) after its playtest fixes. Phases
1–5 were built the same day (see [Built](#built)); 6 and 7 are to come. Decisions taken the same day are marked **Decided**. Doc 33 keeps the rules; doc 18 keeps the encounter log. This
document is about how a fight looks, feels and flows.

## What a fight is like today

Played as a player, by mouse: a duel with a sword against the Fire Gift, two fights with residents, one fight watched.

**The rhythm is "wait thirteen seconds, act for two".** In a duel both bars fill together in real time, so most of a
fight is both players watching bars. A bot duel took 100 s for 8 turns each. A human one takes 3–5 minutes, with
nothing to do between turns: you can't plan ahead, read the foe or prepare.

**Few real choices.** The winning play is to stand next to the foe, click them and end the turn. You rarely need to
move. Facing matters (+10/+20% from the side and behind), but you can't see that, so nobody plays for it. There is no
defensive move. Holding a sword spends a whole turn. "Roll" (put out flames) is a button with no explanation.

**Hits don't land.** Wolves are 10–13 px `W` glyphs. A bite is a small `*` and a 0.3-tile nudge. There are no damage
numbers, no health bars (only the words "Scratched", "Wounded"…), no flash and no shake. Fire is a red cone with a
countdown, then a few sparse `^` marks. What happened is only in the log text.

**The arena doesn't look like a fight.** The view zooms in, but the arena is the town's own busy glyph field. The
tiles you can reach are a big beige block that looks like paving and hides the ground. Where the arena ends, who
faces where and how far you can strike can't be seen.

**There's no start and no end.**
- It begins with a hard cut, with no "FIGHT" moment.
- It ends just as abruptly: back in the plaza, one grey line in the story ("The fight is over · Gale's side stands").
- There is no result, no "what now" for the winner, and no prompt to tend the loser.
- A friendly duel ends with the loser Downed on a 12-minute bleed-out clock that can end in permanent death.

**The screen is cluttered and has the wrong things on it.** During a fight, about half the screen is irrelevant:
- Listen/Look/Smell/Sit/End scene, the pace and walk strip, a 20-row In Sight list, the minimap, wind and exposure.
- What matters is scattered: whose turn (map banner and panel), health (text tags), stamina (side panel, not
  combat-specific), mana (small text), turn order (bars under each wolf).
- The fight panel is mostly instruction text ("Click a lit tile to move. Click a foe (or their name below) to bite…").
- Every control is a word; none can be read from its look.

**What works and is kept:**
- Overlapping turns: no one waits for a queue.
- The challenge prompt.
- Truces.
- Fire's visible countdown (a real "tell" you can dodge).
- Writing roleplay lines mid-fight.
- The red square and Watch from outside.
- Struggle up.
- The town remembering what you did.

## Goals

1. **Readable at a glance.** Whose turn, who is hurt, what I can do and what it would do, without reading a sentence.
2. **Hits feel like hits.** Every exchange has a moment you see and feel.
3. **Every second is yours.** Waiting is time to plan, not dead air.
4. **Real choices, few buttons.** Position, facing and timing matter, and the screen shows why.
5. **A beginning and an end.** A fight opens like a scene and closes with a result and a next step.
6. **Still a roleplay game.** Writing stays one keystroke away and the story still tells the fight in prose.
7. **Icons first, words second.** Every control can be read from its shape; words are in tooltips and the log.

## The plan

### 1. Fight mode: the screen changes when a fight starts

When you enter a fight the HUD changes to a combat layout. It changes back when you leave.

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│  ◉ Fen 0:04  ›  ◯ Dun wolf (2)  ›  ◯ Guard  ›  ◉ Fen          ROUND 3 · 1 watching │  turn order
├───────────────┬──────────────────────────────────────────┬───────────────────┤
│ THE FIGHT     │                                          │ ┌───────────────┐ │
│ Fen bites the │                                          │ │ [wolf] Fen    │ │
│ dun wolf. 12  │             the arena                    │ │ ███████░░ 74  │ │
│ The dun wolf  │      (everything outside it dimmed)      │ │ stamina ▮▮▮▯  │ │
│ cuts Fen. 22  │                                          │ │ mana    ▮▮▯▯  │ │
│ …             │                                          │ └───────────────┘ │
│               │                                          │ ┌───────────────┐ │
│ [composer:    │                                          │ │ [wolf] Dun (2)│ │
│  write your   │                                          │ │ █████░░░░ 53  │ │
│  part]        │                                          │ │ 🔥 burning 2  │ │
│               │                                          │ └───────────────┘ │
├───────────────┴──────────────────────────────────────────┴───────────────────┤
│  [1 🦷 Bite 75%·12]  [2 ⚔ Sword]  [3 🔥 Fire 25]  [⛨ Guard*]  [✋ Tend]  [🏳 Truce]  [↩ Flee]  [⏎ END TURN ▓▓▓░ 9s] │
└──────────────────────────────────────────────────────────────────────────────┘
```

(* Guard comes with phase 6; the built bar has Bite, Sword, Fire, Tend, Roll, Pick up, Stow, Truce, Flee and Yield.)

- **Turn order strip (top):** small portraits from the existing drawn-wolf art (`wolfArt.ts`) in the order turns will
  come, each with a filling ring; the one acting has a countdown ring. It replaces bars under every wolf and the
  "Acting now" text.
- **Combatant cards (right):** one per fighter, foes and friends grouped: a portrait, a health bar with its number
  (and the injury word as a tooltip), stamina and mana as pip bars, status icons (burning, away, holding a sword,
  downed with its timer). Hover a card to light that wolf on the map; click a foe's card to target it.
- **Action bar (bottom):** icon buttons with hotkeys 1–9. Each shows what it would do on the hovered or selected
  target: hit chance and damage ("75% · 12"), cost (stamina or mana) and why it's greyed out ("Too far", "Winded").
  End Turn is the big button with your turn's timer inside it.
- **Fight story (left):** the log as short lines with icons and the numbers called out, above the composer, which
  stays. Your roleplay and the fight read in one column.
- **Hidden in a fight:** Listen/Look/Smell/Sit/End scene, the pace strip, In Sight, the minimap and the weather
  header (they come back the moment you're out). Watchers get the same screen without the action bar.

### 2. The arena: built to be read

- **Dim the world outside the arena** and quiet the ground glyphs inside it (ground texture at low contrast, no
  decoration), so wolves and effects stand out. The arena's edge is a clear frame; the flee rows have a ↩ marker.
- **Wolf tokens:** a round token per fighter in the side's colour (you amber, friends blue, foes red), with the `W` and
  a facing wedge on the rim, and a health arc round it. Larger than today (about 70% of a tile). Downed tokens tip
  over and pulse; dead ones are grey.
- **Your reach:** the tiles you can reach are a thin outline with dots, not a fill. Hovering a tile shows the path and
  where you'd end up facing. Hovering a foe shows a strike line, the hit chance badge ("75%", "+20% behind") and
  greys tiles you couldn't strike from.
- **Facing you can see:** each wolf's front, sides and back as a faint three-part ring when hovered; your own facing
  set by dragging from your token (or Q/E), with the arrow grid as a fallback.
- **Ranges:** the sword's two-tile reach and fire's cone outlined before you commit; a second click confirms.

### 3. Feedback: hits that land

- Floating damage numbers (white hit, small grey graze, "miss" drifting off), colour-matched to the side.
- The struck token flashes and recoils; a heavy hit (sword, fire, 18+) adds a short screen shake (off with reduced
  motion).
- A health bar drains with a lag (red under white), so you see how much a blow took.
- Downed: the token falls over with a burst and the card goes red with the bleed-out timer.
- Fire: the cone fills with animated flame for a full second, ash afterwards, and Burning shows as a flame on the token
  and card.
- **Sound (Decided: yes, a small set):** short bite, cut, fire whoosh and a thud when someone goes down, and a soft
  chime on your turn. With a volume setting and mute in Settings. It is the game's first audio.

### 4. Moments: a start and an end

- **Start:** a half-second "versus" card (portraits, names, the terms) while the world dims to the arena. For a resident
  set on: their trade and "a crime: the watch will hear".
- **During:** "YOUR TURN" as a brief centred flash with a soft chime, then out of the way.
- **End:** a result card over the arena for a few seconds:
  - who stands, and what it cost you;
  - consequences ("Wanted for assault", "They yield");
  - next steps as buttons: **Tend them**, **Leave**, **Write**.
  - Then the fade back to the world, as today.
- **Duel terms (Decided):** a challenge names its terms:
  - **to first blood** (ends at the first wound);
  - **to yield** (ends when one side yields or would be Downed; no bleed-out);
  - **to the death** (today's rules).
  The default is "to yield", so a friendly duel never kills. A **Yield** action ends a fight you're losing, in any
  fight: the other side decides whether to accept.

### 5. Pace: every second yours

- **Plan while you wait:** while your bar fills you can choose your move and action ahead (shown as a ghost path and
  an icon over your token). When your turn comes it plays out, and you still have the turn's time to change it.
  Waiting becomes planning.
- **No dead air:** when no player is acting and no fire is gathering, bars fill faster (×2.5) until someone's turn comes.
  With everyone deciding quickly, a duel would take about half as long.
- **NPC turns are shown, not just run:** a short "the guard steps in… bites" with the token moving, so you can follow
  who did what without reading the log.
- **Decided:** keep overlapping real-time bars, with planning ahead and "no dead air" (not strict alternating turns,
  nor only faster bars).
- **Decided (2026-10-04):** the bars stay at 25 s (DEX 50) and "no dead air" stays at ×2.5, so a duel comes round in
  about 10 s at best; the 8 s target is dropped. Fights stay paced for reading and writing.
- **Decided (2026-10-04):** NPC turns play out briefly on the map, about a second each (the token walks, then strikes).

### 6. Choices: a little more to play with

- **Guard** (action): no attack; +20% dodge and facing turns toward an attacker until your next turn. The defensive
  answer a player can choose.
- **Shove** (action): push an adjacent wolf one tile, out of a doorway or into the flee rows.
- **Hold sword** while moving: taking up or putting down a sword becomes part of the move, not the whole action.
- **Roll** gets an icon and a tooltip ("put out the flames"), and shows only while you're burning.
- **Decided:** Guard, Shove and the sword in the move are all in scope.

### 7. Learning by doing

- The first fight shows three short tips anchored to the screen: "Your turn: move or act", "Hover a foe to see your
  chance", "End turn ends it sooner". Each shows once per character, and all can be switched off in Settings.
- Every icon has a tooltip with its key, its cost and what it does. With those, the instruction text in the fight
  panel goes.

## Phases

| Phase | What | Mostly |
| --- | --- | --- |
| 1 | Fight mode layout: turn order strip, combatant cards, icon action bar, the log beside the composer, the rest hidden | page |
| 2 | Arena readability: dimmed world, quiet ground, tokens, outlined reach, hover previews (path, hit chance), facing | page |
| 3 | Feedback: damage numbers, flash and recoil, lagged health bars, shake, downed fall, fire animation, sound | page |
| 4 | Moments: the versus card, "your turn", the result card with next steps; duel terms and Yield | page + server |
| 5 | Pace: planning ahead, no dead air, NPC turns shown | server + page |
| 6 | Choices: Guard, Shove, the sword in the move | server + page |
| 7 | First-fight tips, tooltips everywhere; a playtest against this document's goals | page |

Each phase is playable on its own. Phases 1–3 change no rules: they are the biggest gain for the least risk. Each ends
with a recorded playtest (screenshots and timings, as for this document) before the next begins.

## Measures

- A new player gets through their first fight without reading any instruction text (playtest with the panel text hidden).
- Median time between one's own turns in a duel: about 13 s when this was written (15 s bars); since the bars went to
  25 s at DEX 50 and turns to 20 s (doc 33) it is over 25 s. Target about 10 s with phase 5 (the 8 s target was dropped
  on 2026-10-04 to keep the 25 s bars). Measured after phase 5: 8.7 s, with both players deciding at once.
- After any blow, a player can say who hit whom, and for how much, without the log.
- In a fight, no more than one panel of information that isn't about the fight.

## Built

**Phase 1, October 4, 2026: the fight screen** (`Client/src/ui/hud/combat.ts`, `Client/src/ui/icons.ts`).
- **Fight mode:** while you fight or watch, the screen gets the `fight-mode` class.
  - Hidden: the In Sight list, the minimap, the pace and stamina panel, the party and place panels, the world-map tabs
    and weather line, the Listen/Look/Smell actions bar, "Looking at", and the place description.
  - The help line lists the fight's keys instead.
  - Notices show above the action bar, not on it.
- **Turn order** along the top of the map:
  - your state in a word or two ("YOUR TURN", "YOUR TURN IN 6", "WATCHING", "YOU ARE DOWN");
  - a round face for each fighter, drawn from the wolf portrait, in the order turns will come, those acting first;
  - each face has a ring in its side's colour (the turn's time running out, or the bar filling) and the seconds left;
  - the round, and how many are watching.
  - It replaces the bars under each wolf and the text banner on the map.
- **Cards** on the right, your side first:
  - a portrait, a health bar with its number (the injury word in its tooltip);
  - breath and mana bars for your own side;
  - marks for burning, gathering fire, a sword held, away and an agreed truce;
  - the bleed-out timer of the Downed;
  - against each foe, your odds from where you stand (the strike's icon, "81%", "~12", dimmed when you would step in
    first).
  - Hover a card to ring that wolf on the map. Click a foe to aim your actions at them (a dashed ring). Click a fallen
    friend on your turn to tend them.
- **Action bar** under the map. Icon buttons with their key, the odds or cost beneath, and a tooltip saying what each
  does and why it can't be done now:
  - Bite (1), Sword or Take sword (2), Fire (3), Tend (4), Roll (5), Pick up (6), Stow, Truce (7), Flee (8);
  - turn left and right (Q, E);
  - End turn (Space), wide, with the turn's time running down inside it, or "in 6 s" while your bar fills.
  - Actions aim at the foe you point at, else the one chosen, else the nearest; out of reach, a bite steps in first.
  - A truce offered gets its own row with Agree and Refuse.
  - Downed: Struggle up. Away: I'm back. Watching: Stop watching. Over: the result.
- **The fight, told** above the composer:
  - every line this page has seen, each with an icon for what happened;
  - coloured by who did it (you, a friend, a foe);
  - the damage drawn out as a figure.
  - The fight's one-line entry in the story is hidden while it lasts.
- **The server** sends more with each fighter (`Game::battleView`):
  - how they look (`appearance`, `lifeStage`);
  - breath and mana for your own side;
  - for each foe, `odds`: `hit` (percent), `damage` (a usual blow) and `reach`.
  - `World::strikeChance` is now the one formula the bite, the sword and the preview share.
- **Tested:**
  - the fight data in `Client/src/game/battle.test.ts` and `Tests/game_tests.cpp`;
  - a duel (keys and clicks), a truce, a watcher, a resident fight, a page reloaded mid-fight, and a 1280×720
    window, all played in the browser.

**Phase 2, October 4, 2026: the arena, built to be read** (`Client/src/game/paint.ts` `drawArena`, `drawToken`,
`chanceBadge`; `Client/src/game/battle.ts`).
- **Ground:**
  - The world outside the arena is dimmed and the ground inside it quietened, with a faint grid when tiles are 18 px
    or more.
  - The arena has a red frame; its flee band has an arrow out at the middle of each side.
  - In a fight, the wind and height words leave the map's top-left corner to the turn order, and the map's terrain
    tooltip is off.
- **Tokens:**
  - Each fighter is a round token, about three quarters of a tile, in its side's colour: you gold, friends blue, foes
    red.
  - Each has its `W`, a wedge on the rim where it faces, a health ring (green, then amber below half, red below a
    quarter) and a † when it holds a sword.
  - The Downed lie on their side with a pulsing orange rim; the dead are grey.
  - Names go beside the others' tokens where they fit; yours is the gold token, unnamed.
- **Your reach:** an outline round the tiles you can reach, your own tile inside it.
  - Among them, the tiles a blow at the aimed-at foe could come from are tinted red, deeper where the chance is
    better (their side and back).
- **Pointing at a tile you can reach:**
  - the way there (a dashed path through tiles you can reach);
  - a ghost of yourself on that tile;
  - if the foe is in reach from there, the chance of a blow from it ("95% · behind").
- **Pointing at a foe on your turn:**
  - in reach: a strike line and the chance badge;
  - out of reach: the tile a click would step to (path and ghost) and the chance from there, or "out of reach".
- **Pointing at any wolf but yourself:** a ring of eight arcs round it, faint in front, amber at the sides, red
  behind: where a blow lands most often.
- **Turning:**
  - drag from your own wolf the way it should face (the wedge follows, and letting go turns it);
  - a plain click on the edge of your tile still turns it that way, with the arrows shown when the pointer is on
    your tile;
  - Q/E and Alt+click as before.
- **The server** also sends `odds.base`, the chance head on. The page adds +10 from the side and +20 from behind,
  within 20–95: the server's own rule (`chanceFrom`, `quarter`, `octantGap`).
  - `stepToward` (the click-to-strike step, now shared by the click and the preview) and `pathTo` (a way through
    tiles you can reach) are in `battle.ts`.
- **Fixed on the way:** the wolf under the pointer was cleared each frame before the arena drew (`hoveredEntity`), so
  nothing could depend on it; the arena now works it out first.
- **Tested:**
  - `Client/src/game/battle.test.ts`: chances by quarter, the step a click takes, paths that keep to tiles you can
    reach and go round a wolf, and turning by dragging versus a plain click;
  - `Tests/game_tests.cpp`: `base`;
  - in the browser: a duel hovered, dragged, fought down to a Downed wolf, and two players left away (below).

**Seen while testing, not yet fixed:**
- When every player in a fight is away, the fight never ends (a duel sat at round 127 with nobody acting). It needs
  a rule: a truce, or the fight lapsing after a while.
- A guard may tell a wanted wolf to "pay or come with me" in the middle of its fight (phase 4's moments).

**Phase 3, October 4, 2026: hits that land** (`Client/src/game/fightFx.ts`, `Client/src/ui/sound.ts`; drawn in
`paint.ts`).
- **What happens shows on the wolf it happens to.** Each new log line, taken in by its sequence (the first sight of a
  fight replays nothing), starts its effects.
  - A figure rises off whoever it befell, starting above their name and fading in about a second:
    - the damage, cream for 18 or more (a heavy blow), grey for a graze, orange for fire;
    - "miss", "DOWN", "DEAD", or "up" for one tended or struggling up.
  - The one struck flashes white at contact.
  - A heavy blow, fire, or a fall shakes the whole view for a third of a second; more when it is you, and a lighter
    shake when you take any blow.
  - A fall bursts in an orange ring.
  - Fire fills its cone for a second: a glow under two flickering tongues of flame a tile, then ash for a few seconds.
  - Health drains instead of snapping. After a blow the token's ring and the card's bar hold a moment, then drain to
    the true figure, the lost part shown pale; health that rises rises at once.
  - A card struck shudders and reddens for a moment.
- **Reduced motion** keeps the figures, but still, with no shake, no burst and no shudder.
- **Sound, the game's first:**
  - short sounds made in the browser from noise and tones, with no files to fetch: a bite's snap and thump, a
    sword's swish and cut, a graze, a miss's whiff, fire's roar and crackle, the gathering hum, a burn's hiss, a
    fall's thud, rising notes for getting up, a chord at the end;
  - a soft two-note chime when your own turn comes.
  - The same sound twice within 60 ms plays once.
  - Settings → **Combat sound**: Off, Quiet, Normal (default), Loud. It is kept on this computer and plays a sample
    when changed.
  - Browsers allow sound only once the player has done something on the page; until then nothing plays.
- **Tested:**
  - `Client/src/game/fightFx.test.ts`: figures and their colours on the right wolf, the flash, the shake (and none in
    reduced motion), a fall's burst and DOWN, a miss, fire then ash, the cues in order, and health held then drained
    then risen;
  - in the browser: a sword duel's heavy cut and a cast of fire caught frame by frame, and the sound setting cycled
    and kept.
  - The sounds themselves were not heard (the test browser has no speakers).

**Phase 4, October 4, 2026: a beginning and an end** (`Client/src/ui/hud/combat.ts` moments; server `Core/RatwBattle.cpp`).
- **Duel terms.**
  - The menu's Challenge offers three: **Duel until one yields** (first, the default), **Duel to first blood**,
    **Fight to the death**. They are sent with the challenge (`{"type":"action","action":"challenge","terms":...}`),
    named in its notice and its prompt ("…challenges you to a fight to first blood"), and kept on the fight
    (`Battle::terms`; `Challenge::terms`).
  - Fights that aren't duels are to the death, as before.
  - **Until one yields:** a blow that would down a wolf has them yield instead, on their feet at 1 health, not
    bleeding.
  - **First blood:** the first wound a foe deals ends it for the one bloodied.
  - A wolf who has yielded (`status "yielded"`) is out of the fight, stays where they are, and walks away when it
    ends.
- **Yield (9)**, in any fight, at any time (your turn or not).
  - Against players, they answer **Spare them** or **Press on** in the action bar's own row; silence for 20 s is a no.
  - Against only NPCs, they let you be at once.
  - Battle verbs `yield`, `spare`, `press`; `World::offerYield`, `answerYield`, `yieldFighter`.
- **How it ended, said:** "First blood · …", "Bo yields · …", or "The fight is over · …'s side stands". The banner is
  now veiled like the log, so a stranger's true name never shows; it used to.
- **A duel settled on terms leaves no one hostile:** "fought you" goes, as after a truce.
- **The versus card:**
  - For two seconds as a fight begins, the portraits and names of each side face each other across "VS", sliding in;
  - under them, the terms: "A duel to first blood", "An assault: the watch will hear of it", "A fight to the death",
    or "Watching · …".
- **Your turn:** "YOUR TURN" rises in the middle of the map for a second (with phase 3's chime).
- **The result card**, over the map from the end of the fight until 16 s later or closed:
  - a word for it, coloured: You stand, Your side stands, You yield, You are down, You die, Beaten, Truce, It lapses;
  - the banner;
  - what you dealt and took (summed from the log's figures);
  - what follows: wanted (from your law status), "The watch will hear of this", or "Bleeding: …";
  - **Tend** (one button per wolf left down, once back in the world), **Write**, **Close**.
  - It replaces the banner the map drew.
- **The watch waits for the end of a fight:** no guard stops, confronts or takes a wanted wolf who is in one; the
  deadline to pay waits too.
- **A fight left by everyone lapses:** every player still standing away (and no NPC fighting on) for 60 s, and it
  ends as a truce, "The fight lapses: no one is left fighting it".
- **Tested:**
  - `Tests/battle_tests.cpp` `dueTerms`: yield instead of down, first blood, to the death, the yield offered, refused,
    lapsed and accepted, and the lapse;
  - `Tests/crime_tests.cpp`: no stop mid-fight;
  - `Client/src/game/battle.test.ts`: the terms menu, the challenge and fight carrying them;
  - in the browser: a duel to first blood chosen from the menu (versus card, both results), and a duel until one
    yields ended by Yield and Spare.
  - The older tests of downing and death now fight to the death.

**Phase 5, October 4, 2026: every second yours** (server `Core/RatwBattle.cpp` `planMove`, `planAct`, `unplan`,
`playPlan`, `meterHaste`; page `Client/src/game/paint.ts` `drawPlan`, `fightFx.ts` captions, `ui/hud/combat.ts`).
- **No dead air:** while no player is taking a turn and no fire is gathering, every bar fills ×2.5 (`battle::Haste`)
  until someone's turn comes.
  - The fighters' `rate` carries it, so the turn order strip and End turn count down truly.
  - The strip says "» ×2.5" meanwhile, with a tooltip.
  - Fire gathering keeps the normal pace, so there is still time to step out of the cone.
- **Planning ahead:** while your bar fills, the fight screen works as it does on your turn, but plans instead of doing.
  - Your next turn's reach is outlined, dashed: the tiles it could take you to from where you stand, with the stamina it
    will have then (`World::planReach`).
  - Click a tile to plan the move. Click a foe (or Bite or Sword) to plan the blow, or a fallen friend to plan tending.
    Fire, Rest, Roll, Take sword, Stow, Pick up and Flee plan from the action bar. Truce and the facing are left for
    the turn itself.
  - The plan is drawn as a ghost of your wolf at the planned tile, the way there dashed, a dashed line to a planned
    blow's foe (or the step a blow out of reach would take), planned fire's cone, and a badge over it: "NEXT · BITE".
  - The planned button is dashed gold. The strip reads "YOUR TURN IN 6 · PLANNED" (or "· PLAN IT").
  - Clicking the planned tile, foe or action again takes it back, as does Clear plan.
  - Battle verbs `plan` (`x`, `y` for the move; `act` and `target`, or `x`, `y` for fire) and `unplan` (`part`:
    `move`, `act`, or both); the snapshot's `you.plan`, `planning` and `haste`.
- **The plan plays** half a second into the turn (`battle::PlanBeat`, after the chime and "YOUR TURN"):
  - the move first; a blow planned at a foe now out of reach steps in first, to the reachable tile nearest them, as a
    click on them does;
  - then the action, once the walk is done, with whatever is in the jaws then (a bite becomes a sword stroke if a sword
    was taken up meanwhile);
  - what can no longer be done is said ("Your plan: Get next to them first.") and left for the player;
  - the turn is still theirs after it: to turn, to write, or to end it.
- **A turn used, left to run out, isn't let pass:** only a turn with no move and no action counts toward being marked
  away, and only it is logged "lets the moment pass". Before, a planned (or simply unhurried) turn that ran its time
  counted as one.
- **NPC turns shown, not just run:**
  - anyone else walking shows the way they are going, dashed in their side's colour, and a ring where it ends (the
    snapshot sends each fighter's `walk`);
  - a word under them as they set off ("steps in" to a foe, "falls back" from them, "moves");
  - then what they did ("bites", "cuts", "swings", "snaps", "gathers fire", "FIRE", "tends", "rolls", "rests",
    "flees", "yields"…), each for about a second and a half, placed where it covers no wolf or name;
  - nothing is said under oneself. NPCs keep their 1.5 s pause before acting, then walk and strike.
- **Tested:**
  - `Tests/battle_tests.cpp` `planningAhead`: the bars faster with no one deciding, not while fire gathers; a move and a
    bite planned, refused out of reach, refused in one's own turn, played as the turn comes, the turn still one's own
    after, and no timeout counted; a bite at a foe out of reach stepping in first; a plan taken back;
  - `Client/src/game/battle.test.ts`: tiles, foes and fallen friends planned while waiting, taken back when clicked
    again, the plan, `walk` and `haste` read;
  - `Client/src/game/fightFx.test.ts`: "steps in" then "bites" under an NPC, nothing under oneself, "falls back", a
    turn ended not said;
  - in the browser (scratch Greyfen):
    - a duel where both planned while waiting (a bite, and a move away) and both plans played as the turns came;
    - a minute of quick turns: 8.7 s median between one's own turns (25 s bars);
    - a fight with residents and the watch, with "steps in" and "bites" under them as they came.
