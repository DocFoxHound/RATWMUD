// Crafting by shopkeepers (Docs/Design/35-items-crafting-industry.md, Phase 5, first part; Data/Items/crafts.json):
// makers start with a good store of materials, make a batch of what runs low from their own stock, buy more from a
// supplier in town when it runs short, and make nothing from nothing.
#include "RatwItems.h"
#include "RatwSociety.h"
#include "RatwTroubles.h"
#include "RatwWire.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>

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

// Seasonal fields (doc 64, Phase 3b), with work by the hours: once it has a week of sales to plan by (doc 64, Phase 13), a
// farm founds its year (the fields as if sown, a store to the harvest, its sales a day set right for the days its average
// has run); in its founding spring it sows more as its sales grow and tops its store up once, a fortnight on; it brings
// in what it sowed at the harvest, plans next year's acres at the harvest's end, and sows them in spring. Its year is
// saved, and an older save's still reads. The harvest call draws the smith, not the baker.
void seasonalFields()
{
    Society::setWorkByHours(true);
    auto s = farmTown();
    const auto till = s.tillOf("farmer");
    const auto* farm = items::producerFor("farms the valley fields");
    const auto sell = [&](std::int64_t d, int n) {
        set(s, "farmer", "wheat", n);
        expect(s.sale(till, "miller", "wheat", n, 1, "test: the mill's order"), "the mill buys the farm's wheat");
        s.spoil(d);
    };
    const auto fed = [&] {                          // (A farmer hungry or worn out goes for food or to bed, not to the fields.)
        auto st = s.state();
        st.residents.at("farmer").hunger = st.residents.at("farmer").fatigue = 20;
        expect(s.restore(st), "the farmer has eaten and slept");
    };
    for (std::int64_t d = 1; d <= 5; ++d)
        sell(d, 10);
    double early = 6 + 10. / 24;                     // (Its sixth day of sales, at the turn of the day.)
    work(s, 600, early);
    expect(!s.state().memory.fields.count(till), "on under a week of sales it brings in as it always did");
    sell(7, 10);
    double day = 10 + 10. / 24;                      // (The tenth day of the year: spring, the sowing.)
    set(s, "farmer", "wheat", 0);
    set(s, "miller", "wheat", 0);
    work(s, 600, day);
    const auto founded = s.state().memory.fields.find(till);
    expect(farm && founded != s.state().memory.fields.end() && founded->second.acres > 0 &&
               founded->second.toHarvest == founded->second.acres && founded->second.founded == 10 && !founded->second.toppedUp,
           "after a week the farm founds its year, its fields as if sown");
    // (Its fields all sown, the farmer is free to take a hire, as a hand whose business has no work for it: Phase 13.)
    const auto idle = [&] { return s.idleHand("farmer", s.payerOf("farmer", *s.jobOf("farmer"), 30)); };
    expect(idle(), "its fields all sown, the farmer is free for a hire");
    // Planned by its first whole week of sales, a day: the test's six days of ten, each its last (half again for the
    // buyers who came after), and the few the mill bought on the sixth; two to a yield. (The running average on the
    // day, set right for its days, had a week's sales of 60.)
    const double rate = founded->second.rate;
    const auto wheat = std::find_if(farm->out.begin(), farm->out.end(), [](const auto& o) { return o.first == "wheat"; });
    const int perYield = wheat == farm->out.end() ? 0 : wheat->second;
    expect(perYield == 2 && rate * 7 * perYield >= 6 * 15 && rate * 7 * perYield <= 6 * 15 + 6 && founded->second.month == rate &&
               founded->second.monthDays == 7,
           "planned by its first week's sales a day: " + std::to_string(rate * 7 * perYield) + " in its week");
    // (The mill buys as the wheat comes in: what was made counts.)
    expect(held(s, "farmer", "wheat") + held(s, "miller", "wheat") > 100, "with a store to the harvest: " + std::to_string(held(s, "farmer", "wheat")));
    const auto saved = wire::readSociety(wire::society(s.state())).memory.fields.at(till);
    expect(std::abs(saved.rate - rate) < 1e-6 * std::max(1., rate) && saved.founded == 10 && !saved.toppedUp &&
               std::abs(saved.acres - founded->second.acres) < 1e-6 * std::max(1., saved.acres) &&
               std::abs(saved.month - rate) < 1e-6 * std::max(1., rate) && saved.monthDays == 7,
           "its year is saved, its month's rate with it");
    auto older = wire::society(s.state());
    older.find("memory")->find("fields")->find(till)->items().resize(9);
    const auto nine = wire::readSociety(older).memory.fields.at(till);
    expect(std::abs(nine.rate - rate) < 1e-6 * std::max(1., rate) && nine.founded == 10 && nine.month == 0 && nine.monthDays == 0,
           "one saved before its month's rate reads, that rate to begin with its sales");
    older.find("memory")->find("fields")->find(till)->items().resize(6);
    const auto old = wire::readSociety(older).memory.fields.at(till);
    expect(old.acres > 0 && old.rate == 0 && old.founded == -1 && old.toppedUp, "and one saved before Phase 13 reads, as founded long ago");
    // Its sales grow: on its own day of the week it plans more acres, and its hands sow them; what was sown stays sown.
    const double first = founded->second.acres, sown = founded->second.sown;
    for (std::int64_t d = 11; d <= 23; ++d)
    {
        double morning = d + 10. / 24;
        work(s, 1200, morning);
        set(s, "farmer", "wheat", 30);
        expect(s.sale(till, "miller", "wheat", 30, 1, "test: the mill's order") && s.shift(till, "miller", "", 0, 30, "test: the mill's takings"),
               "the mill buys more of the farm's wheat");
    }
    // (Its year read afresh each time: set() restores the society, and a reference into the old one's fields is gone.)
    const auto grown = [&] { return s.state().memory.fields.at(till); };
    expect(grown().acres > first && grown().sown > sown && grown().toHarvest > sown,
           "its sales grown, its founding spring sows more: " + std::to_string(grown().acres) + " acres, from " + std::to_string(first));
    set(s, "farmer", "wheat", 0);
    set(s, "miller", "wheat", 0);
    s.takeJournal();
    double fortnight = 25 + 10. / 24;
    work(s, 600, fortnight);
    expect(grown().toppedUp && count(s.takeJournal(), "a year's store", "wheat") > 0, "and a fortnight on its store is topped up, once");
    double later = 26 + 10. / 24;
    set(s, "farmer", "wheat", 0);
    work(s, 600, later);
    expect(count(s.takeJournal(), "a year's store", "wheat") == 0, "only once");
    const double acres = grown().acres;
    set(s, "farmer", "wheat", 0);
    set(s, "miller", "wheat", 0);
    s.takeJournal();
    double summer = 150 + 10. / 24;
    work(s, 3600, summer, 1);
    expect(count(s.takeJournal(), "brought in", "wheat") == 0, "between sowing and the harvest the fields bring nothing in");
    expect(idle(), "and its farmer is free");
    double harvest = 220 + 10. / 24;
    fed();
    work(s, 4 * 3600, harvest, 2);
    expect(count(s.takeJournal(), "brought in", "wheat") > 0 && s.state().memory.fields.at(till).toHarvest < acres,
           "at the harvest it brings in what it sowed");
    expect(s.state().memory.fields.at(till).toHarvest <= 0 || !idle(), "and its farmer is at work while there's some to bring in");
    // (At night, its farmer asleep: the day's pass plans it, whether the farmer works that day or not.)
    double after = 280 + 2. / 24;
    fed();
    work(s, 60, after, 3);
    const auto planned = s.state().memory.fields.at(till);    // (A copy: fed() restores the society.)
    expect(planned.year == 0 && planned.toHarvest == 0 && planned.sown == 0 && planned.acres >= 0,
           "at the harvest's end it plans next year's acres");
    double spring = 365 + 10 + 10. / 24;
    fed();
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

// Food kept over Restday (doc 64, Phase 13): on the eve of a day the shops aren't properly open, the baker keeps that
// day's bread too (bread keeps four days: two days' sales, and the shut day's); firewood's store doesn't change.
void foodKeptOverRestday()
{
    // (Two towns alike, one on an ordinary day and one on the eve.)
    const auto on = [](std::int64_t day) {
        auto s = farmTown();
        const auto till = s.tillOf("baker");
        for (std::int64_t d = 1; d <= 6; ++d)
        {
            set(s, "baker", "bread", 10);
            expect(s.sale(till, "stall", "bread", 10, 1, "test: the stall's bread"), "the stall buys the baker's bread");
            s.spoil(d);
        }
        double at = double(day) + 10. / 24;
        work(s, 60, at, 1);
        return std::pair{s.shelfTarget(till, "bread", 0), s.shelfTarget(s.tillOf("farmer"), "firewood", 0)};
    };
    std::int64_t eve = 100, plain = 100;
    while (Society::shutAhead(eve) == 0)
        ++eve;
    while (Society::shutAhead(plain) > 0 || Society::shutAhead(plain - 1) > 0)
        ++plain;
    const auto usual = on(plain), ahead = on(eve);
    expect(usual.first > 0 && ahead.first > usual.first && ahead.second == usual.second,
           "on the eve the baker keeps more bread: " + std::to_string(usual.first) + " to " + std::to_string(ahead.first));
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
// fund. A piece it keeps first (finery to a piece a head), bought from what a shop sells, and waited for while its town's
// shops have none to be had (no works meanwhile); with all it wants, works on its home, bought by the amount from a
// builder's hours and its planks, and no fancy besides. The planks count as the carpenter's sales, so it makes them
// again, and its works toward its hands are kept for the save.
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
    set(s, "keeper", "straw_hat", 2);               // (Its wares, but its last few: none to be had today.)
    for (const char* trinket : {"charm", "fang_charm", "glass_beads"})
        set(s, "keeper", trinket, 0);               // (Its other finery.)
    set(s, "carpenter", "straw_hat", 6);            // (Not its wares: never sold from its till.)
    set(s, "carpenter", "planks", 20);
    {
        auto state = s.state();
        state.memory.wantFund["homes"] = 100;
        state.memory.piecesFounded = 1;             // (Founded already: nothing from nothing.)
        expect(s.restore(state), "the household's want fund restores");
    }
    const auto members = [](const std::string& who) { return who == "wren" || who == "ash"; };
    const auto worksPaid = [&](const std::vector<EconomyEntry>& journal) {
        std::int64_t paid = 0;
        for (const auto& e : journal)
            paid += e.kind == "works on the home" && members(e.from) ? e.coins : 0;
        return paid;
    };
    s.takeJournal();
    day = 1 + 10. / 24;
    work(s, 60, day);
    auto journal = s.takeJournal();
    expect(worksPaid(journal) == 0 && std::none_of(journal.begin(), journal.end(), [](const EconomyEntry& e) { return e.kind == "a want: finery"; }) &&
               s.state().memory.pieceWait.count("homes") && s.state().memory.pieceWait.at("homes") == 1,
           "finery its town sells and its fund runs to, none to be had today, is waited for: no works on its home meanwhile");
    set(s, "keeper", "straw_hat", 6);
    day = 2 + 10. / 24;
    work(s, 60, day);
    journal = s.takeJournal();
    // (Its sale missed, at the turn of the day in the store's sales of its finery: so it makes or buys in more.)
    double missed = 0;
    if (const auto* rates = s.sellRates(s.tillOf("keeper")))
        for (const auto& [item, rate] : *rates)
            missed += Society::belongingKind(item) == "finery" ? rate : 0;
    expect(std::abs(missed - .3) < 1e-9, "the store hears of the piece it didn't have: a sale missed, " + std::to_string(missed / .3));
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
    day = 3 + 10. / 24;
    work(s, 60, day);
    journal = s.takeJournal();
    const auto till = s.tillOf("carpenter");
    std::int64_t paid = 0;
    int planks = 0, sold = 0;
    bool piece = false, fancy = false;
    for (const auto& e : journal)
    {
        paid += e.kind == "works on the home" && members(e.from) && e.to == till ? e.coins : 0;
        planks += e.kind == "used in works on the home" && e.from == till && e.item == "planks" ? e.quantity : 0;
        sold += e.to == till && items::baseOf(e.item) == "planks" && e.coins > 0 ? e.quantity : 0;
        piece |= e.kind == "a want: finery" || e.kind == "a want: a rapier" || e.kind == "a want: a sword";
        fancy |= e.kind.rfind("a want: ", 0) == 0 && members(e.from);
    }
    expect(!piece && paid > 0 && paid <= 100 && planks > 0,
           "with all it wants, the carpenter works on its home with its planks, for half the fund at most: " + std::to_string(paid) + "p, " +
               std::to_string(planks) + " planks");
    expect(!fancy, "and the day's one want is the works: no fancy besides");
    // Its planks used, as sold (Society::spoil, at the next day's pass): so its shelf's plan and its hands follow its works.
    const auto rate = [&] {
        const auto* rates = s.sellRates(till);
        return rates && rates->count("planks") ? rates->at("planks") : 0.;
    };
    const double before = rate();
    day = 4 + 10. / 24;
    work(s, 1, day);
    expect(rate() >= .7 * before + .3 * double(planks + sold) - 1e-9,
           "the planks its works used count as the carpenter's sales: " + std::to_string(rate()) + " a day");
    // Its works, toward its hands: kept in the saved memory at the day pass's end, and back from it on a restore.
    expect(s.state().memory.worksRate.count(till) && s.state().memory.worksRate.at(till) > 0, "the carpenter's works are kept for the save");
    {
        auto state = s.state();
        state.memory.worksRate[till] = 80;
        expect(s.restore(state), "a carpenter with works in hand restores");
        const int busy = s.handsWanted(till);
        state.memory.worksRate.erase(till);
        expect(s.restore(state), "and one with none");
        expect(busy > s.handsWanted(till), "a restored carpenter's works count toward its hands");
    }
    Society::setWorkByHours(false);
}

// A household with money, its two members at work, and a general store, a carpenter and (with `smith`) an armourer in
// its town (doc 64, Phase 13), with work by the hours, founded already: nothing from nothing.
Society householdTown(bool smith)
{
    AuthoredRoster roster;
    auto wren = person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian");
    auto ash = person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian");
    auto keeper = person("keeper", "shopkeeper at the general store", {"store", 2.5, 2.5});
    auto carpenter = person("carpenter", "carpenter at the yard", {"yard", 2.5, 2.5});
    auto forge = person("smith", "the armourer at the gate", {"forge", 2.5, 2.5});
    keeper.home = {"store_flat", 2.5, 2.5};
    carpenter.home = {"yard_flat", 2.5, 2.5};
    forge.home = {"forge_flat", 2.5, 2.5};
    roster.residents = {keeper, carpenter, wren, ash};
    if (smith)
        roster.residents.push_back(forge);
    roster.economy.treasury = 5000;
    Society s(Roster::None);
    s.configure(roster);
    double day = 10. / 24;
    work(s, 60, day);
    set(s, "carpenter", "planks", 20);
    auto state = s.state();
    state.memory.piecesFounded = 1;
    expect(s.restore(state), "the household's world restores");
    expect(s.jobOf("wren") && s.jobOf("wren")->paid, "the household works");
    return s;
}

// The day's want of the household (wren and ash) on `day`: what it paid for works on its home, and the wants bought.
std::pair<std::int64_t, std::vector<std::string>> householdsDay(Society& s, std::int64_t day, double fund)
{
    {
        auto state = s.state();
        state.memory.wantFund["homes"] = fund;
        expect(s.restore(state), "the household's want fund restores");
    }
    s.takeJournal();
    double at = double(day) + 10. / 24;
    work(s, 60, at);
    std::int64_t works = 0;
    std::vector<std::string> wants;
    for (const auto& e : s.takeJournal())
        if (e.from == "wren" || e.from == "ash")
        {
            works += e.kind == "works on the home" ? e.coins : 0;
            if (e.kind.rfind("a want: ", 0) == 0)
                wants.push_back(e.kind.substr(8));
        }
    return {works, wants};
}

// A piece waited for a few days at most (doc 64, Phase 13; model.py's wants): finery its fund runs to and the store has
// none of on its shelves is waited for, no works meanwhile, the store told each day of the sale it missed; after
// PieceWaitDays, its works go on while it still looks. The wait is saved; with a piece found, it is over, and a household
// that's gone waits no more.
void aPieceWaitedForAFewDays()
{
    Society::setWorkByHours(true);
    auto s = householdTown(false);
    set(s, "keeper", "straw_hat", 2);               // (Its last few: none to be had.)
    for (const char* trinket : {"charm", "fang_charm", "glass_beads"})
        set(s, "keeper", trinket, 0);
    {
        auto state = s.state();
        state.accounts.at("ash").stock["rapier"] = 1;   // (Its blade: no sword wanted.)
        expect(s.restore(state), "the household's rapier restores");
    }
    expect(s.shift("treasury", "ash", "", 0, 1000, "test: a good year"), "the household has money");
    for (std::int64_t d = 1; d <= Society::PieceWaitDays; ++d)
    {
        const auto [works, wants] = householdsDay(s, d, 200);
        expect(works == 0 && std::find(wants.begin(), wants.end(), "finery") == wants.end() && s.state().memory.pieceWait.count("homes") &&
                   s.state().memory.pieceWait.at("homes") == 1,
               "day " + std::to_string(d) + " of its wait: no works, and no finery to be had");
    }
    const auto saved = wire::readSociety(wire::society(s.state())).memory.pieceWait;
    expect(saved.count("homes") && saved.at("homes") == 1, "its wait is saved");
    auto older = wire::society(s.state());
    older.find("memory")->erase("pieceWait");
    expect(wire::readSociety(older).memory.pieceWait.empty(), "and an older save has none");
    const auto [works, wants] = householdsDay(s, Society::PieceWaitDays + 1, 200);
    expect(works > 0 && wants.empty() && s.state().memory.pieceWait.count("homes"),
           "after " + std::to_string(Society::PieceWaitDays) + " days its works go on, still looking: " + std::to_string(works) + "p");
    set(s, "keeper", "straw_hat", 6);
    {
        auto state = s.state();
        state.memory.pieceWait["gone_home"] = 2;    // (A household no longer there.)
        expect(s.restore(state), "a household gone, its wait left");
    }
    const auto found = householdsDay(s, Society::PieceWaitDays + 2, 200);
    expect(std::find(found.second.begin(), found.second.end(), "finery") != found.second.end() && !s.state().memory.pieceWait.count("homes"),
           "with a piece on the shelf, it buys it, and its wait is over");
    expect(s.state().memory.pieceWait.empty(), "and a household gone waits no more");
    Society::setWorkByHours(false);
}

// A piece it can't get goes on to the rest (doc 64, Phase 13; model.py's wants: only one its fund runs to): a household
// that works and holds no blade, its finery owned, in a town whose shops sell no sword, has works on its home, and
// wants no sword; in one with an armourer whose swords are past what its fund holds, the same; and none is waited for.
void piecesItCantGetGoOnToWorks()
{
    Society::setWorkByHours(true);
    for (const bool smith : {false, true})
    {
        auto s = householdTown(smith);
        {
            auto state = s.state();
            state.accounts.at("ash").stock["straw_hat"] = 4;   // (Finery to a piece a head, and two over.)
            expect(s.restore(state), "the household's finery restores");
        }
        expect(s.shift("treasury", "ash", "", 0, 1000, "test: a good year"), "the household has money");
        std::int64_t fund = 100;
        if (smith)
        {
            set(s, "smith", "iron_sword", 20);
            set(s, "smith", "steel_sword", 20);
            auto state = s.state();                 // (Its swords dear: ten times their cost.)
            for (const char* blade : {"iron_sword", "steel_sword"})
                state.memory.margins[s.tillOf("smith") + "|" + blade] = 9;
            expect(s.restore(state), "the armourer's margins restore");
            fund = std::min(s.shopPrice("smith", "iron_sword"), s.shopPrice("smith", "steel_sword")) / 2;
            expect(fund >= 100, "its cheapest sword twice the fund: " + std::to_string(fund) + "p");
        }
        const auto [works, wants] = householdsDay(s, 1, double(fund));
        expect(works > 0 && std::find(wants.begin(), wants.end(), "a sword") == wants.end() && !s.state().memory.pieceWait.count("homes"),
               std::string(smith ? "swords past its fund (" + std::to_string(fund) + "p)" : "no sword sold in its town") +
                   ": works on its home, no sword, no wait: " + std::to_string(works) + "p");
    }
    Society::setWorkByHours(false);
}

// A piece it could pay for and found on no shelf is a sale missed of one its fund runs to (doc 64, Phase 13; the
// model's asks): an armourer with no swords on its shelves hears of the cheapest, never of the dear one past the fund
// (a 40p fund told an armourer of a 90p steel sword); a store with no finery, of the dearest piece the fund runs to (the
// dearest is what it buys), never of its charms at ten times their cost. Twenty days of looking, each day's by its own
// lot. (The fund is the cheapest piece's price before the day's share comes into it.)
void aMissedPieceIsOneItCouldPayFor()
{
    Society::setWorkByHours(true);
    for (const bool sword : {true, false})
    {
        auto s = householdTown(true);
        const std::string shop = sword ? "smith" : "keeper", kind = sword ? "sword" : "finery";
        const auto till = s.tillOf(shop);
        {
            auto state = s.state();
            if (sword)
                state.accounts.at("ash").stock["straw_hat"] = 4;   // (Finery to a piece a head, and two over.)
            else
            {
                state.accounts.at("ash").stock["rapier"] = 1;      // (Its blade: no sword wanted.)
                state.memory.margins[till + "|charm"] = 19;        // (Its charms dear.)
            }
            expect(s.restore(state), "the household's belongings restore");
        }
        for (const char* who : {"ash", "wren"})
            expect(s.shift("treasury", who, "", 0, 1000, "test: a good year"), "the household has money");
        std::vector<std::pair<std::string, std::int64_t>> pieces;
        for (const auto& ware : s.wares(shop))
            if (Society::belongingKind(ware) == kind)
                pieces.push_back({ware, s.shopPrice(shop, ware)});
        std::sort(pieces.begin(), pieces.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        const auto fund = pieces.front().second;
        const auto dear = pieces.back();
        expect(pieces.size() >= 2 && dear.second >= fund + 50, "the " + shop + " sells a " + kind + " at " + std::to_string(fund) +
                                                                       "p and a dear one: " + dear.first + " at " + std::to_string(dear.second) + "p");
        // (The one it could pay for: the cheapest sword, or the dearest finery short of the dear one.)
        const auto told = sword ? pieces.front().first : pieces[pieces.size() - 2].first;
        const auto rate = [&](const std::string& piece) {
            const auto* rates = s.sellRates(till);
            return rates && rates->count(piece) ? rates->at(piece) : 0.;
        };
        // (Each day's sales into the running average at the turn of the day: none sold, nor missed, it only wanes.)
        double was = 0, heardWas = 0;
        bool heard = false;
        for (std::int64_t d = 1; d <= 21; ++d)
        {
            for (const auto& [piece, price] : pieces)
                set(s, shop, piece, 0);             // (None on its shelves.)
            householdsDay(s, d, double(fund));
            if (d > 1)
                expect(std::abs(rate(dear.first) - .7 * was) < 1e-12, "day " + std::to_string(d) + ", never of the one past its fund: " +
                                                                          dear.first + " at " + std::to_string(dear.second) + "p");
            heard |= d > 1 && rate(told) > .7 * heardWas + 1e-9;
            was = rate(dear.first), heardWas = rate(told);
        }
        expect(heard, "the " + shop + " hears of the " + kind + " the household could pay for: " + told);
    }
    Society::setWorkByHours(false);
}

// Works that use a builder's last planks are a sell-out (doc 64, Phase 13; Society::spoil): its sales a day counted half
// again, as a shelf sold bare is, so it plans for more. The same works from twenty planks are sales as they are. (A
// carpenter with nothing to make planks from, its hours in hand from a day at the yard.)
void worksSellOutTheShelf()
{
    Society::setWorkByHours(true);
    const auto worksWith = [](int planks) {
        AuthoredRoster roster;
        auto carpenter = person("carpenter", "carpenter at the yard", {"yard", 2.5, 2.5});
        carpenter.home = {"yard_flat", 2.5, 2.5};
        roster.residents = {carpenter, person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian"),
                            person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian")};
        roster.economy.treasury = 5000;
        Society s(Roster::None);
        s.configure(roster);
        double day = 8. / 24;
        work(s, 60, day);                           // (Its till and the purses founded.)
        const auto till = s.tillOf("carpenter");
        {
            auto state = s.state();
            state.memory.piecesFounded = 1;
            state.accounts.at("ash").stock["rapier"] = 1;
            state.accounts.at("ash").stock["straw_hat"] = 4;
            auto& yard = state.accounts.at(till).stock;
            for (auto it = yard.begin(); it != yard.end();)
                it = !Society::edible(it->first) ? yard.erase(it) : std::next(it);   // (Nothing to make planks from.)
            yard["planks"] = planks;
            expect(s.restore(state), "the household owns all it wants");
        }
        expect(s.shift("treasury", "ash", "", 0, 1000, "test: a good year"), "the household has money");
        work(s, 9 * 3600, day);                     // (A day at the yard: its hours in hand.)
        const auto rate = [&] {
            const auto* rates = s.sellRates(till);
            return rates && rates->count("planks") ? rates->at("planks") : 0.;
        };
        const double before = rate();
        auto state = s.state();
        state.memory.wantFund["homes"] = 200;
        expect(s.restore(state), "the household's fund restores");
        s.takeJournal();
        day = 1 + 10. / 24;
        work(s, 1, day);
        int used = 0;
        for (const auto& e : s.takeJournal())
            used += e.kind == "used in works on the home" && e.from == till ? e.quantity : 0;
        const int left = held(s, "carpenter", "planks");
        day = 2 + 10. / 24;
        work(s, 1, day);
        return std::tuple{used, left, before, rate()};
    };
    const auto [used, left, before, after] = worksWith(20);
    expect(used > 0 && left == 20 - used && before == 0 && std::abs(after - .3 * used) < 1e-9,
           "works from twenty planks: " + std::to_string(used) + " used, as sold (" + std::to_string(after / .3) + ")");
    const auto [allUsed, none, was, now] = worksWith(used);
    expect(allUsed == used && none == 0 && was == 0 && now >= .3 * (used + std::max(1., used * .5)) - 1e-9,
           "the same works from its last planks: a sell-out, half again (" + std::to_string(now / .3) + " counted for " + std::to_string(used) + ")");
    Society::setWorkByHours(false);
}

// Works on a home are the building trades' (doc 64, Phase 13): a smith's nails and fittings are a few of its wares, and
// its hands are for its tools and blades. A household with all it wants, in a town with a smith and no builder, has none
// done (its fund waits, clamped).
void worksFromTheBuildingTrades()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    auto wren = person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian");
    auto ash = person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian");
    auto smith = person("smith", "the smith at the forge", {"forge", 2.5, 2.5});
    smith.home = {"forge_flat", 2.5, 2.5};
    roster.residents = {smith, wren, ash};
    roster.economy.treasury = 5000;
    Society s(Roster::None);
    s.configure(roster);
    double day = 10. / 24;
    work(s, 60, day);
    set(s, "smith", "nails", 40);
    set(s, "smith", "fittings", 40);
    {
        auto state = s.state();
        state.accounts.at("ash").stock["rapier"] = 1;
        state.accounts.at("ash").stock["straw_hat"] = 4;
        state.memory.wantFund["homes"] = 200;
        state.memory.piecesFounded = 1;
        expect(s.restore(state), "a household that owns all it wants restores");
    }
    expect(s.shift("treasury", "ash", "", 0, 1000, "test: a good year"), "the household has money");
    s.takeJournal();
    for (int d = 1; d <= 2; ++d)
    {
        day = d + 10. / 24;
        work(s, 60, day);
    }
    const auto journal = s.takeJournal();
    expect(std::none_of(journal.begin(), journal.end(), [](const EconomyEntry& e) { return e.kind == "works on the home"; }),
           "a smith's nails are never works on a home");
    Society::setWorkByHours(false);
}

// Households are founded with pieces of every age (doc 63's model; doc 64, Phase 13), with work by the hours: once, in a
// new world's first day pass (EconomyMemory::piecesFounded, saved), never again though its belongings were never
// counted, and never into a world that has lived (its purses founded).
void piecesFoundedOnce()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    auto keeper = person("keeper", "shopkeeper at the general store", {"store", 2.5, 2.5});
    keeper.home = {"store_flat", 2.5, 2.5};
    roster.residents = {keeper};
    for (const char* who : {"wren", "ash", "fern", "moss", "reed", "sedge"})
    {
        auto r = person(who, "weaving at home", {std::string(who) + "_house", 2.5, 2.5}, "civilian");
        r.home = r.work;
        roster.residents.push_back(r);
    }
    roster.economy.treasury = 5000;
    const auto founded = [](const std::vector<EconomyEntry>& journal) {
        int n = 0;
        for (const auto& e : journal)
            n += e.kind == "belongings to start with" ? e.quantity : 0;
        return n;
    };
    {
        Society s(Roster::None);
        s.configure(roster);
        double day = 10. / 24;
        work(s, 60, day);
        s.takeJournal();
        day = 1 + 10. / 24;
        work(s, 60, day);
        expect(founded(s.takeJournal()) > 0 && s.state().memory.piecesFounded == 1, "a new world's households are founded with pieces");
        auto state = s.state();
        state.memory.belongings.clear();
        expect(s.restore(state), "a world whose belongings were never counted restores");
        day = 2 + 10. / 24;
        work(s, 60, day);
        expect(founded(s.takeJournal()) == 0, "once: never again");
    }
    {
        Society s(Roster::None);
        s.configure(roster);
        double day = 10. / 24;
        work(s, 60, day);
        auto state = s.state();
        state.memory.purses = Society::PursesFounded;   // (Its realm's money stands: an older save.)
        expect(state.memory.piecesFounded == 0 && s.restore(state), "a world that has lived restores");
        s.takeJournal();
        day = 1 + 10. / 24;
        work(s, 60, day);
        expect(founded(s.takeJournal()) == 0 && s.state().memory.piecesFounded == 1, "and is never given pieces from nothing");
    }
    Society::setWorkByHours(false);
}

// With the hours off (the game's default, the old pace, as it was: doc 64), a household's wants are its fancies alone: no
// piece wanted first (a sword, finery to a piece a head), no works on its home, nothing founded. Each grown wolf has its
// own, from its share of the fund (not one a household). Finery is one of its fancies, kept while its buyer holds
// FineryKept pieces or fewer and used past them (nothing wears or is mended).
void wantsAtTheOldPace()
{
    AuthoredRoster roster;
    auto wren = person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian");
    auto ash = person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian");
    auto keeper = person("keeper", "shopkeeper at the general store", {"store", 2.5, 2.5});
    auto smith = person("smith", "the smith at the forge", {"forge", 2.5, 2.5});
    auto carpenter = person("carpenter", "carpenter at the yard", {"yard", 2.5, 2.5});
    keeper.home = {"store_flat", 2.5, 2.5};
    smith.home = {"forge_flat", 2.5, 2.5};
    carpenter.home = {"yard_flat", 2.5, 2.5};
    roster.residents = {keeper, smith, carpenter, wren, ash};
    roster.economy.treasury = 5000;
    Society s(Roster::None);
    s.configure(roster);
    double day = 10. / 24;
    work(s, 60, day);
    set(s, "smith", "sword", 20);
    set(s, "carpenter", "planks", 20);
    set(s, "keeper", "straw_hat", 40);
    expect(s.shift("treasury", "ash", "", 0, 1000, "test: a good year"), "the household has money");
    const auto members = [](const std::string& who) { return who == "wren" || who == "ash"; };
    bool piece = false, works = false, founded = false, both = false;
    // (Until a day it fancies finery: kept, its buyer holding fewer; then, each holding its few (Society::FineryKept), used.)
    constexpr int FineryKept = 6;
    for (const int held : {0, FineryKept})
    {
        {
            auto state = s.state();
            for (const char* who : {"wren", "ash"})
            {
                auto& stock = state.accounts.at(who).stock;
                for (auto it = stock.begin(); it != stock.end();)
                    it = Society::belongingKind(it->first) == "finery" ? stock.erase(it) : std::next(it);
                stock["straw_hat"] = held;
            }
            expect(s.restore(state), "the household's finery restores");
        }
        std::string bought, buyer;
        bool used = false;
        for (int d = 1; d <= 30 && bought.empty(); ++d)
        {
            {
                auto state = s.state();
                state.memory.wantFund["homes"] = 100;
                expect(s.restore(state), "the household's want fund restores");
            }
            s.takeJournal();
            day = std::floor(day) + 1 + 10. / 24;
            work(s, 60, day);
            std::set<std::string> wanting;
            for (const auto& e : s.takeJournal())
            {
                if (e.kind.rfind("a want: ", 0) == 0 && members(e.from))
                    wanting.insert(e.from);
                piece |= e.kind == "a want: a sword" || e.kind == "a want: a rapier";
                works |= e.kind == "works on the home";
                founded |= e.kind == "belongings to start with";
                if ((e.kind == "a want: finery" || e.kind == "a want: jewellery") && members(e.from))
                    bought = e.item, buyer = e.from;
                used |= e.kind == "used" && members(e.from) && Society::belongingKind(e.item) == "finery";
            }
            both |= wanting.size() == 2;
        }
        expect(!bought.empty(), "the household fancies finery now and then: " + std::to_string(held) + " held");
        int finery = 0;
        for (const auto& [item, n] : s.account(buyer)->stock)
            finery += Society::belongingKind(item) == "finery" ? n : 0;
        if (held < FineryKept)
            expect(!used && finery == held + 1, "kept, its buyer holding few: " + bought);
        else
            expect(used && finery == held, "used, its buyer holding its few already: " + bought);
    }
    expect(!piece && !works && !founded, "no piece wanted first, no works on its home and nothing founded, with the hours off");
    expect(both, "and each grown wolf has a want of its own: some days both buy");
}

// A hungry child's bite is what a food shop sells (doc 64, Phase 13): never what it makes things from, though it is food,
// as a provisioner's rabbits for smoking; its fish, which it smokes and sells as they are, it does.
void aBiteNotTheMakersMaterials()
{
    AuthoredRoster roster;
    auto provisioner = person("provisioner", "the provisioner by the gate", {"provisions", 2.5, 2.5});
    provisioner.home = {"provisions_flat", 2.5, 2.5};
    auto ma = person("ma", "weaving at home", {"homes", 3.5, 2.5}, "civilian");
    auto pip = person("pip", "plays in the streets", {"homes", 4.5, 2.5}, "civilian");
    pip.age = 8;
    roster.residents = {provisioner, ma, pip};
    roster.economy.treasury = 500;
    Society s(Roster::None);
    s.configure(roster);
    LifeDay town;                                    // (One town, as the world's day plans give it: the church's alms go by it.)
    town.communityOf = [](const std::string&) { return std::string("town"); };
    s.setDay(town);
    auto bodies = atWork(s);
    bodies["pip"].age = 8;
    const auto run = [&](int seconds, double& day) {
        for (int i = 0; i < seconds; ++i)
        {
            day += 1. / 86400;
            s.tick(1, day, 0, bodies);
            expect(s.conserved(), "money stays conserved");
        }
    };
    double day = 10. / 24;
    run(60, day);
    expect(s.shift("treasury", "pip", "", 0, 20, "test: a child's pennies"), "the child has pennies");
    for (const bool fish : {false, true})
    {
        {
            auto state = s.state();
            auto& shelves = state.accounts.at(s.tillOf("provisioner")).stock;
            shelves = {{"rabbit", 20}};             // (For its smoked rabbit, and none smoked yet.)
            if (fish)
                shelves["fish"] = 20;
            state.accounts.at("pip").stock.clear();
            state.residents.at("pip").hunger = 80;
            expect(s.restore(state), "a hungry child and a provisioner with its rabbits restore");
        }
        s.takeJournal();
        day = std::floor(day) + 1 + 10. / 24;
        run(1, day);
        std::string bite;
        for (const auto& e : s.takeJournal())
            if (e.kind == "a child's spending" && e.from == "pip")
                bite = items::baseOf(e.item);
        if (fish)
            expect(bite == "fish", "the child buys the provisioner's fish, its wares: " + bite);
        else
            expect(bite.empty(), "the child never buys the rabbits it smokes: " + bite);
    }
    // Nor the church, buying alms for the hungry (buyForSurplus): a penniless child with nothing in its larder is fed from
    // the town's shops, the last of their food too, but never from the provisioner's rabbits.
    expect(s.shift("pip", "treasury", "", 0, s.account("pip")->cash, "test: pennies spent"), "the child spends its pennies");
    s.openAccount(Society::SharedChurch);           // (A town without a church of its own: the land's.)
    expect(s.shift("treasury", Society::SharedChurch, "", 0, 100, "test: the plate"), "the church has alms to give");
    for (const bool fish : {false, true})
    {
        {
            auto state = s.state();
            auto& shelves = state.accounts.at(s.tillOf("provisioner")).stock;
            shelves = {{"rabbit", 20}};
            if (fish)
                shelves["fish"] = 20;
            state.accounts.at("pip").stock.clear();
            if (const auto larder = state.accounts.find(Society::homeStore("homes", "larder")); larder != state.accounts.end())
                larder->second.stock.clear();
            expect(s.restore(state), "a penniless child and a bare larder restore");
        }
        s.takeJournal();
        day = std::floor(day) + 1 + 10. / 24;
        run(1, day);
        std::string alms;
        for (const auto& e : s.takeJournal())
            if (e.kind == "alms bought for the hungry")
                alms += (alms.empty() ? "" : ", ") + items::baseOf(e.item);
        if (fish)
            expect(alms == "fish", "the church buys the provisioner's fish for the hungry: " + alms);
        else
            expect(alms.empty(), "never the rabbits it smokes: " + alms);
    }
}


// The household rule keeps its want fund to what the household holds over its target, a month's income and
// HomeNeedDays of its needs, every day (doc 64, Phase 13, with work by the hours: a fund clamped only on a day with
// nothing to spare counted the same money again each day). With the hours off, the old pace, as it was: only on a day
// with nothing to spare.
void theWantFundClamped()
{
    for (const bool hours : {true, false})
    {
        Society::setWorkByHours(hours);
        AuthoredRoster roster;
        roster.residents = {person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian"),
                            person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian")};
        roster.economy.treasury = 5000;
        Society s(Roster::None);
        s.configure(roster);
        double day = 10. / 24;
        work(s, 60, day);
        expect(s.shift("treasury", "ash", "", 0, 1000, "test: a good year"), "the household has money to spare");
        {
            auto state = s.state();
            state.memory.wantFund["homes"] = 1e6;
            expect(s.restore(state), "an overfull want fund restores");
        }
        day = 1 + 10. / 24;
        work(s, 60, day);
        const auto& funds = s.state().memory.wantFund;
        const double fund = funds.count("homes") ? funds.at("homes") : 0.;
        const auto held = s.account("wren")->cash + s.account("ash")->cash;
        const double target = s.monthIncomeOf("wren") + s.monthIncomeOf("ash") + Society::HomeNeedDays * 2. * double(Society::FoodADay);
        if (hours)
            expect(fund > 0 && fund <= double(held) - target + 1e-6,
                   "on a day with money to spare, the fund is no more than the household holds over its target: " + std::to_string(fund) + " of " +
                       std::to_string(held) + "p, its target " + std::to_string(target) + "p");
        else
            expect(fund > double(held), "at the old pace it isn't clamped on a day with money to spare, as it was: " + std::to_string(fund));
    }
    Society::setWorkByHours(false);
}

// A treat is never bought in a town gone hungry (doc 64, Phase 13, with work by the hours: what's on its shelves is for
// the hungry). A butcher's raw meat is a treat to a household with its fund, now and then; never while a wolf of its
// town is at 70 (the monitor's hungry), and at 69 still. With the hours off, the old pace, as it was: hungry or not.
void noTreatsInAHungryTown()
{
    for (const bool hours : {true, false})
    {
        Society::setWorkByHours(hours);
        AuthoredRoster roster;
        auto butcher = person("butcher", "the butcher on the corner", {"shambles", 2.5, 2.5});
        butcher.home = {"shambles_flat", 2.5, 2.5};
        roster.residents = {butcher, person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian"),
                            person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian")};
        roster.economy.treasury = 5000;
        Society s(Roster::None);
        s.configure(roster);
        double day = 10. / 24;
        work(s, 60, day);
        expect(s.shift("treasury", "ash", "", 0, 1000, "test: a good year"), "the household has money");
        const auto members = [](const std::string& who) { return who == "wren" || who == "ash"; };
        for (const bool hungry : {false, true})
        {
            bool treat = false;
            for (int d = 1; d <= 30 && !treat; ++d)
            {
                {
                    auto state = s.state();
                    state.accounts.at(s.tillOf("butcher")).stock["raw_meat"] = 30;
                    state.memory.wantFund["homes"] = 100;
                    state.residents.at("butcher").hunger = hungry ? 70 : 69;
                    expect(s.restore(state), "the butcher's meat and the household's fund restore");
                }
                s.takeJournal();
                day = std::floor(day) + 1 + 10. / 24;
                work(s, 60, day);
                for (const auto& e : s.takeJournal())
                    treat |= e.kind == "a want: a treat" && members(e.from);
            }
            if (hours)
                expect(treat != hungry, hungry ? "never a treat in a town gone hungry, a wolf at 70"
                                               : "a household with its fund has a treat now and then, a wolf at 69");
            else
                expect(treat, std::string("at the old pace a treat now and then, as it was: a wolf at ") + (hungry ? "70" : "69"));
        }
    }
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

// The labour market unpinned (doc 64, Phase 13; doc 63's model, market_wage): the going rate is the middle of what the
// unskilled trades that make things pay, each as a labourer's day; a stall's wage, which no market moves, isn't in it.
// In a land this small (no town with TownRateWeight hands) every town has the realm's, and it is saved.
void theGoingRateFromTheMakers()
{
    Society::setWorkByHours(true);                  // (At the seconds' pace, the old measure: its shops'.)
    auto s = town();
    const auto scribe = s.tillOf("scribe"), stall = s.tillOf("stall");
    expect(s.makesThings(scribe) && !s.makesThings(stall), "the scribe makes things, the stall sells");
    auto state = s.state();
    state.memory.employers[scribe] = {15 * (1 + s.employerSkill(scribe) / items::ladder().skillWage)};
    state.memory.employers[stall] = {12};
    expect(s.restore(state), "the employers' books restore");
    double day = 1 + 10. / 24;
    work(s, 1, day);
    const auto& going = s.state().memory.going;
    expect(going.size() == 1 && going.count("") && std::abs(going.at("") - 15) < 1e-6,
           "the going rate is the scribe's labourer's day, not the stall's: " + std::to_string(going.empty() ? 0. : going.begin()->second));
    expect(std::abs(s.labourDay("anywhere") - 15) < 1e-6, "and every town has the realm's");
    auto again = town();
    expect(again.restore(wire::readSociety(wire::society(s.state()))), "the saved society restores");
    expect(std::abs(again.labourDay("") - 15) < 1e-6, "with its going rate, before the day's first measure");
    Society::setWorkByHours(false);
}

// At the seconds' pace the going rate is each town's, as it was before the hours (doc 64, Phase 13): a town whose
// unskilled shops have nobody working keeps the rate it last had, not the realm's.
void aTownKeepsItsRateAtTheOldPace()
{
    auto s = farmTown();
    const auto far = s.tillOf("farstall");
    auto state = s.state();
    for (const auto* keeper : {"miller", "baker", "stall"})
        state.memory.employers[s.tillOf(keeper)] = {10};
    state.memory.employers[far] = {20};
    expect(s.restore(state), "the employers' books restore");
    double day = 1 + 10. / 24;
    work(s, 1, day);
    const auto rate = [&](const char* town) {
        const auto& going = s.state().memory.going;
        return going.count(town) ? going.at(town) : 0.;
    };
    expect(std::abs(rate("far") - 20) < 1e-6 && std::abs(rate("town") - 10) < 1e-6,
           "each town its own shops' rate: " + std::to_string(rate("far")) + " far, " + std::to_string(rate("town")) + " in town");
    state = s.state();
    for (auto& [id, post] : state.careers.positions)
        if (post.holder == "farstall")
            post.holder.clear();
    expect(s.restore(state), "the far stall's keeper gone");
    day = 2 + 10. / 24;
    work(s, 1, day);
    expect(!s.jobOf("farstall") && std::abs(rate("far") - 20) < 1e-6,
           "the far town, nobody at its stall, keeps the rate it had: " + std::to_string(rate("far")));
}

// Hands by what can be made (doc 64, Phase 13; doc 63's model), with work by the hours: a tannery's leather, which three
// recipes make (from hides, sheepskins or pelts), counted once, by the first with its materials in hand; and a bakery
// short of flour, with nobody to sell it any, wants no hands for the bread it can't bake.
void handsByWhatCanBeMade()
{
    Society::setWorkByHours(true);
    // A good's sales over a fortnight (the buyer uses them up), at the calibrated hours: its rate a day.
    const auto selling = [](Society& s, const std::string& keeper, const std::string& good, int n) {
        auto state = s.state();
        state.memory.pace = 1;
        expect(s.restore(state), "the calibrated hours restore");
        const auto till = s.tillOf(keeper);
        expect(s.shift("treasury", "scribe", "", 0, 14 * n, "test: the buyer's money"), "the buyer has money");
        for (std::int64_t d = 1; d <= 14; ++d)
        {
            set(s, keeper, good, n);
            expect(s.sale(till, "scribe", good, n, 1, "test: the day's buying"), "the buyer buys");
            auto st = s.state();
            st.accounts.at("scribe").stock.erase(good);
            expect(s.restore(st), "used up");
            s.spoil(d);
        }
        return s.sellRates(till)->at(good);
    };
    AuthoredRoster roster;
    roster.residents = {person("tanner", "the tanner at the pits", {"pits", 2.5, 2.5}), person("scribe", "the scribe", {"desk", 2.5, 2.5})};
    roster.economy.treasury = 1000;
    Society tannery(Roster::None);
    tannery.configure(roster);
    const double rate = selling(tannery, "tanner", "leather", 30);
    for (const char* in : {"hide", "oak_bark", "sheepskin", "small_pelt"})
        set(tannery, "tanner", in, 10);
    const auto each = [](const char* recipe) {
        for (const auto* k : items::craftsFor("tannery"))
            if (k->id == recipe)
                return k->hours / k->out.front().second;
        return 0.;
    };
    const double first = each("leather"), all = first + each("leather_sheepskin") + each("leather_pelts");
    const int wanted = tannery.handsWanted(tannery.tillOf("tanner"));
    // (At most half again near bare and a fifth over for a staple, by the first recipe; the three summed, a fifth less.)
    const int most = int(std::ceil(rate * 1.5 * Society::StapleSlack * first / Society::PaidSpells));
    expect(first > 0 && rate > 0 && wanted <= most && std::ceil(rate * .8 * all / Society::PaidSpells) > most,
           "leather counted once, by the first recipe with its materials in hand: " + std::to_string(wanted) + " hands");
    auto s = town();
    const auto bakery = s.tillOf("baker");
    selling(s, "baker", "bread", 30);
    const int able = s.handsWanted(bakery);
    expect(able >= 2, "with its flour, the bakery wants hands for its bread: " + std::to_string(able));
    set(s, "baker", "bread", 0);
    set(s, "baker", "flour", 0);
    set(s, "stall", "flour", 0);
    double day = 15 + 10. / 24;
    work(s, 120, day);
    expect(s.handsWanted(bakery) == 1, "short of flour with nobody to sell it, it wants its keeper: " + std::to_string(s.handsWanted(bakery)));
    Society::setWorkByHours(false);
}

// A business short of hands it can't hire (doc 64, Phase 13; doc 63's model, whose posts are the hands wanted less those
// had): a bakery whose bread sells for more hands than its baker, its till and its keeper's purse emptied each evening so
// it can post no hire, stands short all the same. Its posts are open, day after day, and its weekly review bids its
// wage up. (Before, only a hire's slots nobody took were open: no hire, no post open, no raise.)
void aShortBusinessBidsUp()
{
    Society::setWorkByHours(true);
    auto s = town();
    auto state = s.state();
    state.memory.pace = 1;
    expect(s.restore(state), "the calibrated hours restore");
    const auto bakery = s.tillOf("baker");
    const auto bodies = atWork(s);
    // (Thirty loaves bought each day and eaten; then the day's money and the baker's own taken away.)
    const auto aDay = [&](std::int64_t d) {
        set(s, "baker", "bread", 30);
        set(s, "baker", "flour", 40);
        expect(s.shift("treasury", "scribe", "", 0, 30 * s.shopPrice("baker", "bread"), "test: the buyer's money"), "the buyer has money");
        expect(s.sale(bakery, "scribe", "bread", 30, s.shopPrice("baker", "bread"), "test: the day's buying"), "the buyer buys");
        auto st = s.state();
        st.accounts.at("scribe").stock.erase("bread");
        expect(s.restore(st), "eaten");
        for (const auto& who : {bakery, std::string("baker")})
            if (const auto cash = s.account(who)->cash; cash > 0)
                expect(s.shift(who, "treasury", "", 0, cash, "test: the evening's money taken"), "its money taken");
        s.tick(1, double(d) + .5, 0, bodies);       // (The day's pass.)
    };
    for (std::int64_t d = 1; d <= 14; ++d)
        aDay(d);
    const double before = s.employerWage(bakery);
    int open = 0, days = 0;
    for (std::int64_t d = 15; d <= 29; ++d)
    {
        aDay(d);
        open = std::max(open, s.vacanciesOf(bakery));
        days += s.vacanciesOf(bakery) > 0;
    }
    expect(s.handsWanted(bakery) >= 2 && s.handsOf(bakery) == 1, "the bakery's bread wants more hands than its baker: " +
                                                                     std::to_string(s.handsWanted(bakery)));
    expect(open >= 1 && days >= 14, "its posts open though it posts no hire: " + std::to_string(open) + " on " + std::to_string(days) + " days");
    expect(s.employerWage(bakery) > before * (1 + Society::WageUp * .3 - 1e-9),
           "and its review bids its wage up: " + std::to_string(before) + "p to " + std::to_string(s.employerWage(bakery)) + "p");
    expect(s.conserved(), "money stays conserved");
    Society::setWorkByHours(false);
}

// A producer's posts open by its sales as they are (doc 64, Phase 13, the review): a fishery's goods go on to the shops,
// so its shelf is always near bare, and planning half again for that it would stand short for good. Its sales a hand's
// day, though it plans for two: no post open. Its sales three hands' days: two open. Its sales ten and more: three, its
// hand and two more at most (doc 63's model: its posts grow by steps, not leaps). Its till and its purse emptied each evening,
// so it posts no hire.
void aProducersPostsBySales()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    roster.residents = {person("fisher", "fishes the lake", {"lake", 4.5, 4.5}, "civilian"), person("scribe", "the scribe", {"desk", 2.5, 2.5})};
    roster.economy.treasury = 100000;
    Society s(Roster::None);
    s.configure(roster);
    auto state = s.state();
    state.memory.pace = 1;
    expect(s.restore(state), "the calibrated hours restore");
    const auto till = s.tillOf("fisher");
    const auto bodies = atWork(s);
    std::int64_t d = 0;
    // (A fortnight of `n` fish sold a day and eaten; the day's money and the fisher's own taken away.)
    const auto fortnight = [&](int n) {
        int open = 0, least = 1000;
        for (int i = 0; i < 14; ++i)
        {
            ++d;
            set(s, "fisher", "fish", n);
            expect(s.shift("treasury", "scribe", "", 0, n, "test: the buyer's money"), "the buyer has money");
            expect(s.sale(till, "scribe", "fish", n, 1, "test: the day's fish"), "the buyer buys");
            auto st = s.state();
            st.accounts.at("scribe").stock.erase("fish");
            expect(s.restore(st), "eaten");
            for (const auto& who : {till, std::string("fisher")})
                if (const auto cash = s.account(who)->cash; cash > 0)
                    expect(s.shift(who, "treasury", "", 0, cash, "test: the evening's money taken"), "its money taken");
            s.tick(1, double(d) + .5, 0, bodies);   // (The day's pass.)
            if (i >= 7)
                open = std::max(open, s.vacanciesOf(till)), least = std::min(least, s.vacanciesOf(till));
        }
        for (const auto& j : s.oddJobs())
            expect(j.payer != till || j.until < d, "it posts no hire");
        return std::make_pair(least, open);
    };
    const auto one = fortnight(8);
    expect(s.handsOf(till) == 1 && s.handsWanted(till) == 2 && s.handsWanted(till, false) == 1,
           "its sales a hand's day, it plans for two for its bare shelf: " + std::to_string(s.handsWanted(till)) + " and " +
               std::to_string(s.handsWanted(till, false)) + " (" + std::to_string(s.sellRates(till)->at("fish")) + " a day)");
    expect(one.second == 0,"and has no post open: " + std::to_string(one.second));
    const auto three = fortnight(24);
    expect(s.handsWanted(till, false) == 3 && three.first == 2 && three.second == 2,
           "its sales three hands' days: two posts open (" + std::to_string(three.first) + " to " + std::to_string(three.second) + ")");
    const auto many = fortnight(99);         // (A ledger entry holds 99 at most.)
    expect(s.handsWanted(till, false) >= 10 && many.second == 3,
           "its sales " + std::to_string(s.handsWanted(till, false)) + " hands' days: its hand and two more open, " + std::to_string(many.second));
    expect(s.conserved(), "money stays conserved");
    Society::setWorkByHours(false);
}

// A hire taken is no post open (doc 64, Phase 13, the review): a fishery whose sales want three hands' days has its
// own and posts a hire for the rest; three idle wolves of its town take every slot of it, and nothing stands open.
void aHireTakenIsNoPostOpen()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    roster.residents = {person("fisher", "fishes the lake", {"lake", 4.5, 4.5}, "civilian"), person("scribe", "the scribe", {"desk", 2.5, 2.5})};
    for (int i = 1; i <= 3; ++i)
        roster.residents.push_back(person("idle" + std::to_string(i), "idling in the square", {"square", 2.5, 2.5}, "civilian"));
    roster.economy.treasury = 100000;
    Society s(Roster::None);
    s.configure(roster);
    auto state = s.state();
    state.memory.pace = 1;
    expect(s.restore(state), "the calibrated hours restore");
    const auto till = s.tillOf("fisher");
    expect(s.shift("treasury", till, "", 0, 20000, "test: the fishery's money"), "the fishery has money to hire with");
    std::map<std::string, LifeBody> bodies = atWork(s);
    int taken = -1, slots = 0, open = -1, wanted = 0;
    for (std::int64_t d = 1; d <= 28 && open < 0; ++d)
    {
        // (A hire running into today with every slot taken yesterday, before the day's pass counts its posts.)
        int full = -1;
        for (const auto& j : s.oddJobs())
            if (j.payer == till && j.until >= d && j.slots >= 2 && int(j.stage.size()) == j.slots)
                full = j.slots;
        // (The day's pass, then its working hours, the hands at the hire's spot when they hold it.)
        for (double t = 10; t < 12; t += 1. / 60)
        {
            s.tick(1, double(d) + t / 24, 0, bodies);
            if (t == 10 && full > 0 && d >= 8)
            {
                open = s.vacanciesOf(till), slots = full, wanted = s.handsWanted(till, false);
                for (const auto& j : s.oddJobs())
                    if (j.payer == till && j.until >= d)
                        taken = std::max(taken, int(j.stage.size()));
            }
            for (const auto& j : s.oddJobs())
                if (j.payer == till && j.until >= d)
                    for (const auto& [who, stage] : j.stage)
                        bodies[who] = {j.from.cell, j.from.x, j.from.y};
        }
        // (The day's fish sold and eaten, before the next day's pass.)
        set(s, "fisher", "fish", 24);
        expect(s.shift("treasury", "scribe", "", 0, 24, "test: the buyer's money"), "the buyer has money");
        expect(s.sale(till, "scribe", "fish", 24, 1, "test: the day's fish"), "the buyer buys");
        auto st = s.state();
        st.accounts.at("scribe").stock.erase("fish");
        for (auto& [id, life] : st.residents)
            life.hunger = life.fatigue = 0;
        expect(s.restore(st), "eaten");
    }
    expect(wanted == 3 && slots >= 2 && taken == slots,
           "its sales three hands' days, its hire's slots all taken: " + std::to_string(taken) + " of " + std::to_string(slots) + " (" +
               std::to_string(wanted) + " wanted)");
    expect(open == 0, "and no post stands open: " + std::to_string(open));
    expect(s.conserved(), "money stays conserved");
    Society::setWorkByHours(false);
}

// A House's crew (doc 64, Phase 13, round 6; doc 63's model, house_week): a town of 143 grown wolves, House Fell's lord, a
// fishery selling for three hands' days, three bakers (the going rate's makers), 125 scribes who keep their own desks and
// thirteen idle wolves. At the reckoning House Fell posts its twelve retainers and an estate crew of a tenth of the realm's
// workers (14, in gangs of twelve and two; it was twelve at most), the fishery its hire for two the same day. The idle
// take the better-paid: the House's posts when the fishery pays half the going rate, the fishery's first when it pays
// twice (before, a House's crew came after every business's hire, whatever it paid). Its places they don't take stand
// open under the House's key. A crew hand's day mends the town while it isn't in full repair. A House that can't pay a
// day's share shuts that gang; with little left, its next week's crew is what its budget pays (the model's staff_to): one.
void aHouseKeepsItsCrew()
{
    Society::setWorkByHours(true);
    const int idle = 13;
    const auto week = [&](double fisheryWage) {
        AuthoredRoster roster;
        roster.residents = {person("lord", "ruling House Fell", {"hall", 2.5, 2.5}, "civilian"),
                            person("fisher", "fishes the lake", {"lake", 4.5, 4.5}, "civilian")};
        for (int i = 1; i <= idle; ++i)
            roster.residents.push_back(person("idle" + std::to_string(i), "idling in the square", {"square", 2.5, 2.5}, "civilian"));
        for (int i = 1; i <= 3; ++i)
            roster.residents.push_back(person("baker" + std::to_string(i), "baker at The Loaf", {"bakery" + std::to_string(i), 2.5, 2.5}));
        for (int i = 1; i <= 125; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 1000000;
        auto s = std::make_unique<Society>(Roster::None);
        s->configure(roster);
        LifeDay plan;
        plan.communityOf = [](const std::string&) { return std::string("town"); };
        s->setDay(plan);
        auto state = s->state();
        state.memory.pace = 1;
        expect(s->restore(state), "the calibrated hours restore");
        const auto till = s->tillOf("fisher");
        auto bodies = atWork(*s);
        // (The day's fish sold and eaten; the fishery's money, and its fisher's, taken away till the reckoning's eve.)
        for (std::int64_t d = 1; d <= 6; ++d)
        {
            s->tick(1, double(d) + .5 / 24, 0, bodies);    // (The day's pass.)
            set(*s, "fisher", "fish", 24);
            expect(s->shift("treasury", "scribe1", "", 0, 24, "test: the buyer's money"), "the buyer has money");
            expect(s->sale(till, "scribe1", "fish", 24, 1, "test: the day's fish"), "the buyer buys");
            auto st = s->state();
            st.accounts.at("scribe1").stock.erase("fish");
            for (auto& [id, life] : st.residents)
                life.hunger = life.fatigue = 0;
            expect(s->restore(st), "eaten");
            for (const auto& who : {till, std::string("fisher")})
                if (const auto cash = s->account(who)->cash; cash > 0)
                    expect(s->shift(who, "treasury", "", 0, cash, "test: the evening's money taken"), "its money taken");
        }
        // The reckoning's eve: the House's money, the fishery's to hire with, and its wage.
        auto st = s->state();
        st.accounts.at("treasury").cash -= 120000;
        st.accounts.at("house:house_fell").cash += 100000;
        st.accounts.at(till).cash += 20000;
        st.memory.employers[till].wage = fisheryWage * s->goingRate("town");
        expect(s->restore(st), "the eve restores");
        s->tick(1, 7 + .5 / 24, 0, bodies);
        return std::make_pair(std::move(s), bodies);
    };
    const auto ofHouse = [](const Society& s, const std::string& kind) {
        std::vector<const Society::OddJob*> out;
        for (const auto& j : s.oddJobs())
            if (j.payer == "house:house_fell" && j.kind == kind && j.until >= 7)
                out.push_back(&j);
        return out;
    };
    // A working morning to `end`: each minute, the hands at the spot of the hire they hold. (To 11:00 a hand does no
    // share, so it isn't free to take a second place, as it is once its first is done.)
    const auto morning = [](Society& s, std::map<std::string, LifeBody>& bodies, std::int64_t d, double end, const std::function<void()>& first) {
        for (double t = 9; t < end; t += 1. / 60)
        {
            s.tick(60, double(d) + t / 24, 0, bodies);
            if (t == 9 && first)
                first();
            // (At the first place it holds with its share not done, as a search down the list finds it.)
            std::set<std::string> placed;
            for (const auto& j : s.oddJobs())
                for (const auto& [who, stage] : j.stage)
                    if (stage < 2 && !j.ended && placed.insert(who).second)
                        bodies[who] = {j.from.cell, j.from.x, j.from.y};
        }
    };
    // The idle wolves on each payer's hires.
    const auto takers = [&](const Society& s) {
        std::map<std::string, int> on;
        for (int i = 1; i <= idle; ++i)
            for (const auto& j : s.oddJobs())
                if (j.stage.count("idle" + std::to_string(i)) && !j.ended)
                {
                    ++on[j.payer + "|" + j.kind];
                    break;
                }
        return on;
    };
    const auto told = [](const std::map<std::string, int>& on) {
        std::string out;
        for (const auto& [k, n] : on)
            out += k + " " + std::to_string(n) + "; ";
        return out;
    };
    {
        auto [s, bodies] = week(.5);
        const auto till = s->tillOf("fisher");
        expect(s->estateMost() == 14, "a tenth of its 143 grown wolves: " + std::to_string(s->estateMost()));
        const auto crew = ofHouse(*s, "building work");
        int slots = 0, most = 0;
        for (const auto* j : crew)
            slots += j->slots, most = std::max(most, j->slots);
        expect(slots == 14 && crew.size() == 2 && most == Society::CrewGang,
               "House Fell posts an estate crew of 14, in gangs of twelve and two: " + std::to_string(slots) + " in " + std::to_string(crew.size()));
        expect(ofHouse(*s, "about the home").size() == 1 && ofHouse(*s, "about the home").front()->slots == 12, "and its twelve retainers");
        const Society::OddJob* fishery = nullptr;
        for (const auto& j : s->oddJobs())
            if (j.payer == till && j.until >= 7)
                fishery = &j;
        expect(fishery && fishery->slots >= 2 && fishery->pay / fishery->slots < crew.front()->pay / crew.front()->slots,
               "the fishery hires for more hands the same day, at half the going rate: " +
                   (fishery ? std::to_string(fishery->slots) + " at " + std::to_string(fishery->pay / fishery->slots) : std::string("none")) +
                   " against " + std::to_string(crew.front()->pay / crew.front()->slots));
        morning(*s, bodies, 7, 11, {});
        auto on = takers(*s);
        expect(on["house:house_fell|about the home"] == 12 && on["house:house_fell|building work"] == 1 && !on.count(till + "|a hand"),
               "the idle take the better-paid, the House's posts: " + told(on));
        // The next day's pass: the places they didn't take stand open, under the House's key.
        s->tick(1, 8 + .5 / 24, 0, bodies);
        expect(s->vacanciesOf("house:house_fell|crew") == 14 + 12 - idle,
               "its places not taken are posts open: " + std::to_string(s->vacanciesOf("house:house_fell|crew")));
        expect(!s->state().memory.employers.count("house:house_fell|crew"), "and no book is kept: it pays the going rate");
        // Its crew's days mend the town, in disrepair.
        auto st = s->state();
        st.memory.condition["town"] = 90;
        expect(s->restore(st), "the town worn");
        const auto before = s->account("house:house_fell")->cash;
        morning(*s, bodies, 8, 12.5, {});
        int paid = 0, retained = 0;
        for (const auto& j : s->oddJobs())
            for (const auto& [who, stage] : j.stage)
                if (j.payer == "house:house_fell" && stage == 2)
                    (j.kind == "building work" ? paid : retained) += 1;
        expect(before - s->account("house:house_fell")->cash == (paid + retained) * crew.front()->pay / crew.front()->slots,
               "each hand's day paid by the House: " + std::to_string(before - s->account("house:house_fell")->cash));
        expect(paid >= 1 && std::abs(s->condition("town") - (90 + paid * Society::CrewMend)) < 1e-9,
               "its hands' days paid, the town is mended by CrewMend each: " + std::to_string(paid) + " paid, " + std::to_string(s->condition("town")));
        // A House that can't pay: its gangs shut.
        s->tick(1, 9 + .5 / 24, 0, bodies);
        morning(*s, bodies, 9, 12.5, [&] {
            auto empty = s->state();
            empty.accounts.at("treasury").cash += empty.accounts.at("house:house_fell").cash;
            empty.accounts.at("house:house_fell").cash = 0;
            for (auto& [id, life] : empty.residents)
                life.hunger = life.fatigue = 0;
            expect(s->restore(empty), "the House's money gone");
        });
        int shut = 0, open = 0;
        for (const auto& j : s->oddJobs())
            if (j.payer == "house:house_fell" && !j.stage.empty())
                (j.ended && j.until == 8 ? shut : open) += 1;
        expect(shut >= 2 && open == 0, "the House can't pay the day's shares: its gangs are shut, " + std::to_string(shut) + " (" +
                                           std::to_string(open) + " not)");
        // Its next week, with 2,000p left: what its budget pays, one.
        st = s->state();
        st.accounts.at("treasury").cash -= 2000;
        st.accounts.at("house:house_fell").cash = 2000;
        expect(s->restore(st), "a little back");
        for (std::int64_t d = 10; d <= 14; ++d)
            s->tick(1, double(d) + .5 / 24, 0, bodies);
        int next = 0;
        for (const auto& j : s->oddJobs())
            if (j.payer == "house:house_fell" && j.kind == "building work" && j.until >= 14)
                next += j.slots;
        expect(next == 1, "with 2,000p, its next week's crew is what its budget pays: " + std::to_string(next));
        expect(s->conserved(), "money stays conserved");
    }
    {
        auto [s, bodies] = week(2);
        morning(*s, bodies, 7, 11, {});
        auto on = takers(*s);
        int hired = 0;
        for (const auto& j : s->oddJobs())
            if (j.payer == s->tillOf("fisher") && j.until >= 7)
                hired += j.slots;
        expect(hired >= 2 && on[s->tillOf("fisher") + "|a hand"] == hired && on["house:house_fell|about the home"] == idle - hired,
               "paying twice the going rate, the fishery's hire is taken first: " + told(on));
        expect(s->conserved(), "money stays conserved");
    }
    Society::setWorkByHours(false);
}

// One labour market for the land (doc 64, Phase 13; doc 63's model): with work by the hours a fishery's hire, like a
// farm's, may be taken by anyone living in a city, not by its own town's folk alone. At the seconds' pace, as it was: a
// farm's only. A shop's hire is the land's at neither.
void theLandsHiresForAnyone()
{
    AuthoredRoster roster;
    roster.residents = {person("farmer", "farms the valley fields", {"fields", 4.5, 4.5}, "civilian"),
                        person("fisher", "fishes the lake", {"lake", 4.5, 4.5}, "civilian"),
                        person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5})};
    roster.economy.treasury = 500;
    Society s(Roster::None);
    s.configure(roster);
    const auto hireBy = [](const std::string& keeper) {
        Society::OddJob j;
        j.producer = keeper;
        j.kind = "a hand";
        return j;
    };
    expect(items::producerFor("fishes the lake") && !Society::farmWork(items::producerFor("fishes the lake")->id), "the fisher works no farm");
    for (const bool hours : {false, true})
    {
        Society::setWorkByHours(hours);
        const std::string pace = hours ? " (by the hours)" : " (at the seconds' pace)";
        expect(s.farmHire(hireBy("farmer")), "a farm's hire is the land's" + pace);
        expect(s.farmHire(hireBy("fisher")) == hours, "a fishery's is the land's with work by the hours only" + pace);
        expect(!s.farmHire(hireBy("baker")), "a bakery's never" + pace);
    }
    Society::setWorkByHours(false);
}

// The cord loop (doc 64, Phase 13): a chandler short of cord for its candles buys it as a supplier does, from the makers
// (another chandler), never from the general store, which supplies cord and only bought it back from the chandlers,
// round and round; and the other chandler keeps back what its own batches need.
void theCordGoesRoundNoMore()
{
    AuthoredRoster roster;
    roster.residents = {person("chandler", "the chandler at the kettle", {"kettle", 2.5, 2.5}),
                        person("other", "the chandler by the wharf", {"wharf", 2.5, 2.5}),
                        person("shop", "keeping the general goods", {"shop", 2.5, 2.5})};
    roster.economy.treasury = 500;
    Society s(Roster::None);
    s.configure(roster);
    set(s, "chandler", "cord", 0);
    set(s, "chandler", "candle_tallow", 0);
    set(s, "shop", "cord", Society::SuppliesKept);
    set(s, "other", "cord", 44);
    for (const char* full : {"candle_tallow", "candle_wax", "fishing_net"})
        set(s, "other", full, 40);              // (Its own shelves full: it uses none of its cord.)
    s.takeJournal();
    double day = 10. / 24;
    work(s, 300, day);
    int fromShop = 0, fromOther = 0;
    for (const auto& e : s.takeJournal())
        if (e.kind == "materials bought" && items::baseOf(e.item) == "cord" && e.from == s.tillOf("chandler"))
            (e.to == s.tillOf("shop") ? fromShop : fromOther) += e.quantity;
    expect(fromShop == 0, "the chandler buys no cord from the general store: " + std::to_string(fromShop));
    expect(fromOther > 0 && held(s, "other", "cord") >= s.keepsBack("other", "cord") && s.keepsBack("other", "cord") > Society::GoodsKept,
           "but from the other chandler, which keeps back its own batches': " + std::to_string(fromOther) + " bought, " +
               std::to_string(held(s, "other", "cord")) + " kept");
}

// A producer with its store full (doc 64, Phase 13), with work by the hours: its yields would bring nothing in, so its
// hours aren't spent on them; they stay in hand, and once its work in hand is full they are lost. Full at one turn of
// the day and the next, it has no work for a hand: not on the first day its work in hand fills.
void aFullStoreLosesItsHours()
{
    Society::setWorkByHours(true);
    auto s = farmTown();
    for (const auto& [item, n] : items::producerFor("farms the valley fields")->out)
        set(s, "farmer", item, 2000);
    s.takeJournal();
    const auto by = [&] {
        const auto found = s.workBy().find("farmer");
        return found == s.workBy().end() ? 0. : found->second.second;
    };
    double day = 8. / 24;
    work(s, 9 * 3600, day);
    expect(count(s.takeJournal(), "brought in", "wheat") == 0, "its store full, nothing is brought in");
    expect(by() > 0 && !s.idleBusiness(s.tillOf("farmer")),
           "and its hours are lost, not spent; but full only since this morning, it isn't idle yet: " + std::to_string(by()));
    day = 1 + 8. / 24;
    work(s, 3600, day);
    expect(s.idleBusiness(s.tillOf("farmer")), "full at the turn of the day too, it is");
    Society::setWorkByHours(false);
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

// A food maker's materials (doc 64, Phase 13; the model's lay_in): a bakery selling sixty loaves a day lays in a week of
// flour (bread is a staple) once a day, its shelf full or not, from what its till can spare and never on credit; a smith's
// bronze stays at its batches' store.
void foodMakersLayIn()
{
    auto s = town();
    const auto till = s.tillOf("baker");
    for (std::int64_t d = 1; d <= 7; ++d)
    {
        set(s, "baker", "bread", 60);
        expect(s.sale(till, "scribe", "bread", 60, 1, "test: the morning's queue") && s.shift(till, "scribe", "", 0, 60, "test: the scribe's wage"),
               "the scribe's household buys sixty loaves");
        s.spoil(d);
        auto st = s.state();                        // (The bread eaten: conserved, it's only goods.)
        st.accounts.at("scribe").stock.erase("bread");
        expect(s.restore(st), "the bread eaten");
    }
    const auto* bread = items::craftsFor("bakery").front();
    const int week = int(std::ceil(s.useRate(till, "flour") * Society::StapleInputDays));
    expect(bread->out.front().first == "bread" && s.inputStore(till, *bread, "flour", Society::MaterialBatches) == week &&
               week > Society::MaterialBatches * 4,
           "its store of flour is a week of its use: " + std::to_string(week));
    const auto* smithy = items::businessFor("the smith at the forge");
    const auto forged = smithy ? items::craftsFor(smithy->id) : std::vector<const items::Craft*>{};
    expect(!forged.empty() && s.inputStore(s.tillOf("smith"), *forged.front(), forged.front()->in.front().first, 8) == 8,
           "a smith's materials are its batches' store");
    set(s, "baker", "bread", Society::ShelfMost);    // (A full shelf: nothing to bake.)
    set(s, "baker", "flour", 0);
    double day = 8 + 10. / 24;
    work(s, 60, day);
    const int laid = held(s, "baker", "flour");
    expect(laid > Society::MaterialBatches && laid <= week + Society::GoodsKept && held(s, "stall", "flour") < Society::SuppliesKept,
           "its shelf full, it lays in flour from the stall all the same: " + std::to_string(laid));
    set(s, "baker", "flour", 0);
    work(s, 600, day);
    expect(held(s, "baker", "flour") == 0, "once a day");
    auto broke = town();
    const auto broketill = broke.tillOf("baker");
    for (std::int64_t d = 1; d <= 7; ++d)
    {
        set(broke, "baker", "bread", 60);
        expect(broke.sale(broketill, "scribe", "bread", 60, 1, "test: the morning's queue") &&
                   broke.shift(broketill, "scribe", "", 0, 60, "test: the scribe's wage"),
               "the scribe's household buys sixty loaves");
        broke.spoil(d);
    }
    set(broke, "baker", "bread", Society::ShelfMost);
    set(broke, "baker", "flour", 0);
    for (const auto& who : {broketill, std::string("baker")})   // (Its till and its owner, who would top it up.)
        if (const auto cash = broke.account(who)->cash; cash > 0)
            broke.shift(who, "treasury", "", 0, cash, "test: the till emptied");
    const auto owedBefore = broke.owed(broketill);
    double then = 8 + 10. / 24;
    broke.takeJournal();
    work(broke, 60, then);
    // (What the day brings its till, a cake sold or its owner's top-up, it may lay in with; never more, on credit.)
    std::int64_t paid = 0;
    for (const auto& e : broke.takeJournal())
        if (e.kind == "materials bought" && e.from == broketill && e.item == "flour")
            paid += e.coins;
    expect(broke.owed(broketill) == owedBefore && held(broke, "baker", "flour") <= paid,
           "with nothing in its till it lays in only what it pays for, not on credit");
}

// What a field is planned by (doc 64, Phase 13): its food, what feeds or goes only into food or flour; the straw, hemp,
// hay and firewood come with it. An orchard's apples (for cider, a drink) are all it grows: it's planned by them.
void fieldsPlannedByTheirFood()
{
    auto s = farmTown();
    const auto* farm = items::producerFor("farms the valley fields");
    const auto* orchard = items::producerFor("tends the orchard");
    expect(farm && orchard, "a farm and an orchard");
    for (const auto* food : {"wheat", "oats", "vegetables"})
        expect(s.grownToEat(food) && s.plannedBy(*farm, food), std::string(food) + " is grown to eat");
    for (const auto* other : {"straw", "hemp", "flax", "hay", "firewood", "apples"})
        expect(!s.grownToEat(other) && !s.plannedBy(*farm, other), std::string(other) + " comes with the farm's food");
    expect(s.plannedBy(*orchard, "apples"), "an orchard's apples are what it is planned by");
    expect(Society::rots(items::good("fish")) && Society::rots(items::good("bread")) && !Society::rots(items::good("milk")) &&
               !Society::rots(items::good("wheat")),
           "fish and bread rot; milk and wheat don't");
    // Planned by its food: a farm selling hay by the hundred and a little wheat wants acres for its wheat.
    const auto till = s.tillOf("farmer");
    set(s, "farmer", "wheat", 8);                   // (Not sold out: no missed buyers counted.)
    set(s, "farmer", "hay", 99);
    expect(s.sale(till, "miller", "wheat", 4, 1, "test: the mill's order") && s.sale(till, "baker", "hay", 90, 1, "test: the horses' hay"),
           "the farm sells hay and wheat");
    s.spoil(1);
    expect(std::abs(s.fieldNeed(till, *farm) - .3 * 4 / 2) < 1e-9, "its acres go by its wheat, not its hay: " + std::to_string(s.fieldNeed(till, *farm)));
}

// A want none of its town's sellers could meet (doc 64, Phase 13; the model's unmet asks) is a sale its producers missed:
// the mill short of wheat with the farm's barn empty tells the farm, once a day, and the farm's sales a day go up by it.
void missedSalesReachTheLand()
{
    auto s = farmTown();
    const auto mill = s.tillOf("miller"), farm = s.tillOf("farmer");
    for (std::int64_t d = 1; d <= 3; ++d)
    {
        set(s, "miller", "flour", 40);
        expect(s.sale(mill, "stall", "flour", 40, 1, "test: the stall's flour") && s.shift(mill, "stall", "", 0, 40, "test: the stall's takings"),
               "the stall buys the mill's flour");
        s.spoil(d);
    }
    expect(s.sellRates(farm) == nullptr || !s.sellRates(farm)->count("wheat"), "the farm has sold no wheat");
    set(s, "farmer", "wheat", 0);
    set(s, "miller", "wheat", 0);
    set(s, "miller", "flour", 0);
    set(s, "stall", "flour", 0);
    auto bodies = atWork(s);
    bodies["farmer"] = {"farmhouse", 2.5, 2.5};     // (At home: bringing nothing in.)
    for (double day = 4 + 10. / 24, end = day + 120. / 86400; day < end;)
    {
        day += 1. / 86400;
        s.tick(1, day, 0, bodies);
    }
    const auto* rates = s.sellRates(farm);
    const double use = s.useRate(mill, "wheat");
    expect(held(s, "miller", "wheat") == 0 && (!rates || !rates->count("wheat")) && use > 0, "the mill finds no wheat");
    s.spoil(5);
    rates = s.sellRates(farm);
    const double missed = rates && rates->count("wheat") ? rates->at("wheat") : 0;
    expect(missed > 0 && missed <= .3 * std::ceil(use) + 1e-9, "a day's use of wheat the mill went without is a sale the farm missed: " +
                                                                   std::to_string(missed) + " (the mill uses " + std::to_string(use) + ")");
    expect(s.conserved(), "money stays conserved");
    // A farm that sold its last that day and was told of the mill's want besides counts the buyers it missed or its
    // sold-out half again, the more, not both: they are the same buyers (the missed sales went in with its sales, and
    // the half again was laid on them too).
    auto t = farmTown();
    const auto field = t.tillOf("farmer");
    set(t, "farmer", "wheat", 4);
    expect(t.sale(field, "miller", "wheat", 4, 1, "test: its last") && t.shift(field, "miller", "", 0, 4, "test: the mill's takings"),
           "the mill buys the farm's last four");
    t.missedAt("town", "wheat", 10);
    t.spoil(1);
    const auto* after = t.sellRates(field);
    const double counted = after && after->count("wheat") ? after->at("wheat") : 0;
    expect(std::abs(counted - .3 * (4 + 10)) < 1e-9, "four sold and ten missed: " + std::to_string(counted / .3) + " counted");
}

// A sale missed is counted when the town's sellers have none to sell, as they keep back for themselves (doc 64, Phase
// 13). The mill, its till empty, lays in no wheat though the farm has some: no sale missed, and the day isn't spent on
// it; later that day, its till filled and the farm's barn empty, its want is counted. And a workshop's store of what it
// works with itself (an ironworks' charcoal, its batches' worth; a foundry's, three batches' and more than the few it
// keeps for its shelf) isn't for sale: a smith that finds only that missed it.
void missedOnlyWhenNoneLeft()
{
    auto s = farmTown();
    const auto mill = s.tillOf("miller"), farm = s.tillOf("farmer");
    for (std::int64_t d = 1; d <= 3; ++d)
    {
        set(s, "miller", "flour", 40);
        expect(s.sale(mill, "stall", "flour", 40, 1, "test: the stall's flour") && s.shift(mill, "stall", "", 0, 40, "test: the stall's takings"),
               "the stall buys the mill's flour");
        s.spoil(d);
    }
    set(s, "farmer", "wheat", 200);
    set(s, "miller", "wheat", 0);
    set(s, "miller", "flour", Society::ShelfMost);  // (Its shelf full: it lays in, and makes nothing.)
    set(s, "stall", "flour", Society::SuppliesKept);
    set(s, "baker", "bread", Society::ShelfMost);    // (Nor the baker buying the mill's flour.)
    for (const auto& who : {mill, std::string("miller")})   // (Its till and its owner, who would top it up.)
        if (const auto cash = s.account(who)->cash; cash > 0)
            s.shift(who, "treasury", "", 0, cash, "test: the till emptied");
    auto bodies = atWork(s);
    bodies["farmer"] = {"farmhouse", 2.5, 2.5};     // (At home: bringing nothing in.)
    const auto tick = [&](double from, int seconds) {
        for (double day = from, end = from + seconds / 86400.; day < end;)
        {
            day += 1. / 86400;
            s.tick(1, day, 0, bodies);
            expect(s.conserved(), "money stays conserved");
        }
    };
    tick(4 + 10. / 24, 60);
    expect(held(s, "miller", "wheat") == 0 && held(s, "farmer", "wheat") == 200, "the mill, with nothing in its till, lays in no wheat");
    set(s, "farmer", "wheat", 0);
    set(s, "miller", "flour", 0);
    s.shift("treasury", mill, "", 0, 100, "test: the day's takings");
    tick(4 + 11. / 24, 60);
    s.spoil(5);
    const auto* rates = s.sellRates(farm);
    expect(rates && rates->count("wheat") && rates->at("wheat") > 0, "later that day, the farm's barn empty, the mill's want is a sale it missed");
    // The smith short of charcoal: the ironworks (or the foundry) has only what its own batches keep back (a couple of
    // batches'), and the burner none.
    for (const char* label : {"the ironworks by the river", "the foundry by the river"})
    {
        AuthoredRoster roster;
        roster.residents = {person("smith", "the smith at the forge", {"forge", 2.5, 2.5}),
                            person("works", label, {"works", 2.5, 2.5}),
                            person("burner", "burns charcoal", {"kilns", 4.5, 4.5}, "civilian")};
        roster.economy.treasury = 500;
        Society t(Roster::None);
        t.configure(roster);
        const auto smith = t.tillOf("smith"), burner = t.tillOf("burner");
        const auto* works = items::businessFor(label);
        int keeps = 0;
        for (const auto* k : works ? items::craftsFor(works->id) : std::vector<const items::Craft*>{})
            for (const auto& [in, n] : k->in)
                keeps += in == "charcoal" ? n * Society::MaterialsLow : 0;
        keeps = std::max(keeps, Society::GoodsKept);
        expect(works && (works->id == "ironworks" || (works->id == "foundry" && keeps > Society::GoodsKept)) && items::producerFor("burns charcoal"),
               std::string("a workshop that burns its own charcoal (") + label + "), and a burner");
        for (std::int64_t d = 1; d <= 3; ++d)
        {
            set(t, "smith", "nails", 60);
            expect(t.sale(smith, "burner", "nails", 60, 1, "test: a roof's nails") && t.shift(smith, "burner", "", 0, 60, "test: the burner's wage"),
                   "the burner buys the smith's nails");
            t.spoil(d);
        }
        set(t, "smith", "nails", 0);
        set(t, "smith", "charcoal", 0);
        set(t, "smith", "iron_bar", 40);
        set(t, "works", "charcoal", keeps);
        set(t, "burner", "charcoal", 0);
        auto there = atWork(t);
        there["works"] = there["burner"] = {"homes", 2.5, 2.5};   // (Both at home: neither works.)
        for (double day = 4 + 10. / 24, end = day + 60. / 86400; day < end;)
        {
            day += 1. / 86400;
            t.tick(1, day, 0, there);
        }
        t.spoil(5);
        const auto* burnt = t.sellRates(burner);
        expect(held(t, "works", "charcoal") == keeps && burnt && burnt->count("charcoal") && burnt->at("charcoal") > 0,
               std::string(label) + " keeps its batches' charcoal (" + std::to_string(keeps) + "), and the smith's want is a sale the burner missed");
        expect(t.conserved(), "money stays conserved");
    }
}

// A sale missed is what a buyer went without of a day's use against all it bought that day (doc 64, Phase 13): a mill
// using about forty wheat a day lays in from two farms, one with 99 and one with none. It buys the 99, two days' and
// more, and calling again for the rest finds the other's barn empty: no sale missed there. With 10 at the first, the
// town's farms are told, between them, of the thirty it went without.
void missedAgainstTheDaysBuying()
{
    for (const int first : {99, 10})
    {
        AuthoredRoster roster;
        auto farmer = person("farmer", "farms the valley fields", {"fields", 4.5, 4.5}, "civilian");
        auto other = person("other", "farms the valley fields", {"fields2", 4.5, 4.5}, "civilian");
        farmer.home = other.home = {"farmhouse", 2.5, 2.5};
        roster.residents = {farmer, other, person("miller", "runs The Ford Mill", {"mill", 2.5, 2.5}),
                            person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}), person("stall", "keeping the stall", {"market", 4.5, 4.5})};
        roster.economy.treasury = 5000;
        Society s(Roster::None);
        s.configure(roster);
        LifeDay plans;
        plans.communityOf = [](const std::string& cell) { return cell.rfind("fields", 0) == 0 ? std::string() : std::string("town"); };
        s.setDay(plans);
        const auto mill = s.tillOf("miller"), empty = s.tillOf("other");
        for (std::int64_t d = 1; d <= 3; ++d)
        {
            set(s, "miller", "flour", 40);
            expect(s.sale(mill, "stall", "flour", 40, 1, "test: the stall's flour") && s.shift(mill, "stall", "", 0, 40, "test: the stall's takings"),
                   "the stall buys the mill's flour");
            s.spoil(d);
        }
        set(s, "farmer", "wheat", first);
        set(s, "other", "wheat", 0);
        set(s, "miller", "wheat", 0);
        set(s, "miller", "flour", Society::ShelfMost);   // (Its shelf full: it lays in, and makes nothing.)
        set(s, "stall", "flour", Society::SuppliesKept);
        set(s, "baker", "bread", Society::ShelfMost);
        expect(s.shift("treasury", mill, "", 0, 2000, "test: the mill's takings"), "the mill has money");
        auto bodies = atWork(s);
        bodies["farmer"] = bodies["other"] = {"farmhouse", 2.5, 2.5};   // (At home: bringing nothing in.)
        const double use = s.useRate(mill, "wheat");
        for (double day = 4 + 10. / 24, end = day + 60. / 86400; day < end;)
        {
            day += 1. / 86400;
            s.tick(1, day, 0, bodies);
            expect(s.conserved(), "money stays conserved");
        }
        const int bought = held(s, "miller", "wheat");
        s.spoil(5);
        const auto* rates = s.sellRates(empty);
        const double told = rates && rates->count("wheat") ? rates->at("wheat") / .3 : 0;
        expect(use > 30 && use < 50 && bought == first, "the mill, using " + std::to_string(use) + " wheat a day, buys the first farm's " +
                                                             std::to_string(first) + ": " + std::to_string(bought));
        if (first == 99)
            expect(told == 0, "two days' bought, the empty farm missed no sale: " + std::to_string(told));
        else
            expect(told > 0 && told <= std::ceil(use) - first + 1e-9, "a quarter of a day's bought, the empty farm is told of its share of what it "
                                                                          "went without: " + std::to_string(told));
    }
}

// A field's month's rate through a restart (doc 64, Phase 13): a farm sold seventy wheat once a week, its society rebuilt
// from its save in its founding spring (its running average of sales, not saved, begun again from nothing). Its month's
// rate stays within a twentieth of its sales a day, whatever the weekday (it went up to 1.29 times them, and its spring's
// acres with it).
void aRestartKeepsTheMonth()
{
    Society::setWorkByHours(true);
    const auto* farm = items::producerFor("farms the valley fields");
    const auto wheat = std::find_if(farm->out.begin(), farm->out.end(), [](const auto& o) { return o.first == "wheat"; });
    expect(wheat != farm->out.end(), "a farm grows wheat");
    const double daily = 10. / wheat->second;       // (Ten a day, in yields.)
    double lowest = 1e18, highest = 0;
    for (int weekday = 0; weekday < 7; ++weekday)
    {
        auto s = std::make_unique<Society>(farmTown());
        const auto till = s->tillOf("farmer");
        set(*s, "farmer", "wheat", 900);
        for (std::int64_t d = 1; d < Society::SowEnd - 10; ++d)
        {
            if (d == 40)
            {
                auto rebuilt = std::make_unique<Society>(farmTown());
                expect(rebuilt->restore(s->state()), "the society rebuilt from its save");
                s = std::move(rebuilt);
            }
            if ((d + weekday) % 7 == 0)
                expect(s->sale(till, "miller", "wheat", 70, 1, "test: the mill's week") && s->shift(till, "miller", "", 0, 70, "test: the mill's takings"),
                       "the mill buys its week's wheat");
            s->spoil(d);
            s->tendFields();
            if (!s->state().memory.fields.count(till) && s->foundingRate(till, *farm) > 0)
                s->planField("farmer", till, *farm, d + 1 + 10. / 24, true);
            if (d >= 40 && s->state().memory.fields.count(till))
            {
                const double month = s->state().memory.fields.at(till).month;
                lowest = std::min(lowest, month);
                highest = std::max(highest, month);
            }
        }
    }
    expect(lowest >= daily * .95 && highest <= daily * 1.05, "rebuilt from its save, its month's rate within a twentieth of its sales a day: " +
                                                               std::to_string(lowest / daily) + " to " + std::to_string(highest / daily) + " of them");
    Society::setWorkByHours(false);
}

// A field's year from its sales a day over whole weeks, and its founding spring's looks from its month's (doc 64, Phase
// 13): a farm whose wheat the mill buys seventy at once, once a week, is founded on ten a day whatever the weekday, and
// its weekly looks don't take its acres up to the day after the mill's; seven farms, the mill's day a different one for
// each, plan alike. (The running average of the last few days went from half to nearly twice its sales a day.) And a
// field whose rate isn't known yet (read from a save without it) waits to top up its store.
void fieldsPlannedByTheirMonth()
{
    Society::setWorkByHours(true);
    const auto* farm = items::producerFor("farms the valley fields");
    const auto wheat = std::find_if(farm->out.begin(), farm->out.end(), [](const auto& o) { return o.first == "wheat"; });
    expect(wheat != farm->out.end(), "a farm grows wheat");
    const double daily = 10. / wheat->second;       // (Ten a day, in yields.)
    const double first = daily * (calendar::DaysPerYear + Society::HarvestEnd - Society::HarvestStart) * Society::FieldMargin / Society::FieldYields;
    double lowest = 1e18, highest = 0;
    for (int weekday = 0; weekday < 7; ++weekday)
    {
        auto s = farmTown();
        const auto till = s.tillOf("farmer");
        set(s, "farmer", "wheat", 900);             // (Never its last: no buyers counted after it.)
        double founded = 0;
        for (std::int64_t d = 1; d < Society::SowEnd - 5; ++d)
        {
            if (d % 7 == weekday)
                expect(s.sale(till, "miller", "wheat", 70, 1, "test: the mill's week") &&
                           s.shift(till, "miller", "", 0, 70, "test: the mill's takings"),
                       "the mill buys its week's wheat");
            s.spoil(d);
            s.tendFields();
            if (const double morning = d + 1 + 10. / 24; s.state().memory.fields.count(till))
                s.reviewField("farmer", till, *farm, morning);
            else if (s.foundingRate(till, *farm) > 0)
            {
                founded = s.foundingRate(till, *farm);
                s.planField("farmer", till, *farm, morning, true);
            }
        }
        expect(std::abs(founded - daily) < 1e-9, "founded on ten a day, the mill's day " + std::to_string(weekday) + ": " +
                                                     std::to_string(founded * wheat->second));
        const double acres = s.state().memory.fields.at(till).acres;
        lowest = std::min(lowest, acres);
        highest = std::max(highest, acres);
    }
    expect(lowest >= first - 1e-6 && highest <= first * 1.05, "its founding spring's acres within a twentieth of its founding's, whatever the weekday: " +
                                                                 std::to_string(lowest) + " to " + std::to_string(highest) + ", of " +
                                                                 std::to_string(first));
    auto s = farmTown();
    const auto till = s.tillOf("farmer");
    auto st = s.state();
    FieldYear unknown;
    unknown.acres = 10;
    unknown.year = -1;
    unknown.founded = 1;
    unknown.toppedUp = false;
    st.memory.fields[till] = unknown;
    expect(s.restore(st), "a field founded on day 1, its rates not saved");
    s.takeJournal();
    s.reviewField("farmer", till, *farm, 20.5);
    expect(!s.state().memory.fields.at(till).toppedUp && count(s.takeJournal(), "a year's store", "wheat") == 0,
           "with no rate yet, its store isn't topped up, nor its top-up spent");
    st = s.state();
    st.memory.fields.at(till).month = daily;
    st.memory.fields.at(till).monthDays = 28;
    expect(s.restore(st), "a month of its sales");
    s.reviewField("farmer", till, *farm, 21.5);
    expect(s.state().memory.fields.at(till).toppedUp && count(s.takeJournal(), "a year's store", "wheat") > 0, "then it is topped up");
    Society::setWorkByHours(false);
}

// A town of one community ("town"), as the world's day plans give it.
void oneTown(Society& s)
{
    LifeDay day;
    day.communityOf = [](const std::string& cell) { return cell == "fields" ? std::string() : std::string("town"); };
    s.setDay(day);
}

// Nothing edible left on a till, or in a wolf's purse and its home's larder.
void bare(Society& s, const std::vector<std::string>& accounts)
{
    auto state = s.state();
    for (const auto& id : accounts)
        if (const auto a = state.accounts.find(id); a != state.accounts.end())
            for (auto it = a->second.stock.begin(); it != a->second.stock.end();)
                it = Society::edible(it->first) ? a->second.stock.erase(it) : std::next(it);
    expect(s.restore(state), "the bare shelves restore");
}

// Missed meals (doc 64, Phase 13), with work by the hours: a bakery sold out, with nothing to bake; a hungry wolf of its
// town, nothing to eat about it or at home, finds no shop with food open. The meal it wanted is counted at the bakery,
// once a day however long it stays hungry, and at the turn of the day it is in the bakery's sales: what it bakes follows
// what was wanted. Not at a brewery, the town's only food shop, whose meal (every food shop's) nobody makes; and with
// the hours off, the old pace, as it was: none.
void missedMealsCount()
{
    enum Case { Bakery, OldPace, Brewery };
    for (const auto which : {Bakery, OldPace, Brewery})
    {
        Society::setWorkByHours(which != OldPace);
        AuthoredRoster roster;
        auto wren = person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian");
        roster.residents = {which == Brewery ? person("baker", "the brewer at the vat", {"bakery", 2.5, 2.5})
                                             : person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}),
                            wren};
        roster.economy.treasury = 500;
        Society s(Roster::None);
        s.configure(roster);
        oneTown(s);
        const auto till = s.tillOf("baker");
        expect(s.foodShop("baker") && s.foodMade("baker").empty() == (which == Brewery), "a food shop; a brewery makes no food");
        double day = 1 + 10. / 24;
        work(s, 5, day);                           // (The day's first: the larders and purses founded.)
        auto state = s.state();
        state.residents.at("wren").hunger = 70;
        expect(s.restore(state), "hungry wren restores");
        // (Nor in the town's stores, which the bakery's meals come from.)
        bare(s, {till, "wren", Society::homeStore(s.resident("wren")->homeCell, "larder"), "treasury"});
        for (const char* in : {"flour", "firewood", "apples", "malt", "grapes", "honey"})
            set(s, "baker", in, 0);
        const auto food = [&] {
            int n = 0;
            for (const auto& [item, k] : s.account(till)->stock)
                n += Society::edible(item) ? k : 0;
            return n;
        };
        expect(food() == 0 && (!s.sellRates(till) || s.sellRates(till)->empty()), "the shop has nothing to eat, and has sold nothing");
        work(s, 600, day);
        expect(s.resident("wren")->hunger >= 60 && food() == 0, "wren stays hungry: there's no food in town (" + std::to_string(s.resident("wren")->hunger) +
                                                                    " hungry, " + std::to_string(food()) + " at the shop; " + s.resident("wren")->task + ")");
        double midnight = 2 - 30. / 86400;
        work(s, 60, midnight);
        const auto* rates = s.sellRates(till);
        std::string good;
        double rate = 0;
        if (rates)
            for (const auto& [item, r] : *rates)
                if (r > 0 && Society::edible(item))   // (Food: a brewery sells its cider.)
                    good = item, rate = r;
        if (which == Bakery)
        {
            const int meal = good.empty() ? 0 : (50 + Society::nourishment(good) - 1) / Society::nourishment(good);
            expect(!good.empty() && std::abs(rate - .3 * meal) < 1e-9,
                   "at the turn of the day the meal wren wanted is in the bakery's sales, once: " + good + " " + std::to_string(rate));
        }
        else
            expect(good.empty(), std::string(which == Brewery ? "at a brewery, no meal counted: " : "at the old pace, no meal counted: ") + good);
        expect(s.conserved(), "money stays conserved");
    }
    Society::setWorkByHours(false);
}

// Missed meals where food is made (doc 64, Phase 13), with work by the hours: a town's bakery and brewery both bare, and
// four hungry wolves: every meal they wanted is counted at the bakery, none lost to the brewery's lot (its meal nobody
// makes). And at the counter: two hungry wolves at a bakery with one loaf, the first buys it, and the second's meal is
// counted; with the hours off, the old pace, as it was, only the loaf sold (and its sold-out half again).
void missedMealsWhereFoodIsMade()
{
    Society::setWorkByHours(true);
    {
        AuthoredRoster roster;
        roster.residents = {person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}), person("brewer", "the brewer at the vat", {"vats", 2.5, 2.5})};
        for (const char* who : {"wren", "ash", "fern", "moss"})
            roster.residents.push_back(person(who, "weaving at home", {"homes", 3.5, 2.5}, "civilian"));
        roster.economy.treasury = 500;
        Society s(Roster::None);
        s.configure(roster);
        oneTown(s);
        double day = 1 + 10. / 24;
        s.takeJournal();
        work(s, 5, day);
        int sold = 0;                                // (A loaf or two bought before the shelves were bare: the church's alms.)
        for (const auto& e : s.takeJournal())
            sold += e.to == s.tillOf("baker") && items::baseOf(e.item) == "bread" && e.coins > 0 ? e.quantity : 0;
        std::vector<std::string> bareOf = {s.tillOf("baker"), s.tillOf("brewer"), "treasury"};
        auto state = s.state();
        for (const char* who : {"wren", "ash", "fern", "moss"})
        {
            state.residents.at(who).hunger = 70;
            bareOf.push_back(who);
            bareOf.push_back(Society::homeStore(state.residents.at(who).homeCell, "larder"));
        }
        expect(s.restore(state), "four hungry wolves restore");
        bare(s, bareOf);
        for (const char* in : {"flour", "firewood"})
            set(s, "baker", in, 0);
        for (const char* in : {"apples", "malt", "grapes", "honey"})
            set(s, "brewer", in, 0);
        work(s, 600, day);
        double midnight = 2 - 30. / 86400;
        work(s, 60, midnight);
        const auto* rates = s.sellRates(s.tillOf("baker"));
        const double bread = rates && rates->count("bread") ? rates->at("bread") : 0;
        const int meal = (50 + Society::nourishment("bread") - 1) / Society::nourishment("bread");
        expect(std::abs(bread - .3 * (sold + 4 * meal)) < 1e-9, "all four meals counted at the bakery: " + std::to_string((bread / .3 - sold) / meal));
    }
    for (const bool hours : {true, false})
    {
        Society::setWorkByHours(hours);
        AuthoredRoster roster;
        auto ash = person("ash", "idling in the square", {"bakery", 2.5, 2.5}, "civilian");
        auto wren = person("wren", "idling in the square", {"bakery", 2.5, 2.5}, "civilian");
        roster.residents = {person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}), ash, wren};
        roster.economy.treasury = 500;
        Society s(Roster::None);
        s.configure(roster);
        oneTown(s);
        double day = 1 + 10. / 24;
        work(s, 5, day);
        const auto till = s.tillOf("baker");
        auto state = s.state();
        for (const char* who : {"ash", "wren"})
            state.residents.at(who).hunger = 70;
        expect(s.restore(state), "two hungry wolves at the counter");
        bare(s, {till, "ash", "wren", Society::homeStore(s.resident("ash")->homeCell, "larder"), "treasury"});
        for (const char* in : {"flour", "firewood"})
            set(s, "baker", in, 0);
        set(s, "baker", "bread", 1);
        s.takeJournal();
        work(s, 600, day);
        int sold = 0;
        for (const auto& e : s.takeJournal())
            sold += e.kind == "resident food purchase" && e.to == till ? e.quantity : 0;
        double midnight = 2 - 30. / 86400;
        work(s, 60, midnight);
        const auto* rates = s.sellRates(till);
        const double bread = rates && rates->count("bread") ? rates->at("bread") : 0;
        const int meal = (50 + Society::nourishment("bread") - 1) / Society::nourishment("bread");
        if (hours)
            expect(sold == 1 && std::abs(bread - .3 * (1 + meal)) < 1e-9, "the loaf sold, and the second wolf's meal counted: " + std::to_string(bread / .3));
        else
            expect(sold == 1 && std::abs(bread - .3 * 2) < 1e-9, "at the old pace, the loaf sold and its half again, as it was: " + std::to_string(bread / .3));
    }
    // And at a brewery's counter (its meal nobody makes: foodMade): three hungry wolves and one meal. The first buys it;
    // the others' meals are counted nowhere, the meal only sold and its half again.
    {
        Society::setWorkByHours(true);
        AuthoredRoster roster;
        roster.residents = {person("brewer", "the brewer at the vat", {"vats", 2.5, 2.5})};
        for (const char* who : {"ash", "wren", "fern"})
            roster.residents.push_back(person(who, "idling in the square", {"vats", 2.5, 2.5}, "civilian"));
        roster.economy.treasury = 500;
        Society s(Roster::None);
        s.configure(roster);
        oneTown(s);
        double day = 1 + 10. / 24;
        work(s, 5, day);
        const auto till = s.tillOf("brewer");
        expect(s.foodMade("brewer").empty(), "the brewery makes no food");
        auto state = s.state();
        std::vector<std::string> bareOf = {till, "treasury"};
        for (const char* who : {"ash", "wren", "fern"})
        {
            state.residents.at(who).hunger = 70;
            bareOf.push_back(who);
            bareOf.push_back(Society::homeStore(state.residents.at(who).homeCell, "larder"));
        }
        expect(s.restore(state), "three hungry wolves at the brewery's counter");
        bare(s, bareOf);
        for (const char* in : {"apples", "malt", "grapes", "honey"})
            set(s, "brewer", in, 0);
        set(s, "brewer", "meal", 1);
        s.takeJournal();
        work(s, 600, day);
        int sold = 0;
        for (const auto& e : s.takeJournal())
            sold += e.kind == "resident food purchase" && e.to == till ? e.quantity : 0;
        double midnight = 2 - 30. / 86400;
        work(s, 60, midnight);
        double counted = 0;                         // (Its food's: the cider they drank aside.)
        if (const auto* rates = s.sellRates(till))
            for (const auto& [item, rate] : *rates)
                counted += items::good(item) && items::good(item)->drink ? 0 : rate;
        expect(sold == 1 && std::abs(counted - .3 * 2) < 1e-9, "the meal sold and its half again, no missed meal at the brewery: " +
                                                                  std::to_string(counted / .3));
    }
    Society::setWorkByHours(false);
}

// The household's errand goes on (doc 64, Phase 13): two bakeries with four loaves each, a household of two laying in
// with nothing at home. Half of the first's leaves it short of what it lays in at, so it goes on to the second.
void theErrandGoesOn()
{
    AuthoredRoster roster;
    auto wren = person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian");
    auto ash = person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian");
    auto loaf = person("loaf", "baker at The Loaf", {"bakery", 2.5, 2.5});
    auto oven = person("oven", "baker at The Morning Oven", {"oven", 2.5, 2.5});
    loaf.home = {"loaf_flat", 2.5, 2.5};
    oven.home = {"oven_flat", 2.5, 2.5};
    roster.residents = {loaf, oven, wren, ash};
    roster.economy.treasury = 500;
    Society s(Roster::None);
    s.configure(roster);
    oneTown(s);
    double day = 1 + 10. / 24;
    work(s, 5, day);
    const auto larder = Society::homeStore(s.resident("wren")->homeCell, "larder");
    expect(s.openAccount(larder), "the household's larder");
    bare(s, {s.tillOf("loaf"), s.tillOf("oven"), "wren", "ash", larder});
    auto state = s.state();
    for (const char* who : {"wren", "ash"})
        state.residents.at(who).hunger = 0;             // (Laying in, not hungry: no meal at the counter.)
    expect(s.restore(state), "the household fed");
    set(s, "loaf", "bread", 4);
    set(s, "oven", "bread", 4);
    s.takeJournal();
    double midnight = 2 - 30. / 86400;
    work(s, 60, midnight);
    int fromLoaf = 0, fromOven = 0;
    for (const auto& e : s.takeJournal())
        if (e.kind == "resident food purchase" && (e.from == "wren" || e.from == "ash"))
        {
            if (e.to == s.tillOf("loaf"))
                fromLoaf += e.quantity;
            if (e.to == s.tillOf("oven"))
                fromOven += e.quantity;
        }
    expect(fromLoaf > 0 && fromLoaf <= 2 && fromOven > 0 && fromOven <= 2,
           "half of each bakery's bread, the first not enough: " + std::to_string(fromLoaf) + " and " + std::to_string(fromOven));
    expect(s.conserved(), "money stays conserved");
}

// What a workshop keeps back of what it works with (doc 64, Phase 13): a chandler its candles' and nets' cord, two
// batches of each (where it would buy more), not a store of them; a butcher none of its raw meat beyond its shelf's few,
// for meat kept back only spoils.
void whatAWorkshopKeepsBack()
{
    AuthoredRoster roster;
    roster.residents = {person("chandler", "the chandler at the kettle", {"kettle", 2.5, 2.5}),
                        person("butcher", "the butcher on the lane", {"lane", 2.5, 2.5})};
    roster.economy.treasury = 500;
    Society s(Roster::None);
    s.configure(roster);
    int cord = 0;
    for (const auto* k : items::craftsFor(items::businessFor("the chandler at the kettle")->id))
        for (const auto& [in, n] : k->in)
            cord += in == "cord" ? n : 0;
    expect(cord > 0 && s.keepsBack("chandler", "cord") == std::max(Society::GoodsKept, cord * Society::MaterialsLow),
           "the chandler keeps back two batches of cord for each craft it works it into: " + std::to_string(s.keepsBack("chandler", "cord")));
    expect(s.keepsBack("butcher", "raw_meat") == Society::GoodsKept && s.keepsBack("chandler", "tallow") >= Society::GoodsKept,
           "the butcher keeps back no store of raw meat: " + std::to_string(s.keepsBack("butcher", "raw_meat")));
}

// Tools worn by real hours (doc 64, Phase 13), with work by the hours: two miners at the same work, one's premises
// improved as far as they go. The improved bring in more for an hour, but their picks wear by the hours worked: each
// yield's hours in hand, back to real hours.
void toolsWornByRealHours()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    roster.residents = {person("plain", "digs ore at the high pit", {"high_pit", 2.5, 2.5}, "civilian"),
                        person("better", "digs ore at the low pit", {"low_pit", 2.5, 2.5}, "civilian")};
    roster.economy.treasury = 500;
    Society s(Roster::None);
    s.configure(roster);
    oneTown(s);
    auto state = s.state();
    state.memory.pace = 1;
    state.memory.improved[s.jobOf("better")->id] = Society::MostImprovement;
    expect(s.restore(state), "the better premises restore");
    const auto* mine = items::producerFor("digs ore at the high pit");
    double everyDays = 0;
    for (const auto& t : items::toolNeeds())
        if (t.producer == mine->id && t.item == "mining_pick")
            everyDays = t.everyDays;
    s.takeJournal();
    double day = 8. / 24;
    work(s, 12000, day);
    const auto journal = s.takeJournal();
    for (const char* who : {"plain", "better"})
    {
        const auto till = s.tillOf(who);
        int yields = 0;
        for (const auto& e : journal)
            if (e.kind == "brought in" && e.to == till && items::baseOf(e.item) == "iron_ore")
                yields += e.quantity;
        const double improved = std::string(who) == "better" ? 1 - Society::ImprovementPace * Society::MostImprovement : 1;
        const double hours = yields * s.workHours(*mine) * improved, worn = s.toolWear(till, "mining_pick");
        expect(everyDays > 0 && yields > 0 && std::abs(worn - hours / (Society::PaidSpells * everyDays)) < 1e-9,
               std::string(who) + "'s pick worn by its real hours: " + std::to_string(worn * Society::PaidSpells * everyDays) + " for " +
                   std::to_string(yields) + " yields, " + std::to_string(hours) + " hours");
    }
    Society::setWorkByHours(false);
}

// A field's hands' wages kept (doc 64, Phase 13), with work by the hours: from a month before the harvest to its end, a
// farm's working money holds what its hands will be paid to bring in what it sowed (its owner's month's take leaves it),
// and between, none. From the last month of the year to the sowing's end, what they'll be paid to sow what's left; a
// field founded sown, none.
void theHarvestsWagesKept()
{
    Society::setWorkByHours(true);
    auto s = farmTown();
    const auto till = s.tillOf("farmer");
    const auto pid = s.jobOf("farmer")->id;
    const auto on = [&](std::int64_t d, double sown = 400) {
        auto state = s.state();
        state.budgetDay = d;
        state.memory.pace = 1;
        state.memory.fields[till] = FieldYear{400, sown, sown, 0, 0, false, 0, -1, true};
        expect(s.restore(state), "the farm's year restores");
        return std::pair{s.fieldWages(till), s.workingMoney(pid)};
    };
    const auto between = on(150), before = on(Society::HarvestStart - 10), during = on(Society::HarvestStart + 30), after = on(Society::HarvestEnd + 5);
    const double acre = s.workHours(*items::producerFor("farms the valley fields")) * Society::FieldYields;
    const double hours = 400 * acre * (1 - Society::SowShare) - double(Society::HarvestEnd - Society::HarvestStart) * Society::PaidSpells;
    expect(between.first == 0 && after.first == 0 && hours > 0 &&
               std::abs(before.first - hours / Society::HireSpells * s.employerWage(till) * Society::HarvestPay) < 1e-6,
           "a month before the harvest its hands' days at its wage, harvest pay: " + std::to_string(before.first));
    expect(before.second >= between.second + std::int64_t(before.first) && during.first > 0,
           "in its working money, until the harvest is in: " + std::to_string(before.second) + " against " + std::to_string(between.second));
    // The sowing: a quarter sown, the rest its hands', the sowing's days at its farmer's own hours less.
    const auto sowing = [&](std::int64_t days) {
        return std::max(0., 300 * acre * Society::SowShare - double(days * Society::PaidSpells)) / Society::HireSpells * s.employerWage(till) *
               Society::HarvestPay;
    };
    const auto winter = on(calendar::DaysPerYear - 25, 100), spring = on(10, 100), founded = on(10, 400);
    expect(sowing(Society::SowEnd) > 0 && std::abs(winter.first - sowing(Society::SowEnd)) < 1e-6 &&
               std::abs(spring.first - sowing(Society::SowEnd - 10)) < 1e-6,
           "the year's last month and the sowing, what's left to sow at its wage: " + std::to_string(winter.first) + ", " + std::to_string(spring.first));
    expect(founded.first == 0, "and a field founded sown, none: " + std::to_string(founded.first));
    Society::setWorkByHours(false);
}

// A field's farmer called back (doc 64, Phase 13), with work by the hours: in its founding spring, its fields sown as
// founded, the farmer takes a bakery's hire with the town's idle; its sales grown, on its own day of the week the day's
// pass looks at its acres again though its farmer is away (it was only its farmer's own work that looked, and a farmer
// on a week's hire never did), and calls it back to sow them. And when the bakery can't pay a hand's share, the hire is
// shut at once: its other hand doesn't work the day out unpaid.
void farmersCalledBackAndHiresShut()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    auto farmer = person("farmer", "farms the valley fields", {"fields", 4.5, 4.5}, "civilian");
    farmer.home = {"farmhouse", 2.5, 2.5};
    auto idler1 = person("idler1", "idling in the square", {"bakery", 2.5, 2.5}, "civilian");
    auto idler2 = person("idler2", "idling in the square", {"bakery", 2.5, 2.5}, "civilian");
    roster.residents = {farmer, person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}), idler1, idler2};
    roster.economy.treasury = 5000;
    Society s(Roster::None);
    s.configure(roster);
    oneTown(s);
    const auto bakery = s.tillOf("baker"), farm = s.tillOf("farmer");
    {
        auto state = s.state();
        state.memory.pace = 1;
        expect(s.restore(state), "the calibrated hours restore");
    }
    // Bread selling, a week: hands wanted.
    for (std::int64_t d = 1; d <= 7; ++d)
    {
        set(s, "baker", "bread", 90);
        expect(s.shift("treasury", "farmer", "", 0, 90, "test: the buyer's money") && s.sale(bakery, "farmer", "bread", 90, 1, "test: the day's buying"),
               "the bread is bought");
        auto st = s.state();
        st.accounts.at("farmer").stock.erase("bread");
        expect(s.restore(st), "and eaten");
        s.spoil(d);
    }
    set(s, "baker", "flour", 200);
    set(s, "baker", "firewood", 200);
    set(s, "baker", "bread", 0);
    auto state = s.state();
    // (Founded on day 15, sown as founded, on sales its acres were planned by: the day its week's look falls two days
    // after the hire.)
    const double planned = 10. * Society::FieldYields / ((calendar::DaysPerYear + Society::HarvestEnd - Society::HarvestStart) * Society::FieldMargin);
    state.memory.fields[farm] = FieldYear{10, 10, 10, 0, 0, false, planned, 15, true, planned, 0, 14};
    for (const char* who : {"farmer", "idler1", "idler2"})
        state.residents.at(who).hunger = state.residents.at(who).fatigue = 0;
    expect(s.restore(state), "the farm's fields all sown");
    const auto looks = std::int64_t(std::hash<std::string>{}(farm + "|acres") % 7);
    std::int64_t hired = 20;
    while ((hired + 2) % 7 != looks)
        ++hired;
    double day = double(hired) + 10. / 24;
    work(s, 120, day);
    const auto hire = [&]() -> const Society::OddJob* {
        for (const auto& j : s.oddJobs())
            if (j.payer == bakery && j.until >= 0)
                return &j;
        return nullptr;
    };
    expect(hire() && hire()->stage.count("farmer") && hire()->stage.count("idler1") && hire()->stage.count("idler2"),
           "the bakery hires, and its farmer free, the farm's hand takes it with the idle (" + std::to_string(hire() ? hire()->stage.size() : 0) + ")");
    state = s.state();
    state.memory.fields[farm].month = 2 * planned;      // (Its sales grown: twice its acres wanted.)
    for (const char* who : {"farmer", "idler1", "idler2"})
        state.residents.at(who).hunger = state.residents.at(who).fatigue = 0;
    expect(s.restore(state), "its sales grown");
    const auto calledBack = [&](double& when) {
        s.takeJournal();
        work(s, 5, when);
        const auto journal = s.takeJournal();
        return std::any_of(journal.begin(), journal.end(), [](const EconomyEntry& e) { return e.kind == "called back to the fields" && e.from == "farmer"; });
    };
    double next = double(hired + 1) + 10. / 24;
    expect(!calledBack(next) && hire() && hire()->stage.count("farmer") && s.state().memory.fields.at(farm).acres == 10,
           "the day after, not its day: its acres as they were, and the farmer on the hire");
    next = double(hired + 2) + 10. / 24;
    expect(calledBack(next) && hire() && !hire()->stage.count("farmer") && std::abs(s.state().memory.fields.at(farm).acres - 20) < 1e-6,
           "on its own day the day's pass looks at its acres, twice what it sowed, and calls the farmer back to the fields: " +
               std::to_string(s.state().memory.fields.at(farm).acres) + " acres");
    // The bakery's money gone before the day's share is done: the first to finish isn't paid, and the hire shuts.
    state = s.state();
    state.accounts.at("treasury").cash += state.accounts.at(bakery).cash;
    state.accounts.at(bakery).cash = 0;
    expect(s.restore(state), "the bakery's till emptied");
    work(s, 600 * Society::HireSpells + 60, next);
    const auto* shut = hire();
    expect(shut && shut->ended && shut->stage.count("idler1") && shut->stage.at("idler1") == 2 &&
               (!shut->stage.count("idler2") || shut->stage.at("idler2") != 2),
           "the first unpaid, the hire shuts: its other hand doesn't work the day out for nothing");
    Society::setWorkByHours(false);
}

// A field's farmer, free with its fields all sown, takes a House's place at the reckoning as it would a business's hire,
// and is called back to its fields from it on the day its field has work again (doc 64, Phase 13, round 6: a House's
// crews are posts, and a hand away on one is called back, or its own post stands open, as from a business's hire).
void aFarmerCalledBackFromAHousesCrew()
{
    Society::setWorkByHours(true);
    for (const std::string farmerId : {"farmer", "grower", "tiller", "sower", "reaper", "gleaner"})
    {
        AuthoredRoster roster;
        auto farmer = person(farmerId, "farms the valley fields", {"fields", 4.5, 4.5}, "civilian");
        farmer.home = {"farmhouse", 2.5, 2.5};
        roster.residents = {farmer, person("lord", "ruling House Fell", {"hall", 2.5, 2.5}, "civilian"), person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5})};
        for (int i = 1; i <= 10; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        oneTown(s);
        const auto farm = s.tillOf(farmerId);
        // (The reckoning on day 21; its field's weekly look within that week.)
        const auto looks = std::int64_t(std::hash<std::string>{}(farm + "|acres") % 7);
        if (looks == 0)
            continue;
        const std::int64_t hired = 21, look = hired + looks;
        s.tick(1, 20 + .5 / 24, 0, atWork(s));               // (The day before's pass: the House founded.)
        auto state = s.state();
        state.memory.pace = 1;
        const double planned = 10. * Society::FieldYields / ((calendar::DaysPerYear + Society::HarvestEnd - Society::HarvestStart) * Society::FieldMargin);
        state.memory.fields[farm] = FieldYear{10, 10, 10, 0, 0, false, planned, 15, true, planned, 0, 14};
        state.accounts.at("treasury").cash -= 100000;
        state.accounts.at("house:house_fell").cash += 100000;
        for (auto& [id, life] : state.residents)
            life.hunger = life.fatigue = 0;
        expect(s.restore(state), "the farm's fields all sown, the House in funds");
        double day = double(hired) + 10. / 24;
        work(s, 120, day);
        const auto onCrew = [&] {
            for (const auto& j : s.oddJobs())
                if (Society::houseCrew(j) && j.stage.count(farmerId))
                    return true;
            return false;
        };
        expect(onCrew(), "its fields all sown, the farmer takes a place with House Fell at the reckoning");
        state = s.state();
        state.memory.fields[farm].month = 2 * planned;      // (Its sales grown: twice its acres wanted.)
        for (auto& [id, life] : state.residents)
            life.hunger = life.fatigue = 0;
        expect(s.restore(state), "its sales grown");
        for (std::int64_t d = hired + 1; d <= look; ++d)
        {
            s.takeJournal();
            double when = double(d) + 10. / 24;
            work(s, 5, when);
            const auto journal = s.takeJournal();
            const bool back = std::any_of(journal.begin(), journal.end(), [&](const EconomyEntry& e) {
                return e.kind == "called back to the fields" && e.from == farmerId;
            });
            expect(back == (d == look) && onCrew() == (d < look),
                   "on its field's own day (" + std::to_string(look) + "), and not before, it is called back from the House's crew: day " +
                       std::to_string(d) + (back ? ", called back" : "") + (onCrew() ? ", on the crew" : ""));
        }
        expect(s.conserved(), "money stays conserved");
        Society::setWorkByHours(false);
        return;
    }
    expect(false, "a farm whose weekly look falls within the week");
}
} // namespace

// A producer's help is free for a hire as its business is (doc 64, Phase 13), with work by the hours: a farm kept as a
// shop, its help's hours going to its till's work in hand, which nothing draws on (its farmer works the fields with its
// own). Full, that read as no work for the help, and with its fields still to sow, the help would be freed each morning
// and called back each day's pass. Free only as its fields are: with them all sown, it is.
void aFarmsHelpFreeAsItsFields()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    auto farmer = person("farmer", "farms the valley fields", {"fields", 4.5, 4.5});
    farmer.home = {"farmhouse", 2.5, 2.5};
    auto hand = person("hand", "helping out", {"fields", 3.5, 4.5}, "civilian");
    hand.home = {"cottage", 2.5, 2.5};
    roster.residents = {farmer, hand};
    roster.economy.treasury = 5000;
    Society s(Roster::None);
    s.configure(roster);
    oneTown(s);
    double day = 8. / 24;
    work(s, 60, day);
    const auto till = s.tillOf("farmer");
    const auto payer = s.payerOf("hand", *s.jobOf("hand"), 30);
    expect(payer.account == till && items::producerFor("farms the valley fields"), "the farm pays its help from its till");
    const auto sow = [&](double sown) {
        auto state = s.state();
        state.memory.pace = 1;
        state.memory.fields[till] = FieldYear{20, sown, sown, 0, 0, false, 0, -1, true};
        for (const char* who : {"farmer", "hand"})
            state.residents.at(who).hunger = state.residents.at(who).fatigue = 0;
        expect(s.restore(state), "the farm's spring restores");
    };
    sow(10);
    work(s, 6 * 3600, day);
    day = 1 + 8. / 24;
    sow(10);
    work(s, 3600, day);
    expect(s.workBankedOf(till) >= 23, "its help's hours fill the till's work in hand: " + std::to_string(s.workBankedOf(till)));
    expect(!s.idleBusiness(till) && !s.idleHand("hand", payer), "half its fields to sow: its help isn't free, its till's hours full though they are");
    sow(20);
    expect(s.idleBusiness(till) && s.idleHand("hand", payer), "all sown: it is");
    Society::setWorkByHours(false);
}

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

// Prices at the hours worked (doc 64, Phase 13; doc 63's normal_cost): with work by the hours, a loaf's normal cost
// takes the hours a batch of bread takes at the realm's pace, and so does the cost a batch is made at; with the hours
// off, the calibrated hours, as before.
void pricedAtTheHoursWorked()
{
    Society::setCostAtPace(true);
    const items::Craft* k = nullptr;
    for (const auto* c : items::craftsFor("bakery"))
        if (c->id == "bread")
            k = c;
    expect(k && k->out.size() == 1 && k->hours > k->seconds / 3600., "a batch of bread takes longer calibrated than at the old pace");
    const double old = k->seconds / 3600., each = 1. / k->out.begin()->second;
    // A batch's hours at the town's wage, a loaf's share.
    const auto labour = [&](double hours) {
        return hours * (1 + k->skill / items::ladder().skillWage) * items::ladder().catalogueDay / Society::PaidSpells * each;
    };
    // A bakery's loaf priced, and then a batch baked (at the old pace's speed: the pace 0), by the hours or not.
    const auto bake = [&](bool byHours, double pace, double& normal, double& made) {
        Society::setWorkByHours(byHours);
        auto s = town();
        auto state = s.state();
        state.memory.pace = pace;
        expect(s.restore(state), "the pace restores");
        expect(std::abs(s.labourDay("") - items::ladder().catalogueDay) < 1e-9, "a village keeps the catalogue's day");
        normal = s.normalCost("baker", "bread");
        set(s, "baker", "bread", 0);
        double day = 10. / 24;
        work(s, 600, day);
        expect(held(s, "baker", "bread") > 0, "the baker bakes");
        made = s.costOf(s.tillOf("baker"), "bread");
        Society::setWorkByHours(false);
    };
    double off = 0, offMade = 0, slow = 0, slowMade = 0, half = 0;
    bake(false, 0, off, offMade);
    bake(true, 0, slow, slowMade);
    expect(std::abs(off - slow - (labour(k->hours) - labour(old))) < 1e-6,
           "by the hours at the old pace, a loaf is costed at the old pace's hours: " + std::to_string(slow) + "p against " + std::to_string(off) + "p");
    expect(std::abs(offMade - slowMade - (labour(k->hours) - labour(old))) < 1e-6,
           "and so is the batch it bakes: " + std::to_string(slowMade) + "p a loaf against " + std::to_string(offMade) + "p");
    Society::setWorkByHours(true);
    {
        auto s = town();
        auto state = s.state();
        state.memory.pace = .5;
        expect(s.restore(state), "half way to the calibrated hours");
        half = s.normalCost("baker", "bread");
    }
    Society::setWorkByHours(false);
    expect(std::abs(off - half - (labour(k->hours) - labour(old + .5 * (k->hours - old)))) < 1e-6,
           "half way there, at the hours half way: " + std::to_string(half) + "p");
    Society::setCostAtPace(false);
    {
        Society::setWorkByHours(true);
        auto s = town();
        auto state = s.state();
        state.memory.pace = .5;
        expect(s.restore(state), "half way, the switch off");
        expect(std::abs(s.normalCost("baker", "bread") - off) < 1e-6, "with ladder.json's costAtPace off, a loaf is costed at the calibrated hours");
        Society::setWorkByHours(false);
    }
}

// A realm (doc 64, Phase 13): a baker and a stall (or not), and its other wolves labourers, each in a home of its own.
Society realm(int wolves, bool shops = true)
{
    AuthoredRoster roster;
    if (shops)
        roster.residents = {person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}), person("stall", "keeping the stall", {"market", 4.5, 4.5})};
    for (int i = int(roster.residents.size()); i < wolves; ++i)
    {
        auto r = person("w" + std::to_string(i), "labouring in the yard", {"yard", 2.5, 2.5}, "civilian");
        r.home = {"home" + std::to_string(i), 2.5, 2.5};
        roster.residents.push_back(r);
    }
    roster.economy.treasury = 500;
    Society s(Roster::None);
    s.configure(roster);
    return s;
}

// The realm's labourer's day as it is founded (doc 64, Phase 13): a land of WorldMoneyResidents, whose money is made up to
// WorldMoney, starts at ladder.json's foundingDay, and what was written in pennies when a day was 12p follows it: a
// business's float, a meal at its catalogue price, the dearest piece the ledger moves, a household's food money (its
// relief). A village made for a test keeps the catalogue's day; a realm with no maker to measure keeps the rate it had.
void foundedAtTheFoundingDay()
{
    const double founding = items::ladder().foundingDay, catalogue = items::ladder().catalogueDay, ratio = founding / catalogue;
    expect(founding > catalogue, "ladder.json founds the realm above the catalogue's day: " + std::to_string(founding) + "p");
    auto big = realm(int(Society::WorldMoneyResidents)), small = realm(int(Society::WorldMoneyResidents) - 1);
    expect(std::abs(big.labourDay("") - founding) < 1e-9 && std::abs(big.state().memory.going.at("") - founding) < 1e-9,
           "a realm starts at the founding's day: " + std::to_string(big.labourDay("")) + "p");
    expect(std::abs(small.labourDay("") - catalogue) < 1e-9 && small.state().memory.going.empty(), "a village at the catalogue's");
    const auto pid = big.jobOf("baker")->id;
    expect(std::abs(double(big.floatOf(pid)) - ratio * double(small.floatOf(pid))) <= 3,
           "a bakery's float follows the day: " + std::to_string(big.floatOf(pid)) + "p against " + std::to_string(small.floatOf(pid)) + "p");
    expect(std::abs(big.townPrice("", "meal") - 6 * ratio) < 1e-9 && std::abs(small.townPrice("", "meal") - 6) < 1e-9,
           "a meal at its catalogue's 6p, at the day: " + std::to_string(big.townPrice("", "meal")) + "p");
    // The dearest piece the ledger moves: 1,000p at a 12p day (a piece of 1,200p, then).
    for (auto* s : {&big, &small})
    {
        expect(s->create("baker", "rapier", 1, "test: a piece"), "a rapier");
        for (int i = 10; i < 16; ++i)
            expect(s->shift("w" + std::to_string(i), "stall", "", 0, 200, "test: its price"), "and the price of it");
        expect(s->sale("baker", "stall", "rapier", 1, 1200, "test: a dear piece") == (s == &big),
               s == &big ? "the realm's ledger moves a piece of 1,200p" : "a village's refuses it");
    }
    // A household with nothing is given its food money in relief at the first day's turn: 5p a head at a 12p day.
    const auto relief = [](Society& s) {
        auto bodies = atWork(s);
        const auto poor = "w3";
        double day = 1 - 3. / 86400;
        s.tick(1, day, 0, bodies);                  // (Its purses founded first.)
        expect(s.shift(poor, "treasury", "", 0, s.account(poor)->cash, "test: penniless"), "a labourer with nothing");
        s.takeJournal();
        for (int i = 0; i < 6; ++i)
        {
            day += 1. / 86400;
            s.tick(1, day, 0, bodies);
        }
        expect(s.conserved(), "money stays conserved");
        std::int64_t given = 0;
        for (const auto& e : s.takeJournal())
            if (e.kind == "relief" && e.to == poor)
                given += e.coins;
        return given;
    };
    const auto bigRelief = relief(big), smallRelief = relief(small);
    expect(smallRelief == 5 && bigRelief == Society::pennies(5 * ratio),
           "a day's food money in relief, at the day: " + std::to_string(bigRelief) + "p against " + std::to_string(smallRelief) + "p");
    expect(std::abs(big.labourDay("") - founding) < 1e-9, "and the realm's day, measured from its makers, is the founding's");
    // (The review, doc 64, Phase 13.) The day it was founded at is remembered (the amounts' day at the seconds' pace),
    // and what was written at a 12p day follows it at either pace: a meal, herbs and a sword at the stores' prices, a
    // player's welcome grant, a keeper's reserve kept back from its shop's till.
    expect(big.state().memory.foundedDay == founding && small.state().memory.foundedDay == 0,
           "the realm remembers the day it was founded at, a village none: " + std::to_string(big.state().memory.foundedDay));
    {
        // (A keeper whose purse is its shop's till, as in an older save: the stall's till folded into its purse.)
        auto state = big.state();
        if (const auto till = big.tillOf("stall"); till != "stall")
        {
            auto& folded = state.accounts.at(till);
            auto& keeper = state.accounts.at("stall");
            keeper.cash += folded.cash;
            for (const auto& [item, n] : folded.stock)
                keeper.stock[item] += n;
            state.accounts.erase(till);
        }
        expect(big.restore(state) && big.tillOf("stall") == "stall", "a keeper whose purse is its shop's till");
    }
    for (const bool hours : {false, true})
    {
        Society::setWorkByHours(hours);
        const std::string pace = hours ? " (by the hours)" : " (at the seconds' pace)";
        expect(std::abs(big.amountDay("") - founding) < 1e-9, "the amounts' day is the founding's" + pace);
        for (const auto& [item, at12] : {std::pair{"meal", 6.}, std::pair{"herbs", 2.}, std::pair{"sword", double(items::good("sword")->price)}})
            expect(std::abs(big.townPrice("", item) - at12 * ratio) < 1e-9 && (hours || std::abs(small.townPrice("", item) - at12) < 1e-9),
                   std::string("the stores' ") + item + " at the day" + pace + ": " + std::to_string(big.townPrice("", item)) + "p");
        const std::string player = hours ? "player-hours" : "player-seconds";
        big.addPlayer(player);
        expect(big.account(player) && big.account(player)->cash == Society::pennies(20 * ratio),
               "a player's welcome grant at the day" + pace + ": " + std::to_string(big.account(player)->cash) + "p");
        // Such a keeper keeps back its reserve (20p at a 12p day) from what it spends.
        expect(big.account("stall")->cash > Society::pennies(20 * ratio) &&
                   big.spendable("stall") == big.account("stall")->cash - Society::pennies(20 * ratio),
               "a keeper keeps back its reserve at the day" + pace + ": " + std::to_string(big.spendable("stall")) + "p of " +
                   std::to_string(big.account("stall")->cash) + "p");
    }
    Society::setWorkByHours(false);
    small.addPlayer("player-village");
    expect(small.account("player-village")->cash == 20, "a village's welcome grant is the catalogue day's 20p");
    expect(big.farmhandsFoodMoney(50, "") == Society::pennies(3 * ratio) && big.farmhandsFoodMoney(0, "") == Society::pennies(2 * ratio) &&
               small.farmhandsFoodMoney(50, "") == 3,
           "a farm's money for its bunkhouse's food at the day: " + std::to_string(big.farmhandsFoodMoney(50, "")) + "p");
    // At the seconds' pace the amounts keep the founding's day whatever a town measures (that pace's measure keeps the
    // skill's premium in: a town of skill-10 shops measures 12.8p in a realm founded at 12p); by the hours they follow it.
    {
        auto state = small.state();
        state.memory.going = {{"", catalogue * 16 / 15}, {"t", catalogue * 16 / 15}};
        expect(small.restore(state), "a village whose shops measure the day with their premium");
        expect(small.amountDay("t") == catalogue && small.pennies(20 / 12., "t") == 20 && std::abs(small.townPrice("t", "meal") - 6) < 1e-9,
               "at the seconds' pace, its amounts as they were: " + std::to_string(small.pennies(20 / 12., "t")) + "p");
        troubles::Reads reads;
        reads.society = &small;
        double dayCost = 0;
        troubles::foodDays({"w3"}, "t", reads, nullptr, &dayCost);
        expect(dayCost == 5, "and the troubles' food a day, 5p a head: " + std::to_string(dayCost) + "p");
        Society::setWorkByHours(true);
        expect(std::abs(small.amountDay("t") - catalogue * 16 / 15) < 1e-9 && small.pennies(20 / 12., "t") == 21,
               "by the hours, at the town's: " + std::to_string(small.pennies(20 / 12., "t")) + "p");
        Society::setWorkByHours(false);
    }
    // A realm with nobody whose wage measures the day keeps the day it had (it fell back to the catalogue's).
    auto bare = realm(int(Society::WorldMoneyResidents), false);
    relief(bare);
    expect(std::abs(bare.labourDay("") - founding) < 1e-9, "with no maker to measure, the realm keeps its day: " + std::to_string(bare.labourDay("")) + "p");
    // A village with none falls back to the catalogue's, as it did (the review): it was never founded at another.
    {
        auto village = realm(int(Society::WorldMoneyResidents) - 1, false);
        auto state = village.state();
        state.memory.going[""] = catalogue * 1.25;
        expect(village.restore(state) && std::abs(village.labourDay("") - catalogue * 1.25) < 1e-9, "a village measured at 15p once");
        relief(village);
        expect(std::abs(village.labourDay("") - catalogue) < 1e-9, "with no maker to measure, a village falls back to the catalogue's day: " +
                                                                        std::to_string(village.labourDay("")) + "p");
    }
    // An older save of a land this size that never founded its purses (the review): its employers' books, kept at the
    // catalogue's day, move with the founding, and its next measure keeps the founding's day.
    {
        auto older = realm(int(Society::WorldMoneyResidents));
        auto bodies = atWork(older);
        double day = .5;
        for (int d = 0; d < 8; ++d, day += 1)
            for (int i = 0; i < 3; ++i)
                older.tick(1, day + i / 86400., 0, bodies);   // (A week of reviews: every employer's book.)
        auto state = older.state();
        expect(!state.memory.employers.empty(), "the employers have their books");
        std::map<std::string, double> founded;
        for (auto& [key, book] : state.memory.employers)
            founded[key] = book.wage, book.wage /= ratio;
        state.memory.purses = 0;
        state.memory.foundedDay = 0;
        for (auto& [town, rate] : state.memory.going)
            rate = catalogue;
        expect(older.restore(state) && std::abs(older.labourDay("") - catalogue) < 1e-9, "an older save at the catalogue's day, its purses never founded");
        older.tick(1, day - .8, 0, bodies);         // (Its purses founded, the same day.)
        bool moved = true;
        for (const auto& [key, book] : older.state().memory.employers)
            if (const auto was = founded.find(key); was != founded.end())
                moved &= std::abs(book.wage - was->second) < 1e-6 * was->second;
        expect(older.state().memory.purses >= 1 && older.state().memory.foundedDay == founding && moved,
               "its purses founded, the employers' books move with the day");
        for (int i = 0; i < 3; ++i)
            older.tick(1, day + i / 86400., 0, bodies);   // (The next day's measure.)
        expect(std::abs(older.labourDay("") - founding) < founding * .02,
               "and its measure keeps the founding's day: " + std::to_string(older.labourDay("")) + "p");
    }
    // The same, loaded afresh from the save (the price review: the labour market's record of each employer's town is
    // made at its day, none yet in a society just loaded, so every book went by the realm's rate), in two towns at
    // different rates: the market's stall in the north at a quarter over the realm's, the rest in the south at it. Each
    // book moves by its own town's.
    {
        LifeDay towns;
        towns.communityOf = [](const std::string& cell) { return cell == "market" ? std::string("north") : std::string("south"); };
        auto older = realm(int(Society::WorldMoneyResidents));
        older.setDay(towns);
        auto bodies = atWork(older);
        double day = .5;
        for (int d = 0; d < 8; ++d, day += 1)
            for (int i = 0; i < 3; ++i)
                older.tick(1, day + i / 86400., 0, bodies);
        auto state = older.state();
        const auto north = "till:" + older.jobOf("stall")->id, south = "till:" + older.jobOf("baker")->id;
        expect(state.memory.employers.count(north) && state.memory.employers.count(south), "the stall and the bakery have their books");
        state.memory.purses = 0;
        state.memory.foundedDay = 0;
        state.memory.going = {{"", catalogue}, {"south", catalogue}, {"north", catalogue * 1.25}};
        state.memory.employers.at(north).wage = catalogue * 1.25;
        state.memory.employers.at(south).wage = catalogue;
        auto loaded = realm(int(Society::WorldMoneyResidents));
        loaded.setDay(towns);
        expect(loaded.restore(wire::readSociety(wire::society(state))), "an older save of two towns, loaded afresh");
        loaded.tick(1, day - .8, 0, bodies);        // (Its purses founded.)
        const auto& books = loaded.state().memory.employers;
        expect(loaded.state().memory.purses >= 1 && std::abs(books.at(north).wage - founding) < 1e-6 && std::abs(books.at(south).wage - founding) < 1e-6,
               "each book by its own town's rate: the north's " + std::to_string(books.at(north).wage) + "p, the south's " +
                   std::to_string(books.at(south).wage) + "p, at " + std::to_string(founding) + "p");
    }
    // A church founded after the realm's money stands (an older save's, its church never opened) is given ChurchHead a
    // resident (10p at a 12p day) from the treasury at its town's day, if more than its month's basket (the price
    // review: it was given the raw 10p).
    {
        auto late = realm(int(Society::WorldMoneyResidents));
        auto bodies = atWork(late);
        double day = .5;
        for (int d = 0; d < 2; ++d, day += 1)
            for (int i = 0; i < 3; ++i)
                late.tick(1, day + i / 86400., 0, bodies);
        expect(late.state().memory.purses >= 2 && late.account(Society::SharedChurch), "the realm's money stands, its church with it");
        expect(late.shift(Society::SharedChurch, "treasury", "", 0, late.account(Society::SharedChurch)->cash, "test: the church's to the treasury"),
               "its purse back to the treasury");
        auto state = late.state();
        state.accounts.erase(Society::SharedChurch);
        expect(late.restore(state) && !late.account(Society::SharedChurch), "an older save with no church");
        for (int i = 0; i < 3; ++i)
            late.tick(1, day + i / 86400., 0, bodies);
        const auto least = Society::pennies(10 * ratio) * std::int64_t(Society::WorldMoneyResidents);
        expect(late.account(Society::SharedChurch) && late.account(Society::SharedChurch)->cash >= least && late.conserved(),
               "a church founded late, ChurchHead a resident at the day: " +
                   std::to_string(late.account(Society::SharedChurch) ? late.account(Society::SharedChurch)->cash : -1) + "p, at least " +
                   std::to_string(least) + "p");
    }
    // A keeper short of a week's working money in its shop's till puts in what it holds over its reserve, at its own
    // town's day (the price review: untried): in the north at a fifth over the realm's, by the hours.
    {
        LifeDay towns;
        towns.communityOf = [](const std::string& cell) { return cell == "market" ? std::string("north") : std::string("south"); };
        auto owners = realm(int(Society::WorldMoneyResidents));
        owners.setDay(towns);
        auto bodies = atWork(owners);
        Society::setWorkByHours(true);
        double day = .5;
        for (int i = 0; i < 3; ++i)
            owners.tick(1, day + i / 86400., 0, bodies);
        const auto till = owners.tillOf("stall");
        expect(till != "stall", "the stall's till its own");
        std::int64_t invested = -1, reserve = 0;
        for (int d = 1; d < 9 && invested < 0; ++d)
        {
            day += 1;
            auto state = owners.state();
            state.memory.going = {{"", founding}, {"north", founding * 1.2}, {"south", founding}};
            state.memory.putIn.erase(till);
            expect(owners.restore(state), "the north a fifth over the realm");
            reserve = owners.pennies(Society::KeeperReserve, "north");
            owners.shift(till, "treasury", "", 0, owners.account(till)->cash, "test: the till short");
            if (const auto cash = owners.account("stall")->cash; cash > reserve + 7)
                owners.shift("stall", "treasury", "", 0, cash - reserve - 7, "test: the keeper's purse");
            else
                owners.shift("treasury", "stall", "", 0, reserve + 7 - cash, "test: the keeper's purse");
            owners.takeJournal();
            owners.tick(1, day, 0, bodies);         // (The day's turn: its owner's day of the week, one of them.)
            for (const auto& e : owners.takeJournal())
                if (e.kind == "an owner's investment" && e.from == "stall" && e.to == till)
                    invested = e.coins;
        }
        Society::setWorkByHours(false);
        expect(invested == 7, "it puts in what it holds over its reserve at its town's day (" + std::to_string(reserve) + "p): " +
                                  std::to_string(invested) + "p");
    }
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
        foodKeptOverRestday();
        piecesWornAndMended();
        aHouseholdsWants();
        aPieceWaitedForAFewDays();
        piecesItCantGetGoOnToWorks();
        aMissedPieceIsOneItCouldPayFor();
        worksSellOutTheShelf();
        worksFromTheBuildingTrades();
        piecesFoundedOnce();
        wantsAtTheOldPace();
        aBiteNotTheMakersMaterials();
        theWantFundClamped();
        noTreatsInAHungryTown();
        theCatchFeedsTheLarder();
        theMint();
        aStallDumpAndTheRebound();
        theGoingRateFromTheMakers();
        aTownKeepsItsRateAtTheOldPace();
        handsByWhatCanBeMade();
        aShortBusinessBidsUp();
        theLandsHiresForAnyone();
        aProducersPostsBySales();
        aHireTakenIsNoPostOpen();
        aHouseKeepsItsCrew();
        theCordGoesRoundNoMore();
        aFullStoreLosesItsHours();
        aTownsTradeBringsInMore();
        millGrindsForTheStall();
        cartedInFromAnotherTown();
        qualities();
        townsfolkBuy();
        outputFollowsSales();
        foodMakersLayIn();
        fieldsPlannedByTheirFood();
        missedSalesReachTheLand();
        missedMealsCount();
        missedMealsWhereFoodIsMade();
        theErrandGoesOn();
        whatAWorkshopKeepsBack();
        toolsWornByRealHours();
        theHarvestsWagesKept();
        farmersCalledBackAndHiresShut();
        aFarmerCalledBackFromAHousesCrew();
        aFarmsHelpFreeAsItsFields();
        missedOnlyWhenNoneLeft();
        missedAgainstTheDaysBuying();
        fieldsPlannedByTheirMonth();
        aRestartKeepsTheMonth();
        pricedAtTheHoursWorked();
        foundedAtTheFoundingDay();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Crafting tests passed: " << checks << " checks.\n";
    return 0;
}

