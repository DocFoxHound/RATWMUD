# World stories in Dungeon Master

Open **Dungeon Master → Story Creator → World stories**. Choose a story, edit its phases, and run **Rehearse all paths**. A player can also write and rehearse a program offline and give its JSON file to a DM. A Studio viewer can draft and rehearse. A DM can approve a release for the selected world. On a hosted world, a faction-relation change needs a builder or owner.

| Example | What makes it advance |
| --- | --- |
| A wheel for the south road | Speak to the carter, then hand that resident an actual cart wheel. Its maker and quality can vary. The wolf who delivers it earns the deed. |
| The small blanket | Follow two witnesses in order. The teacher can receive a guest line if the DM invites a guest author for that visit. The authored clue works on its own. |
| Before the Wardens arrive | Each volunteer must check supplies with the provisioner and then report to the innkeeper. At 60%, the town schedules a festival. The last journal entry follows two game days later. |
| When the ferries stop | Survey the stranded passengers, rig a ferry line, then settle the accounts. Each of the three playable phases has two required missions and an optional one. |
| The cost of holding the road | A volunteer brings a bandit camp into play. The blockade changes market conditions and the watch’s stance toward the road company. A camp clearance begins recurring watches. A caravan or an authorized accord can bring peace. |

The bundled programs are templates. Use **World references for an author** to find the intended residents, cells and factions on the destination world. In particular, the war example needs the two named factions or their local replacements. Approval checks resident, player, map and faction IDs used by effects. The game makes the final decision on current world conditions, including placement, eligibility and economic rules.

## Event library and campaign missions

Under **Event library**, search the 100 templates by title, objective or subject, or filter by scale and play style. **Start a story from this event** builds a standalone draft with its opening trigger, ending effects and rehearsals. **Add its mission to this phase** copies the objectives into the selected phase. Set the campaign’s opening, consequences and rehearsal cases for the combined story. **Write a custom mission** starts with a blank objective.

A phase can offer up to 16 concurrent missions, each with one to eight objectives in order. A matching fact can advance one objective in each open mission. Set the required amount to count occurrences or add the units or coins recorded by an event. For example, four ropes can arrive in several gifts, and a marked or crafted rope counts toward a base rope request. Use filters to identify a recipient, item, place or detail when the action must be specific. A general letter or notice objective counts any matching letter or notice. A later objective can also match a field from the first objective using `sameAsFirst`. The missing-wagon search binds the notice ID in `detail`, so an answer to a different notice leaves the search open. The tool-chest mission uses the same rule for loan IDs.

| Play style | Who makes progress | When the mission is ready |
| --- | --- | --- |
| Individual | Each eligible character has a separate checklist. | The configured number or percentage has finished all its objectives. Other players can still finish while the phase stays open. |
| Group | Eligible contributors share one ordered checklist. | All objectives are complete and the specified number of distinct characters has contributed. The minimum is two. |
| Either | One character can finish the checklist, or several can share it. | All objectives are complete. Every contributor receives credit. |

Standalone individual templates keep the mission open for every character in the opening audience. Choose **Specific players** to run one for a single character, or change the finish threshold for a wider campaign.

Choose **missions completed** as a phase trigger, select the required missions, and choose the next phase. All selected missions must be ready. Leave a mission unchecked to offer a side quest. Other exits can branch the story or enforce a deadline. Each new visit to a phase opens fresh missions, including a return for a recurring event. Player journals show the character’s own instructions, amounts and credit. The DM can inspect archived progress by visit.

**When the ferries stop** shows how these rules fit together. In its first phase, 60% of the opening audience must each interview the carter and report to the innkeeper. Meanwhile, anyone can send the two dispatches, and a volunteer can separately copy the manifest. In the next phase, one wolf can purchase all the provisions, but at least two must contribute to the ferry line. The final phase opens once both jobs are done.

The catalogue is available in [event-library.json](../Data/Storylines/event-library.json). The complete campaign and its rehearsal cases are in [river-relief.json](../Data/Storylines/programs/river-relief.json).

## Writing a program

The portable format is `ratw-story-program`, version 1. A program has an ID, title, audience, opening phase, phases and rehearsal cases. The examples under `Data/Storylines/programs/` are complete programs. The catalogue returned by the author API lists every supported event and the fields of each effect.

* A phase has a unique `id`, player-facing `journal`, entry `effects` and ordered `exits`. An empty exit list ends the run. Revisit a phase to run its entry effects and start its timers and participation counts again. Only reached entries enter the player’s journal.
* An exit requires `on` and `to`. Its optional `match` object compares exact actor, target, cell, item or detail. `minCoins` and `minQuantity` test a single transaction. A gift filter with a base item ID accepts crafted and marked variants. Use `arrival` for signing in and `enter place` for walking across a boundary or entering through a door.
* Use `participant: true` for any member of the release audience. Use `players: 3` for three different participants or `percent: 60` for a percentage, rounded up. A fixed audience of one makes a personal run. Active audiences are player **characters** with recorded activity in the chosen lookback of 1–90 real days. Publication freezes the set. A player disconnecting leaves the denominator intact. Each run supports up to 500 characters. Add ordered `prerequisites` to a participation gate to require several events from the same character before counting them. Each prerequisite has an event type in `on` and optional `match` filters.
* Use a `clock` exit with `afterDays` for a deadline or repeat interval in game days. A delay on another event requires that event to occur after the delay. The first qualifying exit wins. When a recorded event arrives at a deadline, the clock is evaluated first. A daily recurrence resumes from the observed game day after a long outage.
* Effects can supply dialogue, move a resident, start a personal quest, award a deed or a lesson under ordinary practice rules, call bandits, schedule a festival, open a town project, change market conditions, or change one faction’s stance toward another. Specify both directions to change both factions’ attitudes. A player effect can target `$actor` when every entrance to the phase requires a participating player. World-wide effects such as a festival also affect players outside the journal audience.
* A `dm signal` exit names its exact direction in `match.detail`. An optional phase `guestVoices` list names the residents whose dialogue may receive a guest line. Add automatic alternatives to any scene that should keep moving when nobody is directing it.
* Each rehearsal gives a name, synthetic cohort, ordered events with unique IDs and game days since opening, and an expected phase sequence. Include wrong items, outsiders, repeat contributions and missed deadlines. Each rehearsal also records the effects in order with `$actor` resolved to the actual participant. Include `expectEffects` to assert that sequence and its recipients. The world’s queue receipts show how the game actually handled those effects.

## Approval, publication and operation

Approval requires passing scenarios covering every reachable phase. The digest covers the entire document, including audience, tests, guest speakers and effects. Approval saves that revision in its target database and returns a publication ticket valid for one day. Clicking **Publish approved story** or handing the ticket to the bridge starts that exact release. Reusing the ticket returns the existing run.

The runner observes checkpointed world events, persists its cursor and votes, and queues each entry’s journal message and effects in order. It waits for a receipt before sending the next effect. A refusal or expiry pauses the run and records the reason. Refresh **Releases** to inspect votes, phase history, queue receipts and delegated lines. **Stop run** cancels future phases and queues journal closure. Previously delivered effects, including faction changes, persist under their normal game rules. Once the underlying condition has been corrected, **Retry refused action** resubmits that one effect and preserves earlier progress. A changed program requires a new approval.

The packaged world starts and supervises the runner after migration. On an unpackaged server, run the following under the world’s DM service credentials alongside the game. Campaigns with missions require PostgreSQL migration 0068 and the corresponding game server, client and Studio builds. It uses no model or AI key.

```sh
python3 tools/story_runner.py --world WORLD_KEY
```

## AI authoring and direction

Run `python3 tools/story_author.py` as a JSON-lines subprocess. Send one JSON object per line. Each reply contains either `ok: true` and `result`, or `ok: false` and `error`. The offline bridge reads packaged examples and an optional context export selected by the operator.

| Request | Result |
| --- | --- |
| `{"op":"catalog"}` | Event and effect catalogue and complete examples |
| `{"op":"draft","example":"wagon"}` | A portable program to edit and return to the human |
| `{"op":"draft","eventType":"war-footing-at-the-gates"}` | A standalone event program with objectives and playable rehearsal cases |
| `{"op":"validate","program": …}` | Version fingerprint, or a validation error |
| `{"op":"rehearse","program": …}` | Expected and actual phase paths, effects and optional speakers |
| `{"op":"atlas.search","query":"bridge"}` | Up to 100 matching resident, area and faction names and IDs |
| `{"op":"npc.context","npc":"rm119"}` | Authored biography and voice, remembered summaries, attributed beliefs and recorded events |

Export author context from Studio and pass its file using `--snapshot FILE`. A trusted operator can instead host the bridge with `--world WORLD_KEY --read-world` for scoped reads. Treat memories and beliefs as a resident’s perspective. The recorded event history identifies who was involved and where. The context omits account data and raw conversation transcripts.

A trusted bridge launched with `--world WORLD_KEY` can redeem tickets over its stdin. Give the author access to that interface, and keep the host’s DM credentials on the host.

| Ticket given by the human | Request |
| --- | --- |
| Publication of an approved revision | `{"op":"publish","ticket":"…"}` |
| One named direction in the current phase visit | `{"op":"signal","ticket":"…"}` |
| One line for a named resident in the current phase visit | `{"op":"voice","ticket":"…","text":"I remember a much smaller set of tracks."}` |

Direction tickets expire in one hour and can be redeemed once. A voice ticket sets one line of up to 200 characters for the selected resident’s conversations with participants in that phase. Moving on retires it. An author holding a voice ticket can read a separately authorized resident context, compose the line and submit it. Keep a fallback authored line for runs that never receive a guest contribution.

Studio uses authenticated `POST /api/dm/story-program` with `target`, `op` and operation-specific fields. The human operations include `approve` (program and reviewed digest), `status` (optional run ID), `stop` (run), `signal-ticket` (run and signal) and `voice-ticket` (run and NPC). Only the human-facing DM interface issues tickets. The offline author bridge has no approval operation.

The catalogue exposes `eventTypes`, `missionEvents` and `missionMeasures`. Studio also accepts `draft-event` with an `eventType` ID. Each rehearsal returns `missionVisits` with the credited player IDs for each mission by visit. Use `expectMissionVisits` to assert the entire sequence of mission credits, including repeated phases. Use `expectMissions` in a case to assert credits in the final active mission phase or the mission phase immediately preceding the ending.
