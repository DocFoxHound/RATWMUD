#include "RatwWorld.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <iostream>
#include <iterator>
#include <map>
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
World town()
{
    World world;
    const auto loaded = world.loadWorldFile(RATW_SOURCE_DIR "/Data/Worlds/Greyfen/world.ratw");
    expect(loaded.ok, "Town loads: " + loaded.message);
    return world;
}
bool standable(const World& world, const std::string& cellId, Vec2 p)
{
    const auto* c = world.cell(cellId);
    const auto* t = c ? c->tile(int(std::floor(p.x)), int(std::floor(p.y))) : nullptr;
    return t && !t->solid;
}
int held(const World& world, const std::string& id, const std::string& item)
{
    const auto* account = world.society().account(id);
    return account ? Society::stock(*account, item) : 0;
}
// Runs whole simulated seconds and fails fast on an unreachable authored goal.
void run(World& world, int seconds)
{
    for (int second = 0; second < seconds; ++second)
    {
        world.tick(1);
        for (const auto& pair : world.entities())
            expect(pair.second.activity.find("route unavailable") == std::string::npos,
                   pair.first + " has no route: " + pair.second.activity);
        expect(world.society().conserved(), "Town money stays conserved");
    }
}
void layout()
{
    auto world = town();
    expect(world.cells().size() == 6, "Town square plus five interiors");
    for (const auto* id : {"town", "shop", "wren_house", "barracks", "cottage", "longhouse"})
        expect(world.cell(id) && world.cell(id)->region == "greyfen", std::string("Town cell exists: ") + id);
    expect(world.cell("town")->outdoors && !world.cell("shop")->outdoors, "Only the streets are outdoors");
    for (const auto& pair : world.doors())
    {
        const auto& d = pair.second;
        expect(standable(world, d.cellId, d.position), pair.first + " sits on a passable tile");
        const auto* other = world.door(d.linkedDoor);
        expect(other && other->linkedDoor == d.id && other->cellId == d.targetCell && other->open == d.open,
               pair.first + " has a reciprocal linked door");
        expect(standable(world, d.targetCell, d.arrival), pair.first + " arrives on a passable tile");
    }
    int guards = 0, civilians = 0, merchants = 0;
    for (const auto& pair : world.society().state().residents)
    {
        const auto* e = world.entity(pair.first);
        expect(e && e->npc && standable(world, e->cellId, e->position), pair.first + " starts on open ground");
        expect(standable(world, pair.second.homeCell, {pair.second.homeX, pair.second.homeY}),
               pair.first + " has a usable bed");
        guards += pair.second.role == "guard";
        civilians += pair.second.role == "civilian";
        merchants += world.society().merchant(pair.first);
    }
    expect(guards == 4 && civilians == 5 && merchants == 1, "Four guards, five civilians and one shopkeeper");
    expect(world.society().resident("wren")->homeCell == "wren_house", "Shopkeeper has own house");
    const auto* sheet = world.society().spec("wren");
    expect(sheet && sheet->personality.find("Shrewd") == 0 && sheet->backstory.find("peddler") != std::string::npos,
           "Character sheets reach the live-dialogue context");
    expect(world.society().resident("sloe")->homeCell == "barracks", "Guards live in the barracks");
}
void playerServices()
{
    auto world = town();
    auto& player = world.addPlayer("player-visitor", "Visitor");
    expect(player.cellId == "town", "Players arrive in the town square");
    expect(world.herbPatchCell() == "town", "Herb garden lives in town");
    const auto patch = world.herbPatchPosition();
    player.position = {patch.x + 1, patch.y};
    expect(world.gather("player-visitor").ok && held(world, "player-visitor", "herbs") == 3,
           "Players gather from the town herb garden");
    auto* keeper = world.entity("wren");
    player.cellId = keeper->cellId;
    player.position = {keeper->position.x, keeper->position.y + 1};
    const auto bought = world.trade("player-visitor", "wren", "meal", 1, true);
    expect(bought.ok, "Players buy from the shopkeeper: " + bought.message);
    expect(held(world, "player-visitor", "meal") == 2 && world.society().conserved(), "Purchase moves real goods");
    expect(!world.trade("player-visitor", "npc_keeper", "meal", 1, true).ok, "Demo keeper is absent from town");
}
void dailyLife()
{
    auto world = town();
    world.setTimeOfDay(11);
    run(world, 240);
    expect(world.society().resident("wren")->task == "trade" &&
               world.entity("wren")->cellId == "shop",
           "Shopkeeper keeps shop by day");
    expect(world.society().resident("sloe")->task == "patrol" &&
               world.entity("sloe")->cellId == "town",
           "Day guard patrols the streets");
    expect(world.society().resident("birch")->task == "sleep", "Night guard sleeps by day");
    world.setTimeOfDay(23);
    run(world, 300);
    for (const auto* id : {"wren", "sorrel", "fennel", "harrow", "sloe"})
    {
        const auto* life = world.society().resident(id);
        const auto* e = world.entity(id);
        expect(life->task == "sleep" && e->cellId == life->homeCell, std::string(id) + " goes home to sleep");
    }
    expect(world.society().resident("tamsin")->task == "patrol" &&
               world.entity("tamsin")->cellId == "town",
           "Night guard walks the streets after dark");
    const auto saved = world.save();
    auto restored = town();
    const auto back = restored.restore(saved);
    expect(back.ok, "Town checkpoints restore into the town: " + back.message + " " + back.targetId);
    World demo;
    expect(!demo.restore(saved).ok, "Town checkpoints are rejected by the demo world");
}
void wagesAndFood()
{
    auto world = town();
    world.setTimeOfDay(12);
    auto state = world.save();
    state.society.residents.at("linden").hunger = 58;
    state.society.accounts.at("linden").stock.erase("meal");
    expect(world.restore(state).ok, "Hungry fixture restores");
    const auto treasury = world.society().account("treasury")->cash;
    run(world, 900);
    bool bought = false;
    for (const auto& entry : world.society().state().ledger)
        bought = bought || (entry.kind == "resident food purchase" && entry.from == "linden");
    expect(bought, "Hungry civilian walks to the shop and buys a meal");
    expect(world.society().account("treasury")->cash < treasury, "Guards and workers draw wages from the treasury");
}
// Copies Greyfen to a scratch folder with one manifest line rewritten.
Result loadVariant(const std::string& from, const std::string& to)
{
    namespace fs = std::filesystem;
    const fs::path source = RATW_SOURCE_DIR "/Data/Worlds/Greyfen";
    const fs::path scratch = fs::temp_directory_path() / "ratw-town-variant";
    fs::remove_all(scratch);
    fs::create_directories(scratch);
    fs::copy(source / "cells", scratch / "cells", fs::copy_options::recursive);
    std::ifstream in(source / "world.ratw");
    std::stringstream text;
    text << in.rdbuf();
    std::string manifest = text.str();
    const auto at = manifest.find(from);
    expect(at != std::string::npos, "Variant anchor exists: " + from);
    manifest.replace(at, from.size(), to);
    std::ofstream(scratch / "world.ratw") << manifest;
    World world;
    const auto result = world.loadWorldFile((scratch / "world.ratw").string());
    fs::remove_all(scratch);
    return result;
}
// Walks a player in one direction for up to `seconds`; returns true once they changed cell.
bool walk(World& world, const std::string& id, Vec2 from, const std::string& cellId, double dx, double dy, double seconds = 3)
{
    auto* p = world.entity(id);
    p->cellId = cellId;
    p->position = from;
    world.move(id, dx, dy);
    for (double t = 0; t < seconds; t += .1)
    {
        world.tick(.1);
        if (world.entity(id)->cellId != cellId)
            return true;
    }
    return false;
}
void doorsWalkThrough()
{
    auto world = town();
    world.addPlayer("player-walker", "Walker");
    expect(walk(world, "player-walker", {12.5, 14.5}, "town", 0, -1) && world.entity("player-walker")->cellId == "shop",
           "Walking into an open shop door enters the shop");
    const auto* inside = world.entity("player-walker");
    expect(std::abs(inside->position.x - 10.5) < .01 && std::abs(inside->position.y - 10.5) < .01,
           "Arrival is the tile beside the far door, so the walker does not bounce back");
    expect(walk(world, "player-walker", {10.5, 10.5}, "shop", 0, 1) && world.entity("player-walker")->cellId == "town",
           "Walking back out through the doorway returns to the street");
    expect(!world.door("link_wren_door_a")->open, "Wren's door starts closed");
    expect(walk(world, "player-walker", {8.5, 23.5}, "town", 0, 1) && world.entity("player-walker")->cellId == "wren_house",
           "Walking into a closed, unlocked door opens it and goes through");
    expect(world.door("link_wren_door_a")->open && world.door("link_wren_door_b")->open, "Both sides of the door are now open");
    expect(!walk(world, "player-walker", {14.5, 20.5}, "town", 1, 0, 1.5), "Walking along the street changes nothing");
}
void lockedDoorsStillBlock()
{
    namespace fs = std::filesystem;
    const fs::path source = RATW_SOURCE_DIR "/Data/Worlds/Greyfen";
    const fs::path scratch = fs::temp_directory_path() / "ratw-town-locked";
    fs::remove_all(scratch);
    fs::create_directories(scratch);
    fs::copy(source / "cells", scratch / "cells", fs::copy_options::recursive);
    std::ifstream in(source / "world.ratw");
    std::stringstream text;
    text << in.rdbuf();
    std::string manifest = text.str();
    for (const auto* pair : {"\"link_wren_door_b\" 0 0 0 0", "\"link_wren_door_a\" 0 0 0 0"})
    {
        const std::string from = pair, to = from.substr(0, from.size() - 7) + "0 1 0 0";
        manifest.replace(manifest.find(from), from.size(), to);
    }
    std::ofstream(scratch / "world.ratw") << manifest;
    World world;
    const auto loaded = world.loadWorldFile((scratch / "world.ratw").string());
    fs::remove_all(scratch);
    expect(loaded.ok, "Locked-door variant loads: " + loaded.message);
    world.addPlayer("player-walker", "Walker");
    expect(!walk(world, "player-walker", {8.5, 23.5}, "town", 0, 1) && !world.door("link_wren_door_a")->open,
           "A locked door still blocks and stays shut");
}
void rejectsBadResidents()
{
    expect(loadVariant("RATW_WORLD 2", "RATW_WORLD 2").ok, "Unmodified copy loads");
    const std::pair<std::string, std::string> bad[] = {
        {"\"town\" 29.5 16.5 \"barracks\" 22.5 2.5", "\"town\" 0.5 0.5 \"barracks\" 22.5 2.5"}, // Harrow's post in a wall.
        {"\"town_watch\" 30 0 1", "\"no_such_route\" 30 0 1"},                                      // Unknown route.
        {"\"merchant\"", "\"wizard\""},                                                               // Unknown role.
        {"\"wren\" \"Wren\"", "\"treasury\" \"Wren\""},                                          // Reserved ID.
        {"economy 1000", "economy -5"},                                                                // Negative treasury.
        {"story \"wren\"", "story \"nobody\""},                                                        // Story without resident.
        {"herbs \"town\" 4.5 14.5", "herbs \"town\" 4.2 14.5"},                                      // Off tile center.
    };
    for (const auto& [from, to] : bad)
        expect(!loadVariant(from, to).ok, "Rejects bad resident data: " + to);
}
} // namespace

// The server loads builds from the world database: the same files, held in memory.
void loadsFromMemory()
{
    const std::string root = RATW_SOURCE_DIR "/Data/Worlds/Greyfen/";
    auto read = [](const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    std::map<std::string, std::string> files{{"world.ratw", read(root + "world.ratw")}};
    for (const auto& entry : std::filesystem::directory_iterator(root + "cells"))
        files["cells/" + entry.path().filename().string()] = read(entry.path().string());
    const World fromDisk = town();
    World fromMemory;
    const auto loaded = fromMemory.loadWorldFiles(files, "build 1");
    expect(loaded.ok, "Build loads from memory: " + loaded.message);
    expect(fromMemory.cells().size() == fromDisk.cells().size() && fromMemory.doors().size() == fromDisk.doors().size() &&
               fromMemory.society().state().residents.size() == fromDisk.society().state().residents.size(),
           "A build in memory loads the same world as its files on disk");
    for (const auto& [id, cell] : fromDisk.cells())
        expect(fromMemory.cell(id) && fromMemory.cell(id)->tiles.size() == cell.tiles.size(), "Same cell " + id);

    // The same checks guard a build: a missing cell file, or a path that tries to leave the build, is refused whole.
    auto missing = files;
    missing.erase(std::prev(missing.end()));
    World refused;
    expect(!refused.loadWorldFiles(missing, "broken").ok, "A build missing a cell file is refused");
    auto escaping = files;
    escaping["world.ratw"].replace(escaping["world.ratw"].find("\"cells/"), 7, "\"../cells/");
    expect(!refused.loadWorldFiles(escaping, "escaping").ok, "A build cannot name files outside itself");
    expect(!refused.loadWorldFiles({}, "empty").ok, "A build needs a manifest");
    expect(refused.cells().size() == World().cells().size(), "Refused builds leave the world untouched");
}

// A new route with a post indoors: the guard walks through the door and back while on watch.
void routesCrossDoors()
{
    const std::string root = RATW_SOURCE_DIR "/Data/Worlds/Greyfen/";
    auto read = [](const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    std::map<std::string, std::string> files{{"world.ratw", read(root + "world.ratw")}};
    for (const auto& entry : std::filesystem::directory_iterator(root + "cells"))
        files["cells/" + entry.path().filename().string()] = read(entry.path().string());
    auto& manifest = files["world.ratw"];
    const auto routeAt = manifest.find("route \"town_watch\"");
    manifest.insert(routeAt, "route \"cross_watch\" 2 \"town\" 30.5 20.5 \"barracks\" 10.5 4.5\n");
    const auto sloe = manifest.find("resident \"sloe\"");
    const auto assigned = manifest.find("\"town_watch\"", sloe);
    manifest.replace(assigned, 12, "\"cross_watch\"");
    World world;
    const auto loaded = world.loadWorldFiles(files, "cross-door route");
    expect(loaded.ok, "A route with an indoor post loads: " + loaded.message);
    world.setTimeOfDay(9);
    std::set<std::string> visited;
    for (int second = 0; second < 900; ++second)
    {
        world.tick(1);
        expect(world.entity("sloe")->activity.find("route unavailable") == std::string::npos, "Sloe can reach every post");
        if (world.society().resident("sloe")->task == "patrol")
            visited.insert(world.entity("sloe")->cellId);
    }
    expect(visited.count("town") && visited.count("barracks"), "The guard walks the route through the barracks door and back");
}

std::map<std::string, std::string> greyfenFiles()
{
    const std::string root = RATW_SOURCE_DIR "/Data/Worlds/Greyfen/";
    auto read = [](const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    std::map<std::string, std::string> files{{"world.ratw", read(root + "world.ratw")}};
    for (const auto& entry : std::filesystem::directory_iterator(root + "cells"))
        files["cells/" + entry.path().filename().string()] = read(entry.path().string());
    return files;
}
std::string lineOf(const std::string& manifest, const std::string& start)
{
    const auto at = manifest.find(start);
    return manifest.substr(at, manifest.find('\n', at) - at);
}

// A civilian with a route travels it through working hours and rests on the road, not at home, at night.
void travellersWalkTheirRoute()
{
    auto files = greyfenFiles();
    auto& manifest = files["world.ratw"];
    const std::string fennel = lineOf(manifest, "resident \"fennel\"");
    std::string traveller = fennel;
    traveller.replace(traveller.find(" 8 17 \"-\" "), 11, " 8 17 \"town_watch\" ");
    manifest.replace(manifest.find(fennel), fennel.size(), traveller);
    World world;
    const auto loaded = world.loadWorldFiles(files, "traveller");
    expect(loaded.ok, "A civilian may walk a route: " + loaded.message);
    world.setTimeOfDay(9);
    std::set<std::pair<double, double>> goals;
    for (int second = 0; second < 900; ++second)
    {
        world.tick(1);
        const auto* life = world.society().resident("fennel");
        expect(world.entity("fennel")->activity.find("route unavailable") == std::string::npos, "Fennel can reach every post");
        if (life->task == "minding a market stall")
            goals.insert({life->goalX, life->goalY});
    }
    expect(goals.size() >= 2, "The traveller moves on from post to post through the day");
    world.setTimeOfDay(23);
    run(world, 120);
    const auto* life = world.society().resident("fennel");
    expect(life->task == "sleep" && world.entity("fennel")->cellId != life->homeCell,
           "The traveller rests on the road rather than going home");
}

// Dungeon Master live changes: one NPC at a time, taken from a validated candidate world, money conserved.
void residentsChangeLive()
{
    auto world = town();
    world.setTimeOfDay(11);
    run(world, 30);
    auto files = greyfenFiles();
    auto& manifest = files["world.ratw"];
    // A newcomer (a copy of Sorrel), Sloe's duty renamed, Fennel gone.
    std::string newcomer = lineOf(manifest, "resident \"sorrel\"");
    newcomer.replace(newcomer.find("\"sorrel\""), 8, "\"newbie\"");
    newcomer.replace(newcomer.find("\"Sorrel\""), 8, "\"Newbie\"");
    manifest += newcomer + "\n";
    const std::string sloe = lineOf(manifest, "resident \"sloe\"");
    std::string renamed = sloe;
    renamed.replace(renamed.find("\"on patrol\""), 11, "\"on the wall\"");
    manifest.replace(manifest.find(sloe), sloe.size(), renamed);
    const std::string fennel = lineOf(manifest, "resident \"fennel\"");
    manifest.erase(manifest.find(fennel), fennel.size() + 1);
    if (const auto story = manifest.find("story \"fennel\""); story != std::string::npos)
        manifest.erase(story, manifest.find('\n', story) - story + 1);
    World candidate;
    const auto loaded = candidate.loadWorldFiles(files, "candidate");
    expect(loaded.ok, "The candidate world loads: " + loaded.message);

    const auto supply = world.society().moneySupply();
    const auto fennelCash = world.society().account("fennel")->cash;
    const auto treasury = world.society().account("treasury")->cash;
    expect(world.adoptResident(candidate, "newbie").ok, "A new NPC joins the running world");
    expect(world.entity("newbie") && world.entity("newbie")->npc && world.society().resident("newbie"), "With a body and a life");
    expect(world.society().conserved(), "The newcomer's authored purse is recorded as new money");
    expect(world.society().moneySupply() == supply + world.society().account("newbie")->cash, "Only the newcomer's purse is new");
    expect(world.adoptResident(candidate, "sloe").ok && world.entity("sloe")->activity == "on the wall", "A changed NPC is updated in place");
    expect(world.society().spec("sloe")->workLabel == "on the wall", "Their definition follows the change");
    expect(world.adoptResident(candidate, "fennel").ok, "A removed NPC leaves");
    expect(!world.entity("fennel") && !world.society().resident("fennel") && !world.society().spec("fennel"), "Body, life and definition are gone");
    expect(world.society().account("treasury")->cash == treasury + fennelCash && world.society().conserved(), "Their purse returns to the town");
    expect(!world.adoptResident(candidate, "nobody").ok, "Unknown NPCs are refused");
    expect(!world.adoptResident(candidate, "player-visitor").ok || !world.entity("player-visitor"), "Players are never adopted as NPCs");
    run(world, 120);
    const auto saved = world.save();
    World restored;
    expect(restored.loadWorldFiles(files, "restart").ok, "The changed world loads on restart");
    expect(restored.restore(saved).ok, "And its save restores into it");
}

// A dead NPC keeps no schedule: no walking, no work, and the town's books still balance.
void deadNpcsRest()
{
    auto world = town();
    world.setTimeOfDay(11);
    expect(world.setDead("sloe", true).ok, "Kill an NPC");
    const auto at = world.entity("sloe")->position;
    run(world, 120);
    expect(world.entity("sloe")->position.x == at.x && world.entity("sloe")->position.y == at.y, "The dead stay where they fell");
    expect(world.setDead("sloe", false).ok, "Revive them");
    run(world, 240);
    expect(world.society().resident("sloe")->task == "patrol",
           "The revived go back to their duties (task: " + world.society().resident("sloe")->task + ", " + world.society().resident("sloe")->reason + ")");
}

// Wander areas (Dungeon Master): a civilian roams the open tiles of their area; walls in the painting are dropped.
void wanderAreas()
{
    auto files = greyfenFiles();
    // Four open tiles in the square and one wall (0,0).
    files["world.ratw"] += "wander \"sorrel\" 5 \"town\" 26.5 18.5 \"town\" 30.5 18.5 \"town\" 26.5 22.5 \"town\" 30.5 22.5 \"town\" 0.5 0.5\n";
    World world;
    const auto loaded = world.loadWorldFiles(files, "wander");
    expect(loaded.ok, "A world with a wander area loads: " + loaded.message);
    expect(world.society().spec("sorrel")->wander.size() == 4, "Solid tiles are dropped from a wander area");
    world.setTimeOfDay(9);
    std::set<std::string> goals;
    for (int second = 0; second < 1800; ++second)
    {
        world.tick(1);
        const auto* life = world.society().resident("sorrel");
        if (life->task != "sleep" && !life->goalCell.empty())
            goals.insert(life->goalCell + ":" + std::to_string(int(life->goalX)) + "," + std::to_string(int(life->goalY)));
    }
    for (const auto& goal : goals)
        expect(goal == "town:26,18" || goal == "town:30,18" || goal == "town:26,22" || goal == "town:30,22" || goal.rfind("shop:", 0) == 0,
               "Sorrel's goals stay inside her wander area (or a meal at the shop): " + goal);
    expect(goals.size() >= 2, "She moves between tiles of the area");

    // A running world takes wander areas (and routes) from a validated candidate.
    auto running = town();
    expect(running.society().spec("sorrel")->wander.empty(), "No wander area before");
    running.adoptLayers(world);
    expect(running.society().spec("sorrel")->wander.size() == 4, "adoptLayers brings the wander area in");
    expect(!files["world.ratw"].empty() && !World().loadWorldFiles({{"world.ratw", greyfenFiles()["world.ratw"] + "wander \"nobody\" 1 \"town\" 26.5 18.5\n"}}, "bad").ok,
           "A wander area for an unknown resident is refused");
}

// Factions from the live tables (Dungeon Master): claims records replace the territory records' claims.
void factionClaims()
{
    auto files = greyfenFiles();
    files["world.ratw"] += "faction \"watch\" \"Town Watch\" \"#aabbcc\"\nclaims \"town\" 1 \"watch\"\nclaims \"shop\" 0\n";
    World world;
    const auto loaded = world.loadWorldFiles(files, "factions");
    expect(loaded.ok, "A world with live claims loads: " + loaded.message);
    expect(world.cell("town")->factionClaims == std::vector<std::string>{"watch"}, "The town is claimed by the watch");
    expect(world.cell("shop")->factionClaims.empty(), "The shop is claimed by nobody");
    for (const auto* bad : {"claims \"town\" 1 \"nobody\"\n", "claims \"nowhere\" 0\n", "claims \"town\" 0\nclaims \"town\" 0\n"})
        expect(!World().loadWorldFiles({{"world.ratw", files["world.ratw"].substr(0, files["world.ratw"].find("claims ")) + bad}}, "bad").ok,
               std::string("Refused: ") + bad);
    auto running = town();
    expect(running.factions().empty(), "No factions before");
    running.adoptFactions(world);
    expect(running.factions().count("watch") && running.cell("town")->factionClaims.size() == 1, "adoptFactions brings factions and claims in");
}

// A checkpoint restores into a world rebuilt since: residents who remain keep their lives and money, a newcomer
// starts fresh with the authored purse, and a departed resident's coins return to the treasury.
void checkpointsSurviveAChangedPopulation()
{
    auto before = town();
    before.setTimeOfDay(11);
    run(before, 60);
    const auto saved = before.save();
    const auto sorrelCash = before.society().account("sorrel")->cash;
    const auto fennelCash = before.society().account("fennel")->cash;
    const auto treasury = before.society().account("treasury")->cash;
    auto files = greyfenFiles();
    auto& manifest = files["world.ratw"];
    std::string newcomer = lineOf(manifest, "resident \"sorrel\"");
    newcomer.replace(newcomer.find("\"sorrel\""), 8, "\"newbie\"");
    newcomer.replace(newcomer.find("\"Sorrel\""), 8, "\"Newbie\"");
    manifest += newcomer + "\n";
    const std::string fennel = lineOf(manifest, "resident \"fennel\"");
    manifest.erase(manifest.find(fennel), fennel.size() + 1);
    if (const auto story = manifest.find("story \"fennel\""); story != std::string::npos)
        manifest.erase(story, manifest.find('\n', story) - story + 1);
    World after;
    expect(after.loadWorldFiles(files, "grown").ok, "The rebuilt town loads");
    const auto restored = after.restore(saved);
    expect(restored.ok, "The old checkpoint restores into the rebuilt town: " + restored.message + " " + restored.targetId);
    expect(after.society().account("sorrel")->cash == sorrelCash, "A resident who remains keeps their money");
    expect(after.society().resident("newbie") && after.entity("newbie"), "The newcomer is in the world");
    expect(!after.society().resident("fennel") && !after.society().account("fennel") && !after.entity("fennel"),
           "The departed resident is gone");
    expect(after.society().account("treasury")->cash == treasury + fennelCash,
           "The departed resident's coins return to the treasury");
    expect(after.society().conserved(), "Money stays conserved across the rebuild");
    run(after, 30);
}

int main()
{
    try
    {
        wanderAreas();
        factionClaims();
        residentsChangeLive();
        deadNpcsRest();
        loadsFromMemory();
        routesCrossDoors();
        travellersWalkTheirRoute();
        checkpointsSurviveAChangedPopulation();
        layout();
        playerServices();
        dailyLife();
        wagesAndFood();
        rejectsBadResidents();
        doorsWalkThrough();
        lockedDoorsStillBlock();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Town tests passed: " << checks << " checks.\n";
    return 0;
}
