# Cast survey: Ridgemere 3 (`ratw_ridgemere3`), read only, 2026-11-22

Nothing was written. All queries ran inside `SET TRANSACTION READ ONLY` and were rolled back.

## 0. Server and access
- **The game server is up.** `build-core/ratw_server --port 7788 --database ridgemere3 --world-kind dev` (pid 12304) has been running for about 3 hours under `Saved/WorldAgent/run.sh` / `tools/world_agent.py`, linked to central. `curl 127.0.0.1:7788/` answers (404 on `/`). The last checkpoint was saved at 20:15 UTC today. Game day is about 65.7 (`calendarDays`); a year is 365 days.
- **How to connect.** `psql` as martinb fails. Use the project helper:
  ```python
  import sys; sys.path.insert(0,'tools'); import world_db
  conn = world_db.connect('ridgemere3', 'owner', autocommit=False)   # the world KEY, not the db name
  cur = conn.cursor(); cur.execute('SET TRANSACTION READ ONLY')
  ```
  The settings come from `Database/.env` (127.0.0.1:5433). The worlds are listed in `Database/worlds.json`.
- **The world id inside the database is `upper_accord`**, not `ridgemere3`. Every `live.*` row has `world_id='upper_accord'`. `world.worlds` has id `upper_accord`, named "Ridgemere 3.0".

## 1. Where residents live (tables)

| Table | What it holds |
|---|---|
| `live.npcs` (1086 rows) | Authored residents: id, name, role (civilian/merchant/guard), work_label, age, appearance (jsonb, with `sex`), personality, backstory, home/work/evening area and x/y, purse, `protected` (all false), `joinable` (all false), band_override. |
| `live.npc_voice` (1086) | home_region (the town where they **grew up**, which sets the dialect), accent, quarter (city register: heights, commons, sump, office, works, wharf, estate), traits jsonb (talk, temper, warmth, humour, honesty, piety, drive), quirk, and the mantle binding (mantle_id, bound_day, bound_how, released_day). |
| `live.npc_state` (777) | alive flag, plus a jsonb with cell, x, y, band, task, hunger. |
| `game.npcs` (777) | The runtime body: lifeStage, age, cell, stats, activity. |
| `live.npc_relations` (333) | parent (278) and spouse (55). |
| `game.bonds` (25,399) | trust, affinity, familiarity, respect, fear and owed, per pair. |
| `game.beliefs` (11,617) | Gossip: kind, claim, subject, object, confidence, source. Kinds: theft.attempt 6280, marriage 1698, bandits.camp 1503, warrant 1001, theft 790, newcomer 296, death 32, assault 17. |
| `game.npc_memories` (64) | Records of conversations with players. |
| `game.events` | A log of event kinds: death, old age, succession, vacancy, apprenticeship, birth, theft, warrant… |
| `game.checkpoints.payload` (4.3 MB JSON) | `society.careers.positions` (who holds each job **now**), spouses (348), parents (118), mantles (the ledger), skill. Also `crime`, `factions`, `talkNews`. |
| `dm.story_residents` (view) | What the story validator checks resident ids against: `live.npcs` UNION profession-slot characters. |

- **Placed and unplaced.** 777 residents are placed (`home_area IS NOT NULL`). 309 are unplaced (`home_area` NULL): 80 cty_, 62 rm_, 167 tn_, including 12 tn_ guards and 13 tn_ merchants. Unplaced residents have no body in the game, but **they still pass approval through `dm.story_residents`**, and every effect on them then fails at runtime ("No living resident"). Cast placed residents only.
- **Id prefixes.** `rm_` (471, of which 409 are placed), `rmx_` (150, all placed), `tn_` (262, 95 placed), `cty_` (200, 120 placed), `born_` (3, runtime births).
- **Ids with a trailing underscore** belong to House family: `rm_aldric_grayrock_`, `rm_isolde_fell_`, `rm_maren_grayrock_`, `rm_corwin_fell_`, `rm_gudrun_fell_`, `rm_ingrid_grayrock_`, `rm_tobias_grayrock_`.
- **House Timberland uses the old name "mossgrave" in its ids.** The faction id is `house_mossgrave` (named "House Timberland"), and its family's ids are `rm_*_mossgrave` (Signe, Halvard, Wenna, Osric Timberland).
- **`live.npcs.work_label` is the authored job and goes stale.** The job someone holds now is `checkpoint.society.careers.positions["job:<founder id>"].holder`. 72 placed residents hold no post. The `resident has job` condition reads the game, not the authored label.

## 2. Counts (placed residents, by where they live)

| | Ridgemere | Coldwater | Larkfield | Fenshore | Southmeadow | Saltwell | Frosthollow | Oakhaven |
|---|---|---|---|---|---|---|---|---|
| All | 600 | 68 | 36 | 24 | 19 | 13 | 9 | 8 |
| Merchants | 102 | 6 | 6 | 4 | 3 | 2 | 2 | 3 |
| Guards | 30 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

- **Towns have no Watch, no clergy and no faction claims.** Every guard, priest and faction official is in the city.
- **Life stage:** adult 643, young 67, adolescent 33, old 34. Sex: female 406, male 371.
- **City, by outdoor cell (civilian / merchant / guard):**

  | Cell | Civilian | Merchant | Guard |
  |---|---|---|---|
  | the Commons and the Rise | 168 | 55 | 21 |
  | the Wharf and the Sump | 97 | 9 | 2 |
  | Northgate | 93 | 35 | 6 |
  | Ridgemere Bay, North Headland | 59 | 3 | 1 |

  Each of Fellhall, Grayrock Hall and Timberland Court has 14 civilians (the House estates, each a cell of its own). About 10 more live out in the country. By `npc_voice.quarter`: commons 212, sump 179, estate 51, heights 42, works 32, office 32, wharf 12, none 40.
- **Faction of the current post** (from `live.faction_claims` on the post's work area): council_of_houses 338, unclaimed 196, no post 72, house_grayrock+ridgemere_watch 41, house_greatly 30, house_mossgrave 27, ridgemere_watch 27, house_grayrock 24, house_fell 20.
  - Factions: council_of_houses, ridgemere_watch, house_grayrock, house_fell, house_greatly, house_mossgrave.
  - `live.faction_members` and `live.faction_relations` are **empty**, so membership is derived and every relation is the default.
  - An **official**, who gives faction missions, is a guard or merchant post holder in a claimed workplace (`Game::officialOf`, `Core/RatwGameFactions.cpp:63`).

## 3. What has changed in the live world (useful hooks)
- **The King is dead.** `rm_gunnar_harrowgate` died of old age on day 41, at 62, in `r2_the_king_s_residence_up`. Mourners: Oda and Ottilie (family), Gerrit Mudhand, Brenna Abbot, Corra Coldridge and Desmond Farrow (friends). His estate was shared among 2 of the family.
- **The post went to a Sump Watch patrolman.** On day 46: "reigning at the Great Palace: someone local takes it on". The holder is **`rmx_dagfinn_thornham`** (Dagfinn Thornham, 31, male, calm). His backstory: "carries a letter meant for Lord Grayrock".
- **The heirs were passed over.** The Queen (`rm_oda_harrowgate`) and the King's daughter (`rm_ottilie_harrowgate`) still live at the Residence.
- **Other deaths:** `tn_cw_iris_greywater` (the Fenshore potter, day 27; her post went to `tn_oh_ludo_beechshaw`) and `rmx_aud_kemp` (a beggar, day 56; mantle rm154 released).
- **Births:** `born_1grqt7uo` (cw161) and `born_1gs05o74` (cw904), both in Coldwater cottages, and `born_1gs7as00` (rm907) at Pennock House.
- **Watch guards moved into shop posts:**
  - `rm_cedric_ashcot` now minds Septimus Slagby's shop door.
  - `rm_hanne_wickworth` minds a shop door.
  - `rm_silas_wickford` minds a shop door.
  - `rm_osric_plankford` minds a shop door.
  - `rm_wystan_mudcot` holds a wheelwright post.
  - `rm_beatha_foggate` keeps the chambers.
  - `rm_frederick_rainham` is at The Pawn and Ledger.
  - `rmx_desmond_hollis`, a Palace guard, now has no post.
- **Apprentices with a story angle:**
  - Rosalind Greatly is apprenticed to her mother Celestine ("keeping Greatly House"), so she is the heir.
  - Tobias Grayrock is apprenticed at the Bonded Warehouse.
  - Corra Coalcot is apprenticed to Karin Wetridge.
  - Desmond Farrow is apprenticed to Chancellor Gerrit Mudhand.
  - Corra Coldridge is apprenticed to the herald.
- **The residents most believed to be thieves (gossip):** `rmx_ingrid_kendrick` (803 beliefs), `rm_josephine_dripgate` (307), `rmx_dorcas_brineley` (268), `rmx_magnus_tarton` (264), `cty_ulla_rusthand` (222), `rm_hilde_dripby` (213), `rm_desmond_ashwick` (191), `rm_corra_coalcot` (176).
- **Theft events by actor:** Corra Coalcot 5, Hilde Dripby 4, then Gorm Pitchmarsh, Dorcas Brineley, Jorunn Saltdale, Torvald Slagton and Eleanor Wetby with 3 each. `honesty=sly` is the trait that makes residents steal.
- **Shared backstory hooks**, sentences that recur across residents (a ready-made web for a campaign; find them with `grep` on `live.npcs.backstory`):

  | Hook | Residents |
  |---|---|
  | lost a brother on the wharf and does not forgive House Grayrock | 56 |
  | dreams of a berth on a Grayrock ship bound somewhere dry | 54 |
  | grew up at a Timberland charcoal camp | 50 |
  | saving for a stall on the Commons Market | 48 |
  | born in the Sump, means to die in the Heights | 48 |
  | cousin in the Fell vineyards | 47 |
  | **once saw the Chancellor weep in the Rain Hall and has told no one** | 46 |
  | **owes a House factor money, to find by the new moon** | 41 |
  | **carries a letter meant for Lord Grayrock, who is never home** | 40 |
  | petition lost in the City Office | 38 |
  | believes the Houses will fall out before next winter | 38 |
  | **keeps a secret list of which Houses pay the Watch to look away** | 35 |

  In the towns: "knows a story about one of the great Houses that would be unwise to repeat" (7), "saw something big move under the ice" (7), "swears the marsh lights are the drowned" (7), "believes the old standing stones remember things", "lost a brother in a mine collapse".

## 4. Protection and mortality
- **Who is protected.** `Game::isProtected` (`Core/RatwGameTroubles.cpp:492`) counts:
  - a DM mark (`npc.protect`, which is permanent and **not** undone when a story ends);
  - authored `live.npcs.protected` (none set);
  - the holder of a House-head post (`Society::houseHead`: "ruling …", "holding court…", or "keeping X Hall/House/Manor/Court");
  - a ranked faction member (none exist).

  **Protected by rule today:** `rm_isolde_fell_`, `rm_celestine_greatly`, `rm_maren_grayrock_`, `rm_signe_mossgrave`.
  **Not protected:** the new King, the Queen, the Watch commander and the abbot. Players can attack them, and `npc.kill` works on them.
- **Old age.** `World::tendOldAge` gives a yearly death chance of about 1% at 40, 7% at 60, 15% at 70 and a third at 80, checked daily. **Only authored `live.npcs.protected` stops it.** Neither `npc.protect` nor the House-head rule does.
  Key residents at risk: Halvard Timberland 66, Ingrid Kendrick 68, Bramble Bogditch 78, Ambrose Greatly 63, Percival Thornbury 64, Oda 60.
  Guard against a death with `resident is <npc> alive` conditions or a fallback branch.
- **Births match deaths.** The project rule (memory `births-match-deaths.md`): never generate NPCs; an empty post goes to a jobless city wolf, then to an **unplaced** Country NPC.
  So **do not add rows to `live.npcs`** for heroes or villains. Cast existing placed residents. For walk-ons use story strangers (below). Bringing in one of the 309 unplaced Country NPCs has no story effect today (see §8).

## 5. How story effects name residents
- **Parameters of type `resident`** (keys `npc`, `resident`, `from`, `like`, and a quest's `giver`) take the plain `live.npcs.id`, e.g. `"npc": "rm_corra_slagmore"`. At approval they are checked against `dm.story_residents` (`tools/story_runner.py:103-125`).
- **The shipped sample programs name stale ids.** `Data/Storylines/programs/{wagon,river-relief,winter-ferries,wardens,hidden-pup}.json` use ids like `rm111`/`rm112` that are not residents of Ridgemere 3; `siege-at-greyfen` uses Greyfen ids. They will not resolve here.
- **Resident effects** (`Data/Storylines/trigger-catalogue.json`, 71 effects):
  - npc.line, npc.offer, npc.speak, npc.walk, npc.move, npc.pose
  - npc.hold / npc.release, npc.follow / npc.unfollow
  - npc.kill (refused on a protected resident), npc.revive, npc.protect / npc.unprotect
  - mantle.assign / mantle.release, visitor.add / visitor.leave
  - storyline.give (takes a resident and a place), story.tell (`from` = the resident who says it)
- **Resident conditions:** `resident is` (alive, dead, indoors, out of doors), `resident has` (job, mood, mantle, town), `resident in place`, `resident in region`.
- **Quest givers** (`tools/story_quests.py:136-200`):
  - The quest field is `giver: <resident id>`.
  - `offer {say ≤300, ask ≤120, accept ≤120, decline ≤120, handIn ≤120, thanks ≤300}`; `handIn` defaults to true when there is a giver.
  - `board: <town>` offers the quest on a notice board instead of through a giver.
- **How story choices appear in the dialogue window** (`Core/RatwGameWindowTalk.cpp:365-530`):
  - They are added to the window's home node, before the last way-out choice, with **at most 9 choices in total**.
  - An offer node shows the `say` text, then Accept, Decline and Goodbye. A hand-in node shows `thanks` or "#thanks#. That's a weight off."
  - An `npc.offer` reply node shows the reply, then "Something else..." and "Goodbye for now."
  - **So a story's talk with a resident is one level deep.** A multi-step quest conversation needs `npc.offer` chains driven by `dialogue choice` triggers, or the new trigger in §8.
- **Strangers** (`visitor.add`, `World::addVisitor`, `Core/RatwWorld.cpp:4883`):
  - A stranger is a transient entity: id 3-40 characters (a-z, 0-9, - and _), unique in the world; a name up to 60 characters; a cell and a tile that can be stood on; 1 to 1440 minutes.
  - Its looks come from `like` (a resident's appearance, plus their description when none is given) and `description` (up to 200 characters). Its age is fixed at 30.
  - It has **no society spec**: no schedule, no job, **no mantle** (`mantle.assign` refuses "not a resident"), no voice row, and no `npc.offer`, `giver` or `npc.walk` targeting unless the code allows it (unverified).
  - It is sent away when the story ends.
  - `live.story_visitors` (doc 58's approved list) has 0 rows. It is not used by `visitor.add`.
  - A recurring villain as a stranger must be re-added at most every 24 hours.
- **The mantle gotcha.** `mantle.assign` (`Game::assignMantle` → `Society::bindMantle`, `Core/RatwHandoff.cpp:469`) **overwrites the resident's own binding**. At the story's end `endStoryWorld` calls `releaseMantle`, which **does not restore the old mantle** (`Core/RatwGameStoryWorld.cpp:479`). The resident is left mantle-less, and their original mantle drifts to spare or resting and may pass to a newborn.
  The bind is also refused unless the mantle is `ready`, worn by no one living, and the resident passes its `fits` line (sex, role, class, job, stage).
  **Advice:** do not cast through `mantle.assign` until it restores the old mantle; carry quest talk in story text (offer, reply, npc.line, scenes, story.tell `from`).

## 6. Mantles and dialogue trees
- **Mantle files.** `Data/Talk/mantles-export/<dialect>/<id>.mantle`: coldwater 119, frosthollow 17, larkfield 68, mirebridge 34, oakhaven 16, plain (Ridgemere) 491, saltwell 26, southmeadow 34. `index.tsv` (id, dialect, tier, state, revision, written_for, written_by) has 805 rows.
  - The game reads `live.mantles` and `live.mantle_lines`; the files are the export.
  - Ids are a dialect prefix plus a number (`rm`, `cw`, `lf`, `sm`, `sw`, `oh`, `fh`, `mb`). 101 and up are written for a resident; 9xx are spares, about 30 unbound.
  - 774 placed residents wear a mantle.
  - **A mantle's dialect follows the wearer's upbringing** (`npc_voice.home_region`): Queen Oda wears cw210 (Coldwater), Dagfinn Thornham mb131 (Mirebridge).
- **The file format** (`Docs/Design/84-mantle-writers-guide.md`):
  - A header: `mantle`, `revision`, `state`, `dialect`, `accent`, `tier` (full is 92+ items, standard 55+), `written_for`, `title`, `fits`, `prefers`, `traits` (all seven; **never change honesty**), `quirk`, likes, dislikes, fears, hopes, topics, styles, marks, persona.
  - Sections: `words`, `any`, `grown`, `young`, `adolescent`, `old`, holding opinion, line, scene, story, `dlg <tree>.<node> [criteria] : text`, and `graft <tree>.<node>` (choice, node, say, mood).
  - A line is at most 160 characters.
  - **Mantles must be name-free:** no literal names of anyone in the world, because they pass to newborns.
  - World text rules: never "dog"; "pups"; male or female only; the Bright One and the Dark One, never "the gods".
- **Tools:** `tools/mantle_brief.py ridgemere3 <npc>`, `tools/mantle_index.py <town> --world ridgemere3`, `build-core/talk_check --mantle FILE`, `tools/mantle_check.py <dir> --world ridgemere3 --strict`, `tools/mantle_load.py ridgemere3 FILE… --batch NAME [--bind]` (refuses a live world).
- **How a tree attaches to a resident.** Trees are in `Data/Talk/dialogue/*.tree`; the most specific `for` wins by priority:

  | Tree | `for` | Priority |
  |---|---|---|
  | receiver | `here.deliveries>0` | 5 |
  | companion | `here.companion=yes` | 4 |
  | watch | `role=guard` | 3 |
  | inn | `job=innkeeper` | 2 |
  | church | `job=priest` | 1 |
  | official | `here.official=yes` | — |
  | shop | `role=merchant` | — |
  | resident | (the fallback hub) | — |

  master and work are entered through entries: `work` (contracts), `master` (apprenticeship).
  The wearer's mantle supplies `dlg` lines for these nodes and `graft`s, which are extra choices.
  Resident nodes: hub, about, town, job, how, business, hours, quirk, opinions, kin, news, gossip, told, vouch, letters, festival, records, trouble, helped, way, place, people, whereis, day, wanted, match, introduced, name, thanks, refused, unclear, bye.
- **Quest-giver and mission roles today** (doc 83 "Missions and work" and the trees by role):
  - **Merchants give contracts:** escort a wagon, courier a letter to a resident, bounty on a bandit camp, three at most on the wheel (`offer kNNN` / `take kNNN`). They complete themselves (wagon arrives; within 3 tiles of the recipient; camp cleared).
  - **Faction officials give missions:** deliver goods, carry a message, guard within 12 tiles; Chapter members only. The official tree also has a paid report, tithe, and treaty or recognition for a Head.
  - **Receivers** take hand-ins (`do deliver`).
  - **The rest of the wheel:** ask for work, apprentice (the master tree), lend a paw, doc 57 troubles (help, peace, speak, sponsor), storyline `talk` objectives.
  - **Story quests** add their giver's offer and hand-in on the window's home node.

## 7. Casting sheet (62 placed, living residents)

Columns: id | name | sex, age, stage | where (cell or town / quarter) | post now | faction of the work area | traits | mantle | fit for the story. Traits read as `honesty`, `temper` and so on; `{}` means none recorded.

**Crown and government (Commons and the Rise, "office")**

| id | name | sex, age, stage | where | post now | faction | traits | mantle | fit |
|---|---|---|---|---|---|---|---|---|
| rmx_dagfinn_thornham | Dagfinn Thornham | m 31 adult | Commons/works | reigning at the Great Palace (was a Sump patrolman) | council | calm | mb131 | Hero: an accidental King; carries the Grayrock letter; unprotected |
| rm_oda_harrowgate | Oda Harrowgate | f 60 adult | Commons/office | the Queen, at the King's Residence | council | restless, sly | cw210 | Villain or schemer: holds the royal purse, passed over |
| rm_ottilie_harrowgate | Ottilie Harrowgate | f 39 adult | Commons/office | the King's daughter | council | calm, warm | rm541 | Hero or rightful heir: carries the Crown's petitions; a coughing, frail heir |
| rmx_brenna_abbot | Brenna Abbot | f 52 adult | Commons | herald of the Great Palace | council | calm, warm, content | rm136 | Quest-giver for proclamations; owes a House factor |
| rm_gerrit_mudhand | Gerrit Mudhand | m 55 adult | Commons/office | chancellor of the Council | council | chatty, warm | rm310 | Ally or quest-giver; mourned the King; saw the Chancellor weep |
| rm_oskar_gullworth | Oskar Gullworth | m 40 adult | Commons/office | Chancellor of Ridgemere | council | cold, guarded | rm344 | Villain or rival chancellor: Sump-born climber |
| rm_percival_rustford | Percival Rustford | m 53 adult | North Headland/sump | serving in the King's household | council | short, sly | rm296 | Informant or traitor in the household |
| rm_mab_coalridge | Mab Coalridge | f 30 adult | Wharf and Sump | serving in the King's household | council | nervy, cold, guarded | rm317 | A spy: £218, far too rich for a servant |

**The Houses (the estates; heads protected)**

| id | name | sex, age, stage | where | post now | faction | traits | mantle | fit |
|---|---|---|---|---|---|---|---|---|
| rm_isolde_fell_ | Isolde Fell | f 58 adult | Fellhall | ruling House Fell (protected) | fell | {} | rm526 | Antagonist power: never lets a debt go |
| rm_corwin_fell_ | Corwin Fell | m 31 adult | Fellhall | keeping the Fell cellars | fell | {} | rm491 | Hothead: wants the Rainward Hills for Fell |
| rm_gudrun_fell_ | Gudrun Fell | f 29 adult | Fellhall | keeping the Fell accounts | fell | {} | rm514 | Double agent: secret Timberland letters |
| rm_maren_grayrock_ | Maren Grayrock | f 47 adult | Grayrock Hall | keeping Grayrock Hall (protected) | grayrock | {} | lf162 | Personal arc: lonely lady, unanswered letters |
| rm_aldric_grayrock_ | Aldric Grayrock | m 52 adult | Grayrock Hall | overseeing the Grayrock wharf | grayrock | {} | rm475 | The absent lord (£400): target of 40 undelivered letters; resented by 56 |
| rm_ingrid_grayrock_ | Ingrid Grayrock | f 16 adolescent | Grayrock Hall | at her lessons in the Hall | grayrock | {} | rm525 | Side story: writes to her father weekly |
| rm_tobias_grayrock_ | Tobias Grayrock | m 23 adult | Grayrock Hall | keeping the wharf's books (apprenticed at the Bonded Warehouse) | grayrock | {} | rm554 | Reluctant heir |
| rm_celestine_greatly | Celestine Greatly | f 58 adult | Northgate/estate | keeping Greatly House (protected) | greatly | {} | sw124 | Information broker: the salon every House attends |
| rm_ambrose_greatly | Ambrose Greatly | m 63 adult | Northgate/estate | overseeing the Counting House | greatly | {} | rm476 | Prime villain: "loans the others could not refuse" |
| rm_rosalind_greatly | Rosalind Greatly | f 22 adult | Northgate/estate | reading in the gardens (Celestine's apprentice) | greatly | {} | rm417 | Whistleblower hero: "a ledger that does not add up" |
| rm_julian_greatly | Julian Greatly | m 29 adult | Northgate/estate | keeping the Greatly Yard's books | greatly | {} | rm530 | Wants to go to sea; drinks at the Sailors' Rest |
| rm_edric_kelpham | Edric Kelpham | m 51 adult | Northgate/estate | stewarding Greatly House | greatly | chatty, merry | rm502 | Debtor in deep: an inside source |
| rm_signe_mossgrave | Signe Timberland | f 61 adult | Timberland Court | keeping Timberland Court (protected) | timberland | {} | rm389 | Council vote broker |
| rm_halvard_mossgrave | Halvard Timberland | m 66 old | Timberland Court | walking the gardens | timberland | {} | rm454 | Every smith would lie for him; old-age risk |
| rm_wenna_mossgrave | Wenna Timberland | f 34 adult | Timberland Court | running the House's iron trade | timberland | {} | rm440 | Ambitious rival to Grayrock shipping |
| rm_osric_mossgrave | Osric Timberland | m 27 adult | Timberland Court | riding out on House business | timberland | {} | sm132 | Country errands, between city and sheep runs |

**The Watch, the clergy and the courts**

| id | name | sex, age, stage | where | post now | faction | traits | mantle | fit |
|---|---|---|---|---|---|---|---|---|
| rm_corra_slagmore | Corra Slagmore | f 50 adult | Commons/heights (Watch House) | commanding the Watch | watch | quiet, calm, guarded | rm490 | Compromised commander: owes a House factor |
| rm_bettany_nailley | Bettany Nailley | f 26 adult | North Headland/sump | sergeant of the Watch | watch | ambitious, calm, warm | rm483 | Hero cop; hates Grayrock |
| rm_dagny_ropeley | Dagny Ropeley | f 42 adult | Commons/sump | sergeant of the Watch | watch | quiet, guarded | rm494 | Steady sergeant; another Grayrock letter |
| rm_desmond_dripcot | Desmond Dripcot | m 40 adult | Commons/sump | keeping the cells | watch | short, frank | rm497 | Holds the list of Houses paying the Watch: evidence |
| rmx_desmond_hollis | Desmond Hollis | m 33 adult | Northgate | (no post; was a Palace guard) | — | short, reserved, sly | cw197 | Dismissed guard: a hireling for a villain |
| rm_lionel_ropestack | Lionel Ropestack | m 46 adult | Northgate/sump | abbot of the cathedral | council | calm, content | rm299 | Moral authority; also holds the Watch list; £4 |
| rm_solveig_tarwell | Solveig Tarwell | f 44 adult | Wharf and Sump/office | priest of the Chapel of the Tide | council | quiet, even | rm549 | Wharf priest; quest-giver for the drowned and the wharf |
| rm_gorm_plankgate | Gorm Plankgate | m 55 adult | Northgate | infirmarian of the abbey | council | cold, guarded | sw117 | Healer; sickness and injury quests |
| rm_cressida_carlisle | Cressida Carlisle | f 48 adult | Commons/heights | pleading cases | council | quiet, restless | rm492 | Advocate for the heir's claim (£305) |
| rm_percival_thornbury | Percival Thornbury | m 64 adult | Commons/heights | pleading cases | council | cold, guarded | rm404 | Opposing advocate (£290) |
| rmx_philippa_coldby | Philippa Coldby | f 41 adult | Commons | clerking for the magistrate | council | plain, sly | rm130 | Forger or leak in the court records |

**Commerce, the underworld and the wharf**

| id | name | sex, age, stage | where | post now | faction | traits | mantle | fit |
|---|---|---|---|---|---|---|---|---|
| rm_corra_coalcot | Corra Coalcot | f 22 adult | Wharf and Sump/works | runs The Fish Hall | council | chatty, merry, sly | rm325 | Rogue or villain: 5 thefts; the bribe list |
| rmx_ingrid_kendrick | Ingrid Kendrick | f 68 old | Northgate | (no post) | — | quiet, merry | rm171 | Red herring: believed a thief by 803 |
| rm_jorunn_ropewick | Jorunn Ropewick | f 43 adult | Commons/sump | pawnbroker at The Pawn and Ledger | council | quiet, dry, short | rm238 | Fence for stolen goods |
| rm_eleanor_saltcot | Eleanor Saltcot | f 48 adult | Commons | moneychanger at The Greatly Bank | greatly | chatty, ambitious | rm505 | The Greatly money trail |
| rm_dagny_rustcot | Dagny Rustcot | f 47 adult | Wharf and Sump/office | harbourmaster of the Wharf Office | grayrock | quiet, calm | cw170 | Ships and smuggling quests |
| rmx_fenwick_mossdale | Fenwick Mossdale | m 33 adult | Northgate | warden of the Market Hall | council | nervy, warm | cw164 | Market troubles quest-giver |
| rm_imogen_nailridge | Imogen Nailridge | f 31 adult | Commons | cartographer at Soundings | council | plain, ambitious | rm153 | Exploration and survey quests |
| rm_mab_carlisle | Mab Carlisle | f 55 adult | Commons/heights | keeping the inn (the Sailors' Rest) | council | short, frank | lf161 | Inn hub; holds a Grayrock letter |
| rm_hilde_dripby | Hilde Dripby | f 61 adult | North Headland (Barracks) | (no post) | — | short, sly | rm518 | Thief who saw the Chancellor weep: blackmail |
| rmx_dorcas_brineley | Dorcas Brineley | f 23 adult | North Headland/sump | crying wares in the Rag Market | grayrock + watch | restless, devout | rm264 | Zealot against Grayrock; a thief |
| rm_desmond_grantham | Desmond Grantham | m 43 adult | Commons/heights | runs The Rainwater Brewery | council | short | rm414 | "The Houses will fall out": an omen-teller |
| rm_josephine_dripgate | Josephine Dripgate | f 21 adult | Commons/sump | shopkeeper at Sump Sundries | council | quiet, dry | rm363 | Sump climber; suspected thief |
| rm_rupert_fogton | Rupert Fogton | m 32 adult | Commons | apothecary at Hollis Physic | council | cold, guarded | rm163 | Poisons or remedies |

**The towns**

| id | name | sex, age, stage | where | post now | traits | mantle | fit |
|---|---|---|---|---|---|---|---|
| tn_cw_fallow_lakewick | Fallow Lakewick | f 58 adult | Coldwater | wool loom master | chatty, devout | cw106 | Town elder who distrusts the city |
| tn_sm_linden_harvestton | Linden Harvestton | f 57 adult | Coldwater | stillmaster at Coldwater Still | quiet, warm, guarded | cw124 | Orchard dream |
| tn_fh_loam_pinefell | Loam Pinefell | m 52 adult | Coldwater | taproom, The Glasswater Inn | short, reserved | cw109 | Inn hub; lost a brother in a mine collapse |
| cty_lorn_hedgeton | Lorn Hedgeton | m 58 adult | Coldwater (Greyteeth farm) | raises sheep at Greyteeth | ambitious, cold, guarded | cw110 | Hard farmer; "watches the road" |
| tn_mb_aspen_marshholm | Aspen Marshholm | f 55 adult | Larkfield | linen loom master | chatty, merry, warm | lf107 | Cheerful quest-giver |
| tn_mb_tansy_eelbridge | Tansy Eelbridge | f 45 adult | Larkfield | village smith | chatty, dry | lf128 | Smith; "a path across any bog": a guide |
| tn_lf_rook_wheatfield | Rook Wheatfield | m 58 adult | Larkfield (Longmead) | farms Longmead | chatty, warm | lf127 | Sweetheart in Southmeadow: a romance errand |
| tn_sm_dun_harvestley | Dun Harvestley | m 46 adult | Larkfield | country printer | dry, warm | lf114 | Pamphlets and news-spreading quests |
| tn_sm_iris_meadowfield | Iris Meadowfield | f 54 adult | Southmeadow (Larkholm) | farms Larkholm | calm, frank | sm109 | Harvest quests |
| tn_lf_rowan_downridge | Rowan Downridge | f 27 adult | Southmeadow | taproom, The Vine and Sickle | quiet | sm104 | Inn hub (£210) |
| cty_briar_cornby | Briar Cornby | f 41 adult | Southmeadow | cellar hand | dry, sly | sm107 | Standing-stones lore; a sly trickster |
| cty_nell_plummeadow | Nell Plummeadow | f 53 adult | Fenshore | taproom, The Heron | plain, even | mb101 | Inn hub; an unanswered love letter |
| tn_oh_ludo_beechshaw | Ludo Beechshaw | m 46 adult | Fenshore | village potter (took the dead Iris Greywater's post) | chatty, sly | oh110 | Suspicious inheritance |
| cty_sloe_mistwick | Sloe Mistwick | f 44 adult | Fenshore (Rivermouth) | skipper | short, sly | mb111 | Smuggler; a well feud |
| tn_mb_skye_eelbank | Skye Eelbank | f 50 adult | Fenshore (Withy Creek) | traps eels | chatty, warm | mb105 | Marsh guide |
| tn_fh_thistle_stonepick | Thistle Stonepick | f 36 adult | Saltwell | shopkeeper, Saltwell Stores | dry, calm | sw101 | Mine-collapse grief |
| tn_mb_oriel_marshditch | Oriel Marshditch | f 30 adult | Saltwell | taproom, The Oasis Inn | ambitious, frank | sw102 | Marsh-lights lore |
| tn_mb_bramble_bogditch | Bramble Bogditch | f 78 old | Saltwell (Saltpan) | (no post) | devout, dry | sw104 | Old seer; high old-age risk |
| tn_fh_tamsin_icefell | Tamsin Icefell | f 31 adult | Oakhaven | taproom, The Felled Cedar | nervy, frank | oh107 | Wants to leave for Ridgemere |
| tn_oh_cairn_oakhaven | Cairn Oakhaven | m 30 adult | Oakhaven | shopkeeper | plain, sly | oh108 | Wary trader |
| cty_ulla_rusthand | Ulla Rusthand | f 31 adult | Oakhaven (Rainwood) | logger | warm, sly | oh106 | Thief believed by 222 |
| cty_fenwick_plankworth | Fenwick Plankworth | m 54 adult | Frosthollow | mine captain | restless, sly | cw162 | Villain or hero of the mine; "something under the ice" |
| tn_oh_thorn_ashwood | Thorn Ashwood | m 52 adult | Frosthollow | taproom, The Lamp and Pick | calm, warm | fh107 | Inn hub |
| tn_fh_aster_coldfell | Aster Coldfell | f 35 adult | Frosthollow | shopkeeper | calm, sly | fh106 | Wants to leave for Ridgemere |

Other usable names:
- **City gates and posts:** north gate Cedric Mudley and Ivo Dripford; east gate Liv Kelpmarsh and Vidar Dripmarsh; south gate Hester Bilgeham and Tilde Nailby; Harbour Gate Hanne Coalton; Cinder Gate Palle Plankmore; Customs Pier Rosamund Sootmarsh.
- **Customs:** Jasper Bilgeford (`rm_jasper_bilgeford`, collects the Crown's customs).
- **Grantham and Thornbury children** (Bertil Grantham, 11, blames Grayrock for a brother's death).
- **Out of the city:** Nan Ashhand (`cty_nan_ashhand`, quarrymaster at the Rainward quarry, saving coin toward something no one knows about).

## 8. Gaps and possible triggers (cast side)
1. **Make `mantle.assign` restore the previous mantle on release, or add a story "part".** A story-only overlay of `dlg` lines and grafts that leaves the resident's own binding alone. This is the safe way to give a story's NPC their own branching window talk.
2. **Multi-node story dialogue.** Story choices are one level deep (offer, reply, thanks) with 9 choices at most. A small story tree (nodes, choices, conditions on story variables), or an `npc.offer` option `next: [offer ids]`, would allow real quest conversations.
3. **Strangers.** No mantle, schedule or dialogue tree, and a 1440-minute cap. Allow a stranger to give quests and to be re-added across days, or keep villains as existing residents.
4. **A story arrival.** Bring an unplaced Country NPC (309 available) into the world: a house, a post, a bound spare mantle. This respects "never generate NPCs".
5. **Keeping key residents alive.** `npc.protect` does not stop old age; only authored `live.npcs.protected` does. A story mortality hold for key cast, undone at the story's end, would fill the gap.
6. **A succession event or condition.** "succession" and "vacancy" exist in `game.events`, but no `post holder is` condition was seen. One would make the King's death and Dagfinn's accession usable as a trigger.
7. **Validation.** Approval should check placed and alive residents, not `dm.story_residents` (which includes the unplaced).

## 9. Commands used
- `python3 -I /tmp/castsurvey/q.py "<SQL>"`: a throwaway read-only helper wrapping `world_db.connect('ridgemere3','owner')` and `SET TRANSACTION READ ONLY`.
- The checkpoint dump is at `/tmp/castsurvey/ckpt.json`. The full resident table with home cells is at `/tmp/castsurvey/res2.tsv`; columns: id, name, sex, age, role, work_label, home, work, home_region, work_region, voice_region, quarter, traits, mantle, purse, personality, backstory, home_cell, home_cell_name, home_name, work_name.
- **Current post holders:** `json.load(open('/tmp/castsurvey/ckpt.json'))['society']['careers']['positions']`.

## Key files
- /home/martinb/Documents/GitHub/MUDProject/Data/Storylines/trigger-catalogue.json
- /home/martinb/Documents/GitHub/MUDProject/tools/story_quests.py
- /home/martinb/Documents/GitHub/MUDProject/tools/story_runner.py
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwGameWindowTalk.cpp
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwGameHandoff.cpp
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwHandoff.cpp
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwGameStoryWorld.cpp
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwWorld.cpp
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwGameTroubles.cpp
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwSociety.cpp
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwGameFactions.cpp
- /home/martinb/Documents/GitHub/MUDProject/Data/Talk/dialogue/
- /home/martinb/Documents/GitHub/MUDProject/Data/Talk/mantles-export/
- /home/martinb/Documents/GitHub/MUDProject/Docs/Design/83-dialogue-trees.md
- /home/martinb/Documents/GitHub/MUDProject/Docs/Design/84-mantle-writers-guide.md
- /home/martinb/Documents/GitHub/MUDProject/Docs/Design/85-story-triggers-and-campaign-control.md