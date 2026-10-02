# 30. A livelier world: the towns built out, and NPCs who talk to each other

Planned 2026-10-02, with doc 29. Two phases, numbered with doc 29's (the order agreed): **5**, the empty towns built and
peopled and the cities filled out; **6**, NPCs talking to each other from a large written library of scenes, with no
language model at runtime.

| Phase | What | State |
|---|---|---|
| 5 | Towns built out; about 950 residents | Built 2026-10-02 (DEV revision 7, build 12) |
| 6 | NPC-to-NPC scenes: an engine and a large written library | Engine built 2026-10-02; library growing |

## Phase 5: The towns built out, the cities filled (built 2026-10-02)

**Before:** DEV held 434 residents, all in Upper Accord (157), Ridgemere (136) and Ser Ferro (141). The western
generator had left Cinderbrook, Saltreach, Lakeside, Westmarch, Accord Crossing, Fenhollow, Amberford, Hollowmere, the
three fortresses and the Ghost Town as walled boxes of empty ground. The simulation counts a place as a town only with
five homes and a merchant, so there were three.

**`tools/worldgen/towns.py`** (`cd tools && python3 -m worldgen.towns [--dry-run] [--only …] [--preview DIR]`):

- **Add-only.** It reads DEV, builds each settlement on a copy of its cell, and saves with the optimistic revision,
  after backing the world up to `artifacts/backups/` (`…_before_towns_….atlas.json`). Everything added carries the
  place's prefix (`tn_ac_`, `tn_sr_`, … `tn_df_`); a place whose prefix is in use is skipped. Only the settlement's own
  ground and the fields outside its gates are changed.
- **Edited ground is refused.** A tile inside the wall that is not natural ground (or the generator's roads and bridges)
  means someone built there: the place is refused and the edits listed.
- **Inside the wall** is the largest dry piece of the box that isn't wall or gate (a shore town's wall stops at the
  water; its box also holds strips of beach outside).
- **The plan:** two streets across the place through a square with a well (and market stalls in the towns), a street
  from each gate, a road just inside the wall, and a middle lane in quarters deep enough for three rows. Buildings come
  from the existing kit (`buildings.py`, `site.py`): the important ones nearest the square, houses where they fit, each
  door on a street. Each place has its style: Ridgemere slate at Saltreach, Ser Ferro whitewash at Cinderbrook, timber
  on the frontier, the Order's stone in the fortresses.
- **What each place is:** written out in `TOWNS`: an inn or tavern, its trades (a forge town, a salt-and-fish town, a
  caravan stop, a lake town of boatwrights, peat cutters and herbalists, a grain town at a ford), a chapel, a watch
  house or toll post, a granary or store, and the work outside its walls (fishers on the shore, farmers in the fields,
  miners and charcoal burners beyond the gate). Places near a city (Accord Crossing, Saltreach, Cinderbrook) are smaller
  and lean on it; remote ones keep more trades. The fortresses hold barracks, a commander's hall, a mess hall and an
  armoury; the Ghost Town stays ruined, with two squatters' shacks.
- **Farms outside the walls:** a fenced field (crops, or salt pans on the coast) with its farmstead, only on ground a
  wolf can walk to from the gate; a farm whose door can't be reached is taken away again.
- **Every door is walkable** from a gate by the game's own step rules, or the place is refused.
- **People** (`populate`, on `citizens.Folk`): the watch (or garrison) sleeps where it serves and walks a route between
  the gates and the square; households fill the houses: a head with the next job (keepers of every shop and inn, a
  server, the chapel's keeper, the healer, the storekeeper, the work outside), a partner with work if any is left, then
  children and elders in the beds left. Names, quirks and histories are each place's own. Interiors take the place's
  ID as their region, so each place is its own town to the simulation.
- **The cities filled out** (`fill_city`): about seventy more each (`uax_`, `rmx_`, `sfx_`) in free house and tenement
  beds (never inns, infirmaries, gaols or barracks): apprentices and errand runners at the shops, porters, street
  sellers, messengers, washers, a lamplighter, beggars near where others already work outdoors, and children and elders.
- **Re-running the western generator** replaces the `w_` cells these stand in: `western.assemble` now drops the `tn_`,
  `rmx_` and `sfx_` content with the cities' and calls `towns.build_all` to build it again on the new ground.

**Result on DEV (revision 7, build 12):** 940 residents authored (966 in the simulation with the cities' families), 14
towns (every settlement but the Ghost Town), 13 caravans on the road between them, 141 new interiors.

| Place | Buildings | People |
|---|---|---|
| Accord Crossing | 14 | 28 |
| Saltreach | 14 | 28 |
| Cinderbrook | 15 | 29 |
| Westmarch | 20 | 40 |
| Lakeside | 19 | 42 |
| Fenhollow | 16 | 36 |
| Amberford | 20 | 38 |
| Hollowmere | 6 | 12 |
| The Ghost Town | 2 | 4 |
| Each fortress | 4 | 13 |
| Upper Accord, Ridgemere, Ser Ferro | (existing) | +70 each |

**Performance.** `world_check … --simulate 7 7.2 --players 20` at first: mean 18.2 ms, steady p99 58 ms, nearly all of
it route searches in the half-second schedule update (up to six at once, one of 76,000 nodes taking 53 ms). Now
**residents plan routes a few a tick** (`World::planWantedRoutes`): the schedule update only notes who needs a route;
each tick plans at most one (no new one past 30,000 nodes of search in that tick), in turn from where the last tick
stopped; caravans plan at most two a turn. Measured: **mean 17.6 ms, steady p99 39.5 ms** (the 434-resident world was
38 ms), p99.9 52 ms. The people digest changes, as it must with five hundred more people.

Tests: `tools/test_towns.py` (a placeholder built and peopled with every resident in a bed of their own, the watch on
a route, the place its own region, nothing outside it changed, a second run skipped; edited ground refused); the server
tests unchanged and passing.

## Phase 6: NPCs talking to each other (engine built 2026-10-02; the library grows in batches)

How games like Skyrim do it: two idle NPCs near each other play a short **scene** chosen from a large pool by
**conditions** (place, who they are to each other, who each is, the time, the weather, recent events), each line with
**variants** and replies that **branch** on the speaker; plus one-line **barks**. That is what this is, with no model.

**The engine** (`Core/RatwScenes.{h,cpp}`):

- **Scene files** (`Data/Voice/scenes/**/*.scene`, plain text): `scene <id>`, `when key=value|value ...` (topic,
  region or a region group, place, band, time, season, day, weather, the topic's own tags such as `item=herbs`
  `dir=up` `kind=raided` `known=no`, and either speaker's `a.sex`, `b.stage`, `a.job`, `b.role`), `weight N`, then
  lines `a : words | words` and `b[old] : words`. A speaker's tagged lines followed by an untagged one form a **turn**:
  the first whose tags hold for that speaker is said, one of its `|` alternatives at random. `a?[friends] :` marks a
  turn spoken only if it holds. Blanks: `{a} {b} {subject} {claim} {news} {item} {price} {other} {place} {town}
  {weekday} {season} {festival} {victim} {a_job} {b_job}`; a scene whose blank can't be filled isn't used.
  `groups.scene` names region groups (`rain_coast = ridgemere saltreach`).
- **Strict checking at load:** unknown topics, places, jobs, blanks, regions and conditions, lines over 160
  characters, a scene without a line each speaker always says, a bark with a second speaker, a duplicate ID. Any
  error and nothing loads (the server says so and falls back to the old library).
- **Choosing:** scenes are indexed by topic; of those that fit the moment and can be filled, never one said in that
  place lately (`recentScenes_`), and **one that none of the listening players has heard** if there is any (weather
  and barks excepted). Scenes that test more about the moment weigh more (written for it, not for anywhere).
- **Jobs** in the scenes' categories (`jobCategory`): from the post's title, the work label and the description
  (smith, baker, innkeeper, guard, soldier, farmer, fisher, miner, healer, priest, scholar, carter, crafter, herder,
  noble, servant, beggar, sailor, apprentice, cook, woodcutter, official, merchant, child...).

**The world's side** (`Core/RatwAmbient.cpp`):

- **Topics** beyond gossip, news, quarrels, friends and the day: **prices** (a town's meal or herb price moved a
  tenth or more in the last day and a half: `World::priceSeen_`, from `tendPrices`), **caravan** (arrived, left or
  raided, to or from this town), **bandits** (a camp lately active, a raid), **crime** (a theft or assault in town in
  the last two days, the offender named only if anyone saw it), **life** and **newcomer** (weddings, deaths,
  apprenticeships, successions, arrests and arrivals, kept by town: `World::townNews_`), **festival** (today, or
  called for the next three days), **player** (a traveller near both: by name if they know them, else "that grey
  stranger"), and the everyday: **family**, **work**, **weather**, **lore** and **smalltalk**, with a little chance in
  their scores so the same two don't always talk of the same thing.
- **Strangers talk now** (small talk, as strangers), and the talk warms them over time. Places are busier where more
  stand about (an exchange every 60 s in a crowd, 100 s with a few, 180 s when quiet); a resident at most every 5 min.
- **The moment** (`World::sceneMoment`): place kind (street, market, tavern, home, work, chapel, barracks, gate,
  field, shore, wild, hall), the town (Upper Accord's campuses count as Upper Accord), the hour, season, day, sky.

**The game's side** (`Core/RatwGameAmbient.cpp`): a picked exchange is voiced from the scenes; only if none fits does
the old library, then the authored lines, speak. The Mind writes exchanges live only if `--ambient-model-calls N` is
given (default 0: never). **Barks:** every few seconds near a player, a resident within earshot may call out a line
(a merchant's cry, the watch, a child at play, the weather), at most one every 25 s in a place. **What each character
has heard** (up to 3,000 scenes) is kept in the save (`scenesHeard`), so a returning player hears new ones.

**Measuring the library:** `python3 tools/scene_report.py [--min N] [--region R]`: scenes by topic and region, lines
written for each kind of speaker, distinct renderings, an estimate of listening hours before an everyday repeat, and
the thin spots to write next.

Tests: `Tests/scenes_tests.cpp` (the format and every check, who says what, optional turns, blanks, groups, the
preference for scenes not heard, weights, variety, barks, job categories, the real library loading whole and covering
every topic); `ambient_tests` (strangers make small talk, friends as friends, a festival); `game_tests`
`residentsTalkFromWrittenScenes` (a scene spoken and heard, the memory surviving a restart).
