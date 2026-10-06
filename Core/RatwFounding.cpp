// Starting money (the user, 2026-10-05: "everyone needs to have starting money"). Every resident, and every body that
// keeps a purse (a town's treasury, its church and its buyers, a great house and its businesses' tills), begins with a
// reasonable one. It is made once, when the world is founded (or, for an older save, the first time it runs with this),
// counted as made like the world's first treasury, and kept out of the month's profit, so nobody is taxed on it. The
// amounts are placeholders for the balance pass (RatwSociety.h).
#include "RatwSociety.h"

#include <algorithm>
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
    return r && r->role == "merchant" ? std::max<std::int64_t>(0, purse->cash - KeeperReserve) : purse->cash;
}

void Society::startingMoney(const std::string& id, std::int64_t coins)
{
    const auto found = state_.accounts.find(id);
    if (coins <= 0 || found == state_.accounts.end())
        return;
    found->second.cash += coins;
    state_.minted += coins;
    record("starting money", "outside", id, "", 0, coins);
}

void Society::foundPurses(const std::map<std::string, LifeBody>& bodies)
{
    auto& memory = state_.memory;
    if (memory.purses >= 1)
        return;
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

void Society::worldMoney(const std::map<std::string, LifeBody>& bodies)
{
    // After the day's pass, so the town's buyers and the church have been founded with theirs.
    if (state_.memory.purses != 1)
        return;
    state_.memory.purses = 2;
    ++state_.memory.revision;
    std::map<std::string, std::int64_t> heads;      // Treasury -> the residents who pay it.
    std::vector<std::string> grown;
    for (const auto& [id, life] : state_.residents)
    {
        const auto body = bodies.find(id);
        const auto* r = spec(id);
        if ((body != bodies.end() ? body->second.age : r ? r->age : 30) >= 16 && account(id))
            grown.push_back(id);
        ++heads[treasuryOfResident(id)];
    }
    const auto gap = WorldMoney - moneySupply();
    if (gap <= 0 || grown.empty() || state_.residents.size() < WorldMoneyResidents)
        return;
    std::int64_t everyone = 0;
    for (const auto& [treasury, n] : heads)
        everyone += account(treasury) ? n : 0;
    const auto toGrown = everyone > 0 ? gap * WorldGrownShare / 100 : gap;
    const auto each = toGrown / std::int64_t(grown.size());
    for (const auto& id : grown)
        startingMoney(id, each);
    // The rest (and the pennies the grown didn't split) to the treasuries, by their residents; the last takes the remainder.
    auto left = gap - each * std::int64_t(grown.size());
    for (auto it = heads.begin(); it != heads.end() && everyone > 0; ++it)
        if (account(it->first))
        {
            const auto share = left * it->second / everyone;
            startingMoney(it->first, std::next(it) == heads.end() ? left : share);
            left -= share;
            everyone -= it->second;
        }
}
} // namespace ratw
