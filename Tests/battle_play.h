#pragma once
// A scripted fighter for tests (Docs/Design/33-combat.md): on its turn it closes on the nearest enemy still standing
// and bites it, then ends the turn.
#include "RatwWorld.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string>

namespace ratw::test
{
inline int apart(int ax, int ay, int bx, int by)
{
    return std::max(std::abs(ax - bx), std::abs(ay - by));
}

// Plays one turn if it is `id`'s; true if it did.
inline bool playTurn(World& w, const std::string& id)
{
    const auto* b = w.battleOf(id);
    if (!b || b->over || b->turn != id)
        return false;
    const auto* me = b->fighter(id);
    if (me->status != "fighting")
    {
        w.battleAct(id, "wait");
        return true;
    }
    const BattleFighter* mark = nullptr;
    int best = std::numeric_limits<int>::max();
    for (const auto& o : b->fighters)
        if (o.side != me->side && o.status == "fighting" && apart(me->x, me->y, o.x, o.y) < best)
        {
            best = apart(me->x, me->y, o.x, o.y);
            mark = &o;
        }
    if (!mark)
    {
        w.battleAct(id, "wait");
        return true;
    }
    const std::string markId = mark->id;
    if (best > 1)
    {
        std::pair<int, int> to{me->x, me->y};
        int closest = best;
        for (const auto& [x, y] : w.battleReach(id))
            if (apart(x, y, mark->x, mark->y) < closest)
            {
                closest = apart(x, y, mark->x, mark->y);
                to = {x, y};
            }
        if (to != std::pair<int, int>{me->x, me->y})
            w.battleMove(id, to.first, to.second);
    }
    if ((b = w.battleOf(id)) && !b->over && b->turn == id)
    {
        me = b->fighter(id);
        const auto* m = b->fighter(markId);
        if (m && apart(me->x, me->y, m->x, m->y) == 1)
            w.battleAct(id, "bite", markId);
    }
    if ((b = w.battleOf(id)) && !b->over && b->turn == id)
        w.battleAct(id, "wait");
    return true;
}
} // namespace ratw::test
