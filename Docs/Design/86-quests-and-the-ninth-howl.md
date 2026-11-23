# 86. Quests and the Ninth Howl: quest packs, the first grand campaign, its cast and talk, and the triggers they need

Draft, 2026-11-22. A design; nothing is built yet. Surveyed at `17f6db3b` on `ridgemere2` (the Story Maker as doc 85
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
  moves the story on (an exit with `minQuantity: 3`); the bear killed at the tarn ends the quest. It rehearses clean: nothing
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
- **Going live** on `ratw_ridgemere3` needs no migration; the game server restarted from this branch (`npc.howl`, the howl
  events, `hunt.stage` and the staged hunt, `story.watch`), the story runner from this branch with the rebuilt
  `libratw_story.so` (a residents count needs both twins), the Editor released from it. The client needs no change.
