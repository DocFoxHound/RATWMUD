# Questions for the next design session

These do not block the first implementation. Confirmed controls, exploration memory, cell boundaries and the one-hour NPC inactivity timer are not reopened here.

## Product decisions worth a playtest

1. **Visible detail after entry:** should entering a cell reveal its entire remembered outline, or only the portions the character can actually see? The safe initial choice is to keep hidden details hidden even after entry.
2. **NPC interpretation:** should permanent summaries record what the NPC believed, including a lie they were told, alongside the source and uncertainty? Initial design preserves source attribution and keeps verified world facts separate.
3. **Party ownership:** can several players recruit the same companion simultaneously, and who directs them? The slice uses one companion with one recruiting character.
4. **Long scenes:** how should characters continue walking or changing rooms while a long queue of prose is still revealing? The simulation continues immediately; the UI preserves received order and offers instant reveal.
5. **Social qualification:** what session duration and reciprocity feel right in actual play? Use the social handoff as the baseline and keep tuning in configuration. NPC conversation contributes no player Social XP.
6. **Wolf appearance:** which coat markings, eye colors, harnesses and bags belong in the first customization palette? The first sheet uses a replaceable static profile drawing.
7. **First release platform:** Linux is tested on this machine; should the first distributed build prioritize Windows, Linux, or both?
8. **Dialogue hosting:** which local or hosted text model, budget and privacy policy should the first live NPC provider use? The slice remains usable with deterministic offline dialogue until configured.

## Operational decisions before a public release

- A source-engine build is needed for the packaged dedicated-server target on this installation.
- Development identities are not public authentication. Real accounts, ownership recovery and moderation require their own release gate.
- How should administrative deletion and privacy requests affect permanently retained NPC summaries? Gameplay memory has no decay; operational retention needs an explicit policy before public use.

## Questions discovered while building and testing

9. **Action-only roleplay and Social XP:** your supplied social document excludes slash-action text from reward evidence. Should a substantial `/pose` scene count, even with no spoken dialogue? The implementation follows the supplied rule for now, but that may underserve nonverbal wolf roleplay.
10. **Scene formation and typing pace:** the reference uses an A–B–A exchange within 30 seconds to form a scene. That can be tight for substantial posts. Should explicitly opening a scene be the normal path, with a much longer automatic window?
11. **Separate conversations in one room:** should two nearby groups form independent scenes? The prototype tracks one active social scene per cell. Hearing privacy is enforced, but scene membership needs a better group model before serious progression.
12. **Companions after logout:** should a recruited wolf wait, return to their schedule, or travel with another party member? Currently the companion waits for its recruiting character; release and transfer need a policy.
13. **Terrain scale and visual direction:** how far should a terrain tile represent, and is vision omnidirectional for awareness or restricted by facing? The prototype uses abstract tile units and omnidirectional line of sight; facing is a deliberate orientation cue.
14. **World time:** should NPC daily routines follow accelerated game time, real time, or chapter/story time? The demonstration cycles compressed routines so movement can be observed quickly. Memory inactivity still uses real elapsed time.
15. **Promises versus memories:** which spoken commitments should become explicit quest/debt/relationship records? A remembered promise currently remains an attributed memory; it never silently changes inventory or completes a quest.
16. **Long-post contribution cap:** valid long posts remain intact, with at most 500 words counted toward session participation. Is that a comfortable ceiling, alongside the existing turn and reciprocity requirements?

The most useful first morning exercise is a two-person scene in the tavern: compare pane proportions, long-post reveal speeds, token size, and conversational distance before choosing more content.
