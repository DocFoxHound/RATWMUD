# Runs Against the World — MVP Execution Plan

**Status:** Finalized MVP baseline — implementation authorized; delivery evidence tracked in `Docs/IMPLEMENTATION_STATUS.md`  
**Date:** 2026-09-21  
**Companion document:** `VISION.md`  
**Planning principle:** Prove the distinctive experience in a narrow vertical slice before building world breadth.

## 1. MVP Objective

Build a networked, persistent RATW vertical slice in which two players can enter a small living world, navigate an ASCII-glyph tilemap, roleplay through text, interact through contextual controls, and encounter an NPC whose schedule, memory, and generated dialogue remain grounded in authoritative game state.

The MVP is successful if it proves all of the following together:

1. An ASCII tilemap can provide useful spatial awareness without replacing prose-driven imagination.
2. The centered `W` plus orbiting facing marker is immediately readable during movement and conversation.
3. Separate MUD-like cells can contain meaningful tile-based local spaces and connect cleanly.
4. Text roleplay, graphical interaction, multiplayer state, and perception rules can share one authoritative simulation.
5. A scheduled, remembering NPC can feel continuous without being allowed to invent game state.
6. Unreal's development and deployment burden is acceptable for the experience it provides.

This is an engine-decision MVP and a product-experience MVP. It is not a content-complete game or a public early-access release.

## 2. Confirmed and Provisional MVP Decisions

The gameplay decisions below incorporate the completed clarification discussion. Remaining implementation choices use the documented defaults and are recorded for morning review in `Docs/MORNING_QUESTIONS.md`. They do not block this development slice. Milestone checkboxes represent verified implementation work, not design approval. This is an initial/current development checklist, not a production-readiness certification; newer follow-ups distinguish implementation from their integration evidence.

### 2.1 Engine Direction

- Begin in Unreal Engine using a C++ project with Blueprints available for presentation and content wiring.
- Use an authoritative dedicated server from the start rather than converting a client-hosted prototype later.
- Pin Unreal Engine **5.8.2**, Linux x64, changelist **56702186**, as observed in the installed engine's Build.version.
- Keep world rules and persisted data independent from rendering APIs so the vertical slice can be reproduced in Godot if Unreal fails the engine gate.
- This installed distribution contains Editor and Game targets but lacks a packaged Server target. Develop and test a separate authoritative headless process through `UnrealEditor -server -nullrhi`; a distributable dedicated-server binary remains a separate source-engine build gate. See `Docs/Design/00-architecture.md`.
- **Superseded 2026-09-28:** the game left Unreal. The server is our own (`Server/ratw_server.cpp` over `Core/`) and the client is a web page (`Client/`); see `Docs/Design/27-browser-client.md` and ADR-007 in `Docs/Design/00-architecture.md`.

### 2.2 The Map Is a Logical Glyph Tilemap

- Each cell owns a rectangular logical grid for the MVP.
- Every terrain tile selects an ASCII glyph plus semantic style data.
- Fixtures, entities, perception, weather, selection, and UI are separate render layers above the terrain.
- A map is not generated as one large text string. Glyphs are individually addressable tiles or instances, which enables clicking, fog of war, color, height cues, and effects.
- Terrain state remains tile-based while entities use continuous local coordinates above the grid.

### 2.3 Wolf Token and Facing Marker

Every wolf is represented by two graphical glyph layers centered on one continuous world position:

1. An upright `W` fixed at the wolf's local position.
2. A literal `>` glyph that orbits the `W` and rotates to point outward in the facing or travel direction.

```text
conceptual cardinal snapshots

    ^
    W          W>          W          <W
                            v
  north       east        south       west
```

The graphical client does not swap the source marker for separately drawn arrow art. It renders the `>` glyph as a small sprite or atlas glyph, rotates that layer, and moves it around an invisible token-space ring.

Conceptually:

```text
marker_position = wolf_center + facing_vector * orbit_radius
marker_rotation = facing_angle
```

Additional rules:

- The marker has no collision or terrain occupancy of its own.
- It never erases or changes the terrain glyph.
- The `W` stays upright while the marker rotates.
- Accepted movement updates travel direction; an explicit face command can update facing without moving.
- An idle wolf retains its last accepted facing.
- A plain-text compatibility view substitutes `^`, `>`, `v`, and `<` when arbitrary glyph rotation is unavailable.

### 2.4 Identity Colors

Use semantic color roles rather than hard-coded meaning scattered through UI code:

- **Self:** controlled player character.
- **Player:** other human-controlled wolves.
- **NPC:** simulated wolves.

Selected, targeted, hostile, injured, hidden, speaking, and party-member states are additional outlines or marks. They must not replace the identity role. A high-contrast accessibility mode must distinguish the three roles without relying on color alone.

### 2.5 Movement and Cells

- Movement is authoritative and continuous within a cell.
- Wolves use continuous local coordinates and a collision radius rather than snapping to tile centers.
- The client predicts or interpolates the composite `W` and marker between authoritative server snapshots.
- Terrain tiles supply collision, movement cost, height, cover, and perception properties sampled by the continuous simulation.
- Reach, doors, and exits use continuous geometry aligned to the underlying terrain grid.
- Crossing an authored exit transfers the entity to a separately stored cell.
- Cells support variable width and height: interiors may be smaller, while outdoor and overland cells may be significantly larger.
- Use **32×24 terrain tiles** as the standard reference cell for common content, readability tests, and perception tuning without forcing every cell to use it.
- A standard cell fits completely inside the local-map pane at the default scale, and most ordinary cells should be visible at once.
- Larger cells retain the standard readable glyph scale and use scrolling; smaller cells remain centered within the pane.
- Scrolling changes the viewport only and cannot bypass current/remembered perception state.
- A terrain tile is a spacious terrain/drawing patch, not an entity slot or character-sized movement square.
- Wolves, fixtures, and effects use continuous sub-tile positions, and the `W` token occupies only a minority of a tile's width at the initial default scale.
- Collision, line of sight, and intelligent pathing use continuous or finer-grained geometry rather than treating the visible glyph grid as the only navigation resolution.
- Multiple wolves may occupy one terrain tile at different continuous positions.
- A wolf's soft-collision core is much smaller than its visible `W` plus orbiting-marker footprint.
- The `>` facing marker and other purely visual marks never participate in collision.
- Very close wolves receive a mild separation/steering correction rather than rigid-body blocking or substantial pushing.
- Player bodies are soft dynamic obstacles for pathing and must not be able to form an impassable wall across ordinary paths, doorways, or cell portals.
- Portal traversal may temporarily relax wolf-to-wolf separation so a crowd cannot prevent a valid transition.
- Combat may later introduce separate engagement and space-control rules; the MVP movement collision does not anticipate them by making social movement obstructive.
- The MVP has three compact playable cells: a tavern, its adjoining exterior street or yard, and a small vertically connected loft/upper level used to prove Z-aware mapping.
- Each cell's authored dimensions alone define its extent and boundary transitions; the implementation does not impose a separate landmark- or scene-based rule for subdividing outdoor areas.
- NPC schedules may reference additional abstract off-screen locations that do not yet need rendered maps.

### 2.6 Local and World Map Boundaries

- The local map renders only the player's current stored cell and never stitches neighboring-cell terrain into it.
- The active world map renders the current cell plus directly adjacent cells classified per observer as **visible**, **remembered**, or **unknown**.
- A presently visible adjacent cell renders normally according to current line of sight and vision.
- Direct visual observation is the only source of world-map memory. Merely seeing into an adjacent cell earns a faint, coarse outline; physically entering the cell upgrades that memory to a more detailed dim outline.
- Remembered outlines show potentially stale topology only. They do not expose live occupants, current fixture state, or other present-time changes while out of sight.
- Glimpsed and visited memories persist permanently per character and never passively decay or disappear. New direct observation refreshes the permitted snapshot, and entering the cell upgrades and refreshes visited detail.
- Maps, directions, hearsay, and character lore do not reveal cells or upgrade their remembered detail in this interface.
- An unknown adjacent cell is absent from the world map.
- Doors, windows, boundaries, vertical openings, obstacles, elevation, weather, light, and character perception determine whether an adjacent cell is currently visible.
- The active world map does not reveal a chain of cells beyond the first adjacency level.
- A separate **Known Routes** destination mode may show cached names, bounds, and placement for all visited cells. It sends no remote glyph detail, live entities, doors, or weather, does not expose merely glimpsed/unvisited destinations, and never changes the Nearby view's first-degree rule.
- The default projection is top-down 2D.
- When a visible adjacent cell is directly above or below another visible cell, the world map places the glyph planes at their authored Z offsets and shifts to a restrained isometric camera.
- A remembered vertical-cell outline by itself does not trigger isometric mode; the relevant vertical relationship must currently be visible.
- When no visible vertical stack remains, the world map returns to top-down 2D.
- Isometric mode is informational only; local movement and contextual interaction remain in the current cell's overhead local map.
- Provide a non-isometric accessibility alternative with explicit layer, above, and below labels.

### 2.7 Control Modes, Pathing, and Facing

- **Navigation mode** is the default state during map play.
- `WASD` supplies direct continuous movement input.
- A normal click on reachable terrain submits a move-to destination for intelligent pathing.
- The authoritative server computes or validates the path against terrain, elevation, doors, fixtures, and dynamic blockers.
- The path can be recalculated or stopped when authoritative world state invalidates it.
- Click-to-path never auto-opens, unlocks, forces, enters, or otherwise activates an interactable.
- A route blocked by a closed door may approach the usable side, then stops and reports/highlights the blocking object.
- The player must invoke **Open** through the contextual menu or equivalent command before a route can cross the doorway.
- If the opened door connects areas within the same cell, the ordinary local route stays cancelled and the player must issue a new movement command. An explicitly selected world journey may resume after Open, as specified in 2.13.
- If the door is a transition portal into another stored cell, the explicit **Open** action also performs the validated cell transition after any required posture preparation. A new move, stop, or posture command cancels a pending crossing.
- An already-open or unobstructed boundary exit automatically transitions cells when direct or click-path movement crosses the authored map edge.
- A blocked transition portal never auto-opens merely because the player clicked or moved toward the edge.
- Every portal defines an authored connection anchor and matching arrival anchor in the destination cell.
- Transition places the wolf exactly at the destination arrival anchor, clears velocity and click-path state, and stops movement.
- Movement held during the transition is latched off until released/reissued so it cannot carry the wolf away from the connection point.
- Exception for an explicitly selected world journey: preserve the high-level destination, stop at the anchor for 0.25 seconds, then issue the next local leg. Ordinary WASD/local-click crossings still require new input.
- The local viewport recenters on the arrival anchor after entering an oversized cell.
- While moving, facing follows the accepted velocity or current accepted path segment.
- When movement stops, the last facing is retained.
- Holding `Alt` while stationary shows a faded candidate `>` toward the mouse on the local map; preview alone never changes authoritative facing.
- Alt-click commits the face-point command, turning through the shortest arc at an initial 180 degrees/second without translating the wolf. `Ctrl`-click remains a compatibility shortcut.
- New movement cancels a manual turn; type mode suppresses the facing preview and navigation bindings.
- Movement from sitting first requires a 0.65-second rise to standing. Movement from lying first requires a 0.45-second rise to crouching, then proceeds at 30% normal speed.
- Crouching persists when stopped until deliberately changed. `/stand` takes 1 second from lying, 0.5 seconds from crouching, or 0.65 seconds from sitting. Sitting/lying down remain immediate in this slice.
- Repeated held input does not restart preparation. Held input or a click path resumes after it; releasing/stopping cancels travel while the posture change may finish.
- Explicit portal actions cannot bypass these delays. Stable posture and sneak/hearing skills persist, while queued movement, portal crossings, manual turns, and preparation timers do not resume after reload; an interrupted rise resolves to its target posture.
- All rates and delays above are initial tuning values, not animation requirements or final balance.
- In navigation mode, pressing `Enter` opens or resumes **type mode** and suppresses movement bindings while the player writes.
- Entering type mode stops ordinary local-click movement, but an explicit world journey continues through writing and panels. Releasing held input or changing focus must not accidentally cancel that journey.
- In type mode, unmodified `Enter` sends the current post and returns immediately to navigation mode.
- `Shift`-`Enter` inserts a newline so the composer supports substantial multi-paragraph roleplay.
- `Escape` exits type mode without sending, returns to navigation, and preserves the unfinished draft.
- Re-entering type mode restores the preserved draft at its prior editing position.

### 2.8 Roleplay Composer, Speaking Color, and Presence

- The composer is a multiline roleplay editor, not a short-message field.
- Draft text is client-local editing state until the player sends it; mode changes do not transmit it.
- Each player character selects one speaking color from a curated palette of approximately 32 choices.
- Store a semantic palette ID on the character rather than arbitrary client-supplied RGB values.
- Each palette ID maps to tested light- and dark-theme swatches so text remains legible.
- The speaking color is separate from the Self/Player/NPC token-identity color.
- The narrative feed renders speech using the selected speaking color. It shows the character's name only when that listener is permitted to identify the visible source.
- While a player is actively entering text in the in-character composer, visible eligible observers see a small bubble containing `...` above that wolf in the speaking color.
- Typing presence transmits only an ephemeral active/inactive state. Draft contents never leave the composing client.
- Typing presence clears after a short inactivity timeout, leaving type mode, sending, or disconnecting and is not persisted.
- As soon as the server accepts a post containing speech, a map marker in the speaking color indicates that the wolf spoke; it does not wait for that post to reach the front of a viewer's reveal queue.
- The speaking marker remains for approximately three to five seconds and then fades. Use four seconds as the initial tuning value.
- Exact speech-marker artwork and fade curve remain presentation-tuning decisions.

### 2.9 Roleplay Post Grammar and World Flow

- The default in-character composer accepts narration, quoted speech, and whitelisted slash commands in one post.
- The server parses a sent post into an ordered list of typed segments rather than rebroadcasting executable raw text.
- Segment kinds begin with speech, narration, transient action, and persistent state change.
- `/action …` and `/pose …` create authored action/pose segments.
- Shorthand verbs such as `/sigh` create bounded transient action segments.
- `/sit`, `/lay`, and `/stand` request validated posture changes.
- `/me …` sets a short declared current-state description visible in the player's UI and to other players through inspection.
- Slash commands come from a server-owned allowlist; arbitrary command names or client-authored state mutations are rejected or treated as text according to a documented escape rule.
- All segments retain one post ID, author, speaking color, audience, and server order.
- The narrative pane progressively reveals the ordered segments inside one continuing speaker-owned flow entry instead of creating a new entry for every segment.
- Each viewer maintains a presentation queue ordered by authoritative server sequence.
- Only one in-character roleplay post reveals at a time. Later posts wait until the active post has completely revealed and never interleave with it.
- Reveal speed is a per-viewer presentation preference and includes an instant setting.
- Different reveal settings may place viewers at different presentation points, but they retain the same post order.
- Progressive reveal and its queue never delay, reorder, or grant authority to state changes. The server commits the validated post and any allowed state change as one ordered event before clients present it.
- A mixed post receives the in-world spoken marker when it contains at least one speech segment. Action-only posts do not claim that the character spoke.
- A separate OOC channel is scoped to the current cell, visually distinct, excluded from in-world speech indicators, and excluded from Social XP qualification.
- Full post text and progressive reveal appear only in the narrative pane. The map shows only the color-matched `...` typing indicator and a brief speaking indicator—never the post body.

### 2.10 Spatial Speech, Action, and Senses

- In-character speech is a spatial perception event rather than an automatic cell-wide broadcast.
- Every speech segment has a voice level: **whisper**, **speak**, or **yell**.
- **Speak** is the default voice level.
- With normal hearing and an unobstructed acoustic path, normal speech begins fully clear and remains clear for roughly half the fixed acoustic span established by the standard reference cell.
- Beyond the clear radius, comprehension falls progressively rather than switching off at one boundary.
- A whisper has a short clear radius and is intended for nearby listeners.
- A yell has a larger clear radius and can propagate through acoustic portals into neighboring cells.
- Each cell supplies acoustic scale, ambient noise, surface, and portal-transmission data.
- Distance, walls, doors, elevation where relevant, weather, ambient noise, and portal attenuation modify transmission.
- Each listener has hearing sensitivity modified by innate ability, hearing skill, age, conditions, and localized ear injury. Implemented skill tuning uses `hearing * earHealth * (1 + 0.75*hearingSkill/100)`, with skill in 0–100.
- The server calculates a separate perceived post for each listener at the moment of speech.
- Words that the listener fails to understand are replaced with `...` while preserving the post's speaker-owned flow and visible punctuation where practical.
- Masking is deterministic for the post/listener pair so reconnect, replay, or UI speed cannot reroll comprehension.
- The client receives only its listener-specific perceived text, never the hidden words.
- Moving closer later does not restore words missed when the speech event occurred.
- Hearing speech without seeing its source produces an anonymous voice label rather than the character's name.
- Anonymous speech retains the speaker's validated color as a recurring similarity-of-voice clue.
- The listener client does not receive a hidden character name or stable entity identifier that would defeat anonymity through inspection.
- Speaking colors are not unique, so color supports player inference without acting as authoritative identity confirmation.
- Local OOC bypasses the acoustic simulation because it is not in-world sound, while remaining scoped to the current cell.
- Initial whisper/yell ranges and degradation curves are tuning values; normal speech at half-cell clarity is the baseline to preserve.

Visual action rules:

- Action and pose narration is delivered according to visual perception rather than automatically to the whole cell.
- Line of sight, distance, illumination, cover, fog/weather, vision sensitivity, age, and eye injury modify action clarity.
- With ordinary vision and no obstruction, action detail is initially clear across roughly the same half-cell reference used for ordinary speech.
- If an observer receives another perceptible part of the same post but cannot make out an action segment, that segment becomes `···`.
- `...` consistently means lost spoken words; `···` consistently means missing visual action or movement.
- If neither the actor nor any part of the event is perceived, the server omits the event instead of sending a placeholder that leaks hidden activity.
- Shorthand action definitions may declare additional sensory channels. For example, a sigh may have an audible cue even when its body movement is unseen.
- The server resolves each segment independently, then assembles one listener-specific version of the original ordered post.

Sneaking and movement-sound rules:

- Crouching is the persistent stealth posture. Its initial close-visibility cutoff is `(7 - 4*sneakSkill/100) * (observerSightRange/27)` terrain units, with skill in 0–100. Existing line of sight, vision, eye health, and weather still apply.
- Outside that cutoff, actor snapshots, map indicators, visual actions, inspection, and actor interaction discovery do not reveal the source. Hearing an actor does not grant visual access.
- Movement sounds are calculated only for actual movement within the same cell. Initial clear radii are 6 units normally or `2.5 - 1.7*sneakSkill/100` while crouching, multiplied by the observer's effective hearing, weather, and occlusion factors. Clarity falls to zero at twice the resulting clear radius.
- A heard-but-unseen moving player may cause an anonymous pawstep cue at most once every three seconds per listener, without source identity, position, or map token. The initial runtime does not emit routine NPC footstep cues.
- Provisional pending user clarification: sneaking reduces movement noise but does not automatically attenuate intentionally selected whisper/speak/yell volume. Hearing skill affects both sound channels. A full skill-training progression system is not implemented by these stat hooks.

Scent and wind rules, added to the implemented slice:

- Sight, hearing, and smell are independent. Scent alone neither supplies otherwise hidden speech/actions nor changes whether an actor can be visually identified or inspected.
- Current body scent comes from other player and NPC wolves in the same stored cell. Sneaking does not reduce body odor, and the observer never receives a cue for their own scent.
- Observer scent sensitivity is `smell * noseHealth * (1 + 0.75*scentSkill/100)`, with nose health in 0–1 and scent skill in 0–100. These are persisted stat hooks, not a completed injury/training system.
- Wind direction describes airflow: east is zero radians and south is `pi/2` in downward-positive map Y. West-to-east air can carry a western source's scent toward an observer to its east; the source cue then points roughly west/upwind.
- Outdoor cells hold a normalized base strength from 0–1, base heading, and optional deterministic variation. Default outdoor wind flows east at base strength 0.5 with smooth gusts/shifts. Indoor air remains calm. Saved base wind and world time preserve the variation phase across restart.
- A normal nose in clear weather has initial calm/cross/upwind full clarity through 1 terrain unit, fading to zero at 3. Directly downwind at strength 0.5, full clarity reaches 6 units and fades to zero at 18. Scent skill, nose health, rain, and snow scale reach; detailed formulas are in `Docs/Design/02-perception-maps.md`.
- Scent follows bounded connected air paths, not visual line of sight. Opaque walls and closed doors block it, open routes can bend around obstacles, low furniture permits air, and diagonal sealed corners cannot leak. This is a gameplay approximation with a 64-unit search/range cap, not fluid simulation.
- An unseen source contributes only a broad 45-degree sector, a 1–3 strength category, and whether wind carried it. Multiple sources in a sector aggregate. No source identity, exact position/distance, speaking color, count, or player/NPC category is transmitted.
- The local map uses fixed-radius lavender arcs and `~~` around the observer, not markers on hidden source tiles. Text status and **Smell** provide the same broad directional information. A cue is not an actor click target and disappears when no longer justified or when its source is seen.
- Scent never earns or refreshes visual map memory and never exposes adjacent cells. "No scent detected" is not a declaration that no other wolf is present.
- Live body scent is implemented; lingering tracks, individual scent recognition, deposited marks, indoor drafts, and cross-cell airflow are explicitly deferred.
- Clear/rain/fog/snow are implemented weather states. Shared environment factors drive sight, movement, hearing, scent and the on-screen conditions display. Rain/snow affect all four; fog strongly reduces sight and has a provisional 5% scent bonus. Wind drives precipitation/scent and masks sound, but does not push wolves. The saved four-hour calendar adds gradual dawn/dusk and lunar night light, provisionally 8% at new moon through 40% at clear full moon before weather modifiers; darkness alone leaves ears and nose intact. Reduced motion preserves static weather and airflow information. Interior shelter is independent of authored artificial light/daylight access: sealed unlit rooms are dark, lit taverns glow at night, and bright daylight suppresses unnecessary glow. Whole-cell lighting has a provisional 8% close-awareness floor; individual lamps/shadows remain deferred.

### 2.11 Authority and AI

- Clients submit intentions; the server validates and applies them.
- The server owns position, facing, perception, inventory, doors, NPC schedules, relationships, social rewards, and persistence.
- The dialogue model may propose NPC prose only.
- Generated text cannot directly grant items, complete quests, change relationships, move entities, spend money, award XP, create canon, or reveal facts outside the NPC's allowed knowledge.

### 2.12 Separate Two-Tier Authoring

- Use **Atlas Workshop**, a separate local-browser map editor launched with `python3 tools/map_editor.py serve`.
- Paint the total world canvas continuously, including terrain and sparse elevation overrides, before or after deciding its cell boundaries.
- Default cuts use the existing **32×24** reference size; custom rectangular cuts, filled-rectangle merges, and horizontal/vertical splits are supported.
- Preserve global terrain and anchor positions through cuts. Existing links/spawn are reassigned to their new cells; conflicting metadata, slivers, blocked arrivals, and same-cell portal conversions fail without partial edits.
- Drill down into a selected cell for detail work, or create an independent detached room and link it through reciprocal doors, passages, or stairs.
- Authoring continuity does not alter player continuity: local maps remain current-cell-only, crossings retain anchored stops, and doors require the established action. Only the separately selected world-travel feature can continue after a brief arrival pause or show its cached Known Routes index; authoring itself grants no map knowledge.
- Save portable authoring JSON and export validated content snapshots containing separate `.cell` files plus `world.ratw`. Load custom content explicitly through `-RatwWorld=/absolute/path/to/world.ratw`.
- The first editor uses rectangular atlases/cells, each at most **256×256** tiles, with at most **256 combined world cells and detached rooms**. Larger-world tooling, NPC/item placement, and legacy `.cell` import are not implemented.
- Custom-save identity is a hash of the manifest **path**, not its content. Topology revisions require a fresh export directory or a fresh explicit `-RatwSave` path; neither hot reload nor save migration is implied.
- Political authoring adds faction/Chapter catalogs and each cell's region, overlapping faction claims, and optional Chapter site. Splits inherit these fields; incompatible merges/recuts fail atomically. Claims are provisional cell-granular metadata, not control, construction rights, or player map knowledge.

See `Docs/Design/12-map-editor.md` and `Docs/EDITOR_CONTRACT.md` for the implemented workflow, validation contract, and limits.

### 2.13 Pace, Stamina, and Remembered World Travel

- Wheel over the local map or Page Up / Page Down adjusts an integer pace from 0 to 10; the segmented pace strip is also clickable. Chat/modal guards prevent accidental changes. Shift-wheel pans vertically and Ctrl-wheel horizontally; prose scrolling remains independent.
- Dexterity is server-owned and bounded 0–100. Walk remains 2.6 tiles/s; sprint cap is `5.2 + .052*dexterity`, with linear intermediate notches. Labels are walk 0, trot 1–5, run 6–8, sprint 9–10.
- Recovery runs continuously at +5 stamina/s up to 100. Actual movement has gross drain `15*(effectivePace/10)^2`, so a full sprint nets −10/s and a middle trot can recover while traveling. Count elapsed time once, not once per waypoint; blocked input, passive bumps, posture preparation, and portal teleport distance do not spend sprint effort.
- At zero stamina, exhaustion limits movement to walking until 20, preserving the selected pace. Crouching always uses walking pace with the existing 0.30 multiplier. Terrain, weather, and rise delays remain authoritative. Values are playtest tuning, not final balance.
- Show named gait, amber sprint emphasis, requested versus server-confirmed pace, posture/exhaustion limits, stamina percentage, and actual recovery/drain. Private stats are owner-only; no full skill-training system is implied.
- Known Routes selects a destination **cell**, not a remote tile. All route cells must be visited and both reciprocal portal endpoints observed; maps/hearsay and unseen shortcuts cannot grant routes.
- Plan remembered inter-cell connections, then rank local exits and use the existing precise local navigation. Do not consult unseen remote live door/weather/terrain state to advertise a route. Changed or blocked local conditions can pause/replan the journey.
- Closed barriers require explicit Open; after that action the active world journey may resume, including through a same-cell barrier. Already-open stairs/passages can be entered as part of the explicit journey. Arrival still stops at the connection anchor, with a 0.25-second pause before an intermediate leg resumes.
- World travel continues during chat and panels. Nonzero WASD, a local destination, Stop/Wait, navigation Escape, or posture/facing commands cancel it. Closing a modal or leaving chat with Escape does not cancel. Invalid replacement destinations preserve existing travel.
- Persist dexterity, stamina, requested pace, and exhaustion with validated ranges; clear transient rates and all route intentions on restore, without offline recovery or offline travel.

See `Docs/Design/13-pace-and-world-travel.md` for the implemented component contract.

## 3. MVP Scope

### Included

| Capability | MVP proof |
|---|---|
| Desktop client | Packaged development client with adjustable split narrative/map layout |
| Dedicated server | Headless authoritative server accepting at least two simultaneous clients |
| Local map | Current cell only, with a tavern, exterior, and small upper cell rendered as layered ASCII tilemaps using the 32×24 reference |
| World map | Nearby current/adjacent view with established memory and visible vertical-stack rules; separate Known Routes selector containing cached visited-cell geometry without remote live state |
| Wolf tokens | Centered `W`, orbiting/rotating `>` marker, three identity color roles |
| Movement | WASD/click paths, gradual Alt facing, timed rises/crouching, dexterity-scaled pace, continuous stamina recovery, sprint/exhaustion, collision, explicit doors, anchored transitions and remembered multi-cell journeys |
| Perception | Independent sight/hearing/smell, skill-sensitive stealth and movement sounds, anonymous directional body-scent cues, open/closed air barriers, remembered tiles, and unseen state |
| Elevation | At least two height levels, a ramp or step, and height-aware visibility/movement |
| Weather | Clear/rain/fog/snow, persistent wind and accelerated daylight clock, stronger map-only weather/day/night effects, shared sensory/movement factors, and protected indoor shelter |
| Narrative | Long-form mixed roleplay posts, queued progressive flow, spatial whisper/speak/yell, local OOC, 32 speaking colors, typing presence, and a spoken marker |
| Interaction | Clickable entities, anchored contextual verbs, and general actions such as Listen and Smell |
| Character surface | Native login/owned-character selection/creator and shared configurable pixel-art wolf portraits on own sheet and visible-character inspection; map remains glyph-only |
| Inventory | Small inventory/equipment panel using item icons plus text labels |
| Multiplayer | Two players see validated movement, facing, speech, emotes, and relevant state |
| NPC population | Six scheduled NPC records, with only nearby NPCs running active spatial behavior |
| Conversational NPC | One primary NPC with grounded generated dialogue and persistent memory |
| Party conversation | One recruitable NPC demonstrating address detection and selective interjection |
| Social tracking | Thin server-validated roleplay session tracking and visible Social XP/level |
| Persistence | Characters, cell state, NPC state, memories, and ledger events survive restart |
| Authoring follow-up | Separate Atlas Workshop: continuous terrain/elevation painting, grid cuts, rectangular merge/split, drill-down editing, detached rooms, reciprocal links, JSON source and validated runtime exports |
| Political/operator follow-up | Atlas claim/site metadata plus a separate trusted-local Storykeeper with campaigns, event approval, observed activity, political records and finite resident migration previews; see M15–M16 for boundaries |
| Testing | Automated rule tests plus a repeatable two-client vertical-slice test |

### Explicitly Deferred

- a large connected world;
- final combat, injury, death, and balance systems;
- working Gifted or Quickened unlocks;
- the complete Story, recap-consent, and recognition systems;
- Chapter construction and land governance;
- a full economy, crafting, or trading simulation;
- production account services, billing, or public matchmaking;
- mobile and console clients;
- player-upload moderation for custom portraits;
- animated map characters or equipment displayed on the map;
- generated dialogue for every NPC;
- lingering scent trails, unique scent recognition, deposited scent marks, and cross-cell airflow;
- production-scale administration and live-operations tooling;
- autonomous migration, physical Chapter construction/jobs, armies, brigands, assassinations, faction-collapse executors, and native gameplay consequences for stored opinions;
- production-scale world editing, NPC/item authoring, legacy `.cell` import, and live topology/save migration; the bounded Atlas Workshop is implemented separately.

Deferred systems still receive clean data seams where the MVP would otherwise create obvious rework. They do not receive speculative implementations.

## 4. Target Player Walkthrough

The completed MVP should support this uninterrupted path:

1. Two development accounts connect to the dedicated server and select separate Normal wolf characters.
2. Both enter the Bent Bough tavern and see the same authoritative cell from their own perception.
3. Each wolf appears as a centered `W`; its `>` marker circles and rotates as facing changes.
4. The controlled character, the other player, and NPCs are distinguishable at a glance.
5. Players use WASD and click-to-path movement across the glyph-tiled floor, encounter collision, climb the raised hearth, and see visibility respond to obstacles.
6. Clicking a door presents **Inspect**, **Listen**, **Knock**, and **Open**. Choosing an action writes its result into the narrative stream.
7. The players write substantial posts that mix narration, quoted speech, and inline slash actions. A color-matched `...` bubble shows who is composing on the map; the resulting segments progressively fill one speaker-owned entry only in the narrative pane at the viewer's chosen speed.
8. Posture/state commands update the player's state panel and become visible when another player inspects that character.
9. A local OOC exchange remains visibly separate and produces no in-world speech marker or Social XP.
10. The players test whisper, ordinary speech, and a yell. Distance and ear injury cause listener-specific `...` gaps, while the yell crosses the exterior portal with reduced clarity.
11. In the exterior, one wolf sneaks beyond visual detection upwind of the other. The observer receives a broad anonymous scent arc and **Smell** result without a token or identity. Reversing the development wind changes detection; rain and nose/scent skill also affect reach. An indoor sealed door blocks a separate nearby-scent test.
12. The tavern keeper responds in character, remembers a prior promise, and cannot fabricate a reward or state change.
13. A recruited NPC joins the conversation, answers when addressed, sometimes interjects when appropriate, and otherwise remains quiet.
14. The players open the line of sight to an upper loft; the world map shifts from top-down to isometric while the local map remains current-cell only.
15. The players leave through the door into the adjoining exterior cell. The world map updates its adjacent visible set.
16. After a server restart, characters, door state, NPC schedule/memory, qualifying social events, sensory stats, and base wind remain intact; saved world time preserves gust continuity while fresh cues are recomputed.

## 5. Technical Shape

### 5.1 Runtime Boundary

```text
Unreal clients
  narrative UI · glyph map · portrait · icons · input
          |
          | intentions / perceived events
          v
Unreal dedicated server
  commands · cells · movement · facing · sight/hearing/scent · wind/weather
  chat audience · NPC schedules · relationships · social ledger
          |
          +---- persistence adapter ---- SQLite for MVP
          |
          +---- dialogue adapter ------- configured model or deterministic fallback
```

Only the server talks to persistence and the dialogue adapter. Clients never submit final state or direct database mutations.

Storykeeper is a separate trusted-local operator surface, not a privileged player
client. An opt-in `-RatwDMDirectory=/absolute/private/path` bridge exports bounded
omniscient snapshots and accepts typed, expiring requests. The Python service
owns a separate planning/political SQLite store and never opens the game save.
Only native authority validates and applies world effects. See
`Docs/DM_BRIDGE_CONTRACT.md` and `Docs/DM_SERVICE_CONTRACT.md`.

### 5.2 Simulation and Presentation Separation

The server simulation uses IDs, continuous local positions with server-defined precision, collision radii, height, normalized facing, and explicit state. It samples the terrain grid but has no dependency on map colors, fonts, icon assets, animation duration, or panel layout.

The client presentation converts perceived state into:

- glyph and style instances;
- smooth movement between accepted states;
- the facing-marker transform;
- map visibility and remembered-state treatment;
- narrative entries and interaction menus;
- portrait and inventory panels.

This boundary is required for deterministic tests and for an honest Unreal-versus-Godot comparison if needed.

### 5.3 Rendering Layers

Render the local map in a stable order:

1. terrain glyphs;
2. height and environmental modifiers;
3. fixtures and interactable objects;
4. tracks, scent, sound, and supernatural perception overlays;
5. wolf base glyphs;
6. facing markers and temporary state marks;
7. fog, remembered-space treatment, and weather;
8. selection, contextual actions, labels, and accessibility overlays.

The MVP may use a custom UMG/Slate widget backed by a glyph atlas. Paper 2D may be evaluated during the rendering spike, but persistent world data must not be authored exclusively inside an experimental tile-map asset format.

### 5.4 Conceptual Data Contracts

Names may evolve, but the following boundaries should exist before UI work expands:

**Cell definition**

```text
cell_id, version, dimensions, world_origin, z_offset, tiles, portals,
world_units_per_tile, ambient_light, weather_exposure,
acoustic_reference, authored_description, base_wind_direction,
base_wind_strength, wind_variable
```

**Tile definition/state**

```text
x, y, terrain_code, glyph_code, elevation,
collision, cover, scent/sound modifiers, fixture_ref
```

**Entity state**

```text
entity_id, entity_kind, cell_id, local_position, velocity,
soft_collision_radius, facing_angle, movement_state, posture,
sneak_skill, hearing_skill, scent_skill, smell_sensitivity, nose_health,
dexterity, stamina, requested_pace, exhausted,
appearance_ref, interaction_ref
```

Transient simulation state additionally includes a turn target, a turning flag, posture target/remaining preparation, input/path intent, a high-level world journey, pending portal crossing, and the last-step stamina rate. Intentions/rate are not resumed from persistence; the saved posture resolves an interrupted rise to its target, and no offline stamina refill is awarded. Only observer-authorized presentation fields cross the network, not another actor's private statistics, hidden route, or movement intention.

**Character presentation**

```text
character_id, display_name, speaking_color_id,
portrait_ref, permitted_profile_fields
```

**Ephemeral typing presence**

```text
character_id, active, audience_scope, expiry
```

Typing presence never contains draft text and is never persisted.

**Roleplay post**

```text
post_id, author_id, channel, speaking_color_id,
ordered_segments, audience_scope, server_sequence, timestamp
```

**Roleplay segment**

```text
segment_kind, display_text, action_id,
voice_level, validated_arguments, resulting_state_ref
```

**Listener sensory profile**

```text
character_id, base_sensitivity, age_modifier,
left_ear_condition, right_ear_condition,
vision_sensitivity, left_eye_condition, right_eye_condition,
smell_sensitivity, nose_health, scent_skill, temporary_modifiers
```

**Perceived roleplay post**

```text
post_id, listener_id, perceived_segments,
hearing_clarity, visual_clarity, perceived_source_handle,
source_label, source_color_id, source_direction, server_sequence
```

**Perception snapshot**

```text
observer_id, world_revision, visible_tiles,
remembered_tiles, perceived_entities, effective_cell_wind,
movement_heard, scent_cues[{sector, strength_category, windborne}]
```

Scent cues aggregate unseen wolves into eight broad directions, never source IDs or locations. Their fixed-radius presentation does not encode distance. Visual map knowledge remains independent. Persist the cell's base wind plus world clock, not a transient effective gust sample or stale cue.

**Adjacent-cell knowledge**

```text
observer_id, current_cell_id, adjacent_cell_id,
visibility_state, familiarity_level,
remembered_outline_revision, relative_xyz
```

`visibility_state` is `visible`, `remembered`, or `unknown`; `familiarity_level` is `none`, `glimpsed`, or `visited`. A remembered record contains only the last permitted topological snapshot suitable for its familiarity level. Only direct sight can establish `glimpsed`, and only entering the cell can establish `visited`; map items and secondhand information do neither. The record persists permanently without passive decay, but it is not a subscription to current cell state. Later direct observation refreshes the stored snapshot.

**Command envelope**

```text
command_id, actor_id, expected_revision,
verb, target_id or local_point, arguments
```

**World event**

```text
event_id, world_revision, event_type,
authoritative_payload, permitted_audience, timestamp
```

**NPC memory**

```text
npc_id, subject_id, memory_type, source_event_id,
summary, confidence, importance, visibility_scope,
conversation_id, consolidated_at, timestamps
```

**Active NPC interaction memory**

```text
conversation_id, npc_id, participant_ids,
bounded_active_context, started_at, last_activity_at,
consolidation_due_at, consolidation_status
```

Active memory supplies detailed context for the current conversation or interaction. Exactly **one hour after the last interaction**, it is consolidated into a compact permanent summary. A new interaction resets that inactivity deadline. Generated summary prose cannot replace or mutate authoritative quest, inventory, injury, relationship, or world state.

**Social ledger entry**

```text
event_id, account_id, character_id, session_id,
reason_code, bounded_amount, validation_metadata, timestamp
```

Stable IDs, revisions, and idempotency are MVP requirements rather than later hardening tasks.

### 5.5 Intended Repository Shape

(The original intent, kept for the record. Today: `Core/` (the game), `Server/` (its server), `Client/` (the browser
client), `Editor/` and `DM/` (the tools' web apps), `tools/`, `Data/`, `Database/`, `Tests/`, `Docs/`.)

```text
RATWMUD.uproject
Source/RATWMUD/
  Core/
  World/
  Networking/
  UI/
  NPC/
  Social/
Content/
  UI/
  Fonts/
  Icons/
  Portraits/
Data/
  Cells/
  NPCs/
  Items/
Config/
Tests/
Docs/
VISION.md
PLAN.md
```

Unreal-generated and local-only directories must be excluded from Git. Binary assets that belong in source control should use Git LFS from the beginning.

## 6. Milestone Sequence

Work proceeds in order. A milestone is complete only when its exit gate passes; partial UI demonstrations do not substitute for the authoritative behavior beneath them.

### M0 — Toolchain, Repository, and Engine Gate Foundation

**Goal:** Produce the smallest reproducible Unreal client/server project.

- [x] Inventory the installed Unreal version, compiler, SDKs, and dedicated-server build capability.
- [x] Decide whether the installed distribution is sufficient or a source build is required.
- [x] Record the pinned version and build commands in `README.md`.
- [x] Create a C++ Unreal project without starter content.
- [x] Add Unreal-aware `.gitignore`, `.gitattributes`, and Git LFS patterns.
- [ ] Add development, test, client, and server build targets.
- [x] Establish a minimal automated test target.
- [ ] Add structured logging with world revision, entity ID, and command/event IDs.
- [x] Record architecture decisions for engine choice, authority, tile simulation, and persistence.

**Delivery evidence / remaining work:** The installed Unreal 5.8.2 module builds, portable/engine tests execute, and two real clients connect to a separate editor-hosted headless authority. A Linux Game package also passes the walkthrough, persistence scenarios and two-client networking against a separate packaged headless listen host. Editor, Game and Server target definitions exist; the optimized Server target is explicitly unsupported by this engine distribution. A full structured command audit log and dedicated-server distribution gate remain open. Ordinary client logs do not archive roleplay prose. The checked development items do not constitute an unconditional engine go/no-go decision.

**Development exit gate:** A clean checkout can build the Unreal game module; one client connects to a separate editor-hosted headless server and receives a versioned snapshot. **Distribution gate:** build a standalone packaged client and dedicated server using an engine distribution supporting those targets. Record these separately; editor-hosted testing does not satisfy packaging.

### M1 — Deterministic World Core

**Goal:** Simulate cells, tiles, entities, and commands without depending on the finished renderer.

- [ ] Define versioned cell, tile, portal, entity, command, and event schemas.
- [x] Implement the tavern, adjoining exterior, and compact upper/loft cells as data.
- [x] Support variable cell dimensions and validate the 32×24 standard reference size at the renderer's default scale.
- [x] Define world-units-per-tile and token-to-tile ratios that leave meaningful sub-tile drawing and movement space.
- [x] Keep navigation and collision resolution independent from the coarse visible terrain-glyph grid.
- [x] Implement a small soft-collision core independent from the visual token and facing marker.
- [x] Add gentle local separation/avoidance without strong displacement or hard player barriers.
- [x] Treat wolves as soft dynamic pathing obstacles and prevent doorway or portal body-blocking.
- [ ] Add an abstract off-screen-location type for NPC schedules.
- [x] Load and validate cell data with useful authoring errors.
- [x] Implement continuous position, collision against tiled terrain, elevation, reach, and portal transitions.
- [x] Implement facing as a normalized direction independent from movement animation.
- [ ] Define cell acoustic scale, ambient-noise, and portal-transmission properties.
- [ ] Define cell lighting, sight-obstruction, and visual-reference-distance properties.
- [x] Give portal connections authored relative X/Y/Z relationships and adjacent-cell visibility rules.
- [x] Implement direct movement-vector, move-to-destination, cancel-movement, and face-point commands.
- [x] Implement authoritative path calculation or validation and dynamic-path invalidation.
- [x] Stop pathing at a closed operable barrier without automatically invoking its verbs.
- [x] Keep same-cell routes cancelled after an explicit door-open action.
- [x] Make an explicit transition-door **Open** action transfer the actor to its connected cell after any required posture preparation, without restoring a canceled crossing.
- [x] Automatically transition across an unobstructed authored boundary exit reached by WASD or click-path movement.
- [x] Resolve every transition to its authored destination anchor and clear velocity, path, and held-input carryover.
- [ ] Validate move, face, inspect, listen, smell, knock, open, wait, and rest commands.
- [ ] Emit immutable authoritative events with stable IDs and world revisions.
- [x] Add deterministic tests for invalid movement, collisions, elevation, doors, and transitions.
- [x] Add shortest-arc stationary turns, non-restarting posture preparation, slow crouching, and cancellation of retained travel.
- [x] Apply sneak/hearing skill to visual and movement-sound detection without leaking hidden actor state or automatically changing speech volume.
- [x] Persist stable posture/skills while normalizing interrupted rises and clearing all transient travel/turn intentions on load.

**Delivery evidence / remaining work:** Portable behavioral assertions cover continuous movement, smoothed quarter-tile A*, crowd passage, door semantics, exact transition anchors, and atomic restore rejection. Follow-up coverage includes gradual facing, sitting/lying preparation, retained/canceled input, delayed portal crossings, skill-sensitive stealth, movement sounds versus speech, and safe posture persistence. The native three-cell walkthrough also passes. Remaining work includes complete versioned content/wire schemas, abstract off-screen locations, authored lighting/acoustic fields, a full Rest/sensory command model, and durable immutable event storage; current acoustic values are prototype tuning. Current test counts and native evidence belong to the test report.

**Exit gate:** A headless automated scenario moves an entity around the tavern, changes facing, rejects blocked moves, opens the door, and transfers into the exterior with an identical result on repeated runs.

### M2 — ASCII Tilemap and Wolf Token

**Goal:** Prove the defining visual language before expanding gameplay.

- [ ] Select and license a readable monospace font or build a controlled glyph atlas.
- [x] Render terrain as individually styleable glyph tiles.
- [x] Add separate fixture, entity, effect, fog, and interaction layers.
- [x] Support continuous sub-tile placement for entities, fixtures, effects, and interaction marks.
- [x] Render `W` at the entity's continuous local position above the terrain grid.
- [x] Render the literal `>` marker as a child glyph layer.
- [x] Compute marker orbit position and graphical rotation from replicated facing.
- [x] Predict or interpolate the composite token between authoritative position snapshots without granting client authority.
- [x] Add Self, Player, and NPC semantic color roles.
- [ ] Add a high-contrast/non-color identity mode.
- [x] Add independently colored typing-bubble and spoken-marker layers above player tokens.
- [ ] Add zoom and font scaling without allowing marker drift or tile misalignment.
- [x] Fit the entire standard cell in the default viewport, center smaller cells, and scroll larger cells without changing glyph scale.
- [ ] Add a reliable recenter-on-player control after manual scrolling.
- [x] Ensure panning never reveals unperceived or unremembered map data.
- [x] Add tile/entity hit testing for pointing interactions.
- [ ] Compare a UMG/Slate renderer with a minimal Paper 2D spike and record the decision.

**Delivery evidence / remaining work:** Native captures show independently rendered terrain, upright wolves, rotated facing markers, semantic colors, weather and map layers across standard, smaller and larger cells. Rendering uses engine-bundled Droid Sans Mono/Roboto. Separate font/license packaging documentation, user zoom/font scaling, high-contrast/non-color roles, a manual recenter control, and an actual Paper 2D comparison remain open. Full zoom-extreme acceptance has therefore not passed.

**Exit gate:** A test view containing terrain, fixtures, one controlled wolf, another player, and several NPCs remains readable at minimum and maximum supported zoom. The `W` stays centered and upright while `>` orbits and points correctly through a full turn.

### M3 — Split Interface and Interaction Loop

**Goal:** Join the imagination and orientation layers into one playable window.

- [x] Implement adjustable narrative/map panes based on the retained window concept.
- [x] Add the scene transcript and composer.
- [x] Support say, emote, out-of-character/system distinction, and command results.
- [x] Size the composer for long-form and multi-paragraph roleplay.
- [x] Parse mixed narration, quoted speech, and slash commands into typed ordered segments.
- [x] Add whisper, speak, and yell selection to the composer and command grammar.
- [x] Add server-owned command definitions for `/action`, `/pose`, `/sigh`, `/sit`, `/lay`, `/stand`, and `/me`.
- [x] Add an escape rule for literal slash-prefixed text.
- [x] Render all segments from one post in one continuing speaker-owned flow entry.
- [x] Add per-viewer reveal-speed settings, including instant display.
- [x] Add a server-sequenced per-viewer reveal queue that permits only one active in-character post.
- [x] Expose unobtrusive queued-post count or backlog state without revealing queued content early.
- [x] Add a separate current-cell OOC channel with distinct presentation.
- [x] Make navigation mode the default and bind `WASD` to direct movement.
- [ ] Bind normal terrain click to a move-to request with path feedback.
- [ ] Surface the specific blocking door/object and its contextual verbs when a requested route stops there.
- [x] Distinguish same-cell doors, transition doors, and open boundary exits in interaction feedback.
- [x] Bind Alt mouse movement to a stationary faded-marker preview and Alt-click to gradual face-point behavior, retaining Ctrl-click compatibility.
- [x] Bind navigation-mode `Enter` to open or resume the composer and suppress navigation bindings while typing.
- [x] Bind type-mode unmodified `Enter` to send and return immediately to navigation mode.
- [x] Bind `Shift`-`Enter` to insert a newline without sending.
- [x] Bind `Escape` to return to navigation without sending or clearing the local draft.
- [ ] Restore preserved draft text and cursor/selection state when type mode resumes.
- [ ] Add a curated, accessible 32-color speaking palette to character presentation settings.
- [x] Apply the speaking color consistently to character text, typing presence, and the spoken marker.
- [x] Add local and world map tabs; the world tab may begin with horizontal two-cell adjacency before adding the vertical test cell.
- [x] Keep adjacent-cell terrain out of the local-map renderer under every portal state.
- [x] Open an anchored contextual menu from a selected entity or fixture.
- [ ] Add the door, NPC, and object verb sets.
- [ ] Add general Listen, Smell, Look, Wait, and Rest controls.
- [ ] Provide keyboard navigation and a command palette equivalent for every visual action.
- [ ] Return every action result to the narrative stream.
- [x] Add neutral speaking/activity marks without animating authored emotes.

**Delivery evidence / remaining work:** Passing engine UI tests cover Enter/Shift-Enter/Escape, exact draft retention/rejection recovery, navigation suppression, sequential reveal and four adjustable pane presets that preserve the draft. Screenshots and the walkthrough cover the split screen and contextual doors; two actual graphical clients delivered 16,000-character posts in both directions with speaking colors retained and a captured reveal queue. Unchecked compound items retain missing work: explicit route/blocked-target feedback, exact cursor/selection restoration evidence, contrast/theme validation for the 32-color palette, mutable object verbs, Rest, and keyboard/accessibility parity for every painted control. Draft text and the 32 color choices already work.

**Exit gate:** A single local client can complete the tavern interaction loop entirely with pointing controls or entirely with keyboard/text controls, and both paths produce the same commands and narrative results.

### M4 — Multiplayer, Perception, Elevation, and Weather

**Goal:** Make the shared spatial world authoritative and observer-specific.

- [x] Connect two clients to the dedicated server with stable development identities.
- [x] Replicate accepted position and facing rather than trusting client transforms.
- [x] Reconcile direct-input prediction and click-path presentation with authoritative movement.
- [ ] Reconcile server-authoritative soft separation without visible snapping or client disagreement.
- [x] Synchronize speech, emotes, doors, fixtures, and cell transitions.
- [x] Replicate rate-limited typing presence only to observers allowed to perceive the character and scene.
- [x] Begin typing presence from active in-character input rather than composer focus alone.
- [x] Expire typing presence after inactivity, mode exit, disconnect, or completed send.
- [x] Replicate an immediate bounded spoken-marker event on server acceptance without copying post text onto the map.
- [x] Display the speaking marker for an initially tuned four seconds, then fade it cleanly.
- [x] Replicate one validated, ordered roleplay-post event rather than timing authoritative state from each viewer's reveal speed.
- [x] Scope local OOC delivery to the current cell and exclude it from in-world indicators.
- [ ] Calculate visible tiles from light, distance, elevation, obstacles, and door state.
- [x] Distinguish currently visible, remembered, and unknown tiles.
- [x] Prevent the server from sending unauthorized hidden-entity detail.
- [ ] Add the raised hearth and height-aware movement/visibility.
- [x] Calculate acoustic path distance and attenuation through terrain, doors, and portals.
- [ ] Apply listener hearing sensitivity, age, conditions, and ear damage.
- [x] Generate deterministic listener-specific word masking with `...`.
- [x] Replace an unseen speaker's name and stable entity ID with a perception-scoped anonymous source handle.
- [x] Preserve the validated speaking color on anonymous voice entries as a similarity clue.
- [ ] Calculate action visibility from distance, line of sight, lighting, weather, vision sensitivity, age, and eye damage.
- [x] Replace an unperceived visual segment with `···` only when some other part of that post is legitimately perceived.
- [x] Omit wholly unperceived events without leaking actor identity or activity.
- [x] Support sensory tags on bounded shorthand actions such as an audibly perceived sigh.
- [x] Ensure clients receive only perceived text and cannot recover masked words from replicated state.
- [x] Propagate yelling into the connected exterior cell with attenuation.
- [x] Keep room-local OOC exempt from acoustic degradation.
- [x] Add rain presentation without obscuring the narrative pane.
- [x] Make rain reduce appropriate sight, movement, and airborne scent reach; retain sheltered indoor behavior.
- [x] Replace flavor-only Smell with authoritative live-body scent using wind, connected air paths, nose health, and scent skill.
- [x] Track outdoor base wind direction/strength and deterministic gusts, while keeping indoor air calm.
- [x] Aggregate unseen scents into broad anonymous directional cues without actor identity, exact locations, counts, inspection, or visual-memory access.
- [x] Verify independent sight/hearing/smell, reversed/cross/calm wind, weather effects, sealed/open barriers, corner routes, and cue removal in portable tests.
- [x] Verify native scent parsing, directional text, airflow labels, noninteractive cues and development wind controls; capture and inspect a real two-client scent viewport.
- [ ] Human playtest of scent readability and wind-drifting precipitation across window sizes and reduced-motion settings.
- [x] Add the three-cell world-map view with current visibility, direct-observation memory, and connection state.
- [x] Restrict the active world-map neighborhood to the current cell and first-degree neighbors.
- [x] Render presently visible adjacent cells normally, directly glimpsed cells as faint coarse outlines, visited cells as more detailed dim outlines, and unknown adjacent cells not at all.
- [x] Establish cell familiarity only through direct visual observation and entry; ensure maps, directions, hearsay, and lore cannot reveal or upgrade world-map cells.
- [x] Persist glimpsed and visited cell memories permanently per character with no time-based degradation, refreshing them only through later direct observation.
- [x] Ensure a dim remembered outline never receives live occupants, fixture changes, or other current-state information from its cell.
- [x] Render ordinary visible adjacency as a top-down 2D composition.
- [x] Add a vertical test connection and shift the world map to a glyph-based isometric view only while that stacked cell is visible.
- [x] Keep the world map in 2D when the only vertical relationship is a remembered outline rather than a currently visible stack.
- [x] Add a labeled above/below accessibility presentation without perspective tilt.

**Delivery evidence / remaining work:** Real two-client transport, movement and IC/OOC exchange pass; portable/engine tests verify hearing masks, hidden-source filtering, injuries, weather, permanent map memory and visible-only vertical projection. The three-cell walkthrough captures the loft, visible vertical overview and rainy exterior. The 2026-09-21 scent extension passes portable checks for wind-sensitive air paths, separate senses, anonymous aggregation, privacy, and atomic persistence; the ordinary portable suite now includes 383 world assertions and 42 runtime checks. All fourteen native tests pass. The editor and rebuilt Linux package pass a two-client scent scenario, including wind reversal and indistinguishable hidden/nonexistent inspection denials. The actual viewport capture was visually inspected. Current rendering includes fixed-radius scent arcs and airflow status, interpolates authority, and projects overview planes in Slate. Dedicated light fields, age/condition systems, elevation-aware visibility, a raised-hearth implementation, lingering scent/recognition, cross-cell airflow, and broader adversarial real-network perception tests remain open. Soft-collision smoothness and sensory readability still need human observation under latency.

**Exit gate:** Two clients can move, face, roleplay, operate the door, and cross cells while receiving different valid perception snapshots. Closing the door, changing height, and enabling rain produce tested changes rather than cosmetic-only effects.

### M5 — Persistence and Recoverability

**Goal:** Make the slice survive process boundaries without duplicating consequences.

- [x] Add a server-only persistence interface with SQLite as the MVP adapter.
- [ ] Add schema versioning and forward migrations.
- [x] Persist accounts/development identities and stable character IDs.
- [ ] Persist character cell, local position, facing, inventory, and appearance references.
- [x] Persist the current structured posture and declared state used by self UI and character inspection.
- [x] Persist smell sensitivity, nose health, scent skill, and base cell wind; preserve deterministic gust phase through the saved world clock and validate malformed restores atomically.
- [x] Persist the validated speaking-color palette ID while excluding ephemeral typing state.
- [ ] Persist door/fixture state and relevant cell revisions.
- [ ] Persist NPC schedule position, relationships, and memories.
- [x] Persist active NPC interaction records and their consolidation deadlines so a restart neither loses nor duplicates a pending summary.
- [x] Persist social ledger entries.
- [ ] Enforce command/event idempotency across retry and reconnect.
- [x] Add safe save checkpoints and clean shutdown behavior.
- [ ] Add backup/export suitable for development debugging.

**Delivery evidence / remaining work:** SQLite reopen, actual server restart, active-memory restoration, aged-deadline consolidation, and full core save/reload/privacy regressions pass. NPC position/activity, companion ownership, weather and doors persist. Schema v1 exists without forward migrations; inventory/appearance are currently read-only prototype presentation, relationship state is limited, and per-cell revisioning/live-save backup tooling remain incomplete. Atlas Workshop now exports authoring content snapshots, not game-save backups or topology migrations. Its custom save defaults are isolated by manifest path. Command receipts provide a bounded 256-command retry window, not the unlimited durable replay guarantee required by the full exit gate.

**Exit gate:** The server can be stopped after meaningful play, restarted, and rejoined without losing state, repeating rewards, reopening a closed door, or duplicating an interaction consequence.

### M6 — Living and Conversational NPCs

**Goal:** Demonstrate continuity, grounded conversation, and different simulation distances.

- [ ] Author six NPC records with homes, work, schedule blocks, relationships, and knowledge limits.
- [ ] Advance off-screen schedules as low-cost data events.
- [ ] Materialize nearby NPCs into the active cell simulation.
- [ ] Handle schedule interruption, blocked travel, weather response, and return to routine.
- [ ] Add simple rule-driven NPC-to-NPC greetings or exchanges.
- [ ] Author the tavern keeper's voice, goals, beliefs, secrets, and allowed knowledge.
- [x] Implement bounded dialogue context assembly.
- [x] Maintain bounded detailed active memory for each current NPC interaction.
- [x] Exactly one hour after the last interaction, idempotently consolidate active context into a compact permanent conversation or interaction summary linked to its participants and authoritative source events; reset the deadline when another interaction occurs.
- [x] Retrieve a bounded relevant set of long-term summaries for later conversations rather than replaying unlimited transcripts.
- [ ] Store structured relationship changes and selected episodic memories separately from generated summary prose.
- [x] Generate candidate dialogue through a provider-neutral adapter.
- [x] Validate all consequential actions outside the language model.
- [x] Add deterministic authored fallback responses for timeout, failure, or disabled generation.
- [ ] Implement one recruited NPC with addressed-speaker detection, turn cooldown, invitation recognition, and personality-based interjection.

**Delivery evidence / remaining work:** Six residents walk authored routines, active detail is bounded to 32 turns, and one-hour inactivity consolidation/retention is tested through restart. Recall uses recent matching context rather than relevance-ranked memory. The loopback dialogue adapter passes strict endpoint validation and six HTTP fixture cases: success, failure, malformed output, empty output, oversized output and timeout; offline fallback and callback-once behavior also pass. These fixtures verify the adapter contract, not production language-model quality. Companion invitation/following/cooldown/interjection logic exists, but complete end-to-end NPC acceptance, abstract off-screen simulation, full home/work/relationship/knowledge records, intentional NPC-to-NPC exchanges and structured promises/quest facts remain open.

**Exit gate:** The tavern keeper follows a home/work routine, carries detailed context through an active interaction, consolidates it once into permanent long-term memory after the configured interval, recognizes a returning player after restart, recalls one promise accurately, refuses or avoids unknown information, and remains playable when the dialogue provider is unavailable. The recruited NPC participates selectively rather than answering every line.

**Live-provider follow-up:** With explicit user authorization, RATW Game's existing OpenAI `gpt-5.6-luna` configuration now works through a bounded server-side bridge. Native speech matches actual generated responses, recall survives restart and permanent-memory consolidation, and a small unknown-information/state-invention sample was manually reviewed. This advances the conversational NPC gate but does not complete all of M6 or constitute production quality/security validation. See `Docs/LIVE_NPC_TEST_REPORT.md`; ordinary launches remain offline.

### M7 — Character, Inventory, and Thin Social Progression

**Goal:** Prove that identity and roleplay continuity extend beyond the immediate scene.

- [ ] Add a character sheet with appearance, profile, classification, and static portrait reference.
- [x] Allow another player to inspect the permitted sheet fields.
- [ ] Add a small inventory and equipment model with icons and textual labels.
- [x] Keep equipment off the map token.
- [x] Record validated roleplay-session metadata separately from chat delivery.
- [x] Require reciprocal participation and multiple meaningful turns for a qualifying session.
- [x] Add duplicate suppression, cadence checks, and repeated-partner decay hooks.
- [x] Award bounded Social XP through an idempotent ledger.
- [x] Display Social XP and level without implementing Gifted or Quickened promotion.
- [ ] Reserve schema concepts for Stories and Chapter membership without exposing unfinished features.

**Delivery evidence / remaining work:** Portable tests verify audience-based reciprocity, meaningful turns, duplicate/cadence controls, repeated-partner decay and one-time capped settlement; the UI displays authoritative totals. Sheet and inventory captures show explicit prototype art and a fixed read-only item list. Full appearance/portrait records, mutable equipment ownership, Stories/Chapter schema and an end-to-end network qualification/retry/restart scenario are still required. Social progression remains development-character scoped until a production account system exists.

**Exit gate:** A qualifying two-player scene produces one explainable ledger result, reconnect/retry cannot duplicate it, ordinary message spam produces none, and the result remains separate from character skill progression.

### M8 — Vertical-Slice Hardening and Engine Decision

**Goal:** Turn the assembled systems into a repeatable product test.

- [x] Build a scripted start-to-finish vertical-slice test.
- [x] Run unit tests for world, perception, persistence, social, and NPC memory rules.
- [ ] Run two-client privacy and audience tests.
- [ ] Test dialogue-provider failure, delay, malformed output, and attempted state invention.
- [ ] Test reconnect during movement, conversation, and cell transition.
- [ ] Test font scaling, high contrast, keyboard-only play, and reduced weather effects.
- [ ] Measure client frame time, server tick time, bandwidth, persistence latency, and dialogue latency.
- [ ] Simulate at least six active and fifty off-screen scheduled NPC records.
- [ ] Conduct a minimum thirty-minute two-player roleplay session.
- [x] Record confusion, writing interruption, missed actions, map readability, and development friction.
- [ ] Package the client and headless server using documented commands.
- [ ] Write the Unreal go/no-go architecture decision.

**Delivery evidence / remaining work:** The executed evidence includes portable tests (176 world assertions and 42 runtime checks), 10/10 native engine automation tests passing with exit code 0 and no warnings, separate-server/two-client networking, restart/consolidation scenarios and native screenshot views. Both graphical clients received 16,000-character posts with the selected speaking colors retained; the queued narrative UI was captured. Screenshot review produced layout/input/rendering fixes and morning questions. Provider success/failure/malformed/empty/oversized/timeout cases and adjustable-pane UI tests pass; an explicit generated-state-invention test remains open. The Linux Game package passes local, multiplayer and restart scenarios. Its separate headless listen host is not an optimized dedicated-server build. Dedicated-server packaging, full accessibility checks, performance profiling, fifty off-screen residents and the thirty-minute human roleplay session are not complete. The Unreal decision remains provisional; this milestone and the full engine gate are open.

**Exit gate:** Every consolidated MVP acceptance criterion passes, known limitations are documented, and the engine decision is explicitly recorded. A failed engine decision triggers a like-for-like M1–M4 Godot spike using the same data contracts.

### M9 — Atlas Workshop Authoring Follow-Up

**Goal:** Make geographical continuity easy to author while preserving separate runtime cells. This user-requested follow-up does not declare the unfinished M0–M8 release gates complete.

- [x] Provide a separate local-browser tool with a shared glyph canvas, sparse heights, zoom/pan, and portable JSON drafts.
- [x] Cut standard/custom rectangular grids without changing terrain; reject cuts that leave cells narrower than four tiles.
- [x] Split and merge cells safely, preserve global link/spawn anchors, and warn about retained/discarded names and descriptions.
- [x] Drill into world cells or detached rooms for fine terrain/elevation and metadata editing.
- [x] Create reciprocal doors, passages, or stairs with unique traversable endpoints and height-compatible arrivals.
- [x] Export source JSON, separately stored cell files, and a versioned world manifest; generate compatible open seams without drawing new walls along cuts.
- [x] Import custom content through `-RatwWorld`, validate before replacing runtime content, and isolate default custom saves by manifest path.
- [x] Keep exports separate from running games and old export directories; document that geometry revisions require a fresh save/export path.
- [ ] Add NPC/item placement, legacy `.cell` import, prefabs, larger-region authoring, or collaborative workflows only as separately scoped increments.

**Delivery boundary:** Rectangular world and cell/room dimensions are 4–256 tiles per axis, with at most 256 combined cells/rooms and 262,144 authored tiles. The editor has no NPC/item authoring, no legacy cell-file importer, no hot reload, and no save-topology migration. It does not claim production-scale performance. See the current test report for executed model, exporter, runtime-import, and browser evidence rather than interpreting these feature checkboxes as a full release certification.

**Exit gate:** A continuous authored map can be partitioned, refined, exported, and played as independent cells without terrain discontinuities introduced by the editor, broken reciprocal links, or changes to player perception and transition rules.

### M10 — Pace and Remembered Travel Follow-Up

**Goal:** Make sustained overland travel readable and convenient without fast travel, automatic door opening, or remote omniscience. Earlier uncompleted release gates remain open.

- [x] Add validated server-owned, owner-scoped dexterity, requested pace, stamina, exhaustion, and safe save defaults.
- [x] Integrate dexterity-scaled speed and continuous recovery/actual-movement drain without waypoint or collision accounting exploits.
- [x] Provide guarded wheel/Page Up/Page Down controls and a legible gait/stamina strip.
- [x] Offer a separate cached visited-cell destination index while retaining Nearby mapping.
- [x] Build remembered reciprocal-connection routes and execute ordinary local movement through multiple cells with brief anchored pauses.
- [x] Pause at closed barriers for explicit Open; support cancellation, rejected replacement requests, writing during travel, and no resumed route after reload.
- [ ] Human-playtest gait/energy balance, party/companion pace, and longer overland journeys.
- [ ] Profile hostile-load and production-scale route/navigation behavior before population expansion.

**Delivery boundary:** Destinations are visited cells, not remote tiles. There is no teleportation, automatic unexplored scouting, offline travel, or full training/party-speed system. See `Docs/TEST_REPORT.md` for actual executed evidence and `Docs/Design/13-pace-and-world-travel.md` for the contract.

### M11 — Visible Weather and Daylight Follow-Up

**Goal:** Make the player experience the same environmental conditions that constrain the character, while keeping the narrative readable and perception authoritative.

- [x] Centralize weather/illumination modifiers shared by terrain/actor vision, speech/pawstep hearing, scent and movement.
- [x] Add a persisted accelerated shared daylight clock with smooth dawn/dusk, strict validation, and safe legacy defaults.
- [x] Render distinct daylight/night tint, wind-driven rain/splashes, drifting snow and fog veils only over the local map.
- [x] Preserve static effects and readable condition labels in reduced-motion mode; prevent effects from creating hidden actor targets.
- [x] Protect indoor cells; limit weather/time mutation to development sessions and include local conditions in NPC dialogue context.
- [x] Add portable, native UI/wire, graphical-weather and process-restart test fixtures. Executed outcomes are in `Docs/TEST_REPORT.md`.
- [ ] Human-playtest readability, night sight and day length; settle individual lamps and storm-front priorities.

**Delivery boundary:** M13 supersedes the initial fixed weather/short clock with seasonal forecasts and lunar light. Regional storm fronts, accumulation, exposure, individual lamps/shadows, sound playback and lingering tracks remain deferred. The accelerated calendar does not change real-time NPC memory inactivity.

### M12 — Cell Atmosphere and Interior Lighting

**Goal:** Convey the current room's shelter, light and weather around its map boundary while matching actual perception.

- [x] Separate whole-cell artificial light, daylight access and warm/neutral/cool tone from shelter; validate and persist profiles with legacy defaults.
- [x] Keep a lit tavern clear at night with warm glow, suppress glow in bright daylight, and make sealed unlit interiors dark even at noon.
- [x] Anchor soft glow, darkness and weather fades to the real cell boundary; clip to the map pane and preserve reduced-motion and hidden-actor privacy rules.
- [x] Add editor metadata fields, split/cut inheritance, conflict-safe merge/recut and runtime export/import.
- [x] Add portable, native renderer/wire and isolated graphical/restart fixtures; executed outcomes are recorded in `Docs/LIGHTING_TEST_REPORT.md`.
- [ ] Human-playtest halo strength and the 8% indoor close-awareness floor; decide whether personal lights or placed lamps should be next.

**Delivery boundary:** Whole-cell ambient light, not per-tile light pools, switchable fixture lamps, window-shaped beams, shadow casting, fuel consumption or dark adaptation. Remembered terrain may remain faintly recognizable in darkness without revealing current occupants.

### M13 — Calendar, Seasons, Moon and Aging

- [x] Shared four-hour days with equal morning/evening halves and 365-day years.
- [x] Seasonal deterministic forecasts, explicit manual override, and moon/weather-dependent night light.
- [x] Persist character birthdays; award early physical/later wisdom growth and bounded age-65+ penalties without changing injury state or Social XP.
- [x] Deliver birthday notices and show dates, moon and annual character statistics.
- [x] Confirm lunar phases follow game days and player characters age while logged out; both match existing behavior.
- [ ] Confirm exact growth bands and server-downtime policy before final balance.

**Boundary:** Natural-death choice/prompts and the mandatory age-100–120 deadline are confirmed next requirements, not implemented in M13; see M17. No leap years, actual-date ephemeris, regional weather fronts or aging during server downtime. See `Docs/Design/14-calendar-aging.md` and the executed evidence in `Docs/SOCIETY_TEST_REPORT.md`.

### M14 — Deterministic Resident Life and Finite Economy

- [x] Six physically navigating residents with hunger/fatigue, food, rest, gathering, cooking, delivery, finite wages and trading.
- [x] Integer-penny purses, real inventories, stock-sensitive buy/sell quotes, demand limits and refusal of unwanted goods.
- [x] Bounded outside export orders as a recurring money source; imports as a sink; welcome grants as finite treasury transfers.
- [x] Persistent needs, accounts, goods, production progress, budgets and audited money conservation; strict atomic save validation.
- [x] Player merchant panel, consumable meals and visible gatherable patch, with authoritative range and state guards.
- [x] Exercise 30 physical game days and recovery from a stalled food chain; preserve real inventories, original daily source caps and complete money conservation.
- [ ] Balance a larger economy, author NPC jobs/resources in Atlas, choose additional source/sink levers and resolve companion needs.

**Boundary:** A two-good, six-resident demonstration, not a universal economy. Deterministic priority selection is implemented; StateTree, Smart Objects and GOAP were researched but are not installed behavior frameworks. Dialogue describes state and cannot mint funds, authorize a trade or complete a job. See `Docs/Design/15-npc-society-economy.md`.

### M15 — Atlas Political Authoring Follow-Up

**Current implementation:** faction/Chapter catalogs; cell/room region, claim
sets and Chapter sites; contested-claim overlays; inherited split metadata;
conflict-safe partitioning; validated authoring/export/native import. None of
these fields changes terrain, player perception or building rights. This is
cell-granular metadata, not legal ownership or a completed diplomacy model.

- [ ] Confirm overlapping-claim semantics, land rights and Chapter permission policy.
- [ ] Author NPCs/jobs and actual housing before treating declared capacity as a settlement economy.
- [ ] Human-review political overlays and continuity using representative authored regions.

See `Docs/TERRITORY_AUTHORING_CONTRACT.md` and
`Docs/Design/16-territory-chapters-migration.md` for the current contract;
implementation descriptions here do not replace executed integration evidence.

### M16 — Storykeeper Local Operator First Slice

**Current implementation:** separate browser/service/operator session; campaigns,
beats, Chapter profiles/members/sites, directed opinions, observed activity and
routes, fixed-UTC event scheduling, audit and explicit migration previews.
The local service has 36 passing isolated tests; native/browser end-to-end
evidence is recorded separately, not inferred from that count.

Supported native requests are announcements, weather, eligible resident
relocation and finite money/goods transfers. Requests are world-bound,
idempotent within documented retention limits and short-lived. Queued does not
mean applied; migration waits for observed arrival before source-claim resentment.
No claim means no invented faction loss. Failures never manufacture completion.

The app is read-only after ten seconds without a valid fresh snapshot. It is
loopback-only and trusted-local, with a private fragment bearer session and no
player RPC or direct game-save access. Chapter overlays and housing/jobs are
operator declarations, not buildings. The finite demo uses `demo_reach`; custom
Atlas worlds lack NPC authoring. New homes affect rest/social time while existing
jobs and food purchases remain demo commutes. Opinions currently inform the DM,
not native hostility, access, taxes or dialogue.

- [ ] Confirm provisional capacity, attraction, treaty and migration cooldown rules through playtests.
- [ ] Build actual funded settlement provisioning before enabling autonomous migration.
- [ ] Add combat/encounter/faction lifecycle components before enabling their story-beat executors.
- [ ] Define public staff authentication, roles, consent, recovery and operational retention before remote/multi-admin use.

See `Docs/Design/17-storykeeper-dm.md` and `Docs/DM_SERVICE_CONTRACT.md`. Armies,
brigands, assassinations and faction collapse remain plans with blocked executors.
These are initial development boundaries, not a production operations checklist.

### M17 — Natural Lifespan and Character Legacy (Planned)

**Confirmed:** lunar time uses game days; characters age while logged out. A
player may choose natural death before a mandatory deadline at age 100 plus
one random interval of 0–20 game years. Death prompts begin at a threshold still
to be chosen. Existing age-65 capability modifiers do not themselves cause death.

- [ ] Set reminder age/frequency, offline-deadline/final-scene policy and random interval/disclosure details.
- [ ] Add persistent lifecycle state, one server-owned deadline, strict schema/legacy handling and idempotent voluntary confirmation.
- [ ] Define account/history retention, possessions/inheritance, companions, Chapter membership and economic consequences before activating death.
- [ ] Add non-disruptive coalesced reminders and a clear voluntary-death/legacy flow.
- [ ] Enforce all dead-character action restrictions on the server; reconnect must not revive or reroll a character.
- [ ] Test exact ages 100/120, fractional deadlines, multi-year catch-up, offline/reconnect, retries, restart and no duplicate effects.

**Boundary:** no natural-death mechanic has been switched on in this update.
NPC lifecycle/replacement and combat/injury death are separate adapters. The
world calendar still pauses during server shutdown. See
`Docs/Design/14-calendar-aging.md`.

### M18 — Readable Combat Presentation (Planned)

**Confirmed:** sparse battle effects on the local map plus a collapsed combat
entry showing only the latest action; expanding reveals the entire perceived
combat log. Recommended grouping is one entry per encounter, updated in place.

- [ ] Define a minimal combat resolver, encounter lifecycle and structured observer-filtered event contract.
- [ ] Render brief attack/contact/defense/evasion cues without full prose, illustrated wolf animation, new hit targets or hidden-attacker leakage.
- [ ] Implement stable encounter rows, latest-action summaries, accessible expansion, complete permitted history via pagination and bounded UI caching.
- [ ] Preserve drafts, the single-post roleplay reveal, keyboard focus, outer/inner scroll anchors and reduced-motion preferences.
- [ ] Handle duplicates, out-of-order delivery, reconnect backfill, concurrent fights, cell changes and event/snapshot timing without replaying old effects.
- [ ] Test and capture a real two-client encounter once the resolver exists; independently choose and playtest combat pacing.

**Boundary:** design captured, not implemented battle mechanics or a working
Storykeeper encounter executor. The collapsed display must not be simulated by
discarding older combat messages. “Full” means all events the viewer was allowed
to perceive, not omniscient history. See `Docs/Design/18-combat-presentation.md`.

## 7. Consolidated Acceptance Criteria

### Visual and Map

- Terrain is a logical glyph tilemap, not a screenshot or monolithic text block.
- `W` stays centered and upright throughout movement and turning.
- The same `>` glyph rotates and orbits around `W` without consuming another tile.
- Holding Alt previews a faded candidate marker only while stationary on the local map; clicking commits a smooth bounded turn rather than snapping or moving.
- Self, other players, and NPCs are distinguishable in normal and high-contrast modes.
- Typing and spoken indicators use the speaker's selected color without changing the wolf's identity color.
- Zooming and resizing do not cause glyph, hit-target, or facing-marker drift.
- A standard cell fits at default scale; smaller cells center cleanly and larger cells scroll without exposing hidden state.
- The `W` token is visibly smaller than a terrain tile and moves continuously within and across tiles without snapping.
- Terrain-glyph boundaries do not create artificial collision or pathing steps.
- Wolf collision is substantially smaller than the visual token; the orbiting marker has none.
- Close wolves receive only a gentle bump/avoidance response, and a coordinated group cannot hard-block the MVP door or cell portal.

### World Rules

- Invalid moves are rejected by the server.
- Click-to-path cannot auto-operate a closed door; it stops at the barrier until the player explicitly invokes **Open**.
- Opening a same-cell door does not resume an old path; opening a transition door changes cells after posture preparation; crossing an open boundary exit changes cells automatically.
- Sitting-to-movement waits for standing; lying-to-movement waits for a crouch and then moves slowly. Repeated input does not restart preparation, and canceled travel never resumes by itself.
- Sneak skill reduces crouching detection distance and movement noise; hearing skill and ear injury modify acoustic detection. Hidden actors cannot be recovered through inspection or actor-action queries.
- Anonymous movement cues carry no source identity or position. Sneaking does not automatically change selected speech volume unless the user subsequently resolves that provisional rule differently.
- A wolf can see, hear, or scent independently. Sneaking does not erase scent, and nose health/scent skill alter detection without granting sight.
- Airflow direction and source-bearing cues are distinct: eastward wind can produce a west/upwind scent hint. Reversing wind changes which observer has the range advantage.
- Scent requires a connected same-cell air path; shut doors and sealed wall corners block it, while open paths can bend around obstacles.
- Anonymous scent cues aggregate sources into eight broad sectors, never hidden tokens, identities, counts, exact positions, actor interaction targets, or new visual map memory.
- Base wind and sensory stats survive restart; live scent cues are recomputed and disappear when sources leave detection. Trails and unique scent recognition are not implied.
- Stable posture and skills survive restart, but movement/turn intentions and pending portal crossings do not.
- Every transition stops at the connected arrival anchor. Ordinary navigation requires fresh movement input; an explicit world journey waits 0.25 seconds before its next local leg.
- Dexterity determines bounded top speed; eleven pace notches preserve weather/terrain/posture rules, and stamina recovery continues during real travel without per-waypoint duplication.
- Sprint, drain/recovery, posture limits, and exhaustion are identifiable in the UI; stamina cannot be supplied or refilled by a client command.
- Remembered journeys use visited cells and observed reciprocal connections, pause at closed doors for Open, preserve active travel on rejected replacements, and obey manual cancellation and safe reload rules.
- Elevation, doors, obstacles, and rain affect gameplay calculations.
- Whisper, speak, and yell produce different validated acoustic reach.
- Ordinary speech is clear for approximately half the standard unobstructed reference span for a normal-hearing listener, then degrades progressively.
- Distance, doors, weather, hearing sensitivity, age, and ear injury alter comprehension.
- A yell can cross an acoustic portal into the neighboring MVP cell with attenuation.
- Unheard words become deterministic `...`, and the listener's client never receives the hidden text.
- Heard-but-unseen speech has no character name or stable identity metadata but retains the speaker's color.
- Visual action narration depends on distance, line of sight, light, weather, vision ability, age, and eye injury.
- Missing visual action becomes `···`, distinct from missing speech, only when the mixed event is otherwise perceived.
- Wholly unseen and unheard events produce no placeholder or identity leak.
- The local map never stitches an adjacent cell into the current-cell view.
- The Nearby world map includes no non-adjacent cell; visible neighbors show current detail, glimpsed neighbors show faint coarse memory, visited neighbors show more detailed dim memory, and unknown neighbors are absent. Only the separate Known Routes index may include non-adjacent visited cells, as cached destination geometry without remote live state.
- Map items and secondhand information cannot establish or upgrade world-map memory.
- Earned cell memories survive elapsed time, logout, and server restart without losing detail, while remaining stale until directly observed again.
- A visible vertical neighbor produces an understandable isometric Z relationship; without one, the map remains 2D.
- A remembered vertical outline does not by itself activate the isometric presentation.
- Local perception still distinguishes visible, remembered, and unknown information inside the current cell.
- A client cannot reveal an unseen NPC merely by inspecting local state.

### Roleplay and Interaction

- Pointing and command input invoke the same authoritative verbs.
- Say and emote results reach the correct audience and survive reconnect where appropriate.
- A mixed roleplay post remains one ordered, speaker-owned entry while speech, narration, and action segments appear in sequence.
- A later in-character post waits until the currently revealing post finishes; two roleplay posts never reveal concurrently or interleave.
- Full post text appears only in the narrative pane; the map exposes no post body before, during, or after reveal.
- Reveal-speed settings alter only local presentation and cannot alter server event order or state timing.
- Whitelisted state commands update structured state; arbitrary slash commands cannot mutate the world.
- Current posture/state is visible to the player and through permitted character inspection.
- Local OOC reaches only the current cell and causes neither a spoken marker nor Social XP.
- Local OOC remains fully legible inside its channel and bypasses diegetic hearing calculations.
- The composer accepts long multiline posts, and `Shift`-`Enter` creates a newline without sending.
- Navigation-mode `Enter` resumes the composer; type-mode `Enter` sends and returns to navigation.
- `Escape` leaves type mode without sending, and the draft is present when type mode resumes.
- Typing presence exposes no draft content and disappears reliably after inactivity or disconnect.
- Composer focus without active input does not leave a permanent typing mark.
- A server-accepted speech post immediately creates a roughly three-to-five-second fading map marker but displays its body only in the narrative feed.
- Speaking-marker timing is independent of the per-viewer progressive-reveal queue.
- The same validated palette choice colors transcript speech, typing presence, and the spoken marker.
- Visual tokens never attempt to perform detailed authored emotes.
- Listen and Smell return observer-appropriate information.

### NPCs

- Six NPC schedules advance without six fully active off-screen actors.
- One NPC retains a relationship and episodic memory across restart.
- Active NPC interaction context transitions idempotently into a permanent compact summary one hour after the last interaction and remains retrievable after restart. Continued interaction resets the inactivity deadline.
- Generated prose uses the correct NPC voice and bounded knowledge.
- Generated output cannot directly mutate consequential state.
- A recruited NPC can speak, wait, respond when addressed, and interject selectively.

### Social and Persistence

- Chat delivery does not automatically grant Social XP.
- A validated scene produces an idempotent, explainable ledger entry.
- Server restart preserves required character, cell, NPC, and social state.
- Retried commands and reconnects do not duplicate rewards or consequences.

### Delivery

- A clean checkout can be built using documented prerequisites and commands.
- Automated tests run without opening the graphical editor.
- Client and dedicated-server packages can complete the target walkthrough.

## 8. Test Strategy

### Unit Tests

- cell schema validation and version rejection;
- continuous movement, terrain collision, elevation, reach, and facing;
- soft wolf separation, shared-tile occupancy, crowd traversal, and anti-body-block behavior;
- door and portal state transitions;
- line of sight and remembered-tile generation;
- adjacent-cell visible/remembered/unknown classification, glimpsed-versus-visited detail, first-degree world-map filtering, remembered-outline privacy, and relative Z placement;
- permanent non-decaying cell-memory persistence and direct-observation refresh rules;
- acoustic pathing, volume curves, hearing modifiers, and deterministic word masking;
- visual action gating, vision modifiers, deterministic `···` substitution, and non-leak behavior;
- scent range under downwind/upwind/crosswind/calm conditions, nose health, scent skill, rain/snow, and independent blind/deaf/anosmic observers;
- sealed/open air barriers, obstacle detours, diagonal corner sealing, eight-sector aggregation, no source metadata, cue removal, and unchanged visual map memory;
- saved base wind, deterministic gust continuity, sensory persistence, legacy-default compatibility, and atomic rejection of invalid wind/sensory fields;
- command authorization and idempotency;
- mixed-post parsing, command allowlisting, literal-slash escaping, and atomic state changes;
- social session qualification and anti-duplication;
- NPC schedule advancement and interruption;
- relationship bounds, active-memory consolidation, summary idempotency, permanent retention, and memory visibility scope.

### Integration Tests

- two clients receiving different perception of one authoritative cell;
- world-map adjacency changing from visible to the appropriate glimpsed or visited dim state when a door, window, or vertical opening changes line of sight, without leaking live state;
- map ownership and secondhand directions failing to reveal or upgrade an unobserved cell;
- glimpsed and visited memory surviving logout and simulated long time passage without decay;
- visible above/below adjacency activating isometric mode while ordinary adjacency remains 2D;
- remembered vertical adjacency remaining in the 2D presentation;
- two listeners receiving different versions of the same speech event due to position or ear damage;
- a heard-but-unseen speaker remaining anonymous in payload and UI while retaining the expected speaking color;
- a scented-but-unseen wolf producing only a broad fixed-radius cue and **Smell** description, with no source token, identity, count, exact position, or inspect target;
- wind reversal and weather changes altering scent cues while airflow labels remain distinct from scent-bearing markers;
- scent and movement-heard status remaining independently correct through local/world view switches, posture changes, source departure, and restart;
- one mixed post producing heard speech plus `···` action for an observer without line of sight;
- a wholly unseen and unheard action producing no client event;
- yelling through an open versus closed inter-cell portal;
- chat and emote audience separation;
- current-cell OOC scoping and exclusion from Social XP;
- ordered mixed-post replication independent from viewer reveal speed;
- identical post ordering at slow, fast, and instant reveal settings, with no concurrent reveal;
- typing-presence audience, timeout, disconnect cleanup, and absence of draft payloads;
- immediate speaking-marker delivery while the associated post remains queued for reveal;
- speaking-color validation and theme mapping;
- absence of hidden names or stable entity IDs in anonymous perceived-post payloads;
- cross-cell transition and reconnect;
- state restoration after server restart;
- NPC off-screen-to-active transition;
- a pending active NPC interaction surviving restart, consolidating exactly once after its deadline, and remaining in long-term memory permanently;
- generated dialogue with success, timeout, failure, and invalid proposed consequence;
- qualifying social scene followed by retry and restart.

### Presentation Tests

- full rotation of the orbiting marker;
- movement prediction/interpolation without granting the client authoritative position;
- direct WASD movement, click-to-path movement, path invalidation, stationary Alt-preview/Alt-click facing, and Ctrl-click compatibility;
- bounded shortest-arc turning, timed standing/crouching, canceled movement during preparation, and slow crouching;
- path-to-door stopping and explicit interaction before traversal;
- same-cell route cancellation, explicit transition-door travel, and automatic open-edge cell transition;
- destination-anchor placement, stopped arrival, held-input latching, and post-transition viewport recentering;
- long-form composer behavior, multiline input, typing bubble, and spoken marker;
- typing inactivity cleanup and the three-to-five-second spoken-marker fade;
- mixed speech/action/state flow within one entry at slow, fast, and instant reveal speeds;
- listener-specific `...` gaps remaining stable across reveal speeds and reconnect;
- listener-specific `···` visual gaps remaining distinct from acoustic loss;
- queued multi-speaker posts revealing one at a time in authoritative order;
- send, resume, escape-with-preserved-draft, and navigation-binding suppression behavior;
- 32 speaking colors across supported themes and high-contrast mode;
- font/zoom extremes and split-pane resizing;
- standard-cell fit, small-cell centering, large-cell scrolling, and recenter-on-player behavior;
- sub-tile movement, placement, hit-testing, and collision independent from glyph-grid boundaries;
- visual-token overlap, small collision cores, gentle separation, and crowded-door traversal;
- contextual menu placement at map edges;
- color-vision and no-color identity distinctions;
- rain, fog-of-war, and remembered-state legibility;
- world-map 2D/isometric switching and labeled non-isometric accessibility mode;
- keyboard-only completion of the target walkthrough.

### Playtest Questions

1. Can players identify self, other players, NPCs, facing, exits, and interactables without explanation?
2. Does free positioning help roleplay, or does it create unnecessary movement precision and crowding problems?
3. Does the facing marker communicate enough without implying animation?
4. Do WASD and click-to-path feel like two coherent ways to control the same wolf?
5. Does switching between navigation and type modes ever cause accidental movement or lost writing?
6. Do the typing and spoken indicators improve turn awareness without pressuring players to write faster?
7. Do speaking colors make a crowded narrative feed easier to follow in every supported theme?
8. Does the serialized reveal queue preserve clarity when several players send at once without making the scene feel sluggish?
9. Does progressive word loss feel like damaged comprehension rather than arbitrary text deletion?
10. Does `···` clearly communicate missing visual action without leaking too much or being confused with unheard `...` speech?
11. Does recurring text color suggest a familiar voice without making anonymous identity feel mechanically confirmed?
12. Are whisper, speak, and yell ranges understandable without exposing excessive numeric UI?
13. Are posture and declared state useful during inspection without replacing authored prose?
14. Do contextual verbs make the world discoverable without discouraging typed commands?
15. Do players keep writing detailed actions when the map is present?
16. Does the NPC feel continuous because of behavior and memory rather than merely verbose dialogue?
17. What information did players expect from the local map versus the world map?

## 9. Risk Register and Containment

| Risk | Containment |
|---|---|
| Unreal iteration or deployment is too heavy | Keep M1 simulation renderer-independent; make M2 and M8 explicit engine gates |
| Paper 2D tile tooling becomes a constraint | Use versioned external cell data and a custom glyph renderer; treat Paper 2D as a spike |
| Glyph map becomes visually noisy | Limit the atlas, preserve layer hierarchy, add zoom, and test crowded scenes early |
| Variable cell sizes make glyph scale inconsistent or maps hard to navigate | Fit the standard cell, preserve scale for large cells, center small cells, and provide recentering |
| Identity depends too heavily on color | Add outline/mark alternatives and high-contrast testing in M2 |
| Speaking colors become illegible or replace speaker identity | Use a curated theme-aware palette, show names only when perception permits identification, and test all 32 swatches |
| Typing presence leaks private behavior or creates pressure | Send only scoped boolean presence, expire quickly, never transmit drafts, and provide a future privacy toggle |
| Progressive text creates a long backlog during active scenes | Serialize posts in server order, show backlog state, provide adjustable/instant reveal, and measure queue depth in playtests |
| Inline slash text becomes an injection or authority path | Parse through a strict server allowlist and convert only validated commands into typed segments |
| Hearing degradation frustrates players or destroys essential context | Tune clear radii conservatively, expose understandable condition feedback, and keep `...` masking deterministic |
| Hidden words leak through clients, logs, or reconnect | Generate listener-specific perceived posts on the server and never replicate the inaccessible source text |
| Anonymous voices leak identity through metadata | Use perception-scoped source handles and omit names/stable entity IDs while preserving only the chosen color clue |
| Visual placeholders leak unseen players or hidden actions | Emit `···` only when another part of the same event is perceived; otherwise send no event |
| Free movement creates fiddly positioning during social play | Use forgiving interaction reach, face-without-move, collision radii, and playtest before adding combat |
| Players use bodies to obstruct doors or paths | Use tiny soft collision, dynamic avoidance, portal relaxation, and explicit crowded-door tests |
| Persistent multiplayer scope expands uncontrollably | Hold the MVP to three compact playable cells, two players, six NPC records, and one primary conversational NPC |
| NPC dialogue hallucinates state or knowledge | Supply bounded facts, separate prose from rules, reject proposed consequences, and keep deterministic fallbacks |
| Dialogue calls add latency or cost | Invoke only for engaged NPCs, use timeouts, concise contexts, and authored responses |
| NPC memory violates privacy or becomes incoherent | Store scoped structured memories linked to authoritative events; never use unlimited transcript recall |
| Social XP becomes farmable or a popularity score | Validate reciprocal sessions, use idempotent ledgers, add diversity/cadence controls, and keep AI out of payout |
| Content becomes trapped in binary assets | Store world definitions in reviewable versioned data and validate them automatically |

## 10. Working Rules for Implementation

- Complete one milestone gate before expanding the next milestone's surface area.
- Prefer a narrow end-to-end behavior over several disconnected mock systems.
- Add an automated rule test with every consequential server behavior.
- Keep clients untrusted even during development.
- Keep generated prose replaceable and non-authoritative.
- Store world and content data in reviewable, versioned formats where practical.
- Record decisions that change authority, persistence, networking, or content formats.
- Do not add a system merely because Unreal provides it; every dependency must serve the product vision.
- Update `VISION.md` when the desired experience changes and `PLAN.md` when execution changes.

## 11. Decisions Needed During M0

These questions should be answered by inspection or a small spike rather than prolonged design debate:

- Which Unreal version is installed and supportable on the development machine?
- Can that installation build the required dedicated-server target, or is a source build necessary?
- Which desktop operating system is the first packaged target?
- Which font or glyph atlas offers the necessary ASCII coverage and readable rotation?
- Does UMG/Slate or Paper 2D provide the cleaner MVP renderer under an identical map test?
- What minimum tile pixel size keeps `W` and the orbiting marker readable?

The dialogue provider does not block M0–M5. M6 begins with the provider-neutral interface and deterministic fallback before any external model is configured.

## 12. First Work Package

When implementation begins, the first bounded work package is:

1. Inspect and pin the Unreal toolchain.
2. Scaffold the C++ client and dedicated-server targets.
3. Define the smallest versioned cell, tile, and entity data structures.
4. Load a tiny test room on the server.
5. Render a grid of ASCII terrain glyphs in the client.
6. Place one `W` on the grid with a literal `>` rotating and orbiting around it.
7. Send WASD movement, move-to, and Alt-click face intentions to the server; render the local faded preview, smooth authoritative turn, and Ctrl-click compatibility path.
8. Add a first authoritative path around a static obstacle.
9. Add automated tests for blocked movement, path invalidation, and facing normalization.
10. Evaluate the renderer and build workflow before authoring the full tavern.

That package should produce the first genuine project checkpoint: not a static mockup, but one authoritative wolf moving and turning correctly on an ASCII tilemap.

## 13. Post-MVP Direction

Only after the MVP gate passes should planning expand to:

- Chapter formation, land rights, and collective construction;
- complete social Stories, recognition, and consented recaps;
- Gifted eligibility and authored Quickened candidacy;
- combat, training, injury, and supernatural Gift control;
- broader economies, professions, settlements, and faction institutions;
- extensions to the implemented Atlas Workshop, including larger-region authoring, NPC/item placement, legacy content import, and separately designed moderation tools;
- population scaling, multiple server processes, and live operations;
- additional portrait equipment/scar layers and any future upload moderation (the limited natural-coat creator is implemented);
- advanced weather, lingering scent/track persistence, individual scent recognition, cross-cell airflow, and richer sound systems;
- a larger connected world built from independently persistent cells.

## 14. Character front door and shared dolls follow-up

The native launch flow is now login/register → owned-character roster → creator
and review → select → world. See `Docs/Design/19-character-creation.md` for the
strict appearance contract and trusted-local account boundary. Five original
grayscale pixel-art species atlases supply four age frames each; runtime palette,
gradient, markings and stature controls drive the same portrait used by the
roster, sheet and sight-authorized inspection. Neither portraits nor gear appear
on the map. Starting age grants no past birthday rewards or Social XP.

This supersedes earlier prototype-only portrait notes in the original milestones.
Production account services remain out of scope: authentication is local-only,
world-save scoped, and remote credentials are blocked pending encrypted transport.
Six slots, age bands and cosmetic editing/retirement rules remain review items.
