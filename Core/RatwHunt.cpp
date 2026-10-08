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

Result World::startHunt(const std::string& player, const std::vector<std::string>& only)
{
    for (const auto& id : only)
        if (!wild::speciesById(id))
            return {false, "No such animal: " + id + ".", {}};
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
    if (!only.empty())
        count = int(only.size()), follow = nullptr, followed.clear();   // (The game asked for, and only that.)
    if (count == 0 || (weights.empty() && only.empty()))
    {
        const bool hunted = huntPressure(p->cellId) >= 1;
        return {false, hunted ? "You cast about, but this ground has been hunted lately: no game shows itself."
                              : "You cast about for a while, but no game shows itself here.",
                {}};
    }
    for (int i = 0; i < count; ++i)
        addAnimal(b, !only.empty() ? only[std::size_t(i)] : i == 0 && follow && follow->fresh ? follow->species : pickSpecies(i), false);
    if (follow)
    {
        auto& mine = tracks_[player];
        mine.erase(std::remove_if(mine.begin(), mine.end(), [&](const Track& t) { return &t == follow; }), mine.end());
        huntRoles_[b.id][player].insert("tracker");   // (Their trail brought the game in: doc 53, 1.7.)
    }
    huntStarter_[b.id] = player;
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
    beginPlacing(b);                                // The hunters take their ground first (doc 40); game stays where it is.
    battles_.push_back(std::move(b));
    return {true, (followed.empty() ? std::string() : "You follow " + followed + "'s trail. ") + "You go out after game. Ahead: " +
                      listed(seen) + ", not yet aware of you.",
            {}};
}

std::string World::huntJoinRefusal(const Battle& b, const std::string& id) const
{
    // Doc 53, 1.4: never one blocked by a hunter; always a hunter's companion, party or Chapter, or one let in or
    // invited; anyone else while the hunt's starter allows hunting partners (only the starter's setting counts).
    if (!b.hunt)
        return {};
    const auto* e = entity(id);
    for (const auto& f : b.fighters)
        if (f.side == 0 && f.status != "fled" && blocked_ && blocked_(f.id, id))
            return "You can't join that hunt.";
    for (const auto& f : b.fighters)
        if (f.side == 0 && f.status != "fled")
        {
            if (e && e->npc && e->leaderId == f.id)
                return {};                          // A companion comes along with the one it follows.
            if (friends_ && friends_(f.id, id))
                return {};
        }
    if (huntInvited(b.id, id))
        return {};
    const auto starter = huntStarterOf(b.id);
    const auto* s = entity(starter);
    if (!s || !s->noHuntPartners)
        return {};
    return "That hunt is closed to strangers: ask to join it.";
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
    // Doc 40's noticing (RatwBattle.cpp, "Sneaking"): an animal looks and listens round at the start of its turn
    // (World::senseFoes) and after each hunter's; here, whether it is now alert to any of them.
    for (const auto& f : b.fighters)
        if (f.side != animal.side && f.status == "fighting" && awareness(b, animal.id, f.id) >= battle::AwareAlert)
            return true;
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
    if (s->flees())
        return fleeingTurn(b, f, a->second, *s, hunters);
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
        // Half noticed something (doc 40): it freezes, head up, toward it, and doesn't graze.
        const BattleFighter* suspect = nullptr;
        for (const auto* o : hunters)
            if (awareness(b, f.id, o->id) >= battle::AwareSuspicious && (!suspect || awareness(b, f.id, o->id) > awareness(b, f.id, suspect->id)))
                suspect = o;
        if (suspect)
        {
            f.facing = battle::octant(suspect->x - f.x, suspect->y - f.y);
            endTurn(b, f);
            return true;
        }
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
    const bool fights = s->temper == "fierce";       // (Every other kind runs: fleeingTurn, doc 53.)
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
    // Game that runs (doc 53): a landed bite or blade kills it outright, a clean kill. Fire and other Gifts' blows keep
    // its health (doc 41).
    if (s->flees() && downedBase == battle::DownedBite && damage > 0)
    {
        a->second.best = std::max(a->second.best, s->health);
        a->second.struckUnaware = !a->second.alert;
        a->second.total += s->health;
        if (!by.empty())
            a->second.lastBy = by;
        a->second.alert = true;
        return 100;
    }
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
    if (s->flees() && !by.empty() && b.fighter(by))
        huntBolt(b, t, a->second, by);              // (Hurt and not killed, game that runs bolts from whoever did it: doc 53.)
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
    if (s->flees() && clean >= 1)
        quality = a->second.saw.count(killer) ? 2 : 3;   // (Doc 53: masterwork only from a wolf it never saw.)
    if (fire > 0)
        quality = std::min(quality, 1);
    // Shared equally among those taking part (doc 53, 1.5): each gets the whole part, the rest goes one by one to
    // sharers drawn by chance, so nothing is made or lost; a companion's share goes to the wolf it follows.
    const auto sharers = huntSharers(b, killer);
    std::map<std::string, std::vector<std::string>> takenBy;
    std::vector<std::string> spoilt;
    int takenKinds = 0;
    for (const auto& [item, n] : s->yield)
    {
        double factor = share;
        if (fire > 0)
            factor *= skinOf(item) ? std::max(0.0, 1 - 2 * fire) : item == "raw_meat" ? 1 - fire / 2 : 1;
        const int got = whole(n * factor, odds(f.id + "|" + item, std::int64_t(calendarDays_ * 1000)));
        const auto kind = quality == 3 ? items::withMaker(items::withQuality(item, 3), killer) : items::withQuality(item, quality);
        if (got > 0 && !sharers.empty())
        {
            const int count = int(sharers.size());
            std::vector<int> parts(std::size_t(count), got / count);
            std::vector<std::size_t> order(static_cast<std::size_t>(count));
            for (std::size_t i = 0; i < order.size(); ++i)
                order[i] = i;
            std::sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) {
                return odds(f.id + "|" + item + "|" + sharers[x] + std::to_string(x), 1) < odds(f.id + "|" + item + "|" + sharers[y] + std::to_string(y), 1);
            });
            for (int r = 0; r < got % count; ++r)
                ++parts[order[std::size_t(r)]];
            std::map<std::string, int> byWolf;
            for (std::size_t i = 0; i < parts.size(); ++i)
                byWolf[sharers[i]] += parts[i];
            for (const auto& [who, part] : byWolf)
                if (part > 0 && entity(who) && society_.create(who, kind, part, "hunted"))
                {
                    addScent(who, kind, part, killer, {});  // Hunted goods carry the hunter's scent (doc 55, 4).
                    takenBy[who].push_back(std::to_string(part) + " " + lower(Society::itemName(kind)));
                    auto& mine = huntShares_[who];
                    if (mine.battle != b.id)
                        mine = {b.id, {}, {}, 0};
                    mine.goods[kind] += part;
                    mine.until = time_ + 600;
                }
            ++takenKinds;
        }
        else if (fire > 0 && skinOf(item))
            spoilt.push_back(lower(Society::itemName(item)));
    }
    const std::string name = names::capitalised(s->name);
    fightLine(b, f.id, killer, "death", name + " falls: " + grade + (fire >= .5 ? ", burnt." : "."));
    recordEvent({"hunted", killer, a->second.species, b.cellId, 0, 0, {}, takenKinds, 0, grade});
    // Driven into a partner (doc 53, 1.7): the one it fled drove it, the one who took it lay in wait for it.
    if (a->second.state == "fleeing" && !a->second.from.empty() && a->second.from != killer)
    {
        huntRoles_[b.id][a->second.from].insert("driver");
        huntRoles_[b.id][killer].insert("ambusher");
    }
    std::set<std::string> players;
    for (const auto& who : sharers)
        if (entity(who) && !entity(who)->npc)
            players.insert(who);
    for (const auto& x : players)
        for (const auto& y : players)
            if (x < y)
                huntPairs_[b.id].insert({x, y});
    for (const auto& who : players)
    {
        auto& mine = huntShares_[who];
        mine.hunters.clear();
        for (const auto& other : players)
            if (other != who)
                mine.hunters.push_back(other);
        const auto got = takenBy.find(who);
        std::string words;
        if (players.size() <= 1)
            words = "You bring down " + s->name + ": " + grade + ". " +
                    (got == takenBy.end() ? "Nothing of it is worth taking." : "You take " + listed(got->second) + ".");
        else
            words = (who == killer ? "You bring down " : (entity(killer) ? entity(killer)->name : std::string("Another")) + " brings down ") +
                    s->name + ": " + grade + ", shared among " + std::to_string(players.size()) + ". " +
                    (got == takenBy.end() ? "Your share: nothing worth taking." : "Your share: " + listed(got->second) + ".");
        if (!spoilt.empty())
            words += " The fire has spoilt the " + listed(spoilt) + ".";
        notice(who, words);
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
    // Doc 53: those who shared a kill grow closer (once a hunt); the parts each played, for the end screen and deeds.
    for (const auto& [a, c] : huntPairs_[b.id])
        bonds_.mutual(a, c, {1, 1, 2, 0, 0}, calendarDays_);
    for (const auto& [id, roles] : huntRoles_[b.id])
        for (const auto& role : roles)
            recordEvent({"huntRole", id, role, b.cellId, 0, 0, {}, 0, 0, b.id});
    huntPairs_.erase(b.id);
    huntRoles_.erase(b.id);
    huntInvited_.erase(b.id);
    huntStarter_.erase(b.id);
    huntAsks_.erase(std::remove_if(huntAsks_.begin(), huntAsks_.end(), [&](const HuntAsk& a) { return a.battle == b.id; }), huntAsks_.end());
    std::vector<std::string> gone;
    for (const auto& [id, a] : animals_)
        if (a.battle == b.id)
            gone.push_back(id);
    for (const auto& id : gone)
    {
        entities_.erase(id);
        animals_.erase(id);
    }
    for (const auto& f : b.fighters)
        huntHunters_.erase(f.id);
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
    huntAsks_.erase(std::remove_if(huntAsks_.begin(), huntAsks_.end(), [&](const HuntAsk& a) { return a.until <= time_; }), huntAsks_.end());
    for (auto it = huntShares_.begin(); it != huntShares_.end();)
        it = it->second.until <= time_ ? huntShares_.erase(it) : std::next(it);   // (The end card's Give my share lapses.)
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
        int standing = 0, hunters = 0;
        for (const auto& f : b.fighters)
        {
            standing += animals_.count(f.id) && f.status == "fighting";
            hunters += f.side == 0 && f.status == "fighting";
        }
        // More hunters, more game (doc 53, 1.6): the chance and how many at once both grow with each extra hunter.
        const int extra = std::max(0, hunters - 1);
        if (standing == 0 || standing >= pop.atOnce + pop.atOncePerHunter * extra ||
            odds(b.id + "|arrive", std::int64_t(time_)) >= arrival->second.second / 6 * (1 + pop.perHunter * extra))
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
    practise(player, "track.found");                      // Tracking comes with doing it (by practice, doc 49).
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
    // The patch it grows in: picked over for a while once its pickings are taken (by players and the wolves who
    // gather for a living alike, doc 42).
    // Foraging together (doc 53, 2.5): a patch gives one more picking for each extra wolf, up to two more.
    auto* joint = jointOf_.count(player) ? &joints_[jointOf_[player]] : nullptr;
    if (joint && joint->kind != "forage")
        joint = nullptr;
    const int extra = joint ? std::min(2, int(joint->members.size()) - 1) : 0;
    if (!takeFromPatch(p->cellId, pick->x, pick->y, extra))
        return {false, "This patch has been picked over lately; try further on.", {}};
    lastWorked_[player] = time_;                         // (At work: others may lend a paw, doc 53.)
    // Weathereye (a Gifted Seer, doc 43): it knew where the good pickings would be: a quarter more, one at least.
    const int count = pick->good->count + (p->gift == "seer" && !p->quickened ? std::max(1, pick->good->count / 4) : 0);
    const auto item = pick->good->item;
    practise(player, "forage.pick");                    // Gathering is labour (by practice: doc 49).
    if (joint && joint->members.size() >= 2)
    {
        // Each picking brings in its count at the joint's rate (fractions by chance), shared exactly among its members.
        const double rate = workRate(player);
        const int total = whole(count * rate, odds(player + "|together", std::int64_t(time_ * 1000)));
        std::vector<int> order(joint->members.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = int(i);
        std::sort(order.begin(), order.end(), [&](int x, int y) {
            return odds(joint->members[std::size_t(x)].id + item, std::int64_t(time_ * 1000)) < odds(joint->members[std::size_t(y)].id + item, std::int64_t(time_ * 1000));
        });
        const auto parts = together::split(total, int(joint->members.size()), order);
        int mine = 0;
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            const auto& m = joint->members[i];
            if (parts[i] > 0 && entity(m.id))
                society_.create(m.id, item, parts[i], "foraged together");
            if (m.id == player)
                mine = parts[i];
            else if (parts[i] > 0)
                notice(m.id, "Your share of what " + p->name + " gathers: " + std::to_string(parts[i]) + " " + lower(Society::itemName(item)) + ".");
        }
        // A beat: this wolf worked, and with whoever else is working. The work is where it was last done.
        joint->x = pick->x + .5;
        joint->y = pick->y + .5;
        for (auto& m : joint->members)
            if (m.id == player)
            {
                m.lastActed = time_;
                ++m.beats;
            }
        for (const auto& m : joint->members)
            if (m.id != player && time_ - m.lastActed <= together::rules().idleSeconds)
                ++joint->beatsTogether[player < m.id ? player + "|" + m.id : m.id + "|" + player];
        recordEvent({"forage", player, {}, p->cellId, 0, 0, item, total, 0, pick->ground->id});
        return {true, "Together you gather " + std::to_string(total) + " " + lower(Society::itemName(item)) + " from " + pick->ground->name +
                          "; your share is " + std::to_string(mine) + ".",
                {}};
    }
    if (!society_.create(player, item, count, "foraged"))
        return {false, "You can't carry any more of that.", {}};
    recordEvent({"forage", player, {}, p->cellId, 0, 0, item, count, 0, pick->ground->id});
    return {true, "You gather " + std::to_string(count) + " " + lower(Society::itemName(item)) + " from " +
                      pick->ground->name + ".",
            {}};
}

// ------------------------------------------------------------------ Game that runs (doc 53, Phase 1)
//
// Every animal but the fierce ones: grazing until it notices a wolf; watching one it has noticed further off than its
// flight distance, frozen and facing it; bolting directly away, faster than any wolf, from one that comes within it
// (or bites at it and misses); calming once it has lost that wolf for two of its turns. Only wolves it has noticed
// count, so one driven can run past a hidden partner. A landed bite or blade kills it; its dodge depends on what it
// knew of the biter. A hunter that ends its turn crouched without biting lies in wait, and springs on one stepping
// beside it.

std::string World::animalState(const std::string& id) const
{
    const auto found = animals_.find(id);
    if (found == animals_.end())
        return {};
    const auto* s = wild::speciesById(found->second.species);
    return s && s->flees() ? found->second.state : std::string();
}

std::string World::animalWatching(const std::string& id) const
{
    const auto found = animals_.find(id);
    return found == animals_.end() || found->second.state != "watching" ? std::string() : found->second.watching;
}

bool World::huntWaiting(const std::string& hunter) const
{
    const auto found = huntHunters_.find(hunter);
    return found != huntHunters_.end() && found->second.waiting;
}

double World::huntDodge(const BattleFighter& biter, const BattleFighter& animal, std::string* why) const
{
    const auto a = animals_.find(animal.id);
    if (a == animals_.end())
        return -1;
    const auto* s = wild::speciesById(a->second.species);
    const auto* b = battleOf(animal.id);
    const auto* e = entity(biter.id);
    if (!s || !s->flees() || !b || !b->hunt || !e)
        return -1;
    const double aw = awareness(*b, animal.id, biter.id);
    double dodge = .05;
    std::string words = "unaware";
    const auto& st = a->second.state;
    if ((st == "fleeing" || st == "calming") && a->second.from == biter.id)
        dodge = .75, words = "fleeing you";
    else if (st == "fleeing" || st == "calming")
    {
        if (!a->second.saw.count(biter.id) && aw < battle::AwareAlert)
            dodge = .15, words = "driven";
        else
            dodge = .6, words = "watching you";
    }
    else if (aw >= battle::AwareAlert)
        dodge = .6, words = "watching you";
    else if (aw >= battle::AwareSuspicious)
        dodge = .05 + .55 * (aw - battle::AwareSuspicious) / (battle::AwareAlert - battle::AwareSuspicious), words = "wary";
    // A little by the kind (a hare dodges better, a badger worse), the biter's dexterity and its fighting skill.
    dodge += s->dodge / 100 - std::max(0.0, effectiveDexterity(*e) - 50) * .002 - std::max(0.0, temperamentOf(*e).skill - 50) * .002;
    if (why)
        *why = words;
    return std::clamp(dodge, .02, .9);
}

int World::huntReach(const Battle& b, const BattleFighter& f) const
{
    // A bolting animal's run: half again the fastest hunter's sprint.
    const auto a = animals_.find(f.id);
    if (!b.hunt || a == animals_.end() || a->second.state != "fleeing")
        return 0;
    int fastest = 0;
    for (const auto& o : b.fighters)
        if (o.side != f.side && o.status == "fighting")
            if (const auto* e = entity(o.id))
                fastest = std::max(fastest, battle::moveRange(effectiveDexterity(*e), e->hurt, 10));
    return fastest > 0 ? int(std::ceil(fastest * 1.5)) : 0;
}

int World::huntNoise(Battle& b, int x, int y, int reach)
{
    int moved = 0;
    for (auto& f : b.fighters)
    {
        const auto found = animals_.find(f.id);
        const auto* s = found == animals_.end() ? nullptr : wild::speciesById(found->second.species);
        if (!s || !s->flees() || f.status != "fighting")
            continue;
        auto& a = found->second;
        const double gap = std::max(std::abs(f.x - x), std::abs(f.y - y));
        if (gap <= s->flight)
        {
            huntBolt(b, f, a, {});
            a.noiseX = x;
            a.noiseY = y;
            ++moved;
        }
        else if (gap <= reach && a.state != "fleeing")
        {
            a.state = "watching";
            a.watching.clear();
            f.facing = battle::octant(x - f.x, y - f.y);
            ++moved;
        }
    }
    return moved;
}

void World::huntBolt(Battle& b, BattleFighter& animal, HuntAnimal& a, const std::string& from)
{
    const bool already = a.state == "fleeing" && a.from == from;
    a.state = "fleeing";
    a.from = from;
    if (!from.empty())
        a.noiseX = a.noiseY = -1;
    a.calm = 0;
    a.alert = true;
    if (!animal.acting)
        animal.meter = 100;                         // It bolts: its bar fills at once.
    if (!already)
        if (const auto* s = wild::speciesById(a.species))
            fightLine(b, animal.id, from, "bolt", names::capitalised(s->name) + " bolts.");
}

void World::huntReact(Battle& b, BattleFighter& animal, const BattleFighter* after)
{
    // At the end of a hunter's turn (`after`), and at the start of the animal's own (null).
    const auto it = animals_.find(animal.id);
    if (it == animals_.end() || animal.status != "fighting")
        return;
    auto& a = it->second;
    const auto* s = wild::speciesById(a.species);
    if (!s || !s->flees())
        return;
    const BattleFighter* nearest = nullptr;
    int nearestGap = std::numeric_limits<int>::max();
    for (const auto& o : b.fighters)
    {
        if (o.side == animal.side || o.status != "fighting")
            continue;
        if (awareness(b, animal.id, o.id) < battle::AwareAlert)
            continue;                               // (Only wolves it has noticed count.)
        a.saw.insert(o.id);
        a.alert = true;
        const int gap = std::max(std::abs(o.x - animal.x), std::abs(o.y - animal.y));
        if (gap < nearestGap)
            nearestGap = gap, nearest = &o;
    }
    if (after)
    {
        if (a.state != "grazing" && a.state != "watching")
            return;
        const bool sees = awareness(b, animal.id, after->id) >= battle::AwareAlert;
        const int gap = std::max(std::abs(after->x - animal.x), std::abs(after->y - animal.y));
        if (sees && gap <= s->flight)
            huntBolt(b, animal, a, after->id);
        else if (nearest && a.state == "grazing")
            a.state = "watching", a.watching = nearest->id;
        return;
    }
    if (a.state == "fleeing")
    {
        const auto* from = b.fighter(a.from);
        if (!from || from->status != "fighting" || awareness(b, animal.id, a.from) < battle::AwareSuspicious)
            ++a.calm;
        else
            a.calm = 0;
        if (a.calm >= 2)
            a.state = "calming";
        return;
    }
    if (a.state == "calming")
        return;
    if (nearest && nearestGap <= s->flight)
        huntBolt(b, animal, a, nearest->id);
    else if (nearest)
        a.state = "watching", a.watching = nearest->id;
    else
        a.state = "grazing", a.watching.clear();
}

bool World::fleeingTurn(Battle& b, BattleFighter& f, HuntAnimal& a, const wild::Species& s, const std::vector<const BattleFighter*>& hunters)
{
    if (!f.moved && f.walk.empty())
        huntReact(b, f, nullptr);
    const auto key = std::int64_t(f.turnsTaken) * 131 + b.turns;
    const auto finish = [&] {
        if (!b.over && f.acting)
        {
            if (a.state == "calming")
            {
                // One turn at its ordinary pace, still away; then watching whoever it notices, or grazing.
                const BattleFighter* seen = nullptr;
                for (const auto* o : hunters)
                    if (awareness(b, f.id, o->id) >= battle::AwareAlert)
                        seen = o;
                a.state = seen ? "watching" : "grazing";
                a.watching = seen ? seen->id : std::string();
            }
            endTurn(b, f);                          // (Quietly: game doesn't announce its waiting.)
        }
        return true;
    };
    if (a.state == "grazing")
    {
        // Half noticed something (doc 40): head up, toward it, and no grazing.
        const BattleFighter* suspect = nullptr;
        for (const auto* o : hunters)
            if (awareness(b, f.id, o->id) >= battle::AwareSuspicious && (!suspect || awareness(b, f.id, o->id) > awareness(b, f.id, suspect->id)))
                suspect = o;
        if (suspect)
        {
            f.facing = battle::octant(suspect->x - f.x, suspect->y - f.y);
            return finish();
        }
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
        return finish();
    }
    if (a.state == "watching")
    {
        if (const auto* w = b.fighter(a.watching))
            f.facing = battle::octant(w->x - f.x, w->y - f.y);
        return finish();
    }
    // Fleeing (or calming): directly away from the one it fled, the tile furthest along that line, a little against
    // straying sideways; off the edge, it gets away.
    const auto* from = b.fighter(a.from);
    const bool noise = !from && a.noiseX >= 0;      // (Or from a noise: doc 53, 4.)
    if (!f.moved && f.walk.empty() && (from || noise) && !b.onEdge(f.x, f.y))
    {
        const int sx = from ? from->x : a.noiseX, sy = from ? from->y : a.noiseY;
        const double dx = f.x - sx, dy = f.y - sy, len = std::max(1e-6, std::hypot(dx, dy));
        const double ux = dx / len, uy = dy / len;
        std::pair<int, int> best{f.x, f.y};
        double bestScore = 0;
        for (const auto& [x, y] : battleReach(f.id))
        {
            const double mx = x - f.x, my = y - f.y;
            const double along = mx * ux + my * uy, side = std::abs(mx * uy - my * ux);
            if (const double score = along - .5 * side; score > bestScore)
                bestScore = score, best = {x, y};
        }
        if (best != std::pair<int, int>{f.x, f.y})
            battleMove(f.id, best.first, best.second);
        else if (from && std::max(std::abs(from->x - f.x), std::abs(from->y - f.y)) == 1)
        {
            // Nowhere further to go: it bites the wolf beside it once.
            auto* e = entity(f.id);
            if (e && !e->exhausted && e->stamina >= battle::BiteStamina)
                battleAct(f.id, "bite", from->id);
        }
        if (!f.walk.empty())
            return true;
    }
    if (!b.over && f.acting && b.onEdge(f.x, f.y))
    {
        fightLine(b, f.id, {}, "flee", names::capitalised(s.name) + " gets away.");
        battleAct(f.id, "flee");
    }
    return finish();
}

void World::huntAfterTurn(Battle& b, BattleFighter& f)
{
    if (!b.hunt || animals_.count(f.id))
        return;
    // The hunter: whether it did anything (for sharing a kill, doc 53 Phase 2), and whether it now lies in wait.
    auto& h = huntHunters_[f.id];
    if (f.moved || f.acted || f.stalking)
        h.lastActive = f.turnsTaken;
    h.waiting = f.stalking && !f.acted && f.status == "fighting";
    for (auto& o : b.fighters)
        if (animals_.count(o.id))
            huntReact(b, o, &f);
}

void World::huntMissed(Battle& b, const BattleFighter& biter, BattleFighter& animal)
{
    const auto it = animals_.find(animal.id);
    if (!b.hunt || it == animals_.end() || animal.status != "fighting")
        return;
    const auto* s = wild::speciesById(it->second.species);
    if (s && s->flees())
        huntBolt(b, animal, it->second, biter.id);  // A dodge sends it off, away from the biter.
}

void World::huntStep(Battle& b, BattleFighter& animal)
{
    // An animal stepping beside a wolf lying in wait: the wolf springs, a bite at once, out of its turn.
    if (!b.hunt || !animals_.count(animal.id) || animal.status != "fighting")
        return;
    for (auto& o : b.fighters)
    {
        if (o.side == animal.side || o.status != "fighting" || std::max(std::abs(o.x - animal.x), std::abs(o.y - animal.y)) != 1)
            continue;
        const auto h = huntHunters_.find(o.id);
        if (h == huntHunters_.end() || !h->second.waiting)
            continue;
        h->second.waiting = false;
        huntRoles_[b.id][o.id].insert("ambusher");
        if (const auto* e = entity(o.id))
            fightLine(b, o.id, animal.id, "spring", e->name + " springs from hiding!");
        bite(b, o, animal.id);
        if (animal.status == "fighting")
            animal.walk.clear();                    // Dodged: it swerves, and stops where it is.
        return;
    }
}

// ------------------------------------------------------------------ Hunting together (doc 53, Phase 2)

std::string World::huntStarterOf(const std::string& battleId) const
{
    const auto found = huntStarter_.find(battleId);
    return found == huntStarter_.end() ? std::string() : found->second;
}

bool World::huntInvited(const std::string& battleId, const std::string& id) const
{
    const auto found = huntInvited_.find(battleId);
    return found != huntInvited_.end() && found->second.count(id);
}

bool World::mayJoinHunt(const Battle& b, const std::string& id) const
{
    return b.hunt && !b.over && huntJoinRefusal(b, id).empty();
}

bool World::mayAskHunt(const Battle& b, const std::string& id) const
{
    if (!b.hunt || b.over || mayJoinHunt(b, id))
        return false;
    for (const auto& f : b.fighters)
        if (f.side == 0 && f.status != "fled" && blocked_ && blocked_(f.id, id))
            return false;
    return std::none_of(huntAsks_.begin(), huntAsks_.end(), [&](const HuntAsk& a) { return a.battle == b.id && a.from == id; });
}

int World::huntTaken(const Battle& b) const
{
    int taken = 0;
    for (const auto& f : b.fighters)
        taken += animals_.count(f.id) && f.status == "dead";
    return taken;
}

const World::HuntShare* World::huntShareOf(const std::string& id) const
{
    const auto found = huntShares_.find(id);
    return found == huntShares_.end() || found->second.goods.empty() ? nullptr : &found->second;
}

std::vector<std::string> World::huntNearby(const std::string& hunter) const
{
    // Wolves near enough to invite: within 20 tiles of the hunter, in the same place, not in a fight, not blocked.
    std::vector<std::string> out;
    const auto* me = entity(hunter);
    if (!me)
        return out;
    for (const auto* o : entitiesIn(me->cellId))
        if (!o->npc && !o->dead && o->id != hunter && !inBattle(o->id) && std::hypot(o->position.x - me->position.x, o->position.y - me->position.y) <= 20 &&
            !(blocked_ && blocked_(hunter, o->id)))
            out.push_back(o->id);
    return out;
}

Result World::setPartners(const std::string& id, const std::string& kind, bool on)
{
    auto* e = entity(id);
    if (!e || e->npc)
        return {false, "No such character.", {}};
    if (kind == "hunt")
        e->noHuntPartners = !on;
    else if (kind == "work")
        e->noWorkPartners = !on;
    else
        return {false, "Hunting or work partners?", {}};
    return {true, std::string(kind == "hunt" ? "Hunting partners: " : "Work partners: ") +
                      (on ? "anyone may join you." : "only those you invite (and your party and Chapter)."),
            {}};
}

Result World::askToJoinHunt(const std::string& id, const std::string& battleId)
{
    auto* b = battleById(battleId);
    const auto* e = entity(id);
    if (!e || e->npc)
        return {false, "No such character.", {}};
    if (!b || !b->hunt || b->over)
        return {false, "That hunt is over.", {}};
    if (inBattle(id))
        return {false, "You are already in a fight.", {}};
    if (e->cellId != b->cellId)
        return {false, "That hunt is somewhere else.", {}};
    if (mayJoinHunt(*b, id))
        return {false, "You may join that hunt as you are.", {}};
    if (!mayAskHunt(*b, id))
        return {false, "You can't ask to join that hunt.", {}};
    huntAsks_.push_back({battleId, id, time_ + 30});
    for (const auto& f : b->fighters)
        if (const auto* h = entity(f.id); h && !h->npc && f.side == 0 && f.status != "fled")
            notice(f.id, e->name + " asks to join your hunt.");
    return {true, "You ask to join the hunt.", {}};
}

Result World::answerHuntAsk(const std::string& hunter, const std::string& asker, bool letIn)
{
    const auto* b = battleOf(hunter);
    const auto* f = b ? b->fighter(hunter) : nullptr;
    if (!b || !b->hunt || !f || f->side != 0)
        return {false, "You are not hunting.", {}};
    const auto ask = std::find_if(huntAsks_.begin(), huntAsks_.end(), [&](const HuntAsk& a) { return a.battle == b->id && a.from == asker; });
    if (ask == huntAsks_.end())
        return {false, "No one is asking that now.", {}};
    const auto battleId = b->id;
    huntAsks_.erase(ask);
    if (!letIn)
    {
        notice(asker, "They'd rather hunt without company just now.");
        return {true, "Not now.", {}};
    }
    huntInvited_[battleId].insert(asker);
    const auto joined = joinBattle(asker, battleId, 0);
    if (!joined.ok)
        notice(asker, "You are let into the hunt, but can't join it: " + joined.message);
    return {true, joined.ok ? "You let them in." : "You let them in, but they can't join: " + joined.message, {}};
}

Result World::inviteToHunt(const std::string& hunter, const std::string& target)
{
    const auto* b = battleOf(hunter);
    const auto* f = b ? b->fighter(hunter) : nullptr;
    if (!b || !b->hunt || b->over || !f || f->side != 0)
        return {false, "You are not hunting.", {}};
    const auto near = huntNearby(hunter);
    if (std::find(near.begin(), near.end(), target) == near.end())
        return {false, "They aren't near enough to invite.", {}};
    huntInvited_[b->id].insert(target);
    if (const auto* h = entity(hunter))
        notice(target, h->name + " invites you to join their hunt.");
    return {true, "You invite them to join the hunt.", {}};
}

Result World::giveHuntShare(const std::string& from, const std::string& to)
{
    // At the end card (doc 53, 1.5): one's share of the last hunt to another who took part, to carry.
    const auto found = huntShares_.find(from);
    if (found == huntShares_.end() || found->second.goods.empty())
        return {false, "You have no share to give.", {}};
    auto& share = found->second;
    if (std::find(share.hunters.begin(), share.hunters.end(), to) == share.hunters.end() || !entity(to))
        return {false, "Give it to someone who hunted with you.", {}};
    int moved = 0;
    for (const auto& [kind, n] : share.goods)
    {
        const auto* account = society_.account(from);
        const int have = account ? Society::stockAll(*account, kind) : 0;
        if (const int give = std::min(n, have); give > 0 && society_.shift(from, to, kind, give, 0, "a hunt's share"))
            moved += give;
    }
    const auto battleId = share.battle;
    huntShares_.erase(found);
    if (moved == 0)
        return {false, "You no longer have it.", {}};
    recordEvent({"huntRole", to, "carrier", entity(to)->cellId, 0, 0, {}, 0, 0, battleId});
    if (const auto* e = entity(from))
        notice(to, e->name + " gives you their share of the hunt to carry.");
    return {true, "You give them your share.", {}};
}

std::vector<std::string> World::huntSharers(const Battle& b, const std::string& killer) const
{
    // Those taking part: still in the hunt, having done something in their last ten turns; the one who made the kill
    // always. A companion's share goes to the wolf it follows.
    std::vector<std::string> out;
    bool killerIn = false;
    for (const auto& f : b.fighters)
    {
        if (f.side != 0 || f.status != "fighting")
            continue;
        const auto* e = entity(f.id);
        const auto h = huntHunters_.find(f.id);
        const bool active = f.id == killer || (h != huntHunters_.end() && h->second.lastActive >= 0 && f.turnsTaken - h->second.lastActive <= 10);
        if (!active || !e)
            continue;
        const auto to = e->npc && !e->leaderId.empty() ? e->leaderId : f.id;
        if (entity(to) && !entity(to)->npc)
            out.push_back(to);
        killerIn = killerIn || f.id == killer;
    }
    if (!killerIn && !killer.empty() && entity(killer) && !entity(killer)->npc)
        out.push_back(killer);
    return out;
}

std::string World::pickHuntSpecies(const Battle& b, const std::string& salt) const
{
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
    double r = odds(b.id + "|" + salt, std::int64_t(time_ * 10)) * total;
    for (const auto& [id, w] : weights)
        if ((r -= w) <= 0)
            return id;
    return weights.empty() ? std::string() : weights.back().first;
}

void World::huntJoined(Battle& b, const std::string& id)
{
    // A hunter joining brings game in with them: half the hunt's expected count, rolled, unaware at the edge.
    const auto* e = entity(id);
    const auto arrival = huntArrivals_.find(b.id);
    if (!b.hunt || !e || e->npc || arrival == huntArrivals_.end())
        return;
    int count = 0;
    for (double left = arrival->second.second * 2 * wild::population().joinerBrings, i = 0; left > 0; left -= 1, ++i)
        count += odds(b.id + "|joined|" + id, std::int64_t(time_ * 1000) + std::int64_t(i)) < std::min(1.0, left) / 2;
    for (int i = 0; i < count; ++i)
        if (const auto species = pickHuntSpecies(b, id + std::to_string(i)); !species.empty())
            addAnimal(b, species, true);
}

bool World::huntHelperTurn(Battle& b, BattleFighter& f)
{
    // A companion in a hunt follows its leader's lead instead of charging the game: crouched beside a stalking leader,
    // or round to the far side of the animal its leader goes at; and lies in wait there.
    if (!b.hunt || f.side != 0 || f.status != "fighting")
        return false;
    const auto* e = entity(f.id);
    if (!e || !e->npc || e->leaderId.empty())
        return false;
    const auto* leader = b.fighter(e->leaderId);
    if (!leader || leader->status != "fighting")
        return false;
    if (b.over || !f.acting || !f.walk.empty())
        return true;
    // An animal beside it that hasn't noticed it: a bite (its best odds).
    for (const auto& o : b.fighters)
        if (!f.acted && animals_.count(o.id) && o.status == "fighting" && apart(f.x, f.y, o.x, o.y) == 1 &&
            awareness(b, o.id, f.id) < battle::AwareAlert && !e->exhausted && e->stamina >= battle::BiteStamina)
        {
            battleAct(f.id, "bite", o.id);
            break;
        }
    if (b.over || !f.acting)
        return true;
    const BattleFighter* game = nullptr;
    for (const auto& o : b.fighters)
        if (animals_.count(o.id) && o.status == "fighting" &&
            (!game || apart(leader->x, leader->y, o.x, o.y) < apart(leader->x, leader->y, game->x, game->y)))
            game = &o;
    if (!f.moved && !f.acted)
    {
        if (!f.stalking)
            battleAct(f.id, "stalk");
        double gx = leader->x, gy = leader->y;
        if (game && !leader->stalking)
        {
            const double dx = game->x - leader->x, dy = game->y - leader->y, len = std::max(1.0, std::hypot(dx, dy));
            gx = game->x + dx / len * 2.5;
            gy = game->y + dy / len * 2.5;
        }
        std::pair<int, int> best{f.x, f.y};
        double bestD = std::hypot(f.x - gx, f.y - gy);
        for (const auto& [x, y] : battleReach(f.id))
            if (const double d = std::hypot(x - gx, y - gy); d < bestD && !(x == leader->x && y == leader->y))
                bestD = d, best = {x, y};
        if (best != std::pair<int, int>{f.x, f.y})
            battleMove(f.id, best.first, best.second);
        if (!f.walk.empty())
            return true;
    }
    if (!b.over && f.acting)
        battleAct(f.id, "wait");                    // (Crouched and not having bitten: it lies in wait.)
    return true;
}
} // namespace ratw
