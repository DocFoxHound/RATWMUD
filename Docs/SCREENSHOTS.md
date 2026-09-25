# Captured from the running applications

## Native character creation and portraits

These captures use disposable local accounts in the real Unreal client. Source
wolf art is generated; the screens themselves are actual viewport captures.

![Local login](../artifacts/screenshots/33-character-login.png)

![Live wolf creator](../artifacts/screenshots/34-character-creator.png)

![Saved character selection](../artifacts/screenshots/35-character-roster.png)

![Own character info card](../artifacts/screenshots/36-player-character-card.png)

![Old Arctic wolf preview](../artifacts/screenshots/37-old-arctic-preview.png)

![Tall maned wolf preview](../artifacts/screenshots/38-maned-wolf-preview.png)

![A separate player's authorized inspection](../artifacts/screenshots/39-other-player-inspection.png)

See [character verification and limitations](CHARACTER_TEST_REPORT.md).

## Atlas political authoring and Storykeeper

These browser captures show actual local applications, connected to an isolated
test world where applicable. Names ending in QA are deliberately noncanonical
fixtures. Storykeeper is separate from the character client; its whole-world
view is not exposed to players.

![Atlas faction claim and independent Chapter site](../artifacts/director/01-atlas-territory.png)

![Storykeeper omniscient world overview and cell inspector](../artifacts/director/02-storykeeper-world.png)

![Chapter profile with declared housing, jobs and attraction](../artifacts/director/03-storykeeper-chapter.png)

![Event desk distinguishes verified weather and relocation from planned encounters](../artifacts/director/04-storykeeper-events.png)

The native and packaged integration runs independently verified actual travel,
finite transfers, scheduled announcements, player isolation and restart safety.
See [the director verification report](DM_TEST_REPORT.md) and
[Storykeeper design](Design/17-storykeeper-dm.md) for boundaries.

## Unreal gameplay captures

These are actual 1600×1000 viewport captures from automated play sessions, not
concept mockups. Test characters and dialogue are fictional fixture data.

## Cell atmosphere and interior lighting

One 20×14 authored tavern under changing light. Glow follows its own boundary,
not the larger map pane. The UI remains text-first and no scene illustration is
introduced. Developer-control messages are test feedback, not roleplay.

![Daylit tavern with clear vision and no atmospheric glow](../artifacts/screenshots/23-tavern-day.png)

![Warmly lit tavern at night, with clear vision and a soft perimeter glow](../artifacts/screenshots/24-tavern-warm-night.png)

![Unlit tavern at night: nearby sight and dim remembered terrain](../artifacts/screenshots/25-tavern-unlit-night.png)

![Sealed unlit room remains dark even at noon](../artifacts/screenshots/26-unlit-cellar-day.png)

![Cool artificial lighting supplies a different interior atmosphere](../artifacts/screenshots/27-tavern-cool-night.png)

![Outdoor night uses dark edges and sky illumination](../artifacts/screenshots/28-outdoor-night-edges.png)

Darkness changes authoritative sight, not just image brightness. Hearing, scent
and sheltered footing remain intact. The current indoor sight floor is 8%; dim
details farther away are remembered terrain, not newly visible surroundings.
The separate restart scenario checks that the unlit profile persists. See
[lighting verification](LIGHTING_TEST_REPORT.md).

## Weather and daylight

The same glade, observer and camera under five conditions. The closed door beside
the wolf occludes the eastern view in every image; the pond beyond it is not
revealed simply to make the screenshot prettier. Weather changes the actual
visible-tile set; previously seen terrain remains dim when fog or night hides it.
The test's developer-control messages are not ordinary roleplay dialogue.

![Clear daytime at Juniper Crossing](../artifacts/screenshots/17-weather-day.png)

![Wind-driven rain with reduced hearing, scent and footing](../artifacts/screenshots/18-weather-rain.png)

![Snow drifts across the ASCII map while prose stays readable](../artifacts/screenshots/19-weather-snow.png)

![Fog conceals distant terrain behind layered veils](../artifacts/screenshots/20-weather-fog.png)

![Night preserves nearby vision and dim memory without weakening ears or nose](../artifacts/screenshots/21-weather-night.png)

![A separate sheltered interior stays steadily lit and free of outdoor precipitation](../artifacts/screenshots/22-weather-shelter.png)

All captures come from the real graphical weather scenario; the separate restart
scenario verifies that the shared night clock survives process restart. See
[weather verification](WEATHER_TEST_REPORT.md) for scope and limitations.

## Pace, stamina, and remembered travel

![Sprinting across a local cell with a selected pace and draining stamina bar](../artifacts/screenshots/15-travel-pace.png)

The eleven-notch control shows selected sprint effort, dexterity-based top speed,
current stamina and net drain. Travel still occurs through the local map, with
the ordinary wolf token and perception rules. This is a disposable flat-ground
integration fixture, not finished world content.

![Known Routes remembers visited destinations without showing remote activity](../artifacts/screenshots/16-known-routes.png)

After crossing three cells, the wolf stops and recovers stamina. The separate
Known Routes view contains cached visited geometry and a destination list. An
interior sharing another cell's origin is labeled as a group rather than drawing
unreadable overlapping names. The test's speech was sent during the journey;
typing and speaking did not cancel it. The scenario also verifies manual
cancellation and waiting at a closed door for an explicit Open action.

## Local tavern and narrative

![Tavern with freely positioned wolf tokens and text pane](../artifacts/screenshots/01-tavern-local.png)

## Two networked players

![Two clients with distinct speaking colors and queued long-form prose](../artifacts/screenshots/network-ash.png)

## Text-first layout preset

![Wider roleplay pane with reflowed text and square terrain tiles](../artifacts/screenshots/09-text-first-layout.png)

## Posture and deliberate facing

![Crouching wolf with low-profile status after slow movement](../artifacts/screenshots/10-crouch-movement.png)

![Faded direction preview northeast of the wolf while actual facing remains west](../artifacts/screenshots/11-alt-facing-preview.png)

These follow-up captures come from the real movement scenario. The first shows
the persistent crouching status after moving from lying down. The second holds
Alt without clicking: the pale preview points toward the cursor while the solid
facing marker still points west. The footer explains the new control.

## A visible upper cell

![Visible loft produces the vertical neighborhood projection](../artifacts/screenshots/06-visible-vertical-world.png)

The loft is only a coarse outline until the character enters it. Hidden residents
and unobserved terrain are not supplied to the client.

## Upstairs

![Quiet Loft local cell](../artifacts/screenshots/07-quiet-loft.png)

## Rain outside

![Rain overlay and partially remembered outdoor terrain](../artifacts/screenshots/08-rain-in-juniper-yard.png)

## Scent without sight

![A lavender westward scent arc beside Ash, with no hidden player token](../artifacts/screenshots/12-upwind-scent.png)

Two real clients were connected for this capture. Air flows west to east, carrying
an unseen crouching player's scent toward Ash. The lavender arc and `~~` sit at a
fixed radius west of Ash; they do not mark the source's location. The other visible
green wolves are NPCs. The narrative reports a broad direction. The two identical
inspection denials are deliberate test probes for a hidden player and a nonexistent
ID: neither response identifies a hidden wolf. Reversing wind removes the distant
scent cue, and restoring it restores detection. This screenshot was visually checked
after fixing the wind label's missing glyph and clarifying the unseen-sense labels.

## Character and item panels

![Prototype static wolf profile and character state](../artifacts/screenshots/02-character-sheet.png)

![Icon-based starter inventory](../artifacts/screenshots/03-inventory.png)

The wolf profile is intentionally labeled prototype art. Inventory is presently
read-only; neither panel changes the simple map token.

## Preferences

![Speaking colors and presentation preferences](../artifacts/screenshots/05-settings.png)

An additional overview capture is `artifacts/screenshots/04-world-map.png`.
Machine-readable scenario results sit alongside the images. Separate packaged
binary captures/results are in `artifacts/packaged-evidence/`.

## Separate Atlas Workshop editor

These are actual 1280×720 browser captures of the standalone local authoring tool,
not game UI mockups. The seeded landscape is an editing demonstration, not newly
established world canon.

![Continuous ASCII landscape with four editable cell partitions](../artifacts/editor/01-world-canvas.png)

![Drilled-down detached tavern with terrain palette and cell metadata](../artifacts/editor/02-cell-detail.png)

## Exported maps inside Unreal

The following 1600×1000 captures use a deliberately simple integration fixture,
not the decorated editor demonstration above. The fixture paints water across a
future cut, merges/splits cells, and creates an off-map interior link before export.

![Player stopped at the corresponding edge of a separately loaded authored cell](../artifacts/screenshots/13-authored-cell-runtime.png)

![Authored off-map interior after an explicit door-open transition](../artifacts/screenshots/14-authored-interior-runtime.png)

Only the current cell is drawn in each local view, with normal perception filtering.
The door transition stops at its arrival. Empty authored scene prose is labeled
honestly instead of showing a misleading connection-progress message.

## Calendar, birthdays and finite trade

These are actual native-client captures from an isolated test save. A
development-only year jump exercises the birthday; it is not normal accelerated
aging beyond the shared four-hour-day calendar. The moon shown follows game
days provisionally, not today's actual lunar date.

![Rowan's finite stock, purse and current buy/sell offers](../artifacts/screenshots/29-finite-merchant.png)

![Character sheet after an annual physical-stat milestone](../artifacts/screenshots/30-birthday-character.png)

![A birthday notification in the narrative pane](../artifacts/screenshots/32-birthday-notice.png)

![Night calendar, lunar illumination and warm indoor light as residents return to rest](../artifacts/screenshots/31-nightly-rest.png)

See `SOCIETY_TEST_REPORT.md` for real transactions, NPC navigation/production,
save-restart evidence and the limits of this two-good demonstration economy.
