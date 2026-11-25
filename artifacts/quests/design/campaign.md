# The Ninth Howl: the first grand campaign of Ridgemere 3 (final design)

Lead designer's merge of the three proposals (`proposal-weave.md`, `proposal-crown.md`, `proposal-wild.md`), with every flaw the three judges named fixed. **Weave is the frame.** From crown come the opening, the Lantern Writ, the intrigue side stories and the dialect gossip discipline. From wild come the recurring wild villain, the Howl Line, the playable finale and the lone-wolf mechanics.

World: Ridgemere 3 (world key `ridgemere3`, db `ratw_ridgemere3`, world id `upper_accord`). Every resident id below was checked as **placed** against the resident dump (`/tmp/castsurvey/res2.tsv`, 777 placed). Every interior id was checked against the reachable list (`/tmp/lore_survey/reachable.txt`). Purses were checked against the 2026-11-22 checkpoint. Nothing in the repo or the world was changed. The one unreachable room, `rm_the_rain_hall`, stays offstage on purpose. The one unplaced resident, `cty_eskar_rimedrift`, is never given a body: he appears only as a foe or a mute stranger dressed `like: cty_stone_sunton` (a placed, living black arctic wolf).

**Revised to the coordinator's decisions** (`decisions.md`, binding) and the completeness critic (`critic.md`, P0-1 to P2-24); §0.4 lists every change and where it sits. Every resident newly named here (the two new fallback Keepers, the two re-picked look-alikes, Speaker Gerrit as the Moot's patron) was re-checked **read only** on 2026-11-22 against `live.npcs`, `live.npc_state` and `world.links` / `world.cells`: placed, alive, home and work reachable. The cross-pack giver registry is `givers.tsv` next to this file: **no campaign resident carries a pack quest**.

**Revised again on 2026-11-22 to the user's answer (a)** (binding; decisions §6): no 40-day calendar. Each chapter moves on by **participation** (a share of the active wolves doing its named tasks) **plus a minimum time**, or when an admin **pushes** it (Advance) from the Campaigns console; a soft maximum raises "ready to push" instead of moving by itself (§3.2; engine-spec R1–R3). The moon, the festivals and Midwinter are flavour and optional timing. §0.4's last row lists every place this changed.

**Contents:**
- §0 At a glance
- §1 Premise
- §2 Design rules
- §3 Structure, readiness and pacing
- §4 The campaign's state (world variables and switches)
- §5 The nine voices and the Howl Line
- §6 The chapters, 1–9. §9.1–9.8 are the subsections of Chapter 9: the night, the Moot, the counting, the endings and recognition.
- §7 Quest packs
- §8 Cast
- §10 Talk plan
- §11 Trigger features
- §12 Risks and checks

---

## 0. At a glance

| | |
|---|---|
| Campaign | **The Ninth Howl**. Programs `ninth-howl.1` … `ninth-howl.9` in `Data/Storylines/ridgemere3/ninth-howl/` (never `programs/`, which three tests count) |
| Shape | 9 chapters in **3 acts**, each act approved as its own `campaign.approve` campaign: Act I "The Lamp" (Ch 1–3), Act II "The Howls" (Ch 4–6), Act III "The Moot Night" (Ch 7–9). **Act I ships on the today-only version** (§11.4) with no engine change; Acts II and III ship when the act before has closed and the DM publishes them, with whatever engine has landed, falling back to their today-only rows (§3.1). |
| Audience | every chapter `{"kind":"everyone","days":30,"cap":5000}` |
| Span | **No fixed length** (user, 2026-11-22). Each chapter moves on when enough active wolves have done its tasks and it has run its minimum time, or when an admin pushes it (§3.2). Typical: about **24 real days** at 200 wolves and about **30** at 5 (a game day is 4 real hours). Past a chapter's soft maximum nothing moves by itself unless that chapter is set to `auto`. |
| Main line | King Gunnar died, and by an old line in his own paw the Palace lamp fell to a Sump patrolman. A Moot of the realm's nine voices is called, to sit when the realm is ready, and the Houses race to make it fail. On the Moot night the realm runs one howl from the Pass and the Waste to the Bay Light, and then the Moot chooses who reigns. |
| Villains | **Ambrose Greatly** (the purse: debts, bought voices, the hidden writ), beaten by ledgers and votes, never by teeth. **The Grey Captain, Eskar Rimedrift of Deepvein** (the roads: camps, signal lamps, the ford siege, the Moot-night strike), who can be beaten, turned or driven off. Ambrose pays the Captain with a promise he cannot keep. |
| Endings | Who reigns: Queen Ottilie with the Lamp beside her / the Lamp-King / the Regency of the Moot / the Bought Crown. And the Concord's verdict: the Realm Answers / the Realm Holds / the Realm Bends. That gives 12 combinations, and every one is a full ending. |
| Side storylines | 3 to 5 per chapter (36 in all), plus one **door quest** per chapter for late joiners |
| Per wolf | 4 trackers in all: "Your voice", "Your town's howl", "This chapter: done", "This chapter: needed" |

### 0.1 Chapters

| # | Title | Advances when (readiness, §3.2.3) | Calendar flavour (optional) | Centre | Shared goal |
|---|---|---|---|---|---|
| 1 | Where Is the King? | 30% of active wolves (at least 3) carried a Summons, fought at Alder Mere, found the King or gave to a post; 10 game days | a new moon may fall on the chalk night | Ridgemere Sump, the Oakhaven road | Find the King, carry the Summons, break the Alder Mere camp |
| 2 | By the New Moon | 25% (at least 2) freed a debtor or finished a side story; 12 game days; then the Reckoning | the Reckoning waits for a new moon within 3 game days | Ridgemere money quarter, Larkfield, Frosthollow | Not one voice sold: free the debtors |
| 3 | Two Long, One Short | 30% (at least 3) doused a lamp, fought at Glasswater, took a list piece or gave to proj-1; 12 game days | a town festival's eve within 3 game days | the Greenholt ridges, Glasswater, the Watch | Douse the signal lamps, open the Glasswater road, decide the Watch lists |
| 4 | The Nine Howls | 30% (at least 3) sang a chorus, heard a part of a howl, found a Mirebridge wolf, hauled silver or sat with Bramble; 12 game days; then the Howl Relay | opens at the season's festival or "the Moot Feast"; a full moon within 3 game days for the relay | every square, the Sunfields stone, Saltwell's pool, Highmere | Find each town's gathering howl; run the first relay |
| 5 | Letters for an Absent Lord | 30% (at least 3) walked a bound, pulled a stake, carried a letter, broke bounders or finished a side story; 18 game days | none | Grayrock Hall, the Rainward Hills, the Oakhaven cut | Walk the old bounds; deliver the forty letters; walk the Envoy down |
| 6 | The Lights in the Fen | 30% (at least 3) gave a name, took a false light, walked the path, fought a southern band, carried truce tokens or gave to the post; 18 game days | fog forced in any season; the Houses fall out | Sallow Fens, the Mirebridge crossroads, the city | Say the Names, lay the false lights, raise the Mirebridge post, keep the peace |
| 7 | Harvest Under Arms | 25% (at least 2) mustered for the ford; 6 game days; then three siege nights | named the Last Sheaf only if it falls in the siege | the Larkfield ford, Longmead, the Downs Road | Hold the ford for three nights |
| 8 | The Bell and the Writ | 30% (at least 3) heard a testimony, searched, took a bearing, worked the haul, taught Tansy, or the List or the Count; 24 game days | the real freeze in winter, else an early freeze | the Upper Mere ice; the City Office and the crypt | Raise the drowned bell; find the Lantern Writ |
| 9 | The Moot | 30% (at least 2) kept a post, carried lanterns, escorted the bell, raised a silent voice or answered the Envoy; 12 game days; the Moot night at dusk the next game day | "the Midwinter Moot" in winter, else "the Moot of the Nine Howls" | all 13 posts of the Howl Line; Market Square | Run the Holding Howl, then sit the Moot |

### 0.2 What was taken from where

| Source | Taken |
|---|---|
| weave | the frame: nine voices and Keepers; pledging to a town's howl; Spark / Ember / Blaze; surplus; distance weight; branches tallied from personal choices; the Mirebridge diaspora and the Book of Names; the bell under the Upper Mere ice; the Stones Remember; Bramble's three Restdays; the House quarrel table; the Moot Roll; the Bought Crown sequel hook; "innkeepers give only helpful knowledge" |
| crown | "Where Is the King?" as the first quest; the Lantern Writ (the Sump's peace-keeper; "the free wolves of the roads"); the weeping dated to the month before the King died; the free wolves' voice capped at 4; Ingrid Kendrick cleared, with Corra Coalcot as the true thief; the four destinations of the Watch list; Rosalind's doubled debts through the customs books; Gudrun's letters as a match with Osric; Maren's thyme; Hilde's whisper; Silver Under the Ice; the Queen's Purse and the Queen's Choice; the testimony race and the three-site search; the per-chapter 8-dialect gossip; `say[self.id=…]` window lines instead of `mantle.assign` |
| wild | Eskar Rimedrift of Deepvein as the Grey Captain, with the lamp-oil list, Fenwick's barred door and Ulric's brother Hakon; the camps that must fall before posts rise; douse-the-lamp ridge nights; the helper-grown goal; a raised town answers by itself; the Howl Line; the playable Holding Howl with the skip-two-steps rule; the Night Runner, the Granary Door and spawns scaled by wolves present; Tansy Reedmere's hidden water Quickening; Gorm as the Wardens' Eye; lay or turn the false lights; scars instead of failures; helpful knowledge written as scent tips |

### 0.3 Every judge's flaw and its fix

| Flaw (judge) | Fix in this design |
|---|---|
| The finale is a scene to watch (J1) | The Moot night is **played first**: the Holding Howl runs post to post, and there is a real fight at the Pass post. The Moot counts what the night carried in (§9.2–9.4). |
| The villain fades after Ch 3 (J1) | The Grey Captain has a beat in **every** chapter: the camps in Ch 1, wagon days bought in Ch 2, lamps in Ch 3, false relay howls in Ch 4, bounders in Ch 5, the southern company in Ch 6, the siege in Ch 7, the Pass in Ch 8, the strike in Ch 9. Desmond Hollis is his inside wolf. |
| The Ch 1 oath locks the ending in week one (J1) | The oath is gone. **Dagfinn's Question** ("Should I stand?") is each wolf's latest answer, changeable every chapter and tallied at the end of Ch 8, so late joiners count (§2.8). |
| Ch 1 is courier work (J1) | Ch 1 opens with a mystery (find the King on his own round), a fight (the Alder Mere camp) and a rescue (Willa). The Summons is one strand of three. |
| Sprawl and too many meters (J1, J3) | 4 trackers per wolf. The town strands stay hidden; each wolf sees only their own town's howl as a mirror (§4.4). |
| Choruses fail below 5 wolves (J1, J3) | A lone wolf howling at dusk with the town's Keeper present counts as a chorus (`resident in region` works today). With J5 the Keeper howls too (§11). |
| Wisp's fallback is weak (J1); Wisp stays, and is badly named (J2) | Wisp is dropped. The marsh lights are the Captain's float lamps. The Quickened thread is Tansy Reedmere, a placed 14-year-old who already "saw something big move under the ice". |
| Selling the bribe list paid from `town:ridgemere` (J1) | Ambrose buys it from the Greatly Bank till `till:job:rm_eleanor_saltcot` once SQ2 lets a story name a till. Until then the treasury pays, with the reason worded as the Bank paying, under the purse counters (§2.7, §2.11). |
| The ballot is opaque (J1) | One voice per community, set by a stated rule. Every town's board announces its verdict, and the reason, before the Moot (§9.4). |
| Coin-donation bars (J1) | No shared bar is fed by coin. Debtors are freed by labour, goods or proof. A coin gift is one path to free a single debtor. |
| Late joiners (J1) | Each chapter has a **door quest** that needs no carried flags, a pledge offer on arrival, and recap lines at every innkeeper (§2.9). |
| No path for wolves on the bad side (J1) | The **Purse** backing: Counting House errands with their own coin, their own marks, their own title and their own named result in the Bought Crown (§2.7). |
| Giver crowding (J1, J3) | A giver budget: at most 2 campaign offers per resident per chapter, Brenna and Dagfinn included; innkeepers carry none; no campaign resident carries a pack quest (§10.3, `givers.tsv`). |
| Rain Hall locked 22 years ago contradicts the witnesses' ages (J2) | Oskar wept **this spring**, the month before the King died. The Council sealed the Hall "for repairs" the week Gunnar died, which is why it is unreachable. |
| Mirebridge drowned 20 years ago contradicts the Mirebridge-raised pups aged 6 to 15 (J2) | Mirebridge drowned **five winters ago**. The unplaced Mirebridge pups are "the still-missing" of the Book of Names. |
| Lantern Law timing against day 46 (J2) | The Palace stood empty five days while the Council argued. On the sixth dawn (day 46) the Sump round's patrolman lit the Palace lamp, because no one else had. |
| The new moon confused with the 28-day month (J2, J3) | Real lunar moons (§3.3), used only as optional timing: a set piece waits for one only when it falls within 3 game days of readiness (§3.2.4). J4 adds `calendar is moon`; until then the setup trigger reads the season day against §3.3's table. |
| "Long Night" is not Ridgemere's festival (J2) | The Moot is **"the Midwinter Moot" only if its night falls in winter**, otherwise "the Moot of the Nine Howls" (§3.2.4). The writ's old words are "until the realm howls a Moot". Festival names follow the hash table (§3.3). |
| Rewriting Dagfinn's `work_label`; an Atlas stone prop (J2) | Neither. Dagfinn still walks his round at dusk, and that is why the label reads as it does. The stone stays in the telling. |
| The potter's cause of death invented (J2) | Left unstated. |
| Warden titles blur with the Concord's Wardens (J2) | No title contains "Warden". |
| Population rule broken by arrivals and Hakon (J2) | No resident arrives and no stranger stays. Hakon goes back over the Pass. Eskar stays unplaced; J7 is optional, and only lets him fill a post already left vacant by a death. |
| The succession explanation contradicts the data (J2) | The writ hands the work to "the wolf who keeps the peace of the Sump". Dagfinn was on Sump patrol. |
| Wrong festival names (J2) | Larkfield's autumn festival is the Last Sheaf (the siege). Oakhaven alone has the Long Light and the Long Night. |
| "The Sound" (J2) | Cedar Sound. |
| Per-wolf `npc.offer` overwrites (J3 #1) | J1 (runner builds `players`). Until then, offers are untargeted with `once: true`. |
| Giver offers leak (J3 #2) | Every gated or hidden quest is a journal `quest.offer` from a trigger, with no `giver`. Its talk happens through objectives and `npc.offer`. |
| Project caps (J3 #3) | Only 6 real projects, at most one open per town at a time: proj-3, proj-2, proj-1, the Glasswater relay, the Mirebridge post and the ford mend. The other 7 posts are story regions with map pings. |
| The Quickening does not count story quests (J3 #4) | J3 (the user's answer (b), 2026-11-22: yes, capped): a quest field `closure: "side"` on a campaign side story writes a `story_closure` receipt; at most 4 a chapter (each chapter is its own program), never on a main line or door quest. Until then, each personal arc ends with a journal note inviting the wolf to record it as a social Story of two scenes. The claim is never made in game text. |
| `visitor.add like:` an unplaced resident has no looks (J3 #5) | The Captain is `like: cty_stone_sunton` (a placed black arctic wolf), with Eskar's own description. |
| Events that don't exist (J3 #6) | `escorted_caravan` becomes `caravan arrives` or `contract done` with C12b `starts`. `howl` becomes J5, with a fallback. Listening becomes regions plus `skill practised` with `item: listening`. `wolf is` is used only for downed, hurt, wanted or held. |
| `npc.protect` is permanent (J3 #7) | Ch 9's epilogue and the cleanup program `ninth-howl.cleanup` unprotect everyone (§8.6). |
| All chapters approved at once (J3 #8) | Three acts. Cross-act per-wolf state uses J2 realm-player variables, with world `wolves` lists as the fallback (§4.4). |
| `each` stops at 500 (J3 #9) | Tallies use world numbers updated at the moment of each choice, not `each`. Only tiers use `each`, and C3 `rank` replaces it before 500 (§2.4). |
| Budgets per program (J3 #10) | ≤ 14 quests per chapter program; board repeatables go to the quest packs. Budgets are in §10.5. |
| `gift` fires once per pair per day (J3 #11) | Gift objectives also match `gift again`; no coin jars. |
| Post changes race the vacancy rule (J3 #12) | C14 is used for one change only, the ruler at the end, as an atomic swap. Every other fate is in talk, figures, holds and world flags. |
| crown: conditional giver quests, the 46-quest envelope, the C14 dependence, `npc.protect` undo, the Moot `each`, C2 dependence (J3) | All fixed by the rows above. |
| weave: the longest feature list, false facts (J3) | The feature list is cut to J1–J8 plus ten of the C-items (§11). |
| wild: talking strangers and arrivals at the core, 13 projects (J3) | The Envoy speaks only through Gorm, by Concord custom. The Captain speaks through Desmond or by letter. There are no arrivals, and 6 projects. |

### 0.4 What the decisions and the critic changed

| Item | Change | Where |
|---|---|---|
| P0-1, P0-8 purses | One counter rule for every program: `coin_<purse>_day` against a day cap of about 10% of the balance, a floor of 60 (back on at 80), paid in kind past the cap. Campaign coin is at most 10 from a town store and 20 from the treasury per deed. No till is named until SQ2; the treasury pays until then. The counters are owned by the new program `nh-purses`. | §2.7, §2.11, §4.5, Ch 2, Ch 3, §9.7, §12 |
| P0-3 camps | A story's own wave is matched by `cell` plus a story switch `spawned_<region>`, never by `camp_w_ridgemere_<cell>`. `camp cleared` is a trigger that completes objectives, not an objective. Posts 2, 10 and 12 already stand open in the world. | §5.2, §5.3, §6 conventions, Ch 1, 3, 5, 6, 7, 8, 9 |
| P0-4, P1-14 givers | `givers.tsv` is the one registry. No campaign resident carries a pack quest; innkeepers carry no quest and no offer anywhere, and are never Keepers or fallbacks. | §5.1, §7, §10.3 |
| P0-6 stage seeds | The seed table is replaced by the stage pack `nh-pack.stage` (decisions §7.3). The late howl and the Silver weekly stay the campaign's own. | §7, Ch 4 |
| P0-7 calendar | Act I ships on the today-only version (kept). **Superseded by the user's answer (a):** no dated plan, no anchors from the publication day, no Act II deadline; the `nh_debts=grace` key is optional (§3.4.4). | §3.1, §3.4, §10.1, §11.3, §11.4 |
| P1-11 cast | cast-and-talk §2's corrections 1–8 applied (Gorm at the Sump Infirmary, Cressida cold and frank, Fell Miremore plain, the clerks at Ashdown House, Eleanor's flat, Corra Coalcot's own list piece, the matching notched ears). Look-alikes re-picked: the Envoy is `like: tn_sm_brisk_plumby`, Hakon `like: rm_palle_bilgemarsh`, the Captain stays `like: cty_stone_sunton`. Three strangers, not two. | §2.10, Ch 3, 4, 5, 6, 8, §8, §12 |
| P1-12 mechanics | The beaten Captain is a stranger held one game day at the Keep's cells, with a board post and talk; no body, no hold. `npc.speak` is gone: Desmond's Ch 2 words are in his own window. | Ch 2, §9.2, §9.8, §8.2, §8.5 |
| P1-13 caps | The Moot backing moves to Speaker Gerrit Mudhand. Dagfinn's Question runs Ch 2–8. Nobody holds more than 2 campaign offers a chapter. The late howl is a journal offer. | §2.6, §2.8, §5.1, §10.3, §10.4 |
| P1-13 fallbacks | Ridgemere's fallback Keeper is Corra Coldridge `rmx_corra_coldridge`; Fenshore's is Jory Corncroft `cty_jory_corncroft`. Both protected. | §5.1, §8.6 |
| P2-21 | The `6_3` camp credits Frosthollow. | §5.3 |
| P2-22 face banners | No `story.tell` carries `from` with a resident's words. Proclamations are signed board posts plus the speaker's own window line; plain narration stays. | §2.1, every chapter |
| P2-23 names | C10 `foe.outlaw` (not "`npc` mode"); C12a `story.notify` / C12b `starts`; C13a `variable changed`. | §3.1, §7, §8, §11 |
| P2-24 checks | The full pre-approval check list, with the giver check. | §12 |
| **User (a), 2026-11-22: participation pacing** | No fixed 40 days and no calendar deadline. **Ready = (a share of the active wolves, with a small floor, have done the chapter's named tasks AND its minimum time is served) OR an admin push** (always possible, logged). Engine: R1 `participation` (floor never above the active count), R2 `ready` blocks with `auto: true` and `slowAfterDays`, R3 Advance, C2 `activeDays: 18`. A soft maximum raises "ready to push" (R2's `slow` alert, as needs-you) instead of moving, unless the chapter's switch `ch<n>_auto` is on. New state: `ch<n>_did`, `ch<n>_did_n`, `ch<n>_auto`, `ch<n>_wait_bar`, `nh_pushed`, `nh_mere_frozen`, `nh_moot`, `was_grace`; a stand-in before Phase 1a (§3.2.5). Gone: the day 80/84/88.6/110/130/133/135/148/218 dates, the anchors from P, the hard latest ends, `nh_season_clock`, the late-launch rules. The grace key is optional. The moon and festivals are optional timing within 3 game days. The finale is "the Moot night", dusk the next game day after the call, named the Midwinter Moot only in winter, else the Moot of the Nine Howls. Act III is "The Moot Night". Ch 4 opens at a festival or "the Moot Feast"; Ch 8 forces an early freeze out of winter; the writ and the Envoy name no date. | §0, §1, §2.2, §2.5, §2.11, §3 (rewritten), §4.1, §4.5, §4.6 (new), §5.1, §6 (every chapter's header and a new **Ends on**), Ch 1–9 texts, §10.1, §10.2, §10.5, §11.1–11.5, §12 |

---

## 1. Premise: why now

| Seed in the data | What it is in the campaign |
|---|---|
| King Gunnar Harrowgate (`rm_gunnar_harrowgate`), Sump-born, died of old age on day 41. On day 46, "reigning at the Great Palace" went to **Dagfinn Thornham** (`rmx_dagfinn_thornham`), 31, Mirebridge-born and raised on the Fenshore boardwalks, a Watch patrolman on the Sump round. | **The lamp-book line.** In the Palace lamp-book, in Gunnar's own paw: *"Should I go up with no word given, let the lamp be kept by the wolf who keeps the peace of the Sump, until the realm howls a Moot."* The Palace stood empty five days while the Council argued. On the sixth dawn, Dagfinn was the one who climbed the Hundred Steps and lit the Palace lamp, because no one else had. The work fell to him. He still walks his Sump round at dusk, "because the alleys don't stop needing it". |
| Queen Oda (`rm_oda_harrowgate`, Coldwater-raised, sly, counts coin) and Princess Ottilie (`rm_ottilie_harrowgate`, calm, warm, coughs in the damp, carries the petitions) were passed over. | Oda will not bow to a patrolman and bargains with Greatly. Ottilie is the blood heir, if the Moot wants one. |
| "Once saw the Chancellor weep in the Rain Hall and has told no one" (46 placed residents, aged 7 to 81). | **The Lantern Writ.** In the month before he died, Gunnar had a full writ read in the Rain Hall before Chancellor Oskar Gullworth (`rm_oskar_gullworth`). It made the lamp-book line law. It named the Moot's **nine voices** (eight towns and Mirebridge), the four Houses' Council votes, and "**the free wolves of the roads**". It denied the Council any right to name a ruler. Ambrose Greatly holds Oskar's debts and told him to lose it. Oskar wept, could not burn it, and hid it. The week the King died, the Council sealed the Rain Hall "for repairs". |
| Mirebridge was erased: a bare crossroads at `w_ridgemere_5_11` (188, 52), with 27 unreachable interiors. Fenshore keeps its speech. `tn_mb_*` residents with the odd "best eel trapper on the river" backstory live scattered from the Saltpan to the Larkfield looms. Unplaced Mirebridge pups aged 6 to 15 still have `home_region = mirebridge`. | **Five winters ago** the Long River came over the Mirebridge causeway in one night. The following spring the Council of Houses struck Mirebridge from the rolls, "a village that no longer is". By old custom a Moot is "full" only with nine voices, so the Houses believe no Moot can ever bind them. The scattered are the ninth voice. The unplaced pups are the Book of Names' "still-missing". |
| "Saw something big move under the ice" (7, all of the Greenholt: Kell Greyton, Willa Coldwick, Myrtle Stillford, Harrow Reedford, and the Reedmere twins Tansy and Gorse, 14). | As Mirebridge drowned, its folk put the gathering bell on a barge "to high water" and sent it upriver to Coldwater kin. The barge went through the first ice of the Upper Mere. **Last winter**, Tansy Reedmere (`tn_cw_tansy_reedmere`) fell through the ice by Kell's landing and *called the water*. The water lifted her out, and the same surge rolled the drowned bell along the bottom. That was the "something big". She has told no one that the water came when she called. She is Quickened. |
| "Owes a House factor money, to find by the new moon" (41: Captain Corra Slagmore, Herald Brenna Abbot, Steward Edric Kelpham, lamplighter Piet Lumley, and about 30 more adults). | **The Reckoning.** On the chalk night (Ch 1) the Counting House chalks the debtors' doors. Ambrose gives them until the Reckoning, "the next new moon, or sooner if the House calls it" (Ch 2): pay, or pledge your howl at the Moot to House Greatly. |
| "Keeps a secret list of which Houses pay the Watch to look away" (35: Treasurer Karin Stanmore, Abbot Lionel Ropestack, cell-keeper Desmond Dripcot, usher Desmond Farrow…). | **The Watch lists.** Put together, they show who paid for the roads to stay closed. |
| "Carries a letter meant for Lord Grayrock, who is never home" (40, including Dagfinn himself). | **The forty letters.** Petitions and griefs, plus a third kind nobody has opened: the Grayrock wagons' toll receipts, signed with a grey paw-mark. Grayrock pays the Captain so its freight passes. |
| Corwin Fell wants the Rainward Hills; Gudrun Fell writes secretly to Timberland; the Houses will fall out before winter (38). | **The Hills, the match and the quarrel** (Ch 5–6). Gudrun's letters propose a match with Osric Timberland. |
| Bandit camps keep forming at `6_8`, `5_4`, `8_6`, `7_12`, `6_12`, `1_3`, `2_3`, `3_3`, `6_3`. | This year they don't starve out. They are one company under the Grey Captain, fed with Greatly coin, Grayrock tolls and wagon days bought from desperate debtors. They sit exactly where the Howl Line must run. |
| `cty_eskar_rimedrift`, unplaced, "digs ore at Deepvein", "saving coin toward something no one else knows about"; lore `fh-snow-1`, Frosthollow's record of the deep winters, "and under it a list of who shared their lamp oil". | **The Grey Captain.** In the deep winter that closed Deepvein, Frosthollow's mine house would not open to its miners, and no Deepvein name is on the lamp-oil list. Ulric Stonevein's brother Hakon was one of the wolves turned away. Eskar has saved for years to buy Deepvein back. Ambrose has promised him its deed after the Moot if the roads stay shut, but Rosalind's ledger shows the Deepvein note pledged to three buyers already. |
| "The marsh lights are the drowned" (7); lore `mb-fens-1`: "a path at the bottom with no note at all". | The Captain's southern company moves through the fog by **float lamps**. The fen-folk think the drowned are walking. The path with no note is the one safe line through. |
| Lore `any-howl-1`: three howls in every town, "and no two towns agree which is which… a number of very short funerals and one very long party". | **The Holding Howl.** The realm must learn to agree on one howl. |
| No `warden_order` faction; Quickened magic raises `wardenAttention`. | Gorm Plankgate (`rm_gorm_plankgate`, Saltwell-raised infirmarian) is **the Wardens' Eye**, the one who wrote to Upper Accord. **Warden-Envoy Ingeborg Rimewater** comes down the Pass. By Concord custom she speaks to a realm only through its own Eye. |
| Lord Aldric Grayrock (`rm_aldric_grayrock_`) is always away; Lady Maren (`rm_maren_grayrock_`, Larkfield-raised) never leaves the Hall; Ingrid (16) writes to her father every week. | The letters bring him home (Ch 5). |

**Why it ends at a Moot.** The lamp-book line holds only "until the realm howls a Moot". The Council has given notice that if the Moot is slow to sit, it will name the ruler itself: a threat with no date, pressure in talk and never a deadline. From Ch 6 on, the Envoy adds the Concord's Question: on the night the Moot sits, let one howl run from the Pass to the Waste with every town answering, or the Concord will keep this realm. The night is named when the realm is ready (§3.2.4): "the Midwinter Moot" if it falls in winter, otherwise "the Moot of the Nine Howls".

---

## 2. Design rules

### 2.1 Three rungs: every deed is answered (weave)
1. **Spark, at once, personal.** The wolf's tracker "Your voice" ticks. The giver thanks them (`offer.thanks`; with C5, `npc.greet` by name). A first deed of a kind in a chapter writes a `deed.award` as a lower-case verb phrase, so residents greet the wolf with "You … didn't you?" with no new code.
2. **Ember, within a game day, the town.** Each town's strand has three steps: *stirring* at 1/3, *rising* at 2/3 and *raised* at the full raise mark. Each step fires a board post on `board:<town>` signed by the Keeper in its text (for example "(signed) The Keeper of the Coldwater howl"), a plain `story.tell` narration to that town's region with no `from` and no quoted speech ("Coldwater's howl is stirring."), and the town talk key `nh_voice=stirring|rising|raised` (C8). The Keeper's own words go in her window: `say[self.id=<keeper> world.mem.nh_voice=stirring|rising|raised priority=3]`.
   - **A banner is not the dialogue window** (decisions §9, P2-22). No `story.tell` in this campaign carries `from` with a resident's words. A resident speaks to a wolf only in: an offer's `say` / `thanks`; `npc.offer` and chain replies; `npc.line`; C5 `npc.greet`; C6 `offer.during`; scenes (each speaker in their own panel); or their own tree lines (`say[self.id=…]`). Before C5, a resident's one-off thanks is a gated tree line or a `deed.award`.
3. **Blaze, the chapter, the realm.** When the chapter goal is met: a `board.post` on all eight boards in the herald's words, signed "the Herald of Ridgemere", with Brenna's own window line saying the same (`say[self.id=rmx_brenna_abbot world.mem.nh_stage=<key>]`) and a plain world narration; a lasting change (a post stands, a figure is set, a relation moves, a resident's fate), and every town's talk moves to the next stage key.

### 2.2 Scaling: helper-grown goals, floors, surplus (wild + weave)
- **Chapter goal** `ch_goal` = base + k × helpers. Every wolf's first contribution in a chapter adds them to the story `wolves` list `ch_helpers` (`in list` false → `list add`, `add ch_goal k`). The goal tracks the wolves who actually play, not idle sign-ins, and it works **today**. With C2 the floor becomes `max(base, k × active)`.
- **Deed points.** An ordinary deed is worth 2, multiplied by distance (§2.3). A camp cleared is 10, a set-piece objective 10, a finished side story 6. Quest packs give 2 (small), 4 (medium) or 8 (large).
- **Surplus is never wasted.** Points past the goal go to `nh_surplus`. Each chapter lists what surplus steps buy: another post, a festival, a better ending clause, a weather night.
- **Fixed races for unique pieces.** The 40 letters, the 46 witnesses and the Book of Names' first 30 names are races, each piece credited by name. The scaled strands of the same chapter soak up the rest of a crowd.
- **Readiness-paced** (user, 2026-11-22). A chapter moves on when enough of the active wolves have done its named tasks and it has served its minimum time, or when an admin pushes it (§3.2); never on a date. A crowd of 200 meets the share within hours and spends the minimum time on the bar, the surplus and the side stories. 5 wolves need 3 of them (or 2) to have taken part. **The bar decides the road, not the timing**: a chapter closed with its bar short takes its worse road (§2.5).

### 2.3 Taking a howl: the pledge (weave)
- At any Keeper (§5.1), a wolf may pledge to that community's howl (`npc.offer` "I'll howl for this town", one slot per Keeper). This sets the player text `p_howl` and adds the wolf to the world list `nh_pledge_<town>`. A wolf can change their pledge once per chapter.
- A pledged wolf's deeds anywhere add a quarter-share to their own town's strand. A lone wolf *is* their town's champion.
- **Distance weight** pulls crowds off Market Square. Deeds done in or for Saltwell, Southmeadow, Fenshore or Mirebridge count ×2. Larkfield, Coldwater and Frosthollow count ×1.5. Ridgemere and Oakhaven count ×1. In points: 4 / 3 / 2.
- Pledging to **Mirebridge** is hard mode: no square, no board, no purse, and the richest personal story (the Scattered, the Book of Names, the crossroads).

### 2.4 Many hands, one weave: branches from personal choices (weave)
Each big branch is the sum of many personal choices, weighted by the chooser's `p_voice` at the moment they choose.
- Each option has a world number, for example `nh_t_ledger_shown`, `nh_t_ledger_kept` and `nh_t_ledger_ash`. A wolf's choice adds `{var:p_voice, of:$actor}` to its option once (`once: member`).
- At the chapter's resolution, an `if`/`elif` ladder compares the totals and writes the branch text variable.
- **No `each` is needed, so the 500 cap does not bite.**
- Wolves on the losing side still get their own personal ending: Rosalind trusts them, Gudrun owes them, and so on.
- The option counters are story-scope numbers, local to the chapter. Only the result is world.

### 2.5 Failure never blocks; scars (wild)
- Every chapter has a goal *and* a close: readiness, an admin's Advance, or `ch<n>_auto` at the soft maximum (§3.2). A goal still short at the close moves on to the chapter's worse road and adds a **scar**: an entry in the world list `nh_scars` that later chapters read. Scars mean harder fights, a higher raise mark, a darker line at the Moot.
- Every loss can be won back later at a price, for example "Buy Back the Howls" in Ch 4.
- Personal quests fail per wolf (`fail` rules, `finish who:$actor`) and come back weekly.
- Every branch has a default, so a choice nobody makes still resolves at the close.
- The Moot always sits.

### 2.6 Four backings: whose side are you on (crown)
Every wolf may back a candidate, setting `p_backs` and the world numbers `nh_back_<x>`:
- **Lamp**: Dagfinn, if he stands;
- **Blood**: Ottilie;
- **Moot**: a Regency of the Moot, with no ruler and a Moot every Midwinter;
- **Purse**: House Greatly's choice, Rosalind or, if she was turned out, Julian.

A wolf backs at any of the four patrons: Dagfinn, Ottilie, Speaker Gerrit Mudhand `rm_gerrit_mudhand` for the Moot (as keeper of the Moot's roll, the Moot is his to back; from Ch 2), or Steward Edric Kelpham for Greatly. The backing can change once per chapter at a cost of 2 `p_voice`. It is an identity and a friendly rivalry, never PvP. It is also the ballot at the Moot (§9.4).

### 2.7 The Purse path: a bad side with its own rewards (J1's cross-cutting ask)
A wolf backing the Purse gets Counting House errands from clerks Rune Gullwell (`rm_rune_gullwell`) and Ivar Pitchwick (`rm_ivar_pitchwick`): call in a Sump note (Ch 2), sell a list page (Ch 3), sign a debtor's pledge (Ch 4), carry Fell wine to the Council stewards (Ch 5), buy Longmead grain at siege price (Ch 7), bring the found writ to Ambrose (Ch 8).
- Each errand pays a little coin, never more than 10 a time, under the purse counters (§2.11). **Greatly Bank till after SQ2; the treasury until then.**
  - Today a story cannot name a till: `check_purse` refuses `till:job:<id>`, and `till:<id>` names no account. So until SQ2 ships (engine Phase 1), every Purse-path payment names `town:ridgemere` (the treasury, 1,355 pennies at the checkpoint), counted on `coin_ridgemere_day`, with the reason worded as the Bank paying ("the Counting House pays its own").
  - When SQ2 lands, a new version of the chapter swaps the purse to `till:job:rm_eleanor_saltcot` (161 pennies; day cap 16), counted on `coin_till_eleanor_saltcot_day`.
- It adds 1 to the hidden `p_marks` and 1 to `nh_bought`.
- It awards a deed whose gossip makes the wolf notorious, for example "called in a Sump debt for the Counting House".
- **Named result:** in the Bought Crown, Purse wolves with `p_marks ≥ 3` are "the Protector's Own" (title C4), are thanked by Ambrose by name, and their names are on the Protector's roll. In every other ending, the Sump remembers them coldly in talk.

### 2.8 Dagfinn's Question (replaces weave's oath)
In **Ch 2–8**, Dagfinn has one `npc.offer` a chapter: "Should you stand at the Moot?", answered "Stand" or "Keep the lamp and let others stand". (Ch 1 is his round chain and the Lamp backing; Ch 9 is the backing only, so he never holds more than 2.) The answer sets the player text `p_advice`, which the wolf can change every chapter it runs. A world number pair `nh_t_stand` / `nh_t_step` is updated by the difference of `p_voice` on each change.
- At the end of Ch 8, after the writ (which Dagfinn reads as meant "for someone like me"), the larger total decides `nh_b_stands`. Writ found adds 20% to "Stand".
- If he does not stand, Lamp backers' voices go to the Moot ("then let the Moot keep the lamp").

### 2.9 Late joiners
- **A door quest per chapter**: one journal quest offered on `arrival` to any wolf with no `quest is` record of it, needing no earlier flags, worth a full side-story share. All nine are listed in each chapter section.
- **A pledge offer** on arrival if `p_howl` is empty: a journal note "Every Keeper takes a pledge; Ridgemere's is the herald at the Palace door."
- **Recap lines**: each innkeeper's `inn.greet` and every resident's `resident.news` carry a "story so far" line gated on `world.mem.nh_stage` (§10.2), so a wolf who arrives in Ch 6 can learn the first five chapters from any taproom.
- Every chapter's titles and tiers count only that chapter's merit, so a Ch 6 arrival can be the Ch 6 Lantern.

### 2.10 Rules of the world, kept throughout
- Wolves and pups; never the forbidden word for them.
- Only the Bright One and the Dark One.
- "Private messages".
- Male or female only.
- Every word pre-written. NPCs speak to wolves only in the dialogue window and in scenes. A `story.tell` banner with a face is **not** the dialogue window (§2.1).
- Dialect by upbringing.
- No new residents: **three strangers** (the Warden-Envoy, the Grey Captain and Hakon) and the foe bands come and go. A stranger's `like:` resident is never cast, never gives a pack quest, and lives where that stranger never walks, with one stated exception: the beaten Captain's one day in the Keep's cells (§8.5).
- Reachable interiors only; real resident, place and faction ids.
- No ids in spoken words, and no personal names in library lines.

### 2.11 Purses: one counter rule, and the program `nh-purses`
Every program that pays coin (the chapters, every pack, the stage pack) follows one rule (decisions §3). Balances at the 2026-11-22 22:34 UTC checkpoint, `society.accounts`:

| Purse | Account | Pennies | Day cap |
|---|---|---|---|
| `town:ridgemere` | `treasury` | 1,355 | 135 |
| `town:coldwater` | `stores:coldwater` | 212 | 21 |
| `town:larkfield` | `stores:larkfield` | 248 | 24 |
| `town:southmeadow` | `stores:southmeadow` | 236 | 23 |
| `town:fenshore` | `stores:fenshore` | 213 | 21 |
| `town:saltwell` | `stores:saltwell` | 224 | 22 |
| `town:oakhaven` | `stores:oakhaven` | 247 | 24 |
| `town:frosthollow` | `stores:frosthollow` | 247 | 24 |
| `till:job:rm_eleanor_saltcot` (Greatly Bank; **after SQ2 only**) | same | 161 | 16 |

Mirebridge has no purse: its coin comes from the treasury. `town:<id>:works|watch|church` accounts do not exist, and no program names them.

1. **The grant.** A program pays only if `coin_<purse>_day + amount ≤ cap` **and** the switch `coin_<purse>_ok` is on. Then `character.grant` with `ifRefused: "skip"`, and `add coin_<purse>_day amount`.
2. **Otherwise in kind.** The wolf gets `bread` ×2, or the quest's own item. The giver's thanks has a "thin purse" variant ("The purse is thin this week, so take this instead."), and the deed, the strand points and the voice land as usual.
3. **A floor of 60.** A purse below 60 pays no story coin until it is back at 80 or more.
4. **The campaign's amounts:** at most 10 from a town store and at most 20 from the treasury per deed; Purse-path errands at most 10.

**The program `nh-purses`** (file `Data/Storylines/ridgemere3/ninth-howl/nh-purses.json`; open audience; one phase; always on; published before Act I and left running across all acts). It owns the counters; every other program only reads them and adds to them.
- **Declares** (with `world-var declare`, the same name and type everywhere): the numbers `coin_ridgemere_day`, `coin_coldwater_day`, `coin_larkfield_day`, `coin_southmeadow_day`, `coin_fenshore_day`, `coin_saltwell_day`, `coin_oakhaven_day`, `coin_frosthollow_day`, `coin_till_eleanor_saltcot_day`, and a switch `coin_<purse>_ok` for each (initial on).
- **Trigger `day begins`:** set every `coin_*_day` to 0.
- **Trigger `purse crossed`** (`target` the purse, `item: "down"`, `detail: "60"`): `coin_<purse>_ok` off. One per purse.
- **Trigger `purse crossed`** (`item: "up"`, `detail: "80"`): `coin_<purse>_ok` on. If `purse crossed` has no "up" direction, a daily `schedule` at the day's start turns the switch back on for any purse whose balance (M2 operand once it exists; until then, the last grant of the previous day having gone through) is at 80 or more.
- **The thin-purse key.** On the down-crossing of a town's purse it also writes the town talk key `nh_coin=thin` (C8 `talk.remember {scope:"town", town:<town>, key:"nh_coin", value:"thin", days:28}`), and on the up-crossing it forgets it (`talk.forget`). The packs' thin-purse window lines are gated on it (quests-small §1.3). Until C8 (Phase 1) the DM seeds and clears the key by hand.
- No quests, no offers and no talk lines of its own: `nh_coin` is the only key it writes. It counts toward no program's budget but its own.

---

## 3. Structure, readiness and pacing

**Revised to the user's answer (a), 2026-11-22** (binding): the campaign is not held to 40 days or to any calendar. A chapter moves on through **participation** (a share of the active wolves doing that chapter's tasks) **plus time** (a minimum stay), and an admin can always **push** it forward from the Campaigns console. The moon, the festivals and Midwinter are flavour and optional timing for set pieces, never deadlines. Decisions §6 is the cross-file rule; the engine is engine-spec's **R1** (participation), **R2** (a phase's `ready` block and the command `advance`) and **R3** (the console's bars and Advance), with **C2** (`audience`, `activeDays`), all in engine Phase 1a.

### 3.1 Acts and how they join
- **Act I "The Lamp"** (Ch 1–3) is **built to the today-only version (§11.4) with no engine change**, and ships on it whenever the DM publishes it: nothing in the design is tied to a game day, so there is no target date (§3.4). If engine Phase 1a (readiness) lands first, Act I ships with its `ready` blocks; otherwise it ships with the stand-in of §3.2.5 and moves onto the `ready` version by `migrate` when Phase 1a lands. When Phase 1b lands (J1, C8, C9, C11, SQ2 and C8's pair scope), the DM moves the running chapter to a new version at the next chapter boundary (doc 85, "moving a run to a new version").
- **Act II "The Howls"** (Ch 4–6) uses J3, J4, J5, J10, C12a `story.notify`, C12b `starts` and C13a `variable changed` (engine Phases 1–6). If they are not landed and tested when Act I's last chapter closes, Act I's interlude simply waits, and the DM either waits longer for the engine or publishes Act II's today-only rows (§11.4).
- **Act III "The Moot Night"** (Ch 7–9) adds J5's resident howl, C10 `foe.add` / `foe.outlaw` (a resident downed, never killed) and C14 for the ruler (Phases 7–8), with today-only rows 7–9 as its fallback, on the same rule.
- Each act is one `campaign.approve` whose chapters chain with `story.start {audience:"same", carry:true, when:"ended"}`. Each chapter declares `activeDays: 18` (C2: 3 real days), and C2 seeds its `seen` map at launch, so a new chapter never opens with 0 active wolves.
- **Between acts**, the last chapter of an act enters a waiting phase ("interlude") that keeps the act's talk and boards alive. It ends on `story started` with `target` = the next act's first program. The DM publishes the next act's ticket from the Campaigns console. The interlude has no cap and no `ready` block: it waits for the DM, who may hold it.
- **Per-wolf state crosses acts** through J2 (realm-player variables). Until J2 exists, it crosses through world `wolves` lists (§4.4). Each act's first chapter rebuilds `p_howl`, `p_backs` and the tiers from those lists on each wolf's `arrival`.

### 3.2 Readiness: how a chapter moves on

No chapter has a date. Each chapter, and each big beat inside one (§3.2.4), moves on when it is **ready**:

> **Ready = (participation met AND the minimum time served) OR an admin push.**

#### 3.2.1 The three ways
1. **Participation** (R1). A stated share of the **active** wolves have done at least one of the chapter's **named tasks** (the table in §3.2.3).
   - **Active** means a wolf of the chapter's audience seen (signed in, or the actor of any event) within the last `activeDays` game days: **18, which is 3 real days** (C2). The audience itself stays `{"kind":"everyone","days":30}`.
   - **Who did it.** Each chapter `n` keeps the story list `ch<n>_did` (wolves) and its count `ch<n>_did_n`. A wolf joins the first time they do one of the chapter's tasks (`in list` false → `list add`, `add ch<n>_did_n 1`), once per chapter. Every other deed still feeds the bar, the strands and `ch_helpers` (§2.2), but not participation.
   - **The test** is R1's `participation` on `ch<n>_did`, `of: "active"`, with the chapter's `percent`, `floor` and `cap: 150` (a crowd of 3,000 never needs more than 150). A wolf who did a task and then went quiet leaves both counts, so the share stays honest.
   - **The floor** (2 or 3 a chapter) means a handful must have played even where the share rounds low. **It never asks for more wolves than are active** (decisions §6.2, a ruling on R1 rule 4): need = max(min(floor, active), ⌈percent × active / 100⌉), capped, never below 1. So **a world of 1 to 5 wolves can always advance**: with 2 active wolves and a floor of 3, both must have taken part; with 1, that one.
2. **The minimum time** in the chapter: R2's `minDays` with `since: "chapter"`, in game days. Even a crowd that does every task in an hour gets the chapter's story, its side stories and its surplus. Held time does not count (R2 rule 8).
3. **An admin push**, always possible, at any moment, even before the minimum time: the console's **Advance…** (R2's command `advance`, R3's button). When the chapter is not ready it is **Advance anyway…**, which asks for the story's typed title. Both need a reason. The command row and `dm.audit` keep who, when, why and the readiness snapshot of what the admin saw (R2 rule 9; doc 85 §5.1, rule 6). The closing phase also adds the chapter's key to the world list `nh_pushed` when its cause was `advance`, so the epilogue knows which chapters the admin moved.

**The bar is not a gate.** The shared goal (`ch_points` against `ch_goal`, §2.2) decides the good or the worse road at the close (§2.5), not when the close comes. A chapter that closes with its bar short takes its worse road and its scar. For a small world that wants the time to fill the bar, the admin turns on the chapter's switch **`ch<n>_wait_bar`**: readiness then also waits for the bar.

**The soft maximum.** A chapter that is not ready after its soft maximum (game days, §3.2.3) does **not** move by itself. R2's `slowAfterDays` raises the console alert this design calls **"ready to push"**, naming the part that is furthest behind, for example *"Ch 3 after 36 game days: 9 of 41 active wolves did its tasks (13 needed). Advance anyway, or wait."* It is a "needs you" alert for every chapter of this campaign (decisions §5.2, R2 +: `slowNeedsYou: true`). The chapter keeps counting, so readiness met later still moves it on.

**`auto` at the soft maximum.** Each chapter has the story switch **`ch<n>_auto`** (initial off), which the admin can turn on from the Variables tab at any time. With it on, the soft maximum itself counts as participation met, so the chapter moves on by itself there (still after its minimum time). Its timeline then reads "ready: the soft maximum, `auto`".

**The `ready` block** of a chapter's working phase (R2), shown for Ch 3 with the literals of §3.2.3 (condition shapes as `Docs/story-programs.md` version 2):
```json
"ready": {
  "all": [
    {"any": [
       {"participation": {"list": "ch3_did"}, "of": "active", "percent": 30, "floor": 3, "cap": 150,
        "label": "Wolves who did the chapter's tasks"},
       {"test": {"all": [{"test": "switch", "var": "ch3_auto"},
                         {"test": "days since", "of": "run", "op": ">=", "value": 36}]},
        "label": "or the soft maximum, with auto on"}],
     "label": "Participation"},
    {"minDays": 12, "since": "chapter", "label": "Days in the chapter"},
    {"test": {"any": [{"test": "switch", "var": "ch3_wait_bar", "value": false},
                      {"test": "compare", "left": {"var": "ch_points"}, "op": ">=", "right": {"var": "ch_goal"}}]},
     "label": "The bar, when the admin asked to wait for it"}],
  "auto": true,
  "slowAfterDays": 36,
  "slowNeedsYou": true
},
"exits": [{"on": "ready", "to": "ch3_closing"}]
```
- `auto: true`: when ready, the chapter moves on by itself in the same tick (R2 rule 5). This campaign does not use `then`: each closing phase times its own set piece (below), so the boards can announce it first.
- A chapter whose close must not cut a running set piece adds one more part, a `test` on its own switch (Ch 8's `haul_running` off).
- Readiness is latched once met (R2 rule 4): a crowd arriving later cannot take it back.
- To change a literal mid-run, the DM moves the run onto a new version (R2 rule 12 keeps `metAt` only when the block is unchanged). `ch<n>_auto` and `ch<n>_wait_bar` are the live switches.

**The close** (the closing phase's entry, whether by readiness, Advance or `ch<n>_auto`):
- a signed board post on all eight boards says what comes and when ("At dusk the herald will read…");
- a named game-clock timer starts for the set piece: the **next dusk at least 3 game hours away** (30 real minutes, so wolves can gather), unless the chapter's row in §3.2.3 says otherwise; **the finale is always dusk the next game day** (§9.2);
- at the timer the set piece plays, every branch resolves (with its default if nobody chose, §2.5), and `finish` chains to the next chapter;
- every task and scoring trigger is a campaign trigger, so deeds done between readiness and the set piece still count for the bar.

#### 3.2.2 Per-chapter phases
- **The working phase** (`ch<n>_main`) carries the `ready` block and its `ready` exit. Chapters with a big beat inside call it from the working phase and close later (R2 "Per chapter": `since: "chapter"` for the time): Ch 4 (`ch4_main` → `ch4_relay` → `ch4_closing` at the dawn after the Howling, by timer); Ch 7 (`ch7_muster` → `ch7_siege` → `ch7_aftermath`, whose `ready` block is only `minDays` 6 `since: "phase"`, → `ch7_closing`); Ch 9 (`ch9_run_up` → `ch9_called` → `ch9_night`).
- **The closing phase** (`ch<n>_closing`) announces, times and plays the set piece, then `finish`. It has no `ready` block, so the console offers Go to phase… there, not Advance.
- **The interlude** ends an act (§3.1).

#### 3.2.3 The pacing table

A game day is 4 real hours, so 6 game days make 1 real day. "Typical" assumes, as before, about 3 deeds per active wolf per real day, and that each of 5 wolves plays most days. At **200 wolves** the share is met within hours, so the minimum time (and the set piece's dusk) decides. At **5 wolves** the floor (3 of the 5, or 2) decides, a little later. These are typical runs, **not limits**.

| Ch | Participation tasks (a wolf counts once per chapter) | % of active | floor | min (game days) | big beat and its own rule | soft max (game days) | typical real days at 5 wolves | at 200 wolves |
|---|---|---|---|---|---|---|---|---|
| 1 | carried a Summons to a Keeper; fought at the Alder Mere camp (a `bandit falls` in cell `5_4`, or in the region when it is cleared); found the King ("Where Is the King?" done); gave to proj-2 or proj-3 | 30% | 3 | 10 | the chalk night: chapter game day 3, or a new moon on game day 2–5 (§3.2.4); the close is the Herald's Word | 30 | ~2 | ~1.8 |
| 2 | freed a debtor (labour, goods, coin or proof); finished a Ch 2 side story or the door quest | 25% | 2 | 12 | the close is the Reckoning: at a new moon if one falls within 3 game days of readiness, else at dusk the next game day | 36 | ~2.5 | ~2.2 |
| 3 | doused a lamp (on a ridge at a lamp hour); fought in a Glasswater wave; took a Watch-list piece from a keeper; gave to proj-1 | 30% | 3 | 12 | the close is the Road Feast eve: the eve of a town festival if one falls within 3 game days, else the next dusk | 36 | ~3 | ~2.2 |
| 4 | sang in a gathering-howl chorus; heard a part of a howl from an old wolf; found one of the Mirebridge-blooded; hauled the Highmere silver; sat a Restday with Bramble | 30% | 3 | 12 | **the Howl Relay** is called on readiness: the coming night, or a full moon within 3 game days; the chapter closes at the dawn after "the Howling" | 42 | ~4 | ~2.5 |
| 5 | walked a bound post; pulled a stake; took or delivered a letter; broke a bounder camp; finished a Ch 5 side story | 30% | 3 | 18 | the close is **Black Cloaks on the Pass Road**: the Envoy comes down at the next dawn | 48 | ~3.5 | ~3 |
| 6 | gave a name to the Book; picked up a false light; walked the path with no note; fought a southern camp or a cudgel band; carried truce tokens; gave to the Mirebridge post | 30% | 3 | 18 | **the Rite of the Tide** at the next dusk once the Book reaches its floor of 30 names (a content gate, as before); the close at the next dusk after readiness | 48 | ~4 | ~3 |
| 7 | **mustered for the ford**: in a siege region after dark; gave to the ford mend; brought grain to Rook; tended in Larkfield | 25% | 2 | 6 | **the siege** is called on readiness: three nights from dusk the next game day ("the Last Sheaf" only if Larkfield's festival falls in them); the chapter closes 6 game days after the third night (the hearing, the relief) | 30 (for the call) | ~2.7 | ~2.7 |
| 8 | heard a testimony; searched a writ site; took a bearing on the ice; gave the frame makings or held a rope; gave Tansy a lesson; the Lamp-Oil List; the Frosthollow Count | 30% | 3 | 24 | the testimonies and search as before (a race, from 23 memories); **the haul**: three dusks from the next dusk once six bearings are placed and the frame is given; the close waits for a haul under way | 60 | ~5 | ~4 |
| 9 | kept a post (a keeper quest done); carried lanterns for the drowned; escorted a stage of the bell's road; raised a silent voice (a late howl or a buy-back); answered the Envoy | 30% | 2 | 12 | **the Moot night**: called on readiness or by Advance; **set at dusk the next game day**, announced on every board at once | 36 | ~3 | ~2.7 |
| | **whole campaign** | | | | | | **~30** | **~24** |

- **Before any alert:** the soft maxima add up to 366 game days (61 real days). Past one, nothing moves unless the admin advances or turns on `ch<n>_auto`. With Advance the campaign can be as short as the admin likes.
- **The interludes** between acts add whatever the DM takes to publish the next act (§3.1).
- **1 or 2 wolves.** The floor becomes "every active wolf", so a lone wolf advances a chapter by doing one of its tasks and serving the minimum time. Their bar will often be short (their scars are the price); the admin may turn on `ch<n>_wait_bar` for them.
- **A crowd** reaches the share within hours and spends the rest of the minimum time on the bar, the surplus (§2.2) and the side stories.
- These replace engine-spec §8 D10's suggested defaults (35%, floor 5, `auto: false`) for this campaign; the `cap` of 150 and `activeDays` 18 are D10's.

#### 3.2.4 Set pieces and their calendar flavour
Calendar moments are **flavour and optional timing**. A set piece waits for one only when it falls within 3 game days (half a real day) of the closing phase's entry; otherwise it plays at its dusk. The closing phase decides this at its entry from J4's `moon` (Phase 2) or the `festival` event, or, without J4, from the season day (`day begins` `quantity`) against the lunar table in §3.3. The DM can `shift` any set-piece timer from the console.
- **The chalk night (Ch 1).** Chapter game day 3, or a new moon if one falls on chapter game days 2 to 5. It is a timed beat inside Ch 1, not a close.
- **The Reckoning (Ch 2).** At a new moon within 3 game days, the board reads *"The new moon rose on the Counting House's day."* Otherwise the Counting House calls its notes early, at dusk the next game day: *"THE COUNTING HOUSE WILL NOT WAIT ON THE MOON."* The residents' hook ("by the new moon") stays true in talk either way: the factors promised a moon, and the House kept its word or broke it.
- **The Road Feast eve (Ch 3).** The eve of the season's town festival (§3.3) if it falls within 3 game days, else the next dusk, with `festival.call` of "the Road Feast" in the town with the most camps cleared nearby (the surplus feast, as before).
- **Ch 4's opening feast.** If the season's festival falls within 3 game days of Ch 4's opening, the Keepers' howls go up at it (Midsummer, the Sun Feast, the Long Light, …, by §3.3). Otherwise Ch 4's setup calls `festival.call` of **"the Moot Feast"** in every town for the next day, and the Keepers' board posts go up at that feast. Either way, "half the square sang the burying howl".
- **The Howl Relay (Ch 4).** The coming night (from the next dusk at least 3 game hours away), or a full moon within 3 game days ("the Long Howl" surplus buys clear weather for it either way).
- **Fog (Ch 6).** Ch 6 forces fog by `weather.set` whatever the season.
- **The siege (Ch 7).** Three nights from dusk the next game day after the call. If Larkfield's autumn festival (the Last Sheaf) falls within those nights or within 3 game days, the siege waits for it and the texts name it; otherwise the texts say "the siege nights". The chapter's harvest is Longmead's granary, which holds grain in every season.
- **The freeze (Ch 8).** If it is winter when Ch 8 opens, the mere freezes by itself. Otherwise Ch 8's setup forces **an early freeze**: `weather.set snow` on Coldwater and the six Upper Mere regions, 72 hours at a time and re-set while the bell thread runs, as Ch 6 forces fog. Either way the world switch `nh_mere_frozen` goes on (packs read it, decisions §6.4) and the Coldwater board reads *"THE MERE IS SKINNING OVER."*
- **The Moot night (Ch 9).** Always **dusk the next game day** after the call (readiness, Advance, or `ch9_auto` at the soft maximum). Its name follows the season on that night: **"the Midwinter Moot"** if the season is winter (`calendar is {what:"season", value:"winter"}`), otherwise **"the Moot of the Nine Howls"**. The call sets the world text `nh_moot` and the realm talk key of the same name (`midwinter` or `nine_howls`), and every board post, narration and deed of Ch 9 uses the matching text. The admin may `shift` the night's timer (for example onto Midwinter's own festival night); the design never waits for it. (This settles engine-spec §8 D12: no wait for the next Midwinter, and no "longest night" in the words.)

#### 3.2.5 Before engine Phase 1a: the stand-in
If Act I is published before Phase 1a lands (engine-spec §7.1), each working phase has, in place of its `ready` block, a `dm signal` direction exit "Move the chapter on" to its closing phase (doc 85, Phase 7). The chapter still keeps `ch<n>_did` and `ch<n>_did_n`, shown on the Variables tab. The DM looks at the Campaigns list once a real day and sends the direction when `ch<n>_did_n` reaches the chapter's share of the list's active count (at least the floor, or every active wolf if fewer) and the minimum days have passed; past the soft maximum, the DM decides. There is no latest-end `clock` exit. When Phase 1a lands, the run moves onto the `ready` version by `migrate` (doc 85, 5b), mid-chapter if need be.

### 3.3 Festival names and the moon (flavour)

| Town | Spring | Summer | Autumn | Winter |
|---|---|---|---|---|
| Ridgemere, Coldwater, Southmeadow, Saltwell | the Blossom Fair | Midsummer | Harvest Home | Midwinter |
| Larkfield, Fenshore, Frosthollow | Seedtide | the Sun Feast | the Last Sheaf | the Hearth Feast |
| Oakhaven | the Greening | the Long Light | the Gathering | the Long Night |

Festival day is day 46 of each season: about game day 137 (summer), 229 (autumn) and 320 (winter) this year.
- The Moot is "the Moot" in every text before Ch 9 calls its night; from then on it is "the Midwinter Moot" in winter and "the Moot of the Nine Howls" otherwise (§3.2.4).
- Town lines use their own festival's name, and only when the chapter has checked that the festival is the one at hand: Larkfield's siege is "the Last Sheaf" only when it falls on it, and Oakhaven's "the Long Night" only on a winter Moot.
- The writ's own words are "until the realm howls a Moot" (Ch 8); no text promises a date.

Lunar dates (29.530588 days, new moon on spring 1), for the closing phases without J4:
- new moons: 88.6, 118.1, 147.7, 177.2, 206.7, 236.2, 265.8, 295.3, 324.8, 354.4;
- full moons: 103.3, 132.9, 162.4, 191.9, 221.5, 251.0, 280.5, 310.1, 339.6.

### 3.4 The launch (decisions §6)

**1. The clock.** Game day 66.32 at the checkpoint, 2026-11-22 22:34 UTC. Six game days pass per real day (a game day is 4 real hours). The real time of game day *d* is 2026-11-22 22:34 UTC + (*d* − 66.32) / 6 real days. The clock is for estimates only: no rule reads a game day.

**2. Act I ships on the today-only version.** Act I (Ch 1–3) is built to §11.4 with no engine change, together with the packs' today-only versions and the stage pack's stage 1–3 quests. That means:
- untargeted offers with `once: true`;
- DM-seeded talk keys (the DM seeds `nh_stage` and the other realm keys by hand until C8);
- DM `save_figure` for the figures until C9;
- the authored `protected` flag on the old cast (a listed DM data edit, §8.6, §11.5);
- camps matched by `cell` and the `spawned_` switches (§6 conventions);
- treasury and town-store coin only, under the counters (§2.11);
- `ready` blocks if Phase 1a has landed, else the stand-in (§3.2.5).

**There is no target date.** The DM publishes `nh-purses` first, then Act I, whenever they are ready; any season works, because every calendar moment is optional flavour (§3.2.4) and the Moot's name follows the season it falls in.

**3. Nothing is computed from the publication day.** Each chapter's clock starts at its own opening (`minDays` `since: "chapter"`), and each set piece is timed from its own closing phase (§3.2.4).

**4. "The factors gave a moon's grace" (optional).** The residents' debt gossip promises something "by the new moon". If a new moon passes before Ch 1's chalk night (or before Act I is published), the DM **may** seed the realm talk key **`nh_debts=grace`** by hand. The debt gossip then plays a grace line in place of the "by the new moon" line (cast-and-talk §4.3–4.4 keep one per dialect; plain: "The factors gave a moon's grace. Generous of them, they say. It only means the interest runs a month longer."). Ch 1's setup notes whether the key was `grace` (story switch `was_grace`) and then sets it to `due`; if it was, the chalk-night notice begins *"THE GRACE IS OVER."* Nothing waits on the key, and nothing goes wrong without it.

**5. If an act's engine is late.** No act has a deadline. When an act's last chapter closes, its interlude waits; the DM publishes the next act when its engine is landed and tested, or publishes its today-only rows (§11.4 rows 4–6 for Act II: howls counted by Keeper-present choruses, generic letters, no Gift gate; rows 7–9 for Act III). Engine items that arrive later still reach a running act at a chapter boundary by moving runs to a new version.

---

## 4. The campaign's state (world variables and switches)

Declared once with the op `world-var {do:"declare"}` before Act I; every chapter and every quest pack that uses a variable declares the same name and type. World variables are loaded at the start of each run's tick and saved at its end, one run after another, so updates are never lost. Another story does not hear a change until C13a (`variable changed`): chapters poll on `hour`.

### 4.1 Progress (packs may add where marked **P**)

| Variable | Type | Meaning | Written by |
|---|---|---|---|
| `nh_stage` | number 1–9 | the chapter now running (packs gate their quests on it) | chapters |
| `nh_realm` | number | the realm's voice: every deed point of the campaign, cumulative | chapters, **P** |
| `nh_voice_ridgemere`, `_coldwater`, `_larkfield`, `_southmeadow`, `_fenshore`, `_saltwell`, `_oakhaven`, `_frosthollow`, `_mirebridge` | number ×9 | each community's strand, in deed points with distance weight | chapters, **P** |
| `nh_raised` | texts | communities whose voice is raised (permanent) | chapters |
| `nh_steps` | texts | Ember steps already fired, e.g. `coldwater-2` | chapters |
| `nh_keepers` | texts | communities whose gathering howl has been found (Ch 4 on) | chapters |
| `nh_posts` | texts | Howl Line posts raised (§5.2 ids) | chapters |
| `nh_surplus` | number | points past chapter goals | chapters |
| `nh_threat` | number 0–100, initial 30 | the Grey Captain's grip on the roads: +5 a camp formed, +3 a raid, −5 a camp cleared, −1 a bandit falling | chapters, **P** (bounties lower it) |
| `nh_names` | number | the Book of Names (Ch 4–6) | chapters, **P** (Ch 4–6 only) |
| `nh_bought` | number | debtors' howls pledged to Greatly; every 5 is one Purse voice at the Moot, 3 at most | chapters, Purse errands |
| `nh_back_lamp`, `_blood`, `_moot`, `_purse` | number ×4 | the backings' weight (sum of backers' `p_voice` at the moment they back) | chapters |
| `nh_t_stand`, `nh_t_step` | number ×2 | Dagfinn's Question totals | chapters |
| `nh_answered` | number 0–13 | posts that answered on the Moot night | Ch 9 |
| `nh_pushed` | texts | chapters the admin advanced (`ch1` … `ch9`), for the epilogue | each closing phase, when its cause was `advance` |
| `nh_mere_frozen` | switch | the Upper Mere is iced: winter, or Ch 8's early freeze (§3.2.4); packs read it | Ch 8 |
| `nh_moot` | text, initial empty | the Moot's name, set at the call: `midwinter` or `nine_howls` (§3.2.4); packs withdraw their `s9` quests when it is set | Ch 9 |
| `nh_scars` | texts | scars: `nobody_came`, `houses_first`, `voices_sold`, `signals_burning`, `glasswater_held`, `relay_broken`, `hills_lost`, `lord_absent`, `lights_faded`, `peace_broken`, `ford_burned`, `truce_broken`, `writ_lost`, `bell_lost`, `tansy_found` | chapters |

### 4.2 Residents lists (world `residents`, unique pieces)

| Variable | Holds |
|---|---|
| `nh_debtors_freed` | debtors freed (Ch 2) |
| `nh_bought_from` | debtors who pledged to Greatly (Ch 2; Ch 4 removes the ones bought back) |
| `nh_petitions_returned` | petitioners answered (Ch 1–3) |
| `nh_carriers_done` | Grayrock letter carriers whose letter was taken (Ch 5) |
| `nh_witnesses_heard` | weep witnesses who have spoken (Ch 8) |
| `nh_owners_returned` | owners of the Captain's cache who got their goods back (Ch 6) |

The source lists are built at authoring time from `live.npcs.backstory`, filtered to placed, alive residents aged 16 or more. Checked on 2026-11-22:
- debt: 41 with the hook, about 34 grown;
- weep witnesses: 46, aged 7 to 81 (children may speak);
- letters: 40;
- Watch lists: 35;
- petitions: 46.

Every chapter guards each step with `resident is <npc> alive`.

### 4.3 Branch results (world)

| Variable | Values | Set in |
|---|---|---|
| `nh_b_willa` | home, alone | Ch 1 |
| `nh_b_logger` | cleared, believed | Ch 1 (sets Oakhaven's Keeper) |
| `nh_b_purse1` | household, greatly | Ch 1 (the Queen's Purse) |
| `nh_b_ledger` | shown, kept, ash, none | Ch 2 |
| `nh_b_spy` | told_king, told_queen, paid, none | Ch 2 |
| `nh_b_seam` | honest, crooked, none | Ch 2 |
| `nh_b_lists` | bettany, press, captain, sold, lost | Ch 3 |
| `nh_b_desmond` | held, fled, free → later mercy, pillory, redeemed | Ch 3, Ch 7 |
| `nh_b_lorn` | turned, shamed, none | Ch 3 |
| `nh_b_relay` | number: posts answered at the rehearsal | Ch 4 |
| `nh_b_silver` | crown, greatly, none | Ch 4 |
| `nh_b_cough` | switch: Ottilie tended | Ch 4 |
| `nh_b_gudrun` | match, shown, ash, none | Ch 5 |
| `nh_b_hills` | oakhaven, split, fell | Ch 5 |
| `nh_b_toll` | cried, bargained, none | Ch 5 |
| `nh_b_grayrock` | crown, wharf, absent | Ch 5 |
| `nh_b_wharf` | switch: Aldric heard the drowned names | Ch 5 |
| `nh_b_envoy` | walked, alone | Ch 5 |
| `nh_b_lights` | laid, turned, faded | Ch 6 |
| `nh_b_quarrel` | fell_timber, houses_greatly, greatly_grayrock, against_crown | Ch 6 |
| `nh_b_peace` | switch | Ch 6 |
| `nh_b_sloe` | turned, exposed, none | Ch 6 |
| `nh_b_ford` | held, burned | Ch 7 |
| `nh_b_parley` | offered, refused, broken, none | Ch 7 |
| `nh_b_writ` | switch | Ch 8 |
| `nh_b_oskar` | confessed, exposed, fled, paid | Ch 8 |
| `nh_b_oda` | contest, accept, none | Ch 8 |
| `nh_b_tansy` | taught, pledged, hidden, found | Ch 8 |
| `nh_b_bell` | rung, cracked, lost | Ch 8 |
| `nh_b_fenwick` | switch: confessed | Ch 8 |
| `nh_b_stands` | switch: Dagfinn stands | Ch 8 |
| `nh_b_captain` | turned, beaten, fled, dead | Ch 9 |
| `nh_b_crown` | ottilie, lamp, regency, bought | Ch 9 |
| `nh_b_concord` | answers, holds, bends | Ch 9 |

**Packs read** any of these to gate quests. For example, a Fenshore pack offers "Bread for the bog-boats" only if `nh_b_sloe = turned`. **Packs never write** a branch.

### 4.4 Per wolf

| Variable | Type | Tracker label | Meaning |
|---|---|---|---|
| `p_voice` | number | "Your voice" | every deed point this wolf earned, campaign-long |
| `p_town_view` | number | "Your town's howl" | a mirror of `nh_voice_<p_howl>`, refreshed hourly by `each over participants` (an if/elif ladder of 9), and on each of the wolf's own deeds |
| `p_howl` | text | — | pledged community (one of 9) |
| `p_backs` | text | — | lamp, blood, moot, purse, or empty |
| `p_advice` | text | — | stand, step (Dagfinn's Question) |
| `p_marks` | number, hidden | — | Purse errands done |
| `p_sides` | number | — | personal side stories finished |
| `p_ch` | number, reset each chapter | — | this chapter's merit, for its tiers |
| story `ch_points` / `ch_goal` | number | "This chapter: done" / "This chapter: needed" | the chapter's shared bar |
| story `ch_helpers` | wolves | — | wolves who contributed this chapter (grows the goal) |

- **Scope.** J2 adds `scope:"realm"`: one value per wolf, kept in `dm.story_world_vars` and shared across campaigns by name and type. Until J2, the `p_*` variables are player scope with `carry` inside an act.
- **Fallback across acts.** The following world `wolves` lists (≤ 500 each) carry per-wolf state across acts: `nh_pledge_<community>` ×9, `nh_backs_<x>` ×4, `nh_roll` (every contributor), `nh_tier_ember` (`p_voice ≥ 20`), `nh_tier_blaze` (≥ 60), `nh_marked`, `nh_quick_registered` and `nh_quick_hidden`. `p_voice` itself restarts per act; the tiers keep the gist.
- **Trackers.** Four per wolf. Quest packs show at most one each, so the 16-tracker cap is safe.

### 4.5 Purse counters and camp switches
| Variable | Scope, type | Meaning | Written by |
|---|---|---|---|
| `coin_<purse>_day` ×9 | world number | coin paid today from that purse by every story | `nh-purses` resets it at `day begins`; every paying program adds (§2.11) |
| `coin_<purse>_ok` ×9 | world switch | the purse is above its floor | `nh-purses` only |
| `spawned_<region>` | story switch, one per story spawn region | this chapter's own wave stands in that region's cell | set by the chapter's `story.area.spawn` there; cleared when its `camp cleared` scores (§6 conventions) |
| `nh_debts` | realm talk key | `due`, `eased`, `called`, **`grace`** | optional `grace`: the DM by hand if a new moon passes before the chalk night (§3.4.4); Ch 1 (`due`) and Ch 2 |
| `nh_moot` | realm talk key | `midwinter`, `nine_howls` | Ch 9 at the call (§3.2.4) |

### 4.6 Readiness (every chapter, §3.2)
| Variable | Scope, type | Meaning | Written by |
|---|---|---|---|
| `ch<n>_did` | story `wolves` | wolves who did one of chapter *n*'s named tasks (§3.2.3); R1 counts it against the active wolves | every task trigger, once per wolf |
| `ch<n>_did_n` | story number | how many (for the boards, the bands of pre-written texts and the stand-in) | the same |
| `ch<n>_auto` | story switch, initial off | the soft maximum counts as participation met (§3.2.1) | the admin, from the console's Variables |
| `ch<n>_wait_bar` | story switch, initial off | readiness also waits for the bar | the admin |
| `was_grace` | story switch (Ch 1 only) | the optional grace key had been seeded | Ch 1's setup |

- **Program fields** of every chapter: `activeDays: 18` (C2) and, on its working phase, the `ready` block with the literals of §3.2.3 (`percent`, `floor`, `cap: 150`, `minDays`, `slowAfterDays`; `auto: true`). The console's bars show each part with its values (R3).
- **Phases** in every chapter: the working phase (two for Ch 4, 7 and 9) and the closing phase (§3.2.2); the last chapter of an act adds `interlude`.
- **Campaign triggers:** every task and scoring trigger, so deeds still count between readiness and the set piece.
- **Per-chapter lists** are story scope and restart with each chapter (C2 seeds `seen` at each launch); only `nh_pushed` is world scope.

---

## 5. The nine voices and the Howl Line

### 5.1 The Keepers (the resident who howls for each community)
No innkeeper is a Keeper **or a Keeper's fallback**: innkeepers carry only helpful knowledge and recaps (§10.3). Each Keeper takes pledges, gives their community's gathering-howl quest (Ch 4) and, where they keep a post, the post quest (Ch 9), and stands at their post on the Moot night. That is at most 2 campaign offers a chapter (§10.3).

| Community | Keeper (id), dialect | Why them | Fallback if dead or not yet named |
|---|---|---|---|
| Ridgemere | Herald Brenna Abbot `rmx_brenna_abbot`, plain. She takes pledges and gives the Ch 4 howl quest; Ridgemere's Ch 9 post quest is Cressida's waystation (post 12), not hers, and the Moot backing is Gerrit's (§2.6). | the campaign's voice; owes a factor herself | Corra Coldridge `rmx_corra_coldridge` (f 21, usher in the Antechamber, the herald's apprentice; plain) |
| Coldwater | Fallow Lakewick `tn_cw_fallow_lakewick`, coldwater | loom master, devout, distrusts the city, "resents every mile" to Ridgemere | Myrtle Stillford `tn_cw_myrtle_stillford` |
| Larkfield | Aspen Marshholm `tn_mb_aspen_marshholm`, larkfield | linen loom master with Mirebridge blood | Dun Harvestley `tn_sm_dun_harvestley` (the printer) |
| Southmeadow | Iris Meadowfield `tn_sm_iris_meadowfield`, southmeadow | farms Larkholm; "once walked all the way to Ridgemere and came back" | Rusk Plummeadow `tn_sm_rusk_plummeadow` |
| Fenshore | Roan Pikewater `tn_mb_roan_pikewater`, mirebridge speech | keeps Fenshore Stores; Mirebridge blood | Jory Corncroft `cty_jory_corncroft` (m 41, carter at Fenshore; mirebridge speech, broad) |
| Saltwell | Thistle Stonepick `tn_fh_thistle_stonepick`, saltwell | keeps Saltwell Stores; lost a brother in a mine collapse (Deepvein, Ch 8) | Lavinia Mudworth `cty_lavinia_mudworth`. Bramble Bogditch `tn_mb_bramble_bogditch` (78) *teaches* the howl and is never the Keeper, so her death changes only a side story. |
| Oakhaven | Ulla Rusthand `cty_ulla_rusthand`, oakhaven, if her name was cleared (Ch 1) | the wolf the town called a thief, howling for it | Cairn Oakhaven `tn_oh_cairn_oakhaven` |
| Frosthollow | Ulric Stonevein `cty_ulric_stonevein`, frosthollow | a miner who "still watches the road"; Hakon's brother | Aster Coldfell `tn_fh_aster_coldfell` |
| Mirebridge | Skye Eelbank `tn_mb_skye_eelbank`, mirebridge speech, named when the Book of Names is read (Ch 6). Until then Mirebridge pledges are taken by cartographer Imogen Nailridge `rm_imogen_nailridge` at Soundings `rm_soundings`. | the marsh guide of Withy Creek | Tamsin Eelbank `tn_mb_tamsin_eelbank` (70) |

**Raise marks** (deed points; with C2, × (1 + active/40)):

| Ridgemere | Coldwater | Larkfield | Southmeadow | Fenshore | Saltwell | Oakhaven | Frosthollow | Mirebridge |
|---|---|---|---|---|---|---|---|---|
| 40 | 20 | 16 | 12 | 12 | 10 | 10 | 10 | 20 |

- A scar touching a town adds 4 to its mark while it is unraised. "Lights laid" takes 2 off Fenshore's.
- A raised voice stays raised. A raised town **answers the Holding Howl by itself**: its Keeper howls from the post with no wolf there.
- So a lone wolf can raise Oakhaven's voice in an evening, and know that Oakhaven will answer on the Moot night because of them.

### 5.2 The Howl Line (13 posts, visible growth)
Only six posts are real town projects (`project.post`). That keeps within one open project per town (two per city) and the five project kinds. The other seven are story regions shown as gold `story.ping` markers when raised, with a board post. "Raised" for a region post means that town's voice is raised and its keeper has been stocked (Ch 9: a gift of `lamp_oil` and a `horn` or `handbell`).

| # | Post id | Post | Where | Kind | Comes in | Keeper on the night |
|---|---|---|---|---|---|---|
| 1 | `pass` | Frosthollow and the Pass | square `w_ridgemere_9_3` (4, 28), by the Pass House `cn_frosthollow_guest_inn` | region | Ch 3 (Frosthollow voice) | Ulric Stonevein |
| 2 | `upper_mere` | Upper Mere post | proj-1 at `w_ridgemere_8_6` (an old camp site) | **project** watchpost (Frosthollow) | Ch 3 | Kell Greyton `tn_cw_kell_greyton` |
| 3 | `coldwater` | Coldwater square | `w_ridgemere_7_7` (164, 4) | region | Ch 2–4 | Fallow Lakewick |
| 4 | `saltwell` | Saltwell square | `w_ridgemere_12_11` (220, 116) | region | Ch 4 | Thistle Stonepick |
| 5 | `southmeadow` | Southmeadow square | `w_ridgemere_8_13` (100, 20) | region | Ch 4 | Iris Meadowfield |
| 6 | `fenshore` | Fenshore boardwalk | `w_ridgemere_3_13` (92, 148) | region | Ch 4–6 | Roan Pikewater |
| 7 | `mirebridge` | Mirebridge crossroads | `w_ridgemere_5_11` (188, 52) | **project** watchpost (Fenshore) | Ch 6 | Skye Eelbank (walked there at dusk) |
| 8 | `larkfield` | Larkfield ford | `w_ridgemere_6_9` (140, 76) | **project** mend (Larkfield) | Ch 7 | Aspen Marshholm |
| 9 | `glasswater` | Glasswater relay (the junction) | the cleared camp site `w_ridgemere_6_8` | **project** watchpost (Coldwater) | Ch 3 | Willa Coldwick `tn_cw_willa_coldwick` if home, else Harrow Reedford `tn_cw_harrow_reedford` |
| 10 | `oakhaven_road` | Oakhaven road post | proj-2 at `w_ridgemere_5_4` (Alder Mere) | **project** watchpost (Oakhaven) | Ch 1 | Moss Blossomwell `tn_sm_moss_blossomwell` |
| 11 | `oakhaven` | Oakhaven square | `w_ridgemere_3_4` (244, 196) | region | Ch 1–4 | Ulla Rusthand or Cairn Oakhaven |
| 12 | `waystation` | Ridgemere waystation | proj-3 at `w_ridgemere_1_1` | **project** waystation (Ridgemere) | Ch 1 | Cressida Cotton `rmx_cressida_cotton` |
| 13 | `bay_light` | the Bay Light, where the howl comes in | `r2_the_bay_light_up` (door `w_ridgemere_0_0` (71, 179)) | region | stands already | Solveig Tarwell `rm_solveig_tarwell`, whose chapel bell answers |

- **Arms.** The north arm runs 1 → 2 → 3 → 9. The south arm runs 4 → 5 → 6 → 7 → 8 → 9. The trunk runs 9 → 10 → 11 → 12 → 13.
- **The camps sit where the Line must run:** `5_4` under post 10, `6_8` under 9, `8_6` under 2, `6_12` and `7_12` by 7 and 5. That is the physical logic, and the reason the Captain fights the Line.
- **Posts 2, 10 and 12 already stand open in the world** (proj-1 at `8_6` for Frosthollow, proj-2 at `5_4` for Oakhaven, proj-3 at `1_1`, the waystation: state open, worked 0, needing stone 4 and timber 10 or 8). The chapters do not post them. **A Line post counts as raised when its project is built and no story camp stands in its cell** (its `spawned_` switch is off). Wolves raise it by clearing the camp and gifting the stone and timber. The other Line projects (posts 7, 8 and 9) are posted by their chapters as written.
- **Surplus posts** (bought with `nh_surplus`, one open project per town at a time): a Southmeadow watch post on the `7_12` camp site and a Saltwell waystation. They add nothing to the Line, but they stand in the world, and the Captain's camps there stop re-forming.
- **Lapse.** Projects lapse after the configured `lapseDays`. A chapter re-posts a lapsed Line project at its next opening.

### 5.3 How a community's voice is fed (one rule for chapters and packs)
- Every deed adds points to `nh_voice_<where it was done>`, weighted by distance.
- A deed done by a pledged wolf also adds a quarter-share (at least 1) to `nh_voice_<p_howl>`.
- A wild deed is credited to the town it serves, by the event's **`cell`** (not its `target`). So a natural camp (`camp_w_ridgemere_<cell>`) and a story camp (`camp_story_<n>`) in the same cell both credit the town. That is right: either way the road is clear.

| Camp cell | Credited to |
|---|---|
| `5_4` | Oakhaven |
| `6_8` | Coldwater (2/3) and Larkfield (1/3) |
| `8_6` | Frosthollow |
| `6_12` | Mirebridge |
| `7_12` | Southmeadow |
| `1_3`, `2_3`, `3_3` | Ridgemere |
| `6_3` | **Frosthollow** (the lime quarry is Frosthollow's work; it agrees with `sq-scree-goats`; ibex can be hunted there: 1,304 rock and 3,984 snow tiles) |

- **Ember steps** come from one hourly campaign trigger per community (9 triggers). Each is an if-ladder on the strand against its mark, and each step fires once through `nh_steps`.

---

## 6. The chapters

Each chapter section covers the same points in the same order:
1. when and where;
2. opening;
3. shared goal and scaling;
4. **ends on**: the readiness rule, the big beat and the close (§3.2);
5. visible world change;
6. the Captain's beat;
7. branches and lasting consequences;
8. side storylines (the door quest first);
9. lone wolf and crowd;
10. recognition;
11. failure;
12. quest types;
13. talk;
14. triggers.

**Conventions:**
- "Journal offer" means a `quest.offer` with no giver, sent from a trigger to one wolf. That is how every gated quest avoids the giver leak.
- A "chain" is `npc.offer` steps on `dialogue choice`.
- Deed phrases are written exactly as the game will speak them.
- **Talk.** Every chapter's talk lines go in `Data/Talk/rules/story_ninth_howl.talk` (barks and smalltalk) and `Data/Talk/scenes/common/story_ninth_howl.talk` (NPC-to-NPC). They are gated on `world.mem.nh_stage=<key>` plus `self.dialect`.
- **Window lines and helpful lines** go in `resident.tree` `news` and `gossip` (`say[world.mem.nh_stage=<key> self.dialect=<d> priority=2]`).
- **Cast lines** are `say[self.id=<id> world.mem.…  priority=3]`.
- **Gates.** No line uses a weather or time word without the matching `here.weather` / `here.time` gate. Names travel through `#world.figure_*#` or roles, never written out.
- **Camps** (decisions §4; this applies to every "break the camp" below).
  1. **Matching.** A chapter's own wave is matched by **`cell` plus the story switch `spawned_<region>`**. The switch is set when the chapter calls `story.area.spawn` in that region and cleared when it scores. It is **never** matched by `camp_w_ridgemere_<cell>`: story spawns are named `camp_story_<n>` (`Core/RatwRoads.cpp:1622`), and most natural camps are inactive at the checkpoint.
  2. **No camp objective.** `camp cleared` is not a mission event. Every "break the camp" is a **trigger** on `camp cleared` (match `cell`, switch on) that clears the switch, scores the chapter's bar and does `objective.complete` for each participant `wolf in region` where a quest is waiting on it. `bandit falls` stays a plain objective.
  3. **From engine Phase 2 (L2, L3):** spawns name their camp (`camp: "nh_alder_mere"` → `camp_story_nh_alder_mere`; names begin `nh_`), `camp cleared` becomes a mission event, and a new version of each chapter moves to exact `target` matches.
  4. **Credit** is by `cell` (§5.3).
- **Speech.** No `story.tell` carries `from` with a resident's words (§2.1). Proclamations are signed `board.post` notices plus the speaker's own window line.
- **Readiness** (§3.2, §4.6). Every chapter has a `ready` block on its working phase and a closing phase. Its **Ends on** names the tasks (each task trigger also does the `ch<n>_did` step), the share, the floor, the minimum and the soft maximum in game days, and the set piece's timing. "The close" in a chapter means §3.2.1's close: readiness, an admin's Advance, or `ch<n>_auto` at the soft maximum. No chapter text names a game day.

### Chapter 1: Where Is the King? (opens at publication, in any season; typically 2 real days at 5 wolves, 1.8 at 200)

**Where:**
- Market Square `w_ridgemere_1_1` (104, 56);
- the Sump: Sump Stairs in `w_ridgemere_0_1` and the Sunken Street `r2_the_sunken_street`;
- the King's Residence `r2_the_king_s_residence` (with `_up` and `_down`);
- the Records Office `r2_the_records_office`, the Couriers' Office `rm_the_hall_of_records`, the Soup Kitchen `rm_the_soup_kitchen`;
- the Alder Mere camp on the Oakhaven road `w_ridgemere_5_4`;
- Coldwater Stores `cn_coldwater_store`, the Coldwater square `w_ridgemere_7_7` (164, 4), the Oakhaven square `w_ridgemere_3_4` (244, 196);
- proj-3 (waystation, `1_1`) and proj-2 (Oakhaven road watch post, `5_4`);
- all eight boards.

**Opening.**
- Entry effects:
  - A plain world narration (`story.tell`, no `from`): *"Bells toll from the Palace. King Gunnar has gone up to the Bright One."*
  - `board.post` on all eight boards: *"THE KING HAS GONE UP TO THE BRIGHT ONE. The Palace lamp is kept. Who keeps it, the herald will say when the herald is sure. Wolves of the road, ask in the Sump for the one who keeps it. (signed) The Herald of Ridgemere."*
  - Brenna's own window line (`say[self.id=rmx_brenna_abbot world.mem.nh_stage=king priority=3]`): "Hear the Palace. King Gunnar has gone up to the Bright One, and the Palace lamp is kept. Who keeps it, I'll say when I'm sure."
  - `npc.line` for Cressida Cotton: "Have you seen him? Russet, notched ear like mine. Smells of the Sump."
  - Realm key `nh_debts=due` (if the DM had seeded the optional `grace`, the setup trigger first turns on `was_grace`, §3.4.4).
- A `story phase` setup trigger (`once: campaign`):
  - `npc.protect` and C11 `npc.spare` on the cast of §8.6;
  - the chalk-night timer: chapter game day 3, or a new moon on game day 2–5 (§3.2.4), a `schedule` the DM can `shift`; nothing else is timed from a date;
  - `story.area.spawn` 4 at the Alder Mere camp region (cell `5_4`), and the switch `spawned_alder_mere` on (with L2: `camp: "nh_alder_mere"`);
  - `npc.move` Willa Coldwick into the camp, then `npc.hold`.
- A daily `schedule` `dusk_round` walks Dagfinn (`npc.walk`, pace walk) to three points of the Sump round and home.

**Shared goal: "Find the King, and let every hearth hear it"** (`ch_goal` = 30 + 4 per helper). Points:
- **The Summons.** Brenna gives each wolf who asks a `letter` (`item.give`) with a journal note. A `gift` of a letter (or `gift again`) to a town's Keeper scores 2 × distance, once per wolf per town (8 triggers, `once: member`).
- **The Alder Mere camp.** A trigger on `camp cleared` with `cell` `5_4` and `spawned_alder_mere` on scores 10 (Oakhaven), clears the switch, and completes Willa's objective 3 for each participant in the region. `bandit falls` scores 2.
- **The first two posts.** proj-3 (the waystation) and proj-2 (the Oakhaven road post) already stand open (§5.2); the chapter does not post them. A `project gift` to either scores 1. `project built` scores 10; proj-2 counts as raised only once the Alder Mere switch is off.
- **Side stories** score 6 each.

**Ends on** (§3.2): **30%** of the active wolves, **at least 3** (or every active wolf if fewer), have done a task, and **10 game days** have passed; or an admin push. Soft maximum **30 game days**.
- **Tasks:** carried a Summons to a Keeper (the scored `gift`); fought at the Alder Mere camp (a `bandit falls` in cell `5_4`, or in the region when the camp is cleared); found the King (Where Is the King? done); gave to proj-2 or proj-3.
- **The beat inside:** the chalk night (below), on chapter game day 3 or a new moon on game day 2–5. It is timed, not a close.
- **The close:** the Herald's Word at the next dusk at least 3 game hours after readiness or a push. With the bar met, the Blaze below; with it short, the Failure road.
**Visible change.**
- **1/3.** All boards: "The Summons is on the roads. Wolves carry it to every hearth." Dagfinn is seen climbing the Hundred Steps at dawn (`npc.walk`).
- **The chalk night** (chapter game day 3, or a new moon on game day 2–5; §3.2.4):
  - clerks Rune Gullwell and Ivar Pitchwick walk the Sump doors at morning;
  - the Ridgemere board posts *"BY ORDER OF THE COUNTING HOUSE: notes fall due at the next new moon, or when the House calls them. A debtor who cannot pay may pledge his howl at the Moot to House Greatly, in lieu."* If `was_grace` is on (§3.4.4), the notice begins *"THE GRACE IS OVER."*;
  - C8 `talk.remember` npc-scope `nh_debt=pressed` on each debtor (today: the DM seeds it).
- **2/3.** Public scene `lamp_at_dusk` at Sump Stairs, with Dagfinn and Cressida: "I kept a lamp. I didn't ask for a crown."
- **Blaze** (the close, §3.2: the next dusk at least 3 game hours after readiness or a push, with the bar met): the public scene **"The Herald's Word"** on Market Square.
  - Brenna reads the lamp-book line.
  - Gerrit Mudhand answers for the Council: "An old wolf's note in a lamp-book is not law."
  - Brenna: "Then let the realm howl a Moot, and say so."
  - The Moot is called, with no night named: "When the nine voices are ready, the herald will name the night." C9 `figure.set` ruler = Dagfinn, title "keeper of the Palace lamp", known in the realm.
  - The talk key moves from `nh_stage=king` to `moot_called`.
  - Pledges open at every Keeper (an `npc.offer` each, untargeted until J1), and every participant gets a journal note. From Ch 2 the Moot backing opens at Speaker Gerrit (§2.6).

**The Captain's beat.**
- The Alder Mere camp doesn't starve out.
- Oakhaven's board gets a notice signed with a grey paw-mark: *"The Oakhaven road has a keeper now."*
- `bandits.call` 2 near a lone wolf on `5_4` at night (`hour between 21 and 4`, `wolf in region`), once per wolf per night.

**Branches** (the first leanings):
- The Queen's Purse: `household` or `greatly` (a tally) seeds the Blood or Purse backing.
- `nh_b_willa`: home, or alone.
- `nh_b_logger`: cleared, or believed. This sets Oakhaven's Keeper.

**Side storylines.**
1. **Where Is the King?** The door quest, and the first quest any wolf sees.
   - **Giver:** Cressida Cotton `rmx_cressida_cotton`, Dagfinn's patrol partner, Frosthollow-raised. *say:* "#greet#. You've a nose on you, I can tell. My patrol partner's been handed the Palace lamp and he's still walking our round. Find him before the Houses do." *thanks:* "#thanks#. He asked after you. Don't let it go to your head."
   - **Objectives:**
     1. Ask a Sump Stairs porter: a hidden signal objective, completed by a trigger on `conversation` with any of `rm_mathis_slagwell`, `rm_oda_greywell` or `rm_piet_wickridge`.
     2. `enter place r2_the_sunken_street`.
     3. `near resident rmx_dagfinn_thornham` while `calendar is period dusk`.
     4. `conversation` with him. This opens his chain (§10.4).
     5. Optional: walk three posts of his round with him (`region entered` ×3 in order).
   - **Deeds:** "found the Sump King on his own round" (`once: campaign`, the first finder only), and for everyone "walked the Sump round with the new King".
2. **Willa's Wheel-Rut** (Coldwater).
   - **Giver:** Myrtle Stillford at Coldwater Stores. *say:* "#greet#, #address#. Willa took the wagon out on the Oakhaven road on Hearthday and it hasn't floated home. She knows that road blind. Would you look?" *thanks:* "#thanks#. Net and line, I thought we'd lost her."
   - **Objectives:**
     1. Find the wheel: `item picked up` `cart_wheel`, placed by `item.place` on the road below Alder Mere.
     2. `region entered` the camp, following the scent of wet ash downwind of the mere.
     3. Free her: a hidden signal objective, completed for each participant in the region by the Alder Mere `camp cleared` trigger (cell `5_4`, `spawned_alder_mere` on). A lone wolf can instead take the small-fight variant, `bandits.call 3` at the wolf and then `bandit falls` ×3.
     4. Walk her home: `npc.release`, `npc.follow` the wolf, then `region entered` the Coldwater square with `resident in region` Willa.
   - **Result:** Coldwater +6. With C5, Willa greets the wolf by name for the rest of the campaign.
   - **Optional:** carry Willa's Oakhaven Summons on to Oakhaven's Keeper.
3. **The Wronged Logger** (Oakhaven).
   - **Offer:** a journal offer on `region entered` of the Oakhaven square. Cairn Oakhaven blames Ulla Rusthand for the robbed wagon's goods; 222 residents believe her a thief.
   - **Objectives:**
     1. Talk to Ulla.
     2. At the camp, find the stolen wool (`story.area.item wool` in the camp region; `item picked up`). It carries the camp's wet-ash smoke, not her cedar sap.
     3. Carry it back to Cairn (`gift wool`).
     4. Put three gossipers right: `conversation` with 3 of an authoring list of residents who hold the belief (from `game.beliefs`).
   - **Result:** `nh_b_logger = cleared`, and Ulla becomes Oakhaven's Keeper. *Her line:* "Shade on you. They'll let me howl for the town now, mm? Root and rain, I didn't think I'd want to." With C8, `belief.plant` "never took a thing from the wagon"; with J8, `belief.forget` on her theft beliefs.
   - **Default** at the close: `believed`. Cairn is the Keeper, and Ulla's lines stay wary.
4. **The Queen's Purse** (crown S1.2).
   - **Giver:** Queen Oda at the Residence. *say:* "#greet#. A King's wake, and the Houses offering to pay for it. I'd rather pay in my own coin, eh? Help me find it."
   - **Objectives:**
     1. `archive work` ×3 at the Records Office, to find the household's old tolls.
     2. 2 `bread` and 2 `herbs` to the Soup Kitchen's keeper.
   - **The choice** at hand-in, a chain: "Pay it from the household purse" (Blood +2, tallied to `household`), or "Let Greatly lend it" (Purse +3; tallied to `greatly`; Ambrose's first loan to the Crown). Oda remembers each wolf's advice.

**Lone wolf and crowd.**
- **A lone wolf.** One wolf can carry the Summons to all eight towns in about two real days, every town greeting them by deed. They can free Willa with the small fight, and the floor rule lets them advance the chapter alone (§3.2.3).
- **A crowd** breaks the camp in an evening. Surplus at 2 × goal buys **Lantern Night**: `festival.call ridgemere "Lantern Night" inDays 2`, and early wake bread (`story.area.buff fed` on Market Square, 24 h).

**Recognition.**
- `p_ch ≥ 3`: deed "kept the lamp's first watch with the realm".
- `p_ch ≥ 8`: Cressida greets the wolf by name for 7 days (C5).
- Top 3 (C3): the title "Lamp-bearer" (C4) and a board post naming them (C1).
- The first finder's name goes on the Ridgemere board.

**Failure.**
- **Closed with the bar short** (readiness, Advance or `ch1_auto`, §3.2). The Herald's Word plays anyway, but Gerrit reads the Council's notice first: `nh_back_purse` +5, and the scar `houses_first`.
- **Willa not freed.** She is released and walks home alone: scar `nobody_came`, and Coldwater's mark +4.

**Quest types:** a mystery hunt (scent and talk), courier, camp-breaking, rescue and walk-home, name-clearing, archive errand, building.

**Talk** (keys `nh_stage=king`, then `moot_called`):

| Dialect | Gossip | Helpful knowledge |
|---|---|---|
| plain | "The old King's gone up to the Bright One. And they say the one keeping the Palace lamp still walks the Sump at dusk." | "Looking for the wolf with the lamp? Ask on the Sump Stairs. The porters always know who's passed." |
| coldwater | "A Moot, eh? My grandsire spoke of one. Said it was the only time the city asked the lake anything." | "The carter's cart went out on the Oakhaven road and hasn't floated home. Net and line, somebody look below Alder Mere." |
| larkfield | "A Watch guard keeping the Palace lamp? By the market cross, the Houses will have that weighed and short-weighed by Marketday." | "The Summons came on the cart. Carry one on to the looms if you're going: Larkfield wants its own reading, lambkin." |
| southmeadow | "A Moot, called? Ripe days for talkers, then." | "We've had no city word since the spring. Blight take the roads. Bring the Summons south and you'll be stood a cup." |
| mirebridge (Fenshore) | "That lamp-keeper's got fen in his mouth, I hear. Talks like my uncle, no?" | "Fen-sure, the city boards are slow. Walk the Summons down the Fen Road yourself and you'll be the first." |
| saltwell | "The Summons reached the well, honoured guest. The well remembers who drew from it. We will see whether the city does." | "If you carry the city's word, my brother, the far towns want it most. Saltwell has heard nothing since the last caravan." |
| oakhaven | "Slow news, the city. A king gone up a fortnight before the Sound Road hears it, mm?" | "Camp's on the Alder Mere side, under the windthrown oaks. You'll smell it before you see it, mm?" |
| frosthollow | "Patrolman keeping the Palace lamp. Deep strange. Keep your lamp lit, they say. Well, someone is." | "Carters down from the Pass say there are lamps on the High Road that aren't ours. Cold iron. I'd walk in a pair." |

- **Recap** (every innkeeper's `inn.greet`, stage `moot_called`): "You're new on the roads? The old King's gone up, a Sump patrolman keeps the Palace lamp, and the herald's called a Moot."
- **Window lines:**
  - Dagfinn (`king`): "#greet#. Don't call me that. I've a round to walk, and the alleys don't care what the Palace says, no?"
  - Cressida: "He's still walking our round. Cold iron, I'd walk it too if they'd handed me a lamp and no say in it."
  - Brenna (`moot_called`): "#greet#. I read it, and I'd read it again. A Moot of the nine voices. Will you howl for a town?"
  - Oda: "Every penny counted twice and the wake still short, eh? The Houses lend with one paw and take with the other."

**Triggers.**
- **Existing:** `story phase` setup; `npc.protect`, `npc.move`, `npc.hold`, `npc.release`, `npc.follow`, `npc.walk`; `schedule`; `item.place`; `item picked up`[w]; `story.area.item`; `story.area.spawn` with a story switch; `camp cleared` (a trigger, matched by `cell`); `bandit falls`; `bandits.call`; `project gift`; `project built`; `gift` and `gift again` with `once: member`; `conversation`; `near resident`[w]; `enter place`; `region entered`; `archive work`; `scene.play`; `board.post`; `story.tell` (narration only); `deed.award`; `festival.call`; `story.area.buff`.
- **Gaps:** C8 (`talk.remember`, `belief.plant`), C9, C11, C5, C3, C4, C1. **Today-only:** the DM seeds the keys and the figures, and sets the `protected` flag (§3.4.2).
- **New:** R1–R3 and C2 (readiness, Phase 1a; until then the stand-in, §3.2.5), J1 (per-wolf offers), J4 (the new moon, optional), J8 (`belief.forget`); L2 / L3 named camps (Phase 2).

### Chapter 2: By the New Moon (to the Reckoning; typically 2.5 real days at 5 wolves, 2.2 at 200)

**Where:**
- the Greatly Counting House `rm_the_greatly_counting_house`, the Greatly Bank `rm_the_greatly_bank` and its flat `rm_the_greatly_bank_flat`, Greatly House `rm_greatly_house`;
- the Pawn and Ledger `rm_the_pawn_and_ledger`, the Customs House `r2_the_customs_house` and `_up`, the Antechamber `r2_the_antechamber`;
- the Watch House `rm_the_watch_house`, the cells `r2_the_keep_down`, the Residence kitchens `r2_the_king_s_residence_down`, the Carters' Yard `r2_the_carters_yard`;
- Larkfield: the Press `cn_larkfield_press` and the Longmead granary `fs_grain_long_granary`;
- Frosthollow: the Mine Captain's House `cn_frosthollow_mine_house` and the mine `w_ridgemere_9_2`.

**Opening.**
- C9 `figure.set` villain = Ambrose Greatly, title "master of the Counting House", known in the realm. Every `#world.figure_villain#` line now plays.
- The Counting House notice on every board, if Ch 1 did not post it.
- `npc.line` for Rune Gullwell: "Notes fall due at the new moon. I only carry the chalk." For Ambrose: "A voice is only a debt with a tune to it."
- The factors' morning walks continue.

**Shared goal: "Not One Voice Sold."** `ch_goal` = 8 + 1 per helper debtors, capped at the 34 grown debtors. A debtor is freed (`nh_debtors_freed`) by one of:
- **labour:** `trouble solved` with that debtor as `target`;
- **goods:** a `gift` or `gift again` to them of 2 `bread`, `salt`, `herbs` or `lamp_oil`;
- **coin:** a single `gift` of ≥ 10 coins (one path among four, never the bar's whole);
- **proof:** the ledger shown (below).

Each freed debtor scores 4 points (Ridgemere). Four debtors count triple: Captain Corra Slagmore, Herald Brenna Abbot, Steward Edric Kelpham and the lamplighter Piet Lumley (`rmx_piet_lumley`, "lights the lamps at dusk").

**Ends on** (§3.2): **25%** of the active wolves, **at least 2**, have done a task, and **12 game days** have passed; or an admin push. Soft maximum **36 game days**.
- **Tasks:** freed a debtor (labour, goods, coin or proof); finished a Ch 2 side story (the ledger, the Captain's note, the ear, the seam note) or the door quest.
- **The close is the Reckoning** (§3.2.4): at a new moon if one falls within 3 game days of readiness; otherwise at dusk the next game day, when the Counting House calls its notes early and every board reads *"THE COUNTING HOUSE WILL NOT WAIT ON THE MOON."* The Blaze (if the bar is met) and the pledges below both happen at the Reckoning.
**Visible change.**
- **1/3.** The Larkfield Press handbill *"A debt is not a voice"* on the Larkfield and Ridgemere boards.
- **2/3.**
  - The debtors' talk turns from fear to anger: npc keys `nh_debt=freed` or `nh_debt=angry`.
  - The factors stop knocking (`cancel` the schedule).
  - **Ambrose's offer** (a chain at Ottilie): "Should the Crown take Greatly's loan and pay every small note?" Accept and every remaining note under 20 pennies is forgiven, but the Crown owes Greatly: Greatly's House vote counts 2 at the Moot, as the boards say plainly, and `nh_back_purse` rises. Refuse and the race goes on.
- **Blaze** (at the Reckoning, if the bar is met). A signed board post on all eight boards, *"The Reckoning came to paid doors. (signed) The Herald of Ridgemere."* (on a new moon: *"The new moon rose on paid doors."*), and the same words in Brenna's window line (`say[self.id=rmx_brenna_abbot world.mem.nh_debts=eased]`).

**At the Reckoning** (the close).
- Every unfreed debtor pledges their howl: `list add nh_bought_from`, `add nh_bought 1`.
- More than half unfreed adds the scar `voices_sold`.
- Every 5 bought is one Purse voice at the Moot (3 at most) until bought back in Ch 4.

**The Captain's beat.**
- Desmond Hollis (`rmx_desmond_hollis`, the dismissed Palace guard) walks to the Carters' Yard each night (`npc.walk` on a schedule).
- He never speaks aloud in the world (`npc.speak` is not used: NPCs talk only in the dialogue window). His words are in **his own window**:
  - today: a tree line in `watch.tree`, `say[self.id=rmx_desmond_hollis world.mem.nh_stage=reckoning priority=3]`: "One mark for the wagon days. Greatly pennies spend the same, eh?" (the DM seeds the key until C8);
  - once C5 exists: `npc.greet` from him to the wolf who came `near resident` him at the yard, with the same words.
- A wolf `near resident` him there at night gets the player switch `p_saw_desmond`, which opens his Ch 3 thread.

**Branches.** `nh_b_ledger` (shown, kept, ash), `nh_b_spy`, `nh_b_seam`.

**Side storylines.**
0. **Door quest: "A Note on the Door."** A journal offer on `arrival`: free one debtor.
1. **A Ledger That Does Not Add Up** (large).
   - **Giver:** Rosalind Greatly `rm_rosalind_greatly`, in the Greatly House gardens. *say:* "#greet#. I've read every ledger in the Counting House, and one of them doesn't add up. I can't carry it out myself. Could you?"
   - **Objectives:**
     1. Get the Counting House day book from Steward Edric Kelpham: free his debt, or `promise` him.
     2. Compare it with the Crown's customs tally: `enter place r2_the_customs_house_up`, then `archive work`, with Jasper Bilgeford `rm_jasper_bilgeford`.
     3. The factors' notes at the Pawn and Ledger: Jorunn Ropewick `rm_jorunn_ropewick`, a `gift` for her trust.
     4. Eleanor Saltcot `rm_eleanor_saltcot` at the Bank confirms the sums.
   - **The finding:** the Sump debts are charged twice, once to the debtor and once to the Crown's customs. In the margin: *"Deepvein note: pledged to three."*
   - **Each finisher chooses** (chain at Rosalind; tallied by `p_voice`):
     - **Show it to the Council** (to Gerrit at the Antechamber; `shown`):
       - the Sump's doubled debts are struck, so every Sump debtor still listed is freed at once and `nh_bought` is halved;
       - `faction.relation house_greatly council_of_houses tense` (B);
       - Rosalind is turned out and sleeps at the abbey (talk, plus a nightly `npc.walk` to `r2_abbey_quarters`);
       - Greatly's House vote at the Moot is cast by Celestine, not Ambrose.
     - **Bargain with Ambrose** through Edric (`kept`):
       - every note under 20 pennies is forgiven, and the big notes stay (Corra Slagmore's among them);
       - Ambrose owes the realm one favour: Greatly's House vote abstains at the Moot;
       - Rosalind turns cold to wolves.
     - **Burn it** (`ash`): nothing changes in the world. Rosalind trusts that wolf above all others, and in Ch 8 she says which hiding place Oskar feared most.
   - **Deed:** "read the ledger that does not add up".
2. **The Captain's Note.**
   - **Offer:** a journal offer to wolves with `p_voice ≥ 6`. Corra Slagmore at the Watch House: "I'll not talk of my note in my own Watch House."
   - **The paths:** free her debt (triple), or find who sold her note to Greatly. That leads to cell-keeper Desmond Dripcot `rm_desmond_dripcot` in `r2_the_keep_down`, who hints at his list (the Ch 3 seed).
   - **Result:** a freed Corra thanks the wolf by name (C5), and her Ch 3 "captain" path becomes the redeemed one.
3. **The Ear in the Residence.**
   - **Offer:** a journal offer on `enter place r2_the_king_s_residence_down`. Servant Mab Coalridge `rm_mab_coalridge` holds 218 pennies, far too much for her wage.
   - **Objectives:**
     1. Talk to her, and then to Eleanor Saltcot: Counting House ink smells of lamp oil.
     2. Follow her at night to the Bank: `near resident` with `hour between 22 and 4`.
   - **The choice:**
     - **Tell Dagfinn** (`told_king`): Mab keeps to the kitchens and speaks to no one. In talk, the spy is gone, so Ambrose learns of the writ search late in Ch 8.
     - **Tell Oda you know** (`told_queen`): Oda owes the wolf, and is reconciled with Ottilie in the Blood ending.
     - **Take Mab's coin** (`paid`): 10 pennies, "paid out of the Bank" (the treasury `town:ridgemere` until SQ2, then `till:job:rm_eleanor_saltcot`; under the counters, §2.11), and `p_marks` +1.
4. **The Seam Note** (Frosthollow).
   - **Offer:** a journal offer on entering the Frosthollow square. Greatly holds the mine's note.
   - **The temptation:** Fenwick Plankworth `cty_fenwick_plankworth` offers the wolf a share to "lose" the Counting House rider's letters on the Pass Road.
   - **The honest road:** fill the mine's quota with Ulric Stonevein: 10 `iron_ore` given to Fenwick, or 3 `hunted` for the miners' larder.
   - **Result:**
     - `honest`: Frosthollow's mark −2, and Fenwick will confess more easily in Ch 8.
     - `crooked`: Fenwick is richer and Frosthollow's mark +4. In Ch 8 the Captain names him aloud.

**The Purse path: "The Counting House's Errand."**
- **Offer:** a journal offer to Purse backers, from Rune Gullwell.
- **The errand:** talk to a debtor not yet freed and pick the chain "Sign the pledge". That debtor goes to `nh_bought_from` and `nh_bought` rises by 1.
- **Pay:** 5 pennies (the treasury until SQ2, then the Greatly till; under the counters, §2.11), and `p_marks` +1.
- **Deed:** "called in a Sump debt for the Counting House". Its gossip follows the wolf.

**In the packs:** the Short-Weighed Sheaves is now chapter 1 of the large line `lq-clipped-silver` (giver Pike Chalkley): the false beam at the Larkfield market is the first thread, leading to the clipped silver. Its talk carries stage 1–2 words (the factors' new moon). Like every pack it only feeds `nh_realm` and the strands; it writes no branch and no handbill of the campaign's.

**Lone wolf and crowd.**
- **5 wolves:** about 13 debtors, and each one thanks their freer.
- **200 wolves:** all 34 in hours. The surplus buys **the Sump Feast** (`festival.call ridgemere "the Sump Feast" inDays 2`; `story.area.buff fed` on Sump Stairs, 24 h). Every further deed fills `nh_surplus`, which Ch 4 spends to buy back pledges.

**Recognition.**
- Deed: "freed a Sump debtor before the Reckoning".
- `p_ch ≥ 8`: a freed debtor greets the wolf by name (C5).
- Top 3: the title "Debt-breaker" (C4).
- The wolves who freed the Captain or the herald are named on the Ridgemere board.

**Failure.** Pledges at the Reckoning, and the scar `voices_sold`. Chapter 4's "Buy Back the Howls" can undo it.

**Quest types:** labour relief, goods hauls, an investigation chain, night shadowing, an ore quota, a printing errand, and a bad-side errand.

**Talk** (`nh_stage=reckoning`; then realm key `nh_debts=eased|called`):

| Dialect | Gossip | Helpful knowledge |
|---|---|---|
| plain | "They want my howl for my note. A howl! As if it were a spoon to pawn." / eased: "A wolf of the road worked off my note and asked nothing back. I'll howl how I please now." | "Want the notes? The Pawn and Ledger buys and sells them. Smell the ink: Counting House ink smells of lamp oil." |
| coldwater | "Factors chalking doors in the city, eh? Out here we'd call that marking a net that isn't yours." | "The carters' yard in the city is buying wagon days by night. Somebody's paying to know where our carts go, eh?" |
| larkfield | "Short-weighed, the lot of it. Fair price to you, they say, and the scale's got a thumb on it." | "Weigh a sack at Longmead and weigh it again at the press. The printer wants the difference in ink, lambkin." |
| southmeadow | "Notes due at the new moon? Chaff and promises, my apple." | "Help a debtor with a day's work and the city counts it paid. A good harvest from a little sweat." |
| mirebridge | "Owe a House a penny and they own your boots. Lucky we go barefoot in the fens, no?" | "The Fenshore boats won't carry Counting House chests. Boggy coin, that." |
| saltwell | "A loan the borrower cannot refuse is not a loan, my sister. It is a leash." | "Bring bread and salt to a debtor's door, honoured guest. In the old way, a guest's gift cannot be called in." |
| oakhaven | "They chalk doors in the city, mm? We'd mark a tree that way before we felled it." | "A debt's lighter carried by two. Lend a paw at a debtor's work and see, mm?" |
| frosthollow | "Captain's quiet. Too quiet. Dead rock talk, that note on the mine." | "The mine's short of ore and the note's due. Bring a load down to the mine house and it's one less hold on us." |

- **Recap:** "You've missed a moon of it. The herald called a Moot, and now the Counting House wants every debtor's howl by the Reckoning."
- **Window lines:**
  - Ambrose: "#greet#. A city that pays what it owes. You'd be surprised how few wolves want that."
  - Rosalind: "Three sums, and none of them agree. Someone's counting twice, and I think I know whose paw it is."
  - Corra Slagmore: "Ask me again on a better day. Not here."
  - Piet Lumley, when freed: "I light the city's lamps and couldn't light my own. #thanks#."

**Triggers.**
- **Existing:** `gift` and `gift again` with `in list` and `measure: coins`; `trouble solved`; `promise`; `archive work`; `enter place`; `near resident`[w]; `hour between`; `schedule` and `cancel`; `faction.relation` (B); `festival.call`; `story.area.buff`; `character.grant` from `town:ridgemere` under the counters.
- **Gaps:** C8 (npc scope), C9, C5 (Desmond's greet), C12a (packs credit the chapter), SQ2 (the Bank's till).
- **New:** R1–R3, J4 (the new moon, optional).

### Chapter 3: Two Long, One Short (typically 3 real days at 5 wolves, 2.2 at 200)

**Where:**
- **Signal regions**, one box each:
  - the hill above Glasswater, `w_ridgemere_6_8`;
  - the Upper Mere ridge, `w_ridgemere_8_6`;
  - the Glasswater east downs, `w_ridgemere_7_8`;
  - the Alder Mere knoll, `w_ridgemere_5_4`.
- **The road:** the Glasswater camp `w_ridgemere_6_8`; proj-1 at `8_6`.
- **The Watch:** the Watch Office `r2_the_keep` (with `_down` and `_up`), the Watch House, Stanmore House `rm_stanmore_house`.
- **Where the list pieces are:** the Customs House `_up`, the Cathedral `r2_the_cathedral_of_ridgemere` and its crypt `r2_the_cathedral_of_ridgemere_crypt`, the Fish Hall `rm_the_fish_hall`, the Council Press `rm_the_council_press`, the Knucklebones `r2_the_knucklebones`, the Carters' Yard.
- **Out of the city:** the Greyteeth shearing barn `fs_sheep_foothills_barn`, the Records Office, the Lamp and Pick `cn_frosthollow_alehouse`.

**Opening.**
- Every board, signed with a grey paw-mark: *"THE ROADS HAVE A KEEPER NOW. One mark a wagon, paid at the ridge."*
- A nightly `schedule`: on each `hour` from 21 to 4, every undoused signal region shows a `story.ping` "a lamp on the ridge" (60 minutes).
- **Hakon**, a stranger (`visitor.add` id `hakon`, `like: rm_palle_bilgemarsh`, description "older and greyer than he should be, lamp-black on his paws, a grey cloak"), stands on the Upper Mere ridge for 360 minutes, re-added nightly. He is mute. His look-alike is a male arctic preserver on Market Square: Hakon walks only the Greenholt ridges and the Pass and never comes to the city, and he no longer stands as Ulric's double.
- `story.area.spawn` wave 1 at Glasswater (cell `6_8`), switch `spawned_glasswater` on (with L2: `camp: "nh_glasswater"`).

**Shared goal: "Douse the lamps, open the Glasswater road"** (`ch_goal` = 40 + 5 per helper).
- **Douse a lamp.** On each `hour` 21–4, if `count in region` ≥ 1, that ridge's lamp is out for the hour: 2 points per wolf present, at most 3 wolves counted per ridge per hour.
- **Glasswater.** Three waves. Each spawn is `min(6, 1 + wolves in region)`, through `count in region` branches, and turns `spawned_glasswater` on. A trigger on `camp cleared` with `cell` `6_8` and the switch on clears it and calls the next wave; the third scores 10, then `project.post` the **Glasswater relay** (watchpost, town coldwater).
- **Upper Mere post.** proj-1 (`8_6`, Frosthollow) already stands open (§5.2). Gifts score 1; built scores 10.
- **List pieces** score 2 each; side stories score 6.

**Ends on** (§3.2): **30%** of the active wolves, **at least 3**, have done a task, and **12 game days** have passed; or an admin push. Soft maximum **36 game days**.
- **Tasks:** doused a lamp (in a signal region at a lamp hour); fought in a Glasswater wave; took a Watch-list piece from a keeper; gave to proj-1.
- **The close:** the eve of a town festival if one falls within 3 game days (§3.2.4), else the next dusk at least 3 game hours away. The Watch-list destinations close 5 game days after the first whole list, or at the close, whichever is first.
**Visible change.**
- **A dark night:** Coldwater and Larkfield boards read "No lamps on the downs last night."
- **1/3:** Lorn's barn sees the pattern.
- **2/3:** Ulric's harness turns up.
- **Blaze:** C9 `figure.set` villain = "the Grey Captain", title "who keeps the roads now", known in region greenholt. In the Greenholt, `#world.figure_villain#` is now the Captain; elsewhere it stays Ambrose. Realm key `nh_captain=named`. The paw-mark notices are torn down.

**Branches.**

***The Watch lists*** (`nh_b_lists`). Each of the 35 keepers gives each wolf one piece, once (player `residents` list `p_keepers`). Four keepers ask a favour first:
- Treasurer Karin Stanmore `rm_karin_stanmore`: bring her the customs tally (`archive work` at the Customs House `_up`).
- Desmond Dripcot: sit a night watch at the cells (`together` in `r2_the_keep_down` at night).
- Abbot Lionel Ropestack `rm_lionel_ropestack`: attend the Restday service (`sermon` on Restday), then ask in the crypt.
- Usher Desmond Farrow `rmx_desmond_farrow`: carry his note to usher Corra Coldridge `rmx_corra_coldridge` by night (`gift letter`).

The other 31 give a piece for a plain conversation. There are fakes too: Corra Coalcot `rm_corra_coalcot` at the Fish Hall sells "lists", and the chain "Smell the paper" shows they reek of fish oil. A wolf who buys one loses 1 `p_voice`. **Corra Coalcot carries the list hook herself**: her own real piece is in her hood. She never sells it; whoever reports her (side story 1) is handed it by Bettany, and it counts as a favour-keeper's piece.

Three pieces, at least one of them from the four favour-keepers, make the whole list. That gives a journal note and a `paper` item; with J9 it is a tagged story item, "the Watch list".

At 2/3, `story.tell` to participants: "Three Houses' names are on every list. One is on none."
- Grayrock pays the wharf posts.
- Fell pays the North Gate posts.
- Greatly pays no coin, because it holds the Watch Captain's note instead.
- A grey paw-mark pays the Cinder Gate's guard, Palle Plankmore `rm_palle_plankmore`.

Each holder chooses a destination (a chain at the destination; tallied by `p_voice`; it closes 5 game days after the first whole list, or at the chapter's end):

| Destination | What happens (lasting) |
|---|---|
| **Sergeant Bettany Nailley** `rm_bettany_nailley`: "Take this to the magistrate." | The magistrate hears it. Corra Slagmore stands aside: talk, plus C9 `figure.set` captain = Bettany, "who leads the Watch". There is no post change. Grayrock's coin to the wharf posts stops, `nh_threat` −15, and in Ch 7 Bettany's Watch rides with the escorts (one wave fewer). |
| **The Council Press**, Ingrid Ashwell `rm_ingrid_ashwell`: "Print it." | Every board names the Houses that paid (no personal names). `faction.relation` house_fell and house_grayrock with ridgemere_watch go hostile (B). `times.nudge ridgemere −20` for 7 days, "the Watch disgraced". Corra resigns in talk: "in honour" if her note was freed in Ch 2, "in disgrace" otherwise. Camps stop re-forming for 20 days (`nh_threat` −20). |
| **Captain Corra Slagmore**: "This is yours to mend." | She burns it in front of the wolf and swears the Watch to the lamp. If her note was freed, she is redeemed in talk. If not, Greatly still owns her, and in Ch 9 the Watch stands aside while the cudgels gather. |
| **Sold to Ambrose**, through Rune Gullwell: "What's Greatly paying?" | 10 pennies "from the Bank" (the treasury until SQ2, then the Greatly till; under the counters, §2.11), and `p_marks` +1. Greatly owns the Watch: `nh_back_purse` +8, `nh_threat` +10, and Bettany is "on the Harbour Gate now" (talk). |
| (no whole list by the close) | `lost`: Greatly buys the keepers' silence. `nh_back_purse` +5. |

***Desmond Hollis*** (`nh_b_desmond`).
- A wolf with `p_saw_desmond` who picks up the copied wagon board (an `item.place letter` in the Carters' Yard at night) and gives it to Bettany: `held`. That is `npc.move` to `r2_the_keep_down` plus `npc.hold`, re-applied at every later chapter's setup. His chain names the hire: "A tall black arctic out of the north, eyes like a frozen tarn. Pays in Greatly pennies."
- The default at the close: `fled`. He walks out of the North Gate, then `npc.move` to the Greyteeth foothills and `npc.hold`, re-applied each chapter. He has no post, so the vacancy rule is untouched. A fled Desmond leads the river company in Ch 7.

***Lorn Hedgeton*** (`nh_b_lorn`): see side story 3.

**Side storylines.**
0. **Door quest: "Douse a Lamp."** A journal offer on `arrival` at night, or on `night falls`.
1. **The Most Suspected Wolf in Ridgemere** (crown S3.1).
   - **Giver:** Sergeant Dagny Ropeley `rm_dagny_ropeley`. *say:* "#greet#. Old Kendrick's believed a thief by half the city. I've never once caught her near a thing that wasn't hers. Want to prove me right?"
   - **Objectives:**
     1. Follow the scent from three stolen-goods reports (`stolen goods smelt`) to the Pawn and Ledger.
     2. From there, to the true thief, Corra Coalcot (5 recorded thefts).
     3. `reported` with `target rm_corra_coalcot`.
   - **Result:** C8 `belief.plant` "never stole a thing in her life" in the city; J8 `belief.forget` on Ingrid Kendrick's theft beliefs. Ingrid Kendrick `rmx_ingrid_kendrick` thanks the wolf by name at Northgate for a week (C5).
2. **Lamp-Black on the Snow.**
   - **Offer:** a journal offer on entering Frosthollow.
   - **Objectives:**
     1. After a douse on the Upper Mere ridge, a harness turns up (`item.place rope`). Pick it up and give it to Ulric.
     2. Ulric's chain: "That's my brother's scent. Older and smokier, but his. He died in the deep winter. Didn't he?"
     3. `archive work` at the Records Office. Frosthollow's winter record appears (lore `fh-snow-1`).
   - **Ulric:** "There's no Deepvein name on the lamp-oil list. Not one."
   - **Result:** world number `nh_ulric` = 1. Ch 8 completes it.
3. **The Shepherd Who Watches the Road.**
   - **The secret:** Lorn Hedgeton `cty_lorn_hedgeton` sells cart days to the Captain for Greatly pennies.
   - **Objective:** count the lamps from his shearing barn on three nights (`hour` trigger plus `wolf in place fs_sheep_foothills_barn`). That catches him out.
   - **The choice:**
     - **Expose him** on the Coldwater square (`shamed`): Coldwater's talk turns on him.
     - **Turn him** (`turned`): he gives up the code (world switch `nh_code_known`: "two long and one short is a wagon on the road"). He feeds the Captain false days, which adds an ambush wave at Glasswater worth 10. In Ch 7 a wolf can flash false signals.
4. **The Dismissed Guard.** Talk with Desmond Hollis at the Knucklebones ("You smell of the yard. So do I."), watch the Carters' Yard at night, and bring the copied board to Bettany (above).

**The Purse path:** sell the list.

**Lone wolf and crowd.**
- **A lone wolf** on one ridge darkens one lamp, and that night's board says so. Four wolves on four ridges blind the company.
- **The Glasswater waves** are a hard fight for 5 wolves and many fights for 200, never a hopeless one.
- **Surplus** posts the Southmeadow watch post on the `7_12` camp site, and calls **the Road Feast** (`festival.call`) in the town with the most camps cleared nearby.

**Recognition.**
- Deeds: "doused the Captain's lamp on the downs", "broke the Glasswater camp", "carried the Watch list where it belonged".
- Top 3: the title "Ridge-walker".
- The first wolf to bring the list to a destination is named on the boards.

**Failure** at the close:
- lamps still lit: the scar `signals_burning` (more false howls in Ch 4);
- Glasswater not cleared: the scar `glasswater_held` (no relay post; Ch 5 offers the camp again).

**Quest types:** night ridge-holding, a three-wave camp siege, building, gathering a secret document, a name-clearing chase, a stake-out, an informer turned, an inside-wolf caught.

**Talk** (`nh_stage=signals`; `nh_lists=*`; `nh_captain=named`):

| Dialect | Gossip | Helpful knowledge |
|---|---|---|
| plain | "Somebody's gathering the lists of who pays the Watch. Mind whose name you say aloud in a taproom." / press: "It's printed. The whole list, on every board. Half the Watch can't meet your eye." | "They say the abbot keeps a list too, and shares it only after the Restday service." |
| coldwater | "Lamps on the downs, two long and one short. That's no fisher's light, eh?" | "Stand on the ridge where they burn and they won't light it. They don't like being seen, eh?" |
| larkfield | "A Watch you can buy is a scale you can lean on. Scales and silver, I'd like to see that list." | "The camp above Glasswater comes back in threes. Take a friend, and then another, lambkin." |
| southmeadow | "The fish-hall wolf sells papers, and they smell of fish oil. Blight take it, don't pay her a penny." | "Two long and one short means a wagon going out. Get to it before they do, my apple." |
| mirebridge | "In the fens the Watch never came at all, so nobody paid. Boggy sort of honesty, no?" | "Lamps on a ridge want oil. Follow the smell of it uphill, tadpole." |
| saltwell | "A Watch that is paid by a House guards that House's well, not yours, my brother." | "By salt and water, the shepherd on the Greyteeth counts every lamp. Ask him why he counts." |
| oakhaven | "Rot starts under the bark, mm? You don't see it till the tree's down." | "There's a lamp-stone on the Alder Mere knoll now. Sit by it after dark and see who comes, mm?" |
| frosthollow | (gated on `world.figure_villain`) "#world.figure_villain#, they're saying. Knows the dark like a good seam." | "Lamps burn on the Upper Mere ridge after the second bell. Go up with your own lamp out." |

- **Recap:** "Lamps on the ridges, a captain in grey taking a mark a wagon, and the Watch's own lists going round. That's the summer."
- **Window lines:**
  - Bettany: "Six years I've waited for a paper like that. If you find one, you know where I stand."
  - Ulric: "Lamp-black and old snow. I know that scent. I buried it, I thought."
  - Lorn: "I watch the road. Leave it, eh?"
  - Corra Coalcot: "Papers, fresh today. Don't sniff them, they're delicate."

**Triggers.**
- **Existing:** `hour`[w] with `count in region`; `story.ping`; `visitor.add`; `story.area.spawn` with count branches and a story switch; `camp cleared` (a trigger, matched by `cell`); `project.post`; `sermon`; `together`; `archive work`; `stolen goods smelt`; `reported`; `item.place`; `item picked up`; `npc.move`; `npc.hold`; `faction.relation` (B); `times.nudge`; `festival.call`.
- **Gaps:** C9, C8 (with `belief.plant`), C4, C1.
- **New:** J8, J9 (tagged items; fallback `paper`).

### Chapter 4: The Nine Howls (opens with a feast; the Howl Relay when the realm is ready; typically 4 real days at 5 wolves, 2.5 at 200)

**Where:**
- every town square (§5.2);
- the Sunfields stone region by the bee garth, `w_ridgemere_7_12` (the stone stays in the telling: no prop);
- the Saltpan home of Bramble Bogditch, `fs_salt_saltpan_home`;
- Highmere, `w_ridgemere_6_1` (Whitefangs, regional snow);
- Soundings `rm_soundings`;
- the Sump Infirmary `rm_the_sump_infirmary` (door `w_ridgemere_0_0` (249, 250)), where Gorm works, though his work label still says "infirmarian of the abbey";
- Withy Creek `fs_marsh_eels_eelhouse` and its landing;
- the Mirebridge crossroads.

**Opening.**
- **At the opening feast** (the season's town festival if it falls within 3 game days of Ch 4's opening, by §3.3's names; otherwise "the Moot Feast", which Ch 4's setup calls with `festival.call` in every town for the next day, §3.2.4; the `festival` event), a board post on that town's board signed by its Keeper, and the same words in the Keeper's window line (`say[self.id=<keeper> world.mem.nh_stage=howls]`, in her dialect). Ridgemere's: "We howled at the feast, and half the square sang the burying howl. A number of very short funerals and one very long party, as the old page says."
- **The herald, on every board** (signed "the Herald of Ridgemere") and in Brenna's window line: "A Moot needs nine howls. Who remembers how?"
- **Each Keeper's `npc.line`:** "We've a howl for the dead and one for a wedding, and I'd not swear which is which."

**Shared goal: "Nine Howls Found"** (`ch_goal` = 40 + 5 per helper). Each community has a medium quest, **"<Town>'s Gathering Howl"**. Its giver is the Keeper; it is ungated.
- **Three parts of the howl,** from the three oldest placed residents of that town (an authoring list, three fixed `conversation` objectives). The `say` warns: "she'll give you half the first time".
- **The chorus at dusk** on the square, completed by a dusk `hour` trigger. It needs `count in region` ≥ max(1, helpers / 20) wolves who each picked the Keeper's chain "Howl it with me" within that game hour, and **the Keeper present** (`resident in region`).
  - A lone wolf plus the Keeper is a chorus of two. That is the fix for small worlds.
  - With J5, the event is `chorus closed` (`quantity` = howlers), and the Keeper's `npc.howl` joins.
- **Result:** found adds 10 × distance to the town's voice, puts the town in `nh_keepers`, and a plain `story.tell` narration to the region (no `from`) says the town's old howl has been found and names its Keeper.
- **Mirebridge** has no square. Its howl is found through **The Scattered** (side story 5): three Mirebridge-blooded asked, then Skye Eelbank sings the first turn at Withy Creek at dusk.

**Ends on** (§3.2): **30%** of the active wolves, **at least 3**, have done a task, and **12 game days** have passed; or an admin push. Soft maximum **42 game days**.
- **Tasks:** sang in a gathering-howl chorus; heard a part of a howl from an old wolf; found one of the Mirebridge-blooded (The Scattered); hauled the Highmere silver; sat a Restday with Bramble.
- **The big beat is the Howl Relay.** Readiness (or a push) **calls** it rather than closing the chapter: the boards announce it, and it runs on the coming night (from the next dusk at least 3 game hours away), or on a full moon if one falls within 3 game days (§3.2.4).
- **The close:** "the Howling" the next night (below), then the chapter closes at the following dawn. Howls not found by then stay `lost` (the late howl, Failure).
**The Howl Relay** (the chapter's showpiece, a dress rehearsal for the Moot night), on the night it is called (Ends on, above):
- **How it runs:** it starts at whichever posts stand at the two ends of the Line (§5.2). Each next post's window is one game hour (10 real minutes).
- **A post answers if** a wolf stands in its region at the window's close, or its town's howl is found and its Keeper is present.
- **A missed link goes "the long road".** The next post can still take the howl from two steps back, at the cost of half a step.
- **Results:**
  - `nh_b_relay` = posts answered;
  - each town whose post answered gets +5;
  - a post that answered with ≥ 5 wolves is marked "practised" (`nh_posts` entry `<id>-strong`) and carries two steps on the Moot night;
  - every board says how far the howl ran, in pre-written texts by count.
- **After it:** `festival.call` in every town for the next night, named "the Howling".

**The Captain's beat.**
- On relay night his ridge lamps answer with false howls. A Greenholt `story.tell`: "A howl came back off the ridge, two long and one short. It was not ours."
- `bandits.call 2` near wolves standing at Greenholt posts, once per wolf. Twice that with the scar `signals_burning`.

**Branch: Buy Back the Howls** (only if `nh_bought > 0`). Each debtor in `nh_bought_from` is won back in one of three ways:
- **a personal deed for them:** a weekly journal quest, a `gift` plus a `conversation`;
- **Silver Under the Ice:** `nh_b_silver = crown` frees 5;
- **surplus:** every 20 points of `nh_surplus` frees one.

Each one won back: `list remove`, `nh_bought` −1, and that debtor's npc key becomes `nh_debt=sings` (they howl in their town's chorus).

**Side storylines.**
0. **Door quest: "A Part of the Howl."** Ask one old wolf for one part of any town's howl.
1. **The Stones Remember** (Southmeadow, large).
   - **Giver:** Briar Cornby `cty_briar_cornby`, a sly cellar hand who "believes the old standing stones remember things". *say:* "#greet#. The old stone on the Sunfields remembers the last Moot. It'll tell you, if you bring it a windfall first."
   - **Three teasing errands** before the real one, each teaching a Southmeadow word:
     1. "chaff": a sack from the Larkholm threshing, `gift` to her;
     2. "a windfall": `honey` from Aster Cornley's bee garth `fs_bees_south_home`;
     3. "my apple": a `clay_pot` from potter Orrin Appleley `tn_sm_orrin_appleley`.
   - **On the fourth night** (`weather is clear`, `calendar is period night`, `region entered` the stone region): a journal note. Nine names are scratched in the stone, and Mirebridge's has been chiselled out.
   - **Lasting:** Southmeadow key `nh_stone=read`; Mirebridge +6; `nh_names` +1; Briar greets that wolf honestly from now on (C5).
2. **The Well's Roll** (Saltwell). Bramble Bogditch (78, devout) keeps the Guest Roll of every caravan watered at the pool.
   - **Objectives:** `together` with her on three Restdays (`calendar is weekday Restday`). Then howl Saltwell's gathering howl *for her* at the Saltwell chorus, which counts even for a wolf alone.
   - **If she dies first** (possible without C11): Thistle Stonepick sings Saltwell's howl for the dead instead. The voice is still full; the line grieves.
3. **Silver Under the Ice** (party of 2–6, Frosthollow; crown S4.2).
   - **Giver:** Fenwick Plankworth. *say:* "#greet#. The old King's first winter, he sent the Kestrel mine's toll silver to the Sump kitchens by sledge, and the sledge went through the Highmere ice. I know where, eh? I want a charter for the Kestrel seam."
   - **Objectives:**
     1. Bring 4 `rope`.
     2. Reach the Highmere shore region (snow is regional there).
     3. Haul together: `together` ×3 inside the region, with `count in region` ≥ 2.
     4. Bring the silver down the Pass Road: `region entered` the Frosthollow square with the party. If a camp stands in cell `8_6` (the natural camp there is active at the checkpoint), it must be cleared first: a trigger on `camp cleared` with `cell` `8_6` completes a hidden "the road is clear" objective for the party in the region.
   - **Weekly.** The quest is `repeat: weekly` for as long as Ch 4 runs (this is the campaign's own "Silver Under the Ice, again", not a pack quest). Only the first finish of each wolf counts for the choice below; later runs pay the haul and the deed.
   - **The choice:**
     - **Give him the charter** (`crown`): 5 pledged howls bought back, Frosthollow +6.
     - **Deny it** (`greatly`): he sells the location to Greatly, `nh_back_purse` +4.
   - **Kell's line:** "What moves is in the Upper Mere. This just sits there, being silver."
4. **The Princess's Cough** (Ridgemere).
   - **Objectives:** bring `yarrow_poultice` and `herbs` to Gorm Plankgate at the Sump Infirmary `rm_the_sump_infirmary`, then sit with Ottilie (`tended`).
   - **Result:** `nh_b_cough` on. In the Blood ending she can stand in the rain at the Moot.
   - **The Warden seed:** Gorm asks oddly exact questions. A wolf with a Gift (J10; otherwise all wolves, with the text "if you carry a Gift") gets the hidden objective "let the infirmarian look at your paws".
5. **The Scattered** (Mirebridge; any wolf, and the heart of a Mirebridge pledge).
   - **Giver:** cartographer Imogen Nailridge at Soundings, who has an old survey with Mirebridge still on it.
   - **Objectives:** seek out the Mirebridge-blooded:
     - Aspen Marshholm, Thorn Marshditch `tn_mb_thorn_marshditch` and the Larkfield smith `tn_mb_tansy_eelbridge`;
     - Roan Pikewater, Skye Eelbank and Tamsin Eelbank;
     - Oriel Marshditch (an innkeeper: a conversation target only, and this is her one target role across all files, decisions §1.2.4) and Bramble Bogditch;
     - Fell Miremore `tn_mb_fell_miremore`, at the Coldwater looms. Though Mirebridge-blooded and living in Coldwater, he was raised in Ridgemere (`home_region ridgemere`), so **his lines are plain**.
   - **Each one found** adds a line to the Book of Names (`nh_names` +1, Mirebridge +4) and a scrap of the howl.

**Lone wolf and crowd.**
- **A lone wolf with the Keeper** is a chorus. The Well's Roll is written for a lone wolf.
- **At 200,** every square at dusk is a real chorus.
- **Surplus buys "the Long Howl":** `weather.set clear` on every town for the relay night, and the "practised" mark on posts that had a crowd.

**Recognition.**
- Deeds, one phrase per town: "found Coldwater's old gathering howl", and so on; and "ran the first howl along the line".
- The top contributor per community (C3 over each `nh_pledge_<town>`; fallback, the first 3 to finish that town's howl): the title "Howl-friend of <town>" (C4), greeted by that Keeper (C5).

**Failure.**
- A howl not found stays `lost`. **The late howl** is the campaign's own (not a stage-pack quest): in Ch 5–9 the same quest is offered again with "the late howl" text, as a **journal offer** on `region entered` of that town's square, so it costs the Keeper no hub slot and every loss can be won back.
- Fewer than half the posts answering adds the scar `relay_broken`.

**Quest types:** oral history, choruses, a timed relay, a lore hunt with dialect lessons, sitting with the old, a healer errand, diaspora-finding, a party haul on ice, buying back.

**Talk** (`nh_stage=howls`; town key `nh_howl=lost|found`):

| Dialect | Gossip | Helpful knowledge |
|---|---|---|
| plain | "Eight towns howling at dusk and one silent. My grandmother said the ninth was the loudest." | "Want the old howl? Ask the oldest wolf in town, then ask her again. She'll give you half the first time." |
| coldwater | "A gathering howl? We've one for the drowned, one for a catch and one for supper, and I'd not swear which, eh?" | "The loom master knows the old howl. Catch her on the square at dusk and she'll start it if you'll carry it, eh?" |
| larkfield | "Larkfield's howl goes up at the end like a price. Scales and silver, it's the best one." | "The pot hand at the alehouse sings the old Mirebridge turn when he's had two. Buy him the second, lambkin." |
| southmeadow | "Our gathering howl? Blight take it, I've only ever known the harvest one." / found: "A good harvest of a night! Half the vale came out to howl." | "The cellar hand says the old stone on the Sunfields remembers. She says a lot of things, my apple. Some are true." |
| mirebridge | "Ninth howl's gone under, tadpole. Mirebridge got drowned, and the drowned don't howl. Or do they, no?" | "Ask the eel-trappers who came out of Mirebridge. Every one of them has a scrap of the old howl, fen-sure." |
| saltwell | "The old one at the salt pans keeps the well's roll, honoured guest. Every caravan that ever drank is in it." | "Sit with her on Restday, my brother, three times. The well remembers who drew from it, and the howl remembers who sat still." |
| oakhaven | "Our howl's slow and it climbs, like good bark. Come by the square at dusk, mm?" | "The keeper starts it low. Don't rush in. A sapling that rushes falls, mm?" |
| frosthollow | "In Frosthollow the warning howl goes low, so it carries down the shafts. The gathering one's lower still." | "If the mine captain asks for rope, he's after the old King's silver under Highmere. Lamp and pick, go in a pair." |

- **Recap:** "The herald says a Moot needs nine howls, and half the towns sang the burying one at the summer feast. They're finding the old ones now."
- **Window lines:**
  - Skye Eelbank: "#greet#. Mirebridge had a howl that went round the reeds twice before it came back. I'll teach you the first turn, no?"
  - Bramble Bogditch: "Sit, my child. The howl remembers who sat still."
  - Gorm (at the Sump Infirmary): "Hold out your paw. No reason. An infirmarian likes to look."
  - Briar Cornby: "The stone remembers. I remember a windfall I'd like brought, first."

**Triggers.**
- **Existing:** `festival`; `hour`[w] with `count in region` and `resident in region`; `conversation`; `dialogue choice`; `together`; `calendar is`; `weather is`; `tended`; `region entered`; `schedule` (relay windows); `list remove`; `weather.set`; `festival.call`; `bandits.call`; `board.post`; `camp cleared` (a trigger, matched by `cell`); `repeat: weekly`.
- **Gaps:** C8 (town keys), C3, C4, C5, C11 (Bramble).
- **New:** J5 (`howled`, `chorus closed`, `npc.howl`), J4 (the full moon, optional), R1–R3, J3 (`closure: "side"` on the side stories, at most 4), J10 (Gift fact).

### Chapter 5: Letters for an Absent Lord (typically 3.5 real days at 5 wolves, 3 at 200)

**Where:**
- Grayrock Hall `rm_grayrock_hall` (and `_up`);
- the Grayrock Wharf Office `rm_the_harbourmaster_s_office`, with harbourmaster Dagny Rustcot `rm_dagny_rustcot`;
- Lord Grayrock's road (patrol route `rm_grayrock_journey`);
- the Chapel of the Tide `rm_the_chapel_of_the_tide`, the Sailors' Rest `rm_the_sailors_rest`;
- Fellhall `rm_fellhall` (and `_up`), the Fell Distillery `r2_the_fell_distillery`, Timberland Court `rm_mossgrave_court`;
- the Rainward Hills: the granite quarry `w_ridgemere_4_1`, Rainmere `w_ridgemere_3_2` and `3_3`;
- the Oakhaven cut `w_ridgemere_4_4`;
- the Larkfield looms `cn_larkfield_weavers`;
- the Larkdowns;
- at the chapter's end, the Pass House `cn_frosthollow_guest_inn` and the Sump Infirmary `rm_the_sump_infirmary`.

**Opening.**
- Corwin Fell's `npc.line`: "The Council should grant Fell the Rainward Hills before any Moot. Ask anyone who drinks."
- The Oakhaven board: *"The cut is ours by old right. Who will walk the bounds with us?"*
- `story.area.spawn` of bounders, "wolves in Fell green" (the Captain's men in Fell pay), at the `3_3` and `5_4` camp sites, with the switches `spawned_bounders_rainmere` and `spawned_bounders_alder` on (with L2: `camp: "nh_bounders_rainmere"` / `"nh_bounders_alder"`). `item.place timber` stakes at the cut.
- The Ridgemere board: *"Forty letters for a lord who is never home."*

**Shared goal** (`ch_goal` = 50 + 6 per helper), in two strands:
- **Walk the Old Bounds.** Twelve scent-post regions round the Rainward Hills and the cut. `region entered` scores 2 for Oakhaven, once per wolf per post per game day. Pulling a stake (`item picked up timber` in the cut) scores 4. Breaking the bounders scores 10 a camp: a trigger on `camp cleared` with `cell` `3_3` or `5_4` and that camp's switch on (which it clears). A cleared `5_4` also lets proj-2 count as raised again.
- **The Forty Letters**, a fixed race for unique pieces.
  - **Taking a letter:** each of the 40 carriers gives their letter to the first wolf who asks and does them a kindness (`conversation` plus a gift of food, or `trouble solved`). The carrier goes to `nh_carriers_done`, and the wolf gets `item.give letter` with a journal note naming its kind.
  - **Delivering it:** a `gift` of the letter to `rm_aldric_grayrock_` scores 3.
  - **Finding him:** the harbourmaster's chain says which road he took, and a `story.ping` marks his last waystation along `rm_grayrock_journey`.
  - **Letter kinds** (`random` at the take): a petition, a grief, or **a toll receipt**: *"Paid one mark, for the Grayrock wagons to pass the High Road. Signed with a grey paw-mark."*
  - **Dagfinn's own letter** goes by journal offer to the chapter's top `p_voice` wolf (C3; fallback, the first wolf to deliver three). It is a receipt too, with Gunnar's note on its back: *"Keep the roads open for a Moot."*

**Ends on** (§3.2): **30%** of the active wolves, **at least 3**, have done a task, and **18 game days** have passed; or an admin push. Soft maximum **48 game days**.
- **Tasks:** walked a bound post; pulled a stake; took or delivered a letter; broke a bounder camp; finished a Ch 5 side story.
- **The close is Black Cloaks on the Pass Road** (below): the Envoy comes down to the Pass House at the next dawn. The Hills and Lord Grayrock are decided then; "Walk the Envoy Down" carries on into Ch 6.
**Visible change.**
- **1/3.** Aldric reads the first letters at a waystation: a scene for participants nearby, in which he reads Ingrid's aloud.
- **2/3.** Aldric turns his wagon for Ridgemere (`npc.walk` by stages). Lady Maren walks out to the Hall gate for the first time in years (`npc.walk`, `npc.pose`). Every resident with the "never home" hook gets a line about it.
- **Blaze.** Aldric sits in the Antechamber, and the Council is whole. Key `nh_grayrock=home`.

**The Captain's beat.**
- The bounders.
- On Lord Grayrock's road, `bandits.call 2` against a wolf carrying a letter who comes `near resident` Aldric, once per wolf.
- The receipts show the Captain is fed by Grayrock.

**Branches (lasting).**
- **The Toll** (`nh_b_toll`). A wolf holding a receipt chooses:
  - **give it to Brenna to cry** (`cried`):
    - `faction.relation house_grayrock council_of_houses tense` (B);
    - the Captain hits Grayrock wagons next (`bandits.call` on Grayrock escorts);
    - the Sump's talk warms to wolves;
  - **give it to Lady Maren quietly** (`bargained`): the toll stops, and Maren pledges two ships for relief at the ford in Ch 7. The Sump never learns.
- **Lord Grayrock** (`nh_b_grayrock`):
  - `crown`: Dagfinn's letter delivered and the toll bargained, or more petitions delivered than griefs. Grayrock keeps the Wharf open for the Moot and its House vote leans to the lamp.
  - `wharf`: delivered, but the toll was cried. The vote is neutral.
  - `absent`: Dagfinn's letter never delivered. Grayrock's vote is cast by Steward Lavinia Plankwick `rm_lavinia_plankwick` with Greatly, unless Tobias was coached.
- **Gudrun's Letters** (`nh_b_gudrun`; crown's match plus weave's choices).
  - **Found, not offered:** a wolf who enters `rm_fellhall_up` on a Fell errand (Corwin's Wager, or a Fell pack contract) gets a journal offer. It is a sealed letter in Gudrun Fell's paw, proposing a match between Gudrun `rm_gudrun_fell_` and Osric Timberland `rm_osric_mossgrave` that would join vines and iron.
  - **The choices:**
    - **Deliver it to Osric** at Timberland Court (`match`): `faction.relation house_fell house_mossgrave friendly` (B). Osric thanks the wolf in his slow Southmeadow cheer.
    - **Show it to Isolde Fell** `rm_isolde_fell_` (`shown`): Gudrun is shut in the accounts room (`npc.hold` 7 game days; undone). Fell and Timberland go tense.
    - **Burn it and tell Gudrun** (`ash`): she owes the wolf. She writes out how Fell will vote at the Moot (journal note).
- **The Hills** (`nh_b_hills`, at the chapter's end):
  - `oakhaven`: bound points reached 60% of the chapter goal. The Council refuses Fell, and Corwin is humiliated. `faction.relation house_fell council_of_houses tense`. Ulla and Nan speak for the cut, and Oakhaven gets +10.
  - `split`: bounds reached 40% and `gudrun = match`, or Nan bought the common right. Fell gets the south slopes for vines, Timberland its charcoal right, and the quarry stays Greatly's.
  - `fell`: otherwise. Fell takes the hills, and Oakhaven's board turns bitter.
    - Oakhaven's lean at the Moot is set **against whichever candidate Fell's House vote backs**, and its board says so.
    - The scar `hills_lost`.
    - Tamsin Icefell says she'll leave for Ridgemere after the Moot (talk only).
- **The wharf dead** (`nh_b_wharf`, from The Names on the Water). Aldric stands at the Chapel of the Tide while Solveig reads the names (a public scene in the wharf region). Lasting:
  - the Sump's hatred of Grayrock eases;
  - Bettany's lines toward the House change;
  - Grayrock's House vote leans to the lamp if `nh_b_grayrock` is not `absent`.

**Side storylines.**
0. **Door quest: "A Letter for the Lord."** Carry one letter.
1. **Every Week, a Letter** (giver Ingrid Grayrock `rm_ingrid_grayrock_`, 16).
   - Carry this week's letter to her father. His reply is a chain.
   - The wolf chooses what to tell her: the truth ("he's at the Wharf Office most days, ten minutes from home"), or a kind lie ("he reads every one on the road").
   - Her greeting differs ever after (two `say[self.id=rm_ingrid_grayrock_ …]` lines gated on the wolf's choice, N2; C5 `npc.greet` once it exists). After the Moot she names that wolf to her father in her own window: C5 `npc.greet` to that wolf, "I told him it was you who carried them.", never a banner.
2. **News of Larkfield** (giver Lady Maren, Larkfield-raised).
   - Carry her message to Aspen Marshholm at the looms, and bring back a twist of Larkdowns `thyme` (`forage` in the larkdowns, or a `gift thyme`).
   - *thanks:* "#thanks#. It smells of home. I'd forgotten home had a smell."
3. **The Names on the Water** (giver Solveig Tarwell).
   - Gather the names of the wharf drowned from five of the 56 bereaved: Bettany, Ingrid Ashwell, Rune Fogworth, Ivar Pitchwick, young Bertil Grantham `rm_bertil_grantham` and others, through an `in list` trigger.
   - Each reply names the brother's trade, never his name.
   - Bring them to Aldric with the letters.
4. **What Nan Is Saving For** (giver Nan Ashhand `cty_nan_ashhand`, at the quarry).
   - She has saved for years to buy the quarry folk's common right in the Rainward Hills.
   - Help her raise the last of it: 3 `stone` gifts to her, and a word with Celestine Greatly at her salon (`conversation rm_celestine_greatly`).
   - **Bought before the hills are settled:** the `split` threshold falls to 40%, and Nan speaks for the cut. **The hills go to Fell anyway:** she spends it on "a cart to Oakhaven" (talk only; she stays where she lives).

**Optional side stories:**
- **A Berth for Julian** (Julian Greatly `rm_julian_greatly` at the Sailors' Rest): he signs on the Grey Tern "for spring". If Rosalind was turned out, the Bought Crown's heir is an empty chair and Ambrose rules as Protector.
- **Corwin's Wager** (comic): a `table game` at the Fell Distillery. Winning shames him and gives a Fell errand into Fellhall.
- **The Reluctant Heir:** Tobias Grayrock `rm_tobias_grayrock_`, if Aldric is not home 12 game days into the chapter.

**Chapter end: Black Cloaks on the Pass Road.**
- **The Envoy arrives.** A stranger, `visitor.add` id `envoy`, `like: tn_sm_brisk_plumby` (an older female arctic, pot hand at the Vine and Sickle in Southmeadow, where the Envoy's road never goes), description "a silver-muzzled arctic wolf in the Concord's black cloak with silver trim", at the Pass House. She is re-added daily and is mute.
- **Gorm revealed** as the Wardens' Eye in a scene at the Sump Infirmary: "Yes, I wrote to them. By salt and water, someone had to." DM prep before this point: faction `warden_order`, with Gorm a ranked member.
- **"Walk the Envoy Down"** (a story-scope quest bridging into Ch 6). Five stages: the Pass House, the Frosthollow square, the Upper Mere post, the Glasswater relay, the Ridgemere waystation.
  - She moves a stage (`visitor.leave` + `visitor.add`) when a wolf reaches the next stage region.
  - `bandits.call 2` at the escorts at stages 2 and 4.
  - **Unescorted** by the chapter's end, she arrives anyway: `nh_b_envoy = alone`, and the Concord's verdict starts one step colder.

**Lone wolf and crowd.**
- **A lone wolf** walks the bounds over two real days and delivers ten letters. Lord Grayrock thanks *that wolf* at the gate, in front of Maren: the gate scene (`scene.play`, Aldric and Maren each in their own panel; the wolf's name through C1, else "the wolf who walked my road"), never a banner.
- **A crowd** is a ring of scent round the hills by nightfall. Surplus buys **the Long Mark**: Oakhaven's `nh_cut=ours` is kept for good (C8 `keep: true`).

**Recognition.**
- Deeds: "walked the old bounds of the cut", "carried a letter home to Lord Grayrock", "brought a twist of Larkdowns thyme to Grayrock Hall", "walked the Concord's envoy down the Pass Road".
- Top 3: the title "Letter-carrier".

**Failure.** Bounds short gives `fell`. Letters short gives `absent` and the scar `lord_absent`. The Envoy unescorted gives `alone`.

**Quest types:** scent-marking, stake-pulling, a letter race, delivery to a moving resident, a secret courier, a homesick errand, grief-gathering, saving for a common right, escorting a stranger.

**Talk** (`nh_stage=letters`; `nh_hills`; `nh_grayrock=road|home`; `nh_wardens=coming`):

| Dialect | Gossip | Helpful knowledge |
|---|---|---|
| plain | "Someone's carrying every letter for the Grayrock lord out to his road at last. Forty of them." / home: "The Grayrock lady walked out to the Wharf gate. Didn't even put her hood up." | "Want the Grayrock lord? Ask at the Wharf Office which road he took. He's always on one." |
| coldwater | "Bounders in green on the Oakhaven road, eh? Fishers don't drive stakes into the ground." | "If you're walking the bounds, mark the alders too. The old line runs along the water, eh?" |
| larkfield | "Fell wants the hills. Fell wants everything with a price on it, lambkin." | "The Grayrock lady's a Larkfield wolf, did you know? Bring her something from the downs and see her face." |
| southmeadow | "A lord who's never home is like a vine never pruned. All runners, no grapes, my apple." | "Thyme grows on the Larkdowns swells where the sheep don't crop. A windfall, if you know where." |
| mirebridge | "Forty letters and not one answered. In the fens we'd call that a very wet postbag, no?" | "Black cloaks on the Pass Road, they say. Fen-sure they'll want walking down." |
| saltwell | "The well remembers who drew from it. The lord has drawn for six years and never come back to the well." | "A letter carried is a debt paid, my sister. Do the carrier a kindness first, and they will give it gladly." |
| oakhaven | "Walk the bounds with us? Mark every ninth cedar and the old boundary stone, mm?" / upheld: "Good bark, those wolves. The cut's ours yet. Root well." | "Pull the green stakes and bring them to the square. Old Oak, they burn well enough." |
| frosthollow | "Three black cloaks came down the Pass on Stoneday. Didn't take their boots off at the Pass House. Deep strange." | "The cloaks want walking down to the city. Go in a pair, whelp. The Captain's men watch the High Road." |

- **Recap:** "A Moot called, debts fought, lamps put out, the old howls found, and now forty letters chasing a lord who is never home. And black cloaks on the Pass."
- **Window lines:**
  - Aldric (`home`): "Forty letters. My daughter's were the heaviest. I read them all on the road."
  - Maren: "I've not been to the gate in years. Is the city still the same out there?"
  - Ingrid: "Did he read it? You can tell me if he didn't."
  - Corwin: "The hills want vines, not quarry dust. Any fool can see it."
  - Gudrun: "Isolde never lets a debt go. Neither do I. Whose errand are you on?"
  - Nan: "Saving for something. Maybe it's this."

**Triggers.**
- **Existing:** `region entered`; `item picked up`[w]; `gift` letter to a resident; `conversation`; `trouble solved`; `random`; `story.ping`; `npc.walk`; `npc.pose`; `npc.hold`; `faction.relation` (B); `scene.play`; `visitor.add` / `visitor.leave`; `bandits.call`; `story.area.spawn` with story switches; `camp cleared` (a trigger, matched by `cell`); `forage`; `table game`; `enter place`.
- **Gaps:** C3, C5, C8 (`keep`), C9, C1.
- **New:** J9 (tagged letters).
- **DM prep:** `warden_order`.

### Chapter 6: The Lights in the Fen (fog in any season; typically 4 real days at 5 wolves, 3 at 200)

**Where:**
- the Sallow Fens: `w_ridgemere_3_10`, `4_10`, `4_11`;
- the Fenshore boardwalk `w_ridgemere_3_13` (92, 148);
- Withy Creek `fs_marsh_eels_eelhouse` and `fs_marsh_eels_home`;
- Rivermouth, Sloe Mistwick's home `fs_fishing_delta_home` and shed `fs_fishing_delta_shed`;
- the Mirebridge crossroads `w_ridgemere_5_11` (188, 52);
- the camps at `6_12` and `7_12`;
- the Fenshore pottery `cn_fenshore_pottery`;
- the Records Office rolls `r2_the_records_office_top`, the Chapel of the Tide, Thornham House `rm_thornham_house`;
- the Antechamber and the four House seats.

**Opening.**
- **The fog.** `weather.set fog` on Fenshore by night, 72 h at a time and re-set.
- **The lights.** `story.ping` "a light in the reeds", moving hourly at night. These are the Captain's float lamps.
- **Tamsin Eelbank's `npc.line`:** "The drowned are walking. They want something said."
- **The Envoy's Question,** a public scene in the Antechamber. Gorm speaks for her, as the Concord's custom requires: "She will not speak to you herself. By the Concord's custom an envoy speaks to a realm only through its own Eye. She asks this: on the night your Moot sits, let one howl run from the Pass to the Waste, every town answering the one before. Then she will write that you keep yourselves." Realm key `nh_wardens=question`.
- **Dagfinn walks to the fens** (`npc.walk`, over two game days). His arrival at the Heron is a scene.
- **The Houses fall out** ("before winter", as 38 residents always said). The quarrel's shape comes from the earlier branches:

| Earlier branches | The quarrel | What it looks like |
|---|---|---|
| `nh_b_gudrun = shown` | Fell against Timberland | Fire at the Timberland Ironworks (`event.add ridgemere works_fire`); charcoal wagons held on the Cinder Road; `times.nudge` down |
| `nh_b_gudrun = match` and `nh_b_ledger = shown` | Fell and Timberland against Greatly | A run on the Greatly Bank (`times.nudge ridgemere`); masons walk off; `harvest.override stone poor 14` |
| `nh_b_ledger = kept` and (`nh_b_lists = press` or `nh_b_toll = cried`) | Greatly against Grayrock | Grayrock closes the Wharf to Greatly stone; `ship.storm` news; the Wharf idles |
| otherwise | All against the Crown | Council petitions to unseat the lamp-keeper before the Moot; extra guards walked to the Palace (talk and `npc.walk`) |

**Shared goal: "Say the Names, Lay the Lights"** (`ch_goal` = 50 + 6 per helper):
- **The Book of Names.** `nh_names` has a fixed floor of 30; the pieces are unique. Each name scores 3 × 2 (Mirebridge). Sources:
  - each Mirebridge-blooded resident gives names once (world list);
  - the Records Office rolls (`archive work`, one per wolf per day, 10 in all; these include the "still-missing" pups);
  - Solveig's sea-drowned lists at the Chapel of the Tide (`conversation` plus a `gift lamp_oil`; 5 in all);
  - walking the path with no note (side story 1): 3 per wolf who finishes.
- **The false lights.** 3–5 `oil_lamp` are placed on floats in the fen cells each night (`item.place`). Each `item picked up` scores 2.
- **The southern camps** at cells `6_12` and `7_12`: the chapter spawns the southern company there (switches `spawned_south_mire` and `spawned_south_sunfields` on; with L2 `nh_south_mire` / `nh_south_sunfields`). A trigger on `camp cleared` with that `cell` and switch on scores 10 each and clears the switch.
- **The Mirebridge crossroads post.** `project.post` a watchpost, town fenshore, at `5_11`: timber 10, stone 4.
- **Keep the Peace** (the city strand):
  - **truce tokens** in Saltwell's custom, `bread` plus `salt`, to each House steward: Edric Kelpham `rm_edric_kelpham`, Millicent Rustwell `rm_millicent_rustwell`, Lavinia Plankwick, Hollis Nailwick `rm_hollis_nailwick`;
  - **guard House wagons:** `caravan arrives`, or `contract done` with C12b `starts`;
  - **break the cudgel bands** in House colours at `2_3` and `3_3` (`story.area.spawn`, switches `spawned_cudgels_west` and `spawned_cudgels_east`; scored by a `camp cleared` trigger on that `cell` with the switch on).

**Ends on** (§3.2): **30%** of the active wolves, **at least 3**, have done a task, and **18 game days** have passed; or an admin push. Soft maximum **48 game days**.
- **Tasks:** gave a name to the Book; picked up a false light; walked the path with no note; fought a southern camp or a cudgel band; carried truce tokens; gave to the Mirebridge post.
- **The beat inside:** the Rite of the Tide, at the next dusk once the Book reaches its floor of 30 names (a content gate, as before, not readiness).
- **The close:** the next dusk at least 3 game hours after readiness or a push. A rite already called plays first; with no rite by the close, the lights are `faded`.
**Visible change.**
- **1/3.** The fen boards read "Fewer lights on the reeds last night."
- **2/3.** Solveig sets out for the crossroads.
- **Blaze, the Rite of the Tide.**
  - Solveig walks to the crossroads (`npc.walk` with a cue). At dusk, with a wolf in the region, a public scene plays: she says the names, with a `response` beat for the wolves, and Skye Eelbank stands in the cast.
  - With the Book at its floor, Skye is named **Mirebridge's Keeper**, and Mirebridge pledges move to her.
  - The post at the crossroads is lit (`story.ping`).

**The Captain's beat.**
- The southern company moves through the fog by float lamps. Sloe Mistwick's boats carry its plunder.
- The Ridgemere board gets a grey paw-mark notice: *"Any House that wants its wagons to pass, ask at the ridge."*

**Branches (lasting).**
- **The lights** (`nh_b_lights`), tallied:
  - **Laid** (the default if any wolf attended the rite):
    - the float lamps stop, and the fog lifts (`weather.set clear` on Fenshore, 24 h);
    - Tamsin blesses the realm, and Fenshore's mark −2;
    - Dagfinn's grief scene: "I knew every street of it, and which flooded first";
    - Lamp backing +3.
  - **Turned**: wolves carry picked-up float lamps onto the "do not" paths of `mb-fens-1` (regions), luring the company's boats into the bog.
    - The southern company drowns its plunder: Ch 7's river wave is halved.
    - But Fenshore's mark +3, and Tamsin's lines turn bitter for good.
  - **Faded** (no rite by the chapter's end): the scar `lights_faded`. Mirebridge stays silent unless raised later; at the Moot the Council speaks for it.
- **The quarrel** (`nh_b_quarrel`, `nh_b_peace`):
  - **Peace kept** (the Keep the Peace strand reached its third of the goal): Celestine Greatly's salon stages a House wedding (`event.add ridgemere house_wedding`), and no House is broken.
  - **Peace failed:** one House's head is shamed. That House's `faction.relation` with the Council goes hostile (lasting), its vote at the Moot goes *against* the others' majority, and the scar `peace_broken`.

**Side storylines.**
0. **Door quest: "A Name for the Book."** One name, from any source.
1. **The Path With No Note** (Skye, then Tamsin Eelbank; lore `mb-fens-1`).
   - Follow the unnoted path in fog: `weather is fog` and five path regions in order, at night.
   - Stepping into a "do not" region costs a wound (`character.injury`, severity 1, W, `ifRefused: skip`).
   - At the drowned causeway lies the Captain's cache of robbed goods (`item.place`): `wool`, `cart_wheel`, `honey`, `salt`, `wine` and `iron_bar`.
   - Return each to its owner (`gift` to named residents): Cairn Oakhaven, Willa Coldwick, Aster Cornley, Thistle Stonepick, Linden Harvestton `tn_sm_linden_harvestton` and Wenna Timberland `rm_wenna_mossgrave`.
   - Every owner thanks that wolf by name (C5), and goes in `nh_owners_returned`.
2. **The King's Mother's Door-Stone** (Dagfinn; wild).
   - Find the carved threshold stone at the crossroads (an `item.place stone`, described in the journal).
   - Carry it to Thornham House and give it to Dagfinn.
   - His line ever after: "#greet#. You're the one who brought my mother's stone up out of the mire."
3. **The Potter's Post** (Fenshore).
   - Ludo Beechshaw `tn_oh_ludo_beechshaw` took the dead potter Iris Greywater's post, and the boardwalks whisper.
   - Objectives: the Records Office (`archive work`), the pottery `cn_fenshore_pottery` (`enter place`), talks with Roan Pikewater and the Fenshore carter Jory Corncroft `cty_jory_corncroft`, who carts the boardwalk's goods. (Nell Plummeadow is an innkeeper; her one target role is Nell's letter in Ch 9.)
   - The records say only that she died and her post fell vacant; nothing says he wished it. **The cause of death is never stated.**
   - Ludo is cleared and becomes a friend. He "carves birds and gives them away": a journal note.
4. **Sloe's Cargo** (Rivermouth).
   - **Turn her:** settle her well feud (`trouble solved`, item feud, near Rivermouth). Her boats bring grain upriver in Ch 7 and carry a copy of the Book to the city (+5 names).
   - **Expose her** to Bettany: `door.lock` on her shed (undone at the run's end), `npc.hold` 3 days, `times.nudge fenshore −20` for 14 days.
5. **The Quickened Wolf's Question.** Offered to wolves with a Gift (J10); without J10, to all, with "if you carry a Gift" in the text.
   - At the Sump Infirmary, Gorm asks for the Envoy.
   - **Register:** a friend's `vouched to the wardens` at Gorm. The deed "stood before the Wardens unafraid", `nh_quick_registered`, and the Concord's trust +1.
   - **Hide:** `carries masking_oil` and stay out of the Envoy's regions for 3 game days. `nh_quick_hidden`: a hidden Quickened wolf can teach Tansy in Ch 8, at a risk.

**Lone wolf and crowd.**
- **A lone wolf** following the lights through the fog alone is the chapter's purest moment: one wolf, one light, the path with no note.
- **A crowd** makes a lantern-line across the fen. With `count in region` along the path, the pings come every 10 minutes instead of every hour.
- **Surplus buys "the Long Remembrance":** Solveig reads the extra names at the Chapel of the Tide on the next Restday (`npc.walk`, scene).

**Recognition.**
- Deeds: "said the drowned names at Mirebridge", "walked the path with no note", "carried bread and salt to a House at odds".
- Top 3: the title "Name-sayer".
- Solveig names the wolf who brought the most names.

**Failure:** `lights_faded`, the post unbuilt, `peace_broken`. None of them blocks.

**Quest types:** night tracking in fog, records, diaspora talk, a rite, returning a cache, an innocence chain, a smuggler intrigue, truce-carrying, wagon guard, a Warden choice.

**Talk** (`nh_stage=lights`; `nh_lights=walking|laid|turned`; `nh_quarrel=*`; `nh_wardens=question`). Lines marked ⁽ʷ⁾ carry a `here.weather=fog` gate or are rephrased as hearsay if `talk_lint` objects.

| Dialect | Gossip | Helpful knowledge |
|---|---|---|
| plain | "The Timberland works burned in the night. Every smith in the Cinders swears it wasn't them." / against the Crown: "The Council wants the lamp-keeper gone before the Moot. Petitions on every desk." | "A House at odds still eats. Bring its steward bread and salt. It's an old custom and nobody refuses it." |
| coldwater | "Houses at each other in the city and lights in the fens. A good season to mend nets, eh?" | "The fen lights smell of lamp oil, they say. No drowned wolf ever carried a lamp, eh?" |
| larkfield | "A black cloak on the Fen Road, and the price of a room on the Long River just doubled." | "The fen boats come up past the ford with no lamps lit. Sold goods, lambkin, and not theirs to sell." |
| southmeadow | "Fen boats on the Long River at night, no lamps lit. Blight take them, my cousin lost a cart of honey." | "The stone's names are nine, and one chiselled out. Take that to the fen priest, my apple." |
| mirebridge | ⁽ʷ⁾ "Dry feet. Mizzle's up. If you see a light, follow it, but only the cold ones, no?" / laid: "Dark marsh and dry feet. The priest from the city said every name." / turned: "They used the drowned for bait. Dark One's mire, we'll pay for that." | "The path with no note, tadpole: start at Withy Creek, keep the reeds on your left, and never step where it shines warm." |
| saltwell | "In the old way, my sister, you bring bread and salt to a House at war. Even a House needs to eat." | "The infirmarian down in the Sump writes to the black cloaks. If a friend carries a Gift, speak for them there, honoured guest." |
| oakhaven | "The Houses are windthrown in a row this autumn, mm? One falls and takes the next." | "The quarrel's carts go by the East Road. Ride with one and nobody touches it, mm?" |
| frosthollow | "Lights in the fens, fire in the city. Black seam of a season." | "The black cloak's healer asks after the Gifted. Say what's true and no more, whelp." |

- **Recap:** "Now the Houses are at each other, the fens are full of lights, and a black cloak wants one howl from the Pass to the Waste on the night the Moot sits."
- **Window lines:**
  - Tamsin Eelbank: "Never sit with your back to the reeds. The drowned are walking, and they want something said."
  - Skye: "Fen-sure I'll read it at the crossing. Somebody's got to say them out loud."
  - Solveig: "Bring me a name and I'll say it over the water. That's all a rite is."
  - Dagfinn, in the fens: "I knew every street of it, and which flooded first. There's reeds now where my mother's door was, no?"
  - Gorm: "I am the realm's Eye, and I'm sorry for it. Ask your question; I'll carry it."
  - Celestine (Saltwell-raised): "My salon is open to every House, honoured guest. Especially the ones at odds."

**Triggers.**
- **Existing:** `weather.set`; `weather is`; `story.ping`; `npc.walk` with a cue → `scene arrived`; `scene.play` with a `response` beat; `region entered` chains; `character.injury` (W); `item.place`; `item picked up`; `archive work`; `gift` to listed residents; `trouble solved`; `door.lock`; `npc.hold`; `times.nudge`; `event.add`; `harvest.override`; `ship.storm`; `faction.relation` (B); `vouched to the wardens` / `told the wardens` (once `warden_order` exists); `project.post`; `caravan arrives`; `carries`; `story.area.spawn` with story switches; `camp cleared` (a trigger, matched by `cell`).
- **Gaps:** C5, C8, C12b (`contract done` … `starts`).
- **New:** J10, J3.

### Chapter 7: Harvest Under Arms (the siege when the realm has mustered; typically 2.7 real days at 5 or 200 wolves)

**Where:**
- **The three siege regions:**
  - the Larkfield ford and square, `w_ridgemere_6_9` (140, 76);
  - the yard of the Longmead granary `fs_grain_long_granary` (its door opens on `w_ridgemere_7_9` (183, 133));
  - the Downs Road south of Larkfield, `w_ridgemere_7_11`.
- **Elsewhere:**
  - the Southmeadow and Saltwell squares (the Night Runner);
  - the Larkfield smithy `cn_larkfield_smithy`;
  - the Magistrate's Court `rm_the_magistrate_s_court`, with Pillory Square at about world (412, 311);
  - Rivermouth;
  - the Wharf.

**Opening.**
- **The boards.** Larkfield, Southmeadow and Saltwell: *"The Captain's companies gather for the ford. Every wolf who can stand, muster at the ford."* At the ford, a grey paw-mark notice: *"Whoever holds the ford holds the winter."*
- **The Envoy, through Gorm** (Gorm's `npc.line`, and his window line `say[self.id=rm_gorm_plankgate world.mem.nh_stage=harvest]`): "She asks whether a realm that cannot hold its own ford can keep its own pups."
- **If Desmond fled,** he is `npc.move`d to the Downs Road camp region and held there. He leads the river company; with C10 `foe.outlaw` he can be downed, never killed.

**Shared goal: "Hold the Ford."** A story-scope quest, **Three Nights at the Ford**: fixed goal 3, `contributors` min(3, helpers). Beside it runs the realm goal of 40 + 6 per helper in harvest deeds.
- **Each of the three siege nights** (from the call; on the Last Sheaf's eve, the festival and the night after when it falls there) has three waves, at 21:00, 0:00 and 3:00, in all three regions.
- **Each wave** spawns `min(6, 1 + wolves in region)`, through `count in region` branches, and turns that region's switch on (`spawned_ford`, `spawned_granary`, `spawned_downs`; with L2 `nh_ford` / `nh_granary` / `nh_downs`). A region **holds** a wave if a trigger on `camp cleared` with its `cell` (`6_9`, `7_9`, `7_11`) and its switch on fires before the next wave hour; the trigger clears the switch. A region whose switch is still on at the next wave hour, or with no wolf in it at the wave hour, is breached. The three regions lie in three different cells (the ford `6_9`; the granary yard outside its door in `7_9`, at (183, 133); the Downs Road `7_11`), so `cell` plus switch tells them apart until L2's names replace the rule.
- **A night is held** if at most one region was breached, or at most two with relief.
- **Relief, each lasting one wave a night:**
  - Grayrock's ships (`nh_b_toll = bargained` or `nh_b_grayrock = crown`): bread to the granary, so the granary has one wave fewer;
  - Sloe's boats (`turned`): no river wave on night 2;
  - Bettany's Watch (`nh_b_lists = bettany`): one wave fewer at the ford;
  - the lamp code (`nh_code_known`), through the Smith's Bog Path: one wave walks into the shallows;
  - `nh_b_lights = turned`: the river waves are halved;
  - `nh_b_ford` needs ≥ 2 nights held.
- **Mend the ford bridge.** `project.post mend` (larkfield: stone 6, timber 6, limestone 4, cord 2). Gifts score 1, built scores 10. A mended ford spawns one bandit fewer per wave.
- **Bring the harvest in.** `bread`, `flour` or `oats` given to Rook Wheatfield at Longmead.
- **Tend the wounded.** `tended` in Larkfield scores 2.

**Ends on** (§3.2): the siege is called when **25%** of the active wolves, **at least 2**, have **mustered for the ford**, and **6 game days** have passed; or by an admin push. Soft maximum for the call **30 game days**.
- **Tasks (mustering):** in a siege region after dark (an `hour` 21–4 with `wolf in region`); gave to the ford mend; brought grain to Rook; tended in Larkfield.
- **The call:** every board in Larkfield, Southmeadow and Saltwell reads *"The companies are on the Downs Road. The siege begins at dusk."* The three siege nights start at dusk the next game day. If Larkfield's festival (the Last Sheaf) falls in those nights or within 3 game days, the siege waits for it and the texts name it; otherwise they say "the siege nights" (§3.2.4).
- **The close:** from the aftermath phase, whose `ready` block is only `minDays` 6 (`since: "phase"`), so it closes 6 game days after the third night (the hearing, the relief, the Question's interim tally), or at once on Advance there. Advance during the muster calls the siege; the siege nights themselves have no Advance (Go to phase… only).
**The lone wolf's jobs.**
- **The Night Runner.** On night 1, a wolf at the ford picks Aspen's chain "I'll run the warning". A timer starts. They must reach the Southmeadow square, then the Saltwell square, each within one game hour (10 real minutes). With J5, they howl at each.
  - Result: both towns "come". There is one wave fewer on nights 2 and 3, and Southmeadow and Saltwell get +10 each.
  - One fast wolf brings two towns.
- **The Granary Door.** Keep the granary yard occupied through a whole wave. A `region emptied`[w] fails it. It scores 6.

**The parley** (night 2, at 22:00).
- A truce lamp burns on the ford's far bank (`story.ping`). The Captain shows himself: a stranger, `visitor.add` id `captain`, `like: cty_stone_sunton`, description "a tall black arctic wolf with pale, nearly silver eyes, lamp-black on his paws, a grey cloak". He is mute.
- **His words come through his speaker** in a scene: Desmond Hollis if he fled. If Desmond is held, the speaker is Rook Wheatfield, reading the Captain's letter, which came by a boy on the Downs Road.
- **What he says:**
  - he is Eskar of Deepvein;
  - Frosthollow's mine house would not open to Deepvein's miners in the deep winter;
  - he is saving to buy Deepvein back, and Greatly has promised him its deed after the Moot if the roads stay shut.
- **The scene choice** (`mode: first`, for the first wolf in the truce region):
  - **Offer terms**: `offered`, if a key exists: `nh_ulric ≥ 1`, or a ledger copy exists (`nh_b_ledger` ≠ none), or `nh_b_seam = honest`. His answer: "Then say it in front of Frosthollow, and show me Greatly's word in writing." With no key he laughs, "With what?", and it counts as `refused`.
  - **Refuse**: `refused`.
  - **Strike under the truce**: `broken`. His lieutenant falls at once (one wave fewer on night 3). But the gossip turns ("truce-breakers"), that wolf's last road deed is revoked (`deed.revoke`), and the scar `truce_broken`.

**Branches (lasting).**
- **Ford held** (`held`):
  - `harvest.override grain good` 30 days;
  - Larkfield +10, `nh_threat` −20;
  - the Captain retreats north, his pride hurt;
  - the Larkfield festival is renamed for a night, `festival.call larkfield "the Held Ford" inDays 1`.
- **Ford burned** (`burned`):
  - `harvest.override grain poor` 30 days, and `times.nudge larkfield −30` for 14 days;
  - the scar `ford_burned`;
  - Ch 8 adds "Feed Larkfield" (food gifts to Larkfield residents);
  - the Captain recruits: his Ch 9 band +2.
- **Desmond's hearing** (if he is held). A scene at the Magistrate's Court, with a `majority` choice among the wolves present:
  - **mercy** (`mercy`): he joins the granary guard on night 3;
  - **the pillory** (`pillory`): `npc.move` to the Pillory Square region and `npc.hold` for one game day. The Sump cheers, and his kin go cold.

**Side storylines.**
0. **Door quest: "A Night at the Ford."** Stand through one wave.
1. **The Night Runner** (above).
2. **The Smith's Bog Path.**
   - The Larkfield smith `tn_mb_tansy_eelbridge` knows "a path across any bog". She shows a flank path through the reed beds by the ford: three regions.
   - With `nh_code_known`, a wolf at the path's end at a wave hour picks her chain "Flash two long and one short". The river company walks into the shallows, and that night has one wave fewer.
3. **Rook's Word.**
   - Rook Wheatfield "has a sweetheart in Southmeadow". Carry his word across the besieged Downs Road to the Southmeadow alehouse, where Rowan Downridge `tn_lf_rowan_downridge` knows who it is for. She is a receiver only.
   - Rook's line afterwards: "She'll know it's from me. That's all I wanted."
4. **A Granary Guard** (if `mercy`). Desmond asks one wolf to stand with him at the granary through the third night (both in the region through a wave). That makes `nh_b_desmond = redeemed`.

**The Purse path: "Siege Price."** Carry a sack from Longmead to the Counting House (a `gift` of `oats` or `flour` to Ivar Pitchwick). It gives `p_marks` +1, `nh_back_purse` +1, and the deed "sold Longmead's grain at siege price".

**Dagfinn's Question** at this point. The boards print the interim tally, in three pre-written variants: "The lamp-keeper asks every wolf: should he stand? So far the realm says stand / says keep the lamp / is split down the middle."

**Lone wolf and crowd.**
- **5 wolves** hold one region a wave. The Runner and the Granary Door are their heroics.
- **200 wolves** fight in every region, every wave.
- **Surplus buys relief convoys.** Escorted wagons to Larkfield (`caravan arrives`) each feed one Larkfield household. A crowd that holds the ford also feeds it.

**Recognition.**
- Deeds: "held the Larkfield ford through the siege nights" ("…on the Last Sheaf" when it fell there), "ran the warning howl from the ford", "kept the granary door".
- Top 3 by `p_ch`: the title "Ford-holder".
- The Larkfield board posts how many wolves held the ford (C1). The fallback is pre-written bands: "a handful of wolves", "dozens of wolves", "hundreds of wolves".

**Failure:** `burned`. The campaign goes on.

**Quest types:** a three-night siege, a timed run, holding a door, a bridge mend, a harvest haul, healing, a parley, a flank path, a courier through a siege, a trial.

**Talk** (`nh_stage=harvest`; `nh_ford=siege|held|burned`; `nh_captain=parley`):

| Dialect | Gossip | Helpful knowledge |
|---|---|---|
| larkfield | "Every sack in Longmead's gone to the ford walls. Scales and silver, I never thought I'd see grain for a rampart." / burned: "Short-weighed, all of it. The winter, the bread, the lot." | "They come up the shallows below the mill. The current's slack there after dark, lambkin." |
| southmeadow | "The runner came at midnight, howling the new howl. We knew it! Sheaf and sickle, we went." | "If the Downs Road's held, go round by the vine lanes. Blight take the Captain, he doesn't know them." |
| saltwell | "Salt and water for the wolves at the ford, my brother. The well gives to those who stand." | "A runner from the ford wants the caravan arc, honoured guest. It is the shortest way to the pool." |
| plain | "They held the Larkfield ford three nights. Blood and bread, I'd have liked to see it." | "The Wharf says two Grayrock ships are loading bread for the ford. Somebody bargained well." |
| mirebridge | (if Sloe turned) "Fen boats bringing grain upriver now, no? Not plunder. That's new." | "The Larkfield smith knows a path across any bog. Ask her before the river company does, tadpole." |
| coldwater | "The Captain's at the ford, eh? Then the downs are quiet. Fair catch, for once." | "Bring blankets to the ford. Wolves who stand all night want warm backs, eh?" |
| oakhaven | "Old Oak, a captain who parleys. That's a wolf with a reason, mm?" | "A truce lamp's a truce lamp. Strike under one and the whole Sound will know by morning, mm?" |
| frosthollow | "The Captain's a Deepvein wolf. Lamp and pick, I could have told you. Nobody shared oil with Deepvein." | "If he names the mine house, ask the mine captain what he barred, whelp." |

- **Recap:** "Now the Captain's at the Larkfield ford with every wolf he has, and the harvest with him."
- **Window lines:**
  - Aspen: "I've never sold a sack I couldn't count. Count wolves for me tonight."
  - Rook: "A word to Southmeadow, if you're going."
  - The Larkfield smith: "A path across any bog, they say. They're right. Want it?"
  - Desmond, held: "Speak for me and I'll stand at the granary. Don't, and I'll not blame you."

**Triggers.**
- **Existing:** `story.area.spawn` per region with `count in region` branches and story switches; `camp cleared` (a trigger, matched by `cell`); `hour`[w]; `region emptied`[w]; `region entered`; `start timer` / `since`; `harvest.override`; `times.nudge`; `tended`; `project gift` / `project built`; `deed.revoke`; `scene.play` (`first` and `majority`); `visitor.add`; `npc.move`; `npc.hold`; `festival.call`; `caravan arrives`; `story.ping`.
- **Gaps:** C10 (`foe.add` for the Captain's band; `foe.outlaw` for Desmond), C1, C3.
- **New:** J5 (the Runner's howl), R1–R3 (two working phases).

### Chapter 8: The Bell and the Writ (the freeze, real or early; typically 5 real days at 5 wolves, 4 at 200)

Two threads run side by side and feed one chapter goal (`ch_goal` = 50 + 6 per helper). The city thread is for wolves who like intrigue; the north thread is for wolves who like the wild. Most wolves will do some of both.

**Where:**
- **City:** the City Office `rm_the_city_office`, the Records Office rolls `r2_the_records_office_top`, the cathedral crypt `r2_the_cathedral_of_ridgemere_crypt`, the Greatly Bank flat `rm_the_greatly_bank_flat`, the Bilge `rm_the_bilge`, the Knucklebones `r2_the_knucklebones`, the King's Residence, the cells.
- **North:**
  - the Upper Mere: `w_ridgemere_8_5`, `9_5`, `10_5`, `8_6`, `9_6`, `10_6`;
  - the Upper Mere Boat Shed `fs_fishing_uppermere_shed`, the Glasswater landing `fs_fishing_glasswater_shed`;
  - the Reedmere home `fs_fishing_glasswater_home`;
  - the Lamp and Pick, the Mine Captain's House, the mine `w_ridgemere_9_2`.

**Opening.**
- **Kell Greyton's `npc.line`:** "It's moving again. I heard it ring under my paws."
- **The Coldwater board:** *"THE MERE IS SKINNING OVER. Wolves with ropes and strong backs, to the Upper Mere when it freezes."*
- **The freeze** (§3.2.4). In winter the mere freezes by itself. Otherwise the setup forces an early freeze: `weather.set snow` on Coldwater and the six ice regions, 72 hours at a time, re-set while the bell thread runs. Either way the world switch `nh_mere_frozen` goes on.
- **In the city:** C8 realm key `nh_writ=whispered`.
- **The Envoy** is seen on the Coldwater square by day (re-added daily).
- **The Captain** moves north: the chapter spawns his band at the `8_6` camp site (switch `spawned_upper_mere` on; with L2 `nh_upper_mere`; `nh_threat` +10 if the ford burned). While the switch is on, the Upper Mere post (proj-1) does not count as raised (§5.2); a `camp cleared` trigger on `cell` `8_6` with the switch on scores 10 and clears it. Hakon appears on the Pass Road at night. The Frosthollow board gets a grey paw-mark notice: *"On the Moot night the Pass is mine."*

**Ends on** (§3.2): **30%** of the active wolves, **at least 3**, have done a task, and **24 game days** have passed; or an admin push. Soft maximum **60 game days**.
- **Tasks:** heard a testimony; searched a writ site; took a bearing on the ice; gave the frame makings or held a rope at a haul; gave Tansy a lesson; the Lamp-Oil List; the Frosthollow Count.
- **The beats inside:** the search opens at 23 memories (a race, as before); **the haul** runs three dusks from the next dusk once six bearings are placed and the frame is given.
- **The close:** the next dusk at least 3 game hours after readiness or a push, but never while a haul is under way: the `ready` block has a part "no haul under way" (`haul_running` off), and after an Advance anyway the closing phase holds its set piece until the last haul dusk. Dagfinn's Question is decided at the close. A bell with no haul begun by the close is `lost`.
#### The city thread: The Chancellor Wept
- **Hilde's Whisper** (door quest).
  - The first wolf to pay Hilde Dripby `rm_hilde_dripby` 10 pennies (`gift` coins) hears: "He wept, in the Rain Hall, the month before the old King died. Over a paper."
  - This starts the testimony race.
- **The Testimonies:** 23 of the 46 witnesses, a fixed race (world list `nh_witnesses_heard`).
  - **How:** each witness gives one memory to the first wolf who does them a kindness: a `conversation` plus a `gift`, or a `shared meal`. Each memory scores 4.
  - **What each saw:** a little of it: the day, the seal on the paper, the wolf beside him, which way he walked after.
  - **Special witnesses:**
    - Gerrit Mudhand, who mourned the King;
    - Percival Rustford `rm_percival_rustford`, of the household;
    - Mathis Slagwell, a porter on the Sump Stairs;
    - Lavinia Plankwick;
    - Desmond Hollis, but only in the cells if held: "A Visit to the Cells". If he fled, his memory is lost.
- **The Search**, which opens at 23 memories. One of three sites holds the writ (a seeded `pick` at the chapter's start).
  - **The Records Office rolls:** `archive work` ×5 by any wolves.
  - **The crypt behind the Abbot's stone:** a `conversation` with Abbot Lionel for his leave, then `enter place` the crypt at night.
  - **The Greatly Bank flat:** the flat is **Eleanor Saltcot's own home**, so the search needs her leave. At night, her chain "Let me in" does `door.unlock` on the flat's door link (U), then `enter place`.
  - **Each false site still yields something:**
    - the rolls: the Council's order striking Mirebridge from the rolls five winters ago, sealed by the Houses' seats (Mirebridge +10, `nh_names` +5);
    - the Bank flat: Ambrose's note to Oskar, *"Lose it, and your note is torn"*. This opens Oskar's "expose" path and counts as Ambrose's lie for the Captain's turn;
    - the crypt: the old King's draft of the lamp-book line, which Brenna can read at the Moot (+1 free-wolf voice if the writ is never found).
  - **Rosalind's hint:** if the ledger branch was `ash`, Rosalind gives her trusted wolf the site Oskar feared (a journal note naming the right one).
- **Writ found** (`nh_b_writ` on). Brenna reads it on Market Square at noon in a public scene, in her own panel; the same words go up on every board as a signed notice (`board.post`, "(signed) The Herald of Ridgemere"), and a plain world narration says the herald has read the Lantern Writ: *"…the work of reigning passes to the wolf who keeps the peace of the Sump, to hold until the realm howls a Moot: the nine voices of the communities, the four Houses in Council, and the free wolves of the roads."* It changes three things:
  - realm key `nh_writ=read`;
  - the free wolves get 4 voices at the Moot (otherwise 2);
  - the Council's threat to name a ruler ends, and "Stand" gains 20% in Dagfinn's Question.
- **Confronting the Chancellor.** Oskar shares Ashdown House with the Counting House clerks Rune Gullwell and Ivar Pitchwick, who watch him for Ambrose; that is why he will not talk at home. A chain at the City Office, for wolves who heard ≥ 3 testimonies:
  - **Opening words:** "You wept in the Rain Hall. Over the King's writ." Oskar: "Who told you that? No. Don't say. Forty wolves saw, didn't they. I counted the faces for months."
  - **"Tell the Council yourself"** (`confessed`): he goes before the Council and confesses Ambrose's debt and the order. `faction.relation council_of_houses house_greatly tense`. He keeps his post, humbled.
  - **"The Press will tell it for you"** (`exposed`, needs Ambrose's note): the Council Press prints the note. `nh_back_purse` −10.
  - **"Run, if you want"** (`fled`): he walks the Fen Road to the Mirebridge crossroads (`npc.walk`, then `npc.hold` there for the chapter). In talk, "last scented where Mirebridge stood".
  - **The tally:** the first option to hold a majority of the confronting wolves.
- **Hilde's race.** If 10 game days pass after the 23rd memory with no confrontation, Hilde gets there first (`paid`): Oskar pays her and stays. If the writ is not yet found, its site is re-picked and the search needs 5 more archive works.
- **The Queen's Choice** (giver Oda, after the writ is read). The writ passes over her daughter in plain words. The wolf's advice:
  - **contest** (`contest`): Blood +5, and Oda fights for Ottilie at the Moot;
  - **accept** (`accept`): Oda keeps the roll beside Gerrit and blesses whoever wins.
- **The Old Wolves of the Bilge.** A `shared meal` with three Sump elders. It adds Gunnar's own saying to the writ's reading, "The Sump keeps its own peace, or nobody does", and Ridgemere +6.

#### The north thread: The Bell under the Ice
- **Kell's Winter.** Kell distrusts the city. Win him by fishing with him (`hunted` duck at the mere) and a `gift` of `fur_oil`. After that he leads the bearings.
- **Find it** (once `nh_mere_frozen` is on: the real freeze in winter, or the early freeze; §3.2.4).
  - On each `hour` from 22 to 4, the story sends `story.tell` to the region: "Under your paws, the ice rings once."
  - A wolf in one of the six ice regions at a ring hour takes that region's bearing (world texts `nh_bearings`).
  - `skill practised` with `item: listening` gives one bonus bearing.
  - Six bearings place the bell.
- **Frame it.** There is no project, because of the caps.
  - Give Kell 8 `timber`, 4 `tug_rope` and 6 `rope`. `story.area.item` at the shore region shows the makings.
  - Willa's cart brings the frame timber from Coldwater: an escort (`npc.follow` from the Coldwater square to the Boat Shed region).
- **The Haul:** three dusks.
  - **Haulers needed:** at least max(4, helpers/8).
  - **Who counts:** the wolves in the haul region who picked Kell's chain "Take a rope" that hour and `carries tug_rope`, plus each resident present (`resident in region`).
  - **The town at the ropes:** if Coldwater's voice is raised, seven of its residents walk to the ropes (`npc.walk`): Kell, Willa Coldwick, Loam Pinefell `tn_fh_loam_pinefell`, Fallow Lakewick, Harrow Reedford, Myrtle Stillford and Bramble Reedmere `tn_cw_bramble_reedmere`. So a lone wolf with the town behind them can haul. With J6 this is a single count.
  - **Result:** three good dusks raise the bell. Two failed dusks crack it.
- **Tansy** (`nh_b_tansy`; tallied from the wolves' choices; the default is `hidden`):
  - **Teach her** (`taught`): **Stillwater Lessons**, a story-scope quest with `contributors` min(3, helpers) and 10 lessons. A lesson is one of:
    - `together` with her;
    - a `performance` at the Glasswater landing;
    - walking the ice with her (`npc.follow` to the black lane region and back).

    Lessons from Gifted or Quickened wolves (J10) count double. Hidden Quickened wolves can teach too, with `chance 20` per lesson of being seen (Concord −1).

    Taught, she calls the water and rolls the bell into the shallows: the haul needs two dusks, not three, and the Envoy sees it (Concord +2).
  - **Give her up** (`pledged`): a wolf's chain at Gorm, "The Reedmere pup is Quickened" (`told the wardens`). She is pledged to the Wardens' school "in spring". It stays in talk: **she stays home this campaign, and no resident leaves the world.** Gorse's lines turn to grief. Concord +1.
  - **Hide her** (`hidden`): give her `masking_oil`, keep her at the Boat Shed, and answer Gorm's chain with the lie "No pups here but ours". Each hiding wolf rolls `chance 30` against the Envoy's search.
    - Found: `found`. She is pledged anyway, Concord −1, and the scar `tansy_found`.
    - Not found: the black lane stays in the ice.
  - **Her chain** (4 steps, for wolves with a lesson): "Does the water talk?" → "It listens. That's worse." → "Hum with me. It goes still when I hum." → "Will you stand at the landing on the Moot night?"
- **The Bell** (`nh_b_bell`):
  - **rung:** scene `the_bell_rises`, and a world `story.tell`: "From the north-east, over the snow, a bell no living wolf has heard." Mirebridge +20. The bell goes to Ridgemere for the Moot (escorted in Ch 9).
  - **cracked:** it rings flat. Mirebridge +10.
  - **lost:** the chapter closed with no haul begun. The scar `bell_lost`, and "the bell still moves" in talk.
- **The Lamp-Oil List** (needs `nh_ulric ≥ 1`).
  - Bring Fenwick to the Lamp and Pick on a Restday evening: a chain at the Mine Captain's House, "Come and say it in front of them", then `npc.walk`.
  - **The confession scene:** he barred the mine house in the deep winter. Deepvein's families died in the snow or went over the Pass. Thistle Stonepick's brother was among the dead.
  - **Result:** `nh_b_fenwick` on, Frosthollow +10, `nh_ulric` = 2. Ulric walks up the Pass to look for Hakon and comes back (`npc.walk`).
  - If the seam was `crooked`, the chain needs two wolves present.
- **The Frosthollow Count** (party). Iron for the bell's yoke. "Nobody goes down alone, and nobody comes up without counting the others": `region entered` the mine with `party size ≥ 2`, then 4 `iron_ore` to Kell.
- **Gorse on the Ice** (small). Gorse Reedmere `tn_cw_gorse_reedmere` keeps creeping out to watch the black lane. Find him (`near resident` in an ice region) and walk him home (`npc.follow`).

**Dagfinn's Question is decided** at the chapter's end (`nh_b_stands`). The boards say "The lamp-keeper will stand at the Moot", or "will keep the lamp and let others stand".

**Lone wolf and crowd.**
- **A lone wolf** can do the testimonies over several days; the race is only fierce in a crowd. They can haul with the town at the ropes.
- **A crowd** finishes the testimonies in hours and then splits three ways for the search.
- **Surplus buys** the Mirebridge ferry-raft keepsakes raised with the bell: one `item.place` per surplus step, which become gifts for the scattered (+1 name each).

**Recognition.**
- Deeds: "found the Lantern Writ", "heard what the Chancellor wept over", "hauled the drowned bell from under the Upper Mere ice", "sat with the Reedmere pup while the water went still".
- Top 3: the titles "Bell-hauler" (north) and "Writ-finder" (city).
- The wolf who finds the writ is named by Brenna in the reading (C1).

**Failure:**
- writ not found: the scar `writ_lost`, so the free wolves have 2 voices, and the Council may claim a short Moot (§9.4);
- Oskar paid or fled;
- the bell lost;
- Tansy found.

**Quest types:** a testimony race, a three-site search, a confrontation, a blackmail race, oral history over a meal, listening for bearings, a frame haul with an escort, a timed group haul with the town, lessons with a Quickened pup, hide or tell, a confession, a party descent, a rescue.

**Talk** (`nh_stage=bell`; `nh_writ`; `nh_oskar`; `nh_bell=moving|rung|cracked|lost`; `nh_mere=still|pledged|hidden`):

| Dialect | Gossip | Helpful knowledge |
|---|---|---|
| plain | "Everyone's asking what the Chancellor wept over in the Rain Hall. Turns out half the city saw him do it." / read: "The old King wrote it down: the Sump's own peace-keeper keeps the lamp until a Moot. So it was no accident." | "Want the Chancellor's story? Ask the porters on the Sump Stairs. One of them saw the seal on the paper." |
| coldwater | "Hear that? Under the ice. Bright One's tide, it's ringing." / rung: "Net and line, they raised it! Green as a pike and loud as a gale, eh?" / still: "The whole mere's still. I've never seen it so. Calm water to her." | "Take a tug rope and oil your coat, eh? And lie flat on the ice. It carries the sound." |
| larkfield | "A writ lost for a debt. That's the dearest paper ever bought in Ridgemere, lambkin." | "The bell'll want a yoke of good iron. Frosthollow's the only seam deep enough. Scales and silver." |
| southmeadow | "The old King wanted all of us to choose? Sheaf and sickle, I'd best think what I'll say." | "If you're hauling on the mere, take something hot. A windfall for whoever's next to you." |
| mirebridge | "Our bell, under all that ice. Mud and eels, I'd walk the whole Long River to hear it, no?" / fled: "The Chancellor ran down the Fen Road and stopped where Mirebridge used to stand. Even the drowned didn't want him, no?" | "Bring the scattered something from the bell, tadpole. A keepsake off the old ferry-raft, and they'll give you a name." |
| saltwell | "A King writes 'let the realm choose', and the Houses hide it in the dark. The Houses hid it, honoured guest, not the King." | "The keeper of our stores lost a brother at a mine. She has asked which one for years, my sister. Ask Frosthollow." |
| oakhaven | "Old Oak. The old King planted something deep before he fell, mm? Now we see the shoot." | "A bell under ice wants a slow pull. Rush it and it cracks, like green wood, mm?" |
| frosthollow | "Our iron's in the bell's yoke. Cold iron for a cold bell. Lamp and pick." | "The mine captain drinks at the Lamp and Pick on Restday. Bring him there if he's got something to say." |

- **Recap:** "The Chancellor's tears, a writ the Houses buried, and a bell under the Upper Mere ice. The Moot's coming."
- **Window lines:**
  - Oskar (`whispered`): "I've no time to talk. The City Office keeps late hours. Some papers keep later."
  - Hilde: "Ten pennies and I'll remember something. Twenty and I'll remember it right."
  - Kell: "Lie flat. The ice carries it, eh? You'll hear it before you see it."
  - Tansy: "I hum when it's restless. Don't tell. Please."
  - Fenwick, confessed: "I barred the door. I've said it. Now leave me the saying of it, eh?"
  - Thistle: "A mine collapse, they told me. Not which mine. Not who barred what. By salt and water, I'll know now."
  - Dagfinn (`read`): "The old King meant it for someone like me. I'd rather he'd told me. I'd have slept better, no?"

**Triggers.**
- **Existing:** `conversation`; `gift` (coins); `shared meal`; `archive work`; `enter place`; `door.unlock`; `pick`; `board.post`; `story.area.spawn` with a story switch; `camp cleared` (a trigger, matched by `cell`); `scene.play`; `npc.walk`; `npc.hold`; `npc.follow`; `region entered`; `hour`[w]; `season begins`[w]; `weather.set`; `skill practised`[w]; `carries`; `resident in region`; `dialogue choice`; `together`; `performance`; `chance`; `told the wardens`; `party size`; `near resident`; `story.area.item`; `item.place`.
- **Gaps:** C1, C5, C8, C11.
- **New:** J6 (optional), J3, J9 (the writ as a tagged item), J10, R1–R3 (with a `haul_running` part).

### Chapter 9: The Moot ("the Midwinter Moot" in winter, else "the Moot of the Nine Howls"; typically 3 real days at 5 wolves, 2.7 at 200)

**Where:** all 13 posts of the Howl Line (§5.2); the Pass region by the Pass House; Market Square, round its board; the Bay Light `r2_the_bay_light_up`; the Mirebridge crossroads.

#### 9.1 The run-up (from the chapter's opening to the call)

**Ends on: the call of the Moot night** (§3.2): **30%** of the active wolves, **at least 2**, have done a run-up task, and **12 game days** have passed; or an admin's Advance (the usual way for an admin to choose the night). Soft maximum **36 game days**: "ready to push", never by itself unless `ch9_auto` is on.
- **Tasks:** kept a post (a keeper quest done); carried lanterns for the drowned; escorted a stage of the bell's road; raised a silent voice (a late howl or a buy-back); answered the Envoy.
- **The call:** the Moot night is **dusk the next game day**. At once, a signed notice on every board names it, the Moot's name is set by the season on that night (`calendar is {what:"season", value:"winter"}` → "the Midwinter Moot", else "the Moot of the Nine Howls"; realm key `nh_moot`), and the keepers' quests and the ballot stay open until that dusk. The admin may `shift` the night's timer (for example onto Midwinter's own festival night); the chapter never waits for a date by itself.
- **The close** is the night itself (§9.2–9.4), then the epilogue and `finish`.

- **Keep a post.** Each of the 13 posts has a keeper quest, "Keep the <post> on the Moot night". The giver is the post's keeper (§5.2); it is ungated. Ridgemere's is Cressida's waystation (post 12): Brenna holds no post quest.
  - Objectives: `lamp_oil`, plus a `horn` or `handbell`, and the chain "Will you stand there?".
  - Done, a region post joins `nh_posts`. A project post (2, 7, 8, 9, 10, 12) counts only once built with its cell's `spawned_` switch off (§5.2).
- **Raise the silent voices.** Late-howl quests, buy-backs and surplus are all offered once more.
- **The last ballot.** A wolf's `p_backs` is their ballot. It can change once more at the four patrons, at a cost of 2 `p_voice`.
- **Verdicts on the boards.** Each town's board posts its verdict so far, with its reason, refreshed daily. Examples: *"Coldwater will howl for the Regency of the Moot: its wolves back it most."* *"Oakhaven will howl against Fell's choice: the hills went to Fell."* The texts are pre-written per backing and lean (§9.4).
- **The bell's road** (if rung or cracked). A story-scope escort in stages, from the Upper Mere along the High and East Roads to the Bay Light. `bandits.call` if `nh_threat > 50` or the lists were `sold`. On the night, Solveig rings it as the howl comes in.
- **Lanterns for the Drowned** (crown S8.2).
  - A **journal offer** (no `giver`) on `region entered` of the Mirebridge crossroads at night; its journal text names Skye Eelbank as the one who asks, with no quoted words of hers. Skye already holds her pledge and the Mirebridge post quest in Ch 9, so this costs her no hub slot (§10.3); her thanks is her window line, gated on the quest (N2), or C5 `npc.greet`.
  - Carry `lamp_oil` to the Mirebridge crossroads on three nights running (`region entered` at night). The marsh lights answer, true ones this time (`story.ping`). Mirebridge +6.
  - Optional: bring Nell Plummeadow there (she is only a receiver). She reads her old unanswered letter at the crossroads and lets it go: a scene.
- **The Envoy's Three Questions** (crown S8.5). Asked through Gorm, of the first three wolves. "What kind of realm is this?" has honest, proud and evasive replies. Honest answers about the lists and the debts give Concord +1 each.
- **The Captain's last road.** His band gathers at the Pass when the night is called (`story.area.spawn` in the Pass region, cell `9_3`, switch `spawned_pass` on; with L2 `nh_pass`). Ulric is walked to the Pass post.

#### 9.2 The Moot night: the Holding Howl
- **At dusk**, a signed notice on every board, *"The Moot night. Let the realm howl. (signed) The Herald of Ridgemere."* (in winter: *"Midwinter, and the Moot night. Let the realm howl."*), the same words in Brenna's window line (`say[self.id=rmx_brenna_abbot world.mem.nh_howl=night]`, one line per `nh_moot` value), and a plain world narration: "Dusk on the Moot night. Somewhere on the Pass, the first howl goes up." The Moot's cast walk to Market Square.
- **Both arms start at once:** the Pass post (1) and Saltwell (4).
- **Windows.** Each post's window opens when the post before it on its arm closes, and lasts one game hour (10 real minutes).
- **A post answers if any of these hold:**
  1. a wolf howls in its region: J5 `howled` or `chorus closed`. The fallback is ≥ 1 wolf in the region at the window's close;
  2. its community's voice is raised and its keeper is in the region. The keepers are walked there at dusk, and J5 `npc.howl` makes it audible. **This is how a small world wins:** a town that was raised answers with no wolf there;
  3. a wolf who finished that post's keeper quest is anywhere in that town (`wolf in town`).
- **Carry.** A post can take up the howl only if one of the two posts before it answered. A post with at least max(2, helpers/25) wolves, or one marked "practised" in Ch 4, is **strong** and carries two steps alone. **One silent post never ends the night**; it just costs that town its answer.
- **The junction.** The trunk's first window (the Glasswater relay) opens when either arm reaches it. The north arm takes 3 windows, the south 5. The south arm's posts still answer in their own windows, and they count, if they finish before the trunk reaches the Bay Light.
- **The trace.** A world `story.tell` follows each post, pre-written for each post and outcome. Examples: "The howl has reached Coldwater." "Fenshore answers, and the fens with it." "Silence at the Upper Mere post…"
- **The Pass fight.** At the Pass post's window the Captain attacks: C10 `foe.add` champion "the Grey Captain", band 6, plus 2 if the ford burned. Today it is `story.area.spawn` 6 plus `bandits.call`, and "won" is a trigger on `camp cleared` with `cell` `9_3` and `spawned_pass` on (C10's `foe defeated` once it exists). The post answers if the fight is won before the window closes, or if a wolf still stands in the region at the close.
- **The Turn.** If at least 2 of these 3 keys hold:
  1. `nh_b_parley = offered`;
  2. `nh_b_fenwick` on;
  3. Ambrose's lie shown: a ledger copy exists, or Ambrose's note was found.

  …and Ulric is at the post, the scene `the_turn` plays. Ulric's lines are in his panel, and the strangers' answers come as region `story.tell` lines: "The tall wolf in grey lowers his lamp."

  Ulric: "Hakon. They said what they did. Frosthollow said it, out loud, at the Lamp and Pick."

  The choices for the wolves present:
  - **"Let them go over the Pass"** (`turned`). Hakon and the Captain go back over the Pass with the Deepvein coin, "to bring the Deepvein families down in spring". The post answers strong, and Frosthollow mourns and forgives. *No one stays: strangers leave* (§8.5).
  - **"Take the Captain to the city"** (`beaten`). The band breaks, and the Captain is led away. Hakon goes back over the Pass alone. See **The beaten Captain** below.
  - **"No quarter"** (`dead`). Only by this explicit choice. Ulric's grief lines follow, and Hakon flees.
- **Without the turn:** a won fight is `beaten` (handled as below); a lost one is `fled`, over the Pass with his coin, and the villain figure stays set (a sequel).
- **The beaten Captain** (decisions §9, P1-12). C10 removes downed foes after the fight, and Eskar has no body, so he is never "led" as a resident and never held with `npc.hold`. Instead:
  - after the Turn, `visitor.add {visitor:"captain", name:"the Grey Captain", like:"cty_stone_sunton", description:"a tall black arctic wolf with pale, nearly silver eyes, lamp-black on his paws, a grey cloak, his paws bound", minutes:1440}` at the Watch cells `r2_the_keep_down`, for **one game day**; he is mute, as ever;
  - `board.post` on `board:ridgemere`: *"The Grey Captain is held at the Keep, to be judged at the Moot's end."*;
  - realm key `nh_captain=beaten`, with talk lines (cast-and-talk §3.24, §4.11–4.12);
  - **the epilogue's judgement is board and talk only** (§9.8): no resident body, no hold, no stranger who stays.
- `nh_answered` = posts answered.

#### 9.3 The Moot (midnight, Market Square)
A public scene, `the_moot`, in the Market Square area round the board. Its cast (≤ 16):
- Brenna, who reads each voice as it came in off the Line;
- Gerrit, keeper of the roll, and Oda beside him if `accept`;
- Dagfinn and Ottilie;
- Ambrose;
- Signe Timberland `rm_signe_mossgrave`, Isolde Fell, Celestine Greatly;
- the Grayrock seat: Aldric, Tobias or Lavinia;
- Gorm, for the Envoy;
- Bettany or Corra, for the Watch;
- Solveig, if the bell came.

The counting opens with Brenna's reading in her own panel of `the_moot`. Then, one a game minute (`wait`), each community's voice is told as a plain narration (`story.tell` to the region, no `from`, reported and not quoted: "The herald reads Coldwater's voice: for the Regency of the Moot."), pre-written per community and verdict, and the same line is added to `board:ridgemere`. (Thirty-six per-community scenes would break the 32-scene budget, and a banner with Brenna's face is not the window.) Then the free wolves' `choice` beat. Then one of four embedded ending scenes.

#### 9.4 How the voices are counted
This rule is posted on every board and in the journal from the start of Ch 9.

1. **The nine community voices.** Each is 1 voice if its howl reached the Bay Light (its post answered; Ridgemere's is the waystation), or ½ if it was silent.
   - **Its verdict** is the backing with the most pledged weight there: the sum of `p_voice` of the wolves pledged to that community, by backing. This is computed at dusk, town by town, with `each over list nh_pledge_<town>` into four reused story numbers; J11 `tally` takes over past 500 wolves.
   - **With no pledged wolf, or a tie,** the town follows its lean:

| Community | Lean | Override |
|---|---|---|
| Ridgemere | Lamp if he stands, else Blood | — |
| Coldwater | Moot | Blood if Oda contests (she is a Coldwater wolf) |
| Larkfield | Moot | Purse if the ledger was kept |
| Southmeadow | Blood | Moot if the stone was read |
| Fenshore | Lamp if he stands and the lights were laid, else Moot | — |
| Saltwell | Blood | — |
| Oakhaven | Moot | **never** Fell's choice if the hills went to Fell (applies even over pledges) |
| Frosthollow | Moot | Lamp if Fenwick confessed; Purse if the seam was crooked |
| Mirebridge | Lamp if he stands, else Moot | silent: the Council casts it, as the writ allows for a voice the rolls do not hold |

2. **The four House votes**, set by the branches:

| House | Vote | Changed by |
|---|---|---|
| Greatly | Purse | cast by Celestine for the Moot if the ledger was shown; abstains if kept; counts 2 if the Crown took Greatly's loan (Ch 2) |
| Fell | Purse | Moot if `gudrun = match` and the hills were refused; against the others' majority if Fell's head was shamed in the quarrel |
| Grayrock | Moot | Lamp (Blood if he doesn't stand) if `crown` and `nh_b_wharf`; Purse if `absent` |
| Timberland | Moot | against the others' majority if Timberland's head was shamed in the quarrel |

3. **The free wolves of the roads:** 4 voices if the writ was found, else 2. Half come from the tally of every participant's backing, weighted by `p_voice`. Half come from the Moot scene's `choice` majority among the wolves present. **A crowd on the night can move at most 2 voices.** The crypt draft adds 1 if the writ was never found.
4. **Bought voices:** floor(`nh_bought` / 5), at most 3, always Purse.

**Result.**
- **The most voices wins.**
- **"The Moot will not crown by one voice":** if the top two are within 1 voice, the Regency of the Moot wins.
- **If Dagfinn does not stand,** Lamp voices go to the Moot.
- **A short Moot:** if fewer than 5 community voices were heard and the writ was not found, the Council claims its old right, and the Council's majority backing gains 2 voices.

#### 9.5 Endings: who reigns

| Ending | When | The world after (lasting) |
|---|---|---|
| **Queen Ottilie, with the Lamp beside her** | Blood wins | C14 atomic swap: Ottilie takes "reigning at the Great Palace". Without C14 the post stays on Dagfinn's books "by custom, until her crowning at the Blossom Fair", which is the sequel hook. Dagfinn is the Lamplighter: he keeps the Palace lamp and walks the Sump round on Restdays (talk). Oda is reconciled if she was `told_queen` or `contest`; otherwise "she means to go home to the lake in spring" (talk). Figures: ruler Ottilie, "Queen of Ridgemere"; captain Bettany if `bettany`. If `nh_b_cough`, she stands bare-headed in the rain at the Moot. |
| **The Lamp-King** | Lamp wins | There is no post change: Dagfinn keeps the work he already holds. Figure ruler "the Lamp-King". His first act: `project.post waystation` at the Mirebridge crossroads (town fenshore), once the watch post there is built. Mirebridge's name is cut back into the Sunfields stone (talk). `faction.relation house_greatly council_of_houses tense`. |
| **The Regency of the Moot** | Moot wins, or a one-voice margin | There is no ruler. Dagfinn keeps the lamp as the Moot's steward (no post change). Brenna is named Speaker of the Moot (figure lord). The Moot sits every Midwinter, so the next campaign is the next Moot. The Council is cut to an advisory voice: talk, and neutral faction relations. |
| **The Bought Crown** | Purse wins | C14 swap: Rosalind is crowned unwillingly. If Rosalind was turned out and Julian has sailed, there is no swap: the chair stands empty and Ambrose rules as Lord Protector in talk. Figures: villain Ambrose, "the Lord Protector". Purse wolves with `p_marks ≥ 3` are "the Protector's Own". The last scene: the new ruler, or the empty chair, alone by the lamp, and Dagfinn at the door saying to the top `p_voice` wolf, by name (C1): "Next Moot, howl louder." Sequel: "The Debt of Ridgemere", with the wolves as the resistance. |

#### 9.6 Endings: the Concord's verdict (the Envoy, through Gorm)
**The score:**
- `nh_answered` (0–13);
- Tansy: taught +2, pledged +1, hidden 0, found −1;
- peace kept +1, ford held +1, the Envoy walked down +1, at least one Quickened wolf registered +1, each honest answer +1;
- truce broken −1; Watch owned by Greatly and cudgels in the city −1.

| Score | Verdict | What happens |
|---|---|---|
| ≥ 14 | **The Realm Answers** | She tears the Concord's Keeping at the Bay Light (`story.tell` world). The Wardens leave by the Pass. The realm keeps its own Quickened, with Gorm, the Order's Eye, watching them from the abbey (talk; the first `warden_order` post for campaign two). Every wolf who held a post: the title "Howler of the Holding". |
| 9–13 | **The Realm Holds** | The Keeping is set aside. One Warden stays at the Grey Gull until spring (talk, and a weekly stranger in the next campaign). The silent towns get a Warden's eye. |
| ≤ 8 | **The Realm Bends** | By spring the Order has an Eye in every silent town, every Quickened wolf must be named to it, and its pups go over the Pass. The next campaign is winning them back. It is still a full ending: every deed, title and thanks is given. |

#### 9.7 Recognition at the Moot
- **The Moot Roll.** Every wolf in `nh_roll` gets a `journal.note` with their pledged town's line, and the deed "stood with the realm at the Midwinter Moot" in winter, else "stood with the realm at the Moot of the Nine Howls".
- **Howl-friend of <town>:** the top max(1, pledged/10) wolves by `p_voice` in each `nh_pledge_<town>` (C3). Each gets the title (C4) and their Keeper's thanks by name (C5): "#thanks#, #address#. My town howled because you walked it."
  - Each also gets 10 pennies from `town:<town>` (the town's stores, 212–248 at the checkpoint; Mirebridge's from the treasury), at most 3 wolves per town, under the counters (§2.11), `ifRefused: skip`. A wolf past the day cap or the floor is paid in kind (`bread` ×2) and the Keeper's thanks takes its "thin purse" variant.
- **Voice of the Realm:** the top max(1, active/25) wolves realm-wide (C3).
- **The Lantern:** the single top wolf becomes `world.figure_hero` (C9 with a wolf), so every existing hero line names them across the realm. The road nickname family already holds "the Lantern".
- **The night's deeds:**
  - "held the Pass on the Moot night";
  - "howled for <town> on the Moot night" (per post);
  - "rang the drowned bell at the Bay Light".
- **The Purse wolves:** "the Protector's Own" (Bought Crown only).
- **Quickening:** each finished side story marked `closure: "side"` wrote a `story_closure` receipt (J3; at most 4 a chapter). A wolf with two or more has met `closedStories: 2`.

#### 9.8 Epilogue and clean-up (after the `finish`)
- **Lasting talk** (C8, `keep: true`, renewed for 90 days): `nh_crown`, `nh_concord`, `nh_captain`, `nh_bell`, `nh_cut`. Every town talks of the night for a season.
- **Lasting world:** the figures set; the posts standing; the faction relations; the ruler's post (C14, or the sequel text).
- **The Captain's judgement** (if `beaten`). Board and talk only: a `board.post` on all eight boards, pre-written per ending (the Queen sends him back over the Pass with the Deepvein note torn; the Lamp-King sets him to carry stone for the Mirebridge waystation "until spring", in talk; the Moot hears him and lets him go; the Protector sells him the deed he was promised), and the realm key `nh_captain=beaten` kept 90 days. His one-day visitor has already left; no resident or stranger stands for the sentence.
- **Unprotect.** `npc.unprotect` and C11 `npc.unspare` on everyone in §8.6. The same list runs in `ninth-howl.cleanup` (§8.6) if any act is stopped early.

**Talk** (`nh_stage=moot`; `nh_moot=midwinter|nine_howls`; then `nh_crown=ottilie|lamp|regency|bought`; `nh_howl=night|ran|broke`):

| Dialect | Before the night / helpful | After |
|---|---|---|
| plain | "Every town's sending its howl to the Moot. Ours goes to whoever kept the peace best." / helpful: "On the Moot night, listen for the Bay Light bell, then count a hundred breaths. That's when the far howls come in." | Ottilie: "Bright One keep the Queen. And the lamp-keeper beside her, bless his flat feet." / Lamp: "A Sump wolf at the lamp, by the realm's own howl. Blood and bread, I howled for him." / Regency: "No crown at all, and a Moot every Midwinter. I'll believe it when it sits twice." / Bought: "The Counting House has the realm now. They call it a Protectorate. I call it a debt with a crown on." |
| coldwater | "If the howl stops at the mere post, it's not over. The next post can take it from two back, eh?" | "We answered, eh? Glasswater answered! A howl over the ice." |
| larkfield | "Our voice to the Moot is bought by deeds, not silver. Scales and silver, that's a market I like." | "Larkfield answered on the dot. Sold, to the realm." |
| southmeadow | "Southmeadow's voice is ready. A windfall of a winter, my apple." | "Ripe days. We howled the burying howl by mistake and nobody minded." |
| mirebridge | "Mirebridge has a voice again, no? Dry feet to every wolf who carried it." | bell rung: "Our bell rang the Moot in. Fen-sure, the drowned heard it." |
| saltwell | "Stand by the pool when the night falls, honoured guest. Saltwell starts the south road's howl." | "The howl came out of the dark to the oasis, my sister, and we sent it on. The well remembers." |
| oakhaven | "A handful of wolves and one voice. That's a lot of shade for a small grove, mm?" | "A night to remember, mm? It came through the cedars slow and climbing." / winter (`nh_moot=midwinter`): "A Long Night to remember, mm? It came through the cedars slow and climbing." |
| frosthollow | "Frosthollow starts the howl at the Pass. Lamp and pick, we'll be heard." | "Held the Pass post. Cold iron and warm hearts, the both of it." / Bought: "Bought, then. Black seam. Next Moot, we howl deeper." |

- **Window lines:**
  - Dagfinn, standing: "I'll stand. Not for the chair. For the round, no?" Not standing: "Let the Moot keep the lamp. I'll carry it to them."
  - Ottilie: "If the realm wants a Harrowgate, it shall have one who carries her own petitions."
  - Ambrose: "Count the voices, by all means. I've already counted mine."
  - Gorm: "She is listening, honoured guest. So am I."
  - Ulric: "If he's alive, I'll stand on the Pass with a lamp lit."
  - Brenna: "I'll read every voice as it comes in off the Line. Make sure yours comes."

**Triggers.**
- **Existing:** `festival`; `hour`; `schedule`; timers; `count in region`; `wolf in town`; `resident in region`; `region entered`; `story.area.spawn`; `bandits.call`; `scene.play` (`choice`, ending scenes); `story.tell`; `wait`; `each over list`; if/elif; `finish`; `npc.walk`; `npc.unprotect`; `project.post`; `faction.relation` (B); `character.grant`; `deed.award`; `journal.note`.
- **Gaps:** C1, C3, C4, C5, C8 (`keep`), C9 (with a wolf hero), C10 (`foe.add`, `foe defeated`), C11 (unspare), C14 (the ruler swap only). Plus the `visitor.add` for the beaten Captain and the purse counters (existing).
- **New:** J5 (howls and `npc.howl`), J11 (`tally`), J2, R1–R3 (two working phases). `calendar is` season (existing) for the Moot's name.

---

## 7. Quest packs: how the small, medium and large packs feed the campaign

- **What a pack is.** An open-audience program with its own id (one live run per program id), for example `nh-pack.small.coldwater` or `nh-pack.medium.fens`. It has at most 32 quests, and it declares only the world variables it touches.
- **Gates.** It gates each quest on `compare nh_stage` (for example, Ch ≥ 4) and on branch variables (for example, a Fenshore "Bread for the bog-boats" quest only if `nh_b_sloe = turned`).
- **What a pack may write**, from a `quest completed` trigger, as a v2 action and not a reward:
  - add to `nh_realm` and to `nh_voice_<town>`, with the distance weight (small 2, medium 4, large 8, times 1, 1.5 or 2);
  - lower `nh_threat` (bounties: −1 small, −3 large);
  - add to `nh_names` (Ch 4–6 only).
- **What a pack never writes:** branches, `nh_stage`, `nh_surplus`, posts or Keepers.
- **Personal credit.** With J2, a pack also adds to the wolf's `p_voice`. Without J2, packs credit towns and the realm but not the wolf's own tracker, so C12a `story.notify` to the running chapter (event `story notified`), with `player: $actor`, is the second route.
- **Coin.** Every pack pays under the one counter rule of §2.11 (small 2–4, medium 6–15, large at most 12 a chapter and 35 a line, the stage pack as its tier; restitution from a villain's till at most 10 a wolf and 60 in all, and only after SQ2). The small pack's own day caps and the medium pack's `coins_<town>` guard are replaced by it.
- **Givers** (decisions §1.2; the registry is `givers.tsv`). These rules replace this section's earlier "one pack quest per innkeeper":
  1. **At most one pack quest per resident, across all packs.** A pack quest is being a quest's `giver` or holding any `npc.offer` choice of one; several `npc.offer` steps of the same quest count as one. A resident who is only a `conversation`, `gift`, `trade done` or delivery target holds no hub item and is not a giver, but no resident is a target in more than two pack quests, and never in a premise that contradicts their other role.
  2. **Campaign cast and keepers carry no pack quests.** They may be a pack's conversation or gift target only where the campaign's facts agree (allowed: Bettany Nailley and Solveig Tarwell in `lq-wreckers-light`, Jorunn Ropewick and Piet Lumley in `mq-sump-lamps`, Ivy Stonehollow in `lq-vanishing-thief`). A `campaign target` may carry one pack quest if its premise agrees with the campaign fact (Palle Plankmore's `mq-cinder-gate-tally` is the campaign's own Ch 3 paw-mark).
  3. **Campaign offers: at most 2 per resident per chapter** (§10.3).
  4. **Innkeepers give helpful knowledge and recap lines only**: no quests, no `npc.offer` and no campaign offers in any file, and they are never Keepers or fallbacks. Each may be a delivery or conversation target in at most one quest across all files (Rowan Downridge: Rook's word, Ch 7; Nell Plummeadow: Nell's letter, Ch 9; Oriel Marshditch: The Scattered, Ch 4; in the medium pack, Loam Pinefell and Bramble Reedford in `mq-coldwater-still` and Thorn Ashwood in `mq-lamp-oil-share`).
  5. **Look-alikes** (§8.5) give no pack quest.
- **Stage-matched pack quests** are the stage pack `nh-pack.stage` (`Data/Storylines/ridgemere3/quests-stage/nh-pack.stage.json`; one open-audience program, phases `quiet`, `s12`, `s3`, `s4`, `s56`, `s78`, `s9` entered by an `hour` trigger that reads `nh_stage`). Its 22 quests (12 small, 5 medium, 5 large) are listed in decisions §7.3 and written in full in `quests-small.md` §2.10, `quests-medium.md` §4b and `quests-large.md` §12b. **The old seed table that stood here is replaced by that list.** Of the old seeds:
  - **kept by the campaign itself:** *a late howl* (the Keepers' Ch 4 howl quest, offered again in Ch 5–9 as a journal offer for a town whose howl is still lost; Ch 4 Failure) and *Silver Under the Ice, weekly* (the Ch 4 side story's own `repeat: weekly`);
  - **already covered elsewhere:** ring the dawn bell (small #14); the lamplighter's round (medium Q1); the Short-Weighed Sheaves (`lq-clipped-silver` ch 1); douse a lamp (Ch 3); the Cinder Gate's paw-mark (medium Q2); carry one letter (Ch 5's forty letters); bread and salt to a steward (Ch 6's truce tokens); return a cache item (Ch 6); the Frosthollow Count (Ch 8); oil for a post and a horn for a keeper (Ch 9's keeper stocking); fish for the post-builders (replaced by `st-post-timber`).

---

## 8. The cast

Everyone named is a **placed, living** resident, checked on 2026-11-22, except the strangers in §8.5. Window talk comes from `say[self.id=…]` tree lines and from story text that opens the dialogue window: offers (`say`, `thanks`), `npc.offer` chains, `npc.line`, C5 `npc.greet`, C6 `offer.during` and scenes (each speaker in their own panel). **No `story.tell` carries `from` with a resident's words** (§2.1). **`mantle.assign` is never used** (C15 is not needed). The corrections found while casting (cast-and-talk §2, items 1–8) are applied throughout; the look-alikes follow decisions §1.6.

### 8.1 Heroes

| Name (id) | Sex, age; voice | Motive | Arc | Fates by branch |
|---|---|---|---|---|
| **Dagfinn Thornham** `rmx_dagfinn_thornham` | m 31; Mirebridge-born, Fenshore-raised (mantle mb131), calm; compact russet arctic, **notched left ear** (the patrol pair's "matching ears" joke with Cressida) | keep the lamp he was handed, and the round | bewildered patrolman (Ch 1) → walks the fens (Ch 6) → hears that the writ was meant for him (Ch 8) → stands or steps aside | Lamp-King; the Queen's Lamplighter; steward of the Moot; the wolf at the door of the Bought Crown |
| **Ottilie Harrowgate** `rm_ottilie_harrowgate` | f 39; plain, calm, warm, coughs in the damp | the petitions answered | frail heir → tended (Ch 4) → the Blood candidate | Queen; the Lamp-King's councillor; the Moot's first petitioner |
| **Herald Brenna Abbot** `rmx_brenna_abbot` | f 52; plain, warm; owes a factor | proclaim the law truly | the campaign's voice: every proclamation, as a signed board post and her own window line; Ridgemere's Keeper (pledges, the Ch 4 howl quest); reads the Moot. The Moot backing is Gerrit's. | Speaker of the Moot under a Regency; always the herald |
| **Cressida Cotton** `rmx_cressida_cotton` | f 19; Frosthollow-raised (fh115), **cold and frank** (blunt, suspicious; clipped lines, never warm); stocky russet red wolf, notched left ear like her partner's | stand by her patrol partner | the first quest-giver (Ch 1) → keeper of the waystation post (Ch 9) | Dagfinn's sergeant in every ending except the Bought Crown, where she "signs on the Grey Tern" (talk) |
| **Sergeant Bettany Nailley** `rm_bettany_nailley` | f 26; plain, ambitious, warm; hates Grayrock | an honest Watch | the lists' champion (Ch 3) → rides with the ford escorts (Ch 7) | leads the Watch (figure captain) if the list reaches her; forgives Grayrock if the wharf names are read |
| **Rosalind Greatly** `rm_rosalind_greatly` | f 22; plain, bookish | the ledger that doesn't add up | whistleblower (Ch 2) → turned out, or silenced | Greatly's conscience; the unwilling Bought Queen |
| **Gerrit Mudhand** `rm_gerrit_mudhand` | m 55; plain, chatty, warm | keep the Houses at one table | receives the ledger; objects to the lamp-book; keeper of the Moot's roll and **patron of the Moot backing** (Ch 2–9; plus his story-heard nodes, never more than 2 offers a chapter) | Speaker of the Council in every ending |
| **Ulric Stonevein** `cty_ulric_stonevein` | m 30; frosthollow, short, warm | find out what became of Hakon | smells the harness (Ch 3) → the lamp-oil list (Ch 8) → the Turn at the Pass (Ch 9) | parts from Hakon at the Pass; or mourns him |
| **Tansy Reedmere** `tn_cw_tansy_reedmere` | f 14; coldwater (cw127), calm, hums old marching songs | hide that the water comes when she calls | the "something big" (backstory) → hidden, taught or given up (Ch 8) | the realm's first kept Quickened; pledged to the Wardens' school "in spring"; hidden, with the black lane |
| **Solveig Tarwell** `rm_solveig_tarwell` | f 44; plain, quiet; carries a Grayrock letter | the drowned deserve a rite | the wharf names (Ch 5) → the Rite of the Tide at Mirebridge (Ch 6) → the Bay Light bell (Ch 9) | rings the drowned bell as the howl comes in |
| **Skye Eelbank** `tn_mb_skye_eelbank` | f 50; mirebridge speech, chatty, warm | keep the paths and the names | the path with no note (Ch 6) → named Mirebridge's Keeper | Mirebridge's voice at the Moot |
| **The Keepers** (§5.1) | own dialects | their town's say | pledges; the howl quests (Ch 4); the posts (Ch 9) | each howls a verdict at the Moot |

### 8.2 Villains and rivals

| Name (id) | Sex, age; voice | Motive | Arc | Fates by branch |
|---|---|---|---|---|
| **Ambrose Greatly** `rm_ambrose_greatly` | m 63; plain, courteous, never raises his voice | "his heir to inherit a city" | chalks the doors (Ch 1–2) → buys voices → owns the Watch, or doesn't (Ch 3) → paid the Captain with a false promise (revealed Ch 7–8) → hid the writ (Ch 8) | shamed on the Council (ledger shown or note printed); owed a favour (kept); Lord Protector (Bought Crown). He is never fought: he is beaten by ledgers and votes. |
| **The Grey Captain, Eskar Rimedrift** (`cty_eskar_rimedrift`, unplaced) | m 44; Frosthollow broad, cold: "Warm hearth, they say up there. Not for Deepvein wolves." | buy back Deepvein for the miners Frosthollow turned out | lamps (Ch 3) → named (Ch 3) → bounders (Ch 5) → southern company (Ch 6) → parley and siege (Ch 7) → the Pass (Ch 9) | **turned** (back over the Pass with Hakon, to bring the Deepvein families down in spring); **beaten** (a mute stranger held one game day at the Watch cells `r2_the_keep_down`, a board post, and a judgement in board and talk only: §9.2, §9.8); **dead** (only by "no quarter"); **fled** (over the Pass with his coin: the sequel's villain) |
| **Desmond Hollis** `rmx_desmond_hollis` | m 33; Coldwater-raised, short, proud, sly; saw the Chancellor weep | lost his Palace post when a Sump patrolman took the lamp | buys wagon days at the Carters' Yard (Ch 2–3) → held or fled (Ch 3) → river company or trial (Ch 7) | held → mercy → redeemed at the granary; the pillory; fled → downed, never killed, at the ford (C10 `foe.outlaw`) |
| **Queen Oda Harrowgate** `rm_oda_harrowgate` | f 60; Coldwater-raised (cw210), restless, sly | the purse, and her daughter | the Queen's Purse (Ch 1) → the ear in the Residence (Ch 2) → the Queen's Choice (Ch 8) | reconciled Queen Mother; keeper of the roll beside Gerrit; going home to the lake in spring |
| **Corwin Fell** `rm_corwin_fell_` | m 31; loud, proud | the Rainward Hills | the claim, the bounders and the wager (Ch 5) | humiliated; the split; granted, and Oakhaven's lasting enemy |
| **Lorn Hedgeton** `cty_lorn_hedgeton` | m 58; Coldwater-raised, cold, ambitious | coin for cart days | the informer (Ch 3) | shamed; turned, and gives the lamp code |
| **Fenwick Plankworth** `cty_fenwick_plankworth` | m 54; Coldwater-born mine captain, restless, sly | save his mine, any way he can | the seam note (Ch 2) → Highmere's silver (Ch 4) → the confession (Ch 8) | honest and confessed (the key to the Turn); crooked and named aloud by the Captain |
| **Hilde Dripby** `rm_hilde_dripby` | f 61; short, sly thief; saw the weeping | coin for what she saw | the whisper (Ch 8) → the race for Oskar | paid off; beaten to the Chancellor's door |
| **Corra Coalcot** `rm_corra_coalcot` | f 22; chatty, merry, sly; 5 recorded thefts; carries the list hook herself (her real piece is in her hood) | coin | sells false lists (Ch 3) → the true thief behind Ingrid Kendrick's name (Ch 3) | reported; her fish-oil papers gossiped about |

### 8.3 Turnable and key NPCs

| Name (id) | Role |
|---|---|
| Chancellor Oskar Gullworth `rm_oskar_gullworth` | the weeping Chancellor: owned, not wicked. He confesses, is exposed, runs, or pays Hilde. He lives at Ashdown House with the two Counting House clerks, who watch him. |
| Captain Corra Slagmore `rm_corra_slagmore` | the indebted commander: freed (redeemed), standing aside, resigning, or owned |
| Gudrun Fell `rm_gudrun_fell_`, Osric Timberland `rm_osric_mossgrave` | the match that sets the quarrel's shape |
| Lord Aldric `rm_aldric_grayrock_`, Lady Maren `rm_maren_grayrock_`, Ingrid `rm_ingrid_grayrock_`, Tobias `rm_tobias_grayrock_` | the forty letters and the homecoming |
| Isolde Fell `rm_isolde_fell_`, Signe Timberland `rm_signe_mossgrave`, Celestine Greatly `rm_celestine_greatly`, Halvard Timberland `rm_halvard_mossgrave` | the House heads (protected by rule) and the old smith-lord; their votes are the Council |
| Karin Stanmore `rm_karin_stanmore`, Abbot Lionel Ropestack `rm_lionel_ropestack`, Desmond Dripcot `rm_desmond_dripcot`, Desmond Farrow `rmx_desmond_farrow` | the four favour-keepers of the Watch lists |
| Mab Coalridge `rm_mab_coalridge` | the ear in the Residence |
| Edric Kelpham `rm_edric_kelpham`, Eleanor Saltcot `rm_eleanor_saltcot`, Jorunn Ropewick `rm_jorunn_ropewick`, Jasper Bilgeford `rm_jasper_bilgeford`, Rune Gullwell `rm_rune_gullwell`, Ivar Pitchwick `rm_ivar_pitchwick` | the money trail and the Purse path. Rune and Ivar live at Ashdown House, the Chancellor's house, and watch him for Ambrose. Eleanor lives in the Greatly Bank flat `rm_the_greatly_bank_flat`, so its search needs her leave (Ch 8). |
| Gorm Plankgate `rm_gorm_plankgate` | the Order's Eye in Ridgemere (an informant, not sworn, not Quickened; the Wardens canon, 2026-11-24): the first `warden_order` member and the Envoy's only voice. Saltwell-raised; works at **the Sump Infirmary** `rm_the_sump_infirmary` (door `w_ridgemere_0_0` (249, 250)), though his work label says "infirmarian of the abbey"; all his scenes and hand-ins are there. |
| Kell Greyton `tn_cw_kell_greyton`, Willa Coldwick `tn_cw_willa_coldwick`, Myrtle Stillford `tn_cw_myrtle_stillford`, Gorse Reedmere `tn_cw_gorse_reedmere`, Bramble Reedmere `tn_cw_bramble_reedmere` | Coldwater and the ice |
| Ulla Rusthand `cty_ulla_rusthand`, Cairn Oakhaven `tn_oh_cairn_oakhaven`, Nan Ashhand `cty_nan_ashhand` | the cut and the common right |
| Briar Cornby `cty_briar_cornby`, Bramble Bogditch `tn_mb_bramble_bogditch`, Thistle Stonepick `tn_fh_thistle_stonepick`, Imogen Nailridge `rm_imogen_nailridge`, Tamsin Eelbank `tn_mb_tamsin_eelbank`, Fell Miremore `tn_mb_fell_miremore` (Mirebridge-blooded, Ridgemere-raised: **plain**) | the stone, the well, the diaspora |
| Sloe Mistwick `cty_sloe_mistwick`, Ludo Beechshaw `tn_oh_ludo_beechshaw`, Rook Wheatfield `tn_lf_rook_wheatfield`, the Larkfield smith `tn_mb_tansy_eelbridge` | the fens and the ford |
| Ingrid Kendrick `rmx_ingrid_kendrick`, Piet Lumley `rmx_piet_lumley`, Hanne Coalton `rm_hanne_coalton` | small lives the campaign answers |

**Name collisions kept apart.** Tansy Reedmere is the only "Tansy" in the story's voice; the other is always "the Larkfield smith". Tamsin Icefell is an innkeeper with helpful lines only; "Tamsin" in the story is Tamsin Eelbank.

### 8.4 The Keepers and post keepers
See §5.1 and §5.2.

### 8.5 Strangers and foes: they come and go
There are **three** strangers. A `like:` resident is never cast, gives no pack quest, and lives where the stranger never walks (decisions §1.6; `givers.tsv` role `look-alike`). Each was checked read only on 2026-11-22: placed, alive, home and work reachable. `cty_ulla_rusthand` (Oakhaven's Keeper), `rm_cressida_carlisle` and `cty_ulric_stonevein` are no longer look-alikes anywhere.

| Who | How | When | Leaves |
|---|---|---|---|
| **Warden-Envoy Ingeborg Rimewater** (f) | `visitor.add` id `envoy`, `like: tn_sm_brisk_plumby` (Brisk Plumby: f 52, arctic, pot hand at the Vine and Sickle, Southmeadow; the Envoy's road, from the Pass House by Coldwater and the city to the Bay Light and Market Square, never reaches Southmeadow), description "a silver-muzzled arctic wolf in the Concord's black cloak with silver trim". A sworn Warden, so Quickened: cold, exacting, cultured, faintly contemptuous. Mute. By the Order's custom a sworn Warden does not speak to the unsworn, so **she speaks only through Gorm**, and never needs a talking stranger. | Ch 5 end – Ch 9; re-added daily where she is | by the Pass, in every ending (talk keeps a Warden at the Grey Gull in "the Realm Holds") |
| **The Grey Captain** | Ch 7 parley: `visitor.add` id `captain`, `like: cty_stone_sunton` (Stone Sunton: m 44, a placed charcoal piebald arctic like Eskar himself, cowherd at Southgate, home Gull Rents in the city), with Eskar's own description. **The one exception to the look-alike rule:** if the Captain is beaten, he sits one game day in the Keep's cells in the city where Stone lives. It is allowed because the cells are an interior Stone never enters, the stay is one day, and the description (pale eyes, bound paws, no cloak) tells them apart; no other dark adult male arctic lives outside the Captain's towns (checked read only, 2026-11-22). Ch 9: C10 `foe.add` champion, `like` the same. If beaten: the same `visitor.add` at the Watch cells `r2_the_keep_down` for one game day (§9.2). His words come through Desmond, or by letter. | Ch 7 (one night), Ch 9 (one night, and one day at the Keep if beaten) | over the Pass, or held a day at the Keep and judged on the boards; never placed, never held as a resident |
| **Hakon** | `visitor.add` id `hakon`, `like: rm_palle_bilgemarsh` (Palle Bilgemarsh: m 35, arctic, preserver on Market Square; Hakon walks only the Greenholt ridges and the Pass and never comes to the city, and is not Ulric's double), his own description; C10 foe at the Pass | Ch 3 ridges by night, Ch 8 Pass Road, Ch 9 Pass | back over the Pass in every fate: he never "goes home with Ulric" |
| The Captain's companies, Fell bounders, House cudgels, the river company | `story.area.spawn` / `bandits.call` (anonymous bandits); C10 bands later | throughout | cleared or scattered |

**Population rule.** No resident is made, no unplaced resident is brought in, and no stranger stays. Optional J7 can only queue an unplaced resident (a turned Eskar, in a later campaign) for a post that a death has already left vacant.

### 8.6 Protection, sparing and the clean-up
- **At Ch 1's setup** (`once: campaign`): `npc.protect` and C11 `npc.spare` on:
  - Dagfinn, Ottilie, Oda, Brenna, Gerrit, Bettany, Rosalind, Oskar, Ambrose, Gorm and Cressida;
  - the nine Keepers and their fallbacks, including the two new fallbacks **Corra Coldridge** `rmx_corra_coldridge` (Ridgemere) and **Jory Corncroft** `cty_jory_corncroft` (Fenshore);
  - the post keepers: Kell, Willa, Harrow, Moss Blossomwell, Solveig;
  - Tansy and Gorse Reedmere;
  - Tamsin Eelbank, Bramble Bogditch, Halvard Timberland, Ingrid Kendrick, Piet Lumley and Fenwick.
- **Desmond Hollis is never protected,** because C10 `foe.outlaw` must be able to down him. Protection would forbid any attack on him.
- **Old age.** Until C11 exists, `npc.protect` does not stop old age. The DM sets the authored `live.npcs.protected` flag on the old for the campaign's span. **This is a DM data edit and must be listed as one**, and cleared at the end. It covers Bramble 78, Tamsin Eelbank 70, Ingrid Kendrick 68, Halvard 66, Ambrose 63, Oda 60 and Piet Lumley 60. Every step on any of them still carries a `resident is alive` guard with its fallback (§5.1).
- **Holds are undone at each run's end,** so every chapter's setup trigger re-applies the lasting holds: Desmond in the cells or the foothills, Oskar at the crossroads if he ran, Gudrun in the accounts room while her 7 days last.
- **Clean-up.** Ch 9's epilogue runs `npc.unprotect` and C11 `npc.unspare` on the whole list. The same effects form the one-phase program `ninth-howl.cleanup`. The DM publishes it if any act is stopped early, because **`npc.protect` is permanent** and is never undone by a run's end.

---

## 10. Talk plan

### 10.1 Files and memory keys
- **New library files** (loaded automatically; a server restart picks them up):
  - `Data/Talk/rules/story_ninth_howl.talk`: barks and smalltalk;
  - `Data/Talk/scenes/common/story_ninth_howl.talk`: NPC-to-NPC scenes on `day`, `smalltalk`, `news`, `gossip`, `bandits`, `lore` and `event`;
  - `say[...]` additions to `resident.tree` (`news`, `gossip`), `inn.tree` (`greet`, the recaps), `watch.tree`, `church.tree` and `official.tree`.
- **Every line is gated** on a `world.mem.nh_*` key, or on `self.id` plus a key. The test town has neither, so `talk_scorecard_quick` and `talk_golden` do not move.
- **The writer** is C8 `talk.remember`. Before C8, the gating side is already valid, and the DM can seed a key by hand.

| Key | Scope | Values | Written |
|---|---|---|---|
| `nh_stage` | realm | king, moot_called, reckoning, signals, howls, letters, lights, harvest, bell, moot | each chapter's entry and Blaze |
| `nh_voice` | town | quiet, stirring, rising, raised | Ember steps |
| `nh_howl` | town (Ch 4); realm (Ch 9) | lost, found; night, ran, broke | Ch 4; Ch 9 |
| `nh_debt` | npc (each debtor) | pressed, freed, angry, pledged, sings | Ch 1–4 |
| `nh_debts` | realm | due, eased, called, **grace** | `grace` (optional): the DM by hand if a new moon passes before the chalk night (§3.4.4); `due`: Ch 1's entry; `eased` / `called`: Ch 2 |
| `nh_lists` | realm | gathering, bettany, press, captain, sold, lost | Ch 3 |
| `nh_captain` | realm | rumour, named, parley, turned, beaten, fled, dead | Ch 1–9 |
| `nh_stone` | town southmeadow | read | Ch 4 |
| `nh_hills` / `nh_cut` | realm / town oakhaven | oakhaven, split, fell / ours | Ch 5 |
| `nh_grayrock` | realm | road, home | Ch 5 |
| `nh_wardens` | realm | coming, question, watching | Ch 5–9 |
| `nh_lights` | realm | walking, laid, turned, faded | Ch 6 |
| `nh_quarrel` | realm | fell_timber, houses_greatly, greatly_grayrock, against_crown, peace | Ch 6 |
| `nh_ford` | realm | siege, held, burned | Ch 7 |
| `nh_writ`, `nh_oskar` | realm | whispered, read / confessed, exposed, fled, paid | Ch 8 |
| `nh_bell`, `nh_mere` | realm | moving, rung, cracked, lost / still, pledged, hidden | Ch 8 |
| `nh_crown`, `nh_concord` | realm, `keep` | ottilie, lamp, regency, bought / answers, holds, bends | Ch 9 |
| `nh_moot` | realm | midwinter, nine_howls | Ch 9 at the call: the Moot's name by the season (§3.2.4) |

### 10.2 Writing rules for every line
- Write the frame in symbols (`#greet#`, `#thanks#`, `#address#`, `#oath#`, `#bc_it_is#`), so each giver's own dialect fills it.
- Write a dialect's set phrase out only for a speaker raised in that dialect. The tables in each chapter are keyed on `self.dialect`. Saltwell's only borrowed words are "wadi" and "Baba".
- **Recap lines** (one per stage) go in `inn.greet` and `resident.news`, so a late joiner can learn the story from any taproom. **Helpful lines** go in `resident.news` and `resident.gossip` at `priority=2`.
- 160 characters at most per alternative, and no ids in speech.
- No personal names in library lines: use `#world.figure_*#` (C9 keeps ruler, villain, captain and hero current) or roles: "the lamp-keeper", "the Grayrock lady".
- Weather and time words only under the matching gate.
- Never the forbidden words. Only the Bright One and the Dark One.
- **Allow-list additions** for `tools/dialect_allow.json` "places" and titles: Moot, Midwinter Moot, Moot of the Nine Howls, Moot night, Moot Feast, Reckoning, Lantern Writ, Concord, Wardens, Grey Captain, Holding Howl, Howl Line, Upper Mere, Highmere, Rainward Hills, Rain Hall, Bay Light, Pass House, Deepvein, Last Sheaf, Book of Names, Counting House, Council Press, Hundred Steps, Sunken Street, Alder Mere, Longmead, Sunfields, Antechamber, Larkholm, Kestrel.

**Two NPC-to-NPC scene samples:**
```
scene nh_ch3_lists
when topic=gossip world.mem.nh_lists=gathering
does tell.news / react.news / assess / react.self
tone serious / even / serious / even
a : There's a wolf going round asking who keeps a list of who pays the Watch. | Somebody's putting the Watch lists together.
b[quarter=sump] : About time. The Sump's paid for the Watch twice, once in coin and once in looking away. | Let them. We've nothing to hide down here but hunger.
b : Careful. Lists like that get wolves hurt. | I'd keep my head down, if I were asked.
a : #bc_it_is# | Bright One keep whoever's carrying it.

scene nh_ch9_night
when topic=day world.mem.nh_howl=ran
does tell.news / react.news / tell.self / react.self
tone warm / warm / even / warm
a : Did you hear it come in off the Bay Light? Like a wave up the hill. | I counted a hundred breaths and there it was, the far howl.
b[self.dialect=coldwater] : Glasswater answered, eh? I'd swear I heard the mere in it. | Calm water, and a howl over it.
b : I heard. I've never heard the like. | Every town on one howl. Who'd have thought it.
a : #bc_it_is# | The Bright One heard it too, I'd say.
```

### 10.3 Giver budget (shared 9-choice hubs)
- **The cross-pack giver table is `givers.tsv`. No campaign resident carries a pack quest.** Its rules are in §7.
- **No resident carries more than 2 campaign offers or chains in one chapter.** Pledges, backings, chains and quest offers all count; journal offers cost nothing.
- **Innkeepers carry none,** in any file, and are never Keepers or fallbacks. All 11 (Loam Pinefell, Bramble Reedford, Rowan Downridge, Nell Plummeadow, Oriel Marshditch, Tamsin Icefell, Thorn Ashwood, Mab Carlisle, Hollis Gullmore, Karin Ashgate and Rosamund Tarridge) give helpful lines and recaps only. Rowan (Rook's word), Nell (her letter, Ch 9) and Oriel (The Scattered, Ch 4) are each a target once here, and the medium pack uses Loam Pinefell and Bramble Reedford (buyers in `mq-coldwater-still`) and Thorn Ashwood (keeper of the shelf in `mq-lamp-oil-share`) once each; that is each one's one target role across all files. Thorn also hosts the Ch 8 confession scene, which is a scene part, not a target.
- **Gated quests are journal offers, so they cost no hub room.** That includes the late howl in Ch 5–9 (Ch 4 Failure).

| Resident | Ch 1 | Ch 2 | Ch 3 | Ch 4 | Ch 5 | Ch 6 | Ch 7 | Ch 8 | Ch 9 |
|---|---|---|---|---|---|---|---|---|---|
| Brenna | Summons, pledge | pledge | pledge | pledge, howl quest | pledge | pledge | pledge | pledge | pledge, Moot rule |
| Gerrit Mudhand | — | Moot backing | Moot backing | Moot backing | Moot backing | Moot backing | Moot backing | Moot backing | Moot backing |
| Dagfinn | round chain, Lamp backing | Question, backing | Question, backing | Question, backing | Question, backing | Question, backing | Question, backing | Question, backing | backing |
| Ottilie | — | loan vote, Blood backing | backing | backing | backing | backing | backing | backing | backing |
| Edric Kelpham | — | Purse backing | backing | backing | backing | backing | backing | backing | backing |
| Each other Keeper | pledge | pledge | pledge | pledge, howl quest | pledge | pledge | pledge | pledge | pledge, post quest |
| Cressida Cotton | Where Is the King? | — | — | — | — | — | — | — | waystation post |
| Gorm | — | — | — | Cough hand-in | Eye scene | Quickened chain | — | Tansy chain | Envoy's questions |
| Imogen Nailridge | — | — | — | The Scattered, Mirebridge pledge | Mirebridge pledge | — | — | — | — |
| Kell | — | — | — | Silver line | — | — | — | rope chain, frame | Upper Mere post |
| Skye Eelbank | — | — | — | first-turn chain | — | path chain, pledge | pledge | pledge | pledge, Mirebridge post quest (Lanterns for the Drowned is a journal offer) |

Single-chapter givers: Myrtle, Oda (Ch 1, Ch 8), Rosalind, Fenwick (Ch 4, Ch 8), Briar, Ingrid Grayrock, Maren, Solveig (Ch 5, Ch 9 post), Nan, Dagny Ropeley, Aspen (Ch 7 Runner chain, on top of her pledge), the Larkfield smith, Rook and Oskar.

### 10.4 Talking paths for the main cast (`npc.offer` chains on `dialogue choice`)
Each step is one pick, `once: true`, per wolf once J1 exists. The reply is in the resident's voice through `#symbols#`. **Bold** marks the wolf's words; the rest is the resident's reply.

- **Dagfinn's round** (Ch 1, the last step of Where Is the King?):
  1. **"Why aren't you at the Palace?"** "Because the Sump doesn't stop needing a round walked, no? The Palace has plenty of wolves. The alleys have me."
  2. **"The realm needs you up there."** "Needs a guard? Or needs a wolf the Houses can blame? I'll go up. Walk with me to the Hundred Steps." (`npc.walk`, `npc.follow`.) Or **"Then stay down here."** "Fen-sure, I might. But somebody ought to be in that room who's slept in a three-family tenement."
  3. **"What's the letter you carry?"** "It's for the Grayrock lord. Everyone's got one. Mine's a receipt, and the old King wrote on the back of it. He's never home to read it." (Plants Ch 5.)
- **Dagfinn's Question** (Ch 2–8):
  - **"You should stand at the Moot."** "Should I? I'd be a lamp with a crown on. Ask me again when you've seen more of it, no?"
  - **"Keep the lamp. Let others stand."** "That's what I'd do, left alone. #thanks# for saying it."
- **Ambrose** (Ch 2 on):
  1. **"What do you want?"** "A city that pays what it owes. You'd be surprised how few wolves want that."
  2. **"And the howls?"** "A voice is only a debt with a tune to it. I collect debts."
  3. (Ch 7 on, if Ambrose's lie is known) **"You promised the Captain Deepvein."** "I promised him a deed. I never said whose name would be on it."
- **Ottilie:** **"Do you want the crown?"** "I want the petitions answered. If that needs a crown, I'll wear one. If it needs a guard from the Sump, I'll carry his petitions too."
- **A Keeper's pledge** (shown for Brenna; each Keeper's in their own voice): **"I'll howl for Ridgemere."** "Then Ridgemere's howl is yours to carry as well as mine. #thanks#. Walk it far."
- **Bramble Bogditch:** **"Teach me the howl."** "#greet#, my child. Sit. The well remembers who drew from it, and the howl remembers who sat still. Come back on Restday." (Three Restdays, three replies; the last: "Now howl it for me at the pool. My voice is old; yours will carry.")
- **Gorm** (Ch 5–8):
  1. **"Why did you write to them?"** "In Saltwell, when I was young, a pup the water obeyed drowned a well and herself with it. No one had taught her. I wrote so it would not happen here."
  2. **"Would you speak for ours?"** "If they are taught, yes. I cannot speak for what is hidden from me."
  3. (Ch 8) The reply turns on `nh_b_tansy`: taught: "Then I will say this realm keeps its own"; hidden: "Then I have nothing to say, and I will say it carefully".
- **Ulric** (Ch 3 → Ch 8):
  1. **"Whose scent is it?"** "That's my brother's scent. Older and smokier, but his. He died in the deep winter. Didn't he?"
  2. (after the records) **"There's no Deepvein name on the list."** "Not one. Somebody barred a door, and somebody wrote it down after."
  3. (Ch 8) **"Fenwick said it."** "Then Frosthollow knows. If he's alive, I'll stand on the Pass with a lamp lit."
- **Lady Maren** (a toll-receipt holder, Ch 5):
  1. **"Your husband's wagons pay the Captain."** "My husband trusts contracts more than wolves. Let him see one he can't refuse."
  2. **"Then stop it quietly."** "Quietly. Yes. And two ships for whatever the realm needs next. That is a contract too."
- **Oskar** (Ch 8): §6, Chapter 8.
- **The Envoy, through Gorm** (Ch 9, the first three wolves): **"What kind of realm is this?"**
  - honest, "A realm that paid the Watch to look away, and is learning to stop.": "She writes that down. She writes slowly when she likes an answer." (+1)
  - proud, "The best realm on the map.": "She writes nothing."
  - evasive, "Ask the Houses.": "She says she has."

### 10.5 Budgets per program (limits from the format)

| Limit | Cap | This design |
|---|---|---|
| Quests per program | 32 | ≤ 14 per chapter; repeatables go to packs |
| Triggers per phase (including campaign triggers) | 64 | about 9 Ember + 6 shared + 25–40 chapter triggers (readiness is a phase block, not a trigger) |
| Offers and choices per program | 64 | 9 pledges + 4 backings + 2 Question + ≤ 30 chapter chains |
| Variables per program | 64 | about 44–62 with the readiness variables (4 a chapter: `ch<n>_did`, `ch<n>_did_n`, `ch<n>_auto`, `ch<n>_wait_bar`). Ch 9 reuses 4 tally numbers per town. With J2, the 9 pledge lists drop out. |
| Trackers per wolf | 16 | 4 (+1 per pack at most) |
| Scenes | 32 | Ch 9: the Moot, 4 endings, the Turn, the bell, Nell's letter = 8 |
| Regions | 128 | Ch 5: 12 bounds + 6 others; Ch 9: 13 posts + 4 |
| Entry effects | 12 | setup work runs in a `story phase` trigger |
| `each` turns | 500 | only tiers and the Ch 9 town tally; C3 / J11 replace them past 500 wolves |

---

## 11. Trigger features: existing, gaps and new

### 11.1 Map of every need

| Need | Chapters | Status | Fallback today |
|---|---|---|---|
| Campaign chapters with carried variables | all | **Existing** (`campaign.approve`, `story.start`, `carry`) | — |
| Shared bars, strands, races | all | **Existing** (world and story variables, story-scope quests, trackers) | — |
| Goals that grow with the wolves who play | all | **Existing** (the `ch_helpers` workaround); better with **C2** | works now |
| Readiness: the active count | all | **C2** (`audience`, `activeDays`, the seeded `seen`; Phase 1a) | the Campaigns list's wolf count, by eye (§3.2.5) |
| Readiness: the share, the minimum time, `auto`, the soft maximum | all | **R1**, **R2** (`ready` block, `slowAfterDays`; R2 + `slowNeedsYou`; R1's floor never above the active count, decisions §6.2) (Phase 1a) | a `dm signal` direction exit and the DM's eye (§3.2.5) |
| An admin push, logged with a snapshot | all | **R2** command `advance`, **R3** Advance… / Advance anyway… (Phase 1a) | the direction exit, audited as any command |
| Camps, waves scaled by wolves present, bounties | 1, 3, 5, 6, 7, 8, 9 | **Existing** (`story.area.spawn`, `count in region`, `camp cleared` as a trigger, `bandit falls`); named story camps and `camp cleared` as a mission event are **L2 / L3** (Phase 2) | match `cell` plus a `spawned_<region>` story switch; `objective.complete` from the trigger (§6 conventions) |
| Story coin from shops' tills | 2, 3 | **SQ2** (Phase 1: `check_purse` accepts `till:job:<id>`) | the treasury, worded as the shop paying, under the counters (§2.11) |
| Purse day caps and floors | all | **Existing** (`day begins`, `purse crossed`, world numbers) in the program `nh-purses` | — |
| Night ridges, dusk choruses, timed windows | 3, 4, 7, 9 | **Existing** (`hour`[w], `calendar is`, timers, `schedule`) | — |
| Residents walking, holding, following, scenes | all | **Existing** | — |
| Per-wolf offers that don't overwrite each other | all | **New J1** | untargeted offers with `once: true` |
| Gated quests that don't leak | all | **Existing** (journal `quest.offer`); **C6** `requires` is nicer | journal offers |
| A giver's line while a quest is under way | all | **Gap C6** (`offer.during`) | none (journal text) |
| Campaign gossip, helpful knowledge, recaps | all | **Gap C8** (`talk.remember`, `belief.plant`) | the DM seeds keys by hand; deeds and figures still play |
| Clearing a false reputation | 1, 3 | **New J8** (`belief.forget`) | `belief.plant` (C8) of the true claim |
| Ruler, villain, captain, hero figures | 1–3, 9 | **Gap C9** (+ a wolf as hero) | DM `save_figure` at each stage |
| Residents thanking one wolf by name | all | **Gap C5** (`npc.greet`) | `deed.award` gossip |
| Ranks and titles | all | **Gap C3**, **C4** | fixed tiers by `each`; deed phrases that the nickname families pick up |
| Names and numbers in boards and banners | 5, 7, 9 | **Gap C1** | pre-written posts per band or threshold |
| Key cast kept from old age | all | **Gap C11** | authored `protected` flag (a DM data edit) |
| Moon timing for set pieces (optional, within 3 game days) | 1, 2, 4 | **New J4** | the season day against §3.3's table; DM `shift` |
| Howls as story events; residents howl | 4, 7, 9 | **New J5** | `count in region` plus the Keeper present (`resident in region`) |
| Residents counted at the haul | 8 | **New J6** (optional) | a ladder of 7 `resident in region` conditions |
| A named villain who can be fought; a resident downed, never killed | 7, 9 | **Gap C10** (`foe.add`; `foe.outlaw` for a resident) | the Captain's band as bandits; Desmond kept offstage |
| The beaten Captain held after the fight | 9 | **Existing** (`visitor.add` at `r2_the_keep_down` for a game day, `board.post`) | — |
| Packs credit a wolf in the running chapter | all | **Gap C12a** (`story.notify`, event `story notified`) | world variables, polled hourly |
| Deed matching by prefix or list | 5, 6 | **Gap C12b** (`match` / `field` with `starts` / `in`) | `caravan arrives` only |
| Wake on a world variable change | all | **Gap C13a** (`variable changed`) | hourly poll |
| A resident's words to a wolf, not in a banner | all | **Existing** (offers, chains, `npc.line`, scenes, `say[self.id=…]`); **C5** `npc.greet`, **C6** `offer.during` | signed `board.post` plus the speaker's own tree line (§2.1) |
| Gift and Quickened facts | 4, 6, 8 | **New J10** (`wolf has`) | offer to all, saying "if you carry a Gift" |
| Unique story items (the list, the letters, the writ) | 3, 5, 8 | **New J9** (tagged items) | generic `paper` / `letter`, plus per-carrier lists |
| Finished side stories count toward the Quickening (answer (b), capped) | all | **New J3** (`closure: "side"`, at most 4 a chapter) | a journal note inviting a social Story; the claim is never made in game text |
| Per-wolf state across acts | Act II, Act III | **New J2** (`scope: "realm"`) | world `wolves` lists (§4.4) |
| A tally over 500 wolves | 9 | **New J11** (`tally` operand) | `each` (≤ 500) |
| The ruler's post changes at the end | 9 | **Gap C14**, as one atomic swap only | talk, a figure, and a sequel line |
| The turned Captain fills a vacancy later | after 9 | **New J7** (optional `npc.queue`) | talk only |
| The Wardens' events (`told` / `vouched to the wardens`) | 5–9 | **Existing**, after the DM creates `warden_order` with Gorm | — |
| Mantle story voices | — | **C15 not needed**: `say[self.id=…]` lines are used instead | — |
| Run-time casting | — | **C7 not needed**: fixed lists from the data | — |

### 11.2 The new items (J1–J11), in the catalogue's style
- **J1, `npc.offer` keeps a `players` list.** In `story_quests.py:706-710` the runner appends the wolf to `players` instead of replacing `player`. The game already reads `players` (`storyChoicesFor`). Small.
- **J2, realm-player variables.** `scope: "realm"` holds one value per wolf in `dm.story_world_vars`, keyed by name and wolf, shared by every story that declares it. It works with `of: $actor|$each` and with trackers.
- **J3, `closure: "side"` on a campaign side story** (`"line"` on a large line's last chapter; answer (b)). When it completes for a wolf, the game writes one `story_closure` receipt for that quest. That counts toward `closedStories` (`RatwGameSocial.cpp:970`). At most 4 closure quests per program, one receipt per quest per wolf, and the game refuses a fifth per wolf per program. A chapter with 5 side stories marks 4; main lines and door quests carry none.
- **J4, `calendar is {what: "moon", value: new|waxing|full|waning}`.** From `LunarCycleDays`. Optional watched event `moon`[w] on each change.
- **J5, howls in stories.**
  - Watched events `howled`[w] (`actor`, `cell`, `x`, `y`, `item` = town or `wild`) and `chorus closed`[w] (`actor` = first howler, `quantity` = howlers including residents, `item` = town), both written from `Game::howl` and chorus close in `RatwGameHowl.cpp`.
  - The effect `npc.howl {npc}`: a resident howls, opening or joining a chorus, with hearers and town memory as for a wolf.
- **J6, `count in region {region, op, value, residents: true | list}`.** Counts residents, or the residents of a list, in a region.
- **J7, `npc.queue {npc, town}`** (optional). An unplaced resident is first in line for the next post that falls vacant in that town by the normal rules. It never creates a vacancy. Lasting.
- **J8, `belief.forget {subject, kind, town?, share 1-100}`.** Lowers residents' confidence in one claim kind about one subject.
- **J9, tagged story items.** `item.give {player, item, quantity, tag, name ≤ 60}`. `gift`, `item picked up` and `carries` can match `tag`. Undone at the run's end unless `keep`.
- **J10, `wolf has {what: gift|family|tier, value}`.** A fact condition on the wolf's Gift.
- **J11, the operand `{"tally": {"var": …, "value": …, "weight": …, "over": "participants|list"}}`.** A weighted count with no 500 cap.
- **Readiness** is engine-spec's R1 (participation), R2 (the `ready` block, the `ready` exit, the command `advance`, the alerts `ready` and `slow`) and R3 (the console's bars and Advance), with C2's `activeDays`, all in Phase 1a. This design asks two small changes of them (decisions §5.2, §6.2): R1's floor never asks for more wolves than are active, and R2's `slow` alert can count as "needs you" (`slowNeedsYou: true`), worded here "ready to push".

### 11.3 Build order by act
The engine phases are engine-spec §9's. There are no dates: each act waits for the DM (§3.1, §3.4).
1. **During Act I: Phases 1–3. Act I needs none of them** (it ships on §11.4 whenever the DM publishes it).
   - Phase 1a first (C2, R1, R2, R3: readiness): when it lands, the running chapter moves onto its `ready` version by `migrate`, mid-chapter if need be (§3.2.5).
   - Then Phase 1b (V1 with the `alive` check, SQ2 tills, J1, C8 with the pair scope, C9, C11, N2): when it lands, the DM moves the running chapter to a new version at the next chapter boundary.
   - Then Phase 2 (C1, C12b, J4, L2 / L3 named camps) and Phase 3 (C5, C4, C3, J11, J8, V2).
   - C3 and C4 can come later: titles fall back to deeds.
2. **Before Act II (published when Act I's last chapter has closed and the DM chooses; if its engine is not landed and tested by then, the interlude waits or Act II ships on its today-only rows):** Phases 4–6: J3, C12a, C13a, C6, N1, J10, J5 (the events first, `npc.howl` second), J6, J9.
3. **Before Act III (published when Act II has closed, on the same rule):** Phases 7–8: C10 `foe.add` / `foe.outlaw`, J2 (or keep the list fallback), C14 (the ruler swap only), C9's wolf hero; whatever is missing falls back to §11.4 rows 7–9.
4. **Later:** J7 (for campaign two).

### 11.4 Today-only version (no engine change at all)
**Act I ships on this version** (§3.4.2), with the packs' today-only versions and the stage pack's stage 1–3 quests. Across every chapter it means: untargeted offers with `once: true`; DM-seeded talk keys; DM `save_figure`; the authored `protected` flag on the old cast (a listed DM data edit); camps matched by `cell` and `spawned_` switches; treasury and town-store coin only, under the counters. Rows 4–6 are Act II's fallback if its engine is not ready when Act I closes, and rows 7–9 Act III's. Before Phase 1a, every row is paced by the stand-in of §3.2.5 (a direction exit and the DM's eye).

| Chapter | What runs | What is lost |
|---|---|---|
| 1 | All quests, the camp, the posts, the scene, pledges (untargeted offers) | town gossip (C8), figures (by the DM instead), named thanks, old-age safety (DM flag instead), the moon test (the season-day table instead), readiness bars and Advance before Phase 1a (the direction exit instead) |
| 2 | All of it; debtors through `in list` | debtor talk keys, named thanks |
| 3 | Lamps, waves, lists, the Desmond hold, Lorn | belief clearing (Ingrid Kendrick only gets the deed); the list is a plain `paper` |
| 4 | Howl quests by the Keeper-present chorus; the relay by regions; the Stones; the Well; the Silver | real howl events; the full-moon test (the season-day table instead) |
| 5 | Bounds, letters (generic `letter` items with per-carrier lists), Gudrun, the homecoming, the Envoy's walk | tagged letters, the top-wolf pick (the first-three rule instead) |
| 6 | The Book, the lights, the rite, the post, the quarrel, the cache | the Gift gate (offered to all) |
| 7 | The siege, the Runner (regions), the Door, the parley, the trial | the named foes (anonymous bands), the Runner's howl |
| 8 | Both threads; the haul by the `resident in region` ladder | the tagged writ; closure receipts |
| 9 | The run by regions and keepers present; the Moot; the counting by `each` (≤ 500 wolves) | audible resident howls; the fightable Captain (bandits instead); the ruler swap (talk and figures instead) |

### 11.5 DM preparation (before Act I unless noted)
- `world-var declare` for every variable in §4 (with §4.5's counters and switches), with notes.
- Publish `nh-purses` (§2.11) before Act I, and leave it running.
- Optional: if a new moon passes before Ch 1's chalk night, seed the realm key `nh_debts=grace` by hand (§3.4.4).
- Per chapter, turn on `ch<n>_auto` or `ch<n>_wait_bar` where the defaults do not suit the world (§3.2.1).
- Look at the Campaigns list once a real day: answer each "ready to push" with Advance anyway… (a reason and the typed title) or let it wait. Before Phase 1a, send each chapter's direction by the stand-in rule (§3.2.5).
- Until C8, seed each chapter's talk keys by hand at its entry and Blaze (`nh_stage`, and the keys §10.1 lists), including `nh_stage=reckoning` for Desmond's Ch 2 window line.
- Keep Dagfinn's "reigning" post as it is. **Do not "fix" it.**
- Fill `live.figures` by `save_figure` at each stage until C9 exists.
- Set the authored `protected` flag on the old (§8.6) until C11. It is a data edit; clear it at the end.
- Before Ch 5's end: create faction `warden_order`, with Gorm Plankgate a ranked member.
- Make the authoring lists from the data, all placed and alive:
  - debtors (adults), weep witnesses, letter carriers, list keepers, petitioners;
  - the believers of Ulla Rusthand's and Ingrid Kendrick's thefts (`game.beliefs`);
  - the three oldest residents of each town;
  - the Mirebridge-blooded.
- Add the words in §10.2 to the allow list.
- Re-post any Line project that lapsed (posts 2, 10 and 12 already stand open: do not post them again). Check each town purse before the Moot grants; the counters pay in kind past the cap.

---

## 12. Risks and checks

- **Approval misses three things** (story-format survey §9):
  - objective `match` ids: check every id is a placed resident;
  - `character.grant` purses: `town:<id>` maps to `stores:<id>` (212–248 pennies at the checkpoint: coldwater 212, fenshore 213, saltwell 224, southmeadow 236, oakhaven 247, frosthollow 247, larkfield 248), `town:ridgemere` to the treasury (1,355). **No till is named before SQ2**: today `check_purse` refuses `till:job:<id>` and `till:<id>` names no account, so the Greatly till `till:job:rm_eleanor_saltcot` (161) is used only by a new version after SQ2. `town:<id>:works|watch|church` do not exist. Every coin reward is at most 10 from a town store and 20 from the treasury, goes through the counters (§2.11) and uses `ifRefused: skip`;
  - `board` towns: only the eight.
- **Unplaced and dead residents** pass approval and fail at run time (V1 adds the `alive` check in Phase 1). Only `cty_eskar_rimedrift` is unplaced here. He is never given a body: he appears only as a stranger or foe dressed `like: cty_stone_sunton`, and his name appears only inside descriptions and narration. The dead `rm_gunnar_harrowgate` and `tn_cw_iris_greywater` are named, never targeted.
- **Look-alike risk.** Stone Sunton lives in the city (`r2_gull_rents`) and works at Southgate, so on the one game day a beaten Captain stands at the Keep's cells, a wolf could meet his double in the streets. The decisions keep this look-alike; the Keep visitor is mute, bound, and gone after a day.
- **One live run per program id.** The packs, the cleanup and every chapter have distinct ids. A chapter stopped mid-run can't be relaunched while its run is live.
- **Holds and doors are undone at each run's end.** The re-apply list is in §8.6.
- **Every rule against the world** holds throughout: wolves and pups, the Bright One and the Dark One only, "private messages", male or female, no ids in speech, reachable interiors only. The Rain Hall stays sealed.
- **Checks before approval** (decisions §9, P2-24; each run and its output listed in the approval note):
  - `story_program.review(p)` passes with nothing uncovered; `story_program.problems(p) == []`; `story_runner.references(conn, 'upper_accord', p)` passes on R3, for every chapter, `nh-purses` and `ninth-howl.cleanup`;
  - the §1 resident query of `decisions.md` (placed, alive, home and work reachable) re-run read only for every resident this file names;
  - the giver check of `decisions.md` §1.2.7 over `givers.tsv` (one pack quest per resident; none on campaign cast, keepers, innkeepers or look-alikes);
  - `tools/dialect_check.py --world ridgemere3`; `build-core/talk_check --data "$PWD/Data/Talk" --validate` and `--render-trees`; `python3 tools/talk_lint.py`;
  - by search: no `story.tell` with `from` carrying a resident's words; no `npc.speak`; no `camp cleared` matched on `camp_w_ridgemere_<cell>` for a story wave; no purse other than the nine of §2.11;
  - one rehearsal case per phase, including every worse road and every ending, with `expectChapters` for the chains;
  - by eye: every offer `say` and `thanks` for symbols and dialect (no tool scans story programs);
  - the same talk checks on every new Data/Talk file (gate values spelled out, `|` alternation allowed; no "…").
- **The risk of length, and of stalling.** The campaign has no fixed length (about 24 real days at 200 wolves, 30 at 5; §3.2.3). A quiet world never strands it: the floor is never more than the active wolves, the soft maximum raises "ready to push", and the admin can push any chapter at any time (logged). A crowd never rushes it: every chapter has a minimum time. Nothing is tied to a date, so a late start or a long pause changes only the season, and with it the Moot's name (§3.2.4).
- **Readiness checks before approval:** a rehearsal case per chapter for each way to close: readiness with 1, 5 and 200 active wolves (R1's `activeWolves` test key); Advance anyway before the minimum time; the soft maximum with `ch<n>_auto` off (the `slow` alert) and on (it moves); `ch<n>_wait_bar` on; and, in Ch 9, a winter and a non-winter call. Each with `{"check": "ready"}` steps and `expectEffects` for the board post and the close.
