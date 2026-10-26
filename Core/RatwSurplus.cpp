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

// Whether a business makes things from a good (a provisioner's rabbits for smoking): one of its own crafts' materials.
bool madeFrom(const items::Business* business, const std::string& base)
{
    if (business)
        for (const auto* k : items::craftsFor(business->id))
            for (const auto& [in, q] : k->in)
                if (in == base)
                    return true;
    return false;
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
        std::vector<std::string> made;
        const auto& sold = waresKept(shop, made);
        const std::vector<std::pair<std::string, int>> held(stockHeld->stock.begin(), stockHeld->stock.end());
        for (const auto& [item, n] : held)
        {
            // (What it sells, not a maker's materials: doc 64, Phase 13.)
            const auto* good = items::good(item);
            if (n < (lastToo ? 1 : 2) || !good || !wanted(items::baseOf(item)) || !onSale(shop, item, sold))
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

bool Society::onSale(const std::string& shop, const std::string& item, const std::vector<std::string>& sold) const
{
    // (A baker's flour and firewood, a carpenter's timber, sit in the same till as its wares: not for sale; nor a
    // provisioner's rabbits, food it smokes, though an inn's raw meat, bought in from hunters to sell on, is.)
    const auto base = items::baseOf(item);
    if (std::find(sold.begin(), sold.end(), base) != sold.end())
        return true;
    if (!edible(item) || !foodShop(shop))
        return false;
    const auto* keeper = spec(shop);
    return !madeFrom(keeper ? items::businessFor(keeper->workLabel) : nullptr, base);
}

const std::vector<std::string>& Society::waresKept(const std::string& shop, std::vector<std::string>& made) const
{
    if (const auto* r = roster_ == Roster::Demo ? nullptr : spec(shop))
        if (const auto kept = waresCache_.find(shop); kept != waresCache_.end() && kept->second.first == r->workLabel)
            return kept->second.second;
    made = wares(shop);
    return made;
}

std::vector<Society::Offer> Society::offersOf(const std::vector<std::string>& shops, const std::function<bool(const std::string& item)>& wanted,
                                              const std::string& salt) const
{
    std::vector<std::pair<std::size_t, Offer>> lots;
    std::vector<std::string> made;
    for (const auto& shop : shops)
    {
        const auto till = tillOf(shop);
        const auto* shelves = account(till);
        if (!shelves)
            continue;
        // (Its wares as wares() keeps them, not a copy, as shopHasFood; onSale's rule, its parts worked out once a shop.)
        const auto& sold = waresKept(shop, made);
        const bool food = foodShop(shop);
        const auto* keeper = food ? spec(shop) : nullptr;
        const auto* business = keeper ? items::businessFor(keeper->workLabel) : nullptr;
        for (const auto& [item, n] : shelves->stock)
        {
            if (n < 3)                               // (Never a shelf's last few: buyForSurplus's (n - 1) / 2.)
                continue;
            const auto base = items::baseOf(item);
            if (wanted(base) && (std::find(sold.begin(), sold.end(), base) != sold.end() || (food && edible(item) && !madeFrom(business, base))))
                lots.push_back({std::hash<std::string>{}(salt + "|" + shop + "|" + item), {shop, till, item, base}});
        }
    }
    std::sort(lots.begin(), lots.end(), [](const auto& a, const auto& b) {
        return a.first < b.first || (a.first == b.first && std::tie(a.second.till, a.second.item) < std::tie(b.second.till, b.second.item));
    });
    std::vector<Offer> out;
    out.reserve(lots.size());
    for (auto& [lot, offer] : lots)
        out.push_back(std::move(offer));
    return out;
}

std::string Society::buyOne(const std::string& buyer, const std::vector<Offer>& offers, std::int64_t budget, const std::string& kind,
                            const std::string& salt, bool dearest, bool fullShelf)
{
    const auto* purse = account(buyer);
    budget = std::min<std::int64_t>(budget, purse ? purse->cash : 0);
    if (budget < 1 || offers.empty())
        return {};
    // From its own place in their order, the first WantSample still on the shelf past its last few that it can pay for,
    // at today's price; the cheapest of them (or the dearest).
    const Offer* pick = nullptr;
    std::int64_t best = 0;
    std::size_t seen = 0;
    const auto from = std::hash<std::string>{}(buyer + "|" + salt) % offers.size();
    for (std::size_t k = 0; k < offers.size() && seen < WantSample; ++k)
    {
        const auto& o = offers[(from + k) % offers.size()];
        const auto* shelves = o.shop == buyer || o.till == buyer ? nullptr : account(o.till);
        if (!shelves || stock(*shelves, o.item) < 3)
            continue;
        if (fullShelf)
            if (const auto rates = sellRate_.find(o.till); rates != sellRate_.end())
                if (const auto rate = rates->second.find(o.base); rate != rates->second.end() && stockAll(*shelves, o.base) < rate->second * keepDays(o.base))
                    continue;
        const auto price = shopPrice(o.shop, o.item);
        if (price > budget)
            continue;
        ++seen;
        if (!pick || (dearest ? price > best : price < best))
            pick = &o, best = price;
    }
    return pick && transfer(pick->till, buyer, pick->item, 1, best, kind) ? pick->item : std::string();
}

void Society::spendSurpluses(std::int64_t day)
{
    if (roster_ != Roster::Authored || day == surplusDay_)
        return;
    surplusDay_ = day;
    // Yesterday's odd jobs are over, but for businesses' hires still running: those who hold them come back to them, at
    // today's pay for a hand in its town (Society::dayWage, doc 64, Phase 6).
    oddJobs_.erase(std::remove_if(oddJobs_.begin(), oddJobs_.end(), [&](const OddJob& j) { return j.until < day; }), oddJobs_.end());
    ++oddVersion_;
    for (auto& j : oddJobs_)
    {
        // (A business's hire by its sales keeps the pay it was posted at, its own wage: doc 64, Phase 6.)
        if (j.until >= 0 && !j.producer.empty() && j.slots > 0 && !(workByHours() && j.payer.rfind("till:", 0) == 0))
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
        const auto poor = pennies(6 / 12., town);   // (Under the price of a meal: 6p at a 12p day.)
        for (const auto& [who, life] : state_.residents)
            if (const auto home = day_.communityOf ? day_.communityOf(life.homeCell) : std::string(); home == town)
                if (const auto* p = account(who); p && p->cash < poor && !hasFood(*p))
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
    // (The dole is gone: daily relief from what the church and the towns set aside weekly, doc 64, Phase 7: dailyRelief.)
    // (The orchestrator's channels are gone, doc 64, Phase 12: what the treasuries, the church and the Houses hold is
    // spent by their own rules, institutionsWeek. A rescue loan still owed is written off a month on.)
    auto& loans = state_.memory.loans;
    for (auto it = loans.begin(); it != loans.end();)
        if (it->second.first <= 0 || day - it->second.second >= MonthDays)
            it = loans.erase(it), ++state_.memory.revision;
        else
            ++it;
    // (Wages are the town's table now, doc 46, Phase 4: no more shares of a day's takings.)
    tendPrices(day);                                // (By today's takings: before they are forgotten.)
    takings_.clear();
    // Farmhands (RatwFarmhands.cpp): hires ended and lodgers home, new hires posted, bunkhouse larders stocked; and the
    // fields' year's rates (doc 64, Phase 13).
    endLodgings(day);
    postFarmHires(day);
    stockBunkhouses(day);
    tendFields();
    // Market dues (the user, 2026-10-05: tax where money gathers): each Restday every shop's till pays its town a
    // twentieth of what it holds above its working money.
    if (calendar::weekdayOf(double(day)) == calendar::Restday)
        keepUpPremises();                            // (Improved premises kept up, or let go: RatwOddJobs.cpp.)
    // (No market dues: tax is on income now, doc 64, Phase 7.)
}

std::int64_t Society::repayRescue(const std::string& till, const std::string& payer, std::int64_t most)
{
    // A resident's trouble solved (doc 57, 3): the loan its town's rescue fund made a business, paid off from a player's
    // purse to its town's treasury (the fund is gone with the orchestrator's channels, doc 64, Phase 12: a move, nothing
    // made or lost).
    auto loan = state_.memory.loans.find(till);
    const auto* purse = account(payer);
    if (loan == state_.memory.loans.end() || !purse || till.rfind("till:", 0) != 0)
        return 0;
    const auto* p = position(till.substr(5));
    if (!p)
        return 0;
    const auto town = p->role == "merchant" && shopTown_.count(till) ? shopTown_.at(till) : communityOfResident(p->founder);
    const auto paid = std::min({loan->second.first, purse->cash, std::max<std::int64_t>(0, most)});
    if (paid <= 0 || !shift(payer, treasuryOf(town), "", 0, paid, "a rescue repaid by a friend"))
        return 0;
    loan->second.first -= paid;
    if (loan->second.first <= 0)
        state_.memory.loans.erase(loan);
    ++state_.memory.revision;
    return paid;
}
} // namespace ratw
