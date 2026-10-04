# Injuries: no death for players, and wounds that stay

Status: **plan, drafted October 4, 2026; phase 1 (no death, the downed period, rest) built the same day** (see Build
phases). It replaces the death rules in
[33-combat.md](33-combat.md) ("Downed and death") for player characters. Doc 33 keeps the fight rules: health, the
Downed state, struggling up and tending. All numbers are starting tuning, not final balance.

## Overview

Player characters no longer die. A wolf at 0 health is Downed, and it stays down for a while instead of bleeding out.
What a fight costs is time and the body: each downing keeps you down longer if you haven't rested since the last one,
and fights leave injuries that the player does not choose and cannot wave away.

Injuries come in three kinds:

| Kind | What it is | How long it lasts | Example |
| --- | --- | --- | --- |
| **Combat** | What a wolf suffers during a fight | Until the fight ends (health heals as now) | Bleeding, Staggered, Burning, Winded |
| **Acute** | Real damage that heals with rest and time | Days to weeks of game time | Sprained foreleg, cracked rib, deep gash, burns |
| **Lasting** | A mark for life | Permanent | Torn ear, bent tail, permanent limp, bad back, scars |

**Most fights leave nothing behind.** Acute injuries come from being Downed or from very heavy hits. Lasting injuries
are rare: they come from enormous single blows, from being Downed again and again without rest, and from fighting on a
badly injured body.

Why: permanent death made every fight a gamble with the whole character, which suits a combat game but not an RP one.
Injuries keep fights meaningful. A wolf with a torn ear and a limp carries a history that other players can see and
role-play with.

## Game time, for scale

A game day is 4 real hours (`calendar::SecondsPerDay` = 14400 s), so a game hour is 10 real minutes and a game week is
28 real hours. "Days to weeks" of healing means a few real hours to a couple of real days. Most of that time a player is
logged out, so logged-out time counts as rest (below): a full rest if they left lying in a bed, a partial one if not.

## Downed: down, not dying

A wolf at 0 health is Downed as now: a sideways `W` flashing orange, crawling at most, unable to attack. The death
timer becomes a **downed period**. When it runs out the wolf gets up on its own at 10 health (Limping). Nothing kills it.

Ways up are as in doc 33:

- **Waiting out the period.**
- **Struggling up**, once per game day.
- **Being tended** by someone else, which ends the period at once.

The period runs in real time, in a fight and out of one, logged in or not. A player who logs out while Downed leaves the
world as any player does (after lingering, in a fight), and the time away counts the period down: back after it ran
out, they are up at 10 health where they lay. Lying down hurt is not rest.

### Repeated downings stretch the period

Every wolf keeps a count of **downings since its last full rest** (see Rest). It goes up by one each time the wolf goes
down and back to 0 after a full rest.

```text
downed period  = base(cause) × stretch(downings since full rest, counting this one) + overkill × 2 s
stretch        = 1×, 2×, 4×, 6×, then 8× for the fifth and after
capped at 30 min
```

| Cause of the downing blow | Base |
| --- | --- |
| Blunt or beating (guard strike) | 3 min |
| Bite or Sword | 2.5 min |
| Fire (including Burning) | 2 min |

A first downing keeps a wolf down about 2½ minutes, about as long as a short fight. A wolf that goes down a fourth time
in an evening without resting lies there 15 minutes. That is the cost of fighting on when you should be resting, along
with the lasting injuries below.

**NPCs stop attacking a Downed wolf** (as now), and a fight whose side is all Downed is lost (as now).

### What becomes of death

- No gameplay path kills a player character. The `dead` flag and `World::setDead` stay, for a Dungeon Master's
  storyline or a correction, never for a timer.
- Characters already dead in DEV are brought back by the Dungeon Master, as today.
- **NPCs.** Recommendation: the same rule for residents. A town whose blacksmith dies in a brawl loses a shop for good,
  and doc 26's lives assume residents persist. Bandits and other hostile NPCs a Dungeon Master calls up could be made
  mortal with a flag, or slink off when they get up. See Open questions.

## Combat injuries

These belong to a fight. They begin with a blow and end when the fight ends, or sooner. Health (`hurt`) and its effect
on speed stay as in doc 33 and heal as now (50 per game hour, once out of the fight).

| Injury | Cause | Effect | Ends |
| --- | --- | --- | --- |
| **Burning** (built) | Fire | Damage at the start of each turn | Rolling, or after its turns |
| **Bleeding** | A bite or sword hit for 18 or more | 2 damage at the start of each turn | 3 turns, or tended |
| **Staggered** | A single hit of 25 or more | Next bar starts 20 lower | The next turn |
| **Winded** (built as exhaustion) | Stamina 0 | No physical attacks until stamina 20 | Recovery |

They show in the fight panel and over the wolf in the arena, and the log says when one starts ("Bo is bleeding").
Combat injuries are how a fight feels in the moment. They never outlast it.

## Acute injuries

### When one happens

Acute injuries are not handed out every fight. A wolf gets one when:

| Trigger | Chance |
| --- | --- |
| It is Downed | Always: one acute injury from the downing blow |
| It takes a single hit of 25 or more without going down | 25% |
| It ends a fight at 75 hurt or more (Limping) without being Downed | 40% |

A wolf can come away from a fight with more than one, from a heavy hit and then a downing. If it already has the same
injury, that injury gets worse by one severity and its healing starts over instead of adding a second copy.

### What it is

The cause picks the injury, and the damage picks how bad it is:

| Cause | Possible injuries |
| --- | --- |
| Bite | Torn flank, bitten foreleg, bitten hind leg, torn ear (acute), wrenched neck |
| Sword | Deep gash, cut foreleg, cut muzzle, cut shoulder |
| Blunt | Bruised ribs, cracked rib, sprained foreleg, knocked senseless |
| Fire | Burned paws, singed coat, burned muzzle |

| Severity | When | Rest to heal |
| --- | --- | --- |
| Minor | Damage up to 20 | 1–2 game days |
| Moderate | 21–35, or a downing | 3–6 game days |
| Severe | Over 35, overkill 15 or more, or a downing with 2 or more downings since full rest | 7–14 game days |

### Effects

Each injury touches one thing, and minor ones barely show. These stack across injuries, with a floor so a wolf is never
crippled by acute injuries alone: speed at least 60%, stamina recovery at least 50%, senses at least 50%.

| Injury | Minor | Moderate | Severe |
| --- | --- | --- | --- |
| Leg (bitten, cut, sprained) | Sprint −5% | Sprint −10%, arena move −1 | Sprint −20%, arena move −1, world walk 2.0 |
| Ribs, flank, back | Stamina recovery −10% | −20%, attacks cost +2 stamina | −35%, attacks cost +4 stamina |
| Neck, shoulder | Bite damage −1 | Bite and sword −2 | −4 |
| Muzzle, nose | Smell −10% (`noseHealth`) | −25% | −40% |
| Ear | Hearing −10% (`earHealth`) | −25% | −40% |
| Knocked senseless | Vision −10% (`eyeHealth`) | Initiative −10% | Initiative −20%, vision −25% |
| Burns | Fire hurts the wearer +1 | Stamina recovery −10% | −20%, and fire hurts +2 |

The ear, eye and nose effects use the `earHealth`, `eyeHealth` and `noseHealth` fields every character already has.
The client already reads them, so hearing, sight and smell narrow without new plumbing.

### Healing: rest, in a row

Each acute injury needs a number of **rest hours** (game hours) to heal: 24 per game day of the table above. Rest hours
build up while the wolf is resting:

| Resting | Rest hours per game hour |
| --- | --- |
| Lying in a bed or on straw (a full rest's way) | 1.5 |
| Lying down or sitting anywhere else, out of a fight, not moving (a partial rest) | 0.75 |
| Logged out: in a bed if they left lying in one, else as a partial rest | 1.5 or 0.75 |
| Up and about | 0.25 (a wolf heals a little anyway) |
| Sprinting, fighting, Downed | 0 |

**Strain sets it back.** Fighting on an unhealed acute injury aggravates it:

- Going into a fight takes off 10% of the healing done so far on each acute injury.
- Being Downed takes off half of it.
- Being Downed with a **severe** injury on that body part can turn it lasting (below).

Rest is the cure. Fighting through injuries is a choice the player makes, and the body keeps the score.

As an injury heals it steps down: severe to moderate to minor to gone, with its effects easing at each step. The player
sees, for example, "Cracked rib: moderate, healing (about 2 more days of rest)".

Herbs and healers (doc 33's future work) speed this up later, as a multiplier on rest hours. They never skip it.

## Lasting injuries

### When one happens

Lasting injuries are the rare, memorable ones. Each trigger is a roll, made when it happens:

| Trigger | Chance |
| --- | --- |
| A single hit of 40 or more (40% of a wolf's health at once) | 35% |
| A downing blow with overkill 25 or more | 35% |
| A downing that is the 3rd since a full rest | 30% |
| A downing that is the 4th or later since a full rest | 60% |
| A downing with a severe acute injury on the struck body part | 50%: that injury sets into its lasting form |
| Burning down to 0 | 25%: burn scars |

**Going without a full rest raises every roll above**: +5 percentage points for each game day since the last full rest
(the first day free), up to +25. Partial rests heal, but only a night in a bed keeps a wolf from wearing down: one who
naps on the road between fights is the one who ends up with the limp.

Only one lasting injury per fight, whatever the rolls, so one disastrous fight leaves one mark, not five.

### What they are

| Lasting injury | Comes from | Effect |
| --- | --- | --- |
| Torn ear (one or both) | Bite, sword | Hearing −10% each (`earHealth`) |
| Bent tail | Blunt, bite | None: a mark |
| Scarred muzzle | Sword, bite, fire | None: a mark |
| Scarred flank or shoulder | Sword, bite | None: a mark |
| Burn scars | Fire | None: a mark; the coat grows back patchy |
| Notched nose | Bite, sword | Smell −10% (`noseHealth`) |
| Clouded eye | Sword, fire, blunt | Vision −15% (`eyeHealth`) |
| Permanent limp | A leg's severe injury setting | Sprint −10%; shows in how the wolf moves |
| Bad back | A ribs or back severe injury setting | Stamina recovery −15% |
| Stiff shoulder | A neck or shoulder severe injury setting | Bite and sword −2 |

**Most lasting injuries are marks, not handicaps.** Functional ones are kept small and capped: one per body part, and
at most 25% off any one ability in total. A veteran wolf should look and feel like a veteran, not be unplayable.

### Seen by others

Lasting injuries are part of how a wolf looks. They join the character's appearance and description: "a dun wolf with a
torn left ear and a bent tail". They also join how strangers name a wolf they don't know (doc 32's veiled labels), so a
torn ear becomes something people recognise you by.

Acute injuries show while they last ("limping on a bitten foreleg"). Combat injuries show only in the fight.

## No choice

Injuries are given by the server and stored on the character, like health. The player cannot decline, hide or remove
them. A Dungeon Master can add or take one away as a correction or for a storyline, through the DM app's Players tab,
with the change logged like other DM actions.

## Rest

**A full rest needs a bed.** It is 6 rest hours without a break lying in a bed or on straw (doc 36's `b` and `z`
tiles), out of a fight: an hour of real time asleep, or logging off while lying in one (logged-out time counts half
again, so 40 real minutes away will do).

A full rest:

- sets the count of downings since full rest to 0;
- gives back the struggle-up, if it was spent (it also comes back at day rollover, as now);
- marks the day of the last full rest, which the lasting-injury rolls count from.

**Every other rest is partial**: lying or sitting still anywhere else, or logged out not lying in a bed. It heals acute
injuries more slowly (the table above) and resets nothing. Standing up, moving, fighting or going down breaks a rest,
and the six hours start again.

The status panel shows rest beside the posture: "resting in a bed · 2.3 of 6 h", or "resting (no bed: a partial
rest)". The actions row has Sit and Lie down.

## What players see

- **Character sheet: a new Injuries section.** Acute injuries with severity and time to heal; lasting ones with when and
  how they were got ("Torn left ear: in a fight with a bandit, early spring, year 2").
- **The story** (doc 18) gets one line per new acute or lasting injury, and one when an acute injury heals.
- **The fight panel** lists combat injuries. The Downed banner shows when you get up ("up in 2:30") and, after a
  second downing, says why it is longer ("Down 2 times without a full rest: it takes longer each time").
- **Look** at another wolf shows its visible injuries.

## Data

On the character (`Entity`), all saved with the character and sent to its owner:

```text
injuries[]        {id, kind ("acute"|"lasting"), type, part, severity (1..3, acute), restLeft (rest hours, acute),
                   gotDay, cause, from (who or what), side ("left"|"right"|"")}
downsSinceRest    int
restRun, bedRun   rest hours in the current unbroken rest, anywhere and in a bed (the full-rest check)
fullRestDay       calendar day of the last full rest (from first entry, for characters made before rest was kept)
awaySince, awayInBed   while logged out: the calendar day they left, and whether lying in a bed
```

Combat injuries live on `BattleFighter` and are not saved. `downedLeft` keeps its name but now counts down to getting
up, not to death.

The effects are worked out from the list wherever they apply: top speed, arena move, stamina, attack damage, senses and
initiative. They are not stored as separate numbers, so healing or a DM correction takes effect at once.

## Build phases

1. **No death.** Built October 4, 2026. The downed period replaces the death timer for players (NPCs keep doc 33's,
   pending the open question); get up at 10 health, in a fight or out; the downings count and stretch; partial and
   full rest, a full one only in a bed; time away counts; "to the death" terms read "until one goes down"; the status
   panel shows rest, the actions row gains Lie down. `battle_tests` restAndRepeatedDowns and gettingUpInAFight.
2. **Combat injuries.** Built October 4, 2026: Bleeding (a bite or sword blow of 18 or more: 2 a turn for 3 turns) and
   Staggered (a blow of 25 or more: the bar set back 20, at once or when the turn it came in ends) beside Burning and
   Winded; with Wounded, Badly hurt, Limping and Down, named on every fighter's card (one's own too) with what each does.
   `battle_tests` restAndCombatInjuries.
3. **Acute injuries.** Triggers, the table, effects, rest hours, strain, healing steps; character sheet and story lines.
4. **Lasting injuries.** Triggers, the one-per-fight rule, effects and caps, appearance and veiled labels, Look.
5. **DM tools.** Add and remove injuries from the Players tab.
6. **Balance**, with play.

## Open questions

| Question | Recommendation |
| --- | --- |
| Do NPCs die? | Residents no, by the same rule. Hostile NPCs a DM calls up: a "mortal" flag, off by default. |
| Does logged-out time count as rest anywhere? | **Decided:** as a partial rest anywhere; a full rest only if they left lying in a bed. |
| Can a downed wolf be robbed or dragged off? | Not now. Worth a design of its own (captivity, ransom) later. |
| Should acute injuries be given on every downing, or rolled? | Every downing. Losing is the main cost now that death is gone. |
| Can lasting injuries ever be healed? | No, by design; a DM correction only. Magic that heals them could be a rare, late story reward. |
