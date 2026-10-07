// The economy orchestrator's channels (Docs/Design/46-economy-orchestrator.md, Phase 5): at each week's decision the
// holders over their band send what it says into funds (applyOrders), and each day every town's funds spend it through
// the game's own work (runChannels): odd jobs and materials for the town's works, hands hired at its businesses and farms,
// goods commissioned from its makers, food bought from its farms into its granary (sold on to its food shops when their
// shelves run low), and its spare goods bought and sent away in trade. Every penny pays someone for work or goods; none is
// handed to the poor, and none is made or lost.
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <vector>

namespace ratw
{
std::string Society::fundOf(const std::string& town, const std::string& channel)
{
    std::string slug = channel;
    std::replace(slug.begin(), slug.end(), ' ', '_');
    return "fund:" + town + ":" + slug;
}

void Society::applyOrders(const orchestra::Brief& brief)
{
    if (!brief.decided || orchestratorDials().mode != "on")
        return;
    openAccount(LandFund);
    // Which orders can be sent: not to a fund still holding what it was last sent (its holder keeps the money then; what
    // keeps growing there raises the pay of those who work for it, the wage table).
    std::vector<const orchestra::Order*> sending;
    std::map<std::string, std::int64_t> local;            // Holder -> what it sends to its own town's funds.
    std::int64_t landOrders = 0, landWanted = 0;
    for (const auto& o : brief.orders)
    {
        if (o.from == "land")
            landWanted += o.coins;
        if (!orchestra::channelLive(o.channel) || o.coins <= 0)
            continue;
        if (const auto* full = account(fundOf(o.town, o.channel)); full && full->cash >= o.coins)
            continue;
        sending.push_back(&o);
        if (o.from == "land")
            landOrders += o.coins;
        else
            local[o.from] += o.coins;
    }
    // What the holders send: to their own towns' funds as ordered, and their share of the land's orders that go.
    const double landPart = landWanted > 0 ? double(landOrders) / double(landWanted) : 0;
    std::map<std::string, std::int64_t> moved;            // Holder -> what it may send to its own town.
    for (const auto& h : brief.holders)
    {
        const auto* purse = h.toSpend > 0 ? account(h.id) : nullptr;
        if (!purse)
            continue;
        // (Never below what the plan has it keep, whatever it holds now: an hour has passed since the snapshot.)
        auto can = std::min<std::int64_t>(h.toSpend, purse->cash - (h.cash - h.toSpend));
        if (can <= 0)
            continue;
        std::int64_t wantedHere = 0;
        for (const auto& o : brief.orders)
            if (o.from == h.id)
                wantedHere += o.coins;
        const auto here = std::min(can, local.count(h.id) ? local.at(h.id) : 0);
        moved[h.id] = here;
        const auto toLand = std::int64_t(std::floor(double(std::max<std::int64_t>(0, std::min(can, h.toSpend) - wantedHere)) * landPart));
        if (toLand > 0)
            shift(h.id, LandFund, "", 0, std::min(toLand, can - here), "orders: to the land's fund");
    }
    for (const auto* o : sending)
    {
        const auto fund = fundOf(o->town, o->channel);
        openAccount(fund);
        if (o->from == "land")
        {
            if (const auto* land = account(LandFund))
                if (const auto n = std::min<std::int64_t>(o->coins, land->cash); n > 0)
                    shift(LandFund, fund, "", 0, n, "orders: " + o->channel);
        }
        else if (auto& left = moved[o->from]; left > 0)
        {
            const auto n = std::min(left, o->coins);
            if (shift(o->from, fund, "", 0, n, "orders: " + o->channel))
                left -= n;
        }
    }
}

void Society::runChannels(std::int64_t day)
{
    if (roster_ != Roster::Authored)
        return;
    // Each town's shops, makers, food shops and businesses (its keepers), its farms, and how many live there.
    std::map<std::string, std::vector<std::string>> shops, makers, foodShops, farms;
    std::map<std::string, std::size_t> folk;
    for (const auto& r : authored_.residents)
    {
        if (!state_.residents.count(r.id))
            continue;
        const auto town = communityOfResident(r.id);
        ++folk[town];
        if (r.role == "merchant")
        {
            shops[town].push_back(r.id);
            if (const auto* business = items::businessFor(r.workLabel); business && !items::craftsFor(business->id).empty())
                makers[town].push_back(r.id);
            if (foodShop(r.id))
                foodShops[town].push_back(r.id);
        }
        else if (items::producerFor(r.workLabel) && tillOf(r.id) != r.id)
            farms[town].push_back(r.id);
    }
    // Each town's food on its shops' shelves, in days a head (the playbook's food security).
    const auto shelfDays = [&](const std::string& town) {
        double onShelves = 0;
        for (const auto& shop : foodShops[town])
            if (const auto* a = account(tillOf(shop)))
                for (const auto& [item, n] : a->stock)
                    if (n > 0)
                        onShelves += double(n) * nourishment(item);
        return onShelves / (orchestratorDials().nourishADay * double(std::max<std::size_t>(1, folk[town])));
    };
    std::vector<std::pair<std::string, std::int64_t>> funds;
    for (auto it = state_.accounts.lower_bound("fund:"); it != state_.accounts.end() && it->first.rfind("fund:", 0) == 0; ++it)
        if (it->first != LandFund && it->second.cash > 0)
            funds.push_back({it->first, it->second.cash});
    for (const auto& [fund, cash] : funds)
    {
        const auto second = fund.find(':', 5);
        if (second == std::string::npos)
            continue;
        const auto town = fund.substr(5, second - 5);
        auto channel = fund.substr(second + 1);
        std::replace(channel.begin(), channel.end(), '_', ' ');
        // A part of it a day, so a week's orders are spent over the week.
        const auto budget = std::min(cash, std::max<std::int64_t>(cash / 4, 20));
        Spending note{fund, town, 0, {}};
        if (channel == "works")
        {
            // Odd jobs for those without work, and materials for the town's works, used up there.
            const auto wages = postOddJobs(fund, town, budget * 2 / 3);
            std::map<std::string, int> got;
            const auto& institutions = items::institutions();
            const auto built = buyForSurplus(fund, shops[town], [&](const std::string& item) {
                for (const auto& in : institutions)
                    if (in.id == "works")
                        for (const auto& [want, rate] : in.basket)
                            if (want == item)
                                return true;
                return item == "nails" || item == "timber" || item == "planks" || item == "bricks";
            }, budget - wages, "orders: materials for the town's works", &got);
            for (const auto& [item, n] : got)
                consume(fund, item, n, "used in the town's works");
            note.total = wages + built;
            note.detail = std::to_string(wages) + "p of odd jobs posted, " + std::to_string(built) + "p of materials";
        }
        else if (channel == "hires")
        {
            // Hands hired for the week at the town's businesses and farms, a few a day, paid from the fund as they work.
            const auto pay = std::int64_t(std::ceil(dayWage(town, "hand") - 1e-9));
            std::int64_t committed = 0;
            for (const auto& j : oddJobs_)
                if (j.payer == fund && j.until >= day)
                    committed += j.pay * (j.until - day + 1);
            std::vector<std::string> places = shops[town];
            places.insert(places.end(), farms[town].begin(), farms[town].end());
            // (A town short of food sends its hands to its food first: its food shops and the farms that grow food.)
            if (shelfDays(town) < orchestratorDials().foodDaysLow)
                std::stable_partition(places.begin(), places.end(), [&](const std::string& keeper) {
                    if (foodShop(keeper))
                        return true;
                    const auto* sp = spec(keeper);
                    const auto* producer = sp ? items::producerFor(sp->workLabel) : nullptr;
                    return producer && !producer->out.empty() && edible(producer->out.front().first);
                });
            int posted = 0;
            for (std::size_t k = 0; k < places.size() && posted < 3 && cash - committed >= pay * HireDays; ++k)
            {
                const auto& keeper = places[(std::size_t(day) * 7 + k) % places.size()];
                const auto* job = jobOf(keeper);
                const bool busy = std::any_of(oddJobs_.begin(), oddJobs_.end(), [&](const OddJob& j) {
                    return j.producer == keeper && j.until >= day && (j.kind == "a hand" || j.kind == "a hand at the shop");
                });
                if (!job || busy)
                    continue;
                const bool shop = job->role == "merchant";
                OddJob j;
                j.id = "odd" + std::to_string(++nextOddJob_);
                j.payer = fund;
                j.community = town;
                j.kind = shop ? "a hand at the shop" : "a hand";
                j.what = "a hand at " + job->title.substr(0, 40) + ", hired for the week";
                j.from = j.to = shop ? job->serve : job->work;
                j.producer = keeper;
                j.slots = 1;
                j.pay = pay;
                j.forChildren = false;
                j.until = day + HireDays - 1;
                oddJobs_.push_back(std::move(j));
        ++oddVersion_;
                committed += pay * HireDays;
                note.total += pay * HireDays;
                ++posted;
            }
            note.detail = std::to_string(posted) + " hands hired for the week at " + std::to_string(pay) + "p a day";
        }
        else if (channel == "commissions")
        {
            // Goods ordered from the town's makers: what they make, not food, kept by those who ordered them.
            std::map<std::string, int> got;
            note.total = buyForSurplus(fund, makers[town], [](const std::string& item) {
                const auto* good = items::good(item);
                return good && !edible(item) && good->price >= 2;
            }, budget, "orders: commissioned", &got);
            for (const auto& [item, n] : got)
                consume(fund, item, n, "commissioned and kept");
            note.detail = std::to_string(note.total) + "p of goods commissioned from its makers";
        }
        else if (channel == "food")
        {
            // Food that keeps (a fortnight or more: salt fish and pork, jerky, cheese, smoked meats, ship's biscuit) into its
            // granary (Phase 7): from its farms at the land's price, then from its shops at the town's, never a shop's last.
            const auto granary = "town:" + town + ":granary";
            openAccount(granary);
            const auto keeps = [](const std::string& item) {
                const auto* good = items::good(item);
                return good && edible(item) && !good->drink && (good->keeps <= 0 || good->keeps >= 14);
            };
            std::int64_t spent = 0;
            for (const auto& farm : farms[town])
            {
                const auto till = tillOf(farm);
                const auto* stock = account(till);
                if (!stock)
                    continue;
                for (const auto& [item, n] : std::map<std::string, int>(stock->stock.begin(), stock->stock.end()))
                {
                    if (n <= ProduceKept || !keeps(item) || spent >= budget)
                        continue;
                    const auto price = pennies(buyingPrice(town, item));
                    const int k = int(std::min<std::int64_t>({n - ProduceKept, (budget - spent) / price, 99}));
                    if (k > 0 && transfer(till, fund, item, k, price, "orders: food for the granary"))
                    {
                        shift(fund, granary, item, k, 0, "into the granary");
                        spent += price * k;
                    }
                }
            }
            // (From its own shops only what they can spare: not while their shelves hold under foodDaysLow days a head. A
            // store bought off bare shelves left Upper Accord's eaters short on the eve of a Restday: the playbook.)
            std::map<std::string, int> got;
            if (spent < budget && shelfDays(town) >= orchestratorDials().foodDaysLow)
                spent += buyForSurplus(fund, foodShops[town], keeps, budget - spent, "orders: food for the granary", &got);
            for (const auto& [item, n] : got)
                shift(fund, granary, item, n, 0, "into the granary");
            // Food from elsewhere (the playbook): a town with no food shops of its own, or short of food, has the carters
            // bring it from the land's best-stocked food shops (those with more than two days' food a head), at their prices.
            if (spent < budget && (foodShops[town].empty() || shelfDays(town) < orchestratorDials().foodDaysLow))
            {
                // (A town with no food shops at all takes from shops holding over a day: in winter few hold two.)
                std::vector<std::pair<double, std::string>> elsewhere;
                const double spare = foodShops[town].empty() ? 1 : 2;
                for (const auto& [other, list] : foodShops)
                    if (other != town && !list.empty())
                        if (const double d = shelfDays(other); d > spare)
                            elsewhere.push_back({-d, other});
                std::sort(elsewhere.begin(), elsewhere.end());
                std::vector<std::string> sellers;
                for (const auto& [d, other] : elsewhere)
                    sellers.insert(sellers.end(), foodShops[other].begin(), foodShops[other].end());
                std::map<std::string, int> brought;
                if (!sellers.empty())
                    spent += buyForSurplus(fund, sellers, keeps, budget - spent, "orders: food brought in for the granary", &brought);
                for (const auto& [item, n] : brought)
                    shift(fund, granary, item, n, 0, "into the granary, brought by the carters");
            }
            note.total = spent;
            note.detail = std::to_string(spent) + "p of food that keeps bought for the granary";
        }
        else if (channel == "trade")
        {
            // What the town's makers and farms have to spare, bought and sent away to be sold elsewhere: coins into the town.
            std::int64_t spent = 0;
            std::vector<std::string> sellers = makers[town];
            sellers.insert(sellers.end(), farms[town].begin(), farms[town].end());
            for (const auto& seller : sellers)
            {
                const auto till = tillOf(seller);
                const auto* stock = account(till);
                const bool farm = std::find(farms[town].begin(), farms[town].end(), seller) != farms[town].end();
                if (!stock)
                    continue;
                for (const auto& [item, n] : std::map<std::string, int>(stock->stock.begin(), stock->stock.end()))
                {
                    const int spare = n - (farm ? ProducerKept / 2 : SuppliesKept / 2);
                    if (spare <= 0 || spent >= budget || !items::good(item))
                        continue;
                    const auto price = pennies(farm ? buyingPrice(town, item) : townPrice(town, item));
                    const int k = int(std::min<std::int64_t>({spare, (budget - spent) / price, 99}));
                    if (k > 0 && transfer(till, fund, item, k, price, "orders: bought for trade"))
                    {
                        consume(fund, item, k, "sent away in trade");
                        spent += price * k;
                    }
                }
            }
            note.total = spent;
            note.detail = std::to_string(spent) + "p of its spare goods bought and sent away in trade";
        }
        if (note.total > 0)
            spendings_.push_back(std::move(note));
    }
    // Price support (Phase 6): what each shop's staples owe it, the gap under their price, from its town's fund.
    for (auto& [till, owed] : supportOwed_)
    {
        const auto town = shopTown_.find(till);
        const auto coins = std::int64_t(std::floor(owed));
        if (town == shopTown_.end() || coins <= 0)
            continue;
        const auto fund = fundOf(town->second, "price support");
        if (const auto* purse = account(fund); purse && purse->cash > 0)
        {
            const auto paid = std::min(coins, purse->cash);
            if (shift(fund, till, "", 0, paid, "price support"))
                owed -= double(paid);
        }
    }
    for (auto it = supportOwed_.begin(); it != supportOwed_.end();)
        it = it->second < 1 ? supportOwed_.erase(it) : std::next(it);
    // Business rescue (Phase 6): a business whose till has fallen under half its float is lent it back to its float from
    // its town's rescue fund; it repays from what it holds above two floats; a month on, what it owes is written off. (A
    // great house's business counts its days rescued, as its house's props did: ProppedDays of them in a month, and the
    // house sells it on.)
    auto& loans = state_.memory.loans;
    for (const auto& p : positions_)
    {
        const auto till = "till:" + p.id;
        const auto* purse = account(till);
        if (!purse || !(p.role == "merchant" || items::producerFor(p.title)))
            continue;
        const auto floatCash = floatOf(p.id);
        const auto town = p.role == "merchant" ? shopTown_.count(till) ? shopTown_.at(till) : communityOfResident(p.founder)
                                               : communityOfResident(p.founder);
        if (auto loan = loans.find(till); loan != loans.end())
        {
            if (const auto spare = purse->cash - 2 * floatCash; spare > 0)
            {
                const auto back = std::min(spare, loan->second.first);
                const auto fund = fundOf(town, "rescue");
                openAccount(fund);
                if (shift(till, fund, "", 0, back, "a rescue repaid"))
                    loan->second.first -= back;
            }
            if (loan->second.first <= 0 || day - loan->second.second >= MonthDays)
                loans.erase(loan);
        }
        if (purse->cash >= floatCash / 2)
            continue;
        const auto fund = fundOf(town, "rescue");
        const auto* rescue = account(fund);
        const auto lend = rescue ? std::min(floatCash - purse->cash, rescue->cash) : 0;
        if (lend > 0 && shift(fund, till, "", 0, lend, "a rescue: lent to keep it open"))
        {
            auto& loan = loans.try_emplace(till, std::pair<std::int64_t, std::int64_t>{0, day}).first->second;
            loan.first += lend;
            if (state_.houses.owner.count(p.id))
                state_.houses.propped[p.id].push_back(double(day));
            spendings_.push_back({fund, town, lend, std::to_string(lend) + "p lent to " + p.title.substr(0, 40) + " to keep it open"});
        }
    }
    ++state_.memory.revision;
    // The granaries: food to the town's food shops when their shelves run low, at the land's price, the takings back
    // into the town's food fund.
    for (auto it = state_.accounts.lower_bound("town:"); it != state_.accounts.end() && it->first.rfind("town:", 0) == 0; ++it)
    {
        const auto& id = it->first;
        if (id.size() < 14 || id.compare(id.size() - 8, 8, ":granary") != 0 || it->second.stock.empty())
            continue;
        const auto town = id.substr(5, id.size() - 13);
        // The messes first (the playbook): the watch's, the mines' and the quarries', with no food in them, draw a day's
        // from the granary (paid for as they can, at the land's price): where a town has no food shops of its own, nobody
        // carries food to its mess, and its guards went hungry on Restdays.
        for (const char* kind : {":watch", ":mines", ":quarries"})
        {
            const auto mess = "town:" + town + kind;
            const auto* rations = account(mess);
            if (!rations || hasFood(*rations))
                continue;
            int drawn = 0;
            for (const auto& [item, n] : std::map<std::string, int>(it->second.stock.begin(), it->second.stock.end()))
            {
                const auto price = pennies(buyingPrice(town, item));
                const int k = int(std::min<std::int64_t>({n, 12 - drawn, account(mess)->cash / std::max<std::int64_t>(1, price)}));
                if (n <= 0 || k <= 0 || !edible(item))
                    continue;
                if (transfer(id, mess, item, k, price, "from the granary, for the mess"))
                    drawn += k;
                if (drawn >= 12)
                    break;
            }
            if (drawn > 0 && (account(fundOf(town, "food")) || openAccount(fundOf(town, "food"))))
                shift(id, fundOf(town, "food"), "", 0, account(id)->cash, "the granary's takings");
        }
        if (it->second.stock.empty())
            continue;
        // A town with no food shops of its own (the fortresses) sells from its granary to those who live there: each
        // morning a hungry wolf carrying no food buys a day's, at the town's price (the user, 2026-10-07).
        if (foodShops[town].empty())
        {
            std::int64_t sold = 0;
            for (const auto& [rid, life] : state_.residents)
            {
                const auto* purse = account(rid);
                if (life.hunger < 40 || !purse || hasFood(*purse) || communityOfResident(rid) != town)
                    continue;
                for (const auto& [item, n] : std::map<std::string, int>(it->second.stock.begin(), it->second.stock.end()))
                {
                    const auto price = pennies(buyingPrice(town, item));
                    if (n > 0 && edible(item) && purse->cash >= price && transfer(id, rid, item, 1, price, "bought from the granary"))
                    {
                        sold += price;
                        break;
                    }
                }
            }
            if (sold > 0 && (account(fundOf(town, "food")) || openAccount(fundOf(town, "food"))))
                shift(id, fundOf(town, "food"), "", 0, account(id)->cash, "the granary's takings");
            if (it->second.stock.empty())
                continue;
        }
        // (The playbook's food security: the granary keeps its store. It fills the shelves to two days' food a head on the
        // eve of a Restday or a festival, when nobody cooks; in winter and spring whenever they hold under two days; and in
        // summer and autumn only for a shortage, under a day. Before, it sold whenever they held under a day and a half
        // all summer, and had nothing by winter.)
        const bool eve = calendar::weekdayOf(double(day + 1) + .5) == calendar::Restday || calendar::festivalDay(double(day + 1) + .5);
        const bool lean = season_ == 3 || season_ == 0;
        const double days = shelfDays(town), enough = eve || lean ? 2 : 1;
        if (days >= enough)
            continue;
        const double people = double(std::max<std::size_t>(1, folk[town]));
        double onShelves = days * orchestratorDials().nourishADay * people;
        const double wanted = enough * orchestratorDials().nourishADay * people;
        if (eve && days < 2)
            spendings_.push_back({id, town, 0, "the granary fills the shelves for the day nobody cooks"});
        const auto fund = fundOf(town, "food");
        openAccount(fund);
        for (const auto& shop : foodShops[town])
        {
            const auto till = tillOf(shop);
            for (const auto& [item, n] : std::map<std::string, int>(it->second.stock.begin(), it->second.stock.end()))
            {
                const auto* shelves = account(till);
                if (n <= 0 || !shelves)
                    continue;
                const auto price = pennies(buyingPrice(town, item));
                const int fill = int(std::ceil(std::max(0.0, wanted - onShelves) / std::max(1, nourishment(item))));
                const int k = int(std::min<std::int64_t>({n, FoodShelf - stockAll(*shelves, item), shelves->cash / price, 99, fill}));
                if (k > 0 && transfer(id, till, item, k, price, "from the granary"))
                {
                    shift(id, fund, "", 0, account(id)->cash, "the granary's takings");
                    onShelves += double(k) * nourishment(item);
                }
            }
        }
    }
}

// --- Money that stops (doc 46, "Money that stops": the user, 2026-10-06 and 07) --------------------------------------
// Each town's bank ("bank:<town>"): its savers' savings, put in at the reckoning and drawn when they run short, its books
// in the orchestrator's state. It keeps a reserve (a third of what its savers have in) and the orchestrator sends the rest
// out through its channels each week, like any holder over its band: a black hole the savers' money goes into and comes
// back out of where the orchestrator says (the user, 2026-10-07: no loans, nothing owed). Then the town levy, and what
// the comfortable spend their savings on (a hand about the home, a piece commissioned, a feast). Nothing is made or lost.
namespace
{
std::int64_t heldIn(const std::map<std::string, std::pair<std::string, std::int64_t>>& m, const std::string& id)
{
    const auto it = m.find(id);
    return it == m.end() ? 0 : it->second.second;
}
}

std::string Society::bankOf(const std::string& town)
{
    return "bank:" + town;
}

std::int64_t Society::savedAtBank(const std::string& resident) const
{
    return heldIn(state_.orchestrator.deposits, resident);
}

std::unordered_map<std::string, double> Society::bankWorths() const
{
    std::unordered_map<std::string, double> saved, worth;
    for (const auto& [id, s] : state_.orchestrator.deposits)
        saved[s.first] += double(s.second);
    for (const auto& [town, in] : saved)
        if (const auto* coins = account(bankOf(town)); coins && in > 0)
            worth[town] = std::clamp(double(coins->cash) / in, 0.0, 1.0);
    return worth;
}

std::int64_t Society::savingsWorth(const std::string& resident, const std::unordered_map<std::string, double>& worths) const
{
    const auto it = state_.orchestrator.deposits.find(resident);
    if (it == state_.orchestrator.deposits.end())
        return 0;
    const auto w = worths.find(it->second.first);
    return w == worths.end() ? 0 : std::int64_t(std::floor(double(it->second.second) * w->second));
}

double Society::taxLevel(const std::string& treasury) const
{
    const auto it = taxLevel_.find(treasury);
    return it == taxLevel_.end() ? 1.0 : it->second;
}

std::int64_t Society::payDue(const std::string& from, const std::string& to, std::int64_t due, const std::string& why)
{
    const auto* purse = account(from);
    if (due <= 0 || !purse)
        return 0;
    std::int64_t paid = 0;
    if (const auto k = std::min(due, purse->cash); k > 0 && shift(from, to, "", 0, k, why))
        paid += k;
    // The rest from what it has at the bank, as far as its bank has the coins.
    auto& deposits = state_.orchestrator.deposits;
    if (const auto saved = deposits.find(from); paid < due && saved != deposits.end())
    {
        const auto bank = bankOf(saved->second.first);
        const auto* coins = account(bank);
        if (const auto k = std::min({due - paid, saved->second.second, coins ? coins->cash : 0}); k > 0 && shift(bank, to, "", 0, k, why + " (from the bank)"))
        {
            paid += k;
            if ((saved->second.second -= k) <= 0)
                deposits.erase(saved);
        }
    }
    return paid;
}

void Society::applyGrants(const orchestra::Brief& brief)
{
    if (!brief.decided || orchestratorDials().mode != "on")
        return;
    for (const auto& g : brief.grants)
        if (const auto* bank = account(g.from); bank && account(g.to))
            if (const auto k = std::min(g.coins, bank->cash); k > 0)
                shift(g.from, g.to, "", 0, k, "from the bank's savings");
}

void Society::bankReckoning(std::map<std::string, Reckoning>& towns)
{
    const auto& dials = orchestratorDials();
    if (dials.mode != "on")
        return;
    auto& deposits = state_.orchestrator.deposits;
    // The town levy: a share of what each resident holds (in purse and what its savings are worth) above its levy line,
    // times its town's tax level, to its town.
    const auto worths = bankWorths();
    for (const auto& [id, life] : state_.residents)
    {
        const auto* purse = account(id);
        if (!purse || (merchant(id) && tillOf(id) == id))
            continue;                                   // (A keeper's purse that is its till: the orchestrator's already.)
        const auto treasury = treasuryOfResident(id);
        const auto line = std::int64_t(dials.levyLine * double(wealthLine(id)));
        const auto held = purse->cash + savingsWorth(id, worths);
        if (const auto due = std::int64_t(std::floor(double(held - line) * dials.levyShare * taxLevel(treasury))); due > 0)
            towns[treasury].levy += payDue(id, treasury, due, "a town levy");
    }
    // Deposits (the savers' own decision): the bank's terms' share of what each holds above
    // its deposit line (the orchestrator's: the playbook), into its town's bank.
    for (const auto& [id, life] : state_.residents)
    {
        const auto* purse = account(id);
        if (!purse || (merchant(id) && tillOf(id) == id))
            continue;
        auto town = communityOfResident(id);
        if (town.empty())
            town = capital_;
        if (const auto known = deposits.find(id); known != deposits.end())
            town = known->second.first;                 // (Its savings stay where it first put them.)
        const auto line = std::int64_t(dials.depositLine * double(wealthLine(id)));
        // (At the bank's terms: the orchestrator's, the playbook's.)
        if (const auto put = std::int64_t(double(purse->cash - line) * state_.orchestrator.memory.depositShare); put > 0)
        {
            openAccount(bankOf(town));
            if (shift(id, bankOf(town), "", 0, put, "saved at the bank"))
            {
                auto& saved = deposits[id];
                saved.first = town;
                saved.second += put;
            }
        }
    }
}

void Society::tendBank(std::int64_t day)
{
    (void)day;
    auto& deposits = state_.orchestrator.deposits;
    std::map<std::string, std::int64_t> drawnToday;
    for (auto it = deposits.begin(); it != deposits.end();)
    {
        const auto bank = bankOf(it->second.first);
        const auto* coins = account(bank);
        const auto* purse = account(it->first);
        const bool gone = !state_.residents.count(it->first);
        // A saver short of a week's food draws back up to its wealth line; one gone, its savings to its town.
        const auto want = gone ? it->second.second : purse && purse->cash < FoodADay * 7 ? wealthLine(it->first) - purse->cash : 0;
        const auto to = gone || !purse ? treasuryOf(it->second.first) : it->first;
        if (const auto k = std::min({want, it->second.second, coins ? coins->cash : 0});
            k > 0 && shift(bank, to, "", 0, k, gone ? "an unclaimed saving" : "drawn from the bank"))
        {
            it->second.second -= k;
            drawnToday[it->second.first] += k;
        }
        it = it->second.second <= 0 ? deposits.erase(it) : std::next(it);
    }
    // (A slow average of what each bank's savers draw a day: its reserve, at the orchestrator's snapshot.)
    for (const auto& [id, saved] : deposits)
        bankDrawn_.try_emplace(saved.first, 0.0);
    for (auto& [town, avg] : bankDrawn_)
        avg += (double(drawnToday.count(town) ? drawnToday.at(town) : 0) - avg) / 14;
}

void Society::savers(std::int64_t day, const std::map<std::string, LifeBody>& bodies)
{
    const auto& dials = orchestratorDials();
    if (dials.mode != "on")
        return;
    std::map<std::string, std::vector<std::string>> makers, foodShops;
    for (const auto& r : authored_.residents)
        if (r.role == "merchant" && state_.residents.count(r.id))
        {
            const auto town = communityOfResident(r.id);
            if (const auto* business = items::businessFor(r.workLabel); business && !items::craftsFor(business->id).empty())
                makers[town].push_back(r.id);
            if (foodShop(r.id))
                foodShops[town].push_back(r.id);
        }
    std::map<std::string, std::vector<std::string>> homes;
    for (const auto& [id, life] : state_.residents)
        if (!life.homeCell.empty() && bodies.count(id))
            homes[life.homeCell].push_back(id);
    std::set<std::string> hiring;                       // (Who has a hand about the home posted already.)
    std::map<std::string, std::int64_t> room;           // Town -> how many more: as many as it has idle wolves.
    for (const auto& [town, idle] : idleHands_)
        room[town] = idle;
    for (const auto& j : oddJobs_)
        if (j.kind == "about the home")
            hiring.insert(j.payer), --room[j.community];
    // (The best off first: they hire before the rest.)
    std::vector<std::pair<std::int64_t, const std::string*>> order;
    const auto worths = bankWorths();
    for (const auto& [id, life] : state_.residents)
        if (const auto* purse = account(id))
            order.push_back({purse->cash + savingsWorth(id, worths), &id});
    std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    const bool restday = calendar::weekdayOf(double(day)) == calendar::Restday;
    for (const auto& [wealth, who] : order)
    {
        const auto& id = *who;
        const auto& life = state_.residents.at(id);
        const auto body = bodies.find(id);
        const auto* r = spec(id);
        const auto* purse = account(id);
        if (body == bodies.end() || body->second.age < 16 || !r || !purse || (merchant(id) && tillOf(id) == id))
            continue;
        const auto line = double(wealthLine(id));
        const auto held = double(wealth);
        const auto town = communityOfResident(id);
        if (town.empty())
            continue;
        // A hand about the home: an odd job a day at its home, paid at its town's odd-job rate from its own purse.
        const auto pay = std::max<std::int64_t>(1, std::int64_t(std::ceil(dayWage(town, "odd job") - 1e-9)));
        if (held > dials.helpLine * line && purse->cash >= pay * 2 && !hiring.count(id) && !r->home.cell.empty() && room[town] > 0)
        {
            --room[town];
            OddJob j;
            j.id = "odd" + std::to_string(++nextOddJob_);
            j.payer = id;
            j.community = town;
            j.kind = "about the home";
            j.what = "a hand about the home: fetching, mending and scrubbing";
            j.from = j.to = r->home;
            j.slots = 1;
            j.pay = pay;
            oddJobs_.push_back(std::move(j));
            ++oddVersion_;
        }
        // A piece commissioned: once a week (its own day), a costly piece from its town's makers.
        const auto twoLines = 2 * dials.levyLine * line;
        if (held > twoLines && std::hash<std::string>{}(id) % 7 == std::size_t(day % 7))
        {
            std::map<std::string, int> got;
            const auto budget = std::min<std::int64_t>(purse->cash - std::int64_t(line), std::int64_t((held - twoLines) / 4));
            if (budget >= 10)
                buyForSurplus(id, makers[town], [](const std::string& item) {
                    const auto* good = items::good(item);
                    return good && !edible(item) && good->price >= 10;
                }, budget, "a piece commissioned", &got);
            for (const auto& [item, n] : got)
                consume(id, item, n, "kept at home");
        }
        // A feast on Restday: a treat for each of its household at home, eaten there and then.
        if (restday && held > dials.levyLine * line && !life.homeCell.empty())
        {
            const auto& members = homes[life.homeCell];
            std::map<std::string, int> got;
            buyForSurplus(id, foodShops[town], [](const std::string& item) {
                const auto* good = items::good(item);
                return good && edible(item) && !good->drink && good->price >= 3;
            }, std::min<std::int64_t>(purse->cash - std::int64_t(line), std::int64_t(members.size()) * 9), "a feast", &got);
            std::size_t next = 0;
            for (const auto& [item, n] : got)
                for (int k = 0; k < n && !members.empty(); ++k, ++next)
                {
                    consume(id, item, 1, "eat");
                    auto& guest = state_.residents.at(members[next % members.size()]);
                    guest.hunger = std::max(0., guest.hunger - nourishment(item) * 1.1);
                }
        }
    }
}
} // namespace ratw
