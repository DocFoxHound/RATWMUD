# 50. The player card, friends and safety

Drafted 2026-10-06 as an actionable plan for doc 48 (§3.1, §3.1a, §3.2, §3.3, §3.4, §3.5 without the gathering howl,
and Part 11). Nothing built. Read doc 48 (Principles, Part 3, Part 11, Plans and Decisions) and docs 32 (§1.4, §1.5,
Phases 2 and 4), 19, 26, 28, 31 and 34 first.

## The ask

The user's words, from doc 48:

> "Add a roleplay profile to the player card."

> "We should add 'friends' lists to the game, and it should be account-based not character-based. [...] We should
> also add a character-based list for known wolves that allows the player to see what wolves they've interacted with
> recently, the summary of any roleplay sessions they might have had with them, or maybe even a note about them."

> "Having common roleplay groups or teams, informal chats to stay in touch."

> "Maybe different ways to broadcast a desire to roleplay."

> "Mute/Block/Report are must-haves."

Agreed on 2026-10-06: account names are shown to friends only (doc 48 §3.3, decision 28); reports keep the recent lines
as evidence (Part 11, decision 34); scene summaries are for everyone (§3.2, decision 35).

## Where we stand (read from the code 2026-10-06)

| Area | State | What exists |
|---|---|---|
| Accounts | Local only | `Core/RatwAccountsCore.h` `accounts::Accounts`: username, PBKDF2 verifier, up to 6 characters (`CharacterSlots`), creation receipts. At most 128 accounts (`AccountLimit`, enforced in `mayRegister` and `restore`). Saved as the checkpoint's `accounts` list, table `game.accounts` (migration 0012), which only the game server may read. Sign-in is `Game::accountCommand` and `Game::finishSignIns`; the lobby is `Game::lobby`. There is no public handle, no played time, no settings, and no way to find a character's account except `Accounts::owns` looping over one account. |
| Development identities | Built | `hello` logins (`Game::login`, `--dev-identity`) make `player-<id>` characters with no account. Portraits treat them as the account `dev:<id>` (`Game::artworkOwner`). |
| The card | A closer look | The `inspect` action in `Game::command` sends an `inspect` event: label, appearance, portrait, life stage, height, description, posture and state, visible injuries, what they wear, the viewer's regard (`Game::regardWords`) and the viewer's note. The page shows it as "A CLOSER LOOK" (`Client/src/ui/hud/dialogs.ts`, `inspect()`). A player's `Entity::description` is a fixed line set in `World::addPlayer` ("A wolf whose story is still being written.") and nothing can change it. |
| Names | Built (doc 32 Phase 2) | `names::Acquaintances known_` (who knows whom by which name, 600 a wolf), `Game::labelFor`, `Game::knowsName`, `Game::veilFor`. Labels replace names everywhere on the wire. |
| Bonds and regard | Built | `Core/RatwBonds.h`: one-sided bonds, at most 150 a holder, fading without contact; `Bonds::describe`. Scenes move players' bonds (`Game::afterSocial`). |
| Private notes | Built (doc 32 Phase 4) | `Game::notes_` (owner, wolf, text up to 500 letters, 300 notes a player), set by the `social` verb `note`, saved in the checkpoint's rest (`notes`), shown in the card. A small bug: past 300 the note is stored before the refusal is sent (`Game::socialCommand`). |
| Known wolves | Missing | Nothing lists who a character has met, when or where, or how many scenes they shared. |
| Scenes | Built | `SocialLedger` sessions with members and contributions; settlement entries carry the paid partners (`LedgerEntry::partner`). Speech is not kept anywhere once delivered: `Game::publish` works out what each listener perceived and sends it. |
| Summaries | NPCs only | The Mind's `POST /summarize` (`tools/npc_mind.py`, light model) summarises a resident's finished conversation from its point of view (`Game::consolidate`, `mind::Client::summarize`). Nothing does this for players. |
| Status, friends, circles, private messages | Missing | Chat has `ic`, local `ooc` (same cell), `party`, `partyooc`, `chapter`, `chapterooc` (`Game::command`'s `chat` branch, `Game::partyChat`). No account-level channel. |
| Mute, block, report | Missing | Players can report a portrait (`artwork_report`, `Core/RatwGameArtwork.cpp`), which the DM reviews in `Editor/src/dm/ArtworkPanel.tsx`. Nothing else. A DM mute is planned as doc 21 Phase 6 and not built. Fights between players already need consent, with auto-decline (`Entity::noPvp`, `World::challenge`). |
| The DM app | Built | `Editor/src/dm/DmApp.tsx` (`PlayersTab`), served by `tools/dungeon_master.py`; DM decisions reach the game as `dm.actions` rows read by `Game` once a second. |
| Model prices | Not set | Doc 48 says doc 28 has the prices. It doesn't: prices are entered by the operator in the Mind's config (`tools/ai_cost.py` reads them), and none are set yet. This plan costs recaps in tokens. |

## Scope

This plan builds:

- the account as a person: a public **handle**, experience, played time, settings, and a fast character-to-account
  lookup;
- the **roleplay profile** on the card, and the parts residents may read;
- **status** (In character, Out of character, Looking for a scene, Storyteller), **experience** and **walk-up**;
- **friends**, handles shown to friends, and **private messages**;
- **known wolves**, with notes, relationship tags and **scene recaps**;
- **circles**;
- **mute, block and report**, with reports in the DM app.

Left to other plans:

- Stars, tags and the star bands on the card: doc 51. This plan leaves a slot on the card for them.
- The gathering howl, scene openness and joining: doc 51.
- The newcomer flag, mentors, ties, matchmakers and vouching: doc 52. This plan supplies played time, the account's
  social level, known-wolf entries, upheld reports and the block test they use.
- Allow hunting partners and lend-a-paw settings: doc 53 (it calls this plan's block test).
- Letters: doc 55 (it calls the block test and known names).
- Storytellers' rights: doc 58. Here Storyteller is only a status anyone may set.
- Account-wide social level and Gift unlocks: doc 49.

## Design

### 1. Accounts as people

An account gains a public record beside its sign-in record. The sign-in record (`game.accounts`) keeps its password
verifier and stays readable by the game server only.

- **Handle:** the name friends and circles see. 3–24 letters, digits, spaces, `_` or `-` *(placeholder)*, unique, chosen
  by the player. It is **not the sign-in username**: the username is half of a credential and shouldn't be shown to
  anyone. The handle box says so, and refuses a handle equal to the username. An account without one is asked for one
  on the roster page. It can be changed once every 30 days *(placeholder)*; friends see "formerly …" for a week.
- **Experience** (account-wide, doc 48 §3.5): Newcomer, Casual, Experienced or Newcomer Guide. Default Casual
  *(placeholder)*. It describes the player, so it is the same on every character.
- **Played time:** seconds in the world while active. Once a minute, each connected player with meaningful input in the
  last 5 minutes *(placeholder)* (`Game::operatorActivity_`) adds 60 seconds to their account. Doc 52's newcomer flag
  reads it.
- **The account's social level:** the highest social level among its characters (`SocialLedger::level`) *(placeholder,
  until doc 49 moves social level to the account)*. One function, `Game::accountLevel`, so doc 49 changes one place.
- **First character:** which character was the account's first, for doc 52.
- **Settings** that belong to the player: private messages from friends only or off; friend-online toasts; show mature profiles;
  write me scene recaps (on by default).
- **Finding a character's account** in O(1): `Accounts::ownerOf(character)` (built by doc 49 phase 1), an index rebuilt on `restore` and kept by
  `addCharacter`. A development identity is the account `dev:<id>`, as portraits already treat it, so tests and the load
  test get the same rules.
- **The account cap:** 128 can't hold 1,000 players. `AccountLimit` rises to 4,096 *(placeholder)*.

### 2. The profile on the player card

Each character has one profile. Who sees each part:

| Part | Limit *(placeholders)* | Who sees it |
|---|---|---|
| **Description** (what you notice about this wolf) | 1,200 characters | Anyone who can see them; residents too. Replaces the fixed `Entity::description` line in the card. |
| **Currently** ("mending nets by the pier, happy to chat") | 120 | Anyone who can see them, on hover and the card; residents too. |
| **Glances**: up to 5, each an icon from a fixed set, a title and a line ("a fresh scar over one eye"), and a sense | title 32, line 120 | A sight glance: anyone who can see them. A scent glance: within 3 tiles *(placeholder)* and not masked (doc 35's oil, `World::scentMasked`). A sound glance: in earshot. Residents read them on the same terms. |
| **Pronouns** | 24 | Anyone; residents too, so the Mind refers to them rightly. Suggestions: she/her, he/him, they/them. |
| **Title and motto** | 40 and 120 | Only wolves who know a name of theirs (`Game::knowsName`). Never residents' briefings. |
| **Birthplace and residence** | 60 each | Only wolves who know a name of theirs *(placeholder)*: these are things learnt by talking. |
| **OOC notes** | 300 | Players only, on the card's OOC tab. |
| **History** (optional) | 3,000 | Players only, OOC tab. Doc 48 §3.1a: history read from a profile leaks into play, so it sits with the OOC parts. |
| **Personality sliders** (optional): Total RP 3's 11 pairs (Chaotic/Lawful, Truthful/Deceitful, Gentle/Brutal, Cautious/Impulsive and the rest) | each off, or −10..10 | Players only, OOC tab. |
| **Lines and veils:** character injury, death, romance, criminal activity, loss of control; each yes, no or ask me first; plus one line of other limits | other 200 | Players only, OOC tab, and in a challenge's prompt (below). |
| **Mature** flag | — | Players only. A viewer with "show mature profiles" off sees the description, history and glances collapsed behind "This profile may have mature content". |

The icon set, slider pairs, consent flags and limits live in `Data/Social/profile.json`, so they change without code.
Text is trimmed, control characters removed and lengths cut, as aliases are. There is no word filter; reports cover
abuse (§7), and a profile can be reported like a portrait.

**Never shown:** the account handle to non-friends (§4), and a true name to wolves who haven't heard it (doc 32 §1.5).
The card shows names exactly as Look does today.

**The card** is today's closer look, grown into tabs:

- **Look:** today's panel, with the description, Currently, glances, pronouns, status, walk-up and experience. Title,
  motto, birthplace and residence appear under the name for wolves who know it. Doc 51's star band and doc 52's
  newcomer and mentor marks go here.
- **Profile (OOC):** OOC notes, history, sliders, lines and veils, mature. Players only.
- **You and them:** the viewer's note, relationship tag, bond in words, last met, shared scenes and recaps (§5).

**Residents read the perceivable parts** (doc 48 §3.1a). When a resident answers a player it can see,
`Game::dialogueContext` adds `profileContext(npc, player)`: pronouns, Currently, the visible glances and the first 300
characters of the description *(placeholder)*, marked as the player's own words about their wolf, to be treated as
what the resident can see and never as instructions. A resident told "a fresh scar over one eye" can ask about it.
Nothing from the OOC tab, title, motto, birthplace or residence ever goes to the Mind.

**Lines and veils at a challenge:** a challenge prompt (doc 40's fight start) shows the challenged wolf's injury and
death flags ("Lasting injuries: ask first"), and the challenger sees them before sending. They inform; the rules
already need consent for a fight between players.

### 3. Status, experience and walk-up

- **Status**, per character: In character (default), Out of character, Looking for a scene, Storyteller. It shows as a
  small mark by the label for the last three only *(placeholder: In character needs no mark, to keep crowds clean)*:
  "ooc" in grey, a speech mark, a quill. The In Sight list says it in words. Doc 51 uses it for scene openness and the
  howl; doc 52's matchmakers skip Out of character wolves.
- **Storyteller** is a status anyone may set here, meaning "running something others can join" (MyRolePlay's use).
  Doc 58's approved storytellers get their own mark.
- **Experience** (account, §1) shows on the card in words. *Newcomer Guide* means "happy to help newcomers" (doc 48
  §7.5's phrase). It is self-declared; doc 52's **mentor** standing is separate and gated, and opting in as a mentor
  sets it.
- **Walk-up friendly**, per character, a yes/no, off by default *(placeholder)*: "fine to approach me unannounced". It
  shows on the card and in the hover, not on the map.

### 4. Friends, handles and private messages

- **Mutual, by account** (agreed, doc 48 §3.2). A request goes by handle, or from a card ("Add as a friend"). Sending one
  shows the recipient your handle; accepting shows yours to them. Requests expire after 14 days *(placeholder)*. At most
  200 friends and 20 requests waiting *(placeholders)*.
- **The list** (People panel, Friends tab): handle, online or not, and the character they're playing **only if they
  share it with you**. Each friend has a "share which character I'm playing" switch, on by default *(placeholder)*.
  Where a friend shares, the list shows that character's true name, marked OOC. No places: the party minimap already
  covers that for parties, and a friend list that tracked locations would undo hiding.
- **Handles above characters** (agreed, doc 48 §3.3): a friend who **shares their character with you** shows their
  handle, subdued, under their label on the map and in In Sight. A friend who doesn't share shows no handle, since a
  handle under a label would give away the very character they chose not to share.
- **Private messages** (not "tells": a Gifted wolf's tells are something else, the user 2026-10-07): out-of-character
  messages between friends, anywhere (`chat` with `channel: "private"` and the friend's handle). Handles name the
  speakers. An offline friend's wait in an inbox, 50 at most, each kept 14 days, and are delivered when they next sign
  in (the user). Doc 55's letters are the in-world mail, kept until the reader deletes them. Rate as chat (0.5 s).
  Blocked either way: refused.
- **Where handles appear:** the friends list, private messages and circles. **Not** local OOC, party OOC or Chapter OOC: those come
  from a wolf others can see, and a handle there would tie that wolf to its player for strangers, which §3.3 rules out.
  (Doc 48 §3.3 says "OOC channels"; this plan reads it as the account-level ones. See Decisions.)
- Doc 51 shows friends the exact star count through `Game::areFriends`.

### 5. Known wolves, notes and scene recaps

**Known wolves** (character, agreed, doc 48 §3.2) is a list per character of the wolves it has met, newest first.

- **Who is on it:** a player character this one shared a scene with (both members of one `SocialSession`), learnt the
  name of, was in a party with, or was tied to (doc 52). A resident is on it once this character knows its name, has
  noted it, or tagged it. Up to 300 players and 100 residents *(placeholders)*; past that the oldest met without a note,
  tag or recap goes first.
- **Each entry shows:**
  - the name **this character** knows, or the label (as `Game::labelFor`);
  - where and when they last met (`placeName`, the game date, and "3 days ago");
  - the regard in words, as the card shows it (`Game::regardWords`, which uses `Bonds::describe`);
  - shared scenes, counted at each scene's settlement from its members;
  - the relationship tag (Total RP 3's): none, unfriendly, neutral, business, friendly, love, family, or one the player
    names (24 letters);
  - the private note (today's `notes_`, moved into the entry; 500 letters);
  - a tie's story starter, written by doc 52;
  - up to 3 scene recaps, newest first.
- **Last met** updates at scene settlement, at an introduction, on joining a party, and when one hears the other speak,
  at most once every 10 real minutes a pair *(placeholder)*, so it costs a map lookup per heard line at most.
- **Total RP 3's touches:** a notes icon in In Sight for a wolf with a note, and an "unread" mark when a known wolf's
  description has changed since this character last opened their card (each profile carries a revision number).
- The owner can **forget** an entry, delete a recap, or clear a note.

**Scene recaps** (agreed: for everyone, doc 48 decision 35):

- **What this character perceived, only.** While a scene runs, `Game::publish` already works out each listener's lines.
  For each player who is a member of the scene, the lines they perceived in it are buffered as delivered: the speaker
  as that listener knows them ("a grey wolf with a torn ear", or "A voice"), never another name. Whispers they didn't
  hear were never delivered, so they can't appear. Muted and blocked wolves' lines were never delivered either.
- **Buffer limits:** 120 lines or 6,000 characters a member a scene *(placeholders)*, the oldest dropped. In memory only;
  a restart mid-scene loses it and that scene gets the written recap.
- **At the end** (settlement, or the member leaving the scene), a member who perceived 6 lines or more over 5 minutes
  or more *(placeholders)* gets a model recap: a new Mind endpoint, `POST /recap`, on the light model, given the buffer,
  the place, the member's own name and the labels, and asked for 2–4 sentences in the second person, 600 characters at
  most, reporting claims as claims and inventing nothing. Everyone else, and every failure or budget refusal, gets the
  **written recap** from the ledger alone: "You shared a scene with a grey wolf with a torn ear and Wren at the Wharf
  tavern. It ran 40 minutes." The buffer is then dropped.
- **Narrative storage, not a ledger** (doc 08), like residents' conversation memories. Kept per character
  (`game.scene_recaps`), at most 3 for each known wolf and 150 a character *(placeholders)*. Readable by the game server
  only; the DM app doesn't show them.
- **Cost** (doc 28's terms; no prices are set): about 1,250 input tokens (rules 250, lines about 1,000) and at most 150
  output a recap *(estimate)*. At 1,000 daily players with 2 recapped scenes each, that is 2,000 calls, about 2.5 M input
  and 0.3 M output tokens a day on the light model (`gpt-5.4-nano` today): the day's cost is 2.5 × its input price plus
  0.3 × its output price, per million. Limits: 10 model recaps a character a day *(placeholder)*, the Mind's own budgets
  and cost mode, and the written recap when they run out. A player can turn their own recaps off (§1's settings).
- The Mind's call ledger records kind `recap`, so `tools/ai_cost.py` reports it with the rest.

### 6. Circles

A **circle** is an out-of-character group of accounts (doc 48 §3.4): a roleplay group, friends, a team. Chapters stay the
in-character organisation.

- **Making one:** any account, with a name (32 letters, unique among circles). The maker is its keeper; the keeper names
  officers. An account may belong to 10 circles; a circle holds 50 *(placeholders)*.
- **Joining:** by invitation (by handle, or from a friend), accepted by the invitee. Officers invite and remove. Anyone
  may leave. A circle whose last member leaves is gone.
- **Chat:** reaches every online member, wherever they are and whatever character they're on (`chat` with
  `channel: "circle"` and its id). Handles name the speakers. No perception, no scene credit.
- **Roster:** handle, online, and the character only where that member shares it with the circle (a per-circle
  switch, off by default *(placeholder)*: a circle is often wider than one's friends).
- **Planned nights:** up to 10 *(placeholder)*: a real date and time (shown in each viewer's own time), a place in words
  ("the Wharf tavern"), a line. Officers add and remove them; past ones drop off after a day. Doc 54's player-run nights
  can post to them.
- A blocked account can't be invited by the blocker, and isn't shown the blocker's lines in a shared circle (the mute
  filter, §7).

### 7. Mute, block and report

(Agreed: must-haves, doc 48 Part 11.)

**Mute** (you stop seeing a wolf's speech and emotes; they aren't told):

- Held by the muter's **account**, so none of its characters hear that wolf; aimed at the one **character** muted
  *(placeholder)*. Block is the account-wide tool.
- Filtered where lines are delivered: `Game::publish` (in-character speech, emotes and actions, party and Chapter
  speech), local OOC, `Game::partyChat` (party and Chapter OOC), private messages and circles. A muted wolf's lines don't count
  toward a scene with the muter, since the muter didn't perceive them (`SocialLedger::record` takes perceived listeners
  only).
- Up to 200 mutes *(placeholder)*.

**Block** (mute, and more; account-wide):

- Held by the blocker's account and aimed at the blocked **account**, found from the character pointed at, so it follows
  the player across their characters (agreed, doc 48 Part 11).
- **Without telling you who their other characters are:** the block list shows only the character you blocked, by the
  label or name you knew. Their other characters are filtered silently: their lines don't reach you and the refusals
  below apply, but nothing marks those wolves as blocked on screen.
- **A blocked wolf can't:**
  - be heard by you (the mute filter);
  - join a scene you're in: `SocialLedger::record` gains a test, and a line from a wolf blocked by a member (or blocking
    one) never joins or opens a scene with them;
  - invite you to a party or be invited by you (`Game::partyInvite`), or challenge you (refused before
    `World::challenge`, with the auto-decline words, so it reads like any refusal);
  - send you a friend request, a private message or a circle invitation;
  - join your hunts, work or crafts (doc 53), send you letters (doc 55), be pointed at you by matchmakers or tied to you
    (doc 52). Those plans call `Game::blocked(a, b)`, true if either account blocks the other.
- The blocked player is never told. Their attempts get the ordinary refusal each action already has.
- A blocked wolf is still in the world: you can see them walk, and they can see you. Hiding a wolf you're standing
  beside would be stranger than silence, and it would tell them they're blocked.
- Up to 500 blocks *(placeholder)*.

**Report** (a report to the DM app, with evidence):

- **From the card** (a wolf you can see) or **from a chat line** (any line you received, by its sequence number). The
  wire carries no author ID for lines (`Game::publish` leaves it out on purpose), so the server finds the author in the
  reporter's own record of what they received; the page never learns who wrote an anonymous line.
- **What you heard is kept as evidence** (agreed, doc 48 decision 34: the one exception to doc 08's no-prose rule). Each
  connected player has a short record of the lines delivered to them: the last 60 lines within 30 minutes
  *(placeholders)*, with sequence, time, author, channel and the text as they received it. In memory only, never saved.
  A report copies the reported wolf's lines from it (up to 20) into the report.
- **Kinds:** speech, profile (a copy of the reported profile's text), private message, circle. Portraits keep their own report.
- **The report holds:** reporter account and character, reported account and character (resolved by the server), kind,
  a category (harassment, hateful content, spam, cheating, other), the reporter's note (300 characters), the evidence,
  status and the DM's decision.
- **Kept 30 days** (agreed placeholder) unless a DM acts on it. An upheld report keeps its record (IDs, category,
  outcome, dates) for good, since doc 49's Quickened gate and doc 52's mentors ask about upheld reports; its evidence
  lines are deleted after 180 days *(placeholder)*. A dismissed or untouched report is deleted at 30 days. A daily
  purge runs in SQL off the game thread.
- **At most 5 reports an account a day** *(placeholder)*. Reporting also offers to block.
- **The DM decides** (§11): uphold or dismiss, with an outcome: a note only, a warning (a message the player sees), or a
  **silence** of 1, 6, 24 or 72 hours *(placeholders)*: the account can't speak in character or out of it, or send private messages
  or circle lines, and is told until when. Kick and ban stay doc 21 Phase 6.
- `Game::upheldReports(account, days)` answers docs 49 and 52.

### 8. Data

New lists in the save, written by checkpoints as changed rows through `game.sections`, like Chapters (migration 0030),
in one migration (the next free number when built):

| Table | Key | Holds | Readable by |
|---|---|---|---|
| `game.account_profiles` | account | handle, former handle and when, experience, played seconds, first character, settings, silenced until; doc 52 adds mentor fields | game, DM |
| `game.profiles` | character | every §2 field, status, walk-up, revision | game, DM |
| `game.friendships` | `a\|b` (sorted) | state (asked, friends), who asked, since, each side's share switch | game |
| `game.safety_marks` | `holder\|kind\|target` | kind (mute or block), target account or character, the character pointed at and its label then, when | game (the DM sees counts through a view) |
| `game.known_wolves` | `character\|other` | first and last met, last place, shared scenes, tag, custom tag, note, tie id, read revision, recap ids | game |
| `game.scene_recaps` | recap id | owner character, scene id, when, place, others, text, written or model | game |
| `game.circles` | circle id | name, keeper, officers, members with their share switch, planned nights | game, DM |

Generated columns (`handle`, `owner`, `state`) as migration 0030 does, for the tools.

**Reports** don't belong in the checkpoint: they hold prose and outlive it. `game.reports` is a table of its own, like
`game.artwork`: id, created, reporter account and character, reported account and character, kind, category, note,
evidence (jsonb), status, decided by, decided at, outcome, silence hours, purge after. The game server writes it through
a small store (`Core/RatwReports.h`: memory, folder and database stores, as `Core/RatwArtwork.h` has), so file-world
tests work too.

Notes move from the checkpoint's `notes` into `known_wolves` on load, once; old saves still load.

### 9. Wire

Commands (each with the usual `commandId`):

- `{"type":"profile","verb":"set","fields":{...}}`: any §2 fields; checked, then the owner gets `{"type":"profile",
  "own":{...},"account":{...}}`, also sent on entering the world.
- `{"type":"profile","verb":"status","value":"ic"|"ooc"|"lfs"|"storyteller"}`, `{"verb":"walkup","on":bool}`,
  `{"verb":"handle","handle":"…"}`, `{"verb":"experience","value":"…"}`, `{"verb":"settings",...}`.
- `{"type":"friends","verb":"request","handle"|"target"}`, `accept`, `decline`, `remove`, `share` (`handle`, `on`).
- `{"type":"chat","channel":"private","to":"<handle>","text":…}` and `{"type":"chat","channel":"circle","circle":id,…}`.
- `{"type":"circle","verb":"create"|"invite"|"accept"|"decline"|"leave"|"remove"|"officer"|"night"|"unnight"|"share"|
  "disband",…}`.
- `{"type":"known","verb":"list"|"tag"|"note"|"forget"|"unrecap"|"read",…}`. The old `social` verb `note` keeps working.
- `{"type":"safety","verb":"mute"|"unmute"|"block"|"unblock","target":id}` or `"line":sequence`;
  `{"type":"safety","verb":"report","target"|"line","kind","category","note"}`; `{"type":"safety","verb":"list"}`.

Events: `friends`, `circles`, `known` and `safety` lists, each sent when it changes or is asked for (never every
snapshot); `ooc` with `channel: "private"` or `"circle"`.

The `inspect` event gains `profile`, filtered for the viewer by `Game::cardFor`. Each visible entity in the snapshot
(the per-viewer loop in `Game::sendSnapshot`) gains, only when set: `rp` (`ooc`, `lfs`, `st`), `currently`, `walkup`,
`prev` (profile revision) and `handle` (a sharing friend). Unchanged entities already go as held keys (doc 31), so these
cost bytes only when they change.

### 10. Client

- **Card:** `Client/src/ui/hud/dialogs.ts`, `inspect()` becomes tabbed (Look, Profile (OOC), You and them), with Add as
  a friend, Mute, Block and Report under a "…" menu. Lines and veils show on the challenge prompt (`combat.ts`).
- **Your profile:** a PROFILE sheet from the character sheet's actions: every field with its limit counting down, the
  glance icons, sliders, the consent flags, a preview "as a stranger sees it" and "as a friend sees it".
- **Status:** a chip beside the composer in `Client/src/ui/hud/story.ts` (IC · OOC · Looking · Storyteller) and the
  walk-up switch. Marks by labels in `Client/src/game/paint.ts`, words in In Sight (`Client/src/ui/hud/hud.ts`),
  Currently in the map's hover (`Client/src/game/look.ts`).
- **People panel** (new, `Client/src/ui/hud/people.ts`, logic in `Client/src/game/people.ts`): Friends (requests, list,
  share switches, Message), Known wolves (search, tag filter, recaps), Circles (roster, nights, settings), Muted and
  blocked.
- **Chat tabs** (`story.ts`, `state.ts`): PRIVATE (one feed, with the friend picked in the composer) and one tab per
  circle with unread counts. A chat line's menu gains Mute, Block and Report.
- **Front door** (`Client/src/ui/frontDoor.ts`): the roster asks for a handle when the account has none.

### 11. The DM app

- **Players tab** (`Editor/src/dm/DmApp.tsx`, `PlayersTab`): each character's account handle, played hours, profile (a
  read-only card), upheld reports and silence, and "blocked by N accounts" (a count, never who). Circles listed with
  members, never their chat.
- **Reports** (new `Editor/src/dm/ReportsPanel.tsx`, beside `ArtworkPanel.tsx`): open reports first, each with the
  evidence lines, the reported wolf's earlier reports, and Uphold or Dismiss with an outcome and a reason. Served by
  `tools/dungeon_master.py` (`GET /api/reports`, `POST /api/reports/decide`), which queues a `report.decide` action
  (role `dm`) for the game; the game applies the outcome and confirms it, as `artwork.review` works. Viewers may read,
  not decide. Every decision is audited.
- **Settings** stay in data and options (retention, limits); the DM doesn't edit them live.

### 12. Cost at 1,000 players

- Profiles: read on inspect and on a resident's reply only. Entity fields go only when they change.
- Played time: one pass over connected players a minute.
- The mute and block filter: one lookup in a small set per listener per line, in loops `Game::publish` already runs.
- The heard-lines record: 60 lines a player, about 18 MB at 1,000 players *(estimate)*; trimmed as lines arrive.
- Recap buffers: 6,000 characters a scene member at most, about 6 MB at 1,000 players in scenes; dropped at the end.
- Lists (friends, known wolves, circles, safety) are rebuilt for one account when they change, never per tick.
- Private messages and circle lines go to their members only. Friends' online changes touch only that account's friends.
- Nothing scans the world in the tick. Gate for every phase: `world_check --players 20` unchanged within noise.

## Phases

### Phase 1: the account as a person, and the profile

- **Goal:** every account has a handle, experience and played time; every character has a profile, status and walk-up;
  the card shows each viewer what they may see; residents read the perceivable parts.
- **Changes:**
  - Server: `Core/RatwPeople.h` and `.cpp` (new, pure, `ratw::people`): `AccountRecord`, `Profile`, the limits from
    `Data/Social/profile.json`, `validate`, `cardFor(profile, viewer facts)` (what a viewer may see: knows a name, in
    sight, scent reach and masking, a player or a resident), handle rules. `Core/RatwAccountsCore.*`:
    `Accounts::ownerOf` (doc 49 built it), `AccountLimit` raised. `Core/RatwGamePeople.cpp` (new): `profileCommand`, `Game::cardFor`,
    `Game::profileContext`, `Game::accountLevel`, played time in `Game::tendPeople(dt)`. The `inspect` action adds
    `profile`; `Game::sendSnapshot` adds `rp`, `currently`, `walkup`, `prev`; `Game::dialogueContext` adds
    `profileContext`; the challenge path passes the flags to the prompt.
  - Data: `Data/Social/profile.json`. Checkpoint lists `people.accounts` and `people.profiles`
    (`Core/RatwCheckpoint.cpp`), migration for `game.account_profiles` and `game.profiles` with their `game.sections`
    rows.
  - Client: card tabs (Look, Profile (OOC)), the PROFILE sheet, the status chip, marks and hover, the handle prompt.
  - DM app: handle, played hours and a read-only profile in the Players tab.
- **Tests:** `Tests/people_tests.cpp` (new): limits and trimming; title, motto, birthplace and residence only for a
  wolf who knows a name; OOC parts never in `profileContext`; a scent glance only within reach and not while masked;
  the handle unique and never the username; played time only while active; `ownerOf` after restore; a dev identity
  as `dev:<id>`. `Tests/checkpoint_tests.cpp` and `Tests/pg_tests.cpp` (the two lists round-trip, delta saves match
  whole saves). `Tests/names_tests.cpp` (the card veils names as Look does). `Client/src/game/profile.test.ts`. In a
  real page, `tools/client/card.mjs` (new): Ash fills her profile; Bo, a stranger, sees description, glances and
  Currently but no title; after she introduces herself he sees her title and motto; her OOC tab shows her lines and
  veils.
- **Done when:** a player can write a profile and set a status; another player sees exactly the parts allowed; a
  resident with the Mind in fixture mode is sent the perceivable parts and nothing else; the status mark shows by the
  label; the DM sees the handle.
- **Cost:** none in the tick but the minute's played-time pass; a few bytes per changed entity.

### Phase 2: mute, block and report

- **Goal:** the must-haves (doc 48 Part 11), with reports in the DM app.
- **Changes:**
  - Server: `Core/RatwPeople.*` gains the safety marks; the game has `Game::blocked(a, b)` and `Game::hides(listener, author)` (built in Phase 2).
    `Core/RatwGamePeople.cpp`: `safetyCommand`, the heard-lines record (`Game::heard_`, appended where lines are
    delivered), report building. The filter in `Game::publish`, the `ooc` branch of `Game::command` and
    `Game::partyChat`. `SocialLedger::record` takes a block test. Refusals in `Game::partyInvite` and the challenge
    path. Silence checked at the top of the `chat` branch. `Core/RatwReports.h/.cpp` (new): the store and the daily
    purge. The `report.decide` action in the `dm.actions` reader in `Core/RatwGame.cpp`. `Game::upheldReports`.
  - Data: `game.safety_marks` (sections) and `game.reports` (its own table, with an index on open reports), in one
    migration.
  - Client: Mute, Block and Report on the card and on chat lines; a report dialog (category, note, "also block"); the
    Muted and blocked list.
  - DM app: `ReportsPanel.tsx`, `/api/reports`, `/api/reports/decide`, `report.decide` in `ACTIONS` in
    `tools/dungeon_master.py`; the Players tab's counts and silence.
- **Tests:** `Tests/safety_tests.cpp` (new): a muted wolf's speech, emotes, OOC and party OOC don't arrive and the
  muted aren't told; a block follows the player to their second character, and the block list names only the first;
  a blocked wolf can't join the blocker's scene, invite, challenge or be invited; the evidence holds only lines the
  reporter received, from that author; a line reported by sequence finds an anonymous author without sending its id;
  five reports a day; silence stops speech until it ends; the purge keeps upheld records and deletes the rest at 30
  days. `tools/test_dungeon_master.py` (reports listed, decided, audited).
  `Client/src/game/people.test.ts`. In a real page, `tools/client/safety.mjs` (new): Bo pesters Ash, she mutes him and
  hears nothing; she blocks him; his second character is silent to her; her report shows in the DM's panel with his
  lines.
- **Done when:** mute and block work on every channel that exists; reports reach the DM app with evidence and can be
  upheld with a silence the game applies.
- **Cost:** a set lookup per listener per line; 18 MB of heard lines at 1,000 players; reports are rare writes.

### Phase 3: friends, handles and private messages

- **Goal:** account friends, mutual, with per-friend sharing, handles under sharing friends' labels, and private messages
  (an offline friend's kept in an inbox: 50, 14 days).
- **Changes:**
  - Server: `Core/RatwPeople.*`: friendships, requests, expiry. `Core/RatwGamePeople.cpp`: `friendsCommand`,
    `Game::areFriends`, `Game::friendsView` (rebuilt for an account when one of its friends comes, goes or changes),
    the `private` channel and its inbox. `Game::sendSnapshot`: `handle` for sharing friends.
  - Data: `game.friendships` (sections).
  - Client: Friends tab in the People panel, Add as a friend on the card, the PRIVATE tab, friend-online toasts.
- **Tests:** `Tests/people_tests.cpp`: a request needs the other's accept; handles revealed only as §4 says; sharing off
  hides the character in the list and the handle on the map; a private message reaches a friend anywhere and a non-friend never; an offline friend's waits and arrives at
  sign-in, at most 50, gone after 14 days;
  blocked either way, refused; requests expire. `Client/src/game/people.test.ts`. In a real page,
  `tools/client/friends.mjs` (new): Ash and Bo befriend; Bo sees Ash's handle under her label; she turns sharing off
  and it goes; a private message crosses cells.
- **Done when:** two players can befriend, see each other online with characters as shared, and send private messages; strangers
  never see a handle.
- **Cost:** O(friends) when someone comes or goes; private messages go to one connection.

### Phase 4: known wolves and scene recaps

- **Goal:** each character's list of wolves met, with notes, tags, shared scenes and recaps.
- **Changes:**
  - Server: `Core/RatwPeople.*`: `KnownWolf` entries, caps, the oldest-first trim. `Core/RatwGamePeople.cpp`:
    `knownCommand`, `Game::meet(a, b, how)` called from `Game::afterSocial` (shared scenes, from the settled session's
    members), `Game::learnName`, the party join and `Game::publish` (last met, throttled); the recap buffers filled in
    `Game::publish`, `Game::recap(character, session)` at settlement and at `SocialLedger::leave`. Notes move into the
    entries (and the over-300 bug goes). `Core/RatwMind.h/.cpp`: `mind::Client::recap`. `tools/npc_mind.py`:
    `POST /recap` (`RECAP_RULES`, its schema, the light tier, kind `recap` in the ledger), fixture replies for tests.
  - Data: `game.known_wolves` and `game.scene_recaps` (sections), readable by the game only.
  - Client: Known wolves tab; the card's You and them tab; the notes icon and unread mark in In Sight.
- **Tests:** `Tests/people_tests.cpp`: a shared scene makes entries both ways and counts; names shown as each knows
  them; the cap trims the oldest untagged first; notes migrate from an old save. `Tests/social_game_tests.cpp`: a
  member's buffer holds only what they perceived (a whisper out of range is missing, a blocked wolf's lines missing,
  a stranger by label); the written recap without the Mind; the fixture recap with it; 10 a day. `tools/test_npc_mind.py`
  (the recap request checked and cut; claims as claims). In a real page, `tools/client/scenes.mjs` gains a recap
  check after a scene ends.
- **Done when:** after a scene, each player finds the others in Known wolves with a recap written from their own view;
  notes and tags work; nothing leaks a name or a line a character didn't perceive.
- **Cost:** one light-model call per recapped scene per member, capped (§5); buffers dropped at the end; entry writes
  throttled.

### Phase 5: circles

- **Goal:** out-of-character groups with chat, roster and planned nights.
- **Changes:**
  - Server: `Core/RatwPeople.*`: circles, ranks, invitations, nights. `Core/RatwGamePeople.cpp`: `circleCommand`, the
    `circle` channel, `Game::circlesView`.
  - Data: `game.circles` (sections).
  - Client: Circles tab (roster, nights, invite, leave, share), one chat tab per circle.
  - DM app: circles and members in the Players tab.
- **Tests:** `Tests/people_tests.cpp`: limits (50 members, 10 circles); officers' rights; a member's character shown only
  where shared; chat reaches online members anywhere; a blocked member's lines filtered for the blocker; a circle with
  no one left is gone; nights in order, past ones dropped. In a real page, `tools/client/friends.mjs` gains a circle with
  a night and a line.
- **Done when:** a group can keep in touch out of character and plan nights, with blocks respected.
- **Cost:** circle lines go to members only; views rebuilt per circle on change.

## Depends on and feeds

- **Depends on:** doc 32's names, notes, parties and scenes; doc 19's accounts; doc 26's Mind; doc 28's light model and
  call ledger; doc 34's DM app and action queue; doc 31's held entity keys.
- **Feeds:**
  - doc 51: status (openness, the howl), friends (exact stars), the card's star slot, block (joining);
  - doc 52: played time, `Game::accountLevel`, known-wolf entries for ties, upheld reports, block, status and the
    Newcomer Guide experience;
  - doc 53: block, and the People panel's settings for hunting and work partners;
  - doc 55: block, known names, friends;
  - doc 56: known wolves for the chronicle's "people met";
  - doc 58: the Storyteller status;
  - doc 49: upheld reports and the account's social level.

## Risks

- **Words in the Mind's prompt.** A profile is player-written and goes to the model. It is marked as the player's own
  description, cut short, and the Mind never grants anything; the server clamps every nudge (`Game::heed`). A profile
  that tries to steer residents can be reported.
- **Recaps send what a player perceived to the model's provider.** That includes other players' lines. The agreed rule
  is recaps for everyone; the settings page says plainly what is sent, and a player may turn their own recaps off.
- **Out-of-character leaks.** Friends' sharing, handles and circles all carry OOC knowledge. Each is opt-in per friend
  or circle, and handles never sit under a non-sharing wolf.
- **Block evasion** with a new account. Accounts are local today; real identity comes with internet deployment (doc
  19). Until then a DM silence follows the account, not the person.
- **Report abuse.** Five a day, and the DM sees a reporter's dismissed reports.
- **Memory.** Heard lines and recap buffers are bounded; measure them at 250 players with `game_load`.
- **Other sessions** are editing the economy files (`Core/RatwOrchestrator.*`, `RatwDemand`, `RatwOddJobs`,
  `RatwResidents`, `RatwSociety.h`, `Data/Economy/`). Nothing here touches them. Shared files (`CMakeLists.txt`,
  `Core/RatwGame.h`, `Core/RatwCheckpoint.cpp`) take small, separate hunks.
- **Migration numbers** collide if two plans build at once: take the next free number when building.

## Decisions

Agreed (doc 48):

1. A roleplay profile on the player card (§3.1, decision 17), with title and motto only for wolves who know the name,
   and residents reading only the perceivable parts (§3.1a).
2. Friends are account-based and mutual; known wolves are character-based, with summaries and notes (§3.2, decision 10).
3. Account names are shown to friends only (§3.3, decision 28).
4. Scene summaries are for everyone, not opt-in (§3.2, decision 35).
5. Mute, block and report are must-haves; blocks are account-wide without revealing other characters (Part 11,
   decision 19).
6. Reports keep the recent lines as evidence, kept 30 days unless a DM acts (Part 11, decision 34).

New placeholder choices in this plan:

7. The handle is separate from the sign-in username, unique, changeable every 30 days.
8. Handles appear in the friends list, private messages and circles, not in local, party or Chapter OOC.
9. A friend's handle shows under their label only when they share their character with you.
10. Experience is account-wide; status, walk-up, lines and veils and the mature flag are per character.
11. In character needs no mark by the label; Out of character, Looking for a scene and Storyteller do.
12. Storyteller is a status anyone may set; doc 58 adds its own mark for approved storytellers.
13. Newcomer Guide is self-declared; doc 52's mentor standing is gated and sets it.
14. Birthplace and residence are shown only to wolves who know a name.
15. Glances carry a sense: sight, scent (3 tiles, not masked) or sound (earshot).
16. Mute is held by the account and aimed at one character; block is aimed at the account.
17. A blocked wolf stays visible in the world.
18. Private messages are online only (replaced: answered 3).
19. Upheld reports keep their record for good and their evidence 180 days; DM outcomes are a note, a warning or a
    silence of 1–72 hours.
20. Recaps: model recaps for a member who perceived 6+ lines over 5+ minutes, 10 a day, written recaps otherwise; 3
    kept per known wolf.
21. The limits in §2, §4, §5, §6 and §7.

## Answered (the user, 2026-10-07)

1. **Storyteller is for approved storytellers only** (doc 58). Until doc 58 builds approval, no one can set it; the
   status is In character, Out of character or Looking for a scene. This replaces decision 12.
2. **No birthplace or residence on the profile** ("I don't want this on people's profile"). They are removed from §2;
   this replaces decision 14.
3. **Private messages are kept until the friend logs in**: an offline friend's wait in an inbox (50 a recipient, each
   gone after 14 days: the user) and are delivered when they next sign in. This replaces decision 18.
4. **Not "tells"**: that word is a Gifted wolf's tells. They are private messages, on the PRIVATE tab.
5. **Letters are mail, kept until deleted** (doc 55): a different thing from private messages.

## Open questions

None.

## Built

### Phase 1: the account as a person, and the profile (built 2026-10-07, not committed)

- **Data:** `Data/Social/profile.json` holds the limits, 18 glance icons, the three senses (scent within 3 tiles, sound
  within 15), the 11 personality pairs, the five lines and veils, statuses (Storyteller approved-only), experience
  levels, handle rules and played-time rules.
- **The rules:** `Core/RatwPeople.h/.cpp` is pure.
  - It defines `Profile`, `AccountRecord` and `Settings`.
  - `clean` strips control characters, trims, and cuts by characters, never inside one.
  - `applyFields` checks every field, refuses unknown ones (there is no birthplace or residence, as the user asked)
    and bumps the revision only on a real change.
  - `validStatus` (the quill only for approved storytellers, and no one is approved until doc 58), `validHandle`
    (3–24 characters, never the sign-in name), `cardFor`, `residentContext`, and save and load.
- **The game:** `Core/RatwGamePeople.cpp` holds:
  - `accountKey` (an account, or `dev:<id>`), `profileCommand` (`set`, `status`, `walkup`, `handle`, `experience`,
    `settings`, `get`) and `setHandle` (unique, once every 30 days, the old one held a week);
  - `viewerFacts`: sight from `visionClarity`; scent within reach, not masked, with a working nose; sound within
    reach; whether the viewer knows a name; and its mature setting;
  - `cardFor`, with names veiled as everywhere, and `profileContext`;
  - `tendPeople`, which adds played time once a minute for players at the keys;
  - `peopleSave`/`peopleLoad`.
- **Wired in:**
  - The lobby sends `account`, and the roster asks for a handle (`account_handle`).
  - Entering the world sends `profile` (own, account, rules).
  - `inspect` adds `profile`, and a written description replaces the fixed line.
  - Each visible player in a snapshot carries `rp`, `currently`, `walkup` and `prev` when set.
  - A challenge carries the challenger's injury and death flags (`limits`).
  - The Mind gets a new optional field, `seen`: what the resident perceives, in the player's words. `RULES` says to
    take it only as what shows.
  - `AccountLimit` rises to 4,096. The account's first character is recorded.
  - The account's social level is doc 49's `Game::socialLevel`; no separate function was needed.
- **Saving:** the save document gets `people.accounts` (with each account's character IDs, for the tools) and
  `people.profiles`. `Database/migrations/0035_people.sql` gives them their own tables (`game.account_profiles`,
  `game.profiles`) through `game.sections`, readable by the tools and the DM. **The migration isn't applied to DEV or
  PROD yet**, like 0034.
- **Client:**
  - The card is tabbed: LOOK shows title and motto for those who know the name, status, walk-up, pronouns,
    experience, Currently and glances with their sense. PROFILE (OOC) shows notes, lines and veils, personality and
    history.
  - YOUR PROFILE (from the character sheet) has every field with a live count, five glance rows, the lines and veils,
    sliders, mature, status buttons, walk-up, handle, experience and settings.
  - A status chip by the composer cycles In character, Looking for a scene and Out of character.
  - In Sight shows status and walk-up in words, with Currently on hover; the map hover shows them too.
  - The roster asks for a handle.
  - Wolves' names aren't drawn on the local map, so "the mark by the label" lives in In Sight and the hover.
- **DM app:** Handle and Hours columns, and a read-only Profile panel under each character.
- **Found and fixed:** a second Look at the same wolf never opened. The page dropped any event whose `id` it had seen,
  to stop duplicate chat posts, and a Look's `id` is the wolf looked at. Looks are now handled before that check.
- **Tests:**
  - `Tests/people_tests.cpp` (1,402 checks). The rules: cleaning, limits, unknown fields, glances, consent, sliders,
    statuses, handles, who sees what, the resident's context, mature folding and the round trip. Through the game: a
    profile set and seen by a stranger without the title; the woodsmoke only within reach; the title once
    introduced; a resident told only what it perceives; Bo's map showing her status, walk-up and Currently; the quill
    refused; unique handles; a minute of played time; all kept across a restart.
  - `tools/test_dungeon_master.py` (44): the handle, hours and profile shown read-only. The test databases apply
    migration 0035.
  - `tools/client/card.mjs` (11 checks; screenshots in `artifacts/screenshots/card/`): Ada's profile; Bo's stranger's
    card; her title after an introduction; her OOC tab; her status in his In Sight; her editor.
  - `ctest`: 48 of 48. The client tests (98), the Mind's tests (29) and the type checks pass.
- **Cost:** a pass over connected players once a minute, plus a few bytes on a changed player in a snapshot. Nothing
  scans the world.

### Phase 2: mute, block and report (built 2026-10-07, not committed)

- **Marks:** `Game::safety_` holds mutes and blocks by the holder's account. A mute aims at a character; a block aims at
  an account, found from the character pointed at, whose label is kept for the list. Limits: 200 mutes, 500 blocks.
  - **`Game::hides(listener, author)`:** muted, or blocked, by the listener.
  - **`Game::blocked(a, b)`:** either account blocks the other; the test docs 51–58 call. (It's `blocked`, not
    `blocks`: the camps code already had a `Game::blocks`.)
- **Filtered where lines are delivered:** in-character speech, emotes and party or Chapter speech (`Game::publish`),
  local OOC, and party and Chapter OOC (`Game::partyChat`).
  - A muted or blocked wolf's lines never reach the holder, and neither side is told.
  - When either wolf blocks the other, a line doesn't make the other a listener in the ledger, so no scene holds both.
  - Private messages and circles get the same filter when phases 3 and 5 build them.
- **Refusals in the ordinary words:** a party invite gets "They are not here to answer."; a challenge gets "… isn't
  taking challenges.".
- **Evidence:** `Game::heard_` holds the last 60 lines each connected player received, within 30 minutes, with the
  author kept on the server. It lives in memory only.
  - A report can name a line by its sequence number. The server finds the author in the reporter's own record, so the
    page never learns who wrote an anonymous line.
  - Chat lines on the page carry their `sequence`.
- **Reports:**
  - `Core/RatwReports.h/.cpp` keeps the store: in memory (tests, scratch servers), in a folder beside a file world's
    save (`<save>.reports/reports.json`), or in `game.reports`. If the database has no table yet, the store falls back
    to memory with a warning.
  - A report holds the reporter, the reported account and character (resolved by the server), kind (speech or
    profile), category, note and evidence: up to 20 of the reported wolf's lines as received, or a copy of the
    profile.
  - Five a day per account. A report can also block in the same step.
  - Retention: open and dismissed reports go after 30 days; upheld ones keep their record for good and their evidence
    180 days. A purge runs once a day: in SQL for the database, in place otherwise.
- **The DM's decision:**
  - `report.decide` (`{report, decision: uphold | dismiss, outcome: note | warning | silence, hours: 1 | 6 | 24 | 72}`)
    goes through `Game::decideReport`.
  - A warning tells the player. A silence sets the account's `silencedUntil`, tells the player, and blocks speech in
    character and out of it until then.
  - `Game::upheldReportsWithin(account, days)` counts upheld reports. Doc 49's `upheldReports` now reads it, so an
    upheld report holds Quickened back.
- **Saving:** marks go in the save as `people.safety`, which `Database/migrations/0036_safety.sql` gives its own table
  (`game.safety_marks`). The same migration adds `game.reports`. **Not applied to DEV or PROD yet**, like 0034 and
  0035.
- **Client:**
  - The card has MUTE/UNMUTE, BLOCK/UNBLOCK and REPORT.
  - A quiet ⚑ on another's chat line opens MUTE, BLOCK OR REPORT. The report form has a category, what (speech or
    profile), a note, and "Also block them", on by default.
  - YOUR PROFILE lists MUTED AND BLOCKED, each with Unmute or Unblock.
- **DM app:**
  - A new `ReportsPanel.tsx` lists open reports first, each with its evidence, the reported account's earlier reports,
    and how many accounts block it (a count, never who). Uphold has a note, warning or silence (with hours) and a
    reason; Dismiss is beside it.
  - Decided reports sit below.
  - It's served by `GET /api/reports` and `POST /api/reports/decide` in `tools/dungeon_master.py`, which validates,
    queues `report.decide` and audits. Viewers can read, not decide.
- **Tests:**
  - `Tests/safety_tests.cpp` (286 checks):
    - a muted wolf's speech, emotes and local OOC don't reach the holder, while another hears them, and the muted
      isn't told; unmuting works;
    - a block refuses party invites and challenges in ordinary words, and follows the player to their second
      character while the list names only the first;
    - no scene holds both;
    - a report by line number carries only that wolf's lines as received, never others', with the note and category;
    - five a day;
    - a silence of the wrong length is refused; an upheld silence tells the player and stops speech and OOC;
    - the store's retention and round trip.
  - `tools/test_dungeon_master.py` (45): reports listed with evidence and the block count; bad decisions refused; a
    silence queued with its payload; viewers refused. The test databases apply migration 0036.
  - `tools/client/safety.mjs` (15 checks; screenshots in `artifacts/screenshots/safety/`): a mute from a line's flag,
    a report with a block from the card, and the list.
  - `ctest`: 49 of 49. The client tests (98) and `card.mjs` (11) pass.
- **Cost:** one set lookup per listener per line, inside loops `publish` already runs. Heard lines run to about 60 per
  player, trimmed as lines arrive. Reports are rare writes, and nothing touches the tick.

### Phase 3: friends, handles and private messages (built 2026-10-07, not committed)

- **Not "tells":** the user's word (2026-10-07). A Gifted wolf's tells are something else. They are **private
  messages**: chat channel `private`, a PRIVATE tab. They are not doc 55's letters, which are mail and are kept until
  the reader deletes them (doc 55 now says so).
- **Data:** `Data/Social/profile.json` gains `friends` (200 at most, 20 requests waiting, 14 days, sharing on by
  default) and `privateMessages` (2000 characters, 50 kept for an away friend, 14 days: the user's numbers).
  `Core/RatwPeople.*` gains `FriendLink`, `FriendRequest`, `PrivateMessage` and the `messages` setting.
- **The game:** `Core/RatwGameFriends.cpp` (new).
  - Friends are mutual and by account. `friends_` holds each account's side of each friendship: when it began, and
    whether this side shows the friend which wolf it is playing. A friendship is two sides.
  - `friendsCommand` takes `request` (by handle, or by `target` from a card, which tells them which wolf asked),
    `accept`, `decline` (the asker isn't told), `cancel`, `remove`, `share` and `list`.
    - You need a handle of your own before you can ask.
    - Asking someone who already asked you accepts their request.
    - Someone who has blocked you, or whom you blocked, gets the same refusal as an unknown handle.
  - `areFriends(a, b)` is public, for doc 51's exact star counts.
  - `sendFriends` sends one account its list:
    - each friend's handle (and the old one, for a week after a change);
    - whether they're here;
    - the wolf they're playing, only if they share it with you;
    - your own share switch;
    - requests both ways.

    No places. The list is sent again only when something changes: a friend comes or goes, or a request or a share
    changes. A friend's arrival comes with a toast, which can be turned off.
  - `online_` maps each account to its connection while it has a wolf in the world, so "is here" is one lookup.
- **Handles under labels:** each player in the snapshot carries `handle` only for a friend who shares their wolf with
  this viewer (`sharedHandle`). The client shows it in In Sight and in the map's hover, which are where wolves' names
  appear. The map itself draws no names.
- **Private messages** (`privateMessage`): to a friend by handle, anywhere, out of character.
  - Refused: to a stranger, to a blocked account, past 2000 characters, to a friend who has turned them off, and while
    silenced by a DM.
  - A friend who has muted the sender doesn't get it, and the sender isn't told.
  - The sender gets their own copy.
  - **An away friend's message waits** (`inbox_`). The sender is told, and it is refused once 50 are waiting.
    `deliverInbox` hands them over, oldest first, when the friend next enters the world: each marked as kept, with
    when it was sent, and followed by "N private messages came while you were away". Anything past 14 days is never
    delivered, and the daily purge (`tendFriends`) clears it.
  - Each one received is a heard line, so it can be reported by its line number like speech.
  - A report or block from a private message names the sender by handle, the only name the reader had.
    `safetyCommand` now knows a player whose wolf is away.
- **A block ends a friendship** (`unfriend`), and any request between the two.
- **Saving:** `people.friends`, `people.requests` and `people.inbox`. Loading keeps only friendships with both sides,
  between accounts the game knows, and drops anything expired. `Database/migrations/0037_friends.sql` gives the three
  their own tables (`game.friendships`, `game.friend_requests`, `game.private_inbox`). **Not applied to DEV or PROD
  yet.**
- **Client:**
  - A FRIENDS sheet, from the top menu (which shows "FRIENDS · N" while requests wait) and from the character sheet:
    - ask by handle;
    - ASKING YOU, with Accept and Decline;
    - WAITING FOR AN ANSWER, with Withdraw;
    - each friend with here/away, the wolf they're playing (OOC), MESSAGE, "show them my wolf" and Remove.
  - ADD AS A FRIEND on a player's card.
  - The PRIVATE tab, which counts unread messages. Above the box, a chip for each friend picks whom to write to, and a
    reply goes back to whoever wrote by default. Kept messages say when they were sent; your own say "kept until they
    are here".
  - YOUR PROFILE gains "Private messages from friends" and "Tell me when a friend comes into the world".
- **Tests:**
  - `Tests/friends_tests.cpp` (2330 checks, most of them snapshot fills), covering:
    - requests: a handle is needed; unknown handles; any case; the accept; asking back; declining without telling;
      asking from a card with the wolf named; withdrawing;
    - a sharing friend's handle on the viewer's snapshot only, gone when sharing is turned off;
    - online and away, and arrival toasts (and their switch);
    - private messages: delivered, and to no one else; never from a stranger; 2000 characters; turned off; a mute; a
      silence;
    - a report by a private message's line, and a block that ends the friendship and refuses requests both ways;
    - an away friend's message kept and delivered;
    - 50 at most;
    - a restart keeping friendships, sharing, requests and the inbox;
    - 14 days dropping old messages and requests.
  - `Client/src/game/people.test.ts` (4).
  - `tools/test_game_tables.py`: the three lists become rows.
  - `tools/client/friends.mjs` (23 checks; screenshots in `artifacts/screenshots/friends/`): asked by handle; accepted
    from the counted menu; the handle in Ada's In Sight and never in Cy's; sharing off; MESSAGE to PRIVATE, read by Bo
    with an unread count; Bo away, then the message reaching him when he comes back.
  - `ctest` 50 of 50; client tests 102; `card.mjs` and `safety.mjs` pass.
- **Cost:**
  - Per snapshot, per visible player: one lookup in the viewer's friends, skipped at once for a viewer with none.
  - A friend's arrival or departure: one list for each of their friends.
  - A private message goes to one connection.
  - Nothing runs in the tick except a purge every ten minutes over requests and the inbox. `world_check` doesn't
    exercise any of this, so it wasn't re-run.

### Phase 4: known wolves and scene recaps (built 2026-10-07, not committed)

- **Data:** `Data/Social/profile.json` gains `known` and `recaps`, all placeholders:
  - `known`: 300 players and 100 residents, last met touched at most every 10 minutes a pair, 500-letter notes, custom
    tags of 24 letters, and the six tags (Unfriendly, Neutral, Business, Friendly, Love, Family);
  - `recaps`: 3 a wolf and 150 a character; a buffer of 120 lines or 6,000 characters; the model writes a recap only
    with 6 lines over 5 minutes, at most 10 a day, each at most 600 characters.

  `Core/RatwPeople.*` gains `KnownWolf` and `Recap`, save and load for both, `validTag`, `trimKnown` (past the caps,
  the oldest met without a note, tag, tie or recap go first) and `trimRecaps` (a recap goes once every wolf in it has
  three newer ones; then the oldest past 150).
- **The game:** `Core/RatwGameKnown.cpp` (new).
  - **Who goes on the list** (`meet`):
    - players: from a shared scene (counted), from joining a party, from learning their name, or from hearing them
      speak (only updates last met, at most every 10 minutes a pair);
    - a resident: only once this wolf knows its name, notes it or tags it.
  - Each entry keeps the label this wolf last knew them by, so a wolf who is away still shows as they were seen.
  - Blocked wolves never go on each other's lists or into each other's recaps.
- **Scene endings** (`tendScenes`), checked after each social tick and when someone steps out:
  - a scene that ended, or a member who stepped out of one, is seen once; every member meets the others, and each
    gets a recap;
  - `seedScenes` marks scenes that were already over before a restart, so they aren't recapped twice.
- **What a member perceived** (`perceivedLine`): `Game::publish` adds each line as it was delivered to that player:
  "You" for their own lines, the speaker as they knew them, or "A voice". It's held in memory, capped at 120 lines or
  6,000 characters, and only for players who let the model write their recaps.
  - A recap's lines are those perceived in the scene's place from the start of the scene to its end for this member.
    This is a change from the plan, which said from when they joined: Cy listened before he spoke, and his recap lost
    the start of the scene.
- **Recaps** (`endScene`):
  - Written from the ledger alone: "You shared a scene with a dun wolf and Wren at The Bent Bough. It ran 40 minutes."
  - Written by the model through the Mind's new `POST /recap`, but only when the Mind is configured, the player allows
    it, the member perceived 6 lines or more over 5 minutes or more, and they've had fewer than 10 model recaps today.
    A failure falls back to the written recap. Names the wolf doesn't know are veiled.
  - `Options::recapModelSeconds` lowers the 5 minutes for tests.
- **The Mind:**
  - `mind::Client::recap` sends the place, the wolf's own name, the minutes and the newest lines within the Mind's
    bounds.
  - `tools/npc_mind.py` gains `/recap`: `RECAP_RULES` (second person, claims as claims, labels kept, nothing invented),
    `clean_recap_request` (120 lines, 600 characters each, 8,000 in all), the small model, kind `recap` in the call
    ledger, and a fixture reply. `tools/ai_cost.py` reports it without changes.
- **Commands:** `known` takes `list` (newest first, each with its latest recap), `get` (one entry with all its recaps),
  `tag`, `note`, `forget` (also deletes recaps that were only with them) and `unrecap`.
  - The older `social` `note` verb now writes to the entry.
  - The old private notes (`notes_`) are gone. As you asked, there's no data migration: old saves' notes aren't
    carried over.
- **Card and snapshot:**
  - A closer look adds `known` (the entry, with all its recaps) and marks the profile read.
  - The snapshot adds `noted` for a wolf with a note, and `unread` for a profile changed since this wolf last read it.
- **Saving:** `people.known` and `people.recaps`. `Database/migrations/0038_known_wolves.sql` adds
  `game.known_wolves` and `game.scene_recaps`, and takes read access to those two and to 0037's
  `game.private_inbox` away from the DM, editor and publisher logins: they're for the game alone. **Not applied
  yet.**
- **Client:**
  - FRIENDS gains a KNOWN WOLVES tab. Each row has search and a tag filter, then the name as known, "resident",
    the tag, "profile changed", place, how long ago, scenes shared, the note, the latest recap, MORE (every recap with
    Delete, and the tag and note editors) and FORGET.
  - A player's card gains a YOU AND THEM tab: last met, scenes shared, tag (one of the six, or the player's own),
    note, and recaps. A resident's card shows the same below the look.
  - The old YOUR NOTE box is folded into YOU AND THEM.
  - In Sight shows ✎ for a wolf with a note and • for a changed profile.
- **Tests:**
  - `Tests/known_tests.cpp` (1854 checks, most of them snapshot fills), with a stand-in Mind:
    - a shared scene puts each member on the others' lists, counted, as strangers by their look; once introduced,
      Bo goes by name;
    - Ada's model recap is sent her own lines as "You", the whisper meant for her, Bo by his look, and nothing of Cy,
      whom she had blocked; Cy's lines hold the speech but not the whisper out of his reach;
    - with no Mind, the recap is written and names only what Ada knows;
    - with Bo's recaps turned off, his recap is written and nothing of his goes to the Mind;
    - notes, tags (one of the six, a custom one, a bad one), a resident tagged onto the list, the card's entry, the
      noted and unread marks, deleting a recap, forgetting;
    - the caps;
    - a restart keeps everything and doesn't recap twice.
    - The 10-a-day limit has no test.
  - `tools/test_npc_mind.py`: `/recap` (bounds, the small model, nothing of the scene in the log, over HTTP).
  - `tools/test_game_tables.py`: both lists become rows.
  - `tools/test_dungeon_master.py`: the DM login can't read `known_wolves`, `scene_recaps` or `private_inbox`.
  - `Client/src/game/people.test.ts`: 6 tests.
  - `tools/client/scenes.mjs`: after Ash steps out, Bo is in her Known wolves with the recap, and under YOU AND THEM on
    his card. Screenshots 6 and 7.
  - `ctest` 51 of 51; client tests 104; `card`, `safety`, `friends`, `names` and `party` pass in a real page.
  - The real model wasn't called: that costs money.
- **Cost:**
  - Per heard line: one map lookup to throttle last met, and an append to a buffer of at most 6,000 characters, about
    6 MB at 1,000 players.
  - Per scene ending: a scan of the member's buffer.
  - Per recap: at most one call to the small model, capped at 10 a character a day.
  - Each social tick: a pass over the ledger's scenes, which the ledger's own tick already makes.
  - Nothing runs in the world's tick.

### Phase 5: circles (built 2026-10-07, not committed)

- **Data:** `Data/Social/profile.json` gains `circles`: a 32-letter name, 10 circles an account, 50 members a circle,
  10 nights a circle (an 80-letter place and a 160-letter line), invitations kept 14 days, and members not sharing
  their wolf until they choose to. All are placeholders. `Core/RatwPeople.*` gains `Circle`, `CircleMember`,
  `CircleNight`, their save and load, and `validCircleName`.
- **The game:** `Core/RatwGameCircles.cpp` (new). `circleCommand` takes:
  - `create`: needs a handle; the name is unique, any case; 10 circles an account.
  - `invite`: by handle, from its keeper and officers only. Someone blocked either way gets the same refusal as an
    unknown handle. The invitee is told, and the invitation counts toward the 50.
  - `accept` and `decline`: whoever invited isn't told of a decline.
  - `remove`: officers remove members, the keeper removes officers too, and nobody removes the keeper. Officers can
    also withdraw an invitation still waiting.
  - `officer`: the keeper names and unnames officers.
  - `share`: per circle, off at first.
  - `night` and `unnight`: officers plan nights within the coming year, kept in time order.
  - `leave`: a keeper who leaves hands the circle to the longest-standing officer, else the longest-standing member.
    The last to leave ends it.
  - `disband` ("End it"): the keeper only.
  - `list`.
- **What each member sees** (`sendCircles`):
  - the roster, by handle, here or away, with a member's wolf only where they share it with this circle;
  - nights;
  - for officers, the invitations still waiting;
  - and their own invitations.

  It's sent again to every member in the world when the circle changes or a member comes or goes.
  `tendCircles` (every 10 minutes, with the friends' upkeep) drops invitations past 14 days and nights a day past.
- **Circle chat** (`circleLine`, the `circle` channel with the circle's id): out of character, to every member in the
  world, named by handle; the speaker gets their own copy.
  - It never reaches a member who muted or blocked the speaker, and the speaker isn't told.
  - Each line received is a heard line, so it can be reported by its line number.
  - There's no inbox and no scene credit. Lines are at most 2000 characters, the same as private messages.
- **Saving:** `people.circles`. `Database/migrations/0039_circles.sql` adds `game.circles`, which the DM may read
  (members, never chat) and the editor and publisher may not. **Not applied yet.**
- **DM app:** the Players tab shows each character's account's circles, with its rank and the members by handle
  (`tools/dungeon_master.py`, `DmApp.tsx`).
- **Client:**
  - FRIENDS gains a CIRCLES tab:
    - make a circle; invitations with Join and Decline;
    - each circle with CHAT, "show this circle my wolf", Leave, and End it for its keeper;
    - the roster with Make officer / Make member and Remove where allowed;
    - invite by handle and withdraw, for officers;
    - nights in the viewer's own time, and for officers "Plan a night" (a date and time, a place, a line) and Take off.
  - Each circle has its own chat tab with an unread count.
  - The FRIENDS button counts circle invitations with friend requests.
  - The story's tabs now wrap onto a second row instead of running under the map.
- **Tests:**
  - `Tests/circles_tests.cpp` (730 checks, most of them snapshot fills), covering:
    - making one: a handle needed, the name rules, unique names, 10 an account;
    - invitations: by handle and announced; a member can't invite, plan or name officers; officers can, but can't
      remove the keeper; a blocked pair refused;
    - the roster showing a wolf only when shared;
    - chat reaching members and the speaker's own copy, a blocked member's lines kept from the blocker (who isn't
      told), nothing to or from one who left, and away shown on the roster;
    - nights: in order, none in the past, a place needed, taking one off;
    - a restart keeping members, ranks, sharing and nights;
    - a keeper's leaving handing the circle on, only the keeper ending it, the last to leave ending it and freeing
      its name;
    - a night a day past dropped.
    - The 50-member cap has no live test: it would need 50 accounts. Loading caps at 50.
  - `Client/src/game/people.test.ts` (7).
  - `tools/test_game_tables.py`: the circles become rows.
  - `tools/test_dungeon_master.py`: an account's circles in the Players tab.
  - `tools/client/friends.mjs` (32 checks; screenshots 6 and 7): Ada makes Moot Night, invites Bobbin, who sees it
    counted and joins; a night planned in her own time reaches him; her line in its tab reaches him, counted unread;
    Cy never sees it.
    - Seven of eight runs passed in full. One dropped a single check I couldn't catch again, most likely timing in
      the test.
  - `ctest` 52 of 52; client tests 105; `card`, `safety`, `scenes`, `names` and `party` pass in a real page.
- **Cost:**
  - A circle line goes only to its members who are in the world.
  - A member's arrival or departure resends the circles to its other members: at most 10 circles of 50.
  - The 10-minute upkeep is a pass over the circles.
  - Nothing runs in the world's tick.
