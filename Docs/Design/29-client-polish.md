# 29. The browser client, polished: smooth movement, a new screen, looking, talking, weather, the map, creation

Planned 2026-10-02. Seven problems found playing the browser client (doc 27), worked through in phases, one commit
each. The towns and NPC conversations agreed the same day are doc 30.

| Phase | What | State |
|---|---|---|
| 1 | Smooth movement | Built 2026-10-02 |
| 2 | A new screen: HTML panels around a larger canvas map, an In Sight list | Planned |
| 3 | Mouse-over: what is under the cursor, in words | Planned |
| 4 | Talk targets: choose who you are speaking to, several at once | Planned |
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
