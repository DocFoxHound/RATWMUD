// What things cost, and prices (Docs/Design/64-economy-migration.md, Phase 4; doc 63's model): each seller prices its
// goods at their normal cost times one and its own margin, and the margin follows its shelf. See RatwSociety.h.
#include "RatwSociety.h"

#include "RatwItems.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>

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

double Society::amountDay(const std::string& town) const
{
    if (workByHours_)
        return goingRate(town);
    return state_.memory.foundedDay > 0 ? state_.memory.foundedDay : items::ladder().catalogueDay;
}

double Society::atGoingRate(double catalogue, const std::string& town) const
{
    return catalogue * amountDay(town) / items::ladder().catalogueDay;
}

double Society::carriage(const std::string& item, double legs, const std::string& town) const
{
    const auto* good = items::good(item);
    return good ? items::ladder().carryPerWeight * good->weight * legs * labourDay(town) / PaidSpells : 0;
}

double Society::normalCost(const std::string& seller, const std::string& item, bool* resold) const
{
    const auto till = tillOf(seller);
    const auto base = items::baseOf(item);
    // A quality's share of the plain good's (its worth's, as its price is).
    const double plainWorth = items::worthOf(base);
    const double quality = plainWorth > 0 ? items::worthOf(item) / plainWorth : 1;
    auto& cached = unitCost_[seller];               // (By the seller: a house's till serves several businesses.)
    if (const auto found = cached.find(base); found != cached.end())
    {
        if (resold)
            *resold = found->second < 0;            // (Round 14, lane 1: told without a second look-up, ChainPricedOnce.)
        return found->second < 0 ? costOf(till, item) : found->second * quality;
    }
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
            // they are bought: tradeUpkeep; doc 64, Phase 3b). The hours as it works them (costedHours, Phase 13).
            const double hours = costedHours(*producer);
            double wear = 0;
            for (const auto& t : items::toolNeeds())
                if (t.producer == producer->id && t.everyDays > 0)
                    wear += hours / PaidSpells / t.everyDays * inputCost(till, t.item);
            unit = (hours * hour + wear) * each;
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
                    unit = (costedHours(*k) * (1 + k->skill / items::ladder().skillWage) * hour + materials) * each;
                    break;
                }
    cached[base] = unit;                             // (-1: it doesn't make it, and sells it at what it paid.)
    if (resold)
        *resold = unit < 0;
    return unit < 0 ? costOf(till, item) : unit * quality;
}

bool Society::resells(const std::string& till, const std::string& good) const
{
    // normalCost's paths by a trade's label: its yield (in season or out), or a craft of its business, makes the good.
    const auto base = items::baseOf(good);
    const auto makes = [&](const std::string& label) {
        const auto has = [&](const std::vector<std::pair<std::string, int>>& out) {
            double all = 0;
            int n = 0;
            for (const auto& [o, count] : out)
            {
                all += count * items::worthOf(o);
                if (o == base)
                    n = count;
            }
            return n > 0 && all > 0;                 // (As normalCost's share: no worth, no cost of its own.)
        };
        if (const auto* producer = label.empty() ? nullptr : items::producerFor(label))
            if (has(producer->out) || has(producer->offSeason))
                return true;
        if (const auto* business = label.empty() ? nullptr : items::businessFor(label))
            for (const auto* k : items::craftsFor(business->id))
                if (has(k->out))
                    return true;
        return false;
    };
    if (till.rfind("till:", 0) == 0)
    {
        const auto* p = position(till.substr(5));
        return !p || !makes(p->title);
    }
    // (A House's till serves several businesses: what any of them makes is the House's own.)
    if (till.rfind("house:", 0) == 0)
    {
        for (const auto& [pid, house] : state_.houses.owner)
            if (house == till)
                if (const auto* p = position(pid); p && makes(p->title))
                    return false;
        return true;
    }
    return true;
}

double Society::missedRate(const std::string& till, const std::string& good) const
{
    if (const auto a = missedRate_.find(till); a != missedRate_.end())
        if (const auto m = a->second.find(items::baseOf(good)); m != a->second.end())
            return m->second;
    return 0;
}

double Society::buyingPrice(const std::string& town, const std::string& item) const
{
    return townPrice(town, item) / (1 + items::ladder().markup);
}

double Society::materialsCost(const std::string& seller, const std::string& item, bool* resold) const
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
    if (resold)
        *resold = true;
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
    const auto base = items::baseOf(item);
    if (storePriced(base))
        return atGoingRate(std::max(1, items::good(item)->price), shopTown(seller));   // (Meals, herbs, swords: as before.)
    // (Its till and good found once for its margin and its markdown: a price is asked for very often, and a second
    // look-up of the till cost the run two fifths of its pace.)
    const auto till = tillOf(seller);
    double margin = items::ladder().markup;
    if (const auto a = margins_.find(till); a != margins_.end())
        if (const auto m = a->second.find(base); m != a->second.end())
            margin = m->second;
    // A good it only sells on (it makes none of it: normalCost's -1, at what it paid) carries the ladder's handling at
    // most (round 14, lane 1, ChainPricedOnce; the user, 2026-10-30): a shortage is priced once, where the good is made
    // (MarginsByShortage), not again by every shop it passes through. Ridgemere's resellers sold fish at 1.89 times the
    // boats' price, and its smoked fish reached 41-44p. (A House's till asked by itself has no trade of its own to tell:
    // its businesses' crafts do, resells. Its markdowns below as before, so a reseller's glut still sells off.)
    bool resold = false;
    const double cost = normalCost(seller, item, workByHours() && wageRule(ChainPricedOnce) ? &resold : nullptr);
    if (resold && seller.rfind("house:", 0) == 0)
        resold = resells(seller, base);
    if (resold)
        margin = std::min(margin, items::ladder().markup);
    const double price = cost * (1 + margin);
    // Marked down from its second day over its shelf target, a step a day, to its floor (reviewMarkdowns; round 11,
    // lane 3). (Not raised to the floor: a price the review has already put under it stays where it is.)
    if (!daysOver_.empty())
        if (const auto a = daysOver_.find(till); a != daysOver_.end())
            if (const auto d = a->second.find(base); d != a->second.end() && d->second > 1)
                return std::max(std::min(price, cost * MarkdownFloor), price * (1 - MarkdownStep * (d->second - 1)));
    return price;
}

int Society::daysOver(const std::string& seller, const std::string& item) const
{
    if (const auto a = daysOver_.find(tillOf(seller)); a != daysOver_.end())
        if (const auto d = a->second.find(items::baseOf(item)); d != a->second.end())
            return d->second;
    return 0;
}

void Society::reviewMarkdowns()
{
    // Each seller's goods, by its own shelf (the user, 2026-10-29; see RatwSociety.h): a day over its target, or a day
    // back toward its price. A shop's and a House's (not the towns' stores, a caravan's load or a fund's).
    if (!workByHours() || !wageRule(Markdowns))
    {
        daysOver_.clear();
        return;
    }
    for (const auto& [till, rates] : sellRate_)
    {
        (void)rates;
        const auto* shelves = till.rfind("till:", 0) == 0 || till.rfind("house:", 0) == 0 ? account(till) : nullptr;
        if (!shelves)
            continue;
        std::set<std::string> goods;
        for (const auto& [item, n] : shelves->stock)
            if (n > 0)
                goods.insert(items::baseOf(item));
        auto found = daysOver_.find(till);
        if (found != daysOver_.end())
            for (const auto& [good, d] : found->second)
                goods.insert(good);
        for (const auto& good : goods)
        {
            if (!items::good(good) || storePriced(good))
                continue;
            const bool over = stockAll(*shelves, good) > shelfTarget(till, good, ProducerKept);
            if (found == daysOver_.end())
            {
                if (!over)
                    continue;
                found = daysOver_.try_emplace(till).first;
            }
            auto& days = found->second[good];
            days = over ? std::min(MarkdownDaysMost, days + 1) : days - 1;
            if (days <= 0)
                found->second.erase(good);
        }
        if (found != daysOver_.end() && found->second.empty())
            daysOver_.erase(found);
    }
    // (A seller gone, or no longer selling, its days with it.)
    for (auto it = daysOver_.begin(); it != daysOver_.end();)
        it = account(it->first) && sellRate_.count(it->first) ? std::next(it) : daysOver_.erase(it);
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
    //
    // With MarginsByShortage (round 14, lane 1, with work by the hours; the user, 2026-10-30), the margin aims at what
    // its seller's shortage earns now: the share of its buyers it turned away (missedRate_ over its rate, missed sales
    // counting) from UnmetFrom to UnmetFull takes the aim from the ladder's to its most, and the review pulls it
    // MarginPull of the way (times its step lot) there. A glut, more on it than 1.7 times its keeping days or over its
    // own shelf target (Markdowns' own test; not a seasonal field's store, which holds the year by design), comes down
    // a step as before. This replaces the raise for a shelf bare at the turn, which a seller serving every buyer and
    // selling its last at dusk earned every week: North Wharf's smokehouse sold 143 of a rate of 144 and sat at 1.6. And
    // the fall could never reach a producer or supplier selling more than about 16 a day (stocked to its shelf target,
    // about 1.8 times its rate, it never held 2.55 times it): the margin climbed to its cap in about 57 weeks and stayed.
    // With ChainPricedOnce, a good it only sells on is kept to the ladder's at most, as priceAt prices it (nothing builds
    // up hidden behind the price).
    const double usual = items::ladder().markup;
    const bool byShortage = workByHours() && wageRule(MarginsByShortage);
    const bool once = workByHours() && wageRule(ChainPricedOnce);
    for (const auto& [till, rates] : sellRate_)
    {
        if (!costed(till) || std::int64_t(std::hash<std::string>{}(till) % 7) != day % 7)
            continue;
        const auto* shelves = account(till);
        if (!shelves)
            continue;
        const bool field = byShortage && state_.memory.fields.count(till);
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
            const double most = g->staple ? StapleMarginMost : MarginMost;
            if (byShortage)
            {
                const double unmet = rate > .01 ? std::min(1., missedRate(till, good) / rate) : 0.;
                if ((days > keep * 1.7 && held > GoodsKept) || (!field && held > shelfTarget(till, good, ProducerKept)))
                    m -= step * MarginStep;
                else
                {
                    const double scarce = rate > .05 ? std::clamp((unmet - UnmetFrom) / (UnmetFull - UnmetFrom), 0., 1.) : 0.;
                    m += (usual + (most - usual) * scarce - m) * step * MarginPull;
                }
            }
            else if (rate > .05 && days < keep * .3)
                m += step * MarginStep;
            else if (days > keep * 1.7 && held > GoodsKept)
                m -= step * MarginStep;
            else
                m += (usual - m) * step * .25;
            if (once && m > usual && resells(till, good))
                m = usual;
            m = std::clamp(m, MarginLeast, most);
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
    memory.markdowns.clear();
    for (const auto& [till, goods] : daysOver_)
        for (const auto& [good, d] : goods)
            memory.markdowns[till + "|" + good] = d;
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
    daysOver_.clear();
    for (const auto& [key, d] : state_.memory.markdowns)
        if (const auto bar = key.rfind('|'); bar != std::string::npos && d > 0)
            daysOver_[key.substr(0, bar)][key.substr(bar + 1)] = d;
}

} // namespace ratw
