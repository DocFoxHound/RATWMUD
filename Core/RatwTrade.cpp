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
constexpr std::int64_t GuardsHire = 6;               // Two guards for the road, paid to the town.
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
    // What a town's shops and producers have to spare, and what its makers and suppliers are short of; and, when its
    // shops are running out of food, the food its food shops sell (Docs/Design/42, "Food between towns").
    Market m;
    for (const auto& r : society_.authored().residents)
    {
        const auto* e = entity(r.id);
        if (!e || e->dead || !society_.resident(r.id) || residentTown(r.id) != town)
            continue;
        ++m.people;
        if (const auto* p = items::producerFor(r.workLabel))
        {
            const auto farm = society_.tillOf(r.id);   // (Its yield is its till's: doc 46, Phase 2.)
            const auto* purse = society_.account(farm);
            if (!purse)
                continue;
            for (const auto& [item, n] : p->out)
                if (const int held = Society::stockAll(*purse, item); held > 0)
                {
                    m.spare[item] += held;
                    m.holders[item].push_back({farm, held});
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
        for (const auto& [item, count] : shelves->stock)
            if (count > 0 && Society::edible(item))
                m.food += count;
        if (society_.foodShop(r.id))
            m.foodShops.push_back(till);
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

std::int64_t World::orderPrice(const Town& from, const std::string& item) const
{
    // The selling town's price (the economy orchestrator's: doc 46, Phase 3), and the road's markup.
    return Society::pennies(society_.townPrice(from.id, item) * Markup);
}

void World::tradeCaravans()
{
    if (towns_.size() < 2)
        return;
    std::map<std::string, Market> markets;
    for (const auto& t : towns_)
        markets[t.id] = marketOf(t.id);
    // Food between towns (doc 42, the user, 2026-10-06): a town whose shops hold under FoodDaysKept days' food for its
    // people wants, for each of its food shops, FoodKept of each of the FoodsSent foods the other towns have most to spare,
    // as a maker wants its materials; standing orders bring them.
    for (const auto& to : towns_)
    {
        auto& m = markets[to.id];
        if (m.food >= FoodDaysKept * m.people || m.foodShops.empty())
            continue;
        std::map<std::string, int> elsewhere;
        for (const auto& from : towns_)
            if (&from != &to)
                for (const auto& [item, n] : markets[from.id].spare)
                    if (const auto* good = items::good(item); good && !good->drink && Society::edible(item))
                        elsewhere[item] += n - (markets[from.id].want.count(item) ? markets[from.id].want.at(item) : 0);
        std::vector<std::pair<int, std::string>> foods;
        for (const auto& [item, n] : elsewhere)
            if (n > 0)
                foods.push_back({n, item});
        std::sort(foods.rbegin(), foods.rend());
        for (std::size_t k = 0; k < foods.size() && k < std::size_t(FoodsSent); ++k)
            for (const auto& till : m.foodShops)
                if (const auto* shelves = society_.account(till))
                    if (const int want = FoodKept - Society::stockAll(*shelves, foods[k].second); want > 0)
                    {
                        m.want[foods[k].second] += want;
                        m.wanters[foods[k].second].push_back({till, want});
                    }
        for (auto& [item, who] : m.wanters)
            std::sort(who.begin(), who.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    }
    // What standing orders already bring, by town and good: not wanted again.
    std::map<std::pair<std::string, std::string>, int> ordered;
    std::set<std::pair<std::string, std::string>> buyers;   // (till, good) with an order.
    for (const auto& o : roads_.orders)
    {
        ordered[{o.town, o.item}] += o.perWeek;
        buyers.insert({o.buyer, o.item});
        markets[o.from].spare[o.item] -= o.perWeek;       // (Promised already.)
    }
    // New standing orders: a shop short of a good its own town can't spare, signed with the town that has the most of
    // it to spare, for about what it is short.
    std::set<std::pair<std::string, std::string>> signedToday;
    for (const auto& to : towns_)
        for (const auto& [item, want] : markets[to.id].want)
        {
            const auto local = markets[to.id].spare.count(item) ? markets[to.id].spare.at(item) : 0;
            const auto covered = ordered.count({to.id, item}) ? ordered.at({to.id, item}) : 0;
            if (want - local - covered < 4)
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
            for (const auto& [buyer, wanted] : markets[to.id].wanters[item])
            {
                if (bestSpare < 4)
                    break;
                if (buyers.count({buyer, item}))
                    continue;
                StandingOrder o;
                o.id = "so" + std::to_string(roads_.nextId++);
                o.buyer = buyer;
                o.town = to.id;
                o.item = item;
                o.from = best->id;
                o.perWeek = std::clamp(wanted, 4, std::min(MostOfAGood, bestSpare));
                o.price = orderPrice(*best, item);
                o.since = calendarDays_;
                o.review = calendarDays_ + MonthDays;
                bestSpare -= o.perWeek;
                markets[best->id].spare[item] -= o.perWeek;
                buyers.insert({buyer, item});
                signedToday.insert({best->id, to.id});
                recordEvent({"standing order", buyer, best->id, to.market, 0, 0, item, o.perWeek, o.price,
                             std::to_string(o.perWeek) + " " + item + " a week from " + best->id + " at " + std::to_string(o.price) + "p"});
                roads_.orders.push_back(std::move(o));
            }
        }
    // Each road with standing orders sends its trader's caravan once a week (on a day of its own), and at once when an
    // order is new; a wagon still on the road goes again next week.
    std::map<std::pair<std::string, std::string>, std::map<std::string, int>> loads;
    for (const auto& o : roads_.orders)
        loads[{o.from, o.town}][o.item] += o.perWeek;
    const auto today = std::int64_t(std::floor(calendarDays_));
    int sent = 0;
    for (const auto& [pair, load] : loads)
    {
        if (sent >= TradeCaravansADay)
            break;
        const bool due = signedToday.count(pair) ||
                         std::hash<std::string>{}(pair.first + ">" + pair.second) % 7 == std::uint64_t(today % 7);
        const auto* from = town(pair.first);
        const auto* to = town(pair.second);
        const bool going = std::any_of(roads_.caravans.begin(), roads_.caravans.end(), [&](const Caravan& c) {
            return !c.trader.empty() && c.from == pair.first && c.to == pair.second && c.status == "travelling";
        });
        if (!due || !from || !to || going)
            continue;
        // What the load costs at home, and the trader can pay.
        std::int64_t cost = 0;
        int total = 0;
        for (const auto& [item, n] : load)
            if (items::good(item) && from)
                cost += Society::pennies(society_.townPrice(from->id, item)) * n, total += n;
        const auto trader = traderOf(*from);
        const auto* purse = society_.account(trader);
        if (cost <= 0 || !purse || purse->cash < cost)
            continue;
        auto* c = sendCaravan(*from, *to, {});
        if (!c)
            continue;
        c->trader = trader;
        society_.shift(trader, c->account, "", 0, cost, "money for the road");
        // Two more guards for a load worth robbing, hired from the town's watch.
        if (from->store != trader && society_.shift(trader, from->store, "", 0, GuardsHire, "guards for the road"))
            c->guards += 2;
        else if (from->store == trader)
            c->guards += 2;
        int bought = 0;
        auto& fm = markets[from->id];
        for (const auto& [item, n] : load)
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
                    const auto each = Society::pennies(society_.townPrice(from->id, sort));
                    if (good && k > 0 && society_.sale(holder, c->account, sort, k, each, "bought for the road"))
                        left -= k, spare -= k, bought += k;
                    if (left <= 0 || spare <= 0)
                        break;
                }
            }
        }
        // Rations for the carters and feed for the horses, bought at home and eaten on the way (the trader's cost).
        for (const auto& [item, n] : std::map<std::string, int>{{"bread", 2}, {"hay", 2}})
            for (auto& [holder, spare] : fm.holders[item])
                if (const auto each = Society::pennies(society_.townPrice(from->id, item));
                    items::good(item) && spare >= n && society_.account(c->account)->cash >= each * n &&
                    society_.sale(holder, c->account, item, n, each, "provisions for the road"))
                {
                    society_.consume(c->account, item, n, "eaten on the road");
                    spare -= n;
                    break;
                }
        if (auto* w = entity("road:" + c->id))
            w->name = "a trader's caravan bound for " + to->id;
        recordEvent({"trade caravan departs", c->id, to->id, from->market, 0, 0, {}, bought, cost,
                     std::to_string(bought) + " of " + std::to_string(total) + " goods ordered for " + to->id + ", " + trader + "'s"});
        ++sent;
    }
}

void World::residentsRenegotiate(std::set<std::string>& busy)
{
    // A standing order due to be looked at again (four weeks on, or deliveries well short): a porter of the buyer's
    // town walks to the selling town's market to renegotiate it, with food for the road from the shop.
    const auto today = std::int64_t(std::floor(calendarDays_));
    for (auto& o : roads_.orders)
    {
        if (!o.negotiator.empty() || (calendarDays_ < o.review && o.shortfall < o.perWeek * 2))
            continue;
        const auto* from = town(o.from);
        if (!from)
            continue;
        std::string chosen;
        std::uint64_t best = ~0ULL;
        for (const auto& [id, life] : society_.state().residents)
        {
            const auto* e = entity(id);
            const auto* job = society_.jobOf(id);
            const bool carries = job && job->role == "civilian" &&
                                 (job->title.find("carries") != std::string::npos || job->title.find("carrying") != std::string::npos ||
                                  job->title.find("hauling") != std::string::npos || job->title.find("messages") != std::string::npos ||
                                  job->title.find("loads") != std::string::npos);
            if (!e || e->dead || e->transient || e->age < 16 || e->age >= Society::RetireAge || busy.count(id) ||
                (job && !carries) || society_.apprenticedTo(id) || lawTown(life.homeCell) != o.town)
                continue;
            if (const auto drawn = std::hash<std::string>{}(o.id + "|" + id + "|" + std::to_string(today)); drawn < best)
                best = drawn, chosen = id;
        }
        if (chosen.empty())
            continue;
        o.negotiator = chosen;
        busy.insert(chosen);
        if (const auto* purse = society_.account(o.buyer))
            provisionTraveller(chosen, o.buyer, o.town, from->market, purse->cash / 8);
        recordEvent({"sent to renegotiate", chosen, o.buyer, entity(chosen)->cellId, 0, 0, o.item, o.perWeek, o.price,
                     "the standing order for " + o.item + " with " + o.from});
    }
}

void World::tendNegotiators()
{
    // At the selling town's market: the order agreed again. A shop that has piled up more than two weeks' worth wants
    // a quarter less; one that went short or ran low, a quarter more; at the price the goods fetch there now. One that
    // wants next to nothing ends it.
    for (auto it = roads_.orders.begin(); it != roads_.orders.end();)
    {
        auto& o = *it;
        const auto* porter = o.negotiator.empty() ? nullptr : entity(o.negotiator);
        if (!o.negotiator.empty() && (!porter || porter->dead))
            o.negotiator.clear();
        const auto* from = town(o.from);
        if (!porter || porter->dead || !from || porter->cellId != from->market ||
            std::hypot(porter->position.x - from->marketX, porter->position.y - from->marketY) > 3)
        {
            ++it;
            continue;
        }
        const auto* shelves = society_.account(o.buyer);
        const int held = shelves ? Society::stockAll(*shelves, o.item) : 0;
        const int before = o.perWeek;
        if (held > o.perWeek * 2)
            o.perWeek = o.perWeek * 3 / 4;
        else if (o.shortfall > 0 || held < o.perWeek / 2)
            o.perWeek = std::min(60, o.perWeek * 5 / 4 + 1);
        o.price = orderPrice(*from, o.item);
        o.review = calendarDays_ + MonthDays;
        o.delivered = o.shortfall = 0;
        if (shelves && society_.shift(o.buyer, o.negotiator, "", 0, std::min<std::int64_t>(3, shelves->cash), "a negotiator's pay"))
            {}
        recordEvent({o.perWeek < 2 ? "standing order ends" : "standing order renegotiated", o.negotiator, o.buyer, porter->cellId, 0, 0,
                     o.item, o.perWeek, o.price,
                     o.item + " from " + o.from + ": " + std::to_string(before) + " a week, now " + std::to_string(o.perWeek) + " at " +
                         std::to_string(o.price) + "p"});
        o.negotiator.clear();
        if (o.perWeek < 2)
            it = roads_.orders.erase(it);
        else
            ++it;
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
    // Its standing orders first, at the price agreed, as far as each buyer can pay; then the rest to whoever wants it.
    for (auto& o : roads_.orders)
    {
        if (o.from != c.from || o.town != c.to)
            continue;
        int given = 0;
        for (const auto& sort : Society::kindsHeld(*society_.account(c.account), o.item))
        {
            const auto* purse = society_.account(o.buyer);
            const int k = int(std::min<std::int64_t>({o.perWeek - given, Society::stock(*society_.account(c.account), sort), 99,
                                                      purse ? purse->cash / std::max<std::int64_t>(1, o.price) : 0}));
            if (k > 0 && society_.sale(c.account, o.buyer, sort, k, o.price, "a standing order"))
                given += k, made += o.price * k;
            if (given >= o.perWeek)
                break;
        }
        o.delivered += given;
        o.shortfall += o.perWeek - given;
    }
    load = society_.account(c.account);
    for (const auto& [item, n] : std::map<std::string, int>(load->stock.begin(), load->stock.end()))
    {
        const auto base = items::baseOf(item);
        const auto* good = items::good(item);
        if (n <= 0 || !good)
            continue;
        // The town's price (the orchestrator's, doc 46), on top of the road's markup: dearer where it is short.
        const auto price = Society::pennies(society_.townPrice(to->id, item) * Markup);
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
