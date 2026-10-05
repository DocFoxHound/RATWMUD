#include "RatwWorld.h"

#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwNames.h"
#include "RatwWild.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

// Hunting and foraging (Docs/Design/41-hunting-and-foraging.md; data in Data/Wild).
//
// A hunt is a fight (RatwBattle.cpp) whose other side is animals. The world outside sees the hunters frozen in the red
// square as for any fight. Animals exist only inside their hunt: they're made when it starts or wander in during it,
// and go when it ends or they get away. Nothing of them is saved. How many a hunt finds depends on the ground of its
// arena, how hard the place has been hunted lately and how many are playing. The hardest single blow on an animal
// decides how much of it is worth taking; fire spoils pelts and meat.
//
// Noticing is a placeholder (animalNotices) until doc 40's sneaking replaces it.
namespace ratw
{
namespace
{
constexpr double Pi = 3.14159265358979323846;
std::uint64_t mix(const std::string& a, std::int64_t b)
{
    std::uint64_t h = 1469598103934665603ULL ^ std::uint64_t(b) * 0x9E3779B97F4A7C15ULL;
    for (unsigned char c : a)
        h = (h ^ c) * 1099511628211ULL;
    h ^= h >> 31;
    h *= 0xBF58476D1CE4E5B9ULL;
    return h ^ (h >> 29);
}
double odds(const std::string& a, std::int64_t b)
{
    return double(mix(a, b) % 100000) / 100000.0;
}
int apart(int ax, int ay, int bx, int by)
{
    return std::max(std::abs(ax - bx), std::abs(ay - by));
}
// A whole count from an expected one: the fraction by chance.
int whole(double expected, double roll)
{
    const double base = std::floor(std::max(0.0, expected));
    return int(base) + (roll < expected - base ? 1 : 0);
}
bool skinOf(const std::string& item)
{
    return item == "hide" || item == "feathers" || item == "bristles" ||
           (item.size() > 5 && item.compare(item.size() - 5, 5, "_pelt") == 0);
}
// "Raw meat (cut)" -> "raw meat (cut)", for the middle of a sentence.
std::string lower(std::string name)
{
    if (!name.empty())
        name[0] = char(std::tolower(static_cast<unsigned char>(name[0])));
    return name;
}
std::string listed(const std::vector<std::string>& parts)
{
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i)
        out += (i == 0 ? "" : i + 1 == parts.size() ? " and " : ", ") + parts[i];
    return out;
}
} // namespace

// ------------------------------------------------------------------ The ground, the game, the pressure

std::map<std::string, double> World::groundIn(const std::string& cellId, int x0, int y0, int w, int h) const
{
    std::map<std::string, double> out;
    const auto* c = cell(cellId);
    if (!c || w <= 0 || h <= 0)
        return out;
    int n = 0;
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x)
            if (const auto* t = c->tile(x, y))
            {
                ++n;
                if (const auto& g = wild::groundOf(t->glyph); !g.empty())
                    out[g] += 1;
            }
    for (auto& [g, count] : out)
        count /= std::max(1, n);
    return out;
}

double World::huntPressure(const std::string& cellId) const
{
    const auto found = huntKills_.find(cellId);
    if (found == huntKills_.end())
        return 0;
    double p = 0;
    for (double day : found->second)
        p += std::exp(-std::max(0.0, calendarDays_ - day) / wild::population().recoveryDays);
    return p;
}

int World::playersOnline() const
{
    int n = 0;
    for (const auto& [id, e] : entities_)
        n += !e.npc && !e.dead;
    return n;
}

double World::huntExpected(const std::string& cellId, const std::map<std::string, double>& ground) const
{
    const auto& pop = wild::population();
    double wildShare = 0, fit = 0;
    for (const auto& [g, share] : ground)
        wildShare += share;
    if (wildShare < pop.wildFrom)
        return 0;
    for (const auto& s : wild::species())
        for (const auto& [g, w] : s.habitats)
            if (const auto at = ground.find(g); at != ground.end())
                fit = std::max(fit, std::min(1.0, w * at->second));
    if (fit <= 0)
        return 0;
    const double pressure = 1 / (1 + huntPressure(cellId) / pop.capacity);
    const double players = std::min(pop.most, 1 + playersOnline() / pop.perPlayers);
    return pop.expected * wildShare * pressure * players;
}

World::WildHere World::wildAround(const std::string& player) const
{
    WildHere here;
    const auto* p = entity(player);
    const auto* c = p ? cell(p->cellId) : nullptr;
    if (!p || !c || !c->loaded)
        return here;
    if (!c->outdoors || townOf(p->cellId))
    {
        here.huntWhy = "There is no game to hunt here; go out into the wild.";
        return here;
    }
    // Foraging: the ground right beside one.
    const int px = int(std::floor(p->position.x)), py = int(std::floor(p->position.y));
    std::vector<std::string> names;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            if (const auto* t = c->tile(px + dx, py + dy))
                for (const auto& g : wild::forage().ground)
                    if (g.tiles.find(t->glyph) != std::string::npos && std::find(names.begin(), names.end(), g.name) == names.end())
                        names.push_back(g.name);
    here.forage = !names.empty();
    here.forageWhat = listed(names);
    // Hunting: the ground of the arena a hunt would have, in blocks of 16 tiles (the ground never changes).
    const int bx = px / 16, by = py / 16;
    const auto key = p->cellId + "|" + std::to_string(bx) + "|" + std::to_string(by);
    auto cached = groundCache_.find(key);
    if (cached == groundCache_.end())
    {
        const int w = std::min(c->width, battle::ArenaWidth), h = std::min(c->height, battle::ArenaHeight);
        const int x0 = std::clamp(bx * 16 + 8 - w / 2, 0, c->width - w), y0 = std::clamp(by * 16 + 8 - h / 2, 0, c->height - h);
        cached = groundCache_.emplace(key, groundIn(p->cellId, x0, y0, w, h)).first;
    }
    const double expected = huntExpected(p->cellId, cached->second);
    here.hunt = expected > 0;
    if (!here.hunt)
        here.huntWhy = "This ground is too built up or bare for game.";
    return here;
}

// ------------------------------------------------------------------ Starting, joining, leaving

bool World::addAnimal(Battle& b, const std::string& speciesId, bool arriving)
{
    const auto* s = wild::speciesById(speciesId);
    if (!s)
        return false;
    const auto id = "wild:" + std::to_string(++nextAnimal_);
    // Somewhere open in the arena, well away from every hunter (or, wandering in, at its edge).
    const auto key = std::int64_t(time_ * 1000) + std::int64_t(nextAnimal_);
    std::pair<int, int> spot{-1, -1};
    int bestFar = -1;
    for (int attempt = 0; attempt < 300; ++attempt)
    {
        int x = b.x0 + int(mix(id + "|x", key + attempt) % std::uint64_t(std::max(1, b.w)));
        int y = b.y0 + int(mix(id + "|y", key + attempt) % std::uint64_t(std::max(1, b.h)));
        if (arriving)
        {
            switch (mix(id + "|side", key + attempt) % 4)
            {
            case 0: x = b.x0 + 1; break;
            case 1: x = b.x0 + b.w - 2; break;
            case 2: y = b.y0 + 1; break;
            default: y = b.y0 + b.h - 2; break;
            }
        }
        if (!arenaOpen(b, x, y))
            continue;
        int nearest = std::numeric_limits<int>::max();
        for (const auto& f : b.fighters)
            if (f.side == 0 && f.status != "fled")
                nearest = std::min(nearest, apart(x, y, f.x, f.y));
        if (nearest > bestFar)
        {
            bestFar = nearest;
            spot = {x, y};
        }
        if (nearest >= wild::population().nearestStart)
            break;
    }
    if (spot.first < 0)
        return false;
    Entity e;
    e.id = id;
    e.name = s->name;
    e.description = "Wild game.";
    e.cellId = b.cellId;
    e.position = {spot.first + .5, spot.second + .5};
    e.npc = true;
    e.transient = true;
    e.age = 30;                                     // (A grown animal: fights' age rules are for wolves.)
    e.dexterity = s->dex;
    e.strength = s->strength;
    e.facing = double(mix(id + "|face", key) % 8) * Pi / 4;
    e.turnTarget = e.facing;
    entities_[id] = std::move(e);
    index_.dirty = true;
    enterBattle(b, id, 1, false);
    auto& f = b.fighters.back();
    f.x = spot.first;
    f.y = spot.second;
    f.lineupX = spot.first + .5;
    f.lineupY = spot.second + .5;
    f.facing = int(mix(id + "|face", key) % 8);     // Grazing, facing wherever: never turned to meet the hunter.
    animals_[id] = {speciesId, b.id, false, 0, 0, 0, {}, false};
    if (arriving)
        fightLine(b, id, {}, "join", names::capitalised(s->name) + " wanders into view.");
    return true;
}

Result World::startHunt(const std::string& player)
{
    auto* p = entity(player);
    if (!p || p->npc || p->dead)
        return {false, "No such character.", {}};
    if (p->downedLeft > 0)
        return {false, "You are down.", {}};
    if (inBattle(player))
        return {false, "You are already in a fight.", {}};
    if (custodyOf(player))
        return {false, "You are held in the gaol.", {}};
    if (const auto why = tooLoadedToFight(player); !why.empty())
        return {false, why, {}};
    if (const auto settle = settleUntil_.find(player); settle != settleUntil_.end() && time_ < settle->second)
        return {false, "You are still finding your feet.", {}};
    if (!ensureLoaded(p->cellId).ok || !cell(p->cellId))
        return {false, "This place isn't loaded.", {}};
    const auto here = wildAround(player);
    if (!here.hunt)
        return {false, here.huntWhy.empty() ? "There is no game to hunt here." : here.huntWhy, {}};
    leaveObserving(player);
    Battle b;
    b.id = "hunt-" + std::to_string(++nextBattle_);
    b.cellId = p->cellId;
    b.hunt = true;
    b.terms = "death";
    enterBattle(b, player, 0, true);
    fitArena(b);
    {
        auto& f = b.fighters.front();
        f.x = std::clamp(int(std::floor(p->position.x)), b.x0, b.x0 + b.w - 1);
        f.y = std::clamp(int(std::floor(p->position.y)), b.y0, b.y0 + b.h - 1);
        if (!arenaOpen(b, f.x, f.y, f.id))
        {
            BattleFighter probe = f;
            for (const auto& [x, y] : reachFrom(b, probe, b.w + b.h))
            {
                f.x = x;
                f.y = y;
                break;
            }
        }
    }
    // What is about: the ground of this arena, the pressure on it, the players in the world.
    const auto ground = groundIn(b.cellId, b.x0, b.y0, b.w, b.h);
    const double expected = huntExpected(b.cellId, ground);
    std::vector<std::pair<std::string, double>> weights;
    double totalWeight = 0;
    for (const auto& s : wild::species())
    {
        double w = 0;
        for (const auto& [g, hw] : s.habitats)
            if (const auto at = ground.find(g); at != ground.end())
                w += hw * at->second;
        if (w > 0)
        {
            weights.push_back({s.id, w * s.rarity});
            totalWeight += w * s.rarity;
        }
    }
    const auto key = std::int64_t(time_ * 1000);
    // Poisson-like: the expected count, give or take, by chance.
    int count = 0;
    for (double left = expected * 2, i = 0; left > 0; left -= 1, ++i)
        count += odds(b.id + "|n|" + player, key + std::int64_t(i)) < std::min(1.0, left) / 2;
    const auto pickSpecies = [&](std::int64_t salt) -> std::string {
        double r = odds(b.id + "|species", key + salt) * totalWeight;
        for (const auto& [id, w] : weights)
            if ((r -= w) <= 0)
                return id;
        return weights.empty() ? std::string() : weights.back().first;
    };
    // A trail this wolf has found near by (tracks, by its nose): what made it is likelier, and a fresh one is still here.
    const Track* follow = nullptr;
    {
        int nearestTrail = 13;
        if (const auto mine = tracks_.find(player); mine != tracks_.end())
            for (const auto& t : mine->second)
                if (t.cell == b.cellId && t.until > time_)
                    for (const auto& [tx, ty] : t.tiles)
                        if (const int d = apart(tx, ty, int(std::floor(p->position.x)), int(std::floor(p->position.y))); d < nearestTrail)
                        {
                            nearestTrail = d;
                            follow = &t;
                        }
    }
    std::string followed;
    if (follow)
        if (const auto* s = wild::speciesById(follow->species))
        {
            const double boost = follow->fresh ? 6 : 3;
            auto w = std::find_if(weights.begin(), weights.end(), [&](const auto& x) { return x.first == s->id; });
            if (w == weights.end())
            {
                weights.push_back({s->id, s->rarity});
                totalWeight += s->rarity;
                w = std::prev(weights.end());
            }
            totalWeight += w->second * (boost - 1);
            w->second *= boost;
            if (follow->fresh)
                count = std::max(count, 1);
            followed = s->name;
        }
    if (count == 0 || weights.empty())
    {
        const bool hunted = huntPressure(p->cellId) >= 1;
        return {false, hunted ? "You cast about, but this ground has been hunted lately: no game shows itself."
                              : "You cast about for a while, but no game shows itself here.",
                {}};
    }
    for (int i = 0; i < count; ++i)
        addAnimal(b, i == 0 && follow && follow->fresh ? follow->species : pickSpecies(i), false);
    if (follow)
    {
        auto& mine = tracks_[player];
        mine.erase(std::remove_if(mine.begin(), mine.end(), [&](const Track& t) { return &t == follow; }), mine.end());
    }
    lineUp(b);
    stop(player);
    setClientWalks(player, false);
    b.opening = player;                             // The animals carry on grazing until the hunter's first turn is over.
    recordEvent({"hunt", player, {}, b.cellId, 0, 0, {}, count, 0, b.id});
    fightLine(b, player, {}, "start", p->name + " goes out after game.");
    std::vector<std::string> seen;
    for (const auto& f : b.fighters)
        if (const auto a = animals_.find(f.id); a != animals_.end())
            if (const auto* s = wild::speciesById(a->second.species))
                seen.push_back(s->name);
    huntArrivals_[b.id] = {time_ + wild::population().arrivalSeconds, expected};
    battles_.push_back(std::move(b));
    return {true, (followed.empty() ? std::string() : "You follow " + followed + "'s trail. ") + "You go out after game. Ahead: " +
                      listed(seen) + ", not yet aware of you.",
            {}};
}

std::string World::huntJoinRefusal(const Battle& b, const std::string& id) const
{
    if (!b.hunt)
        return {};
    const auto* e = entity(id);
    for (const auto& f : b.fighters)
        if (f.side == 0 && f.status != "fled")
        {
            if (e && e->npc && e->leaderId == f.id)
                return {};                          // A companion comes along with the one it follows.
            if (friends_ && friends_(f.id, id))
                return {};
        }
    return "That is someone else's hunt: only their party or Chapter may join it.";
}

Result World::leaveHunt(const std::string& player)
{
    auto* b = battleFor(player);
    if (!b || !b->hunt)
        return {false, "You aren't hunting.", {}};
    auto* f = b->fighter(player);
    if (!f || f->status != "fighting")
        return {false, "You can't leave the hunt now.", {}};
    if (f->acting)
        endTurn(*b, *f);
    leaveArena(*b, *f, true);
    fightLine(*b, player, {}, "flee", (entity(player) ? entity(player)->name : std::string("A hunter")) + " gives up the hunt.");
    checkOver(*b);
    return {true, "You give up the hunt.", {}};
}

// ------------------------------------------------------------------ Animals in the arena

std::string World::animalOf(const std::string& id) const
{
    const auto found = animals_.find(id);
    return found == animals_.end() ? std::string() : found->second.species;
}

bool World::animalUnaware(const std::string& id) const
{
    const auto found = animals_.find(id);
    return found != animals_.end() && !found->second.alert;
}

bool World::animalNotices(const Battle& b, const BattleFighter& animal) const
{
    // A placeholder until doc 40 (Docs/Design/40-sneaking.md, "The seam hunting leaves"): a hunter within the species'
    // alert distance (twice that for one at a sprint) is noticed, as is any hunter next to it.
    const auto a = animals_.find(animal.id);
    const auto* s = a == animals_.end() ? nullptr : wild::speciesById(a->second.species);
    if (!s)
        return true;
    for (const auto& f : b.fighters)
    {
        if (f.side == animal.side || f.status != "fighting")
            continue;
        const auto* e = entity(f.id);
        const double reach = s->alert * (e && fightPace(*e) >= 8 ? 2 : 1);
        const int d = apart(f.x, f.y, animal.x, animal.y);
        if (d <= 1 || d <= reach)
            return true;
    }
    return false;
}

bool World::animalTurn(Battle& b, BattleFighter& f)
{
    const auto a = animals_.find(f.id);
    if (a == animals_.end())
        return false;
    auto* e = entity(f.id);
    const auto* s = wild::speciesById(a->second.species);
    if (!e || !s || f.status != "fighting")
    {
        if (f.acting)
            endTurn(b, f);                  // (Quietly: game doesn't announce its waiting.)
        return true;
    }
    if (b.over || !f.acting)
        return true;
    std::vector<const BattleFighter*> hunters;
    for (const auto& o : b.fighters)
        if (o.side != f.side && o.status == "fighting")
            hunters.push_back(&o);
    if (hunters.empty())
    {
        endTurn(b, f);                  // (Quietly: game doesn't announce its waiting.)
        return true;
    }
    if (!a->second.alert && animalNotices(b, f))
    {
        a->second.alert = true;
        fightLine(b, f.id, {}, "notice",
                  names::capitalised(s->name) + (s->temper == "fierce" ? " catches your scent and turns to face you." : " startles."));
    }
    const auto nearest = [&](int x, int y) {
        int best = std::numeric_limits<int>::max();
        for (const auto* o : hunters)
            best = std::min(best, apart(x, y, o->x, o->y));
        return best;
    };
    const auto key = std::int64_t(f.turnsTaken) * 131 + b.turns;
    if (!a->second.alert)
    {
        // Grazing: now and then a step or two, facing wherever it goes.
        if (!f.moved && odds(f.id + "|graze", key) < .4)
        {
            std::vector<std::pair<int, int>> near;
            for (const auto& [x, y] : battleReach(f.id))
                if (apart(x, y, f.x, f.y) <= 2 && (x != f.x || y != f.y))
                    near.push_back({x, y});
            if (!near.empty())
            {
                const auto& to = near[mix(f.id + "|to", key) % near.size()];
                battleMove(f.id, to.first, to.second);
            }
            if (!f.walk.empty())
                return true;
        }
        if (!b.over && f.acting)
            endTurn(b, f);                  // (Quietly: game doesn't announce its waiting.)
        return true;
    }
    const bool cornered = nearest(f.x, f.y) <= 1 || e->hurt > 0;
    const bool fights = s->temper == "fierce" || (s->temper == "cornered" && cornered);
    if (fights)
    {
        // At the nearest hunter.
        const BattleFighter* mark = hunters.front();
        for (const auto* o : hunters)
            if (apart(f.x, f.y, o->x, o->y) < apart(f.x, f.y, mark->x, mark->y))
                mark = o;
        if (!f.moved && apart(f.x, f.y, mark->x, mark->y) > 1)
        {
            std::pair<int, int> best{f.x, f.y};
            int bestD = apart(f.x, f.y, mark->x, mark->y);
            for (const auto& [x, y] : battleReach(f.id))
                if (const int d = apart(x, y, mark->x, mark->y); d < bestD && d >= 1)
                {
                    bestD = d;
                    best = {x, y};
                }
            if (best != std::pair<int, int>{f.x, f.y})
                battleMove(f.id, best.first, best.second);
            if (!f.walk.empty())
                return true;
        }
        if (!b.over && f.acting && apart(f.x, f.y, mark->x, mark->y) == 1 && !e->exhausted && e->stamina >= battle::BiteStamina)
            battleAct(f.id, "bite", mark->id);
        if (!b.over && f.acting)
            endTurn(b, f);                  // (Quietly: game doesn't announce its waiting.)
        return true;
    }
    // Away: to the edge, as far from the hunters as it can get, and off.
    if (!f.moved && !b.onEdge(f.x, f.y))
    {
        std::pair<int, int> best{f.x, f.y};
        int bestScore = std::numeric_limits<int>::min();
        for (const auto& [x, y] : battleReach(f.id))
        {
            const int edge = std::min({x - b.x0, y - b.y0, b.x0 + b.w - 1 - x, b.y0 + b.h - 1 - y});
            const int score = -edge * 4 + nearest(x, y);
            if (score > bestScore)
            {
                bestScore = score;
                best = {x, y};
            }
        }
        if (best != std::pair<int, int>{f.x, f.y})
            battleMove(f.id, best.first, best.second);
        if (!f.walk.empty())
            return true;
    }
    if (!b.over && f.acting)
    {
        if (b.onEdge(f.x, f.y))
        {
            fightLine(b, f.id, {}, "flee", names::capitalised(s->name) + " gets away.");
            battleAct(f.id, "flee");
        }
        if (!b.over && f.acting)
            endTurn(b, f);                  // (Quietly: game doesn't announce its waiting.)
    }
    return true;
}

double World::huntBlow(Battle& b, BattleFighter& t, double damage, double downedBase, const std::string& by)
{
    const auto a = animals_.find(t.id);
    if (!b.hunt || a == animals_.end())
        return damage;
    const auto* s = wild::speciesById(a->second.species);
    if (!s)
        return damage;
    // The hardest single blow, and how much was fire: they decide what of it is worth taking (huntKill).
    if (downedBase == battle::DownedFire)
        a->second.fire += damage;
    else if (damage > a->second.best)
    {
        a->second.best = damage;
        a->second.struckUnaware = !a->second.alert;
    }
    a->second.total += damage;
    if (!by.empty())
        a->second.lastBy = by;
    a->second.alert = true;                         // Hurt, it knows.
    return damage * 100 / s->health;                // Its own health, on the fighters' scale of 100.
}

bool World::huntKill(Battle& b, BattleFighter& f, const std::string& by)
{
    const auto a = animals_.find(f.id);
    if (!b.hunt || a == animals_.end())
        return false;
    auto* e = entity(f.id);
    const auto* s = wild::speciesById(a->second.species);
    f.status = "dead";
    f.meter = 0;
    f.walk.clear();
    f.burning = 0;
    f.bleeding = 0;
    const bool wasActing = f.acting;
    f.acting = false;
    if (e)
    {
        e->dead = true;
        e->hurt = 100;
        e->posture = "lying";
    }
    if (wasActing)
        endTurn(b, f);
    if (!s)
        return true;
    // Who brought it down: the one whose blow did it, else whoever last struck it (its burns), else the first hunter.
    std::string killer = by;
    if (killer.empty() || !entity(killer) || entity(killer)->npc)
        killer = a->second.lastBy;
    if (killer.empty() || !entity(killer) || entity(killer)->npc)
        for (const auto& o : b.fighters)
            if (o.side == 0 && entity(o.id) && !entity(o.id)->npc)
            {
                killer = o.id;
                break;
            }
    huntKills_[b.cellId].push_back(calendarDays_);
    // How clean a kill: the hardest single blow against its health. Fire spoils the skins (wholly past half the damage)
    // and chars a share of the meat.
    const double clean = a->second.best / s->health;
    const char* grade = clean >= 1 ? "a clean kill" : clean >= .5 ? "a good kill" : clean >= .25 ? "a rough kill" : "a ragged kill";
    const double share = clean >= 1 ? 1 : clean >= .5 ? .8 : clean >= .25 ? .6 : .4;
    const double fire = a->second.total > 0 ? a->second.fire / a->second.total : 0;
    // And of what quality (doc 35, Part 4): a clean kill is fine, masterwork if one blow did it before it knew; a ragged
    // one crude; a fire-touched kill no better than common.
    int quality = clean >= 1 ? (a->second.struckUnaware && fire <= 0 ? 3 : 2) : clean >= .25 ? 1 : 0;
    if (fire > 0)
        quality = std::min(quality, 1);
    std::vector<std::string> taken, spoilt;
    for (const auto& [item, n] : s->yield)
    {
        double factor = share;
        if (fire > 0)
            factor *= skinOf(item) ? std::max(0.0, 1 - 2 * fire) : item == "raw_meat" ? 1 - fire / 2 : 1;
        const int got = whole(n * factor, odds(f.id + "|" + item, std::int64_t(calendarDays_ * 1000)));
        const auto kind = quality == 3 ? items::withMaker(items::withQuality(item, 3), killer) : items::withQuality(item, quality);
        if (got > 0 && entity(killer) && society_.create(killer, kind, got, "hunted"))
            taken.push_back(std::to_string(got) + " " + lower(Society::itemName(kind)));
        else if (fire > 0 && skinOf(item))
            spoilt.push_back(lower(Society::itemName(item)));
    }
    const std::string name = names::capitalised(s->name);
    fightLine(b, f.id, killer, "death", name + " falls: " + grade + (fire >= .5 ? ", burnt." : "."));
    recordEvent({"hunted", killer, a->second.species, b.cellId, 0, 0, {}, int(taken.size()), 0, grade});
    if (!killer.empty() && entity(killer) && !entity(killer)->npc)
    {
        std::string words = "You bring down " + s->name + ": " + grade + ". ";
        words += taken.empty() ? "Nothing of it is worth taking." : "You take " + listed(taken) + ".";
        if (!spoilt.empty())
            words += " The fire has spoilt the " + listed(spoilt) + ".";
        notice(killer, words);
    }
    return true;
}

void World::huntBanner(Battle& b)
{
    if (!b.hunt)
        return;
    int taken = 0, away = 0;
    for (const auto& f : b.fighters)
        if (animals_.count(f.id))
        {
            taken += f.status == "dead";
            away += f.status == "fled";
        }
    bool huntersStand = false;
    for (const auto& f : b.fighters)
        huntersStand = huntersStand || (f.side == 0 && f.status == "fighting");
    b.banner = std::string(huntersStand || taken > 0 ? "The hunt is over" : "The hunt goes badly") + " · " +
               std::to_string(taken) + " taken, " + std::to_string(away) + " got away";
    if (!b.log.empty() && b.log.back().kind == "over")
        b.log.back().text = b.banner + ".";
}

void World::endHunt(Battle& b)
{
    if (!b.hunt)
        return;
    std::vector<std::string> gone;
    for (const auto& [id, a] : animals_)
        if (a.battle == b.id)
            gone.push_back(id);
    for (const auto& id : gone)
    {
        entities_.erase(id);
        animals_.erase(id);
    }
    index_.dirty = true;
    huntArrivals_.erase(b.id);
}

void World::tendHunts()
{
    for (auto it = tracks_.begin(); it != tracks_.end();)
    {
        auto& list = it->second;
        list.erase(std::remove_if(list.begin(), list.end(), [&](const Track& t) { return t.until <= time_; }), list.end());
        it = list.empty() ? tracks_.erase(it) : std::next(it);
    }
    if (animals_.empty() && huntArrivals_.empty())
        return;
    const auto& pop = wild::population();
    for (auto& b : battles_)
    {
        if (!b.hunt || b.over)
            continue;
        auto arrival = huntArrivals_.find(b.id);
        if (arrival == huntArrivals_.end() || time_ < arrival->second.first)
            continue;
        arrival->second.first = time_ + pop.arrivalSeconds;
        int standing = 0;
        for (const auto& f : b.fighters)
            standing += animals_.count(f.id) && f.status == "fighting";
        if (standing == 0 || standing >= pop.atOnce ||
            odds(b.id + "|arrive", std::int64_t(time_)) >= arrival->second.second / 6)
            continue;
        const auto ground = groundIn(b.cellId, b.x0, b.y0, b.w, b.h);
        std::vector<std::pair<std::string, double>> weights;
        double total = 0;
        for (const auto& s : wild::species())
        {
            double w = 0;
            for (const auto& [g, hw] : s.habitats)
                if (const auto at = ground.find(g); at != ground.end())
                    w += hw * at->second;
            if (w > 0)
            {
                weights.push_back({s.id, w * s.rarity});
                total += w * s.rarity;
            }
        }
        double r = odds(b.id + "|which", std::int64_t(time_)) * total;
        for (const auto& [id, w] : weights)
            if ((r -= w) <= 0)
            {
                addAnimal(b, id, true);
                break;
            }
    }
    for (auto it = huntArrivals_.begin(); it != huntArrivals_.end();)
        it = battle(it->first) ? std::next(it) : huntArrivals_.erase(it);
    // Those that got away are gone into the wild: nothing of them stays in the world.
    std::vector<std::string> gone;
    for (const auto& [id, a] : animals_)
    {
        const auto* b = battle(a.battle);
        const auto* f = b ? b->fighter(id) : nullptr;
        if (!b || !f || f->status == "fled")
            gone.push_back(id);
    }
    for (const auto& id : gone)
    {
        entities_.erase(id);
        animals_.erase(id);
        index_.dirty = true;
    }
    // Old kills stop counting once they have faded to nothing.
    for (auto it = huntKills_.begin(); it != huntKills_.end();)
    {
        auto& days = it->second;
        days.erase(std::remove_if(days.begin(), days.end(), [&](double d) { return calendarDays_ - d > pop.recoveryDays * 6; }), days.end());
        it = days.empty() ? huntKills_.erase(it) : std::next(it);
    }
}

// ------------------------------------------------------------------ Tracks

Result World::smellTracks(const std::string& player)
{
    // Out in the wild only, and with a nose that works: elsewhere the smell command says what it always has.
    auto* p = entity(player);
    const auto* c = p ? cell(p->cellId) : nullptr;
    if (!p || p->npc || !c || !c->loaded || !c->outdoors || townOf(p->cellId) || inBattle(player) || p->smell <= 0 ||
        p->noseHealth <= 0)
        return {false, {}, {}};
    // How far the nose reaches over the ground: its keenness, the tracking skill, and the weather (rain washes trails).
    const auto env = environmentAt(p->cellId, p->position);
    const double nose = p->smell * p->noseHealth * (1 + .75 * p->scentSkill / 100) * env.scent;
    const double reach = std::clamp(12 * nose, 4.0, 30.0);
    const int px = int(std::floor(p->position.x)), py = int(std::floor(p->position.y));
    // Trails are the ground's, not the wolf's: the same for everyone, changing every four hours, and as many as the
    // game there (the ground, the pressure on it, the players about; doc 41) would leave.
    constexpr int Block = 16;
    const auto window = std::int64_t(std::floor(calendarDays_ * 6));
    const double perBlock = double(Block * Block) / (battle::ArenaWidth * battle::ArenaHeight) * 3;
    std::vector<Track> found;
    for (int by = std::max(0, int((py - reach) / Block)); by <= std::min(c->height - 1, int(py + reach)) / Block; ++by)
        for (int bx = std::max(0, int((px - reach) / Block)); bx <= std::min(c->width - 1, int(px + reach)) / Block; ++bx)
        {
            const auto ground = groundIn(p->cellId, bx * Block, by * Block, Block, Block);
            const double expected = huntExpected(p->cellId, ground) * perBlock;
            if (expected <= 0)
                continue;
            std::vector<std::pair<std::string, double>> weights;
            double total = 0;
            for (const auto& s : wild::species())
            {
                double w = 0;
                for (const auto& [g, hw] : s.habitats)
                    if (const auto at = ground.find(g); at != ground.end())
                        w += hw * at->second;
                if (w > 0)
                {
                    weights.push_back({s.id, w * s.rarity});
                    total += w * s.rarity;
                }
            }
            const auto key = p->cellId + "|" + std::to_string(bx) + "|" + std::to_string(by);
            const int trails = whole(expected, odds(key + "|trails", window));
            for (int i = 0; i < trails && total > 0; ++i)
            {
                const auto trail = key + "|" + std::to_string(i);
                double r = odds(trail + "|species", window) * total;
                std::string species = weights.back().first;
                for (const auto& [id, w] : weights)
                    if ((r -= w) <= 0)
                    {
                        species = id;
                        break;
                    }
                const auto natural = [&](int x, int y) {
                    const auto* t = c->tile(x, y);
                    return t && !t->solid && !wild::groundOf(t->glyph).empty();
                };
                int x = -1, y = -1;
                for (int a = 0; a < 30 && x < 0; ++a)
                {
                    const int tx = bx * Block + int(mix(trail + "|sx", window + a) % Block), ty = by * Block + int(mix(trail + "|sy", window + a) % Block);
                    if (natural(tx, ty))
                        x = tx, y = ty;
                }
                if (x < 0)
                    continue;
                // Its way: mostly on, now and then a turn, over open natural ground.
                static const int dx[] = {1, 1, 0, -1, -1, -1, 0, 1}, dy[] = {0, 1, 1, 1, 0, -1, -1, -1};
                int dir = int(mix(trail + "|dir", window) % 8);
                const int length = 6 + int(mix(trail + "|len", window) % 8);
                Track t;
                t.cell = p->cellId;
                t.species = species;
                t.fresh = odds(trail + "|fresh", window) < .35;
                t.until = time_ + 600;
                for (int step = 0; step < length; ++step)
                {
                    if (std::hypot(x - px, y - py) <= reach)
                        t.tiles.push_back({x, y});
                    const double turn = odds(trail + "|turn", window * 31 + step);
                    if (turn < .3)
                        dir = (dir + (turn < .15 ? 1 : 7)) % 8;
                    int tries = 0;
                    while (!natural(x + dx[dir], y + dy[dir]) && tries++ < 8)
                        dir = (dir + 1) % 8;
                    if (tries > 8)
                        break;
                    x += dx[dir];
                    y += dy[dir];
                }
                if (t.tiles.size() >= 2)
                    found.push_back(std::move(t));
            }
        }
    tracks_[player] = found;
    if (found.empty())
        return {true, "You nose the ground for game, and find no sign of any near by.", {}};
    p->scentSkill = std::min(100.0, p->scentSkill + .3);   // Tracking comes with doing it.
    static const char* bearings[] = {"east", "south-east", "south", "south-west", "west", "north-west", "north", "north-east"};
    std::vector<std::string> said;
    for (const auto& t : found)
    {
        if (said.size() >= 4)
            break;
        const auto* s = wild::speciesById(t.species);
        auto nearest = t.tiles.front();
        for (const auto& tile : t.tiles)
            if (std::hypot(tile.first - px, tile.second - py) < std::hypot(nearest.first - px, nearest.second - py))
                nearest = tile;
        const double d = std::hypot(nearest.first - px, nearest.second - py);
        const int sector = int(std::lround(std::atan2(nearest.second - py, nearest.first - px) / (Pi / 4) + 8)) % 8;
        said.push_back((s ? s->name : std::string("something")) + "'s, " + (t.fresh ? "fresh" : "old") +
                       (d < 3 ? ", right here" : d < 8 ? ", close by to the " + std::string(bearings[sector]) : ", to the " + std::string(bearings[sector])));
    }
    return {true, "You nose the ground and find tracks: " + listed(said) + (found.size() > said.size() ? ", and more besides." : "."), {}};
}

std::vector<World::Track> World::tracksOf(const std::string& player) const
{
    std::vector<Track> out;
    const auto* p = entity(player);
    const auto mine = tracks_.find(player);
    if (!p || mine == tracks_.end())
        return out;
    for (const auto& t : mine->second)
        if (t.cell == p->cellId && t.until > time_)
            out.push_back(t);
    return out;
}

// ------------------------------------------------------------------ Foraging

Result World::forage(const std::string& player)
{
    auto* p = entity(player);
    if (!p || p->npc || p->dead)
        return {false, "No such character.", {}};
    if (p->downedLeft > 0)
        return {false, "You are down.", {}};
    if (inBattle(player))
        return {false, "Not in the middle of a fight.", {}};
    const auto* c = cell(p->cellId);
    if (!c || !c->outdoors || townOf(p->cellId))
        return {false, "There is nothing to forage here; go out into the wild.", {}};
    const auto& data = wild::forage();
    if (const auto next = forageNext_.find(player); next != forageNext_.end() && time_ < next->second)
        return {false, "You are still at it.", {}};
    const int season = int(calendar::calendarAt(calendarDays_).season);
    const int px = int(std::floor(p->position.x)), py = int(std::floor(p->position.y));
    struct Choice
    {
        const wild::ForageGround* ground;
        const wild::ForageGood* good;
        int x, y;
    };
    std::vector<Choice> choices;
    double total = 0;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            if (const auto* t = c->tile(px + dx, py + dy))
                for (const auto& g : data.ground)
                    if (g.tiles.find(t->glyph) != std::string::npos)
                        for (const auto& good : g.goods)
                            if (good.seasons.empty() || std::find(good.seasons.begin(), good.seasons.end(), season) != good.seasons.end())
                            {
                                choices.push_back({&g, &good, px + dx, py + dy});
                                total += good.weight;
                            }
    if (choices.empty())
        return {false, "Nothing here is worth foraging at this time of year.", {}};
    forageNext_[player] = time_ + data.seconds;
    double r = odds(player + "|forage", std::int64_t(time_ * 1000)) * total;
    const Choice* pick = &choices.back();
    for (const auto& ch : choices)
        if ((r -= ch.good->weight) <= 0)
        {
            pick = &ch;
            break;
        }
    // The patch it grows in: picked over for a while once its pickings are taken.
    const auto patch = p->cellId + "|" + std::to_string(pick->x / data.patchTiles) + "|" + std::to_string(pick->y / data.patchTiles);
    auto& [used, day] = forage_[patch];
    const double regrown = (calendarDays_ - day) * 24 / data.regrowHours;
    if (regrown >= 1)
    {
        used = std::max(0, used - int(regrown));
        day = calendarDays_;
    }
    if (used == 0)
        day = calendarDays_;
    if (used >= data.picks)
        return {false, "This patch has been picked over lately; try further on.", {}};
    if (!society_.create(player, pick->good->item, pick->good->count, "foraged"))
        return {false, "You can't carry any more of that.", {}};
    ++used;
    recordEvent({"forage", player, {}, p->cellId, 0, 0, pick->good->item, pick->good->count, 0, pick->ground->id});
    return {true, "You gather " + std::to_string(pick->good->count) + " " + lower(Society::itemName(pick->good->item)) + " from " +
                      pick->ground->name + ".",
            {}};
}
} // namespace ratw
