// The port (Docs/Design/79-a-staged-economy.md, 8.3, 8.6, D8; Phase 5): ships as a committed timetable, never seen. A port
// is a market with posts of the Wharf's job words; its ships are rolled a week ahead and kept on the Market Board, so they
// are saved, journalled and restored with it (file and Postgres: Tests/port_pg.py); on its day a ship docks (its cargo
// cheaper toward the ship baseline, never dearer; what it takes dearer; the port's ship goods refilled; the Wharf's posts
// "unloading the Gull", the Wharf a crowd) and sails that evening with the shops' gluts of what it takes; a storm keeps
// one out (+0.1, a trouble, "No ship this week"); the timetable is "the Wharf office"'s notice and talk's world.ships. A
// world with no port has none of it and refills each morning; a port deleted loses its ships and its notice; shipsPerWeek
// 0 stops them. Players paid by the port's shops on ship day stay under the caps, real money balanced. The catalogue is
// the real Data (RATW_SOURCE_DIR). The harness is trade_show_tests'.
#include "test_purse.h"
#include "RatwCalendar.h"
#include "RatwCheckpoint.h"
#include "RatwGame.h"
#include "RatwItems.h"
#include "RatwJournal.h"
#include "RatwMarket.h"
#include "RatwWorld.h"
#include "battle_play.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <map>
#include <set>
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

constexpr int Side = 16;
std::map<int, std::string> named;   // Cells given a map's own ids (a farm's work room).
std::string id(int i)
{
    const auto found = named.find(i);
    return found != named.end() ? found->second : "c_" + std::to_string(i) + "_0";
}

struct Fixture
{
    std::string manifest;
    std::map<std::string, std::string> cells, seams;
    World::CellSource source()
    {
        return {[this](const std::string& cell, std::string& header) {
                    const auto found = cells.find(cell);
                    if (found == cells.end()) return std::string("no such cell");
                    header = found->second.substr(0, found->second.find("grid:") + 5);
                    return std::string();
                },
                [this](const std::string& cell, std::string& text, std::string& sides) {
                    const auto found = cells.find(cell);
                    if (found == cells.end()) return std::string("no such cell");
                    text = found->second;
                    sides = seams[cell];
                    return std::string();
                }};
    }
};

std::string resident(const std::string& who, const std::string& name, const std::string& role, const std::string& work,
                     int home, double hx, int at, double wx)
{
    std::ostringstream r;
    r << "resident \"" << who << "\" \"" << name << "\" \"" << role << "\" \"" << work << "\" \"Someone.\" \"Hello.\" 30 \"timber\" "
      << "\"male\" \"average\" \"saddle\" 3 1 5 1 1 8 17 \"-\" 40 0 1 \"" << id(home) << "\" " << hx << " 4.5 \"" << id(at)
      << "\" " << wx << " 8.5 \"" << id(home) << "\" " << hx << " 5.5\n";
    return r.str();
}

// A strip of cells, one letter each: 'E' the city "east" (a food stall, a smith, its carters), 'W' the town "west" (a food
// stall and a general store), 'G' a farm's work room in wild country, '.' wild country. Each place's first cell is where
// its people live, its second its market.
Fixture strip(const std::string& layout = "EE.....WW", const std::string& extra = "", bool carters = true)
{
    Fixture f;
    const int cells = int(layout.size());
    std::ostringstream m;
    m << "RATW_WORLD 3\n";
    int seam = 0;
    for (int i = 0; i < cells; ++i)
    {
        std::ostringstream cell;
        cell << "id: " << id(i) << "\nname: Stretch " << i << "\ndescription: Open ground.\nworld: " << i * Side
             << " 0 0\noutdoors: true\nweather: clear\nsize: " << Side << ' ' << Side << "\ngrid:\n";
        for (int y = 0; y < Side; ++y) cell << std::string(Side, '.') << '\n';
        f.cells[id(i)] = cell.str();
        m << "area \"" << id(i) << "\"\n";
        if (i + 1 < cells)
            for (int k = 0; k < Side; ++k)
            {
                const std::string a = "seam_" + std::to_string(seam) + "_a", b = "seam_" + std::to_string(seam) + "_b";
                std::ostringstream one, two;
                one << "door \"" << a << "\" \"Open boundary\" \"" << id(i) << "\" " << Side - .5 << ' ' << k + .5 << " \"" << id(i + 1)
                    << "\" .5 " << k + .5 << " \"" << b << "\" 1 0 1 1 \"E\"\n";
                two << "door \"" << b << "\" \"Open boundary\" \"" << id(i + 1) << "\" .5 " << k + .5 << " \"" << id(i) << "\" "
                    << Side - .5 << ' ' << k + .5 << " \"" << a << "\" 1 0 1 1 \"W\"\n";
                f.seams[id(i)] += one.str();
                f.seams[id(i + 1)] += two.str();
                ++seam;
            }
    }
    const auto region = [](char c) { return c == 'E' ? "east" : c == 'W' ? "west" : c == 'N' ? "north" : "wilds"; };
    for (int i = 0; i < cells; ++i)
    {
        m << "exits \"" << id(i) << "\" " << ((i > 0) + (i + 1 < cells));
        if (i > 0) m << " \"" << id(i - 1) << '"';
        if (i + 1 < cells) m << " \"" << id(i + 1) << '"';
        m << '\n';
        m << "territory \"" << id(i) << "\" \"" << region(layout[std::size_t(i)]) << "\" \"-\" 0\n";
    }
    m << "spawn \"" << id(0) << "\" 8.5 8.5\n";
    m << "economy 20000 100 50 10 12\n";
    const auto first = [&](char c) { return int(layout.find(c)); };
    if (const int e = first('E'); e >= 0)
    {
        m << resident("em", "Ember Oak", "merchant", "keeping the stall", e, 2.5, e + 1, 8.5);
        m << resident("es", "Ivo Anvil", "merchant", "smith at The Gate Forge", e, 3.5, e + 1, 10.5);
        if (carters)
        {
            m << resident("c1", "Tam Reed", "civilian", "driving for the carters", e, 4.5, e + 1, 4.5);
            m << resident("c2", "Hob Lane", "civilian", "driving for the carters", e, 5.5, e + 1, 5.5);
        }
        for (int n = 1; n <= 5; ++n)
            m << resident("e" + std::to_string(n), "East " + std::to_string(n), "civilian", "working", e, 6.5 + n, e, 6.5 + n);
    }
    if (const int w = first('W'); w >= 0)
    {
        m << resident("wm", "Wren Reed", "merchant", "keeping the stall", w, 2.5, w + 1, 8.5);
        m << resident("wg", "Gil Moss", "merchant", "shopkeeper at The West Store", w, 3.5, w + 1, 10.5);
        for (int n = 1; n <= 5; ++n)
            m << resident("w" + std::to_string(n), "West " + std::to_string(n), "civilian", "working", w, 4.5 + n, w, 4.5 + n);
    }
    // 'N': a hamlet "north", a market (its stall) but too few homes to be a town for the law (D7: plain).
    if (const int n = first('N'); n >= 0)
    {
        m << resident("nm", "Nell Ash", "merchant", "keeping the stall", n, 2.5, n + 1, 8.5);
        m << resident("n1", "North 1", "civilian", "working", n, 4.5, n, 4.5);
    }
    m << extra;
    f.manifest = m.str();
    return f;
}

World load(Fixture& f)
{
    World world;
    world.setCellSource(f.source());
    const auto loaded = world.loadWorldFiles({{"world.ratw", f.manifest}}, "strip");
    expect(loaded.ok, "The strip loads: " + loaded.message);
    return world;
}

int stock(const World& w, const std::string& who, const std::string& item)
{
    const auto* a = w.society().account(w.society().tillOf(who));
    return a ? Society::stock(*a, item) : 0;
}

void run(World& w, double seconds)
{
    for (double t = 0; t < seconds; t += 1)
        w.tick(1);
}

// The next day, early (as a restart would find it): the roads' day turns at the first tick.
void nextMorning(World& w)
{
    auto saved = w.save();
    saved.calendarDays = std::floor(saved.calendarDays) + 1.2;
    expect(w.restore(saved).ok, "A day passes");
    w.tick(.6);
}

std::string lowered(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// A player's connection to a game, for the boards (as town_fixture.h's).
json::Value parsed(const std::string& text)
{
    json::Value v;
    std::string error;
    json::parse(text, v, error);
    return v;
}
struct Client final : game::Connection
{
    std::vector<json::Value> events;
    void event(const std::string& text) override { events.push_back(parsed(text)); }
    void snapshot(const std::string&) override {}
    void motion(const json::Value&) override {}
    bool allowsLocalCredentials() const override { return true; }
    const json::Value* last(const std::string& type) const
    {
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (it->string("type") == type)
                return &*it;
        return nullptr;
    }
    std::string said() const
    {
        std::string out;
        for (const auto& e : events)
            if (e.string("type") == "system")
                out += e.string("text") + "\n";
        return out;
    }
};
std::string cmd(std::initializer_list<std::pair<const char*, json::Value>> fields)
{
    auto o = json::Value::object();
    for (const auto& [k, v] : fields)
        o.add(k, v);
    return json::dump(o);
}

// The strip as a world on disk (cells, seams, the manifest), for a game to start from; and a game's options on it.
std::filesystem::path onDisk(Fixture& f, const std::string& name)
{
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("ratw-s2stage-" + name + "-" + std::to_string(::getpid()));
    fs::remove_all(root);
    fs::create_directories(root / "cells");
    fs::create_directories(root / "seams");
    for (const auto& [cellId, text] : f.cells)
    {
        std::ofstream(root / "cells" / (cellId + ".cell")) << text;
        std::ofstream(root / "seams" / cellId) << f.seams[cellId];
    }
    std::ofstream(root / "world.ratw") << f.manifest;
    return root;
}
game::Options optionsOn(const std::filesystem::path& root, const std::string& save = {})
{
    game::Options o;
    o.forkSnapshots = false;
    o.tiesOptional = true;
    o.hiddenNames = false;
    o.devIdentity = true;
    o.worldExport = root.string();
    o.savePath = save;
    return o;
}
// A player at a community's notice board, and what a board command tells them.
void enterAt(game::Game& g, Client& c, int n, const char* who, const std::string& community)
{
    c.id = std::uint64_t(n);
    g.connect(&c);
    g.command(&c, cmd({{"type", "hello"}, {"id", who}, {"name", who}}));
    testPurse(g.world().society(), c.entityId, 50);
    const auto* spot = g.world().boardSpot(community);
    expect(spot != nullptr, community + " has a board");
    auto* e = g.world().entity(c.entityId);
    expect(e != nullptr, std::string(who) + " is in the world");
    e->cellId = spot->cell;
    e->position = {spot->x + .5, spot->y};
}
std::string board(game::Game& g, Client& c, std::initializer_list<std::pair<const char*, json::Value>> more)
{
    auto o = json::Value::object();
    o.add("type", "board");
    for (const auto& [k, v] : more)
        o.add(k, v);
    c.events.clear();
    g.command(&c, json::dump(o));
    return c.said();
}

// ---------------------------------------------------------------------------------------------------------------- Tests

const char* const WharfPosts[] = {"\"d1\"", "\"d2\"", "\"d3\"", "\"hm\""};

// The strip with a port: east's third cell its Wharf (its sheds: indoors, so no weather stops the work), four posts of
// the Wharf's job words there (two that haul, a tally clerk, the harbourmaster), and three more shops at east's market
// (an inn, a general store, a tannery). `docks` false: the same world with no Wharf post (a host deleted the port).
Fixture harbour(bool carters = true, bool docks = true)
{
    std::string extra = resident("ek", "Kit Vane", "merchant", "innkeeper at The Anchor", 0, 2.5, 1, 12.5) +
                        resident("eg", "Gus Vane", "merchant", "shopkeeper at The East Store", 0, 3.5, 1, 13.5) +
                        resident("et", "Tess Vane", "merchant", "tanner at The Tannery", 0, 3.5, 1, 14.5);
    if (docks)
        extra += resident("d1", "Dock One", "civilian", "unloading ships at the Wharf", 0, 12.5, 2, 4.5) +
                 resident("d2", "Dock Two", "civilian", "working the cranes at the Wharf", 0, 13.5, 2, 5.5) +
                 resident("d3", "Tally Three", "civilian", "tallying in the long store", 0, 14.5, 2, 6.5) +
                 resident("hm", "Hale Marsh", "civilian", "harbourmaster", 0, 1.5, 2, 7.5);
    auto f = strip("EEE....WW", extra, carters);
    auto& sheds = f.cells[id(2)];
    sheds.replace(sheds.find("outdoors: true"), std::string("outdoors: true").size(), "outdoors: false");
    return f;
}

std::string shipsOf(const World& w)
{
    const auto j = market::encode(w.society().board());
    return json::dump(j["ships"]) + json::dump(j["shipsThrough"]) + json::dump(j["nextShip"]);
}

std::int64_t today(const World& w)
{
    return std::int64_t(std::floor(w.calendarDays()));
}

// Mornings on until a ship is due that day at the port (never a stormbound one: those are "storm" by then): a copy.
market::Ship toShipDay(World& w, const std::string& port = "east", std::vector<WorldEvent>* events = nullptr)
{
    for (int day = 0; day < 40; ++day)
    {
        for (const auto& s : w.society().board().ships)
            if (s.port == port && s.status == "due" && s.day == today(w))
                return s;
        nextMorning(w);
        for (auto& e : w.takeEvents())
            if (events)
                events->push_back(e);
    }
    expect(false, "a ship comes within forty days");
    return {};
}

const market::Ship* shipById(const World& w, const std::string& shipId)
{
    for (const auto& s : w.society().board().ships)
        if (s.id == shipId)
            return &s;
    return nullptr;
}

// To a little after its hour: it docks.
void dock(World& w, const market::Ship& s)
{
    expect(w.setTimeOfDay(s.hour + .1).ok, "to the ship's hour");
    for (int t = 0; t < 4; ++t)
        w.tick(.5);
    const auto* in = w.shipIn(s.port);
    expect(in && in->id == s.id, s.name + " is in at its hour");
}

int count(const std::vector<WorldEvent>& events, const std::string& kind, const std::string& actor = {})
{
    return int(std::count_if(events.begin(), events.end(), [&](const WorldEvent& e) { return e.kind == kind && (actor.empty() || e.actor == actor); }));
}

bool hasWord(const std::string& text, const std::string& word)
{
    return lowered(text).find(lowered(word)) != std::string::npos;
}

// A port is a market with the Wharf's posts (8.3); its ships are committed a week ahead, seeded, and kept as posted.
void aPortAndItsTimetable()
{
    auto f = harbour();
    auto w = load(f);
    w.tick(.6);
    expect(w.ports().size() == 1 && w.port("east") && !w.port("west"), "east is a port, west is none");
    const auto port = *w.port("east");
    expect(port.posts == 4 && port.held == 4 && port.cell == id(2) && port.cells.count(id(2)),
           "its four posts, held, at the Wharf: " + std::to_string(port.posts) + "/" + std::to_string(port.held) + " at " + port.cell);
    const auto& data = market::data();
    expect(data.shipBrings.size() == 12 && data.shipTakes.size() == 13 && data.shipNames.size() >= 8,
           "market.json's wharf block: the 12 imports, the 13 takes, the ships' names: " + std::to_string(data.shipBrings.size()) + " " +
               std::to_string(data.shipTakes.size()) + " " + std::to_string(data.shipNames.size()));
    expect(std::find(data.shipBrings.begin(), data.shipBrings.end(), "cheese") == data.shipBrings.end(), "never cheese (the wharf's `never`)");
    for (const auto& name : data.shipNames)
        expect(!hasWord(name, "dog") && name.rfind("the ", 0) == 0, "a ship's name: " + name);
    expect(!port.imports.empty(), "its shops set out goods that wait for ships");
    for (const auto& g : port.imports)
        expect(std::find(data.shipBrings.begin(), data.shipBrings.end(), g) != data.shipBrings.end() && w.society().imported("east", g),
               g + " is one of the wharf's imports, and waits for a ship at east");
    expect(!w.society().imported("west", port.imports.front()) || w.society().tradePoints().find("west") != nullptr,
           "west's own goods are its own matter");
    // Three weeks of mornings: every ship as it was first posted, posted a day ahead at least, never on Restday, the
    // week's count the rule's (1-3 at the rule's 2), no two of a name on the timetable at once.
    std::map<std::string, market::Ship> first;
    std::map<std::string, std::int64_t> seenOn;
    std::map<std::int64_t, int> perWeek;
    const auto startWeek = today(w) / 7;
    for (int day = 0; day < 21; ++day)
    {
        const auto& b = w.society().board();
        expect(b.ships.size() <= market::ShipsAPort, "the timetable bounded");
        expect(b.shipsThrough.count("east") && b.shipsThrough.at("east") == today(w) / 7 + 1, "rolled through next week");
        std::set<std::string> names;
        for (const auto& s : b.ships)
        {
            expect(s.port == "east" && names.insert(s.name).second, "one ship of a name on the timetable: " + s.name);
            expect(std::find(data.shipNames.begin(), data.shipNames.end(), s.name) != data.shipNames.end(), "a name from market.json: " + s.name);
            const auto [slot, fresh] = first.try_emplace(s.id, s);
            if (fresh)
            {
                seenOn[s.id] = today(w);
                ++perWeek[s.day / 7];
                expect(s.day > today(w), s.name + " posted a day ahead at least");
                expect(s.status == "due" && s.hour >= 7 && s.hour <= 10 && s.sails > s.hour + 8 && s.sails <= 19.5,
                       "in during the morning, out by the evening: " + std::to_string(s.hour) + " " + std::to_string(s.sails));
                expect(calendar::weekdayOf(double(s.day)) != calendar::Restday, "never on Restday");
                expect(!s.brings.empty() && int(s.brings.size()) <= 4 && int(s.takes.size()) <= 3, "a short cargo each way");
                for (const auto& [g, n] : s.brings)
                    expect(std::find(port.imports.begin(), port.imports.end(), g) != port.imports.end() && n >= 1,
                           "it brings what the port's shops wait for: " + g);
                for (const auto& [g, n] : s.takes)
                    expect(std::find(data.shipTakes.begin(), data.shipTakes.end(), g) != data.shipTakes.end() && n >= 1,
                           "it takes from the wharf's `takes`: " + g);
            }
            else
                expect(slot->second.day == s.day && slot->second.hour == s.hour && slot->second.sails == s.sails &&
                           slot->second.name == s.name && slot->second.brings == s.brings && slot->second.takes == s.takes,
                       s.name + " stays as it was posted");
        }
        const auto text = w.shipsNotice("east");
        expect(text.size() <= 280 && !hasWord(text, "dog") && (b.ships.empty() || text.rfind("Ships at East.", 0) == 0), "the timetable in words: " + text);
        nextMorning(w);
    }
    expect(first.size() >= 3, "ships came: " + std::to_string(first.size()));
    for (const auto& [week, n] : perWeek)
        if (week > startWeek && week <= startWeek + 2)
            expect(n == w.shipsInWeek("east", week) && n >= 1 && n <= 3,
                   "week " + std::to_string(week) + ": " + std::to_string(n) + " ships, the rule's " + std::to_string(w.shipsInWeek("east", week)));
    // Seeded: the same world again shows the same ships.
    auto again = load(f);
    again.tick(.6);
    for (int day = 0; day < 21; ++day)
        nextMorning(again);
    expect(shipsOf(again) == shipsOf(w), "the same world, the same ships");
    // 1-3 a week at the rule's 2, fewer in the winter's storms than in summer; the rule's own number moves it; 0, none.
    std::map<calendar::Season, std::pair<int, int>> bySeason;
    for (std::int64_t week = 0; week < 400; ++week)
    {
        const int n = w.shipsInWeek("east", week);
        expect(n >= 1 && n <= 3, "1-3 ships a week");
        auto& [sum, weeks] = bySeason[calendar::calendarAt(double(week * 7) + 3.5).season];
        sum += n, ++weeks;
    }
    const auto mean = [&](calendar::Season s) { return double(bySeason[s].first) / std::max(1, bySeason[s].second); };
    expect(mean(calendar::Season::Winter) < mean(calendar::Season::Summer) - .3 && mean(calendar::Season::Winter) < 2,
           "fewer in winter: " + std::to_string(mean(calendar::Season::Winter)) + " vs summer's " + std::to_string(mean(calendar::Season::Summer)));
    expect(w.shipsInWeek("west", 3) == 0, "no port, no ships");
    auto spec = w.society().authored().economy;
    spec.rules["shipsPerWeek"] = 5;
    w.society().adoptEconomy(spec);
    expect(w.shipsInWeek("east", 3) >= 4 && w.shipsInWeek("east", 3) <= 6, "the DM's rule: about five");
    spec.rules["shipsPerWeek"] = 0;
    w.society().adoptEconomy(spec);
    expect(w.shipsInWeek("east", 3) == 0, "0: none");
    // The Board's ships round-trip, and a Board from before them reads as none.
    const auto& b = again.society().board();
    const auto back = market::decode(market::encode(b));
    expect(back.ships.size() == b.ships.size() && back.shipsThrough == b.shipsThrough && back.nextShip == b.nextShip, "the timetable round-trips");
    for (std::size_t i = 0; i < b.ships.size(); ++i)
        expect(back.ships[i].id == b.ships[i].id && back.ships[i].port == b.ships[i].port && back.ships[i].name == b.ships[i].name &&
                   back.ships[i].day == b.ships[i].day && back.ships[i].hour == b.ships[i].hour && back.ships[i].sails == b.ships[i].sails &&
                   back.ships[i].brings == b.ships[i].brings && back.ships[i].takes == b.ships[i].takes && back.ships[i].status == b.ships[i].status,
               "each ship the same, in order");
    auto old = market::encode(b);
    auto before = json::Value::object();
    for (const auto& [k, v] : old.fields())
        if (k != "ships" && k != "shipsThrough" && k != "nextShip")
            before.add(k, v);
    const auto plain = market::decode(before);
    expect(plain.ships.empty() && plain.shipsThrough.empty() && plain.nextShip == 1, "a Board saved before the ships: none");
    expect(market::Board().empty() && json::dump(market::encode(market::Board())).find("ships") == std::string::npos,
           "and a Board with no ships writes none");
}

// A restart shows the same ships (8.3: committed and saved): before the hour, after it (caught up, docked once), with the
// ship in; and a save from before the ships gets its timetable at once.
void theTimetableSurvivesARestart()
{
    auto f = harbour();
    auto w = load(f);
    w.tick(.6);
    const auto s = toShipDay(w);
    const auto before = shipsOf(w);
    const auto notice = w.shipsNotice("east");
    const auto saved = w.save();
    {
        auto v = load(f);
        expect(v.restore(saved).ok, "the save loads");
        expect(shipsOf(v) == before, "the same timetable after a restart");
        v.tick(.6);
        expect(shipsOf(v) == before && v.shipsNotice("east") == notice, "and after its first tick: " + v.shipsNotice("east"));
        // Both go on a week: still the same ships.
        auto u = load(f);
        expect(u.restore(saved).ok, "and again");
        for (int day = 0; day < 8; ++day)
        {
            nextMorning(v);
            nextMorning(u);
        }
        expect(shipsOf(v) == shipsOf(u) && !v.society().board().ships.empty(), "two restarts, a week on: the same ships");
    }
    {
        // The world was down over its hour: it docks at the first pass, once.
        auto late = saved;
        late.calendarDays = double(s.day) + (s.hour + 1) / 24;
        auto v = load(f);
        expect(v.restore(late).ok, "loaded after its hour");
        v.takeEvents();
        v.tick(.6);
        auto events = v.takeEvents();
        expect(count(events, "ship docks", s.id) == 1 && v.shipIn("east") && v.shipIn("east")->id == s.id, "it docks at the first pass");
        run(v, 10);
        events = v.takeEvents();
        expect(count(events, "ship docks") == 0, "once only");
        // Saved with the ship in: in still, its hands at it, never docked twice.
        const auto mid = v.save();
        auto u = load(f);
        expect(u.restore(mid).ok, "saved with the ship in");
        expect(u.shipIn("east") && u.shipIn("east")->id == s.id && u.wharfCrowd(id(2)), "in still, straight from the load");
        u.takeEvents();
        expect(u.setTimeOfDay(s.hour + 2).ok, "mid-morning");
        run(u, 30);
        events = u.takeEvents();
        expect(count(events, "ship docks") == 0 && u.shipIn("east"), "not docked again");
        const auto* hand = u.entity("d1");
        expect(hand && hand->activity.rfind("unloading " + s.name, 0) == 0, "its hands still unloading: " + (hand ? hand->activity : std::string()));
        // Down over its sailing too: gone when the world comes back.
        auto gone = mid;
        gone.calendarDays = double(s.day) + (s.sails + .5) / 24;
        auto t = load(f);
        expect(t.restore(gone).ok, "loaded after it sailed");
        t.takeEvents();
        t.tick(.6);
        events = t.takeEvents();
        expect(count(events, "ship sails", s.id) == 1 && !t.shipIn("east") && shipById(t, s.id) && shipById(t, s.id)->status == "sailed",
               "it sailed while the world was down");
    }
    {
        // A save from before the ships (Phase 4's): no ships on its Board; its timetable from the first pass, nothing lost.
        auto old = saved;
        old.society.board.ships.clear();
        old.society.board.shipsThrough.clear();
        old.society.board.nextShip = 1;
        auto v = load(f);
        expect(v.restore(old).ok, "a save from before the ships loads");
        expect(v.society().board().ships.empty(), "with none on its Board");
        v.tick(.6);
        const auto& b = v.society().board();
        expect(!b.ships.empty() && b.shipsThrough.count("east"), "its timetable rolled at the first pass");
        for (const auto& x : b.ships)
            expect(x.day > today(v) && x.status == "due", "each a day ahead at least");
        expect(v.society().realBalanced(), "real money is balanced");
    }
}

// Ship day (8.3, 8.6; T6): the Wharf's posts unloading it by name, the Wharf a crowd, its cargo cheaper toward the ship
// baseline and never dearer, what it takes dearer and never cheaper, the port's ship goods refilled with their back
// rooms; it sails that evening with the shops' gluts of what it takes.
void shipDay()
{
    auto f = harbour();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    w.roads().camps.clear();
    std::vector<WorldEvent> events;
    const auto s = toShipDay(w, "east", &events);
    auto& soc = w.society();
    std::map<std::string, std::string> facts;
    w.talkTown("d1", facts);
    expect(facts["world.ships"] == "due" && facts["world.ship_name"] == s.name, "due today, by name: " + facts["world.ships"] + " " + facts["world.ship_name"]);
    facts.clear();
    w.talkTown("w1", facts);
    expect(!facts.count("world.ships") && !facts.count("world.ship_name"), "no ship talk where there is no port");
    expect(hasWord(w.shipsNotice("east"), s.name + " today"), "the timetable names it for today: " + w.shipsNotice("east"));
    // The shelves of what ships bring sold out, the prices pushed about, before it comes.
    const auto imports = w.port("east")->imports;
    std::vector<std::pair<std::string, std::string>> shelves;   // (Keeper, ware.)
    for (const auto* keeper : {"em", "es", "ek", "eg", "et"})
        for (const auto& ware : soc.wares(keeper))
            if (std::find(imports.begin(), imports.end(), items::baseOf(ware)) != imports.end() && soc.shelfTarget(soc.tillOf(keeper), ware) > 0)
            {
                shelves.push_back({keeper, ware});
                soc.consume(soc.tillOf(keeper), ware, stock(w, keeper, ware), "test: sold out");
            }
    expect(!shelves.empty(), "east's shops set out what ships bring");
    expect(w.setTimeOfDay(s.hour - .3).ok, "a little before its hour");
    const auto dear = s.brings.front().first;
    const auto cheap = s.brings.size() > 1 ? s.brings[1].first : std::string();
    soc.setPressure("east", dear, .3, "test");
    if (!cheap.empty())
        soc.setPressure("east", cheap, -.3, "test");
    const auto wanted = s.takes.empty() ? std::string() : s.takes.front().first;
    const auto high = s.takes.size() > 1 ? s.takes[1].first : std::string();
    expect(!wanted.empty(), "it takes something east has");
    soc.setPressure("east", wanted, 0, "test");
    if (!high.empty())
        soc.setPressure("east", high, .4, "test");
    run(w, 5);
    expect(!w.shipIn("east") && !w.wharfCrowd(id(2)), "not in before its hour");
    const auto* hand = w.entity("d1");
    expect(hand && hand->activity.rfind("unloading ships at the Wharf", 0) == 0, "the hands at their posts as ever: " + (hand ? hand->activity : std::string()));
    w.takeEvents();
    dock(w, s);
    events = w.takeEvents();
    expect(count(events, "ship docks", s.id) == 1, "it docks");
    for (const auto& e : events)
        if (e.kind == "ship docks")
            expect(e.target == "east" && e.cell == id(2) && e.item == s.name && e.detail.rfind("The " + s.name.substr(4) + " is in", 0) == 0 &&
                       !hasWord(e.detail, "dog"),
                   "said where and what: " + e.detail);
    // The Board's nudges (8.6: imports toward −0.1, exports up), halfway, one way only, inside P's range.
    expect(std::abs(soc.pressure("east", dear) - .1) < .02, dear + " halfway from +0.3 to the ship baseline: " + std::to_string(soc.pressure("east", dear)));
    if (!cheap.empty())
        expect(std::abs(soc.pressure("east", cheap) + .3) < .02, cheap + " already cheaper is never raised: " + std::to_string(soc.pressure("east", cheap)));
    expect(std::abs(soc.pressure("east", wanted) - .05) < .02, wanted + " dearer, halfway to +0.1: " + std::to_string(soc.pressure("east", wanted)));
    if (!high.empty())
        expect(std::abs(soc.pressure("east", high) - .4) < .02, high + " already dearer is never lowered: " + std::to_string(soc.pressure("east", high)));
    const auto& cells = soc.board().cell;
    expect(cells.count("east") && cells.at("east").count(dear) && cells.at("east").at(dear).cause == "ship" &&
               cells.at("east").at(wanted).cause == "ship",
           "the ship is the cause, in a word");
    expect(soc.pressure("west", dear) == 0 || std::abs(soc.pressure("west", dear)) < .2, "only at the port's market");
    // The shops that sell its goods refill: shelves at their targets, a back room of two shelves behind each.
    for (const auto& [keeper, ware] : shelves)
    {
        const auto till = soc.tillOf(keeper);
        const int target = soc.shelfTarget(till, ware);
        expect(stock(w, keeper, ware) >= target, keeper + "'s " + ware + " refilled: " + std::to_string(stock(w, keeper, ware)) + " of " + std::to_string(target));
        const auto& rooms = soc.board().backRoom;
        const auto room = rooms.count(till) && rooms.at(till).count(items::baseOf(ware)) ? rooms.at(till).at(items::baseOf(ware)) : 0;
        expect(room >= 2 * target, "and two shelves behind: " + std::to_string(room));
    }
    // The Wharf's posts, by their words; the Wharf a crowd; talk's facts.
    expect(w.setTimeOfDay(s.hour + 1).ok, "an hour on");
    run(w, 30);
    const auto doing = [&](const char* who) {
        const auto* e = w.entity(who);
        return e ? e->activity : std::string();
    };
    expect(doing("d1").rfind("unloading " + s.name + " — A ship is in.", 0) == 0 && doing("d2").rfind("unloading " + s.name, 0) == 0,
           "the dock hands unloading it, by name: " + doing("d1") + " / " + doing("d2"));
    expect(doing("d3").rfind("tallying " + s.name + "'s cargo", 0) == 0 && doing("hm").rfind("seeing to " + s.name + "'s cargo", 0) == 0,
           "the clerk tallying, the harbourmaster seeing to it: " + doing("d3") + " / " + doing("hm"));
    expect(doing("e1").find(s.name) == std::string::npos && doing("em").find(s.name) == std::string::npos, "nobody else's task is the ship");
    expect(w.entity("d1")->cellId == id(2), "at their own posts, where they were");
    facts.clear();
    w.talkTown("d1", facts);
    expect(facts["world.ships"] == "in" && facts["world.ship_name"] == s.name && facts["here.crowd"] == "crowd",
           "talk knows: in, by name, the Wharf a crowd: " + facts["world.ships"] + " " + facts["here.crowd"]);
    facts.clear();
    w.talkTown("w1", facts);
    expect(!facts.count("world.ships") && facts["here.crowd"] != "crowd", "west knows nothing of it");
    expect(hasWord(w.shipsNotice("east"), s.name + " is in, unloading") && hasWord(w.shipsNotice("east"), "sails this evening"),
           "the timetable says it is in: " + w.shipsNotice("east"));
    // The shop list's price for its cargo is inside the clamp, whatever the nudges.
    for (const auto& [keeper, ware] : shelves)
    {
        const auto till = soc.tillOf(keeper);
        const int n = std::min(20, stock(w, keeper, ware));
        const auto q = soc.quote("player-ada", keeper, ware, n, true);
        const double factor = double(q.total) / (double(n) * double(Society::catalogPrice(ware)) * soc.baseFactor(till, ware));
        expect(q.ok && factor > market::FactorLow - .06 && factor < market::FactorHigh + .06,
               keeper + "'s " + ware + " inside the clamp: " + std::to_string(factor));
    }
    // It sails that evening with the gluts of what it takes.
    std::string glutKeeper, glutWare;
    for (const auto* keeper : {"em", "es", "ek", "eg", "et"})
        for (const auto& ware : soc.wares(keeper))
            for (const auto& [g, n] : s.takes)
                if (glutWare.empty() && items::baseOf(ware) == g && ware == g)
                    glutKeeper = keeper, glutWare = ware;
    int target = 0;
    if (!glutWare.empty())
    {
        const auto till = soc.tillOf(glutKeeper);
        target = soc.shelfTarget(till, glutWare);
        soc.create(till, glutWare, target + 25, "test: a glut sold in");
    }
    w.takeEvents();
    expect(w.setTimeOfDay(std::min(23.5, s.sails + .1)).ok, "to its evening hour");
    run(w, 5);
    events = w.takeEvents();
    expect(count(events, "ship sails", s.id) == 1 && !w.shipIn("east") && !w.wharfCrowd(id(2)), "it sails that evening");
    expect(shipById(w, s.id) && shipById(w, s.id)->status == "sailed", "sailed, on the timetable still");
    if (!glutWare.empty())
        expect(stock(w, glutKeeper, glutWare) <= target, "and takes the glut of " + glutWare + ": " + std::to_string(stock(w, glutKeeper, glutWare)) +
                                                              " left of a shelf of " + std::to_string(target));
    facts.clear();
    w.talkTown("d1", facts);
    expect(facts["world.ships"] == "sailed" && facts["world.ship_name"] == s.name, "talk knows it sailed: " + facts["world.ships"]);
    expect(hasWord(w.shipsNotice("east"), s.name + " sailed today"), "and the timetable: " + w.shipsNotice("east"));
    run(w, 30);
    expect(doing("d1").find(s.name) == std::string::npos, "the hands' task their own again: " + doing("d1"));
    // Every P on the Board inside its range, and real money balanced (ships move no coin).
    for (const auto& [m, goods] : soc.board().cell)
        for (const auto& [g, c] : goods)
            expect(std::abs(soc.pressure(m, g)) <= market::PressureMost + 1e-9, "P inside its range: " + m + " " + g);
    expect(soc.realBalanced(), "real money is balanced");
    // Two days on it is off the timetable.
    nextMorning(w);
    nextMorning(w);
    expect(!shipById(w, s.id), "and gone from the timetable two days on");
}

// D27 for ships, in place of Phase 4's stopgap: what the port's shops get only by ship thins between ships and refills
// when one docks; with the ships stopped (shipsPerWeek 0), or no port, it refills each morning as ever.
void importsWaitForShips()
{
    auto f = harbour(false);                        // (No carters: nothing here comes by wagon.)
    auto w = load(f);
    w.tick(.6);
    auto& soc = w.society();
    std::string keeper, ware;
    for (const auto* k : {"em", "ek", "eg"})
        for (const auto& x : soc.wares(k))
            if (ware.empty() && soc.imported("east", items::baseOf(x)) && soc.shelfTarget(soc.tillOf(k), x) > 1)
                keeper = k, ware = x;
    expect(!ware.empty(), "an east shop sets out a good only ships bring");
    const auto base = items::baseOf(ware);
    const auto imports = w.port("east")->imports;
    expect(std::find(imports.begin(), imports.end(), base) != imports.end(), base + " is the port's");
    const auto till = soc.tillOf(keeper);
    const int target = soc.shelfTarget(till, ware);
    // Sold out every day: between ships the back room sets out what it has, then nothing; a ship in, the shelf is full.
    bool thinned = false, refilled = false;
    int least = target;
    for (int day = 0; day < 30 && !(thinned && refilled); ++day)
    {
        soc.consume(till, ware, stock(w, keeper, ware), "test: sold out");
        nextMorning(w);
        bool shipToday = false;
        for (const auto& s : soc.board().ships)
            shipToday = shipToday || (s.day == today(w) && s.status != "storm");
        expect(w.setTimeOfDay(11.5).ok, "to the late morning");
        run(w, 30);
        if (shipToday)
        {
            expect(stock(w, keeper, ware) >= target, "a ship in: the shelf full again: " + std::to_string(stock(w, keeper, ware)));
            refilled = refilled || thinned;
            continue;
        }
        least = std::min(least, stock(w, keeper, ware));
        thinned = thinned || stock(w, keeper, ware) < target;
    }
    expect(thinned && least == 0, "the shelf thins to nothing between ships: " + std::to_string(least) + " of " + std::to_string(target));
    expect(refilled, "and a ship fills it again");
    // The ships stopped: nothing waits for them; the shelf refills the next morning.
    auto spec = soc.authored().economy;
    spec.rules["shipsPerWeek"] = 0;
    soc.adoptEconomy(spec);
    w.tick(.6);
    expect(!soc.imported("east", base) && w.port("east") && w.port("east")->imports.empty(), "with the ships stopped, nothing waits for one");
    soc.consume(till, ware, stock(w, keeper, ware), "test: sold out");
    nextMorning(w);
    expect(w.setTimeOfDay(11.5).ok, "to the late morning");
    run(w, 30);
    expect(stock(w, keeper, ware) >= target, "and the shelf refills each morning: " + std::to_string(stock(w, keeper, ware)));
    // No port at all: the same.
    auto bare = harbour(false, false);
    auto v = load(bare);
    v.tick(.6);
    expect(v.ports().empty() && !v.society().imported("east", base), "no port: nothing waits for a ship");
    v.society().consume(v.society().tillOf(keeper), ware, stock(v, keeper, ware), "test: sold out");
    nextMorning(v);
    expect(v.setTimeOfDay(11.5).ok, "to the late morning");
    run(v, 30);
    expect(stock(v, keeper, ware) >= v.society().shelfTarget(v.society().tillOf(keeper), ware), "and its shelf refills each morning");
}

// A storm keeps a ship out (8.3, 8.6): its cargo a step dearer inside P's range, a trouble for the market's Times, "No
// ship this week" on the board, talk's "late"; it never docks, and is off the timetable in a few days.
void aStormboundShip()
{
    auto f = harbour();
    auto w = load(f);
    w.tick(.6);
    auto& soc = w.society();
    // The first week with one ship due and nothing else of that week on the timetable: its storm leaves no ship.
    market::Ship s;
    for (int day = 0; day < 60 && s.id.empty(); ++day)
    {
        std::map<std::int64_t, int> perWeek;
        for (const auto& x : soc.board().ships)
            ++perWeek[x.day / 7];
        for (const auto& x : soc.board().ships)
            if (s.id.empty() && x.status == "due" && x.day > today(w) && perWeek[x.day / 7] == 1)
                s = x;
        if (s.id.empty())
            nextMorning(w);
    }
    expect(!s.id.empty(), "a week with one ship");
    const auto good = s.brings.front().first;
    soc.setPressure("east", good, .1, "test");
    const auto second = s.brings.size() > 1 ? s.brings[1].first : std::string();
    if (!second.empty())
        soc.setPressure("east", second, .47, "test");
    const auto troubles = soc.board().troubles.size();
    w.takeEvents();
    expect(!w.stormShip("s-none") && w.stormShip(s.id), "the DM's storm keeps it out");
    auto events = w.takeEvents();
    expect(count(events, "ship stormbound", s.id) == 1 && shipById(w, s.id)->status == "storm", "stormbound");
    expect(!w.stormShip(s.id), "once");
    expect(std::abs(soc.pressure("east", good) - .2) < .02 && soc.board().cell.at("east").at(good).cause == "ship late",
           good + " a step dearer, the cause said: " + std::to_string(soc.pressure("east", good)));
    if (!second.empty())
        expect(soc.pressure("east", second) <= market::PressureMost + 1e-9 && soc.pressure("east", second) > .47,
               second + " dearer but inside P's range: " + std::to_string(soc.pressure("east", second)));
    expect(soc.board().troubles.size() == troubles + 1 && soc.board().troubles.back().market == "east" && soc.board().troubles.back().kind == "late",
           "a trouble for the market's Times");
    const auto text = w.shipsNotice("east");
    expect(hasWord(text, s.name + " is stormbound and will not come.") && hasWord(text, "no ship this week.") && text.size() <= 280,
           "posted: " + text);
    expect(count(events, "ships posted", "east") >= 1, "and the notice changed");
    // Its day comes and goes: never in.
    while (today(w) < s.day)
        nextMorning(w);
    std::map<std::string, std::string> facts;
    w.talkTown("d1", facts);
    expect(facts["world.ships"] == "late" && facts["world.ship_name"] == s.name, "talk knows it is late: " + facts["world.ships"]);
    w.takeEvents();
    expect(w.setTimeOfDay(s.hour + .5).ok, "its hour");
    run(w, 10);
    expect(w.setTimeOfDay(19.8).ok, "the evening");
    run(w, 10);
    events = w.takeEvents();
    expect(count(events, "ship docks", s.id) == 0 && count(events, "ship sails", s.id) == 0 && !w.wharfCrowd(id(2)) && !w.shipIn("east"),
           "it never docks");
    for (int day = 0; day < 4; ++day)
        nextMorning(w);
    expect(!shipById(w, s.id), "off the timetable in a few days");
    // The seeded storms: over a year and more of mornings (a winter among them) a few ships are kept out, most come.
    auto v = load(f);
    v.tick(.6);
    int docks = 0, storms = 0;
    for (int day = 0; day < 400; ++day)
    {
        nextMorning(v);
        expect(v.setTimeOfDay(12).ok, "noon");
        v.tick(.6);
        for (const auto& e : v.takeEvents())
            docks += e.kind == "ship docks", storms += e.kind == "ship stormbound";
    }
    expect(docks > 20 && storms >= 1 && storms * 4 < docks, "most ships come, a few are stormbound: " + std::to_string(docks) + " in, " +
                                                                 std::to_string(storms) + " kept out");
    for (const auto& [m, goods] : v.society().board().cell)
        for (const auto& [g, c] : goods)
            expect(std::abs(v.society().pressure(m, g)) <= market::PressureMost + 1e-9, "P inside its range: " + m + " " + g);
    expect(v.society().board().ships.size() <= market::ShipsAPort && v.society().realBalanced(), "bounded, and real money balanced");
}

// The rule at its most (6: a ship most days): the timetable stays inside a port's bound, no two ships of a name on it
// at once, every one docking or stormbound, real money balanced.
void theRuleAtItsMost()
{
    auto f = harbour();
    auto w = load(f);
    auto spec = w.society().authored().economy;
    spec.rules["shipsPerWeek"] = 6;
    w.society().adoptEconomy(spec);
    w.tick(.6);
    std::size_t most = 0;
    int docks = 0, storms = 0;
    for (int day = 0; day < 28; ++day)
    {
        nextMorning(w);
        const auto& b = w.society().board();
        most = std::max(most, b.ships.size());
        expect(b.ships.size() <= market::ShipsAPort, "a port's timetable bounded: " + std::to_string(b.ships.size()));
        std::set<std::string> names;
        for (const auto& s : b.ships)
            expect(names.insert(s.name).second, "one ship of a name at once, with a ship most days: " + s.name);
        expect(w.shipsNotice("east").size() <= 280, "the notice fits: " + w.shipsNotice("east"));
        expect(w.setTimeOfDay(12).ok, "noon");
        w.tick(.6);
        for (const auto& e : w.takeEvents())
            docks += e.kind == "ship docks", storms += e.kind == "ship stormbound";
    }
    const auto week = today(w) / 7 + 1;
    expect(w.shipsInWeek("east", week) >= 5 && w.shipsInWeek("east", week) <= 7, "five to seven a week: " + std::to_string(w.shipsInWeek("east", week)));
    expect(most >= 10 && docks + storms >= 18, "two weeks ahead on the timetable, and they come: " + std::to_string(most) + " posted at most, " +
                                                    std::to_string(docks) + " in, " + std::to_string(storms) + " kept out");
    expect(w.society().realBalanced() && w.society().unregisteredFaucets() == 0, "real money balanced, no unregistered faucet");
}

// No port (8.3, 5.1): no timetable, no notice, no ship talk, nothing that waits for a ship.
void aWorldWithNoPort()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    std::vector<WorldEvent> events;
    for (int day = 0; day < 10; ++day)
    {
        nextMorning(w);
        expect(w.setTimeOfDay(12).ok, "noon");
        run(w, 5);
        for (auto& e : w.takeEvents())
            events.push_back(e);
    }
    expect(w.ports().empty() && !w.port("east"), "no port");
    const auto& b = w.society().board();
    expect(b.ships.empty() && b.shipsThrough.empty() && b.nextShip == 1, "no ship on the Board");
    for (const auto& e : events)
        expect(e.kind.rfind("ship", 0) != 0, "no ship event: " + e.kind);
    expect(w.shipsNotice("east").empty() && !w.shipIn("east") && w.shipsInWeek("east", 1) == 0 && !w.stormShip("s1") && !w.wharfCrowd(id(1)),
           "nothing to tell, nothing in, nothing to keep out");
    std::map<std::string, std::string> facts;
    w.talkTown("e1", facts);
    expect(!facts.count("world.ships") && !facts.count("world.ship_name"), "no ship talk");
    const auto j = market::encode(b);
    expect(json::dump(j).find("\"ships\"") == std::string::npos && json::dump(j).find("shipsThrough") == std::string::npos, "and none saved");
    // Nothing waits for a ship: with no carters either, every shelf refills each morning.
    auto lone = strip("EE.....WW", "", false);
    auto v = load(lone);
    v.tick(.6);
    for (const auto& g : market::data().shipBrings)
        expect(!v.society().imported("east", g) && !v.society().imported("west", g), g + " waits for nothing");
    std::string ware;
    for (const auto& x : v.society().wares("em"))
        if (ware.empty() && v.society().shelfTarget(v.society().tillOf("em"), x) > 0)
            ware = x;
    expect(!ware.empty(), "the stall sets something out");
    v.society().consume(v.society().tillOf("em"), ware, stock(v, "em", ware), "test: sold out");
    nextMorning(v);
    expect(v.setTimeOfDay(11.5).ok, "to the late morning");
    run(v, 30);
    expect(stock(v, "em", ware) >= v.society().shelfTarget(v.society().tillOf("em"), ware), "its shelf refills each morning");
    expect(w.society().realBalanced() && v.society().realBalanced(), "real money is balanced");
}

// A port deleted (5.1: "Delete the Wharf and there are no ships"): a save with ships loads into the world without it, its
// ships and its week's roll dropped, said once; its goods refill each morning again; the DM's rule at 0 does the same for
// the ships still due, and back on they are rolled again.
void aPortDeleted()
{
    auto f = harbour(false);
    auto w = load(f);
    w.tick(.6);
    const auto s = toShipDay(w);
    dock(w, s);
    const auto base = w.port("east")->imports.front();
    expect(w.society().imported("east", base), base + " waits for ships");
    const auto saved = w.save();
    expect(saved.society.board.ships.size() >= 1 && saved.society.board.shipsThrough.count("east"), "a save with ships, one in");
    auto edited = harbour(false, false);
    auto v = load(edited);
    const auto restored = v.restore(saved);
    expect(restored.ok, "the save loads into the world with no Wharf: " + restored.message);
    auto events = v.takeEvents();
    const auto& b = v.society().board();
    expect(v.ports().empty() && b.ships.empty() && b.shipsThrough.empty(), "its ships dropped on load");
    expect(count(events, "ships cleared", "east") == 1, "said once, for its notice");
    expect(!v.shipIn("east") && !v.wharfCrowd(id(2)) && v.shipsNotice("east").empty(), "no ship in, no crowd, nothing to tell");
    v.tick(.6);
    run(v, 10);
    events = v.takeEvents();
    for (const auto& e : events)
        expect(e.kind.rfind("ship", 0) != 0, "nothing more of ships: " + e.kind);
    std::map<std::string, std::string> facts;
    v.talkTown("e1", facts);
    expect(!facts.count("world.ships"), "no ship talk");
    expect(!v.society().imported("east", base), base + " waits for nothing now");
    std::string keeper, ware;
    for (const auto* k : {"em", "ek", "eg"})
        for (const auto& x : v.society().wares(k))
            if (ware.empty() && items::baseOf(x) == base && v.society().shelfTarget(v.society().tillOf(k), x) > 0)
                keeper = k, ware = x;
    expect(!ware.empty(), "a shop sets it out");
    v.society().consume(v.society().tillOf(keeper), ware, stock(v, keeper, ware), "test: sold out");
    nextMorning(v);
    expect(v.setTimeOfDay(11.5).ok, "to the late morning");
    run(v, 30);
    expect(stock(v, keeper, ware) >= v.society().shelfTarget(v.society().tillOf(keeper), ware), "and it refills each morning again");
    for (int day = 0; day < 8; ++day)
        nextMorning(v);
    expect(v.society().board().ships.empty() && v.society().board().shipsThrough.empty(), "a week on: still none");
    expect(v.society().realBalanced(), "real money is balanced");
    // The rule at 0 while a ship is in and others due: those due are dropped, the one in still sails; back on, rolled again.
    auto spec = w.society().authored().economy;
    spec.rules["shipsPerWeek"] = 0;
    w.society().adoptEconomy(spec);
    w.takeEvents();
    w.tick(.6);
    const auto& wb = w.society().board();
    expect(wb.ships.size() == 1 && wb.ships.front().id == s.id && wb.ships.front().status == "in" && wb.shipsThrough.empty(),
           "the ships still due dropped, the one in kept: " + std::to_string(wb.ships.size()));
    expect(w.setTimeOfDay(std::min(23.5, s.sails + .1)).ok, "its evening");
    run(w, 5);
    events = w.takeEvents();
    expect(count(events, "ship sails", s.id) == 1, "and it sails");
    for (int day = 0; day < 9; ++day)
        nextMorning(w);
    expect(w.society().board().ships.empty() && w.shipsNotice("east").empty(), "no ships while the rule is 0");
    events = w.takeEvents();
    spec.rules["shipsPerWeek"] = 2;
    w.society().adoptEconomy(spec);
    w.tick(.6);
    expect(!w.society().board().ships.empty() && w.society().board().shipsThrough.count("east"), "back on: rolled again at once");
    for (const auto& x : w.society().board().ships)
        expect(x.day > today(w) && x.id != s.id, "new ships, a day ahead at least");
    expect(w.society().realBalanced(), "real money is balanced");
}

// Players paid by the port's shops on ship day (9.3, 9.4): a player sells what the ship takes at its dearer price; the
// shops' staged coins reach the player under the day's caps (the holder's sell total, each shop's budget), every coin a
// registered faucet, real money balanced.
void shipsPayingPlayers()
{
    auto f = harbour();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    w.roads().camps.clear();
    auto& soc = w.society();
    expect(soc.realBalanced(), "balanced at the start");
    // A ship that takes a good a shop of east buys from players.
    market::Ship s;
    std::string keeper, good;
    for (int tries = 0; tries < 12 && good.empty(); ++tries)
    {
        if (tries)
            nextMorning(w);
        s = toShipDay(w);
        for (const auto& [g, n] : s.takes)
            for (const auto* k : {"et", "eg", "em", "ek", "es"})
                if (good.empty())
                {
                    soc.create("player-ada", g, 1, "test: gathered");
                    if (soc.quote("player-ada", k, g, 1, false).ok)
                        keeper = k, good = g;
                    soc.consume("player-ada", g, 1, "test: put back");
                }
    }
    expect(!good.empty(), "a ship takes a good an east shop buys");
    soc.setPressure("east", good, 0, "test");
    soc.create("player-ada", good, 400, "test: gathered");
    const auto before = soc.quote("player-ada", keeper, good, 5, false);
    dock(w, s);
    const auto after = soc.quote("player-ada", keeper, good, 5, false);
    expect(before.ok && after.ok && after.total >= before.total, "the ship's buyers about: it sells for no less: " + std::to_string(before.total) +
                                                                     " then " + std::to_string(after.total));
    // The day's sell cap first (the DM's rule, set low): the shops stop paying at it, by its own words.
    auto spec = soc.authored().economy;
    spec.rules["playerSellMost"] = 25;
    soc.adoptEconomy(spec);
    const auto holder = soc.holderOf("player-ada");
    const auto purse = soc.cash("player-ada");
    const auto till = soc.tillOf(keeper);
    std::int64_t paid = 0;
    int sales = 0;
    std::string refusal;
    const auto sell = [&](int each) {
        refusal.clear();
        for (int i = 0; i < 80 && refusal.empty(); ++i)
            if (const auto sold = soc.trade("player-ada", keeper, good, each, false); sold.ok)
                paid += sold.total, ++sales;
            else
                refusal = sold.message;
    };
    sell(1);
    expect(sales > 0 && paid > 0 && paid <= 25 && refusal == "No one here is buying more from you today.", "the shop paid for " + good + " up to the day's sell cap: " + std::to_string(paid) +
                                                                        "p in " + std::to_string(sales) + " sales, then: " + refusal);
    expect(soc.soldToday(holder) == paid && soc.cash("player-ada") == purse + paid, "every penny counted, every penny in the purse");
    // The rule back at its 400: the shop's own day's budget is the next cap, and holds.
    spec.rules["playerSellMost"] = 400;
    soc.adoptEconomy(spec);
    const auto cap = std::int64_t(soc.rule("playerSellMost"));
    sell(1);
    expect(paid > 25 && paid <= soc.budgetOf(till) && paid <= cap && soc.budgetLeft(till) >= 0 && refusal == "I've spent all I'll spend today.",
           "then up to the shop's budget (" + std::to_string(soc.budgetOf(till)) + "p): " + std::to_string(paid) + "p, then: " + refusal);
    expect(soc.soldToday(holder) == paid && soc.cash("player-ada") == purse + paid, "counted and in the purse still");
    for (const auto* k : {"et", "eg", "em", "ek", "es"})
        expect(soc.budgetLeft(soc.tillOf(k)) >= 0 && soc.budgetLeft(soc.tillOf(k)) <= soc.budgetOf(soc.tillOf(k)), std::string(k) + "'s budget never overdrawn");
    expect(soc.faucetToday(holder, "trade") >= paid, "each a coin through the faucet door: " + std::to_string(soc.faucetToday(holder, "trade")));
    expect(soc.unregisteredFaucets() == 0 && soc.realBalanced(), "no coin from nowhere: real money is balanced");
    expect(std::abs(soc.pressure("east", good)) <= market::PressureMost + 1e-9, "P inside its range after the glut");
    // The ship sails with the glut the player sold in; still balanced, and nothing was paid by the ship itself.
    expect(w.setTimeOfDay(std::min(23.5, s.sails + .1)).ok, "its evening");
    run(w, 5);
    expect(!w.shipIn("east") && soc.cash("player-ada") == purse + paid && soc.realBalanced(), "it sails; the purse as it was; balanced");
    for (const auto& [acct, a] : soc.state().accounts)
        expect(acct.rfind("ship", 0) != 0, "a ship has no account: " + acct);
}

// The Wharf office's notice (8.3, D8): one a port on the board a player at its market reads, changed as the ships come
// and go, down when the DM stops the ships; never counted against the board's pins.
void theWharfOfficesNotice()
{
    auto f = harbour();
    const auto root = onDisk(f, "port-notice");
    {
        game::Game g(optionsOn(root));
        std::string problem;
        expect(g.start(problem), "a game starts on the harbour: " + problem);
        auto& w = g.world();
        w.tick(.6);
        w.tick(.6);
        w.roads().camps.clear();
        const auto* n = g.documents().find("ships-east");
        expect(n && n->kind == "notice" && n->board == "board:east" && n->author == "the Wharf office" && n->sign == "the Wharf office" &&
                   n->what == "ships:east" && n->text == w.shipsNotice("east") && n->text.rfind("Ships at East.", 0) == 0 &&
                   n->text.size() <= 280 && n->expires > w.calendarDays() + 6,
               "the timetable pinned on east's board by the Wharf office: " + (n ? n->text : std::string("none")));
        expect(!g.documents().find("ships-west"), "none at west");
        Client ash;
        enterAt(g, ash, 83, "ash", "east");
        board(g, ash, {{"verb", "read"}});
        bool read = false;
        if (const auto* shown = ash.last("board"))
            for (const auto& x : shown->array("notices"))
                read = read || (x.string("sign") == "the Wharf office" && x.string("text") == n->text);
        expect(read, "a player at east's board reads it");
        // Ship day: the notice says it is in, then that it sailed.
        const auto s = toShipDay(w);
        dock(w, s);
        n = g.documents().find("ships-east");
        expect(n && hasWord(n->text, s.name + " is in") && n->text == w.shipsNotice("east"), "it says the ship is in: " + (n ? n->text : std::string("none")));
        expect(w.setTimeOfDay(std::min(23.5, s.sails + .1)).ok, "its evening");
        run(w, 5);
        n = g.documents().find("ships-east");
        expect(n && hasWord(n->text, s.name + " sailed today"), "then that it sailed: " + (n ? n->text : std::string("none")));
        // A storm: "No ship this week" where it leaves the week bare; else that it will not come.
        nextMorning(w);
        const market::Ship* next = nullptr;
        for (const auto& x : w.society().board().ships)
            if (!next && x.status == "due" && x.day > today(w))
                next = &x;
        expect(next != nullptr, "another ship is due");
        const auto name = next->name;
        expect(w.stormShip(next->id), "a storm");
        n = g.documents().find("ships-east");
        expect(n && hasWord(n->text, name + " is stormbound and will not come."), "posted: " + (n ? n->text : std::string("none")));
        // Up afresh each morning, so it never lapses while ships come.
        for (int day = 0; day < 9; ++day)
            nextMorning(w);
        n = g.documents().find("ships-east");
        expect(n && n->expires > w.calendarDays() + 6 && n->written > w.calendarDays() - 1, "posted afresh each morning");
        int wharfs = 0;
        for (const auto* d : g.documents().onBoard("board:east"))
            wharfs += d->author == "the Wharf office";
        expect(wharfs == 1, "one notice a port, never a pile: " + std::to_string(wharfs));
        // The DM stops the ships: the notice comes down.
        auto spec = w.society().authored().economy;
        spec.rules["shipsPerWeek"] = 0;
        w.society().adoptEconomy(spec);
        w.tick(.6);
        w.tick(.6);
        if (w.shipIn("east"))
        {
            expect(w.setTimeOfDay(21).ok, "the evening");
            run(w, 5);
        }
        for (int day = 0; day < 3; ++day)
            nextMorning(w);
        expect(!g.documents().find("ships-east"), "no ships, no notice");
        expect(w.society().realBalanced(), "real money is balanced");
    }
    std::filesystem::remove_all(root);
}

struct Saved
{
    std::string checkpoint;                         // The replayed checkpoint (file mode's save and its journal).
    std::string ships, in, notice;                  // The live timetable, the ship in, the notice's words.
};

// The timetable through the game's save (file mode): a checkpoint, then a ship docks and an economy record is journalled;
// a crash; the game started again from the save and its journal shows the same ships, the one in still in, docked once.
// With `out`, what Postgres is given (the journal replayed over the checkpoint) and what must come back.
Saved theTimetableThroughACrash(bool check = true)
{
    auto f = harbour();
    const auto root = onDisk(f, "port-journal");
    const std::string path = (std::filesystem::temp_directory_path() / ("ratw-s2stage-port-" + std::to_string(::getpid()) + ".json")).string();
    const auto clean = [&] {
        for (const auto* end : {"", ".journal", ".art", ".lock", ".reports", ".snapshot"})
            std::filesystem::remove_all(path + end);
    };
    clean();
    Saved out;
    market::Ship s;
    const auto slurp = [](const std::string& file) {
        std::ifstream in(file, std::ios::binary);
        std::ostringstream o;
        o << in.rdbuf();
        return o.str();
    };
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "a game starts on the harbour with a file save: " + problem);
        auto& w = g.world();
        w.tick(.6);
        w.tick(.6);
        w.roads().camps.clear();
        s = toShipDay(w);
        g.save();
        g.settle();                                 // (The checkpoint, on its day: the ship due, not yet in.)
        const auto savedText = slurp(path);
        dock(w, s);
        Client ash;
        enterAt(g, ash, 84, "ash", "east");
        board(g, ash, {{"verb", "post"}, {"kind", "other"}, {"text", "A trade, journalled."}});
        g.settle();                                 // (Its reply waits for its journal record.)
        expect(ash.said().find("You pin your notice") != std::string::npos, "a penny paid: an economy record: " + ash.said());
        out.ships = shipsOf(w);
        out.in = s.id;
        out.notice = w.shipsNotice("east");
        expect(slurp(path) == savedText, "no save was taken after the ship docked (the journal is the tail)");
        // The journal replayed over the checkpoint, as the package tool and the server read it.
        std::vector<journal::Record> records;
        expect(journal::readFile(path + ".journal", 0, records, problem), "the journal reads back: " + problem);
        auto document = parsed(savedText);
        std::uint64_t last = 0;
        expect(journal::replay(document, records, std::uint64_t(document.number("journal")), last, problem),
               "the journal replays over the checkpoint: " + problem);
        PersistedWorld saved;
        checkpoint::ServerState server;
        expect(checkpoint::decode(document, saved, server, problem), "the replayed save decodes: " + problem);
        bool in = false;
        for (const auto& x : saved.society.board.ships)
            in = in || (x.id == s.id && x.status == "in");
        expect(in, "the journal carried the ship in");
        out.checkpoint = json::dump(checkpoint::encode(saved, server, saved.npcs, saved.time));
        // A crash: the game goes without saving.
    }
    if (check)
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "starts again after the crash: " + problem);
        auto& w = g.world();
        expect(shipsOf(w) == out.ships, "the same ships after the crash");
        expect(w.shipIn("east") && w.shipIn("east")->id == s.id, "the one in still in");
        w.takeEvents();
        w.tick(.6);
        w.tick(.6);
        const auto events = w.takeEvents();
        expect(count(events, "ship docks") == 0 && shipsOf(w) == out.ships, "never docked twice");
        expect(w.shipsNotice("east") == out.notice, "the same words: " + w.shipsNotice("east"));
        const auto* n = g.documents().find("ships-east");
        expect(n && n->text == out.notice, "and the notice up again: " + (n ? n->text : std::string("none")));
        expect(w.society().realBalanced(), "real money is balanced");
    }
    clean();
    std::filesystem::remove_all(root);
    return out;
}

void theTimetableSurvivesACrash()
{
    (void)theTimetableThroughACrash();
}

// A port deleted under a running game's save (a host's edit between two runs): the save loads into the world with no
// Wharf, the ships gone and the Wharf office's notice down.
void aPortDeletedUnderTheGame()
{
    auto f = harbour();
    auto root = onDisk(f, "port-deleted");
    const std::string path = (std::filesystem::temp_directory_path() / ("ratw-s2stage-portdel-" + std::to_string(::getpid()) + ".json")).string();
    const auto clean = [&] {
        for (const auto* end : {"", ".journal", ".art", ".lock", ".reports", ".snapshot"})
            std::filesystem::remove_all(path + end);
    };
    clean();
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "a game starts on the harbour: " + problem);
        auto& w = g.world();
        w.tick(.6);
        w.tick(.6);
        const auto s = toShipDay(w);
        dock(w, s);
        expect(g.documents().find("ships-east") != nullptr, "the timetable is up");
        g.save();
        g.settle();
    }
    std::filesystem::remove_all(root);
    auto edited = harbour(true, false);
    root = onDisk(edited, "port-deleted");
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "the save starts on the world with no Wharf: " + problem);
        auto& w = g.world();
        expect(w.ports().empty() && w.society().board().ships.empty() && w.society().board().shipsThrough.empty(), "its ships gone");
        w.tick(.6);
        for (int t = 0; t < 30; ++t)
            g.tick(.05);                            // (The game's own pass: its notices tended.)
        expect(!g.documents().find("ships-east"), "and the Wharf office's notice down");
        for (int day = 0; day < 3; ++day)
            nextMorning(w);
        expect(!g.documents().find("ships-east") && w.society().board().ships.empty(), "for good");
        expect(w.society().realBalanced(), "real money is balanced");
    }
    clean();
    std::filesystem::remove_all(root);
}

// --write-timetable DIR: the replayed checkpoint (timetable.json) and what it must still say (timetable.expected.json),
// for Tests/port_pg.py to save through game.save_checkpoint into a throwaway database and load back.
int writeTimetable(const std::filesystem::path& dir)
{
    const auto saved = theTimetableThroughACrash(false);
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "timetable.json", std::ios::binary) << saved.checkpoint;
    auto expected = json::Value::object();
    expected.add("ships", saved.ships);
    expected.add("in", saved.in);
    expected.add("notice", saved.notice);
    std::ofstream(dir / "timetable.expected.json", std::ios::binary) << json::dump(expected);
    return 0;
}

// --check-timetable BACK DIR: the checkpoint as Postgres gave it back decodes to the same timetable, and a world restored
// from it shows the same ships, the one in still in, the same words on its notice; it sails that evening.
int checkTimetable(const std::filesystem::path& back, const std::filesystem::path& dir)
{
    const auto slurp = [](const std::filesystem::path& file) {
        std::ifstream in(file, std::ios::binary);
        std::ostringstream o;
        o << in.rdbuf();
        return o.str();
    };
    const auto expected = parsed(slurp(dir / "timetable.expected.json"));
    PersistedWorld after;
    checkpoint::ServerState server;
    std::string problem;
    expect(checkpoint::decode(parsed(slurp(back / "timetable.json")), after, server, problem), "decodes after Postgres: " + problem);
    expect(!after.society.board.ships.empty(), "ships came back");
    auto f = harbour();
    auto w = load(f);
    const auto restored = w.restore(after);
    expect(restored.ok, "a world restores from it: " + restored.message);
    expect(shipsOf(w) == expected.string("ships"), "the same timetable through Postgres");
    expect(w.shipIn("east") && w.shipIn("east")->id == expected.string("in"), "the ship in still in");
    w.takeEvents();
    w.tick(.6);
    expect(w.shipsNotice("east") == expected.string("notice"), "the same words: " + w.shipsNotice("east"));
    expect(count(w.takeEvents(), "ship docks") == 0, "not docked again");
    const auto in = *w.shipIn("east");
    expect(w.setTimeOfDay(std::min(23.5, in.sails + .1)).ok, "its evening");
    run(w, 5);
    expect(!w.shipIn("east") && shipById(w, in.id) && shipById(w, in.id)->status == "sailed", "and it sails that evening");
    // The whole checkpoint the same again (Postgres's jsonb keeps no key order: json::Value's == compares whatever the order).
    PersistedWorld again;
    checkpoint::ServerState server2;
    expect(checkpoint::decode(parsed(slurp(dir / "timetable.json")), again, server2, problem), "the written one decodes: " + problem);
    expect(market::encode(again.society.board) == market::encode(after.society.board), "the Board the same through Postgres");
    std::cout << "port_tests --check-timetable: the timetable the same through Postgres (" << checks << " checks)\n";
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--write-timetable")
            return writeTimetable(argv[2]);
        if (argc == 4 && std::string(argv[1]) == "--check-timetable")
            return checkTimetable(argv[2], argv[3]);
        aPortAndItsTimetable();
        theTimetableSurvivesARestart();
        shipDay();
        importsWaitForShips();
        aStormboundShip();
        theRuleAtItsMost();
        aWorldWithNoPort();
        aPortDeleted();
        shipsPayingPlayers();
        theWharfOfficesNotice();
        theTimetableSurvivesACrash();
        aPortDeletedUnderTheGame();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Port tests passed: " << checks << " checks.\n";
    return 0;
}
