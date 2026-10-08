// A Chapter's rented places (RatwEstates.h; Docs/Design/32-parties-chapters-factions.md, 5.2).
#include "RatwEstates.h"

#include <algorithm>
#include <cmath>

namespace ratw::estate
{
using json::Value;

void Estates::define(const Property& p)
{
    if (!p.id.empty())
        properties_[p.id] = p;
}

void Estates::clearDerived()
{
    for (auto it = properties_.begin(); it != properties_.end();)
        it = authored_.count(it->first) || leases_.count(it->first) ? std::next(it) : properties_.erase(it);
}

const Property* Estates::property(const std::string& cell) const
{
    const auto found = properties_.find(cell);
    return found == properties_.end() ? nullptr : &found->second;
}

const Lease* Estates::lease(const std::string& cell) const
{
    const auto found = leases_.find(cell);
    return found == leases_.end() ? nullptr : &found->second;
}

Lease* Estates::lease(const std::string& cell)
{
    const auto found = leases_.find(cell);
    return found == leases_.end() ? nullptr : &found->second;
}

std::vector<const Lease*> Estates::leasesOf(const std::string& chapter) const
{
    std::vector<const Lease*> out;
    for (const auto& [cell, l] : leases_)
        if (l.chapter == chapter)
            out.push_back(&l);
    return out;
}

Outcome Estates::open(const std::string& cell, const std::string& chapter, double day)
{
    const auto* p = property(cell);
    if (!p)
        return {false, "This place isn't to let."};
    if (leases_.count(cell))
        return {false, "Someone already rents it."};
    if (leasesOf(chapter).size() >= 3)
        return {false, "Your Chapter rents as many places as it can keep."};
    Lease l;
    l.property = cell;
    l.chapter = chapter;
    l.landlord = p->landlord;
    l.rent = p->rent;
    l.started = day;
    l.paidTo = day + WeekDays;
    leases_[cell] = l;
    return {true, {}};
}

Outcome Estates::close(const std::string& cell)
{
    return leases_.erase(cell) ? Outcome{true, {}} : Outcome{false, "It isn't rented."};
}

std::vector<Estates::Due> Estates::due(double day) const
{
    std::vector<Due> out;
    for (const auto& [cell, l] : leases_)
        if (day >= l.paidTo)
            out.push_back({cell, l.chapter, l.landlord, l.rent});
    return out;
}

void Estates::paid(const std::string& cell, double day)
{
    if (auto* l = lease(cell))
    {
        l->paidTo = std::max(l->paidTo, day) + WeekDays;
        l->state = "active";
    }
}

bool Estates::unpaid(const std::string& cell, double day)
{
    auto* l = lease(cell);
    if (!l)
        return false;
    if (day >= l->paidTo + GraceDays)
    {
        leases_.erase(cell);
        return true;
    }
    l->state = "grace";
    return false;
}

bool Estates::heldLongEnough(const std::string& chapter, double day) const
{
    for (const auto& [cell, l] : leases_)
        if (l.chapter == chapter && l.state == "active" && day - l.started >= HeldForGate)
            return true;
    return false;
}

Value Estates::save() const
{
    auto root = Value::object();
    auto authored = Value::array();
    for (const auto& cell : authored_)
        if (const auto* p = property(cell))
        {
            auto j = Value::object();
            j.add("id", p->id); j.add("name", p->name); j.add("kind", p->kind); j.add("landlord", p->landlord);
            j.add("faction", p->faction); j.add("rent", double(p->rent)); j.add("level", p->level);
            if (p->individuals)
                j.add("individuals", true), j.add("night", double(p->night));
            authored.push(j);
        }
    root.add("authored", authored);
    auto leases = Value::array();
    for (const auto& [cell, l] : leases_)
    {
        auto j = Value::object();
        j.add("property", l.property); j.add("chapter", l.chapter); j.add("landlord", l.landlord); j.add("rent", double(l.rent));
        j.add("started", l.started); j.add("paidTo", l.paidTo); j.add("state", l.state);
        auto guests = Value::array();
        for (const auto& g : l.guests)
            guests.push(g);
        j.add("guests", guests);
        auto notices = Value::array();
        for (const auto& n : l.notices)
        {
            auto k = Value::object();
            k.add("by", n.by); k.add("text", n.text); k.add("at", n.at);
            notices.push(k);
        }
        j.add("notices", notices);
        // The place itself, so a lease outlives a change in what the world offers.
        if (const auto* p = property(cell))
        {
            auto k = Value::object();
            k.add("id", p->id); k.add("name", p->name); k.add("kind", p->kind); k.add("landlord", p->landlord);
            k.add("faction", p->faction); k.add("rent", double(p->rent)); k.add("level", p->level);
            j.add("place", k);
        }
        leases.push(j);
    }
    root.add("leases", leases);
    return root;
}

void Estates::load(const Value& saved)
{
    leases_.clear();
    if (!saved.isObject())
        return;
    const auto place = [](const Value& k) {
        return Property{k.string("id"), k.string("name"), k.string("kind", "hall"), k.string("landlord", "treasury"), k.string("faction"),
                        std::int64_t(k.number("rent")), int(k.number("level", 2)), k.boolean("individuals"), std::int64_t(k.number("night"))};
    };
    for (const auto& k : saved.array("authored"))
    {
        define(place(k));
        authored_.insert(k.string("id"));
    }
    for (const auto& j : saved.array("leases"))
    {
        Lease l;
        l.property = j.string("property"); l.chapter = j.string("chapter"); l.landlord = j.string("landlord");
        l.rent = std::int64_t(j.number("rent")); l.started = j.number("started"); l.paidTo = j.number("paidTo");
        l.state = j.string("state", "active");
        for (const auto& g : j.array("guests"))
            if (g.isString())
                l.guests.insert(g.asString());
        for (const auto& k : j.array("notices"))
            l.notices.push_back({k.string("by"), k.string("text"), k.number("at")});
        if (l.property.empty())
            continue;
        if (!property(l.property) && j["place"].isObject())
            define(place(j["place"]));
        leases_[l.property] = l;
    }
}
} // namespace ratw::estate
