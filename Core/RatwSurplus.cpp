// The rule against hoarding (Docs/Design/42-money-in-circulation.md): every place money gathers (a town's treasury, a
// church, a great house) keeps a reserve, four weeks of what it usually spends, and sends a tenth of anything above it
// back out each day, in ways that suit it: a town hires hands and buys materials for its works, a church feeds the poor,
// a house rewards its people, feasts them and commissions fine goods. Spending pays wolves for work or goods, or gives
// the poor food; it never makes or destroys money.
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace ratw
{
namespace
{
bool collector(const std::string& id)
{
    return id == "treasury" || id.rfind("stores:", 0) == 0 || id.rfind("house:", 0) == 0 || id == Society::SharedChurch;
}
} // namespace

void Society::noteOutgoing(const std::string& from, const std::string& kind, std::int64_t coins)
{
    // What a collector spends in the ordinary way (wages, its buyers' funds...), for its reserve; not what it spends
    // of a surplus.
    if (coins > 0 && collector(from) && kind.rfind("surplus: ", 0) != 0)
        spentToday_[from] += coins;
    if (coins > 0 && (from == "treasury" || from.rfind("stores:", 0) == 0) && kind.find("wage") != std::string::npos)
        wagesToday_[from] += coins;
}

void Society::noteIncoming(const std::string& to, const std::string& kind, std::int64_t coins)
{
    if (coins > 0 && (to == "treasury" || to.rfind("stores:", 0) == 0) && kind != "starting money")
        incomeToday_[to] += coins;
}

std::vector<Society::Spending> Society::takeSpendings()
{
    std::vector<Spending> out;
    out.swap(spendings_);
    return out;
}

std::int64_t Society::buyForSurplus(const std::string& buyer, const std::vector<std::string>& shops,
                                    const std::function<bool(const std::string& item)>& wanted, std::int64_t budget,
                                    const std::string& kind, std::map<std::string, int>* got, bool lastToo)
{
    std::int64_t spent = 0;
    for (const auto& shop : shops)
    {
        const auto till = tillOf(shop);
        const auto* stockHeld = account(till);
        if (!stockHeld)
            continue;
        const std::vector<std::pair<std::string, int>> held(stockHeld->stock.begin(), stockHeld->stock.end());
        for (const auto& [item, n] : held)
        {
            const auto* good = items::good(item);
            if (n < (lastToo ? 1 : 2) || !good || !wanted(items::baseOf(item)))
                continue;
            const std::int64_t price = shopPrice(shop, item);
            // Never the shop's last few: a town's shelves are for its townsfolk first (but for the hungry, `lastToo`).
            const int count = int(std::min<std::int64_t>({lastToo ? n : (n - 1) / 2, 20, (budget - spent) / price, account(buyer)->cash / price}));
            if (count > 0 && transfer(till, buyer, item, count, price, kind))
            {
                spent += price * count;
                if (got)
                    (*got)[item] += count;
            }
            if (budget - spent < price)
                break;
        }
        if (budget - spent <= 0)
            break;
    }
    return spent;
}

void Society::spendSurpluses(std::int64_t day)
{
    if (roster_ != Roster::Authored || day == surplusDay_)
        return;
    surplusDay_ = day;
    // Yesterday's odd jobs are over, but for businesses' hires still running: those who hold them come back to them, at
    // today's pay for a hand in its town (the wage table, doc 46, Phase 4).
    oddJobs_.erase(std::remove_if(oddJobs_.begin(), oddJobs_.end(), [&](const OddJob& j) { return j.until < day; }), oddJobs_.end());
    ++oddVersion_;
    for (auto& j : oddJobs_)
    {
        if (j.until >= 0 && !j.producer.empty() && j.slots > 0)
            j.pay = std::int64_t(std::ceil(dayWage(j.community, "hand") - 1e-9)) * j.slots;
        for (auto& [who, stage] : j.stage)
            stage = 0, j.progress[who] = 0;
    }
    // How much each collector usually spends (a slow average of its days), for its reserve.
    for (auto& [id, avg] : state_.memory.outgoing)
        avg *= .8;
    for (const auto& [id, coins] : spentToday_)
        state_.memory.outgoing[id] += .2 * double(coins);
    spentToday_.clear();
    ++state_.memory.revision;
    // The communities: who lives in each, and its shops.
    std::map<std::string, std::vector<std::string>> folk, shops;
    for (const auto& r : authored_.residents)
        if (state_.residents.count(r.id))
        {
            const auto c = communityOfResident(r.id);
            folk[c].push_back(r.id);
            if (r.role == "merchant")
                shops[c].push_back(r.id);
        }
    // Each town's church feeds its town's poor every day, surplus or not (the user, 2026-10-05: no one starves with a church
    // in town): a day's food each to those with no food and no pennies to buy it (a child whose larder is empty, an
    // apprentice), hungry yet or not, bought at the town's shops with the land's purse (SharedChurch).
    const std::string land = SharedChurch;
    for (const auto& entry : folk)
    {
        if (!account(land))
            break;
        const auto& town = entry.first;
        const auto& id = land;
        // Its town's by home (a salt raker who works out at the pans is still its own).
        std::vector<std::string> hungry;
        for (const auto& [who, life] : state_.residents)
            if (const auto home = day_.communityOf ? day_.communityOf(life.homeCell) : std::string(); home == town)
                if (const auto* p = account(who); p && p->cash < 6 && !hasFood(*p))
                {
                    const auto larder = homeStore(life.homeCell, "larder");
                    if (!account(larder) || !hasFood(*account(larder)))
                        hungry.push_back(who);
                }
        if (hungry.empty())
            continue;
        // Food from the town's shops, the last of it too if need be (for the hungry, not the church's table).
        std::map<std::string, int> bread;
        buyForSurplus(id, shops[town], [](const std::string& item) { return edible(item); }, std::int64_t(hungry.size()) * 6,
                      "alms bought for the hungry", &bread, true);
        for (const auto& who : hungry)
            for (auto& [item, n] : bread)
                if (n > 0 && shift(id, who, item, 1, 0, "alms"))
                {
                    --n;
                    break;
                }
        // What is left, to its church's storehouse (its table and tomorrow's alms).
        openAccount(churchStore(town));
        for (const auto& [item, n] : bread)
            if (n > 0)
                shift(id, churchStore(town), item, n, 0, "to the church's storehouse");
    }
    // The dole (the user, 2026-10-06: money back out at the lowest level): each household short of a week's food is given
    // DoleADay a member in coins, to its eldest, to spend at its own town's shops; the poorest households first, and never
    // more than a thirtieth of the church's purse a day.
    if (const auto* purse = account(land))
    {
        struct Household
        {
            std::vector<std::string> members;
            std::int64_t held = 0;
        };
        std::map<std::string, Household> homes;
        for (const auto& [who, life] : state_.residents)
            if (!life.homeCell.empty())
                if (const auto* p = account(who))
                {
                    auto& h = homes[life.homeCell];
                    h.members.push_back(who);
                    h.held += p->cash;
                }
        std::vector<std::pair<double, const Household*>> short_;   // (Purse a head, household.)
        for (const auto& [home, h] : homes)
            if (h.held < 7 * householdNeed(h.members.size()))
                short_.push_back({double(h.held) / double(h.members.size()), &h});
        std::sort(short_.begin(), short_.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        std::int64_t left = purse->cash / 30;
        for (const auto& [perHead, h] : short_)
        {
            const auto dole = DoleADay * std::int64_t(h->members.size());
            if (dole > left)
                break;
            const auto eldest = *std::max_element(h->members.begin(), h->members.end(), [&](const std::string& a, const std::string& b) {
                const auto* ra = spec(a);
                const auto* rb = spec(b);
                return (ra ? ra->age : 0) < (rb ? rb->age : 0);
            });
            if (shift(land, eldest, "", 0, dole, "the church's dole"))
                left -= dole;
        }
    }
    // What the treasuries, the church and the great houses hold above their band is the economy orchestrator's to send
    // out now, through its channels (doc 46, Phase 5; RatwChannels.cpp): the town's funds spend today.
    runChannels(day);
    // (Wages are the town's table now, doc 46, Phase 4: no more shares of a day's takings.)
    tendPrices(day);                                // (By today's takings: before they are forgotten.)
    takings_.clear();
    // Farmhands (RatwFarmhands.cpp): hires ended and lodgers home, new hires posted, bunkhouse larders stocked.
    endLodgings(day);
    postFarmHires(day);
    stockBunkhouses(day);
    // Market dues (the user, 2026-10-05: tax where money gathers): each Restday every shop's till pays its town a
    // twentieth of what it holds above its float.
    if (calendar::weekdayOf(double(day)) == calendar::Restday)
        keepUpPremises();                            // (Improved premises kept up, or let go: RatwOddJobs.cpp.)
    if (calendar::weekdayOf(double(day)) == calendar::Restday)
        for (const auto& p : positions_)
        {
            if (p.role != "merchant")
                continue;
            const auto held = state_.careers.positions.find(p.id);
            if (held == state_.careers.positions.end() || held->second.holder.empty())
                continue;
            const auto till = tillOf(held->second.holder);
            const auto* purse = account(till);
            const auto town = communityOfResident(held->second.holder);
            const auto due = purse ? (purse->cash - floatOf(p.id) - (till == held->second.holder ? KeeperReserve : 0)) / 20 : 0;
            if (due > 0 && !town.empty())
                shift(till, treasuryOf(town), "", 0, due, "market dues");
        }
}
} // namespace ratw
