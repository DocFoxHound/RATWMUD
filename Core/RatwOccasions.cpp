// Occasions and shared meals (Docs/Design/55-letters-gifts-favours.md, 6; Phase 6), in the world.
// - A wedding (a marriage): the next Restday, 11:00 to 12:00, at the hosts' church (its pulpit, from the day plans; else
//   at the first host's home). A funeral (a death): the next morning, 10:00 to 11:00, the same. The couple, or the
//   mourners, are its hosts, sent there by errand (World::errand), which overrides the day's plan.
// - Game invites players by letter (RatwResidentLetters.cpp). An invited player present at the hour, near the place,
//   stands witness: the hosts warm to it (+5), and a `witnessed` event is recorded.
// - Fed: eating gives 2 game hours of it (stamina back a tenth faster, rest healing 5% faster); a wolf within 2 tiles
//   who ate in the last 10 game minutes (or a resident eating now) makes it a shared meal: 4 hours for both, and the two
//   grow closer once a game day.
// Placeholders throughout. Occasions aren't saved: one planned before a restart is let go.
#include "RatwWorld.h"

#include "RatwCalendar.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
namespace
{
constexpr double FedDays = 2. / 24, SharedDays = 4. / 24, AteWithin = 10. / 1440, WitnessTiles = 10;
}

void World::fed(Entity& e)
{
    e.ateAt = calendarDays_;
    e.fedUntil = std::max(e.fedUntil, calendarDays_ + FedDays);
    for (const Entity* o : entitiesIn(e.cellId))
    {
        if (!o || o->id == e.id || o->dead || std::hypot(o->position.x - e.position.x, o->position.y - e.position.y) > 2)
            continue;
        const auto* life = o->npc ? society_.resident(o->id) : nullptr;
        const bool eating = o->npc ? life && life->task == "eat" : calendarDays_ - o->ateAt <= AteWithin;
        if (!eating)
            continue;
        e.fedUntil = std::max(e.fedUntil, calendarDays_ + SharedDays);
        if (auto* other = entity(o->id); other && !other->npc)
            other->fedUntil = std::max(other->fedUntil, calendarDays_ + SharedDays);
        const auto key = e.id < o->id ? e.id + "|" + o->id : o->id + "|" + e.id;
        if (const double today = std::floor(calendarDays_); !mealBondDay_.count(key) || mealBondDay_[key] < today)
        {
            mealBondDay_[key] = today;
            bonds_.mutual(e.id, o->id, {1, 0, 1, 0, 0}, calendarDays_);
            recordEvent({"shared meal", e.id, o->id, e.cellId, 0, 0, {}, 0, 0, {}});
        }
    }
}

Occasion* World::occasion(const std::string& id)
{
    for (auto& o : occasions_)
        if (o.id == id)
            return &o;
    return nullptr;
}

void World::planOccasion(const std::string& kind, const std::vector<std::string>& hosts, const std::string& about)
{
    // One occasion for its subject: a death's funeral gathers every mourner as a host.
    const auto id = kind + ":" + about;
    if (auto* had = occasion(id))
    {
        for (const auto& h : hosts)
            if (std::find(had->hosts.begin(), had->hosts.end(), h) == had->hosts.end())
                had->hosts.push_back(h);
        return;
    }
    const auto* first = hosts.empty() ? nullptr : entity(hosts.front());
    const auto* spec = first ? society_.spec(first->id) : nullptr;
    if (!first || !spec)
        return;
    Occasion o;
    o.id = id;
    o.kind = kind;
    o.hosts = hosts;
    o.community = communityOf(spec->home.cell.empty() ? first->cellId : spec->home.cell);
    // The church: the community's pulpit in the day plans; else the first host's home.
    const auto& plans = society_.day().plans;
    if (const auto plan = plans.find(o.community); plan != plans.end() && !plan->second.pulpit.cell.empty())
        o.cell = plan->second.pulpit.cell, o.x = plan->second.pulpit.x, o.y = plan->second.pulpit.y;
    else
        o.cell = spec->home.cell, o.x = spec->home.x, o.y = spec->home.y;
    // When: a wedding the next Restday at 11:00; a funeral the next morning at 10:00. An hour each.
    double day = std::floor(calendarDays_) + 1;
    if (kind == "wedding")
        while (calendar::weekdayOf(day) != calendar::Restday)
            day += 1;
    o.start = day + (kind == "wedding" ? 11. : 10.) / 24;
    o.end = o.start + 1. / 24;
    occasions_.push_back(std::move(o));
}

void World::tendOccasions(double dt)
{
    // Once a game minute or so: the invited near the place during the hour stand witness; over, it goes.
    occasionsAccumulator_ += dt;
    if (occasionsAccumulator_ < 5)
        return;
    occasionsAccumulator_ = 0;
    for (auto it = occasions_.begin(); it != occasions_.end();)
    {
        auto& o = *it;
        if (calendarDays_ >= o.end)
        {
            it = occasions_.erase(it);
            continue;
        }
        if (calendarDays_ >= o.start)
            for (const auto& player : o.invited)
                if (const auto* p = entity(player); p && !o.witnesses.count(player) && p->cellId == o.cell &&
                                                    std::hypot(p->position.x - o.x, p->position.y - o.y) <= WitnessTiles)
                {
                    o.witnesses.insert(player);
                    for (const auto& host : o.hosts)
                        bonds_.change(host, player, {5, 0, 1, 0, 0}, calendarDays_);
                    recordEvent({"witnessed", player, o.hosts.empty() ? std::string() : o.hosts.front(), o.cell, 0, 0, o.kind, 0, 0, o.id});
                    notice(player, o.kind == "wedding" ? "You stand witness at the wedding." : "You stand with the mourners.");
                }
        ++it;
    }
}
} // namespace ratw
