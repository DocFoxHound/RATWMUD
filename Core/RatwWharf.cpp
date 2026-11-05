// The Wharf (Docs/Design/73-the-wharf.md; the user, 2026-11-04: "The Country should turn into the Wharf, which is going
// to turn into where, for the game, we can inject and take money, excess items, and import shortages"): the realm's
// valve to the world beyond it. It lands what the realm's farms can't fill of a good on its imports list (the last lot of
// a store draw) and every good no area grows, as the Country did; it takes what piles on the farms and works over
// wharfKeepDays of their yield; it buys the city's exports with what credit is left. All at the stable price (the store
// room's at markup 100 and the season's factor). What the realm pays for imports leaves it (sunk, "paid beyond the
// realm"), what the beyond pays comes in (minted, "from beyond the realm"), so money stays conserved (money == minted -
// sunk); the beyond pays in only as far as the realm has paid it (the trade credit), which the DM may move. The Wharf
// keeps its fee (wharfFee percent: of its imports' takings, and over its exports', paid by the beyond).
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
std::int64_t Society::stablePrice(const std::string& item, int quantity) const
{
    if (quantity < 1)
        return 0;
    const std::int64_t base = catalogPrice(item) * quantity;
    // An area's price a year (the granary's), else the season's (its group's), else the catalogue's: never a shop's own.
    const double year = farmPriceFactor(item);
    const double f = year > 0 ? year : priceFactor(item);
    if (f == 1)
        return base;
    return std::max<std::int64_t>(1, std::int64_t(std::ceil(double(base) * f - 1e-9)));
}

bool Society::wharfImports(const std::string& item) const
{
    const auto base = items::baseOf(item);
    const auto& lists = items::cityLoop().wharf;
    const auto found = lists.imports.find(base);
    return wharfValveOn() && found != lists.imports.end() && found->second.on && !lists.refuses(base) &&
           !authored_.economy.wharfOff.count(base) && rule("wharfImportScale") > 0;
}

int Society::wharfRoom(const std::string& item) const
{
    if (!wharfImports(item))
        return 0;
    const auto base = items::baseOf(item);
    const auto most = std::int64_t(items::cityLoop().wharf.imports.at(base).most) * rule("wharfImportScale") / 100;
    const auto landed = loopToday_.wharfImported.find(base);
    return int(std::max<std::int64_t>(0, most - (landed == loopToday_.wharfImported.end() ? 0 : landed->second.pieces)));
}

bool Society::wharfTakes(const std::string& item) const
{
    const auto base = items::baseOf(item);
    const auto& lists = items::cityLoop().wharf;
    const auto found = lists.takes.find(base);
    return wharfValveOn() && found != lists.takes.end() && found->second.on && !lists.refuses(base) && rule("wharfTakeScale") > 0;
}

int Society::wharfKeep(std::size_t farm, const std::string& item) const
{
    // Its yield a day today (the season's, blended, times its yield scale), and what its makes make of it a day.
    const auto& f = farms_[farm];
    double aDay = 0;
    if (const auto y = f.def->yield.find(items::baseOf(item)); y != f.def->yield.end())
        aDay += seasonalValue(y->second, yearDay_) * f.yieldScale;
    for (const auto& m : f.def->makes)
        if (m.good == items::baseOf(item) && m.dedicated())
            aDay += seasonalValue(m.aDay, yearDay_) * f.yieldScale / std::max(1, m.in);
    return std::max(rule("wharfKeepLeast"), int(std::ceil(aDay * rule("wharfKeepDays") - 1e-9)));
}

std::int64_t Society::wharfExportRoom() const
{
    const auto& book = state_.loop.wharf;
    return std::max<std::int64_t>(0, std::min<std::int64_t>(book.credit, rule("wharfExportMost") - loopToday_.wharfIn));
}

void Society::wharfMint(std::int64_t coins, const std::string& why)
{
    if (coins <= 0)
        return;
    state_.accounts.at(WharfFund).cash += coins;
    state_.minted += coins;
    state_.loop.wharf.in += coins;
    loopToday_.wharfIn += coins;
    record(why, "beyond", WharfFund, "", 0, coins);
}

void Society::wharfSink(std::int64_t coins, const std::string& why)
{
    coins = std::min(coins, cash(WharfFund));
    if (coins <= 0)
        return;
    state_.accounts.at(WharfFund).cash -= coins;
    state_.sunk += coins;
    state_.loop.wharf.out += coins;
    loopToday_.wharfOut += coins;
    record(why, WharfFund, "beyond", "", 0, coins);
}

void Society::bookWharfFee(std::int64_t coins)
{
    // (The Wharf's fees stay in its purse, House Grayrock's estate's while the estates are split: booked as its income.)
    if (const auto* e = coins > 0 && estatesOn() ? estateOfFund(WharfFund) : nullptr)
        loopToday_.estates[e->id].fees += coins;
}

bool Society::wharfPaysBeyond(const std::string& item) const
{
    if (farmAreaOf(item) >= 0)
        return wharfImports(item);
    return !areaOfGood_.count(items::baseOf(item));
}

void Society::bookWharfLanding(const std::string& item, int n, std::int64_t coins)
{
    const auto base = items::baseOf(item);
    auto& today = loopToday_.wharfImported[base];
    today.pieces += n;
    today.pennies += coins;
    if (!wharfValveOn() || coins <= 0 || !account(WharfFund) || !wharfPaysBeyond(item))
        return;                                     // (The valve off, or an area's good before its farms: kept, as the Country did.)
    // The beyond's share, never past what the credit has room for (wharfCreditMost: the most the realm may have paid
    // beyond and not had back), so the valve nets to about nothing over time; what is over is kept in the Wharf's purse
    // (wharf7, doc 73: every landing paid beyond while the farms came on, the credit at its cap from day 5 and ~1,070p a
    // day lost to the realm).
    auto& credit = state_.loop.wharf.credit;
    const auto fee = coins * std::clamp(rule("wharfFee"), 0, 100) / 100;
    const auto room = std::max<std::int64_t>(0, rule("wharfCreditMost") - credit);
    const auto beyond = std::min({coins - fee, cash(WharfFund), room});
    wharfSink(beyond, "paid beyond the realm: " + base);
    state_.loop.wharf.fee += fee;
    loopToday_.wharfFee += fee;
    bookWharfFee(coins - std::max<std::int64_t>(0, beyond));   // (What stays: its fee, and what the credit had no room for.)
    if (beyond > 0)
    {
        credit += beyond;
        loopToday_.wharfLandedBeyond[base] += beyond;
    }
}

EconomyResult Society::orderFromWharf(const std::string& buyer, const std::string& item, int n, const std::string& why)
{
    if (n < 1 || n > StoreDrawMost)
        return {false, "Order a whole quantity from 1 to " + std::to_string(StoreDrawMost) + "."};
    if (!itemValid(item) || !account(buyer))
        return {false, "No such goods or buyer."};
    if (!account(WharfFund))
        return {false, "The Wharf's purse is missing."};
    const auto base = items::baseOf(item);
    const int area = farmAreaOf(item);
    // A good no area grows: the Wharf's as the Country's was (the season's cap, the world's off list); an area's good: its
    // imports list's.
    if (area < 0 ? (authored_.economy.wharfOff.count(base) || items::cityLoop().wharf.refuses(base)) : !wharfImports(item))
        return {false, "The Wharf does not land " + item + "."};
    if (const int room = area < 0 ? supplyLeft(item) : wharfRoom(item); n > room)
        return {false, "The Wharf can land " + (room > 0 ? "only " + std::to_string(room) : std::string("no more")) + " " + item + " today."};
    const auto cost = stablePrice(item, n);
    const auto toll = gateToll(buyer, cost, false);
    const auto carter = carters();
    const auto [carriage, subsidy] = carter == buyer ? std::pair<std::int64_t, std::int64_t>{0, 0} : carriageSplit(buyer, item, n, false);
    const auto& a = *account(buyer);
    if (stock(a, item) > StockCap - n || (!a.stock.count(item) && a.stock.size() >= MaxGoodsKinds))
        return {false, "The buyer can't hold that many " + item + ".", catalogPrice(item), cost};
    if (a.cash < cost + carriage)
        return {false, "The buyer can't pay " + std::to_string(cost + carriage) + "p for it.", catalogPrice(item), cost};
    if (carriage + subsidy > 0 && carter == CarriageFund && !account(CarriageFund))
        openAccount(CarriageFund);
    const auto pay = [&](const std::string& to, std::int64_t coins, const std::string& kind) { return coins == 0 || shift(buyer, to, "", 0, coins, kind); };
    if (!pay(WharfFund, cost - toll, why) || !pay("treasury", toll, "gate toll: " + why) || !pay(carter, carriage, "carriage in") ||
        !create(buyer, item, n, why))
        return {false, "The order could not be delivered.", catalogPrice(item), cost};   // (Not reached: checked above.)
    gateToll(buyer, cost, true);
    if (carter != buyer)
        carriageSplit(buyer, item, n, true);
    if (subsidy > 0 && shift("treasury", carter, "", 0, subsidy, "carriage subsidy"))
        loopToday_.subsidy += subsidy;
    if (const auto* group = seasonGroup(item); group && area < 0)
        seasonDrawn_[group->id] += n;               // (The sea's season on what it lands of a good no area grows.)
    bookWharfLanding(item, n, cost - toll);
    loopToday_.wharfSales += cost - toll;
    loopToday_.toll += toll;
    loopToday_.carriage += carriage + subsidy;
    return {true, "Ordered " + std::to_string(n) + " " + itemName(item) + " from the Wharf for " + std::to_string(cost) + "p" +
                      (carriage ? ", " + std::to_string(carriage) + "p carriage" : "") + ".",
            catalogPrice(item), cost};
}

EconomyResult Society::moveWharfCredit(std::int64_t amount, const std::string& who, const std::string& note)
{
    const auto most = rule("wharfActionMost");
    if (most <= 0)
        return {false, "The Wharf's credit can't be moved in this world (wharfActionMost is 0)."};
    if (amount == 0 || amount > most || amount < -most)
        return {false, "Move the credit by 1 to " + std::to_string(most) + " pennies either way."};
    auto& book = state_.loop.wharf;
    const auto before = book.credit;
    book.credit = std::min<std::int64_t>(rule("wharfCreditMost"), book.credit + amount);
    const auto moved = book.credit - before;
    book.moved += moved;
    loopToday_.wharfMoved += moved;
    record("wharf credit: " + std::to_string(moved) + "p by " + (who.empty() ? std::string("the DM") : who) +
               (note.empty() ? std::string() : ": " + note),
           "beyond", WharfFund, "", 0, 0);
    return {true, "The Wharf's credit moved by " + std::to_string(moved) + "p, to " + std::to_string(book.credit) + "p.", 0, moved};
}

void Society::settleWharf()
{
    if (!wharfValveOn())
        return;
    const std::int64_t move = rule("wharfInjectADay") - rule("wharfTakeADay");
    if (move == 0)
        return;
    auto& book = state_.loop.wharf;
    const auto before = book.credit;
    book.credit = std::min<std::int64_t>(rule("wharfCreditMost"), book.credit + move);
    book.moved += book.credit - before;
    loopToday_.wharfMoved += book.credit - before;
}

std::int64_t Society::wharfTakesRun(bool commit)
{
    // Each good on the takes list, in the list's order: the farms and works holding it over their keep, and the shops of
    // its `shops` trades over theirs (after wharf7b: the butchers' hides and bones), the largest excess first (then by
    // till), whole pieces, within the good's most a day; committed, within the export room too. A good the room can't
    // fit is left for the next (wharf7b: the run stopped at the first, so the hides were never reached).
    if (!wharfValveOn() || !account(WharfFund))
        return 0;
    const auto& lists = items::cityLoop().wharf;
    const int fee = std::clamp(rule("wharfFee"), 0, 100);
    std::vector<std::pair<const std::string*, const items::WharfGood*>> order;
    for (const auto& [good, def] : lists.takes)
        order.push_back({&good, &def});
    std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.second->rank < b.second->rank; });
    // The shops of each trade a take names (a merchant's post held, its till open), once.
    std::map<std::string, std::vector<std::string>> shopsOf;
    const bool anyShops = std::any_of(order.begin(), order.end(), [](const auto& o) { return !o.second->shops.empty(); });
    if (anyShops)
        for (const auto& p : positions_)
        {
            if (p.role != "merchant")
                continue;
            const auto held = state_.careers.positions.find(p.id);
            if (held == state_.careers.positions.end() || held->second.holder.empty() || dead_.count(held->second.holder))
                continue;
            if (const auto* b = items::businessFor(p.title); b && account("till:" + p.id))
                shopsOf[b->id].push_back("till:" + p.id);
        }
    const int keepDays = rule("wharfKeepDays");
    const auto shopKeep = [&](const std::string& till, const std::string& good, const items::WharfGood& def) {
        if (def.keep >= 0)
            return def.keep;
        double aDay = 0;
        if (const auto t = state_.loop.sales.find(till); t != state_.loop.sales.end())
            if (const auto w = t->second.find(good); w != t->second.end())
                aDay = w->second;
        return std::max({rule("wharfKeepLeast"), int(std::ceil(aDay * keepDays - 1e-9)), shelfOf(till, good)});
    };
    std::int64_t wanted = 0;
    for (const auto& [goodPtr, defPtr] : order)
    {
        const auto& good = *goodPtr;
        const auto& def = *defPtr;
        if (!wharfTakes(good))
            continue;
        const int area = farmAreaOf(good);
        if (area < 0 && def.shops.empty())
            continue;                               // (No area of it on and no shop it is taken from: nothing to take.)
        const auto taken = loopToday_.wharfTaken.find(good);
        int capLeft = int(std::max<std::int64_t>(0, std::int64_t(def.most) * rule("wharfTakeScale") / 100 -
                                                        (taken == loopToday_.wharfTaken.end() ? 0 : taken->second.pieces)));
        struct Holder
        {
            int over;
            std::string till;
            int farm;                               // (Its index in farms_, or -1: a city shop.)
        };
        std::vector<Holder> holders;
        if (area >= 0)
            for (std::size_t i = 0; i < farms_.size(); ++i)
            {
                const auto& f = farms_[i];
                if (!farmAreaOn(f.area) || !account(f.till) || !farmSells(f, std::size_t(area), good))
                    continue;
                if (const int over = stock(*account(f.till), good) - wharfKeep(i, good); over > 0)
                    holders.push_back({over, f.till, int(i)});
            }
        for (const auto& trade : def.shops)
            if (const auto found = shopsOf.find(trade); found != shopsOf.end())
                for (const auto& till : found->second)
                    if (const int over = stock(*account(till), good) - shopKeep(till, good, def); over > 0)
                        holders.push_back({over, till, -1});
        std::stable_sort(holders.begin(), holders.end(), [](const Holder& a, const Holder& b) {
            return a.over != b.over ? a.over > b.over : a.till < b.till;
        });
        const auto total = [&](int q) {
            const auto p = stablePrice(good, q);
            return p + carriageOn(WharfFund, good, q, false) + p * fee / 100;
        };
        // (What the takes wanted of it within its cap, for the unfit: once a good, not once a holder, as a holder's
        // shortfall passes on to the next as cap left.)
        int wantN = 0;
        for (const auto& h : holders)
            wantN = int(std::min<std::int64_t>(std::int64_t(wantN) + h.over, capLeft));
        const int capStart = capLeft;
        for (const auto& h : holders)
        {
            if (capLeft <= 0)
                break;
            int n = std::min(h.over, capLeft);
            if (!commit)
            {
                wanted += total(n);
                capLeft -= n;
                continue;
            }
            const auto room = wharfExportRoom();
            // (Down in proportion, but never past 1 at a step: from 60 bones and room for one, the proportion floored to 0.)
            while (n > 0 && total(n) > room)
                n = std::min(n - 1, std::max(1, int(std::floor(double(n) * double(room) / double(std::max<std::int64_t>(1, total(n)))))));
            if (n < 1)
                break;                              // (No room for this good today: the next, cheaper, may fit.)
            const bool farm = h.farm >= 0;
            const auto price = stablePrice(good, n);
            const auto carriage = carriageOn(WharfFund, good, n, true);
            const auto wharfage = price * fee / 100;
            // (A farm's goods pass the gate as any draw's; a city shop's go to the wharf inside the walls.)
            const auto toll = farm ? gateToll(h.till, price, true) : 0;
            const auto carter = carters();
            if (carriage > 0 && carter == CarriageFund && !account(CarriageFund))
                openAccount(CarriageFund);
            wharfMint(price + carriage + wharfage, "from beyond the realm: " + good);
            shift(WharfFund, h.till, "", 0, price - toll, "bought for beyond the realm: " + good);
            if (toll > 0)
                shift(WharfFund, "treasury", "", 0, toll, "gate toll: to the wharf");
            if (carriage > 0)
                shift(WharfFund, carter, "", 0, carriage, "carriage to the wharf");
            for (int left = n; left > 0; left -= 99)
                shift(h.till, WharfFund, good, std::min(left, 99), 0, "bought for beyond the realm: " + good);
            consume(WharfFund, good, n, "shipped beyond the realm");
            auto& book = state_.loop.wharf;
            book.credit -= price + carriage + wharfage;
            book.fee += wharfage;
            loopToday_.wharfFee += wharfage;
            bookWharfFee(wharfage);
            auto& t = loopToday_.wharfTaken[good];
            t.pieces += n;
            t.pennies += price;
            if (farm)
            {
                auto& f = farms_[std::size_t(h.farm)];
                auto& day = loopToday_.farms[f.def->id];
                day.sold += n;
                day.goods[good].sold += n;
                day.takings += price - toll;
                f.rentTakings += price - toll;      // (Its land rent's or works' rent's base tonight.)
                loopToday_.goodsSold[good] += n;
            }
            else
            {
                auto& c = loopToday_.wharfTakenShops[good];
                c.pieces += n;
                c.pennies += price;
            }
            loopToday_.toll += toll;
            loopToday_.carriage += carriage;
            capLeft -= n;
            wanted += price + carriage + wharfage;
        }
        if (const int missed = wantN - (capStart - capLeft); commit && missed > 0)
            loopToday_.wharfTakeUnfit += total(missed);
    }
    return wanted;
}

std::vector<std::pair<const std::string*, double>> Society::wharfBasketNow() const
{
    // The first market's basket as the farms take the food over (doc 71, 2.6; the user, 2026-11-01): re-weighted to the
    // goods the farms' basket doesn't buy and no food or drink, in proportion to the food areas' takings switched on so far.
    std::vector<std::pair<const std::string*, double>> basket;
    const auto& market = items::cityLoop().markets.front();
    const double shift = wharfBasketShift();
    const auto& after = items::farms().wharfBasket;
    if (shift <= 0 || after.empty())
    {
        for (const auto& [item, share] : market.basket)
            basket.push_back({&item, double(share)});
        return basket;
    }
    // Each good its share of the old basket and of the new, by how far the switch has gone.
    double before = 0, now = 0;
    for (const auto& [item, share] : market.basket)
        before += share;
    for (const auto& [item, share] : after)
        now += share;
    std::map<std::string, std::pair<const std::string*, double>> blend;
    for (const auto& [item, share] : market.basket)
        blend[item] = {&item, before > 0 ? (1 - shift) * share / before : 0.};
    for (const auto& [item, share] : after)
    {
        auto& b = blend.try_emplace(item, std::pair<const std::string*, double>{&item, 0.}).first->second;
        b.second += now > 0 ? shift * share / now : 0.;
    }
    for (const auto& [item, share] : market.basket)
        if (blend[item].second > 0)
            basket.push_back(blend[item]);
    for (const auto& [item, share] : after)
        if (std::none_of(market.basket.begin(), market.basket.end(), [&](const auto& b) { return b.first == item; }))
            basket.push_back(blend[item]);
    return basket;
}

void Society::wharfExports(int hod)
{
    // The export basket (doc 73, 4.5): what credit is left after today's takes buys the city's exports, spread over the
    // shop hours left: the hour's budget minted into the Wharf's purse, spent on the basket at the catalogue's price from
    // the makers' open shops (their goods shipped), and what it did not spend sunk back at once; the fee over what it
    // spent minted to the Wharf. Before the takes' hour their want is kept back for them.
    if (!wharfValveOn() || !account(WharfFund) || hod < ShopOpen || hod >= ShopShut || items::cityLoop().markets.empty())
        return;
    if (loopToday_.wharfTakeWant < 0)
        loopToday_.wharfTakeWant = wharfTakesRun(false);
    const auto reserve = hod < WharfTakeHour ? loopToday_.wharfTakeWant : 0;
    const int fee = std::clamp(rule("wharfFee"), 0, 100);
    const auto room = wharfExportRoom() - reserve;
    if (room <= 0)
        return;
    const auto budget = room * 100 / (100 + fee) / std::max(1, ShopShut - hod);
    if (budget < 1)
        return;
    // Never a good both ways (doc 73, 10, "double-routing"): not one on its imports list (on), nor one it has landed
    // today (copper and tin bars, iron hoops: the realm short of it), so the beyond never buys back what it sold the realm.
    auto basket = wharfBasketNow();
    basket.erase(std::remove_if(basket.begin(), basket.end(),
                                [&](const auto& b) {
                                    const auto base = items::baseOf(*b.first);
                                    const auto landed = loopToday_.wharfImported.find(base);
                                    return wharfImports(base) || (landed != loopToday_.wharfImported.end() && landed->second.pieces > 0);
                                }),
                 basket.end());
    if (basket.empty())
        return;
    const auto keep = cash(WharfFund);
    wharfMint(budget, "from beyond the realm: the export basket");
    std::int64_t carriage = 0;
    const auto spent = basketBuys(WharfFund, basket, double(budget), keep, hod, openShops(), "for beyond the realm",
                                  "shipped beyond the realm", &carriage, false, false, false);
    if (const auto left = cash(WharfFund) - keep; left > 0)
    {
        // (Unspent, back beyond the realm: nothing minted lingers. Counted off the beyond's pennies in, not out.)
        state_.accounts.at(WharfFund).cash -= left;
        state_.minted -= left;
        state_.loop.wharf.in -= left;
        loopToday_.wharfIn -= left;
        record("unspent, back beyond the realm", WharfFund, "beyond", "", 0, left);
    }
    const auto wharfage = spent * fee / 100;
    wharfMint(wharfage, "wharfage on the export basket");
    auto& book = state_.loop.wharf;
    book.credit -= spent + carriage + wharfage;
    book.fee += wharfage;
    loopToday_.wharfFee += wharfage;
    bookWharfFee(wharfage);
    loopToday_.wharfBasket += spent;
}
} // namespace ratw
