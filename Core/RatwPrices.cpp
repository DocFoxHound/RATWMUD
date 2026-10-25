// What things cost, and prices (Docs/Design/64-economy-migration.md, Phase 4; doc 63's model): each seller prices its
// goods at their normal cost times one and its own margin, and the margin follows its shelf. See RatwSociety.h.
#include "RatwSociety.h"

#include "RatwItems.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace ratw
{
namespace
{
// A lot from 0 to 1, fixed by its words (a seller's week's review repeats exactly).
double lot(const std::string& words)
{
    return double(std::hash<std::string>{}(words) % 100000) / 100000;
}
}

bool Society::costed(const std::string& account)
{
    for (const char* prefix : {"till:", "house:", "caravan:", "fund:", "stores:"})
        if (account.rfind(prefix, 0) == 0)
            return true;
    return false;
}

std::string Society::accountTown(const std::string& account) const
{
    if (const auto found = shopTown_.find(account); found != shopTown_.end())
        return found->second;
    return communityOfResident(account);
}

double Society::costOf(const std::string& account, const std::string& item) const
{
    if (const auto a = cost_.find(account); a != cost_.end())
        if (const auto c = a->second.find(item); c != a->second.end())
            return c->second;
    // None on record: its worth at the town's wage, less the margin a price carries.
    return items::worthOf(item) * labourDay(accountTown(account)) / (1 + items::ladder().markup);
}

double Society::inputCost(const std::string& account, const std::string& item) const
{
    if (const auto a = cost_.find(account); a != cost_.end())
        if (const auto c = a->second.find(item); c != a->second.end())
            return c->second;
    return townPrice(accountTown(account), item);     // (What it would pay for it: the going price.)
}

void Society::addCost(const std::string& account, const std::string& item, int had, int n, double each)
{
    if (!costed(account) || n < 1 || !std::isfinite(each) || each < 0)
        return;
    auto& goods = cost_[account];
    const auto found = goods.find(item);
    had = std::max(0, had);
    const double before = found != goods.end() ? found->second : each;
    goods[item] = (before * had + each * n) / (had + n);
}

double Society::carriage(const std::string& item, double legs, const std::string& town) const
{
    const auto* good = items::good(item);
    return good ? items::ladder().carryPerWeight * good->weight * legs * labourDay(town) / PaidSpells : 0;
}

double Society::normalCost(const std::string& seller, const std::string& item) const
{
    const auto till = tillOf(seller);
    const auto base = items::baseOf(item);
    // A quality's share of the plain good's (its worth's, as its price is).
    const double plainWorth = items::worthOf(base);
    const double quality = plainWorth > 0 ? items::worthOf(item) / plainWorth : 1;
    auto& cached = unitCost_[seller];               // (By the seller: a house's till serves several businesses.)
    if (const auto found = cached.find(base); found != cached.end())
        return found->second < 0 ? costOf(till, item) : found->second * quality;
    const auto* r = spec(seller);
    const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr;
    const std::string label = r ? r->workLabel : p ? p->title : std::string();
    const auto town = accountTown(till).empty() ? communityOfResident(seller) : accountTown(till);
    const double hour = labourDay(town) / PaidSpells;
    // A batch's goods share its cost by their worth (a sheep farm's mutton, wool and skin).
    const auto share = [&](const std::vector<std::pair<std::string, int>>& out) {
        double all = 0, mine = 0;
        int n = 0;
        for (const auto& [o, count] : out)
        {
            all += count * items::worthOf(o);
            if (o == base)
                mine = count * items::worthOf(o), n = count;
        }
        return n > 0 && all > 0 ? mine / all / n : -1.;
    };
    double unit = -1;
    if (const auto* producer = label.empty() ? nullptr : items::producerFor(label))
    {
        double each = share(producer->out);
        if (each < 0)
            each = share(producer->offSeason);
        if (each >= 0)
        {
            // Its tools' wear (crafts.json `tools`): a tool lasts so many days of spells, worn by the hours of work (as
            // they are bought: tradeUpkeep; doc 64, Phase 3b).
            double wear = 0;
            for (const auto& t : items::toolNeeds())
                if (t.producer == producer->id && t.everyDays > 0)
                    wear += producer->hours / PaidSpells / t.everyDays * inputCost(till, t.item);
            unit = (producer->hours * hour + wear) * each;
            // On a House's land, its rent is a cost like any other: a share of the price (doc 64, Phase 5).
            if (p && !landlordOf(p->id).empty())
                unit /= 1 - RentShare;
        }
    }
    if (unit < 0 && !label.empty())
        if (const auto* business = items::businessFor(label))
            for (const auto* k : items::craftsFor(business->id))
                if (const double each = share(k->out); each >= 0)
                {
                    double materials = 0;
                    for (const auto& [in, count] : k->in)
                        materials += count * (inputCost(till, in) + carriage(in, 1, town));
                    unit = (k->hours * (1 + k->skill / items::ladder().skillWage) * hour + materials) * each;
                    break;
                }
    cached[base] = unit;                             // (-1: it doesn't make it, and sells it at what it paid.)
    return unit < 0 ? costOf(till, item) : unit * quality;
}

double Society::buyingPrice(const std::string& town, const std::string& item) const
{
    return townPrice(town, item) / (1 + items::ladder().markup);
}

double Society::materialsCost(const std::string& seller, const std::string& item) const
{
    const auto till = tillOf(seller);
    const auto base = items::baseOf(item);
    const auto* r = spec(seller);
    const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr;
    const std::string label = r ? r->workLabel : p ? p->title : std::string();
    const auto town = accountTown(till).empty() ? communityOfResident(seller) : accountTown(till);
    if (const auto* producer = label.empty() ? nullptr : items::producerFor(label))
    {
        // A producer's: its tools' wear, a yield's share.
        int n = 0;
        for (const auto& [o, count] : producer->out)
            n += count;
        double wear = 0;
        for (const auto& t : items::toolNeeds())
            if (t.producer == producer->id && t.everyDays > 0)
                wear += producer->seconds / (PaidSpells * 3600.) / t.everyDays * inputCost(till, t.item);
        return n > 0 ? wear / n : 0;
    }
    if (const auto* business = label.empty() ? nullptr : items::businessFor(label))
        for (const auto* k : items::craftsFor(business->id))
        {
            // A batch's materials shared among its goods by their worth, as its cost is (normalCost): a carcass's among
            // its cuts, not the whole carcass against each.
            double all = 0, mine = 0;
            int n = 0;
            for (const auto& [o, count] : k->out)
            {
                all += count * items::worthOf(o);
                if (o == base)
                    mine = count * items::worthOf(o), n = count;
            }
            if (n <= 0)
                continue;
            double materials = 0;
            for (const auto& [in, q] : k->in)
                materials += q * (inputCost(till, in) + carriage(in, 1, town));
            return all > 0 ? materials * mine / all / n : materials / n;
        }
    return costOf(till, item);                       // (Bought to sell on: the goods themselves.)
}

double Society::marginOf(const std::string& seller, const std::string& item) const
{
    if (const auto a = margins_.find(tillOf(seller)); a != margins_.end())
        if (const auto m = a->second.find(items::baseOf(item)); m != a->second.end())
            return m->second;
    return items::ladder().markup;
}

double Society::priceAt(const std::string& seller, const std::string& item) const
{
    if (!items::good(item))
        return 1;
    if (storePriced(items::baseOf(item)))
        return std::max(1, items::good(item)->price);   // (Meals, herbs, swords: as before, for now.)
    return normalCost(seller, item) * (1 + marginOf(seller, item));
}

double Society::keepDays(const std::string& item)
{
    const auto* good = items::good(item);
    if (!good)
        return 8;
    if (good->keeps > 0)
        return std::clamp(good->keeps / 2, 1., 6.);
    const double worth = items::worthOf(item);
    return good->staple ? 6 : worth < 2 ? 10 : worth < 6 ? 12 : 20;
}

void Society::reviewMargins(std::int64_t day)
{
    // Each seller, once a week on its own day: a shelf emptying faster than it keeps (under three tenths of its days)
    // raises the margin a little, most weeks; one that won't move (over 1.7 times them, and more on it than a maker
    // keeps) lowers it; between, competition wears it back toward the ladder's. Staples may rise to double their cost.
    const double usual = items::ladder().markup;
    for (const auto& [till, rates] : sellRate_)
    {
        if (!costed(till) || std::int64_t(std::hash<std::string>{}(till) % 7) != day % 7)
            continue;
        const auto* shelves = account(till);
        if (!shelves)
            continue;
        for (const auto& [good, rate] : rates)
        {
            const auto* g = items::good(good);
            if (!g || storePriced(good))
                continue;
            const auto words = till + "|" + good + "|" + std::to_string(day);
            if (lot(words + "|review") >= .75)
                continue;
            const double held = stockAll(*shelves, good);
            const double keep = keepDays(good);
            const double days = rate > .01 ? held / rate : held > 0 ? 999 : 0;
            auto& m = margins_[till].try_emplace(good, usual).first->second;
            const double step = lot(words + "|step");
            if (rate > .05 && days < keep * .3)
                m += step * MarginStep;
            else if (days > keep * 1.7 && held > GoodsKept)
                m -= step * MarginStep;
            else
                m += (usual - m) * step * .25;
            m = std::clamp(m, MarginLeast, g->staple ? StapleMarginMost : MarginMost);
        }
    }
}

void Society::keepPrices()
{
    auto& memory = state_.memory;
    memory.costs.clear();
    memory.margins.clear();
    for (const auto& [account, goods] : cost_)
        if (const auto* a = this->account(account))
            for (const auto& [item, c] : goods)
                if (a->stock.count(item))              // (What it no longer holds is priced afresh when it comes back.)
                    memory.costs[account + "|" + item] = c;
    for (const auto& [account, goods] : margins_)
        for (const auto& [good, m] : goods)
            if (std::abs(m - items::ladder().markup) > 1e-6)
                memory.margins[account + "|" + good] = m;
}

void Society::loadPrices()
{
    cost_.clear();
    margins_.clear();
    unitCost_.clear();
    for (const auto& [key, c] : state_.memory.costs)
        if (const auto bar = key.rfind('|'); bar != std::string::npos)
            cost_[key.substr(0, bar)][key.substr(bar + 1)] = c;
    for (const auto& [key, m] : state_.memory.margins)
        if (const auto bar = key.rfind('|'); bar != std::string::npos)
            margins_[key.substr(0, bar)][key.substr(bar + 1)] = m;
}

} // namespace ratw
