# 43. Gifts: the eight families, Gifted and Quickened

Drafted 2026-10-04 with the user. **Built** (end of this doc): every family's fight abilities, Gifted and Quickened,
with their glyph effects on the arena; the work abilities; choosing a Gift at character creation; the DM's Give Gift
for all eight. Every number is a *placeholder* for play-testing; mana pools stay as they are until the stats pass.

The user's brief: a player picks a **Gifted** wolf to excel at industry or to play support, and a **Quickened** wolf to
excel at magical combat. Gifted abilities are utility: they help with work, and in a fight they change the odds without
dealing damage. Quickened abilities are explicitly for combat: mostly offence, with at most one support ability or
passive each.

## What exists (read from the docs 2026-10-04)

- **The setting bible** (`Docs/References/RATW_World_Setting_Bible.md`, sections 9–20):
  - Every supernatural wolf has a Power Classification (Normal, Gifted, Quickened) and exactly one Gift Family.
  - Quickening deepens the one family, never adds another.
  - Every Gift has a **Limit**, a **Cost** and a **Tell**, and a foe who knows the Tell can interrupt it.
  - The bible's registry has Fire, Blinker, Water, Wind, Earth, Healer/Blood and Death Walker.
- **Combat** (doc 33):
  - Mana is 20 + WIS × 0.8 for both tiers. It comes back 2 a turn in a fight and 10 a minute outside one.
  - Spell power is base × (0.5 + WIS/100).
  - The one spell is **Flamethrower**, for Gifted and Quickened Fire wolves alike: a countdown everyone sees, locked
    tiles, a hit on the caster breaks it off, and Burning.
  - The DM gives the Gift with `character.gift` `{"gift": "fire" | "", "quickened": bool}`.
- **No criticals.** Doc 33 left them out. A blow from the side gets +10% to hit and one from behind +20%, and an ambush
  (doc 40) counts as from behind. The leather cosh stuns a turn from behind or unseen (doc 35).
- Injuries (doc 38): combat injuries (Bleeding, Burning…), acute injuries that heal over days, and Downed, which ends
  when its period runs out or someone tends the wolf.
- Sneaking, noise and scent (doc 40), noses and scent masking (doc 41), gear durability and repair (doc 35), and
  weather and elevation (doc 22).

## The rules for every Gift

1. **One family per wolf.** The playable families are **Fire, Earth, Water, Wind, Sound, Blinker, Gravity and Seer**.
   Sound, Gravity and Seer are new to the bible's registry (entries at the end of this doc).
2. **Healers and Blood Wolves are not a playable family.** Healer is a profession. Hemlock is the only Blood Wolf in
   the world, ever. **Death Walkers** are rare and NPC-only.
3. **Gifted = utility, Quickened = combat.**
   - Gifted abilities never deal damage. Buffing an ally's damage is acceptable later, but not in this first set.
   - **Quickened wolves do not keep their family's Gifted abilities.** The Gifted stay the best at industry, so that
     Quickened wolves aren't the best at everything.
4. **Limit, Cost, Tell for everything.**
   - The Tell is visible or audible to anyone who can notice it, using the noticing model from doc 40.
   - A hit on a wolf while its Tell is under way breaks the Gift off and spends half its mana, as with the Flamethrower
     today.
   - Quickened Tells are fainter: shorter, partly gone, or (for a Blinker) absent.
5. **Overreach.** Each Quickened family's Cost becomes much worse when the wolf casts at 0 mana or uses the same
   ability two turns running.
6. **Warden attention.** Quickened magic seen by a witness raises Warden attention on the caster. It is recorded now
   and used by a later Warden/Concord system. Gifted magic doesn't raise it.
7. **Flamethrower becomes Quickened-only.**

### Kinds of ability

| Kind | How it works |
| --- | --- |
| **Instant** | Used as the turn's action. |
| **Gathered** | Like the Flamethrower: a countdown everyone sees, with the tiles locked and shown when the cast starts. It goes off when the countdown runs out, and a hit on the caster breaks it off. |
| **Channelled** | Uses the turn's action and holds until the caster stops, moves, runs out of mana or is hit. It costs mana each turn it is held. A wolf holds one channel at a time. |
| **Reaction** | Switched on and off by the player. It fires by itself on someone else's turn, then has a cooldown. |
| **Fight-long** | Switched on when the fight starts (or at its first turn) and lasts the whole fight. |
| **Passive** | Always on. |
| **Shape** | A Quickened gathered spell whose tiles the player paints: a connected run of tiles in any direction, bends allowed, every tile within 10 tiles of the caster. The bigger it is, the more it costs. |

### "No critical": a blow from any side counts as from the front

The combat system has no criticals, and none are added. When an ability says **no critical**, it means:

- Every blow against the wolf rolls as if from the front: no +10% from the side, no +20% from behind.
- An ambush on it gets no from-behind bonus, and the cosh's stun from behind or unseen doesn't apply.
- Hit zones still follow the side the blow really came from (doc 40), since that is where it lands.

Four Quickened passives give this (Heat Sense, Stone Armor, Water Screen, Critical Sight), and each adds something
different besides.

## The families

Mana costs below are against today's pool: about 44 at WIS 30. Range is in tiles.

### Fire

**Domain:** heat and fire.

**Gifted.** *Tell:* holds its breath. *Cost:* stamina and a little health. *Limit:* a palm-sized flame, by touch.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Forge Heat | work | 6 | Keeps a forge, kiln or oven at working heat without fuel. Quality bonus to smithing, firing and cooking. |
| Kindle | work | 2 | Lights a fire on damp wood or in the rain. |
| Cauterize | instant, touch | 10 | Stops an ally's Bleeding. It hurts: the ally's bar is set back a little. |
| Flare | instant, adjacent | 6 | A flash in a foe's face knocks their bar back about 20%. |
| Smother to Smoke | instant | 8 | Turns a fire or brush into smoke that blocks sight across 3 tiles for 2 turns. |
| Warm Through | instant, touch | 6 | Removes an ally's cold and soaked penalties for the rest of the fight. |
| Heat Sense | channelled | 4/turn | Senses body heat within 6 tiles and shows warm wolves, hidden or not. |

**Quickened.** *Tell:* a short held breath, so it gathers faster than the Gifted. *Cost:* stamina and health.
*Overreach:* overheating, which doubles the self-damage.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Flamethrower | gathered | 40 | Doc 33's Quickened numbers: a 5-tile cone, 45 base, Burning. |
| Heat Lance | gathered, 4-tile line | 20 | 30 base to the first wolf in the line. Metal armour on the zone it hits makes it worse: the wolf starts Burning as well. |
| Blastwave | instant, around the caster | 15 | 20 base to everyone adjacent, allies included, and shoves them back 1 tile. |
| Wall of Fire | shape, up to 10 tiles | 10 + 3/tile | The tiles burn for 3 turns. Crossing or standing in them sets a wolf Burning. Stamina: 8 + 1/tile. |
| Heat Sense | passive | — | Sees hidden wolves within 12 tiles, through smoke and sneaking. No critical. |

### Earth

**Domain:** soil, clay and stone.

**Gifted.** *Tell:* bracing bare paws on the ground. Paw wraps or boots defeat it, and it can't be used on a wooden
floor. *Cost:* stamina and aching paws. *Limit:* one tile, shallow.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Clay Hand | work | 6 | Quality bonus to pottery and masonry. |
| Stone Sense | work | 6 | Finds ore seams, flawed stone and weak walls. |
| Loosen Ground | instant, range 5 | 8 | One tile turns soft: crossing it costs double stamina, with a chance to stumble and lose the rest of the move. |
| Firm Footing | instant, touch | 6 | Packs the ally's tile: they can't be shoved and get a bonus on guard while they stay on it. |
| Feel Footfalls | channelled | 5/turn | Shows every wolf moving within 6 tiles, sneakers included. A wolf standing still isn't felt. |

**Quickened.** *Tell:* bracing paws, now through wraps, but still not on wood. *Cost:* heavy stamina and cracked pads
(a movement penalty). *Overreach:* an acute paw injury.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Upheaval | gathered, range 6 | 18 | The tile erupts: 25 base, and the wolf on it is knocked down and loses their next move. |
| Hurl Stone | gathered, range 10 | 25 | Tears up a rock and throws it at one wolf: 40 base. |
| Fissure | shape, up to 10 tiles | 12 + 3/tile | 15 base to wolves on it, who fall prone. The tiles can't be crossed for 2 turns. Stamina: 10 + 1.5/tile. |
| Stone Wall | shape, up to 5 tiles | 10 + 5/tile | Raises a wall that blocks movement and sight until broken (about 30 damage a tile). Stamina: 10 + 2/tile. |
| Stone Armor | fight-long | 15 | A large armour bonus on every zone. No critical. 1 tile less movement and the bar fills 15% slower. |

### Water

**Domain:** water and its motion.

**Gifted.** *Tell:* needs free water within 2 tiles or an open waterskin. *Cost:* thirst, so stamina comes back
slower. *Limit:* about a bucket at a time.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Draw Water | work | 4 | Moves water: irrigation, filling, and washing hides for tanning and dyeing (quality bonus). |
| Dowse | work | 6 | Finds water under the ground. |
| Douse | instant, range 3 | 5 | Puts out an ally's Burning, or a burning tile. |
| Slick | instant, range 5 | 8 | Wets a tile. On paving or stone it becomes slippery, with a chance to slip and lose the rest of the move. |
| Splash Eyes | instant, adjacent | 7 | The foe's next attack gets −20% to hit. |
| Wash Out | instant, touch | 6 | Washes an ally's scent off (masking, doc 41) and clears dust or smoke from their eyes. |
| Mend | work, touch | 10 | Tends an ally's acute injury so it heals 1.5× faster. Once a day per injury. |

**Quickened.** *Tell:* water within reach, now within 4 tiles, including rain and wet ground. *Cost:* dehydration,
so stamina stops coming back. *Overreach:* health loss.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Pressure Jet | instant, 5-tile line | 18 | 28 base to the first wolf in the line, shoved back 1 tile. |
| Wave | gathered | 30 | A wave 3 tiles wide rolls 4 tiles forward: 15 base, everyone is shoved 2 tiles and knocked down, and it leaves shallow water behind. |
| Freeze | instant, range 6 | 12 | Only on a wolf standing in shallow water: they lose the move part of their next turn but can still act. |
| Flood | gathered, 3×3 within 6 | 20 | The area turns into shallow water for 5 turns: everyone in it except the caster moves slowly, and fire there is weakened. |
| Water Screen | fight-long | 12 | No critical, and fire damage to the wolf (Burning included) is cut by 75%. |

### Wind

**Domain:** air moving and pressing. Wind is not sound, weather or flight (the bible).

**Gifted.** *Tell:* a long breath in, then a breath out. *Cost:* lightheadedness (stamina). *Limit:* a breeze; only
light things move.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Winnow and Dry | work | 4 | Threshing and milling, and drying herbs, hides and laundry. Speeds the work. |
| Bellows | work | 4 | Draws a forge hotter (smithing quality bonus). |
| Turn the Wind | channelled | 4/turn | Turns the local wind: the party's scent stays away from enemy noses, or the enemy's scent reaches the party's. |
| Clear the Air | instant | 6 | Blows smoke or dust off a 3-tile area. |
| Back Breeze | instant, touch | 6 | An ally's next move costs less stamina. |
| Air Blast | instant, range 3 | 8 | A blast of grit into a foe's eyes cuts their sight range for the rest of the fight. |

**Quickened.** *Tell:* a quick breath in and out. *Cost:* lightheadedness (stamina). *Overreach:* dizziness, or
fainting and losing a turn.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Battering Gust | instant, 5-tile line | 18 | Throws the first wolf up to 3 tiles back and down: 15 base, +10 if they hit a wall or another wolf. |
| Pressure Drop | gathered, 2×2 within 6 | 20 | 12 base and ear damage (`earHealth`), and each wolf in it loses half its next bar. |
| Steal Breath | channelled, range 4 | 10 + 5/turn | The foe loses 15 stamina a turn and can't hold a breath or hum: it breaks Fire and Sound Tells. |
| Whirlwind | instant, 2 turns | 22 | Adjacent foes are flung 2 tiles out, and the dust cuts their sight range for the rest of the fight. |
| Tailwind | passive | — | The wolf moves twice as far as any other wolf. It does nothing for allies. |

### Sound (new family)

**Domain:** voice, vibration and noise. See the registry entry.

**Gifted.** *Tell:* an open-throated hum that can be heard nearby. *Cost:* a hoarse voice (no speaking or howling
for a while) and ringing ears. *Limit:* a voice's worth of sound.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Ring True | work | 4 | Taps metal or stone to find flaws: a quality bonus to crafting and appraising goods. A smith or other crafter who uses it while mending makes the repair cheaper and quicker. |
| Echo | work | 6 | Maps a cave or mine. |
| Carry | work | 4 | A herder's or crier's voice is heard far away. |
| Hush | channelled | 5/turn | Allies within 2 tiles make no noise (doc 40 hearing). |
| Throw Voice | instant, range 8 | 6 | A noise from a chosen tile draws an NPC's attention and turns their facing. |
| Steady Beat | channelled | 6/turn | While the caster stays still, allies within 3 tiles fill their bars 10% faster. |
| Whisper Thread | instant | 3 | Speaks to one ally without anyone else hearing. |

**Quickened.** *Tell:* a barely audible hum. *Cost:* voice loss. *Overreach:* the wolf's own ears rupture, costing
`earHealth` and hearing.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Shatterhowl | gathered, 5-tile cone | 25 | 22 base, deafens, and knocks bars back 20%. |
| Resonance | instant, range 5 | 18 | Makes a foe's sword or armour ring until it cracks: a large loss of durability and 10 damage to the wearer. A sword can break. Hardiness from Gifted Gravity's Settle helps an item hold out. |
| Thunderclap | instant, within 2 | 20 | 10 base and −40% bar to everyone in it, allies included. The caster is deafened too. |
| Dread Note | instant, within 4 | 15 | Foes only: NPCs below 50 health flee, and players get −15% to hit for 2 turns. |
| Battle Sense | passive | — | The wolf gives off no scent and makes very little noise (×0.2), in the world and in fights. The rogue family. |

### Blinker

**Domain:** moving from one place, or state, to another (the bible).

**Gifted.** *Tell:* a hop; a wolf that is pinned, rooted or prone can't blink. *Cost:* dizziness, −15% to hit and
to dodge on the next turn. *Limit:* 2–3 tiles, self only.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Shortcut | work | 6 | Blinks across a wall, a gap or a doorway: couriers, burglars, escapes. |
| Slip | reaction, 3-turn cooldown | 10 | When the wolf is attacked, it blinks 1 tile straight back (or to the nearest empty tile) and the attack misses. |
| Interpose | reaction, 3-turn cooldown | 12 | When an adjacent ally is attacked, the wolf blinks in front and takes the blow on guard. |
| Blink | instant | 10 | Blinks up to 3 tiles. |

**Slip and Interpose never chain.** When either fires, neither can fire again until the wolf's own next turn, so it
can't Interpose into a blow and then Slip out of it.

**Quickened.** *Tell:* none. *Cost:* nausea and nosebleeds (health). *Overreach:* dissociation, so the wolf loses its
next turn.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Blink Strike | instant, range 6 | 15 | Blinks behind a foe and attacks in the same action, as from behind. |
| Chain Blink | instant | 35 | Blink Strikes up to 3 different foes in one turn at 60% damage each. The heaviest nausea. |
| Displace | instant, touch | 18 | Sends a foe up to 4 tiles away: into a Wall of Fire, or away from their side. |
| Unmoor | instant, touch | 30 | Pulls a foe's mind loose from their body: they lose their next turn and then fight disoriented (no planning ahead, −hit) for 2 turns. |
| Extract | instant, range 4 | 15 | Blinks an ally to a safe tile, or swaps places with them. |

### Gravity (new family)

**Domain:** weight. Gravity is not wind and not displacement: it changes how heavy something is, never where it is.

**Gifted.** *Tell:* tapping feet, which anyone listening can hear (doc 40 hearing). *Cost:* the wolf feels heavy
afterwards (slower) and gets pressure headaches. *Limit:* one wolf or object, a modest change of weight.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Lighten Load | work | 6 | The wolf, or one it touches, can carry more for a while (the load rules). |
| Settle | work | 8 | Used while crafting a weapon or armour: the piece gets more hardiness (maximum durability). |
| Lighten | channelled, range 4 | 4/turn | An ally's armour and load count as lighter: further moves for less stamina. |
| Burden | channelled, range 4 | 5/turn | A foe's weapon and armour count as heavier: each attack and move costs more stamina. |
| Anchor | channelled, range 4 | 4/turn | An ally can't be shoved or knocked down. Unlike Earth's Firm Footing, it moves with the wolf. |
| Lift Up | instant, range 5 | 12 | Lifts a Downed ally to their feet from a distance, as if tended: the downed period ends, and nothing is healed. |

**Quickened.** *Tell:* a faint tap of the feet. *Cost:* the wolf feels heavy (slower) and gets pressure headaches.
*Overreach:* a blackout, so the wolf loses its next turn.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Crush | channelled, range 5 | 12 + 6/turn | The foe weighs a great deal: 8 a turn, more in heavy armour, and they can't move more than 1 tile. |
| Slam | two turns, range 5 | 30 | Turn one lifts the foe, and they lose their turns while held. Turn two drops them: 40 base and knocked down. A hit on the caster in between drops them early at half damage. |
| Well | gathered, range 6 | 25 | For 3 turns everyone within 3 tiles of a point is pulled 1 tile toward it at the start of each turn, allies included. |
| Hurl | instant, range 4 | 18 | Makes a foe nearly weightless and throws them up to 4 tiles: 15 if they hit a wall or another wolf. |
| Weightless | instant | 20 | For one turn, allies within 5 tiles move 3 tiles further and pay no stamina to move. |

### Seer (new family)

**Domain:** what is about to happen, seconds ahead (hours, faintly, for weather).

**Gifted.** *Tell:* eyes go glassy. *Cost:* a headache, then double vision (−to-hit). *Limit:* seconds ahead, only
of what the Seer can see, and blurry.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Weathereye | passive, work | — | Bigger harvests and forage amounts. |
| Danger Sense | passive | — | Warns of an ambush ahead on the road. |
| Read the Line | instant, in sight | 10 | Shows one foe's planned next move and target (the doc 37 ghost plan). |
| Forewarn | instant, range 6 | 8 | An ally gets +25% to dodge the next attack on them. |
| Never Surprised | passive | — | Hidden attackers don't get the opening on the Seer's side (doc 40). When the fight starts, one sneaking foe is shown. |
| Glimpse the Order | instant | 5 | Shows the next few turns' order. |

**Quickened.** *Tell:* a flicker of the eyes. *Cost:* headaches and double vision. *Overreach:* losing track of
"now": −hit and a slower bar.

| Ability | Kind | Mana | Effect |
| --- | --- | --- | --- |
| Seen Opening | instant | 15 | The Seer's next attack can't miss and lands on the foe's least armoured zone. |
| Riposte | channelled stance | 6/turn | The next attack on the Seer misses and the Seer strikes back at once. Ends when it fires. |
| Doom Mark | instant, in sight | 20 | The Seer sees a foe's death: for 3 turns the foe can't dodge and everyone gets +15% to hit them. |
| Shared Sight | channelled | 6/turn | Allies within 3 tiles get Forewarn every turn. |
| Critical Sight | passive | — | No critical, and the Seer takes 15% less damage from everything. |

## Registry entries for the new families

The bible says a new family must be formally added to the registry. These are the entries; they could later be copied
into the bible itself.

**Sound**
- *Domain:* voice, vibration, noise. *Common name:* Singers (placeholder).
- *Gifted:* hushing, throwing a voice, hearing flaws in metal and stone. *Quickened:* sound as force: deafening,
  stunning, shattering.
- *Limit:* the strength of the wolf's own voice (Gifted). *Cost:* hoarseness, ringing ears, ruptured ears.
  *Tell:* a hum.
- *Cannot:* move air (Wind), make light, reach minds.
- *Professions:* smiths' assayers, criers, herders, miners. *Military:* signalling, quiet scouting. *Criminal:*
  burglary, decoys. *Countermeasures:* Steal Breath, deafness, wax in the ears.

**Gravity**
- *Domain:* weight. *Common name:* Weighers (placeholder).
- *Gifted:* lightening and burdening one thing, hardening crafted gear. *Quickened:* weight as a weapon: crushing,
  lifting and dropping, pulling.
- *Limit:* one target (Gifted). *Cost:* heaviness and pressure headaches, blackouts. *Tell:* tapping feet.
- *Cannot:* move a thing without changing its weight (that is Wind or Blinker), fly.
- *Professions:* porters, armourers, masons. *Military:* sieges, heavy infantry. *Criminal:* moving heavy loot.
  *Countermeasures:* attack the tapping wolf, ears to hear it.

**Seer**
- *Domain:* the near future. *Common name:* Seers.
- *Gifted:* glimpses seconds ahead, and of weather hours ahead. *Quickened:* reading a fight as it will go.
- *Limit:* seconds, only of what the Seer sees. *Cost:* headaches, double vision, losing track of now. *Tell:* glassy
  or flickering eyes.
- *Cannot:* see the far future, read minds, undo anything.
- *Professions:* farmers, sailors, guides, guards. *Military:* bodyguards, duellists. *Criminal:* cardsharps,
  lookouts. *Countermeasures:* blindness (Air Blast, smoke), fighting out of its sight.

## Choosing a Gift at character creation

A player chooses a wolf's Power Classification and, for a Gifted or Quickened wolf, one of the eight families when they
create it. They see every ability the family gives at that tier, and a short line on what it is best for, before they
commit.

### What exists

- The creator (`Client/src/ui/frontDoor.ts`, doc 29 phase 9) has a live preview beside five tabs (**Body**, **Coat**,
  **Markings**, **Eyes**, **Name & age**), a **Randomise** button, and a review page before **Confirm & create**.
- Creation is authoritative on the server (doc 19). Today every new character is Normal, and "the client cannot grant
  magic eligibility". This plan changes that: the player's choice is allowed, and the server checks it.

### The Gift tab

A sixth tab, **Gift**, between Eyes and Name & age. It has two steps on one panel.

**1. Classification: three cards.** Normal is chosen by default.

| Card | Line on the card |
| --- | --- |
| **Normal** | No Gift. Strength through skill, training, gear and friends. A seasoned Normal fighter is a match for most Gifted wolves. |
| **Gifted** | A small, real Gift. Best for industry and for supporting a party: better work, and a fight's odds turned your side's way. No magical damage. |
| **Quickened** | A Gift at a frightening scale. Best for magical combat. Rare, feared, and watched: Wardens take an interest in Quickened magic that others see. |

**2. Family: eight cards** (shown only for Gifted or Quickened). Each card has the family's name, a small symbol and its
best-for line *for the chosen tier*, since Gifted and Quickened wolves of one family play very differently. Choosing a
card opens its details below the grid.

| Family | Gifted: best for | Quickened: best for |
| --- | --- | --- |
| Fire | Smiths, cooks and potters. In a fight, the medic who stops bleeding and finds hidden foes by their heat. | The destroyer: cones, lances and walls of fire. Sees through stealth. |
| Earth | Masons, potters and miners. In a fight, makes the ground work for your side and feels sneakers' footsteps. | The fortress: walls, fissures and thrown stone, armoured on every side. |
| Water | Farmers, tanners and dyers, and mending injuries between fights. In a fight, puts out fire and blinds foes. | The controller: waves, floods, and freezing the wolves caught in them. Shrugs off fire. |
| Wind | Millers, farmers and smiths. The hunter's and sneak's friend: turns the wind against noses. | The skirmisher: throws wolves about, steals their breath, moves twice as fast. |
| Sound | Smiths, appraisers and repairers. In a fight, the drummer who speeds the side and hushes its steps. | The rogue: unseen and unsmelt, shatters weapons and ears. |
| Blinker | Couriers and burglars. In a fight, the bodyguard who slips blows and steps in front of them. | The assassin: strikes from behind, scatters foes, unmoors minds. |
| Gravity | Porters, armourers and masons. In a fight, lightens friends, burdens foes and lifts the fallen. | The crusher: lifts, drops and pulls wolves where it wants them. |
| Seer | Farmers and foragers. In a fight, calls out what's coming. | The duellist: can't be flanked, never misses its opening, marks a foe for death. |

**The details** for the chosen family and tier:

- **The Gift itself:** the family's domain in one line, then its **Tell**, **Cost** and **Limit** at this tier, as in
  this doc. A player choosing a Gift should know how a foe will spot it.
- **The abilities**, one row each: the name, chips for its kind (Work, Fight, Channelled, Reaction, Fight-long,
  Passive, Shape) and its mana cost, then a one-line effect. Gifted lists split into **At work** and **In a fight**.
- Nothing more: no numbers beyond mana, no formulas. The full rules are in game.

```text
┌ preview ────────┐  Body  Coat  Markings  Eyes  [Gift]  Name & age
│                 │
│     (wolf)      │  ( Normal )  ( Gifted ✓ )  ( Quickened )
│                 │
│                 │  [Fire] [Earth] [Water✓] [Wind] [Sound] [Blinker] [Gravity] [Seer]
│  Gifted · Water │
└─────────────────┘  WATER · Gifted
                     Farmers, tanners and dyers, and mending injuries between fights.
                     Tell: water within 2 tiles or an open waterskin.  Cost: thirst.
                     Limit: about a bucket at a time.
                     AT WORK
                       Draw Water   Work · 4   Irrigation, filling, washing hides.
                       Mend         Work · 10  An acute injury heals 1.5× faster.
                     IN A FIGHT
                       Douse        Fight · 5  Puts out an ally's Burning.
                       …
```

- The preview's caption under the wolf shows the choice ("Gifted · Water"). The wolf's look doesn't change.
- **Randomise** leaves the Gift alone; it is a choice about play, not looks.
- The **review** page lists the classification and family with the other choices, and repeats the best-for line.
- At phone width, the family cards wrap to two columns and the details sit below them.
- Switching between Gifted and Quickened keeps the chosen family and swaps its details.

### Where the text comes from

One catalog, `Data/Gifts/families.json`, holds each family's name, domain, best-for lines, Tell, Cost and Limit per
tier, and each ability's name, tier, kind, mana and one-line summary. The server reads it for the rules and sends the
creator the player-facing part, so the screen can never describe a Gift differently from how it works. A check (like
`tools/item_catalog.py`) confirms that every family has both tiers, every ability has a kind and a summary, and Death
Walker is marked NPC-only.

### On the server

- The create request gains `gift`: `{"tier": "normal" | "gifted" | "quickened", "family": "<one of eight>"}`. A
  missing `gift` means Normal, so older clients still work.
- The server refuses an unknown tier or family, a family for a Normal wolf, a Gifted or Quickened wolf with no family,
  and Death Walker. The choice is kept with the character's other creation choices, under the same command ID
  (retrying with a different Gift is refused, as today).
- The character starts with that family's mana and abilities. After creation only the DM can change a Gift (Give
  Gift, Make Quickened, Take Gift away), as now.
- Doc 19's line about magic eligibility is updated to say this.

Tests: the wire refuses bad choices and keeps a good one; a new character has the right family, tier and abilities;
the catalog check; the front-door browser test picks a Gifted Water wolf and a Quickened Blinker, sees their
abilities, and creates them.

## Changes to what is built

- Flamethrower: Quickened only. A Gifted Fire wolf gets the Gifted Fire list instead.
- `gift` accepts the eight families, and the DM's Give Gift offers all eight (Death Walker for NPCs only).
- Character creation offers Normal, Gifted or Quickened and a family (above).
- A record of Warden attention per character, written whenever a witness notices Quickened magic.

## Phases

| # | What | Where |
| --- | --- | --- |
| 1 | The frame: the eight families, ability kinds (instant, gathered, channelled, reaction, fight-long, passive), Tells that can be broken, "no critical", Flamethrower Quickened-only, DM Give Gift for all eight | server + page |
| 2 | The Gifts catalog (`Data/Gifts/families.json`) and its check; choosing a Gift at character creation | data + server + page |
| 3 | Gifted combat support for all eight families | server + page |
| 4 | Gifted work: crafting, repair, harvest, load and durability bonuses | server |
| 5 | Painting shapes in the arena; Quickened Fire and Earth | server + page |
| 6 | Quickened Water, Wind, Sound, Blinker, Gravity, Seer | server + page |
| 7 | NPCs use their Gifts in fights; witnesses raise Warden attention | server |

Phase 2 can ship before the abilities work: a wolf created then has its family and tier, and gains each ability as
its phase lands.

## Open questions

| Question | Note |
| --- | --- |
| Mana pools, regen and spell power by tier | Left for the stats pass. Today's pool (about 44) fits about one big Quickened spell per fight. |
| Should anything limit choosing Quickened? | **Decided (the user, 2026-10-04): free for now.** It will later become part of the social levelling system (doc 08). |
| The Warden system that reads Warden attention | A later doc. |
| Buffs to an ally's damage for some Gifted families | Acceptable later; left out of this first set. |
| Should Tailwind and Battle Sense work outside fights? | Battle Sense yes (it is the rogue's sneaking); Tailwind assumed fights only. |

## Built

**2026-10-04: the families, and choosing a Gift at creation** (phase 2, and the parts of phase 1 that need no new
ability):
- **The catalog** `Data/Gifts/families.json`: the three tiers' best-for lines; the eight playable families and the
  NPC-only Death Walker, each with its colour, domain and, per tier, best-for line, Tell, Cost, Limit (Quickened:
  Overreach) and abilities (id, name, kind, mana, `perTurn`, `perTile`, summary; `work: true` for a passive that helps
  at work). `tools/gift_catalog.py` checks it (both tiers whole, known kinds, unique ids, the eight playable, the Death
  Walker NPC-only, no Gifted shape, gathered or two-turn ability); `tools/test_gift_catalog.py`.
- **The server** (`Core/RatwGifts.*`): `gifts::known`, `playable`, `name`, `creatorCatalog` and `hasFlame`.
  - A saved Gift may be any family in the catalog (`wire::readEntity`).
  - `World::giveGift` gives a player any playable family, an NPC any family, and refuses others.
  - **Flamethrower is Quickened Fire only**: `battle "flame"` and `castFlame` refuse a Gifted Fire wolf ("Your Gift
    of fire is too small for that."), and the fight view sends `flameLength`/`flameAngle`/`flameMana` only to a
    Quickened one, so only they get the button.
- **Creation** (`character_create`): an optional `gift: {"tier", "family"}`, refused unless the tier is normal,
  gifted or quickened, a non-Normal wolf has a playable family, and a Normal wolf none. The Gift is part of the
  request's fingerprint only when it isn't Normal, so a Normal wolf's receipt is what it always was. The character
  starts with the Gift and its mana. The lobby sends `gifts` (the creator's catalog) to a signed-in account, and each
  roster character's `gift` and `quickened`.
- **The creator** (`Client/src/ui/frontDoor.ts`): the **Gift** tab between Eyes and Name & age, as planned above:
  three classification cards, eight family cards (a coloured edge each) with the tier's best-for line, and the chosen
  family's Tell, Cost, Limit, Overreach and abilities (kind and mana as chips; Gifted split into At work and In a
  fight). Choosing a family scrolls its details into view. The preview caption, the review and the roster show the
  Gift ("Quickened · Blinker"); review sends a Gifted or Quickened wolf with no family back to the Gift tab. The
  creator is a fixed 1440×940 layout scaled to the window (doc 29), so it has no separate phone layout.
- **The character sheet** names the tier and family ("Gifted · Water").
- **The Dungeon Master** (`Editor/src/dm/DmApp.tsx`, `tools/dungeon_master.py`): a family list with Make Gifted,
  Make Quickened and Take Gift away; `character.gift` takes any playable family.
- Tests: `battle_tests` (`theFlame`: a Gifted Fire wolf has no Flamethrower, a Quickened one has; `dodgingTheFire` at
  the Quickened countdown; `giftFamilies`: the catalog, each family given to a player, the Death Walker only to an NPC;
  the save keeps any known family and drops an unknown one); `game_tests` (`accountsAndARestart`: the lobby's eight
  families, bad Gift choices refused, a Quickened Blinker and a Gifted Water wolf made and shown on the roster, a
  reused request ID with another Gift refused); `test_dungeon_master`; the front-door browser test (the tiers, the
  eight families, the Gifted Water abilities, switching tier, the preview, the Gift sent, the roster, a Normal wolf
  sending none). Checked in a scratch Greyfen server: an account made a Quickened Blinker and it came back on the
  roster.

**2026-10-04: the abilities, in the game** (phases 1 and 3 to 7):
- **The rules** (`Core/RatwMagic.cpp`, World members; `Core/RatwBattle.cpp` calls in at a turn's start, a step, a blow,
  a hit, a cast going off, noticing and NPC turns):
  - Each fight ability's aim (`target`: self, foe, ally, downed, any, tile, dir, shape, foe+tile), range, area, gather
    time, the Tell's stamina and hurt, and weight are a table there (`Rules`); its name, kind and mana come from the
    catalog (`gifts::ability`).
  - **On a fighter** (`BattleFighter::Magic`): a held Gift (`channel`, on whom), effects with turns left (`fx`: frozen,
    prone, held, crushed, burdened, lightened, anchored, firm, breeze, weightless, doomed, forewarned, splashed, dusted,
    soaked, deafened, dread, unmoored, disoriented, breathless, seen_opening, riposte, warmed, washed, read,
    stone_armor, water_screen, and the Costs: dizzy, nausea, thirsty, dehydrated, hoarse, heavy, double_vision,
    cracked, dissociated, blackout), cooldowns, armed reactions, and the last ability used (Overreach).
  - **On the ground** (`Battle::ground`): fire, fissure (no one stands there), wall (blocks movement; crumbles after 8
    rounds: nothing breaks it sooner yet), loose, slick, water (wading: half the move, no burning, Freeze) and a well, each for some
    rounds. `arenaOpen` treats walls and fissures as solid.
  - **Kinds:** instant; gathered (a cast on the Flamethrower's countdown and locked tiles: Heat Lance, Wall of Fire,
    Upheaval, Hurl Stone, Fissure, Stone Wall, Wave, Flood, Pressure Drop, Shatterhowl, Well); channelled (mana each turn
    at its start; let go by moving, being hit, going down, running out of mana or Let go); reactions (Slip, Interpose:
    armed at any time, fire by themselves, rest 3 turns, never chain: one fired, neither again until its own turn);
    fight-long (Stone Armor, Water Screen: before or on one's first turn, no action spent); passives (Heat Sense,
    Tailwind, Battle Sense, Critical Sight, Never Surprised); Slam's two turns.
  - **Tells:** breath (Fire, Wind, Sound: none while Breathless), bare braced paws (Gifted Earth; never on wood: `8`
    bridge or pier, `R` carpet, `z` straw), water within 2 or 4 tiles, rain, or a skin of water carried (Water),
    a hop (Gifted Blinker: not prone, frozen, held or crushed), tapping feet (Gravity: a Gifted one's gives away a
    hidden wolf). Every Gift used costs its family's Cost; a Quickened wolf may cast past its mana, or the same Gift
    two turns running, and **overreaches** (fire overheats, earth cracks its pads, water wrings it dry, wind makes it
    dizzy, sound ruptures its ears, a Blinker or Gravity wolf loses its next turn, a Seer loses track of now).
  - **No critical** (`World::unflankable`: Quickened Fire and Seer, Stone Armor, Water Screen): no side, back or
    ambush bonus to hit, and no ambush damage.
  - Damage: base × spell power (0.5 + WIS/100) × 0.85–1.15; stone and water blows through the armour where they land.
    Water Screen cuts fire (and burning) to a quarter; Stone Armor takes 5 off a blow (at least a quarter gets through);
    Critical Sight takes 15% off everything.
  - Senses (`magicSenses`): Hush (no noise within 2 of the hummer), Turn the Wind (the side's scent kept from noses),
    Washed (no scent), Battle Sense (no scent, a fifth of the noise, in the world too), grit (half the sight),
    deafness (no hearing). Never Surprised: a Gifted Seer's side can't be ambushed, and one sneaking foe is shown when
    the fight begins; Heat Sense (Quickened) finds hidden wolves within 12 tiles at each of its turns.
  - **NPCs** with a Gift use it (`npcGift`): the Quickened strike with theirs when the mark is in reach and the mana is
    there (never overreaching); the Gifted cauterize, douse, lift the fallen, arm Slip, flare, splash or blast grit.
  - **Warden attention** (`Entity::wardenAttention`, saved): each Quickened Gift used adds one for each witness (other
    fighters, watchers, residents near by who can see it), five at most a cast.
- **The fight view** (`RatwGameBattle.cpp`): `you.gifts` (each ability: target, range, tiles, mana, perTurn, perTile,
  ready and why not, on, cooldown, summary), `you.channel`, `you.manaMax`; each fighter's `fx`, `gift`, `quickened`,
  and its `intent` when one's side has read its line; `ground`; each cast's `spell`. Commands: `battle` verbs `gift`
  (`ability`, and `target`, `x`/`y`, both, `tiles` or `text`), `react` (`ability`, `on`) and `letgo`.
- **The fight screen** (`Client/src`):
  - A **Gift row** above the action bar in the family's colour: the family, mana, and a button an ability (kind,
    mana, held/armed/on/rests, why not on hover). While aiming, what to click is said in the row.
  - Aiming (`state.ts`): a wolf (rings on those in reach), a tile (its area previewed), a way (the line, cone or wave
    previewed), a shape (tiles painted one by one, joined, numbered, with the mana so far; Enter or the button casts),
    or a foe and then where to send it. Esc cancels.
  - **Glyphs** (`paint.ts`, `fightFx.ts`): the ground's marks (fire `^~*`, water `≈`, fissure `≀`, wall `#`, loose `∴`,
    slick `~`, a well's `@` and its rings); a gathering Gift's locked tiles in its family's colour, with its countdown;
    each family's glyphs flickering over the tiles its magic takes (fire `^*~`, earth `▲▪∴`, water `≈~°`, wind `≋~»`,
    sound `)(·`, blinker `✦·*`, gravity `◎•○`, seer `◉✧·`); a blink's spark and trail; a crash's `✸`; a turning ring round
    a wolf holding a Gift; a glyph beside each wolf for what is on it (frozen `*`, held `↑`, doomed `☠`...); a gold
    dashed line from a read foe to whom it means to strike. Chips on the cards name each effect and its turns.
  - The old Fire button gives way to the Gift row's Flamethrower.
- **At work** (`World::useWorkGift`; command `giftwork` `{ability, target}`; the character sheet's Gift box lists them
  with Use):
  - **Lent to a workshop** within 4.5 paces: Forge Heat, Kindle, Clay Hand, Stone Sense, Draw Water, Winnow and Dry,
    Bellows, Ring True, Settle, each to its trades (a smithy, bakery, pottery, tannery, mill...). The maker's next batch
    scores 8–15 more toward a better quality (`Society::lendGift`, 30 at most; RatwCrafting.cpp), and the shop pays the
    wolf 4p if it can. Once in ten minutes each.
  - **For oneself:** Mend (an acute injury's rest left cut by a third, once a day each), Shortcut (three lengths the
    way one faces, over what is between), Lighten Load (half again to carry for ten minutes), Carry (speech carries as a
    yell for two minutes), Dowse (where the nearest water is), Echo (the place's size and doorways). Once a minute.
  - **Always:** Ring True makes a Gifted Sound wolf's repairs a quarter cheaper; Weathereye forages a quarter more (one
    at least); Danger Sense tells a Gifted Seer of a bandit within 24 paces, once each.
- Tests: `Tests/magic_tests.cpp` (147 checks: options and Tells, each family's abilities, effects, ground, reactions,
  channels and their breaking, Overreach, Warden attention, the work Gifts, saving); `battle_tests` (a stale pointer in
  the walled-off test fixed); the client's `battle.test.ts` (reading, aiming every kind, painting a shape, sending,
  drawing). `tools/client/gifts.mjs` plays eight duels in the real page on a scratch Greyfen (Wall of Fire painted and
  Heat Lance, Stone Armor and a Stone Wall, Flood and Freeze, a Well and Slam, a Battering Gust, Read the Line, Blink
  Strike, Shatterhowl), lends Forge Heat to the wolf keeping the forge, and screenshots the character sheet. No page
  errors.

Not built yet: walls blocking sight (they block movement only); Echo's map; Gifted Water's tanning and dyeing beyond
the lent lift; NPCs choosing Gifted support beyond the few above. Whisper Thread asks for its words with the browser's
prompt.
