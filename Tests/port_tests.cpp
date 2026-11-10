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
#include "RatwSeeded.h"
#include "RatwWorld.h"
#include "battle_play.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
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
Fixture harbour(bool carters = true, bool docks = true, const std::string& more = {})
{
    std::string extra = resident("ek", "Kit Vane", "merchant", "innkeeper at The Anchor", 0, 2.5, 1, 12.5) +
                        resident("eg", "Gus Vane", "merchant", "shopkeeper at The East Store", 0, 3.5, 1, 13.5) +
                        resident("et", "Tess Vane", "merchant", "tanner at The Tannery", 0, 3.5, 1, 14.5);
    if (docks)
        extra += resident("d1", "Dock One", "civilian", "unloading ships at the Wharf", 0, 12.5, 2, 4.5) +
                 resident("d2", "Dock Two", "civilian", "working the cranes at the Wharf", 0, 13.5, 2, 5.5) +
                 resident("d3", "Tally Three", "civilian", "tallying in the long store", 0, 14.5, 2, 6.5) +
                 resident("hm", "Hale Marsh", "civilian", "harbourmaster", 0, 1.5, 2, 7.5);
    auto f = strip("EEE....WW", extra + more, carters);
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
        expect(!market::forbiddenWord(name) && name.rfind("the ", 0) == 0, "a ship's name: " + name);
    expect(!port.imports.empty(), "its shops set out goods that wait for ships");
    // What waits for a ship (7.7, D27): market.json's imports its shops set out and nobody there makes, and whatever else
    // they set out that nothing in the realm makes and no wagon brings; never what the wharf's `never` names.
    const auto& points = w.society().tradePoints();
    const auto* eastPoint = points.find("east");
    expect(eastPoint != nullptr, "east is a trade point");
    std::set<std::string> realmMakes;
    for (const auto& m : points.markets)
        realmMakes.insert(m.crafted.begin(), m.crafted.end());
    for (const auto& farm : w.society().farms())
        if (farm.def)
            realmMakes.insert(farm.def->yields.begin(), farm.def->yields.end());
    bool listedOne = false, unlisted = false;
    for (const auto& g : port.imports)
    {
        const bool onList = std::find(data.shipBrings.begin(), data.shipBrings.end(), g) != data.shipBrings.end();
        listedOne = listedOne || onList, unlisted = unlisted || !onList;
        expect(eastPoint->sold.count(g) && !eastPoint->crafted.count(g) && !data.shipNever.count(g) && (onList || !realmMakes.count(g)) &&
                   w.society().imported("east", g),
               g + " is the wharf's import or made nowhere in the realm, and waits for a ship at east");
    }
    expect(listedOne && unlisted, "the wharf's own imports, and the goods nothing in the realm makes (not butter alone)");
    for (const auto& g : eastPoint->sold)
        if (!eastPoint->crafted.count(g) && items::good(g) && !data.shipNever.count(g) && !realmMakes.count(g))
            expect(w.society().imported("east", g), g + ": sold at east, made nowhere: it waits for a ship or a wagon, never the morning");
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
                expect(s.status == "due" && s.hour >= 8.5 && s.hour <= 11 && s.sails >= 17 && s.sails <= 19.5,
                       "in while the Wharf is at work, out that evening: " + std::to_string(s.hour) + " " + std::to_string(s.sails));
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
    // More than butter (the repair): over three weeks the ships bring a spread of the port's goods, the wharf's own and
    // the others, and no ship the same first word as every other.
    std::set<std::string> brought, leads;
    bool others = false;
    for (const auto& [shipId, ship] : first)
    {
        leads.insert(ship.brings.front().first);
        for (const auto& [g, n] : ship.brings)
        {
            brought.insert(g);
            others = others || std::find(data.shipBrings.begin(), data.shipBrings.end(), g) == data.shipBrings.end();
        }
    }
    expect(brought.size() >= std::min<std::size_t>(6, port.imports.size()) && leads.size() >= 2 && others,
           "a spread of cargo: " + std::to_string(brought.size()) + " goods, " + std::to_string(leads.size()) + " first words");
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
    expect(testPurse(soc, "player-ada", 5000), "a purse to price a shelf with");
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
    // No ship in: nobody is seen unloading one. The post named for it minds the cargo; the others are at their own work.
    expect(w.setTimeOfDay(s.hour - .2).ok, "still before its hour");
    run(w, 30);
    const auto* hand = w.entity("d1");
    const auto* crane = w.entity("d2");
    expect(hand && hand->activity.rfind("minding the cargo at the Wharf — No ship is in.", 0) == 0 && hand->activity.find("unloading") == std::string::npos,
           "no ship in, nobody unloading one: " + (hand ? hand->activity : std::string()));
    expect(crane && crane->activity.rfind("working the cranes at the Wharf", 0) == 0, "the cranes as ever: " + (crane ? crane->activity : std::string()));
    expect(!w.shipIn("east") && !w.wharfCrowd(id(2)), "and still not in");
    w.takeEvents();
    dock(w, s);
    events = w.takeEvents();
    expect(count(events, "ship docks", s.id) == 1, "it docks");
    for (const auto& e : events)
        if (e.kind == "ship docks")
            expect(e.target == "east" && e.cell == id(2) && e.item == s.name && e.detail.rfind("The " + s.name.substr(4) + " is in with ", 0) == 0 &&
                       !market::forbiddenWord(e.detail),
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
               cells.at("east").at(wanted).cause == "ship takes",
           "the ship is the cause: its cargo's, and what it takes its own");
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
    expect(hasWord(w.shipsNotice("east"), s.name + " is in with ") && hasWord(w.shipsNotice("east"), "sails this evening") &&
               !hasWord(w.shipsNotice("east"), "unloading"),
           "the timetable says it is in, and what with: " + w.shipsNotice("east"));
    // The shop list's price for its cargo is inside the clamp, whatever the nudges.
    for (const auto& [keeper, ware] : shelves)
    {
        const auto till = soc.tillOf(keeper);
        const int n = std::min(20, stock(w, keeper, ware));
        const auto q = soc.quote("player-ada", keeper, ware, n, true);
        const double factor = double(q.total) / (double(n) * double(Society::catalogPrice(ware)) * soc.baseFactor(till, ware));
        expect(q.ok && factor > market::FactorLow - .06 && factor < market::FactorHigh + .06,
               keeper + "'s " + ware + " inside the clamp: " + std::to_string(factor) + " " + q.message);
    }
    std::string buyer, taken;                       // (A shop's ware the ship takes, dear enough to show a nudge.)
    // The shop list says why a price moved in a player's words, by the ship's name (Section 11): never the Board's own
    // word for it ("ship", "ship takes"), and never "came in" as the cause of a dear price.
    {
        // (A ware dear enough that the nudge shows as a penny at least: a pat of butter at 3p rounds it away.)
        std::string keeper, ware;
        for (const auto& [k, x] : shelves)
            for (const auto& [g, n] : s.brings)
                if (ware.empty() && items::baseOf(x) == g)
                {
                    const double was = soc.pressure("east", g);
                    soc.setPressure("east", g, -.4, "ship");
                    if (soc.listing("player-ada", k, x).trend < 0)
                        keeper = k, ware = x;
                    soc.setPressure("east", g, was, "ship");
                }
        expect(!ware.empty(), "a shelf of the ship's cargo whose price shows the ship");
        const auto base = items::baseOf(ware);
        const double was = soc.pressure("east", base);
        soc.setPressure("east", base, -.4, "ship");
        auto l = soc.listing("player-ada", keeper, ware);
        expect(l.trend < 0 && l.cause == s.name + " came in", "cheaper, and why: " + l.cause);
        soc.setPressure("east", base, .4, "ship");
        l = soc.listing("player-ada", keeper, ware);
        expect(l.trend > 0 && l.cause == "dear still, though " + s.name + " came in", "dear, and never because a ship came in: " + l.cause);
        soc.setPressure("east", base, was, "ship");
        for (const auto* k : {"em", "es", "ek", "eg", "et"})
            for (const auto& x : soc.wares(k))
                for (const auto& [g, n] : s.takes)
                    if (taken.empty() && items::baseOf(x) == g && Society::catalogPrice(x) >= 12)
                        buyer = k, taken = x;
        if (!taken.empty())
        {
            const auto g = items::baseOf(taken);
            const double before = soc.pressure("east", g);
            soc.setPressure("east", g, .4, "ship takes");
            l = soc.listing("player-ada", buyer, taken);
            expect(l.trend > 0 && l.cause == s.name + " is in, buying", "what it takes is dearer, and why: " + l.cause);
            soc.setPressure("east", g, before, "ship takes");
        }
    }
    // Its hands gone home with the ship still in (it sails after their day): a ship at a quiet pier, no crowd.
    if (s.sails >= 17.5)
    {
        expect(w.setTimeOfDay(s.sails - .2).ok, "after the Wharf's day");
        run(w, 30);
        expect(w.shipIn("east") && !w.wharfCrowd(id(2)), "in still, and nobody at it: no crowd");
        facts.clear();
        w.talkTown("d1", facts);
        expect(facts["world.ships"] == "in" && facts["world.ship_name"] == s.name, "talk knows it is in still: " + facts["world.ships"]);
        expect(w.entity("d1")->activity.find(s.name) == std::string::npos, "its hands' day done: " + w.entity("d1")->activity);
    }
    // It sails that evening with the gluts of what it takes.
    std::string glutKeeper, glutWare;
    for (const auto* keeper : {"em", "es", "ek", "eg", "et"})
        for (const auto& ware : soc.stagedShelfGoods(keeper))   // (What a shop sets out: what it holds over its shelf is a glut.)
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
    if (!taken.empty())
    {
        const auto g = items::baseOf(taken);
        soc.setPressure("east", g, .4, "ship takes");
        const auto l = soc.listing("player-ada", buyer, taken);
        expect(l.trend > 0 && l.cause == s.name + " bought a load", "dear since it sailed, and why: " + l.cause);
    }
    run(w, 30);
    expect(doing("d1").find(s.name) == std::string::npos, "the hands' task their own again: " + doing("d1"));
    // Every P on the Board inside its range, and real money balanced (ships move no coin).
    for (const auto& [m, goods] : soc.board().cell)
        for (const auto& [g, c] : goods)
            expect(std::abs(soc.pressure(m, g)) <= market::PressureMost + 1e-9, "P inside its range: " + m + " " + g);
    expect(soc.realBalanced(), "real money is balanced");
    // Told of the day after too; kept on the timetable through its week's end (the week it came in is never "no ship
    // this week"); then off it.
    nextMorning(w);
    expect(hasWord(w.shipsNotice("east"), s.name + " sailed yesterday"), "the day after: " + w.shipsNotice("east"));
    facts.clear();
    w.talkTown("d1", facts);
    expect(facts["world.ships"] != "sailed" || facts["world.ship_name"] == s.name, "talk's ship by its own name: " + facts["world.ship_name"]);
    const auto weekEnd = s.day / 7 * 7 + 6;
    while (today(w) <= std::max(s.day + 1, weekEnd))
    {
        expect(shipById(w, s.id) != nullptr, "kept on the timetable through its week");
        expect(!hasWord(w.shipsNotice("east"), "no ship this week"), "a week a ship came in is never a week with none: " + w.shipsNotice("east"));
        if (today(w) > s.day + 1)
            expect(!hasWord(w.shipsNotice("east"), s.name + " sailed"), "but no longer told of: " + w.shipsNotice("east"));
        nextMorning(w);
    }
    expect(!shipById(w, s.id), "and off the timetable after its week");
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
    // ("No ship this week" where the week it leaves bare is this one: noShipThisWeek has the rest.)
    expect(hasWord(text, s.name + " is stormbound and will not come.") && hasWord(text, "no ship this week.") == (s.day / 7 == today(w) / 7) &&
               text.size() <= 280,
           "posted: " + text);
    expect(count(events, "ships posted", "east") >= 1, "and the notice changed");
    // The shop list's cause, in a player's words and by the ship's name: never the Board's "ship late".
    {
        w.addPlayer("player-ada", "Ada");
        expect(testPurse(soc, "player-ada", 5000), "a purse to price a shelf with");
        std::string cause;
        for (const auto* k : {"em", "es", "ek", "eg", "et"})
            for (const auto& x : soc.wares(k))
                for (const auto& [g, n] : s.brings)
                    if (cause.empty() && items::baseOf(x) == g)
                    {
                        const double was = soc.pressure("east", g);
                        soc.setPressure("east", g, .45, "ship late");
                        if (const auto l = soc.listing("player-ada", k, x); l.trend > 0)
                            cause = l.cause;
                        soc.setPressure("east", g, was, "ship late");
                    }
        expect(cause == s.name + " is stormbound", "dear, and why: " + cause);
    }
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
    // (Five or six: never seven, with nobody at the Wharf on Restday.)
    expect(w.shipsInWeek("east", week) >= 5 && w.shipsInWeek("east", week) <= 6, "five or six a week: " + std::to_string(w.shipsInWeek("east", week)));
    for (const auto& s : w.society().board().ships)
        expect(calendar::weekdayOf(double(s.day)) != calendar::Restday, "never on Restday: " + s.name);
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
    expect(wb.ships.size() == 1 && wb.ships.front().id == s.id && wb.ships.front().status == "in",
           "the ships still due dropped, the one in kept: " + std::to_string(wb.ships.size()));
    expect(hasWord(w.shipsNotice("east"), s.name + " is in") && !hasWord(w.shipsNotice("east"), "due"), "its notice tells of the one in only: " + w.shipsNotice("east"));
    expect(w.setTimeOfDay(std::min(23.5, s.sails + .1)).ok, "its evening");
    run(w, 5);
    events = w.takeEvents();
    expect(count(events, "ship sails", s.id) == 1, "and it sails");
    // Its last word comes down with it: the notice cleared once, said so (the board's is taken down by that), and the
    // port as one that never had ships.
    for (int day = 0; day < 9; ++day)
    {
        nextMorning(w);
        for (auto& e : w.takeEvents())
            events.push_back(e);
    }
    expect(w.society().board().ships.empty() && w.shipsNotice("east").empty(), "no ships while the rule is 0");
    expect(count(events, "ships cleared", "east") == 1 && w.society().board().shipsThrough.empty(),
           "and the last notice taken down, once: " + std::to_string(count(events, "ships cleared", "east")));
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
        std::string scent = "none";
        if (const auto* shown = ash.last("board"))
            for (const auto& x : shown->array("notices"))
                if (x.string("sign") == "the Wharf office" && x.string("text") == n->text)
                    read = true, scent = x.string("scent");
        expect(read, "a player at east's board reads it");
        // (A notice no one wolf wrote: never the masked letter's "It carries no scent at all. Someone took care.")
        expect(scent == "It smells of tar and salt, and of many paws.", "and what a nose makes of it: " + scent);
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
// ------------------------------------------------------------------------------------------- The repair's tests (Phase 5)

void shipsAWeek(World& w, int perWeek)
{
    auto spec = w.society().authored().economy;
    spec.rules["shipsPerWeek"] = perWeek;
    w.society().adoptEconomy(spec);
}

// "No ship this week." is true or not said: only of the week the calendar is in, only when a storm kept a ship of it out,
// and only when no other ship of it came in, is in or is still due.
bool noShipLineIsTrue(const World& w, const std::string& where)
{
    const auto t = today(w);
    bool storm = false, other = false;
    for (const auto& s : w.society().board().ships)
        if (s.port == "east" && s.day / 7 == t / 7)
            (s.status == "storm" ? storm : other) = true;
    const auto text = w.shipsNotice("east");
    const bool said = hasWord(text, "no ship this week");
    expect(said == (storm && !other), "\"No ship this week\" only when it is so (" + where + "): " + text);
    return said;
}
void noShipThisWeek()
{
    // A week with one ship, still to come: the DM's storm of it says nothing of this week; when its week comes, it does.
    {
        auto f = harbour();
        auto w = load(f);
        w.tick(.6);
        market::Ship s;
        for (int day = 0; day < 90 && s.id.empty(); ++day)
        {
            std::map<std::int64_t, int> perWeek;
            for (const auto& x : w.society().board().ships)
                ++perWeek[x.day / 7];
            for (const auto& x : w.society().board().ships)
                if (s.id.empty() && x.status == "due" && x.day / 7 == today(w) / 7 + 1 && perWeek[x.day / 7] == 1 && today(w) % 7 >= 4)
                    s = x;
            if (s.id.empty())
                nextMorning(w);
        }
        expect(!s.id.empty(), "a week to come with one ship");
        expect(w.stormShip(s.id), "a storm keeps next week's ship out");
        auto text = w.shipsNotice("east");
        expect(hasWord(text, s.name + " is stormbound and will not come.") && !hasWord(text, "no ship this week"),
               "next week's storm is no word on this week: " + text);
        int said = 0;
        while (shipById(w, s.id))
        {
            nextMorning(w);
            if (!shipById(w, s.id))
                break;
            text = w.shipsNotice("east");
            const bool itsWeek = today(w) / 7 == s.day / 7;
            expect(hasWord(text, "no ship this week") == itsWeek, std::string(itsWeek ? "said in its week" : "not said before its week") + ": " + text);
            said += itsWeek;
            noShipLineIsTrue(w, "a morning");
        }
        expect(said >= 1, "its week came while it was posted");
    }
    // Weeks of several ships: one stormbound, the others come, and it is never a week with no ship. Through each day (the
    // morning, a ship in, a ship sailed), three weeks of them.
    {
        auto f = harbour();
        auto w = load(f);
        shipsAWeek(w, 3);
        w.tick(.6);
        int storms = 0, shared = 0;
        for (int day = 0; day < 24; ++day)
        {
            nextMorning(w);
            // (The DM's storm: the first ship of each week to come that has another.)
            std::map<std::int64_t, int> perWeek;
            for (const auto& x : w.society().board().ships)
                ++perWeek[x.day / 7];
            std::string pick;
            for (const auto& x : w.society().board().ships)
                if (pick.empty() && x.status == "due" && x.day > today(w) && x.day / 7 == today(w) / 7 + 1 && perWeek[x.day / 7] >= 2 &&
                    today(w) % 7 == 5)
                    pick = x.id;
            if (!pick.empty())
                storms += w.stormShip(pick);
            for (const double hour : {5., 12., 20.5})
            {
                expect(w.setTimeOfDay(hour).ok, "through the day");
                run(w, 3);
                const bool said = noShipLineIsTrue(w, "day " + std::to_string(today(w)) + " at " + std::to_string(int(hour)));
                bool stormNow = false, otherNow = false;
                for (const auto& x : w.society().board().ships)
                    if (x.day / 7 == today(w) / 7)
                        (x.status == "storm" ? stormNow : otherNow) = true;
                shared += stormNow && otherNow && !said;
            }
            w.takeEvents();
        }
        expect(storms >= 2 && shared >= 3, "weeks with a ship stormbound and others that came, never \"no ship\": " + std::to_string(storms) +
                                               " storms, " + std::to_string(shared) + " such readings");
        expect(w.society().realBalanced(), "real money balanced");
    }
}

// A ship in while nobody works (the DM brings one in at night): in, by name, at a quiet pier.
void aShipAtAQuietPier()
{
    auto f = harbour();
    auto w = load(f);
    w.tick(.6);
    nextMorning(w);
    std::vector<market::Ship> due;
    for (const auto& x : w.society().board().ships)
        if (x.status == "due" && x.day > today(w))
            due.push_back(x);
    expect(due.size() >= 2, "ships due");
    const auto s = due[0];
    expect(w.setTimeOfDay(22).ok, "late in the evening");
    run(w, 30);
    w.takeEvents();
    expect(!w.dockShip("s-none"), "no such ship");
    expect(w.dockShip(s.id), "the DM brings " + s.name + " in");
    auto events = w.takeEvents();
    const auto* in = w.shipIn("east");
    expect(in && in->id == s.id && in->day == today(w) && in->sails > 22 && in->sails < 24 && count(events, "ship docks", s.id) == 1,
           "it is in, tonight, and sails before the day is out");
    run(w, 30);
    expect(!w.wharfCrowd(id(2)), "nobody at the Wharf at this hour: no crowd");
    for (const auto* who : {"d1", "d2", "d3", "hm"})
        expect(w.entity(who) && w.entity(who)->activity.find(s.name) == std::string::npos,
               std::string(who) + " not at its cargo at night: " + (w.entity(who) ? w.entity(who)->activity : std::string()));
    std::map<std::string, std::string> facts;
    w.talkTown("d1", facts);
    expect(facts["world.ships"] == "in" && facts["world.ship_name"] == s.name, "talk knows it is in: " + facts["world.ships"]);
    const auto text = w.shipsNotice("east");
    expect(hasWord(text, s.name + " is in with ") && hasWord(text, "and sails this evening") && !hasWord(text, "unloading") && text.size() <= 280,
           "the notice: " + text);
    expect(!w.dockShip(due[1].id) && !w.dockShip(s.id), "not a second while one is in, nor the same one twice");
    expect(w.setTimeOfDay(23.85).ok, "its hour to sail");
    run(w, 10);
    events = w.takeEvents();
    expect(count(events, "ship sails", s.id) == 1 && !w.shipIn("east"), "it sails that night");
    expect(!w.dockShip(due[1].id), "nor a second ship the same day");
    expect(w.society().realBalanced() && w.society().unregisteredFaucets() == 0, "real money balanced, no unregistered faucet");
}

// The rule at 0 with ships only due: they are dropped and the notice comes down at once, once.
void theRuleStopsTheShips()
{
    auto f = harbour();
    const auto root = onDisk(f, "port-rule0");
    {
        game::Game g(optionsOn(root));
        std::string problem;
        expect(g.start(problem), "a game starts on the harbour: " + problem);
        auto& w = g.world();
        w.tick(.6);
        w.tick(.6);
        expect(!w.society().board().ships.empty() && !w.shipIn("east") && g.documents().find("ships-east"), "ships due, none in, their notice up");
        w.takeEvents();
        shipsAWeek(w, 0);
        w.tick(.6);
        w.tick(.6);
        auto events = w.takeEvents();
        expect(w.society().board().ships.empty() && w.shipsNotice("east").empty() && w.society().board().shipsThrough.empty(),
               "the ships still due dropped: " + std::to_string(w.society().board().ships.size()));
        expect(count(events, "ships cleared", "east") == 1, "the notice's end told once: " + std::to_string(count(events, "ships cleared", "east")));
        expect(!g.documents().find("ships-east"), "and the notice is down at once, not left up a week");
        std::map<std::string, std::string> facts;
        w.talkTown("d1", facts);
        expect(facts["world.ships"] == "no", "talk has no ship: " + facts["world.ships"]);
        int more = 0;
        for (int day = 0; day < 3; ++day)
        {
            nextMorning(w);
            for (const auto& e : w.takeEvents())
                more += e.kind == "ships cleared" || e.kind == "ships posted" || e.kind == "ship posted";
        }
        expect(more == 0 && !g.documents().find("ships-east"), "and nothing more of ships: " + std::to_string(more));
        shipsAWeek(w, 2);
        w.tick(.6);
        w.tick(.6);
        expect(!w.society().board().ships.empty() && g.documents().find("ships-east"), "the rule set back: a timetable and its notice at once");
    }
    std::filesystem::remove_all(root);
}

// What makes a port (8.3): a post of the Wharf's own words, at the market its work room is in or nearest to.
void whatMakesAPort()
{
    // A town with a long store and its carriers, and no wharf: no port (the sea trade's other posts make none alone).
    {
        auto f = harbour(true, true,
                         resident("x1", "Stow One", "civilian", "tallying in the long store", 7, 12.5, 8, 4.5) +
                             resident("x2", "Stow Two", "civilian", "carrying from the long store", 7, 13.5, 8, 5.5) +
                             resident("x3", "Stow Three", "civilian", "keeping the new store", 7, 14.5, 8, 6.5));
        auto w = load(f);
        w.tick(.6);
        expect(w.ports().size() == 1 && w.port("east") && !w.port("west"), "west's stores make it no port: " + std::to_string(w.ports().size()));
        expect(w.port("east")->posts == 4, "and east's posts are its own: " + std::to_string(w.port("east")->posts));
        for (const auto& s : w.society().board().ships)
            expect(s.port == "east", "no ship for west");
        expect(w.shipsNotice("west").empty() && !w.wharfCrowd(id(8)), "no timetable at west");
    }
    // A tannery and a smokehouse with "Wharf" in their names are no Wharf, nor is the Watch's round of it: no port at
    // west for them, and no posts of east's (so none of them is shown seeing to a ship's cargo).
    {
        auto f = harbour(true, true,
                         resident("t1", "Tan One", "civilian", "helping at The Wharf Tannery", 7, 12.5, 8, 4.5) +
                             resident("t2", "Smoke Two", "civilian", "works at The North Wharf Smokehouse", 7, 13.5, 8, 5.5) +
                             resident("t3", "Tan Three", "civilian", "helping at The Wharf Tannery", 0, 12.5, 2, 8.5) +
                             resident("t4", "Watch Four", "civilian", "on patrol on the wharf", 0, 13.5, 2, 9.5));
        auto w = load(f);
        w.tick(.6);
        expect(w.ports().size() == 1 && w.port("east") && !w.port("west"), "a business by the water makes no port: " + std::to_string(w.ports().size()));
        expect(w.port("east")->posts == 4, "nor is its hand a dock hand: " + std::to_string(w.port("east")->posts));
        for (const auto& s : w.society().board().ships)
            expect(s.port == "east", "no ship for a tannery");
    }
    // A hand of east's Wharf who lives at west: one Wharf, one port.
    {
        auto f = harbour(true, true, resident("x4", "Far Hand", "civilian", "unloading ships at the Wharf", 7, 12.5, 2, 8.5));
        auto w = load(f);
        w.tick(.6);
        expect(w.ports().size() == 1 && w.port("east") && !w.port("west"), "one port, wherever its hands live: " + std::to_string(w.ports().size()));
        expect(w.port("east")->posts == 5 && w.port("east")->cell == id(2), "the hand is east's: " + std::to_string(w.port("east")->posts));
        for (const auto& s : w.society().board().ships)
            expect(s.port == "east", "no ship for west");
    }
    // A Wharf out of town, in a cell that is no market's: its posts are the nearest market's by the road.
    {
        // (One of its hands lives at west: still east's Wharf, never a port of each hand's home.)
        auto f = harbour(true, false,
                         resident("d1", "Dock One", "civilian", "unloading ships at the Wharf", 0, 12.5, 3, 4.5) +
                             resident("hm", "Hale Marsh", "civilian", "harbourmaster", 0, 1.5, 3, 7.5) +
                             resident("x5", "Far Crane", "civilian", "working the cranes at the Wharf", 7, 12.5, 3, 8.5));
        auto w = load(f);
        w.tick(.6);
        expect(w.ports().size() == 1 && w.port("east") && w.port("east")->cell == id(3) && w.port("east")->posts == 3 && !w.port("west"),
               "a Wharf outside every market's cells is the nearest market's, whole: " + std::to_string(w.ports().size()) + " at " +
                   (w.port("east") ? w.port("east")->cell : std::string("none")));
        for (const auto& s : w.society().board().ships)
            expect(s.port == "east", "one Wharf, one timetable: none for " + s.port);
        expect(!w.society().board().ships.empty() && !w.shipsNotice("east").empty(), "and its ships are posted there");
        auto g = harbour(true, false,
                         resident("d1", "Dock One", "civilian", "unloading ships at the Wharf", 7, 12.5, 6, 4.5) +
                             resident("hm", "Hale Marsh", "civilian", "harbourmaster", 7, 1.5, 6, 7.5));
        auto v = load(g);
        v.tick(.6);
        expect(v.ports().size() == 1 && v.port("west") && v.port("west")->cell == id(6), "or west's, nearer west: " + std::to_string(v.ports().size()));
    }
}

// A hand-made timetable into a world, as a save would bring it (the week's roll kept, so nothing is rolled over it).
market::Ship aShip(const std::string& shipId, const std::string& name, std::int64_t day, const std::string& status)
{
    market::Ship s;
    s.id = shipId;
    s.port = "east";
    s.name = name;
    s.day = day;
    s.hour = 9;
    s.sails = 18;
    s.status = status;
    s.brings = {{"butter", 2}};
    return s;
}
void withShips(World& w, std::vector<market::Ship> ships, double hour = -1)
{
    auto saved = w.save();
    saved.society.board.ships = std::move(ships);
    saved.society.board.shipsThrough["east"] = today(w) / 7 + 1;
    if (hour >= 0)
        saved.calendarDays = std::floor(saved.calendarDays) + hour / 24;
    expect(w.restore(saved).ok, "a timetable restored");
}

// Talk's ship, of two of a kind: the first that is due, the latest that sailed (today's, not yesterday's).
void whichShipTalkNames()
{
    auto f = harbour();
    auto w = load(f);
    w.tick(.6);
    for (int day = 0; day < 3; ++day)
        nextMorning(w);
    const auto t = today(w);
    for (const bool flip : {false, true})
    {
        const auto gull = aShip("s901", "the Gull", t + 1, "due"), heron = aShip("s902", "the Heron", t + 2, "due");
        withShips(w, flip ? std::vector<market::Ship>{heron, gull} : std::vector<market::Ship>{gull, heron});
        std::map<std::string, std::string> facts;
        w.talkTown("d1", facts);
        expect(facts["world.ships"] == "due" && facts["world.ship_name"] == "the Gull", "the first due is the one talked of: " + facts["world.ship_name"]);
    }
    for (const bool flip : {false, true})
    {
        const auto gull = aShip("s901", "the Gull", t - 1, "sailed"), heron = aShip("s902", "the Heron", t, "sailed");
        withShips(w, flip ? std::vector<market::Ship>{heron, gull} : std::vector<market::Ship>{gull, heron}, 20);
        std::map<std::string, std::string> facts;
        w.talkTown("d1", facts);
        expect(facts["world.ships"] == "sailed" && facts["world.ship_name"] == "the Heron",
               "today's ship is the one that sailed, not yesterday's: " + facts["world.ship_name"]);
        const auto text = w.shipsNotice("east");
        expect(hasWord(text, "the Gull sailed yesterday") && hasWord(text, "the Heron sailed today"), "and the notice has both: " + text);
    }
}

// The notice in words at every rule (8.3): inside 280 characters, every ship due in seven days named or counted, a ship in
// and a ship stormbound always told of, never half a cargo, never nothing while a ship is due.
struct NoticeSeen
{
    int counted = 0, bare = 0, full = 0, next = 0, readings = 0;
    std::size_t storms = 0;                         // The most ships stormbound on the notice at once.
};
void noticeHolds(const World& w, NoticeSeen& seen, const std::string& where)
{
    const auto text = w.shipsNotice("east");
    const auto t = today(w);
    const auto say = [&](const std::string& what) { return what + " (" + where + "): " + text; };
    const auto cargo = [](const std::vector<std::pair<std::string, int>>& goods) {
        std::vector<std::string> words;
        for (const auto& g : goods)
            words.push_back(World::cargoWords(g.first));
        return seeded::listed(words);
    };
    std::vector<const market::Ship*> due, storms, later;
    const market::Ship* in = nullptr;
    for (const auto& s : w.society().board().ships)
    {
        if (s.port != "east")
            continue;
        if (s.status == "due" && s.day < t + 7)
            due.push_back(&s);
        else if (s.status == "due")
            later.push_back(&s);
        else if (s.status == "storm")
            storms.push_back(&s);
        else if (s.status == "in")
            in = &s;
    }
    ++seen.readings;
    expect(text.size() <= 280 && !market::forbiddenWord(text), say("inside a notice's 280 characters"));
    if (due.empty() && later.empty() && storms.empty() && !in)
        return;
    expect(!text.empty() && text.rfind("Ships at East.", 0) == 0 && text.back() == '.', say("a notice while there is a ship to tell of, whole sentences"));
    if (in)
        expect(hasWord(text, in->name + " is in") && hasWord(text, "and sails this evening"), say("the ship in"));
    // Every ship due these seven days: by name, or in a count that says how many and by when.
    int named = 0, counted = 0;
    for (const auto* s : due)
        named += text.find(s->name + (s->day == t ? " today" : " on " + calendar::weekdayName(calendar::weekdayOf(double(s->day))))) != std::string::npos;
    for (const char* lead : {", and ", " Due: "})
        for (auto at = text.find(lead); at != std::string::npos; at = text.find(lead, at + 1))
        {
            const auto digits = at + std::string(lead).size();
            if (digits < text.size() && std::isdigit(static_cast<unsigned char>(text[digits])))
            {
                counted = std::atoi(text.c_str() + digits);
                const auto tail = text.substr(digits, 24);
                expect(tail.find(lead[1] == 'D' ? (counted == 1 ? " ship by " : " ships by ") : " more by ") != std::string::npos, say("a count and by when"));
            }
        }
    expect(named + counted == int(due.size()), say("every ship due in seven days named or counted: " + std::to_string(named) + " + " +
                                                    std::to_string(counted) + " of " + std::to_string(due.size())));
    seen.counted += counted > 0;
    // Never half a cargo: a ship told of "with" goods is told of with all it brings.
    for (const auto* s : due)
    {
        const auto lead = s->name + (s->day == t ? " today" : " on " + calendar::weekdayName(calendar::weekdayOf(double(s->day))));
        const auto at = text.find(lead);
        if (at == std::string::npos)
            continue;
        const auto after = at + lead.size();
        if (text.compare(after, 6, " with ") != 0)
        {
            ++seen.bare;
            expect(text[after] == ';' || text[after] == '.' || text[after] == ',' || text.compare(after, 8, " taking ") == 0, say("a ship by name and day"));
            continue;
        }
        const auto whole = cargo(s->brings);
        expect(text.compare(after + 6, whole.size(), whole) == 0, say("all of " + s->name + "'s cargo (" + whole + ") or none"));
        const auto end = after + 6 + whole.size();
        expect(text[end] == ';' || text[end] == '.' || text[end] == ',', say("and no more than its cargo"));
        ++seen.full;
        if (text.compare(end, 9, ", taking ") == 0)
        {
            const auto takes = cargo(s->takes);
            expect(text.compare(end + 9, takes.size(), takes) == 0 && (text[end + 9 + takes.size()] == ';' || text[end + 9 + takes.size()] == '.'),
                   say("all it takes (" + takes + ") or none"));
        }
    }
    // The stormbound: each by name, or counted.
    seen.storms = std::max(seen.storms, storms.size());
    if (!storms.empty())
    {
        bool all = true;
        for (const auto* s : storms)
            all = all && hasWord(text, s->name);
        expect(hasWord(text, "stormbound and will not come.") &&
                   (all || hasWord(text, std::to_string(storms.size()) + (storms.size() == 1 ? " ship is stormbound" : " ships are stormbound"))),
               say("the stormbound told of"));
    }
    // Nothing due these seven days, one after: the next, by name and day.
    if (due.empty() && !later.empty())
    {
        const auto* next = *std::min_element(later.begin(), later.end(), [](const market::Ship* a, const market::Ship* b) { return a->day < b->day; });
        expect(hasWord(text, "The next is " + next->name + ", on " + calendar::weekdayName(calendar::weekdayOf(double(next->day)))), say("the next ship"));
        ++seen.next;
    }
}
void theNoticeAtEveryRule()
{
    for (const int rule : {1, 2, 4, 5, 6})
    {
        auto f = harbour();
        auto w = load(f);
        shipsAWeek(w, rule);
        w.tick(.6);
        NoticeSeen seen;
        int storms = 0;
        for (int day = 0; day < (rule == 1 ? 63 : 21); ++day)
        {
            nextMorning(w);
            // (Several storms at once, at the rules with many ships: the last three due, on three mornings.)
            if (rule >= 4 && (day == 4 || day == 11 || day == 12))
                for (int n = 0; n < 3; ++n)
                {
                    const market::Ship* last = nullptr;
                    for (const auto& x : w.society().board().ships)
                        if (x.status == "due" && x.day > today(w) && x.day < today(w) + 6 && (!last || x.day > last->day))
                            last = &x;
                    if (last)
                        storms += w.stormShip(std::string(last->id));
                }
            for (const double hour : {5., 12., 21.})
            {
                expect(w.setTimeOfDay(hour).ok, "through the day");
                run(w, 3);
                noticeHolds(w, seen, "rule " + std::to_string(rule) + ", day " + std::to_string(today(w)) + " at " + std::to_string(int(hour)));
            }
            w.takeEvents();
        }
        const auto told = "rule " + std::to_string(rule) + ": " + std::to_string(seen.readings) + " readings, " + std::to_string(seen.full) +
                          " cargoes, " + std::to_string(seen.bare) + " by name, " + std::to_string(seen.counted) + " counted, " +
                          std::to_string(seen.next) + " next, " + std::to_string(storms) + " storms, " + std::to_string(seen.storms) + " at once";
        expect(seen.full > 0 && (rule < 4 || (storms >= 5 && seen.storms >= 3)), told);
        if (rule >= 5)
            expect(seen.bare + seen.counted > 0, "a full week's timetable is shortened, never cut: " + told);
        expect(w.society().realBalanced(), "real money balanced");
    }
    // Between ships (the rule at one a week, the week's ship gone and the next more than seven days off): the notice
    // stays, with the next by name.
    {
        auto f = harbour();
        auto w = load(f);
        shipsAWeek(w, 1);
        w.tick(.6);
        for (int day = 0; day < 3; ++day)
            nextMorning(w);
        const auto t = today(w);
        const auto weekStart = t / 7 * 7;
        withShips(w, {aShip("s901", "the Gull", weekStart + 7 + 5, "due")});
        w.tick(.6);
        NoticeSeen seen;
        noticeHolds(w, seen, "between ships");
        const auto text = w.shipsNotice("east");
        expect(seen.next == 1 && text == "Ships at East. The next is the Gull, on " + calendar::weekdayName(5) + " of next week.", "between ships: " + text);
    }
}

// A cargo in words (the notice, the docks' news): things by the piece in the plural, measures as they are.
void cargoInWords()
{
    const std::map<std::string, std::string> words{
        {"butter", "butter"}, {"wheat", "wheat"}, {"copper_ore", "copper ore"}, {"hide", "hides"}, {"small_pelt", "small pelts"},
        {"handbell", "handbells"}, {"sheepskin", "sheepskins"}, {"sheep", "sheep"}, {"offal", "offal"}, {"resin", "pine resin"},
        {"horse_tack", "horse harness"}, {"eggs", "eggs"}, {"bones", "bones"}, {"berries", "berries"}, {"honey", "honey"}};
    for (const auto& [g, want] : words)
        expect(World::cargoWords(g) == want, g + " as cargo: " + World::cargoWords(g) + ", not " + want);
    const auto& d = market::data();
    for (const auto* list : {&d.shipBrings, &d.shipTakes})
        for (const auto& g : *list)
        {
            const auto said = World::cargoWords(g);
            expect(!said.empty() && said.find('_') == std::string::npos && said.find('(') == std::string::npos && !market::forbiddenWord(said),
                   g + " in a cargo's words: " + said);
        }
    // The forbidden word (the user's rule), whole and in any case; never a word that only holds it.
    for (const auto* bad : {"the Sea Dog", "Dogs", "the dog-watch", "a DOG.", "sea dogs' rest"})
        expect(market::forbiddenWord(bad), std::string("forbidden: ") + bad);
    for (const auto* fine : {"the Dogfish", "dogged", "the Bulldog", "endogamy", "the Gull", ""})
        expect(!market::forbiddenWord(fine), std::string("fine: ") + fine);
    // A name is held to more (Phases 4 and 5 merged): the word anywhere in it, so a compound made with it is out too.
    for (const auto* bad : {"the Seadog", "the Sea Dog", "the Dogfish", "the Bulldog", "SEADOGS", "the Old dOg"})
        expect(market::forbiddenInName(bad) && !market::shipNameFits(bad), std::string("no name for a ship: ") + bad);
    for (const auto* fine : {"the Gull", "the Good Tide", "the Dragon", "do", ""})
        expect(!market::forbiddenInName(fine), std::string("a name: ") + fine);
    // A host's own list of ships (market.json's `wharf.ships`) is held to it by the loader, as the game's list is.
    const auto names = market::shipNamesOf(parsed(
        R"(["the Gull", "the Sea Dog", "the Old Dogs", "the Gull", "the Dogfish", "the Seadog", "the 9th Wave", "", 7, "the Salt-Rose"])").items());
    expect(names == std::vector<std::string>{"the Gull", "the Salt-Rose"}, "a host's ships, checked: " + seeded::listed(names));
    for (const auto& name : market::data().shipNames)
        expect(market::shipNamesOf({json::Value(name)}).size() == 1, "the game's own pass: " + name);
}

// A timetable at its bound: a week's ships are still rolled, the oldest gone making room.
void aFullTimetable()
{
    auto f = harbour();
    auto w = load(f);
    shipsAWeek(w, 6);
    w.tick(.6);
    for (int day = 0; day < 15; ++day)
    {
        nextMorning(w);
        const auto week = today(w) / 7 + 1;
        int n = 0;
        for (const auto& s : w.society().board().ships)
        {
            n += s.day / 7 == week;
            expect(calendar::weekdayOf(double(s.day)) != calendar::Restday, "never a ship on Restday: " + s.name);
        }
        expect(n == w.shipsInWeek("east", week) && n >= 5 && n <= 6, "next week's ships all posted: " + std::to_string(n) + " of " +
                                                                         std::to_string(w.shipsInWeek("east", week)));
        w.takeEvents();
    }
    auto saved = w.save();
    auto& b = saved.society.board;
    const auto t = today(w), week = t / 7;
    b.ships.erase(std::remove_if(b.ships.begin(), b.ships.end(), [&](const market::Ship& s) { return s.day / 7 > week; }), b.ships.end());
    b.shipsThrough["east"] = week;
    for (int n = 0; b.ships.size() < market::ShipsAPort; ++n)
        b.ships.push_back(aShip("s9" + std::to_string(n), "the Pad " + std::string(1, char('A' + n)), t - 1 - n % 2, "storm"));
    expect(b.ships.size() == market::ShipsAPort, "a full timetable");
    const auto kept = std::count_if(b.ships.begin(), b.ships.end(), [](const market::Ship& s) { return s.status == "due" || s.status == "in"; });
    expect(w.restore(saved).ok, "restored full");
    w.tick(.6);
    int next = 0;
    long still = 0;
    for (const auto& s : w.society().board().ships)
        next += s.day / 7 == week + 1, still += s.day / 7 <= week && (s.status == "due" || s.status == "in");
    expect(next == w.shipsInWeek("east", week + 1) && next >= 5, "the week to come is rolled all the same: " + std::to_string(next) + " of " +
                                                                    std::to_string(w.shipsInWeek("east", week + 1)));
    expect(w.society().board().ships.size() <= market::ShipsAPort && still == kept, "inside the bound, and no ship still to come lost: " +
                                                                                       std::to_string(w.society().board().ships.size()));
}

// A restart on ship day: a load leaves the shelves and the back rooms as they were saved (a shelf players bought bare
// stays bare till its morning: nothing is set out again at a load, as Phase 5's repair did to mend a crash; the tills a
// ship fills are journalled with the Board now, shipDayThroughACrash), and the ship in docks once.
void aRestartOnShipDay()
{
    auto f = harbour();
    auto w = load(f);
    w.tick(.6);
    auto& soc = w.society();
    const auto s = toShipDay(w);
    dock(w, s);
    const auto port = *w.port("east");
    const auto rooms = [&](const market::Board& b) {
        int n = 0;
        for (const auto& [till, goods] : b.backRoom)
            for (const auto& [g, pieces] : goods)
                if (std::find(port.imports.begin(), port.imports.end(), g) != port.imports.end())
                    n += pieces;
        return n;
    };
    // (Players buy a shelf of what it brought bare.)
    std::vector<std::pair<std::string, std::string>> bare;
    for (const auto* k : {"em", "es", "ek", "eg", "et"})
        for (const auto& ware : soc.stagedShelfGoods(k))
            if (std::find(port.imports.begin(), port.imports.end(), items::baseOf(ware)) != port.imports.end() && stock(w, k, ware) > 0 && bare.size() < 3)
            {
                soc.consume(soc.tillOf(k), ware, stock(w, k, ware), "test: sold out");
                bare.push_back({k, ware});
            }
    const int before = rooms(soc.board());
    expect(!bare.empty() && before > 0, "the ship's goods behind, a shelf of them sold bare: " + std::to_string(before) + " behind");
    const auto stood = shipsOf(w);
    auto saved = w.save();
    w.takeEvents();
    expect(w.restore(saved).ok, "the restart, the ship in");
    expect(w.shipIn("east") && w.shipIn("east")->id == s.id, "the ship is in");
    for (const auto& [k, ware] : bare)
        expect(stock(w, k, ware) == 0, k + "'s " + ware + " as saved, not set out again: " + std::to_string(stock(w, k, ware)));
    expect(rooms(soc.board()) == before, "the back rooms as saved: " + std::to_string(rooms(soc.board())) + " behind of " + std::to_string(before));
    w.tick(.6);
    const auto events = w.takeEvents();
    expect(count(events, "ship docks", s.id) == 0, "docked once: " + std::to_string(count(events, "ship docks", s.id)));
    expect(shipsOf(w) == stood, "the timetable as saved");
    expect(rooms(soc.board()) == before, "and the back rooms a pass of the roads on");
    expect(soc.realBalanced(), "real money balanced");
    expect(soc.unregisteredFaucets() == 0, "no unregistered faucet: " + std::to_string(soc.unregisteredFaucets()));
}

// A jump of the calendar (a host's /time, a save from days ago): the ships missed are gone quietly, their goods on the
// shelves once; none docks days late, none is left due in the past.
void aCalendarJump()
{
    auto f = harbour();
    auto w = load(f);
    shipsAWeek(w, 6);
    w.tick(.6);
    nextMorning(w);
    auto& soc = w.society();
    const auto port = *w.port("east");
    w.takeEvents();
    expect(w.advanceCalendar(10).ok, "ten days on");
    w.tick(.6);
    w.tick(.6);
    const auto events = w.takeEvents();
    const auto t = today(w);
    expect(count(events, "ship docks") <= 2 && count(events, "ship sails") <= 2,
           "no pile of ten days' ships docking at once: " + std::to_string(count(events, "ship docks")) + " docked");
    int in = 0;
    for (const auto& s : soc.board().ships)
    {
        expect(s.status != "due" || s.day >= t, "none left due in the past: " + s.name + " of day " + std::to_string(s.day) + " on " + std::to_string(t));
        expect(s.day >= t - 7, "nor kept from before the jump: " + s.name);
        in += s.status == "in";
    }
    const auto week = t / 7;
    int next = 0;
    for (const auto& s : soc.board().ships)
        next += s.day / 7 == week + 1;
    expect(in <= 1 && next == w.shipsInWeek("east", week + 1) && soc.board().shipsThrough.at("east") == week + 1,
           "the timetable rolled on from the new day: " + std::to_string(next));
    for (const auto* k : {"em", "es", "ek", "eg", "et"})
        for (const auto& ware : soc.stagedShelfGoods(k))
            if (ware == items::baseOf(ware) && std::find(port.imports.begin(), port.imports.end(), ware) != port.imports.end())
                expect(stock(w, k, ware) >= std::min(1, soc.shelfTarget(soc.tillOf(k), ware)), std::string(k) + "'s " + ware + " came by the ships missed");
    noShipLineIsTrue(w, "after the jump");
    expect(w.shipsNotice("east").size() <= 280 && soc.realBalanced() && soc.unregisteredFaucets() == 0, "real money balanced, no unregistered faucet");
}

// A day turned by ticks alone (no restart, no jump): the port's morning comes with it.
void aDayTurnedByTicks()
{
    auto f = harbour();
    auto w = load(f);
    w.tick(.6);
    while (today(w) % 7 != 6)
        nextMorning(w);
    const auto day = today(w), week = day / 7;
    expect(w.society().board().shipsThrough.at("east") == week + 1, "rolled through next week");
    expect(w.setTimeOfDay(23.95).ok, "just before midnight");
    w.tick(.6);
    w.takeEvents();
    run(w, 90);
    const auto events = w.takeEvents();
    expect(today(w) == day + 1, "midnight passed by ticks");
    int after = 0;
    for (const auto& s : w.society().board().ships)
        after += s.day / 7 == week + 2;
    expect(w.society().board().shipsThrough.at("east") == week + 2 && after == w.shipsInWeek("east", week + 2) && after >= 1,
           "the new week's morning rolls the week after: " + std::to_string(after));
    expect(count(events, "ships posted", "east") >= 1 && count(events, "ship posted") == after, "posted afresh that morning");
}

// What a ship brings and takes is its port's and its day's: the same whatever was rolled before it.
void cargoByTheDay()
{
    auto f = harbour();
    auto a = load(f);
    a.tick(.6);
    auto saved = a.save();
    saved.society.board.ships.clear();
    saved.society.board.shipsThrough.clear();
    saved.society.board.nextShip = 41;
    auto b = load(f);
    expect(b.restore(saved).ok, "the same world, its ships' counter elsewhere");
    b.tick(.6);
    const auto& first = a.society().board().ships;
    const auto& second = b.society().board().ships;
    expect(!first.empty() && first.size() == second.size(), "as many ships: " + std::to_string(first.size()) + " and " + std::to_string(second.size()));
    for (const auto& s : first)
    {
        const auto same = std::find_if(second.begin(), second.end(), [&](const market::Ship& o) { return o.day == s.day; });
        expect(same != second.end() && same->id != s.id, "a ship that day, counted on: " + s.name);
        expect(same->name == s.name && same->hour == s.hour && same->sails == s.sails && same->brings == s.brings && same->takes == s.takes,
               s.name + " the same ship with the same cargo");
    }
}

// A save that says what cannot be: a ship in or sailed on a day still to come is due again; a ship twice is one ship.
void aSaveThatLies()
{
    auto f = harbour();
    auto w = load(f);
    w.tick(.6);
    nextMorning(w);
    auto saved = w.save();
    std::vector<std::string> ahead;
    for (auto& s : saved.society.board.ships)
        if (s.status == "due" && s.day > today(w) && ahead.size() < 2)
        {
            s.status = ahead.empty() ? "in" : "sailed";
            ahead.push_back(s.id);
        }
    expect(ahead.size() == 2, "two ships to come");
    expect(w.restore(saved).ok, "a save with a ship in on a day to come");
    expect(shipById(w, ahead[0])->status == "due" && shipById(w, ahead[1])->status == "due" && !w.shipIn("east") && !w.wharfCrowd(id(2)),
           "they are due again: " + shipById(w, ahead[0])->status + ", " + shipById(w, ahead[1])->status);
    std::map<std::string, std::string> facts;
    w.talkTown("d1", facts);
    expect(facts["world.ships"] != "in" && facts["world.ships"] != "sailed", "and talk has none in: " + facts["world.ships"]);
    w.tick(.6);
    expect(shipById(w, ahead[0])->status == "due" && !w.shipIn("east") && !hasWord(w.shipsNotice("east"), " is in"), "and stay so");
    // A timetable with a ship twice (its id, or its port and day): one is kept.
    auto board = w.society().board();
    const auto real = board.ships.size();
    auto twin = board.ships.front();
    twin.day += 40;
    twin.name = "the Twin";
    board.ships.push_back(twin);
    auto sameDay = board.ships.front();
    sameDay.id = "s777";
    sameDay.name = "the Shadow";
    board.ships.push_back(sameDay);
    const auto back = market::decode(market::encode(board));
    std::set<std::string> ids;
    std::set<std::pair<std::string, std::int64_t>> days;
    for (const auto& s : back.ships)
        expect(ids.insert(s.id).second && days.insert({s.port, s.day}).second, "one ship of an id, one a port a day: " + s.id);
    expect(back.ships.size() == real && back.ships.front().name == board.ships.front().name, "the first kept: " + std::to_string(back.ships.size()));
}

// A timetable a save lies about (Phases 4 and 5 merged: leftovers B and C of Phase 5's last check). Whatever a save says,
// the game keeps a timetable it could have rolled: the next ship's id above every id there; a name the loader would
// take; a cargo of goods a ship may bring, in sane numbers; sixteen ships a port; none of a day past the weeks rolled.
void aTimetableThatLies()
{
    auto f = harbour();
    auto w = load(f);
    w.tick(.6);
    nextMorning(w);
    const auto t = today(w), week = t / 7;
    const auto idsOnce = [](const market::Board& b, const std::string& what) {
        std::set<std::string> ids;
        std::set<std::pair<std::string, std::int64_t>> days;
        for (const auto& s : b.ships)
            expect(ids.insert(s.id).second && days.insert({s.port, s.day}).second, what + ": one ship of an id, one a port a day: " + s.id);
    };
    const auto number = [](const std::string& shipId) { return std::int64_t(std::stoll(shipId.substr(1))); };
    // B. The next id below those there: raised at the load (the state's, and a decoded save's), so the week rolled next
    // gives no id twice.
    {
        auto saved = w.save();
        auto& b = saved.society.board;
        expect(b.ships.size() >= 2 && b.nextShip > 2, "a timetable to lie about");
        std::int64_t highest = 0;
        for (const auto& s : b.ships)
            highest = std::max(highest, number(s.id));
        b.nextShip = 1;
        expect(market::decode(market::encode(b)).nextShip == highest + 1, "a decoded save's next id is above its ships': " +
                                                                              std::to_string(market::decode(market::encode(b)).nextShip));
        auto v = load(f);
        expect(v.restore(saved).ok && v.society().board().nextShip == highest + 1, "and a restored state's: " +
                                                                                       std::to_string(v.society().board().nextShip));
        const auto had = v.society().board().ships.size();
        for (int day = 0; day < 8; ++day)
            nextMorning(v);
        idsOnce(v.society().board(), "a week on");
        std::int64_t newest = 0;
        for (const auto& s : v.society().board().ships)
            newest = std::max(newest, number(s.id));
        expect(newest > highest && had > 0, "new ships posted, each with an id of its own: s" + std::to_string(newest));
    }
    // C. A ship's name, its cargo, its day.
    {
        auto saved = w.save();
        auto& b = saved.society.board;
        std::vector<std::string> due;
        for (const auto& s : b.ships)
            if (s.status == "due" && s.day > t)
                due.push_back(s.id);
        expect(due.size() >= 3, "three ships to come: " + std::to_string(due.size()));
        const auto of = [&](const std::string& shipId) -> market::Ship& {
            return *std::find_if(b.ships.begin(), b.ships.end(), [&](const market::Ship& s) { return s.id == shipId; });
        };
        of(due[0]).name = "the Seadog";
        of(due[0]).brings = {{"cheese", 5}, {"butter", 1000000}, {"butter", 3}, {"no_such_good", 2}, {"wheat", -4}, {"eggs", 0}, {"cider", 7}};
        of(due[0]).takes = {{"hide", 2000000000}, {"hide", 1}, {"cheese", 2}};
        of(due[0]).hour = 99;
        of(due[0]).sails = -3;
        of(due[1]).name = "<b>the 9th Wave</b>";
        of(due[2]).status = "storm";                // (Inside the weeks rolled: a storm called ahead reads the same. Kept.)
        b.ships.push_back(aShip("s900", "the Far Storm", (week + 2) * 7 + 30, "storm"));
        b.ships.push_back(aShip("s901", "the Far Ship", (week + 2) * 7, "due"));
        b.ships.push_back(aShip("s902", "the Edge", (week + 2) * 7 - 1, "due"));
        b.shipsThrough["east"] = week + 9;
        const auto dayOfEdge = (week + 2) * 7 - 1;
        const bool edgeTaken = std::any_of(b.ships.begin(), b.ships.end() - 1, [&](const market::Ship& s) { return s.port == "east" && s.day == dayOfEdge; });
        auto v = load(f);
        expect(v.restore(saved).ok, "a save that lies of its ships is restored");
        const auto& now = v.society().board();
        const auto* first = shipById(v, due[0]);
        expect(first && market::shipNameFits(first->name) && first->name != "the Seadog", "a name with the word in it: another: " +
                                                                                                (first ? first->name : std::string("gone")));
        const auto& listed = market::data().shipNames;
        expect(first && std::find(listed.begin(), listed.end(), first->name) != listed.end(), "one of the list's");
        expect(first && first->brings == std::vector<std::pair<std::string, int>>{{"butter", 999}, {"cider", 7}},
               "its cargo: no cheese, no good twice, none unknown, none of no pieces, 999 at most: " + std::to_string(first ? first->brings.size() : 0));
        expect(first && first->takes == std::vector<std::pair<std::string, int>>{{"hide", 999}, {"cheese", 2}}, "what it takes, the same (cheese may leave)");
        expect(first && first->hour >= 0 && first->hour <= 23.99 && first->sails >= first->hour && first->sails <= 23.99, "its hours inside the day");
        const auto* second = shipById(v, due[1]);
        expect(second && market::shipNameFits(second->name) && second->name != (first ? first->name : std::string()), "a name no list would take: another, its own");
        for (const auto& s : now.ships)
            expect(market::shipNameFits(s.name) && std::count_if(now.ships.begin(), now.ships.end(), [&](const market::Ship& o) {
                       return o.port == s.port && o.name == s.name;
                   }) == 1, "each ship a fit name, once at its port: " + s.name);
        expect(shipById(v, due[2]) && shipById(v, due[2])->status == "storm", "a storm on a day to come, inside the weeks rolled: kept (one called ahead)");
        expect(!shipById(v, "s900") && !shipById(v, "s901"), "no ship of a day past the weeks rolled");
        expect(edgeTaken || shipById(v, "s902"), "the last day of next week is inside them");
        expect(now.shipsThrough.at("east") == week + 1, "and no week rolled further than the next: " + std::to_string(now.shipsThrough.at("east")));
        expect(!market::forbiddenInName(v.shipsNotice("east")), "the notice: " + v.shipsNotice("east"));
        idsOnce(now, "restored");
        v.tick(.6);
        for (int day = 0; day < 8; ++day)
            nextMorning(v);
        int next = 0;
        for (const auto& s : v.society().board().ships)
            next += s.day / 7 == today(v) / 7 + 1;
        expect(next == v.shipsInWeek("east", today(v) / 7 + 1) && next >= 1, "and the weeks roll on from the calendar: " + std::to_string(next));
        idsOnce(v.society().board(), "a week on");
    }
    // Sixteen a port, whatever a save holds (decode's bound, and a restored state's): the oldest sailed or stormbound go
    // first, then the last still due; the ship in stays.
    {
        market::Board b;
        const auto d = t + 20;                      // (A day with days before it: a save keeps none before the first.)
        b.ships.push_back(aShip("s1", "the Gull", d, "in"));
        for (int n = 0; n < 6; ++n)
            b.ships.push_back(aShip("s" + std::to_string(10 + n), "the Old " + std::string(1, char('A' + n)), d - 1 - n, n % 2 ? "storm" : "sailed"));
        for (int n = 0; n < 13; ++n)
            b.ships.push_back(aShip("s" + std::to_string(30 + n), "the New " + std::string(1, char('A' + n)), d + 1 + n, "due"));
        auto other = aShip("s77", "the Gull", d, "due");
        other.port = "west";
        b.ships.push_back(other);
        b.nextShip = 5;
        const auto back = market::decode(market::encode(b));
        const auto count = [&](const std::string& status) {
            return std::count_if(back.ships.begin(), back.ships.end(), [&](const market::Ship& s) { return s.port == "east" && s.status == status; });
        };
        expect(back.ships.size() == market::ShipsAPort + 1 && count("in") == 1 && count("due") == 13 && count("sailed") + count("storm") == 2,
               "sixteen at east: " + std::to_string(back.ships.size()) + ", " + std::to_string(count("due")) + " due");
        for (const auto& s : back.ships)
            expect(s.status == "in" || s.status == "due" || s.day >= d - 2, "the newest of those gone kept: " + s.name);
        expect(back.nextShip == 78, "the next id above them all: " + std::to_string(back.nextShip));
        for (int n = 0; n < 6; ++n)
            b.ships.push_back(aShip("s" + std::to_string(50 + n), "the Late " + std::string(1, char('A' + n)), d + 20 + n, "due"));
        const auto fewer = market::decode(market::encode(b));
        long due = 0, far = 0;
        for (const auto& s : fewer.ships)
            due += s.port == "east" && s.status == "due", far += s.port == "east" && s.day >= d + 22;
        expect(fewer.ships.size() == market::ShipsAPort + 1 && due == 15 && far == 0, "with only ships to come over the bound, the last due go: " +
                                                                                         std::to_string(due) + " due, " + std::to_string(far) + " far");
        // (And through a state restored, not decoded.)
        auto saved = w.save();
        saved.society.board.ships.clear();
        for (int n = 0; n < 30; ++n)
            saved.society.board.ships.push_back(aShip("s" + std::to_string(100 + n), "the Pad " + std::string(1, char('A' + n % 26)) + (n >= 26 ? "a" : ""),
                                                      t - 2 + n % 3, n % 3 == 2 ? "due" : "storm"));
        auto v = load(f);
        expect(v.restore(saved).ok && v.society().board().ships.size() <= market::ShipsAPort, "a restored state's timetable bounded: " +
                                                                                                std::to_string(v.society().board().ships.size()));
    }
}

// The rule off and on the same morning (leftover D): the DM stops the ships and sets them going again before the day's
// ship is in. It comes as it was posted, the same ship under the same id, with the rest of the timetable; one whose hour
// passed while the rule was off did not come.
void theRuleOffAndOnTheSameMorning()
{
    auto f = harbour();
    auto w = load(f);
    w.tick(.6);
    const int rule = int(w.society().rule("shipsPerWeek"));
    const auto s = toShipDay(w);
    expect(w.setTimeOfDay(s.hour - 1).ok, "an hour before it is due");
    w.tick(.6);
    const auto before = shipsOf(w);
    std::vector<std::string> due;
    for (const auto& x : w.society().board().ships)
        if (x.status == "due")
            due.push_back(x.id);
    expect(shipById(w, s.id) && shipById(w, s.id)->status == "due" && due.size() >= 2, "today's ship still due, and others");
    w.takeEvents();
    shipsAWeek(w, 0);
    w.tick(.6);
    w.tick(.6);
    expect(!shipById(w, s.id) && w.shipsNotice("east").empty(), "the rule at 0: no ship due, the notice down");
    shipsAWeek(w, rule);
    w.tick(.6);
    w.tick(.6);
    auto events = w.takeEvents();
    const auto* back = shipById(w, s.id);
    expect(back && back->status == "due" && back->day == s.day && back->name == s.name && back->brings == s.brings && back->takes == s.takes,
           "the rule back the same morning: today's ship is due again, as it was posted");
    for (const auto& shipId : due)
        expect(shipById(w, shipId) && shipById(w, shipId)->status == "due", "and the others, under their ids: " + shipId);
    expect(shipsOf(w) == before, "the timetable as it was");
    expect(count(events, "ship posted") == 0 && count(events, "ships cleared", "east") == 1 && count(events, "ships posted", "east") >= 1,
           "no ship posted anew; the notice down and up again: " + std::to_string(count(events, "ship posted")));
    expect(hasWord(w.shipsNotice("east"), s.name), "its name on the notice again: " + w.shipsNotice("east"));
    expect(w.setTimeOfDay(s.hour + .1).ok, "its hour");
    run(w, 5);
    events = w.takeEvents();
    expect(count(events, "ship docks", s.id) == 1 && w.shipIn("east"), "and it docks at its hour, once");
    // Past its hour while the rule was off: it did not come; the ships to come are back.
    auto v = load(f);
    v.tick(.6);
    const auto s2 = toShipDay(v);
    expect(v.setTimeOfDay(s2.hour - 1).ok, "an hour before");
    v.tick(.6);
    std::vector<std::string> later;
    for (const auto& x : v.society().board().ships)
        if (x.status == "due" && x.day > today(v))
            later.push_back(x.id);
    shipsAWeek(v, 0);
    v.tick(.6);
    expect(v.setTimeOfDay(s2.hour + .5).ok, "its hour passes with the rule off");
    v.tick(.6);
    v.takeEvents();
    shipsAWeek(v, rule);
    v.tick(.6);
    v.tick(.6);
    events = v.takeEvents();
    expect(!shipById(v, s2.id) && !v.shipIn("east") && count(events, "ship docks") == 0, "a ship whose hour passed with the rule off did not come");
    for (const auto& shipId : later)
        expect(shipById(v, shipId) && shipById(v, shipId)->status == "due", "those to come are back: " + shipId);
    // Off through a night: the day's ships are forgotten, and the timetable is rolled anew from the day the rule is back.
    auto u = load(f);
    u.tick(.6);
    const auto s3 = toShipDay(u);
    shipsAWeek(u, 0);
    u.tick(.6);
    nextMorning(u);
    shipsAWeek(u, rule);
    u.tick(.6);
    u.tick(.6);
    expect(!shipById(u, s3.id), "yesterday's ship is not back a day late");
    for (const auto& x : u.society().board().ships)
        expect(x.status == "due" && x.day > today(u), "a new timetable, from tomorrow: " + x.name);
    expect(!u.society().board().ships.empty(), "and there is one");
}

// What players sold to a shop that only buys the good goes with the ship too (leftover G): goods move on (7.5) from every
// till of the port that holds more than its shelf of a good the ship takes, set out or not.
void aShipTakesWhatAShopOnlyBuys()
{
    auto f = harbour();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    w.roads().camps.clear();
    auto& soc = w.society();
    market::Ship s;
    std::string keeper, good;
    for (int tries = 0; tries < 20 && good.empty(); ++tries)
    {
        if (tries)
            nextMorning(w);
        s = toShipDay(w);
        for (const auto& [g, n] : s.takes)
            for (const auto* k : {"et", "eg", "em", "ek", "es"})
            {
                const auto set = soc.stagedShelfGoods(k);
                if (!good.empty() || std::any_of(set.begin(), set.end(), [&](const std::string& ware) { return items::baseOf(ware) == g; }))
                    continue;
                soc.create("player-ada", g, 1, "test: gathered");
                if (soc.quote("player-ada", k, g, 1, false).ok)
                    keeper = k, good = g;
                soc.consume("player-ada", g, 1, "test: put back");
            }
    }
    expect(!good.empty(), "a ship takes a good an east shop buys and never sets out");
    const auto till = soc.tillOf(keeper);
    const int target = soc.shelfTarget(till, good);
    soc.create("player-ada", good, 60, "test: gathered");
    int sold = 0;
    for (int i = 0; i < 60; ++i)
        sold += soc.trade("player-ada", keeper, good, 1, false).ok;
    const int held = stock(w, keeper, good);
    expect(sold > 0 && held > target, "the shop holds more than its shelf of " + good + ": " + std::to_string(held) + " over " + std::to_string(target));
    dock(w, s);
    expect(stock(w, keeper, good) == held, "the ship in: it is there still");
    w.takeEvents();
    expect(w.setTimeOfDay(std::min(23.5, s.sails + .1)).ok, "its evening");
    run(w, 5);
    const auto events = w.takeEvents();
    int pieces = 0;
    for (const auto& e : events)
        if (e.kind == "ship sails" && e.actor == s.id)
            pieces = e.quantity;
    expect(count(events, "ship sails", s.id) == 1 && stock(w, keeper, good) == std::min(held, soc.shelfTarget(till, good)),
           "it sails with what the shop held over its shelf: " + std::to_string(stock(w, keeper, good)) + " left of " + std::to_string(held));
    expect(pieces >= held - stock(w, keeper, good) && soc.realBalanced(), "counted in its load: " + std::to_string(pieces) + "; real money balanced");
}

// ---- Ship day through a crash (Phases 4 and 5 merged). The ships live on the Market Board, which the journal writes
// whole with any record of the economy and as each checkpoint is taken (journalCheckpoint: Phase 4's third repair); the
// tills a ship filled or took from go with it (Society::takeTillsMoved). Each case plays to its crash on a file save: a
// checkpoint, then a ship's step on a world tick (it docks, it sails, a storm keeps it out, a morning rolls a new week),
// then a player's trade journalled, then the game gone without saving; and is checked after the restart: the step has
// happened once (not again, not lost), with its prices, its shelves and its back rooms. Each case twice: plain, and
// ("<case>_window") with a snapshot between the step and the trade that is taken and does not land, so the store still
// holds the checkpoint before the step when the game goes. The same cases go through Postgres (Tests/port_pg.py:
// --write-crashes, --check-crashes).
struct ShipCrash
{
    const char* name;
    std::function<json::Value(game::Game&)> before;                 // The checkpoint, then the step: what was so then.
    std::function<void(game::Game&, const json::Value&)> after;     // After the restart (and what was so at the crash).
};

const char* const EastKeepers[] = {"em", "es", "ek", "eg", "et"};

// The port as it stands, in words a file keeps: the timetable, each ship good's price, east's shelves and back rooms of
// what ships bring and take, its troubles, its notice.
json::Value portAsItStands(const World& w)
{
    const auto& soc = w.society();
    const auto& b = soc.board();
    std::set<std::string> goods;
    if (const auto* p = w.port("east"))
        goods.insert(p->imports.begin(), p->imports.end());
    for (const auto& s : b.ships)
    {
        for (const auto& [g, n] : s.brings)
            goods.insert(g);
        for (const auto& [g, n] : s.takes)
            goods.insert(g);
    }
    auto out = json::Value::object();
    out.add("ships", shipsOf(w));
    std::ostringstream prices, shelves, rooms;
    prices.precision(6);
    prices << std::fixed;
    for (const auto& g : goods)
        prices << g << ' ' << soc.pressure("east", g) << ' ';
    for (const auto* k : EastKeepers)
    {
        const auto till = soc.tillOf(k);
        for (const auto& ware : soc.wares(k))
            if (goods.count(items::baseOf(ware)))
                shelves << k << ' ' << ware << ' ' << stock(w, k, ware) << ' ';
        if (const auto r = b.backRoom.find(till); r != b.backRoom.end())
            for (const auto& [g, n] : r->second)
                if (goods.count(g))
                    rooms << k << ' ' << g << ' ' << n << ' ';
    }
    out.add("prices", prices.str());
    out.add("shelves", shelves.str());
    out.add("rooms", rooms.str());
    out.add("troubles", int(b.troubles.size()));
    out.add("notice", w.shipsNotice("east"));
    return out;
}

// Two lists of prices ("good pressure good pressure ..."), the same to the fourth place (a save keeps a pressure to six).
bool samePrices(const std::string& one, const std::string& other)
{
    std::istringstream a(one), b(other);
    std::string ga, gb;
    double pa = 0, pb = 0;
    while (a >> ga >> pa)
        if (!(b >> gb >> pb) || ga != gb || std::abs(pa - pb) > 1e-4)
            return false;
    return !(b >> gb);
}

// The step happened once: the port after the restart is the port of the crash, and the roads' next passes do none of
// the ship's steps again.
void thePortOfTheCrash(game::Game& g, const json::Value& was, const std::string& name)
{
    auto& w = g.world();
    const auto& stood = was["atCrash"];
    const auto check = [&](const std::string& when) {
        const auto now = portAsItStands(w);
        expect(samePrices(now.string("prices"), stood.string("prices")), name + ": " + when + ", the prices of the crash:\n  " + now.string("prices") + "\n  for\n  " + stood.string("prices"));
        for (const char* key : {"ships", "shelves", "rooms", "notice"})
            expect(now.string(key) == stood.string(key), name + ": " + when + ", the " + key + " of the crash:\n  " + now.string(key) + "\n  for\n  " + stood.string(key));
        expect(now.number("troubles") == stood.number("troubles"), name + ": " + when + ", the troubles of the crash: " +
                                                                    std::to_string(now.number("troubles")) + " for " + std::to_string(stood.number("troubles")));
    };
    check("after the restart");
    w.takeEvents();
    w.tick(.6);
    w.tick(.6);
    const auto events = w.takeEvents();
    for (const char* kind : {"ship docks", "ship sails", "ship stormbound", "ship posted"})
        expect(count(events, kind) == 0, name + ": no \"" + kind + "\" a second time: " + std::to_string(count(events, kind)));
    check("two passes of the roads on");
    const auto* n = g.documents().find("ships-east");
    expect(n && n->text == stood.string("notice"), name + ": the Wharf office's notice up, with the words of the crash: " + (n ? n->text : std::string("none")));
    std::set<std::string> ids;
    std::set<std::int64_t> days;
    for (const auto& s : w.society().board().ships)
        expect(ids.insert(s.id).second && days.insert(s.day).second, name + ": one ship of an id, one a day: " + s.id);
    expect(w.society().realBalanced(), name + ": real money is balanced");
}

// The market's own morning done (it comes with the society's next pass after dawn): a case's crash is not left between
// a day's turn on the roads and the Board's, for the restart to finish and be told from a step done twice.
void theMarketsMorningDone(World& w, const std::string& name)
{
    for (int t = 0; t < 400 && w.society().board().day < w.society().marketDay(); ++t)
        w.tick(.6);
    expect(w.society().board().day == w.society().marketDay(), name + ": the Board on the market's day");
}

const std::vector<ShipCrash>& shipCrashes()
{
    static const std::vector<ShipCrash> cases{
        // A ship docked after the checkpoint (its cargo cheaper, the shelves it fills and two more behind): in, once.
        {"ship_docks",
         [](game::Game& g) {
             auto& w = g.world();
             auto& soc = w.society();
             w.tick(.6);
             w.tick(.6);
             w.roads().camps.clear();
             const auto s = toShipDay(w);
             expect(w.setTimeOfDay(s.hour - .3).ok, "ship_docks: a little before its hour");
             // (The shelves of what ships bring sold out, its cargo dear: what its docking moves is there to see.)
             const auto imports = w.port("east")->imports;
             int shelves = 0;
             for (const auto* k : EastKeepers)
                 for (const auto& ware : soc.wares(k))
                     if (std::find(imports.begin(), imports.end(), items::baseOf(ware)) != imports.end() && stock(w, k, ware) > 0)
                     {
                         soc.consume(soc.tillOf(k), ware, stock(w, k, ware), "test: sold out");
                         ++shelves;
                     }
             expect(shelves > 0, "ship_docks: east's shops set out what ships bring");
             soc.setPressure("east", s.brings.front().first, .3, "players");
             g.save();
             g.settle();                            // (The checkpoint, on its day: the ship due, the shelves bare.)
             const auto bare = portAsItStands(w);
             w.takeEvents();
             dock(w, s);
             expect(count(w.takeEvents(), "ship docks", s.id) == 1, "ship_docks: it docks after the checkpoint");
             const auto full = portAsItStands(w);
             expect(full.string("shelves") != bare.string("shelves") && full.string("rooms") != bare.string("rooms") &&
                        full.string("prices") != bare.string("prices"),
                    "ship_docks: its docking filled shelves and back rooms and moved a price");
             auto was = json::Value::object();
             was.add("ship", s.id);
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             const auto* s = shipById(g.world(), was.string("ship"));
             expect(s && s->status == "in" && g.world().shipIn("east") && g.world().shipIn("east")->id == s->id, "ship_docks: in after the crash");
         }},
        // A ship sailed after the checkpoint with a glut of what it takes: sailed, once, and the glut not back on the shelf.
        {"ship_sails",
         [](game::Game& g) {
             auto& w = g.world();
             auto& soc = w.society();
             w.tick(.6);
             w.tick(.6);
             w.roads().camps.clear();
             const auto s = toShipDay(w);
             dock(w, s);
             std::string keeper, ware;
             for (const auto* k : EastKeepers)
                 for (const auto& x : soc.stagedShelfGoods(k))
                     for (const auto& [good, n] : s.takes)
                         if (ware.empty() && x == good)
                             keeper = k, ware = x;
             expect(!ware.empty(), "ship_sails: a shop sets out something it takes");
             const auto till = soc.tillOf(keeper);
             const int target = soc.shelfTarget(till, ware);
             soc.create(till, ware, target + 25 - stock(w, keeper, ware), "test: a glut sold in");
             expect(w.setTimeOfDay(std::min(23.4, s.sails - .1)).ok, "ship_sails: a little before it sails");
             g.save();
             g.settle();                            // (The checkpoint: the ship in, the glut on the shelf.)
             expect(stock(w, keeper, ware) == target + 25, "ship_sails: the glut on the shelf at the checkpoint");
             w.takeEvents();
             expect(w.setTimeOfDay(std::min(23.5, s.sails + .1)).ok, "ship_sails: its evening hour");
             run(w, 5);
             expect(count(w.takeEvents(), "ship sails", s.id) == 1, "ship_sails: it sails after the checkpoint");
             expect(stock(w, keeper, ware) <= target, "ship_sails: with the glut");
             auto was = json::Value::object();
             was.add("ship", s.id);
             was.add("keeper", keeper);
             was.add("ware", ware);
             was.add("target", target);
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             auto& w = g.world();
             const auto* s = shipById(w, was.string("ship"));
             expect(s && s->status == "sailed" && !w.shipIn("east"), "ship_sails: sailed after the crash");
             const int have = stock(w, was.string("keeper"), was.string("ware"));
             expect(have <= int(was.number("target")), "ship_sails: the load that sailed is not back on the shelf: " + std::to_string(have) + " of a shelf of " +
                                                           std::to_string(int(was.number("target"))));
         }},
        // A storm kept a ship out at its day's turn after the checkpoint (+0.1 on its cargo, a trouble): once.
        {"ship_storm",
         [](game::Game& g) {
             auto& w = g.world();
             w.tick(.6);
             w.tick(.6);
             w.roads().camps.clear();
             std::string ship;
             for (int day = 0; day < 500 && ship.empty(); ++day)
             {
                 g.save();
                 g.settle();                        // (The checkpoint, the day before: the ship still due.)
                 w.takeEvents();
                 nextMorning(w);
                 for (const auto& e : w.takeEvents())
                     if (e.kind == "ship stormbound")
                         ship = e.actor;
             }
             theMarketsMorningDone(w, "ship_storm");
             expect(!ship.empty() && shipById(w, ship) && shipById(w, ship)->status == "storm", "ship_storm: a storm keeps a ship out at its day's turn");
             auto was = json::Value::object();
             was.add("ship", ship);
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             const auto* s = shipById(g.world(), was.string("ship"));
             expect(s && s->status == "storm" && !g.world().shipIn("east"), "ship_storm: stormbound after the crash");
         }},
        // A morning rolled a new week of ships after the checkpoint: the same timetable, the same ids, none twice.
        {"ship_morning",
         [](game::Game& g) {
             auto& w = g.world();
             w.tick(.6);
             w.tick(.6);
             w.roads().camps.clear();
             std::string before, after;
             for (int day = 0; day < 30 && before == after; ++day)
             {
                 g.save();
                 g.settle();                        // (The checkpoint, the day before.)
                 before = shipsOf(w);
                 w.takeEvents();
                 nextMorning(w);
                 const auto events = w.takeEvents();
                 after = count(events, "ship posted") > 0 && count(events, "ship stormbound") == 0 ? shipsOf(w) : before;
             }
             expect(before != after, "ship_morning: a morning posts new ships");
             theMarketsMorningDone(w, "ship_morning");
             auto was = json::Value::object();
             was.add("next", double(w.society().board().nextShip));
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             expect(double(g.world().society().board().nextShip) == was.number("next"), "ship_morning: the next ship's number as it was");
         }},
    };
    return cases;
}

struct ShipRun
{
    const ShipCrash* c;
    bool window;
    std::string name;
};
const std::vector<ShipRun>& shipRuns()
{
    static const std::vector<ShipRun> runs = [] {
        std::vector<ShipRun> all;
        for (const bool window : {false, true})
            for (const auto& c : shipCrashes())
                all.push_back({&c, window, std::string(c.name) + (window ? "_window" : "")});
        return all;
    }();
    return runs;
}

std::string slurp(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    expect(bool(in), "reads " + file.string());
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}
std::string crashSave(const std::string& name)
{
    return (std::filesystem::temp_directory_path() / ("ratw-s2stage-shipcrash-" + name + "-" + std::to_string(::getpid()) + ".json")).string();
}
void cleanSave(const std::string& path)
{
    for (const auto* end : {"", ".journal", ".art", ".lock", ".reports", ".snapshot"})
        std::filesystem::remove_all(path + end);
}

// The window between a snapshot's capture and its landing, held open (trade_show_tests' own): the tick's snapshot is
// really forked, and its child cannot write its file, so the save stays the checkpoint before it.
void aSnapshotDoesNotLand(game::Game& g, const std::string& path, const std::string& log, const std::string& name)
{
    g.settle();
    const auto held = slurp(path);
    std::filesystem::create_directory(path + ".snapshot");   // (Where the child writes: it cannot now.)
    const auto failed = [&] { return log.find("the snapshot could not be written") != std::string::npos; };
    for (int t = 0; t < 45 && !failed(); ++t)
        g.tick(1);                                  // (The snapshot is forked in here, after its thirty seconds.)
    for (int t = 0; t < 500 && !failed(); ++t)
    {
        ::usleep(10000);
        g.tick(.01);
    }
    std::filesystem::remove_all(path + ".snapshot");
    expect(failed(), name + ": a snapshot is taken and cannot be written");
    expect(slurp(path) == held, name + ": the save is still the checkpoint before it");
}

// A purchase at one of west's shops (an economy record that moves the Board; never east's shelves, which the case
// compares). Before the shops open the Board is moved as a purchase moves it, for the next trade's record to carry.
void buysInWest(game::Game& g, Client& c, const std::string& name)
{
    auto& w = g.world();
    const auto cash = [&] {
        const auto* a = w.society().account(c.entityId);
        return a ? a->cash : 0;
    };
    const auto before = cash();
    std::string said;
    for (const char* keeper : {"wg", "wm"})
        for (const char* item : {"meal", "nails", "flour", "herbs", "oats"})
        {
            const auto* k = w.entity(keeper);
            auto* me = w.entity(c.entityId);
            if (!k || !me || cash() < before)
                continue;
            me->cellId = k->cellId;
            me->position = {k->position.x + .5, k->position.y};
            c.events.clear();
            g.command(&c, cmd({{"type", "trade"}, {"target", keeper}, {"item", item}, {"quantity", 1}, {"buy", true}}));
            g.settle();
            said += c.said();
        }
    if (cash() == before)
    {
        const double p = w.society().pressure("west", "meal");
        w.society().setPressure("west", "meal", p + .02, "players");
        expect(std::abs(w.society().pressure("west", "meal") - p - .02) < 1e-6, name + ": no shop open (" + said + "), the Board moved as a purchase moves it");
        return;
    }
    expect(cash() < before, name + ": a purchase at a shop in west: " + said);
}

// To the crash: the case's own play, then (a window run) a snapshot that doesn't land and a purchase, then a player's
// trade (a penny for a notice: an economy record), then the game goes without saving.
json::Value toTheShipCrash(const ShipRun& r, const std::filesystem::path& root, const std::string& path)
{
    std::string log;
    auto o = optionsOn(root, path);
    o.forkSnapshots = r.window;
    game::Game g(o);
    g.log = [&log](const char*, const std::string& text) { log += text + "\n"; };
    std::string problem;
    expect(g.start(problem), r.name + ": a game starts on the harbour with a file save: " + problem);
    // (A window run's player is in before the case's checkpoint: nothing is journalled between the ship's step and the
    // snapshot that doesn't land, so that snapshot's own record is the one that must carry it.)
    Client ash;
    if (r.window)
        enterAt(g, ash, 84, "ash", "east");
    auto was = r.c->before(g);
    const auto saved = slurp(path);
    if (!r.window)
        enterAt(g, ash, 84, "ash", "east");
    if (r.window)
    {
        aSnapshotDoesNotLand(g, path, log, r.name);
        buysInWest(g, ash, r.name);
        const auto* spot = g.world().boardSpot("east");
        auto* me = g.world().entity(ash.entityId);
        expect(spot && me, r.name + ": back to east's board");
        me->cellId = spot->cell;
        me->position = {spot->x + .5, spot->y};
    }
    board(g, ash, {{"verb", "post"}, {"kind", "other"}, {"text", "A trade, journalled."}});
    g.settle();                                     // (Its reply waits for its journal record.)
    const auto said = ash.said();
    expect(said.find("You pin your notice") != std::string::npos, r.name + ": a penny paid: an economy record: " + said);
    g.disconnect(&ash);
    expect(slurp(path) == saved, r.name + ": the save is the checkpoint from before the ship's step (the journal is the tail)");
    was.add("atCrash", portAsItStands(g.world()));
    return was;
}

void afterTheShipCrash(const ShipRun& r, const std::filesystem::path& root, const std::string& path, const json::Value& was)
{
    game::Game g(optionsOn(root, path));
    std::string problem;
    expect(g.start(problem), r.name + ": starts again after the crash: " + problem);
    r.c->after(g, was);
    thePortOfTheCrash(g, was, r.name);
    r.c->after(g, was);
}

void shipDayThroughACrash()
{
    for (const auto& r : shipRuns())
    {
        auto f = harbour();
        const auto root = onDisk(f, "shipcrash-" + r.name);
        const auto path = crashSave(r.name);
        cleanSave(path);
        const auto was = toTheShipCrash(r, root, path);
        afterTheShipCrash(r, root, path, was);
        cleanSave(path);
        std::filesystem::remove_all(root);
    }
}

// A port deleted, through a crash. Its ships are dropped when the save loads into the world without its Wharf, and the
// Board without them goes whole into the journal with the next record of the economy: a crash after it never brings the
// old timetable back, not even into a world whose Wharf is built again (new ships are posted there, none found in).
void aPortDeletedThroughACrash()
{
    auto f = harbour();
    auto root = onDisk(f, "port-gone-crash");
    const auto path = crashSave("port_gone");
    cleanSave(path);
    const auto trades = [&](game::Game& g, Client& c, int n, const char* who, const std::string& name) {
        enterAt(g, c, n, who, "east");
        buysInWest(g, c, name);
        const auto* spot = g.world().boardSpot("east");
        auto* me = g.world().entity(c.entityId);
        expect(spot && me, name + ": at east's board");
        me->cellId = spot->cell;
        me->position = {spot->x + .5, spot->y};
        board(g, c, {{"verb", "post"}, {"kind", "other"}, {"text", "A trade, journalled."}});
        g.settle();
        expect(c.said().find("You pin your notice") != std::string::npos, name + ": a penny paid: an economy record: " + c.said());
        g.disconnect(&c);
    };
    std::set<std::string> old;
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "a game starts on the harbour: " + problem);
        auto& w = g.world();
        w.tick(.6);
        w.tick(.6);
        const auto s = toShipDay(w);
        g.save();
        g.settle();                                 // (The checkpoint: its ships due.)
        dock(w, s);
        Client ash;
        trades(g, ash, 84, "ash", "the port's last day");
        for (const auto& x : w.society().board().ships)
            old.insert(x.id);
        expect(w.shipIn("east") && old.size() >= 2, "the journal holds a timetable, a ship in");
    }                                               // (The game gone without saving.)
    std::filesystem::remove_all(root);
    auto edited = harbour(true, false);
    root = onDisk(edited, "port-gone-crash");
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "the crash's save starts on the world with no Wharf: " + problem);
        auto& w = g.world();
        expect(w.ports().empty() && w.society().board().ships.empty() && w.society().board().shipsThrough.empty(), "its ships dropped at the load");
        Client bryn;
        trades(g, bryn, 85, "bryn", "the day after the Wharf went");
        expect(w.society().board().ships.empty(), "none since");
    }                                               // (Gone again without saving: the drop is the journal's to keep.)
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "starts again with no Wharf: " + problem);
        auto& w = g.world();
        expect(w.society().board().ships.empty() && w.society().board().shipsThrough.empty(), "no ships after the second crash");
        w.tick(.6);
        for (int t = 0; t < 30; ++t)
            g.tick(.05);
        expect(!g.documents().find("ships-east"), "the Wharf office's notice down");
        expect(w.society().realBalanced(), "real money is balanced");
    }
    std::filesystem::remove_all(root);
    root = onDisk(f, "port-gone-crash");
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "and on the world with its Wharf built again: " + problem);
        auto& w = g.world();
        expect(!w.ports().empty() && !w.shipIn("east"), "the old timetable is not back: no ship found in");
        for (const auto& x : w.society().board().ships)
            expect(x.status == "due", "nor one of it docked or sailed: " + x.id + " " + x.status);
        w.takeEvents();
        w.tick(.6);
        w.tick(.6);
        const auto events = w.takeEvents();
        expect(count(events, "ship docks") == 0 && count(events, "ship sails") == 0, "nothing docks or sails a second time");
        std::set<std::string> ids;
        std::set<std::int64_t> days;
        for (const auto& x : w.society().board().ships)
            expect(x.status == "due" && x.day > today(w) && ids.insert(x.id).second && days.insert(x.day).second,
                   "new ships posted, each still to come, one of an id, one a day: " + x.id);
        expect(!ids.empty(), "the port has ships again");
        expect(w.society().realBalanced(), "real money is balanced");
    }
    cleanSave(path);
    std::filesystem::remove_all(root);
}

// --write-crashes DIR: each case played to its crash; its checkpoint (DIR/<case>.json), its journal (DIR/<case>.journal,
// a record a line: its number, a tab, its text) and what was so at the crash (DIR/<case>.was.json), for Tests/port_pg.py
// to put through game.save_checkpoint and game.journal in a throwaway database and read back.
int writeCrashes(const std::filesystem::path& dir)
{
    namespace fs = std::filesystem;
    fs::create_directories(dir);
    for (const auto& r : shipRuns())
    {
        auto f = harbour();
        const auto root = onDisk(f, "shipcrash-" + r.name);
        const auto path = crashSave(r.name);
        cleanSave(path);
        const auto was = toTheShipCrash(r, root, path);
        expect(fs::exists(path) && fs::exists(path + ".journal"), r.name + ": a checkpoint and a journal written");
        fs::copy_file(path, dir / (r.name + ".json"), fs::copy_options::overwrite_existing);
        fs::copy_file(path + ".journal", dir / (r.name + ".journal"), fs::copy_options::overwrite_existing);
        std::ofstream(dir / (r.name + ".was.json"), std::ios::binary) << json::dump(was);
        cleanSave(path);
        fs::remove_all(root);
    }
    return 0;
}

// --check-crashes BACK DIR: each case's checkpoint and journal as Postgres gave them back (BACK/<case>.json and
// .journal), restarted on and checked as the file save's are.
int checkCrashes(const std::filesystem::path& back, const std::filesystem::path& dir)
{
    namespace fs = std::filesystem;
    for (const auto& r : shipRuns())
    {
        auto f = harbour();
        const auto root = onDisk(f, "shipcrash-" + r.name);
        const auto path = crashSave(r.name);
        cleanSave(path);
        fs::copy_file(back / (r.name + ".json"), path);
        fs::copy_file(back / (r.name + ".journal"), path + ".journal");
        json::Value was;
        std::string error;
        expect(json::parse(slurp(dir / (r.name + ".was.json")), was, error), r.name + ": what was so reads: " + error);
        afterTheShipCrash(r, root, path, was);
        cleanSave(path);
        fs::remove_all(root);
    }
    std::cout << "port_tests --check-crashes: " << shipRuns().size() << " ship crashes replayed the same through Postgres (" << checks << " checks)\n";
    return 0;
}

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
        if (argc == 3 && std::string(argv[1]) == "--write-crashes")
            return writeCrashes(argv[2]);
        if (argc == 4 && std::string(argv[1]) == "--check-crashes")
            return checkCrashes(argv[2], argv[3]);
        const std::vector<std::pair<std::string, void (*)()>> tests{
            {"aPortAndItsTimetable", aPortAndItsTimetable},
            {"theTimetableSurvivesARestart", theTimetableSurvivesARestart},
            {"shipDay", shipDay},
            {"importsWaitForShips", importsWaitForShips},
            {"aStormboundShip", aStormboundShip},
            {"theRuleAtItsMost", theRuleAtItsMost},
            {"aWorldWithNoPort", aWorldWithNoPort},
            {"aPortDeleted", aPortDeleted},
            {"shipsPayingPlayers", shipsPayingPlayers},
            {"theWharfOfficesNotice", theWharfOfficesNotice},
            {"theTimetableSurvivesACrash", theTimetableSurvivesACrash},
            {"aPortDeletedUnderTheGame", aPortDeletedUnderTheGame},
            // The repair's.
            {"noShipThisWeek", noShipThisWeek},
            {"aShipAtAQuietPier", aShipAtAQuietPier},
            {"theRuleStopsTheShips", theRuleStopsTheShips},
            {"whatMakesAPort", whatMakesAPort},
            {"whichShipTalkNames", whichShipTalkNames},
            {"theNoticeAtEveryRule", theNoticeAtEveryRule},
            {"cargoInWords", cargoInWords},
            {"aFullTimetable", aFullTimetable},
            {"aRestartOnShipDay", aRestartOnShipDay},
            {"aCalendarJump", aCalendarJump},
            {"aDayTurnedByTicks", aDayTurnedByTicks},
            {"cargoByTheDay", cargoByTheDay},
            {"aSaveThatLies", aSaveThatLies},
            // Phases 4 and 5 merged.
            {"shipDayThroughACrash", shipDayThroughACrash},
            {"aPortDeletedThroughACrash", aPortDeletedThroughACrash},
            {"aTimetableThatLies", aTimetableThatLies},
            {"theRuleOffAndOnTheSameMorning", theRuleOffAndOnTheSameMorning},
            {"aShipTakesWhatAShopOnlyBuys", aShipTakesWhatAShopOnlyBuys}};
        // `--only NAME`: one test (to see it fail on the code before a repair); `--each`: every test, each to its end.
        if (argc == 3 && std::string(argv[1]) == "--only")
        {
            for (const auto& [name, test] : tests)
                if (name == argv[2])
                {
                    test();
                    std::cout << name << " passed: " << checks << " checks.\n";
                    return 0;
                }
            std::cerr << "No such test: " << argv[2] << '\n';
            return 2;
        }
        if (argc == 2 && std::string(argv[1]) == "--each")
        {
            int failed = 0;
            for (const auto& [name, test] : tests)
            {
                try
                {
                    test();
                    std::cout << "PASS " << name << '\n';
                }
                catch (const std::exception& error)
                {
                    ++failed;
                    std::cout << "FAIL " << name << ": " << error.what() << '\n';
                }
            }
            return failed ? 1 : 0;
        }
        for (const auto& [name, test] : tests)
            test();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Port tests passed: " << checks << " checks.\n";
    return 0;
}
