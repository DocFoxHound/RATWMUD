# Combat: turn-based arenas, attacks, injury and death

Status: designed October 2, 2026; the turn-based core built the same day (see [Built](#built) at the end). Sword,
Flamethrower, mana and the attack animations are still to come. This
document sets the combat rules; [18-combat-presentation.md](18-combat-presentation.md)
still governs the encounter log and map-cue presentation except where noted.
All numbers are starting tuning, not final balance. Shared working copy:
https://claude.ai/code/artifact/39ec1b30-75e1-460b-b93e-b94d296582ad

## Overview

Combat v1 is turn-based, in the style of Final Fantasy Tactics, and adds three attacks: Bite, Sword (held in the mouth) and Flamethrower. A fight moves its fighters into a private arena copied from their cell, while the world outside sees them lined up inside a red square. Turns come from an initiative meter that fills by dexterity. Injury lingers as lost movement, and death is permanent, so the system rewards caution and teamwork.

**Design pillars**

- **Low intensity.** Few inputs, slow enough to read. A fight is a handful of exchanges, not a flurry.
- **Stats decide outcomes.** Hit, dodge, speed and damage all derive from character stats, so builds feel different.
- **Everything costs something.** Physical attacks spend stamina; magic spends mana and hurts the caster; getting hurt slows you down.
- **Readable at a glance.** The W icon nudges on attacks and recoils on hits; magic is shown with glyphs and always has a visible tell.
- **Death is rare but real.** A generous downed timer, one self-recovery, then rescue by others or permanent death.

## Stats that drive combat

Combat reuses the stats the server already owns (`Entity` in `Core/RatwWorld.h`), all on a 0–100 scale. It adds two new pools, health and mana, and one new stat, fighting skill.

| Stat | Exists today | Default | Combat role |
| --- | --- | --- | --- |
| Strength | Yes | 50 | Physical damage, resisting having a sword knocked loose |
| Dexterity | Yes | 50 | Accuracy, dodge, initiative speed, move range |
| Wisdom | Yes | 30 | Mana pool, spell power, spell charge speed |
| Stamina | Yes | 100 | Spent by physical attacks; +8 back per turn |
| Hurt | Yes (0–100) | 0 | The injury track; health = 100 − hurt |
| Mana | **New** | 20 + wisdom × 0.8 | Spell cost; only Gifted/Quickened characters have any |
| Fighting skill | **New** | 50 (NPCs by profession) | Small bonus or penalty to hit and dodge |

**Core formulas (v1)**

```text
hit chance   = clamp(75% + (attacker DEX − defender DEX) × 0.5%
                     + (attacker skill − defender skill) × 0.3%
                     + 10% from the side / 20% from behind, 20%, 95%)
damage       = weapon base × (0.6 + STR / 125) × random(0.85–1.15)
spell power  = spell base × (0.5 + WIS / 100)
```

One roll per attack keeps it legible: hit, graze (within 10% of the threshold, half damage) or miss. Crits are left out of v1 on purpose; they make low-intensity fights swingy.

Age already adjusts effective dexterity and senses, so old characters dodge less without any extra rule.

## A fight at a glance

```text
                         Fight starts
            (attack, arrest, or accepted challenge)
                              |
                              v
                  Arena copied from the cell
           (world shows the lineup in a red square)
                              |
                              v
   Joiners  --------->      Turns      --------->  Fled
   edge Join or        fullest initiative          flee from an edge tile;
   auto-join;          meter acts next;            can only observe after
   start at 0          move + one action,
   initiative          30 s per player
                              |
                              v
                          Fight ends
           (one side all down or fled, or a truce)
                              |
                              v
                Everyone fades into the world
               (where they stood in the arena)
```

Wolves can join a fight while it runs, and anyone can leave by fleeing, but no one who fled can come back as a fighter.

## Starting a fight: the arena

Combat is turn-based, in the style of Final Fantasy Tactics. When a fight starts, the server copies a piece of the current cell into a private arena. Only the fighters are inside it. The rest of the world keeps running in real time around them.

**What starts a fight.** A committed attack on an NPC, an NPC attacking (a guard arresting, bandits on the road), or a player-vs-player request that is accepted. There is no separate "enter combat" command.

**Arena size.** Twice the default map view in width and in height, centred on the fighters, cut from the cell. A cell smaller than that, like the 32×24 tavern, is used whole. The arena never extends past the cell's edges. The arena also grows with battle size: each fighter who enters past the first two adds room (for example, about 1 tile of width and height per extra fighter), so a large battle always has open tiles for everyone to arrive on and move into. It stops growing at the cell's edges; past that, fighters arriving at a full edge are placed at the nearest open tile.

**What is copied.** Terrain, elevation, fixtures, doors (open or closed), light level and weather, as they were at the start. The copy is frozen; a storm starting outside does not reach a fight already underway.

**What carries back.** Only the fighters' own state: health, stamina, mana, Downed status, position and items. A sword knocked loose lands at the matching spot in the world when the fight ends. Nothing else done in the arena changes the real cell.

**Grid.** The arena uses whole terrain tiles for movement and range. Fighters are snapped to the nearest open tile when they arrive, keeping their positions relative to each other.

A cell can hold several fights at once. A new attack on someone already fighting is treated as a request to join that fight, not a second fight.

## What the world sees: the red square

In the real cell, the fighters stand frozen in two facing lines, one per side, with a red square drawn around them. The square is compact: the lineup plus a one-tile margin, much smaller than the arena itself.

```text
  ┌───────────┐
  │  W  W  W  │    <- one side
  │           │
  │  W  W     │    <- the other side
  └───────────┘   (drawn in red)
```

- **Walkers go around.** Other wolves path around the square. If it blocks a doorway or the only way through, they may cross its empty tiles, but never the frozen fighters.
- **Live state.** A Downed fighter shows as a sideways `W` flashing orange; a dead one as a sideways `W` in gray. A fighter who flees leaves the lineup. Hovering the square shows "Fight · 3 v 2 · round 4", filtered by what the viewer can perceive.
- **Perception still applies.** Only wolves who can see the spot see the square. Those who only hear it get a vague "sounds of a fight nearby" line, through the existing hearing rules.
- **Encounter card.** Onlookers who can see the fight get the single expandable combat entry from doc 18 in their feed.
- **Fighters are not interactable** from outside except through Join or Observe. No trading, talking-to or picking up items from them while the fight lasts.

## Joining a fight

Anyone can join a fight by walking to the edge of the red square and choosing **Join fight**, then a side. They appear at the arena edge nearest where they stood, and wait for their first turn.

**Initiative on entry.** Fighters present when the fight starts begin with a full initiative meter and act at once (highest dexterity first; ties go to whoever started the fight). Anyone who joins later starts at 0 and waits for the meter to fill, so a fast wolf gets in sooner than a slow one.

**Party auto-join.** When a fight starts, party members of any fighter who can see the spot are pulled in on that fighter's side. A party member who comes into view later is pulled in too. Recommendation: show a 5-second "Joining Bracken's fight…" countdown with a Stay out button, so a player mid-roleplay isn't yanked without warning.

**NPC auto-join.** The same rule covers NPCs: an NPC's companions and group (a bandit band, a guard patrol) join with it. Guards who see an assault in progress join against the attacker, as they already respond to assaults today. Each NPC's temperament (below) decides how hard it then fights.

**Starting player vs player needs approval; joining does not.** Attacking a player sends a challenge; the target has 30 s to accept or decline, and silence counts as a decline. Once any fight exists, any player may join it on either side with no approval, including a fight that started as a one-on-one duel. Fights are public events in the world, and wolves stepping in, for whatever reason, is part of the roleplay.

**Who cannot join as a fighter:** wolves who already fled this fight, anyone who has observed this fight, Downed or dead wolves, and young wolves (age 6–12).

## Observe mode

A player who wants to watch can enter a fight as a bodiless observer. Observers see the arena exactly as the fighters do but can never affect it. Once a player has observed a fight, they can only ever return to it as an observer, so no one can scout a fight before deciding to jump in.

**Entering.** At the edge of the red square, choose **Observe** instead of Join fight. In the world the observer's wolf stays standing at the square's edge, watching; nothing about it changes.

**What an observer can do:** move a free camera over the whole arena, see the turn order bar and every fighter's health label, read the encounter log, and talk normally. Their speech comes from where their wolf stands in the world, so fighters hear onlookers shouting from the edge.

**What an observer cannot do:** take a turn, move a body in the arena, attack, cast, tend wounds or flee. Observers have no tile, block nothing, cannot be targeted and take no damage, including from spell cones. Fighters do not see observers on the arena grid; a small "3 watching" count shows above the turn order bar.

**Leaving and coming back.** Leaving returns the player to their normal view of the world. They may come back any time while the fight lasts, but only through Observe; the Join fight option is gone for them in that fight.

**Other rules.**

- Observing a fight exempts you from party auto-join into that same fight.
- A wolf who fled may observe the rest of the fight it fled from.
- Downed fighters keep watching from their own body, not as observers.
- There is no cap on observers; they cost the server only a view of the fight.

## Turns and initiative

Each fighter has an initiative meter from 0 to 100 that fills by dexterity. Whoever reaches 100 takes a turn while everyone else waits; then the meters keep filling. Between turns no real time passes, so the fight moves from one turn straight to the next.

```text
meter gain per tick  = 6 + DEX / 10                                        // DEX 50 → 11, DEX 100 → 16
move range (tiles)   = floor((3 + DEX / 25) × injury factor), minimum 1    // DEX 50, unhurt → 5
meter after a turn   = 0, +20 if you did not move, +20 if you did not act, − the action's weight
stamina per turn     = +8 at the start of each of your own turns
```

**On your turn** you may move up to your range and take one action, in either order. Actions are Bite, Sword, a spell, Tend wounds, Flee or Wait. Skipping the move or the action makes your next turn come sooner, as in Final Fantasy Tactics. Heavy actions carry a weight that delays the next turn (Sword 10).

**Turn timer.** A player has 30 s per turn. Typing a roleplay line extends it once by 15 s; the line is attached to that turn's action in the encounter log. Running out of time counts as Wait. After three timeouts in a row the player is marked away and their turns are skipped instantly until they act again, so one absent player cannot stall everyone.

**NPC turns** resolve on their own, with a 1.5 s pause so players can follow what happened.

**Turn order bar.** Every fighter sees the next six turns in order, like the Tactics turn list, so a slow sword swing visibly pushes the swinger down the list.

**Facing.** You choose your facing at the end of your turn. Attacks from the side get +10% to hit and from behind +20%, which gives positioning a real payoff.

**Range on the grid.** Bite reaches adjacent tiles (diagonals included), Sword reaches 2 tiles, and Flamethrower is a cone you aim in one of 8 directions. Attacks need line of sight; fixtures block it.

## NPC fighting temperament

How well an NPC fights, and how willingly, comes from three things: its profession, its age and a fighting skill. A farmer flinches and runs early; a guard presses in and keeps going.

**Fighting skill (0–100)** is a new stat. It adds (skill − 50) × 0.3% to the NPC's hit chance and to its dodge. Players get the same stat at 50, open to training later.

| Profession | Base skill | Temperament | Flees below health |
| --- | --- | --- | --- |
| Guard | 70 | Aggressive | 10 |
| Bandit (road folk) | 55 | Aggressive | 30 |
| Forager | 40 | Cautious | 50 |
| Farmer / resident | 30 | Timid | 60 |
| Merchant | 30 | Timid | 60 |
| Cook | 25 | Timid | 70 |

**Age** adjusts both: adolescents −10 skill, old wolves (65+) −15 skill and flee 10 health sooner. Young wolves (6–12) never fight; they run.

**What each temperament does on its turn:**

- **Aggressive:** closes on the nearest enemy, prefers wounded targets, flanks to attack from the side or back.
- **Cautious:** attacks only targets already adjacent or in reach, keeps near allies, tends Downed allies when it can.
- **Timid:** keeps distance, attacks only when cornered, and tries to flee as soon as it is hurt at all if no ally is close.

All NPCs flee once below their threshold, and ignore Downed enemies. These numbers are a starting point for a few roles; adding a profession means adding one row.

## Fleeing and ending a fight

Any player or NPC can flee, but a wolf who flees is out of that fight for good. This stops anyone from slipping out, healing and coming back fresh.

**How to flee.** Flee is an action taken while standing on an arena edge tile. With no enemy adjacent it always works. With an enemy adjacent it succeeds on 50% + (your DEX − their best DEX)%, between 20% and 90%; a failed attempt uses the turn. Downed wolves cannot flee; they can only crawl.

**After fleeing** the wolf fades into the world at the spot matching the arena edge tile it fled from (see below), at its current health and stamina. It cannot rejoin that fight or be pulled back by party auto-join; it may only observe. It also cannot start a new fight with anyone still in that fight until the fight ends, which closes the loophole of fleeing and immediately attacking again.

**A fight ends** when:

- one side has no one left standing (all Downed, fled or dead), or
- every remaining fighter agrees to a truce.

### When the fight is over

Fighters move around the arena, so they rarely finish where their frozen wolves stand in the world. When a fight ends, everyone fades back into the real world at the spot where they were standing in the arena. No one slides or walks back.

1. **Last action resolves.** The arena shows a 2 s banner: "The fight is over · Bracken's side stands" (or "Truce").
2. **Arena fades out** for fighters and observers over 0.5 s.
3. **The world swaps at once.** The red square and the frozen lineup fade out while every fighter fades in at their arena spot, both over 0.75 s. Bystanders see the lineup dissolve and the fighters appear where the fight left them.
4. **Encounter card closes** with an ended state and a narrative line such as "The fight breaks apart."

**Mapping arena to world.** The arena is a cut of the real cell, so each arena tile is the same tile in the world; no conversion is needed. If that spot is now blocked (another wolf stands there, a door has closed), the wolf appears at the nearest open tile, never across a wall or through a closed door. If nothing is open within 3 tiles, it appears at its old lineup spot instead.

**What returns with them.**

- **Standing fighters** keep their arena facing, health, stamina and mana, and stand in place. Any movement a player was holding before the fight is cleared and must be pressed again (the existing `transitioned` release rule).
- **Downed wolves** fade in as a sideways `W` flashing orange at the tile where they fell, timer still running.
- **The dead** fade in as a gray sideways `W` at the tile where they died.
- **Dropped items** such as a knocked-loose sword appear on the tile where they fell.

**A 5 s settle.** For 5 seconds after fading in, no one from that fight can start a new one. This prevents a loser from being attacked again before they can see where they are.

Fled wolves follow the same rule when they leave: they fade out of the lineup and fade in at the world spot of the edge tile they fled from, so a successful escape really puts distance between them and the fight.

## Physical attacks

Bite is the always-available baseline; Sword trades stamina and speed for reach and damage. Both spend stamina from the same bar as sprinting, so chasing and fighting compete for one resource.

| | Bite | Sword (held in mouth) |
| --- | --- | --- |
| Requires | Nothing | A sword in the mouth slot |
| Range | Adjacent tile | 2 tiles |
| Weight (delays next turn) | 0 | 10 |
| Base damage | 12 | 20 |
| Stamina cost | 8 | 14 |
| Side effect | None in v1 | Can be knocked loose (below) |

At 100 health and 75% hit chance, a DEX 50 / STR 50 wolf downs an equal opponent in about 12 bite turns or 7 sword turns. With players taking around 15 s a turn, a one-on-one fight lasts about 3–4 minutes. Bite stamina breaks even with the +8 per turn; Sword runs a 6-per-turn deficit that only matters in long fights.

**Stamina limits.** Below the attack's cost, the attack is refused with a short "too winded" message. Exhaustion (stamina 0 until it recovers to 20) blocks physical attacks too, reusing the existing hysteresis.

**The mouth slot.** A wolf holding a sword cannot bite or pick up another item until it drops or stows the sword. Speech while holding it stays allowed, but its IC text is marked as muffled. A hit dealing 18+ damage has a 20% − (STR / 10)% chance to knock the sword to the ground at the wielder's feet.

**Unarmed defence.** There is no block button in v1. Dodge is passive and comes from dexterity (see the hit formula), so a defender's only active choices are to strike back, back off or flee.

## Magic: Flamethrower

Flamethrower is a Fire Gift, so only Gifted or Quickened Fire Wolves can use it. It is powerful, slow to start and costly in three ways: mana, stamina and the caster's own health. This keeps the setting bible's rule that magic is rare and limited, not casual spellcasting.

**Scale by tier.** A Gifted caster produces a short sputtering gout. A Quickened caster produces a real blaze; this is the "enormous and frightening" moment the lore calls for.

| | Gifted | Quickened |
| --- | --- | --- |
| Charge speed (per tick) | 12 + WIS / 10 | 22 + WIS / 10 |
| Area | 3-tile cone | 5-tile cone, wider |
| Base damage | 21 | 45 |
| Mana cost | 25 | 40 |
| Stamina cost (held breath) | 12 | 20 |
| Self-damage to caster | 3 | 5 |
| Weight (delays next turn) | 20 | 10 |

**The tell.** Casting starts a separate charge meter from 0 to 100, and the spell goes off when it fills. The cone's tiles are locked when the cast starts and shown to everyone in red. A Gifted caster with WIS 30 fills it in 7 ticks, so most other fighters get a turn first and can step out of the cone or hit the caster. The caster cannot move while charging, and taking any damage cancels the spell and still spends half the mana. Observers also get a narrative line ("Ember draws a deep breath; heat shimmers at her jaw").

**Costs beyond mana.** The breath comes from the lungs, so it drains stamina like a sprint. Every cast singes the caster's muzzle for a small fixed amount of hurt, which also counts toward the movement penalty. Casting at 0 mana is allowed but burns double health, a desperate last resort.

**Special effects.**

- **Burning:** a hit target takes 3 damage at the start of each of its next 3 turns. Spending the action to roll puts it out.
- **Fear:** NPC targets below 50 health that are hit attempt to flee instead of fighting back.
- **Light:** the cone lights the area for the burst, using the existing lighting system; it can reveal hidden wolves.
- **Smoke:** masks scent in the cone for 3 rounds, using the existing scent system.
- **Weather:** heavy rain cuts damage by 40%; standing in shallow water makes a target immune to Burning.
- **Friendly fire:** the cone hits everyone in it, allies included. Spells always have a downside for the group too.

Flammable fixtures (hay, wooden tables) catching fire is a natural next step but is out of scope for v1.

## Visual feedback

Recommendation: glyphs, not graphics. They match the ASCII-derived map and the vocabulary already set in [18-combat-presentation.md](18-combat-presentation.md). They also leave the detail to prose, which is the project's core idea.

**Arena view.** Inside a fight the map shows the arena grid. On your turn your reachable tiles are tinted, and attack range and spell cones are outlined before you confirm. The turn order bar sits above the map. A charging spell's locked cone stays outlined in red until it fires.

**W motion (nudge and recoil).** The `W` slides a fraction of a tile and springs back. This is a display offset only: the server position never changes, so it cannot affect collision or hits. It amends the rule in doc 18 that a wolf is never moved for an effect.

| Event | W motion | Glyph cue |
| --- | --- | --- |
| Bite | Lunge 0.3 tiles toward target, back in 0.25 s | Short stroke, then `*` on contact |
| Sword | Lunge 0.4 tiles with a slight sideways arc | Arc `)` or `(` on the swing side |
| Hit taken | Recoil 0.25 tiles away from attacker, brief shake | `*` on the victim |
| Graze | Recoil 0.1 tiles | Small `'` |
| Miss / dodge | Defender sidesteps 0.2 tiles | Faded open arc |
| Spell charge | Small tremble, no travel | `.` `:` `*` building at the muzzle |
| Downed | `W` turns sideways (rotated 90°) | Flashes orange while the timer runs |
| Dead | `W` turns sideways (rotated 90°) | Steady gray, no flash |

**Flamethrower on the map.** The cone fills over the burst with flickering `^` `~` `*` in amber and red, thinning at the edges. A burning target carries a small `~` above it. Smoke leaves faint `░` for its 3 rounds.

```text
     ~ ^ *
 W>  ^ * ~ ^      <- 3-tile Gifted cone, flickers for 1 s
     * ~ ^
```

**Accessibility.** Outcomes differ by shape as well as colour. Reduced-motion mode drops the slide and fades, and shows static marks. Effects never cover speech or typing indicators.

## Injury, stamina and speed

Injury cuts movement in both places a wolf moves. In a fight it shrinks the tiles you can move per turn. In the world it lowers top speed, so a wolf who lost a fight limps away from it. It never freezes anyone in place.

```text
injury factor  = 1 − 0.6 × (hurt / 100)
arena move     = floor((3 + DEX / 25) × injury factor), minimum 1 tile
world sprint   = walk + (5.2 + 0.052 × DEX − walk) × injury factor
world walk     = 2.6, or 2.0 once hurt ≥ 75 (limping)
```

| Health | Hurt | Label | Arena move at DEX 50 (tiles) | World sprint at DEX 50 (tiles/s) | Notes |
| --- | --- | --- | --- | --- | --- |
| 100–76 | 0–24 | Scratched | 5 → 4 | 7.8 → 7.1 | No other effect |
| 75–51 | 25–49 | Wounded | 4 → 3 | 7.0 → 6.3 | Stamina +6 per turn |
| 50–26 | 50–74 | Badly hurt | 3 → 2 | 6.2 → 5.5 | Stamina +4 per turn |
| 25–1 | 75–99 | Limping | 2 | 5.2 → 4.4 | World walk 2.0; no sprint notches 9–10 |
| 0 | 100 | Downed | Crawl 1 | — | See Downed and death |

The pace strip and stamina bar already show top speed (see [13-pace-and-world-travel.md](13-pace-and-world-travel.md)), so the world penalty is visible without new UI. The label appears to observers too, filtered by sight like other public state.

**Healing.** Out of combat, hurt heals at the existing rate (50 per in-game hour, `HealPerHour` in `Core/RatwCrime.cpp`). No healing happens while the wolf is in an active fight. Herbs, rest and healer NPCs are future work.

**Stamina in combat.** In the arena, stamina comes back +8 at the start of each of your turns, reduced by injury as above. Outside a fight the usual +5/s applies. A hurt wolf therefore runs short sooner in both.

## Downed and death

A wolf at 0 health is Downed, not dead. Each wolf has one self-recovery; after using it, a second downing can only be undone by another player. If the timer runs out, the character dies permanently. Timers are long because death cannot be undone.

```text
                       health hits 0
 Fighting           ------------------>  Downed                 timer runs out
 (self-recovery     <------------------  (can struggle up   -------------------+
  ready)                ally tends        once)                                |
     ^                                      |                                  v
     | new in-game day                      | struggle up (takes a turn;       Dead
     |                                      | uses the one self-recovery)   (permanent)
     |                                      v                                  ^
 Fighting           ------------------>  Downed                                |
 (self-recovery     <------------------  (needs an ally     -------------------+
  spent)                ally tends        to revive)        timer runs out
```

The bottom row is the danger zone: once the self-recovery is spent, a second downing needs an ally before the timer ends. Both Downed states run the same 7–20 minute timer, counted in turns while a fight lasts.

**Downed timer.** Set in minutes by what put the wolf down. Overkill (damage past 0) shortens it by 10 s per point, to no less than 60% of the base. Inside a fight the timer counts in the Downed wolf's own turns, one minute per turn, so a slow round can never kill anyone by itself. Whatever is left runs in real minutes after the fight ends.

| Cause of the downing blow | Base timer | Minimum |
| --- | --- | --- |
| Blunt or beating (current guard strike) | 20 min | 12 min |
| Bite or Sword | 15 min | 9 min |
| Fire (including Burning) | 12 min | 7 min |

**While Downed** the wolf shows as a sideways `W` flashing orange, can crawl 1 tile per turn (0.5 tiles/s in the world), can speak (quietly) and sees its timer. It cannot attack, cast or pick things up. Allies who can see it also see the timer. NPC enemies stop attacking a Downed wolf and move to another target.

**Self-recovery (once).** The Downed wolf spends its turn to "struggle up" and stands at the start of its next turn, unless it takes damage in between. Outside a fight it takes 20 s instead. It stands at 15 health (Limping) and its recovery is spent.

**Revive by another.** In a fight, any ally standing next to a Downed wolf can spend their action to "tend wounds"; the wolf stands at once at 20 health. Outside a fight it takes 10 s and is interrupted by damage to either of them. It works whether or not the self-recovery is spent and costs the helper 10 stamina.

**When the self-recovery comes back.** Once per in-game day, at the shared calendar's day rollover. Being revived by someone else does not restore it, and a wolf who is Downed at rollover still has to get up first.

**Death.** At 0 on the timer the wolf dies and its body stays where it fell as a gray sideways `W`, using the existing `dead` flag. The player keeps the account and makes a new character. Today the Dungeon Master can revive the dead (`World::setDead`); for permanent death that must become an admin-only correction for bugs, not a gameplay path.

**Disconnecting.** Recommendation: if the owner disconnects while Downed, the timer freezes and the body stays in the world. Allies can still revive it. A dropped connection should never be what kills a character.

## Gaps and open questions

The biggest gap is that combat partly exists already: `World::assault` in `Core/RatwCrime.cpp` hits for 18–35 hurt, opens assault incidents, alerts guards and refuses player-vs-player attacks. A separate bandit fight (`World::attack`) in `Core/RatwRoads.cpp` uses its own hit points. v1 should replace both with one resolver rather than add a third.

| Question | Recommended default |
| --- | --- |
| How many tiles is "twice the screen"? | Measure the default map view and fix the arena size in tiles, so it doesn't change with window size or zoom |
| Is there a fighter cap? | No. Battles can be any size, so very large ones need decisions: the arena grows with the number of fighters (see Arena size), up to the whole cell, and long rounds may need a shorter turn timer or NPC turns resolved together |
| Can a party member opt out of auto-join? | Yes, the 5 s Stay out button, plus a per-character setting to never auto-join |
| How is a truce offered? | A Truce action proposes it; it passes only if every remaining fighter accepts on their next turn |
| Can someone deliberately kill a Downed wolf? | Not in v1. Death comes only from the timer, so it is never one player's click |
| Does combat still count as a crime? | Yes: keep assault incidents, witnesses, guards and bond penalties from the current assault |
| A red square blocking a doorway or the only path | Let walkers pass through the square's empty tiles; it blocks only the frozen fighters themselves |
| Frozen NPCs miss their schedule (work, meals) | Pause their schedule while fighting and resume it after; no catch-up |
| Who gets the Fire Gift, and how? | Needs a lore decision; Gifted/Quickened eligibility is still undecided in doc 08 |
| What happens to a dead wolf's inventory and body? | Items stay on the body and can be taken; the body decays after a set time |
| Armour, blocking, healing items | Out of scope for v1; dodge is the only defence and rest the only healing |
| Do fights make noise? | Yes, through the existing hearing system, so nearby wolves can notice a fight they cannot see |
| Targeted wounds (eyes, ears, nose) | Later; `eyeHealth`, `earHealth` and `noseHealth` fields already exist for it |
| Narrative-only fights between roleplayers | Keep allowed: prose with no mechanics stays the default unless an attack is committed |
| How do we test balance? | A headless fight simulator in `Tests/` that runs thousands of duels and reports turns-to-down |

## Build phases

Build the resolver first and the spectacle last; each phase is playable on its own.

1. **Arena and turns.** Encounter lifecycle, arena copied from the cell, grid snapping, initiative meter, move plus act, turn timer and away-skipping, fade-out and fade-in back to the world at arena positions. Bite only, against one NPC. Replaces `assault` and the bandit `attack`.
2. **World view and joining.** Red square and lineup, edge Join, late joiners starting at 0 initiative, party and NPC auto-join, PvP challenge and approval, Observe mode and its observe-only lock.
3. **Fleeing, Downed and death.** Edge Flee and the no-rejoin rule, Downed timers counted in turns, struggle up, tend wounds, permanent death.
4. **NPC temperament.** Fighting skill, profession table, age modifiers, aggressive / cautious / timid turn behaviour.
5. **Feedback.** Reachable-tile tint, range previews, turn order bar, W nudge and recoil, contact glyphs, the encounter card from doc 18.
6. **Sword, then Flamethrower.** Mouth slot and knock-loose; then mana, Fire Gift eligibility, charge meter, locked cone, Burning, self-damage.
7. **Balance and playtest.** Fight simulator runs, then a real multi-client fight to check turn pacing.

## Built

Built October 2, 2026: the turn-based core (phases 1–4 above, and the parts of 5 that a fight can't be played
without). Sword and Flamethrower (phase 6) and the balance work (phase 7) are next.

- **The rules** (`Core/RatwBattle.h`, `Core/RatwBattle.cpp`, World members):
  - Starting a fight: an attack on an NPC, bandits who lose patience, or an accepted challenge between players.
  - The arena is cut from the cell: 64×48 tiles (twice a 32×24 view) plus a tile each way per fighter past two, never
    past the cell's edges.
  - Fighters stand frozen in two facing lines in the world, where there is room.
  - The initiative meter (6 + DEX/10 a tick); starters begin full, joiners empty.
  - A turn is a move and one action (Bite, Tend, Flee, Struggle, Wait), with the next turn's head start when either is
    skipped.
  - Turn timers: 30 s, plus 15 once while typing; three run out in a row and the player is away and skipped until they
    act again.
  - Facing gives +10% to hit from the side and +20% from behind.
  - Bite: 12 base damage, 8 stamina; stamina comes back +8 a turn, less when hurt; move range shrinks with injury.
  - Downed: the timer by cause, minus overkill, counted a minute a turn in a fight and in real seconds outside one.
    Struggling up works once a game day; anyone can tend; when the timer runs out, death is permanent.
  - Fleeing from the arena's outer two rows, against adjacent foes' dexterity; no rejoining as a fighter, no new fight
    with them until it ends.
  - Joining from the square's edge, on either side, with no approval.
  - Observe mode, and the observe-only lock.
  - The 2 s banner, then everyone back in the world at their arena tile (or the nearest open spot), with a 5 s settle.
  - NPC temperament: fighting skill, aggressive, cautious or timid by trade and age, and flee thresholds.
- **What it replaced:**
  - The real-time `World::assault` and the bandits' `World::attack` and blows are gone.
  - An assault on a resident is still a crime (incident, witnesses, fear, the Watch). On-duty guards who see it join
    against the attacker.
  - A bandit camp's fight is the whole band against the traveller. When the leader goes down, the rest run and the camp
    is broken; a traveller left Downed is robbed.
- **Through the game** (`Core/RatwGameBattle.cpp`):
  - `{"type":"battle","verb":...}` commands, and the `challenge`, `accept`, `decline`, `struggle` and `tend` actions.
  - The snapshot's `battle` (the arena's ground, fighters, turn, order, reachable tiles, log), `fights` (red squares
    in sight) and `challenge`, and `self.health` / `downedLeft`.
  - A fighter or anyone Downed is in movement mode 2 (no walking).
  - `downedLeft` and `recoveryUsed` are saved.
- **The page** (`Client/src/game/battle.ts`, `paint.ts`, `ui/hud/fight.ts`):
  - The arena drawn over the cell with only it in sight, reachable tiles lit, fighters on their tiles, and the
    turn-holder ringed.
  - Downed is a sideways `W` flashing orange; dead is a sideways `W` in gray (in the world too).
  - Click a lit tile to move, a foe to bite, a fallen friend to tend.
  - The fight panel: turn and timer, the next six turns, Wait/End turn, Flee, Struggle up, who is how hurt, and the
    fight's log.
  - Challenge prompts, being Downed out of a fight, and red squares with Join/Watch.
  - The fade out at the end, and back in.
- **Tests:**
  - `Tests/battle_tests.cpp`, a fight through commands and snapshots in `Tests/game_tests.cpp`, and the rewritten crime
    and bandit fights in `Tests/crime_tests.cpp` and `Tests/roads_tests.cpp`.
  - `Client/src/game/battle.test.ts`.
  - A real fight in headless Chromium, three players each in their own browser: `node tools/client/fight.mjs`.

Not yet:
- Sword and the mouth slot, Flamethrower, mana and the Fire Gift.
- The W nudge and recoil and the glyph effects (the log says what happened).
- Doc 18's single expandable encounter entry (the fight panel's log stands in).
- Truces.
- Player parties auto-joining (there are no player parties yet; NPC companions and bandit bands do come in).
- A disconnected fighter's body staying 60 s: a player who leaves the world is out of the fight.
- Crawling out of a fight.
- Training fighting skill.
- Combat noise through hearing.
