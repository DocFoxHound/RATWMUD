# 32. Parties, Chapters, factions and the social game

Planned 2026-10-02. **Phases 1 (parties) and 2 (names and introductions) built 2026-10-02; the rest not started.** Five decisions were agreed on 2026-10-02: player-versus-player combat by
request, party chat in two forms, names hidden until introduced, crimes as a burden that leaves with the member, and
reports that must be asked for. They are written into the parts below and listed under "Decisions".

This plan expands the social and roleplay game. It covers:

- **Parties:** small, voluntary groups with their own chat. Some NPCs may join them.
- **Chapters:** this game's clans. They are larger and longer-lived than parties, rise through five levels by roleplay,
  and gain faction reputation.
- **Factions and reputation:** a runtime faction system. Reputation with a faction belongs only to a Chapter. A player
  has reputation only with individuals.
- **Territory:** a Chapter's path from a place to gather, to rented rooms, to camps, to a small fortress.
- **Who is who on screen:** the colours that mark your party, your Chapter and anyone hostile.

All numbers here are placeholders to be tuned. Each is marked *(placeholder)* the first time it appears.

## Where we stand (read from the code 2026-10-02)

| Area | State | What exists |
|---|---|---|
| Social XP and scenes | Built, simple | `SocialLedger` (`Core/RatwSocialCore.*`). Doc 08's scene rules: an A–B–A exchange forms a scene. Qualifying takes 2 turns, 35 words and 1 reply, with at least 2 humans qualifying. Awards are 20/12/5, halved for each repeat pairing, capped at 100 a day. Level = `1 + XP/100`. Saved in the checkpoint, shown in the character dialog. No titles or tiers. |
| Bonds (any character → any other) | Built | `Core/RatwBonds.h`: affinity, trust, respect, familiarity, fear and owed pennies. Events move them: trade, talk, gifts, harm, theft, promises, time together, and the Mind's nudges of ±3 a reply. Saved to `game.bonds`. Players already hold and receive bonds, but no player-to-player action moves them. |
| Rumours | Built | `game.beliefs`: residents learn and spread claims about others, crimes included. |
| Factions | Data rich, game thin | The DM has `live.factions` (kinds: npc, city, guild, clan, other), claims, a relations matrix (disposition −100..100 plus a stance), a relation log and NPC members with ranks. **The game loads only name, colour and claims.** Relations and members affect nothing in play. Eleven factions are authored for Ridgemere and Ser Ferro. |
| Chapters | Stub | `world.chapters(id, name)` and `Cell::chapter`. Membership lives only in `tools/dm_service.py` (SQLite) and drives DM notices and migration previews. The game has no Chapter membership. |
| Parties | Stub | `recruit` works only for Bracken (`npc_scout`): `companionOwner_`, follow-the-leader movement, and a hard-coded line when asked "what do you think". No player-to-player grouping and no dismiss. |
| Chat channels | Local only | IC speech (whisper, speak, yell, heard by perception) and OOC to the same cell. No party, Chapter or direct channel. |
| Hostility | Bandits only | `World::hostile()` is true only for a standing bandit. Players cannot attack each other (`"You can't attack another player."`). An assault on a resident is a crime with witnesses, the Watch and the gaol. |
| On-screen wolves | Three colours | In `paint.ts`, self is amber (framed), NPCs sage and other players blue. "Hostile" appears only as text in the In Sight list. |
| Territory and ownership | Claims only | Faction claims per place. Nothing to rent, own or build. |

The old Unreal prototype's handoff (`Docs/References/SOCIAL_PROGRESSION_ROLEPLAY_TRACKING.md`) designed more than was
ported: Gold Stars, Stories, Story Stars, earned tiers, scene UI, private scenes and clan-scoped Stories. This plan
draws on it where it fits.

## Principles

1. **Roleplay is the engine.** Social progress for a player, a Chapter or a faction relationship comes from
   qualified roleplay and real deeds in the world. It does not come from chat volume, idle presence or money alone.
2. **The rules are the authority, never the model.** The NPC Mind may speak, react and nudge a bond within its
   limits. It never grants XP, renown, standing, items, ranks or land. (This is unchanged from docs 08 and 26.)
3. **Two separate reputation lanes:**
   - **Individual:** bonds between characters, player or NPC. "How the people of Ridgemere see you" is *read* from
     the bonds and rumours of Ridgemere's residents. It is never stored as a faction score on a player.
   - **Collective:** a faction's standing toward a Chapter. Only Chapters have it.
4. **No prose in ledgers.** Renown, standing and treasury ledgers hold IDs, amounts and reasons, never chat text.
5. **Perks are social, logistical and territorial, not raw power.** A level 5 Chapter is influential, rich and
   housed. Its members do not hit harder. This matches the setting: practical power comes from position, not magic
   or levels.
6. **Money is conserved** (doc 15). Rent, wages, building materials and tithes move between real purses. Founding
   fees and upkeep are sinks, recorded as sinks.
7. **Cheap at a thousand players** (doc 31). Membership is an O(1) lookup. A viewer's relationship to an entity is
   computed from small sets. Group chat goes only to members. Nothing here may add a full-world scan to the tick.
8. **Terrain is never edited.** Camps and fortresses are a structure layer on top of the world, like doors and
   objects. Atlas and the DM can see and remove them.

---

## Part 1: The individual social game

The individual game gets wider, not more complicated.

### 1.1 Scenes the player can see

Scenes are invisible today, so players cannot tell whether their roleplay counts.

- **Scene indicator:** a small HUD chip reads "In a scene with Ash, Wren · 3 turns" while a scene is active. It
  shows "quiet" after 15 minutes without activity and "ended · +14 social" at settlement. It shows confirmed
  numbers only, never a prediction.
- **Party scenes:** if two or more party members are in earshot, their exchange joins *the party's* scene instead
  of the cell's one automatic scene. This fixes doc 08's open problem of two conversations in one cell merging.
  Strangers who join in still go through the A–B–A rule.
- **Actions count, at a lower weight.** `/me` and `/action` text has counted for nothing. Proposal: authored
  actions count as half-weight words *(placeholder)*. The word, turn and reply rules still apply, so a pure
  emote-spammer still cannot qualify. Posture and state changes still count for nothing.

### 1.2 Recognition and Stories (ported from the prototype)

- **Gold Star:** after a qualified scene, each participant may give one other participant a star. It is binary, a
  thank-you, not a rating. It is worth up to 2 XP, decays per pair, and is capped at 10 given a day.
- **Stories:** a chain of qualified scenes that share participants, proposed by one player and approved by two
  thirds. On closing (two or more scenes) it pays a continuity bonus: `floor(paid scene XP / 4) + min(scenes − 1, 5)`.
  Stories matter here because **Chapter Stories** are a main source of Chapter renown (Part 3).
- **Story Star:** one per member per closed Story, up to 4 XP.

### 1.3 What a player's social level is for

Social Level stays `1 + XP/100`, with titles added. It unlocks standing *within the social game*:

| Social level | Title *(placeholder)* | Unlocks |
|---|---|---|
| 1 | Stranger | Parties. Joining a Chapter as an initiate. |
| 3 | Known | NPC companions who join out of friendship (see 2.3). Can hold a Chapter office. |
| 5 | Familiar Face | Can co-found a Chapter. |
| 8 | Respected | Can lead a Chapter. NPCs offer personal favours and introductions. |
| 12 | Notable | Faction NPCs will discuss Chapter business with you by name. |

The prototype's earned tiers (Regular, Gifted, Quickened) stay out. The setting bible says Quickened is not a
level, and this plan does not reopen that.

### 1.4 Reputation with individuals

This is already built as bonds. The plan makes it visible and gives it consequences.

- **Your standing with someone:** the Look panel on a known NPC shows their regard in words. `Bonds::describe`
  already writes this ("knows you well, likes you, trusts you a little"). The numbers stay hidden.
- **Your name in a town:** a derived summary over the residents of a place who know you, such as "Well liked in
  Ridgemere's Wharf; distrusted by the Watch". It is computed when asked, from bonds and rumours, and never stored.
  It shows the player how word gets around without giving them a faction score.
- **Player to player:** players already hold bonds. Shared qualified scenes, Gold Stars, gifts and trade move them
  the way they move NPC bonds, so the game can say "you and Ash have shared 6 scenes". Assault and theft cannot
  apply, since players cannot attack each other. A player's private feelings stay theirs: the bond is a record of
  dealings, not a mood. One addition is a private **note** on any known wolf, kept for the owner only.

### 1.5 Names and introductions (agreed)

**You don't know a wolf's name until they introduce themselves.** This holds for players and NPCs alike.

- **A stranger is their appearance.** Until introduced, a wolf is labelled by a short description built from the
  appearance system: "a grey wolf with a torn ear", "a stout red wolf in a guard's tabard". This label is used
  everywhere: map hover, In Sight, Look, the chat log ("A grey wolf with a torn ear says…"), party and Chapter
  invites (made by pointing at a wolf, not by name), and the NPC Mind's prompt.
  - **Public roles still show:** "the baker", "a Ser Ferro guard". You can see what someone does without knowing
    their name.
  - **Repeat strangers stay distinct:** two grey wolves with no distinguishing marks get "a grey wolf" and "another
    grey wolf" in a crowd, so that chat stays readable.
- **Your names.** Each character has a **true name** and up to 3 **aliases** *(placeholder)*, set on the character
  sheet. Players assign their own aliases (agreed 2026-10-02):
  - at character creation (an optional step in doc 19's flow), and later from the character sheet at any time;
  - **retiring an alias** stops it from being detected in new speech, but wolves who already know you by it keep
    knowing you by it. Every name must be unique among a
  wolf's own names and pass the name filter. The game only ever recognises names on this list, which is what makes
  detection deterministic.
- **Introducing yourself happens in speech, automatically.** When a player's in-character speech contains an
  introduction phrase directly followed by one of their own registered names, the server treats it as an
  introduction. Everyone who perceived that line learns the name *as spoken*.
  - **Phrases** *(placeholder list, case-insensitive)*:
    - "I'm", "I am", "my name is", "name's"
    - "call me", "they call me", "you can call me"
    - a bare name followed by "of" or "at your service" ("Ash, of the Ashen Lodge")
  - **The match is exact:** the phrase, then optional punctuation, then a registered name on whole-word
    boundaries. "I'm tired" can't match because "Tired" isn't a registered name. "Have you seen Wren?" introduces
    nobody, because only your *own* names count. Naming someone else is hearsay, not an introduction.
  - **Hearers** are whoever perceived the line: talk targets, the party in earshot, a whole tavern if yelled. This
    is the existing perception filter, so a whisper introduces you only to whoever was close enough.
  - **A receipt** appears in your own chat log: "You introduced yourself as Kestrel to the grey wolf and the
    baker." You always know which name you gave to whom. Speech can't be unsaid, so there is no undo.
  - It counts as an ordinary scene turn. Nothing extra is earned for it.
- **The Introduce button** (Look panel or talk target, or `introduce [as <name>]`) does the same thing for players
  who'd rather not phrase it. It speaks a stock line in your voice ("I'm Kestrel.") so the introduction is always
  heard in the world.
- **What a hearer knows** is the name they were *given*, stored per knower (`game.acquaintances`: knower, known
  wolf, name heard, day, how). One wolf can know you as Ash, another as Kestrel. A wolf who hears a second name from
  you keeps both ("Kestrel, who also calls herself Ash"), and the newer one shows first. Labels, chat and the NPC
  Mind all use the name *that knower* holds.
- **NPCs hearing a player's introduction** use the same detector. Speech to an NPC already passes through the
  server, so "my name is Kestrel" records Kestrel in that NPC's acquaintances, and the Mind is told "they told you
  their name is Kestrel".
- **NPCs introduce themselves** when it is natural:
  - when asked their name, unless they distrust you (trust below −20 *(placeholder)*), are hiding something, or
    their authored persona is secretive;
  - after you introduce yourself, if they are friendly (affinity ≥ 0);
  - unprompted, once familiarity is high (they have seen you around for days).

  The Mind's structured reply gains an `introduces: true` field. The server checks it against these rules before
  recording the name, so an NPC cannot be talked into a name the rules forbid. An NPC recognises a player by
  appearance until the player introduces themself. Its bonds form with the wolf either way.
- **Groups:** joining a party or Chapter does **not** introduce you automatically. You still introduce yourself to
  each wolf, in character. The party and Chapter rosters show unknown members by description. *(Placeholder: a
  Chapter's founding scene introduces the founders to one another.)*
- **Forgetting:** names are not forgotten in this version.
- **Hearsay** (another wolf naming a third: "that's Wren, the cooper") is left out for now. It may come later
  through the rumour system as an unconfirmed name.
- **Aliases are allowed** (agreed 2026-10-02). Giving a false name is just introducing yourself with one of your
  aliases, and nothing in the system marks it as false.
- **How a true name comes out** in this version:
  - you introduce yourself again by it;
  - a faction report (4.2b) names you, if its witnesses knew your true name;
  - the DM reveals it.

  Hearsay, papers, and NPCs who see through aliases are for later.

---

## Part 2: Parties

A party is a light, voluntary group for an evening's play.

### 2.1 Forming and leaving

- `party invite <name>` while in sight or in the same cell. The invitee accepts or declines. Invites expire after
  60 seconds.
- **The leader** is the founder by default. They can invite, remove, pass leadership and disband. Any member can
  leave.
- **Size:** up to **6 players and 2 NPCs** *(placeholder)*.
- **Disconnects:** a disconnected member holds their place for 10 minutes. A party with no connected players
  dissolves after 10 minutes. Parties are kept in the checkpoint, so a server restart does not break them.
- A player is in at most one party.

### 2.2 Party chat (agreed)

There are two channels:

- **Party (in character), limited by distance** (the Party tab, or prefix `/p`).
  - Speech addressed to the party. It reaches only the party members who can hear it at the chosen volume:
    whisper, speak or yell, judged by perception exactly like local speech.
  - A party member out of earshot gets nothing. The game doesn't tell them they missed a line.
  - Non-members in earshot overhear it as they would any speech at that volume. Addressing the party picks the
    listeners. It is not a secret channel. A whisper keeps it close.
  - It earns scene credit like any local speech, in the party's scene (1.1).
  - In the chat log it is tinted the party colour, so it stands out in a crowded square.
- **Party OOC, any distance** (the Party OOC tab, or prefix `/po`).
  - Reaches every player member wherever they are.
  - Plain text, no perception filter, no XP. It is for "meet at the north gate" and "brb".
- **NPC members** hear in-character party speech when in earshot, like any member, and the Mind gets it as party
  context. They never see party OOC.

### 2.3 NPCs in the party

This generalizes the Bracken slice into a rule.

**Who can join.** An NPC is *joinable* if all of these hold:

- Authored as joinable in Atlas or the DM (a flag on the resident), or of a joinable kind: traveller, sellsword,
  guide, hunter, pilgrim, idle apprentice.
- Not holding a position that is on shift, not protected (the cook, keeper and forager), not Quickened, not wanted,
  not in custody.
- Willing, for one of these reasons:
  - **Friendship:** affinity ≥ 40 and trust ≥ 30 toward the asking player *(placeholder)*, and the player is at
    social level 3 or higher.
  - **Hire:** a wage per game day agreed up front, paid from the hirer's purse into the NPC's at each game dawn.
    Unpaid means they leave.
  - **Story:** the DM or a running story assigns them.

**How they behave.**

- **Movement:** they follow the party leader using today's follow movement, with a formation offset so two
  companions don't stack. They use doors and roads. When the leader leaves the cell they cross after them, with the
  existing transition.
- **Orders:**
  - `wait here`
  - `follow`
  - `go home` (they walk home, they do not vanish)
  - `dismiss`
  - Hired help can also be told `keep watch`.
- **Their life:** their schedule pauses while they travel, as today. A worker's post stands empty and the world
  notices: the shop is shut and rumours say where they went. A friend leaves after a game day or two *(placeholder)*
  unless the bond is very strong. Hired help stays while paid. They eat from their own purse or the party's food.
- **Leaving on their own:** if harmed by the party, if they see the party commit a crime that offends them (a
  lawful NPC watching a theft), if unpaid, or if their bond falls below what made them join.
- **Fights:** against bandits they help, using their own stamina, and they retreat when badly hurt. They are never
  made to fight the Watch.
- **Bonds:** time together raises familiarity. Shared danger raises trust. A long journey together should leave a
  real friendship behind.

**How they roleplay.** This is the expensive part. It is budgeted under doc 28.

- **When addressed** (talk target, or their name spoken), they reply with the main voice, as any NPC does today.
- **Unprompted lines** have triggers: arriving somewhere new, danger seen, a long silence in a party scene, a party
  member hurt. At most **one unprompted line per party every 3 real minutes** *(placeholder)*, on the cheap model,
  falling back to written lines from the scene library (doc 30) when over budget.
- **Party context in the prompt:** the Mind is told who is in the party, how the NPC regards each member, where the
  party is going (if the leader has said, `party goal "reach Ser Ferro"`), and the last few local lines.
- **NPC lines never count** toward a scene's qualification. A player alone with two companions cannot farm XP.
- **Server-wide cap** on joined NPCs, e.g. 1 per 4 connected players *(placeholder)*, to bound AI cost and the
  followers' share of the tick.

### 2.4 On screen: who is who

Every wolf drawn on the map gets a **relation to the viewer**, computed on the server and sent with its view. The
colours, in priority order:

| Relation | Colour | Extra mark (colour is never the only cue) |
|---|---|---|
| You | Amber, framed | As today |
| Hostile | **Red** | A small `!` above the glyph; "hostile · why" in Look and In Sight |
| Party member (player or NPC) | **Amber, the player's own colour** | A small dot under the glyph; name on hover |
| Chapter member | The Chapter's colour | A thin underline |
| Other NPC | Sage | As today |
| Other player | Blue | As today |

- **Chapter colours** are chosen at founding from a palette with the red range removed, so nothing but hostility
  is red.
- The **minimap** shows party members as amber dots, always, even out of sight. You know roughly where your
  friends are; it is a party perk. Chapter members show only when in sight.
- **Accessibility:** the marks above carry the meaning without colour, and the In Sight list says it in words.

**Who is hostile (red).** A wolf is hostile *to you* when one of these holds:

1. **A bandit,** as today.
2. **It attacked you or a party member** in the last 5 real minutes *(placeholder)*, or until it is beaten down,
   taken by the Watch, or out of sight for a minute. This gives NPCs a short "aggro" memory they lack today.
3. **It is on the other side of a fight you or a party member are in** (see "Player versus player" below).
4. **It is on your Chapter's hostile list.** Officers mark individuals (players or NPCs), whole other Chapters, or
   factions, with a reason that members can read. The mark is visible only to your Chapter, and the target is not
   told. Strangers are marked by pointing at them, and the list shows them by description until introduced.
5. **It belongs to a faction at war with your Chapter** (stance `war`, Part 4).

A guard coming to arrest a wanted player is *not* red. They are lawful, and the WANTED label already covers it.

**Player versus player (agreed; designed in the combat work, not here).**

- A fight between players **starts only by request**: one player challenges and the other accepts.
- **Once a fight has started, other players may join it** on either side, without anyone's permission.
- Whoever is on the other side of a fight you are in, or that a party member is in, shows red to you for as long
  as the fight lasts and for the aggro time after.
- A Chapter's hostile list, or a war between factions, colours wolves red but **does not by itself start a fight**.
  The request is still needed.
- The combat thread owns the rules for fights. This plan only consumes their state: who is fighting, and on which
  side.

---

## Part 3: Chapters

A Chapter is a lasting, named fellowship of wolves. It roleplays together, grows in renown, deals with the
factions as one body, and in time holds its own ground.

### 3.1 Founding

- **Founders:** 3 players *(placeholder)*, each at social level 5 or higher, none already in a Chapter.
- **The founding is a scene.** The founders hold a qualified roleplay scene together and then perform `chapter
  found` before it ends. The Chapter is born from a moment, not from a menu.
- **Founding fee:** 2 marks *(placeholder)*. This is a money sink.
- **Chosen at founding:** a name (unique, checked for offensive words), a colour, a short public charter (free text
  stored with the Chapter, not in any ledger), and a meeting place (see 5.1).
- In the database a Chapter **is a faction** of kind `chapter`, alongside the existing `clan` and `guild` kinds.
  The DM's relations matrix, claims and stories then apply to it unchanged.

### 3.2 Membership and ranks

- One Chapter per character.
- **Ranks:** Head, Officer, Member, Initiate *(placeholder names; a Chapter may rename its ranks)*.
  - The **Head** has everything.
  - **Officers** invite, remove initiates, keep the hostile list, spend from the treasury within a limit, and start
    building.
  - **Members** vote, use Chapter property and deposit.
  - **Initiates** have probation: chat and meeting rights, no property keys, and their renown counts at half.
- **Succession:** if the Head is away for 30 real days *(placeholder)*, the longest-serving active Officer takes
  over. The rank log keeps who held what and when.
- **Leaving and removal** are logged. A removed member's open contributions settle normally.
- **Chapter chat:** the same two forms as the party's (2.2).
  - **In character, limited by distance** (`/c`): reaches members who can hear it.
  - **OOC, any distance** (`/co`).
  - Chapter notices from the DM also arrive here (already built in `dm_service`, moved to the game).
- **NPC members** come at level 4 (sworn retainers). Hired NPC staff at camps and halls work for the Chapter but are
  not members.

### 3.3 Chapter renown, and how it is earned

Renown is the Chapter's social XP. A dedicated, replay-safe ledger records it, with IDs and reasons and no prose.

| Source | Renown | Notes |
|---|---|---|
| A member's settled, paid scene | 25% of their paid XP | Only scenes with at least one other human. Scenes are not double-counted across members. |
| A **Chapter scene** (2+ members qualified together) | +5 to the Chapter | Once per scene. |
| An **outreach scene** (a member qualified with a non-member) | +3 | Rewards a Chapter that plays with the world, not only itself. |
| A **Chapter Story** closed (a Story whose qualified members are mostly the Chapter's) | +20 + 5 per scene, up to +50 | The main source of large gains. Counts toward level gates. |
| A faction mission completed (Part 4) | +5 to +15 | Also moves standing. |
| DM award | Any amount, with reason | Logged with who gave it. |

Limits, so that size or grinding does not decide everything:

- **Weekly cap:** 300 renown *(placeholder)*.
- **Diminishing with size:** the member-scene share is scaled by `sqrt(10 / active members)` when there are more
  than 10 active members. A large Chapter grows faster, but not linearly.
- **Pair decay, inherited:** a scene that pays a member nothing after pair decay pays the Chapter nothing.

### 3.4 The five levels

Renown alone is not enough. Like the prototype's tiers, each level needs renown, enough *active* members (a
qualified scene in the last 14 days), closed Chapter Stories, and the right ground. Levels are sticky: **a Chapter
never loses a level.** Neglect costs property, not rank (see 5.6).

| Level | Name *(placeholder)* | Renown | Active members | Chapter Stories | Ground needed |
|---|---|---|---|---|---|
| I | Gathering | 0 (founded) | 3 | 0 | A meeting place |
| II | Lodge | 400 | 5 | 1 | — |
| III | Company | 1,500 | 8 | 3 | A rented hall held 2 game weeks |
| IV | Hall | 4,000 | 12 | 6 | A camp standing |
| V | Hold | 10,000 | 16 | 10 | A fortified camp, plus Friendly or better with a faction (for land it claims) |

**Perks by level.** Each level keeps the ones before it.

- **I. Gathering**
  - Chapter chat, roster and ranks
  - Chapter colour on screen
  - A declared meeting place on members' minimaps
  - The hostile list
  - Faction standing begins to be tracked
  - The DM's Chapter notices
- **II. Lodge**
  - **Rent** rooms and small halls in towns (5.2)
  - A Chapter treasury with a deposit/withdraw log
  - A notice board in rented property
  - Faction missions at the first tier
  - Rank renaming
  - Members' scenes in Chapter property count as Chapter scenes even with one member present *(placeholder; at
    least two humans are still needed to qualify)*
- **III. Company**
  - **Build a camp** (5.3)
  - Rent larger halls and warehouses
  - Hire NPC staff: a cook, a watch, a quartermaster
  - Shared storage across properties
  - Faction missions at the second tier
  - Treaties with factions (Part 4)
  - A **Chapter Story** type with its own board
- **IV. Hall**
  - **Fortify** camps: palisade, hall, workshops (5.4)
  - Up to 3 sites
  - **Sworn NPC members**: residents who join the Chapter for life, from deep bonds (doc 26 households)
  - Faction quartermasters sell the Chapter **bonus gear** at Trusted
  - Escorts and introductions from allied factions
- **V. Hold**
  - **Build a small fortress** (5.5)
  - The Chapter becomes a **claimant**: its own faction claim on the cell it holds
  - Residents may **migrate** to the Hold (doc 16's pipeline, with real housing and jobs)
  - May be recognized by a friendly faction as a **minor House** (a title and a seat at that faction's table; the
    setting's Houses make this a natural crown)
  - Tolls on its own roads *(for discussion)*

---

## Part 4: Factions and reputation

### 4.1 What a faction is in play

The DM already authors factions, claims, relations and members. The game needs to **load and use** them:

- **Load relations and members** into the game (`faction_relations`, `faction_members`), hot-reloaded like
  `factions.sync`. An NPC knows its factions and rank.
- **Fill in the factions** using the setting bible's Faction Template (§25): public and actual purpose, what it can
  offer, what it can take away, rivals, and visual identifier. Start with the eleven of Ridgemere and Ser Ferro,
  plus the **Concord**, the **Warden Order** (Upper Accord) and the **Syndicate** (Ser Ferro's underworld, acting
  through the Couriers). This is writing work, done in the DM.
- **Faction NPCs act on standing:** a guard, clerk, quartermaster or priest of a faction reads their faction's
  stance toward the speaker's Chapter, alongside their personal bond. The Mind's context gains one line: "Your
  faction (the Ser Ferro Guard) regards the Ashen Lodge as friendly." A faction member's private bond with a player
  stays their own. A guard may like you while the Guard distrusts your Chapter.

### 4.2 Standing: Chapter toward faction

Standing reuses the DM's relation model: a disposition from −100 to 100 and a stance (allied, friendly, neutral,
tense, hostile, war), with a reason and a log. The game adds named bands so players can read it:

| Disposition | Band | Stance by default |
|---|---|---|
| 75..100 | Sworn | allied |
| 40..74 | Trusted | friendly |
| 15..39 | Known well | friendly |
| −14..14 | Neutral | neutral |
| −39..−15 | Distrusted | tense |
| −74..−40 | Hostile | hostile |
| −100..−75 | Enemy | war, if the faction declares it |

The DM can still set a stance by hand, and that wins. Standing is **directed**: the Guard's view of you is not your
view of the Guard. Only the faction's view is a game score. A Chapter's attitude is whatever its members do.

**What moves standing.** Every change is a logged reason with a cap.

| Event | Change | Cap |
|---|---|---|
| Faction mission completed | +3 to +8 | Per mission |
| A Chapter scene or Story held *with* the faction's members, or in its halls | +1 | 5 a week |
| Trade with the faction's merchants | +1 per 5 marks | 3 a week |
| Tithe or donation | +1 per 2 marks | 4 a week, so standing cannot simply be bought |
| A member's crime against the faction's members or property | Adds to that member's **burden** (4.2a), not to the Chapter's own standing | — |
| Helping the faction's enemy (a mission for a faction it is hostile to or at war with) | −2 to −5 | Per mission |
| Drawing away its residents (doc 16 resentment) | As doc 16 | — |
| Ignoring a levy when allied (4.4) | −10 | Per levy |
| Story actions and DM adjustments | Any | Logged with who and why |

- **Ripples:** a gain or loss with a faction moves its allies by a quarter and its enemies by minus a quarter
  *(placeholder)*, through the faction relations matrix. Helping the Crown pleases the Church and annoys the
  Syndicate.
- **Drift:** each game week standing drifts 1 toward Neutral, never past it. A relationship must be kept up.
- **Exclusive oaths:** a Chapter can be **Sworn** to only one of any two factions that are hostile to each other.

### 4.2a Members' crimes: a burden the Chapter carries (agreed)

A member's crimes weigh on the whole Chapter, but **the weight belongs to the member, not to the Chapter**. If the
Chapter expels them, the weight leaves with them, and the Chapter can win back its standing that way.

- **Two parts to standing.** A faction's working view of a Chapter is:

  ```text
  effective standing = earned standing − Σ burden of the Chapter's current members, as the faction knows them
  ```

  The earned part comes from missions, trade, tithes, levies and ripples (the table above). The burden part is the
  faction's grievance against particular members.
- **What adds burden.** Burden is added toward the faction that was wronged:
  - **an incident** against the faction's members or property in which the offender was identified: −3 to −10
    *(placeholder)*, scaled by the crime (theft less than assault) and by how clearly they were seen;
  - **a conviction** (fined or gaoled by the faction's Watch): −2 more.

  These come from the existing crime system (doc 26 phase 7): incidents, witnesses, identification, warrants. Crimes
  never seen by anyone add nothing.
- **Fading.** Burden fades slowly (1 point a game week *(placeholder)*), faster once restitution is paid. A
  reformed member stops weighing on the Chapter in time.
- **It is not a player's faction reputation.** Burden gives the player nothing and costs them nothing on their own.
  It only weighs on whichever Chapter they belong to. Outside a Chapter it does nothing.
- **It follows the wolf.** A member who is expelled and joins another Chapter brings their burden with them *(for
  discussion)*. Otherwise a friendly Chapter could take in a troublemaker, launder them and send them back.
- **Expulsion only counts once the faction knows.** Removing a member doesn't change standing at once. The burden
  comes off when word of the expulsion reaches the faction: when a member tells one of its officials, or when the
  news travels by rumour (a game day or two). This also keeps expulsion from being a way to test who was guilty.
- **Rejoin cooldown.** An expelled wolf cannot rejoin the same Chapter for 14 real days *(placeholder)*.
- **Scrutiny.** A Trusted or Sworn Chapter's members carry double burden with that faction. More is expected of
  them.

### 4.2b The Chapter doesn't know who did it (agreed)

The Chapter window shows each faction's **standing band** (Known well, Distrusted, and so on). It never shows the
number, and never says whose deeds lie behind it. At most it carries a vague line such as "Something weighs on your
name with the Ser Ferro Guard", shown once a burden is large enough to change the band.

To learn more, the Chapter must **go and ask**, in the world:

- **Who to ask:** an official of the faction, meaning an NPC with a rank in `faction_members` (a Watch sergeant, a
  guild clerk, a house steward). Asking is a conversation. The NPC answers from what the faction knows: the
  incidents, warrants and beliefs its members hold. The rules decide what the NPC knows. The Mind only phrases it.
- **What the report says** depends on what was witnessed and who is asking:
  - It gives **what the faction knows, as it knows it**: the place, the day, the kind of crime, and the offender as
    the witnesses saw them. Witnesses who knew the offender's name give the name. Otherwise it is an appearance:
    "a grey wolf with a torn ear, wearing your colours".
  - **Heard but not seen** gives less: "someone of yours, by the voice of it".
  - **Standing changes the answer.**
    - **Friendly or better:** the faction tells you plainly.
    - **Neutral:** it wants something first, such as a fee or a favour.
    - **Distrusted or Hostile:** it gives little, or refuses ("You know what your wolves did").
  - **The asker's own bond** with the official matters too.
- **Rumour and news.** The same facts also travel through the rumour system (`game.beliefs`), so members may hear
  gossip in a tavern before any official tells them, and gossip can be partial or wrong. A report from an official
  is better evidence than tavern talk. Neither is shown as a certainty.
- **Telling the faction** that a member was expelled is the same kind of visit, in reverse. It is the fastest way
  for the burden to come off.

This keeps the Chapter's internal justice in the fiction. Officers investigate, ask around, confront a member and
decide. The game never hands them a list of names.

### 4.3 What standing gives

| Band | Benefits |
|---|---|
| Known well | Rent the faction's property. First-tier faction missions. Fair prices from its merchants. |
| Trusted | Build a camp on land it claims. Second-tier missions. Its quartermaster sells bonus gear to the Chapter. Its guards hear a member's word over a stranger's (a member's report weighs more with the Watch). |
| Sworn | Fortify and build a Hold on its land. Third-tier missions. Escorts and introductions. Access to its restricted places. Recognition as a minor House (at Hold level). |
| Distrusted | Higher prices. No rentals from it. Its guards watch members more closely (they notice more). |
| Hostile | Its merchants refuse members. Its guards turn members out of its halls. Existing leases on its land lapse at the next rent. |
| Enemy / war | Members show as hostile to its members and vice versa. Its patrols on the roads may stop members. Its structures are closed. |

### 4.4 What standing costs (the drawbacks)

Benefits come with obligations, so choosing a patron is a real decision:

- **Enemies of your friends.** Ripples make rivals cool toward you.
- **Taxes:** a camp or Hold on a faction's land pays a weekly tithe set by the treaty, from the Chapter treasury.
- **Levies:** a Sworn Chapter may be called on: "send four members to guard the harvest road this week". Answering
  earns standing. Ignoring costs it. Levies come from DM stories or a simple schedule.
- **Shared fate:** if your patron goes to war, its enemy treats you as part of it. If it collapses, your licence to
  build on its land goes into doubt (doc 17's succession rules).
- **Scrutiny:** a Trusted or Sworn Chapter's members carry double burden with that faction (4.2a).

### 4.5 Faction missions

A **mission board** in each faction's hall, shown to members of a Chapter that meets the tier.

- Missions are typed, checkable tasks: deliver goods, escort a resident, clear bandits from a road, carry a
  message, guard a place for a time, recover a stolen item, find a missing resident. The world checks completion
  itself. No mission is ever judged by the AI.
- They are drawn from what the world needs: real shortages, real bandit activity on its roads, and real crimes
  against it.
- Rewards are paid from the faction's real purse (doc 15): pennies or marks, standing, and Chapter renown.
- Higher tiers unlock longer, riskier and more political missions, and DM-written story missions.

### 4.6 Treaties (level III and up)

A treaty is a written, logged agreement between a Chapter and a faction. It has typed terms: rent rate, building
rights in named places, tithe, levy obligations, a labour clause (doc 16's treaty modifier on migration
resentment), and its duration. Proposed in the DM or through a faction NPC with authority (a rank in
`faction_members`), and approved by the DM in the first version. Breaking a term costs standing.

---

## Part 5: Territory, a Chapter's ground

The ladder is: **gather → rent → camp → fortified camp → small fortress.**

### 5.1 The meeting place (level I)

- Any public spot an officer names: a tavern corner, a bridge, a clearing. It is not owned, only *declared*.
- It shows on members' minimaps, and scenes held there count as Chapter scenes.
- Two Chapters may meet in the same tavern. Nobody owns the tavern.

### 5.2 Renting (level II)

- **What can be rented** is authored in Atlas or the DM: a room above an inn, a back room, a warehouse, a small
  hall. Each has a `rentable` flag, a landlord (an NPC or a faction), a weekly rent and a capacity.
- **A lease** runs weekly from the Chapter treasury to the landlord's purse.
  - Unpaid rent gives one week's grace, then eviction. Stored goods are held by the landlord for two weeks against
    the debt.
  - The landlord's faction standing gates the lease (Known well or better for faction property).
- **Inside:** the door is locked to all but members and their guests. There is shared storage, a notice board, and
  a place to rest and to log out safely. The interior is the authored one; Chapters may name it and choose a few
  furnishings from a list.

### 5.3 Camps (level III)

A camp is the first ground a Chapter makes its own.

- **Where:** unclaimed wilderness freely, or land a faction claims with Trusted standing or a treaty. Not on roads,
  in towns, in water, on steep ground (elevation, doc 22), or within 20 tiles *(placeholder)* of another Chapter's
  site.
- **What:** a set of **structures** placed on tiles, from a small catalogue: tent, firepit, lean-to, storage pile,
  hitching post, cookfire, watch post, palisade section, gate.
- **Building is play.** An officer lays out a plan. Then members bring materials (bought or gathered) and **work**
  the site with the `build` action. A structure rises with work-hours and shows as half-built until done. Several
  members working together build faster, and a scene held while building counts as a Chapter scene. Building is
  meant to be done together, in character.
- **What a camp gives:** a rest point and safe logout, shared storage, a cookfire to make meals, a place for hired
  NPC staff, and a visible mark on members' maps.

### 5.4 Fortified camp, or Hall (level IV)

- Adds a palisade ring and gate, a timber hall, workshops (a smithy or a tannery from the economy's trades), a
  stable and a well.
- **Hired staff** keep it: a cook and a watch, paid wages from the treasury like any resident.
- Up to **3 sites**. One is the Chapter's seat.

### 5.5 The Hold, a small fortress (level V)

- A stone keep, stone walls, towers and a gatehouse, built over weeks of real play with costly materials.
- The Chapter **claims the cell** it stands on: a faction claim like any other, shown on maps and in the DM.
- **Residents can come:** with houses and jobs built and funded, doc 16's migration pipeline can bring residents
  to live and work there. Their home faction's resentment is shown as doc 16 describes.
- **Sworn NPC members** live there. The Hold becomes a small living town.

### 5.6 Upkeep and decay

- Each structure has a **condition**. It wears slowly with weather and time and is repaired with work and
  materials.
- A site with no member present for 21 real days *(placeholder)* begins to decay faster. Tents fall first and stone
  last. A site left long enough becomes a **ruin**: it stays in the world as scenery and history, does nothing, and
  can be reclaimed.
- **Levels are never lost.** A level V Chapter that loses its Hold is still level V and can rebuild without
  re-earning the rank.

### 5.7 How structures fit the world

- Structures are a **layer**, not terrain. They are stored as `game.structures` rows (owner, place, tile footprint,
  kind, condition, build progress) and sent to clients like doors and objects. They block movement and give
  shelter (weather, doc 22). Ownership of the land itself is never written into the terrain.
- Atlas shows them read-only. The DM can inspect, transfer, repair or remove any of them, with a logged reason.
- **Raids and sieges are out of scope.** Nobody can attack a camp or Hold in this plan (see "Decisions").

---

## Part 6: The system's parts

### Data (Postgres, per the project's preference)

| Table | Holds |
|---|---|
| `game.chapters` | id (also a `live.factions` row of kind `chapter`), name, colour, charter, level, renown, founded day, seat site |
| `game.chapter_members` | chapter, character, rank, joined, last active |
| `game.chapter_rank_log` | who held which rank, when, and by whose hand |
| `game.chapter_renown` | ledger: source ID, kind, amount (zero receipts too), day |
| `game.chapter_treasury` | ledger: deposit, withdraw, rent, wage, tithe, mission pay, with counterpart |
| `game.chapter_hostiles` | chapter, target (character, chapter or faction), reason, set by, when |
| `live.faction_relations` / `_log` | Reused for faction → Chapter standing |
| `game.leases` | property, chapter, landlord, rent, paid-to, state |
| `game.structures` | Above |
| `game.missions` | board, faction, tier, kind, target, reward, taker, state |
| `game.social_*` | Gold Stars and Stories, ported from the prototype's schema |
| `game.acquaintances` | knower, known, day introduced, how (introduced themself, founding scene) |
| `game.faction_burdens` | faction, character, burden, source incidents, last change; plus whether the faction has learned of an expulsion |

Parties are short-lived and kept in the checkpoint, not in tables. The Chapter records now in `dm_service`'s
SQLite move to these tables, and the DM reads them there.

### Wire

- **New commands:**
  - `party.invite`, `party.accept`, `party.leave`, `party.kick`, `party.lead`, `party.goal`
  - `chapter.found`, `chapter.invite`, `chapter.rank`, `chapter.leave`, `chapter.hostile`, `chapter.deposit`,
    `chapter.lease`, `chapter.plan`
  - `build`
  - `mission.take`, `mission.turnin`
  - `introduce`
  - `chat` gains `channel: "party" | "party-ooc" | "chapter" | "chapter-ooc"`. The in-character two take a volume
    and go through perception like local speech.
- **Each visible entity** gains `rel` (`party`, `chapter`, `hostile`, or absent) and, when hostile, a short `why`.
  Its name is sent only if the viewer knows it. Otherwise the entity carries its description label (1.5). The same
  applies to speaker names in every chat message, so the client never receives a name it shouldn't show.
  It is sent only when it changes, which fits doc 31's plan to stop resending unchanged entity details.
- **New owner-only state:** party roster (with members' rough positions for the minimap), Chapter summary (level,
  renown progress, standings, properties) and scene status.

### Cost at scale

- Membership lookups are per character, O(1). A viewer's `rel` for each visible entity checks party (≤8), Chapter
  ID equality, the hostile set (small, per Chapter) and recent attackers (small, per player).
- Group chat fans out to members only. Rate limits match local chat.
- Party NPCs are capped server-wide. Their unprompted lines are budgeted per party.
- Renown and standing settle at events (scene end, mission done), never per tick.
- **Gate for every phase:** `world_check --simulate --players 20` unchanged within noise, plus the doc 31 whole-game
  benchmark once it exists.

### Client

- **Party panel:** roster, leader mark, NPC members' state (following, waiting), invite and leave.
- **Chat tabs:** Local, Party, Party OOC, Chapter, Chapter OOC, OOC.
- **Chapter window:**
  - roster and ranks
  - level and progress to the next (each gate shown separately)
  - renown log
  - standing *bands* with every known faction, never numbers or names behind them (4.2b)
  - properties and sites
  - the hostile list
  - treasury
- **Map:** relation colours and marks (2.4), party dots on the minimap, Chapter sites, mission board markers.
- **Look panel:** the name (if introduced) or description, regard in words, faction membership of NPCs, and
  "hostile · why". It also has an **Introduce** button.

---

## Phases (proposed order)

Each phase ends playable and tested, with the perf gate passed.

| # | Phase | Delivers |
|---|---|---|
| 1 | **Parties** | Invite (by pointing), leave, leader, in-character and OOC party chat, party colours and minimap dots, hostility red (bandits, recent attackers, the other side of a fight once the combat work exists), checkpointed parties. |
| 2 | **Names and introductions** | Description labels, registered true names and aliases, introductions detected in speech plus the Introduce button, receipts, the name each knower was given, NPCs introducing themselves under rules, names withheld on the wire, the Mind prompted with descriptions. It touches every label, so it comes early. |
| 3 | **NPC companions** | Joinable NPCs by friendship, hire or story. Orders, formation, leaving, wages. Party context for the Mind, budgeted interjections, server cap. Bracken folded into the general rule. |
| 4 | **Scenes visible, social widened** | Scene chip, party scenes, half-weight actions, Gold Stars, Stories, titles, regard in Look, "your name in a town", private notes. |
| 5 | **Chapters** | Founding scene, ranks, both chats, colours, hostile list, renown ledger, levels I–II, treasury. Chapter data moved from `dm_service` to Postgres. |
| 6 | **Factions in play** | Relations and members loaded. Standing bands, sources and ripples. Burdens and expulsion, reports from officials and rumour. Faction context in NPC prompts. Prices and guards reacting. Mission boards (tiers 1–2). Template writing for the 14 factions. |
| 7 | **Renting** | Rentable property in Atlas/DM, leases, eviction, locked doors, storage, notice boards. |
| 8 | **Camps** | Structure layer, placement rules, building by work, catalogue, condition and decay, hired staff. Levels III–IV gates live. |
| 9 | **Halls and Holds** | Fortification, stone, multiple sites, Chapter claims, treaties, levies, sworn NPC members, migration to a Hold, minor House recognition. Level V. |

**Note for Phase 1:** when this plan is started, the party system must be built to fit the combat plan (being
designed in a separate thread). Check these against it before building:
- fight requests and joining (can a party join a fight together, and does joining pull in party NPCs?);
- which side party members are on, and what is shown red;
- party NPCs fighting alongside players;
- how a fight affects the party's scene and its chat;
- what happens to a party member who is beaten down or gaoled.

Phases 1–4 need nothing from 5–9 and could start soon. Phase 2 should land before 5, since rosters and reports rely on
descriptions. 6 needs 5. 7–9 need 5 and 6.

## Built

### Phase 1: parties (built 2026-10-02)

- **The rules** (`Core/RatwParty.h`, `Core/RatwParty.cpp`, pure, no world):
  - Invite: by a player in no party, or by the leader. The invitation lasts 60 s, and one waits per player. The party
    exists once it is accepted.
  - Up to 6 players, one party each.
  - Leader actions: invite, send someone away, hand on the lead, disband. Anyone can leave. A leader who leaves hands
    the lead to whoever joined next. A party of one is no party.
  - A member out of the world keeps their place for 10 minutes. A party with nobody in the world goes after 10
    minutes.
  - Each player has a setting to never be called into a party mate's fight.
  - Saved in the checkpoint (`parties`), offline times included.
- **Through the game** (`Core/RatwGameParty.cpp`):
  - `{"type":"party","verb":...}` commands: invite, accept, decline, leave, remove, lead, disband, stayout, autojoin.
    There is also an `invite` action on any player in sight, offered to someone in no party or to the leader.
  - Everyone affected is told in their story column.
- **Chat:**
  - **Party** (`channel: "party"`) is ordinary in-character speech, heard as speech is. Its party listeners get
    `party: true`. Anyone else close by overhears it as plain speech.
  - **Party OOC** (`channel: "partyooc"`) goes to the party's players wherever they are. Both need a party.
- **The fight call-in** (doc 33's "party auto-join"):
  - A party mate who can see a party mate's fight, and isn't fighting, watching it or held, is told "Joining Ash's
    fight…".
  - After 5 s they join on that side, unless they chose Stay out. They are called once per fight.
  - Nobody is called into a fight with party mates on both sides (sparring).
- **Who is who** (each entity's `rel` and `why`, worked out per viewer):
  - `party`;
  - `hostile` with a reason: "fighting you", "fighting your party", "fought you" or "fought your party" (for 5
    minutes after), or "bandit".
- **The snapshot's `self.party`:** members with name, leader, online, cell, place, position, health, downed and
  fighting; an invitation waiting; a fight calling (with seconds); and the auto-join setting.
- **The page** (`Client/src/game/party.ts`, `ui/hud/party.ts`, `story.ts`, `paint.ts`, `minimap.ts`):
  - **On the map:** party mates are drawn in the player's amber with a dot under them. Hostile wolves are red with a
    `!` above.
  - **In Sight** names are coloured the same way and say "your party" or "hostile · why". The Look line says it too.
  - **Minimap:** party mates are amber dots wherever they are, in any place the player knows. Hostile wolves in sight
    are red dots.
  - **The Party panel:** an invitation with Accept/Decline, the fight call-in with Stay out, and members with place
    and state. Leaders get Lead and Remove buttons; everyone gets Leave, Disband (leader only) and the join-their-
    fights setting.
  - **Chat tabs:** PARTY and PARTY OOC, shown only in a party. The PARTY feed shows lines said to the party, and those
    lines are edged in gold in the IN WORLD feed too.
- **Tests:**
  - `Tests/party_tests.cpp`: the rules, then through the game: invitations and the view, the two chats (overheard,
    out of earshot, anywhere), being called into a fight, staying out, auto-join off, and a restart.
  - `Client/src/game/party.test.ts`.
  - In a real browser with three players: `node tools/client/party.mjs` (RATW_SERVER and RATW_WEB pick the server and
    page). It checks the invitation, the party mate on the map, OOC privacy, Cy red while fighting the party, and Bo
    called in on Ash's side.
- **Load:** `game_load` with 100 players (cities): mean 11.5 ms, p99 19.9 ms. The load test's players are in no
  party, so this shows only that parties add nothing when unused.

Not yet: NPC party members (Phase 3; Bracken still follows by the old rule, and companions still join their leader's
fights through the combat code), and the party's own scene for social XP (Phase 4).

### Phase 2: names and introductions (built 2026-10-02)

- **The rules** (`Core/RatwNames.h`, `Core/RatwNames.cpp`, pure):
  - **Hearing an introduction:** an introduction phrase followed by one of the speaker's own registered names, whole
    words, any case, curly apostrophes too. The phrases are "I'm", "I am", "my name is", "name's", "call me", "they
    call me", "you can call me" and "the name is". A name opening the line followed by ", of" or "at your service"
    also counts.
  - What doesn't count: "I'm tired", naming someone else, or talking about oneself.
  - **Aliases:** up to 3, each 2–24 letters (spaces, hyphens and apostrophes only inside). None may repeat the true name
    or another alias.
  - **A stranger's look:** a size or build, then age, coat colour and "wolf", then the most visible marking ("a tall
    russet wolf with white socks"). Colours are named from the nearest of a few.
  - **Veiling the game's own messages:** whole-word, capitalised names replaced by labels. Possessives still count. A
    name followed by another capitalised word ("Ash Hollow") is left alone.
  - **Who knows whom** is one-sided and keeps every name each wolf was given, using the newest. At most 600 per wolf,
    the oldest met dropped first. Saved in the checkpoint (`acquaintances`, `aliases`).
- **Through the game** (`Core/RatwGameNames.cpp`):
  - **What each viewer calls each wolf:** the name they were given, else the wolf's look. A resident is known by their
    trade: "the innkeeper" where they're the only one in the community, "a guard" where there are several. Road folk
    (bandits, carters) go by what they are.
  - Two strangers who look alike are numbered in a fixed order ("a dun wolf (2)").
  - Looks are worked out once a second on the game thread, so views built in parallel only read them.
  - **Where labels replace names:** entity `name` (with `known: false` for strangers), chat speakers and `to`, local
    and party OOC, Look, talk targets, the trader, a challenge, the Party panel, and fighters and side names in fights.
    The game's own messages and fight logs are veiled per reader.
  - **Introductions:** whoever perceived the line learns the name as spoken. A whisper reaches only those close
    enough. The speaker gets a receipt ("You introduced yourself as Kestrel to the innkeeper and a dun wolf."). Each
    listener is told ("A dun wolf is Kestrel.").
  - **Residents giving their names:**
    - When asked ("What is your name?"), if they are willing. A resident is unwilling when their trust in the asker is
      below −20, or when they are a bandit. An unwilling resident answers "My name's my own business."
    - In return when a player introduces themself to a friendly one (affinity ≥ 0).
    - Unprompted once familiarity reaches 50.
    - The language model is told whether it knows the player's name and whether to give its own. The server records
      a name only within these rules.
  - **What a resident calls a player:** the name they were given, else the player's look. This goes for the Mind's
    context, the relationship line, promises and rumours.
  - **Saves from before names were hidden:** wolves already well acquainted with a player (familiarity ≥ 25) keep each
    other's names.
  - `Options::hiddenNames` (on by default) turns it off, for older tests only.
- **The page:**
  - **Introduce** and **Introduce as Kestrel** in a wolf's menu, for anyone within 6 tiles who doesn't know your name.
    It is said aloud as speech (`"I'm Kestrel."`), so hearing decides who learns it.
  - **Your names** on the character sheet: the true name, aliases as chips (× retires one; those who know it keep
    it), and a box to add one.
  - Typing in any text box no longer moves the wolf.
- **Tests:**
  - `Tests/names_tests.cpp`: the rules; strangers and introductions; an alias; a whisper heard only close by; the
    game's messages veiled; residents giving their name in return, keeping it from one they distrust, and telling one
    who asks; a restart; and names shown when not hidden.
  - `Client/src/game/names.test.ts`.
  - Older tests run with `hiddenNames = false`: `Tests/game_tests.cpp` and `Tests/party_tests.cpp`.
  - In a real browser: `node tools/client/names.mjs`. Ash sees Bo as he looks; Bo takes an alias on his sheet and
    introduces himself by it from his menu; Ash then knows him as Kestrel; Bo gets the receipt.
- **Load:** `game_load` with 100 players, all strangers to one another: mean 11.9 ms, p99 22.6 ms (11.5 and 19.9
  before).

Not yet:
- Choosing aliases during character creation; for now they are added on the character sheet.
- Hearsay names.
- Residents who see through aliases.
- Wolves whose looks are identical: development identities all share the default look, so they are told apart only by
  number.

## Decisions

### Agreed 2026-10-02

1. **Player versus player** fights start only by request and acceptance. Once a fight has started, other players
   may join it without permission. The combat design (a separate thread) owns the rules. This plan shows the sides
   in red (2.4). A hostile mark or a war colours wolves red but never starts a fight by itself.
2. **Party chat** has two forms: in character and limited by distance, and OOC at any distance. Chapter chat
   mirrors it.
3. **Names are hidden until a wolf introduces themself,** players and NPCs alike (1.5).
4. **Members' crimes weigh on the whole Chapter** as a burden that belongs to the member. Expelling them removes it
   once the faction learns of the expulsion (4.2a).
5. **The Chapter isn't told who did what.** It must ask the faction's officials, or pick it up from rumour and news
   (4.2b).

### Still open

6. **Party size:** 6 players + 2 NPCs?
7. **Do authored `/me` actions count** toward scenes at half weight?
8. **Chapter size:** no hard cap with sublinear renown *(recommended)*, or a cap (e.g. 50)?
9. **Level names:** Gathering, Lodge, Company, Hall, Hold, or names from the setting?
10. **Founding gate:** 3 founders at social level 5 and a 2-mark fee?
11. **Who approves treaties and minor-House recognition:** the DM by hand at first *(recommended)*, or rules?
12. **Can a Chapter's level drop?** Recommended no; neglect costs property instead.
13. **Does burden follow an expelled wolf** into a new Chapter? Recommended yes, to stop laundering.
14. **Aliases** are allowed (agreed 2026-10-02). Introductions are detected deterministically: a registered name
    after an introduction phrase in the wolf's own speech, plus an Introduce button (1.5). Still open: how many
    aliases, and how true names leak out later.
15. **Joining a party or Chapter:** should it introduce members to each other automatically? The draft says no:
    you introduce yourself in character, and only founders are introduced by the founding scene.
16. **Raids and sieges** on camps and Holds: a later plan, built on the combat work?

## Not in this plan

- Raids, sieges and capturing territory (decision 16). Player fights themselves belong to the combat work.
- Chapter-versus-Chapter diplomacy beyond the hostile list. Wars between Chapters are DM-declared only.
- Player-owned private housing for individuals.
- Taxes collected *by* a Chapter on others (tolls are only mentioned for discussion).
- Multiple characters per account, and how Chapter membership treats alts.
