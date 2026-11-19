# Handoff: living conversations (doc 84), paused 2026-11-17

> **Cloud agents: read `AGENTS.md` at the repo root first** (what to write, where, which checks run without a database).
>
> **Where we left off (2026-11-19):** stages 1-4 of doc 84 are done and live (`ridgemere2` `67ad2c8f`); 378 of 773
> residents wear a mantle and 32 spares wait for newborns. Everything is committed and pushed. What's left for later is
> section 9 below: 395 Ridgemere city residents to write (all of them are now planned in 11 batches with briefs in
> `artifacts/talk/p14/city-work/`; `city-a3` is partly drafted), a third spare for each small pool, the stage kits and mark banks, the final
> listen-through and second blind ratings (P16), a few small checker items, a facts pass on resident data, and a desktop
> client release for the stage 4 client changes (ask the user before tagging). Sections 3 and 4 are history: both WIP
> branches were finished and merged.

This is where the work on NPC dialogue stopped, for the next agent to pick up. Read this first, then
`Docs/Design/84-living-conversations.md` (the design and every "as built" section, §23 onward). The mantle writing
itself has its own handbook: `Docs/Mantle-writing-handbook.md`.

## 1. The user's goals (2026-11-16, verbatim in doc 84 §1)

1. Every NPC a unique dialogue set ("mantle") in its region's dialect.
2. Extra sets per region for NPCs not yet born (spare pools).
3. Name-free lines (slots) so a set can be reused.
4. A dead wolf's set passes to a newborn in the same dialect region.
5. NPC-to-NPC talk that answers itself naturally, with no AI in the backend.
6. Robust: "some of the most believable NPC conversations in history, all preprogrammed".
7. NPCs never answer players' open chat; NPC-to-player talk only through the dialogue window (doc 83).

The user's decisions: the live AI answers a player's own typed words in the window only when it is plugged in,
otherwise the written answers do (Q1); mantles are world-database rows editable in Atlas and the DM tool (Q2); the
richer size, written by AI agents, no paid model calls (Q3); newborns inherit the mantle's honesty (Q4); births stay
realm-wide (Q5); ticking 100-hour listening runs are allowed at each stage's end, reported (Q6). Only two deities exist:
the Bright One (benevolent, God-like) and the Dark One (malevolent, Satan-like); never "gods".

## 2. What is done and live

Live means merged into `ridgemere2` and running on the laptop world (port 7788, `Saved/WorldAgent/run.sh`, database
`ratw_ridgemere3`), with central updated (`tools/deploy_central.sh`). `ridgemere2` head at the pause: `7f61190d`.

| Stage | What | Where |
|---|---|---|
| 1 | The instrument (talk_sim, scorecard, listen and rating tools, pinned 25-day fixture) and the truth fixes (no raw ids, victims and kin, housemates, claims) | merge `0ec6d092`; doc §23-§26 |
| 2 | The conversation engine, as a **hybrid**: the engine picks what/when/who/what's true/who has heard; whole written scenes carry the body; stitching only at the edges. Opinions, temperament, moods, the talk ledger, topics and agendas. The Bright One / Dark One sweep and its lint | merge `46b6ad88`; doc §27-§37 |
| 3 | Mantles in the world database (migration 0066 applied to `ratw_ridgemere3`), passing from the dead to newborns, name pools and slots, the writer's kit, **43 pilot mantles** (all of Saltwell, all living Fenshore residents, the 8 placed luna residents), the Mantles panel in Atlas and the DM tool, the mantle format frozen | merge `7f61190d`; doc §38-§41 |

Blind ratings (three blind raters, 20 conversations per source, 1-5): old system 3.97, hybrid **4.05**, shuffled
control 1.47 (doc §37). The pure line-by-line engine had rated 3.38 and 3.48, which is why the hybrid exists: keep it.

## 3. What is unfinished (work in progress, pushed, NOT built or tested)

Both were stopped mid-implementation at the user's request. Each is one WIP commit on its own branch, pushed to origin,
in its own worktree under `.claude/worktrees/`.

| Branch | Worktree | Commit | Task | Stopped at |
|---|---|---|---|---|
| `r3-talk-u1` | `.claude/worktrees/r3-talk-u1` | `a1c9b901` | Raise U1 (share of a mantle-wearer's talk from their own mantle) from 11.6% to 20-35%: the wearer's own mantle gets first claim whenever the wearer is either party (cast mantle scenes with the wearer in either role; own lines first for replies, joins and edges; prefer topics the mantle covers), without hurting coherence (C1, C3, X1) or repetition (R1, R4, R5) | the game side in `ambientBegin`; 13 files touched (`Core/RatwTalk*`, `RatwTopics*`, `RatwGameAmbient.cpp`, `RatwMantle.cpp`, `Tests/mantle_tests.cpp`, `tools/talk_card.py`, `tools/talk_score.py`) |
| `r3-talk-p12` | `.claude/worktrees/r3-talk-p12` | `62eef4dd` | Stage 4, Phase P12 (window-only plumbing, doc §13 and §18): `residentsAnswerChat` off; `overheard` (notice, emote, hint "Talk to X (T)", never a reply in words); Talk opens the window; `react.customer`, `welcomeAtInn`, `companionSays` converted; the companion tree's "What do you think?"; storyline event from the window only; legacy lines removed; client changes (§13.2). Gate: W3 = 0; dialogue_tests; game_tests | `companionSays` on an "alone" query, then writing `Data/Talk/rules/companion.talk`; new file `Core/RatwGameWindowOnly.cpp` |

To resume either: `cd` into its worktree, `git pull --ff-only`, read the diff (`git show HEAD`), finish, build
(`cmake --build build-core -j8`), run the tests and the card, then replace the WIP commit's claim with a real one.

## 4. What remains, in order

1. **Finish U1** (`r3-talk-u1`), measured on the pilot wearers (the P11 inns profile) with `talk_card`.
2. **Finish P12** (`r3-talk-p12`).
3. **P13, the window's talk and typed words** (doc §13, §18): `[does <move>]` choices and `answers` nodes; the hub
   rebuilt; Ask about...; `introduce`, `ask_name`; greetings by meeting and disposition; bye and thanks; day, hours,
   wanted, where-is; docs 52/54/55 nodes; grafts shown; the player claim store and knowledge choices; the ledger with
   players; `understand.json` and the 500-line corpus; unclear lines; the Mind answers typed words only when connected,
   authored understanding otherwise and on failure (Q1); line rotation; `#way.*#`/`#refuse.*#`. Gate: W2; the hub never
   shows more than 9 choices.
4. **Stage 4's end:** full suite; end to end in the client on a scratch server; one ticking 100-hour listen; one review
   and fix; merge U1 + P12 + P13 into `ridgemere2` and put it live (section 6). A client release (SLAPorSLOP) is needed
   for the client side of P12/P13; ask the user before publishing.
5. **Stage 5, the writing (offloaded):** P14, the remaining ~730 mantles, and P15, 88 spares, 16 stage kits and the
   mark banks. Follow `Docs/Mantle-writing-handbook.md`. None of stage 4 depends on these: the engine runs with whatever
   mantles exist, and residents without one speak from the shared library.
6. **P16, the final listen-through** (after most mantles exist): a ticking 100-hour listen, the second blind ratings
   (section 5), review and fix, the ambient Mind path deleted. Gate: every hard gate; H1 3.8+.

## 5. How the work was done (keep doing it this way)

- **One agent per phase**, each in its own git worktree off the previous phase's branch; targeted tests per phase; the
  full suite, a ticking listen, one review and fixes at each stage's end. Commit with explicit paths only (never
  `git add -A`, never stash); `git push --no-thin`.
- **The instrument:** `Tests/talk_sim.cpp` (frozen and `--fixtures` modes over P0's pinned 25-day fixture,
  `--tick` for listening runs), `tools/talk_card.py` (runs every profile, diffs against the baseline; must say "no gate
  broken" on hard gates; refreeze only with a written reason), `tools/talk_score.py`, `talk_listen.py`,
  `talk_rate.py`, `talk_bench`. Every phase report carries its scorecard diff.
- **Blind ratings:** `tools/talk_rate.py` builds a packet of 20 conversations each from the old baseline, the new
  build's ticking listen and a shuffled control, blinded and shuffled, with the key kept elsewhere. Three independent
  raters who see only the packet score each conversation 1-5 for believable and coherent; then unblind. A change goes
  live only if it doesn't rate below the old system.
- **Checks to keep green:** `talk_check --validate --render-trees --mantle --mantle-coverage`, `tools/dialect_check.py
  --world ridgemere3` (includes the deities and pronoun lints), `tools/talk_lint.py`, `tools/mantle_check.py`,
  `tools/mantle_export.py --check`, the full `ctest` in `build-core`, client `npm run typecheck`, `npm test`,
  `npm run build`.

## 6. Putting a stage live

From the main checkout (`/home/martinb/Documents/GitHub/MUDProject`, branch `ridgemere2`):

```
git fetch origin && git merge --ff-only origin/ridgemere2
git merge --no-ff origin/<stage branch>          # resolve, test
git push --no-thin origin ridgemere2
cmake --build build-core -j8                     # then spot-check game/talk/mantle/dialogue/checkpoint/save_compat tests
(cd Client && npm ci && npm run build); (cd Editor && npm ci && npm run build)
```

Restart the laptop world: stop `Saved/WorldAgent/run.sh` **and** its three children (`tools/world_agent.py run`,
`build-core/ratw_server --port 7788`, `tools/npc_mind.py`), then start `bash Saved/WorldAgent/run.sh` again in the
background and wait until `https://ratw.188-68-41-241.sslip.io/worlds` lists port 7788. Then `bash
tools/deploy_central.sh`. The launcher restarts the world by itself after a published build (exit 75), but not after
a crash or a maintenance close (exit 76).

## 7. Standing rules

- Never touch PROD (`ratw_prod`) or old DEV (`ratw_dev`). `ratw_ridgemere3` is DEV-kind: change it freely, but with
  committed, re-runnable tools, after dumping what you change into `Saved/Backups/`.
- No paid model calls (never run the NPC Mind against a live model for tests). Ask before any economy simulation; the
  only ticking runs allowed without asking are the planned 100-hour listens.
- Ask the user before publishing a client release or tagging.
- World text: wolves, never "dog/dogs" ("pups" for young); "private messages", never "tells"; male or female only;
  pronouns follow sex; deities only the Bright One and the Dark One.
- Known data problems the pilot writers found (doc §40/§41): workplaces that are districts, friend slots pointing at
  kin, fen backstories on Saltwell residents, the dialect word list flagging ordinary English. Fix in a data pass;
  ask the user if unsure.

## 8. Problems found while writing the handbook: FIXED (fix 8, 2026-11-18)

Found by the agent that wrote `Docs/Mantle-writing-handbook.md` (commit `e4a597b1`, with `tools/mantle_batch.py`).
**All nine are fixed** on branch `r3-talk-fix8` (doc 84 §42; the handbook is updated to match), so stage 5's writing
can go ahead: mark banks parse; window lines on fact nodes are gated in all 145 mantles, refused by G2 and made to stand
aside by the engine; G2 reads tagged lines; personas are pronoun-free; 183 misplaced `ask :` lines are folded into the
listener's; `mantle_load` exits 2 on a failed bind and takes `--written-by`; `mantle_check --pools` exists; the kits
test knows plain. The 145 are reloaded into `ratw_ridgemere3` and exported. What each problem was:

- **Mark banks can't be written yet:** the grammar parser refuses `mark.oath`, the name the engine looks for. P15's mark
  banks are blocked on this engine fix.
- **The pilot's window lines can hide real facts:** ungated news, gossip, business, shop and inn lines in mantles outrank
  the trees' own (84 lines). E.g. `sw101`'s "Nothing worth the telling" won over real news 20 times in 20. Gate those
  lines on the facts (or lower their priority) and fix the 84.
- **G2 (`mantle_check`) doesn't check tagged scene lines** (`b[...] #tag :`, `a@tag :`), though doc §40 says it does.
  `mantle_batch.py lint` covers it and found 52 hidden problems in the pilot (5-word runs, a Southmeadow phrase in
  `mb120`, a resident's name "Ash" used as a word). Fix the 52 and teach G2.
- **15 pilot personas use pronouns**, though a mantle can pass to either sex: make them sex-neutral or slotted.
- **`cw102`'s stories misuse `ask :`** (it is the teller's line, not the listener's).
- **`mantle_load` exits 0 when a binding fails:** read its output, or make it exit non-zero.
- **`mantle_check --pools` was never built.**
- **`written_by` is always `claude-<batch>`**, whoever writes the batch: make it a parameter.
- **The plain kits will fail `test_mantle_check.py`** until its kit table gets a plain-to-Ridgemere entry (the handbook
  allows that one-line edit).

## 9. Status update, 2026-11-18 (stages 1-4 live; writing trimmed for budget)

Done and live (`ridgemere2` `a7d5f3e7`, laptop world restarted, central deployed): the section 8 fixes (`d33aad57`),
U1 (a wearer's own mantle first; U1 16% to 20%, `1b134b80`), stage 4 (P12 + P13 + the stage end and its review,
`a7d5f3e7`: residents never answer open chat, only a glance and a "Talk to X (T)" hint; the window carries all
NPC-to-player talk; typed words go to the Mind only when it is connected), DEV's `live.dialects` refreshed. Sections 3
and 4 above are superseded: both WIP branches were finished and merged.

Mantles: **378 of 773 residents wear one** (43 pilot, 102 by Codex in cw-1/cw-2/lf-1, 159 town residents, 74 city
residents in the Commons), plus **32 spares** (plain 12, Coldwater 5, Larkfield 5, 2 each for the five small pools).
Batch reports: `artifacts/talk/p14/*/report.md`, `artifacts/talk/p15/spares-1/report.md`.

**Left for later (dropped at the user's request to save tokens):**

1. **395 Ridgemere city residents** still without a mantle (Heights, Sump, Office, Works, Estate, Wharf, and the rest of
   the Commons). Batch `city-a3` (36) was cancelled mid-writing: its drafts are uncommitted in
   `.claude/worktrees/r3-mantles-city-a/Saved/Mantles/city-a3/drafts/` (5 complete by their writers' gates, 5 partial,
   none G3-checked or loaded). Batches `city-a4`..`city-a6` (125 residents, ids rm221-rm345) are planned in
   `.claude/worktrees/r3-mantles-city-a/Saved/Mantles/`; pass `--dir` to that folder so ids don't collide. The rest of
   the city needs planning with `mantle_batch.py plan`.
2. **Spares:** a third spare for each small pool (Frosthollow, Oakhaven, Southmeadow, Saltwell, Mirebridge) to be green
   in year 1 and inside the honesty mix; Coldwater needs about one more. `mantle_check --pools --world ridgemere3`.
3. **The 16 stage kits and the mark banks** (P15); mark banks are writable since fix 8.
4. **The final listen-through (P16):** a ticking 100-hour listen, the second blind ratings, review, and deleting the
   ambient Mind path.
5. **Small known items:** G1 refuses `self.house` on graft choices (the fix belongs in one helper shared with P13's window
   choice parsing); older plain mantles rm105-rm110 have greetings that ask a question; 274 everyday-English 5-word runs
   shared across dialects in one writer's six city files (G2 doesn't compare across dialects); `rm117` and `cw197` each
   carry one G2 warning; one T2 line in the card is a stale snapshot in the scorer; R1 (first identical talk) is an
   open hard gate at about 3.7 h, below its target since Slice 2.
6. **Resident data the writers flagged** (doc §40/§41 and the batch reports): workplaces that are districts, friend
   slots pointing at kin, fen backstories on Saltwell residents, Watch guards with no post, many briefs sharing the same
   few quirks and backstories. A facts pass by the owner.
7. **A desktop client release** for P12/P13's client changes (ask the user before tagging).

Reserved dialogue-tree node names (doc 84 §45) must not be used for mantle graft nodes.
