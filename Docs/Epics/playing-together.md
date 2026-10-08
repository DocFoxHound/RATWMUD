# Epic: Playing together

The central document for this epic: which plans it has, in what order, and where each one stands. Work through the
plans one at a time. When a plan's status changes, update its row here.

Started 2026-10-06.

## The goal

Make RATW a roleplaying game that people stay in. Research says three things drive enjoyment and continued play:
**autonomy** (my choices change the world), **competence** (I'm getting better) and **relatedness** (people know me).
In a roleplaying game, relatedness comes first. The epic makes players' choices visible in the world, lets
characters grow by practice instead of levels, helps wolves find and remember each other, and gives them goals that
need each other.

## Where things are written

- **The design and every decision:** [doc 48, Playing together](../Design/48-playing-together.md). It holds the
  reasons, the user's decisions (Decisions, three rounds on 2026-10-06), and the research with sources (appendix).
  Read it before starting any plan.
- **The plans:** docs 49 to 58 below, one per area. Each has its own phases, tests and "done when" checks.
- **This document:** the list, the order and the status. Nothing here overrides doc 48 or a plan.

## The plans

| Order | Doc | Plan | Carries out (doc 48) | Depends on | Status |
|---|---|---|---|---|---|
| 1 | [50](../Design/50-player-card-friends-safety.md) | The player card, friends and safety | 3.1–3.5 (profile, status, friends, known wolves, circles, account names), Part 11 (mute, block, report) | — | Built and pushed (5 of 5 phases, last 1a2f550); migrations applied |
| 2 | [51](../Design/51-scenes-and-stars.md) | Scenes and stars | 3.6 (star totals, bands, tags), Part 4 (scene openness, joining), 8.1 (end screens), the gathering howl; Story books and the bookshelf (Phase 7, the user's) | 50 | Built (7 of 7 phases; pushed, last 6071c37); migrations 0040 and 0041 applied |
| 3 | [52](../Design/52-newcomers.md) | Newcomers | Part 7 (start town, newcomer flag, mentors, ties, residents as matchmakers), vouching | 50, 51 | Built (5 of 5 phases; pushed 6071c37); migrations 0042 and 0043 applied |
| 4 | [53](../Design/53-hunting-and-working-together.md) | Hunting and working together | Part 6 (hunting, lend a paw, crafts for two, Gifted and Quickened roles), 5.3 (training grounds) | 50 | Built (7 of 7, pushed 6071c37; Phase 7 part B waits on doc 35's Craft panel) |
| 5 | [55](../Design/55-letters-gifts-favours.md) | Letters, gifts and favours | 3.8 (letters, residents' thanks, scent on crafted items), 3.9 (grooming, shared meals, lending) | 50 | Drafted (7 phases) |
| 6 | [54](../Design/54-gathering-places.md) | Gathering places | Part 5 (taverns, markets, notice boards, festivals, venues and renting), Part 10 (tavern games, library) | 50, 51 | Drafted (7 phases) |
| 7 | [56](../Design/56-fame-and-memory.md) | Fame and memory | 3.7 (deeds, nicknames), 8.4–8.6 (chronicle, unfinished business, welcome back), festival criers | 50; 54 for criers | Drafted (6 phases) |
| 8 | [57](../Design/57-changing-the-world.md) | Changing the world | Part 1 (residents' troubles, town projects, the economy's visibility) | 56, 53 | Drafted (6 phases) |
| 9 | [58](../Design/58-player-storytellers.md) | Player storytellers and personal stories | Part 9, 8.2, 8.3, 6.8 | 51; doc 34's story planner | Drafted (6 phases) |
| Alongside | [49](../Design/49-characters-and-earned-gifts.md) | Characters without levels, and earned Gifts | 2.1, 2.2 | Its own phases 1–3 before earned Gifts (phase 5) | Built (5 of 5 phases), not committed; migration 0034 not yet applied |

**Why this order:** the player card (50) is what everything else hangs on: status, friends, block and known wolves.
Stars and scenes (51) come next, because newcomers' Welcoming stars, Gift unlocks and storytellers all use them.
Newcomers (52) then has the tools it needs. Hunting (53) has fully agreed rules and stands mostly alone. Letters (55)
come before gathering places (54) because boards and festivals use letters and scent. Fame (56) feeds changing the
world (57). Storytellers (58) come last, because they build on doc 34's story planner.

**Doc 49 runs alongside, and started first** (the user's call, 2026-10-06). It changes character creation and removes
levels, and the user does its balance pass. Its earned Gift tiers (phase 5) need the account-wide social level from its
own phase 3, but not plan 51: they count today's Gold and Story Stars, and plan 51 later feeds the same counters.

### Statuses

- **Drafting:** the plan document is being written.
- **Drafted:** written; waiting for the user to read it and answer its open questions.
- **Approved:** the user has agreed it; ready to build.
- **Building (phase n of m):** a session is working through its phases.
- **Built:** every phase is done and tested; the plan's "Built" section says what was made.
- **Committed / Pushed:** in git.

## How to work through a plan

1. Read doc 48 (at least its Principles and Decisions) and the plan.
2. If the plan has open questions, settle them with the user first, and record the answers in the plan's Decisions.
3. Build phase by phase. Each phase has its own tests and "done when" checks. Run the perf gate (`world_check
   --players 20`) where a phase says so.
4. After each phase, add a "Built" note to the plan, and update the status here ("Building (2 of 5)").
5. If building changes a rule that doc 48 set, record it in doc 48's Decisions too, so the design stays true.
6. Commit only your own hunks. Other sessions often work in the same files, the economy code especially.

## Open questions, by plan

Each plan's Open questions section has the detail and a recommendation. Settle a plan's questions before building it.

- **49 Characters and earned Gifts:** all answered 2026-10-06. Nothing is migrated, since there are no real players
  yet; one wolf per account in the world; Quickened opens with the DM's hold standing in for reports.
- **50 The player card:** all answered 2026-10-07: Storyteller only for approved storytellers (doc 58); no
  birthplace or residence on profiles; private messages (not "tells") kept for offline friends until they log in: 50, 14 days.
- **51 Scenes and stars:** all answered 2026-10-07: the star rate is shown; talking a fight through pays a scene's
  ordinary pay; scenes on the minimap are off by default and a player may turn them on, with Knock scenes shown only
  when a friend is in them.
  Phase 7 (Story books and the bookshelf, added by the user): answered 2026-10-07: finishing by a majority of the
  recently active or unopposed after no activity; Private scenes may be linked, with a warning on finishing, and read
  by all once finished; finished books link into volumes (related, sequel, prequel); "World" is the DM's world
  storylines; books are the one kind of Story, and "official" is a book two thirds of its wolves agreed to (it pays as
  today's Stories do).
- **52 Newcomers:** all answered 2026-10-07: a first character may choose its start town (the busiest preselected);
  vouching carries to the resident's household at half the share.
- **53 Hunting and working together:** all answered 2026-10-07: ×1.8 is each wolf's rate (kept, balanced later); only
  the hunt's starter controls whether strangers may join; a landed blade kills a fleeing animal.
- **54 Gathering places:** must a full rest be in a bed the wolf has a right to? Should residents gamble with players?
  Festival prizes from entrants' pots, or funded by towns?
- **55 Letters, gifts and favours:** should Well-groomed last at least 2 game hours when given late in the day? Do
  close residents ever groom players? May players write to residents (a paid model call each)?
- **56 Fame and memory:** do criers call deeds by look when no one knows the name? Does a deed warm a resident to the
  doer once (+5 liking)?
- **57 Changing the world:** should the economy's own works appear as open projects? Should residents get ailments
  and tools that matter (so those troubles can exist)? A "deck" structure so bridges can be built? Which later lever
  first (introducing two residents recommended)?
- **58 Storytellers:** keep storyteller narration 30 days as DM evidence? May storytellers recruit strangers through
  board calls? Does a finished storyline pay a little XP, or recognition only? (Storyteller status: as plan 50.)
- **Across the epic:** a star rate on the card (plan 51).

## Things the plans found

While drafting, the plans checked doc 48 against the code. What changes doc 48 or another plan:

- **Accounts:** social level and stars are per character today, and nothing maps a character to its account. Plan 49
  builds `Accounts::ownerOf` (phase 1) and the account-wide social level (phase 3); plans 50–58 use those names.
  Accounts are capped at 128 (`AccountLimit`); plan 50 raises it.
- **Order:** plan 51's Welcoming tag needs plan 52's newcomer flag. Plan 52's phase 1 is small and can go before 51.
- **Names:** innkeepers introduce wolves by look, not by names they know (that would be hearsay; plan 52). Account
  handles are separate from sign-in names, never shown in local, party or Chapter OOC, and shown under a friend's
  label only if they share their character with you (plan 50).
- **Things doc 48 assumed that don't exist:** residents' debts, tools that wear, residents' injuries or sickness,
  water chores, raids on towns (plan 57); a wolf's scent strength and picked-up smells, a lasting effect from eating, a
  give command (plan 55); a quest or story engine (plan 58 builds a small step tracker and journal; doc 34's planner
  later writes into it); market wardens (plan 52 uses each town's market merchant).
- **Bugs found** (each fixed by the plan named):
  - the hunt square offers Join buttons the server refuses (53);
  - ambient gossip speaks a player's true name (56);
  - bonds fade while a player is away, against Principle 7 (56);
  - a private note past 300 is stored before the refusal (50);
  - `Society::shift` doesn't check the 64-kinds limit that loading enforces (55);
  - `estate.set` can't be sent from the DM app (54);
  - the social ledger is walked whole on every award, and every scene ever made is looped over for every client
    (49 and 51).
- **Model costs:** doc 28 has no prices set, so plans give model costs in tokens.
- **Shared files:** plans 53–57 need small hunks in the economy session's files (`RatwSociety.h`, `RatwResidents.cpp`,
  `RatwOrchestrate.cpp` and others). Each plan lists them. Agree them with that session before building.

## Log

- **2026-10-06:**
  - Research on engagement and roleplay.
  - Doc 48 drafted with the user's notes, and answered in three rounds.
  - Split into plans 49–58, which are being drafted.
  - This epic document created.
  - All ten plans drafted (docs 49–58), each read against the code.
  - Plan 49, phase 1 built: the practice engine (`Data/Progression/skills.json`, `Core/RatwPractice.*`,
    `World::practise`), growth lines, skills with caps on the Status window and in the DM app, the pace simulator.
    Not committed.
  - Plan 49, phase 2 built: fighting skill grows by fighting (`fight.blow`, `fight.end`); no fight reads a level;
    `level_sim` works at skill bands. Not committed.
  - Plan 49, phase 3 built: social level counts roleplay alone and is the account's; doc 44's XP awards became practice;
    the "+N experience" lines are gone. Not committed.
  - Plan 49, phase 4 built: strengths, weaknesses, a specialty and presets at creation (the Strengths tab); stamina as
    an attribute; every attribute grows by practice. Not committed.
  - Plan 49, phase 5 built: earned Gift tiers per account (locked cards with progress, DM grant/revoke/hold), one wolf
    per account in the world, the DM's account columns. **Plan 49 is built.** Not committed; migration 0034 waits to
    be applied to DEV and PROD.
- **2026-10-07:**
  - Plan 50's questions answered (Storyteller approved-only; no birthplace or residence; private messages kept for offline friends).
  - Plan 50, phase 1 built: handles, experience and played time on the account; the roleplay profile on the card (Look
    and OOC tabs, the YOUR PROFILE editor); status and walk-up; residents told what they perceive. Fixed a client bug
    where a second Look at the same wolf never opened. Not committed; migration 0035 waits with 0034.
  - Plan 50, phase 2 built: mute, block (account-wide, refusing scenes, invites and challenges) and report (with the
    lines received as evidence) in the page; the DM's Reports panel, with upheld silences the game applies. Not
    committed; migration 0036 waits with 0034–0035.
  - Plans 49 and 50 (phases 1–2) committed and pushed (ab6ef1b); migrations 0034–0036 applied to DEV and PROD.
  - "Tells" renamed private messages (a Gifted wolf's tells are something else); letters (doc 55) are mail, kept until
    deleted.
  - Plan 50, phase 3 built: friends by handle, sharing which wolf you play, handles under sharing friends' labels, and
    private messages (an away friend's kept: 50, 14 days). Not committed; migration 0037 not applied.
  - Phase 3 pushed (2493451); migration 0037 applied to DEV and PROD.
  - Plan 50, phase 4 built: Known wolves (scenes, parties, names; residents when noted or tagged), tags and notes, the
    unread and noted marks, and scene recaps written from only what each member perceived (the small model through
    the Mind's /recap, or plainly from the ledger). Notes, recaps and private messages are kept from the DM's login.
    Not committed; migration 0038 not applied.
  - Phase 4 pushed (fb3a01c); migration 0038 applied.
  - Plan 50, phase 5 built: circles (keeper, officers, invitations by handle, a roster sharing wolves only by choice,
    planned nights, a chat tab each, blocks held). Plan 50 is complete. Not committed; migration 0039 not applied.
  - Plan 50 phase 5 pushed (1a2f550); migration 0039 applied. Plan 51's questions answered; Phase 1 begun.
  - Plan 51, phase 1 built: the star book (stars by account, counting limits, bands for strangers, exact for oneself
    and friends who see), Quickened reading it, and fight roleplay paid as a scene. Not committed; migration 0040 not
    applied.
  - Plan 51 phase 1 pushed (9f556ed); migration 0040 applied.
  - Plan 51, phase 2 built: tags on stars (a chip after giving one), Known for at 50, tag shares in words for
    strangers, and the star rate in words. Not committed.
  - Plan 51, phase 3 built: Open / Knock / Private scenes (party scenes and rented places start Private), Join,
    knocking with LET IN / NOT NOW, nearby scenes by count, several scenes in one place, and the scene views reading
    indexes instead of every scene ever. Not committed.
  - Plan 51 phases 2–3 pushed (f4257c4). The user asked for wolves' names and portraits when hovering over A SCENE HERE
    (a scrolling list), and added Phase 7: Story books (scenes linked into player-driven Stories at any time, shared
    with friends, circles, Chapters or the World) on a bookshelf, newest first, with filters, an Unaffiliated tab,
    chapter timelines, and finishing with flavour text.
  - Scenes are now kept 8 days, and players are told 7 (the user). Phase 7 questions answered, except books against
    official Stories.
  - Books are the one kind of Story (the user). Plan 51, phase 4 built: each line says its scene (colour bar for one's
    own, a tag for an open one nearby), MY SCENE ONLY, and open scenes on the minimap behind a setting (Knock ones
    only with a friend in them). Not committed (the user: commit and push at the end).
  - Plan 51, phase 5 built: end screens (where, how long, who, moments in each viewer's names, a fight's figures) and,
    at the user's request, fights as part of their scenes: the scene keeps the fight as a moment, stays alive through
    it, and keeps its blow-by-blow log, readable from the card and part of recaps. Not committed.
  - Plan 51, phase 6 built: the gathering howl (a direction and a distance, never a name; a chorus carries further;
    residents turn and speak; the town remembers; howling together brings wolves closer). Not committed.
  - Plan 51, phase 7 built: Story books (scenes linked before, during or after; chapters outliving their scenes;
    summaries by hand or from recaps; sharing; finishing by majority or quiet days with the private-scene warning;
    volumes; official books driving the ledger's Story) on a bookshelf, newest first, with filters and Unaffiliated.
    Plan 51 is complete. Not committed; migration 0041 applied (DEV and PROD) on 2026-10-07.
  - 2026-10-07, at the user's request: scenes told as short stories (up to about 1,000 words) from the wolves' cards
    and the lines with their actions and speech marked; a book's later chapters carry on from the earlier ones without
    describing the same wolves again; stories on `gpt-5.6-luna` (`story_model`), which now costs about what nano does.
    Tried on the real model (doc 51, "Scenes told as stories"). Not committed.
  - 2026-10-07: plan 52's open questions answered (a first character may choose its start town; vouching carries to
    the household at half). Phase 1 built: the newcomer flag and the three start towns, with the creator's Arrival
    tab. Not committed.
  - 2026-10-07: plan 52, Phase 2 built: mentors (social level 5, no upheld report in 30 days; available or busy; the
    newcomer's ✦; turned off by an upheld report or a Dungeon Master). Not committed.
  - 2026-10-07: plan 52, Phase 3 built: ties (doc 48's eight starters; mentors first, each offered in turn, then a
    resident who fits; told where to find each other, a marker for 20 minutes; lapsing at 7 days or 3 scenes, the
    mentor resting a day). Migration 0042 written, not applied. Not committed.
  - 2026-10-07: plan 52, Phase 4 built: residents as matchmakers (innkeepers, priests and market merchants point a
    player at a wolf they know in town, for a reason the server chose, by the look the player knows; a written line
    without the Mind). Not committed.
  - 2026-10-07: plan 52, Phase 5 built: a newcomer's first evening at an inn (welcomed with the other newcomers there,
    the wolves pointed out as each listener knows them, Introduce prompts) and vouching (a share of trust, half to the
    household, the voucher's standing at risk). Plan 52 is complete. Migration 0043 written, not applied. Not committed.
  - 2026-10-07: migration 0043 applied. Plan 53's questions answered (×1.8 each wolf's rate, kept; only the hunt's
    starter controls joining; blades kill fleeing game). Phase 1 built: game freezes, watches, bolts directly away,
    calms; one bite or blade kills; the dodge by what it knew; lying in wait; the hunt sim. Not committed.
  - 2026-10-07: plan 53, Phase 2 built: open hunts (only the starter's setting counts), asks and invites, equal shares
    and Give my share, more game per hunter, companions that lie in wait, roles named. The 1.5× pair target isn't met
    in the sim; left for the balance pass the user asked for later. Not committed.
  - 2026-10-07: plan 53, Phase 3 built: the joint activity (`together::rate`, joints kept by World, leaving at 8 tiles
    from the others or 60 s idle, the bond), foraging together (each of a pair 1.79× a lone forager, a patch 5 then 6
    pickings), Lend a paw and Ask to lend a paw in the menu, work as a scene (talk while working pays), the work row.
    Not committed.
  - 2026-10-07: names always start with a capital (the user). Plan 53, Phase 4 built: Help with the harvest (autumn)
    and the threshing (winter) beside a resident farmer at its post; a spell's yield share and piece-rate pay from the
    farm's till (×1.4 alone, ×2.0 each for two); no work when the till can't pay or the barn is full; no hunk in the
    economy session's files. Not committed.
  - 2026-10-08: plan 53, Phases 5-7A built.
    - Phase 5: spars, bruises at worst; training grounds as data; trainers; the practice post.
    - Phase 6: Winnow and Dry lifts threshing; Lighten Load over a joint; Weathereye at the harvest; Throw Voice drives
      game; Keep watch; residents face their talkers; partners keep a Quickened wolf's secret; tell and vouch at a
      Warden.
    - Phase 7A: a paw at a resident's bench, its batches faster and thriftier (one declaration and two hunks in the
      economy session's `RatwSociety.h` / `RatwCrafting.cpp`).
    - Part B waits for doc 35's player Craft panel. Not committed.
