// Hunting and foraging (Docs/Design/41-hunting-and-foraging.md): a hunt is a fight against game found by the ground,
// the game lives only in its hunt, the hardest blow decides the yield and fire spoils it, kills press on a place, only
// friends may join, and foraging gives the ground's goods until a patch is picked over.
#include "RatwCrime.h"
#include "RatwItems.h"
#include "RatwWild.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cstdlib>
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
    // Past the positioning phase (doc 40): the hunter ready, as a player does.
    w.readyToFight(id, true);
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
    // Many small blows, the hardest a tenth of its health: over many boars (game that runs dies to one bite, doc 53;
    // boars and bears keep their health), less than clean kills would give.
    int got = 0, clean = 0;
    for (int attempt = 0; attempt < 40 && clean < 30; ++attempt)
    {
        auto w = wilds();
        hunter(w, "player-ada");
        for (int k = 0; k < attempt * 7; ++k)
            w.tick(1);                              // (Another moment, another hunt.)
        expect(w.startHunt("player-ada", {"boar", "boar"}).ok, "a hunt with boars");
        w.readyToFight("player-ada", true);
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
    w.hurtFighter(b, *b.fighter(runner), .01, battle::DownedBlunt, "player-ada", false);   // Hurt, it knows (not bitten: a bite kills).
    expect(!w.animalUnaware(runner), "a hurt animal is alert");
    expect(w.animalState(runner).empty() || w.animalState(runner) == "fleeing", "and game that runs bolts from whoever hurt it");
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
    // The nose: a physical stat that sharpens with use, to its cap (by practice: doc 49, Data/Progression/skills.json).
    {
        auto w = wilds();
        double clock = 1e9;
        w.realClock = [&clock] { return clock; };
        auto& ada = hunter(w, "player-ada");
        const double before = World::noseAcuity(ada), cap = practice::skill("smell")->cap;
        for (int i = 0; i < 4000; ++i, clock += 700)
            w.trainNose("player-ada");
        expect(ada.smell > 1.4 && ada.smell <= cap && World::noseAcuity(ada) > before, "the nose sharpens with use, to its cap");
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

// ------------------------------------------------------------------ Game that runs (doc 53, Phase 1)

// A hunt with just this game, the hunter ready; each animal and the hunter placed (tiles in the arena).
Battle& chosenHunt(World& w, const std::vector<std::string>& game)
{
    const auto started = w.startHunt("player-ada", game);
    expect(started.ok, "a hunt with chosen game: " + started.message);
    w.readyToFight("player-ada", true);
    w.tick(.2);
    return huntOf(w, "player-ada");
}

void put(Battle& b, const std::string& id, int dx, int dy)
{
    auto* f = b.fighter(id);
    f->x = b.x0 + dx;
    f->y = b.y0 + dy;
    f->walk.clear();
}

// Ticks until it is `id`'s turn (or a while passes); true if it is.
bool turnOf(World& w, const std::string& id, double most = 60)
{
    for (double t = 0; t < most; t += .1)
    {
        const auto* b = w.battleOf(id);
        if (b && b->fighter(id) && b->fighter(id)->acting && b->fighter(id)->walk.empty())
            return true;
        w.tick(.1);
    }
    return false;
}

void watchingAndBolting()
{
    auto w = wilds();
    hunter(w, "player-ada");
    auto& b = chosenHunt(w, {"roe_deer"});
    const auto deer = animalsIn(w, b).front();
    const auto* s = wild::speciesById("roe_deer");
    expect(s->flight == 6 && s->flees(), "a roe deer runs, and bolts from a wolf within 6 tiles");
    put(b, deer, 30, 20);
    put(b, "player-ada", 18, 20);                   // Twelve tiles off: further than its flight distance.
    expect(w.animalState(deer) == "grazing", "unaware, it grazes");
    expect(turnOf(w, "player-ada"), "Ada's turn");
    b.aware[{deer, "player-ada"}] = battle::AwareKept;   // It has seen her.
    w.battleAct("player-ada", "wait");
    expect(w.animalState(deer) == "watching" && w.animalWatching(deer) == "player-ada", "a far wolf it sees: it watches her");
    // Within its flight distance at the end of her turn: it bolts, its bar full.
    expect(turnOf(w, "player-ada"), "Ada's next turn");
    put(b, "player-ada", 26, 20);
    b.aware[{deer, "player-ada"}] = battle::AwareKept;
    w.battleAct("player-ada", "wait");
    expect(w.animalState(deer) == "fleeing" && b.fighter(deer)->meter >= 100, "within its flight distance: it bolts at once");
    const int before = std::max(std::abs(b.fighter(deer)->x - b.fighter("player-ada")->x), std::abs(b.fighter(deer)->y - b.fighter("player-ada")->y));
    const int fromX = b.fighter(deer)->x, fromY = b.fighter(deer)->y;
    for (double t = 0; t < 30 && (b.fighter(deer)->turnsTaken == 0 || b.fighter(deer)->acting || !b.fighter(deer)->walk.empty()); t += .1)
        w.tick(.1);
    const auto* d = b.fighter(deer);
    const int ran = std::max(std::abs(d->x - fromX), std::abs(d->y - fromY));
    const auto* ada = w.entity("player-ada");
    const int sprint = battle::moveRange(ada->dexterity, 0, 10);
    expect(d->status == "fled" || ran > sprint, "it runs further than her sprint: " + std::to_string(ran) + " > " + std::to_string(sprint));
    expect(d->status == "fled" || (d->x - fromX) > 0, "directly away from her (she was to the west)");
    expect(d->status == "fled" || std::max(std::abs(d->x - b.fighter("player-ada")->x), std::abs(d->y - b.fighter("player-ada")->y)) > before,
           "and further from her");
}

// Throw Voice drives (doc 53, 4): a noise thrown within a deer's flight distance makes it bolt away from that tile, though
// no wolf is near it; one further off only watches that way.
void throwVoiceDrives()
{
    auto w = wilds();
    hunter(w, "player-ada");
    expect(w.giveGift("player-ada", "sound", false).ok, "Ada is Gifted: Sound");
    auto& b = chosenHunt(w, {"roe_deer"});
    const auto deer = animalsIn(w, b).front();
    put(b, deer, 30, 20);
    put(b, "player-ada", 20, 20);                   // Ten tiles off: further than its flight distance of 6.
    expect(turnOf(w, "player-ada"), "Ada's turn");
    w.entity("player-ada")->mana = 100;
    const int fromX = b.fighter(deer)->x;
    const auto thrown = w.useGift("player-ada", "throw_voice", std::to_string(b.x0 + 27) + "," + std::to_string(b.y0 + 20));
    expect(thrown.ok && w.animalState(deer) == "fleeing", "a noise three tiles from it: it bolts: " + thrown.message);
    for (double t = 0; t < 30 && (b.fighter(deer)->turnsTaken == 0 || b.fighter(deer)->acting || !b.fighter(deer)->walk.empty()); t += .1)
    {
        if (b.fighter("player-ada")->acting)
            w.battleAct("player-ada", "wait");
        w.tick(.1);
    }
    const auto* d = b.fighter(deer);
    expect(d->status == "fled" || d->x > fromX, "away from the noise (to its west): east");
}

void aHiddenPartnerDoesntTurnIt()
{
    auto w = wilds();
    hunter(w, "player-ada");
    hunter(w, "player-bo", 41.5, 30.5);
    w.setFriends([](const std::string&, const std::string&) { return true; });
    auto& b = chosenHunt(w, {"roe_deer"});
    const auto deer = animalsIn(w, b).front();
    expect(w.joinBattle("player-bo", b.id, 0).ok, "Bo joins");
    w.readyToFight("player-bo", true);
    put(b, deer, 30, 20);
    put(b, "player-ada", 26, 20);                   // West of it, close.
    put(b, "player-bo", 36, 22);                    // East of it, beside its line, unseen.
    b.aware[{deer, "player-ada"}] = battle::AwareKept;
    b.aware[{deer, "player-bo"}] = 0;
    expect(turnOf(w, "player-ada"), "Ada's turn");
    b.aware[{deer, "player-ada"}] = battle::AwareKept;
    b.aware[{deer, "player-bo"}] = 0;
    w.battleAct("player-ada", "wait");
    expect(w.animalState(deer) == "fleeing", "it bolts from Ada");
    for (double t = 0; t < 30 && b.fighter(deer)->turnsTaken == 0; t += .1)
    {
        b.aware[{deer, "player-bo"}] = 0;
        w.tick(.1);
    }
    for (double t = 0; t < 10 && !b.fighter(deer)->walk.empty(); t += .1)
        w.tick(.1);
    const auto* d = b.fighter(deer);
    expect(d->status == "fled" || (d->x > 30 && std::abs(d->y - (b.y0 + 20)) <= 2), "straight on east, past Bo, whom it never noticed");
}

void calming()
{
    auto w = wilds();
    hunter(w, "player-ada");
    auto& b = chosenHunt(w, {"red_deer"});
    const auto deer = animalsIn(w, b).front();
    put(b, deer, 20, 20);
    put(b, "player-ada", 18, 20);
    expect(turnOf(w, "player-ada"), "Ada's turn");
    b.aware[{deer, "player-ada"}] = battle::AwareKept;
    w.battleAct("player-ada", "wait");
    expect(w.animalState(deer) == "fleeing", "it bolts");
    // She drops out of its senses: two of its turns later it calms, then goes back to grazing.
    bool calmed = false;
    for (double t = 0; t < 200 && b.fighter(deer)->status == "fighting" && w.animalState(deer) != "grazing"; t += .1)
    {
        b.aware[{deer, "player-ada"}] = 0;
        if (const auto* me = b.fighter("player-ada"); me && me->acting && me->walk.empty())
            w.battleAct("player-ada", "wait");
        w.tick(.1);
        calmed = calmed || w.animalState(deer) == "calming";
    }
    expect(b.fighter(deer)->status != "fighting" || (calmed && w.animalState(deer) == "grazing"), "it calms, then grazes");
}

void theDodge()
{
    auto w = wilds();
    hunter(w, "player-ada");
    hunter(w, "player-bo", 41.5, 30.5);
    w.setFriends([](const std::string&, const std::string&) { return true; });
    auto& b = chosenHunt(w, {"roe_deer", "hare"});
    const auto ids = animalsIn(w, b);
    const auto deer = w.animalOf(ids[0]) == "roe_deer" ? ids[0] : ids[1], hare = deer == ids[0] ? ids[1] : ids[0];
    expect(w.joinBattle("player-bo", b.id, 0).ok, "Bo joins");
    auto* ada = w.entity("player-ada");
    ada->dexterity = 50;
    const auto& me = *b.fighter("player-ada");
    std::string why;
    b.aware[{deer, "player-ada"}] = 0;
    expect(std::abs(w.huntDodge(me, *b.fighter(deer), &why) - .05) < 1e-9 && why == "unaware", "grazing, unaware of her: 5%");
    b.aware[{deer, "player-ada"}] = (battle::AwareSuspicious + battle::AwareAlert) / 2;
    expect(std::abs(w.huntDodge(me, *b.fighter(deer)) - .325) < 1e-9, "half noticing her: between 5% and 60%");
    b.aware[{deer, "player-ada"}] = battle::AwareKept;
    expect(std::abs(w.huntDodge(me, *b.fighter(deer), &why) - .6) < 1e-9 && why == "watching you", "alert to her: 60%");
    b.aware[{hare, "player-ada"}] = 0;
    expect(std::abs(w.huntDodge(me, *b.fighter(hare)) - .10) < 1e-9, "a hare dodges five points better");
    ada->dexterity = 70;
    b.aware[{deer, "player-ada"}] = 0;
    expect(std::abs(w.huntDodge(me, *b.fighter(deer)) - .02) < 1e-9, "a quick wolf: less, but never under 2%");
}

void drivenPast()
{
    // Driven: fleeing Bo, never having noticed Ada (crouched far off from the first): 15%; to Bo, whom it flees: 75%.
    auto w = wilds();
    hunter(w, "player-ada");
    hunter(w, "player-bo", 41.5, 30.5);
    w.setFriends([](const std::string&, const std::string&) { return true; });
    w.entity("player-ada")->posture = "crouching";
    w.entity("player-ada")->dexterity = 50;
    auto& b = chosenHunt(w, {"roe_deer"});
    const auto deer = animalsIn(w, b).front();
    expect(w.joinBattle("player-bo", b.id, 0).ok, "Bo joins");
    w.readyToFight("player-bo", true);
    put(b, deer, 30, 20);
    put(b, "player-ada", 2, 2);
    put(b, "player-bo", 28, 20);
    for (double t = 0; t < 60; t += .1)
    {
        b.aware[{deer, "player-ada"}] = 0;
        if (const auto* bo = b.fighter("player-bo"); bo && bo->acting && bo->walk.empty())
        {
            b.aware[{deer, "player-bo"}] = battle::AwareKept;
            w.battleAct("player-bo", "wait");
            break;
        }
        if (const auto* ada = b.fighter("player-ada"); ada && ada->acting && ada->walk.empty())
            w.battleAct("player-ada", "wait");
        w.tick(.1);
    }
    expect(w.animalState(deer) == "fleeing", "it bolts from Bo");
    std::string why;
    expect(std::abs(w.huntDodge(*b.fighter("player-ada"), *b.fighter(deer), &why) - .15) < 1e-9 && why == "driven",
           "driven past Ada, who it never saw: 15% (" + why + ")");
    expect(std::abs(w.huntDodge(*b.fighter("player-bo"), *b.fighter(deer)) - .75) < 1e-9, "to the one it flees: 75%");
}

void oneBiteAndQuality()
{
    auto w = wilds();
    hunter(w, "player-ada");
    auto& b = chosenHunt(w, {"red_deer", "red_deer", "boar"});
    std::vector<std::string> deer, boar;
    for (const auto& id : animalsIn(w, b))
        (w.animalOf(id) == "boar" ? boar : deer).push_back(id);
    const auto* account = w.society().account("player-ada");
    const auto meat = [&](int quality) {
        return Society::stock(*w.society().account("player-ada"), quality == 3 ? items::withMaker(items::withQuality("raw_meat", 3), "player-ada")
                                                                                : items::withQuality("raw_meat", quality));
    };
    (void)account;
    // A bite's worth on a red deer that never saw her: dead, masterwork.
    const int master = meat(3);
    w.hurtFighter(b, *b.fighter(deer[0]), 12, battle::DownedBite, "player-ada", true);
    expect(b.fighter(deer[0])->status == "dead", "one landed bite kills a red deer");
    expect(meat(3) > master, "never having seen her: masterwork");
    // One that has seen her: fine.
    expect(turnOf(w, "player-ada"), "Ada's turn");
    b.aware[{deer[1], "player-ada"}] = battle::AwareKept;
    w.battleAct("player-ada", "wait");          // (It notices her at the end of her turn.)
    const int fine = meat(2);
    w.hurtFighter(b, *b.fighter(deer[1]), 12, battle::DownedBite, "player-ada", true);
    expect(b.fighter(deer[1])->status == "dead" && meat(2) > fine, "one that saw her: fine, not masterwork");
    // A boar keeps its health.
    w.hurtFighter(b, *b.fighter(boar[0]), 12, battle::DownedBite, "player-ada", true);
    expect(b.fighter(boar[0])->status == "fighting", "a boar takes more than one bite");
}

void aDodgeSendsItOff()
{
    // A watching deer, bitten at from beside it: killed, or it dodges and flees the biter. (A new hunter each time,
    // so each bite's roll is its own.)
    bool dodged = false;
    int bites = 0;
    for (int attempt = 0; attempt < 40 && !dodged; ++attempt)
    {
        auto w = wilds();
        const std::string id = "player-a" + std::to_string(attempt);
        hunter(w, id);
        const auto started = w.startHunt(id, {"roe_deer"});
        expect(started.ok, "a hunt: " + started.message);
        w.readyToFight(id, true);
        w.tick(.2);
        auto& b = huntOf(w, id);
        const auto deer = animalsIn(w, b).front();
        if (!turnOf(w, id) || b.fighter(deer)->status != "fighting")
            continue;
        put(b, deer, 30, 20);
        put(b, id, 29, 20);
        b.aware[{deer, id}] = battle::AwareKept;
        w.battleAct(id, "bite", deer);
        ++bites;
        if (b.fighter(deer)->status == "fighting")
        {
            dodged = true;
            expect(w.animalState(deer) == "fleeing", "a dodge sends it fleeing, from the biter");
            expect(std::abs(w.huntDodge(*b.fighter(id), *b.fighter(deer)) - .75) < 1e-9, "and it flees the biter now");
        }
    }
    expect(dodged, "some bites are dodged (" + std::to_string(bites) + " tried)");
}

void lyingInWait()
{
    auto w = wilds();
    hunter(w, "player-ada");
    auto& b = chosenHunt(w, {"roe_deer"});
    const auto deer = animalsIn(w, b).front();
    put(b, deer, 40, 10);
    put(b, "player-ada", 30, 21);
    b.aware[{deer, "player-ada"}] = 0;
    expect(turnOf(w, "player-ada"), "Ada's turn");
    w.battleAct("player-ada", "stalk");
    w.battleAct("player-ada", "wait");
    expect(w.huntWaiting("player-ada"), "crouched at the end of her turn without biting: she lies in wait");
    // The deer steps past her.
    auto* d = b.fighter(deer);
    d->x = b.x0 + 28, d->y = b.y0 + 20;
    d->walk = {{b.x0 + 29, b.y0 + 20}, {b.x0 + 30, b.y0 + 20}, {b.x0 + 31, b.y0 + 20}};
    d->stepAt = 0;
    w.tick(.05);
    bool sprang = false;
    for (const auto& line : b.log)
        sprang = sprang || line.text.find("springs from hiding") != std::string::npos;
    expect(sprang && !w.huntWaiting("player-ada"), "she springs as it passes");
    expect(d->status == "dead" || d->walk.empty(), "it dies, or swerves and stops");
}

void whoMayJoin()
{
    // Doc 53, 1.4: anyone may join while the hunt's starter allows hunting partners; only the starter's setting counts;
    // a closed hunt may be asked into, or a wolf near invited; never one a hunter has blocked; never the game's side.
    auto w = wilds();
    hunter(w, "player-ada");
    hunter(w, "player-bo", 41.5, 30.5);
    hunter(w, "player-cy", 42.5, 30.5);
    hunter(w, "player-di", 43.5, 30.5);
    hunter(w, "player-ed", 44.5, 30.5);
    setOut(w, "player-ada");
    const auto id = huntOf(w, "player-ada").id;
    expect(w.huntStarterOf(id) == "player-ada", "Ada started it");
    expect(!w.joinBattle("player-bo", id, 1).ok, "never the game's side");
    expect(w.mayJoinHunt(huntOf(w, "player-ada"), "player-bo") && w.joinBattle("player-bo", id, 0).ok, "open: a stranger joins");
    // Bo turns his own setting off: it isn't his hunt, so it stays open.
    expect(w.setPartners("player-bo", "hunt", false).ok && w.entity("player-bo")->noHuntPartners, "Bo: no hunting partners");
    expect(w.mayJoinHunt(huntOf(w, "player-ada"), "player-cy"), "only the starter's setting counts (the user)");
    // Ada turns hers off: closed to strangers.
    w.setPartners("player-ada", "hunt", false);
    expect(!w.mayJoinHunt(huntOf(w, "player-ada"), "player-cy") && !w.joinBattle("player-cy", id, 0).ok, "the starter closes it");
    expect(w.mayAskHunt(huntOf(w, "player-ada"), "player-cy"), "Cy may ask");
    expect(w.askToJoinHunt("player-cy", id).ok && !w.askToJoinHunt("player-cy", id).ok, "Cy asks, once");
    expect(w.huntAsks().size() == 1 && w.huntAsks().front().from == "player-cy", "the ask waits");
    const auto let = w.answerHuntAsk("player-ada", "player-cy", true);
    expect(let.ok && w.battleOf("player-cy") && w.battleOf("player-cy")->id == id, "let in, Cy joins: " + let.message);
    // Asked and refused; then a wolf near invited.
    expect(w.askToJoinHunt("player-di", id).ok && w.answerHuntAsk("player-bo", "player-di", false).ok && !w.battleOf("player-di"),
           "Not now: Di stays out");
    expect(w.inviteToHunt("player-ada", "player-di").ok && w.mayJoinHunt(huntOf(w, "player-ada"), "player-di") && w.joinBattle("player-di", id, 0).ok,
           "invited: Di joins");
    // Blocked by a hunter: neither joins nor asks, even if the hunt were open.
    w.setBlocked([](const std::string& a, const std::string& b) { return (a == "player-ada" && b == "player-ed") || (a == "player-ed" && b == "player-ada"); });
    w.setPartners("player-ada", "hunt", true);
    expect(!w.mayJoinHunt(huntOf(w, "player-ada"), "player-ed") && !w.mayAskHunt(huntOf(w, "player-ada"), "player-ed"), "a blocked wolf is shut out");
    expect(w.leaveHunt("player-ada").ok, "one may give up a hunt");
    expect(!w.battleOf("player-ada") || w.battleOf("player-ada")->fighter("player-ada")->status == "fled", "and is out of it");
}

void sharing()
{
    // Doc 53, 1.5: a kill shared equally among those taking part, nothing made or lost; one idle gets nothing; the one
    // who made the kill always shares; Give my share moves it.
    auto w = wilds();
    hunter(w, "player-ada");
    hunter(w, "player-bo", 41.5, 30.5);
    hunter(w, "player-cy", 42.5, 30.5);
    auto& b = chosenHunt(w, {"red_deer", "red_deer"});
    expect(w.joinBattle("player-bo", b.id, 0).ok && w.joinBattle("player-cy", b.id, 0).ok, "Bo and Cy join");
    w.readyToFight("player-bo", true);
    w.readyToFight("player-cy", true);
    // Ada and Bo each take a turn that does something (they crouch); Cy idles.
    for (const auto* who : {"player-ada", "player-bo"})
    {
        expect(turnOf(w, who), std::string(who) + "'s turn");
        w.battleAct(who, "stalk");
        w.battleAct(who, "wait");
    }
    const auto deer = animalsIn(w, b);
    const auto meat = [&](const std::string& id) { return held(w, id, "raw_meat"); };
    const int adaBefore = meat("player-ada"), boBefore = meat("player-bo"), cyBefore = meat("player-cy");
    w.hurtFighter(b, *b.fighter(deer[0]), 12, battle::DownedBite, "player-ada", true);
    const int adaGot = meat("player-ada") - adaBefore, boGot = meat("player-bo") - boBefore, cyGot = meat("player-cy") - cyBefore;
    expect(adaGot + boGot == 8 && std::abs(adaGot - boGot) <= 1, "8 raw meat split between Ada and Bo, no more, no less: " +
                                                                     std::to_string(adaGot) + " and " + std::to_string(boGot));
    expect(cyGot == 0, "Cy, idle, gets nothing");
    const int hides = held(w, "player-ada", "hide") + held(w, "player-bo", "hide");
    expect(hides == 1, "the one hide to one of them, by chance");
    // Give my share: Bo's to Ada, to carry.
    const auto* share = w.huntShareOf("player-bo");
    expect(share && std::find(share->hunters.begin(), share->hunters.end(), "player-ada") != share->hunters.end(), "Bo has a share to give, to Ada");
    expect(w.giveHuntShare("player-bo", "player-cy").ok == false, "not to one who didn't share it");
    const int adaNow = meat("player-ada");
    expect(w.giveHuntShare("player-bo", "player-ada").ok && meat("player-bo") == boBefore && meat("player-ada") == adaNow + boGot,
           "Bo's share is Ada's to carry");
}

void moreGameAndCompanions()
{
    // Doc 53, 1.6: a hunter joining brings game in with them (half the expected count, by chance: some of the time).
    int brought = 0;
    for (int attempt = 0; attempt < 12 && brought == 0; ++attempt)
    {
        auto w = wilds();
        hunter(w, "player-ada");
        const std::string joiner = "player-j" + std::to_string(attempt);
        hunter(w, joiner, 41.5, 30.5);
        setOut(w, "player-ada");
        auto& b = huntOf(w, "player-ada");
        const auto before = animalsIn(w, b).size();
        expect(w.joinBattle(joiner, b.id, 0).ok, "a joiner");
        brought += int(animalsIn(w, b).size() - before);
    }
    expect(brought > 0, "a joiner brings game in");
    // Doc 53, 1.8: a companion lies in wait beside its stalking leader, instead of charging the game.
    auto w = wilds();
    hunter(w, "player-ada");
    auto& fern = w.addPlayer("npc-fern", "Fern");
    fern.npc = true;
    fern.leaderId = "player-ada";
    fern.cellId = "wilds";
    fern.position = {41.5, 30.5};
    auto& b = chosenHunt(w, {"roe_deer"});
    expect(w.joinBattle("npc-fern", b.id, 0).ok, "Fern comes along");
    const auto deer = animalsIn(w, b).front();
    put(b, deer, 40, 20);
    put(b, "player-ada", 20, 20);
    put(b, "npc-fern", 21, 21);
    b.fighter("player-ada")->stalking = true;
    for (double t = 0; t < 60 && b.fighter("npc-fern")->turnsTaken == 0; t += .1)
    {
        if (const auto* me = b.fighter("player-ada"); me && me->acting && me->walk.empty())
            w.battleAct("player-ada", "wait");
        w.tick(.1);
    }
    for (double t = 0; t < 10 && b.fighter("npc-fern")->acting; t += .1)
        w.tick(.1);
    expect(w.huntWaiting("npc-fern") && b.fighter("npc-fern")->stalking, "Fern crouched and lies in wait");
    expect(std::max(std::abs(b.fighter("npc-fern")->x - b.fighter("player-ada")->x), std::abs(b.fighter("npc-fern")->y - b.fighter("player-ada")->y)) <= 3,
           "beside her stalking leader, not charging off at the deer");
}

int main()
{
    try
    {
        dataLoads();
        startingAHunt();
        aCleanKillAndAFireKill();
        aRaggedKillYieldsLess();
        gameGetsAwayAndTheHuntEnds();
        whoMayJoin();
        sharing();
        moreGameAndCompanions();
        watchingAndBolting();
        throwVoiceDrives();
        aHiddenPartnerDoesntTurnIt();
        calming();
        theDodge();
        drivenPast();
        oneBiteAndQuality();
        aDodgeSendsItOff();
        lyingInWait();
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
