// Turn-based fights in arenas (Core/RatwBattle.h; Docs/Design/33-combat.md), in the demo world.
#include "RatwCheckpoint.h"
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
        if (!b || b->over || test::acting(b, id))
            return;
        for (const auto& other : waiters)
            if (test::acting(b, other))
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
    // Those who started it act at once: both their bars began full. Turns don't wait on each other (doc 33).
    w.tick(.05);
    expect(test::acting(&b, "ada") && test::acting(&b, "bo"), "Both act at once");
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
    const auto from = std::pair<int, int>{me->x, me->y};
    expect(w.battleMove("ada", to.first, to.second).ok, "and does");
    expect(std::pair<int, int>{me->x, me->y} == from && !me->walk.empty(), "She walks there: a move takes time (doc 33)");
    for (int i = 0; i < 40 && !me->walk.empty(); ++i)
        w.tick(.1);
    expect(std::pair<int, int>{me->x, me->y} == to, "and gets there");
    expect(!w.battleMove("ada", me->x, me->y + 1).ok, "One move a turn");
    expect(w.battleAct("bo", "wait").ok && !test::acting(&b, "bo"), "Bo, in his own turn meanwhile, ends it");
    expect(!w.battleAct("bo", "wait").ok, "and can't act again until his bar fills");
    const double stamina = ada.stamina;
    r = w.battleAct("ada", "bite", "bo");
    expect(r.ok, "A bite: " + r.message);
    expect(std::abs(ada.stamina - (stamina - battle::BiteStamina)) < 1e-9, "It costs stamina");
    expect(test::acting(&b, "ada"), "Moved and acted, her turn goes on until its time or she ends it (doc 33)");
    expect(w.battleFace("ada", (me->facing + 4) % 8).ok, "so she can still turn");
    expect(w.battleAct("ada", "wait").ok && !test::acting(&b, "ada"), "and ends it herself");
    // Bo lets his turns run out: each counts as waiting, and after three he is away and skipped at once.
    for (int i = 0; i < 200 && !test::acting(&b, "bo"); ++i)
        w.tick(.1);
    expect(test::acting(&b, "bo"), "Bo's bar fills: his turn");
    for (double t = 0; t < battle::TurnSeconds + .5; t += .5)
        w.tick(.5);
    expect(b.fighter("bo")->timeouts == 1, "Time runs out: it counts as waiting");
    for (int i = 0; i < 800 && !b.fighter("bo")->away; ++i)
    {
        if (test::acting(&b, "ada"))
            w.battleAct("ada", "wait");
        w.tick(.1);
    }
    expect(b.fighter("bo")->away, "Three in a row: away");
    bool lingered = false;
    for (int i = 0; i < 150; ++i)
    {
        if (test::acting(&b, "ada"))
            w.battleAct("ada", "wait");
        w.tick(.1);
        lingered = lingered || (test::acting(&b, "bo") && b.fighter("bo")->turnStarted < w.time() - .2);
    }
    expect(!lingered && b.fighter("bo")->timeouts == 3, "An away player's turns pass at once");
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
    w.attack("ada", "bo", "death");                  // (To the death: doc 33's Downed and dying.)
    w.answerChallenge("bo", true);
    bo.hurt = 95;
    auto* b = &fight(w, "ada");
    const std::string battleId = b->id;
    bool downed = false;
    for (int i = 0; i < 2000 && !downed; ++i)
    {
        test::playTurn(w, "ada");
        if (w.battleOf("bo") && test::acting(w.battleOf("bo"), "bo"))
            w.battleAct("bo", "wait");
        w.tick(.1);
        downed = bo.downedLeft > 0;
    }
    expect(downed, "Bitten past his last: Bo is Downed");
    expect(bo.downedLeft >= battle::GetUpBite && bo.downedLeft <= battle::GetUpBite + 100 * battle::GetUpOverkillSeconds,
           "down a bite's while, his first downing: " + std::to_string(bo.downedLeft));
    expect(bo.downsSinceRest == 1, "His first downing since a full rest");
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
    // Down again the same day: he can't struggle up, but nobody dies (doc 38): his time down runs out and he gets up.
    bo.hurt = 100;
    bo.downedLeft = 30;
    bo.state = "downed";
    expect(!w.struggleUp("bo").ok, "Once a day only");
    for (int i = 0; i < 35; ++i)
        w.tick(1);
    expect(!bo.dead && bo.downedLeft <= 0 && bo.posture == "standing" && std::abs(bo.hurt - (100 - battle::GetUpHealth)) < 1,
           "Left untended, he gets up when his time is up, sore, alive");
}

void gettingUpInAFight()
{
    World w;
    quiet(w);
    auto& ada = wolf(w, "ada");
    auto& bo = wolf(w, "bo", 1.2);
    auto& cy = wolf(w, "cy", -1.2);
    ada.dexterity = 90;
    ada.strength = 100;
    w.attack("ada", "bo", "death");
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
            if (w.battleOf(other) && test::acting(w.battleOf(other), other))
                w.battleAct(other, "wait");
        w.tick(.1);
    }
    expect(bo.downedLeft > 0 && !b.over, "Bo is down, Cy still stands: the fight goes on");
    // With his getting-up spent and his time down run out, he gets up in the fight: a player never dies (doc 38).
    bo.downedLeft = 3;
    bo.recoveryUsed = std::floor(w.calendarDays());
    for (int i = 0; i < 100 && bo.downedLeft > 0; ++i)
    {
        for (const auto* other : {"ada", "cy"})
            if (w.battleOf(other) && test::acting(w.battleOf(other), other))
                w.battleAct(other, "wait");
        w.tick(.1);
    }
    expect(!bo.dead && b.fighter("bo")->status == "fighting" && std::abs(bo.hurt - (100 - battle::GetUpHealth)) < 1,
           "Bo gets back up in the fight, alive");
    expect(std::any_of(b.log.begin(), b.log.end(), [](const auto& l) { return l.text.find("gets back up") != std::string::npos; }),
           "and the fight says so");
}

// Doc 38: downings without a full rest keep a wolf down longer; only a bed gives a full rest; time away counts.
void restAndRepeatedDowns()
{
    World w;
    quiet(w);
    auto& ada = wolf(w, "ada");
    auto& bo = wolf(w, "bo", 1.2);
    ada.dexterity = 90;
    ada.strength = 100;
    std::vector<double> downs;
    for (int round = 0; round < 5; ++round)
    {
        bo.hurt = 95;
        bo.recoveryUsed = std::floor(w.calendarDays());   // (No struggling up: each time down is waited out.)
        expect(w.attack("ada", "bo", "death").ok && w.answerChallenge("bo", true).ok, "a fight, round " + std::to_string(round));
        for (int i = 0; i < 3000 && bo.downedLeft <= 0; ++i)
        {
            test::playTurn(w, "ada");
            if (w.battleOf("bo") && test::acting(w.battleOf("bo"), "bo"))
                w.battleAct("bo", "wait");
            w.tick(.1);
        }
        downs.push_back(bo.downedLeft);
        for (int i = 0; i < 4000 && (bo.downedLeft > 0 || w.inBattle("bo") || w.inBattle("ada")); ++i)
            w.tick(1);
        expect(!bo.dead && bo.downedLeft <= 0, "He gets up each time, round " + std::to_string(round));
        for (int i = 0; i < 10; ++i)
            w.tick(1);                                // (The settling after a fight.)
    }
    expect(bo.downsSinceRest == 5, "Five downings without rest: " + std::to_string(bo.downsSinceRest));
    const auto stretch = [&](int n) { return downs[n] / downs[0]; };
    expect(stretch(1) > 1.6 && stretch(2) > 3.2 && stretch(3) > 4.8 && stretch(4) > 6.4,
           "each keeps him down longer: " + std::to_string(downs[0]) + " " + std::to_string(downs[4]));
    expect(downs[4] <= battle::GetUpLongest, "never past half an hour");
    // A partial rest: six hours lying on the ground helps, but resets nothing.
    bo.hurt = 0;
    w.setPosture("bo", "lying");
    for (int i = 0; i < 2; ++i)
        w.tick(1);
    for (int i = 0; i < int(battle::FullRestHours * battle::RestHourSeconds / 30) + 2; ++i)
        w.tick(30);
    expect(bo.restRun >= battle::FullRestHours && bo.bedRun == 0, "Lying on the ground is a partial rest: " + std::to_string(bo.restRun));
    expect(bo.downsSinceRest == 5 && !w.recoveryAvailable(bo), "and resets nothing");
    // A full rest: six hours lying in a bed.
    auto* c = w.cell(bo.cellId);
    c->tile(int(std::floor(bo.position.x)), int(std::floor(bo.position.y)))->glyph = 'b';
    expect(w.inBed(bo), "He lies in a bed");
    for (int i = 0; i < int(battle::FullRestHours * battle::RestHourSeconds / 30) + 2; ++i)
        w.tick(30);
    expect(bo.downsSinceRest == 0 && w.recoveryAvailable(bo) && bo.fullRestDay > 0, "Six hours in a bed: a full rest");
    // Getting up breaks a rest.
    w.setPosture("bo", "standing");
    for (int i = 0; i < 4; ++i)
        w.tick(1);
    expect(bo.restRun == 0 && bo.bedRun == 0, "Standing breaks the rest");
    // Away (logged out): the time counts down a downing, then rests; in a bed only if they left lying in one.
    Entity gone = bo;
    gone.downsSinceRest = 3;
    gone.hurt = 100;
    gone.downedLeft = 600;
    gone.awaySince = w.calendarDays() - 1200 / calendar::SecondsPerDay;
    w.returnFromAway(gone);
    expect(gone.downedLeft <= 0 && std::abs(gone.hurt - (100 - battle::GetUpHealth)) < 1 && gone.awaySince < 0,
           "Away longer than his time down: he is up when he comes back");
    expect(gone.downsSinceRest == 3, "but lying down hurt is no rest");
    gone.awaySince = w.calendarDays() - 5 * battle::RestHourSeconds / calendar::SecondsPerDay;
    gone.awayInBed = false;
    gone.restRun = gone.bedRun = 0;
    w.returnFromAway(gone);
    expect(gone.downsSinceRest == 3 && gone.restRun >= 7, "Away five hours, not in a bed: a partial rest, half again");
    gone.awaySince = w.calendarDays() - 5 * battle::RestHourSeconds / calendar::SecondsPerDay;
    gone.awayInBed = true;
    gone.restRun = gone.bedRun = 0;
    w.returnFromAway(gone);
    expect(gone.downsSinceRest == 0, "Away five hours in a bed: 7.5 hours' rest, a full one");
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
    expect(test::acting(&b, "bo"), "Bo's turn");
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
        if (test::acting(w.battleOf("ada"), "ada"))
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
    e.mouth = "sword";
    e.gift = "fire";
    e.quickened = true;
    e.mana = 33;
    e.fightingSkill = 61;
    e.downsSinceRest = 3;
    e.restRun = 2.5;
    e.bedRun = 1.5;
    e.fullRestDay = 12.25;
    e.awaySince = 13.5;
    e.awayInBed = true;
    const auto back = wire::readEntity(wire::persistEntity(e, 0));
    expect(back.downsSinceRest == 3 && back.restRun == 2.5 && back.bedRun == 1.5 && back.fullRestDay == 12.25 &&
               back.awaySince == 13.5 && back.awayInBed,
           "and rest: downings since a full rest, the rest run, the last full rest, time away (doc 38)");
    expect(back.downedLeft == 321 && back.recoveryUsed == 4 && back.hurt == 100, "Downed, and the day's getting-up, are saved");
    expect(back.mouth == "sword" && back.gift == "fire" && back.quickened && back.mana == 33 && back.fightingSkill == 61,
           "and the sword in the jaws, the Gift, its mana, and fighting skill");
}

// Two players in a fight, by challenge: "player-ad" goes first.
Battle& duel(World& w)
{
    quiet(w);
    auto& ad = w.addPlayer("player-ad", "Ad");
    auto& bo = w.addPlayer("player-bo", "Bo");
    bo.cellId = ad.cellId;
    bo.position = {ad.position.x + 1.2, ad.position.y};
    ad.dexterity = 90;
    bo.dexterity = 40;
    w.attack("player-ad", "player-bo", "death");     // (doc 33's rules: these tests go down and die.)
    w.answerChallenge("player-bo", true);
    auto* b = const_cast<Battle*>(w.battleOf("player-ad"));
    expect(b, "a duel");
    w.tick(.05);
    expect(test::acting(b, "player-ad") && test::acting(b, "player-bo"), "Both begin acting at once");
    w.battleAct("player-bo", "wait");               // Bo lets his first turn go: Ad's alone, for the tests.
    return *b;
}

void facingAndTruce()
{
    World w;
    auto& b = duel(w);
    // Turning is free, and only on one's own turn.
    expect(w.battleFace("player-ad", 3).ok && b.fighter("player-ad")->facing == 3, "Ad turns");
    expect(!b.fighter("player-ad")->moved && !b.fighter("player-ad")->acted && test::acting(&b, "player-ad"), "and it costs nothing");
    expect(!w.battleFace("player-bo", 1).ok, "Bo can't turn on Ad's turn");
    expect(!w.battleFace("player-ad", 9).ok, "Facing is one of eight ways");
    // A truce: offered, then agreed by everyone standing, ends it.
    expect(w.offerTruce("player-ad").ok && b.truceBy == "player-ad", "Ad offers a truce");
    expect(!b.over, "It needs Bo's word");
    expect(w.answerTruce("player-bo", false).ok && b.truceBy.empty(), "Bo refuses: it's off");
    expect(!w.offerTruce("player-ad").ok, "One offer a turn (it is her action)");
    w.battleAct("player-ad", "wait");
    untilTurnOf(w, "player-ad", {"player-bo"});
    expect(w.offerTruce("player-ad").ok, "Offered again");
    expect(w.answerTruce("player-bo", true).ok && b.over && b.banner.find("truce") != std::string::npos, "Agreed: the fight ends in a truce");
}

void theSword()
{
    World w;
    auto& b = duel(w);
    auto* ad = w.entity("player-ad");
    auto* bo = w.entity("player-bo");
    expect(!w.holdItem("player-ad", "sword").ok, "No sword to hold");
    expect(w.society().create("player-ad", "sword", 1, "test"), "A sword is made");
    expect(w.society().create("player-bo", "sword", 1, "test"), "and another");
    const auto reachBefore = w.battleReach("player-ad").size();
    expect(w.battleAct("player-ad", "hold").ok && ad->mouth == "sword", "Ad takes it in her jaws");
    expect(!b.fighter("player-ad")->acted && b.fighter("player-ad")->drew, "as part of her move, not her action (doc 37)");
    expect(w.battleReach("player-ad").size() < reachBefore, "a tile off the move, taken before it");
    expect(!w.battleAct("player-ad", "stow").ok, "once a turn");
    w.battleAct("player-ad", "wait");
    untilTurnOf(w, "player-ad", {"player-bo"});
    // Two tiles apart: a sword reaches, a bite doesn't.
    auto* fa = b.fighter("player-ad");
    auto* fb = b.fighter("player-bo");
    fb->x = fa->x + 2;
    fb->y = fa->y;
    expect(w.battleAct("player-ad", "bite", "player-bo").ok == false, "No biting with a sword in the mouth");
    const double stamina = ad->stamina;
    const auto r = w.battleAct("player-ad", "sword", "player-bo");
    expect(r.ok && std::abs(ad->stamina - (stamina - battle::SwordStamina)) < 1e-9, "A sword reaches two tiles: " + r.message);
    expect(b.fighter("player-ad")->weight == battle::SwordWeight, "and is heavy: the next turn comes later");
    // Bo goes down holding his own: it drops, and stays where it fell after the fight.
    bo->mouth = "sword";
    bo->hurt = 99.5;
    w.battleAct("player-ad", "wait");
    for (int i = 0; i < 400 && bo->downedLeft <= 0; ++i)
    {
        if (test::acting(&b, "player-ad"))
        {
            const auto* f = b.fighter("player-ad");
            const auto* o = b.fighter("player-bo");
            ad->stamina = 100;
            ad->exhausted = false;
            if (test::apart(f->x, f->y, o->x, o->y) <= 2)
                w.battleAct("player-ad", "sword", "player-bo");
            else
                for (const auto& [x, y] : w.battleReach("player-ad"))
                    if (test::apart(x, y, o->x, o->y) <= 2)
                    {
                        w.battleMove("player-ad", x, y);
                        w.battleAct("player-ad", "sword", "player-bo");
                        break;
                    }
            if (test::acting(&b, "player-ad"))
                w.battleAct("player-ad", "wait");
        }
        if (test::acting(&b, "player-bo"))
            w.battleAct("player-bo", "wait");
        w.tick(.1);
    }
    expect(bo->downedLeft > 0 && bo->mouth.empty() && b.drops.size() == 1, "Bo goes down and his sword falls (downed " + std::to_string(bo->downedLeft) +
               ", mouth " + bo->mouth + ", drops " + std::to_string(b.drops.size()) + ", over " + std::to_string(b.over) +
               ", hurt " + std::to_string(bo->hurt) + ", last " + b.log.back().text + ")");
    for (int i = 0; i < 40; ++i)
        w.tick(.1);
    expect(w.groundItems().size() == 1 && w.groundItems()[0].item == "sword", "After the fight it lies on the ground");
    const auto g = w.groundItems()[0];
    ad->position = {g.x + .5, g.y};
    const int before = Society::stock(*w.society().account("player-ad"), "sword");
    expect(w.takeItem("player-ad", g.id).ok && w.groundItems().empty(), "Ad picks it up");
    expect(Society::stock(*w.society().account("player-ad"), "sword") == before + 1, "and has it");
    expect(w.entity("player-ad")->fightingSkill > 50, "Fighting taught her something");
}

void theFlame()
{
    World w;
    auto& b = duel(w);
    auto* ad = w.entity("player-ad");
    auto* bo = w.entity("player-bo");
    const auto r0 = w.battleAct("player-ad", "flame", "1,1");
    expect(!r0.ok, "No Gift, no fire");
    expect(w.giveGift("player-ad", "fire", false).ok && ad->mana == battle::manaMax(ad->wisdom, true), "Given the Gift of fire");
    auto* fa = b.fighter("player-ad");
    auto* fb = b.fighter("player-bo");
    fb->x = fa->x + 2;
    fb->y = fa->y;
    const double mana = ad->mana, stamina = ad->stamina;
    const auto r = w.battleAct("player-ad", "flame", std::to_string(fb->x) + "," + std::to_string(fb->y));
    expect(r.ok && fa->casting && b.casts.size() == 1, "She gathers the fire: " + r.message);
    expect(mana - ad->mana == battle::GiftedFlame.mana && stamina - ad->stamina == battle::GiftedFlame.stamina, "Mana and breath");
    expect(std::abs(ad->hurt - battle::GiftedFlame.self) < 1e-9, "and a singed muzzle");
    const auto& tiles = b.casts[0].tiles;
    expect(std::find(tiles.begin(), tiles.end(), std::pair<int, int>{fb->x, fb->y}) != tiles.end(), "The cone takes in Bo's tile");
    expect(test::acting(&b, "player-ad") && !w.battleMove("player-ad", fa->x + 1, fa->y).ok,
           "Casting, her turn goes on (to turn, or talk) but she can't move");
    expect(w.battleAct("player-ad", "wait").ok && !test::acting(&b, "player-ad"), "She ends it");
    // Bo steps aside? He waits: the fire comes.
    for (int i = 0; i < 200 && !b.casts.empty(); ++i)
    {
        if (test::acting(&b, "player-bo"))
            w.battleAct("player-bo", "wait");
        if (test::acting(&b, "player-ad"))
            w.battleAct("player-ad", "wait");
        w.tick(.1);
    }
    expect(b.casts.empty() && !fa->casting, "It goes off");
    expect(bo->hurt > 0 && fb->burning > 0, "Bo is burnt, and burning");
    untilTurnOf(w, "player-bo", {"player-ad"});
    expect(fb->burning == battle::BurnTurns - 1, "His turn begins with the burn");
    expect(w.battleAct("player-bo", "roll").ok && fb->burning == 0, "Rolling puts it out");
}

void crawling()
{
    World w;
    quiet(w);
    auto& ad = w.addPlayer("player-ad", "Ad");
    ad.hurt = 100;
    ad.downedLeft = 600;
    ad.state = "downed";
    ad.posture = "lying";
    const auto from = ad.position;
    for (int i = 0; i < 20; ++i)
    {
        w.move("player-ad", 1, 0);
        w.tick(.1);
    }
    const double went = std::hypot(ad.position.x - from.x, ad.position.y - from.y);
    expect(went > .3 && went <= 1.05, "Downed, a wolf crawls half a tile a second: " + std::to_string(went));
    expect(ad.posture == "lying" && ad.downedLeft > 0, "lying still");
}

// Pace in a fight (doc 33): the wheel's pace sets how far a turn's move goes; running costs stamina a tile; stamina
// comes back a little each turn, by strength; an exhausted wolf walks.
void paceAndStamina()
{
    World w;
    auto& b = duel(w);
    auto* fa = b.fighter("player-ad");
    auto* ad = w.entity("player-ad");
    const auto farthest = [&] {
        int far = 0;
        for (const auto& [x, y] : w.battleReach("player-ad"))
            far = std::max(far, test::apart(x, y, fa->x, fa->y));
        return far;
    };
    ad->stamina = 100;
    w.setPace("player-ad", 0);
    const int walking = farthest();
    w.setPace("player-ad", 10);
    const int sprinting = farthest();
    expect(walking >= 1 && sprinting >= walking * 2, "A sprint goes far further than a walk: " + std::to_string(walking) + " v " +
           std::to_string(sprinting));
    ad->stamina = 5;
    expect(farthest() < sprinting && farthest() >= walking, "Short of breath, only as far as it pays for (a walk is free)");
    ad->stamina = 100;
    std::pair<int, int> to{fa->x, fa->y};
    for (const auto& t : w.battleReach("player-ad"))
        if (test::apart(t.first, t.second, fa->x, fa->y) > test::apart(to.first, to.second, fa->x, fa->y))
            to = t;
    expect(w.battleMove("player-ad", to.first, to.second).ok, "She sprints");
    const double spent = 100 - ad->stamina, tiles = double(fa->walk.size());
    expect(std::abs(spent - tiles * battle::tileStamina(10)) < 1e-9 && spent > 0,
           "and it costs her breath, by the tile: " + std::to_string(spent));
    for (int i = 0; i < 60 && !fa->walk.empty(); ++i)
        w.tick(.1);
    expect(fa->x == to.first && fa->y == to.second, "She gets there");
    // Exhausted, she walks.
    ad->exhausted = true;
    w.battleAct("player-ad", "wait");
    expect(battle::staminaPerTurn(0, 80) > battle::staminaPerTurn(0, 20) && battle::staminaPerTurn(60, 50) < battle::staminaPerTurn(0, 50),
           "Stamina back a turn: more for the strong, less for the hurt");
    expect(battle::moveRange(50, 0, 0) < battle::moveRange(50, 0, 5) && battle::moveRange(50, 0, 5) < battle::moveRange(50, 0, 10),
           "Range by pace");
}

// Resting a turn (doc 33): no move, twice the stamina back at the next; and the combat injuries (doc 38): a hard bite
// bleeds, a very hard blow staggers.
void restAndCombatInjuries()
{
    World w;
    auto& b = duel(w);
    auto* fa = b.fighter("player-ad");
    auto* fb = b.fighter("player-bo");
    auto* ad = w.entity("player-ad");
    ad->stamina = 30;
    expect(w.battleAct("player-ad", "rest").ok && fa->resting && fa->moved && fa->acted, "Ad rests: no move and no action this turn");
    expect(!w.battleMove("player-ad", fa->x + 1, fa->y).ok, "she can't move after resting");
    expect(!w.battleAct("player-ad", "bite", "player-bo").ok, "nor bite");
    expect(test::acting(&b, "player-ad") && w.battleFace("player-ad", (fa->facing + 1) % 8).ok,
           "but her turn goes on: she can turn, and write, till its time or End turn");
    w.battleAct("player-ad", "wait");
    for (int i = 0; i < 600 && !fa->acting; ++i)
    {
        if (fb->acting)
            w.battleAct("player-bo", "wait");
        w.tick(.1);
    }
    expect(fa->acting && !fa->resting, "Her next turn comes");
    expect(std::abs(ad->stamina - (30 + 2 * battle::staminaPerTurn(ad->hurt, ad->strength))) < 1e-6,
           "with twice the stamina back: " + std::to_string(ad->stamina));
    expect(!w.battleMove("player-ad", fa->x + 99, fa->y).ok && !fa->moved, "a new turn: her move is hers again");
    w.battleAct("player-ad", "wait");
    for (int i = 0; i < 600 && !fa->acting; ++i)
    {
        if (fb->acting)
            w.battleAct("player-bo", "wait");
        w.tick(.1);
    }
    const double before = ad->stamina;
    w.battleAct("player-ad", "wait");
    for (int i = 0; i < 600 && !fa->acting; ++i)
    {
        if (fb->acting)
            w.battleAct("player-bo", "wait");
        w.tick(.1);
    }
    expect(std::abs(ad->stamina - std::min(100.0, before + battle::staminaPerTurn(ad->hurt, ad->strength))) < 1e-6,
           "Not resting again: the usual back (rest is chosen each turn)");
    // A hard bite bleeds; a very hard blow staggers.
    auto* bo = w.entity("player-bo");
    const double hurt0 = bo->hurt;
    fb->meter = 50;
    w.hurtFighter(b, *fb, 26, battle::DownedBite, "player-ad", true);
    expect(fb->bleeding == battle::BleedTurns, "A bite of 26: Bo bleeds");
    expect(fb->staggered == 2 && fb->meter == 50 - battle::StaggerSetback, "and staggers: his bar set back");
    w.battleAct("player-ad", "wait");
    for (int i = 0; i < 600 && !fb->acting; ++i)
    {
        if (fa->acting)
            w.battleAct("player-ad", "wait");
        w.tick(.1);
    }
    expect(std::abs(bo->hurt - (hurt0 + 26 + battle::BleedDamage)) < 1e-6 && fb->bleeding == battle::BleedTurns - 1 && fb->staggered == 0,
           "At his turn he bleeds, and is no longer staggered: " + std::to_string(bo->hurt));
    w.hurtFighter(b, *fb, 10, battle::DownedBite, "player-ad", true);
    expect(fb->bleeding == battle::BleedTurns - 1, "A light bite doesn't start it again");
}

// A turn's three parts (doc 33): move, action and facing. With all three used the turn ends by itself, a moment
// after the last; turning again puts it off. Faced first, the walk keeps that facing.
void threePartTurns()
{
    World w;
    auto& b = duel(w);
    auto* fa = b.fighter("player-ad");
    w.entity("player-ad")->pace = 5;
    const int chosen = (fa->facing + 3) % 8;
    expect(w.battleFace("player-ad", chosen).ok && fa->faced, "Ad faces first: one part");
    std::pair<int, int> to{fa->x, fa->y};
    for (const auto& t : w.battleReach("player-ad"))
        if (test::apart(t.first, t.second, fa->x, fa->y) > test::apart(to.first, to.second, fa->x, fa->y))
            to = t;
    expect(w.battleMove("player-ad", to.first, to.second).ok, "then moves: two");
    for (int i = 0; i < 60 && !fa->walk.empty(); ++i)
        w.tick(.1);
    expect(fa->facing == chosen, "She walked there still facing the way she chose");
    expect(test::acting(&b, "player-ad"), "Two parts: her turn goes on");
    expect(w.battleAct("player-ad", "rest").ok == false, "(no rest once she has moved)");
    // Her action: offering a truce.
    expect(w.battleAct("player-ad", "truce").ok && fa->acted, "an action: three");
    w.tick(.5);
    expect(test::acting(&b, "player-ad"), "a moment to settle her facing");
    expect(w.battleFace("player-ad", (chosen + 1) % 8).ok, "she turns again");
    w.tick(1.2);
    expect(test::acting(&b, "player-ad"), "which puts the end off");
    for (int i = 0; i < 10 && fa->acting; ++i)
        w.tick(.1);
    expect(!fa->acting, "then all three used, her turn ends by itself");
}

void barsFillInRealTime()
{
    World w;
    auto& b = duel(w);
    auto* fa = b.fighter("player-ad");
    auto* fb = b.fighter("player-bo");
    // Ad is acting; Bo let his turn go, and his bar is filling from its head start.
    expect(fa->acting && !fb->acting && fb->meter == 40, "Ad acts; Bo's bar fills again from 40");
    const auto reach = w.battleReach("player-ad");
    const auto [tx, ty] = reach.back();
    w.battleMove("player-ad", tx, ty);
    const double bo0 = fb->meter;
    w.tick(1);
    const double boPerSecond = battle::meterGain(effectiveDexterity(*w.entity("player-bo"))) * battle::MeterPerSecond;
    expect(std::abs(fb->meter - bo0 - boPerSecond) < 1, "Bo's bar fills in real time while Ad acts");
    for (int i = 0; i < 60 && !fa->walk.empty(); ++i)
        w.tick(.1);
    expect(fa->x == tx && fa->y == ty, "She walks where she was going");
    const int walked = fa->facing;
    expect(w.battleFace("player-ad", (walked + 2) % 8).ok && fa->facing == (walked + 2) % 8,
           "She faced the way she walked; turning by hand after it stands (doc 33)");
    expect(w.battleAct("player-ad", "wait").ok && fa->meter == 20 && !fa->acting,
           "Ending early: her bar starts again at once (a head start for not acting)");
    // The moment Bo's bar is full he acts, whoever else is acting.
    for (int i = 0; i < 400 && !fb->acting; ++i)
        w.tick(.1);
    expect(fb->acting, "Bo's bar full: his turn, at once");
    for (int i = 0; i < int(battle::TurnSeconds * 10) + 5; ++i)
        w.tick(.1);
    expect(!fb->acting && fb->timeouts == 1, "A turn lasts its fifteen seconds");
}

void dodgingTheFire()
{
    // The fire's countdown is in seconds, for everyone to see; a wolf whose bar fills in time steps out of it.
    World w;
    auto& b = duel(w);
    auto* ad = w.entity("player-ad");
    w.giveGift("player-ad", "fire", false);
    auto* fa = b.fighter("player-ad");
    auto* fb = b.fighter("player-bo");
    w.entity("player-bo")->dexterity = 100;             // A quick wolf.
    fb->x = fa->x + 2;
    fb->y = fa->y;
    fb->meter = 95;                                       // About to act.
    const auto r = w.battleAct("player-ad", "flame", std::to_string(fb->x) + "," + std::to_string(fb->y));
    expect(r.ok && b.casts.size() == 1, "Ad gathers fire at Bo: " + r.message);
    const double wait = b.casts[0].firesAt - b.casts[0].castAt;
    expect(std::abs(wait - battle::GiftedFlame.charge / (1 + ad->wisdom / 200)) < 1e-9 && wait > 3 && wait < 4,
           "It goes off in about three and a half seconds: " + std::to_string(wait));
    for (int i = 0; i < 20 && !fb->acting; ++i)
        w.tick(.1);
    expect(fb->acting, "Bo's bar fills first");
    const auto& cone = b.casts[0].tiles;
    std::pair<int, int> out{-1, -1};
    for (const auto& [x, y] : w.battleReach("player-bo"))
        if (std::find(cone.begin(), cone.end(), std::pair<int, int>{x, y}) == cone.end())
            out = {x, y};
    expect(out.first >= 0 && w.battleMove("player-bo", out.first, out.second).ok, "He steps out of the cone");
    const double hurt = w.entity("player-bo")->hurt;
    for (int i = 0; i < 50 && !b.casts.empty(); ++i)
        w.tick(.1);
    expect(b.casts.empty() && w.entity("player-bo")->hurt == hurt && fb->burning == 0, "The fire goes off where he was: he is untouched");
}

void smiths()
{
    // Greyfen's smith, Brann, and the demo's, Ash: they sell dull bronze swords (and Brann a little cheap ironwork,
    // Docs/Design/39), never meals.
    World town;
    expect(town.loadWorldFile(RATW_SOURCE_DIR "/Data/Worlds/Greyfen/world.ratw").ok, "Greyfen loads");
    auto& soc = town.society();
    expect(soc.merchant("brann") && soc.smith("brann") && !soc.smith("wren"), "Brann is a smith; Wren is not");
    const auto iron = soc.wares("brann");
    expect(iron.front() == "sword" && std::find(iron.begin(), iron.end(), "meal") == iron.end(), "a smith deals in swords");
    expect(Society::stock(*soc.account("brann"), "sword") == Society::SmithSwords, "and has a few on hand");
    expect(std::string(Society::itemName("sword")) == "Dull bronze sword", "a dull bronze sword");
    auto& ada = town.addPlayer("player-ada", "Ada");
    const auto* brann = town.entity("brann");
    ada.cellId = brann->cellId;
    ada.position = {brann->position.x + 1, brann->position.y};
    soc.create("player-ada", "herbs", 1, "test");
    town.society().shift("treasury", "player-ada", "", 0, 100, "test: a purse for a sword");
    expect(!soc.quote("player-ada", "brann", "herbs", 1, false).ok, "Brann won't buy herbs");
    const auto bought = soc.trade("player-ada", "brann", "sword", 1, true);
    expect(bought.ok && Society::stock(*soc.account("player-ada"), "sword") == 1, "Ada buys a sword: " + bought.message);
    // Herbs, a meal and a sword: three kinds of goods, and the save still holds.
    soc.create("player-ada", "meal", 1, "test");
    const auto saved = town.save();
    {
        // And through the checkpoint document (where a purse once held at most two kinds of goods).
        checkpoint::ServerState server;
        PersistedWorld back;
        std::string problem;
        const auto doc = checkpoint::encode(saved, server, {}, 0);
        expect(checkpoint::decode(doc, back, server, problem), "The checkpoint reads back: " + problem);
    }
    World again;
    expect(again.loadWorldFile(RATW_SOURCE_DIR "/Data/Worlds/Greyfen/world.ratw").ok, "Greyfen loads again");
    expect(again.restore(saved).ok, "A purse with herbs, a meal and a sword is saved and restored");
    expect(Society::stock(*again.society().account("player-ada"), "sword") == 1, "the sword with it");
    World demo;
    expect(demo.society().smith("npc_smith") && Society::stock(*demo.society().account("npc_smith"), "sword") > 0,
           "In the demo world, Ash keeps a forge too");
}

// From the playtest of October 4 (doc 33, "Playtest fixes").
void playtestFixes()
{
    World w;
    quiet(w);
    auto& ada = wolf(w, "ada");
    std::string resident;
    for (const auto& [id, e] : w.entities())
        if (e.npc && !e.transient && e.cellId == ada.cellId && e.age >= battle::YoungestFighter)
        {
            resident = id;
            break;
        }
    expect(!resident.empty(), "a resident to try it on");
    auto* npc = w.entity(resident);
    npc->leaderId.clear();
    ada.position = {npc->position.x - 1, npc->position.y};
    auto r = w.attack("ada", resident);
    expect(r.ok, "Ada goes for them: " + r.message);
    auto& b = fight(w, "ada");
    // The one who starts it strikes first: those set on wait out her first turn, however long she takes.
    for (int i = 0; i < 60; ++i)
        w.tick(.1);
    expect(test::acting(&b, "ada") && ada.hurt == 0, "Six seconds in, nobody has struck at her before her first turn");
    expect(b.opening == "ada", "She has the opening");
    expect(w.battleAct("ada", "wait").ok && b.opening.empty(), "Her first turn done, the opening is gone");
    // The flee message says which way out.
    for (int i = 0; i < 400 && !test::acting(&b, "ada"); ++i)
        w.tick(.1);
    if (test::acting(&b, "ada") && !b.onEdge(b.fighter("ada")->x, b.fighter("ada")->y))
    {
        r = w.battleAct("ada", "flee");
        expect(!r.ok && r.message.find("tiles to the") != std::string::npos, "Not at the edge: told how far, and which way: " + r.message);
    }
    // Gone and back: her turns are hers again, not skipped as away.
    w.linger("ada");
    expect(b.fighter("ada")->away, "Gone from the world, she is away");
    w.stopLingering("ada");
    expect(!b.fighter("ada")->away && !ada.lingering, "Back again, she is not");
    // An away player says they're back.
    b.fighter("ada")->away = true;
    r = w.battleAct("ada", "back");
    expect(r.ok && !b.fighter("ada")->away, "\"I'm back\" brings an away player back: " + r.message);
    // Injury slows a wolf in the world too: the speed above a walk, and badly hurt, the walk itself.
    Entity well;
    well.pace = 10;
    well.stamina = 100;
    Entity hurt = well;
    hurt.hurt = 50;
    Entity limping = well;
    limping.hurt = 80;
    expect(paceSpeed(hurt) < paceSpeed(well) - 1, "Hurt, a wolf runs slower");
    well.pace = hurt.pace = limping.pace = 0;
    expect(paceSpeed(hurt) == paceSpeed(well) && paceSpeed(limping) < paceSpeed(well), "Limping, it walks slower too");
}

// A duel on terms (doc 37): first blood, until one yields, or to the death; yielding at any time; a fight left.
Battle& duelOn(World& w, const std::string& terms)
{
    quiet(w);
    auto& ad = w.addPlayer("player-ad", "Ad");
    auto& bo = w.addPlayer("player-bo", "Bo");
    bo.cellId = ad.cellId;
    bo.position = {ad.position.x + 1.2, ad.position.y};
    expect(w.attack("player-ad", "player-bo", terms).ok && w.challengeTo("player-bo")->terms == (terms.empty() ? "yield" : terms),
           "a challenge on its terms");
    w.answerChallenge("player-bo", true);
    auto* b = const_cast<Battle*>(w.battleOf("player-ad"));
    expect(b && b->terms == (terms.empty() ? "yield" : terms), "a duel on them");
    w.tick(.05);
    return *b;
}

void dueTerms()
{
    {
        // Until one yields (the default): a blow that would down Bo has him yield instead, on his feet, not bleeding.
        World w;
        auto& b = duelOn(w, "");
        auto* bo = w.entity("player-bo");
        bo->hurt = 99.5;
        w.hurtFighter(b, *b.fighter("player-bo"), 12, battle::DownedBite, "player-ad", true);
        expect(b.fighter("player-bo")->status == "yielded" && bo->downedLeft <= 0 && bo->hurt < 100, "Bo yields instead of going down");
        expect(b.over && b.banner.find("Bo yields") == 0, "and the fight is over: " + b.banner);
    }
    {
        // First blood: the first wound ends it.
        World w;
        auto& b = duelOn(w, "blood");
        w.hurtFighter(b, *b.fighter("player-bo"), 6, battle::DownedBite, "player-ad", true);
        expect(b.fighter("player-bo")->status == "yielded" && b.over && b.banner.find("First blood") == 0, "First blood: " + b.banner);
    }
    {
        // To the death: doc 33's rules.
        World w;
        auto& b = duelOn(w, "death");
        w.entity("player-bo")->hurt = 95;
        w.hurtFighter(b, *b.fighter("player-bo"), 12, battle::DownedBite, "player-ad", true);
        expect(b.fighter("player-bo")->status == "downed" && w.entity("player-bo")->downedLeft > 0, "To the death, Bo goes down and bleeds");
    }
    {
        // Yielding at any time: offered, then accepted or refused by a player on the other side; silence is a no.
        World w;
        auto& b = duelOn(w, "death");
        auto r = w.battleAct("player-bo", "yield");
        expect(r.ok && b.yieldBy == "player-bo", "Bo offers to yield, his turn or not: " + r.message);
        expect(!w.answerYield("player-bo", true).ok, "it isn't his to answer");
        expect(w.answerYield("player-ad", false).ok && b.yieldBy.empty() && !b.over, "Ad presses on");
        w.battleAct("player-bo", "yield");
        for (int i = 0; i < int(battle::YieldSeconds * 10) + 5; ++i)
            w.tick(.1);
        expect(b.yieldBy.empty() && !b.over, "unanswered, the offer lapses");
        w.battleAct("player-bo", "yield");
        expect(w.answerYield("player-ad", true).ok && b.over && b.fighter("player-bo")->status == "yielded", "Ad spares him: " + b.banner);
    }
    {
        // Every player in it away, and no NPC to fight on: the fight lapses after a while.
        World w;
        auto& b = duelOn(w, "");
        b.fighter("player-ad")->away = b.fighter("player-bo")->away = true;
        for (int i = 0; i < int(battle::LapseSeconds * 10) + 20 && !b.over; ++i)
            w.tick(.1);
        expect(b.over && b.truced, "it lapses: " + b.banner);
    }
}

// Pace (doc 37, phase 5): no dead air, and planning one's turn while one's bar fills.
void planningAhead()
{
    {
        // No dead air: with no player deciding and no fire gathering, every bar fills faster.
        World w;
        auto& b = duel(w);
        auto* fb = b.fighter("player-bo");
        expect(w.meterHaste(b) == 1, "Ad deciding: the bars fill as ever");
        w.battleAct("player-ad", "wait");
        expect(!fb->acting && w.meterHaste(b) == battle::Haste, "no one deciding: faster");
        const double bo0 = fb->meter;
        w.tick(1);
        const double perSecond = battle::meterGain(effectiveDexterity(*w.entity("player-bo"))) * battle::MeterPerSecond;
        expect(std::abs(fb->meter - bo0 - perSecond * battle::Haste) < 1, "Bo's bar fills " + std::to_string(fb->meter - bo0) + " in a second");
        b.casts.push_back({});
        expect(w.meterHaste(b) == 1, "while fire gathers, as ever (time to step out of it)");
    }
    {
        // A move and a bite planned while Bo's bar fills, played as his turn comes; the rest of the turn is his.
        World w;
        auto& b = duel(w);
        auto* fa = b.fighter("player-ad");
        auto* fb = b.fighter("player-bo");
        expect(w.battleReach("player-bo").empty() && !w.planReach("player-bo").empty(), "Bo can't move now, but sees where his next turn reaches");
        std::pair<int, int> to{-1, -1};
        for (const auto& [x, y] : w.planReach("player-bo"))
            if (test::apart(x, y, fa->x, fa->y) == 1 && (x != fb->x || y != fb->y))
                to = {x, y};
        expect(to.first >= 0, "a tile beside Ad");
        expect(!w.planMove("player-bo", fa->x + 40, fa->y).ok, "not out of his reach");
        expect(w.planMove("player-bo", to.first, to.second).ok && w.planAct("player-bo", "bite", "player-ad").ok, "Bo plans: there, then bite Ad");
        expect(!w.planAct("player-bo", "bite", "player-bo").ok && !w.planAct("player-bo", "dance", "").ok, "(only what can be planned)");
        expect(!w.planAct("player-ad", "bite", "player-bo").ok, "Ad, taking her turn, does it now instead");
        expect(!w.battleAct("player-bo", "bite", "player-ad").ok, "and it is still not his turn");
        w.battleAct("player-ad", "wait");
        for (int i = 0; i < 400 && !fb->acting; ++i)
        {
            w.tick(.1);
            if (fa->acting)
                w.battleAct("player-ad", "wait");
        }
        expect(fb->acting && !fb->moved, "his turn comes");
        expect(!w.planMove("player-bo", to.first, to.second).ok, "(no planning in one's own turn)");
        for (int i = 0; i < 60 && (!fb->plan.empty() || !fb->walk.empty()); ++i)
            w.tick(.1);
        expect(fb->x == to.first && fb->y == to.second && fb->moved && fb->acted, "he went there and bit");
        expect(std::any_of(b.log.begin(), b.log.end(), [](const BattleLine& l) { return l.actor == "player-bo" && (l.kind == "hit" || l.kind == "graze" || l.kind == "miss"); }),
               "the bite is in the log");
        expect(fb->acting, "his turn is still his (to turn, to write)");
        for (int i = 0; i < int(battle::TurnSeconds * 10) + 5 && fb->acting; ++i)
            w.tick(.1);
        expect(!fb->acting && fb->timeouts == 0, "a turn used, left to run out, is not a turn let pass");
    }
    {
        // A bite planned at a foe out of reach steps in first, as a click on them does.
        World w;
        auto& b = duel(w);
        auto* fa = b.fighter("player-ad");
        auto* fb = b.fighter("player-bo");
        w.battleAct("player-ad", "wait");
        std::pair<int, int> far{-1, -1};
        for (const auto& [x, y] : w.planReach("player-bo"))
            if (test::apart(x, y, fb->x, fb->y) >= 2 && test::apart(x, y, fa->x, fa->y) >= 2)
                far = {x, y};
        expect(far.first >= 0, "room to stand apart");
        fa->x = far.first;                          // (Ad stands off, where Bo's move reaches.)
        fa->y = far.second;
        expect(w.planAct("player-bo", "bite", "player-ad").ok, "Bo plans a bite at Ad, out of reach");
        expect(w.unplan("player-bo", "act").ok && fb->plan.empty(), "takes it back");
        w.planAct("player-bo", "bite", "player-ad");
        for (int i = 0; i < 400 && (!fb->acting || !fb->plan.empty() || !fb->walk.empty()); ++i)
        {
            w.tick(.1);
            if (fa->acting)
                w.battleAct("player-ad", "wait");
        }
        expect(test::apart(fb->x, fb->y, fa->x, fa->y) == 1 && fb->acted, "he steps in and bites");
    }
}

// Choices (doc 37, phase 6): Guard, Shove.
void guardAndShove()
{
    {
        // On guard: harder to hit, and turning to meet the blow (no back or side to it).
        World w;
        auto& b = duel(w);
        auto* fa = b.fighter("player-ad");
        auto* fb = b.fighter("player-bo");
        fb->facing = battle::octant(fb->x - fa->x, fb->y - fa->y);      // Bo's back to Ad.
        const double behind = w.strikeChance(*fa, *fb);
        fb->guarding = true;
        const double guarded = w.strikeChance(*fa, *fb);
        auto headOn = *fb;
        headOn.guarding = false;
        headOn.facing = battle::octant(fa->x - fb->x, fa->y - fb->y);
        const double front = w.strikeChance(*fa, headOn);
        expect(guarded < behind && std::abs(guarded - std::clamp(front - battle::GuardDodge, .2, .95)) < .051,
               "on guard, no blow from behind, and 20 less than head on: " + std::to_string(behind) + " → " + std::to_string(guarded));
        w.battleAct("player-ad", "bite", "player-bo");
        expect(fb->facing == battle::octant(fa->x - fb->x, fa->y - fb->y), "and Bo turned to meet it");
        fb->guarding = false;
        // Guard is an action; it lasts until one's next turn.
        w.battleAct("player-ad", "wait");
        untilTurnOf(w, "player-bo", {"player-ad"});
        expect(w.battleAct("player-bo", "guard").ok && fb->guarding && fb->acted, "Bo stands on guard: his action");
        expect(!w.battleAct("player-bo", "bite", "player-ad").ok, "(no blow as well)");
        w.battleAct("player-bo", "wait");
        expect(fb->guarding, "still on guard after his turn");
        untilTurnOf(w, "player-bo", {"player-ad"});
        expect(!fb->guarding, "until his next turn");
    }
    {
        // A shove: an adjacent wolf a tile straight back, strength against strength.
        World w;
        auto& b = duel(w);
        auto* fa = b.fighter("player-ad");
        auto* fb = b.fighter("player-bo");
        w.entity("player-ad")->strength = 100;                 // (Sure to push.)
        w.entity("player-bo")->strength = 10;
        const int bx = fb->x, by = fb->y, dx = fb->x - fa->x, dy = fb->y - fa->y;
        expect(test::apart(fa->x, fa->y, fb->x, fb->y) == 1, "side by side");
        const double stamina = w.entity("player-ad")->stamina;
        const auto r = w.battleAct("player-ad", "shove", "player-bo");
        expect(r.ok && fa->acted && w.entity("player-ad")->stamina == stamina - battle::ShoveStamina, "Ad shoves: her action, and breath: " + r.message);
        const bool moved = fb->x == bx + dx && fb->y == by + dy;
        expect(moved || b.log.back().text.find("nowhere") != std::string::npos, "Bo goes back a tile (or had nowhere to go): " + b.log.back().text);
        expect(!w.battleAct("player-ad", "shove", "player-bo").ok, "(once: it was her action)");
        w.battleAct("player-ad", "wait");
        // Not from afar.
        untilTurnOf(w, "player-ad", {"player-bo"});
        fb->x = fa->x + 3;
        fb->y = fa->y;
        expect(!w.battleAct("player-ad", "shove", "player-bo").ok, "a shove needs them next to you");
    }
}

// Armour by hit zone (doc 35, Part 8): a blow lands where the side it comes at allows, and only the armour there
// counts; never all of it is stopped; heavy armour slows the bar.
void armourInFights()
{
    World w;
    auto& b = duel(w);
    auto* bo = w.entity("player-bo");
    expect(battle::throughArmour(*bo, "throat", 12, "thrust") == 12, "No armour: the whole blow");
    bo->worn["neck"] = "steel_gorget";                          // Protect 5 on the throat.
    expect(battle::throughArmour(*bo, "throat", 12, "thrust") == 7, "a throat guard: a bite on the throat does 7");
    expect(battle::throughArmour(*bo, "body", 12, "thrust") == 12 && battle::throughArmour(*bo, "head", 12, "thrust") == 12,
           "but on the shoulder or the face, all 12");
    expect(std::abs(battle::expectedThrough(*bo, 0, 12, "thrust") - 10.5) < 1e-9, "head on, a bite does 10.5 on average (the throat 30% of the time)");
    expect(battle::expectedThrough(*bo, 2, 12, "thrust") > battle::expectedThrough(*bo, 0, 12, "thrust"), "from behind, the throat is rarely reached");
    bo->worn["body"] = "brigandine_coat";                      // Protect 6, +1 against thrusts, DEX −5.
    expect(battle::armourAt(*bo, "body", "thrust") == 7 && battle::armourAt(*bo, "body", "cut") == 6, "a brigandine: 7 against a bite, 6 a cut");
    expect(battle::throughArmour(*bo, "body", 20, "cut") == 14 && battle::throughArmour(*bo, "body", 12, "thrust", 5) == 10, "a cut does 14; pierce goes through");
    expect(battle::throughArmour(*bo, "body", 8, "thrust") == 2, "a quarter always gets through");
    expect(battle::armourPieceAt(*bo, "body") == "Brigandine coat" && battle::armourPieceAt(*bo, "legs").empty(), "the piece on each zone");
    for (int q = 0; q < 3; ++q)
    {
        bool head = false;
        for (const auto& z : battle::hitZones(q))
            head = head || std::string(z.zone) == "head";
        expect(head == (q < 2), "the head is struck from in front or the side, never from behind");
    }
    // In the fight: bites head on land on the throat (the gorget taking some) or elsewhere (all of it), and say where.
    bo->worn.erase("body");
    auto* fb = b.fighter("player-bo");
    auto* fa = b.fighter("player-ad");
    bool throat = false, elsewhere = false;
    for (int i = 0; i < 60 && !(throat && elsewhere); ++i)
    {
        fb->facing = battle::octant(fa->x - fb->x, fa->y - fb->y);
        bo->hurt = 0;
        w.entity("player-ad")->stamina = 100;
        fa->acted = false;
        w.battleAct("player-ad", "bite", "player-bo");
        const auto& line = b.log.back().text;
        throat = throat || line.find(" on the throat, the steel gorget taking ") != std::string::npos;
        elsewhere = elsewhere || ((line.find(" on the face (") != std::string::npos || line.find(" on the shoulder (") != std::string::npos ||
                                   line.find(" on a foreleg (") != std::string::npos));
    }
    expect(throat && elsewhere, "bites land on the gorget, and where there is no armour");
    bo->worn["body"] = "brigandine_coat";
    bo->worn["paws"] = "splinted_greaves";
    expect(battle::armourDex(*bo) == -8, "the armour weighs on the bar: DEX −8 (with the gorget's −1)");
    const double gain = fb->meter;
    w.tick(1);
    const double armoured = battle::meterGain(effectiveDexterity(*bo) - 8) * battle::MeterPerSecond;
    expect(std::abs(fb->meter - gain - armoured) < .5, "his bar fills as DEX 8 lower");
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

// The Dev Console's team fight (/fight-test-team-1): the player and two allies, made up for it, against three weak
// bandits; the allies fight beside the player, and allies and bandits alike are gone once it is over.
void devConsoleTeamFight()
{
    World w;
    quiet(w);
    auto& ada = wolf(w, "ada");
    const auto started = w.testFightTeam("ada");
    expect(started.ok, "A team fight starts where Ada stands: " + started.message);
    auto* b = &fight(w, "ada");
    const int side = b->fighter("ada")->side;
    std::vector<std::string> allies, bandits;
    for (const auto& f : b->fighters)
        if (f.id != "ada")
            (f.side == side ? allies : bandits).push_back(f.id);
    expect(allies.size() == 2 && bandits.size() == 3, "Ada and two allies against three bandits: " + std::to_string(allies.size()) + " v " +
                                                      std::to_string(bandits.size()));
    for (const auto& id : bandits)
    {
        expect(w.hostile(id) && World::testCamp(b->camp), "each bandit, of the test camp: " + id);
        expect(w.temperamentOf(*w.entity(id)).skill <= 30, "and weak");
    }
    for (const auto& id : allies)
    {
        const auto* e = w.entity(id);
        expect(e && e->npc && !w.hostile(id) && !e->name.empty(), "each ally a named passer-by, not hostile: " + (e ? e->name : id));
        expect(w.temperamentOf(*e).kind == "aggressive", "who stands and fights");
        const auto* f = b->fighter(id);
        expect(test::apart(f->x, f->y, b->fighter("ada")->x, b->fighter("ada")->y) <= 3, "beside Ada");
    }
    expect(!w.testFightTeam("ada").ok, "One test fight at a time");
    // Ada waits: her allies fight the bandits.
    bool alliesStruck = false;
    for (int i = 0; i < 6000 && w.battleOf("ada") && !w.battleOf("ada")->over; ++i)
    {
        b = const_cast<Battle*>(w.battleOf("ada"));
        for (const auto& line : b->log)
            alliesStruck = alliesStruck || (std::find(allies.begin(), allies.end(), line.actor) != allies.end() &&
                                            (line.kind == "hit" || line.kind == "graze" || line.kind == "miss"));
        if (test::acting(b, "ada"))
            w.battleAct("ada", "wait");
        ada.hurt = 0;                               // (Not a test of Ada losing.)
        w.tick(.1);
    }
    expect(alliesStruck, "Ada's allies go for the bandits");
    expect(w.battleOf("ada") && w.battleOf("ada")->over, "and the fight comes to an end");
    for (int i = 0; i < 100; ++i)
        w.tick(.1);
    expect(!w.inBattle("ada"), "Back in the world");
    for (int i = 0; i < 60; ++i)
        w.tick(.5);
    for (const auto& id : allies)
        expect(!w.entity(id), "the ally is gone with the fight: " + id);
    for (const auto& id : bandits)
        expect(!w.entity(id), "the bandit too: " + id);
    for (const auto& c : w.roads().camps)
        expect(!World::testCamp(c.id), "and the camp");
}

// The Dev Console's fights (a player marked Dungeon Master): one weak bandit on the far side of the arena, with a clear
// way to the player; ended as a draw on the player's word; and nothing left of the bandit or its camp afterwards.
void devConsoleFights()
{
    World w;
    quiet(w);
    auto& ada = wolf(w, "ada");
    expect(!w.endFightInDraw("ada").ok, "No fight to end yet");
    const auto started = w.testFight("ada");
    expect(started.ok, "A test fight starts where Ada stands: " + started.message);
    const auto bandit = started.targetId;
    auto* b = &fight(w, "ada");
    expect(b->fighters.size() == 2 && b->fighter(bandit) && b->fighter(bandit)->side != b->fighter("ada")->side,
           "Ada against one bandit, and nobody else");
    expect(World::testCamp(b->camp), "a test camp's bandit");
    const auto* me = b->fighter("ada");
    const auto* them = b->fighter(bandit);
    const int start = test::apart(me->x, me->y, them->x, them->y);
    expect(start > int(battle::StartReach) + 2, "on the far side of the arena: " + std::to_string(start) + " tiles off");
    expect(!w.testFight("ada").ok, "One test fight at a time");
    // A clear way: waiting, Ada is reached (round the tavern's tables, by the way there is).
    int closest = start;
    for (int i = 0; i < 3000 && closest > 1; ++i)
    {
        if ((b = const_cast<Battle*>(w.battleOf("ada"))) == nullptr || b->over)
            break;
        if (test::acting(b, "ada"))
            w.battleAct("ada", "wait");
        w.tick(.1);
        if ((b = const_cast<Battle*>(w.battleOf("ada"))) && b->fighter(bandit))
            closest = std::min(closest, test::apart(b->fighter("ada")->x, b->fighter("ada")->y, b->fighter(bandit)->x, b->fighter(bandit)->y));
    }
    expect(closest <= 1, "The bandit closes the distance to Ada (closest " + std::to_string(closest) + ")");
    // Ended as a draw, on Ada's word.
    ada.hurt = 0;
    expect(w.endFightInDraw("ada").ok, "Ada calls it a draw");
    b = &fight(w, "ada");
    expect(b->over && b->truced && b->banner.find("draw") != std::string::npos, "The fight is over, a draw: " + b->banner);
    expect(!w.endFightInDraw("ada").ok, "and can't be ended twice");
    for (int i = 0; i < 100; ++i)
        w.tick(.1);
    expect(!w.inBattle("ada"), "Back in the world");
    for (int i = 0; i < 60; ++i)
        w.tick(.5);
    expect(!w.entity(bandit), "The test bandit is gone with its fight");
    for (const auto& c : w.roads().camps)
        expect(!World::testCamp(c.id), "and so is its camp");
    expect(ada.downedLeft <= 0 && !ada.dead, "Ada is none the worse");
    expect(w.testFight("ada").ok, "Another can be started");

    // On raised ground (a hillside, a valley floor above the sea): every tile two steps up, so nothing is at height 0.
    World hill;
    quiet(hill);
    auto& bo = wolf(hill, "ada");
    for (auto& t : hill.cell(bo.cellId)->tiles)
        t.height += 2;
    const auto onHill = hill.testFight("ada");
    expect(onHill.ok, "A test fight starts on raised ground too: " + onHill.message);
    const auto* hb = hill.battleOf("ada");
    const auto* hm = hb ? hb->fighter("ada") : nullptr;
    const auto* ht = hb ? hb->fighter(onHill.targetId) : nullptr;
    expect(hm && ht && test::apart(hm->x, hm->y, ht->x, ht->y) > int(battle::StartReach) + 2, "the bandit across the arena there as well");
    int near = 1000;
    for (int i = 0; i < 3000 && near > 1; ++i)
    {
        hb = hill.battleOf("ada");
        if (!hb || hb->over)
            break;
        if (test::acting(hb, "ada"))
            hill.battleAct("ada", "wait");
        hill.tick(.1);
        if ((hb = hill.battleOf("ada")) && hb->fighter(onHill.targetId))
            near = std::min(near, test::apart(hb->fighter("ada")->x, hb->fighter("ada")->y, hb->fighter(onHill.targetId)->x,
                                              hb->fighter(onHill.targetId)->y));
    }
    expect(near <= 1, "and it reaches Ada up there (closest " + std::to_string(near) + ")");
}

int main()
{
    try
    {
        rules();
        challengeAndTurns();
        downedAndBackToTheWorld();
        gettingUpInAFight();
        restAndRepeatedDowns();
        watchingJoiningFleeing();
        aTimidResidentRuns();
        downedIsSaved();
        facingAndTruce();
        theSword();
        theFlame();
        crawling();
        smiths();
        barsFillInRealTime();
        paceAndStamina();
        restAndCombatInjuries();
        threePartTurns();
        dodgingTheFire();
        playtestFixes();
        dueTerms();
        planningAhead();
        guardAndShove();
        armourInFights();
        devConsoleFights();
        devConsoleTeamFight();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "Battle tests passed: " << checks << " checks.\n";
    return 0;
}
