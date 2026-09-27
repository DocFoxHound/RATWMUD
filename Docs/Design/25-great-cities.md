# 25 — The Great Cities: Ridgemere and Ser Ferro

The western world's two great cities, designed on the world canvas by `tools/worldgen/cities.py` and peopled by
`tools/worldgen/citizens.py`. Both are regenerated with the rest of the western world:

```
cd tools
python3 -m worldgen.western --preview DIR --out DIR   # world + cities + people (~5 minutes); validates the result
python3 -m worldgen.western --import-dev              # replace every generated cell (w_), interior, door, resident
                                                      # and route (rm_, sf_) in DEV; everything else is kept
python3 -m unittest test_cities                       # the kit, holdings, names
RATW_SLOW=1 python3 -m unittest test_cities           # + the whole world built, validated and checked (~5 min)
```

Previews: `artifacts/cities/` — `ridgemere_region.png` (the city, its quay and the five estates),
`ridgemere_old_city.png`, `grayrock_estate.png`, `ser_ferro.png`, `ser_ferro_palace.png`.

## Ridgemere (Ridgemere Heights, North West)

A Seattle-like, medieval-industrial harbour city where it always rains (its cells' weather is rain).

- **The walls enclose only the old city**, a rough oval on the harbour: the **government quarter** on the landward
  side (the Rain Hall, where the Council of Houses sits; the Magistrate's Court; the Watch House; the Gaol; the Hall
  of Tallies), the **market** square in the middle with its well and stalls, the **Chapel of the Tide**, and **the
  Sump**, the slums by the Quay Gate: sixteen crowded tenements on narrow lanes, the Customs House, the Sump
  Infirmary, the Soup Kitchen and the Drowned Lantern. Three gates: the Quay Gate (harbour), the South Gate (the
  Ridgemere Southway) and the East Gate (the Accord Road and the Houses' roads).
- **The quay**, outside the walls between them and the water: a street along the wall and one along the water, the
  Brinewater Yard (shipwrights), the Vesk smokehouses and Fish Hall, the Houses' stores (iron, timber, glass), the
  Harbourmaster's Office, the Tarred Rope, drying racks and piers out over the harbour.
- **The great Houses own the land outside the walls.** Each has a walled estate (its own road to the East Gate) with
  its seat (a manor: great hall with the House's banners and high seat, the family's chambers above, kitchens and
  servants' bunks below), stables, workers' cottages, gardens on the manor's side and a working yard by the gate, and
  its industry:

  | House | Industry | Estate |
  |---|---|---|
  | Grayrock | quarry and ironworks | Grayrock Hall; a stepped quarry pit with a cart ramp, the Ironworks, the Quarry Office, the bunkhouse |
  | Fell | timber | Fellhall; the water-driven Fell Sawmill and millpond, the log yard, the loggers' bunkhouse |
  | Ashcombe | charcoal and glass | Ashcombe House; the Glassworks, the Kilnhouse, charcoal kilns smoking in the yard, Burners' Row |
  | Brinewater | ships and rope | Brinewater House; the Ropewalk (and the Brinewater Yard on the quay) |
  | Vesk | fish, smoke and salt | Vesk Manor; the Salt Store (and the smokehouses and Fish Hall on the quay) |

**People (136):** each House's family (Grayrock: Lord Aldric, *always travelling* — he walks the Grayrock Road and
the Accord Road to Upper Accord's Main Gate and back, camping on the road; Lady Maren, *always at home*, keeping
Grayrock Hall; their heir Tobias at the ironworks and daughter Ingrid; Lady Isolde Fell and her son Corwin; Lord
Edric Ashcombe, who still blows glass himself; Lady Petra Brinewater and her consort Leif; Lord Magnus Vesk), each
seat's steward and servants, and the Houses' workforces (forgehands, quarrymen, sawyers, loggers, glassblowers,
charcoal burners, shipwrights, ropemakers, smokers, salt rakers) — some lodge on the estates, the rest walk out from
the Sump each day. The Chancellor, the magistrate and the city's officers; the City Watch (captain, wall walk and
quay patrols day and night, gate guards); shopkeepers and taverners with their households; the Sump's dockhands,
washerwomen, rag-pickers, beggars and children.

## Ser Ferro (The Ser Ferro Marches, South)

A bright city of white walls and red roofs on the great river, built in **tiers** that climb away from the river and
toward the east. No tier meets another more than a step apart; white retaining walls line each edge and stairs cross
wherever a street does.

1. **The wharf** (along the river): warehouses, the Fish Market, the River Office, and the only slums in the city —
   seven crumbling tenements. Piers run out through water gates in the river wall.
2. **The lower town** (west): the middle-class city — some fifty family houses, bakers, butchers, potters, weavers,
   carpenters, smiths, coopers, vintners and more, the Three Bells, the Golden Sheaf Inn, the Muddy Oar, the Wool
   Guild Hall, the City Guard Barracks, the House of Saint Chiara, and the market square.
3. **The Cathedral Rise** (centre): the **Cathedral of the Iron Saint** (nave, altar under the saint's statue, crypt of
   the Princes) on its marble square with fountains and statues, the Chapter House and the Clergy House, finer shops
   (apothecary, scribes, cartographer, chandler), more houses and the Sunlit Cup.
4. **The Heights** (east): the elite — six palazzi (Valmonte, Lucenti, Aldobrandi, Orsenna, Castellane, Marenzi)
   among gardens, the Merchants' Guildhall, a jeweller, a bank and a silk merchant.
5. **The palace terrace** (the top, east): the **Palace of Ser Ferro** (the Court with its twin thrones, the Royal
   Apartments above, the Palace Kitchens below), the Hall of Petitions, the Palace Guardhouse, the Chapel Royal, and
   the palace gardens (lawns, clipped hedges, fountains, statues) behind a marble forecourt.

**People (141):** Prince Aurelio di Castellane, Princess Livia, the heir Cassio, Serafina and young Matteo; the
Chancellor Ottavia Lanza, herald and courtiers, the palace cook and household, the Captain and the palace guard; the
Archprelate Benedetto Albani, priests, choirmaster, sexton, acolyte and scribe, and the sisters of Saint Chiara; the
elite families and their servants; the guildmasters; shopkeepers, taverners and their families; painters, musicians,
flower sellers and market traders on the squares; the city guard (captain, three patrol routes day and night, gate
guards); bargemen, fishers, porters, laundresses, a beggar and the wharf children.

**Factions (11, with claims on their buildings and cells):** the Council of Houses, the Ridgemere Watch, Houses
Grayrock, Fell, Ashcombe, Brinewater and Vesk; the Crown of Ser Ferro, the Church of the Iron Saint, the Ser Ferro
Guard and the Merchants' Guild.

## What the game needed

- **More residents:** the world now holds 434 (Upper Accord 157, Ridgemere 136, Ser Ferro 141); the society, the
  loader, saves, Atlas and the host allow **1024** (`MaxResidents`, `MAX_PEOPLE`).
- **Travellers:** a civilian with a route now walks it through their working hours and rests on the road wherever the
  day ends (`RatwResidents.cpp`); guards still patrol, merchants still keep shop. Atlas offers a civilian a "Travel
  route" beside a guard's "Patrol route".
- **Checkpoints survive a changed population.** A server restarting on a world that has gained or lost residents
  used to refuse its checkpoint and stop. Now residents who remain keep their lives and money, newcomers start fresh
  with their authored purse (minted), departed residents' coins return to the treasury (so money stays conserved),
  and a resident whose saved home or spot was rebuilt goes back to their authored bed. Malformed records still
  reject the whole checkpoint.
- **New tiles:** red tile roof, whitewashed wall, furnace, waterwheel, lumber stack, cinder yard.

## How it runs (DEV revision 6, build 10)

| | |
|---|---|
| World | 105 cells + 398 interiors = 503 places, 6.64 M tiles, 72,428 doors and seams; 434 residents, 12 routes |
| `world_check --simulate 7 9` | tick mean 8.8 ms, p99 17 ms (budget 50 ms); p99.9 42 ms without cell loads; everyone reaches their places |
| Game server | restores the old checkpoint into the grown world; ready at once; streams ~420 of 503 places (people are everywhere) |

**Known limits:** a traveller's first post is chosen by the time of day, so on a fresh start Lord Grayrock may cut
across country once to reach it before following the road post to post. The occasional slow tick (~0.7 s worst) is a
cell being loaded as residents spread out; loading places off the game thread is the next step if that matters.
