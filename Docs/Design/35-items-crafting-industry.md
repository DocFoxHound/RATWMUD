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

- **The game knows herbs, meals, swords and the catalog's wearables** (`Core/RatwItems.cpp` reads `Data/Items`). The
  rest of the catalog (materials, food, tools, the other mouth weapons) isn't in the game yet. *Since doc 39
  (2026-10-04)* every item's name and price are known, and shops sell a handful of their kind's cheap goods.
- **Stations are not yet objects in the engine.** Each is drawn with an existing tile that stands in for it (Part 5).
- No crafting, carrying weight, wear and tear or industry demand. These are the phases at the end.

**Built 2026-10-04 (Phase 4, first part, with part of Phase 2):**

- **Wearing.** The slots and fur spots of 1.1, `wear` and `take off` (not in a fight), saved with the character. What is
  worn stays in the purse, so it can't be sold off one's back; anything worn that the purse no longer has is let go of
  on joining and after a trade. The held sword, sold, is let go of the same way.
- **What others see.** A closer look says what one wears and where the jewellery is.
- **Shops.** A shopkeeper whose work label matches a business's `match` words (tailor, weaver, jeweller,
  harness-maker, tanner, armourer) sells that business's wearables, and makes one more of whatever is sold out while
  at work, as the smith forges blades *(placeholder until crafting takes materials)*. Prices are the catalog's.
- **The Dev Console:** `/give <item> [count]` and `/wearables [word]` for Dungeon Masters.
- **The status screen** shows the slots around one's portrait, the fur spots on a wolf in outline, and belongings to
  put on, clip on and take off.
- **Armour and weapons at a glance:** every visible wolf's card (In Sight, and the fighter cards in a fight) carries a
  little wolf in outline with its armour shaded and a blade at the muzzle; pointing at it lists them. Others' equipment
  pages, from a closer look, are to look at only.
- **Not yet:** harness loops, warmth and rain with the weather, jingle, scent. (Armour in fights is built: Part 8;
  carrying, below.)

**Built 2026-10-04: carrying** (1.2; `World::loadOf` in `Core/RatwWear.cpp`). Everything in a player's purse weighs, worn
or not, at the catalog's weights; residents carry freely (a shopkeeper's stock is a shop's, not a load).

- **Comfortable** up to 12 + STR ÷ 4 lb (24.5 at STR 50): no cost.
- **Heavy**, up to twice that: the top pace falls a notch for each quarter over (sprint 10 down to a run of 6), and
  running's stamina cost rises by up to half (×1 to ×1.5 across the band). *Placeholders for the balance pass.*
- **Overloaded**, beyond twice: a walk only, and no starting, challenging, accepting or joining a fight ("You are
  carrying too much to fight. Put something down first."). One already in a fight, or attacked, still fights.
- The cap is the server's: `effectivePace` is held to `Entity::loadPace`, worked out each tick from the purse, so the
  page's walking speed (`walkSpeed`) and top speed follow without a client rule.
- **On the page:** the snapshot's `self.load` ({carried, comfortable, state, pace, drain}) and each belonging's
  `weight`. A bar on the status screen and the Belongings sheet ("LOAD 41.2 LB OF 24.5 LB · HEAVY"), marked at
  comfortable and filled to twice it, saying what the load costs; a "Heavy load" or "Overloaded" condition; and in the
  status panel a chip only while heavy or overloaded. Each belonging shows its weight. Catalog goods other than herbs,
  meals, the sword and wearables (bought at a shop, given) are now listed among belongings too, since they weigh.
- **Worn armour** on the status screen, by hit zone (doc 33): "ARMOUR · HEAD 3 · THROAT 5 · BODY 4 · LEGS —", the best
  piece worn on each.
- Tests: `wear_tests` `carrying`; `social.test.ts` (the words); `tools/client/carrying.mjs` (the real page; screenshots
  in `artifacts/screenshots/carrying/`). `world_check --simulate 12 13 --players 20` on DEV build 22: mean 11.1 ms, p99
  22.7 ms a tick.

**Built 2026-10-04 (Phase 5, first part): shopkeepers craft from materials.** Nothing on a shop's shelf is made from
nothing any more; the "one more of whatever is sold out" placeholder and the smith's free swords are gone.

- **The starter crafts** (`Data/Items/crafts.json`): 17 whole-batch crafts, each from one or two ingredients, chosen
  with the user to start the economy:

  | Kind | Craft | Ingredients → batch | Makers |
  |---|---|---|---|
  | Food | bread | 1 flour + 1 firewood → 6 | bakery |
  | Food | porridge | 1 oats + 1 milk → 4 | inn |
  | Food | stew (the prepared meal) | 1 raw meat + 1 vegetables → 1 | inn |
  | Food | smoked fish | 2 fish + 1 firewood → 2 | fishmonger, provisioner |
  | Drink | cider | 2 apples → 1 | brewery, inn |
  | Clothing | wool scarf | 1 wool → 1 | weaver |
  | Clothing | straw hat | 1 straw → 2 | weaver, general store |
  | Clothing | linen neckerchief | 1 linen cloth → 4 | tailor, weaver |
  | Armour | quilted vest | 2 linen cloth + 1 wool → 1 | tailor, armoury |
  | Armour | leather cap | 1 leather → 2 | tannery, armoury, saddlery |
  | Paw wear | leather bindings | 1 leather → 2 | tannery, saddlery |
  | Jewellery | fang charm | 1 bones + 1 cord → 4 | tinker, jeweller, general store |
  | Household | tallow candle | 1 tallow + 1 cord → 2 | chandlery |
  | Household | clay pot | 1 clay → 2 | pottery |
  | Medicine | bandages | 1 linen cloth → 2 | apothecary, herbalist |
  | Weapon | sword | 1 bronze bar + 1 charcoal → 1 | smithy |
  | Hardware | nails | 1 iron bar + 1 charcoal → 6 | smithy, tinker |

- **Suppliers** (`supplies` in the same file) sell the ingredients: stalls (flour, oats, milk, vegetables, apples),
  provisioners (those, raw meat, fish and firewood), general stores (flour, oats, firewood, straw, wool, linen cloth,
  cord, tallow, clay), butchers (raw meat, bones, tallow, leather) and fishmongers (fish).
- **Inns and taverns** are now a business (`inn`, matched by "keeping the inn", "taproom", "refectory", "cooking
  for"...), so innkeepers cook.
- **What a shop sells:** a maker sells what it makes, a supplier what it supplies (plus meals and herbs as before). A
  kind of shop that makes nothing yet (a scribe, a mason, a glassworks) keeps the handful of goods it had and sells
  them down.
- **The work** (`Core/RatwCrafting.cpp`): a maker at work whose shelf holds fewer than 4 of something it makes begins
  a batch, and the goods appear when the batch's working time is done (bread 2 game minutes, a sword 15). The
  materials are taken then, so a batch interrupted by a restart is simply begun again.
- **Buying materials:** below 2 batches' worth of an ingredient, the maker buys back up to 8 batches' worth from a
  supplier in the same community, at the catalog price. The money goes to the supplier, so it stays conserved.
- **A good store to start with:** every maker starts with 8 batches' worth of its ingredients, and every supplier with
  40 of each thing it supplies (*placeholders*). A save from before crafting gets this once, the first time it runs
  (`craftingStocked` in the saved society).
- **Not yet:** residents don't eat bread or porridge, or buy clothes, candles and the rest (only the meal), so shops
  sell only to players for now and a town's makers soon have all they want on their shelves (doc 35 Part 7's
  buyers). Bronze and iron have no source, so smiths have only their starting store. Quality is built (below). Players can't craft yet (decided:
  rent a town station first, own a workshop later; the Craft panel comes with it).
- `tools/item_catalog.py --crafts` lists the crafts with their cost and value, and checks them (one or two whole
  ingredients, real makers, never at a loss; producers' goods; anything nothing supplies, makes or produces);
  `Tests/crafting_tests.cpp` covers the work.

### Quality (built 2026-10-04)

Part 4's four qualities are in the game. Prices are *placeholders*.

- **In the catalog** (`Core/RatwItems.cpp`): every good but herbs, meals, the sword and water has a crude, a fine and a
  masterwork kind beside its common one. The id carries the quality (`hide~fine`), and the name says it ("Fine hide").
  - Price: crude 0.6×, fine 1.6×, masterwork 3×.
  - Wear: armour a quarter weaker (crude) or stronger (fine, at least +1), half as strong again for masterwork; status
    one step down or up (two for masterwork); masterwork one warmer.
  - `good()` and `wearable()` find every kind through an index (lookups are now hashed, not scanned). The catalog's
    lists stay common goods only, so no shop's handful picks a fine one at random.
  - Helpers: `items::baseOf`, `qualityOf`, `withQuality`, `kindsOf`.
- **Made goods** (`Core/RatwCrafting.cpp`): a batch's quality comes from the maker's skill at their position (doc 26's
  careers), the materials (a crude one drags it down, a fine one lifts it), and luck.
  - Fine needs a skill of 60, and is common only past 75.
  - Masterwork needs 85, a very good day, and a further one in seven.
  - Crude comes when the score falls low (the unskilled, poor materials).
  - Makers use any quality of a material, plain ones first, and buy any quality at its price.
  - On DEV over a working morning: 237 common, 59 fine, 2 masterwork.
- **Hunted goods** (doc 41): a clean kill is fine, masterwork if one blow did it before the animal knew (doc 40's
  ambush), a ragged kill crude, and a fire-touched kill no better than common.
- **Shops** deal in every quality of their goods: a tanner buys a fine hide at the fine price. The trade panel lists
  each quality the shop or the player has, named ("Fine hide").
- **Weapons** (built 2026-10-04): the sword has the four kinds too ("Fine bronze sword").
  - A blow scales by quality: crude 0.85×, fine 1.15×, masterwork 1.3× (`items::qualityDamage`), and the fight screen's
    damage preview shows it.
  - The jaws still hold "sword" (`Entity::mouth`), and `Entity::swordKind` says which. Taking up a sword takes the best
    one has. A sword knocked loose drops that very kind.
- **Wear and tear** (built 2026-10-04, `Core/RatwDurability.cpp`): gear in service wears out.
  - A sword wears a point with each swing.
  - Armour wears with each blow that lands where it covers: a point, plus a quarter of what it kept off. `land()` now
    says the hit zone.
  - Clothes, harness and jewellery wear a point a game day while worn.
  - How much use a piece takes is its catalog `durability`, × 0.6 crude, 1.5 fine, 2.5 masterwork.
  - Wear is kept by item kind (`Entity::wear`, saved), so taking a piece off and on doesn't mend it.
  - Worn out, it falls apart: one fewer in the purse, and a spare of the kind goes into service fresh. Without one, the
    slot is empty. The player is told.
  - **Mending:** a shop that sells, makes or deals in the kind of thing (a weaver a scarf, a smith or armourer a sword)
    mends it, for half the good's price times how worn it is, paid to the shop (`repair` command; `World::repairGear`).
    The inventory shows each piece's condition, and beside a shop that can mend it a **REPAIR · Np** button.
- **The maker's mark** (built 2026-10-04): a masterwork carries its maker in its id (`sword~masterwork@sorrel`; a hunted
  masterwork, the hunter's).
  - `good()` and `wearable()` find it as the masterwork kind. Stock, crafting, buying and the trade panel count marked
    goods with their kind (`Society::kindsHeld`).
  - Its name reads "Masterwork bronze sword · Sorrel Brook's mark" to anyone who knows the maker (doc 32's
    introductions), "your own mark" to its maker, and "a maker's mark, and a scent you don't know" to anyone else.
- **Mending never costs durability** (the user, 2026-10-04): a repair makes a piece whole, and it lasts as long as it
  ever did.
- **The nose** (built 2026-10-04, `Core/RatwMarks.cpp`): how keen a nose is
  (`World::noseAcuity` = `smell` × nose health × (1 + 0.75 × tracking skill)) is a physical stat, `Entity::smell`.
  - It sharpens with use (the user: "a physical stat that increases with level and/or usage"; characters have no level
    yet). Every Smell grows it a little, slower as it nears 1.6 (*placeholder*), and the tracking skill with it.
  - Smell also reports up to three masterworks carried near by, by the maker's scent on them: "A maker's scent reaches
    you from something a lean grey wolf carries: Masterwork bronze sword · Sorrel Brook's mark". It needs the scent's
    clarity × the nose's keenness ≥ 0.5, so a keen nose catches it farther off, and downwind.
- **Stolen goods give themselves away:** a theft now and then lifts a masterwork (one in four, where the victim has one
  not being worn). Every 10 s, an NPC who would know the work can catch the maker's scent on a thief still carrying it.
  - Who would know it: the one robbed, its maker, or a guard on duty, in the same place.
  - The chance per try is 0.15 × the scent's clarity × the nose's keenness² (*placeholder*).
  - Caught, that NPC becomes a witness who knows the thief, believes they stole it ("smelt it"), and the crime takes
    its course (doc 26 Phase 7; a cold case opens again). A player thief is told: "Wren sniffs the air near you, and
    looks hard at what you carry."
- **Masking oil:** used from the inventory (**USE**; the `mask` command), it hides one's scent, and that of what one
  carries, for 4 game hours (a crude oil 0.6×, fine 1.5×, masterwork 2.5×; uses add up).
  - While masked, no scent of the wolf reaches anyone (`scentClarity`): no scent cues, no stolen goods smelt out, no
    marks read on it. Doc 40's sneaking checks `World::scentMasked` too.
  - Apothecaries and perfumers make it from 2 wormwood and a resin. Herbalists supply wormwood. Woodcutters bring in
    resin, and players forage both (wormwood in summer and autumn grass).
- **Not yet:** masking against tracks (a hunter's own trail is not yet a thing), and a nose's keenness from a level when
  levels exist.

### Workshops for the starter crafts (built 2026-10-04)

The suppliers are refilled by work, not a starting store. Still in `Data/Items/crafts.json`:

- **Producers** (`producers`): residents whose work label matches bring goods in from the land, one yield per spell of
  work (`seconds`), keeping at most 20 of each (*placeholder*) for the town to buy. Crops only in spring, summer and
  autumn. A producer working out in the country belongs to the town they live in.

  | Producer | Matched by | One yield | Every |
  |---|---|---|---|
  | Farm | "farms the", "works the crossing/lakeside fields" | 2 wheat, 1 oats, 2 vegetables, 1 straw, 1 flax, 1 hemp, 1 apples | 30 min |
  | Sheep pasture | "keeps sheep", "shepherd" | 1 wool, 2 milk | 30 min |
  | Fishing | "fishes the" | 2 fish | 15 min |
  | Woodcutting | "cuts wood", "woodcutter" | 3 firewood, 1 oak bark, 1 timber | 15 min |
  | Charcoal burning | "burns charcoal", "charcoal kilns"... | 1 charcoal | 30 min |
  | Quarry | "cutting stone" | 1 stone, 1 limestone, 2 clay | 30 min |
  | Salt pans | "raking salt", "rakes the salt" | 2 salt | 30 min |
  | Stock-breeding | "stables" (the stables' keepers) | 1 pig, 1 sheep | 1 hour |

- **Workshop crafts** make the suppliers' goods: a mill grinds 2 wheat into 2 flour; a weaver weaves 2 flax into
  linen cloth; a tannery makes 2 leather from a hide and 2 oak bark (lime and the 3-day pits come later); a chandler
  spins a hemp into 3 cord; a butcher cuts a pig into 4 meat, a bone, 2 tallow and a hide, or a sheep into 3 meat, a
  bone and a tallow. Mills (The Ford Mill, Mulino del Borgo, The Millers' Row), La Concia and the stables are now
  known by their keepers' work.
- **Who buys from whom:** a maker buys its ingredients from a supplier, else from a workshop or producer. A supplier
  whose store of something is under a quarter buys back up to 40, from workshops and producers only. A workshop keeps
  making what other trades use (judged by a batch's first good: a butcher's meat, not its hides) up to half a
  supplier's store, and keeps 4 for its own shelf; suppliers and producers sell all they have.
- **Carted in:** what a community can't find at home is bought from another at half as much again (*placeholder*),
  the extra going to the seller. This feeds the cities, which have few farms, until caravans carry the goods.
- **DEV, a working morning** (`world_check --simulate 9 13 --events`): farmers brought in 144 wheat and 144
  vegetables, fishers 48 fish, the stables 32 pigs and 32 sheep; mills ground 48 flour, tanneries made 72 leather,
  weavers 89 linen cloth; weavers in towns without flax had 67 carted in. The perf gate (`--players 20`) is unchanged
  (12.4–12.6 ms mean either way).

What is still missing, by ingredient:

| Ingredient | Comes from now | Missing |
|---|---|---|
| Flour | mills, from farms' wheat | rye; mill keepers only where named so |
| Oats, vegetables, straw, flax, hemp, apples | farms | orchards of their own; fields that run out |
| Milk, wool | shepherds | a dairy herd; cattle |
| Meat, bones, tallow, hides | butchers, from the stables' pigs and sheep | cattle (40p a head is more than its cuts are worth: a balance-pass item) |
| Fish | fishers | — |
| Firewood, oak bark, timber | the one woodcutter | more woodcutters (only Hollowmere has one, so most towns cart firewood in) |
| Linen cloth | weavers, from flax | — |
| Leather | tanneries, from hides and oak bark | lime and the 3-day wait |
| Cord | chandlers, from hemp | the ropewalks' rope-twisters (they make rope, not cord) |
| Clay | the quarry | a clay pit near the potters |
| Charcoal | the burners | timber as its input |
| Bronze bar | nothing | mines (copper, tin) and the foundries working |
| Iron bar | nothing | iron mines and the ironworks working |

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

Settled with the user on 2026-10-04 (catalog slot names in brackets):

| Slot | How many | What goes there |
|---|---|---|
| **Muzzle** [`mouth`] | 1 | For holding: a weapon, a tool, a lantern, a basket or a letter. Holding something stops Bite and muffles speech (doc 33). |
| **Head** [`head`] | 1 | A hat, hood or helm. Every one has ear slits or ear sleeves. |
| **Neck** [`throat`] | 1 | A scarf, neckerchief, leather wrap, gorget or neck guard. Wolves bite for the throat, so the armoured ones matter most. |
| **Body** [`body`] | 1 | A vest, coat or barding. |
| **Harness** [`harness`] | 1 | The carrying frame around the chest, worn under the sides. Its **loops** (0 to 6) take a sheath, pouch, panniers (two loops), lantern hook or tool roll. |
| **Chest, left** and **Chest, right** [`sling`] | 1 each | A satchel, sling bag, scabbard sling, water skin or bandolier on each side, over the harness. They need no harness. |
| **Back** [`shoulders`] | 1 | A shawl, cape or mantle, worn over everything. |
| **Paws** [`paws`] | 1 set | Wraps, bindings, boots, claw caps or leg guards. One item covers all four paws. |
| **Jewellery** [`jewelry`] | No limit | Clipped to the fur at a **spot** (below). Bracelets go on the legs, above the paws. |

**Fur spots.** Each piece of jewellery is clipped at one spot: ears, crown, ruff, chest, back, left foreleg, right
foreleg, left hind leg, right hind leg or tail. A spot takes any number of pieces. An item's `spots` in the catalog say
where it can go (`any` for fur clips, charms and bells; `ears` for ear cuffs; `tail` for tail rings; the legs for
bracelets; the ruff for collars and beads). The status screen shows the spots as markers on the wolf.

Drawing a sheathed weapon is free outside a fight. In a fight it is part of the move, not the action: once a turn,
and a tile off the walk if drawn before it (doc 37, phase 6, built for the sword); a guard harness makes it free.
*(placeholder)*

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

### Households (built 2026-10-04)

The first buyers: the townsfolk themselves (`Core/RatwDemand.cpp`).

- **Food is anything that feeds.** A hungry resident eats the best food it carries: a meal fills it (55 of hunger),
  anything else by its `nourish` (× 1.1), and drinks don't count. It puts the rest in the larder at home, and fetches
  from there first.
- **Buying food:** a shop is open to the hungry while it has any food for sale (no longer only meals). There the
  resident buys whatever gives the most nourishment for its money by its own taste (a steady per-wolf liking, ±40%), so
  bread and porridge sell most and a meal or pie now and then. It buys enough for now (a meal's worth) and, with a larder
  at home, two meals' worth more. It needs only a penny.
- **Household errands:** once a game day, whoever in a household has the most money buys what is due from a shop in
  its community that has it. Each is used up at home at once (`households` in `Data/Items/crafts.json`; all
  *placeholders*):

  | Need | How often |
  |---|---|
  | firewood | a bundle a day; two in winter |
  | a tallow candle | every 2 days |
  | cider | every 4 days |
  | bandages | every 20 days |
  | a scarf, a straw hat or a neckerchief | every 25 days, for each grown wolf |
  | a clay pot | every 30 days |

  Households don't all shop on the same day. A household always keeps 6p a head back for food. A shopkeeper's
  household uses its own shop's stock first. What nobody in town has, it goes without that day.
- **Firewood:** farms now bring in 2 bundles with each yield (hedges and coppice), beside the one woodcutter.
- Money only moves: food and goods are paid to the shop, which buys its materials from suppliers and producers, who are
  residents who buy food. Wages (the treasury) and market dues close the loop.
- Food spoiling was left out on purpose (the user, 2026-10-04: not sure it's wanted).

### The town's own buyers and contracts for goods (built 2026-10-04)

- **The buyers** (`institutions` in `Data/Items/crafts.json`; `Society::townBuyers`, `Core/RatwDemand.cpp`): every
  community of five or more has a Town Works, a watch and a church, each with its own account (`town:<town>:works`,
  `:watch`, `:church`). Each uses up a basket a day (*placeholders*):

  | Buyer | A day | Per |
  |---|---|---|
  | the Town Works | 2 stone, a timber, a limestone, a cord | 100 residents |
  | the watch | 2 bread, half a smoked fish, a fifth of a bandage, a leather cap every month or so | guard |
  | the church | 2 tallow candles, 3 bread (alms) | 100 residents |

  - Each keeps 3 days of its basket in stock and buys from its town's shops (any quality).
  - The treasury funds it, moving money, never making it: enough to keep twice its days' worth in hand, at most a
    twentieth of the treasury a day.
- **Contracts for goods** (`Core/RatwProcure.cpp`): what a buyer can't buy in town (a third of its stock or more, at
  least 2) it asks for as a "procure" contract in that town.
  - The contract names the good and how many ("The watch of Amberford wants 3 smoked fish"), for its price × 1.3, put
    up in escrow from the buyer's funds. One standing contract per buyer per good. It lapses after 7 days, and the rest
    of the reward goes back.
  - A player hears of it from any merchant in town (*ask for work*, *take kN*, as for doc 26's contracts). They bring
    the goods, any quality, and *hand in goods kN* at any merchant of that town. They can hand in a few at a time, are
    paid by the piece, and get the rest of the reward with the last.
  - `Contract` now has `item`, `quantity` and `delivered`, saved with the roads.
- **Staples kept deeper:** makers keep up to 20 (not 4) of what households and the town's buyers use up every few days,
  and of bread, porridge and meals (`items::traded`).
- **On DEV, a night and a morning:**
  - The treasury funded the buyers with 5,567p.
  - The watches bought 397p of bread, smoked fish, bandages and leather caps; the Town Works 138p of stone and cord.
  - 84 contracts for goods went up at midnight: timber and limestone (sold by no shop), bread the bakers hadn't baked
    enough of, candles, bandages, smoked fish.
- **Not yet:** NPCs filling contracts (only players do), and buyers for the rest of the table below (docks, mines,
  administration, caravans).

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
  *Built October 4, 2026, with hit zones, as doc 33's "Armour, by hit zone": where a blow lands is rolled by the side it
  comes at (head on: the face, throat, shoulder or a foreleg), only the armour there counts, and at least a quarter
  gets through; the DEX penalties slow the initiative bar. Aiming for a zone, Bite going for the throat, leg hits
  slowing a wolf, the other weapons, weight and medicine are still to come.*
- **Weight:** body armour's DEX penalty and a heavy load slow the initiative meter.
- **Medicine as actions:** bandages and stitching kits make Tend wounds stronger; smelling salts wake a Downed ally.
- **Emplacements:** a crossbow set up on a cell can be crewed in a fight that cell's arena copies (doc 33 Part 2).

**Decided (2026-10-04): who builds what.** The combat session (doc 33/37) adds armour to the fight rules: a blow's
damage less the target's summed `protect` minus the weapon's pierce (with the "+n vs cut/thrust" extras), never below
1; worn armour's DEX penalty and a heavy load slowing the initiative bar. The battle view should send each fighter's
protection (and the reduction in `odds.damage`) so the page can show it. The UI session shows it: protection on the
status screen and the cards' gear doll, and the strike previews' damage figures after armour.

## Phases

1. **Done (2026-10-03):** the catalog, recipes, stations and business types, with their checker; the towns' and
   cities' new workshops and their staff on DEV.
2. **Catalog in the game.**
   - The server loads `Data/Items` in place of the hard-coded list. Old saves keep `herbs`, `meal` and `sword` under
     the same ids.
   - Weight, stacks, quality, maker and spoiling; the inventory shows the load.
   - **Decided (2026-10-04):** weight and load come first (the UI session): a load bar on the status screen and the
     belongings sheet ("Load 18 of 24.5 lb · comfortable"), and a chip in the status panel only when heavy or
     overloaded, saying what it costs. Quality and the maker's mark move to Phase 5, with crafting: until then
     everything sold is Common.
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
   **Decided (2026-10-04):** rent first, own later. Players rent a town station by the hour or work at a master's;
   owning a workshop comes later, through Chapter halls or a rented place (doc 32's leases).
5. **Gift crafting:** may a matching Gift help a trade a little (an Earth Wolf at the kiln, a Fire Wolf at the forge)?
   The bible says magic never replaces a trade.
6. **Grain in a wolf's diet:** this plan treats bread and porridge as cheap filler that marks poverty.
