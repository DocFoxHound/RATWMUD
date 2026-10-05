// Hunting and foraging (Docs/Design/41-hunting-and-foraging.md): a hunt is a fight against game found by the ground,
// the game lives only in its hunt, the hardest blow decides the yield and fire spoils it, kills press on a place, only
// friends may join, and foraging gives the ground's goods until a patch is picked over.
#include "RatwCrime.h"
#include "RatwItems.h"
#include "RatwWild.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <sstream>
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

// One wild cell, 80 by 60: woodland and grass with scrub, a stream of shallows, and a resident so there is an economy.
World wilds()
{
    std::ostringstream cell;
    cell << "id: wilds\nname: The Wilds\ndescription: Woods and meadow.\nworld: 0 0 0\nsize: 80 60\noutdoors: true\n"
            "weather: clear\nwind: 0 0.5 1\nlighting: 1 1 warm\ngrid:\n";
    for (int y = 0; y < 60; ++y)
    {
        std::string row;
        for (int x = 0; x < 80; ++x)
            row += (x * 7 + y * 13) % 11 == 0 ? 'Y' : (x * 3 + y * 5) % 17 == 0 ? 'B' : x == 70 ? '~' : ',';
        cell << row << '\n';
    }
    std::map<std::string, std::string> files;
    files["cells/wilds.cell"] = cell.str();
    files["world.ratw"] =
        "RATW_WORLD 2\ncell \"wilds\" \"cells/wilds.cell\"\nterritory \"wilds\" \"wilds\" \"-\" 0\nspawn \"wilds\" 40.5 30.5\n"
        "economy 1000 100 50 10 12\n"
        "resident \"sorrel\" \"Sorrel\" \"civilian\" \"tending the herb garden\" \"A gardener.\" \"Hello.\" 38 \"timber\" \"female\" "
        "\"average\" \"piebald\" 4 3 0 8 1 8 17 \"-\" 30 0 1 \"wilds\" 5.5 5.5 \"wilds\" 6.5 5.5 \"wilds\" 7.5 5.5\n"
        "resident \"wren\" \"Wren\" \"merchant\" \"weaver at Greywool\" \"A weaver.\" \"Hello.\" 40 \"timber\" \"female\" "
        "\"average\" \"piebald\" 4 3 0 8 1 8 17 \"-\" 30 0 1 \"wilds\" 10.5 10.5 \"wilds\" 12.5 10.5 \"wilds\" 12.5 10.5\n";
    World w;
    const auto loaded = w.loadWorldFiles(files, "wilds");
    expect(loaded.ok, "the wilds load: " + loaded.message);
    return w;
}

Entity& hunter(World& w, const std::string& id, double x = 40.5, double y = 30.5)
{
    auto& e = w.addPlayer(id, id);
    e.cellId = "wilds";
    e.position = {x, y};
    return e;
}

int held(const World& w, const std::string& id, const std::string& item)
{
    const auto* a = w.society().account(id);
    return a ? Society::stockAll(*a, item) : 0;     // Of any quality (a clean kill's goods are fine).
}

Battle& huntOf(World& w, const std::string& id)
{
    auto* b = const_cast<Battle*>(w.battleOf(id));
    expect(b && b->hunt, id + " is hunting");
    return *b;
}

std::vector<std::string> animalsIn(const World& w, const Battle& b)
{
    std::vector<std::string> out;
    for (const auto& f : b.fighters)
        if (!w.animalOf(f.id).empty())
            out.push_back(f.id);
    return out;
}

// Sets out (trying a few moments if the first finds nothing: the count is by chance).
void setOut(World& w, const std::string& id)
{
    for (int i = 0; i < 40 && !w.inBattle(id); ++i)
    {
        const auto r = w.startHunt(id);
        if (!r.ok)
            w.tick(1);
    }
    expect(w.inBattle(id), id + " finds game");
}

void dataLoads()
{
    std::string error;
    expect(wild::load(&error), "the wild's data loads: " + error);
    expect(wild::speciesById("rabbit") && wild::speciesById("red_deer"), "rabbits and deer");
    expect(wild::groundOf('Y') == "forest" && wild::groundOf(',') == "grass" && wild::groundOf('#').empty(), "ground kinds by tile");
}

void startingAHunt()
{
    auto w = wilds();
    hunter(w, "player-ada");
    const auto here = w.wildAround("player-ada");
    expect(here.hunt && here.forage, "out in the wild one may hunt and forage: " + here.huntWhy);
    setOut(w, "player-ada");
    auto& b = huntOf(w, "player-ada");
    const auto animals = animalsIn(w, b);
    expect(!animals.empty(), "a hunt has game in it");
    for (const auto& id : animals)
    {
        const auto* f = b.fighter(id);
        const auto* me = b.fighter("player-ada");
        expect(f->side == 1 && me->side == 0, "the game is the other side");
        expect(std::max(std::abs(f->x - me->x), std::abs(f->y - me->y)) >= 3, "and starts off at a distance");
        expect(w.animalUnaware(id), "not yet aware of the hunter");
        expect(w.visionClarity("player-ada", id) == 0, "and never seen in the world, only in the hunt");
    }
    expect(!w.startHunt("player-ada").ok, "one hunt at a time");
}

void aCleanKillAndAFireKill()
{
    auto w = wilds();
    hunter(w, "player-ada");
    setOut(w, "player-ada");
    auto& b = huntOf(w, "player-ada");
    const auto animals = animalsIn(w, b);
    const auto& first = animals.front();
    const auto* s = wild::speciesById(w.animalOf(first));
    std::map<std::string, int> before;
    for (const auto& [item, n] : s->yield)
        before[item] = held(w, "player-ada", item);
    // One blow harder than its health: a clean kill, all of it worth taking.
    w.hurtFighter(b, *b.fighter(first), s->health * 1.2, battle::DownedBite, "player-ada", true);
    expect(b.fighter(first)->status == "dead", "one blow brings it down");
    for (const auto& [item, n] : s->yield)
    {
        expect(held(w, "player-ada", item) == before[item] + n, "a clean kill: all its " + item);
        // One blow before it knew: masterwork (doc 35, Part 4; doc 40's ambush will make this the stalker's prize).
        const auto* mine = w.society().account("player-ada");
        expect(Society::stock(*mine, items::withMaker(items::withQuality(item, 3), "player-ada")) >= n || !items::qualityApplies(item),
               "of masterwork quality, under the hunter's own mark: " + item);
    }
    expect(w.huntPressure("wilds") >= .99, "a kill presses on the place");
    if (animals.size() > 1)
    {
        // Burnt to death: no pelt, hide or feathers, and some of the meat charred.
        const auto& second = animals[1];
        const auto* t = wild::speciesById(w.animalOf(second));
        std::map<std::string, int> was;
        for (const auto& [item, n] : t->yield)
            was[item] = held(w, "player-ada", item);
        w.hurtFighter(b, *b.fighter(second), t->health * 1.2, battle::DownedFire, "player-ada", true);
        for (const auto& [item, n] : t->yield)
            if (item == "hide" || item == "feathers" || item.find("_pelt") != std::string::npos)
                expect(held(w, "player-ada", item) == was[item], "fire spoils the " + item);
    }
}

void aRaggedKillYieldsLess()
{
    // Many small blows, the hardest a tenth of its health: over many animals, less than clean kills would give.
    int got = 0, clean = 0;
    for (int attempt = 0; attempt < 40 && clean < 30; ++attempt)
    {
        auto w = wilds();
        hunter(w, "player-ada");
        for (int k = 0; k < attempt * 7; ++k)
            w.tick(1);                              // (Another moment, another hunt.)
        setOut(w, "player-ada");
        auto& b = huntOf(w, "player-ada");
        for (const auto& id : animalsIn(w, b))
        {
            const auto* s = wild::speciesById(w.animalOf(id));
            std::map<std::string, int> before;
            for (const auto& [item, n] : s->yield)
            {
                before[item] = held(w, "player-ada", item);
                clean += n;
            }
            for (int i = 0; i < 40 && b.fighter(id)->status == "fighting"; ++i)
                w.hurtFighter(b, *b.fighter(id), s->health * .1, battle::DownedBite, "player-ada", false);
            for (const auto& [item, n] : s->yield)
                got += held(w, "player-ada", item) - before[item];
        }
    }
    expect(clean > 0 && got < clean * .7, "a ragged kill gives less than a clean one (" + std::to_string(got) + " of " + std::to_string(clean) + ")");
}

void gameGetsAwayAndTheHuntEnds()
{
    auto w = wilds();
    hunter(w, "player-ada");
    setOut(w, "player-ada");
    auto& b = huntOf(w, "player-ada");
    const auto animals = animalsIn(w, b);
    // Kill all but one; that one, put on the edge and startled, runs.
    for (std::size_t i = 1; i < animals.size(); ++i)
        w.hurtFighter(b, *b.fighter(animals[i]), 500, battle::DownedBite, "player-ada", true);
    const auto& runner = animals.front();
    w.hurtFighter(b, *b.fighter(runner), .01, battle::DownedBite, "player-ada", false);   // Hurt, it knows.
    expect(!w.animalUnaware(runner), "a hurt animal is alert");
    for (double t = 0; t < 400 && w.battleOf("player-ada") && !w.battleOf("player-ada")->over; t += .1)
    {
        if (const auto* bb = w.battleOf("player-ada"); bb && bb->fighter("player-ada") && bb->fighter("player-ada")->acting)
            w.battleAct("player-ada", "wait");
        w.tick(.1);
    }
    const auto* end = w.battleOf("player-ada");
    expect(!end || end->over, "with no game left, the hunt is over");
    for (int i = 0; i < 100; ++i)
        w.tick(.1);
    for (const auto& id : animals)
        expect(!w.entity(id), "the game goes with the hunt: " + id);
}

void onlyFriendsJoin()
{
    auto w = wilds();
    hunter(w, "player-ada");
    hunter(w, "player-bo", 41.5, 30.5);
    setOut(w, "player-ada");
    const auto id = huntOf(w, "player-ada").id;
    expect(!w.joinBattle("player-bo", id, 0).ok, "a stranger may not join someone's hunt");
    w.setFriends([](const std::string& a, const std::string& b) { return (a == "player-ada" && b == "player-bo") || (a == "player-bo" && b == "player-ada"); });
    expect(!w.joinBattle("player-bo", id, 1).ok, "nor join the game's side");
    const auto r = w.joinBattle("player-bo", id, 0);
    expect(r.ok, "a friend may: " + r.message);
    expect(w.leaveHunt("player-ada").ok, "one may give up a hunt");
    expect(!w.battleOf("player-ada") || w.battleOf("player-ada")->fighter("player-ada")->status == "fled", "and is out of it");
}

void foraging()
{
    auto w = wilds();
    hunter(w, "player-ada");
    int gathered = 0;
    std::string last;
    for (int i = 0; i < 40; ++i)
    {
        const auto r = w.forage("player-ada");
        gathered += r.ok;
        last = r.message;
        w.tick(5);
    }
    expect(gathered > 0, "foraging gives the ground's goods");
    expect(gathered <= wild::forage().picks * 9, "but a patch is soon picked over: " + last);
    expect(!w.forage("player-ada").ok || true, "(cooldown checked below)");
    auto w2 = wilds();
    hunter(w2, "player-ada");
    expect(w2.forage("player-ada").ok, "the first picking");
    expect(!w2.forage("player-ada").ok, "and none again at once");
}
void tracks()
{
    auto w = wilds();
    hunter(w, "player-ada");
    hunter(w, "player-bo");
    const auto r = w.smellTracks("player-ada");
    expect(r.ok, "out in the wild the nose finds trails: " + r.message);
    const auto mine = w.tracksOf("player-ada");
    expect(!mine.empty() && r.message.find("tracks") != std::string::npos, "tracks found, and said: " + r.message);
    for (const auto& t : mine)
    {
        expect(wild::speciesById(t.species) && t.tiles.size() >= 2, "each a trail of an animal's");
        for (const auto& [x, y] : t.tiles)
        {
            const auto* tile = w.cell("wilds")->tile(x, y);
            expect(tile && !tile->solid && !wild::groundOf(tile->glyph).empty(), "over natural ground");
            expect(std::hypot(x + .5 - 40.5, y + .5 - 30.5) <= 31, "within the nose's reach");
        }
    }
    w.smellTracks("player-bo");
    const auto theirs = w.tracksOf("player-bo");
    expect(theirs.size() == mine.size() && theirs.front().tiles == mine.front().tiles, "the ground's trails, the same for any nose");
    // A fresh trail near by: a hunt begun there finds what made it first.
    for (int hour = 0; hour < 48; ++hour)
    {
        const World::Track* fresh = nullptr;
        for (const auto& t : w.tracksOf("player-ada"))
            for (const auto& [x, y] : t.tiles)
                if (t.fresh && std::max(std::abs(x - 40), std::abs(y - 30)) <= 12)
                    fresh = &t;
        if (fresh)
        {
            const auto species = fresh->species;
            const auto started = w.startHunt("player-ada");
            expect(started.ok && started.message.find("trail") != std::string::npos, "following the trail: " + started.message);
            const auto& b = huntOf(w, "player-ada");
            const auto animals = animalsIn(w, b);
            expect(!animals.empty() && w.animalOf(animals.front()) == species, "what made the trail is there");
            w.leaveHunt("player-ada");
            for (int i = 0; i < 120; ++i)
                w.tick(.1);
            break;
        }
        for (int m = 0; m < 6; ++m)
            w.tick(600);                            // (A game hour; then smell again.)
        w.smellTracks("player-ada");
        expect(hour < 47, "a fresh trail turns up some time");
    }
    // They fade after a while.
    w.smellTracks("player-bo");
    for (int i = 0; i < 70; ++i)
        w.tick(10);
    expect(w.tracksOf("player-bo").empty(), "trails fade from the map");
}
void gearWearsAndMends()
{
    auto w = wilds();
    auto& ada = hunter(w, "player-ada");
    // A sword of each kind: the best is taken up.
    w.society().create("player-ada", "sword", 1, "test");
    w.society().create("player-ada", "sword~fine", 1, "test");
    expect(w.holdItem("player-ada", "sword").ok && World::swordHeld(ada) == "sword~fine", "the best sword one has is taken up");
    expect(w.stowItem("player-ada").ok && World::swordHeld(ada).empty(), "and put away");
    // A scarf worn to its end falls apart; with a spare, the spare goes on.
    w.society().create("player-ada", "wool_scarf", 2, "test");
    expect(w.wear("player-ada", "wool_scarf", "").ok, "a scarf on");
    const int most = World::durabilityOf("wool_scarf");
    expect(most > 0 && w.conditionOf(ada, "wool_scarf") == 1, "new, it is whole");
    ada.wear["wool_scarf"] = most - .01;
    for (int i = 0; i < 1300; ++i)
        w.tick(1);                                  // (Over two game hours: clothes wear with the days.)
    expect(Society::stock(*w.society().account("player-ada"), "wool_scarf") == 1, "worn out, it falls apart");
    expect(ada.worn.count("neck") && ada.worn.at("neck") == "wool_scarf", "and the spare goes on");
    // Mending: worn half through, a shop that deals in it mends it for a fee.
    ada.wear["wool_scarf"] = most / 2.0;
    expect(w.conditionOf(ada, "wool_scarf") < .6, "half worn");
    const auto* weaver = w.entity("wren");
    expect(weaver && w.canRepair("wren", "wool_scarf"), "a weaver mends a scarf");
    ada.position = {weaver->position.x + 1, weaver->position.y};
    const auto cash = w.society().account("player-ada")->cash;
    const auto cost = w.repairCost(ada, "wool_scarf");
    const auto r = w.repairGear("player-ada", "wren", "wool_scarf");
    expect(r.ok && cost > 0 && w.society().account("player-ada")->cash == cash - cost, "mended, for a fee: " + r.message);
    expect(w.conditionOf(ada, "wool_scarf") == 1, "whole again");
    expect(!w.canRepair("wren", "sword"), "but a weaver doesn't mend swords");
}
void noseAndMarks()
{
    // The nose: a physical stat that sharpens with use, to a limit.
    {
        auto w = wilds();
        auto& ada = hunter(w, "player-ada");
        const double before = World::noseAcuity(ada);
        for (int i = 0; i < 2000; ++i)
            w.trainNose("player-ada");
        expect(ada.smell > 1.4 && ada.smell <= 1.6 && World::noseAcuity(ada) > before, "the nose sharpens with use, to a limit");
    }
    // Masking oil hides a wolf's scent for a few hours.
    {
        auto w = wilds();
        auto& ada = hunter(w, "player-ada", 11.5, 10.5);
        expect(!w.maskScent("player-ada").ok, "no oil, no masking");
        w.society().create("player-ada", "masking_oil", 1, "test");
        expect(w.maskScent("player-ada").ok && w.scentMasked(ada), "masked");
        expect(w.scentClarity("wren", "player-ada") == 0, "nobody smells a masked wolf");
        for (int i = 0; i < 2500; ++i)
            w.tick(1);
        expect(!w.scentMasked(ada), "and it wears off in a few hours");
    }
    // Stolen goods give themselves away: the one robbed catches its maker's scent on the thief.
    const auto robbed = [](bool masked) {
        auto w = wilds();
        auto& ada = hunter(w, "player-ada", 10.5, 10.5);    // West of the weaver: the wind carries her scent to him.
        w.society().create("player-ada", "wool_scarf~masterwork@wren", 1, "test");
        if (masked)
        {
            w.society().create("player-ada", "masking_oil", 3, "test");
            w.maskScent("player-ada");
            w.maskScent("player-ada");
            w.maskScent("player-ada");
        }
        Incident inc;
        inc.id = "inc-test";
        inc.kind = "theft";
        inc.offender = "player-ada";
        inc.victim = "wren";
        inc.cell = "wilds";
        inc.item = "wool_scarf~masterwork@wren";
        inc.quantity = 1;
        w.crime().incidents.push_back(inc);
        for (int i = 0; i < 1200; ++i)
        {
            w.entity("player-ada")->position = {10.5, 10.5};
            w.tick(1);
        }
        (void)ada;
        for (const auto& i : w.crime().incidents)
            if (i.id == "inc-test")
                for (const auto& wit : i.witnesses)
                    if (wit.id == "wren" && wit.identified)
                        return true;
        return false;
    };
    expect(robbed(false), "the weaver smells her own work on the thief");
    expect(!robbed(true), "but not through masking oil");
    // A keen nose reads whose work someone carries.
    {
        auto w = wilds();
        auto& ada = hunter(w, "player-ada", 4.5, 5.5);      // West of the gardener.
        ada.smell = 1.6;
        ada.scentSkill = 100;
        w.society().create("sorrel", "wool_scarf~masterwork@wren", 1, "test");
        bool found = false;
        for (int i = 0; i < 20 && !found; ++i)
        {
            for (const auto& m : w.marksSmelt("player-ada"))
                found = found || (m.holder == "sorrel" && items::makerOf(m.item) == "wren");
            w.tick(1);
            ada.position = {w.entity("sorrel")->position.x - 1, w.entity("sorrel")->position.y};
        }
        expect(found, "a keen nose knows the weaver's work on the gardener");
    }
}
} // namespace

int main()
{
    try
    {
        dataLoads();
        startingAHunt();
        aCleanKillAndAFireKill();
        aRaggedKillYieldsLess();
        gameGetsAwayAndTheHuntEnds();
        onlyFriendsJoin();
        foraging();
        tracks();
        gearWearsAndMends();
        noseAndMarks();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Hunt tests passed: " << checks << " checks.\n";
    return 0;
}
