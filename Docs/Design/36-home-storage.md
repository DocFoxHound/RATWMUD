# 36. Home storage and food to start with

Built 2026-10-03. Every home indoors has a larder, a chest, a wardrobe and a woodpile; every household starts with
food in its larder, and every player with food of their own.

It extends:

- `15-npc-society-economy.md`: accounts, money and goods;
- `26-living-npcs.md`: residents' days, households and homes;
- `35-items-crafting-industry.md`: the item catalog these stores will hold once the game reads it.

All amounts are *placeholders* for the balance pass.

## Why

- Most residents started with no meal or one. A town got hungry at the same hour and set off for the shop together:
  the week check's biggest crowds were these food runs (doc 31).
- A player started with one meal, and only while the treasury had any left; later players could start with none.
- Nobody had anywhere to keep anything but what they carried.

## The stores

| Store | Holds | Today |
| --- | --- | --- |
| **Larder** | Food | Meals. Spoiling, salted and smoked food come with doc 35. |
| **Chest** | Goods | Herbs, swords, anything not food or wear. |
| **Wardrobe** | Wear | Empty until doc 35's clothes and harnesses exist. |
| **Woodpile** | Fuel | Empty until doc 35's firewood, peat and candles exist. |

- **Each store is a society account** (`home:<home cell>:<kind>`, a facility account like a town's stores or a
  caravan): saved, restored and checked like every other, and the goods in it are real goods.
- **It belongs to the household:** everyone whose home is that cell.
- **Every home indoors has all four.** A home outdoors (a camp, the open road) has none.

## Where they stand

- The world places them, the same way every time, when a home's interior is first in memory
  (`World::placeHomeStores`): along a wall, on open floor, two tiles or more from a door, a tile and a half or more
  from where anyone sleeps, a tile and a half apart, the larder first.
- No home needed editing: DEV's 298 homes all have room for all four.
- **Later:** the generator and Atlas may place them by hand. The engine's placement is then the fallback.

## Food to start with

- **A household's larder** starts with 3 meals for each who lives there, and its chest with 2 herbs each. Anyone of
  them carrying no meal is given one. This happens once, when the home's stores are first opened: on a new world, or
  on the first load of a save made before home storage.
- **A new player** starts with 3 meals of their own. Their 20 pennies and 2 herbs still come from the treasury, and
  run out with it.
- **The exception:** these meals are made, not taken from the treasury. Doc 15's rule is that nothing is made from
  nothing, so a character's starting meals are its one exception. Someone could make characters only to sell their
  meals to shopkeepers (about 12p each, against shopkeepers who only buy so much). If that becomes a problem, starting
  meals can be made unsellable.

## How residents use them

1. **Hungry, carrying no meal, food in the larder:** home to the larder, take a meal, eat it ("fetch food"). This comes
   before buying. Travellers on the road don't: home is far behind.
2. **Buying:** a resident with a larder buys up to three meals at a time (as the purse allows): one to eat, the rest to
   keep.
3. **Putting away:** at home, meals beyond the one carried go into the larder. A shopkeeper's are the shop's stock and
   stay with them.

## Seen in the game

- Inside a home, the page draws each store (`%` larder, `=` chest, `H` wardrobe, `#` woodpile).
- With the pointer near one, it says whose it is and what's in it ("Holly and Laurel's larder · 6 meals").
- Players can't open others' stores. Taking from one would be theft (doc 26's crime), for later.

## Checks

- `town_tests` (`larderFirst`): Greyfen's homes have all four stores, the larder stocked and standing on open floor.
  A hungry resident with food at home eats from the larder and doesn't spend at the shop. A new player starts with
  three meals.
- `society_tests`, `appearance_tests`: starting meals are a player's own; money and herbs still come from the
  treasury and run out.

## Not built yet

- Players' own storage (they have no homes yet).
- Taking from another's store, and locks.
- What goes in wardrobes and woodpiles, and spoiling: doc 35.
- Households buying fuel, candles and the rest: doc 35's household demand.
