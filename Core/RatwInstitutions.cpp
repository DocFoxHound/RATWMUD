// Households and institutions (Docs/Design/64-economy-migration.md, Phase 7; doc 63's model): income, tax and tithe on
// it, the household rule, institutions' weekly budgets, daily relief and bakers' leftovers. See RatwSociety.h.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace ratw
{
namespace
{
double lot(const std::string& words)
{
    return double(std::hash<std::string>{}(words) % 100000) / 100000;
}
bool townAccount(const std::string& id)
{
    return id == "treasury" || id.rfind("stores:", 0) == 0;
}
bool building(const std::string& item)
{
    return item == "bricks" || item == "planks" || item == "nails" || item == "stone" || item == "roof_tiles" || item == "timber" ||
           item == "lime" || item == "fittings" || item == "plaster";
}
bool finery(const std::string& item)
{
    const auto* g = items::good(item);
    const auto c = g ? g->category : std::string();
    return c == "jewelry" || c == "luxury" || c == "hat" || c == "scarf" || c == "shawl";
}
}

bool Society::unearnedKind(const std::string& kind)
{
    return kind == "inheritance" || kind == "operator transfer" || kind == "the shop's till" || kind == "sale of a business" ||
           kind == "starting money" || kind == "a child's first pennies" || kind == "a child's stipend" || kind == "the household purse" ||
           kind == "the church's dole" || kind == "the church's share" || kind == "a gift" || kind == "a letter's enclosure" ||
           kind == "an owner's investment" || kind == "relief" || kind.rfind("test:", 0) == 0;   // (A test's money, put there.)
}

double Society::monthIncomeOf(const std::string& account) const
{
    const auto found = state_.memory.monthIncome.find(account);
    return found == state_.memory.monthIncome.end() ? 0 : found->second;
}

void Society::tendIncome()
{
    auto& memory = state_.memory;
    for (auto it = memory.monthIncome.begin(); it != memory.monthIncome.end();)
    {
        it->second *= double(MonthDays - 1) / MonthDays;
        it = it->second < .5 && !earnedToday_.count(it->first) ? memory.monthIncome.erase(it) : std::next(it);
    }
    flushEarnings();
}

void Society::flushEarnings()
{
    auto& memory = state_.memory;
    for (const auto& [id, coins] : earnedToday_)
    {
        memory.monthIncome[id] += coins;
        memory.weekIncome[id] += coins;
    }
    earnedToday_.clear();
}

Spot Society::townSpot(const std::string& community, const std::string& salt) const
{
    // Its square, or else before one of its shops.
    if (const auto plan = day_.plans.find(community); plan != day_.plans.end() && !plan->second.crowd.empty())
        return plan->second.crowd[std::hash<std::string>{}(salt) % plan->second.crowd.size()];
    std::vector<Spot> about;
    for (const auto& p : positions_)
        if (p.role == "merchant" && communityOfResident(p.founder) == community)
            about.push_back(p.serve);
    return about.empty() ? Spot{} : about[std::hash<std::string>{}(salt) % about.size()];
}

std::int64_t Society::hireForWeek(const std::string& payer, const std::string& community, const std::string& kind, const std::string& what,
                                  const Spot& at, int slots, std::int64_t wage, int days, std::int64_t day)
{
    if (slots < 1 || wage < 1 || at.cell.empty() || !account(payer) || spendable(payer) < wage * slots)
        return 0;
    OddJob j;
    j.id = "odd" + std::to_string(++nextOddJob_);
    j.payer = payer;
    j.community = community;
    j.kind = kind;
    j.what = what.substr(0, 80);
    j.from = j.to = at;
    j.slots = std::min(slots, 12);
    j.pay = wage * j.slots;
    j.forChildren = false;
    j.until = day + days - 1;
    oddJobs_.push_back(std::move(j));
    ++oddVersion_;
    return oddJobs_.back().pay;
}

std::int64_t Society::buyAndUse(const std::string& payer, const std::string& community, const std::function<bool(const std::string&)>& is,
                                std::int64_t budget, const std::string& what, bool keep)
{
    if (budget < 2)
        return 0;
    std::vector<std::string> shops;
    for (const auto& p : positions_)
        if (p.role == "merchant")
            if (const auto held = state_.careers.positions.find(p.id); held != state_.careers.positions.end() &&
                                                                     !held->second.holder.empty() && communityOfResident(held->second.holder) == community)
                shops.push_back(held->second.holder);
    std::map<std::string, int> got;
    const auto spent = buyForSurplus(payer, shops, is, budget, keep ? what : "bought for " + what, &got);
    if (!keep)
        for (const auto& [item, n] : got)
            consume(payer, item, n, "used in " + what);
    return spent;
}

void Society::institutionsWeek(std::int64_t day)
{
    auto& memory = state_.memory;
    // Each institution's week's budget: a quarter of a month's income, and its share of what it holds over k months of
    // it. In the realm's first month, before it has a month's income, it reckons that a k+1-th of what it holds.
    const auto budgetOf = [&](const std::string& id, double k, double a2) {
        const auto* purse = account(id);
        if (!purse)
            return std::int64_t(0);
        const double held = double(purse->cash);
        const double month = day < MonthDays ? held / (k + 1) : monthIncomeOf(id);
        return std::int64_t(std::clamp((month + a2 * (held - k * month)) / 4, 0., held));
    };
    // The wages it paid this week are its own posts' (the watch, the clergy, the Town Works): out of its budget first.
    const auto wagesPaid = [&](const std::string& id) {
        const auto found = memory.wagesWeek.find(id);
        return found == memory.wagesWeek.end() ? std::int64_t(0) : std::int64_t(found->second);
    };
    std::vector<std::string> towns;
    for (const auto& [id, a] : state_.accounts)
        if (townAccount(id))
            towns.push_back(id);
    for (const auto& id : towns)
    {
        const auto community = id == "treasury" ? capital() : id.substr(7);
        auto budget = budgetOf(id, 2, .4);
        // The week's relief, set aside: what it gave last week and a third over, and a twentieth of its budget besides.
        const auto relief = std::min<std::int64_t>(budget * 3 / 10, std::int64_t(memory.reliefWeek[id] * 1.33) + budget / 20);
        memory.reliefLeft[id] = double(relief);
        memory.reliefWeek[id] = 0;
        budget -= relief + wagesPaid(id);
        if (budget <= 0 || community.empty())
            continue;
        // Its works: a crew hired for the week with most of the rest, materials with what's left.
        const auto wage = std::int64_t(std::ceil(goingRate(community) - 1e-9));
        const int crew = int(budget * 8 / 10 / std::max<std::int64_t>(1, wage * 7));
        budget -= 7 * hireForWeek(id, community, "building work", "the town's works", townSpot(community, id + "|works"), crew, wage, 7, day);
        buyAndUse(id, community, building, budget, "the town's works", false);
    }
    // The church: its relief (at least a tenth of its budget), its clergy, its works, and its own masons.
    if (account(SharedChurch))
    {
        auto budget = budgetOf(SharedChurch, 2, .4);
        const auto relief = std::min<std::int64_t>(budget, std::max<std::int64_t>(std::int64_t(memory.reliefWeek[SharedChurch] * 1.33), budget / 10));
        memory.reliefLeft[SharedChurch] = double(relief);
        memory.reliefWeek[SharedChurch] = 0;
        budget -= relief + wagesPaid(SharedChurch);
        if (budget > 0)
        {
            const auto community = capital();
            budget -= buyAndUse(SharedChurch, community, building, budget / 2, "the church's works", false);
            const auto wage = std::int64_t(std::ceil(goingRate(community) - 1e-9));
            hireForWeek(SharedChurch, community, "building work", "the church's masons", townSpot(community, "church|masons"),
                        int(budget / std::max<std::int64_t>(1, wage * 7)), wage, 7, day);
        }
    }
    // The Houses: retainers, finery, works and an estate crew.
    for (const auto& h : houses())
    {
        auto budget = budgetOf(h.id, 4, .3);
        const auto* seat = position(h.head);
        if (budget <= 0 || !seat)
            continue;
        const auto wage = std::int64_t(std::ceil(goingRate(h.community) - 1e-9));
        budget -= 7 * hireForWeek(h.id, h.community, "about the home", "a retainer of " + h.name, seat->work,
                                  std::min(12, int(budget * 3 / 10 / std::max<std::int64_t>(1, wage * 7))), wage, 7, day);
        // (Finery kept, a few pieces a House: what it has worn out goes in Phase 9.)
        int pieces = 0;
        for (const auto& [item, n] : account(h.id)->stock)
            pieces += finery(item) ? n : 0;
        if (pieces < 12)
            budget -= buyAndUse(h.id, h.community, finery, budget * 3 / 10, "a House's finery", true);
        budget -= buyAndUse(h.id, h.community, building, budget * 4 / 10, "a House's works", false);
        hireForWeek(h.id, h.community, "building work", "the estate of " + h.name, seat->work,
                    int(std::max<std::int64_t>(0, budget) / std::max<std::int64_t>(1, wage * 7)), wage, 7, day);
    }
    memory.wagesWeek.clear();
    ++memory.revision;
}

void Society::dailyRelief(std::int64_t day, const std::map<std::string, LifeBody>& bodies)
{
    (void)day;
    auto& memory = state_.memory;
    // Each household short of a day's food money (or hungry with no food in): a day's, from the church, then its town.
    std::map<std::string, std::vector<std::string>> homes;
    for (const auto& [id, life] : state_.residents)
        if (!life.homeCell.empty() && bodies.count(id))
            homes[life.homeCell].push_back(id);
    for (const auto& [home, members] : homes)
    {
        std::int64_t pot = 0;
        bool hungry = false;
        for (const auto& id : members)
        {
            const auto* a = account(id);
            pot += a ? a->cash : 0;
            hungry |= state_.residents.at(id).hunger >= 60 && a && !hasFood(*a);
        }
        const auto need = householdNeed(members.size());
        if (pot >= need && !hungry)
            continue;
        const auto eldest = *std::max_element(members.begin(), members.end(), [&](const std::string& a, const std::string& b) {
            return bodies.at(a).age < bodies.at(b).age;
        });
        auto want = std::max<std::int64_t>(1, need - std::max<std::int64_t>(0, pot));
        const auto town = treasuryOf(communityOfResident(eldest));
        for (const auto& payer : {std::string(SharedChurch), town})
        {
            // (Before its first reckoning, what its first week's budget would set aside: a k+1-th of what it holds, a
            // quarter of that, and a tenth of it for the church, a twentieth for a town.)
            if (!memory.reliefLeft.count(payer) && account(payer))
                memory.reliefLeft[payer] = double(account(payer)->cash) / 3 / 4 * (payer == SharedChurch ? .1 : .05);
            auto& left = memory.reliefLeft[payer];
            const auto give = std::min<std::int64_t>(want, std::int64_t(left));
            if (give > 0 && account(payer) && account(payer)->cash >= give && shift(payer, eldest, "", 0, give, "relief"))
            {
                left -= double(give);
                memory.reliefWeek[payer] += double(give);
                want -= give;
            }
            if (want <= 0)
                break;
        }
    }
}

void Society::householdRule(const std::string& home, const std::vector<std::string>& members, std::int64_t pot, std::int64_t day,
                            bool lodger)
{
    auto& memory = state_.memory;
    double month = 0;
    for (const auto& id : members)
        month += monthIncomeOf(id);
    const auto need = householdNeed(members.size());
    // The one rule: a share of what's held over a month's income and HomeNeedDays of its needs, half towards things
    // and half on help about the home.
    const double target = month + HomeNeedDays * double(need);
    const double a2 = HomeShareLeast + (HomeShareMost - HomeShareLeast) * lot(home + "|a2");
    const double spare = (double(pot) - target) * a2 / MonthDays;
    auto& fund = memory.wantFund[home];
    auto& help = memory.helpBudget[home];
    if (spare <= 0)
    {
        fund = std::min(fund, std::max(0., double(pot) - target));
        help *= .9;
    }
    else if (lodger)
        fund += spare;
    else
    {
        fund += spare / 2;
        help = help * .9 + spare / 2;
    }
    if (lodger)
    {
        memory.helpBudget.erase(home);
        if (fund <= 0)
            memory.wantFund.erase(home);
        return;
    }
    // Help about the home: a hand for the week, at a little under the going rate, once the budget runs to it (a week's
    // hire, not a day's: fewer jobs on the board, the same work).
    const auto community = communityOfResident(members.front());
    const auto pay = std::int64_t(std::ceil(.8 * goingRate(community) - 1e-9));
    const auto* where = state_.residents.count(members.front()) ? &state_.residents.at(members.front()) : nullptr;
    const auto richest = *std::max_element(members.begin(), members.end(), [&](const std::string& a, const std::string& b) {
        return (account(a) ? account(a)->cash : 0) < (account(b) ? account(b)->cash : 0);
    });
    if (where && help >= double(7 * pay) && hireForWeek(richest, community, "about the home", "help about the home",
                                                        Spot{where->homeCell, where->homeX, where->homeY}, 1, pay, 7, day) > 0)
        help -= double(7 * pay);
    // Servants: holding over half a year of its income (an inheritance, a windfall), posts of its own for the week, as many
    // as a tenth of the excess pays for a month.
    const double excess = double(pot) - ServantMonths * std::max(month, 7. * double(need));
    if (where && excess > 0 && std::int64_t(std::hash<std::string>{}(home + "|servants") % 7) == day % 7)
    {
        const auto wage = std::int64_t(std::ceil(.9 * goingRate(community) - 1e-9));
        hireForWeek(richest, community, "about the home", "a servant of the house", Spot{where->homeCell, where->homeX, where->homeY},
                    std::min(3, int(excess * .1 / double(wage * MonthDays))), wage, 7, day);
    }
    if (fund <= 0)
        memory.wantFund.erase(home);
    if (help <= .5)
        memory.helpBudget.erase(home);
}

void Society::leftovers(std::int64_t day)
{
    (void)day;
    // At the day's end a baker gives what would spoil overnight to the households that need it: the hungry first, then
    // the poor (under three days' food money). Goods, not money: into their larders.
    std::vector<std::pair<int, std::string>> hungry, poor;     // (Members, home.)
    std::map<std::string, std::pair<int, std::int64_t>> homes;
    std::map<std::string, bool> starving;
    for (const auto& [id, life] : state_.residents)
        if (!life.homeCell.empty())
        {
            auto& h = homes[life.homeCell];
            ++h.first;
            h.second += account(id) ? account(id)->cash : 0;
            starving[life.homeCell] = starving[life.homeCell] || (life.hunger >= 60 && account(id) && !hasFood(*account(id)));
        }
    for (const auto& [home, h] : homes)
        if (starving[home])
            hungry.push_back({h.first, home});
        else if (h.second < 3 * householdNeed(std::size_t(h.first)))
            poor.push_back({h.first, home});
    std::vector<std::pair<int, std::string>> queue = hungry;
    queue.insert(queue.end(), poor.begin(), poor.end());
    std::size_t next = 0;
    for (auto& [till, goods] : batches_)
    {
        if (till.rfind("till:", 0) != 0 && till.rfind("house:", 0) != 0)
            continue;
        for (auto& [item, lots] : goods)
        {
            const auto* good = items::good(item);
            if (!good || good->keeps <= 0 || items::baseOf(item) != "bread" && items::baseOf(item) != "fine_bread")
                continue;
            // What would be past keeping by tomorrow.
            int going = 0;
            for (const auto& [made, n] : lots)
                if (double(state_.budgetDay + 1 - made) > good->keeps)
                    going += n;
            going = std::min(going, account(till) ? stock(*account(till), item) : 0);
            while (going > 0 && next < queue.size())
            {
                const auto& [members, home] = queue[next];
                const auto larder = homeStore(home, "larder");
                openAccount(larder);
                const int give = std::min(going, std::max(1, members * 2));
                if (shift(till, larder, item, give, 0, "a baker's leftovers"))
                    going -= give;
                ++next;
            }
        }
    }
}
} // namespace ratw
