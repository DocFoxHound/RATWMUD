// Farmhands (Docs/Design/42-money-in-circulation.md, "Farmhands"; the user, 2026-10-06: spread the farms' money out).
// Every farm, orchard and vineyard with money to spare hires a hand for a few days (longer in its season), at the going
// hire pay (Society::hirePay_, rising while the place goes unfilled). Its town's folk may take it, and so may anyone living
// in a city; one from elsewhere lodges in the farm's bunkhouse for the hire (up to BunkBeds), its home there meanwhile,
// eating from the bunkhouse larder, which the farm stocks from its own food first, then from the town's shops. When the
// hire ends the hand goes home. All daily, in the day's pass: nothing here runs per tick.
#include "RatwSociety.h"

#include "RatwItems.h"

#include <algorithm>
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
        const auto dayPay = hirePay_.try_emplace(r.id, HirePay).first->second;
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
