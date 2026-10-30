// Households (the user, 2026-10-05; Docs/Design/42-money-in-circulation.md). The grown members of a home share one purse:
// once a day what they hold is evened out among them (a shopkeeper's purse, which is its shop's till, kept apart). A
// household that has stayed comfortable a week keeps one of its working members at home, to mind the children, do the
// shopping and look about; it goes back to work when the purse runs low. A household that has stayed poor a week sends
// everyone who can to work, those who kept the house too.
#include "RatwSociety.h"

#include <algorithm>

namespace ratw
{
bool Society::communalHome(const std::string& home)
{
    return home.find("barracks") != std::string::npos || home.find("bunkhouse") != std::string::npos ||
           home.find("quarters") != std::string::npos || home.find("watch_house") != std::string::npos ||
           home.find("lodging") != std::string::npos;
}

bool Society::homemaking(const std::string& title)
{
    return title.find("keeps the house") != std::string::npos || title.find("keeping the house") != std::string::npos;
}

void Society::tendHouseholds(std::int64_t day, const std::map<std::string, LifeBody>& bodies)
{
    auto& memory = state_.memory;
    std::map<std::string, std::vector<std::string>> homes, grown;
    for (const auto& [id, life] : state_.residents)
        if (const auto body = bodies.find(id); body != bodies.end() && !life.homeCell.empty())
        {
            homes[life.homeCell].push_back(id);
            if (body->second.age >= 16)
                grown[life.homeCell].push_back(id);
        }
    // Each town's homes, and those under PoorDays of food money, by community (round 13, lane 2: SurplusByNeed's need; a
    // measure, so counted whatever the rules: it moves nothing). A family home by its shared purse; in a barracks, a
    // bunkhouse, quarters or lodgings each grown wolf by its own; a home whose only purse is a keeper's till isn't counted.
    std::map<std::string, std::pair<int, int>> poorHomes;     // Community -> homes poor, homes.
    const auto count = [&](const std::string& community, std::int64_t pot, std::int64_t need) {
        auto& [poor, all] = poorHomes[community];
        ++all;
        poor += pot < need * PoorDays;
    };
    // Children kept (round 13, lane 2; ChildrenKept; the user, 2026-10-29), with work by the hours: a child of a family
    // home with a grown wolf sharing its purse, not apprenticed nor holding a paid post, keeps ChildAllowance (a week of
    // the largest stipend) and passes the rest to the household's purse, its poorest sharing member, so the home's pot
    // counts it (its wants, its help, its holdings tax). In round 12's run the babies took hires from their first day
    // (a newborn stood "growing up", so out of work, its age under a child's eight) and the 53 born_ held 343,908p on day
    // 729, 280k of the 711k the city's top tenth of homes held. Odd jobs from ChildWorkAge only: Society::oddJobFor.
    const bool childrenKept = workByHours() && wageRule(ChildrenKept);
    for (const auto& [home, members] : homes)
    {
        const auto adults = grown.find(home);
        if (adults == grown.end() || adults->second.empty())
            continue;
        // A barracks, a bunkhouse, quarters or lodgings house many who aren't a family: no shared purse, nobody kept home.
        if (adults->second.size() > 8 || communalHome(home))
        {
            memory.keeper.erase(home);
            // Each its own household, by its own purse: the rule all the same, or its purse only fills (a bunkhouse's
            // farmers and loggers held half the households' money by day 60, doc 64, "The households' gap").
            for (const auto& id : adults->second)
                if (!(keepsShop(id) && tillOf(id) == id))   // (A keeper whose purse is its till; its post's, TradeByPost.)
                    if (const auto* a = account(id))
                    {
                        const auto community = communityOfResident(id);
                        count(community, a->cash, householdNeed(1, community));
                        householdRule(id, {id}, a->cash, day, true);
                    }
            continue;
        }
        // The household purse: its grown members' money, a keeper's till aside, evened out among them.
        std::vector<std::string> sharing;
        std::int64_t pot = 0;
        for (const auto& id : adults->second)
        {
            if (keepsShop(id) && tillOf(id) == id)   // (Its post's, TradeByPost.)
                continue;
            if (const auto* a = account(id))
                pot += a->cash, sharing.push_back(id);
        }
        if (childrenKept && !sharing.empty())
        {
            const auto allowance = childAllowance(communityOfResident(members.front()));
            for (const auto& id : members)
            {
                const auto body = bodies.find(id);
                const auto* job = jobOf(id);
                const auto* a = account(id);
                if (body == bodies.end() || body->second.age >= 16 || !a || a->cash <= allowance || apprenticedTo(id) || (job && job->paid))
                    continue;
                const auto poorest = *std::min_element(sharing.begin(), sharing.end(), [&](const std::string& x, const std::string& y) {
                    return account(x)->cash < account(y)->cash;
                });
                if (const auto excess = a->cash - allowance; shift(id, poorest, "", 0, excess, "the household purse"))
                    pot += excess;
            }
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
        // The household rule (doc 64, Phase 7): its want fund, help about the home, servants.
        householdRule(home, members, pot, day);
        // Comfortable or poor, and for how long.
        const auto need = householdNeed(members.size(), communityOfResident(members.front()));
        if (!sharing.empty())
            count(communityOfResident(members.front()), pot, need);
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
    poorShare_.clear();
    for (const auto& [community, homesPoor] : poorHomes)
        if (homesPoor.second > 0)
            poorShare_[community] = double(homesPoor.first) / homesPoor.second;
    ++memory.revision;
}
} // namespace ratw
