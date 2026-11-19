# Writing mantles: the handbook for the remaining NPC voices

**For:** an AI coding agent (any vendor) asked to write the remaining NPC mantles of *Run Against the World* (RATW)
in this repository, with no other context. **State when written:** 2026-11-17, branch `ridgemere2` at 7f61190d or
later; DEV world `ridgemere3` (database `ratw_ridgemere3`) at game day about 34.7. **What is done:** the format,
the engine, the tools, and a pilot of 43 mantles (Saltwell, Fenshore and the eight old "luna" residents). **What
is left:** 730 placed residents' mantles (Docs/Design/84 phase P14), then 88 spare mantles, 12 stage kits and the
mark banks (P15).

The design is `Docs/Design/84-living-conversations.md` (doc 84; §-numbers below are its sections unless another file
is named). The pilot's own short guide is `Docs/Design/84-mantle-writers-guide.md`. This handbook replaces neither:
it is everything you need, in one place and in the order you'll need it, with every command checked on 2026-11-17.
Where this handbook and the pilot disagree, **this handbook wins**: it was written after the pilot, and it lists
mistakes the pilot made (§4.6, §8) that you must not copy.

**Fix 8 (2026-11-18, branch `r3-talk-fix8`)** fixed the nine problems this handbook first listed in §8 (handoff §8):
mark banks can be written (§3.6); window lines on fact nodes are gated in all 145 mantles, refused by G2 and made to
stand aside by the game (§4.5.8); G2 reads tagged scene lines (§6.1); the personas have no pronouns; the misplaced
`ask :` lines are folded into the listener's (§4.5.6); `mantle_load` exits non-zero when a bind fails and takes
`--written-by` (§7.3); `mantle_check --pools` exists (§3.6); the kits test knows plain (§3.6). The text below is
updated to match; §8 says what each was.

**Read §1 to §9 once, fully, before you write anything.** Then, for each batch: §3.2 (pick it), §4 (write each
mantle), §6 (check), §7 (load, export, commit), §9 (report).

Contents

1. What a mantle is, what "done right" means, and the user's hard rules
2. Setup: environment, database, build, worktree, commits
3. The work list and the order
4. One resident, step by step (with a complete annotated example and a second, shorter one)
5. Writing quality
6. Checking: every gate, what each failure means, the self-review checklist
7. Loading and binding into DEV, exporting, committing, and what not to do
8. Known data problems, and how to handle each
9. The batch report template

Appendix A: command cheat sheet. Appendix B: the facts a line can test or say.

---

## 1. What a mantle is, what "done right" means, and the user's hard rules

### 1.1 A mantle, in one paragraph

A **mantle** is one wolf's way of talking (every character in RATW is a wolf), written **without their name**, in
the **dialect of where they grew up**. It holds their card (traits, quirk, likes, favourite topics, habits), their
own words (grammar symbols), opinions with reasons, short lines for the edges of a conversation (greetings, goodbyes,
reactions), **whole written scenes** (the body of most conversations they open), stories, barks, the lines they say
to a player in the dialogue window, and extra window branches ("grafts"). It is stored as rows in the world database
(`live.mantles`, one row a mantle; `live.mantle_lines`, one row an item), written and exported as `.mantle` text, and
**worn by one living wolf at a time**. When the wearer dies, the mantle rests a game year and then passes to a
newborn of the same dialect, who inherits its honesty and temperament (§9, §12). Every name, kin, trade and place
in it is a **slot** (`#self.spouse#`, `#self.workplace#`...) filled from whoever wears it, so it stays true when
reused (§11).

### 1.2 Why it exists: the user's ask, verbatim (§1, 2026-11-15)

> 1. Make sure NPCs all have unique dialogue trees that follow the dialect of their region.
> 2. Make sure we have an extra set of dialogues for each region to cover NPCs that are yet to be born.
> 3. Make sure the dialogues can interchange their own names so that we can reuse dialogue trees if we run out of
>    premade dialogue trees
> 4. Make it so that dialogue trees of old dead wolves can be reused for newborns in the same dialect region.
> 5. Expand dialogue trees into more discussions, and make sure they make sense and sound natural with other dialogue
>    trees. The goal, here, is to make an intelligent system of dialogues that can respond to other chatting NPC's
>    naturally without plugging an AI into the backend.
> 6. Make this dialogue system robust. I am aiming for having some of the most believable NPC conversations in history,
>    all preprogrammed and very intelligent.
> 7. For NPC-to-player interactions we should have some unique and believable dialogues as well, but they should all
>    be done through that specific social window interaction we designed earlier. That is to say, I do not expect NPCs
>    to be responding to players in general conversation

### 1.3 The user's hard rules (non-negotiable)

| # | Rule | The user's words | What it means for your writing | Enforced by |
|---|---|---|---|---|
| R1 | Every NPC a unique dialogue set, in its region's dialect | "unique dialogue trees that follow the dialect of their region" | One mantle per resident, in the dialect of where they **grew up** (the brief tells you: Fenshore speaks `mirebridge`; Ridgemere speaks `plain`). Nothing copied between mantles; no 5-word run shared within a dialect; no oath or saying shared within a town. | `mantle_check` (G2), `mantle_batch lint` |
| R2 | Name-free, so it can be reused | "can interchange their own names so that we can reuse dialogue trees" | Never write a name of anyone in the world. Every person, kin, trade and place comes through a slot (§4.4.9). A line whose slot is empty for a wearer simply isn't said by them. | G2 (names list of all 1,083 NPCs, living or dead) |
| R3 | Passed from the dead to newborns of the same dialect region | "dialogue trees of old dead wolves can be reused for newborns in the same dialect region" | Write nothing that is true only of this one wolf unless a fact gates it (`self.married=yes`, `self.job=smith`, `self.age>=40`). `fits` is **stage only** (never sex, role or job: a newborn has none and would never qualify). The persona has **no pronoun**. | G2 (persona, truth), lint (fits) |
| R4 | NPCs never answer players' open chat | "I do not expect NPCs to be responding to players in general conversation" | Everything a resident says *to* a player is a `dlg` line or a `graft` (the dialogue window). Scenes, lines and stories are NPC-to-NPC. Barks are said to no one. Never write a line that answers what a player typed in chat. | the engine (residents never answer chat, §13) |
| R5 | Deities | (the user, 2026-11-17) the world has two deities: **the Bright One** (benevolent, God-like) and **the Dark One** (malevolent, Satan-like) | Never "god", "gods", "Gods!", "godsend", "godly", "godforsaken", "goddess". Blessings, prayers, oaths of good: the Bright One. Curses, evil, temptation: the Dark One. Capitalised exactly so. | G2 (`gods?`, `goddess`), lint (the whole family) |
| R6 | Never "dog" | "I would not use the word Dogs, it'll confuse things." (2026-10-14) | They are wolves; the young are **pups**. No "dog", "dogs", "puppy", "doggy". Reword idioms: dog-tired → bone-tired; wet dog → wet fur; going to the dogs → going to ruin; lucky dog → lucky so-and-so; dogsbody → drudge. Use cats, goats, hens, geese where a pet or stray is meant. | G2 |
| R7 | "private messages", never "tells" | (the user's rule for player messaging) | If a line ever mentions players' private messages, call them that. "tells" as a verb ("she tells me") is fine. | G2 (`a/your/my/private tells`) |
| R8 | Male or female only; pronouns follow sex | (the user, 2026-10-07) "created characters are either male or female; no other options" | **Never "they/them/their" for one known wolf.** A mantle doesn't know a slot person's sex, so in a sentence that names someone by a slot use **no** pronoun: repeat the slot, say "that one", or restructure. For the topic's person use `#topic.they#`, `#topic.them#`, `#topic.their#` (sexed by the engine); for the listener `#other.they#`. Never "he/she" about the listener. | G2 (he/she beside a slot, persona), lint (they_named) |
| R9 | The richer size | Q3: "richer" (about 0.7 million words), "written by Claude agents in sessions, no paid model calls" | Full tier ≥ 92 items, standard ≥ 55, spare ≥ 64 (items that aren't symbols); the pilot's real counts are in §3.4. **No paid model calls**: never run `tools/voice_tables.py`, `tools/live_npc_smoke.py` or anything that calls the NPC Mind or an AI API to write or judge lines. You write every line yourself. | G2 `--strict` (size) |
| R10 | Most believable conversations, all preprogrammed | goal 6 | Whole scenes that read as one natural talk in every thread; true to the moment, the wearer and the town; no stock filler (§5). | blind ratings (§34-§37), your self-review |

### 1.4 What "done right" means

A mantle is done right when **all** of these hold:

1. It passes every gate with **0 refusals**: G1 (`talk_check --mantle`), G2 (`mantle_check --strict`), the lint
   (`mantle_batch.py lint`), G3 (`--mantle-coverage`: 0 holes in the core moves), and after loading, G6 (the export
   round trip). §6.
2. It **reads as one wolf**: a neighbour would know them by their oath, their way of answering, the things they come
   back to. It sounds like nobody else in the town (§5.10).
3. Its **scenes are whole talks**: every thread of every scene reads as one natural exchange, 3-5 turns, no
   non-sequiturs, no cryptic replies, no question left hanging at the end (§5.4).
4. It is **true**: true to the moment (weather, hour, season, place gated), to the wearer (job, family, age, wealth
   gated), and to the world (no invented facts about real residents, prices, crews or culprits) (§5.7).
5. It is **reusable**: every claim about the wearer is gated by a fact, so a later wearer of another trade, family or
   age never says something false (§5.8).
6. It is **in dialect, by symbols, not caricature**, at the density the gates ask (60% of the wearer's own
   alternatives carry a dialect marker, for every dialect but plain) (§5.6).
7. Its **window lines never hide what the window must report** (news, gossip, business, a bare shop, a warrant) (§4.5.8).
8. It is **sized to its tier** (§3.4), loaded into DEV, bound to its resident, exported, committed and pushed (§7),
   and reported (§9).

---

## 2. Setup: environment, database, build, worktree, commits

### 2.1 The machine and the repository

| What | Where / value |
|---|---|
| Repository (the "main checkout") | `/home/martinb/Documents/GitHub/MUDProject` |
| Branch everything lands on | `ridgemere2` (push to `origin ridgemere2`) |
| OS / shell | Linux (Fedora), bash; Python 3; PostgreSQL 18 on this machine |
| The game's checker binary | `build-core/talk_check` in the main checkout (C++, built with CMake) |
| Your scratch space (git-ignored) | `Saved/Mantles/` (briefs, plans, facts dumps); `Saved/Backups/` (database backups) |
| Committed outputs | `Data/Talk/mantles-export/<dialect>/<id>.mantle`, `Data/Talk/mantles-export/index.tsv`, your batch report under `artifacts/talk/p14/<batch>/` |
| Other sessions | Several agents work in this repository at once. Never touch files you didn't create; never stage with `git add -A` or `git add .`. |

### 2.2 The database: DEV is `ridgemere3`, nothing else

```
$ python3 tools/world_db.py list
KEY         NAME                                   KIND  UPSTREAM  DATABASE
prod        Ridgemere (live)                       live            ratw_prod
dev         Ridgemere 2.0 (dev) (also ridgemere2)  dev   prod      ratw_ridgemere2
dev-old     Ridgemere (old map, archive)           dev             ratw_dev
ridgemere3  Ridgemere 3.0 (dev)                    dev             ratw_ridgemere3  [atlas] [live] [dm]
```

- **Always pass the key `ridgemere3`** (database `ratw_ridgemere3`). It is DEV: Atlas's world, and also the world the
  local "live" server on port 7788 runs. Mantles are written only here.
- **Never** use `prod` (`ratw_prod`, the live world), `dev` (that key is **`ratw_ridgemere2`**, an older world:
  the name is a trap) or `dev-old` (`ratw_dev`). `tools/mantle_load.py` refuses a live world; `tools/mantle_batch.py`
  refuses any non-dev world and warns on any key but `ridgemere3`.
- Connections use `Database/.env` (git-ignored, in the main checkout). If a tool says it can't connect, check
  `python3 tools/world_db.py list` works; if PostgreSQL is down, `python3 tools/world_db.py up` starts it (it keeps
  data). Never run `world_db.py migrate`, `use`, `serve-world`, `new`, `down`, or `psql` with writes. **Never write SQL
  by hand.** The only tool that writes mantle rows is `tools/mantle_load.py` (§7).
- The read-only tools (`mantle_brief`, `mantle_index`, `mantle_check --world`, `mantle_batch`, `mantle_export
  --check`, `mantle_pool forecast`, `talk_check --world`) open read-only transactions or scratch copies and write
  nothing.

### 2.3 The build: `talk_check`

Every check runs the game's own loader, `build-core/talk_check` (built 2026-11-17 10:36, after 7f61190d). Rebuild it
(in the main checkout) only if C++ under `Core/` or `Tests/` changed since:

```
cd /home/martinb/Documents/GitHub/MUDProject
ls -l --time-style=+%F_%T build-core/talk_check          # when it was built
git log -1 --format='%ci %h' -- Core Tests CMakeLists.txt  # when the C++ last changed
cmake --build build-core --target talk_check -j8          # rebuild (only if the C++ is newer)
# if build-core doesn't exist at all:
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release && cmake --build build-core --target talk_check -j8
```

You never edit C++; you only rebuild if someone else changed it. A full build of everything is not needed.

### 2.4 Your worktree and branch

Work in a git worktree on a branch of your own, one per batch (or one for a run of sequential batches), so your
drafts never sit in the shared main checkout:

```
cd /home/martinb/Documents/GitHub/MUDProject
git fetch origin
B=cw-1                                         # the batch's name (§3.2)
git worktree add .claude/worktrees/mantles-$B -b r3-mantles-$B origin/ridgemere2
cd .claude/worktrees/mantles-$B
cp /home/martinb/Documents/GitHub/MUDProject/Database/.env Database/.env     # git-ignored; the tools need it
export RATW_TALK_CHECK=/home/martinb/Documents/GitHub/MUDProject/build-core/talk_check
mkdir -p Saved/Mantles
```

- Run every `python3 tools/...` command **from the worktree root**: the tools read the worktree's own
  `Data/Talk/` (your drafts) and `tools/`.
- `RATW_TALK_CHECK` lets `tools/mantle_store.py` (used by `mantle_load`) find the binary (a worktree has no
  `build*/`). `tools/mantle_batch.py` finds it by itself and always passes `--data <worktree>/Data/Talk`.
- If you call `talk_check` by hand from a worktree, **always** pass `--data "$PWD/Data/Talk"`: without it the binary
  reads the main checkout's `Data/Talk`, not yours.
- `ctest` runs in the main checkout only (it checks the main checkout's files): run it there after merging (§7.5).
- When the batch is merged and pushed, remove the worktree: `git worktree remove .claude/worktrees/mantles-$B` (delete
  `Database/.env` in it first; it is a copy of a secret).

### 2.5 Where to run what

| Command | Run from | Writes |
|---|---|---|
| `tools/mantle_batch.py facts/todo/plan/lint/gates/pools` | worktree | `Saved/Mantles/` only (and a temp dir) |
| `tools/mantle_brief.py`, `tools/mantle_index.py` | worktree | nothing |
| `$RATW_TALK_CHECK --data "$PWD/Data/Talk" --mantle ...` | worktree | nothing |
| `tools/mantle_check.py ... --world ridgemere3 --strict` | worktree | nothing |
| `tools/mantle_load.py ridgemere3 ... --batch NAME --bind` | worktree, with `RATW_TALK_CHECK` set | DEV rows, a backup in `Saved/Backups/`, your files (canonical write-back) |
| `tools/mantle_export.py ridgemere3 [--check]` | worktree | your `Data/Talk/mantles-export/` |
| `ctest --test-dir build-core -R 'mantle_|dialect_check|talk_lint'` | main checkout, after merge | nothing |
| `git merge`, `git push --no-thin` | main checkout | the branch |

### 2.6 Commits, merges and pushes

- **Stage explicit paths only**: `git add Data/Talk/mantles-export/coldwater/cw103.mantle ...` (or the batch's
  dialect folders plus `index.tsv`), and your report folder. Check `git status --short` and `git diff --cached --stat`
  before every commit: only your batch's files may be staged.
- **Commit in the worktree** after the batch is loaded and exported (§7.5), one commit per batch:

```
Doc 84 P14 batch <name>: <N> mantles for <town or quarter> (<dialects>), loaded into DEV and bound

<2-6 lines: who (town, quarter, tiers), how many items/alternatives/words, gates (G1, G2 strict, lint, G3: 0 holes),
the round trip (mantle_export --check: agree), anything left open (see the report).>

Co-Authored-By: <your model's name and contact, as your own tool requires>
<your session or run link, if your tool adds one>
```

  Leave those last lines to your own tool's attribution conventions; don't copy another agent's.
- **Merge and push from the main checkout**, then stop if anything is unusual:

```
cd /home/martinb/Documents/GitHub/MUDProject
git status --short | grep -v '^??'          # tracked changes by others? if any touch Data/Talk/mantles-export, STOP and report
git fetch origin
git merge --ff-only origin/ridgemere2        # only if the local branch is behind; if this fails, STOP and report
git merge --no-ff r3-mantles-$B -m "Merge r3-mantles-$B: doc 84 P14 batch $B, <N> mantles for <where>"
git push --no-thin origin ridgemere2         # --no-thin: this repo's GitHub push fails without it
```

- If the merge conflicts **only** in `Data/Talk/mantles-export/index.tsv` (another batch landed meanwhile), take
  either side, then rewrite it from the rows (the rows are the truth): `python3 tools/mantle_export.py ridgemere3`
  in the main checkout, `git add Data/Talk/mantles-export/index.tsv`, and finish the merge. Any other conflict:
  `git merge --abort` and report.
- Never force-push, never rebase `ridgemere2`, never commit `Saved/`, `Database/.env`, `artifacts/economy/` or
  anything you didn't write.

---

## 3. The work list and the order

### 3.1 What is left

`tools/mantle_batch.py todo` lists every placed, living resident who wears no mantle and has none written (in DEV's
rows, any state, or in a file under `Data/Talk/mantles-export/` or a `--dir`), **most heard first** (the order of
`artifacts/talk/most-heard-p0.tsv`, from Phase 0's listening runs; residents who weren't ranked, such as pups born in
the game, come last):

```
python3 tools/mantle_batch.py facts --out Saved/Mantles/residents.jsonl      # once a session: every resident's facts (15 s)
python3 tools/mantle_batch.py todo --facts Saved/Mantles/residents.jsonl                       # everyone left
python3 tools/mantle_batch.py todo --facts Saved/Mantles/residents.jsonl --town coldwater      # one town
python3 tools/mantle_batch.py todo --facts Saved/Mantles/residents.jsonl --town ridgemere --quarter sump --limit 40
```

The snapshot on 2026-11-17 (DEV day 34.7): **730** residents, **205 full tier, 525 standard**, 97 of them pups or
adolescents:

| Town (where they live) | Left | By dialect (where they grew up) |
|---|---|---|
| Coldwater | 66 | coldwater 59, plain 6, southmeadow 1 (one is a pup born in the game, `born_1grqt7uo`) |
| Larkfield | 36 | larkfield 35, southmeadow 1 |
| Southmeadow | 19 | southmeadow 19 |
| Frosthollow | 9 | frosthollow 7, coldwater 1, larkfield 1 |
| Oakhaven | 8 | oakhaven 8 |
| Ridgemere, the Commons | 207 | plain 161, coldwater 20, larkfield 11, frosthollow 4, mirebridge 4, southmeadow 4, saltwell 2, oakhaven 1 |
| Ridgemere, the Sump | 178 | plain 144, coldwater 15, larkfield 7, saltwell 5, mirebridge 4, southmeadow 2, frosthollow 1 |
| Ridgemere, the Estate | 51 | plain 41, coldwater 4, southmeadow 2, frosthollow 1, larkfield 1, mirebridge 1, saltwell 1 |
| Ridgemere, the Heights | 42 | plain 37, coldwater 2, larkfield 2, southmeadow 1 |
| Ridgemere, the Office | 32 | plain 27, coldwater 4, mirebridge 1 |
| Ridgemere, the Works | 32 | plain 24, coldwater 4, larkfield 1, mirebridge 1, saltwell 1, southmeadow 1 |
| Ridgemere, the Wharf | 12 | plain 8, coldwater 1, frosthollow 1, larkfield 1, oakhaven 1 |
| Ridgemere, no quarter | 38 | plain 28, coldwater 2, larkfield 2, oakhaven 2, saltwell 2, frosthollow 1, mirebridge 1 |

Residents keep being born and dying: always run `todo` fresh; never trust this table for ids or names.

### 3.2 The order, and how to pick a batch

The order is doc 84 §14.3's, with batches of 30-40 (the pilot's measured pace makes a batch about 2-3 writer-hours;
smaller batches keep the 5-word-run check and your review manageable):

| # | Batch name(s) | Who | Size |
|---|---|---|---|
| 1 | `cw-1`, `cw-2` | Coldwater | 33 + 33 |
| 2 | `lf-1` | Larkfield | 36 |
| 3 | `sfo-1` | Southmeadow, Frosthollow, Oakhaven (`--town southmeadow,frosthollow,oakhaven`) | 36 |
| 4 | `rm-commons-1` … `rm-commons-6` | Ridgemere, the Commons | 6 × ~35 |
| 5 | `rm-sump-1` … `rm-sump-5` | the Sump | 5 × ~36 |
| 6 | `rm-estate-1`, `rm-estate-2`, `rm-heights-1` | the Estate, the Heights | 26 + 25 + 42 |
| 7 | `rm-works-1`, `rm-office-1` | the Works, the Office | 32 + 32 |
| 8 | `rm-wharf-none-1` (`--quarter wharf,none`) | the Wharf and those with no quarter | 50 |
| 9 | P15 (§3.6) | spares, kits | |

Within a batch the order is most-heard first (what `plan` does). Doc 84 §14.2 also says "public roles first": the
most-heard list puts most of them first anyway.

**Plan the batch** (read-only; writes only under `Saved/Mantles/<batch>/`):

```
python3 tools/mantle_batch.py plan --name cw-1 --town coldwater --size 33 \
    --facts Saved/Mantles/residents.jsonl --out Saved/Mantles/cw-1 --dir Saved/Mantles
```

It writes:
- `Saved/Mantles/cw-1/batch.tsv`: one row a mantle: the new id (the next free number of its dialect, from 101 up;
  the test fixture owns the low numbers and 900+ is kept for spares), the resident, name, dialect, tier, accents,
  stage, town, quarter, most-heard rank, work and the export path;
- `briefs/<id>.md`: the resident's brief (`tools/mantle_brief.py`'s), with the id, path, tier sizes, and for plain
  speakers the plain grammar's whole symbol list and their quarter's register;
- `start/<id>.mantle`: a starting file with the header filled from the facts (dialect, accents, tier, `written_for`,
  `fits`, traits with honesty kept, quirk, likes, dislikes) and `TODO` where you decide; the lint refuses any `TODO`
  left;
- `index-<town>.md`: what the town's mantles already took (`tools/mantle_index.py`).

`--dir Saved/Mantles` makes `plan` skip residents and ids that another planned batch already holds, so two plans
never collide. Copy each starting file to its export path as you begin it:
`cp Saved/Mantles/cw-1/start/cw103.mantle Data/Talk/mantles-export/coldwater/`.

**Writers in parallel.** The pilot ran ten writer agents side by side, 4-5 mantles each, all in the same worktree,
each checking its own files with G1 and G2 over the whole dialect folder (so collisions with neighbours written at
the same time show), plus one lead who wrote the first mantle to try the guide, then read, checked and fixed the
whole (§40). If you can run sub-agents, do the same: give each a disjoint set of rows from `batch.tsv` (same town),
this handbook, and the instruction to run `python3 tools/mantle_batch.py gates <their files>` until it passes. If you
can't, write them one after another; it is the same work. Either way, **you** run the batch's gates and the load.
Measured pace (pilot): about 980 items an hour per writer, about 55,000 tokens a mantle.

### 3.3 Tiers: who gets which

`mantle_brief` and `mantle_batch` decide it from the facts (§9.3): **full** for merchants (`role=merchant`),
innkeepers, priests, guards, masters (`master=yes`) and House family (`house` set); **standard** for everyone else,
pups included. Don't change a tier (the lint warns).

### 3.4 How much, per tier (the richer size)

G2 `--strict` refuses a mantle under its tier's **items** (anything that isn't a symbol: lines, opinions, scenes,
stories, rules, window lines, grafts): **full 92, standard 55, spare 64**. The pilot wrote more, and its counts are
the bar the lint warns under:

| Part | Full (pilot median; lint floor) | Standard (pilot median; lint floor) |
|---|---|---|
| Words (symbols in `words`) | 18 (16) | 14 (12) |
| Whole scenes (2-3 threads each) | 31 (27) | 19 (17) |
| Stories (3-4 beats, with the listener's answer) | 5 (5) | 3 (3) |
| Opinions with reasons | 8-9 (8) | 6 (5) |
| Lines (edge moves) | 33 (30) | 23 (18) |
| Barks (`rule` blocks, `concept=bark`) | 6 (6) | 4 (4) |
| Window lines (`dlg`) | 15 (12) | 6 (4) |
| Grafts (2 nodes each) | 2 (2) | 1 (1) |
| **Items (not symbols)** | **102 (G2: 92)** | **62 (G2: 55)** |

Spares (P15): the standard counts plus `young`, `adolescent` and `grown` sections (G2: 64 items).

### 3.5 Pups and adolescents (97 of the 730)

A pup (`stage=young`, about 0-12) or adolescent (13-17) wearer needs `fits stage=young|adolescent|adult|old` and
`young`, `adolescent` **and** `grown` sections: the mantle grows up with them and later passes to another newborn
(the lint refuses a pup's mantle without `young` and `adolescent`). Put most items in the stage they are now, a
solid `grown` section (at least the edge lines and a dozen scenes) for later, and read the pilot's pup,
`Data/Talk/mantles-export/plain/rm102.mantle`, and the kits in `Data/Talk/kits/` for how pups talk.

### 3.6 After the residents: P15 (spares, kits, mark banks)

**Spares: 88** mantles written for nobody, for the unborn of each dialect (§12.1):

| Pool | plain | coldwater | larkfield | southmeadow | saltwell | mirebridge | frosthollow | oakhaven |
|---|---|---|---|---|---|---|---|---|
| Spares | 20 | 26 | 10 | 8 | 6 | 6 | 6 | 6 |

A spare: `tier spare`, ids from 901 (`cw901`...), no `written_for`, `fits stage=young|adolescent|adult|old` (stage
only), `young`, `adolescent` and `grown` sections, a sex-neutral persona and lines (no `^m`/`^f`, no "my
husband"/"my wife" but through `#self.spouse_word#` gated on `self.married=yes`), and honesty written to the pool's
mix: each pool's share of `sly` + `guarded` within 5 points of its dialect's living residents (§9.5). Check with
`python3 tools/mantle_batch.py pools Data/Talk/mantles-export --facts Saved/Mantles/residents.jsonl` while drafting,
and gate with `python3 tools/mantle_check.py --pools --world ridgemere3 Data/Talk/mantles-export/<dialect>/<id>.mantle ...`
(your drafts counted beside the rows): each pool's spare + ready mantles against the births forecast (`mantle_pool
forecast`'s) for five years, year by year (the births drawn, the wearers' deaths coming back once rested), green when a
year starts with 1.5 x its half-year's births + 2 (the game's own line), and the spares' sly + guarded within 5 points
of the dialect's living. It exits 1 while any pool isn't green: today every pool is (no spares exist yet), which is
what P15 is for. Spares are loaded with `mantle_load` without `--bind` (nobody to bind).

**Stage kits: 12** left of 16 (`saltwell` and `mirebridge` young and adolescent exist): `plain`, `coldwater`,
`larkfield`, `southmeadow`, `frosthollow`, `oakhaven`, each `young` and `adolescent`. A kit is a shared **`.talk`**
file, `Data/Talk/kits/<dialect>.<stage>.talk` (not doc 84 §7.10's `.kit`), about 40 entries (whole scenes and edge
rules) whose every entry is gated `self.from=<the dialect's towns> self.stage=<stage>` (read
`Data/Talk/kits/saltwell.young.talk`). Checks: `$RATW_TALK_CHECK --data "$PWD/Data/Talk" --validate <each file>` (name the files; a
directory validates nothing), `python3 tools/talk_migrate.py --check`, `python3 tools/talk_lint.py` (0 new),
`python3 tools/dialect_check.py` (0 failures), `$RATW_TALK_CHECK --data "$PWD/Data/Talk" --coverage` (0 holes in the core moves), `python3
tools/test_mantle_check.py` (its kit table maps `plain` to `ridgemere` since fix 8, so a plain kit is gated
`self.from=ridgemere`; the other dialects' kits are gated on their own town's name).

**Mark banks** (doc 84 §7.10.4, §12.4): when a mantle passes to a newborn the neighbours would remember it in, its
marks are drawn again from the newborn's dialect's bank, `mark.<symbol>` (`Library::redrawHabit`), else from the
mantle's own other alternatives (which is why every mark needs two or more). Since fix 8 the grammar parser takes
`mark.<symbol> = alt | alt` lines: write them in the dialect's own grammar file, under its `grammar <dialect>` line
(`Data/Talk/grammar/coldwater.grammar`...; plain speech's bank in `plain.grammar`), about 140 alternatives each for
`mark.oath`, `mark.saying`, `mark.laugh` and `mark.pet` (§7.10.4). A bank is in no layer below (plain needn't have
it), takes no `.broad` and no `!` (its values become the wearer's own habit), and its values are sex-neutral (a sexed
`^m`/`^f` alternative is never drawn) and end without a stop, like any word. Check with `$RATW_TALK_CHECK --data
"$PWD/Data/Talk" --validate` (the grammars load with the library), `python3 tools/dialect_check.py --world
ridgemere3` (the deity lint reads every grammar line), then `python3 tools/talk_seed.py --only dialects --world
ridgemere3` to bring the world's `live.dialects` rows up to the files (the game reads the rows over the files; a
world whose rows predate the banks redraws from the mantles' own alternatives until this is run). The two banks
of one dialect never share a value with a mantle's `oath`/`saying` worn in the same town (G2's town check covers the
mantles only: check by eye against the index).

---

## 4. One resident, step by step

### 4.1 Get the brief

```
python3 tools/mantle_brief.py ridgemere3 tn_fh_jory_rimepeak --facts Saved/Mantles/residents.jsonl
python3 tools/mantle_index.py coldwater --world ridgemere3          # the town; run again before each new mantle
```

(`plan` already wrote both into `Saved/Mantles/<batch>/`; re-run the index as the batch grows, since your own
batch's earlier mantles are in it once their files are at their export paths.)

### 4.2 Read the brief: what each part is for

A real brief (abridged), for the most-heard Coldwater resident left:

```
# tn_fh_jory_rimepeak: Jory Rimepeak
male, 54 (adult); civilian / job none / trade words "loom hand at Coldwater Looms"
work: loom hand at Coldwater Looms; post loom hand at Coldwater Looms; workplace slot "Coldwater Loom Shed"; class lower, poor
home cn_coldwater_cottage_6 in coldwater (here.region=coldwater); street "The Greenholt, Middle"; quarter -
grew up: coldwater (Coldwater); away=no; accent light -> mantle dialect **coldwater**, accents `light broad`; tier **standard**
kept (never changed): honesty frank, quarter -
traits: talk=quiet humour=dry honesty=frank temper=even warmth=reserved piety=ordinary drive=steady; quirk remembers_every_name_ever_heard; mood even
likes blackberries draughts market_bustle autumn_wind; dislikes cold_tea bad_manners
their words for things: food_words "blackberries off the bush"; ... weather_words "a good blustery autumn"; dislike_words "tea gone cold"
married yes, widowed no, parent no, mourning no, master no, apprentice no
## Slots filled for this wearer (a line using another does not fit them)
- #self.spouse# = Varg Acorndell     - #self.spouse_word# = husband    - #self.friend# = Marl Cragpeak
- #self.boss# = Fallow Lakewick      - #self.workplace# = Coldwater Loom Shed   - #self.street# = The Greenholt, Middle ...
## Kin, household
- child: Dun Rimepeak (in the Country, not in a town: no slot names them) ... - same home: Marl Cragpeak (loom hand ...)
## Stances the game works out
- merchants: against 3 — there's nothing coming into my house, and still they put the bread up ...
## Personality and backstory (honour them; never copy: they name the wolf)
personality: Honest to a fault and quietly funny; he remembers every name ever heard.
backstory: Jory means to go down to Ridgemere and never come back up.
## The town's lore (coldwater) ... Its homes hold soap, loom and still hands, swineherds and shepherds, and hardly a fisher ...
## Example lines of the place ...
## The coldwater grammar's words (#symbol#; the dialect's own speech) greet = Still water | Calm water to you; ...
```

| Brief field | What you do with it |
|---|---|
| sex, age, stage | Who they are now. **Not in the mantle**: never write "as a woman", "my age" without gates; the persona has no pronoun. Age matters only to gate age claims (`self.age>=40`). |
| role / job / trade words / work / post / workplace slot | The work talk. Gate it: `self.job=<job>` when the job is meaningful (smith, baker, guard, merchant...); for `job none` (149 residents: loom hands, swineherds, apple pickers, royals...) see §8.3. Say the place and trade through `#self.workplace#` / `#self.trade_words#` where a name fits. |
| class, wealth, House | Register and worries (a poor loom hand vs a House cousin). Gate wealth talk: `self.wealth=poor|destitute`. House members: `self.house` (text); speak of "the House" generally. |
| home, town, `here.region`, street, quarter | Where they stand: gate place talk `here.region=coldwater` (Saltwell is `waste`, Fenshore `fens`); Ridgemere's quarter sets the register (§5.5). |
| grew up / away / dialect / accents | The dialect you write in and the accents line, **exactly as the brief says**. `away=yes` (they live away from their home town): homesick talk gated `self.away=yes`. |
| kept: honesty | Copy it into `traits` unchanged (it chooses thieves; the lint refuses a change). |
| traits, quirk | Copy all seven axes; you may refine the talk-only middle values (`warmth`, `talk`, `humour`, `temper`, `piety`, `drive`) if the voice needs it; the quirk may be changed only where two others in the town already have it (G2). |
| likes, dislikes, their words for things | The card's `likes`/`dislikes` (copy the ids), and material for scenes. **Write the words in your own phrasing**: every writer of the town gets the same brief text, and a 5-word run copied from it collides (the lint warns). |
| married/widowed/parent/mourning/master/apprentice | What you may claim, and gates (`self.married=yes`, `self.parent=yes`). |
| Slots filled | Which `#self.X#` slots this wearer fills now. A line using another slot isn't heard from them (keep at most a few such lines, for later wearers, and gate them). |
| Kin, household | Who they live with and their kin, for **feelings** and gating, never for names. Kin "in the Country" have no slot. |
| Stances | The opinions the game would give them, with reasons. Your `opinion` items override them; take the side, write the reason **in your own words**. |
| Personality, backstory, old greeting | Honour the gist (an honest, quietly funny loom hand who wants to leave for the city); never copy (they contain the name), and drop what isn't true in the world (§8.4). |
| Town lore, example lines | The place's things and idiom; what not to claim (Coldwater: "hardly a fisher"). |
| Grammar words | The dialect's symbols: your density comes from them and your own words (§5.6). For plain speakers the plan's brief lists plain's symbols. |

### 4.3 Decide the voice (a 10-minute worksheet; write the answers as a comment at the top of your file if you like)

1. **One line that captures them** (becomes `title`, 2-5 words, never spoken): "the name-keeper".
2. **Temperament in speech**: quiet → short turns, lets others talk; chatty → runs on, "and another thing"; dry →
   undercuts; merry → teases; warm → asks after you, offers food; cold → clipped, declines without softening; nervy →
   worries a thing twice; short-tempered → bristles; devout → the Bright One in passing; ambitious → money, plans.
3. **Their habit words**: an **oath** (2-3 alternatives) and a **saying** (2-3) nobody else in the town has (check
   the index); maybe a laugh, a pet name, a tic. These are the `marks`.
4. **Their own words** (10-18 symbols): the things they handle and love, said their way (`names = every name I ever
   heard | the names I keep`). These carry density and identity at once.
5. **Their favourite topics** (the card's `topics`, director topics with weights −1..+1): 3-5 they bring up, 1-2 they
   avoid. Give each favourite 2-3 scenes.
6. **What they want, fear, dislike**: `hopes`, `fears`, `dislikes` (ids `lower_case_words`).
7. **How they handle each move**: how they greet a stranger vs kin, take bad news, decline, thank, tease, leave.
8. **Their stories** (3 or 5): a memory only they would tell, gated so a later wearer can tell it truly.
9. **What a player sees** (window): how they greet a stranger, a known face, a hostile one; what they say about
   their work; their one graft (a question a player would ask them, because of the quirk).
10. **What nobody else in the town has**: read the index; if the quirk or a scene idea is taken, turn it.

### 4.4 The format reference (format 1, frozen 2026-11-17: §41)

A change to this format is a "format 2" (an engine and migration change): **never change the format**. Write
exactly this.

#### 4.4.1 The header (one field a line, in this order)

| Field | Required | Value |
|---|---|---|
| `mantle` | yes | the id: two letters of the dialect (`rm` plain, `cw` coldwater, `lf` larkfield, `sm` southmeadow, `oh` oakhaven, `fh` frosthollow, `mb` mirebridge, `sw` saltwell) and 3-4 digits (`cw103`); from `batch.tsv` |
| `revision` | **no, in a new draft** | written by the export; a loaded mantle's file keeps it (never edit it) |
| `state` | yes | `ready` (a `draft` is invisible to the game; write `ready`) |
| `dialect` | yes | `plain` or one of the seven: the brief's |
| `accent` | yes | 1-3 of `none light broad`, the first as written: the brief's `mantle accents` (`light broad`, `broad light` or `none`) |
| `tier` | yes | `full`, `standard` or `spare` |
| `written_for` | yes (not spares) | the resident's id |
| `written_by` | no | written by the loader: `--written-by WHO` (e.g. `codex`, `claude-opus`), else `claude-<batch>` |
| `title` | yes | 2-5 words for writers and the DM, never spoken: "the stone-keeper" |
| `fits` | yes | **stage only**: `stage=adult|old`, or `stage=young|adolescent|adult|old` for a pup's or a spare |
| `prefers` | yes | soft criteria for who suits it: `role=merchant`, `job=smith`, `temper=short`... |
| `traits` | yes | all seven: `temper=calm|even|nervy|short talk=quiet|plain|chatty warmth=cold|reserved|warm humour=none|dry|merry honesty=frank|plain|guarded|sly piety=devout|ordinary drive=content|steady|restless|ambitious` |
| `quirk` | yes | `lower_case_words` (the brief's, unless two in the town have it) |
| `likes`, `dislikes`, `fears`, `hopes` | yes | space-separated `lower_case_words` ids |
| `topics` | yes | director topics with weights in −1..+1: `topics lore +0.4 prices +0.3 gossip -0.4` (topics in §4.4.6) |
| `styles` | yes | `comfort=practical|company|... gossip=reluctant|eager|keen... refuse=soft|short... tease=none|dry|...` (free lower-case words; read by people, used lightly) |
| `marks` | yes | 2-4 of its own habits (`oath`, `saying`, maybe `laugh`, `pet`...), each a `name!` symbol in `words` with **2 or more** alternatives |
| `persona` | yes | one line for the window's AI (when one is connected), naming the wearer only as `#self.first#`, **with no pronoun at all** |
| `format` | no | omit (1) |

A comment line (`# ...`) may come first; keep the export's own comment line as the loader writes it.

#### 4.4.2 Sections

| Section | Who says its items |
|---|---|
| `words` | symbols (every wearer of the mantle) |
| `any` | items for every stage: usually the `opinion`s |
| `grown` | adults **and** the old (always, frozen so in §41) |
| `young` | pups, and adolescents unless an `adolescent` section exists |
| `adolescent` | adolescents |
| `old` | the old, **on top of** `grown` (for things only the old say) |

Put most items in `grown` (most wearers are adults). Pups' and spares' mantles need `young` and `adolescent` too.

#### 4.4.3 Items and their syntax

```
words
oath! = By the pebble and the pool | Stone and shade           # `!` a habit (one alternative fixed per wearer), `!!` per pair
stone = a wadi stone | a smooth one from the dry bed           # an own word, used as #stone# (and #stone.capitalize#)
any
opinion @1 road against 2 : Every load comes in a day late now, and it's #shelves# that pay for it.
opinion @2 road against 2 [world.wagons=raided] : #bandits.capitalize# on #saltroad# again.
grown
line @1 greet [pair.band=strangers] : #greet#, #address#. Come out of the sun. | #greet#. You've the look of #waste# on you.
rule @scales                                                   # a bark (or any full rule); runs to the next blank line
when concept=bark working=yes self.role=merchant
does aside
cooldown 6h
say Salt, oil, rope and dates. | Water skins mended while you wait.

scene noon_shutters                                            # a whole talk the wearer opens
when topic=weather|day here.time=day here.weather=clear self.working=yes
does tell.self / react.self / assess                           # one move a turn, in order
a : First thread. | Second thread. | Third thread.
b[from=saltwell] #sw : A Saltwell listener's answer, thread 1. | ...2 | ...3
b : Anyone else's answer, thread 1. | ...2 | ...3
a@sw : The wearer's reply to the Saltwell one. | ... | ...
a : The wearer's reply to anyone else. | ... | ...

story first_stone [topic=memories|lore self.age>=25]          # told once to each listener
  : A beat of the teller's.
  : Another beat.
  ask : A question the TELLER asks the listener?
  b[old] : An old listener's answer.
  b : Anyone's answer.

dlg @1 shop.greet [here.shop!=bare] : #greet#. Salt, oil, rope and dates, and every weight #honest#.
dlg @2 resident.news [world.news] : Word off the carts: #world.news#. Ask #carter# for the rest.

graft resident.hub                                             # the wearer's own window branch
choice What's in the jar on the counter? -> stones
node stones
say Stones from the dry wadi beds. Smooth ones.
mood pleased
free
choice Why keep them? -> stones_why
choice Goodbye. -> bye
node stones_why
say I hold one at noon and it stays cool. Pick one, if you like.
choice I'll leave them be. -> hub
choice Goodbye. -> bye
```

(The `#` comments above are this handbook's explanations; don't put trailing comments on item lines in your files.)

| Item | Syntax | Notes |
|---|---|---|
| symbol | `name = alt \| alt`, `name! = ...`, `name!! = ...` | in `words` only. A value **never ends with `.` `;` `!` `?`** (it is spliced into lines). Names `[a-z0-9_]`. Defining a plain/dialect symbol's name (e.g. `pet`) overrides it for this wearer. |
| opinion | `opinion [@name] <issue> <for\|against\|mixed> <0-3> [criteria] : alt \| alt` | issues in §4.4.7; one or two per issue, each with a reason in the words |
| line | `line [@name] <move>[.variant] [criteria] : alt \| alt` | 2-3 alternatives; moves in §4.4.5 |
| rule | `rule [@name]` then `when ...`, `does ...`, optional `cooldown 6h`/`tone`, `say alt \| alt` | barks: `when concept=bark ...`, `does aside`, a cooldown of 6-12 h |
| scene | `scene <name>` then `when ...`, `does m1 / m2 / ...`, optional `tone t1 / t2 / ...`, then turns | §4.5.5 |
| story | `story <name> [criteria]` then indented lines `  : beat`, `  ask : question`, `  b[...] : answer` | no `does`; the engine assigns tell.story / ask.detail / react.story / answer.detail. **`ask :` is the teller's question to the listener** ("Would you have stayed?"), never the listener's to the teller ("Did you catch one?": that goes at the end of the `b` line after it). At least one `b` line. |
| dlg | `dlg [@name] <tree>.<node> [criteria] : alt \| alt` | the node must exist (§4.4.8) |
| graft | `graft [@name] <tree>.<node>` then `choice <player's words> [criteria] -> <node>`, `node <name>`, `say ...`, `mood ...`, `free` | every node says something and has a way back; choices lead to its own nodes, the tree's nodes, `bye` or `end` |

Blocks (`rule`, `scene`, `story`, `graft`) run to the next blank line or the next item keyword at a line's start:
**put a blank line after every block.**

#### 4.4.4 Keys

Every item has a stable key: a line `<section>.<move>.<name>` (`grown.greet.1`, `grown.request.carry`), others by kind
and name (`words.oath`, `opinion.road.1`, `grown.scene.noon_shutters`, `story.first_stone`, `dlg.shop.greet.1`,
`graft.resident.hub`). In a draft you may leave `@name` off lines, opinions and dlgs (the loader numbers them and
writes the numbers back); **always name scenes and stories** (`scene noon_shutters`), uniquely within the mantle.
After loading, never rename or renumber an item: a key is how history, reviews and the DM point at it.

#### 4.4.5 Moves (`Data/Talk/moves.json`)

greet, greet.return, kin.open, again.open, hail, check_in, answer.check_in, tell.news, tell.gossip, tell.complaint,
tell.boast, tell.plan, tell.feeling, tell.worry, tell.self, tell.story, tell.memory, react.news (.new .known .doubt
.sorrow .glad .glee .again), react.complaint (.agree .disagree), react.self, react.story, assess, sympathise,
reassure, congratulate, laugh, ask.how, answer.how (.well .poorly .busy .grieving .deflect), ask.detail,
answer.detail, ask.opinion, give.opinion, ask.after, answer.after, ask.plan, answer.plan, ask.whereabouts,
answer.whereabouts, ask.name, agree, disagree, concede, insist, challenge, counter, tease, tease.return, tease.laugh,
take_offence, apologise, accept_apology, decline_apology, thank, deflect_thanks, praise, deflect_praise, confide,
keep_confidence, introduce, introduce.return, warn, pre.request, go_ahead, put_off, request, grant, decline,
accept_decline, offer, accept, refuse, invite, accept_invite, ask.advice, give.advice, raise, pivot, change_subject,
return, recall, repair, clarify, join.add, join.correct, join.defend, join.side, join.tease, join.object,
welcome_join, aside, preclose, accept_preclose, extend, farewell, farewell.return, leave, break, ack, backchannel,
echo. G1 refuses any other. Tones: `light even serious grave tender cross`.

#### 4.4.6 Topics (a scene's `when topic=...`; the card's `topics`)

Everyday: weather, day, smalltalk, work, food, family, home, friends, pastimes, animals, health, memories, dreams,
faith, travel, land, season, lore, accent. News: gossip, crime, bandits, caravan, prices, ships, event, times,
harvest, newcomer, festival, life, deed. Also `player` (talk about a traveller nearby; never "strangers" as a
topic), `quarrel`, `news`, `price_cause`.

#### 4.4.7 Opinion issues (`Data/Talk/opinions.json`)

watch, houses, dole, newcomers, church, merchants, city, road, festivals, drink, times, sump, strangers.

#### 4.4.8 Window nodes (`Data/Talk/dialogue/*.tree`)

| Tree | Who opens it | Nodes |
|---|---|---|
| resident | everyone | hub, town, job, business, news, gossip, trouble, helped, way, place, refused, bye |
| shop | merchants | greet, prices, bye |
| inn | innkeepers (and inn helpers) | greet, rooms, roomed, rest, refused, bye |
| watch | guards | hub, crime, reported, fine, paid, law, unpaid, bounties, refused, bye |
| church | priests | greet, service, poor, offering, thanked, refused, bye |
| master | masters | learn, work, wages, takes, taken, refused, bye |
| work, official, receiver, companion | job givers, officials, receivers, companions | see the tree files |

Read the tree file for any node you write: its default lines show which facts it reports (§4.5.8). Graft moods: the
trees use calm, formal, keen, pleased, stern, warm (G1 refuses an unknown mood).

#### 4.4.9 Slots (§11; filled from the wearer's facts; a line with an empty slot isn't said)

| Slot | Is | Gate a scene on it with |
|---|---|---|
| `#self.first#`, `#self.surname#` | the wearer's own names (rarely needed: other people say your name, you don't) | |
| `#other.called#` | what this wolf calls the listener: first name if they know each other, Master/Mistress + surname across a class gap in Ridgemere, else the dialect's `#address#` | |
| `#address#` | the dialect's way to address someone ("neighbour", "shore-friend", "my brother"/"my sister") | |
| `#other.first#` | the listener's first name | `pair.named=yes` |
| `#topic.player#` | the player's name (window lines only) | `topic.meeting=known` |
| `#self.spouse#`, `#self.spouse_word#` (husband/wife) | spouse | `self.married=yes` |
| `#self.child#`, `#self.mother#`, `#self.father#` | kin | `self.parent=yes`, `self.mother`, `self.father` |
| `#self.late#` | the kin last mourned (empty for everyone today) | `self.late` |
| `#self.neighbour#`, `#self.friend#`, `#self.rival#` (empty today), `#self.boss#`, `#self.hand#`, `#self.priest#`, `#self.shopkeeper#`, `#self.leader#` | people | `self.neighbour`, `self.friend`... |
| `#self.workplace#` ("Coldwater Loom Shed"), `#self.trade_words#` ("loom hand at Coldwater Looms"), `#self.street#`, `#self.from_town#` | places and trades | `self.workplace`, `self.from_town` |
| `#here.town#`, `#here.place_name#` | where they stand | |
| `#topic.subject#`, `#topic.claim#`, `#topic.victim#`, `#topic.they#`/`.them`/`.their`, `#world.news#`, `#world.news_from#`, `#world.event_words#`, `#topic.ship#`, `#topic.good#`... | the news a topic carries (news scenes and window lines) | the topic, `topic.claim_kind=...`, `world.news` |
| `#pair.echo_of#` | the dead last wearer's name, when the listener knew them (echo lines) | `pair.echo=yes` |

Facts you test but don't say: `self.away` (yes/no), `self.dialect`, `self.from`, `self.mantle`. Rules for slots:
gate a scene or story on the slot's fact so it renders whole; **no he/she/they in a sentence with a person slot**;
**claims about a slot person** only as feelings or what the line itself records (§5.7).

#### 4.4.10 Criteria and tags

- Criteria are `key=value`, `key=a|b`, `key!=value`, `!key` (absent), `key` (present), `key>=N`, `key<N`.
  In scenes `self.`/`a.` is the wearer, `other.`/`b.` the listener; in `b[...]` brackets the listener's facts are
  written bare (`b[old]`, `b[job=fisher]`, `b[from=saltwell]`, `b[friends]`, `b[wealth=poor|destitute]`,
  `b[stage=young]`, `b[pair.band=spouses]`).
- `b[...] #tag :` marks a listener's kind; `a@tag :` is the wearer's reply to that kind; a bare `b :` and `a :`
  follow for everyone else. **Always give a bare `b :`** (a scene must fit any listener).
- `{k=v}` before an alternative sets a conversation tag (`{did=swim} Had my swim...`), read later as `conv.did=swim`.
  Use sparingly; the pilot rarely needed it.
- Symbol modifiers: `#word.capitalize#` at a sentence's start, `.a` (a/an), `.s` (plural).

### 4.5 Writing each part, in order

#### 4.5.1 The card

Fill the header from the starting file and your worksheet. `prefers` names the role or job and one temperament
(`prefers role=merchant humour=dry`). The persona: "#self.first# keeps the store by the spring and lives above it:
patient, wry and plain, slow to gossip, honest at the scales, with a jar of smooth wadi stones on the counter." (No
"she", "he", "they", "their".)

#### 4.5.2 Words (12-20 symbols)

- `oath!` and `saying!` (the marks), 2-3 alternatives each, unique in the town (index), true to the dialect's
  idiom but **not** the grammar's own oath values (those are everyone's).
- 10-16 own words: their things, said their way. Prefer phrases a neighbour would recognise as *theirs*
  (`jar = the stone jar | my jar of stones`, `weighed = weighed twice | weighed and weighed again`).
- Values never end with `.`, `;`, `!` or `?`; one alternative is allowed but two read better.
- For an accent-`none` mantle, own words must be plain English (no dialect phrase written out).

#### 4.5.3 Opinions (8 full, 5-6 standard)

`opinion <issue> <side> <0-3> [criteria] : <the view, with a reason>`. Take the brief's stances as a start,
override where the voice wants, and **write the reasons in your own words**. Add a second, gated one where the
world can sharpen it (`[world.wagons=raided]`, `[self.robbed=yes]`). One or two sentences, in voice, with a symbol
or own word.

#### 4.5.4 Lines (30+ full, 18+ standard)

The edges of a talk: what the wearer says before and after a scene, in answer to a direct question, or as a joiner.
Write at least:

| Move | Variants to write |
|---|---|
| greet | `[pair.band=strangers]`, `[self.working=yes ...]`, `[here.time=dawn]`, `[here.time=dusk|night]` |
| kin.open | `[pair.kin!=no]` (kin don't greet formally) |
| again.open | met earlier today: a **statement**, never a question ("Twice in one day; I'm honoured.") |
| greet.return | bare, `[pair.band=friends|family]`, `[self.working=yes]`, `[here.time=dusk|night]` (**required**) |
| farewell / farewell.return | a pair (farewell.return **required**), and a night variant |
| react.news .new .known .doubt | (react.news **required**) |
| react.news.sorrow | `[topic.kind=death|loss|theft|assault|arrest|crime]` |
| react.news.glad | `[topic.kind=marriage|birth|apprentice|succession|work]` |
| answer.how (+ .busy `[self.working=yes]`), ask.how | |
| sympathise, reassure | |
| decline (+ accept_decline) | hedged, with a reason, 6+ words |
| thank, deflect_thanks | |
| preclose | `[self.working=yes]` and bare |
| tease / tease.return | only if they tease (`humour` dry/merry); `[pair.band=friends]` |
| ack | one or two words in voice ("#bc_it_is# \| #dryly#") |
| give.advice, warn `[world.bandits=near]`, hail `[pair.band=friends]` | as the character suggests |

Greetings in the opener's breath are **dropped by the engine when they ask a question** ("Morning. Sleep well?",
"Back again?", §37): write greetings as statements.

#### 4.5.5 Scenes (the body of the mantle: 30 full, 18 standard)

Since §37's "hybrid", a conversation is mostly **one whole written scene**, chosen by the engine for a topic, a pair
and a moment, and played as written; single lines are stitched only at the edges. When the wearer opens a talk on a
topic their mantle has a scene for, **their scene wins** (+5) and the listener hears *this* wolf. So scenes are where
a mantle is heard. Rules:

- **3-5 turns**, `a` first, alternating (`a / b / a`, `a / b / a / b`); `does` names one move per turn, `b?`
  (optional) turns included: `does tell.self / react.self / assess / ack`.
- **2-3 parallel threads**: the n-th alternative of every line belongs to thread n; every line has exactly 1 or N
  alternatives (the lint refuses a mismatch). Each thread must read as **one natural talk on its own**: read thread 1
  top to bottom, then 2, then 3.
- A turn is one or two short sentences (160 characters at most per alternative).
- **End on a line that closes**: an assessment, a laugh, a "so it is", a backchannel. **Never end a thread on a
  question nobody answers** (the lint warns).
- **The listener's lines are anyone's**: a bare `b :` must suit a stranger, a friend, a pup, any job; no claim about
  them but what its criteria say (`b[job=fisher]` may say "my nets"); never a dialect word written out in a bare
  `b :` (only `b[from=...]` may); listener lines carry dialect only through symbols (`#bc_it_is#`, `#hot#`).
- **Tagged branches** (`b[old] #elder :` ... `a@elder :`) give the talk a different turn for a kind of listener:
  1-2 per scene at most, where it makes the talk better (an old listener, a fellow of the trade, a pup, kin).
- **Gate the moment** in `when`: weather words `here.weather=` (indoors: `here.outdoors=`), hour words `here.time=`,
  season words `here.season=`, work talk `self.working=yes` and/or `self.job=`, place talk `here.region=`, home
  talk `self.from=` / `self.away=yes`, family talk `self.married=yes` etc.
- **Topics**: cover the everyday topics this wolf would really talk about (weather by kind, day, work, food, home,
  family if they have one, friends, pastimes, animals, health, memories, dreams, faith, season, lore, accent, travel,
  land), 2-3 scenes on each favourite topic, and a few news scenes (§5.9).
- **News scenes** use only the topic's facts (`#topic.subject# #topic.claim#`, `#world.event_words#`, `#topic.good#`,
  `world.times=hard|lean`, `world.wagons=late|raided`) and gate on the kind (`topic.claim_kind=theft|violence|...`
  before pity; `marriage|newcomer|promise_kept` before gladness). Never invent a price, a crew, a culprit, a cargo or
  a date.

#### 4.5.6 Stories (5 full, 3 standard)

A story is a scene told once to each listener, on `talk.memories` unless its criteria name a topic: 2-4 teller beats
(`  : ...`), optionally the teller's own question (`  ask : Do you keep anything, from when you were small?`), and
the listener's answer(s) (`  b : ...`, `  b[old] : ...`), then maybe a closing beat. Gate the invented past:
`self.age>=25` for "when I was a pup..." told as long ago, the job for trade memories (`self.role=merchant`),
`here.region=` for local legends. Only people **not in the world** in invented memories ("my grandad", "an old
carter"), never a resident.

**`ask :` is the teller speaking.** The engine gives it to the teller (ask.detail) and the next `b` line answers it
(answer.detail). So it asks the *listener* something ("Would you have gone back in for them?", "Do you know the ones
with a ring right round them?"), or teases the listener with what comes next ("Did the batch spoil?", answered with
a guess: "You caught the heat in time, perhaps."). It never asks the teller about the story: "Did you catch one?",
"Were you hurt?", "What did you do?" are the listener's questions, so they end the listener's line:

```
  : I waited at the river bend till the light went.
  b : You found the better water. Did you catch one?
  : Enough for a meal after the shift.
```

(Before fix 8, 183 stories in 106 mantles had the listener's question under `ask :`, so the teller asked it of the
listener; fix 8 folded each into the `b` line after it. The lint refuses the common forms now: `ask : Did you...`,
`Were you...`, `What did you...`, `Is that why you...`.)

#### 4.5.7 Barks (6 full, 4 standard)

Unaddressed calls, said to no one (`concept=bark`), each with a cooldown of 6-12 hours and gated: the trade's cry
`working=yes self.role=merchant` (or the job), the hour, the weather, a dislike (`self.dislikes=rats`), the world
(`world.wagons=late|raided`). One short sentence, two alternatives.

#### 4.5.8 Window lines (12-16 full, 4-7 standard)

What the wearer says to a player in the dialogue window. A mantle's `dlg` line **outranks the tree's own lines**
(+5), so (fix 8: G2 refuses a line that breaks the first rule below, and the game makes one that slipped in, in
Atlas or the DM, stand aside wherever one of its tree's fact lines holds: `yieldToTree` in Core/RatwMantle.cpp gives
it each fact line's first fact, negated):

- **Never write an ungated line for a node that reports facts.** The tree's lines there say real things (`resident.news`:
  `world.news`, `world.event_words`, `world.wagons=raided`, `world.death`, `world.wedding`; `resident.gossip`:
  `here.gossip=yes`; `resident.business`: `business=thriving|failing`; `shop.greet`: a bare shop; `inn.greet`: an inn
  helper; `watch.hub`/`fine`/`law`: `here.wanted`; `church.greet`/`service`: `here.day`; `master.learn`:
  `here.teaches`). An ungated mantle line there **hides them** (verified: `sw101`'s "Nothing worth the telling" wins
  over real news 20 times in 20). Write one line per case, gated like the tree's:
  ```
  dlg resident.news [world.news] : Word off the carts from #world.news_from#: #world.news#. That's all I've had.
  dlg resident.news [world.event_words] : It's all #world.event_words# at the well today. #saying#.
  dlg resident.news [!world.news !world.event_words world.wagons!=raided world.death!=recent world.wedding!=recent] : #news_none# #carts.capitalize# will bring some, if they come.
  dlg resident.business [business=thriving] : Turning folk away, near enough.
  dlg resident.business [business=steady] : Steady. #carts.capitalize# are late more than they're early.
  dlg shop.greet [here.shop!=bare] : #greet#. Salt, oil, rope and dates, and every weight #honest#.
  dlg watch.hub [here.wanted=no] : #greet#. Quiet so far, and I'd like it kept so.
  ```
  G2 refuses an ungated line on such a node, and its message gives the plain case to write. What counts as such a
  node and its facts is read from the trees (`mantle_check.window_facts`: every fact a node's `say[...]` lines test,
  but the hour, the meeting, the mood and the trade, and but a line of priority 10, which wins anyway). An
  alternative that says a fact (`#world.event_words#`) goes in a line gated on that fact, never beside a "nothing
  new" one.
- **Greetings by meeting and disposition** at the hub (and the role's greeting node): bare, `[topic.meeting=known]`
  (`#topic.player#` is the player's name), `[here.time=night]`, `[pair.disposition=hostile|wary]`.
- `resident.job`: their work in their words (gate on the job if the words are trade-specific); `resident.bye`.
- Full tier also: the role tree's nodes (shop.greet/prices/bye; inn.greet/rooms/roomed/rest/refused/bye; watch.hub
  ...; church.greet ...), copying the facts the tree's line uses (`inn.rooms`: `#here.bed_night#p`,
  `#here.bed_week#p`; `*.refused`: `#topic.why#`).

#### 4.5.9 Grafts (2 full, 1 standard)

A graft adds the wearer's own branch at a node (usually `resident.hub`, or the role's greeting): one choice a
player would really ask *this* wolf (about the quirk, the trade, the place), leading to 2 nodes, each with a way
back (`-> hub`, `-> greet`, `-> bye`). The player's choice text is plain English (players have no dialect); gate a
choice that only makes sense in one place (`[here.region=fens]`). The window shows at most 9 choices at a node and
the resident hub is already near that cap: graft **one** choice onto a hub, never more.

#### 4.5.10 `young`, `adolescent`, `old`

A pup's lines are a pup's: short, eager or shy, about play, food, kin, being told off, the town's wonders; never adult
work or money worries. An adolescent wants to be taken seriously, works as a learner, teases, is embarrassed. The
`old` section holds what only the old say (knees, the old days, the dead they knew, "in my day" gated on old).

### 4.6 A complete annotated example: `sw101`, the stone-keeper (full tier, Saltwell)

The pilot's lead wrote this first, written for Saltwell's storekeeper (`tn_fh_thistle_stonepick`), and it passes
G1, G2 `--strict` and G3. It is the best model of breadth and voice. **It also had four faults the newer checks
catch** (marked ✗ below); write yours without them. The whole file, in order, as it was at revision 1, with notes
after each part. (Fix 8 corrected the persona, the ungated window lines and a 5-word run in the file itself, now at
revision 2: read `Data/Talk/mantles-export/saltwell/sw101.mantle` for the corrected lines. The `resident.job` fault
below is still in it: no check catches it.)

```
# sw101, the stone-keeper (Docs/Design/84). Exported from ridgemere3 at revision 1: edit it in Atlas or the DM, or as a draft through tools/mantle_load.py.
mantle sw101
revision 1
state ready
dialect saltwell
accent light broad
tier full
written_for tn_fh_thistle_stonepick
written_by claude-p11-pilot
title the stone-keeper
fits stage=adult|old
prefers role=merchant humour=dry
traits temper=calm talk=plain warmth=reserved humour=dry honesty=plain piety=ordinary drive=steady
quirk keeps_smooth_stones_from_the_wadi_beds
likes barley_soup old_tales festival_bells summer_evenings
dislikes early_bells rats short_weight
fears a_dry_spring
hopes a_shelf_that_never_empties
topics lore +0.4 prices +0.3 weather +0.2 player +0.2 gossip -0.4
styles comfort=practical gossip=reluctant refuse=soft tease=dry
marks oath saying
persona #self.first# keeps the store by the spring and lives above it: patient, wry and plain, slow to gossip, honest at the scales, and she keeps a jar of smooth stones from the wadi beds on the counter.
```

- The comment, `revision 1` and `written_by` were written by the loader and export; your draft has no `revision`
  or `written_by` line.
- `accent light broad`: the wearer is light (the first, "as written"); a broad later wearer is allowed. `tier full`:
  she is a merchant. `fits stage=adult|old`: stage only, so a newborn can inherit it (they grow into it).
- `traits`: all seven, honesty `plain` kept from the brief. `quirk`: a `lower_case_words` id, specific enough to
  write scenes from (a jar of wadi stones on the counter), and not a third in the town (the index shows the town's).
- `topics`: lore, prices, weather and travellers up; gossip down ("slow to gossip").
- `marks oath saying`: both have three alternatives in `words`.
- ✗ **The persona says "she"**: a male newborn could inherit this mantle. Write: "...honest at the scales, with a
  jar of smooth stones from the wadi beds on the counter." (G2 refuses any pronoun in a persona since fix 8.)

```
words
oath! = By the pebble and the pool | Stone and shade | By scale and spring
saying! = A smooth stone was a rough one once | The well keeps water, not secrets | What the sand takes, a cart brings back
stone = a wadi stone | a smooth one from the dry bed | a river-stone, flat and grey
jar = the stone jar | my jar of stones
shelves = my shelves | the shelves
counter = the counter | this counter
carts = the Saltpan carts | the carts off the sand
saltroad = the Saltpan road | the road off the pan
soup = a bowl of barley soup | barley soup, thick enough to stand a spoon in
bells = the festival bells | the bells on a feast morning
evening = a long summer evening | the cool after a long day
noon = the white noon | noon, when the shadows hide
ratword = the grain-thieves | the whiskered robbers
weighed = weighed twice | weighed and weighed again
dryly = Hm | Well, now
honest = honest as the spring | true to the grain
walk = a walk to the wadi | a turn by the dry bed
sweet = sweet as the first dates | sweet
breath = a breath of shade | a sit in the shade
upstairs = up over the shop | up the back stair
carter = a carter | a carter off the sand
waste = the Waste | the sand
spring = the spring | the spring pool
oil = lamp oil | good lamp oil
rakers = the rakers | the pan-rakers
city = the city | Ridgemere
till = the till | the cash box
datesw = the dates | the new dates
bell = a bell | some fool's bell
```

- 29 symbols (full asks 16-20; more is fine). The oath and saying are hers alone in Saltwell (index). Each own word
  is a thing she handles or loves, said her way; together they carry most of the mantle's 60%+ density: every line
  that says `#jar#` or `#carts#` counts as Saltwell speech.
- `dryly = Hm | Well, now` is a tic used as a symbol: fine once in a while, padding if on every line.
- No value ends with a stop. `soup`'s second alternative is long and vivid: own words can be little pictures.

```
any
opinion @1 road against 2 : #saltroad.capitalize#? Every load comes in a day late now, and it's #shelves# that pay for it.
opinion @2 road against 2 [world.wagons=raided] : #bandits.capitalize# on #saltroad# again. A cart that never comes is the dearest cart of all.
opinion @1 strangers for 2 : Travellers are half my trade and all my news. #saying#.
opinion @1 newcomers for 1 : New faces bring new coin. I'm not too proud to take it, and they're not too proud to pay.
opinion @1 merchants for 1 : I keep a counter myself. Nobody who hasn't paid #carter# should talk to me about prices.
opinion @1 watch for 1 : They keep the light paws off #counter#. That's all I ask of anyone with a stick.
opinion @1 city against 1 : Ridgemere? Smoke and elbows, and nobody knows whose door is whose. #dryly#. They can keep it.
opinion @1 festivals for 1 : I'll forgive a feast anything for #bells#. Even the dancing.
opinion @1 drink mixed 1 : A cup of #ale# at dusk is a kindness. Three is a debt.
```

- Opinions live in `any` (all stages). Every one gives a **reason** in her voice; the second `road` opinion is gated
  on the world (`world.wagons=raided`) so it is said only when it's true.
- "Smoke and elbows" came from the brief's stance reason ("it's all smoke and elbows"): two other pilot mantles
  used it too. Paraphrase the brief; the lint warns on 5-word copies.

```
grown
line @1 greet [pair.band=strangers] : #greet#, #address#. Come out of the sun. | #greet#. You've the look of #waste# on you, #address#.
line @2 greet [self.working=yes self.role=merchant] : #greet#. Mind the step; it's worn smooth. | #greet#. #counter.capitalize#'s open, and so am I.
line @3 greet [here.time=dawn] : #greet#. You're up before the shutters. | #greet#. The well's still cold at this hour.
line @4 greet [here.time=dusk|night] : #greet#. Late for buying, early for sleep. | #greet#. The stars are out, and so are you.
line @1 kin.open [pair.kin!=no] : Oh. You. Sit down before you knock #jar# over. | There you are, #pet#. I'd wondered.
line @1 again.open : #welcome_back#. I've nothing new since this morning. | Back so soon? #shelves.capitalize# haven't moved.
line @1 hail [pair.band=friends] : #address.capitalize#! A word, when you've a breath. | Hoi. Over here, out of the glare.
line @1 greet.return : #greet#. | #greet#. Mind the step.
line @2 greet.return [pair.band=friends|family] : #greet#. Come in, then. | #greet#. I was just thinking of you.
line @3 greet.return [self.working=yes self.role=merchant] : #greet#. #counter.capitalize#'s yours. | #greet#. Take your time.
line @1 farewell : #farewell#. | #farewell#. Mind the step.
line @2 farewell [here.time=dusk|night] : #farewell#. Keep to the lit side of the lane. | #farewell#. The night's #cold# off #waste#.
line @1 farewell.return : #farewell#. | #farewell#, #address#.
line @1 react.news.new : #oath#. I'd not heard. | #surprise#. Well, it'll be all over the well by noon.
line @1 react.news.known : I had it from #carter# at #counter#. | #knew#. Old news by the time it reached me.
line @1 react.news.doubt : #dryly#. Carters' news. I'll believe it when it's weighed. | #doubt#. #waste.capitalize# tells tall tales.
line @1 react.news.sorrow [topic.kind=death|loss|theft|assault|arrest|crime] : That's hard. #pity#. | I'm sorry to hear it, #very#.
line @1 react.news.glad [topic.kind=marriage|birth|apprentice|succession|work] : #good.capitalize# news. Something #sweet# for once. | #gladnews#. I'll take that.
line @1 answer.how : Steady. #shelves.capitalize# are full and my feet aren't sore. | #fine#. I've had worse days and sold more.
line @1 answer.how.busy [self.working=yes] : Run off my paws. Three of #carts# in and one boy to unload them. | #busy.capitalize#, and glad of it.
line @1 ask.how : And you? Keeping out of #noon#? | How's your day weighing, #address#?
line @1 sympathise : That's a heavy sack. Sit a while. | #sympathy#. I'll put the kettle over.
line @1 decline : #hedge#, but no. #counter.capitalize# won't mind itself, and I can't leave it. | I'd like to. Only #carts# come today, and someone has to count them in.
line @1 accept_decline : #no_matter.capitalize#. No harm. | It'll keep. Most things do, out of #noon#.
line @1 thank : #thanks#. | That's good of you, #address#. I'll remember it.
line @1 deflect_thanks : #deflect.capitalize#. You'd do the same. | #dryly#. Pay me in news.
line @1 preclose [self.working=yes] : Well. #shelves.capitalize# won't fill themselves. | I'd best get back to #counter#.
line @2 preclose : Well. #preclose#. | The light's going, and so am I. #farewell#.
line @1 tease [pair.band=friends] : You walk like one of #carts# with a wheel off. | You've sand in your ears again, #fool#.
line @1 tease.return : Says the one with a jar of stones. | #dryly#. I'll weigh that remark and charge you for it.
line @1 ack : #bc_it_is# | #dryly#.
line @1 give.advice : #advise.capitalize#: buy salt before #carts# come, not after. | Never trust a scale you haven't watched settle. #saying#.
line @1 warn [world.bandits=near] : Keep off #saltroad# a while. #warn#. | If you're crossing, cross with company.
```

- Openers by frame (strangers, at work, dawn, dusk), kin's own opener, a friend's hail, and `again.open`. The
  answers to the core moves (greet.return, farewell.return, react.news and its kinds) the tier requires.
- `react.news.sorrow`/`.glad` are gated on the news kind: pity only for bad news, gladness only for good.
- `decline`: hedged (`#hedge#`) with a reason, both alternatives; `accept_decline` paired with it.
- Closings come in pairs (farewell / farewell.return). `preclose` gated on work.
- The work lines are gated `self.working=yes self.role=merchant`: a later wearer who isn't a merchant never says
  "the counter's open". ("Mind the step" is used three times across lines: a deliberate habit; don't go further.)
- ✗ `again.open`'s second alternative asks ("Back so soon?"); the engine drops questions from the opener's breath
  (§37). Write it as a statement.

```
scene stone_jar
when topic=smalltalk|pastimes self.working=yes self.role=merchant
does tell.self / ask.detail / answer.detail / assess
a : Found another for #jar# this morning. Grey, flat, smooth as a cup. | That jar on #counter#? Every one of them came out of a dry wadi bed. | Count them if you like. Sixty-one. I've stopped pretending it's a reason.
b : Why stones? | Why do you keep them? | What are they for?
a : Because the water made them smooth, once, and they remember it. | A stone that's lain in running water has a look. You'll see it once I've shown you. | For nothing. That's what I like about them.
b : #bc_fair_enough# | Strange thing to keep. | #bc_maybe#

scene noon_shutters
when topic=weather|day here.time=day here.weather=clear self.working=yes
does tell.self / react.self / assess
a : #noon.capitalize#. Even #datesw# are sweating in their sacks. | Shutters half down. Nobody buys rope at #noon#. | I keep #stone# in my paw at this hour. It stays cool longer than you'd think.
b[from=saltwell] #sw : Work at dawn, work at dusk, and leave the noon alone. | Nobody buys anything at noon. We sleep. | My mother kept one in her cheek. Said it fooled the thirst.
b : #hot.capitalize# out there. | Sensible. | Does it help?
a@sw : I've told #shelves# the same. They don't listen. | Most of us. I count. | Clever woman. I'll try it.
a : #bc_it_is# | Shade and a stool. That's my trade at #noon#. | It does. Try it. Any flat one will do.

scene cool_evening
when topic=weather|day here.time=dusk here.weather=clear
does tell.feeling / react.self / assess / ack
a : This is the hour I'd keep if I could keep only one. #evening.capitalize#. | Smell that? #waste.capitalize# gives the heat back to the sky. | The day's heat is going off the walls like a sigh.
b : It is a good hour. | I can smell it. | I'd keep it too.
a : Nobody wants anything from me. That's the joy of it. | The Waste breathes out. Then we can breathe in. | Then we're agreed on something. #bc_ha#
b? : #bc_lovely# | #bc_true# | Mark the day.

scene night_stars
when topic=weather|day here.time=night here.weather=clear
does tell.self / react.self / assess
a : Look up. You can see the dust between the stars tonight. | I take my #soup# up on the roof on nights like this. | There's frost on #jar#'s lid. Out here, after a day like that.
b : I'd never seen so many till I came out here. | Up on the roof? | #cold.capitalize#.
a : They're the only thing in #waste# nobody's selling. | Every roof in #here.town# has someone on it. Look. | #cold.capitalize#, and the sky's never clearer. A fair trade.

scene sand_wind
when topic=weather here.weather=sandstorm|overcast here.region=waste
does tell.complaint / react.complaint / assess
a : Sand in the #oil# again. I'll be straining it till Restday. | Wind off the dunes. Everything I own will taste of it. | Shut the door behind you, for pity's sake. #oath#.
b : It gets into everything. | Every wind, the same. | Sorry. Shut.
a : Everything but #jar#. They're used to it. | #saying#. | #dryly#. You're forgiven. Once.

scene rain_bloom
when topic=weather|season here.weather=rain here.region=waste
does tell.news / react.news / assess
a : Rain. Real rain. Leave the door wide; I want to hear it. | #rain.capitalize#! Come and look at the street. | Rain on the roofs. I'd forgotten the noise it makes.
b : I'll stand in it. | It's like a feast day. | Loud, isn't it?
a : Do. Nobody will think you mad today. | In a week there'll be flowers on the dunes, and gone in another. | Louder than #bells#. I'll forgive it.
```

- **Three threads, every line three alternatives.** Read thread 3 of `stone_jar` alone: "Count them if you like.
  Sixty-one. I've stopped pretending it's a reason." / "What are they for?" / "For nothing. That's what I like
  about them." / "#bc_maybe#". It is one talk, dry, and ends on a closing backchannel.
- Every scene **gates its moment**: the noon scene `here.time=day here.weather=clear`, dusk `here.time=dusk`, the
  sand wind `here.weather=sandstorm|overcast here.region=waste`, the rain `here.weather=rain here.region=waste`.
- `noon_shutters` shows a tagged branch: a Saltwell listener (`b[from=saltwell] #sw`) knows the noon custom and may
  write Saltwell words out; the wearer answers them (`a@sw`); everyone else gets the bare `b :` and `a :`.
- The weather is spread over **several** scenes by kind and hour, so the topic comes round as different talks.
- `cool_evening` ends on an optional `b?` backchannel: the scene ends whether or not it is said.

```
scene feast_bells
when topic=festival|season self.likes=festival_bells
does tell.feeling / ask.opinion / give.opinion / assess
a : Did you hear #bells# this morning? I was up before them for once. | I'd stand the whole feast for #bells# alone. | When #bells# go I put the shutters up, and nobody minds.
b : I heard. Too early. | You like the bells? | Shutters up on a feast day? You'll lose trade.
a : Early's the point. They wake the whole Waste. | More than the dancing. Less than #soup#. | I'll lose a morning. I'd lose more not hearing them.
b : #bc_fair_enough# | Each to their own. | #bc_true#

scene early_bells
when topic=day|faith here.time=dawn
does tell.complaint / react.complaint / assess / ack
a : Somebody rang #bell# before light again. Not a feast. Just #bell#. | Bells before dawn ought to be a matter for the Watch. | I was dreaming of soup, and then #bell#.
b : A bell? Who? | You'd lock up a bell-ringer? | Lucky dream, until then.
a : I didn't ask. I'd have said something. | For a night or two. To think it over. | It was #soup#, too. Gone.
b : #bc_ha# | Fair. | #bc_bad_luck#

scene soup_and_bread
when topic=food
does tell.self / react.self / assess / ack
a : I've #soup# on the hob upstairs. It'll be better tomorrow. | Barley, an onion, salt from the pan. #soup.capitalize# isn't clever, and that's why I like it. | You can tell a wolf by their soup. Mine is plain and keeps.
b : I'll smell it from the street. | Salt from the pan? | What's mine say, then?
a : You'll smell it and you'll come asking. | Weighed twice, like everything. | Bring me a bowl and I'll tell you.
b? : #bc_maybe# | #bc_good# | Ha.

scene dates_in
when topic=food|prices self.working=yes self.role=merchant
does tell.news / ask.detail / answer.detail / assess
a : Dates are in from the palms. The good ones, the dark ones. | #datesw.capitalize# are sweet this year. I've had three and I'm counting. | Half the date sacks are full of dust and twigs. The other half are worth it.
b : Dear? | How much? | Which half did I buy?
a : Fair. I'll not pretend they grew themselves. | A copper for a fistful, if you don't eat them while I'm weighing. | The good half. I picked them over myself.
b : #bc_fair# | I'll take a fistful. | #bc_thank_you#

scene short_weight
when topic=prices|work self.working=yes self.role=merchant
does tell.complaint / react.complaint / insist / ack
a : The last sack of salt came in light. A finger short, near enough. | Somebody between the pan and here keeps a heavy thumb. | I #weighed# that rope and it came up a stride short.
b : Will you say something? | Could be the scale. | They'll say it stretched.
a : I'll say it to #carter#, and #carter# can say it up the line. | My scale's #honest#. I've watched it settle every morning for years. | Then it can stretch back. I'll be waiting.
b : #bc_good# | #bc_fair_enough# | Ha.

scene carts_due
when topic=caravan|work self.working=yes
does tell.worry / reassure / assess / ack
a : #carts.capitalize# were due at dawn. I've been looking up #saltroad# since. | No dust on #saltroad# yet. They're late. | Two days late. I've half a shelf of rope and no #oil#.
b : They'll come. #reassure#. | Late isn't lost. | Can't you buy elsewhere?
a : They will. I'll still look. | I know it. I say it to #jar#. | Elsewhere is three days over #waste#. I'll wait.
b? : #bc_true# | #bc_we_will_see# | #bc_fair#
```

- `feast_bells` is gated on her own like (`self.likes=festival_bells`): a later wearer who doesn't share it won't
  claim it.
- `early_bells` is her dislike, played as dry comedy; `soup_and_bread` a like. Card and scenes agree.
- `dates_in` thread 2 names a price ("a copper for a fistful"): a small own-shop claim, gated on a merchant at work.
  Don't go further than this: never a price as news.
- ✗ `carts_due` is on `topic=caravan` but gated only on work: it claims the carts are late whenever the caravan
  topic comes up. Gate it on the world: `world.wagons=late` (and write a `world.wagons=in` scene for when they come).

```
scene salt_from_pan
when topic=work|land
does tell.self / ask.detail / answer.detail
a : #rakers.capitalize# bring the salt down from the pan in the cool, and I take it by the sack. | Saltpan salt's coarse. #city.capitalize# wants it fine. I tell them to bring their own mortar. | I've never raked a day in my life, and I'm glad of it.
b : Is it good salt? | Do they want it fine? | Hard work, raking?
a : Clean, white, and sharp enough to wake you. The best in the realm, and I'll say it twice. | They want everything fine in #city#, and want it cheap. | Hard enough that I never haggle with a raker. Not by a grain.

scene upstairs_rooms
when topic=home
does tell.self / react.self / assess
a : I live #upstairs#. Two rooms and a window that looks at #spring#. | I can hear #till# from my bed. It's a comfort, oddly. | The stair's steep, and my knees know it already.
b : A view of the spring? | You hear the till? | Steep stairs keep thieves out.
a : #spring.capitalize# and every soul who comes to it. It's better than news. | Silence in #till# at night means nobody's in it. | Thieves and visitors both. #bc_ha#

scene rats_again
when topic=animals|home
does tell.complaint / ask.detail / answer.detail / assess
a : #ratword.capitalize# have been at the barley again. Through the sack, neat as you like. | I set a trap for the rats and caught a lizard. He looked as sorry as I felt. | I'd keep a cat if I could trust it with #datesw#.
b : Did you catch it? | What did you do with the lizard? | Would a cat eat dates?
a : Not yet. It's cleverer than I am, and quieter. | Let him go. He'd done nothing but be curious. | A cat would eat anything I'd priced.
b : #bc_ha# | #bc_good# | #bc_true#

scene the_spring
when topic=lore|smalltalk here.region=waste
does tell.story / react.story / assess
a : They say #spring# was found by a lost carter following a hawk. | My #grandad# said the pool was deeper once. You could dive and never touch. | Every stone at the bottom of #spring# is a wish somebody made.
b[from=saltwell] #sw : That's the tale, and a good one. | Mine said the same. Every grandfather does. | And never one of them answered, my mother said.
b : A hawk? | Is that true? | Have you thrown one?
a@sw : I'd rather that tale than the truth. | They'd all dived in it, to hear them. | Answered or not, they're still there.
a : A hawk, and then a well, and then us. | Maybe. Old wolves lie kindly. | Never. I keep #jar#.

scene old_tales
when topic=lore|pastimes self.likes=old_tales
does tell.feeling / ask.detail / answer.detail / assess
a : I'll trade a good tale for a cup of oil, if it's told properly. | There's #carter# who comes in Marketday who knows every tale of the old caravans. I sell to him at cost. | I like a tale told slowly, with the ending kept back.
b : What makes it proper? | At cost? For tales? | Like what?
a : No hurrying, and nothing explained. | At cost, and he knows it, and I know he knows. | The one about #carter# and the well that moved. I'll tell it you some evening.
b : #bc_fair# | Ha. | #bc_i_hope_so#

scene the_city
when topic=travel|accent
does give.opinion / ask.opinion / insist / ack
a : I went to Ridgemere once. Once. | #city.capitalize# sends its rope out here and calls it rope. | They asked me in Ridgemere what a well was for. I didn't know where to start.
b : What did you think of it? | It's not good rope? | Truly?
a : Smoke and elbows. I came home and sat by #spring# a whole day. | It's rope for a city. Out here we hang buckets on it. | Truly. #bc_true#. They have pipes.
b? : Pipes. | #bc_ha# | I'd like to see it.

scene traveller_news
when topic=newcomer|player|smalltalk self.working=yes self.role=merchant
does tell.self / react.self / assess / ack
a : A traveller in this morning wanted water, a map and the way to Ridgemere, in that order. | Strangers come to #counter# for water and leave with all my news. | I like a traveller. They pay, and they tell you how things are three wells off.
b : Did you give them a map? | Fair trade, news for water. | You like strangers?
a : I gave them water, and the way. A map's nothing out here. #waste.capitalize# moves. | Fair enough, if it's true news. | I like their coin. And their manners, mostly.
b : #bc_fair# | #bc_true# | #bc_ha#

scene gossip_held
when topic=gossip|crime
does tell.gossip / react.news / insist / ack
a : I heard a thing at #counter#. I'll not say who said it. | People talk at the scales as if I'm a sack. I hear everything and say little. | #dryly#. Half of what's said in this shop is about somebody not in it.
b : Go on, then. | You must hear plenty. | And the other half?
a : No. It'll keep, and so will I. | Plenty. And I sell it for nothing, which is to say I don't. | About the price of salt. The #honest# half.
b : #bc_ha# | #bc_fair_enough# | Ha. #bc_true#

scene lost_brother
when topic=memories|health self.age>=25
does tell.memory / sympathise / assess
tone grave / tender / even
a : I had a brother who went down a mine. He didn't come up. It was a long time ago. | Every time I hear a pick I think of a brother of mine. Old ache. | I don't go near a mine. You'll know why if you know me.
b : I'm sorry. | #sympathy#. | I didn't know.
a : It's an old stone. Smooth now. I can hold it. | #bc_thank_you#. Carry on. | Few do. I don't say it often.
```

- `the_spring`: local lore gated `here.region=waste`; the `b[from=saltwell]` branch knows the tale already.
- `gossip_held`: a **news topic with no facts invented**: she *withholds* gossip, which fits "slow to gossip" and
  needs no claim. The best kind of news scene for a reticent wolf.
- `lost_brother`: an invented past about someone **not in the world** (an unnamed brother, long dead), gated
  `self.age>=25`; `tone grave / tender / even` keeps light lines away after it.
- `traveller_news` says "them" of an unnamed traveller and `rats_again` "he" of a lizard: both fine. The rule (R8)
  is about a wolf the line **names by a slot**; an unnamed, unseen person may be "they".

```
scene bread_and_salt
when topic=friends|lore here.region=waste
does tell.self / react.self / assess
a : Bread and salt shared, and you're a friend. That's the custom. I've made a lot of friends at a copper a sack. | A guest is fed three days, then a neighbour. I'm still waiting on a few to turn neighbour. | The stranger draws first at the well. Even in a drought. That's us.
b : Cheap friends. | Some never do. | Even in a drought?
a : The best kind. They come back. | #dryly#. Some just come back for the bread. | Even then. That's the point of it.

scene dun_scale
when topic=work self.working=yes self.role=merchant
does tell.self / ask.detail / answer.detail / assess
a : I set the scale every morning against a stone I know the weight of. | Everything on #counter# gets #weighed#. Even #datesw# I eat. | I keep a lump of pan-salt for a weight. It's never lied to me.
b : How do you know its weight? | Even your own dates? | Salt for a weight?
a : My #dad# weighed it against his, and his father against his. It's the oldest thing I own. | Especially those. #oath#. | It's heavier than it looks. Like most #honest# things.
b? : #bc_wise# | #bc_ha# | #bc_true#

scene hard_times
when topic=times|prices world.times=hard|lean
does tell.worry / react.complaint / reassure / ack
a : Nobody's buying past the bread and the salt. I've rope going grey on the hook. | Lean, this month. I've let three wolves run a tab. | I can tell the times by what's left on #shelves# at dusk.
b : Hard for everyone. | Will they pay? | What's left?
a : Hard, but #spring# still runs. #reassure#. | They'll pay. Bread and salt, I said. That's a promise. | Everything but the salt.
b? : #bc_true# | #bc_i_hope_so# | #bc_ha#

scene good_times
when topic=times|prices world.times=good|boom
does tell.news / react.news / assess
a : Sold out of #oil# by noon. I've not said that in a year. | Coin's moving. Even #rakers# bought something they didn't need. | Three carts in a week, and every one of them heavy.
b : Good for you. | What did they buy? | Heavy with what?
a : #shelves.capitalize# will thank me. I will, once I have counted. | Ribbon. For the feast. I didn't ask. | Salt going out, dates and rope coming in. The way it should be.

scene summer_evening
when topic=season|day here.season=summer here.time=dusk
does tell.feeling / react.self / assess
a : #evening.capitalize#, and nobody hurrying. If I had a wish, I'd spend it on this. | Summer evenings are why I never left. | Sit a while. The shop can wait till the stars.
b : I'll sit. | Never once? | The shop can wait?
a : #bc_good#. Mind the stool, it rocks. | Once, to #city#. I told you how that went. | It's waited before. It'll sulk, and then forgive me.

scene winter_nights
when topic=season|weather here.season=winter
does tell.complaint / react.complaint / assess
a : Winter in #waste#. Warm by day, and at night I sleep in two blankets and my coat. | The cistern skinned over with ice this morning. In #waste#. | Nobody believes the nights out here till they've spent one.
b : Two blankets? | Ice? | I believe it now.
a : And a hot stone wrapped in wool. I've stones enough. | Thin as a leaf, but ice. #surprise#. | Everyone does, after one.

scene neighbour_vines
when topic=friends|smalltalk self.neighbour
does tell.self / react.self / assess
a : #self.neighbour# came by for #oil# and stayed an hour. I sold nothing else. | I can hear #self.neighbour# through the wall some nights, singing. Badly. | #self.neighbour# brought me a stone off the hill road. It's not a wadi stone. I kept it anyway.
b : Good company, though? | Badly? | Kind, though.
a : Good company's worth an hour. Just. | Badly, and every night. I'd miss it if it stopped. | Kind, and wrong about stones. I'll take kind.

scene dreams_of_rain
when topic=dreams
does tell.self / ask.detail / answer.detail / assess
a : I dreamt #spring# ran over and filled the street, and I was selling boats. | I dream of #shelves# empty and #carts# never coming. Then I wake and count them. | Last night I dreamt I could hear #jar# in my jar talking. They were dull.
b : Did you sell any? | Every night? | What did they say?
a : Three. Weighed twice. Even in a dream. | Not every night. Enough. | The weather, mostly. Like everyone else.
b : #bc_ha# | #bc_poor_soul# | Ha.

scene faith_ordinary
when topic=faith
does give.opinion / react.self / assess
a : I give at the feast, and I keep my scale #honest#. That's my prayer. | The Bright One watches the scales, my #mum# said. I'd rather not find out. | I'm no great one for the chapel. I say thank you for #spring# every morning, though.
b : That's prayer enough. | Wise not to. | That's something.
a : I hope so. It's the one I can keep. | #bc_ha#. Wise, or frightened. | Every morning. It's never once failed.

scene bad_health
when topic=health
does tell.self / react.self / reassure
a : My back's stiff from the date sacks. I'll be an old crone by Restday. | I've a cough from #waste# wind. A cup of #remedy# and it'll pass. | I've not been sick a day in years. I'm afraid to say it aloud.
b : Lift with your legs. | #remedy.capitalize#? | Knock on the counter.
a : I lift with whatever's nearest. That's the trouble. | My #gran#'s cure. It works or it doesn't. | Knocked. Twice, for luck.

scene accent_strangers
when topic=accent self.away=no
does tell.self / react.self / assess
a : A city wolf asked me today why we bless everyone. I said it's cheaper than not. | Strangers think we talk slowly. We talk the length the words need. | Every carter I've ever met talks like their road.
b : Is it? | Fair enough. | And yours?
a : Peace costs nothing. A quarrel costs a customer. | Hurry's for #city#. Out here the heat sets the pace. | Mine's #saltroad#. Long, dry, and gets there in the end.
```

- `hard_times`/`good_times` are the right way to do the times: **two scenes gated on the world's own state**
  (`world.times=hard|lean`, `good|boom`). G3 said neither was heard in the pilot's run: the save had ordinary
  times. That's fine: they wait for the world.
- `neighbour_vines` uses a **person slot** (`#self.neighbour#`), gated on `self.neighbour`, with **no pronoun** in
  any sentence that names them.
- `faith_ordinary`: the Bright One, never "gods".
- `accent_strangers` is gated `self.away=no`: she talks of Saltwell speech as one who lives there.
- ✗ `bad_health` thread 1 says "I'll be an old crone": "crone" is a woman's word, so a male later wearer says it
  wrongly. A mantle doesn't know its wearer's sex: use sex-neutral words ("an old stick", "bent double").
- ✗ `neighbour_vines` invents deeds of a real resident ("came by for oil and stayed an hour", "singing through the
  wall"): §9.6 rule 4 allows slot people only for **feelings, facts the game holds, or what the line records**.
  Prefer "I'd trust #self.neighbour# with my keys" or "#self.neighbour#'s good company on a long evening".

```
story first_stone [topic=memories|lore self.age>=25]
  : My #grandad# put #stone# in my paw when I was a #child#. Said it had been a rock once, sharp, and the water took its time.
  : I kept it in my cheek through a whole dry summer. It's at the bottom of #jar# now.
  ask : Do you keep anything, from when you were small?
  b[old] : Nothing I can find. My memory, mostly.
  b : Not a thing. I wish I had.

story moving_well [topic=lore|pastimes]
  : There was #carter# who swore the well moved in the night. Every morning he came in, the well was a step further from his cart.
  : He moved his cart to follow it, and by the end of the season he'd crossed the whole square.
  : Turned out his ox was walking in his sleep. #dryly#. The ox's, I mean.
  b : Ha! The ox! | That's a good one.
  b[old] : I've heard it with a goat.

story first_counter [topic=work|memories self.role=merchant self.age>=25]
  : The first day I kept the store alone I sold #carter# a coil of rope and forgot to charge him.
  : He came back the next week and paid, with a stone off the dunes on top of the coin.
  ask : Would you have come back?
  b : I'd like to think so.
  b[wealth=poor|destitute] : For rope? I'd have kept it, and been ashamed.

story the_jar_full [topic=smalltalk|pastimes]
  : When #jar#'s full, I've promised myself I'll stop.
  : That was three jars ago. #saying#.
  b : Ha. Get a bigger jar.
  b[friends] : You'll never stop. I know you.

story cold_night_carter [topic=travel|lore here.region=waste]
  : #carter.capitalize# came in once at midnight, blue with cold, and swore #waste# was hot.
  : I gave him soup and a blanket and he sat by the stove till dawn arguing it.
  : By sunrise he'd talked himself warm and #waste# had done the same.
  b : The Waste always wins.
  b[job=carter] : That's every carter. I've done it myself.
```

- Five stories, each gated where it must be (age, the shop, the region). `ask :` is **the teller's** question to the
  listener ("Do you keep anything...?"), then the listener's answers. (`cw102`'s writer used `ask :` for the
  *listener's* question: "Is that why you carve them?" said by the teller; so did 105 other mantles, 183 stories in
  all, until fix 8 folded them into the `b` lines. Don't: §4.5.6.)
- The unnamed carter is "he": an invented person **not in the world**, so a pronoun is fine. Real residents (slots)
  never get one.

```
rule @scales
when concept=bark working=yes self.role=merchant
does aside
cooldown 6h
say Salt, oil, rope and dates. #weighed.capitalize#. | Water skins mended while you wait. Ask at #counter#. | Fresh dates from the palms. The dark ones are the sweet ones.

rule @stones_bark
when concept=bark working=yes
does aside
cooldown 8h
say Mind #jar#. Those stones are older than the shop. | #oath#. Who moved #jar#?

rule @noon_bark
when concept=bark here.time=day here.weather=clear
does aside
cooldown 8h
say #noon.capitalize#. Shutters half down, and no shame in it. | Nobody hurries at #noon#. Not here.

rule @dusk_bark
when concept=bark here.time=dusk
does aside
cooldown 8h
say There's the cool. About time. | #soup.capitalize#, and then the roof. That's the evening.

rule @rat_bark
when concept=bark working=yes self.dislikes=rats
does aside
cooldown 12h
say Out, you whiskered robber. Out. | #ratword.capitalize# again. #annoyed#.

rule @carts_bark
when concept=bark working=yes world.wagons=late|raided
does aside
cooldown 8h
say Still no dust on #saltroad#. | Late again. #shelves.capitalize# will be bare by Restday.
```

- Six barks: the trade's cry (merchant at work), the quirk, the hour, the weather, a dislike, the world's state.
  Each `does aside`, each with a cooldown, each gated. `@scales` etc. name them. A blank line after each.

```
dlg @1 shop.greet : #greet#. Salt, oil, rope and dates, and every weight #honest#. | #greet#, #address#. Look all you like; #jar# aren't for sale.
dlg @2 shop.greet [topic.meeting=known] : #greet#, #topic.player#. Back for more rope, or for news? | #topic.player#. #shelves.capitalize# are as you left them, near enough.
dlg @3 shop.greet [here.time=day here.weather=clear] : #greet#. Out of the noon, quick. Water first, then business.
dlg @4 shop.greet [pair.disposition=hostile|wary] : What is it you want? Ask, and I'll weigh it.
dlg @1 shop.prices : Salt's cheap, it's under our feet. Everything else crossed #waste#, and #waste# charges. | #shop_dear# I don't set the carters' prices.
dlg @1 shop.bye : #farewell#. Mind the step; it's worn smooth. | #shop_bye# Come back when #carts# are in.
dlg @1 resident.hub : #greet#. Sit, if you like. What is it? | #greet#, #address#. Out of the glare first.
dlg @2 resident.hub [topic.meeting=known] : #greet#, #topic.player#. I've kept the stool warm. What's wanted?
dlg @3 resident.hub [here.time=night] : It's late. If it's #oil#, you're in luck. If it's talk, the same.
dlg @1 resident.job : I keep #self.workplace#, and I live over it. Salt, oil, rope, dates. And the news, though I don't sell that. | I keep the store. I weigh things. I listen. That's most of it.
dlg @1 resident.business : Steady. #carts.capitalize# are late more than they're early, and the salt's always here. | It pays for #soup# and a stone now and then.
dlg @1 resident.news : #news_none# #carts.capitalize# will bring some, if they come. | Nothing worth the telling. Ask #carter#; they talk for a living.
dlg @1 resident.gossip : #gossip_none# | I hear plenty. I'm not a well to be drawn on.
dlg @1 resident.bye : #farewell#. Mind the step. | #farewell#, #address#. Come back when you've news.
```

- Greetings by meeting (`topic.meeting=known` with `#topic.player#`), by the moment, and by disposition
  (`pair.disposition=hostile|wary`): the window's W1 measure counts how many hub and greeting lines are the wearer's
  own (the pilot: 99%).
- ✗✗ **The big one:** `shop.greet @1` (no `here.shop` gate), `resident.business`, `resident.news` and
  `resident.gossip` are **ungated on nodes that report facts**. They outrank the tree's lines and hide a bare shop,
  a failing business, real news and real gossip. All 42 of the pilot's `resident.news` lines had this fault (fix 8
  gated them on their plain case). Write them as §4.5.8 shows; G2 refuses them now.
- ✗ `resident.job` names `#self.workplace#` and the goods ungated: a later wearer who isn't a storekeeper says "salt,
  oil, rope, dates". Gate it `[self.role=merchant]` and keep an ungated one that fits anyone.

```
graft resident.hub
choice What's in the jar on the counter? -> stones
node stones
say Stones from the dry wadi beds. Smooth ones. Every one of them was rough once, and the water took its time.
mood pleased
free
choice Why keep them? -> stones_why
choice Goodbye. -> bye
node stones_why
say I hold one at #noon# and it stays cool. They remember the water. And I like them. Pick one, if you like.
choice I'll leave them be. -> hub
choice Goodbye. -> bye

graft shop.greet
choice Is your scale honest? -> scale
node scale
say Honest as #spring#. I set it every morning against a lump of pan-salt my father weighed against his. Watch it settle, if you doubt it.
mood pleased
choice I believe you. -> greet
choice Then I'll watch. -> scale_watch
node scale_watch
say There. Settled, and not a grain either way. Now, what are you buying?
choice Let me see your wares. -> greet
choice Goodbye. -> bye
```

- Two grafts (full tier): the quirk at the hub, the trade at the shop's greeting. Each node says something, has a
  mood where it matters, offers free text (`free`), and every choice leads back (`hub`, `greet`, `bye`). The
  player's choices are plain English.
- "What's in the jar on the counter?" assumes a counter: grafted on `resident.hub`, it shows for every wearer. Gate
  the choice: `choice What's in the jar on the counter? [self.role=merchant] -> stones`.

### 4.7 A second example: `cw102`, the turf-counter (standard tier, Coldwater dialect, accent `none`), abridged

Written for Briar Greenshaw, a Coldwater-born peat cutter living in Fenshore, who speaks with **no accent**: the
mantle is Coldwater's in content (its words, its home), but **no dialect word is written out** anywhere; the dialect
comes only through symbols (`#greet#`, `#good#`, `#tired#`...), which say plain words to an accent-`none` wearer and
Coldwater's to a later light or broad one.

```
mantle cw102
state ready
dialect coldwater
accent none
tier standard
written_for tn_oh_briar_greenshaw
title the turf-counter
fits stage=adult|old
prefers job=miner warmth=warm
traits temper=even talk=plain warmth=warm humour=none honesty=plain piety=ordinary drive=ambitious
quirk counts_the_turves_aloud_as_they_stack
likes honey_cakes draughts fresh_straw autumn_wind
dislikes haggling stale_bread
fears growing_old_on_somebody_else_s_cutting
hopes a_cutting_of_my_own
topics work +0.4 pastimes +0.3 dreams +0.3 food +0.2 newcomer +0.2 faith -0.3
styles comfort=practical gossip=reluctant refuse=soft tease=none
marks oath saying
persona #self.first# cuts peat in the fens and counts every turf onto the stack: warm and plain, saving toward a cutting of one's own, and forever whittling little birds to give away.

words
oath! = Turf and timber | Spade and splinter
saying! = A stack goes up one turf at a time | Give a thing away and your paws stay busy
turf = a turf | a good square turf
cutting = the cutting | the peat face
stack = the stack | the drying stack
tally = forty-one, forty-two | ninety-nine, a hundred
someday = a cutting of my own | my own name on a stack

any
opinion @1 houses against 2 : #oath#, the Houses. Years I've cut for them, and not one has looked up from the ledger at me.
opinion @1 church against 1 : I'll thank the Bright One in my own words. A sermon never cut #turf# or put bread on my table.

grown
line @3 greet [here.time=dawn] : Morning. I've been at #cutting# an hour already. | Morning, #address#. Up early as me.
line @1 greet.return : #greet#. | #greet#, #address#. Are you well? | #greet#. #good.capitalize# to see you.
line @1 decline : I'd like to, #address#, but I've #turf# to lift before dark. Another day. | I'll have to say no this time, #address#. I'm saving every penny.
rule @tally
when concept=bark working=yes self.job=miner
does aside
cooldown 6h
say #tally#... and on. | Thirty-six, thirty-seven. #oath#, my back.

scene cut_count
when topic=work self.job=miner
does tell.self / react.self / assess
a : Three hundred and twelve turves today. I counted every one onto #stack#. | I cut faster than anyone at #cutting# now. Not boasting. I counted. | Lifted #turf# this morning with a little black beetle asleep in it. Put it back.
b[job=miner|laborer] #dig : Your back must be in pieces. | I'll take your word. | Soft heart.
b : That's a lot of peat. | Do they pay you more for it? | That was kind.
a@dig : It is. #saying.capitalize#. | You can count me if you like. | #bc_it_is#
a : #good.capitalize# day's work. | Not yet. They will. | It wasn't doing #very# much harm.

scene home_coldwater
when topic=memories self.away=yes self.from=coldwater
does tell.self / react.self / assess
a : Where I grew up, in #self.from_town#, the wind came off the water with ice in it. Here it comes with peat smoke. | I learned to whittle on a jetty, waiting for boats. Still do it the same way. | I came here for #work#. Peat pays. Slowly, but it pays.
b[from=coldwater] #cold : I know that wind. | Same jetty, maybe. | And do you miss it?
b : Do you miss it? | Who taught you? | Will you go back?
a@cold : Then you know why I'm warm-coated. | Maybe. #good.capitalize# to think so. | Some days. Not the cold.
a : The folk, a bit. Not the cold. | Nobody. I watched an old wolf do it. | When I've #someday#. Maybe not even then.

dlg @1 resident.hub [topic.meeting=stranger] : #greet#, #address#. New here? Have you eaten? I've a bit of something.
dlg @1 resident.job [self.job=miner] : I cut peat at #cutting#. Square turves, stacked to dry, counted every one. I'll have #someday# one day.
```

What it shows (two lines above are corrected from the pilot's file: its persona said "a cutting of their own", and
its `resident.job` line was ungated, so a later wearer of another trade would say "I cut peat"):
- **Accent none, Coldwater content**: no "still water", "shore-friend" or "netted" written out; `#greet#`,
  `#address#`, `#good#`, `#very#` carry Coldwater's words to Coldwater-accented later wearers; the own words
  (`turf`, `cutting`, `tally`) are plain English and so are spoken by any wearer. Density 74%.
- **The quirk in every corner**: the oath, the saying, the `tally` word, a bark that counts, a scene that counts.
- **Away from home**: `home_coldwater` is gated `self.away=yes self.from=coldwater` and says the home town by slot
  (`#self.from_town#`); a `b[from=coldwater]` listener shares the memory.
- **The job gate**: work scenes and the job window line are gated `self.job=miner` (the game files peat cutters as
  miners), so a later wearer of another trade never says "I cut peat".
- The decline hedges with a reason in both alternatives; "Morning" is gated on dawn (the truth rules refuse
  "Morning" at other hours).

---

## 5. Writing quality

### 5.1 How the game hears you, and what that asks of the writing

Since §37 the engine chooses **what** is said (the topic, the moment, the pair, the truth checks, who has heard
what) and **a written scene says it**. Line-by-line stitching happens only at the edges: one greeting in the opener's
breath, a goodbye pair (only after talks of four lines or more), a joiner's word and its answer, a short answer to a
direct question. Three blind raters scored talk that was **one whole scene 4.11 out of 5**; talk **stitched** from
separate pieces 2.95 (§37). So:

- Put most of the mantle into scenes; make each thread a whole, natural talk that needs nothing before or after it.
- Keep edge lines short, in voice, and usable after **any** scene (a greeting can't refer to what comes next).
- A scene is heard once per listener per thread; three threads mean three different talks. Spread a topic over
  several scenes by moment (hour, weather, season, work or not) so it comes round fresh.

### 5.2 What the raters marked down, and what good sounds like

**Bad (real lines the blind raters scored lowest, §34-§37):**

| Line(s) | What went wrong | The rule |
|---|---|---|
| "Back again? Back home rich and poor drink at the same well." | an opener stacked on a line starting with the same word | one breath, one opener; never echo |
| "I've not eaten since yesterday morning." / "Two empty pots." | cryptic reply | answers say plainly what they mean |
| "Is that so. Thank you for telling me." (to wedding news) | stiff, clerkly filler; "like a clerk rather than a neighbour" | react as this wolf would: "A wedding! I'll want to hear who cried." |
| "Back so soon? We had a proper house once." / "Aye." / goodbyes | a confession met with a bare "Aye" and a goodbye | a hard thing gets a real answer, in the scene itself |
| "Up the hill, that's news. Down here it's the weather." / "Nobody's blaming you." | non-sequitur | each line answers the one before it |
| "Planks are dear in Frosthollow..." / "I'll hear about it when it hits my supper." | a reply that doesn't fit the news (food vs planks) | write the reply for *this* line, not "any news" |
| "There'll be meat on every table tonight, with the drovers in." / "In my day it was the same, and we managed." | good news answered as hardship | gate on the kind of news |
| "Doing anything tonight, love?" ... "Wish I had the money for it." | a change of subject nobody asked for, stitched | never change the topic mid-scene unless the scene is written that way |
| "She'll be tucked in a cove somewhere, the Kestrel." (as an opener) | a reply written as an opener; a ship from nowhere | an opener says what it's about |

**Good (the pilot):**
- "This is the hour I'd keep if I could keep only one." / "It is a good hour." / "Nobody wants anything from me.
  That's the joy of it." (`sw101`): a feeling, a plain agreement, a turn that reveals character.
- "Count them if you like. Sixty-one. I've stopped pretending it's a reason." / "What are they for?" / "For
  nothing. That's what I like about them." (`sw101`): dry, specific, ends closed.
- "Seven of #silver# in the first three pots this morning. Best on the river, and I'll say so to anyone."
  (`mb105`, a chatty eel-trapper): a boast in voice, with a number and a place.
- "Otters don't tie the cord back on, #pet#." (`mb105`): a reply that answers ("An otter, maybe?") and characterises.

### 5.3 Natural speech: rhythm, length, variety

- **Short turns**: one or two sentences. Mix lengths: a seven-word line, then a two-word one, then a longer one.
  Fewer than 30% of the wearer's alternatives should be three words or fewer (the lint warns over).
- **Contractions, ellipsis, fragments** as people speak: "Two days late. I've half a shelf of rope and no oil."
- **Concrete over abstract**: numbers, objects, places ("a finger short", "the second post", "the low boards").
- **Variety**: no two alternatives of a line start the same way; no two scenes in one mantle with the same shape
  (complaint / agreement / sigh) back to back; use different moves (`ask.detail / answer.detail`, `tease /
  tease.return`, `tell.worry / reassure`, `give.opinion / disagree / concede`).
- **No stock phrases**: "Thank you for telling me", "Is that so", "Indeed", "How interesting", "I see", "Good." as a
  whole reply, "Well, well", "Such is life" (that's plain's saying already). The lint warns on the worst.
- **Answer what was said**: a question gets its answer (`ask.detail` → `answer.detail` saying what was asked: who,
  where, when, why); a statement gets a reaction to *it*, not to the topic in general.
- **Disagreement, refusal, doubt** are hedged and give a reason, or run six words or more; "No." is never a
  decline (the lint refuses).
- **Closings in pairs**; a scene ends on a closing line, never on a question.

### 5.4 Whole scenes that hang together

- Read each thread alone, top to bottom, aloud in your head. If any line could be swapped with another thread's
  without anyone noticing, the threads are too generic: make each its own small story.
- A tagged branch (`b[old] #elder` / `a@elder`) must make sense in that thread too.
- **A symbol picks its alternative on its own**, independent of the thread: if a later line must answer an earlier
  one's exact words (a riddle and its answer, a number), write the words out in each thread, not through a symbol.
- No two lines running by one speaker unless the scene is written so (it rarely should be).
- A scene that needs context ("the last of this", "that one") must give it in its own first line.

### 5.5 True to role, class, House, town

- **The job shapes the talk**: a smith's heat and iron, a carter's road, a priest's care, a guard's watch. Gate it
  (`self.job=`). Merchants talk stock, prices and customers; innkeepers rooms and regulars.
- **Class shapes the register**: lower-class plain and practical; middle-class careful about money and standing;
  upper-class (House family, the Heights, royals) formal, assured, never "skint" or "toffs".
- **Ridgemere's quarters** (plain speech, no density rule, so the register *is* the voice; doc 84 §10 names the
  Heights formal, the Sump rough and the Wharf salt, and the rest are this handbook's suggestions): the **Heights** formal and
  measured ("I should think so", "one does"); the **Estate** genteel, comfortable; the **Office** clerkly, precise,
  dry; the **Commons** ordinary city speech; the **Works** blunt, tired, shift-bound; the **Sump** rough, quick,
  wary ("skint", "the Watch", "rabble", "lump"), never cruel caricature; the **Wharf** salt (ropes, cranes, tides,
  ships by name only through facts).
- **Towns**: Coldwater is the lake's town of **soap, loom and still hands, swineherds and shepherds, and hardly a
  fisher**: its idiom is the lake (still water, calm, the jetty, the mist), but nobody claims nets without
  `self.job=fisher|sailor` (truth.json refuses "my nets"; a fishing town's words need `here.fishers>=3`). Larkfield
  is a downs market town: trade, weights, bargains, sheep, looms. Southmeadow: grain, orchards, harvest, cheerful.
  Frosthollow: mines and snow, short and dark-humoured. Oakhaven: the Rainwood, slow and rooted. Read each town's
  `Data/Talk/scenes/regions/<town>.talk` head (the brief prints it).

### 5.6 Dialect by symbols, not caricature

- **Density (non-plain mantles, `--strict`): 60% of the wearer's own alternatives** (lines, dlg, barks, opinions,
  and the `a` lines of scenes and stories; the listener's `b :` lines count neither way) carry a marker: a dialect
  symbol (`#greet#`, `#farewell#`, `#oath#`, `#saying#`, `#address#`, `#very#`, `#good#`, `#bad#`, `#hot#`,
  `#cold#`, `#tired#`, `#child#`, `#mum#`, `#bc_it_is#`... the brief lists the dialect's), one of the mantle's own
  words, or (only when the accents don't include `none`) a dialect word written out ("the jetty", "mizzle").
  The pilot reached 63-98%.
- **Weave, don't pad**: put a symbol where a wolf of that place would use their word. `#address#` on every line, or
  a tic word, reads as a tic. Own words carry density and voice together.
- **Another dialect's words are refused** (a Coldwater wolf never says "dry feet"; a Saltwell one never "still
  water"). The lexicon is broad: some ordinary English counts as a dialect's ("out with it, then" is
  Southmeadow's; "who then" Mirebridge's; "time's money", "rushed off my feet", "like the cold" too). Rephrase when
  refused.
- **A broad form is never called by name**: `#greet#` says `greet.broad` to a broad wearer; `#greet.broad#` is
  refused.
- **Accent none**: no dialect word written out anywhere, own words included (§4.7).
- **Saltwell's voice is dignified, never a mock accent** (doc 83): unhurried courtesy, peace and welcome first, the
  guest honoured, proverbs, trade by measure and oath; the broad accent is *more formal* (one courtesy more), never
  broken grammar; no real-world places, peoples or religious words ("wadi" and "Baba" are its only borrowed words).
  The same respect goes for every dialect: dialect is vocabulary, idiom and rhythm, not misspelling.

### 5.7 Truth: only claim what the world makes true (§9.6; `Data/Talk/truth.json`)

1. **Opinions and feelings: freely.** "I can't abide the noon heat."
2. **Claims about the wearer need a fact behind them**: "my shop" `self.role=merchant`; "my husband" →
   `my #self.spouse_word#` gated `self.married=yes`; "my children" `self.parent=yes`; "back home" `self.away=yes`;
   "my nets" `self.job=fisher|sailor`; an age claim ("forty years", "when I was twenty") `self.age>=N`.
3. **The invented past only about people not in the world** ("my grandad", "an old carter"), never a resident.
4. **Named residents only through slots, and only for feelings, facts the game holds, or what the line itself
   records**: "#self.friend# is the one I'd trust with my keys" (a feeling), not "#self.friend# came back from the
   salt with cracked paws" (an invented deed of a real wolf).
5. **The moment**: weather words need `here.weather=`/`here.outdoors=`; "Good morning"/"Morning to you" need dawn;
   "Good night" dusk or night; season words the season.
6. **The world**: news, prices, ships, wagons and events only through their facts and gates; never a price, crew,
   culprit, cargo or date the facts don't give. "I heard", "they say" for what the wearer didn't see.
7. **truth.json catches only a few patterns** (rain, fog, snow, greetings by hour, spouse, children, nets, pity and
   gladness on news). Everything else is your job.

### 5.8 Fit for any later wearer

Read every line as if a different wolf of the same dialect wore it next: another sex, trade, family, age. Does it
stay true? If not, gate it (`self.job=`, `self.married=yes`, `self.age>=`, `self.likes=`) or rephrase it. Sex is
never gated: write sex-neutral words ("an old stick", not "an old crone"; "the young 'un", not "my lad"), and don't
use the grammars' `^m`/`^f` alternative marks in a mantle. A few lines for slots the current
wearer doesn't fill are fine (a later wearer may), gated.

### 5.9 U1: making the wearer heard (§40, §41)

The pilot's wearers said their **own** lines in only **11.6%** of their talk (U1's target is 20-35%): a wearer's
scene plays only when they open a talk on a topic their mantle covers, the rest of their talk is others' scenes
(where they say the shared `b` lines), and towns talk news half the time, where the pilot wrote few scenes. The
format can't carry a mantle's own `b` turns for others' scenes (frozen), so within it:

- **Cover more topics**: a scene for every everyday topic this wolf would talk about, 2-3 for each favourite.
- **Write news scenes the facts allow**: gossip that passes on `#topic.subject# #topic.claim#` in their voice (or,
  like `sw101`'s `gossip_held`, withholds it), gated on the claim's kind for pity or gladness; the times
  (`world.times=hard|lean`, `good|boom`), the wagons (`world.wagons=late|in|raided`), the town's events
  (`world.event_words`, `world.event=<id> world.event_when=now`), newcomers (`topic=newcomer`), festivals.
- **Strong edge lines**: many greet/greet.return/farewell/react variants (they are said around every talk).
- **Card topics** up for what they have scenes for.
- More **goodbyes of their own**: in a two-wolf room the most-said line was the dialect's stock farewell "Dry feet."
  (85 times in 870 talks, §41).

### 5.10 No collisions with neighbours

- **The town**: no oath or saying the same as another mantle's in the town; a quirk at most twice (G2 with
  `--world`; for farm residents check the index yourself, §8.8). Run the index before each mantle.
- **The dialect**: no 5-word run shared with any other mantle of the dialect, anywhere (G2 checks every line, the
  tagged ones too). Plain will hold about 600 mantles: phrase things your own way from the start.
- **The brief's words**: likes, "their words for things" and stance reasons go to every writer of the town;
  paraphrase them (the lint warns on a copied 5-word run).
- **Same place, same hour**: two wolves in one room saying the same distinctive line is a hard failure of the
  listening scorecard (R5); unique words avoid it.

---

## 6. Checking: every gate, what each failure means, and the self-review checklist

### 6.1 The gates

| Gate | What | Command | Pass |
|---|---|---|---|
| G1 | the game's loader: syntax, facts, symbols, moves, 160 characters, trees and nodes, graft paths, `does` counts | `$RATW_TALK_CHECK --data "$PWD/Data/Talk" --mantle FILE...` | "0 refused" |
| G2 | the rules, over every line (tagged scene lines too, since fix 8): names, capitals, dog/gods/tells, pronouns by slot, a persona with no pronoun, window lines on fact nodes gated on their case, truth, dialect by accent, density 60%, uniqueness (5-word runs, town oaths/sayings/quirks), habits ≤25%, marks ≥2 alternatives, tier minimum and size | `python3 tools/mantle_check.py Data/Talk/mantles-export/<dialect> --world ridgemere3 --strict --facts Saved/Mantles/residents.jsonl` | "0 refusals, 0 warnings" |
| lint | what G1/G2 miss (§6.3): the deity family, they_named, a story's `ask :` asking the teller, TODOs, header vs wearer, thread counts, bare declines; warnings | `python3 tools/mantle_batch.py lint FILE... --facts Saved/Mantles/residents.jsonl` | 0 refusals; warnings read and fixed or justified |
| G3 | coverage against real listeners of the wearer's town: core moves answered both ways, scenes heard | `$RATW_TALK_CHECK --data "$PWD/Data/Talk" --world ridgemere3 --mantles-from DIR --mantle-coverage` (DIR holds only your batch) | "0 holes in the core moves"; scenes never heard listed and explained |
| all at once | G1 + G2 (whole dialect folders) + lint + G3 | `python3 tools/mantle_batch.py gates Data/Talk/mantles-export/<dialect>/<id>.mantle ... --facts Saved/Mantles/residents.jsonl` | "G1 pass, G2 pass, lint pass, G3 pass" |
| pools | P15: the pools' health for five forecast years and the spares' honesty | `python3 tools/mantle_check.py --pools --world ridgemere3 <spare drafts>` | "8 pools: 0 not green" (exit 1 until the spares exist) |
| G6 | the round trip, after loading | `python3 tools/mantle_export.py ridgemere3 --check` | "the rows and ... agree" |
| G4 | the scorecard (`talk_card`) and ticking listens | **not yours**: the owner runs them at a slice's end (they are simulations) | |
| ctest | G1/G2 over the committed export, the tools' tests | in the main checkout after merging: `ctest --test-dir build-core -R 'mantle_export|mantle_check_rules|dialect_check|talk_lint'` | all pass |

`mantle_load` itself runs G1 and G2 `--strict` again on the batch and refuses it whole on any problem.

### 6.2 When to run what

- **Per mantle, while writing**: `mantle_batch.py gates <your file>` (6-15 s). Fix, repeat, until all pass.
  (`--no-g3` while drafting; always with G3 before you call it done.)
- **Per batch, before loading**: `mantle_batch.py gates Data/Talk/mantles-export/<dialect>/<id>.mantle ...` over
  every file of the batch together (cross-mantle collisions inside the batch show only then), then the self-review
  (§6.5), then a dry run (§7.1).
- **Per batch, after loading**: §7.4.

### 6.3 What each failure means, and the fix (real messages)

| Message (abridged) | From | Means | Fix |
|---|---|---|---|
| `#nosuchsymbol# is not in the plain grammar nor the mantle's words` | G1 | a symbol nobody defines | define it in `words`, or use a grammar's (brief) |
| `#greet.broad# is not a symbol or a fact` | G1 | a broad form called by name | write `#greet#` |
| `unknown fact 'self.colour'` | G1 | a criterion on a fact that doesn't exist | Appendix B; `Data/Talk/facts.json` |
| `'react.news.wow' is not a move (moves.json)` | G1 | a misspelt move or variant | §4.4.5 |
| `a line of more than 160 characters` | G1 | an alternative too long once expanded | shorten; split into two sentences across turns |
| `does names 2 turns' moves; it has 3` | G1 | the `does` line doesn't match the turns | one move per turn, `b?` included |
| `no window tree has the node resident.nowhere` | G1 | a `dlg` node that doesn't exist | §4.4.8 |
| `a choice leads to 'x', which is neither its node nor the tree's, bye or end` | G1 | a graft dead end | add the node or point at `hub`/`bye` |
| `topics is 'topic weight ...'`, `'TODO' is lower-case words joined by _` | G1/parser | a header left from the starting file | fill it |
| `"Varg" is the name of someone in the world` | G2 | a resident's name (or a name-word: Ash, Moss, Reed, Heron...) | a slot; or a lowercase/rephrased word (§8.9) |
| `"Xyz" is capitalised mid-sentence: a name?` | G2 | an unknown capitalised word | lowercase it; a real place of the realm missing from the allow-list: report it (§8.10) |
| `never the word dog in world text` / `never gods` | G2 | R5/R6 | reword |
| `he, she, him or her beside a person named by a slot` | G2 | R8 | no pronoun in that sentence |
| `ends its own sentence (defect 14)` | G2 | a word's value ends with `.;!?` | remove the stop |
| `takes for granted what its criteria don't say (fisher_self: self.job=fisher\|sailor)` | G2 | truth.json pattern unbacked | gate the item, or rephrase |
| `"calm water to you" is coldwater speech written out, and its wearer may speak with no accent` | G2 | accent-none mantle writes dialect | a symbol, or plain words |
| `"dry feet" is mirebridge speech, not coldwater's` | G2 | another dialect's phrase (sometimes ordinary English) | rephrase |
| `... written out in a listener's line` | G2/lint | a bare `b :` writes the dialect out | a symbol, or `b[from=<town>]` |
| `"a stack goes up one" is word for word in cw102` | G2/lint | a 5-word run shared within the dialect | rephrase yours (never the other mantle) |
| `"Turf and timber" is cw102's oath too, in the same town` | G2 | a town's habit taken | another oath |
| `quirk ...: three in one town have it` | G2 | the quirk is taken twice already | turn it |
| `4 of 12 alternatives carry a dialect marker (33%; 60% wanted)` | G2 strict | density | weave symbols and own words |
| `its habits are in N of M alternatives (...; at most 25%)` | G2 | oath/saying overused | cut to 10-15% |
| `8 items of the standard tier's 55` | G2 strict | under size | write more scenes |
| `under its tier's minimum (...): no line answering greet.return` | G2 | a core move missing | write it |
| `marks: 'oath' has 1 alternative` | G2 | a mark can't be redrawn | 2+ alternatives |
| any G2 message on a tagged line (`b[...] #tag :`, `a@tag :`) | G2 | G2 reads them since fix 8 (the lint did before) | fix as for any line |
| `"ask : Did you catch one?": ask : is the teller's question to the listener...` | lint | the listener's question under the teller's `ask` | end the `b` line after it with the question; drop the `ask` (§4.5.6) |
| `"gods": the deities are the Bright One and the Dark One` (line N) | lint | the deity family anywhere | the Bright One / the Dark One |
| `"they" (them, their) after one wolf named by a slot` | lint | R8 | repeat the slot, rephrase; if "them" means things, make that plain ("rubbing fat into the cracks") |
| `persona: "she": the persona passes to the next wearer` | G2 | R3/R8 | no pronoun |
| `a TODO is left` | lint | | finish it |
| `X speaks with accent light: the accents must include it` / `speaks coldwater, not ...` / `honesty is kept` / `the fit refuses them` / `is a pup: a young section is needed` | lint | the header doesn't fit the wearer | copy the brief's values |
| `an \`a\` line has 2 alternatives where it has 3 threads` | lint | threads misaligned | 1 or N alternatives every line |
| `"No.": a decline hedges and gives a reason` | lint | bald dispreferred answer | hedge + reason, 6+ words |
| `resident.news: its tree reports world.death, world.event_words, world.news... and this line tests none of them` | G2 | the line would hide real news (the game makes it stand aside; say the case) | gate it on the plain case the message gives, or one line a case (§4.5.8) |
| warning `a thread ends on a question nobody answers` | lint | | end on a closing line |
| warning `a claim of years needs an age gate` | lint | "forty years" for any wearer | `self.age>=N` or `old` section |
| warning `the brief's own words` | lint | collision bait | paraphrase |
| warning `a stock line the blind raters marked down` | lint | | say it as this wolf would |
| warning `N of M ... are three words or fewer` | lint | clipped | longer turns |
| warning `under the pilot's counts for its tier` | lint | thin | write more |
| G3 `core moves it can't answer` / `owed it unanswered` | G3 | a hole: a move with no answer for some listener | add the missing line (e.g. `accept_preclose`, `accept_decline`), or a bare variant of a gated one |
| G3 `never heard: cw101.grown.scene.storm_broke` | G3 | no listener/moment fits it in the save now | fine if it waits on the world (a storm, hard times); otherwise its gates are wrong (a typo, an impossible combination) |

### 6.4 What no check catches (your eyes only)

Non-sequiturs inside a thread; cryptic replies; stiffness; a scene whose threads are interchangeable; an invented deed
of a real resident; a claim true only of this wearer but not caught by truth.json (a trade noun, a family member, a
house, an age); `ask :` used for the listener's question in a story, in a form the lint doesn't know; sex-marked words ("crone", "lad", "lass")
about the wearer; caricatured dialect; the same scene shape repeated; news words with no gate ("the carts are
late" on any caravan topic); a graft choice that assumes a shop or trade without a gate.

### 6.5 The self-review checklist (tick every box for every mantle)

```
[ ] Header: id from batch.tsv; state ready; dialect, accents, tier, written_for as the brief; fits by stage only;
    all seven traits, honesty unchanged; quirk not a third in the town; likes/dislikes ids; fears, hopes, topics,
    styles filled; marks name 2-4 habits with 2+ alternatives each; persona one line, #self.first#, no pronoun.
[ ] Words: oath and saying unique in the town (index), no value ends with a stop, own words are theirs.
[ ] Sizes: >= 92 / 55 / 64 items; the pilot's counts per part (lint has no "under the pilot's counts" warning).
[ ] Every scene: 3-5 turns; does = turns; 2-3 threads, each line 1 or N alternatives; each thread read alone is one
    natural talk; ends closed; a bare b : in every scene; tagged branches make sense.
[ ] Every scene, line and story gated for its moment and its claims (weather, hour, season, place, job, family, age,
    likes, world state); nothing true only of this wearer left ungated.
[ ] No name, no "dog", no "god(s)", no "tells" for messages, no he/she/they beside a slot, no sex-marked word about
    the wearer, no invented deed of a real resident, no invented price/crew/culprit/cargo.
[ ] Dialect: density >= 60% (non-plain) by weaving, not padding; accent none writes nothing out; bare b : lines write
    nothing out; no other dialect's phrase. Plain: the quarter's register.
[ ] Lines: the core moves (greet.return, farewell.return, react.news), openers by frame as statements, closings in
    pairs, hedged declines with reasons, pity/gladness gated on the news kind.
[ ] Stories: 5 / 3, ask : is the teller's question, at least one b line, invented past gated by age and about
    people not in the world.
[ ] Barks: 6 / 4, does aside, cooldowns, gated.
[ ] Window: hub greetings by meeting, hour and disposition; job; bye; data nodes (news, gossip, business, shop
    greet, watch, church, inn) gated per case; role nodes for full tier; graft(s) with ways back, choices gated if
    they assume a trade.
[ ] Read aloud, whole: one wolf, unlike any other in the town; a neighbour would know them by ear.
[ ] mantle_batch.py gates <file>: G1 pass, G2 pass, lint pass (warnings fixed or noted), G3 pass.
```

---

## 7. Loading and binding into DEV, exporting, committing, and what not to do

### 7.1 Before loading

All of the batch's files pass `mantle_batch.py gates` together, and every self-review box is ticked. Then a dry
run (checks everything, writes nothing):

```
cd .claude/worktrees/mantles-$B
export RATW_TALK_CHECK=/home/martinb/Documents/GitHub/MUDProject/build-core/talk_check
python3 tools/mantle_load.py ridgemere3 $(cut -f12 Saved/Mantles/$B/batch.tsv | tail -n +2) --batch $B --written-by <you> --bind --dry-run
# each mantle: "cw103: {'revision': None, 'dry_run': True, 'items': 64}"; any problem is listed by file, mantle, item
```

### 7.2 Back up

`mantle_load` dumps every row it will change to `Saved/Backups/mantles-ridgemere3-<batch>-<stamp>.jsonl` by itself.
Take a whole backup too, once per session:

```
python3 tools/mantle_load.py backup ridgemere3        # every mantle, item, history row and binding -> Saved/Backups/
```

(`python3 tools/mantle_load.py restore FILE` puts a backup back. Never restore without the owner's say-so.)

### 7.3 Load and bind

```
python3 tools/mantle_load.py ridgemere3 $(cut -f12 Saved/Mantles/$B/batch.tsv | tail -n +2) --batch $B --written-by <you> --bind
echo $?                                   # 0 done; 1 refused, nothing written; 2 written, a bind failed
```

It (§17.4): checks every file (G1, G2 `--strict`; one problem refuses the whole batch), compares revisions, backs
up, writes in one transaction as Atlas's role (history source `mantle_load:<batch>`; `written_by` is
`--written-by WHO`: pass your own provenance, e.g. `--written-by codex` or `--written-by claude-opus`, else it reads
`claude-<batch>`; to fix someone else's batch keep theirs: `--written-by claude-codex-cw-1`), binds
each mantle to its `written_for` resident (`bound_how authored`; refused if they're dead, wear another, or don't fit),
queues one `voice.sync` for the running DEV server (expected; it is not a restart), and **rewrites your files in
canonical form** (every item named `@1`, `@2`..., a `revision 1` header). Read its output: lines `cwNNN: {...}`,
`bound cwNNN -> <npc>`, `backup: ...`. It exits 0 when all is done, 1 when the batch is refused (nothing written),
and **2 when it wrote but a mantle wasn't bound** (`cwNNN not bound: <why>`, then `N loaded but not bound: ...`; before
fix 8 that exited 0).
Loading the same files again changes nothing.

### 7.4 Verify, export, round trip

```
python3 tools/mantle_load.py check ridgemere3                  # "N placed residents wear no mantle (of 773); M mantles, M worn" and nothing else
python3 tools/mantle_export.py ridgemere3                      # rewrites every mantle file and index.tsv from the rows (your worktree)
python3 tools/mantle_export.py ridgemere3 --check               # "the rows and Data/Talk/mantles-export agree" (G6)
$RATW_TALK_CHECK --data "$PWD/Data/Talk" --mantle Data/Talk/mantles-export   # G1 over everything: 0 refused
python3 tools/mantle_check.py Data/Talk/mantles-export --world ridgemere3 --strict --facts Saved/Mantles/residents.jsonl
$RATW_TALK_CHECK --data "$PWD/Data/Talk" --world ridgemere3 --mantle-coverage $(cut -f1 Saved/Mantles/$B/batch.tsv | tail -n +2)   # G3 on the rows
git status --short Data/Talk/mantles-export                     # only your batch's files (+ index.tsv) should show
```

If `mantle_export` (without `--check`) changed files of **other** mantles, someone edited them in Atlas or the DM
since the last export: that's expected, commit them with yours and say so in the report ("N items in M mantles
changed in Atlas/the DM since the last export"). Your `G2` over the whole export must still show 0 refusals; if a
pilot mantle fails, report it, don't fix it.

### 7.5 Commit, merge, push

```
git add Data/Talk/mantles-export/<dialect>/<id>.mantle ... Data/Talk/mantles-export/index.tsv artifacts/talk/p14/$B/
git diff --cached --stat                  # only those
git commit                                # the message of §2.6
# then from the main checkout: §2.6's merge and `git push --no-thin origin ridgemere2`
# then, in the main checkout:
ctest --test-dir build-core -R 'mantle_export|mantle_check_rules|dialect_check|talk_lint'
```

### 7.6 Fixing a mantle after it is loaded

Edit its file (keep the `revision N` header and every `@` key), run the gates, and load it again with the same
command: changed items are updated, the revision rises, the file is rewritten. If someone changed it in Atlas or the
DM since (the rows moved on), the load refuses that mantle: export it again (`mantle_export.py ridgemere3 --mantle
<id>`), redo your edit on the new text, and load. `--take-mine <id>` / `--take-theirs <id>` exist; use them only
when you understand which edit to lose. Items you delete from a file stay in the rows unless `--prune`. Never
rename an item's key.

### 7.7 What NOT to do

- **Don't edit the engine or the tools**: nothing under `Core/`, `Client/`, `Editor/`, `Tests/` (C++), CMake files,
  `Database/migrations/`, `tools/*.py`, `tools/*.json`
  (allow-lists: report a missing place instead), `Data/Talk/*.json`, `Data/Talk/grammar/`, `Data/Talk/dialogue/`,
  the library's `.talk` files (except the kits you write in P15), `Data/Talk/names/` (and `Data/Talk/grammar/`
  only for P15's mark banks, `mark.<symbol>` lines, §3.6).
- **Don't change the format** (§4.4): no new header fields, sections, item kinds or syntax.
- **Don't touch PROD** (`prod`, `ratw_prod`) or the other worlds (`dev` = `ratw_ridgemere2`, `dev-old`); no
  `world_db.py migrate/use/serve-world/new/down`, no hand-written SQL, no `psql` writes, no `world_sync.py` imports,
  pulls or pushes, no `mantle.assign`/`release`/`retire` DM acts.
- **Don't run simulations**: no economy runs (`world_check --simulate`, `econ_watch`, `game_load`), no ticking talk
  listens (`talk_sim --tick`), no `talk_card`/scorecard runs, no `--run HOURS`. The owner runs G4 and listening at a
  slice's end.
- **No paid model calls**: no `tools/voice_tables.py`, `tools/live_npc_smoke.py`, nothing that calls an AI API or the
  NPC Mind; don't start servers with the live AI.
- **Don't restart, stop or start the server on port 7788**, and don't start the real server (`tools/live.sh`,
  `tools/server.sh`). If you must look at the game, use a scratch server (`bash tools/scratch.sh dev` in the
  background; kill it when done); you shouldn't need to.
- **Don't edit the 43 pilot mantles** or anyone else's batch: report their problems (§8.11).
- **Don't bind, release or retire by hand**; don't load drafts that fail a gate with `--lenient`.
- **Don't commit** `Saved/`, `Database/.env`, `artifacts/economy/`, `.claude/`, or anything not yours; no
  `git add -A`/`.`, no force-push, no rebase of `ridgemere2`.
- **Don't write mantles for the dead** (`todo` excludes them) or for the Country's 310 unplaced (they take spares
  when they arrive).

---

## 8. Known data problems, and how to handle each

The pilot's writers met most of these (§40, §41); the rest were found while writing this handbook (2026-11-17).
None is yours to fix in the data: handle it in the writing, and list what you met in the report.

1. **Workplace slots that are a district, not a place** (127 residents): `#self.workplace#` = "Ridgemere, the Wharf
   and the Sump" (35), "Ridgemere, the Commons and the Rise" (20), "The Rainwood, North-east"... "I'll be at
   #self.workplace# till dark" reads badly. For such wearers don't put `#self.workplace#` in a line that needs a
   building; use own words for the work ("the cranes", "the stairs") gated on the job, or `#self.trade_words#`
   where it reads well ("carrying from Sump Stairs").
2. **Friend slots that are kin** (199 residents: `#self.friend#` is their spouse, parent or child). Write friend
   lines that stay true if the friend is kin ("#self.friend# and I go back years", "I'd trust #self.friend# with my
   keys"); never "not family, but..." or a line that contrasts friend and kin.
3. **`job none` for real work** (149 residents: loom hands, soap and still hands, swineherds, cowherds, apple
   pickers, butter churners, cranes, porters, ushers, royals "reigning", the Queen...; and §40's fowler, pot hands,
   peat cutters, vine and cellar hands). Work can't be gated on the job. Gate work scenes `self.working=yes` (and
   `self.job=none` if the words would be wrong for a smith), say the trade through `#self.trade_words#` /
   `#self.workplace#`, and keep trade nouns ("the shuttle", "the vats") out of ungated lines, or accept that any
   later job-none wearer says them and keep them few. Peat cutters read as `job=miner`; skippers, deckhands and
   boatmen `sailor`; warreners `herder` (§41).
4. **Backstories and likes that don't fit the place**: Saltwell residents with fen backstories ("the best eel
   trapper on the river", marsh lights), likes like "first snow" or "deep mud" in the Waste. The place wins: never
   write fen life for a Waste wolf; keep such a like on the card if harmless, but write scenes only where it can be
   true (gate `here.season=winter` / `here.region=`), or leave it out of scenes.
5. **The dialect lexicon flags ordinary English** ("rushed off my feet", "time's money", "who is it, then?", "like
   the cold", "out with it, then", "who then", "truly said", "the alms-bread"). Rephrase; list what you hit in the
   report. Don't edit `tools/dialect_allow.json`.
6. **Kin in the Country**: kin marked "(in the Country, not in a town: no slot names them)" can't be named, and the
   wearer may be `parent no` though they have children there (Jory Rimepeak). Don't write "my children" unless
   `self.parent=yes`; talk of them only as feelings with no claim ("the young ones have their own roads now", gated
   on `self.age>=45`, if at all).
7. **Odd regions**: Ada Coalstack's hill home is in region `ridgemere`; farm homes (`fs_...`, about 60 residents)
   sit in their town's region. Use the brief's `here.region` value, whatever the home's name says.
8. **Farm residents escape G2's town checks**: `mantle_check` finds a wearer's town from the home id (`cn_<town>_`,
   `rm_`), so farm folk's oaths, sayings and quirks aren't compared with their town's. `mantle_batch` and
   `mantle_index` use the home's region: check a farm resident's oath and saying against the index by eye.
9. **Residents' names that are ordinary words**: Abbot, Alder, Ash, Birch, Bramble, Briar, Clover, Dawn, Elm, Ember,
   Fern, Flint, Frost, Garth, Hazel, Heath, Heather, Hob, Holly, Holt, Iris, Ivy, Juniper, Kite, Lark, Linden,
   Loam, Marl, Moss, Opal, Pike, Reed, Ridge, Rook, Rowan, Rue, Sage, Slate, Sloe, Sorrel, Stone, Tansy, Tarn,
   Thistle, Thorn, Vale, Wainwright, Wick, Wren, Wynn, Yarrow (and more as people are born). G2 refuses such a word
   **capitalised** mid-sentence, or at a sentence's start when a comma, another name or a word of telling follows
   it ("Ash, cut green." is refused; "Frost on the jetty." passes). Write them lowercase mid-sentence ("cut green
   ash", "the first frost") or rephrase.
10. **Places missing from the allow-list**: G2 allows the realm's towns, Houses, weekdays, deities and the pilot
    towns' places; a real place of Coldwater, Larkfield or Ridgemere (an inn, a street, a hill) may be refused as
    "capitalised mid-sentence". Use `#here.place_name#`/`#self.workplace#`/`#self.street#`, a lowercase description
    ("the inn by the jetty"), or list it in the report for the owner to add to `tools/mantle_rules.json`.
11. **The pilot's own faults** (don't copy). Fixed by fix 8 (2026-11-18) in all 145 mantles then written, so G2 and
    the lint over the whole export show 0 refusals; what they were, for the record. On 2026-11-17 the lint over the 43
    found 160 refusals and 163 warnings: 15 personas with a pronoun; 84 window lines on fact-reporting nodes left ungated (42
    `resident.news`, 14 `resident.gossip`, 8 `resident.business`, 12 `shop.greet`, 5 `inn.greet`, 3 `inn.rest`),
    which hide real facts in the window; 52 problems in tagged scene lines G2 never saw (5-word runs, a Southmeadow
    phrase in `mb120`, listener lines writing dialect out, "Ash"); 8 "they" after a slot; 1 bare doubt; 22 age
    claims ungated; 121 brief phrases copied. By eye: `cw102`'s stories use `ask :` for the listener's question,
    `sw101`'s "old crone", a few invented deeds of slot people. Fix 8 fixed every refusal (the 84 window lines gated on
    their case, 4 news alternatives that said `#world.event_words#` moved to lines of their own; the 15 personas; the
    52 tagged-line problems and the 8 "they" and the bare doubt rephrased; 3 mantles' density lifted after G2 began
    counting tagged lines; 183 misplaced `ask :` lines in 106 mantles, the pilot's and cw-1/cw-2/lf-1's, folded into the
    listener's). The warnings (age claims, brief phrases...) stand: read them in your own batch.
12. **Fixed (fix 8): G2 reads tagged lines** (`b[...] #tag :`, `a@tag :`); before, its line reader skipped them and
    only the lint saw them. `mantle_load` refuses them now too.
13. **Fixed (fix 8): `mantle_check --pools`** is built (§3.6).
14. **Fixed (fix 8): mark banks can be written** (`mark.<symbol>` in a dialect's grammar): §3.6.
15. **A pup born in the game with no mantle**: `born_1grqt7uo` (Juniper Coldmere, Coldwater, young). He is on the
    list: write him a pup's mantle in the Coldwater batch (§3.5). More births will appear; `todo` shows them.
16. **`#self.late#` and `#self.rival#` are empty for every resident today** (nobody mourned yet; no rivals in the
    save): lines with them are never heard now. One gated scene for each at most, for later wearers.
17. **Iris Greywater (Fenshore's potter) is dead**; the dead are never written for. Some pilot scene names mention
    residents (`friend_iris`): keys aren't spoken, but don't name keys after residents.
18. **Fixed (fix 8): `mantle_load` exits 2 when a bind fails** (it prints `<id> not bound: <why>`) (§7.3).
19. **Fixed (fix 8): `written_by` is `--written-by WHO`** (default `claude-<batch>`) (§7.3).
20. **Talk in Coldwater about fishing**: Coldwater's homes hold "hardly a fisher"; its grammar is the lake's. Use the
    lake's idiom, never fishing claims without `self.job=fisher|sailor` (and a fishing town's words need
    `here.fishers>=3`).

---

## 9. The batch report template

Write it to `artifacts/talk/p14/<batch>/report.md` (P15: `artifacts/talk/p15/<batch>/report.md`) with a
`writing-log.tsv` beside it, and commit both with the batch (§7.5). Fill every field; "none" is an answer.

````
# Mantle batch <name>: <town / quarter> (<date>)

Writer(s): <agent and model; how many in parallel>    Branch: r3-mantles-<name>    Commit: <hash>    Merge: <hash>
World: ridgemere3 at game day <day> (from `python3 tools/mantle_pool.py forecast ridgemere3 | head -1`)

## What was written
| id | resident | name | dialect | accents | tier | stage | title | items | scenes | stories | opinions | lines | barks | dlg | grafts | symbols | alternatives |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| cw103 | tn_fh_jory_rimepeak | Jory Rimepeak | coldwater | light broad | standard | adult | the name-keeper | 64 | 20 | 3 | 6 | 22 | 4 | 7 | 1 | 14 | 290 |
| ... |
Totals: <N> mantles (<full> full, <standard> standard, <pups> pups); <items> items, <alternatives> alternatives,
about <words> words. Residents left after this batch: <`todo` count>.

## Gates (paste the summary lines)
- gates (per batch, before loading): G1 <...>; G2 <"N mantles: 0 refusals, 0 warnings">; lint <"0 refusals, W warnings">;
  G3 <"N mantles worn; X of Y scenes heard; 0 holes in the core moves; W1 ...%">
- Scenes never heard in G3, and why each waits on the world (or was fixed): <list>
- Lint warnings left, with why each is acceptable: <list or none>
- mantle_load: <"N written, N bound", backup path>; `mantle_load check`: <line>
- mantle_export --check: <"agree">; G1 and G2 over the whole export: <lines>
- ctest in the main checkout after merge: <passed / failed which>

## Voices (one line each: what makes this wolf unlike any other in the town)
- cw103 the name-keeper: dry, quiet; remembers every name and says none; oath "..."; wants to leave for Ridgemere ...

## Decisions and deviations
- Quirks turned (three in a town): <id: from -> to>
- Traits refined (talk-only axes): <id: axis from -> to, why>
- Tiers, fits or dialects differing from the brief, and why: <none>

## Data problems met (§8 numbers, and new ones)
- <id>: <problem>, <how handled>

## Collisions resolved
- <id> vs <id>: <the 5-word run / oath>, <changed in which>

## For review (pick 3-5 mantles a reviewer should read whole, and why)
- <id>: <e.g. the first House family member; a pup; an accent-none mantle; a guard's window lines>

## Open questions for the owner
- <e.g. a place to add to the allow-list; a resident whose facts look wrong>

## Time
Started <time>, finished <time>; writing <h>, gates and fixes <h>; tokens if known.
````

`writing-log.tsv` (the pilot's format, one row a mantle):

```
mantle	writer	started	finished	items	alternatives	note
cw103	W1	2026-11-18T09:02:17-07:00	2026-11-18T09:14:48-07:00	64	290	loom hand, job none: work gated self.working=yes, trade by slot; density 71%; one 5-word run with cw102 fixed
```

---

## Appendix A: command cheat sheet

```
# once per session (worktree root)
export RATW_TALK_CHECK=/home/martinb/Documents/GitHub/MUDProject/build-core/talk_check
python3 tools/mantle_batch.py facts --out Saved/Mantles/residents.jsonl
F=Saved/Mantles/residents.jsonl

# what's left, and plan a batch
python3 tools/mantle_batch.py todo --facts $F --town coldwater
python3 tools/mantle_batch.py plan --name cw-1 --town coldwater --size 33 --facts $F --out Saved/Mantles/cw-1 --dir Saved/Mantles

# per resident
python3 tools/mantle_brief.py ridgemere3 <npc_id> --facts $F
python3 tools/mantle_index.py <town> --world ridgemere3
cp Saved/Mantles/cw-1/start/cw103.mantle Data/Talk/mantles-export/coldwater/
python3 tools/mantle_batch.py gates Data/Talk/mantles-export/coldwater/cw103.mantle --facts $F

# the pieces, by hand
$RATW_TALK_CHECK --data "$PWD/Data/Talk" --mantle Data/Talk/mantles-export/coldwater/cw103.mantle
python3 tools/mantle_check.py Data/Talk/mantles-export/coldwater --world ridgemere3 --strict --facts $F
python3 tools/mantle_batch.py lint Data/Talk/mantles-export/coldwater/cw103.mantle --facts $F
$RATW_TALK_CHECK --data "$PWD/Data/Talk" --world ridgemere3 --mantles-from <dir with only the batch> --mantle-coverage

# load (after all gates pass)
python3 tools/mantle_load.py backup ridgemere3
python3 tools/mantle_load.py ridgemere3 <files> --batch cw-1 --written-by <you> --bind --dry-run
python3 tools/mantle_load.py ridgemere3 <files> --batch cw-1 --written-by <you> --bind   # exit 2: a bind failed
python3 tools/mantle_load.py check ridgemere3
python3 tools/mantle_export.py ridgemere3 && python3 tools/mantle_export.py ridgemere3 --check

# spares (P15)
python3 tools/mantle_batch.py pools Data/Talk/mantles-export --facts $F
python3 tools/mantle_check.py --pools --world ridgemere3 <spare drafts>      # P15's gate
python3 tools/mantle_pool.py forecast ridgemere3
```

(Every read-only command above was run while writing this handbook, on 2026-11-17, against DEV and the pilot's
files: `facts`, `todo`, `plan`, `lint`, `gates`, `pools`, `mantle_brief`, `mantle_index`, `talk_check --mantle`,
`--mantle-coverage` with and without `--mantles-from`, `mantle_check --strict` (files and `--one`),
`mantle_load check`, `mantle_load --dry-run` (from the main checkout and from a worktree), `mantle_export --check`,
`mantle_pool forecast`/`names`, `dialect_check`, `talk_lint`, `talk_migrate --check`, the ctests named. The writing
commands (`mantle_load` without `--dry-run`, `backup`, `mantle_export` without `--check`) were not run: they write.)

## Appendix B: the facts a line can test or say (`Data/Talk/facts.json`)

- **The wearer** (`self.` in lines and `when`; `a.` in scenes): id, name, first, surname, sex, age, stage
  (young/adolescent/adult/old), job (merchant, smith, baker, brewer, innkeeper, guard, soldier, farmer, fisher,
  miner, healer, priest, scholar, laborer, carter, crafter, servant, sailor, herder, woodcutter, cook, official,
  noble, child, beggar, none...), role (merchant/guard/civilian), trade, wealth (destitute/poor/comfortable/rich),
  class (upper/middle/lower), business (thriving/steady/failing/none), hungry, homeless, injured, tired, mourning,
  robbed, owes, on_dole, working, shift, on_watch, travelling, married, widowed, parent, master, apprentice, house,
  from (town grown up in), accent, quarter, away, dialect, quirk, mantle, likes/dislikes/fears/hopes (list facts:
  `self.likes=old_tales`), mood (low/sore/bright/even), the seven traits, and the slots (§4.4.9).
- **The listener** (`other.` / `b.`, and bare inside `b[...]`): the same facts.
- **The pair** (`pair.`): band (strangers, acquaintances, friends, rivals, family, spouses, apprentice, housemates),
  kin (spouses, parent, siblings, family, household, no), named, disposition (hostile, wary, cool, neutral, warm,
  fond), boss (a/b/no), richer, same_from, neighbours, echo, echo_of, affinity, trust, familiarity...
- **The place and moment** (`here.`): region, place (street, market, tavern, home, work, chapel, barracks, gate,
  field, shore, wild, hall), time (dawn, day, dusk, night), season, day (work, market, rest, festival), weather
  (clear, rain, fog, snow, overcast, storm, sandstorm, indoors), outdoors, town, place_name, weekday, festival,
  crowd, fishers, and the window's facts (shop, wanted, fine, gossip, inn_helper, lodging, bed_night, ...).
- **The world** (`world.`): bandits (near/no), crime/death/wedding/birth/arrest (recent/no), festival (today, soon,
  no), mood (grief, uneasy, festive, edgy), news, news_from, harvest, ships, wagons (in, late, raided, no), times
  (hard, lean, ordinary, good, boom), event, event_words, event_when, `<good>_price` (up/down/steady).
- **The topic** (`topic.`): subject, claim, claim_kind (death, marriage, violence, theft, victim, wanted, bandit,
  promise_kept, promise_broken, prowling, newcomer, other), kind, good, item, price, dir, ship, victim, who, where,
  why, player, meeting (stranger/known/returning/famous)... and pronouns `topic.they/them/their`.
- **The conversation** (`conv.`): move, prev, tone, topic, issue, and any `{k=v}` tag.

The full schema, with every value, is `Data/Talk/facts.json`; G1 refuses anything not in it.
