# 48. Playing together: choices that matter, being known, and goals that need each other

Drafted 2026-10-06 from the user's notes on the engagement research (the research itself is the appendix), and
updated the same day with the user's answers (Decisions, second round). Nothing here is built. Read docs 08 (social
progression), 32 (parties, Chapters, factions), 26 (living NPCs), 41 (hunting), 35 (items and crafting), 43 (Gifts),
44 (levelling) and 34 (the Dungeon Master refresh) first.

Marks used below: **(agreed)** is the user's decision from their notes; **(proposed)** is a recommendation waiting
for the user; *(placeholder)* is a number or name that is one constant to change.

## The ask

The user asked what is proven, in the game industry and in psychology, to make a roleplaying game fun, easy,
intuitive and rewarding, and to keep players attached and feeling accomplished. Their notes on the research:

> "We need to look into making autonomy matter more (player's choices mattering in the game world [...] I have a
> feeling it leans towards NPC's, but maybe even economical impact and impact on cities/towns/farms/wolves), the
> competence part I think is fine though I would like your opinion on it. Relatedness (people know me) is another
> thing I think we need to look into."

> "Goals that need each other: this is a good idea and I think it might be the biggest research project in RATW. I
> need to find ways that put wolves working toward the same goal, but from two angles, together."

The rest of the notes are quoted where they apply.

---

## Principles

1. **Three needs.** Every feature here should serve autonomy (my choices change the world), competence (I'm getting
   better) or relatedness (people know me). In a roleplaying game relatedness comes first.
2. **Recognition over pay.** Expected rewards for something people already enjoy make it less enjoyable; thanks and
   recognition make it more so (appendix). Stars, nicknames and being remembered carry the social game; XP stays
   small.
3. **Together is better; alone still works.** A second wolf makes work, hunting and crafting much better. A lone wolf
   can still do all of it, more slowly. Nothing that matters is locked behind finding a group.
4. **Two angles.** Cooperation means wolves doing *different* things toward one goal: one holds, one strikes; one
   tracks, one stalks. Two wolves doing the same thing side by side is the weakest form.
5. **Visibility follows openness.** Roleplay that is open to newcomers should be easy to see and easy to join.
   Private roleplay should be quiet.
6. **The world remembers and says so.** A deed matters when it is seen, when it lasts, when someone mentions it, and
   when it cost something.
7. **Never punish absence.** No streaks to break and no decay that a week away makes painful. The world notices that
   you were gone, and welcomes you back.
8. **Existing rules hold.** No prose in ledgers; the NPC Mind never grants anything; names stay hidden until a wolf
   introduces themself; nothing edits terrain; nothing adds a full-world scan to the tick (docs 08, 32, 31).

---

## Part 1: Autonomy, or choices that change the world

Janet Murray's definition of agency is the useful one: "the satisfying power to take meaningful action and see the
results of our decisions and choices". RATW already simulates the results: residents with jobs, purses, bonds,
memories and rumours, towns with economies, farms with harvests. What is missing is mostly the *seeing*: players
rarely find out what their actions changed.

### 1.1 The test for a choice that matters

A choice matters when it passes four tests:

| Test | Meaning | RATW today |
|---|---|---|
| **Seen** | The player can find out what changed. | Weak. Bonds show in words; little else is reported back. |
| **Lasting** | The change persists after the player logs off. | Strong. The simulation keeps state. |
| **Spoken of** | Someone in the world mentions it. | Partly. Rumours exist; deeds, nicknames and thanks don't. |
| **Costly** | It took effort, money or a side, so it means something. | Varies. |

Most of this part is about making **Seen** and **Spoken of** strong.

### 1.2 Levers at each scale (proposed)

| Scale | What a player can do | What changes in the simulation | Who remembers |
|---|---|---|---|
| **A wolf** | Solve a resident's trouble (1.3), vouch for them to an employer, pay an apprenticeship fee, pay off a debt, testify or post bail, escort them to a new town, introduce two residents (they may marry). | Their job, home, purse, freedom, marriage, bonds. | That resident, their family, and everyone they tell. |
| **A farm or business** | Work the harvest, mend tools, bring supplies, invest, rescue a failing shop. | Yield, prices, whether the business survives, who works there. | The owner and workers; the town's news. |
| **A town** | Contribute to a town project (1.4), sponsor a festival, guard caravans, clear bandits off a road. | New structures, cheaper goods, safer roads, news travelling faster (rumours ride with caravans, doc 26). | The town: a plaque, a project named for its biggest donor, nicknames. |
| **A region or faction** | Chapter standing, faction missions, choosing a side. | Treaties, hostility, who trades with whom. | Factions (doc 32 Part 4). |

### 1.3 Residents' troubles (proposed)

Each resident may carry one current **trouble**, drawn from their real state rather than invented: a debt they can't
pay (purse), lost or broken tools (doc 35 durability), a sick or injured family member (doc 38), a feud (bonds), no
work (unemployed), a child of age with no apprenticeship. A resident mentions their trouble in conversation once
they trust the player a little (the Mind is briefed with it). Solving it changes their simulation state, moves the
bond, and becomes a rumour ("the grey wolf paid off Hale's debt").

These are the small personal stories that make one resident's life visibly different because a player passed by.
They also feed nicknames (3.7) and the chronicle (Part 8).

### 1.4 Town projects (proposed: the main new system for autonomy)

A **town project** is a public work the town needs: mend the bridge, dig a well, build a granary, raise a wall,
re-roof the church, clear the road. Projects come from real needs the simulation already knows (the economy
orchestrator, doc 46, knows shortages; doc 39 knows each settlement's buildings). A DM can also post one.

- A project needs **materials** (goods delivered), **labour** (cooperative work, Part 6) and **coin** (donations,
  conserved, doc 15). Progress shows on the town's notice board (5.4).
- A finished project changes the simulation: a bridge shortens a route, a granary stores grain through winter, a well
  removes a water-fetching chore, a wall makes raids rarer. It is a structure, never a terrain edit (doc 32 principle
  8).
- Contributors are recorded (IDs and amounts only). The biggest contributors are named on a **plaque**, residents
  remember them fondly, and a large enough project can be named for its chief donor ("Kestrel's Bridge").
- Projects wear like Chapter buildings do (doc 32 §5.6), so the world doesn't fill up and freeze, and keeping a town
  in good repair stays a reason to come back.

### 1.5 The economy as a lever (proposed)

Players already move goods. The economy should make that visible:

- **Trade between towns** moves prices: carrying grain to a town short of it should lower its price there, and the
  town news can say so.
- **Guarding caravans** makes them arrive, and because rumours cross towns only with caravans (doc 26), a guarded road
  also carries news, fame included, further.
- **Player help counts as a real flow** in the orchestrator's books (doc 46): if players feed a hungry town, the
  orchestrator sees less need. (This touches the orchestrator's design, which another thread owns.)

### 1.6 Bounds

Ultima Online's designers planned a living ecology and cut it before launch because players killed everything. A
world that players can change can also be broken by players. So:

- Protected residents (rulers, story figures) can't be killed or ruined by players.
- Changes wear or regrow unless kept up.
- A DM can see and reverse any player-made change (doc 34).
- Nothing edits terrain.

---

## Part 2: Competence

### 2.1 Opinion

The user thinks competence is fine. For fighters it mostly is: doc 37 made fights readable, and gear (doc 47), Gifts
(doc 43) and tactics decide fights more than levels (doc 44). For everyone else it is thin. A crafter, tracker,
storyteller or host gets little feedback that they are getting better. The fixes are:

- **Show growth when it happens:** "Your tracking sharpened (31)". Skills that grow by use (sneaking, hearing, scent,
  trades) already exist; most of them grow silently.
- **Stars are competence feedback for roleplay** (3.6). Tags tell a player *what* others valued.
- **End screens** (Part 8) show what a wolf did well in a fight, a scene or a story.

### 2.2 Characters without levels (agreed 2026-10-06; the user balances it later)

> "What if we didn't let characters level up? What if it was just simply... you chose what your character's
> strengths/weaknesses were at character creation? And you can IMPROVE a character's skill through practice?"

> "No character levels are a yes, but don't worry about balancing it. I will do that later. For now, let's just make
> sure its documented."

**Why it fits:** a roleplaying game is better without levels, and relatedness gains most. Levels split players: a
level 2 and a level 20 can't really hunt together. A practice-based character with a modest range lets a newcomer and
a veteran share a hunt on day one. Ultima Online, Morrowind and most roleplay MUDs worked this way.

**What it replaces:** doc 44's character level and its +1.5 fighting skill per level. It reverses doc 44's choice to
fold fighting skill into levels. Doc 44's typed awards, daily limits and rested time carry over, as below.

**Balance is the user's, later.** Every number in this section is a *(placeholder)*. Doc 45 tuned the Gifts around
levels, and the battle and level simulations will need re-running at three skill bands ("new", "seasoned",
"veteran"). The user will do that pass.

#### The shape

- **Character level goes away. Social level stays.** Social level measures standing, not power. It gates mentors
  (7.3), player storytellers (Part 9), Chapters (doc 32 §1.3) and Gift unlocks (below). Social XP alone feeds it:
  scenes, stars and Stories.
- **At creation, strengths and weaknesses:** a wolf spends a small point budget. Strengths start higher and can reach
  higher; weaknesses start lower and cap lower. The choices are STR, DEX, WIS, stamina, hearing, vision and smell,
  plus one starting **specialty** (a fighting style, sneaking, tracking or a trade).
  - Examples from the user: a Normal wolf that is "weak" and specialises in dexterity; a Quickened wolf that is
    "strong" and specialises in wisdom.
  - **Presets** ("Hunter", "Scholar", "Smith's hand", "Brawler") are there for players who don't want to build from
    scratch.
- **Practice:** a skill grows when it's used for real.
  - It grows faster near a better wolf, which makes teaching (7.3) and apprenticeships (doc 26) worth seeking out.
  - It grows faster against a live partner than against a dummy.
  - Growth slows near the wolf's cap, and each skill has a daily soft limit, so grinding is pointless.
  - Doc 44's typed awards (work, practice, milestone) become practice for the matching skill.
  - Rested time (doc 44) becomes faster practice after a break.
  - Each gain shows as it happens: "Your tracking sharpened (31)" (2.1).
- **Fighting skill grows by fighting and sparring again**, and the training grounds (5.3) are the place for it.
- **Two risks to plan for:**
  - Macroing, UO's old problem: practice counts only on meaningful use, such as a real target, a varied action, or
    not the same tile again and again.
  - Min-maxing at creation: the presets and caps soften it.

#### Gift tiers are earned, not chosen (agreed 2026-10-06)

> "Quickened wolves wouldn't be selectable to new players. Right now my schema for that is a player needs to spend
> time roleplaying as a normal wolf before they can make a gifted wolf, and then achieve a roleplay level and/or star
> amount (or some sort of way to measure that they're good roleplayers) before they can unlock Quickened wolves."

This replaces doc 43's "Quickened is free to choose at creation for now". It brings back the idea behind the
prototype's earned tiers: several signals together, never XP alone (Docs/References/SOCIAL_PROGRESSION_ROLEPLAY_TRACKING.md §8).
It also reverses doc 32 §1.3's "the prototype's earned tiers stay out".

| Tier | Who can create one | Unlocked by *(placeholders)* |
|---|---|---|
| **Normal** | Every account, always. A new account's first character is Normal. | — |
| **Gifted** | Accounts that have roleplayed as a Normal wolf for a while. | Social level 3, and 10 rewarded scenes on Normal characters, with any wolves (agreed 2026-10-06: no count of different wolves for Gifted). |
| **Quickened** | Accounts with Gifted unlocked that have shown they're good roleplayers. | Social level 8, 100 stars from at least 30 different wolves, 2 closed Stories, and no upheld reports in the last 30 days (agreed 2026-10-06). |

- **The measures live on the account** (stars, social level, reports), so an unlock applies to every character the
  account makes afterwards.
- **Unlocks are kept** once earned, as in the prototype, even if thresholds change later.
- **An unlock changes nothing about existing characters.** A Normal wolf stays Normal; the unlock is for new
  characters. A DM can still make a wolf Gifted or Quickened, or take a Gift away (doc 43).
- **The character creator shows what's locked and what it takes**, for example "Quickened: 62 of 100 stars", so the
  path is visible (the goal-gradient finding).
- **Quickened still carries the setting's cost** on top of the unlock: it's rare, Wardens take an interest
  (`wardenAttention` is already recorded, doc 43), and residents fear it. Whether a Gift tier also costs creation
  points is part of the user's balance pass.

---

## Part 3: Relatedness, or people know me

### 3.1 The player card and roleplay profile (agreed: "add a roleplay profile to the player card")

Total RP 3 and MyRolePlay exist because World of Warcraft gave roleplayers nothing to work with. RATW is a roleplay
game, so the profile belongs on the player card from the start. What to take from them is in **3.1a** below. In
short:

- **What a stranger sees:** the appearance description (already used as a stranger's label, doc 32 §1.5), a short
  written **description** (what you notice about this wolf), a **Currently** line ("mending nets by the pier,
  happy to chat"), and up to five **glances**: small icons with a line each, such as "a fresh scar over one eye" or
  "smells of smoke".
- **What a wolf who knows them sees as well:** the name they were given, plus a title and motto if the owner fills
  them in.
- **OOC:** a line of out-of-character notes, the roleplay **status** (3.5) and **experience** (3.5), and optional
  **lines and veils** (themes the player won't play, or wants kept off-screen).
- **Never shown:** the account name to non-friends (3.3), and the true name to wolves who haven't heard it.

#### 3.1a What Total RP 3 and MyRolePlay offer, and what RATW takes

Read from the addons' own source (Total RP 3 v3.4.3, TRP3 Extended v2.3.5, MyRolePlay 12.1) on 2026-10-06. Both
share profiles over the Mary Sue Protocol, so their core fields are the same.

| What the addons have | RATW | Why |
|---|---|---|
| **Name, title, nickname, house, motto** | Take title and motto, shown only to wolves who know the name. | Names are already handled by introductions (doc 32 §1.5); nickname comes from residents (3.7), not the player. |
| **Race, class, age, eyes, height, body shape** | Already there: species, age, appearance and stature come from the creator (doc 19). | |
| **Birthplace, residence** | Take as optional text; residence could later point at a rented room (5.7). | |
| **Description and History** | Take Description (what a stranger perceives). History goes in the OOC section. | Players complain that long profiles go unread, and that history read from a profile leaks into play. |
| **Currently** (what the character is doing now) and **OOC information** | Take both. Currently shows on hover. | The most used lines in both addons. |
| **At first glance:** up to 5 icon-and-line slots | Take, as "glances". | A quick read of a stranger without opening anything. |
| **Personality sliders** (11 pairs: Chaotic/Lawful, Truthful/Deceitful, Gentle/Brutal, Cautious/Impulsive and others) | Optional, in the OOC section. | Some players like them; others say they flatten a character into scales. |
| **Pronouns** | Take. | Both addons have it. |
| **Relationship status, voice reference, theme music, companion profiles, outfit switching** | Leave out for now. Outfit switching could return as disguise profiles that go with aliases. | |
| **Character status:** IC / OOC / Looking for contact / Storyteller, and TRP3's separate **Walkup friendly** flag | Take all of them (3.5). | Lets roleplayers find each other. |
| **Roleplay proficiency:** Newcomer / Casual / Veteran (or Experienced) / **Newcomer Guide** | Take (3.5). | "Newcomer Guide" is the ready-made signal for mentors (7.3). |
| **TRP3's roleplay-style consent flags:** accept character injury, death, romance, criminal activity, loss of control; each yes, no or "with player permission" | Take, as the profile's **lines and veils** (Part 11). Injury and criminal activity map onto doc 38 and doc 32. | Sets expectations before a scene starts. |
| **Relations you mark on other characters:** None, Unfriendly, Neutral, Business, Friendly, Love, Family, plus custom ones | Take, in known wolves (3.2). | |
| **Private notes** and a **directory** of profiles met, with last seen time and place | Take: these are known wolves (3.2). | |
| **Tooltip and label touches:** a border tinted by relation, a notes icon, an "unread description" mark | Take the notes icon and the unread mark. | |
| **Chat:** RP names in chat; NPC speech, emotes and OOC picked out | Already there: chat uses the names each listener knows (doc 32 §1.5), and OOC has its own tabs. | |
| **TRP3's map scan** for other roleplayers (opt-out; off automatically when OOC) | Covered differently: open scenes on the map (Part 4) and the gathering howl (3.5). | A scan of every roleplayer's location doesn't suit a world where a wolf might be hiding. |
| **Ignore** with a reason; a mature-content filter | Covered by block (Part 11), plus a mature flag on the profile *(proposed)*. | |
| **NPC speech frame, dice rolls** | Take, for player storytellers (Part 9). | |
| **TRP3 Extended:** player-made items, documents (books, signs, contracts), campaigns of quests and steps, cutscenes, auras | Documents: yes, as letters, notices and books (3.8, 5.4, Part 10). Campaigns and quests: yes, through the storyteller tool (Part 9). Items and auras made from nothing: no. | Items and buffs from nothing would break the conserved economy (doc 15). |

**What RATW gets that the addons can't:** residents can **read a wolf's profile**, but only the parts a wolf could
perceive: Description, Currently and glances, never OOC, history or sliders. A resident told "a fresh scar over one
eye" can ask about it. The addons only work between players who both have them installed. In RATW everyone has the
profile.

### 3.2 Friends, known wolves and notes (agreed)

> "We should add 'friends' lists to the game, and it should be account-based not character-based. [...] We should
> also add a character-based list for known wolves that allows the player to see what wolves they've interacted with
> recently, the summary of any roleplay sessions they might have had with them, or maybe even a note about them."

**Friends (account):**
- Mutual: a request is sent by account handle or from a known wolf's card, and both must accept. Sending one reveals
  your handle to them; accepting reveals theirs.
- Shows who is online, and **which character they're playing only if they choose to share it** (a per-friend
  setting, default on *(placeholder)*). Some players keep characters apart on purpose, and roleplay suffers when
  out-of-character knowledge leaks into the story.
- OOC private messages ("tells") between friends, anywhere.

**Known wolves (character):**
- Every wolf this character has met, newest first: the name *this character* knows (or the description), where and
  when they last met, the bond in words (`Bonds::describe`), how many scenes they've shared, and the last scene's
  summary.
- **Scene summaries:** a short private recap of a shared scene, written from what *this* character perceived, so
  whispers they didn't hear stay out. It is narrative storage, not a ledger (doc 08); the owner can delete it.
  **Summaries are for everyone** (agreed 2026-10-06), not opt-in. Cost: one cheap-model call per scene per
  participant (doc 28 has the prices).
- **Relationship tag**, as in Total RP 3: none, unfriendly, neutral, business, friendly, love, family, or one the
  player names.
- **Private note**: already planned in doc 32 §1.4.

### 3.3 Account names above characters (agreed 2026-10-06: friends only)

> "Being able to see a player's login name subdued above or below their character name?"

**Opinion: not for strangers.** It clashes with the name rule that RATW's introductions are built on (doc 32 §1.5).
A stranger's label would read "a grey wolf with a torn ear · DocFoxHound", which tells everyone who they are before
any introduction. It also lets players treat a character as the person behind it, and breaks aliases.

Instead:
- **Friends see your account handle** under your label, subdued.
- Account handles appear in OOC channels, the friends list and circles (3.4), where they belong.

### 3.4 Circles: roleplay groups and staying in touch (proposed)

> "Having common roleplay groups or teams, informal chats to stay in touch."

A **circle** is an out-of-character group of accounts: a roleplay group, a set of friends, a team. FFXIV's linkshells
fill this role apart from its in-character Free Companies. In RATW, Chapters are the in-character organisation and
circles are the out-of-character one:

- An OOC chat that reaches every online member, wherever they are and whatever character they're on.
- A roster showing who is online, with the character only where the member shares it.
- A small shared list of planned nights ("Thursday, the Wharf tavern, 8 pm").
- Any account can join several circles. Size limit *(placeholder: 50)*.

### 3.5 Saying you want to roleplay (proposed)

> "Maybe different ways to broadcast a desire to roleplay."

- **Status** on the player card, shown as a small mark by the label: *In character*, *Out of character*, *Looking for
  a scene*, *Storyteller* (running something others can join; MyRolePlay has this one).
- **Experience:** *Newcomer*, *Casual*, *Experienced*, *Newcomer Guide* (both addons use these words; a guide is
  happy to help newcomers). The newcomer flag (7.2) is separate and automatic.
- **Walk-up friendly**: a yes/no flag for "fine to approach me unannounced" (Total RP 3's own flag).
- **The gathering howl:** a wolf can howl to say "come and find me". Wolves within a wide range hear it and see a
  faint direction mark on the map for a minute, with the howler's status. It is a real in-world sound, so residents
  react too. When others join the howl within a few seconds it becomes a **chorus**: it carries further, and every
  wolf in it gets a small bond increase with the others. Real wolves howl together to gather and bond, so this is
  the most natural way for a wolf to say "I'm here". Cooldown *(placeholder: 10 minutes)*.
- **Open scenes on the map** (Part 4).
- **The public board** (5.4).

### 3.6 Stars (agreed, with proposed details)

> "Reduce the XP bonus while roleplaying during a fight, instead lets lean into stars. Maybe we can show player's
> total earned stars somewhere on their character sheet? [...] This would be account-bound, not character-bound."

- **Roleplay during fights earns less XP** (agreed). Doc 08's scene XP keeps working out of fights. In a fight, the
  stars matter instead.
- **Account total** (agreed): every Gold Star, Story Star and milestone star (8.3) an account has received. It shows
  on every character's player card.
- **Everyone can see it** (agreed 2026-10-06): "I think it'd be good for all players to see what quality of
  roleplayer they're getting involved with."
- **Strangers see it in bands** (agreed 2026-10-06): *10+, 25+, 50+, 100+, 250+, 500+, 1,000+* *(placeholder)*. An
  exact account-wide number on every card would link a player's characters to each other: two "different" wolves
  with exactly 437 stars are the same person. The player and their friends see the exact count.
- **Proposed: show how many different wolves gave them** ("from 60+ wolves"), also in bands. Pair decay (doc 08)
  already limits two friends starring each other forever; showing the spread keeps the number honest.
- **Proposed: a star rate**, if the user wants a measure of quality rather than quantity: stars per scene shared, shown
  in words ("most wolves who play with them leave a star"). A newer wolf with 20 stars from 25 scenes is a better bet
  than a veteran with 200 from 2,000.

**Tags (the user's idea; agreed 2026-10-06 with these names):**

> "When rewarding stars players can select one of three different types of stars? 'Writer', 'Teamwork', 'Fun', that
> become visible after a player reaches 50 stars total."

**Opinion: yes.** It tells players *what* others valued, which is competence feedback, and it lets a card say what
someone is like to play with. Three is the right number.

Other games are worth a look:
- Overwatch launched endorsements in 2018 with three categories: Sportsmanship, Good Teammate and Shot Caller. In
  2019 Blizzard reported 40% fewer matches with disruptive behaviour, counting together with Looking for Group and
  leaver penalties.
- Overwatch 2 cut the categories to one generic endorsement in 2022. Fan wikis say optional categories came back in
  2026.
- League of Legends dropped its three Honor categories in 2025.
- Neither company gave a reason. A likely one is friction: in a quick match, choosing a category is one more click
  that most people skip.

RATW's scenes are slower and more personal than a match, so the categories mean more here. Still, keep the tag a
second, optional click (below).

Names (agreed 2026-10-06):
- **Storyteller** instead of Writer. It praises moving the story along, not polished prose, which would favour fluent
  native writers.
- **Packmate** instead of Teamwork. It means the same and sounds like the setting.
- **Good fun.**
- A fourth, **Welcoming**, which only newcomers (7.2) can give. It counts toward mentor standing (7.3) and
  rewards the behaviour new players need most.

Tags become visible once the account has 50 stars (agreed). The card then reads "**Known for:** Storyteller", naming
the most-given tag, with a small breakdown on hover. Giving a star stays one click, and picking a tag is a second,
optional click.

### 3.7 Nicknames, deeds and fame (agreed)

> "NPC's should DEFINITELY come up with nicknames for wolves they recognize as having done something good and also
> mention your deeds and achievements. These should spread by rumor to other towns, too, but it should depend on the
> severity/immensity of the event, and whether NPC's recognize you."

- **Deeds** are ledger events with a **weight** *(placeholder scale)*:

  | Weight | Example | How far it travels |
  |---|---|---|
  | Small | Paid a resident's debt; mended a fence. | The resident and the few they tell (doc 26's existing spread). |
  | Notable | Saved a life; caught a thief; a town project finished. | The whole town within days. |
  | Great | Saved a town from a raid; ended a feud between houses. | Neighbouring towns, carried by caravan. |
  | Legendary | A world story's climax (8.3). | Everywhere, and festival criers recount it (5.6). |

- **Recognition:** a deed is tied to a wolf only as far as the witnesses knew them. A witness who knew your name
  spreads "Kestrel saved the miller's pup". One who didn't spreads "a grey wolf with a torn ear saved the miller's
  pup", and the deed attaches to your *description*. When someone later learns your name and has heard the story,
  they can join the two ("So *you're* the one who…"). This uses the name rule instead of fighting it.
- **Nicknames:** a notable or greater deed, or several small ones of the same kind, lets residents coin an epithet
  from a template plus the deed's details: "the Ferryman" (escorted many across), "Hale's Luck", "the wolf who pulled
  the miller's pup from the river". The Mind uses it in conversation; the first resident to coin it is remembered as
  its source. Players can't pick their own, and a player can ask residents to stop using one they dislike
  *(proposed)*.
- **Mentioning deeds:** residents who know a deed bring it up when it fits: greeting you, introducing you to others
  (7.5), at festivals.
- Bad deeds travel too, through the existing crime rumours (doc 32 §4.2b). This plan adds only the good side.

### 3.8 Letters and gifts (agreed)

> "Letters, gifts; we should do this. Specifically between players, but also from NPC's to players that NPC's like or
> want to thank."

**Between players:**
- Written at an inn, a post or a home desk, addressed to a wolf you know (by the name you know them by). A courier
  carries it, and **delivery takes travel time** by distance, which keeps it in the world.
- A letter can carry a small item or coin (conserved, doc 15) and is sealed. It **carries the writer's scent**: a
  wolf who knows that scent can tell who wrote it even unsigned. Masking oil (doc 35) makes an anonymous letter.
- Letters arrive at the recipient's inn or home, or are handed over by a courier who finds them in town.

**From residents:**
- **Thanks** after a deed, from a resident whose regard is high: a short letter, sometimes with a small gift from
  their own goods (conserved).
- **Invitations:** weddings, a child's naming, funerals, a festival meal at their table.
- **Requests:** a favour, which becomes a contract (doc 26 Phase 5).
- Written by the cheap model or from templates. Cost is in doc 28's terms: a few letters per player per week.

**Crafted items carry the maker's scent** (agreed). Doc 35 already marks masterworks with the maker's name and lets
scent give stolen goods away. Proposed: *every* crafted item and gift carries its maker's scent, readable by wolves
who know it. Gifts carry the giver's too, so it "still smells of her".

### 3.9 Favours (proposed)

> "How else can we add favor and gift giving? Maybe something like having a free daily buff that you can apply to
> yourself or to another wolf? What would this even be?"

**Grooming** (agreed 2026-10-06). Real wolves groom each other to bond, licking wounds clean and their coats free of
smells. It needs no hands, and it is plainly a gift of time and trust.

- Once a game day *(placeholder)*, a wolf can **groom** another. The other must accept, since grooming is intimate.
- The groomed wolf is **Well-groomed** for **the rest of the game day** (agreed):
  - **First impressions:** residents warm to them a little faster, in first meetings and trades.
  - **Healing:** rest heals their injuries a little faster (doc 38).
  - **Fewer lasting injuries** (agreed): every lasting-injury roll (doc 38's table) is *(placeholder: 10 percentage
    points)* lower while Well-groomed. Grooming a wolf who already has a severe acute injury also lowers the chance it
    sets into its lasting form at the next downing. Licked-clean wounds heal clean.
  - **Less scent** (agreed): grooming strips picked-up smells (blood, smoke, a fight's sweat) and lowers the wolf's
    own scent strength for *(placeholder: 2 game hours)*, so animals and watchers smell them from less far (doc 40's
    `(4 + 14 × strength)` tiles). It's weaker than Water's Wash Out, which removes scent entirely (doc 43), so the Gift
    keeps its worth.
- **Self-grooming** gives a weaker, shorter version *(placeholder: half the effects, 2 game hours)*. Being groomed by
  another wolf is what counts.
- Groomer and groomed both gain bond, and it counts as an authored action in the scene (doc 32 §1.1).

Other favours that fit:
- **Vouching:** a wolf with a good bond to a resident introduces a friend ("She's with me"). Part of the voucher's
  trust carries over to the newcomer, and the voucher's bond takes the hit if the newcomer misbehaves. This is the
  natural favour for mentors (7.3).
- **Sharing a meal:** food eaten at one table with other wolves keeps its effect longer. Real wolves share a kill.
- **Lending** gear or a room, **carrying a letter**, **standing as witness** or **sponsoring** someone into a Chapter.

---

## Part 4: Being seen, and joining in

> "Groups roleplaying should be noticeable. [...] if two players are just talking in a market their text should be
> more subdued, but wolves in a group roleplaying should have text that is easily identifiable and makes them stand
> out. I may be going the wrong way with this [...] How does this affect being able to join in to a roleplay,
> though? I want to make it easy for other characters to hop in if they want to."

**Agreed 2026-10-06**, with private scenes shown normally rather than faded. The idea: tie visibility to **whether a
scene is open**, not to whether its wolves are in a party. Two strangers chatting in a market are often how a scene *starts* (doc 08's A–B–A rule), so fading them out
would make scenes harder to begin. A party's private talk is the roleplay that *should* be quiet.

### 4.1 Scene openness (proposed)

Every scene (doc 08) has an **openness**, set by any member and shown on the scene line:

| Openness | Who can join | How its speech looks to wolves nearby who aren't in it |
|---|---|---|
| **Open** (default in public places) | Anyone in earshot, by speaking. | Clear, with a coloured bar and a small tag: "scene at the pier · open". |
| **Knock** | Anyone who asks and is let in. | Normal, with the tag "· knock to join". |
| **Private** (default for a party scene *(placeholder)*) | Members only. | Normal, as two or more wolves talking with one another: no scene tag, no Join button. Never faded (agreed 2026-10-06). |

### 4.2 Joining (proposed)

- An open scene in earshot shows a **Join scene** button on the scene line. Your next line counts toward it, with no
  need to wait for an A–B–A exchange.
- **Knock** sends the members a request with your label, newcomer flag and status. One accept lets you in.
- A newcomer who joins is marked for the others, so they know to make room.
- **Open scenes on the map:** a small speech mark on the minimap within *(placeholder: 60 tiles)*, with the number
  of wolves and the place ("3 wolves at the Wharf tavern · open"). This is what makes a tavern look alive from across
  town.
- **Your own scene is picked out.** In the chat log, lines from the scene you're in carry your scene's bar; open
  scenes nearby carry their tag; everything else, private scenes included, shows normally. Nothing is faded. A
  setting filters the log to "my scene only".

---

## Part 5: Centres of activity (third places)

> "I think we need to take a look at ways we can make centers of activity and social meeting in the game. My thought
> on this centers around markets (buy/sell/trade), taverns (rest and healing), training grounds (so players can
> practice fighting), quests (storyline and DM events), and 'activities' [...] healing at the cantina being a good
> example."

The rule from Star Wars Galaxies' cantinas: **a place becomes a centre when something you need can only be had
there, and it gets better with company.** Each centre below has that draw, plus a way for strangers to end up
talking.

### 5.1 Taverns (agreed: rest and healing)

- **Healing:** an inn bed already heals fastest (doc 38: 1.5 rest hours an hour). Proposed: resting in an inn's
  common room heals almost as fast as a bed (*placeholder: 1.25*), and faster still with company *(placeholder:
  +10% per other wolf present, up to +30%)* or a performer (5.7). This is the SWG lesson: healing brings people to the
  bar.
- **Rested time** (doc 44) builds faster at an inn.
- **The innkeeper introduces people** (7.5): a resident who knows everyone is the natural matchmaker.
- **Tavern games** (Part 10): a reason for two strangers to sit at one table.
- Chapters already take a tavern corner as a meeting place (doc 32 §5.1).

### 5.2 Markets (agreed: buy, sell, trade)

- Players can rent a **stall by the day** on Marketday (cities have stalls already, doc 39) and sell their own goods,
  with their scent and marks on them.
- Trade between players happens face to face, where it can turn into a scene.
- Contracts for goods (doc 42) are posted at the market's board.

### 5.3 Training grounds (agreed: practice fighting)

- **Sparring:** a friendly bout that ends at yield, never Downed, with no injuries past bruises *(proposed)*. With
  practice-based skills (2.2) it is where fighting skill grows, and faster against a live partner than against a
  trainer or a post.
- A resident **trainer** spars with anyone when no players are around, so practice is never blocked.
- **Watchers** can stand at the ring's edge, and **friendly contests** on festival days draw a crowd (5.6).
- It's where Quickened and Gifted wolves can practise openly, which is also where Wardens watch (doc 43).

### 5.4 Notice boards (agreed)

> "It should be where players can go to find work and storylines, but there should be a 'public forum' one, too,
> where players can post 'letters' or 'signs' on the board."

Every town square gets a board with two sides:

- **Work and stories:** contracts (doc 26), town projects (1.4), Storykeeper quests (doc 34), faction missions (doc
  32), and player storytellers' calls (Part 9).
- **Public:** notices players post: "Seeking an apprentice", "Music at the Wharf tonight", "Lost: a copper ring",
  sign-ups for a hunt. A notice is in-world writing, carries its writer's scent, and expires *(placeholder: 7 game
  days)*. Posting costs a penny *(placeholder)*, which stops spam and pays the town. Residents read it too, and may
  answer a "seeking" notice.

### 5.5 Activity grounds (agreed)

Hunting grounds, foraging patches, farms at harvest, the mine, the timber yard, the fishing pier: places where
cooperative work (Part 6) happens and a passer-by can lend a paw.

### 5.6 Festivals and gatherings (agreed: they should draw players, not only residents)

Festivals already gather the town at its square from noon with a free meal (doc 26). What brings players:

- **Contests:** a hunting contest, howling, tug-of-war (a team contest, Part 6), races, a sparring tourney, and a
  **storytelling contest** judged by the audience's stars.
- **The season's deeds:** a crier recounts the notable deeds since the last festival and the nicknames earned (3.7).
  Players come to hear their names, and everyone learns who's who.
- **Festival goods** sold only that day; Chapter displays; a festival quest from the Storykeeper.
- Rested time builds fast all festival day.

### 5.7 Venues and player-run nights (agreed in spirit; proposed details)

> "Players should be able to rent out anywhere that isn't a leadership center (kings, great house bedrooms, etc
> etc) but I don't know how this could impact stories."

Renting is Chapter-only today (doc 32 Phase 7). Proposed:

- **Individual players can rent too:** an inn room, a spare room, an empty shop, a barn, a warehouse, a tavern's back
  room for a night.
- **Never:** seats of power (thrones, council halls, great house chambers), churches, guardhouses, and any place a
  resident lives in or works from unless the owner is letting a spare part of it. Letting a room is income for the
  resident, a real economy channel, so no one is ever evicted to make room.
- **Stories first:** a DM or the Storykeeper can mark a place **held for a story**. It can't be newly leased, and an
  existing lease gets a week's notice ("the landlord needs the room back"), with the rest of the rent returned. A
  renter can also *become* part of a story: the watch searches a rented room, a stranger asks to lodge. Leases are
  never a reason a story can't happen.
- **Player-run nights:** a renter can post a night on the public board and on circles' lists (3.4). A **performer**
  (singing, a story, music with a played instrument) gives everyone present the company bonus of 5.1.

---

## Part 6: Together, or goals that need each other

This is the user's "biggest research project in RATW". The psychology is old and solid: strangers become allies when
a goal needs all of them (the Robbers Cave study, the jigsaw classroom; appendix). The design question is how to make
the need feel natural for wolves.

### 6.1 Wolves have no hands, which is the answer

> "Since they don't have hands, having one wolf hold a threshing tool and the other gather makes things go faster.
> Additionally, having two wolves hunt works MUCH better than one."

Doc 35 already says it: one mouth, one grip, and holding something stops a wolf biting and muffles its speech. Almost
every real two-person job translates directly: one wolf holds and the other works. Real wolves also hunt as a pack,
some driving the prey while others cut it off.

### 6.2 The two-angle patterns (proposed)

| Pattern | The two angles | Examples |
|---|---|---|
| **Holder and worker** | One holds, steadies or turns; the other strikes, pulls or gathers. | Smith and striker (real history: the smith holds the work in tongs, the striker swings the sledge). Rock drilling (one turns the steel, one swings). Threshing (one holds the flail, one gathers). A crosscut saw. Net fishing (one at each end). Rope-making (one turns, one walks the rope). |
| **Finder and taker** | One finds with nose or Gift; the other takes. | Tracker and stalker. Earth's Stone Sense finds ore, a miner digs it. Water's Dowse finds water, diggers sink the well. |
| **Driver and ambusher** | One pushes prey or quarry; the other waits where it will run. | Hunting (6.4). Herding strays. Catching a thief. |
| **Worker and guard** | One works; the other watches and fights if needed. | Foraging or logging in dangerous country; a caravan and its escort. Seer's Danger Sense makes a natural lookout. |
| **Team haul** | Several wolves in harness; more wolves, heavier loads. | Carts, sledges, timber, the plough. A tug-of-war contest. |
| **Many paws** | A big job that many wolves of any kind push along. | A barn raising, a harvest, a town project (1.4). |
| **Talker and doer** | One holds attention while the other acts. | Distracting a guard while a friend slips past; Sound's Throw Voice. Haggling while a partner checks the goods. |

### 6.3 How much better together (proposed)

- One wolf works at ×1.
- Two wolves in **different** roles work at ×1.8 *(placeholder)*. In the **same** role, ×1.4.
- Each extra wolf adds less: +0.4 for the third, +0.25 for the fourth. Most activities top out at four to six.
- A **resident hand** (an apprentice or a hired hand) counts as ×1.4 in a different role. A lone player can always
  get help, but a player partner is better.
- Every wolf in the activity practises its own skill (2.2), gains bond with the others, and can talk, so the activity
  is also a scene.

### 6.4 Hunting together (agreed 2026-10-06)

> "If a player walks upon another in the wilderness who is hunting, that player should be able to join the hunting
> wolf in their hunt and it will drop them into the hunting instance just like in combat (every player should have
> an option in their settings to toggle on/off allow hunting partners, which should default to on)."

> "Kills should be shared, yes, but it should be worth the cooperation as well. Animals should be fast and hard to
> catch, but should freeze when they see a wolf at a distance, and then flee in the opposite direction if the wolf
> gets close. This allows players to chase animals into a team mate. Animals should go down with a single bite, but
> there should still be a chance at the animal dodging."

**Joining:**
- The hunt's square in the world shows a **Join hunt** button, as Join fight does (doc 33).
- Today only the hunter's party, Chapter or companions can join (doc 41). This opens it to anyone, as long as the
  hunter's **Allow hunting partners** setting is on.
- The setting is per player and **on by default**. A hunter with it off can still invite people.

**How animals behave** (this replaces doc 41's `flee` and `cornered` alert behaviour):

| State | When | What the animal does |
|---|---|---|
| **Unaware** | It hasn't noticed a wolf. | Grazes and wanders, as now (doc 41). |
| **Frozen** | It notices a wolf at a distance: within its senses (doc 40's sight, hearing and smell for animals), but further than its flight distance. | Stops dead and watches that wolf, facing it. A frozen animal doesn't graze or wander. |
| **Fleeing** | The wolf it's watching comes within its **flight distance** *(placeholder: half its alert range)*, or it's bitten at and missed. | Runs **directly away from that wolf** at full speed, faster than any wolf can run *(placeholder: ×1.5 a wolf's sprint)*. |
| **Calming** | The wolf it fled from is out of its senses for a while. | Slows, then freezes again if it still sees a wolf, or goes back to grazing. |

- **It flees from the wolf it noticed, and only that one.** A wolf it hasn't noticed (crouched, downwind, out of
  sight; doc 40) isn't part of its reckoning. So an animal driven by one wolf can run straight into a hidden partner.
  This is the user's "chase animals into a team mate".
- **A lone wolf** must stalk close to an unaware animal (doc 40) and take one chance. Once the animal flees, it is
  gone, because it outruns any wolf.

**One bite brings it down, unless it dodges:**
- A bite that lands kills any animal that flees (rabbits to deer, red deer and elk included) at once.
- **Dodge chance**, by what the animal knew *(placeholders)*:

  | The animal… | Dodge chance |
  |---|---|
  | is unaware of the biter | 5% |
  | is fleeing *another* wolf and never noticed the biter (the drive works) | 15% |
  | is frozen and watching the biter | 60% |
  | is fleeing the biter | 75% (it's already running) |

  The species and the biter's DEX and skill shift these a little. A dodge sends the animal fleeing from the biter.
- **Fierce animals** (boar, bear) keep doc 41's health and fight back, so they stay a job for several wolves. The
  one-bite rule is only for animals that flee (agreed 2026-10-06).
- **Quality:** with one-bite kills every kill is clean, so quality comes from what the animal knew (doc 35 Part 4).
  A bite on an animal that never saw the biter, whether unaware or driven, gives **masterwork** goods. Any other bite
  gives **fine** goods. A driven kill is therefore the best kill there is.

**Shared, and worth sharing:**
- **The kill is shared equally** among every wolf still in the hunt who took part *(placeholder: moved, drove or bit
  at an animal in the last 10 turns)*. Fractions round by chance, as doc 41 already does with yields.
- **Worth the cooperation:** a lone wolf gets one chance per animal; two wolves can drive animals into each other and
  take masterwork kills. More hunters already bring more animals (doc 41: the expected count scales with players).
- **Tuning target:** each wolf in a pair takes home at least *(placeholder: 1.5×)* what a lone hunter takes in the same
  time, and a group of three or four does at least as well per wolf as a pair. Checked by a hunt simulation when this
  is built.

**Roles** are what each wolf does, not something they pick:
- **Tracker:** the nose leads, so the trail shows sooner and that animal is likelier (doc 41).
- **Driver:** shows itself and pushes; the animal freezes on it, then flees away from it.
- **Ambusher:** crouched and downwind where the animal will run.
- **Carrier:** brings the carcass home before it spoils, and can carry more.

### 6.5 Gathering, farming and other work (proposed)

- **Foraging** is solo today (doc 41). With a partner: one digs or pulls while the other carries and sorts, and the
  patch yields more of its pickings *(placeholder: the 4 pickings become 5 with two wolves)*.
- **Farm work at harvest and threshing:** players can join a farm's work and be paid by the farmer (doc 42's wages,
  conserved), which also helps the farm's yield (1.2).
- **Lend a paw:** a wolf at work in the world shows a "Lend a paw" button to passers-by, with the same **Allow work
  partners** setting (default on).

### 6.6 Crafts for two (proposed)

> "Maybe some sort of crafting can require two wolves performing two different tasks?"

Doc 35 already plans "recipes that need a second wolf as a helper". Proposed rule: **lead and hand.**

- The **lead's** skill sets the quality. The **hand's** skill sets the speed and how little material is wasted.
- Heavy work (big blades, armour, bells, large looms, timber framing) **needs** a hand. Most other recipes are
  better with one.
- A novice can be a useful hand on day one, and learns fast beside a master (2.2). This is how player apprenticeships
  work, and it gives veterans a reason to take newcomers along.
- A resident hand can fill in (6.3).

### 6.7 Gifted and Quickened, naturally (proposed)

> "It'd be ideal if there were ways to integrate different classes of gifted/quickened wolves as well, but it needs
> to feel natural."

**Gifted** wolves are already built for this: their abilities are work and support (doc 43), and lending one to a
workshop is built for residents' shops. The natural step is to make each Gifted family a third angle in shared work:

| Family | Its place in shared work |
|---|---|
| Fire | Forge Heat for the smith and striker; Kindle for a damp camp; Cauterize after a hunt. |
| Earth | Stone Sense finds the seam the miners dig; Clay Hand for the potter's or mason's lead. |
| Water | Dowse finds the well's spot; Draw Water for the tannery and fields; Mend after a hard hunt. |
| Wind | Winnow and Dry at threshing; Bellows for the forge; Turn the Wind keeps hunters downwind. |
| Sound | Ring True for the appraiser and the smith; Carry for the crier; Hush for an ambush. |
| Blinker | Shortcut across a gap for the haul team; Interpose for the worker and guard. |
| Gravity | Lighten Load for the team haul; Settle on finished gear. |
| Seer | Weathereye at harvest; Danger Sense as the lookout. |

**Quickened** wolves have no work abilities by design (doc 43). Their natural place is **protector**: guarding a work
party in dangerous country, the escort on a caravan, the one who faces the bear the hunters raised. The setting adds
a social angle no ability could: Quickened are rare, feared and watched by Wardens, so **the wolves who work beside a
Quickened wolf can choose to keep their secret**, vouch for them, or turn them in. That is roleplay the rules create
without scripting it.

### 6.8 Quests with two angles (proposed)

Storykeeper quests (doc 34) should often have **parallel objectives that need different wolves**: one talks to the
guard captain while another searches the warehouse; one tracks the thief while another warns the house he's heading
for. The quest finishes only when both are done, and each objective suits a different kind of wolf (a talker, a
nose, a fighter, a Gift).

### 6.9 Player jobs, later (agreed: a note for the future)

> "I eventually want to implement jobs for players to take, but lets make a note that these jobs should be in
> proximity or in cooperation with other wolves."

When player jobs come, each should be done in company: watch patrols walk in pairs, caravans need crews, ferries
need a pole and a rope, the mine works in teams, the smithy has a smith and a striker, a healer needs a carrier.
GTA roleplay servers show the effect: roles that only work with other people make stories on their own.

---

## Part 7: Newcomers

### 7.1 Where new characters start (agreed)

> "Let's prioritize Upper Accord for the default starting location."

- **A new account's first character** starts in **Upper Accord** (today's spawn, doc 23), unless Ser Ferro or
  Ridgemere has more active wolves online nearby. Then it starts in whichever of the three has the most.
- **Later characters** can choose any of the three.
- "Active wolves nearby" means players online within the town's bounds over the last *(placeholder: 30 minutes)*, so
  one busy evening doesn't swing it.

### 7.2 The newcomer flag (agreed)

- Shown as a small mark by the label and on the card: "new to these parts". Residents treat a newcomer kindly, and
  the Mind is told.
- Lasts until *(placeholder: 15 hours played or social level 3, whichever comes first)*. It is account-based: a
  veteran's new character is not a newcomer.

### 7.3 Mentors (agreed: social level 5)

- Any account at social level 5 *(agreed placeholder)* can opt in as a mentor. It needs no recent reports upheld
  against it *(proposed)*.
- A mentor's card shows a mentor mark, and they can set themselves *available* or *busy*.
- Mentors are the first choice for ties (7.4). Their reward is recognition: Welcoming stars (3.6), a mentor title, and
  "mentored 12 newcomers" on their card. It is never big XP.

### 7.4 Ties (agreed)

> "The ties should be connected to other 'mentor' wolves nearby that aren't presently tasked out by another tie, but
> those should be timed so that mentors don't always get stuck with them. The tie's should be any number of story
> starters, and I think the suggested one should be randomized for every character. New accounts MUST have a tie.
> For additional characters it will be optional."

- **At creation,** the player is offered a **tie**: a story starter that links their character to another wolf. One
  is suggested at random, and the player can reroll or pick from the list. A new account's first character must take
  one; later characters may skip.
- **Who the tie connects to:**
  1. an **available mentor** in or near the starting town who holds no other tie;
  2. otherwise, a **resident** chosen to fit (the miller for "you owe the miller money", a resident of the right age
     and family for "your aunt").
- **The mentor agrees first.** They're offered the tie with the newcomer's description and story starter, and have
  *(placeholder: 3 minutes)* to accept. If they don't, the next mentor is asked, then a resident.
- **Ties are timed** so mentors aren't stuck: a mentor holds one tie at a time; it lapses after *(placeholder: 7 real
  days)* or once both have shared *(placeholder: 3)* scenes, whichever comes first; and the mentor rests *(placeholder:
  1 day)* before the next. The relationship stays; only the tie's claim on the mentor ends.
- **Both are told the story starter** and where to find each other. The tie appears in both wolves' known-wolves list
  (3.2) with the starter as a note.
- **Story starters** *(the list grows; placeholders)*:
  - "You arrived on the same cart."
  - "They pulled you out of the river on the road here."
  - "You carry a letter for them from someone they haven't seen in years."
  - "You owe them a small debt."
  - "You're distant cousins; your mothers wrote to each other."
  - "They're looking for an apprentice, and someone told you to ask."
  - "You both saw something on the road you shouldn't have."
  - "You're new to the town and they've been asked to show you around."

### 7.5 Residents as matchmakers (agreed: the user's favourite; noted specifically)

> "Now to your notes about what to add: using NPC's to matchmake is a fantastic idea. I love it. Note this
> specifically."

A resident who knows two players (or a player and a resident) **points them at each other** when something links
them. This uses what's already built: bonds, the Mind, names and descriptions, jobs and trades.

- **When:** a resident talking to wolf A knows wolf B is nearby or often in town, and A and B share a reason to meet.
  The reasons *(proposed)*:
  - matching needs: A asked about work B's trade offers, or wants a skill B has (an apprentice and a master; a hunter
    and someone who wants hides);
  - a newcomer (7.2) and a mentor or a *Happy to help newcomers* player;
  - both are *Looking for a scene*;
  - shared ties: same home town, same Chapter's allies, a shared story;
  - a contract or town project that needs two angles (Part 6).
- **How:** in the resident's own voice, by description or by the name the resident knows: "The cooper's after someone
  who knows the river roads. Try the grey wolf by the fire; she came up from Ser Ferro last week." The resident never
  gives a name the player hasn't offered to it, and never a name the asker shouldn't know (doc 32 §1.5).
- **Innkeepers, priests and market wardens** are the natural matchmakers, because they know everyone.
- **Limits:** a player is pointed at someone at most *(placeholder: once a game hour)*, nobody is pointed at a wolf
  who is *Out of character* or has the matchmaking setting off, and a blocked wolf (Part 11) is never suggested.
- **The Mind is briefed** with the pairing and the reason. The server picks the pair; the model only says it.

### 7.6 Welcoming arrivals (proposed)

- On a newcomer's first evening, the innkeeper offers to **introduce them** to whoever is in the common room: a short
  line in the innkeeper's voice for each wolf the innkeeper knows (by the names it knows). Players can then introduce
  themselves.
- Vouching (3.9) lets a mentor carry a newcomer into the town's good graces.

---

## Part 8: Peaks, ends and visible progress

### 8.1 End screens (agreed in spirit)

> "Ending a fight or a roleplay session brings up the star screen, what if we also had a stats screen that showed
> involvement in both?"

- **After a fight:** the star screen plus a short account of the fight: blows landed, ground held, allies covered,
  who got whom up from Downed. Fights produce numbers naturally.
- **After a scene (proposed): moments, not word counts.** Showing words typed or turns taken would turn roleplay into
  a race for numbers, which is the very thing the stars replace. Show *what happened* from the ledger instead:
  "Kestrel introduced herself to Ash and Wren. Wren gave Ash a gift. The scene ran 40 minutes. Ash and Wren's bond
  grew." The page is about the people, not the score.

### 8.2 Personal storylines with markers (agreed)

> "We should build stories that individual players can follow and have trackable markers, so that players can see
> themselves progress along it."

- A **personal story** is a short chain of steps (three to seven) from a tie (7.4), a resident's trouble (1.3), a
  contract chain, or the Storykeeper (doc 34's planner). Its steps show in a journal with the next marker.
- Each step done is a small win and shows on the end screen. The endowed-progress finding applies: a new story
  starts with its first step already ticked (arriving, meeting the tie).

### 8.3 Big stories: credits and stars (agreed)

> "For BIG, GLOBAL stories, each milestone should have a screen like this that shows who did what for the story, and
> let players assign a maximum of 3 stars to whoever they want that contributed?"

- At each milestone of a world story, everyone who took part sees a **credits screen**: who did what, from the ledger
  (IDs and deeds only, never prose).
- Each participant may give **up to 3 stars** (agreed) to any contributors, with tags (3.6). The usual pair decay and
  daily limits apply.
- The milestone is a **great** or **legendary** deed (3.7) for its main contributors.

### 8.4 The chronicle (agreed)

A character's **chronicle** is a page of their life, written from the ledgers: first arrival, people met, ties,
Stories closed, deeds and nicknames, Chapter moments, town projects, milestones. Entries are short lines generated
from templates, never stored prose. It is the character's history and their sense of accomplishment in one place.

### 8.5 Unfinished business (agreed)

On logging out, and on the card's journal tab, a short list of open threads: promises made to residents (doc 26
already records them), a Story one scene from closing, a contract half done, a letter unanswered. Nothing nags; it is
there to be seen.

### 8.6 Welcome back (agreed)

- After a break of *(placeholder: 3 days)* or more, residents who know the wolf notice: "Haven't seen you since the
  thaw!" The Mind is told how long it's been.
- A short "while you were away" card: letters waiting, news from the towns they know, what happened to residents they
  helped, Chapter news.
- Rested time (doc 44) is already waiting for them. Nothing is lost for being away.

---

## Part 9: Player storytellers (agreed: a DM access level for players)

> "Let's design a level of access for the Dungeon Master tool that allows specific players to conduct and orchestrate
> stories for their Chapter/Party/Friends. This should be something they can apply for after, say, social level 5 or
> something."

Ultima Online's volunteer Seers and Total RP 3's player-made campaigns show the value: players running stories for
other players multiply the game's content.

- **Not the DM app.** Doc 17 keeps DM accounts separate, and player credentials can never grant DM access. A
  storyteller works through an **in-game storyteller tool** with a narrow set of permissions, audited like the DM
  app.
- **Who:** an account at social level 5 *(agreed placeholder)* applies. A DM or admin approves, and can revoke at any
  time.
- **Whose stories:** the storyteller's own Chapter, party, circle or friends. Players opt in to a story by joining it.
- **What a storyteller can do** *(proposed)*:
  - write a short story with steps and markers (a small version of doc 34's planner) that appears in participants'
    journals (8.2);
  - post quests and notices to their Chapter's board or the town board's work side;
  - speak **narration** and **NPC lines** to the participants, clearly marked as the storyteller's (Total RP 3's
    "NPC speech" is the model);
  - roll dice openly;
  - bring in **temporary visitors** from a DM-approved list (a messenger, a stranger, a beast), with no economy
    effect;
  - give prizes from **their own or their Chapter's purse** only (money is conserved, doc 15).
- **What they can't do:** grant XP, items from nothing, coin from nothing, Gifts or standing; move or kill residents;
  edit terrain; act outside their participants.
- **Recognition:** participants can star the storyteller with the Storyteller tag (3.6). Storytellers carry the
  *Storyteller* status (3.5).
- **The DM sees everything** storytellers do, on the DM app's Players tab (doc 34), and can stop a story.

---

## Part 10: Other ways to play

> "Players want different things: we should probably try to figure out exploration or other sorts of games within
> this game. Minigames may help, such as a library sorting minigame that allows players to explore lore while they
> work?"

**Opinion:** the library is a good *kind* of idea: work that teaches the world. It fits if it pays like work, trains
a skill (a scholar's), and gives out lore a piece at a time that residents can then talk about. On its own, a sorting
minigame becomes a chore quickly. The bigger win is games that **put two players at one table**:

- **Tavern games** (agreed 2026-10-06): short games for two to four wolves that give the tavern (5.1) something to
  do and put strangers at one table. FFXIV's Triple Triad and The Witcher's Gwent show how far this goes. A first set
  *(proposed)*:
  - **Knucklebones:** toss and catch bones. A quick game of skill, where DEX helps a little.
  - **Wolves and Deer:** a hunt board game. One side plays the pack, the other the herd, and each side wins
    differently. Hunt games of this shape are old and real (fox and geese, wolf and sheep), and in a world of wolves
    the hunted side is the deer.
  - **Liar's Bones:** a bluffing dice game for small coin stakes. Money is conserved (doc 15), stakes are capped
    *(placeholder: 5 pennies)*, and the house takes a penny a round.
  - **Rules for all of them:**
    - They take minutes, so a game is an easy invitation to a stranger.
    - A resident at the tavern plays when no player is free, so a lone wolf can always find a game.
    - Talking over a game counts toward a scene (doc 08) like any other talk.
    - Skill at a game grows with practice (2.2).
    - Festivals hold tournaments (5.6), and teaching a newcomer a game is a natural thing for a mentor to do (7.3).
- **Exploration:** ruins and old roads, lore fragments, hidden places that teach a trail (the nose, doc 41), a
  bestiary and herbarium that fill as a wolf finds things, and **maps** players can draw and sell. Discovery already
  awards XP (doc 44).
- **The library and archive** (the user's idea): sorting and copying for pay; each finished task shows a fragment of
  lore and adds it to the wolf's chronicle; scholars become people residents ask about history.

---

## Part 11: Safety (agreed: must-haves)

> "Mute/Block/Report are must-haves."

Only the DM can mute today, and that is a future phase (doc 21 Phase 6).

- **Mute:** you stop seeing a wolf's speech and emotes. They aren't told.
- **Block:** mute, plus they can't join your scenes, hunts, work or party, can't send you letters or tells, aren't
  suggested to you by matchmakers (7.5), and can't be tied to you (7.4). Blocks are **account-wide**, so they follow
  the player across characters, without telling you who the other characters are.
- **Report:** sends the DM app a report with the recent lines you saw from that wolf. This is the one place speech is
  kept, as evidence, and only in the report (an exception to doc 08's no-prose rule, agreed 2026-10-06). It's kept
  for *(placeholder: 30 days)* unless a DM acts on it.
- **Lines and veils** on the profile (3.1, 3.1a) let players say what they won't play before a scene starts.
  Following Total RP 3's flags: character injury, death, romance, criminal activity and loss of control, each *yes*,
  *no* or *ask me first*.
- PvP auto-decline already exists (doc 40).

---

## Plans

This document is too big to build from directly (agreed 2026-10-06). It stays the reason and the record of decisions.
Each area has its own actionable plan (docs 49 to 58), and **the list of plans, their order and their status live in
the epic document, [Docs/Epics/playing-together.md](../Epics/playing-together.md).** Work through the plans from
there.

---

## Decisions

### Agreed 2026-10-06 (the user's notes)

1. Cut XP for roleplay during fights; lean on stars.
2. Show an account-bound star total on the player card.
3. Starred tags that become visible at 50 stars (names proposed in 3.6).
4. Centres of activity: markets, taverns (rest and healing), training grounds, quests, activities.
5. Cooperative work and hunting are more efficient with more wolves; anyone can join a hunt like a fight; an **Allow
   hunting partners** setting, default on.
6. Crafts and quests that need two wolves from two angles; Gifted and Quickened fitted in naturally.
7. End screens for fights and scenes; credits screens at big stories' milestones with up to 3 stars each.
8. Personal storylines with markers.
9. Players improving residents' lives, farms and towns, remembered fondly.
10. Friends list (account); known-wolves list (character) with summaries and notes.
11. Crafted items hold their maker's scent.
12. A storyteller access level for players, applied for after social level 5.
13. Player jobs, when they come, are done in company.
14. Letters and gifts between players, and from residents to players.
15. **Residents as matchmakers.**
16. Upper Accord as the default start, or Ser Ferro or Ridgemere if busier; later characters choose any of the three.
17. A roleplay profile on the player card.
18. Newcomer flag; mentors from social level 5.
19. Mute, block and report.
20. Ties at creation, with available mentors first, then residents; timed; randomised suggestion; required for a new
    account's first character, optional after.
21. A notice board in every town, with a work-and-stories side and a public side.
22. Festivals that draw players; venues and player-run nights; renting anywhere but seats of power.
23. Nicknames and deeds from residents, spread by rumour according to the deed's size and whether witnesses knew the
    wolf.
24. Chronicle, unfinished business, welcome back.

### Agreed 2026-10-06, second round (the user's answers)

25. **Characters without levels:** strengths and weaknesses at creation, skills grow by practice. Documented here
    (2.2); the user balances it later.
26. **Gift tiers are earned:** Normal first; Gifted after roleplaying as a Normal wolf; Quickened after a social level
    and star measure. Quickened can't be chosen by new players (2.2).
27. **Everyone sees a wolf's stars,** in bands for strangers and exact for the player and their friends (3.6).
28. **Account names** are shown to friends only (3.3).
29. **Tags:** Storyteller, Packmate, Good fun, and Welcoming from newcomers (3.6).
30. **Scene openness** (Open, Knock, Private) decides how a scene shows. Private scenes are never faded: they show as
    wolves talking with one another (Part 4).
31. **Grooming** is the daily favour. Being groomed lasts the rest of the day, lowers lasting-injury rolls and strips
    scent, on top of first impressions and healing (3.9).
32. **Tavern games** (Part 10).
33. **Hunting:** kills shared; animals fast, freezing at a distance and fleeing from a wolf that comes close, so they
    can be driven into a partner; one bite kills, with a chance to dodge; the cooperation must pay (6.4).
34. **Reports keep the recent lines** as evidence (Part 11).
35. **Scene summaries are for everyone** (3.2).

### Agreed 2026-10-06, third round

36. **Gift unlocks:** Gifted needs social level 3 and 10 rewarded scenes on Normal characters, with no count of
    different wolves. Quickened keeps all its requirements: social level 8, 100 stars from at least 30 different
    wolves, 2 closed Stories, no upheld reports in 30 days (2.2). The numbers stay placeholders for the balance pass.
37. **Boars and bears** keep their health and fight back; one-bite kills are for animals that flee (6.4).
38. **Split into plans:** this document stays the reason and the record of decisions. Each area gets its own
    actionable plan document (Plans, above).

### Open, for the user

1. **A star rate** on the card as a quality measure, alongside the total (3.6)?

---

## Appendix: the research behind this

From the 2026-10-06 research answer, kept here so it isn't lost.

| Finding | Source |
|---|---|
| Autonomy, competence and relatedness each separately predict enjoyment and future play, MMOs included. | Ryan, Rigby & Przybylski 2006, *The Motivational Pull of Video Games*, Motivation and Emotion 30. |
| Expected rewards for an enjoyable task lower motivation for it; praise and feedback raise it; surprise rewards do no harm. | Deci, Koestner & Ryan 1999, meta-analysis of 128 studies (the overjustification effect). |
| Agency: "the satisfying power to take meaningful action and see the results of our decisions and choices". | Janet Murray 1997, *Hamlet on the Holodeck*. |
| People befriend those they keep bumping into. | Festinger, Schachter & Back 1950; Zajonc's mere-exposure effect. |
| Online worlds work as "third places". | Oldenburg, *The Great Good Place*; Steinkuehler & Williams 2006. |
| Shared goals that need everyone turn rivals into allies. | Sherif's Robbers Cave study; Aronson's jigsaw classroom. |
| Asking a favour makes the other person like you more; gifts invite gifts. | The Ben Franklin effect; Cialdini on reciprocity. |
| Many players want an audience more than a group. | Ducheneaut, Yee, Nickell & Moore 2006, *"Alone Together?"* |
| Newcomers learn a community by watching from the edge. | Lave & Wenger 1991, legitimate peripheral participation. |
| Visible progress pulls harder near the end, and harder with a head start. | Kivetz, Urminsky & Zheng 2006; Nunes & Drèze 2006. |
| Small daily wins are the strongest everyday motivator. | Amabile & Kramer 2011, *The Progress Principle*. |
| Experiences are judged by their peak and their end. | Kahneman et al. 1993; Redelmeier & Kahneman 1996. |
| Interrupted tasks pull people back to finish them (about two thirds return); the claim that they're *remembered* better did not hold up. | Ghibellini & Meier 2025 meta-analysis (Ovsiankina and Zeigarnik effects). |
| People value what they made more than it's worth. | Norton, Mochon & Ariely 2012 (the IKEA effect). |
| Players' motives differ: achievement, social and immersion are separate. | Bartle 1996; Yee 2006; Lazzaro 2004. |
| EVE: new players killed by other players in their first 15 days were more likely to stay; contact beats isolation. | CCP's 2015 study, as reported by players' sites. |
| WoW roleplayers built Total RP 3 and MyRolePlay because the game lacked profiles and status flags. | The addons themselves (3.1a): github.com/Total-RP/Total-RP-3, github.com/Total-RP/Total-RP-3-Extended, wowinterface.com MyRolePlay. |
| Overwatch's endorsements, with Looking for Group and leaver penalties, cut matches with disruptive behaviour by 40%. | Blizzard at GDC 2019 (gamedeveloper.com). |
| Overwatch 2 (2022) and League of Legends (2025) both dropped endorsement categories for a single vote. | overwatch.weirdgloop.org/w/Endorsements; wiki.leagueoflegends.com/en-us/Honor. |
| FFXIV allows one commendation per duty, to a player outside your own party. | na.finalfantasyxiv.com game manual. |
| SWG's cantinas became social hubs because fighters had to visit an entertainer to heal. | Star Wars Galaxies, before 2005's New Game Enhancements. |
