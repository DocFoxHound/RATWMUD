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
    // Churches stand by each other (a diocese): one run dry (under 2p a head) is given what it lacks of 5p a head by the
    // richest that holds more than twice its own floor.
    {
        std::map<std::string, std::int64_t> heads;
        for (const auto& [who, life] : state_.residents)
            ++heads[day_.communityOf ? day_.communityOf(life.homeCell) : std::string()];
        std::string richest;
        double most = 0;
        for (const auto& id : collectors)
            if (id.rfind("town:", 0) == 0)
                if (const auto n = heads[townOf(id)]; n > 0)
                    if (const double spare = double(account(id)->cash) / double(n * ChurchHead); spare > 2 && spare > most)
                        most = spare, richest = id;
        for (const auto& [id, a] : state_.accounts)
            if (id.rfind("town:", 0) == 0 && id.size() > 7 && id.compare(id.size() - 7, 7, ":church") == 0 && id != richest && !richest.empty())
                if (const auto n = heads[townOf(id)]; n > 0 && a.cash < 2 * n)
                    shift(richest, id, "", 0, std::min<std::int64_t>(5 * n - a.cash, account(richest)->cash / 10), "from the mother church");
    }
    // Each church feeds its town's poor every day, surplus or not (the user, 2026-10-05: no one starves with a church in
    // town): a day's food each to those with no food and no pennies to buy it (a child whose larder is empty, an
    // apprentice), hungry yet or not, bought at the town's shops.
    for (const auto& id : collectors)
    {
        if (id.rfind("town:", 0) != 0)
            continue;
        const auto town = townOf(id);
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
    }
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
        const auto reserve = std::max<std::int64_t>(floor, std::int64_t(MonthDays * (state_.memory.outgoing.count(id) ? state_.memory.outgoing.at(id) : 0)));
        const auto surplus = account(id)->cash - reserve;
        auto budget = surplus / SurplusShare;
        // Relief (the user, 2026-10-05: funding follows need): a town with many poor (over a tenth under 12p) and not lean
        // funds odd jobs from up to a fiftieth of its treasury a day, above its reserve or not.
        int poorHere = 0;
        for (const auto& who : folk[town])
            if (const auto* p = account(who); p && p->cash < 12)
                ++poorHere;
        const bool relief = !church && !house && folk.count(town) && poorHere * 10 > int(folk[town].size()) && !treasuryLean(id);
        if (relief)
            budget = std::max(budget, account(id)->cash / 50);
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
    takings_.clear();
    // A shopkeeper whose shop is its own (the user, 2026-10-05: nothing hoards): above its food money and two floats for
    // the shop, a tenth of what it holds goes out each day. Four tenths to the shop's help, a share of the takings (so
    // wages follow what the shop takes in); a third of the rest on food for its own larder; the rest on goods from its
    // town's other shops, kept at home.
    for (const auto& r : authored_.residents)
    {
        const auto* job = r.role == "merchant" && state_.residents.count(r.id) && tillOf(r.id) == r.id ? jobOf(r.id) : nullptr;
        const auto* purse = job ? account(r.id) : nullptr;
        if (!purse)
            continue;
        const auto reserve = KeeperReserve + 2 * floatOf(job->id) + 50;
        auto budget = (purse->cash - reserve) / SurplusShare;
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
    // A producer doing well (a farmer, a quarryman with more than it needs) puts a tenth of its spare to work the same way:
    // hands for its fields, its ground improved.
    for (const auto& r : authored_.residents)
    {
        const auto* job = state_.residents.count(r.id) ? jobOf(r.id) : nullptr;
        const auto* purse = job && items::producerFor(job->title) ? account(r.id) : nullptr;
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
