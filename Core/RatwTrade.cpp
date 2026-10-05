// Trade between towns (Docs/Design/42-money-in-circulation.md, Phase 7). Each day, where one town has goods to spare
// that another's makers and suppliers are short of, a trading house of the first (one of its great houses, or else
// its treasury) buys them at home, sends them by caravan with the money left over, and sells them on arrival to the
// shops that want them, a little dearer for the road. The coins ride home with the wagon. What a town lacks comes
// only this way: bandits who rob a caravan take its goods and its money. Nothing is made or lost but what is eaten.
#include "RatwItems.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
namespace
{
constexpr double Markup = 1.4;                       // What carted goods sell for, against the catalog price.
constexpr int MostOfAGood = 30, MostALoad = 80;      // A wagon's load (placeholders).
constexpr int TradeCaravansADay = 12;
}

std::string World::residentTown(const std::string& id) const
{
    const auto* job = society_.jobOf(id);
    const auto* spec = society_.spec(id);
    if (!spec)
        return {};
    if (const auto work = lawTown(job ? job->work.cell : spec->work.cell); !work.empty())
        return work;
    const auto* life = society_.resident(id);
    return lawTown(life && !life->homeCell.empty() ? life->homeCell : spec->home.cell);
}

World::Market World::marketOf(const std::string& town) const
{
    // What a town's shops and producers have to spare, and what its makers and suppliers are short of.
    Market m;
    for (const auto& r : society_.authored().residents)
    {
        const auto* e = entity(r.id);
        if (!e || e->dead || !society_.resident(r.id) || residentTown(r.id) != town)
            continue;
        if (const auto* p = items::producerFor(r.workLabel))
        {
            const auto* purse = society_.account(r.id);
            if (!purse)
                continue;
            for (const auto& [item, n] : p->out)
                if (const int held = Society::stockAll(*purse, item); held > 0)
                {
                    m.spare[item] += held;
                    m.holders[item].push_back({r.id, held});
                }
            continue;
        }
        if (r.role != "merchant")
            continue;
        const auto* business = items::businessFor(r.workLabel);
        const auto till = society_.tillOf(r.id);
        const auto* shelves = society_.account(till);
        if (!business || !shelves)
            continue;
        const auto& supplies = items::suppliesFor(business->id);
        for (const auto* k : items::craftsFor(business->id))
        {
            for (const auto& [item, n] : k->out)
            {
                const bool staple = std::find(supplies.begin(), supplies.end(), item) != supplies.end() || items::traded(item);
                if (const int spare = Society::stockAll(*shelves, item) - (staple ? Society::SuppliesKept / 2 : Society::GoodsKept); spare > 0)
                {
                    m.spare[item] += spare;
                    m.holders[item].push_back({till, spare});
                }
            }
            for (const auto& [item, n] : k->in)
                if (const int want = n * Society::MaterialBatches - Society::stockAll(*shelves, item); want > 0)
                {
                    m.want[item] += want;
                    m.wanters[item].push_back({till, want});
                }
        }
        for (const auto& item : supplies)
            if (const int want = Society::SuppliesKept / 2 - Society::stockAll(*shelves, item); want > 0)
            {
                m.want[item] += want;
                m.wanters[item].push_back({till, want});
            }
    }
    for (auto* list : {&m.holders, &m.wanters})
        for (auto& [item, who] : *list)
            std::sort(who.begin(), who.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    return m;
}

std::string World::traderOf(const Town& t) const
{
    // A great house of the town, the richest; else its treasury.
    std::string best = t.store;
    std::int64_t most = -1;
    for (const auto& h : society_.houses())
        if (h.community == t.id)
            if (const auto* purse = society_.account(h.id); purse && purse->cash > most)
                most = purse->cash, best = h.id;
    return best;
}

void World::tradeCaravans()
{
    if (towns_.size() < 2)
        return;
    std::map<std::string, Market> markets;
    for (const auto& t : towns_)
        markets[t.id] = marketOf(t.id);
    // Each town's net want of a good (after its own spare), and where it is spare (after the town's own want).
    struct Order
    {
        std::map<std::string, int> load;
        int total = 0;
    };
    std::map<std::pair<std::string, std::string>, Order> orders;   // (from, to)
    for (const auto& to : towns_)
        for (const auto& [item, want] : markets[to.id].want)
        {
            const int need = want - (markets[to.id].spare.count(item) ? markets[to.id].spare.at(item) : 0);
            if (need < 4)
                continue;
            const Town* best = nullptr;
            int bestSpare = 3;
            for (const auto& from : towns_)
            {
                if (&from == &to)
                    continue;
                auto& fm = markets[from.id];
                const int spare = (fm.spare.count(item) ? fm.spare.at(item) : 0) - (fm.want.count(item) ? fm.want.at(item) : 0);
                if (spare > bestSpare)
                    best = &from, bestSpare = spare;
            }
            if (!best)
                continue;
            auto& o = orders[{best->id, to.id}];
            const int n = std::min({need, bestSpare, MostOfAGood, MostALoad - o.total});
            if (n <= 0)
                continue;
            o.load[item] += n;
            o.total += n;
            markets[best->id].spare[item] -= n;           // (Not promised twice.)
        }
    int sent = 0;
    for (const auto& [pair, order] : orders)
    {
        if (sent >= TradeCaravansADay || order.load.empty())
            break;
        const auto* from = town(pair.first);
        const auto* to = town(pair.second);
        const bool going = std::any_of(roads_.caravans.begin(), roads_.caravans.end(), [&](const Caravan& c) {
            return !c.trader.empty() && c.from == pair.first && c.to == pair.second && c.status == "travelling";
        });
        if (!from || !to || going)
            continue;
        // What the load costs at home, and the trader can pay.
        std::int64_t cost = 0;
        for (const auto& [item, n] : order.load)
            if (const auto* good = items::good(item))
                cost += std::int64_t(std::max(1, good->price)) * n;
        const auto trader = traderOf(*from);
        const auto* purse = society_.account(trader);
        if (cost <= 0 || !purse || purse->cash < cost)
            continue;
        auto* c = sendCaravan(*from, *to, {});
        if (!c)
            continue;
        c->trader = trader;
        society_.shift(trader, c->account, "", 0, cost, "money for the road");
        int bought = 0;
        auto& fm = markets[from->id];
        for (const auto& [item, n] : order.load)
        {
            int left = n;
            for (auto& [holder, spare] : fm.holders[item])
            {
                if (left <= 0)
                    break;
                const auto* shelves = society_.account(holder);
                for (const auto& sort : shelves ? Society::kindsHeld(*shelves, item) : std::vector<std::string>{})
                {
                    const auto* good = items::good(sort);
                    const int k = std::min({left, spare, Society::stock(*society_.account(holder), sort), 99});
                    if (good && k > 0 && society_.sale(holder, c->account, sort, k, std::max(1, good->price), "bought for the road"))
                        left -= k, spare -= k, bought += k;
                    if (left <= 0 || spare <= 0)
                        break;
                }
            }
        }
        // Rations for the carters and feed for the horses, bought at home and eaten on the way (the trader's cost).
        for (const auto& [item, n] : std::map<std::string, int>{{"bread", 2}, {"hay", 2}})
            for (auto& [holder, spare] : fm.holders[item])
                if (const auto* good = items::good(item); good && spare >= n && society_.account(c->account)->cash >= good->price * n &&
                                                          society_.sale(holder, c->account, item, n, std::max(1, good->price), "provisions for the road"))
                {
                    society_.consume(c->account, item, n, "eaten on the road");
                    spare -= n;
                    break;
                }
        if (auto* w = entity("road:" + c->id))
            w->name = "a trader's caravan bound for " + to->id;
        recordEvent({"trade caravan departs", c->id, to->id, from->market, 0, 0, {}, bought, cost,
                     std::to_string(bought) + " goods for " + to->id + ", " + trader + "'s"});
        ++sent;
    }
}

void World::tradeCaravanArrived(Caravan& c)
{
    // Sold to the shops that want the goods, a little dearer for the road; what nobody takes rides home.
    const auto* to = town(c.to);
    if (!to)
        return;
    auto market = marketOf(to->id);
    std::int64_t made = 0;
    const auto* load = society_.account(c.account);
    if (!load)
        return;
    for (const auto& [item, n] : std::map<std::string, int>(load->stock.begin(), load->stock.end()))
    {
        const auto base = items::baseOf(item);
        const auto* good = items::good(item);
        if (n <= 0 || !good)
            continue;
        const auto price = std::int64_t(std::ceil(std::max(1, good->price) * Markup));
        int left = n;
        for (auto& [buyer, want] : market.wanters[base])
        {
            const auto* purse = society_.account(buyer);
            const int k = int(std::min<std::int64_t>({left, want, 99, purse ? purse->cash / price : 0}));
            if (k > 0 && society_.sale(c.account, buyer, item, k, price, "carted in by caravan"))
                left -= k, want -= k, made += price * k;
            if (left <= 0)
                break;
        }
    }
    recordEvent({"trade caravan sells", c.id, c.to, c.cell, 0, 0, {}, 0, made, "sold for " + std::to_string(made) + "p in " + c.to});
}

void World::tradeCaravanHome(Caravan& c)
{
    // Home: its takings and anything unsold go back to the trader (unsold goods into the town's store, the store paying
    // what it can for them, so the trader isn't left holding them).
    const auto* home = town(c.from);
    const auto* load = society_.account(c.account);
    if (!load || !home)
        return;
    for (const auto& [item, n] : std::map<std::string, int>(load->stock.begin(), load->stock.end()))
        if (n > 0)
        {
            const auto* good = items::good(item);
            const auto* store = society_.account(home->store);
            const int paid = good && store ? int(std::min<std::int64_t>(n, store->cash / std::max(1, good->price))) : 0;
            if (paid > 0)
                society_.sale(c.account, home->store, item, std::min(paid, 99), std::max(1, good->price), "unsold, back to the stores");
        }
    if (const auto cash = society_.account(c.account)->cash; cash > 0)
        society_.shift(c.account, c.trader, "", 0, cash, "trade takings");
}
} // namespace ratw
