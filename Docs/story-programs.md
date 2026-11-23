# World stories in Dungeon Master

**For admins, start with the Story Maker's guide: `Docs/Story-Maker-guide.md` (also the Dungeon Master suite's **Guide** tab).** It walks through a small quest in five minutes and a campaign of chapters, and explains every event, condition, action and effect from the trigger catalogue with an example. This page is the format in detail.

Open **Dungeon Master → Story Creator → World stories**. Choose **New blank story**, compose triggers in a phase, and run **Rehearse all paths**. Use **Start from a story** to inspect a worked example. A player can also write and rehearse a program offline and give its JSON file to a DM. A Studio viewer can draft and rehearse. A DM can approve a release for the selected world. On a hosted world, a faction-relation change needs a builder or owner.

| Example | What makes it advance |
| --- | --- |
| The breach at Greyfen | Three bandits fall inside the breach. A runner carries the report through the barracks door. Two guards must reach their posts before relief missions open. |
| Ninety days on the winter ferries | A weekly watch and a spring council continue across phase changes. The winter changes household prosperity. At day 90 the ferry stores determine the market outcome. |
| Two ways across Reed Ford | Deliver rope or negotiate passage. Both routes send the same signal and award the wolf who opens the crossing. A notice or a two-day timer settles the story. |
| A wheel for the south road | Speak to the carter, then hand that resident an actual cart wheel. Its maker and quality can vary. The wolf who delivers it earns the deed. |
| The small blanket | Follow two witnesses in order. The teacher can receive a guest line if the DM invites a guest author for that visit. The authored clue works on its own. |
| Before the Wardens arrive | Each volunteer must check supplies with the provisioner and then report to the innkeeper. At 60%, the town schedules a festival. The last journal entry follows two game days later. |
| When the ferries stop | Survey the stranded passengers, rig a ferry line, then settle the accounts. Each of the three playable phases has two required missions and an optional one. |
| The cost of holding the road | A volunteer brings a bandit camp into play. The blockade changes market conditions and the watch’s stance toward the road company. A camp clearance begins recurring watches. A caravan or an authorized accord can bring peace. |

The bundled programs are templates. Use **World references for an author** to find the intended residents, cells and factions on the destination world. In particular, the war example needs the two named factions or their local replacements. Approval checks resident, player, map and faction IDs used by effects. The game makes the final decision on current world conditions, including placement, eligibility and economic rules.

## Where the rules run

The native `ratw_story` library evaluates live event–condition–action rules. The game server records the action and any witnessed tile, moves the residents, delivers speech and applies world effects. The Python runner reads the event log, schedules deadlines, tracks journal missions and commits each decision with its event cursor. PostgreSQL keeps approved program documents, run progress, trigger history and game receipts. Authors can change the approved story content through Studio without changing the server code. Source-only rehearsals can use the Python implementation offline.

Install the matching runner, Studio, migration 0070 and native build together. Building the `ratw_server` target builds its story library. For a source deployment in a custom build directory, point `RATW_STORY_LIBRARY` at that library.

## Drawing a breach and directing a scene

A named region is an inclusive rectangle within one map cell. Draw its opposite corners, name it, then choose that region in a trigger condition. A tile on the boundary is inside. A matching pair of coordinates in another cell is outside. Historical records and world-wide events that lack a witnessed tile cannot establish that something happened inside or outside the rectangle. A named story signal carries the original fact’s location. A timer expiry has no witnessed tile. To detect the crossing itself, listen for `region entered` or `region left` with the region ID as the target. Entry records the first tile inside. Exit records the last tile inside. Starting a run or logging in establishes the starting position, so a player already inside must leave and return to cause an entry. A camp can spawn in the rectangle. An area effect can damage eligible occupants, give them items, or apply the timed fed benefit. Occupancy is sampled by the game when it applies the order.

In **The breach at Greyfen**, the gap is the south road in the bundled town, tiles (29, 34) through (34, 38). The story opens a bandit camp there. Each `bandit falls` event names the defender who landed the last blow and records the tile where the bandit fell. The area condition tests the fallen enemy’s tile. The scenario awards one personal defeat to the eligible defender and adds one to the shared total. Three defeats by one defender or by several open the same next phase.

Harrow takes the king’s part in Greyfen’s watch muster. He sits at the command post while Sloe runs from the gap to the barracks and delivers her report. Harrow rises, crosses the hall and calls Birch and Tamsin forward. Each guard’s confirmed arrival counts once. Once both reach their posts, players can take the supply and roll-call missions. A blocked actor can send the scene to its reserve ending. The example uses ordinary Greyfen doors and residents. For a different palace, bind the throne, route, breach and cast to that world’s cells and residents.

Each order has a short cue chosen by the author. A walk produces `scene arrived` after the resident reaches the destination tile. A spoken line and a change of posture produce `scene spoken` and `scene posed`. Use those events, the speaker or mover’s ID, and the cue for the next beat. A live scene cue belongs to its release and phase visit. **Rehearse all paths** exercises a shared defense, a solo defense, misplaced or unlocated kills, and an interrupted courier. [Greyfen siege program](../Data/Storylines/programs/siege-at-greyfen.json)

## Reusable scenes and ceremonies

Open **Dungeon Master → Story Creator → Scenes** to write a ceremony once, rehearse its different endings, and export it as a `ratw-scene` JSON file. For a one-off performance, choose **Rehearse standalone release**, then review, approve and publish the resulting release. To give a campaign a performance, import the scene into its world-story draft and add a **Play scene** effect. The approved campaign includes its own copy of the beats, cast and text.

**The watch takes its oath** is set in Greyfen’s square. Harrow walks to the speaking place. A merchant recalls the previous winter. The audience chooses where to carry the first lantern. A majority for the bridge ends the ceremony at the crossing. A majority for the south road lets a volunteer type who the patrol should look for. Harrow can answer in character through the host’s linked NPC Mind. With ordinary authored playback he promises to meet the last carters. If nobody replies, he acknowledges their silence and gives the watch its charge. [Ceremony document](../Data/Storylines/scenes/greyfen-oath.json)

- **Cast and framing.** Bind a role to a particular resident, or choose by role or cell. The game selects the cast at the start and keeps those residents for that performance. Give each line its own reading time in real seconds. A camera point frames a map tile for viewers.
- **Speaker panels.** A scene is the one place where players and residents speak outside the dialogue window (doc 84 §13). Everyone who speaks during a scene gets their own panel with their painted face and name. This includes each cast resident, each player who types a reply, and a player who speaks in character nearby. The panels stack with the newest at the bottom and the oldest faded, up to four at once. A player’s face and name appear only to viewers who are shown that player, the same rule as for faces beside chat lines. Anyone else sees “Someone” with no face. Typed replies reach the scene’s audience and the linked AI only. They stay out of the event log and the saved story.
- **Movement lock.** The lock holds only participants who have joined. For a private or party scene, that is its audience from the first beat. For a public scene, it is a viewer who votes or replies. Walking into a public scene shows it without holding you. No one is held for more than five minutes, however long the scene runs, and a fight always releases the lock.
- **Audience.** A public performance has a rectangular viewing area. Players walking in see the current beat. The players present when a question opens form its electorate. Later arrivals can participate at the next prompt. Choose **individual** for a private encounter or **party** to capture the initiating player’s current party. Private choreography and dialogue are visible to that audience.
- **Voting.** Give each choice a reading and voting interval in real seconds. In first-response mode the first eligible ballot selects its branch. A strict majority settles a majority vote early. On expiry the largest vote total wins, with the authored default resolving a tie or silence.
- **Written responses.** Choose first reply or replies from all eligible participants. Set a next beat for submitted answers and an optional alternate beat for a timeout. A campaign can also act on the result. For example, a `scene response` trigger matching `first-watch.name-the-watch.submitted` can open a patrol mission, while `first-watch.name-the-watch.timeout` can send a messenger. Players see a notice when their words can inform a linked AI. Free text stays out of the public event log.
- **Linked dialogue.** On a spoken line, choice or written prompt, select **Use linked AI for this line**. In **Personality**, describe how the resident speaks. In **Response intent**, say what they want to accomplish here. The game supplies up to eight recent answers with their questions, plus the resident’s chronicle when available. Write the fallback line as you want players to hear it. When the provider is offline, busy or slow, the game plays the fallback and gives players the full reading or voting interval.

For an embedded performance set the effect’s `scene`, `player` and `cue`. A public ceremony can use `world`; a private scene needs a specific character or the participating `$actor`. A cue of `first-watch` with winning option `carters` produces a `scene choice` with detail `first-watch.carters`. A trigger can remember that result in a counter. Wait for `scene finished` with detail `first-watch` before opening the patrol mission so the closing speech has time to play. Use `scene failed` for an interrupted performance.

Studio and the author bridge accept requests of up to 1 MiB. A rehearsal also runs within a work and time budget: about 200,000 dispatched facts, signals and fired rules, or 8 seconds. Only two rehearsals run at once. A request over the budget gets a polite refusal asking for fewer or shorter rehearsal cases. Every bundled example rehearses well within the budget.

The author API exposes `scene.draft`, `scene.validate`, `scene.rehearse`, `scene.import` and `scene.wrap`. For the ceremony, request `scene.draft` with `"example":"greyfen-oath"`. Rehearse the road ending with `"choices":["carters"]` and `"responses":{"name-the-watch":"submitted"}`. Use `"timeout"` to inspect the silent branch. The bridge ending can be rehearsed with no submitted answers.

## Composing triggers

Story Creator opens in **World stories**. Select a phase in the campaign strip. The trigger table shows its events, conditions and actions side by side. Select a row to edit it. **Signal connections** opens the triggers that send or receive a signal and identifies its journal listeners. Start with **New trigger** or open **Components from the 100 examples**. Search by event, objective, title or subject, select a component, and copy it into the current phase. The example picker exposes 205 objective components, 17 opening events, 100 participation rules, and 13 outcomes. Each objective component supplies both an event rule and a journal listener. You can use either piece on its own. **Complete mission examples** also opens a whole editable example with its rehearsals.

To give players two ways to achieve the same outcome:

1. Add a trigger for a gift. Set its recipient and item filters, choose **participant** as its audience, and add the event’s quantity to a story counter named `rope`. Add a second action that sends `inspect`.
2. Add another trigger for a sealed pact. Set a story counter named `charter` to 1 and send the same `inspect` signal.
3. Have a third trigger listen for `inspect`. Under **Conditions**, choose **any** and compare `rope >= 4` and `charter >= 1`. Send `open-crossing` when either passes.
4. Listen for `open-crossing`, award the participating wolf a deed, then enter the next phase. A journal objective can listen for the same signal. **Rehearsal cases** lets you supply the character cohort, facts, game days and expected path. Open **Trigger trace** to see which rules actually fired.

[Complete Reed Ford example](../Data/Storylines/programs/reed-ford.json)

| Rule part | Available choices |
| --- | --- |
| Event | World events from the catalogue, a named story signal, a phase timer, or a named timer set against the world calendar or elapsed real time. |
| Conditions | Nest **all**, **any** and **not**. Compare event fields or counters, or check a mission’s completion threshold. |
| Actions | Set or add to a counter, emit a named signal, start or cancel a timer, apply a world effect, or enter a phase. Set resident dialogue in the destination phase to give it a defined lifetime. |
| Participation | Use the whole world or the opening cohort. Fire on every match, once per visit, or once per eligible character per visit, once per campaign, or once per eligible character for the entire campaign. |
| Memory | Story counters are shared. Player counters belong to the acting cohort member. Both start at zero, survive phase transitions and persist across runner restarts. A counter set to 0 or 1 works as a switch. |

Within one phase visit the engine evaluates triggers in display order. Counter changes are visible to subsequent rules. Named signals retain the originating actor, item, target, place, quantities and detail. The engine delivers them in emission order after the rules for the current event, once per signal name per world fact. It rejects signal cycles. A phase transition ends dispatch in the old phase, including any queued signals for that visit. Put **Enter a phase** last in a chain. Each journal mission can credit at most one objective per originating fact.

**Phase triggers** belong to one phase. **Campaign triggers** are shared across listening phases and run before local rules. The checkbox in the campaign tab controls whether the selected phase listens. By default, a phase with local rules or exits listens, and an ending stays closed. A campaign rule can set a counter in one phase and act on it in another. Mission-completion conditions refer to the journal of the current phase and belong in local rules.

To schedule work after an arbitrary event, choose **Start a named timer**. Set a signal, delay, clock, lifetime and optional repetition. A timer can live for one visit or cross phase boundaries. Starting the same name for the same owner replaces its deadline. The signal carries the scheduling character. **Cancel a named timer** stops future expirations. A real timer counts 24-hour days since publication and keeps time while the game calendar stands still. Missed repeats coalesce into one firing when the runner resumes. Independent overdue campaign timers survive a transition and are processed in the next listening phase. A release that has ended no longer dispatches timers.

The winter-ferries example includes two endings and a rehearsal that jumps directly from opening to day 98. To rehearse against wall time, set **Elapsed real days** on each fact where real time advances. **Game day** is a separate input. [Winter campaign](../Data/Storylines/programs/winter-ferries.json)

World effects go through the existing approval and receipt system. An effect can run while a phase stays open. A trigger that also enters a phase delivers its effects before that phase’s ordinary queued effects. The journal opens the new phase first. Available consequences include changes to NPC speech, location, protection and revival, character injuries and practice, public works, household prosperity, harvests, shipping, festivals, faction relations and markets. The game applies its usual eligibility and placement rules. Rehearsal traces list trigger IDs, resulting counters, intended effects and rules omitted by the scenarios.

### Version 2: variables and flow

A program of `"version": 2` (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 5) declares its memory under
`variables`: each `{"id", "type", "scope"}` with an optional first value (`initial`), a label to show it under (`show`,
numbers, switches and timers) and a `note`. Types: `number`, `switch`, `text`, `wolf`, `resident`, `place`, `region`,
`item`, `faction`, the lists `numbers`, `texts`, `wolves`, `residents` (up to 500 values), and `timer` (a moment: days
since it started, on the `game` or `real` clock). Scopes: `story` (one value), `player` (one per wolf), `party` (one per
party; a wolf in no known party is a party of one) and `world` (kept after the run, shared by every story of the world that
declares the name with that type; `dm.story_world_vars`). A misspelt name is an error, not a new counter.

Conditions add `compare` (two values: a constant, `{"var"}`, `{"field"}`, `{"count": list}`, `{"since": timer}`,
`{"each": true}`, or `plus`/`minus`/`min`/`max` of two), `switch`, `in list`, `phase is`, `visit is`, `timer` (running,
or less than N days left), `fired` (a trigger's firings this visit or run), `chance` (seeded; a rehearsal case may pin
`rolls`) and `days since` (the phase, the run, or a timer variable). A variable of each wolf is read `of` `$actor` (the
default) or `$each`. Actions nest: `if` (with `elif` and `else`), `each` (over the participants, the acting wolf's party
or a list; `$each` in effects and variables), `random` (weighted choices), `wait` (the rest of the list later, for the same
wolf and event; a campaign trigger's survives the phase), `run`, `enable` and `disable` (a trigger with `"enabled": false`
starts off), `stop`, `note` (a line in the timeline only) and `finish` (an outcome for everyone, which ends the run, or
for one wolf). State actions: `set`, `add`, `subtract`, `multiply`, `clamp`, `turn on`, `turn off`, `toggle`,
`remember`, `list add`, `list remove`, `list clear`, `pick`, `start timer`, `stop timer`, and `shift` for a named timer.
Up to 24 actions a trigger, counting nested ones, nested three deep. Rehearsal cases may add `seed`, `rolls`, `parties`,
`expectVariables` (only those named) and `expectOutcome`. A version 1 program runs exactly as before; the workbench's
**Make it version 2** (the op `upgrade`) declares its counters as numbers and rehearses the same. The bundled
[breach at Greyfen](../Data/Storylines/programs/siege-at-greyfen.json) is version 2.

## Campaign missions

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

## Quests and a living audience (version 2)

A version 2 story may declare up to 32 `quests` (Docs/Design/85-story-triggers-and-campaign-control.md, 3.6 and Phase 6). A
quest is a mission that any wolf the story lets in can pick up, that lasts the campaign (or, with `"lifetime": "phase"`, its
phase), and that carries its own rewards. In the Story Creator, **New… → A small quest** makes one in a few questions: who
gives it, what to do, the reward.

| Field | Meaning |
| --- | --- |
| `scope` | `player` (each wolf's own copy), `party` (one copy for the party the wolf was in when it started; its members are fixed then, and one who leaves keeps the credit earned so far), `story` (one copy for everyone; `contributors` different wolves needed) |
| `objectives` | 1–12: an event of a wolf (or a story signal) with filters, `goal` and `measure` as missions; `after` names the objectives that must be done first (none: open from the start); `optional`; `hidden` (not in the journal until revealed or done) |
| `giver`, `offer` | The resident who offers it in their dialogue window and takes it back: the wolf's words to ask, the resident's offer in their own voice (`say`, which may use the talk library's `#symbols#`), accept, decline, hand in, thanks |
| `handIn` | With a giver (the default), a quest whose required objectives are done is **ready** and finished by handing it in to the giver |
| `join` | `who` may take it (`anyone` in an open or everyone story, else `participants`), the story `region` to stand in, `cap` (copies under way at once) and `limit` (starts in all) |
| `phases`, `repeat` | When it is offered; `once`, `daily`, `weekly` or `always` again after it is done (a failed or abandoned quest may be taken again at once) |
| `fail` | Up to four: an event (`{"on": "death", "match": {"actor": "rm111"}}`) or a deadline (`{"afterDays": 3}`) |
| `rewards`, `bonus` | Story effects given when it is done, to each wolf with credit where they name `$actor`, once otherwise; the bonus when every optional objective was done too. Coins come from a named purse, never minted: `character.grant` with `purse` (`town:<town>` its treasury, `town:<town>:works`, `watch` or `church`, `till:<id>`, `chapter:<id>`), refused when the purse is short (the story notes it and carries on). Also `item.give`, `standing.adjust`, `deed.award`. |
| `abandon` | Whether the journal lets a wolf put it aside (default yes) |

An audience may be `{"kind": "open"}` (nobody at first: a wolf comes in by taking a quest or by `participant.add`) or
`{"kind": "everyone", "days": 30}` (the wolves active in the last 30 days, and each who signs in), each with an optional
`cap` (up to 5,000). A phase with quests open in it goes on without exits or triggers: a small quest is one phase and one
quest. Triggers can `quest.offer`, `quest.start`, `quest.complete`, `quest.fail`, `objective.complete`, `objective.fail`,
`objective.reveal`, `objective.progress`, `participant.add`, `participant.remove`, and lay a choice over a resident's window
with `npc.offer` (the wolf's words, the resident's reply; picking it is a `dialogue choice` event). They hear `quest accepted`,
`quest declined`, `quest handed in`, `quest abandoned`, `party joined`, `party left` from the game, and the story's own
`quest started`, `quest completed`, `quest failed` and `objective completed`. A rehearsal case may give an open story an empty
`cohort` and assert `expectQuests` (`{"quest": {"wolf" | "party:<id>" | "story": "done"}}`).

The journal shows each quest with its objectives (optional, hidden, failed, counts), what must come first, its deadline,
"return to" its giver when ready, the story's shown variables as trackers, and Abandon (or Accept and Decline for a quest
offered with no giver). The Campaigns console's **Quests** tab is the grid of wolves (or parties, or the story) and
objectives, with Complete, Set progress, Reveal, Fail, Reset, Give the quest, Take it away; **Wolves & map** adds and removes
wolves. Each wolf's own state of such a story is kept in `dm.story_player_state` (migration 0075).

## The world's facts and the watched events (version 2)

A version 2 condition may read the world as it was when the event happened (Docs/Design/85-story-triggers-and-campaign-control.md,
3.3, 3.4 and Phase 8): where a wolf is, what they carry, the hour, the weather, standing, a resident's state, a quest's state.
`who` is `$actor` (the default), `$each` inside For each over wolves, or a wolf's id.

| Condition | Parameters | True when |
| --- | --- | --- |
| `wolf here` | `who` | the wolf is in the world (signed in) |
| `wolf in region`, `wolf in place`, `wolf in town`, `wolf indoors` | `who`, `region` / `place` / `town` / `is` | where the wolf stands now |
| `count in region` | `region`, `op`, `value` | so many of the story's wolves stand in the region (StarCraft's Bring) |
| `carries`, `coins` | `who`, `item`, `op`, `value` | what the wolf has in their pack (a marked or crafted item counts as its base item), their purse |
| `standing`, `reputation` | `who`, `faction` or `town`, `op`, `value` | their Chapter's standing with a faction or House (-100..100); what a town's residents who know them think (liking less disliking) |
| `wolf is` | `who`, `state` (`downed`, `hurt`, `wanted`, `held`), `is` | downed in a fight, carrying wounds or an injury, wanted by the watch, held in a gaol |
| `party size`, `in party with` | `who`, `op`, `value` / `other` | the wolf's party (1: alone); both in one party |
| `resident is`, `resident in place`, `resident in region`, `resident has` | `npc`, `state` / `place` / `region` / `what` (`job`, `mood`, `mantle`, `town`) and `value` | a named resident alive or dead, indoors, where, their kind of work, mood, mantle, town |
| `hour between`, `calendar is` | `from`, `to` / `what` (`weekday`, `season`, `period`), `value` | the hour (22 to 4 runs through midnight), Marketday or Restday, the season, dawn, day, dusk or night |
| `weather is` | `where` (`$actor`: where the acting wolf is, or a place), `weather` | clear, overcast, rain, storm, fog, snow or sandstorm (indoors: the weather outside) |
| `relation is` | `faction`, `op` (`=`, `!=`, `>=` at least, `<=` at most), `stance`, `other` | one faction's stance toward another, in the order war, hostile, tense, neutral, friendly, allied |
| `quest is`, `objective is` | `quest`, `objective`, `of` (`$actor`, `$each`, `story`), `status` | a quest's state for the wolf (their party's copy for a party quest) or the story's copy; an objective done, failed, to do or hidden |

A fact that is missing (the wolf is away, the game has written no facts yet) makes the condition **unknown**, and unknown never
fires, not even under `not`. The facts travel with the event log: while a story asks for them, the game writes one document
of the world's facts (`game.story_facts`, migration 0076) with every write of its events, in the same transaction, and the
runner reads each event with the facts written with it, so a fact is never older than its event (and at most a second newer).
A wait resumes with the facts of the moment it resumes. What the game puts in the document is worked out from the program
when it is approved (its **watch list**, sent with each phase): a story that asks nothing costs nothing. A rehearsal case
states the world as `facts` (`wolves`, `residents`, `world`, `weather`, `relations` by id, as the game writes them), and a
fact of the case may change it from then on, section by section (`null` takes a wolf or a resident away).

Twenty-four events are written by the game only while a live story listens for them (they are marked `watch` in the
catalogue): `door opened`, `door closed`, `door locked` (a wolf tried a locked door); `building entered`, `building left`;
`region occupied` and `region emptied` (the story's wolves in one of its regions go from none to one, or back; signing in
inside a region now counts as entering it); `weather changed` (in a place the story names, or where a wolf stands);
`hour`, `day begins` (with the weekday), `dawn`, `night falls`, `season begins`; `item picked up`, `item dropped`; `trade
done` (bought or sold, with the coins); `dialogue opened`, `dialogue node`, `dialogue closed`; `crime seen` (the witness,
the offender, the victim); `wolf downed`, `wolf revived`; `party formed`, `party ended`. The Campaigns console's Waiting for
shows the world as the game last told the story, and its Why not judges each logged event with the facts written with it.

## World effects, what is undone at the end, and who may send them (Phase 9)

The world effects of Docs/Design/85, Phase 9 (telling the wolves, journal notes, map markers, trackers, taking items, warrants,
strangers, mantles, holds, followers, doors, items on the ground, notices, the weather, market events, prices, projects, deeds,
personal quests, stopping a scene) are each in the catalogue with their parameters, a sentence and help; the guide lists them
all with an example. **What the game undoes when a run ends** (`story.stop`, `story.finish`, an ending phase): a door's lock
as it was before the story first touched it (`door.lock`, `door.unlock`), held residents (`npc.hold`), residents sent along
(`npc.follow`), strangers (`visitor.add`), mantles bound (`mantle.assign`), notices (`board.post`), the weather
(`weather.set`) and the maps' markers (`story.ping`); journal notes stay, and everything given (items, coins, deeds, standing,
warrants, relations, market events, prices, projects) stays. **The permission marks** in the catalogue drive every hosted
check: `actsOnWolves` (`character.injury`, `story.area.damage`, `item.take`, `warrant.issue`) needs the owner or "may act on
players' wolves"; `needsBuilder` (`faction.relation`, `standing.adjust`, and since Phase 10 `prices.reset`,
`project.complete`, `project.cancel`) needs a builder or the owner, to approve the story and to steer a control that could
release it. The rest need what the same act needs by hand on LIVE: a Dungeon Master.

## Chapters, world variables and other stories (Phase 10)

A **campaign of chapters** is several stories approved together, in order (`Campaigns → New campaign…`, op `campaign.approve`;
`dm.story_campaigns`, migration 0079). The first is published with a ticket; a chapter starts the next with the story's own
effect `story.start` (`story`: a chapter of the same campaign; `audience`: `same`, `own` or `list` with `list` a declared list of
wolves; `carry`: the story and per-wolf variables the next declares with the same name, scope and type start with this one's
values, yes by default; `when`: `ended`, the default, once this story has ended in the game so what it held is let go first,
or `now`). A story that starts another is refused when approved alone; a rehearsal case may expect `expectChapters`. Every
other live story that listens hears `story started` and `story ended` (target: the other story's id; item: its run; detail:
the campaign, or how it ended), through its command inbox. **World variables** may be declared for the world in Campaigns →
World variables (a type, a first value, a note; set and removed there with a reason, audited `story.world-var`; op
`world-var`); a story declaring one as another type is refused at approval. The events Phase 8 left are watched too: `near
resident` (names the resident), `purse crossed` (names the purse and the amount in `detail`), `standing crossed` (the faction
and the number), `relation changed`, `mantle bound`, `mantle released`, `skill practised` (a whole step), `injury healed`.

## Readiness and participation (doc 86, Phase 1a)

**The audience counted.** A version 2 program may give `activeDays` (1 to 120 game days, 18 when not given): a wolf of the
story is **active** when seen (the actor of any event) within that many game days. Values add `{"audience": "all" | "active"
| "online"}` (the story's wolves, the active ones, those signed in now: unknown until the world's facts say who is online,
and the program then watches the `wolves` facts), `{"times": [a, b]}` and `{"div": [a, b]}` (floored toward zero; by 0, no
value), and the share of the wolves who took part: `{"percent": {"list": <a story or world list of wolves>}, "of": "active"
| "all" | "online"}` (0 to 100) or `{"among": …}` (how many), with `{"where": <a condition tested for each wolf, the wolf as
$each>}` in place of the list. The condition `participation` (`list` or `where`, `of`, `percent` 1 to 100, `floor` and
`cap` 1 to 5,000; `source` "list" or "where" for the workbench) is true when the wolves counted are at least
max(min(floor, D), ⌈percent × D / 100⌉), then at most `cap`, never fewer than 1, where D is how many are active (or all, or
online); false with D = 0, unknown with online not known. A `where` holds `compare`, `switch`, `in list`, `days since`,
`quest is` and `objective is` (a wolf's own variables and quests read `of: "$each"`), groups, 3 deep and 8 conditions; no
facts, event fields, loop values or counting inside. Counting is not allowed inside For each, nor more than one level deep in
arithmetic. The runner keeps `state.seen` (each wolf's last game day, filled at launch from the world's log) and sends both
twins the same counts and ids.

**Readiness.** A version 2 phase may have `ready`: `{"all": [parts], "auto", "then", "slowAfterDays", "slowNeedsYou",
"autoAfterDays"}` and then exactly one exit `{"on": "ready", "to": <phase>}` (no other fields; it has no acting wolf). Parts
(1 to 8; `{"any": [2 to 4 parts], "label"}` one deep; each with an optional `label` of 80 characters): `{"participation":
{"list" | "where"}, "of", "percent", "floor", "cap"}`, `{"minDays": 0-3650, "since": "phase" | "chapter"}` (held days not
counted), `{"value": <number>, "atLeast": <number>}` (story and world values only), `{"test": <condition over the story's own
state>}`, `{"missions": [this phase's missions]}`, `{"quest": <a story-scope quest>}`. Once every part is met it is ready for
the visit (latched). `auto` (default false) takes the exit by itself, at once or at the `then` moment (`{"on": "day begins" |
"hour" | "season begins" | "festival" | "dm signal", "match"}`); otherwise it waits for the command `advance` (`{"when": "now"
| "then", "anyway": bool}`), or `autoAfterDays` after it became ready. `slowAfterDays` raises the `slow` alert (needs you with
`slowNeedsYou`). The runner measures it at most once a tick and at least once a game hour into `state.ready` (and the last 20
changes into `state.readyLog`); a held run is not measured, and its held days (`state.heldDays`) never count. Moving onto a new
version keeps the moment it was met when the phase and its Readiness are unchanged. Rehearsal cases may add `audience` (the
counts) and `activeWolves`, and among their events the steps `{"check": "ready", "met": bool}` and `{"advance": "now" |
"anyway" | "then"}`; the review lists `uncoveredReady`.

## Talk, rumours, figures, holds and purses (doc 86, Phase 1b)

**Talk memory.** `talk.remember {scope, town?, npc?, player?, key, value, days, keep?}` and `talk.forget {scope, town?, npc?,
player?, key}` (game orders, target the run): `scope` `realm` (no other part), `town` (with `town`), `npc` (with `npc`) or
`pair` (with `npc` and `player`, which must be `$actor` in a participant trigger or `$each` in For each over wolves, or a
quest's reward; never at a phase's opening). `key` is `[a-z][a-z0-9_]{0,39}`, `value` `[a-z0-9_]{1,40}`, `days` 1 to 365. The
game keeps them apart from talk's own memory (64 an owner, 16 a pair; 8,192 and 20,000 in all; the soonest to go first),
recalls them as `world.mem.<key>` (the realm's, then the town's over it, then talk's own), `self.mem.<key>` and `pair.mem.<key>`
(talk's own of the pair over it), saves them with the checkpoint (`people.storyMemory`), and forgets a run's when it ends
unless `keep`. **Rumours.** `belief.plant {subject, claim, town, count 1-50, confidence 10-90, claimKind?}` (target the
subject): `count` residents of the town (alive, with a home, not the subject; ordered by the run's seed) believe the claim
with the kind `story.rumour` (Data/Talk/claims.json: "#x.name# #claim.text#") or, with `claimKind: "story.cleared"`, its
clearing (believed as "never <claim>"). The claim is a phrase that follows a name: a small letter first, 120 characters, no
`#`, brackets or ids (a word with `_`, one that mixes letters and digits, or one that starts `wolf-`: the same rule in
`story_program.claim_has_id` and the game). **Figures.** `figure.set {figure, role, npc | player | name, title?, known: realm | region | house,
region?, house?, about?}` and `figure.clear {figure}` (needs a builder): the runner writes `live.figures` (as the Dungeon
Master's Figures panel does: `story_runner.write_figure`), keeps the row it replaced in the run's history (`entry.figure`),
and sends `voice.sync`; the op `figure-undo {history}` puts it back. **Holds.** `npc.spare {npc, lasts: run | campaign}`,
`npc.unspare {npc}`, and `npc.protect`'s `lasts` (`ever`, the default and the mark it always was; `run`; `campaign`): the
runner sends the run (and its campaign); the game keeps each resident's owners (`people.storyHolds`), skips a spared one in
old age, counts a protected one as protected, drops a run's owners when it ends and a campaign's on `story.campaign_end`
(sent once when the campaign ends; the op `campaign-release {campaign}` sends it for an act stopped early). `lasts:
campaign` is refused unless the story is approved as a chapter. **Choices for a list of wolves.** `npc.offer` with a `player`
offered again under the same id adds that wolf (`players`, up to 5,000; the words and reply the latest; who picked it kept).
**Told to residents.** A `player` (later `realm`) variable of type number, switch or text may have `"talk": true` (at most 8):
each wolf's stored value goes with their journal as `talk` (`{name: value}`: digits, `yes`/`no`, or text of `[a-z0-9_]{1,40}`;
other text is kept, not told, noted once), and the window knows it as `player.story.<name>`; each quest view as
`player.quest.<program id>/<quest id>` (the game keeps the program's id from `story.phase` and `story.quests`). **Purses.**
`till:job:<founder>` (and `till:<founder>`, mapped to it) pays from a business's till. At approval every purse a program
names (rewards, grants, `purse crossed`) must be an account of the world's latest checkpoint (a warning while it has none);
`town:<id>:works|watch|church` is refused. **Approval also checks** a quest's objective targets and items, its `board`
(one of the world's notice boards), and that every resident named (speaker, giver, subject, face, cast, objective target) has
a place and is alive (a face, `like`, only warns; `npc.kill` of the dead only warns; a `death` filter may name the dead).

## Writing a program

The portable format is `ratw-story-program`, version 1. A program has an ID, title, audience, opening phase, phases and rehearsal cases. The examples under `Data/Storylines/programs/` are complete programs. The catalogue returned by the author API lists every supported event and the fields of each effect.

* A phase has a unique `id`, player-facing `journal`, entry `effects` and ordered `exits`. A phase with neither exits nor triggers ends the run. A phase with triggers can stay open indefinitely. Revisit a phase to run its entry effects and start its timers and participation counts again. Only reached entries enter the player’s journal.
* An exit requires `on` and `to`. Its optional `match` object compares exact actor, target, cell, item or detail. `minCoins` and `minQuantity` test a single transaction. A gift filter with a base item ID accepts crafted and marked variants. Use `arrival` for signing in and `enter place` for walking across a boundary or entering through a door.
* Use `participant: true` for any member of the release audience. Use `players: 3` for three different participants or `percent: 60` for a percentage, rounded up. A fixed audience of one makes a personal run. Active audiences are player **characters** with recorded activity in the chosen lookback of 1–90 real days. Publication freezes the set. A player disconnecting leaves the denominator intact. Each run supports up to 500 characters. Add ordered `prerequisites` to a participation gate to require several events from the same character before counting them. Each prerequisite has an event type in `on` and optional `match` filters.
* Use a `clock` exit with `afterDays` for a deadline or repeat interval in game days. A delay on another event requires that event to occur after the delay. The first qualifying exit wins. When a recorded event arrives at a deadline, the clock is evaluated first. A daily recurrence resumes from the observed game day after a long outage.
* Effects can supply dialogue, move a resident, start a personal quest, award a deed or a lesson under ordinary practice rules, call bandits, schedule a festival, open a town project, change market conditions, or change one faction’s stance toward another. Specify both directions to change both factions’ attitudes. A player effect can target `$actor` when every entrance to the phase requires a participating player. World-wide effects such as a festival also affect players outside the journal audience.
* A `dm signal` exit names its exact direction in `match.detail`. An optional phase `guestVoices` list names the residents whose dialogue may receive a guest line. Add automatic alternatives to any scene that should keep moving when nobody is directing it.
* Each rehearsal gives a name, synthetic cohort, ordered events with unique IDs and game days since opening, and an expected phase sequence. Include wrong items, outsiders, repeat contributions and missed deadlines. Each rehearsal also records the effects in order with `$actor` resolved to the actual participant. Include `expectEffects` to assert that sequence and its recipients. The world’s queue receipts show how the game actually handled those effects.

## Approval, publication and operation

Approval requires passing scenarios covering every reachable phase. The digest covers the entire document, including audience, tests, guest speakers and effects. Approval saves that revision in its target database and returns a publication ticket valid for one day. Clicking **Publish approved story** or handing the ticket to the bridge starts that exact release. Reusing the ticket returns the existing run.

The runner observes checkpointed world events, persists its cursor and votes, and queues each entry’s journal message and effects in order. It waits for a receipt before sending the next effect. A refusal or expiry pauses the run and records the reason. The Dungeon Master's **Campaigns** tab (beside Story Creator; doc 85, Phase 1) lists every run and refreshes itself every 2 seconds: where each is, who is in it and who is in the world now, what happened last, what it is waiting for, and its alerts (paused, the game not answering, falling behind, quiet). One run shows its timeline (phase visits, triggers fired with the values they changed, orders with their receipts, and the admins' commands, merged), its triggers, counters and timers, the missions per wolf, its wolves on the map, its orders and an audit. Since Phase 2 a Dungeon Master steers a run from there: hold and resume (catching up on what happened meanwhile, or from now), hold all, stop, send a signal, fire a trigger (its conditions checked or not), turn a trigger off and on, go to any phase (with or without its entry effects), set a counter, fire, move, cancel or start a timer, complete or reset a mission for a wolf, retry or skip a stuck order, tell the story's wolves (the new `story.tell` order) and start an embedded scene. Each is a command in `dm.story_commands` that the runner takes first in its next tick, between two world events; each asks for a reason, is refused if the story has moved on to another visit or version, is done once if sent twice, and is written to `dm.audit` with its values before and after. On a hosted world only the owner, or an account the owner marked "may steer campaigns", steers. **Stop…** cancels future phases and queues journal closure; the run is *stopping* until the game acknowledges. Previously delivered effects, including faction changes, persist under their normal game rules. Once the underlying condition has been corrected, **Retry** (on a paused run, for a refused or an expired order) resubmits that one effect and preserves earlier progress; if the game applied it after all, the run simply resumes. An area reward skips a participant whose pack is full and tells them. A changed program requires a new approval.

Since Phase 3 ("Explain it") the console also says **why**. The Overview's *Waiting for* lists every trigger and exit of the phase, each with where it stands (on or off, used up and for whom, due when, sent by what), its conditions as a tree marked true, false or unknown with the values behind them, and the wolves of the story it could fire for now (those in the world first); *Why not?* shows the last ten events of its kind and where each stopped (a filter that did not match, with what it held and what was wanted; a wolf not in the story; turned off; already fired; a condition false, judged with today's values). *What if…* runs a made-up event or signal through the rules on a copy of the run (nothing is saved or sent), and every steering dialog's "This will do" is now a dry run: the command taken by the runner's own code on the run as it stands, in a transaction that is rolled back. *Next by itself* lists the timers with an estimate in real time from the world's speed. New alerts: **cannot move on** (nothing in the phase can still fire by itself: each trigger off, used up, waiting for wolves gone 14 days, or a signal nothing left can send), a **timer overdue** (5 minutes past due), the **audience nearly full** (90% of 500). The "needs you" alerts and the runner down also show as a count on the Campaigns tab, a line on LIVE, and (told once to each Dungeon Master in the world) in the game's Dev Console. Each world's owner may change the alert rules (*Alert rules…*), and a program may set its own `quietAfterHours` (0: slow on purpose). In Story Creator a **Problems** list shows every problem of the draft at once, each with **Go**: the program's errors, warnings the program alone can tell (a signal sent that nothing hears, one heard that nothing sends, a counter read that nothing sets, a timer cancelled that nothing starts), the triggers the last rehearsal did not reach, and for a Dungeon Master the world's references, including those approval does not check (markets, harvest areas, towns, ships, the tiles of `npc.move` and `project.post`, exits' filters and prerequisites, wolves and items in filters).

**A new version of a running campaign** (doc 85, Phase 5b). A changed program is still a new approval, but it can be approved *as a new version of a run* (Story Creator, under the approval: "or as a new version of a running campaign", for a live run of the same story id) and the run moved onto it from the Campaigns console (**⇪ Move onto a new version…**). The console shows what changed (triggers, variables, phases, missions, timers, regions, scenes) and what the run keeps, resets, sets back to its first value and drops, each with why: variables by name, scope and type (a version 1 run's counters as version 2 names them: a wolf's counter named in both scopes is `<name>.player`), timers whose signal is still heard, a mission's progress while its objectives are the same, a trigger's once-marks and on/off by trigger, a pending Wait only while what it still has to run is unchanged; a trigger the new version starts off is turned off. If the new version no longer has the run's phase, the admin names the phase it lands in. The move is a command like the others (a reason, a dry run, audited), taken between two world events in the same visit: no entry effect is sent again, nothing given is taken back, and the game is told the new words, regions and missions for the visit (`story.phase` with `revise`). If the game refuses them, the run goes back to the version it was on. The timeline says "moved to version … by …", the Audit tab lists the versions, and commands made against the old version are refused.

The packaged world starts and supervises the runner after migration. On an unpackaged server, run the following under the world’s DM service credentials alongside the game. Apply migrations through 0074, 0077 and 0078 (0078 for the trigger history's rule; 0070 for the event tile columns; 0071 for the command inbox, the runner's heartbeat and "may steer campaigns"; 0072 for each world's alert rules and the alerts the game tells its Dungeon Masters; 0073 for drafts; 0074 for a run's versions; 0077 for world variables). Use the matching runner, Studio, native library, game server and world-story client. The runner and native game exchange receipts through the world database. It uses no model or AI key.

```sh
python3 tools/story_runner.py --world WORLD_KEY
```

On the laptop’s Ridgemere 3, which `Saved/WorldAgent/run.sh` hosts, start the runner beside the world from the checkout root. The runner reads the DM credentials from `Database/.env` and finds `build-core/libratw_story.so`, which is built with `ratw_server`:

```sh
python3 tools/story_runner.py --world ridgemere3 >> Saved/WorldAgent/stories.log 2>&1 &
```

**How fast it follows the world** (doc 85, Phase 7). While a world story is live the game writes its events to the log every second between saves (they used to wait for the next save, up to 30 seconds), and each write wakes the runner (channel `ratw_events`); a console command wakes it too (`ratw_story`). The runner reads until it has caught up, a page of 1,000 events at a time, and sends what a turn's triggers asked for to the game together: one `story.effects` order holding them all (up to 61 effects and the journal's own orders), applied in order and answered order by order. The game looks for orders four times a second once told of one. A scene's outcome (a vote, a reply, a resident arriving or speaking for a story) no longer saves the whole world on the game thread: it asks for a save within half a second, written in the background with the outcome's events in the same transaction. On a copy of DEV's Ridgemere 2.0 a resident's next line now follows the last within about a second, where it took about a minute. Old trigger history goes by a rule (migration 0078): older than 90 days, except a run's newest 10,000.

**When the game refuses an effect.** Every world effect may say what the story does then, with `ifRefused`: `"pause"` (the default: the run pauses, and the effects after it in the same batch wait with it), `"skip"` (noted in the run's history, and the story carries on), or `"when here"` (for an effect to one wolf: held, and sent again the next time that wolf comes into the world, for up to `waitDays` real days, 7 when not said). A quest's reward is `"when here"` unless it says otherwise. In the Story Creator it is the "if refused" box after each world effect. In the console, a paused run's stuck order can be retried (with the orders after it), held for its wolf (**Retry when … is next here**), or skipped; the Orders tab lists the effects waiting for their wolf.

```json
{"do": "effect", "effect": {"kind": "deed.award", "player": "$actor", "detail": "Brought the rope.", "ifRefused": "when here", "waitDays": 14}}
```

A direction redeemed from a ticket is no longer refused while world events wait past the run's cursor: it goes into the runner's inbox and lands between two world events, as the console's commands do.

## AI authoring and direction

Run `python3 tools/story_author.py` as a JSON-lines subprocess. Send one JSON object per line. Each reply contains either `ok: true` and `result`, or `ok: false` and `error`. The offline bridge reads packaged examples and an optional context export selected by the operator.

| Request | Result |
| --- | --- |
| `{"op":"catalog"}` | Event and effect catalogue and complete examples |
| `{"op":"draft","blank":true}` | An empty, rehearsable program |
| `{"op":"components","eventType":"axle-at-the-ford"}` | Copyable event rules, journal objectives, participation and outcomes for one example |
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

Portable phase rules use the `triggers` array. Each rule has a unique `id`, an `on` event, and an ordered `actions` array. The optional `if` is a condition tree. Action names are `add`, `set`, `emit`, `schedule`, `cancel`, `effect` and `enter`. A signal listener uses `"on":"story signal"` with a separate `signal` name. The catalogue exposes the vocabulary under `triggerRules` and the components under each `eventTypes` entry. Rehearsal cases can assert `expectTriggers` (ordered fired rule IDs) and `expectCounters` (the final story and player values). The Reed Ford program contains working examples of both.

The optional top-level `triggers` array defines campaign triggers. A phase can set `campaignRules` to opt in or out explicitly. A `schedule` action accepts `days`, `signal`, optional `clock` (`game` or `real`), `owner` (`story` or `player`), `scope` (`visit` or `campaign`) and `repeat`. The frozen eligible cohort supports personal credits and percentage gates throughout a run. Releases support up to 100 phases and up to 64 active triggers per phase, counting shared rules.

Choose **Separate timer for each participant** for an individual deadline within a shared campaign. A second player can start the same named timer and keep a different deadline. Cancelling a personal timer affects the character who caused that event. A shared timer has one deadline for the whole campaign.

**Live trigger history** records the phase visit, actor, event or timer, fired rule and actions in the transaction that advances the cursor. Inspect counters, active deadlines and game receipts in **Campaigns**. The DM status API accepts `beforeVisit` and `beforeTrigger` cursors to page through earlier visits and trigger firings, `since` (`{visit, trigger}`) for only what is newer, and `have` (a digest) to leave out a program the console already holds; the `timeline` op merges visits, firings and orders, newest first, with `before` and `since` cursors. The `explain` op (a read) answers what a run waits for, and with `trigger` or `exit` why it has not fired, with `fact` what an event would do, with `command` what a console command would do; `problems` checks a draft; `alerts` is the summary for the tab bar and LIVE; `alert-rules` changes a world's rules. Player journals show the most recent 32 visits. Older visits remain available in the DM history.
