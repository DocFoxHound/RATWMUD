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
constexpr int MostOfAGood = 30, MostALoad = 80;      // A wagon's load (placeholders).
constexpr int TradeCaravansADay = 12;
constexpr double OrderSettles = 7;                   // Days before a new or raised standing order is looked at again.
constexpr int RoundTripDays = 2;                     // About as long as a wagon is out (placeholder).
// A food that rots within the week a caravan takes to come again (fish, rabbit, porridge) isn't ordered by the roads
// (doc 64, Phase 13): it's made where it's eaten, the town's own producers told of what its makers went without.
constexpr double RoadKeeps = 7;
bool rotsOnTheRoad(const std::string& item)
{
    const auto* good = items::good(items::baseOf(item));
    return Society::rots(good) && good->keeps < RoadKeeps;
}
// What a week's order holds back of the seller's spare: all of it, but half for a town whose trade the good is (doc 61),
// whose makers and producers keep making it all week, so that it can promise twice what it has to spare on the day.
int promised(const std::string& town, const std::string& item, int perWeek)
{
    return items::townGoodOf(town, item) ? (perWeek + 1) / 2 : perWeek;
}
constexpr double GuardsHire = 6 / 12.;              // Two guards for the road, paid to the town: in LD (6p at a 12p day).
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
        std::map<std::string, int> tillWant;            // What this shop wants, by good.
        for (const auto* k : items::craftsFor(business->id))
        {
            for (const auto& [item, n] : k->out)
            {
                const bool staple = std::find(supplies.begin(), supplies.end(), item) != supplies.end() || items::traded(item);
                // (Over what it keeps for its own sales, doc 64, Phase 3b; not its store for the roads' orders, which is
                // what it has to send.)
                if (const int spare = Society::stockAll(*shelves, item) -
                                      society_.shelfTarget(till, item, staple ? Society::SuppliesKept / 2 : Society::GoodsKept, false);
                    spare > 0)
                {
                    m.spare[item] += spare;
                    m.holders[item].push_back({till, spare});
                }
            }
            // (One store of each material, whichever crafts it goes into: a smith's iron for its nails, tools and
            // fittings is one store of bars, not one for each, doc 61; it was counted fourteen times over, and a town
            // with a smith never had iron to spare.)
            // (Materials for the batches that fill its shelf by what it sells, as Society::craft buys them: doc 64, 3b.)
            // (A food maker's, days of its use, as it lays them in: doc 64, Phase 13.)
            const int perBatch = k->out.empty() ? 1 : std::max(1, k->out.front().second);
            const int kept = k->out.empty() ? 0 : society_.shelfTarget(till, k->out.front().first, Society::GoodsKept, false);
            const int batches = std::max(Society::MaterialBatches, (kept + perBatch - 1) / perBatch);
            for (const auto& [item, n] : k->in)
                tillWant[item] = std::max(tillWant[item], society_.inputStore(till, *k, item, n * batches) - Society::stockAll(*shelves, item));
        }
        for (const auto& item : supplies)
            tillWant[item] = std::max(tillWant[item], society_.shelfTarget(till, item, Society::SuppliesKept / 2, false) -
                                                          Society::stockAll(*shelves, item));
        for (const auto& [item, want] : tillWant)
            if (want > 0)
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
    // The selling town's going price and the carriage, the carrier's margin on it (doc 64, Phase 4: carriage by weight,
    // in place of the road's flat markup).
    return Society::pennies(society_.townPrice(from.id, item) +
                            society_.carriage(item, Society::CarriageLegs, from.id) * (1 + items::ladder().markup));
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
                    if (const auto* good = items::good(item); good && !good->drink && Society::edible(item) && !rotsOnTheRoad(item))
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
        markets[o.from].spare[o.item] -= promised(o.from, o.item, o.perWeek);   // (Promised already.)
    }
    // New standing orders: a shop short of a good its own town can't spare, signed with the town that has the most of
    // it to spare, for about what it is short. A town whose trade the good is (doc 61) comes first, if it has any to
    // spare; else the most to spare. Not, for a month, a town whose order with the buyer was cut or ended for what it
    // didn't send (RoadsState::failed; doc 64, Phase 13: signed again the next day with the town that had failed it,
    // for more): another, if one has some to spare.
    const auto today = std::int64_t(std::floor(calendarDays_));
    for (auto it = roads_.failed.begin(); it != roads_.failed.end();)
        it = today - it->second >= MonthDays ? roads_.failed.erase(it) : std::next(it);
    const auto failedWith = [&](const std::string& buyer, const std::string& item, const std::string& from) {
        return !buyer.empty() && roads_.failed.count(buyer + "|" + item + "|" + from) > 0;
    };
    const auto bestFor = [&](const Town& to, const std::string& item, const std::string& buyer) {
        std::pair<const Town*, int> best{nullptr, 3};
        bool bestTrade = false;
        for (const auto& from : towns_)
        {
            if (&from == &to || failedWith(buyer, item, from.id))
                continue;
            auto& fm = markets[from.id];
            const int spare = (fm.spare.count(item) ? fm.spare.at(item) : 0) - (fm.want.count(item) ? fm.want.at(item) : 0);
            const bool trade = items::townGoodOf(from.id, item);
            if (spare > 3 && ((trade && !bestTrade) || (trade == bestTrade && spare > best.second)))
                best = {&from, spare}, bestTrade = trade;
        }
        return best;
    };
    std::set<std::pair<std::string, std::string>> signedToday;
    std::set<std::string> fresh;                            // The orders signed today.
    for (const auto& to : towns_)
        for (const auto& [item, want] : markets[to.id].want)
        {
            const auto local = markets[to.id].spare.count(item) ? markets[to.id].spare.at(item) : 0;
            const auto covered = ordered.count({to.id, item}) ? ordered.at({to.id, item}) : 0;
            if (want - local - covered < 4 || rotsOnTheRoad(item))
                continue;
            auto [best, bestSpare] = bestFor(to, item, {});
            if (!best)
                continue;
            for (const auto& [buyer, wanted] : markets[to.id].wanters[item])
            {
                if (bestSpare < 4)
                    break;
                if (buyers.count({buyer, item}))
                    continue;
                const auto [seller, spare] = failedWith(buyer, item, best->id) ? bestFor(to, item, buyer) : std::pair{best, bestSpare};
                if (!seller || spare < 4)
                    continue;
                StandingOrder o;
                o.id = "so" + std::to_string(roads_.nextId++);
                o.buyer = buyer;
                o.town = to.id;
                o.item = item;
                o.from = seller->id;
                // For a week of what it uses (doc 61), or what it is short now if that is more.
                const int weekly = int(std::ceil(society_.useRate(buyer, item) * 7));
                o.perWeek = std::clamp(std::max(wanted, weekly), 4, std::min(MostOfAGood, spare));
                o.price = orderPrice(*seller, item);
                o.since = calendarDays_;
                o.review = calendarDays_ + OrderSettles;      // (Looked at again once a week of its use is known.)
                if (seller == best)
                    bestSpare -= promised(best->id, item, o.perWeek);
                markets[seller->id].spare[item] -= promised(seller->id, item, o.perWeek);
                buyers.insert({buyer, item});
                signedToday.insert({seller->id, to.id});
                fresh.insert(o.id);
                recordEvent({"standing order", buyer, seller->id, to.market, 0, 0, item, o.perWeek, o.price,
                             std::to_string(o.perWeek) + " " + item + " a week from " + seller->id + " at " + std::to_string(o.price) + "p"});
                roads_.orders.push_back(std::move(o));
            }
        }
    // What the orders come to, told to the towns' makers and the houses (doc 61): how many of each good each town sends a
    // week, kept in store for the caravan; what each buyer's cost a week, kept in its till to pay for them.
    std::map<std::pair<std::string, std::string>, int> perWeek;
    std::map<std::string, std::int64_t> costs;
    for (const auto& o : roads_.orders)
    {
        perWeek[{o.from, o.item}] += o.perWeek;
        costs[o.buyer] += o.price * o.perWeek;
    }
    society_.setStandingOrders(std::move(perWeek), std::move(costs));
    // What each town has to spare now, over its own makers' wants and what the orders promise: as far as an order with it
    // may be raised till the next day's trade (agreeAgain).
    canSpare_.clear();
    for (const auto& [town, m] : markets)
        for (const auto& [item, n] : m.spare)
            canSpare_[{town, item}] = n - (m.want.count(item) ? m.want.at(item) : 0);
    canSpareKnown_ = true;
    // Each road with standing orders sends its trader's caravan once a week (on a day of its own), and at once when an
    // order is new; a wagon still on the road goes again next week. Those due go first, in an order of the day's (doc 64,
    // Phase 13: taken by name, new orders' wagons counted among them, the cap left Southmeadow's road to the city without
    // its week's caravan on the days it was reached), then the roads with an order new today, with what the cap leaves,
    // carrying only today's orders, unless their week's caravan is due before a wagon would be back: then the week's load
    // goes with them, and the road's week is served (sentEarly; it went again on its day, each order had two weeks' in
    // two days, and its town's producers were told of a week's sales missed that they had made the day before).
    std::map<std::pair<std::string, std::string>, std::map<std::string, int>> loads, freshLoads;
    std::map<std::pair<std::string, std::string>, std::vector<std::string>> freshIds;
    for (const auto& o : roads_.orders)
    {
        loads[{o.from, o.town}][o.item] += o.perWeek;
        if (fresh.count(o.id))
            freshLoads[{o.from, o.town}][o.item] += o.perWeek, freshIds[{o.from, o.town}].push_back(o.id);
    }
    const auto roadOf = [](const std::pair<std::string, std::string>& pair) { return pair.first + ">" + pair.second; };
    const auto dueIn = [&](const std::pair<std::string, std::string>& pair) {
        return int((std::hash<std::string>{}(roadOf(pair)) % 7 + 7 - std::uint64_t(today % 7)) % 7);
    };
    for (auto it = roads_.sentEarly.begin(); it != roads_.sentEarly.end();)
        it = it->second < today ? roads_.sentEarly.erase(it) : std::next(it);
    std::vector<std::pair<std::uint64_t, std::pair<std::string, std::string>>> due, extra;
    for (const auto& [pair, load] : loads)
        // (On its day, unless its week's went early: then only an order new today, by itself.)
        if (const bool onItsDay = dueIn(pair) == 0 && !roads_.sentEarly.count(roadOf(pair)); onItsDay || signedToday.count(pair))
            (onItsDay ? due : extra).push_back({std::hash<std::string>{}(roadOf(pair) + "|" + std::to_string(today)), pair});
    std::sort(due.begin(), due.end());
    std::sort(extra.begin(), extra.end());
    // A road's caravan, with the week's load (whole) or today's orders': whether it went.
    const auto send = [&](const std::pair<std::string, std::string>& pair, bool whole) {
        auto load = whole ? loads[pair] : freshLoads[pair];
        const auto* from = town(pair.first);
        const auto* to = town(pair.second);
        const bool going = std::any_of(roads_.caravans.begin(), roads_.caravans.end(), [&](const Caravan& c) {
            return !c.trader.empty() && c.from == pair.first && c.to == pair.second && c.status == "travelling";
        });
        if (!from || !to || going)
            return false;
        // What the load costs at home, and the trader can pay: a load it can't pay for whole goes as much of it as it
        // can (doc 64, Phase 13: it was all or nothing, and Larkfield's load to the city had come to 18,652p).
        const auto cost = [&] {
            std::int64_t sum = 0;
            for (const auto& [item, n] : load)
                if (items::good(item))
                    sum += Society::pennies(society_.townPrice(from->id, item)) * n;   // (About what it'll cost.)
            return sum;
        };
        const auto trader = traderOf(*from);
        const auto* purse = society_.account(trader);
        if (!purse)
            return false;
        const auto guards = society_.pennies(GuardsHire, from->id);
        const auto budget = std::max<std::int64_t>(0, purse->cash - (from->store == trader ? 0 : guards));
        if (const auto dear = cost(); dear > budget)
            for (auto& [item, n] : load)
                n = int(std::int64_t(n) * budget / dear);
        const auto spend = cost();
        int total = 0;
        for (const auto& [item, n] : load)
            total += items::good(item) ? n : 0;
        if (spend <= 0 || purse->cash < spend)
            return false;
        auto* c = sendCaravan(*from, *to, {});
        if (!c)
            return false;
        c->trader = trader;
        if (!whole)
            c->orders = freshIds[pair];
        // (What each order it carries is for a week as it sets out: its share of the load on arrival.)
        for (const auto& o : roads_.orders)
            if (o.from == pair.first && o.town == pair.second && (whole || fresh.count(o.id)))
                c->loaded[o.id] = o.perWeek;
        society_.shift(trader, c->account, "", 0, spend, "money for the road");
        // Two more guards for a load worth robbing, hired from the town's watch.
        if (from->store != trader && society_.shift(trader, from->store, "", 0, guards, "guards for the road"))
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
                    // (A ledger entry holds 99 at most: as many as it has, 99 at a time. Doc 64, Phase 13: one sale a
                    // quality, Larkfield's fifteen farms sent the city 1,485 oats a week, whatever was ordered.)
                    const auto each = std::max<std::int64_t>(1, Society::pennies(society_.priceAt(holder, sort)));   // (The holder's own.)
                    while (items::good(sort) && left > 0 && spare > 0)
                    {
                        const int k = int(std::min<std::int64_t>({left, spare, Society::stock(*society_.account(holder), sort), 99,
                                                                  society_.account(c->account)->cash / each}));
                        if (k <= 0 || !society_.sale(holder, c->account, sort, k, each, "bought for the road"))
                            break;
                        left -= k, spare -= k, bought += k;
                    }
                    if (left <= 0 || spare <= 0)
                        break;
                }
            }
            // What its town couldn't spare of the week's orders is a sale its producers missed (doc 64, Phase 13); not what
            // the wagon's money didn't stretch to at its holders' prices (what they still had to spare).
            if (whole && left > 0)
            {
                int still = 0;
                for (const auto& [holder, spare] : fm.holders[item])
                    still += std::max(0, spare);
                if (left > still)
                    society_.missedAt(from->id, item, left - still);
            }
        }
        // Rations for the carters and feed for the horses, bought at home and eaten on the way (the trader's cost).
        for (const auto& [item, n] : std::map<std::string, int>{{"bread", 2}, {"hay", 2}})
            for (auto& [holder, spare] : fm.holders[item])
                if (const auto each = Society::pennies(society_.priceAt(holder, item));
                    items::good(item) && spare >= n && society_.account(c->account)->cash >= each * n &&
                    society_.sale(holder, c->account, item, n, each, "provisions for the road"))
                {
                    society_.consume(c->account, item, n, "eaten on the road");
                    spare -= n;
                    break;
                }
        if (auto* w = entity("road:" + c->id))
            w->name = "a trader's caravan bound for " + to->id;
        recordEvent({"trade caravan departs", c->id, to->id, from->market, 0, 0, {}, bought, spend,
                     std::to_string(bought) + " of " + std::to_string(total) + " goods ordered for " + to->id + ", " + trader + "'s"});
        return true;
    };
    // A road due whose wagon didn't go (the cap reached, its trader without the money, a wagon still out): its orders are
    // short their week, as if it had come empty (doc 64, Phase 13: a seller that sent nothing had kept up, and was raised);
    // not one signed today, whose week begins.
    const auto unsent = [&](const std::pair<std::string, std::string>& pair) {
        for (auto& o : roads_.orders)
            if (o.from == pair.first && o.town == pair.second && !fresh.count(o.id))
                o.shortfall += o.perWeek;
    };
    int sent = 0;
    for (const auto& [drawn, pair] : due)
        if (sent < TradeCaravansADay && send(pair, true))
            ++sent;
        else
            unsent(pair);
    for (const auto& [drawn, pair] : extra)
    {
        if (sent >= TradeCaravansADay)
            break;
        const bool whole = dueIn(pair) <= RoundTripDays && !roads_.sentEarly.count(roadOf(pair));
        if (!send(pair, whole))
            continue;
        ++sent;
        if (whole)
            roads_.sentEarly[roadOf(pair)] = today + dueIn(pair);
    }
}

void World::residentsRenegotiate(std::set<std::string>& busy)
{
    // A standing order due to be looked at again (a week after it was signed or raised, else four weeks on, or
    // deliveries well short): a porter of the buyer's town walks to the selling town's market to renegotiate it, with
    // food for the road from the shop.
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
    // At the selling town's market: the order agreed again (agreeAgain). A shop that has piled up more than two weeks'
    // worth wants less; one that ran low, more; at the price the goods fetch there now. One that wants next to nothing
    // ends it.
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
        const int before = agreeAgain(o, *from);
        if (before < 0)
        {
            o.negotiator.clear();                   // (Its seller's room not known yet: still due, and looked at again.)
            ++it;
            continue;
        }
        if (const auto* shelves = society_.account(o.buyer))
            society_.shift(o.buyer, o.negotiator, "", 0, std::min<std::int64_t>(3, shelves->cash), "a negotiator's pay");
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

int World::agreeAgain(StandingOrder& o, const Town& from)
{
    const auto* shelves = society_.account(o.buyer);
    const int held = shelves ? Society::stockAll(*shelves, o.item) : 0;
    const int before = o.perWeek;
    // What a week of its use comes to (doc 61): an order is brought toward it, a shop running low (under half a week's
    // worth) raising it a quarter at least, one with more than two weeks' worth piled up lowering it a quarter at
    // most. (Not raised because the seller fell short, nor for use alone: looked at weekly, that raised orders the
    // seller couldn't fill and shops stocked from their own town, fruit and fish by the hundred, till the rescue fund
    // kept them open.)
    // (Doc 64, Phase 13: raised only when the seller kept up, a quarter short at most of all it was to send since, and as
    // far as a week and a quarter of its use, ShelfMost at most, where that is over the 90 a week it stopped at, under a
    // city bakery's flour; but by no more than the seller's town has to spare, nor more than twice over at once: a city
    // potter's clay went from 23 a week to 1,133, and Mirebridge sent none of it for three weeks. A seller that sent
    // under half has its order halved, and one that sent nothing in two weeks' worth loses it, so that another town may
    // be signed: it isn't signed again with the buyer for a month (RoadsState::failed). What its buyer couldn't pay for
    // isn't its seller's shortfall (tradeCaravanArrived). A food that rots on the road ends. A raise its seller's room
    // held where it was is looked at again in a week, as a raise is; one with that room not known yet, the day's trade
    // not run since the world was loaded, isn't agreed: the order stays due.)
    const int weekly = int(std::ceil(society_.useRate(o.buyer, o.item) * 7 * 1.1));
    const int most = std::min(std::max(MostOfAGood * 3, int(std::ceil(society_.useRate(o.buyer, o.item) * 7 * 1.25))), Society::ShelfMost);
    const int owed = o.delivered + o.shortfall;     // (What it was to send since: its trips' and the weeks none went.)
    bool raising = false;
    if (rotsOnTheRoad(o.item))
        o.perWeek = 0;
    else if (owed > 0 && o.delivered * 2 < owed)
    {
        o.perWeek = o.delivered == 0 && owed >= o.perWeek * 2 ? 0 : o.perWeek / 2;
        roads_.failed[o.buyer + "|" + o.item + "|" + o.from] = std::int64_t(std::floor(calendarDays_));
    }
    else if (held > std::max(o.perWeek, weekly) * 2)   // (Never raised for it: a week of its use over the order, it was.)
        o.perWeek = std::min(o.perWeek, std::max(weekly, o.perWeek * 3 / 4));
    else if (held < std::max(o.perWeek, weekly) / 2 && o.delivered > 0 && o.shortfall * 4 <= owed)
    {
        if (!canSpareKnown_)
            return -1;
        raising = true;
        auto& spare = canSpare_[{from.id, o.item}];
        const int room = std::max(0, spare) * (items::townGoodOf(from.id, o.item) ? 2 : 1);   // (Promised by half: promised.)
        o.perWeek = std::min({most, std::max(weekly, o.perWeek * 5 / 4 + 1), o.perWeek * 2, o.perWeek + room});
        spare -= promised(from.id, o.item, o.perWeek) - promised(from.id, o.item, before);
    }
    o.price = orderPrice(from, o.item);
    // Raised (or to be, its seller's room allowing), it is looked at again in a week, till it has caught up with the
    // shop's use; else in a month.
    o.review = calendarDays_ + (raising ? OrderSettles : MonthDays);
    o.delivered = o.shortfall = 0;
    return before;
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
    // An order due to be looked at again that no porter has gone for (a town whose people all have work to do): the
    // trader agrees it again here, on the same terms a porter would have had (doc 61; the towns' iron stayed at the few
    // bars of its first week). Before this week's goods are handed over (doc 64, Phase 3b): judged after, a shop that had
    // run out looked well stocked, and its order was never raised (Oakhaven's baker, 6 flour a week for 72 wolves). What
    // each order was loaded for is its week before that (doc 64, Phase 13: a load bought for the old orders, shared out
    // by the new, left every order on the road a shortfall its seller hadn't made, and none of them was raised again):
    // as it set out (Caravan::loaded), before a raise a porter agreed while it was on the road; an older save's, as they
    // stand on arrival.
    auto loadedFor = c.loaded;
    if (c.loaded.empty())
        for (const auto& o : roads_.orders)
            if (o.from == c.from && o.town == c.to)
                loadedFor[o.id] = o.perWeek;
    if (const auto* from = town(c.from))
        for (auto it = roads_.orders.begin(); it != roads_.orders.end();)
        {
            auto& o = *it;
            if (o.from != c.from || o.town != c.to || !o.negotiator.empty() ||
                (calendarDays_ < o.review && o.shortfall < o.perWeek * 2))
            {
                ++it;
                continue;
            }
            const int before = agreeAgain(o, *from);
            if (before < 0)
            {
                ++it;                               // (Its seller's room not known yet: still due.)
                continue;
            }
            recordEvent({o.perWeek < 2 ? "standing order ends" : "standing order renegotiated", c.trader, o.buyer, to->market, 0, 0,
                         o.item, o.perWeek, o.price,
                         o.item + " from " + o.from + ": " + std::to_string(before) + " a week, now " + std::to_string(o.perWeek) +
                             " at " + std::to_string(o.price) + "p, with the trader"});
            if (o.perWeek < 2)
                it = roads_.orders.erase(it);
            else
                ++it;
        }
    // Its standing orders first (those it carries: all its road's, or those signed the day it left), at the price
    // agreed, as far as each buyer can pay; then the rest to whoever wants it. Short of a good, each order has its share
    // of what came, then what is left in the order they were signed (doc 64, Phase 13: in that order alone, the supply
    // shops signed first took the city's oats and its inns had none).
    std::vector<StandingOrder*> carried;
    std::vector<int> week;                          // (Each one's, as far as it was loaded for.)
    std::map<std::string, int> asked, came;
    for (auto& o : roads_.orders)
        if (o.from == c.from && o.town == c.to && (c.orders.empty() || std::find(c.orders.begin(), c.orders.end(), o.id) != c.orders.end()))
        {
            const auto loaded = loadedFor.find(o.id);
            carried.push_back(&o);
            // (Not loaded for, signed after it set out: none of this load.)
            week.push_back(loaded == loadedFor.end() ? (c.loaded.empty() ? o.perWeek : 0) : std::min(o.perWeek, loaded->second));
            asked[o.item] += week.back();
        }
    for (const auto& [item, n] : asked)
        came[item] = Society::stockAll(*load, item);
    const auto hand = [&](const StandingOrder& o, int most) {
        int given = 0;
        for (const auto& sort : Society::kindsHeld(*society_.account(c.account), o.item))
            while (given < most)                    // (99 at a time: a ledger entry's most.)
            {
                const auto* purse = society_.account(o.buyer);
                const int k = int(std::min<std::int64_t>({most - given, Society::stock(*society_.account(c.account), sort), 99,
                                                          purse ? purse->cash / std::max<std::int64_t>(1, o.price) : 0}));
                if (k <= 0 || !society_.sale(c.account, o.buyer, sort, k, o.price, "a standing order"))
                    break;
                given += k, made += o.price * k;
            }
        return given;
    };
    std::vector<int> given(carried.size(), 0);
    for (std::size_t i = 0; i < carried.size(); ++i)
    {
        const auto& item = carried[i]->item;
        given[i] = hand(*carried[i], came[item] >= asked[item] ? week[i] : int(std::int64_t(came[item]) * week[i] / std::max(1, asked[item])));
    }
    for (std::size_t i = 0; i < carried.size(); ++i)
    {
        given[i] += hand(*carried[i], week[i] - given[i]);
        // (What the wagon still holds of it was refused for the buyer's want of money: not its seller's shortfall. Doc 64,
        // Phase 13: a buyer short of cash for two weeks had its order ended, and signed it again.)
        const int unpaid = std::min(week[i] - given[i], Society::stockAll(*society_.account(c.account), carried[i]->item));
        carried[i]->delivered += given[i];
        carried[i]->shortfall += week[i] - given[i] - unpaid;
    }
    load = society_.account(c.account);
    for (const auto& [item, n] : std::map<std::string, int>(load->stock.begin(), load->stock.end()))
    {
        const auto base = items::baseOf(item);
        const auto* good = items::good(item);
        if (n <= 0 || !good)
            continue;
        // What it cost the caravan and its carriage, with the carrier's margin (doc 64, Phase 4).
        const auto price = Society::pennies(society_.costOf(c.account, item) +
                                            society_.carriage(item, Society::CarriageLegs, to->id) * (1 + items::ladder().markup));
        int left = n;
        for (auto& [buyer, want] : market.wanters[base])
        {
            while (left > 0 && want > 0)            // (99 at a time.)
            {
                const auto* purse = society_.account(buyer);
                const int k = int(std::min<std::int64_t>({left, want, 99, purse ? purse->cash / price : 0}));
                if (k <= 0 || !society_.sale(c.account, buyer, item, k, price, "carted in by caravan"))
                    break;
                left -= k, want -= k, made += price * k;
            }
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
            // (At what the caravan paid for them: doc 64, Phase 4.)
            const auto each = Society::pennies(society_.costOf(c.account, item));
            for (int paid = good && store ? int(std::min<std::int64_t>(n, store->cash / each)) : 0; paid > 0; paid -= 99)
                if (!society_.sale(c.account, home->store, item, std::min(paid, 99), each, "unsold, back to the stores"))
                    break;
        }
    if (const auto cash = society_.account(c.account)->cash; cash > 0)
        society_.shift(c.account, c.trader, "", 0, cash, "trade takings");
}
} // namespace ratw
