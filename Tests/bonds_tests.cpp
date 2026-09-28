// Relationships (RatwBonds.h): how characters come to regard one another, and that only rules move it.
#include "RatwBonds.h"
#include "RatwWorld.h"

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

void feelingsGrowAndFade()
{
    Bonds b;
    expect(!b.find("ash", "wren") && b.describe("ash", "wren", "Wren").empty(), "Strangers have no bond");
    b.change("ash", "wren", {10, 0, 10, 0, 0}, 1);
    const double first = b.find("ash", "wren")->affinity;
    for (int i = 0; i < 40; ++i)
        b.change("ash", "wren", {10, 0, 10, 0, 0}, 1);
    const auto* bond = b.find("ash", "wren");
    expect(first == 10 && bond->affinity < 100 && bond->affinity > 80, "Liking grows, more slowly near its limit");
    expect(bond->familiarity <= 100 && bond->familiarity > 90, "Familiarity is capped at 100");
    expect(!b.find("wren", "ash"), "A bond is one-sided");
    b.change("ash", "wren", {-300, -300, -300, 300, -300}, 1);
    bond = b.find("ash", "wren");
    expect(bond->affinity == -100 && bond->trust == -100 && bond->familiarity == 0 && bond->fear == 100 &&
               bond->respect == -100,
           "Every feeling stays within its range");
    b.mutual("moss", "rook", {0, 0, 2, 0, 0}, 1);
    expect(b.find("moss", "rook") && b.find("rook", "moss"), "A mutual change goes both ways");
    b.change("ash", "ash", {5, 5, 5, 5, 5}, 1);
    expect(!b.find("ash", "ash"), "Nobody has a bond with themselves");
    // Fading: an acquaintance not seen for days is forgotten; debts are not.
    b.change("ash", "finch", {0, 0, 1, 0, 0}, 1);
    b.addOwed("ash", "birch", 3, 1);
    for (double day = 2; day < 10; ++day)
        b.fade(day);
    expect(!b.find("ash", "finch"), "A passing acquaintance fades away");
    expect(b.find("ash", "birch") && b.find("ash", "birch")->owed == 3, "A debt is remembered");
    expect(b.find("ash", "wren")->fear < 100, "Fear passes");
    b.forget("wren");
    expect(!b.find("ash", "wren") && !b.find("wren", "ash"), "Someone who has left is forgotten by all");
}

void aCrowdKeepsTheStrongest()
{
    Bonds b;
    b.change("guard", "captain", {40, 40, 60, 0, 30}, 1);
    for (int i = 0; i < int(Bonds::MaxPerHolder) + 30; ++i)
        b.change("guard", "passer" + std::to_string(i), {0, 0, 1, 0, 0}, 1);
    expect(b.of("guard")->size() == Bonds::MaxPerHolder, "At most so many acquaintances");
    expect(b.find("guard", "captain"), "The faintest go first, never the strong");
    expect(b.find("guard", "passer" + std::to_string(Bonds::MaxPerHolder + 29)), "and never the newest");
}

void describedInWords()
{
    Bonds b;
    b.change("wren", "player-ash", {30, 10, 70, 0, 0}, 1);
    b.addOwed("wren", "player-ash", 3, 1);
    const auto words = b.describe("wren", "player-ash", "Ash");
    expect(words.find("You know Ash well") == 0, "Named: " + words);
    expect(words.find("like them") != std::string::npos && words.find("trust them a little") != std::string::npos,
           "Liking and trust in words: " + words);
    expect(words.find("Ash owes you 3 pennies.") != std::string::npos, "and the debt: " + words);
    b.change("rook", "player-ash", {-70, 0, 10, 40, 0}, 1);
    const auto wary = b.describe("rook", "player-ash", "Ash");
    expect(wary.find("dislike them very much") != std::string::npos && wary.find("fear them") != std::string::npos,
           "Dislike and fear: " + wary);
}

void savedAndRestored()
{
    Bonds b;
    b.change("ash", "wren", {12, 3, 40, 1, 2}, 5);
    b.addOwed("wren", "ash", -2, 5);
    Bonds again;
    expect(again.restore(b.save()) && again.count() == 2 && again.find("ash", "wren")->familiarity == 40 &&
               again.find("wren", "ash")->owed == -2,
           "Bonds survive a save");
    auto bad = b.save();
    bad[0].bond.affinity = 500;
    expect(!again.restore(bad) && again.count() == 2, "An impossible saved feeling is refused, changing nothing");
    auto twice = b.save();
    twice.push_back(twice[0]);
    expect(!again.restore(twice), "A bond saved twice is refused");
}

// In the world: bonds follow from what happens, not from what anyone says.
void theWorldMovesThem()
{
    World w;
    for (const auto& e : w.entities())
        if (e.second.npc)
            w.entity(e.first)->leaderId = "test-frozen";
    w.addPlayer("player-ash", "Ash");
    auto* ash = w.entity("player-ash");
    ash->cellId = "tavern";
    ash->position = {9.5, 7.5};
    w.entity("npc_keeper")->position = {9.5, 6.5};
    expect(w.trade("player-ash", "npc_keeper", "meal", 1, true).ok, "A purchase");
    const auto* keeper = w.bonds().find("npc_keeper", "player-ash");
    const auto* customer = w.bonds().find("player-ash", "npc_keeper");
    expect(keeper && customer && keeper->familiarity > 0 && keeper->trust > 0, "An honest sale starts a bond both ways");
    w.recordEvent({"conversation", "player-ash", "npc_keeper", {}, 0, 0, {}, 0, 0, {}});
    expect(w.bonds().find("npc_keeper", "player-ash")->familiarity > keeper->familiarity - 1e-9, "Talking adds to it");
    w.recordEvent({"conversation", "player-ash", "treasury", {}, 0, 0, {}, 0, 0, {}});
    expect(!w.bonds().find("player-ash", "treasury"), "Only characters have bonds");
    w.recordEvent({"harm", "player-ash", "npc_scout", {}, 0, 0, {}, 0, 0, {}});
    const auto* hurt = w.bonds().find("npc_scout", "player-ash");
    expect(hurt && hurt->affinity < 0 && hurt->fear > 0 && hurt->trust < 0, "Harm breeds dislike, fear and distrust");
    const auto saved = w.save();
    World restored;
    expect(restored.restore(saved).ok, "The world restores");
    expect(restored.bonds().count() == w.bonds().count() && restored.bonds().find("npc_scout", "player-ash"),
           "with its bonds");
}

// Jump the world's calendar ahead through a save, then run the day's tending (bonds, promises, marriages...).
void daysPass(World& w, double days)
{
    auto saved = w.save();
    saved.calendarDays += days;
    expect(w.restore(saved).ok, "The world moves on");
    w.tick(.6);
}

void promisesKeptAndBroken()
{
    World w;
    for (const auto& e : w.entities())
        if (e.second.npc)
            w.entity(e.first)->leaderId = "test-frozen";
    w.addPlayer("player-ash", "Ash");
    auto* ash = w.entity("player-ash");
    ash->cellId = "tavern";
    ash->position = {9.5, 7.5};
    w.entity("npc_keeper")->position = {9.5, 6.5};
    w.promise("player-ash", "npc_keeper", "come back for a meal", 3);
    expect(w.promisesBetween("npc_keeper", "player-ash", "Ash").find("Ash promised you: come back for a meal (due in 3 days).") == 0,
           "An open promise, in words: " + w.promisesBetween("npc_keeper", "player-ash", "Ash"));
    const double trustBefore = w.bonds().find("npc_keeper", "player-ash") ? w.bonds().find("npc_keeper", "player-ash")->trust : 0;
    expect(w.trade("player-ash", "npc_keeper", "meal", 1, true).ok, "Ash comes back and buys a meal");
    expect(w.promises().back().status == "kept" && w.bonds().find("npc_keeper", "player-ash")->trust > trustBefore + 5,
           "The promise is kept, and trust grows");
    w.promise("player-ash", "npc_keeper", "bring firewood", 2);
    const double trust = w.bonds().find("npc_keeper", "player-ash")->trust;
    daysPass(w, 3);
    expect(w.promises().back().status == "broken", "A promise left past its day is broken");
    expect(w.bonds().find("npc_keeper", "player-ash")->trust < trust - 5, "and trust falls further than it rose");
    bool logged = false;
    for (const auto& e : w.takeEvents())
        logged |= e.kind == "promise broken" && e.actor == "player-ash";
    expect(logged, "Both are in the log");
    expect(w.promisesBetween("npc_keeper", "player-ash", "Ash").empty(), "Nothing open remains");
}

void marriages()
{
    World w;
    for (const auto& e : w.entities())
        if (e.second.npc)
            w.entity(e.first)->leaderId = "test-frozen";
    w.entity("npc_cook")->age = 30;
    w.entity("npc_smith")->age = 32;
    w.bonds().mutual("npc_cook", "npc_smith", {80, 60, 90, 0, 0}, w.calendarDays());
    for (int week = 0; week < 12 && !w.society().spouse("npc_cook"); ++week)
    {
        w.bonds().mutual("npc_cook", "npc_smith", {0, 0, 5, 0, 0}, w.calendarDays());   // They keep seeing each other.
        daysPass(w, 7);
    }
    expect(w.society().spouse("npc_cook") && *w.society().spouse("npc_cook") == "npc_smith", "Two who love each other marry");
    const auto* cook = w.society().resident("npc_cook");
    const auto* smith = w.society().resident("npc_smith");
    expect(!cook->relocationCell.empty() || !smith->relocationCell.empty(), "and one of them moves in with the other");
    expect(w.restore(w.save()).ok, "A save with the move under way restores");
}

void timeTogether()
{
    // Residents at work together for a working day come to know each other.
    World w;
    const std::size_t before = w.bonds().count();
    w.setTimeOfDay(9);
    for (int second = 0; second < 6 * 600; ++second)             // Six game hours.
        w.tick(1);
    expect(w.bonds().count() > before, "A day at work together makes acquaintances (" + std::to_string(w.bonds().count()) + ")");
}
} // namespace

int main()
{
    try
    {
        feelingsGrowAndFade();
        aCrowdKeepsTheStrongest();
        describedInWords();
        savedAndRestored();
        theWorldMovesThem();
        promisesKeptAndBroken();
        marriages();
        timeTogether();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Bond tests passed: " << checks << " checks.\n";
    return 0;
}
