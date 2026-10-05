# 41. Hunting and foraging

Planned and built 2026-10-04. Players gather goods in the wild and sell them to the towns, so what they bring in feeds
doc 35's crafting economy. Sneaking up on game is doc 40's, for the combat session. Until then, hunting uses a
placeholder for noticing. All numbers are *placeholders* for play-testing.

## The user's brief

- Players gather things to put into the economy: gatherable goods, and hunting.
- Animals, from rabbits to deer, as NPCs with no stories, memory or speech, never tracked outside where they're loaded.
- Their numbers depend only on how recently a place was hunted, each place's spawn rates, and how many are playing.
- A wolf in the wilderness enters **hunting mode**. It works like a fight: the hunter goes into an arena, and the world
  sees them in a red square. Friends and party members may join; nobody else.
- Which animals appear depends on the region.
- Hunting means sneaking up and attacking. The more damage in a single blow, the better the hides and meat. Fire spoils
  much of what one wants.

## How it works

**Hunting** (`Core/RatwHunt.cpp`):

- **Hunt** in the action bar, shown out in the wild: outdoors, not in a town, on ground that is at least half natural
  (`wildFrom`).
- A hunt is a fight (doc 33) with `Battle::hunt` set. The hunter's arena is cut from the cell as for any fight, and the
  world sees the hunter frozen in the red square.
- **What turns up** is decided by the arena's ground. Every tile is counted by kind: forest, grass, heather, shrub,
  crop, water, reeds, mud, rock, snow, sand.
  - Each species has weights for the ground it likes (`Data/Wild/animals.json`), and a rarity.
  - The expected number in a hunt is `expected` (3) × the arena's share of natural ground × the pressure × the players,
    rolled by chance.
  - **Pressure:** each kill in a cell adds 1, fading over `recoveryDays` (2 game days). `capacity` (6) recent kills
    halve what a hunt there finds.
  - **Players:** × `1 + online / perPlayers` (50), up to `most` (2). A busy world has more game about, so it isn't
    hunted bare. Pressure is per cell, so a crowd still empties a place.
  - Nothing turns up: no hunt, and the hunter is told so (or that the ground has been hunted lately).
- **During a hunt**, every `arrivalSeconds` (30) another animal may wander in at the arena's edge (at a chance of
  expected / 6, up to `atOnce`, 6).
- **Animals live only in their hunt.** They are transient entities (`wild:N`), invisible to the world's sight check, so
  no view, motion frame or action outside the hunt ever shows them. They go when they get away (off the edge) or when
  the hunt ends. Nothing of them is saved.
- **An animal's turn** (`World::animalTurn`, hooked into `npcTurn`):
  - **Unaware:** it grazes, now and then a step or two, facing wherever it goes. It is never turned to meet the hunter,
    so a blow from behind is possible.
  - **Notices** (the placeholder, `World::animalNotices`): a hunter within its `alert` tiles (twice that for one at a
    sprint), or next to it, or being hurt.
  - **Alert**, by `temper`:
    - `flee` runs for the edge and away;
    - `cornered` fights only when cornered or hurt (fox, badger, red deer, mountain goat, elk);
    - `fierce` turns on the hunter (boar, bear).
  - The arena card marks an alert animal with "!".
- **Health:** a blow is scaled by the species' `health` (a rabbit 8, a roe deer 35, a boar 50, a bear 120) onto the
  fighters' scale of 100 (`World::huntBlow`, hooked into `hurtFighter`). A downed animal dies at once (`World::huntKill`,
  hooked into `downFighter`).
- **What a kill yields** goes to whoever brought it down (the last to strike it if its burns did it). It is the
  species' `yield`, scaled by **the hardest single blow** against its health:

  | Hardest blow | Kill | Share of the yield |
  |---|---|---|
  | its whole health | clean | all |
  | half | good | 0.8 |
  | a quarter | rough | 0.6 |
  | less | ragged | 0.4 |

  - Fractions round by chance, so a 0.6 share of one pelt is a pelt three times in five.
  - **Fire** (anything with the Fire Gift's downed cause) spoils pelts, hides, feathers and bristles in proportion,
    wholly once fire did half the damage. It chars half its share of the meat.
  - **Quality** (doc 35, Part 4): a clean kill's goods are fine, masterwork if one blow did it before the animal knew;
    a ragged kill's are crude; fire leaves them common at best.
  - The hunter is told: "You bring down a roe deer: a clean kill. You take 4 fine raw meat, 1 fine hide and 1 fine
    sinew."
- **Joining:** only the hunter's party or Chapter (the game's `setFriends`), or a companion who follows a hunter, and
  only on the hunters' side. A real friends list doesn't exist yet. When it does, it goes into the same rule.
- **Leaving:** **Give up hunt** (any time, from anywhere in the arena), or fleeing off the edge as in any fight. The
  hunt ends when no game or no hunter is left. Its banner says "The hunt is over · 2 taken, 1 got away".
- A boar or bear can down a hunter: players are never killed (doc 38).

**The animals** (`Data/Wild/animals.json`):

| Animal | Ground it likes | Health | Temper | A clean kill gives |
|---|---|---|---|---|
| Rabbit | grass, scrub, crops | 8 | flee | 1 raw meat, 1 small pelt |
| Hare | grass, heather, snow | 10 | flee | 1 raw meat, 1 small pelt |
| Pheasant | grass, scrub, crops | 6 | flee | 1 raw meat, 3 feathers |
| Grouse | heather, snow | 6 | flee | 1 raw meat, 2 feathers |
| Wild duck | water, reeds | 6 | flee | 1 raw meat, 2 feathers |
| Red fox | forest, scrub | 14 | cornered | 1 fox pelt |
| Badger | forest | 20 | cornered | 1 badger pelt, 2 fat, 1 raw meat |
| Roe deer | forest, scrub | 35 | flee | 4 raw meat, 1 hide, 1 sinew |
| Red deer | forest, grass, heather | 55 | cornered | 8 raw meat, 1 hide, 1 antler, 1 sinew, 2 bones |
| Wild boar | forest, mud | 50 | fierce | 6 raw meat, 1 hide, 1 bristles, 2 fat |
| Mountain goat | rock, snow | 35 | cornered | 3 raw meat, 1 hide, 2 horn |
| Elk | snow, mud, reeds | 80 | cornered | 12 raw meat, 2 hides, 1 antler, 2 sinew |
| Brown bear (rare) | forest, rock | 120 | fierce | 10 raw meat, 1 bear pelt, 4 fat, 2 bones |

New items: small pelt (2p), fox pelt (6p), badger pelt (4p), bear pelt (30p).

**Foraging** (`World::forage`, `Data/Wild/forage.json`):

- **Forage** in the action bar, out in the wild, takes something from the ground beside the wolf (the 3×3 tiles
  around it), chosen by weight among what grows there in the season:
  - trees: firewood, oak bark, mushrooms, oak galls;
  - pines: firewood, resin;
  - deadwood: firewood, mushrooms;
  - scrub: berries, firewood;
  - grass: cooking herbs, yarrow, thyme;
  - wildflowers: chamomile, lavender, herbs;
  - heather: thyme, herbs;
  - moss and ferns: mushrooms, comfrey;
  - reeds: reeds;
  - mud: clay;
  - sand: sand;
  - shallows: shellfish, reeds.
- Herbs, berries and mushrooms are seasonal; nothing green in winter.
- Once every `seconds` (4).
- **Patches:** each 8×8 tile patch gives `picks` (4) pickings, and one comes back every `regrowHours` (2 game hours).
  Then: "This patch has been picked over lately; try further on."
- Weight counts (doc 35's load): a bundle of firewood weighs 15, so foraging gives one at a time. Building stone (60) is
  left out.

**Selling it on** (`buys` in `Data/Items/crafts.json`): shops buy gathered and hunted goods from players and sell them
on to the trades that use them. They aren't stocked with them to start with, and each holds up to a supplier's store
(40).

| Shop | Buys |
|---|---|
| Herbalist, apothecary | herbs, yarrow, thyme, chamomile, lavender, comfrey and the other medicinal herbs; resin |
| General store | firewood, mushrooms, berries, resin, reeds, feathers, oak galls, stone, clay, sand, small pelts |
| Provisioner, stall, inn | mushrooms, berries, raw meat, shellfish (and fish, vegetables) |
| Butcher | raw meat, hides, fat, bones, sinew, antler, horn, bristles |
| Tannery | hides, oak bark, every pelt |
| Tailor | small, fox and badger pelts |
| Chandlery | fat, resin, beeswax, reeds |
| Fishmonger | shellfish, fish |

A tannery that buys a hunter's hide makes leather of it (doc 35's crafts); a butcher sells the meat on to the inns.

`tools/item_catalog.py` checks it all: every animal's ground and yield, every forage good, and that a shop buys each
one.

## Tracks (built 2026-10-04)

- **Smell** out in the wild (the action bar's Smell, as before) also noses the ground for game
  (`World::smellTracks`). The trails it finds are drawn faintly on the map's tiles in the animal's colour, fainter when
  old. Pointing at one names it ("a roe deer's trail · fresh"). The message says what it found and where ("a red deer's,
  fresh, to the east, and a hare's, right here").
- **The nose's reach:** 12 tiles × smell × nose health × (1 + 0.75 × tracking skill) × the weather's scent (rain
  washes trails), between 4 and 30 tiles.
- **Trails are the ground's, not the wolf's:** the same for anyone who smells there. They change every four game hours.
  Each 16-tile block of natural ground has as many as the game there would leave: the hunt's expected count (ground,
  pressure, players) for the block's share, ×3. Each is a short wandering way (6–13 tiles) over open natural ground. A
  third are fresh.
- **Following one:** a hunt begun within 12 tiles of a trail one has found makes its animal three times likelier (six,
  fresh). A fresh trail means it is still here, so it is the hunt's first animal. "You follow a hare's trail." The
  trail is used up.
- Trails stay on the map for 10 minutes. Finding some trains the tracking skill (`scentSkill`) a little.
- In the snapshot: `wild.tracks` (species, name, colour, fresh, tiles). Not saved.

## Built (2026-10-04)

- `Data/Wild/{animals,forage}.json`, `Core/RatwWild.*` (the data), `Core/RatwHunt.cpp` (the hunt, the animals,
  foraging).
- Hooks in `Core/RatwBattle.cpp`, one line each: `hurtFighter` (huntBlow), `downFighter` (huntKill), `npcTurn`
  (animalTurn), `checkOver` (huntBanner), `finishBattle` (endHunt) and `joinBattle` (only friends, only the hunters'
  side). `World::tick` calls `tendHunts` after `tendBattles`. `visionClarity` hides animals from the world.
- The game: the `hunt`, `forage` and `leaveHunt` commands; `wild` in each snapshot (whether one may hunt here, what is
  to be foraged); `hunt` and each fighter's `animal` (species, glyph, colour, aware) in the fight view; hunts joined by
  party or Chapter.
- The client: **Forage**, **Hunt** and **Give up hunt** in the action bar. In the arena, animals are their glyph in
  their colour ("!" once alert). On the fight screen's cards, the glyph stands where a wolf's portrait would.
- `Tests/hunt_tests.cpp`: hunts start with game at a distance, unaware and unseen in the world; clean and fire kills;
  ragged kills give less; game gets away and the hunt ends and cleans up; only friends join; foraging and its patches.
- On DEV's real ground (a probe over Greenholt, the Mirelands, the Amber Steppe and others): hares, rabbits, roe and
  red deer, boar and pheasant turn up by the ground; forage gives firewood, resin, thyme, herbs, mushrooms and comfrey.
  The perf gate is unchanged.

## Not yet

- **Sneaking** (doc 40): stalking, cover, wind, an ambush's bonus. Until then animals notice by distance.
- A hunting skill of its own (tracking trains the nose; the kill trains fighting).
- **Traps and snares** (the catalog has a snare) and fishing by players.
- **A friends list:** the join rule uses party and Chapter until there is one.
- **Pressure is in memory only:** a restart lets every place recover at once.

## Open questions

1. Joining: the brief's wording ("other players except for friends and party members can join") was read as "only
   friends and party members". Is that right?
2. Players and game: more players online means more game (up to twice as much), with each place's pressure still its
   own. Or should more players mean less to go round?
3. Should a hunter's party share a kill's yield, rather than the one who struck last taking it all?
