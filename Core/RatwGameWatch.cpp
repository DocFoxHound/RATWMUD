// The LIVE map's frames (Docs/Design/34-dungeon-master-refresh.md, 1.1; RatwWatch.h): everyone's place, made only
// while a Dungeon Master is watching.
#include "RatwGame.h"

#include <cmath>

namespace ratw::game
{
namespace
{
double tenth(double v) { return std::round(v * 10) / 10; }
} // namespace

void Game::feedWatch(double dt)
{
    if (!watch_ || (watchAccumulator_ += dt) < watch::Feed::Interval)
        return;
    watchAccumulator_ = 0;
    if (watch_->watching())
        watch_->offer(watchFrame());
}

// {"day": calendar days, "people": [[id, name, kind, cell, x, y, flags, role, doing], ...],
//  "shops": [[merchant, name, label, cell, x, y, at a stall], ...]}
// kind: "p" a player in the world, "o" a player character not in it (where they were saved), "n" an NPC, "r" folk of
// the road (a caravan's wagon, bandits: made as needed, never saved), "t" a temporary visitor (World::addVisitor).
// flags: 1 dead, 2 downed, 4 off stage, 8 in a fight. "weather": the systems over the world (below).
std::string Game::watchFrame() const
{
    using json::Value;
    Value people = Value::array(), shops = Value::array();
    const auto& society = world_.society();
    const auto row = [&](const Entity& e, const char* kind, int flags, const std::string& role, const std::string& doing) {
        Value r = Value::array();
        r.push(e.id);
        r.push(e.name);
        r.push(kind);
        r.push(e.cellId);
        r.push(tenth(e.position.x));
        r.push(tenth(e.position.y));
        r.push(flags);
        r.push(role);
        r.push(doing);
        people.push(std::move(r));
    };
    for (const auto& [id, e] : world_.entities())
    {
        const int flags = (e.dead ? 1 : 0) | (e.downedLeft > 0 ? 2 : 0) | (e.offstage ? 4 : 0) | (world_.inBattle(id) ? 8 : 0);
        if (!e.npc)
        {
            row(e, "p", flags, "", e.activity.empty() ? e.state : e.activity);
            continue;
        }
        const auto* spec = society.spec(id);
        const auto* life = society.resident(id);
        const std::string role = spec ? spec->role : life ? life->role : std::string();
        if (const double leaves = world_.visitorLeaves(id); leaves >= 0)
        {
            row(e, "t", flags, "", "leaves in " + std::to_string(int(std::ceil((leaves - world_.worldTime()) / 60))) + " min");
            continue;
        }
        row(e, e.transient ? "r" : "n", flags, role, life ? life->task : e.activity.empty() ? e.state : e.activity);
        if (spec && spec->role == "merchant" && !spec->work.cell.empty())
        {
            Value s = Value::array();
            s.push(id);
            s.push(spec->name);
            s.push(spec->workLabel);
            s.push(spec->work.cell);
            s.push(tenth(spec->work.x));
            s.push(tenth(spec->work.y));
            s.push(society.atStall(id));
            shops.push(std::move(s));
        }
    }
    for (const auto& [id, e] : characters_)
        if (!world_.entity(id))
            row(e, "o", e.dead ? 1 : 0, "", "");
    // The weather systems over the world now: [id, kind, x, y (their middle, world tiles), reach, strength there].
    Value weather = Value::array();
    const double day = world_.calendarDays();
    for (const auto& s : world_.weatherSystems())
    {
        const double x = s.x + s.vx * (day - s.born), y = s.y + s.vy * (day - s.born);
        const double strength = weatherStrengthAt(s, x, y, day);
        if (strength <= .02)
            continue;
        Value w = Value::array();
        w.push(s.id);
        w.push(weatherName(s.kind));
        w.push(tenth(x));
        w.push(tenth(y));
        w.push(tenth(s.radius));
        w.push(std::round(strength * 100) / 100);
        weather.push(std::move(w));
    }
    Value frame = Value::object();
    frame.add("day", world_.calendarDays());
    frame.add("people", std::move(people));
    frame.add("shops", std::move(shops));
    frame.add("weather", std::move(weather));
    return json::dump(frame);
}
} // namespace ratw::game
