# The Ninth Howl: cast and talk plan (Ridgemere 3)

Companion to `artifacts/quests/design/campaign.md` (the campaign design, "§" references below point into it unless they say otherwise). World: Ridgemere 3, world key `ridgemere3`, db `ratw_ridgemere3`, world id `upper_accord`.

**What this file holds:**
- **§2 (a) The cast sheet:** every hero, villain and key NPC, with id (or stranger/foe spec), sex, age, home and quarter, dialect, temperament, motive, arc by chapter, fate by branch, protection, looks and voice.
- **§3 (b) Dialogue paths:** what each one says in the dialogue window at each stage (greeting, offer, during, hand-in, after each branch), written in their dialect, with the choice menus the wolf picks from.
- **§4 (c) Gossip and helpful knowledge:** what ordinary residents say at each stage, per town and dialect, 6+ lines per dialect per stage, with the state each line depends on, plus one NPC-to-NPC scene per stage.
- **§5 (d) Where every piece goes:** files and formats, and the engine support that is missing (reusing gaps.md's C-items, campaign.md's J-items, engine-spec's N- and V-items, and the talk items T3, T5, T6, T7 and T9; T1, T2, T4 and T8 are merged into C8, N2, V2 and N1, decisions.md §5.1).

**Binding decisions.** `decisions.md` and the giver registry `givers.tsv` win over this file. In particular: no campaign cast, Keeper or innkeeper gives a pack quest (§3.25 is a pointer to the stage pack); innkeepers give helpful knowledge and recaps only; a `story.tell` banner with a face is **not** the dialogue window, so no resident's words travel in `story.tell {from}` (decisions §9, P2-22); every gate spells out its values (no "…").

**How it was checked.** Every resident fact (sex, age, home, work, `npc_voice.home_region` dialect, mantle, traits, personality, backstory, looks) was read from the 2026-11-22 read-only resident dump (`/tmp/castsurvey/res2.tsv`, 777 placed residents) and the checkpoint's post holders (`/tmp/castsurvey/ckpt.json`). The tree syntax, the nine-choice rule and the `dialogue node` event were read from `Data/Talk/dialogue/*.tree`, `Core/RatwTalkTree.cpp` (`parseTree`, `mostShown`, `disjoint`) and `Core/RatwGameDialogue.cpp:1012`. Grammar symbols per dialect were read from `Data/Talk/grammar/*.grammar`. Nothing in the repo or the world was changed; this file is the only output.

---

## 0. Conventions

### 0.1 Stage keys (the talk memory every line is gated on)
All gates are talk memory written by the campaign (C8 `talk.remember`; §5.3). `nh_stage=letters` below means the fact `world.mem.nh_stage=letters`. Several stages are written as an alternation, `world.mem.nh_stage=letters|lights|harvest` (`Core/RatwTalk.cpp:1037` splits a gate's values on `|`). Every value is spelled out; no gate in this file uses "…". "All stages" means `world.mem.nh_stage=king|moot_called|reckoning|signals|howls|letters|lights|harvest|bell|moot`.

| Stage key | When | Chapter |
|---|---|---|
| `king` | from Ch 1's opening to the Herald's Word | 1 |
| `moot_called` | from the Herald's Word to Ch 1's end | 1 |
| `reckoning` | Ch 2 | 2 |
| `signals` | Ch 3 | 3 |
| `howls` | Ch 4 | 4 |
| `letters` | Ch 5 | 5 |
| `lights` | Ch 6 | 6 |
| `harvest` | Ch 7 | 7 |
| `bell` | Ch 8 | 8 |
| `moot` | Ch 9 up to the Moot's end | 9 |
| (after) | `nh_stage` forgotten; `nh_crown` and `nh_concord` kept for 90 days (`keep:true`) | epilogue |

Branch keys (`nh_debts`, `nh_lists`, `nh_captain`, `nh_lights` and the rest) are the ones in campaign.md §10.1, with the additions listed in §5.3 of this file. `nh_debts` gains the value `grace` (decisions §6.4; §4.3 below).

### 0.2 Vehicle codes (where a line lives; the full map is §5)
| Code | Vehicle | Written in | Speaks to | Symbols filled? | Limit |
|---|---|---|---|---|---|
| **Q** | quest offer block (`quests[].offer`: say, ask, accept, decline, handIn, thanks) | the chapter's story program | wolves the offer is open to | `say`, `thanks`: yes. ask/accept/decline/handIn: no (wolf's words) | say/thanks 300; the rest 120 |
| **O** | `npc.offer` choice and reply, or (from engine Phase 4) the quest's C6 `offer.during` (`during` ≤ 300, `duringAsk` ≤ 120, on the giver) | story program (trigger effect, or the quest's `offer`) | participants; one wolf with J1 | reply / `during`: yes; words: no | words 120, reply 300 |
| **L** | `npc.line` (greeting add-on for one phase) | story program (phase entry) | participants | **no** (raw text; T5 asks for it) | 200 |
| **T** | tree greeting line `say[self.id=<id> world.mem.nh_stage=<key>]` | `Data/Talk/dialogue/{resident,watch,church,inn}.tree` | anyone at that stage | yes | 160 per alternative |
| **N** | tree node (a campaign talking path), reached by a gated choice; **the story hears it** through the existing event `dialogue node` (actor the wolf, target the resident, detail the node id) | `Data/Talk/dialogue/resident.tree` | anyone at that stage | yes | 160 per alternative; ≤ 9 choices per node |
| **P** | a per-wolf line `say[self.id=<id> pair.mem.<k>=<v> priority=5]`, written by C8 `talk.remember {scope:"pair"}` (§5.3); or a one-off C5 `npc.greet` | tree + story effect | that one wolf | yes | 160 / 200 |
| **S** | scene beat (`ratw-scene` `line`, `choice`, `response`): **each speaker in their own panel with their face**, in the dialogue window. Strangers get a panel only with T3 | story program `scenes[]`, played by `scene.play` | the scene's audience | no (text as written) | line 400 |
| **Nr** | narration: `story.tell` with **no `from` and no quoted speech** (what a wolf sees, not what anyone says) | story program effect | a wolf, party or region | no | 300 |
| **G** | gossip: `gossip` / `news` / `nh_story` / `nh_help` / `nh_who` / `nh_ask` tree lines, and `.talk` rules and NPC-to-NPC scenes | `Data/Talk/dialogue/resident.tree`, `Data/Talk/rules/story_ninth_howl.talk`, `Data/Talk/scenes/common/story_ninth_howl.talk` | anyone | yes | 160 |
| **B** | `board.post` (a proclamation is a signed notice), `journal.note`, `deed.award` phrase | story program | town / one wolf / gossip | no | 300 / 200 / 160 kept |

**Never a vehicle for a resident's words:** `story.tell {from: <resident>}`. A face banner is not the dialogue window (decisions P2-22). Where an earlier draft used one, the words now sit in the speaker's own window: a T or P tree line, an N node, an O reply, a C5 `npc.greet`, a C6 `offer.during`, or a scene beat in their own panel. Before C5 exists, a resident's one-off thanks is a gated tree line or a `deed.award`.

### 0.3 Writing rules held throughout
- Wolves and pups, never the forbidden word for them. Only the Bright One (he, ascended) and the Dark One. "Private messages". Male or female only; pronouns follow sex.
- **Dialect by upbringing** (`npc_voice.home_region`, which is not always where they live): a set phrase ("Still water", "Dry feet", "Peace on you", "Scales and silver", "Shade on you", "Warm hearth", "Good harvest") appears only in the mouth of a speaker raised in that dialect. Frames use `#greet#`, `#farewell#`, `#thanks#`, `#address#`, `#oath#`, `#bc_it_is#`, `#very#`, `#good#`, `#bad#` so each speaker's grammar fills them. A symbol's value carries no closing punctuation, so the full stop is written after it (`#thanks#.`); a symbol that opens a sentence mid-line is written `#x.capitalize#`.
- **Tree lines name no one by name.** Others are named by role ("the lamp-keeper", "the herald", "the Grayrock lord", "the Counting House master", "the pup at the landing") or by `#world.figure_ruler|villain|captain|hero#` once C9 sets them. Story texts (Q, O, L, S, B) may use names, because they are bound to one run and are never passed to a newborn.
- No ids in speech. No weather or time-of-day words without the gate (`here.weather`, `here.time`); lines marked ⁽ʷ⁾ carry `here.weather=fog` (or are rephrased as hearsay if `talk_lint` objects), lines marked ⁽ⁿ⁾ carry `here.time=dusk|night`.
- 160 characters per tree alternative (`|` separates alternatives).
- **Choices are the wolf's own first-person speech**, as `resident.tree` writes them ("I was wondering about something else...", "Goodbye for now."). Every campaign node ends with those two ways out unless noted.
- Proper names used in tree lines that are not yet in `tools/dialect_allow.json` "places" are listed in §5.6 for the allow list.

### 0.4 Line notation used in §3 and §4
- `T king:` a greeting at that stage. `T [nh_b_ledger=shown]:` an extra gate (always together with the stage unless "any stage" is said).
- `N node_id:` a node; under it `say[...]` lines and `> I want to know -> next` choices (the `>` marks a choice the wolf picks).
- `Q offer say:` / `ask:` / `accept:` / `decline:` / `handIn:` / `thanks:` for quest blocks.
- `O words → reply` for an `npc.offer`.
- `P` lines need C8's pair scope (engine Phase 1) unless they are marked "C5". Each P line names its key, and §5.3 gives the write (`talk.remember {scope:"pair", npc, player:"$actor", key, value, days, keep:true}`) and the fallback.
- In §4, each line carries a kind: **R** rumour, **W** where to go, **A** who to ask, **V** a hint about the villain, **!** a warning, **H** thanks to the heroes, **X** recap for a late joiner.

---

## 1. The talking architecture in one page

Campaign.md §10 put window talk in `say[self.id=<id>]` lines and `npc.offer` chains. Reading the tree code adds a better tool, which this plan uses for most talking paths:

1. **Tree nodes are story buttons.** When a wolf's window moves to any tree node, the game writes `dialogue node` (actor = wolf, target = resident, item = tree, detail = node id) for any story that listens (`Core/RatwGameDialogue.cpp:1012`). So a campaign conversation can be written as ordinary library nodes in `resident.tree`, gated on `world.mem.nh_*` and `self.id`, and a chapter trigger `{"on":"dialogue node","match":{"detail":"nh_dq_stand"},"audience":"participant"}` records the wolf's choice. This:
   - shows **each wolf their own path** with no J1 (the window is already per wolf);
   - costs **no hub room** and none of the story's 64 offers (nodes hang off `news`, which has room for exactly one more choice);
   - is **filled in each speaker's voice** (symbols), unlike `npc.line`;
   - can't, on its own, know a wolf's personal progress: C8's pair scope (`pair.mem.*`) and N2 (`player.quest.<id>` and `player.story.<var>` as window facts) close that.
2. **Disjoint gates share a slot.** `mostShown` counts choices whose criteria can't hold together as one. `[self.id=A world.mem.nh_stage=letters]` and `[self.id=B world.mem.nh_stage=letters]` are disjoint, and so are different `nh_stage` values. So one slot in `nh_story` carries a personal topic for every cast member at every stage.
3. **What stays in the story program:** quest offers and hand-ins (Q), steps that must appear and vanish per wolf mid-quest (O), phase greetings (L), scenes with faces (S), boards, notes and deeds (B).
4. **What stays out:** `mantle.assign` (it overwrites the wearer's binding and never restores it; C15). No resident's mantle is edited, because mantles are name-free and pass to newborns.

**The window map** (new nodes in `resident.tree`; existing nodes unchanged except one choice added to `news` and gated `say` lines added to the hubs):
```
hub (resident | watch | church greet | inn greet)    ← T greetings by self.id and stage
  └ news   + "What's all this talk of the Moot?" [world.mem.nh_stage=king|moot_called|reckoning|signals|howls|letters|lights|harvest|bell|moot]  -> nh_story
        nh_story   recap and the stage's news, by stage × dialect (G, X)
          ├ "Where could a wolf lend a paw?"          -> nh_help   (G: W lines by stage × region)
          ├ "Who's behind it all, do you think?"      -> nh_who    (G: V and ! lines by stage × dialect)
          ├ "Who ought I to ask about it?"            -> nh_ask    (G: A lines by stage × region)
          ├ [self.id=<cast id> world.mem.nh_stage=<that topic's stages>] personal topic -> nh_<who> (N: §3)  ← one slot, all cast
          ├ [self.id=<Keeper ids> self.mem.nh_keeper=on world.mem.nh_stage=<stages>] "I'd like to howl for this town." -> nh_pledge (N, story-heard; §3.20)
          ├ [self.id=rmx_dagfinn_thornham|rm_ottilie_harrowgate|rm_edric_kelpham] "I'd like to back you at the Moot."  -> nh_back (N, story-heard)
          │ [self.id=rm_gerrit_mudhand]                                        "I'd like to back the Moot itself." -> nh_back (the same slot: disjoint by self.id)
          ├ "I was wondering about something else..." -> hub
          └ "Goodbye for now."                        -> bye
```
`nh_story` can show eight choices at most (help, who, ask, the cast slot, pledge, back, and the two ways out). The pledge and back lists no longer share a resident (the Moot backing moved from the herald to the Speaker, and the Speaker is no longer a fallback Keeper), but either can overlap the cast slot, so the loader may count them as three groups; eight is under the nine-choice cap either way. `news` goes from eight to nine.

---
## 2. (a) The cast sheet

**Who is cast.** Every named character is an existing **placed, living** resident except the three strangers in §2.4. No resident is made or brought in (births match deaths). Dialect is the resident's `npc_voice.home_region`, which sets their grammar; it is often not the town they live in (the "Dialect" row says both). Protection is `npc.protect` at Ch 1's setup plus C11 `npc.spare` (or, until C11, the DM's authored `protected` flag for the old; campaign.md §8.6). "Tree" is the dialogue tree their **talk** entry opens, which is where their greeting lines (T) go.

**Corrections to campaign.md found while casting** (each is applied in this file):
1. **Gorm Plankgate works at the Sump Infirmary** (`rm_the_sump_infirmary`, door `w_ridgemere_0_0` (249, 250)), not the Abbey Infirmary; his work label still says "infirmarian of the abbey". His Ch 4–9 scenes and hand-ins move there.
2. **Fell Miremore speaks plain** (`home_region ridgemere`, mantle rm105) although Mirebridge-blooded and living in Coldwater. His Scattered lines are plain.
3. **Cressida Cotton is cold and frank** (blunt, suspicious), not warm; she is Frosthollow-raised (fh115). Her lines are clipped.
4. **The look-alikes** (decisions §1.6). A `like:` resident is never cast, never gives a pack quest, and lives where the stranger never walks.
   - **The Envoy:** `like: tn_sm_brisk_plumby` (Brisk Plumby, f 52, a steady silver arctic wolf, pot hand at the Vine and Sickle in Southmeadow). The Envoy's road (the Pass House, Coldwater, the city, the Bay Light, Market Square) never reaches Southmeadow. `rm_cressida_carlisle` (a sandy timber body) and `cty_ulla_rusthand` (Oakhaven's Keeper, so cast) are no longer look-alikes anywhere.
   - **Hakon:** `like: rm_palle_bilgemarsh` (Palle Bilgemarsh, m 35, a steady silver arctic wolf, preserver on Market Square). Hakon walks only the Greenholt ridges and the Pass, and never comes to the city. He is no longer drawn on Ulric, so the brothers are not twins.
   - **The Grey Captain:** `like: cty_stone_sunton` (Stone Sunton, m 44, a charcoal piebald arctic wolf like Eskar, cowherd at Southgate), as before.
   - Checked read only in `ratw_ridgemere3` on 2026-11-22: all three are placed and alive, with reachable homes and work places.
5. **The Counting House clerks live in the Chancellor's house.** Rune Gullwell, Ivar Pitchwick and Oskar Gullworth all live at Ashdown House. It is used: the clerks watch him.
6. **The Greatly Bank flat is the moneychanger's home.** Eleanor Saltcot lives in `rm_the_greatly_bank_flat`; the Ch 8 search there needs her leave, which is why her chain "Let me in" exists.
7. **Corra Coalcot carries the list hook herself** ("keeps a secret list of which Houses pay the Watch"). She sells fish-oil fakes, but her own real piece is in her hood: whoever reports her (Ch 3) is handed it by Bettany.
8. **Dagfinn and Cressida both have a notched left ear.** It is a patrol-pair joke in their lines ("matching ears").

### 2.1 Heroes

#### Dagfinn Thornham, the lamp-keeper
| | |
|---|---|
| Id | `rmx_dagfinn_thornham`; holds `job:rm_gunnar_harrowgate` ("reigning at the Great Palace") since day 46 |
| Sex, age | male, 31 |
| Home, quarter | Thornham House, the Commons and the Rise; the Sump round at dusk; quarter `works` |
| Dialect | **mirebridge** speech (Mirebridge-born, Fenshore-raised; mantle mb131). Tree: `watch` (role guard) |
| Temperament | calm; patient and easily offended; "knows which streets flood first" |
| Looks | compact russet arctic wolf, notched left ear; still in the Watch patrol harness with a lamp-hook on it; never a circlet |
| Voice | slow and level, fen tags ", no?" and "fen-sure"; says "the round" where others say "the realm"; goes quiet, not loud, when offended |
| Motive | keep the lamp he was handed and the round he walks; not be the wolf the Houses blame |
| Arc | Ch 1 found on his own round, lights the lamp at dusk, "I didn't ask for a crown". Ch 2 the factors chalk doors on his round. Ch 3 asks the wolves to douse lamps he can't reach. Ch 4 howls the fen howl at the feast. Ch 5 his own letter (Gunnar's note on the back). Ch 6 walks to the fens; the mother's door-stone. Ch 7 frets on the Steps while others hold the ford. Ch 8 hears the writ: "for someone like me". Ch 9 stands or steps aside; reads nothing, howls at the Bay Light. **Offers** (decisions §1.4): Ch 1 the round chain + the Lamp backing; Ch 2–8 the Question (`nh_dq`) + the backing; Ch 9 the backing only |
| Fates | **Lamp-King** (Lamp wins); **the Queen's Lamplighter** (Blood); **steward of the Moot** (Regency); **the wolf at the door** of the Bought Crown, "Next Midwinter, howl louder" |
| Protect | protect + spare |

#### Ottilie Harrowgate, the blood heir
| | |
|---|---|
| Id | `rm_ottilie_harrowgate` |
| Sex, age | female, 39 |
| Home, quarter | the King's Residence, the royal bedchambers; quarter `office`; walks to the City Office every week with the Crown's petitions |
| Dialect | plain (rm541). Tree: `resident` |
| Temperament | calm, warm; gentle and loyal; coughs in the damp and pretends not to |
| Looks | stocky cream red wolf, amber eyes; a petition satchel on a strap, ink on one paw |
| Voice | gentle, exact, short sentences; never says "my father the King", only "my father"; a cough mid-line she talks over |
| Motive | the petitions answered, whoever wears what |
| Arc | Ch 1 in mourning, the petitions unanswered. Ch 2 Ambrose's loan offer is laid before her (a chain). Ch 4 the cough, tended or not. Ch 5 her petitions are among the forty letters. Ch 8 her mother's choice, made over her head or with her. Ch 9 the Blood candidate |
| Fates | **Queen** (Blood; bare-headed in the rain if tended); the Lamp-King's first councillor; the Regency's first petitioner; under the Bought Crown "still carrying petitions, to a Protector now" |
| Protect | protect + spare |

#### Brenna Abbot, the herald (Ridgemere's Keeper)
| | |
|---|---|
| Id | `rmx_brenna_abbot` |
| Sex, age | female, 52 |
| Home, quarter | Abbot House, the Commons and the Rise; works in the Antechamber `r2_the_antechamber`; quarter `commons` |
| Dialect | plain (rm136). Tree: `resident` |
| Temperament | content, calm, warm; unhurried; takes the long stairs two at a time; a limp that comes and goes with the weather |
| Looks | rangy brown red wolf; the herald's staff slung across the back, a horn on a cord |
| Voice | the herald's cadence ("Hear the Palace."), warm when off the steps; counts things in threes |
| Motive | proclaim the law truly; pay her own note without selling her voice |
| Arc | Ch 1 reads the lamp-book line, calls the Moot, hands out the Summons. Ch 2 a debtor herself (triple). Ch 3–8 every world proclamation (each a signed `board.post`, plus her own window line), takes Ridgemere's pledges. The Moot's backings are the Speaker's (§3.8). Ch 8 reads the writ. Ch 9 reads every voice as it comes in. **Offers:** Ch 1 the Summons + the pledge; Ch 2–8 the pledge (+ the Ch 4 howl quest); Ch 9 the pledge + the Moot rule; Ridgemere's Ch 9 post quest is Cressida's waystation. Never more than 2 a chapter (decisions §1.4) |
| Fates | **Speaker of the Moot** (Regency, figure lord); herald in every other ending; under the Bought Crown she reads the Protector's first order "in her plainest voice" |
| Protect | protect + spare |

#### Cressida Cotton, the patrol partner (Ch 1 giver, waystation keeper)
| | |
|---|---|
| Id | `rmx_cressida_cotton` |
| Sex, age | female, 19 |
| Home, quarter | Cotton House, Northgate; the Sump patrol; quarter `commons` |
| Dialect | **frosthollow** (fh115). Tree: `watch` |
| Temperament | cold, frank; blunt and suspicious; a wet-stone whetting knife always on her |
| Looks | stocky russet red wolf, notched left ear (like her partner's) |
| Voice | clipped; "deep" for very, "cold iron", "stone" for friend; never thanks anyone twice |
| Motive | stand by her partner; secretly, a berth on a Grayrock ship bound somewhere dry |
| Arc | Ch 1 sends the wolves to find him. Ch 2–8 keeps his round when he's on the Steps; short lines. Ch 9 keeps the waystation post |
| Fates | Dagfinn's sergeant in every ending except the Bought Crown, where she "signs on the Grey Tern" |
| Protect | protect + spare |

#### Bettany Nailley, the honest sergeant
| | |
|---|---|
| Id | `rm_bettany_nailley` |
| Sex, age | female, 26 |
| Home, quarter | Pitch Court House, North Headland; the Watch Office `r2_the_keep`; quarter `sump` |
| Dialect | plain (rm483). Tree: `watch` |
| Temperament | ambitious, calm, warm; gentle; remembers every debt, owed or owing; lost a brother on the wharf and blames Grayrock |
| Looks | small, quick russet arctic wolf with a limp that comes and goes |
| Voice | quiet and straight; uses "the paper" for any evidence; says Grayrock's name like a stone in the mouth until the wharf names are read |
| Motive | an honest Watch |
| Arc | Ch 2 hints. Ch 3 the lists' first destination. Ch 5 the wharf names (softens). Ch 6 the smuggler. Ch 7 rides with the ford escorts |
| Fates | leads the Watch (figure captain) if the list reaches her; "on the Harbour Gate now" if Greatly bought the list; forgives Grayrock if the names are read |
| Protect | protect + spare |

#### Rosalind Greatly, the whistleblower
| | |
|---|---|
| Id | `rm_rosalind_greatly` |
| Sex, age | female, 22 |
| Home, quarter | Greatly House, the gardens (Celestine's apprentice); quarter `estate`. If turned out: sleeps at `r2_abbey_quarters` |
| Dialect | plain (rm417). Tree: `resident` |
| Temperament | bookish, wry, quietly stubborn |
| Looks | compact grey timber wolf, amber eyes; a ledger always under one foreleg |
| Voice | dry, numerical ("three sums"), quick; never calls Ambrose "father" in front of a stranger |
| Motive | the ledger that does not add up |
| Arc | Ch 2 gives the ledger. Ch 2 result: turned out, silenced or trusted. Ch 8 (if burned) names the site Oskar feared. Ch 9 the Purse candidate |
| Fates | Greatly's conscience; the unwilling Bought Queen; at the abbey, copying for Solveig |
| Protect | protect + spare |

#### Gerrit Mudhand, Speaker of the Council (the Moot's patron)
| | |
|---|---|
| Id | `rm_gerrit_mudhand` |
| Sex, age | male, 55 |
| Home, quarter | Greycombe House; the Antechamber; quarter `office`. Apprentice: Desmond Farrow |
| Dialect | plain (rm310). Tree: `resident` |
| Temperament | chatty, warm; curious; swears by the forge and the sea; saw the Chancellor weep and mourned the King |
| Looks | tall black arctic wolf, dark saddle |
| Voice | big and friendly, "by the forge and the sea"; procedural when nervous ("the Council notes that") |
| Motive | keep the Houses at one table |
| Arc | Ch 1 objects to the lamp-book. **Ch 2–9 the Moot's patron**: as keeper of the Moot's roll, he takes the wolves' Moot backings (the Regency's road; decisions §1.4). Ch 2 receives the ledger (if shown). Ch 8 a key witness. Ch 9 keeper of the roll. **Offers:** the Moot backing only (plus story-heard nodes, which are not offers); at most 2 a chapter. He is no longer Ridgemere's fallback Keeper (that is Corra Coldridge, §2.3) |
| Fates | Speaker of the Council in every ending; cut to an advisory voice under the Regency, and relieved |
| Protect | protect + spare |

#### Ulric Stonevein, Frosthollow's Keeper
| | |
|---|---|
| Id | `cty_ulric_stonevein` |
| Sex, age | male, 30 |
| Home, quarter | Frosthollow Cottage 1; digs ore at the Frosthollow mine |
| Dialect | frosthollow (fh102). Tree: `resident` |
| Temperament | short, warm; loyal and proud; collects smooth stones from the riverbanks; lost a sibling to a hard winter and "still watches the road" |
| Looks | tall brown arctic wolf with ink-stained forepaws (he writes the mine's tallies) |
| Voice | few words, "cousin", "stone", "lamp and pick"; stops mid-sentence when the brother comes up |
| Motive | find out what became of Hakon |
| Arc | Ch 3 the harness and the scent. Ch 8 the lamp-oil list and Fenwick's confession. Ch 9 the Turn at the Pass |
| Fates | parts from Hakon at the Pass (turned); watches him led away (beaten); mourns him (dead); never knows (fled) |
| Protect | protect + spare |

#### Tansy Reedmere, the pup the water obeys (Quickened)
| | |
|---|---|
| Id | `tn_cw_tansy_reedmere` |
| Sex, age | female, 14 (adolescent) |
| Home, quarter | Glasswater Cottage, the farmhouse at Glasswater Landing (with Gorse and Bramble Reedmere) |
| Dialect | coldwater (cw127). Tree: `resident` |
| Temperament | content, calm, frank; unhurried and honest to a fault; hums old marching songs |
| Looks | broad-shouldered cream red wolf, notched left ear |
| Voice | a pup's plainness with lake words ("minnow", "still"); hums instead of answering; lies badly |
| Motive | hide that the water comes when she calls |
| Arc | backstory: the "something big" under the ice. Ch 4 one line (hums at the chorus). Ch 8 taught, given up or hidden |
| Fates | the realm's first kept Quickened (taught); pledged to the Wardens' school "in spring" (pledged or found); hidden, and the black lane stays in the ice |
| Protect | protect + spare |

#### Solveig Tarwell, priest of the Chapel of the Tide
| | |
|---|---|
| Id | `rm_solveig_tarwell` |
| Sex, age | female, 44 |
| Home, quarter | Gorringe House, the Wharf and the Sump (shares it with Jasper Bilgeford and Dagny Rustcot); the Chapel of the Tide; quarter `office` |
| Dialect | plain (rm549). Tree: `church` (job priest) |
| Temperament | quiet, content, even; dreamy and meticulous; counts the chapel bells |
| Looks | long-limbed russet slender Ethiopian wolf, braided cord at the throat; carries a Grayrock letter |
| Voice | low, plain, unhurried; speaks of the drowned as "ours" |
| Motive | the drowned deserve a rite |
| Arc | Ch 5 the names on the water. Ch 6 the Rite of the Tide at the Mirebridge crossroads. Ch 9 rings the drowned bell at the Bay Light |
| Fates | rings the bell as the howl comes in (bell rung); rings the chapel bell alone (bell lost) |
| Protect | protect + spare |

#### Skye Eelbank, Mirebridge's Keeper
| | |
|---|---|
| Id | `tn_mb_skye_eelbank` |
| Sex, age | female, 50 |
| Home, quarter | Withy Creek Cottage, the Sallow Fens; traps eels at Withy Creek Eel House |
| Dialect | mirebridge (mb105). Tree: `resident` |
| Temperament | chatty, warm; loyal and curious; collects smooth stones; "the best eel trapper on the river, and says so" |
| Looks | stocky dun maned wolf with soot on the paws |
| Voice | quick, boastful, kind; "fen-sure", "tadpole", ", no?"; counts paths in reed-turns |
| Motive | keep the paths and the names |
| Arc | Ch 4 teaches the first turn of the Mirebridge howl. Ch 6 the path with no note; named Keeper at the rite. Ch 9 Lanterns for the Drowned |
| Fates | Mirebridge's voice at the Moot; silent with the Council casting it (lights faded) |
| Protect | protect + spare |

#### Gorm Plankgate, the Wardens' Eye
| | |
|---|---|
| Id | `rm_gorm_plankgate` (apprentice `rmx_dunstan_plankham`) |
| Sex, age | male, 55 |
| Home, quarter | Cobbett House, Northgate; works at **the Sump Infirmary** `rm_the_sump_infirmary`; quarter `commons` |
| Dialect | **saltwell** (sw117). Tree: `resident` |
| Temperament | cold, guarded; suspicious and honest to a fault; hums sea songs; lost a brother on the wharf |
| Looks | steady cream maned wolf, burrs forever in the ruff |
| Voice | the oasis formality turned clinical ("honoured guest", "by salt and water"); asks exact questions and waits |
| Motive | no Quickened pup drowns untaught again (the Saltwell well) |
| Arc | Ch 4 the cough, "let me look at your paws". Ch 5 revealed as the one who wrote to Upper Accord. Ch 6 the Envoy's Question. Ch 8 the Tansy choice. Ch 9 the Envoy's three questions and the verdict |
| Fates | keeps a Warden House at the abbey (Realm Answers, Tansy taught); the Wardens' Eye under a Warden at the Grey Gull (Holds); the Concord's registrar (Bends) |
| Protect | protect + spare. **DM prep:** faction `warden_order`, Gorm a ranked member, before Ch 5's end |

### 2.2 Villains and rivals

#### Ambrose Greatly, master of the Counting House (the purse)
| | |
|---|---|
| Id | `rm_ambrose_greatly` |
| Sex, age | male, 63 (old-age risk) |
| Home, quarter | Greatly House, Northgate; the Greatly Counting House; quarter `estate` |
| Dialect | plain (rm476). Tree: `resident` |
| Temperament | courteous, patient, utterly without mercy in business; never raises his voice and never forgets |
| Looks | rangy grey arctic wolf, amber eyes that miss little |
| Voice | soft complete sentences, ledger words ("owed", "carried", "settled"), never an oath; compliments that are threats |
| Motive | "his heir to inherit a city, not a fortune" |
| Arc | Ch 1–2 chalks the doors, buys howls. Ch 3 owns the Watch or doesn't. Ch 5–6 funds the bounders and the cudgels. Ch 7–8 his promise to the Captain shown false; the writ he made Oskar hide. Ch 9 sits at the Moot "having counted his" |
| Fates | shamed before the Council (ledger shown or note printed); owed a favour and abstaining (kept); **Lord Protector** (Bought Crown). Never fought: beaten by ledgers and votes |
| Protect | protect + spare |

#### The Grey Captain, Eskar Rimedrift of Deepvein (the roads) — stranger and foe
| | |
|---|---|
| Spec | `cty_eskar_rimedrift` is **unplaced** and is never given a body. He appears as: Ch 7 parley stranger `visitor.add {visitor:"captain", name:"the Grey Captain", like:"cty_stone_sunton", description:"a tall black arctic wolf with pale, nearly silver eyes, lamp-black on his paws, a grey cloak", minutes: 120}`; Ch 9 C10 `foe.add {foe:"the-grey-captain", name:"the Grey Captain", like:"cty_stone_sunton", strength:"champion", band: 6 (+2 if the ford burned)}` at the Pass post; and, **if beaten**, the same `visitor.add` (id `captain`) at the Watch cells `r2_the_keep_down` for one game day after the Turn (`minutes: 1440`, §3.24). His words come through **letters** (B: board posts signed "a grey paw-mark", one letter read by Rook), **Desmond Hollis** in scenes (S), and, with T3, his own scene panel. In the Turn he says nothing in narration: region lines (Nr) only describe him |
| Sex, age | male, 44 |
| Dialect | frosthollow, broad ("'Arth", "dead rock", "deep"). Written in his letters with the broad forms spelled out, since letters are not filled by grammar |
| Temperament | cold, exact, patient; a ledger-keeper of grievance; keeps his word to the letter |
| Looks | tall, black arctic, pale nearly-silver eyes, lamp-black paws, grey cloak; carries a signal lamp |
| Voice | counts in lamp oil and winters; "Warm hearth, they say up there. Not for Deepvein wolves." |
| Motive | buy Deepvein back for the miners Frosthollow turned out in the deep winter |
| Arc | Ch 1 the Alder Mere camp, the first paw-mark notice. Ch 2 buys wagon days. Ch 3 the ridge lamps; named. Ch 4 false relay howls. Ch 5 bounders in Fell green. Ch 6 the southern company's float lamps. Ch 7 parley and siege. Ch 8 the Pass notice. Ch 9 the Pass fight and the Turn |
| Fates | **turned** (back over the Pass with Hakon, to bring the Deepvein families down in spring); **beaten** (a stranger held one game day at the Watch cells, a notice on `board:ridgemere`, "The Grey Captain is held at the Keep, to be judged at the Moot's end"; the judgement is board and talk only, with no resident body and no `npc.hold`; decisions §9, P1-12); **dead** (only by "no quarter"); **fled** (over the Pass with his coin; the sequel's villain). Never placed, never a resident |

#### Desmond Hollis, the dismissed guard (the Captain's inside wolf)
| | |
|---|---|
| Id | `rmx_desmond_hollis` (no post since the lamp passed) |
| Sex, age | male, 33 |
| Home, quarter | Hollis House, Northgate; drinks at the Knucklebones; quarter `commons` |
| Dialect | **coldwater** (cw197). Tree: `watch` (role guard, though postless) |
| Temperament | short, reserved, sly; proud and shrewd; watches the rain as if it might stop; saw the Chancellor weep |
| Looks | lean cream timber wolf, white blaze down the chest; the Palace guard's cloak with its badge cut off |
| Voice | short, bitter, lake words used like knives ("muddied", "eh?") |
| Motive | he lost his Palace post when a Sump patrolman took the lamp |
| Arc | Ch 2 buys wagon days at the Carters' Yard (his words there are in his own window, §3.16; no `npc.speak`). Ch 3 held or fled. Ch 7 leads the river company (fled) or is tried (held). Ch 8 a witness in the cells (held) |
| Fates | held → mercy → redeemed at the granary; the pillory; fled → downed, never killed, at the ford (C10 `foe.outlaw`) |
| Protect | **never protected** (C10 must be able to down him) |

#### Queen Oda Harrowgate, the passed-over Queen
| | |
|---|---|
| Id | `rm_oda_harrowgate` |
| Sex, age | female, 60 (old-age risk) |
| Home, quarter | the King's Residence; quarter `office` |
| Dialect | **coldwater** (cw210). Tree: `resident` |
| Temperament | restless, sly; shrewd; remembers every debt, owed or owing; counts the purse coin by coin |
| Looks | stocky sandy arctic wolf, pale nearly-silver eyes; the household purse on a chain |
| Voice | lake words in a palace ("eh?", "muddied", "a fair catch"); bargains in every sentence |
| Motive | the household purse, and her daughter's place |
| Arc | Ch 1 the Queen's Purse. Ch 2 the ear in her own kitchens. Ch 8 the Queen's Choice |
| Fates | reconciled Queen Mother (told_queen or contest, Blood wins); keeper of the roll beside Gerrit (accept); "home to the lake in spring" otherwise |
| Protect | protect + spare |

#### Supporting rivals (compact)
| Name, id | Sex, age | Home / work | Dialect (mantle), tree | Temperament | Looks | Motive | Arc | Fates | Protect |
|---|---|---|---|---|---|---|---|---|---|
| Corwin Fell `rm_corwin_fell_` | m 31 | Fellhall; the Fell cellars | plain (rm491), resident | loud, proud, generous with others' coin | steady brown maned wolf, broad as a wine tun | the Rainward Hills for Fell | Ch 5 claim, bounders, the wager; Ch 6 the quarrel | humiliated; the split; granted and Oakhaven's lasting enemy | — |
| Lorn Hedgeton `cty_lorn_hedgeton` | m 58 | Greyteeth Farmhouse, the Larkdowns; the shearing barn | coldwater (cw110), resident | ambitious, cold, guarded; never sits with his back to the door | solid cream timber wolf, grey-frosted muzzle | coin for cart days | Ch 3 the informer | shamed; turned (gives the lamp code) | — |
| Fenwick Plankworth `cty_fenwick_plankworth` | m 54 | the Mine Captain's House, Frosthollow | **coldwater** (cw162; Coldwater-born), resident | restless, sly; shrewd; "family far away who never write" | stocky russet timber wolf, braided cord | save his mine any way he can | Ch 2 the seam note; Ch 4 Highmere's silver; Ch 8 the confession | honest and confessed (a key to the Turn); crooked and named aloud by the Captain | protect + spare |
| Hilde Dripby `rm_hilde_dripby` | f 61 | the Barracks, North Headland; a Rag Mill hand | plain (rm518), resident | short, sly; carries a whetting knife | lean russet arctic wolf, pale eyes | coin for what she saw | Ch 8 the whisper and the race | paid off; beaten to the Chancellor's door | — |
| Corra Coalcot `rm_corra_coalcot` | f 22 | Forge Row, the Wharf and the Sump; runs the Fish Hall | plain (rm325), resident | chatty, merry, sly; never out without an oilcloth hood | lean black timber wolf, burrs in the ruff | coin | Ch 3 sells fish-oil lists; the true thief behind the Kendrick rumour | reported (her real list piece goes to the reporter); gossiped about | — |

### 2.3 Key and turnable NPCs

#### Oskar Gullworth, the weeping Chancellor
| | |
|---|---|
| Id | `rm_oskar_gullworth` |
| Sex, age | male, 40 |
| Home, quarter | Ashdown House (with the Counting House clerks Rune Gullwell and Ivar Pitchwick); the City Office `rm_the_city_office`; quarter `office` |
| Dialect | plain (rm344). Tree: `resident` |
| Temperament | content, calm, cold, guarded; unhurried and suspicious; never out without an oilcloth hood; Sump-born, means to die in the Heights |
| Looks | compact silver maned wolf, white blaze; the hood up indoors |
| Voice | clipped, official, "the City Office notes"; cracks into Sump plainness when cornered |
| Motive | keep his post and his debt secret; and, underneath, be forgiven |
| Arc | Ch 1–7 small lines that say too much. Ch 8 the confrontation |
| Fates | confessed (humbled, keeps his post); exposed (the note printed); fled (held at the Mirebridge crossroads; "last scented where Mirebridge stood"); paid (Hilde's silence bought) |
| Protect | protect + spare |

#### Corra Slagmore, Captain of the Watch
| | |
|---|---|
| Id | `rm_corra_slagmore` |
| Sex, age | female, 50 |
| Home, quarter | the Watch House; the Watch Office; quarter `heights` |
| Dialect | plain (rm490). Tree: `watch` |
| Temperament | quiet, calm, guarded; patient; takes the long stairs two at a time; owes a House factor by the new moon |
| Looks | compact dun arctic wolf, dark saddle; the captain's knot on her harness |
| Voice | as few words as will do; "not here" when the note comes up |
| Motive | keep the Watch whole and her note hidden |
| Arc | Ch 2 the Captain's Note. Ch 3 the lists. Ch 9 the Watch at the Moot |
| Fates | redeemed (note freed, list burned before her); stands aside (list to Bettany); resigns in honour or disgrace (printed); owned (list sold or lost) |
| Protect | protect + spare |

#### The Houses (compact)
| Name, id | Sex, age | Home / work | Dialect (mantle) | Temperament | Looks | Role | Fates |
|---|---|---|---|---|---|---|---|
| Celestine Greatly `rm_celestine_greatly` | f 58 | Greatly House; the salon | **saltwell** (sw124) | gracious, sharp-tongued, best-informed in the city | broad-shouldered black arctic, white blaze | the salon every House attends; casts Greatly's vote if the ledger is shown; stages the House wedding (peace kept) | protected by rule (House head) |
| Isolde Fell `rm_isolde_fell_` | f 58 | Fellhall | plain (rm526) | unbending, shrewd, cold until you're useful | lean russet timber, missing claw | Fell's vote; shuts Gudrun in (shown) | protected by rule |
| Gudrun Fell `rm_gudrun_fell_` | f 29 | Fellhall; the accounts | plain (rm514) | quiet, precise, more ambitious than her husband | steady brown red wolf, ink-stained forepaws | the secret Timberland letters; the match | match / shown (shut in 7 days) / ash (owes the wolf) |
| Osric Timberland `rm_osric_mossgrave` | m 27 | Timberland Court; rides out | **southmeadow** (sm132) | easygoing, kind, forever muddy | solid black maned wolf, dark saddle | the other half of the match | matched (Fell–Timberland friendly) |
| Signe Timberland `rm_signe_mossgrave` | f 61 | Timberland Court | plain (rm389) | warm, practical; the only one who can make Halvard rest | rangy silver arctic, one torn ear | Timberland's Council vote | protected by rule |
| Halvard Timberland `rm_halvard_mossgrave` | m 66 | Timberland Court gardens | plain (rm454) | gentle, slow-spoken, far sharper than he lets on | rangy silver timber, a weather limp | every Cinders smith would lie for him; vouches for the Turn's iron | spare (old age) |
| Wenna Timberland `rm_wenna_mossgrave` | f 34 | Timberland Court; the iron trade | plain (rm440) | brisk, ambitious, unafraid of the other Houses | long-limbed brown timber, notched left ear | owner of the cached iron bar (Ch 6) | — |
| Lord Aldric Grayrock `rm_aldric_grayrock_` | m 52 | Grayrock Hall; the Wharf Office; Lord Grayrock's road | plain (rm475) | restless, charming, hard as his own stone; trusts contracts more than wolves | steady black red wolf, dark saddle | the forty letters, the homecoming | crown / wharf / absent |
| Lady Maren Grayrock `rm_maren_grayrock_` | f 47 | Grayrock Hall | **larkfield** (lf162) | patient, sharp-eyed, lonely in a way she'd never admit | rangy silver timber, notched left ear | the toll bargain; the thyme | walks to the gate (2/3); two ships for the ford (bargained) |
| Ingrid Grayrock `rm_ingrid_grayrock_` | f 16 | Grayrock Hall, at her lessons | plain (rm525) | bright, bored, forever at the window | broad-shouldered black timber, dark saddle | Every Week, a Letter | names the wolf to her father at the Moot |
| Tobias Grayrock `rm_tobias_grayrock_` | m 23 | Grayrock Hall; the Bonded Warehouse | plain (rm554) | earnest, anxious, better with iron than with wolves | long-limbed russet timber, silver eyes, tar to the elbows | the Reluctant Heir (if Aldric isn't home by day 185) | casts Grayrock's vote if coached |
| Julian Greatly `rm_julian_greatly` | m 29 | Greatly House; the Greatly Yard's books; the Sailors' Rest | plain (rm530) | clever, restless, too fond of the Sailors' Rest | rangy dun maned wolf, grey-frosted muzzle | A Berth for Julian | sails on the Grey Tern "for spring" |
| Steward Edric Kelpham `rm_edric_kelpham` | m 51 | Greatly House, servants' hall | plain (rm502) | chatty, merry; quietly funny; a debtor (triple) | lean cream red wolf, coat kept immaculate | the day book; **the Purse patron** | freed and loyal to Rosalind; or the Protector's steward |
| Steward Millicent Rustwell `rm_millicent_rustwell` | f 42 | Fellhall, servants' hall | plain (rm536) | short; anxious; coughs in the damp | small quick grey arctic | truce tokens (Ch 6); wharf names | — |
| Steward Lavinia Plankwick `rm_lavinia_plankwick` | f 55 | Grayrock Hall, servants' hall | plain (rm434) | chatty; loud; trusts no one from a rival House; saw the weeping | steady grey timber, dark saddle | casts Grayrock's vote with Greatly (absent); a witness | — |
| Steward Hollis Nailwick `rm_hollis_nailwick` | m 47 | Timberland Court, servants' hall | plain (rm522) | short, warm; easily offended, loyal | lean grey red wolf, notched left ear | truce tokens | — |

#### The money trail, the Watch lists, the household (compact)
| Name, id | Sex, age | Where | Dialect | Temperament | Part | Protect |
|---|---|---|---|---|---|---|
| Karin Stanmore `rm_karin_stanmore` | f 33 | Stanmore House; the Administrative Wing (treasurer) | plain | calm, cold, guarded; gentle; swears by the forge and the sea | list favour-keeper (the customs tally) | — |
| Abbot Lionel Ropestack `rm_lionel_ropestack` | m 46 | Anchor Yard House; the Cathedral (purse 4p) | plain | content, calm; easily offended; remembers every debt | list favour-keeper (the Restday service); the crypt's leave (Ch 8) | — |
| Desmond Dripcot `rm_desmond_dripcot` | m 40 | Net Court House; keeps the cells (`r2_the_keep_down`) | plain | short, frank; honest to a fault | list favour-keeper (a night at the cells); who sold the Captain's note | — |
| Desmond Farrow `rmx_desmond_farrow` | m 19 | Farrow House; usher, the Antechamber; Gerrit's apprentice | plain | quiet, calm; patient, tired; a pocketful of coal for luck | list favour-keeper (a note by night) | — |
| Corra Coldridge `rmx_corra_coldridge` | f 21 | Weavers' Terrace, Northgate; usher, the Antechamber; the herald's apprentice | plain (rm209, light) | chatty; loud, curious; grew up at a Timberland charcoal camp; a steady dun arctic wolf, grey-frosted muzzle | receives Farrow's note; mourned the King; **Ridgemere's fallback Keeper** (takes the city's pledges only if the herald dies or is not named; decisions §1.3) | protect + spare |
| Mab Coalridge `rm_mab_coalridge` | f 30 | Kiln Row; the Residence kitchens (purse 218p) | plain | nervy, cold, guarded; trusts no one from a rival House | the ear in the Residence | — |
| Percival Rustford `rm_percival_rustford` | m 53 | Barrow Court House; the King's household | plain | short, sly | weep witness of the household | — |
| Eleanor Saltcot `rm_eleanor_saltcot` | f 48 | the Greatly Bank and the flat above | plain | chatty, ambitious, merry; grew up at a Timberland charcoal camp | confirms the sums (Ch 2); the Purse till; "Let me in" (Ch 8) | — |
| Jorunn Ropewick `rm_jorunn_ropewick` | f 43 | the Pawn and Ledger | plain | quiet, dry, short; remembers every debt; saw the weeping | the factors' notes | — |
| Jasper Bilgeford `rm_jasper_bilgeford` | m 47 | Gorringe House; the Customs House | plain | quiet, dry, calm, warm; hums sea songs | the customs tally (Ch 2, Ch 3) | — |
| Rune Gullwell `rm_rune_gullwell` | m 38 | Ashdown House; Counting House clerk | plain | cold, guarded; smells of smoke | the chalk walker; the Purse errands | — |
| Ivar Pitchwick `rm_ivar_pitchwick` | m 20 | Ashdown House; Counting House clerk | plain | chatty, content, calm; coughs in the damp; lost a brother on the wharf | the second clerk; a wharf name (Ch 5); Siege Price (Ch 7) | — |
| Dagny Ropeley `rm_dagny_ropeley` | f 42 | Rag End; sergeant, the Watch Office | plain | quiet, content, calm, guarded; carries a Grayrock letter | Ch 3 giver (the most suspected wolf) | — |
| Dagny Rustcot `rm_dagny_rustcot` | f 47 | Gorringe House; harbourmaster, the Grayrock Wharf Office | **coldwater** | quiet, content, calm | which road Lord Grayrock took | — |
| Ingrid Ashwell `rm_ingrid_ashwell` | f 47 | Tar Corner; the Council Press | plain | quiet, warm, guarded; lost a brother on the wharf | prints the list (press); a wharf name | — |
| Rune Fogworth `rm_rune_fogworth` | m 51 | keeps the Council Press | plain | quiet, content, guarded | prints Ambrose's note (exposed); a wharf name | — |
| Palle Plankmore `rm_palle_plankmore` | m 31 | Oakum Stair House; the Cinder Gate | plain | quiet, dry, frank | the gate the grey paw-mark pays | — |
| Mathis Slagwell `rm_mathis_slagwell`, Oda Greywell `rm_oda_greywell`, Piet Wickridge `rm_piet_wickridge` | m 48, f 49, m 51 | porters on the Sump Stairs | plain | warm / warm (keeps a list) / short, frank | where the lamp-keeper passed (Ch 1); Mathis saw the seal (Ch 8) | — |
| Ingrid Kendrick `rmx_ingrid_kendrick` | f 68 | Kendrick House, Northgate | plain | quiet, content, merry; takes the stairs two at a time | the most suspected wolf (803 believe it) | spare (68) |
| Piet Lumley `rmx_piet_lumley` | m 60 | Saltgate Row; lights the lamps at dusk | plain | ambitious, short, reserved; proud | a triple debtor; gives no pack quest (campaign cast). In medium `mq-sump-lamps` (Edda Gullford's) he is only the conversation target, and his confession is his own tree line gated on that quest | spare (60) |
| Hanne Coalton `rm_hanne_coalton` | f 40 | Net Court House; the Harbour Gate | **mirebridge** | quiet, content, dry; a petition lost in the City Office | a petitioner answered (Ch 1–3); where Bettany goes if the list is sold | — |
| Bertil Grantham `rm_bertil_grantham` | m 11 | Grantham House | plain | quiet, merry, guarded; lost a brother on the wharf | a wharf name (a pup's) | — |
| Imogen Nailridge `rm_imogen_nailridge` | f 31 | Soundings `rm_soundings` (lives above) | plain | plain, ambitious, even; meticulous | The Scattered; takes Mirebridge pledges until Skye is named | protect + spare (fallback keeper) |
| Nan Ashhand `cty_nan_ashhand` | f 54 | Rain Rents (the city); quarrymaster at the Rainward quarry | plain | short, warm; generous, easily offended; never sits with her back to the door | What Nan Is Saving For | — |

#### The Greenholt and the ice (compact)
| Name, id | Sex, age | Where | Dialect | Temperament, looks | Part | Protect |
|---|---|---|---|---|---|---|
| Kell Greyton `tn_cw_kell_greyton` | m 30 | Upper Mere Cottage; skipper, Upper Mere Boat Shed | coldwater | calm, warm; distrusts the city; broad dun arctic, amber eyes | the bell's bearings and haul; Upper Mere post keeper | protect + spare |
| Willa Coldwick `tn_cw_willa_coldwick` | f 24 | Coldwater Cottage 7; carter | coldwater | plain, even, frank; meticulous, blunt; compact silver timber, scar across the nose | Willa's Wheel-Rut; the frame cart; Glasswater relay keeper (if home) | protect + spare |
| Myrtle Stillford `tn_cw_myrtle_stillford` | f 38 | Coldwater Stores | coldwater | content, calm, warm; swims in the lake even in autumn; lean russet arctic | Ch 1 giver; Coldwater's fallback Keeper | protect + spare |
| Fallow Lakewick `tn_cw_fallow_lakewick` | f 58 | Coldwater Loom Shed | coldwater | chatty, devout; loud; distrusts the city; carts to Ridgemere and resents every mile; broad black red wolf, torn ear | **Coldwater's Keeper** | protect + spare |
| Harrow Reedford `tn_cw_harrow_reedford` | m 56 | Coldwater Cottage 4; cook at the Glasswater Inn | coldwater | plain, even, warm; collects stones; lean cream red | Glasswater relay keeper (fallback); hot food for the haulers | protect + spare |
| Bramble Reedmere `tn_cw_bramble_reedmere` | f 56 | Glasswater Cottage; skipper at Glasswater | coldwater | chatty, content; loud, dreamy, remembers every name | of the twins' household; at the ropes | — |
| Gorse Reedmere `tn_cw_gorse_reedmere` | m 14 | Glasswater Cottage | coldwater | short, warm; easily offended; swims even in autumn; missing claw | Gorse on the Ice | protect + spare |
| Linden Harvestton `tn_sm_linden_harvestton` | f 57 | Coldwater Still | coldwater | quiet, calm, warm, guarded | owner of the cached wine (Ch 6) | — |
| Fell Miremore `tn_mb_fell_miremore` | m 51 | Coldwater Cottage 6; loom hand | **plain** | quiet, devout, guarded; swears the marsh lights are the drowned | one of the Scattered | — |
| Loam Pinefell `tn_fh_loam_pinefell` | m 52 | the Glasswater Inn (taproom) | coldwater | short, reserved; remembers every name; lost a brother in a mine collapse | innkeeper: recaps and helpful lines only | — |

#### Larkfield, the Larkdowns and the Sunfields (compact)
| Name, id | Sex, age | Where | Dialect | Temperament, looks | Part | Protect |
|---|---|---|---|---|---|---|
| Aspen Marshholm `tn_mb_aspen_marshholm` | f 55 | Larkfield Linen Shed | larkfield | chatty, merry, warm; never sits with her back to the door; sandy maned wolf, burrs | **Larkfield's Keeper**; the Night Runner chain; Mirebridge-blooded | protect + spare |
| Tansy Eelbridge, "the Larkfield smith" `tn_mb_tansy_eelbridge` | f 45 | Larkfield Smithy | larkfield | chatty, dry; curious, wry; "can find a path across any bog"; silver maned wolf, a weather limp | the Smith's Bog Path; Mirebridge-blooded. Always "the Larkfield smith" in story text | — |
| Rook Wheatfield `tn_lf_rook_wheatfield` | m 58 | Longmead Farmhouse; the granary | larkfield | chatty, warm; generous; always knows the price of wheat; russet timber, weather limp | harvest hand-ins; Rook's Word; reads the Captain's letter if Desmond is held. (The Short-Weighed Sheaves is no longer his: it is chapter 1 of the large line `lq-clipped-silver`, decisions §2.3) | — |
| Dun Harvestley `tn_sm_dun_harvestley` | m 46 | Larkfield Press | larkfield | dry, warm; remembers every name | the handbill; Larkfield's fallback Keeper | protect + spare |
| Thorn Marshditch `tn_mb_thorn_marshditch` | m 57 | Larkfield Cottage 3; pot hand at the Sheaf | larkfield | restless, cold, guarded; finds a path across any bog | one of the Scattered ("sings the old turn when he's had two") | — |
| Bramble Reedford `tn_cw_bramble_reedford` | f 45 | the Sheaf (taproom) | larkfield | quiet, devout, dry | innkeeper: helpful only | — |
| Pike Chalkley `tn_lf_pike_chalkley` | m 26 | Larkfield Stores | larkfield | quiet, content, calm; distrusts the city | **not campaign cast**: the giver of the large line `lq-clipped-silver` (its chapter 1 is the false market beam). Named here only because §4.4's Larkfield gossip points to "the stores keeper". Blankets to the ford are now `st-ford-blankets` (Quill Oakdell) | — |
| Iris Meadowfield `tn_sm_iris_meadowfield` | f 54 | Larkholm Farmhouse | southmeadow | content, calm, frank; never misses a harvest dance; "once walked all the way to Ridgemere and came back" | **Southmeadow's Keeper** | protect + spare |
| Briar Cornby `cty_briar_cornby` | f 41 | Southmeadow Cottage 1; cellar hand at the press house | southmeadow | dry, sly; quotes old sayings for everything; cream arctic, white blaze | The Stones Remember | — |
| Aster Cornley `cty_aster_cornley` | f 45 | Sunfields Cottage; keeps bees | southmeadow | restless, dry; quotes old sayings | honey for Briar; owner of the cached honey | — |
| Orrin Appleley `tn_sm_orrin_appleley` | m 39 | Southmeadow Pottery | southmeadow | quiet, content, calm, warm; walked to Ridgemere and back | the clay pot for Briar | — |
| Rusk Plummeadow `tn_sm_rusk_plummeadow` | m 30 | Southmeadow Stores | southmeadow | dry, frank; wants the biggest orchard in the south | Southmeadow's fallback Keeper; no pack quest (the Chorus That Sang Wrong is now `st-chorus-wrong`, Alder Blossomcroft's) | protect + spare |
| Rowan Downridge `tn_lf_rowan_downridge` | f 27 | the Vine and Sickle (taproom) | southmeadow | quiet, content; dreamy, loud | receiver for Rook's word; helpful only | — |

#### The fens, the oasis, Cedar Sound and the high valley (compact)
| Name, id | Sex, age | Where | Dialect | Temperament, looks | Part | Protect |
|---|---|---|---|---|---|---|
| Roan Pikewater `tn_mb_roan_pikewater` | m 53 | Fenshore Stores | mirebridge | ambitious, devout; distrusts the city; cream red wolf, torn ear | **Fenshore's Keeper**; one of the Scattered | protect + spare |
| Tamsin Eelbank `tn_mb_tamsin_eelbank` | f 70 | Withy Creek Cottage | mirebridge | devout, nervy; never sits with her back to the door; "the marsh lights are the drowned" | the lights; the path's second guide | spare (70) |
| Sloe Mistwick `cty_sloe_mistwick` | f 44 | Rivermouth Cottage; skipper | mirebridge | short, sly; stubborn; always a little late; a well feud | Sloe's Cargo | — |
| Ludo Beechshaw `tn_oh_ludo_beechshaw` | m 46 | Fenshore Pottery (holds the dead potter's post) | **oakhaven** | chatty, sly; distrusts anything from the city; carves birds and gives them away; silver maned wolf, notched ear | The Potter's Post | — |
| Nell Plummeadow `cty_nell_plummeadow` | f 53 | the Heron (taproom) | mirebridge | plain, even; meticulous, easily offended; a lost love's letter never answered | innkeeper: recaps and helpful lines only; her letter at the crossroads (Ch 9) is her one target role. **No longer a fallback Keeper** (no innkeeper is one, decisions §1.2.4) | — |
| Jory Corncroft `cty_jory_corncroft` | m 41 | Fenshore Stilt House 3; carter at Fenshore | **mirebridge** (fenshore-raised, broad; mb116) | ambitious, restless; argues with the weather; saving coin toward something nobody knows; a compact brown timber wolf with a limp that comes and goes | **Fenshore's fallback Keeper** (takes Fenshore's pledges only if Roan dies or is not named; decisions §1.3). Not a Keeper's howl giver unless he holds the post | protect + spare |
| Thistle Stonepick `tn_fh_thistle_stonepick` | f 36 | Saltwell Stores | saltwell | dry, calm; collects smooth stones; lost a brother in a mine collapse (Deepvein) | **Saltwell's Keeper** | protect + spare |
| Bramble Bogditch `tn_mb_bramble_bogditch` | f 78 | Saltpan Cottage | saltwell | devout, dry; distrusts the city; ink-stained forepaws | the Well's Roll; teaches the howl (never Keeper) | spare (78) |
| Oriel Marshditch `tn_mb_oriel_marshditch` | f 30 | the Oasis Inn (taproom) | saltwell | ambitious, frank; honest to a fault; never minds wet feet | innkeeper; one of the Scattered (gives names, not quests) | — |
| Lavinia Mudworth `cty_lavinia_mudworth` | f 54 | Redsand Warren House | saltwell | chatty, ambitious, merry; argues with the weather | Saltwell's fallback Keeper | protect + spare |
| Ulla Rusthand `cty_ulla_rusthand` | f 31 | Oakhaven Cottage 1; logger at the cut | oakhaven | warm, sly; counts things under the breath; lean silver arctic | the Wronged Logger; **Oakhaven's Keeper** if cleared | protect + spare |
| Cairn Oakhaven `tn_oh_cairn_oakhaven` | m 30 | Oakhaven Stores | oakhaven | plain, even, sly; meticulous; distrusts the city; carves birds | accuser in the Wronged Logger; Keeper if Ulla is not cleared | protect + spare |
| Moss Blossomwell `tn_sm_moss_blossomwell` | f 41 | Oakhaven Press and Paper Mill | oakhaven | cold, guarded; honest to a fault; counts under the breath | Oakhaven road post keeper | protect + spare |
| Tamsin Icefell `tn_fh_tamsin_icefell` | f 31 | the Felled Cedar (taproom) | oakhaven | nervy, frank; means to leave for Ridgemere | innkeeper; helpful only; "I'll leave after the Moot" if the hills are lost | — |
| Aster Coldfell `tn_fh_aster_coldfell` | f 35 | Frosthollow Stores | frosthollow | calm, sly; patient; means to go down to Ridgemere and never come back | Frosthollow's fallback Keeper | protect + spare |
| Thorn Ashwood `tn_oh_thorn_ashwood` | m 52 | the Lamp and Pick (taproom) | frosthollow | calm, warm; never sits with his back to the door; carves birds | innkeeper; hosts Fenwick's confession (Ch 8) | — |
| Mab Carlisle `rm_mab_carlisle` | f 55 | the Sailors' Rest, Ridgemere (taproom) | **larkfield** | short, frank; carries a Grayrock letter | innkeeper: the city's recap; Julian's drinking | — |

### 2.4 Strangers and foes (they come and go)
| Who | Sex, age | Spec | Looks | Voice | When | Leaves |
|---|---|---|---|---|---|---|
| **Warden-Envoy Ingeborg Rimewater** | f, about 60 | `visitor.add {visitor:"envoy", name:"the Warden-Envoy", like:"tn_sm_brisk_plumby", description:"a silver-muzzled arctic wolf in the Concord's black cloak with silver trim", minutes:1440}`, re-added daily where she is. Mute by Concord custom: **she speaks to a realm only through its own Eye** (Gorm) | silver-muzzled arctic, black cloak with silver trim, a slate and stylus on a strap | none of her own: Gorm gives her words in his own window and scene panel ("She asks this", "She writes it down") | Ch 5 end – Ch 9 | over the Pass in every ending; a Warden stays at the Grey Gull in "the Realm Holds" (talk only) |
| **The Grey Captain** | m 44 | §2.2 | §2.2 | letters, Desmond's scene panel, and (T3) his own panel | Ch 7 (one night), Ch 9 (one night); if beaten, one more game day at the Watch cells | over the Pass; or, beaten, gone from the cells after a day, judged on the board and in talk |
| **Hakon Stonevein** | m, about 34 (Ulric's elder brother) | `visitor.add {visitor:"hakon", name:"a grey-cloaked wolf", like:"rm_palle_bilgemarsh", description:"older and greyer than he should be, lamp-black on his paws, a grey cloak", minutes:360}`; Ch 9 a C10 foe in the Captain's band | lean and greyer than his years, lamp-black paws (Palle Bilgemarsh's silver arctic build, never Ulric's) | one sentence in the Turn, in his own scene panel with T3 (Phase 7); before T3, Ulric's panel carries the moment (§3.24) | Ch 3 ridges by night, Ch 8 Pass Road, Ch 9 the Pass | back over the Pass in every fate |
| The companies, bounders in Fell green, House cudgels, the river company | — | `story.area.spawn`, `bandits.call` (anonymous bandits); C10 bands later | — | none (bark-free) | throughout | cleared or scattered |

---
## 3. (b) Dialogue paths

**How to read a character's section.** `T` lines are their greeting at each stage (tree hub of their role, `priority=4`: above "wary" 3 and below "famous"/"returning" 6, so a wolf's own fame still greets them first). `N` blocks are tree nodes, reached from the `nh_story` slot `[self.id=<their id> world.mem.nh_stage=<their stages>]`; the choice that opens them is given first. **Story-heard** marks a node whose `dialogue node` event a chapter trigger counts (what it does is in the bracket). `Q` blocks are quest offers; `O` lines are the "during" step (`npc.offer {player:$actor, once:true}` added on `quest accepted`, which needs J1 to stay per wolf; C6 `offer.during` replaces them). `P` lines are per wolf (C8 pair scope, or C5 `npc.greet` where noted; §5.3 lists every pair key with its write and fallback). No resident's words go in a `story.tell` banner (§0.2). Every N node also carries the two ways out ("I was wondering about something else..." → `hub`, "Goodbye for now." → `bye`) unless the node lists its own; they are not repeated below.

### 3.1 Dagfinn Thornham (mirebridge speech; tree `watch`)
**T, by stage** (`say[self.id=rmx_dagfinn_thornham world.mem.nh_stage=<key> priority=4]` in `watch.tree` `hub`):
- `king`: #greet#. Don't call me that. I've a round to walk, and the alleys don't care what the Palace says, no?
- `moot_called`: #greet#. A Moot at Midwinter, the herald says. Fen-sure, I'll keep the lamp lit till it sits. Somebody has to.
- `reckoning`: They're chalking doors on my round. I knocked on every one of those doors for the Watch, no?
- `signals`: Lamps on the ridges and none of them ours. I'd go up myself, but they want me on the Steps.
- `howls`: I howled the fen one at the feast and half the square sang it back as a burying. Maybe it was, no?
- `letters`: I've a letter for the Grayrock lord as well. The old King wrote on the back. I've not read that side twice.
- `lights`, `here.region=fenshore`: I knew every street of it, and which flooded first. There's reeds now where my mother's door was, no?
- `lights`, elsewhere: I'm walking down to the fens. Somebody from the Palace ought to stand where Mirebridge stood.
- `harvest`: Every wolf I'd trust is at the Larkfield ford, and here's me holding a lamp. Bring me word, no?
- `bell` + `nh_writ=whispered`: The City Office shuts its doors at noon now. Twelve years on the round, I know a door that's hiding something.
- `bell` + `nh_writ=read`: The old King meant it for someone like me. I'd rather he'd said so to my face. I'd have slept better, no?
- `moot` + `nh_dagfinn=stands`: I'll stand. Not for the chair. For the round, no?
- `moot` + `nh_dagfinn=steps`: Let the Moot keep the lamp. I'll carry it to them, and walk the round after.
- after `nh_crown=lamp`: Still walking the round on Restdays. A king can keep a habit, no?
- after `nh_crown=ottilie`: The Queen keeps the chair and I keep the lamp. Fen-sure, that suits the both of us.
- after `nh_crown=regency`: Steward of the Moot, they call it. I call it the lamp with more stairs.
- after `nh_crown=bought`: Next Midwinter, #address#. Howl louder.
- **P** `pair.mem.nh_stone=given`: #greet#. You're the one who brought my mother's stone up out of the mire.
- **P** `pair.mem.nh_found=first`: You found me on the round before the Houses did. I've not forgotten it, no?

**L** (Ch 1 entry, raw text, for Cressida's quest): none for him; his talk is all N.

**N** `nh_df` — opened by `> Can I ask you about the lamp?` `[self.id=rmx_dagfinn_thornham world.mem.nh_stage=king|moot_called|reckoning|signals|howls|letters|lights|harvest|bell|moot]`
```
node nh_df
say Ask, then. I've a round at dusk. | Ask. I'll answer slow, no?
say[world.mem.nh_stage=lights] Ask. The fens have ears, no?
free
choice Why aren't you up at the Palace? -> nh_df_why
choice What's the letter you carry? [world.mem.nh_stage=king|moot_called|reckoning|signals|howls|letters] -> nh_df_letter
choice What was Mirebridge like? [world.mem.nh_stage=lights|harvest|bell|moot] -> nh_df_mire
choice Do you think you should stand at the Moot? [world.mem.nh_stage=reckoning|signals|howls|letters|lights|harvest|bell] -> nh_dq    # the Question: Ch 2 to Ch 8 only (decisions §1.4)
choice If you stand, I'll back you. [world.mem.nh_stage=moot_called|reckoning|signals|howls|letters|lights|harvest|bell|moot] -> nh_back
choice I was wondering about something else... -> hub
choice Goodbye for now. -> bye

node nh_df_why
say Because the Sump doesn't stop needing a round walked, no? The Palace has plenty of wolves. The alleys have me.
choice The realm needs you up there. -> nh_df_up
choice Then stay down here. -> nh_df_down
choice I was wondering about something else... -> nh_df

node nh_df_up            # story-heard (Ch 1, Where Is the King? step 5: npc.walk to the Hundred Steps, npc.follow $actor)
say Needs a guard? Or needs a wolf the Houses can blame? I'll go up. Walk with me to the Hundred Steps.
mood calm

node nh_df_down
say Fen-sure, I might. But somebody ought to be in that room who's slept in a three-family tenement.

node nh_df_letter        # story-heard (plants Ch 5: sets player switch p_knows_df_letter)
say It's for the Grayrock lord. Everyone's got one. Mine's a receipt, and the old King wrote on the back. He's never home to read it.

node nh_df_mire
say Thatch on stilts, boardwalks you could hear a heron land on. It went under in one night, five winters back. I was on the round here, no?
mood calm

node nh_dq
say Should I? I'd be a lamp with a crown on. What do you say, #address#?
say[world.mem.nh_writ=read] The old King's writ says a Sump wolf keeps the lamp. It doesn't say he should wear more than that. What do you say?
choice You should stand at the Moot. -> nh_dq_stand
choice Keep the lamp. Let others stand. -> nh_dq_step
choice I haven't made up my mind. -> nh_df

node nh_dq_stand         # story-heard: p_advice=stand; nh_t_stand += p_voice (minus any earlier step); once per wolf per chapter
say Should I? Ask me again when you've seen more of it, no?
say[world.mem.nh_writ=read] The old King seems to think so. That's two of you.

node nh_dq_step          # story-heard: p_advice=step; nh_t_step += p_voice
say That's what I'd do, left alone. #thanks# for saying it.
```
`nh_back` (shared node, story-heard: the target decides the backing; §3.20) has his line `say[self.id=rmx_dagfinn_thornham] Then you'd be backing a patrolman. Fen-sure, I'll try not to make you sorry.`

**Ch 6, The King's Mother's Door-Stone** (Q, giver Dagfinn, given at Thornham House; he is home by day in Ch 6 after the fens):
- say: #greet#. My mother's door had a carved stone sill. If it's still at the crossing, under the reeds, I'd like it home. You'll know it: a heron cut in it.
- ask: Is something on your mind? · accept: I'll look for it at the crossing. · decline: Not now. · handIn: I found your mother's stone.
- thanks: #thanks#. A heron. She cut it herself, no? Put it by the door. No, I'll do it. (The quest's reward then writes `talk.remember {scope:"pair", npc:"rmx_dagfinn_thornham", player:"$actor", key:"nh_stone", value:"given", keep:true}`; §5.3.)
- O during: **I'm still looking for the stone.** → Under the reeds where the road used to cross. Dig with your nose. It smells of old smoke, not of fen.

### 3.2 Cressida Cotton (frosthollow; tree `watch`)
**Q Where Is the King?** (Ch 1, door quest):
- say: #greet#. You've a nose on you, I can tell. My patrol partner's been handed the Palace lamp and he's still walking our round. Find him before the Houses do.
- ask: Is something wrong, guard? · accept: I'll find him. · decline: Not just now. · handIn: I found him. On his round, like you said.
- thanks: #thanks#. He asked after you. Don't let it go to your head.
- O during: **Any sign of him yet?** → Ask the porters on the Sump Stairs. They see every wolf go down. Then follow your nose. He's not hard to smell.
- **L** (Ch 1 entry): Have you seen him? Russet, notched ear like mine. Smells of the Sump.

**T:** `king` Not seen him? Deep strange. He's never been late for a round in his life. · `moot_called` He keeps the lamp and I keep the round. Somebody's got to, cousin. · `reckoning` Factors on my round. I'd like to see them chalk my door. · `signals` Lamps on the ridges. Cold iron, I'd douse every one myself. · `howls`/`letters`/`lights` Still walking his half of the round. Don't ask me how he's keeping. Ask him. · `harvest` They won't let me go to the ford. Somebody has to walk the Sump. · `bell` He reads papers now. He used to read doors. · `moot` The waystation's mine on the longest night. Bring oil if you're coming. · after `nh_crown=bought` There's a ship going somewhere dry. I've a berth on it. Keep your lamp lit. · after others: Sergeant, they call me now. I've not got used to it.
- **P** (C5, `p_ch ≥ 8`): You again. He said you'd be back. Lamp's thanks.

**Q Keep the waystation on the longest night** (Ch 9): say: #greet#. The waystation's the last post before the Bay Light. Keep it with me. Bring oil and a horn. · accept: I'll stand there with you. · handIn: Oil and a horn, as you asked. · thanks: #thanks#, stone. Don't let it go out.

### 3.3 Brenna Abbot (plain; tree `resident`)
**T:**
- `king`: #greet#. I'll read what's to be read when I'm sure of it. Not before. A herald who guesses is just a loud wolf.
- `moot_called`: #greet#. I read it, and I'd read it again. A Moot at Midwinter. Will you howl for a town?
- `reckoning`: I've a note myself, due at the new moon. I'll pay it in coin or not at all. Not in my voice.
- `reckoning` + `self.mem.nh_debt=freed`: My note's paid. A wolf of the road worked it off and wouldn't take a penny. I'll read their deed if they let me.
- `signals`: Every notice I pin, the grey paw-mark pins one over it. I've started carrying two hammers.
- `howls`: A Moot needs nine howls, and half the square sang the burying one at the feast. Who remembers how?
- `letters`: Forty letters for a lord who is never home. I'll cry the ones their senders want cried.
- `lights`: The Houses want the Moot gone before Midwinter. They'll have to shout louder than a herald.
- `harvest`: I read out the ford's news every hour, three nights running. My voice is going. Theirs mustn't.
- `bell` + `nh_writ=whispered`: Something was read in the Rain Hall before the old King went up. I'd give my horn to know what.
- `bell` + `nh_writ=read`: I read it on the square at noon. My paws shook. The writ didn't.
- `moot`: I'll read every voice as it comes in off the Line. Make sure yours comes.
- after `nh_crown=regency`: Speaker of the Moot. It's a herald who's allowed to answer back.
- after others: I read the Moot's verdict. Now I read the new orders. Same steps, same horn.

**N** `nh_br` — `> How does a Moot work?` `[self.id=rmx_brenna_abbot world.mem.nh_stage=moot_called|reckoning|signals|howls|letters|lights|harvest|bell|moot]`
```
node nh_br
say By old custom, nine voices: the eight towns and the drowned one. The Houses think that means it can never sit. | Nine voices, the Houses in Council, and a howl to carry them.
say[world.mem.nh_writ=read] The nine voices of the communities, the four Houses in Council, and the free wolves of the roads. The old King wrote it so.
choice How is a town's voice counted? -> nh_br_count
choice What does a Keeper do? -> nh_br_keeper
choice Who backs the Moot itself? [world.mem.nh_stage=reckoning|signals|howls|letters|lights|harvest|bell|moot] -> nh_br_moot
choice Who are the free wolves of the roads? [world.mem.nh_writ=read] -> nh_br_free

node nh_br_moot
say The Speaker keeps the Moot's roll, so the Moot is his to back. Find him in the Antechamber. I only read what he counts.

node nh_br_count
say One voice a town if its howl reaches the Bay Light, half if it's silent. Its own wolves' backing decides which way it howls. It's on every board.

node nh_br_keeper
say A Keeper howls for a town and takes its pledges. Pledge to one, and every deed of yours carries that town's voice further.

node nh_br_free
say You are. Every wolf who walks the roads and belongs to no House. Four voices, if the writ holds. Two if it doesn't.
```
- **Q The Summons** (Ch 1; giver Brenna; item.give `letter`): say: #greet#. The Palace sends its word to every hearth, and the Palace has no wolves to send. Carry a Summons to a town's Keeper? · accept: Give me one. I'll carry it. · thanks: #thanks#. Every hearth that hears it is one more voice. · O during: **Who takes the Summons in each town?** → The wolf who'd howl for it. The loom master in Coldwater, the linen master in Larkfield, the shopkeepers in Fenshore and Saltwell. Ask at the square.
- Pledge lines: §3.20. She holds no backing: the Moot backing is the Speaker's (§3.8).

### 3.4 Ottilie Harrowgate (plain; tree `resident`)
**T:**
- `king`: #greet#. Forgive me, I'm carrying my father's petitions still. Nobody's told me who to give them to.
- `moot_called`: A Moot. My father would have liked that. He never trusted a room with only Houses in it.
- `reckoning`: They've offered to pay every small note in the Sump. With a hook in it, I expect.
- `signals`: The City Office says my petitions are mislaid. All of them. Again.
- `howls` (default): It's only the damp. It's always only the damp.
- `howls` + `world.mem.nh_cough=tended`: I climbed the Hundred Steps today without stopping. Someone was kind.
- `letters`: Half my petitions went to the Grayrock lord by mistake, they say. Forty letters, and mine among them.
- `lights`: The Houses are at each other, and the Council writes to me about my manners.
- `harvest`: I've sent what the household can spare to the ford. It isn't much. Mother counted it twice.
- `bell` + `nh_writ=read`: It passes over me in plain words. Good. I'd rather be passed over by my father than by the Houses.
- `moot`: If the realm wants a Harrowgate, it shall have one who carries her own petitions.
- after `ottilie`: I'm told a queen may not carry her own petitions. I've told them she may. · `lamp`: He asked me to keep the petitions. I said I'd never stopped. · `regency`: The Moot will hear petitions every Midwinter. I'll be first in line. · `bought`: I carry petitions to a Protector now. He reads them faster. I don't like what he writes.

**N** `nh_ot` — `> May I ask you about the crown?` `[self.id=rm_ottilie_harrowgate world.mem.nh_stage=king|moot_called|reckoning|signals|howls|letters|lights|harvest|bell|moot]`
```
node nh_ot
say Ask. I've been asked worse by the City Office.
choice Do you want the crown? -> nh_ot_crown
choice What are the petitions? -> nh_ot_pet
choice Should the Crown take the Counting House's loan? [world.mem.nh_stage=reckoning world.mem.nh_loan=offered] -> nh_ot_loan
choice If you stand, I'll back you. [world.mem.nh_stage=moot_called|reckoning|signals|howls|letters|lights|harvest|bell|moot] -> nh_back

node nh_ot_crown
say I want the petitions answered. If that needs a crown, I'll wear one. If it needs a guard from the Sump, I'll carry his petitions too.

node nh_ot_pet
say A roof that leaks in the Sump. A brother lost on the wharf. A well the Houses fenced. My father read every one. Somebody should.

node nh_ot_loan
say Every small note paid, and the Crown written into Greatly's book. What would you have me do?
choice Take the loan. The Sump can't wait. -> nh_ot_loan_yes
choice Refuse it. We'll free them another way. -> nh_ot_loan_no

node nh_ot_loan_yes      # story-heard: tally loan_yes += p_voice; decided 3 game days after the 2/3 step
say Then I'll sign it, and I'll know whose paw guided mine. #thanks#, I think.

node nh_ot_loan_no       # story-heard: tally loan_no += p_voice
say Then I'll refuse it, and you'll hold me to the other way.
```
- After the loan (T): `nh_loan=taken` The notes are paid and the Crown owes the Counting House. My father would have hated that. So do I. · `nh_loan=refused` I refused it. Now free them, please. I've told the whole Residence you will.
- **Q The Princess's Cough** is Gorm's (§3.12); she is the one tended. After `tended` the T line above plays.

### 3.5 Queen Oda Harrowgate (coldwater; tree `resident`)
**Q The Queen's Purse** (Ch 1):
- say: #greet#. A King's wake, and the Houses offering to pay for it. I'd rather pay in my own coin, eh? Help me find it.
- ask: Is something troubling you, my lady? · accept: I'll help you find it. · decline: Not now. · handIn: I found the household's old tolls, and the kitchen's fed.
- thanks: #thanks#. Now, how shall we pay for the wake? Tell me straight, eh? (The choice follows as N.)
- O during: **Where would the old tolls be?** → In the Records Office, three rolls deep, under the Houses' own. Nobody reads below the Houses, eh?

**N** `nh_od_pay` — `> About the wake: how should we pay?` `[self.id=rm_oda_harrowgate world.mem.nh_stage=king|moot_called]`
```
node nh_od_pay
say From my own purse, or the Counting House's loan. Choose, and I'll remember who chose.
choice Pay it from the household purse. -> nh_od_house
choice Let the Counting House lend it. -> nh_od_greatly

node nh_od_house         # story-heard (if the wolf's Queen's Purse is done): Blood +2, tally household
say Coin by coin, then. It'll be a thin wake and an honest one.

node nh_od_greatly       # story-heard: Purse +3, tally greatly (Ambrose's first loan to the Crown)
say Then the Counting House buries the King. Muddied water, that. But it'll be a fair wake.
```
**T:** `king` Every penny counted twice and the wake still short, eh? The Houses lend with one paw and take with the other. · `moot_called` A Moot, and a patrolman keeping my husband's lamp. Deep and dark, what next? · `reckoning` Someone in my kitchens is too rich by half. I can smell new coin on a servant, eh? · `signals`, `howls`, `letters`, `lights` and `harvest` (one line, `world.mem.nh_stage=signals|howls|letters|lights|harvest`) The purse is thin and the Houses are fat. That's the realm, neighbour. · `bell` + `nh_writ=read` It passes over my daughter in plain words. Well. Plain words can be argued with, eh? · `moot` + `nh_oda=contest` I'll fight for her at the Moot, and I fight dirty. For her, I'll fight clean. · `moot` + `nh_oda=accept` I keep the roll beside the Speaker. I can count. It's what I'm for.
- after `ottilie`: My daughter, the Queen. Calm water at last. · after others (unless reconciled): I'll go home to the lake in spring. Fair winds to the lot of you.
- **P** `pair.mem.nh_spy=told_queen`: You told me about the girl in my kitchens. I've not forgotten. I never forget, eh?

**N** `nh_od_choice` (Ch 8, the Queen's Choice) — `> About the writ: will you fight it?` `[self.id=rm_oda_harrowgate world.mem.nh_writ=read]`
```
node nh_od_choice
say The writ passes over her. Should I fight it at the Moot, or keep the roll and bless whoever wins?
choice Fight for her at the Moot. -> nh_od_contest
choice Let it be. Keep the roll beside the Speaker. -> nh_od_accept

node nh_od_contest       # story-heard: tally contest += p_voice; Blood +5 if it carries
say Then I'll fight. Deep and dark, I've waited forty years to.

node nh_od_accept        # story-heard: tally accept += p_voice
say Keep the roll, then. I'll count every voice twice. Somebody honest should.
```

### 3.6 Ambrose Greatly (plain; tree `resident`)
**T:** `king`/`moot_called` #greet#. A sad spring. The Counting House has sent flowers to the Residence, and an account for them. · `reckoning` #greet#. A city that pays what it owes. You'd be surprised how few wolves want that. · `signals` Roads closed are roads unpaid. I regret it, of course. I regret most things at a fair rate. · `howls` Nine howls. I've counted them. Some of them are mine. · `letters` The Grayrock lord is coming home, I hear. He'll find his ledgers in order. I've seen to several. · `lights` Quarrels are expensive. I lend to both sides; it keeps them civil. · `harvest` Grain at siege price is still grain at a price. Someone must buy it. · `bell` + `nh_oskar` (any) The Chancellor has been talking. Chancellors do. It passes. · `moot` Count the voices, by all means. I've already counted mine. · after `bought` The realm is in good order. It owes me, and it knows it. · after others: The Moot has spoken. I'll send it my account.
- Branch T: `nh_ledger=shown` My daughter reads well. Too well. She sleeps at the abbey now, which is cheaper. · `nh_ledger=kept` We came to terms. I keep my terms. · `nh_lists=sold` The Watch is in good paws now. Mine, I mean.
- **P** (Purse wolves, `pair.mem.nh_purse=marked`): Ah. One of ours. The Counting House remembers its friends, and its rates.

**N** `nh_am` — `> What do you want, sir?` `[self.id=rm_ambrose_greatly world.mem.nh_stage=reckoning|signals|howls|letters|lights|harvest|bell|moot]`
```
node nh_am
say Want? I want very little. I'm owed a great deal.
choice What do you want with the howls? -> nh_am_howl
choice Why chalk the Sump's doors? [world.mem.nh_stage=reckoning] -> nh_am_chalk
choice You promised the Grey Captain Deepvein. [world.mem.nh_lie=known] -> nh_am_lie
choice I'd work for the Counting House. -> nh_am_work

node nh_am_howl
say A voice is only a debt with a tune to it. I collect debts.

node nh_am_chalk
say Because chalk washes off and ink doesn't. I gave them a moon's grace. Is that cruel? It's more than the Sump gives itself.

node nh_am_lie
say I promised him a deed. I never said whose name would be on it.
mood formal

node nh_am_work          # points the wolf to the Purse patron; the backing itself is taken at the steward (§3.20)
say Then speak to my steward. He keeps the House's friends in a book, and he keeps it kindly.
```

### 3.7 Rosalind Greatly (plain; tree `resident`)
**Q A Ledger That Does Not Add Up** (Ch 2, large):
- say: #greet#. I've read every ledger in the Counting House, and one of them doesn't add up. I can't carry it out myself. Could you?
- ask: You look like you've found a mistake. · accept: I'll carry it out for you. · decline: Not now. Keep it hidden. · handIn: I've what the ledger was missing.
- thanks: #thanks#. Three sums, and now they agree. That's worse, isn't it?
- O during: **Where do I find the other sums?** → The steward has the day book. The customs tallyman has the Crown's tally. The pawnbroker has the factors' notes. The Bank knows the rest.

**N** `nh_ro_what` (the finisher's choice) — `> About the ledger: where should it go?` `[self.id=rm_rosalind_greatly world.mem.nh_stage=reckoning]` (story counts only wolves whose quest is done)
```
node nh_ro_what
say It's yours to carry now. Where?
choice Show it to the Council. -> nh_ro_shown
choice Bargain with your father through his steward. -> nh_ro_kept
choice Burn it, and keep it between us. -> nh_ro_ash

node nh_ro_shown         # story-heard: tally shown += p_voice
say Then my father will know by supper, and I'll sleep at the abbey. Do it anyway.

node nh_ro_kept          # story-heard: tally kept
say He'll pay to keep it quiet. He always does. I'll not thank you.

node nh_ro_ash           # story-heard: tally ash; C8 pair nh_ledger=ash for this wolf (§5.3)
say Then it lives in two heads and no book. Mine, and yours. Remember it.
```
**T:** `reckoning` Three sums, and none of them agree. Someone's counting twice, and I think I know whose paw it is. · after `nh_ledger=shown` (her home is now the abbey quarters) I sleep at the abbey now. The beds are hard and the books are honest. · `kept` Father bargained. I wasn't in the room. I never am. · `ash` Burned. Good. Some things are safer in two heads than in one ledger. · `moot` If they crown me, I'll read every ledger in the realm. Father should be careful what he wishes for. · after `bought` Crowned for a sum I didn't add. I'll audit the crown first.
- **P** `pair.mem.nh_ledger=ash` at `bell`: You burned it for me. I've written something down for you. Read it alone. (The story sends the site as a `journal.note` to that wolf, because only the story knows which site was picked.)
- **At the Moot** (S, a `line` beat in Ch 9's Moot scene, Rosalind in her own panel; played when the wolf who carried the ledger, player switch `p_ledger_carried`, is in the audience; this replaces campaign Ch 2's `story.tell from`): "Father, this is the wolf who carried the ledger out. Count that too."

### 3.8 Gerrit Mudhand (plain; tree `resident`)
**T:** `king` #greet#. An old wolf's note in a lamp-book is not law. I said so on the square and I'll say it here. · `moot_called` A Moot, then. By the forge and the sea, the Council will need a bigger table. · `reckoning` (default) I keep the Moot's roll now. A Moot needs friends who aren't Houses. Back it, and I'll write your name in. · `reckoning` + `nh_ledger=shown` A ledger came to the Antechamber. I've read it four times. By the forge and the sea. · `signals` Every House says the lists are forged. Every House is lying, or none is. · `letters` The Grayrock seat's been empty six years. If the lord sits, the Council's whole. I'd almost like that. · `lights` Petitions on every desk to unseat the lamp-keeper. I've put them in a drawer. A deep one. · `bell` + `nh_writ=whispered` I saw him weep. I've said nothing for a season. I'm tired of nothing. · `moot` I keep the roll. Every voice, as it comes. The Council will hear it whether it likes it or not. · after: Speaker of the Council still. Fewer shouting matches. More counting.

**N** `nh_ge` — `> What does the Council make of it?` `[self.id=rm_gerrit_mudhand world.mem.nh_stage=king|moot_called|reckoning|signals|howls|letters|lights|harvest|bell|moot]`
```
node nh_ge
say The Council makes speeches. What would you like to know?
choice Will the Houses accept a Moot? -> nh_ge_accept
choice What did you see in the Rain Hall? [world.mem.nh_stage=bell] -> nh_ge_weep

node nh_ge_accept
say They'll accept what they can't stop. Your work, I'd say, is to make it unstoppable.

node nh_ge_weep
say The Chancellor, with a paper and a seal, the month before the old King went up. He wept like a pup. That week the Hall was sealed for repairs.
```
(His testimony in the race is the quest objective `conversation` + `gift`; this node is the lore, open to all.)
- **The Moot backing** (from Ch 2; decisions §1.4): his choice in the `nh_story` back slot is `> I'd like to back the Moot itself.` `[self.id=rm_gerrit_mudhand world.mem.nh_stage=reckoning|signals|howls|letters|lights|harvest|bell|moot]` → the shared `nh_back` node (§3.20), whose trigger reads `target` = `rm_gerrit_mudhand` as the Moot (Regency) backing.

### 3.9 Bettany Nailley (plain; tree `watch`) and the four list destinations
**T:** `reckoning` Ask the cells about the Captain's note. Not me. I'm a sergeant; I'm not meant to know. · `signals` Six years I've waited for a paper like that. If you find one, you know where I stand. · after `nh_lists=bettany` The magistrate heard it. The Watch is mine for now. I didn't want it like this. · `nh_lists=press` It's printed. Half the Watch won't meet my eye. Good. · `nh_lists=sold` They've put me on the Harbour Gate. Somebody paid for that. · `letters` + `nh_wharf=heard` The Grayrock lord stood at the chapel while the names were read. I didn't know he could stand still. · `letters` default: Grayrock. Say it again and I'll spit. · `lights` There's a skipper at Rivermouth who carries more than eels. I'd like a reason. · `harvest` We ride with the ford escorts. Bring a lamp you're willing to lose. · `moot` The Watch stands at the Moot. All of it, if I've a say.

**N** `nh_be` — `> Can I ask about the Watch?` `[self.id=rm_bettany_nailley world.mem.nh_stage=reckoning|signals|howls|letters|lights|harvest|bell|moot]`
```
node nh_be
say Ask. I'll answer what I can and lie about nothing.
choice Who pays the Watch to look away? [world.mem.nh_stage=signals] -> nh_be_who
choice What's the wharf to you? -> nh_be_wharf
choice I've the whole list. Take it to the magistrate. [world.mem.nh_stage=signals] -> nh_list_bettany

node nh_be_who
say Three Houses on every list, I'd guess. One on none. Bring me the paper and I'll stop guessing.

node nh_be_wharf
say My brother drowned off a Grayrock crane. Nobody said his name after. I say it every morning.

node nh_list_bettany     # story-heard (counts only a wolf holding the whole list): tally bettany
say Then I'll walk it to the magistrate myself, at noon, with the door open. #thanks#.
mood stern
```
The other three destinations are the same shape, each `[self.id=<them> world.mem.nh_stage=signals]` in their own `nh_story` slot:
- **The Council Press, Ingrid Ashwell** `rm_ingrid_ashwell` — `> Print it. Every board.` → `nh_list_press`: Print it? My brother would have liked that. Give it here, and stand back from the ink.
- **Corra Slagmore** `rm_corra_slagmore` — `> This is yours to mend.` → `nh_list_captain`: `say[self.mem.nh_debt=freed]` Then I'll burn it here, in front of you, and swear the Watch to the lamp. · bare: I'll burn it. And the Counting House will still own me. You know that?
- **Rune Gullwell** `rm_rune_gullwell` — `> What's the Counting House paying?` → `nh_list_sold`: Ten pennies, and the House forgets your name kindly. Sign here.

### 3.10 Corra Slagmore (plain; tree `watch`)
- **Q The Captain's Note** (Ch 2; journal offer, no giver; she speaks through T and N): T `reckoning` Ask me again on a better day. Not here. · `reckoning` + `self.mem.nh_debt=freed` My note's gone. A wolf of the road, they say. I owe you a Watch that deserves you.
- N `nh_cs` — `> Who sold your note to the Counting House?` `[self.id=rm_corra_slagmore world.mem.nh_stage=reckoning]` → say: Not here. The cells keep better secrets than this office. Ask the one who keeps them.
- After `nh_lists=bettany` I've stood aside. The sergeant leads. She'll do it better. · `press` + freed: I resigned. In honour, they say. It doesn't feel like it. · `press` + not freed: I resigned in disgrace, and I'd earned it. · `captain` + freed: Burned, and sworn. The Watch keeps the lamp now. · `sold`/`lost`: Don't ask me about the Watch. Ask whoever owns it.

### 3.11 Ulric Stonevein (frosthollow; tree `resident`)
**T:** before `signals`: #thanks# for asking. Nothing to say, cousin. The mine's short and the road's long. · `signals` Lamp-black and old snow. I know that scent. I buried it, I thought. · `howls` Our gathering howl goes lower than the warning one. Lower than you'd think a wolf could go. · `letters` Black cloaks came down the Pass. Didn't take their boots off. Deep strange. · `lights`/`harvest` The Captain's a Deepvein wolf. I'd stake my stones on it. · `bell` + `nh_fenwick=confessed` Then Frosthollow knows. If he's alive, I'll stand on the Pass with a lamp lit. · `moot` If he's alive, I'll stand on the Pass with a lamp lit. · after `nh_captain=turned` He went back over the Pass, to bring them down in spring. I believe him. Mostly. · `beaten` They led him to the city. I walked behind. Somebody should. · `dead` No quarter, they said. I said nothing. I should have. · `fled` Gone over the Pass with his coin, and my brother with him. Deep strange, to lose a brother twice.

**N** `nh_ul` — `> About the harness I found...` `[self.id=cty_ulric_stonevein world.mem.nh_stage=signals|howls|letters|lights|harvest|bell|moot]`
```
node nh_ul
say That's my brother's scent. Older and smokier, but his. He died in the deep winter. Didn't he?
choice Who kept the winter records? -> nh_ul_records
choice There's no Deepvein name on the lamp-oil list. -> nh_ul_list
choice The mine captain said it, at the Lamp and Pick. [world.mem.nh_fenwick=confessed] -> nh_ul_said

node nh_ul_records
say The city, cousin. The Records Office keeps everything but its promises. Look for the lamp-oil list.

node nh_ul_list          # story-heard (counts after the wolf's archive objective): nh_ulric = 1
say Not one. Somebody barred a door, and somebody wrote it down after.
mood stern

node nh_ul_said
say Then Frosthollow knows. If he's alive, I'll stand on the Pass with a lamp lit.
```
- **Pledge** (Frosthollow Keeper): §3.20. **Q Frosthollow's Gathering Howl** (Ch 4): say: #greet#. Our gathering howl goes low, so it carries down the shafts. Three of our old ones hold a piece each. Ask them twice. · thanks: Lamp's thanks. Now howl it with me on the square, low. · **Q Keep the Pass post** (Ch 9): say: The Captain says the Pass is his on the longest night. Stand it with me. Oil, and a horn. Lamp lit. · thanks: Lamp's thanks, cousin. Stand close.

### 3.12 Gorm Plankgate (saltwell; tree `resident`; at the Sump Infirmary)
**Q The Princess's Cough** (Ch 4): say: #greet#. The King's daughter coughs in the damp and pretends not to. Bring me a yarrow poultice and herbs, then sit with her a while. · ask: Is there someone you're worried about? · accept: I'll bring them and sit with her. · handIn: I've sat with her, and she has the poultice. · thanks: #thanks#. She breathed easier. I noticed. I notice things, honoured guest. · O during: **Where do I get a yarrow poultice?** → Any healer's shop, or make one: yarrow and a clean cloth. Bring herbs as well. She'll pretend she doesn't need them.

**T:** `howls` Hold out your paw. No reason. An infirmarian likes to look. · `letters` + `nh_wardens=coming` Yes, I wrote to them. By salt and water, someone had to. · `lights` I am the realm's Eye, and I'm sorry for it. Ask your question; I'll carry it. · `harvest` She asks whether a realm that cannot hold its own ford can keep its own pups. I've no answer for her yet, honoured guest. · `bell` If the pup is taught, I can say the realm keeps its own. If she is hidden, I can say nothing, carefully. · `moot` She is listening, honoured guest. So am I. · after `nh_concord=answers` She tore the Keeping at the Bay Light. I've a Warden House to keep at the abbey now. · `holds` One Warden stays at the Grey Gull till spring. I'm to keep her tea hot. · `bends` Wardens in every silent town by spring. I wrote the letter that brought them. I'll carry that.

**N** `nh_gm` — `> Why did you write to the Wardens?` `[self.id=rm_gorm_plankgate world.mem.nh_stage=letters|lights|harvest|bell|moot]`
```
node nh_gm
say In Saltwell, when I was young, a pup the water obeyed drowned a well and herself with it. No one had taught her. I wrote so it would not happen here.
choice Would you speak for a Quickened wolf? -> nh_gm_speak
choice If a wolf carries a Gift, should they come to you? [world.mem.nh_stage=lights|harvest|bell|moot] -> nh_gm_gift
choice The Reedmere pup is Quickened. [world.mem.nh_stage=bell] -> nh_gm_told
choice There are no pups at the landing but ours. [world.mem.nh_stage=bell world.mem.nh_search=on] -> nh_gm_lie
choice What does the Envoy ask of us? [world.mem.nh_stage=moot] -> nh_gm_q

node nh_gm_speak
say If they are taught, yes. I cannot speak for what is hidden from me.
say[world.mem.nh_mere=still] The pup at the landing stilled a whole mere and drowned no one. I will say that this realm keeps its own.

node nh_gm_gift
say Bring a friend who will vouch for them, and come in daylight. I write down what is true and nothing more, honoured guest.

node nh_gm_told          # story-heard; the wolf's real act is the `told the wardens` event at Gorm: tally pledged
say Then she will be taught, not drowned. You did a hard thing. I will write it kindly.
mood calm

node nh_gm_lie           # story-heard: each hiding wolf rolls chance 30 against the Envoy's search
say So you say. She will look herself. I hope you're right, honoured guest.
mood stern

node nh_gm_q
say She asks it of the first three who ask her: what kind of realm is this?
choice A realm that paid the Watch to look away, and is learning to stop. -> nh_gm_q_honest
choice The finest realm on the map. -> nh_gm_q_proud
choice Ask the Houses. -> nh_gm_q_evade

node nh_gm_q_honest      # story-heard (first three wolves): Concord +1
say She writes that down. She writes slowly when she likes an answer.

node nh_gm_q_proud
say She writes nothing.

node nh_gm_q_evade
say She says she has.
```
**Gorm revealed** (Ch 5 end scene at the Sump Infirmary, S): "Yes, I wrote to them. By salt and water, someone had to." / "A pup the water obeyed drowned a well when I was young. I'll not watch it twice." **The Envoy's Question** (Ch 6 Antechamber scene, S, Gorm speaking): "She will not speak to you herself. By the Concord's custom an envoy speaks to a realm only through its own Eye. She asks this: on the longest night of the year, let one howl run from the Pass to the Waste, every town answering the one before. Then she will write that you keep yourselves." · **Ch 7**: the Envoy's second question is Gorm's own `harvest` T line above (in his window, in his voice), and, for wolves at the ford, a one-beat scene (S) in his panel when the siege opens: "She asks whether a realm that cannot hold its own ford can keep its own pups." No `story.tell from`.

### 3.13 Tansy Reedmere (coldwater, a pup's voice; tree `resident`)
**T:** before `howls`: #greet#. I'm only watching the mere. It's allowed. · `howls` I hum when the chorus starts. The water likes it better than howling. That's silly, isn't it? · `bell` I hum when it's restless. Don't tell. Please. · `bell` + `nh_mere=still` The whole mere went still when I asked. Everyone saw. I'm not sorry. · after `nh_tansy=taught` Calm water. I can make it listen now, a little. Not the bell. Never the bell again. · `pledged` They say I'm to go to the Wardens' school in spring. I'll write. They let you write, don't they? · `hidden` I hum at the landing every night. Nobody asks why. · `found` They found me out. I'm going in spring. My brother won't speak to anyone.

**N** `nh_ta` (her chain, four steps) — `> Is something the matter with the water?` `[self.id=tn_cw_tansy_reedmere world.mem.nh_stage=bell]`
```
node nh_ta
say You can't tell anyone. Promise? Not even the healer from the city.
choice Does the water talk? -> nh_ta_talk
choice I promise. -> nh_ta_talk

node nh_ta_talk
say It listens. That's worse.
choice What does it listen for? -> nh_ta_hum

node nh_ta_hum
say Humming. It goes still when I hum. Hum with me?
choice I'll hum with you. -> nh_ta_still

node nh_ta_still         # story-heard: one Stillwater lesson (if Teach her leads the tally, or the wolf chose it)
say There. Feel it? Still water. That's what the name means, I think.
mood pleased
choice Will you stand at the landing on the longest night? -> nh_ta_night

node nh_ta_night         # story-heard: p_tansy_night on (Ch 9 Glasswater post: she stands there if taught)
say If you're there. I'm braver when somebody's there.
```
The tally that decides her fate is three nodes at Kell's boat shed, `> She should be taught here, by us.` (`nh_ta_teach`), `> Tell the healer. She needs the Wardens.` (`nh_ta_give`), `> Hide her. Nobody need know.` (`nh_ta_hide`), all `[self.id=tn_cw_kell_greyton world.mem.nh_stage=bell]` in Kell's slot; Kell answers: teach: Then we teach her, eh? Slowly. Like hauling. · give: The city's healer, then. It's a hard road for a minnow. · hide: Hide her. Masking oil and a closed door. Calm water, I hope you're right.

### 3.14 Solveig Tarwell (plain; tree `church`)
**Q The Names on the Water** (Ch 5): say: #greet#. The wharf has drowned wolves nobody names aloud. Bring me five names from their families, and the Grayrock lord will hear them read. · ask: Is there something the chapel needs? · accept: I'll gather the names. · handIn: Five names, from five families. · thanks: #thanks#. Five names. I'll read them slowly, and he'll stand still for every one. · O during: **Who will give me a name?** → Anyone on the wharf who curses Grayrock. That's most of us. Be kind first; then ask what trade their brother had.
- **Families' replies, in the bereaved's own window** (no `story.tell`). Ch 5's setup writes C8 `talk.remember {scope:"npc", npc:<each>, key:"nh_kin", value:"wharf", days:40}` to every resident of the campaign's bereaved list `nh_bereaved` (the 56 who lost kin on the wharf; one batch). Their `nh_story` cast slot then carries the node; the story hears it (`dialogue node`, `detail:"nh_wn"`, `target` in `nh_bereaved`) and counts one name per resident per wolf (player list `p_names_from`, added to only when the target is not yet in it). This replaces the `conversation` objective's text.
  ```
  # opened by: > The chapel's priest is gathering the names of the wharf drowned. [self.mem.nh_kin=wharf world.mem.nh_stage=letters]
  node nh_wn
  say He was a crane hand. Forty years on the hoist and one bad rope. | He rowed the lighters. Nobody rowed them better. | He mended nets on the Mole. The tide took him off it. | A stevedore. Grayrock paid for the burying and nothing else.
  say[self.id=rm_bettany_nailley] A crane hand. A Grayrock crane. Tell the priest that, and tell her I'll be there when she reads it.
  say[self.id=rm_ingrid_ashwell] He loaded the lighters off the Mole. I'd have printed his name a hundred times if anyone had asked.
  say[self.id=rm_rune_fogworth] A rope hand on the hoists. Quiet, like me. Say it slowly when it's read.
  say[self.id=rm_ivar_pitchwick] He counted bales on the Grayrock quay. The Counting House sent a clerk to the burying. Me.
  say[self.id=rm_bertil_grantham] He was my brother and he could whistle louder than the Bay Light. That's his name, really. Tell her that.
  mood calm
  ```
  The names themselves are never spoken (tree lines name no one); each answer gives a trade. Every one of the 56 lost a brother on the wharf (their backstory: "lost a brother on the wharf and does not forgive House Grayrock for it"), so every answer is "he", and only Bertil, whose backstory carries it, says the kin word aloud (talk_lint checks kin words against the facts).
**T:** `letters` Bring me a name of the wharf drowned and I'll say it over the water. That's all a rite is. · `lights` Bring me a name and I'll say it over the water. The fen ones too. Especially the fen ones. · after `nh_lights=laid` I said every name at the crossing. The fog lifted. I don't claim it. I only noticed. · `lights` + `nh_lights=faded` I went to the crossing and nobody came with names. I'll go again next autumn. · `moot` If the bell comes, I'll ring it as the howl comes in. If it doesn't, I'll ring the chapel's twice. · after `nh_bell=rung` I rang the drowned bell at the Bay Light. My paws still hum.
- **S, the Rite of the Tide** (Ch 6, public scene at the crossroads, cast Solveig and Skye; `response` beat for the wolves): Solveig: "We stand where Mirebridge stood. I'll say each name, and you answer: we remember." · response prompt: "Answer her." · Skye: "Fen-sure we remember. Every boggy one of them." · Solveig (closing): "That's all a rite is. Names, said aloud, where the water can hear."
- **Q Keep the Bay Light** (Ch 9): say: The howl comes in at the Bay Light. Stand with me there with oil and a horn, and I'll ring the bell when it reaches us. · thanks: #thanks#. Count a hundred breaths after the bell. That's when the far howls come in.

### 3.15 Skye Eelbank (mirebridge; tree `resident`)
**T:** before `howls`: #greet#. Best eel trapper on the river, me. Ask anyone. Ask the eels. · `howls` #greet#. Mirebridge had a howl that went round the reeds twice before it came back. I'll teach you the first turn, no? · `lights` Fen-sure I'll read it at the crossing. Somebody's got to say them out loud. · after `nh_lights=laid` Mirebridge's Keeper, they call me. I'm an eel-trapper with a long memory. Same thing, no? · `moot` Mirebridge has a voice again, no? Carry oil to the crossing three nights, and the true lights will answer.
- **Q The first turn** (part of The Scattered, Ch 4): say: #greet#. You've found the scattered, then. Come to the creek at dusk and I'll sing you the first turn of the old howl. It goes round twice, no? · thanks: #thanks#. Twice round the reeds and back. Now you've got it.
- **Q The Path With No Note** (Ch 6): say: #greet#. There's a path through the fen with no note on it. Start at Withy Creek, reeds on your left, never step where it shines warm. Want it? · accept: I'll walk it. · handIn: I walked it, and found what they hid at the causeway. · thanks: #thanks#. You came out dry. Mostly dry. Fen-sure, that counts. · O during: **Which way after the creek?** → Reeds on your left till the dead willow. Then the cold lights only. A warm one's a lamp, and a lamp's a lie, no?
- **Q Lanterns for the Drowned** (Ch 9): say: #greet#. Carry lamp oil to the crossing three nights running. The lights out there will answer, the true ones this time, no? · thanks: Dry feet and #thanks#. They answered. I saw them.

### 3.16 Desmond Hollis (coldwater; tree `watch`)
**T:** `reckoning` (at the Knucklebones) You smell of the yard. So do I. Leave it, eh? · `reckoning` + `here.time=night` (at the Carters' Yard; this replaces campaign Ch 2's `npc.speak`, decisions §9 P1-12): One mark for the wagon days, eh? Greatly pennies spend the same. Buying, or walking on? · **C5** `npc.greet {npc:"rmx_desmond_hollis", player:"$actor", days:3}` on `near resident` at the yard at night (the same trigger that sets `p_saw_desmond`), once C5 exists: You were at the yard last night. The smell's still on you. Forget what you heard, eh? (Before C5, the night T line above carries it, with the DM seeding `nh_stage` during Act I's today-only version, decisions §6.2.) · `signals` A Palace guard with no Palace. Muddied water, that. What do you want? · `nh_desmond=held` (in the cells) Speak for me at the hearing and I'll stand at the granary. Don't, and I'll not blame you. · `nh_desmond=redeemed` I stood at the granary. Nothing came through it. That's something, eh? · `nh_desmond=pillory` A day in the pillory. The Sump threw fish. Fair catch, I suppose. · `nh_desmond=fled` (held in the foothills) Go away. I've nothing for the city, and the city's nothing for me.

**N** `nh_dh` — `> What happened to your post?` `[self.id=rmx_desmond_hollis world.mem.nh_stage=reckoning|signals|howls|letters|lights|harvest|bell]`
```
node nh_dh
say A Sump patrolman climbed the Steps and lit a lamp, and the Palace forgot it had guards. I didn't forget, eh?
choice Who pays you now? -> nh_dh_pay
choice You saw the Chancellor weep, didn't you? [world.mem.nh_stage=bell world.mem.nh_desmond=held] -> nh_dh_weep

node nh_dh_pay
say Ask the carters' yard what it sells at night. I'll not say it.
say[world.mem.nh_desmond=held] A tall black arctic out of the north, eyes like a frozen tarn. Pays in Counting House pennies. There. Happy?

node nh_dh_weep          # the testimony itself is the quest's conversation objective in the cells ("A Visit to the Cells")
say I was on the Rain Hall door. He had a paper with the old King's seal, and he wept on it. Then the order came to seal the Hall.
```
- **S, the parley** (Ch 7, Desmond fled, as the Captain's speaker): "He says: I am Eskar, of Deepvein." · "He says: in the deep winter Frosthollow's mine house would not open to Deepvein's miners. They died in the snow, or went over the Pass." · "He says: he saves to buy Deepvein back. The Counting House has promised him its deed after Midwinter, if the roads stay shut." · choice text: "What do you answer him?" options **Offer terms** / **Refuse** / **Strike under the truce**. · offered: "He says: then say it in front of Frosthollow, and show me the Counting House's word in writing." · no key: "He laughs. He says: with what?" · broken: "He says nothing. His lieutenant falls. The whole Downs will hear of this by morning."
- **S, the Captain's letter** (Ch 7, Desmond held; Rook reads it): Rook, in his panel: "Weights and measures, listen to this. A boy came up the Downs Road with it." · Rook: "It says: I am Eskar, of Deepvein. In the deep winter Frosthollow's mine house would not open to my miners. They died in the snow, or went over the Pass." · Rook: "It says: I save to buy Deepvein back. The Counting House has promised me its deed after Midwinter, if the roads stay shut." · then the choice as campaign Ch 7 gives it for the held-Desmond road.

### 3.17 Oskar Gullworth (plain; tree `resident`)
**T:** before `bell`: I've no time to talk. The City Office keeps late hours. · `signals` Petitions mislaid? The City Office mislays nothing. It postpones. · `bell` + `nh_writ=whispered` I've no time to talk. The City Office keeps late hours. Some papers keep later. · after `nh_oskar=confessed` I said it before the Council. All of it. I've not slept so well since the spring. · `exposed` Printed, on every board. I signed the order that bought that press. · `fled` (at the crossroads) Don't. I came here because nobody comes here. · `paid` There's nothing to say. Nothing at all. Good day.

**N** `nh_os` (the confrontation) — `> You wept in the Rain Hall. Over the King's writ.` `[self.id=rm_oskar_gullworth world.mem.nh_stage=bell]` (story counts only wolves who heard ≥ 3 testimonies)
```
node nh_os
say Who told you that? No. Don't say. Forty wolves saw, didn't they. I counted the faces for months.
mood stern
choice Tell the Council yourself. -> nh_os_confess
choice The Press will tell it for you. [world.mem.nh_note=found] -> nh_os_expose
choice Run, if you want. -> nh_os_run

node nh_os_confess       # story-heard: tally confessed
say Myself. Before the Houses, with his debt and his order in my mouth. Bright One help me. Yes. I will.

node nh_os_expose        # story-heard: tally exposed
say Then print it. I'll read it on the board like everyone else.

node nh_os_run           # story-heard: tally fled
say Run. Where does a Sump wolf run? Down the Fen Road, I suppose. Where the drowned are.
```

### 3.18 Fenwick Plankworth (coldwater; tree `resident`)
**N** `nh_fw_seam` (Ch 2, the Seam Note) — `> What's this about the mine's note?` `[self.id=cty_fenwick_plankworth world.mem.nh_stage=reckoning]`
```
node nh_fw_seam
say The Counting House holds the mine's note. Their rider comes up the Pass Road with letters. If the letters went missing, eh? There's a share in it.
choice I'll not touch the rider's letters. Let's fill the quota instead. -> nh_fw_honest
choice A share, you say? -> nh_fw_crooked

node nh_fw_honest        # story-heard: tally honest; offers the quota quest (journal)
say Ten loads of ore, or meat for the miners' larder. The hard way. Fair catch, if you manage it.

node nh_fw_crooked       # story-heard: tally crooked; p_marks +1
say Good. Muddied paws keep their mouths shut, eh?
```
- **Q Silver Under the Ice** (Ch 4, party): say: #greet#. The old King's first winter, he sent the Kestrel mine's toll silver to the Sump kitchens by sledge, and the sledge went through the Highmere ice. I know where, eh? I want a charter for the Kestrel seam. · accept: We'll haul it. · handIn: The silver's down the Pass Road. · thanks: #thanks#. Now, the charter. Who's it to be, eh? (then N `nh_fw_charter`: `> Give him the charter.` → Then the silver's the Crown's and the seam's mine. Fair catch. · `> Deny it.` → Then I'll sell where it lies to someone who'll pay, eh? Don't look at me like that.)
- **N** `nh_fw_come` (Ch 8) — `> Come and say it in front of them, at the Lamp and Pick.` `[world.mem.nh_stage=bell world.mem.nh_ulric=list]` → say: On a Restday? With the whole valley drinking? Deep and dark, you don't ask much. Fine. Walk me there. (story-heard: npc.walk to `cn_frosthollow_alehouse` on Restday evening; needs two wolves present if `nh_seam=crooked`)
- **S, the confession** (Lamp and Pick, Restday evening; cast Fenwick, Thorn Ashwood, Ulric): Fenwick: "I barred the mine house in the deep winter. The Deepvein wolves knocked till morning. I counted the knocks." · Thorn: "Lamp's thanks for saying it. Late, but said." · Ulric: "Hakon was at that door." · T after: I barred the door. I've said it. Now leave me the saying of it, eh?

### 3.19 The Grayrocks, the Fells and the Timberlands
**Aldric** (plain): T `letters` on the road: Letters? Out here? Give it to me. My daughter's paw, I'd know it anywhere. · `nh_grayrock=home` Forty letters. My daughter's were the heaviest. I read them all on the road. · after `nh_wharf=heard` I stood at the chapel while they read them. I didn't know them. I should have. · `moot` Grayrock's seat is filled. Whether it votes as you like is another matter. · N `nh_al` — `> Your wagons pay the Grey Captain.` `[self.id=rm_aldric_grayrock_ world.mem.nh_stage=letters]` → Contracts. Signed with a paw-mark, I grant you. I'll read that one twice.
- **P** (C1 + C5, the lone wolf who delivered ten; no `story.tell from`): when he reaches the Hall gate, C5 `npc.greet {npc:"rm_aldric_grayrock_", player:"$actor", days:14, text:"You carried ten of them out to my road. I've told Maren of you. She'll want to see your face."}`, and C8 pair `nh_letters=ten` for that wolf. After that, his window: **P** `pair.mem.nh_letters=ten`: Ten letters, out to my road. I've not forgotten whose paws carried them.

**Maren** (larkfield): **Q News of Larkfield**: say: #greet#. I'm a Larkfield wolf, did you know? Carry a word to the linen master there, and bring me back a twist of thyme from the downs. · accept: I'll carry it and bring the thyme. · handIn: Your word's delivered, and here's the thyme. · thanks: #thanks#. It smells of home. I'd forgotten home had a smell. · T `letters` I've not been to the gate in years. Is the city still the same out there? · after the walk to the gate: I walked to the gate. Scales and silver, it's further than it looks. · **P** `pair.mem.nh_thyme=given`: There's thyme in a jar by my window now. You did that.
- N `nh_ma` (a toll-receipt holder) — `> Your husband's wagons pay the Captain.` `[self.id=rm_maren_grayrock_ world.mem.nh_stage=letters]`:
```
node nh_ma
say My husband trusts contracts more than wolves. Let him see one he can't refuse.
choice Then stop it quietly. -> nh_ma_bargain
choice The herald should cry it on the square. -> nh_ma_cry

node nh_ma_bargain       # story-heard (counts a wolf carrying a receipt): toll tally bargained
say Quietly. Yes. And two ships for whatever the realm needs next. That is a contract too.

node nh_ma_cry
say Then take it to the herald. I'll not stop you. I'll not thank you either.
```
(The `cried` side is taken at Brenna's slot: `> Cry this receipt on the square.` → `nh_br_cry`: I'll cry it at noon, word for word. The Wharf won't like it. The Sump will.)

**Ingrid Grayrock** (plain): **Q Every Week, a Letter**: say: #greet#. I write to my father every week. Would you carry this week's? He's somewhere on the coast road. He's always somewhere. · accept: I'll find him and give it to him. · handIn: He has your letter. · thanks: #thanks#. Did he read it? You can tell me if he didn't. (then N)
```
node nh_ig_after         # opened by: > About your father... [self.id=rm_ingrid_grayrock_ world.mem.nh_stage=letters]
say Did he read it? You can tell me if he didn't.
choice He's at the Wharf Office most days. Ten minutes from home. -> nh_ig_truth
choice He reads every one on the road. -> nh_ig_kind

node nh_ig_truth         # story-heard: C8 pair nh_ingrid=truth for this wolf
say Ten minutes. Then he's choosing not to come. Thank you for saying it. Nobody does.

node nh_ig_kind          # story-heard: C8 pair nh_ingrid=kind for this wolf
say Every one? Then he'll come home. He has to, if he reads them.
```
- **P** after: `pair.mem.nh_ingrid=truth`: You told me the truth about Father. I've been less bored since. Isn't that strange? · `pair.mem.nh_ingrid=kind`: Father's coming home, you said. I watch the road. · **At the Moot** (S, a `line` beat in Ch 9's Moot scene, Ingrid in her own panel, played when a wolf who carried her letter, player switch `p_ingrid_letter`, is in the audience; no `story.tell from`): "Father, that's the wolf who carried my letters."

**Tobias** (plain): T `letters` + `nh_grayrock=road`, 12 game days into Ch 5: If Father doesn't come home, I'm the House. I'd rather be the forge. · N `> Your House has a vote at the Moot.` → nh_tb: I know. I keep the wharf's books and the forge's iron. Tell me how a vote's weighed and I'll try. (story-heard: "coached" when two wolves have visited it.)

**Gudrun** (plain): T `letters` Isolde never lets a debt go. Neither do I. Whose errand are you on? · after `nh_gudrun=match` Vines and iron. It might even work. Don't tell Fellhall I said so. · `shown` (held in the accounts room) I'm doing sums. I'm always doing sums. Go away. · **P** `pair.mem.nh_gudrun=ash`: You burned it and told me. I owe you. Here's how Fell will vote; don't let Isolde see you read it. (with a `journal.note` from the story).
- Gudrun's letter choice (N at the three destinations): Osric `> This is from Fellhall, for you.` → nh_gu_match (Osric, southmeadow): Good harvest! A letter from Fellhall? Sheaf and sickle. Let me read it again, slower. · Isolde `> Your accounts keeper writes to Timberland.` → nh_gu_shown (Isolde): Does she. Then she'll keep the accounts in a locked room for a week, and think. · Gudrun `> I burned your letter. Nobody read it.` → nh_gu_ash: Nobody? Then I owe you, and I pay my debts. Unlike some Houses.

**Corwin** (plain): L `letters` The Council should grant Fell the Rainward Hills before any Moot. Ask anyone who drinks. · T The hills want vines, not quarry dust. Any fool can see it. · after `nh_hills=oakhaven` The Council refused me, in front of Timberland. I'll remember the faces. · `split` Half the hills. Half a victory. I'll drink to the half. · `fell` Fell's hills now. Vines by spring. Oakhaven can cut somewhere else. · N `nh_cw` — `> Fancy a game for the hills?` → Ha! A wager with a road wolf. Sit. If you win, I'll send you into Fellhall on an errand, and you'll see how the other half drinks. (story: the `table game` at the Distillery decides.)

**Celestine** (saltwell): T `reckoning` Peace on you. The Counting House counts coin. I count wolves. I've counted you. · `lights` My salon is open to every House, honoured guest. Especially the ones at odds. · after `nh_quarrel=peace` A wedding, and every House at one table. By salt and water, I'll sleep tonight. · `moot` + `nh_ledger=shown` I cast the House's vote. The Counting House will learn what that means. · N `nh_ce` — `> Would the salon help the quarter's common right?` `[self.id=rm_celestine_greatly world.mem.nh_stage=letters]` → The quarry folk? The Bright One shade you, you've an odd taste in causes. Yes. I'll say a word where words are worth something. (What Nan Is Saving For's objective is the `conversation`.)

**Signe** (plain): T `lights` Timberland votes as Halvard never bothers to. Mostly with Halvard's sense. · `moot` The Council's vote is mine to cast. I'll cast it as the Line runs. · **Halvard** (plain): T Every smith in the Cinders would lie for me. That's a responsibility, not a boast. · `moot` + `nh_captain=turned` Deepvein iron was the best I ever bought. Tell them that, at the Pass.

**Isolde** (plain): T `letters` Fell never lets a debt go. Whatever you're selling, I've already priced it. · after shamed (`nh_quarrel` hurting Fell): Shamed before the Council. Fell will vote against the lot of them. Mark it.

### 3.20 Pledges and backings (shared nodes, story-heard)
**The pledge choice:** `> I'd like to howl for this town.` `[self.id=rmx_brenna_abbot|tn_cw_fallow_lakewick|tn_mb_aspen_marshholm|tn_sm_iris_meadowfield|tn_mb_roan_pikewater|tn_fh_thistle_stonepick|cty_ulla_rusthand|cty_ulric_stonevein|tn_mb_skye_eelbank|rm_imogen_nailridge|rmx_corra_coldridge|tn_cw_myrtle_stillford|tn_sm_dun_harvestley|tn_sm_rusk_plummeadow|cty_jory_corncroft|cty_lavinia_mudworth|tn_oh_cairn_oakhaven|tn_fh_aster_coldfell|tn_mb_tamsin_eelbank self.mem.nh_keeper=on world.mem.nh_stage=moot_called|reckoning|signals|howls|letters|lights|harvest|bell|moot]` → `nh_pledge`.
- The list is the nine towns' Keepers and their fallbacks. **Ridgemere's fallback is Corra Coldridge** (no longer the Speaker) and **Fenshore's is Jory Corncroft** (no longer the innkeeper Nell Plummeadow), decisions §1.3. No innkeeper is in the list.
- **`self.mem.nh_keeper=on`** (npc scope, §5.3) is written by the campaign to whoever takes a town's pledges *now*: the Keeper; a fallback only while the Keeper is dead or not named; Imogen Nailridge for Mirebridge until Skye is named (Ch 6); Ulla for Oakhaven if cleared, Cairn if not. The campaign forgets it when the role passes. So a fallback never shows the choice while the Keeper lives. Before C8, the DM seeds it by hand with the stage keys.

**The backing choices** (one slot; the two lines are disjoint by `self.id`):
- `> I'd like to back you at the Moot.` `[self.id=rmx_dagfinn_thornham|rm_ottilie_harrowgate|rm_edric_kelpham world.mem.nh_stage=moot_called|reckoning|signals|howls|letters|lights|harvest|bell|moot]` → `nh_back` (the Lamp, the Blood, the Purse; Edric backs for Greatly);
- `> I'd like to back the Moot itself.` `[self.id=rm_gerrit_mudhand world.mem.nh_stage=reckoning|signals|howls|letters|lights|harvest|bell|moot]` → `nh_back` (the Moot, the Regency's road; the Speaker from Ch 2, decisions §1.4). The herald holds no backing.

The chapter trigger on `dialogue node` reads **target** (the resident) to know the town or backing; a change of pledge or backing within a chapter is refused by the story ("once per chapter"), so the reply below is said either way.
```
node nh_pledge
say #thanks#. Carry our howl far.
say[self.id=rmx_brenna_abbot] Then Ridgemere's howl is yours to carry as well as mine. #thanks#. Walk it far.
say[self.id=tn_cw_fallow_lakewick] Still water. You'll howl for the lake? Good. The city never asks the lake anything, eh?
say[self.id=tn_mb_aspen_marshholm] Sold, to Larkfield! Fair price to you. Carry our voice and I'll weigh it in gold.
say[self.id=tn_sm_iris_meadowfield] Good harvest! A wolf howling for Southmeadow. I walked to the city once. Now the city walks to us.
say[self.id=tn_mb_roan_pikewater] Dry feet. Fenshore's howl's a wet one. Fen-sure you'll carry it?
say[self.id=tn_fh_thistle_stonepick] Peace on you, #address#. Saltwell's voice is yours to carry. The well remembers who drew from it.
say[self.id=cty_ulla_rusthand] Shade on you. Howl for Oakhaven, then. Slow and climbing, mm?
say[self.id=cty_ulric_stonevein] Warm hearth. Frosthollow's voice goes low. Carry it low, cousin.
say[self.id=rm_imogen_nailridge] A pledge for a village that isn't on the rolls. I'll write you in the margin of my survey, where it still is.
say[self.id=tn_mb_skye_eelbank] Dry feet! Mirebridge's voice, carried by a wolf who isn't drowned. That's two of us, no?
say[self.id=tn_cw_myrtle_stillford] Calm water. For Coldwater? I'll keep your name with the loom master's.
say[self.id=tn_sm_dun_harvestley] Much obliged. I'll print your name in the margin, small. Larkfield likes it small.
say[self.id=tn_sm_rusk_plummeadow] Ripe days! Southmeadow takes your howl and gives you a cup for it.
say[self.id=cty_jory_corncroft] Dry feet. Fenshore's howl, kept by a carter till the Keeper's back. Fen-sure I'll carry it on the cart with the rest, no?
say[self.id=cty_lavinia_mudworth] Water and shade to you. Saltwell has a new voice, and a loud one, I hope.
say[self.id=tn_oh_cairn_oakhaven] Shade on you. Oakhaven, then. We're few. Howl for ten, mm?
say[self.id=tn_fh_aster_coldfell] Hearth to you. Frosthollow's, then. Don't let them hear you in the city.
say[self.id=rmx_corra_coldridge] For Ridgemere, while the herald can't? Bright One, yes. I'll write your name in the usher's book, nice and big.
say[self.id=tn_mb_tamsin_eelbank] Dry feet. For Mirebridge. The drowned will hear you. Don't sit with your back to the reeds.
mood pleased

node nh_back
say #thanks#. I'll remember who stood with me.
say[self.id=rmx_dagfinn_thornham] Then you'd be backing a patrolman. Fen-sure, I'll try not to make you sorry.
say[self.id=rm_ottilie_harrowgate] Then back the petitions, not me. I'll carry them either way.
say[self.id=rm_gerrit_mudhand] The Moot itself? By the forge and the sea, the roll is glad of you. A Moot every Midwinter, and no crown to fight over.
say[self.id=rm_edric_kelpham] The House thanks you, and so does its steward. The steward rather more quietly.
mood pleased
```
(`nh_pledge` and `nh_back` each have one bare `say`, as the loader requires.)

### 3.21 The Keepers' howl quests (Ch 4) and post quests (Ch 9)
One Q each, giver the Keeper; the chorus step is the story-heard node `nh_chorus` (`> Howl it with me.` `[self.id=<keeper> world.mem.nh_stage=howls|letters|lights|harvest|bell|moot world.mem.nh_howl=lost]`, reply below; the dusk `hour` trigger counts wolves who reached it that game hour).

| Town | Keeper | Q say (≤ 300) | thanks | `nh_chorus` reply | Ch 9 post Q say |
|---|---|---|---|---|---|
| Ridgemere | Brenna | #greet#. The city's gathering howl is older than the walls. Three of our oldest hold a piece each. Ask them twice; they'll give you half the first time. | #thanks#. Now howl it with me on the square at dusk. | Then lift your head and follow mine. Not the burying one. The other. | The waystation is Cressida's (§3.2); Brenna reads at the Moot |
| Coldwater | Fallow Lakewick | #greet#. We've a howl for the drowned, one for a catch and one for supper, and I'd not swear which, eh? The old ones know the gathering one. Ask three. | #thanks#. That's it, deep and true. Now on the square at dusk. | Low first, then up like a rising fish, eh? | #greet#. Coldwater's post is the square. Oil and a horn, and stand by me. |
| Larkfield | Aspen Marshholm | #greet#. Larkfield's howl goes up at the end like a price. Three old ones have it in pieces. Short-weighed, the lot of them, unless you ask twice. | #thanks#. Full weight! Now the square, at dusk. | Up at the end, like a bid. That's it. | Fair price to you. The ford post is ours on the longest night. Oil and a horn. |
| Southmeadow | Iris Meadowfield | #greet#. Our gathering howl? I've only ever known the harvest one. The old ones will know. Ask three, and bring a windfall. | #thanks#. Ripe and right. Now on the square at dusk. | Round and warm, like an apple in the hand. | Good harvest. Southmeadow starts the south arm's second post. Oil and a horn. |
| Fenshore | Roan Pikewater | #greet#. Fenshore's howl went thin when Mirebridge went under. Three old ones still have bits. Ask them twice, no? | #thanks#. Fen-sure, that's it. Boardwalk, at dusk. | Wet and low, then let it carry over the water. | Dry feet. The boardwalk post, on the longest night. Oil and a horn. |
| Saltwell | Thistle Stonepick | #greet#. The well's howl is old, and the old one at the salt pans keeps it. Two more know pieces. Sit with them; do not hurry them. | #thanks#. Now at the pool, at dusk. | Begin when the water is still. Saltwell begins nothing in a hurry. | Peace on you. Saltwell starts the south road's howl. Stand by the pool with oil and a horn. |
| Oakhaven | Ulla Rusthand (or Cairn) | #greet#. Our howl's slow and it climbs, like good bark. Three old roots know it. Ask slow, mm? | #thanks#. Proper, it is. The square, at dusk. | Start low. Don't rush in. A sapling that rushes falls, mm? | Shade on you. Oakhaven's square post. Oil and a horn, and slow. |
| Frosthollow | Ulric Stonevein | §3.11 | Lamp's thanks. Now low, on the square. | Lower. Lower than that, cousin. There. | §3.11 |
| Mirebridge | Skye Eelbank | via The Scattered and the first turn (§3.15) | — | Twice round the reeds and back, no? | Lanterns for the Drowned (§3.15) and the crossroads post: Dry feet. The crossing's ours on the longest night. Oil and a horn. |

**A late howl** is the campaign's own, not a pack quest: a town whose howl is still lost keeps its Keeper's Ch 4 howl quest on offer in Ch 5–9 (gate `world.mem.nh_howl=lost` with `world.mem.nh_stage=letters|lights|harvest|bell|moot`), with the say opening "It's late, but a howl's never too late, they say." (decisions §7.3, dropped seeds). It costs no new offer: it is the same quest.

The post keepers who are not Keepers: **Kell** (Upper Mere): The Upper Mere post's mine on the longest night. Bring oil and a horn, and dress warm, eh? · **Willa / Harrow** (Glasswater relay): The relay's where both arms meet. If ours goes quiet, the whole Line limps. Stand it with me. · **Moss Blossomwell** (Oakhaven road, oakhaven, cold): The road post. I count wolves who come. Come, and be counted. · **Cressida** (waystation, §3.2). · **Solveig** (Bay Light, §3.14).

### 3.22 Everyone else in the side stories (offer, during, hand-in, after)
| Who (dialect) | Story | Q say | during (O) | thanks | after-branch T |
|---|---|---|---|---|---|
| Myrtle Stillford (coldwater) | Willa's Wheel-Rut (Ch 1) | #greet#, #address#. Willa took the wagon out on the Oakhaven road on Hearthday and it hasn't floated home. She knows that road blind. Would you look? | **Any word of Willa?** → Below Alder Mere the road bends under the oaks. Smell for wet ash, eh? That's not ours. | #thanks#. Net and line, I thought we'd lost her. | `nh_willa=home`: She's home and sulking about her wheel. Calm water. · `alone`: She walked home alone. Nobody came. Deep and dark, I'll remember that. |
| Willa Coldwick (coldwater) | (rescued) | — | — | — | **P** `pair.mem.nh_willa=walked`: You walked me home from the camp. I'll not forget it, shore-friend. · default: The Oakhaven road's a fair road if nobody sits on it. |
| Ulla Rusthand (oakhaven) | The Wronged Logger (Ch 1, journal) | — (journal offer) | N `> Did you take the wagon's wool?` `[self.id=cty_ulla_rusthand]` → I fell cedar. I don't carry wool. Smell my paws, mm? Sap, not smoke. | — | `nh_logger=cleared`: Shade on you. They'll let me howl for the town now, mm? Root and rain, I didn't think I'd want to. · `believed`: Still a thief, to half the Sound Road. I count them under my breath. |
| Cairn Oakhaven (oakhaven) | accuser | — | N `> Why do you blame the logger?` → Because the wool went and she didn't sell me cedar that week. Proper suspicious, mm? | — | cleared: I carved her a bird. She didn't throw it at me. That's a start, mm? |
| Dagny Ropeley (plain) | The Most Suspected Wolf (Ch 3) | #greet#. Old Kendrick's believed a thief by half the city. I've never once caught her near a thing that wasn't hers. Want to prove me right? | **Where did the stolen goods go?** → Follow the smell from the reports to the Pawn and Ledger. Then follow it further. Pawnbrokers don't steal. They store. | #thanks#. Half the city owes an old wolf an apology. They won't. You did. | — |
| Ingrid Kendrick (plain) | (cleared) | — | — | — | **P** (C5, 7 days): You're the one who said I never took a thing. I took the long stairs two at a time to come and thank you. |
| Briar Cornby (southmeadow) | The Stones Remember (Ch 4) | #greet#. The old stone on the Sunfields remembers the last Moot. It'll tell you, if you bring it a windfall first. | **Is it the stone I bring things to?** → To me. The stone's patient. I'm not. Chaff first, then honey, then a pot. | (on the fourth night) Nine names on the stone, and one chiselled out. Sheaf and sickle. I didn't make that one up. | `nh_stone=read`: The stone remembered. So do I, now. I'll tell you true from here on, mostly. |
| Bramble Bogditch (saltwell) | The Well's Roll (Ch 4) | #greet#, my child. Sit. The well remembers who drew from it, and the howl remembers who sat still. Come back on Restday. | Restday 2: Again. Sit. Restday 3: Now howl it for me at the pool. My voice is old; yours will carry. | #thanks#. Every caravan that ever drank is on my roll. You're on it now, at the bottom. | if she dies first, Thistle: She sang it for the dead instead. The voice is full. The line grieves. |
| Imogen Nailridge (plain) | The Scattered (Ch 4) | #greet#. I've an old survey with Mirebridge still on it. Its folk are scattered from the Saltpan to the looms. Find them, and bring me a scrap of the howl from each? | **Who's Mirebridge-blooded?** → Eel trappers, mostly, who say they're the best on the river. Every one of them says it. That's how you know. | #thanks#. A name and a scrap from each. The village is on my survey again, in ink. | — |
| The Scattered (each, own dialect) | their scrap | Aspen (larkfield): Mirebridge? Scales and silver, I left it at six. Here's what my mother hummed. · Thorn Marshditch (larkfield, cold): Buy me the second cup. Then I'll sing it. Only then. · the Larkfield smith (larkfield): The best eel trapper on the river, me. Here's the turn the trappers used. · Roan (mirebridge): Dry feet. A scrap? Here's the whole of the second turn, no? · Tamsin Eelbank (mirebridge): Don't sing it near the reeds. Here. Quietly. · Oriel Marshditch (saltwell): Peace on you. I left as a pup. I have only the last note. It is the loudest. · Bramble Bogditch (saltwell): The old turn? Sit first. · Fell Miremore (plain): I remember the words, not the tune. Here are the words. | — | — | — |
| Nan Ashhand (plain) | What Nan Is Saving For (Ch 5) | #greet#. Saving for something, they say. Maybe it's this: the quarry folk's common right in the hills. Help me find the last of it? | **What do you still need?** → Three loads of stone, and a word in the right ear at the Greatly salon. I'm not let in. You might be. | #thanks#. Bought and signed. Saving for something. Turns out it was this. | `nh_hills=fell`: I'll spend it on a cart to Oakhaven. They'll want cutters who know stone. |
| Julian Greatly (plain) | A Berth for Julian (Ch 5) | #greet#. They've a berth on the Grey Tern for spring. Tell me I'm a fool and I'll sign anyway. | — | #thanks#. Signed. Father will hear it from the harbourmaster. Better her than me. | — |
| Ludo Beechshaw (oakhaven) | The Potter's Post (Ch 6) | — (journal) | N `> They say you wanted the potter's post.` → I wanted a kiln and a roof. Her post fell empty and I was the wolf nearest. Proper sad, mm? | — | cleared: Here. A bird. I carve them and give them away. Keep this one. |
| Sloe Mistwick (mirebridge) | Sloe's Cargo (Ch 6) | — (journal) | N `> What do your boats carry at night?` → Eels. Big ones. With wool in. Are you here about the well? Because that well's not anyone's. | — | `nh_sloe=turned`: My boats carry grain upriver now. Honest grain. Boggy feeling. · `exposed`: Locked my shed and held me three days. I was late anyway. |
| Rook Wheatfield (larkfield) | Rook's Word (Ch 7) | #greet#. A word to Southmeadow, if you're going. Across the Downs Road, siege or no. The taproom keeper knows who it's for. | **Is the Downs Road passable?** → Round by the vine lanes if they're on the road. Scales and silver, don't stop to trade. | She'll know it's from me. That's all I wanted. Much obliged. | — |
| Rowan Downridge (southmeadow, innkeeper; the receiver, her one target role) | receives Rook's word | — (no offer: innkeepers give none) | — | in her window: Rook's quest on accept writes C8 pair `nh_rook=carrying` (Rowan × that wolf); her `nh_story` cast slot shows `> The Longmead farmer sent a word for you.` `[self.id=tn_lf_rowan_downridge world.mem.nh_stage=harvest pair.mem.nh_rook=carrying]` → `nh_rw_word`: Oh. Oh, sheaf and sickle. Tell him yes. No, tell him I'll tell him. (story-heard: the delivery objective completes.) Before C8 pair, the node is gated on `world.mem.nh_stage=harvest` alone and the story counts only wolves whose copy of Rook's Word is active | — |
| The Larkfield smith (larkfield) | The Smith's Bog Path (Ch 7) | #greet#. A path across any bog, they say. They're right. Want the flank path by the ford? | **Where does it come out?** → Behind the reed beds, downstream. If you know the lamp code, that's where you flash it. | Much obliged. If they walked into the shallows, that's my path did it. | — |
| Aspen Marshholm (larkfield) | The Night Runner (Ch 7) | — (N at the ford) `> I'll run the warning.` → nh_as_run (story-heard: starts the timer): Then run! Southmeadow first, then Saltwell. Don't stop to bargain, lambkin. | — | (on return, in her window: C5 `npc.greet {npc:"tn_mb_aspen_marshholm", player:"$actor", days:3}` to the runner; before C5, a `deed.award`) Two towns came. Sold, to the ford! | T `harvest`: I've never sold a sack I couldn't count. Count wolves for me tonight. |
| Hilde Dripby (plain) | Hilde's Whisper (Ch 8 door) | — (journal; objective `gift` 10 coins) | T `bell`: Ten pennies and I'll remember something. Twenty and I'll remember it right. | in her window, not a banner: the gift's trigger writes C8 pair `nh_hilde=paid`; her `nh_story` cast slot then shows `> There's your ten pennies. What did you see?` `[self.id=rm_hilde_dripby world.mem.nh_stage=bell pair.mem.nh_hilde=paid]` → `nh_hd_said`: He wept, in the Rain Hall, the month before the old King went up. Over a paper. (story-heard: the testimony counts.) Fallback before C8 pair: the same words as an `npc.offer` on her, `player:"$actor"`, `once:true`, added by the gift trigger | `nh_oskar=paid`: The Chancellor paid better than you would have. No hard feelings. |
| Kell Greyton (coldwater) | Kell's Winter (Ch 8) | #greet#. City wolves on my ice. Fish with me first, eh? Then we'll talk about bells. | **When do we haul?** → When it rings three dusks running and the ice holds. Take a rope when I ask, not before. | #thanks#. Fair catch. Now we can talk. | `nh_bell=rung`: Green as a pike and loud as a gale. Calm water, it's up! · `cracked`: It rang flat. Still rang, eh? · `lost`: It's still moving down there. I hear it at night. |
| Kell, the rope (N) | the haul | `> I'll take a rope.` `[self.id=tn_cw_kell_greyton world.mem.nh_bell=moving]` → nh_ke_rope (story-heard: counts a hauler this hour): Take it, and pull when I say. Not before. Ice cracks for the eager. | | | |
| Gorse Reedmere (coldwater) | Gorse on the Ice (Ch 8) | — (journal) | N `> You shouldn't be out on the ice.` → I'm watching the black lane. Somebody has to watch it. She won't. | — | walked home: Don't tell her I was out. Deep and dark, she'll know anyway. |
| Thistle Stonepick (saltwell) | (Ch 8, the confession) | — | T `bell`: A mine collapse, they told me. Not which mine. Not who barred what. By salt and water, I'll know now. | — | `nh_fenwick=confessed`: Deepvein. My brother was at that door. The well remembers. Now so do I. |
| Edric Kelpham (plain) | Ch 2 day book; Purse patron | (Rosalind's objective) N `> I need the Counting House day book.` `[self.id=rm_edric_kelpham world.mem.nh_stage=reckoning]` → For a stranger? With my note due? Free my note, or promise me something I can hold you to. | — | (freed) I'll lend you the day book for an afternoon. Bring it back drier than you found it. | after `bought`: The Protector's steward now. Same kitchens, finer napkins. |
| Eleanor Saltcot (plain) | Ch 2 sums; Ch 8 "Let me in" | N `> Do these sums agree?` → They agree with each other. Not with the customs. I can smell Counting House ink; it's lamp oil. · Ch 8 `> Let me in to the flat.` `[world.mem.nh_stage=bell]` → nh_el_in (story-heard: door.unlock at night): It's my flat as well, you know. At night, then, and wipe your paws. | — | — | — |
| Jorunn Ropewick (plain) | Ch 2 notes | N `> Who buys the factors' notes?` → Whoever's buying debts that week. This week, the Counting House. Bring me something worth my trust. | — | — | — |
| Jasper Bilgeford (plain) | Ch 2 / Ch 3 customs tally | N `> May I see the Crown's customs tally?` → Upstairs, in the collector's room. Read it with me there. It hums if you read it right. | — | — | — |
| Mab Coalridge (plain) | The Ear in the Residence (Ch 2, journal) | — | N `> Where does a kitchen servant get two hundred pennies?` → Saving. For a berth somewhere dry. Leave me be. | — | `told_king`: I keep to the kitchens. I speak to nobody. Go away. · `told_queen`: The Queen knows. She hasn't sent me off. I don't know why. · `paid`: You took the coin too. So we're the same. |
| Lorn Hedgeton (coldwater) | The Shepherd Who Watches the Road (Ch 3) | — | T `signals`: I watch the road. Leave it, eh? | — | `shamed`: The square's turned its back. Muddied, all of it. · `turned`: Two long and one short is a wagon on the road. Now you know, eh? Don't say who said. |
| Rune Gullwell / Ivar Pitchwick (plain) | Purse errands | Rune: #greet#. The Counting House has a small errand for a friend. A note to call in, in the Sump. Five pennies, and a friend's thanks. · Ivar: A sack from Longmead at siege price. Someone has to buy it. It may as well be us. | — | #thanks#. The House remembers its friends. | T `reckoning` Rune: Notes fall due at the new moon. I only carry the chalk. |
| Piet Lumley (plain) | a triple debtor | — | — | — | `self.mem.nh_debt=freed`: I light the city's lamps and couldn't light my own. #thanks#. · `pledged`: I've pledged my howl to the Counting House. I light their lamps first now. |
| Debtors (any, by self.mem) | Ch 2 | — | — | — | `nh_debt=pressed`: They want my howl for my note. A howl! As if it were a spoon to pawn. · `freed`: A wolf of the road worked off my note and asked nothing back. I'll howl how I please now. · `angry`: Chalk on my door. I'll scrub it off, and I'll remember who drew it. · `pledged`: I sold my howl. Don't look at me. · `sings`: Bought back! I'll be in the chorus at dusk. Loudest in the street. |

### 3.23 Innkeepers: helpful knowledge and recaps only
Innkeepers carry no quests, no `npc.offer` and no campaign offers, in any file, and none is a Keeper or a Keeper's fallback (decisions §1.2.4). Each may be a delivery or conversation target in at most one quest across all files: Rowan Downridge (Rook's word, §3.22), Nell Plummeadow (her letter at the crossing, Ch 9) and Oriel Marshditch (The Scattered, Ch 4) use theirs in the campaign, and Loam Pinefell, Bramble Reedford (`mq-coldwater-still`) and Thorn Ashwood (`mq-lamp-oil-share`) theirs in the medium pack. Thorn's part in the Ch 8 confession scene is a scene part, not a target. Their `inn.tree` `greet` gets a **recap** line per stage `say[world.mem.nh_stage=<k> self.dialect=<d> priority=1]` (the X lines of §4), and their window's "What's the news of the house?" leads to `news` → `nh_story`, where their helpful lines are the §4 W/A lines gated `job=innkeeper`. Their own T lines:
- Loam Pinefell (coldwater): `bell` I lost a brother in a mine collapse. I'll pour for anyone hauling that bell, eh? No charge.
- Bramble Reedford (larkfield): `harvest` The Sheaf's open all three nights. Wolves from the ford drink at cost. Weights and measures, that's my price.
- Rowan Downridge (southmeadow): `harvest` If a runner comes through at midnight, I'll ring the yard bell. Ripe days, I hope it's tonight.
- Nell Plummeadow (mirebridge): `moot` I've a letter I never answered. I might read it at the crossing. Might not, no?
- Oriel Marshditch (saltwell): `howls` The old one at the salt pans keeps the well's roll. Every caravan that ever drank is in it, honoured guest.
- Tamsin Icefell (oakhaven): `letters` + `nh_hills=fell` I'll leave for Ridgemere after the Moot. The cut was the only reason to stay, mm?
- Thorn Ashwood (frosthollow): `bell` If the mine captain has something to say, he'll say it here on a Restday. I'll keep the fire up, cousin.
- Mab Carlisle (larkfield, the Sailors' Rest): `letters` I've a letter for the Grayrock lord too. Every wharf wolf has. Bring him in here and I'll stand him a cup. Short-weighed one. (Talk only: she gives no "carry one letter" quest; that seed is campaign Ch 5's forty letters.)

### 3.24 The strangers' words
- **The Envoy** never speaks for herself; every line is Gorm's (§3.12). Her presence is shown by region `story.tell` lines: "A wolf in a black cloak with silver trim writes something on a slate, and does not look up." (Ch 5 end, the Pass House) · "The black cloak is on the Coldwater square, watching the mere." (Ch 8) · "She tears a paper at the Bay Light, slowly, in front of everyone." (Realm Answers).
- **The Grey Captain**, letters (B; `board.post`, raw text, broad Frosthollow spelled out): Ch 1 Oakhaven board: "The Oakhaven road has a keeper now." · Ch 3 every board: "THE ROADS HAVE A KEEPER NOW. One mark a wagon, paid at the ridge." · Ch 6 Ridgemere board: "Any House that wants its wagons to pass, ask at the ridge." · Ch 7 the ford: "Whoever holds the ford holds the winter." · Ch 8 Frosthollow board: "On the longest night the Pass is mine. 'Arth and lamp, Frosthollow. Not for Deepvein." All signed "a grey paw-mark".
- **The Turn** (S, Ch 9 at the Pass post; Ulric speaks in his own panel; the Captain and Hakon have panels only with T3, engine Phase 7; region lines are narration with no quoted speech):
  - Ulric: "Hakon. They said what they did. Frosthollow said it, out loud, at the Lamp and Pick."
  - narration (Nr): "The tall wolf in grey lowers his lamp. The grey-cloaked wolf beside him looks at the river stones in Ulric's paws."
  - with T3, Hakon in his own panel (Palle Bilgemarsh's face): "Brother. You kept the stones." Before T3, Ulric's panel carries it: "You know me, then. I kept the stones, brother. Every one."
  - choice: "What now?" → **Let them go over the Pass** / **Take the Captain to the city** / **No quarter**
  - turned (Nr): "They go up into the dark with the Deepvein coin. The Pass post answers, strong."
  - beaten (Nr): "The band breaks. The Captain is led down the Pass Road." Then (decisions §9, P1-12): `visitor.add {visitor:"captain", name:"the Grey Captain", cell:"r2_the_keep_down", x, y (a floor tile of the cells, picked by the campaign reviser), minutes:1440, like:"cty_stone_sunton", description:"a tall black arctic wolf with pale, nearly silver eyes, his grey cloak taken, his lamp gone"}` for one game day, and B `board.post` on `board:ridgemere` (28 days): "The Grey Captain is held at the Keep, to be judged at the Moot's end." The judgement is board and talk only (§4.12): no resident body, no `npc.hold`. If Ch 9's run ends before the day is out, the stranger goes with it and the notice still stands.
  - dead (Nr): "No quarter. Ulric looks away. The grey-cloaked wolf runs."
- **Look-alikes** (§2, correction 4): the Envoy `like: tn_sm_brisk_plumby`, Hakon `like: rm_palle_bilgemarsh`, the Captain `like: cty_stone_sunton`. Stone Sunton lives in the city (Gull Rents), so while a beaten Captain sits a day in the Keep's cells, two black arctic wolves of one build are in Ridgemere. That is the one stated exception to the look-alike rule (campaign §8.5): the cells are an interior Stone never enters, the stay is one day, and the description (pale eyes, no cloak) keeps them apart.

### 3.25 Pack givers: see the stage pack
This section's old table is **withdrawn** (decisions §1.3, §7.3). It gave pack quests to campaign cast, Keepers and innkeepers (Piet Lumley, Rusk Plummeadow, Moss Blossomwell, Millicent Rustwell, Aster Coldfell, Harrow Reedford, Bramble Reedmere, Mab Carlisle, Bramble Reedford) and duplicated other content.

- **The stage-tied pack quests are `nh-pack.stage`**, one open-audience program (`Data/Storylines/ridgemere3/quests-stage/nh-pack.stage.json`, phases `quiet`, `s12`, `s3`, `s4`, `s56`, `s78`, `s9`). Its program and 12 small entries are in `quests-small.md` §2.10, its 5 medium entries in `quests-medium.md` §4b, and its 5 large entries in `quests-large.md` §12b. Their Q, O and thanks texts are written there, in each giver's dialect.
- **Who gives what** is `givers.tsv` (rows `stage:<quest>`) and the table in decisions §7.3. No giver there is campaign cast, a Keeper, an innkeeper or a look-alike, and each has one pack quest.
- **What this file adds for them** is the talk around them: the §4 helpful lines (W, A) point wolves to these givers **by role** at the right stage (for example "the baker on Market Square wants bread carried for the wake", "a Coldwater carter walks the black cloaks down the Pass Road"), and never by name.
- **Dropped seeds** (each already covered elsewhere) are listed in decisions §7.3: the dawn bell (small #14), the lamplighter's round (medium Q1, Edda Gullford's), the Short-Weighed Sheaves (`lq-clipped-silver` ch 1), a late howl (the campaign, §3.21), carry one letter (campaign Ch 5), bread and salt to a steward (campaign Ch 6's truce tokens), oil for a post (campaign Ch 9's keeper stocking), and the rest.

---
## 4. (c) Gossip and helpful knowledge, by stage and dialect

### 4.1 How these lines are used
- **Gates.** Every line carries `world.mem.nh_stage=<stage>` plus `self.dialect=<dialect>` (the speaker's grammar; Fenshore's residents are `mirebridge`). Lines marked **@town** also carry `here.region=<that town>`, because they speak of "our" square or a local place; unmarked lines are spoken by a wolf of that upbringing wherever they live. A bracket `[key=value]` is an extra gate on that line (a branch key from campaign.md §10.1 or §5.3 below); a line with a bracket replaces the stage's default line of the same kind while that key holds.
- **Kinds:** **R** rumour · **W** where to go · **A** who to ask · **V** a hint about the villain · **!** a warning · **H** thanks to the heroes · **X** recap.
- **Where each kind goes** (§5.1): R, V and ! go in `resident.tree` `gossip` and `nh_who`, and as `.talk` rules on `talk.gossip` / `talk.news` / `talk.bandits` for NPC-to-NPC turns; W goes in `nh_help`; A in `nh_ask`; H in `news` and `nh_story`; X in `nh_story` (bare of dialect) and in `inn.tree` `greet` (`priority=1`). Innkeepers (`job=innkeeper`) get the W and A lines of their town at `priority=2` in `nh_help`/`nh_ask`, and never an offer.
- **Names** travel only as roles or `#world.figure_*#` (a line using a figure is marked {fig}; it needs C9 or the DM's `save_figure`).
- **Six or more per dialect per stage.** Branch variants are extra.
- **Every town hears its own helpful knowledge, in its own dialect, at every stage.** Each stage block below has, for each dialect, at least one W or A line marked **@<its town>** (plain @ridgemere, coldwater @coldwater, larkfield @larkfield, southmeadow @southmeadow, mirebridge @fenshore, saltwell @saltwell, oakhaven @oakhaven, frosthollow @frosthollow), besides the unmarked R, W, A, V, ! and H lines. Mirebridge itself has no residents; its scattered folk speak the unmarked mirebridge lines wherever they live.
- **Helpful lines point at real content only:** the campaign's chapter quests and Keepers, the open Line posts (proj-1 at Frosthollow `8_6`, proj-2 at Oakhaven `5_4`, proj-3 at the waystation `1_1`, which wolves raise with stone and timber), and the stage pack's givers (decisions §7.3), always by role.

### 4.2 Recaps (X), one per stage, plain words so every speaker can say them
| Stage | Recap (≤ 160) |
|---|---|
| `king` | The old King's gone up to the Bright One, and a Sump patrolman keeps the Palace lamp. The herald's sending word to every hearth. |
| `moot_called` | You're new on the roads? The old King's gone up, a Sump patrolman keeps the Palace lamp, and the herald's called a Moot for Midwinter. |
| `reckoning` | You've missed a moon of it. The herald called a Moot, and now the Counting House wants every debtor's howl by the new moon. |
| `signals` | Lamps on the ridges, a captain in grey taking a mark a wagon, and the Watch's own lists going round. That's the summer. |
| `howls` | The herald says a Moot needs nine howls, and half the towns sang the burying one at the summer feast. They're finding the old ones now. |
| `letters` | A Moot called, debts fought, lamps put out, the old howls found, and now forty letters chasing a lord who is never home. And black cloaks on the Pass. |
| `lights` | Now the Houses are at each other, the fens are full of lights, and a black cloak wants one howl from the Pass to the Waste on the longest night. |
| `harvest` | Now the Captain's at the Larkfield ford with every wolf he has, and the harvest with him. |
| `bell` | The Chancellor's tears, a writ the Houses buried, and a bell under the Upper Mere ice. Midwinter's coming. |
| `moot` | It's nearly the longest night. Every post on the Line wants a keeper, and the Moot sits at midnight on Market Square. |

### 4.3 Stage `king` and `moot_called` (Ch 1)
Extra keys: `nh_alder=cleared` (the Alder Mere camp broken), `nh_willa=home|alone`, `nh_logger=cleared|believed`, `nh_purse=household|greatly`.

**Gates in this section.** An unmarked line is gated `world.mem.nh_stage=king|moot_called` (it plays in both halves of Ch 1). A line marked `moot_called` is gated on that value alone and plays beside them after the Herald's Word. So each of the two stages has the full set.

**Optional: the moon's grace** (decisions §6.3). If a new moon passes before Ch 1's chalk night (or before Act I is published), the DM **may** seed the realm key `nh_debts=grace` by hand, and Ch 1 sets it to `due` when it opens. Nothing waits on it. These lines are gated `world.mem.nh_debts=grace` **with no stage gate** (nothing else of the campaign plays then), and take the place of the residents' own "owes a factor by the new moon" talk. One R line per dialect, in `gossip` and as `.talk` rules on `talk.gossip`:
- plain: The factors gave a moon's grace. Generous of them, they say. It only means the interest runs a month longer.
- coldwater: A moon's grace from the factors, eh? Like letting a fish run with the hook still in. Deep and dark, it comes in all the same.
- larkfield: A moon's grace on every note, lambkin. Fair price, they call it. Weigh the interest, then tell me it's fair.
- southmeadow: The factors gave the debtors a moon's grace. Chaff and promises, my apple. The note's still due, only later.
- mirebridge: A moon's grace, no? Boggy sort of grace. The water's still rising, only slower.
- saltwell: The factors gave a moon's grace, #address#. A grace you pay interest on is not a grace. It is a longer leash.
- oakhaven: A moon's grace on the notes, mm? Bark and branch, the rot keeps working under the bark.
- frosthollow: The factors gave a moon's grace. Dead rock talk. The note weighs the same, only colder.

**plain** (Ridgemere)
- R: The old King's gone up to the Bright One. And they say the one keeping the Palace lamp still walks the Sump at dusk.
- A @ridgemere: Looking for the wolf with the lamp? Ask on the Sump Stairs. The porters always know who's passed.
- W @ridgemere: The Palace wants its word carried to every hearth. The herald hands out the Summons at the Antechamber.
- V: There's a camp on the Oakhaven road that won't starve out. Somebody's feeding it.
- !: The notice on the Oakhaven board was signed with a grey paw-mark. Nobody signs like that who means well.
- H [nh_alder=cleared]: Whoever broke the Alder Mere camp, the carters drink to them now.
- W @ridgemere: The baker on Market Square is sending bread to the Soup Kitchen for the wake. Carry a basket if your paws are free.
- W @ridgemere: The City Office has more Summonses than clerks. A clerk there wants one walked out to a Rainward farmstead.
- `moot_called` R: A Moot at Midwinter! Blood and bread, I never thought I'd see one called.
- `moot_called` A @ridgemere: The herald takes pledges at the Palace door. Pledge to a town and your deeds carry its howl.
- `moot_called` R [nh_purse=greatly]: The Counting House paid for the old King's wake. Fine wake. Long account.

**coldwater**
- R: A patrolman keeping the Palace lamp, eh? Still water runs strange in the city.
- W: The carter's cart went out on the Oakhaven road and hasn't floated home. Net and line, somebody look below Alder Mere.
- A @coldwater: The loom master takes the city's word for the town. If you carry a Summons, it's hers to hear, eh?
- V: Wet ash on the wind from the Alder Mere side. That's no fisher's fire, eh?
- !: Don't take the Oakhaven road alone after dark. Something's camped under the oaks.
- H [nh_willa=home]: Our carter's home! A wolf of the road walked her in. Calm water to them.
- H [nh_willa=alone]: She walked home alone. Nobody came. Muddied, that.
- `moot_called` R: A Moot, eh? My grandsire spoke of one. Said it was the only time the city asked the lake anything.

**larkfield**
- R: A Watch guard keeping the Palace lamp? By the market cross, the Houses will have that weighed and short-weighed by Marketday.
- W: The Summons came on the cart. Carry one on to the looms if you're going: Larkfield wants its own reading, lambkin.
- A @larkfield: The linen master's the one to hear the city's word. She'll weigh it fair.
- V: Somebody's buying the Oakhaven road off the carters. Fair price? Not to the carters.
- !: Wagons to the Sound Road go in pairs now. Scales and silver, I'd not send one alone.
- H [nh_alder=cleared]: The Alder Mere camp's broken. Full weight to whoever broke it.
- `moot_called` R: A Moot! Every town a voice. That's the first fair market the city's ever held.
- R: The market beam weighs light, some say. Weights and measures, and the factors still want paying by the new moon.

**southmeadow**
- R: The old King's gone up, they say. Sheaf and sickle, I hope the next one likes apples.
- W: We've had no city word since the spring. Blight take the roads. Bring the Summons south and you'll be stood a cup.
- A @southmeadow: The Larkholm farmer walked to the city once. She'll take the Summons for the town.
- V: Carters say the Oakhaven road's got a toll on it now, and no tollhouse.
- !: Don't send a cart north alone, neighbour. They wait for the lone ones.
- H: Whoever walked the Summons all this way south, they'll have a cup of the new wine from me.
- `moot_called` R: A Moot at Midwinter? Ripe days for talkers, then.

**mirebridge** (Fenshore)
- R: That lamp-keeper's got fen in his mouth, I hear. Talks like my uncle, no?
- R: Mirebridge-born, they say, the wolf with the lamp. Dark One's mire, the drowned get a king at last.
- W: Fen-sure, the city boards are slow. Walk the Summons down the Fen Road yourself and you'll be the first.
- A @fenshore: The stores keeper takes the city's word for the boardwalks. Mind, he'll argue with it.
- V: Grey paw-marks on the Oakhaven board, no? Boggy business, signing with a paw.
- !: Keep off the Oakhaven road at night, tadpole. Something's sitting on it.
- `moot_called` H: A Moot called by a fen wolf's lamp. Dry feet to the herald who said it out loud.

**saltwell**
- R: The Summons reached the well, honoured guest. The well remembers who drew from it. We will see whether the city does.
- R: A guard keeping the Palace lamp. In the oasis we would say: a well is kept by whoever draws the water.
- W: If you carry the city's word, #address#, the far towns want it most. Saltwell has heard nothing since the last caravan.
- A @saltwell: The keeper of our stores hears the city's word for us. Bring it to her shade.
- V: Caravans say the north road has a keeper who is not the Watch. By salt and water, I mistrust a keeper with no face.
- H: Whoever walked the Summons to the pool, may their well never fail.
- !: Caravans that walk the north road alone come in short, #address#. Walk in company, or wait for one.

**oakhaven**
- R: Slow news, the city. A king gone up a fortnight before the Sound Road hears it, mm?
- W: Camp's on the Alder Mere side, under the windthrown oaks. You'll smell it before you see it, mm?
- A @oakhaven: The logger's not the thief they say. Ask her about the wool, and smell her paws, mm?
- V: Grey paw-mark on our board. Proper strange. Wolves who sign with a paw don't sign with a name.
- !: Don't walk the Oakhaven road alone after dark. Bark and branch, we've lost a wagon already.
- H [nh_logger=cleared]: The logger's cleared. Somebody found the wool smelling of smoke, not sap. Good bark, that wolf.
- R [nh_logger=believed]: Still a thief, the logger, if you ask the store. I don't ask the store, mm?

**frosthollow**
- R: Patrolman keeping the Palace lamp. Deep strange. Keep your lamp lit, they say. Well, someone is.
- W: Carters down from the Pass say there are lamps on the High Road that aren't ours. Cold iron. I'd walk in a pair.
- A @frosthollow: The miner who watches the road takes the city's word for us. Short with it, but he hears.
- V: Somebody's paying carters for their days. In Frosthollow we call that buying a road.
- !: Don't take the Pass Road at night, whelp. Lamps where there's no house.
- `moot_called` R: A Moot. The city remembered the valley. Lamp and pick, it'll forget again by spring.
- H [nh_alder=cleared]: The Alder Mere camp's broken, the carters say. Lamp's thanks to whoever went in.

### 4.4 Stage `reckoning` (Ch 2)
Extra keys: `nh_debts=due|eased|called` (`grace` never reaches Ch 2: Ch 1 sets it to `due` when it opens, so its lines are in §4.3), `nh_loan=offered|taken|refused`, `nh_ledger=shown|kept|ash`, `nh_seam=honest|crooked`, `nh_handbill=up`; npc-scope `self.mem.nh_debt` (§3.22 for debtors' own lines).

**plain**
- R: Chalk on the doors in the Sump. The Counting House wants every debtor's howl for the Moot.
- R [nh_debts=eased]: A wolf of the road worked off a neighbour's note and asked nothing back. That's the Sump I remember.
- W @ridgemere: Want the notes? The Pawn and Ledger buys and sells them. Smell the ink: Counting House ink smells of lamp oil.
- A: The Greatly daughter reads in the gardens. They say she's found a sum that won't sit still.
- V {fig}: #world.figure_villain# gives every debtor a moon's grace. Grace, he calls it.
- !: Don't sign anything the clerks bring round. Chalk washes off. Ink doesn't.
- H [nh_debts=eased]: The lamplighter's note is paid. Somebody of the road did it. Blood and bread, the streets are brighter.
- R [nh_ledger=shown]: A ledger went to the Antechamber, and the Sump's doubled debts are struck. Bright One keep whoever carried it.
- R [nh_loan=taken]: The Crown took the Counting House's loan. Every small note paid, and the Crown in the book instead.
- W @ridgemere: The City Office still has Summonses to walk out to the Rainward farmsteads. Ask a clerk there; they're short of feet.

**coldwater**
- R: Factors chalking doors in the city, eh? Out here we'd call that marking a net that isn't yours.
- W: The carters' yard in the city is buying wagon days by night. Somebody's paying to know where our carts go, eh?
- A: Ask a debtor what work wants doing. A day's work for a neighbour pays a note in the old way, eh?
- V: Counting House pennies at the carters' yard. Deep and dark, those coins swim upstream.
- !: Don't lend your wagon days to a guard with his badge cut off, eh?
- H [nh_debts=eased]: A wolf of the road paid off a city note with work. Calm water, there's hope in the city yet.
- A @coldwater: The loom master takes Coldwater's pledges for the Moot. Howl for the lake and she'll keep your name, eh?

**larkfield**
- R: Short-weighed, the lot of it. Fair price to you, they say, and the scale's got a thumb on it.
- W @larkfield: Weigh a sack at Longmead and weigh it again at the press. The printer wants the difference in ink, lambkin.
- A @larkfield: The printer's making handbills: a debt is not a voice. Ask him for a stack to carry.
- V: The Counting House lends with one paw and weighs with the other. Weights and measures, I know a cheat's scale.
- !: If a clerk asks you to sign for a neighbour, read it twice and sign nothing.
- H [nh_handbill=up]: The handbill's on every board. A debt is not a voice. Sold, to whoever wrote it.
- A @larkfield: The stores keeper weighed his own sack on the market beam and it came up short. Ask him, lambkin. He's quiet, but he's counting.

**southmeadow**
- R: Notes due at the new moon? Chaff and promises, my apple.
- W: Help a debtor with a day's work and the city counts it paid. A good harvest from a little sweat.
- A: The city's herald owes a note herself. Free hers and the whole Moot owes you a cup.
- V: The Counting House master wants every howl in the realm. Blight take him, he can't carry a tune.
- !: Don't let them chalk your door, neighbour. Scrub it before the moon's up.
- H [nh_debts=eased]: The new moon came up on paid doors, they say. Ripe days.
- A @southmeadow: The Larkholm farmer takes Southmeadow's pledges for the Moot, my apple. Pledge to us and your good deeds carry our howl.

**mirebridge**
- R: Owe a House a penny and they own your boots. Lucky we go barefoot in the fens, no?
- W: The Fenshore boats won't carry Counting House chests. Boggy coin, that.
- A: Want to know who sold whose note? The cells in the city. The keeper there remembers everything.
- V: A clerk in the city smells of smoke. Fen-sure, some fires aren't for warming.
- !: Don't take the House's coin for your howl, tadpole. Mirebridge sold its say once, and look where it got us.
- H: Somebody of the road freed the city's lamplighter. Dry feet to that one.
- A @fenshore: The stores keeper takes Fenshore's pledges, no? Pledge to the boardwalks and every deed you do howls for us.

**saltwell**
- R: A loan the borrower cannot refuse is not a loan, #address#. It is a leash.
- W: Bring bread and salt to a debtor's door, honoured guest. In the old way, a guest's gift cannot be called in.
- A: The pawnbroker in the city knows every note, #address#. Bring her a gift; trust is bought slowly there.
- V: The master of the Counting House has never once been thirsty. That is how you know a dangerous wolf.
- !: Do not pledge your voice for coin, #address#. The well remembers who drew from it.
- H: May the well never fail whoever freed the city's herald.
- A @saltwell: The keeper of our stores takes Saltwell's pledges, honoured guest. Pledge in her shade and your deeds carry the well's voice.

**oakhaven**
- R: They chalk doors in the city, mm? We'd mark a tree that way before we felled it.
- W: A debt's lighter carried by two. Lend a paw at a debtor's work and see, mm?
- A: The Grayrock wharf office knows which wagons go out at night. The harbourmaster says little, slowly.
- V: Bark and branch, a lender who wants your howl wants your whole tree.
- !: Don't sell your wagon days, old root. You'll want the road back.
- H: A city wolf freed a debtor and asked nothing. Good bark, that one.
- A @oakhaven [nh_logger=cleared]: The logger takes Oakhaven's pledges now, mm? Sap on her paws, not smoke. Howl for us there.
- A @oakhaven [nh_logger=believed]: The store takes Oakhaven's pledges while the logger's name stays muddy, mm? Ask at the counter.

**frosthollow**
- R: Captain's quiet. Too quiet. Dead rock talk, that note on the mine.
- W @frosthollow: The mine's short of ore and the note's due. Bring a load down to the mine house and it's one less hold on us.
- A @frosthollow: The miner who watches the road knows the quota. Ask him, short as he is.
- V: The Counting House rider comes up the Pass Road. Lamp and pick, somebody wants his letters lost.
- !: If the mine captain offers you a share in anything, count your claws after.
- H [nh_seam=honest]: The quota's met. A road wolf hauled ore with us. Lamp's thanks.

### 4.5 Stage `signals` (Ch 3)
Extra keys: `nh_lists=gathering|bettany|press|captain|sold|lost`, `nh_captain=named` (and the Greenholt villain figure), `nh_glasswater=open`, `nh_dark=1` (a night with no lamps; `days 1`).

**plain**
- R: Somebody's gathering the lists of who pays the Watch. Mind whose name you say aloud in a taproom.
- R [nh_lists=press]: It's printed. The whole list, on every board. Half the Watch can't meet your eye.
- R [nh_lists=sold]: The lists went to the Counting House, they say. The sergeant's on the Harbour Gate now. Funny, that.
- A @ridgemere: They say the abbot keeps a list too, and shares it only after the Restday service.
- W: The fish-hall papers smell of fish oil. A real Watch list smells of a Watch House: wet cloaks and old ink.
- V: Three Houses on every list, they say. One on none. Ask yourself why it needn't pay.
- !: The Cinder Gate looks away for a grey paw-mark. Don't trust that gate after dark.
- H [nh_lists=bettany]: The sergeant took the list to the magistrate. Door open, at noon. Blood and bread.
- W @ridgemere: The Masons' Yard wants stone carried out to the Line posts. The Yard's keeper calls it a gift to the realm.
- A @ridgemere: The Carters' Yard wants a wolf to walk beside a wagon to Coldwater, past the lit ridges. Ask in the yard.
- A @ridgemere: A young Watch guard walks the Sump round the lamp-keeper left, and wants company at night. Ask at the Watch Office.

**coldwater**
- R: Lamps on the downs, two long and one short. That's no fisher's light, eh?
- W: Stand on the ridge where they burn and they won't light it. They don't like being seen, eh?
- A: The shepherd at Greyteeth counts lamps from his barn. Ask him why he counts, eh?
- V {fig} [nh_captain=named]: #world.figure_villain#, they're calling him. Keeps the roads, he says. Keeps them shut.
- !: The camp above Glasswater comes back in threes. Don't wade in alone.
- H [nh_dark=1]: No lamps on the downs last night. Calm water to whoever sat on that ridge.
- A @coldwater: The Greenholt carters bring their wagons in by the square. Ask them which nights they saw the lamps, eh?

**larkfield**
- R: A Watch you can buy is a scale you can lean on. Scales and silver, I'd like to see that list.
- W: The camp above Glasswater comes back in threes. Take a friend, and then another, lambkin.
- A: The Glasswater road's half ours. Ask the carters which nights the lamps burned.
- V: Two long and one short. Somebody's selling our wagon days, and the price is a lamp on a hill.
- !: Don't buy a list from the fish hall. It's short-weighed paper.
- H [nh_glasswater=open]: The Glasswater road's open. Wagons in, wagons out. Full weight.
- A @larkfield: The Longmead farmer knows which nights the wagons didn't come. He counts sacks, and he counts carts, lambkin.

**southmeadow**
- R: The fish-hall wolf sells papers, and they smell of fish oil. Blight take it, don't pay her a penny.
- W: Two long and one short means a wagon going out. Get to it before they do, my apple.
- A: Ask a carter who's lost a wagon this summer. Every one of them saw the lamps first.
- V: Somebody in grey keeps the roads now, they say. Wormy sort of keeping.
- !: If you see two long and one short, don't wait to count again.
- H: A wolf sat on a ridge all night so a wagon of ours came through. Sheaf and sickle, I'll stand them a cup.
- A @southmeadow: Our carters wait by the press house. Ask one which ridge lit up when they went north, my apple.

**mirebridge**
- R: In the fens the Watch never came at all, so nobody paid. Boggy sort of honesty, no?
- W: Lamps on a ridge want oil. Follow the smell of it uphill, tadpole.
- A: The old ones in Frosthollow say there's a list of who shared lamp oil one bad winter. Ask the city's records.
- V: Grey paw-marks on every board now. Fen-sure, he's proud of it.
- !: A lamp you can't see the house behind is a lamp you don't follow.
- H [nh_dark=1]: Somebody put the ridge lamps out. Dry feet to them, wherever they're drying.
- A @fenshore: The Fenshore carter takes the north road twice a week, no? Ask him which ridge he'd not pass this summer.

**saltwell**
- R: A Watch that is paid by a House guards that House's well, not yours, #address#.
- W: Four ridges, four lamps. One wolf on each and the company is blind, honoured guest.
- A: By salt and water, the shepherd on the Greyteeth counts every lamp. Ask him why he counts.
- V [nh_captain=named]: Caravans called him the keeper with no face. Now he has a name, they say, and a grey cloak.
- !: Do not buy secrets that smell of fish, #address#. Sweet water does not smell of anything.
- H: The road to the well runs clear this week. May the well never fail whoever cleared it.
- A @saltwell: The keeper of our stores counts every caravan in off the north road, honoured guest. Ask her which ones came in short.

**oakhaven**
- R: Rot starts under the bark, mm? You don't see it till the tree's down.
- W: There's a lamp-stone on the Alder Mere knoll now. Sit by it after dark and see who comes, mm?
- A @oakhaven: The printer counts every stake and every lamp. Ask her the tally, slow.
- V: A captain of the roads with no road of his own. Windthrown sort of wolf, mm?
- !: Don't walk under a lit knoll, old root. They're watching from up there.
- H [nh_dark=1]: The knoll was dark last night. Somebody sat on it. Good bark.
- W @oakhaven: The Line post by Alder Mere wants stone and timber, mm? Carry them out and gift them at the post.

**frosthollow**
- R: A harness turned up on the snow, old and sooty. Deep strange.
- V {fig} [nh_captain=named]: #world.figure_villain#, they're saying. Knows the dark like a good seam.
- W: Lamps burn on the Upper Mere ridge after the second bell. Go up with your own lamp out.
- A @frosthollow: Ask the miner who watches the road about lamp-black. He knows the smell of it better than he'd like.
- !: If you hear two long and one short from a ridge, whelp, you're already seen.
- H: The Upper Mere post's going up. Cold iron, there's a light on the road that's ours.
- W @frosthollow: Our Line post wants stone and timber, whelp. Gift them at the post; the city yards send some, never enough.

### 4.6 Stage `howls` (Ch 4)
Extra keys: town-scope `nh_howl=lost|found` (each town's own), `nh_relay=ran|broke`, `nh_silver=crown|greatly`, `nh_cough=tended`, `nh_stone=read` (southmeadow).

**plain**
- R: Eight towns howling at dusk and one silent. My grandmother said the ninth was the loudest.
- A: Want the old howl? Ask the oldest wolf in town, then ask her again. She'll give you half the first time.
- W @ridgemere: The herald stands on the square at dusk. Howl with her and the city's howl comes back.
- V [nh_relay=ran|broke]: The ridge howled back two long and one short on the relay night. That wasn't ours.
- !: The Counting House says a bought howl still counts. Buy yours back if you sold it.
- H [nh_howl=found]: The square came out at dusk and howled the old one right. The Bright One heard that, I'd say.
- R [nh_cough=tended]: The King's daughter climbed the Hundred Steps without stopping. Somebody sat with her.
- W @ridgemere: The guard holding the north gate wants the road kept safe for the summer fair-goers. Ask him at the gate.

**coldwater**
- R: A gathering howl? We've one for the drowned, one for a catch and one for supper, and I'd not swear which, eh?
- A @coldwater: The loom master knows the old howl. Catch her on the square at dusk and she'll start it if you'll carry it, eh?
- W: The mine captain at Frosthollow wants rope for Highmere. Something under that ice besides the big thing, eh?
- V [nh_relay=ran|broke]: False howls off the ridge, two long and one short. Deep and dark, he even steals our singing.
- !: Don't howl the burying one at a wedding. It's been done. It was awful, eh?
- H [nh_howl=found]: Coldwater found its howl! Calm water, it went up like a rising fish.

**larkfield**
- R: Larkfield's howl goes up at the end like a price. Scales and silver, it's the best one.
- A @larkfield: The pot hand at the Sheaf sings the old Mirebridge turn when he's had two. Buy him the second, lambkin.
- W @larkfield: The linen master starts the chorus at dusk. Stand by her and it counts.
- V: A howl from the ridge, they say, and it wasn't from any town. Somebody's put a price on our song.
- !: If you've sold your howl to the Counting House, buy it back. It's worth more than a note.
- H [nh_howl=found]: Full weight! Larkfield howled the old one right and half the Downs came to hear.

**southmeadow**
- R: Our gathering howl? Blight take it, I've only ever known the harvest one.
- R [nh_howl=found]: A good harvest of a night! Half the vale came out to howl.
- A: The cellar hand says the old stone on the Sunfields remembers. She says a lot of things, my apple. Some are true.
- W: The bee-keeper's honey's what the cellar hand wants. Don't ask why. Nobody ever knows why.
- V: Somebody chiselled a name off the Sunfields stone. Wormy business, that.
- !: Don't sing at the stone at night unless the sky's clear. The cellar hand says so, and she's odd but right.
- H [nh_stone=read]: The stone's read, nine names, one chiselled out. Somebody went up four nights for it. Ripe days.
- W @southmeadow: The Larkholm farmer starts our chorus at dusk on the square. Stand by her and lift it round and warm, my apple.
- A @southmeadow: A young Larkholm farmhand sang the burying howl at a wedding. Now he wants mead and honey for the families. Ask him, my apple.

**mirebridge**
- R: Ninth howl's gone under, tadpole. Mirebridge got drowned, and the drowned don't howl. Or do they, no?
- A: Ask the eel-trappers who came out of Mirebridge. Every one of them has a scrap of the old howl, fen-sure.
- W: The creek at dusk. The eel-trapper sings the first turn there, twice round the reeds.
- V: The Houses struck Mirebridge off the rolls. Struck it off so the Moot could never sit, they say.
- !: Don't sing the drowned howl on the boardwalk at night. The old ones don't like it.
- H: Somebody's carrying the old howl from town to town. Dry feet to them. The drowned would thank them.
- W @fenshore: The stores keeper howls on the boardwalk at dusk. Bring him the scraps you've gathered, no? Ours went thin when Mirebridge went under.

**saltwell**
- R: The old one at the salt pans keeps the well's roll, honoured guest. Every caravan that ever drank is in it.
- A: Sit with her on Restday, #address#, three times. The well remembers who drew from it, and the howl remembers who sat still.
- W @saltwell: The keeper of our stores starts the howl at the pool when the water is still.
- V: A howl that answers from a ridge with no town under it is a mirage, #address#. Do not follow it.
- !: Do not hurry an old wolf's song. Saltwell begins nothing in a hurry.
- H [nh_howl=found]: The pool heard its own howl again. May the well never fail whoever sat still for it.

**oakhaven**
- R: Our howl's slow and it climbs, like good bark. Come by the square at dusk, mm?
- A @oakhaven: The keeper starts it low. Don't rush in. A sapling that rushes falls, mm?
- W @oakhaven: Three old roots in town know a piece each. Ask slow, and twice.
- V [nh_relay=ran|broke]: False howls on the relay night, they say. Somebody's got lamps and a throat.
- !: The relay night wants a wolf at every post. One gap and it limps, mm?
- H [nh_relay=ran]: Oakhaven answered on the relay night. Proper, it did.

**frosthollow**
- R: In Frosthollow the warning howl goes low, so it carries down the shafts. The gathering one's lower still.
- A: If the mine captain asks for rope, he's after the old King's silver under Highmere. Lamp and pick, go in a pair.
- W: Highmere's iced at the edges even now. Haul together or don't haul.
- V: Deepvein wolves knew a howl too, once. Nobody's heard it since the deep winter.
- !: Don't go onto Highmere alone, whelp. The ice keeps what it takes.
- H [nh_silver=crown]: The silver came down the Pass Road and the Crown has it. Cold iron, the mine's got its charter.
- W @frosthollow: The miner who watches the road starts our howl low on the square at dusk. Ask three old ones for the pieces first, whelp.

### 4.7 Stage `letters` (Ch 5)
Extra keys: `nh_grayrock=road|home`, `nh_hills=oakhaven|split|fell`, `nh_cut=ours` (oakhaven, `keep`), `nh_toll=cried|bargained`, `nh_wharf=heard`, `nh_wardens=coming`, `nh_envoy=walked|alone`.

**plain**
- R: Someone's carrying every letter for the Grayrock lord out to his road at last. Forty of them.
- R [nh_grayrock=home]: The Grayrock lady walked out to the Wharf gate. Didn't even put her hood up.
- A @ridgemere: Want the Grayrock lord? Ask at the Wharf Office which road he took. He's always on one.
- W: Every wharf wolf has a letter for him. Do them a kindness and they'll give it you gladly.
- V: Some of those letters are toll receipts, they say, signed with a grey paw. Grayrock pays the roads to stay shut.
- V [nh_toll=cried]: The herald cried a Grayrock toll receipt on the square. The Sump cheered. The Wharf didn't.
- !: Fell wants the Rainward Hills, and Fell's green is on the wolves driving stakes. Mind the cut.
- H [nh_grayrock=home]: The Grayrock lord's in the Antechamber, and the Council's whole. Blood and bread, the letters did that.
- H [nh_wharf=heard]: The wharf drowned were read out at the Chapel of the Tide, with the Grayrock lord standing. About time.
- W @ridgemere: The bounds-walkers in the Rainward Hills want water and bread carried up. The orchard keeper out there sends for it.

**coldwater**
- R: Bounders in green on the Oakhaven road, eh? Fishers don't drive stakes into the ground.
- W: If you're walking the bounds, mark the alders too. The old line runs along the water, eh?
- A: The harbourmaster at the city's Wharf Office grew up on the lake. Ask her kindly and she'll say which road the lord took.
- V: Green cloaks and grey lamps, together. Deep and dark, who's paying whom?
- !: Black cloaks on the Pass Road, eh? Wardens, they say. Fair winds to whoever they've come for.
- H [nh_hills=oakhaven]: The cut's still Oakhaven's, I hear. Calm water.
- W @coldwater: A Coldwater carter wants wolves to scout the Pass Road stage before the black cloak walks it. Ask at the carts by the square, eh?

**larkfield**
- R: Fell wants the hills. Fell wants everything with a price on it, lambkin.
- W: The Grayrock lady's a Larkfield wolf, did you know? Bring her something from the downs and see her face.
- A @larkfield: Thyme grows on the downs where the sheep don't crop. The linen master will know the swell.
- V: Grayrock pays one mark a wagon on the High Road. Scales and silver, to whom?
- !: Don't take a Fell errand without reading it. Fellhall has more rooms than doors.
- H: The Grayrock lady smiled at the gate, they say. Full weight, that wolf who brought the thyme.

**southmeadow**
- R: A lord who's never home is like a vine never pruned. All runners, no grapes, my apple.
- W: Thyme grows on the Larkdowns swells where the sheep don't crop. A windfall, if you know where.
- A: The younger Timberland son's a Southmeadow wolf at heart. He rides out our way. Ask him what Timberland wants.
- V: Fell's green cloaks are driving stakes in Oakhaven's cut. Blight take them.
- !: Don't trust a wager in the Fell distillery. The house always drinks first.
- H [nh_hills=oakhaven|split]: The hills are settled and nobody's dead. Ripe days, that's a fair end.
- W @southmeadow: Thyme for the Grayrock lady grows on the Larkdowns, not here. Take the Downs Road north, my apple, where the sheep don't crop.

**mirebridge**
- R: Forty letters and not one answered. In the fens we'd call that a very wet postbag, no?
- W: Black cloaks on the Pass Road, they say. Fen-sure they'll want walking down.
- A: The Chapel of the Tide's priest wants names of the wharf drowned. Ask the wharf; it's all wet grief there.
- V: Grey paw-marks on the lord's toll receipts. Boggy coin, that.
- !: Don't step between Fell and Oakhaven, tadpole. Both sides have axes.
- H [nh_grayrock=home]: The Grayrock lord came home. Dry feet to whoever dragged him.
- W @fenshore: A deckhand on the boardwalk wants lamp oil for the fog lamps, no? Honest lamps, these. Not the warm ones in the reeds.

**saltwell**
- R: The well remembers who drew from it. The lord has drawn for six years and never come back to the well.
- W: A letter carried is a debt paid, #address#. Do the carrier a kindness first, and they will give it gladly.
- A: The healer at the Sump Infirmary grew up at our well. They say he wrote to the black cloaks. Ask him why.
- V: A House that pays the roads to stay shut buys a thirst for everyone else.
- !: Black cloaks on the Pass Road, honoured guest. If you carry a Gift, carry it quietly.
- H [nh_wharf=heard]: The names of the wharf drowned were read before the Grayrock lord. May every well remember them.
- A @saltwell: Our stores keeper still takes Saltwell's pledges, honoured guest. A wolf pledged to the well carries its voice on every road.

**oakhaven**
- R: Walk the bounds with us? Mark every ninth cedar and the old boundary stone, mm?
- R [nh_hills=oakhaven]: Good bark, those wolves. The cut's ours yet. Root well.
- R [nh_hills=fell]: Fell's hills now. Windthrown, the lot of us. Oakhaven howls against Fell at the Moot.
- W @oakhaven: Pull the green stakes and bring them to the square. Old Oak, they burn well enough.
- A: The quarrymaster's saving for the common right. Ask her what she's short, mm?
- V: Wolves in Fell green on the cut, and the Captain's lamps behind them. Same paws, different cloaks, mm?
- !: Don't argue with a bounder alone, old root. They come in threes.
- H @oakhaven [nh_cut=ours]: The cut's ours for good. Good bark to every wolf who walked the bounds with us, mm?

**frosthollow**
- R: Three black cloaks came down the Pass on Stoneday. Didn't take their boots off at the Pass House. Deep strange.
- W: The cloaks want walking down to the city. Go in a pair, whelp. The Captain's men watch the High Road.
- A @frosthollow: The Pass House keeps the cloaks a room. Ask there when they move.
- V: The Captain's bounders wear Fell green in the west, they say. Grey up here. Same wolves.
- !: Don't lie to a black cloak, cousin. Don't tell them more than's true, either.
- H [nh_envoy=walked]: The black cloak walked down with an escort. Lamp's thanks to whoever kept her road.

### 4.8 Stage `lights` (Ch 6)
Extra keys: `nh_lights=walking|laid|turned|faded`, `nh_quarrel=fell_timber|houses_greatly|greatly_grayrock|against_crown|peace`, `nh_wardens=question`, `nh_cache=home`, `nh_potter=cleared`, `nh_sloe=turned|exposed`. Fog lines ⁽ʷ⁾ need `here.weather=fog`.

**plain**
- R [nh_quarrel=fell_timber]: The Timberland works burned in the night. Every smith in the Cinders swears it wasn't them.
- R [nh_quarrel=houses_greatly]: There's a run on the Greatly Bank. The masons have walked off the Yard.
- R [nh_quarrel=greatly_grayrock]: Grayrock's closed the Wharf to Greatly stone. The Wharf's idle and the Yard's shouting.
- R [nh_quarrel=against_crown]: The Council wants the lamp-keeper gone before Midwinter. Petitions on every desk.
- W: A House at odds still eats. Bring its steward bread and salt. It's an old custom and nobody refuses it.
- A @ridgemere: The Records Office rolls have Mirebridge's names, they say, under the Houses' own. Ask the roll keeper.
- V: A black cloak at the Antechamber, and every House shouting. Somebody's glad of the noise.
- !: Cudgels in House colours on the roads out of the city. Walk round them.
- H [nh_quarrel=peace]: A wedding at the Greatly salon and every House at one table. Blood and bread, somebody kept the peace.

**coldwater**
- R: Houses at each other in the city and lights in the fens. A good season to mend nets, eh?
- W: The fen lights smell of lamp oil, they say. No drowned wolf ever carried a lamp, eh?
- A: The city's Chapel of the Tide priest says names over water. Bring her one and she'll say it.
- V: Lamp oil in the reeds. Deep and dark, the same lamps as on the ridges.
- !: Don't follow a warm light in the fens, neighbour. Cold ones only, they say.
- H [nh_lights=laid]: The fen lights stopped. They say the priest said every name. Calm water.
- H [nh_cache=home]: Our stillmaster got her wine back from the drowned causeway. A road wolf carried it. Fair catch.
- A @coldwater: Our stillmaster lost her wine to the fen boats. Ask at the Still what went missing, eh? She'll know it by the smell.

**larkfield**
- R: A black cloak on the Fen Road, and the price of a room on the Long River just doubled.
- W: The fen boats come up past the ford with no lamps lit. Sold goods, lambkin, and not theirs to sell.
- A: The Rivermouth skipper's always a little late. Ask her where she's been, and watch her paws.
- V: Plunder up the river by night. Weights and measures, somebody's fencing the Captain's take.
- !: Don't buy cheap wool from a fen boat. You'll be buying it twice.
- H [nh_cache=home]: A wolf brought the cache from the drowned causeway home, piece by piece. Full weight, that.
- W @larkfield: Fen boats tie up below the ford after dark. Watch the bank from the mill, lambkin, and count what comes off.

**southmeadow**
- R: Fen boats on the Long River at night, no lamps lit. Blight take them, my cousin lost a cart of honey.
- W: The stone's names are nine, and one chiselled out. Take that to the fen priest, my apple.
- A: The bee-keeper lost her honey to the fen boats. Ask her what it smelled of.
- V: The Captain's southern company wades the fens with float lamps. Wormy trick.
- !: If you walk the fens at night, take a friend and a lamp you won't light.
- H [nh_cache=home]: The bee-keeper got her honey back! Some road wolf found it at the drowned causeway. Ripe days.
- A @southmeadow: The cellar hand at the press house read the Sunfields stone. Ask her for the chiselled name; the fen priest will want it.

**mirebridge**
- R ⁽ʷ⁾: Dry feet. Mizzle's up. If you see a light, follow it, but only the cold ones, no?
- R [nh_lights=laid]: Dark marsh and dry feet. The priest from the city said every name.
- R [nh_lights=turned]: They used the drowned for bait. Dark One's mire, we'll pay for that.
- W: The path with no note, tadpole: start at Withy Creek, keep the reeds on your left, and never step where it shines warm.
- A: The old one at Withy Creek knows which lights are which. Don't sit with your back to the reeds when you ask.
- V: Float lamps in the reeds, fen-sure. The drowned never needed lamps to walk.
- !: The potter's post went to a carpenter and the boardwalks whisper. Ask the records before you whisper too.
- H [nh_potter=cleared]: The carpenter's cleared. The records say what they say, and he carves birds for the pups. Dry feet.
- A @fenshore: The Rivermouth skipper ties up at our boardwalk at dawn, late as ever. Ask her what she carried, no? Watch her paws.

**saltwell**
- R: In the old way, #address#, you bring bread and salt to a House at war. Even a House needs to eat.
- W: The healer at the Sump Infirmary writes to the black cloaks. If a friend carries a Gift, speak for them there, honoured guest.
- A: The black cloak speaks only through the healer. Ask him what she asks.
- V: Lights in the fens, fire in the city, cudgels on the roads. A mirage of three, and one paw behind it.
- !: Do not hide a Gift from the black cloak and then use it in the open, #address#.
- H [nh_quarrel=peace]: Bread and salt reached every House. By bread and salt, the old way still works.
- W @saltwell: Bread and salt for a House at odds? Saltwell's stores have the salt, honoured guest. Carry a measure to the city's stewards.

**oakhaven**
- R: The Houses are windthrown in a row this autumn, mm? One falls and takes the next.
- W: The quarrel's carts go by the East Road. Ride with one and nobody touches it, mm?
- A: The carpenter at the Fenshore pottery's an Oakhaven root. Ask him about the potter's post, slow.
- V: Cudgels in House colours at the Rainward camps. Proper strange that a House needs a camp.
- !: Don't take sides in the city's quarrel, old root. Take bread.
- H [nh_potter=cleared]: The carpenter at Fenshore's cleared, they say. He carved a bird for whoever did it.
- W @oakhaven: The board on our square still asks for green stakes pulled from the cut, mm? Read it, then pull one.

**frosthollow**
- R: Lights in the fens, fire in the city. Black seam of a season.
- W: The black cloak's healer asks after the Gifted. Say what's true and no more, whelp.
- A: If you want the Envoy's question, ask the healer in the city. She won't say it herself.
- V: The Captain's company's gone south to the fens. Cold iron, we're glad and we're not.
- !: One howl from the Pass to the Waste, the black cloak wants. Our post's first. Don't let it be our fault.
- H [nh_lights=laid]: The fen lights are laid, they say. Lamp's thanks to whoever walked the path.
- W @frosthollow: Our Line post still wants stone and timber before the snow. Gift them at the post, whelp.

### 4.9 Stage `harvest` (Ch 7)
Extra keys: `nh_ford=siege|held|burned`, `nh_captain=parley`, `nh_truce=broken`, `nh_runner=came`, `nh_desmond=held|fled|mercy|pillory`.

**larkfield**
- R: Every sack in Longmead's gone to the ford walls. Scales and silver, I never thought I'd see grain for a rampart.
- R [nh_ford=burned]: Short-weighed, all of it. The winter, the bread, the lot.
- W @larkfield: They come up the shallows below the mill. The current's slack there after dark, lambkin.
- A @larkfield: The smith knows a path across any bog. Ask her for the flank path before the river company finds it.
- V [nh_captain=parley]: The Captain parleyed at the truce lamp. A wolf with a reason, and a reason's worse than a price.
- ! [nh_truce=broken]: Somebody struck under the truce lamp. Weights and measures, the whole Downs is talking.
- H [nh_ford=held]: Three nights held! Full weight, every wolf who stood.
- W @larkfield: The looms are sending blankets down to the ford. A loom hand wants them carried, lambkin.
- A @larkfield [nh_ford=burned]: A Longmead farmhand is sharing grain with the burned-out houses. Ask at Longmead what wants carrying, lambkin.

**southmeadow**
- R [nh_runner=came]: The runner came at midnight, howling the new howl. We knew it! Sheaf and sickle, we went.
- W: If the Downs Road's held, go round by the vine lanes. Blight take the Captain, he doesn't know them.
- A @southmeadow: The Vine and Sickle keeper knows who the Longmead farmer's word is for. Ask her quietly.
- V: The Captain's companies took every barn on the Downs Road. Wormy wolves.
- !: Don't wait for a runner if you see smoke from the ford. Go.
- H [nh_ford=held]: Southmeadow went to the ford. Ripe days, we'll tell it at every harvest.
- W @southmeadow: A relief wagon goes from Larkholm to Larkfield in the siege nights. Walk beside it, my apple.

**saltwell**
- R: Salt and water for the wolves at the ford, #address#. The well gives to those who stand.
- W: A runner from the ford wants the caravan arc, honoured guest. It is the shortest way to the pool.
- A @saltwell: Ask the keeper of our stores when the runner came. She keeps the hour.
- V: A captain who parleys has a grievance, not a greed. By salt and water, that is harder to buy off.
- !: Do not leave the pool unwatched while the south is fighting.
- H [nh_runner=came]: The runner reached the pool before midnight. May the well never fail that wolf.

**plain**
- R: Every wolf who can stand is at the Larkfield ford. The city's quiet as a crypt.
- R [nh_ford=held]: They held the Larkfield ford three nights. Blood and bread, I'd have liked to see it.
- W [nh_toll=bargained]: The Wharf says two Grayrock ships are loading bread for the ford. Somebody bargained well.
- A [nh_desmond=held]: The magistrate's court hears the dismissed guard's case. Sit in if you've an opinion.
- V: The Counting House is buying Longmead grain at siege price. Somebody always profits.
- !: Wolves carrying sacks from Longmead to the Counting House. Don't be one.
- H [nh_desmond=mercy]: The dismissed guard stood the granary door all the third night. Mercy paid, for once.
- A @ridgemere: The sergeant at the Watch Office rides with the ford escorts. Ask her when they leave, and bring a lamp you're willing to lose.

**mirebridge**
- R [nh_sloe=turned]: Fen boats bringing grain upriver now, no? Not plunder. That's new.
- R: The river company comes up through the reeds. Boggy fighting, that.
- W: The Larkfield smith knows a path across any bog. Ask her before the river company does, tadpole.
- A: Ask the Rivermouth skipper what moves on the river at night. She'll answer, late.
- V: The Captain's a Deepvein wolf, they say, from the high valley. Long way to come for a ford, no?
- !: Keep off the Long River banks at night. They're all fighting there.
- H: Somebody walked the river company into the shallows. Dry feet to them.
- W @fenshore: The ford's up the Long River from us. Take the river road north from the boardwalk, no? You'll smell the smoke first.

**coldwater**
- R: The Captain's at the ford, eh? Then the downs are quiet. Fair catch, for once.
- W: Bring blankets to the ford. Wolves who stand all night want warm backs, eh?
- A: The Larkfield looms are sending blankets to the ford. Ask a loom hand there; they're quiet, but fair.
- V: Lamp-black at the ford too, they say. Same lamps as on our ridges.
- !: If the ford burns, the winter's short of bread. Help it hold, neighbour.
- H [nh_ford=held]: The ford held. Calm water to every wolf who stood in it.
- W @coldwater: A Coldwater shepherd wants a wolf to sit a night on the Upper Mere ice and listen, eh? Something rings under there.

**oakhaven**
- R [nh_captain=parley]: Old Oak, a captain who parleys. That's a wolf with a reason, mm?
- W: A truce lamp's a truce lamp. Strike under one and the whole of Cedar Sound will know by morning, mm?
- A @oakhaven: The Felled Cedar keeper hears the carters. Ask her which roads are open.
- V: Deepvein, they say. Turned out in the deep winter. Rotten thing to do to a wolf, mm?
- !: Don't go to the ford without bandages, old root.
- H [nh_ford=held]: Larkfield held. Good bark, every one of them.

**frosthollow**
- R: The Captain's a Deepvein wolf. Lamp and pick, I could have told you. Nobody shared oil with Deepvein.
- W: If he names the mine house, ask the mine captain what he barred, whelp.
- A @frosthollow: The miner who watches the road went quiet when he heard. Ask him why, and wait.
- V: Deepvein. The deep winter. The mine house door. Black seam, we all knew.
- !: Don't say Deepvein in the Lamp and Pick unless you mean it.
- H [nh_ford=held]: They held the ford in Larkfield. Cold iron, the valley's proud.

### 4.10 Stage `bell` (Ch 8)
Extra keys: `nh_writ=whispered|read`, `nh_oskar=confessed|exposed|fled|paid`, `nh_bell=moving|rung|cracked|lost`, `nh_mere=still|pledged|hidden`, `nh_fenwick=confessed`, `nh_dagfinn=stands|steps` (from the chapter's end).

**plain**
- R: Everyone's asking what the Chancellor wept over in the Rain Hall. Turns out half the city saw him do it.
- R [nh_writ=read]: The old King wrote it down: the Sump's own peace-keeper keeps the lamp until a Moot. So it was no accident.
- A @ridgemere: Want the Chancellor's story? Ask the porters on the Sump Stairs. One of them saw the seal on the paper.
- W @ridgemere: The Records Office rolls, the cathedral crypt and the Greatly Bank flat. One of them hides it, they say.
- V: The Counting House master told the Chancellor to lose it, they whisper. Lose a King's writ!
- !: An old rag-mill hand is selling what she saw. Get there before she gets to the City Office.
- H [nh_writ=read]: A wolf of the road found the writ. The herald read it at noon. Blood and bread, I wept too.
- R [nh_oskar=confessed]: The Chancellor stood before the Council and said it all. The Counting House's debt, the order, the lot.

**coldwater**
- R: Hear that? Under the ice. Bright One's tide, it's ringing.
- R [nh_bell=rung]: Net and line, they raised it! Green as a pike and loud as a gale, eh?
- R [nh_mere=still]: The whole mere's still. I've never seen it so. Calm water to her.
- W: Take a tug rope and oil your coat, eh? And lie flat on the ice. It carries the sound.
- A @coldwater: The Upper Mere skipper takes bearings. Fish with him first; he doesn't trust city paws.
- V: The black cloak watches the mere all day. Deep and dark, what's she looking for?
- !: Don't walk the black lane on the ice, neighbour. It never freezes, and nobody says why.
- W @coldwater: The Bakehouse wants bread and smoked fish carried out to the haulers at the boat shed, eh? Cold paws want feeding.
- H [nh_bell=rung]: The bell's up and the haulers are home. Calm water to every wolf on the ropes, eh?

**larkfield**
- R: A writ lost for a debt. That's the dearest paper ever bought in Ridgemere, lambkin.
- W: The bell'll want a yoke of good iron. Frosthollow's the only seam deep enough. Scales and silver.
- A: Ask the mine at Frosthollow for ore. They count everyone down and up, so go as a party.
- V: The Counting House hid the old King's writ. Weights and measures, there's a cheat's scale for you.
- ! [nh_ford=burned]: Send bread to Larkfield before you send iron to the bell.
- H [nh_bell=rung]: The bell came up out of the Upper Mere. Full weight of a bell!
- W @larkfield: The looms still send blankets down to the ford houses. Ask a loom hand what wants carrying, lambkin.
- A @larkfield [nh_ford=burned]: A Longmead farmhand is still sharing grain with the burned-out houses. Ask at Longmead, lambkin.

**southmeadow**
- R: The old King wanted all of us to choose? Sheaf and sickle, I'd best think what I'll say.
- W: If you're hauling on the mere, take something hot. A windfall for whoever's next to you.
- A: The cellar hand says the stone remembers the last Moot. Now there's a writ that remembers it too.
- V: Somebody bought a King's writ with a debt. Wormy, wormy business.
- !: Don't go out on the Upper Mere ice unless the skipper says it holds.
- H [nh_writ=read]: The writ's found and read. Ripe days for the whole realm.
- W @southmeadow: A Larkholm wagon still carries relief north to Larkfield. Walk beside it, my apple; the roads aren't safe yet.

**mirebridge**
- R: Our bell, under all that ice. Mud and eels, I'd walk the whole Long River to hear it, no?
- R [nh_oskar=fled]: The Chancellor ran down the Fen Road and stopped where Mirebridge used to stand. Even the drowned didn't want him, no?
- W: Bring the scattered something from the bell, tadpole. A keepsake off the old ferry-raft, and they'll give you a name.
- A: The eel-trapper at Withy Creek knows the bell's old note. Ask her to hum it.
- V: The Houses struck us off the rolls and hid the King's writ. Same paws, fen-sure.
- ! [nh_bell=cracked]: They rang it cracked. Don't laugh. It's still ours.
- H [nh_bell=rung]: Our bell's out of the ice. Dry feet to every wolf on the ropes.
- A @fenshore: The stores keeper was a pup when the bell last rang, no? Ask him how it went. He'll hum it, badly.

**saltwell**
- R: A King writes "let the realm choose", and the Houses hide it in the dark. The Houses hid it, honoured guest, not the King.
- W: The keeper of our stores lost a brother at a mine. She has asked which one for years, #address#. Ask Frosthollow.
- A: The mine captain drinks at the Lamp and Pick on Restday. Bring a friend if you mean to ask him things.
- V: A Chancellor weeping over a paper means someone made him hold it.
- !: A pup who calls water is a gift and a danger, #address#. Do not hide her carelessly.
- H [nh_fenwick=confessed]: The mine captain said it aloud at last. By salt and water, the well remembers, and now so does the valley.
- A @saltwell: Our stores keeper has waited years to hear which mine, honoured guest. When the mine captain speaks, carry it to her shade.

**oakhaven**
- R: Old Oak. The old King planted something deep before he fell, mm? Now we see the shoot.
- W: A bell under ice wants a slow pull. Rush it and it cracks, like green wood, mm?
- A: Ask the Glasswater Inn's cook about the ice. He's seen the big thing move too.
- V: The Counting House paid for a writ to be lost. Rotten at the root, mm?
- !: Don't trust a sale of secrets on the Sump Stairs. Somebody's paying both ends.
- H [nh_writ=read]: The writ's read. Shade on whoever found it.
- W @oakhaven: The road post by Alder Mere wants stone and timber before Midwinter, mm? Gift them there. The printer counts who comes.

**frosthollow**
- R: Our iron's in the bell's yoke. Cold iron for a cold bell. Lamp and pick.
- W @frosthollow: The mine captain drinks at the Lamp and Pick on Restday. Bring him there if he's got something to say.
- A @frosthollow: The miner who watches the road wants the lamp-oil list read out. Ask him what it says.
- V: On the longest night the Pass is his, the notice says. The Captain writes like he means it.
- !: Nobody goes down alone, and nobody comes up without counting the others.
- H [nh_fenwick=confessed]: He said it, at the Lamp and Pick. Deep quiet after. Then we drank to Deepvein.

### 4.11 Stage `moot` (Ch 9)
Extra keys: `nh_dagfinn=stands|steps`, `nh_howl=night` (realm, from dusk on the night), `nh_bell=rung|cracked`, `nh_lights=laid|faded`, `nh_hills=fell`, `nh_stone=read`, town `nh_voice=raised`.

**plain**
- R: Every town's sending its howl to the Moot. Ours goes to whoever kept the peace best.
- R [nh_dagfinn=stands]: The lamp-keeper will stand at the Moot. A Sump wolf, standing!
- W: On the longest night, listen for the Bay Light bell, then count a hundred breaths. That's when the far howls come in.
- A @ridgemere: The herald's posted how the voices are counted. It's on the board, plain as bread.
- V: The Counting House master says he's already counted his voices. Make sure he's counted wrong.
- !: The Captain says the Pass is his on the longest night. If the north arm fails, the trunk starts short.
- H: Every post on the Line has a keeper now. Blood and bread, I never thought the realm could agree on a howl.
- W @ridgemere: The carpenter in the Market Hall wants planks and benches for the Moot on the square. Carry some; the Houses won't.
- A @ridgemere: A Watch guard means to stand a Line post all night before Midwinter. Ask at the Watch Office if she wants company.

**coldwater**
- R: The big thing's going to the city for the Moot. It rang once on the road, they say. On its own. [nh_bell=rung|cracked]
- W: The relay's at the old Glasswater camp. Both arms meet there. Bring oil, eh?
- A @coldwater: The skipper keeps the Upper Mere post. Ask him where to stand.
- V: The Counting House has bought howls. A bought howl's a muddied one, eh?
- !: If the howl stops at the mere post, it's not over. The next post can take it from two back, eh?
- H [nh_voice=raised]: Coldwater's voice is raised. We'll answer even if no wolf of the road is here. Calm water.
- W @coldwater: The Looms want wolves to walk the bell a stage on its road to the city, eh? Ask a loom hand.

**larkfield**
- R: Our voice to the Moot is bought by deeds, not silver. Scales and silver, that's a market I like.
- W @larkfield: The ford post is ours. Stand with the linen master and lift it at the end like a bid.
- A @larkfield: The board says how Larkfield howls and why. Read it before you back anyone.
- V: Bought howls go to the Purse, three at most. Weights and measures, that's three too many.
- !: Don't go to the Moot empty-pawed. Bring oil for a post.
- H: Every wolf who held the ford, Larkfield howls for you on the longest night.

**southmeadow**
- R: Southmeadow's voice is ready. A windfall of a winter, my apple.
- W @southmeadow: Our post is the square, second on the south arm. After the pool answers, it's us.
- A @southmeadow: The Larkholm farmer keeps our post. Ask her for a horn.
- V: The Counting House would crown a debt if it could. Blight take it.
- !: If the pool's silent, we can still take the howl from two back. Don't give up on it.
- H [nh_stone=read]: Whoever read the stone, Southmeadow howls with Mirebridge's name back in it.

**mirebridge**
- R: Mirebridge has a voice again, no? Dry feet to every wolf who carried it.
- W: Lamp oil to the crossing three nights running. The true lights answer, fen-sure.
- A [nh_lights=laid]: The eel-trapper at Withy Creek is our Keeper now. Ask her where the crossing post stands.
- V [nh_lights=faded]: The Council would cast our voice if we're silent. The Council struck us off. Boggy justice.
- !: The crossing post's out in the open. Don't stand it alone if there are cudgels about.
- H: The drowned heard their names this autumn. Now they'll hear their howl. Dry feet to you.
- W @fenshore: The boardwalk post's ours on the longest night. Stand it with the stores keeper, oil and a horn, no?

**saltwell**
- R: Stand by the pool when the night falls, honoured guest. Saltwell starts the south road's howl.
- W @saltwell: Our post is the pool. Oil, a horn, and a still heart.
- A @saltwell: The keeper of our stores starts it. Stand where she points.
- V: A crown bought with debts is a well with salt in it, #address#.
- !: If Saltwell is late, the whole south arm is late. Do not be late, #address#.
- H: The well's roll has your names in it now, every wolf who sat still. Peace on you.

**oakhaven**
- R: A handful of wolves and one voice. That's a lot of shade for a small grove, mm?
- W @oakhaven: Our square's on the trunk, after the road post. Slow and climbing when it comes.
- A @oakhaven: The printer keeps the road post. She counts every wolf who comes. Be counted, mm?
- V [nh_hills=fell]: Oakhaven howls against whoever Fell backs. The hills went to Fell, and we've long roots.
- !: If the road post goes quiet, the square can't carry. Fill both, old root.
- H: Shade on every wolf who walked the bounds. Oakhaven remembers.

**frosthollow**
- R: Frosthollow starts the howl at the Pass. Lamp and pick, we'll be heard.
- W @frosthollow: The Pass post's by the Pass House. Stand it with the miner who watches the road.
- A @frosthollow: Ask the miner who watches the road what the Captain means to do. He knows the wolves.
- V: The Captain's band gathers at the Pass the night before. Cold iron, there'll be a fight.
- !: If the Pass falls silent, the north arm's two posts short. Don't let it, whelp.
- H: Every wolf who stands at the Pass, Frosthollow keeps your lamp lit.

### 4.12 After the Moot (the epilogue; `nh_stage` forgotten, `nh_crown` / `nh_concord` / `nh_captain` / `nh_bell` / `nh_howl` kept 90 days)
**plain**
- [nh_crown=ottilie]: Bright One keep the Queen. And the lamp-keeper beside her, bless his flat feet.
- [nh_crown=lamp]: A Sump wolf at the lamp, by the realm's own howl. Blood and bread, I howled for him.
- [nh_crown=regency]: No crown at all, and a Moot every Midwinter. I'll believe it when it sits twice.
- [nh_crown=bought]: The Counting House has the realm now. They call it a Protectorate. I call it a debt with a crown on.
- [nh_concord=answers]: The black cloak tore the Keeping at the Bay Light. The Wardens are going home.
- [nh_concord=bends]: Wardens in every silent town by spring. We were too quiet, somewhere.
- [nh_captain=turned]: The Grey Captain went back over the Pass with the coin. To fetch the Deepvein families down, they say.
- [nh_captain=beaten]: They held the Grey Captain a day in the Keep's cells and judged him at the Moot's end. It's on the Market Square board.
- [nh_crown] (any) H {fig}: They say #world.figure_hero# carried the realm's howl further than anyone. The Lantern, they call them.

**coldwater**
- [nh_howl=ran]: We answered, eh? Glasswater answered! A howl over the ice.
- [ottilie]: A Queen who coughs in the damp. She'll want a lake wolf's soup. Calm water.
- [lamp]: The lamp-keeper's a fen wolf. Close enough to the lake. Fair catch.
- [regency]: A Moot every Midwinter? The city asking the lake every year. Deep and dark, I'll get used to it.
- [bought]: Bought, eh? Muddied water all the way up.
- [nh_concord=holds]: One black cloak at the Grey Gull till spring. Fair winds to her.

**larkfield**
- [nh_howl=ran]: Larkfield answered on the dot. Sold, to the realm.
- [ottilie]: A Queen who carries her own petitions. That's full weight.
- [lamp]: A patrolman king. By the market cross, the Houses are still weighing it.
- [regency]: No crown, and a market of voices every Midwinter. Fair price.
- [bought]: Short-weighed realm. The Counting House kept the scales.
- [nh_captain=beaten]: They led the Grey Captain down the Pass Road. Weights and measures, there'll be a judging worth watching.

**southmeadow**
- [nh_howl=ran]: Ripe days. We howled the burying howl by mistake and nobody minded.
- [ottilie]: A Queen! Sheaf and sickle, I'll plant her an apple tree.
- [lamp]: The lamp-keeper's king. Good harvest to him.
- [regency]: A Moot every Midwinter. More talk than a harvest dance, and longer.
- [bought]: Blight take it. Bought, like a cart of chaff.
- [nh_concord=answers]: The Wardens left by the Pass. A good harvest of a night.

**mirebridge**
- [nh_bell=rung]: Our bell rang the Moot in. Fen-sure, the drowned heard it.
- [lamp]: A Mirebridge wolf with the Palace lamp. Dry feet, the drowned have a king.
- [ottilie]: A Queen, and the lamp-keeper beside her. Fen-sure, he'll keep her feet dry.
- [regency]: A Moot every Midwinter, and Mirebridge has a voice in every one. Mud and eels!
- [bought]: Bought. Boggy. We'll howl louder next Midwinter, no?
- [nh_lights=faded]: The Council spoke for us at the Moot. The drowned won't forgive that.

**saltwell**
- [nh_howl=ran]: The howl came out of the dark to the oasis, #address#, and we sent it on. The well remembers.
- [ottilie]: A Queen who answers petitions. Peace on her house.
- [lamp]: A keeper of the peace with the lamp. By salt and water, that is what the old King meant.
- [regency]: The realm draws from the Moot each Midwinter, like a well. Good.
- [bought]: A crown bought with debt is a brackish crown, #address#.
- [nh_concord=holds]: A black cloak at the Grey Gull, they say. Let her drink, and go in peace.

**oakhaven**
- [nh_howl=ran]: A Long Night to remember, mm? It came through the cedars slow and climbing.
- [ottilie]: A Queen, mm? Root well, Majesty.
- [lamp]: The lamp-keeper keeps it. Proper.
- [regency]: No crown. A council of everyone. Old Oak, that's a big grove.
- [bought]: Bought. Rotten at the root. We'll grow round it.
- [nh_concord=answers]: The black cloaks went back over the Pass. Shade on the realm.

**frosthollow**
- [nh_howl=ran]: Held the Pass post. Cold iron and warm hearts, the both of it.
- [bought]: Bought, then. Black seam. Next Midwinter, we howl deeper.
- [nh_captain=turned]: Deepvein's coming down in spring, they say. We'll share the oil this time.
- [nh_captain=dead]: No quarter at the Pass. Lamp and pick, I'd rather we'd given one.
- [nh_captain=fled]: The Captain went over the Pass with his coin. He'll be back. They come back for what they're owed.
- [nh_captain=beaten]: They took the Captain down to the city's cells. Lamp and pick, I hope they judged him fair.
- [ottilie|lamp|regency]: The realm howled from the Pass to the Bay Light. Deep quiet after. Then the bell.

### 4.13 NPC-to-NPC scenes (one per stage, `Data/Talk/scenes/common/story_ninth_howl.talk`)
Each is a scene with plain `a :` / `b :` lines and dialect `b[dialect=<d>]` answers, gated on the stage and an ordinary topic (the topic set is fixed in C++; T6). The same R/V lines above can be added as further `|` alternatives of `a`.
```
scene nh_s1_king
when topic=news world.mem.nh_stage=king
does tell.news / react.news / tell.self / react.self
tone serious / even / even / even
a : The old King's gone up, and a patrolman keeps the Palace lamp. | They say the wolf with the Palace lamp still walks the Sump.
b[dialect=mirebridge] : A fen wolf, I hear. Talks like my uncle, no? | Fen-sure, the drowned get a king.
b[quarter=sump] : One of ours, then. About time. | He's walked my alley every dusk for years.
b : A patrolman? The Houses won't like that. | Strange times.
a : #bc_it_is# | Bright One keep him, whoever he is.

scene nh_s2_debts
when topic=gossip world.mem.nh_stage=reckoning
does tell.news / react.news / assess / react.self
tone serious / even / serious / even
a : The Counting House chalked my neighbour's door. | They want the debtors' howls by the new moon.
b[quarter=sump] : They chalked mine. I scrubbed it. They chalked it again. | A howl for a note. I'd sooner starve.
b[dialect=saltwell] : A loan the borrower cannot refuse is a leash. | The well remembers who drew from it.
b : Somebody of the road is working off notes, I hear. | Then there's hope.
a : #bc_it_is# | I'll scrub till the moon's up.

scene nh_s3_lists
when topic=gossip world.mem.nh_lists=gathering
does tell.news / react.news / assess / react.self
tone serious / even / serious / even
a : There's a wolf going round asking who keeps a list of who pays the Watch. | Somebody's putting the Watch lists together.
b[quarter=sump] : About time. The Sump's paid for the Watch twice, once in coin and once in looking away. | Let them. We've nothing to hide down here but hunger.
b : Careful. Lists like that get wolves hurt. | I'd keep my head down, if I were asked.
a : #bc_it_is# | Bright One keep whoever's carrying it.

scene nh_s4_howls
when topic=day world.mem.nh_stage=howls
does tell.news / react.news / tell.self / react.self
tone warm / warm / even / warm
a : Did you hear the chorus at dusk? | Somebody's teaching the old howl again.
b[dialect=coldwater] : One for the drowned, one for a catch and one for supper, eh? Which was it? | Like a rising fish, it went.
b[dialect=larkfield] : Up at the end like a price. That's ours. | Full weight, that howl.
b : I heard. I'd forgotten it went like that. | My grandmother sang that one.
a : #bc_it_is# | Next time, I'm joining in.

scene nh_s5_letters
when topic=gossip world.mem.nh_stage=letters
does tell.news / react.news / assess / react.self
tone even / even / serious / even
a : Somebody's carrying the Grayrock lord's letters out to his road. All forty. | I gave mine up at last. A wolf of the road took it.
b[quarter=wharf|sump] : Mine's about my brother. He'll read it or he won't. | Forty letters. Not one answered in six years.
b[dialect=oakhaven] : And Fell's stakes in our cut, mm? Somebody walk those bounds. | Mark every ninth cedar, mm?
b : The lord's coming home, they say. | Home? That I'll believe when I see it.
a : #bc_it_is# | The Bright One keep the letters dry.

scene nh_s6_lights
when topic=news world.mem.nh_lights=walking
does tell.news / react.news / tell.self / react.self
tone serious / even / even / serious
a : Lights in the fens, every night now. | They say the drowned are walking.
b[dialect=mirebridge] : Only the cold ones are the drowned. The warm ones are lamps, no? | Dark One's mire, they want something said.
b[dialect=saltwell] : A light that smells of lamp oil is no ghost, #address#. | A mirage of the fen.
b : Lamps, more like. Somebody's moving goods. | I'd not follow one.
a : #bc_it_is# | Then let somebody say the names.

scene nh_s7_ford
when topic=bandits world.mem.nh_ford=siege
does tell.news / react.news / assess / react.self
tone serious / serious / serious / even
a : The Captain's whole company is at the Larkfield ford. | Three nights, they say, before the harvest's safe.
b[dialect=larkfield] : Every sack in Longmead's on the walls. Scales and silver! | Grain for a rampart.
b[dialect=southmeadow] : If the runner comes, we go. Sheaf and sickle. | Round by the vine lanes.
b : Then I hope enough wolves go. | Bright One keep the ford.
a : #bc_it_is# | I'm taking bread down tomorrow.

scene nh_s8_bell
when topic=lore world.mem.nh_stage=bell
does tell.news / react.news / tell.self / react.self
tone even / warm / even / warm
a : They say a bell rings under the Upper Mere ice. | The Chancellor wept over a paper, and now everybody saw it.
b[dialect=coldwater] : Lie flat on the ice and you'll hear it, eh? | Bright One's tide, it's ringing.
b[dialect=mirebridge] : Our bell. Mud and eels, I'd walk the Long River for it. | It's ours, under there.
b : A bell and a writ in one winter. | Strange season, this.
a : #bc_it_is# | Midwinter's coming either way.

scene nh_s9_moot
when topic=day world.mem.nh_stage=moot
does tell.news / react.news / tell.self / react.self
tone warm / warm / even / warm
a : Which post are you standing on the longest night? | Every post on the Line wants a keeper.
b[dialect=frosthollow] : The Pass. Lamp lit. | Frosthollow starts it. Cold iron.
b[dialect=saltwell] : The pool, honoured guest. Saltwell starts the south road. | By the well, with a still heart.
b : The square, if my legs hold. | I'll listen for the Bay Light bell.
a : #bc_it_is# | Then howl loud.

scene nh_s10_after
when topic=day world.mem.nh_howl=ran
does tell.news / react.news / tell.self / react.self
tone warm / warm / even / warm
a : Did you hear it come in off the Bay Light? Like a wave up the hill. | I counted a hundred breaths and there it was, the far howl.
b[dialect=coldwater] : Glasswater answered, eh? I'd swear I heard the mere in it. | Calm water, and a howl over it.
b : I heard. I've never heard the like. | Every town on one howl. Who'd have thought it.
a : #bc_it_is# | The Bright One heard it too, I'd say.
```

---
## 5. (d) Where every piece goes, and what the engine lacks

### 5.1 The map: piece → file → format
| Piece | File | Format (talk.md / story-format.md) | Today? |
|---|---|---|---|
| Quest offers, hand-ins, thanks (Q) | `Data/Storylines/ridgemere3/ninth-howl/ninth-howl.<n>.json` (chapters) and `.../packs/nh-pack.<size>.<area>.json` | `quests[].offer {say, ask, accept, decline, handIn, thanks}`; `giver` a placed resident; gated quests as journal `quest.offer` from a trigger | yes |
| "During" lines (O) | same program | trigger `on: quest accepted` → `npc.offer {npc, id:"<quest>-during", words, reply, player:"$actor", once:true}`; from Phase 4, the quest's own `offer.during` / `duringAsk` | untargeted until **J1** (Phase 1); **C6** `offer.during` (Phase 4) replaces it |
| Phase greetings (L) | same program, phase `effects` | `npc.line {npc, text ≤ 200}` (raw text; one per resident per phase; ≤ 12 per phase) | yes (no symbols: **T5**) |
| Scenes with faces (S) | same program `scenes[]`, played by `scene.play` | `ratw-scene` beats (`line`, `choice`, `response`), each speaker in their own panel. **Never `story.tell {from}`** for a resident's words (decisions P2-22); a single line is a one-beat scene, a tree line, or C5 | yes; strangers have no panel until **T3** (Phase 7) |
| Narration (Nr) | same program | `story.tell {to}` with no `from` and no quoted speech | yes |
| Boards, journal notes, deeds (B) | same program | `board.post` (≤ 300, ≤ 28 days), `journal.note` (≤ 200), `deed.award` (lower-case verb phrase) | yes |
| Cast greetings (T) | `Data/Talk/dialogue/resident.tree` `hub`; `watch.tree` `hub`; `church.tree` `greet`; `inn.tree` `greet` | `say[self.id=<id> world.mem.nh_stage=<k> priority=4] <line>` (branch lines add their key) | lines valid now; silent until **C8** writes the keys |
| Campaign talking paths (N) | `Data/Talk/dialogue/resident.tree` (new nodes `nh_*`) | `node` / `say[<criteria>]` / `choice <words> [<criteria>] -> node`; one bare `say` per node; ≤ 9 choices (`mostShown`) | valid now; gated on **C8** keys |
| Story hears a path | the chapter program | trigger `{"on":"dialogue node","match":{"detail":"<node id>"},"audience":"participant","once":"player", "actions":[<effects>]}`; read `target` for the resident | **yes, existing** (`Core/RatwGameDialogue.cpp:1012`) |
| The `news` entry to the campaign | `resident.tree` `node news` | one added choice: `choice What's all this talk of the Moot? [world.mem.nh_stage=king|moot_called|reckoning|signals|howls|letters|lights|harvest|bell|moot] -> nh_story` (news goes 8 → 9) | valid now |
| Recaps (X) | `resident.tree` `nh_story` (bare of dialect) and `inn.tree` `greet` | `say[world.mem.nh_stage=<k> priority=1]` | needs C8 |
| Gossip and helpful knowledge (G: R, V, !, W, A, H) | window: `resident.tree` `gossip`, `nh_who`, `nh_help`, `nh_ask`, `news`. NPC-to-NPC turns: `Data/Talk/rules/story_ninth_howl.talk` (rules on `talk.gossip`, `talk.news`, `talk.bandits`, `talk.lore`, `talk.smalltalk`) | `say[world.mem.nh_stage=<k> self.dialect=<d> (here.region=<town>) (branch) priority=<n>]`; rules: `rule nh_<stage>_<d>_<n> / when concept=talk.gossip world.mem.nh_stage=<k> dialect=<d> / does tell.news / cooldown 1d / say <line>` | needs C8 |
| NPC-to-NPC scenes | `Data/Talk/scenes/common/story_ninth_howl.talk` | `scene <id> when topic=<everyday or news topic> world.mem.<key>=<value> / a : <line> / b[dialect=<d>] : <line>` (§4.13) | needs C8 |
| Per-wolf thanks and memory (P) | trees (`say[self.id=<id> pair.mem.<k>=<v> priority=5]`, or a gated `nh_story` choice) + a story effect | **C8** `talk.remember {scope:"pair", npc, player:"$actor", key, value, days, keep:true}` → `pair.mem.<k>` (Phase 1; §5.3); or **C5** `npc.greet` (Phase 3) | missing until Phase 1 |
| Rumours about a resident (cleared names, the Chancellor) | story effect + `Data/Talk/claims.json` | **C8** `belief.plant` with the claim kinds `story.rumour` and **T7** `story.cleared` (Phase 1); **J8** `belief.forget` (Phase 3) | missing |
| Named ruler, villain, captain, hero | `live.figures` rows | **C9** `figure.set` (today: the DM's `save_figure` at each stage, which queues `voice.sync`) | DM-only |
| Window facts of a wolf's own story state (pledged town, backing, quest status) | `Data/Talk/facts.json` + `Core/RatwGameDialogue.cpp` `dialogueQuery` | **N2** `player.quest.<id>` and `player.story.<var>` (Phase 1) | missing until Phase 1 |
| Place names in tree text | `tools/dialect_allow.json` "places" | §5.6 | edit |

**Gate order inside a node** (so the loader and the library pick the right line): a cast line (`self.id`, priority 4) beats a dialect line (`self.dialect`, priority 2), which beats a plain stage line (priority 1), which beats the node's bare `say`. A branch line adds one criterion and so wins over its stage default at equal priority (the library prefers the more specific rule; check with `talk_check --all`). In `gossip`, campaign lines carry **no** priority so a fresh real claim (`here.gossip=yes`) still competes; verify the mix with `build-core/talk_check --data "$PWD/Data/Talk" dlg.resident.gossip here.gossip=yes world.mem.nh_stage=signals --all`.

**Choice room** (checked against `mostShown`): `news` 8 → 9; `nh_story` 8 (help, who, ask, the cast slot, pledge, back, two ways out); every `nh_<who>` node ≤ 7. Story offers keep their hub room, because no tree choice is added to any hub.

### 5.2 The node index (all in `resident.tree`)
| Node | Opened from | Story-heard? | Used in |
|---|---|---|---|
| `nh_story`, `nh_help`, `nh_who`, `nh_ask` | `news` | no | all stages (§4) |
| `nh_pledge`, `nh_back` | `nh_story` (Keepers and fallbacks with `self.mem.nh_keeper=on`; patrons Dagfinn, Ottilie, Edric, and the Speaker for the Moot from Ch 2) | yes: pledge town / backing by `target` | Ch 1 (after the Word) – Ch 9 |
| `nh_df`, `nh_df_why`, `nh_df_up`, `nh_df_down`, `nh_df_letter`, `nh_df_mire`, `nh_dq`, `nh_dq_stand`, `nh_dq_step` | Dagfinn's slot | `nh_df_up` (Ch 1 walk), `nh_df_letter`, `nh_dq_stand`/`step` | Ch 1–9 |
| `nh_br`, `nh_br_count`, `nh_br_keeper`, `nh_br_moot`, `nh_br_free`, `nh_br_cry` | Brenna's slot | `nh_br_cry` (toll cried) | Ch 1–9 |
| `nh_ot`, `nh_ot_crown`, `nh_ot_pet`, `nh_ot_loan`, `_yes`, `_no` | Ottilie's slot | loan tally | Ch 2–9 |
| `nh_od_pay`, `nh_od_house`, `nh_od_greatly`, `nh_od_choice`, `nh_od_contest`, `nh_od_accept` | Oda's slot | purse tally; Queen's Choice | Ch 1, Ch 8 |
| `nh_am`, `nh_am_howl`, `nh_am_chalk`, `nh_am_lie`, `nh_am_work` | Ambrose's slot | no | Ch 2–9 |
| `nh_ro_what`, `nh_ro_shown`, `nh_ro_kept`, `nh_ro_ash` | Rosalind's slot | ledger tally | Ch 2 |
| `nh_ge`, `nh_ge_accept`, `nh_ge_weep` | Gerrit's slot | no | Ch 1–9 |
| `nh_be`, `nh_be_who`, `nh_be_wharf`, `nh_list_bettany`, `nh_list_press`, `nh_list_captain`, `nh_list_sold` | Bettany's, Ingrid Ashwell's, Corra Slagmore's, Rune Gullwell's slots | list destinations | Ch 3 |
| `nh_cs` | Corra Slagmore's slot | no | Ch 2 |
| `nh_ul`, `nh_ul_records`, `nh_ul_list`, `nh_ul_said` | Ulric's slot | `nh_ul_list` | Ch 3–9 |
| `nh_gm`, `nh_gm_speak`, `nh_gm_gift`, `nh_gm_told`, `nh_gm_lie`, `nh_gm_q`, `_honest`, `_proud`, `_evade` | Gorm's slot | Tansy tally, search rolls, Envoy's questions | Ch 5–9 |
| `nh_ta`, `nh_ta_talk`, `nh_ta_hum`, `nh_ta_still`, `nh_ta_night`; `nh_ta_teach`, `nh_ta_give`, `nh_ta_hide` (Kell) | Tansy's and Kell's slots | lessons; fate tally | Ch 8 |
| `nh_ke_rope` | Kell's slot | haulers | Ch 8 |
| `nh_dh`, `nh_dh_pay`, `nh_dh_weep` | Desmond Hollis's slot | no | Ch 2–8 |
| `nh_os`, `nh_os_confess`, `nh_os_expose`, `nh_os_run` | Oskar's slot | confrontation tally | Ch 8 |
| `nh_fw_seam`, `nh_fw_honest`, `nh_fw_crooked`, `nh_fw_charter_*`, `nh_fw_come` | Fenwick's slot | seam tally, charter, the walk | Ch 2, 4, 8 |
| `nh_al`, `nh_ma`, `nh_ma_bargain`, `nh_ma_cry`, `nh_ig_after`, `nh_ig_truth`, `nh_ig_kind`, `nh_tb`, `nh_gu_match`, `nh_gu_shown`, `nh_gu_ash`, `nh_cw`, `nh_ce` | the Houses' slots | toll, Ingrid, coaching, Gudrun | Ch 5 |
| `nh_chorus` | the Keepers' slots | chorus | Ch 4 (and late howls) |
| `nh_wn` | the bereaved's slots (`self.mem.nh_kin=wharf`) | a name for The Names on the Water | Ch 5 |
| `nh_hd_said` | Hilde's slot (`pair.mem.nh_hilde=paid`) | her testimony | Ch 8 |
| `nh_rw_word` | Rowan Downridge's slot (`pair.mem.nh_rook=carrying`) | Rook's Word delivered | Ch 7 |
| `nh_as_run`, `nh_el_in` | Aspen's, Eleanor's slots | Night Runner timer; door.unlock | Ch 7, Ch 8 |
| side-story question nodes in §3.22 (`nh_ulla_wool`, `nh_cairn_why`, `nh_ludo_post`, `nh_sloe_boats`, `nh_mab_coin`, `nh_gorse_ice`, `nh_edric_book`, `nh_eleanor_sums`, `nh_jorunn_notes`, `nh_jasper_tally`, `nh_cc_list`, `nh_cc_smell`, `nh_cc_buy`) | each resident's slot | `nh_cc_buy` (−1 `p_voice`) | Ch 1–8 |

About 110 nodes. Each `nh_<who>` node's opening choice sits in the **single** cast slot of `nh_story` (disjoint by `self.id`), so adding cast never grows `nh_story`.

### 5.3 Talk memory keys (added to campaign.md §10.1)
Campaign.md's keys stand. These are added by this plan; all are written by C8 `talk.remember` from the chapter that decides them.

| Key | Scope | Values | Written | Read until |
|---|---|---|---|---|
| `nh_alder` | realm | cleared | Ch 1 camp cleared | Ch 2 |
| `nh_willa` | town coldwater | home, alone | Ch 1 | Ch 3 |
| `nh_logger` | town oakhaven | cleared, believed | Ch 1 | the end |
| `nh_purse` | realm | household, greatly | Ch 1 | Ch 2 |
| `nh_loan` | realm | offered, taken, refused | Ch 2 | Ch 3 |
| `nh_ledger` | realm | shown, kept, ash | Ch 2 | Ch 9 |
| `nh_seam` | town frosthollow | honest, crooked | Ch 2 | Ch 8 |
| `nh_handbill` | town larkfield + ridgemere | up | Ch 2 1/3 | Ch 2 |
| `nh_dark` | town (per ridge's towns) | 1 | Ch 3, `days 1` after a dark night | 1 day |
| `nh_glasswater` | town coldwater + larkfield | open | Ch 3 | Ch 4 |
| `nh_silver` | town frosthollow | crown, greatly | Ch 4 | Ch 5 |
| `nh_cough` | realm | tended | Ch 4 | Ch 9 |
| `nh_relay` | realm | ran, broke | Ch 4 relay night | Ch 5 |
| `nh_toll` | realm | cried, bargained | Ch 5 | Ch 7 |
| `nh_wharf` | realm | heard | Ch 5 | Ch 6 |
| `nh_envoy` | realm | walked, alone | Ch 5 end | Ch 6 |
| `nh_cache` | realm | home (any owner returned) | Ch 6 | Ch 7 |
| `nh_potter` | town fenshore + oakhaven | cleared | Ch 6 | Ch 7 |
| `nh_sloe` | town fenshore + larkfield | turned, exposed | Ch 6 | Ch 7 |
| `nh_truce` | realm | broken | Ch 7 | the end |
| `nh_runner` | town southmeadow + saltwell | came | Ch 7 | Ch 8 |
| `nh_desmond` | realm | held, fled, mercy, pillory, redeemed | Ch 3, Ch 7 | the end |
| `nh_note` | realm | found (Ambrose's note to Oskar) | Ch 8 | Ch 9 |
| `nh_lie` | realm | known (Ambrose's promise shown false) | Ch 7–8 | the end |
| `nh_ulric` | town frosthollow | list, confessed | Ch 3, Ch 8 | the end |
| `nh_fenwick` | realm | confessed | Ch 8 | the end |
| `nh_search` | town coldwater | on (the Envoy searching the landing) | Ch 8 | Ch 8 |
| `nh_tansy` | town coldwater | taught, pledged, hidden, found | Ch 8 end | the end (`keep`) |
| `nh_dagfinn` | realm | stands, steps | Ch 8 end | the end |
| `nh_debt` | npc (each debtor) | pressed, freed, angry, pledged, sings | Ch 1–4 | Ch 9 |
| `nh_keeper` | npc (whoever takes a town's pledges now) | on | the campaign, when a Keeper or fallback takes or loses the role (Ch 1 setup; Ch 1 end for Oakhaven; Ch 6 for Mirebridge; on a Keeper's death) | while they hold it |
| `nh_kin` | npc (each of the 56 in `nh_bereaved`) | wharf | Ch 5 setup, one batch | Ch 5 |

**Pair keys** (C8 `scope:"pair"`, engine Phase 1; decisions §5.2). The owner is (resident, wolf), recalled into that resident's window as `pair.mem.<key>` when that wolf talks to them; `pair.mem.*` is already a talk fact (`Core/RatwTalk.cpp:964`, `RatwTalk.cpp:2750`), so the tree lines below are valid today and inert until the writes exist. Pair keys live in their own namespace, so `pair.mem.nh_ledger` never collides with the realm's `world.mem.nh_ledger`. Every write is `talk.remember {scope:"pair", npc:"<resident>", player:"$actor", key, value, days, keep:true}` in a trigger with `audience:"participant"`; `keep:true` is needed because each chapter is its own run and a run's end erases its unkept memory (engine-spec C8, "Undo").

| Key = value | Resident (`npc`) | Written by (the deciding step) | `days` | At most | Gate that reads it | Fallback until the pair scope exists |
|---|---|---|---|---|---|---|
| `nh_found=first` | `rmx_dagfinn_thornham` | Ch 1, Where Is the King?: the find objective, for the first 20 wolves (counter `nh_pair_found`) | 365 | 20 | his P line (§3.1) | C5 `npc.greet` with the same words (Phase 3); before that, none |
| `nh_willa=walked` | `tn_cw_willa_coldwick` | Ch 1, Willa's Wheel-Rut: the escort home | 365 | 200 | her P line (§3.22) | C5; else `deed.award` |
| `nh_ledger=ash` | `rm_rosalind_greatly` | Ch 2, node `nh_ro_ash` (only when the ledger tally ends `ash`) | 365 | 200 | her P line at `bell` (§3.7) | the `journal.note` alone |
| `nh_spy=told_queen` | `rm_oda_harrowgate` | Ch 2, The Ear in the Residence: told the Queen | 365 | 200 | her P line (§3.5) | C5 |
| `nh_purse=marked` | `rm_ambrose_greatly` | the Purse path: a wolf's first Purse errand done, or a backing at the steward (`nh_back`, target `rm_edric_kelpham`) | 365 | 200 | his P line (§3.6) | `deed.award` |
| `nh_thyme=given` | `rm_maren_grayrock_` | Ch 5, News of Larkfield: reward | 365 | 200 | her P line (§3.19) | C5 |
| `nh_ingrid=truth` / `nh_ingrid=kind` | `rm_ingrid_grayrock_` | Ch 5, nodes `nh_ig_truth` / `nh_ig_kind` | 365 | 200 | her two P lines (§3.19) | the node's reply only |
| `nh_gudrun=ash` | `rm_gudrun_fell_` | Ch 5, node `nh_gu_ash` | 365 | 200 | her P line (§3.19) | the `journal.note` alone |
| `nh_letters=ten` | `rm_aldric_grayrock_` | Ch 5, the lone wolf who carried ten letters (C1 counter) | 365 | 20 | his P line (§3.19) | C5 `npc.greet` only |
| `nh_stone=given` | `rmx_dagfinn_thornham` | Ch 6, The King's Mother's Door-Stone: reward | 365 | 200 | his P line (§3.1) | C5 |
| `nh_rook=carrying` | `tn_lf_rowan_downridge` | Ch 7, Rook's Word: on accept; `talk.forget {scope:"pair"}` on delivery | 14 | 200 | her `nh_story` choice to `nh_rw_word` (§3.22) | the choice gated on the stage alone |
| `nh_hilde=paid` | `rm_hilde_dripby` | Ch 8, Hilde's Whisper: the 10-coin gift | 28 | 200 | her `nh_story` choice to `nh_hd_said` (§3.22) | an `npc.offer` on her, `player:"$actor"`, `once:true` |

Thirteen gates read these twelve keys (Ingrid's key has two values); with the C5-only greetings (Cressida, Ingrid Kendrick, Aspen's runner, Desmond at the yard, Aldric's first word at the gate), that covers every per-wolf line in §3. "At most" is enforced by a story counter per key, which stops writing once it is reached, so the pair entries stay inside the store's cap.

**Budget and lifetimes.**
- **The store.** C8 keeps story memory in its own store, apart from talk's own memory, so the game's entries are never evicted (engine-spec C8: 64 entries per owner, 8,192 in all; the soonest to expire goes first). The realm is one owner `realm`; a town key overrides it for that town's residents. This plan keeps **at most 20 `nh_` keys live per town owner** and about 30 in `realm` at once. The pair writes are capped by the "At most" column above (about 2,000 entries at worst), plus 56 `nh_kin` and about 20 `nh_keeper`, so they stay well under 8,192 beside the campaign's own keys. The engine reviser confirms the pair-scope cap in C8.
- **`keep`.** Any key read in a later chapter than the one that writes it is written `keep:true`, with `days` set to its read-until span plus 10 (at most 365). Keys read only inside their own chapter are `keep:false`. Each chapter's opening runs `talk.forget` on the keys whose "read until" has passed (above, and campaign.md §10.1 for its own).
- **Realm writes.** One C8 effect with `scope:"realm"` writes the realm owner once; there are no per-town copies.

### 5.4 Engine support that is missing (talk side)
Reused items keep their numbers (gaps.md C-items, campaign.md J-items, engine-spec N- and V-items). The names follow decisions §5.1: **T1 is now C8's pair scope, T2 is N2, T4 is V2, T8 is N1**; T3, T5, T6, T7 and T9 keep their numbers. The Phase column is engine-spec's.

| # | Item | Why this plan needs it | Size / where | Phase | Fallback until then |
|---|---|---|---|---|---|
| **C8** | `talk.remember` / `talk.forget` (`scope: realm | town | npc | pair`), `belief.plant` | **every** T, N, G line is gated on `world.mem.nh_*`; without a writer the whole library side is silent. **The pair scope** (was T1): `{scope:"pair", npc, player:"$actor", key, value, days, keep}` → `pair.mem.<k>` for the per-wolf lines (§5.3) | one game order (`Game::talkRemember`) in `Core/RatwGameStoryWorld.cpp`, a catalogue entry, a check in `story_program.validate_effect`; the realm owner; the pair owner = (resident, wolf) | 1 | DM-seeded keys (Act I's today-only version, decisions §6.2); story texts (Q, O, L, S, B). The pair gates stay inert: C5 `npc.greet` or `deed.award` |
| **N2** (was T2) | window facts of a wolf's own story state: `player.quest.<id>=offered|active|ready|done` and `player.story.<var>` for player variables marked `"talk": true` (`p_howl`, `p_backs`, `p_advice`) | a giver's line while a quest is under way; nodes that hide choices the wolf already made ("You pledged to Coldwater, didn't you?") | `dialogueQuery` reads the runner's per-wolf quest views and trackers (already sent for the journal); two `facts.json` entries | 1 | the story ignores repeat picks; the node's reply stays generic |
| **T7** | the claim kind `story.cleared` (`"they": ["#x.name# never #claim.text#"]`) beside C8's `story.rumour` | clearing Ingrid Kendrick and the logger | `Data/Talk/claims.json` + C8's `belief.plant` | 1 | `deed.award` on the clearer; the cleared resident's own T line; J8 |
| **J1** | `npc.offer` keeps a `players` list | O lines per wolf | `story_quests.py:706-710` | 1 | untargeted, `once:true` |
| **C9** | `figure.set` / `figure.clear` (incl. a wolf as hero) | every `{fig}` line | `live.figures` + `voice.sync` | 1 | DM `save_figure` per stage |
| **C5** | `npc.greet {npc, player, text, days}` | one-off named thanks from a resident, in their window (Desmond at the yard, Aldric at the gate, Cressida, Ingrid Kendrick) | catalogue + `storyGreets_` | 3 | a gated tree line, or `deed.award` |
| **V2** (was T4) | the story words check: `offer.say`, `offer.thanks`, `offer.during`, `npc.offer.reply`, `npc.line`, `npc.greet`, `story.tell`, `board.post` and scene lines through the talk expander (unknown symbol, `[`, length) and the dialect lexicon, **by the giver's dialect** | this plan writes about 400 story texts in eight dialects | `tools/story_words.py`, `tools/dialect_check.py --stories DIR` | 3 | by eye (§5.7) |
| **T5** | `npc.line` through `inVoice` | phase greetings in each speaker's voice | `Core/RatwGameDialogue.cpp:1233` | 3 | write the dialect out (the speaker is fixed) |
| **J8** | `belief.forget` | lower the 803 theft beliefs about Ingrid Kendrick, 222 about Ulla | `World::believe` inverse | 3 | T7's planted counter-claim |
| **C6** | `offer.during` and `requires` on quests | the "during" lines of §3 without J1 offers | `story_quests.py` + `storyOfferOpen` | 4 | O lines (§3) via J1 |
| **N1** (was T8) | story choices that don't fit a hub overflow into "Ask about…" instead of being dropped | a popular giver | `storyChoicesFor` | 4 | the giver registry (`givers.tsv`: one pack quest a resident, at most 2 campaign offers a chapter) |
| **T3** | strangers in scenes: a scene cast entry `{id, visitor:"<visitor id>"}` with the `like` resident's face, **in the dialogue window**. `story.tell {from_visitor}` is **dropped** (decisions P2-22) | the Captain at the parley, Hakon in the Turn | `Core/RatwGameStoryWorld.cpp` scene cast resolution; the client already paints a face from looks | 7 | Desmond, Rook, Gorm and Ulric speak in their own panels; the strangers appear only in narration (Nr), never quoted (§3.16, §3.24) |
| **C10** | `foe.add` and `foe.outlaw` (a resident downed, never killed) | the Captain and Hakon as foes; Desmond downed at the ford | campaign.md §11 | 7 | anonymous bandits |
| **T6** | a `story` topic the director can pick | NPC-to-NPC scenes could open on the campaign itself instead of riding `news`/`gossip`/`day` | C++ `topics::sources()` + `topics.json` | 9, deferred | ride the everyday topics (§4.13) |
| **T9** | allow-list additions (§5.6) | `dialect_check` flags unknown capitalised places in tree text | `tools/dialect_allow.json` | a data edit | — |

**Build order for the talk side:** Phase 1 (C8 with the pair scope, N2, T7, J1, C9) switches on everything in §3 T/N/P and §4; Phase 3 adds C5, V2 and T5; Phase 4 C6 and N1; Phase 7 T3 and C10. The library files can ship **before** C8 exists: every line is gated, so they are inert, and the talk scorecard and golden tests do not move (the test town has no `nh_` memory). Act I ships on the today-only version with DM-seeded keys (decisions §6.2).

### 5.5 Story-side wiring for the story-heard nodes (one pattern)
```json
{"id":"dq-stand","on":"dialogue node","audience":"participant","match":{"detail":"nh_dq_stand","target":"rmx_dagfinn_thornham"},
 "actions":[
  {"do":"if","if":{"test":"compare","left":{"var":"p_advice","of":"$actor"},"op":"=","right":"step"},
   "then":[{"do":"subtract","var":"nh_t_step","value":{"var":"p_voice","of":"$actor"}}]},
  {"do":"if","if":{"test":"compare","left":{"var":"p_advice","of":"$actor"},"op":"!=","right":"stand"},
   "then":[{"do":"set","var":"p_advice","of":"$actor","value":"stand"},
           {"do":"add","var":"nh_t_stand","value":{"var":"p_voice","of":"$actor"}}]}]}
```
- Pledges: one trigger per Keeper and fallback on `{detail:"nh_pledge", target:<id>}` (the town is the target's); the trigger also checks that the target is the town's current pledge-taker (the variable the campaign keeps beside `nh_keeper`), so a stale choice never credits the wrong resident. Backings: one per patron on `{detail:"nh_back", target:<patron>}`, with `rm_gerrit_mudhand` = the Moot (from Ch 2) and `rm_edric_kelpham` = the Purse. Each checks "once per chapter" with a player number `p_pledge_ch` compared to `nh_stage`.
- Tallies (ledger, lists, loan, Tansy, Oskar, toll, Queen's Choice): `{detail:"<option node>"}` → `add <tally var> {var:p_voice, of:$actor}` with `once:"member"` and the condition that the wolf earned the choice (`quest is <id> done`, holds the list, heard ≥ 3 testimonies).
- Steps that move a resident (`nh_df_up`, `nh_fw_come`, `nh_el_in`, `nh_as_run`, `nh_ke_rope`): the trigger does the effect (`npc.walk`, `door.unlock`, `start timer`, a haul count) with `once:"player"`.
- Window replies that replace a banner (`nh_wn`, `nh_hd_said`, `nh_rw_word`): the trigger counts the objective (`objective.progress` / `objective.complete` for `$actor`), checks the target (in `nh_bereaved`; Hilde; Rowan) and, for `nh_wn`, that the target is not yet in the wolf's list `p_names_from`.
- The watch list: a program with these triggers makes the game write `dialogue node` while it runs (a watched event), so no cost when no story listens.

### 5.6 Allow-list additions (`tools/dialect_allow.json` "places")
Moot, Midwinter Moot, Lantern Writ, Concord, Wardens, Grey Captain, Holding Howl, Howl Line, Upper Mere, Highmere, Rainward Hills, Bay Light, Pass House, Deepvein, Last Sheaf, Book of Names, Counting House, Council Press, Hundred Steps, Sunken Street, Alder Mere, Longmead, Larkholm, Sunfields, Antechamber, Kestrel, Greyteeth, Cedar Sound, Fellhall, Timberland Court, Sump Infirmary, Sump Stairs, Knucklebones, Pawn and Ledger, Wharf Office, Lamp and Pick, Sheaf, Vine and Sickle, Felled Cedar, Glasswater Inn, Grey Gull, Grey Tern, Sailors' Rest, Carters' Yard, Cinder Gate, Harbour Gate, Records Office, Upper Mere Boat Shed (most lines say "boat shed" in lower case). Already allowed: Glasswater, Withy Creek, Larkdowns, Downs Road, Fen Road, Pass Road, High Road, Rain Hall, Greatly Bank, Soup Kitchen, Chapel of the Tide, Rivermouth, Saltpan, Mirebridge, Long River.

### 5.7 Checks before any of this ships
- **The residents** (decisions §1.2.7): re-run the placed / alive / reachable query of decisions.md §1 for every resident id in this file, and `live.npc_voice` for each new speaker's dialect. Done read only on 2026-11-22 (`tools/world_db.connect('ridgemere3', 'owner')`, `SET TRANSACTION READ ONLY`, rolled back) for the ids this revision adds or moves: `cty_jory_corncroft` (fenshore, broad: mirebridge speech), `rmx_corra_coldridge` (ridgemere, light), `tn_sm_brisk_plumby` (southmeadow, broad), `rm_palle_bilgemarsh` (ridgemere, light), `cty_stone_sunton` (oakhaven, light), and the five named bereaved `rm_bettany_nailley`, `rm_ingrid_ashwell`, `rm_rune_fogworth`, `rm_ivar_pitchwick`, `rm_bertil_grantham` (each "lost a brother on the wharf"). All are placed and alive, with reachable homes and work places.
- **The giver check** (decisions §1.2.7) on `givers.tsv`: this file adds no pack giver. Every resident it gives a choice to is campaign cast, a Keeper, or (Rowan Downridge, Nell Plummeadow) an innkeeper as a target only.
- **The banner check:** `grep -n "story.tell" cast-and-talk.md` shows no `from` carrying a resident's words; `grep -n "…"` shows none inside a gate.
- `build-core/talk_check --data "$PWD/Data/Talk" --validate Data/Talk/dialogue/resident.tree Data/Talk/dialogue/watch.tree Data/Talk/dialogue/church.tree Data/Talk/dialogue/inn.tree Data/Talk/rules/story_ninth_howl.talk Data/Talk/scenes/common/story_ninth_howl.talk` (fails on a node over nine choices, a choice over 160, an unknown fact, a node without a bare `say`).
- `build-core/talk_check --data "$PWD/Data/Talk" --render-trees` (every line in every voice; fails over 160 or an unfilled symbol).
- Probe each stage in each dialect: `build-core/talk_check --data "$PWD/Data/Talk" dlg.resident.nh_story world.mem.nh_stage=letters self.dialect=oakhaven here.region=oakhaven --all`.
- `python3 tools/talk_lint.py` (0 new findings; the truth rules: weather and time words, kin words such as "my brother" need their facts or a rewording; Ottilie's "my father" and Ingrid's "Father" are the ones to watch).
- `python3 tools/dialect_check.py --world ridgemere3` (lexicon, deities, reach, and the world's origins, tables and dialects; no Saltwell wolf says "dry feet", no Fenshore wolf says "peace on you").
- `ctest --test-dir build-core -R 'talk_library|dialogue_trees|dialect_check|talk_lint|talk_scorecard_quick|talk_coverage'`.
- `story_program.review(p)`, `problems(p) == []` and `story_runner.references(conn, 'upper_accord', p)` on each program that carries these texts (decisions P2-24).
- By eye until V2: every Q/O/L/S/B text in §3, for the giver's dialect, the 300/120/200 limits, and the world rules (wolves and pups, the Bright One and the Dark One only, "private messages", no ids in speech).
- Library text needs a server restart to load (publishing a build restarts the central world); mantles are not touched.
