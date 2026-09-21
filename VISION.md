# Runs Against the World — Product Vision

**Status:** Working vision, not an implementation plan  
**Date:** 2026-09-20  
**Purpose:** Record the experience we are trying to create before choosing detailed systems, milestones, or production scope.

## Vision in One Sentence

**Runs Against the World is a persistent, roleplay-first world in which prose supplies the performance, an ASCII-derived spatial interface supplies shared physical truth, and a living society remembers what its players do.**

The project should feel less like a conventional graphical RPG with a chat box and more like a modern stage for collaborative imagination. The interface helps players understand, remember, and interact with the world. It should not illustrate every action for them.

## Product Thesis

Classic MUDs, MUSHes, and MUXes made imagination unusually powerful because the text did not compete with the player's mental picture. Their limitations also created unnecessary friction: players became lost, forgot who or what was present, struggled to discover commands, and could not easily recover the context of a story after time away.

RATW should preserve the first quality and solve the second set of problems.

The resulting form is a deliberate middle ground:

- Text remains the principal medium for roleplay, dialogue, action, atmosphere, and interpretation.
- A low-graphic map establishes objective spatial facts: where entities are, what can be seen, which way something faces, and what can be interacted with.
- The world remains divided into persistent cells analogous to MUD rooms, while each cell contains a traversable two-dimensional space.
- UI panels make character identity, inventory, memory, and available interactions legible without turning the game into an icon-driven action RPG.
- The world continues to live when a player is not speaking to it.

## Experience Pillars

### 1. The Imagination Layer

Prose owns everything whose meaning benefits from interpretation:

- expression, emotion, intent, and subtext;
- the exact manner of an action;
- detailed sensory impressions;
- social performance and character voice;
- violence beyond the minimum state needed to resolve rules;
- the difference between what happened and what a character believes happened.

The map owns only shared physical facts. A wolf's map representation may show position, facing, movement, and a few broad conditions such as resting, injured, or hidden. It should not attempt to animate every bow, pounce, sniff, embrace, threat, or nervous ear movement described in prose.

This boundary is the answer to the “missing animation” problem: the visual representation should not make a promise to perform the player's authored action. When a player writes a rich action, the map can show a restrained activity mark, target highlight, or changed physical state while the narrative stream carries the actual performance.

### 2. The Orientation Layer

Players should rarely need to ask basic interface questions such as “Where am I?”, “Who is here?”, “What exits exist?”, or “What can I interact with?”

Orientation is provided by:

- the local map of the current cell;
- a world map of known and currently perceivable neighboring cells;
- stable positioning of characters and objects;
- visible exits, doors, obstacles, elevation, and hazards;
- target-aware interaction menus;
- clear distinctions between seen, remembered, inferred, and unknown information.

### 3. The Memory Layer

The game should help humans and simulated characters sustain long-running stories.

Memory includes:

- player-authored profiles, journals, and character descriptions;
- private and consensually shared recaps;
- character knowledge that is distinct from player knowledge;
- NPC memories of meetings, promises, conflicts, favors, quests, and reputations;
- persistent changes to locations and Chapters;
- records of social progression based on validated events rather than subjective prose scoring.

Memory is not omniscience. Every memory has an owner, source, confidence, scope, and retention policy. Private conversations must not silently become public lore.

### 4. A Living Society

NPCs are residents, not vending machines. They should have homes, work, obligations, relationships, routines, preferences, and limited knowledge. Their lives should be visible through behavior: traveling to work, closing a shop, eating, resting, meeting others, responding to weather, and conversing with one another.

The illusion of life depends more on continuity and consequence than on an unlimited amount of generated dialogue.

### 5. Social Play Is Core Play

Roleplay is not a decorative activity beside the “real” combat game. It is one of the principal ways a player advances, earns trust, changes the world, gains access, and becomes eligible for greater narrative responsibility.

Combat, training, and practice primarily develop a character's capabilities. Sustained roleplay develops the player's social standing and access. Neither track replaces the other.

## The Primary Window

The previously established split-window concept remains the working direction and will be iterated later.

- The **narrative side** contains scene prose, dialogue, roleplay actions, system results, and writing controls.
- The **world side** contains the local or world map, contextual interaction menus, inspected-object details, and compact orientation information.
- Character sheets, inventory, equipment, journals, and memory views open as focused panels without permanently crowding the play space.
- The balance between narrative and map should be adjustable. Text must be able to dominate when a scene becomes writing-heavy.

The interface should support both pointing and typing. Every visual action should have a keyboard-accessible equivalent, and experienced players should be able to use a command palette or textual commands without hunting through menus.

During ordinary play the client is in **navigation mode**. `WASD` moves the wolf directly, while clicking a reachable point requests intelligently pathed movement to that location. Pressing `Enter` opens or resumes **type mode**, preventing movement keys from firing while the player writes. In type mode, unmodified `Enter` sends the post and returns to navigation, while `Shift`-`Enter` inserts a newline. `Escape` returns to navigation without sending and preserves the unfinished draft for the next time type mode opens. The composer is designed for substantial roleplay posts rather than short chat alone. Facing follows accepted movement; while stationary, `Ctrl`-click turns the wolf toward the selected world point without moving.

Intelligent pathing never substitutes for an interaction decision. If a route reaches a closed door or another operable barrier, the wolf may approach it but stops there and identifies the obstruction. The player must explicitly choose **Open** through the contextual menu or an equivalent command before movement can continue.

After opening a door that connects two areas inside the same cell, the previous path remains cancelled and the player issues a new movement command. If the opened door is itself a portal into another stored cell, the explicit **Open** action completes that transition. An unobstructed boundary exit at the edge of the local map transitions automatically when the player runs or click-paths across it.

Every cell transition places the wolf at the authored arrival anchor corresponding to that connection and stops there. Velocity, click-path state, and carried movement intent are cleared; continuing into the new cell requires fresh movement input. On oversized cells, the local viewport recenters on the arrival point.

An in-character post may interleave quoted speech, narration, and slash actions. For example:

```text
"I haven't ever heard of anything like that," Jason said. /sigh "And I don't ever want to."
```

The client parses this into an ordered roleplay flow rather than printing raw commands. Speech, the narrated bridge, the sigh, and the final speech remain attached to one speaker-owned entry while the text is progressively revealed. Only one roleplay post reveals at a time. If another player sends while an entry is still unfolding, the later post waits in server order and begins only after the active entry finishes. Reveal speed is a local accessibility preference, including an instant option; it never changes authoritative event order or delays server-side consequences. A separate local OOC channel reaches only the current cell and does not produce in-world speaking cues or roleplay rewards.

In-character speech is spatial. Each speech segment has a voice level—initially **whisper**, **speak**, or **yell**—and each listener has a hearing sensitivity affected by innate ability, age, temporary conditions, and injuries such as ear damage. With normal speech, ordinary hearing, and no obstruction, words should remain clear for roughly half the fixed acoustic span established by the standard reference cell. Beyond that range, comprehension declines progressively and unheard words appear as `...` in that listener's narrative flow. A whisper requires close proximity. A yell carries farther and may propagate through exits into neighboring cells, losing clarity through distance, walls, doors, weather, and other acoustic conditions.

The server creates the perceived version separately for each listener. A client never receives words that its character failed to hear, and moving closer afterward does not retroactively restore them. Word loss is stable for that listener and post rather than changing on refresh. Local OOC is interface communication, not diegetic sound, and therefore does not use the hearing simulation.

Hearing a voice without seeing its source does not reveal the character's name. The narrative entry uses an anonymous voice label while retaining the speaker's chosen text color. The recurring color creates a similarity-of-voice clue that players may recognize, but the interface does not confirm the identity for them.

Action and pose narration is spatially visual in the same way speech is spatially audible. Line of sight, distance, lighting, obstacles, fog, vision ability, age, and eye injury determine whether an observer receives an action segment. Normal vision in an unobstructed, adequately lit room should provide clear action detail across roughly the same half-cell baseline used for normal speech; beyond that, clarity declines. When an observer receives another part of a mixed post but cannot make out an embedded action, the narrative flow substitutes `···`. This visually distinguishes missing action from the `...` used for unheard words. If the actor and event are completely unperceived, no placeholder is delivered merely to reveal that something happened.

## Visual Direction: ASCII as a Spatial Medium

The local map should look like an overhead two-dimensional RPG, but its visual vocabulary should be composed primarily of typographic glyphs. It is **ASCII as art direction**, not a terminal emulator.

Glyphs may be:

- assigned to logical terrain tiles while entity and effect layers move independently above them;
- positioned independently above the terrain grid for entities, facing, selection, weather, and perception;
- layered, colored, scaled, faded, outlined, and clipped by sight;
- rendered from a font atlas or glyph atlas for predictable performance;
- accompanied by restrained lighting, shadows, weather, and selection effects.

A portable plain-ASCII fallback should exist even if the principal client also uses Unicode box-drawing and typographic symbols.

### What the Map Should Depict

- terrain, walls, floors, thresholds, doors, windows, water, vegetation, and cover;
- elevation changes and traversable slopes;
- characters and NPCs as restrained directional glyph-sprites;
- interactable objects and their broad physical state;
- weather and visibility;
- tracks, scent, sound, or supernatural traces only when the observing character can perceive them.

### What the Map Should Not Depict

- detailed facial performance or body acting;
- the full appearance of a character;
- equipped bags, weapons, and clothing as miniature map attachments;
- literal animation for every player-authored emote;
- information that the observing character has not perceived.

### Character Representation on the Map

The map avatar is a **spatial token**, not a miniature portrait. Every wolf uses an upright `W` fixed at the center of its continuous local position. A separate `>` glyph orbits that center and rotates to point in the wolf's current facing or travel direction.

```text
conceptual cardinal snapshots

    ^
    W          W>          W          <W
                            v
```

In the graphical client, the source mark remains the `>` character: its small glyph-sprite is rotated graphically and moved around an invisible token-space ring. It does not occupy a world tile or overwrite the terrain glyph beneath the wolf. A plain-text compatibility view may substitute `^`, `>`, `v`, and `<` at cardinal facings.

While a wolf moves, the marker follows the direction of accepted travel. When movement stops, it retains the last facing. A stationary player can deliberately change facing with `Ctrl`-click without changing position.

The controlled character, other player characters, and NPCs use three separate semantic color roles. Selection, hostility, party membership, injury, and other temporary states should use additional outlines or marks rather than replacing identity color. Accessibility modes must provide a non-color distinction as well.

Each player character also selects a speaking color from a curated palette of approximately 32 choices. This is distinct from the map-token identity color. The selected speaking color is used consistently for that character's text in the narrative feed, their `...` typing bubble, and their brief spoken marker. When the speaker is visibly identifiable, display names and other non-color cues remain visible so color is an aid rather than the only means of identifying a speaker. When a voice is heard but its source is unseen, the color remains while the name does not.

The glyph should not flap its mouth when a player speaks or pantomime prose. While a player is actively entering text in the in-character composer, observers see a small speech bubble containing `...` above the wolf; the mark clears after typing inactivity, mode exit, send, or disconnect. As soon as the server accepts a post containing speech, a speaking marker appears above the wolf for roughly three to five seconds and fades, regardless of where that post currently sits in a viewer's reveal queue. Both indicators use the character's selected speaking color. Full text never appears over the map; the actual prose and its progressive reveal exist only in the narrative side of the interface.

Whitelisted posture and state commands such as `/sit`, `/lay`, `/stand`, and `/me …` can change a character's current declared state. The controlled player sees that state in their own UI, and other players can see it when inspecting the character. Inline actions such as `/sigh`, `/action …`, and `/pose …` become ordered action segments in the roleplay flow; they do not require a literal character animation.

### Character Art and Sheets

Every player may have a customizable, static character portrait shown on their character sheet and viewable by other players according to privacy settings. For this setting, the portrait is a standing wolf profile rather than a humanoid paper doll.

The portrait may express:

- coat colors and markings;
- build, age, scars, and distinguishing traits;
- harness, bags, jewelry, armor, or other visible equipment;
- an optional background or heraldic association.

Portrait art communicates appearance. It is deliberately separate from the map token so that the local map stays readable and does not imply that every described action requires art or animation.

### Inventory and Equipment

Inventory, gear, and weapons should use recognizable item icons in their menus. These icons assist recognition and comparison but do not appear on the overhead character token. Selecting an item should still expose a proper textual name and description; icons must never be the only carrier of meaning.

### Contextual Interaction

Clicking or selecting an entity opens a compact action menu anchored near it:

- a door might offer **Inspect**, **Listen**, **Knock**, **Open**, or **Force**;
- an NPC might offer **Inspect**, **Speak**, **Offer**, **Follow**, or context-specific actions;
- an object might offer **Inspect**, **Take**, **Use**, **Move**, or **Place**.

General actions such as **Listen**, **Wait**, **Rest**, **Look**, or **Smell** belong in a small persistent action menu or command palette. Contextual menus should contain only actions that are currently meaningful, permitted, or worth explaining. Risky, irreversible, or consent-sensitive actions require explicit treatment.

Every selected action resolves through the authoritative game rules and writes an intelligible result to the narrative stream. The pop-up is a discovery and input mechanism, not a second rules system.

Movement requests do not silently invoke these verbs. Opening, unlocking, forcing, entering, or otherwise operating an interactable requires an explicit player action.

## World and Map Model

### Two Map Scales

**Local map — the current cell**

The local map is a traversable overhead space containing the current room, clearing, street segment, courtyard, or other bounded scene. It provides exact-enough positions for interaction, line of sight, cover, doors, elevation, and movement.

The local map never stitches terrain from a neighboring stored cell into the current cell, even through an open portal. It remains a stable view of the space the player currently occupies.

Cells may differ substantially in dimensions. The standard reference cell is **32×24 terrain tiles**. Interiors can be smaller, while roads, fields, and other overland locations can be larger. Each cell's authored dimensions define its extent and therefore where its boundary transitions occur; outdoor cells do not require an additional landmark- or scene-based division rule. At the default local-map scale, the standard cell should fit completely within the map pane and most ordinary cells should be visible at once. Larger cells preserve readable glyph scale and require map scrolling; smaller cells remain centered rather than being enlarged without limit.

A `1×1` terrain tile is not a movement square or a wolf-sized slot. It is a comparatively spacious terrain and drawing patch. The `W` token, fixtures, effects, and interaction marks use continuous sub-tile coordinates above it. A tile should be visually large enough to contain meaningful positional variation and layered detail; the initial readability target is for a wolf token to occupy only a minority of the tile's width rather than filling it.

The token's visual footprint is not its collision footprint. A wolf has a very small soft-collision core centered under the `W`; the orbiting `>` marker has no collision at all. Wolves that approach too closely receive only a gentle separation or steering correction. They do not become hard walls, and groups must not be able to body-block a doorway, path, or cell transition during ordinary movement.

Scrolling or panning changes only the viewport. It never reveals terrain, entities, or activity that the observing character has not currently perceived or remembered.

**World map — cells and their relationships**

The active world map is a perception-limited neighborhood view. It shows the current cell and directly adjacent cells. Adjacent cells that are presently visible and within the player's line of sight through doors, windows, boundaries, vertical openings, elevation, weather, and character abilities render normally. Direct visual observation is the only way to create spatial memory: an adjacent cell merely glimpsed from elsewhere later remains as a faint, coarse outline, while a cell the character has physically entered can retain a more detailed dim outline. Both are remembered and potentially stale topology, never live occupants or current state. An external map, directions, hearsay, or general lore does not reveal a cell in this interface. Completely unobserved adjacent cells do not appear, and the view does not expand into a remote omniscient atlas.

The world map normally remains a top-down 2D composition. When a currently visible adjacent cell is spatially stacked directly above or below another visible cell, the view shifts into a restrained isometric presentation so the player can understand the Z relationship. Unreal may use true 3D positioning and a tilted camera for this view while continuing to render the world with glyph tiles. A merely remembered vertical outline does not trigger this shift; when no currently visible vertical stack exists, the world map returns to the simpler 2D presentation. A non-isometric accessibility representation must communicate the same layers with explicit above/below labels.

### Recommended Movement Compromise

The recommended middle ground is:

- **authoritative free movement across the local map while tiles define terrain beneath it**;
- **discrete transitions through authored exits into separately stored cells**.

This preserves the strong sense of place and manageable simulation boundaries of a MUD room without reducing a tavern, woodland clearing, or market to a single point. Terrain tiles provide broad surface, elevation, cover, and environmental data, but wolves have continuous local coordinates and are not restricted to tile centers. Collision, pathing, and line of sight may use finer continuous geometry than the visible terrain-glyph grid. Several wolves may occupy different continuous positions inside one terrain tile. Movement should be deliberate rather than twitch-based, and interaction should use forgiving physical ranges rather than pixel-perfect positioning. Hearing and vision use stable world distances derived from the standard reference scale, not a literal fraction of every differently sized cell.

### Cell Structure

Each cell is independently stored and contains at least:

- a stable identity and authored prose description;
- a two-dimensional footprint and local coordinate system;
- variable width and height plus a consistent world-units-per-tile scale;
- terrain, collision, portals, and interactive fixtures;
- an authoritative height field or elevation layers;
- acoustic, scent, light, and weather-exposure properties;
- entities currently present;
- exits connecting it to other cells;
- persistent state and change history;
- visibility rules for looking into or out of the cell.

Separating storage by cell supports persistence, streaming, moderation, authoring, and low-cost simulation. It does not require every cell to look like a rectangular room.

### Visibility, Knowledge, and Fog of War

The client receives only what the character is entitled to perceive. Visibility considers:

- distance and facing when appropriate;
- walls, doors, windows, terrain, cover, and elevation;
- ambient light and character vision;
- fog, rain, snow, smoke, and other weather;
- hiding, observation, tracking, and supernatural abilities;
- whether information is current, remembered, heard, smelled, or inferred.

The map should visually distinguish these states instead of treating fog of war as a single black curtain.

World-map memory has two earned levels. **Glimpsed** memory records only the coarse silhouette of a cell the character has directly seen into. **Visited** memory records a more detailed topological snapshot after the character physically enters that cell. Neither level is granted by possessing or reading a map, and neither provides continuing awareness after line of sight is lost. Once earned, both memory levels persist permanently for that character and do not decay with time. Their contents may become stale when the world changes and are refreshed only through new direct observation; entering a cell upgrades and refreshes its visited memory.

### Elevation

Height should be part of authoritative cell data, not merely a decorative underlay. The default map can communicate it through several restrained cues used together:

- contour or ledge glyphs at meaningful boundaries;
- small shadows or vertical offsets;
- directional stair, ramp, and slope glyphs;
- occlusion and line-of-sight behavior;
- an optional `+1`, `+2`, or contour overlay for inspection and accessibility.

A full topographic overlay can be offered as a toggle, but it should not sit permanently beneath every glyph. Players should read the room first and study the terrain when it matters.

### Weather

Weather is both presentation and simulation.

Rain, fog, snow, sun, wind, and storms can appear as restrained screen-space or map-space effects, but they must also affect appropriate systems such as:

- visibility and light;
- movement and footing;
- sound range and direction;
- scent strength, direction, and persistence;
- tracks and their decay;
- fire, shelter, warmth, and exposure.

Weather effects need reduced-motion, reduced-density, and high-contrast accessibility options. Atmospheric treatment must never make the text stream difficult to read.

## World Identity

The included world setting bible is the canonical creative foundation for this project. Its most important consequences for the product are not cosmetic.

- The inhabitants are sapient **quadrupedal wolves**, not humans with wolf portraits.
- Doors, latches, furniture, writing tools, storage, weapons, clothing, and architecture must be usable by mouths, forepaws, body weight, and harnesses.
- Scent is a first-class perception channel rather than flavor text alone.
- Ear, tail, posture, hackle, and scent cues matter to social play and deception.
- The material culture is late-medieval to early-modern; magic does not casually erase labor, transport, medicine, or class.
- Social position and institutional power matter as much as fighting ability.
- A wolf has one Gift family; every Gift has a limit, a cost, and a tell.
- Gifted wolves are uncommon and Quickened wolves are exceptional, politically consequential individuals.

The interface should expose these facts naturally. For example, **Smell** deserves the same interaction status that **Look** receives in a human-centered fantasy game, and a tavern should contain low surfaces, floor places, mouth-pull latches, and harness storage rather than ordinary chairs and human-height bar stools.

## Living NPCs

### Life Simulation

Every important NPC should be defined by more than a dialogue prompt. Their simulation model may include:

- residence, workplace, profession, and schedule;
- needs, duties, current goals, and interruptions;
- relationships with other NPCs, players, factions, and Chapters;
- possessions and access rights;
- beliefs, knowledge boundaries, rumors, and secrets;
- personality, speech style, values, and social risk tolerance;
- recent experiences and long-term memories.

NPCs can travel between home and work, gather socially, converse with one another, respond to closures or weather, and change their plans after meaningful events.

### Simulation at Different Distances

The entire population must not exist as fully active graphical actors at all times.

1. **Off-screen residents** advance through low-cost schedules, commitments, and event simulation.
2. **Nearby residents** receive spatial movement, perception, and reactive behavior.
3. **Engaged residents** receive full conversational state and detailed memory retrieval.
4. **Important consequences** are committed by deterministic game rules and persistent storage.

This layered approach makes a large world feel alive without requiring an AI model call or per-frame actor simulation for every resident.

### Generated Dialogue and Authority

Generated text should make an NPC sound like that individual and respond to the immediate social context. The dialogue system may receive a deliberately bounded context containing the NPC's voice, beliefs, current situation, relevant memories, and facts the NPC is allowed to know.

The language model may propose what the NPC says. It is **not** authoritative over:

- player identity or permissions;
- location, inventory, money, damage, or movement;
- quest completion or rewards;
- powers, social tier, institutional rank, or canon;
- whether a private message is delivered or remembered by someone else.

Rules determine outcomes first; generated prose expresses those outcomes. If generation is unavailable, authored barks and rule-driven responses keep the world playable.

### NPC Memory

NPC memory should be structured rather than an unlimited transcript. It has two temporal layers:

1. **Active memory** retains the detailed context needed to follow the current conversation or unfolding interaction.
2. **Long-term memory** contains compact summaries produced **one hour after the last interaction**. Any new interaction resets that inactivity timer, so an ongoing conversation keeps its active context.

Consolidation transitions the NPC's conversational context from detailed active material to a durable summary linked to the relevant characters and authoritative events. The summary persists permanently. It can be supplemented or corrected by later experiences, but it is not passively forgotten or deleted. Consequential facts such as an accepted quest, transferred item, injury, or relationship mutation remain separate authoritative state rather than depending upon generated summary prose.

Useful long-term forms include:

- stable facts about a known player;
- relationship values such as trust, respect, irritation, fear, debt, or familiarity;
- episodic memories of significant encounters;
- commitments, promises, warnings, and unfinished business;
- rumors with a source and confidence;
- compact conversation and interaction summaries with links to authoritative events.

The NPC retrieves only bounded relevant summaries for a new conversation rather than replaying an unlimited transcript. Consequential state remains explicit and inspectable by the server.

### Recruited and Party NPCs

A recruited NPC should participate in a party conversation more like a considerate player than a reactive chatbot.

The conversation system should track:

- who is speaking and who was addressed;
- whether the NPC has relevant knowledge;
- how recently the NPC spoke;
- whether silence, hesitation, interruption, or an unsolicited comment fits the personality;
- explicit invitations such as “What do you think?”;
- moments when gameplay requires a short, decisive response.

Not every NPC should answer every line. Turn-taking, selective attention, and the ability to remain quiet are essential to believable group conversation.

## Player Progression

### Two Independent Axes

**Character capability** grows through practice, training, risk, and appropriate use. This includes physical skills, learned techniques, and improved control of an existing supernatural Gift.

**Player social standing** grows through sustained, reciprocal, server-validated roleplay across multiple partners and stories. It unlocks trust, access, creation options, and eligibility for greater narrative responsibility.

### Rewarding Roleplay Without Grading Prose

The system should reward participation without appointing an algorithm as a literary judge.

- The server validates session structure, reciprocal participation, cadence, partner diversity, continuity, and completed Story involvement.
- The progression ledger stores narrow metadata and stable event identifiers, not a model's opinion of prose quality.
- Repetition, collusion, duplicate events, extremely rapid cadence, and single-partner farming receive diminishing or no credit.
- Recognition from other players can contribute as a bounded signal, never as an unlimited popularity score.
- AI may assist with private summaries or offline evaluation experiments, but it does not directly grant progression.

### Normal, Gifted, and Quickened

The requested social ladder and the setting's supernatural rules are reconciled by separating **player eligibility** from **character ontology**.

- A new player begins with access to Normal characters.
- Reaching the Gifted social tier can make the player eligible to create or portray a Gifted character, or to reveal a pre-authored latent Gift through a fitting story.
- Reaching the Quickened social tier establishes candidacy for a rare Quickened role; it does not automatically transform an existing character.
- Quickening remains a named, authored, politically consequential event with additional narrative and world-governance gates.

Roleplay XP therefore increases the player's level of trust and access. It does not teach an ordinary wolf enough social experience to spontaneously become supernatural. This preserves the desired progression loop without contradicting the world bible's rule that Quickened is an intrinsic classification rather than a routine XP level.

### Stories and Consent

Long-form Stories can join qualified scenes into a shared narrative. Publication of recaps, canon changes, or participant-authored summaries requires clear scope and consent. A successful chat delivery is not automatically a rewarded scene; a rewarded scene is not automatically a Story; and a Story is not automatically public canon.

## Chapters and Building

Guilds are called **Chapters**. Player construction is available through Chapters rather than individual private building.

This makes construction a social institution and gives it a place in the world economy. A Chapter may acquire or earn building rights, propose work, gather resources, assign permissions, complete construction over time, and maintain what it creates. Chapter governance should make ownership, editing rights, and succession explicit.

The restriction is purposeful:

- construction becomes a reason to organize and roleplay;
- settlements reflect collective history rather than unbounded private clutter;
- world changes can pass through setting, land, safety, and moderation rules;
- completed spaces carry visible provenance and memory.

## Engine Direction

### Current Recommendation

**Proceed with Unreal Engine for a deliberately small vertical slice, but treat the engine choice as a gate to be earned rather than a permanent commitment today.**

Unreal is capable of this project. Its Paper 2D system supports sprites and tile maps, UMG/Slate can build the split interface and contextual menus, its networking model supports authoritative dedicated servers, and StateTree or Behavior Trees can drive nearby NPC behavior. Existing RATW prototype notes also describe an Unreal client connected to a server-side social and NPC system, which reduces conceptual migration risk even though that prototype code is not present in this repository.

The concern is not whether Unreal can draw a 2D map. It can. The concern is production weight: build complexity, dedicated-server deployment, engine overhead, and the temptation to solve a low-graphic social world with high-cost graphical systems.

Godot remains the leading fallback because it has a dedicated 2D renderer, efficient tile-map tooling, high-level multiplayer, and headless dedicated-server exports. It is likely to be faster for a purely 2D client. Changing engines, however, would sacrifice the value of prior Unreal experimentation and would not remove the need for an authoritative persistent-world architecture.

### Rendering Approach in Unreal

Paper 2D can help prototype layout and collision, but the shipped look should not depend completely on experimental tile-map tooling. A custom glyph renderer can treat characters from a font or texture atlas as instanced sprites, with separate layers for terrain, objects, residents, perception, weather, and selection.

UMG/Slate should own:

- the narrative stream and composer;
- character sheets and static portraits;
- inventory and item icons;
- contextual action popovers;
- map labels, legends, accessibility overlays, and world-map controls.

### Architectural Boundary

The client is a view and input surface, not the source of truth.

```text
Unreal client
  glyph map · prose UI · icons · input
           |
           v
authoritative world simulation
  cells · movement · sight · weather · commands · combat
  social ledger · Chapters · NPC schedules · memory policy
           |
           +---- persistent structured data
           |
           +---- bounded dialogue service
                    candidate NPC prose only
```

The first server may run inside an Unreal dedicated-server process, but domain boundaries should remain clean enough to move persistent simulation into separate services later. Off-screen citizens should advance as data and scheduled events, not as thousands of ticking Unreal Actors.

## Engine-Decision Vertical Slice

Before full production planning, build one representative slice:

- one quadruped-designed tavern cell with an elevation change and multiple interactive objects;
- a connected exterior or street cell and a real door transition;
- a small upper or loft cell that proves visible vertical adjacency and the world map's isometric shift;
- two connected player clients;
- six NPCs with home/work/social schedules, including a bartender;
- one NPC who recognizes a returning player and recalls a promise;
- one recruited NPC who participates selectively in a group conversation;
- local and world map views with sight, doors, remembered space, and fog of war;
- one weather condition that changes presentation, visibility, scent, and movement;
- contextual actions for a door, object, NPC, and general scene command;
- persistence across server restart;
- a thin version of validated roleplay-session tracking, without final tier tuning.

The slice should answer:

1. Does the ASCII-derived map improve orientation without reducing the urge to imagine and write?
2. Can Unreal deliver the UI and glyph rendering quickly enough for frequent iteration?
3. Can a headless authoritative server persist the cell and NPC state reliably?
4. Can off-screen schedules and active nearby behavior transition without visible contradictions?
5. Can generated NPC dialogue remain grounded in rules, memory, and limited knowledge?
6. Is the development and deployment burden proportionate to the result?

If Unreal fails this test primarily because of workflow or deployment weight, reproduce the same slice in Godot before building the wider world. The comparison should use the same data model and success criteria.

## Explicit Non-Goals

At this stage RATW is not trying to become:

- a fully animated graphical RPG;
- a twitch-action MMO;
- a map where art performs every authored emote;
- an AI improvisation box without authoritative rules or canon;
- a popularity contest disguised as social progression;
- a private player-housing sprawl;
- a world in which magic trivializes ordinary society;
- a conventional humanoid fantasy game reskinned with wolves.

## Decisions Recorded

- Keep the split narrative/map window concept for later iteration.
- Use an overhead logical tilemap whose terrain, fixtures, and overlays are rendered with ASCII-derived glyphs.
- Represent every wolf with a centered `W` and an independently rotated, orbiting `>` facing marker; reserve detailed customizable wolf art for character sheets.
- Give the controlled character, other players, and NPCs separate semantic identity colors.
- Use item, gear, and weapon icons inside menus, not on the map avatar.
- Preserve distinct Imagination, Orientation, and Memory layers.
- Use contextual action menus over selected entities and a compact general-action menu.
- Maintain separate local and world maps.
- Store the world as separate persistent cells with traversable local space.
- Make sight, obstacles, doors, elevation, weather, skills, and fog of war materially affect perception.
- Treat height as simulation data and offer restrained elevation cues plus an optional analytical overlay.
- Make weather both an accessible visual treatment and a mechanical condition.
- Give NPCs schedules, relationships, bounded knowledge, persistent memory, and grounded generated dialogue.
- Make recruited NPCs capable of selective, personality-driven party conversation.
- Restrict player construction to Chapters.
- Make social progression equal in importance to capability progression without using AI to grade prose.
- Treat Gifted and Quickened progression as player eligibility and authored character opportunity, not automatic supernatural mutation.
- Use the supplied RATW setting bible as the world foundation.
- Test Unreal through a representative vertical slice before making the engine commitment permanent.

## Questions Deliberately Left Open

- What tile size and zoom range remain readable at the expected number of simultaneous residents?
- Should a long-term NPC summary represent an objective event record or the NPC's subjective understanding of it?
- What exact human moderation gates are required for Gifted and Quickened eligibility?
- How do Chapters obtain land and construction authority in each political region?
- Which parts of the earlier Unreal/Python social prototype should be ported, rewritten, or retained as tests?

## Source Relationship

This vision is informed by `RATW_World_Setting_Bible.md` and `SOCIAL_PROGRESSION_ROLEPLAY_TRACKING.md`. Those documents are design sources, not instructions embedded into this repository. The world bible supplies setting canon and invariants. The social-progression handoff supplies tested system principles and implementation history; its prototype status does not imply that its code or configuration currently exists in this otherwise empty project.
