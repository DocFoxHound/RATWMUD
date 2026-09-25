# Deterministic resident life and a finite living economy

September 21, 2026 first-version design. NPCs should have reasons to act, require
real resources, and leave observable consequences. Dialogue generation remains
an expression layer; it does not choose daily actions, mint money, invent stock,
or authorize transactions. This is a small authoritative settlement simulation,
not yet a production-scale economy or a general-purpose artificial-life engine.

## Existing systems and the recommended fit

Unreal already provides relevant building blocks. StateTree organizes behavior
as hierarchical states and transitions, and supports selecting among possible
states. Smart Objects provide reservable activity opportunities such as beds
or workstations. They complement rather than replace economic rules: neither
defines what bread costs or whether a merchant can afford another purchase.
[Epic StateTree](https://dev.epicgames.com/documentation/en-us/unreal-engine/state-tree-in-unreal-engine),
[Epic Smart Objects](https://dev.epicgames.com/documentation/unreal-engine/smart-objects-in-unreal-engine---overview)

Behavior Trees are another built-in option, with event-driven branching and
Blackboard state. They remain useful for reactive behavior such as interrupting
work to flee danger. [Epic Behavior Tree overview](https://dev.epicgames.com/documentation/unreal-engine/behavior-tree-in-unreal-engine---overview)

GOAP instead searches a sequence of actions using explicit preconditions,
effects, and costs: “need food” might lead to earning money, buying ingredients,
cooking, then eating. Existing portable implementations include Abraham
Stolk's GPGOAP in C, published under Apache-2.0. It is an implementation candidate
to inspect and benchmark, not a dependency installed by this change.
[GPGOAP's author-maintained repository](https://github.com/stolk/GPGOAP)

Recommendation: keep money, inventories, needs, reservations, and action effects
in the engine-independent server core. Use a small deterministic priority
selector first, then consider StateTree as the authoring/execution adapter when
designers need visual tooling. Introduce bounded GOAP only when real alternate
procurement chains justify planning search. A C++ loop with clear state is more
honest than describing this first version as an installed StateTree or GOAP
implementation; neither is wired into runtime here.

## First settlement slice

The demonstration uses the existing tavern, exterior, and loft. Six residents
have accounts and persistent needs: a keeper, a cook, a forager, and three
residents who undertake paid service work. Each has a current task, reason,
progress, target position, hunger, and fatigue. These are inspectable simulation
facts rather than generated biography.

At a one-second decision cadence the planner chooses, in order, a necessary
meal, rest, food procurement, role work, or social time. Sleeping has hysteresis
so a tired resident does not repeatedly switch between walking and sleeping.
The current rest schedule is 22:00–06:00; it is an authored lifestyle schedule,
not another definition of astronomical night. Ordinary work is suspended while
an NPC is recruited into a player's party. Needs remain bounded and persistent.

The world supplies each actor's physical location and moves it toward its
goal using authoritative navigation. Work requires arrival at the correct cell
and within station reach; goods do not teleport between distant counterparties.
Production and consumption complete after action time. The planner must recheck
cash, stock, target presence, and capacity when an action actually commits.
Malformed bodies are ignored rather than introducing invalid goal coordinates.

| Activity | First-version effect |
| --- | --- |
| Gather | After 30 seconds at the patch, consume one patch bundle and gain one herb |
| Cook | After 45 seconds at the bench, consume two herbs and produce one meal |
| Eat | After 8 seconds, consume one carried meal and reduce hunger |
| Sleep | Recover fatigue only while at the assigned resting location |
| Deliver ingredients | Forager sells actual herbs to a physically present cook |
| Deliver meals | Cook sells actual meals to a physically present keeper |
| Buy food | Resident pays the keeper for an available meal |
| Buy ingredients | Cook can obtain herbs that players sold to the keeper |
| Paid work | Resident completes a 1,200-second service contract at its worksite |

Food acquisition is availability-aware: hungry residents continue productive
work when an empty shop has nothing to sell. The keeper does not attempt to buy
its own nonexistent stock. The cook explicitly meets a supplying forager in the
kitchen, and a partly filled foraging load can be delivered when ingredients
are urgently needed or the patch is exhausted. Deliveries scale down to the
buyer's actual purchasing power instead of requiring a full unaffordable batch.
An eating action retains its position anchor through small crowd bumps so
normal collision resolution cannot continually restart its timer.

Beds and workbenches use deterministic reservations. Shared merchant interaction
points must not become permanent single-owner locks: a broke cook waiting for
ingredients must not prevent another resident buying food. Stable actor ordering
and transaction revalidation resolve simultaneous requests without duplicate
spending. General fair queues, authored station catalogs, threat interruption,
relationship-driven social visits, and multi-step dynamic planning are future
extensions, not claimed features of this first selector.

## Real stock, limited demand, and real purses

Currency uses integer silver pennies. Every account has a finite balance and
item counts. Current goods are cooking herbs and prepared meals. Unknown or
irrelevant goods are refused. The keeper will also refuse otherwise useful
goods when its demand cap is reached, stock is unavailable, or cash is short.
Players cannot send their own accepted price; the authority quotes and validates
the trade again before changing either account.

The initial keeper caps buying demand at 20 herbs and 24 meals. Base retail is
2 pennies per herb and 6 per meal; low stock increases the quote and abundant
stock discounts it. Buying from a player pays a smaller fraction than retail.
Price spread plus finite demand prevents an immediate buy/sell profit loop;
quotes are not promises to purchase unlimited material forever.

The initial supply chain is intentionally small:

```text
seasonal herb patch → forager → cook → keeper → residents / players
players → keeper's herb inventory → cook
```

Forager deliveries currently pay 1 penny per herb. Keeper-to-cook ingredients
cost 2 per herb, cook-to-keeper meals cost 5, and ordinary resident food costs 6.
The cook can therefore process purchased herbs at a small positive margin.
Player depletion, player sales, absent workers, unaffordable inputs, and
consumption all change what is actually available. There is no free restock
timer attached to a shop's display.

If the keeper is out of meals and cannot finance new inventory, a hungry NPC
customer may buy a cook-owned meal through the counter on consignment. Customer,
cook and keeper must all be physically together; the customer pays the same six
pennies, split five to the cook and one to the keeper. The meal moves from the
cook's inventory, not an invented shelf count. Both ledger entries and all
balance/capacity checks form one authoritative transaction. This is an NPC
logistics fallback, not player credit or a new player-facing offer: the player's
merchant menu continues to quote only actual keeper-owned stock.

Generic residents can earn 2 pennies per completed service contract, up to
three contracts per game day, during 08:00–18:00. Payment comes from the keeper's
existing account, never from an invisible wage faucet. This lets residents
recirculate money into food purchases without granting them infinite cash.
Employer insolvency remains possible and should be visible as a blocked reason.

## Separate transfers, sources, sinks, and transformations

A sale between residents does not add money to the world. Cooking transforms
items but does not add money. Eating removes an item but is not a currency sink.
This distinction is essential when diagnosing scarcity or inflation.

The MUD Starmourn is a relevant precedent: its published economy describes
currency faucets/sinks and production arrangements that pay other participants
for inputs. It demonstrates that a text-led world can support meaningful supply
chains. Our policy below is a RATW design choice, not a copy of its balance.
[Starmourn economy design](https://www.starmourn.com/economy/)

| Mechanism | Accounting | First-version limit |
| --- | --- | --- |
| Initial world funding | Explicit one-time money creation | Treasury 1,000; six residents 80 each |
| New-character welcome | Treasury-to-character transfer | Up to 20 pennies, two herbs, one meal while reserve lasts |
| Local sale / wages | Account-to-account transfer | Real payer funds, real goods or completed work |
| Outside export order | Money source, item removal | Up to eight meals × 7 pennies = 56 pennies per game day |
| Emergency outside herbs | Money sink, item source | Up to four herbs × 4 pennies = 16 pennies per game day |
| Gathering | Regenerating item source | Finite shared patch; seasonal daily recovery |
| Cooking / eating | Item transformation / item sink | Required actual ingredients / meal |

The patch begins with 40 bundles and caps at 60. Its daily recovery is
Spring 20, Summer 30, Autumn 12, Winter 4. Outside orders normally buy existing
keeper-owned meals above a reserve of eight. If the keeper has fewer than five
pennies, a physically present cook can instead consign a real surplus meal:
five pennies go to the cook and two to the keeper. This consumes the **same**
eight-order allowance and creates the **same** seven pennies per order; it is
not an additional source. At least eight meals must remain across those two
co-located accounts. The cook can keep producing toward this reserve rather
than stopping forever with a full delivery batch and no funded buyer.

Paid herb imports occur only below minimum shop ingredients, when the available
cook lacks a recipe's ingredients and the available forager has neither a
sufficient carried load nor a harvestable local supply. Imports remain an
expensive, capped fallback rather than competing immediately with local work.
The daily budgets are limits, not guaranteed payouts or automatic restocks.
Skipping many calendar days refreshes one bounded allowance instead of paying
an unlimited backlog. Creating many new characters exhausts the finite welcome
reserve; it is not an endless money exploit.

The maintained accounting invariant is:

```text
sum(all account balances) = cumulative minted pennies − cumulative sunk pennies
```

All transactions have a reason, actors, quantities, penny amount, and increasing
sequence. The first version retains the latest 128 entries plus cumulative
money counters. This is useful debugging evidence, not a complete immutable
production audit journal. Checkpoint validation rejects negative balances,
unknown goods, unsafe counts, invalid needs, malformed ledger order, and broken
money conservation before replacing live state.

## Recommended future intervention points

Controlled outside demand is the best initial inflow because it rewards a real
surplus and leaves a clear audit trail. Add the following only as explicit,
observable policies after the first loop is measured:

1. Settlement procurement contracts: capped purchases of locally needed goods,
   funded from a treasury. If replenishing that treasury creates money, record
   that as the source; do not mislabel the subsequent payment as a new source.
2. Chapter construction and maintenance contracts: budgets pay actual labor and
   material delivery. Building authority remains restricted to Chapters.
3. External caravans: limited import/export manifests, travel delays, and
   regional demand connect settlements without infinite merchants.
4. Relief reserves: a visible, capped emergency intervention when food access or
   settlement liquidity crosses a threshold; never secretly refill every purse.
5. Repairs, transport, and optional conveniences: useful recurring expenditures.
   Paying an NPC is a transfer unless some payment explicitly leaves circulation.

Track pennies entering/leaving per day, balances by role, unmet food demand,
stockouts, herb production/consumption, blocked contracts, and trader margins.
Source/sink tuning should support roleplay access and cooperation, not turn
basic participation into a starvation treadmill. Project Horseshoe's design
report emphasizes that resource flows and scarcity are choices made by game
designers, and that small imbalance can compound over time.
[Project Horseshoe: prosocial economics](https://www.projecthorseshoe.com/reports/featured/ph19r7.htm)

These first numbers do not prove long-run equilibrium. The revised settlement
survives a 30-day Spring probe and recovers from the earlier food-delivery
deadlock, but cash still accumulates unevenly by occupation and the keeper can
remain illiquid. Winter ingredients can be scarce, and a
population larger than the demo can exhaust demand or relief capacity. Do not
“fix” those situations with silent money creation: expose them, simulate
alternatives, and adjust declared budgets or productive opportunities.

## Verification and limits

`Tests/society_tests.cpp` covers finite welcome reserves, quote/trade atomicity,
unwanted goods and demand limits, scarcity, physically local timed production,
consumption and sleep, capped paid work, invalid bodies, outside budget caps,
money conservation, corrupt checkpoint rejection, and deterministic replay.
World integration must additionally enforce player reach and identity; the
economy core deliberately does not know player navigation or line of sight.

The implemented roles and positions are demonstration content. Arbitrary
authored settlements, player-to-player trade, player crafting, regional markets,
banking/debt, wages for players, business ownership, perishability, transport
inventories, durable equipment, and production load tests are not included.
Similarly, “socialize” is a deterministic activity state, not autonomous
LLM-generated background conversations. Chat providers remain separate.
