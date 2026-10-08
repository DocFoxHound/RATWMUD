#pragma once
// Scripted hunters for the hunt simulation and tests (Docs/Design/53-hunting-and-working-together.md): a lone stalker
// who crouches when game is near, creeps up on the nearest animal and bites it. A move is walked, so a turn may take a
// few calls (with ticks between) to finish.
#include "RatwItems.h"
#include "RatwWild.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

namespace ratw::hunt
{
inline int apart(int ax, int ay, int bx, int by)
{
    return std::max(std::abs(ax - bx), std::abs(ay - by));
}

// What an animal's kill is worth at the catalog's prices (a rabbit 4 pennies, a red deer 35).
inline double gameWorth(const std::string& species)
{
    double value = 0;
    if (const auto* s = wild::speciesById(species))
        for (const auto& [item, n] : s->yield)
            if (const auto* g = items::good(item))
                value += double(g->price) * n;
    return std::max(1.0, value);
}

// The animal to go after: the nearest still unaware (grazing) first, then one watching, then one running. A pack
// (`byWorth`) weighs that against what each is worth: worth the long way round for a deer.
inline const BattleFighter* nearestGame(const World& w, const Battle& b, const BattleFighter& me, bool byWorth = false)
{
    const BattleFighter* mark = nullptr;
    double best = std::numeric_limits<double>::max();
    for (const auto& o : b.fighters)
    {
        if (o.side == me.side || o.status != "fighting" || w.animalOf(o.id).empty())
            continue;
        const auto state = w.animalState(o.id);
        double score = apart(me.x, me.y, o.x, o.y) + (state == "watching" ? 15 : state == "fleeing" || state == "calming" ? 40 : 0);
        if (byWorth)
            score /= std::sqrt(gameWorth(w.animalOf(o.id)));
        if (score < best)
            best = score, mark = &o;
    }
    return mark;
}

// A lone stalker's turn, if it is `id`'s; true if it played (or is walking). A pack member's stalk weighs worth.
inline bool stalkerTurn(World& w, const std::string& id, bool byWorth = false)
{
    const auto* b = w.battleOf(id);
    const auto* me = b ? b->fighter(id) : nullptr;
    if (!b || b->over || !me || !me->acting)
        return false;
    if (!me->walk.empty())
        return true;
    if (me->status != "fighting")
    {
        w.battleAct(id, "wait");
        return true;
    }
    const auto* mark = nearestGame(w, *b, *me, byWorth);
    if (!mark)
    {
        w.battleAct(id, "wait");
        return true;
    }
    const std::string markId = mark->id;
    int gap = apart(me->x, me->y, mark->x, mark->y);
    // Crouched to creep up unseen; risen to rush one that is watching once it is close (doc 53: the 60% row).
    if (!me->moved)
    {
        const bool rush = w.animalState(markId) == "watching" && gap <= 6;
        if (rush && me->stalking)
            w.battleAct(id, "rise");
        else if (!rush && !me->stalking)
            w.battleAct(id, "stalk");
    }
    b = w.battleOf(id);
    me = b->fighter(id);
    mark = b->fighter(markId);
    if (gap > 1 && !me->moved)
    {
        std::pair<int, int> to{me->x, me->y};
        int closest = gap;
        for (const auto& [x, y] : w.battleReach(id))
            if (const int d = apart(x, y, mark->x, mark->y); d < closest && d >= 1)
            {
                closest = d;
                to = {x, y};
            }
        if (to != std::pair<int, int>{me->x, me->y})
            w.battleMove(id, to.first, to.second);
        b = w.battleOf(id);
        if (b && b->fighter(id) && !b->fighter(id)->walk.empty())
            return true;
    }
    b = w.battleOf(id);
    if (b && !b->over && b->fighter(id) && b->fighter(id)->acting)
    {
        me = b->fighter(id);
        const auto* m = b->fighter(markId);
        if (m && m->status == "fighting" && apart(me->x, me->y, m->x, m->y) == 1)
            w.battleAct(id, "bite", markId);
    }
    b = w.battleOf(id);
    if (b && !b->over && b->fighter(id) && b->fighter(id)->acting)
        w.battleAct(id, "wait");
    return true;
}
} // namespace ratw::hunt

namespace ratw::hunt
{
// ------------------------------------------------------------------ Hunting together (doc 53, Phase 2)

// Moves toward (gx, gy) as far as this turn allows; true while walking there.
inline bool moveToward(World& w, const std::string& id, double gx, double gy, int keepFrom = -1, int kx = 0, int ky = 0)
{
    const auto* b = w.battleOf(id);
    const auto* me = b ? b->fighter(id) : nullptr;
    if (!me || me->moved)
        return false;
    std::pair<int, int> best{me->x, me->y};
    double bestD = std::hypot(me->x - gx, me->y - gy);
    for (const auto& [x, y] : w.battleReach(id))
    {
        if (keepFrom >= 0 && apart(x, y, kx, ky) < keepFrom)
            continue;
        if (const double d = std::hypot(x - gx, y - gy); d < bestD)
            bestD = d, best = {x, y};
    }
    if (best != std::pair<int, int>{me->x, me->y})
        w.battleMove(id, best.first, best.second);
    b = w.battleOf(id);
    return b && b->fighter(id) && !b->fighter(id)->walk.empty();
}

inline bool biteIfBeside(World& w, const std::string& id)
{
    const auto* b = w.battleOf(id);
    const auto* me = b ? b->fighter(id) : nullptr;
    if (!me || me->acted)
        return false;
    for (const auto& o : b->fighters)
        if (o.side != me->side && o.status == "fighting" && !w.animalOf(o.id).empty() && apart(me->x, me->y, o.x, o.y) == 1)
        {
            w.battleAct(id, "bite", o.id);
            return true;
        }
    return false;
}

} // namespace ratw::hunt

namespace ratw::hunt
{
// A pack member's turn (doc 53): it stalks unaware game as a lone wolf does; an animal watching a packmate it hasn't
// noticed this wolf for, it circles round to the far side of and lies in wait; one watching this wolf it holds off
// until a packmate lies in wait beyond it, then steps in so it bolts into them.
inline bool packTurn(World& w, const std::string& id, const std::vector<std::string>& pack)
{
    const auto* b = w.battleOf(id);
    const auto* me = b ? b->fighter(id) : nullptr;
    if (!b || b->over || !me || !me->acting)
        return false;
    if (!me->walk.empty())
        return true;
    if (me->status != "fighting")
    {
        w.battleAct(id, "wait");
        return true;
    }
    biteIfBeside(w, id);
    b = w.battleOf(id);
    me = b ? b->fighter(id) : nullptr;
    if (!b || b->over || !me || !me->acting)
        return true;
    // An animal a packmate is watched by, that hasn't noticed this wolf: ambush it.
    const BattleFighter* ambush = nullptr;
    const BattleFighter* watchedMate = nullptr;
    for (const auto& o : b->fighters)
    {
        if (w.animalOf(o.id).empty() || o.status != "fighting" || w.animalState(o.id) != "watching")
            continue;
        const auto watcher = w.animalWatching(o.id);
        if (watcher == id || std::find(pack.begin(), pack.end(), watcher) == pack.end() || w.awareness(*b, o.id, id) >= .99)
            continue;
        if (const auto* mate = b->fighter(watcher); !mate || apart(mate->x, mate->y, o.x, o.y) > 11)
            continue;                               // (The watched one is too far off to drive it.)
        if (!ambush || apart(me->x, me->y, o.x, o.y) < apart(me->x, me->y, ambush->x, ambush->y))
            ambush = &o, watchedMate = b->fighter(watcher);
    }
    if (ambush && watchedMate && !me->moved && !me->acted && apart(me->x, me->y, ambush->x, ambush->y) <= 25)
    {
        if (!me->stalking)
            w.battleAct(id, "stalk");
        const double dx = ambush->x - watchedMate->x, dy = ambush->y - watchedMate->y, len = std::max(1.0, std::hypot(dx, dy));
        if (moveToward(w, id, ambush->x + dx / len * 2, ambush->y + dy / len * 2, 2, ambush->x, ambush->y))
            return true;
        b = w.battleOf(id);
        if (b && !b->over && b->fighter(id) && b->fighter(id)->acting)
            w.battleAct(id, "wait");                // (Crouched: lying in wait for it.)
        return true;
    }
    // One watching this wolf: hold off until a packmate lies in wait beyond it, then step in.
    for (const auto& o : b->fighters)
        if (!w.animalOf(o.id).empty() && o.status == "fighting" && w.animalState(o.id) == "watching" && w.animalWatching(o.id) == id &&
            pack.size() > 1 && !me->moved)
        {
            bool ready = false, coming = false;
            for (const auto& m : pack)
                if (const auto* f = b->fighter(m); m != id && f && f->status == "fighting")
                {
                    const bool beyond = (f->x - o.x) * (o.x - me->x) + (f->y - o.y) * (o.y - me->y) >= 0;
                    ready = ready || (w.huntWaiting(m) && apart(f->x, f->y, o.x, o.y) <= 4 && beyond);
                    coming = coming || (apart(f->x, f->y, o.x, o.y) <= 14 && w.awareness(*b, o.id, m) < .99);
                }
            if (!ready && !coming)
                break;                              // (No one to drive it to: hunt it as a lone wolf would.)
            const std::string species = w.animalOf(o.id);
            const double flight = species == "rabbit" || species == "grouse" ? 3.5 : 6;
            if (me->stalking)
                w.battleAct(id, "rise");
            const double dx = me->x - o.x, dy = me->y - o.y, len = std::max(1.0, std::hypot(dx, dy));
            const double stand = ready ? std::max(1.0, flight - 1) : flight + 2;
            moveToward(w, id, o.x + dx / len * stand, o.y + dy / len * stand, ready ? -1 : int(flight) + 1, o.x, o.y);
            b = w.battleOf(id);
            if (b && b->fighter(id) && !b->fighter(id)->walk.empty())
                return true;
            b = w.battleOf(id);
            if (b && !b->over && b->fighter(id) && b->fighter(id)->acting)
                w.battleAct(id, "wait");
            return true;
        }
    return stalkerTurn(w, id, true);                // Otherwise, a stalk on unaware game, worth weighed.
}
} // namespace ratw::hunt
