// Staged events and causes (Docs/Design/79-a-staged-economy.md, 5.3, 8.4-8.6, T13, T16, D25; Phase 7). Each market sees
// 1-3 events a week of market.json's list it can show (seeded, committed on the Market Board a week ahead, so saved,
// journalled and restored with it: file and Postgres, Tests/events_pg.py), each with its notice, crier's line, crowd and
// talk facts, its goods pressed toward their targets inside the clamp, its Times points in its market's score; events
// never touch money, bands or posts. A move of the slider comes with its cause (the DM's words, else the seeded list's),
// told once in every market and said as world.times_cause and in the Mind's Means; the DM's Times nudges and harvest
// overrides post their notices. A world with no port, no wagon and no farm still has events. The catalogue is the real
// Data (RATW_SOURCE_DIR); the harness is port_tests'.
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
std::string rules;                  // Rule records the next strip's manifest carries ("rule \"eventRate\" 300\n").
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
    if (!rules.empty())
        m << rules;
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

// ---------------------------------------------------------------------------------------------------------- Phase 7

const char* const EastKeepers[] = {"em", "es", "ek", "eg", "et"};

std::vector<const market::StagedEvent*> eventsAt(const World& w, const std::string& market)
{
    std::vector<const market::StagedEvent*> out;
    for (const auto& e : w.society().board().events)
        if (e.market == market)
            out.push_back(&e);
    return out;
}

std::string eventsOf(const World& w)
{
    const auto j = market::encode(w.society().board());
    return json::dump(j["events"]) + json::dump(j["eventsThrough"]) + json::dump(j["nextEvent"]);
}

void setRule(World& w, const std::string& name, int value)
{
    auto spec = w.society().authored().economy;
    spec.rules[name] = value;
    w.society().adoptEconomy(spec);
}

double factorOf(const World& w, const std::string& keeper, const std::string& ware, const std::string& market)
{
    const auto& soc = w.society();
    return market::factor(soc.baseFactor(soc.tillOf(keeper), ware), soc.pressure(market, items::baseOf(ware)));
}

// The list in market.json (8.4) and the slider's causes (8.5) as the loader takes them; a host's entries held to the same
// rules: a bad one left out, the rest kept; never the forbidden word (an id by the name rule, its letters anywhere; words
// by whole words), and the Dungeon Master's cause by the same.
void theListAndItsChecks()
{
    const auto& d = market::data();
    expect(d.loaded && d.events.size() >= 12, "market.json's events: " + std::to_string(d.events.size()));
    std::set<std::string> ids;
    for (const auto& k : d.events)
    {
        ids.insert(k.id);
        expect(!market::forbiddenInName(k.id) && !market::forbiddenWord(k.words) && !market::forbiddenWord(k.notice) &&
                   !market::forbiddenWord(k.cry) && !market::forbiddenWord(k.sign),
               "no forbidden word in " + k.id);
        expect(market::plainWords(k.words, 80) && market::plainWords(k.notice, 280) && market::plainWords(k.cry, 200), "plain words: " + k.id);
        for (const auto& [g, P] : k.goods)
            expect(items::good(g) && std::abs(P) <= market::EventTargetMost + 1e-9, k.id + "'s goods known, inside the clamp: " + g);
    }
    for (const char* plan : {"house_wedding", "works_fire", "good_catch", "orchard_blight", "festival_buying", "lame_ox", "rich_cargo", "lost_ship"})
        expect(ids.count(plan), std::string("8.4's list has ") + plan);
    expect(d.eventKind("rich_cargo")->ship == "docks" && d.eventKind("lost_ship")->ship == "lost" && d.eventKind("lame_ox")->holdsCart &&
               d.eventKind("house_wedding")->needs == "city" && d.eventKind("festival_buying")->needs == "festival",
           "the kinds that ride a ship, hold a cart, need the city or a festival");
    expect(d.causesDown.size() >= 3 && d.causesUp.size() >= 3, "causes both ways");
    for (const auto* list : {&d.causesDown, &d.causesUp})
        for (const auto& c : *list)
            expect(!market::forbiddenInName(c.id) && market::plainWords(c.words, 80) && market::plainWords(c.notice, 280), "a plain cause: " + c.id);
    // A host's own list.
    const auto host = parsed(R"([
        {"id": "seadog_feast", "words": "a feast", "goods": {"fish": -0.1}},
        {"id": "loose", "words": "the dogs loose", "goods": {"fish": 0.1}},
        {"id": "fight", "words": "a fair", "notice": "A Dog fight at the fair.", "goods": {"fish": 0.1}},
        {"id": "unknown", "words": "a fair", "goods": {"no_such_good": 0.1}},
        {"id": "Bad Id", "words": "a fair", "goods": {"fish": 0.1}},
        {"id": "needy", "words": "a fair", "needs": "moon", "goods": {"fish": 0.1}},
        {"id": "broken", "words": "a fair\nwith a line", "goods": {"fish": 0.1}},
        {"id": "fine", "words": "a fair at {market}", "notice": "A fair at {market} on {day}.", "goods": {"fish": 0.9, "ale": -0.1}, "days": 40, "times": 99},
        {"id": "fine", "words": "a second of an id", "goods": {"fish": 0.1}}])");
    const auto kinds = market::eventKindsOf(host.items());
    expect(kinds.size() == 1 && kinds[0].id == "fine", "only the plain one kept: " + std::to_string(kinds.size()));
    // (Its old `times`, the points an event once put on its market's Times, is read past: the kind is kept, 5.3.)
    expect(kinds[0].days == 7 && std::abs(kinds[0].goods[0].second - market::EventTargetMost) < 1e-9,
           "its numbers held to their ranges, its old Times points read past");
    const auto causes = market::causesOf(parsed(R"([{"id": "hotdog_tax", "words": "a tax"}, {"id": "a", "words": "the dog tax"},
                                                     {"id": "b", "words": "the salt tax"}])").items());
    expect(causes.size() == 1 && causes[0].id == "b", "a host's causes held to the same rules");
    // The Dungeon Master's words: plain, else the seeded list's stand in.
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    auto& soc = w.society();
    expect(!soc.setProsperity(-50, false, "the dogs' tax"), "the DM's words with the forbidden word are not taken");
    const auto& cause = soc.board().sliderCause;
    expect(cause.dir == -1 && !cause.dm && !market::forbiddenWord(cause.words) &&
               std::any_of(d.causesDown.begin(), d.causesDown.end(), [&](const market::Data::Cause& c) { return c.words == cause.words; }),
           "a seeded cause for the move down stands in: " + cause.words);
    expect(!soc.setProsperity(-60, false, "a cause\nof two lines") && soc.board().sliderCause.words == cause.words, "nor words with a line break");
    // Phase 9's second repair: the host's cause cleared (economy.sync then passes none) while the DM's is in force: the
    // world's own words of its way take over, quietly (told as the DM's were), standing or moved the same way.
    const auto worlds = [&](const market::SliderCause& c) {
        return !c.dm && std::any_of(d.causesDown.begin(), d.causesDown.end(), [&](const market::Data::Cause& k) { return k.words == c.words; });
    };
    expect(soc.setProsperity(-40, false, "the Crown's war levy") && soc.board().sliderCause.dm, "second repair: the DM's cause");
    soc.stagedBoard().sliderCause.told = true;
    expect(soc.setProsperity(-40, false, "") && worlds(soc.board().sliderCause) && soc.board().sliderCause.told,
           "second repair: cleared, standing: the world's words, not told again: " + soc.board().sliderCause.words);
    expect(soc.setProsperity(-40, false, "the Crown's war levy") && soc.board().sliderCause.dm, "second repair: the DM's again");
    expect(soc.setProsperity(-60, false, "") && worlds(soc.board().sliderCause), "second repair: cleared, moved the same way: the world's words");
    const auto kept = soc.board().sliderCause.words;
    expect(soc.setProsperity(-70, false, "") && soc.board().sliderCause.words == kept, "second repair: the world's own cause kept on a move the same way");
}

// 8.4: a seeded scheduler, 1 to 3 events a market a week at the rule's own 100 (x eventRate), each of the list's kinds a
// market can show (its shops deal in the goods), none twice in a week, one a day, committed a week ahead: the same world
// the same events; the rule at 0 none, at 300 about three times as many.
void eventsEachWeek()
{
    rules.clear();
    named.clear();
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    std::map<int, int> counts;
    for (std::int64_t week = 0; week < 300; ++week)
        for (const char* m : {"east", "west"})
        {
            const int n = w.eventsInWeek(m, week);
            expect(n >= 1 && n <= 3, "1 to 3 a week: " + std::to_string(n));
            ++counts[n];
        }
    expect(counts[1] > 20 && counts[2] > 100 && counts[3] > 20, "1, 2 and 3 all seen, 2 the most");
    expect(w.eventsInWeek("nowhere", 3) == 0, "no market, no events");
    std::map<std::string, market::StagedEvent> seen;
    std::map<std::string, std::int64_t> firstRolled;
    const auto start = today(w);
    for (int day = 0; day < 42; ++day)
    {
        nextMorning(w);
        for (const auto& e : w.society().board().events)
        {
            if (!seen.count(e.id))
                firstRolled[e.id] = today(w);
            seen[e.id] = e;
        }
    }
    expect(!seen.empty(), "events rolled");
    std::map<std::pair<std::string, std::int64_t>, std::vector<const market::StagedEvent*>> byWeek;
    for (const auto& [id, e] : seen)
    {
        byWeek[{e.market, e.day / 7}].push_back(&e);
        const auto* tp = w.society().tradePoints().find(e.market);
        expect(tp != nullptr, "a market's: " + e.market);
        expect(e.day > firstRolled[id] - 1 && e.day <= firstRolled[id] + 14, "committed ahead, never for a day gone: " + id);
        for (const auto& [g, P] : e.goods)
            expect((tp->sold.count(g) || tp->bought.count(g)) && std::abs(P) <= market::EventTargetMost + 1e-9,
                   e.kind + " at " + e.market + ": a good its shops deal in, inside the clamp: " + g);
        expect(!e.words.empty() && e.words.find('{') == std::string::npos && e.notice.find('{') == std::string::npos, "its words filled: " + e.words);
        expect(e.subject.empty() || e.subject.rfind("farm:", 0) == 0, "no ship's kind without a port: " + e.kind);
        expect(e.kind != "house_wedding" || e.market == "east" || e.market == "west", "the city's");
    }
    int full = 0;
    for (const auto& [key, list] : byWeek)
    {
        std::set<std::int64_t> days;
        std::set<std::string> kinds;
        for (const auto* e : list)
        {
            expect(days.insert(e->day).second, "one event a day at a market");
            expect(kinds.insert(e->kind).second, "a kind once a week at a market: " + e->kind);
        }
        // (A week rolled whole, ahead: as many as the scheduler says.)
        if (key.second > start / 7 + 1 && key.second < (start + 42) / 7)
        {
            ++full;
            expect(int(list.size()) == w.eventsInWeek(key.first, key.second),
                   key.first + " week " + std::to_string(key.second) + ": " + std::to_string(list.size()) + " of " +
                       std::to_string(w.eventsInWeek(key.first, key.second)));
        }
    }
    expect(full >= 6, "whole weeks seen: " + std::to_string(full));
    // The same world, the same events.
    auto again = strip();
    auto w2 = load(again);
    w2.tick(.6);
    for (int day = 0; day < 42; ++day)
        nextMorning(w2);
    expect(eventsOf(w2) == eventsOf(w), "the same world rolls the same events");
    // The rule: 0 none (and none rolled), 300 about three times as many.
    double most = 0;
    setRule(w, "eventRate", 300);
    for (std::int64_t week = 0; week < 100; ++week)
        most += w.eventsInWeek("east", week);
    expect(most / 100 >= 5, "at 300: " + std::to_string(most / 100) + " a week");
    setRule(w, "eventRate", 0);
    expect(w.eventsInWeek("east", 9) == 0, "at 0: none");
    const auto next = w.society().board().nextEvent;
    for (int day = 0; day < 22; ++day)
        nextMorning(w);
    expect(w.society().board().nextEvent == next, "and none rolled while it is 0");
    for (const auto& e : w.society().board().events)
        expect(e.status == "over" || e.status == "cancelled", "those rolled before are over by now");
}

// An event begins at its hour (8.4, 8.6's row): its goods pressed halfway toward their targets (cause "event"), its
// shelves as it says, its Times points in its market's score and its words the Times' cause, talk's world.event, the
// crowd, the price list's cause words; pressed again each morning while on, then over, talked of two days, then gone.
// Inside the clamp, and a player's own push still capped.
void anEventPressesItsGoods()
{
    rules = "rule \"eventRate\" 0\n";
    auto f = strip();
    auto w = load(f);
    rules.clear();
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    w.roads().camps.clear();
    auto& soc = w.society();
    expect(w.setTimeOfDay(9).ok, "morning");
    expect(soc.board().events.empty(), "no seeded events at eventRate 0");
    std::string problem;
    expect(w.addStagedEvent("east", "no_such_kind", today(w), 9, problem).empty() && !problem.empty(), "an unknown kind refused: " + problem);
    expect(w.addStagedEvent("nowhere", "good_catch", today(w), 9, problem).empty(), "an unknown market refused");
    expect(w.addStagedEvent("east", "good_catch", today(w) - 1, 9, problem).empty(), "a day gone refused");
    expect(w.addStagedEvent("east", "good_catch", today(w) + 15, 9, problem).empty(), "a day past next week refused: " + problem);
    expect(w.addStagedEvent("east", "rich_cargo", today(w), 9, problem).empty(), "a ship's kind without a port refused: " + problem);
    // The shellfish sold out first, to see the catch set out.
    for (const auto& ware : soc.wares("em"))
        if (items::baseOf(ware) == "shellfish")
            soc.consume(soc.tillOf("em"), ware, stock(w, "em", "shellfish"), "test: sold out");
    const auto id = w.addStagedEvent("east", "good_catch", today(w), 9.5, problem);
    expect(!id.empty(), "the DM adds a good catch at east: " + problem);
    const auto* e = w.stagedEvent(id);
    expect(e && e->status == "due" && e->by == "dm" && e->words == "a good catch" && e->notice.find("East") != std::string::npos,
           "due, the DM's, its words filled: " + (e ? e->notice : std::string()));
    std::set<std::string> goods;
    for (const auto& [g, P] : e->goods)
        goods.insert(g);
    expect(goods.count("shellfish") && !goods.count("fish"), "only what east's shops deal in");
    const int before = soc.timesScore("east");
    w.takeEvents();
    expect(w.setTimeOfDay(9.6).ok, "its hour");
    w.tick(.6);
    e = w.stagedEvent(id);
    const auto events = w.takeEvents();
    expect(e && e->status == "on" && count(events, "event begins", id) == 1, "it begins at its hour, once");
    double target = 0;
    for (const auto& [g, t] : e->goods)
        if (g == "shellfish")
            target = t;
    const double P = soc.pressure("east", "shellfish");
    expect(target < 0 && std::abs(P - target / 2) < .005 && soc.board().cell.at("east").at("shellfish").cause == "event",
           "halfway to its target, its cause the event: " + std::to_string(P));
    // (Its cheap goods set out where a shop sets them out: a stall that only buys fish from players has none to set out.)
    for (const auto& [g, t] : e->goods)
        for (const char* k : {"em", "es"})
            for (const auto& ware : soc.stagedShelfGoods(k))
                if (items::baseOf(ware) == g && t < 0)
                    expect(stock(w, k, ware) >= soc.shelfTarget(soc.tillOf(k), ware), "the catch set out (refill): " + ware);
    // (Decided 2026-11-12: an event moves its goods' prices and the talk, never its market's Times.)
    expect(soc.timesScore("east") == before && soc.marketTimes("east") == 0 && soc.timesCause("east") != "a good catch",
           "no Times points: the score as it was, the cause never the event's: " + std::to_string(soc.timesScore("east")));
    // (At a band's edge, 18 of the 20 that make good times or lean ones, the event on tips nothing: the Times' band is what
    // the bands' weekly cut, the staples' price factor, the day's thefts and the shops' budgets read. The catch's old five
    // points made 23 of it.)
    {
        auto& board = soc.stagedBoard();
        const int slider = board.slider, target = board.sliderTarget;
        for (const int edge : {18, -18})
        {
            board.slider = board.sliderTarget = edge;
            expect(soc.timesScore("east") == before - slider + edge && soc.marketTimes("east") == soc.marketTimes("west"),
                   "at the edge the event's market is in the Times of one with none: " + std::to_string(soc.timesScore("east")));
            expect(before - slider != 0 || soc.marketTimes("east") == 0, "ordinary still, at " + std::to_string(edge));
        }
        board.slider = slider, board.sliderTarget = target;
    }
    std::map<std::string, std::string> facts;
    w.talkTown("e1", facts);
    expect(facts["world.event"] == "good_catch" && facts["world.event_words"] == "a good catch" && facts["world.event_when"] == "now",
           "talk knows it: " + facts["world.event"] + " " + facts["world.event_when"]);
    // (Ordinary times: no cause said for them.)
    const bool noCause = facts.count("world.times_cause") == 0;
    expect(facts["world.times"] == "ordinary" && noCause, "and the times, with no cause");
    const auto* place = w.roadMarket("east");
    expect(place && w.eventCrowd(place->market), "a crowd at east's market place");
    std::map<std::string, std::string> west;
    w.talkTown("w1", west);
    expect(!west.count("world.event"), "west hears nothing of it");
    // The price list says why, and every factor stays inside the clamp.
    for (const auto& ware : soc.wares("em"))
        if (items::baseOf(ware) == "shellfish")
        {
            const auto row = soc.listing("player-ada", "em", ware);
            expect(row.trend <= 0 && (row.trend == 0 || row.cause == "a good catch"), "the list's cause: " + row.cause);
        }
    for (const auto& [g, target] : e->goods)
        for (const char* k : {"em", "es"})
            for (const auto& ware : soc.wares(k))
                if (items::baseOf(ware) == g)
                {
                    const double x = factorOf(w, k, ware, "east");
                    expect(x >= market::FactorLow - 1e-9 && x <= market::FactorHigh + 1e-9, "inside the clamp: " + std::to_string(x));
                }
    // A player's own push on it is capped as ever.
    testPurse(soc, "player-ada", 500);
    expect(w.setTimeOfDay(12).ok, "noon");
    for (int i = 0; i < 40; ++i)
        soc.trade("player-ada", "em", "shellfish", 1, true);
    const auto& pushed = soc.board().pushed;
    double push = 0;
    if (pushed.count("east") && pushed.at("east").count(soc.holderOf("player-ada")) && pushed.at("east").at(soc.holderOf("player-ada")).count("shellfish"))
        push = pushed.at("east").at(soc.holderOf("player-ada")).at("shellfish").amount;
    expect(push <= soc.marketRules().pushMost + 1e-9 && std::abs(soc.pressure("east", "shellfish")) <= market::PressureMost + 1e-9,
           "the holder's push capped: " + std::to_string(push));
    expect(soc.realBalanced(), "real money balanced");
    // Pressed again the next morning; over after its days; talked of two days; gone.
    soc.setPressure("east", "shellfish", target / 2, "event");
    nextMorning(w);
    const double next = soc.pressure("east", "shellfish");
    expect(next < target / 2 - .005 && next > target - 1e-6, "pressed again in the morning, never past its target: " + std::to_string(next));
    w.takeEvents();
    for (int day = 0; day < 2; ++day)
        nextMorning(w);
    e = w.stagedEvent(id);
    expect(e && e->status == "over" && count(w.takeEvents(), "event over", id) == 1, "over after its two days, once");
    expect(soc.timesScore("east") == before, "the Times as they were, on and over");
    facts.clear();
    w.talkTown("e1", facts);
    expect(facts["world.event_when"] == "past", "talked of as past: " + facts["world.event_when"]);
    for (int day = 0; day < 3; ++day)
        nextMorning(w);
    expect(!w.stagedEvent(id), "dropped once it is old news");
    // Cancelled by the DM: due or on, never twice.
    const auto other = w.addStagedEvent("west", "mill_stopped", today(w) + 1, 8, problem);
    expect(!other.empty() && w.cancelStagedEvent(other) && !w.cancelStagedEvent(other) && w.stagedEvent(other)->status == "cancelled",
           "the DM calls one off: " + problem);
}

// A cause for every slider move (8.5, T16): the DM's words, else the seeded list's for its way; told once in every market
// (a notice each, its crier's line); a move on the same way keeps its cause; world.times_cause and the Mind's Means say
// it while it is the largest part of the Times; the realm's harvests lean its way while the slider moves.
void theSlidersCause()
{
    rules = "rule \"eventRate\" 0\n";
    auto f = strip();
    auto w = load(f);
    rules.clear();
    w.tick(.6);
    auto& soc = w.society();
    const auto& d = market::data();
    const auto& cause = soc.board().sliderCause;
    expect(cause.words.empty() && cause.told, "no cause before a move");
    expect(soc.setProsperity(-60), "a move down with no words");
    const auto inDown = [&](const std::string& words) {
        return std::any_of(d.causesDown.begin(), d.causesDown.end(), [&](const market::Data::Cause& c) { return c.words == words; });
    };
    expect(cause.dir == -1 && inDown(cause.words) && !cause.told && !cause.dm && !cause.notice.empty(), "a seeded cause for it: " + cause.words);
    const auto first = cause.n;
    w.takeEvents();
    w.tick(.6);
    auto events = w.takeEvents();
    expect(count(events, "times cause") == int(w.roadMarkets().size()) && w.roadMarkets().size() == 2 && cause.told,
           "told once in every market: " + std::to_string(count(events, "times cause")));
    for (const auto& e : events)
        if (e.kind == "times cause")
            expect(e.detail == cause.notice, "the notice's words: " + e.detail);
    w.tick(.6);
    expect(count(w.takeEvents(), "times cause") == 0, "and not again");
    expect(soc.setProsperity(-80) && cause.n == first, "further the same way: the same cause");
    // The DM's words, their notice and cry.
    expect(soc.setProsperity(-90, false, "the Crown's war levy."), "the DM's words taken");
    expect(cause.words == "the Crown's war levy" && cause.dm && cause.notice == "Hard times ahead: the Crown's war levy." &&
               cause.cry == "Hear ye! The Crown's war levy! Hard times ahead!",
           "with their notice and cry: " + cause.notice + " / " + cause.cry);
    w.tick(.6);
    expect(count(w.takeEvents(), "times cause") == 2, "told again, the new cause");
    const auto dmCause = cause.n;
    expect(soc.setProsperity(-90, false, "the Crown's war levy") && soc.setProsperity(-95, false, "the Crown's war levy") && cause.n == dmCause,
           "the same words again (a sync, a nudge further) are the same cause");
    w.tick(.6);
    expect(count(w.takeEvents(), "times cause") == 0, "not told again");
    // Up: a cause for that way.
    expect(soc.setProsperity(40), "a move up");
    expect(cause.dir == 1 && std::any_of(d.causesUp.begin(), d.causesUp.end(), [&](const market::Data::Cause& c) { return c.words == cause.words; }),
           "a seeded cause up: " + cause.words);
    // Said as world.times_cause, and in the Means.
    soc.setProsperity(-70, true, "the Crown's war levy");
    std::map<std::string, std::string> facts;
    w.talkTown("e1", facts);
    expect(facts["world.times"] == "hard" && facts["world.times_cause"] == "the Crown's war levy",
           "talk: " + facts["world.times"] + ", since " + facts["world.times_cause"]);
    for (int day = 0; day < 8; ++day)
        nextMorning(w);
    std::string means;
    for (const auto& [id, life] : soc.state().residents)
        if (const auto* s = soc.statusOf(id); s && !s->market.empty() && means.empty())
            means = soc.meansNote(id);
    expect(means.find("Times here: hard, since the Crown's war levy.") != std::string::npos && means.size() <= 300, "the Means: " + means);
    // The harvests lean its way while it moves (and only then).
    soc.setProsperity(0, true);
    const auto harvests = [&] {
        int sum = 0;
        for (int i = 0; i < 400; ++i)
            sum += w.harvestOf("area" + std::to_string(i));
        return sum;
    };
    const int still = harvests();
    soc.setProsperity(-100);
    const int falling = harvests();
    soc.setProsperity(100);
    const int rising = harvests();
    expect(falling < still - 20 && rising > still + 20, "harvests lean the slider's way while it moves: " + std::to_string(falling) + " " +
                                                           std::to_string(still) + " " + std::to_string(rising));
    soc.setProsperity(0, true);
    expect(harvests() == still, "and not once it is there");
}

// The DM's Times nudge and harvest override (8.5, 12): their notices where they bear, unless the rule overrideNotices is
// 0; the nudge in the Times score and its words the cause; both with their expiry.
void theDMsNudgeAndHarvest()
{
    named = {{3, "fs_grain_long_granary"}};
    rules = "rule \"eventRate\" 0\n";
    auto probe = strip("EE.G...WW");
    auto p = load(probe);
    std::string label;
    for (const auto& farm : p.society().farms())
        if (farm.def->id == "grain_farm_1")
            label = p.society().farmLabel(farm, 0);
    auto f = strip("EE.G...WW", resident("fk", "Bran Long", "civilian", label, 0, 9.5, 3, 9.5));
    auto w = load(f);
    rules.clear();
    named.clear();
    w.tick(.6);
    auto& soc = w.society();
    expect(!w.nudgeTimes("nowhere", 30, 3, "boom") && !w.nudgeTimes("east", 30, 3, "a dog show"), "an unknown market or the forbidden word refused");
    expect(w.nudgeTimes("east", 40, 3, "boom at the port"), "a nudge");
    expect(soc.timesScore("east") == 40 && soc.timesCause("east") == "boom at the port" && soc.marketTimes("east") == 1, "in the score, its words the cause");
    w.takeEvents();
    w.tick(.6);
    auto events = w.takeEvents();
    expect(count(events, "times nudged", "east") == 1, "posted at east");
    for (const auto& e : events)
        if (e.kind == "times nudged")
            expect(e.detail == "Boom at the port.", "its words: " + e.detail);
    setRule(w, "overrideNotices", 0);
    expect(w.nudgeTimes("west", -30, 2, ""), "one with no words");
    w.tick(.6);
    expect(count(w.takeEvents(), "times nudged") == 0, "no notice with the rule off");
    expect(soc.timesCause("west") == "trade falling off", "its cause in plain words: " + soc.timesCause("west"));
    setRule(w, "overrideNotices", 1);
    for (int day = 0; day < 4; ++day)
        nextMorning(w);
    expect(soc.timesScore("east") == 0 && soc.board().nudges.empty() && count(w.takeEvents(), "nudge over") == 2, "both lapse");
    // The harvest override.
    const auto end = std::find_if(w.cartEnds().begin(), w.cartEnds().end(), [](const auto& e) { return e.farmId == "grain_farm_1"; });
    expect(end != w.cartEnds().end(), "the farm's cart end");
    expect(!w.overrideHarvest("no_such_area", 0, 30, "") && !w.overrideHarvest(end->area, 0, 30, "dog days"), "an unknown area, or the forbidden word, refused");
    expect(w.overrideHarvest(end->area, 0, 30, "blight in the fields") && w.harvestOf(end->area) == 0, "a poor harvest called");
    w.tick(.6);
    events = w.takeEvents();
    expect(count(events, "harvest called") == 1, "posted where its carts go");
    for (const auto& e : events)
        if (e.kind == "harvest called")
            expect(e.target == "east" && e.detail.rfind("Blight in the fields: a poor season for ", 0) == 0 && e.detail.find(end->area) == std::string::npos &&
                       e.detail.find('_') == std::string::npos,
                   "at east, in the words of what its farms grow, never the area's id: " + e.detail);
    w.tick(.6);
    expect(count(w.takeEvents(), "harvest called") == 0, "once");
}

// Doc 79, Phase 9: the DM's live acts through the game (Game::applyEconomyCommand), each with its clamps, refusals and
// outcome; the authored band against the DM's timed one; grants and fines through the faucet door; "reset prices" eased
// over a day; the harvest called and cleared; the Prosperity report; the host's market and status overrides.
Result dmAct(game::Game& g, const std::string& kind, const std::string& target,
             std::initializer_list<std::pair<const char*, json::Value>> fields);
void theDMsLevers()
{
    named = {{3, "fs_grain_long_granary"}};
    rules = "rule \"eventRate\" 0\n";
    auto probe = strip("EE.G...WW");
    auto p = load(probe);
    std::string label;
    for (const auto& farm : p.society().farms())
        if (farm.def->id == "grain_farm_1")
            label = p.society().farmLabel(farm, 0);
    auto f = strip("EE.G...WW", resident("fk", "Bran Long", "civilian", label, 0, 9.5, 3, 9.5) + "band \"e2\" rich\n");
    rules.clear();
    named.clear();
    const auto root = onDisk(f, "events-dmlevers");
    game::Game g(optionsOn(root));
    std::string problem;
    expect(g.start(problem), "a game on the strip: " + problem);
    auto& w = g.world();
    auto& soc = w.society();
    w.tick(.6);
    w.roads().camps.clear();
    const auto act = [&](const std::string& kind, const std::string& target, std::initializer_list<std::pair<const char*, json::Value>> fields) {
        return dmAct(g, kind, target, fields);
    };
    // The report, from the start.
    const auto& report = soc.report();
    expect(report.number("v") == 1 && report.array("markets").size() >= 2 && report.object("prosperity").number("target") == 0,
           "the report at start: " + json::dump(report).substr(0, 200));
    // times.nudge: clamped, never refused for a number; an unknown market refused; 0 lifts it.
    auto r = act("times.nudge", "east", {{"points", 9999}, {"days", 9999}, {"words", "boom at the port"}});
    expect(r.ok && soc.board().nudges.at("east").points == 100 && soc.board().nudges.at("east").until <= w.calendarDays() + 366.01,
           "a nudge clamped: " + r.message);
    expect(r.message.find("+100 for 366 days: boom at the port.") != std::string::npos, "its outcome: " + r.message);
    expect(json::dump(soc.report()).find("\"nudge\":{\"points\":100") != std::string::npos, "the report has it");
    expect(!act("times.nudge", "nowhere", {{"points", 10}}).ok, "an unknown market refused");
    expect(!act("times.nudge", "east", {{"points", 10}, {"words", "a dog show"}}).ok, "the forbidden word refused");
    expect(act("times.nudge", "east", {{"points", 0}}).ok && !soc.board().nudges.count("east"), "points 0 lifts it");
    // harvest.override and its clearing.
    const auto end = std::find_if(w.cartEnds().begin(), w.cartEnds().end(), [](const auto& e) { return e.farmId == "grain_farm_1"; });
    expect(end != w.cartEnds().end(), "the farm's cart end");
    expect(!act("harvest.override", end->area, {{"word", "splendid"}}).ok, "an unknown word refused");
    expect(!act("harvest.override", "no_such_area", {{"word", "poor"}}).ok, "an unknown area refused");
    r = act("harvest.override", end->area, {{"word", "poor"}, {"words", "blight in the fields"}});
    expect(r.ok && w.harvestOf(end->area) == 0, "a poor harvest called: " + r.message);
    {
        // (Second repair: the outcome names the area as the Prosperity tab does, not by its id.)
        std::string name = end->area;
        for (const auto& a : items::farms().areas)
            if (a.id == end->area && !a.name.empty())
                name = a.name;
        expect(r.message.rfind("The harvest at " + name + " called poor", 0) == 0 && name != end->area, "second repair: named: " + r.message);
    }
    const double until = soc.board().harvestOverride.at(end->area).until;
    expect(until > w.calendarDays() && until <= w.calendarDays() + 92, "its days the season's left by default");
    w.tick(.6);
    for (int t = 0; t < 4; ++t)
        g.tick(.05);
    bool posted = false;
    for (const auto& [id, d] : g.documents().all())
        posted |= d.what == "harvest:" + end->area;
    expect(posted, "its notice up");
    w.takeEvents();
    r = act("harvest.override", end->area, {{"word", "clear"}});
    expect(r.ok && !soc.board().harvestOverride.count(end->area) && count(w.takeEvents(), "harvest over", end->area) == 1, "cleared: " + r.message);
    posted = false;
    for (const auto& [id, d] : g.documents().all())
        posted |= d.what == "harvest:" + end->area;
    expect(!posted, "its notice down at once");
    expect(!act("harvest.override", end->area, {{"word", "clear"}}).ok, "nothing to clear");
    // The harvest bias (12: a realm-wide lean): the score moved by bias/100.
    {
        int good = 0, goodBiased = 0;
        auto spec = soc.authored().economy;
        for (int i = 0; i < 60; ++i)
        {
            nextMorning(w);
            good += w.harvestOf(end->area) == 2;
        }
        spec.rules["harvestBias"] = 20;
        soc.adoptEconomy(spec);
        for (int i = 0; i < 60; ++i)
        {
            nextMorning(w);
            goodBiased += w.harvestOf(end->area) == 2;
        }
        spec.rules.erase("harvestBias");
        soc.adoptEconomy(spec);
        expect(goodBiased >= good, "the harvest bias leans the harvests good: " + std::to_string(good) + " to " + std::to_string(goodBiased));
    }
    // event.add and event.cancel.
    r = act("event.add", "east", {{"kind", "house_wedding"}, {"day", double(today(w) + 2)}, {"hour", 99}});
    expect(r.ok && r.message.rfind(r.targetId + ": ", 0) == 0 && w.stagedEvent(r.targetId)->hour == 23, "an event, its hour clamped: " + r.message);
    expect(act("event.cancel", r.targetId, {}).ok && w.stagedEvent(r.targetId)->status == "cancelled", "cancelled");
    expect(!act("event.cancel", "e999", {}).ok, "an unknown event refused");
    expect(!act("event.add", "east", {{"kind", "no_such_kind"}}).ok, "an unknown kind refused");
    r = act("event.add", "east", {{"kind", "house_wedding"}, {"day", 1e9}});
    expect(!r.ok || w.stagedEvent(r.targetId)->day < (today(w) / 7 + 2) * 7, "a day past next week clamped into it");
    // ship.storm with no ship (no port here).
    expect(!act("ship.storm", "s1", {}).ok, "no such ship");
    // prices.reset: half at half a day, none at a day; a touched cell eases from its touch.
    soc.setPressure("east", "meal", .4, "players");
    soc.setPressure("east", "nails", .2, "players");
    expect(!act("prices.reset", "nowhere", {}).ok, "an unknown market refused");
    r = act("prices.reset", "east", {});
    expect(r.ok && soc.board().resets.at("east").words == "fresh stock everywhere", "reset, with the default words: " + r.message);
    const double start = soc.marketClock(), meal = soc.pressure("east", "meal");
    {
        auto saved = w.save();
        saved.calendarDays += .25;
        expect(w.restore(saved).ok, "a quarter day on");
        w.tick(.6);
        soc.setPressure("east", "nails", .2, "players");   // (Touched mid-reset: it eases from now.)
        saved = w.save();
        saved.calendarDays += .25;
        expect(w.restore(saved).ok, "half a day on");
        w.tick(.6);
        const double at = soc.marketClock();
        const double free = market::decayed(meal, start, at, market::halfLife("meal", soc.marketRules()));
        const double share = (start + 1 - at) / 1.;
        expect(std::abs(soc.pressure("east", "meal") - free * share) < .01,
               "meal halfway eased: " + std::to_string(soc.pressure("east", "meal")) + " for " + std::to_string(free * share) + " start " +
                   std::to_string(start) + " at " + std::to_string(at) + " cal " + std::to_string(w.calendarDays()) + " last " +
                   std::to_string(soc.board().resets.count("east") ? soc.board().resets.at("east").last : -1) + " free " + std::to_string(free));
        expect(soc.pressure("east", "nails") > .2 * share, "a cell touched since eases from its touch: " + std::to_string(soc.pressure("east", "nails")));
        saved = w.save();
        saved.calendarDays += .6;
        expect(w.restore(saved).ok, "a day on");
        w.tick(.6);
        expect(soc.pressure("east", "meal") == 0 && soc.pressure("east", "nails") == 0 && !soc.board().resets.count("east"),
               "at a day: all at 0, the reset over");
    }
    expect(act("prices.reset", "all", {{"words", "a dog fair"}}).ok && soc.board().resets.at("west").words == "fresh stock everywhere",
           "all markets; words that won't do are the default");
    // band.override over the authored band (e2's household authored rich).
    const auto h2 = soc.householdOf("e2");
    expect(!h2.empty() && soc.statusOf("e2") && soc.statusOf("e2")->band == status::Rich && soc.statusOf("e2")->overridden,
           "the authored band holds the whole household: " + std::to_string(soc.statusOf("e2") ? soc.statusOf("e2")->band : -1));
    r = act("band.override", "e2", {{"band", "poor"}, {"days", 500}});
    expect(r.ok && soc.statusOf("e2")->band == status::Poor && soc.state().staged.overrides.at(h2).until == soc.state().budgetDay + 112,
           "the DM's timed band wins, its days clamped: " + r.message);
    expect(r.message.rfind(w.entity("e2")->name + "'s household is held poor", 0) == 0, "second repair: the resident named: " + r.message);
    expect(act("band.override", "e2", {{"band", "clear"}}).ok && soc.statusOf("e2")->band == status::Rich, "cleared: the authored band again");
    expect(!act("band.override", "e2", {{"band", "wealthy"}}).ok && !act("band.override", "nobody", {{"band", "poor"}}).ok, "a bad word or resident refused");
    // character.grant: a faucet capped a day, a fine never past the cash; the door balanced; nothing unregistered.
    w.addPlayer("player-ada", "Ada");
    testPurse(soc, "player-ada", 30);
    soc.openRealBalance();
    const auto base = soc.account("player-ada")->cash;
    r = act("character.grant", "player-ada", {{"coins", 300}, {"reason", "for the bridge"}});
    expect(r.ok && soc.account("player-ada")->cash == base + 300 && r.message == "Granted 300p: for the bridge.", "a grant: " + r.message);
    expect(!act("character.grant", "player-ada", {{"coins", 9999}, {"reason", "more"}}).ok && soc.account("player-ada")->cash == base + 300,
           "over the day's cap (500): refused");
    expect(act("character.grant", "player-ada", {{"coins", 200}, {"reason", "the rest"}}).ok && soc.account("player-ada")->cash == base + 500, "to the cap");
    expect(!act("character.grant", "player-ada", {{"coins", 1}, {"reason", "one more"}}).ok, "and no further today");
    {
        const auto had = soc.account("player-ada")->cash;
        r = act("character.grant", "player-ada", {{"coins", -2500}, {"reason", "for the broken cart"}});
        expect(r.ok && soc.account("player-ada")->cash == 0 &&
                   r.message == "Fined " + std::to_string(had) + "p (all player-ada had; 2500p asked): for the broken cart.",
               "a fine of no more than the purse, told as what it took: " + r.message);
    }
    expect(!act("character.grant", "player-ada", {{"coins", -5}, {"reason", "again"}}).ok, "nothing left to fine");
    expect(!act("character.grant", "e1", {{"coins", 5}, {"reason", "x"}}).ok, "a resident isn't granted");
    expect(!act("character.grant", "player-ada", {{"coins", 5}, {"reason", ""}}).ok, "a reason is needed");
    expect(!act("character.grant", "player-ada", {{"coins", 0}, {"reason", "nothing"}}).ok, "nothing to grant");
    expect(soc.realBalanced() && soc.unregisteredFaucets() == 0 && soc.faucetToday("player-ada", "grant") == 500, "balanced; registered; tallied");
    // The host's overrides: a cap held to 5x, a status override that sums wrong falling back, the market's events and ships.
    {
        auto spec = soc.authored().economy;
        json::Value v;
        expect(json::parse(R"({"caps": {"grantsADay": 99999, "rewardsADay": 99999, "nope": 1},
                               "shares": [{"at": -100, "shares": [40, 40, 40, 20]}, {"at": 100, "shares": [2, 12, 70, 16]}],
                               "bands": {"x": 1}})", v, problem), problem);
        spec.overrides.status = v;
        expect(json::parse(R"({"events": {"perWeek": 9, "list": ["house_wedding", {"id": "dog_show", "words": "the show"}]},
                               "wharf": {"ships": ["the Heron", "the Dogfish"], "never": ["salt"]}, "rules": 1})", v, problem), problem);
        spec.overrides.market = v;
        spec.overrides.neverImported = {"salt", "no_such_good"};
        spec.overrides.neverSet = true;
        std::vector<std::string> warnings;
        soc.adoptOverrides(spec.overrides, &warnings);
        expect(soc.statusTable().cap("grantsADay") == 2500 && soc.statusTable().cap("rewardsADay") == 1000, "caps held to their ceilings");
        expect(soc.statusTable().shares.front().shares[0] == status::defaults().shares.front().shares[0], "shares summing to 140 fall back");
        const auto& m = soc.market();
        expect(m.eventsPerWeek == 3 && m.events.size() == 1 && m.events[0].id == "house_wedding", "perWeek clamped, the list taken, the forbidden kind out");
        expect(m.shipNames == std::vector<std::string>{"the Heron"} && m.shipNever.count("salt") && !m.shipNever.count("no_such_good"),
               "ships' names held to the name rule; never-imported known goods only");
        const auto said = [&](const std::string& part) {
            return std::any_of(warnings.begin(), warnings.end(), [&](const std::string& x) { return x.find(part) != std::string::npos; });
        };
        expect(said("caps.nope") && said("sums to 140") && said("bands skipped") && said("rules skipped") && said("wharf.never") &&
                   said("no_such_good"),
               "each skipped with a warning");
        soc.adoptOverrides({}, nullptr);
        expect(&soc.market() == &market::data() && soc.statusTable().cap("grantsADay") == 500, "none: the shipped defaults again");
    }
    expect(soc.realBalanced(), "money still balanced");
    std::filesystem::remove_all(root);
}

// A ship's rich cargo and a ship lost (8.4, 8.6: Phase 5 left both to this phase): an event riding a ship on the port's
// timetable, its words naming the ship; the cargo's comes in with it (cancelled if a storm keeps it out); the lost one
// never comes, is "lost at sea" on the timetable, world.ships "lost", its cargo dearer and the price list saying why.
void shipsInTheEvents()
{
    rules = "rule \"eventRate\" 0\n";
    auto f = harbour(true, true, resident("ef", "Fen Gill", "merchant", "fishmonger at the Fish Hall", 0, 4.5, 1, 6.5) +
                                     resident("ev", "Vera Lees", "merchant", "vintner at the Wine Vault", 0, 5.5, 1, 7.5));
    auto w = load(f);
    rules.clear();
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    w.roads().camps.clear();
    auto& soc = w.society();
    const auto dueShips = [&] {
        std::vector<market::Ship> out;
        for (const auto& s : soc.board().ships)
            if (s.status == "due" && s.day > today(w))
                out.push_back(s);
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.day < b.day; });
        return out;
    };
    // A good catch where a shop sets fish out: it is set out as the catch comes in (8.6: "as the event says").
    {
        std::string problem;
        expect(w.setTimeOfDay(8).ok, "morning");
        const auto catchId = w.addStagedEvent("east", "good_catch", today(w), 8.2, problem);
        expect(!catchId.empty(), "a good catch at the harbour: " + problem);
        std::vector<std::pair<std::string, std::string>> setOut;
        for (const auto& [g, t] : w.stagedEvent(catchId)->goods)
            for (const char* k : {"em", "es", "ek", "eg", "et", "ef"})
                for (const auto& ware : soc.stagedShelfGoods(k))
                    if (items::baseOf(ware) == g && t < 0)
                    {
                        soc.consume(soc.tillOf(k), ware, stock(w, k, ware), "test: sold out");
                        setOut.push_back({k, ware});
                    }
        expect(!setOut.empty(), "a shop at the harbour sets out the catch's fish");
        expect(w.setTimeOfDay(8.3).ok, "its hour");
        w.tick(.6);
        expect(w.stagedEvent(catchId)->status == "on", "the catch is in");
        for (const auto& [k, ware] : setOut)
            expect(stock(w, k, ware) >= soc.shelfTarget(soc.tillOf(k), ware), "set out: " + ware + " at " + k);
        expect(w.cancelStagedEvent(catchId), "called off, for the ships below");
    }
    std::vector<market::Ship> due;
    for (int day = 0; day < 20 && due.size() < 2; ++day)
    {
        nextMorning(w);
        due = dueShips();
        if (due.size() >= 2 && due[0].day / 7 != due[1].day / 7)
            due.clear();                            // (Two in one week, for the two kinds.)
    }
    expect(due.size() >= 2, "two ships due in a week");
    std::string problem;
    const auto rich = w.addStagedEvent("east", "rich_cargo", due[0].day, 0, problem);
    expect(!rich.empty(), "a rich cargo on the first: " + problem);
    const auto* e = w.stagedEvent(rich);
    expect(e->subject == "ship:" + due[0].id && e->day == due[0].day && e->words == due[0].name + "'s rich cargo" &&
               e->notice.rfind("The " + due[0].name.substr(4) + " is due on ", 0) == 0,
           "it rides " + due[0].name + ": " + e->words + " / " + e->notice);
    // (The repair: the kind's own rich goods first, where east deals in them, then the ship's everyday cargo.)
    {
        const auto* tp = soc.tradePoints().find("east");
        std::vector<std::string> dealt;
        for (const auto& [g, P] : market::data().eventKind("rich_cargo")->goods)
            if (tp->sold.count(g) || tp->bought.count(g))
                dealt.push_back(g);
        expect(!dealt.empty(), "east deals in some of a rich cargo's goods");
        for (std::size_t i = 0; i < dealt.size() && i < e->goods.size(); ++i)
            expect(e->goods[i].first == dealt[i], "a rich cargo of its own rich goods first: " + e->goods[i].first);
        expect(e->noticeOn.rfind("The " + due[0].name.substr(4) + " is in with a rich cargo: ", 0) == 0, "and its notice once she is in: " + e->noticeOn);
    }
    const auto lost = w.addStagedEvent("east", "lost_ship", due[0].day, 0, problem);
    expect(!lost.empty() && w.stagedEvent(lost)->subject == "lost:" + due[1].id, "a lost ship: the next ship no event has yet: " + problem);
    // The rich cargo's ship docks: the event begins with it.
    while (today(w) < due[0].day)
        nextMorning(w);
    expect(w.stagedEvent(rich)->status == "due", "due till its ship is in");
    w.takeEvents();
    dock(w, due[0]);
    w.tick(.6);
    expect(w.stagedEvent(rich)->status == "on" && count(w.takeEvents(), "event begins", rich) == 1, "it begins as its ship is in");
    expect(w.stagedEvent(rich)->notice.rfind("The " + due[0].name.substr(4) + " is in with a rich cargo: ", 0) == 0 && w.stagedEvent(rich)->noticeOn.empty(),
           "its notice says she is in, never still due: " + w.stagedEvent(rich)->notice);
    for (const auto& [g, target] : w.stagedEvent(rich)->goods)
        expect(target < 0 && soc.pressure("east", g) < 0, "its goods cheaper: " + g);
    // She sails that evening with her cargo's two days still running: the notice says she has sailed, told once, never
    // "is in" of a ship gone (the slice 2 verify's leftover); the event stays on.
    {
        expect(!w.stagedEvent(rich)->noticeAfter.empty(), "a notice kept for when she has sailed");
        w.takeEvents();
        expect(w.setTimeOfDay(due[0].sails + .1).ok, "to her sailing hour");
        for (int t = 0; t < 4; ++t)
            w.tick(.5);
        const auto* gone = shipById(w, due[0].id);
        const auto* ev = w.stagedEvent(rich);
        expect(gone && gone->status == "sailed" && ev->status == "on", "she has sailed, her cargo's event still on");
        expect(ev->notice.rfind("The " + due[0].name.substr(4) + " has sailed, ", 0) == 0 && ev->notice.find(" is in") == std::string::npos &&
                   ev->noticeAfter.empty(),
               "its notice says she has sailed, never still in: " + ev->notice);
        expect(count(w.takeEvents(), "event notice", rich) == 1, "the new notice told once");
        for (int t = 0; t < 4; ++t)
            w.tick(.5);
        expect(count(w.takeEvents(), "event notice", rich) == 0, "and never again");
        // (Saved with the Board, so a restart between her docking and her sailing keeps it; an event over has none.)
        auto rolled = market::decode(market::encode(soc.board()));
        for (auto& e : rolled.events)
            if (e.id == rich)
                e.status = "due", e.noticeAfter = "The Gull has sailed.";
        const auto kept = market::decode(market::encode(rolled));
        const auto same = std::find_if(kept.events.begin(), kept.events.end(), [&](const auto& e) { return e.id == rich; });
        expect(same != kept.events.end() && same->noticeAfter == "The Gull has sailed.", "kept through a save while it waits");
        for (auto& e : rolled.events)
            if (e.id == rich)
                e.status = "over";
        market::mendEvents(rolled);
        expect(std::all_of(rolled.events.begin(), rolled.events.end(), [](const auto& e) { return e.status != "over" || e.noticeAfter.empty(); }),
               "none kept once it is over");
    }
    // The lost ship's day.
    while (today(w) < due[1].day)
        nextMorning(w);
    w.tick(.6);
    const auto* ship = shipById(w, due[1].id);
    expect(ship && ship->status == "storm" && ship->lost, "lost at sea, never in");
    expect(w.stagedEvent(lost)->status == "on", "its story on");
    // (The repair: her loss a trouble of the sea, never a late wagon, and her name kept off the timetable.)
    expect(!soc.board().troubles.empty() && soc.board().troubles.back().kind == "lost" && soc.board().lostShips.count(due[1].name),
           "a lost ship's trouble, and her name remembered");
    expect(w.shipsNotice("east").find(due[1].name.substr(4) + " is lost at sea.") != std::string::npos, "the timetable says so: " + w.shipsNotice("east"));
    std::map<std::string, std::string> facts;
    w.talkTown("d1", facts);
    expect(facts["world.ships"] == "lost" && facts["world.ship_name"] == due[1].name && facts["world.event"] == "lost_ship", "talk knows it");
    for (const auto& [g, target] : w.stagedEvent(lost)->goods)
    {
        expect(target > 0 && soc.pressure("east", g) > 0, "its cargo dearer: " + g + " " + std::to_string(soc.pressure("east", g)));
        for (const auto* k : EastKeepers)
            for (const auto& ware : soc.wares(k))
                if (items::baseOf(ware) == g)
                {
                    const auto row = soc.listing("player-ada", k, ware);
                    expect(row.trend <= 0 || row.cause.find("lost") != std::string::npos || row.cause.find("loss") != std::string::npos,
                           "the list says why: " + row.cause);
                }
    }
    // A storm keeps a ship out: its rich cargo is called off.
    std::vector<market::Ship> more;
    for (int day = 0; day < 20 && more.empty(); ++day)
    {
        nextMorning(w);
        more = dueShips();
    }
    expect(!more.empty(), "another ship due");
    const auto stormed = w.addStagedEvent("east", "rich_cargo", more[0].day, 0, problem);
    expect(!stormed.empty() && w.stormShip(more[0].id), "its ship stormbound: " + problem);
    w.takeEvents();
    w.tick(.6);
    expect(w.stagedEvent(stormed)->status == "cancelled" && count(w.takeEvents(), "event cancelled", stormed) == 1, "called off: its ship never came");
    expect(soc.board().troubles.back().kind == "storm", "a storm's trouble, its own");
    expect(soc.realBalanced(), "real money balanced");
    // A lost ship never sails again (the repair: her name was only kept while she was on the timetable): ninety days of
    // mornings at a ship most days, and none of her name is posted; through a save, too.
    setRule(w, "shipsPerWeek", 6);
    std::set<std::string> names;
    for (int day = 0; day < 90; ++day)
    {
        nextMorning(w);
        for (const auto& s : soc.board().ships)
            if (s.port == "east" && s.status == "due")
            {
                names.insert(s.name);
                expect(s.name != due[1].name, "never of the lost ship's name: " + s.name);
            }
    }
    expect(names.size() >= 6 && soc.board().lostShips.count(due[1].name), "other ships sail: " + std::to_string(names.size()));
    const auto back = market::decode(parsed(json::dump(market::encode(soc.board()))));
    expect(back.lostShips.size() == soc.board().lostShips.size() && back.lostShips.count(due[1].name), "the lost ships' names through the wire");
}

// A lame ox delaying a farm's cart (8.4): the cart kept home that day, its goods a little dearer, its words naming the farm.
void aLameOx()
{
    named = {{3, "fs_grain_long_granary"}};
    rules = "rule \"eventRate\" 0\n";
    auto probe = strip("EE.G...WW");
    auto p = load(probe);
    std::string label;
    for (const auto& farm : p.society().farms())
        if (farm.def->id == "grain_farm_1")
            label = p.society().farmLabel(farm, 0);
    auto f = strip("EE.G...WW", resident("fk", "Bran Long", "civilian", label, 0, 9.5, 3, 9.5));
    auto w = load(f);
    rules.clear();
    named.clear();
    w.tick(.6);
    const auto end = std::find_if(w.cartEnds().begin(), w.cartEnds().end(), [](const auto& e) { return e.farmId == "grain_farm_1"; });
    expect(end != w.cartEnds().end(), "the farm's cart end");
    w.society().stagedBoard().harvestOverride[end->area] = {2, w.calendarDays() + 60};
    const auto cartToday = [&] {
        for (const auto& d : w.society().board().committed)
            if (d.kind == "cart" && d.from == "grain_farm_1" && d.day == today(w))
                return true;
        for (const auto& c : w.roads().caravans)
            if (c.kind == "cart" && c.from == "grain_farm_1" && std::floor(c.departed) == double(today(w)))
                return true;
        return false;
    };
    nextMorning(w);
    expect(cartToday(), "a good harvest: its cart comes every morning");
    std::string problem;
    const auto id = w.addStagedEvent("east", "lame_ox", today(w) + 1, 5, problem);
    expect(!id.empty(), "a lame ox: " + problem);
    const auto* e = w.stagedEvent(id);
    expect(e->subject == "farm:grain_farm_1" && e->words.rfind("the lame ox that held up the ", 0) == 0 && e->words.size() > 34 &&
               e->words.size() > 5 && e->words.compare(e->words.size() - 5, 5, " cart") == 0,
           "its farm's cart: " + e->words);
    std::map<std::string, double> before;
    for (const auto& [g, target] : e->goods)
        before[g] = w.society().pressure("east", g);
    expect(!before.empty(), "the goods its cart brings");
    nextMorning(w);
    expect(!cartToday(), "the cart kept home");
    expect(w.setTimeOfDay(5.5).ok, "its hour");
    w.tick(.6);
    expect(w.stagedEvent(id)->status == "on", "and its story on");
    for (const auto& [g, P] : before)
        expect(w.society().pressure("east", g) > P, "dearer: " + g);
    nextMorning(w);
    expect(cartToday(), "the next day it comes");
}

// A world with no port, no wagon and no farm (one market, no carters) still has its events (D25: what keeps a quiet world
// from looking dead), of the goods its shops deal in; never a ship's or a cart's.
void aWorldWithNothingButAMarket()
{
    rules.clear();
    named.clear();
    auto f = strip("EE", "", false);
    auto w = load(f);
    w.tick(.6);
    expect(w.roadMarkets().size() == 1 && w.ports().empty() && w.cartEnds().empty(), "one market, no port, no farm");
    std::set<std::string> kinds;
    int begun = 0;
    for (int day = 0; day < 28; ++day)
    {
        nextMorning(w);
        for (int t = 0; t < 24; ++t)
        {
            expect(w.setTimeOfDay(double(t) + .5).ok, "through the day");
            w.tick(.6);
        }
        for (const auto& e : w.takeEvents())
            begun += e.kind == "event begins";
        for (const auto& e : w.society().board().events)
            kinds.insert(e.kind);
    }
    expect(begun >= 4, "its events begin: " + std::to_string(begun));
    for (const auto& k : kinds)
        expect(k != "rich_cargo" && k != "lost_ship" && k != "lame_ox", "never a ship's or a cart's: " + k);
    expect(w.society().realBalanced(), "real money balanced");
}

// The festival's buying ahead (8.4): the day before a festival, at a market whose shops sell its goods.
void aFestivalsBuyingAhead()
{
    rules = "rule \"eventRate\" 300\n";
    auto f = strip();
    auto w = load(f);
    rules.clear();
    w.tick(.6);
    std::int64_t festival = -1;
    for (std::int64_t day = today(w) + 10; day < today(w) + 400 && festival < 0; ++day)
        if (calendar::festivalDay(double(day) + .5))
            festival = day;
    expect(festival > 0, "a festival in the year");
    auto saved = w.save();
    saved.calendarDays = double(festival - 9) + .2;
    expect(w.restore(saved).ok, "to nine days before it");
    w.tick(.6);
    std::set<std::string> seen;
    for (int day = 0; day < 9; ++day)
    {
        nextMorning(w);
        for (const auto& e : w.society().board().events)
            if (e.kind == "festival_buying")
            {
                seen.insert(e.id);
                expect(calendar::festivalDay(double(e.day + 1) + .5), "the day before a festival: " + std::to_string(e.day));
            }
    }
    expect(!seen.empty(), "the inns and kitchens buying ahead");
}

// Money stays real (9, 7.6): events never touch money; a player trading through weeks of them is paid and charged as
// ever, every coin a registered faucet or sink, real money balanced; buying where an event made a good cheap and selling
// it back there or anywhere else never pays (I2, I3's backstop).
void moneyStaysReal()
{
    rules = "rule \"eventRate\" 300\n";
    auto f = harbour();
    auto w = load(f);
    rules.clear();
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    w.roads().camps.clear();
    auto& soc = w.society();
    testPurse(soc, "player-ada", 2000);
    expect(soc.realBalanced(), "balanced at the start");
    int begun = 0, compared = 0;
    for (int day = 0; day < 21; ++day)
    {
        nextMorning(w);                             // (A restore: the gate's counts are its own after it.)
        const auto faucets = soc.realFaucets();
        for (int t = 6; t < 22; ++t)
        {
            expect(w.setTimeOfDay(double(t) + .3).ok, "through the day");
            w.tick(.6);
        }
        for (const auto& e : w.takeEvents())
            begun += e.kind == "event begins";
        expect(soc.realFaucets() == faucets, "no faucet but a player's own trade");
        expect(w.setTimeOfDay(12).ok, "noon");
        // Every event's good: bought where it is pressed down, sold back anywhere else never pays.
        for (const auto& e : soc.board().events)
            if (e.status == "on")
                for (const auto& [g, target] : e.goods)
                    for (const auto* k : EastKeepers)
                        for (const auto& ware : soc.wares(k))
                            if (items::baseOf(ware) == g)
                            {
                                const int q = std::min(3, stock(w, k, ware));
                                const auto buy = q > 0 ? soc.quote("player-ada", k, ware, q, true) : EconomyResult{};
                                if (!buy.ok)
                                    continue;
                                soc.create("player-ada", ware, q, "test: to sell");
                                for (const char* other : {"em", "es", "ek", "eg", "et", "wm", "wg"})
                                    if (const auto sell = soc.quote("player-ada", other, ware, q, false); sell.ok)
                                    {
                                        ++compared;
                                        expect(sell.total < buy.total, "never pays: " + ware + " bought for " + std::to_string(buy.total) +
                                                                           ", sold for " + std::to_string(sell.total));
                                    }
                                soc.consume("player-ada", ware, q, "test: put back");
                                if (soc.trade("player-ada", k, ware, 1, true).ok)
                                    soc.trade("player-ada", k, ware, 1, false);
                                const double x = factorOf(w, k, ware, "east");
                                expect(x >= market::FactorLow - 1e-9 && x <= market::FactorHigh + 1e-9, "inside the clamp");
                            }
        expect(soc.realBalanced() && soc.unregisteredFaucets() == 0, "real money balanced, every faucet registered");
    }
    expect(begun >= 10, "three weeks of events: " + std::to_string(begun));
    expect(compared > 0, "a buy and a sale compared");
}

// Saves (14): the Board's events, its cause, its nudges, a harvest override's words and a lost ship round-trip through the
// wire and a world's save and restore, an event on staying on and never begun twice; a save from before Phase 7 (Phases
// 4-6's Board) loads with none and rolls them from that day; a save that lies is put right, never refused.
void savesAndOlderSaves()
{
    rules = "rule \"eventRate\" 0\n";
    auto f = harbour();
    auto w = load(f);
    rules.clear();
    w.tick(.6);
    w.roads().camps.clear();
    auto& soc = w.society();
    for (int day = 0; day < 3; ++day)
        nextMorning(w);
    setRule(w, "eventRate", 100);
    nextMorning(w);
    expect(!soc.board().events.empty(), "events rolled");
    std::string problem;
    const auto on = w.addStagedEvent("east", "drovers_in", today(w), 0, problem);
    expect(!on.empty(), "an event to be on: " + problem);
    w.tick(.6);
    expect(w.stagedEvent(on)->status == "on", "on");
    soc.setProsperity(-40, false, "the Crown's war levy");
    expect(w.nudgeTimes("east", 25, 5, "boom at the port"), "a nudge");
    soc.stagedBoard().harvestOverride["grain"] = {0, w.calendarDays() + 30, "blight in the fields", false};
    if (!soc.board().ships.empty())
    {
        soc.stagedBoard().ships.front().status = "storm";
        soc.stagedBoard().ships.front().lost = true;
    }
    // Through the wire.
    const auto encoded = json::dump(market::encode(soc.board()));
    const auto back = market::decode(parsed(encoded));
    expect(json::dump(market::encode(back)) == encoded, "the Board's Phase 7 state round-trips through the wire");
    expect(back.events.size() == soc.board().events.size() && back.sliderCause.words == "the Crown's war levy" &&
               back.nudges.at("east").cause == "boom at the port" && back.harvestOverride.at("grain").cause == "blight in the fields" &&
               !back.harvestOverride.at("grain").told,
           "every part of it");
    // Through a save and a restore, an event on staying on.
    const auto before = eventsOf(w);
    const double P = soc.pressure("east", "raw_meat");
    auto saved = w.save();
    auto g = harbour();
    auto again = load(g);
    expect(again.restore(saved).ok, "restores");
    expect(eventsOf(again) == before && std::abs(again.society().pressure("east", "raw_meat") - P) < 1e-6, "the same events and prices");
    again.takeEvents();
    again.tick(.6);
    again.tick(.6);
    expect(count(again.takeEvents(), "event begins") == 0 && again.stagedEvent(on)->status == "on", "never begun twice");
    // A save from before Phase 7: none, then rolled from that day.
    {
        auto j = market::encode(soc.board());
        auto older = json::Value::object();
        for (const auto& [k, v] : j.fields())
            if (k != "events" && k != "eventsThrough" && k != "nextEvent" && k != "sliderCause" && k != "nudges" && k != "harvestShift")
                older.add(k, v);
        auto old = saved;
        old.society.board = market::decode(older);
        for (auto& s : old.society.board.ships)
            s.lost = false;
        auto h = harbour();
        auto w3 = load(h);
        expect(w3.restore(old).ok, "a Phase 6 save loads");
        expect(w3.society().board().events.empty() && w3.society().board().sliderCause.words.empty(), "with no events");
        nextMorning(w3);
        expect(!w3.society().board().events.empty(), "and rolls them from that day");
        expect(w3.society().realBalanced(), "real money balanced");
    }
    // A save that lies.
    {
        auto j = market::encode(soc.board());
        auto lies = parsed(R"([
            {"id": "e900", "market": "east", "kind": "good_catch", "day": 1, "words": "a dog day", "status": "on"},
            {"id": "e901", "market": "east", "kind": "good_catch", "day": 1, "hour": 99, "days": 40, "times": 900, "words": "a catch",
             "goods": [["shellfish", 9], ["no_such_good", 0.1], ["shellfish", -0.1]], "status": "weird"},
            {"id": "e901", "market": "east", "kind": "good_catch", "day": 1, "words": "twice"},
            {"id": "e902", "market": "nowhere", "kind": "good_catch", "day": 1, "words": "a catch"},
            {"id": "e903", "market": "east", "kind": "good_catch", "day": 999999, "words": "a catch far ahead"}])");
        auto board = json::Value::object();
        for (const auto& [k, v] : j.fields())
            if (k != "events")
                board.add(k, v);
        board.add("events", lies);
        board.add("nextEvent", 3);
        const auto b = market::decode(board);
        const auto e901 = std::find_if(b.events.begin(), b.events.end(), [](const auto& e) { return e.id == "e901"; });
        expect(std::none_of(b.events.begin(), b.events.end(), [](const auto& e) { return e.id == "e900"; }), "the forbidden word never reaches a board");
        // (Its `times`, as an older save carries them, read past: the event loads.)
        expect(e901 != b.events.end() && e901->status == "due" && e901->hour <= 23.99 && e901->days == 7 &&
                   e901->goods.size() == 1 && e901->goods[0].second == market::EventTargetMost && e901->words == "a catch",
               "a lying event put right");
        {
            const auto again = market::encode(b);
            bool points = false;
            for (const auto& row : again.array("events"))
                points |= row.find("times") != nullptr;
            expect(!again.array("events").empty() && !points, "and no event is saved with Times points again");
        }
        expect(b.nextEvent == 904, "the next id above every one: " + std::to_string(b.nextEvent));
        auto lying = saved;
        lying.society.board = b;
        auto h = harbour();
        auto w4 = load(h);
        expect(w4.restore(lying).ok, "a lying save loads");
        for (const auto& e : w4.society().board().events)
            expect(e.market != "nowhere" && e.day < 999999, "a market gone or a day past the rolled weeks dropped: " + e.id);
    }
}

// Through a game (8.4, 8.5): an event's notice pinned ahead (its own sign, the market's scent, never one of the board's 30
// pins), down when it is over; the slider's cause and the DM's nudge on every market's board; the crier calling it at the
// square where a player can hear.
void noticesAndTheCrier()
{
    rules = "rule \"eventRate\" 0\n";
    auto f = harbour(true, true, resident("ik", "Ida Kell", "merchant", "keeping the inn at The Lantern", 0, 5.5, 1, 3.5));
    rules.clear();
    const auto root = onDisk(f, "events-notices");
    game::Game g(optionsOn(root));
    std::string problem;
    expect(g.start(problem), "a game starts on the harbour: " + problem);
    auto& w = g.world();
    w.tick(.6);
    w.roads().camps.clear();
    Client ash;
    enterAt(g, ash, 84, "ash", "east");
    expect(w.setTimeOfDay(9).ok, "morning");
    const auto id = w.addStagedEvent("east", "house_wedding", today(w) + 2, 10, problem);
    expect(!id.empty(), "a House wedding two days on: " + problem);
    for (int t = 0; t < 30; ++t)
        g.tick(.1);
    const auto* n = g.documents().find("event-" + id);
    const auto* e = w.stagedEvent(id);
    expect(n && e && n->text == e->notice && n->sign == "the stewards of the Houses" && e->announced, "pinned ahead: " + (n ? n->text : std::string("none")));
    expect(n->text.find(calendar::weekdayName(calendar::weekdayOf(double(e->day) + .5))) != std::string::npos, "with its weekday: " + n->text);
    board(g, ash, {{"verb", "read"}});
    bool listed = false;
    for (const auto& one : ash.last("board")->array("notices"))
        if (one.string("text") == e->notice)
            listed = one.string("scent").find("the market's many paws") != std::string::npos;
    expect(listed, "on east's board, with the market's scent");
    for (int k = 0; k < 3; ++k)
        expect(board(g, ash, {{"verb", "post"}, {"kind", "other"}, {"text", "Mine " + std::to_string(k) + "."}}).find("You pin your notice") != std::string::npos,
               "a player's notices still pin beside it");
    // The slider's cause and a nudge on the boards.
    w.society().setProsperity(-30, false, "rot in the granaries");
    expect(w.nudgeTimes("east", 20, 2, "a fair wind for trade"), "a nudge");
    for (int t = 0; t < 30; ++t)
        g.tick(.1);
    for (const char* m : {"east", "west"})
    {
        const auto* c = g.documents().find(std::string("times-") + m);
        expect(c && c->text == "Hard times ahead: rot in the granaries.", std::string("the cause on ") + m + "'s board: " + (c ? c->text : std::string("none")));
    }
    const auto* nudge = g.documents().find("nudge-east");
    expect(nudge && nudge->text == "A fair wind for trade.", "the nudge's: " + (nudge ? nudge->text : std::string("none")));
    // The day: it begins, the crier calls it at the square where ash stands.
    const auto* square = w.marketSpot("east");
    expect(square != nullptr, "east has a square");
    w.advanceCalendar(2);
    expect(w.setTimeOfDay(10.2).ok, "its hour");
    auto* me = w.entity(ash.entityId);
    me->cellId = square->cell;
    me->position = {square->x + 1.5, square->y + 1.5};
    ash.events.clear();
    bool heard = false;
    for (int t = 0; t < 900 && !heard; ++t)
    {
        g.tick(1);
        for (const auto& ev : ash.events)
            heard = heard || json::dump(ev).find("A House wedding today, and the stewards buying ") != std::string::npos;
    }
    expect(w.stagedEvent(id)->status == "on", "it began");
    expect(heard, "the crier called it at the square");
    // Over: its notice down.
    w.advanceCalendar(3.5);
    for (int t = 0; t < 5; ++t)
        g.tick(.2);
    expect(w.stagedEvent(id)->status == "over" && !g.documents().find("event-" + id), "over, its notice down");
    expect(!g.documents().find("nudge-east"), "the lapsed nudge's notice down");
    g.disconnect(&ash);
    std::filesystem::remove_all(root);
}

// ---------------------------------------------------------------------------------------------------- Phase 7 repaired

// To a day of the calendar (its morning), as a restart there would find it.
void toDay(World& w, std::int64_t day)
{
    auto saved = w.save();
    saved.calendarDays = double(day) + .2;
    expect(w.restore(saved).ok, "to day " + std::to_string(day));
    w.tick(.6);
}

// The first day on or after `from` in a season (0 spring .. 3 winter).
std::int64_t dayIn(std::int64_t from, int season)
{
    for (auto day = from; day < from + 400; ++day)
        if (market::seasonBit(double(day) + .5) == 1 << season)
            return day + 3;
    expect(false, "a day of that season");
    return from;
}

// The Phase 7 check's findings, each fixed: the season (events and causes), the needs the Dungeon Master's events are held
// to, the ships' troubles in their own words, an event over unseen taking its notice down, the rule at 0 calling off the
// seeded events still to come, a market at the Board's bound, ids a damaged save can't make twice, a nudge's expiry held
// to the calendar, and the event words read in a sentence.
void phase7Repaired()
{
    rules = "rule \"eventRate\" 0\n";
    auto f = strip();
    auto w = load(f);
    rules.clear();
    w.tick(.6);
    w.roads().camps.clear();
    auto& soc = w.society();
    const auto& d = market::data();
    std::string problem;
    // Every event's and cause's words read as a noun phrase: after "since", after a colon, after "talk of" (no verb
    // standing alone: "the drovers in", "a brewing too many", "the buying ahead").
    for (const auto& k : d.events)
        for (const char* bad : {"the drovers in ", "a brewing too many", "buying ahead", "a lame ox on the cart", "mill wheel stopped", "salt pans flooded"})
            expect(k.words.find(bad) == std::string::npos, "a noun phrase: " + k.words);
    expect(d.eventKind("house_wedding")->cry.find("wine for the stewards") == std::string::npos &&
               d.eventKind("house_wedding")->cry.find("{goods}") != std::string::npos && d.eventKind("house_wedding")->cry.find("{day}") == std::string::npos,
           "the wedding's cry: its own goods, today: " + d.eventKind("house_wedding")->cry);
    // The seasons: no good catch or orchard blight in winter, by the scheduler or the DM; the rest go on.
    expect(!(d.eventKind("good_catch")->seasons & 8) && !(d.eventKind("orchard_blight")->seasons & 8) && d.eventKind("mill_stopped")->seasons == 15,
           "the list's seasons");
    toDay(w, dayIn(today(w), 3));
    expect(market::seasonBit(w.calendarDays()) == 8, "winter");
    expect(w.addStagedEvent("east", "good_catch", today(w) + 1, 9, problem).empty() && problem.find("season") != std::string::npos,
           "no good catch in winter: " + problem);
    expect(w.addStagedEvent("east", "orchard_blight", today(w) + 1, 9, problem).empty(), "nor blight in the orchards");
    expect(!w.addStagedEvent("east", "mill_stopped", today(w) + 1, 9, problem).empty(), "a mill wheel breaks in any season: " + problem);
    setRule(w, "eventRate", 300);
    std::set<std::string> kinds;
    for (int day = 0; day < 28 && market::seasonBit(w.calendarDays() + 7) == 8; ++day)
    {
        nextMorning(w);
        for (const auto& e : soc.board().events)
            if (market::seasonBit(double(e.day) + .5) == 8)
                kinds.insert(e.kind);
    }
    expect(!kinds.empty() && !kinds.count("good_catch") && !kinds.count("orchard_blight"), "the winter's weeks roll neither: " + std::to_string(kinds.size()));
    setRule(w, "eventRate", 0);
    // The causes: "the hard winter" never in summer, "the rich harvest" never in spring; the hard winter in winter.
    const auto causesIn = [&](int season) {
        toDay(w, dayIn(today(w), season));
        std::set<std::string> seen;
        for (int i = 0; i < 40; ++i)
        {
            soc.setProsperity(i % 2 ? 60 : -60);
            seen.insert(soc.board().sliderCause.words);
            if (i % 8 == 7)
                nextMorning(w);
        }
        soc.setProsperity(0, true);
        return seen;
    };
    const auto summer = causesIn(1);
    expect(!summer.count("the hard winter") && summer.count("the rich harvest") && summer.size() >= 4, "summer's causes: no hard winter");
    const auto winter = causesIn(3);
    expect(winter.count("the hard winter") && !winter.count("the rich harvest"), "winter's: the hard winter, no rich harvest");
    // The needs, for the DM as for the scheduler: a House wedding at the realm's largest market only; the festival's
    // buying only the day before a festival.
    const std::string city = "east", other = "west";   // (The strip's city: east, its largest market.)
    expect(w.addStagedEvent(other, "house_wedding", today(w) + 1, 10, problem).empty() && !problem.empty(), "no House wedding but in the city: " + problem);
    std::int64_t festival = -1;
    for (auto day = today(w) + 1; day < today(w) + 14 && festival < 0; ++day)
        if (calendar::festivalDay(double(day) + .5))
            festival = day;
    for (auto day = today(w); day <= (today(w) / 7 + 2) * 7 - 1; ++day)
        if (day + 1 != festival)
            expect(w.addStagedEvent(city, "festival_buying", day, 9, problem).empty(), "no festival's buying the day before none: " + std::to_string(day));
    // The ships' troubles in their own words, never the wagons'.
    {
        auto& b = soc.stagedBoard();
        b.troubles = {{w.calendarDays(), "east", "storm"}};
        soc.setTimesShift({{"east", -5}});
        expect(soc.timesCause("east") == "the storms keeping the ships out", "a storm's: " + soc.timesCause("east"));
        b.troubles = {{w.calendarDays(), "east", "lost"}};
        expect(soc.timesCause("east") == "the ship lost at sea", "a lost ship's: " + soc.timesCause("east"));
        const auto back = market::decode(parsed(json::dump(market::encode(soc.board()))));
        expect(back.troubles.size() == 1 && back.troubles[0].kind == "lost", "kept through the wire");
        b.troubles.clear();
        soc.setTimesShift({});
    }
    // An event posted ahead whose days pass in a jump of the calendar: over, and its notice taken down (a record).
    {
        const auto id = w.addStagedEvent(city, "house_wedding", today(w) + 2, 10, problem);
        expect(!id.empty() && w.stagedEvent(id)->announced, "a wedding posted ahead: " + problem);
        w.takeEvents();
        expect(w.advanceCalendar(6).ok, "six days on at once");
        w.tick(.6);
        expect(w.stagedEvent(id) == nullptr || w.stagedEvent(id)->status == "over", "over unseen");
        expect(count(w.takeEvents(), "event over", id) == 1, "and its notice taken down");
    }
    // The rule at 0: the seeded events still to come called off (their notices down), the DM's kept.
    {
        setRule(w, "eventRate", 300);
        nextMorning(w);
        const auto dm = w.addStagedEvent("west", "mill_stopped", today(w) + 3, 9, problem);
        const auto seeded = std::count_if(soc.board().events.begin(), soc.board().events.end(),
                                          [](const auto& e) { return e.by == "seeded" && e.status == "due"; });
        expect(!dm.empty() && seeded > 0, "seeded events due, and the DM's");
        setRule(w, "eventRate", 0);
        w.takeEvents();
        w.tick(.6);
        for (const auto& e : soc.board().events)
            expect(e.status != "due" || e.by == "dm", "the seeded ones called off: " + e.id);
        expect(w.stagedEvent(dm)->status == "due" && count(w.takeEvents(), "event cancelled") >= int(seeded), "the DM's kept");
        w.cancelStagedEvent(dm);
    }
    // A market at the Board's bound: the oldest over make room; with none over, the week is not marked rolled, and it is
    // rolled the morning after room is made.
    {
        setRule(w, "eventRate", 300);
        nextMorning(w);
        auto& b = soc.stagedBoard();
        b.events.erase(std::remove_if(b.events.begin(), b.events.end(), [](const auto& e) { return e.market == "east"; }), b.events.end());
        b.eventsThrough.erase("east");
        const auto fake = [&](const std::string& status, std::int64_t day) {
            market::StagedEvent e;
            e.id = "e" + std::to_string(b.nextEvent++);
            e.market = "east";
            e.kind = "mill_stopped";
            e.day = day;
            e.days = 1;
            e.words = "the broken mill wheel";
            e.status = status;
            e.by = "dm";
            return e;
        };
        for (std::size_t i = 0; i < market::EventsAMarket; ++i)
            b.events.push_back(fake("over", today(w)));
        nextMorning(w);
        const auto rolled = std::count_if(b.events.begin(), b.events.end(), [&](const auto& e) { return e.market == "east" && e.by == "seeded"; });
        expect(rolled > 0 && eventsAt(w, "east").size() <= market::EventsAMarket, "the oldest over made room: " + std::to_string(rolled));
        b.events.erase(std::remove_if(b.events.begin(), b.events.end(), [](const auto& e) { return e.market == "east"; }), b.events.end());
        b.eventsThrough.erase("east");
        for (std::size_t i = 0; i < market::EventsAMarket; ++i)
            b.events.push_back(fake("due", today(w) + 9));
        nextMorning(w);
        const auto week = today(w) / 7;
        expect(!b.eventsThrough.count("east") || b.eventsThrough.at("east") < week, "a week cut short is not marked rolled");
        for (auto& e : b.events)
            if (e.market == "east" && e.by == "dm")
                e.status = "cancelled";
        nextMorning(w);
        expect(b.eventsThrough.count("east") && b.eventsThrough.at("east") == today(w) / 7 + 1 &&
                   std::any_of(b.events.begin(), b.events.end(), [&](const auto& e) { return e.market == "east" && e.by == "seeded" && e.status == "due"; }),
               "rolled once there is room");
        setRule(w, "eventRate", 0);
    }
    // Ids a damaged save can't make twice: one the game never gives is dropped, and the next id is above every one.
    {
        auto j = market::encode(soc.board());
        auto board = json::Value::object();
        for (const auto& [k, v] : j.fields())
            if (k != "events" && k != "nextEvent" && k != "nudges")
                board.add(k, v);
        board.add("events", parsed(R"([
            {"id": "e1000000000000000", "market": "east", "kind": "good_catch", "day": 1, "words": "a good catch"},
            {"id": "e12345678901234567", "market": "east", "kind": "good_catch", "day": 1, "words": "a good catch"},
            {"id": "e05", "market": "east", "kind": "good_catch", "day": 1, "words": "a good catch"},
            {"id": "x7", "market": "east", "kind": "good_catch", "day": 1, "words": "a good catch"},
            {"id": "e8", "market": "east", "kind": "good_catch", "day": 1, "words": "a good catch",
             "goods": [["fish", -0.1], ["shellfish", -0.1], ["salt_fish", -0.1], ["smoked_fish", -0.1], ["ale", 0.1], ["mead", 0.1]]}])"));
        board.add("nextEvent", 1e15);
        auto nudges = json::Value::object();
        auto n = json::Value::object();
        n.add("points", 100);
        n.add("until", 1e12);
        nudges.add("east", n);
        board.add("nudges", nudges);
        const auto b = market::decode(board);
        expect(b.events.size() == 2 && b.nextEvent == 1000000000000001LL, "only the game's ids kept, the next above them: " + std::to_string(b.nextEvent));
        for (const auto& e : b.events)
            expect(e.goods.size() <= market::EventGoodsMost, "four goods at most, as made: " + e.id);
        // A nudge's "until" of 10^12 held to a year from now at the restore: it lapses.
        auto saved = w.save();
        saved.society.board = b;
        auto again = strip();
        auto w2 = load(again);
        expect(w2.restore(saved).ok, "a damaged save loads");
        const auto& nudge = w2.society().board().nudges;
        expect(nudge.count("east") && nudge.at("east").until <= w2.calendarDays() + 366.5, "its nudge held to the calendar");
        expect(w2.advanceCalendar(370).ok, "a year on");
        w2.tick(.6);
        expect(!w2.society().board().nudges.count("east") && w2.society().timesScore("east") < 100, "and the nudge lapsed");
    }
    expect(soc.realBalanced(), "real money balanced");
}

// The repair on the boards (through a game): a harvest override's notice down when it lapses; a rich cargo's notice says
// she is in once she is; a festival's crier never kept from the festival by a staged cry.
void theRepairOnTheBoards()
{
    named = {{3, "fs_grain_long_granary"}};
    rules = "rule \"eventRate\" 0\n";
    auto probe = strip("EE.G...WW");
    auto p = load(probe);
    std::string label;
    for (const auto& farm : p.society().farms())
        if (farm.def->id == "grain_farm_1")
            label = p.society().farmLabel(farm, 0);
    auto f = strip("EE.G...WW", resident("fk", "Bran Long", "civilian", label, 0, 9.5, 3, 9.5) +
                                    resident("ik", "Ida Kell", "merchant", "keeping the inn at The Lantern", 0, 5.5, 1, 3.5));
    rules.clear();
    named.clear();
    const auto root = onDisk(f, "events-repair");
    {
        game::Game g(optionsOn(root));
        std::string problem;
        expect(g.start(problem), "a game starts: " + problem);
        auto& w = g.world();
        w.tick(.6);
        w.roads().camps.clear();
        const auto end = std::find_if(w.cartEnds().begin(), w.cartEnds().end(), [](const auto& e) { return e.farmId == "grain_farm_1"; });
        expect(end != w.cartEnds().end(), "the farm's cart end");
        expect(w.overrideHarvest(end->area, 2, 1.5, ""), "a good harvest called, a day and a half");
        for (int t = 0; t < 20; ++t)
            g.tick(.1);
        const auto noticeId = "harvest-" + end->area + "-east";
        const auto* n = g.documents().find(noticeId);
        expect(n && n->text.rfind("A good season for ", 0) == 0 && n->expires <= w.society().board().harvestOverride.at(end->area).until + 1e-9,
               "its notice, down when it lapses: " + (n ? n->text : std::string("none")));
        expect(w.advanceCalendar(2).ok, "two days on");
        for (int t = 0; t < 40; ++t)
            g.tick(.2);
        expect(!g.documents().find(noticeId), "the lapsed harvest's notice down");
        // A festival's crier comes first: a staged cry holding the crier at the square gives way at 20:00.
        Client ash;
        enterAt(g, ash, 85, "ash", "east");
        std::int64_t festival = -1;
        for (auto day = std::int64_t(std::floor(w.calendarDays())) + 1; festival < 0 && day < 500; ++day)
            if (calendar::festivalDay(double(day) + .5) && (market::seasonBit(double(day) + .5) & 7))
                festival = day;
        expect(festival > 0, "a festival in spring, summer or autumn");
        expect(w.advanceCalendar(double(festival) - std::floor(w.calendarDays())).ok && w.setTimeOfDay(19.8).ok, "the festival's evening");
        const auto* square = w.marketSpot("east");
        expect(square != nullptr, "east has a square");
        auto* me = w.entity(ash.entityId);
        me->cellId = square->cell;
        me->position = {square->x + 6.5, square->y + 6.5};
        const auto id = w.addStagedEvent("east", "good_catch", std::int64_t(std::floor(w.calendarDays())), 19.85, problem);
        expect(!id.empty(), "a good catch at dusk: " + problem);
        for (int t = 0; t < 120 && w.stagedEvent(id)->status != "on"; ++t)
            g.tick(1);
        for (int t = 0; t < 5; ++t)
            g.tick(1);
        expect(w.stagedEvent(id)->status == "on", "its cry waiting for the crier");
        expect(w.setTimeOfDay(20.05).ok, "the festival crier's hour");
        ash.events.clear();
        for (int t = 0; t < 600; ++t)
            g.tick(1);
        bool festive = false;
        for (const auto& ev : ash.events)
            for (const char* opening : {"the town remembers its own", "and the deeds of the season", "would have you know who did it proud"})
                festive = festive || json::dump(ev).find(opening) != std::string::npos;
        expect(festive, "the festival's crier called at the square");
        g.disconnect(&ash);
    }
    std::filesystem::remove_all(root);
}

// The two repairs together (the merge of r2-staged-rev and r2-staged-rtalk): the events' new words (noun phrases), the
// ships' troubles' words and the rich cargo's goods, through the real path of talk (the director's picks for a listening
// player, Game::talkTopicQuery, the library's converse) and the Mind's Means. RATW_TALK_SAMPLES=1 prints what is said.
void theRepairsTogether()
{
    rules = "rule \"eventRate\" 0\n";
    auto f = harbour(true, true, resident("ef", "Fen Gill", "merchant", "fishmonger at the Fish Hall", 0, 4.5, 1, 6.5) +
                                     resident("ev", "Vera Lees", "merchant", "vintner at the Wine Vault", 0, 5.5, 1, 7.5));
    rules.clear();
    const bool print = std::getenv("RATW_TALK_SAMPLES") != nullptr;
    const auto root = onDisk(f, "events-together");
    {
        auto options = optionsOn(root);
        options.voiceData = std::string(RATW_SOURCE_DIR) + "/Data/Voice";   // (The written lines: Data/Talk beside it.)
        game::Game g(options);
        std::string problem;
        expect(g.start(problem), "a game starts: " + problem);
        expect(g.talkLibrarySize() > 1000, "the talk library loaded: " + std::to_string(g.talkLibrarySize()));
        auto& w = g.world();
        auto& soc = w.society();
        w.tick(.6);
        w.roads().camps.clear();
        Client ash;
        enterAt(g, ash, 86, "ash", "east");
        const auto none = [](const std::string&) { return false; };
        const std::vector<std::pair<std::string, std::string>> pairs{{"e1", "e2"}, {"em", "e3"}, {"d1", "e4"}, {"ev", "ef"}};
        const auto ready = [&] {
            double x = 3.5;
            for (const auto& [a, b] : pairs)
                for (const auto* who : {&a, &b})
                    if (auto* e = w.entity(*who))
                    {
                        e->cellId = id(1);
                        e->position = {x += 1, 8.5};
                        e->path.clear();
                        e->velocity = {0, 0};
                        e->speakingUntil = 0;
                    }
            if (auto* me = w.entity(ash.entityId))
                me->cellId = id(1), me->position = {6.0, 9.5};
        };
        // Every line the talk of `concept` says (topics that fit), from the pairs, over the hours (the hourly draw).
        const auto lowered = [](std::string t) {
            for (auto& c : t)
                c = char(std::tolower(static_cast<unsigned char>(c)));
            return t;
        };
        std::set<std::string> everything;
        talk::Query timesQuery;                     // (The last Times topic the director gave: for the troubles below.)
        const auto heard = [&](const std::string& concept, const std::function<bool(const talk::Query&)>& fits, int hours = 96) {
            std::set<std::string> out;
            for (int h = 0; h < hours; ++h)
            {
                ready();
                for (const auto& pick : w.ambientPicks({ash.entityId}))
                    expect(pick.hearer == ash.entityId, "the director picks for the listening player");
                for (const auto& [a, b] : pairs)
                {
                    const auto q = g.talkTopicQuery(a, b, ash.entityId);
                    if (q.concept == "talk.times")
                        timesQuery = q;
                    if (q.concept != concept || (fits && !fits(q)))
                        continue;
                    for (std::uint64_t seed = 0; seed < 16; ++seed)
                        for (const auto& [who, line] : g.talkLibrary().converse(q, seed, none, {}).lines)
                        {
                            out.insert(line);
                            everything.insert(line);
                            const auto low = lowered(line);
                            expect(line.find('#') == std::string::npos && line.find('{') == std::string::npos, "every blank filled: " + line);
                            expect(low.find("though the ") == std::string::npos || low.find("though the wagon") != std::string::npos,
                                   "never \"though\" before an event's noun phrase: " + line);
                            expect(low.find("wagons late on the road") == std::string::npos, "never a wagon's words for a ship: " + line);
                        }
                }
                w.advanceCalendar(1.0 / 24);
            }
            if (print)
                for (const auto& line : out)
                    std::cout << "    [" << concept << "] " << line << "\n";
            return out;
        };
        const auto said = [](const std::set<std::string>& lines, const std::string& words) {
            return std::any_of(lines.begin(), lines.end(), [&](const std::string& l) { return l.find(words) != std::string::npos; });
        };
        const auto capital = [](std::string t) {
            if (!t.empty())
                t[0] = char(std::toupper(static_cast<unsigned char>(t[0])));
            return t;
        };
        const auto find = [&](const std::string& eventId) -> market::StagedEvent& {
            for (auto& e : soc.stagedBoard().events)
                if (e.id == eventId)
                    return e;
            throw std::runtime_error("no event " + eventId);
        };
        // (1) Each kind of the catalogue east can show, on: its talk; the Times' cause never the event's (decided
        // 2026-11-12: the market's own harvest takes the band past ordinary, and that is the cause, in talk and in the
        // Means, whatever is on at the market); then soon, then over.
        int kinds = 0, timesHeard = 0, causeSaid = 0;
        for (const auto& kind : market::data().events)
        {
            if (!kind.ship.empty())
                continue;
            const auto eventId = w.addStagedEvent("east", kind.id, today(w), std::min(23.5, (w.calendarDays() - std::floor(w.calendarDays())) * 24 + .05), problem);
            if (eventId.empty())
            {
                if (print)
                    std::cout << "  (" << kind.id << " not here: " << problem << ")\n";
                continue;
            }
            ++kinds;
            for (int t = 0; t < 400 && w.stagedEvent(eventId)->status != "on"; ++t)
                w.tick(.6);
            expect(w.stagedEvent(eventId)->status == "on", kind.id + " begins");
            find(eventId).days = 9;
            const auto words = w.stagedEvent(eventId)->words;
            if (print)
                std::cout << "  " << kind.id << ": \"" << words << "\"\n";
            const int way = kinds % 2 ? -30 : 30;
            const std::string harvest = way < 0 ? "a poor harvest" : "a good harvest";
            expect(soc.timesScore("east") == 0, kind.id + " on: the Times' score unmoved: " + std::to_string(soc.timesScore("east")));
            soc.setHarvestShift({{"east", way}});
            soc.setTimesShift({{"east", way}});
            expect(soc.timesScore("east") == way && soc.marketTimes("east") == (way < 0 ? -1 : 1) && soc.timesCause("east") == harvest,
                   kind.id + ": the Times' cause is the harvest, never the event: " + soc.timesCause("east"));
            {
                std::map<std::string, std::string> facts;
                w.eventFacts("east", facts);
                expect(facts["world.times_cause"] == harvest && facts["world.event_words"] == words,
                       kind.id + ": talk's facts keep them apart: " + facts["world.times_cause"] + "; " + facts["world.event_words"]);
            }
            const auto means = soc.meansNote("e1");
            expect(means.find(", since " + harvest + ".") != std::string::npos && means.find("since " + words) == std::string::npos, "the Means: " + means);
            if (print)
                std::cout << "    [means] " << means << "\n";
            const auto evNow = heard("talk.event", {});
            expect(!evNow.empty(), kind.id + " is talked of");
            const auto times = heard("talk.times", {});
            timesHeard += !times.empty();           // (The hourly draw: most kinds' Times are heard in four days.)
            causeSaid += said(times, harvest) || said(times, capital(harvest));
            expect(!said(times, words) && !said(times, capital(words)), kind.id + ": the Times are never put down to the event");
            // (Its goods' prices, pressed its way: the cause said as the event.)
            heard("talk.prices", [&](const talk::Query& q) { return q.facts.count("topic.cause") && (q.facts.at("topic.cause") == "event" || q.facts.at("topic.cause") == "still"); }, 48);
            // A good it presses, pushed the other way: "still", in a noun phrase's words.
            for (const auto& [good, target] : w.stagedEvent(eventId)->goods)
            {
                soc.setPressure("east", good, target > 0 ? -.2 : .2, "event");
                for (const auto* k : {"em", "es", "ek", "eg", "et", "ef", "ev"})
                    for (const auto& ware : soc.wares(k))
                        if (items::baseOf(ware) == good)
                        {
                            const auto row = soc.listing(ash.entityId, k, ware, g.accountOf(ash.entityId));
                            if (row.trend != 0)
                                expect(row.cause.find("though " + words) == std::string::npos &&
                                           (row.cause == words || row.cause == (row.trend > 0 ? "dear still, despite " : "cheap still, despite ") + words ||
                                            row.cause.find(words) == std::string::npos),
                                       "the list's still: " + row.cause);
                            if (print && row.trend != 0)
                                std::cout << "    [list] " << ware << ": " << row.cause << "\n";
                        }
                heard("talk.prices", [&](const talk::Query& q) { return q.facts.count("topic.cause") && q.facts.at("topic.cause") == "still"; }, 48);
                soc.setPressure("east", good, target / 2, "event");
                break;
            }
            soc.setTimesShift({});
            soc.setHarvestShift({});
            // Soon (posted, due tomorrow: a kind posted ahead), then over.
            if (kind.ahead > 0)
            {
                auto& e = find(eventId);
                e.status = "due", e.announced = true, e.day = today(w) + 1, e.hour = 9;
                heard("talk.event", {}, 24);
            }
            auto& o = find(eventId);
            o.status = "over", o.day = today(w) - 1, o.days = 1;
            heard("talk.event", {}, 30);
            expect(w.cancelStagedEvent(eventId) || find(eventId).status != "on", kind.id + " done");
            for (auto& x : soc.stagedBoard().events)
                if (x.id == eventId)
                    x.status = "cancelled", x.day = today(w) - 3;
        }
        expect(kinds >= 6 && timesHeard >= kinds / 2 && causeSaid >= kinds / 2,
               "most kinds talked of: " + std::to_string(kinds) + ", the Times beside them " + std::to_string(timesHeard) +
                   ", by the harvest's words " + std::to_string(causeSaid));
        // (2) The ships: a rich cargo's ship in, a lost one, a stormbound one.
        std::vector<market::Ship> due;
        for (int day = 0; day < 30 && due.size() < 2; ++day)
        {
            nextMorning(w);
            due.clear();
            for (const auto& s : soc.board().ships)
                if (s.status == "due" && s.day > today(w))
                    due.push_back(s);
            std::sort(due.begin(), due.end(), [](const auto& a, const auto& b) { return a.day < b.day; });
            if (due.size() >= 2 && due[0].day / 7 != due[1].day / 7)
                due.resize(1);
        }
        expect(due.size() >= 2, "two ships due in a week");
        const auto rich = w.addStagedEvent("east", "rich_cargo", due[0].day, 0, problem);
        expect(!rich.empty(), "a rich cargo: " + problem);
        const auto lost = w.addStagedEvent("east", "lost_ship", due[0].day, 0, problem);
        expect(!lost.empty(), "a lost ship: " + problem);
        while (today(w) < due[0].day)
            nextMorning(w);
        dock(w, due[0]);
        w.tick(.6);
        expect(w.stagedEvent(rich)->status == "on", "the rich cargo is in");
        find(rich).days = 9;
        std::vector<std::string> richGoods;
        for (const auto& [gd, t] : w.stagedEvent(rich)->goods)
            richGoods.push_back(gd);
        if (print)
            std::cout << "  rich_cargo: \"" << w.stagedEvent(rich)->words << "\" " << w.stagedEvent(rich)->notice << "\n";
        // (A few hours only: the lost ship's day is still to come.)
        const auto shipsIn = heard("talk.ships", [](const talk::Query& q) { return q.facts.count("topic.kind") && q.facts.at("topic.kind") == "in"; }, 6);
        const auto cargoNow = heard("talk.event", {}, 6);
        expect(!shipsIn.empty() && !cargoNow.empty(), "the ship and her cargo talked of");
        // (Her rich cargo's goods in the ship's talk, "is in" with them; never silk where east sells none.)
        const auto firstRich = World::cargoWords(richGoods.front());
        expect(said(shipsIn, "in with " + firstRich) || said(shipsIn, "swinging " + firstRich), "the ship's talk names her rich cargo: " + firstRich);
        const bool silk = std::find(richGoods.begin(), richGoods.end(), "silk") != richGoods.end();
        for (const auto* lines : {&shipsIn, &cargoNow})
            for (const auto& line : *lines)
                expect(silk || lowered(line).find("silk") == std::string::npos, "no silk in a cargo without it: " + line);
        // The board's notice once she has sailed, her cargo's days still running (the hours above took the day past her
        // sailing): "has sailed", never "is in" of a ship gone (the slice 2 verify's leftover).
        {
            for (int t = 0; t < 50 && shipById(w, due[0].id)->status != "sailed"; ++t)
                g.tick(.2);
            for (int t = 0; t < 5; ++t)
                g.tick(.2);
            const auto* pinned = g.documents().find("event-" + rich);
            expect(shipById(w, due[0].id)->status == "sailed" && w.stagedEvent(rich)->status == "on", "she has sailed, the event still on");
            expect(pinned && pinned->text == w.stagedEvent(rich)->notice && pinned->text.find(" has sailed, ") != std::string::npos &&
                       pinned->text.find(" is in") == std::string::npos,
                   "the board says she has sailed: " + (pinned ? pinned->text : std::string("no notice")));
            if (print)
                std::cout << "  rich_cargo, sailed: " << (pinned ? pinned->text : std::string()) << "\n";
        }
        while (today(w) < due[1].day)
            nextMorning(w);
        w.tick(.6);
        expect(w.stagedEvent(lost) && w.stagedEvent(lost)->status == "on", "the lost ship's story on");
        find(lost).days = 9;
        if (print)
            std::cout << "  lost_ship: \"" << w.stagedEvent(lost)->words << "\"\n";
        heard("talk.ships", [](const talk::Query& q) { return q.facts.count("topic.kind") && q.facts.at("topic.kind") == "lost"; });
        heard("talk.event", {});
        soc.setTimesShift({{"east", -18}});
        if (print)
            std::cout << "    [means] " << soc.meansNote("e1") << "\n";
        heard("talk.times", {});
        // Only the troubles of the sea (the events called off): their own words in the Times, through the facts talk is
        // given (World::eventFacts) into the last Times topic the director gave, and the Means. (While a ship's loss or an
        // event is the port's news it outscores the Times: the draw is Phase 10's weight.)
        find(lost).status = "cancelled";
        find(rich).status = "cancelled";
        expect(timesQuery.concept == "talk.times", "the director gave a Times topic");
        const auto timesSaid = [&](const std::string& words) {
            soc.setTimesShift({{"east", -25}});     // (The morning's shift, troubles in it, as the world sets it: lean.)
            std::map<std::string, std::string> facts;
            w.eventFacts("east", facts);
            expect(facts["world.times"] == "lean" && facts["world.times_cause"] == words, "talk's Times: " + facts["world.times"] + ", " + facts["world.times_cause"]);
            auto q = timesQuery;
            q.facts["world.times"] = facts["world.times"];
            q.facts["world.times_cause"] = facts["world.times_cause"];
            std::set<std::string> lines;
            for (std::uint64_t seed = 0; seed < 32; ++seed)
                for (const auto& [who, line] : g.talkLibrary().converse(q, seed, none, {}).lines)
                    lines.insert(line);
            if (print)
                for (const auto& line : lines)
                    std::cout << "    [talk.times] " << line << "\n";
            expect(said(lines, words) || said(lines, capital(words)), "and talk says it: " + words);
            const auto means = soc.meansNote("e1");
            expect(means.find("since " + words + ".") != std::string::npos, "the Means: " + means);
            if (print)
                std::cout << "    [means] " << means << "\n";
        };
        timesSaid("the ship lost at sea");
        soc.stagedBoard().troubles.clear();
        soc.stagedBoard().troubles.push_back({w.calendarDays(), "east", "storm"});
        timesSaid("the storms keeping the ships out");
        soc.setTimesShift({});
        g.disconnect(&ash);
    }
    std::filesystem::remove_all(root);
}

// ------------------------------------------------------------------------------------------------- Through a crash

// Each case: the checkpoint, then a step a world tick takes (an event begins, one ends, a morning rolls the week's, a ship
// is lost, the slider's cause is told), then (a window run) a snapshot that doesn't land and a purchase, then a player's
// trade journalled, then the game gone without saving. After the restart the events are as at the crash, the step done
// once and never again (file mode here; the same through Postgres: Tests/events_pg.py, --write-crashes, --check-crashes).
struct EventCrash
{
    const char* name;
    std::function<json::Value(game::Game&)> before;
    std::function<void(game::Game&, const json::Value&)> after;
};

// The events as they stand, in words a file keeps: the Board's events, cause and nudges, each event good's price at its
// market, east's Times score, the boards' staged notices.
json::Value eventsAsItStands(game::Game& g)
{
    const auto& w = g.world();
    const auto& soc = w.society();
    const auto j = market::encode(soc.board());
    auto out = json::Value::object();
    out.add("events", json::dump(j["events"]) + json::dump(j["eventsThrough"]) + json::dump(j["nextEvent"]));
    out.add("cause", json::dump(j["sliderCause"]) + json::dump(j["nudges"]));
    out.add("ships", shipsOf(w));
    std::ostringstream prices;
    prices.precision(6);
    prices << std::fixed;
    std::set<std::pair<std::string, std::string>> goods;
    for (const auto& e : soc.board().events)
        for (const auto& [g2, t] : e.goods)
            goods.insert({e.market, g2});
    for (const auto& [m, g2] : goods)
        prices << m << ':' << g2 << ' ' << soc.pressure(m, g2) << ' ';
    out.add("prices", prices.str());
    out.add("times", soc.timesScore("east"));
    std::string notices;
    for (const auto& [id, d] : g.documents().all())
        if (d.kind == "notice" && (id.rfind("event-", 0) == 0 || id.rfind("times-", 0) == 0 || id.rfind("nudge-", 0) == 0))
            notices += id + "=" + d.text + "\n";
    out.add("notices", notices);
    return out;
}

bool samePricesAt(const std::string& one, const std::string& other)
{
    std::istringstream a(one), b(other);
    std::string ga, gb;
    double pa = 0, pb = 0;
    while (a >> ga >> pa)
        if (!(b >> gb >> pb) || ga != gb || std::abs(pa - pb) > 1e-4)
            return false;
    return !(b >> gb);
}

void theEventsOfTheCrash(game::Game& g, const json::Value& was, const std::string& name)
{
    auto& w = g.world();
    const auto& stood = was["atCrash"];
    const auto check = [&](const std::string& when) {
        const auto now = eventsAsItStands(g);
        expect(samePricesAt(now.string("prices"), stood.string("prices")), name + ": " + when + ", the prices of the crash:\n  " + now.string("prices") +
                                                                             "\n  for\n  " + stood.string("prices"));
        for (const char* key : {"events", "cause", "ships", "notices"})
            expect(now.string(key) == stood.string(key), name + ": " + when + ", the " + key + " of the crash:\n  " + now.string(key) + "\n  for\n  " +
                                                             stood.string(key));
        expect(now.number("times") == stood.number("times"), name + ": " + when + ", east's Times as at the crash");
    };
    check("after the restart");
    w.takeEvents();
    w.tick(.6);
    w.tick(.6);
    for (int t = 0; t < 5; ++t)
        g.tick(.05);
    const auto events = w.takeEvents();
    for (const char* kind : {"event begins", "event over", "event rolled", "event posted", "times cause", "ship lost", "event cancelled"})
        expect(count(events, kind) == 0, name + ": no \"" + kind + "\" a second time: " + std::to_string(count(events, kind)));
    check("two passes on");
    std::set<std::string> ids;
    for (const auto& e : w.society().board().events)
        expect(ids.insert(e.id).second, name + ": one event of an id: " + e.id);
    expect(w.society().realBalanced(), name + ": real money is balanced");
}

std::string dueToday(World& w, const std::string& kind, double hour)
{
    std::string problem;
    const auto id = w.addStagedEvent("east", kind, today(w), hour, problem);
    expect(!id.empty(), kind + " added: " + problem);
    return id;
}

// The checkpoint before a DM's act (the crashes of doc 79's Phase 9), and the act as dm.actions would apply it.
void dmCheckpoint(game::Game& g)
{
    auto& w = g.world();
    w.tick(.6);
    w.roads().camps.clear();
    g.save();
    g.settle();
}
Result dmAct(game::Game& g, const std::string& kind, const std::string& target,
             std::initializer_list<std::pair<const char*, json::Value>> fields)
{
    auto payload = json::Value::object();
    for (const auto& [k, v] : fields)
        payload.add(k, v);
    return g.applyEconomyCommand(kind, target, payload);
}
// (The game's passes after an act, until what it posts is told: the crash comes after.)
void dmTold(game::Game& g)
{
    for (int t = 0; t < 30; ++t)
        g.tick(.1);
}

const std::vector<EventCrash>& eventCrashes()
{
    static const std::vector<EventCrash> cases{
        // An event began after the checkpoint (its goods pressed, its shelves set out, its notice pinned): on, once.
        {"event_begins",
         [](game::Game& g) {
             auto& w = g.world();
             w.tick(.6);
             w.roads().camps.clear();
             expect(w.setTimeOfDay(9).ok, "event_begins: morning");
             const auto id = dueToday(w, "good_catch", 9.5);
             g.save();
             g.settle();                            // (The checkpoint: the catch due.)
             expect(w.setTimeOfDay(9.6).ok, "event_begins: its hour");
             for (int t = 0; t < 4; ++t)
                 g.tick(.2);
             expect(w.stagedEvent(id)->status == "on" && g.documents().find("event-" + id), "event_begins: it began, its notice up");
             auto was = json::Value::object();
             was.add("event", id);
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             const auto* e = g.world().stagedEvent(was.string("event"));
             expect(e && e->status == "on", "event_begins: on after the crash");
         }},
        // An event ended after the checkpoint (its notice down): over, once.
        {"event_over",
         [](game::Game& g) {
             auto& w = g.world();
             w.tick(.6);
             w.roads().camps.clear();
             expect(w.setTimeOfDay(9).ok, "event_over: morning");
             const auto id = dueToday(w, "drovers_in", 9.1);
             expect(w.setTimeOfDay(9.2).ok, "event_over: its hour");
             for (int t = 0; t < 4; ++t)
                 g.tick(.2);
             expect(w.stagedEvent(id)->status == "on", "event_over: on");
             w.advanceCalendar(1.9);
             for (int t = 0; t < 4; ++t)
                 g.tick(.2);
             g.save();
             g.settle();                            // (The checkpoint: still on.)
             w.advanceCalendar(.2);
             for (int t = 0; t < 4; ++t)
                 g.tick(.2);
             expect(w.stagedEvent(id)->status == "over" && !g.documents().find("event-" + id), "event_over: over, its notice down");
             auto was = json::Value::object();
             was.add("event", id);
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             const auto* e = g.world().stagedEvent(was.string("event"));
             expect(e && e->status == "over" && !g.documents().find("event-" + was.string("event")), "event_over: over after the crash");
         }},
        // A morning rolled the next week's events after the checkpoint: the same events, the same ids, none twice.
        {"event_morning",
         [](game::Game& g) {
             auto& w = g.world();
             w.tick(.6);
             w.roads().camps.clear();
             std::string before, after;
             for (int day = 0; day < 30 && before == after; ++day)
             {
                 g.save();
                 g.settle();                        // (The checkpoint, the day before.)
                 before = eventsOf(w);
                 w.takeEvents();
                 nextMorning(w);
                 after = count(w.takeEvents(), "event rolled") > 0 ? eventsOf(w) : before;
             }
             expect(before != after, "event_morning: a morning rolls new events");
             for (int t = 0; t < 4; ++t)
                 g.tick(.05);
             auto was = json::Value::object();
             was.add("next", double(w.society().board().nextEvent));
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             expect(double(g.world().society().board().nextEvent) == was.number("next"), "event_morning: the next event's number as it was");
         }},
        // A ship lost after the checkpoint (the timetable's "lost at sea", its cargo dearer, the event on): once.
        {"ship_lost",
         [](game::Game& g) {
             auto& w = g.world();
             w.tick(.6);
             w.roads().camps.clear();
             std::string id, ship;
             for (int day = 0; day < 30 && id.empty(); ++day)
             {
                 for (const auto& s : w.society().board().ships)
                     if (s.status == "due" && s.day == today(w) + 1)
                     {
                         std::string problem;
                         id = w.addStagedEvent("east", "lost_ship", s.day, 0, problem);
                         ship = s.id;
                         break;
                     }
                 if (id.empty())
                     nextMorning(w);
             }
             expect(!id.empty(), "ship_lost: a ship due tomorrow, to be lost");
             expect(w.stagedEvent(id)->subject == "lost:" + ship, "ship_lost: that ship");
             g.save();
             g.settle();                            // (The checkpoint, the day before.)
             nextMorning(w);
             for (int t = 0; t < 4; ++t)
                 g.tick(.05);
             const auto* s = shipById(w, ship);
             expect(s && s->status == "storm" && s->lost && w.stagedEvent(id)->status == "on", "ship_lost: lost, its event on");
             auto was = json::Value::object();
             was.add("event", id);
             was.add("ship", ship);
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             const auto* s = shipById(g.world(), was.string("ship"));
             expect(s && s->status == "storm" && s->lost && g.world().stagedEvent(was.string("event"))->status == "on", "ship_lost: lost after the crash");
         }},
        // The slider's cause told after the checkpoint (its notices on every board): told, once.
        {"times_cause",
         [](game::Game& g) {
             auto& w = g.world();
             w.tick(.6);
             w.roads().camps.clear();
             g.save();
             g.settle();                            // (The checkpoint: no cause.)
             // (The DM's slider: the world's rules edited, then economy.sync, so a restart reads the same: doc 79, Phase 9.)
             const std::string edit = "rule \"prosperity\" -50\nrule \"prosperityCause\" \"the Crown's war levy\"\n";
             std::ofstream(std::filesystem::path(g.options().worldExport) / "world.ratw", std::ios::app) << edit;
             expect(g.applyEconomyCommand("economy.sync", "economy", json::Value::object()).ok, "times_cause: the slider moved");
             expect(w.nudgeTimes("east", 15, 4, "talk of a fair"), "times_cause: a nudge besides");
             for (int t = 0; t < 30; ++t)
                 g.tick(.1);
             expect(w.society().board().sliderCause.told && g.documents().find("times-east") && g.documents().find("times-west") &&
                        g.documents().find("nudge-east"),
                    "times_cause: told, on every board");
             auto was = json::Value::object();
             was.add("manifest", edit);                 // (The host's edit, there again at the restart.)
             return was;
         },
         [](game::Game& g, const json::Value&) {
             const auto& c = g.world().society().board().sliderCause;
             expect(c.told && c.words == "the Crown's war levy" && g.documents().find("times-east"), "times_cause: told after the crash");
         }},
        // Doc 79, Phase 9: each of the DM's live acts after the checkpoint (Game::applyEconomyCommand journals it), then the
        // crash: there once after the restart.
        {"dm_nudge",
         [](game::Game& g) {
             dmCheckpoint(g);
             expect(dmAct(g, "times.nudge", "east", {{"points", 37}, {"days", 3}, {"words", "boom at the port"}}).ok, "dm_nudge: applied");
             dmTold(g);
             return json::Value::object();
         },
         [](game::Game& g, const json::Value&) {
             const auto& n = g.world().society().board().nudges;
             expect(n.count("east") && n.at("east").points == 37 && n.at("east").cause == "boom at the port", "dm_nudge: there after the crash");
         }},
        {"dm_event_add",
         [](game::Game& g) {
             dmCheckpoint(g);
             const auto r = dmAct(g, "event.add", "east", {{"kind", "house_wedding"}, {"day", double(today(g.world()) + 2)}, {"hour", 10}});
             expect(r.ok && r.message.rfind(r.targetId + ": ", 0) == 0, "dm_event_add: " + r.message);
             dmTold(g);
             auto was = json::Value::object();
             was.add("event", r.targetId);
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             const auto* e = g.world().stagedEvent(was.string("event"));
             expect(e && e->by == "dm" && e->kind == "house_wedding", "dm_event_add: the event, once, after the crash");
         }},
        {"dm_event_cancel",
         [](game::Game& g) {
             std::string problem;
             g.world().tick(.6);
             const auto id = g.world().addStagedEvent("east", "house_wedding", today(g.world()) + 2, 10, problem);
             expect(!id.empty(), "dm_event_cancel: an event to cancel: " + problem);
             dmTold(g);
             dmCheckpoint(g);
             expect(dmAct(g, "event.cancel", id, {}).ok, "dm_event_cancel: applied");
             dmTold(g);
             auto was = json::Value::object();
             was.add("event", id);
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             const auto* e = g.world().stagedEvent(was.string("event"));
             expect(e && e->status == "cancelled", "dm_event_cancel: cancelled after the crash");
         }},
        {"dm_storm",
         [](game::Game& g) {
             g.world().tick(.6);
             std::string ship, name;
             for (const auto& s : g.world().society().board().ships)
                 if (s.status == "due" && ship.empty())
                     ship = s.id, name = s.name;
             expect(!ship.empty(), "dm_storm: a ship due");
             dmCheckpoint(g);
             const auto stormed = dmAct(g, "ship.storm", ship, {});
             expect(stormed.ok && stormed.message.rfind("A storm keeps " + name + " (" + ship + ", for ", 0) == 0,
                    "dm_storm: applied, the ship named (second repair): " + stormed.message);
             dmTold(g);
             auto was = json::Value::object();
             was.add("ship", ship);
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             const auto* s = shipById(g.world(), was.string("ship"));
             expect(s && s->status == "storm", "dm_storm: stormbound after the crash");
         }},
        {"dm_reset",
         [](game::Game& g) {
             g.world().tick(.6);
             g.world().society().setPressure("east", "meal", .3, "players");
             dmCheckpoint(g);
             expect(dmAct(g, "prices.reset", "east", {{"words", "a new fair"}}).ok, "dm_reset: applied");
             dmTold(g);
             return json::Value::object();
         },
         [](game::Game& g, const json::Value&) {
             const auto& r = g.world().society().board().resets;
             expect(r.size() == 1 && r.count("east") && r.at("east").words == "a new fair", "dm_reset: the reset, once, after the crash");
         }},
        {"dm_band",
         [](game::Game& g) {
             g.world().tick(.6);
             nextMorning(g.world());                // (Households made at the day's turn.)
             for (int h = 6; h < 10; ++h)
             {
                 expect(g.world().setTimeOfDay(double(h) + .3).ok, "dm_band: the morning");
                 g.world().tick(.6);
             }
             expect(!g.world().society().householdOf("e1").empty(), "dm_band: e1 in a household");
             dmCheckpoint(g);
             const auto r = dmAct(g, "band.override", "e1", {{"band", "poor"}, {"days", 9}});
             expect(r.ok, "dm_band: " + r.message);
             auto was = json::Value::object();
             was.add("household", g.world().society().householdOf("e1"));
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             const auto& o = g.world().society().state().staged.overrides;
             const auto found = o.find(was.string("household"));
             expect(found != o.end() && found->second.band == status::Poor, "dm_band: held poor after the crash");
         }},
        {"dm_grant",
         [](game::Game& g) {
             Client bea;
             enterAt(g, bea, 85, "bea", "east");
             const auto id = bea.entityId;
             g.disconnect(&bea);
             dmCheckpoint(g);
             const auto before = g.world().society().account(id)->cash;
             auto payload = json::Value::object();
             payload.add("coins", 40);
             payload.add("reason", "for the bridge");
             expect(g.applyEconomyCommand("character.grant", id, payload, "7071").ok, "dm_grant: applied");
             expect(g.dmActs().find("7071") != nullptr, "dm_grant: its dm.actions id kept");
             auto was = json::Value::object();
             was.add("player", id);
             was.add("cash", double(before + 40));
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             const auto* a = g.world().society().account(was.string("player"));
             expect(a && double(a->cash) == was.number("cash"), "dm_grant: paid once after the crash");
             // (The row a crash left queued is answered from the record, never applied again: Game::applyDmActions.)
             const auto* act = g.dmActs().find("7071");
             expect(act && act->boolean("ok") && act->string("text") == "Granted 40p: for the bridge.",
                    "dm_grant: its dm.actions id in the record, after the crash");
         }},
        {"dm_slider",
         [](game::Game& g) {
             dmCheckpoint(g);
             const std::string edit = "rule \"prosperity\" 40\nrule \"prosperityCause\" \"the silver strike\"\n";
             std::ofstream(std::filesystem::path(g.options().worldExport) / "world.ratw", std::ios::app) << edit;
             auto now = json::Value::object();
             now.add("now", true);
             expect(g.applyEconomyCommand("economy.sync", "economy", now).ok, "dm_slider: applied");
             expect(g.world().society().board().slider == 40, "dm_slider: at once");
             dmTold(g);
             auto was = json::Value::object();
             was.add("manifest", edit);
             return was;
         },
         [](game::Game& g, const json::Value&) {
             const auto& b = g.world().society().board();
             expect(b.slider == 40 && b.sliderTarget == 40 && b.sliderCause.words == "the silver strike", "dm_slider: as moved, after the crash");
         }},
    };
    return cases;
}

struct EventRun
{
    const EventCrash* c;
    bool window;
    std::string name;
};
const std::vector<EventRun>& eventRuns()
{
    static const std::vector<EventRun> runs = [] {
        std::vector<EventRun> all;
        for (const bool window : {false, true})
            for (const auto& c : eventCrashes())
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
    return (std::filesystem::temp_directory_path() / ("ratw-s2stage-eventcrash-" + name + "-" + std::to_string(::getpid()) + ".json")).string();
}
void cleanSave(const std::string& path)
{
    for (const auto* end : {"", ".journal", ".art", ".lock", ".reports", ".snapshot"})
        std::filesystem::remove_all(path + end);
}

// The window between a snapshot's capture and its landing, held open (port_tests' own).
void aSnapshotDoesNotLand(game::Game& g, const std::string& path, const std::string& log, const std::string& name)
{
    g.settle();
    const auto held = slurp(path);
    // (A folder in the snapshot's way, with a file in it: a failed snapshot's clean-up removes its document's path, and an
    // empty folder went with it, so the next one forked in the same pass landed under a loaded machine. Phase 9's first
    // repair: dm_reset_window failed once that way.)
    std::filesystem::create_directory(path + ".snapshot");
    std::ofstream(path + ".snapshot/in-the-way") << "x";
    const auto failed = [&] { return log.find("the snapshot could not be written") != std::string::npos; };
    for (int t = 0; t < 45 && !failed(); ++t)
        g.tick(1);
    for (int t = 0; t < 500 && !failed(); ++t)
    {
        ::usleep(10000);
        g.tick(.01);
    }
    g.waitForSnapshot();                            // (One forked after the failure fails too, and is done with.)
    std::filesystem::remove_all(path + ".snapshot");
    expect(failed(), name + ": a snapshot is taken and cannot be written");
    expect(slurp(path) == held, name + ": the save is still the checkpoint before it");
}

// A purchase at one of west's shops (an economy record; port_tests' own).
void buysInWest(game::Game& g, Client& c, const std::string& name)
{
    auto& w = g.world();
    const auto cash = [&] {
        const auto* a = w.society().account(c.entityId);
        return a ? a->cash : 0;
    };
    const auto before = cash();
    for (const char* keeper : {"wg", "wm"})
        for (const char* item : {"meal", "nails", "flour", "herbs", "oats"})
        {
            const auto* k = w.entity(keeper);
            auto* me = w.entity(c.entityId);
            if (!k || !me || cash() < before)
                continue;
            me->cellId = k->cellId;
            me->position = {k->position.x + .5, k->position.y};
            g.command(&c, cmd({{"type", "trade"}, {"target", keeper}, {"item", item}, {"quantity", 1}, {"buy", true}}));
            g.settle();
        }
    if (cash() == before)
    {
        const double p = w.society().pressure("west", "meal");
        w.society().setPressure("west", "meal", p + .02, "players");
        expect(std::abs(w.society().pressure("west", "meal") - p - .02) < 1e-6, name + ": the Board moved as a purchase moves it");
    }
}

json::Value toTheEventCrash(const EventRun& r, const std::filesystem::path& root, const std::string& path)
{
    std::string log;
    auto o = optionsOn(root, path);
    o.forkSnapshots = r.window;
    game::Game g(o);
    g.log = [&log](const char*, const std::string& text) { log += text + "\n"; };
    std::string problem;
    expect(g.start(problem), r.name + ": a game starts on the harbour with a file save: " + problem);
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
    g.settle();
    const auto said = ash.said();
    expect(said.find("You pin your notice") != std::string::npos, r.name + ": a penny paid: an economy record: " + said);
    g.disconnect(&ash);
    expect(slurp(path) == saved, r.name + ": the save is the checkpoint from before the step (the journal is the tail)");
    was.add("atCrash", eventsAsItStands(g));
    return was;
}

void afterTheEventCrash(const EventRun& r, const std::filesystem::path& root, const std::string& path, const json::Value& was)
{
    // (A host's edit of the world's records made before the crash is there at the restart: a fresh copy of the world gets it.)
    if (const auto edit = was.string("manifest"); !edit.empty())
        if (const auto text = slurp(root / "world.ratw"); text.size() < edit.size() || text.compare(text.size() - edit.size(), edit.size(), edit) != 0)
            std::ofstream(root / "world.ratw", std::ios::app) << edit;
    game::Game g(optionsOn(root, path));
    std::string problem;
    expect(g.start(problem), r.name + ": starts again after the crash: " + problem);
    r.c->after(g, was);
    theEventsOfTheCrash(g, was, r.name);
    r.c->after(g, was);
}

Fixture crashWorld()
{
    rules = "rule \"eventRate\" 0\n";
    auto f = harbour();
    rules.clear();
    return f;
}

void eventsThroughACrash()
{
    for (const auto& r : eventRuns())
    {
        auto f = crashWorld();
        if (std::string(r.c->name) == "event_morning")
            f = harbour();                          // (Its morning rolls the seeded events.)
        const auto root = onDisk(f, "eventcrash-" + r.name);
        const auto path = crashSave(r.name);
        cleanSave(path);
        const auto was = toTheEventCrash(r, root, path);
        afterTheEventCrash(r, root, path, was);
        cleanSave(path);
        std::filesystem::remove_all(root);
    }
}

// --write-crashes DIR / --check-crashes BACK DIR: for Tests/events_pg.py (port_tests' own shape).
int writeCrashes(const std::filesystem::path& dir)
{
    namespace fs = std::filesystem;
    fs::create_directories(dir);
    for (const auto& r : eventRuns())
    {
        auto f = std::string(r.c->name) == "event_morning" ? harbour() : crashWorld();
        const auto root = onDisk(f, "eventcrash-" + r.name);
        const auto path = crashSave(r.name);
        cleanSave(path);
        const auto was = toTheEventCrash(r, root, path);
        expect(fs::exists(path) && fs::exists(path + ".journal"), r.name + ": a checkpoint and a journal written");
        fs::copy_file(path, dir / (r.name + ".json"), fs::copy_options::overwrite_existing);
        fs::copy_file(path + ".journal", dir / (r.name + ".journal"), fs::copy_options::overwrite_existing);
        std::ofstream(dir / (r.name + ".was.json"), std::ios::binary) << json::dump(was);
        cleanSave(path);
        fs::remove_all(root);
    }
    return 0;
}

int checkCrashes(const std::filesystem::path& back, const std::filesystem::path& dir)
{
    namespace fs = std::filesystem;
    for (const auto& r : eventRuns())
    {
        auto f = std::string(r.c->name) == "event_morning" ? harbour() : crashWorld();
        const auto root = onDisk(f, "eventcrash-" + r.name);
        const auto path = crashSave(r.name);
        cleanSave(path);
        fs::copy_file(back / (r.name + ".json"), path);
        fs::copy_file(back / (r.name + ".journal"), path + ".journal");
        json::Value was;
        std::string error;
        expect(json::parse(slurp(dir / (r.name + ".was.json")), was, error), r.name + ": what was so reads: " + error);
        afterTheEventCrash(r, root, path, was);
        cleanSave(path);
        fs::remove_all(root);
    }
    std::cout << "events_tests --check-crashes: " << eventRuns().size() << " event crashes replayed the same through Postgres (" << checks << " checks)\n";
    return 0;
}
// Doc 79, Phase 9's first repair: a host's welcomeDays held to its default (a shorter period is the generous way); the
// report's DM bands with their end and members, and next week's first day; a new world's report counting its households
// once its first bands are worked out, not a day later; a DM's act journalled with its dm.actions id (dmActs, kept
// through a checkpoint), and the player told of a grant only once the record is committed.
void theFirstRepair()
{
    namespace fs = std::filesystem;
    auto fixture = crashWorld();
    const auto root = onDisk(fixture, "events-firstrepair");
    const auto save = crashSave("firstrepair");
    cleanSave(save);
    std::string problem;
    {
        game::Game g(optionsOn(root, save));
        expect(g.start(problem), "first repair: a game on the harbour with a file save: " + problem);
        auto& w = g.world();
        auto& soc = w.society();
        const auto households = [&] {
            int n = 0;
            for (const auto& m : soc.report().array("markets"))
                for (const auto& [band, count] : m.object("households").fields())
                    n += int(count.asNumber());
            return n;
        };
        expect(soc.state().staged.bands.empty() && households() == 0, "first repair: a new world, no bands yet");
        for (int t = 0; t < 200 && soc.state().staged.bands.empty(); ++t)
            g.tick(.1);
        expect(!soc.state().staged.bands.empty(), "first repair: its first bands");
        g.tick(.1);
        const auto day = soc.state().budgetDay;
        expect(households() > 0 && soc.report().number("day") == double(day),
               "first repair: the report counts the households at once: " + json::dump(soc.report()).substr(0, 300));
        // welcomeDays: held to at least the shipped default (7), as every other cap is held to five times its own.
        {
            auto spec = soc.authored().economy;
            json::Value v;
            expect(json::parse(R"({"caps": {"welcomeDays": 0, "welcomeCoins": 100}})", v, problem), problem);
            spec.overrides.status = v;
            std::vector<std::string> warnings;
            soc.adoptOverrides(spec.overrides, &warnings);
            expect(soc.statusTable().cap("welcomeDays") == 7 && soc.statusTable().cap("welcomeCoins") == 100,
                   "first repair: welcomeDays 0 is held to 7 (the welcome stays weekly)");
            soc.adoptOverrides({}, nullptr);
        }
        // The report: the DM's band for a while, with its end and members; next week's first day.
        std::string npc;
        for (const auto& [id, life] : soc.state().residents)
            if (!soc.householdOf(id).empty())
            {
                npc = id;
                break;
            }
        expect(!npc.empty(), "first repair: a resident in a household");
        auto band = json::Value::object();
        band.add("band", "poor");
        band.add("days", 10);
        expect(g.applyEconomyCommand("band.override", npc, band).ok, "first repair: a band for a while");
        bool shown = false;
        for (const auto& o : soc.report().array("bandOverrides"))
            if (o.string("household") == soc.householdOf(npc) && o.string("band") == "poor" && o.number("until") == double(day + 10))
                for (const auto& m : o.array("members"))
                    shown |= m.asString() == npc;
        expect(shown, "first repair: the report shows it, its end and its members: " + json::dump(soc.report().array("bandOverrides")));
        // Second repair: a band for N days is N days, counting today: in force on its last day (the DM tool shows it), gone
        // the day after (it lasted a day more than asked, and the report dropped it a day before it lapsed).
        {
            soc.overrideBand(soc.householdOf(npc), -1, 0);
            soc.stagedBands(day + 2);
            const int natural = soc.statusOf(npc)->band;
            const char* other = natural == status::Rich ? "destitute" : "rich";
            auto two = json::Value::object();
            two.add("band", other);
            two.add("days", 2);
            expect(g.applyEconomyCommand("band.override", npc, two).ok && soc.state().staged.overrides.at(soc.householdOf(npc)).until == day + 2,
                   "second repair: a band for two days");
            soc.stagedBands(day + 1);
            expect(soc.statusOf(npc)->band == status::bandOf(other) && soc.statusOf(npc)->overridden, "second repair: in force on its second day");
            soc.stagedBands(day + 2);
            expect(!soc.statusOf(npc)->overridden || soc.spec(npc)->band >= 0, "second repair: lapsed on the third (" +
                                                                                   std::to_string(natural) + ")");
            soc.stagedBands(day);
        }
        const auto today = std::int64_t(std::floor(w.calendarDays()));
        expect(soc.report().number("nextWeek") == double((today / calendar::DaysPerWeek + 1) * calendar::DaysPerWeek),
               "first repair: next week's first day");
        // A grant with its dm.actions id: journalled with it; the player told only once the record is written.
        Client cy;
        enterAt(g, cy, 91, "cy", "east");
        g.save();
        g.settle();
        const auto journal = save + ".journal";
        if (!fs::exists(journal))
            std::ofstream(journal, std::ios::binary);
        if (::geteuid() != 0)
        {
            // (The writer can't write: it retries. A trim after the save writes the journal anew, so the mode is set
            // again until it holds.)
            for (int t = 0; t < 20; ++t)
            {
                fs::permissions(journal, fs::perms::owner_read, fs::perm_options::replace);
                ::usleep(50000);
                if ((fs::status(journal).permissions() & fs::perms::owner_write) == fs::perms::none)
                    break;
            }
            auto grant = json::Value::object();
            grant.add("coins", 23);
            grant.add("reason", "for the lost ewe");
            const auto r = g.applyEconomyCommand("character.grant", cy.entityId, grant, "4242");
            expect(r.ok && g.dmActs().find("4242"), "first repair: granted, its id kept: " + r.message);
            for (int t = 0; t < 10; ++t)
            {
                ::usleep(20000);
                g.tick(.05);
            }
            expect(cy.said().find("grants you") == std::string::npos, "first repair: not told while the record isn't written: " + cy.said());
            fs::permissions(journal, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace);
            for (int t = 0; t < 100 && cy.said().find("grants you") == std::string::npos; ++t)
            {
                ::usleep(20000);
                g.tick(.05);
            }
            expect(cy.said().find("A Dungeon Master grants you 23 pennies: for the lost ewe.") != std::string::npos,
                   "first repair: told once it is: " + cy.said());
            const auto text = slurp(journal);
            expect(text.find("\"dmActs\"") != std::string::npos && text.find("\"4242\"") != std::string::npos,
                   "first repair: the record carries the act's id");
        }
        g.disconnect(&cy);
        g.save();
        g.settle();
    }
    if (::geteuid() != 0)
    {
        game::Game again(optionsOn(root, save));
        expect(again.start(problem), "first repair: restarted: " + problem);
        expect(again.dmActs().find("4242") != nullptr, "first repair: the act's id kept through the checkpoint");
    }
    cleanSave(save);
    fs::remove_all(root);
}

// What this run left in the temporary folder (its worlds on disk and their saves), removed passed or failed.
void cleanTemp()
{
    namespace fs = std::filesystem;
    std::error_code error;
    const auto pid = "-" + std::to_string(::getpid());
    std::vector<fs::path> mine;
    for (const auto& entry : fs::directory_iterator(fs::temp_directory_path(), error))
    {
        const auto name = entry.path().filename().string();
        if ((name.rfind("ratw-s2stage-event", 0) == 0) && name.find(pid) != std::string::npos)
            mine.push_back(entry.path());
    }
    for (const auto& path : mine)
        fs::remove_all(path, error);
}
} // namespace

int main(int argc, char** argv)
{
    struct Clean
    {
        ~Clean() { cleanTemp(); }
    } clean;
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--write-crashes")
            return writeCrashes(argv[2]);
        if (argc == 4 && std::string(argv[1]) == "--check-crashes")
            return checkCrashes(argv[2], argv[3]);
        const std::vector<std::pair<std::string, void (*)()>> tests{
            {"theListAndItsChecks", theListAndItsChecks},
            {"eventsEachWeek", eventsEachWeek},
            {"anEventPressesItsGoods", anEventPressesItsGoods},
            {"theSlidersCause", theSlidersCause},
            {"theDMsNudgeAndHarvest", theDMsNudgeAndHarvest},
            {"shipsInTheEvents", shipsInTheEvents},
            {"aLameOx", aLameOx},
            {"aWorldWithNothingButAMarket", aWorldWithNothingButAMarket},
            {"aFestivalsBuyingAhead", aFestivalsBuyingAhead},
            {"moneyStaysReal", moneyStaysReal},
            {"savesAndOlderSaves", savesAndOlderSaves},
            {"noticesAndTheCrier", noticesAndTheCrier},
            {"eventsThroughACrash", eventsThroughACrash},
            // Phase 7 repaired.
            {"phase7Repaired", phase7Repaired},
            {"theRepairOnTheBoards", theRepairOnTheBoards},
            // Phases 7 and 8 repaired, together.
            {"theRepairsTogether", theRepairsTogether},
            // Doc 79, Phase 9: the DM's levers.
            {"theDMsLevers", theDMsLevers},
            // Doc 79, Phase 9's first repair.
            {"theFirstRepair", theFirstRepair}};
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
    std::cout << "Events tests passed: " << checks << " checks.\n";
    return 0;
}
