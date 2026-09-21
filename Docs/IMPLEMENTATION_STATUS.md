# MVP implementation handoff

Date: 2026-09-20 overnight build (verification continued September 21 UTC).

## Outcome

A playable native Unreal development slice exists, with a packaged Linux client,
three connected cells, free movement over glyph terrain, spatial multiplayer
roleplay, living NPC routines and persistent conversation memory. This is a
testable foundation, not a finished public game or a claim that every roadmap
checkbox is complete.

The source is in the RATWMUD Git workspace with
`https://github.com/DocFoxHound/RATWMUD.git` configured as `origin`.
No remote push or public deployment was performed.

## Component status

| Component | Working behavior | Boundary / next work |
| --- | --- | --- |
| World | 32×24 tavern, 40×28 rainy yard, 20×14 loft; separate authored cell files; height-aware continuous movement, quarter-tile pathing, tiny soft wolf collisions | Broader authoring tools, combat and scale testing are not built |
| Navigation | WASD, click paths, Ctrl-click facing; upright `W` and orbiting literal `>`; explicit doors, transition anchors and stop-on-arrival | Real-player feel and latency tuning still needed |
| Local/world maps | Current-cell-only local view; adjacent-only overview, permanent glimpse/visited knowledge, obscured terrain and residents, visible vertical-stack view | Vertical view is a Slate projection, not an Unreal 3D camera; animated transitions and independent zoom remain later work |
| Interface | Native split layout with four proportions, map tabs, contextual verbs, general actions, preference controls | Some painted controls lack a full accessibility tree and focus treatment; settings are session-local |
| Roleplay | Multiline composer, Enter/Shift-Enter/Escape behavior, draft recovery after rejection, mixed speech/actions, one-at-a-time reveal, IC/local OOC, 32 speaking colors | Transcript/draft persistence across client crashes and offline event replay are not supplied |
| Perception | Distance/occlusion/weather-sensitive hearing and vision, whisper/speak/yell, ear/eye health modifiers, masked words/actions, anonymous heard-only voices | No age system, full lighting model, final injury gameplay or sophisticated scent simulation |
| Character/items | Static original wolf profile with satchel, description/state/posture, authoritative social totals, icon-based starter inventory | Profile art is a labeled prototype; custom coats, equipment mutation and other-player illustrated sheets remain incomplete |
| NPC life/party | Six schedule-driven wolves; contextual greetings; one recruitable scout follows cells and selectively responds/interjects | Companion release/transfer and leader-logout policy unresolved; no population LOD or deep inter-NPC social simulation |
| NPC memory | Active detail, retained attributed excerpts, permanent extractive summaries eligible after 3,600 inactive seconds, restart recovery and source IDs | Consolidation runs on startup or the next five-second checkpoint; richer relevance and belief/quest records require design |
| Dialogue | Working authored fallback; restricted-context optional local HTTP adapter; success/error/malformed/timeout tests | No real generative model selected or evaluated. Unreal redirect behavior requires a trusted local provider, not an untrusted endpoint |
| Social progression | Server-derived evidence, reciprocal human sessions, explicit/inactivity settlement, capped receipts and persistent XP/level; no AI scoring | One automatic scene per cell; full source-document Stories, private scenes, account-wide identity and Gifted/Quickened governance deferred |
| Networking | Separate authoritative process, owner-bound commands, compressed observer-filtered snapshots and reliable prose, two real clients | Development identities are not authentication; replay receipt window is bounded; hostile-load and internet-latency testing remain |
| Persistence | Transactional SQLite world checkpoint, per-character exploration/state, NPCs, memories, companions and social records; invalid saves fail closed | Version 1 only; no production migration or administrative recovery UI |

## What was deliberately not built

Chapter construction, combat/training balance, magic unlocks, economy/crafting,
production accounts, moderation, uploaded-art handling and large-world content
remain future milestones. Their vision is retained, not replaced by decorative
buttons that imply those systems exist.

## Engine decision

**Provisional go for the next playtest, not an unconditional engine commitment.**
Unreal now renders the intended low-graphics presentation, carries the actual
two-client world and produces a Linux package. The initial development archive
is about 692 MB including debugging symbols: a meaningful overhead for a
text-first game. The installed distribution cannot build the optimized
`RATWMUDServer` target; a compatible source-engine build is needed for that target.
The standalone development server uses the editor executable. A separate packaged
headless listen host was also verified with two real clients, without the editor
or a local host character. It is not a stripped dedicated-server binary; it
retains the Game target's runtime footprint.

The simulation remains plain C++ with independent tests, keeping a future
renderer/engine change feasible. Human roleplay usability, deployment cost and
network performance should decide the engine's long-term place.

## Read next

- `README.md`: launch instructions and controls.
- `PLAN.md`: finalized direction, verified checkboxes and remaining milestones.
- `Docs/Design/00-architecture.md`: index of the eleven component designs.
- `Docs/TEST_REPORT.md`: executed checks, fixes and untested areas.
- `Docs/MORNING_QUESTIONS.md`: decisions to review after playing.
- `Docs/SCREENSHOTS.md`: real game captures and what each demonstrates.
