// Schedules (RatwSchedules.h; Docs/Design/26-living-npcs.md, Phase 9). World members, kept here.
#include "RatwSchedules.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
namespace
{
constexpr int Stalls = 24, Crowd = 64;              // Spots worked out around each market square.
constexpr int StallReach = 40;                      // How far from the market's middle its built stalls are looked for.
constexpr int FestivalsKept = 64;
// Each town has its own name for each season's festival: one of these, as its name falls.
const char* const FestivalNames[4][3] = {
    {"the Blossom Fair", "the Greening", "Seedtide"},
    {"Midsummer", "the Long Light", "the Sun Feast"},
    {"Harvest Home", "the Gathering", "the Last Sheaf"},
    {"Midwinter", "the Long Night", "the Hearth Feast"},
};
std::uint64_t fnv(const std::string& text)
{
    std::uint64_t h = 1469598103934665603ULL;
    for (const unsigned char c : text)
        h = (h ^ c) * 1099511628211ULL;
    return h;
}
} // namespace

std::string World::festivalName(const std::string& community, int season) const
{
    season = std::clamp(season, 0, 3);
    return FestivalNames[season][fnv(community) % 3];
}

std::uint64_t World::squarePrint(const std::string& cellId) const
{
    // What a square's stalls and crowd rest on besides its cell's tiles: the posts worked in its cell, the homes in it, and
    // what stands on its ground (a Chapter's camp: obstacles, which part its regions). (Its doors count by where they are,
    // never by whether they stand open: fillSquare.)
    std::uint64_t h = 1469598103934665603ULL;
    const auto mix = [&](double v) { h = (h ^ std::uint64_t(std::int64_t(std::floor(v * 4)))) * 1099511628211ULL; };
    for (const auto& p : society_.positions())
        if (p.work.cell == cellId)
            mix(p.work.x), mix(p.work.y);
    h = (h ^ 0x9e3779b97f4a7c15ULL) * 1099511628211ULL;
    for (const auto& [id, life] : society_.state().residents)
        if (life.homeCell == cellId)
            mix(life.homeX), mix(life.homeY);
    h = (h ^ 0x9e3779b97f4a7c15ULL) * 1099511628211ULL;
    if (const auto o = obstacles.find(cellId); o != obstacles.end())
        for (const auto& [x, y] : o->second)
            mix(x), mix(y);
    return h;
}

const World::MarketSquare& World::square(const std::string& community)
{
    // Worked out again when what it rests on moves: when the roster's places do (positions, residents, the towns), and at
    // each day's turn (a camp gone up on the square), each square is checked against its market's cell and spot, its
    // keepers, its homes and what stands on its ground (squarePrint), and only one that differs is worked out afresh (its
    // cells loaded). A restore works them all out afresh. (Worked out afresh once a day, it was: the Country's seven
    // market squares then loaded two cells each, about 50 ms of the day's turn; the repair after doc 79's Phases 4 and 5
    // merged.)
    if (squaresDay_ < 0)
    {
        squares_.clear();
        squaresDay_ = 0;
    }
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (const auto key = society_.placesRevision(); key != squaresKey_ || today != squaresToday_)
    {
        squaresKey_ = key;
        squaresToday_ = today;
        ++squaresCheck_;
    }
    auto found = squares_.find(community);
    if (found != squares_.end() && found->second.checked == squaresCheck_)
        return found->second;
    if (community.empty())
    {
        auto& none = squares_[community];
        none.checked = squaresCheck_;
        return none;                                // Those who live outside any town have no market to go to.
    }
    // The market: a town's (where caravans load), or, without town records, where most of its merchants work.
    std::string at;
    Vec2 where;
    if (const auto* t = town(community); t && !t->market.empty())
        at = t->market, where = {t->marketX, t->marketY};
    else
    {
        std::map<std::string, int> counts;
        for (const auto& p : society_.positions())
            if (p.role == "merchant" && lawTown(p.work.cell) == community)
                ++counts[p.work.cell];
        int most = 0;
        for (const auto& [cellId, n] : counts)
            if (n > most)
                most = n, at = cellId;
        for (const auto& p : society_.positions())
            if (p.role == "merchant" && p.work.cell == at)
            {
                where = {p.work.x, p.work.y};
                break;
            }
    }
    if (found != squares_.end() && found->second.found && found->second.fromCell == at && found->second.fromSpot.x == where.x &&
        found->second.fromSpot.y == where.y && found->second.print == squarePrint(found->second.at.cell))
    {
        found->second.checked = squaresCheck_;      // (The same: kept.)
        return found->second;
    }
    auto& sq = squares_[community];
    sq = MarketSquare{};
    fillSquare(sq, at, where);
    sq.fromCell = at;
    sq.fromSpot = where;
    sq.print = squarePrint(sq.at.cell);
    sq.checked = squaresCheck_;
    return sq;
}

// A market square's middle, stalls and crowd, around `where` in `at` (found: false where there is none).
void World::fillSquare(MarketSquare& sq, std::string at, Vec2 where)
{
    if (at.empty() || !ensureLoaded(at).ok)
        return;
    // A market indoors (a shop) spills out of its door: the square is the street before it.
    if (const auto* c = cell(at); c && !c->outdoors)
        for (const Door* d : doorsIn(at))
            if (d->portal && !d->locked)
                if (const auto* out = cell(d->targetCell); out && out->outdoors && ensureLoaded(d->targetCell).ok)
                {
                    at = d->targetCell;
                    where = d->arrival;
                    break;
                }
    const auto* c = cell(at);
    if (!c || !c->loaded)
        return;
    sq.found = true;
    sq.at = {at, std::floor(where.x) + .5, std::floor(where.y) + .5};
    // Where people can stand around it: open ground reachable from the square, not a bed, not in a doorway.
    std::set<std::pair<int, int>> homes;
    for (const auto& [id, life] : society_.state().residents)
        if (life.homeCell == at)
            homes.insert({int(std::floor(life.homeX)), int(std::floor(life.homeY))});
    const int cx = int(std::floor(where.x)), cy = int(std::floor(where.y));
    const int region = regionAt(*c, where);
    // Its stalls are the ones built on the ground (Docs/Design/39): the open places beside each, where a stallholder
    // doesn't already stand. A town has none, and so no market and no market day; a city has its square of them.
    std::set<std::pair<int, int>> keepers;
    for (const auto& p : society_.positions())
        if (p.work.cell == at)
            keepers.insert({int(std::floor(p.work.x)), int(std::floor(p.work.y))});
    std::set<std::pair<int, int>> stallSpots;
    for (int ty = std::max(0, cy - StallReach); ty <= std::min(c->height - 1, cy + StallReach); ++ty)
        for (int tx = std::max(0, cx - StallReach); tx <= std::min(c->width - 1, cx + StallReach); ++tx)
        {
            const auto* stall = c->tile(tx, ty);
            if (!stall || stall->glyph != 'u')
                continue;
            for (const auto [dx, dy] : {std::pair{0, 1}, std::pair{0, -1}, std::pair{1, 0}, std::pair{-1, 0}})
            {
                const int sx = tx + dx, sy = ty + dy;
                const auto* t = c->tile(sx, sy);
                const Vec2 p{sx + .5, sy + .5};
                if (t && !t->solid && !keepers.count({sx, sy}) && !stallSpots.count({sx, sy}) && int(sq.stalls.size()) < Stalls &&
                    !inDoorway(at, p) && (region < 0 || regionAt(*c, p) == region))
                {
                    stallSpots.insert({sx, sy});
                    sq.stalls.push_back({at, p.x, p.y});
                }
            }
        }
    for (int ring = 1; ring <= 8 && int(sq.crowd.size()) < Crowd; ++ring)
        for (int dy = -ring; dy <= ring; ++dy)
            for (int dx = -ring; dx <= ring; ++dx)
            {
                if (std::max(std::abs(dx), std::abs(dy)) != ring)
                    continue;
                const int tx = cx + dx, ty = cy + dy;
                const auto* t = c->tile(tx, ty);
                const Vec2 p{tx + .5, ty + .5};
                if (!t || t->solid || homes.count({tx, ty}) || inDoorway(at, p) || nearPortal(at, p, 1.2) ||
                    (region >= 0 && regionAt(*c, p) != region) || stallSpots.count({tx, ty}) || keepers.count({tx, ty}))
                    continue;
                if (int(sq.crowd.size()) < Crowd)
                    sq.crowd.push_back({at, p.x, p.y});
            }
}

const std::vector<World::Chapel>& World::churches(const std::string& community)
{
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (today != chapelsDay_)
    {
        chapels_.clear();
        chapelsDay_ = today;
    }
    if (const auto found = chapels_.find(community); found != chapels_.end())
        return found->second;
    auto& list = chapels_[community];
    if (community.empty())
        return list;
    // The rooms indoors its clergy work in (a priest's, a chapel keeper's post), in the order of their first posts, and
    // which of them have pews built: those are its places of worship; without any, the first room alone (a hall kept as
    // a chapel). In Ridgemere 2.0 all its clergy's rooms were one "chapel" (the Chapter House, the first), sixty seats on
    // 56 tiles for about 200 wolves, while the Cathedral's 189 pews and the Chapel of the Tide's stood empty.
    std::vector<const Position*> firsts;
    std::vector<bool> pewed;
    for (const auto& p : society_.positions())
    {
        if (!Society::clergy(p.title) || lawTown(p.work.cell) != community ||
            std::any_of(firsts.begin(), firsts.end(), [&](const Position* f) { return f->work.cell == p.work.cell; }))
            continue;
        const auto* c = cell(p.work.cell);
        if (!c || c->outdoors || !ensureLoaded(p.work.cell).ok || !(c = cell(p.work.cell)))
            continue;
        firsts.push_back(&p);
        pewed.push_back(std::any_of(c->tiles.begin(), c->tiles.end(), [](const Tile& t) { return t.glyph == 'p'; }));
    }
    const bool anyPews = std::find(pewed.begin(), pewed.end(), true) != pewed.end();
    for (std::size_t i = 0; i < firsts.size(); ++i)
    {
        if (anyPews ? !pewed[i] : i > 0)
            continue;
        const auto& at = firsts[i]->work;
        const auto* c = cell(at.cell);
        std::set<std::pair<int, int>> posts;        // (Where its clergy and anyone else working there stand.)
        for (const auto& p : society_.positions())
            if (p.work.cell == at.cell)
                posts.insert({int(std::floor(p.work.x)), int(std::floor(p.work.y))});
        Chapel ch;
        ch.found = true;
        ch.pulpit = at;
        for (int y = 1; y < c->height - 1; ++y)
            for (int x = 1; x < c->width - 1; ++x)
            {
                const Vec2 p{x + .5, y + .5};
                const bool beside = [&] {
                    for (const auto [dx, dy] : {std::pair{0, 1}, std::pair{0, -1}, std::pair{1, 0}, std::pair{-1, 0}})
                        if (const auto* t = c->tile(x + dx, y + dy); t && t->glyph == 'p')
                            return true;
                    return false;
                }();
                if ((pewed[i] ? beside : (x + y) % 2 == 0) && std::hypot(p.x - at.x, p.y - at.y) >= 2.5 && passable(at.cell, p) &&
                    !posts.count({x, y}) && !inDoorway(at.cell, p))
                    ch.pews.push_back({at.cell, p.x, p.y});
            }
        // The front first (nearest the pulpit), so a half-full church fills from the front.
        std::stable_sort(ch.pews.begin(), ch.pews.end(), [&](const Spot& a, const Spot& b) {
            return std::hypot(a.x - at.x, a.y - at.y) < std::hypot(b.x - at.x, b.y - at.y);
        });
        if (!pewed[i] && ch.pews.size() > std::size_t(ChapelSeats))
            ch.pews.resize(std::size_t(ChapelSeats));
        list.push_back(std::move(ch));
    }
    return list;
}

const World::Chapel& World::chapel(const std::string& community)
{
    static const Chapel none;
    const auto& list = churches(community);
    return list.empty() ? none : list.front();
}

void World::seatService(LifeDay& day, std::int64_t today)
{
    // Restday's service (doc 42, Phase 6): where each one is at it, worked out once a day (and again if the roster's places
    // move). In each community keeping Restday: in each of its churches the first clergy post held preaches at the pulpit
    // and its other clergy serve at their own posts; the townsfolk going this week (Society::goesToChurch: a third, never
    // the Watch, never clergy) take a seat each, in a lottery of the day, in the nearest church with one free. Those left
    // without one keep their Restday (home, a walk, family, the tavern).
    const auto key = society_.placesRevision();
    if (!service_ || serviceDay_ != today || serviceKey_ != key)
    {
        serviceDay_ = today;
        serviceKey_ = key;
        auto service = std::make_shared<std::unordered_map<std::string, ServicePlace>>();
        // Where a place is on the map: an outdoor spot by its cell's corner, an interior by where its way out comes out
        // into the open (the fewest doors out: an upstairs room by its house's door), else by its own corner.
        std::unordered_map<std::string, std::vector<const Door*>> doorsFrom;
        for (const auto& [id, d] : doors_)
            if (!d.targetCell.empty() && d.targetCell != d.cellId)
                doorsFrom[d.cellId].push_back(&d);
        std::unordered_map<std::string, Vec2> outside;
        const auto mapAt = [&](const std::string& cellId, double x, double y) -> Vec2 {
            const auto* c = cell(cellId);
            if (!c)
                return {x, y};
            if (c->outdoors)
                return {c->worldX + x, c->worldY + y};
            auto [found, fresh] = outside.try_emplace(cellId, Vec2{c->worldX + x, c->worldY + y});
            if (fresh)
            {
                std::vector<std::string> queue{cellId};
                std::set<std::string> seen{cellId};
                bool out = false;
                for (std::size_t at = 0; at < queue.size() && at < 16 && !out; ++at)
                    if (const auto from = doorsFrom.find(queue[at]); from != doorsFrom.end())
                        for (const Door* d : from->second)
                        {
                            const auto* to = cell(d->targetCell);
                            if (to && to->outdoors)
                            {
                                found->second = {to->worldX + d->arrival.x, to->worldY + d->arrival.y};
                                out = true;
                                break;
                            }
                            if (to && seen.insert(d->targetCell).second)
                                queue.push_back(d->targetCell);
                        }
            }
            return found->second;
        };
        std::map<std::string, std::vector<std::pair<std::uint64_t, std::string>>> goers;   // By community: lot, resident.
        for (const auto& [id, life] : society_.state().residents)
        {
            const auto plan = day.plans.find(lawTown(life.homeCell));
            if (plan == day.plans.end() || plan->first.empty() || plan->second.kind != "rest" || !Society::goesToChurch(id, today))
                continue;
            if (const auto* job = society_.jobOf(id); job && (job->role == "guard" || Society::clergy(job->title)))
                continue;
            if (const auto* e = entity(id); e && e->dead)
                continue;
            goers[plan->first].push_back({std::hash<std::string>{}(id + "|pew|" + std::to_string(today)), id});
        }
        for (auto& [community, list] : goers)
        {
            const auto& rooms = churches(community);
            if (rooms.empty())
                continue;
            std::vector<std::size_t> taken(rooms.size(), 0);
            std::vector<Vec2> where;
            for (const auto& room : rooms)
                where.push_back(mapAt(room.pulpit.cell, room.pulpit.x, room.pulpit.y));
            std::sort(list.begin(), list.end());
            for (const auto& [lot, id] : list)
            {
                const auto& life = society_.state().residents.at(id);
                const auto home = mapAt(life.homeCell, life.homeX, life.homeY);
                std::size_t best = rooms.size();
                for (std::size_t r = 0; r < rooms.size(); ++r)
                    if (taken[r] < rooms[r].pews.size() &&
                        (best == rooms.size() ||
                         std::hypot(where[r].x - home.x, where[r].y - home.y) < std::hypot(where[best].x - home.x, where[best].y - home.y)))
                        best = r;
                if (best < rooms.size())
                    (*service)[id] = {rooms[best].pews[taken[best]++], ServicePlace::Seated};
            }
        }
        // The clergy: each church's first post held preaches; the others there serve at their own posts.
        for (const auto& [community, plan] : day.plans)
        {
            if (community.empty() || community.find('#') != std::string::npos || plan.kind != "rest")
                continue;
            const auto& rooms = churches(community);
            std::set<std::string> preached;
            for (const auto& p : society_.positions())
            {
                if (!Society::clergy(p.title))
                    continue;
                const auto room = std::find_if(rooms.begin(), rooms.end(), [&](const Chapel& ch) { return ch.pulpit.cell == p.work.cell; });
                if (room == rooms.end())
                    continue;
                const auto held = society_.state().careers.positions.find(p.id);
                if (held == society_.state().careers.positions.end() || held->second.holder.empty())
                    continue;
                if (const auto* e = entity(held->second.holder); e && e->dead)
                    continue;
                if (preached.insert(room->pulpit.cell).second)
                    (*service)[held->second.holder] = {room->pulpit, ServicePlace::Preaching};
                else
                    (*service)[held->second.holder] = {p.work, ServicePlace::Serving};
            }
        }
        service_ = std::move(service);
    }
    day.service = service_;
}

DayPlan World::dayPlan(const std::string& community)
{
    DayPlan plan;
    const auto today = std::int64_t(std::floor(calendarDays_));
    const auto date = calendar::calendarAt(calendarDays_);
    const auto called = std::find_if(festivals_.begin(), festivals_.end(), [&](const CalledFestival& f) {
        return f.day == today && f.community == community;
    });
    const int weekday = calendar::weekdayOf(calendarDays_);
    if (called != festivals_.end() || calendar::festivalDay(calendarDays_))
    {
        plan.kind = "festival";
        plan.name = called != festivals_.end() && !called->name.empty() ? called->name
                                                                          : festivalName(community, int(date.season));
    }
    else if (weekday == calendar::Marketday)
        plan.kind = "market";
    else if (weekday == calendar::Restday)
        plan.kind = "rest";
    // The square's built stalls (where its shopkeepers set up on Marketday), and its crowd on a market or festival
    // day. A place with no stalls built has no market, and its Marketday is an ordinary working day (Docs/Design/39).
    const auto& sq = square(community);
    if (plan.kind == "market" && sq.stalls.empty())
        plan.kind = "work";
    if (sq.found)
    {
        plan.stalls = sq.stalls;
        if (plan.kind == "market" && !letStalls_.empty())   // (Spots let to players: doc 54, 3.)
            plan.stalls.erase(std::remove_if(plan.stalls.begin(), plan.stalls.end(),
                                             [&](const Spot& s) { return letStalls_.count(stallKey(s)) > 0; }),
                              plan.stalls.end());
        plan.foul = skyAt(sq.at.cell, {sq.at.x, sq.at.y}) == 2;   // (The weather over the square itself.)
        // A market cover standing (doc 57, Phase 4): stalls set up under it in snow and storms (worn, every other day).
        if (plan.foul)
            if (const auto cover = works_.covers.find(community);
                cover != works_.covers.end() && (cover->second >= 1 || (cover->second > 0 && today % 2 == 0)))
                plan.foul = false;
        if (plan.foul && plan.kind == "market")
            foulMarkets_[community].insert(today);      // (Counted for the town's proposals.)
        if (plan.kind == "market" || plan.kind == "festival")
            plan.crowd = sq.crowd;
    }
    if (plan.kind == "rest")
        if (const auto& ch = chapel(community); ch.found)
        {
            plan.pews = ch.pews;
            plan.pulpit = ch.pulpit;
        }
    return plan;
}

std::string World::stallKey(const Spot& s)
{
    return s.cell + "|" + std::to_string(int(std::floor(s.x))) + "|" + std::to_string(int(std::floor(s.y)));
}

void World::setLetStalls(std::set<std::string> keys)
{
    if (keys == letStalls_)
        return;
    letStalls_ = std::move(keys);
    ++festivalsChanged_;                            // (Plans made again: merchants set up at the spots left.)
}

std::string World::dayLabel(const std::string& cellId)
{
    const auto plan = dayPlan(communityOf(cellId));
    const auto weekday = calendar::weekdayName(calendar::weekdayOf(calendarDays_));
    if (plan.kind == "festival")
        return weekday + " · " + plan.name;
    return weekday;
}

Result World::callFestival(const std::string& community, const std::string& name, int inDays)
{
    if (inDays < 0 || inDays > 30)
        return {false, "A festival can be called for today or up to thirty days ahead.", community};
    bool known = community.empty();
    for (const auto& [id, life] : society_.state().residents)
        known |= lawTown(life.homeCell) == community;
    if (!known)
        return {false, "Nobody lives in " + community + ".", community};
    std::string clean;
    for (const char ch : name.substr(0, 60))
        if (static_cast<unsigned char>(ch) >= 0x20)
            clean += ch;
    const auto day = std::int64_t(std::floor(calendarDays_)) + inDays;
    festivals_.erase(std::remove_if(festivals_.begin(), festivals_.end(),
                                    [&](const CalledFestival& f) {
                                        return (f.day == day && f.community == community) ||
                                               f.day < std::int64_t(std::floor(calendarDays_)) - 1;
                                    }),
                     festivals_.end());
    if (festivals_.size() >= std::size_t(FestivalsKept))
        return {false, "Too many festivals are called already.", community};
    festivals_.push_back({community, clean, day});
    ++festivalsChanged_;
    const auto& title = clean.empty() ? festivalName(community, int(calendar::calendarAt(double(day)).season)) : clean;
    recordEvent({"festival called", community, {}, {}, 0, 0, {}, 0, 0, title});
    return {true, title + (inDays ? " is called for " + std::to_string(inDays) + " days from now." : " is called for today, from noon."),
            community};
}

void World::findMarkets()
{
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (today == marketsDay_)
        return;
    marketsDay_ = today;
    otherSquares_.clear();
    markets_.reset();
    // Stallholders: merchants who work in the open, in a community. Those close together keep one market.
    struct Market
    {
        std::string community;
        double x = 0, y = 0;                        // (Its first stall, on the map.)
        std::map<std::string, int> cells;
        std::vector<const Position*> stands;
    };
    std::vector<Market> found;
    for (const auto& p : society_.positions())
    {
        if (p.role != "merchant")
            continue;
        const auto* c = cell(p.work.cell);
        if (!c || !c->outdoors)
            continue;
        const auto community = lawTown(p.work.cell);
        if (community.empty())
            continue;
        const double x = c->worldX + p.work.x, y = c->worldY + p.work.y;
        auto m = std::find_if(found.begin(), found.end(), [&](const Market& m) {
            return m.community == community && std::hypot(m.x - x, m.y - y) <= StallReach;
        });
        if (m == found.end())
            m = found.insert(found.end(), Market{community, x, y, {}, {}});
        ++m->cells[p.work.cell];
        m->stands.push_back(&p);
    }
    // Each besides its community's square, with stalls built: a square of its own, in the cell most of it is in.
    auto markets = std::make_shared<Markets>();
    for (const auto& m : found)
    {
        const auto& main = square(m.community);
        if (!main.found)
            continue;
        const auto* mainCell = cell(main.at.cell);
        if (!mainCell || std::hypot(mainCell->worldX + main.at.x - m.x, mainCell->worldY + main.at.y - m.y) <= StallReach)
            continue;
        const auto most = std::max_element(m.cells.begin(), m.cells.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        const auto stand = std::find_if(m.stands.begin(), m.stands.end(), [&](const Position* p) { return p->work.cell == most->first; });
        MarketSquare sq;
        fillSquare(sq, most->first, {(*stand)->work.x, (*stand)->work.y});
        if (!sq.found || sq.stalls.empty())
            continue;
        auto& others = otherSquares_[m.community];
        others.push_back(std::move(sq));
        auto& list = markets->squares[m.community];
        if (list.empty())
            list.push_back({m.community, {main.at.cell, mainCell->worldX + main.at.x, mainCell->worldY + main.at.y}});
        const auto* c = cell(others.back().at.cell);
        list.push_back({m.community + "#" + std::to_string(others.size()),
                        {others.back().at.cell, c->worldX + others.back().at.x, c->worldY + others.back().at.y}});
    }
    if (markets->squares.empty())
        return;
    // Where those communities' places are on the map: an outdoor cell by its corner, an interior by its door (one
    // upstairs, or further in, by the door that leads to it in turn).
    for (const auto& [id, c] : cells_)
        if (c.outdoors)
            if (auto community = lawTown(id); markets->squares.count(community))
                markets->places[id] = {std::move(community), c.worldX, c.worldY, true};
    for (int depth = 0; depth < 4; ++depth)
    {
        bool more = false;
        for (const auto& [id, d] : doors_)
        {
            if (markets->places.count(d.targetCell))
                continue;
            const auto from = markets->places.find(d.cellId);
            if (from == markets->places.end())
                continue;
            if (auto community = lawTown(d.targetCell); markets->squares.count(community))
            {
                const auto& f = from->second;
                markets->places[d.targetCell] = {std::move(community), f.outdoors ? f.x + d.position.x : f.x,
                                                 f.outdoors ? f.y + d.position.y : f.y, false};
                more = true;
            }
        }
        if (!more)
            break;
    }
    markets_ = std::move(markets);
}

void World::planDays()
{
    // Again every ten game minutes (the weather at the squares changes), and whenever a festival is called.
    const auto at = std::int64_t(std::floor(calendarDays_ * 144));
    if (at == plannedAt_ && festivalsChanged_ == plannedFor_)
        return;
    plannedAt_ = at;
    plannedFor_ = festivalsChanged_;
    findMarkets();
    LifeDay day;
    std::set<std::string> communities{""};
    for (const auto& [id, life] : society_.state().residents)
        communities.insert(lawTown(life.homeCell));
    const auto today = std::int64_t(std::floor(calendarDays_));
    const double hour = (calendarDays_ - std::floor(calendarDays_)) * 24;
    for (const auto& community : communities)
    {
        auto plan = dayPlan(community);
        if (plan.kind == "festival" && hour >= 12 && !community.empty() &&
            festivalsBegun_.insert(community + "|" + std::to_string(today)).second)
            recordEvent({"festival", community, {}, square(community).at.cell, 0, 0, {}, 0, 0, plan.name});
        // A city's other markets (doc 60): the same day, each with its own stalls and crowd on Marketday.
        if (const auto others = otherSquares_.find(community); others != otherSquares_.end())
            for (std::size_t i = 0; i < others->second.size(); ++i)
            {
                auto theirs = plan;
                if (plan.kind == "market")
                {
                    const auto& sq = others->second[i];
                    theirs.stalls = sq.stalls;
                    theirs.crowd = sq.crowd;
                    theirs.foul = skyAt(sq.at.cell, {sq.at.x, sq.at.y}) == 2;
                }
                day.plans.emplace(community + "#" + std::to_string(i + 1), std::move(theirs));
            }
        day.plans.emplace(community, std::move(plan));
    }
    day.markets = markets_;
    seatService(day, today);
    if (festivalsBegun_.size() > 4096)
        festivalsBegun_.clear();
    day.communityOf = [this](const std::string& cellId) { return lawTown(cellId); };
    day.houseOf = [this](const std::string& cellId) { return districtHouse(cellId); };
    day.sky = [this](const Spot& at) { return skyAt(at.cell, {at.x, at.y}); };   // The weather where they'd be.
    society_.setDay(std::move(day));
}

std::string World::districtHouse(const std::string& cellId) const
{
    // Out from the cell through the doors that lead into it, the nearest first (doors in id order), stopping at the first
    // outdoor cell: a shop's own cell rarely carries its district's claim (in Ridgemere 2.0 most interiors are claimed by
    // no House while the street outside is), so the street's is the district's.
    std::vector<std::string> queue{cellId};
    std::set<std::string> seen{cellId};
    for (std::size_t at = 0; at < queue.size() && at < 16; ++at)
    {
        const auto* c = cell(queue[at]);
        if (!c)
            continue;
        for (const auto& claim : c->factionClaims)
            if (const auto f = factions_.find(claim); f != factions_.end() && f->second.name.rfind("House ", 0) == 0)
                return f->second.name.substr(6);
        if (c->outdoors)
            return {};                              // (The street has no House's claim: none.)
        for (const auto& [id, d] : doors_)
            if (d.targetCell == queue[at] && d.cellId != queue[at] && seen.insert(d.cellId).second)
                queue.push_back(d.cellId);
    }
    return {};
}
} // namespace ratw
