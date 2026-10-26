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
    // (Sooner, though not twice as many: a skilled baker's batches come about as fast as it looks for the next, doc 64.)
    expect(alone > 0 && helped > alone, "with a paw at x2.0 the batches come sooner: " + std::to_string(helped) + " against " +
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
    s.takeJournal();
    work(s, 700, day);
    // (Counted as brought in: the mill may buy the wheat at once.)
    const auto first = s.takeJournal();
    expect(count(first, "brought in", "wheat") == 2 && count(first, "brought in", "vegetables") == 2,
           "a spell in the fields brings in wheat and vegetables");
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

// Seasonal fields (doc 64, Phase 3b), with work by the hours: once it has sales to plan by, a farm founds its year (the
// fields as if sown, a store to the harvest); it brings in what it sowed at the harvest, plans next year's acres at the
// harvest's end, and sows them in spring. The harvest call draws the smith, not the baker.
void seasonalFields()
{
    Society::setWorkByHours(true);
    auto s = farmTown();
    const auto till = s.tillOf("farmer");
    for (std::int64_t d = 1; d <= 6; ++d)
    {
        set(s, "farmer", "wheat", 10);
        expect(s.sale(till, "miller", "wheat", 10, 1, "test: the mill's order"), "the mill buys the farm's wheat");
        s.spoil(d);
    }
    double day = 10 + 10. / 24;                      // (The tenth day of the year: spring, the sowing.)
    set(s, "farmer", "wheat", 0);
    work(s, 600, day);
    const auto founded = s.state().memory.fields.find(till);
    expect(founded != s.state().memory.fields.end() && founded->second.acres > 0 && founded->second.toHarvest == founded->second.acres,
           "the farm founds its year, its fields as if sown");
    // (The mill buys as the wheat comes in: what was made counts.)
    expect(held(s, "farmer", "wheat") + held(s, "miller", "wheat") > 100, "with a store to the harvest: " + std::to_string(held(s, "farmer", "wheat")));
    const double acres = founded->second.acres;
    set(s, "farmer", "wheat", 0);
    set(s, "miller", "wheat", 0);
    s.takeJournal();
    double summer = 150 + 10. / 24;
    work(s, 3600, summer, 1);
    expect(count(s.takeJournal(), "brought in", "wheat") == 0, "between sowing and the harvest the fields bring nothing in");
    double harvest = 220 + 10. / 24;
    work(s, 4 * 3600, harvest, 2);
    expect(count(s.takeJournal(), "brought in", "wheat") > 0 && s.state().memory.fields.at(till).toHarvest < acres,
           "at the harvest it brings in what it sowed");
    double after = 280 + 10. / 24;
    work(s, 600, after, 3);
    const auto& planned = s.state().memory.fields.at(till);
    expect(planned.year == 0 && planned.toHarvest == 0 && planned.sown == 0 && planned.acres >= 0,
           "at the harvest's end it plans next year's acres");
    double spring = 365 + 10 + 10. / 24;
    work(s, 4 * 3600, spring, 0);
    expect(planned.acres == 0 || s.state().memory.fields.at(till).sown > 0, "and sows them in spring");
    // The harvest call: some sowing morning the smith goes to the fields; the baker (bread is a staple) never.
    AuthoredRoster roster;
    roster.residents = {person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}), person("smith", "the smith at the forge", {"forge", 2.5, 2.5}),
                        person("bhand", "works at The Loaf", {"bakery", 3.5, 3.5}, "civilian"),
                        person("shand", "works at the forge", {"forge", 3.5, 3.5}, "civilian")};
    roster.economy.treasury = 500;
    Society t(Roster::None);
    t.configure(roster);
    const auto* smithy = t.jobOf("shand");
    const auto* bakery = t.jobOf("bhand");
    int smith = 0, baker = 0;
    for (std::int64_t d = 0; d < 60 && smithy && bakery; ++d)
    {
        smith += t.calledToTheLand("shand", *smithy, 30, d);
        baker += t.calledToTheLand("bhand", *bakery, 30, d);
    }
    expect(smithy && bakery && smith > 0 && smith < 20 && baker == 0,
           "the smith's hand is called to the land now and then, the baker's never: " + std::to_string(smith) + ", " + std::to_string(baker));
    int summerCalls = 0;
    for (std::int64_t d = 120; d < 180 && smithy; ++d)
        summerCalls += t.calledToTheLand("shand", *smithy, 30, d);
    expect(summerCalls == 0, "and not between sowing and the harvest");
    Society::setWorkByHours(false);
}

// Wood laid in for winter (doc 64, Phase 3b): a month before it, firewood's seller keeps its winter rate's store
// (households burn it twice as fast then: crafts.json `winterDays`); bread's store doesn't change.
void woodLaidInForWinter()
{
    // (Two towns alike, one on an autumn day and one a month before winter.)
    const auto on = [](double day) {
        auto s = farmTown();
        const auto till = s.tillOf("farmer");
        for (std::int64_t d = 1; d <= 6; ++d)
        {
            set(s, "farmer", "firewood", 10);
            expect(s.sale(till, "baker", "firewood", 10, 1, "test: the bakery's wood"), "the bakery buys the farm's firewood");
            s.spoil(d);
        }
        work(s, 60, day, 2);
        return std::pair{s.shelfTarget(till, "firewood", 0), s.shelfTarget(s.tillOf("baker"), "bread", 0)};
    };
    const auto before = on(200 + 10. / 24), now = on(255 + 10. / 24);
    expect(before.first > 0 && now.first >= 2 * before.first - 1 && now.second == before.second,
           "a month before winter the firewood laid in doubles: " + std::to_string(before.first) + " to " + std::to_string(now.first));
}

// Wear and mending for NPCs (doc 64, Phase 9), with work by the hours: a grown wolf owns its clothes and the sword it
// holds; worn to MendAt, an heirloom sword goes to the smith, who mends it whole for a fee, out of its own hours.
void piecesWornAndMended()
{
    Society::setWorkByHours(true);
    auto s = town();
    set(s, "smith", "sword", 20);                   // (Its shelf full: its hours bank.)
    {
        auto state = s.state();                     // (In its own purse: the scribe keeps a shop, and its till is the shop's.)
        state.accounts.at("scribe").stock["sword"] = 1;
        expect(s.restore(state), "the scribe's sword restores");
    }
    expect(s.shift("treasury", "scribe", "", 0, 200, "test: a fee in hand"), "the scribe has money");
    double day = 10. / 24;
    work(s, 600, day);
    day = 1 + 10. / 24;
    work(s, 60, day);
    const auto found = s.state().memory.belongings.find("scribe");
    const auto has = [&](bool held, const std::string& item) {
        return found != s.state().memory.belongings.end() &&
               std::any_of(found->second.begin(), found->second.end(), [&](const Belonging& b) { return b.held == held && b.item == item; });
    };
    expect(has(true, "sword") && has(false, "wool_scarf"), "the scribe owns the sword it holds, and its clothes");
    auto state = s.state();
    for (auto& b : state.memory.belongings.at("scribe"))
        if (b.item == "sword")
            b.cond = .3, b.heirloom = true;
    state.memory.wantFund["scribe"] = 150;
    expect(s.restore(state), "the worn sword restores");
    work(s, 600, day);
    s.takeJournal();
    day = 2 + 10. / 24;
    work(s, 60, day);
    const auto journal = s.takeJournal();
    const bool mended = std::any_of(journal.begin(), journal.end(), [&](const EconomyEntry& e) {
        return e.kind == "a piece mended" && e.from == "scribe" && e.to == s.tillOf("smith") && e.coins > 0;
    });
    double cond = 0;
    for (const auto& b : s.state().memory.belongings.at("scribe"))
        if (b.item == "sword")
            cond = b.cond;
    expect(mended && cond > .99 && Society::stock(*s.account("scribe"), "sword") == 1,
           "the smith mends the heirloom whole, for a fee");
    Society::setWorkByHours(false);
}

// What a household wants (doc 64, Phase 13; doc 63's model), with work by the hours: one want a day, out of its want
// fund. A piece it keeps first (finery to a piece a head), bought from what a shop sells; with all it wants, works on its
// home, bought by the amount from a builder's hours and its planks.
void aHouseholdsWants()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    auto wren = person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian");
    auto ash = person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian");
    auto keeper = person("keeper", "shopkeeper at the general store", {"store", 2.5, 2.5});
    auto carpenter = person("carpenter", "carpenter at the yard", {"yard", 2.5, 2.5});
    keeper.home = {"store_flat", 2.5, 2.5};
    carpenter.home = {"yard_flat", 2.5, 2.5};
    roster.residents = {keeper, carpenter, wren, ash};
    roster.economy.treasury = 5000;
    Society s(Roster::None);
    s.configure(roster);
    double day = 10. / 24;
    work(s, 60, day);
    set(s, "keeper", "straw_hat", 6);
    set(s, "carpenter", "straw_hat", 6);            // (Not its wares: never sold from its till.)
    set(s, "carpenter", "planks", 20);
    {
        auto state = s.state();
        state.memory.wantFund["homes"] = 100;
        state.memory.belongings["keeper"];          // (Belongings counted already: nothing founded.)
        expect(s.restore(state), "the household's want fund restores");
    }
    const auto members = [](const std::string& who) { return who == "wren" || who == "ash"; };
    s.takeJournal();
    day = 1 + 10. / 24;
    work(s, 60, day);
    auto journal = s.takeJournal();
    int wants = 0;
    std::string bought, buyer;
    for (const auto& e : journal)
        if (e.kind.rfind("a want: ", 0) == 0 && members(e.from))
        {
            ++wants, bought = e.item, buyer = e.from;
            expect(e.to == s.tillOf("keeper"), "bought from what a shop sells, never from a till that only holds it");
        }
    expect(wants == 1 && Society::belongingKind(bought) == "finery" && Society::stock(*s.account(buyer), bought) >= 1,
           "the household buys one piece of finery, and keeps it: " + bought);
    // All it wants, owned (a rapier, finery to a piece a head and two over): works on its home.
    {
        auto state = s.state();
        state.accounts.at("ash").stock["rapier"] = 1;
        state.accounts.at("ash").stock["straw_hat"] += 4;
        state.memory.wantFund["homes"] = 200;
        expect(s.restore(state), "a household that owns all it wants restores");
    }
    expect(s.shift("treasury", "ash", "", 0, 1000, "test: a good year"), "the household has money");
    day = 2 + 10. / 24;
    work(s, 60, day);
    journal = s.takeJournal();
    std::int64_t paid = 0;
    int planks = 0;
    bool piece = false;
    for (const auto& e : journal)
    {
        paid += e.kind == "works on the home" && members(e.from) && e.to == s.tillOf("carpenter") ? e.coins : 0;
        planks += e.kind == "used in works on the home" && e.from == s.tillOf("carpenter") && e.item == "planks" ? e.quantity : 0;
        piece |= e.kind == "a want: finery" || e.kind == "a want: a rapier" || e.kind == "a want: a sword";
    }
    expect(!piece && paid > 0 && paid <= 100 && planks > 0,
           "with all it wants, the carpenter works on its home with its planks, for half the fund at most: " + std::to_string(paid) + "p, " +
               std::to_string(planks) + " planks");
    Society::setWorkByHours(false);
}

// Hunting and hides (doc 64, Phase 10), with work by the hours: what a wolf brings in from the wild to eat goes to its
// household's larder first, up to three days of its food; the rest, and the hides, are for selling.
void theCatchFeedsTheLarder()
{
    Society::setWorkByHours(true);
    auto s = farmTown();
    const auto larder = Society::homeStore("farmhouse", "larder");
    auto state = s.state();
    state.accounts[larder].stock.clear();          // (Opened: a society without a map opens no home stores.)
    state.accounts.at("farmer").stock["rabbit"] = 12;
    state.accounts.at("farmer").stock["hide"] = 2;
    expect(s.restore(state), "the day's catch restores");
    const int kept = s.keepForTheLarder("farmer");
    const int rabbits = Society::stock(*s.account(larder), "rabbit");
    expect(kept > 0 && rabbits > 0 && rabbits * Society::nourishment("rabbit") >= Society::LarderDays * Society::DayNourishment &&
               (rabbits - 1) * Society::nourishment("rabbit") < Society::LarderDays * Society::DayNourishment,
           "the catch fills the larder to three days' food: " + std::to_string(rabbits) + " rabbits");
    expect(Society::stock(*s.account("farmer"), "rabbit") == 12 - rabbits && Society::stock(*s.account("farmer"), "hide") == 2,
           "the rest, and the hides, kept for selling");
    expect(s.keepForTheLarder("farmer") == 0, "a full larder takes no more");
    Society::setWorkByHours(false);
    expect(farmTown().keepForTheLarder("farmer") == 0, "(at the old pace, all of it is sold)");
}

// The mint (doc 64, Phase 11): a player who sits on a third of the realm's money while the realm goes hungry, and the
// mint strikes (after eight weeks of wages to judge by, a hundredth of the money a week at most); the hoard spent again,
// it melts what it struck, from every purse but a poor wolf's. Off, it does nothing.
void theMint()
{
    const auto realm = [](bool on) {
        auto s = town();
        auto state = s.state();
        const auto money = state.minted - state.sunk;
        const auto hoard = std::min<std::int64_t>(state.accounts.at("treasury").cash, money / 3);
        state.accounts.at("treasury").cash -= hoard;
        state.accounts["player-hoard"].cash = hoard;
        state.accounts.at("treasury").cash += state.accounts.at("scribe").cash - 20;
        state.accounts.at("scribe").cash = 20;      // (A poor wolf: under a week's food money.)
        for (auto& [id, life] : state.residents)
            life.hunger = 80;
        state.memory.mint.on = on;
        expect(s.restore(state), "the hoard restores");
        return s;
    };
    auto s = realm(true);
    const auto before = s.state().minted;
    s.takeJournal();
    for (std::int64_t week = 0; week < 7; ++week)
        s.tendMint(7 * week);
    expect(s.state().minted == before, "not before eight weeks of wages to judge by");
    for (std::int64_t week = 7; week < 20; ++week)
        s.tendMint(7 * week);
    const auto journal = s.takeJournal();
    const auto struck = s.state().minted - before;
    const auto target = s.state().memory.mint.target;
    expect(struck > 0 && std::any_of(journal.begin(), journal.end(), [](const EconomyEntry& e) { return e.kind == "the mint strikes" && e.to == "treasury"; }),
           "the hoard held and the realm hungry, the mint strikes into the treasury: " + std::to_string(struck) + "p");
    expect(double(struck) <= 13 * Society::MintStrikeMost * target + 13, "a hundredth of the money a week at most");
    expect(s.conserved(), "money counted as minted");
    // The hoard spent again, the realm fed: what was struck melts.
    auto state = s.state();
    state.accounts.at("treasury").cash += state.accounts.at("player-hoard").cash;
    state.accounts.at("player-hoard").cash = 0;
    for (auto& [id, life] : state.residents)
        life.hunger = 0;
    expect(s.restore(state), "the hoard spent restores");
    const auto sunk = s.state().sunk;
    for (std::int64_t week = 20; week < 40; ++week)
        s.tendMint(7 * week);
    expect(s.state().sunk > sunk && s.state().sunk - sunk <= struck, "the hoard spent again, it melts what it struck, no more: " +
                                                                         std::to_string(s.state().sunk - sunk) + "p");
    expect(s.account("scribe")->cash == 20, "and not from a poor wolf's purse");
    expect(s.conserved(), "money counted as sunk");
    auto off = realm(false);
    const auto offBefore = off.state().minted;
    for (std::int64_t week = 0; week < 20; ++week)
        off.tendMint(7 * week);
    expect(off.state().minted == offBefore, "turned off, the mint does nothing");
}

// Players' goods (doc 64, Phase 11): a player's stall dumps bread at a penny for three days, and the town buys there
// instead; the bakery's sales and the store it keeps fall. The stall gone, they come back as they were (doc 63's crash
// tests: the market rebounds by its own rules).
void aStallDumpAndTheRebound()
{
    auto s = town();
    const auto till = s.tillOf("baker");
    expect(s.shift("treasury", "scribe", "", 0, 400, "test: the town's bread money"), "the town has bread money");
    const auto day = [&](std::int64_t d, int sold) {
        if (sold > 0)
        {
            set(s, "baker", "bread", sold);
            expect(s.sale(till, "scribe", "bread", sold, 1, "test: the morning's queue"), "the town buys the bakery's bread");
            auto st = s.state();                    // (Eaten.)
            st.accounts.at("scribe").stock.erase("bread");
            expect(s.restore(st), "the bread eaten");
        }
        s.spoil(d);
    };
    for (std::int64_t d = 1; d <= 10; ++d)
        day(d, 10);
    const int before = s.shelfTarget(till, "bread", 0);
    for (std::int64_t d = 11; d <= 13; ++d)
        day(d, 0);                                  // (The player's penny bread: nobody comes to the bakery.)
    const int dumped = s.shelfTarget(till, "bread", 0);
    expect(dumped < before, "while the stall dumps, the bakery sells less and keeps less: " + std::to_string(before) + " to " + std::to_string(dumped));
    for (std::int64_t d = 14; d <= 40; ++d)
        day(d, 10);
    const int after = s.shelfTarget(till, "bread", 0);
    expect(after >= before - 1 && after <= before + 1, "the stall gone, it comes back as it was: " + std::to_string(after));
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
    // At the mill's own price and the carriage (doc 64, Phase 4), in place of half as much again.
    const auto each = Society::pennies(s.priceAt("miller", "flour") + s.carriage("flour", Society::CarriageLegs, "farstall"));
    expect(paid == got * each, "carted in, at the mill's price and the carriage: " + std::to_string(paid) + "p for " +
                                   std::to_string(got) + " (" + std::to_string(each) + "p each)");
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
// Output follows sales (doc 64, Phase 3b): a bakery that sells out of bread day after day keeps more of it, its target
// growing with what it sells, missed sales counting; one that sells none keeps the few it always kept. And it bakes to
// the larger shelf.
void outputFollowsSales()
{
    auto s = town();
    const auto till = s.tillOf("baker");
    expect(s.shelfTarget(till, "bread", Society::GoodsKept) == Society::GoodsKept, "a shelf with no sales keeps the few it always kept");
    int before = Society::GoodsKept;
    for (std::int64_t d = 1; d <= 6; ++d)
    {
        set(s, "baker", "bread", 12);
        expect(s.sale(till, "scribe", "bread", 12, 1, "test: the morning's queue"), "the scribe's household buys the bread");
        s.spoil(d);
        const int now = s.shelfTarget(till, "bread", Society::GoodsKept);
        expect(now >= before, "selling out, the shelf it keeps doesn't shrink (" + std::to_string(now) + ")");
        before = now;
        auto st = s.state();                        // (The bread eaten: conserved, it's only goods.)
        st.accounts.at("scribe").stock.erase("bread");
        expect(s.restore(st), "the bread eaten");
    }
    expect(before > 12, "a bakery selling out every day keeps more than it sold: " + std::to_string(before));
    expect(s.shelfTarget(s.tillOf("smith"), "bread", Society::GoodsKept) == Society::GoodsKept, "another shop's shelf is its own");
    set(s, "baker", "bread", 0);
    set(s, "baker", "flour", 40);
    set(s, "baker", "firewood", 40);
    double day = 6 + 10. / 24;
    work(s, 3600, day);
    expect(held(s, "baker", "bread") > Society::GoodsKept, "and bakes past the old few: " + std::to_string(held(s, "baker", "bread")));
}
} // namespace

// A town's trade (Docs/Design/61; Data/Economy/specialties.json): iron is Frosthollow's. A miner at Frosthollow's
// pit and one at a pit elsewhere work the same hours; Frosthollow's brings in more ore (yield), and keeps more of it to
// send away (keep), the fraction by a lot fixed by the spell, so the run repeats.
void aTownsTradeBringsInMore()
{
    expect(items::townGoodOf("frosthollow", "iron_ore") && !items::townGoodOf("larkfield", "iron_ore"), "Iron is Frosthollow's trade");
    AuthoredRoster roster;
    roster.residents = {person("north", "digs ore at the high pit", {"north_pit", 2.5, 2.5}, "civilian"),
                        person("south", "digs ore at the low pit", {"south_pit", 2.5, 2.5}, "civilian")};
    roster.economy.treasury = 500;
    Society s(Roster::None);
    s.configure(roster);
    LifeDay day;
    day.communityOf = [](const std::string& cell) { return cell == "north_pit" ? std::string("frosthollow") : std::string("elsewhere"); };
    s.setDay(day);
    double when = 10. / 24;
    work(s, 4 * 3600, when);
    const int north = held(s, "north", "iron_ore"), south = held(s, "south", "iron_ore");
    expect(north > south * 1.25 && north < south * 1.75 + 2, "Frosthollow's miner brings in about half as much again: " +
                                                                 std::to_string(north) + " to " + std::to_string(south));
    set(s, "north", "iron_ore", Society::ProducerKept - 1);       // Both nearly as much as a producer keeps.
    set(s, "south", "iron_ore", Society::ProducerKept - 1);
    work(s, 4 * 3600, when);
    expect(held(s, "south", "iron_ore") <= Society::ProducerKept && held(s, "north", "iron_ore") > Society::ProducerKept,
           "and keeps more of it to send away: " + std::to_string(held(s, "north", "iron_ore")) + " to " + std::to_string(held(s, "south", "iron_ore")));
}

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
        seasonalFields();
        woodLaidInForWinter();
        piecesWornAndMended();
        aHouseholdsWants();
        theCatchFeedsTheLarder();
        theMint();
        aStallDumpAndTheRebound();
        aTownsTradeBringsInMore();
        millGrindsForTheStall();
        cartedInFromAnotherTown();
        qualities();
        townsfolkBuy();
        outputFollowsSales();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Crafting tests passed: " << checks << " checks.\n";
    return 0;
}

