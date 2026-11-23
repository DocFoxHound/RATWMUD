# The Sump Crown: the first grand campaign of Ridgemere 3 (designer "crown")

Angle: political. The succession crisis: a commoner on the throne by accident, the four Houses circling, the debts falling due, the Watch bribe lists. The main storyline asks one question all realm long: **who holds Ridgemere?**

World: Ridgemere 3 (`ridgemere3`, db `ratw_ridgemere3`, world id `upper_accord`). Every resident, place and faction id below was checked against the survey tables (`/tmp/castsurvey/res2.tsv`, `/tmp/lore_survey/reachable.txt`). Every interior named is on the reachable list. No new residents are made.

---

## 0. The campaign at a glance

| | |
|---|---|
| Campaign id | `sump-crown` (chapter programs `sump-crown-1` … `sump-crown-8`, approved together with `campaign.approve`) |
| Audience | `everyone`, `days: 30`, `cap: 5000` for every chapter; each chapter ends with `story.start {audience: "own", carry: true, when: "ended"}` |
| Span | Spring day ~70 to Midwinter (winter day 46 = game day 321), about 250 game days. Each chapter has a goal *and* a deadline, so a quiet world still moves on. The DM shortens it with `shift` from the Campaigns console. |
| Main line | The work of reigning fell by the vacancy rule to a Sump patrolman. Eight chapters lead to the Long Night Moot, where the realm decides who holds the crown. |
| Endings | The Sump Crown (Dagfinn crowned), The Harrowgate Restored (Ottilie reigns, Dagfinn her shield), The Council Crown (a Houses' regency), The Counting-House Crown (Ambrose Greatly as Lord Protector: the dark ending, which opens campaign two) |
| Side storylines | 3 to 5 per chapter, 30 in all; offered by choices, by place, and by the wolf's own backing |
| Recognition | personal tracker, chapter tiers, top-three crier calls, campaign titles, residents who thank a wolf by name, the realm's hero figure |

Chapters:
1. **The Bell for Gunnar** (mourning, and the accident found out)
2. **A Guard in the Antechamber** (the first claims; the Council's interim word)
3. **Lists in the Dark** (the Watch bribe lists)
4. **By the New Moon** (the Greatly debts fall due)
5. **Letters for an Absent Lord** (the forty letters; Lord Grayrock brought home)
6. **The Rainward Quarrel** (the Houses fall out before winter)
7. **The Chancellor Wept** (the King's lost writ)
8. **The Long Night Moot** (every town's voice; the crown decided)

---

## 1. Premise: why now

King Gunnar Harrowgate (`rm_gunnar_harrowgate`), Sump-born, died of old age on day 41 in the royal bedchambers (`r2_the_king_s_residence_up`). On day 46 the game's vacancy rule gave "reigning at the Great Palace" to the first local wolf who could take it on: **Dagfinn Thornham** (`rmx_dagfinn_thornham`), 31, a calm russet arctic wolf with a notched left ear, raised on the Fenshore boardwalks, who was walking the Sump patrol at the time. His work label still says "on patrol in the Sump". The Queen (`rm_oda_harrowgate`) and the King's daughter (`rm_ottilie_harrowgate`) were passed over.

The campaign writes the accident in on purpose. Everything else the residents already believe makes it explosive:
- 41 residents owe a House factor money "by the new moon", **including the Captain of the Watch** (`rm_corra_slagmore`), the herald (`rmx_brenna_abbot`) and the Greatly steward (`rm_edric_kelpham`).
- 35 keep "a secret list of which Houses pay the Watch to look away": the Crown's treasurer (`rm_karin_stanmore`), the keeper of the cells (`rm_desmond_dripcot`), the abbot (`rm_lionel_ropestack`) and an Antechamber usher (`rmx_desmond_farrow`) among them. And 41 Watch posts sit on ground claimed by both Grayrock and the Watch.
- 40 carry "a letter meant for Lord Grayrock, who is never home", Dagfinn himself among them.
- 46 "once saw the Chancellor weep in the Rain Hall and told no one".
- 38 believe "the Houses will fall out before next winter". Corwin Fell wants the Rainward Hills; Gudrun Fell writes secretly to Timberland.
- 56 lost a brother on the wharf and do not forgive House Grayrock.

**The hidden truth** the campaign reveals in Chapter 7: in his last month Gunnar wrote a writ, the **Lantern Writ**, and had it read in the Rain Hall before the Chancellor of Ridgemere (`rm_oskar_gullworth`). It says that when he dies, "the work of reigning passes to the next wolf who keeps the peace of the Sump, to hold it until the realm calls a Moot on the Long Night: the Houses, the towns, and the free wolves of the roads." Oskar wept because Ambrose Greatly held his debts and told him to lose it. He could not burn it, so he hid it. The "accident" was the old King's wish, written by a Sump wolf who did not trust the Houses to choose. And the "free wolves of the roads" are the players: the writ gives them a voice at the Moot.

So the campaign has an honest core: the players are not helping a faction win. They are the free wolves the old King asked to choose.

---

## 2. Design pillars

1. **One question, many hands.** Every chapter's shared goal feeds two realm meters, **the Crown's Hold** (`crown_hold`, 0–100) and **the Houses' Grip** (`houses_grip`, 0–100), plus the four backings. Players see both as trackers.
2. **Always a difference.** Every deed gives an instant personal answer (a tracker tick, a resident's reply, a journal note), and every quarter of a shared goal changes the world visibly: a board notice, a crier call, residents' talk, an event, a resident's walk.
3. **Lone wolf and crowd.** Each shared goal is `max(floor, per × active wolves)` (needs C2), and is split by town or by keeper so a crowd spreads out. A lone wolf always has a personal storyline that ends well on its own and is named back to them.
4. **Failure never blocks.** Shared goals have deadlines that move the story on with a worse flag, never an ending. Personal quests fail per wolf and come back the next day or through another giver.
5. **Pre-written, in dialect.** All talk is written ahead: quest offers in the giver's voice through `#symbols#`, story window lines per resident gated on `self.id` and campaign memory, and gossip per town dialect gated on `world.mem.crown_*`.
6. **No new population.** The cast are placed, living residents. The only newcomers are the Concord's assessor and her clerk (story strangers, Chapter 8) and the bandit bands (story foes). They come and go.

---

## 3. Variables and meters (all chapters; `carry: true`)

**World scope** (`dm.story_world_vars`, shared with the quest packs):

| Variable | Type | Shown as | Meaning |
|---|---|---|---|
| `crown_hold` | number 0–100, initial 30 | "The Crown's hold" | how well the Crown, whoever wears it, keeps the realm |
| `houses_grip` | number 0–100, initial 50 | "The Houses' grip" | how much the Houses rule from behind the Crown |
| `back_sump` / `back_blood` / `back_council` / `back_purse` | number | (shown in Ch2 and Ch8) | the four backings: Dagfinn, Ottilie, a Council regency, Greatly |
| `crown_seat` | text: dagfinn, ottilie, council | — | who sits after the Council's interim word (Ch2) |
| `lists_fate` | text: bettany, press, captain, sold, lost | — | where the bribe list went (Ch3) |
| `debts_broken` | switch | — | Rosalind's ledger laid before the Council (Ch4) |
| `ice_silver` | switch | — | the Crown's silver brought up from Highmere (Ch4) |
| `aldric_home` | switch | — | Lord Grayrock came home to sit (Ch5) |
| `wharf_answered` | switch | — | Aldric answered for the wharf dead (Ch5) |
| `rainward` | text: fell, split, crown, war | — | the Rainward settlement (Ch6) |
| `gudrun_letters` | text: delivered, shown, burned | — | Gudrun's Timberland letters (Ch6) |
| `writ_found` | switch | — | the Lantern Writ recovered (Ch7) |
| `oskar_fate` | text: confessed, exposed, fled | — | the Chancellor (Ch7) |
| `forged_roll` | text: none, used, exposed | — | Philippa's forged kin-roll (Ch2, Ch7) |
| `voice_<town>` | text per town: sump, blood, council, purse, none | — | each town's voice at the Moot (Ch8), towns ridgemere, coldwater, larkfield, southmeadow, fenshore, saltwell, oakhaven, frosthollow |
| `mirebridge_lit` | switch | — | lanterns at the Mirebridge crossroads (Ch8) |

**Player scope** (carried): `crown_merit` (number, shown "Your service to the Crown"), `crown_backs` (text: sump, blood, council, purse, none), `ch_merit` (reset each chapter, for the chapter's ranks), and one switch per side storyline taken.

**Scaling** (C2 `audience` operand plus `multiply`): `goal = max(floor, ceil(per × A))`, with A the wolves active in the last 7 days. The table gives each chapter's goal at 5, 50 and 200 active wolves. Until C2 exists, the floor is the goal and the DM raises it with `set` from the Campaigns console.

| Ch | Shared goal | floor | per | 5 | 50 | 200 |
|---|---|---|---|---|---|---|
| 1 | the Long Howl (vigil deeds) | 15 | 3 | 15 | 150 | 600 |
| 2 | the Crown's standing (backing deeds, any side) | 20 | 4 | 20 | 200 | 800 |
| 3 | the Clean Watch (list pieces; one per keeper per wolf) | 12 | 2 | 12 | 100 | 400 |
| 4 | the Debt Jar (pennies given or earned for debtors) | 300 | 40 | 300 | 2,000 | 8,000 |
| 5 | the Letter Bag: 40 letters, fixed; the Lord's Road guarded | 10 | 3 | 10 | 150 | 600 |
| 6 | the Rainward Peace (patrol, escort, feuds settled) | 20 | 4 | 20 | 200 | 800 |
| 7 | the Testimonies: 23 of 46 witnesses, fixed; the Writ search | 1 | — | 23 | 23 | 23 |
| 8 | the Moot voices: per town `max(4, ceil(0.5 × A_town))` | 4 | 0.5 | ~4 each | ~4–12 | ~4–40 |

The fixed goals in Ch5 and Ch7 (forty letters, forty-six witnesses) are deliberate: a crowd *races* for unique pieces there, so every piece is credited to one wolf by name, and the scaled half of the chapter (guarding the road, searching for the writ) soaks up the rest of the crowd.

**Quarter beats.** Every shared goal fires world changes at 25, 50, 75 and 100 percent through a campaign trigger on `variable changed` (C13), or on `hour` polling with `compare` today. Each beat is pre-written in the chapter section.

---

## 4. The cast

All are placed, living residents. Key cast get `npc.protect` (so they cannot be attacked or killed by story effects) and `npc.spare` (C11, so old age cannot take them during the campaign) at the start of Chapter 1; both are undone at the campaign's end. Where C11 is missing, every chapter branches on `resident is <npc> alive` with a fallback giver.

### 4.1 Heroes and the claimants

**Dagfinn Thornham** `rmx_dagfinn_thornham` (m, 31; Fenshore-raised, speaks mirebridge; home `rm_thornham_house`). *The Sump King.* Calm, plain, uneasy in silk; keeps walking his old Sump round at dusk because "the alleys don't stop needing it." Carries his own unopened letter for Lord Grayrock. Motive: to do right by the Sump without becoming the Houses' puppet. Voice: short sentences, fen wryness ("Fen-sure", ", no?" when his accent allows). Arc: hiding on his round (Ch1) → standing up in the Antechamber (Ch2) → choosing whom to trust with the lists (Ch3) → learning the writ was meant for someone like him (Ch7). Fates: crowned King (Sump Crown); named Captain of the Watch and the Queen's shield (Harrowgate Restored); back on the Sump round, honoured, a "King for a season" (Council Crown); in the gaol cells `r2_the_keep_down` on a Greatly warrant until wolves free him (Counting-House Crown, which carries into campaign two).

**Ottilie Harrowgate** `rm_ottilie_harrowgate` (f, 39; Ridgemere, plain). *The blood heir.* Calm, warm, coughs in the damp; carries the Crown's petitions to the City Office herself every week. Motive: the petitions, not the crown; she wants a Crown that answers its people. Arc: the forgotten daughter (Ch1) → the petition-bearer whose cause wins hearts (Ch2) → the heir who must decide if she will take the crown from a good wolf (Ch7–8). Fates: Queen regnant; Chancellor of Petitions under King Dagfinn; a Council pensioner, still carrying petitions; under house watch in the Residence (dark ending).

**Cressida Cotton** `rmx_cressida_cotton` (f, 19; Frosthollow-raised; Sump patrol). *Dagfinn's patrol partner.* Frank and cold to strangers, loyal to him. Dreams of a Grayrock ship "bound somewhere dry". The first quest-giver of the campaign. Fates: Dagfinn's sergeant; signs on the Grey Tern if Dagfinn falls.

**Sergeant Bettany Nailley** `rm_bettany_nailley` (f, 26; plain). *The honest Watch.* Ambitious, warm, hates Grayrock over a drowned brother. The clean-Watch hero of Chapter 3. Fates: Captain of the Watch if the lists reach her; demoted to the Harbour Gate if the lists are sold.

**Gerrit Mudhand** `rm_gerrit_mudhand` (m, 55; plain). *Chancellor of the Council.* Chatty, warm, mourned the King, saw the Chancellor weep. The neutral broker; the Council regency's face. Fates: Lord Chancellor (Council Crown); keeper of the Moot's roll in every other ending.

**Rosalind Greatly** `rm_rosalind_greatly` (f, 22). *The whistleblower.* Celestine's heir; has "read every ledger in the Counting House and found one that does not add up." Fates: brings the ledger to the Council and breaks her father's hold; or is sent to her mother's Saltwell kin, where wolves can find her in Chapter 8.

**Sergeant Dagny Ropeley** `rm_dagny_ropeley` (f, 42), **Abbot Lionel Ropestack** `rm_lionel_ropestack` (m, 46), **Solveig Tarwell** `rm_solveig_tarwell` (f, 44, the Chapel of the Tide), **Brenna Abbot** `rmx_brenna_abbot` (f, 52, the herald): allies and quest-givers.

### 4.2 Villains

**Ambrose Greatly** `rm_ambrose_greatly` (m, 63; plain; Greatly House and the Counting House). *The prime villain.* Made Greatly first "with loans the others could not refuse"; "means his heir to inherit a city, not a fortune." He holds the Captain's debt, the Chancellor's debt and half the Sump's. He never raises a paw himself; his factors call in debts and his coin hires bandits. Motive: a Greatly Protectorate behind a weak or empty crown. Fates: humbled on the Council (debts broken); unbroken, he becomes Lord Protector (dark ending). He is protected throughout: he is beaten by ledgers and votes, never by teeth. He is the realm's villain figure from Chapter 4 (C9).

**Oskar Gullworth** `rm_oskar_gullworth` (m, 40; plain; the City Office). *The weeping Chancellor.* Sump-born climber, cold and guarded, the wolf who lost the writ. Not wicked: owned. Fates per branch: confesses to the Council and keeps his seat as a humbled wolf (crown_hold +10); exposed at the Council Press and removed from his post (C14); flees by the Fen Road and is last scented at the Mirebridge crossroads.

**Desmond Hollis** `rmx_desmond_hollis` (m, 33; Coldwater-raised; no post). *The Grey Captain.* A Palace guard dismissed when the household was reshuffled after the succession; sly, reserved, also saw the Chancellor weep. He blames the Sump King and takes Greatly coin to lead the road bands. A **fightable foe** through C10 (`foe.add` with `npc: rmx_desmond_hollis`, downed, never killed): Chapter 5 on the Glasswater roads, Chapter 8 at the Moot. Fates: held in the cells and tried at the Magistrate's Court (`rm_the_magistrate_s_court`); or, if he is never beaten, he becomes the Lord Protector's captain.

**Queen Oda Harrowgate** `rm_oda_harrowgate` (f, 60; Coldwater-raised). *The schemer who is not a villain.* Restless and sly; counts the purse coin by coin. She would sell the crown to Greatly for the household's debts, or fight for her daughter, depending on what wolves show her. Fates: Queen Mother; or Ambrose's ally in the dark ending.

**Philippa Coldby** `rmx_philippa_coldby` (f, 41; the magistrate's clerk). A forger who offers Dagfinn's backers a "kin-roll" proving him Gunnar's Mirebridge cousin. A temptation, not a boss.

### 4.3 Key residents by House

- **Greatly**: Celestine Greatly `rm_celestine_greatly` (Saltwell-raised salon host; information broker; protected), Julian Greatly `rm_julian_greatly` (wants to go to sea), Edric Kelpham `rm_edric_kelpham` (steward, in debt: inside source), Eleanor Saltcot `rm_eleanor_saltcot` (moneychanger, the money trail), Ivar Pitchwick `rm_ivar_pitchwick` and Rune Gullwell `rm_rune_gullwell` (Counting House clerks: the factors who knock).
- **Fell**: Isolde Fell `rm_isolde_fell_` (head, protected, never lets a debt go), Corwin Fell `rm_corwin_fell_` (the Rainward claim), Gudrun Fell `rm_gudrun_fell_` (the Timberland letters), Millicent Rustwell `rm_millicent_rustwell` (steward).
- **Grayrock**: Aldric Grayrock `rm_aldric_grayrock_` (the absent lord; on patrol route `rm_grayrock_journey`), Maren Grayrock `rm_maren_grayrock_` (Larkfield-raised; never leaves the Hall; protected), Ingrid Grayrock `rm_ingrid_grayrock_` (16, writes weekly), Tobias Grayrock `rm_tobias_grayrock_` (reluctant heir), Lavinia Plankwick `rm_lavinia_plankwick` (steward; saw the Chancellor weep), Dagny Rustcot `rm_dagny_rustcot` (harbourmaster).
- **Timberland**: Signe Timberland `rm_signe_mossgrave` (Council seat, protected), Halvard Timberland `rm_halvard_mossgrave` (66; every Cinders smith would lie for him; needs `npc.spare`), Wenna Timberland `rm_wenna_mossgrave` (iron trade), Osric Timberland `rm_osric_mossgrave` (Southmeadow-raised, sheep runs), Hollis Nailwick `rm_hollis_nailwick` (steward).
- **Crown household**: Karin Stanmore `rm_karin_stanmore` (treasurer, bribe list), Percival Rustford `rm_percival_rustford` (servant, saw the weeping), Mab Coalridge `rm_mab_coalridge` (servant, 218 pennies too rich: Greatly's ear in the Residence), Jasper Bilgeford `rm_jasper_bilgeford` (customs), ushers Corra Coldridge `rmx_corra_coldridge` and Desmond Farrow `rmx_desmond_farrow`.
- **The Watch**: Corra Slagmore `rm_corra_slagmore` (Captain; owes a factor), Desmond Dripcot `rm_desmond_dripcot` (cells, bribe list), Brand Mudridge `rm_brand_mudridge` (weapon racks, owes a factor).
- **The Sump and the wharf**: Hilde Dripby `rm_hilde_dripby` (thief and blackmailer; saw the weeping), Corra Coalcot `rm_corra_coalcot` (Fish Hall; sells false lists), Ingrid Kendrick `rmx_ingrid_kendrick` (68, believed a thief by 803 residents: innocent), Jorunn Ropewick `rm_jorunn_ropewick` (the Pawn and Ledger: fence), Mab Carlisle `rm_mab_carlisle` (the Sailors' Rest; a Grayrock letter), the Council Press printers Ingrid Ashwell `rm_ingrid_ashwell` and Rune Fogworth `rm_rune_fogworth`, Bertil Grantham `rm_bertil_grantham` (11, lost a brother on the wharf).
- **Out of the city**: Nan Ashhand `cty_nan_ashhand` (Rainward quarrymaster), Fenwick Plankworth `cty_fenwick_plankworth` (Frosthollow mine captain), and the town hubs listed per chapter.

### 4.4 Strangers and foes (they come and go)

| Who | How | When | Notes |
|---|---|---|---|
| The Grey Captain's road band | `foe.add` (C10) band 2–6, or `story.area.spawn` at a camp today | Ch5, Ch6, Ch8 | anonymous bandits; `camp cleared` and `bandit falls` objectives |
| Desmond Hollis as the Grey Captain | `foe.add {npc: rmx_desmond_hollis, strength: tough}` (C10, downed not killed) | Ch5, Ch8 | today: his band only; Hollis is `npc.hold` in the cells by quest outcome |
| **Assessor Hedda Varrow** (f), of the Concord | `visitor.add` (talking stranger, N4), black cloak with silver trim, `like` a Heights resident | Ch8, up to 3 game days, re-added daily | sent from Upper Accord to judge whether Ridgemere governs itself or needs Concord stewardship |
| **Clerk Arne Wilde** (m), her clerk | `visitor.add` | Ch8 | carries her words through `story.tell from` the herald until N4 exists |
| Rainward feud wolves | `foe.add` small bands of "hired cudgels" in House colours | Ch6 | lets wolves fight House bullies without assaulting residents |

---

## 5. The chapters

Every chapter section gives: where; the shared goal and the deeds that feed it; the quarter beats (visible change); side storylines; branch points; recognition; failure; quest types; talk; triggers.

### Chapter 1: The Bell for Gunnar (spring, ~day 70 to 90)

**Where.** Market Square (`w_ridgemere_1_1`), the Clock Tower bell loft (`r2_the_clock_tower_up`), the Cathedral (`r2_the_cathedral_of_ridgemere`) and its crypt, the King's Residence, the Sump alleys and the Sunken Street (`r2_the_sunken_street`), and all eight town boards.

**Shared goal: the Long Howl** (story-scope quest, `contributors: min(3, A)`). The realm mourns its King the wolf way. Deeds that count (one point each, per wolf per day per kind):
- the dusk howl: a `performance` or `together` event inside the Market Square region at dusk (`calendar is period dusk`);
- carrying the bell notice to a town board (`notice posted` at `board:<town>`, or a courier contract to a town hub);
- sitting vigil in the cathedral (`enter place r2_the_cathedral_of_ridgemere` + `hour between 20 and 4`);
- keeping the Sump calm through the mourning (`reported`, `arrest` or `trouble solved peace` in the Sump quarter cell `w_ridgemere_0_1`);
- bringing bread or herbs to the Soup Kitchen (`gift` to its keeper, `quantity`).

**Quarter beats.**
- 25%: `board.post` on every town board: "The bell of Ridgemere tolls for King Gunnar. The free wolves of the roads keep the Long Howl." `talk.remember crown_stage=mourning` realm-wide (C8).
- 50%: `weather.set` overcast over Ridgemere for 24 hours; `npc.walk` Queen Oda and Ottilie to the cathedral at dusk; crier call (N1) on Market Square.
- 75%: `festival.call` "the King's Wake" in Ridgemere in 2 days (a small festival with entries: howling, the table games); `story.area.buff fed` on Market Square for 24 hours (the Soup Kitchen's wake bread).
- 100%: the herald proclaims the accident aloud. `scene.play` "The Herald's Word" on Market Square (Brenna Abbot reads the post's holder; the crowd's reaction is a beat; Dagfinn is not there). `talk.remember crown_stage=found_out`. Enter Chapter 2.

**Side storylines.**
- **S1.1 "Where is the King?"** (giver Cressida Cotton, Sump patrol; the first quest any wolf sees). "#greet#. You've a good nose, I can tell. My patrol partner's been handed a crown and he's still walking our round. Find him before the Houses do." Objectives: talk to three Sump porters at Sump Stairs (`rm_mathis_slagwell`, `rm_oda_greywell`, `rm_piet_wickridge`, any one counts: an `in list` trigger); `region entered` the Sunken Street; `near resident rmx_dagfinn_thornham` at dusk (he is `npc.walk`ed onto his round each dusk); talk to him (an `npc.offer` chain, §7.1). The first wolf to find him gets `deed.award "found the Sump King on his own round"` (`once: campaign`); every wolf who does gets "walked the Sump round with the new King".
- **S1.2 "The Queen's Purse"** (giver Queen Oda, the Residence). She needs the funeral paid without the Houses' coin. Do archive work at the Records Office (`archive work`, 3 times) to find the household's old tolls; bring 2 bread and 2 herbs for the wake. Choice at hand-in (`npc.offer`): "Pay it from the household purse" (Oda's trust; `back_blood +2`) or "Let Greatly lend it" (`back_purse +3`, `houses_grip +2`: Ambrose's first loan to the Crown).
- **S1.3 "Ring the Bell"** (board quest, Ridgemere; repeat daily). Climb to the bell loft at dawn and pull the rope (`enter place r2_the_clock_tower_up` + a `dialogue choice` with the bell-ringer's offer). Small and repeatable, for new wolves.
- **S1.4 "Hilde's Whisper"** (hidden; offered only to wolves with `ch_merit ≥ 3`). Hilde Dripby, at the Rag Mill, asks for 10 pennies "to remember something about a Chancellor". Pay her (`gift coins ≥ 10`) and she says only: "He wept, in the Rain Hall, the month before the old King died. Over a paper." Plants the Chapter 7 seed; the wolf's `heard_weeping` switch carries.
- **S1.5 "Word to the towns"** (board quests on all eight boards). Carry the bell notice from the Couriers' Office (`rm_the_hall_of_records`) to a town hub: Loam Pinefell at the Glasswater Inn (Coldwater), Aspen Marshholm (Larkfield), Rowan Downridge at the Vine and Sickle (Southmeadow), Nell Plummeadow at the Heron (Fenshore), Oriel Marshditch at the Oasis Inn (Saltwell), Tamsin Icefell at the Felled Cedar (Oakhaven), Thorn Ashwood at the Lamp and Pick (Frosthollow).

**Branch point.** None in the world yet; it records the wolves' first leanings (S1.2's choice) into the backings.

**Recognition.** Tracker "Your service to the Crown". At chapter end: `ch_merit ≥ 3` → `deed.award "kept the Long Howl for King Gunnar"`; `≥ 8` → Cressida greets the wolf by name for 7 days (C5: "#greet#. You're the one who found him. He asks after you."); the top three by `rank` (C3) → `title.give "Bell-Warden"` and a crier call naming them.

**Failure.** If the Long Howl is not filled by day 90, the herald proclaims anyway: `houses_grip +5` ("the Houses read the proclamation before the Crown did") and Chapter 2 opens with the Houses' claims already lodged.

**Quest types spawned.** Small: bell ringing, wake bread, a town notice. Medium: finding Dagfinn, the Queen's purse. Large: none (the chapter is a gentle opening).

### Chapter 2: A Guard in the Antechamber (spring into summer, ~day 90 to 115)

**Where.** The Antechamber (`r2_the_antechamber`, the Council seat), the City Office (`rm_the_city_office`), the Records Office rolls (`r2_the_records_office_top`), the four House seats (`rm_greatly_house`, `rm_fellhall`, `rm_grayrock_hall`, `rm_mossgrave_court`), the Magistrate's Court, Sump Stairs.

**Shared goal: the Crown's standing.** Every wolf chooses a backing once (`crown_backs`), through the dialogue windows of the four patrons: Dagfinn ("the Sump's own"), Ottilie ("the blood of Harrowgate"), Gerrit Mudhand ("a Council regency until the Houses agree"), or Edric Kelpham on Ambrose's behalf ("Greatly will steady the realm"). A wolf may change backing once per chapter, at a cost in merit. Backing deeds add to the shared total *and* to that backing:
- Sump: settle Sump troubles (`trouble solved` in `w_ridgemere_0_1`), escort Sump porters' carts, bring food to the Soup Kitchen;
- Blood: return lost petitions (S2.1), carry the Queen's letters to the towns;
- Council: carry the Council's summonses to the Houses (S2.3), vouch for residents before the magistrate;
- Purse: take Greatly contracts at the Counting House and the Greatly Bank (`contract done` with detail starting "Greatly", needs C12 `starts`).

**Quarter beats.**
- 25%: the Houses post their claims on the Ridgemere board ("House Fell holds that the reigning post is void"; one notice per House, staggered over 4 days). `talk.remember crown_stage=contested`.
- 50%: `npc.walk` Dagfinn from his round to the Antechamber at dawn, and he stays there by day (`npc.hold` during day hours, released at dusk so he can still walk his round). Residents across the Sump `band.override` to their usual bands but are told by `story.tell region` "The Sump King went up the Hundred Steps this morning."
- 75%: `figure.set ruler` to Dagfinn, title "who holds the work of reigning" (C9): every existing `#world.figure_ruler#` line plays.
- 100%: **the Council session**, a public scene in the Antechamber region (cast: Gerrit, Signe Timberland, Isolde Fell, Celestine Greatly, Lavinia Plankwick standing in for the absent Grayrock lord, Dagfinn, Ottilie). The wolves present vote (`choice`, `mode: majority`), but their vote is one weight among the backings: the interim word goes to the largest of `back_sump`, `back_blood`, `back_council` and `back_purse`, with the scene majority adding 20% of the total to its pick.

**Branch point: the Council's interim word** (sets `crown_seat`, lasting until Chapter 8):
- **Sump leads** → `crown_seat=dagfinn`: "the Sump King holds until the Houses agree." Dagfinn gives the Crown's quests in Ch3–6. `crown_hold +5`, `houses_grip −5`.
- **Blood leads** → `crown_seat=ottilie`: Ottilie is named Regent; Dagfinn is sworn her shield and stays at her side. Ottilie gives the Crown's quests. `crown_hold +5`.
- **Council or Purse leads** → `crown_seat=council`: a Council regency under Gerrit; Dagfinn is told to "keep the seat warm". Gerrit gives the Crown's quests. `houses_grip +10`; if Purse led, Ambrose is named the regency's treasurer (`houses_grip +5` more).

**Side storylines.**
- **S2.1 "The Lost Petitions"** (giver Ottilie, the City Office). 38 residents have a petition lost in the City Office. A wolf does archive work at the Records Office, then finds the petitioner (an `in list` match on a `residents` list of the 38, per wolf). Each petition returned gives that resident's thanks (C5, e.g. Hanne Coalton at the Harbour Gate: "#thanks#. Eleven weeks I've asked. You found it in a day.") and `back_blood +1`. A lone wolf can finish three and be thanked three times; a crowd empties the list.
- **S2.2 "The Sump Stands"** (giver Dagfinn). Three Sump Stairs porters have grievances against the Wharf Office: a lame cart, a short wage, a brother's lost wage. Settle them (`trouble solved`, `gift`, `contract done`). Ends with Dagfinn: "#thanks#. That's what I'm for, no? Not the chair."
- **S2.3 "Summonses for the Council"** (giver Gerrit Mudhand, the Antechamber). Carry the Council's summons to each House head: Isolde (Fellhall), Celestine (Greatly House), Signe (Timberland Court), and Lord Grayrock, who is not there: Lavinia the steward takes it and sighs. The Grayrock step plants Chapter 5.
- **S2.4 "A Roll in a Careful Hand"** (offered only to wolves backing Sump, by Philippa Coldby at the Magistrate's Court). A forged kin-roll making Dagfinn the old King's Mirebridge cousin, for 30 pennies. Take it to the Council (`back_sump +8`, `forged_roll=used`, a lasting risk: if it is exposed in Ch7, `back_sump −15` and the wolf's merit is docked), or report Philippa to Sergeant Dagny Ropeley (`warrant` on the clerk, she is fined, `forged_roll=exposed` now, `crown_hold +3`, deed "would not buy a forged crown").
- **S2.5 "The Ear in the Residence"** (Queen Oda, hidden, for blood-backers). Mab Coalridge, a servant, is far too rich. Track her at night to the Greatly Bank (`near resident` at night in the Commons). Show Oda (`npc.offer` choice): Mab is dismissed (C14 `npc.post none`) or turned to carry false word back to Ambrose (a switch that makes Ch4's ledger easier).

**Failure.** If the standing goal is not met by day 115, the Council meets anyway on the Houses' terms: `crown_seat=council`, `houses_grip +10`. The backings still count toward Chapter 8.

**Quest types.** Small: return a petition, a porter's grievance, a summons. Medium: the forged roll, the ear in the Residence. Large: the Council session (story-scope).

### Chapter 3: Lists in the Dark (summer, ~day 115 to 140)

**Where.** The Watch Office (`r2_the_keep`) with the cells (`r2_the_keep_down`) and the captain's rooms (`r2_the_keep_up`), the Watch House (`rm_the_watch_house`), Stanmore House (`rm_stanmore_house`), the cathedral crypt, the Antechamber, the Fish Hall (`rm_the_fish_hall`), the Pawn and Ledger, the Council Press (`rm_the_council_press`).

**Premise.** Whoever holds the seat cannot govern with a bought Watch. Thirty-five residents keep a private list of which Houses pay the Watch to look away. None has the whole truth. Put enough together and the real list appears.

**Shared goal: the Clean Watch.** Each keeper gives each wolf one piece, once (a player-scope `residents` list of keepers visited). Pieces come from conversation plus a small favour that suits the keeper:
- Karin Stanmore (treasurer, cold): bring her a copy of the Crown's customs tally from Jasper Bilgeford at the Customs House (`r2_the_customs_house_up`).
- Desmond Dripcot (cells, frank, 2 pennies to his name): sit a night watch at the cells with him (`together` in `r2_the_keep_down`).
- Abbot Lionel (devout): attend the Restday service, 9 to 11 (`sermon` on Restday), then ask in the crypt.
- Desmond Farrow (usher, quiet): carry his note to Corra Coldridge, the other usher, unseen (`sneak` practice, `story.practice watch`).
- The other 31 keepers: a plain conversation with a hint choice through `npc.offer`.
Fakes: Corra Coalcot at the Fish Hall sells "lists" for 5 pennies. A wolf with a good nose notices the fish-oil scent on fresh ink (`npc.offer` "Smell the paper" is shown only to wolves whose smell attribute is high enough, via a `wolf is` fact or a quest prerequisite C6). Buying a fake costs merit and adds nothing.

**Quarter beats.**
- 25%: Watch guards on the Sump and wharf patrols are told to "look away from no one". `talk.remember crown_lists=gathering`. Captain Slagmore posts a board notice warning against "slanderous papers".
- 50%: the pattern shows. `story.tell participants`: "Three Houses' names are on every list. One is on none." (Grayrock pays the wharf posts; Fell pays the North Gate and Northgate posts; Greatly pays nobody in coin, because it holds the Captain's debt instead; Timberland pays nothing, but the Cinders smiths lie for Halvard.)
- 75%: Ambrose's factors offer coin for the list in every Commons tavern: `npc.offer` on Rune Gullwell and Ivar Pitchwick ("Greatly pays well for paper").
- 100%: the list is whole, and a story item "the Watch list" (N3) is given to every wolf who gathered three or more pieces. Then **the wolves decide where it goes**.

**Branch point: the list's fate** (`lists_fate`; each wolf with the list chooses one destination; after 5 game days, or once two thirds of holders have chosen, the most-chosen destination wins; each wolf's own choice is remembered by the residents involved):
- **To Sergeant Bettany Nailley** (`bettany`): she takes it to the magistrate. Captain Slagmore is relieved of command (C14 `npc.post`); Bettany becomes Captain. Grayrock's wharf posts are cut. `crown_hold +10`, `houses_grip −10`. The Watch answers to the seat.
- **To the Council Press** (`press`): Ingrid Ashwell prints it. Everyone knows by morning. `houses_grip −15`, `times.nudge ridgemere −20 "the Watch disgraced"`, thefts in the Sump rise for 7 days (`band.override` for the sly residents to a hungrier band), and Slagmore resigns. `crown_hold +5`.
- **To Captain Slagmore** (`captain`): she burns it in front of the wolf and swears the Watch to the seat. Corruption stays, but the Watch is loyal: `crown_hold +5`, `houses_grip` unchanged. She remains a debtor (Ch4).
- **Sold to Ambrose** (`sold`): each selling wolf gets 40 pennies from `till:` the Greatly Bank and `crown_backs` moves to purse. Greatly now owns the Watch: `houses_grip +15`. Bettany is sent to the Harbour Gate.

**Side storylines.**
- **S3.1 "The Most Suspected Wolf in Ridgemere"** (giver Dagny Ropeley). Old Ingrid Kendrick is believed a thief by more residents than any wolf in the city (803 beliefs). She is innocent. Follow the scent from three stolen-goods reports to the Pawn and Ledger (Jorunn Ropewick), then to the true thief, Corra Coalcot (5 recorded thefts). `reported` with Corra as target clears Ingrid; with C8 `belief.plant` the city learns "Ingrid Kendrick never stole a thing". Ingrid thanks the wolf by name at the Sump for a week.
- **S3.2 "The Abbot's Crypt"** (giver Abbot Lionel). His list is not paper: it is scratched on the back of a crypt stone. The wolf copies it in the crypt at night (`enter place r2_the_cathedral_of_ridgemere_crypt`, `hour between 22 and 4`) and keeps it from the factors' watcher (a `bandits.call 2` ambush in the cathedral close if the wolf leaves by the north door, avoidable by sneaking out through the Sump side).
- **S3.3 "A Wage for the Cells"** (Desmond Dripcot, frank and penniless). The cells' keeper has not been paid since the King died. Bring his wage from the treasurer (a carry quest Karin → Desmond) or pay it yourself (`gift coins ≥ 15`). If paid by a wolf, he names that wolf to the next Captain.
- **S3.4 "The Captain's Rooms"** (hidden, for wolves backing blood or sump). Sneak into `r2_the_keep_up` at night (a `door.lock` on its door from dusk, opened by the usher's key item) and find the factor's note proving Greatly holds Slagmore's debt. Feeds Chapter 4.

**Failure.** If the list is not whole by day 140, the Houses' factors buy the keepers' silence: `lists_fate=lost`, `houses_grip +8`. Bettany's side storyline continues regardless.

**Quest types.** Small: a keeper's favour, the Restday service. Medium: the most suspected wolf, the crypt. Large: the list (story scope) and its fate.

### Chapter 4: By the New Moon (summer, ~day 140 to 168; the new moon of day 168)

**Where.** The Greatly Counting House (`rm_the_greatly_counting_house`), the Greatly Bank (`rm_the_greatly_bank`), Greatly House, the Soup Kitchen (`rm_the_soup_kitchen`), the Chapel of the Tide (`rm_the_chapel_of_the_tide`), the Sailors' Rest; Frosthollow (`cn_frosthollow_mine_house`, the Lamp and Pick) and the Highmere tarn in the Whitefangs.

**Premise.** The factors' deadline comes: "find it by the new moon". Greatly calls in every debt at once, to buy the Council with fear before the summer festival. Forty-one debtors, the Captain and the herald among them.

**Shared goal: the Debt Jar.** Pennies given into the Crown's relief purse (`gift coins` to Solveig Tarwell at the Chapel of the Tide, or to the Soup Kitchen's keeper), coins earned in contracts for named debtors, and goods brought to debtors' houses (bread, rope, lamp oil). Scaled goal (300 / 2,000 / 8,000 pennies). The coin really moves: the story pays it from the relief purse to debtors through `character.grant`-style resident payments (N6 `npc.pay`), or, today, simply counts and spends on outcomes.

**Quarter beats.**
- 25%: the factors knock. `npc.walk` Rune Gullwell and Ivar Pitchwick to debtors' doors in the Sump at morning; `story.tell region` lines from debtors ("They've chalked a mark on my door"). `talk.remember crown_debts=due`.
- 50%: the herald Brenna Abbot's own debt is paid by the jar: from now on her proclamations carry wolves' names (C1 text variables). The Soup Kitchen feeds the Sump (`story.area.buff fed`, 48 h).
- 75%: Ambrose answers. He offers the Crown a loan to pay every debt "for the Council's gratitude". The seat's holder asks the wolves (an `npc.offer` vote through the seat's giver: refuse or accept). Accept: `houses_grip +10` and the jar closes early with the debts paid (the easy road). Refuse: the race goes on.
- 100%: the new moon passes with the debts met. `talk.remember crown_debts=eased`. Captain Slagmore (if still Captain) is free of her factor.

**Branch point: Rosalind's ledger** (`debts_broken`). Rosalind Greatly has found a ledger that does not add up: the Counting House has been charging the Sump debts twice, once to the debtor and once to the Crown's customs through Jasper Bilgeford's books. If wolves complete S4.1 before the new moon, she lays it before the Council: the doubled debts are struck off (`debts_broken=on`, `houses_grip −15`, Ambrose loses his seat as the regency's treasurer if he held it), and every debtor in the jar's list is freed at once. If they do not, Ambrose sends her "to her mother's people in Saltwell for her health" (`npc.walk` to the Oasis Inn and `npc.hold` there): she becomes the Chapter 8 side storyline "The Daughter at the Well".

**Side storylines.**
- **S4.1 "A Ledger That Does Not Add Up"** (giver Rosalind, in the Greatly gardens). A three-part investigation: get the Counting House day book from Edric Kelpham (he owes too; pay his debt or promise to: `promise` event), compare it with Jasper Bilgeford's customs tally (`archive work` at the Customs House), and bring both to Eleanor Saltcot at the Bank, who confirms the doubled sums if the wolf has her trust (a gift). Final step: escort Rosalind to the Antechamber by day without the Counting House clerks scenting her (a `npc.follow` walk; `wolf is hurt` or a `bandits.call 2` hired cudgel ambush in the Commons).
- **S4.2 "Silver Under the Ice"** (giver Fenwick Plankworth at Frosthollow; a heavy group quest, `scope: party`, 2–6 wolves). The legend "something big moved under the ice" is the old King's toll sledge: in his first winter Gunnar sent the Kestrel mine's toll silver to the Sump soup kitchens, and the sledge went through the ice of Highmere. Fenwick knows where, and wants a Crown charter for the Kestrel mine in return. Haul it up: bring 4 rope (`gift` to Fenwick), go to the Highmere shore region in snow (`weather is snow`), and haul together (`together` events, 3 at once, in the region: `count in region ≥ 2`). Escort the silver down the Pass Road to Ridgemere against bandits (`camp cleared` at `w_ridgemere_8_6`). Reward: `ice_silver=on`, the Debt Jar jumps by a quarter, `crown_hold +5`, and Frosthollow's Moot voice leans to the seat. If Fenwick is denied his charter, he sells the silver's location to Greatly instead.
- **S4.3 "A Berth for Julian"** (giver Julian Greatly at the Sailors' Rest). Ambrose's son wants to sail. Help him sign on the Grey Tern (`ship sails`) with a letter from Mab Carlisle and a sea chest from the Ropewalk. If he sails, Ambrose's heir line is Rosalind alone and Celestine softens toward the Crown (`back_purse −5`, Celestine's salon gossip turns against her husband in Ch5–8).
- **S4.4 "The Captain's Debt"** (only if Slagmore is still Captain). She owes Greatly 120 pennies. Pay it from the jar (she becomes the seat's), expose it to the Council (she is replaced by Bettany late), or leave it (Greatly owns her in Ch8).

**Failure.** If the jar is short at the new moon, Greatly calls the remaining debts: the poorest debtors lose their homes for a season (`band.override` down a band, 28 days), and each Sump debtor's talk turns bitter (`crown_debts=called`). `houses_grip +10`. The story still moves on.

**Quest types.** Small: a gift to a debtor, a debtor's chore. Medium: Julian's berth, the Captain's debt. Large: the ledger investigation; Silver Under the Ice (party).

### Chapter 5: Letters for an Absent Lord (summer into autumn, ~day 168 to 200)

**Where.** Grayrock Hall (`rm_grayrock_hall` and `_up`), the Grayrock Wharf Office (`rm_the_harbourmaster_s_office`), the Chapel of the Tide, the Sailors' Rest; Lord Grayrock's road (patrol route `rm_grayrock_journey`), the Glasswater road camp `w_ridgemere_6_8`, the road to Oakhaven camp `w_ridgemere_5_4`, Larkfield's Sheaf (`cn_larkfield_alehouse`).

**Premise.** The Council cannot decide anything binding without House Grayrock's seat, and Lord Aldric Grayrock has not slept a full week in the Hall in six years. Forty wolves of the city carry letters for him, Dagfinn among them. If the letters reach him together, he will have to come home.

**Shared goal (fixed half): the Letter Bag.** 40 letters. Each carrier gives a letter (story item, N3) to the *first* wolf who asks and does them a small kindness; a world `residents` list removes the carrier. The wolf who gathered each letter is credited by name in the bag (C1 text: "Carried by …"). This is a race the crowd runs, and a lone wolf can still gather ten across a week.

**Shared goal (scaled half): the Lord's Road.** Aldric is on the road. Wolves find him by scent along `rm_grayrock_journey` (he is `npc.walk`ed between waystations; a `story.ping` marks only his last known waystation), guard the road (`escorted_caravan`, `camp cleared` at the Glasswater and Oakhaven road camps), and bring the letters to him (`gift` of the letter items to `rm_aldric_grayrock_`).

**Quarter beats.**
- 25%: the Grey Captain appears: Desmond Hollis leads a band on the Glasswater roads (`foe.add` C10, or `story.area.spawn` at `w_ridgemere_6_8` today), robbing Grayrock wagons. `figure.set villain` gains a second, local face: "the Grey Captain" known in region greenholt. `talk.remember crown_letters=gathering`.
- 50%: Aldric reads the first twenty letters at a waystation (a `scene.play` for participants nearby: he reads a few aloud, one of them Ingrid's).
- 75%: Aldric turns his wagon for Ridgemere. `npc.walk` him by stages up the Wharf Road. Lady Maren comes out to the Hall's gate for the first time in years (`npc.walk` and `npc.pose`). Residents with the "never home" hook start to talk about his return.
- 100%: Aldric sits in the Antechamber (`aldric_home=on`): the Council is whole. `talk.remember crown_letters=answered`.

**Branch point: the wharf dead** (`wharf_answered`). Fifty-six residents lost a brother on the wharf and do not forgive Grayrock. If wolves gather the names of the drowned from the Chapel of the Tide's roll (S5.4) and bring them to Aldric with the letters, he goes to the Chapel of the Tide and stands at the water while Solveig reads them (`scene.play` public, Wharf region). Lasting: the Sump's hatred of Grayrock eases (`back_sump +5`, Grayrock's Moot voice may side with the seat), and Bettany Nailley forgives the House. If not, Aldric sits but the Sump curses him, and Grayrock's Council vote stays with the Houses.

**Side storylines.**
- **S5.1 "Every Week, a Letter"** (giver Ingrid Grayrock, 16, at her lessons in the Hall). She has written to her father every week and never had an answer. Carry this week's letter, and bring back an answer: Aldric's reply is a `npc.offer` reply the wolf carries home (two versions, depending on whether the wolf says how many letters she wrote: "She wrote every week? Every one?"). The first wolf to bring a reply gets Ingrid's thanks for a month, and Ingrid speaks the wolf's name to her father at the Moot.
- **S5.2 "News of Larkfield"** (giver Lady Maren, Larkfield-raised, who has not left the Hall's grounds in years). Carry her message to Aspen Marshholm at the Larkfield linen shed and bring back a twist of Larkdowns thyme (`forage` herbs in larkdowns). Maren says: "#thanks#. It smells of home. I'd forgotten home had a smell."
- **S5.3 "The Reluctant Heir"** (giver Tobias Grayrock; only if Aldric is not home by day 190). Tobias must take the Council seat. Coach him: a day at the Bonded Warehouse books, a day at the forge (he would rather), a speech practised with Gerrit. If done, Tobias sits ably; the Grayrock seat leans to the Crown. If Aldric then comes home late, father and son sit together.
- **S5.4 "The Names on the Water"** (giver Solveig Tarwell, Chapel of the Tide). Gather the names of the wharf drowned from five bereaved Sump residents (Bettany Nailley, Millicent Rustwell, Ingrid Ashwell, Rune Fogworth, Bertil Grantham, any five of the 56 through `in list`). Each is a short, quiet conversation (the reply text names the brother's trade, never his name, to keep it id- and name-safe).
- **S5.5 "The Grey Captain's Road"** (board quests, Coldwater and Larkfield; party; daily). Break the Grey Captain's band. When the band falls (`foe defeated` with target the Grey Captain, C10), Hollis is downed and taken to the cells (`npc.move` to `r2_the_keep_down`, `npc.hold`); he is released at chapter end "for lack of a willing witness" unless a wolf testifies (Ch7). Today, without C10, the camp's clearing (`camp cleared` at `camp_w_ridgemere_6_8`) ends the band, and Hollis is moved to the cells by the story.

**Failure.** If Aldric is not home by day 200, Tobias takes the seat (if coached) or Lavinia Plankwick votes Grayrock's seat with Greatly (`houses_grip +8`).

**Quest types.** Small: one letter, Maren's news. Medium: Ingrid's reply, the names on the water, the reluctant heir. Large: the Lord's Road, the Grey Captain's band (party, repeatable).

### Chapter 6: The Rainward Quarrel (autumn, ~day 200 to 245; the autumn festival is day 230)

**Where.** The Rainward Hills (granite quarry `w_ridgemere_4_1`, Rainmere lake), Fellhall and the Fell Distillery (`r2_the_fell_distillery`), Timberland Court, the East Road (Fellhall → Coldwater), the Timberland charcoal hearths, Coldwater (`cn_coldwater_inn`), Oakhaven's cut (`w_ridgemere_4_4`), Southmeadow's Vine and Sickle.

**Premise.** The Houses fall out before winter, as 38 residents said they would. Corwin Fell demands the whole of the Rainward Hills for Fell vines; Greatly's granite quarry, run by Nan Ashhand, stands in the way; Timberland's charcoal camps (fifty residents grew up at one) burn in the same woods. House carters snarl on the East Road. Isolde Fell never lets a debt go, and Greatly holds Fell's.

**Shared goal: the Rainward Peace.** Patrol the hills (`region occupied` in story regions on the quarry and the Rainmere shore), escort carts on the East Road (`escorted_caravan`), settle feuds (`trouble solved feud` and `peace`), break the House cudgel bands (C10 `foe.add` bands in House colours: green Fell cudgels, blue Greatly quarry gangs), and carry the seat's peace terms between the three Houses (conversation objectives at the three seats).

**Quarter beats.**
- 25%: the quarrel breaks out. `faction.relation house_fell house_greatly hostile` (B). `event.add ridgemere lame_ox`-style hold-ups on the East Road; `harvest.override stone poor 14 "the quarry road is barred"`. `talk.remember crown_rainward=quarrel`.
- 50%: the seat calls the Houses to talk at the autumn festival (`festival.call ridgemere "the Rainward Truce" inDays 3`).
- 75%: a fire at the charcoal hearths (`event.add ridgemere works_fire` for the city's works trade, and `story.tell region` lines in the Rainward Hills). Wolves who fight the fire (`together` at the hearths) count double for a day.
- 100%: the Houses sit for terms.

**Branch point: the Rainward settlement** (`rainward`), decided by which terms the wolves carried most (each wolf's carried terms add to one option):
- **Fell takes the hills** (`fell`): Corwin's vines climb the slopes; the quarry is closed (`harvest.override stone poor 60`), Nan Ashhand loses her post (C14) and Greatly's grip loosens on Fell. `faction.relation house_fell house_greatly tense`. `houses_grip +5` (Fell rises).
- **Split** (`split`): Fell gets the south slopes, Greatly keeps the quarry, Timberland keeps the hearths. `faction.relation` all neutral. `crown_hold +5`.
- **Crown land** (`crown`): only if `crown_hold ≥ 55`. The seat takes the Rainward Hills as Crown land and leases them to all three Houses. `crown_hold +10`, `houses_grip −10`; Corwin storms out (he gives no quests in Ch8, and his Fell Moot voice goes to Greatly).
- **No terms** (`war`, the failure state): the Houses' cudgels keep the hills; `faction.relation house_fell house_greatly hostile` stays; `houses_grip +10`; bandit camps form twice as often in `w_ridgemere_2_3` and `w_ridgemere_3_3`.

**Side storylines.**
- **S6.1 "Gudrun's Letters"** (found, not offered: a wolf who enters Fellhall's family chambers on a Fell errand finds a sealed letter in Gudrun's paw-writing, a story item). The letters propose a Fell–Timberland match between Gudrun and Osric Timberland that would join vines and iron against Greatly. Three choices: **deliver** it to Osric at Timberland Court (`gudrun_letters=delivered`: Fell and Timberland become friendly, Greatly is isolated; the Southmeadow-raised Osric thanks the wolf in his own slow cheer), **show** it to Isolde (`shown`: Gudrun is shamed, Fell–Timberland tense, Isolde owes the wolf: she forgives one Fell debt of the wolf's choosing in Ch8), or **burn** it at the Fell Distillery's still (`burned`: nothing changes, but Gudrun learns who did it and asks the wolf, privately, why).
- **S6.2 "What Nan Is Saving For"** (giver Nan Ashhand, at the quarry). Nan has saved for years, toward something no one knows. It is to buy the quarry from Greatly. Help her raise the last of it (3 stone contracts, a gift of 20 pennies, or a word with Celestine at her salon). If she buys it before the settlement, the quarry is no longer Greatly's to lose: the `split` and `crown` terms become easier, and Nan sides with the seat.
- **S6.3 "The Charcoal Children"** (board quests, Oakhaven and Coldwater). Escort charcoal carts from the Greenholt hearths past the cudgels to the Timberland Ironworks (`rm_the_grayrock_ironworks`, the Timberland Ironworks). Halvard Timberland, old and walking the gardens, thanks each wolf who did it: "Every smith in the Cinders will know your name." (With C8, a `belief.plant` among Cinders smiths makes it true.)
- **S6.4 "Corwin's Wager"** (giver Corwin Fell, in any Commons tavern). Corwin bets he can make the Council give Fell the hills; he wants wolves to carry Fell wine to every Council member's steward. Taking it adds Fell terms; it also gives the wolf a back door to Corwin in Ch8.

**Failure.** As the `war` branch above; the campaign goes on.

**Quest types.** Small: an escort, a patrol, a feud. Medium: Nan's savings, Corwin's wager. Large: the cudgel bands (party), Gudrun's letters (a hidden find with lasting results).

### Chapter 7: The Chancellor Wept (autumn into winter, ~day 245 to 290)

**Where.** The City Office (Oskar's desk), the Records Office rolls, the cathedral crypt, the Greatly Bank's flat (`rm_the_greatly_bank_flat`), the Bilge and the Knucklebones (where the Sump's old wolves drink), the Magistrate's Court; the Mirebridge crossroads (`w_ridgemere_5_11` local (188, 52)) if Oskar flees.

**Premise.** Forty-six residents saw the Chancellor of Ridgemere weep in the Rain Hall and told no one. Each saw a little: the day, the paper's seal, the wolf beside him, which way he walked after. Put half their memories together and the Lantern Writ's story appears.

**Shared goal (fixed): the Testimonies.** 23 of the 46 witnesses (a world `residents` list; each witness gives one wolf their memory, first come). Witnesses include Gerrit Mudhand, Percival Rustford, Desmond Hollis (in the cells, if he was caught: his testimony only after a wolf visits him there), Hilde Dripby (who wants paying again), Lavinia Plankwick, and 41 others. A wolf who heard Hilde in Ch1 (`heard_weeping`) starts with one memory.

**The search** (scaled, once the testimonies reach 23): the memories point three ways. Wolves search all three, and the writ is in one (seeded `pick` at the chapter's start): the Records Office rolls (`archive work`, many times), the cathedral crypt behind the Abbot's stone (`enter place` at night, plus the Abbot's leave), or the Greatly Bank flat, where Oskar was made to leave a copy (a sneaking quest: `door.lock` and a key from Eleanor Saltcot). Each false site still yields something useful (the rolls: proof of Philippa's forgery; the bank flat: Ambrose's note to Oskar).

**Quarter beats.**
- 25%: `talk.remember crown_writ=whispered`; residents start asking each other what the Chancellor wept over.
- 50%: Oskar knows. He stops coming to the City Office at night (`npc.walk` him home early); the seat's giver warns: "He's afraid. Afraid wolves run."
- 75%: Ambrose acts: the Grey Captain's men (or House cudgels) watch the three sites (`foe.add` small bands at each site region).
- 100%: **the writ is found** (`writ_found=on`). The herald reads it on Market Square at noon (`scene.play` public, Market Square; Brenna reads, and if her debt was paid in Ch4 she names the wolf who found it). `figure.set` gains nothing new; instead `talk.remember crown_writ=read`: the realm now knows a Moot is called for the Long Night.

**Branch point: the Chancellor's fate** (`oskar_fate`). Wolves who confront Oskar (an `npc.offer` chain at the City Office, §7.4) choose:
- **Let him confess** (`confessed`): he goes before the Council and confesses all of it, Ambrose's debt and the order to lose the writ. `crown_hold +10`, `houses_grip −10`; he keeps his post, humbled, and gives a Chapter 8 side storyline (he knows which House stewards Ambrose has bought).
- **Expose him** (`exposed`): the Council Press prints Ambrose's note. Oskar is removed (C14); Ambrose is shamed (`houses_grip −15`) but the Sump's distrust of all officials grows (`crown_hold −5`).
- **Let him run** (`fled`, also the failure state if no one confronts him by day 280): he flees down the Fen Road and is last scented at the Mirebridge crossroads. Ambrose is untouched (`houses_grip +5`).

**The forgery.** If `forged_roll=used`, the Records Office search turns up Philippa's draft. `forged_roll=exposed`: `back_sump −15`, and every wolf who used it loses 5 merit and gets `npc.greet` from Dagfinn: "You meant well, no? I'd rather have the truth than a cousin."

**Side storylines.**
- **S7.1 "Hilde's Price"** (giver Hilde Dripby). She will tell the rest for 40 pennies, or a wolf can report her blackmail to the Watch. Pay: a testimony worth three. Report: she is fined (`warrant.issue` is for wolves only, so today a story `npc.hold` for a day), and her testimony is lost.
- **S7.2 "A Visit to the Cells"** (only if Hollis is held). Desmond Hollis, the Grey Captain, saw the weeping too. He trades his testimony for a word to the magistrate. If a wolf speaks for him, he testifies and is released on his oath; he does not lead the Moot raid in Ch8. If not, he escapes on the Long Night.
- **S7.3 "The Old Wolves of the Bilge"** (the Bilge and the Knucklebones). Sump elders remember Gunnar's toll sledge, his first winter, and his saying: "The Sump keeps its own peace, or nobody does." Share a meal (`shared meal`) with three of them; their memories give the writ's wording a line that the herald reads (pre-written).
- **S7.4 "The Queen's Choice"** (giver Queen Oda, once the writ is read). The writ passes over her daughter in plain words. Oda asks the wolf whether to contest it. Advise contest (`back_blood +5`, Oda fights for Ottilie at the Moot) or accept (`crown_hold +5`, Oda becomes the Moot's keeper of the roll beside Gerrit, and blesses whoever wins). The wolf's word is remembered by name.

**Failure.** If the testimonies stop short, the writ is never found: the Moot is called anyway, by the Council, "in the old King's memory", but only Houses and towns sit, and the free wolves' voice counts half (`writ_found=off`).

**Quest types.** Small: a witness's memory, a shared meal. Medium: Hilde's price, the cells, the Queen's choice. Large: the search (three sites), the Chancellor confronted.

### Chapter 8: The Long Night Moot (winter, ~day 290 to 321; the Moot on winter day 46)

**Where.** All eight towns' squares (`w_ridgemere_7_7` Coldwater, `6_9` Larkfield, `8_13` Southmeadow, `3_13` Fenshore, `12_11` Saltwell, `3_4` Oakhaven, `9_3` Frosthollow), the Mirebridge crossroads, Market Square and the Antechamber, the Grayrock Wharf.

**Premise.** The writ (or the Council) calls a Moot on the Long Night. Every town must send a voice; the free wolves of the roads are the third estate. The Concord has heard: an assessor arrives from Upper Accord to see whether Ridgemere can choose a ruler without blood in the street.

**Shared goal: the Moot voices.** Each town's voice is won by deeds in that town during the chapter (a town-scope count per backing): its board's quests, its residents' troubles, its roads escorted, its festival entries. When a town's count reaches `max(4, ceil(0.5 × A_town))`, its voice goes to the backing with the most deeds there (`voice_<town>`), and its square gets a lit lamp (`item.place` a lantern-like item, or a `story.ping` labelled "the Moot lamp"). A lone wolf can win Saltwell's or Oakhaven's voice alone in a few days; a crowd splits across eight towns. The town hubs proclaim it in their own words (§6).

**The Mirebridge voice.** Mirebridge was erased; Fenshore still speaks its speech and swears the marsh lights are the drowned. If wolves carry lanterns to the Mirebridge crossroads on the nights before the Moot (S8.2), Mirebridge sends a voice through Fenshore (`mirebridge_lit=on`): Fenshore's voice counts double. It always goes to the Sump King's side if Dagfinn is still a claimant, because he is a fen wolf.

**Quarter beats.**
- 25%: Assessor Hedda Varrow and her clerk arrive at the North Gate (`visitor.add`, re-added daily), and the herald announces "the Concord's eyes" in the city. `talk.remember crown_moot=called`.
- 50%: town lamps lit in half the realm; criers in every town square (N1) call the voices so far.
- 75%: the Grey Captain's last raid (if Hollis is free): a `foe.add` band at the South Gate road on the Moot's eve. Every wolf who stands at the gate counts.
- 100%: **the Moot** on Market Square at dusk of the Long Night.

**The Moot** (public scene, Market Square area; cast: Gerrit and, if allied, Oda as keepers of the roll; Signe, Isolde, Celestine and the Grayrock seat (Aldric, Tobias or Lavinia); Dagfinn; Ottilie; Ambrose if he still holds a Council office; the herald). The voices are counted in this order (each a `line` beat with pre-written text per value):
1. the eight towns' voices (plus Mirebridge's, through Fenshore);
2. the four Houses' Council votes, set by the earlier branches (for example Grayrock leans to the seat if `wharf_answered`; Fell to Greatly if `rainward=crown` angered Corwin; Timberland with Fell if `gudrun_letters=delivered`);
3. the free wolves: a `choice` beat with `mode: majority` among the wolves present, weighted as one voice per 10 percent of the cohort's total merit present, at most four voices (so a crowd at the Moot matters, but cannot undo a realm of towns), and halved if `writ_found=off`.

**Endings** (`finish` everyone with one of four texts, then the epilogue chapter's lasting effects):

| Ending | When | What changes |
|---|---|---|
| **The Sump Crown** | Sump voices lead and `crown_hold ≥ 50` | Dagfinn is crowned King of Ridgemere (C14 post kept, label "reigning, crowned at the Long Night Moot"); Ottilie becomes Chancellor of Petitions; Bettany or Cressida captains the Watch; `figure.set ruler` "the Sump King"; `talk.remember crown_seat=sump_king keep`. |
| **The Harrowgate Restored** | Blood voices lead | Ottilie is crowned Queen; Dagfinn is named Captain of the Watch and her shield; Oda is Queen Mother; `figure.set ruler` "Queen Ottilie". |
| **The Council Crown** | Council voices lead, or no side leads by two voices | Gerrit is Lord Chancellor of a Council regency; Dagfinn goes back to the Sump round with a pension and the realm's thanks ("a King for a season"); `houses_grip` stays where it is. |
| **The Counting-House Crown** | Purse voices lead, or `houses_grip ≥ 75` and `crown_hold < 35` | Ambrose Greatly is named Lord Protector "until the debts are settled". Dagfinn is put in the cells on a Greatly warrant. `figure.set villain` "the Lord Protector". This opens campaign two, "The Debt of Ridgemere", with the wolves as the resistance. |

**The Concord.** Whatever the ending, Assessor Varrow's verdict depends on the Moot's peace: if the Grey Captain's raid was broken and no House cudgels fought in the city during the chapter, she writes "Ridgemere governs itself" and leaves. If not, she leaves a Warden behind "to watch the Quickened among you": the hook for the first Warden storyline (needs a `warden_order` faction and a resident member, made by the DM first).

**Side storylines.**
- **S8.1 "A Voice from Your Town"** (each town hub, by where the wolf is). The hub asks the wolf to carry the town's voice to Ridgemere on the Long Night: be present at the Moot with the town's token (story item). The token's bearer is named at the Moot ("Saltwell's voice was carried by …", C1).
- **S8.2 "Lanterns for the Drowned"** (giver Nell Plummeadow at the Heron, Fenshore, or Skye Eelbank at Withy Creek). Carry lamp oil from the Fenshore stores to the Mirebridge crossroads at night, three nights running (`region entered` at night, `weather is fog` optional bonus). The marsh lights answer (`story.ping` lights over the fen; `weather.set fog sallow_fens`). Nell's unanswered letter: if the wolf brings it, she reads it at the crossroads and lets it go.
- **S8.3 "The Daughter at the Well"** (only if Rosalind was sent away). Rosalind is at the Oasis Inn in Saltwell. Bring her home for the Moot (escort along the Waste Road against a band); her ledger, laid at the Moot, still breaks the debts (`debts_broken=on` late, `back_purse −20`).
- **S8.4 "The Grey Captain's Last Road"** (if Hollis is free). The final fight at the South Gate road (`foe.add` champion strength with a band of 6). Whoever lands the last blow is named by the herald at the Moot.
- **S8.5 "The Assessor's Questions"** (the Concord's clerk, through the herald). The assessor asks three wolves (first come) what kind of realm this is: each answer is an `npc.offer` reply that sets her verdict a little (honest answers about the lists and the debts help).

**Failure.** No town voice blocks the Moot: a town that sends no voice is "silent", and the Moot counts without it.

**Quest types.** Small: town errands for the voice, lamp oil. Medium: the drowned's lanterns, the daughter at the well, the assessor's questions. Large: the last road (party), the Moot itself.

---

## 6. Talk: what residents say at each stage, per dialect

All gossip below is library text for `Data/Talk/scenes/common/story_sump_crown.talk` (new file; it loads automatically), as `rule` barks and smalltalk gated on `world.mem.crown_*=` keys that C8's `talk.remember` writes. They name no one: people are carried by `#world.figure_ruler#`, `#world.figure_villain#` and `#world.figure_hero#` (set by C9), or by role ("the Sump guard", "the old Queen"). Each line is under 160 characters, uses no weather words without a weather gate, and uses only its own dialect's words. The `here.region` gate chooses the dialect line; `plain` lines are the fallback everywhere.

Rule shape (validated pattern from the talk survey):
```
rule crown_mourning_smalltalk
when concept=talk.smalltalk|talk.news world.mem.crown_stage=mourning
does tell.news
cooldown 1d
say The old King's gone up to the Bright One. Sump-born, and he never forgot it. | ...
```

### 6.1 Chapter 1, `crown_stage=mourning` / `found_out`

- **plain (Ridgemere):** "The old King's gone up to the Bright One. Sump-born, and he never let the Heights forget it. Blood and bread, I'll miss him." / *helpful:* "If you want to be useful, the Couriers' Office has the bell notices for the town boards. Somebody has to carry them." / *found_out, sump quarter:* "They say the one reigning now still walks the Sump round at dusk. Out of habit, or because the alleys trust him more than the Palace?"
- **coldwater:** "Bright One's tide take the old King gently, eh? The Queen grew up by Glasswater, you know. A Coldwater wolf in the Residence."
- **larkfield:** "A Watch guard on the throne? By the market cross, the Houses will have that weighed and short-weighed before Marketday."
- **southmeadow:** "We'll drink to the old King at the Vine and Sickle. Ripe days he gave us, mostly. Sheaf and sickle, may the next do half as well."
- **mirebridge (Fenshore):** "The new one reigning is Fenshore-raised, no? Learned to walk on our boardwalks. Mud and eels, a fen wolf in the Palace."
- **saltwell:** "Peace on the old King. The well remembers who drew from it, honoured guest, and he drew for the poor."
- **oakhaven:** "Root and rain. The old King's windthrown at last. Slow to fall, that one, mm?"
- **frosthollow:** "Cold iron. The King's dead and the Houses are counting already. Keep your lamp lit, whelp."

### 6.2 Chapter 2, `crown_stage=contested`; then `crown_seat=dagfinn|ottilie|council`

- **plain:** "The Houses have pinned their claims on the board like washing. Four Houses, four claims, one chair." / *helpful:* "The King's daughter is looking for lost petitions in the City Office. She'd thank anyone who found a few." / *seat=dagfinn:* "#world.figure_ruler# sits in the Antechamber by day and walks the Sump at dusk. I've never seen a ruler do both."
- **coldwater:** *seat=ottilie:* "The daughter's Regent now, eh? It'll float, I think. She's carried every petition in this realm on her own back."
- **larkfield:** "Four Houses bidding for one chair. Sold to the highest, if you ask me, lambkin."
- **southmeadow:** *seat=council:* "A Council of Houses keeping the chair warm. That's chaff, my apple, and they know it."
- **mirebridge:** *seat=dagfinn:* "Fen-sure he'll keep it. A fen wolf doesn't let go of a plank once he's on it, no?"
- **saltwell:** "Mirage-talk, all of it. A crown is a well: who digs it, who guards it, who drinks. Ask the Houses which they mean to be."
- **oakhaven:** "Shade on you. The city's chair is like a young cedar, mm? Plant it in the wrong ground and it leans."
- **frosthollow:** "A guard on the chair. Dead rock or a good seam, we'll know by winter."

### 6.3 Chapter 3, `crown_lists=gathering` / `bettany|press|captain|sold|lost`

- **plain:** "Somebody's gathering the lists of who pays the Watch. Mind whose name you say aloud in a tavern." / *helpful:* "They say the abbot keeps a list too, and shares it only after the Restday service." / *press:* "It's printed! The Council Press ran off the whole list. Half the Watch can't meet your eye this week." / *sold:* "The list went to the Counting House. Of course it did. Everything in this city ends up in the Counting House."
- **coldwater:** *bettany:* "The young sergeant has the Watch now, eh? Fair catch for the Sump, that."
- **larkfield:** "A Watch you can buy is a scale you can lean on. Scales and silver, I'd like to see that list."
- **southmeadow:** *helpful:* "The fish-hall wolf sells papers, they say, but they smell of fish oil. Blight take it, don't pay her a penny."
- **mirebridge:** "Lists of who pays the Watch. In the fens the Watch never came at all, so nobody paid. Boggy sort of honesty, no?"
- **saltwell:** "By salt and water, a Watch that is paid by a House guards that House's well, not yours, my brother."
- **oakhaven:** "Bark and branch. Rot starts under the bark, mm? You don't see it till the tree's down."
- **frosthollow:** "Black seam. Every pit has a captain who takes a cut. The city's no different, just deeper."

### 6.4 Chapter 4, `crown_debts=due` / `eased` / `called`; `ice_silver`

- **plain:** "The factors have chalked marks on Sump doors. The new moon's close and nobody has the coin." / *helpful:* "The Chapel of the Tide is taking pennies for the debtors. Every one counts, the priest says." / *eased:* "#world.figure_villain# called in every debt at once, and the realm paid it. I didn't think we had it in us."
- **coldwater:** *helpful:* "If you've a spare penny, the chapel in the city's filling a jar for the Sump debtors, eh?"
- **larkfield:** "Counted twice, they say. Once to the debtor, once to the Crown's books. Short-weighed on both sides of the scale."
- **southmeadow:** *eased:* "The debts eased in the city? Now that's a windfall nobody expected. Good harvest to whoever did it."
- **mirebridge:** "Owe the Houses a penny and they own your boots. Lucky we go barefoot in the fens, no?"
- **saltwell:** "Water and shade to the debtors. A loan the borrower cannot refuse is not a loan. It is a leash."
- **oakhaven:** *called:* "They took homes in the city for debts, mm? Windthrown families. That's no way to fell a tree."
- **frosthollow:** *ice_silver:* "They brought the old toll silver up from Highmere. Deep cold work. Something big under the ice after all."

### 6.5 Chapter 5, `crown_letters=gathering` / `answered`; `wharf_answered`

- **plain:** "Someone's carrying every letter for the Grayrock lord out to his road at last. Forty of them, I hear." / *helpful:* "The Grayrock girl at the Hall writes to her father every week. If you're going his way, she'd give you this week's." / *wharf_answered:* "The Grayrock lord stood at the Chapel of the Tide and heard the names of the drowned. I never thought I'd see it."
- **coldwater:** "Bandits on the Glasswater roads, robbing Grayrock wagons. A grey-cloaked captain leads them, they say, eh?"
- **larkfield:** *helpful:* "The Grayrock lady is a Larkfield wolf, did you know? Bring her news from the Sheaf and she'll weep for joy."
- **southmeadow:** "A lord who's never home is like a vine never pruned. All runners, no grapes, my apple."
- **mirebridge:** "Forty letters and not one answered. In the fens we'd call that a very wet postbag, no?"
- **saltwell:** "The well remembers who drew from it. The lord has drawn for six years and never come back to the well."
- **oakhaven:** "Root well, they say to saplings. That lord never put a root down in his own Hall, mm?"
- **frosthollow:** "A lord on the road all year. Warm hearth, cold Hall."

### 6.6 Chapter 6, `crown_rainward=quarrel` / `fell|split|crown|war`

- **plain:** "Fell carters and Greatly quarry wolves snarling on the East Road. The Houses are falling out, just like everyone said." / *helpful:* "The quarrymaster at the Rainward quarry is saving for something. Ask her what. She might tell you."
- **coldwater:** "Carts held up on the East Road all week, eh? Calm water's a long way off between those Houses."
- **larkfield:** *split:* "They split the hills three ways. A fair price to each, for once. Sold, and everyone grumbling, which means it's fair."
- **southmeadow:** *fell:* "The Fell vines are climbing the Rainward slopes. Ripe days for the cellars, chaff for the quarry wolves."
- **mirebridge:** *war:* "The Houses are fighting over hills. In the fens we fight over the dry bits, and there are fewer of those, no?"
- **saltwell:** *crown:* "The hills are the Crown's now, and leased to all. That is how a well is kept, my sister: by everyone, for everyone."
- **oakhaven:** "Timberland's hearths burned in the woods near the Rainward road, mm? Bark and branch, I hope the charcoal children got out."
- **frosthollow:** "Houses scrapping over a quarry. Give them a real seam to fight over and they'd hide in the cellar."

### 6.7 Chapter 7, `crown_writ=whispered` / `read`; `oskar_fate`

- **plain:** "Everyone's asking what the Chancellor wept over in the Rain Hall. Turns out half the city saw him do it." / *read:* "The old King wrote it down: the Sump's own peace-keeper holds the work until a Moot on the Long Night. So it was no accident." / *confessed:* "The Chancellor stood up in the Council and told it all. Owed the Counting House, he did. Weeping again, they say."
- **coldwater:** *read:* "A Moot on the Long Night, eh? With the free wolves of the roads to speak too. Net and line, that's a thing."
- **larkfield:** "A writ lost for a debt. That's the dearest paper ever bought in Ridgemere, lambkin."
- **southmeadow:** "The old King wanted all of us to choose? Sheaf and sickle, I'd best think what I'll say."
- **mirebridge:** *fled:* "The Chancellor ran down the Fen Road, they say, and stopped where Mirebridge used to stand. Even the drowned didn't want him, no?"
- **saltwell:** "A ruler who writes 'let the people choose' and hides it in the dark. The Houses hid it, honoured guest, not the King."
- **oakhaven:** "Old Oak. The old King planted something deep before he fell, mm? Now we see the shoot."
- **frosthollow:** "So the guard on the chair was meant to be there. Dead rock to the Houses. A good seam for the rest of us."

### 6.8 Chapter 8, `crown_moot=called`; `voice_<town>=…`; then the ending

- **plain:** "Every town's sending a voice to the Long Night Moot. Ours goes to whoever kept the peace best this winter." / *helpful:* "The Concord's sent an assessor. Black cloak, silver trim. She asks questions. Answer them honestly." / *ending sump:* "#world.figure_ruler# was crowned on Market Square at the Long Night, and went home by the Sump round."
- **coldwater:** *voice won:* "Coldwater's lamp is lit for the Moot, eh? Fair winds to whoever carries our word."
- **larkfield:** "Our voice to the Moot is bought by deeds, not silver. Scales and silver, that's a market I like."
- **southmeadow:** "Southmeadow's voice is ready. A windfall of a winter, my apple, and we'll have our say."
- **mirebridge:** *mirebridge_lit:* "Lanterns at the old Mirebridge crossing, three nights. The marsh lights answered. Dry feet, the drowned are voting too, no?"
- **saltwell:** "Saltwell's voice goes to the Moot. May your well never fail, and may the one we choose remember who drew from it."
- **oakhaven:** "Oakhaven has eight wolves and one voice. That's a lot of shade for a small grove, mm?"
- **frosthollow:** "Frosthollow sends its voice down the Pass Road. Lamp and pick, we'll be heard."
- **ending protector (plain):** "The Counting House has the realm now. They call it a Protectorate. I call it a debt with a crown on."

### 6.9 NPC-to-NPC scene (sample)

```
scene crown_ch3_lists
when topic=gossip world.mem.crown_lists=gathering
does tell.news / react.news / assess / react.self
tone serious / even / serious / even
a : There's a wolf going round asking who keeps a list of who pays the Watch. | Somebody's putting the Watch lists together.
b[quarter=sump] : About time. The Sump's paid for the Watch twice, once in coin and once in looking away. | Let them. We've nothing to hide down here but hunger.
b : Careful. Lists like that get wolves hurt. | I'd keep my head down, if I were asked.
a : #bc_it_is# | Bright One keep whoever's carrying it.
b : Bright One keep us all. | Blood and bread, they'll need it.
```

### 6.10 Window lines for the cast (tree `say[...]`, no mantle changes)

Per resident, gated on `self.id` and campaign memory, added to `resident.tree` (and `watch.tree`, `church.tree` for guards and priests) at `priority=3`. Samples:
- `say[self.id=rmx_dagfinn_thornham world.mem.crown_stage=found_out priority=3]` "#greet#. Don't call me that. I've a round to walk and the alleys don't care what the herald says."
- `say[self.id=rmx_dagfinn_thornham world.mem.crown_writ=read priority=3]` "The old King meant it for someone like me. I'd rather he'd told me. I'd have slept better, no?"
- `say[self.id=rm_ottilie_harrowgate world.mem.crown_stage=contested priority=3]` "#greet#. Forgive the cough. Do you know how many petitions the City Office has lost? Neither does the City Office."
- `say[self.id=rm_oda_harrowgate world.mem.crown_debts=due priority=3]` "Every penny counted twice and still short, eh? The Houses lend with one paw and take with the other."
- `say[self.id=rm_ambrose_greatly world.mem.crown_debts=eased priority=3]` "You paid it. All of it. How very generous of the realm. There will be another new moon."
- `say[self.id=rm_oskar_gullworth world.mem.crown_writ=whispered priority=3]` "I've no time to talk. The City Office keeps late hours. Some papers keep later."
- `say[self.id=rm_corra_slagmore world.mem.crown_lists=press priority=3]` "Read the papers, did you? So did I. I'll be gone by Restday. The Watch will be better for it, or worse."
- `say[self.id=rm_aldric_grayrock_ world.mem.crown_letters=answered priority=3]` "Forty letters. My daughter's were the heaviest. I read them all on the Wharf Road. I'm home."

---

## 7. Talking paths for quest-givers (dialogue chains)

Quest offers use the quest `offer` block; deeper talk uses `npc.offer` chains on `dialogue choice` (one step per pick; `player: $actor`, `once: true`), which work today. With C6 the giver also speaks while the quest is under way (`offer.during`).

### 7.1 Dagfinn on his round (Ch1, S1.1's last step)

1. Offer `dag-1` words "Why aren't you at the Palace?" → reply: "Because the Sump doesn't stop needing a round walked, no? The Palace has plenty of wolves. The alleys have me."
2. On `dialogue choice dag-1`: offers `dag-2a` "The realm needs you up there." and `dag-2b` "Then stay down here. The Houses will eat you."
   - `dag-2a` reply: "Needs a guard? Or needs a wolf the Houses can blame? #oath#. I'll go up. Walk with me to the Hundred Steps." (sets `crown_backs` lean sump, +1 merit; `npc.walk` toward the Hundred Steps; `npc.follow` the wolf for the walk)
   - `dag-2b` reply: "Fen-sure, they might. But somebody ought to be in that room who's slept in a three-family tenement." (`back_council +1` and no walk; he goes up later anyway)
3. On either: `dag-3` "What's in the letter you carry?" → "It's for the Grayrock lord. Everyone's got one. Mine says the wharf cranes kill wolves. He's never home to read it." (plants Ch5)

### 7.2 Ottilie and the petitions (Ch2)

Quest offer `say`: "#greet#, #address#. Forgive the cough. The City Office has lost thirty-eight petitions, and every one belongs to a wolf who still hopes. Will you help me find them?" `ask` "You look tired." `accept` "I'll find them." `thanks` "#thanks#. Thirty-eight wolves, and you found theirs. Mother says a crown is coin. I say it's this."
`npc.offer` `ott-1` "Do you want the crown?" → "I want the petitions answered. If that needs a crown, I'll wear one. If it needs a guard from the Sump, I'll carry his petitions too."

### 7.3 The list's destination (Ch3)

Each wolf holding the list gets four `npc.offer` choices at the seat's giver ("Where should the list go?") or at the four destinations themselves: Bettany "Take this to the magistrate." → "#oath#. I've waited six years for this paper. I'll walk it there myself."; Ingrid Ashwell at the Council Press "Print it." → "Every word? By morning the whole city will know. Mind your back."; Captain Slagmore "This is yours to mend." → "I'll burn it, and I'll mend it. You'll see which."; Rune Gullwell at the Counting House "What's Greatly paying?" → "Forty pennies, and the House's gratitude. The second is worth more."

### 7.4 Confronting the Chancellor (Ch7)

1. `osk-1` "You wept in the Rain Hall. Over the King's writ." → "Who told you that? No. Don't say. Forty wolves saw, didn't they. I counted the faces for months."
2. Then three choices: `osk-confess` "Tell the Council yourself." → "And lose everything? I lost it the day I took the Counting House's coin. Very well." / `osk-press` "The Press will tell it for you." → "Then I hope they print the note too. His paw, not mine." / `osk-run` "Run, if you want. I won't stop you." → "The Fen Road, then. Even the drowned keep better accounts than I did."
Each sets `oskar_fate` through a tally (first choice to reach a majority of the wolves who confronted him by day 280).

### 7.5 Town hubs for the Moot (Ch8, S8.1, each in the hub's own voice through symbols)

- Loam Pinefell (Coldwater, short and reserved): "#greet#. Coldwater wants a voice at the Moot, eh? Then somebody's got to carry it. You've the legs for it."
- Rowan Downridge (Southmeadow, quiet): "#greet#. We're small, but we'd like our say. Would you carry Southmeadow's word down to the city?"
- Oriel Marshditch (Saltwell, ambitious and frank): "#greet#, honoured guest. Saltwell will speak at the Moot. Carry our word and the well will remember you."
- Thorn Ashwood (Frosthollow, calm and warm): "#greet#. Frosthollow's voice goes down the Pass Road with you, whelp. Don't drop it."

---

## 8. Recognition: lone wolf and crowd

- **Instant:** every counted deed shows "+1 to the Long Howl" style progress on the shared quest's bar, and the wolf's own tracker ticks.
- **Named thanks:** C5 `npc.greet` from the resident the wolf helped, by the wolf's name, in that resident's voice, for 7 days. Today: `npc.offer` one-off replies for that wolf.
- **Chapter tiers** (`each over participants` with `compare` on `ch_merit`): 3 → a deed in lower-case verb form (fame, nickname, crier); 8 → the chapter's patron greets them by name; 15 → `character.grant` 20–50 pennies from the town purse that benefits (`town:ridgemere`, checked to hold the coin) and `story.practice` in the chapter's skill.
- **Top three per chapter** (C3 `rank`): a campaign title (C4: Bell-Warden, Petition-Bearer, List-Finder, Debt-Breaker, Lord's Courier, Rainward Peacemaker, Witness of the Rain Hall, Moot-Warden) and a crier call with names (N1 + C1).
- **First-finders:** the first wolf to find Dagfinn, the first to complete the list, the wolf who finds the writ, the bearer of each town's voice: each is named in a board notice and by the herald.
- **Campaign end:** the wolf with the highest `crown_merit` becomes the realm's `hero` figure (C9 with a wolf: "#world.figure_hero#" then plays in fifteen existing talk lines); everyone with merit ≥ 20 gets the title "Crown-Friend" (or "Sworn of the Sump Crown", "Shield of the Harrowgates", "Friend of the Council", or, in the dark ending, "Remembered by the Sump").
- **For a crowd of 200:** goals scale; unique pieces (letters, witnesses) are raced and credited; the town voices spread wolves over eight towns; ranks and titles go to the top, but every tier is open to all.
- **For a lone wolf:** floors keep every shared goal possible for 5 wolves in about a week; each chapter has at least two side storylines a wolf finishes alone with a named result; the small towns' Moot voices can be won single-pawed.

---

## 9. Trigger features required

| Need | Today | Gap item |
|---|---|---|
| Campaign of 8 chapters, carried variables | `campaign.approve`, `story.start {own, carry}` | — |
| Shared progress bars | story-scope quests with `contributors` | — |
| Goals sized by audience | fixed floors, DM `set` | **C2** `audience` operand + `multiply` |
| Quarter beats on a shared world counter | `hour` polling + `compare` | **C13** `variable changed` |
| The new moon deadline | `schedule` 28 days | **C13** `month begins` |
| Dagfinn's accession as a fact; the crowning | none | **C13** `succession` event, `resident has post`; **C14** `npc.post` |
| Board and crier texts with wolves' names and numbers | pre-written branches | **C1** variables in texts |
| Crier calls in a town square | `story.tell region` | **N1** `crier.call {town, text, from?}`: spoken by the town crier or herald, logged as a `crier` event so fame and talk pick it up |
| Ranks, titles, named thanks | tiers via `each`; deeds | **C3** `rank`; **C4** `title.give`; **C5** `npc.greet` |
| Gossip that follows the campaign | none | **C8** `talk.remember` (all of §6), `belief.plant` (Ingrid Kendrick cleared, the smiths' word) |
| Ruler, villain and hero figures | DM `save_figure` by hand | **C9** `figure.set`, extended to name a wolf for `hero` |
| The Grey Captain and the cudgel bands | `story.area.spawn`, `bandits.call` | **C10** `foe.add` with `npc` (downed, not killed) |
| Key cast kept alive 250 days | `npc.protect` + alive branches | **C11** `npc.spare` |
| Quest packs feeding the campaign; Greatly contracts | world vars, polled | **C12** `story.notify`; `field` op `starts` |
| Chained personal quests that don't leak | journal `quest.offer` + `quest is` | **C6** `requires`, `offer.during` |
| Fallback givers if a resident dies | static givers | **C7** `cast` |
| Unique story items: the list, the letters, the writ, town tokens | generic `letter` | **N3** story items: `item.give {item, tag, name ≤60}`; `gift` and `item picked up` match `tag`; undone at the end unless `keep` |
| A wolf's choice tallied into a realm decision (list, Chancellor, terms) | `npc.offer` + `once: member` + world numbers | **N2** (optional) `ballot {id, options, closes, rule: most|majority}` with an event `ballot closed`, to keep the counting tidy |
| The Concord assessor who speaks | mute `visitor.add`, words through the herald | **N4** a talking stranger: a visitor may be `from` in `story.tell`, scene cast, and give `npc.offer` choices; stays up to 7 days |
| The Debt Jar paying debtors | counted only | **N5** `npc.pay {from purse, npc, coins}`: move coin from a story purse to a resident (no minting) |
| Town reputation for Moot voices | `reputation` condition | **N6** `reputation.adjust {player, town, points}` and a per-town count operand |
| The Council and the Moot | public scenes, `choice` majority | — (scenes exist; Market Square is outdoors, so the Moot works today) |
| Warden hook at the end | none | DM prerequisite: create `warden_order` faction and a member resident |
| Restore mantles | not used | **C15** (not needed: this design uses tree `say[self.id=…]` lines instead of `mantle.assign`) |

---

## 10. How it is built, and what works with no new code

**Programs.** Eight chapter programs, each one `everyone` audience, under `Data/Storylines/ridgemere3/sump-crown/` (not `programs/`, which three tests count). Each chapter: one or two story-scope shared quests, 8–14 personal quests, up to 4 board quests per town, 30–50 triggers. Quest packs from the other designers feed `crown_hold` and the chapter's counter through world variables (polled) or C12.

**Givers spread out.** Every hub has 9 choices shared by every live story. The campaign never puts more than two offers on one resident in a chapter, and avoids innkeepers who host other packs; it uses the cast's own residents first.

**Today-only version (no engine changes).** The whole campaign runs on fixed goals sized for 5 wolves; beats on `hour` polling; `board.post` and `story.tell` with pre-written branches; `npc.offer` chains; `deed.award` recognition; the Grey Captain's band as a `story.area.spawn` camp with Hollis moved by the story; figures filled by the DM through `save_figure` at each chapter's start; gossip limited to deeds, events and figures. What it loses: town gossip about the campaign (§6, needs C8), named crier calls (C1, N1), titles and ranks (C3, C4), the fightable Grey Captain (C10) and the talking assessor (N4).

**Build order for the engine items** (smallest first, most value): C8 `talk.remember` (unlocks all of §6), C1 text variables, C13 events, C9 `figure.set`, C2 `audience`, C5 `npc.greet`, C11 `npc.spare`, C3/C4 ranks and titles, N3 story items, C10 `foe.add`, C14 `npc.post`, N1, N4, N5, N6, N2.

**Checks the author must do by hand** (approval misses them): every objective `match` id is a placed resident; every `board` town is one of the eight; every `character.grant` purse holds the coin; no ids or personal names in library lines; every capitalised place or title in a library line is on the allow lists (add Moot, Long Night, Concord, Highmere, Rainward, Council Press, Counting House, Hundred Steps, Sunken Street if `dialect_check` reports them); no "dog", "god(s)" or "tells"; Fenshore lines use mirebridge words only; Saltwell borrowed words only "wadi" and "Baba"; every Data/Talk file passes `talk_check --validate`, `talk_lint` and `dialect_check`.

**DM steps before publishing.** Fill `live.figures` (ruler: Dagfinn) if C9 is missing; decide to keep Dagfinn's "reigning" post (this design needs it kept); `npc.protect` and, until C11, mark `live.npcs.protected` for Dagfinn, Ottilie, Oda, Gerrit, Oskar, Ambrose, Halvard, Corra Slagmore and Rosalind for the campaign's span (and clear it at the end); create `warden_order` for the epilogue hook.
