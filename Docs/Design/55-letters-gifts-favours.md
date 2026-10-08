# 55. Letters, gifts and favours

Drafted 2026-10-06 as an actionable plan for doc 48 (§3.8, and §3.9 except vouching). Open questions answered
2026-10-08 (see "Open questions"); building began the same day. Read doc 48
(§3.8, §3.9, Part 11 and the Decisions) and docs 35 (the maker's mark, masking oil), 38 (rest and lasting injuries),
40 (noticing and scent), 41 (the nose), 43 (Wash Out), 26 (residents, contracts, the Mind), 28 (AI cost), 15 (money)
and 32 (names) first.

Marks: **(agreed)** is the user's decision, with a pointer to doc 48; *(placeholder)* is one constant to change.

## The ask

> "Letters, gifts; we should do this. Specifically between players, but also from NPC's to players that NPC's like or
> want to thank." (doc 48 §3.8, agreed)

> "How else can we add favor and gift giving? Maybe something like having a free daily buff that you can apply to
> yourself or to another wolf? What would this even be?" (doc 48 §3.9)

The answers the user agreed on 2026-10-06: letters between players and from residents; every crafted item and gift
carries its maker's or giver's scent (decision 11); grooming as the daily favour (decision 31). Shared meals, lending,
witnessing and sponsoring were proposed alongside and come last here. Vouching is doc 52's.

## Where we stand (read from the code 2026-10-06)

**Letters and couriers**
- **Courier contracts exist, for residents only** (`Contract` kind `"courier"`, `Core/RatwRoads.h:57`). A resident who
  cares for someone in another town sometimes pays to have a letter carried (`roadsDaily`, `Core/RatwRoads.cpp`
  1146–1169). The letter has no text and no enclosure. It is done when the taker stands within 3 tiles of the
  recipient (`tendRoads`, 391–407), and the recipient must be a resident. Untaken for a week, it rides a caravan
  (`Caravan::letters`, `sendCaravan`).
- **The catalog already has the goods:** `letter` (with `"text": true`, which the loader ignores), `paper`, `ink`,
  `quill`, `sealing_wax`, the `scribe_desk` station and the `scriptorium` business (`Data/Items/`).
- **No player letters, no stored player text beyond small lists:** private notes (500 letters, `Game::notes_`), lease
  notices (200 letters, `RatwGameEstates.cpp:224`) and Chapter charters, all in the checkpoint row.
- **No inbox.** A notice to a player who is offline is dropped (`RatwGame.cpp:2025–2027`).
- **No travel-time helper.** `World::routeBetween(from, to)` (`RatwRoads.cpp:66`) gives the cells along the roads, and
  the game keeps every road between towns (`roadRoutes_`). The cell count is the only distance proxy in use.

**Giving**
- **No give command.** The wire type `"gift"` is taken: it is the Dev Console's grant of a magic Gift
  (`RatwGame.cpp:3860–3872`).
- **The bond rule for a gift exists but is dead:** `World::bondsFromEvent` (`RatwWorld.cpp:2436`) warms the receiver
  by {5, 3, 2, 0, 1} on a `gift` or `help` event, and nothing ever records a `gift` event.
- **`Society::shift` doesn't check the 64-kinds limit** (`RatwCareers.cpp:138`), but `Society::restore` refuses a save
  with more (`RatwSociety.cpp:1066`). An unchecked give could make a checkpoint that won't load.
- **Gift-like kinds must be listed as unearned** in `Society::record` (`RatwSociety.cpp:344–346`) and `unearned()`
  (`RatwOrchestrate.cpp:58–67`), or a gift counts as a resident's profit.

**Scent**
- **Items are stacks** keyed by id (`EconomyAccount`, `RatwSociety.h:20`). Only a masterwork carries a maker, in its id
  (`sword~masterwork@sorrel`; `items::withMaker`, `RatwItems.cpp:442`; given in `Society::craft` and
  `World::huntKill`). `Game::itemLabel` (`RatwGame.cpp:4621`) names the maker to those who know their name.
  `World::marksSmelt` and `tendMarks` (`Core/RatwMarks.cpp`) read masterwork marks by nose.
- **No wolf has a scent strength.** Doc 40's `(4 + 14 × strength)` (`World::noticeSenses`, `RatwBattle.cpp:1629`) is
  the *wind's* strength. Nothing raises a wolf's scent (no blood, smoke or sweat), and nothing lowers it except masking
  oil (`World::maskScent`, `scentMasked`; zero scent for 4 game hours).
- **`World::scentCues` ignores masking oil** (`RatwWorld.cpp:4236`): a masked wolf still shows in the scent sectors.
- **Wash Out is fight-only:** `washed` for 4 of the target's turns, scent 0 (`RatwMagic.cpp:1470`, `magicSenses`).
- **Scent never identifies a living wolf** (doc 02's rule; `SensoryResult` says so, `RatwWorld.h:473`). Doc 35's marks
  already identify a maker on an *object*, using name knowledge as the stand-in (`Game::knowsName`).
- **No record of known scents.** Names known are `names::Acquaintances` (`Known{names, how, day}`, `RatwNames.h`).

**Favours**
- **No grooming**, only the `grooming_brush` and `tick_comb` items.
- **Timed states out of fights** are ad hoc `*Until` fields on `Entity`; `scentMaskedUntil` is the saved one to copy
  (`RatwWorld.h:267`, `self.scentMasked`, the Status window's CONDITION list in `dialogs.ts`).
- **Rest** rates are literals in `World::restPlayers` (`RatwBattle.cpp:3243`) and `returnFromAway` (3306).
- **Lasting-injury rolls** (`World::giveLasting`, `injureOnDown`, `Core/RatwInjury.cpp:493, 539`) have no modifier
  that lowers them; weariness only raises them.
- **No first impressions:** bonds start at zero (`Bonds::change`), and `RatwAppearance.h` says appearance never
  changes anything mechanical.
- **Eating** (`World::eat`, `RatwWorld.cpp:4169`) gives 10 stamina and nothing that lasts. Only `meal` is eaten.
- **No lending.** `Bond::owed` exists but no game code sets a debt (`Bonds::addOwed` is called only by tests).

**Wire and the Mind**
- Owner-only state in `self` is resent with every snapshot, five a second; only eight parts are sent on change
  (`sections::strip`). Big owner state goes as events on request, as faction missions do (`{"type":"missions"}`).
- The Mind writes short texts off the game thread already: `polish`, `summarize`, `exchange` (`Core/RatwMind.cpp`,
  `Client::post`), on the light model (gpt-5.4-nano), with the cost ledger (`tools/ai_cost.py`).

## Scope

This plan builds:
- the **document store**, where in-world writing lives (doc 54's notices use it too);
- **letters between players**: writing, couriers with travel time, delivery, scent, anonymity, enclosures;
- **giving**, with the giver's scent, and the **maker's scent** on crafted goods;
- **letters from residents**: thanks with gifts, requests that become contracts, and invitations to **occasions**;
- **grooming**, **shared meals**, **lending**, **witnessing** and **sponsoring**.

It leaves to other plans:
- **Vouching** (doc 52). **Block, report and known wolves** (doc 50): this plan calls `blocked(a, b)` and puts letters
  and grooming on the known-wolves card. **Stars and scenes** (doc 51).
- **Unfinished business, "while you were away" and the chronicle** (doc 56): they read this plan's letters and events.
- **Homes:** players have none until doc 54's renting; letters go to lodgings once doc 54 builds them.
- **Residents' troubles** (doc 57), which will make most residents' requests. **Player crafting** (doc 35, doc 53).

## Design

### 1. The document store

In-world writing (letters, notices, pacts) is kept in one store, apart from every ledger (doc 08). Ledgers and
`game.events` carry only the document's id.

- **Where:** a new list in the save, `documents`, written as its own table `game.documents` (migration
  `00NN_documents.sql`, the next free number; a `game.sections` row; deltas like `game.bonds`). Generated columns for
  the DM and tools: `kind`, `author`, `recipient`, `board`, `expires`.
- **A document:** `id`, `kind` (`letter`, `resident_letter`, `notice`, `chapter_notice`, `pact`), `author` (always
  kept, for the DM and reports), `scent` (the author's id, or `""` when masked), `to`, `board`, `text`, `sign` (the
  name signed, or `""`), `written`, `deliverAt`, `postTown`, `state` (`travelling`, `waiting`, `delivered`, `read`,
  `returned`), `kept`, `expires`, `enclosure` (`{account, item, quantity, coins}`), and for residents' letters
  `template` and `facts`.
- **Valuables:** writing a letter with an enclosure goes through the journal (doc 31) with its text, so a crash keeps
  both or neither. On load, an escrow account with no document goes back to its writer.
- **Retention:** notices go at expiry (doc 54). **Letters are mail, held until the reader deletes them** (the user,
  2026-10-07: "Mail should be held indefinitely until the player deletes it"): read or unread, nothing is thrown out.
  Only a flood is stopped: past 100 unread *(placeholder)* the courier refuses ("Their post is full"). They are not
  doc 50's private messages, which are out of character and go after 14 days.
- **Privacy:** the DM app shows a letter's metadata; its text only inside a report (doc 50) or through an audited
  admin read.

### 2. Letters between players (agreed, doc 48 §3.8)

**Writing.** At an inn or tavern, at a writing desk (`scribe_desk`) in a scriptorium, or in a wolf's own lodgings or
Chapter place (doc 54, doc 32). The page shows WRITE A LETTER there.
- **To:** a wolf the writer knows **by name**, typed as the name they know (resolved through their acquaintances; two
  wolves known by one name are told apart by description). Never an account handle.
- **Text:** 1 to 800 letters *(placeholder)*, plain text, as private notes are.
- **Signed** with one of the writer's own names (true name or alias), or unsigned. A signature introduces the writer
  to the reader by that name (`Known::how` = `"letter"`), with the usual receipt: "You signed as Kestrel."
- **Sealed**, always. The seal breaks when it is first read.
- **Cost:** the courier's fee, 1p within a town and 2p to another town plus 1p per 10 road cells *(placeholders)*,
  paid to the writing town's treasury (`Society::treasuryOf`), which pays its messengers. The paper and wax are the
  town's own (doc 42's hall of records already buys them in the cities). No good is made or handed over, so a letter
  is never a sellable item.
- **Limits:** 10 letters a game day *(placeholder)*. A wolf who has blocked the writer (doc 50) is unreachable: "The
  courier can't find them", the same words as for a wolf who no longer exists, so a block isn't revealed.

**Scent** (agreed). A letter carries its writer's scent unless the writer is masked (`World::scentMasked`) when
writing. The reader's nose reads it:

| The reader | Reads |
|---|---|
| knows the writer's name | "It smells of Kestrel." |
| knows the writer only by sight: a bond with familiarity 20 or more *(placeholder)*, or a known-wolves entry (doc 50) | "It smells of the grey wolf with a torn ear." |
| neither | "A wolf's scent you don't know." |
| the writer was masked | "It carries no scent at all. Someone took care." |

The scent fades after 14 game days ("Its scent has faded.") *(placeholder)*. A nose too damaged to smell
(`noseAcuity` below 0.2) reads nothing. This identifies only an object in paw, as doc 35's marks do; living wolves stay
anonymous to scent, as doc 02 requires.

**Anonymous letters:** masked and unsigned. The server still knows the author, for the DM and reports.

**The courier.** Couriers are not walked: delivery is a time.
- **Travel time:** 1 game hour (10 real minutes) within a town; between towns `1 + cells ÷ 3` game hours, at most 12
  *(placeholders)*. `World::courierHours(fromTown, toTown)` is worked out once from `roadRoutes_` when the towns are set
  up. A wolf outside any town counts from the nearest.
- **Where it goes: the recipient's post town**, the first of: their lodgings' town (doc 54); the town they last
  logged out or slept a full rest in (`Entity::postTown`, set then); the town they are in when it arrives; the
  capital.
- **Arrival:** if the recipient is online in their post town, a messenger finds them ("A messenger finds you with a
  letter.") and it goes straight into their letter case. Otherwise it waits at that town's inns, and the case says
  "1 letter waiting at Accord Crossing". Walking into any inn or tavern of that town collects it. From anywhere, SEND
  IT ON (1p, the courier's time again) brings it to the town they are in.
- **A letter carried by a friend** is Phase 7's favour: a courier contract given to a player instead of the town.

**Reading.** The letter case (a LETTERS tab in the character dialog): unread first, then read and kept. Each letter
shows its signature (or "unsigned"), the scent line, when it was written and where it came from, the text, and
REPLY, KEEP, BURN.
- **Reply** goes to the writer if the reader knows their name. An anonymous letter can be answered once **by the same
  courier**: the server delivers it to the writer, and the replier never learns who it was.

### 3. Giving, and the giver's scent

**A give command** (named `give`, so as not to clash with the magic `gift`). Within 2 tiles, both out of a fight:
- an item (not worn, not lent), a quantity, and/or coins;
- to a **player**, who accepts or declines within 30 s (so nobody is loaded down against their will); to a
  **resident**, who takes it unless hostile.
- The move uses `Society::transfer`'s checks (the 64 kinds) with kind `gift`. `recordEvent({"gift", ...})` then warms
  the receiver by the existing rule. The bond counts once a game day per pair *(placeholder)*, so pennies can't buy
  affection.
- `gift` and `letter enclosure` join the unearned lists (`Society::record`, `unearned()`), agreed with the economy
  session that owns `RatwOrchestrate.cpp`.

**Enclosures** (agreed: "a small item or coin, conserved"). A letter may carry one kind of item weighing at most 1 lb
in all *(placeholder: a ring, a charm, a cake, not a sword)* and up to 50p *(placeholder)*.
- Held in a facility account `letter:<id>` (a new prefix in `facilityAccount`, `RatwCareers.cpp:56`) from writing to
  taking. TAKE moves it to the reader if their purse has room; otherwise it stays in the letter.
- A letter with an enclosure unread for 56 game days *(placeholder)*, or undeliverable, goes back to its writer with
  the enclosure ("returned to sender"). Letters without one wait.

**Scent records.** Players' goods carry who made them and who gave them, beside the stack, not in the id (marking
every id would split every shop's stock):
- `Entity::scents`: up to 60 entries *(placeholder)* of `{item, maker, giver, count, madeDay, givenDay}`, saved with
  the character. Only players have them; residents' and tills' stocks stay as they are.
- A record moves with the goods between players (give, enclosure, a doc 54 stall sale) and is dropped when the goods
  go to a resident, a till or a Chapter's stores. Units used up or sold come off the oldest record first.
- **The giver's scent** lasts 7 game days ("It still smells of her.") *(placeholder)*; a masked giver leaves none.
- Shown on each belonging ("Honey cake · it smells of Kestrel") and told on receiving ("It smells of Kestrel.").

### 4. The maker's scent on crafted goods (agreed, doc 48 §3.8, decision 11)

- **When a player buys** a good from a shop that makes it (its business makes it in `crafts.json` or `recipes.json`),
  the record's maker is the shop's keeper: "Leather cap · it smells of the tanner at La Concia" (by name once known).
  A seller who didn't make it adds no maker.
- **Hunted goods** carry the hunter (the kill's hunter, as masterworks already do). Foraged goods carry nobody.
- **Player-made goods** (when doc 35 and doc 53 let players craft) carry the player.
- **Masterworks** keep their mark in the id, as now; nothing changes for them.
- The maker's scent lasts 28 game days from when the player got the good *(placeholder)*.

### 5. Letters from residents (agreed, doc 48 §3.8)

The rules decide who writes, why, what it says in facts and what goes with it. Words come from templates, and the
light model may only polish them.

| Kind | When *(placeholders)* | What comes with it |
|---|---|---|
| **Thanks** | The day after a deed for the resident or their household (a contract done for them, tending them when Downed, a gift worth 5p or more, and doc 56's and doc 57's deeds as they arrive), from a resident with affinity 40+ and trust 20+ toward the player. | One time in three, a gift from the resident's **own purse** (never a till's stock or the last meal), worth at most 6p and a tenth of what they hold; else nothing. |
| **Request** | A resident about to post a courier contract (today's 2.5% a day) who trusts a player (30+) offers it to them first. Later, doc 57's troubles. | The contract, reserved for that player for 2 game days (`Contract::offeredTo`, `offeredUntil`), then open to all. TAKE IT ON from the letter. |
| **Invitation** | Weddings, a child's naming, funerals, a festival meal (6, below). | A time and a place. |

- **Templates:** `Data/Voice/letters.json` (`ratw-letters`): by kind, the six tones taken from the personality (as
  `router.json` does), and how well the resident knows the player. Blanks: `{to} {deed} {gift} {item} {count}
  {reward} {place} {when} {couple} {child} {deceased} {festival} {town} {from}`. Checked at load like the scenes.
  `tools/letter_library.py` can draft more with the light model, checked and reviewed like `ambient_library.py`.
- **The signature:** the resident's name if they would give it (`Game::willName`, and familiarity 50+ or a name owed);
  otherwise their public role ("the baker at The Amber Loaf"). A name given in a letter is recorded with `how`
  `"letter"`.
- **Polish:** with `Options::letterModelCallsPerHour` above 0 (default 0, like the ambient director), the Mind's new
  `/letter` rewrites the filled template in the resident's voice on the light model. Every name, item, number, place
  and time must survive, or the template stands.
- **Limits:** a player gets at most 3 residents' letters a game week and one from any resident a game week
  *(placeholders)*. Candidates come from events as they are recorded (no scans) and are sent in one pass a game hour.
- **The Mind's briefing** of that resident gains a line from the store: "You wrote to them on Hearthday to thank them
  for carrying your letter, and sent a honey cake." (the last 28 days, at most two lines, in `dialogueContext`'s
  activity).

### 6. Occasions, invitations and shared meals

**Occasions** (`Core/RatwOccasions.cpp`): a gathering residents host, with a place, an hour and hosts.
- **A wedding** (doc 26's `marriage` event): the next Restday after the service, at the church, 11:00 to 12:00. **A
  naming** (a birth): the same. **A funeral** (a death): the next morning at 10:00, at the church. **A festival meal**:
  a household head with affinity 60+ invites a player to eat at their table at 18:00 on a festival day *(placeholders)*.
- **Hosts** (the couple and family, the parents, the mourners, the household) are sent there by the errand the
  couriers and guards already use, which overrides the day's plan. `RatwResidents.cpp` isn't touched.
- **Invitations:** each host household invites up to 3 players it likes most (affinity 50+), by letter, a game day
  ahead where it can. The letter has ANSWER: COMING / CAN'T COME.
- **Witnesses:** invited players present at the hour are named the occasion's witnesses (IDs only, a `witnessed`
  event): the hosts' affinity +5 *(placeholder)*, a chronicle line (doc 56), and the hosts' briefing remembers it ("They
  stood witness at your wedding.").

**Shared meals** (doc 48 §3.9). Meals have no lasting effect today, so this plan gives them one first:
- **Fed:** eating gives `Entity::fedUntil` 2 game hours: stamina comes back 10% faster and rest heals 5% faster
  *(placeholders)*.
- **Shared:** another wolf within 2 tiles who ate in the last 10 game minutes (player or resident) makes it a shared
  meal: Fed lasts 4 game hours for both, and each pair gains familiarity +1 and affinity +1, once a game day.
- A festival meal at a resident's table is shared, from the household's larder as a gift. Doc 54's festival feast
  for players counts too.

### 7. Grooming (agreed, doc 48 §3.9, decision 31)

- **Asking:** GROOM on a wolf's card or in Look, within 1.5 tiles, both out of a fight and still. The other accepts
  within 30 s (grooming is intimate). A blocked wolf (doc 50) can't ask. A resident accepts only from a player it likes
  (affinity 40+, trust 30+) *(placeholders)*; residents don't groom players in this version.
- **The grooming:** 15 s *(placeholder)*, both staying within 1.5 tiles. Moving apart or a fight ends it with nothing.
- **Once a game day** for the groomer (agreed placeholder). Being groomed again only renews it.
- **Well-groomed lasts 24 game hours, or until the wolf's next full rest**, whichever comes first (the user,
  2026-10-08, replacing "until midnight").
- **Self-grooming:** GROOM YOURSELF, once a game day; half of every effect for 2 game hours (agreed placeholder).

| Effect | By another | Self | Where |
|---|---|---|---|
| **First impressions:** residents' liking and trust grow faster toward the wolf while their familiarity is under 25 | ×1.25 | ×1.125 | `bondsFromEvent` (trade, talk) and `Game::heed` (the Mind's nudges, still within 3 a reply and 6 an hour) |
| **Healing:** rest hours | ×1.1 | ×1.05 | `restPlayers`, `returnFromAway` (only the part of the time away before it ends) |
| **Fewer lasting injuries** (agreed): every lasting-injury roll | −10 points | −5 | `giveLasting`, after weariness, floored at 0 |
| **Licked clean:** each severe acute injury groomed carries `Injury::cleaned` until its next downing or until it eases below severe | its setting roll −10, even after Well-groomed ends; not added to the line above | — | `injureOnDown` |
| **Less scent** (agreed), for 2 game hours | smelt from 0.6 as far | 0.8 | below |

All numbers in the table are placeholders but the agreed 10 points.

**Less scent needs a scent on the wolf.** Doc 48 assumed doc 40's formula had one; it is the wind's. So this plan adds
`World::scentScale(const Entity&)`: 0 while masked, 0.6 groomed by another, 0.8 self-groomed, else 1. It multiplies the
scent range in `noticeSenses` and the clarity in `scentClarity` and `scentCues`; `scentCues` learns masking oil on the
way. Grooming stays weaker than both Wash Out (no scent at all, in a fight) and masking oil (no scent for 4 game hours):
it never goes below 0.6, and masking wins when both apply. Doc 48's "strips picked-up smells" waits for such smells to
exist.

- **Bond for both** (agreed): affinity +3, trust +2, familiarity +2 each way, once a pair a game day *(placeholders)*.
- **A scene action** (agreed): the groomer's line is posted through the chat path as an `/action` and counts at half
  weight (doc 32 §1.1). It is the player's own words if they give some (200 letters), else a stock line in their
  name ("Kestrel grooms the grey wolf's ruff, slow and careful.").
- **Shown:** the Status window's CONDITION list ("Well-groomed, by Kestrel · until midnight", with what it does);
  `self.groomed`; the groomed wolf's Look line ("freshly groomed") for others.

### 8. Lending, letters carried by friends, pacts and sponsors

- **Lending gear** (`{"type":"lend"}`): an item, for 1 to 7 game days *(placeholder)*, to a player who accepts.
  - The goods move (`transfer`, kind `lent`) with a `Loan{id, lender, borrower, item, quantity, due}` (saved).
  - The borrower may wear and use them, never sell, give, enclose or store them (`lentCount` in those checks).
  - RETURN within 2 tiles, or at the due day a courier carries them back (1p from the borrower). If they are gone (worn
    out, eaten), the borrower owes the lender the catalog price (`Bonds::addOwed`, its first game use) and the lender's
    trust falls 10 *(placeholder)*.
  - Wear isn't carried across in this version (wear is kept by kind on the wearer).
- **Lending a room** is doc 54's guest list on a lease.
- **Carrying a letter for a friend:** a writer may give the delivery to a player who knows the recipient: a courier
  contract (`Contract` kind `courier`, now allowed a player recipient) reserved to that player, who is paid the fee
  instead of the town and delivers by standing near the recipient (`tendRoads`).
- **Pacts:** a document of kind `pact` (400 letters): terms one wolf writes, naming another wolf known by name. Both
  seal it; up to 3 witnesses seal it. Each holds a copy in the letter case. Nothing enforces it: rules never judge
  prose. A `pact sealed` event, with IDs only, feeds the chronicle.
- **Sponsors:** when a member invites a wolf into a Chapter (doc 32), they are recorded as its sponsor
  (`chapter::Member::sponsor`), shown on the roster, and told when the initiate is promoted or removed. Standing and
  renown don't change.

### Wire

| Direction | Message |
|---|---|
| Commands | `letter` with `verb` `write` (`to`, `text`, `sign`, `enclose`), `read`, `take`, `keep`, `burn`, `sendOn`, `reply`, `answer` (`yes`), `carry` (`by`); `letters` (asks for the case); `give` (`target`, `item`, `quantity`, `coins`), `giveAnswer`; `groom` (`target` or `"self"`, `words`), `groomAnswer`; `lend`, `lendAnswer`, `return`; `pact` (`write`, `seal`, `witness`) |
| Events | `letters` (the case, on request or change), `letterArrived`, `giveOffer`, `groomOffer`, `lendOffer` |
| `self` (small, resent each snapshot) | `letters: {unread, waiting}`, `groomed: {until, by, half}`, `fedUntil`, `loans` (a count) |
| `game.events` kinds | `letter sent`, `letter delivered`, `letter returned`, `resident letter`, `gift`, `groomed`, `lent`, `loan returned`, `loan unreturned`, `pact sealed`, `witnessed` (detail: a document or loan id, never text) |

### Client

- `Client/src/ui/hud/letters.ts`: the letter case and the writing sheet (to, text with a count, sign, enclosure).
- GIVE, GROOM and LEND on a wolf's card and in Look; GROOM YOURSELF in the actions row; offers as prompts.
- Scent lines on belongings (`dialogs.ts`'s inventory); Well-groomed and Fed in CONDITION.
- `Client/src/game/state.ts`: `sendLetter`, `sendGive`, `sendGroom`; the events into state.

### The Mind

The server decides who writes, what happens and what is given; the model only words residents' letters (§5).
- New briefing lines in `Game::dialogueContext`'s activity, at most two, from the store and the event log: a letter
  the resident sent ("You wrote to thank them…"), a gift the player gave them in the last 7 days ("They gave you a
  honey cake on Stoneday."), an occasion they witnessed ("They stood witness at your wedding.").
- **Players' letters never go into a prompt.** They are players' prose; the Mind sees only facts.
- Well-groomed reaches residents only through the bond rule (§7); the model isn't told.

### The DM app

- **Players tab:** a character's letters (who, when, from where, enclosure; no text), loans, Well-groomed, Fed.
- **Money tab:** money and goods in letters' escrow, beside caravans and contracts.
- **Reports** (doc 50) carry a letter's text when a player reports it.
- Actions `letter.return` (send a stuck letter back) and `loan.end`, added to `ACTIONS` (`tools/dungeon_master.py`)
  and handled in `Game::applyDmActions`.

## Phases

### Phase 1: the document store and letters between players

- **Goal:** players write sealed, scented letters to wolves they know, and couriers deliver them in good time.
- **Changes:**
  - `Database/migrations/00NN_documents.sql`: `game.documents` and its `game.sections` row.
  - `Core/RatwDocuments.{h,cpp}` (new): the `Document` struct, the store, limits, save and load, the delivery queue.
  - `Core/RatwGameLetters.cpp` (new): `Game::letterCommand`, `lettersView`, delivery and collecting at inns,
    `Entity::postTown` set in `Game::leaveCharacter` and at a full rest (`World::fullRest`).
  - The list of inns' common rooms (a merchant post whose label matches the `inn` business), built in
    `refreshEstates`; doc 54 reuses it (`commonRooms_`).
  - `Core/RatwRoads.cpp`: `World::courierHours`, built in `setupTowns` from `roadRoutes_`.
  - `Core/RatwWorld.h`, `Core/RatwWire.cpp`: `Entity::postTown`, saved.
  - `Core/RatwGame.cpp`: dispatch `letter` and `letters`; `self.letters`.
  - `Client/src/ui/hud/letters.ts`, `state.ts`, `dialogs.ts` (the LETTERS tab).
- **Tests:**
  - `Tests/letters_tests.cpp` (new): writing only where allowed; addressing by a known name, refused by an unknown
    one; the fee to the right treasury; courier hours in town and between towns; handed over in town, waiting
    elsewhere, collected at an inn, sent on; scent read by name, by sight, unknown, masked, faded; a signature
    introduces; a reply by the same courier; limits; saving and loading.
  - `Tests/pg_tests.cpp`: the documents table round trip. `Client/src/game/letters.test.ts` (node:test).
  - `tools/client/letters.mjs`: Ash writes to Bo at the inn, Bo receives it across town and reads the scent line.
- **Done when:** two players in different towns exchange letters, delivery takes the stated time, the scent and
  signature read as the table says, and a restart keeps letters in transit.
- **Cost:** commands only; delivery from a queue ordered by `deliverAt`, so a tick looks at what is due and nothing
  else. A thousand players writing a few letters a day is a few hundred documents a real hour; the store is bounded by
  the per-wolf caps. `world_check --players 20` unchanged; `game_load --players 100` unchanged.

### Phase 2: giving, enclosures and the giver's scent

- **Goal:** wolves give things face to face or by letter, conserved, and gifts smell of the giver.
- **Changes:** `Core/RatwGameGive.cpp` (new): `give`, `giveAnswer`, the `gift` event, the once-a-day bond rule;
  enclosures in `RatwGameLetters.cpp` with the `letter:` facility account (`RatwCareers.cpp`); `Entity::scents` with
  giver records (`RatwWorld.h`, `RatwWire.cpp`); `Game::itemLabel` and the inventory view read them; the unearned kinds
  (with the economy session).
- **Tests:** `Tests/letters_tests.cpp` (an enclosure escrowed, taken, refused for a full purse, returned at 56 days,
  money conserved); `Tests/social_game_tests.cpp` (a give accepted and declined, a resident's warmth from the `gift`
  event, once a day, the 64-kinds refusal); `Tests/bonds_tests.cpp`.
- **Done when:** a gift and an enclosure arrive with "It smells of …", `Society::conserved()` holds, and no save can
  exceed 64 kinds through them.
- **Cost:** commands only; scent records are touched only when goods move to or from a player.

### Phase 3: grooming

- **Goal:** the daily favour, as agreed.
- **Changes:** `Core/RatwFavours.cpp` (new): asking, consent, the 15 s grooming, limits, effects, bonds, the posted
  action; `World::scentScale` in `RatwMarks.cpp`, used by `noticeSenses` (`RatwBattle.cpp`), `scentClarity` and
  `scentCues` (`RatwWorld.cpp`, which also learns masking); the healing factor in `restPlayers` and `returnFromAway`;
  the roll modifier in `giveLasting` and `Injury::cleaned` in `injureOnDown` (`RatwInjury.*`); first impressions in
  `bondsFromEvent` and `Game::heed`; `Entity::groomedUntil`, `groomHalf`, `groomScentUntil`, `groomedBy`,
  `groomedOtherDay`, `groomedSelfDay` (saved); the chat path's posting factored into `Game::postAs` for the action
  line; the client's buttons, prompt and CONDITION row.
- **Tests:** `Tests/grooming_tests.cpp` (new): consent and refusal, distance, once a day, until midnight, self for two
  hours at half; `injury_tests` (a lasting roll 10 points lower; licked clean on a severe injury, not added; floored);
  `battle_tests` (`sneak::noticing` with a groomed wolf smelt from less far; masking still wins); `bonds_tests` (first
  impressions only while familiarity is low); the posted action counted at half weight; `tools/client/grooming.mjs`.
- **Done when:** the table above holds in tests, and in the browser a grooming shows on both cards and in the scene.
- **Cost:** one field read in each sense check and each rest step; nothing per tick beyond that.

### Phase 4: the maker's scent on crafted goods

- **Goal:** every crafted good a player holds tells whose work it is.
- **Changes:** maker records written where players buy (`Game`'s `trade` handler, `RatwGame.cpp:3724`) when the shop's
  business makes the good (`items::businessFor` with `crafts.json` and `recipes.json`), and in `World::huntKill`;
  records dropped on selling to a shop; `itemLabel` and the inventory show them.
- **Tests:** `Tests/crafting_tests.cpp` or `letters_tests.cpp`: bought from the maker (scented), from a reseller (not),
  hunted (the hunter), sold back (dropped), faded at 28 days; masterworks unchanged (`tendMarks` still catches a
  thief).
- **Done when:** a cap bought at the tannery reads "it smells of the tanner at La Concia", and the economy's stocks are
  untouched.
- **Cost:** a lookup per purchase by a player; nothing for residents.

### Phase 5: letters from residents

- **Goal:** residents who like a player thank them, sometimes with a gift, and offer them work.
- **Changes:** `Core/RatwResidentLetters.cpp` (new): candidates from events, the hourly pass, the caps, gifts from the
  resident's purse; `Data/Voice/letters.json` and its loader and checks; `Contract::offeredTo`, `offeredUntil` and the
  courier offer in `roadsDaily` (`RatwRoads.*`); `mind::Client::letter` and the Mind's `/letter` (`tools/npc_mind.py`,
  light model, fact checks); `Options::letterModelCallsPerHour`; the briefing line in `dialogueContext`;
  `tools/letter_library.py`; `tools/ai_cost.py` learns the kind `letter`.
- **Tests:** `Tests/resident_letters_tests.cpp` (new): a contract done, then thanks a day later from a fond resident and
  none from an indifferent one; a gift from the purse only, never the last meal; the caps; a courier offer reserved,
  then open; the signature by name or role; the briefing line; `Tests/voice_tests.cpp` (the templates load and fill);
  `tools/test_npc_mind.py` (`/letter` in fixture mode: facts kept, a reply that drops one refused).
- **Done when:** in a game test a player is thanked and gifted, the money balances, and nothing calls a model unless
  the option is on.
- **Cost:** event-driven, an hourly pass over a small queue. Templates are free. With polish on, each letter is about
  400 input and 120 output tokens on gpt-5.4-nano; the caps allow at most about 110 letters a real hour at 1,000
  players, and the hourly limit caps the calls below that. Money follows the prices in the Mind's config.

### Phase 6: occasions, invitations and shared meals

- **Goal:** residents invite players to their weddings, namings, funerals and festival tables, and eating together
  matters.
- **Changes:** `Core/RatwOccasions.cpp` (new): occasions from `marriage`, birth and death events and festival days,
  hosts sent by errand, invitations through Phase 5's letters, witnesses; `Entity::fedUntil`, `ateAt` in `World::eat`;
  the Fed factors in stamina recovery and `restPlayers`; the shared-meal check and bond; the hosts' briefing line.
- **Tests:** `Tests/occasions_tests.cpp` (new): a marriage, an invitation, the hosts at the church at 11:00, a player
  there named witness, one absent not; a funeral; a festival meal shared; Fed alone and shared; `schedules_tests` still
  pass (the errands end and the day's plan resumes).
- **Done when:** a player invited to a wedding stands witness in a game test, and a shared meal lasts twice as long.
- **Cost:** occasions come from events, a handful a game day per town; the shared-meal check looks at the eater's own
  cell (`entitiesIn`).

### Phase 7: lending, letters carried by friends, pacts and sponsors

- **Goal:** the remaining favours.
- **Changes:** `Core/RatwFavours.cpp`: loans (`lend`, accept, return, due, debts through `Bonds::addOwed`), `lentCount`
  in the trade, give, enclosure and stores checks; courier contracts with player recipients and carriers
  (`RatwRoads.cpp` `tendRoads`); pacts in the document store; `chapter::Member::sponsor` (`RatwChapters.*`).
- **Tests:** `Tests/favour_tests.cpp` (new): a loan lent, worn, returned; due and carried back; gone, so owed and
  distrusted; a lent item can't be sold or given; a letter carried by Bo to Cy pays Bo; a pact sealed and witnessed;
  a sponsor recorded and told. `chapter_tests` for the sponsor.
- **Done when:** each favour works through the game, and money stays conserved.
- **Cost:** commands and a due-date queue for loans.

## Built

### Phase 1 (2026-10-08): the document store and letters between players

- **The store** (`Core/RatwDocuments.{h,cpp}`, new):
  - A `Document` (id, kind, author, scent, to, text, sign, fromTown, postTown, state, written, deliverAt, readAt,
    kept, viaCourier, answered, replyTo) and a `Store` indexed by reader and author, with the courier's queue ordered
    by arrival. A tick looks only at what is due.
  - Rules in `Data/Social/letters.json` (800 letters, 10 a day, 100 unread, the fees, the hours, 14 days' scent,
    familiarity 20 for a scent known by sight).
  - Saved in the people root's `documents` list. Migration `0044_documents.sql` gives it its own table,
    `game.documents` (kind, author, recipient, state as generated columns); it is written, not applied. Until it is,
    letters ride in the checkpoint row like the rest of the people root.
- **Letters** (`Core/RatwGameLetters.cpp`, new; the `letter` and `letters` commands):
  - **Where a letter can be written:** a cell where an inn's or a scriptorium's post works (by `items::businessFor` of
    the post's title, refreshed every five minutes), or a Chapter's own rented place for its members.
  - **The recipient:** a wolf the writer knows by the name typed, any case (`knownByName`, through `known_`'s names,
    or everyone's when names aren't hidden); two wolves known by one name are refused. Unknown: "You know no wolf
    called Zed." A blocked writer, a missing wolf and oneself all get "The courier can't find them."
  - **Signature:** one of the writer's own names, or none.
  - **The fee:** 1p in town, 2p plus 1p each 10 road cells to another town, by `Society::shift` to the writing town's
    treasury (kind "a letter's postage").
  - **The courier's hours:** 1 in town; `1 + cells / 3` between towns, at most 12 (`Game::courierCells` from
    `World::routeBetween` between the towns' markets).
  - **Limits:** 10 a game day; "Their post is full" past 100 unread.
  - **The receipt:** "The courier takes your letter to Bo (1p). It should reach Upper Accord in about 1 hour. You signed
    as Ash."
- **Delivery:**
  - A letter goes to the reader's **post town** (`Entity::postTown`, saved): set when it leaves the world, and at a full
    rest (watched through `fullRestDay`). Without one, the town it is in, else the capital.
  - On arrival, if the reader is online in that town, a messenger finds it ("A messenger finds you with a letter.", a
    `letterArrived` event). Otherwise it waits at that town's inns, and the reader, if online, is told where.
  - Walking into an inn of that town collects what waits there.
  - **SEND IT ON** works by town ("from": the town it waits in): a penny a letter and the courier's time again, to the
    town the reader is in. The plan had it per letter; by town fits the case, which lists waiting letters by town.
  - A letter whose reader is gone is let go.
- **Reading:**
  - The case (a `letters` event): delivered letters, unread first; each sealed until opened, with its signature, its
    scent line, when and where it came from, KEEP and BURN. Waiting letters are counted by town.
  - Breaking the seal of a signed letter introduces the writer by that name (`learnName`, how `"letter"`): "You break
    the seal. It is signed Ash."
  - **Scent lines:** by name ("It smells of Ash."); by sight, through a bond's familiarity of 20 or a known-wolves entry
    ("It smells of the grey wolf…"); "A wolf's scent you don't know."; masked when written ("It carries no scent at
    all. Someone took care."); past 14 days ("Its scent has faded."); a nose below 0.2 reads nothing.
  - **REPLY** goes to a writer known by name. Otherwise it goes once by the same courier (`viaCourier`), and the replier
    never learns who wrote: "The courier will take your answer back the way the letter came."
  - `self.letters` gives {unread, waiting, waitingAt}.
- **The page** (`Client/src/ui/hud/letters.ts`, new):
  - The letter case opens from the character sheet's LETTERS button, which shows the unread count.
  - It has the writing sheet (To, the text with its count against 800, a signature of one's own names or Unsigned,
    SEND), letters waiting by town with SEND IT ON, and each letter with BREAK THE SEAL, REPLY, KEEP and BURN.
  - "A letter has come for you." shows as a toast.
  - Drafts are kept in state (`letterDraft`) and survive rebuilds while typing.
- **Tests:**
  - `Tests/letters_tests.cpp` (new, 38 checks): the courier's hours and fees; the store's order, queue, unread and save.
    In the game, on a three-town strip with an inn in each town:
    - an unknown name refused; not in the street; the fee to the treasury;
    - delivered in town after an hour, sealed, with an unknown scent, then opened and introduced ("It smells of Ash.");
    - masked and unsigned with no scent, answered once by the same courier;
    - ten a day; to Ridgemere for 2p and about 4 hours, waiting there, sent on;
    - collected at an inn; faded after a fortnight; a letter in transit across a restart.
  - `Client/src/game/letters.test.ts`: opening asks for the case; the case kept; an arrival refreshes it; the command;
    letters counted, not bytes.
  - `tools/client/letters.mjs` (new; the strip with an inn by the spawn; `--speed 30`): Bo introduces himself; Ash writes
    from the case, signed; the courier's receipt; a messenger finds Bo; sealed, signed Ash, an unknown scent; opened, it
    smells of Ash. Screenshots in `artifacts/screenshots/letters/`.
- **Not done:** the DM app's view of letters (Phase 1 kept to the game); `pg_tests`' round trip, which waits for
  migration 0044 to be applied.

### Phase 2 (2026-10-08): giving, enclosures and the giver's scent

- **Give** (`Core/RatwGameGive.cpp`, new; `give` and `giveAnswer`; "Give" in any wolf's menu within 2 tiles, opening
  a Give sheet):
  - What can be given: an item held and not worn or in the jaws (`Game::spareOf`), a quantity, and/or coins, within 2
    tiles, both out of a fight.
  - A **player** is offered it (`self.giveOffer`, an Accept / Decline row under the map, 30 s). A **resident** takes it
    unless its liking for the giver is −30 or worse. A blocked giver is refused like a wolf who won't take it.
  - The move is `Society::shift` with kind "a gift", added to the unearned lists in `Society::record` and the
    orchestrator's `unearned()` (the economy session's files: one line each), so it is never a resident's profit.
  - The `gift` event warms the receiver by `bondsFromEvent`'s rule, once a game day a pair (`giftBondDay_`). Later
    gifts that day record "gift again", which warms nothing.
- **The 64 kinds:** `Society::shift` now refuses goods of a new kind past `MaxGoodsKinds`, so no give, enclosure or
  any other move can make a save that won't load (the bug the epic logged). A give says "Bo can't carry another kind
  of thing."
- **Enclosures** (`write` with `enclose: {item, quantity, coins}`):
  - One kind of thing, a pound in all at most, and none weightless (livestock weighs 0), plus up to 50p. Two loaves are
    refused.
  - Held in an escrow account `letter:<id>@<writer>` (the `letter:` prefix added to `facilityAccount`), journaled with
    the economy (`record(Economy | Character)`).
  - **TAKE** moves it to the reader. With no room for its kind, the pennies come out and the rest stays in the letter
    ("You take 5p from the letter; you have no room for the rest, so it stays in the letter.").
  - Unread for 56 game days (checked once a game hour), undelivered, or burnt unopened, it goes back to its writer, or
    to the capital's treasury if the writer is gone.
  - On load, an escrow with no letter (a crash between the journal and the checkpoint) goes back to the writer named in
    its account id.
- **Scent records** (`Entity::scents`, `ScentRecord{item, maker, giver, count, madeDay, givenDay}`, at most 60, saved):
  - They move with the goods between players, oldest first, the giver's own scent on them unless masked, and are
    dropped when goods go to a resident or a till.
  - Read newest first within what is held, so goods sold or eaten simply stop counting: a giver for 7 game days, a
    maker for 28.
  - Shown on belongings ("It smells of Ash") and told on taking ("You take 1 trencher bread from Ash. It smells of
    Ash.").
- **Tests:**
  - `letters_tests` `giving` (now 59 checks): offered, declined, accepted, it smells of her, money conserved; warmed
    once a day; too far off; a resident takes it; refused at 64 kinds; an enclosure escrowed, too heavy refused, taken
    in part when there's no room, then in full with her scent; returned at 56 days; conserved throughout.
  - `tools/client/letters.mjs` gains the gift: Give from Bo's menu, the Give sheet, his Accept row, his bread "smells of
    Ash" in his belongings.
- **Not done:** a resident's letters' gifts and the maker's scent are Phases 5 and 4. Doc 54's stalls will move scent
  records when they exist.

### Phase 3 (2026-10-08): grooming

- **Asking** (`Core/RatwGameFavours.cpp`, new; `groom` and `groomAnswer`; "Groom" in the menu of a wolf within 1.5
  tiles; "Groom yourself" in the actions row):
  - Both must be out of a fight and still; not blocked either way; one grooming at a time.
  - A player accepts within 30 s (`self.groomOffer`, an Accept / Decline row). A resident accepts only from a player it
    likes (liking 40, trust 30). Residents never groom players (the user).
  - Fifteen seconds, both within 1.5 tiles. Moving apart or a fight breaks it off with nothing.
  - Once a game day for the groomer, and once for oneself (`groomedOtherDay`, `groomedSelfDay`).
  - Done, the groomer's line is posted as its action (`/action`, counted at half weight): its own words (200 letters),
    or "grooms Bo's ruff, slow and careful."
- **Its effects** (`Core/RatwFavours.cpp`, new; `World::applyGrooming`, `groomedFactor`, `scentScale`):
  - **Well-groomed** lasts 24 game hours, or until the wolf's next full rest (`fullRest` ends it), as the user answered.
    Groomed again only renews. Self-grooming gives half of every effect, for 2 game hours, and never replaces a grooming
    by another that is still on.
  - **First impressions:** in `bondsFromEvent` (trade and talk) and `Game::heed` (the Mind's nudges, still within their
    caps), a resident who hardly knows the wolf (familiarity under 25) warms ×1.25 (×1.125 self-groomed).
  - **Healing:** injuries heal ×1.1 at rest (`restPlayers`), and away for the part of the time it lasted
    (`returnFromAway`).
  - **Fewer lasting injuries:** every lasting roll −10 points (−5 self-groomed), after weariness, floored at 0
    (`giveLasting`).
  - **Licked clean:** a severe acute injury groomed by another carries `Injury::cleaned` (saved). Its setting roll is
    10 lower even after Well-groomed ends, but not added to the general −10.
  - **Less scent** for 2 game hours: the wolf's scent scale (0.6, 0.8 self-groomed, 0 masked) multiplies the nose's
    reach in `noticeSenses` and the clarity in `scentClarity` and `scentCues`. `scentCues` now also leaves out a masked
    wolf, a gap the plan noted.
  - **The bond for both:** +3 liking, +2 trust, +2 familiarity, once a pair a game day.
  - A `groomed` event, with IDs only.
- **Shown:**
  - `self.groomed` {by, half, hours, scent}, and the Status window's CONDITION row ("Well-groomed, by Ash · 23.9 h"),
    with what it does.
  - Others see the entity's `groomed` flag as "freshly groomed" in Look.
  - The fields are saved on the character (`groomedUntil`, `groomScentUntil`, `groomHalf`, `groomedBy`, and the two
    days).
- **Tests:**
  - `letters_tests` `grooming` (now 74 checks): asked and declined; walking off breaks it off; fifteen seconds and
    Well-groomed for a day, the scent at 0.6, the bond both ways, the line posted and seen; the Status block; once a day;
    self-grooming at half for two hours and once a day; self-grooming doesn't undo another's; masking wins; first
    impressions at exactly a quarter faster; worn off a day later.
  - `tools/client/letters.mjs` gains it: Groom from Bo's menu, his Accept row, Well-groomed by Ash, freshly groomed to
    her, "grooms Bo's ruff, slow and careful." in his story, and his Status CONDITION row.
- **Not tested on their own:** the lasting-roll and licked-clean modifiers, and the healing factor. They are one line
  each beside rules already tested; `giveLasting` is private and caps one lasting injury a fight, so a statistical test
  needs a hook. Left for the injury suite.

### Phase 4 (2026-10-08): the maker's scent on crafted goods

- **Bought from its maker** (`Game::makersScent`, after a player's purchase in the `trade` command): when the keeper's
  business makes the good (`items::craftsFor` its business, by the good's base id), the buyer gets a record with the
  keeper as maker. A reseller adds none. `recipes.json` isn't read: residents make from `crafts.json` only, and player
  crafting doesn't exist yet.
- **Hunted goods** carry the hunter: one line in `huntKill`, through the new `World::addScent`, which Game's
  `addScent` now uses too.
- **Sold to a shop**, the records go with the goods, oldest first, and are dropped there (`moveScents` to a resident).
  So a stale record can't scent a later purchase.
- **Shown** on belongings for 28 game days ("It smells of the keeper at the inn", by name once known), meals and herbs
  included (their special entries in the inventory now read scents too).
- **Masterworks** keep their mark in the id; nothing changed for them.
- **Tests:**
  - `letters_tests` `makersScent` (now 79 checks): a meal bought at the inn that cooks it smells of its keeper; bread
    from the stall smells of nobody; sold back, the record goes, and a meal got elsewhere smells of nobody.
  - `hunt_tests`: a clean kill's take carries the hunter's scent.

### Phase 5 (2026-10-08): letters from residents

- **Candidates from events** (`World::setEventWatcher`, new: told of each event as it is recorded; `Game::watchEvent`):
  a contract done for a resident, tending it when Downed (`tended`), or a gift worth 5p or more (coins plus the
  catalog price). One waiting thanks a pair, at most 500 in all. Nothing scans.
- **The hourly pass** (`Game::tendResidentLetters`, once a game hour):
  - **Thanks** go out the day after, if the resident's liking is 40 and trust 20.
  - One time in three they carry a gift from the resident's **own purse**: at most 6p and a tenth of what it holds,
    in escrow like a player's enclosure. Never a till's stock.
  - **Requests:** a resident's courier contract posted in the last day is offered first to the player its poster
    trusts most (30 or more), by letter: `Contract::offeredTo` and `offeredUntil` (2 game days), saved. Only that player
    may take it (`World::takeContract` refuses others), from anywhere, with TAKE IT ON (the letter verb `takeWork`,
    through `World::takeOfferedContract`). With nobody to offer it to, it is the board's.
  - **Caps:** 3 residents' letters a game week to a player, one from any resident.
- **The words** (`Data/Voice/letters.json`, `ratw-letters`): thanks and requests, each by tone, plus gift lines.
  - The tone comes from how the resident feels: warm at liking 70, brief when it trusts more than it likes, else
    plain. The plan's six personality tones are left for when the templates grow.
  - Blanks: `{to} {deed} {gift} {from} {place} {reward} {days}`. The deed in words comes from the event ("tending me
    when I was down", "carrying that letter"…).
- **The signature:** the resident's name if it would give it (`willName`) and knows the player well (familiarity 50).
  The name is then learnt on reading (how `"letter"`). Otherwise the case shows it by how the player knows it ("From The
  wolf who keeps the inn"). Residents' letters can't be answered by post (the user: not now).
- **Polish:** with `Options::letterModelCallsPerHour` above 0 (default 0), the filled template goes to the Mind's
  existing `polish`, which keeps every fact or returns nothing. The polished words replace the text if it is still
  unread. No new Mind endpoint was needed, so `/letter` and `letter_library.py` weren't built.
- **The briefing:** `residentLetterBriefing` adds up to two lines to `dialogueContext`'s activity, from the store: "You
  wrote to this wolf early spring, year 1 to thank them for tending me when I was down, and sent 4 pennies with it."
- **Tests:** `letters_tests` `residentLetters` (now 89 checks):
  - not the same day; the next day, thanks from the fond innkeeper only, by its role, for tending it, with no reply by
    post, money conserved;
  - three a week at most from five deeds;
  - a courier contract offered by letter to the player trusted most, naming the place and the pennies; Bo can't take
    it from the board while it is reserved; Ash takes it on from the letter.
- **Not tested:** the briefing line, which reaches only the Mind's prompt (no test hook), and polishing with the model
  (off by default; `polish` has its own tests).

### Phase 6 (2026-10-08): occasions, invitations and shared meals

- **Occasions** (`Core/RatwOccasions.cpp`, new; `World::planOccasion`, `tendOccasions`; `Occasion` with hosts,
  invited, coming and witnesses):
  - **A wedding** comes from a `marriage` between two residents: the next Restday, 11:00 to 12:00. **A funeral**
    comes from a family `mourning`: the next morning, 10:00 to 11:00, one funeral per death with every mourning family
    member a host.
  - The place is the community's church (its pulpit in the economy's day plans), else the first host's home.
  - **Hosts go there by errand**: a few lines at the top of `World::errand` (the roads session's file), which overrides
    the day's plan, as couriers' errands do. `RatwResidents.cpp` isn't touched.
  - **Invitations** (`Game::inviteToOccasions`, in the hourly letters pass): a day ahead, the hosts invite up to 3
    players they like (liking 50), by a resident's letter (`wedding` and `funeral` templates: `{when} {place} {couple}
    {deceased}`).
  - The letter has **COMING / CAN'T COME** (the `answer` verb; `Occasion::coming`).
  - **Witnesses:** an invited player within 10 tiles of the place during the hour stands witness. The hosts warm to it
    (+5 asked; the bond rule's own dampening applies), a `witnessed` event is recorded, and each host's briefing
    remembers it ("This wolf stood witness at your wedding.").
  - Occasions aren't saved: one planned before a restart is let go.
- **Not built:** namings (the world records no birth event yet) and the festival meal at a resident's table (it needs a
  way to find which players a household likes without a scan; doc 56's per-wolf memory will give one).
- **Meals** (`World::fed`, from `World::eat`):
  - Eating gives **Fed** for 2 game hours: stamina comes back ×1.1 out of a fight, and rest heals ×1.05.
  - A wolf within 2 tiles who ate in the last 10 game minutes (a resident eating right now counts) makes it a **shared
    meal**: Fed lasts 4 hours for both, and each pair gains +1 liking and +1 familiarity, once a game day (a `shared
    meal` event).
  - `fedUntil` and `ateAt` are saved; `self.fedHours` feeds the Status window's "Fed · 1.9 h" row.
- **Tests:** `letters_tests` `occasionsAndMeals` (now 100 checks):
  - a marriage's wedding falls on a Restday at 11:00; a day ahead, Ash is invited by letter and Bo isn't; she answers
    she'll come;
  - at the hour, Ash stands witness and Bo, there but uninvited, doesn't; the host warms to her;
  - a family death's funeral the next morning at 10:00;
  - Fed alone for 2 hours; a shared meal 4 hours for both, and closer.
- **Not tested:** the hosts walking to the church. Their errand is a few lines, but the strip's residents have no
  church to walk to in a test.

### Phase 7 (2026-10-08): lending, letters carried by friends, pacts and sponsors

- **Lending** (`lend`, `lendAnswer`, `return`; "Lend" in a player's menu within 2 tiles, opening the Give sheet in its
  Lend mode with days):
  - An item for 1 to 7 game days to a player who accepts (`self.lendOffer`, an Accept / Decline row, 30 s). The goods
    move by `Society::shift` (kind "lent"), with a saved `Loan` in the people root's `loans` list.
  - The borrower may wear and use them, but `spareOf` takes lent goods off what it may give or put in a letter, and the
    `trade` command refuses to sell them ("That is lent to you; it isn't yours to sell.").
  - **RETURN** within 2 tiles of the lender; or at the due day, once a game hour, a courier carries them back and the
    borrower pays 1p to the capital's treasury.
  - **Gone** (used up, sold before the rule, lost): the borrower owes the catalog price (`Bonds::addOwed`, its first
    use in the game), and the lender's trust falls by 10 (the bond rule dampens it a little).
  - Belongings list what is borrowed (with RETURN) and what is lent out (`self.loans`). Wear isn't carried across, as
    planned. Chapter stores don't check loans yet.
- **A letter carried by a friend** (`write` with `by`: a player the writer knows by name, who knows the recipient by
  name):
  - The town's postage is given back and the fee goes into a courier contract (`World::postContract`, escrowed from the
    writer) already taken by the friend. The letter waits in the friend's keeping (state `carried`, out of the
    courier's queue and out of the recipient's case).
  - Standing within 3 tiles of the recipient completes the contract by the existing rule (`tendRoads` already accepted
    a player recipient). The friend is paid and the recipient told "Bo hands you a letter."
  - If the contract lapses, the town's courier takes it. The friend is told; they don't choose to accept, a small gap.
- **Pacts** (`pact` with `write`, `seal`, `witness`; a SET DOWN A PACT form in the letter case):
  - Terms of 1 to 400 letters naming another wolf known by name; the writer seals at once. A copy goes to each party
    (document kind `pact`, a shared `pact` id, with `party`, `seals` and `witnesses`, all saved). The other party seals
    from its case.
  - When both have sealed, a `pact sealed` event (IDs only) is recorded for the chronicle.
  - A party may ask up to 3 witnesses known by name: each gets a copy and may seal it as a witness. Every copy shows
    every seal. Nothing enforces a pact.
- **Sponsors** (`chapter::Member::sponsor`, saved): who invited a member is recorded at accepting
  (`Chapters::invitedBy_`), shown on the roster ("brought in by Ash"), and told when the initiate rises in rank or is
  sent away. Standing and renown don't change.
- **Tests:**
  - `letters_tests` `favours` (now 118 checks): a loan offered, borrowed, not to be given, listed, handed back; at the
    day carried back for 1p; used up, a debt and less trust; money conserved; a letter carried by Bo reaching Cy only
    when handed over, Bo paid; a pact set down, sealed by both (`pact sealed`), witnessed by Cy, three seals on every
    copy.
  - `chapter_tests`: the inviter is the sponsor, and it is saved.
- **Not done:** loans in Chapter stores; asking the friend's consent before carrying a letter; the DM app's views of
  letters, loans and escrow, and its `letter.return` and `loan.end` actions (the plan's DM section). All are noted for
  the next pass.

## Depends on and feeds

- **Depends on:** doc 50 for `blocked()` and the known-wolves entry used by scent (Phases 1 to 3 work without it:
  blocking is a no-op, and scent by sight uses bonds alone); doc 54 for lodgings (letters use post towns until then).
- **Feeds:** doc 54 (notices live in the document store; stalls carry scent records); doc 56 (letters waiting in
  "while you were away", unanswered letters in unfinished business, witnesses, pacts and gifts in the chronicle);
  doc 57 (residents' troubles become requests by letter); doc 52 (a mentor's welcome letter can use residents' letter
  machinery); doc 58 (storytellers may send letters as a story's messengers).

## Risks

- **Abuse through letters:** harassment by post. Blocks (doc 50) stop it, reports carry the text, and the per-day limit
  slows it. Anonymous letters are anonymous to players only.
- **Money parked in letters:** bounded by the 50p and 1 lb limits and the 56-day return.
- **Scent identity:** it is a change from doc 02's "scent never identifies". This plan keeps doc 02 for living wolves
  and lets scent identify only objects in paw, gated by what the reader already knows.
- **Economy files:** the unearned kinds touch `RatwOrchestrate.cpp`, and occasions must not fight the day plans
  (`RatwResidents.cpp`). Both are owned by the economy session today; the errand override avoids the second, and the
  first is a two-line list change to agree with its owner.
- **First impressions** are a mechanical effect of a visible state, which `RatwAppearance.h` rules out for appearance.
  Grooming is a state, not appearance; the header's comment should say so.
- **Grooming late in the day** is worth little, since it ends at midnight (agreed). See the open questions.

## Decisions

Agreed (doc 48):
1. Letters between players, written at an inn, a post or a desk, addressed by the name you know, delivered with travel
   time, carrying an item or coin, sealed, with the writer's scent; anonymous with masking oil (§3.8, decision 14).
2. Letters from residents: thanks with gifts from their own goods, invitations, requests that become contracts; by
   template or the cheap model (§3.8).
3. Every crafted item and gift carries its maker's or giver's scent (§3.8, decision 11).
4. Grooming: once a game day, with consent; Well-groomed for the rest of the game day; first impressions, healing,
   every lasting-injury roll 10 points lower, less scent for 2 game hours; weaker than Wash Out; self-grooming half for
   2 hours; bond for both; an authored scene action (§3.9, decision 31).

New placeholder choices in this plan:
5. One document store (`game.documents`) for all in-world writing; letters are documents, never sellable items.
6. Couriers are times, not walkers: 1 game hour in town, `1 + cells ÷ 3` between towns, at most 12.
7. Letters wait in the recipient's post town and are collected at its inns or handed over in town.
8. A signature introduces; an anonymous letter can be answered once by the same courier.
9. Scent records sit beside players' stacks, not in item ids; residents' stocks stay unmarked.
10. Wolves get a scent scale; grooming sets it to 0.6 (self 0.8); masking oil stays 0.
11. Meals gain a Fed state, which a shared meal doubles.
12. Occasions (weddings, namings, funerals, festival meals) with invited players named witnesses.
13. Lending has a due day; goods lost on loan become a debt. Pacts are documents nothing enforces. Sponsors are
    recorded on the Chapter roster and change nothing else.

## Open questions

All answered by the user, 2026-10-08:
1. **Late grooming:** Well-groomed lasts 24 game hours or until the next full rest, whichever is first; not until
   midnight.
2. **Residents grooming players:** no. Players groom; residents may be groomed by players only.
3. **Letters to residents:** not now. Speaking to them is enough; residents write to players.
