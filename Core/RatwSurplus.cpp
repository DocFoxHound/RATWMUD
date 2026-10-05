// The rule against hoarding (Docs/Design/42-money-in-circulation.md): every place money gathers (a town's treasury, a
// church, a great house) keeps a reserve, four weeks of what it usually spends, and sends a tenth of anything above it
// back out each day, in ways that suit it: a town hires hands and buys materials for its works, a church feeds the poor,
// a house rewards its people, feasts them and commissions fine goods. Spending pays wolves for work or goods, or gives
// the poor food; it never makes or destroys money.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
namespace
{
bool collector(const std::string& id)
{
    return id == "treasury" || id.rfind("stores:", 0) == 0 || id.rfind("house:", 0) == 0 ||
           (id.rfind("town:", 0) == 0 && id.size() > 7 && id.compare(id.size() - 7, 7, ":church") == 0);
}
} // namespace

void Society::noteOutgoing(const std::string& from, const std::string& kind, std::int64_t coins)
{
    // What a collector spends in the ordinary way (wages, its buyers' funds...), for its reserve; not what it spends
    // of a surplus.
    if (coins > 0 && collector(from) && kind.rfind("surplus: ", 0) != 0)
        spentToday_[from] += coins;
}

std::vector<Society::Spending> Society::takeSpendings()
{
    std::vector<Spending> out;
    out.swap(spendings_);
    return out;
}

std::int64_t Society::buyForSurplus(const std::string& buyer, const std::vector<std::string>& shops,
                                    const std::function<bool(const std::string& item)>& wanted, std::int64_t budget,
                                    const std::string& kind, std::map<std::string, int>* got)
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
            if (n <= 1 || !good || !wanted(items::baseOf(item)))
                continue;
            const std::int64_t price = std::max(1, good->price);
            // Never the shop's last few: a town's shelves are for its townsfolk first.
            const int count = int(std::min<std::int64_t>({(n - 1) / 2, 20, (budget - spent) / price, account(buyer)->cash / price}));
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
    // How much each collector usually spends (a slow average of its days), for its reserve.
    for (auto& [id, avg] : outgoing_)
        avg *= .8;
    for (const auto& [id, coins] : spentToday_)
        outgoing_[id] += .2 * double(coins);
    spentToday_.clear();
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
    const auto townOf = [&](const std::string& id) -> std::string {
        if (id == "treasury")
            return capital_;
        if (id.rfind("stores:", 0) == 0)
            return id.substr(7);
        if (id.rfind("town:", 0) == 0)
            return id.substr(5, id.size() - 5 - 7);
        for (const auto& h : houses())
            if (h.id == id)
                return h.community;
        return {};
    };
    std::vector<std::string> collectors;
    for (const auto& [id, a] : state_.accounts)
        if (collector(id) && a.cash > 0)
            collectors.push_back(id);
    for (const auto& id : collectors)
    {
        const auto town = townOf(id);
        const auto people = folk.count(town) ? folk.at(town).size() : 0;
        const bool church = id.rfind("town:", 0) == 0, house = id.rfind("house:", 0) == 0;
        // The reserve: four weeks of its usual spending, and never less than a floor by what it looks after.
        std::int64_t floor = church ? 10 * std::int64_t(people) : 30 * std::int64_t(people);
        if (house)
        {
            floor = 200;
            for (const auto& [pid, owner] : state_.houses.owner)
                if (owner == id)
                    floor += 2 * floatOf(pid);
        }
        const auto reserve = std::max<std::int64_t>(floor, std::int64_t(MonthDays * (outgoing_.count(id) ? outgoing_.at(id) : 0)));
        const auto surplus = account(id)->cash - reserve;
        const auto budget = surplus / SurplusShare;
        if (budget < 10 || town.empty() || !folk.count(town))
            continue;
        Spending note{id, town, 0, {}};
        const auto& here = shops[town];
        const auto pay = [&](const std::vector<std::string>& to, std::int64_t pot, std::int64_t each, const std::string& kind) {
            std::int64_t paid = 0;
            int n = 0;
            for (const auto& who : to)
            {
                if (pot - paid < each)
                    break;
                if (shift(id, who, "", 0, each, kind))
                    paid += each, ++n;
            }
            return std::pair<std::int64_t, int>{paid, n};
        };
        if (church)
        {
            // Alms: bread (or whatever feeds most cheaply) for the town's poorest, given, not sold.
            std::vector<std::string> poor;
            for (const auto& who : folk[town])
                if (const auto* p = account(who); p && p->cash < 12 && bestFood(*p).empty())
                    poor.push_back(who);
            std::map<std::string, int> bread;
            const auto bought = buyForSurplus(id, here, [](const std::string& item) { return edible(item); }, budget * 7 / 10,
                                              "surplus: alms bought", &bread);
            int given = 0;
            for (const auto& who : poor)
                for (auto& [item, n] : bread)
                    if (n > 0 && shift(id, who, item, 1, 0, "alms"))
                    {
                        --n;
                        ++given;
                        break;
                    }
            // What nobody needed today is eaten at the church's own table.
            for (const auto& [item, n] : bread)
                if (n > 0)
                    consume(id, item, n, "the church's table");
            // The rest: candles for the church, burned.
            std::map<std::string, int> candles;
            const auto lit = buyForSurplus(id, here, [](const std::string& item) { return item.rfind("candle", 0) == 0; },
                                           budget - bought, "surplus: candles", &candles);
            for (const auto& [item, n] : candles)
                consume(id, item, n, "burned in the church");
            note.total = bought + lit;
            note.detail = std::to_string(bought) + "p of food, " + std::to_string(given) + " given as alms to the poor; " +
                          std::to_string(lit) + "p of candles";
        }
        else if (house)
        {
            // A bonus to those who work at its businesses, a feast for its people, and fine things commissioned.
            std::vector<std::string> staff;
            for (const auto& [pid, owner] : state_.houses.owner)
                if (owner == id)
                    if (const auto* business = position(pid))
                        for (const auto& p : positions_)
                            if (p.work.cell == business->work.cell)
                                if (const auto held = state_.careers.positions.find(p.id); held != state_.careers.positions.end() &&
                                                                                            !held->second.holder.empty())
                                    staff.push_back(held->second.holder);
            const auto [bonus, hands] = pay(staff, budget * 4 / 10, 4, "surplus: a bonus from the house");
            std::map<std::string, int> feast;
            const auto fed = buyForSurplus(id, here, [](const std::string& item) { return edible(item); }, (budget - bonus) / 2,
                                           "surplus: a feast", &feast);
            for (const auto& [item, n] : feast)
                consume(id, item, n, "eaten at the house's feast");
            std::map<std::string, int> fine;
            const auto commissioned = buyForSurplus(id, here, [](const std::string& item) {
                const auto* good = items::good(item);
                return good && !edible(item) && good->price >= 4;
            }, budget - bonus - fed, "surplus: commissioned", &fine);
            for (const auto& [item, n] : fine)
                consume(id, item, n, "kept at the house");   // (Out of circulation as goods: the house's own.)
            note.total = bonus + fed + commissioned;
            note.detail = std::to_string(bonus) + "p in bonuses to " + std::to_string(hands) + " of its people, " +
                          std::to_string(fed) + "p on a feast, " + std::to_string(commissioned) + "p of fine goods commissioned";
        }
        else
        {
            // A town: hands hired for its works (those labouring for it or out of work in the wild), and materials.
            std::vector<std::string> hands;
            for (const auto& who : folk[town])
                if (const auto* life = resident(who); life && (life->task == LabourTitle || outworkTitled(life->task) || life->task == "looking for work"))
                    hands.push_back(who);
            const auto [wages, hired] = pay(hands, budget / 2, 4, "surplus: hired for the town's works");
            std::map<std::string, int> materials;
            const auto& works = items::institutions();
            const auto built = buyForSurplus(id, here, [&](const std::string& item) {
                for (const auto& in : works)
                    if (in.id == "works")
                        for (const auto& [want, rate] : in.basket)
                            if (want == item)
                                return true;
                return item == "nails" || item == "timber" || item == "planks" || item == "bricks";
            }, budget - wages, "surplus: the town's works", &materials);
            for (const auto& [item, n] : materials)
                consume(id, item, n, "used in the town's works");
            note.total = wages + built;
            // Nothing to spend it on in town: it goes to the church, for the poor.
            if (note.total == 0 && shift(id, churchOf(id == "treasury" ? "treasury" : id), "", 0, budget, "surplus: for the poor"))
                note.total = budget, note.detail = std::to_string(budget) + "p to the church, for the poor";
            else
                note.detail = std::to_string(wages) + "p to " + std::to_string(hired) + " hands hired for its works, " +
                              std::to_string(built) + "p of materials";
        }
        if (note.total > 0)
            spendings_.push_back(std::move(note));
    }
}
} // namespace ratw
