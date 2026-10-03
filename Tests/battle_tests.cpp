// Turn-based fights in arenas (Core/RatwBattle.h; Docs/Design/33-combat.md), in the demo world.
#include "RatwWire.h"
#include "RatwWorld.h"
#include "battle_play.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
// Residents stay where they are (they follow nobody who exists), so nothing walks into a test.
void quiet(World& w)
{
    for (const auto& e : w.entities())
        if (e.second.npc)
            w.entity(e.first)->leaderId = "test-frozen";
}
Entity& wolf(World& w, const std::string& id, double dx = 0)
{
    auto& e = w.addPlayer(id, id);
    const auto* first = w.entity("ada");
    if (first && id != "ada")
    {
        e.cellId = first->cellId;
        e.position = {first->position.x + dx, first->position.y};
    }
    return e;
}
Battle& fight(World& w, const std::string& id)
{
    auto* b = const_cast<Battle*>(w.battleOf(id));
    expect(b, id + " is in a fight");
    return *b;
}
// Lets turns that aren't `id`'s go by (the others wait at once).
void untilTurnOf(World& w, const std::string& id, const std::vector<std::string>& waiters, double limit = 200)
{
    for (double t = 0; t < limit; t += .1)
    {
        const auto* b = w.battleOf(id);
        if (!b || b->over || b->turn == id)
            return;
        for (const auto& other : waiters)
            if (b->turn == other)
                w.battleAct(other, "wait");
        w.tick(.1);
    }
}
std::pair<int, int> besideOn(World& w, const std::string& id, const std::string& other)
{
    const auto& b = fight(w, id);
    const auto* o = b.fighter(other);
    for (const auto& [x, y] : w.battleReach(id))
        if (test::apart(x, y, o->x, o->y) == 1)
            return {x, y};
    return {-1, -1};
}

void challengeAndTurns()
{
    World w;
    quiet(w);
    auto& ada = wolf(w, "ada");
    auto& bo = wolf(w, "bo", 1.2);
    ada.dexterity = 80;
    bo.dexterity = 40;
    // Between players, an attack is a challenge: nothing happens until it is accepted.
    auto r = w.attack("ada", "bo");
    expect(r.ok && !w.inBattle("ada") && w.challengeTo("bo"), "Attacking a player challenges them: " + r.message);
    r = w.answerChallenge("bo", true);
    expect(r.ok && w.inBattle("ada") && w.inBattle("bo"), "Accepted, it is a fight: " + r.message);
    auto& b = fight(w, "ada");
    expect(b.pvp, "a fight between players");
    const auto* c = w.cell(b.cellId);
    expect(b.w == std::min(c->width, battle::ArenaWidth) && b.h == std::min(c->height, battle::ArenaHeight),
           "The arena is twice the view, cut from the cell: " + std::to_string(b.w) + "x" + std::to_string(b.h));
    // Those who started it act at once, quickest first.
    w.tick(.05);
    expect(b.turn == "ada", "Ada, the quicker, goes first");
    // In the world they stand frozen.
    const Vec2 lineup = ada.position;
    w.tick(1);
    expect(ada.position.x == lineup.x && ada.position.y == lineup.y, "Frozen in the world while the fight goes on");
    w.move("ada", 1, 0);
    w.tick(.5);
    expect(ada.position.x == lineup.x && ada.position.y == lineup.y, "No walking in a fight");
    // Moving: only as far as her range, once a turn.
    const auto reach = w.battleReach("ada");
    const auto* me = b.fighter("ada");
    const int range = battle::moveRange(effectiveDexterity(ada), ada.hurt);
    expect(!reach.empty(), "She has somewhere to go");
    for (const auto& [x, y] : reach)
        expect(test::apart(x, y, me->x, me->y) <= range, "never past her range");
    const auto to = besideOn(w, "ada", "bo");
    expect(to.first >= 0, "She can get next to Bo");
    expect(w.battleMove("ada", to.first, to.second).ok, "and does");
    expect(!w.battleMove("ada", me->x, me->y + 1).ok, "One move a turn");
    expect(!w.battleAct("bo", "wait").ok, "Nobody acts out of turn");
    const double stamina = ada.stamina;
    r = w.battleAct("ada", "bite", "bo");
    expect(r.ok, "A bite: " + r.message);
    expect(std::abs(ada.stamina - (stamina - battle::BiteStamina)) < 1e-9, "It costs stamina");
    expect(b.turn != "ada", "Moved and acted: her turn is over");
    // Bo lets his turns run out: each counts as waiting, and after three he is away and skipped at once.
    w.tick(.05);
    expect(b.turn == "bo", "Bo's turn");
    for (double t = 0; t < battle::TurnSeconds + .5; t += .5)
        w.tick(.5);
    expect(b.fighter("bo")->timeouts == 1 && b.log.back().kind == "timeout", "Time runs out: it counts as waiting");
    for (int i = 0; i < 400 && !b.fighter("bo")->away; ++i)
    {
        if (b.turn == "ada")
            w.battleAct("ada", "wait");
        w.tick(.5);
    }
    expect(b.fighter("bo")->away, "Three in a row: away");
    w.battleAct("ada", "wait");
    w.tick(.05);
    expect(b.turn == "ada", "An away player's turn is skipped at once");
    // Coming back: acting again (any fight command) brings him back.
    expect(w.battleAct("bo", "wait").ok == false && !b.fighter("bo")->away, "Any try brings him back");
}

void downedAndBackToTheWorld()
{
    World w;
    quiet(w);
    auto& ada = wolf(w, "ada");
    auto& bo = wolf(w, "bo", 1.2);
    ada.dexterity = 90;
    ada.strength = 90;
    w.attack("ada", "bo");
    w.answerChallenge("bo", true);
    bo.hurt = 95;
    auto* b = &fight(w, "ada");
    const std::string battleId = b->id;
    bool downed = false;
    for (int i = 0; i < 2000 && !downed; ++i)
    {
        test::playTurn(w, "ada");
        if (w.battleOf("bo") && w.battleOf("bo")->turn == "bo")
            w.battleAct("bo", "wait");
        w.tick(.1);
        downed = bo.downedLeft > 0;
    }
    expect(downed, "Bitten past his last: Bo is Downed");
    expect(bo.downedLeft >= battle::DownedBite * battle::DownedMinimum && bo.downedLeft <= battle::DownedBite,
           "with a bite's timer: " + std::to_string(bo.downedLeft));
    expect(bo.state == "downed" && bo.posture == "lying" && !bo.dead, "lying, not dead");
    b = &fight(w, "ada");
    expect(b->over, "His side has nobody standing: the fight is over");
    expect(b->banner.find("ada's side stands") != std::string::npos, "and says so: " + b->banner);
    const auto tileA = std::pair<int, int>{b->fighter("ada")->x, b->fighter("ada")->y};
    const auto tileB = std::pair<int, int>{b->fighter("bo")->x, b->fighter("bo")->y};
    for (int i = 0; i < 40; ++i)
        w.tick(.1);
    expect(!w.battle(battleId), "After the banner, the arena closes");
    expect(!w.inBattle("ada") && !w.inBattle("bo"), "Everyone is back in the world");
    expect(std::floor(ada.position.x) == tileA.first && std::floor(ada.position.y) == tileA.second,
           "Ada fades in where she stood in the arena");
    expect(std::abs(bo.position.x - (tileB.first + .5)) <= 3.6 && std::abs(bo.position.y - (tileB.second + .5)) <= 3.6,
           "and Bo where he fell (or the nearest open spot)");
    expect(bo.downedLeft > 0, "still Downed, the timer running");
    expect(!w.attack("ada", "bo").ok, "Nobody fights one who is down");
    // Out of a fight he can struggle up, once a day.
    expect(w.struggleUp("bo").ok, "He struggles");
    for (int i = 0; i < 25; ++i)
        w.tick(1);
    expect(bo.downedLeft <= 0 && std::abs(bo.hurt - 85) < 1, "and is up, on his last legs");
    // Down again the same day: he can't get up by himself.
    bo.hurt = 100;
    bo.downedLeft = 30;
    bo.state = "downed";
    expect(!w.struggleUp("bo").ok, "Once a day only");
    for (int i = 0; i < 35; ++i)
        w.tick(1);
    expect(bo.dead, "Left untended, the timer runs out: he dies");
    expect(w.setDead("bo", true).ok == false, "and stays dead");
}

void deathInAFight()
{
    World w;
    quiet(w);
    auto& ada = wolf(w, "ada");
    auto& bo = wolf(w, "bo", 1.2);
    auto& cy = wolf(w, "cy", -1.2);
    ada.dexterity = 90;
    ada.strength = 100;
    w.attack("ada", "bo");
    w.answerChallenge("bo", true);
    auto& b = fight(w, "ada");
    expect(w.joinBattle("cy", b.id, 1).ok, "Cy joins on Bo's side: anyone may join");
    expect(b.fighter("cy")->meter == 0, "A joiner starts with an empty meter");
    expect(b.onEdge(b.fighter("cy")->x, b.fighter("cy")->y), "and comes in at the edge");
    bo.hurt = 95;
    for (int i = 0; i < 3000 && bo.downedLeft <= 0; ++i)
    {
        test::playTurn(w, "ada");
        for (const auto* other : {"bo", "cy"})
            if (w.battleOf(other) && w.battleOf(other)->turn == other)
                w.battleAct(other, "wait");
        w.tick(.1);
    }
    expect(bo.downedLeft > 0 && !b.over, "Bo is down, Cy still stands: the fight goes on");
    // His turns still come: each takes a minute off his timer. With little left and his getting-up spent, he dies.
    bo.downedLeft = 30;
    bo.recoveryUsed = std::floor(w.calendarDays());
    for (int i = 0; i < 2000 && !bo.dead; ++i)
    {
        for (const auto* other : {"ada", "cy"})
            if (w.battleOf(other) && w.battleOf(other)->turn == other)
                w.battleAct(other, "wait");
        w.tick(.1);
    }
    expect(bo.dead && b.fighter("bo")->status == "dead", "Bo dies in the fight");
}

void watchingJoiningFleeing()
{
    World w;
    quiet(w);
    auto& ada = wolf(w, "ada");
    wolf(w, "bo", 1.2);
    auto& cy = wolf(w, "cy", -1.2);
    auto& dee = wolf(w, "dee", 2.4);
    auto& kit = wolf(w, "kit", -2.4);
    kit.age = 9;
    w.attack("ada", "bo");
    w.answerChallenge("bo", true);
    auto& b = fight(w, "ada");
    // Watching: bodiless, and only ever watching after.
    expect(w.observeBattle("cy", b.id).ok && w.watching("cy") == &b, "Cy watches");
    expect(!w.joinBattle("cy", b.id, 0).ok, "Having watched, she can't join");
    expect(w.leaveObserving("cy").ok && !w.watching("cy"), "She stops watching");
    expect(!w.joinBattle("cy", b.id, 0).ok && w.observeBattle("cy", b.id).ok, "and may only come back to watch");
    expect(!cy.dead && !w.inBattle("cy"), "Watching is not fighting");
    expect(!w.joinBattle("kit", b.id, 0).ok, "The young can't join");
    expect(!w.attack("kit", "ada").ok, "nor start one");
    // The arena grows with the fight.
    const int before = b.w;
    expect(w.joinBattle("dee", b.id, 0).ok, "Dee joins Ada's side");
    const auto* c = w.cell(b.cellId);
    expect(b.w == std::min(c->width, before + 1) || b.w == before, "a fighter past two adds room, up to the cell");
    // Fleeing: from the arena's edge, out for good.
    auto* bf = b.fighter("bo");
    bf->x = b.x0 + 1;
    bf->y = b.y0 + b.h / 2;
    for (auto& f : b.fighters)
        if (f.id != "bo" && test::apart(f.x, f.y, bf->x, bf->y) <= 1)
            f.x = b.x0 + b.w - 3;
    untilTurnOf(w, "bo", {"ada", "dee"});
    expect(b.turn == "bo", "Bo's turn");
    const auto fled = w.battleAct("bo", "flee");
    expect(fled.ok && b.fighter("bo")->status == "fled", "Bo flees from the edge: " + fled.message);
    const auto* bo = w.entity("bo");
    expect(std::abs(bo->position.x - (b.x0 + 1.5)) <= 3.6, "and fades into the world at the edge he fled from");
    expect(!w.joinBattle("bo", b.id, 1).ok, "He can't come back to fight");
    expect(b.over, "His side is empty: the fight is over");
    expect(!w.attack("bo", "dee").ok, "He can't start a new fight with them straight away");
    for (int i = 0; i < 40; ++i)
        w.tick(.1);
    expect(!w.inBattle("ada") && !w.watching("cy"), "The arena closes; watchers go back to the world");
    (void)ada;
    (void)dee;
}

void aTimidResidentRuns()
{
    World w;
    quiet(w);
    auto& ada = wolf(w, "ada");
    // Any resident of the demo world: none of them is a fighter by trade.
    std::string resident;
    for (const auto& [id, e] : w.entities())
        if (e.npc && !e.transient && e.cellId == ada.cellId && e.age >= battle::YoungestFighter)
        {
            resident = id;
            break;
        }
    if (resident.empty())
        return;                                     // (No one to try it on in this world.)
    auto* npc = w.entity(resident);
    npc->leaderId.clear();
    if (w.temperamentOf(*npc).kind != "timid")
        return;
    ada.position = {npc->position.x - 1, npc->position.y};
    const auto r = w.attack("ada", resident);
    expect(r.ok && w.inBattle(resident), "A resident set on is in the fight: " + r.message);
    npc->hurt = 50;
    bool ran = false;
    for (int i = 0; i < 4000 && !ran && w.inBattle("ada"); ++i)
    {
        if (w.battleOf("ada")->turn == "ada")
            w.battleAct("ada", "wait");
        w.tick(.1);
        ran = !w.inBattle(resident);
    }
    expect(ran, "Hurt and timid, it makes for the edge and runs");
}

void downedIsSaved()
{
    Entity e;
    e.id = "p";
    e.name = "P";
    e.hurt = 100;
    e.downedLeft = 321;
    e.recoveryUsed = 4;
    const auto back = wire::readEntity(wire::persistEntity(e, 0));
    expect(back.downedLeft == 321 && back.recoveryUsed == 4 && back.hurt == 100, "Downed, and the day's getting-up, are saved");
}

void rules()
{
    expect(battle::moveRange(50, 0) == 5, "DEX 50, unhurt: five tiles");
    expect(battle::moveRange(50, 99) == 2 && battle::moveRange(0, 100) == 1, "Hurt moves less, never none");
    expect(battle::meterGain(50) == 11 && battle::meterGain(100) == 16, "The meter fills by dexterity");
    expect(battle::octant(1, 0) == 0 && battle::octant(0, 1) == 2 && battle::octant(-1, 0) == 4, "Facings");
    expect(battle::octantGap(0, 4) == 4 && battle::octantGap(7, 1) == 2, "Facing gaps wrap");
    const auto guard = battle::temperament("guard", false, 30, true);
    const auto old = battle::temperament("cook", false, 70, true);
    expect(guard.kind == "aggressive" && guard.skill == 70 && guard.fleeBelow == 10, "Guards press in");
    expect(old.kind == "timid" && old.skill == 10 && old.fleeBelow == 80, "An old cook is timid, and runs early");
    expect(battle::temperament("bandit", true, 30, true).kind == "aggressive", "Bandits are aggressive");
}
} // namespace

int main()
{
    try
    {
        rules();
        challengeAndTurns();
        downedAndBackToTheWorld();
        deathInAFight();
        watchingJoiningFleeing();
        aTimidResidentRuns();
        downedIsSaved();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "Battle tests passed: " << checks << " checks.\n";
    return 0;
}
