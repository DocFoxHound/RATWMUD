# AGENTS.md: start here

You are picking up **mantle writing** for *Run Against the World* (a wolf MUD). A mantle is one NPC's own name-free
dialogue set, in its region's dialect. Your job is to **write mantle files** for the Ridgemere city residents who still
have none, from briefs that are already prepared, and push them on a branch. Nothing else.

## 1. Where you are

- **Branch to start from: `ridgemere2`.** That is only the branch's old name. The world we are working on is
  **Ridgemere 3** (world key `ridgemere3`, database `ratw_ridgemere3`). Everything below is Ridgemere 3.
- **Read these, in order, before writing anything:**
  1. `Docs/Handoff-living-conversations.md`: the note at the top and section 9 (what is done and what is left).
  2. `Docs/Mantle-writing-handbook.md`: **the rules and the exact file format. Follow it exactly.**
  3. Good models: existing mantles in `Data/Talk/mantles-export/plain/` (Ridgemere's plain speech), e.g. `rm111`-`rm184`
     (the city batches city-a1 and city-a2), and the handbook's annotated `sw101`.
  4. `artifacts/talk/p14/city-work/CITY-BRIEF.md`: the city coordinator's notes (quarters, voice, what to avoid).

## 2. What you can and cannot do in a cloud environment

You only have this repository. There is **no database, no running game, no `Database/.env`**.

- **You do:** write `.mantle` files; run the file-only checks (section 5); commit and push your branch.
- **You do not:** load or bind mantles into a database (`tools/mantle_load.py`), run `mantle_export.py`, run G3
  (`--mantle-coverage` against a world), run any simulation (`talk_sim`, `talk_card`), start a server, or merge into
  `ridgemere2`. Those happen afterwards on the owner's machine, batch by batch.
- Never touch `ratw_prod` (PROD) or any database at all. Don't edit the engine (`Core/`), the tools, or other mantles.

## 3. The work list (already planned; do the batches in this order)

All 395 remaining residents are planned in `artifacts/talk/p14/city-work/`, one folder per batch:

| Order | Batch | Residents | Notes |
|---|---|---|---|
| 1 | `city-a3` | 36 | **Started:** `city-a3/drafts/` has drafts for 18 of them: rm185, rm186, rm189, rm194, rm206 look complete (still run every check); rm187, rm192, rm193, rm195, rm197 are partial; the rest are untouched starting files. Finish them (move each to its export path), then the other 18. |
| 2-4 | `city-a4`, `city-a5`, `city-a6` | 42, 42, 41 | |
| 5-11 | `city-b1` … `city-b7` | 34 each, last 30 | |

Each batch folder has:
- `batch.tsv`: one row per resident: **mantle id**, npc id, name, dialect (`plain`), tier (`full`/`standard`), accents,
  age stage, town, quarter, rank, work, and **the export path to write to** (`Data/Talk/mantles-export/plain/rmNNN.mantle`).
- `briefs/<id>.md`: everything about that resident (role, caste, House, family, stage, sex, workplace, temperament,
  honesty, what the brief's reasons and likes are) plus the sizes to hit.
- `start/<id>.mantle`: a starting file with the header filled in. Copy it to its export path and write there. **Every
  `TODO` must be gone.**
- `index-ridgemere.md`: who else lives nearby and which voices are taken (avoid collisions).

Resident facts for the checks: `artifacts/talk/p14/city-work/residents.jsonl` (pass it as `--facts`).

## 4. The rules (the owner's, non-negotiable; full text in the handbook)

- Every mantle **unique**, in the wolf's region's dialect (Ridgemere = plain speech; make the city's quarters distinct:
  Heights, Commons and the Rise, Northgate, the Wharf, the Sump, the Works, the Office, the Estate).
- **Name-free:** use the slots (`#self.first#`, `#self.spouse#`, `#self.friend#`, `#self.workplace#`… see the
  handbook) so a mantle can pass to a newborn after its wearer dies.
- **Deities:** only "the Bright One" (benevolent, God-like) and "the Dark One" (malevolent, Satan-like). Never "god",
  "gods", "godsend" or any other deity word.
- They are **wolves**: never "dog"/"dogs" (young are "pups"). Say "private messages", never "tells".
- Characters are **male or female only**; **pronouns follow sex** (he/him, she/her), never "they" for one known wolf.
  Personas carry no pronoun (a mantle can pass to either sex).
- Whole scenes that hang together: each line answers the one before. Blind raters punished non sequiturs, cryptic
  one-word replies, stiff stock lines ("Thank you for telling me"), greetings that ask a question.
- Don't invent deeds of real residents; don't copy phrases from other mantles or the handbook's model; no doubled words
  ("a a copper"); don't write "I'm #self.trade_words#".
- Graft nodes must not use the reserved dialogue-tree node names (Docs/Design/84-living-conversations.md §45).
- The **richer** size: each tier's item counts as the brief says.

## 5. Checks you run (file-only), per mantle and per batch

Use `F=artifacts/talk/p14/city-work/residents.jsonl`.

```
python3 tools/mantle_check.py --strict --facts $F Data/Talk/mantles-export/plain/rmNNN.mantle   # G2: 0 refusals
python3 tools/mantle_batch.py lint --facts $F Data/Talk/mantles-export/plain/rmNNN.mantle        # 0 refusals
python3 tools/dialect_check.py                                                                     # 0 failures
python3 tools/talk_lint.py                                                                         # "0 new"
```

G1 (the game's own loader) needs the C++ build: `cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release && cmake --build
build-core --target talk_check -j8`, then `build-core/talk_check --mantle Data/Talk/mantles-export/plain/rmNNN.mantle`
(0 refused). If the build can't be done in your environment, say so in the report; the owner runs G1.
A warning about `self.trade_words` "never heard from them" is acceptable (kept for a later wearer); refusals are not.

## 6. Committing

- One branch per batch, from `ridgemere2`: `cloud/mantles-<batch>` (e.g. `cloud/mantles-city-a3`).
- Commit only your mantle files and the batch report, with explicit paths (never `git add -A`).
- Batch report: `artifacts/talk/p14/<batch>/report.md`, using the handbook's report template (section 9): mantles
  written, tier, items, checks run and results, anything left, data problems found.
- Push the branch. **Do not merge** into `ridgemere2`; the owner loads the batch into the world (G3, export check)
  and merges it.
- Commit messages: plain English; end with your own attribution line.

## 7. If something doesn't fit

If a brief's facts look wrong (a workplace that is a district, a "friend" who is kin, a backstory from another town),
write the mantle so it stays true for any later wearer, and list the problem in the batch report. Don't edit
`residents.jsonl` or the briefs.
