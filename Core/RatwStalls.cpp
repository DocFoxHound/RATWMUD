// Market stalls (Docs/Design/54-gathering-places.md, 3; Phase 4). On Marketday from 7 to 2, in fair weather, a wolf
// standing at a free stall spot on a city's square (World::stallSpots) may rent it for the morning: 3p to the town's
// treasury, at most 6 a square (half its spots, in a square with fewer than 12), one a wolf. A spot let to a player is
// left out of the merchants' plan that day (World::setLetStalls), so they set up at the others (two may share, as
// before). Wares go from the purse into the stall (`stall:<id>`), so nothing listed can be sold elsewhere, given or
// eaten until it is taken off; each has a price apiece (1 to 999p) the keeper may change at any time. A buyer within 2
// tiles buys only while the keeper stands within 3 tiles of the stall and has stirred in the last 10 minutes: face to
// face. Goods and coins move at once (Society::shift, "stall sale", with the 64 kinds a purse may hold). At 2 the stall
// clears and its goods go home; foul weather clears it too, and the fee comes back.
#include "RatwGame.h"

#include "RatwItems.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr std::int64_t StallFee = 3, PriceMost = 999;
constexpr int SquareMost = 6, WareKinds = 10;
constexpr double RentReach = 1.5, BuyReach = 2, KeeperReach = 3, KeeperQuietSeconds = 600;

double hourOf(double days) { return (days - std::floor(days)) * 24; }
template <class S> std::string account(const S& st) { return "stall:" + st.id; }   // (A stall's goods.)
} // namespace

bool Game::marketOpen(const std::string& community)
{
    if (community.empty())
        return false;
    const auto plan = world_.dayPlan(community);
    const double hour = hourOf(world_.calendarDays());
    return plan.kind == "market" && !plan.foul && hour >= 7 && hour < 14;
}

bool Game::keeperPresent(const Stall& st)
{
    const auto* k = world_.entity(st.keeper);
    if (!k || k->dead || k->cellId != st.cell || !clientOf(st.keeper) ||
        std::hypot(k->position.x - st.x, k->position.y - st.y) > KeeperReach)
        return false;
    const auto at = lastActiveReal_.find(st.keeper);
    return at != lastActiveReal_.end() && now() - at->second <= KeeperQuietSeconds;
}

namespace
{
// The stall spot on this community's square within reach of the wolf, if any.
const Spot* spotNear(World& world, const std::string& community, const Entity& e)
{
    for (const auto& s : world.stallSpots(community))
        if (s.cell == e.cellId && std::hypot(s.x - e.position.x, s.y - e.position.y) <= RentReach)
            return &s;
    return nullptr;
}
int squareMost(World& world, const std::string& community)
{
    const int spots = int(world.stallSpots(community).size());
    return std::min(SquareMost, std::max(1, spots / 2));
}
} // namespace

void Game::clearStall(Stall& st, const std::string& why, bool refund)
{
    // Its goods home (what won't fit waits in the stall until there is room), and the fee back in foul weather.
    auto& society = world_.society();
    bool waiting = false;
    if (const auto* a = society.account(account(st)))
    {
        const auto held = a->stock;
        for (const auto& [item, n] : held)
            if (n > 0 && !society.shift(account(st), st.keeper, item, n, 0, "taken off a stall"))
                waiting = true;
    }
    if (refund && st.fee > 0)
    {
        const auto treasury = society.treasuryOf(st.community);
        society.shift(treasury, st.keeper, "", 0, std::min(st.fee, society.spendable(treasury)), "stall fee returned");
    }
    if (!st.closed)
        world_.recordEvent({"stall closed", st.keeper, {}, st.cell, 0, 0, {}, 0, st.takings, st.id});
    st.closed = true;
    st.prices.clear();
    if (auto* c = clientOf(st.keeper); c && !why.empty())
        system(c, why + (waiting ? " What won't fit in your pack waits at the stall until there is room." : ""));
    record(Economy | Character, st.keeper);
}

void Game::residentsAtStalls()
{
    // Grown residents of the stall's own town, within 6 tiles of it, while its keeper is there: each looks over the wares
    // once a market day, and buys the first it wants (a treat, a household need, finery, care, a pastime, something for
    // the home) at no more than the town's price and within a tenth of what it has beyond a week's food. Three buyers a
    // stall an hour at most; about one in three who look buy.
    auto& society = world_.society();
    const double today = std::floor(world_.calendarDays());
    const auto& residents = society.state().residents;
    const auto wanted = [](const std::string& item) {
        const auto base = items::baseOf(item);
        const auto* g = items::good(base);
        if (!g)
            return false;
        if (Society::edible(base))
            return g->price >= 3;                   // (A treat: bread and porridge come from the shops.)
        for (const auto& need : items::householdNeeds())
            if (std::find(need.any.begin(), need.any.end(), base) != need.any.end())
                return true;
        const auto& c = g->category;
        return c == "jewelry" || c == "hat" || c == "scarf" || c == "shawl" || c == "paw_wear" || c == "care" || c == "luxury" ||
               c == "instrument" || c == "household";
    };
    for (auto& st : stalls_)
    {
        if (st.closed || st.prices.empty() || !keeperPresent(st))
            continue;
        int buyers = 0;
        for (const auto* e : world_.entitiesIn(st.cell))
        {
            if (buyers >= 3)
                break;
            if (!e || !e->npc || e->dead || e->age < 16 || std::hypot(e->position.x - st.x, e->position.y - st.y) > 6)
                continue;
            const auto life = residents.find(e->id);
            const auto key = e->id + "|" + std::to_string(std::int64_t(today));
            if (life == residents.end() || world_.lawTown(life->second.homeCell) != st.community || stallBuyers_.count(key))
                continue;
            stallBuyers_.insert(key);
            if (double(std::hash<std::string>{}(key + "|stall") % 1000) / 1000 >= .35)
                continue;                           // (Looked, and walked on.)
            const auto* purse = society.account(e->id);
            const auto budget = purse ? (purse->cash - Society::FoodADay * 7) / 10 : 0;
            const auto* wares = society.account(account(st));
            for (const auto& [item, price] : st.prices)
            {
                if (!wares || Society::stock(*wares, item) <= 0 || !wanted(item) || price > budget ||
                    double(price) > std::floor(society.townPrice(st.community, item) + 1e-9))
                    continue;
                if (!purse->stock.count(item) && purse->stock.size() >= MaxGoodsKinds)
                    continue;
                if (!society.shift(account(st), e->id, item, 1, 0, "stall sale"))
                    continue;
                if (!society.shift(e->id, st.keeper, "", 0, price, "stall sale"))
                {
                    society.shift(e->id, account(st), item, 1, 0, "stall sale undone");
                    continue;
                }
                if (!Society::edible(items::baseOf(item)))
                    society.consume(e->id, item, 1, "used at home");   // (Food is eaten as it eats what it carries.)
                moveScents(st.keeper, e->id, item, 1, false);
                st.takings += price;
                ++buyers;
                world_.recordEvent({"stall sale", e->id, st.keeper, st.cell, 0, 0, item, 1, price, st.id});
                record(Economy | Character, st.keeper);
                if (auto* k = clientOf(st.keeper))
                    system(k, names::capitalised(labelFor(st.keeper, e->id)) + " buys " + goodsWords(item, 1, 0) + " from your stall for " +
                                  std::to_string(price) + "p.");
                break;
            }
        }
        if (const auto* left = society.account(account(st)))
            for (auto it = st.prices.begin(); it != st.prices.end();)
                it = Society::stock(*left, it->first) <= 0 ? st.prices.erase(it) : std::next(it);
    }
}

void Game::tendStalls()
{
    if (const double hour = std::floor(world_.calendarDays() * 24); hour != stallBuyersHour_)
    {
        stallBuyersHour_ = hour;
        if (stallBuyers_.size() > 20000)
            stallBuyers_.clear();
        residentsAtStalls();
    }
    if (stalls_.empty())
        return;
    const double today = std::floor(world_.calendarDays());
    bool changed = false;
    for (auto& st : stalls_)
    {
        if (st.closed)
            continue;
        if (st.day != today || !marketOpen(st.community))
        {
            const bool foul = st.day == today && world_.dayPlan(st.community).foul;
            clearStall(st, foul ? "The weather is too foul for the market: you pack up your stall, and the town returns the fee."
                                : "The market is over: you pack up your stall.",
                       foul);
            changed = true;
        }
    }
    // Cleared stalls whose goods have gone home are done with.
    auto& society = world_.society();
    for (auto it = stalls_.begin(); it != stalls_.end();)
    {
        if (!it->closed)
        {
            ++it;
            continue;
        }
        bool empty = true;
        if (const auto* a = society.account(account(*it)))
        {
            const auto held = a->stock;
            for (const auto& [item, n] : held)
                if (n > 0 && !society.shift(account(*it), it->keeper, item, n, 0, "taken off a stall"))
                    empty = false;
        }
        if (empty || !world_.entity(it->keeper))
        {
            society.closeAccount(account(*it));
            it = stalls_.erase(it);
            changed = true;
        }
        else
            ++it;
    }
    if (changed)
    {
        std::set<std::string> keys;
        for (const auto& st : stalls_)
            if (!st.closed)
                keys.insert(World::stallKey({st.cell, st.x, st.y}));
        world_.setLetStalls(std::move(keys));
    }
}

bool Game::stallCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"verb": "rent"} / "list" {item, quantity, price} / "unlist" {item, quantity} / "price" {item, price} / "close" /
    // "buy" {id, item, quantity}.
    const auto me = c->entityId;
    const auto verb = j.string("verb");
    auto* e = world_.entity(me);
    if (!e)
        return result = {false, "No such character.", {}}, true;
    auto& society = world_.society();
    const auto mine = std::find_if(stalls_.begin(), stalls_.end(), [&](const Stall& s) { return s.keeper == me && !s.closed; });
    if (verb == "rent")
    {
        const auto community = world_.lawTown(e->cellId);
        const auto* spot = community.empty() ? nullptr : spotNear(world_, community, *e);
        if (!spot)
            return result = {false, "Stand at a stall on a city's market square.", {}}, true;
        if (!marketOpen(community))
            return result = {false, world_.dayPlan(community).foul && world_.dayPlan(community).kind == "market"
                                        ? "No stalls today: the weather is too foul."
                                        : "Stalls are let on Marketday mornings, from 7 to 2.",
                             {}},
                   true;
        if (mine != stalls_.end())
            return result = {false, "You have a stall already.", {}}, true;
        const auto key = World::stallKey(*spot);
        int let = 0;
        for (const auto& st : stalls_)
            if (!st.closed && st.community == community)
            {
                ++let;
                if (World::stallKey({st.cell, st.x, st.y}) == key)
                    return result = {false, "Someone has this stall today.", {}}, true;
            }
        if (let >= squareMost(world_, community))
            return result = {false, "The town has let all the stalls it lets today.", {}}, true;
        const auto treasury = society.treasuryOf(community);
        if (society.spendable(me) < StallFee || !society.shift(me, treasury, "", 0, StallFee, "stall rent"))
            return result = {false, "A stall costs " + std::to_string(StallFee) + "p.", {}}, true;
        Stall st;
        st.id = "st" + std::to_string(nextStall_++);
        st.keeper = me;
        st.community = community;
        st.cell = spot->cell;
        st.x = spot->x;
        st.y = spot->y;
        st.day = std::floor(world_.calendarDays());
        st.fee = StallFee;
        society.openAccount(account(st));
        stalls_.push_back(st);
        std::set<std::string> keys;
        for (const auto& s : stalls_)
            if (!s.closed)
                keys.insert(World::stallKey({s.cell, s.x, s.y}));
        world_.setLetStalls(std::move(keys));
        world_.recordEvent({"stall rented", me, {}, st.cell, 0, 0, {}, 0, StallFee, st.id});
        record(Economy | Character, me);
        return result = {true, "You rent the stall for the morning (" + std::to_string(StallFee) + "p to the town). Lay out your wares.", {}}, true;
    }
    if (verb == "buy")
    {
        const auto id = j.string("id"), item = j.string("item");
        const int quantity = std::clamp(int(j.number("quantity", 1)), 1, 99);
        const auto st = std::find_if(stalls_.begin(), stalls_.end(), [&](const Stall& s) { return s.id == id && !s.closed; });
        if (st == stalls_.end() || e->cellId != st->cell || std::hypot(e->position.x - st->x, e->position.y - st->y) > BuyReach)
            return result = {false, "Go up to the stall first.", {}}, true;
        if (st->keeper == me)
            return result = {false, "It's your own stall.", {}}, true;
        if (!keeperPresent(*st))
            return result = {false, "The keeper isn't at the stall; wait for them.", {}}, true;
        const auto* a = society.account(account(*st));
        const auto price = st->prices.find(item);
        if (!a || price == st->prices.end() || Society::stock(*a, item) < quantity)
            return result = {false, "The stall hasn't that many.", {}}, true;
        const auto cost = price->second * quantity;
        if (society.spendable(me) < cost)
            return result = {false, "You haven't " + std::to_string(cost) + "p.", {}}, true;
        if (const auto* purse = society.account(me); purse && !purse->stock.count(item) && purse->stock.size() >= MaxGoodsKinds)
            return result = {false, "You can't carry another kind of thing.", {}}, true;
        if (!society.shift(account(*st), me, item, quantity, 0, "stall sale"))
            return result = {false, "It can't be bought now.", {}}, true;
        if (!society.shift(me, st->keeper, "", 0, cost, "stall sale"))
        {
            society.shift(me, account(*st), item, quantity, 0, "stall sale undone");
            return result = {false, "It can't be bought now.", {}}, true;
        }
        moveScents(st->keeper, me, item, quantity, false);
        st->takings += cost;
        if (const auto* left = society.account(account(*st)); left && Society::stock(*left, item) <= 0)
            st->prices.erase(item);
        const auto what = goodsWords(item, quantity, 0);
        world_.recordEvent({"stall sale", me, st->keeper, st->cell, 0, 0, item, quantity, cost, st->id});
        record(Economy | Character, me);
        record(Economy | Character, st->keeper);
        if (auto* k = clientOf(st->keeper))
            system(k, names::capitalised(labelFor(st->keeper, me)) + " buys " + what + " from your stall for " + std::to_string(cost) + "p.");
        return result = {true, "You buy " + what + " for " + std::to_string(cost) + "p.", st->keeper}, true;
    }
    // The keeper's own verbs.
    if (mine == stalls_.end())
        return result = {false, "You have no stall today.", {}}, true;
    auto& st = *mine;
    if (verb == "close")
    {
        clearStall(st, "You pack up your stall.", false);
        tendStalls();
        return result = {true, "", {}}, true;
    }
    if (e->cellId != st.cell || std::hypot(e->position.x - st.x, e->position.y - st.y) > KeeperReach)
        return result = {false, "Go back to your stall.", {}}, true;
    const auto item = j.string("item");
    if (verb == "list")
    {
        const int quantity = std::clamp(int(j.number("quantity", 1)), 1, 99);
        const auto price = std::int64_t(j.number("price", 0));
        if (price < 1 || price > PriceMost)
            return result = {false, "A price from 1p to " + std::to_string(PriceMost) + "p.", {}}, true;
        if (!items::good(items::baseOf(item)) || spareOf(me, item) < quantity)
            return result = {false, "You haven't that to sell (what you wear, and what is lent to you, stays off the stall).", {}}, true;
        const auto* a = society.account(account(st));
        if (a && !a->stock.count(item) && int(a->stock.size()) >= WareKinds)
            return result = {false, "Ten kinds of ware at most.", {}}, true;
        if (!society.shift(me, account(st), item, quantity, 0, "laid on a stall"))
            return result = {false, "It won't go on the stall.", {}}, true;
        st.prices[item] = price;
        record(Economy | Character, me);
        return result = {true, "You lay out " + goodsWords(item, quantity, 0) + " at " + std::to_string(price) + "p apiece.", {}}, true;
    }
    if (verb == "unlist")
    {
        const auto* a = society.account(account(st));
        const int held = a ? Society::stock(*a, item) : 0;
        const int quantity = std::clamp(int(j.number("quantity", held)), 1, std::max(1, held));
        if (held <= 0 || !society.shift(account(st), me, item, quantity, 0, "taken off a stall"))
            return result = {false, "That isn't on your stall, or you have no room for it.", {}}, true;
        if (quantity >= held)
            st.prices.erase(item);
        record(Economy | Character, me);
        return result = {true, "You take " + goodsWords(item, quantity, 0) + " off the stall.", {}}, true;
    }
    if (verb == "price")
    {
        const auto price = std::int64_t(j.number("price", 0));
        const auto found = st.prices.find(item);
        if (found == st.prices.end())
            return result = {false, "That isn't on your stall.", {}}, true;
        if (price < 1 || price > PriceMost)
            return result = {false, "A price from 1p to " + std::to_string(PriceMost) + "p.", {}}, true;
        found->second = price;
        return result = {true, "Now " + std::to_string(price) + "p apiece.", {}}, true;
    }
    return result = {false, "That isn't something a stall does.", {}}, true;
}

json::Value Game::stallsView(const std::string& cellId)
{
    auto list = Value::array();
    for (const auto& st : stalls_)
        if (!st.closed && st.cell == cellId)
        {
            auto o = Value::object();
            o.add("id", st.id);
            o.add("x", st.x);
            o.add("y", st.y);
            o.add("wares", int(st.prices.size()));
            list.push(o);
        }
    return list;
}

json::Value Game::stallSelf(const std::string& viewer)
{
    // One's own stall; a stall within reach (its wares, prices and scent); or the offer of the spot one stands at.
    const auto* e = world_.entity(viewer);
    if (!e)
        return {};
    auto& society = world_.society();
    const auto wares = [&](const Stall& st, bool scent) {
        auto list = Value::array();
        const auto* a = society.account(account(st));
        const auto* purse = society.account(st.keeper);
        for (const auto& [item, price] : st.prices)
        {
            const int n = a ? Society::stock(*a, item) : 0;
            if (n <= 0)
                continue;
            auto o = Value::object();
            o.add("id", item);
            o.add("name", Society::itemName(item));
            o.add("quantity", n);
            o.add("price", double(price));
            if (scent)
                if (auto line = scentOfItem(viewer, st.keeper, item, n + (purse ? Society::stock(*purse, item) : 0)); !line.empty())
                    o.add("scent", line);
            list.push(o);
        }
        return list;
    };
    auto out = Value::object();
    bool any = false;
    for (const auto& st : stalls_)
    {
        if (st.closed || st.cell != e->cellId)
            continue;
        const double d = std::hypot(e->position.x - st.x, e->position.y - st.y);
        if (st.keeper == viewer)
        {
            auto o = Value::object();
            o.add("id", st.id);
            o.add("here", d <= KeeperReach);
            o.add("takings", double(st.takings));
            o.add("wares", wares(st, false));
            out.add("mine", o);
            any = true;
        }
        else if (d <= BuyReach && !out.find("here"))
        {
            auto o = Value::object();
            o.add("id", st.id);
            o.add("keeper", names::capitalised(labelFor(viewer, st.keeper)));
            o.add("present", keeperPresent(st));
            o.add("wares", wares(st, true));
            out.add("here", o);
            any = true;
        }
    }
    // The spot one stands at, if it can be let now.
    if (!out.find("mine") && std::none_of(stalls_.begin(), stalls_.end(), [&](const Stall& s) { return s.keeper == viewer && !s.closed; }))
        if (const auto community = world_.lawTown(e->cellId); !community.empty())
            if (const auto* spot = spotNear(world_, community, *e))
            {
                const auto key = World::stallKey(*spot);
                int let = 0;
                bool taken = false;
                for (const auto& st : stalls_)
                    if (!st.closed && st.community == community)
                        ++let, taken |= World::stallKey({st.cell, st.x, st.y}) == key;
                if (!taken && let < squareMost(world_, community) && marketOpen(community))
                {
                    auto o = Value::object();
                    o.add("fee", double(StallFee));
                    out.add("offer", o);
                    any = true;
                }
            }
    return any ? out : Value();
}

void Game::stallsSave(json::Value& root) const
{
    auto list = Value::array();
    for (const auto& st : stalls_)
    {
        auto o = Value::object();
        o.add("id", st.id);
        o.add("keeper", st.keeper);
        o.add("community", st.community);
        o.add("cell", st.cell);
        o.add("x", st.x);
        o.add("y", st.y);
        o.add("day", st.day);
        o.add("fee", double(st.fee));
        o.add("takings", double(st.takings));
        o.add("closed", st.closed);
        auto prices = Value::object();
        for (const auto& [item, price] : st.prices)
            prices.add(item, double(price));
        o.add("prices", prices);
        list.push(o);
    }
    root.add("stalls", list);
}

void Game::stallsLoad(const json::Value& saved)
{
    stalls_.clear();
    for (const auto& o : saved.array("stalls"))
    {
        Stall st;
        st.id = o.string("id");
        st.keeper = o.string("keeper");
        st.community = o.string("community");
        st.cell = o.string("cell");
        st.x = o.number("x");
        st.y = o.number("y");
        st.day = o.number("day");
        st.fee = std::int64_t(o.number("fee"));
        st.takings = std::int64_t(o.number("takings"));
        st.closed = o.boolean("closed");
        if (const auto* prices = o.find("prices"); prices && prices->isObject())
            for (const auto& [item, price] : prices->fields())
                if (price.isNumber())
                    st.prices[item] = std::int64_t(price.asNumber());
        if (st.id.empty() || st.keeper.empty())
            continue;
        if (st.id.size() > 2)
            nextStall_ = std::max<std::uint64_t>(nextStall_, std::strtoull(st.id.c_str() + 2, nullptr, 10) + 1);
        stalls_.push_back(std::move(st));
    }
    std::set<std::string> keys;
    for (const auto& st : stalls_)
        if (!st.closed)
            keys.insert(World::stallKey({st.cell, st.x, st.y}));
    world_.setLetStalls(std::move(keys));
}
} // namespace ratw::game
