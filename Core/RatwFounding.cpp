// Starting money (the user, 2026-10-05: "everyone needs to have starting money"). Every resident, and every body that
// keeps a purse (a town's treasury, its church and its buyers, a great house and its businesses' tills), begins with a
// reasonable one. It is made once, when the world is founded (or, for an older save, the first time it runs with this),
// counted as made like the world's first treasury, and kept out of the month's profit, so nobody is taxed on it. The
// amounts are placeholders for the balance pass (RatwSociety.h).
#include "RatwSociety.h"

#include "RatwItems.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace ratw
{
std::int64_t Society::startingPurse(const std::string& id, int age) const
{
    if (age < 16)
        return ChildPurse + age / 2;
    const auto* job = jobOf(id);
    if (!job)
        return age >= RetireAge ? ElderPurse : AdultPurse;
    if (job->role == "merchant")
        return tillOf(id) == id ? std::max<std::int64_t>(KeeperPurse, floatOf(job->id)) : AdultPurse;
    const auto& t = job->title;
    if (t.find("begg") != std::string::npos || t.find("rags") != std::string::npos || t.find("scaveng") != std::string::npos)
        return PoorPurse;
    return AdultPurse;
}

std::int64_t Society::spendable(const std::string& id) const
{
    const auto* purse = account(id);
    if (!purse)
        return 0;
    const auto* r = state_.residents.count(id) ? spec(id) : nullptr;
    // (Only a keeper whose purse is its shop's till keeps back its food money: one with a till of its own spends its own.
    // At its town's day, accountTown, as the wealth line and an owner's top-up reckon it.)
    return r && r->role == "merchant" && tillOf(id) == id ? std::max<std::int64_t>(0, purse->cash - pennies(KeeperReserve, accountTown(id)))
                                                         : purse->cash;
}

void Society::startingMoney(const std::string& id, std::int64_t coins)
{
    const auto found = state_.accounts.find(id);
    if (coins <= 0 || found == state_.accounts.end())
        return;
    // Once the realm's money stands (doc 64, Phase 1), nothing more is made: a purse for something new (a town's buyers
    // or church opened late) comes from its treasury, as far as the treasury has it.
    if (state_.memory.purses >= PursesFounded)
    {
        // (A town's account names its town: "town:<it>:works", "stores:<it>".)
        std::string town = capital();
        if (id.rfind("town:", 0) == 0 && id.find(':', 5) != std::string::npos)
            town = id.substr(5, id.find(':', 5) - 5);
        else if (id.rfind("stores:", 0) == 0)
            town = id.substr(7);
        const auto treasury = treasuryOf(town);
        if (treasury != id)
            shift(treasury, id, "", 0, std::min(coins, account(treasury) ? account(treasury)->cash : 0), "a new purse from the treasury");
        return;
    }
    found->second.cash += coins;
    state_.minted += coins;
    record("starting money", "outside", id, "", 0, coins);
}

void Society::foundWage(const std::map<std::string, LifeBody>* bodies)
{
    auto& memory = state_.memory;
    if (memory.purses >= 1 || (state_.residents.size() < WorldMoneyResidents && !foundAnyLand_))
        return;
    ++memory.revision;
    const double day = items::ladder().foundingDay;
    // (An older save's employers' books, founding its purses now: each wage by its town's rate to the founding's, so
    // they stand as far above or below it as they stood, and the first measure keeps the day. Each employer's town from
    // its posts, as the labour market finds it: its own record of them is made at its day, none yet in a save just
    // loaded: the price review.)
    std::map<std::string, std::string> townOf;
    if (!memory.employers.empty())
        for (const auto& p : positions_)
        {
            const auto held = state_.careers.positions.find(p.id);
            const auto holder = held == state_.careers.positions.end() ? std::string() : held->second.holder;
            // (Its holder's age as tendLabour takes it: its body's, else its spec's. Round 6, the tidy's review.)
            int age = 30;
            if (const auto* r = holder.empty() ? nullptr : spec(holder))
                age = r->age;
            if (bodies && !holder.empty())
                if (const auto b = bodies->find(holder); b != bodies->end())
                    age = b->second.age;
            if (const auto key = postEmployer(p, holder, age); !key.empty())
                townOf[key] = communityOfResident(p.founder);
        }
    for (auto& [key, book] : memory.employers)
    {
        const auto town = townOf.find(key);
        if (const double was = goingRate(town != townOf.end() ? town->second : employerTownOf(key)); book.wage > 0 && was > 0)
            book.wage *= day / was;
    }
    for (auto& [town, rate] : memory.going)
        rate = day;
    memory.going[""] = day;
    memory.foundedDay = day;
}

void Society::foundPurses(const std::map<std::string, LifeBody>& bodies)
{
    auto& memory = state_.memory;
    if (memory.purses >= 1)
        return;
    foundWage(&bodies);                             // (Already, for a world founded afresh: resetAuthored.)
    memory.purses = 1;
    ++memory.revision;
    std::map<std::string, std::int64_t> heads;      // Treasury -> the residents who pay it.
    for (const auto& [id, life] : state_.residents)
    {
        const auto body = bodies.find(id);
        const auto* r = spec(id);
        const int age = body != bodies.end() ? body->second.age : r ? r->age : 30;
        if (const auto* purse = account(id))
            startingMoney(id, startingPurse(id, age) - purse->cash);
        ++heads[treasuryOfResident(id)];
    }
    std::int64_t everyone = 0;
    for (const auto& [treasury, n] : heads)
    {
        if (const auto* purse = account(treasury))
            startingMoney(treasury, TreasuryHead * n - purse->cash);
        everyone += n;
    }
    // An older save's church, founded already (a new world's is founded with its own, RatwDemand.cpp): one purse for the
    // land, ChurchHead a resident.
    if (const auto* church = account(SharedChurch))
        startingMoney(SharedChurch, ChurchHead * everyone - church->cash);
    // An older save's great houses and their businesses (a new world's are founded with theirs, RatwHouses.cpp).
    for (const auto& h : houses())
        if (const auto* purse = account(h.id))
            startingMoney(h.id, HouseFortune - purse->cash);
    for (const auto& [pid, house] : state_.houses.owner)
        if (const auto* till = account("till:" + pid))
            startingMoney("till:" + pid, floatOf(pid) - till->cash);
}

namespace
{
// A household's lot, the same every run: an even spread of 0..1 by the resident's id, made exponential, as wealth runs
// (doc 63, "What economics says": random trade alone spreads money so).
double lot(const std::string& id)
{
    std::uint64_t h = 1469598103934665603ULL;
    for (const unsigned char c : id)
        h = (h ^ c) * 1099511628211ULL;
    const double u = (double(h % 1000000) + 0.5) / 1000000.0;
    return -std::log(u);
}

// `coins` shared among `weights` by weight; the last takes the pennies the shares didn't split.
template <typename Give>
void share(std::int64_t coins, const std::vector<std::pair<std::string, double>>& weights, Give give)
{
    double total = 0;
    for (const auto& [id, w] : weights)
        total += w;
    if (coins <= 0 || total <= 0)
        return;
    std::int64_t left = coins;
    for (std::size_t i = 0; i < weights.size(); ++i)
    {
        const auto part = i + 1 == weights.size() ? left : std::int64_t(double(coins) * weights[i].second / total);
        if (part > 0)
            give(weights[i].first, part);
        left -= part;
    }
}
} // namespace

void Society::worldMoney(const std::map<std::string, LifeBody>& bodies)
{
    // After the day's pass, so the town's buyers and the church have their purses.
    if (state_.memory.purses != 1)
        return;
    ++state_.memory.revision;
    // (The realm's money stands once this is done: marked at the end, since until then its purses are made, not drawn
    // from a treasury. See startingMoney.)
    struct Stands
    {
        Society& society;
        ~Stands() { society.state_.memory.purses = PursesFounded; }
    } stands{*this};
    if (state_.residents.size() < WorldMoneyResidents)
        return;
    const auto gap = WorldMoney - moneySupply();
    if (gap <= 0)
        return;
    // Who holds what now, by kind, and how each kind's share is spread among its own.
    std::vector<std::pair<std::string, double>> homes, businesses, houses, treasuries;
    std::int64_t heldHomes = 0, heldBusinesses = 0, heldHouses = 0, heldChurch = 0, heldTreasuries = 0;
    std::map<std::string, double> heads;              // Treasury -> its residents.
    for (const auto& [id, life] : state_.residents)
    {
        const auto* purse = account(id);
        if (!purse)
            continue;
        ++heads[treasuryOfResident(id)];
        if (merchant(id) && tillOf(id) == id)
        {
            heldBusinesses += purse->cash;           // A keeper whose purse is its shop's till: a business's money.
            if (const auto* job = jobOf(id))
                businesses.push_back({id, double(std::max<std::int64_t>(1, floatOf(job->id)))});
            continue;
        }
        heldHomes += purse->cash;
        const auto body = bodies.find(id);
        const auto* r = spec(id);
        if ((body != bodies.end() ? body->second.age : r ? r->age : 30) >= 16)
            homes.push_back({id, lot(id)});
    }
    std::map<std::string, double> owned;              // House -> the businesses it owns.
    for (const auto& [pid, house] : state_.houses.owner)
        owned[house] += 1;
    for (const auto& [id, a] : state_.accounts)
    {
        if (id.rfind("till:", 0) == 0)
        {
            heldBusinesses += a.cash;
            businesses.push_back({id, double(std::max<std::int64_t>(1, floatOf(id.substr(5))))});
        }
        else if (id.rfind("house:", 0) == 0)
        {
            heldHouses += a.cash;
            houses.push_back({id, 1 + owned[id]});
        }
        else if (id == SharedChurch)
            heldChurch += a.cash;
        else if (id == "treasury" || id.rfind("stores:", 0) == 0)
        {
            heldTreasuries += a.cash;
            if (heads.count(id))
                treasuries.push_back({id, heads.at(id)});
        }
    }
    const auto want = [&](int percent, std::int64_t held) {
        return std::max<std::int64_t>(0, WorldMoney * percent / 100 - held);
    };
    const int treasuryShare = 100 - WorldHomesShare - WorldBusinessShare - WorldHousesShare - WorldChurchShare;
    std::vector<std::int64_t> wants = {want(WorldHomesShare, heldHomes), want(WorldBusinessShare, heldBusinesses),
                                       want(WorldHousesShare, heldHouses), want(WorldChurchShare, heldChurch),
                                       want(treasuryShare, heldTreasuries)};
    if (homes.empty())
        wants[0] = 0;
    if (businesses.empty())
        wants[1] = 0;
    if (houses.empty())
        wants[2] = 0;
    if (!account(SharedChurch))
        wants[3] = 0;
    if (treasuries.empty())
        wants[4] = 0;
    std::int64_t wanted = 0;
    for (const auto w : wants)
        wanted += w;
    if (wanted <= 0)
        return;
    // What each kind gets: its want, scaled to the gap if the wants come to more (some kind already held over its
    // share); the treasuries take the pennies the scaling drops.
    std::vector<std::int64_t> gets(wants.size());
    std::int64_t given = 0;
    for (std::size_t i = 0; i < wants.size(); ++i)
        given += gets[i] = wanted > gap ? std::int64_t(double(wants[i]) * double(gap) / double(wanted)) : wants[i];
    gets[treasuries.empty() ? 0 : 4] += std::max<std::int64_t>(0, std::min(gap, wanted) - given);
    const auto give = [&](const std::string& id, std::int64_t coins) { startingMoney(id, coins); };
    share(gets[0], homes, give);
    share(gets[1], businesses, give);
    share(gets[2], houses, give);
    if (gets[3] > 0)
        startingMoney(SharedChurch, gets[3]);
    share(gets[4], treasuries, give);
}
} // namespace ratw
