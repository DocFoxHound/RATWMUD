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

std::int64_t Society::townBudget(const std::string& treasury, std::int64_t wanted)
{
    if (!trial("town_budget") || wanted <= 0)
        return std::max<std::int64_t>(0, wanted);
    auto& left = budgetLeft_[treasury];
    const auto allowed = std::min(wanted, std::max<std::int64_t>(0, left));
    left -= allowed;
    return allowed;
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
    // Yesterday's odd jobs are over, but for businesses' hires still running: those who hold them come back to them.
    // A hire with places left unfilled pays more tomorrow; one fully taken, a little less (demand for work sets its pay).
    for (const auto& j : oddJobs_)
        if (j.until >= 0 && !j.producer.empty() && j.slots > 0)
        {
            auto& dayPay = hirePay_.try_emplace(j.producer, HirePay).first->second;
            dayPay = int(j.stage.size()) < j.slots ? std::min(MostHirePay, dayPay + HireRaise) : std::max(LeastHirePay, dayPay - 1);
        }
    oddJobs_.erase(std::remove_if(oddJobs_.begin(), oddJobs_.end(), [&](const OddJob& j) { return j.until < day; }), oddJobs_.end());
    for (auto& j : oddJobs_)
    {
        if (j.until >= 0 && !j.producer.empty() && j.slots > 0)
            j.pay = hirePay_[j.producer] * j.slots;   // (At today's pay.)
        for (auto& [who, stage] : j.stage)
            stage = 0, j.progress[who] = 0;
    }
    // How much each collector usually spends (a slow average of its days), for its reserve.
    for (auto& [id, avg] : state_.memory.outgoing)
        avg *= .8;
    for (const auto& [id, coins] : spentToday_)
        state_.memory.outgoing[id] += .2 * double(coins);
    spentToday_.clear();
    // Each treasury's takings and wages a day (running averages), and so today's budget for its extras: what it takes in
    // above its wages (TRIAL town_budget).
    for (auto [m, today] : {std::pair{&incomeAvg_, &incomeToday_}, std::pair{&wagesAvg_, &wagesToday_}})
    {
        for (const auto& [id, coins] : *today)
            m->try_emplace(id, double(coins));       // (A first day: as if every day were like it.)
        for (auto& [id, avg] : *m)
            avg = .8 * avg + .2 * double(today->count(id) ? today->at(id) : 0);
    }
    incomeToday_.clear();
    wagesToday_.clear();
    budgetLeft_.clear();
    for (const auto& [id, avg] : incomeAvg_)
        budgetLeft_[id] = std::int64_t(std::max(0., avg - (wagesAvg_.count(id) ? wagesAvg_.at(id) : 0.)));
    // Town wages by takings (the user, 2026-10-06): each treasury's wages move toward a bill of TownWageShare percent of
    // what it takes in, a fifth at most a day (Society::wageFor).
    for (const auto& [id, takings] : incomeAvg_)
    {
        auto& scale = townWageScale_.try_emplace(id, 1.).first->second;
        if (const double bill = wagesAvg_.count(id) ? wagesAvg_.at(id) : 0.; bill > 0)
            scale = std::clamp(scale * std::clamp(takings * TownWageShare / 100 / bill, .8, 1.25), .25, 3.);
    }
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
        // (TRIAL church_share: what the church holds above its floor, a tenth of it a day, to the households under the
        // land's median purse a head, by how far under: money at the bottom, as much as comes in.)
        if (trial("church_share") && landMedian_ > 0)
        {
            std::int64_t people = 0;
            for (const auto& [home, h] : homes)
                people += std::int64_t(h.members.size());
            const auto spare = (account(land)->cash - ChurchHead * people) / 10;
            std::vector<std::pair<double, const Household*>> under;     // (How far under, a head x members; household.)
            double all = 0;
            for (const auto& [home, h] : homes)
                if (const double perHead = double(h.held) / double(h.members.size()); perHead < double(landMedian_))
                {
                    under.push_back({(double(landMedian_) - perHead) * double(h.members.size()), &h});
                    all += under.back().first;
                }
            for (const auto& [weight, h] : under)
                if (const auto part = std::int64_t(double(std::max<std::int64_t>(0, spare)) * weight / std::max(1., all)); part > 0)
                {
                    const auto eldest = *std::max_element(h->members.begin(), h->members.end(), [&](const std::string& a, const std::string& b) {
                        const auto* ra = spec(a);
                        const auto* rb = spec(b);
                        return (ra ? ra->age : 0) < (rb ? rb->age : 0);
                    });
                    shift(land, eldest, "", 0, part, "the church's share");
                }
        }
    }
    // What each collector spends today, and in which town. The land's church spends in every town, each its share by need
    // (its poor, and a little by its size): the poorest places get the most.
    const auto poorIn = [&](const std::string& town) {
        int n = 0;
        for (const auto& who : folk[town])
            if (const auto* p = account(who); p && p->cash < 12)
                ++n;
        return n;
    };
    std::vector<std::tuple<std::string, std::string, std::int64_t>> work;   // Collector, town, budget.
    for (const auto& id : collectors)
    {
        const bool church = id == SharedChurch, house = id.rfind("house:", 0) == 0;
        const auto town = church ? std::string() : townOf(id);
        std::size_t people = 0;
        if (church)
            for (const auto& [t, members] : folk)
                people += members.size();
        else
            people = folk.count(town) ? folk.at(town).size() : 0;
        // The reserve: four weeks of its usual spending, and never less than a floor by what it looks after.
        std::int64_t floor = church ? ChurchHead * std::int64_t(people) : house ? houseFloor(id) : 30 * std::int64_t(people);
        // (A great house keeps only its floor, its businesses' floats: what it props its shops with and sends on the road
        // came back to it, and is no reason to keep more. It spends a HouseSurplusShare-th of the rest a day.)
        // (TRIAL church_valve: the church keeps only its floor, not four weeks of what it spends, which grows as it does.)
        const auto reserve = house || (church && trial("church_valve"))
                                 ? floor
                                 : std::max<std::int64_t>(floor, std::int64_t(MonthDays * (state_.memory.outgoing.count(id) ? state_.memory.outgoing.at(id) : 0)));
        const auto surplus = account(id)->cash - reserve;
        auto budget = surplus / (house ? HouseSurplusShare : SurplusShare);
        if (church)
        {
            if (budget < 10)
                continue;
            std::map<std::string, double> need;
            double all = 0;
            for (const auto& [t, members] : folk)
                if (!t.empty())
                    all += need[t] = poorIn(t) + members.size() / 10.0;
            for (const auto& [t, share] : need)
                if (const auto part = std::int64_t(double(budget) * share / std::max(1.0, all)); part >= 10)
                    work.emplace_back(id, t, part);
            continue;
        }
        // Relief (the user, 2026-10-05: funding follows need): a town with many poor (over a tenth under 12p) and not lean
        // funds odd jobs from up to a fiftieth of its treasury a day, above its reserve or not.
        const int poorHere = poorIn(town);
        const bool relief = !house && folk.count(town) && poorHere * 10 > int(folk[town].size()) && !treasuryLean(id);
        if (relief && !trial("town_budget"))
            budget = std::max(budget, account(id)->cash / 50);
        // (TRIAL town_budget: a treasury's extras from what it takes in above its wages, whatever it holds.)
        // (Its savings above its reserve still go out a tenth a day; relief comes from the budget.)
        if (trial("town_budget") && !house)
            budget = std::max<std::int64_t>(0, budget) + townBudget(id, relief ? account(id)->cash / 50 : 0);
        if (budget < 10 || town.empty() || !folk.count(town))
            continue;
        work.emplace_back(id, town, budget);
    }
    for (const auto& [id, town, budget] : work)
    {
        const bool church = id == SharedChurch, house = id.rfind("house:", 0) == 0;
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
                if (const auto* p = account(who); p && p->cash < 12 && !hasFood(*p))
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
            // Half the rest: odd jobs for the poor; the rest, candles for the church, burned.
            const auto jobs = postOddJobs(id, town, (budget - bought) / 2);
            std::map<std::string, int> candles;
            const auto lit = buyForSurplus(id, here, [](const std::string& item) { return item.rfind("candle", 0) == 0; },
                                           budget - bought - jobs, "surplus: candles", &candles);
            for (const auto& [item, n] : candles)
                consume(id, item, n, "burned in the church");
            note.total = bought + lit + jobs;
            note.detail = std::to_string(bought) + "p of food, " + std::to_string(given) + " given as alms to the poor; " +
                          std::to_string(jobs / OddJobPay) + " odd jobs posted; " + std::to_string(lit) + "p of candles";
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
            // Its businesses first: hands hired and premises improved (RatwOddJobs.cpp), the one least improved.
            std::int64_t invested = 0;
            {
                const Position* least = nullptr;
                std::string manager;
                for (const auto& [pid, owner] : state_.houses.owner)
                    if (owner == id)
                        if (const auto* business = position(pid))
                            if (const auto held = state_.careers.positions.find(pid); held != state_.careers.positions.end() &&
                                                                                    !held->second.holder.empty() &&
                                                                                    (!least || improvement(pid) < improvement(least->id)))
                                least = business, manager = held->second.holder;
                if (least)
                    invested = businessSpends(id, manager, *least, budget * 3 / 10);
            }
            // (What a house holds above its floor raises the wages it pays: Society::wageFor, the user, 2026-10-06.)
            const auto [bonus, hands] = pay(staff, (budget - invested) * 4 / 10, 4, "surplus: a bonus from the house");
            std::map<std::string, int> feast;
            const auto fed = buyForSurplus(id, here, [](const std::string& item) { return edible(item); }, (budget - invested - bonus) / 2,
                                           "surplus: a feast", &feast);
            for (const auto& [item, n] : feast)
                consume(id, item, n, "eaten at the house's feast");
            std::map<std::string, int> fine;
            const auto commissioned = buyForSurplus(id, here, [](const std::string& item) {
                const auto* good = items::good(item);
                return good && !edible(item) && good->price >= 4;
            }, budget - invested - bonus - fed, "surplus: commissioned", &fine);
            for (const auto& [item, n] : fine)
                consume(id, item, n, "kept at the house");   // (Out of circulation as goods: the house's own.)
            note.total = invested + bonus + fed + commissioned;
            note.detail = std::to_string(invested) + "p in hands and premises for its businesses, " + std::to_string(bonus) +
                          "p in bonuses to " + std::to_string(hands) + " of its people, " +
                          std::to_string(fed) + "p on a feast, " + std::to_string(commissioned) + "p of fine goods commissioned";
        }
        else
        {
            // A town: odd jobs for those without work and the children (the user, 2026-10-05: more of them the more of
            // its people are poor), and materials for its works.
            int poor = 0;
            for (const auto& who : folk[town])
                if (const auto* p = account(who); p && p->cash < 12)
                    ++poor;
            const bool hard = folk[town].size() > 0 && poor * 10 > int(folk[town].size());
            const auto wages = postOddJobs(id, town, hard ? budget * 3 / 4 : budget / 2);
            const auto hired = int(wages / OddJobPay);
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
                note.detail = std::to_string(hired) + " odd jobs posted (" + std::to_string(wages) + "p, paid when done), " +
                              std::to_string(built) + "p of materials";
        }
        if (note.total > 0)
            spendings_.push_back(std::move(note));
    }
    // Wages follow takings (the user, 2026-10-05): each shop shares a part of what it took in today among its help.
    for (const auto& job : positions_)
    {
        if (job.role != "merchant")
            continue;
        const auto keeper = state_.careers.positions.find(job.id);
        if (keeper == state_.careers.positions.end() || keeper->second.holder.empty())
            continue;
        const auto till = tillOf(keeper->second.holder);
        const auto taken = takings_.count(till) ? takings_.at(till) : 0;
        std::vector<std::string> help;
        for (const auto& p : positions_)
            if (p.id != job.id && p.role != "merchant" && p.work.cell == job.work.cell)
                if (const auto held = state_.careers.positions.find(p.id); held != state_.careers.positions.end() && !held->second.holder.empty())
                    help.push_back(held->second.holder);
        const auto* purse = account(till);
        if (help.empty() || taken <= 0 || !purse)
            continue;
        const auto keep = floatOf(job.id) / 2 + (till == keeper->second.holder ? KeeperReserve : 0);
        const auto pot = std::min<std::int64_t>(taken * TakingsShare / 10, purse->cash - keep);
        const auto each = std::min<std::int64_t>(TakingsCap, pot / std::int64_t(help.size()));
        if (each > 0)
            for (const auto& who : help)
                shift(till, who, "", 0, each, "a share of the day's takings");
        // More than its help can share: more hands, hired for the week (businessSpends puts it to hires and premises).
        if (const auto over = pot - each * std::int64_t(help.size()); over >= HirePay)
            businessSpends(till, keeper->second.holder, job, over);
    }
    tendPrices(day);                                // (By today's takings: before they are forgotten.)
    takings_.clear();
    // A shopkeeper whose shop is its own (the user, 2026-10-05: nothing hoards): above its food money and a float for the
    // shop, a KeeperSurplusShare-th of what it holds goes out each day (the user, 2026-10-06: where money pools, it goes
    // back out; two floats and a tenth kept a third of the land's money on the keepers' counters). Four tenths to the shop's help, a share of the takings (so
    // wages follow what the shop takes in); a third of the rest on food for its own larder; the rest on goods from its
    // town's other shops, kept at home.
    for (const auto& r : authored_.residents)
    {
        const auto* job = r.role == "merchant" && state_.residents.count(r.id) && tillOf(r.id) == r.id ? jobOf(r.id) : nullptr;
        const auto* purse = job ? account(r.id) : nullptr;
        if (!purse)
            continue;
        const auto reserve = KeeperReserve + floatOf(job->id);
        auto budget = (purse->cash - reserve) / KeeperSurplusShare;
        const auto town = communityOfResident(r.id);
        if (budget < 10 || town.empty())
            continue;
        Spending note{r.id, town, 0, {}};
        // Half of it put to work first: hands for the shop, its premises improved (RatwOddJobs.cpp).
        const auto invested = businessSpends(r.id, r.id, *job, budget / 2);
        budget -= invested;
        std::int64_t shared = 0;
        int hands = 0;
        for (const auto& p : positions_)
        {
            if (p.id == job->id || p.work.cell != job->work.cell || shared + 2 > budget * 4 / 10)
                continue;
            const auto held = state_.careers.positions.find(p.id);
            if (held != state_.careers.positions.end() && !held->second.holder.empty() &&
                shift(r.id, held->second.holder, "", 0, 2, "surplus: a share of the takings"))
                shared += 2, ++hands;
        }
        std::vector<std::string> others;
        for (const auto& shop : shops[town])
            if (shop != r.id)
                others.push_back(shop);
        const auto larder = homeStore(resident(r.id) ? resident(r.id)->homeCell : r.home.cell, "larder");
        std::map<std::string, int> food;
        const auto stocked = buyForSurplus(r.id, others, [](const std::string& item) { return edible(item); }, (budget - shared) / 3,
                                           "surplus: the keeper's larder", &food);
        for (const auto& [item, n] : food)
            if (!account(larder) || !shift(r.id, larder, item, n, 0, "put away in the larder"))
                consume(r.id, item, n, "eaten at the keeper's table");
        std::map<std::string, int> fine;
        const auto kept = buyForSurplus(r.id, others, [](const std::string& item) {
            const auto* good = items::good(item);
            return good && !edible(item) && good->price >= 2;
        }, budget - shared - stocked, "surplus: goods for the keeper's home", &fine);
        for (const auto& [item, n] : fine)
            consume(r.id, item, n, "kept at the keeper's home");
        note.total = invested + shared + stocked + kept;
        note.detail = std::to_string(invested) + "p in hands and its premises, " + std::to_string(shared) + "p shared with " +
                      std::to_string(hands) + " of its help, " + std::to_string(stocked) +
                      "p of food for its larder, " + std::to_string(kept) + "p of goods for its home";
        if (note.total > 0)
            spendings_.push_back(std::move(note));
    }
    // Farmhands (RatwFarmhands.cpp): hires ended and lodgers home, new hires posted, bunkhouse larders stocked.
    endLodgings(day);
    postFarmHires(day);
    stockBunkhouses(day);
    // A producer doing well (a farmer, a quarryman with more than it needs) puts a tenth of its spare to work the same way:
    // hands for its fields, its ground improved.
    for (const auto& r : authored_.residents)
    {
        const auto* job = state_.residents.count(r.id) ? jobOf(r.id) : nullptr;
        // (Only one without a till of its own: a farm's till spends above its floats, RatwTills.cpp.)
        const auto* purse = job && items::producerFor(job->title) && tillOf(r.id) == r.id ? account(r.id) : nullptr;
        if (purse && purse->cash > 300)
            businessSpends(r.id, r.id, *job, (purse->cash - 300) / SurplusShare);
    }
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
