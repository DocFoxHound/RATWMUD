// Households (the user, 2026-10-05; Docs/Design/42-money-in-circulation.md). The grown members of a home share one purse:
// once a day what they hold is evened out among them (a shopkeeper's purse, which is its shop's till, kept apart). A
// household that has stayed comfortable a week keeps one of its working members at home, to mind the children, do the
// shopping and look about; it goes back to work when the purse runs low. A household that has stayed poor a week sends
// everyone who can to work, those who kept the house too.
#include "RatwSociety.h"

#include <algorithm>

namespace ratw
{
bool Society::homemaking(const std::string& title)
{
    return title.find("keeps the house") != std::string::npos || title.find("keeping the house") != std::string::npos;
}

void Society::tendHouseholds(std::int64_t day, const std::map<std::string, LifeBody>& bodies)
{
    (void)day;
    auto& memory = state_.memory;
    std::map<std::string, std::vector<std::string>> homes, grown;
    for (const auto& [id, life] : state_.residents)
        if (const auto body = bodies.find(id); body != bodies.end() && !life.homeCell.empty())
        {
            homes[life.homeCell].push_back(id);
            if (body->second.age >= 16)
                grown[life.homeCell].push_back(id);
        }
    for (const auto& [home, members] : homes)
    {
        const auto adults = grown.find(home);
        if (adults == grown.end() || adults->second.empty())
            continue;
        // A barracks, a bunkhouse, quarters or lodgings house many who aren't a family: no shared purse, nobody kept home.
        if (adults->second.size() > 8 || home.find("barracks") != std::string::npos || home.find("bunkhouse") != std::string::npos ||
            home.find("quarters") != std::string::npos || home.find("watch_house") != std::string::npos ||
            home.find("lodging") != std::string::npos)
        {
            memory.keeper.erase(home);
            continue;
        }
        // The household purse: its grown members' money, a keeper's till aside, evened out among them.
        std::vector<std::string> sharing;
        std::int64_t pot = 0;
        for (const auto& id : adults->second)
        {
            const auto* r = spec(id);
            if (r && r->role == "merchant" && tillOf(id) == id)
                continue;
            if (const auto* a = account(id))
                pot += a->cash, sharing.push_back(id);
        }
        if (sharing.size() >= 2)
        {
            const auto each = pot / std::int64_t(sharing.size());
            for (const auto& giver : sharing)
                for (const auto& taker : sharing)
                {
                    const auto over = account(giver)->cash - each, under = each - account(taker)->cash;
                    if (giver != taker && over > 0 && under > 0)
                        shift(giver, taker, "", 0, std::min(over, under), "the household purse");
                }
        }
        // Comfortable or poor, and for how long.
        const auto need = householdNeed(members.size());
        auto& streak = memory.comfort[home];
        if (pot >= need * ComfortDays)
            streak = std::max(0, streak) + 1;
        else if (pot < need * PoorDays)
            streak = std::min(0, streak) - 1;
        else
            streak += streak > 0 ? -1 : streak < 0 ? 1 : 0;
        // Who keeps the house: after a week comfortable, one of two or more working grown members (not a shopkeeper, a
        // guard, the clergy or a house's head); back to work when the purse is under KeeperDays' food.
        const auto kept = memory.keeper.find(home);
        if (kept != memory.keeper.end() &&
            (pot < need * KeeperDays || !bodies.count(kept->second) || !state_.residents.count(kept->second)))
            memory.keeper.erase(kept);
        else if (kept == memory.keeper.end() && streak >= StreakDays)
        {
            std::vector<std::string> workers;
            for (const auto& id : adults->second)
                if (const auto* job = jobOf(id); job && job->paid && job->role == "civilian" && !clergy(job->title) &&
                                                  !houseHead(job->title) && bodies.at(id).age < RetireAge)
                    workers.push_back(id);
            if (workers.size() >= 2)
                memory.keeper[home] = workers.back();     // (The last by name: steady.)
        }
        // A household poor a week: everyone who can goes to work, those who kept the house too.
        if (streak <= -StreakDays)
            memory.toWork.insert(home);
        else if (streak >= 0)
            memory.toWork.erase(home);
    }
    ++memory.revision;
}
} // namespace ratw
