# 35. Items, crafting and industry

Planned 2026-10-02. **Catalog and town layouts built 2026-10-03; the game does not use them yet.** This is the item
catalog, the crafting and industry model the economy will run on, and the workshops the towns and cities now have.

It extends:

- `15-npc-society-economy.md`: money, stock and the accounting rules;
- `26-living-npcs.md`: positions, apprentices, stores and caravans;
- `33-combat.md`: the mouth slot and the sword.

All numbers are *placeholders* for a later balance pass. Prices are in silver pennies (p); a mark is 10p.

## Decisions (2026-10-02 and 2026-10-03)

1. **Weapons are held in the mouth from one grip.** Nothing that needs two "hands" exists as a weapon.
2. **Crossbows are stationary.** They are emplacements like ballistas: rare, higher-tier, hard to move and slow to set
   up. A wolf never wears one; there are no harness mounts.
3. **Jewellery has no count limit, only weight.** It is wolf-specific: fur clips, ruff beads, ear cuffs, tail rings.
4. **Collars are decorative.** Their only social meaning is how much they look like they cost.
5. **Wolves wear** hats, shawls, harnesses, slings across the body and paw wraps.
6. **Horses exist but are never ridden.** They pull carts, wagons, ploughs and mill-gins.
7. **Printing presses exist.** Crafting stations are a new kind of thing, and they bring new businesses with them.
8. **The full catalog now**, with materials, recipes, prices, damage and armour values; the balance pass comes later.
9. **Towns and cities get workshops "within reason".** A small town doesn't have everything; a city has at least one
   of each; industrial Ridgemere has several.

## Where we stand

**Built (2026-10-03):**

- **The catalog** in `Data/Items/`:
  - `items.json`: 410 items;
  - `recipes.json`: 325 recipes;
  - `stations.json`: 41 station types;
  - `businesses.json`: 42 business types.
- **`tools/item_catalog.py`** checks the catalog: every recipe names real items and stations, every item has a source,
  and no recipe loses money at Common quality. `--margins` lists each recipe's cost, value and profit per minute, for
  the balance pass. `--item ID` shows one item, what makes it and what it goes into.
- **New workshops and shops in the towns and cities** (Part 6), in DEV revision 9 (DEV build 14). Every building
  records its stations in the generator's building list.

**Not built:**

- **The game still knows only three goods:** herbs, meals and swords (`Core/RatwSociety.cpp:10`). Nothing reads
  `Data/Items` yet.
- **Stations are not yet objects in the engine.** Each is drawn with an existing tile that stands in for it (Part 5).
- No equipping, crafting, carrying weight, wear or industry demand. These are the phases at the end.

## Principles

1. **Built for a wolf body.** Mouth grips (bits), paw straps, pull-cords, harness loops, low benches. Nothing is
   held in a hand.
2. **One mouth, one grip.** A weapon has to be usable from a single grip point. Bows, spears, polearms, greatswords,
   two-handed hammers and held shields don't exist.
3. **Weight is the limit, not counts**, wherever that makes sense.
4. **Every item comes from somewhere and goes somewhere.** It is gathered, made or imported, and then eaten, worn out,
   used up, built into something or exported. The economy grows through what is made and used, never through silent
   restocking (doc 15).
5. **Data, not code.** Adding a hat never means editing C++.
6. **Clothes say who you are.** Wealth, house colours and trade show in inspection text, never as map decoration
   (doc 05).

## Part 1: The body and what it can wear

### 1.1 Wear slots

| Slot | How many | What goes there |
|---|---|---|
| **Mouth** | 1 | A weapon, a tool, a lantern, a basket, a letter, or fang caps. Holding something stops Bite and muffles speech (doc 33). |
| **Head** | 1 | A hat, hood or helm. Every one has ear slits or ear sleeves. |
| **Throat** | 1 | A gorget or neck guard. Wolves bite for the throat, so this is the armour that matters most. |
| **Body** | 1 | A vest, coat or barding, worn under the harness. |
| **Harness** | 1 | The carrying frame. Its **loops** (0 to 6) take a sheath, pouch, panniers (two loops), lantern hook or tool roll. |
| **Shoulders** | 1 | A shawl, cape or mantle, worn over the harness. |
| **Slings** | 2 | Sling bags, scabbard slings, water skins, bandoliers, crossed over the body. They need no harness. |
| **Paws** | 1 set | Paw wraps, boots, claw caps or leg guards. One item covers all four paws. |
| **Jewellery** | No limit | Fur clips, ruff beads, ear cuffs, tail rings, charms, decorative collars. |

Drawing a sheathed weapon is free outside a fight. In a fight it costs the move, not the action (a guard harness
makes it free). *(placeholder)*

### 1.2 Carrying

Weights are in pounds; an adult wolf is about 90 to 110 lb. *(placeholders)*

```text
comfortable load = 12 + 0.25 × STR            (STR 50 → 24.5 lb)
heavy load       = up to 2 × comfortable      (sprint cap falls in steps, stamina drains faster)
overloaded       = over 2 × comfortable       (walk only; can't start or join a fight)
mouth            = 8 lb held at most; a weapon is 6 lb at most
```

Everything worn counts, jewellery included.

- **A wolf** in a work or draught harness pulls a handcart (200 lb) or a sled (150 lb).
- **A horse** pulls a wagon (800 lb). A wagon is how a siege crossbow is moved.

### 1.3 Jewellery and collars

There is no limit on the number of pieces. What holds a wolf back:

- **Weight.** Every piece counts towards the load.
- **Noise.** Metal pieces have a *jingle* value. Above a threshold, a wolf moving faster than a walk can be heard
  further away and can't sneak. Silk-wrapped ("muffled") versions cost 50% more and are silent.
- **Scent.** Scent lockets hold perfume and give it off for days (the bible's scent layer).
- **Looks.** Each piece has a *status* value (0 to 5). Inspection sums them into "lightly adorned" through "dripping
  with silver". It shows wealth, and it draws pickpockets (doc 26 Phase 7).
- **Issued pieces.** House signet clips and Chapter tokens are issued, never sold. Wearing one you weren't given is
  impersonation.

**Collars** (ribbon, tooled leather, brass-studded, silver chain, gold filigree, jewelled) are jewellery. They mean
nothing but how much they look like they cost. The **spiked collar** is throat armour rather than jewellery, and
punishes a wolf that bites the throat.

### 1.4 What clothes do

| Effect | From | Ties to |
|---|---|---|
| Warmth (0–4) | Shawls, mantles, hoods, felted wraps, fleece boots | Cold and snow (doc 22) |
| Rain (0–3) | Oiled capes, oilcloth hats, fur oil | Wet fur stops shedding the cold |
| Footing | Boots on rock, felted wraps on snow, marsh wraps in mud | Terrain cost (doc 13) |
| Quiet | Soft-soled wraps (quieter); hobnails, mail and jewellery (louder) | Sneaking and hearing (doc 02) |
| Armour | Throat, head, body and leg pieces | Part 8 |
| Status (0–5) | Fine materials, house colours, jewellery | Inspection, NPC attitude |
| Scent | Perfume, masking oil, worn clothes | Tracking, disguise, recognition |

## Part 2: The catalog

`Data/Items/items.json` is the source of truth. Every item has an id, name, category, weight, price and a description
for its inventory card (doc 05). Items also carry the fields that apply to them:

- **Gear:** `slot`, `durability`, and `wear` (warmth, rain, jingle, status, loops, holds, footing).
- **Combat:** `weapon`, `armor` or `siege` values.
- **Food and medicine:** `food` (nourishment, drink, effect), `keeps` (days before it spoils) or `effect`.
- **Raw materials:** `source` (where they are gathered or bought).
- **Flags:** `issued` (never sold) and `tags` such as `restricted`, `fuel`, `dye` and `gem`.

| Category | Items | Examples |
|---|---|---|
| Raw materials | 77 | grain, flax, hemp, wool, honey, livestock and draught horses, timber, bark, ores, stone, clay, sand, peat, herbs, dyes, gems, imports |
| Intermediate goods | 85 | charcoal, iron bloom and bar, steel, bronze, brass, pewter, type metal, wire, nails, fittings, leather, cloth, felt, dyed and scarlet cloth, oilcloth, rope, lime, mortar, plaster, bricks, tiles, glass, lenses, gears, millstones, crossbow prods, printing ink, a case of type |
| Weapons | 13 | below |
| Siege | 4 | wall and siege crossbows, their bolts (Part 2.2) |
| Armour | 14 | below |
| Hats | 10 | felt cap, straw hat, wool hood, oilcloth hat, chaperon, coif, velvet beret, mourning veil, cleric's cap, sheepskin hat |
| Shawls | 10 | patched wrap to scarlet mantle and silk shawl; house livery |
| Harnesses and loop gear | 13 | strap, work, courier, guard, pack, draught, dress and silver-fitted harnesses; sheath, pouch, panniers, lantern hook, tool roll |
| Slings | 6 | sling bag, scabbard sling, water skin, document tube, bandolier, gatherer's basket |
| Paw wear | 8 | linen wraps to silk dancing wraps; fleece-lined boots |
| Jewellery | 32 | bone to jewelled gold fur clips, ruff beads, ear cuffs and rings, tail rings, bells, scent lockets, betrothal pairs, six collars |
| Food and drink | 35 | raw meat and fish, stew (the existing meal), roasts, pies, sausage, jerky, salt fish, pemmican, ship's biscuit, cheese, ale, mead, cider, wine |
| Medicine | 18 | bandages, poultices, salves, splints, stitching kits, willow tea, smelling salts, paw balm, flea powder, clove oil, theriac |
| Care and luxuries | 24 | four perfumes, scent-masking oil, soaps, fur oil, grooming tools, pipes, spectacles, ear trumpet, mirror, chews, dice, instruments |
| Tools and household | 40 | candles, lamps, lanterns, fire kit, bowls, cushions, locks, chisels, saws, picks, snares, nets, lockpicks, quills, ink, paper |
| Containers and transport | 9 | satchel, mouth basket, sack, crate, barrel, handcart, sled, wagon, horse harness |
| Documents | 12 | letter, contract, deed, writ, map, hand-copied and printed books, ledger, broadsheet, pamphlet, almanac, notices |

Wolves eat mostly meat. Bread and porridge are cheap filler and mark the poor. Food spoils, which is what makes salt,
smoking and drying valuable. Spectacles and ear trumpets offset the age-65+ penalties (doc 14).

### 2.1 Weapons and armour

A wolf's weapon is gripped by a **bit**, a padded bar across the back teeth with a lip guard. The blade or head comes
out of the side of the mouth and is swung with the head and neck, so it must balance near the bit. Damage follows doc
33's formula: the sword is 20 base, reach 2, turn weight 10 and 14 stamina. **Pierce** ignores that many points of
armour.

| Item | lb | p | Reach | Dmg | Type | Pierce | Turn wt | Stamina | Special |
|---|---|---|---|---|---|---|---|---|---|
| Fang knife | 1 | 12 | 1 | 12 | cut | 0 | 3 | 6 |  |
| Iron bit-sword | 3 | 22 | 2 | 16 | cut | 0 | 10 | 14 | Cheap; bends rather than breaks |
| Bit-sword | 3 | 40 | 2 | 20 | cut | 0 | 10 | 14 | The existing sword |
| Needle rapier | 2.5 | 90 | 2 | 16 | thrust | 2 | 6 | 10 | Thrust only; +10 to hit |
| Hook cleaver | 3.5 | 45 | 2 | 22 | cut | 0 | 12 | 16 | 25% to tear the target's weapon from its mouth |
| Bit-axe | 4 | 35 | 1 | 24 | cut | 2 | 12 | 16 | +50% against leather |
| War pick | 4 | 50 | 1 | 20 | thrust | 5 | 12 | 16 | Against mail and brigandine |
| Weighted cudgel | 3 | 6 | 1 | 14 | blunt | 0 | 8 | 10 | Non-lethal: Downs, never kills (the watch) |
| Flanged mace | 5 | 55 | 1 | 22 | blunt | 3 | 14 | 18 | Mail's bonus against cuts doesn't apply |
| Mouth flail | 3 | 30 | 2 | 18 | blunt | 0 | 10 | 16 | +10 to hit; 10% to hit itself on a miss |
| Leather cosh | 1 | 4 | 1 | 8 | blunt | 0 | 4 | 6 | From behind or unseen: stuns a turn |
| Steel fang caps | 0.5 | 30 | 1 | +6 Bite | thrust | 1 | 0 | 0 | Worn in the mouth; can't be knocked loose |
| Steel claw caps | 1 | 25 | 1 | 10 | cut | 0 | 6 | 8 | Paw slot: a rake usable with something in the mouth |

| Item | Slot | Zone | lb | p | Protect | Extra | DEX | Special |
|---|---|---|---|---|---|---|---|---|
| Quilted vest | body | body | 5 | 18 | 2 |  | 0 | Warmth 2 |
| Leather barding | body | body | 9 | 34 | 3 |  | 0 |  |
| Boiled-leather barding | body | body | 12 | 60 | 4 | +1 vs cut | −2 |  |
| Brigandine coat | body | body | 18 | 140 | 6 | +1 vs thrust | −5 |  |
| Mail coat | body | body | 20 | 180 | 5 | +3 vs cut | −5 | Jingles |
| Leather gorget | throat | throat | 1.5 | 10 | 2 |  | 0 |  |
| Spiked collar | throat | throat | 3 | 40 | 3 |  | 0 | A Bite on the throat costs the biter 4 |
| Mail neck guard | throat | throat | 3.5 | 60 | 4 | +2 vs cut | 0 |  |
| Steel gorget | throat | throat | 4 | 80 | 5 |  | −1 |  |
| Leather cap | head | head | 1 | 8 | 1 |  | 0 |  |
| Kettle helm | head | head | 4 | 45 | 3 |  | 0 | Hearing −10% |
| Chamfron | head | head | 4 | 60 | 3 | +1 vs cut | 0 |  |
| Leg guards and paw boots | paws | legs | 3 | 25 | 2 |  | −1 | Count as paw boots |
| Splinted greaves | paws | legs | 5 | 70 | 4 |  | −2 |  |

### 2.2 Emplacement crossbows

Crossbows are never carried or worn. They travel **dismantled** and are **set up** into a station, on a wall, a tower
or a prepared spot. Once set up:

- one wolf aims it with a **mouth tiller** and fires it by biting a **trigger cord**;
- spanning it takes a **paw-cranked windlass**;
- taking it down and moving it is as slow as setting it up.

| | Wall crossbow | Siege crossbow |
|---|---|---|
| Moved by | Handcart | Wagon and horse |
| Set up by | 2 wolves, 10 minutes | 3 wolves, an hour |
| Crew to shoot | 1 | 2 |
| Damage / reach / pierce | 40 / 10 / 4 | 70 / 16 / 8, and knocks the target down |
| Reload | 2 turns | 3 turns |
| Price | 450p | 1,500p |
| Made from | prod, windlass, cable, 2 beams, fittings | 2 prods, 2 windlasses, 2 cables, 6 beams, fittings, wheels, axle |

What makes them rare: the **prod** is six steel bars forged by a master armourer (skill 75, two wolves, 40 minutes).
The assembly needs an **engineer** (skill 70 or 85) at an **arsenal**. There are only four arsenals:

- Grayrock's, at Ridgemere;
- the Royal Arsenal, in Ser Ferro's Il Borgo;
- the North Arsenal, at the northern fortress;
- Upper Accord's, later.

The fletcher's trade keeps its name and makes **bolts** for these, plus snares. Arrows don't exist.

## Part 3: Materials and chains

Raw goods come from finite, regenerating sources, like doc 15's herb patch:

- **sources:** fields, pastures, orchards, apiaries, forests, herb gardens, mines, quarries, clay pits, sand banks, peat
  bogs, reed beds, rivers and the coast, hunting grounds and the hearths' ash;
- **imports:** spices, silk, velvet, gold, fine gems, wine, kermes, musk, citrus, frankincense, pipe-weed. Buying them
  takes money out of the world.

Butchery is the hub. A head of cattle gives 10 meat plus a hide, bones, fat, offal, gut, sinew, horn and marrow bones,
and every one of those feeds another trade.

```text
forest → timber → charcoal ─┐
mine   → iron ore ──────────┴→ smelter → bloom → forge → wrought iron → steel → blades, tools, nails, fittings, prods
mine   → copper, tin, lead, silver, calamine → crucible → bronze (bells), brass (buckles), pewter (bowls), type metal

cattle → butcher → meat, hide, bones, fat, offal, gut, sinew, horn
hide + oak bark + lime → tanning pits (3 days) → leather → harnesses, barding, boots, satchels
fat → tallow → candles, soap (with lye from ash), salves
bones → glue, broth, dice, charms        horn → hartshorn salts, lantern panes

sheep → wool → spinning wheel → loom → cloth → dye vat → shawls, hats, livery
flax → linen → bandages, wraps, paper (from rags)       flaxseed → linseed oil → oilcloth, lamp oil, printing ink
hemp → yarn → ropewalk → rope, cord; loom → canvas → sacks, sails

limestone → lime kiln → lime → mortar, plaster, tanning     clay → kiln → bricks, roof tiles, pots, pipes
sand + potash (from ash) → glass furnace → glass → panes, vials, lenses, beads, mirrors

rags → paper vat → paper ┐
lampblack + linseed oil ─┼→ printing press (with a case of type) → books, broadsheets, pamphlets, almanacs, notices
lead + tin → type metal ─┘
```

## Part 4: Crafting

A recipe is: inputs + station + skill + time (+ a wait in days) → outputs (+ byproducts). It may need a tool item at
the station, or a helper.

```json
{"id": "leather", "trade": "tanner", "station": "tanning_pits", "skill": 10, "seconds": 300,
 "in": {"hide": 1, "oak_bark": 2, "lime": 1}, "out": {"leather": 2}, "wait": 3}
```

- **Counts can be fractions:** a fang knife uses a quarter of a steel bar.
- **Groups:** `@fuel`, `@dye` and `@gem` stand for any item with that tag (firewood, peat, coal or charcoal; any dye;
  any gemstone).
- **At the station:** crafting needs the wolf at the station and within reach, like cooking in doc 15.
- **Waits:** leather sits in the pits, malt sprouts, cheese ages, mead ferments. The goods stay in the station and
  belong to whoever started them.
- **Helpers:** the smelter, steel, crossbow prods, big timber, rope and brigandine need a second wolf.
- **Skills** are the trade skills positions and apprentices already have (0 to 100; an apprenticeship completes at 70,
  doc 26). A recipe sets a minimum skill.
- **Learning recipes:** a wolf learns them through the apprenticeship as its skill rises, from a book, or by being
  shown by a master who is present.
- **Quality:** Crude (0.6× price), Common, Fine (skill 60+, 1.6×) or Masterwork (skill 85+, rare, 3×, named).
  Durability scales with quality.
- **The maker's mark:** every made item keeps its maker's name and **scent**. A wolf can sniff out who made a blade, so
  stolen goods give themselves away to a good nose, unless masked.
- **Sinks:** gear wears out and is repaired (each repair costs a little of its maximum durability), food spoils, and
  consumables are used up. Without these, crafters would run out of customers.

**Margins.** Every recipe makes at least as much as its inputs cost at Common quality (checked). The richest per
minute are perfumes, smelling salts, clove oil and masking oil. The bulk metal and building recipes run thin.
`python3 tools/item_catalog.py --margins` lists them all; that's where the balance pass starts.

## Part 5: Stations

**Stations are placed objects, not map tiles.** A map tile is one ASCII character, and only 8 codes are still free;
there are 41 station types. As objects, stations also carry state:

- who is using one, and what is waiting in it;
- its condition and owner;
- for an emplacement crossbow: being taken down, carted elsewhere and set up again, which a tile can't do.

`Data/Items/stations.json` gives each station its own glyph (forge `⚒`, loom `▥`, tanning pits `▤`, printing press `⊟`,
still `⚗`, wall crossbow `➶` and so on), an ASCII fallback, colour, footprint, crew and fuel.

**Until the engine has station objects**, each one is drawn with an existing tile that stands in for it (an anvil for
the forge, a furnace for a kiln, shallow water for vats), and recorded with its position:

- in the generator's building list, each room has `stations: [{x, y, station}]` next to its `beds` and `work`;
- 242 stations are recorded across the generated buildings.

The existing shops' interiors are **unchanged**: their fixtures are now *labelled* as stations (a smithy's anvil is its
forge, a bakery's ovens are ovens). Doc 35's new buildings have full station layouts.

## Part 6: Businesses, and where they are

`Data/Items/businesses.json` lists 42 business types. For each it gives:

- **trades:** whose skill it uses;
- **stations:** what equipment it has;
- **kind:** shop, works or yard;
- **sells:** what it sells;
- **tier:** the smallest kind of settlement that normally has one (hamlet, town, city, industrial).

| Tier | Has, within reason |
|---|---|
| Hamlet | General goods; a herbalist; a dairy |
| Town | Its daily needs (smithy, baker, butcher, provisioner), plus the industry of what it lives on |
| City | At least one of each city business: armourer, jeweller, foundry, dyeworks, paper mill, printing house, saddler, glassblower, arsenal |
| Industrial (Ridgemere) | Several of each heavy trade: ironworks, tanneries, foundries, kilns |

### 6.1 Where they went (DEV revision 9)

**Ridgemere.** Inside its walls there was no room left, so its new trades went to a new walled **works quarter,
the Tanners' Reach**. It lies out by the East Gate among the Houses' estates, with its own road to the gate. It holds:

- **works:** two tanneries, the Rag Mill (paper), the Blue Vats (dyes), a foundry, the South Gate Stables,
  Brinewater's Sail Loft and the Vesk Salt Pans;
- **shops:** the Anchor Forge, the Rivet and Rain (armourer), Collar and Trace (harness), Greywool, Moss & Marrow, the
  Wetink Lane scriveners and the Council Press;
- **lodgings:** eight workers' tenements.

The estates gained works too:

- **Grayrock:** an arsenal, a foundry and lime kilns;
- **Fell:** a wagon yard;
- **Ashcombe:** a tileworks.

**Ser Ferro.** On the Cathedral Rise: a perfumer and the Stamperia della Santa (printing house). Everything else is in
a new walled quarter, **Il Borgo**, outside the East Gate with its own road. It holds:

- **works:** a tannery, a paper mill, the Red Tile Works, a mill, a dyeworks, the bell foundry, a cartwright, stables
  and the Royal Arsenal;
- **shops:** an armourer, a saddler, a glassblower and a marble mason;
- **lodgings:** six workers' tenements.

**Towns, by what each lives on:**

| Place | Added |
|---|---|
| Accord Crossing (caravans) | Wheelwright's yard (cartwright), stables, harness-maker, carters' bunkhouse |
| Saltreach (salt and fish) | Salt pans, smokehouse, lodgings. *The chandler found no room.* |
| Cinderbrook (forge and mine) | Bloomery (ironworks), charcoal sheds, bell pit (foundry), Furnace Row |
| Westmarch (frontier market) | Stables, weaver, harness-maker |
| Lakeside (fishers) | *The smokehouse found no room, inside the walls or out.* |
| Fenhollow (peat and herbs) | Potter's kiln, brewery, kiln loft |
| Amberford (grain) | Mill, stables, millers' row. *The cooperage found no room.* |
| Hollowmere (hamlet) | Herbalist |
| Northern fortress | The North Arsenal, engineers' quarters |
| Isle and dark fortresses | A smithy and a loft each |

Where a town's walls were full, its new workshops were built **outside near a gate**, door towards the gate, as mills
and stables were. **Upper Accord is unchanged:** it isn't built by this generator, and its businesses are a later step
(in Atlas, or an additive script).

Each new business has a master who runs it and sells what it makes, plus a hand or two at its stations: 122 new
residents in all, 1,062 on DEV. Nobody uses the stations yet.

### 6.2 How the layouts were changed without disturbing anyone

The generator rebuilds every town and both cities from seeds, and residents' ids, names, homes and work come out of
the same random sequences. The NPC memories, bonds and careers in the game's tables hang on those ids. So doc 35's
additions are generated so that everything from before comes out **exactly** as it was:

- **Last in the world build.** The two quarters and their roads are built after the roads, the ground and the
  scattered features (`cities.build_quarters`).
- **Last in each district or town.** Inside districts and towns, the new buildings (`Building.fresh`) take whatever lots
  are left after the original ones are placed. A town's industry is placed after its farms and shore spots, and taken
  away again if it would cut off any door from the gate.
- **Separate random sequences.** New buildings and their staff draw from their own sequences (`new_rng`, `…:doc35`).
- **Unchanged interiors.** Existing shop and works interiors are untouched; their fixtures were only labelled as
  stations.
- **Separate staffing.** New staff are generated after everyone else and sleep in the new lodgings, or in beds left
  free. The cities' newcomers (`rmx_`/`sfx_`) ignore the new rooms (`site.FRESH_ROOMS`).

**Checked before import:**

- **Unchanged:** all 713 generated residents, 368 generated interiors and 539 doors are identical to revision 8, and
  so are the routes.
- **Changed cells:** only the towns' cells and the cells the quarters and their roads cross.
- **Backup:** revision 8 is in `artifacts/backups/upper_accord_dev_r8_before_western_20261003-015308.atlas.json`
  (`western --import-dev` now writes this backup itself).
- **Performance:** `world_check --simulate 7 7.2 --players 20` on build 14:
  - 720 places loaded with 0 failures, money conserved, every resident routed;
  - after the first minute the tick's p99 is 25.5 ms and its p99.9 is 33.7 ms (the overall worst ticks are
    place-loading spikes).

**Rule for later changes:** anything added to the generator must keep this property. Re-check it by comparing the new
project with the previous one, resident by resident.

## Part 7: The industry the economy uses up

Crafting needs steady buyers who aren't players. These buyers each hold a real account and a small stock, use it up
on a schedule, and post **procurement contracts** when they run low (doc 26's supply contracts, doc 34's quests). A
crafter, NPC or player fills a contract with real goods and is paid from the buyer's account.

| Buyer | Uses up (placeholder baskets, scaled by population) | Paid from |
|---|---|---|
| Households | Food, fuel, candles, soap, salt; clothes and wraps as they wear | Wages |
| Town Works | Planks, beams, nails, lime, mortar, plaster, tiles, bricks, dressed stone, rope | Treasury |
| Garrison / watch | Rations, bolts, armour and weapon repair, paw boots, lamp oil | Treasury |
| Church | Beeswax candles, incense, wine, paper, glass | Tithes |
| Docks and shipyards | Timber, pitch, rope, canvas, nails, barrels, salt pork | Harbour dues |
| Mines and quarries | Rope, props, lamp oil, picks, food | Owners |
| Administration | Paper, ink, sealing wax, ledgers, printed notices | Treasury |
| Caravans | Wheels, axles, harness, sacks, rations; hay and oats for the horses | Merchant houses |

- **Buildings decay.** Each building has a condition that falls with weather, and Town Works buys materials to mend
  it. A town that can't pay shows it: cracked plaster, missing tiles, a closed bridge.
- **Horses eat a bale of hay or two measures of oats a day.** That is a steady sink.
- **Money rules stay doc 15's.** Transfers never create money. **Sources** are capped export orders for real surplus:
  iron, cloth, leather, salt fish, glass, books. **Sinks** are imports and fees that leave circulation. Taxes and dues
  are transfers into treasuries, which buy upkeep from crafters; that loop keeps the trades employed.

## Part 8: Combat gear (into doc 33)

- **Weapons:** the values in 2.1, with doc 33's mouth slot, knock-loose and muffled speech. Lighter weapons are knocked
  loose less often.
- **Armour:** first a flat reduction, the summed protection minus the attack's pierce, with the per-type extras. Hit
  zones (throat, head, body, legs) come later: Bite aims for the throat, and leg hits add to lingering movement loss.
- **Weight:** body armour's DEX penalty and a heavy load slow the initiative meter.
- **Medicine as actions:** bandages and stitching kits make Tend wounds stronger; smelling salts wake a Downed ally.
- **Emplacements:** a crossbow set up on a cell can be crewed in a fight that cell's arena copies (doc 33 Part 2).

## Phases

1. **Done (2026-10-03):** the catalog, recipes, stations and business types, with their checker; the towns' and
   cities' new workshops and their staff on DEV.
2. **Catalog in the game.**
   - The server loads `Data/Items` in place of the hard-coded list. Old saves keep `herbs`, `meal` and `sword` under
     the same ids.
   - Weight, stacks, quality, maker and spoiling; the inventory shows the load.
   - Shops sell what they actually stock.
3. **Station objects.**
   - The engine places stations from the building data, draws their glyphs and keeps their state.
   - Atlas can place and move them.
4. **Wearing.**
   - Slots, loops, slings and jewellery; equip and unequip.
   - Inspection text; warmth and rain with weather; jingle; scent.
5. **Crafting.**
   - Timed work at stations, helpers, waits, skills and learning.
   - A Craft panel for players; NPC routines make what their trade makes.
6. **Raw sources and chains.** Fields, forests, mines, quarries and the rest as finite, regenerating sources; iron,
   leather and cloth first.
7. **Industry demand.** The buyers in Part 7, building condition, export orders and imports.
8. **Combat gear and emplacements** (Part 8, into doc 33).
9. **Repair and wear.**

Each phase passes `world_check --players 20`, with doc 15's money check throughout.

## Open questions

1. **Gunpowder.** Not in the bible; crossbows fill the role. *Recommendation:* none for now.
2. **The three that found no room:** Saltreach's chandler, Lakeside's smokehouse and Amberford's cooperage. Should
   those towns' walls grow, or should these go without?
3. **Upper Accord's businesses:** in Atlas by hand, or with an additive script that only places into empty lots?
4. **Players as crafters:** can a player own a workshop, or only rent a station (a town forge for hire) or work at a
   master's?
5. **Gift crafting:** may a matching Gift help a trade a little (an Earth Wolf at the kiln, a Fire Wolf at the forge)?
   The bible says magic never replaces a trade.
6. **Grain in a wolf's diet:** this plan treats bread and porridge as cheap filler that marks poverty.
