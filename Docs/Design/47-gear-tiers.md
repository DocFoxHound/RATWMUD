# 47. Weapon and armour tiers

Built and tuned 2026-10-06. Read docs 33 (combat), 35 (items: Part 4's qualities, 2.1's weapons and armour), 44
(levelling) and 45 (Gift balance) first.

## The ask

> "Develop tiers for weapons and armor and start scaling their stats. [...] three tiers for each. These are not
> quality tiers, but just three different "good" tiers of weapons and armor: Basic. Professional. Exceptional. Basic
> blades should be: bronze, iron, steel. For armor it should be: cloth, leather, steel. They should have scaling damage
> and armor ratings [...] Run tests without weapons and with armor only in 1v1 up to 5v5 fights, then run tests with
> weapons only and no armor the same way. Then start scaling up through the tiers and finding a happy balance for the
> stats. My goal, here, isn't to make fights end too quickly, this is a roleplay game after all, but weapons should
> have a significant advantage over unarmed combat."

Read as: blades come in three metals, a tier each (bronze Basic, iron Professional, steel Exceptional), and armour in
three kits (cloth, leather, steel). A tier is not a quality: a bronze sword is Basic however well it is made.

## Targets (set here; the user's two rules turned into numbers)

All at equal levels (L10), one a side, both playing smart (the simulator's tactics), 11 tiles apart:

- **A blade is a significant advantage:** a blade against bare teeth wins about 70–75% (Basic), 80% (Professional),
  90% (Exceptional).
- **Armour helps, less than a blade:** a kit against a bare wolf about 60–65%, 70%, 75%.
- **Fights don't end too quickly:** two wolves geared at the same tier (blade and kit) fight about as long as two bare
  wolves (at least 0.85 times the rounds). A blade alone shortens a fight; its tier's armour gives the time back.
- **A tier is a step:** each tier beats the one below, evenly up the ladder.
- **A blade always beats biting**, even into the best armour, and the best armour holds a Basic blade off.

## What was there

- One sword in a fight: the id `sword`, named "Dull bronze sword" in the game, at a fixed 20 a blow, turn weight 10 and
  14 stamina (`battle::SwordDamage`). The catalog's iron sword (16), rapier, axe, mace and the rest weren't usable in
  a fight; the catalog called `sword` a steel blade while crafts made it from bronze.
- Armour by hit zone (doc 33), whole numbers: a leather kit (barding 3, gorget 2, cap 1, leg guards 2), the steel pieces
  at 3–6. Only a quilted vest for cloth.
- The baseline (below): a bronze sword won 93% against bare teeth and halved a fight's length; a steel kit won 94%.

## The baseline (2026-10-06, before tuning)

L10, tactics, 11 apart, 400 fights a row; mirror fights within 2–3 points of 50%. Rounds are turns each.

| | 1v1 | 2v2 | 3v3 | 4v4 | 5v5 | rounds 1v1 (bare 11.7) |
| --- | --- | --- | --- | --- | --- | --- |
| Cloth (vest 2, the rest 1) vs bare | 66% | 77% | 80% | 83% | 83% | 13.1 mirror |
| Leather kit vs bare | 75% | 86% | 91% | 93% | 95% | 14.1 mirror |
| Steel kit vs bare | 94% | 99% | 100% | 100% | 100% | 18.4 mirror (41 at 5v5) |
| Bronze sword (20) vs bare | 93% | 98% | 99% | 99% | 100% | 6.3 mirror |
| Iron sword (16) vs bare | 74% | 82% | 87% | 86% | 91% | 8.5 mirror |
| Steel sword (24, a first guess) vs bare | 99% | 100% | 100% | 100% | 100% | 5.1 mirror |

Iron at 16 against bronze at 20 shows the cliff: a blow of 18 bled, every time, and below it never.

## What was built

- **The catalog** (`Data/Items/items.json`): `tier` (basic, professional, exceptional) on every blade and piece of
  armour; `weapon.class: "blade"` on the three a fight takes. `items::Item` has `tier` and a `Weapon` (damage, stamina,
  turn weight, reach, type, pierce, hit bonus, knock-loose); `items::blade(id)` and `items::tierName`.
  - `sword` is the **Bronze bit-sword** (the game's "Dull bronze sword"); `iron_sword`; a new `steel_sword` (armoury
    only: crafts and a recipe from a steel bar).
  - Cloth for a whole kit: new `padded_collar`, `quilted_hood`, `quilted_leggings` (tailor and armoury).
- **A fight reads the blade** (`World::bladeHeld`): its blow, stamina, turn weight, kind of blow, pierce and hit bonus,
  in `swordStrike`, the NPCs' choice to swing, a Riposte, the knock-loose chance and the strike preview. Every blade
  reaches 2 (`battle::SwordReach`). Any blade can be taken up; "hold sword" takes the best one has (its blow, make
  counted), or one named. The inventory marks blades (`blade`) and shows each piece's tier; the client's sword button
  works for any blade.
- **Armour takes fractions** (`protect`, `vsCut`… are doubles), so a quality can scale it smoothly; views round to a
  tenth.
- **Quality within a tier** (`items::qualityDamage`, `qualityGuard`): a blade's blow crude 0.92, fine 1.05, masterwork
  1.1 (was 0.85 / 1.15 / 1.3: a fine bronze sword had beaten a common one 73%, more than a tier's step, and a
  masterwork bronze beat common steel 68%); armour's protection 0.75 / 1.25 / 1.5 of every figure, no longer rounded
  with "at least +1" (which had made fine cloth a match for leather). A masterwork is now about the next tier up.
- **Bleeding is a chance** (doc 38): from a blow of 14, likelier the harder, sure from 22 (`battle::BleedFrom`,
  `BleedSure`). Bites (at most about 14) almost never bleed, as before.
- **The simulator** (`Tests/level_sim.cpp`): wolves carry a blade and a kit by name ("plain+iron+leather",
  "plain+steel~fine+plate"); suites `tiers` (all of these), `armour`, `weapons`, `ladders`, `cross`; rounds per wolf
  in each row; `SIM_ARMED` picks doc 45's armed kit (iron and leather by default).
  - **A bias fixed:** side B always stood 11 tiles off the spot both sides came in, where the town's walls are, and lost
    team fights it should have drawn (bare 5v5: 72% to side A). Which side stands where now alternates; mirror fights
    are 46–51% at every size.

## The numbers

| Blade | Tier | Damage | Stamina | Turn weight | Pierce | Durability |
| --- | --- | --- | --- | --- | --- | --- |
| Bronze bit-sword | Basic | 14 | 10 | 4 | 0 | 200 |
| Iron bit-sword | Professional | 15 | 10 | 3 | 0 | 200 |
| Steel bit-sword | Exceptional | 16 | 9 | 2 | 0 | 300 |

Each metal cuts a point harder and swings lighter. (A bite: 12, stamina 8, no weight.) Lighter swings matter as much
as the damage: a blade at a bite's 12 with the old 14 stamina and weight 10 lost 74% to bare teeth; at 11 and 6 it
broke even.

| Kit | Tier | Body | Throat | Head | Legs | DEX |
| --- | --- | --- | --- | --- | --- | --- |
| Cloth: quilted vest, padded collar, quilted hood, quilted leg wraps | Basic | 1 | 1 | 1 | 1 | 0 |
| Leather: barding, gorget, cap, leg guards | Professional | 2 | 2 | 1 | 1 | −1 |
| Steel: mail coat, steel gorget, kettle helm, splinted greaves | Exceptional | 3 | 3 | 2 | 2 | −8 |

Every piece also takes 1 more off a cut. The others: boiled-leather barding 3 (+1 cut, DEX −2), spiked collar 2,
leather neck wrap 1 (Professional); brigandine coat 3 (+1 against a bite instead, DEX −5), mail neck guard 2 (+2 cut),
chamfron 2 (+2 cut) (Exceptional).

## Where it stands (L10, tactics, 11 apart, 400 fights a row)

Each kind alone, against bare wolves:

| | 1v1 | 2v2 | 3v3 | 4v4 | 5v5 |
| --- | --- | --- | --- | --- | --- |
| Cloth vs bare | 63% | 70% | 75% | 75% | 73% |
| Leather vs bare | 69% | 77% | 83% | 85% | 89% |
| Steel kit vs bare | 74% | 87% | 90% | 90% | 90% |
| Bronze vs bare | 71% | 82% | 83% | 83% | 92% |
| Iron vs bare | 81% | 91% | 93% | 91% | 98% |
| Steel vs bare | 89% | 96% | 98% | 97% | 100% |

Fight lengths (rounds each; mirror fights):

| | 1v1 | 3v3 | 5v5 |
| --- | --- | --- | --- |
| Bare | 11.7 | 19.5 | 26.0 |
| Cloth / leather / steel kit, no blades | 12.8 / 13.5 / 15.0 | 22.2 / 23.9 / 26.9 | 27.2 / 28.3 / 32.4 |
| Bronze / iron / steel, no armour | 9.5 / 8.7 / 8.1 | 22.1 / 17.6 / 17.3 | 21.1 / 18.2 / 17.2 |
| Matched kits: bronze+cloth / iron+leather / steel+steel | 11.6 / 11.2 / 11.2 | 26.4 / 25.7 / 23.4 | |

The tiers against each other:

| | 1v1 | 3v3 |
| --- | --- | --- |
| Iron vs bronze; steel vs iron; steel vs bronze (blades only) | 66%; 67%; 76% | 68%; 70%; 84% |
| Leather vs cloth; steel vs leather; steel vs cloth (kits only) | 58%; 57%; 63% | 64%; 59%; 75% |
| Professional kit vs Basic; Exceptional vs Professional; Exceptional vs Basic | 66%; 66%; 84% | 80%; 74%; 92% |
| A full kit vs bare: Basic / Professional / Exceptional | 82% / 91% / 97% | 95% / 99% / 100% |

A blade alone against armour alone, one a side (the blade's wins):

| | Cloth | Leather | Steel kit |
| --- | --- | --- | --- |
| Bronze | 47% | 41% | 34% |
| Iron | 59% | 56% | 49% |
| Steel | 73% | 69% | 65% |

A blade into armour still beats biting into it: two wolves in steel kits, one with a bronze blade, the blade wins 63%.

Gear against levels (L1 geared against a bare L25 unless said; a bare L25 beats a bare L1 86%):

| L1 with | Wins |
| --- | --- |
| A bronze / iron / steel blade | 30% / 43% / 58% |
| A cloth / leather / steel kit | 15% / 21% / 31% |
| Bronze and cloth / steel and the steel kit | 41% / 77% |
| Bronze and cloth, against an L25 the same | 12% |
| Iron and leather against an L25 with bronze and cloth; steel and steel against an L25 with iron and leather | 28%; 28% |

Before this a bronze sword alone let an L1 beat an L25 98% of the time; now levels and gear both count. A tier is worth
a little more than 24 levels.

Quality against the tiers (1v1, 600 fights): a fine bronze sword against a common one 55%, a masterwork 65%; common
against crude 62%; a masterwork bronze against common iron 52%, a masterwork iron against common steel 51%; masterwork
bronze and cloth against the common Professional kit 59%. Masterwork leather against the common steel kit 53%,
masterwork cloth against common leather 47%.

## The Gifts, re-checked (doc 45's yardsticks)

Doc 45's "armed" was a sword (20) and the old leather kit. The tiers made blades softer and armour lighter, so a Gift is
a bigger share of an armed fight. "Armed" is now the Professional kit (iron and leather; `SIM_ARMED` for the others),
and five Gifts changed, each only where blades or armour come into it:

| Gift | Change | Why |
| --- | --- | --- |
| Flare (Gifted Fire) | reaches 2 tiles (was adjacent) | Armed foes fight from a blade's two tiles: armed partner 50% → 60% |
| Firm Footing (Gifted Earth) | 8% harder to hit, 16% on guard (was 10, 20) | A sword-fighter keeps its tile: armed partner 67% → 65% |
| Battering Gust (Quickened Wind) | armour takes it, as a jet (it took half) | Armed 85% → 82% |
| Riposte (Quickened Seer) | strikes back with what is in its jaws, through the attacker's armour (it was a bite, through nothing) | Armed 81–84% |
| Blink Strike (Quickened Blinker) | finds the least armoured spot behind, with no aim's cost, and lands a fifth harder | Armed 61% → 71%, bare 69% → 77% |

Where they stand (smart play, L10, 5 tiles apart, 600 fights a row; armed = iron and leather on every wolf):

| | Fire | Earth | Water | Wind | Sound | Blinker | Gravity | Seer |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Gifted partner in a pair, bare (target 55–65%) | 57% | 57% | 56% | 57% | 56% | 56% | 60% | 59% |
| Gifted partner in a pair, armed | 60% | 65% | 60% | 58% | 56% | 57% | 61% | 61% |
| Quickened vs a plain equal, bare (target 70–80%) | 82% | 77% | 76% | 72% | 77% | 77% | 79% | 74% |
| Quickened vs a plain equal, armed | 80% | 76% | 76% | 83% | 78% | 71% | 75% | 82% |

Before the five changes the armed Gifted Fire partner was 48–53% at every tier, Earth 62–68%, and the Quickened Wind
82–86%, Seer 81–84%, Blinker 61–68%. Wind and Seer armed, and Fire bare (as in doc 45), sit two or three points over.
The Basic and Exceptional kits gave the same picture as the Professional one (each within about 3 points).

Quickened against Quickened (doc 45's table, re-run; row's wins, bare / armed): every family keeps a counter at about
40% or worse, and the averages run 39–59% bare, 39–61% armed (Gravity lowest, Sound and Wind highest), as before. The
Blinker's average rose (41% to 48/51%) with its Blink Strike; a Seer armed now loses to Sound 18% (was 29%), its
Riposte no longer passing through armour.

The Trance (doc 45, third pass) was re-tuned once the simulator's sides stood on fair ground: at N = 10 a Quickened wolf
ten levels up wins 48–52% of its 3v1s (mean 50%) and 20–30% of its 5v1s (mean 24%).

## Second round (the user, 2026-10-06)

> "Fix the spiked collar bite-back, and [...] the bronze and iron sword pricing. [...] Adjust the values for quality.
> Give guards a full "Professional" set of gear. Make the Rapier an "Exceptional" level item. Remove mace and axe."
> And: "The rapier should be good against armored wolves, keeping its damage values better, but not necessarily be as
> strong as a steel sword. It's a specialty weapon."

- **The spiked collar bites back:** a bite that lands on its wearer's throat costs the biter 4 (`armor.spikes` in the
  catalog, `battle::spikesAt`; a quality scales it). Not a blade's blow.
- **Prices:** bronze sword 30p (was 40, and the shop's price for it was a number in `Society::quote`; it is the
  catalog's now), iron 45p (was 22), steel and rapier 90p. The bronze sword's recipe takes bronze (it said steel). The
  rest of weapon and armour pricing is for later.
- **Quality:** kept as above (crude 0.92, fine 1.05, masterwork 1.1 for a blade; 0.75, 1.25, 1.5 for armour).
- **The watch's kit** (`World::kitOut`): a resident whose job is a guard's wears the Professional kit (leather barding,
  gorget, cap, leg guards) and takes up an iron sword as it enters a fight, putting it away after. It is issued, not
  owned: never in the purse, so it can't be sold or looted, a sword knocked loose skids away for the fight (nothing to
  pick up), and what wears out is issued again. Checked each game hour; one who leaves the watch gives it back.
- **The needle rapier** is an Exceptional blade (`class: blade`): 14 a thrust, +5 to hit, 1 pierce, turn weight 2,
  stamina 9, harder to knock loose (0.7). A thrust misses a kit's extra against a cut, and the pierce takes a point
  more. In the open it is an iron sword's match, not steel's; into armour it is the best blade. A rapier lunges, stabs
  and pricks in the log.

| Rapier (L10, 1v1, 400 fights) | Wins | Steel sword, the same |
| --- | --- | --- |
| vs bare | 82% | 89% |
| vs an iron sword / a steel sword | 50% / 32% | 67% / — |
| vs a wolf in leather / in the steel kit (no blade) | 80% / 75% | 68% / 64% |
| both in leather, vs steel; both in the steel kit, vs steel | 56%; 60% | — |

- **The mace and the bit-axe are gone** from the catalog, the recipes and the smithy (no saved world held one: DEV and
  PROD were checked).

## Open

- **Prices** for the rest of the weapons and armour (the user: later).
- **The other weapons** (fang knife, hook cleaver) are still catalog-only: no `class`, so a fight doesn't take them. The
  kettle helm's hearing isn't built.
- **Other NPCs carry no gear** into a fight; guards carry the watch's. A guard (fighting skill 70) in leather with an
  iron sword is a hard fight for a player now.
- **Big armoured team fights** sometimes stall: 1–2% of 3v3–5v5 fights in kits run the simulator's 30 minutes undecided.
- **Fights grow one-sided with numbers** (a 5v5 of Basic blades against bare teeth: 92%): every small edge compounds
  with more wolves a side, as in doc 45.
- **Removing a catalog item** fails a saved world that still holds one (`Society` refuses the whole state): check DEV
  and PROD first, as for the mace and axe.

How to re-run: `SIM_TACTICS=1 SIM_GAP=11 build/level_sim 400 tiers` (about 8 minutes); one row:
`SIM_TACTICS=1 build/level_sim 400 duel plain+iron+leather plain+bronze+cloth 10 11` (blades bronze, iron, steel,
rapier; kits cloth, leather, plate).
