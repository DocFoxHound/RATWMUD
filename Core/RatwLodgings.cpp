// Renting by individuals, and venues (Docs/Design/54-gathering-places.md, 4; Phase 3). Chapters keep everything doc 32
// gave them (RatwGameEstates.cpp); here any wolf may take:
// - a bed upstairs at an inn ("<inn>, upstairs", whose landlord is its keeper): a night (to noon) 2p, a week 10p, to
//   the inn's till; the bed nearest it that nobody has;
// - a lodger's bed: a spare bed in a resident's home (one the household doesn't sleep on), a week 6p to the head's own
//   purse, from a head who doesn't dislike the wolf (liking and trust 0 or more): letting is income for the resident;
// - the inn's whole upstairs for a night (to 06:00) 8p, not while a bed there is let (nor the other way round);
// - a place a DM or Atlas listed for individuals (estate::Property::individuals): a night or a week, as set.
// Never a seat of power, a church, a guardhouse or where a resident lives or works: nothing else is offered. One
// lodging a wolf. Each has a small chest (`let:<id>`, 10 kinds and 20 lb). Held for a story (estate.hold), new lodgings
// are refused and running ones get a week's notice, then end with the rest refunded from the landlord (owed if it
// can't). A renter of a whole place may open its doors for the night (anyone may come in) and have 3 guests.
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
constexpr std::int64_t BedNight = 2, BedWeek = 10, LodgerWeek = 6, UpstairsNight = 8;
constexpr int ChestKinds = 10, GuestsMost = 3;
constexpr double ChestPounds = 20, NoticeDays = 7;

// The next noon (a bed's night) or the next 06:00 (a whole night's), in calendar days.
double nextHour(double now, double hour)
{
    const double day = std::floor(now), at = day + hour / 24;
    return now < at ? at : at + 1;
}
} // namespace

const estate::Property* Game::innUpstairs(const std::string& cell) const
{
    const auto* p = estates_.property(cell);
    if (!p || p->kind != "hall")
        return nullptr;
    const auto comma = p->name.rfind(", upstairs");
    return comma != std::string::npos && comma + 10 == p->name.size() && p->landlord != "treasury" ? p : nullptr;
}

const Game::Lodging* Game::lodgingOf(const std::string& holder) const
{
    for (const auto& l : lodgings_)
        if (l.holder == holder)
            return &l;
    return nullptr;
}

const Game::Lodging* Game::wholeLodgingAt(const std::string& cell) const
{
    for (const auto& l : lodgings_)
        if (l.cell == cell && (l.kind == "night" || l.kind == "place"))
            return &l;
    return nullptr;
}

bool Game::lodgeCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"verb": "bed", "period": "night"|"week"} / "night" / "place", "period" / "ask", "target" (a resident) / "open" /
    // "close" / "guest", "target", "on" / "end" / "put" | "take", "item", "quantity".
    const auto me = c->entityId;
    const auto verb = j.string("verb");
    auto* e = world_.entity(me);
    auto& society = world_.society();
    if (!e)
        return result = {false, "No such character.", {}}, true;
    const double now = world_.calendarDays();
    const auto mine = std::find_if(lodgings_.begin(), lodgings_.end(), [&](const Lodging& l) { return l.holder == me; });
    // Taking one.
    if (verb == "bed" || verb == "night" || verb == "place" || verb == "ask")
    {
        if (mine != lodgings_.end())
            return result = {false, "You have a lodging already; give it up first.", {}}, true;
        std::string cell = e->cellId, kind, landlord, account, period = j.string("period", "night");
        int x = -1, y = -1;
        std::int64_t rent = 0;
        if (verb == "ask")
        {
            // A lodger's bed in a resident's home.
            const auto target = j.string("target");
            const auto* r = world_.entity(target);
            const auto* life = world_.society().resident(target);
            if (!r || !r->npc || !life || life->homeCell.empty())
                return result = {false, "Ask whom?", target}, true;
            if (std::hypot(r->position.x - e->position.x, r->position.y - e->position.y) > 3 || r->cellId != e->cellId)
                return result = {false, "Go to them to ask.", target}, true;
            const auto* bond = world_.bonds().find(target, me);
            if (bond && (bond->affinity < 0 || bond->trust < 0))
                return result = {false, names::capitalised(labelFor(me, target)) + " won't have you under their roof.", target}, true;
            cell = life->homeCell;
            std::vector<std::pair<int, int>> free;
            for (const auto& b : world_.spareBeds(cell))
                if (std::none_of(lodgings_.begin(), lodgings_.end(), [&](const Lodging& l) { return l.cell == cell && l.x == b.first && l.y == b.second; }))
                    free.push_back(b);
            if (free.empty())
                return result = {false, names::capitalised(labelFor(me, target)) + " has no bed to spare.", target}, true;
            kind = "lodger", landlord = target, account = target, period = "week", rent = LodgerWeek;
            x = free.front().first, y = free.front().second;
        }
        else if (verb == "place")
        {
            const auto* p = estates_.property(cell);
            if (!p || !p->individuals || estates_.lease(cell) || wholeLodgingAt(cell))
                return result = {false, p && p->individuals ? "It is taken." : "This place isn't to let to you.", {}}, true;
            kind = "place", landlord = p->landlord;
            account = p->landlord == "treasury" ? society.treasuryOf(townFor(cell)) : society.tillOf(p->landlord);
            rent = period == "week" ? p->rent : std::max<std::int64_t>(1, p->night > 0 ? p->night : (p->rent + 3) / 4);
        }
        else
        {
            const auto* p = innUpstairs(cell);
            if (!p)
                return result = {false, "Beds are let upstairs at an inn.", {}}, true;
            if (estates_.lease(cell) || wholeLodgingAt(cell))
                return result = {false, "The upstairs is taken tonight.", {}}, true;
            landlord = p->landlord, account = society.tillOf(p->landlord);
            if (verb == "night")
            {
                if (std::any_of(lodgings_.begin(), lodgings_.end(), [&](const Lodging& l) { return l.cell == cell; }))
                    return result = {false, "A bed up here is let; the whole upstairs can't be.", {}}, true;
                kind = "night", period = "night", rent = UpstairsNight;
            }
            else
            {
                // The bed nearest the wolf that nobody has.
                double best = 1e9;
                for (const auto& b : world_.bedTiles(cell))
                    if (std::none_of(lodgings_.begin(), lodgings_.end(), [&](const Lodging& l) { return l.cell == cell && l.x == b.first && l.y == b.second; }))
                        if (const double d = std::hypot(b.first + .5 - e->position.x, b.second + .5 - e->position.y); d < best)
                            best = d, x = b.first, y = b.second;
                if (x < 0)
                    return result = {false, "Every bed up here is let.", {}}, true;
                kind = "bed", period = period == "week" ? "week" : "night", rent = period == "week" ? BedWeek : BedNight;
            }
        }
        if (const auto hold = holds_.find(cell); hold != holds_.end() && hold->second.until > now)
            return result = {false, "The landlord has promised it to someone.", {}}, true;
        if (society.spendable(me) < rent || !society.shift(me, account, "", 0, rent, "rent"))
            return result = {false, "It is " + std::to_string(rent) + "p, more than you have.", {}}, true;
        Lodging l;
        l.id = "lodging-" + std::to_string(nextLodging_++);
        l.holder = me, l.cell = cell, l.kind = kind, l.landlord = landlord, l.account = account, l.period = period, l.rent = rent;
        l.x = x, l.y = y;
        l.town = townFor(cell);
        l.paidTo = period == "week" ? now + 7 : kind == "night" || kind == "place" ? nextHour(now, 6) : nextHour(now, 12);
        lodgings_.push_back(l);
        world_.recordEvent({"lease", me, landlord, cell, 0, 0, kind, 0, rent, l.id});
        record(Economy | Character, me);
        const auto* where = world_.cell(cell);
        return result = {true, (kind == "bed" ? "You take a bed for the " + period : kind == "lodger" ? "You take the spare bed, by the week"
                                : kind == "night" ? std::string("You hire the whole upstairs for tonight")
                                                  : "You take " + (where ? where->name : std::string("the place")) + " for the " + period) +
                                   " (" + std::to_string(rent) + "p). There is a small chest for your things.",
                         {}},
               true;
    }
    if (mine == lodgings_.end())
        return result = {false, "You have no lodging.", {}}, true;
    auto& l = *mine;
    if (verb == "end")
    {
        endLodging(std::size_t(mine - lodgings_.begin()), "You give up your lodging.", 0);
        return result = {true, "", {}}, true;
    }
    if (verb == "open" || verb == "close")
    {
        if (l.kind != "night" && l.kind != "place")
            return result = {false, "Only a whole place can be opened to all.", {}}, true;
        l.open = verb == "open";
        return result = {true, l.open ? "You open the doors: anyone may come in tonight." : "You close the doors to all but your guests.", {}}, true;
    }
    if (verb == "guest")
    {
        const auto target = j.string("target");
        if (j.boolean("on", true))
        {
            if (int(l.guests.size()) >= GuestsMost)
                return result = {false, "Three guests at most.", {}}, true;
            l.guests.insert(target);
        }
        else
            l.guests.erase(target);
        return result = {true, "Your guests are as you say.", {}}, true;
    }
    if (verb == "put" || verb == "take")
    {
        // The small chest: 10 kinds and 20 lb.
        const auto item = j.string("item");
        const int quantity = std::max(1, int(j.number("quantity", 1)));
        const auto chest = "let:" + l.id;
        society.openAccount(chest);
        if (e->cellId != l.cell)
            return result = {false, "Your chest is at your lodging.", {}}, true;
        if (verb == "put")
        {
            const auto* a = society.account(chest);
            double pounds = 0;
            for (const auto& [it, n] : a->stock)
                if (const auto* g = items::good(items::baseOf(it)))
                    pounds += g->weight * n;
            const auto* g = items::good(items::baseOf(item));
            if (!g || spareOf(me, item) < quantity)
                return result = {false, "You haven't that to put away.", {}}, true;
            if ((!a->stock.count(item) && int(a->stock.size()) >= ChestKinds) || pounds + g->weight * quantity > ChestPounds + 1e-9)
                return result = {false, "The chest is full.", {}}, true;
            if (!society.shift(me, chest, item, quantity, 0, "put in a chest"))
                return result = {false, "It won't go in.", {}}, true;
            moveScents(me, chest, item, quantity, false);
        }
        else if (!society.shift(chest, me, item, quantity, 0, "taken from a chest"))
            return result = {false, "That isn't in your chest, or you have no room for it.", {}}, true;
        record(Economy | Character, me);
        return result = {true, verb == "put" ? "You put it in your chest." : "You take it from your chest.", {}}, true;
    }
    return result = {false, "That isn't something a lodging does.", {}}, true;
}

void Game::endLodging(std::size_t index, const std::string& why, std::int64_t refund)
{
    // The chest's goods to the renter if there is room (else they stay with the landlord); a refund, or what is owed.
    if (index >= lodgings_.size())
        return;
    const auto l = lodgings_[index];
    lodgings_.erase(lodgings_.begin() + std::ptrdiff_t(index));
    auto& society = world_.society();
    bool left = false;
    if (const auto* chest = society.account("let:" + l.id))
    {
        const auto held = *chest;
        for (const auto& [item, n] : held.stock)
            if (n > 0 && !society.shift("let:" + l.id, l.holder, item, n, 0, "taken from a chest"))
                left = society.shift("let:" + l.id, l.account, item, n, 0, "left with the landlord") || left;
    }
    if (refund > 0 && !society.shift(l.account, l.holder, "", 0, std::min(refund, society.spendable(l.account)), "rent returned"))
        world_.bonds().addOwed(l.holder, l.landlord, refund, world_.calendarDays());
    world_.recordEvent({"lease ended", l.holder, l.landlord, l.cell, 0, 0, l.kind, 0, refund, l.id});
    if (auto* c = clientOf(l.holder))
        system(c, why + (left ? " What wouldn't fit in your pack was left with the landlord." : ""));
    record(Economy, l.holder);
}

void Game::tendLodgings()
{
    // In order of what falls due: a notice run out ends it, the rest refunded; a night ends at its hour; a week renews
    // from the renter's purse, or ends.
    const double now = world_.calendarDays();
    auto& society = world_.society();
    for (std::size_t i = 0; i < lodgings_.size();)
    {
        auto& l = lodgings_[i];
        const double periodDays = l.period == "week" ? 7 : 1;
        if (l.noticeUntil > 0 && now >= l.noticeUntil)
        {
            const auto refund = std::int64_t(std::floor(double(l.rent) * std::max(0., l.paidTo - now) / periodDays));
            endLodging(i, "The landlord needs the room back; your lodging ends.", refund);
            continue;
        }
        if (now < l.paidTo)
        {
            ++i;
            continue;
        }
        if (l.period == "week" && society.spendable(l.holder) >= l.rent && society.shift(l.holder, l.account, "", 0, l.rent, "rent"))
        {
            l.paidTo += 7;
            ++i;
            continue;
        }
        endLodging(i, l.period == "week" ? "You couldn't pay the week's rent; your lodging ends." : "Your night's lodging is over.", 0);
    }
}

Result Game::holdPlace(const std::string& cell, double days, const std::string& reason)
{
    // Held for a story (a DM's, or a story's): no new lodgings; running ones a week's notice.
    holds_[cell] = {world_.calendarDays() + std::max(.1, days), reason};
    for (auto& l : lodgings_)
        if (l.cell == cell && l.noticeUntil < 0)
        {
            l.noticeUntil = world_.calendarDays() + NoticeDays;
            if (auto* c = clientOf(l.holder))
                system(c, "The landlord needs the room back: your lodging ends in a week, the rest of the rent returned.");
        }
    world_.recordEvent({"held for a story", {}, {}, cell, 0, 0, {}, 0, 0, reason});
    return {true, "Held.", cell};
}

json::Value Game::lodgingView(const std::string& viewer)
{
    // One's own lodging, and what can be taken where one stands.
    auto v = Value::object();
    if (const auto* l = lodgingOf(viewer))
    {
        auto m = Value::object();
        const auto* where = world_.cell(l->cell);
        m.add("kind", l->kind);
        m.add("where", where ? where->name : l->cell);
        m.add("period", l->period);
        m.add("days", std::round((l->paidTo - world_.calendarDays()) * 10) / 10);
        m.add("open", l->open);
        if (l->noticeUntil > 0)
            m.add("notice", std::round((l->noticeUntil - world_.calendarDays()) * 10) / 10);
        auto chest = Value::array();
        if (const auto* a = world_.society().account("let:" + l->id))
            for (const auto& [item, n] : a->stock)
                if (n > 0)
                {
                    auto o = Value::object();
                    o.add("id", item);
                    o.add("name", Society::itemName(item));
                    o.add("count", n);
                    chest.push(o);
                }
        m.add("chest", chest);
        m.add("here", world_.entity(viewer) && world_.entity(viewer)->cellId == l->cell);
        v.add("mine", m);
    }
    auto offers = Value::array();
    const auto add = [&](const char* verb, const char* period, const std::string& label, std::int64_t price) {
        auto o = Value::object();
        o.add("verb", verb);
        o.add("period", period);
        o.add("label", label);
        o.add("price", price);
        offers.push(o);
    };
    if (const auto* e = world_.entity(viewer); e && !lodgingOf(viewer))
    {
        const bool held = holds_.count(e->cellId) && holds_.at(e->cellId).until > world_.calendarDays();
        if (!held && innUpstairs(e->cellId) && !estates_.lease(e->cellId) && !wholeLodgingAt(e->cellId))
        {
            add("bed", "night", "Take a bed for the night", BedNight);
            add("bed", "week", "Take a bed for a week", BedWeek);
            if (std::none_of(lodgings_.begin(), lodgings_.end(), [&](const Lodging& l) { return l.cell == e->cellId; }))
                add("night", "night", "Hire the upstairs for tonight", UpstairsNight);
        }
        if (const auto* p = estates_.property(e->cellId); !held && p && p->individuals && !estates_.lease(e->cellId) && !wholeLodgingAt(e->cellId))
        {
            add("place", "night", "Take it for the night", std::max<std::int64_t>(1, p->night > 0 ? p->night : (p->rent + 3) / 4));
            add("place", "week", "Take it for a week", p->rent);
        }
    }
    if (offers.items().empty() && !v.has("mine"))
        return {};
    v.add("offers", offers);
    return v;
}

void Game::lodgingsSave(json::Value& root) const
{
    auto list = Value::array();
    for (const auto& l : lodgings_)
    {
        auto o = Value::object();
        o.add("id", l.id);
        o.add("holder", l.holder);
        o.add("cell", l.cell);
        o.add("kind", l.kind);
        o.add("landlord", l.landlord);
        o.add("account", l.account);
        o.add("period", l.period);
        o.add("town", l.town);
        o.add("x", l.x);
        o.add("y", l.y);
        o.add("rent", double(l.rent));
        o.add("paidTo", l.paidTo);
        o.add("noticeUntil", l.noticeUntil);
        o.add("open", l.open);
        auto guests = Value::array();
        for (const auto& g : l.guests)
            guests.push(g);
        o.add("guests", guests);
        list.push(o);
    }
    root.add("lodgings", list);
    auto holds = Value::array();
    for (const auto& [cell, h] : holds_)
    {
        auto o = Value::object();
        o.add("cell", cell);
        o.add("until", h.until);
        o.add("reason", h.reason);
        holds.push(o);
    }
    root.add("holds", holds);
}

void Game::lodgingsLoad(const json::Value& saved)
{
    lodgings_.clear();
    holds_.clear();
    for (const auto& o : saved.array("lodgings"))
    {
        Lodging l;
        l.id = o.string("id");
        l.holder = o.string("holder");
        l.cell = o.string("cell");
        l.kind = o.string("kind");
        l.landlord = o.string("landlord");
        l.account = o.string("account");
        l.period = o.string("period", "night");
        l.town = o.string("town");
        l.x = int(o.number("x", -1));
        l.y = int(o.number("y", -1));
        l.rent = std::int64_t(o.number("rent"));
        l.paidTo = o.number("paidTo");
        l.noticeUntil = o.number("noticeUntil", -1);
        l.open = o.boolean("open");
        for (const auto& g : o.array("guests"))
            if (g.isString())
                l.guests.insert(g.asString());
        if (l.id.empty() || l.holder.empty())
            continue;
        if (const auto n = std::strtoull(l.id.c_str() + 8, nullptr, 10); n >= nextLodging_)
            nextLodging_ = n + 1;
        lodgings_.push_back(std::move(l));
    }
    for (const auto& o : saved.array("holds"))
        holds_[o.string("cell")] = {o.number("until"), o.string("reason")};
}
} // namespace ratw::game
