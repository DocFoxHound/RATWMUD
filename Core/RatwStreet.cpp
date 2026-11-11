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

namespace ratw
{
namespace
{
// A shop's business word from its counts (10.3): the day ended (sold, players), the day before it, whether a ware went
// out before noon, the players' three days against its market's middle, the mean pressure on its wares, its market's
// Times, and whether it deals in luxuries.
std::string businessWord(const StagedState::Business& b, int target, double meanP, int times, bool luxuries, double playersMedian)
{
    const int players3 = b.playersBefore[0] + b.playersBefore[1] + b.players;
    const int sold2 = b.soldBefore[0] + b.soldBefore[1];
    if (meanP < -.15)
        return "failing";                           // A glut on its wares: the smith whose ore was dumped.
    if (b.outEarlyBefore || (target > 0 && b.soldBefore[0] * 2 >= target) || (players3 > 0 && players3 > playersMedian) || times >= 1)
        return "thriving";
    if ((target > 0 && sold2 * 20 < target) || (times <= -2 && luxuries))
        return "failing";                           // The shelf barely moves, or hard times for a luxury.
    return "steady";
}
void rollBusiness(StagedState::Business& b, std::int64_t day)
{
    if (b.day == day)
        return;
    if (b.day == day - 1)
    {
        b.soldBefore = {b.sold, b.soldBefore[0]};
        b.playersBefore = {b.players, b.playersBefore[0]};
        b.outEarlyBefore = b.outEarly;
    }
    else if (b.day < day)
    {
        b.soldBefore = {0, 0};
        b.playersBefore = {0, 0};
        b.outEarlyBefore = false;
    }
    b.sold = b.players = 0;
    b.outEarly = false;
    b.day = day;
}
} // namespace

// ---- The day's wares, luxuries and services by market (6.5, 7.7) ----

void Society::streetKeepers()
{
    // What a household of each market may buy besides its food: each keeper's wares it sets out (not the bands' foods,
    // not a luxury), the luxuries some keeper there sets out, and the tills of each kind of business (for its services).
    marketWares_.clear();
    marketLuxuries_.clear();
    businessTills_.clear();
    std::set<std::string> foods;
    for (const auto& row : statusTable().rows)
        foods.insert(row.food.begin(), row.food.end());
    const auto& lux = market::data().luxuries;
    const std::set<std::string> luxuries(lux.begin(), lux.end());
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
            if (!itemValid(ware) || (nourishment(ware) > 0 && foods.count(ware)))
                continue;
            if (luxuries.count(ware) || luxuries.count(items::baseOf(ware)))
                marketLuxuries_[market].push_back({till, ware});
            else if (nourishment(ware) <= 0)
                marketWares_[market].push_back({till, ware});
        }
    }
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
        if ((till->second.stock[buy.ware] -= 1) == 0 && buy.ware != "herbs" && buy.ware != "meal" && buy.ware != "sword")
            till->second.stock.erase(buy.ware);
        wallet->second.stock[buy.ware] += 1;
        noteSale(buy.till, buy.ware, 1, false);
        return true;
    }
    return false;
}

void Society::stagedShopping(const std::string& household)
{
    const auto found = stagedHomes_.find(household);
    if (found == stagedHomes_.end() || found->second.members.empty())
        return;
    const auto& h = found->second;
    const auto& row = statusTable().rows[std::size_t(std::clamp(h.band, 0, status::Bands - 1))];
    const auto day = std::to_string(stagedDay_);
    std::vector<StreetBuy> buys;
    // An everyday ware (its band's chance a day), a luxury (its chance a week), each seeded by the household and the day.
    const auto pick = [&](const std::map<std::string, std::vector<std::pair<std::string, std::string>>>& by, const char* what) {
        const auto list = by.find(h.market);
        if (list == by.end() || list->second.empty())
            return;
        const auto& [till, ware] = list->second[status::hash(household + "|" + what + "|" + day) % list->second.size()];
        buys.push_back({household, till, ware});
    };
    if (status::lot(household + "|shops|" + day) < row.shops)
        pick(marketWares_, "ware");
    if (status::lot(household + "|luxury|" + day) < row.luxury / 7)
        pick(marketLuxuries_, "luxury");
    for (const auto& b : buys)
        if (street_.watched && street_.watched(*this, b.till))
        {
            if (buysDeferred_.size() < 4096)
                buysDeferred_.push_back(b);     // (Waits for the player to go: 7.7.)
        }
        else
            streetBuy(b);
    // The services (6.5: `every` x band; a patron's only for the rich): a sale for the business, nothing on a shelf.
    const auto kinds = businessTills_.find(h.market);
    if (kinds == businessTills_.end() || row.services <= 0)
        return;
    for (const auto& s : market::data().services)
    {
        int every = s.every;
        if (const auto own = authored_.economy.services.find(s.id); own != authored_.economy.services.end())
        {
            if (!own->second.on)
                continue;
            every = std::max(1, own->second.every);
        }
        if (s.patrons && h.band < status::Rich)
            continue;
        const auto tills = kinds->second.find(s.business);
        if (tills == kinds->second.end() || tills->second.empty())
            continue;
        if (status::lot(household + "|service|" + s.id + "|" + day) < row.services / double(every))
            noteSale(tills->second[status::hash(household + "|" + s.id) % tills->second.size()], s.id, 1, false);
    }
}

void Society::streetHour(int hod)
{
    // The food and buys that waited for a player to leave a till's cell (7.7), tried again; at the shops' last hour, the
    // food still watched is fed from elsewhere with no stock moved, and the buys go undone.
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
            if (!last)
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
    stagedFoodBy(household, h->second.members, FoodBy::Runner);
    runnerWaiting_.erase(household);
    return had;
}

void Society::runnerFailed(const std::string& household)
{
    if (runnerWaiting_.erase(household) && stagedHomes_.count(household))
        foodDeferred_.insert(household);
}

// ---- Business (10.3) ----

void Society::noteSale(const std::string& till, const std::string& ware, int pieces, bool player)
{
    tillsMoved_.insert(till);                       // (Its shelves moved: journalled whole with the next record.)
    if (pieces == 0 || till.rfind("till:", 0) != 0)
        return;
    auto& b = state_.staged.business[till];
    const auto day = marketDay();
    rollBusiness(b, day);
    (player ? b.players : b.sold) = std::max(0, (player ? b.players : b.sold) + pieces);
    if (pieces > 0)
        if (const auto* shelf = account(till); shelf && !ware.empty() && stock(*shelf, ware) <= 0)
            if (const double hour = (marketClock() - std::floor(marketClock())) * 24; hour < 12)
                b.outEarly = true;                  // (Out before noon: a queue, an empty shelf by noon.)
}

void Society::businessDay(std::int64_t day)
{
    // Yesterday's word for every shop with a keeper (10.3), against its market's middle of players' buying; three days
    // failing running is a status event for the keeper's household (-0.3, 6.3).
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
    const auto& lux = market::data().luxuries;
    for (const auto& [till, keeper] : shops)
    {
        auto& b = all[till];
        const auto market = marketOf(till);
        const auto* job = jobOf(keeper);
        int target = 0, wares = 0;
        double P = 0;
        bool luxuries = false;
        if (job)
            for (const auto& ware : stagedShelfGoods(job->founder))
            {
                target += shelfTarget(till, ware);
                P += pressure(market, items::baseOf(ware));
                ++wares;
                luxuries |= std::find(lux.begin(), lux.end(), items::baseOf(ware)) != lux.end();
            }
        // (Rolled to today: the word reads the days gone, yesterday first.)
        b.word = businessWord(b, target, wares ? P / wares : 0, marketTimes(market), luxuries, medians[market]);
        b.failing = b.word == "failing" ? std::min(1000, b.failing + 1) : 0;
        if (b.failing >= 3)
            noteStatusEvent(keeper, "failing");
    }
    // (A till no keeper holds any more, or gone, is forgotten.)
    for (auto it = all.begin(); it != all.end();)
        it = std::none_of(shops.begin(), shops.end(), [&](const auto& s) { return s.first == it->first; }) ? all.erase(it) : std::next(it);
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
    if (h == stagedHomes_.end() || h->second.band > status::Poor)
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
    LadderStart s;
    s.market = market;
    s.base = items::baseOf(item);
    s.depth = depthOf(market, s.base);
    s.factor = marketBase(market, s.base, 1);
    s.each = double(catalogPrice(item));
    s.P = pressure(market, s.base);
    s.cap = marketRules().pushMost;
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

int Society::liftByHelp(const std::string& id, std::int64_t worth, int days)
{
    const auto* s = statusOf(id);
    if (!s || s->band > status::Poor || stagedDay_ < 0)
        return 0;
    const auto household = s->household;
    const auto market = s->market;
    if (days <= 0)
    {
        const auto week = bandFoodCost(id, 7), month = bandFoodCost(id, 30);
        days = month > 0 && worth >= month ? 30 : week > 0 && worth >= week ? 7 : 0;
        if (days <= 0)
            return 0;
    }
    // At most half the market's destitute share lifted at once (never fewer than one), so one rich player can't empty a
    // city of its beggars overnight.
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
    if (lifted >= room)
        return 0;
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
            if (!r || r->age < 16 || dead_.count(id) || life == state_.residents.end())
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
} // namespace

const Society::TheftDay& World::theftPlan()
{
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (theftPlanDay_ != today)
    {
        theftPlanDay_ = today;
        theftPlan_ = society_.theftDay(today, [this](const std::string& id) {
            const auto* e = entity(id);
            const auto* job = society_.jobOf(id);
            if (!e || e->dead || !e->npc || custodyOf(id) || (job && job->role == "guard") || !e->leaderId.empty() ||
                e->state == "following")
                return 0.;
            return 1.;
        });
    }
    return theftPlan_;
}

bool World::tillWatched(const std::string& till) const
{
    if (till.rfind("till:", 0) != 0)
        return false;
    if (watchedAt_ != time_)
    {
        watchedCells_ = stageCells();
        watchedAt_ = time_;
    }
    if (watchedCells_.empty())
        return false;
    const auto* p = society_.position(till.substr(5));
    if (!p)
        return false;
    if (watchedCells_.count(p->work.cell))
        return true;
    // (A keeper at a stall on the square sells from it: its cell is the shop's then.)
    const auto held = society_.state().careers.positions.find(p->id);
    if (held != society_.state().careers.positions.end())
        if (const auto* keeper = entity(held->second.holder); keeper && watchedCells_.count(keeper->cellId))
            return true;
    return false;
}

bool World::sendRunner(const std::string& household, const std::string& till)
{
    // A household's errand-runner (6.6): a member free to go (no post at its hours first), to the counter of the shop while
    // it is open, while that cell has fewer than RunnersACell and its market fewer than RunnersAMarket.
    const auto* members = society_.membersOf(household);
    const auto* p = till.rfind("till:", 0) == 0 ? society_.position(till.substr(5)) : nullptr;
    if (!members || !p || !tillWatched(till))
        return false;
    Spot at = p->serve.cell.empty() ? p->work : p->serve;
    const auto held = society_.state().careers.positions.find(p->id);
    // (Only to an open shop: its keeper keeping it. A shut one's shelf waits, as its food does, for the player to go.)
    const auto* keeping = held == society_.state().careers.positions.end() ? nullptr : society_.resident(held->second.holder);
    if (!keeping || keeping->task != "trade")
        return false;
    if (held != society_.state().careers.positions.end())
        if (const auto* keeper = entity(held->second.holder); keeper && !keeper->dead && keeper->cellId != p->work.cell &&
                                                            society_.atStall(held->second.holder))
            at = {keeper->cellId, keeper->position.x + 1, keeper->position.y};   // (Its stall: the keeper's side of it.)
    const auto market = society_.marketOf(till);
    int inCell = 0, inMarket = 0;
    for (const auto& [id, r] : runners_)
    {
        inCell += r.at.cell == at.cell;
        inMarket += society_.marketOf(r.till) == market;
    }
    if (inCell >= RunnersACell || inMarket >= RunnersAMarket)
        return false;
    for (const bool posted : {false, true})
        for (const auto& id : *members)
        {
            const auto* e = entity(id);
            const auto* life = society_.resident(id);
            const auto* job = society_.jobOf(id);
            if (!e || !life || e->dead || !e->npc || e->age < 10 || !e->leaderId.empty() || e->state == "following" || custodyOf(id) ||
                drivers_.count(id) || seated_.count(id) || runners_.count(id) || beggars_.count(id) || inBattle(id) ||
                bool(job) != posted)
                continue;
            if (life->task == "sleep" || life->task == "eat" || life->task == "companion" ||
                (job && (life->task == job->title || life->task == "trade" || life->task == "patrol" || life->task == "watch" ||
                         life->task == "preaching")))
                continue;
            runners_[id] = {household, till, at, time_, -1};
            return true;
        }
    return false;
}

const Spot* World::chapelSteps(const std::string& community)
{
    // Outside the chapel's door (10.2): where its first door to the open air comes out, else the chapel itself.
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (chapelStepsDay_ != today)
    {
        chapelSteps_.clear();
        chapelStepsDay_ = today;
    }
    if (const auto found = chapelSteps_.find(community); found != chapelSteps_.end())
        return found->second.cell.empty() ? nullptr : &found->second;
    auto& steps = chapelSteps_[community];
    const auto& ch = chapel(community);
    if (!ch.found)
        return nullptr;
    for (const auto& [id, d] : doors_)
        if (d.cellId == ch.pulpit.cell && !d.targetCell.empty())
            if (const auto* out = cell(d.targetCell); out && out->outdoors)
            {
                steps = {d.targetCell, d.arrival.x, d.arrival.y};
                break;
            }
    if (steps.cell.empty())
        steps = ch.pews.empty() ? ch.pulpit : ch.pews.back();
    return &steps;
}

void World::placeBeggars()
{
    // Marketday and Restday, 8:00 to 18:00 (10.2): destitute adults without a post, not lifted, at their market's square
    // (by its board) and its chapel's steps, at most beggarsACell a cell, in the order of their ids.
    beggars_.clear();
    beggarWhere_.clear();
    const int weekday = calendar::weekdayOf(calendarDays_);
    const double hour = (calendarDays_ - std::floor(calendarDays_)) * 24;
    if ((weekday != calendar::Marketday && weekday != calendar::Restday) || hour < 8 || hour >= 18)
        return;
    const auto& table = society_.statusTable();
    std::map<std::string, std::vector<std::string>> byMarket;
    for (const auto& [id, life] : society_.state().residents)
    {
        const auto* s = society_.statusOf(id);
        const auto* e = entity(id);
        const auto* r = society_.spec(id);
        if (!s || s->band != status::Destitute || society_.liftedNow(id) || !e || e->dead || !e->npc || !r || r->age < 16 ||
            society_.jobOf(id) || society_.apprenticedTo(id) || !e->leaderId.empty() || e->state == "following" || custodyOf(id) ||
            drivers_.count(id) || seated_.count(id))
            continue;
        byMarket[s->market].push_back(id);
    }
    for (const auto& [market, list] : byMarket)
    {
        const auto* t = roadMarket(market);
        const auto community = t && !t->market.empty() ? lawTown(t->market) : market;
        std::vector<std::pair<Spot, std::string>> spots;
        if (const auto* steps = chapelSteps(community))
            spots.push_back({*steps, "at the chapel steps"});
        if (const auto* board = boardSpot(community))
            spots.push_back({*board, "at the market"});
        else if (t && !t->market.empty())
            spots.push_back({{t->market, t->marketX + 2, t->marketY + 1}, "at the market"});
        std::map<std::string, int> inCell;
        std::size_t next = 0;
        for (const auto& [spot, where] : spots)
            for (int k = 0; next < list.size() && inCell[spot.cell] < table.beggarsACell; ++k)
            {
                Spot at = spot;
                at.x += k % 2 ? 1 : 0;
                at.y += k / 2;
                if (!passable(at.cell, {at.x, at.y}))
                    at = spot;
                beggars_[list[next]] = at;
                beggarWhere_[list[next]] = where;
                ++inCell[spot.cell];
                ++next;
            }
    }
}

void World::placeSleepers()
{
    // Some winter nights (D12: a third of them, by a seeded lot), the destitute with nowhere of their own (no home, or a
    // communal one) sleep by a warm wall in their market: a bakehouse's or a forge's.
    warmWall_.clear();
    const double hour = (calendarDays_ - std::floor(calendarDays_)) * 24;
    if (calendar::calendarAt(calendarDays_).season != calendar::Season::Winter || (hour >= 6 && hour < 22))
        return;
    const auto night = std::int64_t(std::floor(calendarDays_ - .25));
    warmPlaces_.clear();
    // (A bakehouse's or a forge's wall, indoors or by its fire in the open; a market with none has its chapel's walls.)
    for (const auto& p : society_.positions())
        if (!p.work.cell.empty() && (warmTrade(p.title) || Society::clergy(p.title)) && cell(p.work.cell))
            if (const auto* m = roadMarketOf(p.work.cell); m && warmPlaces_[m->id].size() < 16)
            {
                auto& places = warmPlaces_[m->id];
                if (Society::clergy(p.title) && !places.empty())
                    continue;
                places.push_back({p.work.cell, p.work.x + 1, p.work.y + 1});
            }
    for (const auto& [id, life] : society_.state().residents)
    {
        const auto* s = society_.statusOf(id);
        const auto* e = entity(id);
        if (!s || s->band != status::Destitute || society_.liftedNow(id) || !e || e->dead || !e->npc || !e->leaderId.empty() ||
            custodyOf(id) || (!life.homeCell.empty() && !Society::communalHome(life.homeCell)))
            continue;
        if (status::lot(id + "|warm wall|" + std::to_string(night)) >= 1. / 3)
            continue;
        const auto places = warmPlaces_.find(s->market);
        if (places == warmPlaces_.end() || places->second.empty())
            continue;
        auto at = places->second[status::hash(id + "|warm") % places->second.size()];
        if (!passable(at.cell, {at.x, at.y}))
            at.x -= 1, at.y -= 1;
        warmWall_[id] = at;
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

void World::tendStreet()
{
    const auto today = std::int64_t(std::floor(calendarDays_));
    // Runners (6.6): at the counter for its seconds, then the household's food from the shelf; one an hour on the way
    // gives up (its food waits for the next hour).
    for (auto it = runners_.begin(); it != runners_.end();)
    {
        auto& r = it->second;
        const auto* e = entity(it->first);
        if (!e || e->dead || !society_.runnerWaiting(r.household))
        {
            it = runners_.erase(it);
            continue;
        }
        if (time_ - r.since > calendar::SecondsPerDay / 24 || !e->leaderId.empty() || custodyOf(it->first))
        {
            society_.runnerFailed(r.household);
            it = runners_.erase(it);
            continue;
        }
        if (e->cellId == r.at.cell && apart(e->position, {r.at.x, r.at.y}) <= 1.6)
        {
            if (r.atCounter < 0)
                r.atCounter = time_;
            else if (time_ - r.atCounter >= RunnerSeconds)
            {
                society_.runnerBought(r.household);
                it = runners_.erase(it);
                continue;
            }
        }
        ++it;
    }
    // Each hour: the beggars' and the sleepers' places, and the Restday dole after the service.
    if (const auto hour = std::int64_t(std::floor(calendarDays_ * 24)); hour != streetHour_)
    {
        streetHour_ = hour;
        placeBeggars();
        placeSleepers();
        dolePriests_.clear();
        const double hod = double(hour % 24);
        if (calendar::weekdayOf(calendarDays_) == calendar::Restday && hod >= Society::ServiceEnd && hod < Society::ServiceEnd + 2)
        {
            // The dole (10.2): the chapel's priest, a real post held, hands bread to the destitute and poor households of
            // its community: a raw add into their wallets, once a Restday, never to a player.
            std::set<std::string> served;
            for (const auto& p : society_.positions())
            {
                if (!Society::clergy(p.title))
                    continue;
                const auto community = lawTown(p.work.cell);
                const auto held = society_.state().careers.positions.find(p.id);
                const auto* priest = held == society_.state().careers.positions.end() ? nullptr : entity(held->second.holder);
                if (community.empty() || served.count(community) || !priest || priest->dead || !chapel(community).found)
                    continue;
                served.insert(community);
                const auto* steps = chapelSteps(community);
                dolePriests_[held->second.holder] = steps ? *steps : p.work;
                if (!dolesGiven_.insert(community + "|" + std::to_string(today)).second)
                    continue;
                std::set<std::string> households;
                for (const auto& [id, life] : society_.state().residents)
                    if (const auto* s = society_.statusOf(id); s && s->band <= status::Poor)
                    {
                        const auto* e = entity(id);
                        const auto home = !life.homeCell.empty() ? life.homeCell : e ? e->cellId : std::string();
                        if (!home.empty() && lawTown(home) == community)
                            households.insert(s->household);
                    }
                for (const auto& h : households)
                    society_.giveDole(h, today);
            }
            if (dolesGiven_.size() > 1024)
                dolesGiven_.clear();
        }
    }
    // Alms (10.2), each game minute: a comfortable or rich resident close by a beggar gives a penny, by its band's chance,
    // once a day a pair (a staged move, between residents only).
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
            if (!b || !bs || b->cellId != spot.cell || apart(b->position, {spot.x, spot.y}) > 2)
                continue;
            for (const auto& [id, e] : entities_)
            {
                if (!e.npc || e.dead || id == beggar || e.cellId != b->cellId || apart(e.position, b->position) > 3)
                    continue;
                const auto* gs = society_.statusOf(id);
                if (!gs || gs->band < status::Comfortable || gs->household == bs->household || almsGiven_.size() > 65536)
                    continue;
                const auto key = id + "|" + beggar;
                if (!almsGiven_.insert(key).second)
                    continue;
                if (status::lot(key + "|alms|" + std::to_string(today)) < rows[std::size_t(gs->band)].alms && society_.cash(id) >= 1)
                    society_.shift(id, beggar, "", 0, 1, "alms");
            }
        }
    }
}
} // namespace ratw
