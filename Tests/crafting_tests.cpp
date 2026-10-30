// Crafting by shopkeepers (Docs/Design/35-items-crafting-industry.md, Phase 5, first part; Data/Items/crafts.json):
// makers start with a good store of materials, make a batch of what runs low from their own stock, buy more from a
// supplier in town when it runs short, and make nothing from nothing.
#include "RatwCalendar.h"
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
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
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

// AcresByHarvest (round 16, lane 2; the user, 2026-10-30), with work by the hours: at the harvest's end a field that left
// more than a tenth of what it sowed in the ground plans no more than half again what it brought in, and no less than
// half its plan; one that brought in nine tenths, or sowed nothing, plans as before; a dry year's half again comes
// first. Planned here by its sales a day alone, its store empty (no carry): 100 acres at its rate, 150 in a dry year.
void aFieldPlansWhatItBroughtIn()
{
    Society::setWorkByHours(true);
    const auto* farm = items::producerFor("farms the valley fields");
    expect(farm && Society::fieldWork(farm->id), "the valley fields are a farm's");
    const double year = calendar::DaysPerYear + Society::HarvestEnd - Society::HarvestStart;
    const double hundred = 100. * Society::FieldYields / (year * Society::FieldMargin);   // (Its sales a day for 100 acres.)
    // Its acres planned at the harvest's end of year 1, from what it sowed and what is left in the ground.
    const auto planned = [&](bool rule, double acresAtItsRate, double sown, double left, bool dry) {
        Society::wageRulesOff(rule ? 0 : Society::AcresByHarvest);
        auto s = farmTown();
        const auto till = s.tillOf("farmer");
        auto st = s.state();
        auto& f = st.memory.fields[till];
        f.rate = f.month = hundred * acresAtItsRate / 100;
        f.monthDays = 28;
        f.sown = sown;
        f.toHarvest = left;
        f.dry = dry;
        f.year = 0;
        f.founded = 10;
        for (const auto& [good, n] : farm->out)
            st.accounts.at(till).stock.erase(good);
        expect(s.restore(st), "the field's year restores");
        s.planField("farmer", till, *farm, calendar::DaysPerYear + Society::HarvestEnd + .1, false);
        const auto after = s.state().memory.fields.at(till);
        expect(after.sown == 0 && after.toHarvest == 0 && !after.dry && after.year == 1, "its year begun afresh");
        expect(s.conserved(), "money is conserved");
        Society::wageRulesOff(0);
        return after.acres;
    };
    const auto near = [](double a, double b) { return std::abs(a - b) < 1e-6; };
    for (const bool rule : {true, false})
    {
        const auto with = [&](double ruled, double otherwise) { return rule ? ruled : otherwise; };
        const auto said = [&](double acres) { return std::to_string(acres) + (rule ? "" : ", without the rule"); };
        double a = planned(rule, 100, 100, 70, false);
        expect(near(a, with(50, 100)), "sown 100, 30 brought in: planned 100 comes down to half (AcresLeast): " + said(a));
        a = planned(rule, 80, 100, 70, false);
        expect(near(a, with(45, 80)), "planned 80 comes down to half again the 30 brought in (AcresGrowth): " + said(a));
        a = planned(rule, 100, 100, 5, false);
        expect(near(a, 100), "95 of 100 brought in: as planned: " + said(a));
        a = planned(rule, 100, 0, 0, false);
        expect(near(a, 100), "nothing sown: as planned: " + said(a));
        a = planned(rule, 100, 100, 70, true);
        expect(near(a, with(75, 150)), "a dry year's 150, 30 brought in: half of it: " + said(a));
    }
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
    Society::wageRulesOff(Society::RichHomes);   // (Round 11's rich homes left out: as built before them.)
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
    Society::wageRulesOff(0);
}

// What a household wants (doc 64, Phase 13; doc 63's model), with work by the hours: one want a day, out of its want
// fund. A piece it keeps first (finery to a piece a head), bought from what a shop sells, and waited for while its town's
// shops have none to be had (no works meanwhile); with all it wants, works on its home, bought by the amount from a
// builder's hours and its planks, and no fancy besides. The planks count as the carpenter's sales, so it makes them
// again, and its works toward its hands are kept for the save.
void aHouseholdsWants()
{
    Society::wageRulesOff(Society::RichHomes);   // (Round 11's rich homes left out: as built before them.)
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
    Society::wageRulesOff(0);
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
    Society::wageRulesOff(Society::RichHomes);   // (Round 11's rich homes left out: as built before them.)
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
    Society::wageRulesOff(0);
}

// A piece it can't get goes on to the rest (doc 64, Phase 13; model.py's wants: only one its fund runs to): a household
// that works and holds no blade, its finery owned, in a town whose shops sell no sword, has works on its home, and
// wants no sword; in one with an armourer whose swords are past what its fund holds, the same; and none is waited for.
void piecesItCantGetGoOnToWorks()
{
    Society::wageRulesOff(Society::RichHomes);   // (Round 11's rich homes left out: as built before them.)
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
    Society::wageRulesOff(0);
}

// A piece it could pay for and found on no shelf is a sale missed of one its fund runs to (doc 64, Phase 13; the
// model's asks): an armourer with no swords on its shelves hears of the cheapest, never of the dear one past the fund
// (a 40p fund told an armourer of a 90p steel sword); a store with no finery, of the dearest piece the fund runs to (the
// dearest is what it buys), never of its charms at ten times their cost. Twenty days of looking, each day's by its own
// lot. (The fund is the cheapest piece's price before the day's share comes into it.)
void aMissedPieceIsOneItCouldPayFor()
{
    // (Round 11's rich homes left out: as built before them. And round 14's ChainPricedOnce: the store's charms, which
    // it only sells on, would be kept to the ladder's margin, not its dear 19.)
    Society::wageRulesOff(Society::RichHomes | Society::ChainPricedOnce);
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
    Society::wageRulesOff(0);
}

// Works that use a builder's last planks are a sell-out (doc 64, Phase 13; Society::spoil): its sales a day counted half
// again, as a shelf sold bare is, so it plans for more. The same works from twenty planks are sales as they are. (A
// carpenter with nothing to make planks from, its hours in hand from a day at the yard.)
void worksSellOutTheShelf()
{
    Society::wageRulesOff(Society::RichHomes);   // (Round 11's rich homes left out: as built before them.)
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
    Society::wageRulesOff(0);
}

// Works on a home are the building trades' (doc 64, Phase 13): a smith's nails and fittings are a few of its wares, and
// its hands are for its tools and blades. A household with all it wants, in a town with a smith and no builder, has none
// done (its fund waits, clamped).
void worksFromTheBuildingTrades()
{
    Society::wageRulesOff(Society::RichHomes);   // (Round 11's rich homes left out: as built before them.)
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
    Society::wageRulesOff(0);
}

// Households are founded with pieces of every age (doc 63's model; doc 64, Phase 13), with work by the hours: once, in a
// new world's first day pass (EconomyMemory::piecesFounded, saved), never again though its belongings were never
// counted, and never into a world that has lived (its purses founded).
void piecesFoundedOnce()
{
    Society::wageRulesOff(Society::RichHomes);   // (Round 11's rich homes left out: as built before them.)
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
    Society::wageRulesOff(0);
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

// A rich home spends with its budget (round 11, lane 2, RichHomes; the user, 2026-10-29), with work by the hours: a
// household of two given 200,000p and a help budget of ten hands' weeks hires on its first day as many hands about the
// home as the budget pays a week of (a post of twelve at most), and none more the next day while those places stand
// untaken; and it keeps as many servants as a tenth of its excess pays a month, posted at once, in posts of twelve. A
// lodger with the same keeps them too. Without the rule, as before: one hand a day, three servants at most on one day
// in seven, a lodger none.
void aRichHomeSpendsWithItsBudget()
{
    Society::setWorkByHours(true);
    for (const bool rule : {true, false})
        for (const bool lodger : {false, true})
        {
            Society::wageRulesOff(rule ? 0 : Society::RichHomes);
            AuthoredRoster roster;
            auto wren = person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian"),
                 ash = person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian");
            if (lodger)
                wren.home = ash.home = {"mossbank_bunkhouse", 2.5, 2.5};
            roster.residents = {wren, ash};
            roster.economy.treasury = 250000;
            Society s(Roster::None);
            s.configure(roster);
            double day = 10. / 24;
            work(s, 60, day);
            expect(s.shift("treasury", "ash", "", 0, 200000, "test: an inheritance"), "the household is given a fortune");
            const auto places = [&](const char* what) {
                int n = 0, posts = 0;
                for (const auto& j : s.oddJobs())
                    if (j.what == what && (j.payer == "wren" || j.payer == "ash") && !j.ended)
                        n += j.slots, ++posts;
                return std::pair{n, posts};
            };
            const std::string who = lodger ? "a lodger" : "a household";
            const double pay = std::ceil(.8 * s.goingRate("") - 1e-9), wage = std::ceil(.9 * s.goingRate("") - 1e-9);
            {
                auto state = s.state();
                state.memory.helpBudget[lodger ? "ash" : "homes"] = 10 * 7 * pay + 1;
                expect(s.restore(state), "the help budget restores");
            }
            day = 1 + 10. / 24;
            work(s, 60, day);
            const int hands = places("help about the home").first;
            int servants = places("a servant of the house").first;
            if (rule)
            {
                expect(hands >= 10 && hands <= 12, who + " hires as many hands as its budget pays a week of: " + std::to_string(hands));
                const int wanted = int(200000 * .1 / (wage * MonthDays));
                expect(servants >= wanted - 1 && servants <= wanted + 1 && places("a servant of the house").second >= 2,
                       who + "'s servants on its first day, as many as a tenth of its excess pays a month, in posts of twelve: " +
                           std::to_string(servants) + " of " + std::to_string(wanted));
            }
            else
                expect(hands == (lodger ? 0 : 1), "without the rule one hand a day, a lodger none: " + std::to_string(hands));
            day = std::floor(day) + 1 + 10. / 24;
            work(s, 60, day);
            if (rule)
                expect(places("help about the home").first == hands, "and no more while its places stand untaken: " +
                                                                         std::to_string(places("help about the home").first));
            for (int d = 3; d <= 8; ++d)
            {
                servants = std::max(servants, places("a servant of the house").first);
                day = std::floor(day) + 1 + 10. / 24;
                work(s, 60, day);
            }
            if (!rule)
                expect(servants == (lodger ? 0 : 3), "without the rule three servants at most, a lodger none: " + std::to_string(servants));
        }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// A rich home's wants with its fund (RichHomes): a household whose fund holds 10,000p and only a butcher's raw meat to
// fancy has a second treat on a day (the day's spending under half its fund); without the rule one thing a day.
void aRichHomeBuysMore()
{
    Society::setWorkByHours(true);
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::RichHomes);
        AuthoredRoster roster;
        auto butcher = person("butcher", "the butcher on the corner", {"shambles", 2.5, 2.5});
        butcher.home = {"shambles_flat", 2.5, 2.5};
        roster.residents = {butcher, person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian"),
                            person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian")};
        roster.economy.treasury = 50000;
        Society s(Roster::None);
        s.configure(roster);
        double day = 10. / 24;
        work(s, 60, day);
        expect(s.shift("treasury", "ash", "", 0, 20000, "test: a good year"), "the household has money");
        int most = 0;
        for (int d = 1; d <= 20; ++d)
        {
            {
                auto state = s.state();
                state.accounts.at(s.tillOf("butcher")).stock["raw_meat"] = 30;
                state.memory.wantFund["homes"] = 10000;
                expect(s.restore(state), "the butcher's meat and the household's fund restore");
            }
            s.takeJournal();
            day = std::floor(day) + 1 + 10. / 24;
            work(s, 60, day);
            int treats = 0;
            for (const auto& e : s.takeJournal())
                treats += e.kind == "a want: a treat" && (e.from == "wren" || e.from == "ash");
            most = std::max(most, treats);
        }
        if (rule)
            expect(most >= 2, "a rich home buys more than one thing a day: " + std::to_string(most));
        else
            expect(most == 1, "without the rule one thing a day: " + std::to_string(most));
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// The holdings tax (round 11, lane 2, HoldingsTax; the user, 2026-10-29), with work by the hours: a household of two
// holding 100,000p pays a seventh a day of 1% of what it holds over six months of its needs, to its town; the Dungeon
// Master's lever sets the rate (0 to 10% a week, in the ledger, saved); at 0, or without the rule, none.
void theHoldingsTax()
{
    Society::setWorkByHours(true);
    for (const int pass : {0, 1, 2})
    {
        // (Round 11's flat tax: round 12's HoldingsBite, graduated, left out; theHoldingsTaxBites.)
        Society::wageRulesOff((pass == 2 ? Society::HoldingsTax : 0) | Society::HoldingsBite);
        AuthoredRoster roster;
        roster.residents = {person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian"),
                            person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian")};
        roster.economy.treasury = 150000;
        Society s(Roster::None);
        s.configure(roster);
        expect(s.holdingsTaxRate() == Society::HoldingsTaxRate, "the holdings tax starts at its constant");
        if (pass == 1)
        {
            expect(!s.setHoldingsTax(.2, "dm:test").ok && s.holdingsTaxRate() == Society::HoldingsTaxRate, "a holdings tax past 10% is refused");
            expect(s.setHoldingsTax(0, "dm:test").ok && s.holdingsTaxRate() == 0, "a Dungeon Master turns it off");
            expect(wire::readSociety(wire::society(s.state())).memory.holdingsTax == 0, "and it is saved");
        }
        double day = 10. / 24;
        work(s, 60, day);
        expect(s.shift("treasury", "ash", "", 0, 100000, "test: an inheritance"), "the household is given a fortune");
        s.takeJournal();
        day = 1 + 10. / 24;
        work(s, 60, day);
        std::int64_t paid = 0;
        std::string to;
        for (const auto& e : s.takeJournal())
            if (e.kind == "holdings tax" && (e.from == "wren" || e.from == "ash"))
                paid += e.coins, to = e.to;
        const auto held = 100000 + 2 * 200;
        if (pass == 0)
            expect(paid <= std::llround(held * .01 / 7 * 1.02) && paid >= std::llround(held * .01 / 7 * .8) && to == "treasury",
                   "a seventh of 1% of what it holds over six months of its needs, to its town: " + std::to_string(paid) + "p to " + to);
        else
            expect(paid == 0, pass == 1 ? "at 0, none" : "without the rule, none");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// A treat is never bought in a town gone hungry (doc 64, Phase 13, with work by the hours: what's on its shelves is for
// the hungry). A butcher's raw meat is a treat to a household with its fund, now and then; never while a wolf of its
// town is at 70 (the monitor's hungry), and at 69 still. With the hours off, the old pace, as it was: hungry or not.
void noTreatsInAHungryTown()
{
    Society::wageRulesOff(Society::RichHomes);   // (Round 11's rich homes left out: as built before them.)
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
    Society::wageRulesOff(0);
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

// The going rate by employees (round 7, the user, 2026-10-26; the model's market_wage, max(1, workers), owners not
// workers): a scribe's office with three clerks paying 20p a labourer's day, and three scribes, each alone at its desk,
// paying 10p. The office counts three, each lone scribe one: the rate is the office's 20p. Before (and without the rule),
// each counted by the hands its work wanted, keeper and all, at most those it had: one each, and the lone scribes' 10p.
// With two clerks the office counts two, not its scribe too: the lone scribes' 10p.
void theGoingRateByEmployees()
{
    Society::setWorkByHours(true);
    const auto rate = [&](std::uint64_t off, int clerks) {
        AuthoredRoster roster;
        roster.residents = {person("scribe", "the scribe", {"desk", 2.5, 2.5})};
        for (int i = 1; i <= clerks; ++i)
            roster.residents.push_back(person("clerk" + std::to_string(i), "works at the desk", {"desk", 2.5 + i, 3.5}, "civilian"));
        for (int i = 1; i <= 3; ++i)
            roster.residents.push_back(person("lone" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 5000;
        Society::wageRulesOff(off);
        Society s(Roster::None);
        s.configure(roster);
        const auto office = s.tillOf("scribe");
        auto state = s.state();
        const double skilled = 1 + s.employerSkill(office) / items::ladder().skillWage;
        state.memory.employers[office] = {20 * skilled};
        for (int i = 1; i <= 3; ++i)
            state.memory.employers[s.tillOf("lone" + std::to_string(i))] = {10 * skilled};
        expect(s.restore(state), "the employers' books restore");
        double day = 1 + 10. / 24;
        work(s, 1, day);
        expect(s.handsOf(office) == 1 + clerks && s.handsWanted(office) == 1,
               "the office has its scribe and its clerks, and wants one: " + std::to_string(s.handsOf(office)) + ", " + std::to_string(s.handsWanted(office)));
        Society::wageRulesOff(0);
        return s.goingRate("");
    };
    const double now = rate(0, 3), before = rate(Society::ByEmployees, 3), two = rate(0, 2);
    expect(std::abs(now - 20) < 1e-6, "the going rate is the office's, by its three clerks: " + std::to_string(now));
    expect(std::abs(before - 10) < 1e-6, "by the hands wanted, the lone scribes': " + std::to_string(before));
    expect(std::abs(two - 10) < 1e-6, "with two clerks, its scribe not counted, the lone scribes': " + std::to_string(two));
    Society::setWorkByHours(false);
}

// A stall lets its surplus help go (round 7, the user, 2026-10-26; the model's firm_week): a stall with four hands
// beside its keeper wants one, its keeper. At each weekly review it lets half the excess go (one at least), whatever the
// realm's pace: two, then one, then the last. Each let go keeps its home and is a jobless wolf, its post kept empty.
// It hires nobody. Without the rule it keeps all four.
void aStallLetsItsSurplusGo()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    roster.residents = {person("stall", "keeping the stall", {"market", 4.5, 4.5}), person("scribe", "the scribe", {"desk", 2.5, 2.5})};
    for (int i = 1; i <= 4; ++i)
        roster.residents.push_back(person("hand" + std::to_string(i), "works at the stall", {"market", 4.5 + i, 3.5}, "civilian"));
    roster.economy.treasury = 5000;
    const auto run = [&](std::uint64_t off, int days) {
        Society::wageRulesOff(off);
        auto s = std::make_unique<Society>(Roster::None);
        s->configure(roster);
        auto state = s->state();
        state.memory.pace = 0;                      // (Whatever the pace.)
        expect(s->restore(state), "the pace restores");
        const auto bodies = atWork(*s);
        for (std::int64_t d = 1; d <= days; ++d)
            s->tick(1, double(d) + .5 / 24, 0, bodies);   // (The day's pass.)
        Society::wageRulesOff(0);
        return s;
    };
    // (Its hands let go: those without a post, every one still in its home.)
    const auto jobless = [](const Society& s) {
        int n = 0;
        for (int i = 1; i <= 4; ++i)
            n += s.jobOf("hand" + std::to_string(i)) == nullptr;
        return n;
    };
    const auto home = [](const Society& s) {
        int n = 0;
        for (int i = 1; i <= 4; ++i)
            n += s.state().residents.at("hand" + std::to_string(i)).homeCell == "homes";
        return n;
    };
    auto s = run(Society::StallsShed, 1);
    const auto till = s->tillOf("stall");
    expect(!s->makesThings(till) && s->handsOf(till) == 5 && s->handsWanted(till) == 1,
           "the stall makes nothing, has five hands and wants one: " + std::to_string(s->handsOf(till)) + ", " + std::to_string(s->handsWanted(till)));
    s = run(0, 7);
    expect(jobless(*s) == 2 && home(*s) == 4, "a week on, it has let two go, both in their homes: " + std::to_string(jobless(*s)) + ", " +
                                                  std::to_string(home(*s)));
    expect(std::none_of(s->oddJobs().begin(), s->oddJobs().end(), [&](const auto& j) { return j.payer == till; }), "and it hires nobody");
    s = run(0, 21);
    expect(jobless(*s) == 4 && home(*s) == 4, "three weeks on, its keeper alone: " + std::to_string(jobless(*s)) + " let go");
    s = run(Society::StallsShed, 21);
    expect(jobless(*s) == 0, "without the rule, it keeps them all: " + std::to_string(jobless(*s)) + " let go");
    expect(s->conserved(), "money stays conserved");
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
    for (const std::uint64_t off : {std::uint64_t(Society::WageBySales), std::uint64_t(0)})
    {
        Society::wageRulesOff(off);
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
        if (off)
            expect(s.employerWage(bakery) > before * (1 + Society::WageUp * .3 - 1e-9),
                   "and its review bids its wage up: " + std::to_string(before) + "p to " + std::to_string(s.employerWage(bakery)) + "p");
        else
            expect(std::abs(s.employerWage(bakery) - before) < 1e-9 && s.tradeNetOf(bakery) < s.employerWage(bakery) * (1 + open),
                   "with WageBySales its trade can't pay it, and it keeps its wage: " + std::to_string(before) + "p to " +
                       std::to_string(s.employerWage(bakery)) + "p, its trade " + std::to_string(s.tradeNetOf(bakery)) + "p a day");
        expect(s.conserved(), "money stays conserved");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// Round 13, lane 1 (the user, 2026-10-29), with work by the hours: a week an employer fell short paying, it is cut (up
// to 4%), and with WageBookMended that is the week's one move, for every employer (here a household's help, whose
// purse pays it: no WageBySales, its bid always "pays"). Round 12's cut stood apart from the review's chain (its
// second, in the chain, could never fire), so a short week went on to the bid or to a week of calm: its book seven days
// open, it was cut, then bid up; nothing open, CalmWeeks less one calm and the realm over 3% idle, it was cut twice.
// With the rule: cut once, its calm as it was.
void aShortPayWeekNeitherBidsNorCalms()
{
    struct Run
    {
        double wage = 0, before = 0, slack = 0;
        int calm = 0;
    };
    const auto run = [](std::uint64_t off, int openDays) {
        Society::setWorkByHours(true);
        Society::wageRulesOff(off);
        AuthoredRoster roster;
        roster.residents = {person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}), person("stall", "keeping the stall", {"market", 4.5, 4.5}),
                            person("smith", "the smith at the forge", {"forge", 2.5, 2.5}), person("scribe", "the scribe", {"desk", 2.5, 2.5}),
                            person("idle1", "-", {"square", 2.5, 2.5}, "civilian")};
        roster.economy.treasury = 5000;
        Society s(Roster::None);
        s.configure(roster);
        const std::string key = "scribe|help";
        const auto bodies = atWork(s);
        const auto review = std::int64_t(std::hash<std::string>{}(key + "|wage") % 7);
        std::int64_t on = 2;
        while (on % 7 != review)
            ++on;
        for (std::int64_t d = 1; d < on; ++d)
            s.tick(1, double(d) + .5 / 24, 0, bodies);   // (The days' passes before its review: the going rate.)
        Run r;
        r.before = s.goingRate("");
        auto state = s.state();
        state.memory.employers[key] = EmployerBook{r.before, openDays, Society::CalmWeeks - 1, true};
        expect(s.restore(state), "its book restores");
        s.tick(1, double(on) + .5 / 24, 0, bodies);
        const auto& book = s.state().memory.employers.at(key);
        r.wage = book.wage, r.calm = book.calm, r.slack = s.slack();
        expect(!book.shortPay && book.openDays == 0, "its week reviewed");
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        Society::setWorkByHours(false);
        return r;
    };
    const auto show = [](const Run& r) {
        return std::to_string(r.before) + "p to " + std::to_string(r.wage) + "p, calm " + std::to_string(r.calm);
    };
    const auto cut = [](const Run& r) { return r.wage <= r.before + 1e-9 && r.wage >= r.before * (1 - 2 * Society::WageDown) - 1e-9; };
    const auto open = run(0, 7), openBefore = run(Society::WageBookMended, 7);
    expect(cut(open) && open.calm == Society::CalmWeeks - 1, "seven days open but short of its pay: cut once, no bid, no calm: " + show(open));
    expect(openBefore.calm == 0 && openBefore.wage > open.wage + 1e-9, "without the rule, cut, then bid up: " + show(openBefore));
    const auto calm = run(0, 0), calmBefore = run(Society::WageBookMended, 0);
    expect(calm.slack > .03 && calmBefore.slack > .03, "the realm over 3% idle: " + std::to_string(calm.slack));
    expect(cut(calm) && calm.calm == Society::CalmWeeks - 1, "nothing open, short of its pay: cut once, its calm as it was: " + show(calm));
    expect(calmBefore.calm == Society::CalmWeeks && calmBefore.wage < calm.wage - 1e-9,
           "without the rule, a sixth week of calm and cut twice in one review: " + show(calmBefore));
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

// A producer's own hire by its sales as well (doc 64, Phase 13, round 6: the round-5 review). A fishery selling for a
// hand's day plans for two for its bare shelf, as a producer's always is near bare; its till funded, it could post a hire
// for the second, and that hire's slot, untaken, stood open as a post its sales never wanted. Now it hires for its sales:
// no hire, and nothing open, for a hand's day; for three hands' days, a hire for two, and no more open than that.
void aProducersHireBySales()
{
    Society::setWorkByHours(true);
    Society::wageRulesOff(Society::MoreHands);   // (Its till kept at 20,000p: its sales' hands, not round 11's over them.)
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
    // (A fortnight of `n` fish sold a day and eaten, its till kept funded; nobody to take its hire.)
    const auto fortnight = [&](int n) {
        int worst = -100, hired = 0;
        for (int i = 0; i < 14; ++i)
        {
            ++d;
            set(s, "fisher", "fish", n);
            expect(s.shift("treasury", "scribe", "", 0, n, "test: the buyer's money"), "the buyer has money");
            expect(s.sale(till, "scribe", "fish", n, 1, "test: the day's fish"), "the buyer buys");
            auto st = s.state();
            st.accounts.at("scribe").stock.erase("fish");
            expect(s.restore(st), "eaten");
            if (const auto cash = s.account(till)->cash; cash < 20000)
                expect(s.shift("treasury", till, "", 0, 20000 - cash, "test: its till kept funded"), "its till funded");
            s.tick(1, double(d) + .5, 0, bodies);   // (The day's pass.)
            if (i >= 7)
            {
                worst = std::max(worst, s.vacanciesOf(till) - std::max(0, s.handsWanted(till, false) - s.handsOf(till)));
                for (const auto& j : s.oddJobs())
                    if (j.payer == till && j.until >= d)
                        hired = std::max(hired, j.slots);
            }
        }
        return std::make_pair(worst, hired);
    };
    const auto one = fortnight(8);
    expect(s.handsWanted(till) == 2 && s.handsWanted(till, false) == 1,
           "its sales a hand's day, two for its bare shelf: " + std::to_string(s.handsWanted(till)) + ", " + std::to_string(s.handsWanted(till, false)));
    expect(one.first <= 0 && one.second == 0,
           "it posts no hire, and stands no post open past its sales' (" + std::to_string(one.first) + " over, a hire of " + std::to_string(one.second) + ")");
    const auto three = fortnight(24);
    expect(three.first <= 0 && three.second >= 1 && three.second <= 2,
           "its sales three hands' days: a hire for two at most, and no more open (" + std::to_string(three.first) + " over, a hire of " +
               std::to_string(three.second) + ")");
    expect(s.conserved(), "money stays conserved");
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// The pace's brake reads the land's hours as they are credited (doc 64, Phase 13, round 6: the round-5 review): two
// fishers' hours count, a beekeeper's don't (an apiary is idle all winter: its season's end would read as the land
// falling); a fisher leaving the roster mid-day doesn't take its life's hours out of the day's reading, nor leave a gap;
// a restart reads on (the count is saved), and an older save, not yet counting, counts from its next pass.
void theBrakeReadsTheLandsHours()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    roster.residents = {person("fisher", "fishes the lake", {"lake", 4.5, 4.5}, "civilian"),
                        person("fisher2", "fishes the lake", {"lake", 6.5, 4.5}, "civilian"),
                        person("bees", "keeps bees at the meadow", {"meadow", 4.5, 4.5}, "civilian"),
                        person("scribe", "the scribe", {"desk", 2.5, 2.5})};
    roster.economy.treasury = 100000;
    Society s(Roster::None);
    s.configure(roster);
    auto state = s.state();
    state.memory.pace = 1;
    expect(s.restore(state), "the calibrated hours restore");
    const auto worked = [&](const std::string& who) {
        const auto found = s.workBy().find(who);
        return found == s.workBy().end() ? 0. : found->second.first;
    };
    double day = 1 + 10. / 24;
    work(s, 2, day);
    expect(s.state().memory.landToday == 0 && s.state().memory.paceLand.empty(), "counting from the first day's pass, with no reading yet");
    const double fisher = worked("fisher"), fisher2 = worked("fisher2"), bees = worked("bees");
    work(s, 3600, day);
    const double today = worked("fisher") - fisher + worked("fisher2") - fisher2;
    expect(today > 0 && worked("bees") > bees, "the fishers and the beekeeper work the morning");
    expect(std::abs(s.state().memory.landToday - today) < 1e-9,
           "the fishers' hours count as credited, the beekeeper's not (" + std::to_string(s.state().memory.landToday) + " of " +
               std::to_string(today) + ")");
    // Saved, and read back; a bad count, or none (an older save), not counting till the next pass.
    const auto saved = s.state();
    expect(wire::readSociety(wire::society(saved)).memory.landToday == saved.memory.landToday, "the count is saved");
    auto bad = saved;
    bad.memory.landToday = 2 * Society::PaceLandMost;
    expect(wire::readSociety(wire::society(bad)).memory.landToday == -1, "a count out of bounds is dropped");
    auto old = wire::society(saved);
    expect(old.find("memory") && old.find("memory")->erase("landToday"), "(A save from before it)");
    expect(wire::readSociety(old).memory.landToday == -1, "an older save isn't counting yet");
    // A restart, then a fisher leaves the roster: the day's reading at the next pass is every hour credited today.
    expect(s.restore(saved), "a restart mid-morning");
    work(s, 600, day);
    const double more = s.state().memory.landToday;
    {
        auto without = roster;
        without.residents.erase(without.residents.begin() + 1);
        Society candidate(Roster::None);
        candidate.configure(without);
        expect(s.adoptResident(candidate, "fisher2") && !s.spec("fisher2"), "the second fisher leaves");
    }
    day = 2 + 10. / 24;
    work(s, 2, day);
    const auto& lands = s.state().memory.paceLand;
    expect(lands.size() == 1 && lands.back().first == 2 && std::abs(lands.back().second - more) < 1e-9,
           "the next pass reads every hour credited since the last (" + std::to_string(lands.empty() ? -1 : lands.back().second) + " of " +
               std::to_string(more) + ")");
    Society::setWorkByHours(false);
}

// Who pays the capital's posts of last resort (doc 64, Phase 13, round 6: the user's decision, real payers for the posts
// the town paid because nobody else did), with work by the hours: a household's live-in servants by its richest not in
// service itself; a great house's seat staff (the chambers above, the kitchen below, its head's own room) and its
// grounds' (its gardens, its gate: the house its home names, else its town's richest) by the house's own purse; the
// docks' timber haulers by the town's joiners; work for hire by nobody (the household keeps them; they take hires as
// the jobless do); the town's own posts by the town. At the seconds' pace, as it was.
void theCapitalsPostsOfLastResort()
{
    const auto at = [](ResidentSpec r, const std::string& home, double x = 3.5) {
        r.home = {home, x, 2.5};
        r.evening = r.home;
        return r;
    };
    auto lady = at(person("lady", "keeping Ash Hall", {"ash_hall", 2.5, 2.5}, "civilian"), "ash_hall_up");
    auto lord = at(person("lord", "ruling House Fell", {"fellhall", 2.5, 2.5}, "civilian"), "fellhall_up");
    lady.paid = lord.paid = false;
    auto master = at(person("vance", "clerking at the Counting House", {"counting_house", 2.5, 2.5}, "civilian"), "vance_house", 2.5);
    auto rich = at(person("servant1", "in service to the Vances", {"vance_house", 4.5, 4.5}, "civilian"), "vance_house", 4.5);
    auto poor = at(person("servant2", "in service to the Vances", {"vance_house", 5.5, 4.5}, "civilian"), "vance_house", 5.5);
    rich.purse = 5000;                              // (Richer than the master: in service all the same.)
    // (Unpaid posts: one at its own home, a great house's son at the House's counting house.)
    auto homebody = at(person("homebody", "at home in the Heights", {"vance_house", 6.5, 2.5}, "civilian"), "vance_house_up");
    auto son = at(person("son", "counting the House's money", {"counting_house", 4.5, 2.5}, "civilian"), "ash_hall_up", 8.5);
    auto overseer = at(person("overseer", "overseeing the joinery", {"joinery", 6.5, 2.5}, "civilian"), "ash_hall_up", 9.5);
    homebody.paid = son.paid = overseer.paid = false;
    AuthoredRoster roster;
    roster.residents = {lady, lord, master, rich, poor, homebody, son, overseer,
                        at(person("chambers", "keeping the chambers", {"ash_hall_up", 6.5, 4.5}, "civilian"), "ash_hall_up", 6.5),
                        at(person("cook", "cooking for the House", {"ash_hall_down", 2.5, 4.5}, "civilian"), "ash_hall_up", 7.5),
                        person("steward", "counting the House's money", {"ash_hall", 4.5, 4.5}, "civilian"),
                        at(person("gardener", "tending the gardens", {"grounds", 2.5, 2.5}, "civilian"), "the_fell_gatehouse"),
                        at(person("gate", "keeping the gate", {"grounds", 4.5, 2.5}, "civilian"), "ash_cottages"),
                        at(person("hedger", "tending the gardens", {"grounds", 6.5, 2.5}, "civilian"), "drayman_house"),
                        person("carrier", "carries loads for hire", {"street", 2.5, 2.5}, "civilian"),
                        person("porter", "carries loads for hire", {"joinery", 4.5, 4.5}, "civilian"),
                        person("kitchen", "cooking for the household", {"vance_house_down", 2.5, 2.5}, "civilian"),
                        person("hauler", "hauling timber at the docks", {"docks", 2.5, 2.5}, "civilian"),
                        person("joiner", "carpenter at Cedar Row Joiners", {"joinery", 2.5, 2.5}),
                        person("clerk", "clerking at the City Office", {"city_office", 2.5, 2.5}, "civilian")};
    roster.economy.treasury = 100000;
    for (const bool hours : {false, true})
    {
        Society::setWorkByHours(hours);
        Society s(Roster::None);
        s.configure(roster);
        LifeDay town;
        town.communityOf = [](const std::string&) { return std::string("town"); };
        s.setDay(town);
        double day = 1 + 8. / 24;                   // (The day's turn: the houses founded.)
        work(s, 2, day);
        expect(s.account("house:ash_hall") && s.account("house:house_fell"), "two great houses, founded");
        expect(s.shift("house:ash_hall", "house:house_fell", "", 0, 1000, "test: Fell the richer"), "Fell the richer");
        const auto payer = [&](const std::string& who) { return s.payerOf(who, *s.jobOf(who), 30); };
        const auto is = [&](const std::string& who, const std::string& account, const std::string& whom) {
            const auto p = payer(who);
            expect(p.account == account && p.whom == whom, std::string(hours ? "by the hours: " : "at the seconds' pace: ") + who +
                                                               " is paid by " + account + " as " + whom + " (" + p.account + ", " + p.whom + ")");
        };
        is("clerk", "treasury", "the town");
        if (!hours)
        {
            for (const char* who : {"servant1", "servant2", "chambers", "cook", "gardener", "gate", "hedger", "carrier", "hauler", "kitchen"})
                is(who, "treasury", "the town");
            is("steward", "lady", "the house");
            is("porter", s.tillOf("joiner"), "the shop");
            is("homebody", "servant1", "the house");     // (Its household's richest, a servant.)
            is("son", "treasury", "the town");
            is("overseer", s.tillOf("joiner"), "the shop");
            continue;
        }
        is("servant1", "vance", "the house");
        is("servant2", "vance", "the house");
        is("kitchen", "vance", "the house");             // (The kitchen below a household's home.)
        is("homebody", "", "lives on the household");    // (At home: its household's own life.)
        is("son", "house:ash_hall", "the house");
        is("overseer", s.tillOf("joiner"), "the shop");   // (A house's son at a shop: the shop's, as before.)
        is("chambers", "house:ash_hall", "the house");
        is("cook", "house:ash_hall", "the house");
        is("steward", "house:ash_hall", "the house");
        is("gardener", "house:house_fell", "the house");
        is("gate", "house:ash_hall", "the house");
        is("hedger", "house:house_fell", "the house");
        is("carrier", "", "lives on the household");
        is("porter", s.tillOf("joiner"), "the shop");    // (Work for hire where a shop is: the shop's, as before.)
        is("hauler", s.tillOf("joiner"), "the shop");
        expect(Society::forHire("carries loads for hire") && Society::forHire("runs messages about the city") &&
                   !Society::forHire("clerking at the City Office"),
               "work for hire takes hires as the jobless do");
        // Its master moved out: the household above pays; and with none but its servants, the town, as before.
        auto state = s.state();
        state.residents.at("vance").homeCell = "elsewhere";
        expect(s.restore(state), "the master moves out");
        work(s, 1, day);
        is("servant2", "homebody", "the house");
        state.residents.at("homebody").homeCell = "elsewhere";
        expect(s.restore(state), "and the rest of the family");
        work(s, 1, day);
        is("servant2", "treasury", "the town");
    }
    Society::setWorkByHours(false);
}

// A hire taken is no post open (doc 64, Phase 13, the review): a fishery whose sales want three hands' days has its
// own and posts a hire for the rest; three idle wolves of its town take every slot of it, and nothing stands open. (And
// three wolves who carry loads for hire, nobody's post to pay them since round 6: they take hires as the jobless do.)
// Taken by children (twelve, from 14:00, as children take a grown wolf's work), they keep the places, and every place
// stands open still (round 7: the model's, nobody under 16 works).
void aHireTakenIsNoPostOpen(const std::string& takers = "idling in the square", int age = 30)
{
    Society::setWorkByHours(true);
    const bool children = age < 16;
    AuthoredRoster roster;
    roster.residents = {person("fisher", "fishes the lake", {"lake", 4.5, 4.5}, "civilian"), person("scribe", "the scribe", {"desk", 2.5, 2.5})};
    for (int i = 1; i <= 3; ++i)
    {
        roster.residents.push_back(person("idle" + std::to_string(i), takers, {"square", 2.5, 2.5}, "civilian"));
        roster.residents.back().age = age;
    }
    roster.economy.treasury = 100000;
    Society s(Roster::None);
    s.configure(roster);
    auto state = s.state();
    state.memory.pace = 1;
    expect(s.restore(state), "the calibrated hours restore");
    const auto till = s.tillOf("fisher");
    expect(s.shift("treasury", till, "", 0, 20000, "test: the fishery's money"), "the fishery has money to hire with");
    std::map<std::string, LifeBody> bodies = atWork(s);
    for (int i = 1; i <= 3; ++i)
        bodies["idle" + std::to_string(i)].age = age;
    int taken = -1, slots = 0, open = -1, wanted = 0;
    for (std::int64_t d = 1; d <= 28 && open < 0; ++d)
    {
        // (Its hires running into today with every slot taken yesterday, two at least, before the day's pass counts its
        // posts. Since round 6 they may be two hires of one: its hire is sized by its sales as they are.)
        int full = 0, held = 0;
        for (const auto& j : s.oddJobs())
            if (j.payer == till && j.until >= d)
                full += j.slots, held += int(j.stage.size());
        if (full < 2 || held < full)
            full = -1;
        // (The day's pass, then its working hours, the hands at the hire's spot when they hold it. Children's from 14:00.)
        const double start = children ? 14 : 10;
        for (double t = start; t < start + 2; t += 1. / 60)
        {
            s.tick(1, double(d) + t / 24, 0, bodies);
            if (t == start && full > 0 && d >= 8)
            {
                open = s.vacanciesOf(till), slots = full, wanted = s.handsWanted(till, false), taken = 0;
                for (const auto& j : s.oddJobs())
                    if (j.payer == till && j.until >= d)
                        taken += int(j.stage.size());
            }
            for (const auto& j : s.oddJobs())
                if (j.payer == till && j.until >= d)
                    for (const auto& [who, stage] : j.stage)
                        bodies[who] = {j.from.cell, j.from.x, j.from.y, false, bodies[who].age};
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
           "its sales three hands' days, its hires' slots all taken by those " + takers + ": " + std::to_string(taken) + " of " + std::to_string(slots) + " (" +
               std::to_string(wanted) + " wanted)");
    if (!children)
        expect(open == 0, "and no post stands open: " + std::to_string(open));
    else
        expect(open == slots, "children hold them, and every place stands open: " + std::to_string(open) + " of " + std::to_string(slots));
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
    // A working morning to `end`: each minute, the hands at the spot of the hire they hold. (To 10:15 a hand does no
    // share, so it isn't free to take a second place, as it is once its first is done. The idle look from 9:00 for an
    // institution's crew since round 7; before, from 10:00, and the first morning ran to 11:00.)
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
        morning(*s, bodies, 7, 10.25, {});
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
               "its hands' days paid, the town is mended by CrewMend each (0 for now): " + std::to_string(paid) + " paid, " + std::to_string(s->condition("town")));
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
        // Its next week, with 2,000p left: what its budget pays, one. (Without round 10's carry: what the gangs shut left of
        // last week's would go on top, aHouseCarriesWhatItDidntPay.)
        Society::wageRulesOff(Society::CarryParts);
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
        Society::wageRulesOff(0);
        expect(s->conserved(), "money stays conserved");
    }
    {
        auto [s, bodies] = week(2);
        morning(*s, bodies, 7, 10.25, {});
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

// Round 7 (the user, 2026-10-26: the institutions' crews go to grown wage earners first, the model's staff_to): a town's
// works crew and the church's masons by a House's crew rules, and their places the grown's at any hour, before the
// children's; a child's place is the day's. A household's keeper on the crew leaves no post open (round 7's review).
void aTownsCrewTakesTheGrownFirst()
{
    Society::setWorkByHours(true);
    // (Its own town's wolves: with crews abroad, round 9's, asked for, the capital's idle may take it too. And its town's
    // money kept as put there: with round 10's rebate, a town holding fifty times its target pays half of it back at once.)
    Society::wageRulesOff(Society::TaxRebate | Society::RichHomes);   // (And round 11's rich homes: their help is no hire for the children.)
    AuthoredRoster roster;
    const auto far = [](ResidentSpec r) {
        r.home = r.evening = {"far_homes", 2.5, 2.5};
        return r;
    };
    const int keepers = 6, children = 6;
    for (int i = 1; i <= keepers; ++i)
        roster.residents.push_back(far(person("keeper" + std::to_string(i), "keeps the house", {"far_homes", 2.5, 2.5}, "civilian")));
    for (int i = 1; i <= children; ++i)
    {
        auto child = far(person("child" + std::to_string(i), "plays in the square", {"far_square", 2.5, 2.5}, "civilian"));
        child.purse = 0;                             // (Poor: it looks every afternoon.)
        roster.residents.push_back(child);
    }
    roster.residents.push_back(far(person("stall", "keeping the stall", {"far_market", 2.5, 2.5})));   // (Where its gangs work.)
    // (The stall's hand keeps its house: its post isn't opened by its days on the crew.)
    auto hand = far(person("hand", "a hand at the stall", {"far_market", 2.5, 2.5}, "civilian"));
    hand.home = hand.evening = {"far_cottage", 2.5, 2.5};
    hand.purse = 100000;
    roster.residents.push_back(hand);
    for (int i = 1; i <= 135; ++i)
        roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
    roster.economy.treasury = 1000000;
    Society s(Roster::None);
    s.configure(roster);
    s.setCapital("town");
    LifeDay plan;
    plan.communityOf = [](const std::string& cell) { return cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town"); };
    s.setDay(plan);
    auto state = s.state();
    state.memory.pace = 1;
    state.memory.keeper["far_cottage"] = "hand";
    expect(s.restore(state), "the calibrated hours restore");
    s.openAccount("stores:far");
    auto bodies = atWork(s);
    for (int i = 1; i <= children; ++i)
        bodies["child" + std::to_string(i)].age = 12;
    const auto rested = [&] {
        auto st = s.state();
        for (auto& [id, life] : st.residents)
            life.hunger = life.fatigue = 0;
        expect(s.restore(st), "rested");
    };
    for (std::int64_t d = 1; d <= 6; ++d)
    {
        s.tick(1, double(d) + .5 / 24, 0, bodies);
        rested();
    }
    // The reckoning's eve: the town's money and the church's.
    expect(s.shift("treasury", "stores:far", "", 0, 100000, "test: the town's money") &&
               s.shift("treasury", Society::SharedChurch, "", 0, 100000, "test: the church's money"),
           "the town and the church have money");
    s.tick(1, 7 + .5 / 24, 0, bodies);
    expect(s.estateMost() == 14, "a tenth of its 143 grown wolves: " + std::to_string(s.estateMost()));
    const auto crewOf = [&](const std::string& payer) {
        int slots = 0, gangs = 0;
        for (const auto& j : s.oddJobs())
            if (j.payer == payer && j.kind == "building work" && j.until >= 7)
                slots += j.slots, ++gangs;
        return std::make_pair(slots, gangs);
    };
    const auto works = crewOf("stores:far"), masons = crewOf(Society::SharedChurch);
    expect(works.first == 14 && works.second == 2, "the town's works crew is what its budget pays, up to a tenth of the realm, in gangs of twelve: " +
                                                       std::to_string(works.first) + " in " + std::to_string(works.second));
    expect(masons.first == 14 && masons.second == 2,
           "and the church's masons: " + std::to_string(masons.first) + " in " + std::to_string(masons.second));
    // The day: each minute, the hands at the spot of the place they hold.
    const auto until = [&](std::int64_t d, double from, double end) {
        for (double t = from; t < end; t += 1. / 60)
        {
            s.tick(60, double(d) + t / 24, 0, bodies);
            std::set<std::string> placed;
            for (const auto& j : s.oddJobs())
                for (const auto& [who, stage] : j.stage)
                    if (stage < 2 && !j.ended && placed.insert(who).second)
                        bodies[who] = {j.from.cell, j.from.x, j.from.y, false, bodies[who].age};
        }
    };
    const auto on = [&](const std::string& who) {
        int n = 0;
        for (int i = 1; i <= (who == "keeper" ? keepers : children); ++i)
            for (const auto& j : s.oddJobs())
                if (j.payer == "stores:far" && j.stage.count(who + std::to_string(i)))
                {
                    ++n;
                    break;
                }
        return n;
    };
    until(7, 8, 9.5);
    expect(on("keeper") == keepers && on("child") == 0,
           "by half past nine the grown keeping the house hold the town's places, and no child: " + std::to_string(on("keeper")) + ", " +
               std::to_string(on("child")));
    until(7, 9.5, 13.5);
    rested();                                        // (Fed by noon.)
    until(7, 13.5, 16);
    const auto heldNow = [&] {
        std::string out;
        for (const auto& j : s.oddJobs())
            if (j.payer == "stores:far" && j.until >= 7)
                for (const auto& [who, stage] : j.stage)
                    out += " " + who + ":" + std::to_string(stage);
        return out;
    };
    // (One place each since round 9: a child holding another hire's place, done for the day, takes none of the crew's.)
    std::map<std::string, int> childPlaces;
    for (const auto& j : s.oddJobs())
        for (const auto& [who, stage] : j.stage)
            if (j.until >= 0 && who.rfind("child", 0) == 0)
                ++childPlaces[who];
    const bool onePlace = std::all_of(childPlaces.begin(), childPlaces.end(), [](const auto& c) { return c.second == 1; });
    expect(on("child") >= 1 && childPlaces.size() >= 2 && onePlace,
           "from mid-afternoon the children take what's left, those not holding another hire's place, one place each: " +
               std::to_string(on("child")) + " of " + std::to_string(childPlaces.size()) + " (" + heldNow() + ")");
    // The next day's pass: a child's place is the day's, the grown keep theirs the week.
    s.tick(1, 8 + .5 / 24, 0, bodies);
    expect(on("child") == 0 && on("keeper") == keepers,
           "at the day's pass the children's places open again, the grown's stand: " + std::to_string(on("child")) + ", " + std::to_string(on("keeper")));
    bool handOn = false;
    for (const auto& j : s.oddJobs())
        handOn = handOn || (j.payer == "stores:far" && j.stage.count("hand"));
    expect(s.state().memory.keeper.count("far_cottage") && handOn && s.vacanciesOf(s.tillOf("stall")) == 0,
           "the stall's hand, keeping its house, holds a place on the crew, and its post at the stall isn't open for it: " +
               std::to_string(handOn) + ", " + std::to_string(s.vacanciesOf(s.tillOf("stall"))));
    // (Before round 9 a keeper whose first share was done by mid-morning might hold a second place: one now.)
    int held = 0;
    for (const auto& j : s.oddJobs())
        held += j.payer == "stores:far" && j.until >= 8 ? int(j.stage.size()) : 0;
    expect(held >= keepers && s.vacanciesOf("stores:far|crew") == works.first - held,
           "the town's places not taken are posts open: " + std::to_string(s.vacanciesOf("stores:far|crew")) + " (" + std::to_string(held) + " held)");
    expect(s.conserved(), "money stays conserved");
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// Round 8 (the user, 2026-10-27): only the market stalls and the moneychangers let their surplus help go. A stall with
// three hands, a moneychanger with two and a fletcher with three, each wanting one, its keeper: three weeks on the stall
// and the moneychanger have let all theirs go, and the fletcher, which makes nothing either, keeps its three. (Round 7's
// rule, every business with no craft of its own, let the fletcher's go too.) And no apprentice is let go, nor counted in
// the excess (the model's firm_week: apprentices are hands over and above): a second stall's first hand, apprenticed to
// its keeper, is kept four weeks on, its other three let go, one a week; without the rule it goes with them by the third
// week, two in the first.
void onlyTheSellersLetGo()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    roster.residents = {person("stall", "keeping the stall", {"market", 4.5, 4.5}), person("changer", "moneychanger at the bank", {"bank", 2.5, 2.5}),
                        person("fletcher", "the fletcher at the Goose", {"goose", 2.5, 2.5}), person("scribe", "the scribe", {"desk", 2.5, 2.5}),
                        person("stall2", "keeping the stall", {"market2", 4.5, 4.5})};
    const std::vector<std::pair<std::string, int>> shops = {{"stall", 3}, {"changer", 2}, {"fletcher", 3}, {"stall2", 4}};
    for (std::size_t k = 0; k < shops.size(); ++k)
        for (int i = 1; i <= shops[k].second; ++i)
            roster.residents.push_back(person(shops[k].first + "_hand" + std::to_string(i), "works at the " + shops[k].first,
                                              {roster.residents[k == 3 ? 4 : k].work.cell, 4.5 + i, 3.5}, "civilian"));
    roster.economy.treasury = 5000;
    const auto run = [&](std::uint64_t off, int days) {
        Society::wageRulesOff(off);
        auto s = std::make_unique<Society>(Roster::None);
        s->configure(roster);
        auto state = s->state();
        state.memory.pace = 0;                      // (Whatever the pace.)
        state.careers.positions["job:stall2"].apprentice = "stall2_hand1";
        expect(s->restore(state), "the pace and the apprenticeship restore");
        const auto bodies = atWork(*s);
        for (std::int64_t d = 1; d <= days; ++d)
            s->tick(1, double(d) + .5 / 24, 0, bodies);   // (The day's pass.)
        Society::wageRulesOff(0);
        return s;
    };
    const auto gone = [](const Society& s, const std::string& shop, int n) {
        int out = 0;
        for (int i = 1; i <= n; ++i)
            out += s.jobOf(shop + "_hand" + std::to_string(i)) == nullptr;
        return out;
    };
    auto s = run(0, 1);
    for (const auto& [shop, n] : shops)
    {
        const auto till = s->tillOf(shop);
        expect(!s->makesThings(till) && s->handsOf(till) == 1 + n && s->handsWanted(till) == 1,
               shop + " makes nothing, has its hands and wants one: " + std::to_string(s->handsOf(till)) + ", " + std::to_string(s->handsWanted(till)));
    }
    expect(s->sellsOnly(s->tillOf("stall")) && s->sellsOnly(s->tillOf("changer")) && !s->sellsOnly(s->tillOf("fletcher")),
           "the stall and the moneychanger only sell; the fletcher is another business");
    expect(s->apprenticedTo("stall2_hand1") != nullptr, "the second stall's first hand is apprenticed to its keeper");
    s = run(0, 14);
    expect(gone(*s, "stall2", 4) == 2, "two weeks on the second stall has let two go, one a week: its apprentice isn't in the excess: " +
                                           std::to_string(gone(*s, "stall2", 4)));
    s = run(0, 21);
    expect(gone(*s, "stall", 3) == 3 && gone(*s, "changer", 2) == 2, "three weeks on, the stall's and the moneychanger's hands are let go: " +
                                                                          std::to_string(gone(*s, "stall", 3)) + ", " + std::to_string(gone(*s, "changer", 2)));
    expect(gone(*s, "fletcher", 3) == 0, "and the fletcher keeps its three: " + std::to_string(gone(*s, "fletcher", 3)) + " let go");
    expect(gone(*s, "stall2", 4) == 3 && s->jobOf("stall2_hand1") && s->apprenticedTo("stall2_hand1"),
           "the second stall has let its three hands go, not its apprentice: " + std::to_string(gone(*s, "stall2", 4)));
    s = run(0, 28);
    expect(gone(*s, "stall2", 4) == 3 && s->jobOf("stall2_hand1"), "four weeks on the apprentice is still kept: " + std::to_string(gone(*s, "stall2", 4)));
    expect(s->conserved(), "money stays conserved");
    s = run(Society::SellersOnly, 21);
    expect(gone(*s, "fletcher", 3) == 3, "with round 7's rule, the fletcher lets its three go too: " + std::to_string(gone(*s, "fletcher", 3)));
    s = run(Society::KeepApprentices, 21);
    expect(gone(*s, "stall2", 4) == 4, "without the apprentices' rule, the apprentice goes with them: " + std::to_string(gone(*s, "stall2", 4)));
    Society::setWorkByHours(false);
}

// A maker keeps its apprentice too (round 8, the user, 2026-10-27; the model's firm_week: apprentices are hands over and
// above), at the calibrated hours, where a maker lets its surplus help go. A bakery selling nothing wants one hand, its
// baker, and has four besides, the first apprenticed to the baker. A week on it has let one go, its excess three less
// its apprentice; three weeks on the other three, and the apprentice is kept. (Round 8's review: the stalls' test had
// it, the makers' step not.) Without the rule, two the first week, and by the third the apprentice with them.
void aMakerKeepsItsApprentice()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    roster.residents = {person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}), person("scribe", "the scribe", {"desk", 2.5, 2.5})};
    for (int i = 1; i <= 4; ++i)
        roster.residents.push_back(person("hand" + std::to_string(i), "works at The Loaf", {"bakery", 3.5 + i, 3.5}, "civilian"));
    roster.economy.treasury = 5000;
    const auto run = [&](std::uint64_t off, int days) {
        Society::wageRulesOff(off);
        auto s = std::make_unique<Society>(Roster::None);
        s->configure(roster);
        auto state = s->state();
        state.memory.pace = 1;                      // (The calibrated hours: a maker lets go only at them.)
        state.careers.positions["job:baker"].apprentice = "hand1";
        expect(s->restore(state), "the pace and the apprenticeship restore");
        const auto bodies = atWork(*s);
        for (std::int64_t d = 1; d <= days; ++d)
            s->tick(1, double(d) + .5 / 24, 0, bodies);   // (The day's pass.)
        Society::wageRulesOff(0);
        return s;
    };
    const auto gone = [](const Society& s) {
        int out = 0;
        for (int i = 1; i <= 4; ++i)
            out += s.jobOf("hand" + std::to_string(i)) == nullptr;
        return out;
    };
    auto s = run(0, 1);
    const auto till = s->tillOf("baker");
    expect(s->makesThings(till) && s->handsOf(till) == 5 && s->handsWanted(till) == 1 && s->apprenticedTo("hand1"),
           "the bakery makes things, has five hands, wants one, and its first is apprenticed: " + std::to_string(s->handsOf(till)) + ", " +
               std::to_string(s->handsWanted(till)));
    s = run(0, 7);
    expect(gone(*s) == 1 && s->jobOf("hand1"), "a week on it has let one go, not its apprentice: " + std::to_string(gone(*s)));
    s = run(0, 21);
    expect(gone(*s) == 3 && s->jobOf("hand1"), "three weeks on, the other three, its apprentice kept: " + std::to_string(gone(*s)));
    expect(s->conserved(), "money stays conserved");
    s = run(Society::KeepApprentices, 7);
    expect(gone(*s) == 2, "without the rule, two the first week: " + std::to_string(gone(*s)));
    s = run(Society::KeepApprentices, 21);
    expect(gone(*s) == 4, "and by the third the apprentice with them: " + std::to_string(gone(*s)));
    Society::setWorkByHours(false);
}

// A producer's own post is never let go (round 9; round 8's review): a producer's own post, its farmer's, its fisher's,
// is among its till's posts (a maker's keeper's isn't), and with only the keeper's own apprentice kept over and above
// a keeper apprenticed to a neighbour is no longer counted out of the excess. At the calibrated hours a fishery wanting
// one hand has its fisher and the fisher's apprentice, on half pay from its till: three weeks on both keep their posts,
// the apprentice over and above. With the apprentices' rule left out the apprentice is in the excess, and a week on it
// is let go, not the fisher, whose post comes first. (Without the guard the fisher went.)
void aProducerKeepsItsOwnPost()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    roster.residents = {person("fisher", "fishes the lake", {"lake", 4.5, 4.5}, "civilian"),
                        person("learner", "apprenticed: fishing", {"lake", 5.5, 3.5}, "civilian")};
    roster.economy.treasury = 5000;
    const auto run = [&](std::uint64_t off, int days) {
        Society::wageRulesOff(off);
        auto s = std::make_unique<Society>(Roster::None);
        s->configure(roster);
        auto state = s->state();
        state.memory.pace = 1;                      // (The calibrated hours: a maker lets go only at them.)
        state.careers.positions["job:fisher"].apprentice = "learner";
        expect(s->restore(state), "the pace and the apprenticeship restore");
        const auto bodies = atWork(*s);
        for (std::int64_t d = 1; d <= days; ++d)
            s->tick(1, double(d) + .5 / 24, 0, bodies);   // (The day's pass.)
        Society::wageRulesOff(0);
        return s;
    };
    auto s = run(0, 1);
    const auto till = s->tillOf("fisher");
    const auto posts = s->helpPosts(till);
    expect(s->handsOf(till) == 2 && s->handsWanted(till) == 1 && posts.size() == 2 && posts.front() == "job:fisher",
           "the fishery has its fisher and its apprentice, the fisher's post first among its till's, and wants one: " +
               std::to_string(s->handsOf(till)));
    s = run(0, 22);
    expect(s->jobOf("fisher") && s->jobOf("learner"), "three weeks on both keep their posts, the apprentice over and above");
    s = run(Society::KeepApprentices, 8);
    expect(s->jobOf("fisher") && s->jobOf("fisher")->id == "job:fisher" && !s->jobOf("learner"),
           "with the apprentices' rule left out a week on the apprentice is let go, not the fisher");
    expect(s->conserved(), "money stays conserved");
    Society::setWorkByHours(false);
}

// Round 8 (the user, 2026-10-27: the middle course): a wolf at a post nobody pays it for looks for an institution's crew
// from waking, before the children; one out of work keeps its old hours and its old look, the businesses' and the farms'
// hires weighed with the crews by pay. A far town's works crew of six: three keeping the house hold places by half past
// nine, the three idling in the square none; from ten the idlers take the rest. (Round 7's rule, the out of work at any
// hour too: all six by half past nine.)
void theOutOfWorkKeepTheirHours()
{
    Society::setWorkByHours(true);
    const auto run = [](std::uint64_t off) {
        Society::wageRulesOff(off);
        AuthoredRoster roster;
        const auto far = [](ResidentSpec r) {
            r.home = r.evening = {"far_homes", 2.5, 2.5};
            return r;
        };
        for (int i = 1; i <= 3; ++i)
        {
            roster.residents.push_back(far(person("keeper" + std::to_string(i), "keeps the house", {"far_homes", 2.5, 2.5}, "civilian")));
            roster.residents.push_back(far(person("idler" + std::to_string(i), "idling in the square", {"far_square", 2.5, 2.5}, "civilian")));
        }
        roster.residents.push_back(far(person("stall", "keeping the stall", {"far_market", 2.5, 2.5})));   // (Where its gang works.)
        for (int i = 1; i <= 60; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string& cell) { return cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town"); };
        s.setDay(plan);
        auto state = s.state();
        state.memory.pace = 1;
        expect(s.restore(state), "the calibrated hours restore");
        s.openAccount("stores:far");
        auto bodies = atWork(s);
        const auto rested = [&] {
            auto st = s.state();
            for (auto& [id, life] : st.residents)
                life.hunger = life.fatigue = 0;
            expect(s.restore(st), "rested");
        };
        for (std::int64_t d = 1; d <= 6; ++d)
        {
            s.tick(1, double(d) + .5 / 24, 0, bodies);
            rested();
        }
        expect(s.shift("treasury", "stores:far", "", 0, 100000, "test: the town's money"), "the town has money");
        s.tick(1, 7 + .5 / 24, 0, bodies);
        int places = 0;
        for (const auto& j : s.oddJobs())
            places += j.payer == "stores:far" && j.kind == "building work" && j.until >= 7 ? j.slots : 0;
        expect(places == 6, "the town's works crew is six, a tenth of its 67 grown wolves: " + std::to_string(places));
        const auto until = [&](double from, double end) {
            for (double t = from; t < end; t += 1. / 60)
            {
                s.tick(60, 7 + t / 24, 0, bodies);
                std::set<std::string> placed;
                for (const auto& j : s.oddJobs())
                    for (const auto& [who, stage] : j.stage)
                        if (stage < 2 && !j.ended && placed.insert(who).second)
                            bodies[who] = {j.from.cell, j.from.x, j.from.y, false, bodies[who].age};
            }
        };
        const auto on = [&](const std::string& who) {
            int n = 0;
            for (int i = 1; i <= 3; ++i)
                for (const auto& j : s.oddJobs())
                    if (j.payer == "stores:far" && j.stage.count(who + std::to_string(i)))
                    {
                        ++n;
                        break;
                    }
            return n;
        };
        until(6, 9.5);
        const auto early = std::make_pair(on("keeper"), on("idler"));
        until(9.5, 11);
        const auto later = std::make_pair(on("keeper"), on("idler"));
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return std::make_pair(early, later);
    };
    const auto [early, later] = run(0);
    expect(early.first == 3 && early.second == 0, "by half past nine those keeping the house hold places, and none idling in the square: " +
                                                      std::to_string(early.first) + ", " + std::to_string(early.second));
    expect(later.first == 3 && later.second == 3, "from ten the idlers take the rest: " + std::to_string(later.first) + ", " + std::to_string(later.second));
    const auto [before, after] = run(Society::UnpaidFirst);
    (void)after;
    expect(before.second == 3, "with round 7's rule, the idlers too by half past nine: " + std::to_string(before.second));
    Society::setWorkByHours(false);
}

// Round 10's rules for the towns (lane B): left out where a test is of the rules before them.
constexpr std::uint64_t Round10Towns = Society::TaxEase | Society::TownPosts | Society::TaxRebate | Society::FoundAtTarget | Society::CapitalLabour;
// (And round 11's lane 2, rich homes and the holdings tax, left out of the tests before them: their towns and hires as built.)
constexpr std::uint64_t Round11Homes = Society::RichHomes | Society::HoldingsTax;

// Round 8 (the user, 2026-10-27): a town's caravans' net takings count in its budget as its tax does. Its road money out
// counts against it (its month below nothing until the takings come home, and kept so on a day it earns nothing), its
// takings for it, and what its stores paid for the caravan's unsold goods against it (that money came home in the
// takings). So its next week's
// works crew is the larger. The capital's treasury trading, and the church, are as they were. (Before, and without the
// rule, a town's month was its tax alone.) Round 9 moved the net takings to the week's budget: this is round 8's form,
// with round 9's rule left out.
void aTownCountsItsCaravans()
{
    Society::setWorkByHours(true);
    struct Seen
    {
        double low = 0, month = 0, capital = 0, church = 0, saved = 0, below = 0;
        int crew = 0;
    };
    const auto run = [](std::uint64_t off) {
        Society::wageRulesOff(off | Round10Towns | Round11Homes);   // (Round 10's towns' rules left out: the forms before them.)
        AuthoredRoster roster;
        const auto far = [](ResidentSpec r) {
            r.home = r.evening = {"far_homes", 2.5, 2.5};
            return r;
        };
        roster.residents.push_back(far(person("stall", "keeping the stall", {"far_market", 2.5, 2.5})));
        for (int i = 1; i <= 3; ++i)
            roster.residents.push_back(far(person("keeper" + std::to_string(i), "keeps the house", {"far_homes", 2.5, 2.5}, "civilian")));
        for (int i = 1; i <= 140; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string& cell) { return cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town"); };
        s.setDay(plan);
        for (const auto* id : {"stores:far", "caravan:far1", "caravan:town1"})
            s.openAccount(id);
        const auto bodies = atWork(s);
        for (std::int64_t d = 1; d <= 29; ++d)
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        Seen seen;
        // Day 29: the town's caravan sets out with its road money; the capital's too.
        expect(s.shift("treasury", "stores:far", "", 0, 2000, "test: the town's money") &&
                   s.shift("stores:far", "caravan:far1", "", 0, 1000, "money for the road") &&
                   s.shift("treasury", "caravan:town1", "", 0, 1000, "money for the road"),
               "the caravans set out");
        const double month0 = s.monthIncomeOf("stores:far"), capital0 = s.monthIncomeOf("treasury"), church0 = s.monthIncomeOf(Society::SharedChurch);
        s.tick(1, 30 + .5 / 24, 0, bodies);
        s.tick(1, 31 + .5 / 24, 0, bodies);         // (A day with nothing earned: its month below nothing is kept.)
        seen.low = s.monthIncomeOf("stores:far") - month0;
        // (Saved and read again as it is, below nothing too: the review.)
        seen.below = s.monthIncomeOf("stores:far");
        const auto reread = wire::readSociety(wire::society(s.state())).memory.monthIncome;
        seen.saved = reread.count("stores:far") ? reread.at("stores:far") : 0;
        // Day 31: home, with their takings, and the town's stores buy back what its caravan didn't sell.
        auto st = s.state();
        st.accounts.at("caravan:far1").stock["bread"] = 10;
        expect(s.restore(st), "the caravan's unsold bread");
        expect(s.shift("treasury", "caravan:far1", "", 0, 8000, "test: sold on the road") &&
                   s.shift("treasury", "caravan:town1", "", 0, 3000, "test: sold on the road") &&
                   s.sale("caravan:far1", "stores:far", "bread", 10, 50, "unsold, back to the stores") &&
                   s.shift("caravan:far1", "stores:far", "", 0, 9500, "trade takings") &&
                   s.shift("caravan:town1", "treasury", "", 0, 4000, "trade takings"),
               "the caravans come home");
        s.tick(1, 32 + .5 / 24, 0, bodies);
        seen.month = s.monthIncomeOf("stores:far") - month0;
        seen.capital = s.monthIncomeOf("treasury") - capital0;
        seen.church = s.monthIncomeOf(Society::SharedChurch) - church0;
        for (std::int64_t d = 33; d <= 35; ++d)
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        for (const auto& j : s.oddJobs())
            seen.crew += j.payer == "stores:far" && j.kind == "building work" && j.until >= 35 ? j.slots : 0;
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    // (Round 8's form: since round 9 the net takings go to the week's budget instead, aTownSpendsItsTradeAsItComes.)
    const auto now = run(Society::CaravansAsTheyCome), before = run(Society::CaravanIncome);
    const double decay = double(MonthDays - 1) / MonthDays;
    expect(now.below < 0 && std::abs(now.saved - now.below) < 1e-6,
           "a month below nothing is saved as it is: " + std::to_string(now.below) + ", read back " + std::to_string(now.saved));
    expect(std::abs(now.low - before.low + 1000 * decay) < 1e-6,
           "its road money out counts against its month, below nothing until the takings come: " + std::to_string(now.low - before.low));
    expect(std::abs(now.month - before.month - (9000 - 1000 * decay * decay)) < 1e-6,
           "home, its month has its net takings, 9,500p less 1,000p of road money and 500p for its unsold bread: " +
               std::to_string(now.month - before.month));
    expect(std::abs(now.capital - before.capital) < 1e-6 && std::abs(now.church - before.church) < 1e-6,
           "the capital's treasury and the church are as they were: " + std::to_string(now.capital - before.capital) + ", " +
               std::to_string(now.church - before.church));
    expect(now.crew > before.crew, "and its next week's works crew is the larger: " + std::to_string(now.crew) + " against " + std::to_string(before.crew));
    Society::setWorkByHours(false);
}

// Round 9 (the user, 2026-10-27): a town's caravans' net takings go to its next week's budget on top of its tax's, spent
// as they come, and its month (two of which its budget holds) is its tax alone. Its caravan sets out on day 33 with
// 1,000p: the week's budget on day 35 adds nothing for it, and the 1,000p stays to be netted against the takings, kept so
// through a save. Home on day 36 with 9,500p, its stores paying 500p for the unsold bread: 8,000p, all of it in the
// day-42 budget, and that week's works crew larger than with its tax alone, and than round 8's. Its month is as it is
// without its caravans. (Round 8 counted the takings in the month, and Oakhaven held two months of its trade.)
void aTownSpendsItsTradeAsItComes()
{
    Society::setWorkByHours(true);
    struct Seen
    {
        double out = 0, saved = 0, home = 0, after = 0, month35 = 0, month42 = 0;
        int crew35 = 0, crew42 = 0;
    };
    const auto run = [](std::uint64_t off) {
        Society::wageRulesOff(off | Round10Towns | Round11Homes);   // (Round 10's towns' rules left out: the forms before them.)
        AuthoredRoster roster;
        const auto far = [](ResidentSpec r) {
            r.home = r.evening = {"far_homes", 2.5, 2.5};
            return r;
        };
        roster.residents.push_back(far(person("stall", "keeping the stall", {"far_market", 2.5, 2.5})));
        for (int i = 1; i <= 3; ++i)
            roster.residents.push_back(far(person("keeper" + std::to_string(i), "keeps the house", {"far_homes", 2.5, 2.5}, "civilian")));
        for (int i = 1; i <= 140; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string& cell) { return cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town"); };
        s.setDay(plan);
        for (const auto* id : {"stores:far", "caravan:far1"})
            s.openAccount(id);
        const auto bodies = atWork(s);
        const auto trade = [&] {
            const auto& week = s.state().memory.caravanWeek;
            return week.count("stores:far") ? week.at("stores:far") : 0.;
        };
        const auto crew = [&](std::int64_t from) {
            int slots = 0;
            for (const auto& j : s.oddJobs())
                slots += j.payer == "stores:far" && j.kind == "building work" && j.until >= from ? j.slots : 0;
            return slots;
        };
        for (std::int64_t d = 1; d <= 33; ++d)
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        Seen seen;
        expect(s.shift("treasury", "stores:far", "", 0, 2000, "test: the town's money") &&
                   s.shift("stores:far", "caravan:far1", "", 0, 1000, "money for the road"),
               "the caravan sets out");
        for (std::int64_t d = 34; d <= 35; ++d)
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        seen.out = trade();
        const auto reread = wire::readSociety(wire::society(s.state())).memory.caravanWeek;
        seen.saved = reread.count("stores:far") ? reread.at("stores:far") : 0;
        seen.month35 = s.monthIncomeOf("stores:far");
        seen.crew35 = crew(35);
        auto st = s.state();
        st.accounts.at("caravan:far1").stock["bread"] = 10;
        expect(s.restore(st), "the caravan's unsold bread");
        expect(s.shift("treasury", "caravan:far1", "", 0, 8000, "test: sold on the road") &&
                   s.sale("caravan:far1", "stores:far", "bread", 10, 50, "unsold, back to the stores") &&
                   s.shift("caravan:far1", "stores:far", "", 0, 9500, "trade takings"),
               "the caravan comes home");
        s.tick(1, 36 + .5 / 24, 0, bodies);
        seen.home = trade();
        for (std::int64_t d = 37; d <= 42; ++d)
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        seen.after = trade();
        seen.month42 = s.monthIncomeOf("stores:far");
        seen.crew42 = crew(42);
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto now = run(0), round8 = run(Society::CaravansAsTheyCome), tax = run(Society::CaravanIncome);
    expect(now.out == -1000 && now.saved == -1000 && now.crew35 == tax.crew35,
           "its road money out, the day-35 budget adds nothing, and the 1,000p stays to be netted, through a save too: " + std::to_string(now.out) +
               ", read back " + std::to_string(now.saved) + "; a crew of " + std::to_string(now.crew35) + " against " + std::to_string(tax.crew35));
    expect(now.home == 8000 && now.after == 0, "home, 9,500p less the road money and 500p for its unsold bread, all spent on day 42: " +
                                                   std::to_string(now.home) + ", then " + std::to_string(now.after));
    expect(now.crew42 > tax.crew42 && now.crew42 > round8.crew42, "that week's works crew the larger: " + std::to_string(now.crew42) + " against " +
                                                                       std::to_string(tax.crew42) + " (its tax alone), " +
                                                                       std::to_string(round8.crew42) + " (round 8)");
    expect(std::abs(now.month35 - tax.month35) < 1e-6 && std::abs(now.month42 - tax.month42) < 1e-6 && round8.month42 > tax.month42 + 1000,
           "its month is its tax alone, as without its caravans: " + std::to_string(now.month42) + " against " + std::to_string(tax.month42) +
               " (round 8: " + std::to_string(round8.month42) + ")");
    Society::setWorkByHours(false);
}

// Round 9's follow-up (the user, 2026-10-27): a town's caravans' money pays its own posts first, and its works get only
// what's left. A far town keeps a watch of three, paid 1,500p a week, on a purse of 4,000p and no tax: the watch's wages
// come to more than its week's tax budget, and what they come to over it is owed. Its caravan sets out on day 33 with 1,000p and comes home on
// day 36 with 5,000p: the day-42 budget makes good what the watch was owed (kept through a save) before its works crew,
// which is the smaller by it, and nothing is owed after. Without the rule the whole 4,000p goes to the works.
void aTownsPostsComeFirst()
{
    Society::setWorkByHours(true);
    struct Seen
    {
        double owed35 = 0, saved = 0, owed42 = 0;
        int crew42 = 0;
    };
    const auto run = [](std::uint64_t off) {
        Society::wageRulesOff(off | Round10Towns | Round11Homes);   // (Round 10's towns' rules left out: the forms before them.)
        AuthoredRoster roster;
        const auto far = [](ResidentSpec r) {
            r.home = r.evening = {"far_homes", 2.5, 2.5};
            return r;
        };
        roster.residents.push_back(far(person("stall", "keeping the stall", {"far_market", 2.5, 2.5})));
        for (int i = 1; i <= 3; ++i)
            roster.residents.push_back(far(person("watch" + std::to_string(i), "keeps the watch", {"far_gate", 2.5 + i, 2.5}, "guard")));
        for (int i = 1; i <= 3; ++i)
            roster.residents.push_back(far(person("keeper" + std::to_string(i), "keeps the house", {"far_homes", 2.5, 2.5}, "civilian")));
        for (int i = 1; i <= 140; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string& cell) { return cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town"); };
        s.setDay(plan);
        for (const auto* id : {"stores:far", "caravan:far1"})
            s.openAccount(id);
        expect(s.shift("treasury", "stores:far", "", 0, 4000, "test: the town's money"), "the town's purse");
        const auto bodies = atWork(s);
        const auto owed = [&](const EconomyMemory& m) { return m.postsOwed.count("stores:far") ? m.postsOwed.at("stores:far") : 0.; };
        const auto crew = [&](std::int64_t from) {
            int slots = 0;
            for (const auto& j : s.oddJobs())
                slots += j.payer == "stores:far" && j.kind == "building work" && j.until >= from ? j.slots : 0;
            return slots;
        };
        for (std::int64_t d = 1; d <= 33; ++d)
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        // (Its watch's week's wages, paid as the day's spells would: only the day's pass is ticked here.)
        const auto payWatch = [&] {
            for (int i = 1; i <= 3; ++i)
                expect(s.shift("stores:far", "watch" + std::to_string(i), "", 0, 500, "watch wages"), "the watch is paid");
        };
        payWatch();
        expect(s.shift("stores:far", "caravan:far1", "", 0, 1000, "money for the road"), "the caravan sets out");
        for (std::int64_t d = 34; d <= 35; ++d)
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        Seen seen;
        seen.owed35 = owed(s.state().memory);
        seen.saved = owed(wire::readSociety(wire::society(s.state())).memory);
        expect(s.shift("treasury", "caravan:far1", "", 0, 8000, "test: sold on the road") &&
                   s.shift("caravan:far1", "stores:far", "", 0, 5000, "trade takings"),
               "the caravan comes home");
        for (std::int64_t d = 36; d <= 42; ++d)
        {
            if (d == 41)
                payWatch();
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        }
        seen.owed42 = owed(s.state().memory);
        seen.crew42 = crew(42);
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto now = run(0), before = run(Society::PostsFirst);
    expect(now.owed35 > 500 && now.saved == now.owed35 && before.owed35 == 0,
           "its watch's wages over its tax budget are owed, through a save too: " + std::to_string(now.owed35) + ", read back " +
               std::to_string(now.saved));
    expect(now.owed42 == 0 && now.crew42 < before.crew42 && now.crew42 > 0,
           "the caravan's money makes it good, and its works crew is the smaller by it: " + std::to_string(now.crew42) + " against " +
               std::to_string(before.crew42) + ", owed " + std::to_string(now.owed42));
    Society::setWorkByHours(false);
}

// Round 9's follow-up (the user, 2026-10-27): a town's works crew may be taken by wolves of other towns. A far town, with
// three of its own keeping the house, posts a works crew of more than three: by noon its own three hold places and the
// capital's idle the rest. The capital's own crews stay its town's. Without the rule the rest stand open.
void aTownsCrewHiresAbroad()
{
    Society::setWorkByHours(true);
    // (Off unless asked since round 10, the user, 2026-10-27: asked for here, Society::wageRulesOff's `on`.)
    const auto run = [](std::uint64_t on) {
        Society::wageRulesOff(Round11Homes, on);
        AuthoredRoster roster;
        const auto far = [](ResidentSpec r) {
            r.home = r.evening = {"far_homes", 2.5, 2.5};
            return r;
        };
        roster.residents.push_back(far(person("stall", "keeping the stall", {"far_market", 2.5, 2.5})));
        for (int i = 1; i <= 3; ++i)
            roster.residents.push_back(far(person("keeper" + std::to_string(i), "keeps the house", {"far_homes", 2.5, 2.5}, "civilian")));
        for (int i = 1; i <= 140; ++i)
            roster.residents.push_back(person("idle" + std::to_string(i), "idling in the square", {"square", 2.5, 2.5}, "civilian"));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string& cell) { return cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town"); };
        s.setDay(plan);
        s.openAccount("stores:far");
        auto bodies = atWork(s);
        const auto rested = [&] {
            auto st = s.state();
            for (auto& [id, life] : st.residents)
                life.hunger = life.fatigue = 0;
            expect(s.restore(st), "rested");
        };
        for (std::int64_t d = 1; d <= 6; ++d)
        {
            s.tick(1, double(d) + .5 / 24, 0, bodies);
            rested();
        }
        expect(s.shift("treasury", "stores:far", "", 0, 100000, "test: the town's money"), "the town has money");
        s.tick(1, 7 + .5 / 24, 0, bodies);
        for (double t = 6; t < 12; t += 1. / 60)
            s.tick(60, 7 + t / 24, 0, bodies);
        int slots = 0, own = 0, abroad = 0, capitalAbroad = 0;
        for (const auto& j : s.oddJobs())
        {
            if (j.payer == "stores:far" && j.kind == "building work" && j.until >= 7)
            {
                slots += j.slots;
                for (const auto& [who, stage] : j.stage)
                    (who.rfind("keeper", 0) == 0 ? own : abroad) += 1;
            }
            if (j.payer == "treasury" && j.kind == "building work")
                for (const auto& [who, stage] : j.stage)
                    capitalAbroad += who.rfind("keeper", 0) == 0;
        }
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return std::make_tuple(slots, own, abroad, capitalAbroad);
    };
    const auto [slots, own, abroad, capitalAbroad] = run(Society::CrewsAbroad);
    expect(slots > 3 && own == 3 && abroad == slots - 3 && capitalAbroad == 0,
           "the town's own three hold places, the capital's idle the rest of its crew of " + std::to_string(slots) + ": " + std::to_string(own) +
               " and " + std::to_string(abroad) + "; none of the town's on the capital's crews: " + std::to_string(capitalAbroad));
    const auto [slots0, own0, abroad0, capital0] = run(0);
    (void)capital0;
    expect(own0 == 3 && abroad0 == 0 && slots0 > 3, "without the rule (the game's since round 10) the rest stand open: " + std::to_string(own0) +
                                                        " and " + std::to_string(abroad0));
    Society::setWorkByHours(false);
}

// Round 10, towns (the user, 2026-10-27): a town's stores over their target lower its tax. The target is the model's
// RULE["treasury"] k, two months of what its tax brings at the full rate. A far town of ten taxpayers, each earning 1,000p
// a week (800p a week of tax at the full rate, a target of 6,400p; a little more with their own earnings), holds
// 100,000p. Its rate falls a quarter of the way to nothing a week, never below it, and is kept through a save. Drained to
// 1,000p it comes back toward the full rate, a
// quarter of the way a week, never over it. Without the rule its rate is the realm's. With the rebate (the game's since
// the user's word, 2026-10-27), over twice its target, half of the rest goes back to its taxpayers a week ("a town's
// rebate"), by their incomes: by day 35 it holds under four times its target (without it, over ten).
void aTownEasesItsTax()
{
    Society::setWorkByHours(true);
    struct Seen
    {
        std::vector<double> share;                   // After each week's reckoning, days 7 to 84.
        std::int64_t tax14 = 0, rebates = 0, held35 = 0, payer1 = 0;
        double target = 0, saved = 0, savedMean = 0, mean = 0;
        bool savedBase = false;
    };
    const auto run = [](std::uint64_t off, std::uint64_t on) {
        Society::wageRulesOff(off | Round11Homes, on);
        AuthoredRoster roster;
        const auto far = [](ResidentSpec r) {
            r.home = r.evening = {"far_homes", 2.5, 2.5};
            return r;
        };
        roster.residents.push_back(far(person("stall", "keeping the stall", {"far_market", 2.5, 2.5})));
        for (int i = 1; i <= 10; ++i)
        {
            auto payer = far(person("payer" + std::to_string(i), "keeps the house", {"far_homes", 2.5, 2.5}, "civilian"));
            payer.purse = 100000;
            roster.residents.push_back(payer);
        }
        for (int i = 1; i <= 140; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string& cell) { return cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town"); };
        s.setDay(plan);
        s.openAccount("stores:far");
        expect(s.shift("treasury", "stores:far", "", 0, 100000, "test: the town's money"), "the town's purse");
        const auto bodies = atWork(s);
        Seen seen;
        for (std::int64_t d = 1; d <= 84; ++d)
        {
            if (d % 7 == 0)
            {
                // (Each taxpayer's week's earnings, 1,000p, as its reckoning finds them.)
                auto st = s.state();
                for (int i = 1; i <= 10; ++i)
                    st.memory.weekIncome["payer" + std::to_string(i)] = 1000;
                expect(s.restore(st), "the week's earnings");
                s.takeJournal();
            }
            if (d == 36)
                expect(s.shift("stores:far", "treasury", "", 0, s.account("stores:far")->cash - 1000, "test: the town's money spent"),
                       "the town's purse drained");
            s.tick(1, double(d) + .5 / 24, 0, bodies);
            if (d % 7 == 0)
            {
                seen.share.push_back(s.townTaxShare("stores:far"));
                for (const auto& e : s.takeJournal())
                {
                    if (d == 14 && e.kind == "town tax" && e.to == "stores:far")
                        seen.tax14 += e.coins;
                    if (e.kind == "a town's rebate" && e.from == "stores:far")
                        seen.rebates += e.coins, seen.payer1 += e.to == "payer1" ? e.coins : 0;
                }
            }
            if (d == 35)
                seen.held35 = s.account("stores:far")->cash;
            if (d == 28)
            {
                seen.target = s.townTaxTarget("stores:far");
                seen.mean = s.townHeldMean("stores:far");
                const auto m = wire::readSociety(wire::society(s.state())).memory;
                seen.saved = m.townTax.count("stores:far") ? m.townTax.at("stores:far") : 1;
                seen.savedMean = m.heldMean.count("stores:far") ? m.heldMean.at("stores:far") : 0;
                seen.savedBase = m.taxBase.count("stores:far") && std::abs(Society::TaxEaseMonths * m.taxBase.at("stores:far") - seen.target) < 1e-6;
            }
        }
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto now = run(Society::TaxRebate, 0), without = run(Society::TaxEase, 0), rebate = run(0, 0);
    const auto& sh = now.share;
    expect(sh.size() == 12 && std::abs(sh[0] - .75) < 1e-9 && std::abs(sh[1] - .5625) < 1e-9,
           "over its target its rate falls a quarter of the way to nothing a week: " + std::to_string(sh[0]) + ", " + std::to_string(sh[1]));
    expect(without.tax14 >= 800 && std::abs(double(now.tax14) - .75 * double(without.tax14)) <= 10,
           "its week's tax at three-quarters of the full rate: " + std::to_string(now.tax14) + "p, against " + std::to_string(without.tax14) +
               "p without the rule");
    expect(std::abs(now.target - 6400) < 200, "its target about two months of its tax at the full rate: " + std::to_string(now.target));
    expect(std::abs(now.saved - sh[3]) < 1e-9 && std::abs(now.savedMean - now.mean) < 1e-6 && now.savedBase,
           "its rate, its mean holding and its month at the full rate kept through a save: " + std::to_string(now.saved));
    bool smooth = true, bounded = true;
    for (std::size_t k = 0; k < sh.size(); ++k)
    {
        bounded = bounded && sh[k] >= 0 && sh[k] <= 1;
        smooth = smooth && std::abs(sh[k] - (k ? sh[k - 1] : 1.)) <= .25 + 1e-9;
    }
    expect(bounded && smooth, "never below nothing nor over the full rate, and a quarter of the way at most a week");
    // (Drained on day 36, its week's mean holding comes down over the next fortnight.)
    expect(sh[5] < sh[4] && sh[11] > sh[7] + .3 && sh[11] < 1 && sh[10] < sh[11],
           "drained, it comes back toward the full rate: " + std::to_string(sh[5]) + " on day 42, " + std::to_string(sh[7]) + " on day 56, " +
               std::to_string(sh[11]) + " on day 84");
    bool full = true;
    for (const auto v : without.share)
        full = full && v == 1;
    expect(full && without.rebates == 0, "without the rule its rate is the realm's");
    double least = 1;
    for (const auto v : rebate.share)
        least = std::min(least, v);
    expect(least >= 0 && rebate.rebates > 40000 && std::abs(double(rebate.payer1) * 10 - double(rebate.rebates)) < .02 * double(rebate.rebates) && now.rebates == 0,
           "with the rebate its taxpayers are paid back, about a tenth each by their incomes, and its rate is never below nothing: " +
               std::to_string(rebate.rebates) + "p, " + std::to_string(rebate.payer1) + "p to the first; " + std::to_string(least));
    expect(rebate.held35 < 4 * rebate.target && now.held35 > 10 * now.target,
           "by day 35 it holds under four times its target: " + std::to_string(rebate.held35) + "p (without the rebate " +
               std::to_string(now.held35) + "p)");
    Society::setWorkByHours(false);
}

// Round 10, towns (the user, 2026-10-27): a town's own posts are kept to its tax as the capital's are to its budget
// (keepPostsTo): its watch, then its own, to half its week's tax budget, the rest let go and their posts kept empty. A
// far town with a watch of two and six clerks of its own, on 3,000p and 800p a week of tax: by day 14 its clerks are let
// go and its watch kept. Its buyers' funds are kept inside its week's budget, as the capital's. Without the rule all six
// keep their posts. (Off unless asked since the user's follow-up, 2026-10-28: Society::OptInRules.)
void aTownKeepsItsPostsToItsTax()
{
    Society::setWorkByHours(true);
    const auto run = [](std::uint64_t on) {
        Society::wageRulesOff(Round11Homes, on);
        AuthoredRoster roster;
        const auto far = [](ResidentSpec r) {
            r.home = r.evening = {"far_homes", 2.5, 2.5};
            return r;
        };
        roster.residents.push_back(far(person("stall", "keeping the stall", {"far_market", 2.5, 2.5})));
        for (int i = 1; i <= 2; ++i)
            roster.residents.push_back(far(person("watch" + std::to_string(i), "keeps the watch", {"far_gate", 2.5 + i, 2.5}, "guard")));
        for (int i = 1; i <= 6; ++i)
            roster.residents.push_back(far(person("clerk" + std::to_string(i), "keeps the town's books", {"far_hall", 2.5 + i, 2.5}, "civilian")));
        for (int i = 1; i <= 10; ++i)
        {
            auto payer = far(person("payer" + std::to_string(i), "keeps the house", {"far_homes", 2.5, 2.5}, "civilian"));
            payer.purse = 100000;
            roster.residents.push_back(payer);
        }
        for (int i = 1; i <= 140; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string& cell) { return cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town"); };
        s.setDay(plan);
        s.openAccount("stores:far");
        expect(s.shift("treasury", "stores:far", "", 0, 3000, "test: the town's money"), "the town's purse");
        expect(s.payerOf("clerk1", *s.jobOf("clerk1"), 30).account == "stores:far" && s.payerOf("watch1", *s.jobOf("watch1"), 30).account == "stores:far",
               "its clerks and its watch are the town's: " + s.payerOf("clerk1", *s.jobOf("clerk1"), 30).account);
        const auto bodies = atWork(s);
        for (std::int64_t d = 1; d <= 14; ++d)
        {
            if (d % 7 == 0)
            {
                auto st = s.state();
                for (int i = 1; i <= 10; ++i)
                    st.memory.weekIncome["payer" + std::to_string(i)] = 1000;
                expect(s.restore(st), "the week's earnings");
            }
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        }
        int clerks = 0, watch = 0;
        for (const auto& [id, ps] : s.state().careers.positions)
        {
            clerks += ps.holder.rfind("clerk", 0) == 0;
            watch += ps.holder.rfind("watch", 0) == 0;
        }
        const bool funds = s.state().memory.fundsLeft.count("stores:far") > 0;
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return std::make_tuple(clerks, watch, funds);
    };
    const auto [clerks, watch, funds] = run(Society::TownPosts);
    expect(clerks == 0 && watch == 2 && funds, "its clerks let go, its watch kept, its buyers inside its budget: " + std::to_string(clerks) + " clerks, " +
                                                    std::to_string(watch) + " of the watch");
    const auto [clerks0, watch0, funds0] = run(0);
    expect(clerks0 == 6 && watch0 == 2 && !funds0, "without the rule all keep their posts: " + std::to_string(clerks0) + " clerks");
    Society::setWorkByHours(false);
}

// Round 10, towns (the user, 2026-10-27; dropped the same day, off unless asked): a town's caravans' road money comes
// from its week's budget, as its buyers' funds do (the model's treasury_week). On 200,000p, its caravan takes 2,000p on day 3; the day-7 budget allows it that and a third
// over, and a twentieth of what's left besides, and each coin it takes on the road comes off it. Its week counts its
// takings, less the unsold goods its stores buy back: not the road money, which its allowance has paid. The capital's
// caravans (its Houses') are not held to it. Without the rule there is no allowance, and the road money comes off its
// week as in round 9.
void aTownsRoadMoneyFromItsBudget()
{
    Society::setWorkByHours(true);
    struct Seen
    {
        std::int64_t allowed = 0, after = 0, capital = 0;
        double week = 0;
    };
    // (Off unless asked since the user's word, 2026-10-27: asked for here.)
    const auto run = [](std::uint64_t on) {
        Society::wageRulesOff(Round11Homes, on);
        AuthoredRoster roster;
        const auto far = [](ResidentSpec r) {
            r.home = r.evening = {"far_homes", 2.5, 2.5};
            return r;
        };
        roster.residents.push_back(far(person("stall", "keeping the stall", {"far_market", 2.5, 2.5})));
        for (int i = 1; i <= 140; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string& cell) { return cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town"); };
        s.setDay(plan);
        for (const auto* id : {"stores:far", "caravan:far1"})
            s.openAccount(id);
        expect(s.shift("treasury", "stores:far", "", 0, 200000, "test: the town's money"), "the town's purse");
        const auto bodies = atWork(s);
        for (std::int64_t d = 1; d <= 7; ++d)
        {
            if (d == 3)
                expect(s.shift("stores:far", "caravan:far1", "", 0, 2000, "money for the road"), "the caravan sets out");
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        }
        Seen seen;
        seen.allowed = s.roadMoneyFor("stores:far");
        seen.capital = s.roadMoneyFor("treasury");
        expect(s.shift("stores:far", "caravan:far1", "", 0, 1000, "money for the road") &&
                   s.shift("caravan:far1", "stores:far", "", 0, 3000, "trade takings"),
               "out again, and home");
        seen.after = s.roadMoneyFor("stores:far");
        const auto& week = s.state().memory.caravanWeek;
        seen.week = week.count("stores:far") ? week.at("stores:far") : 0;
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto now = run(Society::RoadBudget | Society::TownPosts), before = run(0);
    expect(now.allowed > 2660 && now.allowed < 2660 + 1200 && now.after == now.allowed - 1000 && now.capital == -1,
           "its road money allowed: what it took and a third over, and a twentieth of the rest: " + std::to_string(now.allowed) + "p, " +
               std::to_string(now.after) + "p after 1,000p more; the capital's not held to it: " + std::to_string(now.capital));
    expect(now.week == 3000, "its week counts its takings, not the road money its allowance paid: " + std::to_string(now.week));
    expect(before.allowed == -1 && before.after == -1 && before.week == 0,
           "without the rule no allowance, and its road money off its week, 2,000p and 1,000p against the 3,000p: " + std::to_string(before.week));
    Society::setWorkByHours(false);
}

// Round 12, lane B (HoldingsBite; the user, 2026-10-29), with work by the hours: the holdings tax graduated over a home's
// target (the lever's rate up to twice it, three times the rate up to four times it, six times above; no band over 10%,
// never below the target), and spent back in its town. A household of two given 100,000p pays its day's seventh of that,
// over four times the flat tax; the treasury keeps it as the bite's (not its income), and the next day a broke
// neighbour with the town's relief spent gets a day's food money from it; at the week what's left is gone to its works.
// Without the rule, the flat tax (round 11), counted as the town's income, and no relief past its allowance.
void theHoldingsTaxBites()
{
    const double t = 10000;
    const auto owed = [&](double pot, double rate = .01, bool graduated = true) { return Society::holdingsOwed(pot, t, rate, graduated); };
    expect(owed(t) == 0 && owed(t / 2) == 0, "nothing at its target or under it");
    expect(std::abs(owed(2 * t) - 100) < 1e-6, "the lever's rate on the first target's worth over it: " + std::to_string(owed(2 * t)));
    expect(std::abs(owed(4 * t) - 700) < 1e-6, "three times it up to four targets: " + std::to_string(owed(4 * t)));
    expect(std::abs(owed(6 * t) - 1900) < 1e-6, "six times it above: " + std::to_string(owed(6 * t)));
    expect(std::abs(owed(6 * t, .05) - 4500) < 1e-6, "no band over 10% a week: " + std::to_string(owed(6 * t, .05)));
    expect(owed(1.5 * t, .1) <= .5 * t && owed(1.01 * t, .1) <= .01 * t + 1e-9, "never below its target");
    expect(std::abs(owed(6 * t, .01, false) - 500) < 1e-6, "flat without it, as round 11's");
    Society::setWorkByHours(true);
    std::int64_t paidWith = 0, paidWithout = 0;
    for (const bool rule : {true, false})
    {
        // (RichHomes left out: its help would hire the neighbour. And round 13's SurplusByNeed: with it the capital folds
        // only what its crew's places take, the rest kept in its pot: theBiteGoesOn.)
        Society::wageRulesOff((rule ? 0 : Society::HoldingsBite) | Society::RichHomes | Society::SurplusByNeed);
        AuthoredRoster roster;
        roster.residents = {person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian"),
                            person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian")};
        auto pip = person("pip", "weaving at home", {"hut", 2.5, 2.5}, "civilian");
        pip.home = pip.evening = {"hut", 2.5, 2.5};
        pip.purse = 0;
        roster.residents.push_back(pip);
        roster.economy.treasury = 150000;
        Society s(Roster::None);
        s.configure(roster);
        double day = 10. / 24;
        work(s, 60, day);
        expect(s.shift("treasury", "ash", "", 0, 100000, "test: an inheritance"), "the household is given a fortune");
        {
            auto st = s.state();                    // (The town's and the church's relief for the week spent.)
            st.memory.reliefLeft["treasury"] = 0;
            st.memory.reliefLeft[Society::SharedChurch] = 0;
            st.memory.monthIncome.erase("treasury");
            expect(s.restore(st), "the week's relief spent");
        }
        s.takeJournal();
        day = 1 + 10. / 24;
        work(s, 60, day);
        std::int64_t paid = 0;
        for (const auto& e : s.takeJournal())
            if (e.kind == "holdings tax" && (e.from == "wren" || e.from == "ash") && e.to == "treasury")
                paid += e.coins;
        (rule ? paidWith : paidWithout) = paid;
        const auto& m = s.state().memory;
        const double bite = m.biteLeft.count("treasury") ? m.biteLeft.at("treasury") : 0;
        expect(rule ? paid > 0 && std::abs(bite - double(paid)) < 1 : bite == 0,
               "the bite kept to spend back: " + std::to_string(paid) + "p, " + std::to_string(bite) + "p");
        if (const auto left = s.account("pip")->cash; left > 0)
            expect(s.shift("pip", "treasury", "", 0, left, "test: spent"), "the neighbour broke again");
        day = 2 + 10. / 24;
        work(s, 60, day);
        std::int64_t relief = 0;
        for (const auto& e : s.takeJournal())
            if (e.kind == "relief" && e.from == "treasury" && e.to == "pip")
                relief += e.coins;
        const double after = s.state().memory.biteLeft.count("treasury") ? s.state().memory.biteLeft.at("treasury") : 0;
        // (Its month's income as the next day reckons it: yesterday's earnings.)
        if (rule)
            expect(s.monthIncomeOf("treasury") < 1, "the bite isn't the town's income");
        else
            expect(s.monthIncomeOf("treasury") >= double(paid) * .9, "without the rule, the town's income: " + std::to_string(s.monthIncomeOf("treasury")));
        if (rule)
            expect(relief > 0 && after < bite + double(paid) && after >= 0,
                   "a broke neighbour's relief from it, past the town's: " + std::to_string(relief) + "p");
        else
            expect(relief == 0, "without the rule, none past the town's relief");
        // To the week's reckoning: what's left of it spent (on its works), not kept.
        const auto bodies = atWork(s);
        for (std::int64_t d = 3; d <= 7; ++d)
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        expect(!s.state().memory.biteLeft.count("treasury") || s.state().memory.biteLeft.at("treasury") < 1500,
               "at its week the bite's rest goes to its works");
        expect(wire::readSociety(wire::society(s.state())).memory.biteLeft == s.state().memory.biteLeft, "the bite saved");
        expect(s.conserved(), "money stays conserved");
    }
    expect(paidWith > 4 * paidWithout && paidWithout > 0,
           "graduated, a fortune pays over four times the flat tax: " + std::to_string(paidWith) + "p against " + std::to_string(paidWithout) + "p");
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// Round 12, lane B (CapitalByIncome; the user, 2026-10-29), with work by the hours: the capital's week kept to its
// income. Its week's relief, day labour and buyers' funds come to a quarter of its month's income while it holds under
// two months of it (its floor), and to that and an eighth of what it holds over the floor above it; the day labour's
// allowance what it asked and a tenth, and half that over at most (its funds get the rest, the first cut). Without the
// rule round 11's: its budget by budgetOf, its posts' share half of it, all of that share's rest on the day labour.
void theCapitalKeptToItsIncome()
{
    Society::setWorkByHours(true);
    struct Seen
    {
        double relief = 0, labour = 0, funds = 0, spent = 0;   // (Spent: what the day drew on them after its week.)
        std::int64_t held = 0;
    };
    const auto run = [](std::uint64_t off, double month) {
        Society::wageRulesOff(off | Society::HoldingsTax | Society::RichHomes);
        AuthoredRoster roster;
        for (int i = 1; i <= 10; ++i)
        {
            auto payer = person("payer" + std::to_string(i), "keeps the house", {"homes", 2.5, 2.5}, "civilian");
            payer.purse = 1000;
            roster.residents.push_back(payer);
        }
        roster.economy.treasury = 100000;
        Society s(Roster::None);
        s.configure(roster);
        const auto bodies = atWork(s);
        Seen seen;
        for (std::int64_t d = 1; d <= 35; ++d)    // (Its fifth week: past the realm's first month, its month's income its own.)
        {
            if (d == 35)
            {
                auto st = s.state();
                st.memory.monthIncome["treasury"] = month;
                st.memory.labourWeek["treasury|asked"] = 1000;
                st.memory.fundsWeek.erase("treasury");
                st.memory.reliefWeek.erase("treasury");
                expect(s.restore(st), "the capital's month, and the day labour it was asked for");
                seen.held = s.account("treasury")->cash;
                s.takeJournal();
            }
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        }
        for (const auto& e : s.takeJournal())
            if (e.from == "treasury" && (e.kind == "town funds" || e.kind == "relief" || e.kind == "day labour"))
                seen.spent += double(e.coins);
        const auto& m = s.state().memory;
        const auto of = [](const std::map<std::string, double>& map) { return map.count("treasury") ? map.at("treasury") : -1.; };
        seen.relief = of(m.reliefLeft);
        seen.labour = of(m.labourLeft);
        seen.funds = of(m.fundsLeft);
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto poor = run(0, 60000), rich = run(0, 20000), without = run(Society::CapitalByIncome, 60000);
    const auto week = [](const Seen& s) { return s.relief + s.labour + s.funds + s.spent; };
    // (Its month as set, or a day's 27/28 less if its income was reckoned first; its tax of the week on top of what it held.)
    expect(poor.labour > 1649 && poor.labour < 1651, "the day labour's allowance what it asked, a tenth, and half that over: " + std::to_string(poor.labour));
    expect(week(poor) > 60000. * 27 / 28 / 4 - 2 && week(poor) < 15002,
           "under its floor, a quarter of its month's income: " + std::to_string(week(poor)) + "p of a " + std::to_string(poor.held) + "p purse");
    const double richWeek = 5000 + (double(rich.held) - 40000) / 8;
    expect(std::abs(week(rich) - richWeek) < 250, "over it, an eighth of what it holds over its floor on top: " + std::to_string(week(rich)) + "p, about " +
                                                      std::to_string(richWeek) + "p");
    expect(without.labour > 3000 && week(without) < week(poor) - 1000,
           "without the rule, its budget by its holding (less, under two months), all its posts' share's rest on the day labour: " +
               std::to_string(without.labour) + "p of " + std::to_string(week(without)) + "p");
    Society::setWorkByHours(false);
}

// Round 11, lane 4 (the user, 2026-10-29: "make the tax fluctuate depending on the wealth of the towns"; ShareByWealth):
// the capital's share of a town's tax by the town's holding against its target and the capital's against a month of its
// income. A far town of ten taxpayers (800p of tax a week at the full rate, a target of about 6,400p) holding 100,000p,
// with the rebate left out: in its first month, a tenth of its tax, as before. From its fifth week, with the capital
// short (its month of income set at twice its purse), it sends the capital about an eighth of what it holds over its
// target (over 10,000p), not a tenth of its tax, and its mean holding falls by it; with the capital rich (a month of
// 100p), a tenth of its tax at the most (twice it, halved). Drained to 1,000p it sends under a tenth of its tax. Without
// the rule, a tenth of its tax each week.
void theCapitalsShareByWealth()
{
    Society::setWorkByHours(true);
    struct Seen
    {
        std::map<std::int64_t, std::int64_t> share, tax;   // Each week's, by day.
        double meanAfter = 0, meanBefore = 0;
    };
    const auto run = [](std::uint64_t off, double capitalMonth) {
        // (The payers' 100,000p would pay round 11's holdings tax into the town, and spend on help: left out, so its
        // holding is the test's.)
        Society::wageRulesOff(off | Society::TaxRebate | Society::HoldingsTax | Society::RichHomes);
        AuthoredRoster roster;
        const auto far = [](ResidentSpec r) {
            r.home = r.evening = {"far_homes", 2.5, 2.5};
            return r;
        };
        roster.residents.push_back(far(person("stall", "keeping the stall", {"far_market", 2.5, 2.5})));
        for (int i = 1; i <= 10; ++i)
        {
            auto payer = far(person("payer" + std::to_string(i), "keeps the house", {"far_homes", 2.5, 2.5}, "civilian"));
            payer.purse = 100000;
            roster.residents.push_back(payer);
        }
        for (int i = 1; i <= 40; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string& cell) { return cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town"); };
        s.setDay(plan);
        s.openAccount("stores:far");
        expect(s.shift("treasury", "stores:far", "", 0, 100000, "test: the town's money"), "the town's purse");
        const auto bodies = atWork(s);
        Seen seen;
        for (std::int64_t d = 1; d <= 49; ++d)
        {
            if (d == 42)
                expect(s.shift("stores:far", "treasury", "", 0, s.account("stores:far")->cash - 1000, "test: the town's money spent"),
                       "the town's purse drained");
            if (d % 7 == 0)
            {
                auto st = s.state();
                for (int i = 1; i <= 10; ++i)
                    st.memory.weekIncome["payer" + std::to_string(i)] = 1000;
                if (d >= 35)
                    st.memory.monthIncome["treasury"] = capitalMonth > 0 ? capitalMonth : double(s.account("treasury")->cash) * 2;
                if (d >= 42)                     // (Drained: its mean holding as its purse now.)
                    st.memory.heldMean["stores:far"] = double(s.account("stores:far")->cash);
                expect(s.restore(st), "the week's earnings");
                s.takeJournal();
                if (d == 35)
                    seen.meanBefore = s.townHeldMean("stores:far");
            }
            s.tick(1, double(d) + .5 / 24, 0, bodies);
            if (d % 7 == 0)
            {
                for (const auto& e : s.takeJournal())
                {
                    if (e.kind == "capital's share" && e.from == "stores:far")
                        seen.share[d] += e.coins;
                    if (e.kind == "town tax" && e.to == "stores:far")
                        seen.tax[d] += e.coins;
                }
                if (d == 35)
                    seen.meanAfter = s.townHeldMean("stores:far");
            }
        }
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto shortOf = run(0, 0), rich = run(0, 100), without = run(Society::ShareByWealth, 0);
    const auto at = [](const std::map<std::int64_t, std::int64_t>& m, std::int64_t d) { return m.count(d) ? m.at(d) : std::int64_t(0); };
    expect(at(shortOf.share, 21) == at(shortOf.tax, 21) / Society::CapitalShare && at(shortOf.share, 21) > 0,
           "in its first month a tenth of its tax, as before: " + std::to_string(at(shortOf.share, 21)) + "p");
    expect(at(shortOf.share, 35) > 10000 && at(shortOf.share, 35) < 13000,
           "with the capital short, an eighth of what it holds over its target: " + std::to_string(at(shortOf.share, 35)) + "p");
    expect(shortOf.meanAfter < shortOf.meanBefore - 9000,
           "its mean holding falls by what it sent: " + std::to_string(shortOf.meanBefore) + " to " + std::to_string(shortOf.meanAfter));
    expect(at(rich.share, 35) <= at(rich.tax, 35) / Society::CapitalShare + 1 && at(rich.share, 35) > 0,
           "with the capital rich, a tenth of its tax at the most: " + std::to_string(at(rich.share, 35)) + "p of " +
               std::to_string(at(rich.tax, 35)) + "p");
    expect(at(shortOf.tax, 49) > 0 && at(shortOf.share, 49) < at(shortOf.tax, 49) / Society::CapitalShare,
           "drained under its target, less than a tenth of its tax: " + std::to_string(at(shortOf.share, 49)) + "p of " +
               std::to_string(at(shortOf.tax, 49)) + "p");
    bool tenth = true;
    for (const auto& [d, coins] : without.share)
        tenth = tenth && coins == at(without.tax, d) / Society::CapitalShare;
    expect(tenth && at(without.share, 35) > 0, "without the rule a tenth of its tax each week: " + std::to_string(at(without.share, 35)) + "p");
    Society::setWorkByHours(false);
}

// Round 13, lane 2 (SurplusByNeed): the realm its tests use. The capital, "town", of forty homes, each one grown wolf
// keeping the house with 5,000p, `poor` of them broke the day before the week's reckoning (so the day's households count
// them under PoorDays: 16 of 40 is its need full, 1 of 40 none); and a far town of ten taxpayers holding 100,000p each,
// none of them poor, its stores at first 100,000p. Its tax, the far town's, about 80p a week at the full rate (a week's
// income of 100p each): its target (TaxEase's) small, so the capital's pot has room for what it sends. Day by day to the
// fifth week's reckoning (day 35), where `atWeek` sets what the test asks; the capital's month a hundred pennies (rich:
// the share's own surplus part nothing, a tenth of the tax at the most).
void oneTown(Society& s);   // (Below: one town, the fields none.)
struct NeedRealm
{
    std::unique_ptr<Society> s;
    std::map<std::string, LifeBody> bodies;
};
NeedRealm needRealm(int poor)
{
    AuthoredRoster roster;
    const auto far = [](ResidentSpec r) {
        r.home = r.evening = {"far_homes", 2.5, 2.5};
        return r;
    };
    roster.residents.push_back(far(person("stall", "keeping the stall", {"far_market", 2.5, 2.5})));
    for (int i = 1; i <= 10; ++i)
    {
        auto payer = far(person("payer" + std::to_string(i), "keeps the house", {"far_homes", 2.5, 2.5}, "civilian"));
        payer.purse = 100000;
        roster.residents.push_back(payer);
    }
    roster.residents.push_back(person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}));   // (Where the city's crews gather.)
    for (int i = 1; i <= 40; ++i)
    {
        auto c = person("c" + std::to_string(i), "keeps the house", {"city_home" + std::to_string(i), 2.5, 2.5}, "civilian");
        c.home = c.evening = c.work;
        c.purse = 5000;
        roster.residents.push_back(c);
    }
    roster.economy.treasury = 1000000;
    NeedRealm r{std::make_unique<Society>(Roster::None), {}};
    auto& s = *r.s;
    s.configure(roster);
    s.setCapital("town");
    LifeDay plan;
    plan.communityOf = [](const std::string& cell) { return cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town"); };
    s.setDay(plan);
    s.openAccount("stores:far");
    expect(s.shift("treasury", "stores:far", "", 0, 100000, "test: the town's money"), "the town's purse");
    r.bodies = atWork(s);
    for (std::int64_t d = 1; d <= 34; ++d)
    {
        auto st = s.state();
        if (d % 7 == 0)
            for (int i = 1; i <= 10; ++i)
                st.memory.weekIncome["payer" + std::to_string(i)] = 100;
        if (d == 34)
            for (int i = 1; i <= poor; ++i)
            {
                auto& a = st.accounts.at("c" + std::to_string(i));
                st.accounts.at("treasury").cash += a.cash;
                a.cash = 0;
            }
        expect(s.restore(st), "the week's earnings, the city's broke");
        s.tick(1, double(d) + .5 / 24, 0, r.bodies);
    }
    for (int i = 1; i <= 10; ++i)
        expect(s.account("payer" + std::to_string(i))->cash > 0, "the far town's payers hold their money");
    return r;
}

// Round 13, lane 2 (SurplusByNeed; the user, 2026-10-29), with work by the hours: a trading town's surplus where the need
// is. The city 40% broke (its need full) and the far town with no poor (its pull all of it), holding four times its
// target: at the week's reckoning, after the capital's share, it sends NeedShare (a quarter) of what it still holds over
// two targets into the capital's holdings-tax pot ("a town's surplus sent on"), not the capital's month's income, and its
// rebate pays nothing. With the city at 2.5% poor, without the rule, and with the capital's pot at its room already,
// nothing is sent and the rebate is as before.
void aTownsSurplusGoesWhereTheNeedIs()
{
    Society::setWorkByHours(true);
    Society::noteTowns(true);
    struct Seen
    {
        std::int64_t sent = 0, share = 0, rebate = 0;
        double target = 0, room = 0, pot = 0, month = 0, need = 0;
    };
    const auto run = [](std::uint64_t off, int poor, bool full) {
        // (The holdings tax left out: the payers' 100,000p would pay it, and the pot is the test's. RichHomes: their help.)
        Society::wageRulesOff(off | Society::HoldingsTax | Society::RichHomes);
        auto realm = needRealm(poor);
        auto& s = *realm.s;
        Seen seen;
        seen.need = s.townNeed("town");
        {
            auto st = s.state();
            seen.target = s.townTaxTarget("stores:far");
            for (int i = 1; i <= 10; ++i)
                st.memory.weekIncome["payer" + std::to_string(i)] = 100;
            st.memory.monthIncome["treasury"] = 100;
            auto& stores = st.accounts.at("stores:far");
            st.accounts.at("treasury").cash += stores.cash - std::llround(4 * seen.target);
            stores.cash = std::llround(4 * seen.target);
            st.memory.heldMean["stores:far"] = double(stores.cash);
            st.memory.biteLeft.erase("treasury");
            expect(s.restore(st), "the far town at four times its target, the capital rich");
            if (full)
            {
                st.memory.biteLeft["treasury"] = s.capitalPotRoom();
                expect(s.restore(st), "the capital's pot at its room");
            }
        }
        seen.room = s.capitalPotRoom();
        s.takeJournal();
        s.takeBiteSpent();
        s.tick(1, 35 + .5 / 24, 0, realm.bodies);
        for (const auto& e : s.takeJournal())
            if (e.from == "stores:far")
            {
                seen.sent += e.kind == "a town's surplus sent on" && e.to == "treasury" ? e.coins : 0;
                seen.share += e.kind == "capital's share" ? e.coins : 0;
                seen.rebate += e.kind == "a town's rebate" ? e.coins : 0;
            }
        const auto spent = s.takeBiteSpent();
        const auto& m = s.state().memory;
        seen.pot = (m.biteLeft.count("treasury") ? m.biteLeft.at("treasury") : 0.) + (spent.count("treasury") ? double(spent.at("treasury")) : 0.);
        s.tick(1, 36 + .5 / 24, 0, realm.bodies);
        seen.month = s.monthIncomeOf("treasury");
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto with = run(0, 16, false), without = run(Society::SurplusByNeed, 16, false), few = run(0, 1, false), fewWithout = run(Society::SurplusByNeed, 1, false),
               full = run(0, 16, true);
    const double asked = Society::NeedShare * (4 * with.target - double(with.share) - 2 * with.target);
    expect(with.need == 1 && few.need == 0, "the city 40% poor, its need full; 2.5%, none: " + std::to_string(with.need) + ", " + std::to_string(few.need));
    expect(with.target > 100 && asked > 10 && with.room > asked, "the far town's target, and the capital's pot with room: " + std::to_string(with.target) +
                                                                     "p, " + std::to_string(asked) + "p asked, " + std::to_string(with.room) + "p of room");
    expect(std::abs(double(with.sent) - asked) <= 1, "a quarter of what it holds over two targets after its share, sent on: " + std::to_string(with.sent) + "p of " +
                                                        std::to_string(asked) + "p");
    expect(std::abs(with.pot - double(with.sent)) < 1, "into the capital's pot (kept, or spent on its crew and relief): " + std::to_string(with.pot) + "p");
    expect(with.rebate == 0 && without.rebate > 0, "its rebate nothing while the city needs it; without the rule " + std::to_string(without.rebate) + "p");
    expect(std::abs(with.month - without.month) < 1, "not the capital's month's income: " + std::to_string(with.month) + " against " + std::to_string(without.month));
    expect(with.share == without.share, "the capital's share as before: " + std::to_string(with.share) + "p");
    expect(without.sent == 0 && fewWithout.sent == 0, "without the rule nothing is sent");
    expect(few.sent == 0 && few.rebate == fewWithout.rebate && few.rebate > 0, "the city at 2.5% poor: nothing sent, the rebate as before: " +
                                                                                  std::to_string(few.rebate) + "p");
    expect(full.sent == 0 && full.rebate == without.rebate, "the capital's pot at its room: nothing sent, the rebate as before: " + std::to_string(full.rebate) + "p");
    Society::noteTowns(false);
    Society::setWorkByHours(false);
}

// Round 13, lane 2 (SurplusByNeed), with work by the hours: the holdings tax goes on. The far town (no poor; the city's
// need full) holds 10,000p of holdings tax not yet spent back, its stores at their target (nothing of their own to send).
// At its week the whole of it goes on to the capital's pot ("holdings tax sent on", inside the pot's room: the city's
// going rate set at 150p, two weeks of its crew of five 10,500p), none on its own works; at the capital's week only what
// its crew's places take on top of its budget's crew is folded in (its crew at its most), and the rest stays in the
// pot. Without the rule the far town's bite is folded into its own works and erased, and the capital's pot has nothing.
void theBiteGoesOn()
{
    Society::setWorkByHours(true);
    Society::noteTowns(true);
    struct Seen
    {
        std::int64_t sent = 0, crew = 0, most = 0;
        double farSpent = 0, capitalLeft = 0, capitalSpent = 0;
        bool farLeft = false;
    };
    const auto run = [](std::uint64_t off) {
        Society::wageRulesOff(off | Society::HoldingsTax | Society::RichHomes);
        auto realm = needRealm(16);
        auto& s = *realm.s;
        {
            auto st = s.state();
            const auto target = std::llround(s.townTaxTarget("stores:far"));
            st.memory.monthIncome["treasury"] = 100;
            st.memory.going["town"] = 150;
            auto& stores = st.accounts.at("stores:far");
            st.accounts.at("treasury").cash += stores.cash - (target + 10000);
            stores.cash = target + 10000;
            st.memory.heldMean["stores:far"] = double(target);
            st.memory.biteLeft.clear();
            st.memory.biteLeft["stores:far"] = 10000;
            // (The capital's own 1,000p, its week's budget about 125p: its budget's crew none, so its places are the pot's.)
            auto& treasury = st.accounts.at("treasury");
            st.accounts.at("c40").cash += treasury.cash - 1000;
            treasury.cash = 1000;
            expect(s.restore(st), "the far town's bite, its stores at their target; the capital lean");
        }
        s.takeJournal();
        s.takeBiteSpent();
        s.tick(1, 35 + .5 / 24, 0, realm.bodies);
        Seen seen;
        for (const auto& e : s.takeJournal())
            seen.sent += e.kind == "holdings tax sent on" && e.from == "stores:far" && e.to == "treasury" ? e.coins : 0;
        const auto spent = s.takeBiteSpent();
        const auto& m = s.state().memory;
        seen.farSpent = spent.count("stores:far") ? double(spent.at("stores:far")) : 0.;
        seen.capitalSpent = spent.count("treasury") ? double(spent.at("treasury")) : 0.;
        seen.capitalLeft = m.biteLeft.count("treasury") ? m.biteLeft.at("treasury") : 0.;
        seen.farLeft = m.biteLeft.count("stores:far") > 0;
        for (const auto& j : s.oddJobs())
            if (j.payer == "treasury" && j.kind == "building work" && j.until >= 35)
                seen.crew += j.slots;
        seen.most = s.estateMost();
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto with = run(0), without = run(Society::SurplusByNeed);
    expect(with.sent == 10000 && with.farSpent == 0 && !with.farLeft,
           "the far town's bite sent on whole, none on its own works: " + std::to_string(with.sent) + "p sent, " + std::to_string(with.farSpent) + "p spent");
    // (Its crew of five at 150p a day for a week, on 80% of its works' budget: 6,563p, less what its own budget brought.)
    const double fold = 5 * 150 * 7 * 10. / 8;
    expect(with.capitalLeft > 0 && with.capitalSpent >= fold - 200 && std::abs(with.capitalLeft + with.capitalSpent - 10000) < 1,
           "at the capital's week its crew's room folded in (with its relief past its allowance), the rest kept in its pot: " +
               std::to_string(with.capitalSpent) + "p spent, " + std::to_string(with.capitalLeft) + "p kept");
    expect(with.crew == with.most && with.most == 5, "its crew at its most: " + std::to_string(with.crew) + " of " + std::to_string(with.most));
    expect(without.sent == 0 && without.farSpent == 10000 && !without.farLeft && without.capitalLeft == 0 && without.crew < without.most,
           "without the rule the far town's bite folded into its works and erased, the capital's crew its budget's: " + std::to_string(without.farSpent) +
               "p, " + std::to_string(without.crew) + " hands");
    Society::noteTowns(false);
    Society::setWorkByHours(false);
}

// Round 13, lane 2 (ChildrenKept; the user, 2026-10-29), with work by the hours: no baby takes a hire. A newborn of the
// town ("growing up": no post, so out of work) with the bakery's hire open, a week of its bread sold (a shop's hire, "a
// hand at the shop"), and at the harvest with a farm's field hire open: with the rule it holds no odd job all day; without
// it, it takes the bakery's place by ten and the field's by noon, as a grown wolf without work would. A poor child of
// eight, the bakery's hire open, still takes a place from the afternoon (from 14:00: its work isn't for children).
// (Here, not beside aKeeperGivesUpABusinesssHire in roads_tests: the strip's residents are all written thirty, and these
// bodies' ages are the test's.)
void noBabyTakesAHire()
{
    Society::setWorkByHours(true);
    struct Seen
    {
        bool babeAnyJob = false, babeShop = false, pipShop = false, babeField = false;
    };
    const auto baby = [](const std::string& id, int age) {
        auto r = person(id, "-", {"homes", 2.5, 2.5}, "civilian");
        r.age = age;
        r.purse = 0;
        return r;
    };
    const auto shop = [&](std::uint64_t off) {
        Society::wageRulesOff(off | Round11Homes);
        AuthoredRoster roster;
        roster.residents = {person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}), person("scribe", "the scribe", {"desk", 2.5, 2.5}),
                            baby("babe", 0), baby("pip", 8)};
        roster.economy.treasury = 5000;
        Society s(Roster::None);
        s.configure(roster);
        oneTown(s);
        const auto bakery = s.tillOf("baker");
        {
            auto state = s.state();
            state.memory.pace = 1;
            expect(s.restore(state), "the calibrated hours restore");
        }
        for (std::int64_t d = 1; d <= 7; ++d)
        {
            set(s, "baker", "bread", 90);
            expect(s.shift("treasury", "scribe", "", 0, 90, "test: the buyer's money") && s.sale(bakery, "scribe", "bread", 90, 1, "test: the day's buying"),
                   "the bread is bought");
            auto st = s.state();
            st.accounts.at("scribe").stock.erase("bread");
            expect(s.restore(st), "and eaten");
            s.spoil(d);
        }
        set(s, "baker", "flour", 200);
        set(s, "baker", "firewood", 200);
        set(s, "baker", "bread", 0);
        auto bodies = atWork(s);
        bodies["babe"].age = 0;
        bodies["pip"].age = 8;
        Seen seen;
        const std::int64_t day = 21;
        // Each minute from 9:30 to 15:30; rested, and the children's pennies spent (the poor look for work from eight),
        // at the start and at noon.
        for (int minute = 9 * 60 + 30; minute < 15 * 60 + 30; ++minute)
        {
            if (minute == 9 * 60 + 31 || minute == 12 * 60)
            {
                auto st = s.state();
                for (auto& [who, life] : st.residents)
                    life.hunger = life.fatigue = 0;
                for (const char* child : {"babe", "pip"})
                    if (const auto a = st.accounts.find(child); a != st.accounts.end())
                    {
                        st.accounts.at("treasury").cash += a->second.cash;
                        a->second.cash = 0;
                    }
                expect(s.restore(st), "rested, the children's pennies spent");
            }
            s.tick(60, double(day) + minute / 1440., 0, bodies);
            for (const auto& j : s.oddJobs())
            {
                seen.babeAnyJob |= j.stage.count("babe") > 0;
                seen.babeShop |= j.payer == bakery && j.kind == "a hand at the shop" && j.stage.count("babe") > 0;
                seen.pipShop |= j.payer == bakery && j.stage.count("pip") > 0;
            }
        }
        bool hiring = false;
        for (const auto& j : s.oddJobs())
            hiring |= j.payer == bakery && j.until >= day;
        expect(hiring, "the bakery hires");
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto field = [&](std::uint64_t off) {
        Society::wageRulesOff(off | Round11Homes);
        AuthoredRoster roster;
        auto farmer = person("farmer", "farms the valley fields", {"fields", 4.5, 4.5}, "civilian");
        farmer.home = {"farmhouse", 2.5, 2.5};
        roster.residents = {farmer, baby("babe", 0)};
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        oneTown(s);
        const auto till = s.tillOf("farmer");
        const std::int64_t day = Society::HarvestStart + 7;
        auto state = s.state();
        state.budgetDay = day - 1;
        state.memory.pace = 1;
        state.memory.fields[till] = FieldYear{400, 400, 400, 0, 0, false, 0, -1, true};
        state.accounts.at("treasury").cash -= 100000;
        state.accounts.at(till).cash += 100000;
        for (auto& [id, life] : state.residents)
            life.hunger = life.fatigue = 0;
        expect(s.restore(state), "the farm at the harvest, in funds");
        auto bodies = atWork(s);
        bodies["babe"].age = 0;
        Seen seen;
        bool hired = false;
        s.tick(1, double(day) + .5 / 24, 0, bodies);   // (The day's pass: its hire posted.)
        for (int minute = 9 * 60 + 30; minute < 13 * 60; ++minute)
        {
            s.tick(60, double(day) + minute / 1440., 0, bodies);
            for (const auto& j : s.oddJobs())
            {
                hired |= j.payer == till && j.field;
                seen.babeAnyJob |= j.stage.count("babe") > 0;
                seen.babeField |= j.payer == till && j.field && j.stage.count("babe") > 0;
            }
        }
        expect(hired, "the farm posts its field's hire");
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto kept = shop(0), loose = shop(Society::ChildrenKept), keptField = field(0), looseField = field(Society::ChildrenKept);
    expect(!kept.babeAnyJob && !keptField.babeAnyJob, "with the rule the baby holds no odd job, the bakery's nor the field's");
    expect(kept.pipShop, "a poor child of eight still takes the bakery's place in the afternoon");
    expect(loose.babeShop, "without the rule the baby takes the bakery's hire, as a grown wolf without work");
    expect(looseField.babeField, "and the field's place at the harvest");
    Society::setWorkByHours(false);
}

// Round 13, lane 2 (ChildrenKept), with work by the hours: a child's pennies to the household. A one-year-old holding
// 10,000p in a family home of two grown wolves keeps ChildAllowance (a week of the largest stipend) after the day's
// households, the rest with the grown (the household's purse, so its pot counts it), the home's money the same. Without
// the rule it keeps it all. In a bunkhouse, its own purse.
void aChildsPenniesToTheHousehold()
{
    Society::setWorkByHours(true);
    struct Seen
    {
        std::int64_t before = 0, after = 0, passed = 0, homeBefore = 0, homeAfter = 0, allowance = 0;
    };
    const auto run = [](std::uint64_t off) {
        Society::wageRulesOff(off | Round11Homes);
        AuthoredRoster roster;
        auto pip = person("pip", "-", {"homes", 4.5, 2.5}, "civilian");
        pip.age = 1;
        pip.purse = 0;
        roster.residents = {person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian"),
                            person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian"), pip};
        roster.economy.treasury = 150000;
        Society s(Roster::None);
        s.configure(roster);
        oneTown(s);
        auto bodies = atWork(s);
        bodies["pip"].age = 1;
        s.tick(1, 10. / 24, 0, bodies);
        expect(s.shift("treasury", "pip", "", 0, 10000, "test: a christening gift"), "the child is given 10,000p");
        Seen seen;
        const auto home = [&] { return s.account("wren")->cash + s.account("ash")->cash + s.account("pip")->cash; };
        seen.before = s.account("pip")->cash;
        seen.homeBefore = home();
        seen.allowance = s.childAllowance("town");
        s.takeJournal();
        s.tick(1, 1 + .5 / 24, 0, bodies);
        // (The home's money the same but for what went in and out of it on the day: its shopping, its wants, its tax.)
        const auto in = [](const std::string& id) { return id == "wren" || id == "ash" || id == "pip"; };
        std::int64_t out = 0;
        for (const auto& e : s.takeJournal())
        {
            if (e.kind == "the household purse" && e.from == "pip" && (e.to == "wren" || e.to == "ash"))
                seen.passed += e.coins;
            out += in(e.from) && !in(e.to) ? e.coins : !in(e.from) && in(e.to) ? -e.coins : 0;
        }
        seen.after = s.account("pip")->cash;
        seen.homeAfter = home() + out;
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto kept = run(0), without = run(Society::ChildrenKept);
    expect(kept.allowance > 0 && kept.passed == kept.before - kept.allowance,
           "the child's pennies over its allowance to the household's purse: " + std::to_string(kept.passed) + "p of " + std::to_string(kept.before) + "p, " +
               std::to_string(kept.allowance) + "p kept");
    expect(kept.after <= kept.allowance + kept.allowance / 7 + 1, "it holds its allowance (and a day's stipend at most): " + std::to_string(kept.after) + "p");
    expect(kept.homeAfter == kept.homeBefore, "the home's money the same: " + std::to_string(kept.homeAfter) + " of " + std::to_string(kept.homeBefore));
    expect(without.passed == 0 && without.after >= without.before - without.allowance, "without the rule it keeps its pennies: " + std::to_string(without.after) + "p");
    Society::setWorkByHours(false);
}

// Round 11, lane 4 (the user, 2026-10-29): the week's allowances for day labour. A small town's (TownLabourFirst): its
// tax's budget after its relief and its other posts, set at its week, kept through a save. The capital's for what it
// carries (CarriedPosts): what it asked last week and a tenth, inside its posts' share after its watch, and what its own
// posts leave of the share; asked for far more than that, it gets what's left of the share and no more. Without the
// rules, no allowance.
void theDayLabourAllowances()
{
    Society::setWorkByHours(true);
    struct Seen
    {
        bool town = false, capital = false, saved = false;
        double townLeft = 0, given = 0;
        std::int64_t held = 0;
    };
    const auto run = [](std::uint64_t off, double asked) {
        // (Round 11's posts' share, half the capital's budget, and its payers' holdings tax its income: round 12's
        // CapitalByIncome and HoldingsBite left out, theCapitalKeptToItsIncome.)
        Society::wageRulesOff(off | Society::CapitalByIncome | Society::HoldingsBite);
        AuthoredRoster roster;
        for (int i = 1; i <= 10; ++i)
        {
            auto payer = person("payer" + std::to_string(i), "keeps the house", {"far_homes", 2.5, 2.5}, "civilian");
            payer.home = payer.evening = {"far_homes", 2.5, 2.5};
            payer.purse = 100000;
            roster.residents.push_back(payer);
        }
        for (int i = 1; i <= 20; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 100000;
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string& cell) { return cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town"); };
        s.setDay(plan);
        s.openAccount("stores:far");
        expect(s.shift("treasury", "stores:far", "", 0, 20000, "test: the town's money"), "the town's purse");
        const auto bodies = atWork(s);
        Seen seen;
        for (std::int64_t d = 1; d <= 7; ++d)
        {
            if (d == 7)
            {
                auto st = s.state();
                for (int i = 1; i <= 10; ++i)
                    st.memory.weekIncome["payer" + std::to_string(i)] = 1000;
                st.memory.labourWeek["treasury|asked"] = asked;
                expect(s.restore(st), "the week's earnings, and the day labour the capital was asked for");
                seen.held = s.account("treasury")->cash;
            }
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        }
        const auto& m = s.state().memory;
        seen.town = m.labourLeft.count("stores:far") > 0;
        seen.townLeft = seen.town ? m.labourLeft.at("stores:far") : 0;
        seen.capital = m.labourLeft.count("treasury") > 0;
        seen.given = seen.capital ? m.labourLeft.at("treasury") : 0;
        seen.saved = wire::readSociety(wire::society(s.state())).memory.labourLeft == m.labourLeft;
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto small = run(0, 1000), big = run(0, 1e8), without = run(Society::TownLabourFirst | Society::CarriedPosts, 1000);
    expect(small.town && small.townLeft > 0 && small.townLeft < 20000,
           "a small town's day labour allowance from its tax's budget: " + std::to_string(small.townLeft) + "p");
    expect(small.capital && small.given >= 1100 && std::abs(small.given - big.given) < 1,
           "the capital's for what it carries, what it asked and a tenth, and the rest of its posts' share (no posts of its own "
           "here): " + std::to_string(small.given) + "p");
    expect(big.capital && big.given > 1100 && big.given <= double(big.held) / 4 * .5,   // (Its posts' share: TreasuryStaff of its week.)
           "asked for far more, what's left of its posts' share: " + std::to_string(big.given) + "p");
    expect(small.saved && big.saved, "the allowances kept through a save");
    expect(!without.town && !without.capital, "without the rules, no allowance");
    Society::setWorkByHours(false);
}

// Round 10, towns (the user, 2026-10-27; FoundAtTarget): a town is founded at its target, two months of what its tax
// would bring at the founding's day (a TaxRate of a founding day's wage for each grown wolf of working age who pays it),
// and what the treasuries' share would have given it over that goes to the households, the businesses, the Houses and
// the church in their shares. A realm of 540: a far town of 40 grown wolves and the capital's 500. Founded so, the far
// town holds its target (2 x 28 x 8% x 60p x 40 = 10,752p) and less than without the rule; the realm holds as much
// either way (no Houses here: their share isn't made), and the households and the church hold the more by the excess's
// shares of it.
void aTownFoundedAtItsTarget()
{
    struct Seen
    {
        std::int64_t far = 0, capital = 0, homes = 0, church = 0, supply = 0;
        Society::FoundingMoved moved;
    };
    const auto run = [](std::uint64_t off) {
        Society::wageRulesOff(off | Round11Homes);
        Society::noteTowns(true);
        AuthoredRoster roster;
        for (int i = 1; i <= 40; ++i)
        {
            auto r = person("far" + std::to_string(i), "keeps the house", {"far_homes", 2.5, 2.5}, "civilian");
            r.home = r.evening = {"far_homes", 2.5, 2.5};
            roster.residents.push_back(r);
        }
        for (int i = 1; i <= 500; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string& cell) { return cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town"); };
        s.setDay(plan);
        s.openAccount("stores:far");
        const auto bodies = atWork(s);
        for (std::int64_t d = 1; d <= 2; ++d)   // (The purses after the first day's pass, the world's money the next.)
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        Seen seen;
        seen.far = s.account("stores:far")->cash;
        seen.capital = s.account("treasury")->cash;
        seen.church = s.account(Society::SharedChurch) ? s.account(Society::SharedChurch)->cash : 0;
        for (const auto& [id, life] : s.state().residents)
            if (id.rfind("far", 0) == 0)
                seen.homes += s.account(id)->cash;
        seen.supply = s.moneySupply();
        seen.moved = s.foundingMoved();
        expect(s.conserved(), "money stays conserved");
        Society::noteTowns(false);
        Society::wageRulesOff(0);
        return seen;
    };
    const auto now = run(0), before = run(Society::FoundAtTarget);
    const std::int64_t target = 2 * 28 * 8 * 60 * 40 / 100;
    expect(now.supply == before.supply && now.supply > 3000000,
           "the realm holds its money either way: " + std::to_string(now.supply) + ", " + std::to_string(before.supply));
    expect(std::abs(double(now.far - target)) <= 2 && before.far > now.far + 1000,
           "the far town founded at its target, " + std::to_string(target) + "p: " + std::to_string(now.far) + "p (without the rule " +
               std::to_string(before.far) + "p)");
    expect(now.moved.excess > 0 && now.homes > before.homes && now.church > before.church && now.capital >= before.capital - 2,
           "what was over it to the others, the households and the church among them: " + std::to_string(now.moved.excess) + "p; the far "
               "town's households " + std::to_string(now.homes) + "p against " + std::to_string(before.homes) + "p");
}

// Round 9 (the user, 2026-10-27): only the business's own apprentice, its keeper's, is kept over and above at the
// let-go; one apprenticed to a fellow hand is help as any other (the Heights' cheese stall kept three such for good, and
// its till ran down). A stall wanting one, with its keeper and four hands: the first apprenticed to its keeper, the
// second to the third hand. A week on it has let the second go; three weeks on the other two as well, one a week, and it
// keeps its keeper's apprentice. (With round 8's rule the second is kept too, and two go.)
void theKeepersOwnApprentice()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    roster.residents = {person("stall", "keeping the stall", {"market", 4.5, 4.5})};
    for (int i = 1; i <= 4; ++i)
        roster.residents.push_back(person("hand" + std::to_string(i), "works at the stall", {"market", 4.5 + i, 3.5}, "civilian"));
    roster.economy.treasury = 5000;
    const auto run = [&](std::uint64_t off, int days) {
        Society::wageRulesOff(off | Round11Homes);
        auto s = std::make_unique<Society>(Roster::None);
        s->configure(roster);
        auto state = s->state();
        state.memory.pace = 1;                      // (The calibrated hours: a maker lets go only at them.)
        state.careers.positions["job:stall"].apprentice = "hand1";
        state.careers.positions["job:hand3"].apprentice = "hand2";
        expect(s->restore(state), "the pace and the apprenticeships restore");
        const auto bodies = atWork(*s);
        for (std::int64_t d = 1; d <= days; ++d)
            s->tick(1, double(d) + .5 / 24, 0, bodies);   // (The day's pass.)
        Society::wageRulesOff(0);
        return s;
    };
    const auto gone = [](const Society& s, const std::string& who, int n) {
        std::string out;
        for (int i = 1; i <= n; ++i)
            if (!s.jobOf(who + std::to_string(i)))
                out += std::to_string(i);
        return out;
    };
    auto s = run(0, 1);
    const auto stall = s->tillOf("stall");
    expect(s->handsOf(stall) == 5 && s->handsWanted(stall) == 1 && s->apprenticedTo("hand1") && s->apprenticedTo("hand2"),
           "the stall has five hands and wants one, its first two apprenticed: " + std::to_string(s->handsOf(stall)));
    s = run(0, 7);
    expect(gone(*s, "hand", 4) == "2", "a week on it lets the hand apprenticed to a fellow hand go: " + gone(*s, "hand", 4));
    s = run(0, 21);
    expect(gone(*s, "hand", 4) == "234" && s->jobOf("hand1"), "three weeks on, all but its keeper's apprentice: " + gone(*s, "hand", 4));
    expect(s->conserved(), "money stays conserved");
    s = run(Society::KeepersApprentice, 21);
    expect(gone(*s, "hand", 4) == "34", "with round 8's rule it keeps both apprentices: " + gone(*s, "hand", 4));
    Society::setWorkByHours(false);
}

// Round 9 (the user, 2026-10-27: "The ones travelling from furthest away should be released first in favor of someone
// closer."): a field's place, full, held by a wolf of another town still on its way there, gives way to a grown wolf
// living nearer the field, the farthest first. At the harvest a farm posts a field hire of three, and from ten the city's
// idle take all of it, from homes 1,010 to 1,030 tiles off, before three of the farm's own town, living 50 tiles off,
// look (the rule left out that day). The farthest of the three reaches the field and works, there the next morning too.
// The next morning the farm posts a second hire, of six, and the city's next six idle take it: not the one at work on
// the first, done with its day's share, which holds one place (round 9; before, it took one on each). From ten the town's three
// look again: the two of the city still on their way to the first give way to two of them, the farther first, and the
// third takes the second's farthest's place; none of the three is paid for the day. A fourth of the town, living 3,000
// tiles off, farther than any of the city's, takes nobody's. And no two bump each other: each gave way to a wolf living
// nearer, once; none of them holds a field's place again by the evening, nor could take one back from those that took
// theirs; and nobody gives way after noon. Without the rule the town's find both full.
void aFarTakerGivesWay()
{
    Society::setWorkByHours(true);
    const auto run = [](std::uint64_t off) {
        Society::wageRulesOff(Society::FarGiveWay | Round11Homes);
        AuthoredRoster roster;
        auto farmer = person("farmer", "farms the valley fields", {"fields", 4.5, 4.5}, "civilian");
        farmer.home = {"farmhouse", 2.5, 2.5};
        roster.residents.push_back(farmer);
        for (int i = 1; i <= 3; ++i)
            roster.residents.push_back(person("local" + std::to_string(i), "idling in the square", {"square", 2.5, 2.5}, "civilian"));
        // And one of the farm's town living 3,000 tiles off, farther than any of the city's.
        auto outlier = person("local4", "idling in the square", {"square", 2.5, 2.5}, "civilian");
        outlier.home = outlier.evening = {"far_home", 2.5, 2.5};
        roster.residents.push_back(outlier);
        for (int i = 1; i <= 150; ++i)
        {
            char id[8];
            std::snprintf(id, sizeof id, "c%03d", i);
            auto c = person(id, "idling in the square", {"city_square", 2.5, 2.5}, "civilian");
            c.home = c.evening = {"city_home" + std::to_string(i), 2.5, 2.5};
            roster.residents.push_back(c);
        }
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        LifeDay plan;
        plan.communityOf = [](const std::string& cell) {
            return cell == "fields" ? std::string() : cell.rfind("city_", 0) == 0 ? std::string("city") : std::string("town");
        };
        // The map: the field at its corner, the town 50 tiles off, the city's homes from 1,000 tiles, ten apart.
        plan.onMap = [](const Spot& at, double& x, double& y) {
            x = at.x + (at.cell == "fields"                   ? 0.
                        : at.cell == "far_home"              ? 3000.
                        : at.cell.rfind("city_home", 0) == 0 ? 1000 + 10 * std::stod(at.cell.substr(9))
                        : at.cell.rfind("city_", 0) == 0     ? 1000.
                                                             : 50.);
            y = at.y;
            return true;
        };
        s.setDay(plan);
        const auto till = s.tillOf("farmer");
        const std::int64_t day = Society::HarvestStart + 7;
        auto state = s.state();
        state.budgetDay = day - 1;
        state.memory.pace = 1;
        state.memory.fields[till] = FieldYear{400, 400, 400, 0, 0, false, 0, -1, true};
        state.accounts.at("treasury").cash -= 100000;
        state.accounts.at(till).cash += 100000;
        for (auto& [id, life] : state.residents)
            life.hunger = life.fatigue = 0;
        expect(s.restore(state), "the farm at the harvest, in funds");
        auto bodies = atWork(s);
        s.tick(1, double(day) + .5 / 24, 0, bodies);   // (The day's pass: its hire posted.)
        const auto hire = [&]() -> const Society::OddJob* {
            for (const auto& j : s.oddJobs())
                if (j.payer == till && j.field && j.until >= day)
                    return &j;
            return nullptr;
        };
        expect(hire() && hire()->slots == 3 && hire()->community == "town", "the farm posts a field hire of three: " +
                                                                                 std::to_string(hire() ? hire()->slots : 0));
        std::vector<EconomyEntry> journal;
        const auto rested = [&] {
            auto st = s.state();
            for (auto& [id, life] : st.residents)
                life.hunger = life.fatigue = 0;
            expect(s.restore(st), "rested");
        };
        // Each minute, the day's hours. (Rested at each morning: nobody here eats or sleeps where it stands.)
        const auto until = [&](std::int64_t d, double from, double end) {
            for (double t = from; t < end; t += 1. / 60)
            {
                s.tick(60, double(d) + t / 24, 0, bodies);
                for (auto& e : s.takeJournal())
                    journal.push_back(std::move(e));
            }
        };
        until(day, 9.5, 11);
        std::string first;
        if (const auto* j = hire())
            for (const auto& [who, stage] : j->stage)
                first += who + " ";
        expect(first == "c001 c002 c003 ", "by eleven the city's first three hold the field's places: " + first);
        // The farthest of them at the field from eleven, at work, and the next morning.
        bodies["c003"] = {"fields", 4.5, 4.5};
        until(day, 11, 18);
        s.tick(1, double(day + 1) + .5 / 24, 0, bodies);
        rested();
        journal.clear();
        Society::wageRulesOff(off | Round11Homes);
        until(day + 1, 6, 12);
        std::string held;
        if (const auto* j = hire())
            for (const auto& [who, stage] : j->stage)
                held += who + " ";
        // Who gave way, to whom: each living farther from the field than the one that took its place, once.
        std::string gave;
        bool far = true, nearer = true;
        std::set<std::string> givers;
        const Spot field{"fields", 4.5, 4.5};
        for (const auto& e : journal)
            if (e.kind == "gave way to a nearer hand")
            {
                gave += e.from + ">" + e.to + " ";
                far = far && e.item == till && e.quantity == int(std::lround(s.livesFrom(e.from, field)));
                nearer = nearer && s.livesFrom(e.from, field) > s.livesFrom(e.to, field) && givers.insert(e.from).second;
            }
        bool paid = false;
        for (const auto& e : journal)
            paid = paid || (e.kind == "an odd job: a hand" && givers.count(e.to));
        // The farm's own wolf living farther than the city's takes nobody's place.
        for (const auto& j : s.oddJobs())
            nearer = nearer && !j.stage.count("local4");
        // The rest of the day: nobody more gives way, and none of those that gave way holds a field's place again, nor
        // could take one back from those that took theirs.
        journal.clear();
        until(day + 1, 12, 18);
        bool settled = true;
        for (const auto& e : journal)
            settled = settled && e.kind != "gave way to a nearer hand";
        for (const auto& j : s.oddJobs())
            if (j.field && j.until >= day + 1)
                for (const auto& who : givers)
                    settled = settled && !j.stage.count(who) && s.givesWayTo(j, who).empty();
        // One place each (round 9): the wolf at work on the first hire holds none on the second.
        int places = 0;
        for (const auto& j : s.oddJobs())
            places += j.field && j.stage.count("c003");
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return std::make_tuple(held, gave, far && nearer, paid, settled, places);
    };
    const auto [held, gave, far, paid, settled, places] = run(0);
    expect(places == 1, "the wolf at work on the first hire holds no place on the second: " + std::to_string(places));
    expect(held == "c003 local1 local2 ",
           "by the next noon two of the town hold the places of the two still on their way, not the one at work: " + held);
    expect(gave == "c002>local1 c001>local2 c009>local3 " && far,
           "the farthest first, each once, each to a nearer wolf, noted with how far it lives from the field: " + gave);
    expect(!paid, "none that gave way is paid for the day");
    expect(settled, "no two bump each other: none gives way after noon, nor takes a place back");
    const auto [without, none, noted, unpaid, still, one] = run(Society::FarGiveWay);
    (void)noted, (void)unpaid, (void)still, (void)one;
    expect(without == "c001 c002 c003 " && none.empty(), "without the rule the town's three find it full: " + without);
    // Before round 9's one place, the wolf at work on the first, done with its day's share, took a place on the second.
    const auto twice = std::get<5>(run(Society::FarGiveWay | Society::OnePlace));
    expect(twice == 2, "without one place each it holds both: " + std::to_string(twice));
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
    // (Its farmer, drawing its farm's keeper's wage, takes the bakery's hire at ten: round 12's WagelessFirst left out.)
    Society::wageRulesOff(Society::WagelessFirst);
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
    Society::wageRulesOff(0);
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
// (`written`: the labourers' age as their specs were written, 0 for person()'s.)
Society realm(int wolves, bool shops = true, int written = 0)
{
    AuthoredRoster roster;
    if (shops)
        roster.residents = {person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}), person("stall", "keeping the stall", {"market", 4.5, 4.5})};
    for (int i = int(roster.residents.size()); i < wolves; ++i)
    {
        auto r = person("w" + std::to_string(i), "labouring in the yard", {"yard", 2.5, 2.5}, "civilian");
        r.home = {"home" + std::to_string(i), 2.5, 2.5};
        if (written > 0)
            r.age = written;
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
    // different rates: the market's stall in the north at a quarter over the realm's, the rest in the south at a fifth
    // under it. Each book moves by its own town's: a till's, and the town's own labour's (round 6, the tidy's review:
    // only tills' were tried). And again with the labourers written as 15, grown up since (their bodies 30): their posts
    // keyed by their bodies' age, as the labour market keys them (they went by the realm's).
    for (const int written : {0, 15})
    {
        LifeDay towns;
        towns.communityOf = [](const std::string& cell) { return cell == "market" ? std::string("north") : std::string("south"); };
        auto older = realm(int(Society::WorldMoneyResidents), true, written);
        older.setDay(towns);
        auto bodies = atWork(older);
        double day = .5;
        for (int d = 0; d < 8; ++d, day += 1)
            for (int i = 0; i < 3; ++i)
                older.tick(1, day + i / 86400., 0, bodies);
        auto state = older.state();
        const auto north = "till:" + older.jobOf("stall")->id, south = "till:" + older.jobOf("baker")->id;
        const std::string labour = "treasury|labour";
        expect(state.memory.employers.count(north) && state.memory.employers.count(south) && state.memory.employers.count(labour),
               "the stall, the bakery and the town's labour have their books");
        state.memory.purses = 0;
        state.memory.foundedDay = 0;
        state.memory.going = {{"", catalogue}, {"south", catalogue * .8}, {"north", catalogue * 1.25}};
        state.memory.employers.at(north).wage = catalogue * 1.25;
        state.memory.employers.at(south).wage = catalogue * .8;
        state.memory.employers.at(labour).wage = catalogue * .8;
        auto loaded = realm(int(Society::WorldMoneyResidents), true, written);
        loaded.setDay(towns);
        expect(loaded.restore(wire::readSociety(wire::society(state))), "an older save of two towns, loaded afresh");
        loaded.tick(1, day - .8, 0, bodies);        // (Its purses founded.)
        const auto& books = loaded.state().memory.employers;
        expect(loaded.state().memory.purses >= 1 && std::abs(books.at(north).wage - founding) < 1e-6 && std::abs(books.at(south).wage - founding) < 1e-6 &&
                   std::abs(books.at(labour).wage - founding) < 1e-6,
               std::string(written ? "(the labourers written at 15) " : "") + "each book by its own town's rate: the north's " +
                   std::to_string(books.at(north).wage) + "p, the south's " + std::to_string(books.at(south).wage) + "p, its labour's " +
                   std::to_string(books.at(labour).wage) + "p, at " + std::to_string(founding) + "p");
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
    // A keeper short of a week's working money in its shop's till puts in what it holds over a week of its household's
    // food (the baker and itself: doc 64, Phase 13, round 8), at its own town's day (the price review: untried): in the
    // north at a fifth over the realm's, by the hours.
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
            reserve = owners.pennies(7 * Society::FoodADay * double(owners.householdOf("stall")), "north");
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
        expect(owners.householdOf("stall") == 2 && invested == 7,
               "it puts in what it holds over its household's week of food at its town's day (" + std::to_string(reserve) + "p): " +
                   std::to_string(invested) + "p");
    }
}

// A shop's goods to sell on for a fortnight (doc 64, Phase 13, round 8), with work by the hours: a stall that sells flour
// it buys in keeps ResaleDays of it in its working money, beside a month of its keeper's wage; the baker keeps a month of
// the flour its bread uses, as before. At the seconds' pace the stall keeps a month of its flour, as it did.
void aShopsGoodsToSellOnForAFortnight()
{
    for (const bool hours : {true, false})
    {
        Society::setWorkByHours(hours);
        auto s = town();
        auto state = s.state();
        state.memory.pace = 1;
        expect(s.restore(state), "the calibrated hours restore");
        expect(s.shift("treasury", "scribe", "", 0, 14 * 20, "test: the buyer's money"), "the buyer has money");
        for (std::int64_t d = 1; d <= 14; ++d)
        {
            for (const auto& [keeper, good] : {std::pair{"stall", "flour"}, std::pair{"baker", "bread"}})
            {
                set(s, keeper, good, 10);
                expect(s.sale(s.tillOf(keeper), "scribe", good, 10, 1, "test: the day's buying"), std::string("the buyer buys ") + good);
            }
            auto st = s.state();
            st.accounts.at("scribe").stock.clear();
            expect(s.restore(st), "used up");
            s.spoil(d);
        }
        // What the stall keeps: a month of its keeper's wage, `days` of the flour it sells, its orders and its fields.
        const auto stallTill = s.tillOf("stall");
        bool resold = false;
        const double flour = s.materialsCost("stall", "flour", &resold);
        expect(resold, "the stall's flour is bought to sell on");
        const auto kept = [&](double days) {
            return std::int64_t(std::ceil(MonthDays * s.employerWage(stallTill) + days * s.sellRates(stallTill)->at("flour") * flour +
                                          double(s.ordersCostOf(stallTill)) + s.fieldWages(stallTill)));
        };
        const auto stall = s.workingMoney(s.jobOf("stall")->id), fortnight = kept(Society::ResaleDays), month = kept(MonthDays);
        expect(fortnight < month && std::llabs(stall - (hours ? fortnight : month)) <= 1,
               std::string(hours ? "by the hours" : "at the seconds' pace") + ", the stall keeps " + (hours ? "a fortnight" : "a month") +
                   " of its flour: " + std::to_string(stall) + "p (a fortnight " + std::to_string(fortnight) + "p, a month " +
                   std::to_string(month) + "p)");
        // The baker's: a month of its keeper and of its bread's flour, by the hours too.
        const auto bakery = s.tillOf("baker");
        resold = false;
        const double materials = s.materialsCost("baker", "bread", &resold);
        const auto baker = s.workingMoney(s.jobOf("baker")->id);
        const auto bread = std::int64_t(std::ceil(MonthDays * (s.employerWage(bakery) + s.sellRates(bakery)->at("bread") * materials) +
                                                  double(s.ordersCostOf(bakery)) + s.fieldWages(bakery)));
        expect(!resold && materials > 0 && std::llabs(baker - bread) <= 1,
               "and the baker a month of its bread's flour: " + std::to_string(baker) + "p, " + std::to_string(bread) + "p");
    }
    Society::setWorkByHours(false);
}

// A lone owner keeps a week of its household's food (doc 64, Phase 13, round 8; the model's owner_tops_up, 7 x
// need_cost), with work by the hours: its till run dry, on its own day of the week a keeper living with its wife and
// child puts in what it holds over three wolves' week of food, and a smith in a bunkhouse what it holds over its own.
// At the seconds' pace each keeps KeeperReserve, as before.
void aLoneOwnerKeepsAWeeksFood()
{
    for (const bool hours : {true, false})
    {
        Society::setWorkByHours(hours);
        AuthoredRoster roster;
        auto keeper = person("stall", "keeping the stall", {"market", 4.5, 4.5});
        auto wife = person("wife", "the scribe", {"desk", 2.5, 2.5}, "civilian");
        auto child = person("child", "at lessons", {"school", 2.5, 2.5}, "civilian");
        auto smith = person("smith", "the smith at the forge", {"forge", 2.5, 2.5});
        auto bunk = person("bunk", "the scribe", {"desk", 2.5, 2.5}, "civilian");
        keeper.home = wife.home = child.home = {"stall_flat", 2.5, 2.5};
        smith.home = bunk.home = {"forge_bunkhouse", 2.5, 2.5};
        child.age = 10;
        roster.residents = {keeper, wife, child, smith, bunk};
        roster.economy.treasury = 5000;
        Society s(Roster::None);
        s.configure(roster);
        expect(s.householdOf("stall") == 3 && s.householdOf("smith") == 1,
               "the keeper's household three, the smith's in the bunkhouse one: " + std::to_string(s.householdOf("stall")) + ", " +
                   std::to_string(s.householdOf("smith")));
        std::map<std::string, std::int64_t> floor, invested;
        for (const std::string who : {"stall", "smith"})
        {
            const auto till = s.tillOf(who);
            expect(till.rfind("till:", 0) == 0, who + " keeps its own till");
            s.shift(till, "treasury", "", 0, s.account(till)->cash, "test: the till run dry");
            floor[who] = hours ? s.pennies(7 * Society::FoodADay * double(s.householdOf(who)), "")
                               : s.pennies(Society::KeeperReserve, "");
            const auto cash = s.account(who)->cash, want = floor[who] + 7;
            if (cash > want)
                s.shift(who, "treasury", "", 0, cash - want, "test: the keeper's purse");
            else
                s.shift("treasury", who, "", 0, want - cash, "test: the keeper's purse");
        }
        s.takeJournal();
        // A week of days, each its owner's day of the week once.
        for (std::int64_t day = 7; day < 14; ++day)
            s.tendTills(day);
        for (const auto& e : s.takeJournal())
            if (e.kind == "an owner's investment" && e.to == s.tillOf(e.from))
                invested[e.from] += e.coins;
        for (const std::string who : {"stall", "smith"})
            expect(invested[who] == 7, std::string(hours ? "by the hours, " : "at the seconds' pace, ") + who + " puts in what it holds over " +
                                           std::to_string(floor[who]) + "p: " + std::to_string(invested[who]) + "p");
        expect(!hours || floor["stall"] > floor["smith"], "a household of three keeps more than one alone");
        expect(s.conserved(), "money stays conserved");
    }
    Society::setWorkByHours(false);
    // A home of more than eight grown houses many who aren't a family, as the household rule counts them (the review):
    // each grown wolf its own household. Eight grown and a child are still one household of nine.
    for (const int many : {8, 9})
    {
        AuthoredRoster roster;
        for (int i = 0; i <= many; ++i)
        {
            auto r = person("lodger" + std::to_string(i), "the scribe", {"desk", 2.5, 2.5}, "civilian");
            r.home = {"great_hall", 2.5, 2.5};
            if (i == many)
                r.age = 10;
            roster.residents.push_back(r);
        }
        roster.economy.treasury = 5000;
        Society s(Roster::None);
        s.configure(roster);
        const auto household = s.householdOf("lodger0");
        expect(household == (many > 8 ? 1u : std::size_t(many + 1)),
               std::to_string(many) + " grown and a child at home: a household of " + std::to_string(household));
    }
}

// A lone owner stops topping up a business that can't pay its wages (doc 64, Phase 13, round 9; the user, 2026-10-27),
// with work by the hours: a stall's till run dry each week is topped up at its owner's first three looks; at the fourth
// in a row short of a week's wages its owner stops, and puts in nothing while it stays short. On a day its till can't
// pay its keeper's wage, the keeper is free for hire work. Saved, the society reads back the same, and goes on so. A
// look that finds it holding a week's wages of its own, and its owner tops it up again. A House's stall run dry each
// week is topped up by its House every week. Without the rule (round 8's), and at the seconds' pace, the stall's owner
// tops it up every week too.
void aLoneOwnerStopsToppingUp()
{
    struct Run
    {
        std::vector<std::int64_t> invested, house;   // What its owner, and the House its stall, put in at each look.
        bool unpaid = false, paidDay = true, saved = false, resumed = false;
    };
    const auto run = [](bool hours, std::uint64_t off) {
        Society::setWorkByHours(hours);
        Society::wageRulesOff(off | Round11Homes);
        AuthoredRoster roster;
        roster.residents = {person("stall", "keeping the stall", {"market", 4.5, 4.5}),
                            person("scribe", "the scribe", {"desk", 2.5, 2.5}, "civilian"),
                            person("lord", "ruling House Fell", {"hall", 2.5, 2.5}, "civilian"),
                            person("housestall", "keeping the stall", {"market2", 4.5, 4.5})};
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        const auto till = s.tillOf("stall"), houses = s.tillOf("housestall");
        const auto& job = *s.jobOf("stall");
        expect(till == "till:" + job.id, "the stall keeps its own till");
        auto owned = s.state();
        owned.houses.owner[s.jobOf("housestall")->id] = "house:house_fell";
        owned.accounts["house:house_fell"].cash = 100000;   // (The House's purse.)
        owned.accounts.at("treasury").cash -= 100000;
        expect(s.restore(owned) && s.ownerOfBusiness(s.jobOf("housestall")->id) == "house:house_fell", "the other stall is House Fell's");
        expect(s.shift("treasury", "stall", "", 0, 100000, "test: the owner's purse"), "the owner has money");
        Run r;
        const auto dry = [&] {
            for (const auto& t : {till, houses})
                s.shift(t, "treasury", "", 0, s.account(t)->cash, "test: the till run dry");
        };
        const auto week = [&](std::int64_t from) {
            s.takeJournal();
            for (std::int64_t day = from; day < from + 7; ++day)
                s.tendTills(day);
            std::int64_t in = 0, house = 0;
            for (const auto& e : s.takeJournal())
                if (e.kind == "an owner's investment")
                    (e.to == till ? in : house) += e.to == till || e.to == houses ? e.coins : 0;
            r.invested.push_back(in);
            r.house.push_back(house);
        };
        for (std::int64_t w = 1; w <= 5; ++w)
        {
            dry();
            week(7 * w);
        }
        // Stopped: a day its till is dry, its keeper's wage unpaid, the keeper is free for hire work; a day it pays it,
        // not.
        dry();
        s.tendTills(42);
        auto st = s.state();
        st.budgetDay = 42;
        expect(s.restore(st), "the day restores");
        r.unpaid = s.ownerUnpaid("stall", job);
        // Saved and read back: the same, and its owner still stopped.
        const auto before = s.state().memory.shortTills;
        expect(s.restore(wire::readSociety(wire::society(s.state()))), "the save restores");
        const auto after = s.state().memory.shortTills;
        r.saved = before.size() == after.size() &&
                  std::equal(before.begin(), before.end(), after.begin(), [](const auto& a, const auto& b) {
                      return a.first == b.first && a.second.weeks == b.second.weeks && a.second.stopped == b.second.stopped &&
                             a.second.unpaid == b.second.unpaid;
                  });
        expect(!hours || off || s.ownerUnpaid("stall", job), "read back, its keeper is still free for hire work");
        dry();
        week(42);
        s.shift("treasury", till, "", 0, std::int64_t(std::ceil(s.employerWage(till))), "test: a day's wage");
        s.tendTills(49);
        st = s.state();
        st.budgetDay = 49;
        expect(s.restore(st), "the day restores");
        r.paidDay = s.ownerUnpaid("stall", job);
        // A week's wages of its own at its look (a fortnight's put in: its keeper is paid its wage each day before it):
        // its owner tops it up again.
        dry();
        s.shift("treasury", till, "", 0, std::int64_t(std::ceil(15 * s.staffing(job.id))), "test: a good week");
        week(56);
        r.resumed = !s.state().memory.shortTills.count(till);
        dry();
        week(63);
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        Society::setWorkByHours(false);
        return r;
    };
    const auto show = [](const std::vector<std::int64_t>& v) {
        std::string text;
        for (const auto x : v)
            text += (text.empty() ? "" : ", ") + std::to_string(x);
        return text;
    };
    const auto r = run(true, 0);
    const auto& in = r.invested;
    expect(in[0] > 0 && in[1] > 0 && in[2] > 0, "its owner tops it up at its first three looks: " + show(in));
    expect(in[3] == 0 && in[4] == 0 && in[5] == 0, "at the fourth short of a week's wages it stops, and stays stopped: " + show(in));
    expect(r.unpaid, "a day its till can't pay its keeper's wage, the keeper is free for hire work");
    expect(!r.paidDay, "a day it pays it, not");
    expect(r.saved, "what it remembers reads back from a save as it was");
    expect(r.resumed && in[7] > 0, "a week's wages of its own, and its owner tops it up again: " + show(in));
    expect(std::all_of(r.house.begin(), r.house.begin() + 6, [](std::int64_t x) { return x > 0; }),
           "the House tops its stall up every week: " + show(r.house));
    for (const auto& [hours, off] : {std::pair{true, std::uint64_t(Society::OwnersStop)}, std::pair{false, std::uint64_t(0)}})
    {
        const auto o = run(hours, off);
        expect(std::all_of(o.invested.begin(), o.invested.begin() + 6, [](std::int64_t x) { return x > 0; }) && !o.unpaid,
               std::string(hours ? "without the rule" : "at the seconds' pace") + ", its owner tops it up every week: " + show(o.invested));
    }
}

// Round 11, lane 1 (the user, 2026-10-29: "if there is no work, for them to just go idle and the owners to return to them
// when there's demand"), with work by the hours: a lake's fisher whose owner stopped topping up its dry till on day 1
// goes idle at its first look IdleWeeks after (IdleTills): no keeper's wage from then, its keeper free for hire work,
// and it wants its keeper only. Saved, it reads back idle. A fortnight of 60 fish sold a day (its till emptied each
// evening, so never a week's wages of its own): its owner returns to it (OwnerReturns), and it trades on. Without
// OwnerReturns it stays idle; without IdleTills it never goes idle, its keeper paid what the till holds. And a business
// holding over its working money takes on a hand more for each month of a hand's wage over it, up to doubling
// (MoreHands); without the rule, only its sales' hands.
void aStoppedBusinessIdlesAndReturns()
{
    struct Run
    {
        bool idled = false, wagePaid = false, freed = false, keeperOnly = false, saved = false, returned = false, stillIdle = false;
        int wanted = 0, wantedRich = 0, wantedBare = 0, wantedBareRich = 0;
        std::string why;   // (Its sales against its hands' wages, after the fortnight.)
    };
    const auto run = [](std::uint64_t off) {
        Society::setWorkByHours(true);
        Society::wageRulesOff(off);
        AuthoredRoster roster;
        roster.residents = {person("fisher", "fishes the lake", {"lake", 4.5, 4.5}, "civilian"), person("scribe", "the scribe", {"desk", 2.5, 2.5})};
        roster.economy.treasury = 100000;
        Society s(Roster::None);
        s.configure(roster);
        auto state = s.state();
        state.memory.pace = 1;
        const auto till = s.tillOf("fisher");
        const auto& job = *s.jobOf("fisher");
        state.memory.shortTills[till] = {Society::StopWeeks, 1, -1};
        expect(s.restore(state), "the stopped fisher restores");
        Run r;
        const auto dry = [&] {
            for (const auto& who : {till, std::string("fisher")})
                if (const auto cash = s.account(who)->cash; cash > 0)
                    s.shift(who, "treasury", "", 0, cash, "test: the money taken");
        };
        // Five weeks of its looks, its till dry and a day's wage in it each morning.
        std::int64_t d = 1;
        for (; d <= 36; ++d)
        {
            dry();
            s.shift("treasury", till, "", 0, std::int64_t(std::ceil(s.employerWage(till))), "test: a day's wage in its till");
            s.takeJournal();
            const bool idleBefore = s.tillIdle(till);   // (The day it goes idle it paid its wage first.)
            s.tendTills(d);
            for (const auto& e : s.takeJournal())
                if (e.kind == "the keeper's wage" && idleBefore)
                    r.wagePaid = true;
        }
        r.idled = s.tillIdle(till);
        auto st = s.state();
        st.budgetDay = double(d - 1);
        expect(s.restore(st), "the day restores");
        r.freed = s.ownerUnpaid("fisher", job);
        r.keeperOnly = s.handsWanted(till) == 1;
        expect(s.restore(wire::readSociety(wire::society(s.state()))), "the save restores");
        r.saved = s.tillIdle(till) == r.idled;
        // A fortnight of buyers: 60 fish a day, the till and the fisher emptied each evening.
        const auto bodies = atWork(s);
        for (int i = 0; i < 14; ++i, ++d)
        {
            set(s, "fisher", "fish", 60);
            expect(s.shift("treasury", "scribe", "", 0, 60, "test: the buyer's money"), "the buyer has money");
            expect(s.sale(till, "scribe", "fish", 60, 1, "test: the day's fish"), "the buyer buys");
            auto eaten = s.state();
            eaten.accounts.at("scribe").stock.erase("fish");
            expect(s.restore(eaten), "eaten");
            dry();
            s.takeJournal();
            s.tick(1, double(d) + .5, 0, bodies);
            for (const auto& e : s.takeJournal())
                r.returned = r.returned || e.kind == "an owner returned to its business";
        }
        r.stillIdle = s.tillIdle(till);
        r.why = std::to_string(s.sellRates(till) && s.sellRates(till)->count("fish") ? s.sellRates(till)->at("fish") : -1.) + " fish a day at " +
                std::to_string(s.shopPrice("fisher", "fish")) + "p, " + std::to_string(s.hoursBySales(till, false)) + " hours at " +
                std::to_string(s.employerWage(till)) + "p";
        // Its hands by its sales, and with three months of a hand's wage over its working money.
        r.wanted = s.handsWanted(till), r.wantedBare = s.handsWanted(till, false);
        if (!r.stillIdle)
        {
            dry();
            const auto over = s.workingMoney(job.id) + s.owed(till) + std::int64_t(3.5 * MonthDays * s.employerWage(till));
            expect(s.shift("treasury", till, "", 0, over, "test: a good quarter"), "the till fills");
            r.wantedRich = s.handsWanted(till), r.wantedBareRich = s.handsWanted(till, false);
        }
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        Society::setWorkByHours(false);
        return r;
    };
    const auto r = run(0);
    expect(r.idled && !r.wagePaid, "stopped four weeks and still short, it goes idle and pays no keeper's wage");
    expect(r.freed && r.keeperOnly, "idle, its keeper is free for hire work, and it wants its keeper only");
    expect(r.saved, "it reads back idle from a save");
    expect(r.returned && !r.stillIdle, "buyers back for its fish, its owner returns to it: " + r.why);
    expect(r.wantedRich == r.wanted + std::min(r.wanted, 3) && r.wantedBareRich == r.wantedBare + std::min(r.wantedBare, 3),
           "holding three months of a hand's wage over its working money, it wants up to three more: " + std::to_string(r.wanted) +
               " -> " + std::to_string(r.wantedRich) + ", " + std::to_string(r.wantedBare) + " -> " + std::to_string(r.wantedBareRich));
    const auto stays = run(Society::OwnerReturns);
    expect(stays.idled && !stays.returned && stays.stillIdle, "without OwnerReturns it stays idle while its till is dry");
    const auto never = run(Society::IdleTills | Society::OwnerReturns);
    expect(!never.idled && !never.returned && !never.stillIdle && never.wanted >= 1, "without IdleTills it never goes idle");
    const auto plain = run(Society::MoreHands);
    expect(plain.returned && plain.wantedRich == plain.wanted && plain.wantedBareRich == plain.wantedBare,
           "without MoreHands, its sales' hands however much it holds: " + std::to_string(plain.wanted) + " -> " +
               std::to_string(plain.wantedRich));
}

// Round 12, lane A (the user, 2026-10-29: the wage spiral), with work by the hours: a scribe's office and three lone
// scribes at a 10p labourer's day (the going rate, their middle). The office's clerk's post stands open, its book four
// days open: with its trade lately (memory.tradeNet) paying its hands at the higher wage it bids its wage up at its
// weekly review (WageBySales); its trade nothing, it keeps its wage and its post open; without the rule it bids up
// whatever it sells. Its two clerks at their desks (none open) at 12.5p, it drifts a quarter of the way back to the
// rate in its week (11.875p); without the rule it keeps 12.5p (calm a week). Holding 20,000p in its till, its only post
// open is MoreHands' extra: it bids nothing with the rule, whatever it sells; without, it bids.
void aWageBidBySales()
{
    struct Run
    {
        double wage = 0, rate = 0;
        int open = 0;
    };
    const auto run = [](std::uint64_t off, int clerks, bool clerkGone, double wage, double net, std::int64_t cash) {
        Society::setWorkByHours(true);
        Society::wageRulesOff(off);
        AuthoredRoster roster;
        roster.residents = {person("scribe", "the scribe", {"desk", 2.5, 2.5})};
        for (int i = 1; i <= clerks; ++i)
            roster.residents.push_back(person("clerk" + std::to_string(i), "works at the desk", {"desk", 2.5 + i, 3.5}, "civilian"));
        for (int i = 1; i <= 3; ++i)
            roster.residents.push_back(person("lone" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 50000;
        Society s(Roster::None);
        s.configure(roster);
        const auto office = s.tillOf("scribe");
        auto state = s.state();
        const double skilled = 1 + s.employerSkill(office) / items::ladder().skillWage;
        state.memory.employers[office] = {wage * skilled, clerkGone ? Society::OpenDays : 0};
        for (int i = 1; i <= 3; ++i)
            state.memory.employers[s.tillOf("lone" + std::to_string(i))] = {10 * skilled};
        if (clerkGone)
            for (auto& [pid, post] : state.careers.positions)
                if (post.holder == "clerk1")
                    post.holder.clear();
        state.memory.tradeNet[office] = {net, Society::SalesDays};
        if (cash > 0)
        {
            state.accounts.at("treasury").cash -= cash;
            state.accounts.at(office).cash += cash;
        }
        expect(s.restore(state), "the office restores");
        const auto bodies = atWork(s);
        Run r;
        for (std::int64_t d = 1; d <= 7; ++d)
        {
            s.tick(1, double(d) + .5 / 24, 0, bodies);   // (The day's pass: its review on one of the seven.)
            r.open = std::max(r.open, s.vacanciesOf(office));
        }
        r.wage = s.employerWage(office) / skilled, r.rate = s.goingRate("");
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        Society::setWorkByHours(false);
        return r;
    };
    const auto show = [](const Run& r) { return std::to_string(r.wage) + "p (rate " + std::to_string(r.rate) + "p, " + std::to_string(r.open) + " open)"; };
    const auto pays = run(Society::MoreHands, 1, true, 10, 1000, 0);
    expect(pays.open > 0 && pays.wage > 10.05 && std::abs(pays.rate - 10) < 1e-6, "a post open, its trade paying, it bids its wage up: " + show(pays));
    const auto poor = run(Society::MoreHands, 1, true, 10, 0, 0);
    expect(poor.open > 0 && std::abs(poor.wage - 10) < 1e-6, "its trade paying nothing, it keeps its wage, its post open: " + show(poor));
    const auto anyway = run(Society::MoreHands | Society::WageBySales, 1, true, 10, 0, 0);
    expect(anyway.wage > 10.05, "without the rule it bids up whatever it sells: " + show(anyway));
    const auto back = run(Society::MoreHands, 2, false, 12.5, 1000, 0);
    expect(back.open == 0 && std::abs(back.wage - (10 + 2.5 * (1 - Society::WageBack))) < 1e-6,
           "its posts filled, it drifts a quarter of the way back to the rate: " + show(back));
    const auto kept = run(Society::MoreHands | Society::WageBySales, 2, false, 12.5, 1000, 0);
    expect(std::abs(kept.wage - 12.5) < 1e-6, "without the rule it keeps its wage: " + show(kept));
    const auto extra = run(0, 0, true, 10, 100000, 20000);
    expect(extra.open > 0 && std::abs(extra.wage - 10) < 1e-6, "its only open post MoreHands' extra, it bids nothing: " + show(extra));
    const auto extraBids = run(Society::WageBySales, 0, true, 10, 100000, 20000);
    expect(extraBids.open > 0 && extraBids.wage > 10.05, "without the rule MoreHands' extra bids its wage up: " + show(extraBids));
}

// Round 13, lane 1 (the user, 2026-10-29: the going rate's slide, 60p to 38.8p in two years), with work by the hours:
// the realm's slack (the idle share of its grown wolves of working age; the weekly calm cut wants it over 3%) counts the
// living only, and a wolf holding a place on a week's hire as at work (LivingSlack). Forty grown: a baker, a stall and
// 33 lone scribes at their posts, and five with none. Three of the five are dead (in the residents, no body: an estate
// is settled, the resident kept), one holds a place on the bakery's hire for the week, one holds none: the slack is 1
// of the 37 living (2.7%), its grown the 37 (a House's estate crew a tenth of them: 3). Without the rule the dead and
// the hired are idle: 5 of 40 (12.5%), its grown 40 (4). (Forty, not twenty: one idle of twenty is 5%, over the calm
// cut's 3%.) And an employer CalmWeeks less one calm, nothing open, at its review: with the rule, the realm under 3%
// idle, it keeps its wage; without, the dead push the slack over, and it is cut.
void theSlackCountsTheLiving()
{
    struct Run
    {
        double slack = 0, wage = 0, before = 0;
        int estate = 0, calm = 0;
        bool hired = false, onReview = false;
    };
    const auto run = [](std::uint64_t off) {
        Society::setWorkByHours(true);
        Society::wageRulesOff(off);
        AuthoredRoster roster;
        roster.residents = {person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}), person("stall", "keeping the stall", {"market", 4.5, 4.5})};
        for (int i = 1; i <= 33; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        for (int i = 1; i <= 5; ++i)
            roster.residents.push_back(person("free" + std::to_string(i), "-", {"square", 2.5 + i, 2.5}, "civilian"));
        roster.economy.treasury = 200000;
        Society s(Roster::None);
        s.configure(roster);
        oneTown(s);
        {
            auto state = s.state();
            state.memory.pace = 1;
            expect(s.restore(state), "the calibrated hours restore");
        }
        const auto bakery = s.tillOf("baker");
        // The living (free1 to free3 dead: no body), and free4 alone about at ten, to take the bakery's hire.
        auto living = atWork(s);
        for (const char* dead : {"free1", "free2", "free3"})
            living.erase(dead);
        const std::map<std::string, LifeBody> taker = {{"free4", living.at("free4")}};
        // (Its bread bought each day and eaten, its till kept funded: it wants hands, and posts its hire for the week. The
        // day's pass at half past midnight, nobody about to take it.)
        const auto pass = [&](std::int64_t d) {
            set(s, "baker", "bread", 30);
            set(s, "baker", "flour", 40);
            expect(s.shift("treasury", "scribe1", "", 0, 30 * s.shopPrice("baker", "bread"), "test: the buyer's money"), "the buyer has money");
            expect(s.sale(bakery, "scribe1", "bread", 30, s.shopPrice("baker", "bread"), "test: the day's buying"), "the buyer buys");
            auto st = s.state();
            st.accounts.at("scribe1").stock.erase("bread");
            expect(s.restore(st), "eaten");
            if (const auto cash = s.account(bakery)->cash; cash < 5000)
                expect(s.shift("treasury", bakery, "", 0, 5000 - cash, "test: its till kept funded"), "its till funded");
            s.tick(1, double(d) + .5 / 24, 0, living);
        };
        // Its hire running past today with a place open, and free4 holding a place on it.
        const auto hire = [&](std::int64_t d) {
            return std::any_of(s.oddJobs().begin(), s.oddJobs().end(), [&](const Society::OddJob& j) {
                return j.payer == bakery && Society::businessHire(j) && j.until > d && !j.ended && int(j.stage.size()) < j.slots;
            });
        };
        const auto holds = [&](std::int64_t d) {
            return std::any_of(s.oddJobs().begin(), s.oddJobs().end(), [&](const Society::OddJob& j) {
                return j.payer == bakery && Society::businessHire(j) && j.until > d && !j.ended && j.stage.count("free4") > 0;
            });
        };
        std::int64_t d = 0;
        while (d < 14)
            pass(++d);
        Run r;
        for (int tries = 0; tries < 8 && !r.hired; ++tries)
        {
            pass(++d);
            if (hire(d))
                for (int i = 1; i <= 120; ++i)
                    s.tick(1, double(d) + 10. / 24 + i / 86400., 0, taker);
            r.hired = holds(d);
        }
        expect(r.hired, "free4 holds a place on the bakery's hire for the week");
        // Tomorrow's pass, the hire running: the slack, and an employer (a household's help, nothing open) at its review
        // that day, CalmWeeks less one calm.
        const std::int64_t m = d + 1;
        std::string key;
        for (const auto& [id, life] : s.state().residents)
            if (key.empty() && std::int64_t(std::hash<std::string>{}(id + "|help|wage") % 7) == m % 7)
                key = id + "|help";
        expect(!key.empty(), "a household's help reviewed that day");
        auto state = s.state();
        r.before = 1.1 * s.goingRate("");
        state.memory.employers[key] = EmployerBook{r.before, 0, Society::CalmWeeks - 1};
        expect(s.restore(state), "its book restores");
        pass(m);
        r.onReview = holds(m - 1);
        r.slack = s.slack(), r.estate = s.estateMost();
        r.wage = s.state().memory.employers.at(key).wage, r.calm = s.state().memory.employers.at(key).calm;
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        Society::setWorkByHours(false);
        return r;
    };
    const auto show = [](const Run& r) {
        return std::to_string(r.slack) + " slack, estate crews " + std::to_string(r.estate) + "; its wage " + std::to_string(r.before) + "p to " +
               std::to_string(r.wage) + "p, calm " + std::to_string(r.calm);
    };
    const auto now = run(0), before = run(Society::LivingSlack);
    expect(now.onReview && before.onReview, "free4 on the hire at the review's pass");
    expect(std::abs(now.slack - 1. / 37) < 1e-9 && now.estate == 3, "the living's slack, one of 37, the hired at work: " + show(now));
    expect(std::abs(before.slack - 5. / 40) < 1e-9 && before.estate == 4, "without the rule, the dead and the hired idle, five of 40: " + show(before));
    expect(std::abs(now.wage - now.before) < 1e-9 && now.calm == Society::CalmWeeks, "the realm under 3% idle, it keeps its wage: " + show(now));
    expect(before.wage < before.before - 1e-9 && before.calm == Society::CalmWeeks, "without the rule, the dead push it over, and it is cut: " + show(before));
}

// Round 13, lane 1 (the user, 2026-10-29), with work by the hours: a business short of the hands its sales want whose
// trade can't pay a higher wage keeps its wage and its posts open (WageBySales, round 12: doc 64), and with
// WageBookMended its weeks of calm too (it isn't calm: it is short). Before, it went on to count a week of calm, and at
// CalmWeeks to the calm cut: a fifth to a third of those cuts, by hands, in the 2-year run, were at tills four days open.
// aWageBidBySales's office, its clerk's post open (the clerk out of work: the realm a fifth idle), its book four days
// open and CalmWeeks less one calm, its trade nothing: with the rule, its wage and its calm as they were; without, a
// sixth week of calm, and the calm cut.
void aShortBusinessThatCantPayKeepsItsWage()
{
    struct Run
    {
        double wage = 0, slack = 0;
        int calm = 0;
        bool open = false;
    };
    const auto run = [](std::uint64_t off) {
        Society::setWorkByHours(true);
        Society::wageRulesOff(off | Society::MoreHands);   // (Its posts open by its sales alone, as aWageBidBySales's.)
        AuthoredRoster roster;
        roster.residents = {person("scribe", "the scribe", {"desk", 2.5, 2.5}), person("clerk1", "works at the desk", {"desk", 3.5, 3.5}, "civilian")};
        for (int i = 1; i <= 3; ++i)
            roster.residents.push_back(person("lone" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 50000;
        Society s(Roster::None);
        s.configure(roster);
        const auto office = s.tillOf("scribe");
        auto state = s.state();
        const double skilled = 1 + s.employerSkill(office) / items::ladder().skillWage;
        state.memory.employers[office] = {10 * skilled, Society::OpenDays, Society::CalmWeeks - 1};
        for (int i = 1; i <= 3; ++i)
            state.memory.employers[s.tillOf("lone" + std::to_string(i))] = {10 * skilled};
        for (auto& [pid, post] : state.careers.positions)
            if (post.holder == "clerk1")
                post.holder.clear();
        state.memory.tradeNet[office] = {0, Society::SalesDays};
        expect(s.restore(state), "the office restores");
        const auto bodies = atWork(s);
        const auto review = std::int64_t(std::hash<std::string>{}(office + "|wage") % 7);
        Run r;
        for (std::int64_t d = 1; d <= 7; ++d)
        {
            s.tick(1, double(d) + .5 / 24, 0, bodies);   // (The day's pass: its review on one of the seven.)
            if (d % 7 == review)
                r.open = s.vacanciesOf(office) > 0, r.slack = s.slack();
        }
        r.wage = s.employerWage(office) / skilled, r.calm = s.state().memory.employers.at(office).calm;
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        Society::setWorkByHours(false);
        return r;
    };
    const auto show = [](const Run& r) {
        return std::to_string(r.wage) + "p, calm " + std::to_string(r.calm) + (r.open ? ", its post open" : ", nothing open") + ", slack " +
               std::to_string(r.slack);
    };
    const auto now = run(0), before = run(Society::WageBookMended);
    expect(now.open && before.open && now.slack > .03 && before.slack > .03, "its post open at its review, the realm over 3% idle: " + show(now));
    expect(std::abs(now.wage - 10) < 1e-6 && now.calm == Society::CalmWeeks - 1, "short of hands, its trade unable to pay more, it keeps its wage and its calm: " + show(now));
    expect(before.wage < 10 - 1e-6 && before.calm == Society::CalmWeeks, "without the rule, a sixth week of calm, and the calm cut: " + show(before));
}

// Round 10, lane A (the user, 2026-10-27), with work by the hours: a House's week's budget, by the model's rule (k 4,
// a2 0.3), over all it pays out, in parts. House Fell holds 5,000p on a month's income of 1,000p (and its caravans' 0)
// at the day-35 reckoning: its budget (1,000 + 0.3 x (5,000 - 4,000)) / 4 = 325p. Its floor is a week's wages for
// twelve at the going rate, and spendable keeps it. Its posts, then its caravans, then its investments in its
// businesses get a tenth of what's left each (they paid nothing last week: else last week's and a third over); its
// works and crews the rest. Its stall, run dry, is topped up with no more than that part in the week, however short;
// a second look the same week gives nothing. Money for the road draws on its caravans' part. Below its target (3,000p
// at day 42, under four months' income) its investments pause: the stall is topped up with nothing, it is marked as a
// lone owner's stopped till is (round 9), and on a day its till can't pay its manager's wage the manager is free for
// hire work. The parts and the mark read back from a save. At its target with a large part, a look tops it up in full
// and clears the mark. A caravan home with 2,000p: on top of its next budget, its caravans' part that. Without the rule
// the House tops its stall up in full every look; without the till's half, nothing is marked.
void aHouseSpendsInsideItsWeek()
{
    struct Run
    {
        std::int64_t floor = 0, posts = 0, road = 0, tops = 0, budget = 0, rate = 0, cash = 0, spendable = 0, roadMay = 0, roadAfter = 0;
        std::int64_t invested = 0, again = 0, investedLow = 0, invested3 = 0, posts56 = 0, road56 = 0;
        bool marked = false, unpaid = false, saved = false, cleared = false;
    };
    const auto run = [](std::uint64_t off) {
        Society::setWorkByHours(true);
        Society::wageRulesOff(off | Round11Homes);
        AuthoredRoster roster;
        roster.residents = {person("lord", "ruling House Fell", {"hall", 2.5, 2.5}, "civilian"),
                            person("housestall", "keeping the stall", {"market2", 4.5, 4.5})};
        for (int i = 1; i <= 20; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}, "civilian"));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        LifeDay plan;
        plan.communityOf = [](const std::string&) { return std::string("town"); };
        s.setDay(plan);
        const auto& job = *s.jobOf("housestall");
        const auto till = "till:" + job.id;
        const std::string house = "house:house_fell";
        auto owned = s.state();
        owned.houses.owner[job.id] = house;
        expect(s.restore(owned) && s.ownerOfBusiness(job.id) == house, "the stall is House Fell's");
        const auto bodies = atWork(s);
        for (std::int64_t d = 1; d <= 34; ++d)
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        // The reckoning's eve: what the House holds and earned, and what its parts paid last week.
        const auto eve = [&](std::int64_t cash, double lastTops) {
            auto st = s.state();
            auto& purse = st.accounts.at(house).cash;
            st.accounts.at("treasury").cash += purse - cash;
            purse = cash;
            st.memory.monthIncome[house] = 1000 * 28. / 27;   // (A 28th off the next morning: 1,000p.)
            st.memory.spendWeek[house + "|takings"] = 0;
            for (const auto* part : {"|posts", "|road"})
                st.memory.spendWeek[house + part] = 0;
            st.memory.spendWeek[house + "|tops"] = lastTops;
            expect(s.restore(st), "the eve restores");
        };
        const auto dry = [&] { s.shift(till, "treasury", "", 0, s.account(till)->cash, "test: the till run dry"); };
        const auto week = [&](std::int64_t from) {
            s.takeJournal();
            std::int64_t in = 0;
            for (std::int64_t day = from; day < from + 7; ++day)
                if (Society::ownersWeekday(job.id) == day % 7)
                {
                    dry();
                    s.tendTills(day);
                }
            for (const auto& e : s.takeJournal())
                in += e.kind == "an owner's investment" && e.to == till ? e.coins : 0;
            return in;
        };
        const auto left = [&](const char* part) {
            const auto& m = s.state().memory.spendLeft;
            const auto found = m.find(house + part);
            return found == m.end() ? std::int64_t(-1) : std::int64_t(found->second);
        };
        Run r;
        eve(5000, 0);
        s.tick(1, 35 + .5 / 24, 0, bodies);
        r.floor = left("|floor"), r.posts = left("|posts"), r.road = left("|road"), r.tops = left("|tops");
        r.rate = std::int64_t(std::ceil(s.goingRate("town") - 1e-9));
        r.cash = s.account(house)->cash;
        r.spendable = s.spendable(house);
        r.roadMay = s.houseMay(house, Society::HouseRoad);
        s.openAccount("caravan:fell1");
        expect(s.shift(house, "caravan:fell1", "", 0, 100, "money for the road"), "a caravan sets out");
        r.roadAfter = left("|road");
        r.invested = week(35);
        r.again = week(35);
        // Below its target: its investments pause.
        eve(3000, 0);
        s.tick(1, 42 + .5 / 24, 0, bodies);
        r.investedLow = week(42);
        r.marked = s.state().memory.shortTills.count(till) && s.state().memory.shortTills.at(till).stopped >= 0;
        dry();
        s.tendHouses(44);
        auto st = s.state();
        st.budgetDay = 44;
        expect(s.restore(st), "the day restores");
        r.unpaid = s.ownerUnpaid("housestall", job);
        const auto back = wire::readSociety(wire::society(s.state())).memory;
        r.saved = back.spendLeft == s.state().memory.spendLeft && back.spendWeek == s.state().memory.spendWeek &&
                  back.shortTills.count(till) == s.state().memory.shortTills.count(till);
        // Its target again, and a large part: the stall topped up in full, and the mark cleared.
        eve(200000, 100000);
        s.tick(1, 49 + .5 / 24, 0, bodies);
        r.invested3 = week(49);
        r.cleared = !s.state().memory.shortTills.count(till);
        // Its caravan home with 2,000p: on top of its next week's budget, and its caravans' part at least that.
        eve(5000, 0);
        s.openAccount("caravan:fell1");
        expect(s.shift("treasury", "caravan:fell1", "", 0, 2000, "test: sold on the road") && s.shift("caravan:fell1", house, "", 0, 2000, "trade takings"),
               "the caravan comes home");
        s.tick(1, 56 + .5 / 24, 0, bodies);
        r.posts56 = left("|posts"), r.road56 = left("|road");
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        Society::setWorkByHours(false);
        return r;
    };
    const auto r = run(0), without = run(Society::HouseBudget), untilled = run(Society::HouseTills);
    const auto say = [](const Run& x) {
        return "floor " + std::to_string(x.floor) + ", posts " + std::to_string(x.posts) + ", road " + std::to_string(x.road) + ", tops " +
               std::to_string(x.tops) + "; invested " + std::to_string(x.invested) + ", again " + std::to_string(x.again) + ", below " +
               std::to_string(x.investedLow) + ", at its target " + std::to_string(x.invested3);
    };
    expect(r.floor == 7 * Society::HouseFloorHands * r.rate && r.spendable == r.cash - r.floor,
           "its floor is a week's wages for twelve, and spendable keeps it: " + say(r) + ", rate " + std::to_string(r.rate));
    // (Its budget: (1,000 + 0.3 x (5,000 - 4,000)) / 4 = 325p, as far as it holds over its floor; a little more for
    // what the day earned.)
    const auto budget = r.posts + r.road + r.tops;
    expect(r.posts >= 32 && r.posts <= 36 && std::abs(r.road - (r.posts * 10 - r.posts) / 10) <= 2 &&
               std::abs(r.tops - (r.posts * 10 - r.posts - r.road) / 10) <= 2,
           "its posts and caravans a tenth of what's left each, and its investments (nothing last week): " + say(r) + ", " +
               std::to_string(budget));
    expect(r.roadMay == std::min(r.road, r.spendable) && r.roadAfter == r.road - 100, "money for the road draws on its caravans' part: " + say(r));
    expect(r.invested > 0 && r.invested <= r.tops && r.again == 0, "its stall topped up with its investments' part, and no more that week: " + say(r));
    expect(r.investedLow == 0 && r.marked && r.unpaid, "below its target its investments pause, and its stall trades on as a lone owner's: " + say(r));
    expect(r.saved, "its parts and its stall's mark read back from a save");
    expect(r.invested3 > r.tops && r.cleared, "topped up in full at its target, and the mark cleared: " + say(r));
    // (Day 56: 325p and the 2,000p home, as far as it holds over its floor: its posts a tenth, its caravans the 2,000p.)
    expect(r.posts56 >= 220 && r.posts56 <= 260 && r.road56 == 2000,
           "its caravans' takings come on top of its next budget, and their part is at least what they brought: posts " +
               std::to_string(r.posts56) + ", road " + std::to_string(r.road56));
    expect(without.invested > r.tops && without.again > 0 && without.investedLow > 0 && without.floor < 0 && without.spendable == without.cash,
           "without the rule, the House tops its stall up in full every look: " + say(without));
    expect(untilled.investedLow == 0 && !untilled.marked && !untilled.unpaid, "without the till's half, nothing is marked: " + say(untilled));
}

// Round 10, lane A's follow-up (the user, 2026-10-27), with work by the hours: what a House didn't pay out of last week's
// budget goes to this week's works and crews. House Fell, 20,000p on a month's income of 4,000p, sets its week's parts
// at the day-35 reckoning; nothing of its posts', caravans' or investments' parts is paid that week, nor all its works
// and crews' (its crews' places stand untaken). At day 42 its works and crews' part is the week's own and what was left
// of each last week, as far as it holds over its floor and at most the week's own budget again; without the rule, the
// week's own. Its parts are the same both ways.
void aHouseCarriesWhatItDidntPay()
{
    struct Run
    {
        std::int64_t left = 0, works = 0, posts = 0, road = 0, tops = 0, drawn = 0;
    };
    const auto run = [](std::uint64_t off) {
        Society::setWorkByHours(true);
        Society::wageRulesOff(off | Round11Homes);
        AuthoredRoster roster;
        roster.residents = {person("lord", "ruling House Fell", {"hall", 2.5, 2.5}, "civilian")};
        for (int i = 1; i <= 20; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}, "civilian"));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        LifeDay plan;
        plan.communityOf = [](const std::string&) { return std::string("town"); };
        s.setDay(plan);
        const std::string house = "house:house_fell";
        const auto bodies = atWork(s);
        for (std::int64_t d = 1; d <= 34; ++d)
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        const auto eve = [&] {
            auto st = s.state();
            auto& purse = st.accounts.at(house).cash;
            st.accounts.at("treasury").cash += purse - 20000;
            purse = 20000;
            st.memory.monthIncome[house] = 4000 * 28. / 27;
            for (const auto* part : {"|posts", "|road", "|tops", "|takings"})
                st.memory.spendWeek[house + part] = 0;
            expect(s.restore(st), "the eve restores");
        };
        const auto at = [&](const char* part) {
            const auto& m = s.state().memory.spendLeft;
            const auto found = m.find(house + part);
            return found == m.end() ? std::int64_t(-1) : std::int64_t(found->second);
        };
        eve();
        s.tick(1, 35 + .5 / 24, 0, bodies);
        Run r;
        // (A crew's share paid draws on its works and crews' part.)
        const auto before = at("|works");
        expect(s.shift(house, "scribe1", "", 0, 100, "an odd job: building work"), "a share paid");
        r.drawn = before - at("|works");
        eve();
        for (const auto* part : {"|posts", "|road", "|tops", "|works"})
            r.left += std::max<std::int64_t>(0, at(part));
        s.tick(1, 42 + .5 / 24, 0, bodies);
        r.works = at("|works"), r.posts = at("|posts"), r.road = at("|road"), r.tops = at("|tops");
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        Society::setWorkByHours(false);
        return r;
    };
    const auto r = run(0), without = run(Society::CarryParts);
    const auto say = [](const Run& x) {
        return "left " + std::to_string(x.left) + ", works " + std::to_string(x.works) + ", posts " + std::to_string(x.posts) + ", road " +
               std::to_string(x.road) + ", tops " + std::to_string(x.tops);
    };
    expect(r.drawn == 100, "a crew's share draws on its works and crews' part: " + std::to_string(r.drawn));
    expect(r.left > 0 && r.posts == without.posts && r.road == without.road && r.tops == without.tops,
           "something was left, and its parts are the same: " + say(r) + "; without " + say(without));
    // (At most the week's own budget again: its parts and its works and crews' without the carry.)
    const auto own = without.works + without.posts + without.road + without.tops;
    expect(r.works == without.works + std::min(r.left, own), "its works and crews get last week's unpaid on top, at most the week's own budget again: " +
                                                                 say(r) + "; without " + say(without));
}

// Round 10, lane A (the user, 2026-10-27), with work by the hours: the church's alms bought for the hungry come out of
// its week's relief with its relief, over its floor (a month of its income). At the day-35 reckoning it holds 300,000p
// on a month's income of 100,000p: its floor 100,000p, and its relief what it gave last week (alms and relief) and a
// third over. Alms bought draw on it, and count in the week's given; once it's spent, no more. Without the rule its
// alms are bought on top, as many as asked, and it has no floor.
void theChurchsAlmsInsideItsRelief()
{
    struct Run
    {
        std::int64_t floor = -1, cash = 0, spendable = 0, relief = 0, mayBefore = 0, mayAfter = 0;
        double given = 0;
        bool early = false;
    };
    const auto run = [](std::uint64_t off) {
        Society::setWorkByHours(true);
        Society::wageRulesOff(off | Round11Homes);
        AuthoredRoster roster;
        roster.residents = {person("stall", "keeping the stall", {"market", 4.5, 4.5})};
        for (int i = 1; i <= 20; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}, "civilian"));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        LifeDay plan;
        plan.communityOf = [](const std::string&) { return std::string("town"); };
        s.setDay(plan);
        const std::string church = Society::SharedChurch;
        s.openAccount(church);
        const auto bodies = atWork(s);
        Run r;
        for (std::int64_t d = 1; d <= 34; ++d)
        {
            s.tick(1, double(d) + .5 / 24, 0, bodies);
            // (Before its first week's reckoning, whatever its first day set aside for relief: all it holds, as before the
            // rule.)
            if (d == 3)
            {
                auto st = s.state();
                st.memory.reliefLeft[church] = 0;
                expect(s.restore(st), "nothing set aside");
                r.early = s.almsMay() == s.account(church)->cash && s.almsMay() > 0;
            }
        }
        const auto eve = [&] {
            auto st = s.state();
            auto& purse = st.accounts.at(church).cash;
            st.accounts.at("treasury").cash += purse - 300000;
            purse = 300000;
            st.memory.monthIncome[church] = 100000 * 28. / 27;
            st.memory.reliefWeek[church] = 6000;
            st.memory.wagesWeek.erase(church);
            expect(s.restore(st), "the eve restores");
        };
        eve();
        s.tick(1, 35 + .5 / 24, 0, bodies);
        const auto& m = s.state().memory;
        r.floor = m.spendLeft.count(church + "|floor") ? std::int64_t(m.spendLeft.at(church + "|floor")) : -1;
        r.cash = s.account(church)->cash;
        r.spendable = s.spendable(church);
        r.relief = std::int64_t(m.reliefLeft.at(church));
        r.mayBefore = s.almsMay();
        // Alms bought, a loaf at a time, to the whole of its relief and past it.
        for (int i = 0; i < 9000; ++i)
        {
            if (i % 99 == 0)
                set(s, "stall", "bread", 99);
            if (s.almsMay() < 1 || !s.sale(s.tillOf("stall"), church, "bread", 1, 1, "alms bought for the hungry"))
                break;
        }
        r.mayAfter = s.almsMay();
        r.given = s.state().memory.reliefWeek.count(church) ? s.state().memory.reliefWeek.at(church) : 0;
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        Society::setWorkByHours(false);
        return r;
    };
    const auto r = run(0), without = run(Society::ChurchBudget);
    const auto say = [](const Run& x) {
        return "floor " + std::to_string(x.floor) + ", spendable " + std::to_string(x.spendable) + " of " + std::to_string(x.cash) + ", relief " +
               std::to_string(x.relief) + ", alms may " + std::to_string(x.mayBefore) + " then " + std::to_string(x.mayAfter) + ", given " +
               std::to_string(std::int64_t(x.given));
    };
    expect(r.early && without.early, "before its first week's reckoning, its alms may spend all it holds");
    expect(r.floor >= 99000 && r.floor <= 101000 && r.spendable == r.cash - r.floor, "its floor is a month of its income: " + say(r));
    expect(r.relief == 7980 && r.mayBefore == r.relief, "its relief is last week's and a third, and its alms may spend it: " + say(r));
    expect(r.mayAfter == 0 && std::abs(r.given - double(r.relief)) < 2, "its alms draw on it, count as given, and stop when it's spent: " + say(r));
    expect(without.floor < 0 && without.spendable == without.cash && without.mayAfter == without.cash - 9000 && without.given < 1,
           "without the rule its alms are bought on top, and it keeps no floor: " + say(without));
}

// Round 10, lane A (the user, 2026-10-27), with work by the hours: alms buy the cheapest food that feeds the wolf (a
// loaf's nourishment, AlmsFeeds), not the shop's best. A provisioner holds eggs (the cheapest, too little to feed),
// smoked fish (the cheapest that feeds) and jerky (the richest, dearer): a penniless child with a bare larder is given
// one smoked fish, at its price, and almsFood names it where bestFood names the jerky. Without the rule, the church
// buys as before.
void theAlmsBuyTheCheapestThatFeeds()
{
    const auto run = [](std::uint64_t off) {
        Society::setWorkByHours(true);
        Society::wageRulesOff(off | Round11Homes);
        AuthoredRoster roster;
        auto provisioner = person("provisioner", "the provisioner by the gate", {"provisions", 2.5, 2.5});
        provisioner.home = {"provisions_flat", 2.5, 2.5};
        auto ma = person("ma", "weaving at home", {"homes", 3.5, 2.5}, "civilian");
        auto pip = person("pip", "plays in the streets", {"homes", 4.5, 2.5}, "civilian");
        pip.age = 8;
        roster.residents = {provisioner, ma, pip};
        roster.economy.treasury = 5000;
        Society s(Roster::None);
        s.configure(roster);
        LifeDay town;
        town.communityOf = [](const std::string&) { return std::string("town"); };
        s.setDay(town);
        auto bodies = atWork(s);
        bodies["pip"].age = 8;
        s.openAccount(Society::SharedChurch);
        expect(s.shift("treasury", Society::SharedChurch, "", 0, 1000, "test: the plate"), "the church has alms to give");
        const auto till = s.tillOf("provisioner");
        {
            auto state = s.state();
            state.accounts.at(till).stock = {{"eggs", 20}, {"jerky", 20}, {"smoked_fish", 20}};
            state.accounts.at("pip").stock.clear();
            state.accounts.at("treasury").cash += state.accounts.at("pip").cash;
            state.accounts.at("pip").cash = 0;
            if (const auto larder = state.accounts.find(Society::homeStore("homes", "larder")); larder != state.accounts.end())
                larder->second.stock.clear();
            expect(s.restore(state), "a penniless child, a bare larder and a provisioner's shelves restore");
        }
        std::int64_t price = 0;
        const auto named = s.almsFood("provisioner", price);
        const auto best = Society::bestFood(*s.account(till));
        const bool priced = price == s.shopPrice("provisioner", "smoked_fish") && price > s.shopPrice("provisioner", "eggs");
        s.takeJournal();
        s.tick(1, 1 + 10. / 24, 0, bodies);
        std::vector<std::string> alms;
        for (const auto& e : s.takeJournal())
            if (e.kind == "alms bought for the hungry")
                for (int i = 0; i < e.quantity; ++i)
                    alms.push_back(items::baseOf(e.item));
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        Society::setWorkByHours(false);
        return std::make_tuple(named, best, priced, alms);
    };
    const auto [named, best, priced, alms] = run(0);
    const auto [named0, best0, priced0, alms0] = run(Society::CheapAlms);
    const auto show = [](const std::vector<std::string>& v) {
        std::string t;
        for (const auto& x : v)
            t += (t.empty() ? "" : ", ") + x;
        return t;
    };
    expect(named == "smoked_fish" && priced && best == "jerky", "the cheapest that feeds is the smoked fish, at its price: " + named + " (the best: " + best + ")");
    expect(alms == std::vector<std::string>{"smoked_fish"}, "the hungry child is given one smoked fish: " + show(alms));
    expect(alms0 != alms, "without the rule, the church buys as before: " + show(alms0));
}

// A seasonal field's farm (round 9's review, the user, 2026-10-27) is judged over its season: its money comes at the
// harvest, so its owner tops it up through the year however short it is, and stops only if it is still short at a
// look after the harvest's end. A farm with no fields of its year (a woodlot) stops at its fourth short look, as a
// shop does.
void aFarmIsJudgedAfterItsHarvest()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    roster.residents = {person("farmer", "farms the valley fields", {"fields", 4.5, 4.5}, "civilian"),
                        person("cutter", "cuts wood in the copse", {"copse", 2.5, 2.5}, "civilian")};
    roster.economy.treasury = 1000000;
    Society s(Roster::None);
    s.configure(roster);
    const auto farm = s.tillOf("farmer"), copse = s.tillOf("cutter");
    expect(farm.rfind("till:", 0) == 0 && copse.rfind("till:", 0) == 0, "each keeps its own till");
    auto st = s.state();
    st.memory.fields[farm] = FieldYear{10, 10, 10, 0, 0, false, 1, 0, false, 1, 0, 7};   // (Its year: sown, to bring in.)
    expect(s.restore(st), "the farm's fields sown");
    for (const std::string who : {"farmer", "cutter"})
        expect(s.shift("treasury", who, "", 0, 100000, "test: the owner's purse"), "the owners have money");
    // Every day from spring to past the harvest, its till run dry (no harvest's money comes): each week's look finds
    // it short.
    std::int64_t farmStopped = -1, copseStopped = -1, firstShort = -1;
    std::int64_t putInBefore = 0, putInAfter = 0;
    for (std::int64_t day = 150; day < Society::HarvestEnd + 21; ++day)
    {
        for (const auto& t : {farm, copse})
            s.shift(t, "treasury", "", 0, s.account(t)->cash, "test: the till run dry");
        s.takeJournal();
        s.tendTills(day);
        for (const auto& e : s.takeJournal())
            if (e.kind == "an owner's investment" && e.to == farm)
                (day < Society::HarvestEnd ? putInBefore : putInAfter) += e.coins;
        const auto& shorts = s.state().memory.shortTills;
        if (const auto f = shorts.find(farm); f != shorts.end())
        {
            if (firstShort < 0)
                firstShort = day;
            if (farmStopped < 0 && f->second.stopped >= 0)
                farmStopped = f->second.stopped;
        }
        if (const auto c = shorts.find(copse); c != shorts.end() && copseStopped < 0 && c->second.stopped >= 0)
            copseStopped = c->second.stopped;
    }
    expect(firstShort >= 150 && firstShort < 157, "the farm's first look finds it short: day " + std::to_string(firstShort));
    expect(copseStopped >= 150 + 21 && copseStopped < 150 + 28, "the woodlot stops at its fourth short look: day " + std::to_string(copseStopped));
    expect(putInBefore > 0, "the farm's owner tops it up through the year to its harvest: " + std::to_string(putInBefore));
    expect(farmStopped >= Society::HarvestEnd && farmStopped < Society::HarvestEnd + 7,
           "still short after its harvest, at its first look after it the farm stops: day " + std::to_string(farmStopped));
    expect(s.conserved(), "money stays conserved");
    expect(Society::harvestEndedSince(Society::HarvestEnd - 1, Society::HarvestEnd) &&
               !Society::harvestEndedSince(Society::HarvestEnd, Society::HarvestEnd + 300) &&
               Society::harvestEndedSince(Society::HarvestEnd, Society::HarvestEnd + 365) && Society::harvestEndedSince(-5, Society::HarvestEnd),
           "a harvest's end between two days, over the year's turn too");
    Society::setWorkByHours(false);
}

// The keeper of a business its owner has stopped topping up (round 9), on a day its till can't pay it, looks for hire
// work as an unpaid hand does: a stall's keeper after its own hours (on them it keeps shop), and a woodcutter leaves its
// wood. A keeper whose owner still tops it up keeps to its own work, and so does a stopped one whose till paid it that
// day.
void aStoppedKeeperTakesAHire()
{
    Society::setWorkByHours(true);
    const auto run = [](bool tillPays) {
        AuthoredRoster roster;
        roster.residents = {person("stall", "keeping the stall", {"market", 4.5, 4.5}),
                            person("stall2", "keeping the stall", {"market2", 4.5, 4.5}),
                            person("cutter", "cuts wood in the copse", {"copse", 2.5, 2.5}, "civilian"),
                            person("cutter2", "cuts wood in the copse", {"copse2", 2.5, 2.5}, "civilian")};
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        LifeDay plan;
        plan.communityOf = [](const std::string&) { return std::string("town"); };
        s.setDay(plan);
        auto bodies = atWork(s);
        s.tick(1, 1 + .5 / 24, 0, bodies);
        // The stall's and the first woodcutter's owners have stopped; every till is run dry, or the stopped ones given a
        // day's wage.
        auto st = s.state();
        for (const std::string who : {"stall", "cutter"})
            st.memory.shortTills[s.tillOf(who)] = {Society::StopWeeks, 1, -1};
        expect(s.restore(st), "the stopped owners restore");
        for (const std::string who : {"stall", "stall2", "cutter", "cutter2"})
        {
            const auto till = s.tillOf(who);
            expect(till.rfind("till:", 0) == 0, who + " keeps its own till");
            s.shift(till, "treasury", "", 0, s.account(till)->cash, "test: the till run dry");
            if (tillPays && (who == "stall" || who == "cutter"))
                s.shift("treasury", till, "", 0, std::int64_t(std::ceil(s.employerWage(till))) + 1, "test: a day's wage");
        }
        s.tick(1, 2 + 9. / 24, 0, bodies);           // (The day's turn: the keepers' wages, as far as their tills pay.)
        expect(!s.postOddJob("treasury", "town", "project", 4, 400, {"site", 2.5, 2.5}, "clearing the site").empty(), "a day's work posted");
        // On its hours (10 to 11) and after them (17 to 18): who has taken a place on the day's work. (The stall keeps
        // shop on its own hours, 8 to 17, and takes hire work only outside them: the user, the round's review.)
        std::set<std::string> took;
        for (const double from : {10., 17.})
            for (double t = from; t < from + 1; t += 1. / 60)
            {
                s.tick(60, 2 + t / 24, 0, bodies);
                for (const auto& j : s.oddJobs())
                    for (const auto& [who, stage] : j.stage)
                        if (j.kind == "project")
                        {
                            took.insert(from < 12 ? who : who + " (evening)");
                            if (stage < 2)
                                bodies[who] = {j.from.cell, j.from.x, j.from.y, false, bodies[who].age};
                        }
            }
        for (const std::string who : {"stall", "cutter", "stall2", "cutter2"})
            if (took.count(who))
                took.erase(who + " (evening)");     // (Taken on its hours, held after.)
        // (The round-9 review.) The next day its till pays its keeper again: the stall's keeper is called back to its
        // shop from the work it held, and the woodcutter, an unpaid hand, isn't (it comes back to it as the unpaid do).
        if (!tillPays)
            for (const std::string who : {"stall", "cutter"})
                s.shift("treasury", s.tillOf(who), "", 0, std::int64_t(std::ceil(s.employerWage(s.tillOf(who)))) + 1, "test: a day's wage");
        s.takeJournal();
        s.tick(1, 3 + 5. / 24, 0, bodies);
        std::set<std::string> back;
        for (const auto& e : s.takeJournal())
            if (e.kind == "called back to its shop")
                back.insert(e.from);
        expect(back == (tillPays ? std::set<std::string>{} : std::set<std::string>{"stall"}),
               std::string(tillPays ? "none held work: nobody" : "its till paying it again, the stall's keeper") + " is called back to its shop (" +
                   std::to_string(back.size()) + ")");
        expect(s.conserved(), "money stays conserved");
        return took;
    };
    const auto took = run(false);
    const auto names = [](const std::set<std::string>& v) {
        std::string text;
        for (const auto& x : v)
            text += (text.empty() ? "" : ", ") + x;
        return "{" + text + "}";
    };
    expect(took == std::set<std::string>{"stall (evening)", "cutter"},
           "the woodcutter its till can't pay takes the day's work, and the stall's keeper after its hours; the others keep to "
           "theirs: " + names(took));
    const auto paid = run(true);
    expect(paid.empty(), "paid by their tills that day, they keep to their own work: " + names(paid));
    Society::setWorkByHours(false);
}

// A hungry wolf buys where it can pay for food (doc 64, Phase 13, round 10, lane C), with work by the hours: at the
// bakery's counter, holding the price of its cheapest loaf but less than the bite (a third of a day), it buys and eats;
// holding less than the loaf it asks alms. Without the rule (as before), with the price of the loaf it asks alms too.
void aHungryWolfBuysAtItsPrice()
{
    Society::setWorkByHours(true);
    const auto run = [](bool rule, bool loaf) {
        // (Without it, round 14's ShopsCompared off too: it looks whatever the purse, so the bite's rule is round 10's.)
        Society::wageRulesOff(rule ? 0 : Society::FoodAtItsPrice | Society::ShopsCompared);
        auto s = folkTown();
        auto state = s.state();
        state.residents.at("wren").hunger = 70;
        expect(s.restore(state), "hungry wren restores");
        std::map<std::string, LifeBody> bodies = atWork(s);
        bodies["wren"] = {"bakery", 2.5, 2.5};     // At the counter.
        double day = 10. / 24;
        s.tick(1, day, 0, bodies);                  // (The baker at its counter: the bakery open.)
        const auto cheapest = s.cheapestFood("baker"), bite = s.pennies(4 / 12., "");
        expect(cheapest > 0 && cheapest < bite, "the bakery's cheapest food (" + std::to_string(cheapest) + "p) under the bite (" +
                                                    std::to_string(bite) + "p)");
        const auto want = loaf ? cheapest : cheapest - 1, cash = s.account("wren")->cash;
        if (cash > want)
            s.shift("wren", "treasury", "", 0, cash - want, "test: wren's purse");
        else
            s.shift("treasury", "wren", "", 0, want - cash, "test: wren's purse");
        s.takeJournal();
        bool ate = false, alms = false;
        for (int i = 0; i < 400 && !ate; ++i)
        {
            day += 1. / 86400;
            s.tick(1, day, 0, bodies);
            ate = s.resident("wren")->hunger < 60;
            alms = alms || s.resident("wren")->task == "seeking alms";
        }
        int bought = 0;
        for (const auto& e : s.takeJournal())
            bought += e.kind == "resident food purchase" && e.from == "wren" ? e.quantity : 0;
        expect(s.conserved(), "money stays conserved");
        return std::tuple{bought, ate, alms};
    };
    const auto [bought, ate, alms] = run(true, true);
    expect(bought > 0 && ate && !alms, "with the price of a loaf and less than the bite, wren buys and eats: bought " + std::to_string(bought));
    const auto [boughtShort, ateShort, almsShort] = run(true, false);
    expect(boughtShort == 0 && almsShort, "a penny short of the loaf, wren asks alms");
    const auto [boughtBefore, ateBefore, almsBefore] = run(false, true);
    expect(boughtBefore == 0 && almsBefore, "without the rule, with the price of a loaf, wren asks alms (as before)");
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// A festival's feast laid in (doc 64, Phase 13, round 10, lane C, the user's), with work by the hours: at the turn of a
// festival's day the town's stores buy from its food shops a meal's worth for each wolf living there, meals first, no
// more than half of what a shop holds of a food; on another day, nothing. At the feast, with no meal in the stores, a
// wolf is given a meal's worth of what they hold. Without the rule nothing is bought, and the feast (a meal from the
// stores, which hold none) gives nothing.
void aFestivalsFeastLaidIn()
{
    Society::setWorkByHours(true);
    std::int64_t festival = -1, other = -1;
    for (std::int64_t d = 1; d < 400 && festival < 0; ++d)
        if (calendar::festivalDay(double(d) + .5))
            festival = d;
    other = festival - 3;
    expect(festival > 0 && !calendar::festivalDay(double(other) + .5), "a festival's day in the calendar: " + std::to_string(festival));
    const auto run = [&](bool rule, std::int64_t day) {
        Society::wageRulesOff(rule ? 0 : Society::FeastLaidIn);
        auto s = folkTown();
        set(s, "baker", "bread", 40);
        set(s, "baker", "meal", 4);
        auto bare = s.state();
        bare.accounts.at("treasury").stock.clear(); // (The stores hold no food, as the capital's on day 230.)
        expect(s.restore(bare), "the bare stores restore");
        s.takeJournal();
        s.layInTheFeast(day);
        int meals = 0, bread = 0;
        for (const auto& e : s.takeJournal())
            if (e.kind == "bought for the festival feast")
                (items::baseOf(e.item) == "meal" ? meals : bread) += e.quantity;
        expect(s.conserved(), "money stays conserved");
        return std::tuple{meals, bread, std::move(s)};
    };
    {
        auto [meals, bread, s] = run(true, festival);
        // Four wolves at home: 200 of nourishment. Two meals (half the baker's four) are 100, then seven loaves of 15.
        expect(meals == 2 && bread == 7, "the stores buy the feast, meals first: " + std::to_string(meals) + " meals, " +
                                             std::to_string(bread) + " loaves");
        expect(Society::stock(*s.account("treasury"), "meal") == 2, "and hold it");
        // The feast: the stores' meals given out first, then a meal's worth of their bread.
        auto state = s.state();
        state.accounts.at("treasury").stock.erase("meal");
        expect(s.restore(state), "the stores without their meals restore");
        LifeDay plan;
        plan.communityOf = [](const std::string&) { return std::string("town"); };
        DayPlan fair;
        fair.kind = "festival";
        fair.name = "the Fair";
        fair.crowd = {{"square", 2.5, 2.5}};
        plan.plans["town"] = fair;
        s.setDay(plan);
        auto bodies = atWork(s);
        bodies["wren"] = {"square", 2.5, 2.5};
        s.takeJournal();
        for (int i = 0; i < 30; ++i)
            s.tick(1, double(festival) + 13. / 24 + i / 86400., 0, bodies);
        // (The day's turn, in the ticks, lays in again what the stores lack: a meal, and bread. Wren is given the meal.)
        int given = 0, bought = 0;
        for (const auto& e : s.takeJournal())
        {
            bought += e.kind == "bought for the festival feast" ? e.quantity : 0;
            given += e.kind == "festival feast" && e.to == "wren" ? e.quantity * Society::nourishment(e.item) : 0;
        }
        expect(bought > 0 && given >= 50, "at the day's turn the stores lay in what they lack (" + std::to_string(bought) +
                                              "), and at the feast wren is given a meal's worth: " + std::to_string(given));
        // With only bread left in the stores, ash comes to the feast and is given a meal's worth of it: four loaves.
        // Wren, fed once, is given no more.
        state = s.state();
        state.accounts.at("treasury").stock.clear();
        state.accounts.at("treasury").stock["bread"] = 10;
        expect(s.restore(state), "the stores with bread alone restore");
        bodies["ash"] = {"square", 2.5, 2.5};
        s.takeJournal();
        int loaves = 0, again = 0;
        for (int i = 0; i < 30; ++i)
            s.tick(1, double(festival) + 14. / 24 + i / 86400., 0, bodies);
        for (const auto& e : s.takeJournal())
            if (e.kind == "festival feast")
                (e.to == "ash" && items::baseOf(e.item) == "bread" ? loaves : again) += e.quantity;
        expect(loaves == 4 && again == 0, "ash is given four loaves, and wren nothing more: " + std::to_string(loaves) + ", " + std::to_string(again));
    }
    {
        auto [meals, bread, s] = run(true, other);
        expect(meals == 0 && bread == 0, "on another day, nothing is bought");
    }
    {
        auto [meals, bread, s] = run(false, festival);
        expect(meals == 0 && bread == 0, "without the rule, nothing is bought (as before)");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// The nearest shop (doc 64, Phase 13, round 10, lane C, the user's), with work by the hours: a hungry wolf of a town with
// no shop open goes to the open food shop nearest it on the map, not the realm's first by its keeper's name. Two bakeries
// in two other towns, the first by name far off. Without the rule it goes to the first by name.
void aFarShopIsTheNearest()
{
    Society::setWorkByHours(true);
    const auto run = [](bool rule) {
        Society::wageRulesOff(rule ? 0 : Society::NearestShop);
        AuthoredRoster roster;
        auto wren = person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian");
        wren.meals = 0;
        auto far = person("abaker", "baker at The Loaf", {"west", 2.5, 2.5});
        auto nearer = person("bbaker", "baker at The Loaf", {"east", 2.5, 2.5});
        far.home = {"west_flat", 2.5, 2.5};
        nearer.home = {"east_flat", 2.5, 2.5};
        roster.residents = {far, nearer, wren};
        roster.economy.treasury = 500;
        Society s(Roster::None);
        s.configure(roster);
        LifeDay day;
        day.communityOf = [](const std::string& cell) {
            return cell.rfind("west", 0) == 0 ? std::string("w") : cell.rfind("east", 0) == 0 ? std::string("e") : std::string("home");
        };
        day.onMap = [](const Spot& at, double& x, double& y) {
            const double base = at.cell.rfind("west", 0) == 0 ? 5000 : at.cell.rfind("east", 0) == 0 ? 200 : 0;
            x = base + at.x;
            y = at.y;
            return true;
        };
        s.setDay(day);
        auto state = s.state();
        state.residents.at("wren").hunger = 70;
        expect(s.restore(state), "hungry wren restores");
        const auto bodies = atWork(s);              // (Wren at home: "homes".)
        for (int i = 0; i < 5; ++i)
            s.tick(1, 10. / 24 + i / 86400., 0, bodies);
        const auto* life = s.resident("wren");
        expect(s.conserved(), "money stays conserved");
        return life ? life->task + " at " + life->goalCell : std::string("nobody");
    };
    const auto nearest = run(true), first = run(false);
    expect(nearest == "buy food at east", "with no shop of its town open, wren goes to the nearest bakery: " + nearest);
    expect(first == "buy food at west", "without the rule, to the first by name: " + first);
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// The kitchens (doc 64, Phase 13, round 13, lane 3: KitchenKept; the user, 2026-10-29). folkTown with its baker hungry
// on duty at hunger `hunger`: nothing on it, no larder, its purse 100p, its till's food only `bread` loaves (none of
// its cakes or meals, and none in the stores to restock its meals from), the general store open with bread of its own;
// the calibrated hours. Hand: a hand at the bakery's bench too, paid by its till.
Society hungryBakery(double hunger, int bread, bool hand = false)
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
    baker.meals = keeper.meals = 0;
    roster.residents = {baker, keeper, wren, ash};
    if (hand)
    {
        auto help = person("hand", "helping out", {"bakery", 3.5, 2.5}, "civilian");
        help.home = {"cottage", 2.5, 2.5};
        help.meals = 0;
        roster.residents.push_back(help);
    }
    roster.economy.treasury = 5000;
    Society s(Roster::None);
    s.configure(roster);
    auto state = s.state();
    state.memory.pace = 1;
    state.residents.at("baker").hunger = hunger;
    auto& shelves = state.accounts.at(s.tillOf("baker")).stock;
    for (auto it = shelves.begin(); it != shelves.end();)
        it = Society::edible(it->first) ? shelves.erase(it) : std::next(it);
    shelves["meal"] = 0;
    if (bread > 0)
        shelves["bread"] = bread;
    state.accounts.at(s.tillOf("keeper")).stock["bread"] = 20;
    state.accounts.at("treasury").stock.clear();    // (No meals in the stores for its counter's wholesale restock.)
    auto& purse = state.accounts.at("baker");
    for (auto it = purse.stock.begin(); it != purse.stock.end();)
        it = Society::edible(it->first) ? purse.stock.erase(it) : std::next(it);
    state.accounts.at("treasury").cash += purse.cash - 100;
    purse.cash = 100;
    expect(s.restore(state), "the hungry bakery restores");
    expect(s.tillOf("baker") != "baker", "the bakery has a till of its own");
    return s;
}

// A hungry baker eats at its own counter (KitchenKept), with work by the hours: on duty at hunger 60, its larder bare,
// its till holding bread and its purse 100p, it stays at its trade and buys a loaf from its own till ("bought at its own
// counter": its purse down by the price, its till up by it, the money conserved), eats it, and its shop is open the
// while (wren, hungry, comes to its counter and buys there). Without the rule it goes to buy food at the general store.
void aHungryBakerEatsAtItsCounter()
{
    Society::setWorkByHours(true);
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::KitchenKept);
        auto s = hungryBakery(60, 40);
        const auto till = s.tillOf("baker");
        auto bodies = atWork(s);                    // (The baker at its counter, the keeper at its store.)
        double day = 10. / 24;
        s.tick(1, day, 0, bodies);                  // (The day's turn.)
        const auto purse = s.account("baker")->cash;
        s.takeJournal();
        std::set<std::string> tasks;
        bool ate = false;
        for (int i = 0; i < 120 && !ate; ++i)
        {
            day += 1. / 86400;
            s.tick(1, day, 0, bodies);
            const auto* life = s.resident("baker");
            tasks.insert(life->task);
            ate = life->hunger < 55;
        }
        const auto journal = s.takeJournal();
        expect(s.conserved(), "money stays conserved");
        if (!rule)
        {
            const auto* life = s.resident("baker");
            expect(!ate && life->task == "buy food" && life->goalCell == "store",
                   "without the rule, the hungry baker goes to buy food at the general store: " + life->task + " at " + life->goalCell);
            continue;
        }
        int bought = 0;
        std::int64_t paid = 0;
        for (const auto& e : journal)
            if (e.kind == "bought at its own counter")
            {
                expect(e.from == "baker" && e.to == till && items::baseOf(e.item) == "bread", "bought from its own till");
                bought += e.quantity, paid += e.coins;
            }
        expect(bought == 1 && paid == s.shopPrice("baker", "bread"), "it buys one loaf at its own price: " + std::to_string(bought) +
                                                                           " for " + std::to_string(paid) + "p");
        expect(s.account("baker")->cash == purse - paid, "its purse down by the price");
        expect(ate && tasks.count("trade") && tasks.count("eat") && !tasks.count("buy food"),
               "it eats at its trade, never going off to buy food");
        day += 1. / 86400;
        s.tick(1, day, 0, bodies);
        expect(s.resident("baker")->task == "trade", "and is back at its trade");
        // Its shop open: wren, hungry at its counter, buys there.
        auto state = s.state();
        state.residents.at("wren").hunger = 70;
        expect(s.restore(state), "hungry wren restores");
        bodies["wren"] = {"bakery", 2.5, 2.5};
        bool served = false;
        for (int i = 0; i < 120 && !served; ++i)
        {
            day += 1. / 86400;
            s.tick(1, day, 0, bodies);
            for (const auto& e : s.takeJournal())
                served = served || (e.kind == "resident food purchase" && e.from == "wren" && e.to == till);
        }
        expect(served, "its shop stays open: wren buys at its counter");
        expect(s.conserved(), "money stays conserved");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// A hungry baker bakes first (KitchenKept), with work by the hours: on duty at hunger 60 with its shelf bare, its flour
// and firewood in its till and its work in hand over a batch of bread's hours, it stays at its trade ("Hungry; baking
// first"); a minute on its bread is on the shelf, and it buys a loaf of it at its own counter. At hunger 85 it goes for
// food as before (it can't wait itself to starve); without the rule it does at 60.
void aHungryBakerBakesFirst()
{
    Society::setWorkByHours(true);
    const auto* bread = items::craftsFor("bakery").front();
    expect(bread->id == "bread", "the bakery's first craft is bread");
    const auto run = [&](bool rule, double hunger, bool& baked, bool& bought) {
        Society::wageRulesOff(rule ? 0 : Society::KitchenKept);
        auto s = hungryBakery(hunger, 0);
        const auto till = s.tillOf("baker");
        expect(held(s, "baker", "flour") >= 1 && held(s, "baker", "firewood") >= 1, "its flour and firewood in");
        auto bodies = atWork(s);
        s.setWorkBanked(till, s.workHours(*bread) + .5);
        double day = 10. / 24;
        s.takeJournal();
        for (int i = 0; i < 2; ++i, day += 1. / 86400)
            s.tick(1, day, 0, bodies);              // (The day's turn, and its first decisions.)
        const auto* life = s.resident("baker");
        const std::string first = life->task + ": " + life->reason;
        baked = bought = false;
        for (int i = 0; i < 300 && !bought; ++i)
        {
            day += 1. / 86400;
            s.tick(1, day, 0, bodies);
            for (const auto& e : s.takeJournal())
            {
                baked = baked || (e.kind == "crafted" && items::baseOf(e.item) == "bread" && e.to == till);
                bought = bought || (e.kind == "bought at its own counter" && e.from == "baker");
            }
            if (!rule || hunger >= Society::BakeFirstBelow)
                break;                              // (Only the first decision is asked of these.)
        }
        expect(s.conserved(), "money stays conserved");
        return first;
    };
    bool baked = false, bought = false;
    const auto with = run(true, 60, baked, bought);
    expect(with == "trade: Hungry; baking first", "with the rule, its shelf bare and a batch ready, it bakes first: " + with);
    expect(baked && bought, "its bread on the shelf a minute on, it buys a loaf at its counter");
    const auto late = run(true, 85, baked, bought);
    expect(late.rfind("buy food", 0) == 0, "at hunger 85 it goes for food: " + late);
    const auto without = run(false, 60, baked, bought);
    expect(without.rfind("buy food", 0) == 0, "without the rule, at 60 it goes for food: " + without);
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// The hands bake without the keeper (KitchenKept), with work by the hours: the baker at home, not keeping its shop, its
// bread gone, its flour in, its work in hand full (24 hours). With the rule a batch of bread begins from its hands' hours
// and is on the shelf a minute on, its work in hand down by the batch's hours, none of the keeper's time counted (its
// last time at work unchanged); its hand's next spell is put in hand, not lost. Without, no batch, and the hand's spell
// is lost at the cap.
void theHandsBakeWithoutTheKeeper()
{
    Society::setWorkByHours(true);
    const auto* bread = items::craftsFor("bakery").front();
    const double full = 24;                         // (Society::WorkBankedMost.)
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::KitchenKept);
        auto s = hungryBakery(0, 0, true);
        auto state = s.state();
        state.accounts.at(s.tillOf("baker")).stock["honey_cake"] = 20;   // (Its cakes in: only its bread is low.)
        expect(s.restore(state), "its cakes restore");
        const auto till = s.tillOf("baker");
        expect(s.payerOf("hand", *s.jobOf("hand"), 30).account == till, "the bakery pays its hand");
        auto bodies = atWork(s);
        bodies["baker"] = {"bakers_flat", 2.5, 2.5};   // (At home: its shop shut.)
        s.setWorkBanked(till, full);
        const double lastAtWork = s.lastAtWorkOf("baker");
        double day = 10. / 24;
        const auto lostSoFar = [&] {
            const auto found = s.workBy().find(till);
            return found == s.workBy().end() ? 0. : found->second.second;
        };
        const double lost = lostSoFar();
        s.takeJournal();
        int baked = 0;
        for (int i = 0; i < 120; ++i)
        {
            day += 1. / 86400;
            s.tick(1, day, 0, bodies);
            for (const auto& e : s.takeJournal())
                baked += e.kind == "crafted" && items::baseOf(e.item) == "bread" ? e.quantity : 0;
        }
        expect(s.resident("baker")->task == "trade" && s.resident("baker")->goalCell == "bakery", "the baker is on its way, not at its counter");
        if (rule)
        {
            // (A batch a minute, to its shelf's store of bread: the first done, the next begun.)
            const int batches = baked / bread->out.front().second + (s.batchAtWork("baker") ? 1 : 0);
            expect(baked >= bread->out.front().second && baked % bread->out.front().second == 0,
                   "a batch of bread begun from the hands' hours and finished: " + std::to_string(baked));
            expect(std::abs(s.workBankedOf(till) - (full - batches * s.workHours(*bread))) < 1e-6,
                   "its work in hand down by the batches' hours: " + std::to_string(s.workBankedOf(till)) + " for " + std::to_string(batches));
            expect(s.lastAtWorkOf("baker") == lastAtWork, "none of the keeper's time counted");
        }
        else
            expect(baked == 0 && !s.batchAtWork("baker") && s.workBankedOf(till) >= full - 1e-9, "without the rule, no batch, its work in hand full");
        // The hand's next spell (one in an hour's work).
        for (int i = 0; i < 3600; ++i)
        {
            day += 1. / 86400;
            s.tick(1, day, 0, bodies);
        }
        if (rule)
            expect(lostSoFar() == lost, "its hand's spell put in hand, none lost: " + std::to_string(lostSoFar() - lost));
        else
            expect(lostSoFar() >= lost + 1 - 1e-9, "without the rule, its hand's spell is lost at the cap: " + std::to_string(lostSoFar() - lost));
        expect(s.conserved(), "money stays conserved");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// The feast bought ahead (doc 64, Phase 13, round 13, lane 3: FeastAhead; the user, 2026-10-29), with work by the hours:
// a baker holding 150 loaves against a shelf target of 40 and a general store 70 meals (30 over its 40), and sixty more
// wolves at home (62 in all: the feast's want 3,100). Three days before the festival the stores buy toward five eighths
// of the want: all 110 loaves over the baker's target, no meals, 99 at most a transfer; a week before, an eighth (26
// loaves); holding bread already, toward the rest. At the turn, a baker holding 220 sells its half, 110, in two. Without
// the rule nothing ahead, and at the turn the one transfer of 110 is refused.
void theFeastIsBoughtAhead()
{
    Society::setWorkByHours(true);
    std::int64_t festival = -1;
    for (std::int64_t d = 10; d < 400 && festival < 0; ++d)
        if (calendar::festivalDay(double(d) + .5))
            festival = d;
    for (std::int64_t d = festival - Society::FeastAheadDays; d < festival; ++d)
        expect(!calendar::festivalDay(double(d) + .5), "no festival in the week before");
    // Bread and meals bought for the feast, its transfers and the most in one.
    struct Bought
    {
        int bread = 0, meals = 0, transfers = 0, most = 0;
    };
    const auto run = [&](bool rule, std::int64_t day, int bread, int meals, int stored) {
        Society::wageRulesOff(rule ? 0 : Society::FeastAhead);
        AuthoredRoster roster;
        auto baker = person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5});
        auto keeper = person("keeper", "shopkeeper at the general store", {"store", 2.5, 2.5});
        baker.home = {"bakers_flat", 2.5, 2.5};
        keeper.home = {"store_flat", 2.5, 2.5};
        roster.residents = {baker, keeper};
        for (int i = 0; i < 60; ++i)
        {
            auto w = person("w" + std::to_string(100 + i), "weaving at home", {"homes", 2.5, 2.5}, "civilian");
            w.meals = 0;
            roster.residents.push_back(w);
        }
        roster.economy.treasury = 50000;
        Society s(Roster::None);
        s.configure(roster);
        auto state = s.state();
        state.accounts.at("treasury").stock.clear();
        if (stored > 0)
            state.accounts.at("treasury").stock["bread"] = stored;
        state.accounts.at(s.tillOf("baker")).stock["bread"] = bread;
        state.accounts.at(s.tillOf("keeper")).stock["meal"] = meals;
        expect(s.restore(state), "the feast's town restores");
        expect(s.shelfTarget(s.tillOf("baker"), "bread", Society::ProducerKept) == 40 &&
                   s.shelfTarget(s.tillOf("keeper"), "meal", Society::ProducerKept) == 40,
               "the shelf targets are 40");
        s.takeJournal();
        s.layInTheFeast(day);
        Bought b;
        for (const auto& e : s.takeJournal())
            if (e.kind == "bought for the festival feast")
            {
                b.meals += items::baseOf(e.item) == "meal" ? e.quantity : 0;
                if (items::baseOf(e.item) == "bread")
                    b.bread += e.quantity, ++b.transfers;
                b.most = std::max(b.most, e.quantity);
            }
        expect(s.conserved(), "money stays conserved");
        return b;
    };
    // Three days ahead: 3,100 x 5/8 = 1,937, more than the 110 loaves over target (1,650).
    const auto three = run(true, festival - 3, 150, 70, 0);
    expect(three.bread == 110 && three.meals == 0 && three.most <= 99 && three.transfers == 2,
           "three days ahead the stores buy the 110 loaves over target, no meals, in 99s: " + std::to_string(three.bread) + " in " +
               std::to_string(three.transfers) + ", " + std::to_string(three.meals) + " meals");
    const auto week = run(true, festival - 7, 150, 70, 0);
    expect(week.bread == 26 && week.meals == 0, "a week ahead, an eighth of the want (387): " + std::to_string(week.bread) + " loaves");
    const auto held = run(true, festival - 3, 150, 70, 100);
    expect(held.bread == 30, "holding 100 loaves, toward the rest (437): " + std::to_string(held.bread) + " loaves");
    const auto turn = run(true, festival, 220, 0, 0);
    expect(turn.bread == 110 && turn.transfers == 2 && turn.most <= 99,
           "at the turn, a baker holding 220 sells its half in two: " + std::to_string(turn.bread) + " in " + std::to_string(turn.transfers));
    const auto before = run(false, festival - 3, 150, 70, 0);
    expect(before.bread == 0 && before.meals == 0, "without the rule nothing is bought ahead");
    const auto refused = run(false, festival, 220, 0, 0);
    expect(refused.bread == 0, "and at the turn the one transfer of 110 is refused: " + std::to_string(refused.bread));
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// Markdowns (doc 64, Phase 13, round 11, lane 3; the user, 2026-10-29: "Shops should mark down what sits over their
// shelf target for more than a day."), with work by the hours: the baker's bread piled over its shelf target keeps its
// price its first day over, then comes down a step (MarkdownStep) a day, never below MarkdownFloor of its normal cost;
// its stock back under the target, the price climbs back a step a day. Without the rule, it keeps its price.
void aGlutIsMarkedDown()
{
    Society::setWorkByHours(true);
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::Markdowns);
        auto s = town();
        auto state = s.state();
        state.memory.pace = 1;
        expect(s.restore(state), "the calibrated hours restore");
        expect(s.shift("treasury", "scribe", "", 0, 200, "test: the buyer's money"), "the buyer has money");
        set(s, "baker", "bread", 10);
        expect(s.sale(s.tillOf("baker"), "scribe", "bread", 2, 1, "test: the day's buying"), "the scribe buys bread");
        s.spoil(1);
        const auto till = s.tillOf("baker");
        const double before = s.priceAt("baker", "bread"), cost = s.normalCost("baker", "bread");
        const int target = s.shelfTarget(till, "bread", Society::ProducerKept);
        set(s, "baker", "bread", target + 50);
        s.reviewMarkdowns();
        expect(std::abs(s.priceAt("baker", "bread") - before) < 1e-9, "a day over its target, the bread keeps its price");
        s.reviewMarkdowns();
        const double second = s.priceAt("baker", "bread");
        const std::string at = rule ? "with the rule" : "without the rule";
        expect(rule ? std::abs(second - before * (1 - Society::MarkdownStep)) < 1e-9 : std::abs(second - before) < 1e-9,
               at + ", the second day over: " + std::to_string(second) + "p (from " + std::to_string(before) + "p)");
        for (int d = 0; d < 40; ++d)
            s.reviewMarkdowns();
        const double floor = s.priceAt("baker", "bread");
        expect(rule ? floor >= cost * Society::MarkdownFloor - 1e-9 && floor < second : std::abs(floor - before) < 1e-9,
               at + ", weeks over: " + std::to_string(floor) + "p, the floor " + std::to_string(cost * Society::MarkdownFloor) + "p");
        set(s, "baker", "bread", 0);
        s.reviewMarkdowns();
        expect(!rule || s.priceAt("baker", "bread") > floor, at + ", under its target again, the price climbs back a step");
        for (int d = 0; d < 40; ++d)
            s.reviewMarkdowns();
        expect(std::abs(s.priceAt("baker", "bread") - before) < 1e-9, at + ", and in time is its own again");
        // (Back from the save: three days over, two steps down.)
        auto saved = s.state();
        saved.memory.markdowns[till + "|bread"] = 3;
        expect(s.restore(saved), "the markdowns restore");
        expect(std::abs(s.priceAt("baker", "bread") - before * (1 - 2 * Society::MarkdownStep)) < 1e-9, at + ", a markdown comes back from the save");
        expect(s.conserved(), "money stays conserved");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// Round 14, lane 3 (the user, 2026-10-30), with work by the hours at the calibrated pace (a fishing yield, two fish, every
// .971 hours): a fishery, its fisher's till, with a day's sales of 200 fish behind it, so its store's kept target
// (shelfTarget) is over the 74 fish of 36 hours' yields; its store then `fish`, its work in hand `banked` hours. Its keeper
// away: nothing ticks after it is made.
Society fishery(int fish, double banked)
{
    AuthoredRoster roster;
    roster.residents = {person("fisher", "fishes the lake", {"lake", 4.5, 4.5}, "civilian"),
                        person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian")};
    roster.economy.treasury = 5000;
    Society s(Roster::None);
    s.configure(roster);
    double day = 10. / 24;
    work(s, 60, day);                               // (Its till and the purses founded.)
    auto state = s.state();
    state.memory.pace = 1;
    expect(s.restore(state), "the calibrated hours restore");
    const auto till = s.tillOf("fisher");
    expect(till != "fisher", "the fishery has a till of its own");
    set(s, "fisher", "fish", 200);
    for (const int n : {99, 99, 2})
        expect(s.sale(till, "treasury", "fish", n, 1, "test: a day's sales"), "a day's sales of fish");
    s.spoil(0);                                     // (Into its running rate of sales: its store's target.)
    set(s, "fisher", "fish", fish);
    s.setWorkBanked("fisher", banked);     // (A producer's work in hand is its keeper's: produce, creditWork.)
    return s;
}

// A hand's day brings in at once (HandsBringIn; the user, 2026-10-30): the fishery's work in hand 20 hours, its store
// under its target, its keeper away, and two hired hands' days (8 hours each) done in the same second. With the rule the
// store gains the yields of the 36 hours (37 yields, 74 fish, under its target), its work in hand is left under a yield's
// hours, and none is lost. Without, 12 of the 16 hours are lost at the bank's cap of 24, and nothing comes in until its
// keeper's own spell.
void aHandsDayBringsInAtOnce()
{
    Society::setWorkByHours(true);
    const auto* fishing = items::producerFor("fishes the lake");
    expect(fishing && fishing->out.size() == 1 && fishing->out.front().second == 2, "a fishing yield is two fish");
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::HandsBringIn);
        auto s = fishery(0, 20);
        const auto till = s.tillOf("fisher");
        const double yield = s.workHours(*fishing);
        const int yields = int(std::floor(36 / yield + 1e-9));
        expect(s.shelfTarget(till, "fish", Society::ProducerKept) >= yields * 2, "its store's target is over the 36 hours' fish: " +
                                                                                    std::to_string(s.shelfTarget(till, "fish", Society::ProducerKept)));
        const auto lost = [&] {
            const auto found = s.workBy().find("fisher");
            return found == s.workBy().end() ? 0. : found->second.second;
        };
        const double lostBefore = lost();
        s.takeJournal();
        const double day = 11. / 24;
        s.handsDayOnTheLand("fisher", Society::HireSpells, day);
        s.handsDayOnTheLand("fisher", Society::HireSpells, day);
        const int fish = count(s.takeJournal(), "brought in", "fish");
        if (rule)
        {
            expect(fish == yields * 2 && held(s, "fisher", "fish") == yields * 2,
                   "with the rule the store gains the 36 hours' yields: " + std::to_string(fish) + " of " + std::to_string(yields * 2));
            expect(s.workBankedOf("fisher") < yield && std::abs(s.workBankedOf("fisher") - (36 - yields * yield)) < 1e-6,
                   "its work in hand under a yield's hours: " + std::to_string(s.workBankedOf("fisher")));
            expect(lost() == lostBefore, "and none of it lost: " + std::to_string(lost() - lostBefore));
        }
        else
        {
            expect(fish == 0 && held(s, "fisher", "fish") == 0, "without the rule nothing comes in: " + std::to_string(fish));
            expect(std::abs(lost() - lostBefore - 12) < 1e-9 && std::abs(s.workBankedOf("fisher") - 24.) < 1e-9,
                   "and 12 of the 16 hours are lost at the cap: " + std::to_string(lost() - lostBefore));
        }
        expect(s.conserved(), "money stays conserved");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// A full store still loses work (HandsBringIn): the same fishery with its store at its target. With the rule as without,
// the hands' hours wait in its work in hand, 12 of the 16 lost past its cap, and nothing comes in: work not wanted is lost.
void aFullStoreStillLosesWork()
{
    Society::setWorkByHours(true);
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::HandsBringIn);
        auto probe = fishery(0, 0);
        const int target = probe.shelfTarget(probe.tillOf("fisher"), "fish", Society::ProducerKept);
        auto s = fishery(target, 20);
        expect(s.shelfTarget(s.tillOf("fisher"), "fish", Society::ProducerKept) == target, "its store at its target");
        const auto lost = [&] {
            const auto found = s.workBy().find("fisher");
            return found == s.workBy().end() ? 0. : found->second.second;
        };
        const double lostBefore = lost();
        s.takeJournal();
        const double day = 11. / 24;
        s.handsDayOnTheLand("fisher", Society::HireSpells, day);
        s.handsDayOnTheLand("fisher", Society::HireSpells, day);
        const std::string at = rule ? "with the rule" : "without the rule";
        expect(count(s.takeJournal(), "brought in", "fish") == 0 && held(s, "fisher", "fish") == target, at + ", nothing comes in");
        expect(std::abs(lost() - lostBefore - 12) < 1e-9 && std::abs(s.workBankedOf("fisher") - 24.) < 1e-9,
               at + ", the hours wait and 12 are lost past the cap: " + std::to_string(lost() - lostBefore));
        expect(s.conserved(), "money stays conserved");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// A dead keeper's hand makes nothing (HandsBringIn): the fisher dies, its post let go (its till its purse no more), and a
// hand's day it had hired finishes. With the rule as without, nothing is brought in to the deceased's purse or the till.
void aDeadKeepersHandMakesNothing()
{
    Society::setWorkByHours(true);
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::HandsBringIn);
        auto s = fishery(0, 20);
        const auto till = s.tillOf("fisher");
        s.died("fisher", 11. / 24);
        expect(s.tillOf("fisher") == "fisher" && !s.jobOf("fisher"), "the dead fisher holds its post no more");
        s.takeJournal();
        s.handsDayOnTheLand("fisher", Society::HireSpells, 11. / 24);
        const std::string at = rule ? "with the rule" : "without the rule";
        expect(count(s.takeJournal(), "brought in", "fish") == 0 && Society::stockAll(*s.account("fisher"), "fish") == 0 &&
                   Society::stockAll(*s.account(till), "fish") == 0,
               at + ", the hand's day brings nothing in to the deceased's purse");
        expect(s.conserved(), "money stays conserved");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// A successor sells on (TradeByPost, round 14, lane 3; the user, 2026-10-30), with work by the hours at the calibrated
// pace. The smelter: its founder dies with 148 iron bars in its till, and one of its hands, written as a civilian who
// 'works at The Frosthollow Smelter', takes its post (as a succession does: its own post let go); the forge in town is
// out of iron. With the rule the next day's sellers of iron bars are the smelter's holder's, the smith buys bars from
// the smelter's till, and with its shelf under its store (as if the roads took the rest) its holder starts a batch of
// bars. The fishery: its founder dies too, and kit, written as born here ('-'), takes its post: it brings fish in at its
// post's spells. Without the rule the sellers are the dead founder's (its purse empty), the smith buys none, the
// smelter's shelf stays over its store and it makes nothing, and kit brings in nothing. (The marketOf half is
// roads_tests' aSuccessorsBarsAreOnTheMarket.)
void aSuccessorSellsOn()
{
    Society::setWorkByHours(true);
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::TradeByPost);
        AuthoredRoster roster;
        roster.residents = {person("thistle", "runs The Frosthollow Smelter", {"smelter", 2.5, 2.5}),
                            person("brisk", "works at The Frosthollow Smelter", {"smelter", 5.5, 2.5}, "civilian"),
                            person("smith", "the smith at the forge", {"forge", 2.5, 2.5}),
                            person("fisher", "fishes the lake", {"lake", 4.5, 4.5}, "civilian"),
                            person("kit", "-", {"homes", 5.5, 2.5}, "civilian"),
                            person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian")};
        roster.economy.treasury = 5000;
        Society s(Roster::None);
        s.configure(roster);
        double day = 10. / 24;
        work(s, 60, day);                           // (The tills and the purses founded.)
        const auto smelter = s.tillOf("thistle"), forge = s.tillOf("smith"), boats = s.tillOf("fisher");
        expect(smelter != "thistle" && forge != "smith" && boats != "fisher", "the smelter, the forge and the fishery have tills");
        expect(Society::stockAll(*s.account(smelter), "iron_ore") >= 2 && Society::stockAll(*s.account(smelter), "charcoal") >= 1,
               "the smelter holds ore and charcoal");
        s.died("thistle", day);
        s.died("fisher", day);
        auto st = s.state();
        st.memory.pace = 1;
        auto& smelterPost = st.careers.positions.at("job:thistle");
        smelterPost.holder = "brisk", smelterPost.vacantSince = -1;
        st.careers.positions.at("job:brisk").holder.clear();
        auto& boatsPost = st.careers.positions.at("job:fisher");
        boatsPost.holder = "kit", boatsPost.vacantSince = -1;
        st.accounts.at(smelter).stock["iron_bar"] = 148;
        auto& shelves = st.accounts.at(forge).stock;    // (The forge out of iron and of nails; its swords, made first, in.)
        for (auto it = shelves.begin(); it != shelves.end();)
            it = items::baseOf(it->first) == "iron_bar" || items::baseOf(it->first) == "nails" ? shelves.erase(it) : std::next(it);
        shelves["charcoal"] = 20, shelves["sword"] = 40;
        st.accounts.at("treasury").cash -= 2000;       // (And the money to buy them: its takings.)
        st.accounts.at(forge).cash += 2000;
        st.accounts.at(boats).stock.erase("fish");
        expect(s.restore(st), "the successions restore");
        s.setWorkBanked("kit", 0);
        expect(s.jobOf("brisk") && s.jobOf("brisk")->id == "job:thistle" && s.tillOf("brisk") == smelter, "brisk holds the smelter");
        expect(s.tillOf("kit") == boats, "kit holds the fishery");
        const std::string at = rule ? "with the rule" : "without the rule";
        expect(s.tradeLabel("brisk") == (rule ? "runs The Frosthollow Smelter" : "works at The Frosthollow Smelter") &&
                   s.keepsShop("brisk") == rule && s.tradeLabel("kit") == (rule ? "fishes the lake" : "-"),
               at + ", the successors' trades");
        auto bodies = atWork(s);
        bodies.erase("thistle");
        bodies.erase("fisher");
        bodies["brisk"] = {"smelter", 2.5, 2.5};    // (At its post's counter.)
        bodies["kit"] = {"lake", 4.5, 4.5};         // (At its post's water.)
        // The next day's pass, and a morning's work.
        day = 1 + 10. / 24;
        s.tick(1, day, 0, bodies);
        const auto sellers = s.sellersListed("iron_bar");
        const auto lists = [&](const std::string& id) {
            return std::any_of(sellers.begin(), sellers.end(), [&](const auto& seller) { return seller.first == id && seller.second == 1; });
        };
        expect(rule ? lists("brisk") && !lists("thistle") : lists("thistle") && !lists("brisk"),
               at + ", the smelter's bars are sold by " + (rule ? "its holder" : "its dead founder"));
        s.takeJournal();
        int bars = 0, fish = 0;
        for (int i = 0; i < 3700; ++i)
        {
            day += 1. / 86400;
            s.tick(1, day, 0, bodies);
            for (const auto& e : s.takeJournal())
            {
                if (e.kind == "materials bought" && e.from == forge && e.to == smelter && items::baseOf(e.item) == "iron_bar")
                    bars += e.quantity;
                if (e.kind == "brought in" && e.to == boats && items::baseOf(e.item) == "fish")
                    fish += e.quantity;
            }
        }
        expect(rule ? bars > 0 : bars == 0, at + ", the smith buys the smelter's bars: " + std::to_string(bars));
        expect(rule ? fish >= 2 : fish == 0, at + ", the fishery's successor brings in fish: " + std::to_string(fish));
        // The smelter's shelf under its store (with the rule, as if the roads took what the smith left), its work in hand
        // full: a batch of bars begins.
        st = s.state();
        if (rule)
            st.accounts.at(smelter).stock["iron_bar"] = 0;
        expect(s.restore(st), "the smelter's shelf restores");
        s.setWorkBanked(smelter, 24);
        s.takeJournal();
        int made = 0;
        for (int i = 0; i < 3700; ++i)
        {
            day += 1. / 86400;
            s.tick(1, day, 0, bodies);
            for (const auto& e : s.takeJournal())
                if (e.kind == "crafted" && e.to == smelter && items::baseOf(e.item) == "iron_bar")
                    made += e.quantity;
        }
        expect(rule ? made > 0 : made == 0, at + ", the smelter " + (rule ? "makes bars again" : "with its shelf full makes none") +
                                                ": " + std::to_string(made));
        expect(s.conserved(), "money stays conserved");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// The lists are the same with no succession (TradeByPost): every founder alive in its post, the sellers of every good
// (sellersOf, as buyMaterials reads them) with the rule are those without it, in the same order, and its sellers are
// the written residents. (The markets' half is roads_tests' theMarketIsTheSameWithNoSuccession.)
void theListsAreTheSameWithNoSuccession()
{
    Society::setWorkByHours(true);
    AuthoredRoster roster;
    auto farmer = person("farmer", "farms the valley fields", {"fields", 4.5, 4.5}, "civilian");
    roster.residents = {farmer,
                        person("fisher", "fishes the lake", {"lake", 4.5, 4.5}, "civilian"),
                        person("miller", "runs The Ford Mill", {"mill", 2.5, 2.5}),
                        person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}),
                        person("stall", "keeping the stall", {"market", 4.5, 4.5}),
                        person("smith", "the smith at the forge", {"forge", 2.5, 2.5}),
                        person("thistle", "runs The Frosthollow Smelter", {"smelter", 2.5, 2.5}),
                        person("brisk", "works at The Frosthollow Smelter", {"smelter", 5.5, 2.5}, "civilian"),
                        person("chandler", "the chandler at the kettle", {"kettle", 2.5, 2.5}),
                        person("keeper", "shopkeeper at the general store", {"store", 2.5, 2.5}),
                        person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian")};
    roster.economy.treasury = 5000;
    Society s(Roster::None);
    s.configure(roster);
    double day = 10. / 24;
    work(s, 60, day);
    std::vector<std::string> written;
    for (const auto& r : s.authored().residents)
        written.push_back(r.id);
    int goods = 0, listed = 0;
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::TradeByPost);
        expect(s.traders() == written, std::string(rule ? "with" : "without") + " the rule, the sellers are the written residents");
    }
    for (const auto& good : items::allGoods())
    {
        Society::wageRulesOff(0);
        const auto with = s.sellersListed(good.id);
        Society::wageRulesOff(Society::TradeByPost);
        const auto without = s.sellersListed(good.id);
        expect(with == without, "the sellers of " + good.id + " are the same, in the same order");
        ++goods, listed += int(with.size());
    }
    expect(goods > 100 && listed > 20, "goods asked: " + std::to_string(goods) + ", sellers listed: " + std::to_string(listed));
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// Round 14, lane 2 (the user, 2026-10-30: ShopsCompared, FestivalShopsOpen), with work by the hours. A food counter of
// the shopping tests: its keeper, its work, where it serves, the one food it sells (40 of them), and that food's margin
// (0: the ladder's).
struct Counter
{
    std::string id, work;
    Spot at;
    std::string food;
    double margin = 0;
};
// A town of food counters (one community, "town"; on the map, each cell `along` tiles east: the square at 0) and wren,
// hungry (70), carrying nothing, holding `purse`; the stores hold no meals for a counter's wholesale restock, and each
// counter only its own food. Ash, a second weaver, at home.
Society shoppingTown(const std::vector<Counter>& counters, std::int64_t purse, bool mapped = false)
{
    AuthoredRoster roster;
    auto wren = person("wren", "weaving at home", {"homes", 3.5, 2.5}, "civilian");
    auto ash = person("ash", "weaving at home", {"homes", 4.5, 2.5}, "civilian");
    wren.meals = ash.meals = 0;
    for (const auto& c : counters)
    {
        auto keeper = person(c.id, c.work, c.at);
        keeper.home = {c.id + "_flat", 2.5, 2.5};
        keeper.meals = 0;
        roster.residents.push_back(keeper);
    }
    roster.residents.push_back(wren);
    roster.residents.push_back(ash);
    roster.economy.treasury = 5000;
    Society s(Roster::None);
    s.configure(roster);
    LifeDay day;
    day.communityOf = [](const std::string&) { return std::string("town"); };
    if (mapped)
        day.onMap = [](const Spot& at, double& x, double& y) {
            static const std::map<std::string, double> along = {{"square", 0}, {"a", 10}, {"b", 15}, {"c", 20}, {"lane", 30}};
            const auto found = along.find(at.cell);
            x = (found == along.end() ? 200 : found->second) + at.x;
            y = at.y;
            return true;
        };
    s.setDay(day);
    auto state = s.state();
    state.memory.pace = 1;
    state.memory.going[""] = 60;                    // (The realm's going rate as the runs': a loaf some 5p.)
    state.residents.at("wren").hunger = 70;
    state.accounts.at("treasury").stock.clear();
    for (const auto& c : counters)
    {
        auto& shelves = state.accounts.at(s.tillOf(c.id)).stock;
        for (auto it = shelves.begin(); it != shelves.end();)
            it = Society::edible(it->first) ? shelves.erase(it) : std::next(it);
        shelves[c.food] = 40;
        if (c.margin > 0)
            state.memory.margins[s.tillOf(c.id) + "|" + c.food] = c.margin;
    }
    auto& w = state.accounts.at("wren");
    for (auto it = w.stock.begin(); it != w.stock.end();)
        it = Society::edible(it->first) ? w.stock.erase(it) : std::next(it);
    state.accounts.at("treasury").cash += w.cash - purse;
    w.cash = purse;
    expect(s.restore(state), "the shopping town restores");
    return s;
}

// Where wren, at `at`, goes for food: its task and the counter it goes to (its goal's cell and x), after the keepers
// have opened their counters (a second at 10:00). With `purse` (0 or more), its purse that after the day's turn (its
// relief paid).
std::string wrenGoes(Society& s, Spot at, std::int64_t purse = -1)
{
    auto bodies = atWork(s);
    bodies["wren"] = {at.cell, at.x, at.y};
    s.tick(1, 10. / 24, 0, bodies);                 // (The keepers to their trade.)
    if (const auto cash = s.account("wren")->cash; purse >= 0 && cash != purse)
        expect(cash > purse ? s.shift("wren", "treasury", "", 0, cash - purse, "test: wren's purse")
                            : s.shift("treasury", "wren", "", 0, purse - cash, "test: wren's purse"),
               "wren's purse set");
    s.tick(1, 10. / 24 + 1. / 86400, 0, bodies);    // (Their counters open: wren decides.)
    expect(s.conserved(), "money stays conserved");
    const auto* life = s.resident("wren");
    std::ostringstream said;
    said << life->task << " at " << life->goalCell << " " << life->goalX;
    return said.str();
}

// The cheaper shop wins (ShopsCompared): a bakery and a smokehouse in wren's cell, the smokehouse's counter nearer, the
// smoked fish at a margin of 3 (some five times the bread's pennies for a unit of nourishment), wren hungry with 100p:
// with the rule it goes to the bakery, without to the smokehouse, the nearest. A penny short of the bread (the cheapest),
// it asks alms at the smokehouse, as before. The bakery across the town (30 tiles off, the smokehouse in its cell): with
// the rule it walks to it; but with three smokehouses of its town nearer than the bakery (ShopsLooked), it doesn't look
// so far, and buys at its own cell's.
void theCheaperShopWins()
{
    Society::setWorkByHours(true);
    const Counter bakery{"baker", "baker at The Loaf", {"square", 2.5, 2.5}, "bread"};
    const Counter smokehouse{"smoker", "runs the smokehouse", {"square", 4.5, 2.5}, "smoked_fish", 3};
    const Spot wrenAt{"square", 5.0, 2.5};
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::ShopsCompared);
        const std::string with = rule ? "with the rule" : "without the rule";
        {
            auto s = shoppingTown({bakery, smokehouse}, 100);
            const auto bread = s.shopPrice("baker", "bread"), fish = s.shopPrice("smoker", "smoked_fish");
            const double breadValue = double(bread) / Society::nourishment("bread"), fishValue = double(fish) / Society::nourishment("smoked_fish");
            expect(fishValue > breadValue * (1 + Society::ValueTie) * 3 && fish < 100,
                   "the smoked fish dear: " + std::to_string(fish) + "p for " + std::to_string(Society::nourishment("smoked_fish")) +
                       " against the bread's " + std::to_string(bread) + "p for " + std::to_string(Society::nourishment("bread")));
            const auto went = wrenGoes(s, wrenAt);
            expect(went == (rule ? "buy food at square 2.5" : "buy food at square 4.5"), with + ", with 100p wren goes: " + went);
            if (rule)
            {
                std::cout << "  the shops compared: bread " << bread << "p (" << breadValue << "p a unit of nourishment), smoked fish " << fish
                          << "p (" << fishValue << "p)\n";
                const auto v = s.shopOffer("smoker");
                expect(std::abs(v.value - fishValue) < 1e-9 && v.cheapest == fish, "the smokehouse valued at its price");
                // (What's on its shelf is now: its smoked fish gone, it offers nothing; back, it offers it again.)
                set(s, "smoker", "smoked_fish", 0);
                expect(s.shopOffer("smoker").cheapest == 0, "its fish gone, the smokehouse offers nothing");
                set(s, "smoker", "smoked_fish", 5);
                expect(s.shopOffer("smoker").cheapest == fish, "its fish back, at the hour's price");
            }
            else
                expect(!s.shopValue("smoker") && !s.shopValue("baker"), "without the rule no shop is valued");
        }
        {
            auto probe = shoppingTown({bakery, smokehouse}, 100);
            const auto bread = probe.shopPrice("baker", "bread");
            auto s = shoppingTown({bakery, smokehouse}, bread - 1);
            const auto went = wrenGoes(s, wrenAt, bread - 1);
            expect(went == "seeking alms at square 4.5", with + ", a penny short of the bread (" + std::to_string(bread) + "p), wren asks alms: " + went);
        }
        {
            auto far = bakery;
            far.at = {"lane", 2.5, 2.5};
            auto s = shoppingTown({far, smokehouse}, 100, true);
            const auto went = wrenGoes(s, wrenAt);
            expect(went == (rule ? "buy food at lane 2.5" : "buy food at square 4.5"), with + ", the bakery across the town: " + went);
        }
        {
            auto far = bakery;
            far.at = {"lane", 2.5, 2.5};
            std::vector<Counter> counters = {far, smokehouse};
            for (const auto* cell : {"a", "b", "c"})
                counters.push_back({std::string("smoker_") + cell, "runs the smokehouse", {cell, 2.5, 2.5}, "smoked_fish", 3});
            auto s = shoppingTown(counters, 100, true);
            const auto went = wrenGoes(s, wrenAt);
            expect(went == "buy food at square 4.5", with + ", three smokehouses nearer than the bakery: " + went);
        }
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// Near enough is nearer (ShopsCompared, ValueTie): two bakeries in wren's cell, the farther (abaker, first by name) its
// bread a few pennies cheaper, within a tenth: wren goes to the nearer. A fifth cheaper, to the farther. Without the
// rule, to the nearer both times.
void nearEnoughIsNearer()
{
    Society::setWorkByHours(true);
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::ShopsCompared);
        const std::string with = rule ? "with the rule" : "without the rule";
        const auto run = [&](double farMargin, double& tie) {
            auto s = shoppingTown({{"abaker", "baker at The Loaf", {"square", 2.5, 2.5}, "bread", farMargin},
                                   {"bbaker", "baker at The Crust", {"square", 4.5, 2.5}, "bread", 6}},
                                  200);
            tie = double(s.shopPrice("bbaker", "bread")) / double(s.shopPrice("abaker", "bread"));
            return wrenGoes(s, {"square", 5.0, 2.5});
        };
        double tie = 0, wide = 0;
        const auto near = run(5.6, tie);
        expect(tie > 1 && tie <= 1 + Society::ValueTie, "the nearer's bread dearer, within a tenth: " + std::to_string(tie));
        expect(near == "buy food at square 4.5", with + ", within a tenth wren goes to the nearer: " + near);
        const auto far = run(4.6, wide);
        if (rule)
            std::cout << "  near enough: the nearer's bread dearer by " << tie << " (it wins), by " << wide << " (the farther wins)\n";
        tie = wide;
        expect(tie > 1 + Society::ValueTie, "the nearer's bread dearer by more than a tenth: " + std::to_string(tie));
        expect(far == (rule ? "buy food at square 2.5" : "buy food at square 4.5"), with + ", a fifth cheaper: " + far);
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// The larder lays in where it's cheap (ShopsCompared): wren and ash's larder bare, wren with 500p, two bakeries in town;
// at the day's turn the household lays in. Without the rule it starts at a shop of its own (by its home); its bread made
// dear (a margin of 3), with the rule it starts at the other, and without, still at its own; the two alike, with the rule
// at its own.
void theLarderLaysInWhereItsCheap()
{
    Society::setWorkByHours(true);
    const auto first = [](bool rule, const std::string& dear) {
        Society::wageRulesOff(rule ? 0 : Society::ShopsCompared);
        std::vector<Counter> counters = {{"abaker", "baker at The Loaf", {"square", 2.5, 2.5}, "bread"},
                                         {"bbaker", "baker at The Crust", {"lane", 2.5, 2.5}, "bread"}};
        for (auto& c : counters)
            c.margin = c.id == dear ? 3 : 0;
        auto s = shoppingTown(counters, 500);
        const auto larder = Society::homeStore("homes", "larder");
        expect(s.openAccount(larder), "the household's larder");
        auto state = s.state();
        state.residents.at("wren").hunger = 0;      // (Laying in, not hungry.)
        expect(s.restore(state), "wren fed restores");
        auto bodies = atWork(s);
        s.tick(1, 10. / 24, 0, bodies);
        s.takeJournal();
        s.tick(1, 1 + 10. / 24, 0, bodies);         // (The day's turn: the households' errands, householdShopping.)
        std::string till;
        int bought = 0;
        for (const auto& e : s.takeJournal())
            if (e.kind == "resident food purchase" && (e.from == "wren" || e.from == "ash"))
            {
                if (till.empty())
                    till = e.to;
                bought += e.quantity;
            }
        expect(bought > 0, "the household lays in");
        expect(s.conserved(), "money stays conserved");
        return till == s.tillOf("abaker") ? std::string("abaker") : till == s.tillOf("bbaker") ? std::string("bbaker") : till;
    };
    const auto own = first(false, "");
    expect(own == "abaker" || own == "bbaker", "the household's own start: " + own);
    const auto other = own == "abaker" ? "bbaker" : "abaker";
    const auto cheap = first(true, own), before = first(false, own);
    expect(cheap == other, "with the rule, its own start's bread dear, it lays in at the other first: " + cheap);
    expect(before == own, "without the rule, at its own start still: " + before);
    const auto same = first(true, "");
    expect(same == own, "with the rule and the two alike, at its own start: " + same);
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// The values are the hour's (ShopsCompared): the bakery's bread made dear at 10:30, its value stays the hour's till the
// hour turns, and wren still goes there; the values are worked out on the main thread, never a deciding thread's, with
// 70 wolves deciding over two threads.
void theValueIsHourly()
{
    Society::setWorkByHours(true);
    Society::wageRulesOff(0);
    AuthoredRoster roster;
    auto baker = person("baker", "baker at The Loaf", {"square", 2.5, 2.5});
    auto oven = person("oven", "baker at The Crust", {"square", 4.5, 2.5});
    baker.home = {"bakers_flat", 2.5, 2.5};
    oven.home = {"ovens_flat", 2.5, 2.5};
    roster.residents = {baker, oven};
    for (int i = 0; i < 70; ++i)
    {
        auto w = person("w" + std::to_string(100 + i), "weaving at home", {"homes", 2.5, 2.5}, "civilian");
        w.meals = 0;
        roster.residents.push_back(w);
    }
    roster.economy.treasury = 50000;
    Society s(Roster::None);
    s.configure(roster);
    LifeDay day;
    day.communityOf = [](const std::string&) { return std::string("town"); };
    s.setDay(day);
    std::set<std::thread::id> deciders;
    std::size_t jobs = 0;
    std::mutex seen;
    s.setParallel([&](std::size_t count, const std::function<void(std::size_t)>& job) {
        std::vector<std::thread> threads;
        for (std::size_t t = 0; t < 2; ++t)
            threads.emplace_back([&, t] {
                for (std::size_t i = t; i < count; i += 2)
                {
                    job(i);
                    const std::lock_guard<std::mutex> lock(seen);
                    deciders.insert(std::this_thread::get_id());
                    ++jobs;
                }
            });
        for (auto& t : threads)
            t.join();
    });
    {
        auto state = s.state();
        state.accounts.at("treasury").stock.clear();
        for (const auto* keeper : {"baker", "oven"})
        {
            auto& shelves = state.accounts.at(s.tillOf(keeper)).stock;
            for (auto it = shelves.begin(); it != shelves.end();)
                it = Society::edible(it->first) ? shelves.erase(it) : std::next(it);
            shelves["bread"] = 90;
        }
        for (auto& [id, life] : state.residents)
            if (id.rfind("w1", 0) == 0)
            {
                life.hunger = 70;
                auto& w = state.accounts.at(id);
                for (auto it = w.stock.begin(); it != w.stock.end();)
                    it = Society::edible(it->first) ? w.stock.erase(it) : std::next(it);
            }
        expect(s.restore(state), "the hungry town restores");
    }
    auto bodies = atWork(s);
    bodies["w100"] = {"square", 5.0, 2.5};          // (Nearer the Crust.)
    double at = 10. / 24;
    s.tick(1, at, 0, bodies);
    at += 1. / 86400;
    s.tick(1, at, 0, bodies);
    expect(s.shopValue("oven") && s.shopOffer("oven").value > 0, "the Crust valued as it opens");
    const double before = s.shopOffer("oven").value;
    const auto hours = s.shopValueHours();
    expect(s.shopValuedBy() == std::this_thread::get_id(), "valued on the main thread");
    expect(jobs > 0 && !deciders.empty() && !deciders.count(std::this_thread::get_id()), "the wolves decided on two other threads");
    expect(s.resident("w100")->task == "buy food" && s.resident("w100")->goalX == 4.5, "w100 goes to the Crust, the nearer of two alike");
    // At 10:30 the Crust's bread dear: its price is, its value isn't till 11:00.
    auto state = s.state();
    state.memory.margins[s.tillOf("oven") + "|bread"] = 3;
    expect(s.restore(state), "the dear bread restores");
    const double dear = double(s.shopPrice("oven", "bread")) / Society::nourishment("bread");
    expect(dear > before * (1 + Society::ValueTie), "the Crust's bread dearer by more than a tenth");
    at = 10.5 / 24;
    for (int i = 0; i < 3; ++i, at += 1. / 86400)
        s.tick(1, at, 0, bodies);
    expect(std::abs(s.shopOffer("oven").value - before) < 1e-9 && s.shopValueHours() == hours, "mid-hour the Crust's value is the hour's");
    expect(s.resident("w100")->goalX == 4.5, "and w100 still goes to it");
    at = 11. / 24 + 1. / 86400;
    for (int i = 0; i < 3; ++i, at += 1. / 86400)
        s.tick(1, at, 0, bodies);
    expect(s.shopValueHours() == hours + 1 && std::abs(s.shopOffer("oven").value - dear) < 1e-9,
           "at 11:00 the Crust is valued afresh: " + std::to_string(s.shopOffer("oven").value) + " (" + std::to_string(dear) + ")");
    expect(s.resident("w100")->task == "buy food" && s.resident("w100")->goalX == 2.5, "and w100 goes to the Loaf: " + s.resident("w100")->task);
    expect(s.shopValuedBy() == std::this_thread::get_id(), "still on the main thread");
    expect(s.conserved(), "money stays conserved");
    s.setParallel({});
    Society::setWorkByHours(false);
}

// A food shop opens at the festival (FestivalShopsOpen): a festival's day at 14:00, the baker on its hours (8 to 17).
// With the rule its task is its trade and its counter open: wren, hungry with 20p, buys a loaf there. Without, it goes to
// the festival, no shop is open, and wren buys nothing. After its hours (18:00) it goes to the festival with the rule
// too.
void aFoodShopOpensAtTheFestival()
{
    Society::setWorkByHours(true);
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::FestivalShopsOpen);
        const std::string with = rule ? "with the rule" : "without the rule";
        auto probe = shoppingTown({{"baker", "baker at The Loaf", {"square", 2.5, 2.5}, "bread"}}, 20);
        auto s = shoppingTown({{"baker", "baker at The Loaf", {"square", 2.5, 2.5}, "bread"}}, 20);
        expect(probe.shopPrice("baker", "bread") <= 20, "a loaf for 20p");
        LifeDay plan = s.day();
        DayPlan fair;
        fair.kind = "festival";
        fair.name = "the Fair";
        fair.crowd = {{"fairground", 2.5, 2.5}};
        plan.plans["town"] = fair;
        s.setDay(plan);
        auto bodies = atWork(s);
        bodies["wren"] = {"square", 3.5, 2.5};
        double at = 14. / 24;
        s.tick(1, at, 0, bodies);
        const auto* baker = s.resident("baker");
        expect(rule ? baker->task == "trade" : baker->task == "festival", with + ", the baker at 14:00: " + baker->task + ", " + baker->reason);
        s.takeJournal();
        int loaves = 0;
        bool open = false;
        for (int i = 0; i < 120 && loaves == 0; ++i)
        {
            at += 1. / 86400;
            s.tick(1, at, 0, bodies);
            open = open || s.resident("wren")->task == "buy food";
            for (const auto& e : s.takeJournal())
                loaves += e.kind == "resident food purchase" && e.from == "wren" && e.to == s.tillOf("baker") ? e.quantity : 0;
        }
        expect(rule ? open && loaves > 0 : !open && loaves == 0, with + ", wren at the baker's counter: " + std::to_string(loaves) + " loaves");
        expect(s.conserved(), "money stays conserved");
        s.tick(1, 18. / 24, 0, bodies);
        expect(s.resident("baker")->task == "festival", with + ", after its hours the baker goes to the festival: " + s.resident("baker")->task);
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// A smith still feasts (FestivalShopsOpen): the smith is no food shop; at 14:00 of a festival's day it goes to the
// festival, with the rule and without.
void aSmithStillFeasts()
{
    Society::setWorkByHours(true);
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::FestivalShopsOpen);
        auto s = town();
        LifeDay plan;
        plan.communityOf = [](const std::string&) { return std::string("town"); };
        DayPlan fair;
        fair.kind = "festival";
        fair.name = "the Fair";
        fair.crowd = {{"fairground", 2.5, 2.5}};
        plan.plans["town"] = fair;
        s.setDay(plan);
        s.tick(1, 14. / 24, 0, atWork(s));
        expect(s.resident("smith")->task == "festival", std::string(rule ? "with" : "without") + " the rule, the smith goes to the festival: " +
                                                            s.resident("smith")->task);
        expect(s.resident("baker")->task == (rule ? "trade" : "festival"), "and the baker keeps its counter only with the rule: " + s.resident("baker")->task);
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// Round 14, lane 1 (the margins; the user, 2026-10-30): a fishing boat, a fishmonger, a smokehouse and a scribe to buy,
// one town.
Society fishTown()
{
    AuthoredRoster roster;
    roster.residents = {person("fisher", "fishes the lake", {"lake", 4.5, 4.5}, "civilian"),
                        person("monger", "the fishmonger by the wharf", {"wharf", 2.5, 2.5}),
                        person("smoker", "the smokehouse on the quay", {"quay", 2.5, 2.5}),
                        person("scribe", "the scribe", {"desk", 2.5, 2.5})};
    roster.economy.treasury = 50000;
    Society s(Roster::None);
    s.configure(roster);
    LifeDay day;
    day.communityOf = [](const std::string&) { return std::string("town"); };
    s.setDay(day);
    expect(s.shift("treasury", "scribe", "", 0, 10000, "test: the buyer's money"), "the buyer has money");
    return s;
}

// A seller's shelf set as set() sets it, its margins kept: restore loads them from the save, which the day's pass
// writes.
void restock(Society& s, const std::string& id, const std::string& item, int count)
{
    auto state = s.state();
    state.accounts.at(s.tillOf(id)).stock[item] = count;
    state.memory.margins.clear();
    for (const auto& [seller, goods] : s.margins())
        for (const auto& [good, m] : goods)
            state.memory.margins[seller + "|" + good] = m;
    expect(s.restore(state), "the restocked shelf restores");
}

// A day's sales of a seller's whole shelf, `n`, to the scribe (its money back to it), the day's turn (spoil) and the
// day's margin review.
void sellOut(Society& s, const std::string& id, const std::string& item, int n, std::int64_t day, bool review = true)
{
    const auto till = s.tillOf(id);
    restock(s, id, item, n);
    expect(s.sale(till, "scribe", item, n, 1, "test: the day's buying") && s.shift(till, "scribe", "", 0, n, "test: the scribe's money back"),
           "the scribe buys the shelf");
    s.spoil(day);
    if (review)
        s.reviewMargins(day);
}

// Whether a till's goods are reviewed on a day (reviewMargins: its own weekday, three weeks in four), and the review's
// step (both lots as RatwPrices.cpp draws them).
double reviewLot(const std::string& words)
{
    return double(std::hash<std::string>{}(words) % 100000) / 100000;
}
bool reviewedOn(const std::string& till, const std::string& good, std::int64_t day)
{
    return std::int64_t(std::hash<std::string>{}(till) % 7) == day % 7 &&
           reviewLot(till + "|" + good + "|" + std::to_string(day) + "|review") < .75;
}
double reviewStep(const std::string& till, const std::string& good, std::int64_t day)
{
    return reviewLot(till + "|" + good + "|" + std::to_string(day) + "|step");
}

// MarginsByShortage (round 14, lane 1), with work by the hours: a fishing boat that sells its whole shelf by the turn
// of each day, turning nobody away, is short of nothing. Its margin set to .9, its reviews pull it back toward the
// ladder's 15%; without the rule its shelf bare at the turn raises it toward a staple's most, 1.0, review by review.
// (North Wharf's smokehouse sold 143 of a rate of 144 and sat at 1.6 times its cost.)
void aSoldOutShelfServingAllKeepsTheLadder()
{
    Society::setWorkByHours(true);
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::MarginsByShortage);
        auto s = fishTown();
        const auto till = s.tillOf("fisher");
        expect(till.rfind("till:", 0) == 0 && items::good("fish") && items::good("fish")->staple, "the boat has a till, and fish is a staple");
        std::int64_t day = 1;
        for (; day <= 14; ++day)
            sellOut(s, "fisher", "fish", 30, day, false);
        auto state = s.state();
        state.memory.margins[till + "|fish"] = .9;
        expect(s.restore(state), "its margin at .9 restores");
        const std::string at = rule ? "with the rule" : "without the rule";
        expect(s.sellRates(till) && s.sellRates(till)->at("fish") >= 30 && s.missedRate(till, "fish") == 0,
               at + ": it sells 30 a day, sold out (" + std::to_string(s.sellRates(till)->at("fish")) + " with the half again), turning none away");
        std::vector<double> reviews;
        double before = .9;
        for (; reviews.size() < 2 && day < 80; ++day)
        {
            sellOut(s, "fisher", "fish", 30, day);
            if (const double m = s.marginOf("fisher", "fish"); m != before)
                reviews.push_back(m), before = m;
        }
        expect(reviews.size() == 2, at + ": two reviews moved its margin");
        if (rule)
            expect(reviews[0] < .9 && reviews[1] < reviews[0] && reviews[1] > .15,
                   at + ", two reviews bring it down toward the ladder's: " + std::to_string(reviews[0]) + ", " + std::to_string(reviews[1]));
        else
            expect(reviews[0] > .9 && reviews[1] > reviews[0] && reviews[1] <= Society::StapleMarginMost + 1e-12,
                   at + ", a bare shelf at the turn raises it: " + std::to_string(reviews[0]) + ", " + std::to_string(reviews[1]));
        expect(s.conserved(), "money stays conserved");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// MarginsByShortage, with work by the hours: the same boat with as many again turned away each day (the town's makers
// finding no fish: missedAt), half its buyers. Its margin climbs toward a staple's most; once the buyers are served
// again it comes back down within a few reviews (the turned away wear out of its average in a week). Without the rule
// it rises by its bare shelf alone, and never comes down while it sells out.
void buyersTurnedAwayRaiseIt()
{
    Society::setWorkByHours(true);
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::MarginsByShortage);
        auto s = fishTown();
        const auto till = s.tillOf("fisher");
        const std::string at = rule ? "with the rule" : "without the rule";
        double peak = 0;
        std::int64_t day = 1;
        for (; day <= 84; ++day)                    // (Twelve weeks short.)
        {
            restock(s, "fisher", "fish", 30);
            expect(s.sale(till, "scribe", "fish", 30, 1, "test: the day's buying") && s.shift(till, "scribe", "", 0, 30, "test: back"),
                   "the scribe buys the boat's catch");
            s.missedAt("town", "fish", 30);
            s.spoil(day);
            s.reviewMargins(day);
            peak = std::max(peak, s.marginOf("fisher", "fish"));
        }
        const double unmet = s.missedRate(till, "fish") / s.sellRates(till)->at("fish");
        expect(std::abs(unmet - .5) < .01, at + ": half its buyers turned away (" + std::to_string(unmet) + ")");
        if (rule)
            expect(peak > .7 && peak <= Society::StapleMarginMost + 1e-12, at + ", its margin climbs toward a staple's most: " + std::to_string(peak));
        else
            expect(peak > .2 && peak < .7, at + ", its bare shelf raises it a step a review: " + std::to_string(peak));
        // The shortage over: it sells out, turning none away.
        std::vector<double> after;
        double before = s.marginOf("fisher", "fish");
        for (const auto end = day + 42; day < end; ++day)
        {
            sellOut(s, "fisher", "fish", 30, day);
            if (const double m = s.marginOf("fisher", "fish"); m != before)
                after.push_back(m), before = m;
        }
        expect(after.size() >= 3, at + ": three reviews or more in the six weeks after");
        std::string moves;
        for (const double m : after)
            moves += " " + std::to_string(m);
        if (rule)
            expect(after[2] < peak - .15 && after.back() < after[2] && after.back() < .5,
                   at + ", served again its margin comes back down:" + moves + " (from " + std::to_string(peak) + ")");
        else
            expect(after.front() > peak && after.back() > after.front(), at + ", it goes on rising:" + moves);
        expect(s.conserved(), "money stays conserved");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// MarginsByShortage, with work by the hours: a boat selling 40 a day holding 90, over its own shelf target (72: a day
// and a half's keeping and a staple's fifth over) but under the 2.55 days' sales the old test asked of a glut, which a
// producer stocked to its target never reached. With the rule its margin comes down a step (MarginStep times the
// review's lot); without, it only wears back toward the ladder's (here, at it, it stays).
void aGlutOverItsOwnStore()
{
    Society::setWorkByHours(true);
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::MarginsByShortage);
        auto s = fishTown();
        const auto till = s.tillOf("fisher");
        for (std::int64_t day = 1; day <= 40; ++day)
        {
            set(s, "fisher", "fish", 200);           // (Not sold out: no half again.)
            expect(s.sale(till, "scribe", "fish", 40, 1, "test: the day's buying") && s.shift(till, "scribe", "", 0, 40, "test: back"),
                   "the scribe buys forty");
            s.spoil(day);
        }
        set(s, "fisher", "fish", 90);
        const double rate = s.sellRates(till)->at("fish");
        const int target = s.shelfTarget(till, "fish", Society::ProducerKept);
        const std::string at = rule ? "with the rule" : "without the rule";
        expect(std::abs(rate - 40) < .01 && target == 72 && 90 < Society::keepDays("fish") * 1.7 * rate,
               at + ": 40 a day, a target of " + std::to_string(target) + ", 90 under the old glut's " +
                   std::to_string(Society::keepDays("fish") * 1.7 * rate));
        std::int64_t day = 41;
        while (!reviewedOn(till, "fish", day))
            ++day;
        const double usual = items::ladder().markup;
        s.reviewMargins(day);
        const double m = s.marginOf("fisher", "fish"), step = reviewStep(till, "fish", day);
        expect(rule ? std::abs(m - (usual - step * Society::MarginStep)) < 1e-12 && m < usual : std::abs(m - usual) < 1e-12,
               at + ": its margin " + std::to_string(m) + " (a step " + std::to_string(step) + ")");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// MarginsByShortage, with work by the hours: a farm whose year is planned (its till under memory.fields) holds a store
// by design. Over its shelf target (90 of wheat at 10 a day: a target of 72) but not 1.7 times its keeping days, it
// isn't lowered by the own-target test; the same farm unplanned is.
void aFieldsStoreIsNoGlut()
{
    Society::setWorkByHours(true);
    for (const bool field : {true, false})
    {
        Society::wageRulesOff(0);
        auto s = farmTown();
        const auto till = s.tillOf("farmer");
        expect(s.shift("treasury", "stall", "", 0, 400, "test: the stall's money"), "the stall has money");
        for (std::int64_t day = 1; day <= 40; ++day)
        {
            set(s, "farmer", "wheat", 200);
            expect(s.sale(till, "stall", "wheat", 10, 1, "test: the day's wheat") && s.shift(till, "stall", "", 0, 10, "test: back"),
                   "the stall buys ten wheat");
            s.spoil(day);
        }
        auto state = s.state();
        state.accounts.at(till).stock["wheat"] = 90;
        if (field)
            state.memory.fields[till] = FieldYear{};
        expect(s.restore(state), "its store restores");
        const double rate = s.sellRates(till)->at("wheat");
        const int target = s.shelfTarget(till, "wheat", Society::ProducerKept);
        expect(target < 90 && 90 < Society::keepDays("wheat") * 1.7 * rate,
               "90 over its target, " + std::to_string(target) + ", under 1.7 times its keeping days' sales");
        std::int64_t day = 41;
        while (!reviewedOn(till, "wheat", day))
            ++day;
        const double usual = items::ladder().markup;
        s.reviewMargins(day);
        const double m = s.marginOf("farmer", "wheat");
        expect(field ? std::abs(m - usual) < 1e-12 : m < usual,
               std::string(field ? "a planned field's store isn't lowered: " : "an unplanned farm's is: ") + std::to_string(m));
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// ChainPricedOnce (round 14, lane 1), with work by the hours: the smokehouse wants ten fish, at a 120p day. The boat
// sells at its cost and the ladder's 15% (about 8.5p); the fishmonger holds fifty, its margin .9. Bought at the boat's
// price: with the rule the smokehouse buys from the boat at the boat's price, and with the boat sold out from the
// fishmonger at what it paid and the ladder's 15%. Bought at half the boat's: from the fishmonger, the cheaper,
// whatever its kind. Bought so that it asks the boat's penny: from the boat (level, the producer first). Without the
// rule, from the fishmonger every time (a supplier first, whatever it asks), at its cost and its .9. (Ridgemere's
// smokehouses bought four fifths of their fish from its resellers at 18-20p while its boats sold at 9-11p.)
void aMakerBuysAtTheSource()
{
    Society::setWorkByHours(true);
    struct Case
    {
        const char* what;
        int boat;
        double paid;                                // What the fishmonger paid, of the boat's price.
        bool fromBoat;                              // With the rule.
    };
    for (const bool rule : {true, false})
        for (const auto& c : {Case{"the boat with fish", 50, 1, true}, Case{"the boat sold out", 0, 1, false},
                              Case{"the fishmonger's bought at half", 50, .5, false}, Case{"the same penny", 50, 1 / 1.15, true}})
        {
            Society::wageRulesOff(rule ? 0 : Society::ChainPricedOnce);
            auto s = fishTown();
            const auto fishery = s.tillOf("fisher"), monger = s.tillOf("monger"), smokehouse = s.tillOf("smoker");
            expect(items::businessFor("the fishmonger by the wharf") && items::businessFor("the fishmonger by the wharf")->id == "fishmonger" &&
                       items::businessFor("the smokehouse on the quay") && items::businessFor("the smokehouse on the quay")->id == "smokehouse",
                   "a fishmonger and a smokehouse");
            auto state = s.state();
            state.memory.going["town"] = 120;
            expect(s.restore(state), "the town's 120p day restores");
            const double source = s.priceAt("fisher", "fish");
            state = s.state();
            state.accounts.at(fishery).stock["fish"] = c.boat;
            state.accounts.at(monger).stock["fish"] = 50;
            state.accounts.at(smokehouse).stock.erase("fish");
            state.memory.costs[monger + "|fish"] = source * c.paid;
            state.memory.margins[monger + "|fish"] = .9;
            expect(s.restore(state), "the fish town restores");
            expect(s.shift("treasury", smokehouse, "", 0, 2000, "test: the smokehouse's money"), "the smokehouse has money");
            expect(std::abs(s.priceAt("fisher", "fish") - source) < 1e-9 && std::abs(s.marginOf("fisher", "fish") - .15) < 1e-12 && source > 8 &&
                       source < 10,
                   "the boat at its cost and the ladder's 15%: " + std::to_string(source) + "p");
            s.takeJournal();
            const int bought = s.buyMaterials("smoker", "quay", "fish", 10);
            std::map<std::string, std::pair<int, std::int64_t>> from;   // Seller's till -> pieces, coins.
            for (const auto& e : s.takeJournal())
                if (e.kind == "materials bought" && e.from == smokehouse)
                    from[e.to].first += e.quantity, from[e.to].second += e.coins;
            const auto at = [&](const std::string& who) { return from.count(who) ? from.at(who) : std::pair<int, std::int64_t>{0, 0}; };
            const std::string what = std::string(rule ? "with the rule, " : "without the rule, ") + c.what;
            expect(bought == 10, what + ": ten bought");
            if (rule && c.fromBoat)
                expect(at(fishery).first == 10 && at(fishery).second == 10 * Society::pennies(source),
                       what + ": from the boat at its price, " + std::to_string(at(fishery).second) + "p for " + std::to_string(at(fishery).first));
            else
            {
                const double each = source * c.paid * (1 + (rule ? .15 : .9));
                expect(at(monger).first == 10 && at(monger).second == 10 * Society::pennies(each),
                       what + ": from the fishmonger at " + std::to_string(each) + "p a piece: " + std::to_string(at(monger).second) + "p for " +
                           std::to_string(at(monger).first));
            }
            expect(s.conserved(), "money stays conserved");
        }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// ChainPricedOnce, with work by the hours: the fishmonger's fish, bought at 9p, its margin .9, sold out at each day's
// turn. With the rule it is priced at what it paid and the ladder's 15% from the first, and its bare-shelf review (the
// old raise, MarginsByShortage left out; or the new aim, both rules) keeps its margin to the ladder's, nothing hidden
// behind the price; a glut over its target is still marked down (Markdowns). Without the rule, its cost and its own
// margin, raised by the review.
void aResoldGoodAtTheLadder()
{
    Society::setWorkByHours(true);
    const double usual = items::ladder().markup;
    for (const std::uint64_t off : {std::uint64_t(0), std::uint64_t(Society::MarginsByShortage),
                                    std::uint64_t(Society::MarginsByShortage | Society::ChainPricedOnce)})
    {
        Society::wageRulesOff(off);
        const bool rule = !(off & Society::ChainPricedOnce);
        const std::string at = rule ? (off ? "with the rule (the old review)" : "with both rules") : "without the rule";
        auto s = fishTown();
        const auto monger = s.tillOf("monger");
        auto state = s.state();
        state.memory.costs[monger + "|fish"] = 9;
        state.memory.margins[monger + "|fish"] = .9;
        state.accounts.at(monger).stock["fish"] = 30;
        expect(s.restore(state), "the fishmonger's fish restores");
        expect(s.resells(monger, "fish") && !s.resells(monger, "smoked_fish") && !s.resells(s.tillOf("fisher"), "fish"),
               "the fishmonger sells its fish on and smokes its own; the boat makes its fish");
        expect(std::abs(s.priceAt("monger", "fish") - 9 * (1 + (rule ? usual : .9))) < 1e-9,
               at + ": priced at " + std::to_string(s.priceAt("monger", "fish")) + "p");
        std::int64_t day = 1;
        int reviews = 0;
        for (; day <= 60 && reviews < 2; ++day)
        {
            reviews += reviewedOn(monger, "fish", day);
            sellOut(s, "monger", "fish", 30, day);
        }
        expect(reviews == 2, at + ": two reviews of its fish");
        const double m = s.marginOf("monger", "fish");
        if (rule)
            expect(std::abs(m - usual) < 1e-12 && std::abs(s.priceAt("monger", "fish") - 9 * (1 + usual)) < 1e-9,
                   at + ": its margin kept to the ladder's (" + std::to_string(m) + "), 9p and 15%");
        else
            expect(m > .9 && std::abs(s.priceAt("monger", "fish") - 9 * (1 + m)) < 1e-9, at + ": its bare shelf raised its margin to " + std::to_string(m));
        // A glut: two days over its target, a step marked down.
        const double before = s.priceAt("monger", "fish");
        restock(s, "monger", "fish", s.shelfTarget(monger, "fish", Society::ProducerKept) + 50);
        s.reviewMarkdowns();
        s.reviewMarkdowns();
        expect(std::abs(s.priceAt("monger", "fish") - before * (1 - Society::MarkdownStep)) < 1e-9,
               at + ": over its target it is marked down: " + std::to_string(s.priceAt("monger", "fish")) + "p from " + std::to_string(before) + "p");
        expect(s.conserved(), "money stays conserved");
    }
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// Round 15, lane 2 (RoadMoneyKept; the user, 2026-10-30), with work by the hours: a town's stores keep the road's week.
// In needRealm (none of the city poor, so nothing is sent on), the far town's day-35 week: its going rate at 100p (a
// works place 700p a week), its caravans' takings this week 3,000p (caravanWeek), and its standing orders (iron bars
// from the far town, setStandingOrders) costing W, about 1,600p, a week at its price: its float a week and a quarter of
// it, F. Its stores hold F and 2,000p (their mean holding still about the 100,000p they held): with the rule its works
// are hired from the 2,000p over the float at the most (a smaller crew than without, whose budget is its takings); its
// mean holding moves toward what it holds less the float (F/7 under it at the day's turn), and its rebate is reckoned on
// what it holds less the float, half of F the less (with TaxRebate; the crews' runs leave it out, so the stores' cash
// at its week is the same both ways). Holding F and 8,000p, its crew is the same both ways. A town whose trader is a
// House (World::traderOf) keeps none; nor does any town without the rule.
void theWorksLeaveTheRoadsWeek()
{
    Society::setWorkByHours(true);
    Society::noteTowns(true);
    struct Seen
    {
        int crew = 0;
        double road = 0, meanBefore = 0, weekCost = 0;
        std::int64_t rebate = 0, sent = 0;
    };
    const auto run = [](std::uint64_t off, std::int64_t over) {
        // (The holdings tax left out, as aTownsSurplusGoesWhereTheNeedIs does: the payers' 100,000p would pay it.)
        Society::wageRulesOff(off | Society::HoldingsTax | Society::RichHomes);
        auto realm = needRealm(0);
        auto& s = *realm.s;
        Seen seen;
        auto st = s.state();
        st.memory.going["far"] = 100;
        expect(s.restore(st), "the far town's going rate");
        const auto price = Society::pennies(s.townPrice("far", "iron_bar"));
        const int bars = int((1600 + price - 1) / price);
        seen.weekCost = double(bars * price);
        s.setStandingOrders({{{"far", "iron_bar"}, bars}}, {});
        seen.road = s.roadFloat("stores:far");
        const auto F = std::llround(Society::RoadWeeksKept * seen.weekCost);
        st = s.state();
        for (int i = 1; i <= 10; ++i)
            st.memory.weekIncome["payer" + std::to_string(i)] = 100;
        auto& stores = st.accounts.at("stores:far");
        st.accounts.at("treasury").cash += stores.cash - (F + over);
        stores.cash = F + over;
        st.memory.caravanWeek["stores:far"] = 3000;
        st.memory.biteLeft.clear();
        st.memory.postsOwed.erase("stores:far");
        expect(s.restore(st), "the far town's stores, its takings in");
        s.takeJournal();
        s.tick(1, 35 + .5 / 24, 0, realm.bodies);
        for (const auto& e : s.takeJournal())
            if (e.from == "stores:far")
            {
                seen.sent += e.kind == "capital's share" || e.kind == "a town's surplus sent on" ? e.coins : 0;
                seen.rebate += e.kind == "a town's rebate" ? e.coins : 0;
            }
        // (Its mean holding at the day's turn: what the week's reckoning left it, and what it sent the capital.)
        seen.meanBefore = s.townHeldMean("stores:far") + double(seen.sent);
        for (const auto& j : s.oddJobs())
            if (j.payer == "stores:far" && j.kind == "building work" && j.until >= 35)
                seen.crew += j.slots;
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto thin = run(Society::TaxRebate, 2000), thinWithout = run(Society::TaxRebate | Society::RoadMoneyKept, 2000);
    const double F = Society::RoadWeeksKept * thin.weekCost;
    expect(thin.weekCost >= 1600 && std::abs(thin.road - F) < 1e-9 && thinWithout.road == 0,
           "the far town's orders, " + std::to_string(thin.weekCost) + "p a week at its price: its float " + std::to_string(thin.road) +
               "p with the rule, none without");
    expect(thin.crew > 0 && thin.crew * 700 <= 2000 * 8 / 10 && thin.crew < thinWithout.crew,
           "with the rule its works are hired from the 2,000p over the float at the most: a crew of " + std::to_string(thin.crew) +
               ", against " + std::to_string(thinWithout.crew) + " without");
    expect(std::abs(thin.meanBefore - (thinWithout.meanBefore - F / Society::HeldMeanDays)) < 1,
           "its mean holding moves toward what it holds less the float: " + std::to_string(thin.meanBefore) + " against " +
               std::to_string(thinWithout.meanBefore));
    const auto rebated = run(0, 2000), rebatedWithout = run(Society::RoadMoneyKept, 2000);
    // (Half of what it holds over two targets a week, after the capital's share: half the float less, but for any
    // difference in the shares, and each payer's part rounded, ten of them.)
    const double less = (F - double(rebatedWithout.sent - rebated.sent)) / 2;
    expect(rebated.rebate > 0 && std::abs(double(rebatedWithout.rebate - rebated.rebate) - less) <= 10,
           "its rebate is reckoned on what it holds over the float: " + std::to_string(rebated.rebate) + "p, against " +
               std::to_string(rebatedWithout.rebate) + "p without (" + std::to_string(less) + "p less, its share " +
               std::to_string(rebated.sent) + "p against " + std::to_string(rebatedWithout.sent) + "p)");
    const auto rich = run(Society::TaxRebate, 8000), richWithout = run(Society::TaxRebate | Society::RoadMoneyKept, 8000);
    expect(rich.crew > thinWithout.crew && rich.crew == richWithout.crew,
           "holding well over it, its crew is the same both ways: " + std::to_string(rich.crew) + " and " + std::to_string(richWithout.crew));
    // A town whose trader is a House: none; the town beside it without one, its float.
    for (const bool rule : {true, false})
    {
        Society::wageRulesOff(rule ? 0 : Society::RoadMoneyKept);
        AuthoredRoster roster;
        auto lord = person("lord", "ruling House Fell", {"far_hall", 2.5, 2.5}, "civilian");
        lord.home = lord.evening = {"far_homes", 2.5, 2.5};
        roster.residents = {lord, person("scribe", "the scribe", {"desk", 2.5, 2.5}, "civilian")};
        roster.economy.treasury = 100000;
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string& cell) {
            return cell.rfind("far_", 0) == 0 ? std::string("far") : cell.rfind("near_", 0) == 0 ? std::string("near") : std::string("town");
        };
        s.setDay(plan);
        const auto bodies = atWork(s);
        for (std::int64_t d = 1; d <= 2; ++d)
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        const bool house = std::any_of(s.houses().begin(), s.houses().end(), [&](const House& h) { return h.community == "far" && s.account(h.id); });
        s.setStandingOrders({{{"far", "iron_bar"}, 20}, {{"near", "iron_bar"}, 20}}, {});
        const double near = Society::RoadWeeksKept * 20 * double(Society::pennies(s.townPrice("near", "iron_bar")));
        if (rule)
            expect(house && s.roadFloat("stores:far") == 0 && std::abs(s.roadFloat("stores:near") - near) < 1e-9,
                   "the far town's trader a House, no float; the near town's " + std::to_string(s.roadFloat("stores:near")) + "p");
        else
            expect(s.roadFloat("stores:far") == 0 && s.roadFloat("stores:near") == 0, "without the rule, none");
        expect(s.roadFloat("treasury") == 0, "and never the capital's");
        expect(s.conserved(), "money stays conserved");
    }
    Society::wageRulesOff(0);
    Society::noteTowns(false);
    Society::setWorkByHours(false);
}

// Round 15, lane 1 (CarriageShared; the user, 2026-10-30), with work by the hours and CapitalByIncome: the capital
// spends the buying town's half of the carriage in its week. A city of 151 (its crew at most 15, estateMost) and a
// 60p day, its month's income 4,000p and its treasury 20,000p before the wagon's money, on the morning of its fifth
// week's reckoning (day 35): no day labour asked, nothing of its holdings tax in hand. A caravan's account pays it N as
// "carriage to the buying town". With the rule its caravans' week holds N, and at its week its budget is N larger:
// relief a twentieth of it more, its buyers' allowance a twentieth of what's left more (both read as set aside, what
// the day drew of them counted back), and its works' crew 80% of the rest more at a week of 60p a place, its crew at
// estateMost at the most; its month's income the same as without (the share is no earning). Without the rule the entry
// adds nothing to its week: the same treasury, the crew of its own budget.
void theCapitalSpendsTheCarriageInItsWeek()
{
    Society::setWorkByHours(true);
    struct Seen
    {
        double week = 0, relief = 0, funds = 0, month = 0, earned = 0;
        int crew = 0, most = 0;
        std::int64_t wage = 0;
    };
    const auto run = [](bool rule, std::int64_t carriage) {
        Society::wageRulesOff((rule ? 0 : Society::CarriageShared) | Society::HoldingsTax | Society::RichHomes);
        AuthoredRoster roster;
        roster.residents.push_back(person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}));   // (Where the city's crews gather.)
        for (int i = 1; i <= 150; ++i)
        {
            auto c = person("c" + std::to_string(i), "keeps the house", {"city_home" + std::to_string(i), 2.5, 2.5}, "civilian");
            c.home = c.evening = c.work;
            c.purse = 1000;
            roster.residents.push_back(c);
        }
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string&) { return std::string("town"); };
        s.setDay(plan);
        s.openAccount("caravan:t");
        const auto bodies = atWork(s);
        for (std::int64_t d = 1; d <= 34; ++d)
            s.tick(1, double(d) + .5 / 24, 0, bodies);
        {
            auto st = s.state();
            st.memory.monthIncome["treasury"] = 4000;
            st.memory.going[""] = st.memory.going["town"] = 60;
            st.memory.labourWeek.erase("treasury");
            st.memory.labourWeek.erase("treasury|asked");
            st.memory.reliefWeek["treasury"] = 0;
            st.memory.fundsWeek["treasury"] = 0;
            st.memory.biteLeft.erase("treasury");
            st.memory.caravanWeek.erase("treasury");
            // (The treasury at 20,000p and the wagon holding N, the rest to a home: the same capital both ways.)
            auto& treasury = st.accounts.at("treasury");
            st.accounts.at("c150").cash += treasury.cash - 20000 - carriage;
            treasury.cash = 20000;
            st.accounts.at("caravan:t").cash = carriage;
            expect(s.restore(st), "the capital's month, its day, the wagon's money");
        }
        expect(s.shift("caravan:t", "treasury", "", 0, carriage, "carriage to the buying town"), "the wagon pays the city its half of the carriage");
        Seen seen;
        const auto of = [](const std::map<std::string, double>& map) { return map.count("treasury") ? map.at("treasury") : 0.; };
        seen.week = of(s.state().memory.caravanWeek);
        s.tick(1, 35 + .5 / 24, 0, bodies);
        const auto& m = s.state().memory;
        seen.relief = of(m.reliefLeft) + of(m.reliefWeek);
        seen.funds = of(m.fundsLeft) + of(m.fundsWeek);
        for (const auto& j : s.oddJobs())
            if (j.payer == "treasury" && j.kind == "building work" && j.until >= 35)
                seen.crew += j.slots, seen.wage = j.pay / std::max(1, j.slots);
        seen.most = s.estateMost();
        s.tick(1, 36 + .5 / 24, 0, bodies);
        seen.month = s.monthIncomeOf("treasury");
        seen.earned = of(s.state().memory.weekIncome);
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const std::int64_t n = 4200;
    const auto with = run(true, n), without = run(false, n), much = run(true, 14000), muchWithout = run(false, 14000);
    const auto said = [](const Seen& s) {
        return "week " + std::to_string(s.week) + ", relief " + std::to_string(s.relief) + ", funds " + std::to_string(s.funds) + ", crew " +
               std::to_string(s.crew) + " of " + std::to_string(s.most) + " at " + std::to_string(s.wage) + "p";
    };
    std::cout << "  the capital's carriage: " << said(with) << "; without, " << said(without) << "; 14,000p: " << said(much) << "\n";
    expect(with.week == double(n) && without.week == 0, "with the rule its caravans' week holds the carriage; without, nothing: " + said(with));
    const double relief = with.relief - without.relief, funds = with.funds - without.funds;
    expect(std::abs(relief - n / 20.) <= 1, "its relief a twentieth of the carriage more: " + said(with) + "; without, " + said(without));
    expect(std::abs(funds - (n - relief) / 20) <= 1, "its buyers' allowance a twentieth of what's left more: " + said(with) + "; without, " + said(without));
    const double places = (double(n) - relief - funds) * 8 / 10 / double(7 * with.wage);
    expect(with.wage == 60 && without.wage == 60 && std::abs(double(with.crew - without.crew) - places) < 1.,
           "its works' crew larger by 80% of the rest at a week a place (" + std::to_string(places) + "): " + said(with) + "; without, " + said(without));
    expect(with.crew < with.most, "under its most: " + said(with));
    expect(much.crew == much.most && much.most == 15 && muchWithout.crew < much.most,
           "14,000p brings it to estateMost, no further: " + said(much) + "; without, " + said(muchWithout));
    expect(std::abs(with.month - without.month) < 1e-9 && std::abs(with.earned - without.earned) < 1e-9 && with.month > 0,
           "its month's income as without the rule: " + std::to_string(with.month) + " and " + std::to_string(without.month));
    Society::setWorkByHours(false);
}

// Round 15, lane 1 (MessesFirst; the user, 2026-10-30), with work by the hours: the mess is fed before the works. A
// capital of two guards and a hundred scribes (the Town Works' basket a resident's, the watch's a guard's), its buyers'
// working money spent, and its week's allowance for its buyers (fundsLeft) set at the watch's one day's grant (what it
// draws with the allowance unbounded). With the rule the watch's "town funds" are that grant and the Works gets none
// that day; without, the Works takes it and the watch gets none. The order: the watch, the mines' and the quarries'
// first (each a bread in its basket), then the Works, the church (it tithes: its bread is the hungry's, its money the
// land's), the docks and the hall as written; without, as written.
void theMessIsFedBeforeTheWorks()
{
    Society::setWorkByHours(true);
    struct Seen
    {
        std::map<std::string, std::int64_t> funds;
        std::vector<std::string> order;
    };
    const auto run = [](bool rule, double allowance) {
        Society::wageRulesOff(rule ? 0 : Society::MessesFirst);
        AuthoredRoster roster;
        for (int i = 1; i <= 2; ++i)
            roster.residents.push_back(person("watch" + std::to_string(i), "keeps the watch", {"gate", 2.5 + i, 2.5}, "guard"));
        for (int i = 1; i <= 100; ++i)
            roster.residents.push_back(person("scribe" + std::to_string(i), "the scribe", {"desk" + std::to_string(i), 2.5, 2.5}));
        roster.economy.treasury = 1000000;
        Society s(Roster::None);
        s.configure(roster);
        s.setCapital("town");
        LifeDay plan;
        plan.communityOf = [](const std::string&) { return std::string("town"); };
        s.setDay(plan);
        const auto bodies = atWork(s);
        s.tick(1, 1 + .5 / 24, 0, bodies);      // (Its buyers founded, with their working money.)
        {
            auto st = s.state();
            for (const auto* id : {"town:town:watch", "town:town:works"})
            {
                expect(st.accounts.count(id) > 0, std::string(id) + " is founded");
                st.accounts.at("treasury").cash += st.accounts.at(id).cash;
                st.accounts.at(id).cash = 0;
            }
            st.memory.fundsLeft["treasury"] = allowance;
            expect(s.restore(st), "its buyers' money spent, the week's allowance for them");
        }
        s.takeJournal();
        s.tick(1, 2 + .5 / 24, 0, bodies);
        Seen seen;
        for (const auto& e : s.takeJournal())
            if (e.from == "treasury" && e.kind == "town funds")
                seen.funds[e.to] += e.coins;
        for (const auto* in : Society::townBuyersOrder())
            seen.order.push_back(in->id);
        expect(s.conserved(), "money stays conserved");
        Society::wageRulesOff(0);
        return seen;
    };
    const auto at = [](const Seen& s, const std::string& id) { return s.funds.count(id) ? s.funds.at(id) : std::int64_t(0); };
    const auto probe = run(true, 1e12);
    const auto watch = at(probe, "town:town:watch"), works = at(probe, "town:town:works");
    expect(watch > 0 && works >= watch, "unbounded, the watch's grant " + std::to_string(watch) + "p, the Works' " + std::to_string(works) + "p");
    const auto with = run(true, double(watch)), without = run(false, double(watch));
    std::cout << "  the mess first: the watch's day " << watch << "p, the Works' " << works << "p unbounded\n";
    expect(at(with, "town:town:watch") == watch && at(with, "town:town:works") == 0,
           "with the rule the watch's grant, none to the Works: " + std::to_string(at(with, "town:town:watch")) + "p and " +
               std::to_string(at(with, "town:town:works")) + "p");
    expect(at(without, "town:town:works") == watch && at(without, "town:town:watch") == 0,
           "without, the Works takes it and the watch none: " + std::to_string(at(without, "town:town:works")) + "p and " +
               std::to_string(at(without, "town:town:watch")) + "p");
    const auto join = [](const std::vector<std::string>& v) {
        std::string out;
        for (const auto& id : v)
            out += (out.empty() ? "" : " ") + id;
        return out;
    };
    expect(join(with.order) == "watch mines quarries works church docks hall", "the messes first, the rest as written: " + join(with.order));
    expect(join(without.order) == "works watch church docks mines quarries hall", "without, as written: " + join(without.order));
    Society::setWorkByHours(false);
}

int main()
{
    try
    {
        theCapitalSpendsTheCarriageInItsWeek();
        theMessIsFedBeforeTheWorks();
        aHandsDayBringsInAtOnce();
        aFullStoreStillLosesWork();
        aDeadKeepersHandMakesNothing();
        aSuccessorSellsOn();
        theListsAreTheSameWithNoSuccession();
        startingMaterials();
        bakesFromMaterials();
        aPawLent();
        buysWhenShort();
        nothingFromNothing();
        smithForges();
        olderSavesGetTheirGrantOnce();
        farmerBringsIn();
        seasonalFields();
        aFieldPlansWhatItBroughtIn();
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
        aRichHomeSpendsWithItsBudget();
        aRichHomeBuysMore();
        theHoldingsTax();
        noTreatsInAHungryTown();
        theCatchFeedsTheLarder();
        theMint();
        aStallDumpAndTheRebound();
        theGoingRateFromTheMakers();
        aTownKeepsItsRateAtTheOldPace();
        handsByWhatCanBeMade();
        aShortBusinessBidsUp();
        aShortPayWeekNeitherBidsNorCalms();
        theLandsHiresForAnyone();
        aProducersPostsBySales();
        aProducersHireBySales();
        theBrakeReadsTheLandsHours();
        aHireTakenIsNoPostOpen();
        aHireTakenIsNoPostOpen("carries loads for hire");
        aHireTakenIsNoPostOpen("idling in the square", 12);
        theGoingRateByEmployees();
        aStallLetsItsSurplusGo();
        theCapitalsPostsOfLastResort();
        aHouseKeepsItsCrew();
        aTownsCrewTakesTheGrownFirst();
        onlyTheSellersLetGo();
        aMakerKeepsItsApprentice();
        theOutOfWorkKeepTheirHours();
        aTownCountsItsCaravans();
        aTownSpendsItsTradeAsItComes();
        aTownsPostsComeFirst();
        aTownsCrewHiresAbroad();
        aTownEasesItsTax();
        aTownKeepsItsPostsToItsTax();
        aTownsRoadMoneyFromItsBudget();
        aTownFoundedAtItsTarget();
        theCapitalsShareByWealth();
        theDayLabourAllowances();
        theHoldingsTaxBites();
        theCapitalKeptToItsIncome();
        aTownsSurplusGoesWhereTheNeedIs();
        theBiteGoesOn();
        noBabyTakesAHire();
        aChildsPenniesToTheHousehold();
        theKeepersOwnApprentice();
        aFarTakerGivesWay();
        aProducerKeepsItsOwnPost();
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
        aShopsGoodsToSellOnForAFortnight();
        aLoneOwnerKeepsAWeeksFood();
        aLoneOwnerStopsToppingUp();
        aStoppedBusinessIdlesAndReturns();
        aWageBidBySales();
        theSlackCountsTheLiving();
        aShortBusinessThatCantPayKeepsItsWage();
        aStoppedKeeperTakesAHire();
        aFarmIsJudgedAfterItsHarvest();
        aHungryWolfBuysAtItsPrice();
        aFestivalsFeastLaidIn();
        aFarShopIsTheNearest();
        aHungryBakerEatsAtItsCounter();
        aHungryBakerBakesFirst();
        theHandsBakeWithoutTheKeeper();
        theFeastIsBoughtAhead();
        aGlutIsMarkedDown();
        aSoldOutShelfServingAllKeepsTheLadder();
        buyersTurnedAwayRaiseIt();
        aGlutOverItsOwnStore();
        aFieldsStoreIsNoGlut();
        aMakerBuysAtTheSource();
        aResoldGoodAtTheLadder();
        theWorksLeaveTheRoadsWeek();
        theCheaperShopWins();
        nearEnoughIsNearer();
        theLarderLaysInWhereItsCheap();
        theValueIsHourly();
        aFoodShopOpensAtTheFestival();
        aSmithStillFeasts();
        aHouseSpendsInsideItsWeek();
        theChurchsAlmsInsideItsRelief();
        theAlmsBuyTheCheapestThatFeeds();
        aHouseCarriesWhatItDidntPay();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Crafting tests passed: " << checks << " checks.\n";
    return 0;
}

