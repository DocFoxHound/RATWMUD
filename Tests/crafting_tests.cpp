// Crafting by shopkeepers (Docs/Design/35-items-crafting-industry.md, Phase 5, first part; Data/Items/crafts.json):
// makers start with a good store of materials, make a batch of what runs low from their own stock, buy more from a
// supplier in town when it runs short, and make nothing from nothing.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

ResidentSpec person(const std::string& id, const std::string& work, Spot job, const std::string& role = "merchant")
{
    ResidentSpec r;
    r.id = id;
    r.name = id;
    r.role = role;
    r.workLabel = work;
    r.home = {"homes", 2.5, 2.5};
    r.work = job;
    r.evening = r.home;
    r.serve = job;
    r.purse = 200;
    r.startHour = 8;
    r.endHour = 17;
    return r;
}

// A baker, a market stall that sells flour, a smith, and a cobbler-like shop that makes nothing yet.
Society town()
{
    AuthoredRoster roster;
    roster.residents = {person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}),
                        person("stall", "keeping the stall", {"market", 4.5, 4.5}),
                        person("smith", "the smith at the forge", {"forge", 2.5, 2.5}),
                        person("scribe", "the scribe", {"desk", 2.5, 2.5})};
    roster.economy.treasury = 500;
    Society s(Roster::None);
    s.configure(roster);
    return s;
}

// The workshops (doc 35, "Workshops for the starter crafts"): a farmer in the fields and a mill in town, a baker and a
// stall; and far off, a second town with a stall and no mill. Communities by cell, as the world's day plans give them.
Society farmTown()
{
    AuthoredRoster roster;
    auto farmer = person("farmer", "farms the valley fields", {"fields", 4.5, 4.5}, "civilian");
    farmer.home = {"farmhouse", 2.5, 2.5};
    roster.residents = {farmer,
                        person("miller", "runs The Ford Mill", {"mill", 2.5, 2.5}),
                        person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}),
                        person("stall", "keeping the stall", {"market", 4.5, 4.5}),
                        person("farstall", "keeping the stall", {"far_market", 4.5, 4.5})};
    roster.economy.treasury = 500;
    Society s(Roster::None);
    s.configure(roster);
    LifeDay day;
    day.communityOf = [](const std::string& cell) {
        return cell == "fields" ? std::string() : cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town");
    };
    s.setDay(day);
    return s;
}

std::map<std::string, LifeBody> atWork(const Society& s)
{
    std::map<std::string, LifeBody> bodies;
    for (const auto& r : s.authored().residents)
        bodies[r.id] = {r.work.cell, r.work.x, r.work.y};
    return bodies;
}

// (A business's goods are its till's since doc 46's Phase 2: tillOf, the resident itself for anyone else.)
int held(const Society& s, const std::string& id, const std::string& item)
{
    const auto* a = s.account(s.tillOf(id));
    return a ? Society::stockAll(*a, item) : 0;     // Of any quality (doc 35, Part 4).
}

void set(Society& s, const std::string& id, const std::string& item, int count)
{
    auto state = s.state();
    state.accounts.at(s.tillOf(id)).stock[item] = count;
    expect(s.restore(state), "the changed stock restores");
}

// A working morning (from 10:00), a game second at a time.
void work(Society& s, int seconds, double& day, int season = 0)
{
    const auto bodies = atWork(s);
    for (int i = 0; i < seconds; ++i)
    {
        day += 1. / 86400;
        s.tick(1, day, season, bodies);
        expect(s.conserved(), "money stays conserved");
    }
}

int count(const std::vector<EconomyEntry>& journal, const std::string& kind, const std::string& item)
{
    int n = 0;
    for (const auto& e : journal)
        if (e.kind == kind && items::baseOf(e.item) == item)
            n += e.quantity;
    return n;
}

void startingMaterials()
{
    auto s = town();
    expect(held(s, "baker", "flour") == Society::MaterialBatches && held(s, "baker", "firewood") == Society::MaterialBatches,
           "the baker starts with flour and firewood for eight batches");
    expect(held(s, "stall", "flour") == Society::SuppliesKept && held(s, "stall", "milk") == Society::SuppliesKept,
           "the stall starts with a store of what it supplies");
    expect(held(s, "smith", "bronze_bar") == Society::MaterialBatches && held(s, "smith", "charcoal") == Society::MaterialBatches,
           "the smith starts with bronze and charcoal");
    const auto bakes = s.wares("baker");
    expect(std::find(bakes.begin(), bakes.end(), "bread") != bakes.end(), "the baker sells bread");
    const auto stall = s.wares("stall");
    expect(std::find(stall.begin(), stall.end(), "flour") != stall.end(), "the stall sells flour");
    expect(s.state().craftingStocked == Society::CraftingStock, "the grant is recorded");
}

void bakesFromMaterials()
{
    auto s = town();
    set(s, "baker", "bread", 0);
    s.takeJournal();
    double day = 10. / 24;
    work(s, 600, day);
    expect(held(s, "baker", "bread") >= Society::GoodsKept, "the baker bakes bread");
    const auto journal = s.takeJournal();
    const int made = count(journal, "crafted", "bread");
    expect(made > 0 && made % 6 == 0, "bread comes in batches of six");
    expect(held(s, "baker", "flour") == Society::MaterialBatches - made / 6, "a batch uses a measure of flour");
    expect(held(s, "baker", "firewood") == Society::MaterialBatches - made / 6, "and a bundle of firewood");
}

// A paw lent (doc 53, 3): a player handing at the bakery makes its batches come sooner by the joint's rate, and now and
// then one input isn't used up. Money only moves.
void aPawLent()
{
    const auto bake = [](bool paw, int& batches, int& flourUsed) {
        auto s = town();
        double day = 10. / 24;
        batches = flourUsed = 0;
        for (int chunk = 0; chunk < 40; ++chunk)
        {
            if (paw)
                s.lendPaw("baker", 2.0, day + .05);
            set(s, "baker", "bread", 0);             // (Sold as it comes: it always wants another batch.)
            set(s, "baker", "flour", 20);
            set(s, "baker", "firewood", 20);
            s.takeJournal();
            work(s, 90, day);
            const auto journal = s.takeJournal();
            batches += count(journal, "crafted", "bread") / 6;
            flourUsed += 20 - held(s, "baker", "flour");
        }
    };
    int alone = 0, aloneFlour = 0, helped = 0, helpedFlour = 0;
    bake(false, alone, aloneFlour);
    bake(true, helped, helpedFlour);
    std::cout << "  the bakery, an hour: " << alone << " batches alone (" << aloneFlour << " flour), " << helped << " with a paw at x2.0 (" << helpedFlour
              << " flour)\n";
    expect(alone > 0 && helped >= alone * 3 / 2, "with a paw at x2.0 the batches come sooner: " + std::to_string(helped) + " against " +
                                                    std::to_string(alone));
    expect(aloneFlour == alone && helpedFlour < helped, "and now and then one input isn't used up: " + std::to_string(helpedFlour) +
                                                            " flour for " + std::to_string(helped) + " batches");
}

void buysWhenShort()
{
    auto s = town();
    set(s, "baker", "bread", 0);
    set(s, "baker", "flour", 1);
    const auto purse = s.account(s.tillOf("baker"))->cash;   // (The bakery's till, since doc 46.)
    double day = 10. / 24;
    work(s, 300, day);
    expect(held(s, "stall", "flour") == Society::SuppliesKept - (Society::MaterialBatches - 1),
           "the baker buys flour from the stall to make up the store");
    expect(held(s, "baker", "bread") > 0, "and bakes");
    expect(s.account(s.tillOf("baker"))->cash < purse, "paying for it");
}

void nothingFromNothing()
{
    auto s = town();
    set(s, "baker", "bread", 0);
    set(s, "baker", "flour", 0);
    set(s, "stall", "flour", 0);
    set(s, "scribe", "ink", 0);
    double day = 10. / 24;
    work(s, 1200, day);
    expect(held(s, "baker", "bread") == 0, "no flour anywhere: no bread");
    const auto journal = s.takeJournal();
    expect(count(journal, "made", "bread") == 0 && count(journal, "made", "ink") == 0, "nobody's shelves refill from nothing");
}

void smithForges()
{
    auto s = town();
    set(s, "smith", "sword", 0);
    double day = 10. / 24;
    work(s, 1000, day);
    expect(held(s, "smith", "sword") == 1, "a sword takes a quarter of an hour at the forge");
    expect(held(s, "smith", "bronze_bar") == Society::MaterialBatches - 1, "from a bar of bronze");
}

void olderSavesGetTheirGrantOnce()
{
    auto s = town();
    auto state = s.state();
    state.craftingStocked = 0;
    state.accounts.at(s.tillOf("baker")).stock.erase("flour");
    expect(s.restore(state), "a save from before crafting restores");
    double day = 10. / 24;
    work(s, 1, day);
    expect(held(s, "baker", "flour") == Society::MaterialBatches, "its makers are given their materials");
    set(s, "baker", "flour", 0);
    work(s, 1, day);
    expect(held(s, "baker", "flour") == 0, "only once");
}
void farmerBringsIn()
{
    auto s = farmTown();
    double day = 10. / 24;
    work(s, 700, day);
    expect(held(s, "farmer", "wheat") == 2 && held(s, "farmer", "vegetables") == 2, "a spell in the fields brings in wheat and vegetables");
    work(s, 3 * 3600, day);
    expect(held(s, "farmer", "wheat") <= Society::ProducerKept, "never more than a farmer keeps");
    auto winter = farmTown();
    double cold = 10. / 24;
    winter.takeJournal();
    work(winter, 3600, cold, 3);
    // Nothing grows in winter, but the barn's grain is threshed (doc 42, Phase 3c: crafts.json `offSeason`). (Counted as
    // brought in: the mill, keeping a full store of flour, buys the wheat as it comes.)
    const auto cold_ = winter.takeJournal();
    expect(count(cold_, "brought in", "wheat") >= 1 && count(cold_, "brought in", "vegetables") == 0 &&
               held(winter, "farmer", "vegetables") == 0,
           "in winter only the barn's grain is threshed, nothing grows");
}

void millGrindsForTheStall()
{
    auto s = farmTown();
    set(s, "stall", "flour", 2);
    set(s, "miller", "wheat", 0);
    {
        // (The stall's till: since doc 46 its keeper keeps a month's living, so a week's takings to buy with.)
        auto state = s.state();
        state.accounts.at("treasury").cash -= 200;
        state.accounts.at(s.tillOf("stall")).cash += 200;
        expect(s.restore(state), "the stall's takings restore");
    }
    const auto farmerPurse = s.account(s.tillOf("farmer"))->cash;   // (The farm's till, since doc 46.)
    double day = 10. / 24;
    work(s, 4 * 3600, day);
    expect(s.account(s.tillOf("farmer"))->cash > farmerPurse, "the mill buys the farmer's wheat");
    expect(held(s, "stall", "flour") >= Society::SuppliesKept / 4, "and the stall buys the mill's flour");
    const auto journal = s.takeJournal();
    expect(count(journal, "crafted", "flour") > 0, "flour ground from wheat");
}

void cartedInFromAnotherTown()
{
    auto s = farmTown();
    set(s, "farstall", "flour", 0);
    set(s, "miller", "flour", 30);
    s.takeJournal();
    double day = 10. / 24;
    work(s, 60, day);
    const int got = held(s, "farstall", "flour");
    expect(got > 0, "a town without a mill gets its flour from another");
    std::int64_t paid = 0;
    for (const auto& e : s.takeJournal())
        if (e.kind == "materials carted in" && e.item == "flour")
            paid += e.coins;
    expect(paid >= got * 3 && paid <= got * 4, "carted in, at half as much again, by the seller's supply (doc 42: 2p flour at 3p or "
                                               "4p): " + std::to_string(paid) + "p for " + std::to_string(got));
}
void qualities()
{
    // The catalog's kinds (doc 35, Part 4).
    const auto* fine = items::good("hide~fine");
    const auto* plain = items::good("hide");
    expect(fine && plain && fine->price == int(std::lround(plain->price * 1.6)) && fine->name == "Fine hide", "a fine hide: dearer, and named so");
    expect(items::good("hide~crude") && items::good("hide~masterwork") && !items::good("hide~shoddy"), "crude, fine, masterwork; nothing else");
    expect(!items::good("meal~fine") && !items::good("herbs~fine"), "meals and herbs have no qualities");
    expect(items::good("sword~fine") && items::good("sword~fine")->name == "Fine bronze sword", "swords do: a fine bronze sword");
    expect(items::qualityDamage(2) > 1 && items::qualityDamage(0) < 1, "a finer blade cuts deeper");
    expect(items::good("sword~fine")->durability > items::good("sword")->durability &&
               items::good("sword~crude")->durability < items::good("sword")->durability, "and lasts longer");
    // A masterwork's maker's mark.
    expect(items::makerOf("hide~masterwork@sorrel") == "sorrel" && items::baseOf("hide~masterwork@sorrel") == "hide" &&
               items::qualityOf("hide~masterwork@sorrel") == 3 && items::good("hide~masterwork@sorrel"), "a mark is read, and the good found");
    expect(items::withMaker("hide~masterwork", "sorrel") == "hide~masterwork@sorrel", "and made");
    const auto* vest = items::wearable("quilted_vest");
    const auto* fineVest = items::wearable("quilted_vest~fine");
    expect(vest && fineVest && fineVest->protect > vest->protect && fineVest->slot == vest->slot, "fine armour protects more");
    expect(items::baseOf("hide~fine") == "hide" && items::qualityOf("hide~crude") == 0 && items::qualityOf("hide") == 1, "ids read back");
    // A skilled baker turns out fine bread now and then; an unskilled one, never.
    const auto bake = [](double skill) {
        auto s = town();
        auto state = s.state();
        for (const auto& p : s.positions())
            if (p.founder == "baker")
                state.careers.skill["baker|" + p.id] = skill;
        state.accounts.at(s.tillOf("baker")).stock["bread"] = 0;
        state.accounts.at(s.tillOf("baker")).stock["flour"] = 200;
        state.accounts.at(s.tillOf("baker")).stock["firewood"] = 200;
        expect(s.restore(state), "the baker's skill restores");
        double day = 10. / 24;
        int fineBread = 0, crudeBread = 0;
        for (int round = 0; round < 40; ++round)
        {
            set(s, "baker", "bread", 0);
            set(s, "baker", "bread~fine", 0);
            set(s, "baker", "bread~crude", 0);
            set(s, "baker", "bread~masterwork", 0);
            work(s, 200, day);
            fineBread += Society::stock(*s.account(s.tillOf("baker")), "bread~fine");
            crudeBread += Society::stock(*s.account(s.tillOf("baker")), "bread~crude");
            if (day - std::floor(day) > 16. / 24)
                day = std::floor(day) + 1 + 10. / 24;
        }
        return std::pair<int, int>{fineBread, crudeBread};
    };
    {
        // A master's masterwork carries their mark.
        auto s = town();
        auto state = s.state();
        for (const auto& p : s.positions())
            if (p.founder == "baker")
                state.careers.skill["baker|" + p.id] = 100;
        state.accounts.at(s.tillOf("baker")).stock["flour"] = 400;
        state.accounts.at(s.tillOf("baker")).stock["firewood"] = 400;
        expect(s.restore(state), "the master baker restores");
        double day = 10. / 24;
        bool marked = false;
        for (int round = 0; round < 150 && !marked; ++round)
        {
            for (const auto& [item, n] : s.account(s.tillOf("baker"))->stock)
                marked = marked || (n > 0 && items::makerOf(item) == "baker" && items::qualityOf(item) == 3);
            auto st = s.state();
            for (auto& [item, n] : st.accounts.at(s.tillOf("baker")).stock)
                if (items::baseOf(item) == "bread")
                    n = 0;
            s.restore(st);
            work(s, 200, day);
            if (day - std::floor(day) > 16. / 24)
                day = std::floor(day) + 1 + 10. / 24;
        }
        expect(marked, "a masterwork loaf carries its baker's mark");
    }
    const auto [masterFine, masterCrude] = bake(95);
    const auto [noviceFine, noviceCrude] = bake(5);
    expect(masterFine > 0 && noviceFine == 0, "skill makes fine bread (" + std::to_string(masterFine) + " against " + std::to_string(noviceFine) + ")");
    expect(noviceCrude > masterCrude, "and the unskilled make crude");
    // A tanner takes a fine hide from a player, at the fine price.
    AuthoredRoster roster;
    roster.residents = {person("tanner", "tanner at The Hide Yard", {"yard", 2.5, 2.5})};
    roster.economy.treasury = 500;
    Society t(Roster::None);
    t.configure(roster);
    t.addPlayer("player-ada");
    t.create("player-ada", "hide~fine", 2, "test");
    const auto q = t.quote("player-ada", "tanner", "hide~fine", 1, false);
    const auto common = t.quote("player-ada", "tanner", "hide", 1, false);
    expect(q.ok && q.unitPrice > common.unitPrice, "a tanner buys a fine hide, dearer than a common one: " + q.message);
}
// A baker, a general store, and a household of two who buy their bread and their firewood (doc 35, Part 7).
Society folkTown()
{
    AuthoredRoster roster;
    auto wren = person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian");
    auto ash = person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian");
    wren.meals = ash.meals = 0;
    wren.purse = 60;
    auto baker = person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5});
    auto keeper = person("keeper", "shopkeeper at the general store", {"store", 2.5, 2.5});
    baker.home = {"bakers_flat", 2.5, 2.5};
    keeper.home = {"store_flat", 2.5, 2.5};
    roster.residents = {baker, keeper, wren, ash};
    roster.economy.treasury = 500;
    Society s(Roster::None);
    s.configure(roster);
    return s;
}

void townsfolkBuy()
{
    // A hungry wolf at the bakery buys bread (cheap filling) and eats it.
    {
        auto s = folkTown();
        auto state = s.state();
        state.residents.at("wren").hunger = 70;
        expect(s.restore(state), "hungry wren restores");
        std::map<std::string, LifeBody> bodies = atWork(s);
        double day = 10. / 24;
        const auto purse = s.account("wren")->cash;
        bool ate = false;
        for (int i = 0; i < 400 && !ate; ++i)
        {
            bodies["wren"] = {"bakery", 2.5, 2.5};     // At the counter.
            day += 1. / 86400;
            s.tick(1, day, 0, bodies);
            expect(s.conserved(), "money stays conserved");
            ate = s.resident("wren")->hunger < 60;
        }
        const auto journal = s.takeJournal();
        expect(count(journal, "resident food purchase", "bread") > 0, "wren buys bread at the bakery");
        expect(s.account("wren")->cash < purse && ate, "pays for it, and eats");
    }
    // Any food carried is eaten when hungry, by what it feeds.
    {
        auto s = folkTown();
        auto state = s.state();
        state.residents.at("ash").hunger = 70;
        state.accounts.at("ash").stock["porridge"] = 1;
        expect(s.restore(state), "ash with porridge restores");
        double day = 10. / 24;
        work(s, 60, day);
        expect(Society::stock(*s.account("ash"), "porridge") == 0 && s.resident("ash")->hunger < 60, "ash eats the porridge");
    }
    // Each day the household buys what is due (firewood every day) from a shop in town, and uses it up.
    {
        auto s = folkTown();
        const auto keeper = s.account(s.tillOf("keeper"))->cash;
        const int wood = Society::stock(*s.account(s.tillOf("keeper")), "firewood");
        expect(wood > 0, "the general store has firewood");
        s.takeJournal();
        double day = 10. / 24;
        for (int d = 0; d < 3; ++d)
        {
            day = std::floor(day) + 1 + 10. / 24;   // The next morning: the day's errands are run.
            work(s, 5, day);
        }
        const auto journal = s.takeJournal();
        expect(count(journal, "household purchase", "firewood") >= 2, "firewood bought, a bundle a day");
        expect(count(journal, "used at home", "firewood") >= count(journal, "household purchase", "firewood"), "and burnt (the shopkeeper's own too)");
        std::int64_t paid = 0;
        for (const auto& e : journal)
            if (e.kind == "household purchase" && e.to == s.tillOf("keeper"))
                paid += e.coins;
        expect(paid > 0, "the shop is paid");
        (void)keeper;
        // Never past what the household keeps back for food.
        auto st = s.state();
        st.accounts.at("treasury").cash += st.accounts.at("wren").cash - 12 + st.accounts.at("ash").cash - 5;   // (Conserved.)
        st.accounts.at("wren").cash = 12;
        st.accounts.at("ash").cash = 5;
        expect(s.restore(st), "a poor household restores");
        day = std::floor(day) + 1 + 10. / 24;
        s.takeJournal();
        work(s, 5, day);
        // (Wren and Ash's: the baker, a keeper with a till of its own since doc 46, keeps its own house from its own purse.)
        int poorBought = 0;
        for (const auto& e : s.takeJournal())
            poorBought += e.kind == "household purchase" && (e.from == "wren" || e.from == "ash");
        expect(poorBought == 0, "a poor household keeps its food money");
    }
}
} // namespace

int main()
{
    try
    {
        startingMaterials();
        bakesFromMaterials();
        aPawLent();
        buysWhenShort();
        nothingFromNothing();
        smithForges();
        olderSavesGetTheirGrantOnce();
        farmerBringsIn();
        millGrindsForTheStall();
        cartedInFromAnotherTown();
        qualities();
        townsfolkBuy();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Crafting tests passed: " << checks << " checks.\n";
    return 0;
}
