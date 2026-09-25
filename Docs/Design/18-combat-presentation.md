# Combat readability: map cues and one expandable encounter log

Status: confirmed presentation direction, September 21, 2026. **Design only**;
the current prototype has no general combat resolver, combat event protocol,
map battle effects or expandable combat transcript. This document defines the
next component, not functionality already shipped.

## The experience

Combat should be understandable without reading a waterfall of repeated attack
messages. The local map shows a few readable effects between visible combatants;
the text pane keeps one compact combat entry that updates in place. A player
can expand that entry to study the entire perceived encounter history.

This settles presentation, not combat speed. Neither attack cadence, turn-based
versus real-time resolution, health/injury rules, PvP consent nor tactical input
has been chosen by this decision. Slower effects must never slow the server,
and a fast simulation must not force the player to read every line immediately.

## Local-map layer

Keep the existing upright `W`, orbiting `>` and continuous movement. Effects
are sparse glyph/vector marks, not illustrated wolf animations. Suggested
vocabulary to prototype (not final color or timing choices):

| Observed event | Simple cue |
| --- | --- |
| Committed attack | Short directional stroke or arc toward the visible target |
| Contact | Brief impact mark such as `*` at the observed contact point |
| Miss or evade | Fading offset stroke or open arc, distinct from contact |
| Defend or parry | Small bracket/shield-like outline, distinct from damage |
| Disengage | Brief retreat cue only when the retreat is actually observed |

Never show prose, a full pose or a miniature combat log on the map. Do not add
permanent equipment sprites, move a wolf to fit an effect, force a false facing
change or introduce collision/hit targets for decorative marks. All outcomes
come from authoritative combat events, not a client animation deciding a hit.

Effects are map-clipped and short-lived, with bounded visual concurrency and
restrained intensity. They must remain distinguishable through weather and
lighting without erasing those conditions. No full-screen flashing or mandatory
camera shake. Reduced-motion mode uses static brief marks; distinguish outcomes
by shape as well as color. Combat cues must not repurpose a player's chosen
speech color as a damage/type indicator or cover typing/speaking indicators.

## Narrative layer

Recommended default: one collapsed entry **per perceived encounter**, anchored where that
encounter first enters this viewer's feed. Each new action replaces the summary
text in that existing entry; it does not append another top-level chat message.
For example: `Combat · 12 actions · Latest: Bracken evades a bite. [Expand]`.
Names and details in examples apply only when the character can perceive them.

Expanded: show all permitted actions in authoritative order, with sequence/time,
participants where identifiable and clear outcomes. Continue appending inside
the same encounter entry, not as independent chat posts. The header still shows
the latest action and encounter state. Collapsing returns to the newest action;
expanding again restores reading position rather than jumping to the bottom.

Keep an expanded log in a bounded-height internal scroll area. When the player
has scrolled up, preserve their position and expose a new-action count/Jump to
latest control. Auto-follow only while already at the live end. New actions must
not steal keyboard focus, erase a draft, interrupt an ongoing roleplay reveal or
force the outer narrative feed to scroll. Keyboard-expand/collapse and readable
focus/expanded state are acceptance requirements, not pointer-only extras.

Finished encounters remain expandable historical entries with an ended state.
Several distinct visible fights get separate encounter IDs/cards; do not merge
unrelated fights into a single room-wide ticker. Encounter merging/splitting and
participant changes are resolver responsibilities, not proximity guesses in UI.

## Order and timing

Combat events need stable event IDs, encounter IDs and authoritative sequence
numbers. Duplicate delivery or reconnect replay cannot repeat an outcome,
duplicate a log row or replay an old animation as a fresh strike. Display the
latest sequence, not simply whichever network packet arrived last.

Roleplay remains one speaker-owned post revealing at a time. Combat updates a
separate structured entry and presents immediate permitted cues without waiting
behind a long IC post. It cannot splice text into that post or bypass the normal
IC speech queue. Damage and movement commit on the server regardless of either
presentation queue. Reconnect backfill populates history without replaying an
entire old battle's effects. A received end marker does not discard late-arriving
earlier history; retain a stable ordered record.

## Perception and privacy

“Entire log” means the viewer's entire **perceived** combat log, not omniscient
combat telemetry. Filter each event on the server at event time. Expanding,
reconnecting, seeing an enemy later, or changing graphical settings must never
reveal previously hidden names, positions, exact damage or unseen actions.

- Two visible combatants may receive a connecting attack/contact cue.
- If only a victim/impact is visible, show only the permitted local impact,
  never a line pointing back to an unseen attacker.
- Audible but unseen fighting may produce a suitably vague heard event; it
  does not create a targetable wolf, exact hit marker, position or attack line.
- Scent alone does not identify a fight, disclose its actions or grant a
  combat target. Preserve the existing sight/hearing/smell separation.
- Moving to another cell clears inappropriate local effects but does not erase
  already perceived history. Hidden fighting must not leak through exact event
  counts, encounter-participant lists, durations or completion notifications.

Per-viewer event IDs/cursors should not expose gaps that enumerate hidden global
actions. An unseen encounter ending is not automatically knowledge the viewer
acquires; use a neutral out-of-perception state if necessary.

## Storage and authority

The server owns encounter resolution and a retrievable per-viewer event journal.
The UI owns expansion state, current scroll position and transient effect age.
For long encounters, paginate the journal and virtualize rows rather than keep
unbounded widgets. The existing bounded chat queue must not silently truncate
the promised full encounter history. Define retention and recovery before
claiming complete historical logs across arbitrary offline periods.

Do not resend the entire encounter in every world snapshot or ask an LLM to
author mechanical outcomes. NPC narration can later describe permitted facts;
it cannot invent a hit, interrupt authority or mint a reward.

## Integration with the current prototype

- `RatwGameMode::Publish` demonstrates per-observer projection, but combat needs
  a distinct event type. Do not pass mechanical actions through `ParsedPost`,
  speech/typing markers, social XP or roleplay participation scoring.
- `SRatwGame::ReceiveEvent`, `FPost`, the feed layout and `Tick` currently form a
  flat painted-post model and serialized IC reveal. Add a distinct encounter
  entry/model rather than making combat look like another speaking character.
- Current new-post behavior resets `TranscriptScroll`; combat updates must not
  use that path. The present 300-post UI/256-event controller caches cannot
  satisfy complete encounter history. A bounded cache needs retrievable older
  pages and an explicit retention policy, not silent loss.
- `DrawLocal` is the map hook. Reliable events and unreliable world snapshots
  can arrive on different timelines: authorize event-time cell/location/expiry
  on the server and never reconstruct an attack from hidden current actor state.
- The feed is presently painted text. Use real keyboard-focusable disclosure
  controls with expanded state instead of claiming accessibility for a drawn
  triangle. Enter/Space may activate a focused disclosure; otherwise retain the
  existing Enter chat, Escape draft/travel and Page Up/Down pace semantics.

## Delivery and acceptance gates

1. Define the minimal resolver/event contract and encounter lifecycle separately
   from presentation. An isolated synthetic fixture may test UI but is not real
   playable combat or a working Storykeeper brigand executor.
2. Implement a server-filtered, sequenced combat-event stream and recoverable
   per-viewer history with explicit retention limits.
3. Add map-cue rendering and an encounter-card model; separate them from roleplay
   reveal timing and decorative speech indicators.
4. Add collapsed/latest, expanded/history, keyboard controls, nested scrolling,
   unread counts, and reduced-motion behavior.
5. Test two simultaneous encounters, long fights, out-of-order/duplicate events,
   reconnect backfill, observer changes, hidden attackers, partial hearing,
   weather/darkness, frame-rate differences and long-form roleplay interleaving.
6. Capture a real two-client fight once the resolver exists and human-playtest
   readability. Verify hundreds of actions update a single collapsed row and
   every permitted action remains accessible when expanded. Log readability
   does not by itself prove comfortable combat pacing.
