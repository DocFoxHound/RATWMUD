# Questions for the next design session

These do not block the first implementation. Confirmed controls, exploration memory, cell boundaries and the one-hour NPC inactivity timer are not reopened here.

## Product decisions worth a playtest

1. **Visible detail after entry:** should entering a cell reveal its entire remembered outline, or only the portions the character can actually see? The safe initial choice is to keep hidden details hidden even after entry.
2. **NPC interpretation:** should permanent summaries record what the NPC believed, including a lie they were told, alongside the source and uncertainty? Initial design preserves source attribution and keeps verified world facts separate.
3. **Party ownership:** can several players recruit the same companion simultaneously, and who directs them? The slice uses one companion with one recruiting character.
4. **Long scenes:** how should characters continue walking or changing rooms while a long queue of prose is still revealing? The simulation continues immediately; the UI preserves received order and offers instant reveal.
5. **Social qualification:** what session duration and reciprocity feel right in actual play? Use the social handoff as the baseline and keep tuning in configuration. NPC conversation contributes no player Social XP.
6. **Wolf appearance:** the creator now uses five species, four age portraits, three statures, eight natural colors and solid/saddle/mantle/piebald markings. Which eye colors, scars, harnesses and bags should come next? Equipment layers are not yet rendered on dolls.
7. **First release platform:** Linux is tested on this machine; should the first distributed build prioritize Windows, Linux, or both?
8. **Production dialogue hosting:** RATW Game's OpenAI `gpt-5.6-luna` configuration is now verified for opt-in local testing. What operating budget, player consent and retention policy should apply before using it for real shared play? Ordinary launches remain offline; test setup and bounded live results are in `LIVE_NPC_TEST_REPORT.md`.

## Operational decisions before a public release

- A source-engine build is needed for the packaged dedicated-server target on this installation.
- Local-only password accounts and ownership are implemented; public encrypted authentication, recovery and moderation remain a separate release gate. Legacy development identities require explicit opt-in.
- How should administrative deletion and privacy requests affect permanently retained NPC summaries? Gameplay memory has no decay; operational retention needs an explicit policy before public use.

## Questions discovered while building and testing

9. **Action-only roleplay and Social XP:** your supplied social document excludes slash-action text from reward evidence. Should a substantial `/pose` scene count, even with no spoken dialogue? The implementation follows the supplied rule for now, but that may underserve nonverbal wolf roleplay.
10. **Scene formation and typing pace:** the reference uses an A–B–A exchange within 30 seconds to form a scene. That can be tight for substantial posts. Should explicitly opening a scene be the normal path, with a much longer automatic window?
11. **Separate conversations in one room:** should two nearby groups form independent scenes? The prototype tracks one active social scene per cell. Hearing privacy is enforced, but scene membership needs a better group model before serious progression.
12. **Companions after logout:** should a recruited wolf wait, return to their schedule, or travel with another party member? Currently the companion waits for its recruiting character; release and transfer need a policy.
13. **Terrain scale and visual direction:** how far should a terrain tile represent, and is vision omnidirectional for awareness or restricted by facing? The prototype uses abstract tile units and omnidirectional line of sight; facing is a deliberate orientation cue.
14. **World time resolved:** deterministic NPC routines now follow the shared four-hour-day calendar and persistent needs. Memory consolidation still uses one real hour after inactivity. Player absence and server-downtime policy remain questions 30–31.
15. **Promises versus memories:** which spoken commitments should become explicit quest/debt/relationship records? A remembered promise currently remains an attributed memory; it never silently changes inventory or completes a quest.
16. **Long-post contribution cap:** valid long posts remain intact, with at most 500 words counted toward session participation. Is that a comfortable ceiling, alongside the existing turn and reciprocity requirements?
17. **Sneaking and deliberate speech:** the movement update quiets pawsteps while preserving whisper/speak/yell volume. Should crouching also reduce deliberate speech, or should players select whisper themselves? Movement detection already uses sneak skill against hearing ability and ear health.
18. **Recognizing scent:** should familiar wolves be identifiable by scent without seeing them, and what earns that familiarity? The initial system reports only anonymous wolf scent and a broad direction, never a name, count or precise marker.
19. **Tracks and ventilation:** should the next scent milestone prioritize lingering trails after a wolf leaves, or scent carried through open connections between cells? The first implementation senses live bodies within the same cell, with wind and local airflow barriers. Neither trails nor cross-cell air transport is implied by the current cue.
20. **Next authoring priority:** after trying Atlas Workshop's paint → cut/merge/split → drill-down workflow, which would help most next: NPC/item placement, reusable terrain/room prefabs, or importing existing legacy `.cell` files? The present editor supports terrain, elevation, detached rooms, and reciprocal connections, but none of those three follow-ups.
21. **Larger and stacked regions:** should expansion first support several linked regional canvases, or aligned above/below world layers? The current tool has one rectangular world canvas up to 256×256 tiles plus detached rooms, with at most 256 combined cells/rooms. It already permits per-cell Z and stair links without requiring a global multilayer editor.
22. **Travel balance:** does a roughly ten-second full sprint, four-second exhaustion recovery band, and sustainably recovering middle trot feel right at the current tile scale? The requested pace currently resumes after recovery unless lowered; these numbers and that feedback need human playtesting, not just simulation tests.
23. **Party pace:** should a traveling party match its slowest member, offer a shared pace control, or let each wolf fall behind independently? Individual dexterity/stamina now work, but coordinated party speed and companion fatigue policy are not implemented.
24. **Destination precision:** is selecting a visited cell and stopping at its entry anchor sufficient initially, or should a later iteration support remembered landmarks/exact local destinations? The current Known Routes view deliberately contains cached visited geometry, not remote live detail.
25. **Day length resolved:** four real hours per game day, equal morning/evening halves, 365-day years. Night light now ranges from 8% at new moon to 40% under a clear full moon, before separate weather/character modifiers; test those readability values.
26. **Lighting next step:** whole-cell light/daylight access, warm/cool glow and lunar outdoor light are implemented. Should the next step be portable personal light, placed lamps with local pools/shadows, or dark adaptation?
27. **Weather evolution:** should the next weather milestone emphasize storms moving across neighboring cells, or persistent wetness/snow and scent trails? Seasonal cell-specific forecasts now evolve, but coherent regional fronts and accumulation do not.
28. **Darkness readability:** should a sealed unlit room retain the current 8% close-awareness sight floor, or become entirely visually black without a light source? Previously seen terrain can remain dim memory either way; hearing and scent remain independent. Also compare the new tavern screenshots for whether the perimeter glow feels too strong or too subtle.
29. **Lunar clock resolved:** lunar phases follow game days, matching the existing roughly 29.53-game-day sequence. Actual-date moon synchronization is not required.
30. **Aging while absent resolved:** characters age while logged out. The existing shared-calendar/checkpoint/login catch-up follows that rule. A game year takes about 60.83 real days of uptime; server downtime currently pauses the calendar and remains a separate decision.
31. **Natural lifespan resolved in principle:** players choose natural death before a mandatory deadline drawn once at age 100 plus 0–20 game years; natural death is not implemented yet. Current +1 strength/dexterity through age 34, then wisdom, and age-65 declines remain provisional balance. Sampling granularity/disclosure, inheritance, successors and NPC lifecycle integration still need design.
32. **Economic intervention:** should the next money source be regional caravan orders, treasury-funded services, or Chapter-sponsored work? The first source is up to eight paid outside meal orders per game day; starter grants draw from a finite treasury rather than minting money. Imports remove currency.
33. **Scarcity consequences:** when an NPC becomes hungry and broke, should relief be a public funded service, aid from relationships/Chapters, or a market opportunity for players? The current simulation exposes shortages and never silently refills purses; it does not implement starvation/death.
34. **Companion needs:** should recruited NPCs consume party supplies, buy their own provisions, or ask for help? Their normal jobs pause while recruited; party provisioning still needs a policy.
35. **Contested territory:** should overlapping faction claims mean disputed jurisdiction, layered rights, or both? Atlas currently preserves several claims without choosing a ruler; Chapter sites do not grant construction rights. Effective control, taxes and permission need separate rules.
36. **Real settlement capacity:** what should qualify as an occupied home or funded job before a Chapter can attract residents? Storykeeper currently accepts explicitly declared planning capacities, not measured buildings, wages or vacancies. Player construction and NPC/job authoring remain prerequisites.
37. **Migration consent and pace:** when should migration become autonomous, and what minimum improvement, family/loyalty ties, source-service protection and return rules should apply? The first adapter requires DM approval for finite named same-region residents, protects essential demo workers and uses a provisional 24-real-hour arrival cooldown.
38. **Political loss:** should resentment reflect headcount, profession scarcity, a percentage of source population, or a combination? Current bounded source-claim penalties apply only after verified arrival and can be reduced by a declared treaty modifier. No claim means no invented angry faction; scores do not yet drive native NPC behavior.
39. **Activity and scheduling:** how much observed attendance is enough to recommend a Chapter event window, and should a later system postpone for absent participants? Current samples begin only when observed; suggested UTC windows require confirmation, approved times stay fixed, and a missed dispatch window becomes blocked rather than silently running much later.
40. **Next executable story system:** should the first physical encounter milestone be road brigands, a caravan economy, or faction patrols? These need real actors, budgets, counterplay and completion rules. Assassinations, armies and faction collapse are currently campaign plans only.
41. **Staff authority and privacy:** before remote or multiple DMs, which roles, second-person approvals, retention rules and absent-player consent are required? The current Storykeeper is trusted-local and single-operator with omniscient metadata, private session files, no player login and no roleplay transcripts. It is not public administration infrastructure.

42. **Natural-death reminders:** begin at age 90, 100, or another age? Proposed: a gentle once-per-game-year prompt from 90, coalesced after absence, never an interrupting combat/roleplay modal. This is not yet enabled.
43. **Deadline while logged out:** finalize natural death on the game calendar, or offer a final scene on return? If there is a scene, specify retrospective narration versus bounded grace without an unlimited age-120 extension. Logged-out aging itself is settled.
44. **Combat pacing:** map effects and a collapsed/latest-action, expandable/full-perceived-log entry are confirmed. Next choose readable attack cadence, tactical inputs and real-time versus turn structure; compact presentation alone must not be mistaken for a pacing decision. Also define historical-log retention and PvP/death consent separately.
45. **Starting ages:** the creator provisionally permits Young 6–12, Adolescent 13–17, Adult 18–64 and Old 65–99. Keep all four available at creation, or restrict initial ages? Starting older does not award past birthday stat increases or Social XP. These fictional-world age bands need your approval.
46. **Appearance lifecycle:** should players be able to edit a created coat later, and should gear/scars use authored overlays? This slice saves appearance at creation, updates the age portrait as the character ages, and keeps sex descriptive rather than making different male/female paintings.
47. **Character slots and accounts:** six characters per local world account is provisional. Confirm the desired limit and deletion/retirement/recovery rules before opening accounts beyond local testing.
48. **Name policy:** should character names be unique across a world, per account, or allowed to repeat? This slice treats names as display labels and uses generated IDs for ownership; duplicate display names are currently allowed.

The most useful first morning exercise is a two-person scene in the tavern: compare pane proportions, long-post reveal speeds, token size, and conversational distance before choosing more content.

For the authoring exercise, run `python3 tools/map_editor.py serve`, paint a path
across a cut, merge and split it, drill into one cell, and connect a detached room.
Save JSON before experimenting. Export/playtest with a fresh manifest/save path
after geometry changes; the custom-save default hashes the manifest path, not
its content, and Atlas Workshop does not hot-reload or migrate old saves.

For a separate Storykeeper exercise, use a fresh playtest save and the private
launch instructions in `README.md`. Inspect the difference between faction
claims, Chapter sites and declared capacities; draft an announcement or weather
event, then approve it explicitly and compare queued versus applied state. Try a
migration preview before approving any move, and watch physical arrival rather
than treating acceptance as settlement. Do not share the private session URL or
use valuable saves for administrative experiments. This initial checklist is
for design review, not a production-readiness sign-off.
