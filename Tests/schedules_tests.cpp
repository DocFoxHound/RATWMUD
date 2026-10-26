// Schedules (Core/RatwSchedules.h; Docs/Design/26-living-npcs.md, Phase 9), in Greyfen: the week, Marketday stalls,
// Restday, festivals (seasonal and called), and the weather changing plans.
#include "RatwCheckpoint.h"
#include "RatwWorld.h"

#include <cmath>
#include <sstream>
#include <map>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <set>
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
// Greyfen on a given calendar day and hour, under a sky of our choosing.
// Market stalls built in the street before Greyfen's shop (Docs/Design/39: only a place with stalls built has a
// market): on open ground a few strides either side of where the shop's door lets out.
void buildStalls(World& w)
{
    for (const auto& [id, d] : w.doors())
        if (d.cellId == "shop" && d.targetCell == "town")
        {
            auto* town = w.cell("town");
            int built = 0;
            for (int dx : {-3, 3, -5, 5, -4, 4})
                for (int dy : {-2, 2, -3, 3})
                {
                    auto* t = town->tile(int(std::floor(d.arrival.x)) + dx, int(std::floor(d.arrival.y)) + dy);
                    if (built < 3 && t && !t->solid && t->glyph != '+')
                    {
                        t->glyph = 'u';
                        t->solid = true;
                        ++built;
                    }
                }
            return;
        }
}
World at(int day, double hour, Weather sky = Weather::Clear, bool stalls = true)
{
    World w;
    const auto loaded = w.loadWorldFile(RATW_SOURCE_DIR "/Data/Worlds/Greyfen/world.ratw");
    expect(loaded.ok, "Town loads: " + loaded.message);
    if (stalls)
        buildStalls(w);
    expect(w.advanceCalendar(day - std::floor(w.calendarDays())).ok, "The calendar moves on");
    expect(w.setTimeOfDay(hour).ok, "The hour is set");
    for (const auto& [id, c] : w.cells())
        if (c.outdoors)
            w.setWeather(id, sky);
    return w;
}
void run(World& w, int seconds)
{
    for (int s = 0; s < seconds; ++s)
    {
        w.tick(1);
        expect(w.society().conserved(), "Money stays conserved");
    }
}
const ResidentLife& life(const World& w, const std::string& id) { return *w.society().resident(id); }
std::vector<std::string> civilians(const World& w)
{
    std::vector<std::string> out;
    for (const auto& [id, r] : w.society().state().residents)
        if (r.role == "civilian")
            out.push_back(id);
    return out;
}
int meals(const World& w, const std::string& id) { return Society::stock(*w.society().account(id), "meal"); }

void theWeek()
{
    expect(calendar::weekdayOf(0) == 0 && calendar::weekdayName(0) == "Dawnday", "Year 1 begins on a Dawnday");
    expect(calendar::weekdayName(calendar::weekdayOf(5.5)) == "Marketday" && calendar::weekdayOf(6.2) == calendar::Restday &&
               calendar::weekdayOf(7) == 0,
           "Marketday, Restday, and round again");
    expect(calendar::festivalDay(45.5) && !calendar::festivalDay(44.5) && calendar::festivalDay(92 + 45.1),
           "A festival on the 46th of every season");
    auto w = at(1, 11);
    expect(w.dayPlan("greyfen").kind == "work" && w.dayLabel("town") == "Hearthday", "An ordinary Hearthday");
    expect(w.festivalName("greyfen", 2) == w.festivalName("greyfen", 2) && !w.festivalName("greyfen", 2).empty(),
           "A town's festival names are its own, and stay so");
}


// Greyfen's files, to change before loading.
std::map<std::string, std::string> greyfenFiles()
{
    namespace fs = std::filesystem;
    std::map<std::string, std::string> files;
    const fs::path root = RATW_SOURCE_DIR "/Data/Worlds/Greyfen";
    for (const auto& f : fs::recursive_directory_iterator(root))
        if (f.is_regular_file() && (f.path().extension() == ".ratw" || f.path().extension() == ".cell"))
        {
            std::ifstream in(f.path(), std::ios::binary);
            std::ostringstream text;
            text << in.rdbuf();
            files[fs::relative(f.path(), root).generic_string()] = text.str();
        }
    return files;
}

// Restday's service (Docs/Design/42-money-in-circulation.md, Phase 6): Greyfen with Maple keeping a chapel in the Long
// House. From nine to eleven a third of the town sits there while she preaches; the plate goes round at the end.
void restdayService()
{
    auto files = greyfenFiles();
    auto& manifest = files["world.ratw"];
    const auto line = manifest.find("resident \"maple\"");
    expect(line != std::string::npos, "Maple is in Greyfen");
    const auto end = manifest.find('\n', line);
    auto maple = manifest.substr(line, end - line);
    maple.replace(maple.find("\"telling stories by the fountain\""), 33, "\"keeps the chapel\"");
    const auto work = maple.find("\"town\" 33.5 20.5");
    maple.replace(work, 17, "\"longhouse\" 17.5 9.5");
    manifest.replace(line, end - line, maple);
    World w;
    const auto loaded = w.loadWorldFiles(files, "greyfen");
    expect(loaded.ok, "Greyfen with a chapel loads: " + loaded.message);
    expect(w.advanceCalendar(6 - std::floor(w.calendarDays())).ok && w.setTimeOfDay(9.2).ok, "Restday morning");
    run(w, 400);
    expect(life(w, "maple").task == "preaching", "Maple preaches (" + life(w, "maple").task + ")");
    int there = 0, folk = 0;
    for (const auto& [id, r] : w.society().state().residents)
    {
        folk += r.role != "guard";
        there += r.task == "at church";
    }
    expect(there >= 1 && there < folk, "Some of the town is at church, not all (" + std::to_string(there) + " of " + std::to_string(folk) + ")");
    expect(w.setTimeOfDay(10.6).ok, "The service ends");
    w.takeEvents();
    run(w, 200);
    bool plate = false;
    for (const auto& e : w.takeEvents())
        plate |= e.kind == "economy" && e.detail == "the collection";
    expect(plate, "and the plate goes round");
    expect(w.setTimeOfDay(12).ok, "Noon");
    run(w, 30);
    expect(life(w, "maple").task != "preaching", "The service is over by noon");
}

// The land's food on Restday (doc 64, Phase 13): Greyfen with Rook keeping rabbits. Through the service it rests like
// everyone; after it, it works the afternoon, as the food shops do, so Dawnday's makers have meat.
void restdayLandFood()
{
    auto files = greyfenFiles();
    auto& manifest = files["world.ratw"];
    const std::string hauling = "\"hauling crates on the high street\"", rabbits = "\"keeps rabbits on the high street\"";
    const auto label = manifest.find(hauling);
    expect(label != std::string::npos, "Rook hauls crates in Greyfen");
    manifest.replace(label, hauling.size(), rabbits);
    World w;
    const auto loaded = w.loadWorldFiles(files, "greyfen");
    expect(loaded.ok, "Greyfen with a rabbit keeper loads: " + loaded.message);
    expect(w.advanceCalendar(6 - std::floor(w.calendarDays())).ok && w.setTimeOfDay(10).ok, "Restday, during the service");
    run(w, 5);
    expect(life(w, "rook").task != "keeps rabbits on the high street", "Rook doesn't work through the service: " + life(w, "rook").task);
    expect(w.setTimeOfDay(13).ok, "The afternoon");
    run(w, 5);
    expect(life(w, "rook").task == "keeps rabbits on the high street", "Rook keeps its rabbits after it: " + life(w, "rook").task);
    expect(life(w, "sorrel").task != "tending the herb garden", "while the gardener rests: " + life(w, "sorrel").task);
}

void restday()
{
    auto w = at(6, 11);
    run(w, 5);
    for (const auto& id : civilians(w))
        expect(life(w, id).task == "resting" || life(w, id).task == "eat" || life(w, id).task == "buy food" ||
                   life(w, id).task == "sleep",
               id + " rests on Restday: " + life(w, id).task);
    expect(life(w, "sloe").task == "patrol", "The watch keeps its hours");
    expect(life(w, "wren").task == "trade", "The shop opens after the morning service, so everyone can eat (doc 42)");
    auto later = at(6, 16);
    run(later, 5);
    expect(later.society().resident("wren")->task != "trade", "and shuts mid-afternoon");
}

void marketday()
{
    // A town with no stalls built has no market: its Marketday is an ordinary working day.
    auto bare = at(5, 9, Weather::Clear, false);
    expect(bare.dayPlan("greyfen").kind == "work" && bare.dayPlan("greyfen").stalls.empty(), "No stalls, no market day");
    auto w = at(5, 9);
    const auto plan = w.dayPlan("greyfen");
    expect(plan.kind == "market" && !plan.stalls.empty() && plan.crowd.size() > 10, "Marketday: stalls and room for a crowd");
    expect(plan.stalls[0].cell == "town", "The market spills out of the shop into the street");
    run(w, 5);
    expect(life(w, "wren").task == "trade" && life(w, "wren").goalCell == "town", "Wren trades from a stall");
    for (int s = 0; s < 600 && !w.society().atStall("wren"); ++s)
        w.tick(1);
    expect(w.society().atStall("wren"), "and sets it up");
    w.addPlayer("player-ada", "Ada");
    const auto cheaper = w.society().quote("player-ada", "wren", "meal", 1, true);
    auto shop = at(4, 9);
    run(shop, 200);
    shop.addPlayer("player-ada", "Ada");
    const auto usual = shop.society().quote("player-ada", "wren", "meal", 1, true);
    expect(cheaper.unitPrice < usual.unitPrice || (cheaper.unitPrice == usual.unitPrice && cheaper.unitPrice <= 1), "Stall prices are a little lower: " +
           std::to_string(cheaper.unitPrice) + " against " + std::to_string(usual.unitPrice));
    // Each of the townsfolk goes for an hour, some time between eight and one.
    std::set<std::string> went;
    for (double hour = 8.5; hour < 13; hour += 1)
    {
        auto h = at(5, hour);
        run(h, 3);
        for (const auto& id : civilians(h))
            if (life(h, id).task == "at the market")
                went.insert(id);
    }
    expect(went.size() >= civilians(w).size() - 1, "Nearly everyone goes to the market (" + std::to_string(went.size()) + " of " +
           std::to_string(civilians(w).size()) + "; the hungry eat first)");
    auto foul = at(5, 9, Weather::Storm);
    expect(foul.dayPlan("greyfen").foul, "In a storm");
    run(foul, 5);
    expect(life(foul, "wren").goalCell == "shop", "the stalls don't go out");
}

// A city of several markets (Docs/Design/60-the-new-map.md): Greyfen with Aster keeping stalls of her own at the far
// east end of the street, beyond the reach of Brann's square by the shop. Each keeps its own Marketday: the shopkeeper
// sets up at the one nearest her shop, and the townsfolk go to the one nearest home.
void cityMarkets()
{
    auto files = greyfenFiles();
    auto& manifest = files["world.ratw"];
    const auto line = manifest.find("resident \"brann\"");
    expect(line != std::string::npos, "Brann is in Greyfen");
    auto aster = manifest.substr(line, manifest.find('\n', line) + 1 - line);
    aster.replace(0, std::string("resident \"brann\" \"Brann\"").size(), "resident \"aster\" \"Aster\"");
    for (const auto& [from, to] : {std::pair{"\"longhouse\" 21.5 9.5", "\"longhouse\" 18.5 9.5"},
                                   std::pair{"\"town\" 20.5 17.5", "\"town\" 61.5 37.5"}})
        aster.replace(aster.find(from), std::string(from).size(), to);
    manifest.insert(line, aster);                   // (First of the merchants in the street: hers is the town's square.)
    World w;
    const auto loaded = w.loadWorldFiles(files, "greyfen");
    expect(loaded.ok, "Greyfen with two markets loads: " + loaded.message);
    buildStalls(w);
    for (const auto [x, y] : {std::pair{62, 36}, std::pair{62, 38}})
    {
        auto* t = w.cell("town")->tile(x, y);
        t->glyph = 'u';
        t->solid = true;
    }
    expect(w.advanceCalendar(5 - std::floor(w.calendarDays())).ok && w.setTimeOfDay(9).ok, "Marketday morning");
    const auto plan = w.dayPlan("greyfen");
    expect(plan.kind == "market" && !plan.stalls.empty(), "Marketday at Aster's square");
    for (const auto& s : plan.stalls)
        expect(s.x > 55, "its stalls are hers, at the east end");
    const Vec2 east{61.5, 37.5}, west{20.5, 17.5};
    const auto near = [&](const Spot& s, Vec2 m) { return std::hypot(s.x - m.x, s.y - m.y); };
    run(w, 5);
    expect(life(w, "wren").task == "trade" && life(w, "wren").goalCell == "town" && life(w, "wren").goalX < 30,
           "Wren sets up at the market nearest her shop, by Brann (" + std::to_string(life(w, "wren").goalX) + ")");
    // Each of the townsfolk goes to the one nearest home (by its door into the street).
    const auto doorOf = [&](const std::string& home) {
        for (const auto& [id, d] : w.doors())
            if (d.cellId == "town" && d.targetCell == home)
                return Spot{"town", d.position.x, d.position.y};
        return Spot{"town", -1000, -1000};
    };
    std::set<std::string> sides;
    for (double hour = 8.5; hour < 13; hour += 1)
    {
        auto h = World();
        expect(h.loadWorldFiles(files, "greyfen").ok, "loads again");
        buildStalls(h);
        for (const auto [x, y] : {std::pair{62, 36}, std::pair{62, 38}})
        {
            auto* t = h.cell("town")->tile(x, y);
            t->glyph = 'u';
            t->solid = true;
        }
        expect(h.advanceCalendar(5 - std::floor(h.calendarDays())).ok && h.setTimeOfDay(hour).ok, "the hour is set");
        run(h, 3);
        for (const auto& id : civilians(h))
        {
            const auto& r = life(h, id);
            if (r.task != "at the market")
                continue;
            const Spot goal{r.goalCell, r.goalX, r.goalY}, home = doorOf(r.homeCell);
            const bool eastward = near(home, east) < near(home, west);
            sides.insert(eastward ? "east" : "west");
            expect(goal.cell == "town" && (near(goal, east) < near(goal, west)) == eastward,
                   id + " goes to the market nearest home (" + std::to_string(goal.x) + ", " + std::to_string(goal.y) + ")");
        }
    }
    expect(sides.size() == 2, "Both markets have their crowd");
}

void festivals()
{
    auto w = at(45, 13);
    const auto plan = w.dayPlan("greyfen");
    expect(plan.kind == "festival" && plan.name == w.festivalName("greyfen", 0), "Spring 46 is the town's spring festival");
    expect(w.dayLabel("town") == calendar::weekdayName(calendar::weekdayOf(45)) + " · " + plan.name, "and says so");
    run(w, 5);
    int gathering = 0;
    for (const auto& id : civilians(w))
        gathering += life(w, id).task == "festival";
    expect(gathering >= int(civilians(w).size()) - 1, "The town gathers at the market from noon");
    expect(life(w, "sloe").task == "patrol", "The watch on duty stays at it");
    std::map<std::string, int> before;
    for (const auto& id : civilians(w))
        before[id] = meals(w, id);
    run(w, 400);
    int fed = 0;
    for (const auto& id : civilians(w))
        fed += meals(w, id) > before[id] || life(w, id).hunger < 30;
    expect(fed >= 1, "The town's stores feed those who come");
    // The Dungeon Master calls one on an ordinary day.
    auto called = at(2, 13);
    const auto result = called.callFestival("greyfen", "The Lantern Night");
    expect(result.ok, "A festival is called: " + result.message);
    expect(called.dayPlan("greyfen").kind == "festival" && called.dayPlan("greyfen").name == "The Lantern Night", "for today");
    expect(!called.callFestival("nowhere", "").ok && !called.callFestival("greyfen", "", 31).ok, "Only for a town, and soon");
    run(called, 5);
    expect(life(called, civilians(called)[0]).reason.find("The Lantern Night") != std::string::npos, "and kept");
    const auto saved = called.save();
    checkpoint::ServerState server;
    std::vector<Entity> npcs(saved.npcs.begin(), saved.npcs.end());
    json::Value parsed;
    std::string error;
    expect(json::parse(json::dump(checkpoint::encode(saved, server, npcs, called.time())), parsed, error), "Saved: " + error);
    PersistedWorld back;
    checkpoint::ServerState serverBack;
    expect(checkpoint::decode(parsed, back, serverBack, error), "Read back: " + error);
    expect(back.festivals.size() == 1 && back.festivals[0].name == "The Lantern Night", "A called festival is kept in the save");
    auto again = at(2, 13);
    expect(again.restore(back).ok && again.dayPlan("greyfen").name == "The Lantern Night", "and holds after a restart");
    auto rained = at(45, 13, Weather::Storm);
    run(rained, 5);
    expect(life(rained, civilians(rained)[0]).task == "at home", "A storm keeps the festival indoors");
}

void weather()
{
    // Who works outdoors in Greyfen, and where their evenings are.
    auto clear = at(1, 11);
    std::string outdoor;
    for (const auto& id : civilians(clear))
        if (const auto* spec = clear.society().spec(id); spec && clear.cell(spec->work.cell)->outdoors)
            outdoor = id;
    expect(!outdoor.empty(), "Someone in Greyfen works outdoors");
    const auto* spec = clear.society().spec(outdoor);
    auto storm = at(1, 11, Weather::Storm);
    run(storm, 5);
    expect(life(storm, outdoor).task == "sheltering", outdoor + " shelters from the storm: " + life(storm, outdoor).task);
    expect(life(storm, "sloe").task == "patrol", "The watch works in all weathers");
    auto rain = at(1, spec->endHour - 1, Weather::Rain);
    run(rain, 5);
    expect(life(rain, outdoor).task == "at home", "Rain ends outdoor work early: " + life(rain, outdoor).task);
    auto dry = at(1, spec->endHour - 1);
    run(dry, 5);
    expect(life(dry, outdoor).task == spec->workLabel || life(dry, outdoor).task != "at home", "Dry, they work on");
}
} // namespace

int main()
{
    try
    {
        theWeek();
        restday();
        restdayService();
        restdayLandFood();
        marketday();
        cityMarkets();
        festivals();
        weather();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "schedules tests passed (" << checks << " checks)\n";
    return 0;
}
