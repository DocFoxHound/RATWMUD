# 86. Quests and the Ninth Howl: quest packs, the first grand campaign, its cast and talk, and the triggers they need

Draft, 2026-11-22. A design; all its engine phases are built (2026-11-24: "Phase 1a as built" to "End of slice as built", on branches `r3-nh-p1a` to `r3-nh-p9`). Surveyed at `17f6db3b` on `ridgemere2` (the Story Maker as doc 85
left it, all eleven of its phases built). World: **Ridgemere 3** (key `ridgemere3`, database `ratw_ridgemere3`, world
id `upper_accord`), read only. Every resident, place, item, purse and event named here was checked against that world
on 2026-11-22; nothing in the world was changed.

Read doc 85 first: it describes the Story Maker as built, including the Campaigns console, steering (Phase 2: the
command inbox, Go to phase, Send signal, Hold) and explaining (Phase 3). This doc says what we will put into the world
with it, and what the Story Maker still lacks to carry it.

This doc stands alone, but it is a summary. The detail lives in the design files, committed beside it under
`artifacts/quests/design/`:

| File | What it holds |
|---|---|
| `campaign.md` | The Ninth Howl, chapter by chapter: premise, state, readiness, every quest, branch, scar, ending and talk table |
| `quests-small.md` | 48 small quests, the stage pack's program and its 12 small entries |
| `quests-medium.md` | 23 medium quests and 5 medium stage entries |
| `quests-large.md` | 11 large quest lines and 5 large stage entries |
| `cast-and-talk.md` | the cast sheet, every dialogue path, gossip and helpful knowledge by stage and dialect, and where each line goes |
| `engine-spec.md` | every new trigger, effect, field and engine change, in the catalogue's style, with checks, tests and phases |
| `decisions.md` | **binding**: the coordinator's rulings across files (givers, purses, camps, names, pacing) |
| `givers.tsv` | the one giver registry: who carries what |
| `critic.md` | the completeness critic's findings (P0-1 to P2-24), each answered in `decisions.md` §9 |
| `proposal-*.md` | the three competing campaign proposals the design was merged from (weave, crown, wild) |

The surveys behind it are in `artifacts/quests/survey/` (`gaps.md`, `story-format.md`, `talk.md`, `cast.md`,
`world-lore.md`).

Decisions are marked **decided (user, 2026-11-22)** where the user settled them, **decided (coordinator)** where
`decisions.md` rules, and **decided (mine)** where this doc chose and the user can overturn. Open questions are in
section 11.3.

## The short version

1. **We add 104 quests and one nine-chapter campaign.** 48 small, 23 medium and 11 large quest lines stand alone in
   every town; 22 more (the stage pack) follow the campaign's progress. The campaign, **The Ninth Howl**, asks the whole
   realm one question: who reigns after King Gunnar? Every quest feeds it.
2. **Progress is by participation, plus time, plus an admin's push** (the user's answer (a)). A chapter moves on when a
   share of the active wolves have done its tasks and it has run its minimum time, or when an admin presses Advance in
   the Campaigns console. There is no calendar and no deadline. A world of 1 to 5 wolves can always advance.
3. **Every deed is answered at once, in the town, and in the realm.** The wolf's own tracker ticks, the town's howl rises
   and its board says so, and the chapter's bar moves. Four backings, a pledge to a town, and Dagfinn's Question make
   each wolf's choices count at the Moot. Twelve endings, all of them full endings.
4. **Talk follows the story.** Every quest giver speaks in the dialogue window in their own dialect. Ordinary residents
   gossip and give helpful knowledge at each stage, in eight dialects, and every innkeeper can tell a late joiner the
   story so far. About 110 new tree nodes, and NPC-to-NPC scenes for each stage.
5. **The Story Maker carries most of it today.** Act I and every pack can ship with no engine change. The rest needs
   56 new engine items in ten phases (plus the T9 allow-list), **about 217.5 agent-hours**, readiness first (Phase 1a,
   21 hours). Nothing is deferred (answer (a)).

---

## 1. The request

### 1.1 The ask (the user, 2026-11-22)

> okay, what I want you to do now is to first, make sure that our game server is actually running, and then do this:
>
> 1. Populate the world with smaller quests that make sense to this wolf world.
> 2. Populate the world with medium quests
> 3. Populate the world with larger/heavier quests
> 4. Make the first grand campaign. I want you to design a large multi-chapter story for this world that has a main
>    storyline that everyone works toward, and side storylines that individual players partake in. It needs to build
>    up gradually, but players should always feel like they're making a difference.
> 5. Create heroes, villains, and key NPC's for these quests if needed
> 6. Design more talking paths for these added NPC's and for all of these quests.
> 7. Design more talking points for various other NPC's such as gossip, or maybe even helpful knowledge, about various
>    quests.
> 8. If the current trigger-system in Story Maker doesn't support to story, then design the triggers that will.

The game server was checked first: Ridgemere 3's server runs on port 7788 (`Saved/WorldAgent/run.sh`), with its story
runner, built after doc 85's Phase 10 merge. `dm.story_releases` on Ridgemere 3 is empty: nothing is approved yet.
Ridgemere 3 has 777 placed residents (774 alive with a reachable home) and 5 wolves.

### 1.2 The user's answers (2026-11-22), binding

| | Question | Answer |
|---|---|---|
| (a) | Scope and pace | "Build everything": all engine phases and all content. **"But don't limit it to only 40 days. I think progression should be made by participation (percentage of active users accomplishing specific tasks, for example), plus time, and progression would be pushed forward by an admin in DM/Story Maker."** |
| (b) | Do finished story quest lines count toward the Quickening's `closedStories`? | **Yes, capped.** Only the last chapter of a large line or a campaign side story counts, at most 4 per program. |
| (c) | How are foe strengths tuned? | **Combat simulations**, several passes if needed, at most 250 players. |

### 1.3 Where each point is answered

| Point | Answered in | Design file |
|---|---|---|
| 1, 2, 3: small, medium and large quests | section 4 | `quests-small.md`, `quests-medium.md`, `quests-large.md` |
| 4: the grand campaign | section 3 | `campaign.md` |
| 5: heroes, villains and key NPCs | section 5 | `campaign.md` §8, `cast-and-talk.md` §2 |
| 6: talking paths | section 6.1–6.3 | `cast-and-talk.md` §3 |
| 7: gossip and helpful knowledge | section 6.4–6.6 | `cast-and-talk.md` §4 |
| 8: triggers the story needs | sections 7 and 8 | `engine-spec.md` |

---

## 2. What exists, and the gaps

### 2.1 What the Story Maker already does

Doc 85 left a real trigger system: Event + Conditions + Actions, 160 world events (more after its Phase 8), typed
variables at story, player and world scope, `if`/`else`, `wait`, `each`, timers, regions, missions, quests anyone can
pick up, scenes, chapters approved together (`campaign.approve`) and chained with `story.start`, and the Campaigns
console. The stress test (`survey/gaps.md`) checked what a grand campaign plus quest packs need against that code.

**Supported today:**

| Need | How |
|---|---|
| Shared world progress fed by many wolves | a story-scope quest, or a world number variable that many programs `add` to |
| Thresholds that advance chapters | a trigger with `compare` and `enter`/`finish`; `story.start` chains up to 12 chapters with `carry` |
| Per-wolf contribution | a player number with `add … of $actor`, shown as a tracker |
| Personal side stories from choices | player variables, `random`/`pick`, and `quest.offer $actor` from a trigger |
| Repeatable small quests | `repeat` daily/weekly, `join.cap`, `join.limit` |
| Timed world events | `schedule`, `clock`, `calendar is`, `festival.call`, `event.add` |
| Failure that never blocks everyone | per-wolf `fail` rules, deadline exits to a worse phase |
| Many packs side by side | one live run per program id, up to 32 quests and 64 triggers a phase each |

### 2.2 Facts that shape every design file

The survey found these in the code. Each one changed how quests are written.

1. **Effect parameters and texts are literals**: progress messages are pre-written bands.
2. **A giver offer is seen by every wolf** with no quest view, so a gated or chained quest is a journal offer.
3. **Another story does not hear a world variable change**: a chapter polls what the packs feed on `hour`.
4. **Deeds can't be told apart** by `match`: the detail ends in the deed id, and `match` is exact.
5. **There is no fightable named villain**: attacking a resident is a crime; foes are anonymous bands of 6 at most.
6. **`npc.protect` stops attacks but not old age**, and no run's end undoes it.
7. **All stories share a giver's 9-choice hub**; what doesn't fit is dropped.
8. **Shop tills can't pay a story today**: `check_purse` refuses `till:job:<id>`.
9. **Story camps are named `camp_story_<n>`**, and `camp cleared` is not a mission event.
10. **Strangers can't speak or give quests**, and last at most a game day.

### 2.3 The gaps

From `gaps.md` §B, the needs that were partial or missing:

| Need | Gap | Fixed by (section 7) |
|---|---|---|
| Goals that scale with the population (Ridgemere 3 has 5 wolves) | no operand for the active count | C2 `audience` |
| Progress by a share of the active wolves | nothing | **R1, R2, R3** (answer (a)) |
| Progress shown in words ("12 wolves have stood watch") | no fill in texts | C1 |
| Recognition by contribution (top N, tiers) | flat quest rewards; `each` stops at 500 | C3 `rank`, J11 `tally` |
| Titles, town regard, a resident thanking one wolf | none | C4 `title.give`, C5 `npc.greet` |
| Quest chains and prerequisites | giver offers can't be gated per wolf | C6 `requires` |
| The giver speaks while a quest is under way | the window shows only offer and hand-in | C6 `offer.during` |
| Quests cast at run time | static givers | C7 |
| Gossip and knowledge that follow the campaign | no story writes talk memory | C8 `talk.remember`, `belief.plant`; C9 `figure.set` |
| A villain fought and defeated | no hostile named NPC | C10 `foe.add`, `foe.outlaw` |
| Key cast kept alive | old age ignores protection | C11 `npc.spare` |
| Packs feeding the running chapter | variables polled; deeds not told apart | C12a `story.notify`, C12b `starts`/`in` |
| Waking on a variable change | none | C13a `variable changed` |
| Lasting fates (the ruler at the end) | none | C14 `npc.post` |
| Rewards in proportion | literal coins | C1, M2 |

---

## 3. The campaign: The Ninth Howl

Full design: `campaign.md`. It merges three proposals. **Weave** gave the frame (nine voices, pledging to a town's howl,
branches tallied from personal choices). **Crown** gave the opening, the Lantern Writ and the intrigue side stories.
**Wild** gave the recurring wild villain, the Howl Line and the playable finale.

### 3.1 The premise

King Gunnar Harrowgate, Sump-born, died of old age on game day 41. In the Palace lamp-book, in his own paw: *"Should I
go up with no word given, let the lamp be kept by the wolf who keeps the peace of the Sump, until the realm howls a
Moot."* The Palace stood empty five days while the Council of Houses argued. On the sixth dawn, Dagfinn Thornham, a Sump
patrolman, climbed the Hundred Steps and lit the lamp, because no one else had. The work of reigning fell to him. He
still walks his round at dusk.

This is what the world data already says. In Ridgemere 3, Dagfinn really did take "reigning at the Great Palace" on day
46 by the vacancy rule. The campaign makes the engine's accident its first fact. **The DM must not "fix" his post.**

A Moot of the realm's **nine voices** (eight towns and drowned Mirebridge) is called. The Houses race to make it fail:
Ambrose Greatly buys debtors' howls, and a grey-cloaked captain closes the roads. On the Moot night the realm runs one
howl from the Pass and the Waste to the Bay Light, and then the Moot chooses who reigns.

The plot grows out of hooks residents already carry: 41 owe a House factor "by the new moon", 46 saw the Chancellor
weep in the Rain Hall, 40 carry a letter for Lord Grayrock "who is never home", 35 keep a list of who pays the Watch, 7
saw "something big move under the ice". The bandit camps sit where the howl must run.

### 3.2 Shape

| | |
|---|---|
| Programs | `ninth-howl.1` … `ninth-howl.9` in `Data/Storylines/ridgemere3/ninth-howl/`, plus `nh-purses` and `ninth-howl.cleanup` |
| Acts | **Act I "The Lamp"** (Ch 1–3), **Act II "The Howls"** (Ch 4–6), **Act III "The Moot Night"** (Ch 7–9), each one `campaign.approve`, chapters chained by `story.start {carry:true}`; an **interlude** between acts waits for the DM |
| Audience | every chapter `{"kind":"everyone","days":30,"cap":5000}`, `activeDays: 18` |
| Side storylines | 3 to 5 a chapter, 36 in all, plus one **door quest** a chapter for late joiners |
| Per wolf | 4 trackers: "Your voice", "Your town's howl", "This chapter: done", "This chapter: needed" |
| Length | none fixed. Typical: about 24 real days at 200 wolves, about 30 at 5 (a game day is 4 real hours) |

### 3.3 The chapters and their readiness rules

Each chapter has a **working phase** with a `ready` block, and a **closing phase** that announces, times and plays its
set piece. The participation tasks are the chapter's named tasks; a wolf counts once a chapter.

| Ch | Title | Centre | Participation tasks | % of active | floor | min (game days) | soft max | The big beat |
|---|---|---|---|---|---|---|---|---|
| 1 | Where Is the King? | the Sump, the Oakhaven road | carried a Summons; fought at the Alder Mere camp; found the King; gave to a Line post | 30 | 3 | 10 | 30 | the chalk night (chapter day 3); the Herald's Word calls the Moot |
| 2 | By the New Moon | the money quarter, Larkfield, Frosthollow | freed a debtor; finished a side story or the door quest | 25 | 2 | 12 | 36 | the Reckoning: a new moon within 3 game days, else the next dusk |
| 3 | Two Long, One Short | the Greenholt ridges, Glasswater, the Watch | doused a signal lamp; fought at Glasswater; took a Watch-list piece; gave to proj-1 | 30 | 3 | 12 | 36 | the Road Feast eve |
| 4 | The Nine Howls | every square, the Sunfields stone, Saltwell's pool, Highmere | sang a chorus; heard part of an old howl; found a Mirebridge wolf; hauled the silver; sat with Bramble | 30 | 3 | 12 | 42 | opens at a feast; the Howl Relay is called on readiness |
| 5 | Letters for an Absent Lord | Grayrock Hall, the Rainward Hills, the Oakhaven cut | walked a bound; pulled a stake; carried a letter; broke bounders; finished a side story | 30 | 3 | 18 | 48 | Black Cloaks on the Pass Road: the Warden-Envoy comes down |
| 6 | The Lights in the Fen | the Sallow Fens, the Mirebridge crossroads, the city | gave a name to the Book; took a false light; walked the path; fought a southern band; carried truce tokens; gave to the post | 30 | 3 | 18 | 48 | fog in any season; the Rite of the Tide at the Book's 30 names |
| 7 | Harvest Under Arms | the Larkfield ford, Longmead, the Downs Road | mustered for the ford | 25 | 2 | 6 | 30 | the siege: three nights, called on readiness |
| 8 | The Bell and the Writ | the Upper Mere ice; the City Office and the crypt | heard a testimony; searched; took a bearing; worked the haul; taught Tansy; the List or the Count | 30 | 3 | 24 | 60 | the freeze (real, or early); raising the drowned bell; finding the Lantern Writ |
| 9 | The Moot | all 13 posts of the Howl Line; Market Square | kept a post; carried lanterns; escorted the bell; raised a silent voice; answered the Envoy | 30 | 2 | 12 | 36 | the Moot night, at dusk the next game day after the call |

The Grey Captain has a beat in **every** chapter, from the camps in Ch 1 to the Moot-night strike in Ch 9.

### 3.4 The participation model and the admin's push

**decided (user, 2026-11-22; coordinator, `decisions.md` §6).**

> **Ready = (participation met AND the minimum time served) OR an admin push.**

1. **Participation.** A stated share of the **active** wolves are in the chapter's list `ch<n>_did`. Active means seen
   within the last 18 game days (3 real days). A wolf who goes quiet leaves both counts, so the share stays honest.
2. **The floor never asks for more wolves than are active.** need = max(min(floor, active), ⌈percent × active / 100⌉),
   capped at 150, never below 1. With 2 active wolves and a floor of 3, both must take part; with 1, that one.
3. **The minimum time** (held time does not count) gives even a fast crowd the chapter's story and side stories.
4. **The admin's push.** **Advance…** in the Campaigns console, at any moment; **Advance anyway…** when not ready, with
   the story's typed title. Both need a reason, and `dm.audit` keeps who, when, why and the readiness the admin saw. The
   world list `nh_pushed` tells the epilogue which chapters were moved by hand.
5. **A ready chapter moves on by itself** (`auto: true`). The push is the accelerator and the safety.
6. **The soft maximum** raises a "needs you" alert instead of moving: *"Ready to push: Ch 3 after 36 game days: 9 of 41
   active wolves did its tasks (13 needed). Advance anyway, or wait."* The switch `ch<n>_auto` makes the soft maximum
   count as participation met.
7. **The bar is not a gate.** The shared goal decides the good or worse road, not the timing; `ch<n>_wait_bar` makes
   readiness wait for it too.
8. **Before Phase 1a lands**, a `dm signal` exit "Move the chapter on" stands in for the `ready` block; the DM sends it
   by the same rule. The run then moves onto the `ready` version by `migrate`.

**Calendar moments are flavour.** A set piece waits for a moon or a festival only when one falls within 3 game days;
otherwise it plays at the next dusk. The Moot is "the Midwinter Moot" only in winter, else "the Moot of the Nine
Howls". Nothing reads a game day.

### 3.5 How a deed is answered: three rungs

| Rung | When | Who sees it | How |
|---|---|---|---|
| **Spark** | at once | the wolf | "Your voice" ticks; the giver thanks them; a first deed of a kind writes a `deed.award`, so residents greet the wolf "You … didn't you?" |
| **Ember** | within a game day | the town | each town's strand has three steps (*stirring*, *rising*, *raised*): a board post signed by the town's Keeper, a plain narration to the region, and the town talk key `nh_voice` |
| **Blaze** | the chapter | the realm | a signed board post on all eight boards, a lasting change (a post stands, a figure is set, a relation moves), and every town's talk moves to the next stage |

**Scaling.** A chapter's goal is base + k × helpers, so it tracks the wolves who play. Deeds are worth 2 (a camp or a
set piece 10, a side story 6; packs 2, 4 or 8), doubled in the far towns, so crowds leave Market Square. Points past the
goal go to `nh_surplus`, which buys extras: a post, a festival, a better ending clause.

### 3.6 The nine voices and the Howl Line

At any town's **Keeper** a wolf may **pledge** to that town's howl; their deeds anywhere add a quarter-share to it. A
raised voice stays raised, and **a raised town answers on the Moot night with no wolf there**: a lone wolf can raise
Oakhaven in an evening. The **Howl Line** is 13 posts from the Pass and Saltwell to the Bay Light: six town projects
(three already open: proj-1, proj-2, proj-3) and seven story regions shown as gold map markers.

### 3.7 Branches and endings

**Many hands, one weave.** Each big branch sums many personal choices, each weighted by the chooser's "Your voice".
The totals are world numbers updated at each choice, so the 500 cap of `each` never bites. Losers still get their own
personal ending. Packs read the 34 branch results and never write them.

**Four backings** are each wolf's side at the Moot: **Lamp** (Dagfinn, if he stands), **Blood** (Ottilie), **Moot** (a
Regency), and **Purse** (House Greatly's choice, a bad side with its own errands, coin and title). **Dagfinn's
Question** ("Should I stand?") is asked in Ch 2–8; the latest answers decide.

**The Moot night is played.** Both arms of the Line start at dusk; a post answers if a wolf howls there, its raised
town's Keeper is there, or its keeper-quest wolf is in town. One silent post never ends the night. At the Pass the
Captain attacks; if enough has been learned, Ulric meets his brother Hakon and **the Turn** plays (let him go, take him
to the city, or no quarter). Then the Moot counts nine community voices, four House votes, the free wolves' voices and
the bought ones, by a rule posted on every board.

| Who reigns | When |
|---|---|
| **Queen Ottilie, with the Lamp beside her** | Blood wins |
| **The Lamp-King** | Lamp wins (Dagfinn stood) |
| **The Regency of the Moot** | Moot wins, or the top two are within one voice |
| **The Bought Crown** | Purse wins; the sequel "The Debt of Ridgemere" |

| The Concord's verdict (the Warden-Envoy, through Gorm) | Score |
|---|---|
| **The Realm Answers**: the Wardens leave by the Pass | 14 or more |
| **The Realm Holds**: one Warden stays until spring | 9–13 |
| **The Realm Bends**: Wardens in every silent town | 8 or less |

Four rulers by three verdicts: **12 endings, every one a full ending.** Lasting after: figures, posts standing, faction
relations, the ruler's post (C14), and realm talk kept for 90 days.

### 3.8 Recognition

| What | Who | How |
|---|---|---|
| Deeds | every wolf | lower-case deed phrases that residents and criers repeat |
| Chapter tiers | each chapter's merit (≥ 3, ≥ 8, top 3) | a deed; a resident's named greeting (C5); a title and a board post (C3, C4) |
| The Moot Roll | every contributor | a journal note and a deed |
| Howl-friend of <town>; Voice of the Realm | each town's top pledged wolves; the realm's top | titles, the Keeper's thanks by name, 10 pennies from the town |
| The Lantern | the single top wolf | becomes the realm's hero figure, named by every hero line |
| Quickening | each wolf | finished campaign side stories (`closure: "side"`, at most 4 a chapter) and large lines' last chapters count toward `closedStories` (J3, answer (b), capped at 4 a program) |

Tiers count only that chapter's merit, so a wolf who arrives in Ch 6 can top Ch 6.

### 3.9 Failure never blocks

- A chapter closed with its bar short takes its **worse road** and adds a **scar** (`nobody_came`, `ford_burned` …):
  harder fights, higher raise marks, a darker line at the Moot. Every loss can be won back later, at a price.
- Personal quests fail per wolf and come back weekly. Every branch has a default. **The Moot always sits.**
- **Late joiners** get a door quest each chapter that needs no earlier flags, a pledge offer, and a recap at every
  innkeeper.

### 3.10 Rules kept throughout

Wolves and pups; the Bright One and the Dark One only; "private messages"; male or female only. Every word pre-written;
dialect by upbringing. **No new residents**: three strangers (the Warden-Envoy, the Grey Captain, Hakon) come and go,
dressed `like:` a resident who is never cast. Reachable interiors only. **A `story.tell` banner with a face is not the
dialogue window.**

---

## 4. The quest packs

### 4.1 Counts

| Tier | Quests | Takes | Programs | Shape |
|---|---|---|---|---|
| Small | 48 + 12 stage | 10–30 real minutes, 1–3 steps | `nh-pack.small.city` (1–19, 46–48), `.north` (20–31), `.south` (32–45) | errands, hunts, foraging, lost pups, scent trails, stall help, a night watch |
| Medium | 23 + 5 stage | 30–120 minutes, 2–5 steps, ends one of two ways | `nh-pack.medium.city` (Q1–Q7), `.greenholt` (Q8–Q12), `.south` (Q13–Q19), `.north` (Q20–Q23) | a choice at the giver decides the ending; towns answer crowds |
| Large | 11 lines + 5 stage | several sessions | one program per line, `quests-large/<id>.json` | three chapters and a shared **reckoning** that turns the roads wolves chose into one lasting state |
| Stage | 22 (12 small, 5 medium, 5 large) | by tier | `nh-pack.stage` | follow the campaign's chapter (phases `quiet`, `s12`, `s3`, `s4`, `s56`, `s78`, `s9`) |

**104 quests** (counting each large line once), on **244 givers and targets** in the registry.

### 4.2 By town

| Town | Small | Medium | Large lines |
|---|---|---|---|
| Ridgemere (city, incl. the estates) | 20 | 7 | 3 (`lq-runnel-fever`, `lq-wreckers-light`, `lq-vanishing-thief`) + the realm-wide `lq-long-run` |
| The wild near the city | 3 (2 near the city, 1 on the Greyteeth) | — | — |
| Coldwater | 5 | 3 | 1 (`lq-old-brown`) |
| Larkfield | 4 | 2 | 1 (`lq-clipped-silver`) |
| Southmeadow | 3 | 3 | 1 (`lq-blight-and-boar`) |
| Fenshore | 4 | 2 | 1 (`lq-eel-famine`) |
| Saltwell | 3 | 2 | 1 (`lq-salt-road`) |
| Oakhaven | 3 (1 board-only) | 2 | 1 (`lq-mast-cedars`) |
| Frosthollow | 3 (1 board-only) | 2 | 1 (`lq-black-hearths`) |

The stage pack spreads its 22 over the city, Coldwater, Larkfield, Southmeadow, Fenshore, Oakhaven and the roads.

### 4.3 The large lines

| Line | Giver | Where | Villain or rival | Feeds |
|---|---|---|---|---|
| The Salt Road | Orm Sootgate | Saltwell, the Red Waste → the city | a salt hoarder | grey paw-mark receipts (Ch 5); Saltwell's voice; threat |
| The Mast Cedars | Ivy Stonehollow | Oakhaven, the Rainwood, Cedar Sound | a Grayrock shipwright | Oakhaven's common right (Ch 5) |
| The Old Brown of the Greyteeth | Jessamy Greyford | the Greyteeth fold, Greytarn | grey-cloaked rustlers; a bear | the Captain's northern band (Ch 3) |
| Clipped Silver | Pike Chalkley | Larkfield → the Market Hall → the Kestrel workings | a coiner in debt; a false beam | the factors' new moon (Ch 2) |
| Blight and Boar | Sloe Harvestby | Southmeadow, the Sunfields, Southwood | the blight; a boar | the harvest (Ch 7) |
| The Runnel Fever | Theodora Wickley | the Sump, Cistern Court, the Wharf | a draught-seller; a careless tanner | the Residence's cough (Ch 4) |
| The Wreckers' Light | Linden Sedgeditch | the North Headland, the Wharf | a lamplighter with a false lamp | the wharf dead (Ch 5), the false lights (Ch 6) |
| The Long Run | Grete Dimmock | every square | the Carters' Yard (a friendly rival) | a rehearsal of the Howl Line |
| The Eel Famine | Ivo Sootridge | Fenshore, Withy Creek, the weir | the Fish Hall's weir | float lamps and the drowned (Ch 6) |
| The Thief Who Wasn't There | Petra Vance | the Heights, the Sunken Street, the Sump | a lender; a pup who isn't Quickened at all | Warden attention (Ch 5–9) |
| The Black Hearths | Marl Craghearth | Frosthollow, the Upper hearths → the Charcoal Store | charcoal thieves who damp the clamps | Frosthollow's voice; threat |

### 4.4 Packaging into programs

- **Open audience**: a wolf joins by taking a quest, and every quest is per wolf. Files go under
  `Data/Storylines/ridgemere3/`, never `programs/` (three tests count the files there).
- **Within budgets** (32 quests, 64 triggers a phase, 64 offers a program): so three small programs and four medium.
- **Large lines** are one program each, whose phases are the world's state: `open` (chapters 1–2), `reckoning`
  (chapter 3's roads, a 3-game-day tally), `after_<road>`, and back to `open` each season.
- **Gates** read `nh_stage` and branch results only, never a game day.
- **What a pack writes** to the campaign: `nh_realm` and `nh_voice_<town>` (distance-weighted), `nh_threat` lowered by
  bounties, `nh_names` in Ch 4–6. **Never** branches, the stage, surplus, posts or Keepers. Credit to the wolf's own
  tracker needs C12a (Phase 4) or J2 (Phase 8).
- **Scaling**: prop errands allow 3–6 at once; unique-object quests allow 1 or 2, repeat weekly, fail after a game day
  and point to another quest nearby when taken. Large fight chapters take 12 over two spawn regions; spawns grow with
  wolves present.

### 4.5 The stage pack

One program, `quests-stage/nh-pack.stage.json` (`quests-small.md` §2.10). An `hour` trigger reads `nh_stage` and enters
the matching phase. Examples: bread for the King's wake (`s12`), stone and timber for the Line posts (`s3`), the chorus
that sang the burying howl at a wedding (`s4`), blankets for the ford (`s78`), benches for the Moot (`s9`). One is gated
on a branch: bread for the burned houses, only if the ford burned.

### 4.6 The purse rule

**decided (coordinator, `decisions.md` §3).** Story coin is real coin from real purses, never minted.

| Purse | Holds (2026-11-22) | Day cap |
|---|---|---|
| `town:ridgemere` (the treasury) | 1,355 | 135 |
| each town store (`town:coldwater` … `town:frosthollow`) | 212–248 | 21–24 |
| `till:job:rm_eleanor_saltcot` (Greatly Bank; after SQ2 only) | 161 | 16 |

1. **A per-purse, per-day counter** `coin_<purse>_day`: a grant happens only if counter + amount ≤ cap and the floor
   switch `coin_<purse>_ok` is on. **Otherwise in kind** (bread ×2), with a "thin purse" thanks; the deed and the feed
   land as usual.
2. **A floor of 60**: below it a purse pays no story coin until it is back at 80.
3. **One owner**: the always-on program `nh-purses` resets the counters each `day begins` and flips the switches on
   `purse crossed`. Every other program only reads and adds.
4. **Amounts per wolf**: small 2–4; medium 6–15; large at most 12 a chapter and 35 a line; campaign at most 10 from a
   store and 20 from the treasury per deed.
5. **No shop till before SQ2** (Phase 1b): until then the treasury or town store pays, worded as the shop paying.

At 500 wolves the treasury pays at most 135 pennies a game day and each store 21–24; the rest are paid in bread.

### 4.7 The giver rule

**decided (coordinator, `decisions.md` §1).** `givers.tsv` is the one registry (244 residents, 247 rows).

1. **At most one pack quest per resident**, across all packs (giving it or holding any of its choices). A target holds
   no hub item, and is a target in at most two pack quests.
2. **Campaign cast and Keepers carry no pack quests**, and at most 2 campaign offers a chapter.
3. **Innkeepers give helpful knowledge and recaps only**, and are never Keepers.
4. **Look-alikes** (the strangers' `like:` residents) give nothing.
5. **Small towns**: where every free resident is taken (Oakhaven has 4, Frosthollow 5), a quest is **board-only**: a
   board posting, a journal offer on entering the square, and a gift to a resident target.

A short check over `givers.tsv` enforces these (`decisions.md` §1.2.7); it passes today.

---

## 5. Heroes, villains and key NPCs

Every one is a **placed, living** resident, checked on 2026-11-22, except the three strangers. Full sheets (home,
quarter, temperament, looks, voice, arc by chapter, fate by branch): `cast-and-talk.md` §2.

### 5.1 Heroes

| Who | Id | Voice | Arc | Fates |
|---|---|---|---|---|
| **Dagfinn Thornham**, the lamp-keeper | `rmx_dagfinn_thornham` | m 31, mirebridge speech, calm | a bewildered patrolman → walks the fens → hears the writ was meant for him → stands or steps aside | the Lamp-King; the Queen's Lamplighter; the Moot's steward; the wolf at the door of the Bought Crown |
| **Ottilie Harrowgate**, the blood heir | `rm_ottilie_harrowgate` | f 39, plain, warm, coughs in the damp | the frail heir → tended → the Blood candidate | Queen; councillor; the Moot's first petitioner |
| **Herald Brenna Abbot** | `rmx_brenna_abbot` | f 52, plain, warm; owes a factor | the campaign's voice and Ridgemere's Keeper; reads the Moot | Speaker of the Moot under a Regency |
| **Cressida Cotton** | `rmx_cressida_cotton` | f 19, frosthollow, cold and frank | the first quest giver → keeper of the waystation post | Dagfinn's sergeant |
| **Sergeant Bettany Nailley** | `rm_bettany_nailley` | f 26, plain, ambitious | the Watch lists' champion → the ford escorts | leads the Watch |
| **Rosalind Greatly** | `rm_rosalind_greatly` | f 22, plain, bookish | the whistleblower with a ledger that doesn't add up | Greatly's conscience; the unwilling Bought Queen |
| **Speaker Gerrit Mudhand** | `rm_gerrit_mudhand` | m 55, plain, chatty | keeper of the Moot's roll; patron of the Moot backing | Speaker of the Council |
| **Ulric Stonevein** | `cty_ulric_stonevein` | m 30, frosthollow | smells his brother's harness → the lamp-oil list → the Turn | parts from Hakon, or mourns him |
| **Tansy Reedmere** | `tn_cw_tansy_reedmere` | f 14, coldwater | the "something big" under the ice: the water obeys her | the realm's first kept Quickened; pledged to the Wardens; hidden |
| **Solveig Tarwell** | `rm_solveig_tarwell` | f 44, plain, quiet | the wharf names → the Rite of the Tide → the Bay Light bell | rings the drowned bell |
| **Skye Eelbank** | `tn_mb_skye_eelbank` | f 50, mirebridge speech | the path with no note → named Mirebridge's Keeper | Mirebridge's voice at the Moot |

### 5.2 Villains and rivals

| Who | Id | Motive | Beaten by |
|---|---|---|---|
| **Ambrose Greatly**, master of the Counting House | `rm_ambrose_greatly` | "his heir to inherit a city" | ledgers and votes, **never teeth**. Shamed, owed a favour, or Lord Protector. |
| **The Grey Captain, Eskar Rimedrift of Deepvein** | `cty_eskar_rimedrift` (unplaced; a stranger and foe only) | buy back Deepvein for the miners Frosthollow turned out | turned, beaten (held a game day at the Keep's cells, judged in board and talk), dead (only by explicit choice), or fled |
| **Desmond Hollis**, the dismissed guard | `rmx_desmond_hollis` | lost his Palace post to a Sump patrolman | held, redeemed, the pillory; or downed, never killed, at the ford (C10 `foe.outlaw`) |
| **Queen Oda Harrowgate** | `rm_oda_harrowgate` | the purse, and her daughter | reconciled, or going home to the lake |
| Corwin Fell, Lorn Hedgeton, Fenwick Plankworth, Hilde Dripby, Corra Coalcot | `rm_corwin_fell_`, `cty_lorn_hedgeton`, `cty_fenwick_plankworth`, `rm_hilde_dripby`, `rm_corra_coalcot` | land, coin, a mine, a secret, theft | shamed, turned, confessed, paid off, reported |

Ambrose pays the Captain with a promise he cannot keep: Rosalind's ledger shows the Deepvein deed pledged to three buyers
already.

### 5.3 Key and turnable NPCs

| Who | Role |
|---|---|
| Chancellor Oskar Gullworth `rm_oskar_gullworth` | the weeping Chancellor, owned, not wicked: confesses, is exposed, runs, or pays |
| Gorm Plankgate `rm_gorm_plankgate` | **the Wardens' Eye**: the first `warden_order` member, and the Envoy's only voice |
| Captain Corra Slagmore; Gudrun Fell and Osric Timberland; the Grayrocks; the four House heads | the indebted Watch; the match that shapes the quarrel; the forty letters; the Council's votes |
| Kell Greyton, Willa Coldwick, Ingrid Kendrick, Piet Lumley, Ulla Rusthand and others | the ice, and the small lives the campaign answers |

### 5.4 The Keepers

| Community | Keeper | Fallback if dead |
|---|---|---|
| Ridgemere | Herald Brenna Abbot | Corra Coldridge `rmx_corra_coldridge` |
| Coldwater | Fallow Lakewick `tn_cw_fallow_lakewick` | Myrtle Stillford |
| Larkfield | Aspen Marshholm `tn_mb_aspen_marshholm` | Dun Harvestley |
| Southmeadow | Iris Meadowfield `tn_sm_iris_meadowfield` | Rusk Plummeadow |
| Fenshore | Roan Pikewater `tn_mb_roan_pikewater` | Jory Corncroft `cty_jory_corncroft` |
| Saltwell | Thistle Stonepick `tn_fh_thistle_stonepick` | Lavinia Mudworth |
| Oakhaven | Ulla Rusthand `cty_ulla_rusthand`, if her name is cleared in Ch 1 | Cairn Oakhaven |
| Frosthollow | Ulric Stonevein | Aster Coldfell |
| Mirebridge | Skye Eelbank, named in Ch 6 (Imogen Nailridge takes pledges until then) | Tamsin Eelbank |

### 5.5 Strangers and foes

| Who | How | Speaks |
|---|---|---|
| **Warden-Envoy Ingeborg Rimewater** | `visitor.add`, `like: tn_sm_brisk_plumby`, Ch 5 end – Ch 9 | only through Gorm, by Concord custom |
| **The Grey Captain** | `visitor.add` at the parley, `like: cty_stone_sunton`; a C10 champion foe at the Pass | through Desmond, or by letter; T3 gives him a scene panel |
| **Hakon**, Ulric's brother | `visitor.add`, `like: rm_palle_bilgemarsh`; a foe at the Pass | in the Turn's scene (T3) |
| The Captain's companies, bounders, cudgels | `story.area.spawn`, `bandits.call`; C10 bands later | — |

No resident is made, no unplaced resident is brought in, and no stranger stays.

### 5.6 Protection and sparing

At Ch 1's setup: `npc.protect` and C11 `npc.spare` on the heroes, Ambrose, Oskar, Gorm, the nine Keepers and their
fallbacks, the post keepers, Tansy and Gorse, and six more of the old (`campaign.md` §8.6). **Desmond is never
protected**, because the ford must be able to down him. Ch 9's epilogue unprotects and unspares everyone, and so does
`ninth-howl.cleanup` if any act is stopped early, because `npc.protect` is permanent.

---

## 6. Talk

Full plan: `cast-and-talk.md`. The writing rules hold for every line: at most 160 characters per tree alternative, no
ids in speech, no personal names in library lines (roles, or `#world.figure_*#` once C9 sets the figures), weather and
time words only under their gate, and frames in symbols (`#greet#`, `#thanks#`, `#address#`, `#oath#`) so each speaker's
own dialect fills them.

### 6.1 Where a resident's words can travel

| Code | Vehicle | Lives in | Symbols filled? |
|---|---|---|---|
| Q | a quest's offer block (`say`, `thanks`) | the story program | yes |
| O | `npc.offer` choice and reply; from Phase 4 the quest's `offer.during` | the story program | reply: yes |
| L | `npc.line`, a greeting add-on for one phase | the story program | no, until T5 |
| T | a tree greeting `say[self.id=<id> world.mem.nh_stage=<key>]` | `Data/Talk/dialogue/*.tree` | yes |
| N | a tree node: a campaign talking path the story **hears** through the existing `dialogue node` event | `resident.tree` | yes |
| P | a per-wolf line gated on pair memory (C8), or a one-off `npc.greet` (C5) | tree + story effect | yes |
| S | a scene beat, each speaker in their own panel | the story program's `scenes[]` | no |
| G | gossip and helpful knowledge | trees, `.talk` rules, NPC-to-NPC scenes | yes |
| B | `board.post`, `journal.note`, `deed.award` | the story program | no |

**Never**: `story.tell {from: <resident>}`. Proclamations are signed board posts plus the speaker's own window line.

### 6.2 Tree nodes are story buttons

The best tool the talk survey found: when a wolf's window moves to any tree node, the game already writes the event
`dialogue node` (actor the wolf, target the resident, detail the node id). So a campaign conversation is written as
ordinary library nodes in `resident.tree`, gated on `world.mem.nh_*` and `self.id`, and a chapter trigger on that node
records the wolf's choice. This shows each wolf their own path, costs no hub room and none of the story's 64 offers, and
is filled in the speaker's voice.

The window map: one new choice on `news` ("What's all this talk of the Moot?") opens `nh_story`, the recap and the
stage's news. From there: `nh_help` (where to lend a paw), `nh_who` (who's behind it), `nh_ask` (who to ask), one
**cast slot** that carries a personal topic for every cast member (disjoint by `self.id`, so adding cast never grows the
node), the pledge, the backing, and the two ways out. About **110 nodes**; no hub gains a choice.

### 6.3 Quest dialogue paths

Every quest gives its giver's words: offer, during, hand-in, and after each ending, in the giver's dialect.

- **Campaign cast** (`cast-and-talk.md` §3.1–3.24): a greeting per stage, a talking path per chapter, branch lines.
  Dagfinn's round in Ch 1: *"Why aren't you at the Palace?"* "Because the Sump doesn't stop needing a round walked, no?
  The Palace has plenty of wolves. The alleys have me." Also Dagfinn's Question, Ambrose's three steps, each Keeper's
  pledge, Bramble's Restdays, Gorm's reasons, and the Envoy's three questions through Gorm.
- **Side stories** (§3.22) and **pack givers** (each pack file): offer, during, hand-in and after. A close is a journal
  note today, a pair-gated tree line with C8, then a one-off `npc.greet` with C5.
- **Targets** speak through their own tree line, gated on the wolf's quest (N2).

### 6.4 Gossip and helpful knowledge, by stage

Every line is gated on `world.mem.nh_stage=<stage>` plus `self.dialect`. There are ten stage keys (`king`,
`moot_called`, `reckoning`, `signals`, `howls`, `letters`, `lights`, `harvest`, `bell`, `moot`) and an epilogue. **At
least six lines per dialect per stage**, of seven kinds: **R** rumour, **W** where to go, **A** who to ask, **V** a hint
about the villain, **!** a warning, **H** thanks to the heroes, **X** a recap. Every town hears its own helpful
knowledge, in its own dialect, at every stage, and helpful lines point at real content only. One NPC-to-NPC scene per
stage, for example:

```
scene nh_ch3_lists
when topic=gossip world.mem.nh_lists=gathering
a : There's a wolf going round asking who keeps a list of who pays the Watch. | Somebody's putting the Watch lists together.
b[quarter=sump] : About time. The Sump's paid for the Watch twice, once in coin and once in looking away.
b : Careful. Lists like that get wolves hurt. | I'd keep my head down, if I were asked.
a : #bc_it_is# | Bright One keep whoever's carrying it.
```

The packs add their own: two town keys per town for the small pack (`sq_on`, `sq_last`), `mq_*` keys for the medium,
`lq_*` keys for each large line's roads and results, `st_*` for the stage pack, and thin-purse lines in eight dialects
gated on `nh_coin=thin`.

### 6.5 Where the talk lives

| Piece | File |
|---|---|
| Campaign barks and smalltalk | `Data/Talk/rules/story_ninth_howl.talk` |
| Campaign NPC-to-NPC scenes | `Data/Talk/scenes/common/story_ninth_howl.talk` |
| Window lines, paths, gossip, helpful knowledge | `say[...]` lines and new `nh_*` nodes in `Data/Talk/dialogue/resident.tree`; greetings in `watch.tree`, `church.tree`, `official.tree`; recaps in `inn.tree` `greet` |
| Medium pack lines and closes | `Data/Talk/rules/story_mq.talk`, `Data/Talk/scenes/common/story_mq.talk` |
| Small and large pack lines | `resident.tree` `news` and `gossip`; recaps in `inn.tree` |
| Rumour claim kinds | `Data/Talk/claims.json` (`story.rumour`, `story.cleared`) |
| Window facts | `Data/Talk/facts.json` (`player.quest.*`, `player.story.*`, `player.title`) |
| Place names | `tools/dialect_allow.json` "places" (the lists in `campaign.md` §10.2 and each pack's talk section) |

### 6.6 How talk follows the campaign's state

1. **The story writes talk memory** (C8 `talk.remember`, realm, town, resident and pair scope, keys prefixed `nh_`,
   `sq_`, `mq_`, `lq_`, `st_`). Each chapter moves `nh_stage`; each branch writes its own key (`nh_ford=burned` and
   some 40 more). Every town's gossip changes with them.
2. **Figures** (C9) stay current, so existing `#world.figure_villain#` lines name Ambrose from Ch 2, and the realm's top
   wolf becomes the hero at the end.
3. **Rumours** spread as beliefs (`belief.plant`) and false ones fade (`belief.forget`): that clears two wrongly blamed
   residents.
4. **The epilogue keeps** the ending's keys for 90 days.
5. **Before C8**, every line is valid but silent (the test town has no `nh_` memory, so the talk scorecards don't
   move); the DM can seed a key by hand.

---

## 7. The new triggers and engine items

Full catalogue entries, semantics, files, checks, undo, tests and guide sections: `engine-spec.md`. Ids are the ones
the design files use: **C** from `gaps.md`, **J** from the campaign, **R** from answer (a), **N**/**V** from the spec,
**SQ**/**M**/**L**/**T** from the small, medium, large and talk plans. W = needs "may act on players' wolves" on a hosted
world; B = needs a builder or the owner; U = undone at the run's end.

**Every change to conditions, operands, actions or `match` is made in both engines** (`tools/story_triggers.py` and
`Core/RatwStoryTriggers.cpp`), with a parity case in `test_story_native.py`.

| Item | What it does | Phase |
|---|---|---|
| T9 | allow-list words for the new place names and titles | 0 |
| **C2** | operand `audience` (all / active / online), the program's `activeDays`, the runner's `seen` map seeded at launch; arithmetic `times`, `div` | **1a** |
| **R1** | condition `participation`: a share of the active wolves in a list, with a floor (never above the active count) and a cap; operands `percent` and `among` | **1a** |
| **R2** | a phase's `ready` block (participation, least days, values, missions, quests; `auto`, `then`, `slowAfterDays`, `slowNeedsYou`, `autoAfterDays`), the exit `ready`, the latch, the held clock, the command `advance`, the alerts `ready` and `slow` | **1a** |
| **R3** | the Campaigns console: readiness bars per running chapter, Advance… / Advance anyway…, Hold beside them, the list's "ready" column and alert, the chapters bar, the guide's "Moving a chapter on" | **1a** |
| V1, SQ2, V3 | approval checks what it misses (objective ids, boards, purses, residents **alive**); shop tills pay (`till:job:<id>`), every purse checked against the checkpoint; a guide fix | 1b |
| J1 | `npc.offer` keeps a list of wolves, so per-wolf offers don't overwrite each other | 1b |
| C11 | `npc.spare` / `npc.unspare` (old age skipped; U); `npc.protect` gets `lasts` | 1b |
| C9 | `figure.set` / `figure.clear` (a resident, a name or a wolf) (B) | 1b |
| C8, T7 | `talk.remember`, `talk.forget` (realm, town, resident and **pair**), `belief.plant` with `story.rumour` and `story.cleared`; U unless `keep` | 1b |
| N2 | window facts `player.quest.<story>/<quest>` and `player.story.<var>` | 1b |
| C1 | operands in effect parameters; `{name}` fills in texts; goals from operands; a tracker "of" | 2 |
| M2 | operand `{"purse": "<id>"}`: what a purse holds now | 2 |
| C12b | `match` and `field` with `starts` and `in` (tells deeds apart) | 2 |
| N3 | a player tracker that mirrors a world variable ("Your town's howl") | 2 |
| J4 | `calendar is moon`, watched event `moon`, and R2's moon anchor | 2 |
| SQ1, SQ5, L3, M5, N4 | objectives gain `where` and `credit`; `camp cleared`, `caravan arrives`, `ship docks`, `ship lost`, `project built`, `festival` become mission events; `cell` on `trade done`; quest `join.again` (a wolf's wait after any end) | 2 |
| L2 | named story camps (`camp: <name>` on spawns), with the program's `prefix` | 2 |
| C5 | `npc.greet`: a resident's words for one wolf, in their voice, until seen | 3 |
| C4 | `title.give` / `title.take`, event `title given`, fact `player.title` | 3 |
| C3 | action `rank` (top N into a list); quest `credit: share`; operand `share` | 3 |
| J11, J8 | operand `tally` (no 500 cap); `belief.forget` | 3 |
| V2, T5 | a story words check at approval (symbols, the giver's dialect, forbidden words, face banners); `npc.line` in the speaker's voice | 3 |
| C12a | `story.notify`, event `story notified`: a pack credits a wolf in the running chapter | 4 |
| C13a, C13b | events `variable changed`, `month begins`, `succession`; condition `resident has post` | 4 |
| C6 | quest `requires` (gated per wolf, no giver leak); `offer.during` (the giver speaks mid-quest) | 4 |
| N1 | story choices that don't fit a hub overflow into "Ask about…" | 4 |
| **J3** | quest `closure: "line" \| "side"`: counts toward the Quickening's `closedStories`, at most 4 a program, the game refusing a fifth per wolf per program (answer (b)) | 4 |
| J10, L5 | `wolf has` (a wolf's Gift); `quest.withdraw` | 4 |
| J5 | events `howled` and `chorus closed`; effect `npc.howl` (a resident howls) | 5 |
| J6, L1 | `count in region` counts residents; `hunt.stage` (the bear, the boar) | 5 |
| J9 | tagged story items (the list, the letters, the writ) (U unless `keep`) | 6 |
| **C10** | `foe.add`, `foe.leave`, `foe.outlaw` (a resident downed, never killed); events `foe downed`, `foe defeated`, `foe fled`; `foes.json` (B, U) | 7 |
| **C10t** | tuning the foes: `foe_sim`, `world_check --foes`, up to 4 passes at ≤ 250 players against a written bar (answer (c)) | 7 |
| T3 | strangers in scenes: a cast entry `{id, visitor}` with the `like` face | 7 |
| J2 | `scope: "realm"` variables: one value per wolf, shared across campaigns | 8 |
| C14, L4 | `npc.post` (one atomic post change: the ruler at the end); `npc.sentence` (a hold that outlives the run) | 8 |
| C7, J7 | `cast.pick` and a giver from a variable; `npc.queue` (an unplaced resident first for a vacancy, campaign two) | 9 |
| C15, SQ3, SQ4, M3, T6 | mantles restored; lend a paw to a resident at work; `rewardLimit`; `house:` purses behind a switch (off); a `story` talk topic | 9 |

**Not needed**: C15 for this campaign (it uses `say[self.id=…]` lines, never `mantle.assign`), and C7 for the packs
(givers are fixed by the registry). Both are built anyway (answer (a)). M1 `wild.spawn` is dropped for L1; M4, M6 and T1,
T2, T4, T8 are merged into L2, SQ2, C8, N2, V2 and N1.

**Migrations**: 0080 (story memory, spares, greets, titles, and a `dm.story_posts` table for C14 and C7), 0081 (realm
variables, `dm.story_realm_vars`), 0082 (an index for closure receipts, if needed). R1–R3 need no migration.

### 7.1 Readiness in one picture (R1–R3)

```json
"ready": {
  "all": [
    {"any": [
       {"participation": {"list": "ch3_did"}, "of": "active", "percent": 30, "floor": 3, "cap": 150,
        "label": "Wolves who did the chapter's tasks"},
       {"test": {"all": [{"test": "switch", "var": "ch3_auto"},
                         {"test": "days since", "of": "run", "op": ">=", "value": 36}]},
        "label": "or the soft maximum, with auto on"}]},
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

The console shows one bar per part: *"42 of 118 active wolves (36%); 30% and at least 3 needed"*, *"11.2 of 12 days in the
chapter (1.5 held days not counted)"*, with "active 118 · in the story 260 · online now 31" above. Readiness is latched
once met: a crowd arriving later can't take it back.

### 7.2 Foes, and how they are tuned (C10, C10t)

A foe is a hostile band with a named leader (**follower**, **tough**, **fierce**, **champion**). Attacking one is no
crime, and guards take the wolves' side. When the leader falls the band breaks. `foe.outlaw` makes a resident a foe for
one fight, downed and never killed.

**decided (user, 2026-11-22), answer (c).** `foe_sim` runs real fights on the real rules; `world_check --foes` checks
the load. A pass is accepted when every row's 95% interval lies inside its band, for example:

| Foe | Wolves | Band to pass |
|---|---|---|
| tough, band 0 | one seasoned, armed | wins 25–40% (a lone wolf should look for help) |
| fierce, band 4 | five seasoned, armed | wins 80–92% |
| champion, band 8 | eight seasoned, armed, + 2 guards | wins 70–85% (the Pass on the Moot night) |
| champion, band 0 | one Quickened veteran | wins ≤ 35% (no wolf solos the Grey Captain) |
| fierce, scaled band | 4, 8, 12, 16 wolves | each 60–85% (a crowd is not a walkover) |
| load | 250 wolves, 6 fierce camps | tick p99 at most 10% over the same run without foes |

Up to **4 passes**, one knob per strength each (skill, then hp, then flee); only `foes.json` changes. A row still out
after pass 4 goes to the user. Load runs go to 20, 100 and 250 players. Economy sims are not covered by answer (c).

### 7.3 Story doors open to wolves (as built)

**decided (coordinator's, for the user to overturn).** Before this, a story's `door.unlock` lifted a door's own lock, but
the town's rule for wolves (`World::playerDoorLocked`: a Ridgemere or community interior is locked to them unless it is
an inn or a merchant keeps shop there in hours) still kept them out, so a story could not open a shop at night or a hall
for a quest. Now:

- **A door a live story has unlocked or opened is open to wolves while the story holds it**, both sides, even where the
  town's rule would lock it. The story records it with the door (`held.doors.<door>.wolves`; `door.open` is now kept
  there too, with no lock, so its end restores none). `door.close` closes it to wolves again and leaves the door's own
  lock alone, so its residents still go in: that is how a story ends a window it opened. `door.lock` closes it to
  wolves too, but locks the residents out as well. The game rebuilds the set from the live stories on every watch
  refresh (a restart included: `Game::refreshStoryDoors` → `World::setStoryOpenDoors`), and the run's end lets it go.
  A door two live stories hold is open to wolves while either still holds it open.
- **Never a private home**: an interior some resident lives in and nobody works in, the interior being the room behind
  the door with every room its stairs, passages and inner doors reach without going out of doors (a shop with a flat
  above is a workplace; a lodging house's empty ground floor with homes up its stairs is a private home; a post's work
  place counts even when vacant). Approval refuses `door.unlock` or `door.open` of such a door
  (`story_runner.phase9_references`, "A story may not open a private home to wolves", from `dm.story_residents` and
  `live.profession_slots`), so §9.3 check 2 catches it; and the game never lets one through for wolves even so
  (`World::privateHome`, from the residents' homes and works and the posts). The effect itself still works on a home
  for residents (its own lock).
- On Ridgemere 3 today (read only) 243 of the 414 doors lead into a building somebody lives in. 160 of those are private
  homes (149 by the room behind the door alone, and 11 more into lodging houses whose homes are up the stairs); the
  other 83 are shops, workshops and inns with homes in them.
- Tests: `story_world_tests` (`storyDoorsForWolves`: a hall, a private home, a shop with a flat and a lodging house
  with a home up its stairs behind community doors; unlocked, opened, shut, locked, rebuilt, ended) and `test_story_effects` (the refusal, by every form of the
  door's id, in a trigger and in a phase's opening effects; lock and close allowed; a workplace allowed).

---

## 8. Build phases

Each phase is one agent's work in sequence. It reuses one build (`build-core`), runs targeted tests, regenerates the
guide reference (`story_guide.py --write`), rehearses one sample program that uses every new feature, and leaves the
tree green. The full suite, the end-to-end and one review-and-fix run once, after Phase 9. Phase 1a also runs its own
end-to-end, because Act I is paced by it. No phase touches `ratw_prod`; checks run on a scratch server or Ridgemere 3
read only.

| # | Phase | Items | Hours | What it unlocks |
|---|---|---|---|---|
| 0 | **Today-only content** | content only; T9 | — | **Act I**, every pack's today version, the stage pack and the talk library, paced by the direction-exit stand-in |
| 1a | **Readiness and participation** (first) | C2, R1, R2, R3 | **21** | every chapter paced by participation + time + Advance; Act I moves onto its `ready` version by `migrate` |
| 1b | **Foundations, purses and Act I's talk** | V1, SQ2, V3, J1, C11, C9, C8, T7, N2 | **26.5** | per-wolf offers; shop tills pay; old cast spared; figures by the story; **all campaign gossip and helpful knowledge plays**; pair memory; the small packs complete |
| 2 | **Values and objectives** | C1, M2, C12b, N3, J4, SQ1, SQ5, L3, L2, M5, N4 | **25** | numbers and names in boards; scaled goals; named camps and camp objectives; the moon; deeds told apart |
| 3 | **Thanks, titles, ranks and words** | C5, C4, C3, J11, J8, V2, T5 | **27** | **Act I complete as designed**: named thanks, titles, ranks without the 500 cap; story words checked at approval |
| 4 | **Stories that talk to each other** | C12a, C13a, C13b, C6, N1, J3, J10, L5 | **25** | packs credit wolves in the running chapter; chained quests without giver leaks; the giver speaks mid-quest; **the Quickening counts story closures** (answer (b)); the medium packs complete |
| 5 | **Howls, crowds and the wild** | J5, J6, L1 | **13** | Act II's howl quests; the Ch 8 haul; the bear and the boar |
| 6 | **Story items** | J9 | **10** | **Act II complete** (tagged lists and letters); the writ |
| 7 | **Foes and strangers** | C10, C10t, T3 | **25** | the ford, the Pass and the parley as designed, foes tuned to the bar; **the large packs complete** except L4 |
| 8 | **Across acts and the crown** | J2, C14, L4 | **16** | **Act III complete**: per-wolf state across acts; the ruler's post at the end |
| 9 | **Casting, mantles, queues, paws, House purses, the story topic** | C7, C15, J7, SQ3, SQ4, M3, T6 | **22** | givers cast in any town; the sequel's hooks; work-beside-a-resident quests |
| — | **End of slice** | full ctest, every Python suite, Editor and client tests and builds; an end-to-end on a scratch Ridgemere 3 server with a throwaway DB (two chapters and a pack: a pledge, a till payment, a notify, a title, a foe, **a chapter moved on by participation and by Advance**, a closure counted); one review-and-fix | **7** | the whole design, all engine items |
| | **Total** | | **217.5** | about 18 real days in sequence at 12 agent-hours a day |

**Order.** Phase 1a goes first and depends on nothing. Phase 1b depends on nothing and may run beside 1a. Phase 2's C1
is used by later operands; C4, C5, C8–C11, L4 and L5 work with literals until it lands. Phases 5, 6, 7 and 8 are
independent of each other and can be reordered. Phase 9 needs Phase 2.

**Acts against phases.** Act I needs no phase; it picks up Phases 1a–3 by `migrate` while it runs. Act II needs Phases
1a–6. Act III needs Phases 7–8. Phase 9 is for the packs and the sequel. If an act is ready before its engine has
landed, the interlude waits (the admin may hold it), or the act ships on its today-only rows (`campaign.md` §11.4). If
the engine runs behind, keep Phase 4 first, then 5, then 6.

---

## 9. The content build plan

### 9.1 Files

| Content | Programs | Where in `Data/` |
|---|---|---|
| Purse counters | `nh-purses` | `Data/Storylines/ridgemere3/ninth-howl/nh-purses.json` |
| The campaign | `ninth-howl.1` … `ninth-howl.9`; `ninth-howl.cleanup` | `Data/Storylines/ridgemere3/ninth-howl/` |
| Small packs | `nh-pack.small.city`, `.north`, `.south` | `Data/Storylines/ridgemere3/packs/` |
| Medium packs | `nh-pack.medium.city`, `.greenholt`, `.south`, `.north` | `Data/Storylines/ridgemere3/packs/` |
| Large lines | 11 programs, `lq-<line>` | `Data/Storylines/ridgemere3/quests-large/<id>.json` |
| Stage pack | `nh-pack.stage` | `Data/Storylines/ridgemere3/quests-stage/nh-pack.stage.json` |
| Talk | section 6.5 | `Data/Talk/…`, `tools/dialect_allow.json` |
| Foes | the tuned table | `Data/Storylines/foes.json` (Phase 7) |

Every program is version 2 (`Docs/story-programs.md`), with a top-level `prefix` (`nh`, `sq`, `mq`, `lq`, `st`) and one
rehearsal case per phase. None goes in `Data/Storylines/programs/`.

### 9.2 Waves

Each wave is written first in its **today-only** form, then given new versions as engine phases land. Hours are
**my estimate** (agent-hours, writing and checking).

| Wave | Content | Needs | Hours |
|---|---|---|---|
| A | `nh-purses`, `ninth-howl.cleanup`, the world-variable declarations, the T9 allow-list | nothing | 3 |
| B | Act I, Ch 1–3, today-only, with the direction-exit stand-in (and `ready` blocks if 1a has landed) | nothing | 24 |
| C | Small packs (48) | nothing; better with 1b | 15 |
| D | Medium packs (23) | nothing; complete with 4 | 18 |
| E | Large lines (11) | nothing; complete with 4, 5, 7 | 30 |
| F | Stage pack (22) | nothing | 8 |
| G | Talk library: about 110 tree nodes, gossip for 10 stages × 8 dialects, scenes, pack lines | nothing (inert until C8) | 24 |
| H | Act II, Ch 4–6 | Phases 1a–6, or today-only rows 4–6 | 24 |
| I | Act III, Ch 7–9 | Phases 7–8, or today-only rows 7–9 | 26 |
| J | New versions as phases land (`ready` blocks, J1 targets, C8 keys, C5 closes, L2 camps, J3 closures, C10 foes) | each phase | 12 |
| | **Total** | | **about 184** |

**Before wave G**, `cast-and-talk.md` must be brought to `decisions.md` §6.4: it still has about 33 lines that date the
Moot ("on the longest night", "before Midwinter", "a Moot every Midwinter", "Midwinter's coming"). Those become "the
Moot" or "the Moot night", with winter variants gated on `nh_moot=midwinter`.

### 9.3 The checks

Each program, before its approval note, runs and lists:

1. `story_program.review(p)` passes with nothing uncovered, and `story_program.problems(p) == []`;
2. `story_runner.references(conn, 'upper_accord', p)` passes on Ridgemere 3 (read only);
3. the placed-alive-reachable query of `decisions.md` §1 for every resident named, and the giver check over
   `givers.tsv`;
4. every purse looked up in the latest checkpoint's `society.accounts`;
5. by search: no `story.tell` with a resident's words, no `npc.speak`, no story wave matched on `camp_w_ridgemere_<cell>`,
   no forbidden word; by eye, every offer's symbols and dialect (until V2);
6. **readiness cases** per chapter: 1, 5 and 200 active wolves; Advance anyway before the minimum time; the soft maximum
   with `ch<n>_auto` off and on; `ch<n>_wait_bar` on; a winter and a non-winter Moot call;
7. a rehearsal case for every worse road and every ending, with `expectChapters` for the chains.

Each talk file runs:

- `python3 tools/dialect_check.py --world ridgemere3`;
- `build-core/talk_check --data "$PWD/Data/Talk" --validate` and `--render-trees` (the 9-choice rule, gate values
  spelled out, `|` alternation allowed, no "…");
- `python3 tools/talk_lint.py`;
- `talk_scorecard_quick` and `talk_golden` unchanged (the test town has no story memory).

The code tests are the engine phases' (`engine-spec.md` §7.2): `test_story_v2`, `test_story_native`, `test_story_quests`,
`test_story_campaigns`, `test_story_steering`, `test_story_effects`, `test_story_catalogue`, `test_story_guide`,
`workbench.test.ts`, the C++ `story_world_tests`, `dialogue_tests`, `storyline_tests`, `battle_tests` and the rest.

---

## 10. Going live on Ridgemere 3

Testing happens first on a **scratch server** (`bash tools/scratch.sh ridgemere3`, background, stopped after), which
writes nothing to the world. Approving and publishing on Ridgemere 3's live world is a step the user starts (task 52 of
this work: "Load into Ridgemere 3, verify live").

### 10.1 DM preparation (before Act I unless noted)

1. **Allow-list** the new place names and titles (T9).
2. **Declare the world variables** (`world-var {do:"declare"}`) of `campaign.md` §4: progress, resident lists, branch
   results, cross-act wolf lists, purse counters and switches.
3. **Protect and spare the cast.** Ch 1's setup does `npc.protect` and (with C11) `npc.spare`. Until C11, the DM sets
   the authored `live.npcs.protected` flag on the seven oldest cast (Bramble Bogditch 78 down to Oda and Piet Lumley
   at 60). **This is a world data edit**, listed as one, and cleared at the end. Desmond Hollis is never protected.
4. **Figures and talk keys**: until C9 and C8, the DM sets the figures with `save_figure` and seeds `nh_stage` and each
   chapter's keys by hand at its entry and Blaze, and each pack's "open" keys.
5. **The `warden_order` faction**: create it before Ch 5's end, with Gorm Plankgate a ranked member.
6. **Leave Dagfinn's post alone**, and do not post proj-1 to proj-3 again (they stand open). Re-post any Line project
   that lapsed.
7. **Authoring lists from the data**, placed and alive: debtors, weep witnesses, letter carriers, list keepers,
   petitioners, the Mirebridge-blooded.

### 10.2 Approval and publication order

1. **`nh-purses`**, approved and published first, and left running across all acts.
2. **The packs and the stage pack.** They stand alone; the stage pack sits in `quiet` until `nh_stage` is set.
3. **`ninth-howl.cleanup`**, approved but not published: the undo if an act is stopped early.
4. **Act I** (Ch 1–3) as one `campaign.approve`, published from the Campaigns console; its chapters chain by themselves.
5. **Act II**, when Act I has closed into its interlude and the DM chooses; then **Act III** the same way.
6. **New versions** reach running chapters by "Move onto a new version…" (doc 85, 5b).

### 10.3 Running it

- Look at the Campaigns list **once a real day**; answer each "ready to push" with **Advance anyway…** or let it wait.
- Turn on `ch<n>_auto` or `ch<n>_wait_bar` where the defaults don't suit. With Ridgemere 3's 5 wolves the floor is what
  matters: 3 of them (or 2) must take part.
- **On a hosted world**, steering (Advance included) is the owner's, and a hosted DM's only with the "may steer
  campaigns" permission (doc 85, decided 2026-11-21).

---

## 11. Decisions taken, and open questions

### 11.1 The user's answers (2026-11-22)

| | Decision | Where it lands |
|---|---|---|
| (a) | Build everything: all ten engine phases and all content; nothing deferred | sections 7, 8, 9 |
| (a) | No 40-day limit. Progress by participation (a share of the active wolves doing named tasks) plus time, pushed forward by an admin | section 3.4; R1–R3, built first (Phase 1a) |
| (b) | Finished story quest lines count toward `closedStories`, capped: only a large line's last chapter (`closure: "line"`) or a campaign side story (`"side"`), at most 4 a program; the game refuses a fifth per wolf per program; 3 social XP each | J3, Phase 4 |
| (c) | Foe strengths tuned by combat simulations, several passes, at most 250 players | C10t, Phase 7 |

### 11.2 The coordinator's binding decisions (`decisions.md`)

| § | Decision |
|---|---|
| 1 | One giver registry and its rules (section 4.7); the Moot backing moves to Speaker Gerrit; new fallback Keepers for Ridgemere and Fenshore; new look-alikes |
| 2 | Duplicate premises resolved: the cough, the false light and the short-weighed sheaves each belong to one large line; the two salt wagons made different |
| 3 | One purse rule (section 4.6) |
| 4 | Story camps matched by `cell` plus a `spawned_<region>` switch; "break the camp" is a trigger until Phase 2 |
| 5 | One name for each engine item; every item the packs and talk plan found is catalogued |
| 6 | **Pacing by readiness** (section 3.4): the floor never above the active count; `slowNeedsYou`; `auto: true`; no wait for Midwinter |
| 7 | New quests for uncovered places; the 22-quest stage pack replaces the old seed lists |
| 8 | Scaling for unique-object quests and large fights |
| 9 | **A face banner is not the dialogue window**; the beaten Captain is a stranger held a game day; every critic item assigned |

**decided (mine)** in this doc:

1. **No target publication date** (`decisions.md` §6, which `engine-spec.md` §7 now follows). Act I ships when the DM
   publishes it, in any season.
2. **The medium pack's four `closure: true` marks (Q3, Q10, Q16, Q22) are dropped.** Under answer (b), only a large
   line's last chapter or a campaign side story counts, and J3 refuses a `line` closure that does not end a chain. Their
   journal notes inviting a social Story stay.
3. **Each chapter marks at most 4 of its side stories `closure: "side"`** (it has 3 to 5); its main line and door
   quest never carry it.
4. **The engine-spec recommendations D2, D3, D5, D6, D7 and D9 stand**: talk keys must carry the program's prefix;
   `title.give` is not "acts on wolves", with a client setting to hide titles; a `dm.story_posts` table for C14 and C7;
   a new `dm.story_realm_vars` table for J2; "Ask about something else…" for N1; `npc.sentence` needs a builder.

### 11.3 Open questions for the user

1. **Should a ready chapter move on by itself, or wait for the admin?** The user said progress "would be pushed forward
   by an admin". The coordinator chose `auto: true`: a ready chapter moves itself, the admin pushes the slow ones, and
   nothing passes its soft maximum without the admin. Waiting for Advance every time (`engine-spec.md` D10's first
   suggestion) is one field per chapter. **Recommendation: keep `auto: true`.**
2. **The social XP for a story closure** is 3 (adopted). It lowers the Quickening's bar, as the user chose; it lives in
   `Data/Social`.
3. **M3's economy check.** Turning on `house:` purses needs an economy simulation, which answer (c) does not cover. The
   Phase 9 agent will ask first.
4. **Act I before Phase 1a?** It can ship today on the stand-in, or wait about two real days and ship with readiness
   bars. **Recommendation: wait for Phase 1a.**
5. **The world data edit for old age** (the `protected` flag on seven residents) is needed only if Act I ships before
   C11 (Phase 1b).

## Phase 1a as built

**Phase 1a as built** (2026-11-22, branch `r3-nh-p1a`, on `ridgemere2` at `5b8aae40`). No migration (readiness lives in the
run's `state`; `dm.story_commands.kind` already takes `advance`); no new game order; no game (C++ server) change: the trigger
language's native twin (`libratw_story.so`) only. Engine spec items C2, R1, R2 and R3.
- **C2: the audience counted; `times`, `div`** (`tools/story_triggers.py`; `Core/RatwStoryTriggers.cpp`, `Dispatch2::value`).
  Values `{"audience": "all" | "active" | "online"}`, `{"times": [a, b]}`, `{"div": [a, b]}` (floored toward zero, the native twin's
  `trunc` of a double; by 0 no value, so nothing fires on it); both in `ARITH`, two deep. A program field `activeDays` (1 to 120 game
  days, 18 when not given; `story_program._check_header`, version 2 only; `effective_phase` carries it as `_activeDays`). **Seen**:
  `story_program.note_seen` keeps `state.seen` ({wolf: game day of their newest event}) for a run that counts its audience or has
  Readiness (`counts_audience`): the runner calls it for every event it reads, before dispatching it (`_tick_run`), and a rehearsal
  for every fact. **Seeded at launch** (`story_runner.seen_lately`, one query on the events' actor index, newest first, within
  `activeDays` of the launch day): a chapter started by `story.start` opens with its wolves already active. Wolves who left the cohort
  are pruned from it. **Online**: the story's wolves among the signed-in wolves of the world's facts when the event carries them (a
  program that counts `online` now watches the `wolves` facts: `watch_of`, `uses_online`), else the run's own `state.online` (a
  rehearsal keeps it from arrivals and departures), else unknown. `story_triggers.audience_of` makes `{all, active, online, activeIds,
  onlineIds}` once per firing (`Ctx.audience`); `story_native.fire_v2` sends the same as the request's `audience` when the rules count
  it (`_needs_of`: `uses_audience`), so both twins read identical counts and ids.
- **R1: who took part** (both twins: `counted`, `share_need`; C++ `counted`, `shareNeed`, `test("participation")`). Values
  `{"percent": src, "of"}` (floor of 100 × |S| / |D|; 0 when |D| = 0) and `{"among": src, "of"}` (|S|), `src` being `{"list": <a story
  or world list of wolves>}` or `{"where": <a condition tested for each wolf, the wolf as $each>}`; `of` `active` (the default), `all`
  or `online` (unknown when not known). The condition **`participation`** (catalogue entry with the new param type `condition`; an
  optional `source` the workbench keeps): need = max(min(floor, |D|), ⌈percent × |D| / 100⌉), then at most `cap`, never below 1
  (decisions §6.2: the floor never asks for more wolves than are active); false with |D| = 0; unknown with D unknown. **Checks**
  (`_check_source`, `_check_where`, `_check_condition_v2`): exactly one of list and where; a list must be a declared story or world
  `wolves` list; a `where` holds only `compare`, `switch`, `in list`, `days since`, `quest is`, `objective is` and groups, 3 deep and 8
  leaves, no event fields or loop values, a wolf's own variable read `of: "$each"`, a quest read for `$each` or the story; floor ≤
  cap; never inside For each (nor inside its own test), at most one level deep in arithmetic. `judge` shows the counts (among, of,
  need) for Explain. The native `member` check now looks the cohort up in a set (5,000 wolves × a test of 4 leaves: about 11 ms
  native, the request's 5,000 wolves' values included), and the Python twin reads personal variables against a cohort set while it
  counts (`var_store(members=)`): 130 ms became 13 ms.
- **R2: Readiness** (`tools/story_program.py`, "readiness"). A version 2 phase's `ready` block (`_check_ready`, `_check_ready_part`):
  `all` (1 to 8 parts; `{"any": [2 to 4]}` one deep; each with an optional 80-character `label`): `participation` (R1's fields as a
  part: `ready_condition`), `minDays` (0 to 3,650, `since` `phase` or `chapter`), `value` with `atLeast` and `test` (the story's and the
  world's values and state only: no facts, rolls, event fields or a wolf's own values), `missions` (this phase's), `quest` (a story-scope
  quest); `auto`, `then` (`day begins`, `hour`, `season begins`, `festival`, `dm signal`; the game is asked for its moment by the watch
  list), `slowAfterDays`, `slowNeedsYou`, `autoAfterDays` (only with `auto` off). Its exit `{"on": "ready", "to"}` (the catalogue's new
  special event `ready`, exits only; exactly one, and only with `ready`; `exit_for` skips it; `triggers.routes` includes it, so the
  graph check and "a `$actor` reward needs a participating wolf on every path" cover it). **`readiness(p, state, day, elapsed,
  facts)`** measures every part into `state.ready` (`{phase, visit, met, metAt, heldAtMet, pushed, parts, checkedAt, auto, to, then,
  waitingFor, slow, slowNeedsYou, days, heldDays}`), latches `metAt` for the visit, logs `ready`, `slow`, `pushed`, `advanced` and
  `taken` in `state.readyLog` (the last 20), and returns the move: `auto` at once, or (`then`) sets `waitingFor`, which
  `ready_anchor` takes in `dispatch` when that event comes; `autoAfterDays` after `metAt` (held days not counted) with the cause `ready
  (no admin)`. **Held time**: `Steer.do_hold` records `atDay` and the visit; `do_resume` adds the span to `state.heldDays` (`run` and
  `visit`; a new visit starts its own at 0); `minDays`, `slowAfterDays` and `autoAfterDays` count running days only (`running_days`,
  `held_days`). **The runner** (`_tick_run`) measures a phase with Readiness at most once a tick, when it read events, at least once a
  game hour, and afresh in a new visit or after a command; a move goes through `leave_visit` and `enter` with the cause `ready` (or
  `ready (no admin)`). **The command `advance`** (`COMMAND_KINDS`, `FLOW`, `RELEASING`, `VISITED`; payload `{when: now | then, anyway}`;
  `Steer.do_advance`): refused without a ready exit ("This phase has no Advance: use Go to phase…"), refused when not ready without
  `anyway` ("Not ready yet: 1 of 10 active wolves (10%; 35% and 4 needed); 0.4 of 2 days. Advance anyway?"), `now` takes the exit,
  `then` pushes it to wait for its moment; the result's before and after hold the readiness snapshot, and the audit's words say it
  ("Advanced to done now (ready): 3 of 6 active wolves (50%; 35% and 3 needed); 2.2 of 2 days. Reason: …"); `_may_release` applies.
  **Explain**: `listeners()` lists the ready exit with its parts (`readiness`), `can` only with `auto`, `then` or `autoAfterDays`
  (else `cannot: admin`); `_waiting` gains `{on: "ready", to, auto, then, met, waitingFor}`; `_stall` never calls a phase with a ready
  exit "stuck". **Alerts** (`_ready_alert`): `ready` (needs you, unless it goes by itself or was pushed) and `slow` (needs you with
  `slowNeedsYou`); the game's Dev Console words both (`_alert_words`). **Views**: `_row_view` and the run's detail carry `ready` (with
  `activeDays`, the slow and auto days, `thenWords`), the detail `readyLog`, `controls()` the phase's Advance, the timeline `ready`
  items (stamped with their real moment by the runner). **Moving onto a new version** (`migration`) keeps `metAt` (and a push) when the
  phase and its Readiness are unchanged (`ready_digest`), and measures afresh otherwise. **Rehearsal**: a case may give `audience` (the
  counts) and `activeWolves`; its events may hold the steps `{"check": "ready", "met": bool}` and `{"advance": "now" | "anyway" |
  "then"}` (`_check_step`, `_ready_step`); the rehearsal measures after every fact and every move (`measure`), the report has `checks`,
  `ready` and `readyMet`, and the review lists `uncoveredReady`.
- **R3: the console** (`Editor/src/lib/campaigns.ts`, `Editor/src/dm/campaigns/Readiness.tsx`). Types `Readiness`, `ReadyPart`,
  `ReadyLogEntry`; `RunRow.ready`, `RunDetail.readyLog`, `Controls.ready`, the `ready` waiting and timeline item, the `ready` and
  `slow` alerts (`alertText`: "Ready to advance: ch1_main → ch1_end since day 93 (all 4 parts met)", "Slow: 21% of the 35% needed
  after 31 days; Advance anyway, or wait", and with `slowNeedsYou` "Ready to push: …. Advance anyway, or wait."), `commandLine('advance',
  …)` ("Advance to ch1_end now (ready)", "… now anyway (not ready: 2 of 4 parts met)", "… at the next new moon"), `typedTitle` for
  `anyway`, `readyBars` (one bar per part: fill, the need's mark, "42 of 118 active wolves (36%); 35% and at least 5 needed", "11.2 of
  10 days in the chapter (1.5 held days not counted)", an `any` group's bars), `readyHeader`, `readySummary`, `chapterReadyText`,
  `waitingLines`. **Readiness.tsx** on the run's Overview under the phase strip: the header, "active 118 · in the story 260 · online
  now 31" and the active window, the bars, **Advance…** (primary when ready), **Advance anyway…** (with the typed title), **Advance at
  the next …** (with `then`), **⏸ Hold / ▶ Resume** beside them, "Holding stops the chapter's clock", each through the SteerBar's
  `AskDialog` with its dry run and the snapshot under "This will do". **The list**: a Ready column (a compact bar, or a green "ready"),
  "ready to advance" first among the alerts, the tab bar's count as before (it counts the `ready` alert); **the chapters bar** and each
  campaign card's running chapter: "Ch 1 · 36% of 35% · day 11 of 10 · ready" with **Advance…**. The timeline words `ready` items.
  **The workbench** (`workbench.ts`, `pickers.tsx`, `TriggerView.tsx`): the new values in words, the `participation` sentence ("at
  least 35% of the active wolves (at least 3; 150 always enough) are in the list did"), the ready exit's sentence, a new
  participation condition, renames and variable uses through shares, readiness steps kept in cases (`isStep`; the rehearsal editor
  shows them); the picker for a `condition` param (a test for each wolf, the world's conditions greyed out), the share and audience
  values in the operand picker, switching "who counts" between the list and a test.
- **Guide and format**: `Docs/Story-Maker-guide.md` §4 "Chapters that move on when the wolves have done enough" (the template and the
  recommended defaults), §6 "Goals that scale" and "Counting who took part", §11 "Moving a chapter on" (with the worked example), §15
  "It says ready but did not move" and "The share reads 0%", §16 the limits; the reference regenerated (`story_guide.py --write`,
  `--check` passes). `Docs/story-programs.md` gains "Readiness and participation". The catalogue: the condition, the `ready` event,
  `types.condition`, `trigger.exit.ready`, the operand text and `about`.
- **Sample** `story_v2_samples.READY` ("The muster at the ford", `v2-muster`, in `all_samples()`, so both twins run it under five
  seeds): every C2 and R1 form in triggers, Readiness with participation, `minDays` since the chapter, an `any` of missions and a
  test, `autoAfterDays`, a second phase `auto` with `then: day begins` and a `value` part; three cases (Advance; a quiet wolf and
  Advance anyway; no admin about, with `audience` and `activeWolves`). It rehearses clean: nothing uncovered.
- **Tests**: `test_story_v2` (Audience: each form, `div` by 0, the counts given, `activeDays` 3 against 18, the checks; Participation:
  12 active and 5 in the list 41% and 5; 40% with floor 6 false, floor 4 true; 2 active floor 3 true; a cap of 3 at 2,000; no active
  wolves 0% and false; online unknown; a test for each wolf; a quiet wolf leaves both counts; judge's counts), `test_story_native`
  (`ParticipationParity`: 42 forms and four conditions over eight states, both twins identical), `test_story_speed` (`CountingSpeed`:
  5,000 wolves under 20 ms native and 150 ms Python; readiness of two parts inside the tick's budget), `test_story_program`
  (`ReadinessShapes`), `test_story_problems` (`ParticipationProblems`), `test_story_v2_runner` (`Readiness`, DSN: seen at launch,
  4 of 10 not met on day 1, latched when 6 more come, the ready alert and no move, auto in the same tick, `then` waits for the day, 3
  held days not counted, slow, `autoAfterDays`, `migrate` keeps `metAt`), `test_story_steering` (`Advancing`), `test_story_explain`
  (`ExplainReadiness`), `test_story_campaigns` (`ReadyChapters`: chapter 1 advanced, chapter 2 opened with its wolves active, moving on
  by itself at the next day), `test_story_console` (`ReadinessInTheConsole`; the shared fixture `ready_program`/`ReadyFixture`
  lives there); Editor `campaigns.test.ts` (bars, header, summary, chapters line, alerts, advances, typed title, needs you, waiting),
  `workbench.test.ts` (the new sentences and values; 111 programs) and the render smoke (Campaigns (ready), Campaigns (ready list)).
  Updated counts: `test_story_catalogue`, `test_story_drafts`. Run: every `tools/test_story_*.py` but the three live ones (with
  `RATW_STORY_TEST_DSN` on a throwaway migrated database, so `test_story_program`'s 17 runtime tests ran), all OK; the Editor's tests
  (139 and 78), type check, build and render smoke (61 panels; the workbench over 108, 12 and 2 programs, none edited).
- **End to end** (a throwaway migrated database, dropped after; the real story runner process; the DM tool's route; the game stood in
  for by answering `dm.actions`, since a scratch server never applies orders): a two-chapter campaign of six wolves approved together
  and published through the route; seen at launch 6 active; two gifts: 2 of 6 (33%), 3 needed; three: participation met, 0.4 of 2
  days; day 3.2: ready, the `ready` alert (needs you; the tab count 1, "is ready to advance from work to done: press Advance"), the phase
  unmoved; the dry run of Advance; Advance with a reason, done; the command row's snapshot and the audit row ("Advanced to done now
  (ready): 3 of 6 active wolves (50%; 35% and 3 needed); 2.2 of 2 days. Reason: …"); chapter 1 complete, chapter 2 running with its 6
  wolves seen at launch; ready at day 5.5 and waiting for `day begins`; moved on by itself when it came; the timeline's `ready` and
  `advanced` items. A scratch Ridgemere 3 server built from this branch started (port printed, page served) and was stopped; it is
  read only, so the run itself was not on it.
- **Left or changed, and why**: `state.opened` is the run's existing `began` (the same moment). R1's memoisation within one dispatch
  is not built: no form is slow enough to need it after the cohort set (and memoising across actions that change a list would differ
  from evaluating afresh). A paused run is not measured (the runner reads only running runs); the console shows its last measure.
  "No longer ready" is never logged: under the latch a phase is ready from its first met moment. `online` is not read from the latest
  `story.cohort` answer (the game does not say who is signed in there); it is the world's facts' signed-in wolves, or unknown. A
  carried story list (`carry: true`) can make the next chapter's participation part met at its opening, as the end to end showed (its
  `did` list came with it): a chapter that wants its own count names its list differently. `RUNNER_VERSION` is `doc86-phase1a`.
- **After the review** (2026-11-22): the launch seeding query names `actor<>''`, so it uses the partial index `events_actor`
  (without it Postgres walked the whole log backward once for each wolf never seen: 0.33 s for 20 such wolves on Ridgemere 3's
  387K events, 0.003 s with it). `note_seen` runs again after an event is dispatched (runner and rehearsal), so a wolf who joins an
  open story by arriving is seen, and online in a rehearsal, from that arrival. Moving onto a new version that keeps the moment it
  was met keeps the whole readiness record (parts and `met`), `_ready_view` gives `parts: []` to a record without them, and the
  console reads `parts ?? []` (the list, the header and Readiness.tsx no longer throw while a move waits for the game). Moving a run
  onto a version that counts its audience seeds `seen` from the events' log as a launch does (`Steer.do_migrate`), so Act I moved
  onto the ready version has its wolves active at once. The workbench finds a variable read by a Readiness part (participation
  lists and tests, values, tests, inside `any`) as a use ("its Readiness reads it"), finds missions and quests named there, and
  renames them there (and inside a share's or a participation's `where`). Advance sends `to` (the line reads "Advance to ch1_end
  now (ready)"), and `do_advance` refuses a `to` that is no longer the phase's ready exit (a press from a page out of date). Each
  readiness log entry has its own number (`n`, from `state.readyLogN`): the timeline's key `r:{n}` no longer shifts as the log is
  trimmed, and an older page (`before.ready`) leaves out the entries a newer one showed. `times` past 2^53 is the double's product
  in the Python twin too, so both twins agree. Not changed: a paused run is still not measured, `online` has no `story.cohort`
  fallback, and R1's memoisation is not built (as above). Tests: `test_story_v2_runner.Readiness` (a joiner by arrival is seen; a
  run moved onto a ready version is seeded, 4 of 10 met; the kept record before the move settles has its parts; the timeline's key
  `r:1` and an older page without it), `test_story_steering.Advancing` (a `to` out of date is refused), `test_story_native`
  (`times` past 2^53), `workbench.test.ts` (uses and renames through Readiness), `campaigns.test.ts` (a record with no parts), the
  render smoke (Campaigns (ready, moving)).
- **Going live** on `ratw_ridgemere3` needs no migration of its own (doc 85's 0071-0079 still apply first, as there); the story runner
  restarted from this branch with the rebuilt `libratw_story.so`; the Editor released from it. Act I's chapters can move onto a
  version with `ready` blocks by `migrate` mid-chapter.

## Phase 1b as built

**Phase 1b as built** (2026-11-22, branch `r3-nh-p1b`, on `r3-nh-p1a` at `db0fdae9`). Engine spec items V1 (with the alive
check), SQ2, V3, J1, C11 (with `npc.protect`'s `lasts`), C9, C8 (with the pair owner), T7 and N2 (with `player.story`). **No
migration**: the stories' talk memory and holds on residents live in the game's checkpoint (`people.storyMemory`,
`people.storyHolds`, beside the protected marks), figures in `live.figures` (0050), the program's id on the storyline's
template field, so 0080 is left for Phase 3's greets and titles and Phase 8's `dm.story_posts`. Seven new effects (78 in the
catalogue), a new action group `talk`, three new parameter types (`talkkey`, `talkvalue`, `figure`).
- **SQ2: tills that pay** (`tools/story_quests.py` `check_purse`, `PURSE_WORDS`; `Core/RatwGameStoryFacts.cpp`
  `Game::storyPurseAccount`). `till:job:<founder>` (three parts, the second `job`) is a purse; `till:<founder>` maps to
  `till:job:<founder>` when it is no account itself, for `character.grant`, `purse crossed` and later M2. Both refusal messages
  say "town:<town>, till:job:<resident> or chapter:<id>". **Approval** (`story_runner.world_checks`, run by `references()` and
  by `reference_problems()` for the Problems panel) refuses a purse (rewards, grants, `purse crossed` filters) whose account
  (`purse_account`: a town's `stores:<town>`, the capital's `treasury` (the town the world's spawn is in), every town's
  `treasury` in a world of one settlement (no `stores:` account at all), a till by either spelling, a Chapter's by its name)
  is not in the latest checkpoint's `society.accounts`, as `Game::storyPurseAccount` maps it (a region that is no town, in a
  world of towns, pays from no account: "\"<region>\" is no town with a store of its own"); `town:<town>:works|watch|church` is refused with "this world has no works,
  watch or church purse"; a checkpoint with no accounts saved yet only warns. The catalogue's `purse` type, `character.grant`
  and `purse crossed` help drop the works, watch and church. The workbench's purse picker lists the towns' treasuries and the
  world's tills (`refs.tills`, from the checkpoint, named "<founder>'s till"); `PURSE` takes `till:job:`.
- **V1: approval checks what it missed** (`world_checks`). A quest's objectives: a `conversation`/`gift`/`gift again` target
  is a resident or a wolf, `near resident` a resident, `enter place` a place, `camp cleared`/`bandit falls` a `camp_<cell>` of a
  cell of the world (or `camp_story_<name>`, for L2); an objective's `item` is in the item catalogue. A quest's `board` is one
  of the world's notice boards (the checkpoint's markets; a warning when none is saved). **Residents named** as `npc`,
  `giver`, `from`, `like`, `subject`, any `resident` parameter, a scene's cast or an objective's target must have a home or a
  work place (`dm.story_residents`) and be alive (`live.npc_state.alive`): "<id> has no place in the world yet", "<id> has
  died". A `like` only warns, so does `npc.kill` of the dead and `npc.revive` of the living (whose `npc` must be the dead); a `death` filter's `actor` and words may name the dead.
  `references()` now returns its warnings, and `approve`, `publish` and `campaign.approve` return them as `warnings`
  (shown under the approval in the Story Creator and the New campaign dialog); `reference_problems()` lists every check with `blocks`.
- **V3: the guide.** §7's fail rule filters `death` on `actor`, with the sentence, and a note that the examples use the test
  town's ids (§2's reward words name the till, not the works).
- **J1: one choice for several wolves** (`story_quests.Run.story_effect`, `choices`, `_offer_players`). An `npc.offer` with a
  `player` is kept as `players` (a set of up to 5,000); the same id offered to another wolf adds them, the words and reply
  the latest, who picked it kept; offered to everyone it replaces as before. An older state's `player` reads as a list of one.
  The game already read `players`. The catalogue's help says so; the guide §7 "A choice for one wolf".
- **N2: what the window knows of the wolf's own part.** The runner sends the program's id with `story.phase` (also a revise)
  and `story.quests`; the game keeps it as the storyline's `templateId` (saved as ever). `Game::windowFacts` sets
  `player.quest.<program>/<quest>` to each quest view's status and `player.story.<name>` from the view's new `talk` map, live
  storykeeper stories in the order they began (the newest last, so its value wins). A version 2 variable may say `"talk":
  true` (a `player` number, switch or text; `realm` accepted for J2; at most 8; a text's first value `[a-z0-9_]{1,40}`):
  `story_quests.talk_map` puts each wolf's stored value in their journal entry as `talk` (digits, `yes`/`no`, the text), a text
  no line could test is kept, not told, and noted once in the timeline ("Not told to residents"). `facts.json` "player" gains
  the patterns `quest.*` (its seven states) and `story.*`; `talk::Library` reads a key by its pattern
  (`player.quest.ninth-howl.1/where_king`) and never aliases a pattern. Approval warns when another live story tells residents
  of a variable of the same name. The workbench's variable gets "told to residents (player.story.<name>)".
- **C11: spared and protected for a while** (`Core/RatwGameStoryWorld.cpp` `applyStoryHold`, `dropStoryHolds`,
  `sparedChanged`; `RatwGameTroubles.cpp` `isProtected`; `RatwWorld.cpp` `tendOldAge`). `npc.spare {npc, lasts: run |
  campaign}` and `npc.unspare {npc}`; `npc.protect` gains `lasts` (`ever`, the default, the Dungeon Master's mark as it always
  was; `run`; `campaign`). The runner's `order_of` sends the run, its visit and its campaign (`HOLD_ORDERS`); the game keeps
  `storyHolds_[spare|protect][npc]` = owners (`run:<run>`, `campaign:<id>`), refuses a hold whose run is not a live story or a
  campaign hold without a campaign, and a protect hold on a resident a Dungeon Master unprotected; the world skips a spared
  resident in old age (`World::setStorySpared`; the world already had a `spared_`, the bandits'); a resident with a protect
  owner is protected (after the Dungeon Master's unprotect mark). A story's `npc.unprotect` drops its own protect owners, and only those when it had one (the resident's own protection,
  a House head's or Atlas's, and the marks stay; "The story's protection is let go."); with none of its own it unmarks as
  ever.
  `endStoryWorld` drops the run's owners (before its early return, so a story that held nothing else lets go too); the
  runner sends `story.campaign_end {campaign}` once, when `run_ended` marks the campaign ended (`status<>'ended' RETURNING`), and
  the game drops that campaign's owners. Saved with the checkpoint (`troublesSave`, `storyTalkSave`). `lasts: campaign` is
  refused by `references(chapters=False)` (a warning in the Problems panel, which cannot know). The Campaigns list's campaign
  card has **Let go of the campaign's holds…** when no chapter runs (op `campaign-release`, the owner or "may steer
  campaigns": `release_campaign` queues `story.campaign_end`).
- **C9: figures of the realm** (`story_runner.write_figure`, `restore_figure`, `order_of`; `dungeon_master.save_figure` now
  calls `write_figure`). `figure.set {figure, role, npc | player | name, title?, known: realm | region | house, region?, house?,
  about?}` and `figure.clear {figure}`, both `needsBuilder`: written to `live.figures` in the runner, as `faction.relation`
  is; a resident's name from `dm.story_residents` (`npc_id` when they are in `live.npcs`), a wolf's from `game.characters` at
  that moment; the row it replaced kept in the run's history (`entry.figure.previous`), then `voice.sync` with the figure's id.
  The checks: exactly one of npc, player, name; `region` exactly with `known: region`, `house` with `house`; the role one of
  `facts.json`'s `figure_<role>`; approval checks the town and the faction. The timeline's history row has **Undo this
  figure…** (op `figure-undo {history}`, a builder on a hosted world): the old row put back, or the figure taken away.
  `_may_release` and the hosted approval's words name the realm's figures.
- **C8: talk memory** (`applyStoryTalk`, `storyRecall`; `Game::talkRecall`). `talk.remember {scope, town?, npc?, player?, key,
  value, days, keep?}` and `talk.forget` (game orders, the run as target): owners `realm`, `town:<region>`,
  `person:<npc>`, `pair:<min>|<max>` (talk's own pair key), in `Game::storyMemory_`, apart from talk's own memory. Caps: 64 an
  owner and 8,192 in all; a pair 16 and 20,000 pair entries; the soonest to be forgotten goes first, noted once a game day
  (`RATW_STORY_MEMORY`). **Recall**: `talkRecall` first recalls the story's for the same owner (and the realm's before a town's),
  so talk's own overrides; a query with no `here.region` still gets the realm's. So `world.mem.<k>`, `self.mem.<k>` and
  `pair.mem.<k>` lines play in the window and in ambient talk (a pair of two residents never matches a story's pair).
  **Checks** (`validate_effect`, `_check_action_v2`, `_check_entry_effect`): the owner's parts exactly; a pair's `player` is
  `$actor` in a participant trigger or `$each` in For each (a named wolf, a phase's opening or a world trigger's `$actor` are
  refused: "Pair memory needs a wolf of the story"); keys `[a-z][a-z0-9_]{0,39}`, values `[a-z0-9_]{1,40}`; approval checks the
  town. **Undo**: a run's memories without `keep` are forgotten at its end; `held.memory` lists what it wrote. Saved with the
  checkpoint (`storyMemory`). The run's **Variables & timers** tab has **What residents remember** (`status`'s `memory`, read
  from the checkpoint's `people.storyMemory`, for a program that remembers).
- **C8 and T7: rumours** (`belief.plant`; `Data/Talk/claims.json` kinds `story.rumour` and `story.cleared`). `{subject,
  claim, town, count 1-50, confidence 10-90, claimKind?}`, the subject as target: residents of the town (alive, with a home,
  not the subject) ordered by `fnv(run|subject|claim|id)` believe it (`World::believe` with the kind set, source
  `story:<run>`); planting again strengthens the same beliefs. The clearing is believed as **"never <claim>"** with the wording
  "#x.name# #claim.text#", so it never merges with the rumour it answers (a holder's beliefs are kept one per subject and
  words). The claim is a phrase that follows a name: a small letter first, 120 characters, no `#`, brackets or ids, by one rule in both (`story_program.claim_has_id`, the game's `idInWords`): a word (letters,
  digits, `_` and `-`) with an `_`, one that mixes letters and digits ("rm111", but "2am" and "3rd-rate" too) or one that
  starts `wolf-`.
- **Workbench and console** (`Editor/`): pickers for `talkkey`, `talkvalue` and `figure` (the keys, values and figures the
  program already uses: `effectValuesOf`), the purse picker's tills, the variable's "told to residents", the Timeline's Undo
  this figure, the campaign card's Let go of the campaign's holds, What residents remember, `memoryOwnerText`,
  `figureUndoText`. The render smoke's refs have a till.
- **Guide and format**: `Docs/Story-Maker-guide.md` §4 "Campaign-long holds", §6 "Told to residents", §7 "A choice for one
  wolf", "Its state in the window", "Purses that pay", §8 "Talk and rumour from a story" (the memory table, the recall order,
  writing the lines, rumours and clearings, figures), §13 (figures need a builder; undo; letting go), §14 (holds, memory,
  rumours and figures), §15 (a memory line that never plays; a refused purse), §16 (the limits); the reference regenerated
  (`story_guide.py --write`; `--check` passes; examples for the five new effects that need agreeing values).
  `Docs/story-programs.md` gains "Talk, rumours, figures, holds and purses".
- **Sample** `story_v2_samples.TALK` ("The pledge at the stone", `v2-pledge`, in `all_samples()`, so both twins run it under
  five seeds): two phases; a choice offered to each wolf who comes (J1), Ivo spared and Wren protected for the campaign, a
  villain named, the realm's memory, a pair memory of each wolf who pledges and a value told to residents, three pennies from
  Ivo's till, then a rumour, a clearing and the town's memory; two cases. It rehearses clean: nothing uncovered.
- **Tests**: `Tests/story_world_tests.cpp` `phase1b` (a till by both spellings, and one nobody has; spared at 120 lives, the
  unspared dies; two stories spare one, one lets go; protect for the run and Unprotect; realm, town, resident and pair recall
  and order, Bo not seeing Ash's pair memory; the caps; forget; a rumour's three holders, the same three again, a clearing in
  its own words, both wordings through claims.json, an id refused; all through a save; the run's end lets go and forgets but
  for `keep`; the campaign's hold outlasts the chapter until `story.campaign_end`), `Tests/dialogue_tests.cpp`
  `storyTalkInTheWindow` (with the test's own lines in a copy of Data/Talk: a choice for two wolves, one picked; a
  `world.mem`, a `pair.mem` (Ash's, not Bo's), a `player.quest`, a `player.story` and a `self.mem` line played in the window;
  `Town` takes a talk folder), `tools/test_story_talk.py` (the catalogue, purses and their accounts, the checks, the orders,
  the sample; with a database: purses, residents placed and alive, quests' boards and objectives, the new effects' towns and a
  campaign's hold, a figure written and put back), `test_story_quests.Phase1b` (J1, an older state, the `talk` map and its
  checks), `test_story_native` (`test_the_talk_effects_match`, with `$each` pair memory), `test_story_campaigns`
  (`story.campaign_end` once; an act stopped early let go), `workbench.test.ts` and `campaigns.test.ts` (the sentences, the
  pickers' values, the purse names; who remembers; undo). Updated: the catalogue counts (78), the builder effects, the order
  table, the guide's examples, the v2 sample count (112), the fixtures' residents placed (`fixture-homes`: V1 refuses an
  unplaced one).
- **Run**: ctest `-R "talk_|dialect|mantle_|story|storyline|troubles|dialogue|checkpoint|save_compat"` (38, all passed but
  `save_compat_pg` skipped without a database) and the game, world and project behaviours, every `tools/test_story_*.py` but the three live ones, `test_dungeon_master`,
  `test_talk_*`, `test_dialect_check`, `talk_lint` (unchanged baseline), the Editor's tests (143 and 78), type check, build and
  render smoke (62 panels; the workbench over 108, 13 and 2 programs). A scratch Greyfen server built from this branch started
  (port printed, page served) and was stopped. **Not run**: every test that needs the local PostgreSQL (`RATW_STORY_TEST_DSN`
  and the throwaway databases) skipped here, because this worktree has no database settings and reading the main checkout's
  was not allowed; so `references()` against a migrated database, the DB parts of `test_story_talk`, `test_story_campaigns`,
  `test_story_effects` and the console tests, and a scratch Ridgemere 3 server, are still to run before the merge.
- **Left or changed, and why**: the spec's `belief.plant` parameter `kind` is **`claimKind`** (an effect's `kind` is its own
  name). `story.rumour` and `story.cleared` have the family `other` (an existing `claim_kind`, so the old replies answer them),
  not a new `rumour`. The clearing is believed as "never <claim>" (above) rather than worded with `never`. `belief.forget`'s
  `story.cleared` value and V2's "never"/"did not" warning wait for Phase 3 (J8, V2). Talk memory and the holds are saved
  in the checkpoint's people section, not in the session hand-off's (`c->server`) or `carried` memory copies. The 400-day test
  of a spared ninety-year-old is a 120-year-old for three days (a year's chance of one: the unspared one dies the first day,
  every time). `figure.set`'s sentence names the figure's id and what is said about them (the catalogue asks every parameter
  to be a slot). A figure written by the Dungeon Master keeps ids of 1 to 40 characters; a story's are 3 to 40. A pair's
  `player` as a wolf variable or `{"field": "actor"}` waits for C1 (operands in effect parameters, Phase 2). Names of
  residents in a claim are V2's (Phase 3). `RUNNER_VERSION` is `doc86-phase1b`.
- **After review** (the review of `d9879d9c`): **B1** `game.checkpoints.payload` is `text`: `checkpoint_accounts`, `refs()`'s
  tills and `run_memory` read `payload::jsonb` (approval of a grant or a `purse crossed`, the workbench's pickers and a
  remembering run's status had failed on SQL). **B2** the order is **`story.campaign_end`** (`dm.actions`' kind is
  `^[a-z]+\.[a-z_]+$`, one dot: `story.campaign.end` was refused, so a campaign's end and "Let go of the campaign's holds…"
  failed). **B3** a new version of a running chapter (`approve(for_run=…)`) is checked as a chapter, so its holds for the
  campaign stand. **B4** `npc.revive` names the dead (the living warn). **B5** the database tests of `test_story_talk` run
  (`fact`, the text payload, `bandit falls` for the camp). **B6** one rule for a rumour's ids in Python and the game (above).
  **G1** a town purse maps as the game's (above). **G2** a story's Unprotect lets go of its own hold only (above). **G3**
  approval returns its warnings (above). **G4** `test_story_talk.InTheWorld` gains the pickers and memory on the text
  checkpoint, a revive of the dead, the capital's and a non-town's purse, the Problems panel's dead giver, missing till and
  works purse, the TALK sample clean in the world (no warning), approval's warnings, and the figure written and put back as
  `ratw_dm`; `test_story_campaigns` a new version of a running chapter that spares for the campaign; `story_world_tests` the
  DM's protection kept through a story's Unprotect and the claims "2am", "3rd-rate", "40s" refused. Nits: `till:job` alone is
  no purse, `ALIVE_WARNED` is `npc.kill` only, `storyTalkLoad` makes no empty owner and caps a pair's at 16 on load. Left:
  `figure-undo`'s and `campaign-release`'s audit and reason, and `restore_figure` over a later change (nits). **Run** (with
  the local PostgreSQL, throwaway `ratw_test_*` databases and `RATW_STORY_TEST_DSN` on one, dropped after): every
  `tools/test_story_*.py` but the three live ones, `test_dungeon_master`, `test_talk_*`, `test_dialect_check`, all passed with
  nothing skipped; ctest `-R "talk_|dialect|mantle_|story|storyline|troubles|dialogue|checkpoint|save_compat|game_|world_|project"`
  (42) passed but `talk_scorecard_quick`'s latency gate (B1L) under a parallel run, which passes alone; the Editor's tests
  (143 and 78), type check, build and render smoke. A scratch Ridgemere 3 server from this branch loaded the world and its
  save, served its page and was stopped; read only, the world's checks on Ridgemere 3 map `town:ridgemere`, `coldwater` and
  `larkfield` and refuse `town:plain`.
- **Going live** on `ratw_ridgemere3` needs no migration of its own; the game server restarted from this branch (it reads
  the new checkpoint fields, absent at first), the story runner from this branch, the Editor released from it. Act I's
  chapters can move onto a version that uses these by `migrate`; a version that holds residents for the campaign is approved
  as a chapter (with its campaign, or as a new version of a running chapter's run).

## Phase 2 as built

**Phase 2 as built** (2026-11-23, branch `r3-nh-p2`, on `r3-nh-p1b` at `f127dc89`). Engine spec items C1 (a–d), M2, C12b, N3,
J4 (with R2's moon anchor), SQ1, SQ5, L3, L2 (with the program's `prefix`), M5 and N4. **No migration** (the spec names none
for this phase: goals live in the copies, mirrors are never stored, camps in the roads' save). No new effect; one new event
(`moon`, 206 events, 69 mission events, 33 watched); a new parameter type `camp`; every effect parameter of type `int`,
`wolf`, `resident`, `place`, `item`, `faction` or `town` marked `"operand": true` (not `npc.line`, not `story.start`), and five
text parameters marked `"fills": true`.
- **C1a: values in effect parameters** (`story_triggers.resolve_effect`, `check_operand_param`; `Dispatch2::resolve` in the
  native twin, which is sent the catalogue's specs of the effects its rules use as `effectSpecs`: `story_native._needs_of`).
  In `_act` (both twins), after `$actor` and `$each`, each operand parameter given a dict is worked out with `value_of`: a
  number floored and kept to the parameter's `min`..`max` (a `nonzero` 0 sends nothing), an id a non-empty text; a missing
  value sends nothing and the firing's notes say "skipped: <param> has no value". The check (`validate_effect`'s new `operand`
  callback): a number value for `int`; for an id, a variable of its kind or a text (`OPERAND_VARS`; a mirror never), the
  event's field that holds one (`OPERAND_FIELDS`: a wolf's `actor`/`target`, a resident's `target`/`item`, a place's
  `cell`/`target`...), the loop's value of its kind; "a wolf variable in a resident param" is refused ("“finder” is a wolf: the
  resident needs a resident"). Phase entry effects stay literal. **Quest rewards** work theirs out when given
  (`Run.reward`, `words_ctx`: the credited wolf as `$actor`, the fact being read as the event; no field, no loop); a reward
  with no value is left out and noted in the state's `rewardNotes` (the last 20). `references()` and the world checks skip a
  dict value (and a tile or item made of one), so the game refuses an unknown id at run time. A pair's `player` (C8) may now
  be a wolf value too.
- **C1b: fills** (`story_triggers.FILL`, `fills_in`, `fill_text`, `check_fills`; `Dispatch2::fillText`, `fillOne`). `{name}` and
  `{name.of}` in `story.tell.text`, `board.post.text`, `journal.note.text`, `npc.speak.text`, `deed.award.detail`, a quest's
  `summary` and objective `label`, and an offer's `say`/`thanks`; `{{`/`}}` braces, a lone brace refused. A number fills as its
  digits (truncated, as both twins agree), a switch as `yes`/`no`, a text as it is, a `wolf`/`resident`/`place`/`item`/`faction`
  as a token `{@w:<id>}`… (`NAME_TOKENS`); `.of` reads the acting wolf, or the looped wolf inside For each over wolves (the
  loop's kind is carried as `Ctx.each_wolf` / `eachWolf`). Checks: declared, not a list or a timer; a wolf's own only with
  `.of` and the story's and the world's only without; an offer's words take no `.of`; widest length (number 11, switch 3, text
  200, wolf 24, other names 40, a mirror 11). **The game** (`Game::fillStoryNames(text, limit)`, `RatwGameStorylines.cpp`)
  turns the tokens into names (a wolf's or a resident's name, "someone" for a wolf not known; a place's, an item's
  `Society::itemName`, a faction's) and cuts at the limit on a word: in `applyStoryTell`, `journal.note`, `board.post`, a
  story scene's `npc.speak`, a deed's words, and the quest views (summary, objective labels) and offers (`say`, `thanks`,
  `summary`) of `story.quests` (`fillQuestWords`). The quest words are filled per wolf in `story_quests.view`/`views` and the
  offers once (`offers`), only for a program whose words or goals read values (`_needs_ctx`); the journal's fingerprint then
  follows the counters.
- **C1c: a goal from a value** (`story_quests.goal_of`, `read_goal`, `Run.read_goals`, `live_goals`, `keep_up`). An objective's
  `goal` may be a number value (no field, no loop), read when a copy starts into `objectives.<id>.goal` (1 to 1,000,000; not
  known: unreachable until it is); `goalLive` (a story-scope quest only) reads it again as the copy is credited, for
  `objective.progress`, and after every firing (`story_program.dispatch` calls `keep_up` after the story's effects), never
  below `n`, completing at once when `n` reaches it. Every reader of a goal (`track`, `objective_done`, the views, the board,
  Readiness's quest part, the console's objective commands) goes through `goal_of`.
- **C1d: a tracker of another** (`_check_show_of`; `trackers` adds `goal`). The client prints "label: a / b" over a thin bar
  (`Client/src/game/quests.ts` `trackerRows`; `archive.ts`).
- **M2: the purse** (`{"purse": <purse>}`: `operand_type` with `check_purse`, `value_of`, `Dispatch2::value`). Read from the
  event's facts' `purses`; the program's watch gets the facts section `purses` and the list `purseFacts` (`watch_of`,
  `purses_in`), which `refreshStoryWatch` keeps and `storyFactsJson` reports as `{<purse>: cash}` through
  `storyPurseAccount` (a purse with no account is left out, so no value). The native request keeps the facts when a rule
  reads a purse. Approval checks every purse a value reads (triggers, Readiness, quests) as SQ2 does (`_purse_sites`).
  `check_world_facts` accepts the section.
- **C12b: `starts` and `in`** (`story_program.match_value_ok`, `match_ok`, `match_one`, `match_words`, `plain_match`,
  `match_values`, `stand_in_match`; `matchOne`, `fieldCompare` in the native twin, both `matches` and the classic `field`
  condition). In triggers, exits, prerequisites, missions, quest objectives and fail rules (the signal rule too), and
  Readiness's `then`; `needsMatch` fields stay exact ("… as exact words (not "begins with" or "one of")"). The approval
  checks read each value of an `in` (a `starts` names nothing to look up); the console's stand-in events use a `starts`'s
  text or an `in`'s first; Why not? words the expected value; a weather filter's `in` watches each cell.
- **N3: a mirror** (`_check_mirror`, `mirror_value`; `Dispatch2::mirror`). A shown player number with `mirror {key, names}`
  (1 to 16, each a declared world number; the key a player text): computed when read, 0 for a key with no name, never set
  (every write refused). `key` as a `realm` text waits for J2 (Phase 8).
- **J4: the moon** (`Game::moonQuarter`, the octant of `moonName`: new, waxing, full, waning). The world facts' `moon`; the
  watched event `moon` (detail the quarter, quantity the day of the round) when the quarter changes, remembered from the
  first look (`storyMoon_`); `calendar is` `moon` in both twins; `READY_ANCHORS` gains `moon` and `ready_words` says "the next
  new moon".
- **SQ1, SQ5, L3: what an objective counts** (`story_quests._check_objective`, `Run.track` (rewritten to group by copy),
  `where`, `credit_of`). `where` cells (1 to 16) or a region (a tile inside; no tile: by the cell; no place: never); `credit`
  `actor`, `target` (stall sale, vouch, told and vouched to the Wardens) or `present` (the cohort wolves in the event's cell
  by the facts' `wolves`, 500 at most); one fact gains a copy once, every credited wolf credited. The six events are mission
  events; the five with no wolf need `present` ("Nobody is the actor of a wagon arriving: credit the wolves present"). The
  approval checks the cells (`world_checks`).
- **M5**: `trade done` has `cell` (`tile` false: the game writes no tile for it).
- **N4: `join.again`** (1 to 365): `story_quests.again` waits that long after any end; done takes the later of it and the
  repeat's gap. The game already shows "Not yet." by the view's `again`.
- **L2: named camps** (`World::namedStoryCamp`, `storyBandits` and `callBandits` with a `name`; the game passes the payload's
  `camp`). The camp is `camp_story_<name>`: a second spawn while it stands adds its strength (and moves it to the new ground
  while none of its bandits stand in person); a cleared one is made anew under the name. The program's top-level `prefix`
  (2 to 8 small letters) is required by a program that names a camp, and every name begins with it and `_`
  (`_check_camps`, `camps_of`, also in the Problems list); V1's objective check accepts `camp_story_<name>` only for a name the
  program spawns.
- **Workbench** (`Editor/`): every operand parameter's picker has "…or a value" (the operand picker of its kind) and shows a
  value in words (`isOperand`, `shown`); filters' and the field condition's forms in words (`matchWords`: "begins with
  “Stood watch”", "one of Ivo Tarmarsh, …"); fills as chips in sentences (`fillChips`, `Sentence.tsx`); the text picker offers
  the declared variables to fill in and the widest length (`fillsOf`, `widestFilled`); the rehearsal shows tokens as
  «wolf: Ash» (`tokenWords`); the quest editor's goal "or a value…" (a number variable, or the goal or two for each active
  wolf) with "keeps up with it", "Counts anywhere / only in some cells / only inside a region", "Who is credited", and "The
  same wolf again after"; the variables' "Shown of" and a mirror's summary; `editable` takes a field condition's list.
- **Guide and format**: `Docs/Story-Maker-guide.md` §6 "What a purse holds now", "Values in an effect", "Words with fills", "A
  number shown of another, and a mirror"; §7 a goal from a value, again after a wait, "Where an objective counts", "Who an
  objective credits", "A story's camps"; §8 filters that begin with or are one of (a deed's detail), the moon; §15 three rows;
  §16 the limits; the reference regenerated (`story_guide.py --write`; `--check` passes). `Docs/story-programs.md` gains
  "Values, fills and objectives". The catalogue's `about`, `types.operand`, `types.camp`, `trigger.match`.
- **Sample** `story_v2_samples.VALUES` ("The beacon road", `v2-beacons`, prefix `sq`, in `all_samples()`, so both twins run it
  under five seeds): a story-scope bar whose goal is `max(12, times(audience active, 4))` with `goalLive`, a tracker of its
  goal, a pledge that sets a world number a wolf's mirror reads (and pays by it), a deed matched by how it begins (and a
  field condition's `in`), a board post with a wolf's name and two numbers filled, a journal note to `{"field": "actor"}`, a
  named camp spawned at the opening and cleared as a plain objective, a wagon objective with `credit: present` and `where`,
  rewarded from what the purse holds (212 pennies pay 10), and a Readiness whose `then` is the next new moon. Its one case
  pins the effects; it rehearses clean: nothing uncovered.
- **Tests**: `test_story_v2` (`Phase2Values`: coins from a value, kept and floored; a missing value skipped with its note; a
  resident from a variable, a wolf from the event; the kinds checked, entry effects literal, `npc.line` refused; fills
  (tokens, digits, braces, undeclared, `.of` rules, a lone brace, widest length at 300 and 301, none at a phase's opening);
  the purse (10 at 212, an unknown purse skipped, watched); the mirror (two wolves, no town 0, read by triggers, never set,
  only world numbers); `showOf`; `starts`/`in` (case-sensitive, the gift rule, the bounds, exact `needsMatch`, text fields
  only); the moon; the prefix and camp names), `test_story_native` (`ValuesParity`: every form over four states and four
  events, both twins identical, notes included), `test_story_quests` (`Phase2`: `where` by cells and by region; `credit`
  target and present, a story copy once, "by the watch" nobody; the six mission events; `trade done`'s cell; a goal from a
  value read at the start and kept up, falling below what is made; `join.again` after abandoning and with a daily repeat; a
  reward that follows the purse, an unknown purse noted, words filled per wolf, an offer's `.of` refused),
  `test_story_program` (`FilterForms`; Readiness `then: moon` accepted, `month begins` still refused), `test_story_problems`
  (`ValuesProblems`; `ValuesInTheWorld` with a throwaway database: an unknown cell in `where`, a named camp's objective, a
  purse a value reads, values passed to the game), `Tests/story_facts_tests.cpp` (`phaseTwoMoonAndPurses`: day 88.6 new, 103.4
  full, one event a change, the facts' moon and purse, nothing after the end), `Tests/roads_tests.cpp` (`namedStoryCamps`: the
  id, a second spawn joins, called bandits join near the wolf, cleared by a fight with `camp cleared` and `bandit falls`
  carrying the id, the name reused), `Tests/story_world_tests.cpp` (`phase2Names`: tokens become names, unknown kinds kept,
  the cut, a told line, an offer's and a journal's words), `Client/src/game/quests.test.ts` (a / b and the bar),
  `workbench.test.ts` (the new words, 113 programs). Updated counts: `test_story_catalogue` (206 events, 69 mission events;
  the hand-written effect checks leave the optional `camp` out), `test_story_facts` and `workbench.test.ts` (33 watched),
  `test_story_drafts` (209).
- **Run** (with the local PostgreSQL: the tests' own throwaway `ratw_test_*` databases and `RATW_STORY_TEST_DSN` on a throwaway
  migrated `ratw_test_nhp2_main`, all dropped after): every `tools/test_story_*.py` but the three live ones (`test_story_scenes`
  through `unittest`), `test_dungeon_master` (two errors on a first run beside a parallel build; clean on the run after),
  `test_dialect_check`, `test_talk_*`, all passed; ctest `-R "talk_|dialect|mantle_|story|storyline|troubles|dialogue|checkpoint|
  save_compat|game_|world_|project|roads|camp"` (44) passed but `talk_scorecard_quick`'s latency gate under the parallel run,
  which passes alone (as in Phase 1b); the Editor's tests (144 and 78), type check, build and render smoke (62 panels; the
  workbench over 108, 14 and 2 programs, none edited); the Client's tests (245) and build.
- **Left or changed, and why**: the spec's C1b fills for `npc.greet.text` and `title.give.title` and C1a's `{"share": true}`
  in rewards come with those effects (C4, C5, C3: Phase 3). A mirror's `key` as a `realm` text waits for J2 (Phase 8). A
  goal not known when its copy starts is out of reach (1,000,000) until it is known, rather than 1. `goalLive` is also read
  after every firing (so a goal lowered by a trigger completes on that event), not only "at each journal sync": the journal
  view shows the live value without storing it. SQ5's `present` makes the program watch its wolves whenever any objective
  says it, not only while one is under way (the watch is per program). A named camp that has bandits out in person keeps
  its ground when a second spawn joins it (its strength grows; new bandits come out the next time it is staged), rather than
  moving under them. A fill's number is truncated toward zero and a text fill counts 200 at its widest, so a text fill does
  not fit a 200-character note or deed (the spec's rule). A phase's entry effects take neither values nor fills (refused
  with "fills words in a trigger's effect…"): they are not run through `_act`. The guide's §13 and §14 are unchanged (nothing
  new is undone at a run's end). `RUNNER_VERSION` is `doc86-phase2`.
- **Going live** on `ratw_ridgemere3` needs no migration of its own; the game server restarted from this branch (the moon,
  the purse facts, named camps and the name tokens are the game's), the story runner from this branch with the rebuilt
  `libratw_story.so` (a run's effects with values or fills need both twins), the Editor and the client released from it. Act
  I's chapters can move onto a version that uses these by `migrate`; camp, wagon and festival objectives become plain
  objectives in a new version, and the `join.cap: 1` quests may take `join.again: 7`. Fills apply to every version 2 story
  this runner plays, also one approved under Phase 1b: before the runner goes live, look through the running releases'
  `story.tell`, `board.post`, `journal.note`, `npc.speak` and `deed.award` words (and quest summaries and labels) for a
  literal `{`, `}` or `{{` (a fill would blank it, with a note).
- **After review** (the review of `326fc330`): **B1** an exit on `near resident`, `purse crossed` or `standing crossed`
  names what the game looks at as exact words, as a trigger does (`_check_exit` with `NEEDS_MATCH`; the number to cross a
  whole number), and `watch_of` passes over a value that is not a text: an exit's "one of" had passed approval and then
  failed the watch (`TypeError: unhashable type: 'dict'`) at publish and on every tick. **G1** a goal not known when its copy
  starts (`goal: null`) is read again whenever the copy is credited (`track`, `objective.progress`) and after every event
  (`keep_up`, now for every quest with a goal from a value, and `Run.unread_goals`, which completes an objective already
  reached), with or without `goalLive`; the console's quest commands work with the world's facts now (`Steer._quest_run`).
  **G2** the journal and the offers are built with the world's facts now (`sync_journal(facts=…)`, `_journal_facts`), the
  view reads a goal not known yet as it reads now, and the fingerprint follows the story's wolves counted (all, active,
  online) and the purses its quests read; the console's Quests grid gives a goal from a value as it reads now (`None` when not
  known) with the value as `goalFrom`. **G3** the summary of a quest a giver or a board offers takes no `{name.of}` (it is
  the offer's words for every wolf), nor does a reward given once (`reward_wolves`, shared by the check and `Run.reward`).
  **G4** the console's Set refuses a mirror ("mirrors a world number: set that number instead"), and a mirror takes no
  `talk`. **G5** a reward not given for a missing value goes to the run's timeline as "Reward not given" (`_reward_notes`,
  drained from `rewardNotes` at each journal sync). **N7** the world checks read each name of an objective's "one of" (the
  camp, the resident or character, the item). **N8** the moon's detail is new, waxing, full or waning (or one of them), for a
  trigger, an exit and Readiness's `then` (`check_moon`), and in a rehearsal's facts. **N9** the workbench's "…or a value"
  for an id parameter offers the variables and event fields of its kind (`OPERAND_VARS`, `OPERAND_FIELDS`, as the check).
  The program's watch also counts the wolves online when a quest's goal reads them (`uses_online` over the quests). Left:
  **N10** (fills for a story approved under Phase 1b: a check before going live, above, as no release records the runner it
  was approved under), **N11** (`deed.award`'s detail 200 in the catalogue, 160 in the game: older than this phase; lowering
  the catalogue would refuse approved stories' longer details, raising the game's needs a rebuild and the deeds' own
  limit checked), **N12** (a text variable holding `{@w:…}` turns into a name: tokens only come from the game's own
  details and a story's own words, and escaping them needs both engines and a native rebuild; left for Phase 3's talk work).
  Tests: `test_story_v2` (`Phase2Values`: an exit's exact words, a mirror's Set and talk, the moon's detail),
  `test_story_quests` (`Phase2`: a goal not known at the start read on credit and after an event, the journal, the board
  and the fingerprint; the summary and a once-given reward with `.of`), `test_story_problems` (`ValuesInTheWorld`: each name
  of a "one of" checked).

## Phase 3 as built

**Phase 3 as built** (2026-11-23, branch `r3-nh-p3`, on `r3-nh-p2` at `cbfafb63`). Engine spec items C3 (a, b), J11, C4,
C5, J8, V2 (with T4) and T5. **No migration**: titles live on the character record, greetings in the checkpoint beside the
stories' talk memory, shares in the quest copies, doubt in the beliefs. Four new effects (82 in all: `title.give`,
`title.take`, `npc.greet`, `belief.forget`, each a game order of its own name, `WORLD_ORDERS`), one new action (`rank`), one
new event (`title given`, group fame, a mission event: 207 events, 70 mission events), two new value forms (`tally`,
`share`) and two quest fields (`credit: "share"`, `creditMin`).
- **C3a: Rank** (`{"do": "rank", "var", "top", "into", "over", "list", "min"}`; `story_triggers.rank` and `_check_action_v2`;
  `Dispatch2::rank` in the native twin). The wolves of the story (or of a story or world list of wolves, only those in the
  story) with a stored player number, at least `min`, highest first, ties by `fnv(seed + "|" + wolf)`, the first `top`
  (1 to 500, a value floored and kept to it) into a story or world list of wolves, cleared first. A `top` or `min` with no
  value changes nothing ("skipped: rank has no value", in both twins). Refused inside For each, by a number that is not each
  wolf's own (or a mirror), into a personal list. `rank` is in `STATE_V2`; `variable_uses` counts its `var` and `list` as read
  and `into` as written.
- **C3b: rewards by share** (`story_quests`: `check_quest`, `Run.share`, `complete`, `reward`; `reward_wolves`). A quest of
  scope `story` or `party` with `"credit": "share"` keeps `shares` (`{wolf: n}`) in its copy: what each credited wolf's
  progress added (a counted event's gain, capped at the goal, to every wolf it credits; an `objective.progress`'s amount; the
  rest of the goal on an `objective.complete`). Its rewards are given for each credited wolf, `{"share": true}` that wolf's
  amount (`Ctx.share`, read by `value_of`); `creditMin` (1 to 1,000,000) leaves every reward out for a wolf below it. A
  `{"share": true}` anywhere else is refused (`operand_type(..., share_ok)`); the native twin answers it with no value (a
  trigger never has one).
- **J11: tally** (`{"tally": {"sum", "where", "is", "over"}}`; `story_triggers.tally`, `_check_tally`; `Dispatch2::tally`,
  `wolvesOf`). Over every wolf of the story (5,000, no 500 cap) or of a story or world list, those whose player text or switch
  `where` is `is` (by its first value when unset), their player number `sum` added (0 when unset), or counted without
  `sum`. Not inside For each nor a test for each wolf (R1), one level deep in arithmetic. The native request already carries
  every wolf's counters for a version 2 story (`STATE_V2`), so no widening was needed.
- **C4: titles** (`Game::applyStoryThanks`, `storyTitleFor`, `titlesView`; `StoryTitle` and `Entity::titles`,
  `titlesHidden` in `RatwWorld.h`, saved with the character in `RatwWire.cpp`). Up to 8 a wolf, `{title, known: "realm" |
  "town:<id>", by: <program id>, run, since, until}`; given again (any case) a title is renewed where it is now known; a
  ninth drops the oldest that lapses, else the oldest; a lapsed one is dropped when another is given and never shown or
  used. The title is filled (C1b) to 60, one line, no `#` and no ids. A deed of kind `award` ("was named <title>", notable,
  source `story`) carries it into gossip and the crier, and the event `title given` is recorded (`item` where it is known,
  `detail` the title). `title.take` takes back this program's (one by its title, or all). The window's facts
  `player.titled` and `player.title` (the newest known where the resident lives), in `Data/Talk/facts.json`. The client's
  character sheet shows them under the name (`Client/src/game/fame.ts` `titleLine`), with **Hide your titles from your card**
  (D3: the command `social` `hidetitles`; residents still use them), and the closer look shows another wolf's unless hidden.
  Not undone at a run's end.
- **C5: a resident's greeting** (`storyGreets_`, `takeStoryGreet`, saved as `storyGreets` beside `storyMemory`). One for each
  resident and wolf (a later replaces it), 20,000 in all (the soonest to lapse goes first), for 1 to 28 days (7 when not
  given), the text filled to 200; for a wolf of the story. When the wolf opens that resident's Talk window (and the resident
  is not asleep), the greeting is the opening line, through `Game::inVoice` (`#symbols#` in the resident's dialect), once.
  Not undone at a run's end.
- **T5**: the phase's `npc.line` for its wolves is said through `inVoice` too (`RatwGameDialogue.cpp`, after the greeting);
  words the expander cannot fill are said as written.
- **J8: doubt** (`World::doubt` in `RatwRoads.cpp`). Each holder (of the town: a resident by their home, anyone else by where
  they are; or everywhere) of a belief of that kind about the subject keeps `1 - share/100` of its strength; at 5% or below it
  is forgotten. The kinds are claims.json's that a story may put to rest; `test_story_catalogue` holds the catalogue's list,
  claims.json and the game's (`doubtable`) together. Lasting.
- **V2: the story words check** (`tools/story_words.py`, `Data/Storylines/words.json`): every word field (effects in phases,
  triggers and rewards, quest titles, summaries, labels and offers, a phase's journal, a finish's line, scene lines),
  rules 1 to 7 of the spec: refused are the words and phrases of `words.json`, a `#symbol#` the talk library does not have
  (any grammar's symbol, a fact of a known section; `player.*` checked against facts.json) or one in words no resident says, a
  stray bracket or unclosed `#`, an id-shaped word in anything a wolf reads (with the world: any resident's or place's id that
  is id-shaped), a length past its limit with fills at their widest, a face banner that quotes its resident; warned of are
  another dialect's phrases in a resident's words (their `live.npc_voice` origin and accent, `settings.json` `dialect_of`, the
  dialect check's lexicon), a face banner at all, `npc.speak` said to a wolf, a clearing that begins "never" or "did not". It
  runs in `story_runner.world_checks` (approval: refusals block, warnings are returned; the console's Problems list through
  `reference_problems`), as `story_author` op `words` (offline), and from the command line (`--stories DIR`, `--world KEY` read
  only, `--json`). The 108 bundled programs, the examples and the samples pass it clean.
- **Workbench** (`Editor/`): Rank's action card and sentence from the catalogue (`newAction`, `actionVars`, the `Action` type);
  a variable slot's `scope` narrows its picker; "…a value" offers a tally ("how many wolves the story has", "every wolf's
  <number>, added up") and, in a program with a quest by share, "the wolf's share"; `operandWords` says a tally and a share
  in words (`tallyWords`); the quest editor's **Reward each by their share** and **The least share**; `operandVars` exported.
- **Guide and format**: `Docs/Story-Maker-guide.md` §5 Rank in the flow table; §6 "Counting over every wolf" (tally against
  For each); §7 rewards in proportion, thanks by name, titles; §8 putting a rumour to rest; §10 the words check; §14 titles,
  greetings and doubt stay; §15 three rows; §16 the limits; the reference regenerated (`story_guide.py --write`; `--check`
  passes). `Docs/story-programs.md` gains "Ranks, titles, thanks and words". The catalogue's `about` and `types.operand`.
- **Sample** `story_v2_samples.THANKS` ("The oil drive", `v2-thanks`, in `all_samples()`, so both twins run it): a story-scope
  quest for nine measures of oil by share (5, 3 and 1 brought; `creditMin` 2 leaves the third out: 10 and 6 pennies), thanked
  in the giver's voice (`npc.greet` with `#greet#`) and named Lamp-bearer of Greyfen for 30 days in the town; a tally of all
  the oil and of the lamp's backers'; the two who brought most ranked and named the Oil-Bringer for the realm; a rumour put to
  rest; a title taken back for a spilled cask; the phase's line with a `#symbol#`. Its case pins every effect; it rehearses
  clean: nothing uncovered.
- **Tests**: `test_story_v2` (`Phase3Ranks`: ties by the seed and another seed, the least and the list cleared, over a list
  into a world list, a top from a value, the checks; tally sums and counts with `where`, a switch's unset value, over a list;
  its checks; 1,200 wolves under the rehearsal budget, both twins agreeing), `test_story_native` (`RankParity`: six tallies,
  two ranks, a tally in arithmetic and titles for the ranked, over four states, both twins identical), `test_story_speed`
  (`TallySpeed`: nine tallies and a rank of 500 over 5,000 wolves, native under 50 ms, both twins agreeing),
  `test_story_quests` (`Phase3Shares`: 5/3/1 paid 10/6/2; `creditMin` 2 leaves one out of every reward; the checks; a
  trigger's progress counted toward the share), `test_story_words` (each rule, the giver's dialect, ids with the world, the
  bundled 108 clean, the CLI over a directory; `talk_check --validate` takes a line gated on `player.titled` with
  `#player.title#` and refuses a value facts.json lacks), `test_story_problems` (`WordsInTheWorld`, a throwaway database:
  forbidden words, an id and a quoting banner refused at approval, the Problems list's; a giver's dialect warned of from
  `live.npc_voice`; a title's and a doubt's town), `test_story_effects` and `test_story_catalogue` (the four orders, 82
  effects, 207 events, 70 mission events; the doubt's kinds against claims.json and the game), `test_story_drafts` (210),
  `Tests/story_world_tests.cpp` (`phase3Thanks`: named for good, the event and the deed, a town's title over the realm's for
  its residents, renewed not doubled, lapsed, the card's view, the ninth (the one that lapses, then the oldest), taken by
  title and all of this story's (not another's), hidden from the card; a greeting kept with names filled, replaced, a wolf
  of the story only; doubt by half twice in the town and not elsewhere, 100 forgets; titles and the greeting through a save),
  `Tests/dialogue_tests.cpp` (`storyGreetingsInTheWindow`: once, in a Larkfield resident's words, not for Bo, replaced, lapsed;
  a line gated on the title said with it; `npc.line` with a `#symbol#` in the speaker's voice), `Client/src/game/fame.test.ts`
  (the card's title lines), `workbench.test.ts` (114 programs; rank, a tally and a share in words; rank's reads and writes).
- **Left or changed, and why**: `belief.forget`'s kind is the parameter `claimKind` (as `belief.plant`'s), not `kind`: an
  effect's own `kind` is its name, so the spec's `kind` could not be a parameter. Rank and tally read **player** numbers
  only: `realm` (the spec's other scope) waits for J2 (Phase 8), and the catalogue's `scope` says `["player"]` until then.
  `title.give`'s `known` is optional (the realm), and its title may hold fills (C1b); `npc.greet`'s days are 7 when not
  given. A title belongs to the program that gave it (`by`: the program's id), so `title.take` takes back only its own, as
  the spec's "this story" reads; a campaign's later chapter cannot take back an earlier chapter's title. D3's setting is a
  flag on the character (the card's button), not a local client preference, so it holds on every device. V2's symbols are
  read from the grammar files and facts.json in Python (the dialect check's reader), not the C++ loader: every grammar's
  symbol, any modifier, and the facts of the known sections (`player.*` by name); its world id rule takes only id-shaped words
  (a place whose id is a plain word, "gate", is not a refusal). V2 runs at approval and in the world's Problems, not in the
  offline Problems list (the offline drafts would otherwise warn of voices they cannot know). The greeting replaces the
  Talk window's opening line but the phase's `npc.line` still follows it. No undo at a run's end (nothing new in
  `UNDONE_AT_END`). `RUNNER_VERSION` is `doc86-phase3`.
- **After review** (the same branch): a quest by share with no `creditMin` left a wolf who added nothing in, clamped up to
  each parameter's least (1 coin, and every flat reward): the least share is now 1 when not given (`Run.complete`;
  `Phase3Shares.test_a_wolf_who_added_nothing_gets_nothing`). V2 let `#symbols#` into `npc.speak` and a scene's lines, which
  the game says as written: a site now says whether it is **voiced** (a greeting, `npc.line`, an offer's words, a choice's
  reply) apart from **spoken** (a resident's: their dialect is still read), and a symbol in an unvoiced one is refused. V2
  now reads every text parameter of the catalogue (`EFFECT_WORDS`: a figure's name, title and about; what people say of
  times, a harvest and prices; a festival's name, a project's title, the reasons of a warrant, a payment, a standing and a
  relation; `title.take`'s title; `deed.revoke`'s detail), and a test holds the catalogue's text parameters to it (`EXEMPT`
  is empty). An id is `wolf-` with a digit (or 8 hex): "wolf-pack" is a word. A fact symbol (`#player.title#`) in a story's
  words is a warning, and the window leaves a symbol it cannot fill out rather than showing it. The greeting is taken only
  by the Talk entry (`dialogueOpen`, `base == "talk"`): Trade, Introduce, a fine and the rest keep their own line and the
  greeting waits (`storyGreetingsInTheWindow`: Introduce answers as ever, then Talk greets). C5's "a reward's greeting is
  from the giver unless it says otherwise" is built: `story_quests.from_giver` fills a reward's `npc.greet` without `npc` from
  the quest's giver (in `check_quest`, `Run.reward` and `possible_effects`), refused with no giver; the catalogue keeps `npc`
  required for triggers. `title.take`'s title has `fills` and the game fills it as `title.give` does, so a title given with a
  name is taken back by the same words; a title renewed writes no second "was named" deed (`phase3Thanks`). The closer look
  veils the names in another wolf's titles as their description is (`titlesView(wolf, viewer)`). Not changed: the value
  picker still offers the share in any number slot of a program with a quest by share, and a tally inside For each (the
  picker does not know where its slot is; the checker refuses both, with a problem shown); the offline Problems list still
  does not run V2 (above).
- **Going live** on `ratw_ridgemere3` needs no migration; the game server restarted from this branch (titles on the
  character record, greetings in the checkpoint, the window's voice, doubt), the story runner from this branch with the
  rebuilt `libratw_story.so` (rank and tally need both twins), the Editor and the client released from it. Act I's chapters
  can move onto a version that uses titles, thanks and ranks by `migrate`. A saved character from before this branch loads
  with no titles; one saved by it loads on an older server with its titles dropped (an unknown field).

## Phase 4 as built

**Phase 4 as built** (2026-11-23, branch `r3-nh-p4`, on `r3-nh-p3` at `1914ce7b`). Engine spec items C12a, C13a, C13b, C6
(with its display side), N1 (= T8), J3 (capped, answer (b)), J10 and L5. **No migration**: the notices wait in the run's
state, the tellings go through the command inbox (0071), `withdrawn` is run state, the closure receipts are the social
ledger's, which lives in the checkpoint (so 0082, "an index if `game.social_entries` is a table", is not needed: it is not).
Two new story effects (84 in all: `story.notify` in Chapters, `quest.withdraw` in Quests, both the story's own, never game
orders), four new events (211: `story notified` and `variable changed`, the runner's own, group story; `month begins`,
watched, group time, 34 watched; `succession`, group town), one new condition (`wolf has`, a world-facts condition: 24 with
the quest ones), `resident has` reads `post`, two quest fields (`requires`, `closure`), two offer fields (`during`,
`duringAsk`), one runner order (`story.closure`) and one game order of that name.
- **C12a: news for another story** (`story_quests.Run.story_effect` queues `state.notices`, 5,000 at most;
  `story_runner.tell_notices`). After a run's tick (in `tick_run`'s `finally`, after every save, the rest patched into the
  state) and after a command's save (`Command.save`), at most 200 notices go: to every other live run (running, paused,
  held) whose program has a trigger or exit on `story notified` whose `match.detail` takes the signal (`_listens` now reads a
  filter, `rules.match_one`), or, when `story` is named, only to the runs of that program that listen. Each is a
  `story-event` command (`_story_event`, shared with `tell_stories`), taken between two events by `do_story_event`, which now
  accepts the four story events (`STORY_EVENTS`) and carries `actor` and `quantity`. The event needs `match.detail` (exact
  words; `match_word` says "news (detail)"). `story.notify` may name `$actor` from any trigger (it only names the wolf:
  `NOTICE_EFFECTS` beside `TAKES_IN`). Approval warns (never refuses) of a `story` no release of the world has.
- **C13a: a world variable changed** (`story_runner.world_save` → `tell_changes`). Every world variable a save writes that
  changed is told to every other live run that declares it (the same name and type, world scope) and listens for
  `variable changed` on it: `{target: name, detail: the value as words, quantity: the number (a switch's 1 or 0, a list's
  length), item: the run}`. Coalesced: a telling of the same variable to the same run not taken yet is brought up to date
  (an `UPDATE` of the queued command's payload) instead of a second. A run never hears its own writes. The check: the trigger's
  `match.target` names a world variable the story declares (`rules.check_variable_changed`).
  **`month begins`**: the game's watch writes it on the day change into a new month (`calendar::monthOfYear`, 28-day months
  counted from the year's first day; the year's 365th day stays in the 13th, so the months are 1 to 13), `quantity` and
  `detail` the number; it is a Readiness `then` moment now (`READY_ANCHORS`, "the next month"). **`succession`**: the careers
  notes were already world events (`World::careerNotes`) and the story intake reads every event, so only the catalogue entry.
- **C13b and J10 (both twins)**: the facts writer adds `residents[id].post` (the title of `Society::jobOf`, "" for none) and
  `wolves[id].gift` (`{tier, family}`); `RESIDENT_HAS` gains `post`; the condition `wolf has` (`what` tier or family, `value`
  checked against `GIFT_TIERS` or the playable families, `GIFT_FAMILIES`, which a test holds to `Data/Gifts/families.json`;
  `""` is a normal wolf's family); `fact_test` and `Dispatch2::fact`; `check_world_facts` takes `gift` and `post`.
- **C6: chains and gates** (`story_quests`: `check_requires`, `requires_personal`, `requires_hold`, `gates`, `offers`,
  `fingerprint`, `Run.may_start`). `requires` is a condition tree of `compare`, `switch`, `in list`, `days since`,
  `quest is`, `objective is` and `phase is` (in all, any, not; 4 deep, 12 conditions; no event field but `actor`, the wolf
  tested; no loop value or share; no purse, not who is online: the world's facts are not known when an offer is worked out;
  `REQUIRES_TESTS`), checked by the v2 checker with the program's quests and phases. `gates` works it out once per state
  (cached by a digest of the trees themselves and of the state's quests, counters, parties, seen and online wolves, phase,
  cohort and day, so a run moved to a new version is worked out again): a tree that reads the wolf's own (`of: $actor`,
  `{"field": "actor"}`, a player or party variable, a quest's copy not the story's) for each wolf of the story, giving the
  offer `players`; else one gate (false: the offer goes with `players: []`, closed to new takers, so the copies under way
  keep the giver's words while under way, the hand-in's words and the thanks). The results and the `withdrawn` marks are in the journal's fingerprint. Taking it (the giver's window, the board,
  `quest.offer`, `quest.start`) checks it again for that wolf: "Not yet." (a toast). `offer.during` (300, fills, `#symbols#`)
  and `duringAsk` (120, "About <title>..."): the offer carries both, filled. In the game, `storyOfferOpen` refuses a wolf not
  in `players` or in `except` (the board's view and the window alike); `storyChoicesFor` adds a `during` choice while the
  wolf's view of the quest is `active` with that giver, whose node `@story:<run>:during:<quest>` (`storyNode`) says the words
  through the window's voice and goes back to the hub; the window's watched `dialogue node` event is raised as for any node.
- **L5: an offer taken back** (`quest.withdraw`, the story's own; a quest effect, so its quest must be the program's own and
  the workbench's rename follows it): out of `questOffers`, into the run's `withdrawn`; the giver's
  offer then carries `except` (or, with a personal `requires`, the wolf left out of `players`); a copy taken stays;
  `quest.offer` to that wolf clears it; a quest gone in a new version drops its marks (`migrate`).
- **N1: many stories at one resident** (`RatwGameWindowTalk.cpp` `windowNode`; `storyChoicesFor`). The story choices come
  hand-ins first, then offers, then the words while under way, then choices, each in story-run order. In a resident's own tree
  (`resident`, home `hub`), the hub takes what fits its nine, the `about` node the next (in what room it leaves), and the hub
  gets "Ask about something else..." only if it has no way to `about` (the resident tree always has one), and then only
  with room for it and one more (or the one place left, when the first story choice is not a hand-in; on a full hub,
  none); only what `about`
  has no room for is left out, noted once a resident a game day (`RATW_STORY ... (a giver crowded with stories)`, the game's
  log; `storyCrowdNoted_`). Other trees keep the hub's room and leave the rest out, noted the same way.
- **J3: quests that count as a finished Story** (`story_quests.closure_of`, `check_quest`, `check_closures`, `Run.complete`;
  `story_runner.order_of`, `world_checks`; `Game::storyClosure`, `SocialLedger::storyQuestClosure`). `closure` is `"line"`
  (`true` read as it), or `"side"`; a quest of scope player or party, taken once; at most 4 a program; a `line` requires
  another quest of the program done and is required done by none. A `side` closure outside a campaign chapter is refused at
  approval ("A side story counts only in a campaign chapter"; a warning in the offline Problems list, which does not know).
  Done, each wolf with credit gets the runner's order `story.closure` (target the wolf; `{run, story, quest, kind, title}`)
  with the rewards (`ifRefused` "when here" as any reward). The game writes one `story_closure` receipt, session
  `quest:<program>/<quest>`, keyed by the program's id (a new version or a second run never counts a quest twice: "Counted
  already"), refuses to pay a fifth from one program for one wolf (applied: "Not counted: already 4 from this story."),
  pays `storyQuests.closure` social (3) within the daily cap (`Data/Progression/standing.json`, read into
  `practice::Standing`, with `closuresMost` 4), and checks the account's unlocks; `measuresOf` already counts the distinct
  sessions into `closedStories`. Never undone.
- **Workbench** (`Editor/`): the quest editor's **Only for wolves who…** (the trigger's condition editor, `ConditionNode`
  with `only={REQUIRES_TESTS}`: the tests it may not use greyed out, "an offer is not an event"; **+ only for wolves who…**
  starts with the quest before it done), **While under way, the giver says…** with the wolf's words, and **Counts as a
  finished Story** (no, the last chapter of a line, a campaign side story); the two effects' and four events' cards and
  sentences come from the catalogue; `wolf has` is a version 2 condition the workbench opens (`V2_TESTS`).
- **Guide and format**: `Docs/Story-Maker-guide.md` §4 "Quest packs that feed a campaign" and "A world variable that
  changes" (instead of looking at a variable every hour); §7 "Chains: requires", "The giver while under way", "Many stories
  at one resident", "Quests that count as a finished Story" (the game's words never claim it); §8 the month, a post, a
  wolf's Gift; §14 two rows; §15 two rows; §16 four rows; the reference regenerated (`story_guide.py --write`; `--check`
  passes). `Docs/story-programs.md` gains "Stories that talk to each other". `RUNNER_VERSION` is `doc86-phase4`.
- **Sample** `story_v2_samples.LINE` ("The lamp line", `v2-lamp-line`, in `all_samples()`, so both twins run it): part one
  at the lamplighter's, part two only for the wolf who did it (Birch is told "Not yet."), with words while under way and
  closing the line (its `story.closure` pinned in the case's effects); the mill road taken back from Ash when he takes part
  two; the news told to every chapter listening and to `nh-ch1` by name; news heard from a pack raising Ash's voice; the
  realm's tally changed by another story, a new month and a post changing hands; a Quickened wolf at the gate while the
  Great Palace's post is held (a normal one does not count). It rehearses clean: nothing uncovered.
- **Tests**: `test_story_quests` (`Phase4Quests`: a chain offered only to the wolf who finished part one, "Not yet." for
  another, the words while under way in the offer; a gate for everyone, one offer or none, in the fingerprint; withdrawn
  from one wolf and still offered to another, no journal mark, offered again clears it, a copy taken stays; a closure once,
  `true` read as a line, its order, each member of a party one; the closure checks (a line not ending a chain either way, a
  fifth, a daily one, a story's); the `requires` and `during` checks (world facts, event fields, an unknown quest); news
  queued), `test_story_campaigns` (`StoriesThatTalk`, a throwaway database: a pack's news raises the running chapter's
  `p_voice`; news nobody listens for sends nothing; 250 notices go 200 then 50; one named for a story not running reaches
  nobody; a world variable changed wakes the chapter once with its value; two changes before it takes the first are one
  telling with the latest; a story's own write tells nobody; the checks; a side closure refused alone and approved in a
  campaign; a warning for news to a story not approved), `test_story_native` (`GiftParity`: `wolf has` and `resident has
  post` over four sets of facts, the two story events, `story.notify` and `quest.withdraw` carried alike by both twins),
  `test_story_facts` (gift and post in the made-up worlds, the checks, the playable families), `test_story_program` (the
  month as a Readiness moment, `succession` not one), `test_story_catalogue` and `test_story_effects` (84 effects, 211
  events), `Tests/story_facts_tests.cpp` (`phaseFourMonthGiftsAndPosts`: the month's numbering, no event inside a month, one
  on its first day and not the next, none after the story ends; Ash's Quickened fire and Bo's none then seer; the post of a
  resident at work), `Tests/storyline_tests.cpp` (`storyClosures`: closedStories up by one, the same quest counted once,
  four from one story, the fifth applied and unpaid, another story pays, a resident or a bad kind refused, the receipts
  through a restart), `Tests/dialogue_tests.cpp` (`storyChainsInTheWindow`: an offer for the wolves listed and not one it was
  taken back from; the words while under way in the giver's voice, and the watched node; a hand-in and twelve choices at one
  giver: the hand-in on the hub, the rest under Ask about... in order, the hub and `about` nine at most, a choice there
  answered as on the hub), `workbench.test.ts` (115 programs; the lamp line in words; the workbench's `REQUIRES_TESTS` the
  checks' own).
- **Left or changed, and why**: J10's tier is read from the wolf itself (`Entity::quickened`, set when the character is made
  and by `World::giveGift`, the way a wolf is Quickened in play), not from the account's standing record: `quickenedAt` there
  says the account may make Quickened wolves, not that this wolf is one. L5 leaves the wolf out with an `except` list on the
  offer, not the `players` list, unless the quest has a personal `requires`: a `players` list can only name the story's
  wolves, and an open story's quest is for wolves who are not in it yet. A personal gate that holds for nobody is still sent,
  with `players: []` (the game shows it to nobody). `requires` may use `phase is` too (the story's own, not the world's).
  The month is 1 to 13 with the year's last day in the 13th (the spec's `floor(dayOfYear / 28) + 1` read one-based). The
  closure's amount lives in `Data/Progression/standing.json` (`storyQuests`), where the social reasons and the daily cap the
  receipt counts against are, not in `Data/Social` (which holds none of the closure amounts). N1's crowding is in the game's
  log, not the console's Problems (which reads the programs and the world's database, not the server's log); the guide says
  how to spread a pack's quests instead. The "words while under way" choice is placed after the offers and before the story
  choices. The C13a runner test is in `test_story_campaigns` (with C12a's, as the phase's gate lists it), not
  `test_story_v2_runner`. The workbench also has **Counts as a finished Story**, which the spec did not ask for.
- **After review** (the review of `6ed2d785`): a gate for everyone that turns false no longer drops the offer: it goes with
  `players: []`, so a wolf whose copy is under way keeps the giver's words while under way, the hand-in's words and the
  thanks (`offers`). The gates' cache is keyed by a digest of the `requires` trees and of everything they read (parties, the
  wolves seen and online too), so a run moved to a new version with other `requires` is worked out again; only the digest is
  kept. `quest.withdraw` is a quest effect (`QUEST_EFFECTS`, both sides): its quest must be the program's own, and the
  workbench's `renameQuest` follows it, and follows every `quest is` and `objective is` in the triggers and in the quests'
  `requires` (`renameObjective` the `objective is`). `requires` refuses a purse, `audience: online` and a share of the wolves
  online (the world's facts are not known when an offer is worked out), and reads `{"field": "actor"}` as the wolf tested
  (personal, worked out for each wolf): "only for the wolves in a list" is `in list` with that value; the workbench's value
  picker inside **Only for wolves who…** offers only that field and leaves out who is online, the loop value and a share
  (`OfferValues`). An exit on `variable changed` must name a declared world variable, as a trigger must. A line's closure
  needs a quest done that is not under a `not` (under an `any`, in every branch; `requires_done`). In the window, a hub with no
  way to `about` gives "Ask about something else..." only a place it can spare (never the hand-in's last place, none on a
  full hub), and what `about` cannot be reached for is noted as left out. `GIFT_FAMILIES` is read from
  `Data/Gifts/families.json` (the families not `npcOnly`), and the workbench resets `wolf has`'s value when what it reads
  changes (a tier "normal", a family none). The runner reads of the other live runs only their program and visit
  (`_live_runs`). `tools/progression_catalog.py`'s social reasons include `npc_dialogue` (`test_progression_catalog` was
  failing before this phase). Tests: `test_story_quests` (`Phase4AfterReview`), `test_story_campaigns`
  (`StoriesThatTalk`: news named for one of two listening chapters reaches only its run; unnamed, both),
  `Tests/dialogue_tests.cpp` (`storyChainsInTheWindow`: an offer closed to new takers keeps the under way choice, the
  hand-in's words and the thanks), `workbench.test.ts` (renaming a quest and an objective follows `quest.withdraw`,
  `quest is` and `objective is`). A test of a hub with no way to `about` needs a resident tree without one, which the game
  does not have; it is not written.
- **Going live** on `ratw_ridgemere3` needs no migration; the game server restarted from this branch (the window's chains,
  the `during` node and the overflow, `story.closure`, the month, the gift and post facts), the story runner from this branch
  with the rebuilt `libratw_story.so` (`wolf has` needs both twins), the Editor released from it. The client needs no change.
  A pack that feeds a chapter by `story.notify` is approved alone; the chapter it names need not be approved yet (a warning).

## Phase 5 as built

**Phase 5 as built** (2026-11-23, branch `r3-nh-p5`, on `r3-nh-p4` at `0eee34c9`). Engine spec items J5, J6 and L1. **No
migration**: a staged hunt lives in the story's `held` (the story book, as the weather and the markers do), the watch sent
again is the story's `watch` (saved with it already), and the runner's mark of what it sent is run state. Two new effects (86
in all: `npc.howl` in Residents, `hunt.stage` in Places, both game orders), two new events (213: `howled` and `chorus closed`,
group talk, watched, mission events; 36 watched, 72 mission events), one new parameter type (`species`), `count in region`
gains `who` and `list`, and two game orders the runner sends itself (`story.watch`; `npc.howl` and `hunt.stage` are the
effects' own).
- **J5: howls** (`Core/RatwGameHowl.cpp`). `Game::howl` keeps a wolf's checks and words and calls the new
  `howlFrom(Entity&, bool resident)`, which opens or joins a chorus as before. `Game::residentHowl(npc)` (the order
  `npc.howl`, target the resident, through `story_runner.action`) refuses one dead, down, in a fight, away or within the
  howl's rest (`howl.cooldown`, 600 s, kept for residents as for wolves: "That resident howled a moment ago."), and answers
  "<Name> howls." or "joins the howl.". A resident's howl is not heard as a sneaking wolf's voice. `howled` is written in
  `howlFrom` while a story listens (`actor`, `cell` and the tile, `item` `townFor(cell)` or `wild`, `detail` the chorus id,
  `quantity` the howlers so far); `chorus closed` in `tendHowls` at the close (`actor` `howlers.front()`, `quantity`
  `howlers.size()`, residents too, the tile of the first howl, kept on the `Chorus`). Bonds, the scene moment and the town's
  memory are unchanged.
- **J6: residents counted in a region** (both twins: `story_triggers.fact_test`, `Dispatch2::fact`). With `"who":
  "residents"` and `list` (a `residents` variable of the story or the world; `list` exactly then; `COUNTED`), the distinct
  members whose facts say alive and inside the region are counted; a list not empty none of whom the facts have is unknown;
  an empty list counts 0. `story_native` trims no residents (only a wolves count reads every wolf). `watch_of` gives such a
  story the `residents` section and `residentLists`; `story_runner.watch_now` puts the lists' members (the run's state; a
  world list as loaded) among the watch's `residents` (2,000 at most) and strips `residentLists`; `enter` and a move onto a
  new version send it and keep a digest in `state.watchSent`; `resend_watch`, after a tick's save (`tick_run`'s `finally`,
  only for a running story that counts residents) and a console command's save, queues `story.watch` (`{watch, visit}`,
  target the run) when the digest changed. The game's `applyStoryWatch` takes it for the visit the story is in only, and
  refreshes the union of the watch lists.
- **L1: a staged hunt** (`Core/RatwGameStoryWorld.cpp`, `Core/RatwHunt.cpp`). `hunt.stage` is a Phase 9-style world order
  (`WORLD_ORDERS`, target the run, the region's bounds in the payload as `story.tell`'s); the game takes the region from the
  story's own regions by id, checks the animal (`wild::speciesById`), 1 to 3, 1 to 1,440 minutes, and keeps
  `{region, cell, x1, y1, x2, y2, species, count, until}` in `held.hunts` (8 a run; `until` in world seconds). A wolf's
  `hunt` command now goes through `Game::storyHunt`: the first live story's unlapsed stage whose rectangle holds the wolf's
  tile gives `World::startHunt` its new `staged` list, added to what the ground gives (a ground that gives nothing that day
  still meets them; an unknown animal refused); the stage is used up only if the hunt starts. `tickStoryWorld` drops lapsed
  stages, `endStoryWorld` the rest (`UNDONE_AT_END`, guide §14), and `refreshStoryWatch` counts a story holding stages among
  `storiesHolding_`. The kill is the ordinary `hunted`, target the species. Approval (`world_checks`, a warning) names an
  animal whose `habitats` none of the region's ground has, read from the terrain (`story_runner.region_ground`,
  `world_store.load_ground`, the glyphs of `Data/Wild/animals.json`), or a region in an interior. `static_refs` gives the
  workbench `species` (with their ground), and the param type `species` is checked against `animals.json`
  (`story_program.animal_habitats`).
- **Workbench** (`Editor/`): the species picker (`useOptions('species')`, `REF_KIND`), `count in region` in words ("at
  least 3 of the residents in crew stand in tarn"; "of the story's wolves" when `who` is not given), a new count names no
  list (optional parameters are left out of a new world condition), and renaming a variable follows a count's `list`
  (`renameVariable`, `conditionVars`).
- **Guide and format**: `Docs/Story-Maker-guide.md` §7 "A beast for the hunt", §8 the two watched events, "Howls" and "How
  many residents stand in a region", §14 two rows, §15 three rows, §16 two rows; the reference regenerated
  (`story_guide.py --write`; `--check` passes; `hunt.stage` has an example). `Docs/story-programs.md` gains "Howls, crowds
  and the wild" and the `count in region` row. `RUNNER_VERSION` is `doc86-phase5`.
- **Sample** `story_v2_samples.HOWLS` ("Old Brown at the tarn", `v2-old-brown`, in `all_samples()`, so both twins run it): at
  nightfall the tarn-keeper howls and a bear is staged at the tarn; a wolf's howl counted; the ropes manned when three of the
  seven of the crew stand at the tarn, not when one of the three is dead there; a chorus of two calls nothing, one of three
  moves the story on (an exit with `minQuantity: 3`); the bear killed in the tarn's hunt ends the quest. It rehearses clean: nothing
  uncovered.
- **Tests**: `Tests/howl_tests.cpp` (`aResidentHowls`: a resident's howl heard as any howl, a wolf joins, one chorus of two, no
  event with no story listening, the rest refused; listening, a resident and two wolves give three `howled` of one chorus and
  one `chorus closed` of three begun by the resident; a dead resident refused), `Tests/hunt_tests.cpp` (`aStagedAnimal`: two
  staged ibex in the line-up, an unknown animal refused; `stagedHuntsThroughTheGame`, a game on the wilds: refusals (an
  unknown animal, a region not the story's, four), a hunt outside the region meets none and the stage waits, inside both
  ibex and the stage used up, the next hunt none, a lapse after a minute, the run's stop drops the rest, a stopped story
  stages nothing), `test_story_facts` (`ResidentsInARegion`: three of seven at the ropes, a dead one, unknown ones, none
  known, an empty list, the wolves as ever; the checks; the watch and `watch_now`; `Parity`: residents counted alike over
  made-up worlds while a trigger takes the one talked to off the list), `test_story_native` (`CrowdParity`: a story's and a
  world's list, dead, unknown, none known, emptied, unset; `npc.howl` and `hunt.stage` carried alike), `test_story_v2`
  (`Phase5Howls`: the tarn in both twins, a chorus of two, a bear killed away from the tarn, the events watched and mission
  evidence), `test_story_effects` (the order's payload, `npc.howl`'s, the species and count checks, undone at the end, no
  permission marks, the picker's animals), `test_story_facts_live` (a real server and runner on a throwaway Greyfen: the
  phase's watch names the list's first member, nothing sent again while the list stands, a trigger's `list add` sends
  `story.watch` once with both, applied, and the facts then have both), `test_story_catalogue`, `test_story_drafts` and
  `workbench.test.ts` (counts; Old Brown in words; a renamed list followed; a fresh count with no list).
- **Run**: `howl_tests`, `hunt_tests`, `story_world_tests`, `story_facts_tests`, `storyline_tests`, `dialogue_tests`,
  `save_compat_tests`; every story Python suite but the scene and siege live ones (with the local PostgreSQL: the tests' own
  throwaway `ratw_test_*` databases and `RATW_STORY_TEST_DSN` on a throwaway migrated one, dropped after), and
  `test_story_facts_live`; `story_guide.py --check`; the Editor's `workbench.test.ts`, `guide.test.ts` and `tsc --noEmit`.
  All OK.
- **Left or changed, and why**: quest objectives have no `minQuantity` (only exits do), so the spec's "a quest objective
  `chorus closed` with `minQuantity` 3" is tested as the sample's exit; an objective may count a chorus's howlers with
  `measure: quantity`, or a trigger read the `quantity` field. The `chorus closed` sentence leaves `{quantity}` out (no event
  sentence may name it; the help says it). `residentsHear` now leaves out the residents who howled (a resident must not turn to,
  or remark on, their own howl). A resident's howl does not give away sneaking wolves near it (that rule is a wolf's voice).
  `npc.howl` is refused for a resident offstage (away with nowhere to howl from) as well. The staged hunt's minutes are world
  seconds (`World::time`, which fast-forward speeds and a checkpoint keeps), as a howl's rest is; the spec's "the game keeps
  `stagedHunts_`" is the story's `held.hunts`, so a restart keeps them with the story and the run's end drops them with the
  rest it held. The hunt tests through the game are in `hunt_tests` (a Game on the wilds export) beside the world's. The
  watch sent again is a game order of its own (`story.watch`) rather than a revised `story.phase`, which would replace the
  visit's missions and words. A dead listed resident is not counted (a dead resident stands nowhere), which the spec did not
  say. An empty list counts 0 (known), not unknown.
- **After review** (2026-11-23). (1) The workbench asks for a count's residents list: `itemParts` shows the `list` slot of
  `count in region` once `who` is `residents` (it was hidden, so a residents count could be finished only in Source), and
  choosing the wolves again drops a stale `list` (`countWho`, from the condition picker). (2) A staged animal is met on
  ground too built up or bare for game of its own: `WildHere` gains `wild` (outdoors and in no town) and `startHunt` goes on
  with a story's `staged` there; a town or an interior still refuses. Approval also warns of a stage in a town
  (`world_checks`: the cell's region a market town of the last checkpoint, two or more of them, not a plain Country
  community of `communities.json`; `checkpoint_markets`, shared with the notice board check). (3) The Python ground map
  keeps a glyph's first ground, as the game does: `w` is water, not mud. (4) `chorus closed`'s help, and the guide, say that
  a resident who began the chorus is its actor, so an objective on it credits the wolves who joined only with `"credit":
  "present"`. (5) Any wolf's hunt in the region meets a stage, a passer-by's too: said in `hunt.stage`'s help, the guide and
  the format (kept: the spec says "a wolf"). (6) The sample's kill objective's `where` is the tarn's cell, not the region (a
  hunt's `hunted` is at the killer's tile in an arena wider than the tarn), and the guide's example and §7 say so;
  `test_story_v2` now has a kill away from the tarn in its cell count, one in another cell not. (7) `howl_tests` covers `npc.howl`
  refused for a resident down, away and in a fight. Tests: `hunt_tests` (a bare yard gives no game but meets a staged ibex,
  alone; indoors refused), `howl_tests`, `test_story_effects` (`w` is water; a stage in a town warned of, with a throwaway
  database), `workbench.test.ts` (the list asked for once residents are counted, dropped going back to wolves).
- **Going live** on `ratw_ridgemere3` needs no migration; the game server restarted from this branch (`npc.howl`, the howl
  events, `hunt.stage` and the staged hunt, `story.watch`), the story runner from this branch with the rebuilt
  `libratw_story.so` (a residents count needs both twins), the Editor released from it. The client needs no change.

## Phase 6 as built

**Phase 6 as built** (2026-11-23, branch `r3-nh-p6`, on `r3-nh-p5` at `b1ebf76e`). Engine spec item J9, tagged story items.
**No migration**: the names live in the game's checkpoint (`storyItemNames`), what a run must take back in its `held.tags`
(the story book), and the items themselves in the purses as any good. No new effect or event (86 effects, 216 events): `item.give`
gains three optional parameters, and there is one new parameter type (`tag`, 50 types).
- **The id.** A story's own item is `<base>^<hash8>.<tag>` (`letter^ab12cd34.watch_list`): `hash8` is the first 8 hex digits
  of the SHA-256 of the program's `id` (`story_program.tag_hash`, `tagged_item`), so two stories' marks never meet and a new
  version of the same story reads what an earlier one gave. An author writes `letter^watch_list` in a filter's `item` (an
  `in` list's values too) and in `carries`; `story_program.expand_tags` gives the program in the hashed form wherever it is
  read (`effective_phase`, which also stamps the phase with `_storyId`; `story_quests.index`; `watch_of`), and `own_items`
  does the same for a rehearsal's events and facts and the console's What if. Neither twin ever sees the short form, and an
  author never writes the hashed one (refused). The expansion is cached per program object, and a program with no tagged
  item is read as it is.
- **Matching (both twins).** `_gift_base` (Python) and `giftBase` (`RatwStoryTriggers.cpp`) split on `^` as on `~` and `@`:
  a filter that names no mark (`letter`) takes a story's own letter as a kind of letter; one naming a mark takes only that
  story's. `carries` reads the facts by key, so the hashed id counts only it, a plain `letter` its base. `story_triggers`
  accepts an item `base^tag` (`story_program.item_ok`); `check_world_facts` takes a hashed id in a wolf's `carries`.
- **Checks** (`story_program.validate_effect`, `_check_tags`): the tag is `[a-z0-9_]{1,32}`; `name` (one line of 60, no
  `#`, brackets or fills) and `keep` only with a `tag`; the base a plain item (no `~` or `@`); one `item^tag` is given with
  one name and one `keep` everywhere in the program; 64 kinds at most; an `item` with `^` anywhere but a rehearsal is the
  short form, and never in a `starts` (after review); one the program never gives is a warning (after review). Approval checks the base against the item catalogue as before (`reference_problems`, `world_checks` split on
  `^` too). The story words check (V2, `story_words.EFFECT_WORDS`) reads the name, as every text a wolf reads.
- **The order.** `story_runner.order_of` sends a tagged `item.give` as `item.give` (target the wolf) with the hashed `item`,
  `name`, `keep`, `run` and `visit`; an untagged one is the order it always was. `RUNNER_VERSION` is `doc86-phase6`.
- **The game** (`Core/RatwStoryArea.cpp`, `Core/RatwGameStoryWorld.cpp`). `applyStoryReward`'s `item.give` calls
  `Game::storyItemGiven` for a tagged id: it refuses a malformed id (a short form, a base with a make, a base not in the
  catalogue), a story not running, a name that is not one line of 60, and more than 4,000 names in the world. After the
  goods are made, `storyItemHeld` keeps the name in `storyItemNames_` and in `items::nameStoryItem` (the table
  `Society::itemName` reads, so the pack, trades, gifts and messages show it) and lists the id in the run's `held.tags`
  unless `keep`.
  `endStoryWorld` takes every unit of those out of every account (`Society::takeAll`: wolves, residents, tills, stalls,
  letters on the road) and off the ground (`World::takeBackItems`), forgets their names and tells each wolf who held one
  ("What the story ... gave you to carry is gone with it."). A kept one stays with its name, saved and loaded with the
  checkpoint (`storyTalkSave` / `storyTalkLoad`).
- **The sweep** (`Core/RatwItems.cpp/.h`). `items::baseOf`, `qualityApplies` and the facts' `baseItem` stop at `^`; the sites
  that split by hand (`tendLoans`, `makersScent`, `giftWorthThanks`) now call `baseOf`. `items::good` finds the base;
  `items::storyTagged` says whether an id is a story's own. Such an item is never a wearable or a blade, has no wear
  (`World::durabilityOf` 0, so the pack shows no condition), is no kind of its base for making, eating or paying
  (`Society::kindsHeld` returns only itself, asked by its own id; after review `Society::edible` and `nourishment` say no
  food, so no wolf or resident eats it and the staged wallet trims keep it), and is refused by a shop (`Society::stagedQuote`), a stall
  (`list`), a town project's hand-in and a loan: "That belongs to a story." Picking it up off the ground names it as its story does.
- **Facts.** `storyFactsJson` counts a carried story item under its own id when the watch names it (the watch's `items` is
  the expanded list), and under its base as well when the watch names that (changed after review: the spec's "under the
  base otherwise" let one story's watch hide the item from another's "carries letter").
- **Workbench** (`Editor/`): `storyItemsOf` / `tagsOf` read the program's tagged gives; the item picker offers "The story's
  own" first (as `letter^watch_list`, named, with kept or taken back); the new `tag` picker (`NamePicker` with a pattern);
  `namesOf` names a story's own item as `“The watch list” (the story's Letter)`. `item.give` reads "Give the acting wolf 1
  letter watch_list “The watch list” kept after the story".
- **Guide and format**: `Docs/Story-Maker-guide.md` §7 "Story items", §14 a row, §15 two rows, §16 a row; the reference
  regenerated (`story_guide.py --write`; `--check` passes); the catalogue's notes on `gift`, `gift again`, `item picked up`,
  `item dropped` and `carries` say how to name a story's own item. `Docs/story-programs.md` gains "A story's own items".
- **Sample** `story_v2_samples.LETTERS` ("The watch list", `v2-watch-list`, in `all_samples()`): the wolf who takes the
  clerk's quest is given the watch list (a letter, taken back at the end); a wolf carrying it is counted, one carrying none
  not; a plain letter given to the warden counts as a letter but does not do the quest, the watch list does; the warden's
  thanks is a writ kept after the story. It rehearses clean in both twins: nothing uncovered.
- **Tests**: `Tests/market_tests.cpp` (`baseOf` with `^`, `~`, `@`; no make or maker; found as its base, never worn or
  wearing down), `Tests/shops_tests.cpp` (`aStorysOwnItem`: no shop quotes or takes it, the plain hide still sells, not a
  kind of hide for making or paying, unnamed it shows as its base, named by its story's name, `takeAll`),
  `Tests/story_world_tests.cpp` (`phase6Items`: refusals, given and named and held, a kept writ not held, the pack's name
  with no wear, the facts under its own id and a plain letter as a letter, never lent, no shop, a gift to a resident is
  that id, one on the ground, the run's end takes it back from the wolf, the resident and the ground and tells the wolf, the
  kept writ's name through a save), `test_story_v2` (`Phase6Items`: the sample in both twins, a plain letter, another story's
  and a crafted one are not the list, the game's id is, carrying none, the expansion, what validation refuses, the name read
  by the words check), `test_story_native` (`StoryItemParity`: gift, `in` list, `item picked up` and `carries` alike in both
  twins over this story's, another story's, plain and crafted letters), `test_story_effects` (the order's payload, kept and
  not, the plain order unchanged, the catalogue's parameters), `workbench.test.ts` (117 programs; the watch list in words;
  the story's items and marks; a plain give hides the new parts).
- **Run**: `market_tests`, `shops_tests`, `story_world_tests` and the whole `ctest`; every story Python suite but the scene,
  siege and facts live ones (with the local PostgreSQL: the tests' own throwaway `ratw_test_*` databases and
  `RATW_STORY_TEST_DSN` on a throwaway migrated one, dropped after); `story_guide.py --check`; the Editor's tests and `tsc
  --noEmit`. All OK.
- **Left or changed, and why**: the spec's "the client pack test: the name shown" is the server's: the client already shows
  each pack row's `name` (and the stall and trade lists theirs), so it needs no change, and the pack is drawn into the DOM,
  which the client's unit tests do not have; `story_world_tests` checks the snapshot's pack row instead. The spec's
  `Game::storyItemNames_` is kept and also mirrored into the items table `Society::itemName` reads (a static function, which
  every name shown goes through). A loan of a story's own item is refused too (the spec did not say): a run's end taking back
  a lent item would charge the borrower for it. A story's own item is never a kind of its base for using up (making, eating,
  paying, building), which is what "can't be crafted from" needs; asked for by its own id it is there. Taking back reaches
  every account, so a letter on the road with one inside loses it; nothing else is touched. A kept item's name lasts as long
  as the world keeps it (4,000 names at most); a name is forgotten only when its run takes the item back.
- **After review** (the review of `f8bcb2fd`). (1) `carries` counts a story's own item under its own id and, independently,
  under its base: the watch is every live story's, so story A watching `letter^watch_list` no longer makes the list stop
  counting as a letter for story B (`story_world_tests` now expects `letter == 2` for Ash's two lists). (2) The workbench
  offers "The story's own" items only where they can be named: an event's filter (a slot with no parameter) and a parameter
  marked `"own": true` in the catalogue (`carries`'s item); never `item.give`'s own item, `item.take`, `item.place` or
  `story.area.item`, which refuse `^` (`ownItemsHere`, `useOptions(type, own)`). `storyItemsOf` now walks the whole program
  (a rehearsal's events left out), as `_check_tags` does. (3) `expand_tags` and `expanded_phase` read the cache once, under
  a lock (`_expanded` returns the expanded program and its phases together), and the oldest program goes one at a time past
  `EXPANDED_MOST` (64), so a thread letting a program go between two reads no longer raises `KeyError`. A program is read
  as it was when first expanded (said in the code: the runner reads a fresh dict each time). (4) A tagged give refused for a
  full pack leaves no name behind: `storyItemGiven` only checks, `storyItemHeld` names after the goods are made. (5)
  Validation refuses `^` in a `starts` ("named whole"), and `program_warnings` warns of an `item^tag` the program never
  gives (`own_items_named` against `story_items`, so that is no longer only the tests'): its hash is this program's, so
  only what an earlier version of the story gave could match it (a warning, not a refusal, for that reason; the parity
  tests' small programs also name one they never give). (6) The guide says a plain filter takes a
  story's own item as its base only on `gift` and `gift again`; `item picked up` and `item dropped` match exactly. (7) A
  story's own food is never eaten (`edible`, `nourishment`; `bestFood`, `eatFirst`, `hasFood`) and the staged wallet trims
  neither drop it as food nor cap it as another good. (8) A story's name beginning with an article reads in the world text
  as "You pick up the watch list.", "You give the warden the watch list.", "You receive 2 of the watch list."
  (`items::counted`, `items::theItem`; a catalogue item's words are unchanged). Kept: (9) the client pack test (as above),
  and a chapter cannot name an earlier chapter's item (the hash is per program; the guide says so). Tests:
  `story_world_tests` (the facts as a letter too, the gift said as "the watch list", a story's bread not edible, a give to a
  full pack refused and unnamed), `test_story_v2` (`Phase6Items`: "named whole" refused, an `in` list of the story's own accepted, one never given warned of
  at its path;
  the cache let go between reads, the oldest evicted, eight programs read from six threads at once), `workbench.test.ts`
  (where the story's items are offered; one deep in a branch found, a rehearsal's not).
- **Going live** on `ratw_ridgemere3` needs no migration; the game server restarted from this branch, the story runner from
  this branch with the rebuilt `libratw_story.so` (the gift filter's split is in both twins), the Editor released from it.
  The client needs no change.

## Phase 7 as built

**Phase 7 as built** (2026-11-24, branch `r3-nh-p7`, on `r3-nh-p6` at `8af298f5`). Engine spec items C10 (foes), C10t (tuning
the foes by simulation) and T3 (strangers in scenes). **No migration**: a foe lives in the world while it lasts and in the
story's `held.foes` (the story book), so a restart makes it again; nothing new is written to the world database. Three new
effects (89 in all: `foe.add`, `foe.leave`, `foe.outlaw`, a new effect group Foes), three new events (219: `foe downed`,
`foe defeated`, `foe fled`, group fights; `foe downed` and `foe defeated` mission events, 74), one new parameter type
(`foe`, 51 types), a new data file (`Data/Storylines/foes.json`), and `scene failed` gains `item` (why, where it is known).
- **C10: foes in the game** (`Core/RatwStoryFoes.cpp/.h`, new). `World::addStoryFoe` makes a story camp `foe_<8 hex of the
  run>_<foe>` (`Game::storyFoeCampId`, FNV-1a of the run id, so the same after a restart) in `roads_.camps`, a leader
  `road:<camp>:0` and 0 to 8 followers `road:<camp>:1..` ("one of <name>'s band", from `foes.json`), road folk of a new
  kind `foe`, in person (never offstage) for their minutes; `like` gives the leader the resident's looks (and words, with
  no description). `World::outlawResident` makes a resident the leader of such a camp for 1 to 720 minutes with 0 to 6
  followers, refused for one protected, dead, down, in a fight, away with a party (or nowhere), or a foe already (the
  Game adds held by another story). `World::campOf` returns the camp for a foe's folk and for a resident made a foe, so
  `startBattle` takes the camp's path: no assault, no warrant; and against a story camp the Watch on duty in the cell
  (`guardOnDuty`) comes in on the wolves' side. `temperamentOf` makes a foe aggressive with its kind's skill and flee;
  `hurtFighter` scales a blow against one by 20 / its hp (`foes::hurtScale`: `RoadFolk.hp` was never read by the fight, so
  hp is a foe's toughness against blows, 20 an ordinary bandit's). `downFighter` raises `foe downed` for a follower
  (never `bandit falls`) and, for the leader (`foeLeader`: `:0`, or the resident), `foe defeated` once (`quantity` the
  wolves who fought it, in this fight and earlier ones), and breaks the band (each runs); a foe down never dies of its
  wounds (it is carried off after the fight), except a resident made a foe with `lethal`. `leaveArena` marks a leader who
  breaks off (`fled`) and the band runs with it. `finishBattle` for a story camp (`foeFightEnds`) robs nobody and clears
  no camp, remembers who fought, carries the downed and the fled of a beaten or fled foe off (`removeRoadFolk`), gets a
  beaten resident up where they lie (`standUp`, 25 health) or, `lethal`, kills them (`death` follows). `tendStoryFoes`
  (from `World::tick`, every half second) ends a foe out of time (`foe fled`, item `time`), lets a beaten one go (no
  event) and a fled one (`foe fled`, item `fled`), never while one of them is in a fight (so no fight is left open), keeps
  the band by its leader, starts a waiting foe's fight with a wolf within 6 tiles of its ground (and walks it back to that
  ground) and a hunting one's with the nearest wolf within 12 (a fight starts from up to 12 tiles: doc 40), never with a
  wolf down, in a fight or just out of one. A story camp is skipped by the roads' daily round (hunger, scattering, the
  strength cap), never holds a road against a new camp gathering, never makes bandits in person (`tendCamp`), is not
  loaded from a checkpoint (`RatwCheckpoint.cpp`), and Danger Sense feels a foe as it feels a bandit; an injury from one is
  "a foe"'s. `World::endStoryFoe(camp, why, evenInAFight)` lets one go (with `foe fled` when `why` is given).
- **C10: the story's side** (`Core/RatwGameStoryWorld.cpp`). `foe.add`, `foe.leave` and `foe.outlaw` are world orders
  (`storyWorldOrder`; `story_runner.WORLD_ORDERS`: target the foe's id, or the resident). `applyStoryWorld` checks the
  minutes (1 to 1,440; a resident 720) and band (0 to 8; 6), 8 foes a story at once, and keeps each in `held.foes` (the
  camp: what makes it again) once the world has made it (`storyFoeFrom`; the name through `fillStoryNames`); `foe.leave`
  is refused while the foe fights. `tickStoryWorld` makes one again after a restart while its time lasts, drops one the
  world let go, and holds a resident beaten and not killed (`held.npcs`, as `npc.hold`: released at the run's end);
  `endStoryWorld` takes every foe away, even mid-fight, with no event; `refreshStoryWatch` counts a story holding foes.
- **C10: checks** (`tools/story_program.py`). The type `foe` is `[a-z0-9][a-z0-9_-]{2,39}`; one foe id is one foe in the
  whole program (one name and one strength wherever it is brought: `_check_foes`); `foe.leave` names a foe the program
  brings; `foe.outlaw` needs a builder (`needsBuilder`, `BUILDER_EFFECTS`); `foe.add` and `foe.outlaw` are undone at the
  end (`UNDONE_AT_END`); `program_warnings` warns of a foe event read for a foe the program never brings nor a resident
  it makes one (`foes_of`, `_match_sites`). Approval checks `foe.add`'s tile (`check_tile`) and its `like` resident, and
  `foe.outlaw`'s resident, as any. `band` is an operand (C1a's band from the wolves present). The story words check
  reads a foe's name and looks (`story_words.EFFECT_WORDS`). Rehearsal facts may be foe events (no change was needed).
  No change to either twin: the foe events are ordinary events, matched on `target`, `item`, `detail` and `quantity`
  alike (`test_story_native.FoeParity`).
- **T3: strangers in scenes**. A scene's cast entry may be `{"id": "<role>", "visitor": "<visitor id>"}` beside `npc` and
  `selector` (exactly one: `story_scenes.validate`, and the game's `validScene`); the program must bring that stranger
  (`visitor.add`: `_check_strangers` refuses one it never brings; `program_warnings` warns of one brought only in a later
  phase than a `scene.play` of it). `Game::startCinematic` resolves the role to the stranger the run holds
  (`held.visitors`), in their own panel in the dialogue window with their `like` face, as a resident speaker; one not in
  the world (never brought, sent away, gone) fails the scene: `scene failed` with `item` "a stranger is not here" (the
  detail stays the cue, so a scene's exits still match). `story.tell from_visitor` is not built (spec §0.5).
- **C10t: the tools.** `Tests/foe_sim.cpp` (new CMake target `foe_sim`, not a test; ctest `foe_sim_smoke` runs `foe_sim
  foes 20`) runs real fights on the real rules in a World of their own each (the demo world's yard): the foe (or a
  resident of the demo world made one) with its band, the wolves a few strides off, the first setting on the leader and
  the rest joining; it includes `level_sim.cpp` (its `main` renamed) for level_sim's wolves, side syntax and play,
  Gifts included. Rows read `foe <strength> band <n|scaled> vs <wolves> [guards <g>] [outlaw]` (`3*plain@seasoned+`
  for three; the spec's `plain@seasoned+` is read as level_sim's `plain+@seasoned`); guards are the Watch's skill (70),
  iron sword and leather kit, played as the wolves are (the sim has no town to post them from). Per row: the wolves' win
  % with its Wilson 95% interval, wolves downed (per fight and per win) and hurt, fight seconds (median, p90),
  leader-first %, the band broken within 5 s %, foe fled %, `foe defeated` once %, and for the bar PASS or MISS; suites
  `foes` (the bar), `band`, `guards`, `quick`, `outlaw`, `scaled`, `all`; `row "<row>" [fights]`; `SIM_FOES` (a trial
  table: read by `foes::` in the game too, so a pass never edits `Data/`), `SIM_SEED`, `SIM_THREADS`; 2,000 fights a row,
  500 past 8 wolves. `world_check --foes K[:strength[:band[:stance]]]` brings K foes through `World::addStoryFoe` to the K
  outdoor cells with the most residents at the start and sends the players after them (each walks to its foe and, in a
  fight, plays battle_play's turn), and reports the fights a game hour, foes beaten and fled, wolves downed and the fights
  left open once their foe was gone (`--foes-crowd-only` sends the same crowds to the same places with no foe, the
  control the load rows are read against); `sim_fast_tests` runs `--players 2 --foes 1` on its small town (and refuses a foe
  of no such strength).
- **C10t: the passes** (records in `artifacts/combat/foes/pass1.txt` to `pass4.txt`, the trial tables `pass2.json` to
  `pass4.json`). The wolves' win %, the bar's rows (a `*` inside its band):

  | Row | Band | Pass 1 | Pass 2 | Pass 3 | Pass 4 |
  |---|---|---|---|---|---|
  | follower 1 v 1, new bare | 50–65 | 99.3 | 16.9 | 71.0 | 37.3 |
  | follower 1 v 1, seasoned armed | 75–90 | 100 | 84.5* | 99.2 | 94.8 |
  | tough band 0, one seasoned | 25–40 | 99.9 | 35.9* | 35.9* | 35.9* |
  | tough band 2, three seasoned | 85–95 | 100 | 31.1 | 62.8 | 43.0 |
  | tough band 2, three new bare | 50–70 | 88.2 | 0.1 | 0.3 | 0.1 |
  | fierce band 4, three seasoned | 50–65 | 18.4 | 0.0 | 0.0 | 0.0 |
  | fierce band 4, five seasoned | 80–92 | 100 | 74.9 | 98.0 | 86.4* |
  | the same + 2 guards | +8–15 points | +0 | +25 | +2 | +14* |
  | fierce band 4, two seasoned + a Quickened veteran | ≤ 85 | 100 | 92.1 | 99.9 | 97.0 |
  | champion band 8, five seasoned | 25–45 | 33.7* | 0.1 | 0.1 | 0.0 |
  | champion band 8, eight seasoned + 2 guards | 70–85 | 100 | 99.6 | 100 | 100 |
  | champion band 0, one Quickened veteran | ≤ 35 | 62.8 | 16.9* | 16.9* | 16.9* |
  | fierce scaled, 4 / 8 / 12 / 16 wolves | 60–85 | 100 all | 94 / 100s | 100 all | 98.6 / 100s |
  | outlaw tough, three seasoned (never killed: 100%) | 85–95 | 100 | 33.5 | 65.4 | 43.4 |
  | leader down: band broke within 5 s, `foe defeated` once | ≥ 95%, 100% | 100, 100 | 100, 100 | 100, 100 | 100, 100 |
  | fight length median: tough, fierce, champion | 20–60, 40–120, 90–240 s | 100, 79*, 117* | 147, 146, 122* | 147, 142, 118* | 147, 152, 117* |

  The tables (skill / strength / hp / flee): pass 1 the spec's starting table (follower 52/55/16/35, tough 60/60/20/25,
  fierce 70/68/24/15, champion 80/75/30/0); pass 2 hp, one knob a strength (follower 42, tough 48, fierce 60, champion
  40); pass 3 flee (follower 35 to 55, fierce 15 to 30; tough and champion unchanged); pass 4 hp again (follower 52,
  fierce 70). Skill came first by the spec's order, but probes at 400 fights showed it all but inert (a tough alone
  against one seasoned armed wolf: 99.9% at skill 60, 99.2% at 100 and 130 with hp 20), so pass 2 took hp (each pass's
  record says why it changed what it did). **The bar was not passed after pass 4, so no pass 5 was run and nothing was
  accepted** (the stop rule): `Data/Storylines/foes.json` keeps the starting table with `"tuned": ""`, and the closest
  table is pass 4's (`artifacts/combat/foes/pass4.json`: 4 of the 17 rows, both leader rules and the champion's length).
  **Why it cannot pass by `foes.json` alone** (for the user, spec C10t rule 5): the fight rules decide most rows by
  numbers, not by a foe's strength. A side that outnumbers the other wins nearly always (the band suite: fierce band 2
  against one seasoned wolf 0%, against three 100%; band 4 against three 0–18%, against five 100%; band 8 against five
  58%, against eight 100%), and a wolf downed gets up again and fights on (doc 38) while a foe does not, so every crowd row
  (scaled 4–16, eight with guards) sits at 94–100% whatever the table, and the rows where the foes have the numbers sit
  near 0. The two one-on-one follower rows want 15–35 points between a new bare wolf and a seasoned armed one; the game
  gives 50–65 points at every follower table tried (`pass2.txt`'s probes), so one of the two always misses. And the bar's
  party rows (tough band 2 against three, 85–95%) want followers far weaker than the one-on-one rows want them. The
  fight lengths are the fight's pace (turns and bars, doc 33), 100–150 s for every tough table. Changing these is a fight
  rule (`RatwBattle.cpp`: the outnumbering, the get-up, the foes' targeting), which the spec puts out of this phase's
  scope; the user decides (the bar, the closest table, or a rule change).
- **C10t: the load pass** (`artifacts/combat/foes/load.txt`): run on the closest table although the 1 v 1 and party rows
  did not pass (to have the load rows before the user decides), on an export of Ridgemere 3 (`world_build.py export`,
  build 82, read only), six game hours from midnight, `--no-births --no-talk-ledger --fast --no-check`, 8 threads, each
  beside the same run without foes: tick time over the six hours (ms), the players spread over the world (as
  `world_check` places them), the same crowds the foes draw with no foe (`--foes-crowd-only`, added for this), and with
  the foes:

  | Players, foes | Spread: mean / p99 | Crowds, no foe: mean / p99 | With foes: mean / p99 | Over the crowds (p99) | Fights; beaten; downed; left open |
  |---|---|---|---|---|---|
  | 20, `2:fierce:8:hunts` | 0.93 / 2.25 | 3.26 / 9.46 | 3.56 / 9.90 | +4.6% | 7; 0; 26; 0 |
  | 100, `6:fierce:8:hunts` | 4.57 / 8.09 | 12.45 / 23.67 | 13.79 / 26.70 | +12.8% | 12; 1; 100; 0 |
  | 250, `6:fierce:8:hunts` | 10.98 / 15.62 | 35.90 / 65.11 | 39.18 / 68.04 | +4.5% | 5; 3; 79; 0 |

  No fight was ever left open once its foe was gone. The foes themselves (their tending and their fights) cost 4.5% to
  12.8% of p99 over the same crowds without them: inside the bar's 10% at 20 and 250 players, just over it at 100 (mean
  +10.7%). Against the spread run, the bar's literal comparison, every row is far over (p99 4.4 times at 20, 3.3 at 100,
  4.4 at 250), but that is the crowding: a profile of the 100-player foes run (`perf`) spends 29% in `World::observe`
  and 10% in `World::snapshot` (and 19% copying for them), each player's sight and view of a crowd of forty, not the
  foes or their fights. At 250 the crowds alone are over the server's 50 ms tick (p99 65 ms), foes or not: forty wolves
  at one place is the cost to watch, not the foe. The 20-player row is well inside the tick budget (p99 9.9 ms). The
  wolves here (battle_play's scripted bites, new wolves) mostly lose to a fierce foe with eight followers: these rows
  measure the cost, not who wins.
- **Workbench and console** (`Editor/`): the `foe` picker (`foesOf`, a `NamePicker` with the id's pattern), a new foe's
  id of the story's own (`foe-<n>`) and a foe sent away the first the story brings (`newEffect`), a foe named by the
  name the story gives (`namesOf`: "The Grey Captain (captain)"); the scene editor's "Cast from" gains "A stranger the
  story brings" (`CastRole.visitor`), and the preview names them.
- **Guide and format**: `Docs/Story-Maker-guide.md` §8 "Foes" (with the population rule: foes are never residents and
  never stay) and "Strangers in scenes", §14 two rows, §15 four rows, §16 two rows; the reference regenerated
  (`story_guide.py --write`; `--check` passes; `foe.add`, `foe.leave` and `foe.outlaw` have examples).
  `Docs/story-programs.md` gains "Foes and strangers". `RUNNER_VERSION` is `doc86-phase7`.
- **Sample** `story_v2_samples.FOES` ("The Grey Captain at the Pass", `v2-grey-pass`, in `all_samples()`, so both twins run
  it): the Captain, a stranger looking like rm111, is brought to the parley and cast in its scene; as it ends (or fails)
  he holds the Pass as a champion foe with a band of min(8, active / 2) and the turncoat rm113 is made a foe beside him;
  two of the band brought down are counted, the turncoat beaten notes it, the Captain beaten by three wins the Pass; a
  second case, the Captain fled, loses it. It rehearses clean in both twins: nothing uncovered.
- **Tests**: `Tests/battle_tests.cpp` (`foefights::storyFoes`, in Greyfen with the Watch on patrol: refusals; the band
  of the foe's camp, as strong as `foes.json` says; set on, no assault and no warrant, the guard on duty on the wolf's
  side, the band in it; `foe defeated` once with the story's id, the name and the one wolf who fought, followers `foe
  downed` and never `bandit falls`, no `death`, and once over the foe, band and camp gone and nobody robbed; time out is
  `foe fled` (time) and they are gone; a hunting foe comes for a wolf, is never ended by time mid-fight and is sent away
  mid-fight with no event, the fight ending; a resident made a foe: a protected one and one away refused, a tough one who
  breaks off is `foe fled` (fled) and herself again, a champion downed not killed and up again after, `lethal` killed
  with `death`, the dead refused), `Tests/story_world_tests.cpp` (`phase7Foes`: through the game, the orders' refusals,
  the Captain brought looking like u2 and held, sent away and back, u1 made a foe; both made again after a restart; u1
  beaten and held by the story; the story's end takes the Captain away with no event and lets u1 go),
  `Tests/storyline_tests.cpp` (`strangerInAScene`: the Captain speaks in his own panel with u2's face; a stranger never
  brought, and one sent away, fail the scene with "a stranger is not here"), `Tests/sim_fast_tests.cpp` (`world_check
  --players 2 --foes 1` completes with no fight left open; a foe of no such strength refused), ctest `foe_sim_smoke`,
  `test_story_effects` (the orders and their payloads, the band from a value, the checks, the builder flag, undone at
  the end, the warning on a foe event; the world's references with a throwaway database), `test_story_scenes`
  (`StrangersInScenes`: the cast form; the stranger the story brings, and brought only later warned of),
  `test_story_v2` (`Phase7Foes`: the Pass in both twins; another foe's beating does not win it; what validation refuses),
  `test_story_native` (`FoeParity`: the foe events and the band, alike in both twins), `test_story_catalogue`,
  `test_story_drafts` (the counts), `workbench.test.ts` (118 programs; the Captain in words; a new foe and one sent away
  pass the checks).
- **Run**: the whole `ctest` (122 tests: all pass; `talk_scorecard_quick` failed once under `-j10` and passes alone, a timing test, untouched here; `pg_client` skipped as ever), with `battle_tests`, `story_world_tests`, `storyline_tests`, `sim_fast_tests` and `foe_sim_smoke` among them; every story Python suite but the scene, siege and facts live ones (with the local PostgreSQL: the tests' own throwaway `ratw_test_*` databases and `RATW_STORY_TEST_DSN` on a throwaway migrated one, dropped after); `story_guide.py --check`; the Editor's tests (`npm test`: 78 and 151, `workbench.test.ts` and `guide.test.ts` among them) and `tsc --noEmit`; a scratch Ridgemere 3 server from this branch (`tools/scratch.sh ridgemere3`: restores the save and listens; stopped after). All OK.
- **Left or changed, and why**: (1) The bar is not passed (above); `foes.json` keeps the starting table, the closest
  table is pass 4's, and the user decides. (2) The console's "Foes" panel was left unbuilt here and built after review
  (below). (3) The spec's
  `World::storyOutlaws_` is the same story-foe registry (`storyFoes_`, by camp) with the resident as its leader; the
  spec's `BanditCamp.story` flag is the camp id's prefix `foe_` (`World::storyFoeCamp`), which needs no change to the
  checkpoint's camp format. (4) A foe is in person for its minutes (never offstage, as a stranger is), not made in person
  only when someone is near as a bandit camp is; it is not saved, and a restart makes it again with its whole band. (5)
  `hp` was never read by the fight for road folk, so it became a foe's toughness against blows (20 / hp). (6) A waiting
  foe's "aggro" is 6 tiles of the ground it holds, a hunting one's 12 tiles of itself; both start the fight at once (a
  fight starts from 12 tiles), rather than walking up first. A resident made a foe never seeks a fight (held where
  they were made one since the review; set on, they fight). (7) The `foe defeated` quantity counts the wolves who fought the foe in all its fights,
  not just the last. (8) A beaten foe and its band are let go once the fight is over (the band runs at the leader's fall
  and leaves the fight then), not when out of sight. (9) The talk test for a stranger in a scene is in
  `storyline_tests.cpp`, where the scene-panel cases are (`talk_tests.cpp` has none). (10) `SIM_FOES` is read by the game
  too (a test hook, unset in play). (11) The load pass ran though its condition (the 1 v 1 and party rows passing) was
  not met, to give the user the numbers.
- **Going live** on `ratw_ridgemere3` needs no migration; the game server restarted from this branch (the foes, the
  battle changes, the stranger in a scene), the story runner from this branch (`WORLD_ORDERS`, the checks; the native
  library needs no change but is rebuilt as usual), the Editor released from it. The client needs no change: a foe is a
  road folk entity with a name and looks, and a stranger's panel is a resident speaker's.

**Phase 7 after review** (2026-11-24, on `9720bcd6`). The review found three bugs and five gaps; all are fixed, with tests.
- **A leader of the story's own who breaks off is `foe fled` (fled).** `tendStoryFoes` tested the leader gone before the
  flight, and `foeFightEnds` carries a fled leader off, so a `foe.add` leader who ran ended with no event and as "" (only
  a resident made a foe, never carried off, worked; foe_sim's "fled %" read 0.0% on every `foe.add` row of passes 1 to 4,
  so pass 3's flee change showed nothing on those rows: the records stand as run). Now the flight is tested first.
  `battle_tests` (`foefights::afterReview`): the Captain breaks off at the arena's edge, `foe fled` (fled), ended "fled".
- **A story's foe asks no purse.** `startBattle` opened a bandit encounter for a story camp, which only the foe's end
  closed, so after any fight it survived (a wolf downed or fled, a draw) the wolf was offered "pay" and `payBandits` took
  half their purse into a loot account nobody drains. Now no encounter is opened for a story camp, and `banditDemand` and
  `payBandits` refuse one. `battle_tests`: a draw with the Warden, nothing asked, nothing paid, no loot account.
- **A story's foe is no bandit camp on the roads.** It robbed passing wagons (`caravanEntered`'s raids), had bounties
  posted on it, was whittled down and cleared by the Watch's bounty patrols, cleared by a settled bounty
  (`completeContract`), and called for escorts (`RatwTradeShow.cpp`). All four skip a story camp, as do the guard's
  "bandits out on the roads" (`here.camps`) and the talk of bandits (`RatwAmbient.cpp`). `trade_show_tests`
  (`aStoryFoeRobsNoWagon`): a strong, starving story camp in the road robs none of six wagons, no bounty, no loot account;
  a bounty on it anyway is never patrolled, and settling it clears nothing.
- **A resident made a foe keeps their ground** (`outlawResident`: `sceneHold` where they stand, "standing against the
  wolves", their pace kept; held again after a fight where they stand; let go when the foe ends), so the band stays at
  their back, instead of a shopkeeper going home to bed with their followers left in the shop. `battle_tests`: Wren made
  a foe at 17:24 keeps her ground for half an hour with her follower beside her, and is let go when her time is up.
- **No other story takes hold of a resident made a foe.** `heldByAnotherStory` counts a foe of another run, so another
  story's `npc.hold`, `npc.follow`, `npc.walk` and `foe.outlaw` are refused; and `whyNotJoin` refuses a party ("They are
  set against the wolves just now."): the story's own `npc.follow` and a wolf asking them along alike.
  `story_world_tests` (`phase7Foes`).
- **A story that ends mid-fight lets the resident go.** `dropStoryFoe` takes a resident made a foe out of the fight they
  are in (as fled), stands them up if they are down (in the fight's last seconds after the leader's fall too, when no foe
  is left to get them up), and lets go of the hold. `tendDowned` gets up anyone left lying with the foes' "until the
  fight's end" mark (1e6 s) and no foe and no fight (a restart in that window). `battle_tests`: the story ends while Wren
  fights, and while she lies beaten in the fight's last seconds.
- **A foe event is the run's.** Its `target` is the story's id, which two runs (of one quest, or two stories) may share.
  The game puts the run's mark in every foe event's `coins` (`foes::runMark`: FNV-1a of the run id, the 8 hex digits of
  the foe's camp id; `coins` was 0 on these events and is not an event field a trigger may read), and the runner gives a
  foe event only to the run whose mark it carries (`story_program.foe_mark`, in the event loop, as region events are
  given by their run; "why has this one not fired?" says "another story's foe"; a made-up one in What if is this run's).
  The twins need no change; the console's timeline does not show the mark. `test_story_program`
  (`test_foe_events_go_to_the_run_whose_foe_it_was`), `battle_tests`, `story_world_tests`.
- **The Foes panel** (C10's Workbench item). The game keeps with each foe in `held.foes` the followers standing (`left`),
  whether it fights (`fighting`) and when its time runs out in real seconds (`endsAt`), written when they change
  (`tickStoryWorld`). The story book is saved in `game.storylines`, which the runner reads (`story_runner.foes_now`,
  `status`'s `foes`). The run's Orders tab lists them ("The Grey Captain (captain), champion, 2 of 4 followers standing,
  about 30 minutes left · fighting now": `campaigns.foeLine`) with **Send away…**: a new command `foe` (`{do: "leave",
  foe}`, a foe the program brings or a resident it makes one) that sends the game `foe.leave` (a resident's camp named
  `npc-<resident>`). The game refuses it while the foe fights. `test_story_steering` (`test_the_foes_panel_and_send_away`),
  `campaigns.test.ts`.
- **The nits fixed**: a foe's id never begins `npc-` (a resident made a foe's camp: `FOE_ID`, and the game); the game
  refuses a follower's strength in a direct `foe.add` order (`foes::leaderKind`); a foe a restart cannot make again (its
  time ran out meanwhile; its resident dead, down or away; its ground gone) is `foe fled` (time), never dropped silently;
  a foe is "a foe" to whoever looks at it (never "bandit") and wants "a fight, not your conversation"; the workbench reads
  "“The Grey Captain” (captain), a champion foe with a band of 4" (no name twice), and both foe sentences say "a band of
  N" (never "1 followers"; the guide's reference regenerated). **Not changed**: the stranger-id nit (a scene cast's
  `visitor` and `visitor.add`'s id are checked by the same pattern in `story_program`, `[a-z0-9][a-z0-9_-]{2,39}`; only the
  game's `validScene` is looser, and it refuses nothing a program may hold). The spec deviations the review lists stay as
  disclosed above. The guide's §8 Foes says all of this.
- **Run**: the whole `ctest` (122: all pass; `talk_scorecard_quick` failed once under `-j8` and passes alone, the timing
  test noted above; `pg_client` skipped as ever); every story Python suite but the scene, siege and facts live ones, with
  `RATW_STORY_TEST_DSN` on a throwaway migrated database (dropped after); `story_guide.py --check`; the Editor's tests
  (`npm test`) and `tsc --noEmit`. Going live is as above, with the Editor released for the Foes panel and the story
  runner restarted for the foe events' marks and the `foe` command.

## Phase 8 as built

**Phase 8 as built** (2026-11-24, branch `r3-nh-p8`, on `r3-nh-p7` at `4e4357b7`). Engine spec items J2 (realm variables),
C14 (`npc.post`, the crown's change) and L4 (`npc.sentence`, a hold that outlives the run). **Two migrations**, as the spec
numbers them: **0080** (`dm.story_posts`, D5's post list) and **0081** (`dm.story_realm_vars`); both applied to a
throwaway migrated database, never to `ratw_ridgemere3`. Two new effects (91 in all: `npc.post`, `npc.sentence`, group
Residents, both `needsBuilder`), two new events (221 in the catalogue's list, 218 a story may name: `sentence begun`,
`sentence served`, group Town life), one new parameter type (`post`), a fifth variable scope (`realm`).
- **J2: the scope, in both twins** (`tools/story_triggers.py`, `Core/RatwStoryTriggers.cpp`). `SCOPES` gains `realm`;
  `PERSONAL` (a wolf's or party's own) holds it, and a new `OWN = ('player', 'realm')` is what one value per wolf means:
  `var_store` and `Dispatch2::store`/`writable` keep it in `counters.realm[wolf]` as `player` is kept, so it is read `of`
  `$actor` or `$each`, set, added to, listed, timed, compared and tested for each wolf like a player value; `rank` and
  `tally` (and their checks) take a number, text or switch of either own scope and read the store of the variable's own
  scope (a tally's `sum` and `where` each their own); a mirror's `key` may be a realm text; N2's told values, the journal's
  trackers (`story_quests.trackers`, `talk_map`, `talk_unsaid`) and the timeline's before and after (`_values`, `_diff`)
  read it. The native request already carries the whole `counters`, so no widening was needed. `test_story_native`
  (`RealmParity`: added to, compared of the actor and of each, ranked, tallied with a player text's `where`, in a For each,
  a mirror keyed by a realm text, a wolf not in the story).
- **J2: the runner** (`tools/story_runner.py`). `realm_load` reads the cohort's rows for the declared names into
  `counters.realm` the first time (at `launch`, and after a move onto a new version, which drops `realmSeen`), then at each
  turn only the rows whose `updated_at` is past the run's cursor (`state.realmSeen`, epoch seconds), read again from
  `REALM_OVERLAP` (120 s) before it so a row another transaction stamped earlier but committed later is not missed (a row
  read twice is harmless: the run's own writes were saved at its last turn's end); a row of another type is not read; a
  wolf who left is let go of. `realm_save` writes only the wolf-name pairs that changed against what was loaded, stamped
  `clock_timestamp()`. `realm_join` reads a wolf's rows when they join: `story_quests.add_wolf` calls the hooks in
  `story_quests.JOINED`, which `tick_run` sets for its run and `Steer.take` for a command (after review). They ride
  on `world_load`/`world_save` (which now return a `_Loaded`: the world snapshot with the realm one as `.realm`), so every
  path that reads or steers a run (a tick, a command, What if, Explain) has the values. `realm_clash` (from
  `references()`): needs 0081, and refuses a declaration of another type than the wolves keep a name as, or than another
  live story of the world declares it. `_carried` never carries realm (nothing to carry). The console: ops `realm-vars`
  (`{name?, wolf?}`: the values newest first, 200 at most, and the names the world's stories declare with who declares
  them) and `realm-var` (`{name, wolf, value, reason}`, the owner or "may steer campaigns", audited `story.realm-var`).
  A rehearsal case may give `realm`: `{wolf: {name: value}}`, what each wolf brings with them from an earlier act
  (checked against the declarations; the workbench's rename follows it).
- **C14: the crown's change**. `Society::swapPost` (`Core/RatwCareers.cpp`; `World::postResident`): unlike `appoint` it takes
  a held post (the holder becomes `lastHolder`, the post never vacant), takes the resident out of any post they held (vacant
  since today, `lastHolder` them), keeps their skill at least what an appointment gives, and notes `succession` (actor the
  new holder, target the one before, detail `<title>: by the story`); refused for a `guard` post ("The watch chooses its
  own"), no such post, the dead or no resident; a post they hold already is no change (`held`, so an order sent again after
  a restart is no refusal). The game's `npc.post` order (`RatwGame.cpp`, payload `{post}`; the runner's single-key
  `action()`), recorded with the economy and saved soon. Lasting. Approval: the post in `dm.story_posts` and not the
  Watch's; `refs().posts` (by title, with the holder; the guard posts left out) for the workbench's new `post` picker.
  `Tests/careers_tests.cpp` (`theCrownsChange`: Dagfinn's "reigning at the Great Palace" to Ottilie, Dagfinn with no post,
  Ottilie's books vacant since the day, a succession note, no money moved, the guard's post refused).
- **0080: `dm.story_posts`** is a **view**, not the table D5 suggested: the last checkpoint's `society.careers.positions`
  (`id`, `holder`, `lastHolder`) joined to each post's founder (`live.npcs`, or a profession slot's character) for its title
  (the founder's work label: `Society::buildPositions`) and role. A view is "refreshed with every checkpoint" by being one,
  needs no writer (the game role writes no `dm` table), and never depends on a live server. Read only against Ridgemere 3
  (the view's query, not the view: nothing was created there) it lists 705 posts, 14 of the Watch, every one titled;
  "reigning at the Great Palace" is `job:rm_gunnar_harrowgate` (its founder dead), held by `rmx_dagfinn_thornham`.
- **L4: a sentence** (`Core/RatwGameStoryWorld.cpp`). `npc.sentence` is a world order (`WORLD_ORDERS`: target the
  resident, with the run). `Game::applyStorySentence` refuses a resident protected, dead, in a fight or down, away (with a
  party, or nowhere), a foe, held by another story, or serving a sentence already (the same order again is no change); a
  place not indoors; 1 to 28 days. It moves them to the open tile nearest the room's middle (`World::teleport`, tried
  outward), holds them (`sceneHold`), says "held at <room>", and keeps `storySentences_[npc] = {place, by: the program id
  (the storyline's `templateId`), run, x, y, began, until, days, pace}`, saved with the checkpoint (`people.storySentences`,
  `storyTalkSave`/`storyTalkLoad`), **never in `Storyline::held`**, so `endStoryWorld` leaves it. `tendSentences` (from
  `tickStoryWorld`, every half second, before the stories and even when no story holds anything) puts one moved away back
  on their tile (after a restart too), never pulling them out of a fight, and at `until` calls `endSentence(npc, "served")`:
  the hold let go (unless a live story still holds them), they walk home on their own day, and `sentence served` (item
  `served`, quantity the whole days held). A dead one's sentence is dropped with no event. While sentenced,
  `heldByAnotherStory` counts it (so no other story's hold, follow, walk or foe takes them) and `whyNotJoin` refuses a party
  ("They are serving a sentence."). `sentence begun` carries the resident (actor), the run (target), the place (cell), the
  tile, the days (quantity) and the program id (detail). The console: op `sentences` (from the checkpoint, with days left by
  its clock) and `sentence-release` (`{npc, reason}`: queues the game's new `npc.unsentence`, which ends it with item
  `released`; the owner or "may steer campaigns"; audited `story.sentence-release`). Approval: the place an interior a link
  reaches (`world.interiors`, `world.links`).
- **Workbench and console** (`Editor/`): the scope chip "each wolf, across stories" in Variables (and told to residents,
  a tally's sum and the "a wolf's own" hint take realm); the `post` picker (`REF_KIND.post`, `refs.posts`: the title, "held by
  <name>" or "standing empty"); the run's state panel shows realm values per wolf; **World variables…** gains the tabs
  **The world's** and **Per wolf** (`RealmVarsPanel`: filter by name and wolf, ✎ Set… with a reason, the names the stories
  declare, the hand changes); the Campaigns list gains **Sentences…** (`SentencesPanel`: "Ilse Coalley, held at The Keep,
  the cells by lq-wreckers-light: 4.3 of 7 days left", Release now… with a reason). The render smoke has both panels.
- **Guide and format**: `Docs/Story-Maker-guide.md` §4 "Across campaigns: realm variables" (and four ways of carrying
  state), §6 the fifth scope and the Per wolf tab, §8 "Lasting changes: the crown's change and a sentence" (with Ridgemere
  3's ids), §13 the builder's effects and the two steering rights, §14 three rows (both effects and a realm value stay),
  §15 two rows, §16 two rows; the reference regenerated (`story_guide.py --write`; `--check` passes; both effects have
  examples, `post` a sample). `Docs/story-programs.md` gains "Across acts and the crown". `RUNNER_VERSION` is
  `doc86-phase8`.
- **Sample** `story_v2_samples.CROWN` ("The Moot Night", `v2-moot-night`, in `all_samples()`, so both twins run it): each
  wolf's `nh_voice` and `nh_pledge` (realm) come from the acts before (the case's `realm`); a wolf who howls adds to their
  own voice; when the chorus closes with three or more, the loudest is ranked and the voices pledged to Coldwater tallied;
  six of them, and rm111 takes the post rm112 held and rm113 serves a week in the cells; a second case, new wolves pledged to
  nobody, and the Moot refuses. It rehearses clean in both twins: nothing uncovered.
- **Tests**: `test_story_v2_runner` (`RealmVariables`, on a throwaway database: two runs adding 1 and 2 to one wolf's value
  every turn keep both, whichever takes its turn first; the console sets a value, refuses a wrong type and a name nobody
  keeps, lists it with its readers and audits it, and a run reads it at its next turn; a third run launched after has the
  value from the start; a type clash against a kept row and against another live story refused; a row of another type not
  read), `test_story_campaigns` (`AcrossActs`: a campaign whose Act I sets each wolf's voice and pledge and whose Act II,
  which carries nothing, reads them: Ash, who pledged, is heard, Elm, who never spoke in Act I, is not; Act II's write kept
  after Act I's), `test_story_effects` (the orders and payloads; approval refuses an unknown post, the Watch's post, a
  sentence outdoors and one in a room nothing reaches; the posts in `refs`; the sentences listed from a checkpoint and
  Release now queued and audited), `test_story_v2` (`Phase8AcrossActs`: the Moot in both twins, what a wolf brings decides
  it, what validation refuses), `test_story_native` (`RealmParity`), `test_story_catalogue`, `test_story_drafts` (the
  counts), `Tests/careers_tests.cpp` (`theCrownsChange`), `Tests/story_world_tests.cpp` (`phase8Sentences`, through the
  game: the refusals; sentenced upstairs with the program's id and `sentence begun`; held against another story and a
  party; still held when the story ends, put back when moved; after a restart still held, on the sixth day still, served on
  the seventh and let go with `sentence served`; Release now with item `released`; `npc.post` takes the stall from its
  keeper with a succession, by the story; the same orders sent again are no change), `workbench.test.ts` (119 programs; the
  Moot in words; a realm variable renamed in a case's realm values and expected values; a new post and sentence pass the
  checks), `campaigns.test.ts` (a sentence in words).
- **Run**: the whole `ctest` (122: all pass but `talk_scorecard_quick` under `-j8`, which passes alone, the timing test
  noted in Phase 7; and, with the database settings, the eight `_pg` tests the first run skipped, all pass; `pg_client`
  skipped as ever); every story Python suite but the scene, siege and facts live ones, with the local PostgreSQL (the
  tests' own `ratw_test_*` databases, and `RATW_STORY_TEST_DSN` on a throwaway database migrated through 0081, dropped
  after); `story_guide.py --check`; the Editor's tests (`npm test`: 78 and 154), `tsc --noEmit` and `npm run smoke`; a
  scratch Ridgemere 3 server from this branch (`tools/scratch.sh ridgemere3`: restored the save, listened, answered; stopped
  after). **A flake found, not caused here**: `story_world_tests` run six at once fails now and then at Phase 6's check
  "said as the watch list" (a client's line read before it is said); Phase 7's build at `4e4357b7` fails the same way, and
  every run alone passes.
- **Left or changed, and why**: (1) `dm.story_posts` is a view of the checkpoint, not a table refreshed by the game (above).
  (2) The spec's per-tick reload "since the run's cursor" reads two minutes before the cursor as well: timestamps alone can
  miss a row committed out of order, and reading a row again is harmless. (3) The console's Per wolf tab sets a value; it
  does not remove one (a removed row could not be told to a run reading by its cursor; set the first value instead). (4)
  Realm values ride in the run's state (`counters.realm`, as world values do) between turns, which the cursor needs; they
  are the shared store's copy, not the truth. (5) A chapter started in the same turn as the one that ends before it reads the
  other's last changes at its own first turn, before any of its triggers fire (the ending run saves after it starts the
  next). (6) `npc.post` and `npc.sentence` sent again are no change rather than a refusal, so a batch item applied again
  after a restart never pauses the run. (7) A sentenced resident moved away is put back by teleport to their tile, not
  walked, and their pace is restored when let go. (8) "Release now" is the console's `sentence-release` and the game's new
  `npc.unsentence`, which the spec did not name. (9) Realm timers are allowed (a moment is the world's day either way).
- **Going live** on `ratw_ridgemere3` needs migrations 0080 and 0081 (the user's to apply); the game server restarted from
  this branch (the crown's change, the sentences in the checkpoint), the story runner from it (realm variables, the new
  orders and checks; the native library rebuilt as usual: the realm scope is in it), the Editor released from it (the
  pickers, Per wolf, Sentences). The client needs no change: a realm tracker is a tracker, and a sentenced resident is a
  resident held where they are.

**Phase 8 after review** (2026-11-24, on `19c914e3`). The review found one bug and two gaps, all fixed with tests, two nits
(one fixed) and one older gap (fixed).
- **A wolf a command takes in has their realm values read then.** Only `tick_run` set the `JOINED` hook, and only the
  console's participant add read a joining wolf itself, so a wolf taken in by a quest given by hand, or by a
  `participant.add` from a trigger fired by hand, a signal or a story event, joined with nothing read. At the next turn
  the cursor passed their older row by, the story started them from the first value, and its write lost what they kept
  (reproduced: Birch keeping 10 ended at 1). Now `Steer.take` sets the hook for every command (the participant add uses
  it too), and as a safety net `realm_load` reads whole, whatever the cursor, every wolf of the cohort with nothing
  loaded (a wolf who keeps no value is read again each turn: one small query). `test_story_v2_runner` (`RealmVariables`:
  Birch, kept an hour before, taken in by a trigger fired by hand, read at once and added to, 11; a wolf put in the cohort
  unread is read whole at the next load).
- **A campaign's chapters agree on each realm name's type.** Each chapter was checked alone, so with no value kept yet
  Act I's number and Act II's text both passed, and the clash showed only when Act II failed to start at Act I's end.
  `approve_campaign` refuses it ("Chapter 2 declares nh_voice (realm) as a text; chapter 1 declares it as a number"). And
  `realm_save`'s upsert no longer rewrites a kept row's type: a row of another type is left as it is. `realm_clash` still
  reads only live runs, not approvals never published (an old unused ticket would block a declaration for good).
  `test_story_campaigns` (`AcrossActs`).
- **The workbench shows "whose" for a realm variable** (`workbench.ts`: realm is a wolf's own with player and party), so
  an author can pick the looped wolf's inside For each, as approval asks of a per-wolf test. `workbench.test.ts`.
- **Nits.** The rank and tally refusals say "a number (a text or switch) each wolf keeps (player or realm)", as does
  `Docs/story-programs.md`. "Release now" still checks the last checkpoint (its refusal says "as the world was last
  saved"; the panel lists only those), left as it is.
- **Older, not this phase's: the world packager.** `strip_people` refused any save written since Phase 1b, which holds
  `storyHolds`, `storyMemory`, `storyGreets`, `storyItemNames` and now `storySentences`; all five are in `PEOPLE_PRUNE`
  (a player's entries go). `test_world_package` (the fixture holds them, with a player's greeting and memory pruned).
- **Run**: every story Python suite but the live ones (with the local PostgreSQL, the tests' own
  `ratw_test_*` databases, dropped after), `test_world_package`, `story_guide.py --check`, the Editor's tests (78 and 154)
  and `tsc --noEmit`. No C++ changed. **Two failures found, not caused here**: `test_world_package`'s end-to-end
  `test_the_planted_players_are_stripped` (12 accounts closed, not 8) and `test_a_hostile_package_is_refused_before_anything_is_written`
  ("written before the checks") fail the same way at `19c914e3` with this change taken out; the other 51 pass.

## Phase 9 as built

**Phase 9 as built** (2026-11-24, branch `r3-nh-p9`, on `r3-nh-p8` at `b2122b8d`). Engine spec items C7 (`cast.pick` and a
giver from a variable), C15 (a mantle given back), J7 (`npc.queue`), SQ3 (a paw lent to a resident at work), SQ4 (quest
`rewardLimit`), M3 (House purses, behind their world switch) and T6 (a `story` talk topic). **No migration**: the casting
reads what 0067 and 0080 already give (`dm.story_residents`, `dm.story_posts`), the queue and the House purses' switch are
kept in the checkpoint. Two new effects (93 in all: `cast.pick`, group Residents, the story's own; `npc.queue`, group
Residents, `needsBuilder`), two new events (223 in the catalogue's list, 220 a story may name: `cast chosen`, `cast failed`,
group story, the story's own), one new quest field (`rewardLimit`), a quest's `giver` that may be `{"var": ...}`, one new
rehearsal case field (`cast`), one new together activity (`beside`), one new talk topic (`story`), one new account prefix
(`purse:`) and one new console op (`house-purses`).
- **C7: casting at run time** (`tools/story_quests.py`, `tools/story_program.py`, `tools/story_runner.py`). `cast.pick`
  `{var, job?, town?, faction?, quarter?, notIn?, near?}` is a story effect (`CAST_EFFECTS`): neither twin needed a change (an
  effect's operand, its town, is worked out by both as any; "near" `$actor` is read by the story from the fact). `Run.cast`
  makes a request (the near wolf's tile from the event's facts, the list left out as it is now, a seed) and answers it at
  once through `story_quests.CASTER`, a context variable: the runner sets it in `tick_run` and `take_commands` to
  `world_caster` (`cast_pick`: `dm.story_residents` with `live.npcs`, the post held from `dm.story_posts`, else the work as
  written, `live.npc_voice`'s quarter, `live.npc_state`'s alive, a House as a member or by the claim of their work place;
  living and placed only; nearest the wolf at their work in the wolf's place, then their home there, then the wolf's town,
  the rest by the seed); a rehearsal sets it to the case's `cast` (`{var: [resident, ...]}`, the first not left out). The
  answer writes the story variable and follows the fact with `cast chosen` (target the resident, detail the variable, actor
  the near wolf) or `cast failed` (the variable keeps what it held). Where nothing answers (What if) the request waits in
  `state.castRequests` and `story_program.advance` answers it before the next fact. Checks: at least one of the work, the
  town, the House or the quarter; `var` a story-scope `resident`, `notIn` a `residents` list of the story or the world;
  approval checks the town and the House. **A giver from a variable**: `giver_of` reads the quest's variable whenever the
  offers are worked out (no offer while it holds nobody), so the offer moves when it is recast; a copy keeps the giver it
  began with (`c['giver']`) for its hand-in, its journal view and a reward's greeting (`from_giver` with the copy's giver),
  and the offers add an entry closed to new takers at a copy's old giver; `references()` skips it.
- **SQ4: `rewardLimit`** `{perDay, scope: quest | purse}` (`Run.coin_allowed`): after `perDay` coin rewards a game day
  (counted in `state.rewardLimits`, for the quest or `purse:<purse>` across the program) a `character.grant` reward is given
  without its coin, with a reward note; where the program declares them it adds the coins to the world number
  `coin_<purse>_day` and pays none while the switch `coin_<purse>_ok` is off, the names `nh-purses` uses
  (`purse_counter`: `town:ridgemere` → `ridgemere`, `till:job:rm_eleanor_saltcot` → `till_eleanor_saltcot`). A quest with
  no `rewardLimit` pays as before. Refused on a quest that pays no coin.
- **J7: first in line** (`Core/RatwCareers.cpp`, `RatwWorld.cpp`, `RatwWire.cpp`, `RatwGame.cpp`). `Society::queueResident`
  keeps `careers.queued[town]` (saved with the careers, read back at most 64 a town); refused for one not unplaced, too young
  or old, or with no town; named again, "held"; named for another town, moved. In `tendCareers`, at a post that pays empty
  its eight days, the first in line for the post's town (`CareerWorld::townOf` of its work) is sent for before the city's
  wolves and the Country's choice (`arrivalFor` with `only`: any class), noted "first in line, by a story"; one placed
  since is let go of; one with no home in reach keeps their place. It never makes a vacancy. The game's order `npc.queue`
  (payload `{town}`, the runner's single-key `action()`), `World::queueResident` refusing an unknown town; approval refuses a
  resident with a place (V1.4's rows, role `queue`) and an unknown town.
- **C15: a mantle given back** (`Core/RatwGameStoryWorld.cpp`, `RatwGame.cpp`, `RatwGameHandoff.cpp`). `held.mantles` is
  `{npc: the mantle worn before, or ""}`, with `held.mantleHow` (how it was bound: `assignMantle` takes it now, "dm" by
  default); a second binding keeps the first's "before"; an older save's list is read as `{npc: ""}`. `endStoryWorld` and
  the story's own `mantle.release` call `putBackMantle`: the mantle before bound again, or the story's released; one passed
  to someone else since cannot come back, so theirs is released (logged).
- **SQ3: a paw at a resident's work** (`Core/RatwTogether.cpp`, `Data/Together/patterns.json`). `World::lendAPaw` to a
  resident goes to `lendToResident`: at their post, in their hours (`residentAtPost`, the check `residentWorkAt` used), and
  not farm work or a bench (those stay `helpAtWork`'s, which `lendToResident` hands them to), the activity `beside` (`at`
  post, beats of 300 s by the clock, three at most): no practice, no `lendPaw` (nothing of theirs made faster), no coin.
  At its end `together` {actor the wolf, target the resident, item `beside`, quantity the beats} and the resident's regard,
  once a game day for the pair (`lentToday_`, by the beats in `beatsTogether`, so a wolf who left first still counts);
  refused again the same day. The resident's menu offers **Lend a paw** (`besideWorkAt`).
- **M3: House purses** (`Core/RatwGameStoryWorld.cpp`, `RatwGameStoryFacts.cpp`, `RatwStagedAccounts.cpp`, `RatwCareers.cpp`).
  `house:<faction>` (or `house:<name>` for `house_<name>`; a faction whose name begins "House ") maps to the account
  `purse:<faction>` (`Game::housePurseFaction`, `storyPurseAccount`), a new facility prefix, staged, **never topped up**
  (`topUp`, `payable`: it pays what it holds). `Game::tendHousePurses`, once a game day, moves to each House's purse a tenth
  (`HouseShare`) of what each of its businesses' tills holds over 80 (`HouseTillFloat`): the posts held whose work cell the
  House claims first (the game's own rule for who serves a faction), 2,500 at most in a purse (`HousePurseMost`). All of it
  behind the **world switch `storyHousePurses`**, off: the game's order `story.house_purses on|off` (kept in the checkpoint as
  `people.storyHousePurses`, which the world packager keeps), the console's op `house-purses` `{on, reason}` (the owner only;
  audited `story.house-purses`). Off, nothing moves and a `house:` purse maps to no account ("There is no such purse to pay
  from"); approval refuses a `house:` purse while the checkpoint says off, an unknown House, and (on) a House with no purse
  as last saved. Read only against Ridgemere 3: the four Houses claim 15 to 25 places each, with 5 (Fell) to 12 (Grayrock)
  businesses that have tills, so the rule finds each House's trades without a list.
- **T6: the `story` topic** (`Data/Talk/topics.json`, `Core/RatwTopics.cpp`, `RatwAmbient.cpp`). A news topic (base 2.2),
  a source the director scores, weighed once for each prefix of a live story's realm memory keys (`nh` for `nh_stage`,
  `topic.story` the prefix), only where the talk library has `talk.story` scenes (`Game::storyTopicsChanged`, after each
  story talk order, a run's end and a load, sets `World::setStoryTopics`). The test town's library has none, so the topic is
  silent there: **the scorecard (`talk_scorecard_quick`) and the golden transcripts (`talk_golden`) were re-run and moved
  nothing**. `Tests/fixtures/talk_topics/topics-today.json` gains the topic (a topics.json must give every source).
- **Workbench and console** (`Editor/`): the quest editor's **Or given by whoever a variable holds** (the story's resident
  variables) and **Coin rewards a game day, at most** (counted for this quest or its purse); `giverWords`; the Quests grid
  says "from whoever <variable> holds" before anyone is cast; renaming a variable follows it into an effect's own variables
  (a tracker's, a chapter's list, a casting's into and not one of), the values in effects and quest rewards, a quest's giver
  and a case's castings. `cast.pick` and `npc.queue` take the pickers they have (variable, town, faction, wolf, text).
- **Guide and format**: `Docs/Story-Maker-guide.md` §7 "Quests any town can give", a House's purse and a plain cap on the
  coin; §8 the story topic, first in line, a paw at a resident's work, a mantle given back; §13 and §14 rows; the reference
  regenerated (`story_guide.py --write`; `--check` passes; both effects have examples). `Docs/story-programs.md` gains
  "Casting, queues, paws and House purses". `RUNNER_VERSION` is `doc86-phase9`.
- **Sample** `story_v2_samples.CASTING` ("The Carter Wanted", `v2-carter-wanted`, in `all_samples()`, so both twins run it):
  the first wolf casts a carter of Greyfen near them (left out once tried); the axle's giver is whoever was cast; a paw
  lent beside them (`together`, item `beside`) counts and binds them the ferryman's mantle; the axle pays four pennies at
  most once a game day (`rewardLimit` by purse, writing `coin_greyfen_day`: Birch's hand-in the same day gets no coin);
  a second case, nobody fits, and a wolf of the Country is first in line (`npc.queue`). It rehearses clean in both twins.
- **Tests**: `test_story_quests` (`Phase9Casting`: a carter cast in Larkfield gives the quest, recast when they die and never
  the one tried, the offer moved and the old giver's closed entry, the copy handed in to its own giver and greeted from it;
  nobody fits; a casting waiting for a caster answered before the next fact; the checks; `rewardLimit` pays twice then
  without coin, writes `coin_larkfield_day` 6, pays again the next day, pays nothing with the floor switch off, and its
  checks), `test_story_v2_runner` (`Casting`, on a throwaway database: the query, living, placed, of the work and town,
  nearest the wolf, never one left out, the dead and the unplaced never cast; a published run casts in its turn, `cast
  chosen` fires in the same turn, the `story.quests` order offers the axle at the resident cast, a third casting fails),
  `test_story_problems` (`Phase9InTheWorld`: `house:greatly` refused while the switch is off and an unknown House, "no
  purse yet" when on, accepted once its account is saved; `npc.queue` refused for a placed resident and an unknown town;
  a casting's town and House), `test_story_native` (`CastParity`: the casting's town from a value, `cast chosen` and `cast
  failed` matched by their variable, a mantle bound to the resident cast, alike in both twins), `test_story_v2`
  (`Phase9Casting`), `test_story_catalogue`, `test_story_effects`, `test_story_drafts` (the counts), `test_story_words` (a
  casting's work and quarter are words nobody hears), `Tests/story_world_tests.cpp` (`phase9Mantles`: u1's rm950 back after
  the story's rm951, u2 with none after, the story's own release, an older save's list; the story topic silent without
  scenes and weighed by its prefix with them; `phase9HousePurses`: off, nothing moves and nothing is paid; on, 42 of the
  stall's 500 to House Greatly's purse at once and once a game day, Ash paid 30 from it, then refused as short, never topped
  up; kept through a restart), `Tests/together_tests.cpp` (`besideAResident`: Tam training the recruits, two spells, no coin,
  nothing made faster, `together` {Ash, Tam, beside, 2}, once a day, nothing off her hours), `Tests/people_flow_tests.cpp`
  (`firstInLine`: the baker first in line at the docks before the fisher the rules would send for, refusals, kept through a
  save, no vacancy made in twelve days, sent for at the docker's death), `Tests/status_tests.cpp` (`purse:` is staged),
  `workbench.test.ts` (120 programs; the Carter in words; renames through the casting, the giver and the case).
- **Run**: see below (the run at the end of this phase).
- **Left or changed, and why**: (1) **M3's economy check was not run**: it is a simulation (a House's purse filled for 30 game
  days against the same run without it), and the user has not approved it (memory: ask before simulations; answer (c)
  covers combat sims only). M3 is built and tested, and stays off behind `storyHousePurses` on every world until the user
  approves the run and the owner turns it on. (2) The spec's `house:` accounts are `purse:<faction>`: `house:` accounts are
  the old economy's, dropped at restore (doc 79, 14). (3) Which businesses are a House's is the game's own rule for who
  serves a faction (the House claims their work place first), not a list; the share, float and cap are placeholders.
  (4) `npc.queue`'s test is in `people_flow_tests` (the arrivals' fixture), not `careers_tests`. (5) A casting's "nearest"
  is by work and home places (the database knows no resident's tile now), then the wolf's town, then the seed. (6) The
  `story` topic needs `talk.story` scenes in the library to open; none is written yet (the talk plan's), so until then the
  campaign rides `news`, `gossip` and `day` as the spec's fallback says. (7) A paw beside a resident gives no practice (the
  spec asks only for no wage and no output change; practice would be a new source of skill), and the once-a-day mark is
  in memory, not saved (a restart may let a pair be counted twice that day). (8) A House purse holds at most 2,500 so a
  quiet House does not hoard the tills' money. (9) A `cast failed` leaves the variable as it was (the spec does not say).
- **Going live** on `ratw_ridgemere3` needs no migration; the game server restarted from this branch (the queue, the
  mantles given back, the paw, the House purses' switch, the story topic), the story runner from it (casting, the giver
  from a variable, `rewardLimit`, the new orders and checks), the Editor released from it (the quest editor). The client
  needs no change: "Lend a paw" is a menu action as any. House purses stay off until the user approves their economy check.

**Phase 9 after review** (2026-11-24, on `d2e3d8c7`). The review found four bugs and three gaps, all fixed with tests, and
six nits (five fixed); two more were found while fixing.
- **The House purses' order is `story.house_purses`.** `dm.actions` takes only `^[a-z]+\.[a-z_]+$`, so the op's
  `story.house-purses` failed its check and the switch could never be sent. Renamed in the runner and the game (the audit
  keeps `story.house-purses`). And the op refused the studio's own `admin`, who outranks an owner: it asks for the owner's
  rank now. `test_story_console` (`HousePursesRoute`, through the route: a Dungeon Master reads it and is refused the
  change, a bad value refused, the owner's order in `dm.actions` and the audit, the admin turns it off, the audit newest
  first).
- **Casting by House matches.** The faction arrays are of the domain `ratw_id`, which psycopg returns as one string, so
  `faction` never matched anyone; they are cast to `text`, and "the House that claims it first" is the first by id, as the
  game sorts a place's claims (`cell->factionClaims.front()`). `test_story_v2_runner` (`Casting.test_by_house`: Greatly
  claims the row first, Zed too but not first, a member of Fell).
- **A quest's coin limit counts hand-ins.** `coin_allowed` counted each grant, so a bonus or a party's second wolf used up
  the day. One decision now covers a whole hand-in (`Run.handing_in`, by the limit's key: its rewards, its bonus, every
  wolf); the coins still add to `coin_<purse>_day` grant by grant. `test_story_quests`
  (`test_a_reward_limit_counts_hand_ins_not_grants`: 3 and 2 paid, `coin_larkfield_day` 5, one counted; Birch's the second
  that day, no coin; a party of two both paid, one counted).
- **The `story` topic after a restart.** The story memory is read before the stories, so the topic closed at every load;
  `storylinesLoad` now calls `storyTopicsChanged`, and `tickStoryWorld` looks again once a game day (a key whose days ran
  out closes it). Found while testing it: the talk library could not hold a `talk.story` line at all (`topic.story` was
  no fact, and `talk.story` is no concept): `Data/Talk/facts.json` gains the fact `topic.story`; the concept is declared in
  `settings.json` by whoever writes the first `talk.story` lines (the library refuses a declared concept with no lines,
  `talk_library`), so nothing said changes now. `story_world_tests` (`phase9StoryTopicRestart`: Data/Talk with the concept
  and one `talk.story` line of the test's own; open on `nh`, still open after a restart, closed when the story ends).
- **Nearest the wolf, in a live run.** The casting read the wolf's place only from the world's facts, which a story that
  reads no other wolf fact never has. Now, near the fact's own actor, it is where the fact happened (game.events' cell and
  tile; the caster finds the place's town); `watch_of` asks for `wolves` when a casting is near another wolf. A trigger
  fired by hand passes its stand-in as the fact (nit N4), so `$actor` is the wolf it was fired for.
  `test_story_v2_runner` (the nearer carter on the row, and in the yard, from the fact alone).
- **A mantle given back only while it is still worn.** `held.mantleBound` keeps what the story bound last; at the end (or
  its own release) the mantle before is put back only while the resident still wears that, so a DM's later binding
  stands. Two stories in turn: when one lets go and the other's "before" is its mantle, the other takes on its "before"
  (`Game::handOnStoryMantle`), so the end of both leaves what was worn before either, whichever ends first. An older save
  without `mantleBound` is put back as before. `story_world_tests` (`phase9MantlesOverlap`: the DM's rm953 left; S1 then
  S2, S2 then S1, and a story's own release under another's binding, all ending in rm950).
- **The console's House purses panel** (`Editor/src/dm/campaigns/HousePurses.tsx`, **Campaigns → House purses…**): the
  switch as last saved, each House's purse (`house_purses_state`, the read `house-purses-state`) and the last changes;
  the owner (or admin) turns it on or off with a reason. `campaigns.test.ts` (`housePurseLine`), the render smoke.
- **Nits.** N1: the day the Houses last took their share is saved (`people.storyHousePurseDay`) and not reset when the
  switch goes on, so a restart, or off and on again, takes no second share that day (`phase9HousePurses`). N2: one first in
  line who is placed since or past working age leaves the line, and one with no home in reach keeps their place while the
  next is tried (`people_flow_tests` `firstInLine`: the old one first, the baker behind them sent for). N3: `npc.queue`'s
  help and the guide say who comes first (the town's own, then the line, before the city's wolves and the Country). N4
  above. N6: the world packager drops `storyHousePurses` and `storyHousePurseDay` (`PEOPLE_RESET`), so a world given
  away starts with House purses off (`test_world_package`). **N5 left**: a casting waiting (What if only) that moves the
  phase still drops the fact it was answered before; dispatching that fact too would need the caller to enter the new
  visit first, which `advance`'s True already asks of it.
- **Run**: the whole `ctest` (122, with the database settings: all pass but `talk_scorecard_quick` under `-j6`, the timing
  test noted before, which passes alone; `pg_client` skipped as ever); every story Python suite but the live ones, with the
  local PostgreSQL (the tests' own `ratw_test_*` databases, and `RATW_STORY_TEST_DSN` on a throwaway database migrated
  whole, dropped after), `test_dungeon_master`, `test_world_package` (all but the two end-to-end failures noted in Phase 8
  after review, unchanged), `story_guide.py --check`; the Editor's tests (78 and 156), `tsc --noEmit` and `npm run smoke`
  (65 panels, House purses among them); a scratch Ridgemere 3 server from this branch (restored the save, answered;
  stopped after).

## End of slice as built

**End of slice as built** (2026-11-24, on `r3-nh-p9` at `32504dbb`; commits `9de99d07` and `491c11d5`). The checks across
all phases, one end-to-end on a copy of Ridgemere 3, and one review-and-fix pass over `ridgemere2...r3-nh-p9`. **No
migration** beyond Phase 8's 0080 and 0081; nothing was written to `ratw_ridgemere3`, `ratw_dev` or `ratw_prod`.
- **The whole check** (the local PostgreSQL; the tests' own `ratw_test_*` databases and `RATW_STORY_TEST_DSN` on a
  throwaway database migrated whole, dropped after):
  - `ctest` (122): all pass but `talk_scorecard_quick` under `-j8`, the timing gate noted in every phase, which passes alone
    three times out of three; `pg_client` skipped as ever. `story_world_behaviors` failed once in 40 runs under load: its
    check that the watch list is said with no count before it (`!contains(gave, "1 the watch list")`) matched the end of
    "Neighbour u1 the watch list" whenever the give's line was in what Ash had heard; it looks for " 1 the" now.
  - Python: every `tools/test_*.py` (74 files, 1,480 tests) one at a time, five at once. The failures are the known ones,
    each failing the same on `ridgemere2`: `test_live_edit` (1), the map and terrain exporters (`test_map_editor` 1,
    `test_terrain_catalog` 1), the Dungeon Master mark's check in `test_live_map` (1), `test_world_admin` (2),
    `test_world_package`'s two end-to-end tests. `test_story_facts_live` and the scene and siege live tests passed
    this time. After the fixes below, every `test_story_*.py` (23), `test_dungeon_master` and
    `test_studio` again: all OK.
  - The Editor: tests (78 and 156), `tsc --noEmit`, `npm run smoke` (65 panels; the workbench over 108 programs, 1,192
    views, and the quests' 2, none edited) and `npm run build`. The client: tests (246) and `npm run build`. `story_guide.py --check`.
  - A scratch Ridgemere 3 server from this branch (`tools/scratch.sh ridgemere3`): restored the save (5 wolves, 777
    residents), listened, served the page; stopped after.
- **End to end** (scripts kept out of the tree, in the session's `/tmp/r3eos/e2e/`). A scratch server applies no orders,
  so the run was on a **copy**: `ratw_ridgemere3` dumped (read only) into a throwaway `ratw_test_eos_r3_*`, migrated
  through 0081 (only 0080 and 0081 were new), and a game server from this branch on it (`--database ridgemere3` with
  `RATW_DATABASE_URL` naming the copy, `--dev-identity`, port 0, 127.0.0.1). The save's accounts are central (ticket)
  accounts, which a password-mode server refuses, so the copy's accounts and its five wolves were taken out first and
  three new wolves (Ash, Birch, Cedar) came in through real game connections (the client's own `Connection`). The real
  story runner ran as its own process (`story_runner.py --world ridgemere3` with `RATW_RIDGEMERE3_DBNAME` the copy), and
  everything an admin does went through the DM tool's route (`POST /api/story-program`, a Dungeon Master account of the
  copy). The stories: **a campaign of two chapters** ("The Commons": chapter 1 ready by participation, `of: active`,
  30% and a floor of 1, `auto`, its closing phase starting chapter 2 with the same wolves; chapter 2 ready after 5 days,
  not `auto`) and **a small pack** ("Raiders at the Commons": Rupert Ropehand the farrier, moved beside the wolves and
  held; a pledge quest and a raid quest that `requires` it, `closure: line`). Seen, in order:
  - `campaign.approve` (both chapters rehearsed, no warnings), chapter 1 published; the pack approved alone and
    published; both phases applied; the farrier moved and held (one `story.effects` batch).
  - Each wolf opened Rupert's window: **Ask about: A pledge to the farrier**, "I'll do it." ("You take on: A pledge to the
    farrier."), then the story's choice **I will stand with the Commons.** ("Then the Commons will remember you."): the
    **pledge** (`dialogue choice`, `p_pledge`, the pledged list) and **a till payment** each: "Paid 3p to Ash from
    till:job:rm_rupert_ropehand: the farrier's penny." (cash 20 to 23, all three).
  - The raid offered only then (`requires`), taken in the window; **a foe**: "Grisk the Raider come to Ridgemere, the
    Commons and the Rise", hunting: he set on Cedar, the Watch on duty (Tilde Nailby, Vidar Dripmarsh) came in, Ash and
    Birch joined from their own cards; Cedar was downed in one; **Grisk broke off: `foe fled` (fled)** in every
    run. The pack's trigger completed every raider's copy; **a title**: "Named Raid-Breaker of the Commons." (all three,
    saved with the wolves, known in Ridgemere for 30 days); **a closure counted**: "A finished Story: The raider on the
    Rise (3 social)." for each.
  - **A notify**: the pack's `story.notify raid_won` for each wolf reached chapter 1 (`story-event` commands, "Story
    notified: e2e-raiders (raid_won); heard fired."); **chapter 1 moved on by participation**: "Ready: 3 of 3 active
    wolves (100%; 30% and 1 needed)", into its closing phase, finished, and started chapter 2 "for 3 wolves".
  - **Chapter 2 moved on by an admin Advance**: the route's `command` `advance` `{when: now, anyway: true}` with a reason:
    "Advanced to done now anyway (not ready: 0 of 1 parts met): 0 of 5 days.", audited (`story.advance`, by the Dungeon
    Master, with the reason); the campaign "ended", both chapters complete, the timelines with their `ready`, chapter
    and command items.
  - **Found by it: the server crashed** at the pack's first batch. `npc.hold` took the story's `npcs` section by
    reference, then made `follow`, whose new field moved the first: the hold was written to freed memory (a segfault on
    Ridgemere 3; in the test town it had passed because earlier sections had left room). Doc 85, Phase 9's code; the
    same in Phase 7's foe loop (a resident made a foe and beaten was held, making `npcs`, while the loop walked `foes`).
    Both fixed (`9de99d07`); `story_world_tests` has a third story whose first order is a hold, which segfaults without
    the fix. The end-to-end then passed, and again after the review's fixes.
- **The review** (one pass over `ridgemere2...r3-nh-p9`, 164 files): four bugs and four nits, all fixed (`491c11d5`).
  - **A casting took 36 s on Ridgemere 3**: `cast_pick` read `dm.story_posts` (a view over the whole checkpoint) once per
    resident. The posts held are read once: 0.1 s, the same choice.
  - **`npc.post` was journalled as Economy only**; positions are journalled with Careers, so a crash before the next
    checkpoint lost the post. `record(Careers | Economy)`.
  - **A resident made a foe who died outside a fight** (old age, a Dungeon Master, `npc.kill`) ended with no event, so a
    story waiting on its foe waited for ever. Now `foe fled` with item **`gone`** (the catalogue's help and the guide's
    reference: fled, time or gone). `battle_tests`: dead outside a fight.
  - **The casting's seed** counted the casting log, which keeps 20, so from the 21st casting on every one asked the same
    seed: `castCount` now (the same seeds as before for the first 20). `test_story_quests`: thirty castings, thirty seeds.
  - Nits: a story's own words over a limit are no longer cut short by the name fills (they come back whole and the order
    refuses them, as before C1b; names that make a line too long are still cut at a word: `story_world_tests`); the Foes
    panel's time left at `--speed`; `npc.queue` says "in line there already" (the newest a story names is first);
    `talk.remember` says "Not remembered" for a memory the caps let go of at once, and the story does not hold it
    (`story_world_tests`).
- **Left, and why**: (1) **`foe defeated` was not seen live**: three new wolves never beat a "tough" foe, who runs below
  25 health; `battle_tests` and `story_world_tests` play it (a champion who never runs). (2) The end-to-end's world is a
  copy with its accounts and wolves taken out (a password server and a ticket save never mix: doc 77), so it says nothing
  about the laptop's central sign-in; the scripts that made it are not in the tree. (3) The closure was read from the
  game's receipt, not from a wolf's Quickening checklist. (4) **M3's economy check is still not run** (a simulation the
  user has not approved): House purses stay off behind `storyHousePurses`. (5) The pre-existing Python failures above are
  not this slice's.
- **Going live on Ridgemere 3** needs, in order: this branch merged into `ridgemere2` (all phases, `r3-nh-p1a` to
  `r3-nh-p9`, are one chain); the migrations no phase has applied there, **0080** (`dm.story_posts`) and **0081**
  (`dm.story_realm_vars`) (`ratw_ridgemere3` is at 0079; `world_db.migrate` applies them in order); the game server rebuilt
  and the laptop's world restarted (`Saved/WorldAgent/run.sh`, port 7788; publishing a build restarts it); the story
  runner restarted (`tools/story_runner.py --world ridgemere3`); the Editor (Studio and the desktop shell's Editor) and
  the client released from it (a client after 0.1.10, for the slice's journal, quest and card changes).
  Then section 10's preparation: the allow-listed words, the world variables, protecting the cast, `warden_order`. House
  purses stay off until the user approves their economy check and the owner turns them on.

---

## Content, today-only waves as built

**Built 2026-11-22** on branch `r3-nh-content`: waves **A, C, D, E, F and G** of §9.2, each in its today-only form (only
what `Data/Storylines/trigger-catalogue.json` has today), each reviewed once and fixed. **Not yet written:** wave B
(Act I), H (Act II) and I (Act III), so `ninth-howl.1` … `ninth-howl.9` do not exist yet; wave J follows the engine
phases. Nothing has been approved, published or loaded into Ridgemere 3, and no database was written.

### What was written

| Wave | Program (file under `Data/Storylines/ridgemere3/`) | Quests | Rehearsals |
|---|---|---|---|
| A | `nh-purses` (`ninth-howl/nh-purses.json`) | — | 4 |
| A | `ninth-howl.cleanup` (`ninth-howl/ninth-howl.cleanup.json`) | — | 1 |
| A | world variables (`ninth-howl/world-vars.json`): **122** declared, campaign §4 and every pack's own | — | — |
| C | `nh-pack.small.city` (`packs/`), quests 1–19 and 46–48 | 22 | 8 |
| C | `nh-pack.small.north` (`packs/`), 20–31 | 12 | 7 |
| C | `nh-pack.small.south` (`packs/`), 32–45 | 14 | 6 |
| D | `nh-pack.medium.city` (`packs/`), Q1–Q7 | 7 | 15 |
| D | `nh-pack.medium.greenholt` (`packs/`), Q8–Q12 | 5 | 11 |
| D | `nh-pack.medium.south` (`packs/`), Q13–Q19 | 7 | 14 |
| D | `nh-pack.medium.north` (`packs/`), Q20–Q23 | 4 | 8 |
| E | 11 large lines (`quests-large/lq-<line>.json`): salt-road, mast-cedars, old-brown, clipped-silver, blight-and-boar, runnel-fever, wreckers-light, long-run, eel-famine, vanishing-thief, black-hearths | 51 (4–5 a line) | 48 |
| F | `nh-pack.stage` (`quests-stage/nh-pack.stage.json`), 7 phases | 22 | 7 |
| | **21 programs** | **144 quests** (104 counting each large line once) | **129** |

- **Notes beside the programs:** `ninth-howl/notes.md` (wave A and the talk library), one `.notes.md` per small pack,
  `packs/nh-pack.medium.notes.md`, `quests-large/lq-lines-1-4.notes.md`, `lq-lines-5-8.notes.md` and one for each of
  eel-famine, vanishing-thief and black-hearths, and `quests-stage/nh-pack.stage.notes.md`. Each says where it differs
  from the design and why, and what its later versions add.
- **Wave G, the talk library** (inert until C8, or until the DM seeds the keys):
  - Part 1 (the cast, the Keepers and the campaign's givers): `Data/Talk/dialogue/resident.tree`, `watch.tree`,
    `church.tree` and `inn.tree`. The trees gain 125 nodes, among them `nh_story`, `nh_help`, `nh_who`, `nh_ask` and
    the `nh_*` talking paths, plus the one new `news` choice.
  - Part 2 (gossip, helpful knowledge, recaps and scenes): 661 campaign lines and 353 pack lines, written into
    `resident.tree` and `inn.tree` between part 2's markers. Also `Data/Talk/rules/story_ninth_howl.talk` (the
    resident-to-resident rules) and `Data/Talk/scenes/common/story_ninth_howl.talk` (eleven scenes). The generator and its
    sources are in `artifacts/quests/build/talk2/` (`campaign.dsl`, `extract_packs.py`, `build_packs.py`, `gen.py`).
    Re-run `python3 artifacts/quests/build/talk2/gen.py Data/Talk` after anything rewrites those two trees; a re-run
    reproduces the committed files exactly.
  - `tools/dialect_allow.json` (T9): the campaign's places and titles.
- **Tools:** `tools/nh_check.py` (the §9.3 checks, and since the third pass the door rule: see "Doors a story
  unlocks" below) and `tools/nh_world_vars.py` (declares the world variables, or lists the DM's steps).

### Conventions forced by today's engine
- **No top-level `prefix` and no `closure`**: today's validator refuses both. Every id carries its prefix by convention
  (`nh` `sq` `mq` `lq` `st` `coin`), and `nh_check` refuses a world variable without one.
- **Locked doors** (doc 82, `World::playerDoorLocked`): a wolf can't enter a private interior. Since the third pass
  (DECISION of 2026-11-22, built on branch `r3-nh-doors`) a door a live story has unlocked (`door.unlock`) is open to
  wolves while the story holds it, and a story never unlocks a home. The programs use it only for the workplaces of
  the residents a wolf could not otherwise reach, and close one with `door.close` (never `door.lock`) where its window
  ends before the run does (see "Doors a story unlocks" below). Every other `enter place` into a
  locked building is an outdoor region at its door. Residents who never come out speak in a one-line scene at their
  own door. Every quest giver, `npc.offer` holder and gift or talk target is a resident a wolf can reach where they are
  in their working hours (see "Givers behind locked doors" below).
- **Props are rationed per wolf** with player timers (once a game day, or once a week for laid trails). A quest set
  aside and taken again would otherwise hand out new goods each time.
- **Fights scale per copy**: two bandits for each wolf's copy of a player-scope quest, and one party-sized band for a
  party quest. Story camps are switched off when they are broken or scattered, and `nh_threat` falls at most 3 a game
  day per line.
- **A window choice is a `dialogue choice` objective**, not a `conversation` (only a typed line logs one). No choice is
  `once` where a wolf could pick it before the step that needs it.

### The checks, as run at the commit
- `python3 tools/nh_check.py --all`: **23 of 23 PASS** (world-vars, the 21 programs, and the talk library).
  - For every program: `validate`, `problems(p) == []`, and `review` with no phase or trigger uncovered.
  - References pass on Ridgemere 3, read only.
  - Every named resident is placed, alive and reachable.
  - The giver rule, the purse rule and the words checks all pass.
- **Across programs** (the commit's own check): no program, quest or `npc.offer` id is used twice. No resident gives
  or holds choices in more than one program. No resident is a target in more than two pack quests outside their own.
- `ctest` talk_golden, talk_scorecard_quick, talk_topics, talk_library, dialogue_trees, dialect_check,
  dialect_check_rules, talk_lint, talk_lint_rules and talk_coverage: **10 of 10 passed**.
- **One cross-piece fix at the commit.** The talk library followed the design where the reviewed small pack had
  changed it. A badger runs and bites only when cornered, and Saltwell's farm cart comes in most mornings. Both are
  fixed in `build_packs.py` and the trees regenerated.

### What waits for which engine phase (§8)
| Phase | What the content gains |
|---|---|
| 1a (C2, R1–R3) | Act I's readiness. Wave B is still to be written, on the direction-exit stand-in or on `ready` blocks. |
| 1b | SQ2: shop tills pay (until then the treasury or a town store pays, worded as the shop). J1: per-wolf offers (today every wolf of a story sees its choices, and a second wolf at the same giver replaces the first wolf's pick). N2: hide choices that presume a deed. C8: every gossip, helpful and `nh_coin=thin` line plays (until then the DM seeds the keys). C9: the `{fig}` figure lines. C11: `npc.unspare` in the cleanup. |
| 2 | C1 values, M2 the purse operand in place of the floor switches, SQ1/SQ5, L2/L3 named camps and camp objectives, N4 |
| 3 | C5 named thanks (the board-only quests' `npc.greet`), C3/C4 titles and ranks, J8, T5 |
| 4 | C12a credit to the running chapter, C6 `requires` (e.g. the Feast Road offered only on feast days), J3 `closure`, J10, L5 |
| 5 | L1: the bear and the boar hunts become required objectives |
| 6 | J9 story items: today any `document_tube` or pair of shears counts |
| 7 | C10 foes tuned to the bar |
| 8 | J2 per-wolf state across acts, C14, L4 |
| 9 | SQ3, M3 House purses (needs an economy run, asked first), T6 a story topic for the scenes |

### Givers behind locked doors (second pass, 2026-11-22)
**The rule, from the code.** `World::playerDoorLocked` (Core/RatwWorld.cpp) decides it, and every one of Ridgemere 3's
414 doors falls under it (all its 241 cells are `w_ridgemere_*`). A door from a street into an interior is locked to
wolves unless the interior is an inn, alehouse or tavern (open day and night), or a **merchant** post works in it and
the hour is inside that merchant's hours. Stairs and passages inside are never locked, so a flat over an open shop can
be reached too. Residents' posts come from `live.npcs` (`role`, `work_label`, `hours_start`/`hours_end`,
`work_area`), so the rule can be worked out read only. A civilian therefore can be reached at work only when they work
out of doors (streets, the wharf, the piers, a stall, a field, a cutting), in an inn, or in a shop whose merchant is in.
Farm sheds, barns, boat sheds, mills, presses, halls, offices, the abbey, the schools and every home are shut to wolves.

**The survey** (a read-only pass over the 21 programs and `ratw_ridgemere3`): 167 residents are named as a giver,
`npc.offer` holder or gift, talk, trade or near target. 84 had a workplace locked to wolves (or none) at every hour of
their working day (the first note of this problem counted the medium packs only). Nine places that
steps needed were locked interiors too: the Couriers' Office (`lq-long-run`), Vance House and the Sunken Street, where
the cache was laid (`lq-vanishing-thief`), the Bay Light's lamp room, the Lamp Store and the Chapel of the Tide
(`lq-wreckers-light`), the cathedral, the Clock Tower and Fellhall's kitchens (`nh-pack.small.city`). And archive work is given nowhere on Ridgemere 3: no work label matches a post in
`Data/Lore/archives.json` ("copying at the Records Office" is not "copying the city rolls"), and the Records Office is
locked anyway.

**Order of fixes, as decided.**
1. *A story's door effects*: not available. `door.unlock` and `door.open` set `Door::locked` and `Door::open`
   (`World::setDoorState`, undone at the run's end), but `playerDoorLocked` is worked out from the cell, the inn names
   and the merchants' hours and never reads them. No effect in today's catalogue opens a workplace to wolves.
2. *Re-cast* to an unused, placed, living resident of the same town who works where a wolf can walk in during their
   hours, keeping one pack quest per resident (`givers.tsv` updated row by row), with the lines rewritten in the new
   giver's speech (`live.npc_voice`: same town, mostly the same dialect; a set phrase only in its own dialect's mouth).
   Most re-casts are the wolf who trades with the locked workplace: the soapworks for Oakhurst's fat, the Looms for
   the Larkdowns wool, the Still for Meresend's apples, the Stores for the Glasswater catch, the estates' cottagers who
   work in city shops.
3. Where a required place was a locked interior, the step moved to an outdoor region at its door (the convention above).

**Re-cast, Ridgemere** (old resident, where locked → new resident, where a wolf finds them):

| Program · quest | Role | Was | Now |
|---|---|---|---|
| small.city · `sq-morning-bread` | target | Theodora Wickley (Sump Infirmary) | Ottilie Nailmarsh `rm_ottilie_nailmarsh`, porter at the foot of the Sump Stairs, out of doors 6–18 |
| small.city · `sq-mending-herbs` | giver | Frederick Nailgate (House of Mending) | Rupert Pitchford `rm_rupert_pitchford`, apothecary's stall on Market Square 6–16 |
| small.city · `sq-heights-round` | giver, target | Gwen Cinderdale (City Office); Roald Wickstack (Vance House) | Kjell Coalham `rmx_kjell_coalham`, Sump Stairs porter 8–18; Eleanor Vance `rm_eleanor_vance`, runs the Rag Mill 6–18 |
| small.city · `sq-restday-candles` | giver | Berit Coalworth (cathedral) | Sten Kelpwell `rm_sten_kelpwell`, chandler at Wickham's 7–19; the service is kept at the cathedral door (region `cathedral_door`) |
| small.city · `sq-almoners-round` | giver | Disa Raincot (Chapter House) | Arne Wickdale `rm_arne_wickdale`, brewer at the Abbey Brewhouse 6–18 |
| small.city · `sq-greatly-stables` | giver | Philippa Rustton (Greatly Stables) | Hjalte Sootby `rm_hjalte_sootby`, the Greatly Yard 6–18 |
| small.city · `sq-dawn-bell` | giver | Corwin Bilgestack (Lamp Store) | Mathis Plankham `rm_mathis_plankham`, Sump Stairs porter 6–18; the bell rope at the Clock Tower door (region `clock_door`) |
| small.city · `sq-heights-thyme` | giver | Egil Grantham (Grantham House) | Mogens Ashley `rm_mogens_ashley`, the Mortar and Pestle 8–18 |
| small.city · `sq-records-copying` | giver | Harriet Platt (Records Office) | Runa Ashridge `rm_runa_ashridge`, the Council Press 8–18; now "Paper for the Rolls": two of paper and an ink, and the hidden page asked of the printer (Ch 4–6) |
| small.city · `sq-fellhall-errand` | giver | Ansel Tarley (Fellhall) | Erland Fogley `rm_erland_fogley`, the Lamp and Wick 8–18, lives in the Fell Cottages; the optional "in by the kitchens" became an optional butter |
| small.city · `sq-grayrock-hall-errand` | giver | Erland Cinderridge (Grayrock Hall) | Agna Dripridge `rm_agna_dripridge`, Bridge Stores 8–18, lives in the Grayrock Cottages |
| small.city · `sq-timberland-court-errand` | target | Arne Wetworth (Timberland Court) | Wenna Timberland `rm_wenna_mossgrave`, the House's iron trade in the yard by the Court, out of doors 6–18 (a campaign target, Ch 6) |
| medium.city · `mq-sump-lamps` | giver | Edda Gullford (Lamp Store, 17–23) | Dorcas Rainford `rm_dorcas_rainford`, the Lamp and Wick 8–18, who sells the Lamp Store its oil |
| `lq-long-run` · answers | offer | Philippa Pitchstack (Administrative Wing) | Wynn Fogworth `rm_wynn_fogworth`, the Council Press 8–18; the tube is taken and brought back at the Couriers' Office door (`co_door`) |
| `lq-runnel-fever` | giver, offer | Theodora Wickley (Sump Infirmary); Ivo Fogstack (Washhouse) | Halvard Ashley `rm_halvard_ashley`, the honest apothecary at the Mortar and Pestle 7–19 (he held the line's "what's in it" choice already); Dirk Langstead `rmx_dirk_langstead`, rag-crier in the Rag Market, out of doors 7–18 |
| `lq-vanishing-thief` | giver, offers | Petra Vance, Roald Wickstack (Vance House); Silas Brinemore (Thornbury House); Hester Greyby, Palle Brinewell (Abbey School) | Eleanor Vance (Rag Mill) speaks for the House, Petra signs the notices; Emmerich Greyridge `rm_emmerich_greyridge`, in service to the Harrowgates, out on the Commons 6–21; Ragnar Roperidge `rm_ragnar_roperidge`, runs the Eastgate Stables 6–18; the thief Una Brineridge `rmx_una_brineridge` (18, Greywool 8–18) and her lookout Finn Dripwick `rmx_finn_dripwick` (18, the Sailors' Rest 7–18). The strongroom is looked over from its door (`vance_door`), the Sunken Street found at its Commons stair (`ss_stair`), and the bricked arch and its cache are by its far door, on the street (`old_drain`) |
| `lq-wreckers-light` | giver, offers, targets | Linden Sedgeditch (North Cove boat shed); Corwin Bilgestack (Lamp Store); Septimus Plankmarsh (Wharf Office) | Greta Gullton `rm_greta_gullton`, the Wharf's cranes 6–18; Dorcas Rainford (the Lamp and Wick); Florence Millhand `rm_florence_millhand`, unloads ships at the piers 6–18. The bearing at the Bay Light's foot (`bay_foot`), the oil at the Lamp Store door (`ls_door`), the dusk round watched at that door; in the reckoning Ilse Coalley waits on her Salt Row stair (`npc.move` + `npc.hold`, let go when the reckoning ends or the run does); the priest and the sergeant answer at the Chapel of the Tide's door and the Keep door, in scenes (`wr_priest`, `wr_sergeant`), and the naming is said at the chapel door |
| stage · `st-summons` | giver, target | Olaf Pitchmore (City Office); Isolde Gullwick (Hillside byre) | Delphine Brackwick `rmx_delphine_brackwick`, the Carters' Yard 8–18; Edda Ropeton `rm_edda_ropeton`, the Round and Rind 6–16, where Hillside's cart calls |
| stage · `st-bounds-water` | giver | Juniper Larkham (Rainward drying house) | Vesper Greenholt `cty_vesper_greenholt`, Rainwood coppice, out of doors 6–17 |
| stage · `st-twelve-bounds` | giver | Ada Coalstack (Heatherside hive shed) | Mogens Mudstack `cty_mogens_mudstack`, Rainwood coppice, out of doors 6–17 |

**Re-cast, the country towns:**

| Town · quest | Role | Was | Now |
|---|---|---|---|
| Coldwater · `sq-fox-at-the-hen-run` | giver | Ingram Hazelshaw (Oakhurst pig yard) | Flint Downcombe `tn_lf_flint_downcombe`, Coldwater Soapworks 7–19 |
| Coldwater · `sq-apple-nose` | giver, offer | Oriel Blossomby (Meresend apple store) | Laurel Pinefell `tn_fh_laurel_pinefell`, pot hand at the Glasswater Inn 7–19; the pup is brought to the inn |
| Coldwater · `sq-straw-for-the-pens` | giver | Rusk Downfield (Larkdowns barn) | Varg Acorndell `tn_oh_varg_acorndell`, Coldwater Looms 7–19 |
| Coldwater · `sq-strayed-ewe` | giver | Brock Marshholm (Larkdowns barn) | Marl Cragpeak `tn_fh_marl_cragpeak`, Coldwater Looms 7–19 |
| Coldwater · `sq-bread-to-the-landing` | target | Grist Reedmere (Glasswater boat shed) | Fennel Cornley, who sends the crews' bread down with the catch cart |
| Coldwater · `mq-glasswater-nets` | giver, offers | Grist Reedmere | Fennel Cornley `tn_sm_fennel_cornley`, Coldwater Stores 7–19 |
| Coldwater · `mq-meresend-blight` | giver, offers | Pell Harvestley (Meresend apple store) | Kindle Applemeadow `tn_sm_kindle_applemeadow`, Coldwater Still 7–19 |
| Coldwater · `st-ice-watch` | giver, offers | Garth Barleyton (Larkdowns barn) | Wald Larkwell `tn_lf_wald_larkwell`, Coldwater Still 7–19 |
| Coldwater · `st-haulers-food` | target | Thorn Mistmere (Upper Mere boat shed) | Laurel Pinefell, who sends the haulers' basket up |
| Larkfield · `sq-plumtree-badger` | giver | Fallow Plumfield (Plumtree apple store) | Vale Miremore `tn_mb_vale_miremore`, Larkfield Looms 7–19 |
| Larkfield · `sq-mill-race` | giver | Gorse Hedgewell (Longmead mill) | Eskar Cornley `tn_sm_eskar_cornley`, the baker at the Larkfield Bakehouse 7–19 |
| Larkfield · `sq-swill-for-southwood` | giver | Njal Rustwick (Southwood pig yard) | Rowan Chalkley `tn_lf_rowan_chalkley`, Larkfield Bakehouse 7–19, whose swill cart goes out at dusk |
| Larkfield · `st-feed-larkfield` | offers | Brand Sunham (Longmead granary) | Clover Snowhollow `tn_fh_clover_snowhollow`, Larkfield Print House 7–19 |
| Fenshore · `sq-fenside-pans` | giver, offer | Fallow Reedwell (Fenside boiling house) | Briar Greenshaw `tn_oh_briar_greenshaw`, peat cutter at the Fenshore cutting, out of doors 6–17 |
| Fenshore · `sq-boardwalk-plank` | giver | Yrsa Plankgate (Fen Shore boat shed) | Ludo Ashwood `tn_oh_ludo_ashwood`, pot hand at the Heron 7–19 |
| Southmeadow · `sq-festival-garlands` | giver | Willa Wheatcombe (Larkholm granary) | Briar Sedgebridge `cty_briar_sedgebridge`, digs clay at the Sallow pits, out of doors 6–17 |

Names: Briar Greenshaw, Ludo Ashwood and Briar Sedgebridge share a first name with campaign cast, as the only free
residents of their towns (decisions §1.2.5's exception); no line names them. Eleanor Vance shares hers with Eleanor
Saltcot, because she is the only Vance a wolf can reach; the lines call her "the Vance at the Rag Mill". The talk
library follows every re-cast (`artifacts/quests/build/talk2/build_packs.py`: the FIX map, the three id-gated window
lines of Halvard, Dorcas and Eleanor, and the garland line now Briar Sedgebridge's, plain), regenerated with `gen.py`.

**Kept, because a wolf can already reach them:**
- The patrol guards (Beatha Foggate, Frederick Rainham, Bertil Saltcot) walk the street patrol.
- The story itself moves or holds them where the wolf needs them: Corra Ropewick, Desmond Bilgeridge, Edric
  Harvestley, Ridge Eelbank, Vidar Ashhand, and now Ilse Coalley in the reckoning.
- Every evening at the Grey Gull, an inn whose door never locks: the couriers Florence Pennock (the timekeeper) and
  Hanne Ash, and the clerks Harriet Pennock and Lothar Abbot ("after work at the Grey Gull" by design).
- At the Grey Goose, which is open only in its cook's hours (7-19), so from the end of work until 19 and all day on a
  day off: the courier Grete Dimmock (the trial leg is taken from the board too, but handed in to her) and the Wharf
  Office clerk Grete Dunn (`mq-wharf-brother`'s "office" step now says where she stops). These are narrow windows,
  not closed ones. The time-sheet labels no longer send the wolf into the Couriers' Office by day.
- Met on a walk the quest itself tells: Kell Reedwade (`mq-logs-adrift`: "walks up from his boat shed at first light
  and at dusk") and Ragnar Wetby (`mq-linen-bolt`: the pig yard is pinged, "walks home to Larkfield at dusk").
- Ludo Beechshaw has no workplace; his home, a flat over the Fenshore pottery, is open in its hours.
- Bettany Nailley and Solveig Tarwell are now only in scenes at their own doors. (Bettany's wave G window line, gated
  on `lq_wreck=watch`, plays only if a wolf meets her on her walks.)

**Left by the second pass, closed by the third: 23 residents in towns with no free resident a wolf can reach** (every
one there already has a pack quest, or none is left). The story now unlocks their workplaces: see "Doors a story
unlocks" below.
- Southmeadow: Alder Blossomcroft (`st-chorus-wrong`), Emrys Stillshore (`sq-hare-in-the-seed-rows`), Hob Meadowfield
  (`sq-smoke-for-the-hives`), Myrtle Larkcombe (`st-relief-convoy`), Disa Fogford (`mq-bee-garth-bear`), Harl Oakbough
  and the cellar hand Briar Cornby (`mq-vineyard-cellar`), and the whole `lq-blight-and-boar` line (Sloe Harvestby,
  Aster Cornley, Harl, and Brisk Lakemere of Larkfield).
- Saltwell (no free resident at all): Fern Merewick (`mq-lost-raker`), Kite Bogditch (`mq-crossroads-names`), both
  also targets in `lq-salt-road`, Senna Applegarth (`sq-ochre-rabbits`), Winifred Fogridge (`sq-salt-for-the-road`).
- Fenshore (its two free residents now give the pans and the plank): Leif Rainley (`sq-twine-for-the-nets`), Stone
  Marshmore (`st-fog-lanterns`), Iris Pikeford and the fowler Nab Bogditch (`mq-withy-lights`), and `lq-eel-famine`
  (Ivo Sootridge, Jasper Greymore, Nab Bogditch).
- Coldwater: `lq-old-brown` (Jessamy Greyford, Vale Pikebridge, Juniper Larkcombe). Two free residents are left (Wren
  Coldpeak, Stone Pinefell), not enough for the line's three, and half a line re-cast is worse than none.

The second pass listed three ways to close these: an engine change, a story effect, or board-only quests. The
decision taken was the second kind with no new effect: `playerDoorLocked` skips a door a live story has unlocked.

Two optional objectives asked for archive work, which Ridgemere 3 gives nowhere: `lq-clipped-silver` "trail" (the
factors' chalk-list) and `lq-salt-road`'s three "ledger" steps. The third pass rewrote them (below).

**The checks after this pass:** `python3 tools/nh_check.py --all` **23 of 23 PASS**; the across-programs check (ids
unique, nobody a giver or choice holder in two programs, nobody a target in more than two programs outside their own)
passes; `ctest` talk_golden, talk_topics, talk_library, dialogue_trees, dialect_check, dialect_check_rules,
talk_lint, talk_lint_rules and talk_coverage pass, and talk_scorecard_quick passes with R2 refrozen (open problem 2;
its B1L timing gate, a 99th-percentile director look, is noisy on this laptop and fails some runs). A read-only pass
modelled on `playerDoorLocked` (each hour's open doors, merchants' hours, stairs open) confirmed every resident re-cast
above is reachable at work in every working hour, and that the regions at the doors lie on the street beside them. The
talk library regenerates byte for byte (`build_packs.py`, then `gen.py Data/Talk`). No database was written.

### Doors a story unlocks (third pass, 2026-11-22)
**The rule** (the DECISION, for the user to overturn; the engine side is built on branch `r3-nh-doors`):
- A door a live story has unlocked (`door.unlock`, `World::setDoorState`, undone at the run's end) is open to wolves
  while the story holds it, even where `World::playerDoorLocked` would lock it, until the story closes it
  (`door.close`, which leaves the door's own lock alone) or locks it, or the run ends.
- A story may never unlock a private home for wolves. Approval refuses a door whose interior (with the rooms up its
  stairs) is anyone's home and not a workplace.
- `tools/nh_check.py` (check 9) is stricter. It refuses a `door.unlock` or `door.open` on a door into anyone's home:
  a resident's or a post's `home_area`, whether the room itself or a room its stairs and passages reach. A shop with
  the keeper's flat up its stairs is refused too.
- It warns of a door into a room nobody works in, and of any `door.lock` (close the door instead). It needs the
  world, so `--no-db` skips it.

**What the programs unlock.** Each door is named by its street side (`link_<world.links.id>_a`, read only from
`world.links`) and unlocked as a phase entry effect (`ifRefused: skip`). That is the phase the quest is first offered
in: the start phase for a quest without `phases`, which is open in every phase of its story (`story_quests.open_in`). No door here has a home behind it, nor a home its stairs reach (a
read-only pass over `world.links`, `world.cells`, `world.interiors`, `live.npcs` and `live.profession_slots`).

| Program · phase | Door | Whom it lets a wolf reach |
|---|---|---|
| `nh-pack.small.south` · open | Larkholm Granary (`link_fs_grain_lark_granary_door_a`) | Hob Meadowfield (`sq-smoke-for-the-hives`) |
| | Larkholm Mill (`link_fs_grain_lark_mill_door_a`) | Emrys Stillshore (`sq-hare-in-the-seed-rows`) |
| | Dune Rabbitry (`link_fs_warren_dunes_rabbitry_door_a`) | Leif Rainley (`sq-twine-for-the-nets`) |
| | Redsand Rabbitry (`link_fs_warren_waste_rabbitry_door_a`) | Winifred Fogridge (`sq-salt-for-the-road`) |
| | Ochre Press House (`link_fs_vineyard_ochre_press_door_a`) | Senna Applegarth (`sq-ochre-rabbits`) |
| `nh-pack.medium.south` · open | Sunfields Hive Shed (`link_fs_bees_south_hives_door_a`) | Disa Fogford (Q13) |
| | Southmeadow Press House (`link_fs_vineyard_south_press_door_a`) | Harl Oakbough, and Briar Cornby in its cellar down the stairs (Q14) |
| | Fenside Boiling House (`link_fs_salt_fencoast_boiling_door_a`) | Iris Pikeford (Q15) |
| | Withy Creek Eel House (`link_fs_marsh_eels_eelhouse_door_a`) | Nab Bogditch (Q15) |
| | Saltpan Salt Store (`link_fs_salt_saltpan_store_door_a`) | Fern Merewick (Q17), Kite Bogditch (Q19) |
| `lq-blight-and-boar` · open | Southmeadow Press House, Sunfields Hive Shed, Southwood Pig Yard (`link_fs_pigs_south_sties_door_a`) | Sloe Harvestby and Harl Oakbough, Aster Cornley, Brisk Lakemere |
| `lq-eel-famine` · open | Fen Shore Boat Shed (`link_fs_fishing_fencoast_shed_door_a`), Rivermouth Boat Shed (`link_fs_fishing_delta_shed_door_a`), Withy Creek Eel House | Ivo Sootridge, Jasper Greymore, Nab Bogditch |
| `lq-old-brown` · open | Greyteeth Shearing Barn (`link_fs_sheep_foothills_barn_door_a`) | Jessamy Greyford, Vale Pikebridge, Juniper Larkcombe |
| `lq-salt-road` · open | Saltpan Salt Store | Fern Merewick, Kite Bogditch (`brackish`) |
| `lq-salt-road` · reckoning | The Records Office (`link_r2_the_records_office_door_a`) | the copyist Hollis Shaw (the "ledger" steps) |
| `lq-clipped-silver` · open | The Records Office | the clerk Harriet Pennock (the "chalk" step) |
| `nh-pack.stage` · s4 / s56 / s78 | Larkholm Granary / Fen Shore Boat Shed / Larkholm Granary | Alder Blossomcroft (`st-chorus-wrong`) / Stone Marshmore (`st-fog-lanterns`) / Myrtle Larkcombe (`st-relief-convoy`); closed again (`door.close`) at s56 / s78 / s9 |

**When a window ends: `door.close`, never `door.lock`.** The map locks no door (`world.links` has no lock, so
`Door::locked` is false everywhere), and `Door::locked` stops residents too: locking a shed would shut its own workers
out of it for the rest of the run. `door.close` of a door the story opened to wolves closes it to them and leaves its
own lock alone (doc 86 §7.3, engine on `r3-nh-doors`), so its workers still go in. Where a window is shorter than the
run, the program closes it so:
- `nh-pack.stage`: `s56` closes the Granary, `s78` the Boat Shed, `s9` both (in case a phase was skipped). The stage
  quests last their phase (`lifetime: phase`), so no wolf is still on one when its door closes.
- Every other door serves a quest open in every phase of its story, to the last (a quest without `phases` is, and so
  are `lq-salt-road`'s road quests, for the Records Office). Their window is the whole run: they stay unlocked until it ends, when the game puts back the lock each had
  before the story touched it (`Game::endStoryWorld`). They are workshops, never homes.

**The archive steps, rewritten.** Ridgemere 3 gives archive work nowhere, so both steps became a conversation with a
named Records Office clerk. Each clerk is reachable at work while the story holds the Records Office open (7-18, up
its stairs, no home behind it), and at an inn after work. What the clerk finds is a journal note, in the plain speech
of Ridgemere's clerks (`live.npc_voice`: Ridgemere, light):
- `lq-clipped-silver` `trail` `chalk` (optional): ask the Records clerk who keeps the old ledgers, Harriet Pennock
  (`rmx_harriet_pennock`, the Grey Gull every evening), for the factors' chalk-list. The trigger `chalk-read` notes
  that Silver and Rain is chalked, owing more than the shop takes in a year, due at the new moon (the design's "shows
  she owes").
- `lq-salt-road` `ledger` (optional, hidden, in all three road quests): ask the Records Office copyist, Hollis Shaw
  (`rmx_hollis_shaw`, the Grey Goose after work until its cook locks up at 19), to copy the provisioner's buying out of
  the rolls. The trigger `ledger-read` notes three times last year's Saltpan salt at Vane and Daughters, with duty paid
  on a third of it.
- Harriet Pennock is now a target in two quests (Q6 of `nh-pack.medium.city` and the trail), and Hollis Shaw in one
  line. Neither gives or holds a choice anywhere. The rehearsals that did archive work now talk to the clerk after
  taking the quest.

**The checks after this pass:**
- `python3 tools/nh_check.py --all`: **23 of 23 PASS**. Every door above is reported as "a workplace of N, no home
  behind it".
- A program unlocking the Southmeadow Vineyard House (Sloe Harvestby's home), or opening the Larkfield Press (the
  keeper's rooms up its stairs), is refused, and a `door.lock` is warned of. This was tried on a copy outside the tree.
- No database was written.

### Open problems
1. **Givers behind locked doors (closed 2026-11-22, third pass)**: the 23 residents are reached at work through the
   doors their stories unlock (above). If the user overturns the DECISION, they are met only on their walks again,
   and the second pass's other choices (board-only quests, a catalogue effect) apply.
2. **22 window lines (closed 2026-11-22).** The large lines' notes wrote 22 lines "for wave G"; they are now in the
   trees (`build_packs.py` `NOTES` and `ALSO`; new `hub` and `watch.hub` nodes in `gen.py`), spoken by the re-cast
   residents where the line's speaker moved. They stay silent until C8 lands or the DM seeds the keys. Adding them moved
   `talk_scorecard_quick`'s R2 (the number of say lines in the hub reseeds the run: 18 dummy lines gated on a resident
   nobody is give the same value), so R2 was refrozen with that reason. The lines: black-hearths 10, eel-famine 4,
   lines 5–8 7, vanishing-thief 1 (`artifacts/quests/build/talk2/notes.md`).
3. **Today's engine limits, accepted as they are:**
   - A story can't take goods out of a pack without the "may act on players' wolves" permission. So Q2a's tally paper
     and Q3b's candle stay with the wolf.
   - Saltwell's shared meal needs someone already eating nearby. The bowl bought at the inn is the reliable path.
   - The stage bounty depends on natural camps forming in its nine cells.
   - "A loaf each to three wolves" can be met by giving one resident three loaves.
   - In the Runnel Fever, if the soakaway wins after a wolf asked for the cover, the city keeps asking for the cover.
   - Q11's first witnesses give no clue until the DM seeds `lq_hearths=undersold`.
   - Rehearsals can't set world variables, so `nh_b_peace` in quests 46–48 was checked by reading.

### Wave J as built (2026-11-24, branch `r3-nh-live`)

The engine phases (1a to 9, `r3-nh-p9`), the story doors (`r3-nh-doors`) and this content (`r3-nh-content`) merged on
`r3-nh-live`; wave J then moved the 21 programs onto what their notes called "later". Each program's notes file has a
"Wave J (as built)" section with its own list and what is still left. Made by scripts kept out of the tree (as the
content waves' were); nothing was approved, published or written to a world.

| Feature | Where |
|---|---|
| top-level `prefix` | all 21: `sq` the small packs, `mq` the medium, `lq` the large lines, `st` the stage pack, `coin` nh-purses, `nh` the cleanup |
| J1 per-wolf choices | every `npc.offer` laid by a wolf's own step (a participant trigger on a wolf's event) goes to that wolf: 41 in the large lines (the packs' were already); the choices laid by a clock, a day or a signal stay every wolf's (there is no acting wolf) |
| C8 talk keys | nh-purses `nh_coin=thin` by town; the small packs' `sq_on` (as each opens, renewed by each errand) and `sq_last` (2 days, per errand); the medium packs' open keys (realm, as the pack opens) and ending keys (town, 90 days, a `qN-talk` trigger beside each ending); the large lines' state key at each phase's opening, the three thin-purse day keys, `lq_rupert`, `lq_rustlers`, `lq_beam`; the stage pack's open keys by phase, `st_last` per small quest and the medium endings' keys (`<x>_talk` triggers). `nh_check --all` now finds **no pack key the library reads that no program writes**, and no memory written that no line reads |
| C5 thanks by name | north 26 and 29 (Tristram, Ebba); south's garland rope (Briar, to every helper); the medium packs' 23 closes (the follow-up choice became the giver's greeting); Ivy and Vigdis; the stage pack's five medium closes and Ingram's |
| C6 `requires` / `during` | every large line's closure roads need its first or second chapter done; medium Q15 (`nh_b_lights` not `laid`) and Q18 (`lq_salt` empty or `raided`); `during` from four city givers and three stage givers |
| J3 closure | 26 roads over the 11 large lines (at most 3 a line), each `"line"`, each after a chapter of its line |
| L2 named camps | every `story.area.spawn` names its camp `<prefix>_<region>`; the camp triggers and `bandit falls` objectives of those cells match the camp's id (the rehearsals carry it) |
| M2 purse operand | every story grant's If (127) also asks the purse itself for 60 and the amount; nh-purses' switches stay for their 60/80 hysteresis |
| C1 values | the small south pack's and the large lines' grants pay `min(amount, purse / 20)`, the counters adding what was paid |
| C2 scaled goal | the garland rope, `max(6, 2 x active)` |
| N4 `join.again` | small 3, 23, 25 (its own weekly gate dropped), 30; medium Q4, Q12, Q17, Q18, Q20; the stage pack's five weekly medium quests |
| SQ2 tills | the cooper's till for Q6 (cap 19), Hollis Physic's for the draught money (cap 9), the Greatly Bank's for the Counting House chit (cap 16); nh-purses watches and counts the two new tills, and `world-vars.json` declares them |
| J9 story items | the City Office letter, the Carters' Yard crate, Greywool's bolt, the Mirebridge names, the Long Run's tube, the Moot summons |
| C10 foes | named leaders with the medium waves (five) and the Salt Road's and the Old Brown's bands: The Brine Captain, Sleet the Drover |
| L1 `hunt.stage` | medium Q13's bear (the hives), the Old Brown at the tarn, the boar in the rows (that hunt now required) |
| J5 howls | the stage pack's mended chorus: Alder howls on the square |

- **Rehearsals:** each still passes; where a new trigger fires, a new effect is sent or a payment moved purse, the
  case's expected triggers, effects or numbers were updated only when that was the whole difference (the scripts
  checked it), and every case gives the purses it pays from 2,000 pennies (`facts.purses`). Three cases changed by
  hand: two took a quest again inside the week (N4 now refuses that, so the event is gone), and the festival's rope
  is six with three active wolves.
- **Behaviour that changed:** a large line's road is offered only to a wolf who did one of its first two chapters (J3's
  rule), so a wolf who only stood a night watch is not offered one; the medium waves' objectives count only the
  story's own camp, never a natural camp in the same cell (decisions 4.3); a poorer store pays less (C1).
- **The checker** (`tools/nh_check.py`, check 10): the prefix by folder, the purse guard on every grant, and the talk
  keys both ways (warnings). `--all`: **24 of 24 PASS** (the 21 programs, `world-vars.json`, the library and the new
  "talk keys" report), with no warning.
- **Left** (in the notes): the shop tills of the small city's errands (14 tills; the economy has not been asked),
  `item.place` takes no story mark (the tufts, the tally, the float lamp, the stake), the clock-laid choices stay
  every wolf's, L3 objectives where a trigger credits more wolves than an objective would, C2 for the Long Run (its
  `contributors` is a whole number), C12a/J2 (no chapter to tell yet: waves B, H and I are not written), C14, L4, M3.

---

## The campaign as built

**Built 2026-11-24** on branch `r3-nh-live`: waves **B, H and I** (the nine chapters) written and each reviewed once by
its own pass (each chapter's `.notes.md` keeps its review), then this campaign check across all of them. Nothing was
approved, published or written on `ratw_ridgemere3`, `ratw_dev` or `ratw_prod`; the approvals and the end-to-end ran on
a throwaway copy, dropped after.

### Files and counts

Under `Data/Storylines/ridgemere3/ninth-howl/` (each chapter has its rehearsals inside; the generators the authors and
reviewers used are outside the tree, so the JSON in the tree is the source now):

| Ch | Program `ninth-howl.<n>` | Phases | Variables (of 64) | Campaign triggers | Most in a phase (of 64) | Quests | Rehearsals | Size |
|---|---|---|---|---|---|---|---|---|
| 1 | Where Is the King? | 3 | 64 | 54 | 60 | 5 | 6 | 214 KB |
| 2 | By the New Moon | 5 | 64 | 44 | 63 | 6 | 7 | 288 KB |
| 3 | Two Long, One Short | 4 | 63 | 46 | 58 | 10 | 16 | 292 KB |
| 4 | The Nine Howls | 3 | 63 | 39 | 63 | 14 | 8 | 419 KB |
| 5 | Letters for an Absent Lord | 2 | 64 | 48 | 64 | 13 | 7 | 290 KB |
| 6 | The Lights in the Fen | 6 | 64 | 46 | 58 | 8 | 8 | 302 KB |
| 7 | Harvest Under Arms | 4 | 64 | 28 | 63 | 10 | 10 | 393 KB |
| 8 | The Bell and the Writ | 2 | 64 | 49 | 62 | 12 | 11 | 389 KB |
| 9 | The Moot Night | 10 | 64 | 40 | 55 | 5 | 6 | 515 KB |

- The acts: **Act I "The Lamp"** = Ch 1–3 (campaign `ninth-howl.act1`), **Act II "The Howls"** = Ch 4–6
  (`ninth-howl.act2`), **Act III "The Moot Night"** = Ch 7–9 (`ninth-howl.act3`). Inside an act each chapter starts the
  next (`story.start`, the same wolves, `carry`, when it has ended); an act's last chapter (3, 6) waits in its interlude
  for the next act's first chapter to be published; Ch 9 starts none.
- `world-vars.json`: **116** world variables (the 18 fallback lists of campaign §4.6 for an engine without realm
  variables are gone: J2 landed, and nothing read them). `nh-purses.json` and `ninth-howl.cleanup.json` as in wave A.
- Notes: `ninth-howl.{1,2,3,5,7,8,9}.notes.md` (Ch 4's and Ch 6's reviews are in their reports and below) and
  `notes.md`.

### The campaign check: across the chapters

Scripts in the session's `/tmp/nhcc/` (out of the tree). What was compared, and what it found:

- **Variables.** Every name across the 9 chapters, the 21 other programs and `world-vars.json`: one type and one
  scope for every world and realm name; every world variable a program declares is in `world-vars.json` with its type.
  The story and per-wolf names two chapters of one act share all have the same scope and type, so `carry` hands every
  one of them on (the runner drops a mismatch without a word: `_carried`). Across an act boundary only the world and
  realm values go on, and no chapter of Act II or III expects an Act I story or per-wolf value (the one name that
  recurs, `p_town_view`, is worked out afresh by each chapter). The realm variables (`p_voice`, `p_howl`, `p_backs`,
  `p_advice`, `p_marks`, `p_sides`, `p_saw_desmond`, `p_pledge_ch`, `p_back_ch`, `p_knows_df_letter`) are one type in
  every chapter that declares them, as `campaign.approve` and approval's `realm_clash` require.
- **Talk keys.** The ten values of the realm key `nh_stage` (`king`, `moot_called`, `reckoning`, `signals`, `howls`,
  `letters`, `lights`, `harvest`, `bell`, `moot`) are each set by their chapter in order, and every value the library's
  gates name (`world.mem.nh_stage=…`, 1,596 gates) is one of them; the world variable `nh_stage` is set to 1–9 by each
  chapter's opening, and the stage pack's `stage_hour` follows it. `nh_check --all` reports no key read that no program
  writes, and no memory written that no line reads.
- **Givers.** At most two `npc.offer` per resident per chapter (decisions §1.3; the most is two, Lorn in Ch 3 and Fenwick
  in Ch 4); no resident gives more than one pack quest; no two programs share an offer id, a scene id or a story item.
  One quest id, `door`, is in both Ch 2 and Ch 9: a quest lives in its run, and the two never run together.
- **Protection.** `ninth-howl.cleanup`'s 40 are exactly Ch 1's `nh_cast` and Ch 9's epilogue's unprotect list. The cleanup
  needs no `npc.unspare`: a spare lasts the campaign, so stopping an act ends it, and one story cannot end another's.

**Found and fixed** (the chapters' files, the library and one tool):

1. **Act I kept running beside Act II.** Ch 3's last phase `end` left the campaign triggers on, so every lamp night,
   Glasswater wave and deed rule of Ch 3 would have run on through the next act. It now has `campaignRules: false`.
   And an act's run never finished at all, which also held Desmond Hollis for Act I for ever: Ch 5, 7 and 8 hold,
   move into scenes and outlaw him, and a resident another live story holds refuses all of that
   (`Game::heldByAnotherStory`). The act runs stay alive on purpose (their spares, `lasts: campaign`, keep the old cast
   through Act II, which spares only its 24 Keepers; their talk not kept stays), and now:
   - Ch 3 `end`: `act3_free` lets Desmond go when `ninth-howl.7` starts (Ch 4 and Ch 6 never hold him, so Act I's hold
     is the one that keeps him in the cells through Act II), and `act_over` finishes the run when `ninth-howl.9` ends
     (`story ended`, any ending);
   - Ch 6 `ch6_end`: `act_over` the same;
   - Ch 7: `desmond_again` lays the muster's hold on him again an hour in, since Act I may let go only after Ch 7's first
     minute.
2. **A howl out of doors in a country town is the wild's.** The game's town of a tile is the town's own region
   (`World::townOf`), and a community's streets, green and square are the realm's region (greenholt, sunfields …), so
   `chorus closed` and `howled` say `wild` there; only Ridgemere's streets are Ridgemere's. Ch 4 had three Keepers
   (Southmeadow's, Frosthollow's, Oakhaven's logger) waiting on their squares at dusk, so those three howls could never
   be found and The Scattered never finished. They now wait in their towns' inns (the Apple Tree, the Frozen Lantern,
   the Acorn Inn; the labels and hand-ins say so), where a howl is the town's. The Well's Roll's howl is now "on the
   Saltwell square" (`where` its post region). Ch 5's late howl counts a chorus under the town's roof or on its square
   (its town regions), split into `late_howl` and `late_howl_b` for the action cap. Ch 9's review had already done the
   same for the Moot; Ch 7's warnings were already by region.
3. **Figures known nowhere.** Ch 7 named the Grey Captain (`nh_road_villain`) and the ford's hero (`nh_ford_hero`) as
   known in `greenholt`, which no resident is from or stands in by name (`region:<from>` or `region:<here.region>`), and
   `nh_road_villain` sorted before Ch 2's `nh_villain` anyway (the last known figure of a role wins). Now
   `nh_villain_coldwater|larkfield|frosthollow` (Ch 3's, renamed Eskar of Deepvein) and `nh_hero_larkfield`.
4. **The talk library.** The granary line for the dismissed guard was gated on `nh_desmond=mercy` (set at the hearing,
   before the nights): now `redeemed`. His window choice "You saw the Chancellor weep" and his "Greatly pennies" line were
   gated on `nh_desmond=held`, which Ch 7 always replaces: now `held|pillory|mercy|redeemed`. The `nh_lists=captain` line
   (three places) said the lists "went to the grey captain"; `captain` is the Watch Captain who burns them: "The Watch
   Captain burned the lists in front of the sergeants, they say. Nobody on them pays twice now."
5. **Ch 1's stage key** lived 30 days, as long as the chapter's soft maximum, after which it waits for the admin: now 90.
6. **The stage pack**: Vesper Greenholt (plain speech) said Frosthollow's "keep your lamp lit": "Mind your footing on the
   high paths." (approval's voice warning).
7. **The Campaigns console could not show Chapter 1** (found by the end-to-end): `story_runner.directed` added each
   effect's `npc` to a set, and Ch 1's `npc.protect {"each": true}` is a dict, so every `status` read of the run failed
   ("Malformed request"). Residents given by an operand are left out now (`test_story_console.DirectedResidents`).

8. **Residents sent onto walls** (found by the end-to-end: Ch 2 paused in its first hour). The copy's game was asked to
   move a resident to every tile any program names for `npc.move`, `npc.walk`, `visitor.add`, `foe.add`, `item.place`
   and `npc.post` (164 tiles): 13 were refused, "No one can stand there", and each now names the nearest tile one can
   stand on (never a door's): the Palace doors (Ch 1's Queen and herald, Ch 2's and Ch 4's Ottilie, Oda and Gerrit:
   (52|55|81,145), the doors being at (51,146) and (80,146)), the Greatly House door (Ch 2's and Ch 4's Rosalind and
   Edric: (173|176,190)), the Carters' Yard (Ch 2's Desmond, (176,35)), the Pass House stranger (Ch 5, (7,6)), the
   Pass road stranger (Ch 8, 9_3 (6,21)), and three in the packs (`nh-pack.medium.north`'s Tristram and his foe at
   4_4 (211|212,186), `lq-old-brown`'s foe at 11_5 (13,245), `lq-salt-road`'s walk to 13_12 (95,238)). Ch 1's
   `cast_out` had skipped its two, so the Queen was never reached at the door. `nh_check` has no terrain to test this
   with; the probe is the session's `/tmp/nhcc/e2e/probe_all.py`, run against a game on a copy.
9. **A refused resident effect paused the run.** Ch 2 (32 effects) and Ch 3 (11) still had moves, holds, walks,
   follows, greetings, items and doors without `ifRefused`, so a resident away, in a fight or dead stopped the chapter
   for every wolf until the admin acted. They skip now, as Ch 4–9's already did. (`npc.offer` and `npc.line` take no
   `ifRefused`; they pause as before.)

**Left as the chapter reviews left them** (each in its notes): Southmeadow's Keeper reachable for pledges only while a
pack holds her granary open; Oda's "how shall we pay" window line shown to every wolf; "three gossips" countable on one;
Ch 5 never reads `p_knows_df_letter`; no chapter reads `nh_b_loan` or `nh_b_spy`, and Ch 3 neither `nh_b_ledger` nor
`nh_debtors_freed`; Ch 9 neither `nh_b_ford` nor re-holds Desmond; a scar's +4 to a town's mark and the raise marks
scaled by active wolves are not built; Oakhaven's howl quest is always given by Ulla (a giver from a variable would
crash `nh_check`); the shop Keepers sing in their shops; `item.place` takes no story mark; walked residents stay put
until their phase ends.

### Approval on a copy of Ridgemere 3

`ratw_ridgemere3` dumped read only into a throwaway `ratw_test_nhcamp_*`, migrated there through 0081 (0080 and 0081
were the only new ones), the 116 world variables declared there with `nh_world_vars.py --apply`, then everything
approved through the Dungeon Master tool's route (`POST /api/story-program`) by an owner account of the copy:
**24 of 24 approved**: `nh-purses`, the seven packs, the eleven large lines, the stage pack and `ninth-howl.cleanup`
(op `approve`), and the three acts (op `campaign.approve`, each chapter rehearsed and its references checked on the
copy). The warnings: the stage pack's Vesper speaking Frosthollow's words (fixed, above); `lq-blight-and-boar`'s boar
staged on crop and grass ("staged there all the same", as its notes accept); and, for Act II and III, that `p_howl`,
`p_backs` and `p_advice` are told to residents by more than one approved chapter ("the newest run's value is the one
they know": the same value, carried as a realm variable, so harmless). On a hosted world a builder or the owner
approves (the figures and relations above).

### The end-to-end of Act I

On that copy (accounts and wolves taken out, as the slice's end-to-end did: a password server and a ticket save never
mix): a game server from this branch at **60 times speed** (`--speed 60`, so a chapter's minimum game days pass in
real minutes; `--dev-identity`, `--dev-tools` for `visit` and `grant`), the real story runner as its own process, the DM
tool's route for every admin step, and three wolves (Ash, Birch, Cedar) on real game connections (the client's own
`Connection`). Scripts in the session's `/tmp/nhcc/e2e/`, out of the tree. It ran twice: the first run found the wall
tiles and the console's crash, the second ran on the fixed tiles.

- **Published** `nh-purses`, `nh-pack.small.city`, the stage pack and Act I (its campaign ticket). **Chapter 1 opened**:
  `nh_stage` 1; `npc.protect` and `npc.spare` (`lasts: campaign`) applied on all 40 of the cast; the Records Office and
  Soup Kitchen doors open to wolves; the realm talk key `nh_stage = king` (90 days); the bells narration ("Bells toll from
  the Palace. King Gunnar has gone up to the Bright One."); the stage pack moved from `quiet` to `s12` on hearing
  "Story started: ninth-howl.1", and Winifred Cinderworth's window offered its stage quest "Baking for the wake?" while
  Frida Netherby's stage-3 quest stayed shut. In the second run the Queen and the herald stood at the Palace doors at
  hour 10 (`cast_out`, applied at (55,145) and (81,145)).
- **A deed on the rungs.** Each wolf took the Summons in Brenna's window ("Is there word from the Palace?", "Give me one.
  I'll carry it."; 8 tagged letters each) and gave one to Ridgemere's Keeper and two far towns' Keepers (9 `gift`
  events). **Spark**: the voice ticked (realm `p_voice` 9, 8, 8), a deed for each first town ("carried the herald's
  Summons to Saltwell", 12 deeds), greetings laid with Brenna and Cressida for each wolf. **Ember**: within the hour
  Saltwell's strand passed its first step: the board post "Saltwell's howl is stirring. (signed) The Keeper of the
  Saltwell howl." and the narration on its square (`nh_steps` `saltwell-1`); the third of the bar posted "The Summons
  is on the roads." on all eight boards. **Blaze**: at the close (below).
- **A side story closed.** Ash took the Queen's Purse in Oda's window, walked in at the Records Office door (`enter
  place`), spoke to the clerk (a typed line: `conversation`), gave two bread and two herbs to a diner in the Soup
  Kitchen, and handed it in to the Queen: the quest done, the deed "found the old household tolls for the Queen", and
  **the closure counted**: "A finished Story: The Queen's Purse (3 social)." (`story.closure`, kind `side`).
- **Participation moved Chapter 1 on.** All three wolves were in `ch1_did` (3 of 3 active; 30% with a floor of 3), and
  at 10 game days in the chapter the run moved by itself (`auto`) to `ch1_closing`, its readiness "3 of 3 active wolves".
- **The Word and Chapter 2.** The Herald's Word came at the next dusk at least three hours on. The bar was short (25 of
  42 points), so the worse road played: the Council's notice and the herald's answer on all eight boards, the three
  wolves named Lamp-bearers on the Ridgemere board, the figure `nh_ruler` (Dagfinn Thornham, keeper of the Palace lamp)
  written to `live.figures`, the Moot called (`ch1_called`); half a game day later the chapter finished (success) and
  **started Chapter 2 for the same three wolves**, their realm values loaded (`p_voice` 15, 8, 8); `nh_stage` 2; the
  stage pack heard "Story started: ninth-howl.2" and "Story ended: ninth-howl.1".
- **The admin's Advance.** On Chapter 2, not ready (no debtor freed, 0.02 of 12 days), the route's command `advance`
  `{when: now, anyway: true}` with a reason: "Advanced to ch2_closing now anyway (not ready: 1 of 3 parts met) …",
  audited (`story.advance`, by the owner, with the reason), and `nh_pushed` became `["ch2"]`. The Reckoning came at the
  new moon ("THE RECKONING IS AT DUSK. The new moon rises on the Counting House's day, as it promised." and then "What it
  found is on every board" on the boards), the chapter finished at `ch2_called` and **started Chapter 3** for the same
  wolves: `nh_stage` 3 and the realm key `nh_stage = signals` (the keys went `king`, `moot_called`, `reckoning`,
  `signals`). In the second run the closing first paused on an `npc.release` of a resident the chapter was not holding
  (the Advance came between `cast_out` and `cast_in`): finding 9, fixed in the files after that copy's approval, so the
  admin skipped the five refused orders from the console (command `order`, `skip`) and the chapter went on.
- **The stage pack followed the stage.** It sat in `quiet` until `nh_stage` was 1, then in `s12`; when Chapter 3
  started it moved to `s3`: Winifred's "Baking for the wake?" was gone from her window and Frida's "Is the yard selling
  stone for the posts?" was in it. `nh-pack.small.city` ran beside it (its errands gated on stage 4 and 6 were not
  reached).
- **Found and fixed** by it: Chapter 2 paused in its first hour (Rosalind sent onto a wall: finding 8), every Campaigns
  console read of Chapter 1 failed (finding 7), and a release with nothing to let go paused Chapter 2's closing (finding
  9). The second run passed the first two on the fixed files; the third is fixed in the files committed.
- **Approved again** after the last fixes, on a fresh copy migrated through 0081 with the world variables declared:
  **24 of 24**, the same warnings.
- **Not seen**: the full Blaze (the bar met: the herald's own Word, a post standing), a camp fought at Alder Mere, a
  pledge at a Keeper, and anything of Chapter 3's play; the rehearsals cover those. The wolves were moved by the DM's
  `character.move` and the dev `visit` (to reach residents across the realm in minutes), so the walking itself is not
  what was tested.

### Going live on Ridgemere 3, in order

1. **This branch** merged into `ridgemere2` and pushed (the engine phases, the doors, the content and this campaign
   check are one chain on `r3-nh-live`); the game server rebuilt and the laptop's world restarted
   (`Saved/WorldAgent/run.sh`, port 7788; publishing a build restarts it); the story runner restarted
   (`tools/story_runner.py --world ridgemere3`); the Editor and a client after 0.1.10 released from it.
2. **Migrations 0080 and 0081** on `ratw_ridgemere3` (`world_db.migrate`; it is at 0079). Every chapter declares realm
   variables, so no act approves without 0081.
3. **DM preparation** (§10.1), before Act I:
   - the allow-listed words (`tools/dialect_allow.json` is in the tree; T9);
   - **declare the 116 world variables**: `python3 tools/nh_world_vars.py --apply --world ridgemere3 --who <account>
     --reason "The Ninth Howl, doc 86 10.1"` (or Campaigns > World variables from `--steps`); `nh_grace` among them;
   - the cast is protected and spared by Ch 1's setup (C11 landed: no authored `protected` flag to set);
   - figures and talk keys are set by the chapters themselves (C8, C9 landed): nothing to seed by hand;
   - `warden_order`, with Gorm Plankgate a ranked member, before Ch 5's close; `nh_b_tobias` and `nh_b_julian` are
     declared with the rest;
   - leave Dagfinn's post and proj-1 to proj-3 alone; re-post any Line project that lapsed.
4. **Approvals**, by a builder or the owner on a hosted world (every chapter but Ch 4 sets a figure; Ch 2, 3, 5, 6, 8 and
   9 move faction relations, Ch 7 outlaws a resident, `lq-wreckers-light` adjusts standing): `nh-purses`, published
   first and left running; the seven packs, the eleven large lines and the stage pack, published (the stage pack sits in `quiet` until `nh_stage` is set); `ninth-howl.cleanup` approved, not published.
5. **Act I** as one `campaign.approve` (`ninth-howl.act1`, "The Lamp": Ch 1, 2, 3), published from the Campaigns console.
   It runs on by itself: each chapter moves on when ready (participation and time, `auto`), or when the admin presses
   Advance (Advance anyway… before it is ready), and starts the next.
6. **Act II** (`ninth-howl.act2`, "The Howls": Ch 4, 5, 6) when Act I waits in its interlude and the DM chooses; then
   **Act III** (`ninth-howl.act3`, "The Moot Night": Ch 7, 8, 9) the same way. Act I's and Act II's runs stay in their
   `end` phases (no rules running) until Ch 9 ends, then finish by themselves.
7. **Running it** (§10.3): the Campaigns list once a real day; answer each "ready to push"; `ch<n>_auto` and
   `ch<n>_wait_bar` on the Variables tab (or by the notices `nh_ch<n>_auto` / `nh_ch<n>_wait_bar`). With 5 wolves the
   floor (2 or 3) is what counts. If an act must be abandoned: stop its run, and publish `ninth-howl.cleanup`.
8. House purses stay off (M3's economy check has not been asked for).

### The checks, as run at the commit

- `python3 tools/nh_check.py --all` on `ratw_ridgemere3` (read only): **33 of 33 PASS**; its warnings are the known two
  (the world has no 0081 yet, so references read the realm variables as per-wolf ones; Ch 1's Soup Kitchen door has
  nobody working behind it). Every chapter's rehearsals pass with nothing uncovered.
- `tools/story_words.py --stories Data/Storylines/ridgemere3`: 30 programs, 0 problems, offline and with the world's
  voices (the dm role, read only). `tools/nh_world_vars.py`: 116 variables, PASS. `nh_check --talk` PASS; the talk
  library regenerated from `artifacts/quests/build/talk2/` reproduces the tree exactly.
- Python: `test_story_console` (with the new `DirectedResidents`), `test_story_campaigns`, `test_story_program`,
  `test_story_words`, `test_story_effects`: all OK. ctest's talk tests, `dialogue_trees` and `story_trigger_parity`:
  all pass but `talk_scorecard_quick`, the timing gate (B1L, a director look's 99th percentile) with the 60x server
  beside it; alone it passes, two runs of two.
- No C++ changed, so the slice's ctest stands.

## Live on Ridgemere 3 (2026-11-24)

- `r3-nh-live` merged into `ridgemere2` (`14d5067a`) and pushed; central deployed at `14d5067a`.
- `ratw_ridgemere3` dumped first (`Saved/Backups/ridgemere3-before-0080-0081-20261124-133556.dump`), then migrations
  0080 and 0081 applied. The server, the client and the Editor rebuilt; the laptop's world and its story runner restarted.
- The 116 world variables declared (`nh_world_vars.py --apply`, by the DM account `ninth-howl`, an owner made for this;
  its password is in the gitignored `Saved/Config/ninth-howl-dm.txt`, the tickets in `Saved/Config/ninth-howl-tickets.json`).
- **Approved:** all 24 (`nh-purses`, 7 packs, 11 large lines, the stage pack, `ninth-howl.cleanup`, and the three acts as
  campaigns). **Published and running:** `nh-purses`, the 7 packs, the 11 large lines, the stage pack (moved to `s12` as
  Chapter 1 set `nh_stage`) and **Act I** (Chapter 1 in `ch1_main`). The game applied every effect, none refused.
- **Waiting for the DM:** Act II and Act III (approved, not published; publish each from the Campaigns console when the
  act before waits in its interlude); `ninth-howl.cleanup` (approved, published only to abandon an act); the
  `warden_order` faction with Gorm Plankgate a ranked member, before Chapter 5 closes; a client and Editor release after
  0.1.10 (the Campaigns console's readiness bars, foes panel and the new workbench pickers are in the desktop Editor only
  then; the browser client and the laptop's DM tool already have them). House purses stay off (M3's economy check).
