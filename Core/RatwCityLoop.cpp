// The city's loop (Docs/Design/68-the-country-and-the-city-loop.md): residents buy their food, needs and comforts from the
// shops; the shops restock from the store room, paying the Country (less the gate toll, the treasury's) and the carters;
// the Country spends what it takes in on the city's goods; tills, the treasury and the Country pay their posts' wages.
// Every coin moves by shift or sale, so the money is conserved; goods come only from the store room and leave by being
// eaten, used up at home or carried away to the Country.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <map>

namespace ratw
{
namespace
{
std::uint64_t fnv(const std::string& text, std::uint64_t h = 1469598103934665603ULL)
{
    for (const unsigned char c : text)
        h = (h ^ c) * 1099511628211ULL;
    return h;
}
std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}
// A resident needs about this much nourishment a day (hunger grows .0035 a second; food counts 1.1 times when eaten).
constexpr double NourishADay = .0035 * 14400 / 1.1;
// What an account holds to eat, in nourishment.
double onHand(const EconomyAccount& a)
{
    double n = 0;
    for (const auto& [item, count] : a.stock)
        if (count > 0)
            n += double(Society::nourishment(item)) * count;
    return n;
}
// One of `weights` picked by `roll` (0..1), in proportion; -1 if they are all 0.
int weighted(const std::vector<double>& weights, double roll)
{
    double total = 0;
    for (const double w : weights)
        total += std::max(0., w);
    if (total <= 0)
        return -1;
    double at = roll * total;
    for (std::size_t i = 0; i < weights.size(); ++i)
    {
        if (weights[i] <= 0)
            continue;
        if (at < weights[i])
            return int(i);
        at -= weights[i];
    }
    for (std::size_t i = weights.size(); i-- > 0;)
        if (weights[i] > 0)
            return int(i);
    return -1;
}
double roll(const std::string& key)
{
    return double(fnv(key) % 1000003ULL) / 1000003.;
}
} // namespace

std::string Society::marketFor(const std::string& item) const
{
    (void)item;                                     // Every market supplies every good for now: the first takes it.
    return items::country().markets.front().id;
}

Society::Pay Society::payOf(const Position& p) const
{
    const auto& e = authored_.economy;
    if (p.role == "merchant")
        return {"keeper", "till:" + p.id, e.wageKeeper};
    if (p.role == "guard")
        return {"watch", "treasury", e.wagePublic};
    // A hand: a post whose work room holds exactly one shop.
    int shops = 0;
    const Position* shop = nullptr;
    for (const auto& q : positions_)
        if (q.role == "merchant" && q.work.cell == p.work.cell)
            ++shops, shop = &q;
    if (shops == 1)
        return {"hand", "till:" + shop->id, e.wageHand};
    const auto title = lower(p.title);
    for (const auto& rule : items::country().wages)
        for (const auto& word : rule.words)
            if (title.find(word) != std::string::npos)
                return rule.payer == "treasury" ? Pay{"public", "treasury", e.wagePublic}
                                                : Pay{"country", marketFund(rule.payer), e.wageCountry};
    return {};
}

int Society::crewOf(const std::string& till) const
{
    if (crewsRevision_ != rosterRevision_ || crews_.empty())
    {
        crews_.clear();
        for (const auto& p : positions_)
        {
            const auto held = state_.careers.positions.find(p.id);
            if (held == state_.careers.positions.end() || held->second.holder.empty())
                continue;
            const auto pay = payOf(p);
            if (pay.kind == "keeper" || pay.kind == "hand")
                ++crews_[pay.payer];
        }
        crews_[""] = 0;                             // (Built, even where there are no shops.)
        crewsRevision_ = rosterRevision_;
    }
    const auto found = crews_.find(till);
    return found == crews_.end() ? 1 : std::max(1, found->second);
}

std::string Society::carters() const
{
    std::string best;
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto* business = items::businessFor(p.title);
        const auto till = "till:" + p.id;
        if (business && business->id == "carters" && account(till) && (best.empty() || cash(till) < cash(best)))
            best = till;
    }
    return best.empty() ? std::string(CarriageFund) : best;
}

std::int64_t Society::carriageOn(const std::string& payer, const std::string& item, int quantity, bool commit, double share)
{
    const int rate = std::clamp(authored_.economy.carriage, 0, 1000);
    const auto* good = items::good(items::baseOf(item));
    if (rate == 0 || !good || quantity < 1 || share <= 0)
        return 0;
    const double owed = (carriageOwed_.count(payer) ? carriageOwed_[payer] : 0.) + good->weight * quantity * rate * share / CarriageLoad;
    const auto whole = std::int64_t(std::floor(owed + 1e-9));
    if (commit)
        carriageOwed_[payer] = owed - double(whole);
    return whole;
}

std::int64_t Society::gateToll(const std::string& till, std::int64_t cost, bool commit)
{
    // The gate toll on a draw's cost, its fraction of a penny kept against the till until it makes one.
    const double owed = (tollOwed_.count(till) ? tollOwed_[till] : 0.) + double(cost) * std::clamp(authored_.economy.gateToll, 0, 90) / 100.;
    const auto whole = std::min(cost, std::int64_t(std::floor(owed + 1e-9)));
    if (commit)
        tollOwed_[till] = std::min(1., owed - double(whole));
    return whole;
}

int Society::subsidyShare(const std::string& service) const
{
    const auto found = authored_.economy.subsidies.find(service);
    return found == authored_.economy.subsidies.end() ? 0 : std::clamp(found->second, 0, 100);
}

std::int64_t Society::subsidyRoom()
{
    if (treasuryOwed_ < 0 || subsidyLeft_ < 0)
    {
        treasuryOwed_ = 0;
        for (const auto& p : positions_)
            if (const auto held = state_.careers.positions.find(p.id);
                held != state_.careers.positions.end() && !held->second.holder.empty())
                if (const auto pay = payOf(p); pay.payer == "treasury")
                    treasuryOwed_ += pay.wage;
        if (subsidyLeft_ < 0)
            subsidyLeft_ = std::int64_t(std::floor(double(cash("treasury")) * SubsidyDay));
    }
    return std::max<std::int64_t>(0, std::min(subsidyLeft_, cash("treasury") - TreasuryFloat - treasuryOwed_));
}

std::pair<std::int64_t, std::int64_t> Society::carriageSplit(const std::string& payer, const std::string& item, int quantity, bool commit)
{
    // The treasury's share (doc 68, 5), while it has room today; else the payer pays it all.
    const double share = subsidyShare("carriage") / 100.;
    if (share > 0)
    {
        const auto city = carriageOn("subsidy:carriage", item, quantity, false, share);
        if (city <= subsidyRoom())
        {
            const auto own = carriageOn(payer, item, quantity, commit, 1 - share);
            if (commit)
            {
                carriageOn("subsidy:carriage", item, quantity, true, share);
                subsidyLeft_ -= city;
            }
            return {own, city};
        }
    }
    return {carriageOn(payer, item, quantity, commit), 0};
}

int Society::shelfOf(const std::string& till, const std::string& ware) const
{
    if (const auto t = shelf_.find(till); t != shelf_.end())
        if (const auto w = t->second.find(ware); w != t->second.end())
            return std::max(startingStock(ware), w->second);
    return startingStock(ware);
}

void Society::loopHourly(std::int64_t hour, int season)
{
    const int hod = int(hour % 24);
    const auto day = hour / 24;
    if (loopToday_.day < 0)
        loopToday_.day = day;
    if (hod == 0)
        turnShelves();
    loopToday_.day = day;
    if (hod >= ShopOpen && hod < ShopShut)
    {
        householdsShop(hod, day, season);
        if (authored_.economy.country)
            countryBuys(hod);
    }
    if (hod == PayHour)
        payWages();
}

void Society::turnShelves()
{
    // A ware's shelf is a quarter more than what sold of it yesterday (doc 68, 7).
    shelf_.clear();
    for (const auto& [till, sold] : soldToday_)
        for (const auto& [ware, n] : sold)
            if (const int shelf = std::min(StoreDrawMost, int(std::ceil(n * 1.25))); shelf > startingStock(ware))
                shelf_[till][ware] = shelf;
    soldToday_.clear();
    subsidyLeft_ = treasuryOwed_ = -1;              // (Today's subsidy: a twentieth of the treasury, worked out when first asked.)
    drawnYesterday_ = std::move(drawnToday_);
    drawnToday_.clear();
    crews_.clear();                                 // (Who holds the posts may have changed.)
    loopYesterday_ = loopToday_;
    loopToday_ = {};
}

namespace
{
// The shops open this hour (their keeper keeping shop), what they hold, and their communities.
struct OpenShop
{
    std::string till, community;
    const items::Business* business = nullptr;
};
} // namespace

void Society::householdsShop(int hod, std::int64_t day, int season)
{
    const auto& cfg = items::country();
    std::vector<OpenShop> open;
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto held = state_.careers.positions.find(p.id);
        if (held == state_.careers.positions.end() || held->second.holder.empty())
            continue;
        const auto* life = resident(held->second.holder);
        const auto till = "till:" + p.id;
        if (!life || life->task != "trade" || !account(till))
            continue;
        open.push_back({till, day_.communityOf ? day_.communityOf(p.work.cell) : std::string(), items::businessFor(p.title)});
    }
    if (open.empty())
        return;
    // A household: the residents sharing a home (in a communal home, each alone), with a child under 16 and a spouse who
    // live elsewhere (a child given a room of its own upstairs): whoever earns feeds the family.
    std::map<std::string, std::string> keyOf;          // Resident -> its home's key.
    for (const auto& [id, life] : state_.residents)
        if (spec(id) && !dead_.count(id) && !life.homeCell.empty())
            keyOf[id] = communalHome(life.homeCell) ? life.homeCell + "|" + id : life.homeCell;
    std::map<std::string, std::string> joined;         // Home key -> the key it shops with (union by kin).
    const std::function<std::string(const std::string&)> root = [&](const std::string& k) {
        const auto found = joined.find(k);
        return found == joined.end() || found->second == k ? k : (joined[k] = root(found->second));
    };
    const auto join = [&](const std::string& a, const std::string& b) {
        const auto ka = keyOf.find(a), kb = keyOf.find(b);
        if (ka == keyOf.end() || kb == keyOf.end())
            return;
        const auto ra = root(ka->second), rb = root(kb->second);
        if (ra != rb)
            joined[std::max(ra, rb)] = std::min(ra, rb);
    };
    for (const auto& [child, parents] : state_.careers.parents)
        if (const auto* c = spec(child); c && c->age < 16)
            for (const auto& parent : parents)
                join(child, parent);
    for (const auto& [a, b] : state_.careers.spouses)
        join(a, b);
    std::map<std::string, std::vector<std::string>> homes;
    for (const auto& [id, key] : keyOf)
        homes[root(key)].push_back(id);
    // The shops a household buys from: its community's open ones, or every open one where its community has none.
    std::map<std::string, std::vector<const OpenShop*>> byCommunity;
    for (const auto& s : open)
        byCommunity[s.community].push_back(&s);
    std::vector<const OpenShop*> all;
    for (const auto& s : open)
        all.push_back(&s);
    const auto sellersOf = [&](const std::vector<const OpenShop*>& shops, const std::string& item, std::vector<const std::string*>& out,
                               std::vector<double>& weights) {
        out.clear();
        weights.clear();
        for (const auto* s : shops)
            if (const int n = stock(*account(s->till), item); n > 0)
            {
                out.push_back(&s->till);
                weights.push_back(n);
            }
    };
    std::vector<const std::string*> tills;
    std::vector<double> weights;
    // Pays from whichever member has the most and can pay (keeping `keep` back across the household), or "" for none.
    const auto payerFor = [&](const std::vector<std::string>& members, std::int64_t price, std::int64_t keep) {
        std::int64_t held = 0;
        for (const auto& m : members)
            held += cash(m);
        const std::string* best = nullptr;
        for (const auto& m : members)
            if (cash(m) >= price && (!best || cash(m) > cash(*best)))
                best = &m;
        return best && held - price >= keep ? *best : std::string();
    };
    // Buys `want` of `item` from the shops (sellers by stock), paid by the household; returns how many came.
    const auto buy = [&](const std::vector<const OpenShop*>& shops, const std::vector<std::string>& members, const std::string& to,
                         const std::string& item, int want, std::int64_t keep, const std::string& key) {
        int got = 0;
        const auto price = catalogPrice(item);
        for (int lot = 0; lot < 4 && got < want; ++lot)
        {
            sellersOf(shops, item, tills, weights);
            const int pick = weighted(weights, roll(key + "|seller|" + std::to_string(lot)));
            if (pick < 0)
                break;
            const auto& till = *tills[std::size_t(pick)];
            int n = std::min({want - got, stock(*account(till), item), 99});
            std::string payer;
            while (n > 0 && (payer = payerFor(members, price * n, keep)).empty())
                n = payerFor(members, price, keep).empty() ? 0 : std::max(1, n / 2);
            if (n < 1 || !sale(till, payer, item, n, price, "market"))
                break;
            if (payer != to && !shift(payer, to, item, n, 0, "for the household"))
                consume(payer, item, n, "used at home");   // (Not reached: the member's purse has room.)
            got += n;
            loopToday_.residentsSpent += price * n;
        }
        return got;
    };
    const auto& households = items::households();
    for (const auto& [home, members] : homes)
    {
        const int marketHour = ShopOpen + int(fnv(home + "|market") % std::uint64_t(ShopShut - ShopOpen));
        if (hod < marketHour)
            continue;
        const bool first = hod == marketHour;
        const auto community = day_.communityOf ? day_.communityOf(state_.residents.at(members.front()).homeCell) : std::string();
        const auto here = byCommunity.find(community);
        const auto& shops = here != byCommunity.end() && !here->second.empty() ? here->second : all;
        // Food: every member topped up to foodDays; one under shortDays again each hour until the shops shut.
        bool shortHere = false;
        for (const auto& m : members)
        {
            const auto* wallet = account(m);
            if (!wallet)
                continue;
            double have = onHand(*wallet);
            if (!(first ? have < cfg.foodDays * NourishADay : have < cfg.shortDays * NourishADay))
                continue;
            for (int k = 0; k < 6 && have < cfg.foodDays * NourishADay; ++k)
            {
                // The food: among what these shops hold, the better nourishment for the price the likelier.
                std::vector<std::string> foods;
                std::vector<double> value;
                for (const auto* s : shops)
                    for (const auto& [item, n] : account(s->till)->stock)
                        if (n > 0 && edible(item) && std::find(foods.begin(), foods.end(), item) == foods.end())
                        {
                            foods.push_back(item);
                            value.push_back(double(nourishment(item)) / double(std::max<std::int64_t>(1, catalogPrice(item))));
                        }
                const auto key = m + "|" + std::to_string(day) + "|" + std::to_string(hod) + "|" + std::to_string(k);
                const int pick = weighted(value, roll(key));
                if (pick < 0)
                    break;
                const auto& food = foods[std::size_t(pick)];
                const int pieces = int(std::ceil((cfg.foodDays * NourishADay - have) / std::max(1, nourishment(food))));
                const int got = buy(shops, members, m, food, std::max(1, pieces), 0, key);
                if (got < 1)
                    break;
                loopToday_.foodBought += got;
                have += double(got) * nourishment(food);
            }
            shortHere = shortHere || have < cfg.shortDays * NourishADay;
        }
        if (shortHere)
            ++loopToday_.householdsShort;
        if (!first)
            continue;
        const std::int64_t keep = std::int64_t(households.reserve) * std::int64_t(members.size());
        // Needs, when due (a fixed hash of the home and the day: nothing saved), used up at home.
        int grown = 0;
        for (const auto& m : members)
            grown += spec(m) && spec(m)->age >= 16;
        for (std::size_t i = 0; i < households.needs.size(); ++i)
        {
            const auto& need = households.needs[i];
            const double days = season == 3 && need.winterDays > 0 ? need.winterDays : need.everyDays;
            int count = days < 1 ? int(std::lround(1 / days))
                                 : (std::uint64_t(day) + fnv(home + "|need|" + std::to_string(i))) % std::uint64_t(std::max(1L, std::lround(days))) == 0;
            if (need.perPerson)
                count *= grown;
            if (count < 1)
                continue;
            std::vector<std::string> options;
            for (const auto& g : need.any)
            {
                sellersOf(shops, g, tills, weights);
                if (!tills.empty())
                    options.push_back(g);
            }
            if (options.empty())
                continue;
            const auto& item = options[fnv(home + "|" + std::to_string(day) + "|" + std::to_string(i)) % options.size()];
            const auto payer = payerFor(members, catalogPrice(item) * count, keep);
            if (payer.empty())
                continue;
            if (const int got = buy(shops, members, payer, item, count, keep, home + "|need|" + std::to_string(day) + "|" + std::to_string(i)); got > 0)
            {
                consume(payer, item, got, "used at home");
                loopToday_.needsBought += got;
            }
        }
        // Comforts: a tenth a day of what is over `above` pennies a head, used up at once.
        std::int64_t held = 0;
        for (const auto& m : members)
            held += cash(m);
        auto budget = std::int64_t(std::floor((held - std::int64_t(cfg.comfortsAbove) * std::int64_t(members.size())) * cfg.comfortsShare));
        for (int k = 0; k < 20 && budget > 0 && !cfg.comforts.empty(); ++k)
        {
            std::vector<std::string> options;
            for (const auto& g : cfg.comforts)
                if (catalogPrice(g) <= budget)
                {
                    sellersOf(shops, g, tills, weights);
                    if (!tills.empty())
                        options.push_back(g);
                }
            if (options.empty())
                break;
            const auto& item = options[fnv(home + "|comfort|" + std::to_string(day) + "|" + std::to_string(k)) % options.size()];
            const auto payer = payerFor(members, catalogPrice(item), 0);
            if (payer.empty() || buy(shops, members, payer, item, 1, 0, home + "|comfort|" + std::to_string(day) + "|" + std::to_string(k)) < 1)
                break;
            consume(payer, item, 1, "used at home");
            budget -= catalogPrice(item);
            ++loopToday_.comfortsBought;
        }
    }
}

void Society::countryBuys(int hod)
{
    const auto& cfg = items::country();
    // The open shops whose crafts make a good (the Country buys the city's own make, from its makers).
    std::vector<std::pair<std::string, const items::Business*>> open;
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto held = state_.careers.positions.find(p.id);
        if (held == state_.careers.positions.end() || held->second.holder.empty())
            continue;
        const auto* life = resident(held->second.holder);
        const auto till = "till:" + p.id;
        if (life && life->task == "trade" && account(till))
            open.push_back({till, items::businessFor(p.title)});
    }
    const auto makes = [](const items::Business* b, const std::string& item) {
        if (!b)
            return false;
        for (const auto* k : items::craftsFor(b->id))
            for (const auto& [out, n] : k->out)
                if (out == item)
                    return true;
        return false;
    };
    for (const auto& market : cfg.markets)
    {
        const auto fund = marketFund(market.id);
        if (!account(fund) || market.basket.empty())
            continue;
        // What it owes its posts tonight stays in the purse with its reserve.
        std::int64_t owed = 0;
        for (const auto& p : positions_)
            if (const auto held = state_.careers.positions.find(p.id);
                held != state_.careers.positions.end() && !held->second.holder.empty())
                if (const auto pay = payOf(p); pay.payer == fund)
                    owed += pay.wage;
        const auto spare = cash(fund) - authored_.economy.countryReserve - owed;
        if (spare <= 0)
            continue;
        const double budget = double(spare) / double(ShopShut - hod);
        // The goods its makers have on their shelves now, and their shares.
        struct Want
        {
            const std::string* item;
            double share;
            std::vector<const std::string*> tills;
        };
        std::vector<Want> wants;
        double shares = 0;
        for (const auto& [item, share] : market.basket)
        {
            Want w{&item, double(share), {}};
            for (const auto& [till, business] : open)
                if (stock(*account(till), item) > 0 && makes(business, item))
                    w.tills.push_back(&till);
            if (!w.tills.empty())
            {
                shares += share;
                wants.push_back(std::move(w));
            }
        }
        double spent = 0;
        for (const auto& w : wants)
        {
            const auto price = catalogPrice(*w.item);
            // Whole pieces of its share; the odd part of a piece by a fixed roll, so small shares still buy.
            const double pieces = budget * w.share / shares / double(price);
            const auto key = fund + "|" + *w.item + "|" + std::to_string(loopToday_.day) + "|" + std::to_string(hod);
            int n = int(std::floor(pieces)) + (roll(key) < pieces - std::floor(pieces) ? 1 : 0);
            // A piece at a time to a seller picked by stock: spread over the makers, never the first in a list.
            std::map<const std::string*, int> lots;
            std::map<const std::string*, int> left;
            for (const auto* t : w.tills)
                left[t] = stock(*account(*t), *w.item);
            for (int i = 0; i < n; ++i)
            {
                std::vector<double> weights;
                for (const auto* t : w.tills)
                    weights.push_back(left[t]);
                const int pick = weighted(weights, roll(key + "|" + std::to_string(i)));
                if (pick < 0)
                    break;
                ++lots[w.tills[std::size_t(pick)]];
                --left[w.tills[std::size_t(pick)]];
            }
            for (const auto& [till, count] : lots)
            {
                int q = std::min(count, 99);
                const auto carriage = carriageSplit(fund, *w.item, q, false).first;
                if (cash(fund) - price * q - carriage < authored_.economy.countryReserve + owed)
                    q = int(std::max<std::int64_t>(0, (cash(fund) - authored_.economy.countryReserve - owed) / std::max<std::int64_t>(1, price + 1)));
                if (q < 1 || !sale(*till, fund, *w.item, q, price, "to the Country"))
                    continue;
                consume(fund, *w.item, q, "carried to the Country");
                // Carriage out: the Country pays it, less the treasury's share (doc 68, 5 and 6).
                if (const auto [own, city] = carriageSplit(fund, *w.item, q, true); own + city > 0)
                {
                    const auto carter = carters();
                    if (carter == CarriageFund && !account(CarriageFund))
                        openAccount(CarriageFund);
                    if (own > 0)
                        shift(fund, carter, "", 0, std::min(own, cash(fund)), "carriage out");
                    if (city > 0 && shift("treasury", carter, "", 0, city, "carriage subsidy"))
                        loopToday_.subsidy += city;
                    loopToday_.carriage += own + city;
                }
                spent += double(price * q);
                loopToday_.countrySpent += price * q;
            }
        }
        if (hod == ShopShut - 1)
            loopToday_.countryUnspent += std::max<std::int64_t>(0, cash(fund) - authored_.economy.countryReserve - owed);
    }
}

void Society::payWages()
{
    // Each payer's posts, held now, in post order.
    struct Due
    {
        const std::string* holder;
        int wage;
    };
    std::map<std::string, std::vector<Due>> byPayer;
    for (const auto& p : positions_)
    {
        const auto held = state_.careers.positions.find(p.id);
        if (held == state_.careers.positions.end() || held->second.holder.empty() || dead_.count(held->second.holder) ||
            !account(held->second.holder))
            continue;
        const auto pay = payOf(p);
        if (pay.kind.empty() || pay.wage <= 0 || !account(pay.payer))
            continue;
        byPayer[pay.payer].push_back({&held->second.holder, pay.wage});
    }
    const auto floorOf = [&](const std::string& payer) -> std::int64_t {
        if (payer == "treasury")
            return TreasuryFloat;
        if (payer.rfind("outside:", 0) == 0)
            return 0;                               // (Its reserve is kept from its buying so that it can pay its posts.)
        const auto drawn = drawnYesterday_.find(payer);
        return std::max(TillFloat, drawn == drawnYesterday_.end() ? 0 : drawn->second);
    };
    for (const auto& [payer, dues] : byPayer)
    {
        std::int64_t owed = 0;
        for (const auto& d : dues)
            owed += d.wage;
        const std::int64_t float_ = floorOf(payer);
        const std::int64_t can = std::max<std::int64_t>(0, cash(payer) - float_);
        std::int64_t paid = 0;
        for (const auto& d : dues)
        {
            // Short of the whole, each post the same share of its wage.
            const std::int64_t coins = can >= owed ? d.wage : std::int64_t(d.wage) * can / owed;
            if (coins > 0 && shift(payer, *d.holder, "", 0, coins, "wages"))
                paid += coins;
        }
        loopToday_.wagesOwed += owed;
        loopToday_.wagesPaid += paid;
        if (paid < owed && payer.rfind("till:", 0) == 0)
            ++loopToday_.tillsShort;
        // A till over its float and a week of its wages pays its keeper the rest, as profit.
        if (payer.rfind("till:", 0) == 0)
        {
            const auto held = state_.careers.positions.find(payer.substr(5));
            const auto cap = float_ + 7 * owed;
            if (const auto excess = cash(payer) - cap; held != state_.careers.positions.end() && !held->second.holder.empty() &&
                                                       excess > 0 && shift(payer, held->second.holder, "", 0, excess, "profit"))
                loopToday_.profit += excess;
        }
    }
}

// The restocking rule (doc 67, 6; revised by doc 68, 7): shelves follow sales, and making takes the crew's hours.
std::vector<std::string> Society::restockFromStore(const std::string& keeper, double absoluteDay)
{
    std::vector<std::string> done;
    const auto hour = std::int64_t(std::floor(absoluteDay * 24));
    if (const auto [last, fresh] = restockedAt_.emplace(keeper, hour); !fresh)
    {
        if (last->second == hour)
            return done;                            // At most once a game hour.
        last->second = hour;
    }
    if (!storeOpen(keeper))
        return done;
    const auto* job = jobOf(keeper);
    const auto* business = items::businessFor(job->title);
    const auto till = tillOf(keeper);
    const auto sold = spec(job->founder) ? wares(job->founder) : std::vector<std::string>{};
    // The crew's hours this hour, with what was left from the last (at most two hours' worth).
    const int crew = crewOf(till);
    auto& bank = workBank_[till];
    bank = std::min(2. * crew, bank + crew);
    struct Want
    {
        std::string ware;
        int lack;
        double ratio;
        const items::Craft* craft;
    };
    std::vector<Want> wants;
    for (const auto& ware : sold)
    {
        const int shelf = shelfOf(till, ware);
        const int have = stock(*account(till), ware);
        if (have >= shelf)
            continue;
        const items::Craft* craft = nullptr;
        if (business)
            for (const auto* k : items::craftsFor(business->id))
                if (!craft && std::any_of(k->out.begin(), k->out.end(), [&](const auto& o) { return o.first == ware; }))
                    craft = k;
        if (!craft && storeUse(keeper, ware) != StoreUse::Resell)
            continue;
        wants.push_back({ware, shelf - have, double(have) / shelf, craft});
    }
    // The ware furthest below its shelf first.
    std::stable_sort(wants.begin(), wants.end(), [](const Want& a, const Want& b) { return a.ratio < b.ratio; });
    for (const auto& w : wants)
    {
        if (!w.craft)
        {
            if (drawFromStore(keeper, w.ware, std::min(StoreDrawMost, w.lack), "restocked from the store room").ok)
                done.push_back("restocked " + w.ware);
            continue;
        }
        const auto& k = *w.craft;
        int per = 1;
        for (const auto& [item, n] : k.out)
            if (item == w.ware)
                per = n;
        int batches = std::min({int(std::ceil(double(w.lack) / per)), int(std::floor(bank / k.hours + 1e-9)), 100});
        if (batches < 1)
            continue;
        // Room for what the batches make, and their inputs drawn for what the shelves lack.
        const auto& shelves = *account(till);
        std::size_t kinds = shelves.stock.size();
        bool can = true;
        for (const auto& [item, n] : k.out)
        {
            can = can && itemValid(item) && stock(shelves, item) <= StockCap - n * batches;
            kinds += shelves.stock.count(item) ? 0 : 1;
        }
        for (const auto& [item, n] : k.in)
        {
            kinds += shelves.stock.count(item) ? 0 : 1;
            can = can && (stock(shelves, item) >= n * batches || storeUse(keeper, item) == StoreUse::Make);
        }
        if (!can || kinds > MaxGoodsKinds)
            continue;
        for (const auto& [item, n] : k.in)
            if (const int lack = n * batches - stock(*account(till), item); lack > 0)
                drawFromStore(keeper, item, std::min(StoreDrawMost, lack), "drawn for " + k.id);
        // As many batches as the inputs on the shelves now allow (a refused draw leaves the rest for the next hour).
        for (const auto& [item, n] : k.in)
            batches = std::min(batches, stock(*account(till), item) / n);
        if (batches < 1)
            continue;
        for (const auto& [item, n] : k.in)
            consume(till, item, n * batches, "made into " + k.id);
        for (const auto& [item, n] : k.out)
            create(till, item, n * batches, "made from the store room");
        bank -= batches * k.hours;
        done.push_back("made " + std::to_string(batches) + " " + k.id);
    }
    return done;
}
} // namespace ratw
