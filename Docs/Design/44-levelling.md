# 44. Levelling: how fast, from what, and what a level does in a fight

Agreed with the user 2026-10-04. Every number is a placeholder for play-testing.

The user's brief: player characters level by roleplay, work and practice as much as by fighting, with no grinding;
a maximum around 25 is imagined; a level should help only a little in a fight, so a higher-level wolf can be fought,
and tactics, gear, Gifts and numbers decide fights more than levels or level-stacked teams.

## What exists (2026-10-04)

- **A level is social** (doc 08): 1 + floor(XP / 100). XP comes only from roleplay scenes (20 a qualifying scene, less
  for later contributors and repeated partners) and fights (10, more if talked through), with Gold and Story Stars;
  at most 100 a rolling day. A daily roleplayer gains about a level a day.
- **A level does nothing in a fight.** What does: STR, DEX (and WIS for Gifts), fighting skill, gear, injuries.
- **Fighting skill** (0–100; players start at 50, NPCs by trade): each point over a foe makes one's blows 0.3% likelier
  to land and theirs 0.3% likelier to miss. A player's grew by fighting: +0.2 a blow landed and +0.5 a fight, slower as
  it climbs, about 1.5 a fight with no limit.
- **Birthdays** (doc 14): +1 STR and +1 DEX a game year to 34, +1 WIS after. A game year is about 61 real days.

## What the simulator showed

`Tests/level_sim.cpp` runs duels between raw wolves (no Gift, no gear: both close in and bite until one goes down) on
the real fight rules, hundreds at a time.

- A raw-wolf fight is a race of about 12 bites each; the winner usually has about 9 health left. Small edges compound.
- **The first blow wins 66–67% between equals.**
- +1 STR and +1 DEX a level: a 5-level gap wins 87%, a 10-level gap 96%. Far too much.
- **Practice outweighed everything:** twenty fights (+25 fighting skill) won about 97% against a fresh wolf.
- Birthdays alone (+6 STR and DEX in a year of play) were worth about as much as a whole level curve.
- The gentlest curve, **+0.5 fighting skill a level**:

| Gap | 1 level | 5 levels | 10 levels | L25 vs L1 | Lower strikes first: 5 levels | 10 levels |
| --- | --- | --- | --- | --- | --- | --- |
| Higher wins | 53% | 59% | 67% | 90% | 46% | 56% |

So striking first is worth about seven or eight levels.

## Decisions

1. **What a level does in a fight: +1.5 fighting skill, and nothing else** (doc 45; it was +0.5). A player's fighting
   skill is 50 + 1.5 × (level − 1): 50 at level 1, 86 at 25. It no longer grows by fighting. NPCs keep their trade's.
   The simulator's tables above were measured with the fight's dice badly mixed (doc 45): a roll moved only a little
   round to round, so fights were streaky and every edge counted for more. Mixed properly, +0.5 a level made L25 vs L1
   only 62%; +1.5 brings the level rows back (L25 vs L1 88%). **A Quickened wolf's** climbs to 100 at 25 (the user,
   doc 45: 50 + 50/24 a level).
2. **Fighting is practice like any other:** a fight earns XP (as now), into the same pool as everything else, under
   the same daily cap and the same lower pay for the same partner again. Fighting isn't punished, and grinding it
   doesn't pay.
3. **No more stat gains on birthdays.** Age still slows the old (doc 14). Gains already had are kept.
4. **The pace:** going from level L to L + 1 costs **100 + 50 × (L − 1)** XP. No hard cap: past 25 a level still
   costs more each time but adds nothing more in a fight (the fighting skill stops at level 25's 86).

| Level | 2 | 3 | 5 | 10 | 15 | 20 | 25 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Total XP | 100 | 250 | 700 | 2,700 | 5,950 | 10,450 | 16,200 |
| A casual player (about 80 XP a session, 4 a week) | a session | 3 sessions | 2 weeks | 2 months | 4½ months | 8 months | a year |

5. **Where XP comes from** (each a typed receipt in the social ledger, never a generic "give XP": doc 08):

| Kind | Pays | When | Limit |
| --- | --- | --- | --- |
| Scene | 20 (as doc 08) | A roleplay scene settled | doc 08's partner decay |
| Fight | 10, more talked through (doc 33) | A fight settled | as scenes |
| Work | 10 | A Gift lent to a workshop (doc 43); an apprentice at their master's work, once a game day | 30 a day |
| Practice | 5 | The first forage, hunt (a kill) and repair of each game day | 15 a day |
| Milestone | 10 | Sneak, listening or tracking reaching 25, 50, 75 (once each) | — |
| Discovery | 5 | The first time in a place (a cell) | 25 a day |
| Story | 25 | A contract fulfilled | — |

   **The daily cap is 150** (a rolling real day, as now), across every kind.

6. **Rested XP:** a player back after a day or more without earning has a rested pool of 50 a day away (300 at most);
   while it lasts, everything earned is paid again from it (doubled), outside the daily cap. A casual player isn't
   left behind by an everyday one.
7. **Titles** (doc 32, 1.3) keep their levels (Known 3, Familiar Face 5, Respected 8, Notable 12) and gain Renowned
   (18) and Legend (25). Level gates (a companion at 3, founding a Chapter) keep their levels, so take a little longer.

## What decides a fight, then

Tactics (striking first, from the side +10% or behind +20%, guarding, ground), gear (doc 35's armour and weapons),
Gifts (doc 43) and numbers. The simulator is kept to check that they keep outweighing levels as they change.

## Built (2026-10-04)

- **The curve and the fight** (`Core/RatwLevels.h`): `levels::stepCost`, `xpFor`, `levelFor` (no cap) and
  `fightingSkill` (50 + 1.5 a level to 86 at 25, doc 45; it was 0.5 to 62). `SocialLedger::level` uses it; `World::levelOf` (set by the game to
  the ledger's level) gives a player's fighting skill in `World::temperamentOf`; the character sheet's FIGHTING is it.
  `World::growSkill` does nothing now (a player's saved `fightingSkill` is no longer read). NPCs keep their trade's.
- **Birthdays** (`advanceAge`) give no statistics; the age, notices and the old's slowing are as before.
- **XP from the world** (`Core/RatwProgress.cpp`): `World::award(who, kind, source)` queues a typed award, sent again
  at most every ten minutes; the game pays them (`SocialLedger::award`) after the world's notices, says
  "+N experience (...)" and "You reach level N.", and saves. Sources:
  - work: a Gift lent to a workshop (`lend:<maker>:<day>`); an apprentice within 8 paces of their master
    (`apprentice:<day>`, once a game day);
  - practice: the first forage, hunt kill and repair of each game day (`forage:`, `hunt:`, `repair:<day>`);
  - milestones: sneak, listening and tracking reaching 25, 50, 75;
  - discovery: each place (cell) the first time;
  - story: a contract fulfilled for a player (`settleContract`).
- **The ledger** (`Core/RatwSocialCore.cpp`): `award` (the amounts and per-kind day limits above; once for each
  source; -1, with no receipt, when the day's limits leave nothing, so it is paid another day); the daily cap 150 for
  everything (`DailyCap`); rested XP (`restedLeft`: 50 a day of the latest gap of a day or more between earnings, 300
  at most, less what has been paid from it since; `pay` adds it as a `rested_bonus` receipt outside the cap). Titles
  Renowned (18) and Legend (25).
- Tests: `Tests/level_tests.cpp` (the curve, the fighting skill, every award kind, its limits, once each, the cap and
  its waiting, rested XP); `battle_tests` (fighting doesn't raise fighting skill; level 25 is 86); `aging_tests` (no
  stats on birthdays); `social_game_tests` (Bo's 5 for a place first visited).
- A bug found on the way: the Dev Console's team test fight (`World::testFightTeam`) kept pointers into the fight's
  fighters while adding more to them, so its allies could be placed by freed memory (15 tiles off). It keeps their
  tiles now.
- **The simulator** (`Tests/level_sim.cpp`, `build-gifts/level_sim [fights]`), ungifted wolves on the real rules, 300
  fights each, starts alternated unless said:

| Fight | First named wins |
| --- | --- |
| L5 vs L1 / L10 vs L1 / L25 vs L1 | 59% / 66% / 86% |
| L10 vs L5 / L25 vs L20 / L25 vs L15 | 60% / 60% / 66% |
| L1 vs L1, L5, L10, L25, the L1 striking first | 69%, 65%, 54%, 21% |
| L1 in a leather kit vs L25 bare | 68% |
| L1 with a sword vs L25 bare | 98% |
| L1 with a sword and leather vs L25 the same | 41% |
| Two L1 vs one L25 / two L5 vs one L15 | 100% / 100% |
| Two L1 vs two L25 / three L1 vs two L25 | 16% / 91% |

  Levels count for a little; striking first, gear and numbers for much more. The sword (about 20 a blow and reach
  2, against a bite's 12) decides a fight between raw wolves almost alone: a matter for the gear's own balance.

## Re-measured (2026-10-05, doc 45)

The tables above were measured with the fight's dice badly mixed (`chance` in RatwBattle.cpp and RatwMagic.cpp: a
plain sum, so a roll moved only about 3% when the fight's log grew by the same lines each round, and a wolf rolled the
same hit zone and nearly the same odds round after round). Mixed properly (splitmix64's finish), with **+1.5 fighting
skill a level** (the user's choice: bring the level feel back), 600 fights each:

| Fight | First named wins | Before |
| --- | --- | --- |
| L5 / L10 / L25 vs L1 | 55% / 64% / 88% | 59% / 66% / 86% |
| L10 vs L5 / L25 vs L20 / L25 vs L15 | 56% / 56% / 65% | 60% / 60% / 66% |
| L1 vs L1, L5, L10, L25, the L1 striking first | 52%, 46%, 36%, 13% | 69%, 65%, 54%, 21% |
| L1 in a leather kit vs L25 bare | 25% | 68% |
| L1 with a sword vs L25 bare | 78% | 98% |
| L1 with a sword and leather vs L25 the same | 19% | 41% |
| Two L1 vs one L25 / two L5 vs one L15 | 100% / 100% | 100% / 100% |
| Two L1 vs two L25 / three L1 vs two L25 | 6% / 98% | 16% / 91% |

The level rows are back. Striking first is worth little now (about 52%: it was the streaky dice), and a leather kit
counts for less than levels; both are open (doc 45).

**With the gear tiers (doc 47, 2026-10-06; tactics, 11 tiles apart, 400 fights):** an L1 against a bare L25 wins 30%
with a bronze blade, 43% with iron, 58% with steel; 15%, 21% and 31% in a cloth, leather or steel kit; 41% with bronze and
cloth, 77% with steel and the steel kit. So a tier is worth a little more than 24 levels, and no longer the whole fight
(the old sword alone won 98%).
