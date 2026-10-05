// Who pays whom (Docs/Design/42-money-in-circulation.md, Phase 2). A position is paid by whoever it works for: a
// shop's help by its keeper, the watch and the town's own by the town, the clergy by the church, a great house's staff
// by its head. Those who bring goods in from the land live by selling them, and children draw no wages.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cctype>

namespace ratw
{
namespace
{
std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}
bool hasAny(const std::string& text, std::initializer_list<const char*> words)
{
    for (const char* w : words)
        if (text.find(w) != std::string::npos)
            return true;
    return false;
}
} // namespace

bool Society::clergy(const std::string& title)
{
    return hasAny(lower(title), {"chapel", "priest", "acolyte", "cathedral", "choir", "prelate", "shrine", "chaplain", "sexton",
                                 "deacon", "abbey", "temple", "church"});
}

bool Society::houseHead(const std::string& title)
{
    // The head of a great house (a lord, a lady, a hall's keeper): unpaid, living on the house's own money.
    const auto t = lower(title);
    return t.rfind("ruling ", 0) == 0 || t.rfind("holding court", 0) == 0 ||
           (t.rfind("keeping ", 0) == 0 && hasAny(title, {" Hall", " House", " Manor"}));
}

void Society::indexEmployers() const
{
    if (!employers_.empty() || positions_.empty())
        return;
    for (const auto& p : positions_)
        if (p.role == "merchant" || (!p.paid && p.role == "civilian" && houseHead(p.title)))
            employers_[p.work.cell].push_back(p.id);
    employers_[""];                                  // (Indexed, even with no employers at all.)
}

Society::Payer Society::payerOf(const std::string& resident, const Position& job, int age) const
{
    if (age < 16)
        return {"", "a child"};
    if (job.role == "guard")
        return {treasuryOfResident(resident), "the town"};
    if (outworkTitled(job.title))
        return {"", "lives by what it brings in"};
    if (job.title == LabourTitle)
        return {treasuryOfResident(resident), "the Town Works"};
    if (const auto* r = spec(resident); r && items::producerFor(r->workLabel))
        return {"", "lives by what it brings in"};
    if (clergy(job.title))
        return {churchOf(treasuryOfResident(resident)), "the church"};
    // Someone with a shop, or a great house, where it works: the first of them there who isn't itself.
    indexEmployers();
    if (const auto found = employers_.find(job.work.cell); found != employers_.end())
        for (const auto& pid : found->second)
        {
            const auto held = state_.careers.positions.find(pid);
            if (pid == job.id || held == state_.careers.positions.end() || held->second.holder.empty() ||
                held->second.holder == resident || !account(held->second.holder))
                continue;
            const auto* employer = position(pid);
            return {held->second.holder, employer && employer->role == "merchant" ? "the shop" : "the house"};
        }
    return {treasuryOfResident(resident), "the town"};
}
} // namespace ratw
