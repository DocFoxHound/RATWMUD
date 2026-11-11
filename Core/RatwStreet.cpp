// Doc 79, Phase 6 (Docs/Design/79-a-staged-economy.md, sections 6.5-6.7, 7.7 and 10): street life. The society's half:
// staged buying besides food (an everyday ware, a luxury, a service) and the standing-still rule's waiting buys; the
// shops' business; the Restday dole; a household's band food priced at its market (the short trouble, the lifts); the
// lifted state and its cap; fed_household fame once a week; what a stolen thing is worth; the day's thieves by market.
// The world's half (thieves' targets, the till-food path, begging, the dole's priest, rough sleeping, errand-runners) is
// below it, after the society's.
#include "RatwCalendar.h"
#include "RatwSociety.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace ratw
{
namespace
{
// How far from an ordinary count a count must be before a shop's trade is said to have moved (10.3): a normal tail of
// about one day in twenty by chance alone, so an ordinary day reads steady.
constexpr double Tail = 1.645;

// A shop's business word from its counts (10.3): the day ended and the day before it (pieces to staged buyers and
// services done; a household's staple food is counted only by its shelf running out), whether a ware went out before
// noon, the players' three days against its market's middle, its ordinary trade (ordinaryTrade: pieces a day), its
// shelves' targets, the mean pressure on its wares, its market's Times, and whether it is a luxury shop.
std::string businessWord(const StagedState::Business& b, double ordinary, int target, double meanP, int times, bool luxuries,
                         double playersMedian)
{
    const int players3 = b.playersBefore[0] + b.playersBefore[1] + b.players;
    const int yesterday = b.soldBefore[0], two = b.soldBefore[0] + b.soldBefore[1];
    if (meanP < -.15)
        return "failing";                           // A glut on its wares: the smith whose ore was dumped.
    // Faster than its shelf: a ware sold out before noon (a queue, an empty shelf by noon), a day's sales of its whole
    // shelves over its ordinary trade, a day well above its ordinary one (past chance, and half as much again); or players
    // buying more there than at its market's middle shop.
    const bool brisk = ordinary > 0 && yesterday >= std::max(ordinary + Tail * std::sqrt(ordinary) + 1, 1.5 * ordinary);
    if (b.outEarlyBefore || (target > 0 && yesterday >= target + ordinary) || brisk || (players3 > 0 && players3 > playersMedian))
        return "thriving";
    // The shelf barely moves: two whole days' sales well under two ordinary days' (past chance, and under half of them),
    // once two whole days are known, and only where an ordinary two days is enough to tell (a cartographer selling no map
    // in two days is no news).
    const double two0 = 2 * ordinary;
    const bool barely = b.judged >= 2 && two0 >= 3 && two < std::min(two0 - Tail * std::sqrt(two0), .5 * two0);
    if (barely || (times <= -2 && luxuries))
        return "failing";                           // Or hard times for a luxury shop.
    if (times >= 1 && ordinary > 0 && yesterday >= ordinary + std::sqrt(ordinary))
        return "thriving";                          // (Times the tie-break: good or boom, and a day better than ordinary.)
    return "steady";
}
void rollBusiness(StagedState::Business& b, std::int64_t day)
{
    // (A sale counted between the day's turn at midnight and the market's dawn is today's, never a step back.)
    if (b.day >= day)
        return;
    if (b.day == day - 1)
    {
        b.soldBefore = {b.sold, b.soldBefore[0]};
        b.playersBefore = {b.players, b.playersBefore[0]};
        b.outEarlyBefore = b.outEarly;
    }
    else
    {
        b.soldBefore = {0, 0};
        b.playersBefore = {0, 0};
        b.outEarlyBefore = false;
        b.judged = 0;                               // (Days missed: two whole days to see again before "barely moves".)
    }
    b.sold = b.players = 0;
    b.outEarly = false;
    b.day = day;
}
// The bands' staple foods (status.json rows' `food`): a household's food from its food shop, not an everyday ware.
bool staple(const status::Table& t, const std::string& ware)
{
    for (const auto& row : t.rows)
        if (std::find(row.food.begin(), row.food.end(), ware) != row.food.end())
            return true;
    return false;
}
bool luxury(const std::string& ware)
{
    const auto& lux = market::data().luxuries;
    return std::find(lux.begin(), lux.end(), ware) != lux.end() || std::find(lux.begin(), lux.end(), items::baseOf(ware)) != lux.end();
}
} // namespace

// ---- The day's wares, luxuries and services by market (6.5, 7.7) ----

void Society::streetKeepers()
{
    // What a household of each market may buy besides its food: each keeper's wares it sets out (not the bands' staples;
    // the other foods, a butcher's meat, the greens, the preserves, are wares), the luxuries some keeper there sets out
    // (cheese: a luxury first, though the rich eat it), and the tills of each kind of business (for its services). Each
    // with its price and its shelf's target, its weight in a household's pick; and by band, the running sum of the weights
    // of those it can afford (a piece at most the top of its purse: no poor wolf takes a steel sword off the shelf).
    marketWares_.clear();
    marketLuxuries_.clear();
    businessTills_.clear();
    const auto& t = statusTable();
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto held = state_.careers.positions.find(p.id);
        const auto till = "till:" + p.id;
        if (held == state_.careers.positions.end() || held->second.holder.empty() || !account(till))
            continue;
        const auto market = marketOf(till);
        const auto* founder = spec(p.founder);
        if (const auto* business = founder ? items::businessFor(founder->workLabel) : nullptr)
            businessTills_[market][business->id].push_back(till);
        if (!founder)
            continue;
        for (const auto& ware : stagedShelfGoods(p.founder))
        {
            if (!itemValid(ware))
                continue;
            const bool lux = luxury(ware);
            if (!lux && nourishment(ware) > 0 && staple(t, ware))
                continue;
            const int target = shelfTarget(till, ware);
            if (target > 0)
                (lux ? marketLuxuries_ : marketWares_)[market].list.push_back({till, ware, catalogPrice(ware), double(target)});
        }
    }
    for (auto* by : {&marketWares_, &marketLuxuries_})
        for (auto& [market, shelves] : *by)
            for (int b = 0; b < status::Bands; ++b)
            {
                const auto most = std::min<std::int64_t>(500, t.rows[std::size_t(b)].purseHigh);
                auto& picks = shelves.picks[std::size_t(b)];
                picks.clear();
                double sum = 0;
                for (const auto& w : shelves.list)
                    picks.push_back(sum += w.price <= most ? w.weight : 0);
            }
}

const Society::StreetWare* Society::streetPick(const std::string& market, bool luxuryWare, int band, double lot) const
{
    const auto& by = luxuryWare ? marketLuxuries_ : marketWares_;
    const auto found = by.find(market);
    if (found == by.end())
        return nullptr;
    const auto& picks = found->second.picks[std::size_t(std::clamp(band, 0, status::Bands - 1))];
    if (picks.empty() || picks.back() <= 0)
        return nullptr;
    const auto at = std::upper_bound(picks.begin(), picks.end(), std::clamp(lot, 0., .999999) * picks.back());
    return at == picks.end() ? nullptr : &found->second.list[std::size_t(at - picks.begin())];
}

int Society::serviceEvery(const market::Data::Service& s) const
{
    if (const auto own = authored_.economy.services.find(s.id); own != authored_.economy.services.end())
        return own->second.on ? std::max(1, own->second.every) : 0;
    return std::max(1, s.every);
}

bool Society::streetBuy(const StreetBuy& buy)
{
    // One piece from the shelf into a member's wallet (a raw move: no money, no ledger, no WorldEvent), trimmed with the
    // rest at night.
    const auto home = stagedHomes_.find(buy.household);
    const auto till = state_.accounts.find(buy.till);
    if (home == stagedHomes_.end() || home->second.members.empty() || till == state_.accounts.end() || stock(till->second, buy.ware) <= 0)
        return false;
    for (const auto& id : home->second.members)
    {
        const auto wallet = state_.accounts.find(id);
        if (wallet == state_.accounts.end() || dead_.count(id) || stock(wallet->second, buy.ware) >= StockCap ||
            (!wallet->second.stock.count(buy.ware) && wallet->second.stock.size() >= MaxGoodsKinds))
            continue;
        if ((till->second.stock[buy.ware] -= 1) == 0 && !keptAtZero(buy.ware))
            till->second.stock.erase(buy.ware);
        wallet->second.stock[buy.ware] += 1;
        noteSale(buy.till, buy.ware, 1, false);
        return true;
    }
    return false;
}

bool Society::runnerBuy(const std::string& household, const std::string& till, const std::string& ware)
{
    return streetBuy({household, till, ware});
}

void Society::stagedShopping(const std::string& household)
{
    const auto found = stagedHomes_.find(household);
    if (found == stagedHomes_.end() || found->second.members.empty())
        return;
    const auto& h = found->second;
    const int band = std::clamp(h.band, 0, status::Bands - 1);
    const auto& row = statusTable().rows[std::size_t(band)];
    const auto day = std::to_string(stagedDay_);
    std::vector<StreetBuy> buys;
    // Everyday wares: its band's pieces a day (a mean: the whole part, and one more by a seeded lot for the rest), each
    // picked by its shelf's target among those its band can afford; a luxury by its chance a week.
    const double rate = std::max(0., row.shops);
    const int pieces = int(std::floor(rate)) + (status::lot(household + "|shops|" + day) < rate - std::floor(rate) ? 1 : 0);
    for (int k = 0; k < pieces && k < 16; ++k)
        if (const auto* w = streetPick(h.market, false, band, status::lot(household + "|ware|" + day + "|" + std::to_string(k))))
            buys.push_back({household, w->till, w->ware});
    if (status::lot(household + "|luxury|" + day) < row.luxury / 7)
        if (const auto* w = streetPick(h.market, true, band, status::lot(household + "|luxury pick|" + day)))
            buys.push_back({household, w->till, w->ware});
    for (const auto& b : buys)
        if (street_.watched && street_.watched(*this, b.till))
        {
            // (7.7: a buyer at the counter, one of the household's own; with none to send, it waits for the player to go.)
            const bool sent = ownRule("errandRunners") > 0 && street_.buyer && street_.buyer(*this, b.household, b.till, b.ware);
            if (!sent && buysDeferred_.size() < 4096)
                buysDeferred_.push_back(b);
        }
        else
            streetBuy(b);
    // The services (6.5: `every` x band; a patron's only for the rich, the poor's only a barber's): a sale for the
    // business, nothing on a shelf.
    const auto kinds = businessTills_.find(h.market);
    if (kinds == businessTills_.end() || row.services <= 0)
        return;
    for (const auto& s : market::data().services)
    {
        const int every = serviceEvery(s);
        if (every <= 0 || (s.patrons && band < status::Rich) || (band <= status::Poor && !s.poor))
            continue;
        const auto tills = kinds->second.find(s.business);
        if (tills == kinds->second.end() || tills->second.empty())
            continue;
        if (status::lot(household + "|service|" + s.id + "|" + day) < row.services / double(every))
            noteSale(tills->second[status::hash(household + "|" + s.id) % tills->second.size()], "", 1, false);
    }
}

void Society::streetHour(int hod)
{
    // The food and buys that waited for a player to leave a till's cell (7.7), tried again (a buyer sent to the counter
    // where one can be); at the shops' last hour, the food still watched is fed from elsewhere with no stock moved, and
    // the buys go undone.
    const bool last = hod >= ShopShut - 1;
    for (const auto& home : std::vector<std::string>(foodDeferred_.begin(), foodDeferred_.end()))
        if (const auto h = stagedHomes_.find(home); h != stagedHomes_.end())
        {
            stagedFoodBy(home, h->second.members, FoodBy::Hour);
            if (last && foodDeferred_.count(home))
                stagedFoodBy(home, h->second.members, FoodBy::Unseen);
        }
        else
            foodDeferred_.erase(home);
    std::vector<StreetBuy> still;
    for (const auto& b : buysDeferred_)
        if (street_.watched && street_.watched(*this, b.till))
        {
            if (!last && !(ownRule("errandRunners") > 0 && street_.buyer && street_.buyer(*this, b.household, b.till, b.ware)))
                still.push_back(b);
        }
        else
            streetBuy(b);
    buysDeferred_ = std::move(still);
}

bool Society::runnerBought(const std::string& household)
{
    const auto waiting = runnerWaiting_.find(household);
    const auto h = stagedHomes_.find(household);
    if (waiting == runnerWaiting_.end() || h == stagedHomes_.end())
        return false;
    const auto till = waiting->second;
    const auto* shelf = account(till);
    const bool had = shelf && stock(*shelf, h->second.food) > 0;
    stagedFoodBy(household, h->second.members, FoodBy::Runner, &till);
    runnerWaiting_.erase(household);
    return had;
}

void Society::runnerFailed(const std::string& household)
{
    if (!runnerWaiting_.erase(household))
        return;
    const auto h = stagedHomes_.find(household);
    if (h == stagedHomes_.end())
        return;
    // (Past the shops' last hour there is no hour left to try again: fed from elsewhere now, the shelf left as it stands,
    // a bare one still a short day for the poor. The comfortable are always fed: 6.5.)
    const double hour = (marketClock() - std::floor(marketClock())) * 24;
    if (hour >= ShopShut - 1 || hour < ShopOpen)
        stagedFoodBy(household, h->second.members, FoodBy::Unseen);
    else
        foodDeferred_.insert(household);
}

// ---- Business (10.3) ----

void Society::noteSale(const std::string& till, const std::string& ware, int pieces, bool player, bool staple)
{
    if (!ware.empty() && pieces != 0)
        tillsMoved_.insert(till);                   // (Its shelves moved: journalled whole with the next record.)
    if (pieces == 0 || till.rfind("till:", 0) != 0)
        return;
    auto& b = state_.staged.business[till];
    rollBusiness(b, marketDay());
    // (A household's staple from its food shop is what it eats, not trade: counted only by its shelf running out.)
    if (!staple)
        (player ? b.players : b.sold) = std::max(0, (player ? b.players : b.sold) + pieces);
    if (pieces > 0 && !ware.empty())
        if (const auto* shelf = account(till); shelf && stock(*shelf, ware) <= 0)
            if (const double hour = (marketClock() - std::floor(marketClock())) * 24; hour < 12)
                b.outEarly = true;                  // (Out before noon: a queue, an empty shelf by noon.)
}

std::map<std::string, double> Society::ordinaryTrade() const
{
    // Each market's households as they would be at the slider's middle (status.json's shares at 0), each band buying its
    // pieces a day of everyday wares, its luxuries and its services as stagedShopping picks them: each till's share.
    std::map<std::string, double> out;
    const auto& t = statusTable();
    const auto shares = status::sharesAt(t, 0, ownRule("neverBroke") > 0);
    std::map<std::string, int> households;
    for (const auto& [key, h] : stagedHomes_)
        ++households[h.market];
    const auto spread = [&](const std::map<std::string, StreetShelves>& by, const std::string& market, int band, double pieces) {
        const auto found = by.find(market);
        if (found == by.end() || pieces <= 0)
            return;
        const auto& picks = found->second.picks[std::size_t(band)];
        if (picks.empty() || picks.back() <= 0)
            return;
        for (std::size_t i = 0; i < picks.size(); ++i)
            if (const double w = picks[i] - (i ? picks[i - 1] : 0); w > 0)
                out[found->second.list[i].till] += pieces * w / picks.back();
    };
    for (const auto& [market, n] : households)
        for (int b = 0; b < status::Bands; ++b)
        {
            const double many = n * shares[std::size_t(b)] / 100.;
            const auto& row = t.rows[std::size_t(b)];
            spread(marketWares_, market, b, many * std::max(0., row.shops));
            spread(marketLuxuries_, market, b, many * std::max(0., row.luxury) / 7);
            const auto kinds = businessTills_.find(market);
            if (kinds == businessTills_.end() || row.services <= 0)
                continue;
            for (const auto& s : market::data().services)
            {
                const int every = serviceEvery(s);
                if (every <= 0 || (s.patrons && b < status::Rich) || (b <= status::Poor && !s.poor))
                    continue;
                const auto tills = kinds->second.find(s.business);
                if (tills == kinds->second.end() || tills->second.empty())
                    continue;
                for (const auto& till : tills->second)
                    out[till] += many * row.services / every / double(tills->second.size());
            }
        }
    return out;
}

void Society::businessDay(std::int64_t day)
{
    // Yesterday's word for every shop with a keeper (10.3), against its ordinary trade and its market's middle of players'
    // buying; three days failing running is a status event for the keeper's household (-0.3, 6.3), once for the run (and
    // again each eventDays it lasts, as the first fades), never each night.
    auto& all = state_.staged.business;
    std::map<std::string, std::vector<int>> playersBy;
    std::vector<std::pair<std::string, std::string>> shops;   // (Till, keeper.)
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto held = state_.careers.positions.find(p.id);
        const auto till = "till:" + p.id;
        if (held == state_.careers.positions.end() || held->second.holder.empty() || !account(till))
            continue;
        shops.push_back({till, held->second.holder});
        auto& b = all[till];
        rollBusiness(b, day);
        playersBy[marketOf(till)].push_back(b.playersBefore[0] + b.playersBefore[1]);
    }
    const auto median = [](std::vector<int> v) {
        if (v.empty())
            return 0.;
        std::sort(v.begin(), v.end());
        return double(v[v.size() / 2]);
    };
    std::map<std::string, double> medians;
    for (const auto& [m, v] : playersBy)
        medians[m] = median(v);
    const auto ordinary = ordinaryTrade();
    const auto& table = statusTable();
    const int eventDays = std::max(1, table.eventDays);
    for (const auto& [till, keeper] : shops)
    {
        auto& b = all[till];
        const auto market = marketOf(till);
        const auto* job = jobOf(keeper);
        int target = 0, wares = 0, luxTarget = 0, allTarget = 0;
        double P = 0;
        if (job)
            for (const auto& ware : stagedShelfGoods(job->founder))
            {
                const int shelf = shelfTarget(till, ware);
                const bool lux = luxury(ware);
                allTarget += shelf;
                luxTarget += lux ? shelf : 0;
                if (lux || nourishment(ware) <= 0 || !staple(table, ware))
                    target += shelf;                // (Its shelves' targets but the staples, as its count.)
                P += pressure(market, items::baseOf(ware));
                ++wares;
            }
        // (Rolled to today: the word reads the days gone, yesterday first. A luxury shop: luxuries half its shelves.)
        const auto e0 = ordinary.find(till);
        b.word = businessWord(b, e0 == ordinary.end() ? 0. : e0->second, target, wares ? P / wares : 0, marketTimes(market),
                              allTarget > 0 && luxTarget * 2 >= allTarget, medians[market]);
        b.judged = std::min(1000, b.judged + 1);
        b.failing = b.word == "failing" ? std::min(1000, b.failing + 1) : 0;
        if (b.failing == 3 || (b.failing > 3 && (b.failing - 3) % eventDays == 0))
            noteStatusEvent(keeper, "failing");
    }
    // (A till no keeper holds any more, or gone, is forgotten.)
    std::set<std::string> kept;
    for (const auto& s : shops)
        kept.insert(s.first);
    for (auto it = all.begin(); it != all.end();)
        it = kept.count(it->first) ? std::next(it) : all.erase(it);
}

std::string Society::businessOf(const std::string& keeper) const
{
    const auto* job = jobOf(keeper);
    if (!job || job->role != "merchant")
        return "none";
    const auto till = tillOf(keeper);
    const auto found = state_.staged.business.find(till);
    // (Before its first day's end there is nothing to say of it: "none", as for one who keeps no shop.)
    return found == state_.staged.business.end() || found->second.word.empty() ? std::string("none") : found->second.word;
}

// ---- The dole (10.2) ----

int Society::giveDole(const std::string& household, std::int64_t day)
{
    const auto h = stagedHomes_.find(household);
    if (h == stagedHomes_.end() || h->second.band > status::Poor || watchHousehold(household))
        return 0;
    if (const auto had = state_.staged.dole.find(household); had != state_.staged.dole.end() && had->second >= day)
        return 0;
    const std::string bread = itemValid("bread") ? "bread" : itemValid("meal") ? "meal" : "";
    if (bread.empty())
        return 0;
    const int each = int(std::ceil(statusTable().doleDays * NourishADay / std::max(1, nourishment(bread)) - 1e-9));
    int given = 0;
    for (const auto& id : h->second.members)
    {
        const auto wallet = state_.accounts.find(id);
        if (wallet == state_.accounts.end() || dead_.count(id) || each <= 0 || stock(wallet->second, bread) > StockCap - each ||
            (!wallet->second.stock.count(bread) && wallet->second.stock.size() >= MaxGoodsKinds))
            continue;
        wallet->second.stock[bread] += each;        // (A raw add: no church account, trimmed with the rest at night.)
        given += each;
    }
    if (given > 0)
        state_.staged.dole[household] = day;
    return given;
}

bool Society::onDole(const std::string& id) const
{
    const auto household = householdOf(id);
    const auto found = state_.staged.dole.find(household);
    return !household.empty() && found != state_.staged.dole.end() && state_.budgetDay - found->second < 7 &&
           found->second <= state_.budgetDay;
}

// ---- Prices for the street (6.7, 7.8) ----

std::int64_t Society::marketLot(const std::string& market, const std::string& item, int quantity) const
{
    if (!itemValid(item) || quantity <= 0)
        return 0;
    const auto& t = tradePoints();
    if (market.empty() || !t.find(market))
        return catalogPrice(item) * quantity;
    // (The market's own quote, no till's district factor and no holder's push: Board::ladderStartIn, as a shop's starts.)
    const auto s = ladderStartIn(market, item, marketBase(market, items::baseOf(item), 1), {});
    const int q = std::min(quantity, 99);
    const auto lot = std::max<std::int64_t>(1, ladder(s, q, true, false).lot);
    return quantity <= 99 ? lot : std::int64_t(std::llround(double(lot) * quantity / q));
}

std::int64_t Society::bandFoodCost(const std::string& id, double days) const
{
    // A day of the household's band food a head (its band's meals, never more than a day's need, in its food: bread,
    // fish, a meal or cheese), at its market's quoted price, for `days`. (The poor's two meals are more than a day's
    // need, Review A: a fortnight of them is not what a fortnight's want costs.)
    const auto key = state_.residents.count(id) ? householdOf(id) : id;
    const auto h = stagedHomes_.find(key);
    if (h == stagedHomes_.end() || days <= 0)
        return 0;
    const auto& row = statusTable().rows[std::size_t(std::clamp(h->second.band, 0, status::Bands - 1))];
    const auto food = !h->second.food.empty() ? h->second.food : std::string("meal");
    const double each = std::max(1, nourishment(food));
    const double ration = std::min(row.meals * std::max(1, nourishment("meal")), NourishADay);
    const double pieces = ration / each * double(std::max<std::size_t>(1, h->second.members.size())) * days;
    const int whole = int(std::clamp(std::ceil(pieces - 1e-9), 1., 99. * 99));
    const auto lot = marketLot(h->second.market, food, std::min(whole, 99));
    return whole <= 99 ? lot : std::int64_t(std::llround(double(lot) * whole / 99));
}

// ---- The lifted state (6.7) ----

bool Society::liftedNow(const std::string& id) const
{
    const auto* s = statusOf(id);
    return s && s->liftedUntil >= stagedDay_ && stagedDay_ >= 0;
}

bool Society::liftRoom(const std::string& id) const
{
    // At most half the market's destitute share lifted at once (never fewer than one), so one rich player can't empty a
    // city of its beggars overnight. (A household lifted already has its place.)
    const auto* s = statusOf(id);
    if (!s || s->band > status::Poor || stagedDay_ < 0)
        return false;
    const auto& household = s->household;
    const auto& market = s->market;
    int households = 0, lifted = 0;
    for (const auto& [key, h] : stagedHomes_)
        if (h.market == market)
        {
            ++households;
            if (const auto b = state_.staged.bands.find(key); b != state_.staged.bands.end() && b->second.liftedUntil >= stagedDay_ &&
                                                               key != household)
                ++lifted;
        }
    const double share = status::sharesAt(statusTable(), prosperity(), ownRule("neverBroke") > 0)[status::Destitute];
    const int room = std::max(1, int(std::ceil(.5 * share / 100. * households - 1e-9)));
    return lifted < room;
}

int Society::liftByHelp(const std::string& id, std::int64_t worth, int days)
{
    // By its worth in the household's band food (6.7): a week's or more lifts it a week, a fortnight's (the short trouble's
    // coins) a fortnight, a month's a month; a gift and the trouble's help alike.
    if (!liftRoom(id))
        return 0;
    if (days <= 0)
    {
        const auto week = bandFoodCost(id, 7), fortnight = bandFoodCost(id, 14), month = bandFoodCost(id, 30);
        days = month > 0 && worth >= month ? 30 : fortnight > 0 && worth >= fortnight ? 14 : week > 0 && worth >= week ? 7 : 0;
        if (days <= 0)
            return 0;
    }
    const std::string household = statusOf(id)->household;
    return liftHousehold(household, stagedDay_ + days) ? days : 0;
}

bool Society::fedFameRoom(const std::string& player, const std::string& household)
{
    if (household.empty())
        return true;                                // (A world without the staged households: the season's limit alone.)
    const auto key = holderOf(player).substr(0, 120) + "|" + household.substr(0, 120);
    auto& fed = state_.staged.fed;
    if (const auto last = fed.find(key); last != fed.end() && state_.budgetDay - last->second < 7 && last->second <= state_.budgetDay)
        return false;
    fed[key] = state_.budgetDay;
    // (Kept a week: older ones go.)
    for (auto it = fed.begin(); it != fed.end();)
        it = state_.budgetDay - it->second >= 7 ? fed.erase(it) : std::next(it);
    fed[key] = state_.budgetDay;
    return true;
}

// ---- The day's thieves (10.1) ----

std::string Society::honestyOf(const std::string& id) const
{
    const auto found = honesty_.find(id);
    return found == honesty_.end() ? std::string("plain") : found->second;
}

const std::vector<std::string>* Society::membersOf(const std::string& household) const
{
    const auto found = stagedHomes_.find(household);
    return found == stagedHomes_.end() ? nullptr : &found->second.members;
}

bool Society::watchHousehold(const std::string& household) const
{
    const auto* members = membersOf(household);
    if (!members)
        return false;
    for (const auto& id : *members)
        if (const auto* r = spec(id); r && r->ofTheWatch())
            return true;
    return false;
}

std::string Society::foodOf(const std::string& household) const
{
    const auto found = stagedHomes_.find(household);
    return found == stagedHomes_.end() ? std::string() : found->second.food;
}

Society::TheftDay Society::theftDay(std::int64_t day, const std::function<double(const std::string& id)>& eligible) const
{
    TheftDay out;
    const auto& t = statusTable();
    const int cap = std::max(0, ownRule("theftCap"));
    const auto dayText = std::to_string(day);
    // Each market's households: the sum of their bands' theft weights, and their adults with their own weights.
    std::map<std::string, double> sum;
    std::map<std::string, std::vector<std::pair<std::string, double>>> adults;
    for (const auto& [key, h] : stagedHomes_)
    {
        const int band = std::clamp(h.band, 0, status::Bands - 1);
        const auto& row = t.rows[std::size_t(band)];
        sum[h.market] += row.theft;
        if (h.lifted)
            continue;                               // (Lifted by a player's help: no want to steal from.)
        for (const auto& id : h.members)
        {
            const auto* r = spec(id);
            const auto life = state_.residents.find(id);
            // (Never a guard: one of the Watch by its roster's role, posted or not, 10.1.)
            if (!r || r->age < 16 || dead_.count(id) || life == state_.residents.end() || r->ofTheWatch())
                continue;
            const auto honesty = honestyOf(id);
            double w = row.theft;
            if (w <= 0 && band == status::Rich && honesty == "sly")
                w = t.slyRich;
            const auto trait = t.theftHonesty.find(honesty);
            w *= trait == t.theftHonesty.end() ? 1. : trait->second;
            if (life->second.hunger >= 55)
                w *= 2;
            if (w > 0)
                w *= eligible ? std::max(0., eligible(id)) : 1.;
            if (w > 0)
                adults[h.market].push_back({id, w});
        }
    }
    for (const auto& [market, s] : sum)
    {
        const int times = std::clamp(marketTimes(market), -2, 2);
        const double E = t.theftBase * s * t.theftTimes[std::size_t(times + 2)];
        int n = int(std::floor(E));
        if (status::lot(market + "|thefts|" + dayText) < E - std::floor(E))
            ++n;
        n = std::min(n, cap);
        out.expected[market] = E;
        auto pool = adults[market];
        int picked = 0;
        for (int k = 0; k < n && !pool.empty(); ++k)
        {
            double total = 0;
            for (const auto& [id, w] : pool)
                total += w;
            if (total <= 0)
                break;
            double at = status::lot(market + "|thief|" + dayText + "|" + std::to_string(k)) * total;
            std::size_t i = 0;
            for (; i + 1 < pool.size() && at >= pool[i].second; ++i)
                at -= pool[i].second;
            Thief thief;
            thief.id = pool[i].first;
            thief.market = market;
            thief.hour = 8 + int(status::hash(thief.id + "|theft hour|" + dayText) % 12);
            out.thieves.push_back(std::move(thief));
            pool.erase(pool.begin() + std::ptrdiff_t(i));
            ++picked;
        }
        out.count[market] = picked;
    }
    return out;
}
} // namespace ratw

// ===================================================================================================== The world's

namespace ratw
{
namespace
{
double apart(Vec2 a, Vec2 b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}
bool warmTrade(const std::string& title)
{
    // A post at a fire all night: a bakehouse, a forge, a kiln (10.2's warm wall, D12).
    std::string t = title;
    for (auto& c : t)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    for (const char* w : {"bak", "forge", "smith", "kiln", "oven", "furnace", "smelt"})
        if (t.find(w) != std::string::npos)
            return true;
    return false;
}
std::string tileOf(const Spot& at)
{
    return at.cell + "|" + std::to_string(int(std::floor(at.x))) + "," + std::to_string(int(std::floor(at.y)));
}
// The tiles round a place, in the order a row fills them: the place, then outward ring by ring, along the row (x)
// before across it; or, for a queue, along the row first (beside the front, then on).
const std::vector<std::pair<int, int>>& besideOrder(bool line)
{
    static const auto row = [] {
        std::vector<std::pair<int, int>> out;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -3; dx <= 3; ++dx)
                out.push_back({dx, dy});
        std::stable_sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
            const auto key = [](const std::pair<int, int>& o) {
                const int ax = std::abs(o.first), ay = std::abs(o.second);
                return std::make_tuple(std::max(ax, ay), ay, ax, o.first < 0, o.second < 0);
            };
            return key(a) < key(b);
        });
        return out;
    }();
    static const auto queue = [] {
        std::vector<std::pair<int, int>> out{{0, 0}};
        for (const int dy : {0, 1, -1, 2, -2})
            for (int dx = 1; dx <= 6; ++dx)
                out.push_back({dx, dy});
        for (const int dy : {0, 1, -1, 2, -2})
            for (int dx = 0; dx >= -3; --dx)
                if (dx || dy)
                    out.push_back({dx, dy});
        return out;
    }();
    return line ? queue : row;
}
} // namespace

Spot World::freeBeside(const Spot& at, std::set<std::string>& taken, bool line) const
{
    // (A row on one tile was the old fallback: passable() with no `from` asks about a step up from height 0, so on DEV's
    // raised streets every tile beside a place looked blocked. Here each is asked from the place's own ground.) Never a
    // door's tile, open or shut (inDoorway, as a square's stalls and crowd since the repair after doc 79's Phases 4 and 5
    // merged): a door open when the hour's places are found would shut on whoever stands in it.
    const auto* c = cell(at.cell);
    const bool loaded = c && c->loaded && c->tiles.size() == std::size_t(c->width) * std::size_t(c->height);
    const Tile* ground = loaded ? c->tile(int(std::floor(at.x)), int(std::floor(at.y))) : nullptr;
    for (const auto& [dx, dy] : besideOrder(line))
    {
        Spot here = at;
        here.x += dx;
        here.y += dy;
        const auto key = tileOf(here);
        if (taken.count(key) || inDoorway(here.cell, {here.x, here.y}))
            continue;
        if (loaded && (!standable(here.cell, {here.x, here.y}) || (ground && !passable(here.cell, {here.x, here.y}, ground))))
            continue;
        taken.insert(key);
        return here;
    }
    return at;
}

const Society::TheftDay& World::theftPlan()
{
    // (Worked out once the society has made its staged households: before that, on a fresh world's first passes, nobody
    // has a band to steal by, and an empty plan kept for the day would leave the whole first day without a thief.)
    static const Society::TheftDay none;
    if (!society_.stagedReady())
        return none;
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (theftPlanDay_ != today)
    {
        theftPlanDay_ = today;
        theftPlan_ = society_.theftDay(today, [this](const std::string& id) {
            const auto* e = entity(id);
            const auto* job = society_.jobOf(id);
            const auto* r = society_.spec(id);
            if (!e || e->dead || !e->npc || custodyOf(id) || (job && job->role == "guard") || (r && r->ofTheWatch()) ||
                !e->leaderId.empty() || e->state == "following")
                return 0.;
            return 1.;
        });
    }
    return theftPlan_;
}

bool World::cellWatched(const std::string& cell) const
{
    if (watchedAt_ != time_)
    {
        watchedCells_ = stageCells();
        watchedAt_ = time_;
    }
    return !cell.empty() && watchedCells_.count(cell) > 0;
}

bool World::tillWatched(const std::string& till) const
{
    if (till.rfind("till:", 0) != 0)
        return false;
    const auto* p = society_.position(till.substr(5));
    if (!p)
        return false;
    if (cellWatched(p->work.cell))
        return true;
    // (A keeper at a stall on the square sells from it: its cell is the shop's then.)
    const auto held = society_.state().careers.positions.find(p->id);
    if (held != society_.state().careers.positions.end())
        if (const auto* keeper = entity(held->second.holder); keeper && cellWatched(keeper->cellId))
            return true;
    return false;
}

const Spot* World::doorOut(const std::string& cellId)
{
    // Where a cell's first door to the open air comes out (a chapel's steps, a bakehouse's doorway); null for none. Kept
    // for the day (the doors move only with a host's edit).
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (chapelStepsDay_ != today)
    {
        chapelSteps_.clear();
        doorsOut_.clear();
        chapelStepsDay_ = today;
    }
    if (const auto found = doorsOut_.find(cellId); found != doorsOut_.end())
        return found->second.cell.empty() ? nullptr : &found->second;
    auto& out = doorsOut_[cellId];
    for (const auto& [id, d] : doors_)
        if (d.cellId == cellId && !d.targetCell.empty())
            if (const auto* to = cell(d.targetCell); to && to->outdoors)
            {
                out = {d.targetCell, d.arrival.x, d.arrival.y};
                break;
            }
    return out.cell.empty() ? nullptr : &out;
}

std::string World::freeMember(const std::string& household) const
{
    const auto* members = society_.membersOf(household);
    if (!members)
        return {};
    for (const bool posted : {false, true})
        for (const auto& id : *members)
        {
            const auto* e = entity(id);
            const auto* life = society_.resident(id);
            const auto* job = society_.jobOf(id);
            const auto* r = society_.spec(id);
            const bool watch = r && r->ofTheWatch();   // (One of the Watch with no post: still at its rounds.)
            if (!e || !life || e->dead || !e->npc || e->age < 10 || !e->leaderId.empty() || e->state == "following" || custodyOf(id) ||
                drivers_.count(id) || seated_.count(id) || runners_.count(id) || doleTakers_.count(id) || beggars_.count(id) ||
                inBattle(id) || (job || watch) != posted)
                continue;
            if (life->task == "sleep" || life->task == "eat" || life->task == "companion" ||
                ((job || watch) && ((job && life->task == job->title) || life->task == "trade" || life->task == "patrol" ||
                                    life->task == "watch" || life->task == "preaching")))
                continue;
            return id;
        }
    return {};
}

namespace
{
// (An open shop: its keeper keeping it. Its counter: the serving spot, or the keeper's side of the stall it keeps.)
bool openShop(const Society& society, const Position& p)
{
    const auto held = society.state().careers.positions.find(p.id);
    const auto* keeping = held == society.state().careers.positions.end() ? nullptr : society.resident(held->second.holder);
    return keeping && keeping->task == "trade";
}
} // namespace

const Position* World::runnerShop(const std::string& household, const std::string& till)
{
    // The household's own food shop while open (its keeper keeping it); that one shut, the open one of its market with
    // the food on its shelf nearest the household's door, by the street (a shop's door on it, a stall's spot); null for
    // none.
    const auto* own = till.rfind("till:", 0) == 0 ? society_.position(till.substr(5)) : nullptr;
    if (!own)
        return nullptr;
    if (openShop(society_, *own))
        return own;
    const auto food = society_.foodOf(household);
    const auto market = society_.marketOf(till);
    // (Where a place is in the world: an outdoor spot's own, an indoor one's door on the street.)
    const auto onStreet = [&](const Spot& at, Vec2& out) {
        const Spot* street = &at;
        const auto* c = cell(at.cell);
        if (c && !c->outdoors)
            street = doorOut(at.cell), c = street ? cell(street->cell) : nullptr;
        if (!c || !street)
            return false;
        out = {c->worldX + street->x, c->worldY + street->y};
        return true;
    };
    Vec2 from;
    bool known = false;
    if (const auto* members = society_.membersOf(household))
        for (const auto& id : *members)
            if (const auto* life = society_.resident(id); life && !life->homeCell.empty() && !known)
                if (const auto* home = cell(life->homeCell))
                    known = onStreet({home->id, home->width / 2., home->height / 2.}, from);
    if (!known)
        known = onStreet(own->work, from);
    const Position* p = nullptr;
    double best = 0;
    for (const auto& other : society_.positions())
    {
        if (other.role != "merchant" || other.id == own->id || !openShop(society_, other))
            continue;
        const auto* shelf = society_.account("till:" + other.id);
        if (!shelf || food.empty() || Society::stock(*shelf, food) <= 0 || society_.marketOf("till:" + other.id) != market)
            continue;
        Vec2 there;
        const double far = known && onStreet(other.work, there) ? apart(from, there) : 1e12;
        if (!p || far < best || (far == best && other.id < p->id))
            p = &other, best = far;
    }
    return p;
}

bool World::sendRunner(const std::string& household, std::string& till)
{
    // A household's errand-runner (6.6): a member free to go (no post at its hours first), to the counter of the shop
    // runnerShop names (its own food shop, or, that one shut, the nearest open one with its food), while that cell has
    // fewer than RunnersACell and its market fewer than RunnersAMarket. `till` is set to the one it goes to.
    if (till.rfind("till:", 0) != 0 || !tillWatched(till))
        return false;
    const Position* p = runnerShop(household, till);
    if (!p)
        return false;
    Spot at = p->serve.cell.empty() ? p->work : p->serve;
    const auto held = society_.state().careers.positions.find(p->id);
    if (held != society_.state().careers.positions.end())
        if (const auto* keeper = entity(held->second.holder); keeper && !keeper->dead && keeper->cellId != p->work.cell &&
                                                            society_.atStall(held->second.holder))
            at = {keeper->cellId, keeper->position.x + 1, keeper->position.y};   // (Its stall: the keeper's side of it.)
    const auto to = "till:" + p->id;
    const auto market = society_.marketOf(to);
    int inCell = 0, inMarket = 0;
    for (const auto& [id, r] : runners_)
    {
        inCell += r.at.cell == at.cell;
        inMarket += society_.marketOf(r.till) == market;
    }
    if (inCell >= RunnersACell || inMarket >= RunnersAMarket)
        return false;
    const auto id = freeMember(household);
    if (id.empty())
        return false;
    runners_[id] = {household, to, at, time_, -1, {}};
    till = to;
    return true;
}

bool World::sendBuyer(const std::string& household, const std::string& till, const std::string& ware)
{
    // A buy besides food at a watched till (7.7): one of the household's own walks to the counter of the open shop (a rich
    // buyer is a rich household's member), within the runners' caps; at the counter the piece leaves the shelf.
    const auto* p = till.rfind("till:", 0) == 0 ? society_.position(till.substr(5)) : nullptr;
    if (!p || ware.empty() || !tillWatched(till) || !openShop(society_, *p))
        return false;
    Spot at = p->serve.cell.empty() ? p->work : p->serve;
    const auto held = society_.state().careers.positions.find(p->id);
    if (held != society_.state().careers.positions.end())
        if (const auto* keeper = entity(held->second.holder); keeper && !keeper->dead && keeper->cellId != p->work.cell &&
                                                            society_.atStall(held->second.holder))
            at = {keeper->cellId, keeper->position.x + 1, keeper->position.y};
    const auto market = society_.marketOf(till);
    int inCell = 0, inMarket = 0;
    for (const auto& [id, r] : runners_)
    {
        inCell += r.at.cell == at.cell;
        inMarket += society_.marketOf(r.till) == market;
    }
    if (inCell >= RunnersACell || inMarket >= RunnersAMarket)
        return false;
    const auto id = freeMember(household);
    if (id.empty())
        return false;
    runners_[id] = {household, till, at, time_, -1, ware};
    return true;
}

const Spot* World::chapelSteps(const std::string& community)
{
    // Outside the chapel's door (10.2): where its first door to the open air comes out, else the chapel itself.
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (chapelStepsDay_ != today)
    {
        chapelSteps_.clear();
        doorsOut_.clear();
        chapelStepsDay_ = today;
    }
    if (const auto found = chapelSteps_.find(community); found != chapelSteps_.end())
        return found->second.cell.empty() ? nullptr : &found->second;
    auto& steps = chapelSteps_[community];
    const auto& ch = chapel(community);
    if (!ch.found)
        return nullptr;
    const auto* out = doorOut(ch.pulpit.cell);
    steps = out ? *out : ch.pews.empty() ? ch.pulpit : ch.pews.back();
    return &steps;
}

void World::placeBeggars()
{
    // Marketday and Restday, 8:00 to 18:00 (10.2): destitute adults without a post (never one of the Watch, posted or
    // not), not lifted, at their market's chapel steps, by its notice board, and before its shops' doors and stalls ("at
    // the market"), at most beggarsACell at one place, each place BeggarApart tiles or more from the next in its cell (10.2's
    // "2 a cell" made per place, since a DEV city cell is a quarter of the city: a change for the user's nod), side by
    // side on tiles of their own; in a seeded order of the day, so who begs where changes, and
    // as many as the band mix makes (T7: more in hard times, few in a boom).
    beggars_.clear();
    beggarWhere_.clear();
    streetReady_ = society_.stagedReady();
    const int weekday = calendar::weekdayOf(calendarDays_);
    const double hour = (calendarDays_ - std::floor(calendarDays_)) * 24;
    if ((weekday != calendar::Marketday && weekday != calendar::Restday) || hour < 8 || hour >= 18)
        return;
    const auto day = std::to_string(std::int64_t(std::floor(calendarDays_)));
    const int most = std::max(0, society_.statusTable().beggarsACell);
    std::map<std::string, std::vector<std::pair<std::uint64_t, std::string>>> byMarket;
    std::set<std::string> taken;                    // (The tiles the beggars stand on.)
    for (const auto& [id, life] : society_.state().residents)
    {
        const auto* s = society_.statusOf(id);
        const auto* e = entity(id);
        const auto* r = society_.spec(id);
        if (!s || s->band != status::Destitute || society_.liftedNow(id) || !e || e->dead || !e->npc || !r || r->age < 16 ||
            r->ofTheWatch() || society_.jobOf(id) || society_.apprenticedTo(id) || !e->leaderId.empty() || e->state == "following" ||
            custodyOf(id) || drivers_.count(id) || seated_.count(id))
            continue;
        byMarket[s->market].push_back({status::hash(id + "|begs|" + day), id});
    }
    for (auto& [market, list] : byMarket)
    {
        if (most <= 0)
            break;
        std::sort(list.begin(), list.end());
        const auto* t = roadMarket(market);
        const auto community = t && !t->market.empty() ? lawTown(t->market) : market;
        std::vector<std::pair<Spot, std::string>> spots;
        const auto add = [&](const Spot& at, const char* where) {
            if (at.cell.empty())
                return;
            for (const auto& [other, w] : spots)
                if (other.cell == at.cell && apart({other.x, other.y}, {at.x, at.y}) < BeggarApart)
                    return;
            spots.push_back({at, where});
        };
        const auto room = [&] { return spots.size() * std::size_t(most) >= list.size(); };
        if (const auto* steps = chapelSteps(community))
        {
            add(*steps, "at the chapel steps");
            taken.insert(tileOf(*steps));           // (The door's mouth, where the priest hands out the dole: beside it.)
        }
        if (const auto* board = boardSpot(community))
            add(*board, "at the market");
        else if (t && !t->market.empty())
            add({t->market, t->marketX + 2, t->marketY + 1}, "at the market");
        // Before the market's shops: a stall's side in the open, a shop's doorway on the street.
        for (const auto& p : society_.positions())
        {
            if (room())
                break;
            if (p.role != "merchant" || p.work.cell.empty() || society_.marketOf("till:" + p.id) != market)
                continue;
            const auto* c = cell(p.work.cell);
            if (c && c->outdoors)
                add({p.work.cell, p.work.x + 2, p.work.y + 1}, "at the market");
            else if (const auto* out = c ? doorOut(p.work.cell) : nullptr)
                add({out->cell, out->x + 1, out->y}, "at the market");
        }
        std::size_t next = 0;
        for (const auto& [spot, where] : spots)
            for (int k = 0; next < list.size() && k < most; ++k)
            {
                const Spot at = freeBeside(spot, taken);   // (Side by side, never two on one tile.)
                beggars_[list[next].second] = at;
                beggarWhere_[list[next].second] = where;
                ++next;
            }
    }
}

void World::placeSleepers()
{
    // Some winter nights (D12: a third of them, by a seeded lot), the destitute with nowhere of their own (no home, or a
    // communal one) sleep by a warm wall in their market: in the shelter of a bakehouse's, a forge's or a kiln's doorway
    // on the street (by its fire, where it works in the open), at most beggarsACell to a wall, never inside; with every
    // warm wall taken, or none, by the chapel's.
    warmWall_.clear();
    const double hour = (calendarDays_ - std::floor(calendarDays_)) * 24;
    if (calendar::calendarAt(calendarDays_).season != calendar::Season::Winter || (hour >= 6 && hour < 22))
        return;
    const auto night = std::int64_t(std::floor(calendarDays_ - .25));
    warmPlaces_.clear();
    std::map<std::string, std::set<std::string>> seen;
    for (const auto& p : society_.positions())
        if (!p.work.cell.empty() && warmTrade(p.title))
            if (const auto* c = cell(p.work.cell))
                if (const auto* m = roadMarketOf(p.work.cell); m && warmPlaces_[m->id].size() < 16)
                {
                    Spot at;
                    if (c->outdoors)
                        at = {p.work.cell, p.work.x + 1, p.work.y + 1};
                    else if (const auto* out = doorOut(p.work.cell))
                        at = *out;
                    if (!at.cell.empty() && seen[m->id].insert(at.cell + "|" + std::to_string(int(at.x)) + "," + std::to_string(int(at.y))).second)
                        warmPlaces_[m->id].push_back(at);
                }
    const int most = std::max(1, society_.statusTable().beggarsACell);
    std::map<std::string, int> taken;
    std::set<std::string> tiles;                    // (The tiles the sleepers lie on.)
    for (const auto& [id, life] : society_.state().residents)
    {
        const auto* s = society_.statusOf(id);
        const auto* e = entity(id);
        const auto* r = society_.spec(id);
        // (Never one of the Watch: its bunk at the Watch House is a communal home, but its town keeps it.)
        if (!s || s->band != status::Destitute || society_.liftedNow(id) || !e || e->dead || !e->npc || !e->leaderId.empty() ||
            custodyOf(id) || (r && r->ofTheWatch()) || (!life.homeCell.empty() && !Society::communalHome(life.homeCell)))
            continue;
        if (status::lot(id + "|warm wall|" + std::to_string(night)) >= 1. / 3)
            continue;
        std::vector<Spot> places;
        if (const auto found = warmPlaces_.find(s->market); found != warmPlaces_.end())
            places = found->second;
        const auto start = places.empty() ? std::size_t(0) : std::size_t(status::hash(id + "|warm") % places.size());
        Spot at;
        for (std::size_t i = 0; i < places.size() && at.cell.empty(); ++i)
        {
            const auto& place = places[(start + i) % places.size()];
            auto& n = taken[place.cell + "|" + std::to_string(int(place.x)) + "," + std::to_string(int(place.y))];
            if (n < most)
                at = place, ++n;
        }
        if (at.cell.empty())
        {
            // (No warm wall left: the chapel's, in its doorway's shelter.)
            const auto* t = roadMarket(s->market);
            const auto* steps = chapelSteps(t && !t->market.empty() ? lawTown(t->market) : s->market);
            if (!steps)
                continue;
            at = *steps;
        }
        warmWall_[id] = freeBeside(at, tiles);       // (Side by side along the wall, never two on one tile.)
    }
}

bool World::streetErrand(const std::string& resident, const ResidentLife& life, std::string& task, std::string& reason,
                         std::string& goalCell, Vec2& goal) const
{
    if (const auto r = runners_.find(resident); r != runners_.end())
    {
        task = "fetching goods";
        reason = "for the household";
        goalCell = r->second.at.cell;
        goal = {r->second.at.x, r->second.at.y};
        return true;
    }
    if (const auto t = doleTakers_.find(resident); t != doleTakers_.end())
    {
        task = "waiting for the dole";
        reason = "Restday bread for the household, at the chapel steps.";
        goalCell = t->second.at.cell;
        goal = {t->second.at.x, t->second.at.y};
        return true;
    }
    if (const auto p = dolePriests_.find(resident); p != dolePriests_.end() && life.task != "sleep")
    {
        task = "handing out the dole";
        reason = "Restday: bread for the poor, at the chapel.";
        goalCell = p->second.cell;
        goal = {p->second.x, p->second.y};
        return true;
    }
    if (const auto b = beggars_.find(resident); b != beggars_.end() && life.task != "sleep")
    {
        const auto where = beggarWhere_.find(resident);
        task = "begging";
        reason = where != beggarWhere_.end() ? where->second : std::string("at the market");
        goalCell = b->second.cell;
        goal = {b->second.x, b->second.y};
        return true;
    }
    if (const auto w = warmWall_.find(resident); w != warmWall_.end() && life.task == "sleep")
    {
        task = "sleeping by the warm wall";
        reason = "A winter night, and nowhere warm of their own.";
        goalCell = w->second.cell;
        goal = {w->second.x, w->second.y};
        return true;
    }
    return false;
}

void World::tendDole(std::int64_t today, double hod)
{
    // The Restday dole (10.2): the hour after the service, the first clergy post held in each community hands bread to
    // its destitute and poor households at the chapel steps. With a player near the steps it is seen: one of each
    // household comes and waits in a queue (at most DoleQueue at once), and is handed it at the front; out of sight it is
    // given as it would be, raw into their wallets. Whoever is still unserved when the dole's hours end is given it then
    // (the priest's helpers take the rest round); after a restart past the dole's hours on a Restday (a crash before the
    // checkpoint took the bread given), the households unserved are given it then, once. Once a Restday a household;
    // never to a player, nor to a household with one of the Watch in it.
    const bool restday = calendar::weekdayOf(calendarDays_) == calendar::Restday;
    const bool open = restday && hod >= Society::ServiceEnd && hod < Society::ServiceEnd + 2;
    const bool after = restday && hod >= Society::ServiceEnd + 2;
    const auto dayText = std::to_string(today);
    // Each hour: the priests at their chapels' steps, and the households of each community that opened its dole today
    // (after its hours, of each that this run never closed: its first hour after a restart).
    const auto hourKey = std::int64_t(std::floor(calendarDays_ * 24));
    if (doleListHour_ != hourKey)
    {
        doleListHour_ = hourKey;
        dolePriests_.clear();
        doleSteps_.clear();
        doleHomes_.clear();
        doleLine_.clear();
        std::set<std::string> late;                 // (Communities whose dole this run never closed, its hours gone.)
        for (const auto& p : society_.positions())
        {
            if (!open && !after)
                break;
            if (!Society::clergy(p.title))
                continue;
            const auto community = lawTown(p.work.cell);
            const auto held = society_.state().careers.positions.find(p.id);
            const auto* priest = held == society_.state().careers.positions.end() ? nullptr : entity(held->second.holder);
            if (community.empty() || doleSteps_.count(community) || late.count(community) || !priest || priest->dead ||
                !chapel(community).found)
                continue;
            if (after)
            {
                if (!doleClosed_.count(community + "|" + dayText))
                    late.insert(community), dolesGiven_.insert(community + "|" + dayText);
                continue;
            }
            const auto* at = chapelSteps(community);
            doleSteps_[community] = at ? *at : p.work;
            dolePriests_[held->second.holder] = doleSteps_[community];
            dolesGiven_.insert(community + "|" + dayText);
        }
        for (const auto& [id, life] : society_.state().residents)
            if (const auto* s = society_.statusOf(id); s && s->band <= status::Poor)
            {
                const auto* e = entity(id);
                const auto home = !life.homeCell.empty() ? life.homeCell : e ? e->cellId : std::string();
                if (home.empty())
                    continue;
                const auto community = lawTown(home);
                if (dolesGiven_.count(community + "|" + dayText) && (open || late.count(community)) &&
                    !society_.watchHousehold(s->household))
                    doleHomes_[community].insert(s->household);
            }
        // The queue's places at each chapel: beside the priest along the steps, each a tile of its own (raised ground
        // too), none where a beggar stands.
        if (open)
        {
            std::set<std::string> taken;
            for (const auto& [id, at] : beggars_)
                taken.insert(tileOf(at));
            for (const auto& [community, at] : doleSteps_)
            {
                taken.insert(tileOf(at));
                auto& line = doleLine_[community];
                for (int place = 0; place < DoleQueue; ++place)
                    line.push_back(freeBeside(at, taken, true));
            }
        }
    }
    // Out of the dole's hours: what is left is given, once, and the queue goes home.
    if (!open)
    {
        doleTakers_.clear();
        for (const auto& [community, homes] : doleHomes_)
            if (doleClosed_.insert(community + "|" + dayText).second)
                for (const auto& h : homes)
                    society_.giveDole(h, today);
        if (dolesGiven_.size() > 1024)
            dolesGiven_.clear(), doleClosed_.clear();
        return;
    }
    // The queue: at the front, served after its seconds, the rest moving up; one an hour on its way goes home.
    for (auto it = doleTakers_.begin(); it != doleTakers_.end();)
    {
        auto& t = it->second;
        const auto* e = entity(it->first);
        if (!e || e->dead || society_.doleGiven(t.household, today) || time_ - t.since > calendar::SecondsPerDay / 24 ||
            !e->leaderId.empty() || custodyOf(it->first))
        {
            it = doleTakers_.erase(it);
            continue;
        }
        if (t.place == 0 && e->cellId == t.at.cell && apart(e->position, {t.at.x, t.at.y}) <= 1.6)
        {
            if (t.atFront < 0)
                t.atFront = time_;
            else if (time_ - t.atFront >= DoleSeconds)
            {
                society_.giveDole(t.household, today);
                it = doleTakers_.erase(it);
                continue;
            }
        }
        ++it;
    }
    for (const auto& [community, at] : doleSteps_)
    {
        const auto homes = doleHomes_.find(community);
        if (homes == doleHomes_.end())
            continue;
        if (!cellWatched(at.cell))
        {
            // (Nobody near to see it: given as it would be.)
            for (const auto& h : homes->second)
                society_.giveDole(h, today);
            for (auto it = doleTakers_.begin(); it != doleTakers_.end();)
                it = it->second.community == community ? doleTakers_.erase(it) : std::next(it);
            continue;
        }
        // In line by the priest: places 0.. along the steps; the queue moved up as the front is served.
        std::vector<std::string> line;
        std::set<std::string> coming;
        for (const auto& [id, t] : doleTakers_)
            if (t.community == community)
                line.push_back(id), coming.insert(t.household);
        std::sort(line.begin(), line.end(), [&](const std::string& a, const std::string& b) {
            return std::pair(doleTakers_.at(a).place, a) < std::pair(doleTakers_.at(b).place, b);
        });
        const auto& places = doleLine_[community];
        const auto spotAt = [&](int place) { return place >= 0 && std::size_t(place) < places.size() ? places[std::size_t(place)] : at; };
        int place = 0;
        for (const auto& id : line)
        {
            auto& t = doleTakers_.at(id);
            if (t.place != place)
                t.place = place, t.atFront = -1;
            t.at = spotAt(place);                   // (The hour's places: a beggar may have come to stand where it was.)
            ++place;
        }
        std::vector<std::pair<std::uint64_t, std::string>> waiting;
        for (const auto& h : homes->second)
            if (!coming.count(h) && !society_.doleGiven(h, today))
                waiting.push_back({status::hash(h + "|dole|" + dayText), h});
        std::sort(waiting.begin(), waiting.end());
        for (const auto& [rank, h] : waiting)
        {
            if (place >= DoleQueue)
                break;
            const auto id = freeMember(h);
            if (id.empty())
                continue;
            doleTakers_[id] = {h, community, spotAt(place), place, time_, -1};
            ++place;
        }
    }
}

void World::tendStreet()
{
    const auto today = std::int64_t(std::floor(calendarDays_));
    // Runners (6.6, 7.7): at the counter for its seconds, then the household's food (or its buy) from the shelf; one an
    // hour on the way gives up (its food waits for the next hour, or past the shops' last hour is fed from elsewhere; a
    // buy is let go).
    for (auto it = runners_.begin(); it != runners_.end();)
    {
        auto& r = it->second;
        const auto* e = entity(it->first);
        const bool food = r.ware.empty();
        if (!e || e->dead || (food && !society_.runnerWaiting(r.household)))
        {
            it = runners_.erase(it);
            continue;
        }
        if (time_ - r.since > calendar::SecondsPerDay / 24 || !e->leaderId.empty() || custodyOf(it->first))
        {
            const auto household = r.household;
            it = runners_.erase(it);
            if (food)
                society_.runnerFailed(household);
            continue;
        }
        if (e->cellId == r.at.cell && apart(e->position, {r.at.x, r.at.y}) <= 1.6)
        {
            if (r.atCounter < 0)
                r.atCounter = time_;
            else if (time_ - r.atCounter >= RunnerSeconds)
            {
                if (food)
                    society_.runnerBought(r.household);
                else
                    society_.runnerBuy(r.household, r.till, r.ware);
                it = runners_.erase(it);
                continue;
            }
        }
        ++it;
    }
    // Each hour (and once the society's staged households are first made): the beggars' and the sleepers' places.
    if (const auto hour = std::int64_t(std::floor(calendarDays_ * 24)); hour != streetHour_ || (!streetReady_ && society_.stagedReady()))
    {
        streetHour_ = hour;
        placeBeggars();
        placeSleepers();
    }
    // The Restday dole, each pass (its queue moves).
    if (calendar::weekdayOf(calendarDays_) == calendar::Restday || !doleTakers_.empty() || !dolePriests_.empty() || !doleSteps_.empty())
        tendDole(today, (calendarDays_ - std::floor(calendarDays_)) * 24);
    // Alms (10.2), each game minute: a comfortable or rich resident close by a beggar gives a penny, by its band's chance,
    // once a day a pair: a staged move between two residents' purses (no ledger, no journal: never a player's).
    if (const auto minute = std::int64_t(std::floor(calendarDays_ * 1440)); minute != streetMinute_ && !beggars_.empty())
    {
        streetMinute_ = minute;
        if (almsDay_ != today)
        {
            almsGiven_.clear();
            almsDay_ = today;
        }
        const auto& rows = society_.statusTable().rows;
        for (const auto& [beggar, spot] : beggars_)
        {
            const auto* b = entity(beggar);
            const auto* bs = society_.statusOf(beggar);
            if (!b || !bs || b->cellId != spot.cell || apart(b->position, {spot.x, spot.y}) > 2 ||
                Society::classify(beggar) != Society::AccountKind::Staged)
                continue;
            for (const auto& [id, e] : entities_)
            {
                if (!e.npc || e.dead || id == beggar || e.cellId != b->cellId || apart(e.position, b->position) > 3)
                    continue;
                const auto* gs = society_.statusOf(id);
                if (!gs || gs->band < status::Comfortable || gs->household == bs->household || almsGiven_.size() > 65536 ||
                    Society::classify(id) != Society::AccountKind::Staged)
                    continue;
                const auto key = id + "|" + beggar;
                if (!almsGiven_.insert(key).second)
                    continue;
                if (status::lot(key + "|alms|" + std::to_string(today)) < rows[std::size_t(gs->band)].alms && society_.cash(id) >= 1)
                {
                    society_.stagedSet(id, society_.cash(id) - 1);
                    society_.stagedSet(beggar, society_.cash(beggar) + 1);
                }
            }
        }
    }
}
} // namespace ratw
