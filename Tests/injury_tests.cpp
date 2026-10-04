// Injuries that outlast a fight (Core/RatwInjury.h; Docs/Design/38-injuries.md, phases 3 to 5): the tables, how an
// injury worsens and heals, what it does, and in the world: going down leaves one, rest heals it, it is saved, and a
// Dungeon Master can add or take one away.
#include "RatwStep.h"
#include "RatwWire.h"
#include "RatwWorld.h"
#include "battle_play.h"

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

void theTables()
{
    // How bad: by the blow's damage, a downing, overkill, and downings since a full rest.
    expect(injury::acute("bite", 18, 0, false, 0, .1, .1).severity == 1, "a hit of 18: minor");
    expect(injury::acute("bite", 30, 0, false, 0, .1, .1).severity == 2, "a hit of 30: moderate");
    expect(injury::acute("bite", 0, 0, true, 1, .1, .1).severity == 2, "a first downing: moderate");
    expect(injury::acute("bite", 0, 0, true, 2, .1, .1).severity == 3, "a downing with two since a full rest: severe");
    expect(injury::acute("bite", 0, 20, true, 1, .1, .1).severity == 3, "overkill 15 or more: severe");
    expect(injury::acute("bite", 40, 0, false, 0, .1, .1).severity == 3, "a hit over 35: severe");
    const auto minor = injury::acute("blunt", 10, 0, false, 0, .5, .0);
    expect(minor.restFull == 24 && minor.restLeft == 24 && injury::part(minor.type) != "", "minor: a day's rest at least");
    expect(injury::part(injury::acute("fire", 10, 0, false, 0, .3, .3).type) == "burns", "fire burns");
    // The same kind again: a severity worse, and healing starts over.
    std::vector<Injury> list;
    auto rib = injury::acute("blunt", 10, 0, false, 0, .3, .5);   // (A blunt kind.)
    rib.id = "a";
    injury::addAcute(list, rib);
    injury::heal(list, 10);
    const auto worse = injury::addAcute(list, rib);
    expect(list.size() == 1 && worse.severity == 2 && worse.restLeft == worse.restFull, "the same again: worse, healing begun again");
    // Healing: it eases as it heals (severe, then moderate, then minor), and then it is gone.
    std::vector<Injury> one{injury::acute("bite", 0, 20, true, 1, .0, .0)};
    one[0].id = "b";
    const double full = one[0].restFull;
    expect(injury::severityNow(one[0]) == 3, "severe at first");
    injury::heal(one, full * .4);
    expect(injury::severityNow(one[0]) == 2, "moderate after some rest");
    injury::heal(one, full * .3);
    expect(injury::severityNow(one[0]) == 1, "then minor");
    expect(injury::describe(one[0]).find("minor, healing (about") != std::string::npos, "told: " + injury::describe(one[0]));
    const auto healed = injury::heal(one, full);
    expect(one.empty() && healed.size() == 1, "and then healed");
    // Strain: fighting on it loses a share of the healing done.
    std::vector<Injury> strained{injury::acute("bite", 30, 0, false, 0, .0, .0)};
    strained[0].id = "c";
    injury::heal(strained, 40);
    const double done = strained[0].restFull - strained[0].restLeft;
    injury::strain(strained, .5);
    expect(std::abs((strained[0].restFull - strained[0].restLeft) - done / 2) < 1e-9, "going down loses half the healing done");
}

void whatTheyDo()
{
    expect(!injury::effects({}).any(), "no injuries, no effects");
    Injury leg;
    leg.type = "bitten_foreleg";
    leg.severity = 3;
    leg.restFull = leg.restLeft = 200;
    const auto e = injury::effects({leg});
    expect(std::abs(e.sprint - .8) < 1e-9 && e.arenaMove == 1 && e.slowWalk, "a severe leg: a sprint 20% slower, a tile off, a slower walk");
    // Acute injuries stack, with floors.
    std::vector<Injury> many(5, leg);
    expect(std::abs(injury::effects(many).sprint - .6) < 1e-9, "never below 60% of the speed above a walk");
    Injury ear = leg;
    ear.type = "torn_ear_acute";
    std::vector<Injury> ears(4, ear);
    expect(std::abs(injury::effects(ears).hearing - .5) < 1e-9, "each sense at least half");
    Injury ribs = leg;
    ribs.type = "cracked_rib";
    ribs.severity = 2;
    expect(injury::effects({ribs}).attackStamina == 2 && std::abs(injury::effects({ribs}).recovery - .8) < 1e-9,
           "moderate ribs: stamina back 20% slower, attacks 2 more breath");
    // Lasting ones: at most a quarter off any one ability.
    Injury t1, t2, nose, eye;
    t1.kind = t2.kind = nose.kind = eye.kind = "lasting";
    t1.type = t2.type = "torn_ear";
    t1.side = "left";
    t2.side = "right";
    expect(std::abs(injury::effects({t1, t2}).hearing - .8) < 1e-9, "two torn ears: hearing 20% less");
    eye.type = "clouded_eye";
    Injury eye2 = eye;
    eye2.side = "right";
    expect(std::abs(injury::effects({eye, eye2}).vision - .75) < 1e-9, "two clouded eyes: at most 25% off sight");
    Injury tail;
    tail.kind = "lasting";
    tail.type = "bent_tail";
    expect(!injury::effects({tail}).any(), "a bent tail is only a mark");
    expect(injury::name(t1) == "Torn left ear", "named with its side: " + injury::name(t1));
    // One a body part: no second bent tail; a torn ear on the other side, yes.
    for (int r = 0; r < 20; ++r)
        expect(injury::lastingFrom("blunt", {tail}, r / 20.0).type != "bent_tail", "no second bent tail");
    const auto other = injury::lastingFrom("bite", {t1, tail}, .0);
    expect(other.type != "torn_ear" || other.side == "right", "the other ear may tear");
    // From a severe acute one: the leg's sets into a limp.
    const auto limp = injury::lastingFrom("bite", {}, .5, &leg);
    expect(limp.type == "permanent_limp", "a severe leg sets into a permanent limp");
    expect(injury::visible({leg, t1}) == "a torn left ear, and limping on a bitten foreleg", "seen: " + injury::visible({leg, t1}));
    expect(injury::weariness(1) == 0 && std::abs(injury::weariness(3) - .1) < 1e-9 && injury::weariness(30) == .25,
           "days without a full rest raise the lasting chance, at most 25 points");
}

void inTheWorld()
{
    World w;
    for (const auto& e : w.entities())
        if (e.second.npc)
            w.entity(e.first)->leaderId = "test-frozen";
    auto& ada = w.addPlayer("ada", "Ada");
    auto& bo = w.addPlayer("bo", "Bo");
    bo.cellId = ada.cellId;
    bo.position = {ada.position.x + 1.2, ada.position.y};
    ada.strength = 100;
    ada.dexterity = 90;
    bo.hurt = 95;
    bo.recoveryUsed = std::floor(w.calendarDays());
    expect(w.attack("ada", "bo", "death").ok && w.answerChallenge("bo", true).ok, "a fight to the end");
    for (int i = 0; i < 3000 && bo.downedLeft <= 0; ++i)
    {
        test::playTurn(w, "ada");
        if (w.battleOf("bo") && test::acting(w.battleOf("bo"), "bo"))
            w.battleAct("bo", "wait");
        w.tick(.1);
    }
    expect(bo.downedLeft > 0, "Bo goes down");
    int acute = 0;
    for (const auto& i : bo.injuries)
        acute += i.kind == "acute";
    expect(acute >= 1, "and comes away with an injury");
    bool told = false;
    for (const auto& n : w.takeNotices())
        told |= n.first == "bo" && n.second.find("You come away with an injury") != std::string::npos;
    expect(told, "and is told so");
    // Saved and read back.
    const auto saved = wire::persistEntity(bo, w.time());
    const auto back = wire::readEntity(saved);
    expect(back.injuries.size() == bo.injuries.size() &&
               back.injuries[0].type == bo.injuries[0].type && back.injuries[0].restLeft == bo.injuries[0].restLeft,
           "injuries are saved with the character");
    // A Dungeon Master's correction: a severe leg, then taken away.
    for (int i = 0; i < 4000 && (bo.downedLeft > 0 || w.inBattle("bo")); ++i)
        w.tick(1);
    bo.injuries.clear();
    bo.hurt = 0;
    const double before = paceSpeed(bo);
    const auto added = w.addInjury("bo", "sprained_foreleg", 3, "left", "a storyline");
    expect(added.ok && bo.injuries.size() == 1 && injury::severityNow(bo.injuries[0]) == 3, "the DM gives a severe sprained foreleg");
    bo.pace = 10;
    expect(paceSpeed(bo) < before || before <= step::WalkSpeed, "it slows him");
    expect(!w.addInjury("bo", "no_such", 1, "", "").ok, "an unknown injury is refused");
    expect(w.addInjury("bo", "bent_tail", 1, "", "").ok && bo.injuries.size() == 2 && bo.injuries[1].kind == "lasting", "and a bent tail");
    // Rest heals the acute one: lying in a bed fastest; time away counts.
    const double left = bo.injuries[0].restLeft;
    w.setPosture("bo", "lying");
    for (int i = 0; i < 40; ++i)
        w.tick(30);
    expect(bo.injuries[0].restLeft < left, "lying still heals it: " + std::to_string(left) + " to " + std::to_string(bo.injuries[0].restLeft));
    expect(w.removeInjury("bo", bo.injuries[0].id).ok && bo.injuries.size() == 1, "the DM takes the leg away");
    expect(!w.removeInjury("bo", "injury-none").ok, "an unknown one can't be taken");
    expect(bo.injuries[0].type == "bent_tail", "the tail stays: lasting");
}
} // namespace

int main()
{
    try
    {
        theTables();
        whatTheyDo();
        inTheWorld();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "Injury tests passed: " << checks << " checks.\n";
    return 0;
}
