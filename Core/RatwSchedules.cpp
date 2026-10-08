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

int World::skyOf(const std::string& cellId) const
{
    const auto* c = cell(cellId);
    if (!c)
        return 0;
    if (!c->outdoors)
        return -1;
    switch (c->weather)
    {
    case Weather::Rain:
        return 1;
    case Weather::Snow:
    case Weather::Storm:
    case Weather::Sandstorm:
        return 2;
    default:
        return 0;
    }
}

const World::MarketSquare& World::square(const std::string& community)
{
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (today != squaresDay_)
    {
        squares_.clear();                           // Worked out afresh each day: markets move with their merchants.
        squaresDay_ = today;
    }
    if (const auto found = squares_.find(community); found != squares_.end())
        return found->second;
    auto& sq = squares_[community];
    if (community.empty())
        return sq;                                  // Those who live outside any town have no market to go to.
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
    if (at.empty() || !ensureLoaded(at).ok)
        return sq;
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
        return sq;
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
                    !blockedByDoor(at, p) && (region < 0 || regionAt(*c, p) == region))
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
                if (!t || t->solid || homes.count({tx, ty}) || blockedByDoor(at, p) || nearPortal(at, p, 1.2) ||
                    (region >= 0 && regionAt(*c, p) != region) || stallSpots.count({tx, ty}) || keepers.count({tx, ty}))
                    continue;
                if (int(sq.crowd.size()) < Crowd)
                    sq.crowd.push_back({at, p.x, p.y});
            }
    return sq;
}

const World::Chapel& World::chapel(const std::string& community)
{
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (today != chapelsDay_)
    {
        chapels_.clear();
        chapelsDay_ = today;
    }
    if (const auto found = chapels_.find(community); found != chapels_.end())
        return found->second;
    auto& ch = chapels_[community];
    if (community.empty())
        return ch;
    // The church is where the town's clergy work (a priest's, a chapel keeper's post): the first such indoors.
    for (const auto& p : society_.positions())
    {
        if (!Society::clergy(p.title) || lawTown(p.work.cell) != community)
            continue;
        const auto* c = cell(p.work.cell);
        if (!c || c->outdoors || !ensureLoaded(p.work.cell).ok)
            continue;
        c = cell(p.work.cell);
        ch.found = true;
        ch.pulpit = p.work;
        // Its seats: open floor a few strides from the pulpit, up to sixty of them.
        for (int y = 1; y < c->height - 1 && ch.pews.size() < 60; ++y)
            for (int x = 1; x < c->width - 1 && ch.pews.size() < 60; ++x)
            {
                const Vec2 at{x + .5, y + .5};
                if (std::hypot(at.x - p.work.x, at.y - p.work.y) >= 2.5 && passable(p.work.cell, at))
                    ch.pews.push_back({p.work.cell, at.x, at.y});
            }
        break;
    }
    return ch;
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
        plan.foul = skyOf(sq.at.cell) == 2;
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

void World::planDays()
{
    // Again every ten game minutes (the weather at the squares changes), and whenever a festival is called.
    const auto at = std::int64_t(std::floor(calendarDays_ * 144));
    if (at == plannedAt_ && festivalsChanged_ == plannedFor_)
        return;
    plannedAt_ = at;
    plannedFor_ = festivalsChanged_;
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
        day.plans.emplace(community, std::move(plan));
    }
    if (festivalsBegun_.size() > 4096)
        festivalsBegun_.clear();
    if (!workGrounds_ && townsReady_)
        findWorkGrounds();                          // The country each town works (doc 42, Phase 3b), once.
    day.grounds = workGrounds_;
    day.harvest = [this](const std::string& who, const WorkGround& at, int season) { return harvestAt(who, at, season); };
    day.communityOf = [this](const std::string& cellId) { return lawTown(cellId); };
    day.sky = [this](const std::string& cellId) { return skyOf(cellId); };
    society_.setDay(std::move(day));
}
} // namespace ratw
