// Not a test: what decides a fight between ungifted wolves (Docs/Design/44-levelling.md), on the real fight rules
// (Core/RatwBattle.cpp): a level gap (a level is +0.5 fighting skill, nothing else), striking first, gear and numbers.
// Each wolf closes on the nearest foe and bites (or cuts, with a sword) until one side is down, hundreds of times over.
//
//   build-gifts/level_sim [fights]
#include "RatwBattle.h"
#include "RatwWorld.h"
#include "battle_play.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using namespace ratw;
namespace
{
struct Wolf
{
    int level = 1;
    bool sword = false, armour = false;     // A bit-sword; a leather kit (barding, gorget, cap, leg guards).
};

// A wolf's turn: close on the nearest foe still standing, and strike when in reach.
void play(World& w, const std::string& id)
{
    const auto* b = w.battleOf(id);
    if (!b || b->over || !test::acting(b, id))
        return;
    const auto* me = b->fighter(id);
    if (!me->walk.empty())
        return;
    if (me->status != "fighting")
    {
        w.battleAct(id, "wait");
        return;
    }
    const bool sword = w.entity(id)->mouth == "sword";
    const int reach = sword ? battle::SwordReach : 1;
    const BattleFighter* mark = nullptr;
    for (const auto& o : b->fighters)
        if (o.side != me->side && o.status == "fighting" && (!mark || test::apart(me->x, me->y, o.x, o.y) < test::apart(me->x, me->y, mark->x, mark->y)))
            mark = &o;
    if (!mark)
    {
        w.battleAct(id, "wait");
        return;
    }
    const std::string markId = mark->id;
    if (test::apart(me->x, me->y, mark->x, mark->y) > reach && !me->moved)
    {
        std::pair<int, int> to{me->x, me->y};
        int best = test::apart(me->x, me->y, mark->x, mark->y);
        for (const auto& [x, y] : w.battleReach(id))
            if (const int d = test::apart(x, y, mark->x, mark->y); d < best)
            {
                best = d;
                to = {x, y};
            }
        if (to != std::pair<int, int>{me->x, me->y})
            w.battleMove(id, to.first, to.second);
        if ((b = w.battleOf(id)) && b->fighter(id) && !b->fighter(id)->walk.empty())
            return;
    }
    if ((b = w.battleOf(id)) && !b->over && test::acting(b, id))
    {
        me = b->fighter(id);
        const auto* m = b->fighter(markId);
        if (m && m->status == "fighting" && test::apart(me->x, me->y, m->x, m->y) <= reach)
            w.battleAct(id, sword ? "sword" : "bite", markId);
    }
    if ((b = w.battleOf(id)) && !b->over && test::acting(b, id))
        w.battleAct(id, "wait");
}

// One fight: side A against side B, the first of A (or of B) starting it and striking first. 1 if A stands, -1 if B
// does, 0 if neither side went down in half an hour.
int fight(const std::vector<Wolf>& a, const std::vector<Wolf>& b, int trial, bool aFirst)
{
    World w;
    for (const auto& e : w.entities())
        if (e.second.npc)
            w.entity(e.first)->leaderId = "test-frozen";
    std::map<std::string, int> level;
    w.levelOf = [&](const std::string& id) { const auto l = level.find(id); return l == level.end() ? 1 : l->second; };
    std::vector<std::string> ids[2];
    const auto make = [&](const Wolf& wolf, int side, int i) {
        const std::string id = std::string(side ? "player-b" : "player-a") + std::to_string(i) + "-" + std::to_string(trial);
        auto& e = w.addPlayer(id, id);
        level[id] = wolf.level;
        if (wolf.sword)
            e.mouth = "sword";
        if (wolf.armour)
            for (const auto& [slot, item] : {std::pair<const char*, const char*>{"body", "leather_barding"}, {"throat", "leather_gorget"},
                                            {"head", "leather_cap"}, {"paws", "leg_guards"}})
                e.worn[slot] = item;
        ids[side].push_back(id);
        return &e;
    };
    auto* lead = make(a[0], 0, 0);
    auto* foe = make(b[0], 1, 0);
    foe->cellId = lead->cellId;
    foe->position = {lead->position.x + 1.2, lead->position.y};
    const std::string first = aFirst ? ids[0][0] : ids[1][0];
    const std::string second = aFirst ? ids[1][0] : ids[0][0];
    const std::string cellId = lead->cellId;
    w.attack(first, second, "death");
    w.answerChallenge(second, true);
    auto* fightP = const_cast<Battle*>(w.battleOf(first));
    if (!fightP)
        return 0;
    // The rest of each side, beside their first (put in directly: a fight between players is one to one to begin).
    for (int side = 0; side < 2; ++side)
        for (std::size_t i = 1; i < (side ? b : a).size(); ++i)
        {
            auto* e = make((side ? b : a)[i], side, int(i));
            e->cellId = cellId;
            const auto* near = fightP->fighter(ids[side][0]);
            BattleFighter f;
            f.id = e->id;
            f.side = near->side;
            f.facing = near->facing;
            bool placed = false;
            for (int r = 1; r <= 3 && !placed; ++r)
                for (int dy = -r; dy <= r && !placed; ++dy)
                    for (int dx = -r; dx <= r && !placed; ++dx)
                        if (w.arenaOpen(*fightP, near->x + dx, near->y + dy))
                        {
                            f.x = near->x + dx;
                            f.y = near->y + dy;
                            placed = true;
                        }
            fightP->fighters.push_back(f);
        }
    test::takeGround(w, first);
    const int firstSide = aFirst ? 0 : 1;
    for (double t = 0; t < 1800; t += .1)
    {
        const auto* now = w.battleOf(first);
        if (!now || now->over)
            break;
        // Several bars full at once (the first turn): the side that started acts first.
        for (int s : {firstSide, 1 - firstSide})
            for (const auto& id : ids[s])
                play(w, id);
        w.tick(.1);
    }
    const auto standing = [&](int side) {
        for (const auto& id : ids[side])
            if (const auto* e = w.entity(id); e && e->hurt < 100)
                return true;
        return false;
    };
    const bool aUp = standing(0), bUp = standing(1);
    return aUp == bUp ? 0 : aUp ? 1 : -1;
}

// Side A's wins of the fights decided: starts alternated (`order` 0), or always A (1) or always B (-1).
double winRate(const std::vector<Wolf>& a, const std::vector<Wolf>& b, int fights, int order = 0)
{
    int wins = 0, decided = 0;
    for (int i = 0; i < fights; ++i)
    {
        const int r = fight(a, b, i, order == 0 ? i % 2 == 0 : order > 0);
        decided += r != 0;
        wins += r > 0;
    }
    return decided ? 100.0 * wins / decided : 0;
}

void row(const char* what, double rate)
{
    std::printf("  %-50s %5.1f%%\n", what, rate);
    std::fflush(stdout);
}
} // namespace

int main(int argc, char** argv)
{
    const int n = argc > 1 ? std::atoi(argv[1]) : 300;
    const Wolf L1{1}, L5{5}, L10{10}, L15{15}, L20{20}, L25{25};
    std::printf("Ungifted wolves, to the ground; %d fights each, who starts alternated unless said. Wins for the first named.\n", n);
    std::printf("\nLevels (a level: +0.5 fighting skill)\n");
    row("L1 vs L1", winRate({L1}, {L1}, n));
    row("L5 vs L1", winRate({L5}, {L1}, n));
    row("L10 vs L1", winRate({L10}, {L1}, n));
    row("L25 vs L1", winRate({L25}, {L1}, n));
    row("L10 vs L5", winRate({L10}, {L5}, n));
    row("L25 vs L20", winRate({L25}, {L20}, n));
    row("L25 vs L15", winRate({L25}, {L15}, n));
    std::printf("\nStriking first (tactics)\n");
    row("L1 vs L1, L1 strikes first", winRate({L1}, {L1}, n, 1));
    row("L1 vs L5, L1 strikes first", winRate({L1}, {L5}, n, 1));
    row("L1 vs L10, L1 strikes first", winRate({L1}, {L10}, n, 1));
    row("L1 vs L25, L1 strikes first", winRate({L1}, {L25}, n, 1));
    std::printf("\nGear\n");
    row("L1 in a leather kit vs L25 bare", winRate({{1, false, true}}, {L25}, n));
    row("L1 with a sword vs L25 bare", winRate({{1, true, false}}, {L25}, n));
    row("L1 with a sword and leather vs L25 bare", winRate({{1, true, true}}, {L25}, n));
    row("L1 with a sword and leather vs L25 the same", winRate({{1, true, true}}, {{25, true, true}}, n));
    std::printf("\nNumbers\n");
    row("two L1 vs one L25", winRate({L1, L1}, {L25}, n));
    row("two L5 vs one L15", winRate({L5, L5}, {L15}, n));
    row("two L1 vs two L25", winRate({L1, L1}, {L25, L25}, n));
    row("three L1 vs two L25", winRate({L1, L1, L1}, {L25, L25}, n));
    return 0;
}
