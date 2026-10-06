// The economy orchestrator's channels (Docs/Design/46-economy-orchestrator.md, Phase 5): at each week's decision the
// holders over their band send what it says into funds (applyOrders), and each day every town's funds spend it through
// the game's own work (runChannels): odd jobs and materials for the town's works, hands hired at its businesses and farms,
// goods commissioned from its makers, food bought from its farms into its granary (sold on to its food shops when their
// shelves run low), and its spare goods bought and sent away in trade. Every penny pays someone for work or goods; none is
// handed to the poor, and none is made or lost.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <map>
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
                    const auto price = std::max<std::int64_t>(1, std::int64_t(std::floor(buyingPrice(town, item))));
                    const int k = int(std::min<std::int64_t>({n - ProduceKept, (budget - spent) / price, 99}));
                    if (k > 0 && transfer(till, fund, item, k, price, "orders: food for the granary"))
                    {
                        shift(fund, granary, item, k, 0, "into the granary");
                        spent += price * k;
                    }
                }
            }
            std::map<std::string, int> got;
            if (spent < budget)
                spent += buyForSurplus(fund, foodShops[town], keeps, budget - spent, "orders: food for the granary", &got);
            for (const auto& [item, n] : got)
                shift(fund, granary, item, n, 0, "into the granary");
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
                    const auto price = std::max<std::int64_t>(1, std::int64_t(std::floor(farm ? buyingPrice(town, item) : townPrice(town, item))));
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
        double onShelves = 0;
        for (const auto& shop : foodShops[town])
            if (const auto* a = account(tillOf(shop)))
                for (const auto& [item, n] : a->stock)
                    if (n > 0)
                        onShelves += double(n) * nourishment(item);
        const double people = double(std::max<std::size_t>(1, folk[town]));
        // (Three days' food a head on its shelves: enough. In summer and autumn the store is kept for the winter, and goes
        // out only below a day and a half's.)
        const double enough = season_ == 1 || season_ == 2 ? 1.5 : 3;
        if (onShelves >= enough * 50 * people)
            continue;
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
                const auto price = std::max<std::int64_t>(1, std::int64_t(std::floor(buyingPrice(town, item))));
                const int k = int(std::min<std::int64_t>({n, FoodShelf - stockAll(*shelves, item), shelves->cash / price, 99}));
                if (k > 0 && transfer(id, till, item, k, price, "from the granary"))
                    shift(id, fund, "", 0, account(id)->cash, "the granary's takings");
            }
        }
    }
}
} // namespace ratw
