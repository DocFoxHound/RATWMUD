# 59. Lamps, homes and household goods

## The user's brief (2026-10-07)

"Ideally, though, I would like to make something else as something that citizens can spend on. Rent? Propel them to
give more to the church? I want to stay away from increasing taxes too much more - maybe housing upkeep and decorations?
Decorations that deteriorate over time but add some sort of value that we don't really have? Household goods like
candles, oils, bedsheets, all that are used. Oh that reminds me, lamps. This world needs lamps (candles and oil lamps)
and they definitely need to be in/outside of every house, so there's an expense we can add to citizens. Also there needs
to be lamps lining the major roads in the game, once every screen-width or so, that has an Area of brightness. Another
menial job we can add, then, is hiring someone to resupply the oil in the lamps (oil comes from processing fish, which
can be done at.... I'm just going to say a tannery for now, why not?)"

## Decisions (the user, 2026-10-07)

- **Citizens spend on all four:** lamps and household goods; home upkeep and decorations; rent; more to the church.
- **A well-kept, decorated home gives all three:** rest and mood, standing in town, and it is seen by players.
- **The Town Works keeps the road lamps:** each town's works fund (the orchestrator's works channel, doc 46) buys the oil
  and posts the lamplighters' odd jobs. A town that can't pay lets its lamps go dark.
- (And for doc 46: the banks keep their savers' books and lend nothing; the orchestrator sends their coins out.)

## Why

The economy (docs 42, 46) moves money between holders, but the comfortable have little to spend it on, so it pools in
their purses. Real things to buy, used up and worn out, draw it back out to the makers, and from the makers to their
hands. And the world is dark at night with nothing in it giving light.

## What there is already

- **The goods:** tallow and beeswax candles (2p and 8p for six), lamp oil (3p a flask), clay oil lamps (6p), torches,
  braziers, blankets, cushions, low tables, chests, soap (Data/Items/items.json, household and care).
- **Who makes them:** the chandlery (candles, oil from linseed oil), the potter (oil lamps), and the fishmonger, which
  renders fish into lamp oil at its rendering pot (`lamp_oil_fish`). The tannery has a rendering pot too, but no recipe for oil.
- **What households use up** (Data/Items/crafts.json, `households.needs`): firewood daily, a pack of tallow candles every
  two days, a clay pot a month, a scarf or hat each, bandages, nails, a cookpot, cider. Each is bought and used up on the
  spot: nothing holds it, nothing comes of it, and going without costs nothing (RatwDemand.cpp, `householdShopping`).
- **Homes:** a household is everyone with the same home cell; its larder, chest, wardrobe and woodpile are accounts
  (`home:<cell>:<kind>`) drawn as glyphs in the room. Nobody owns a home, nobody pays rent. Shops pay their town's great
  houses a ground rent of 20p a month.
- **The Restday plate:** a third of the town at church gives 1p or 2p each.
- **Residents** have hunger and fatigue, nothing for mood, comfort or standing.
- **Roads** are tiles (dirt road, cobbled street, flagstones), not a network; nothing marks a major road.
- **The night:** the client darkens the land and leaves a pool of the player's own sight; rooms have an authored light
  level. Nothing in the world gives off light of its own.

## Part 1: lamps and household goods at home

Every home has **two lamps: one inside, one outside its door.** Each is a fixture (an oil lamp inside, a door lantern
outside, bought once and worn out), and each burns through the night while there's something to burn:
- **Inside:** candles or oil, from dusk until the household sleeps (a candle a night, a pack of six lasting the week for
  a small household; oil, a flask in three nights).
- **Outside:** oil only, from dusk to dawn (a flask in two nights).

A home's lamps keep what they burn in a **lamp store** (`home:<cell>:lamps`, like the larder), and the household buys
candles and oil to keep it stocked (a few nights ahead), as it does food. A home with nothing to burn is dark. At about
2 to 3p a night a household, it's the largest new expense: about a tenth of a poor family's food.

**Household goods that wear out**, held at home and replaced when worn (not used up on the spot): bedsheets (new: linen,
from the weaver or tailor; a set a bed, two months), blankets (a winter), soap (a cake a week), clay pots and cookpots (as
now, but held until they break). The candles need in `households.needs` becomes the lamps.

**Fish oil at the tannery:** the tannery renders fish into lamp oil at its rendering pot as the fishmonger does, and
sells it. Fish is in demand for oil as well as food.

## Part 2: the home itself: upkeep, decorations and comfort

Each home gets a small record (saved with the society):
- **Condition** (0 to 100): it wears a little each day (faster in winter), and is mended with materials (nails, planks,
  plaster, thatch) and a hand: an odd job, **"mending a home"**, posted and paid by whoever keeps it up (Part 3).
- **Decorations:** a few pieces at a time (a rug, a wall hanging, a painted pot, a carving, flowers, fine cushions), each
  with its own wear: a rug lasts a season, flowers a week. Bought by a household with money to spare (a new want, "for the
  home"), from the weaver, dyer, potter, carpenter and the market's flower sellers, and replaced when worn out. New goods
  in the catalog for the ones it lacks (rug, wall hanging, carving, flowers).
- **Comfort** (0 to 100), worked out each night from: its condition; clean bedding; its lamps lit; its decorations; a
  fire in winter (firewood). A plain, sound, lit home is about 50; a dark, cold, worn-out one near 0; a rich, decorated one
  near 100.

What comfort gives:
- **Rest and mood:** a wolf sleeps better in a comfortable home (its fatigue falls faster, from 0.8 times as fast in a
  wretched one to 1.2 times in a fine one), and has a **mood**: a new measure, from its comfort, hunger and money, that its
  voice speaks from (the Mind's mood, now set by the game) and that colours what it says of its home.
- **Standing in town:** each household's standing, a slow average of its home's comfort, that its neighbours know. It
  weighs where doc 48 weighs a match or a tie (a hook, `homeStanding`), and the Dungeon Master sees it.
- **Seen by players:** a home's lamps, lit or dark, its decorations and its state (sound, shabby, ruinous) show in the
  room: glyphs like the larder's, and the light of its lamps at night (Part 5).

## Part 3: rent

**Every home has a landlord,** worked out from the world (nothing authored):
- a **great house** in a town that has them (as for ground rents: one house, the same for a home each month);
- else the **town** (its treasury);
- a shop's flat belongs to the shop: its keeper pays no rent; a farm's bunkhouse to the farm.

**Rent** is paid at the weekly reckoning, by the household (its richest member, as for its needs), `rentADay` a head
(1p, a placeholder) times 7, more for a fine home (by its comfort). A household under its poor line pays nothing that
week (it isn't turned out; the landlord goes without). **The landlord keeps the home up:** its condition is mended from
its rent (materials and a hired hand); the tenants buy their own lamps, goods and decorations. So rent goes back out as
work, and a landlord that pools it is a holder over its band like any other (doc 46).

## Part 4: more to the church

The Restday plate grows with what a wolf can spare: a hundredth of what it holds above its wealth line (at least the 1p
or 2p it gives now), and wolves in comfortable homes go to church a little more often. The church spends it as it does
now: alms, its clergy, and (new) beeswax candles for its services, from the chandlers.

## Part 5: the road lamps, and light at night

**Lamp posts along the roads:** placed by the server when a world loads, from its road tiles (nothing is written into the
world's data, so the Atlas's edits are safe): along every road at least three tiles wide, and every cobbled street, a lamp
beside the road about every 40 tiles (a screen's width at the usual zoom), in towns and between them. Each belongs to the
town whose land it stands on (by community), and its state (the oil in it) is saved with the society.

Each lamp **holds oil** (three flasks, six nights), burns from dusk to dawn while it has any, and gives **light**: a pool
about 8 tiles across, warm, soft at the edge.

**The lamplighters:** each afternoon a town's Town Works posts **"tending the lamps"** odd jobs from its works fund (doc 46):
a round of up to six lamps that are low, the oil bought from its chandlers, fishmongers and tannery at its prices. The hand
walks the round, refills each lamp, and is paid as for an odd job (more for a long round). A town whose works fund is empty
posts none, and its lamps go dark one by one. The orchestrator's works channel takes the lamps' cost into account (it
spends on them first, as on the town's repairs).

**In the client:** at night each lit lamp (road lamps, and the homes' outside lanterns, smaller) cuts a pool of warm light
out of the darkness; inside, a lit room's lamp warms it. A lamp that has gone out is drawn dark. The light is for seeing
too: a wolf in a lamp's pool is seen at night as by day (doc 40's sneaking: hiding is harder under a lamp).

## Phases

1. **Lamps and goods at home** (server): the lamp store, nightly burning, buying candles and oil, the fixtures; bedsheets,
   soap and pots held and worn out; fish oil at the tannery. Measured with a 28-day `econ_watch`: what households spend, and
   on whom.
2. **The home's record:** condition, decorations, comfort; rest and mood from it; standing; the DM sees each home.
3. **Rent and the Restday plate:** landlords, rent at the reckoning, landlords mending their homes; giving by what a wolf
   can spare.
4. **The road lamps:** placement, oil, the lamplighters from the works fund.
5. **Light at night in the client:** lamp pools on the roads and at the homes' doors, rooms lit by their lamps, home
   decorations and state drawn; sneaking under a lamp.
6. **Balance:** a 56-day run against doc 46's last; the poorer half, what the comfortable spend, what the lamps cost.

## Open questions

1. **How dark is dark?** Should a home without light cost its household anything besides comfort (a stubbed paw, nothing
   done in the evening)? *Recommendation:* comfort only, for now.
2. **Rent's level:** 1p a head a day is about a fifth of a day's food. *Recommendation:* start there, rise with comfort
   to 2p, and measure.
3. **Players' homes and lamps:** do players pay rent and keep lamps (doc 36's homes, Chapters' leases)? *Recommendation:*
   not yet: residents first; players' homes later.
4. **Which roads:** every road three tiles or wider and every cobbled street, or only the roads between towns?
   *Recommendation:* both: town streets and the roads between them, no trails.
5. **Mood's other uses:** should mood also change work (a miserable wolf works slower) or ties (doc 48)?
   *Recommendation:* its voice and standing only, for now.
