#include "RatwSociety.h"
#include "RatwWire.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool condition, const char* message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b)
{
    return std::abs(a - b) < 1e-7;
}
// Every penny in every account: the demo's money only moves (no wage, export or import makes or sinks any since doc 79).
std::int64_t totalCash(const Society& society)
{
    std::int64_t n = 0;
    for (const auto& [id, a] : society.state().accounts)
        n += a.cash;
    return n;
}
int held(const Society& society, const std::string& id, const std::string& item)
{
    const auto* account = society.account(id);
    return account ? Society::stock(*account, item) : 0;
}
void advance(Society& society, int seconds, const std::map<std::string, LifeBody>& bodies = {}, double day = .5,
             int season = 0)
{
    for (int second = 0; second < seconds; ++second)
        society.tick(1, day, season, bodies);
}
SocietyState isolated(Society& society, const std::string& active)
{
    auto state = society.state();
    for (auto& resident : state.residents)
    {
        resident.second.hunger = 0;
        resident.second.fatigue = 0;
        resident.second.progress = 0;
        resident.second.task = "idle";
        resident.second.goalCell.clear();
    }
    (void)active;
    return state;
}
void balancesAndTrade()
{
    Society society;
    expect(society.state().enabled && society.state().residents.size() == 6 && totalCash(society) == 1480 &&
               society.realBalanced() && society.realTotal() == 0,
           "Demo begins with six funded residents, a finite treasury and no real money");
    society.addPlayer("player-one");
    expect(society.account("player-one")->cash == 20 && society.account("treasury")->cash == 1000 &&
               held(society, "player-one", "herbs") == 2 && held(society, "player-one", "meal") == Society::StartingMeals &&
               society.account("treasury")->stock.at("herbs") == 100 && society.realTotal() == 20 &&
               society.realFaucets() == 20 && society.realBalanced(),
           "The welcome is made, a faucet (doc 79, 9.2): 20p, 2 herbs and a few days' meals, the treasury untouched");
    const auto entries = society.state().ledger.size();
    society.addPlayer("player-one");
    society.addPlayer("npc-impostor");
    expect(society.state().ledger.size() == entries && !society.account("npc-impostor") &&
               society.account("player-one")->cash == 20,
           "Repeated login and invalid identity cannot create grants");
    // (The Market Board's price, doc 79, 7: a meal 6p, the second a step up the per-piece ladder.)
    const auto quote = society.quote("player-one", "npc_keeper", "meal", 2, true);
    expect(quote.ok && quote.unitPrice == 6 && quote.total >= 12 && quote.total <= 14 && society.state().ledger.size() == entries,
           "Server quote derives the Board's price without mutating accounts");
    expect(society.trade("player-one", "npc_keeper", "meal", 2, true).ok && society.account("player-one")->cash == 20 - quote.total &&
               held(society, "player-one", "meal") == Society::StartingMeals + 2 && held(society, "npc_keeper", "meal") == 10 &&
               society.realBalanced(),
           "Purchase atomically transfers both finite goods and money");
    const auto offer = society.quote("player-one", "npc_keeper", "herbs", 2, false);
    const auto sale = society.trade("player-one", "npc_keeper", "herbs", 2, false);
    expect(sale.ok && sale.total == offer.total && sale.total >= 1 && held(society, "player-one", "herbs") == 0 &&
               held(society, "npc_keeper", "herbs") == 10 && society.realBalanced(),
           "Merchant pays only its finite cash for wanted stocked goods");
    for (int quantity : {-1, 0, 100, std::numeric_limits<int>::max()})
        expect(!society.trade("player-one", "npc_keeper", "meal", quantity, true).ok,
               "Malformed quantities cannot overflow or reverse a trade");
    expect(!society.trade("player-one", "npc_keeper", "junk", 1, false).ok &&
               !society.trade("player-one", "npc_scout", "meal", 1, true).ok &&
               !society.trade("missing", "npc_keeper", "meal", 1, true).ok,
           "Unknown goods, nonmerchants, and absent accounts are rejected");
    const auto cash = society.account("player-one")->cash;
    const auto meals = held(society, "player-one", "meal");
    expect(!society.trade("player-one", "npc_keeper", "meal", 99, true).ok &&
               society.account("player-one")->cash == cash && held(society, "player-one", "meal") == meals &&
               society.realBalanced(),
           "Failed trades leave both accounts unchanged");
    auto state = society.state();
    state.accounts["npc_keeper"].stock["herbs"] = 20;
    state.accounts["player-one"].stock["herbs"] = 2;
    expect(society.restore(state), "Full-merchant demand fixture restores");
    expect(!society.quote("player-one", "npc_keeper", "herbs", 1, false).ok,
           "Merchant refuses otherwise valid items above its demand cap");
    state = society.state();
    state.accounts["npc_keeper"].stock["herbs"] = 0;
    state.accounts["treasury"].cash += state.accounts["npc_keeper"].cash;
    state.accounts["npc_keeper"].cash = 0;
    expect(society.restore(state), "Cash-starved merchant fixture restores");
    expect(!society.quote("player-one", "npc_keeper", "herbs", 1, false).ok,
           "Demand alone does not grant a merchant unlimited purchasing power");
    // The welcome's cap (D23): once per account holder per world per game week. Many characters of one account holder in
    // a week: the first has the coins and herbs, the rest their meals only; a new week, a new welcome.
    Society treasury;
    for (int index = 0; index < 55; ++index)
    {
        treasury.setHolder("player-" + std::to_string(index), "holder-one");
        treasury.addPlayer("player-" + std::to_string(index));
    }
    expect(treasury.account("player-0")->cash == 20 && treasury.account("player-54")->cash == 0 &&
               held(treasury, "player-54", "herbs") == 0 && treasury.account("treasury")->cash == 1000 &&
               treasury.realTotal() == 20 && treasury.realBalanced(),
           "Many characters of one account holder have one welcome's money and herbs a week, the treasury untouched");
    // The one exception (doc 36): everyone starts with a few days' food.
    expect(held(treasury, "player-54", "meal") == Society::StartingMeals, "but every character still starts with its own meals");
    treasury.setHolder("player-other", "holder-two");
    treasury.addPlayer("player-other");
    expect(treasury.account("player-other")->cash == 20, "another account holder has its own welcome");
    treasury.tick(1, 7.5, 0, {});
    treasury.setHolder("player-later", "holder-one");
    treasury.addPlayer("player-later");
    expect(treasury.account("player-later")->cash == 20 && treasury.realBalanced(), "and the first holder's next week, its next");
}
void resourcesAndLocalWork()
{
    Society society;
    society.addPlayer("player-one");
    for (int count = 0; count < 40; ++count)
        expect(society.gather("player-one").ok, "Finite herb bundle can be gathered");
    expect(!society.gather("player-one").ok && society.state().herbPatch == 0 &&
               held(society, "player-one", "herbs") == 42 && society.realBalanced(),
           "Gathering depletes the shared patch without creating currency");
    for (int had = held(society, "player-one", "meal"); had > 0; --had)
        expect(society.eat("player-one").ok && held(society, "player-one", "meal") == had - 1, "Eating consumes a real meal");
    expect(!society.eat("player-one").ok, "and cannot consume absent stock");
    society.tick(1, 1, 3, {});
    expect(society.state().herbPatch == 4, "Winter supplies only four new herb bundles per calendar day");
    society.tick(1, 2, 1, {});
    expect(society.state().herbPatch == 34, "Summer regenerates more herbs than winter");
    society.tick(1, 1000, 1, {});
    expect(society.state().herbPatch == 60, "Missed days reset one bounded budget, not a backlog of grants");
    Society cook;
    auto state = isolated(cook, "npc_cook");
    state.accounts["npc_cook"].stock = {{"herbs", 2}, {"meal", 0}};
    expect(cook.restore(state), "Cooking fixture restores");
    std::map<std::string, LifeBody> bodies = {{"npc_cook", {"exterior", 26.5, 6.5, false}}};
    advance(cook, 50, bodies);
    expect(held(cook, "npc_cook", "herbs") == 2 && held(cook, "npc_cook", "meal") == 0 &&
               cook.resident("npc_cook")->task == "cook",
           "Cooking cannot complete from a different cell");
    bodies["npc_cook"].cell = "tavern";
    bodies["npc_cook"].x = 20;
    advance(cook, 50, bodies);
    expect(held(cook, "npc_cook", "meal") == 0, "A cook must physically reach the preparation station");
    bodies["npc_cook"].x = 26.5;
    advance(cook, 44, bodies);
    expect(held(cook, "npc_cook", "meal") == 0, "Crafting waits for its whole action duration");
    advance(cook, 1, bodies);
    expect(held(cook, "npc_cook", "herbs") == 0 && held(cook, "npc_cook", "meal") == 1 && cook.realBalanced(),
           "Two real herbs transform into one meal without creating money");
    advance(cook, 1, bodies);
    expect(cook.resident("npc_cook")->task == "buy ingredients",
           "An empty cook replans toward acquiring ingredients instead of free production");
    Society consumer;
    state = isolated(consumer, "npc_scout");
    state.residents["npc_scout"].hunger = 60;
    state.accounts["npc_scout"].stock["meal"] = 1;
    expect(consumer.restore(state), "Hunger fixture restores");
    bodies = {{"npc_scout", {"tavern", 19.5, 13.5, false}}};
    advance(consumer, 7, bodies);
    expect(held(consumer, "npc_scout", "meal") == 1, "NPC eating is a timed action rather than an instant deletion");
    advance(consumer, 1, bodies);
    expect(held(consumer, "npc_scout", "meal") == 0 && consumer.resident("npc_scout")->hunger < 6,
           "Hungry resident consumes a held meal and reduces its real need");
    Society sleeper;
    state = isolated(sleeper, "npc_scout");
    state.residents["npc_scout"].fatigue = 90;
    expect(sleeper.restore(state), "Fatigue fixture restores");
    bodies = {{"npc_scout", {"tavern", 19.5, 15.5, false}}};
    advance(sleeper, 20, bodies);
    expect(sleeper.resident("npc_scout")->task == "sleep" && sleeper.resident("npc_scout")->fatigue < 90,
           "Tired resident actually rests at its bed and recovers fatigue");
    bodies["npc_scout"].companion = true;
    advance(sleeper, 1, bodies);
    expect(sleeper.resident("npc_scout")->task == "companion" && sleeper.resident("npc_scout")->goalCell.empty() &&
               near(sleeper.resident("npc_scout")->progress, 0),
           "Recruitment suspends ordinary work and clears stale station goals");
}
void noOutsideMoney()
{
    // The demo's outside orders and imports (which made and sank money) went with doc 79's Phase 3: a keeper at its counter
    // with a glut of meals and no herbs serves, and nothing makes or sinks a penny or brings a herb from outside.
    Society society;
    auto state = isolated(society, "npc_keeper");
    state.accounts["npc_keeper"].stock = {{"herbs", 0}, {"meal", 30}};
    state.accounts["npc_cook"].stock["herbs"] = 0;
    state.accounts["npc_porter"].stock["herbs"] = 0;
    state.herbPatch = 0;
    expect(society.restore(state), "Outside-trade fixture restores");
    const auto supply = totalCash(society);
    const std::map<std::string, LifeBody> bodies = {{"npc_keeper", {"tavern", 9.5, 6.5, false}}};
    advance(society, 240, bodies);
    expect(society.resident("npc_keeper")->task == "trade" && totalCash(society) == supply &&
               held(society, "npc_keeper", "meal") == 30 && held(society, "npc_keeper", "herbs") == 0 && society.realBalanced(),
           "A keeper's trade at the counter neither exports meals nor imports herbs: no money made or sunk");
    society.tick(1, 1.5, 0, {});
    expect(totalCash(society) == supply, "and a new day renews no outside budget");
}
void residentWagesAndBodyValidation()
{
    Society society;
    auto state = isolated(society, "npc_scout");
    expect(society.restore(state), "Paid-work fixture restores");
    std::map<std::string, LifeBody> bodies = {{"npc_scout", {"tavern", 18.5, 14.5, false}}};
    advance(society, 1200, bodies);
    expect(society.account("npc_scout")->cash == 80 && society.resident("npc_scout")->task == "paid work",
           "A worker must reach its actual work cell before earning a contract");
    bodies["npc_scout"].cell = "exterior";
    advance(society, 1199, bodies);
    expect(society.account("npc_scout")->cash == 80, "Wages require the complete service duration");
    advance(society, 1, bodies);
    expect(society.account("npc_scout")->cash == 82 && society.account("npc_keeper")->cash == 78 &&
               society.resident("npc_scout")->wagesToday == 1 && society.realBalanced(),
           "Work pays from the employer's real purse rather than creating money");
    advance(society, 2400, bodies);
    expect(society.resident("npc_scout")->wagesToday == 3 && society.account("npc_scout")->cash == 86,
           "Resident can complete three bounded daily service contracts");
    advance(society, 1200, bodies);
    expect(society.account("npc_scout")->cash == 86 && society.resident("npc_scout")->task == "socialize",
           "Exhausted daily work allowance sends a fed resident back to free time");
    state = society.state();
    state.accounts["treasury"].cash += state.accounts["npc_keeper"].cash;
    state.accounts["npc_keeper"].cash = 0;
    state.residents["npc_scout"].hunger = 0;
    state.residents["npc_scout"].fatigue = 0;
    expect(society.restore(state), "Unfunded-employer fixture restores");
    advance(society, 1200, bodies, 1.5);
    expect(society.account("npc_scout")->cash == 86 && society.resident("npc_scout")->wagesToday == 0 &&
               society.realBalanced(),
           "A new day renews eligibility but not the employer's depleted cash");
    Society invalidBody;
    state = isolated(invalidBody, "npc_scout");
    state.residents["npc_scout"].hunger = 70;
    expect(invalidBody.restore(state), "Invalid-body fixture restores");
    for (const LifeBody& body : {LifeBody{"tavern", std::numeric_limits<double>::quiet_NaN(), 1, false},
                                 LifeBody{"tavern", 1, std::numeric_limits<double>::infinity(), false},
                                 LifeBody{"", 1, 1, false}, LifeBody{"tavern", -1, 1, false}})
    {
        const auto previous = invalidBody.state().nextEntry;
        advance(invalidBody, 10, {{"npc_scout", body}});
        expect(invalidBody.state().nextEntry == previous && near(invalidBody.resident("npc_scout")->hunger, 70) &&
                   invalidBody.restore(invalidBody.state()),
               "Malformed physical bodies cannot consume goods, change needs, or poison persisted goals");
    }
}
void sharedMerchantQueue()
{
    Society society;
    auto state = isolated(society, "npc_cook");
    state.accounts["treasury"].cash += state.accounts["npc_cook"].cash;
    state.accounts["npc_cook"].cash = 0;
    state.accounts["npc_cook"].stock = {{"herbs", 0}, {"meal", 0}};
    state.accounts["npc_keeper"].stock = {{"herbs", 8}, {"meal", 8}};
    state.accounts["npc_scout"].stock["meal"] = 0;
    state.residents["npc_scout"].hunger = 56;
    expect(society.restore(state), "Shared merchant interaction fixture restores");
    const std::map<std::string, LifeBody> bodies = {{"npc_cook", {"tavern", 10.5, 6.5, false}},
                                                    {"npc_keeper", {"tavern", 9.5, 6.5, false}},
                                                    {"npc_scout", {"tavern", 10.5, 6.5, false}}};
    advance(society, 12, bodies);
    expect(held(society, "npc_cook", "herbs") == 0 && held(society, "npc_scout", "meal") == 1 &&
               society.account("npc_scout")->cash == 74 && society.realBalanced(),
           "A broke cook waiting at the shared counter cannot reserve food buyers out forever");
}
void procurementRecovery()
{
    Society society;
    auto state = isolated(society, "npc_cook");
    state.accounts["npc_cook"].stock = {{"herbs", 0}, {"meal", 0}};
    state.accounts["npc_keeper"].stock = {{"herbs", 0}, {"meal", 0}};
    state.accounts["npc_porter"].stock = {{"herbs", 6}, {"meal", 0}};
    state.residents["npc_porter"].hunger = 100;
    state.residents["npc_keeper"].hunger = 100;
    expect(society.restore(state), "Empty-retailer/full-forager recovery fixture restores");
    std::map<std::string, LifeBody> bodies = {{"npc_cook", {"tavern", 10.5, 6.5, false}},
                                              {"npc_porter", {"tavern", 25.5, 6.5, false}},
                                              {"npc_keeper", {"tavern", 9.5, 6.5, false}}};
    advance(society, 1, bodies);
    expect(society.resident("npc_cook")->task == "receive herbs" && near(society.resident("npc_cook")->goalX, 25.5),
           "Ingredient-starved cook rendezvous with available forager instead of waiting at an empty shop");
    expect(society.resident("npc_porter")->task == "deliver herbs" && society.resident("npc_keeper")->task == "trade",
           "Hungry producer keeps food production moving and hungry keeper never attempts to buy from itself");
    expect(held(society, "npc_cook", "herbs") == 0, "Selecting a supplier cannot teleport distant goods");
    bodies["npc_cook"].x = 25.5; // Unit fixture supplies the navigation result, not the production effect.
    advance(society, 12, bodies);
    expect(held(society, "npc_cook", "herbs") >= 2 && held(society, "npc_porter", "herbs") < 6 && society.realBalanced(),
           "Physically reunited supplier and cook transfer real paid-for ingredients");
    expect(totalCash(society) == 1480, "Nothing burns money while local ingredients can be delivered");
    state = isolated(society, "npc_cook");
    state.accounts["treasury"].cash += state.accounts["npc_keeper"].cash - 5;
    state.accounts["npc_keeper"].cash = 5;
    state.accounts["npc_keeper"].stock["meal"] = 0;
    state.accounts["npc_cook"].stock = {{"herbs", 2}, {"meal", 3}};
    expect(society.restore(state), "One-meal working-capital fixture restores");
    bodies = {{"npc_cook", {"tavern", 10.5, 6.5, false}}, {"npc_keeper", {"tavern", 9.5, 6.5, false}}};
    advance(society, 12, bodies);
    expect(held(society, "npc_keeper", "meal") == 1 && society.account("npc_keeper")->cash == 0 &&
               held(society, "npc_cook", "meal") == 2 && society.realBalanced(),
           "An affordable one-meal delivery succeeds when a three-meal batch would fail");

    Society bumps;
    state = isolated(bumps, "npc_scout");
    state.residents["npc_scout"].hunger = 70;
    state.accounts["npc_scout"].stock["meal"] = 1;
    expect(bumps.restore(state), "Gentle-bump eating fixture restores");
    for (int second = 0; second < 8; ++second)
        bumps.tick(1, .5, 0, {{"npc_scout", {"tavern", 10.5 + .03 * second, 6.5, false}}});
    expect(held(bumps, "npc_scout", "meal") == 0 && bumps.resident("npc_scout")->hunger < 16,
           "Small crowd bumps do not endlessly reset the anchored eating action");
}

void consignmentTransactions()
{
    Society society;
    auto state = isolated(society, "npc_scout");
    state.accounts["treasury"].cash += state.accounts["npc_keeper"].cash;
    state.accounts["npc_keeper"].cash = 0;
    state.accounts["npc_keeper"].stock["meal"] = 0;
    state.accounts["npc_cook"].stock = {{"herbs", 0}, {"meal", 3}};
    state.accounts["npc_scout"].stock["meal"] = 0;
    state.residents["npc_scout"].hunger = 56;
    expect(society.restore(state), "Cashless keeper consignment fixture restores");
    std::map<std::string, LifeBody> bodies = {{"npc_keeper", {"tavern", 9.5, 6.5, false}},
                                              {"npc_cook", {"tavern", 25.5, 6.5, false}},
                                              {"npc_scout", {"tavern", 10.5, 6.5, false}}};
    advance(society, 12, bodies);
    expect(society.resident("npc_cook")->task == "deliver meals" && held(society, "npc_scout", "meal") == 0 &&
               society.account("npc_scout")->cash == 80,
           "Hungry funded demand brings the cook to market but cannot buy remotely through the keeper");
    bodies["npc_cook"].x = 10.5;
    const auto cookCash = society.account("npc_cook")->cash;
    const auto supply = totalCash(society);
    advance(society, 12, bodies);
    expect(held(society, "npc_scout", "meal") == 1 && held(society, "npc_cook", "meal") == 2 &&
               society.account("npc_scout")->cash == 74 && society.account("npc_cook")->cash == cookCash + 5 &&
               society.account("npc_keeper")->cash == 1 && totalCash(society) == supply && society.realBalanced(),
           "A co-located six-penny consignment pays five to cook and one to keeper without creating money or stock");
    const auto& ledger = society.state().ledger;
    expect(ledger.size() >= 2 && ledger[ledger.size() - 2].kind == "consigned meal sale" &&
               ledger[ledger.size() - 2].coins == 5 && ledger.back().kind == "market commission" &&
               ledger.back().coins == 1 && ledger.back().from == "npc_scout" && ledger.back().to == "npc_keeper",
           "Consignment records the two actual money recipients as distinct ledger flows");
    state = isolated(society, "npc_scout");
    state.accounts["treasury"].cash += state.accounts["npc_scout"].cash - 5;
    state.accounts["npc_scout"].cash = 5;
    state.accounts["npc_scout"].stock["meal"] = 0;
    state.residents["npc_scout"].hunger = 100;
    expect(society.restore(state), "Underfunded consignment buyer fixture restores");
    advance(society, 12, bodies);
    expect(held(society, "npc_scout", "meal") == 0 && society.account("npc_scout")->cash == 5 &&
               society.resident("npc_scout")->task == "paid work" && society.realBalanced(),
           "Five pennies cannot bypass the full retail price and an underfunded hungry worker seeks paid work");
}
void noConsignedExports()
{
    // A cook's surplus is never exported (the outside's orders went with doc 79's Phase 3): it cooks on and carries its
    // meals to the counter, and no outside money comes in for them.
    Society society;
    auto state = isolated(society, "npc_cook");
    state.accounts["treasury"].cash += state.accounts["npc_keeper"].cash;
    state.accounts["npc_keeper"].cash = 0;
    state.accounts["npc_keeper"].stock = {{"herbs", 2}, {"meal", 0}};
    state.accounts["npc_cook"].stock = {{"herbs", 2}, {"meal", 30}};
    expect(society.restore(state), "A large real surplus fixture restores");
    const auto supply = totalCash(society);
    std::map<std::string, LifeBody> bodies = {{"npc_cook", {"tavern", 10.5, 6.5, false}},
                                              {"npc_keeper", {"tavern", 9.5, 6.5, false}}};
    advance(society, 240, bodies);
    expect(totalCash(society) == supply && held(society, "npc_cook", "meal") + held(society, "npc_keeper", "meal") >= 30 &&
               society.realBalanced(),
           "A wealthy producer's surplus brings no outside money and leaves the realm with none of its meals");
    for (const auto& e : society.state().ledger)
        expect(e.kind == "initial funding" || (e.from != "outside" && e.to != "outside"), "no ledger entry to or from the outside");
}
void persistenceAndReplay()
{
    Society society;
    society.addPlayer("player-one");
    society.tick(.4, .5, 0, {});
    const auto saved = society.state();
    Society replay(false);
    expect(replay.restore(saved) && replay.realBalanced() && near(replay.state().decisionRemainder, .4),
           "Accounts, tasks, budgets, and partial decision interval restore atomically");
    {
        // An apprentice's task is "apprenticed: " and its master's title: a long title's save loads (DEV, 2026-10-31).
        auto apprenticed = saved;
        apprenticed.residents.begin()->second.task = "apprenticed: cooking for the King's household";
        Society again(false);
        expect(again.restore(apprenticed), "A resident apprenticed to a long title restores");
        apprenticed.residents.begin()->second.task = std::string(MaxTaskText + 1, 'x');
        Society refused(false);
        expect(!refused.restore(apprenticed), "A task past the limit is still refused (a broken life, not the economy's)");
    }
    std::map<std::string, LifeBody> bodies = {{"npc_keeper", {"tavern", 9.5, 6.5, false}},
                                              {"npc_cook", {"tavern", 26.5, 6.5, false}},
                                              {"npc_porter", {"exterior", 17.5, 7.5, false}}};
    for (int second = 0; second < 300; ++second)
    {
        const double date = .5 + second / 14400.0;
        society.tick(1, date, 0, bodies);
        replay.tick(1, date, 0, bodies);
    }
    bool equal = society.state().nextEntry == replay.state().nextEntry &&
                 totalCash(society) == totalCash(replay) && society.realBalanced() && replay.realBalanced();
    for (const auto& pair : society.state().accounts)
        equal &= replay.account(pair.first) && pair.second.cash == replay.account(pair.first)->cash &&
                 pair.second.stock == replay.account(pair.first)->stock;
    for (const auto& pair : society.state().residents)
        equal &= replay.resident(pair.first) && pair.second.task == replay.resident(pair.first)->task &&
                 near(pair.second.hunger, replay.resident(pair.first)->hunger) &&
                 near(pair.second.fatigue, replay.resident(pair.first)->fatigue);
    expect(equal, "Restarted resident/economy simulation replays deterministically without dialogue or wall clock");
    // Restore is lenient (doc 79, 14): a real account is kept to the penny as saved (no sum gate), a broken one refused;
    // the rest is reconciled, never a refusal.
    auto invalid = saved;
    invalid.accounts["player-one"].cash += 1;
    {
        Society kept(false);
        expect(kept.restore(invalid) && kept.account("player-one")->cash == saved.accounts.at("player-one").cash + 1 &&
                   kept.realBalanced(),
               "Restore keeps a player's purse to the penny as saved");
    }
    invalid = saved;
    invalid.accounts["player-one"].cash = -1;
    expect(!society.restore(invalid), "Restore rejects a negative real balance");
    invalid = saved;
    invalid.accounts["player-one"].cash = 1000000001;
    expect(!society.restore(invalid), "Restore rejects an overflowing real balance");
    invalid = saved;
    invalid.accounts["player-one"].stock["junk"] = 3;
    {
        Society kept(false);
        const auto* q = kept.restore(invalid) ? kept.quarantined("player-one") : nullptr;
        expect(q && q->count("junk") && q->at("junk") == 3 && !kept.account("player-one")->stock.count("junk"),
               "Unknown goods in a player's purse are quarantined, not lost and not refused");
        expect(json::dump(wire::stagedValue(kept.state().staged)).find("junk") != std::string::npos,
               "and the quarantine is saved");
    }
    // An escrow's quarantine follows its goods when it is emptied and closed (doc 79, 14): a stall's unknown good to its
    // keeper, still there after another restore, never left on a closed account for the next restore to drop.
    invalid = saved;
    invalid.accounts["stall:quarantine-test"].stock["junk"] = 3;
    {
        Society kept(false);
        const bool restored = kept.restore(invalid);
        const auto* q = restored ? kept.quarantined("stall:quarantine-test") : nullptr;
        expect(q && q->count("junk") && q->at("junk") == 3 && kept.account("stall:quarantine-test") &&
                   kept.account("stall:quarantine-test")->stock.empty(),
               "An unknown good in a stall is quarantined at restore, the stall's own stock empty");
        expect(!kept.closeAccount("stall:quarantine-test") && !kept.closeAccount("stall:quarantine-test", "player-nobody") &&
                   kept.quarantined("stall:quarantine-test"),
               "a stall with goods in quarantine and no heir with an account stays open, keeping them");
        const auto* keeper = kept.closeAccount("stall:quarantine-test", "player-one") ? kept.quarantined("player-one") : nullptr;
        expect(keeper && keeper->count("junk") && keeper->at("junk") == 3 && !kept.account("stall:quarantine-test") &&
                   !kept.quarantined("stall:quarantine-test"),
               "the stall cleared and closed, its quarantine goes to the keeper");
        Society again(false);
        const auto* still = again.restore(kept.state()) ? again.quarantined("player-one") : nullptr;
        expect(still && still->count("junk") && still->at("junk") == 3 && again.realBalanced(),
               "and after another restore the good is still quarantined for the keeper");
    }
    invalid = saved;
    invalid.accounts["npc_cook"].stock["junk"] = 3;
    invalid.accounts["npc_cook"].cash = -5;
    {
        Society kept(false);
        expect(kept.restore(invalid) && !kept.account("npc_cook")->stock.count("junk") && kept.cash("npc_cook") == 0 &&
                   !kept.quarantined("npc_cook"),
               "A resident's (staged) unknown goods are dropped and its money settled in range");
    }
    invalid = saved;
    invalid.accounts["player-one"].stock["herbs"] = 10001;
    expect(!society.restore(invalid), "Restore rejects excessive inventory in a real account");
    invalid = saved;
    invalid.residents["npc_cook"].hunger = std::numeric_limits<double>::quiet_NaN();
    expect(!society.restore(invalid), "Restore rejects nonfinite needs (a broken life)");
    invalid = saved;
    invalid.residents["npc_cook"].role = "generative oracle";
    {
        Society kept(false);
        expect(kept.restore(invalid) && kept.resident("npc_cook")->role == "cook", "A changed role takes the authored one");
    }
    invalid = saved;
    invalid.accounts.erase("treasury");
    {
        Society kept(false);
        expect(kept.restore(invalid) && kept.account("treasury"), "A save without the treasury is given one");
    }
    invalid = saved;
    invalid.accounts["fund:relief"] = {500, {{"meal", 4}}};
    invalid.accounts["outside:wharf"] = {900, {}};
    invalid.accounts["somebody-gone"] = {30, {}};
    {
        Society kept(false);
        expect(kept.restore(invalid) && !kept.account("fund:relief") && !kept.account("outside:wharf") && !kept.account("somebody-gone") &&
                   kept.account("player-one")->cash == saved.accounts.at("player-one").cash,
               "The old economy's accounts and a departed resident's are dropped; the player's purse is kept");
    }
    invalid = saved;
    invalid.ledger.front().sequence = invalid.nextEntry;
    expect(!society.restore(invalid), "Restore rejects invalid ledger sequencing");
    expect(society.realBalanced() && totalCash(society) == totalCash(replay),
           "Rejected checkpoints never partly overwrite live balances");
    const auto before = society.state().nextEntry;
    for (double seconds : {-1., 61., std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        society.tick(seconds, .5, 0, bodies);
    society.tick(1, -1, 0, bodies);
    society.tick(1, .5, 4, bodies);
    expect(society.state().nextEntry == before && society.realBalanced(),
           "Invalid simulation deltas, epochs, and seasons have no economic effects");
    Society disabled(false);
    expect(!disabled.state().enabled && disabled.state().residents.empty() && disabled.realBalanced(),
           "Non-demo world has no surprise seeded resident simulation");
}
} // namespace
int main()
{
    try
    {
        balancesAndTrade();
        resourcesAndLocalWork();
        noOutsideMoney();
        residentWagesAndBodyValidation();
        sharedMerchantQueue();
        procurementRecovery();
        consignmentTransactions();
        noConsignedExports();
        persistenceAndReplay();
        std::cout << "Passed " << checks << " deterministic resident and finite-economy checks.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Society check " << checks << " failed: " << error.what() << '\n';
        return 1;
    }
}
