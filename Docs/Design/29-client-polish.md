# 29. The browser client, polished: smooth movement, a new screen, looking, talking, weather, the map, creation

Planned 2026-10-02. Seven problems found playing the browser client (doc 27), worked through in phases, one commit
each. The towns and NPC conversations agreed the same day are doc 30.

| Phase | What | State |
|---|---|---|
| 1 | Smooth movement | Built 2026-10-02 |
| 2 | A new screen: HTML panels around a larger canvas map, an In Sight list | Built 2026-10-02 |
| 3 | Mouse-over: what is under the cursor, in words | Built 2026-10-02 |
| 4 | Talk targets: choose who you are speaking to, several at once | Built 2026-10-02 |
| 7 | Regional weather: moving systems that fade with distance, driving the simulation | Planned |
| 8 | A real minimap: true positions and sizes, coloured by terrain | Planned |
| 9 | Character creation: layered coats and markings, uploaded portraits a DM approves | Planned |

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
