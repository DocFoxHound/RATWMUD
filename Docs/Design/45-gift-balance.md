# 45. Balancing Gifts in fights: a handoff

Written 2026-10-05 at the end of the session that built docs 43 and 44, for the session that picks this up. Read docs
43 (Gifts) and 44 (levelling) first. **Done 2026-10-05:** the simulator, the targets (agreed with the user), seven
rounds of tuning, a fix to the fight's dice, and two rule changes; see "Done" at the end.

## The user's goal

> "Right now I have a progression that I like with regular non-gifted, non-quickened wolves. Now I need to take that
> leveling scheme and analyze what it looks like with gifted and quickened wolves since they have added abilities. My
> goal is to balance gifts in fights, meaning we'll have to run several tests using them and using them smartly solo
> against other gifted and ungifted wolves, on teams with gifted and ungifted wolves, etc etc. I want to focus on
> balancing those gifts so that, even as support, they provide a noticeable but not definitive advantage in a fight."

Standing decisions to respect (docs 43, 44, and the user's words):
- **Gifted** = utility and support, no damage; **Quickened** = combat. Quickened wolves don't get the Gifted abilities.
- Tactics, gear, Gifts and numbers should decide fights more than levels or level-stacked teams. A level is only +0.5
  fighting skill (doc 44); keep that.
- "Noticeable but not definitive": a Gift should move a fight's odds clearly, never make it a foregone conclusion.
- **Mana pools are untouched** until a stats pass (doc 43): today 20 + WIS × 0.8 for both tiers (about 44 at WIS 30),
  +2 a turn in a fight. Several Quickened Gifts cost 25–40, so a Quickened wolf gets one or two big casts a fight. If
  balance needs bigger pools, propose it to the user rather than deciding.
- Every number in docs 43 and 44 is a placeholder; changing them is the job, but say what changed and why.

## Where things stand (commit 98ccb9f)

- **The rules:** `Core/RatwMagic.cpp` (every fight ability: a `Rules` table of aim, range, area, gather time, Tell
  stamina and hurt, weight; then `useGift`, `magicResolve`, `magicTurnStart` and the hooks `RatwBattle.cpp` calls).
  Names, kinds and mana are in `Data/Gifts/families.json`; `tools/gift_catalog.py` checks it.
- **NPC use:** `World::npcGift` (RatwMagic.cpp) is simple: the Quickened strike with one or two abilities when the mark
  is in reach and the mana is there (never overreaching); the Gifted cauterize, douse, lift the fallen, arm Slip, flare,
  splash or blast grit. This is what a simulator can drive, but it is not "smart".
- **Tests:** `Tests/magic_tests.cpp` (147 checks of what each ability does); `tools/client/gifts.mjs` (eight duels in
  the real page on a scratch server, with screenshots).
- **The level simulator:** `Tests/level_sim.cpp`, built as `build-gifts/level_sim [fights]` (`cmake --build build-gifts
  --target level_sim`). It sets each wolf's level through `World::levelOf`, makes fights of any size (`fight(sideA,
  sideB, trial, aFirst)`: the first of each side by challenge, the rest pushed into the fight's fighters beside them),
  gives a sword or a leather kit, and plays each turn with a scripted wolf (`play`: close on the nearest foe, bite or
  cut). About 0.25 s a fight; 300 fights a row.
- **Baseline (ungifted, 300 fights, starts alternated):**

| Fight | First named wins |
| --- | --- |
| L5 / L10 / L25 vs L1 | 59% / 66% / 86% |
| L1 vs L1, L5, L10, L25, the L1 striking first | 69%, 65%, 54%, 21% |
| L1 leather vs L25 bare; L1 sword vs L25 bare | 68%; 98% |
| two L1 vs one L25; two L5 vs one L15; two L1 vs two L25; three L1 vs two L25 | 100%; 100%; 16%; 91% |

  The first blow is worth about 66–67% between equals. The sword nearly decides fights alone (a gear question, noted
  in doc 44, not this one's).

## The plan

1. **Give the simulator Gifts.** Add `gift` and `quickened` to its `Wolf`, set with `World::giveGift`. Give each wolf
   a full mana pool at the start (a fight's mana is the entity's `mana`).
2. **Teach it to use them smartly**: a per-family policy in `play()` (not `npcGift`, which stays the NPCs'), choosing
   each turn between its Gift and a blow. At least:
   - Quickened: open with the strongest cast in reach (Flamethrower or Heat Lance at range, Wall of Fire or Fissure
     between it and a closing foe, Hurl Stone, Wave/Flood then Freeze, Battering Gust, Shatterhowl, Blink Strike,
     Slam or Hurl, Seen Opening before a blow, Doom Mark for a team); fight-long ones (Stone Armor, Water Screen) on the
     first turn; never overreach unless winning on it; back off while gathering.
   - Gifted: support the side (Cauterize a bleeder, Forewarn the one under attack, Firm Footing or Anchor the
     front-liner, Burden or Splash the strongest foe, Air Blast, Lift Up the fallen, Hush or Turn the Wind before a
     sneak, Slip and Interpose armed) and bite when there is nothing better.
   - A "naive" policy too (cast whatever is ready), to see how much smart use matters.
3. **Run the matrix**, each at levels 1, 10 and 25, starts alternated, 300 fights:
   - each Quickened family vs an ungifted wolf of the same level, and vs each other Quickened family;
   - each Gifted family vs an ungifted wolf (alone it can only help itself: expect close to 50%);
   - teams: Gifted + ungifted vs two ungifted (what the support is worth); Quickened + ungifted vs two ungifted;
     Gifted + Quickened vs Quickened + ungifted; three-a-side mixes;
   - with gear (sword, leather) on both sides, so Gifts are judged against armed wolves too;
   - level gaps: does a Quickened L1 beat an ungifted L10, L25? (Gifts should count more than levels.)
4. **Targets** (to agree with the user before tuning):
   - a Quickened wolf vs an ungifted equal: about 60–70% (noticeable, not definitive; compare the first blow's 66%);
   - a Gifted supporter added to a pair vs an ungifted pair: about 55–65% for its side;
   - no family far from the others (each Quickened family within about ±7 points of the rest);
   - no ability that wins alone (Unmoor, Slam, Freeze and Chain Blink are the likeliest to be too strong; Interpose
     and Riposte cancel blows outright).
5. **Tune** the `Rules` table and the catalog's mana, the effects' turns and the damage bases in `magicResolve` and
   `useGift`; re-run; record each round's table in this doc. Re-run `magic_tests`, which checks behaviours, not
   numbers, and fix any it pins.
6. Then (with the user) the open questions: Quickened mana pools, and Gifted damage-buff support (doc 43 left out).

## Traps learned this session

- **Other sessions work in this tree.** A session building doc 42 (money) has uncommitted changes in shared files
  (`RatwWorld.h`, `RatwSociety.h`, `RatwGame.cpp/.h`, `RatwWire.cpp`, `CMakeLists.txt`, `RatwCrafting.cpp`,
  `RatwHunt.cpp`, `RatwRoads.cpp`...). Commit only your own hunks. What worked: a clean `git worktree` at HEAD, your
  files and hunks copied in (hand edits where both sessions changed one hunk), built and tested there, committed
  there, then `git update-ref` the branch to it and `git reset` (mixed) in the main tree, and push.
- **Uncommitted, deliberately:** `Core/RatwMagic.cpp` in the working tree uses the money session's `Society::tillOf`
  (a workshop's till) where the commit uses the keeper's own id; it goes in with their commit.
- **Stale objects:** when another session edits a header mid-build, objects can mismatch (seen as `free(): invalid
  size`). `cmake --build build-gifts --clean-first` fixes it.
- **Pointers into `Battle::fighters`** go stale when a fighter is added (the vector moves). Fetch afresh after any
  `enterBattle` or `push_back`; two such bugs were found this session.
- Build in `build-gifts` (yours), not `build-core` (the launch scripts' and other sessions'). A scratch server
  (`tools/scratch.sh town`, CLAUDE.md) rebuilds `build-core`; stop it when done. Never start the real server unasked;
  the DEV server on 7788 was running an older build at the end of this session.
- Fights are deterministic per id and sequence (`chance` hashes them), so vary the wolves' ids per trial, and
  alternate who starts and who acts first in a tick, or the results lean (two-thirds) to whoever's script ran first.

## Done (2026-10-05)

### Decisions (the user, 2026-10-05)

- **Targets:** a Quickened wolf against a plain wolf of its level wins **70–80%** (stronger than this doc's proposed
  60–70%: it should feel frightening). A Gifted wolf alone: 50–55%. A Gifted partner in a pair: 55–65%. Families within
  about ±7 points of each other.
- **Fewer casts, not smaller ones:** keep the big hits, make them rare. (Some damage had to come down as well: see
  below.)
- **In range both bare and armed** (a sword and a leather kit on both sides), not bare alone.
- **The dice:** fix them, and bring the level feel back (+1.5 fighting skill a level: doc 44).
- **Gifted help takes either the turn's move or its action** (the user's rule).

### The simulator (`Tests/level_sim.cpp`)

- Wolves have a Gift (`giveGift`, full mana; a Water wolf carries a waterskin). Fights run in parallel threads (about
  25 s for 48 rows of 300 fights) and start **5 tiles apart**: nearer or farther, whoever waited for the other to walk
  in won about 65% (at 2, 4 or 6 tiles), and at 5 neither does. Which side stands west alternates too, since the town
  ground differs.
- **Smart play**, per family: a Quickened wolf opens with its best cast in reach, gathers only when no foe can reach and
  strike it before it goes off (it reads the bars), never overreaches, hangs back for its moment only when alone, and
  in a team casts from afar only when it couldn't reach a foe that turn. A Gifted wolf helps where it is free (beside a
  foe already: the move), else walks up and helps with the action only when it matters (Lift Up, Cauterize a bleeding
  that would down someone, Douse); it keeps breath for its blows. **Naive play:** whatever is ready.
- Suites: `levels`, `core` (the yardsticks), `wide`, `worth`, `gifts`, `naive`; `duel <side> <side> [level] [gap]`
  for one row. `SIM_DETAILS` lists the abilities used, `SIM_TRACE` prints a fight.
- **What a head start is worth** (`worth`, L10): 10 health ahead wins 71% bare, 63% armed; 20 ahead 85% / 68%; 30
  ahead 94% / 78%. A sword blow is a fifth of a wolf, so armed fights swing more and an edge counts for less; an edge
  that takes the foe's actions away (a lost turn, a pushed-back bar, a miss) counts for more armed, as it should.

### What was wrong

- **The fight's dice** (`chance`: a plain sum of the salt's hash and the fight's sequence) moved only about 3% a round
  when the fight's log grew by the same lines each round, so a wolf rolled the same zone and nearly the same odds round
  after round. It inflated every edge: the first blow's 66% was mostly that. Now mixed (splitmix64's finish). The same
  pattern is still in `RatwCareers.cpp` and `RatwCrime.cpp` (not fights; left alone).
- **Round 0** (old dice, smart play, 6 tiles): every Quickened family 98–100% against a plain equal except Blinker
  (72%), and a Quickened L1 beat a plain L25 92–100%. Riposte cost nothing up front and cancelled every blow; Stone
  Armor took a flat 5 off a bite (40%); one Flamethrower (45 and Burning) was five bites in one action; with a Quickened
  Fire wolf on each side of a pair, whoever flamed first won outright.
- **Gifted help didn't pay**: it took the turn's action and the bar's head start for not acting, so a smart wolf
  mostly didn't use it (Fire, Earth, Water, Seer, Wind and Sound partners 51–54%, a plain partner's), while Slip (a
  reaction, no action), Lift Up (about four times a fight) and Air Blast (the whole fight) were far too strong.

### What changed

Rules:
- The dice mixed (above); **+1.5 fighting skill a level** (doc 44).
- **A Quickened wolf's mana doesn't come back in a fight** (`battle::QuickenedManaPerTurn` 0; a Gifted wolf's still +2
  a turn). With today's pool (about 44) each family gets one signature cast a fight, or two small ones.
- **A Gifted wolf's help takes the move while it hasn't moved, else the action** (`useGift`; `giftWhyNot`). A move spent
  on help keeps the bar's head start (`Magic::helped`): it hasn't stepped. A Gifted NPC can still strike after helping.
- Forewarn and Firm Footing are for others only. Lift Up once for each ally a fight. Steady Beat isn't broken by blows
  and speeds the rest of the side, not the drummer.

Numbers (bases at WIS 30 are × 0.8; the second pass, below, changed several again):

| Family | Quickened | Gifted |
| --- | --- | --- |
| Fire | Flamethrower 45 → 26 and a 25% bar knock-back; Heat Lance 30 → 24; Blastwave 20 → 16 | Flare knocks the bar back 35% (was 20), no health cost, 3 stamina |
| Earth | Stone Armor: 15% off every blow (was 5 flat), bar 5% slower (was 15%), no lost tile; Hurl Stone 40 → 20; Upheaval 25 → 18 | Firm Footing: 10% harder to hit (20% on guard), not only on guard |
| Water | Pressure Jet 28 → 12, and water in the eyes (−12% to the next blow); the Cost is thirst, not dehydration | Splash Eyes −20% → −12%, 7 → 10 mana |
| Wind | Battering Gust: armour doesn't take it, crash 10 → 12, 5 stamina (was 10); Whirlwind 6 stamina (was 12) | Air Blast: 2 turns (was the fight), 8 → 18 mana |
| Sound | Shatterhowl 22 → 24, bars back 40% (was 20); Resonance shakes a sword loose 40% (was 25) | Steady Beat +15% (was 10), others only, kept through blows |
| Blinker | Blink Strike 15 → 25 mana | Slip 10 → 20 mana, rests 6 (was 3); Interpose 12 → 24 mana, rests 6, no guard |
| Gravity | Slam 40 → 22 (the early drop 20 → 12) | Lift Up 12 → 22 mana, once for each ally |
| Seer | Riposte 24 to take up (was free); Critical Sight 15% → 6% | Forewarn +25% → +30%, others only |

### Where the first pass stood (smart play, L10, 5 tiles apart, 300 fights; superseded by the second pass below)

| Quickened vs a plain equal | Fire | Earth | Water | Wind | Sound | Blinker | Gravity | Seer |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Bare | 79% | 84% | 83% | 76% | 71% | 82% | 71% | 74% |
| Armed | 70% | 70% | 64% | 68% | 64% | 72% | 79% | 78% |
| One of a pair, vs a plain pair (bare) | 65% | 60% | 64% | 57% | 64% | 78% | 56% | 59% |

| Gifted | Fire | Earth | Water | Wind | Sound | Blinker | Gravity | Seer |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Alone, bare / armed | 51 / 58% | 48 / 51% | 60 / 59% | 62 / 60% | 52 / 51% | 50 / 60% | 48 / 51% | 48 / 51% |
| A partner in a pair, bare / armed (plain pair 51%) | 56 / 48% | 58 / 51% | 54 / 50% | 57 / 56% | 54 / 45% | 58 / 68% | 66 / 66% | 58 / 58% |
| One of three, bare (plain three 51%) | 55% | 66% | 55% | 51% | 60% | 37% | 70% | 62% |

Wider (`wide`, `naive`):
- **Levels against Gifts:** a Quickened L1 beats a plain L10 53–71% and loses to a plain L25 (27–48%): a Quickened
  Gift is worth about ten levels. One Quickened wolf never beats two plain equals.
- **Quickened against Quickened** (bare): mostly 40–70%, but Blinker–Seer 15/85 (Riposte answers Blink Strike),
  Earth–Water 86/14, Fire–Wind 26/74, Water–Wind 28/72, Sound–Blinker 29/71.
- **Naive play:** a Quickened wolf pressing every ready Gift still wins 52–73% (Seer 30%); a Gifted one loses nearly
  every fight (0–16%), spending its move, action and breath on help that does little.

### Open

- The Quickened-against-Quickened outliers above (rock-paper-scissors is fine; 85/15 is a foregone conclusion).
- Gifted Blinker in a team of three (37%): Interpose and Slip leave it dizzy (easier to hit, worse at hitting); Gifted
  Sound and Fire partners armed (45–48%); Quickened Water and Blinker in a team of three (44%, 49%).
- A Gifted Earth wolf has no fight Gift in paw armour (its Tell: bare paws), so armed it is a plain wolf. As designed.
- Naive Gifted play is punished hard. Worth a hint in the Gift row (what the help costs) or softer Costs.
- Players' fighting skill now reaches 86 against NPC trades' 25–55: high-level players will find NPC fights much
  easier. Check bandits, the watch and the hunts.
- Striking first is worth little now, and a leather kit less than levels (doc 44's re-measured table): gear's own
  balance.
- The simulated plain wolf doesn't guard, doesn't go for a wolf gathering a Gift, and fights on a town map: real players
  will do better against Tells, so gathered Gifts may play weaker than here.
- Quickened mana pools themselves are unchanged (the stats pass); the gift-damage power scale (0.5 + WIS/100) too.
- Tests: `magic_tests` (172 checks; new: help takes the move and keeps the head start, then the action; Lift Up once;
  Steady Beat through a bite; Firm Footing without guard and not on oneself; Forewarn not on oneself; Quickened mana
  kept), `battle_tests` and `level_tests` (86 at 25), `crime_tests` (a guard who struggled up mid-fight isn't "down
  twice"). All 45 tests pass.

## Second pass (2026-10-05)

### The user's asks

- Every Gifted wolf as a partner at **55–65%**, bare and armed (Sound and Fire fell short).
- A Quickened wolf's fighting skill climbs to **100** at level 25 (others stay at 86).
- Quickened against Quickened: lopsided is fine as **hard counters that make sense**; no power without a counter unless
  it isn't strong overall.
- Apply the fixes found, keep iterating.

### The simulator, made fairer

- **Every wolf ends its turn facing the nearest foe** (turning is free). Before, a wolf never turned, so one that got
  behind it (a Blink Strike, a throw) kept the +20% from behind all fight: most of the Quickened Blinker's edge.
- **A wolf goes for a foe gathering a Gift** when it can reach and strike it that turn (breaking its Tell).
- Line Gifts check the exact tiles for a friend first (a jet had been hitting its own partner); in a team a wolf doesn't
  shove or throw a foe off a partner who is fighting it; a Gravity wolf lifts a foe that has spent its turn.
- A Gifted Earth wolf goes without paw armour (its Gift needs bare paws). `SIM_SKIP=<abilities>` leaves abilities
  unused, to see what each is worth; suites `gifted2`, `quick2` (with pairs) and `qvq` (every matchup, bare and armed).

### What changed

Rules:
- **Quickened fighting skill:** 50 + 50/24 a level, 100 at 25 (`levels::fightingSkill(level, quickened)`).
- **Counters:** Resonance cracks Stone Armor off (Sound answers Earth). Grit in a Seer's eyes (Whirlwind, Air Blast)
  stops its Riposte and lets it be flanked (Wind answers Seer). Stone Armor is too heavy to shove, throw or knock down,
  and takes half a gust (Earth answers Wind; `Magic::steady()`). A pinned Blinker (held, crushed or frozen) can't blink.
- **Slip** saves only if the hop takes the wolf out of the weapon's reach (a sword's 2 tiles follow a one-tile hop);
  Slip and Interpose share one rest (8 turns); after either the Blinker is only *blink-dazed* (−10% to hit), not dizzy.
- **Cauterize** sears the wound (no bleeding for 3 turns) without setting the bar back. **Lift Up** stands the ally with
  5 health, dazed for its next turn. **Thirst** (Water's Cost) takes a quarter of a turn's stamina back, not half.
- **Blink Strike:** the nausea comes after the blow, not before it.
- **Whirlwind's grit lasts 3 turns** (it was the fight): it made Wind the strongest against every other Quickened.
- Found on the way: a sword's blow reaches the Gift rules as "blade", but Slip and Seen Opening looked for "sword", so
  Seen Opening judged a sword's armour as a bite's.

Numbers:

| Family | Quickened | Gifted |
| --- | --- | --- |
| Fire | Flamethrower 26 → 20 | Flare: 20% bar knock-back (was 35), 8 mana; Cauterize: 3 stamina |
| Earth | Hurl Stone 20 → 17; Stone Armor 15% → 12% | — |
| Water | Pressure Jet 12 (unchanged after trying 10) | Splash Eyes −13%, range 3 (was adjacent) |
| Wind | Gust: armour takes half | — |
| Sound | Resonance sheds a sword 50% (was 40) | Steady Beat +30% (was 15) |
| Blinker | Blink Strike 25 → 18 mana | (rules above) |
| Gravity | Slam 22 → 12 | (rules above) |
| Seer | Critical Sight 6% → 3% | — |

### Where it stands (smart play, L10, 5 tiles apart; Gifted 1,000 fights a row, about ±1.6; Quickened 600 / 400)

| Gifted | Fire | Earth | Water | Wind | Sound | Blinker | Gravity | Seer |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Partner in a pair, bare (plain pair 51%) | 59% | 60% | 57% | 59% | 57% | 56% | 60% | 61% |
| Partner in a pair, armed (plain pair 50%) | 56% | 59% | 54% | 56% | 54% | 57% | 63% | 57% |
| Alone, bare / armed | 58 / 60% | 48 / 58% | 63 / 61% | 62 / 60% | 50 / 52% | 65 / 52% | 48 / 52% | 48 / 52% |
| One of three, bare (plain three 51%) | 62% | 66% | 55% | 51% | 72% | 47% | 65% | 62% |

| Quickened | Fire | Earth | Water | Wind | Sound | Blinker | Gravity | Seer |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| vs a plain equal, bare | 82% | 77% | 76% | 72% | 77% | 69% | 79% | 74% |
| vs a plain equal, armed | 70% | 71% | 70% | 72% | 68% | 67% | 70% | 80% |
| One of a pair, bare | 69% | 66% | 61% | 69% | 72% | 83% | 67% | 62% |
| L1 vs a plain L10 / L25 | 61 / 35% | 57 / 31% | 56 / 26% | 56 / 30% | 53 / 27% | 45 / 18% | 55 / 28% | 53 / 25% |

Quickened against Quickened (row's wins; bare / armed):

| | Fire | Earth | Water | Wind | Sound | Blinker | Gravity | Seer | Counter |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Fire | — | 54/47 | 38/38 | 22/18 | 43/52 | 62/54 | 67/64 | 56/50 | Wind steals its breath; Water |
| Earth | 46/53 | — | 67/77 | 60/74 | 15/20 | 75/66 | 63/70 | 55/45 | Sound cracks the stone |
| Water | 62/62 | 33/23 | — | 28/20 | 33/44 | 66/40 | 60/54 | 50/51 | Earth dams it; Wind |
| Wind | 78/82 | 40/26 | 72/80 | — | 71/68 | 36/48 | 52/71 | 62/61 | Earth (stone won't move); Blinker bare |
| Sound | 57/48 | 85/80 | 67/56 | 29/32 | — | 62/50 | 75/65 | 49/71 | Wind steals its breath |
| Blinker | 38/46 | 25/34 | 34/60 | 64/52 | 38/50 | — | 54/55 | 35/35 | Earth (no critical: no "from behind"); Seer |
| Gravity | 33/36 | 37/30 | 40/46 | 48/29 | 25/35 | 46/45 | — | 52/60 | Sound hears its tapping feet; Earth |
| Seer | 44/50 | 45/55 | 50/49 | 38/39 | 51/29 | 65/65 | 48/40 | — | Wind's grit |

Every family has a counter at 38% or worse bare and armed, and each makes sense. Averages against the others run
40–61% bare (Gravity 40, Blinker 41 at the bottom; Sound 61, Wind 59 at the top) and 40–62% armed.

### Open

- Gravity and Blinker are the weakest against other Quickened (about 40%), though in range against plain wolves.
- A Gifted Sound wolf in a team of three (72%: the beat speeds two friends) and a Gifted Blinker there (47%).
- A Quickened Blinker in a pair (83%): blinking behind a foe its partner is fighting is easy flanking.
- Naive Gifted play still loses almost every fight (it spends move and action on help every turn and never bites).
- Alone, the support-only Gifted (Earth, Gravity, Seer, Sound) are a plain wolf (48–52%): their help is for others.
- Tests: `magic_tests` 194 checks (the counters, the pinned Blinker, the sword past Slip, seared wounds, Lift Up's daze),
  `level_tests` (a Quickened wolf's 100). All 45 pass.
