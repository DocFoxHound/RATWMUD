// A Chapter's rented places as players use them (Docs/Design/32-parties-chapters-factions.md, 5.2; the rules are
// RatwEstates.cpp): what the world has to let (rooms above inns, warehouses, and whatever a DM adds), leasing from the
// Chapter's treasury, rent each week to the landlord's purse, grace and eviction, the locked door, guests, the notice
// board, the Chapter's stores, and the rented hall the Company level needs.
#include "RatwGame.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
std::string treasuryOf(const std::string& chapterId) { return "chapter:" + chapterId; }
const char* const Goods[] = {"herbs", "meal", "sword"};
} // namespace

void Game::refreshEstates()
{
    // Where the towns are, for camps (5.3): every place someone works or lives.
    townCells_.clear();
    for (const auto& pos : world_.society().positions())
        townCells_.insert(pos.work.cell);
    for (const auto& [id, life] : world_.society().state().residents)
        if (!life.homeCell.empty())
            townCells_.insert(life.homeCell);
    // Rooms above an inn ("<inn>, upstairs") from its keeper; warehouses from the town. A DM's own are kept, and
    // Atlas's (`let` records) come first.
    estates_.clearDerived();
    for (const auto& [cellId, l] : world_.lettings())
        if (const auto* c = world_.cell(cellId); c && !estates_.property(cellId))
            estates_.define({cellId, c->name, l.kind, l.landlord, c->factionClaims.empty() ? std::string() : c->factionClaims.front(), l.rent,
                             l.level});
    std::map<std::string, std::string> byName;
    for (const auto& [id, c] : world_.cells())
        byName[c.name] = id;
    for (const auto& [id, c] : world_.cells())
    {
        estate::Property p;
        p.id = id;
        p.name = c.name;
        if (!c.factionClaims.empty())
            p.faction = c.factionClaims.front();
        const auto comma = c.name.rfind(", upstairs");
        if (comma != std::string::npos && comma + 10 == c.name.size())
        {
            const auto base = byName.find(c.name.substr(0, comma));
            if (base == byName.end())
                continue;
            for (const auto& pos : world_.society().positions())
                if (pos.role == "merchant" && pos.work.cell == base->second)
                    if (const auto held = world_.society().state().careers.positions.find(pos.id);
                        held != world_.society().state().careers.positions.end() && !held->second.holder.empty())
                        p.landlord = held->second.holder;
            if (p.landlord.empty())
                continue;
            if (p.faction.empty())
                if (const auto* cell = world_.cell(base->second); cell && !cell->factionClaims.empty())
                    p.faction = cell->factionClaims.front();
            p.kind = "hall";
            p.rent = 30;
            p.level = 2;
        }
        else if (c.name.find("Warehouse") != std::string::npos)
        {
            p.kind = "warehouse";
            p.landlord = "treasury";
            p.rent = 50;
            p.level = 3;
        }
        else
            continue;
        if (!estates_.property(id))
            estates_.define(p);
    }
}

bool Game::mayEnterPlace(const std::string& who, const std::string& cell) const
{
    // A whole place a wolf rents (doc 54, 4): its renter, its guests, or anyone on a night it is opened.
    if (const auto* l = wholeLodgingAt(cell))
        return l->holder == who || l->guests.count(who) || l->open;
    const auto* lease = estates_.lease(cell);
    if (!lease)
        return true;
    if (const auto* c = chapters_.of(who); c && c->id == lease->chapter)
        return true;
    return lease->guests.count(who) > 0;
}

void Game::estateTick(double dt)
{
    estateAccumulator_ += dt;
    if (estateAccumulator_ < 5)
        return;
    estateAccumulator_ = 0;
    if ((estateRefresh_ -= 5) <= 0)
    {
        refreshEstates();
        estateRefresh_ = 300;
    }
    tendLodgings();                                 // Individuals' lodgings (doc 54, 4).
    const double day = world_.calendarDays();
    for (const auto& d : estates_.due(day))
    {
        const auto* p = estates_.property(d.property);
        const std::string name = p ? p->name : d.property;
        // The land's faction has turned against the Chapter: the lease lapses at the rent (4.3).
        if (p && !p->faction.empty() && standingOf(p->faction, d.chapter) <= -40)
        {
            estates_.close(d.property);
            tellChapter(d.chapter, "The lease on " + name + " has lapsed: its faction won't let to the Chapter now.");
            continue;
        }
        const auto landlord = world_.society().account(d.landlord) ? d.landlord : std::string("treasury");
        if (world_.society().shift(treasuryOf(d.chapter), landlord, "", 0, d.rent, "chapter rent"))
        {
            estates_.paid(d.property, day);
            record(Economy);
            tellChapter(d.chapter, std::to_string(d.rent) + " pennies paid from the treasury: a week's rent on " + name + ".");
        }
        else if (estates_.unpaid(d.property, day))
            tellChapter(d.chapter, "Evicted from " + name + ": the rent went unpaid past the week's grace.");
        else if (!rentWarned_.count(d.property))
        {
            rentWarned_.insert(d.property);
            tellChapter(d.chapter, "The rent on " + name + " is due and the treasury can't pay it. A week's grace, then eviction.");
        }
        chapterViewsDirty_ = true;
    }
    for (auto it = rentWarned_.begin(); it != rentWarned_.end();)
        it = estates_.lease(*it) && estates_.lease(*it)->state == "grace" ? std::next(it) : rentWarned_.erase(it);
    // The rented hall the Company level needs: held, paid up, for two weeks.
    std::vector<std::string> ids;
    for (const auto& [id, c] : chapters_.all())
        ids.push_back(id);
    for (const auto& id : ids)
        if (auto* c = chapters_.byId(id))
        {
            const bool held = estates_.heldLongEnough(id, day);
            if (held != c->hallHeldTwoWeeks)
            {
                c->hallHeldTwoWeeks = held;
                chapterAdvanced(id);
            }
        }
}

bool Game::estateCommand(Connection* c, const Value& j, Result& result)
{
    const std::string id = c->entityId, verb = j.string("verb"), target = j.string("target");
    const auto* me = world_.entity(id);
    const auto* chapter = chapters_.of(id);
    const auto* member = chapters_.member(id);
    const double day = world_.calendarDays();
    if (!me || !chapter || !member)
    {
        result = {false, "Only a Chapter rents.", {}};
        return true;
    }
    const auto here = me->cellId;
    const auto* p = estates_.property(here);
    auto* lease = estates_.lease(here);
    const bool ours = lease && lease->chapter == chapter->id;
    const bool officer = member->rank <= chapter::RankOfficer;
    if (verb == "lease")
    {
        if (!p)
            result = {false, "This place isn't to let.", {}};
        else if (!officer)
            result = {false, "Only an Officer or the Head takes a lease.", {}};
        else if (chapter->level < p->level)
            result = {false, std::string("Renting ") + (p->kind == "warehouse" ? "a warehouse" : "rooms") + " takes a " +
                                 chapter::levelName(p->level) + " (level " + std::to_string(p->level) + ").",
                      {}};
        else if (!p->faction.empty() && standingOf(p->faction, chapter->id) < 15)
            result = {false, "Its faction lets only to a Chapter it knows well.", {}};
        else if (lease)
            result = {false, "Someone already rents it.", {}};
        else
        {
            const auto landlord = world_.society().account(p->landlord) ? p->landlord : std::string("treasury");
            if (!world_.society().shift(treasuryOf(chapter->id), landlord, "", 0, p->rent, "chapter rent"))
                result = {false, "The first week is " + std::to_string(p->rent) + " pennies, from the Chapter's treasury, and it hasn't them.", {}};
            else
            {
                const auto opened = estates_.open(here, chapter->id, day);
                if (!opened.ok)
                {
                    world_.society().shift(landlord, treasuryOf(chapter->id), "", 0, p->rent, "chapter rent refunded");
                    result = {false, opened.message, {}};
                }
                else
                {
                    record(Economy);
                    tellChapter(chapter->id, "The Chapter rents " + p->name + " now, at " + std::to_string(p->rent) + " pennies a week.", id);
                    result = {true, "You take the lease on " + p->name + ". The door is the Chapter's.", {}};
                }
            }
        }
    }
    else if (verb == "endlease")
    {
        if (!ours || !officer)
            result = {false, "Only an Officer ends the Chapter's own lease, from inside.", {}};
        else
        {
            estates_.close(here);
            tellChapter(chapter->id, "The Chapter gives up " + (p ? p->name : here) + ".", id);
            result = {true, "You give up the lease.", {}};
        }
    }
    else if (verb == "guest" || verb == "unguest")
    {
        if (!ours || !officer)
            result = {false, "Only an Officer lets guests in, from inside the Chapter's place.", target};
        else
        {
            if (verb == "guest")
                lease->guests.insert(target);
            else
                lease->guests.erase(target);
            result = {true, verb == "guest" ? labelFor(id, target) + " may come and go here." : labelFor(id, target) + " may not come in now.", target};
        }
    }
    else if (verb == "notice")
    {
        const auto text = mind::trim(j.string("text"));
        if (!ours)
            result = {false, "Post notices inside the Chapter's own place.", {}};
        else if (text.empty() || text.size() > 200)
            result = {false, "A notice is 1 to 200 letters.", {}};
        else
        {
            lease->notices.push_back({id, text, day});
            if (lease->notices.size() > 20)
                lease->notices.erase(lease->notices.begin());
            result = {true, "Posted.", {}};
        }
    }
    else if (verb == "unnotice")
    {
        const int at = int(j.number("index", -1));
        if (!ours || at < 0 || at >= int(lease->notices.size()) || (lease->notices[std::size_t(at)].by != id && !officer))
            result = {false, "You can't take that down.", {}};
        else
        {
            lease->notices.erase(lease->notices.begin() + at);
            result = {true, "Taken down.", {}};
        }
    }
    else if (verb == "store" || verb == "take")
    {
        // The Chapter's stores: kept in its treasury's purse, reached from any place it holds.
        const auto item = j.string("item");
        const int quantity = int(j.number("quantity", 1));
        const bool known = std::find(std::begin(Goods), std::end(Goods), item) != std::end(Goods);
        if (!ours && !campHere(id))
            result = {false, "The Chapter's stores are kept in its own places.", {}};
        else if (!known || quantity < 1 || quantity > 99)
            result = {false, "Say what, and how many.", {}};
        else if (verb == "take" && member->rank == chapter::RankInitiate)
            result = {false, "Initiates don't take from the stores.", {}};
        else
        {
            const bool in = verb == "store";
            const bool moved = world_.society().shift(in ? id : treasuryOf(chapter->id), in ? treasuryOf(chapter->id) : id, item, quantity, 0,
                                                      in ? "chapter stores" : "chapter stores taken");
            if (moved)
                record(Economy, id);
            result = {moved, moved ? (in ? "Put in the stores." : "Taken from the stores.") : (in ? "You haven't that." : "The stores haven't that."), {}};
        }
    }
    else
        return false;
    if (result.ok)
    {
        chapterViewsDirty_ = true;
        saveSoon();
    }
    return true;
}

Value Game::placeView(const std::string& viewer) const
{
    // The rentable place this wolf stands in: its terms, who holds it, and for its Chapter, the board and the stores.
    const auto* me = world_.entity(viewer);
    const auto* p = me ? estates_.property(me->cellId) : nullptr;
    if (!p)
        return {};
    auto v = Value::object();
    v.add("id", p->id);
    v.add("name", p->name);
    v.add("kind", p->kind);
    v.add("rent", double(p->rent));
    v.add("level", p->level);
    v.add("landlord", p->landlord == "treasury" ? std::string("the town") : names::capitalised(labelFor(viewer, p->landlord)));
    if (const auto* f = factions_.find(p->faction))
        v.add("faction", f->name);
    if (const auto* l = estates_.lease(p->id))
    {
        const auto* holder = chapters_.byId(l->chapter);
        v.add("heldBy", holder ? holder->name : std::string("a Chapter"));
        const auto* mine = chapters_.of(viewer);
        if (mine && mine->id == l->chapter)
        {
            v.add("mine", true);
            v.add("state", l->state);
            v.add("daysPaid", std::max(0.0, std::floor((l->paidTo - world_.calendarDays()) * 10) / 10));
            auto notices = Value::array();
            for (const auto& n : l->notices)
            {
                auto o = Value::object();
                o.add("by", names::capitalised(labelFor(viewer, n.by)));
                o.add("text", n.text);
                notices.push(o);
            }
            v.add("notices", notices);
            auto stores = Value::object();
            if (const auto* purse = world_.society().account(treasuryOf(l->chapter)))
                for (const char* item : Goods)
                    if (const int n = Society::stock(*purse, item); n > 0)
                        stores.add(item, n);
            v.add("stores", stores);
        }
    }
    return v;
}

Value Game::leasesView(const std::string& chapterId) const
{
    auto list = Value::array();
    for (const auto* l : estates_.leasesOf(chapterId))
    {
        auto o = Value::object();
        const auto* p = estates_.property(l->property);
        o.add("name", p ? p->name : l->property);
        o.add("rent", double(l->rent));
        o.add("state", l->state);
        o.add("daysPaid", std::max(0.0, std::floor((l->paidTo - world_.calendarDays()) * 10) / 10));
        o.add("heldDays", std::floor(world_.calendarDays() - l->started));
        list.push(o);
    }
    return list;
}
} // namespace ratw::game

