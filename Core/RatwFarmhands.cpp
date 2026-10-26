// Farmhands (Docs/Design/42-money-in-circulation.md, "Farmhands"; the user, 2026-10-06: spread the farms' money out).
// Every farm, orchard and vineyard with money to spare hires a hand for a few days (longer in its season), at the going
// pay for a hand in its town (Society::dayWage: doc 64, Phase 6). Its town's folk may take it, and so may anyone living
// in a city; one from elsewhere lodges in the farm's bunkhouse for the hire (up to BunkBeds), its home there meanwhile,
// eating from the bunkhouse larder, which the farm stocks from its own food first, then from the town's shops. When the
// hire ends the hand goes home. All daily, in the day's pass: nothing here runs per tick.
#include "RatwSociety.h"

#include "RatwCalendar.h"
#include "RatwItems.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>

namespace ratw
{
namespace
{
struct Lodged
{
    std::string cell;
    double x = 0, y = 0;
    std::int64_t until = 0;
};
Lodged readLodged(const std::string& s)
{
    Lodged l;
    std::stringstream in(s);
    std::string part;
    std::getline(in, l.cell, '|');
    if (std::getline(in, part, '|'))
        l.x = std::atof(part.c_str());
    if (std::getline(in, part, '|'))
        l.y = std::atof(part.c_str());
    if (std::getline(in, part, '|'))
        l.until = std::atoll(part.c_str());
    return l;
}
} // namespace

bool Society::farmWork(const std::string& producer)
{
    return producer == "farm" || producer == "orchard" || producer == "vineyard" || producer == "sheep_farm" ||
           producer == "rabbit_farm" || producer == "dairy_herd" || producer == "swineherd" || producer == "sheep_pasture";
}

bool Society::fieldWork(const std::string& producer)
{
    return producer == "farm" || producer == "orchard" || producer == "vineyard";
}

int Society::fieldWindow(double absoluteDay)
{
    const auto d = std::int64_t(std::floor(std::max(0., absoluteDay))) % calendar::DaysPerYear;
    return d < SowEnd ? 1 : d >= HarvestStart && d < HarvestEnd ? 2 : 0;
}

bool Society::grownToEat(const std::string& item) const
{
    // What feeds, or what goes only into food or flour (wheat, oats, vegetables; not straw, hemp or firewood, which
    // other trades burn and weave too). (Worked out once a good: the catalog doesn't change.)
    if (const auto known = grownToEat_.find(item); known != grownToEat_.end())
        return known->second;
    bool used = false, otherwise = false;
    if (!edible(item))
        for (const auto& b : items::businesses())
            for (const auto* k : items::craftsFor(b.id))
                if (std::any_of(k->in.begin(), k->in.end(), [&](const auto& i) { return i.first == item; }))
                {
                    used = true;
                    otherwise = otherwise || k->out.empty() || (!edible(k->out.front().first) && k->out.front().first != "flour");
                }
    return grownToEat_[item] = edible(item) || (used && !otherwise);
}

bool Society::plannedBy(const items::Producer& p, const std::string& item) const
{
    // Its food, or all it grows where it grows none (an orchard's apples, for cider).
    return grownToEat(item) || std::none_of(p.out.begin(), p.out.end(), [&](const auto& o) { return grownToEat(o.first); });
}

double Society::fieldNeed(const std::string& till, const items::Producer& p) const
{
    const auto rates = sellRate_.find(till);
    double most = 0;
    if (rates != sellRate_.end())
        for (const auto& [good, n] : p.out)
            if (const auto r = rates->second.find(good); r != rates->second.end() && plannedBy(p, good))
                most = std::max(most, r->second / std::max(1, n));
    return most;
}

bool Society::fieldTill(const std::string& till) const
{
    const auto* job = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr;
    const auto* p = job ? items::producerFor(job->title) : nullptr;
    return p && fieldWork(p->id);
}

double Society::foundingRate(const std::string& till, const items::Producer& p) const
{
    // Its sales a day over its whole weeks of them, the most of what it is planned by, in yields (doc 64, Phase 13: the
    // running average on the day it was founded swung from half to nearly twice its sales a day by the weekday its
    // caravan bought on; the model's is a month's sum over its days).
    const auto sales = fieldSales_.find(till);
    if (sales == fieldSales_.end() || sales->second.days < std::max(7, FieldFoundDays))
        return 0;
    const double days = 7. * (sales->second.days / 7);
    double most = 0;
    for (const auto& [good, n] : p.out)
        if (const auto s = sales->second.weeks.find(good); s != sales->second.weeks.end() && plannedBy(p, good))
            most = std::max(most, s->second / days / std::max(1, n));
    return most;
}

void Society::planField(const std::string& id, const std::string& till, const items::Producer& p, double absoluteDay, bool founding)
{
    auto& f = state_.memory.fields[till];
    const auto day = std::int64_t(std::floor(absoluteDay));
    const auto d = day % calendar::DaysPerYear;
    // Its sales a day: at its founding, over its whole weeks of them (foundingRate); at a harvest's end, its month's or
    // its year's, the more (the model's max(year_sales / YEAR, daily_sales)), or its last few days' if its month's is
    // still to come (a save from before it).
    const double daily = founding ? foundingRate(till, p) : std::max({f.month, f.rate, f.monthDays < 7 ? fieldNeed(till, p) : 0.});
    // To the harvest's end after next (or this year's, still to come): what the store must last.
    const double year = calendar::DaysPerYear + HarvestEnd - HarvestStart;
    const double held = fieldHeld(till, p);
    if (founding)
    {
        // First seen (the founding, or the switch turned on): this year's fields as if sown, and a store to its harvest.
        // Its month's and year's rates begin there, its month's as if over the weeks it was founded on.
        const double toEnd = double((HarvestEnd - d + calendar::DaysPerYear) % calendar::DaysPerYear);
        const auto sales = fieldSales_.find(till);
        f.acres = daily * year * FieldMargin / FieldYields;
        f.sown = f.toHarvest = d < HarvestEnd ? f.acres : 0;
        f.year = day / calendar::DaysPerYear - (d < HarvestEnd ? 1 : 0);   // (The year it last planned at a harvest's end.)
        f.rate = f.month = daily;
        f.week = 0;
        f.monthDays = sales == fieldSales_.end() ? 0 : 7 * (sales->second.days / 7);
        f.founded = day;
        f.toppedUp = false;
        storeYields(id, till, p, daily * toEnd * FieldMargin - held);
        return;
    }
    // At the harvest's end: what's left in the fields is lost, and next year's acres are planned, the store's carry off.
    const double carry = std::max(0., held - daily * (HarvestStart + calendar::DaysPerYear - HarvestEnd));
    f.acres = std::max(0., daily * year * FieldMargin * (f.dry ? DryYear : 1.) - carry) / FieldYields;
    f.sown = f.toHarvest = 0;
    f.dry = false;
    f.year = day / calendar::DaysPerYear;
}

double Society::fieldHeld(const std::string& till, const items::Producer& p) const
{
    // What it holds of what it is planned by, in yields (the scarcest of them).
    double held = 1e18;
    if (const auto* a = account(till))
        for (const auto& [good, n] : p.out)
            if (plannedBy(p, good))
                held = std::min(held, double(stockAll(*a, good)) / std::max(1, n));
    return held > 1e17 ? 0 : held;
}

void Society::storeYields(const std::string& id, const std::string& till, const items::Producer& p, double want)
{
    if (want < 1 || !account(till))
        return;
    for (const auto& [good, n] : p.out)
        for (int more = int(std::ceil(want * n)); more > 0; more -= 99)   // (A ledger entry holds 99 at most.)
        {
            addCost(till, good, stock(*account(till), good), std::min(more, 99), normalCost(id, good));
            create(till, good, std::min(more, 99), "a year's store");
        }
}

void Society::reviewField(const std::string& id, const std::string& till, const items::Producer& p, double absoluteDay)
{
    // Its founding spring (doc 64, Phase 13): founded on a week of its sales, before the roads' orders on it were known,
    // it looks at its acres again once a week, on a day of its own, its idle hands sowing what more its sales want (what
    // is sown stays sown); and a fortnight on, its store is topped up to the harvest, once. Both by its month's rate, not
    // the last few days' (they swung from half to nearly twice its sales a day with the weekday, and its acres went up to
    // the most of it), the store by its year's if that is more.
    auto& f = state_.memory.fields[till];
    const auto day = std::int64_t(std::floor(absoluteDay));
    if (f.founded < 0 || day / calendar::DaysPerYear != f.founded / calendar::DaysPerYear || fieldWindow(absoluteDay) != 1)
        return;
    if (std::hash<std::string>{}(till + "|acres") % 7 == std::uint64_t(day % 7) && f.monthDays >= 7)
        f.acres = std::max(f.acres, f.month * (calendar::DaysPerYear + HarvestEnd - HarvestStart) * FieldMargin / FieldYields);
    // (Not till its rate is known: a field read from a save without one waits for its sales, its top-up still to come.)
    if (const double daily = std::max(f.month, f.rate); !f.toppedUp && day >= f.founded + FieldTopUpDays && daily > 0)
    {
        f.toppedUp = true;
        storeYields(id, till, p, daily * double(HarvestEnd - day % calendar::DaysPerYear) * FieldMargin - fieldHeld(till, p));
    }
}

void Society::tendFields()
{
    // Daily: each field's yields a day wanted (its running average set right for the days it has run, so a restart's
    // first days aren't counted short) into its year's rate (a slow average over the year, the model's year's sales, doc
    // 64, Phase 13) and its week's; at a week's end, the week's into its month's, set right for the weeks it has run.
    // (Not a day at a time: with a farm's caravan once a week, that still went a quarter over in its founding spring.)
    // A restart's first days aren't counted: its running average begun again from nothing, a farm sold to once a week
    // read its week's lump as a day's sales, and planned its spring and its year at up to 1.29 times them. Its week begins
    // again once its average has a week behind it (or, with no sales yet, the society has run a week).
    ++fieldPasses_;
    for (auto& [till, f] : state_.memory.fields)
    {
        const auto* job = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr;
        const auto* p = job ? items::producerFor(job->title) : nullptr;
        if (!p)
            continue;
        const auto days = salesDays_.find(till);
        if (days == salesDays_.end() ? fieldPasses_ <= 7 : days->second < 7)
        {
            f.week = 0;
            f.monthDays -= f.monthDays % 7;
            continue;
        }
        const double today = days == salesDays_.end() ? 0. : fieldNeed(till, *p) / (1 - std::pow(.7, days->second));
        f.rate += (today - f.rate) / calendar::DaysPerYear;
        f.week += today;
        if (++f.monthDays % 7 == 0)
        {
            const int weeks = std::min(f.monthDays / 7, 1000);
            f.month += (f.week / 7 - f.month) / FieldMonthWeeks / (1 - std::pow(1 - 1. / FieldMonthWeeks, weeks));
            f.week = 0;
        }
    }
}

bool Society::farmHire(const OddJob& j) const
{
    const auto* r = spec(j.producer);
    const auto* p = r ? items::producerFor(r->workLabel) : nullptr;
    return p && farmWork(p->id);
}

void Society::postFarmHires(std::int64_t day)
{
    // The cities: where anyone may come from to work a farm.
    std::map<std::string, std::size_t> folk;
    for (const auto& [id, life] : state_.residents)
        ++folk[communityOfResident(id)];
    cities_.clear();
    for (const auto& [community, n] : folk)
        if (n >= CityResidents && !community.empty())
            cities_.insert(community);
    // (With work by the hours, a farm hires as its sales want, at its weekly review: doc 64, Phase 6, RatwLabour.cpp.)
    if (workByHours())
        return;
    std::set<std::string> hiring;
    for (const auto& j : oddJobs_)
        if (j.until >= day && j.kind == "a hand")
            hiring.insert(j.producer);
    for (const auto& r : authored_.residents)
    {
        const auto* p = items::producerFor(r.workLabel);
        const auto* job = p && farmWork(p->id) && state_.residents.count(r.id) ? jobOf(r.id) : nullptr;
        const auto farm = tillOf(r.id);             // (Its till pays its hands: doc 46, Phase 2.)
        const auto* purse = job ? account(farm) : nullptr;
        if (!purse || hiring.count(r.id))
            continue;
        // For as long as its work wants: four to six days in its season, two or three out of it.
        const bool inSeason = p->seasons.empty() || std::find(p->seasons.begin(), p->seasons.end(), season_) != p->seasons.end();
        const auto h = std::hash<std::string>{}(r.id + "|hire|" + std::to_string(day));
        const int days = inSeason ? 4 + int(h % 3) : 2 + int(h % 2);
        const auto dayPay = std::int64_t(std::ceil(dayWage(communityOfResident(r.id), "hand") - 1e-9));
        // Only with the whole hire to spare, above a comfortable month's living.
        if (purse->cash - wealthLine(farm) < dayPay * days)
            continue;
        OddJob j;
        j.id = "odd" + std::to_string(++nextOddJob_);
        j.producer = r.id;
        j.payer = farm;
        j.community = communityOfResident(r.id);
        j.kind = "a hand";
        j.what = "a farmhand at " + job->title.substr(0, 40) + ", hired for " + std::to_string(days) + " days";
        j.from = j.to = job->work;
        j.slots = 1;
        j.pay = dayPay;
        j.forChildren = false;
        j.until = day + days - 1;
        oddJobs_.push_back(std::move(j));
        ++oddVersion_;
    }
}

void Society::lodgeHand(const std::string& id, const OddJob& job)
{
    // A hand from elsewhere (not the farm's own town) lodges in its bunkhouse, if there is one with a bed.
    if (job.until < 0 || state_.memory.lodging.count(id) || communityOfResident(id) == job.community)
        return;
    const auto bunk = bunkhouses_.find(job.producer);
    auto life = state_.residents.find(id);
    if (bunk == bunkhouses_.end() || life == state_.residents.end())
        return;
    int lodged = 0;
    for (const auto& [who, s] : state_.memory.lodging)
        lodged += readLodged(s).cell == bunk->second.cell;
    if (lodged >= BunkBeds)
        return;
    std::ostringstream home;
    home << life->second.homeCell << '|' << life->second.homeX << '|' << life->second.homeY << '|' << job.until;
    state_.memory.lodging[id] = home.str();
    life->second.homeCell = bunk->second.cell;
    life->second.homeX = bunk->second.x + .3 * (lodged % 5);
    life->second.homeY = bunk->second.y + .3 * (lodged / 5);
    openAccount(homeStore(bunk->second.cell, "larder"));
    ++state_.memory.revision;
    ++rosterRevision_;                              // (Its record keeps its home and larder.)
    record("lodged at the farm", id, bunk->second.cell, "", 0, 0);
}

void Society::endLodgings(std::int64_t day)
{
    // Home again when the hire is over (or gone: hires aren't saved, so after a restart too).
    std::set<std::string> working;
    for (const auto& j : oddJobs_)
        if (j.until >= day)
            for (const auto& [who, stage] : j.stage)
                working.insert(who);
    for (auto it = state_.memory.lodging.begin(); it != state_.memory.lodging.end();)
    {
        const auto l = readLodged(it->second);
        if (l.until >= day && working.count(it->first))
        {
            ++it;
            continue;
        }
        if (auto life = state_.residents.find(it->first); life != state_.residents.end())
        {
            life->second.homeCell = l.cell;
            life->second.homeX = l.x;
            life->second.homeY = l.y;
            record("home from the farm", it->first, l.cell, "", 0, 0);
        }
        it = state_.memory.lodging.erase(it);
        ++state_.memory.revision;
        ++rosterRevision_;
    }
}

void Society::stockBunkhouses(std::int64_t day)
{
    (void)day;
    if (state_.memory.lodging.empty())
        return;
    // Each bunkhouse's lodgers, and the farm that hired each.
    std::map<std::string, std::vector<std::string>> payers;    // Bunkhouse cell -> the farms paying for its lodgers.
    for (const auto& j : oddJobs_)
        if (j.until >= 0)
            for (const auto& [who, stage] : j.stage)
                if (const auto lodged = state_.memory.lodging.find(who); lodged != state_.memory.lodging.end())
                    if (const auto bunk = bunkhouses_.find(j.producer); bunk != bunkhouses_.end())
                        payers[bunk->second.cell].push_back(j.producer);
    std::map<std::string, std::vector<std::string>> shops;     // Community -> its food shops.
    for (const auto& r : authored_.residents)
        if (r.role == "merchant" && state_.residents.count(r.id) && foodShop(r.id))
            shops[communityOfResident(r.id)].push_back(r.id);
    for (const auto& [cell, farms] : payers)
    {
        const auto larder = homeStore(cell, "larder");
        openAccount(larder);
        // A day's food for each lodger (50), less what is there; each farm its share.
        int held = 0;
        for (const auto& [item, n] : account(larder)->stock)
            if (n > 0 && edible(item))
                held += n * nourishment(item);
        int wanted = 50 * int(farms.size()) - held;
        for (const auto& farm : farms)
        {
            if (wanted <= 0)
                break;
            // Its own food first (a rabbit farm's rabbits, a dairy's milk), then bought at its town's shops.
            const auto till = tillOf(farm);         // (The farm's till and yield: doc 46, Phase 2.)
            if (const auto* own = account(till))
                for (const auto& [item, n] : std::map<std::string, int>(own->stock.begin(), own->stock.end()))
                    if (wanted > 0 && n > 0 && edible(item))
                    {
                        const int give = std::min(n, (wanted + nourishment(item) - 1) / std::max(1, nourishment(item)));
                        if (give > 0 && shift(till, larder, item, give, 0, "food for the farmhands"))
                            wanted -= give * nourishment(item);
                    }
            if (wanted <= 0)
                break;
            std::map<std::string, int> got;
            buyForSurplus(till, shops[communityOfResident(farm)], [](const std::string& item) { return edible(item); },
                          std::max<std::int64_t>(2, wanted / 15), "food bought for the farmhands", &got);
            for (const auto& [item, n] : got)
                if (n > 0 && shift(till, larder, item, n, 0, "food for the farmhands"))
                    wanted -= n * nourishment(item);
        }
    }
}
} // namespace ratw
