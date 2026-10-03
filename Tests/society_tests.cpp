#include "RatwSociety.h"

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
    expect(society.state().enabled && society.state().residents.size() == 6 && society.moneySupply() == 1480 &&
               society.conserved(),
           "Demo begins with six funded residents and a finite conserved treasury");
    const auto initial = society.moneySupply();
    society.addPlayer("player-one");
    expect(society.account("player-one")->cash == 20 && society.account("treasury")->cash == 980 &&
               held(society, "player-one", "herbs") == 2 && held(society, "player-one", "meal") == Society::StartingMeals &&
               society.account("treasury")->stock.at("meal") == 50 && society.moneySupply() == initial && society.conserved(),
           "Welcome money and herbs come from the finite treasury; a few days' meals are a player's own (doc 36)");
    const auto entries = society.state().ledger.size();
    society.addPlayer("player-one");
    society.addPlayer("npc-impostor");
    expect(society.state().ledger.size() == entries && !society.account("npc-impostor") &&
               society.account("player-one")->cash == 20,
           "Repeated login and invalid identity cannot create grants");
    const auto quote = society.quote("player-one", "npc_keeper", "meal", 2, true);
    expect(quote.ok && quote.unitPrice == 6 && quote.total == 12 && society.state().ledger.size() == entries,
           "Server quote derives a stock-dependent price without mutating accounts");
    expect(society.trade("player-one", "npc_keeper", "meal", 2, true).ok && society.account("player-one")->cash == 8 &&
               held(society, "player-one", "meal") == Society::StartingMeals + 2 && held(society, "npc_keeper", "meal") == 10 &&
               society.conserved(),
           "Purchase atomically transfers both finite goods and money");
    const auto sale = society.trade("player-one", "npc_keeper", "herbs", 2, false);
    expect(sale.ok && sale.total == 2 && held(society, "player-one", "herbs") == 0 &&
               held(society, "npc_keeper", "herbs") == 10 && society.conserved(),
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
               society.conserved(),
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
    Society treasury;
    for (int index = 0; index < 55; ++index)
        treasury.addPlayer("player-" + std::to_string(index));
    expect(treasury.account("treasury")->cash == 0 && treasury.account("player-54")->cash == 0 &&
               held(treasury, "player-54", "herbs") == 0 && treasury.moneySupply() == 1480 && treasury.conserved(),
           "Many character creations exhaust the treasury's grants of money and herbs instead of minting them");
    // The one exception (doc 36): everyone starts with a few days' food, the treasury empty or not.
    expect(held(treasury, "player-54", "meal") == Society::StartingMeals, "but every character still starts with its own meals");
}
void resourcesAndLocalWork()
{
    Society society;
    society.addPlayer("player-one");
    for (int count = 0; count < 40; ++count)
        expect(society.gather("player-one").ok, "Finite herb bundle can be gathered");
    expect(!society.gather("player-one").ok && society.state().herbPatch == 0 &&
               held(society, "player-one", "herbs") == 42 && society.conserved(),
           "Gathering depletes the shared patch without creating currency");
    for (int had = held(society, "player-one", "meal"); had > 0; --had)
        expect(society.eat("player-one").ok && held(society, "player-one", "meal") == had - 1, "Eating consumes a real meal");
    expect(!society.eat("player-one").ok, "and cannot consume absent stock");
    society.tick(1, 1, 3, {});
    expect(society.state().herbPatch == 4, "Winter supplies only four new herb bundles per calendar day");
    society.tick(1, 2, 1, {});
    expect(society.state().herbPatch == 34, "Summer regenerates more herbs than winter");
    society.tick(1, 1000, 1, {});
    expect(society.state().herbPatch == 60 && society.state().exportsRemaining == 8 &&
               society.state().importsRemaining == 4,
           "Missed days reset one bounded budget, not a backlog of grants");
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
    expect(held(cook, "npc_cook", "herbs") == 0 && held(cook, "npc_cook", "meal") == 1 && cook.conserved(),
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
void cappedOutsideEconomy()
{
    Society society;
    auto state = isolated(society, "npc_keeper");
    state.accounts["npc_keeper"].stock = {{"herbs", 0}, {"meal", 30}};
    state.accounts["npc_cook"].stock["herbs"] = 0;
    state.accounts["npc_porter"].stock["herbs"] = 0;
    state.herbPatch = 0; // Imports are a genuine supply fallback, not automatic shelf filling.
    expect(society.restore(state), "Outside-trade fixture restores");
    const std::map<std::string, LifeBody> bodies = {{"npc_keeper", {"tavern", 9.5, 6.5, false}}};
    advance(society, 120, bodies);
    expect(society.state().exportsRemaining == 0 && society.state().minted == 1480 + 56 &&
               held(society, "npc_keeper", "meal") == 22,
           "Outside orders buy at most eight real meals for 56 new pennies per game day");
    expect(society.state().importsRemaining == 2 && society.state().sunk == 8 &&
               held(society, "npc_keeper", "herbs") == 2,
           "Need-driven imports consume actual pennies and stop when minimum ingredients are available");
    const auto supply = society.moneySupply();
    advance(society, 120, bodies);
    expect(society.moneySupply() == supply && society.state().minted == 1536 && society.conserved(),
           "Exhausted outside orders do not refill money during the same game day");
    state = society.state();
    state.accounts["npc_keeper"].stock["herbs"] = 0;
    expect(society.restore(state), "Consumed-import fixture restores");
    advance(society, 24, bodies);
    expect(society.state().importsRemaining == 0 && society.state().sunk == 16,
           "At most four imported bundles can remove sixteen pennies per day");
    state = society.state();
    state.accounts["npc_keeper"].stock["herbs"] = 0;
    expect(society.restore(state), "Exhausted-import fixture restores");
    advance(society, 24, bodies);
    expect(held(society, "npc_keeper", "herbs") == 0 && society.state().sunk == 16 && society.conserved(),
           "Daily import cap prevents an unlimited outside restock even when merchant is wealthy");
    society.tick(1, 1.5, 0, {});
    expect(society.state().exportsRemaining == 8 && society.state().importsRemaining == 4,
           "Shared-calendar day boundary renews bounded trade budgets");
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
               society.resident("npc_scout")->wagesToday == 1 && society.conserved(),
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
               society.conserved(),
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
               society.account("npc_scout")->cash == 74 && society.conserved(),
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
    expect(held(society, "npc_cook", "herbs") >= 2 && held(society, "npc_porter", "herbs") < 6 && society.conserved(),
           "Physically reunited supplier and cook transfer real paid-for ingredients");
    expect(society.state().sunk == 0,
           "Keeper does not burn emergency-import money while local ingredients can be delivered");
    state = isolated(society, "npc_cook");
    state.accounts["treasury"].cash += state.accounts["npc_keeper"].cash - 5;
    state.accounts["npc_keeper"].cash = 5;
    state.accounts["npc_keeper"].stock["meal"] = 0;
    state.accounts["npc_cook"].stock = {{"herbs", 2}, {"meal", 3}};
    expect(society.restore(state), "One-meal working-capital fixture restores");
    bodies = {{"npc_cook", {"tavern", 10.5, 6.5, false}}, {"npc_keeper", {"tavern", 9.5, 6.5, false}}};
    advance(society, 12, bodies);
    expect(held(society, "npc_keeper", "meal") == 1 && society.account("npc_keeper")->cash == 0 &&
               held(society, "npc_cook", "meal") == 2 && society.conserved(),
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
    const auto minted = society.state().minted;
    advance(society, 12, bodies);
    expect(held(society, "npc_scout", "meal") == 1 && held(society, "npc_cook", "meal") == 2 &&
               society.account("npc_scout")->cash == 74 && society.account("npc_cook")->cash == cookCash + 5 &&
               society.account("npc_keeper")->cash == 1 && society.state().minted == minted && society.conserved(),
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
               society.resident("npc_scout")->task == "paid work" && society.conserved(),
           "Five pennies cannot bypass the full retail price and an underfunded hungry worker seeks paid work");
}
void consignedExportLimits()
{
    Society society;
    auto state = isolated(society, "npc_cook");
    state.accounts["treasury"].cash += state.accounts["npc_keeper"].cash;
    state.accounts["npc_keeper"].cash = 0;
    state.accounts["npc_keeper"].stock = {{"herbs", 2}, {"meal", 0}};
    state.accounts["npc_cook"].stock = {{"herbs", 2}, {"meal", 8}};
    expect(society.restore(state), "Brokerage surplus-reserve fixture restores");
    std::map<std::string, LifeBody> bodies = {{"npc_cook", {"tavern", 26.5, 6.5, false}},
                                              {"npc_keeper", {"tavern", 9.5, 6.5, false}}};
    advance(society, 46, bodies);
    expect(held(society, "npc_cook", "meal") == 9 && held(society, "npc_cook", "herbs") == 0 &&
               society.resident("npc_cook")->task == "deliver meals" && society.state().exportsRemaining == 8,
           "Cash-starved market keeps productive cooking until a real surplus exists, without exporting remotely");
    bodies["npc_cook"].x = 10.5;
    const auto cookCash = society.account("npc_cook")->cash;
    advance(society, 12, bodies);
    expect(held(society, "npc_cook", "meal") == 8 && society.account("npc_cook")->cash == cookCash + 5 &&
               society.account("npc_keeper")->cash == 2 && society.state().minted == 1487 &&
               society.state().exportsRemaining == 7 && society.conserved(),
           "A co-located consigned surplus uses one existing seven-penny order split five/two");
    const auto& ledger = society.state().ledger;
    expect(ledger[ledger.size() - 2].kind == "outside consigned sale" && ledger[ledger.size() - 2].coins == 5 &&
               ledger.back().kind == "export commission" && ledger.back().coins == 2,
           "Consigned exports record both actual outside payments instead of hiding a second money source");
    advance(society, 24, bodies);
    expect(held(society, "npc_cook", "meal") + held(society, "npc_keeper", "meal") == 8 &&
               society.state().minted == 1487,
           "The combined eight-meal domestic reserve cannot be exported away");
    state = isolated(society, "npc_cook");
    state.accounts["npc_cook"].stock["meal"] = 30;
    expect(society.restore(state), "Large real consignment stock fixture restores");
    advance(society, 240, bodies);
    expect(society.state().exportsRemaining == 0 && society.state().minted == 1536 && society.conserved(),
           "Normal and consigned exports share exactly eight orders, never eight each");
    const auto supply = society.moneySupply();
    advance(society, 120, bodies);
    expect(society.moneySupply() == supply && society.state().minted == 1536,
           "A wealthy producer with more goods cannot extend the exhausted daily source cap");
}
void persistenceAndReplay()
{
    Society society;
    society.addPlayer("player-one");
    society.tick(.4, .5, 0, {});
    const auto saved = society.state();
    Society replay(false);
    expect(replay.restore(saved) && replay.conserved() && near(replay.state().decisionRemainder, .4),
           "Accounts, tasks, budgets, and partial decision interval restore atomically");
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
                 society.moneySupply() == replay.moneySupply() && society.conserved() && replay.conserved();
    for (const auto& pair : society.state().accounts)
        equal &= replay.account(pair.first) && pair.second.cash == replay.account(pair.first)->cash &&
                 pair.second.stock == replay.account(pair.first)->stock;
    for (const auto& pair : society.state().residents)
        equal &= replay.resident(pair.first) && pair.second.task == replay.resident(pair.first)->task &&
                 near(pair.second.hunger, replay.resident(pair.first)->hunger) &&
                 near(pair.second.fatigue, replay.resident(pair.first)->fatigue);
    expect(equal, "Restarted resident/economy simulation replays deterministically without dialogue or wall clock");
    auto invalid = saved;
    invalid.accounts["player-one"].cash += 1;
    expect(!society.restore(invalid), "Restore rejects unledgered money creation");
    invalid = saved;
    invalid.accounts["player-one"].cash = -1;
    expect(!society.restore(invalid), "Restore rejects negative balances");
    invalid = saved;
    invalid.accounts["player-one"].stock["junk"] = 1;
    expect(!society.restore(invalid), "Restore rejects unsupported goods");
    invalid = saved;
    invalid.accounts["player-one"].stock["herbs"] = 10001;
    expect(!society.restore(invalid), "Restore rejects excessive inventory");
    invalid = saved;
    invalid.residents["npc_cook"].hunger = std::numeric_limits<double>::quiet_NaN();
    expect(!society.restore(invalid), "Restore rejects nonfinite needs");
    invalid = saved;
    invalid.residents["npc_cook"].role = "generative oracle";
    expect(!society.restore(invalid), "Restore rejects unknown planner roles");
    invalid = saved;
    invalid.exportsRemaining = 9;
    expect(!society.restore(invalid), "Restore rejects expanded mint budgets");
    invalid = saved;
    invalid.accounts.erase("treasury");
    expect(!society.restore(invalid), "Restore requires finite grant reserve account");
    invalid = saved;
    invalid.ledger.front().sequence = invalid.nextEntry;
    expect(!society.restore(invalid), "Restore rejects invalid ledger sequencing");
    expect(society.conserved() && society.moneySupply() == replay.moneySupply(),
           "Rejected checkpoints never partly overwrite live balances");
    const auto before = society.state().nextEntry;
    for (double seconds : {-1., 61., std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        society.tick(seconds, .5, 0, bodies);
    society.tick(1, -1, 0, bodies);
    society.tick(1, .5, 4, bodies);
    expect(society.state().nextEntry == before && society.conserved(),
           "Invalid simulation deltas, epochs, and seasons have no economic effects");
    Society disabled(false);
    expect(!disabled.state().enabled && disabled.state().residents.empty() && disabled.conserved(),
           "Non-demo world has no surprise seeded resident simulation");
}
} // namespace
int main()
{
    try
    {
        balancesAndTrade();
        resourcesAndLocalWork();
        cappedOutsideEconomy();
        residentWagesAndBodyValidation();
        sharedMerchantQueue();
        procurementRecovery();
        consignmentTransactions();
        consignedExportLimits();
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
