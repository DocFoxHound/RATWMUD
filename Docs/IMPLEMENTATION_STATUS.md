# MVP implementation handoff

Date: 2026-09-20 overnight build, with subsequent updates through September 21 including Atlas Workshop, travel, calendar/aging, finite NPC economy, political authoring and the Storykeeper first slice.

## Outcome

A playable native Unreal development slice exists, with a packaged Linux client,
three connected cells, free movement over glyph terrain, spatial multiplayer
roleplay, living NPC routines and persistent conversation memory. This is a
testable foundation, not a finished public game or a claim that every roadmap
checkbox is complete.

The default client now starts with local account login, owned-character selection
and a live wolf creator. Twenty source age/species dolls share natural palette,
marking and stature controls across the roster, own sheet and authorized player
inspection. Native and packaged restart/multiplayer flows pass; see
[character creation verification](CHARACTER_TEST_REPORT.md). This remains
trusted-local authentication, not internet-ready account hosting.

Atlas Workshop now supplies a separate local-browser authoring workflow:
continuous terrain painting, default/custom cuts, rectangular merge/split,
cell-level detail and elevation, and reciprocal links to detached interiors.
It exports independent runtime cells; it does not replace the player's existing
cell-by-cell experience with a continuous-world camera.

Atlas also authors faction/Chapter catalogs and per-cell region, overlapping
claims and Chapter sites. Storykeeper is a separate trusted-local browser/service
for an omniscient roster/map, campaigns, approved events, Chapter profiles,
observed activity and finite resident migration previews. These are initial
development components, not public administrative infrastructure or a completed
settlement/politics system. The service's 36 isolated tests pass; native and
browser integration evidence is reported separately, not implied by this count.

Movement now has dexterity-scaled top speed, eleven selectable pace notches,
continuous stamina recovery, sprint drain, and exhaustion feedback. A separate
Known Routes view starts on-foot journeys across previously visited cells while
the ordinary Nearby map and local-cell boundaries remain unchanged.

The four-hour calendar now drives seasons, lunar night light and annual aging.
Six deterministic residents share real food supplies and finite purses with
players. The 30-day Spring and stalled-settlement recovery probes pass, but
wealth distribution and long-term economic balance remain unfinished. See
[calendar/economy verification](SOCIETY_TEST_REPORT.md).

The source is in the RATWMUD Git workspace with
`https://github.com/DocFoxHound/RATWMUD.git` configured as `origin`.
No remote push or public deployment was performed.

## Component status

| Component | Working behavior | Boundary / next work |
| --- | --- | --- |
| World | 32×24 tavern, 40×28 rainy yard, 20×14 loft; separate authored cell files; height-aware continuous movement, quarter-tile pathing, tiny soft wolf collisions; opt-in validated custom-world manifests | Combat, broad authored content, and production-scale testing remain open |
| Atlas Workshop | Separate browser editor; continuous glyph/elevation canvas, 32×24 or custom cuts, safe rectangular merge/split, drill-down detail, detached rooms, reciprocal links, JSON drafts and validated exports; faction/Chapter catalogs with region/claim/site overlays | World and cells/rooms max 256×256; max 256 total cells/rooms. Claims do not imply control/building rights. No NPC/item authoring, legacy `.cell` import, collaborative editing, hot reload, or save-topology migration |
| Navigation | WASD/click paths; gradual Alt/Ctrl facing; timed rises and slow crouch/sneak; dexterity-scaled pace, constant stamina recovery, sprint drain and exhaustion; explicit doors and anchored transitions | Human pace/posture/stealth balance and full skill-training progression remain open |
| World travel | Visited-cell journeys over observed reciprocal connections; ordinary local navigation, brief anchored crossing pauses, explicit closed-door Open, manual cancellation; continues during writing/panels | Cell-level destination only; no teleportation/offline travel, party-pace matching, or production-scale route/load proof |
| Local/world maps | Current-cell-only local view; adjacent-only Nearby overview; permanent glimpse/visited knowledge and visible vertical stacks; separate Known Routes index of cached visited geometry without remote live state | Vertical view is a Slate projection, not an Unreal 3D camera; animated transitions and independent zoom remain later work |
| Interface | Native split layout with four proportions, map tabs, contextual verbs, general actions, preference controls | Some painted controls lack a full accessibility tree and focus treatment; settings are session-local |
| Roleplay | Multiline composer, Enter/Shift-Enter/Escape behavior, draft recovery after rejection, mixed speech/actions, one-at-a-time reveal, IC/local OOC, 32 speaking colors | Transcript/draft persistence across client crashes and offline event replay are not supplied |
| Perception | Independent sight/hearing/smell, injury/skill/age modifiers, spatial speech/vision, sneak cutoff and quieter movement, anonymous pawsteps/scent arcs; daylight/interior/moon light affect sight | Sneaking changes movement noise, not deliberate speech or body scent. No familiar-scent recognition, trails, cross-cell scent or complete injury gameplay |
| Weather/wind/calendar | Four-hour days, 365-day seasonal calendar, deterministic seasonal forecasts and moon/weather night light; rain/snow/fog, wind, cell-boundary atmosphere and indoor light profiles | Game-day lunar clock confirmed; not actual-date ephemeris. No regional fronts, accumulation, exposure or individual lamp shadows; night-light curves need playtesting |
| Accounts/creator | Default native login/register, six owned character slots, create/review/select, persisted sex/age/species/stature/coat/pattern, live pixel-art preview, shared own/visible-other portrait | Trusted-local accounts scoped to one world save only; remote login blocked until encrypted transport exists. No recovery/deletion or post-creation cosmetic editing. Age bands and slot count provisional |
| Character/items | Shared configurable wolf profile, authoritative age/annual stats and social totals, finite herb/meal inventory, purse, consumption, gathering and contextual trade; logged-out aging confirmed and implemented | Natural-death choice/prompts and mandatory random age-100–120 deadline remain design only. Equipment/scar portrait layers remain future work; satchel/keepsake remain prototype non-tradeable cards |
| Combat presentation | Confirmed design: sparse local-map cues plus one collapsed/latest-action or expanded/full-perceived-log entry per encounter | Design only. No combat resolver, event stream, effects, grouped-log UI or combat acceptance result is claimed; see `Docs/Design/18-combat-presentation.md` |
| NPC life/economy/party | Six deterministic needs-driven wolves physically work, gather, cook, deliver, buy food, sleep and earn bounded wages; finite purses/stock with capped external orders/imports; one recruitable scout | Two goods, demo population only; no general job authoring, LOD or deep inter-NPC relationships. Companion provisioning, release/transfer and leader logout need policy |
| NPC memory | Active detail, retained attributed excerpts, permanent extractive summaries eligible after 3,600 inactive seconds, restart recovery and source IDs | Consolidation runs on startup or the next five-second checkpoint; richer relevance and belief/quest records require design |
| Dialogue | Authored fallback; optional credential-isolated OpenAI bridge using RATW Game configuration; real generated delivery and active/permanent memory recall verified through process restarts | Small synthetic sample only, not production quality/security evaluation. Offline remains default; trusted-local bridge is request/time bounded. See `LIVE_NPC_TEST_REPORT.md` |
| Social progression | Server-derived evidence, reciprocal human sessions, explicit/inactivity settlement, capped receipts and persistent XP/level; no AI scoring | One automatic scene per cell; full source-document Stories, private scenes, account-wide identity and Gifted/Quickened governance deferred |
| Storykeeper | Separate local operator app, private session, omniscient snapshots, campaigns/beats, fixed-UTC scheduling, audit; allowlisted native announcements, weather, resident relocation and finite asset transfers | Opt-in private bridge only, not player RPC. Stale state is read-only. No public/remote staff authentication, multi-admin roles, rollback or durable offline announcements. Unsupported encounter/army/faction effects remain plans |
| Chapters/political records | Operator profiles, members and conflict-checked site overlays; declared housing/jobs/attraction; observed active minutes/routes; deterministic same-region migration previews and explicit approval | Finite existing residents only, verified arrival before source-claim resentment; no automatic migration, actual Chapter buildings/jobs, or inferred claims. Opinions are service records, not native NPC mechanics; new residents retain existing work/food commutes |
| Networking | Separate authoritative process, account-owned character entry, compressed observer-filtered snapshots and reliable prose, two real clients | Account credentials restricted to standalone/loopback; explicit development-identity bypass only for tests. Public encrypted authentication, hostile-load and internet-latency testing remain |
| Persistence | Transactional SQLite world checkpoint, per-character exploration/state, NPCs, memories, companions and social records; invalid saves fail closed | Version 1 only; no production migration or administrative recovery UI |

## Authoring handoff

Run `python3 tools/map_editor.py serve` and open the printed loopback URL.
Save authoring JSON separately from game saves. Export a ZIP, extract it into a
new directory, and launch with `-RatwWorld=/absolute/path/to/world.ratw`.
The game validates that package independently and rejects invalid custom content
instead of silently returning to the demonstration world.

Custom-save identity is a hash of the manifest **path**, not a content hash.
Re-exporting changed geometry over the same location would reuse that save by
default; use a fresh export directory or fresh explicit `-RatwSave` path after
topology changes. No save migration or live-world write is performed by the
editor. Custom imports replace demo content and currently contain no authored
NPC population. See `Docs/Design/12-map-editor.md` for workflow and exact limits.

Political catalogs and territory metadata travel through the same source/export/
import path. Splits inherit metadata, while incompatible claims, regions or
Chapter sites block merges/recuts. Overlapping claims are intentionally retained
as provisional assertions, not resolved ownership. The authoring overlay never
reveals those cells to a player. See `Docs/TERRITORY_AUTHORING_CONTRACT.md`.

## Storykeeper handoff

Current verification: 23/23 Unreal Automation suites; 23 end-to-end checks on
both native and packaged server/client paths, including restart; 11/11 portable
and sanitizer suites; 36 service, 30 exporter, and 61 combined browser-model
tests. Actual browser workflows and captures are documented in
`Docs/DM_TEST_REPORT.md` and `Docs/SCREENSHOTS.md`.

Start the authority with `-RatwDMDirectory=/absolute/private/ratw-bridge` and an
isolated `-RatwSave` for experiments, then run:

```bash
python3 tools/dm_service.py --exchange /absolute/private/ratw-bridge --state-dir /absolute/private/storykeeper --port 8780
```

Use actual absolute paths with owner-only `0700` bridge/state folders. Open the
private `session.json` file in the state directory and use its `url` in a local
browser; do not share the token-bearing URL. This is an operator session, not a
player login. The Python service has its own database and never opens the game
save. The native opt-in channel is unavailable through public player RPCs.

The default three-cell world is grouped as `demo_reach`, with no invented canon
claims. A DM can explicitly bootstrap Chapter sites/capacity there without
rewriting Atlas; contradictory authored Chapter assignments are rejected.
Migration requires a named existing eligible resident, known shared region,
available declared capacity and explicit approval. Acceptance starts navigation;
fresh home/arrival evidence is required for political consequences. Declared
capacity is not measured infrastructure, and known active-player samples are not
fabricated attendance history. See `Docs/DM_SERVICE_CONTRACT.md` for scheduling,
staleness, retry and unconfirmed-outcome limits.

## What was deliberately not built

Chapter construction, autonomous migration, general NPC/job authoring, combat/training balance,
magic unlocks, general economy/crafting expansion, production accounts/staff roles, moderation,
uploaded-art handling and production-scale world content/tooling
remain future milestones. Their vision is retained, not replaced by decorative
buttons that imply those systems exist.

Brigands, assassinations, armies, route robbery and faction collapse have no
native executors. Stored faction opinions do not yet cause NPC hostility, access
restrictions, taxes or dialogue changes. Campaign records do not prove any of
those consequences happened.

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
- `Docs/Design/00-architecture.md`: component design index, including Atlas Workshop.
- `Docs/Design/12-map-editor.md`: separate map editing, exports and save boundaries.
- `Docs/Design/13-pace-and-world-travel.md`: movement controls, stamina tuning, route privacy and interruption.
- `Docs/Design/16-territory-chapters-migration.md`: claims, sites, finite population and verified arrival.
- `Docs/Design/17-storykeeper-dm.md`: separate operator workflow and deferred executors.
- `Docs/DM_SERVICE_CONTRACT.md` and `Docs/DM_BRIDGE_CONTRACT.md`: private launch/authentication, API and native lifecycle boundaries.
- `Docs/TEST_REPORT.md`: executed checks, fixes and untested areas.
- `Docs/MORNING_QUESTIONS.md`: decisions to review after playing.
- `Docs/SCREENSHOTS.md`: real game captures and what each demonstrates.
