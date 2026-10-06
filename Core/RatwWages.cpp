// Who pays whom (Docs/Design/42-money-in-circulation.md, Phase 2). A position is paid by whoever it works for: a
// shop's help by its keeper, the watch and the town's own by the town, the clergy by the church, a great house's staff
// by its head. Those who bring goods in from the land live by selling them, and children draw no wages.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <unordered_map>
#include <utility>
#include <vector>

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
// A hand at a trade's works (at the forge, working the saws, smoking fish): the words of its title, and the kinds of shop
// that pay it (payerOf).
const std::vector<std::pair<const char*, std::vector<const char*>>> trades = {
    {"forge", {"smithy", "ironworks", "foundry"}}, {"saws", {"sawmill", "carpenter"}}, {"smoking fish", {"smokehouse", "fishmonger"}},
    {"rope", {"ropewalk", "chandlery"}}, {"glass", {"glassworks"}}, {"kiln", {"brickworks", "pottery"}}, {"salt", {"saltworks"}},
    {"tann", {"tannery"}}, {"brew", {"brewery"}}, {"dye", {"dyeworks"}}, {"ships", {}}, {"mill", {"mill"}}, {"cooper", {"cooperage"}},
    {"the catch", {"fishmonger", "smokehouse"}}, {"foreman", {}}, {"overseeing", {}}};
// What a position's title alone says of who pays it (payerOf), worked out once a title: each thread remembers.
struct TitleWages
{
    bool household = false;                         // Work nobody pays a wage for.
    bool church = false;                            // The clergy, and those who tend the sick.
    int trade = -1;                                 // The first of `trades` its title names, or -1.
};
const TitleWages& titleWages(const std::string& title)
{
    thread_local std::unordered_map<std::string, TitleWages> known;
    if (const auto found = known.find(title); found != known.end())
        return found->second;
    TitleWages w;
    const auto t = lower(title);
    w.household = hasAny(t, {"keeps the house", "keeping the house", "apprenticed", "sells from", "picks through", "sits ", "sits by",
                             "telling stories", "plays", "beg"});
    w.church = Society::clergy(title) || hasAny(t, {"the sick", "the hurt"});
    for (std::size_t i = 0; i < trades.size() && w.trade < 0; ++i)
        if (t.find(trades[i].first) != std::string::npos)
            w.trade = int(i);
    return known.emplace(title, w).first->second;
}
} // namespace

bool Society::clergy(const std::string& title)
{
    thread_local std::unordered_map<std::string, bool> known;      // (Asked of the same titles every decision.)
    if (const auto found = known.find(title); found != known.end())
        return found->second;
    const bool is = hasAny(lower(title), {"chapel", "priest", "acolyte", "cathedral", "choir", "prelate", "shrine", "chaplain",
                                          "sexton", "deacon", "abbey", "temple", "church", "the mass", "confession", "the crypt",
                                          "healer"});
    return known.emplace(title, is).first->second;
}

bool Society::houseHead(const std::string& title)
{
    // The head of a great house (a lord, a lady, a hall's keeper): unpaid, living on the house's own money.
    thread_local std::unordered_map<std::string, bool> known;      // (As clergy.)
    if (const auto found = known.find(title); found != known.end())
        return found->second;
    const auto t = lower(title);
    const bool is = t.rfind("ruling ", 0) == 0 || t.rfind("holding court", 0) == 0 ||
                    (t.rfind("keeping ", 0) == 0 && hasAny(title, {" Hall", " House", " Manor"}));
    return known.emplace(title, is).first->second;
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
    // A fortress's garrison is the crown's: paid from the capital's treasury, not the fortress's own small purse.
    const auto town = communityOfResident(resident);
    const bool fortress = town.find("fortress") != std::string::npos;
    if (job.role == "guard")
        return {fortress ? std::string("treasury") : treasuryOfResident(resident), "the town"};
    if (outworkTitled(job.title))
        return {"", "lives by what it brings in"};
    if (job.title == LabourTitle)
        return {treasuryOfResident(resident), "the Town Works"};
    if (const auto* r = spec(resident); r && items::producerFor(r->workLabel))
        return {"", "lives by what it brings in"};
    const auto& said = titleWages(job.title);
    // Work nobody pays a wage for (doc 42, 2026-10-05): keeping one's own house, learning a trade, hawking, scavenging,
    // sitting by the well. The household keeps them, or they live by what they sell.
    if (said.household)
        return {"", "lives on the household"};
    if (said.church)
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
    // A household's servant ("serving the household", "keeping the chambers"): paid by the richest of the household
    // whose home it works in.
    if (const auto own = state_.residents.find(resident); own == state_.residents.end() || own->second.homeCell != job.work.cell)
        if (const auto* master = richestAt(job.work.cell, resident))
            return {*master, "the house"};
    // A hand at a trade's works (at the forge, working the saws, smoking fish): paid by a keeper of that trade in its
    // town, or else by its town's richest great house, who own the industry.
    if (said.trade >= 0)
    {
        const auto& kinds = trades[std::size_t(said.trade)].second;
        for (const auto& p : positions_)
        {
            if (p.role != "merchant" || communityOfResident(p.founder) != town)
                continue;
            const auto* business = items::businessFor(p.title);
            const auto held = state_.careers.positions.find(p.id);
            if (business && std::find(kinds.begin(), kinds.end(), business->id) != kinds.end() &&
                held != state_.careers.positions.end() && !held->second.holder.empty())
                return {held->second.holder, "the shop"};
        }
        std::string house;
        std::int64_t most = -1;
        for (const auto& h : houses())
            if (const auto* purse = account(h.id); purse && h.community == town && purse->cash > most)
                most = purse->cash, house = h.id;
        if (!house.empty())
            return {house, "the house"};
    }
    return {fortress ? std::string("treasury") : treasuryOfResident(resident), "the town"};
}

std::int64_t Society::wageFor(const Payer& payer) const
{
    const auto* purse = account(payer.account);
    if (!purse)
        return 0;
    const bool business = payer.whom == "the shop" || payer.whom == "the house";
    const bool treasury = payer.account.rfind("stores:", 0) == 0 || payer.account == "treasury";
    if (business ? purse->cash <= ComfortableTill : treasury && treasuryLean(payer.account))
        return 1;                                   // (A lean one pays half: doc 42.)
    // A town pays by what it takes in (the user, 2026-10-06): 2p a spell scaled so its wage bill follows its takings.
    if (treasury)
        if (const auto scale = townWageScale_.find(payer.account); scale != townWageScale_.end())
            return std::clamp<std::int64_t>(std::int64_t(std::lround(2 * scale->second)), 1, MostWage);
    std::int64_t floor = 0;
    if (payer.account.rfind("house:", 0) == 0)
        floor = houseFloor(payer.account);
    else if (payer.account.rfind("till:", 0) == 0)
        floor = floatOf(payer.account.substr(5));   // (A house's business: "till:" and its position.)
    else if (const auto* job = jobOf(payer.account); business && job)
        floor = floatOf(job->id);
    else if (treasury)
        floor = TreasuryHead * std::max(1, people_.count(payer.account) ? people_.at(payer.account) : 0);
    else if (payer.account == SharedChurch)
        floor = ChurchHead * std::int64_t(std::max<std::size_t>(1, state_.residents.size()));
    if (floor <= 0)
        return 2;
    auto times = purse->cash / floor;
    // (TRIAL wages_up: a great house's shop pays by what the house holds against its floor, if more than its till
    // does, the house standing behind its shops' wages; and the richest pay up to twice MostWage.)
    const bool up = trial("wages_up");
    if (up && payer.account.rfind("till:", 0) == 0)
        if (const auto owner = state_.houses.owner.find(payer.account.substr(5)); owner != state_.houses.owner.end())
            if (const auto* house = account(owner->second))
                times = std::max(times, house->cash / std::max<std::int64_t>(1, houseFloor(owner->second)));
    return std::clamp<std::int64_t>(1 + times, 2, up ? 2 * MostWage : MostWage);
}

const std::string* Society::richestAt(const std::string& homeCell, const std::string& besides) const
{
    // The household living in a cell, by its richest grown member (cached for the day).
    if (richestDay_ != state_.budgetDay)
    {
        richest_.clear();
        richestDay_ = state_.budgetDay;
        std::map<std::string, std::int64_t> most;
        for (const auto& [id, life] : state_.residents)
            if (const auto* purse = account(id); purse && !life.homeCell.empty() && (!most.count(life.homeCell) || purse->cash > most[life.homeCell]))
                most[life.homeCell] = purse->cash, richest_[life.homeCell] = id;
    }
    // The room itself, or the house it is a room of (a palazzo's salon below its family's chambers: "villa_x",
    // "villa_x_up").
    for (const auto& cell : {homeCell, homeCell + "_up", homeCell.size() > 3 && homeCell.compare(homeCell.size() - 3, 3, "_up") == 0
                                                            ? homeCell.substr(0, homeCell.size() - 3) : std::string()})
        if (const auto found = richest_.find(cell); !cell.empty() && found != richest_.end() && found->second != besides)
            return &found->second;
    return nullptr;
}

bool Society::treasuryLean(const std::string& treasury) const
{
    // Under LeanTreasury pennies a head of its town (cached for the day): it pays its posts half (doc 42).
    if (peopleDay_ != state_.budgetDay)
    {
        people_.clear();
        peopleDay_ = state_.budgetDay;
        for (const auto& [id, life] : state_.residents)
            ++people_[treasuryOfResident(id)];
    }
    const auto* purse = account(treasury);
    const auto n = people_.count(treasury) ? people_.at(treasury) : 0;
    return !purse || purse->cash < std::int64_t(LeanTreasury) * n;
}
} // namespace ratw
