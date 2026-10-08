# 53. Hunting and working together

Drafted 2026-10-06 as an actionable plan for doc 48 (§§6.1–6.7, 6.9 and 5.3). Open questions answered 2026-10-07;
Phases 1 to 4 built the same day; 5, 6 and Phase 7's part A the next (see "Built"). Phase 7's part B waits for doc 35's
player Craft panel. Read doc 48 (Principles,
Part 6, §5.3 and the Decisions) and docs 41, 40, 33, 37, 35, 43, 42, 26, 38, 44 and 31 first.

Marks: **(agreed)** is the user's decision, with its place in doc 48; *(placeholder)* is one constant to change.

## The ask

> "Goals that need each other: this is a good idea and I think it might be the biggest research project in RATW. I
> need to find ways that put wolves working toward the same goal, but from two angles, together."

> "If a player walks upon another in the wilderness who is hunting, that player should be able to join the hunting
> wolf in their hunt and it will drop them into the hunting instance just like in combat (every player should have
> an option in their settings to toggle on/off allow hunting partners, which should default to on)."

> "Kills should be shared, yes, but it should be worth the cooperation as well. Animals should be fast and hard to
> catch, but should freeze when they see a wolf at a distance, and then flee in the opposite direction if the wolf
> gets close. This allows players to chase animals into a team mate. Animals should go down with a single bite, but
> there should still be a chance at the animal dodging."

> "Since they don't have hands, having one wolf hold a threshing tool and the other gather makes things go faster."
> · "Maybe some sort of crafting can require two wolves performing two different tasks?"

Gifted and Quickened wolves should fit in "naturally", and future player jobs should be done "in proximity or in
cooperation with other wolves".

## Where we stand (read from the code 2026-10-06)

**Hunting** (doc 41, `Core/RatwHunt.cpp`; animals in `Data/Wild/animals.json`, read by `Core/RatwWild.*`):

- `World::startHunt` makes a fight with `Battle::hunt`, finds game by ground, the cell's pressure and the players online
  (`World::huntExpected`), boosts a trail the starter smelt, then runs doc 40's positioning phase. `World::addAnimal`
  puts each animal far off, grazing. Its state is `World::HuntAnimal` (`RatwWorld.h`): species, one `alert` flag for
  all hunters, the hardest blow, fire, `lastBy`, `struckUnaware`.
- `World::animalTurn`: unaware, it grazes; half-noticing a hunter (doc 40's `Battle::aware`), it freezes and faces it;
  alert, `flee` runs for the edge away from all hunters, `cornered` fights when a hunter is adjacent or it is hurt,
  `fierce` attacks.
- A bite's odds are the fight's (`World::strikeChance`: 0.75 ± DEX, skill and angle, plus doc 40's ambush bonus).
  `World::huntBlow` scales blows by the species' health; `World::huntKill` grades by the hardest blow, spoils for
  fire, sets quality and gives **everything to the killer**, with `award(killer, "practice", "hunt:<day>")`.
- `World::tendHunts`: an animal may wander in every 30 s at a chance of expected / 6, up to 6, however many hunt.
- **Joining:** `World::joinBattle` asks `World::huntJoinRefusal`: a hunter's companion, or a friend by
  `World::setFriends` (`Game` wires it to party or Chapter). A joiner under way comes in at the edge, not stalking.
  Party companions join a party player's fight in sight (`RatwGameCompanions.cpp`) and charge the nearest animal.
- **A bug:** `Game::fightsInView` sends `canJoin` without the hunt rule, and `Client/src/ui/hud/fight.ts` offers a
  button per side, so strangers see "Join Ash" and "Join A rabbit" on a hunt, and the server refuses both.
- A hunt is already a scene (`Game::tendFightScenes`, `SocialLedger::joinFight`). Tests: `Tests/hunt_tests.cpp`
  (`onlyFriendsJoin` among them), `battle_tests` `sneak::noticing`.

**Settings:** the one per-player fight setting is `Entity::noPvp` (saved in `RatwWire.cpp`, command `noPvp`, a toggle
in `Client/src/ui/hud/dialogs.ts`). No block list yet (doc 50).

**Work:**

- Foraging (`World::forage`) is solo; patches (`World::takeFromPatch`, 4 pickings each) are shared with residents
  working out of town (`RatwOutwork.cpp`).
- Farms produce by spells (`Society::produce`): `out` in season, `offSeason` (threshing) in winter, up to
  `ProducerKept`. Farm hires (`RatwFarmhands.cpp`) and odd jobs (`Society::OddJob`) are for residents. Players can't
  work a farm. A hand's pay is `Society::dayWage(town, "hand")` (doc 46: 12p a day to start), by `PaidSpells` (8).
- Work Gifts (`World::useWorkGift`, `lends()` in `RatwMagic.cpp`) lend a lift to a resident's workshop
  (`Society::lendGift`); none to farms or hauling.
- Crafting is residents' only (`Society::craft`). `Data/Items/recipes.json` marks ten recipes `helper: true`, but the
  game reads `crafts.json`. Doc 35's player Craft panel (Phase 5) isn't built ("rent first, own later", its Open
  question 4).

**Fighting practice:**

- Duel terms are `blood`, `yield` (the default) and `death` (`Battle::terms`). In a yield duel `World::injureOnBlow`
  (doc 38) still rolls acute (25+) and lasting (40+) injuries, and bites bleed.
- `World::growSkill` does nothing: fighting skill comes from level (`levels::fightingSkill`) until doc 49.
- Watching (`World::observeBattle`) locks a watcher out of that fight.
- No training ground exists in the game, only as ground: the Warden Training Grounds (region `training_grounds`, rings
  and targets from `tools/worldgen/campuses.py`) and barracks cells (Greyfen's `barracks.cell`). No resident spars.
- Quickened magic seen by anyone, partners included, adds to `Entity::wardenAttention` (`World::wardensSee`).

**DM:** the watch frame (`Game::watchFrame`) flags "in a fight" (8) and has a `doing` line; nothing shows hunts or
shared work.

### Checks against doc 48

1. **"More hunters already bring more animals (doc 41: the expected count scales with players)."** Not so. It scales
   with players online in the world. A second hunter in a hunt brings no more game, so equal shares would halve each
   wolf's haul. This plan adds game per hunter (Design 1.6).
2. **§6.3's ×1.8 can be read two ways.** If it is the team's output, two partners each earn 0.9 of a lone wolf, a
   resident hand (×1.4 for the player alone) beats a player partner, and groups do worse per wolf. That contradicts
   §6.3 ("a player partner is better"), Decision 5 ("more efficient with more wolves") and the hunting target. This
   plan reads it as **each wolf's rate** (Design 2.1). Open question 1.
3. **§6.7's table** mostly names built Gifts, with four gaps: Cauterize works only in a fight (it stops Bleeding), so
   "after a hunt" becomes "when a boar opens a partner's flank"; Winnow and Dry isn't lent to farms; Lighten Load lifts
   only one's own load; there is no hauling or player mining for Lighten Load or Stone Sense to help. Phase 6 adjusts.
4. Doc 40's seam names `Data/Animals/animals.json`. The file is `Data/Wild/animals.json`.
5. Doc 48's dependency on doc 50 is softer than its table says. Phase 2 adds the two settings as saved flags (the
   `noPvp` pattern) and a block hook. Doc 50 can fold both into its own settings and block list later.

## Scope

**This plan does:** hunting together (§6.4); the cooperation scaling (§6.3) and the two-angle patterns (§6.2) as one
reusable joint-activity mechanism; foraging together, farm work at harvest and threshing, and Lend a paw (§6.5);
crafts for two (§6.6); Gifted and Quickened angles (§6.7); training grounds and sparring (§5.3); the note on player
jobs (§6.9).

**It leaves:** the settings panel, block, mute and report to doc 50 (this plan calls its block check); scene
openness, stars and end screens to doc 51; matchmaking and vouching to doc 52; festival contests to doc 54 (built on
this plan's hunts and spars); deeds to doc 56; town projects to doc 57 (using `together::rate`); two-angle quests
(§6.8) to doc 58; and the skills and how they grow to doc 49 (this plan says which skill each act practises).

## Design

### 1. Hunting together (agreed, doc 48 §6.4)

#### 1.1 Four states

Each animal that flees (every species but the `fierce` ones) is in one state, kept in `HuntAnimal`:

| State | When | What it does |
|---|---|---|
| **Grazing** | Alert to no hunter. | As now: grazes, a step now and then. Half-noticing a hunter (doc 40's suspicious) it lifts its head, faces it and stops grazing. |
| **Watching** | Alert to a hunter further off than its **flight distance**. | Stands frozen, facing that hunter (the nearest it is alert to). No grazing, no wandering. |
| **Fleeing** | The hunter it watches ends a turn within its flight distance; or it is bitten at and missed; or it is alert to a hunter already within flight distance. | Its bar fills at once (it bolts) and it runs **directly away from that hunter**, a move of **1.5 × the fastest hunter's sprint move** *(placeholder)*, every turn until it is off the edge or calms. |
| **Calming** | Its awareness of the hunter it fled has been under suspicious for 2 of its turns *(placeholder)*. | One turn at its ordinary move, still away. Then Watching if it is alert to any hunter, else Grazing. |

- **Flight distance** is a new species field `flight`, seeded at half of `alert` *(placeholder)*: a rabbit 3.5, a roe
  deer 6, a red deer 7. (`alert` is read but unused since doc 40's senses took over noticing.)
- **Only noticed wolves count** (agreed). A wolf the animal isn't alert to plays no part in where it runs. The flight
  line ignores it, so a driven animal can pass beside a hidden partner.
- **When it reacts:** at the end of each hunter's turn (where doc 40 already checks noticing) and at the start of the
  animal's own turn. Not at each step. So a hunter can rush a watching animal and bite in the same turn: that is the
  60% row below.
- "Directly away": of the tiles in reach, the one furthest along the line from the hunter through the animal, with
  a small cost for straying sideways. On the edge it gets away, as now.
- `cornered` keeps one meaning: a fleeing animal with no tile further away bites the wolf beside it once instead
  *(placeholder)*. Fleeing costs no stamina *(placeholder)*. `fierce` animals (boar, bear) keep doc 41's health,
  temper and quality rules (agreed, Decision 37).

#### 1.2 One bite, and the dodge

- **A landed bite kills any animal that flees** (agreed): rabbits to roe deer, red deer, elk, fox, badger and mountain
  goat. A blade's landed blow counts the same (the user, 2026-10-07). Fire and other Gift blows keep doc 41's health
  rule.
- The bite's chance is **1 − dodge**, replacing `strikeChance` for these animals:

  | The animal… | Dodge *(placeholders, agreed shape)* |
  |---|---|
  | is grazing and not alert to the biter | 5% |
  | is half-noticing the biter (suspicious) | between 5% and 60%, by how much it has noticed |
  | is fleeing another wolf and has never been alert to the biter (the drive works) | 15% |
  | is watching the biter, or alert to it while fleeing another | 60% |
  | is fleeing the biter | 75% |

- Shifts *(placeholders)*: a new species field `dodge` (hare +5 points, badger and elk −5); −0.2 points per point of
  the biter's DEX over 50; −0.2 per point of fighting skill over 50 (doc 49's hunting skill when it exists). Clamped
  to 2%–90%.
- A dodge sends the animal fleeing from the biter (agreed).
- **Quality** (agreed): masterwork when the animal was never alert to the biter, fine otherwise. Each animal keeps the
  set of wolves it has been alert to (`HuntAnimal::saw`). Fire still caps quality at common. A one-bite kill is clean:
  the full yield.

#### 1.3 Lying in wait

A drive only works if the ambusher can strike when the animal passes. In a turn-based arena it passes during the
animal's own turn, so:

- A hunter who **ends a turn crouched (stalking) without having bitten is lying in wait** until its next turn.
- When an animal steps next to a wolf lying in wait, that wolf **springs**: a bite at once, out of turn, at the odds
  above (usually the 15% row), costing a bite's stamina. One spring per wait.
- A kill ends the animal's run. A dodge stops it where it is (it swerves), and it flees from the springer on its next
  turn.
- Its own side sees "lying in wait" on its token. The spring counts as an ambush for `sneakSkill` (doc 40).

#### 1.4 Joining (agreed)

- The hunt's square shows **Join hunt** to anyone who can see it, as Join fight does.
- Who may join: the hunters' party or Chapter and companions (as now), and **anyone at all while the hunter who started
  it has Allow hunting partners on**. Only the starter's setting counts (the user, 2026-10-07: "only the one who started
  it should have control"). No one blocked by any hunter (doc 50) may join or see the button.
- **A closed hunt** shows **Ask to join**: the hunters get "Bo asks to join your hunt" (Let in / Not now, 30 s
  *(placeholder)*). Their hunt panel lists wolves within 20 tiles of the square *(placeholder)* with **Invite**. So a
  hunter with the setting off can still invite (agreed).
- The setting is per player and **on by default** (agreed), saved as `Entity::noHuntPartners` (missing means on).
- A joiner comes in at the edge, as now. Animals start unaware of them, so they can stalk at once.

#### 1.5 Sharing (agreed)

- **A kill is shared equally among the hunters taking part**: still in the hunt, and having moved, stalked, bitten or
  sprung in their last 10 own turns *(placeholder)*. The one who made the kill always takes part.
- Each item is split exactly: everyone gets the whole part, and the remainder goes one by one to sharers drawn by
  chance (doc 41's "fractions round by chance", without making or losing goods). Masterwork goods carry the killer's
  mark (`items::withMaker`).
- A companion's share goes to the wolf it follows *(placeholder)*.
- "You and Bo bring down a roe deer from hiding. Your share: 2 fine raw meat and the hide." Each sharer practises
  what it did (tracking, sneaking, the bite; doc 49, as in 2.2). Once a hunt, every pair who shared a kill gains bond
  (`Bonds::mutual`: +1 affinity, +1 trust, +2 familiarity *(placeholder)*).
- **Carrying:** at the end card each sharer may **Give my share to** another hunter (one `Society::shift` each, kind
  "a hunt's share"). Load and spoilage (docs 35, 42) make a strong carrier worth having.

#### 1.6 More hunters, more game

- Shares split one supply, so it grows with the hunters. Arrivals (`tendHunts`): the chance × (1 + 0.75 × (hunters
  − 1)), and `atOnce` + 2 per extra hunter *(placeholders)*.
- A joiner brings game in with them: half the hunt's expected count, rolled, entering unaware at the edge
  *(placeholder)*. A fresh trail any hunter smelt near the hunt counts as the starter's does today (the tracker's part).
- Pressure stays per cell (doc 41), so a pack that wants a big haul must roam, as real packs do.

#### 1.7 Roles emerge (agreed)

The server names each hunter's part at the end from what happened, as ledger events of IDs only
(`recordEvent("huntRole", ...)`) for doc 51's end screen and doc 56's deeds: **tracker** (their trail brought an animal
in), **driver** (an animal fled from them and a partner took it), **ambusher** (they took an animal fleeing someone
else, or sprang), **carrier** (others gave them their shares).

#### 1.8 Companions in a hunt

Today a companion charges the nearest animal and scares everything off. In a hunt it follows its leader's lead
instead: if the leader stalks, it stalks beside them and lies in wait; if the leader stands and walks at game, it
circles (stalking) to the far side of the animal the leader approaches and lies in wait there. The same code drives the
hunt simulation's ambusher (`World::huntHelperTurn`), so the sim measures what players will get from a companion.

#### 1.9 What the page shows

- Over each animal: grazing (its glyph), "?" half-noticing you, "!" watching, "»" fleeing, "~" calming. A watching
  animal shows a faint line from the wolf it watches, through itself and on: where it will run.
- The odds on its card are 1 − dodge, named: "95% · unaware", "85% · driven", "40% · watching you".
- One's own wolves lying in wait carry a mark. The hunt panel lists hunters, who takes part, kills, shares, Invite
  and asks. Outside: "Ash's hunt · 2 hunting · 1 taken", with Join hunt or Ask to join, never a button for the game.

### 2. Working together: the joint activity

#### 2.1 Cooperation scaling (§6.3)

One pure function in `Core/RatwTogether.h`, `together::rate(members, most)`: **each wolf's rate against working
alone**. Counting players first (in the order they joined) and resident hands after, the 2nd member adds 0.8, the
3rd 0.4, the 4th 0.25, the 5th and 6th 0.1 each, up to the activity's `most` *(placeholders, doc 48)*.

- A step is halved when every member so far has the same role (two in one role: ×1.4), and halved for a resident hand
  (a player and a resident hand in another role: ×1.4) (doc 48).
- So: alone 1.0; two players, two angles 1.8; same angle 1.4; with a resident hand 1.4; three 2.2; four 2.45; two
  players and a resident hand 2.0.
- Every member contributes its rate to the joint's output, whatever its role. Roles decide the rate, not who produces.
- Doc 57's labour is the sum of members' rates over the beats worked (`World::workLabour`).

#### 2.2 The joint activity

`JointWork` in `Core/RatwTogether.h`, kept by World (`joints_`, with `jointOf_` from each member), never saved:

- `id`, `kind` (an activity in `Data/Together/patterns.json`), `cellId`, a spot, a `target` (a patch, a producer, a
  workshop), and its `members` (id, role, resident or not, joined, last acted, beats).
- **Patterns and activities are data** (`Data/Together/patterns.json`): each activity names its pattern, its two roles
  and their words ("digs" / "carries and sorts"), its `most`, its beat in seconds, the skill each role practises, and
  the Gifts that add an angle (Phase 6).
- A wolf leaves by **Leave**, by going 8 tiles off from all the others (so a pair may range patch to patch together),
  or after 60 s idle *(placeholders)*. With no player left it ends.
- Ending: every pair of players who worked 2 beats together gains bond (+1 affinity, +0.5 trust, +2 familiarity
  *(placeholder)*), a resident partner more (+2 affinity, +2 trust: residents remember who helped), and a ledger event
  of IDs (`together`) for docs 51 and 56.
- **It is a scene** (§6.3): `SocialLedger::workScene(id)` and a `work:<id>` tag, as fights have, so talk while
  working counts and is paid like a fight's scene. Doc 51's openness applies when it lands (open by default).
- **Practice:** each beat, each member practises its role's skill through doc 49's call, faster beside a better wolf
  (doc 49). Until doc 49: `award(id, "work", "together:<kind>:<day>")`.

#### 2.3 Lend a paw (§6.5)

- A player at work (foraging now, or in a joint) shows **Lend a paw** in the menus of players within 6 tiles
  *(placeholder)*, if the worker's **Allow work partners** is on (default on, `Entity::noWorkPartners`) and neither
  has blocked the other (doc 50).
- Lending makes or joins the joint, with the free role offered first and the other as a second choice.
- A worker with the setting off can still invite: "Ask Bo to lend a paw" in Bo's menu.

#### 2.4 The patterns (§6.2)

| Pattern | How this plan carries it out | Phase |
|---|---|---|
| Holder and worker | Threshing (holds the flail / gathers and sorts); foraging (digs / carries and sorts); crafts (lead / hand). | 3, 4, 7 |
| Finder and taker | Hunting's tracker; a Gift that finds (Stone Sense, Dowse) as a third angle. | 2, 6 |
| Driver and ambusher | Hunting (1.1–1.3); Throw Voice as a driver from afar. | 1, 6 |
| Worker and guard | "Keep watch" as a role in any joint in the wild. | 6 |
| Team haul | Not yet: carts and sleds are in the catalog (`work_harness`) but not the game. | — |
| Many paws | Harvest joints up to 6; doc 57's projects. | 4, doc 57 |
| Talker and doer | A resident in talk faces the talker; Throw Voice. | 6 |

#### 2.5 Foraging together (§6.5)

- Both press Forage as now. Each picking taken by a member gives `count × rate` (fractions by chance) into the joint,
  shared exactly among its members as in 1.5.
- The patch gives one more picking for each extra member, up to two more (4 → 5 → 6) (doc 48's placeholder, extended).
- So two foragers on two angles each take 1.8× a lone forager's goods in the same time, and a patch gives the pair 9
  goods where it gave one wolf 4.

#### 2.6 Farm work at harvest and threshing (§6.5)

- **When:** at harvest (autumn) for farms, orchards and vineyards, and threshing (winter, the producer's `offSeason`)
  *(placeholder: other seasons later)*.
- **Where:** a farm worker at work at its spot shows **Help with the harvest** (or **the threshing**) in a player's
  menu. That starts a joint with the resident as a member (so a player alone works at ×1.4), and other players may lend
  a paw.
- **Each beat** (a spell: 30 game minutes, five real minutes *(placeholder)*) a player's work brings in its rate × a
  share of one spell's yield *(placeholder: a third)* for the farm, from the producer's `out` or `offSeason`, while the
  farm holds under `ProducerKept`.
- **Pay** comes from the farm's own till: `dayWage(town, "hand") / PaidSpells × rate` per beat, a piece rate, with
  fractions of a penny carried. Money only moves (doc 15). If the till can't pay or the barn is full, there is no work:
  "Hale can't pay for more hands today."
- The farmer's own spells go on as before; players add to them (doc 48 §1.2). The pay counts as the "hand" wage kind,
  so the orchestrator sees it (doc 46).

### 3. Crafts for two (§6.6)

- **Lead and hand:** the lead's skill sets the quality; the hand's skill sets the speed (the batch's seconds ÷ (1 +
  hand's skill / 100) *(placeholder)*) and the waste (a chance, hand's skill / 200, that one input isn't used up
  *(placeholder)*).
- **Heavy work needs a hand:** the recipes with `helper: true`. A resident hand can fill in, hired for the beat at the
  hand rate from the lead's purse (to the resident: conserved). Most other recipes go at the joint's rate with a hand.
- **A player as hand at a resident's workshop** needs no player crafting, so it comes first: the maker's next batches
  go faster by the player's rate and waste less (`Society::lendPaw`, shaped like `lendGift`), the shop pays the hand
  rate per beat, and the player practises the trade beside a master (docs 26, 49).
- **Player-led crafting** waits for doc 35 Phase 5 (the player Craft panel, renting a station).

### 4. Gifted and Quickened (§6.7)

**Gifted wolves add a third angle.** A Gifted member using its family's work Gift on the joint counts as a different
role even if it shares one, and lifts every member's rate by 0.2 for that beat *(placeholder)*, at the Gift's mana:

| Family | In a hunt (hunts are fights: these work there now) | At shared work |
|---|---|---|
| Fire | Cauterize when a boar opens a partner's flank; Smother to Smoke hides a stalk. | Forge Heat and Kindle (crafts, Phase 7). |
| Earth | Loosen Ground across the flight line can trip a fleeing animal (doc 43's stumble). | Clay Hand for the mason's lead; Stone Sense finds the seam (crafts; doc 57's quarries). |
| Water | Wash Out strips a partner's scent before the stalk. | Draw Water for tanners and fields; Dowse finds the well's spot (doc 57). |
| Wind | Turn the Wind keeps the ambusher downwind. | **Winnow and Dry at threshing** (new: farms join its list); Bellows (crafts). |
| Sound | Hush silences the ambush; **Throw Voice drives** (new: a noise within an animal's flight distance makes it flee from that tile; further off, it watches it). | Ring True for the smith; Carry for a work crew's calls. |
| Blinker | Blink puts the ambusher across the flight line in one move. | Shortcut for the forager; Interpose for the guard (if a fight breaks out). |
| Gravity | Burden a charging boar. | **Lighten Load for the whole joint** (new: its members carry half again while it holds). |
| Seer | Read the Line shows whom the boar means to charge. | Weathereye at harvest (new: farm work as it does foraging); Danger Sense as the lookout. |

**Quickened wolves protect.** They have no work Gifts (doc 43), so their angle is **Keep watch**, a role open to any
wolf in a joint in the wild: it counts as a different role; a creeping bandit (doc 40's `tendCamp`) must get past the
watcher's notice as well as each worker's; and if a fight starts the watcher starts with a full bar and can't be
taken unawares. In a hunt, the Quickened wolf is the one who faces the bear the others raised.

**Keeping the secret** (§6.7, the setting's angle):

- A partner who sees Quickened magic (a hunter, joint member, party mate or fighter on its side) **no longer adds to
  `wardenAttention` by seeing it**. Foes and resident onlookers still do (`World::wardensSee` changes).
- Each such partner gets a private witness record: witness, wolf, day, place, IDs only.
- **Keeping the secret** is doing nothing. The other two choices are made at a resident of the Warden Order:
  - **Tell the Wardens:** +3 attention, once per witness and wolf *(placeholder)*, and a deed for doc 56. The Quickened
    wolf isn't told who told, though rumour may say.
  - **Vouch for them:** −1 attention, once a game month per witness *(placeholder)*; the voucher's own standing with
    the Wardens rides on it (doc 52's vouching shape).

### 5. Training grounds and sparring (§5.3)

- **Spar** is a fourth duel term, `spar` (agreed shape: "ends at yield, never Downed, no injuries past bruises"):
  - it ends at yield as `yield` does, and blows don't bleed or burn;
  - no lasting-injury rolls; an acute roll gives at most a minor bruise (`injury::bruise`, new);
  - a blade strikes blunted: half damage, no bleeding (the Training Stores' "blunted blades");
  - fighting on an unhealed injury still sets it back (doc 38's strain stays);
  - offered in Challenge's menu as "Spar (bruises only)". Auto-decline (doc 40) declines spars too.
- **Training grounds** are data, not new ground (`Data/Together/training.json`): the region `training_grounds`, and
  cells or rectangles by a barracks or guardhouse, added add-only through the world DB (doc 20). Nothing edits
  terrain.
- **A resident trainer** (a guard on duty at a training ground, or a trainer post there) offers **Ask to spar** when
  no player is free. It is not an assault, raises no crime, and the trainer fights at its temperament's skill.
- **A practice post** ("Practise at the post") gives practice with no partner, so practice is never blocked.
- **Watchers:** watching a spar doesn't lock a wolf out of the ring afterwards (the `observed` lock is skipped for
  spars). The spar's card says how many watch. Doc 54 builds festival contests on spars; doc 51's stars apply.
- **Practice** (doc 49): fighting skill grows by sparring, against a live partner ×1, a trainer ×0.6, a post ×0.3, and
  ×1.5 at a training ground *(placeholders)*. Until doc 49: `award(id, "practice", "spar:<day>")`.
- Gifted and Quickened wolves can practise openly here. Quickened magic in a spar is seen as any is (Wardens watch),
  so the secret-keeping rule above gives the ring its tension.

### 6. Player jobs, later (§6.9, agreed note)

When player jobs come, each is built on the joint activity: watch patrols walk in pairs (Keep watch and a walker),
caravans need crews, ferries a pole and a rope, the mine works in teams, the smithy is a smith and a striker, a healer
needs a carrier. Each job names its activity in `patterns.json`, pays from a real purse through the wage table, and is
better in company by `together::rate`.

### 7. Settings, safety and the DM

- Two settings, both on by default: **Hunting partners** and **Work partners** (saved flags, in Settings beside
  "Fights with players"). Doc 50 may move them into its own settings store.
- Block: `World::setBlocked(std::function<bool(a, b)>)`, wired by `Game` to doc 50's account-wide list when it exists
  (until then it returns false). A block hides Join hunt, Ask to join, Lend a paw and invites both ways.
- **The DM** (doc 34's LIVE): watch-frame flags 16 hunting, 32 working together, 64 sparring; `doing` reads "hunting
  with Bo" or "threshing at Hale's"; the Players layer narrows by them; the inspector shows the two settings and the
  current hunt or joint; Events show shared kills, farm pay and Warden reports. No DM controls are needed.

### 8. Wire messages

| Message | Direction | What |
|---|---|---|
| `{"type":"partners","kind":"hunt"\|"work","on":bool}` | to server | The two settings. |
| `self.noHuntPartners`, `self.noWorkPartners` | snapshot | Sent only when off. |
| `fights[].hunt`, `hunters`, `taken`, `canJoin`, `canAsk` | snapshot | The square outside a hunt; `canJoin` now asks the hunt rule. |
| `{"type":"hunt","verb":"ask","battle":id}` / `"invite","to":id` / `"letIn"\|"turnAway","who":id` / `"give","to":id` | to server | Asking, inviting, answering, handing over a share. |
| `battle.huntAsks`, `battle.nearby`, `battle.sharing` | snapshot (hunters) | Waiting asks; wolves near the square; who takes part. |
| `fighters[].animal.state`, `animal.watching` | snapshot | grazing, watching, fleeing, calming; whom it watches. |
| `fighters[].waiting` | snapshot (own side) | Lying in wait. |
| `odds.hit`, `odds.why` | snapshot | 1 − dodge, and "unaware", "driven", "watching you", "fleeing you". |
| `{"type":"work","verb":"lend","with":id,"role":r}` / `"start","kind":k,"at":id` / `"ask","to":id` / `"leave"` | to server | Lend a paw; start farm work; invite; leave. |
| `work` | snapshot | The joint one is in: kind, members and roles, rate, beats, pay so far. |
| menu actions `lend a paw`, `ask to lend a paw`, `help with the harvest`, `help with the threshing`, `lend a paw at the workshop`, `ask to spar`, `practise at the post` | snapshot | Per entity, as the menus are built now. |
| challenge `terms: "spar"`; `{"type":"action","action":"spar","target":id}` | to server | A spar with a player or a trainer. |
| `{"type":"wardens","verb":"tell"\|"vouch","about":id}` | to server | At a Warden; IDs only. |

## Phases

Hunting comes first: its rules are agreed. Every phase keeps `world_check --players 20` (doc 31) where it was, adds
nothing to the tick that scans the world, and is cheap when nobody hunts or works together.

**Files other sessions are editing:** the economy session has `RatwSociety.h`, `RatwOrchestrator.*`, `RatwDemand`,
`RatwOddJobs`, `RatwResidents` and `Data/Economy/` open, and shared hunks of `RatwCrafting.cpp` and `RatwHunt.cpp`
before. This plan touches `RatwSociety.h` (two declarations, Phases 4 and 7), `RatwCrafting.cpp` (one hunk, Phase 7)
and `RatwHunt.cpp` (hunting's own rules, Phases 1–2; the outwork's `takeFromPatch` and `huntPressure` stay as they
are). The combat rules (`RatwBattle.cpp`) get one-line hooks only, as doc 41 did; `RatwBattle.h` is left alone. Commit
only this plan's hunks.

### Phase 1: animals that freeze and flee, and one-bite kills

- **Goal:** the new hunt for one wolf: the four states, one-bite kills, the dodge, lying in wait, quality by what the
  animal saw.
- **Changes:**
  - `Data/Wild/animals.json`: `flight` and `dodge` per species; `Core/RatwWild.*` reads them.
  - `RatwWorld.h`: `HuntAnimal` gains `state`, `watching`, `from`, `calm`, `saw`; a `HuntHunter` map (last active turn,
    lying in wait), so `BattleFighter` doesn't change.
  - `Core/RatwHunt.cpp`: `World::animalTurn` rewritten; new `World::huntDodge`, `huntReach`, `huntStep` (springs),
    `huntAfterTurn` (flight checks and taking part); `huntBlow` (a landed bite or blade on a fleeing animal deals its
    whole health); `huntKill` (masterwork by `saw`).
  - `Core/RatwBattle.cpp`, one line each: `strikeChance` (`huntDodge`), `reachWith` (`huntReach`), `walkFighters`
    (`huntStep` after each step), `endTurn` (`huntAfterTurn`).
  - `Core/RatwGameBattle.cpp`: `animal.state`, `animal.watching`, `waiting`, `odds.why`. Client: `battle.ts`,
    `paint.ts` (marks, flight line), the cards' odds. Dev Console (`RatwGameDev.cpp`): `/hunt <species…>`.
  - `Tests/hunt_sim.cpp` (new; not a test, a CMake target like `level_sim`) and `Tests/hunt_play.h`: a lone stalker over
    many seeded hunts of fixed game time, printing kills, goods (catalog price, masterwork 3×) and the masterwork share
    per hunter-hour. Run on HEAD first for the baseline.
- **Tests** (`hunt_tests`): a far wolf seen makes an animal watch it; within flight distance it bolts directly away
  (bar full, a move longer than the fastest hunter's sprint); a hidden wolf to the side doesn't change its line; it
  calms; `huntDodge` row by row with the shifts and clamps; one bite kills a red deer, a boar keeps its health;
  masterwork unseen, fine when seen; a dodge makes it flee the biter; a wolf lying in wait springs on a driven animal;
  fire still caps quality. `battle_tests` `sneak::noticing` updated. `Client/src/game/battle.test.ts`.
- **Done when:** on a scratch DEV server (`/hunt roe_deer`) a deer watches a standing wolf, bolts when it comes close,
  and dies to one bite from a stalk; the sim prints a lone baseline beside HEAD's.
- **Cost:** per animal turn, a pass over a few hunters; per animal step, a check of those lying in wait. Hunts hold a
  dozen fighters at most. Nothing outside hunts changes.

### Phase 2: hunting together

- **Goal:** open hunts, asks and invites, equal shares, more game per hunter, helpful companions, roles named, and the
  agreed targets met in the sim.
- **Changes:**
  - `Entity::noHuntPartners` and `noWorkPartners` (saved, `RatwWire.cpp`); command `partners`; toggles in
    `dialogs.ts` and `state.ts`.
  - `Core/RatwHunt.cpp`: `huntJoinRefusal` (1.4); public `World::mayJoinHunt`; `askToJoinHunt`, `inviteToHunt`,
    `answerHuntAsk`; `World::setBlocked`; shares in `huntKill` (`World::shareOut`); `giveHuntShare`; bonds and role
    events in `endHunt`; more game in `tendHunts` and a one-line `joinBattle` hook (`huntJoined`).
  - `World::huntHelperTurn` for companions (one line in `npcTurn`), shared with the sim's ambusher.
  - `Game::fightsInView`: `hunt`, `hunters`, `taken`, `canJoin` by `mayJoinHunt`, `canAsk`; for hunters `huntAsks`,
    `nearby`, `sharing`. `Client/src/ui/hud/fight.ts`: Join hunt / Ask to join, the prompt, Invite, the end card.
    `RatwGameWatch.cpp`: flag 16 and `doing`.
  - `hunt_play.h`: driver and ambusher; `hunt_sim` suites `lone`, `pair`, `three`, `four`, `companion`, staying put
    and roaming between cells.
- **Tests:** `hunt_tests` `whoMayJoin` replaces `onlyFriendsJoin` (a stranger joins an open hunt; refused once any
  hunter turns it off; asks and invites; a blocked wolf refused and shown no button); `sharing` (exact split, nothing
  made or lost; idle past 10 turns gets nothing; a companion's share to its leader; Give my share); `moreGame`;
  `companionsWait`. `game_tests`: the square, the ask round trip, the setting saved. `wire_tests`: the flags.
  `tools/client/hunt.mjs` (new, scratch DEV): Bo joins Ash's hunt and both see the shares; with the setting off, Bo's
  ask is let in.
- **Done when:** `hunt_sim` shows each wolf of a pair taking home at least **1.5×** a lone hunter's goods in the same
  time, and three and four at least a pair's per wolf, roaming included (agreed target, §6.4). If not, tune in order:
  game per hunter, flight distance, the fleeing move, the 15% row, a spring from two tiles.
- **Cost:** join checks per viewer per hunt in sight; sharing per kill; bonds once a hunt. No new per-tick work.

### Phase 3: the joint activity, foraging together, Lend a paw

- **Goal:** the reusable mechanism (§6.2, §6.3) and its first use, with Allow work partners and work as a scene.
- **Changes:**
  - `Core/RatwTogether.h` / `.cpp` (new, added to `ratw_core` in `CMakeLists.txt`): `together::rate`, `JointWork`,
    `World::lendAPaw`, `askToLend`, `leaveWork`, `tendJoints` (once a second, over `joints_` only), `workLabour`;
    `shareOut` moves here.
  - `Data/Together/patterns.json` (new): the seven patterns and the `forage` activity.
  - `World::forage`: a member's picking gives `count × rate` to the joint; extra pickings per member (the outwork's
    `takeFromPatch` calls unchanged).
  - `Core/RatwSocialCore.*`: `workScene`, `workTag`; `Game::tendWorkScenes`. `RatwGame.cpp`: the `work` command, the
    menu actions, the `work` block. Client: a work row ("Foraging with Bo · ×1.8 · Leave"), the menu items, the toggle.
    `RatwGameWatch.cpp`: flag 32.
- **Tests:** `Tests/together_tests.cpp` (new, in CMake): `rate` for every case in 2.1; Lend a paw refused when off or
  blocked, invited anyway; members drop at 8 tiles and 60 s; two foragers each take 1.8× a lone forager's goods, the
  patch gives 5 then 6; exact shares; bonds; the work scene pays. `game_tests`: menus and the `work` block.
  `tools/client/together.mjs` (new): Bo lends Ash a paw.
- **Done when:** two players forage together in the browser and each takes about 1.8× what they would alone.
- **Cost:** `tendJoints` walks only live joints (dozens at 1,000 players). Menu checks are map lookups.

### Phase 4: farm work at harvest and threshing

- **Goal:** players work beside farmers and each other, paid by the farm, and the farm's yield grows.
- **Changes:**
  - `RatwSociety.h` (economy session's file; one declaration, agreed with them first): `Society::handBeat(producer,
    hand, rate, day, why)`, in `Core/RatwPlayerWork.cpp` (new): the season, the yield share up to `ProducerKept`,
    piece-rate pay from the farm's till by `transfer` (kind "farm work", the "hand" wage kind).
  - `patterns.json`: `harvest` (many paws, `most` 6), `threshing` (holder and worker, `most` 4). `RatwTogether.cpp`:
    beat-driven joints call `handBeat`; the farmer is a member while at its spot. `RatwGame.cpp`: the menu actions.
- **Tests:** `together_tests` `farmWork`: none in spring; autumn and winter; a beat's yield and pay; a full barn or an
  empty till stops it; money across all accounts unchanged (doc 15); pay and goods follow `together::rate` (with the
  farmer: 1.4 for one player, 2.0 each for two).
- **Done when:** a player threshes with Hale in the browser, paid from Hale's till, and Hale's grain rises; the economy
  session has read the hunk.
- **Cost:** one Society call per player per five real minutes of work.

### Phase 5: training grounds and sparring

- **Goal:** spars with bruises at worst; trainers and practice posts; watchers; fighting practice.
- **Changes:**
  - `Core/RatwBattle.cpp` (one-line hooks): `World::challenge` accepts `spar`; `hurtFighter` treats it as `yield`, with
    `World::sparBlow` skipping Bleeding and Burning. `Core/RatwInjury.cpp` `injureOnBlow`: no lasting roll, at most
    `injury::bruise`.
  - `Core/RatwTraining.cpp` (new): `World::trainingGround`, `sparWithTrainer`, `practiseAtPost`, `sparBlow`, blunted
    blades, no `observed` lock for spars, practice (doc 49, else `award`).
  - `Data/Together/training.json` (new): Greyfen's barracks yard and the Warden Training Grounds' three rings, add-only.
  - Client: "Spar (bruises only)", Ask to spar, Practise at the post, the watch count. `RatwGameWatch.cpp`: flag 64.
- **Tests:** `battle_tests` `spar`: a 40-damage blow leaves at most a bruise and no lasting injury; no bleeding; the
  yield at 100; a blade halved; a trainer spar raises no incident; watching then sparring allowed. `game_tests`: the
  menu only at a training ground. `tools/client/fight.mjs` gains a spar.
- **Done when:** two players spar at Greyfen's barracks yard and leave bruised at worst; a lone player spars the
  trainer.
- **Cost:** none outside fights; the training-ground test is a lookup by cell.

### Phase 6: Gifted and Quickened angles, and the rest of the patterns

- **Goal:** each Gifted family as a third angle; Keep watch; Throw Voice drives; talker and doer; the Wardens' secret.
- **Changes:**
  - `patterns.json`: the Gifts each activity takes; a `keep_watch` role in the wild.
  - `Core/RatwMagic.cpp` (the Gift session's file): Winnow and Dry lent to threshing; `useWorkGift` on a joint lifts
    it; Lighten Load over a joint; Weathereye at harvest; Throw Voice calls `huntNoise`; `wardensSee` skips partners
    and writes witness records.
  - Keep watch: a watcher's notice in `tendCamp`'s check (one line in `RatwRoads.cpp`); a full bar, no ambush.
  - `RatwGameVoice.cpp`: a resident in talk faces the wolf for 20 s after the last line (`World::face` for NPCs).
  - The `wardens` command at a Warden Order resident; witness records saved (IDs only).
- **Tests:** `magic_tests`: the lift at threshing; Throw Voice makes a rabbit flee the tile; partners add no attention,
  foes do; tell and vouch move it once. `together_tests`: Keep watch is a role. `roads_tests`: the watcher catches a
  creeping camp. `battle_tests` `sneak::inTheWorld`: a resident in talk faces the talker.
- **Done when:** a Gifted Wind wolf's lift shows on a threshing row; a partner steals from a stall while another talks
  to the keeper; a Quickened partner's magic adds nothing until someone tells.
- **Cost:** none per tick; a few witness records per Quickened Gift use.

### Phase 7: crafts for two

- **Goal:** lead and hand: first a player as hand at a resident's workshop, then player-led crafting.
- **Changes, part A (now):** a `RatwSociety.h` declaration and a `RatwCrafting.cpp` hunk (economy session's; agree
  first): `Society::lendPaw(maker, rate, untilDay)`, read where `craftNext_` is set and where inputs are used; pay per
  beat; the menu action; practice toward the trade and doc 26's apprenticeship.
- **Changes, part B (needs doc 35 Phase 5's Craft panel):** `RatwItems.cpp` reads `helper`; lead and hand in the panel;
  hired resident hands.
- **Tests:** `crafting_tests`: a lent paw brings the batch sooner and sometimes saves an input; pay conserved.
  `together_tests`: a `helper` recipe refuses without a hand; quality follows the lead, speed the hand.
- **Done when:** A: a player hands for a smith and the batch comes sooner. B: two players make steel.
- **Cost:** one lookup per batch.

## Built

### Phase 1 (2026-10-07): animals that freeze and flee, and one-bite kills

- **Data** (`Data/Wild/animals.json`, read by `Core/RatwWild.*`):
  - every species but the fierce ones has `flight`, half its `alert` (a rabbit 3.5, a roe deer 6, a red deer 7);
  - `dodge` adds points to its dodge (a hare +5, a badger and an elk −5);
  - `Species::flees()` is every temper but `fierce`, so "cornered" kinds run too, biting once only when they have
    nowhere further to go.
- **The four states** (`Core/RatwHunt.cpp`):
  - `HuntAnimal` gains `state`, `watching`, `from`, `calm` and `saw`, the wolves it has been alert to.
  - `World::huntReact` runs at the end of each hunter's turn (after doc 40's noticing) and at the start of the animal's
    own. A noticed wolf further off than the flight distance is watched. One within it makes the animal bolt
    (`huntBolt`), its bar filling at once. So does a missed bite (`huntMissed`), or any hurt short of death.
  - A fleeing animal calms after two of its turns without the wolf in its senses, takes one ordinary-paced turn away,
    then watches or grazes.
  - Only wolves it has noticed count, so a driven animal runs past a hidden partner.
  - `World::fleeingTurn` runs directly away from the wolf it flees: the tile furthest along that line, a little against
    straying sideways, off the edge to get away. Its run (`huntReach`) is 1.5 × the fastest hunter's sprint and costs it
    no stamina.
- **One bite** (`huntBlow`): a landed bite or blade (both land as `DownedBite`) kills any animal that runs, cleanly
  (the user: blades too). Fire and other Gifts' blows keep doc 41's health rule. Boars and bears are unchanged.
- **The dodge** (`World::huntDodge`, replacing `strikeChance` for game that runs):
  - 5% unaware, rising to 60% while it half notices the biter;
  - 15% driven past a wolf it never noticed;
  - 60% when it watches the biter;
  - 75% when it flees the biter.

  Then the species' points are added, 0.2 points come off per point of the biter's DEX and fighting skill over 50, and
  the result is clamped to 2%–90%. The battle view's odds carry it as `hit` with `why` ("95% · unaware", "85% ·
  driven"), from any side.
- **Quality** (`huntKill`): masterwork when the animal never saw the killer, fine otherwise; fire still caps it at common.
- **Lying in wait:**
  - A hunter that ends its turn crouched without biting lies in wait (`huntHunters_`, cleared when its turn comes
    again).
  - An animal stepping beside it (`huntStep`, after each step in `walkFighters`) gets a bite at once, out of turn
    ("Ada springs from hiding!"). A dodge stops the animal where it is.
  - Each hunter's last active turn is kept for Phase 2's sharing.
- **Combat hooks:** one line each in `RatwBattle.cpp`: `beginTurn`, `walkFighters`, `endTurn`, `reachWith`,
  `battleMove` (no stamina for a bolting animal), `strikeChance`, and the bite's and sword's misses.
- **The page:**
  - Over game: "?" while it half notices you, "!" watching, "»" fleeing, "~" calming.
  - A faint line runs from the wolf a watching animal watches, through it and on: where it will run.
  - The odds badge and card name the dodge.
  - One's own wolves lying in wait carry "⋯".
  - `animal.state`, `flight`, `watching`, `watchingYou`, the fighter's `waiting`, and `odds.why` are sent
    (`RatwGameBattle.cpp`).
- **Dev Console:** `/hunt <animal> [animal…]` starts a hunt where one stands with just that game. `World::startHunt`
  gains an optional list of species, which tests use too.
- **The sim** (`Tests/hunt_sim.cpp`, a CMake target, not a test; `Tests/hunt_play.h`): a lone stalker over seeded runs
  of fixed game time, for 40 runs of 30 minutes:

  | | kills / hunter-hour | goods | pennies of goods | masterwork |
  |---|---|---|---|---|
  | Before (doc 41's rules) | 1.85 | 15.8 | 63 | 8% |
  | Now | 1.05 | 12.1 | 56 | 9% |

  The new stalker crouches throughout, prefers unaware game and rushes a watching animal from close; the old run used
  a plainer stalker. A lone wolf now catches fewer, mostly by staying unseen, which leaves the room Phase 2's pairs need
  (the target is 1.5× a lone wolf's goods each).
- **Tests:**
  - `Tests/hunt_tests.cpp`, now 179 checks:
    - a far wolf it sees: it watches her;
    - within its flight distance: it bolts at once, runs further than her sprint, directly away;
    - a hidden partner beside its line doesn't turn it;
    - it calms, then grazes;
    - the dodge row by row, with the hare's points and the clamp;
    - driven past a wolf it never saw: 15%;
    - one bite kills a red deer, masterwork unseen, fine once seen;
    - a boar keeps its health;
    - a dodge sends it fleeing the biter;
    - lying in wait, she springs as it passes.
  - The ragged-kill test now uses boars, and the runner is startled by a blunt blow (a bite kills).
  - `Client/src/game/battle.test.ts` covers the state, the watcher, the dodge from any side, and the waiting mark.
- **Not done:** a look on a scratch DEV server (`/hunt roe_deer`). The Dev Console needs a character marked Dungeon
  Master, and the behaviour is exercised in the tests instead.

### Phase 2 (2026-10-07): hunting together

- **The settings:**
  - `Entity::noHuntPartners` and `noWorkPartners` are saved only when off (`RatwWire.cpp`).
  - The command is `{"type": "partners", "kind": "hunt" | "work", "on": bool}`, with Settings toggles ("Hunting
    partners: Anyone may join / Only those I invite").
  - The snapshot's `self` shows them.
- **Who may join** (`World::huntJoinRefusal`, `mayJoinHunt`):
  - never one blocked by a hunter (`World::setBlocked`, wired to doc 50's block);
  - always a hunter's companion, party or Chapter, or one let in or invited;
  - anyone else while the hunt's starter (`huntStarterOf`) allows hunting partners. Only the starter's setting counts
    (the user).
- **A closed hunt:**
  - `askToJoinHunt` holds the ask for 30 s; the hunters are told.
  - `answerHuntAsk` lets them in (they join at once) or says not now.
  - `inviteToHunt` lets in a wolf within 20 tiles (`huntNearby`).
  - The hunters' battle view has a `huntPanel` (asks, wolves near enough to invite), shown under the map with Let in,
    Not now and Invite.
- **Shares** (`huntKill`, `huntSharers`):
  - A kill is split equally among the hunters still in the hunt who did something in their last ten turns; the killer
    always shares.
  - Each item gives everyone the whole part, and the remainder goes one by one to sharers drawn by chance, so nothing is
    made or lost.
  - Masterwork keeps the killer's mark. A companion's share goes to its leader.
  - Each sharer is told "… shared among 2. Your share: …".
  - The snapshot's `huntShare` (for ten minutes) gives the end card's Give my share to … (`giveHuntShare`, one
    `Society::shift` each, kind "a hunt's share"); the one given it is the carrier.
- **Bonds and roles** (`endHunt`):
  - Pairs who shared a kill gain bond once a hunt (`Bonds::mutual` +1 affinity, +1 trust, +2 familiarity).
  - `huntRole` events name each part: tracker (their trail brought the game), driver (it fled them into a partner's
    jaws), ambusher (took one fleeing another, or sprang), carrier.
- **More game per hunter** (`tendHunts`, `huntJoined`):
  - Arrivals' chance grows by `perHunter` (0.75) a share per extra hunter, and how many may be in at once by
    `atOncePerHunter` (2).
  - A hunter joining brings in `joinerBrings` (0.5) of the expected count, rolled.
  - The numbers are now in `animals.json`'s `population`.
- **Companions** (`World::huntHelperTurn`, one line in `npcTurn`):
  - in a hunt a companion bites an animal beside it that hasn't noticed it;
  - otherwise it stalks, beside a stalking leader or round to the far side of the animal its leader goes at, and lies
    in wait, instead of charging.
- **The page:**
  - A hunt in sight reads "A dun wolf's hunt · 2 hunting · 1 taken", with Join hunt, or Ask to join when it is closed to
    one. There is never a button for the game's side (this fixes the bug noted under "Where we stand").
  - The DM's watch frame flags hunters (16) and says "hunting".
  - Joining says "joins the hunt".
- **The sim** (`Tests/hunt_sim.cpp`, `Tests/hunt_play.h`):
  - suites lone, pair, three, four and companion, each staying in one place and roaming between three;
  - the wolves trot (pace 7), as a player closing on game would; at a walk the lone stalker barely catches anything;
  - a pack member (`packTurn`) stalks unaware game, weighing game by its worth. An animal watching a packmate it hasn't
    noticed, it circles round and lies in wait for; one watching itself, it holds off until a packmate is round, then
    steps in to drive it.

  Measured, 12 runs of 30 game minutes, per wolf per hunter-hour, the placeholders as built:

  | Suite | Ground | Kills (all) | Goods | Pennies | Masterwork | Deer taken |
  |---|---|---|---|---|---|---|
  | Lone | stay | 10.7 | 27.5 | 118 | 22% | 0 |
  | Lone | roam | 17.7 | 43.7 | 210 | 39% | 0 (1 elk) |
  | Pair | stay | 15.0 | 23.2 | 112 | 41% | 4 |
  | Pair | roam | 18.8 | 26.8 | 124 | 43% | 2 |
  | Three | stay | 20.8 | 23.9 | 138 | 54% | 17 |
  | Three | roam | 20.3 | 22.7 | 127 | 54% | 13 |
  | Four | stay | 21.5 | 16.2 | 81 | 46% | 6 |
  | Four | roam | 22.7 | 21.5 | 128 | 63% | 19 |
  | With a companion | stay | 11.3 | 31.2 | 124 | 23% | 1 |
  | With a companion | roam | 15.7 | 46.7 | 194 | 20% | 4 |
- **The target isn't met yet.** The agreed target is each wolf of a pair at 1.5× a lone wolf's goods. Measured, a pair
  comes to about a lone wolf's goods each staying in one place, and less roaming.
  - Packs catch deer that lone wolves never do, and more masterwork.
  - A lone wolf at a trot already takes about ten small animals an hour unseen, so two wolves must bring in three times
    that between them.
  - Doubling game per hunter barely moved it (the hunters' pace is the limit, not the game).
  - A companion is worth about a little over a lone wolf's goods staying put (its share goes to its leader), and about
    the same roaming.
  - The plan's next levers are flight distance, the bolting run, the 15% driven dodge and a spring from two tiles. The
    user asked for hunting's balance to come later (2026-10-07), so they are left for that pass, with the sim ready for
    it.
- **Tests:**
  - `Tests/hunt_tests.cpp`, now 219 checks.
    - `whoMayJoin` replaces `onlyFriendsJoin`: a stranger joins an open hunt; another hunter's setting doesn't close it;
      the starter's does; one asks once and is let in; one is refused, then invited; a blocked wolf can neither join
      nor ask; never the game's side.
    - `sharing`: 8 raw meat split 4 and 4 between the two who took part, the one hide to one of them, nothing to the one
      who idled, Give my share moving it (not to one who didn't share).
    - `moreGameAndCompanions`: a joiner brings game in; a companion lies in wait beside its stalking leader.
  - `Tests/game_tests.cpp` `huntsTogether`: the hunt square seen by a stranger, the setting kept and shown, the ask
    round trip, the invitation.
  - `Tests/wire_tests.cpp` `partnersSaved`.
  - `Client/src/game/battle.test.ts`: the hunt square.
  - `tools/client/hunt.mjs` (its own wild world): Bo joins Ash's hunt from the square; Ash closes it; Cy asks; Ash lets
    her in. Screenshots in `artifacts/screenshots/hunt/`.

### Phase 3 (2026-10-07): the joint activity, foraging together, Lend a paw

- **The mechanism** (`Core/RatwTogether.h` / `.cpp`, new, in `ratw_core`):
  - `together::rate` as in 2.1 (players first, a step halved while all so far share one role, halved for a resident
    hand); `together::split` shares a count exactly, the remainder one by one in an order drawn by chance.
  - `JointWork` kept by World (`joints_`, `jointOf_`), never saved. `World::lendAPaw` makes or joins the joint, the
    free role first (a role may be named); `askToLend`, `leaveWork`, `dropFromJoint`, `endJoint`, `workRate`,
    `workLabour`, `atWork` (in a joint, or foraged within the last 60 s).
  - `tendJoints` runs once a second over live joints only. A member has left when it is 8 tiles off from **all the
    others** (not from where the work began: foraging moves patch to patch, and a fixed spot ended every pair's joint
    at the next patch), idle 60 s, down, in a fight, or elsewhere. A joint with fewer than two, or no player, ends.
  - Ending: each pair who worked 2 beats together gains bond (+1 affinity, +0.5 trust, +2 familiarity; a resident
    +2/+2) and a `together` ledger event of IDs.
  - `Data/Together/patterns.json` (new): the steps, distances, bond, the seven patterns and the `forage` activity
    (roles "digs" / "carries and sorts", with "dig" / "carry and sort" for oneself; most 4; beat 4 s).
  - `shareOut` stayed where it was: the hunt's shares are per item and per sharer's activity, foraging's per picking;
    they share only `split`'s idea, written once in each.
- **Foraging together** (`World::forage`): a member's picking gives `count × rate` (the fraction by chance), split
  exactly among the members, each told their share ("Together you gather 2 cooking herbs from the grass; your share is
  1."). The patch gives one more picking for each extra member, up to two (`takeFromPatch(…, extra)`; the outwork's calls
  unchanged). Each picking is a beat; a beat counts toward the bond with every member who worked in the last minute.
  Practice is foraging's own (`forage.pick`, each picking).
- **Lend a paw** (`World::mayLend`): within 6 tiles of a player at work, not blocked either way, and the worker's
  Allow work partners on (`Entity::noWorkPartners`, Phase 2's setting and toggle) or the worker asked. In the menu:
  "Lend a paw", or "Ask to lend a paw" of a player near who isn't at work. The command is
  `{"type": "work", "verb": "lend" | "ask" | "leave", "with" | "to": id}`.
- **A scene**: `SocialLedger::workScene` / `workTag` / `isWork` / `joinWork` / `settleWork` (fights and work now share
  `settleScene`). `Game::tendWorkScenes` keeps every joint's members in its scene (open, as any scene) and settles it
  when the work ends. A wolf's words while in a joint carry the work's tag (unless it is fighting), so talking it
  through is paid as a fight's talkers are; working in silence pays nothing.
- **The page**: the snapshot's `self.work` (kind, name, rate, members with their role words and beats); a row under the
  map, "Foraging with A dun wolf · ×1.8 · you dig · A dun wolf carries and sorts · Leave". The DM's watch frame flags
  joint workers (32) and says "Foraging together".
- **Measured** (`together_tests`): 40 rounds of two pickings each, moving to a fresh patch every round: alone 80 goods;
  a pair 144 and 142, each 1.79× a lone forager. A patch gives 4 pickings alone, 5 for two, 6 for three.
- **Tests:**
  - `Tests/together_tests.cpp` (new, 39 checks): `rate` in every case of 2.1, six wolves, and the activity's most;
    `split`; Lend a paw refused when not at work, when the worker's setting is off (until asked) and when blocked; the
    free role; going off ends the joint; idle a minute leaves; each of a pair at about 1.8×, shared evenly; 4, 5 and 6
    pickings a patch; the bond and the `together` event.
  - `Tests/game_tests.cpp` `worksTogether`: Lend a paw in Bo's menu only once Ash forages, Ask to lend a paw in hers;
    the work block (1.8, dig / carries and sorts) for both; a picking shared; Cy asked and joining (2.2); their words in
    the work's scene; both leaving ends it.
  - `Tests/social_game_tests.cpp` `workScenes`: the scene open with its members from the start; talkers paid 20 each,
    the silent worker nothing, never twice.
  - `tools/client/together.mjs` (new, its own wild world): Ash forages; Bo opens her menu and lends a paw; both see the
    row; a picking is shared; Bo leaves. Screenshots in `artifacts/screenshots/together/`.
- **Noticed, then fixed in Phase 4 (names always start with a capital):** a notice naming another wolf ("Bo lends you a paw", "Bo asks to join your
  hunt", "Your share of what Bo gathers") is veiled for a reader who doesn't know the name only where the name starts
  with a capital (`names::veil`). Character names may start lower case (`accounts::validDisplayName`), so a wolf
  named "bo" is shown by name to strangers in every such notice, not only this plan's. Capitalising names at creation,
  or notices carrying IDs that are labelled for each reader, would close it.

### Phase 4 (2026-10-07): farm work at harvest and threshing

- **When and where** (`World::residentWorkAt`): a resident farmer at work at its post (its task is its post's title, in
  its hours, and it stands within 1.5 tiles of its spot), whose producer (`Data/Items/crafts.json`) an activity in
  season names:
  - the harvest in autumn at farms, orchards and vineyards;
  - threshing in winter at farms only (an orchard's or vineyard's winter firewood isn't threshing).
  - In its menu, within 6 tiles: **Help with the harvest** / **Help with the threshing**. The command is also
    `{"type": "work", "verb": "start", "at": id}`.
- **The joint** (`World::helpAtWork`):
  - The farmer is a resident member in the second role, so a player alone works at ×1.4.
  - Other players join by Lend a paw on a player in it, or by helping the farmer too. The free role is counted among
    players only, so two players take both angles beside the farmer's and work at ×2.0 each.
  - Refused when the farm's till can't pay two spells' wages ("Hale can't pay for more hands today."), or its barn holds
    `ProducerKept` of everything the work brings in ("The barn is full; …").
- **The beat** (`World::workBeat`, from `tendJoints`): a spell, 300 s (30 game minutes). For each player who did
  anything in the spell (`World::noteActive`, called on every command, so talking while working counts):
  - **Goods:** its rate × a third of a spell's yield (the producer's `out`, or `offSeason` for threshing), the fractions
    by chance, into the farm's till up to `ProducerKept` ("brought in by a hand").
  - **Pay:** the town's hand wage a spell (`dayWage(town, "hand") / PaidSpells`) × its rate, from the farm's till by
    `Society::shift` (kind "farm work"), fractions of a penny carried in the member.
  - A spell it came part of the way through counts in part. Without that, a player who joined two seconds after a spell
    began was paid nothing for the whole five minutes.
  - It is told: "A spell's threshing beside Hale: 1 wheat (measure), 1 firewood (bundle) brought in; you are paid 3p
    (3p so far)." The ledger gets a `farm work` event.
  - Beats with the farmer and each other count toward the bond at the end (a resident +2 affinity, +2 trust).
- **Leaving:** a player idle for a whole spell has left. The joint ends when the farmer stops (its hours end, the
  weather, Restday), when the till can't pay, or when the barn fills ("Hale stops work, and so do you.").
- **No hunk in the economy session's files.** Everything needed is public on `Society` (`spec`, `jobOf`, `resident`,
  `tillOf`, `account`, `stock`, `create`, `shift`, `spendable`, `dayWage`), so `Society::handBeat` wasn't added and
  `RatwSociety.h` is untouched. The pay from a till is seen by the orchestrator through `shift`'s
  `noteForOrchestra`, as a till's spending.
- **The page:** the work block adds `farmer`, `earned` and `nextBeat`; the row reads "Threshing with Hale · ×1.4 ·
  3p earned · next spell in 4:55". The DM's watch says "Threshing at Hale's". A dev calendar step `{"type":
  "calendar", "value": "season"}` goes to the next season's first day (`RatwWire.cpp`).
- **Measured** (`together_tests`, against the same world without the hand): four spells of threshing beside Hale paid
  8p (2.1p a spell at ×1.4, carried) from his till, and put 7 more goods in his barn than he brought in alone. With Bo
  too, at ×2.0, each was paid 3p a spell.
- **Tests:**
  - `Tests/together_tests.cpp` `farmWork` (now 62 checks): nothing in spring, the harvest in autumn, threshing in
    winter; ×1.4 alone, ×2.0 each with Bo; four spells' pay exact, from the till; money across all accounts moving
    exactly as in the world without the hand (doc 15); more in the barn; Bo's part spell; idle a spell leaves; a full
    barn ends the work and refuses a new start; an empty till refuses; not from afar.
  - `tools/client/farm.mjs` (new; its own field with Hale; `--speed 10`, so a spell is half a minute): winter at
    midday by the dev calendar and clock; Ash helps with the threshing from Hale's menu; the row at ×1.4; Bo lends a paw
    at ×2.0; a spell later Ash is paid and the row says so. Screenshots in `artifacts/screenshots/farm/`.
- **Also this phase:** names always start with a capital (the user, 2026-10-07): at creation, for dev identities, on
  entering the world (older characters too), and as the player types. So a name is veiled wherever it is written
  (`names::veil` looks for capitalised names). Tested in `newcomer_tests`.

### Phase 5 (2026-10-08): training grounds and sparring

- **Spar, a duel's fourth term** (`challenge(…, "spar")`; "as a spar (bruises only)"):
  - It ends at yield as `yield` does.
  - Nothing bleeds: the bleed roll skips spars, and at each turn's start a spar puts out burning and stops bleeding.
  - A blade strikes blunted: `World::sparBlow` halves a sword's blow (`bladeBlunted` 0.5).
  - `injureOnBlow` in a spar gives at most one minor bruise (`bruised_ribs`, severity 1, through `injury::given`; not a
    second, which `addAcute` would make moderate) and never a lasting mark. `injury::bruise` wasn't needed.
  - Doc 38's strain from fighting on an unhealed injury stays. Auto-decline declines spars.
- **Watching:** a spar can't be joined, only watched ("A spar is between those who agreed to it; you may watch.").
  So the `observed` lock never matters for it. The fight square says "Ad v Bo · a spar · round 3 · 2 watching"
  (`spar`, `watchers`).
- **Training grounds** (`Data/Together/training.json`, `World::trainingGround`): a cell whose region is
  `training_grounds`, whose id is `barracks` (Greyfen's), or whose id holds "barracks", "training", "drill_yard" or
  "guardhouse". Nothing edits terrain. There:
  - **A resident trainer** (`World::trainer`: a guard on duty there, or one whose post's title trains or drills)
    offers **Ask to spar** (`sparWithTrainer`). `startBattle` takes the terms, so a spar is never an assault: no
    incident, no lost liking, no guard comes in.
  - **Practise at the post** (an ACTIONS button shown where `self.trainingGround`; the `post` command): practice
    `spar.post` (new in `skills.json`, +0.2 fighting) × 0.3 × 1.5, once every 20 s.
- **Practice:** `growSkill` weighs a spar's blows by `sparPractice`: × 1.5 on a training ground. A trainer teaches less
  than a player only by the practice engine's own weight for a resident partner. The plan's extra × 0.6 was built, then
  taken out: the user (2026-10-08) didn't want the two to stack.
- **The DM:** watch flag 64, doing "sparring".
- **Tests:**
  - `battle_tests` `sparring`: forty 40-damage injury rolls leave one minor bruise and nothing lasting, where the same
    rolls in a duel leave a lasting mark; no bleeding; a blade halved; nothing burns at the turn's start; a watcher
    can't join; yield at 100 on his feet; auto-decline declines a spar.
  - `together_tests` `training` (now 76 checks): a cell named for training trains; Tam is a trainer there and not
    elsewhere; a trainer spar has no incident, no lost liking, nobody joins; the post, its wait, fighting growing.
  - `tools/client/spar.mjs` (new; its own training yard with Tam): Practise at the post shows; Ash challenges Bo,
    choosing Spar (bruises only); Bo is asked and accepts; the fight reads "A duel as a spar (bruises only)"; Cy works
    at the post, then asks Tam to spar, with no assault. Screenshots in `artifacts/screenshots/spar/`.
  - Greyfen's barracks itself wasn't tried in the browser; the yard stands in for it.

### Phase 6 (2026-10-08): Gifted and Quickened angles, and the rest of the patterns

- **Work Gifts on a joint** (`World::jointTakesGift`, `giftOnJoint`; one hook in `useWorkGift`, after its checks):
  - A Gift the joint's activity lists (`patterns.json` `gifts`; so far Winnow and Dry at threshing) spends its mana
    and wait as ever. It then holds to the end of the beat (a minute for work without beats).
  - While it holds, the member is on an angle of its own (`gift:<ability>` to the scaling) and every member works
    0.2 faster (`together::rateOf`; `GiftLift`). Ash threshing beside Hale: ×1.4, winnowing ×1.6; with Bo too, ×2.2.
  - The row names the Gift ("you hold the flail · Winnow and Dry").
  - **Lighten Load** now lightens every player in the caster's joint.
  - **Weathereye** brings in a quarter more at the harvest, as it does at foraging (no new Gift use: it is passive).
  - The user (2026-10-08) asked for the other work Gifts to help the group too. Each activity's `gifts`:
    - the harvest: Draw Water and Carry;
    - threshing: Winnow and Dry and Carry;
    - foraging: Shortcut (in a joint it lifts the foraging instead of blinking);
    - a maker's bench: Forge Heat, Kindle, Clay Hand, Bellows, Ring True, Settle, Draw Water, Winnow and Dry. A Gift
      that helps the maker's trade (doc 43's lend table) also lifts its next batch's quality, as lent.
  - Stone Sense, Dowse and Echo have no group work yet (doc 57's quarries, wells and mines). Mend was left alone.
- **Throw Voice drives** (`World::huntNoise`, one line in its fight effect): in a hunt, game within its flight
  distance of the noise's tile bolts away from that tile (`HuntAnimal::noiseX/noiseY`; the fleeing move runs from the
  tile when there is no wolf to run from), and calms two turns later. Game further off within the Gift's reach
  watches that way.
- **Keep watch** (`World::keepWatch`, `watchersOver`; the work verb `watch`; a Keep watch / Back to work button on a
  foraging row):
  - A role any wolf may take in a joint in the wild, on an angle of its own: two diggers ×1.4, a digger and a watcher
    ×1.8. A watcher is never dropped for being idle.
  - `tendCamp`: the bandits' creep must get past each watcher's notice as well as the traveller's, both before they
    creep and while they creep. Noticed, they step out and ask, or rise and rush, instead of springing an ambush.
  - `startBattle`: the watcher itself is never taken unawares. Those it watches over are protected by its notice, not
    outright.
- **Talker and doer** (`World::faceTalker`, `tendTalkers`; called from `Game::talk`): a resident a wolf talks to turns
  to it and keeps facing it, standing, until 20 s after the last line. Doc 40's vision cone does the rest: a partner
  behind the resident is out of its sight when stealing (`World::steal`'s watchfulness). Theft is still a pickpocket's
  from the resident's own purse; there is no stall theft yet.
- **Keeping the secret** (`World::wardensSee`):
  - A partner who sees Quickened magic adds no attention: a fighter on its side, a member of its joint, or a party
    mate (`World::setPartnered`, wired to doc 32's parties). Foes, bodiless watchers and resident onlookers still count.
  - Each partner keeps a witness record on its own character: `Entity::witnessed`, the wolf to the day. Saved, IDs only,
    with `toldWardens` and `vouchedDay`.
  - **At a Warden of the Order** (a resident whose faction is `warden_order`, within 3 tiles), a wolf with records
    sees **Tell the Wardens** and **Vouch to the Wardens** in its menu, then a list of the wolves it saw (the snapshot's
    `self.witnessed`, by its own names for them). The command is `{"type": "wardens", "verb": "tell" | "vouch",
    "about": id, "at": warden}`.
    - Tell: +3 attention, once a witness and wolf, and a `told the wardens` ledger event. The wolf isn't told who told.
    - Vouch: −1, once a game month (28 days) a witness, and a `vouched to the wardens` event.
    - **Vouching's risk** (the user, 2026-10-08; doc 52's shape): the voucher's word rides on it for the month
      (`Entity::vouchedFor`, `wardenStanding`, saved).
      - If that wolf draws the Wardens' attention again in that time, by being seen or by someone telling,
        `World::attentionRose` lowers the voucher's standing by 1, once a vouch, and tells it so.
      - At −2 the Wardens no longer take its word.
- **Tests:**
  - `together_tests` `anglesOfTheGifts` (now 96 checks): Winnow and Dry lifts threshing 1.4 → 1.6 for the spell, then
    back; Keep watch is an angle (1.4 → 1.8), watching over Ash, never idle; a resident faces the talker, and is free
    20 s later.
  - `magic_tests` `keepingTheSecret`: a partner adds no attention where a foe does, and keeps the record; tell +3 once;
    vouch −1, not again that month; neither without a record; another's telling lowers the voucher's standing once, and
    she is told; at −2 no more vouching; all saved.
  - `together_tests` also: Carry lifts threshing; Winnow and Dry at a bakery's bench lifts the joint and the batch.
  - `hunt_tests` `throwVoiceDrives`: a noise three tiles from a deer ten from Ada makes it bolt, away from the noise.
  - `roads_tests` `aWatcherCatchesTheCreep`: Ada forages with her back to a camp while Bo keeps watch facing it, and
    she isn't taken unawares. Beside her but not keeping watch, she is.
  - `tools/client/farm.mjs` gains the lift: Ash, made Gifted (Wind) by the dev command, winnows; the row reads ×2.2
    and names Winnow and Dry. Screenshot `2a-winnow-and-dry.png`.
  - Not tried in the browser: the Wardens' menu (no Warden in a test world yet) and a theft behind a talking keeper.

### Phase 7, part A (2026-10-08): a player's paw at a resident's bench

- **Lend a paw at the workshop:** a resident maker at work at its post, whose business crafts (any season), offers
  **Lend a paw at the workshop**.
  - It is the same joint as farm work (`at: "workshop"` in `patterns.json`; `World::residentWorkAt`, `helpAtWork` and
    `workBeat`, renamed from the farm names since they now serve both).
  - The maker leads ("works the craft") and players take the other role ("hold and fetch"): a player alone works at
    ×1.4.
  - Each spell, a player there is paid the hand wage × the rate from the shop's till (kind "a hand at the bench"), and
    practises `apprentice.craft` beside the maker as teacher (a spell's worth, 60 units). A farm hand likewise
    practises `apprentice.labour` beside the farmer.
- **The economy hunk** (the economy session's files; one declaration and two small hunks):
  - `Society::lendPaw(maker, rate, untilDay)`, kept in `pawLent_`, not saved. Lent when a player sets to work and again
    each spell, for a spell and a half.
  - `Society::craft` divides a batch's time by the rate while it holds.
  - At a batch's end, one batch in four doesn't use up one of its first material.
- **Measured** (`crafting_tests` `aPawLent`): an hour at the bakery, 29 batches alone (29 flour), 59 with a paw at ×2.0
  (44 flour). Money stays conserved throughout.
- **Tests:**
  - `crafting_tests` `aPawLent`: batches at least 1.5 times as many, and fewer inputs used than batches.
  - `together_tests` `benchWork`: the baker offers the workshop; Ash hands while Tam leads; ×1.4; two spells paid
    exactly what the ledger's `bench work` events say.
  - `tools/client/bench.mjs` (new; its own bakery with Bea; `--speed 10`): Lend a paw at the workshop from Bea's menu;
    the row "Work at the bench with … · ×1.4"; a spell later Ash is paid. Screenshots in `artifacts/screenshots/bench/`.
- **Not done, part B:** player-led crafting, `helper` recipes refusing without a hand, quality by the lead and speed by
  the hand, and hired resident hands. There is no player Craft panel yet (doc 35 Phase 5's second part), and the
  `helper` recipes are in `recipes.json`, the player catalog the residents don't use.

## Depends on and feeds

- **Depends on:** docs 41 and 40 (built); doc 50 for block and the settings panel (soft: hooks until then); doc 49 for
  practice (soft: typed awards until then); doc 35 Phase 5 for player-led crafting (Phase 7 part B only); doc 46's wage
  table (built).
- **Feeds:** doc 51 (hunts and joints are scenes; roles, shares and spars for end screens and stars); doc 52 (open
  hunts and joints are things a matchmaker can point to); doc 54 (hunting contests and sparring tourneys at festivals);
  doc 56 (events for deeds: a bear taken by four, a harvest brought in); doc 57 (`together::rate` and
  `World::workLabour` for town projects' labour); doc 58 (two-angle quests can use the joint activity).

## Risks

1. **The sim measures its own policies.** If the scripted driver and ambusher are poor, pairs look worse than players
   will make them. The policies are the companions' code, and the sim reports a naive and a careful pair.
2. **Lone hunting may get harder.** A fleeing animal is gone, and a rush wins 40%. Phase 1 compares the lone haul with
   HEAD's so the user can see it before Phase 2.
3. **The spring is a new out-of-turn action** inside `walkFighters`'s loop. A bite there can kill the walker, end the
   hunt or end turns mid-loop and leave stale references. Run the clang sanitizer build (`build-core-clang-sanitize`)
   on `hunt_tests` and `battle_tests` each phase.
4. **More goods from the land.** Shared hunts, foraging and farm work bring in more. Pressure, patches, `ProducerKept`
   and the shops' stores cap it, but the economy session should see the volumes (econ_watch).
5. **Open hunts invite spoilers:** a stranger can join to scare game off. Settings default on (agreed), so the answer is
   block (doc 50), turning the setting off, and leaving. The DM sees who hunted with whom.
6. **Pressure per cell punishes groups** that stay put. The sim's roaming suite shows the real figure.
7. **The Wardens aren't built.** Reports change a number with no consequence yet (doc 43).

## Decisions

**Agreed (doc 48):**

1. Anyone may join a hunt like a fight, gated by Allow hunting partners (per player, default on); a hunter with it off
   can still invite (§6.4, Decision 5).
2. Animals freeze at a distance and flee directly away, faster than any wolf, from a noticed wolf that comes close; only
   noticed wolves count (§6.4, Decision 33).
3. One landed bite kills any animal that flees, red deer and elk included; dodge 5/15/60/75% by what it knew, shifted a
   little by species, DEX and skill; a dodge sends it fleeing. Boars and bears keep their health (Decision 37).
4. Masterwork when the animal never saw the biter, fine otherwise; kills shared equally among those taking part (§6.4).
5. The target: each wolf of a pair at least 1.5× a lone hunter's haul in the same time; three or four as well per wolf.
6. Roles emerge: tracker, driver, ambusher, carrier (§6.4).
7. Cooperation ×1, ×1.8 / ×1.4, +0.4, +0.25, a resident hand ×1.4, with practice, bond and a scene (§6.3).
8. Allow work partners, default on; foraging with a partner; farm work paid by the farmer from doc 42's wages (§6.5).
9. Crafts from two angles; Gifted and Quickened fitted in naturally; training grounds; player jobs done in company
   (Decisions 4, 6, 13).

**New placeholder choices in this plan:**

10. ×1.8 is each wolf's rate, not the team's (Design 2.1). Kept by the user, 2026-10-07, to be balanced later.
11. Animals react at the end of a hunter's turn, not at each step, so a rush is possible; a bolting animal's bar fills.
12. Lying in wait: a hunter ending its turn crouched without biting springs on an animal stepping beside it.
13. A hunt is open to strangers while its starter has the setting on; only the starter's setting counts (the user,
    2026-10-07).
14. More game per hunter: arrivals × (1 + 0.75 per extra), `atOnce` + 2, a joiner brings half the expected count.
15. A blade's landed blow on a fleeing animal kills too (the user, 2026-10-07); Gift blows keep doc 41's health rule.
16. A companion's share goes to its leader; companions in hunts lie in wait instead of charging.
17. Every joint member contributes its rate whatever its role; outputs are shared exactly.
18. Farm work at harvest (autumn) and threshing (winter) only, piece-rate pay from the farm's till.
19. Spar is a fourth duel term: yield, no bleeding or burning, bruises at most, blunted blades.
20. Partners who see Quickened magic keep the secret by default; telling or vouching is a choice.
21. A Gifted member's work Gift lifts every member's rate by 0.2 for a beat.

## Open questions

None: all three answered by the user on 2026-10-07 (decisions 10, 13 and 15). ×1.8 is each wolf's rate, to be
balanced later; only the starter's Allow hunting partners opens or closes a hunt; a landed blade kills a fleeing animal
as a bite does.
