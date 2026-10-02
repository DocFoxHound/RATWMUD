# 29. The browser client, polished: smooth movement, a new screen, looking, talking, weather, the map, creation

Planned 2026-10-02. Seven problems found playing the browser client (doc 27), worked through in phases, one commit
each. The towns and NPC conversations agreed the same day are doc 30.

| Phase | What | State |
|---|---|---|
| 1 | Smooth movement | Built 2026-10-02 |
| 2 | A new screen: HTML panels around a larger canvas map, an In Sight list | Built 2026-10-02 |
| 3 | Mouse-over: what is under the cursor, in words | Built 2026-10-02 |
| 4 | Talk targets: choose who you are speaking to, several at once | Built 2026-10-02 |
| 7 | Regional weather: moving systems that fade with distance, driving the simulation | Built 2026-10-02 |
| 8 | A real minimap: true positions and sizes, coloured by terrain | Built 2026-10-02 |
| 9 | Character creation: layered coats and markings, uploaded portraits a DM approves | Built 2026-10-02 |

(Phases 5 and 6, towns and NPC conversations, are in doc 30.)

## Phase 1: Smooth movement (built 2026-10-02)

Walking hitched because the client redrew everything every frame and did its decoding on the page's own thread:

- **The ground is drawn once, offscreen** (`Client/src/game/terrainLayer.ts`). The tiles on screen and a margin of 8
  around them go into an offscreen canvas; each frame copies it, snapped to whole device pixels so glyphs stay crisp.
  It is drawn again only when what it shows changes: the ground, what the wolf can see, the light (in fiftieths), the
  wolf's own height, the glyph style, or the view walking off its margin. Tile styles (colours as CSS strings, glyph
  widths) are worked out once per kind of tile, not per tile per frame.
- **Unchanged parts stay the same objects.** The snapshot sections the server leaves out come back as the very same
  arrays (`net/sections.ts`), so `applySnapshot` keeps their decoded forms (rows, visibility, heights) and the ground
  is not redrawn. `json.arr` returns one shared empty list for the same reason.
- **Decoding off the page's thread** (`net/decodeWorker.ts`): inflating and parsing run on a worker; messages are handed
  on strictly in arrival order. Without workers (Node, the headless client) the same code runs inline.
- **What the wolf can see is sent as changes** (`Core/RatwSections.cpp`, `rowDelta`): against the version the client
  is known to hold, as row edits `{"$delta": key, "edits": [[row, column, text]]}`, when under half the size. A client
  without that base asks for everything, as for any missing part.
- **The player's own wolf answers at once.** Others are still drawn a tenth of a second in the past, between real
  poses. The own wolf is drawn from its newest pose, carried ahead at its measured velocity for at most 0.12 s while
  moving (0.05 s otherwise), and eased toward rather than jumped to; a jump of over two tiles (a door, a correction)
  is taken at once.
- **Crossing into another cell** keeps the wolf where it was on screen and slides the view home (`cameraShift`). The
  snapshot's cell now carries its world position (`x`, `y`, `z`).
- **Smaller savings:** each post's wrapped lines are kept until it reveals more; weather patterns are made once per
  sheet; a halo hidden behind a room covering the whole map is skipped; weather sheets are made in idle moments
  ahead of need; glyphs are measured again once the game's fonts arrive.
- **Measuring:** `?perf` shows frame intervals, drawing time and ground redraws in a corner.
  `python3 tools/client_frame_smoke.py [--world MANIFEST] [--seconds N] [--lenient]` walks a development wolf in
  headless Chromium (`tools/client/frames.mjs`) and reports the game's work per frame and long frames. Headless
  Chromium paints without a GPU and caps its own frame rate near 30 a second whatever the page does, so frame
  intervals there are not the measure; the game's work per frame is (budget: p99 under 8 ms, worst under 25 ms, no
  frame over 100 ms). On the demo world the new client's drawing took 1.4 ms at the median, 5.9 ms at p99.

Tests: `server_parts_tests` (visibility deltas: a step sends only changed rows, edits stack, a missing base asks for
everything, a wholesale change goes whole, edits outside the view are refused); `net.test.ts` (deltas on the client);
`game.test.ts` (the ground drawn once and redrawn only on change; the own wolf close to its newest pose, others
further back, a long jump taken at once).

## Phase 2: A new screen (built 2026-10-02)

The fixed 1600×1000 canvas, scaled to the window, became HTML panels around a canvas that is only the map
(`Client/src/ui/hud/`):

- **Layout** (`hud.ts`, `styles.css`): a CSS grid that fills the window. A slim top bar (calendar, weather, moon, the
  day, Character / Inventory / Settings, LIVE); the **story column** on the left (`story.ts`), the main focus as
  before, widened or narrowed by dragging its edge (kept in the browser) or with Settings' four presets; the **map** in
  the middle with its header (place, what the weather does to the senses, zoom − +, local and world map) and the
  actions beneath; and on the right **In Sight** and the wolf's status (pace, stamina, senses, posture, the law).
- **The story** is real text: wrapping, selection and scrolling are the browser's. The transcript follows the newest
  post until the reader scrolls up; posts still unfold one at a time.
- **In Sight** lists every wolf the server shows, nearest first: a portrait drawn from their appearance and life stage,
  their name (sage for residents, blue for players), what they do (the post's title, or the trade they were written
  with: the snapshot's entities gained `work`), whether they are hostile, and which way and how far. Pointing at a row
  rings that wolf on the map; pointing at a wolf on the map lights its row. Clicking a row (or right-clicking) opens
  its menu at the pointer.
- **The map** fills its space at any window size (DPR-aware). A small room is drawn up to 28 pixels a tile as before;
  anything larger shows at the zoom chosen with − + or the = and − keys (14, 18, 22 or 28 pixels a tile), so a wide
  window shows much more of the world. Its corner words (wind, height, travel, turning) follow its edges. The world
  map and travel atlas keep their old drawing, fitted into the map, until phase 8 replaces them.
- **Menus, sheets and messages are HTML:** the action menu (still keys 1 to 6), the character, belongings, trade,
  settings, leaving and closer-look sheets (`dialogs.ts`, built again only when what they show changes), and the toast.
  Buttons never take keyboard focus, so WASD stays with the map; a click outside a menu closes it.

## Phase 3: Looking with the pointer (built 2026-10-02)

Glyphs are not always recognisable, so the pointer is the wolf's eyes (`Client/src/game/look.ts`):

- **What is under it**, in this order: a wolf (name, what they do, hostile, a life stage other than adult, what they are
  doing; never an exact age), a door (its name, open or closed), the herb patch (bundles left), the ground (the terrain
  catalogue's `name` and `effect`, which were generated all along but never shown, and its height against the wolf's:
  above, well above, below, a sheer drop). Remembered ground says "(remembered)"; ground never seen is "Unexplored",
  and gives nothing away.
- **Shown twice:** always on the LOOKING AT line under the map, and beside the pointer after a moment's rest (off in
  Settings: "Pointer labels"). Pointing at a name in In Sight shows that wolf on the line too.
- Only what the client already holds is used: nothing is asked of the server, so nothing hidden can be learned.

## Phase 4: Talk targets (built 2026-10-02)

Before, an NPC answered only when its name was in the line; "Talk" sent one canned greeting; any NPC in the world that
caught its name answered at once, all together, and nothing said who a reply was for.

- **Choosing:** click a resident's row in In Sight (again to let go), or "Talk" in their menu (the server checks they
  can see and hear each other, then answers `{"type": "talkTarget"}`; nothing is said for the player). Up to four.
  They stay chosen until they leave sight, are clicked off (the chip's ×), or Esc is pressed on the map. Chosen wolves
  have a dashed gold ring on the map and a gold edge in the list.
- **Above the composer:** "TALKING TO Ash × Bram ×", or, with no one chosen, "SPEAKING TO Ash (nearby)" when exactly
  one resident is close, else "no one in particular".
- **Who answers** (`Game::command`, "chat" with `targets`): only residents in the speaker's place are considered
  (never the whole world); those addressed are the chosen ones who can hear it, in the order chosen, then anyone named
  (and a companion asked for their thoughts); with no one chosen and no one named, the one resident within six tiles
  who hears it clearly, if there is exactly one and the post is speech. Nobody else answers.
- **In turn:** several addressed answer one after another (`TalkChain`, `continueChain`); each later one is told what
  the earlier ones just said. A chosen wolf who can't hear is reported ("Ash is too far away to hear you.").
- **Thinking:** a resident shows "..." from the moment they take up a line until they answer.
- **For whom:** speech events carry `to`, each listener told as they can tell ("you", a name they can see, or
  "someone"); the transcript shows "ASH → you" (in gold), "ASH → Bram".

Tests: `game_tests` `talkTargets` (only the chosen answer, the player's words name them, two answer in order, a named
wolf joins in, out of earshot is told, Talk chooses without speaking); `server_smoke` (the one close resident answers
"→ you"); `game.test.ts` (choosing, sending, letting go when out of sight and with Esc).

## Phase 7: Regional weather (built 2026-10-02)

Weather was one value per cell, rolled for each cell on its own every six game hours: neighbours didn't agree,
nothing moved, and rain stopped at a cell's edge. Now there is a field of **weather systems** (`Core/RatwWeather.cpp`):

- **The world's own weather is a pure function of the seed and the calendar.** Every six game hours each outdoor cell
  may give birth to a system, by its **climate** and the season: wet (the rain coast: rain, storms), marsh (fog,
  rain), cold (snow, fog), arid (sandstorms) or temperate. The climate is read from the cell's region name (coast,
  isle, moor, peak, steppe, barrens, fen, mire...), so no data or migration was needed. A system has a kind, a reach
  of 140 to 520 tiles, a strength, a life of 7 to 30 game hours, and drifts east on the prevailing wind (with a little
  north or south). It gathers over its first sixth, holds, and fades over its last third; its strength falls smoothly
  from its middle to its edge. The same moment always has the same weather, after a restart too.
- **A point's weather** is the strongest system over it (and the next strongest where two meet). Fog gathers in low
  ground. A cell set by hand (development presets, the DM's weather action) is **pinned**, all of it at full strength,
  as before. Every 15 game minutes the systems are worked out again, and each cell left to the seasons shows the
  weather at its middle (for its name, the schedules, the old views); this replaced the per-cell forecast loop.
- **The senses follow the weather where each wolf is:** `World::environmentAt(cell, position)` scales sight, hearing,
  scent, movement and the light by the strength there; sight radius, movement, hearing and scent pass the wolf's own
  position. Walk out of the rain and the rain stops mattering. (Schedules still read the weather at a cell's middle.)
- **Fronts** can be called up: the DM's new `weather_front` action (kind, middle in world tiles, reach, heading,
  hours) and, for developers, `{"type": "front", "value": "rain"}` (a 45-tile squall over the player, already grown;
  Settings has buttons). Called-up fronts are saved (`weatherFronts` in the checkpoint); the world's own aren't.
- **The client** gets the weather where the wolf stands (`cell.localWeather`: kind and strength, shown as "LIGHT
  RAIN", "HEAVY SNOW") and a coarse grid of the field over the cell (`cell.weatherField`: a letter for the kind and a
  digit for the strength every 16 tiles). Each kind in view is painted on an offscreen canvas and cut down by a soft
  mask of its strength (the grid scaled up smoothly), so rain thins toward a front's edge and gives way to another
  kind where two meet. The tint, splashes and lightning follow the strength where the wolf stands. The spoken
  description says so too ("Here it is only light, at the edge of it.").
- **Cost:** with 966 residents and 20 players, mean 19.1 ms (from 17.6) and steady p99 40.7 ms (from 39.5).

Tests: `weather_tests` `regionalField` (strongest at a front's middle and weaker away from it, the senses suffering
more in the thick of it, drift, a front saved and restored, a pinned cell, the same systems in two worlds at the same
moment, the grid); `server_parts_tests` (the DM's front; an unknown kind refused); `pace_tests` (rain set by hand
pins its cell); `game.test.ts` (local weather and strength in words, the field parsed, a malformed one refused).

## Phase 8: The map of the country (built 2026-10-02)

The old "minimap" was the World Map's nearby view: every place a fixed 206×145 box, placed by dividing its offset by
the current cell's size (so everything further piled up at the edges), interiors stacked on each other, and a
17×5 sample of glyphs in one colour. Now (`Client/src/game/minimap.ts`):

- **True positions and sizes:** every known place at the wolf's height is drawn where it lies in the world and as
  large as it is, north up, centred on the wolf, **one pixel a tile in its ground's colour** (the terrain catalogue's
  colours). Seen ground is bright, remembered ground dim, unexplored ground dark. Each place's picture is made once per
  change of what the wolf remembers of it; the wolf's own place is drawn from its rows (at most twice a second).
- **On top:** the doors of the wolf's place, the wolves it can see (talk targets ringed), the wolf as an arrow facing
  its way, and the weather field, faintly.
- **The minimap** sits at the top of the right column (2 pixels a tile to start; the wheel zooms it; a click opens the
  World Map). **The World Map (M)** uses the same drawing, filling the map, with places named: it opens fitted to the
  known places, the wheel zooms (0.25 to 8 pixels a tile) and Shift or Ctrl with the wheel pans. KNOWN ROUTES (the
  travel atlas) is unchanged.
- **The server** (`World::snapshot`) lists every known place outdoors within 512 tiles at the same height, not only
  the cells through a door; the current cell's remembered glyphs are no longer sent (the client has its rows).

Tests: `game.test.ts` (places drawn at their true offset and size around the wolf, a place never seen not drawn);
`travel_tests` (the nearby map includes a visited place a few cells away).

## Phase 9: Character creation and uploaded portraits (built 2026-10-02)

**A richer appearance** (`Core/RatwAppearance.h`, `wire::readAppearance`; the client's `readAppearance`): beyond the
nine fields every appearance has, optional `coat`, `gradientTint`, `markingTint` and `eyes` ("#rrggbb"), `build`
(lean, average, heavy) and up to six `markings`, each `{mask, color, opacity}` with the mask from a fixed set (socks,
stockings, blaze, face mask, cape, chest bib, pale belly, tail tip, ear tips, muzzle freckles, brindle, merle, old
scar, eye patches, saddle). Only choices made are written, so older characters and every resident keep their nine
fields; anything else is refused.

**One stylised wolf, drawn in code** (`Client/src/ui/wolfArt.ts`) replaces the recoloured painted sheets. It is built
from its parts (far legs, tail, body, neck, head, muzzle, near legs, ears), outlined as one shape. Species sets the
proportions (the maned wolf's long legs and ears, the arctic wolf's stocky frame and full tail, the Ethiopian wolf's
long muzzle); sex and build change the frame; youth means a big head on short legs, age a greying muzzle. Markings
are painted within the parts they belong to, so they line up on every wolf; the old patterns (saddle, mantle,
piebald) are markings too. The coat gives way to the belly colour as far as the gradient's strength says. The same
drawing serves the creator, the character and closer-look sheets, and the In Sight list (cached by look and age).

**The creator** (`frontDoor.ts`): a live preview beside tabs: **Body** (species, sex, build, stature), **Coat** (48
natural coats or any colour, for the coat and for the belly and legs, and how far the belly colour reaches),
**Markings** (add up to six, each a shape, a colour and a strength; reorder; remove), **Eyes**, and **Name & age**;
and **Randomise**. The review lists the choices before creating.

Tests: `wire_tests` `appearanceV2Tests` (written only when chosen, read back, bad colours, builds, fields, seven
markings, unknown masks and strengths refused, older appearances unchanged); the front-door browser tests.

**Uploaded portraits** (`Core/RatwArtwork.*`, `Core/RatwGameArtwork.cpp`, `Client/src/ui/artwork.ts`). The plan had a
`POST /api/artwork` route that decoded the file on the server with a vendored image library. That was changed so no
image parser runs on the server at all. The browser decodes the picture, crops the middle square and scales it to
256×256. It then sends the raw RGBA pixels over the signed-in WebSocket, as `artwork_upload` in eight base64 parts
(each well under the 64 KB command cap), in order, finished within two minutes. The server checks the parts add up
to exactly 256×256×4 bytes and encodes the PNG itself (zlib, no metadata). It keeps the SHA-256, so nothing from the
uploaded file survives. An account may upload ten in a day, and a character's newer upload replaces one still waiting.
Each image is an `art-…` ID. File worlds keep them in a folder beside the save (`<save>.art/`, with
`index.json`); database worlds keep them in `game.artwork` (migration 0027, the PNG as base64).

**Who sees it.** The newest portrait of a character that hasn't been rejected is the character's portrait. Entities,
the roster, the closer look and `self` carry its ID only when it is approved, or when the viewer owns it, so the owner
sees theirs at once and everyone else sees the drawn wolf until a Dungeon Master approves. The client fetches an image
once by ID (`artwork_get`, refused for anyone else's pending or any rejected one) and keeps it. The closer-look sheet
has **Report**, which sends an approved portrait back to pending and hides it again.

**Review** (`tools/dungeon_master.py` `artwork`/`review_artwork`; `Editor/src/dm/ArtworkPanel.tsx` in the Players
workspace). The DM sees what is waiting, reported first, with the image. Approving or rejecting with a reason
queues `artwork.review` on `dm.actions`. The game applies it within a second (`Game::reviewArtwork`), stores the
decision, and tells the owner if they're on. File worlds have no DM queue, so they review from tests and code only.

Tests: `game_tests` `uploadedPortraits` (parts, order, size, ownership, the daily limit, replacement, who sees the ID
and the image, report, review); `server_parts` `artworkTests` (the PNG encoder against a decoder, folder and memory
stores, safe IDs); `test_dungeon_master` `ArtworkTests` (the queue, a queued decision, bad input, viewers refused).
