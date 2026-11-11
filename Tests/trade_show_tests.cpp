// Staged trade on the roads (Docs/Design/79-a-staged-economy.md, Phase 4; 16.5, grown from roads_tests' townsAndCaravans):
// wagons committed when posted, between real markets on real routes, with no account, keeping their notice's promise
// through a DM change; driven by real carters away from their posts for the trip, home robbed or hurt after a raid, never
// dead; manifests of goods the destination's shops stock or buy; notices with the calendar's weekday names; an arrival's
// refill and Board nudge at that moment, a departure's glut halved, a raid's +0.15 and its goods at the camp, a late
// wagon's cause and its +0.05 a day; imports thin between wagons and refilled on arrival from the back room (D27), a
// market with no route refilling every morning; farm carts every morning by their harvest, with their baseline and
// Marketday's stalls; no staffed farm, no carts; an endpoint deleted while a wagon travels a clean drop on load, its
// letters reopened or refunded. The catalogue is the real Data (RATW_SOURCE_DIR).
#include "test_purse.h"
#include "RatwCalendar.h"
#include "RatwGame.h"
#include "RatwItems.h"
#include "RatwWorld.h"
#include "battle_play.h"

#include <algorithm>
#include <thread>
#include <chrono>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <unistd.h>
#include <iostream>
#include <map>
#include <memory>
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

const market::Departure* firstWagon(const World& w)
{
    for (const auto& d : w.society().board().committed)
        if (d.kind == "wagon")
            return &d;
    return nullptr;
}

// Days on until a wagon is posted (at most two weeks): its departure.
market::Departure postedWagon(World& w, std::vector<WorldEvent>* events = nullptr)
{
    for (int day = 0; day < 14; ++day)
    {
        if (const auto* d = firstWagon(w))
            return *d;
        nextMorning(w);
        for (auto& e : w.takeEvents())
            if (events)
                events->push_back(e);
    }
    expect(false, "a wagon is posted within two weeks");
    return {};
}

// To the departure's hour on its day, and a little after: it sets out.
const Caravan* setOut(World& w, const market::Departure& d)
{
    while (std::floor(w.calendarDays()) < double(d.day))
        nextMorning(w);
    expect(w.setTimeOfDay(std::min(23.5, d.hour + .05)).ok, "to the departure's hour");
    for (int t = 0; t < 8; ++t)
        w.tick(.5);
    for (const auto& c : w.roads().caravans)
        if (c.departure == d.id)
            return &c;
    return nullptr;
}

const Caravan* caravan(const World& w, const std::string& trip)
{
    for (const auto& c : w.roads().caravans)
        if (c.id == trip)
            return &c;
    return nullptr;
}

bool until(World& w, const std::string& kind, const std::string& trip, int seconds, std::vector<WorldEvent>* seen = nullptr)
{
    for (int t = 0; t < seconds; ++t)
    {
        w.tick(1);
        for (auto& e : w.takeEvents())
        {
            if (seen)
                seen->push_back(e);
            if (e.kind == kind && e.actor == trip)
                return true;
        }
    }
    return false;
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

// The roads run on the markets, with their carters (5.1; D26), and a wagon is committed when posted (8.1).
void wagonsCommittedWhenPosted()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    expect(w.roadMarkets().size() == 2 && w.roadMarket("east") && w.roadMarket("west"), "two markets on the roads");
    expect(w.cartersOf("east") == std::vector<std::string>{"c1", "c2"} && w.cartersOf("west").empty(),
           "east's carters are Tam and Hob; west has none");
    std::vector<WorldEvent> events;
    const auto d = postedWagon(w, &events);
    const auto today = std::int64_t(std::floor(w.calendarDays()));
    expect(d.from == "east" && d.to == "west" && (d.driver == "c1" || d.driver == "c2"),
           "a wagon from east (west has no carter to send one), driven by one of east's carters: " + d.from + ">" + d.to + " " + d.driver);
    expect(d.day > today - 1 && d.day <= today + 2, "posted two days ahead at most");
    expect(!d.manifest.empty() && d.manifest.size() <= 5, "carrying up to five goods");
    const auto* west = w.society().tradePoints().find("west");
    expect(west, "west is a market of the Board's");
    for (const auto& [good, n] : d.manifest)
        expect(n > 0 && (west->sold.count(good) || west->bought.count(good)),
               "each good one some shop in west stocks or buys: " + good);
    // Its notices, with the calendar's weekday names, at both ends.
    const auto weekday = calendar::weekdayName(calendar::weekdayOf(double(d.day) + d.hour / 24));
    bool posted = false;
    for (const auto& e : events)
        if (e.kind == "wagon posted" && e.actor == d.id)
        {
            posted = true;
            const auto cut = e.detail.find('\n');
            const auto atSource = e.detail.substr(0, cut), atEnd = e.detail.substr(cut + 1);
            expect(atSource.rfind("Wagon to West, " + weekday + ": ", 0) == 0, "the source's notice names its day: " + atSource);
            expect(atEnd.rfind("Wagon from East, due " + weekday + ": ", 0) == 0, "and the far end's: " + atEnd);
            const auto* driver = w.entity(d.driver);
            expect(driver && atSource.find(driver->name + " driving.") != std::string::npos, "and its carter: " + atSource);
            expect(atSource.find(World::goodWords(d.manifest.front().first)) != std::string::npos && atSource.find('(') == std::string::npos,
                   "and what it carries, in plain words: " + atSource);
            expect(e.item == "east" && e.target == "west", "between its two markets");
        }
    expect(posted, "a notice is posted for it");
    // A DM change (no wagons at all) after posting: the posted one still keeps its notice's promise.
    auto spec = w.society().authored().economy;
    spec.rules["caravanRate"] = 0;
    w.society().adoptEconomy(spec);
    const auto* out = setOut(w, d);
    expect(out && out->kind == "wagon" && out->from == "east" && out->to == "west" && out->account.empty() &&
               !w.society().account("caravan:" + out->id) && out->route.size() >= 2,
           "it sets out on its day between real markets on a real route, carrying no account");
    expect(out->manifest == d.manifest && out->driver == d.driver, "with the manifest and the carter it was posted with");
    // And no new wagon is posted after the change.
    const auto trip = out->id;
    for (int day = 0; day < 4; ++day)
        nextMorning(w);
    for (const auto& dep : w.society().board().committed)
        expect(dep.kind != "wagon" || dep.posted < double(d.day), "no wagon posted once the DM stopped them");
    expect(w.society().realBalanced(), "real money is balanced");
    (void)trip;
}

// The carter is a real resident away from his post for the trip (D26), and the trip moves the Board at the moments a
// player could see (7.4, 8.6): the glut halved as it loads, the gap closed by a fifth as it arrives (never up), the
// shelves refilled, the far market given the source's news.
void aWagonsTrip()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    const auto d = postedWagon(w);
    w.roads().camps.clear();
    const auto good = d.manifest.front().first;
    // A glut of it in east before it loads, and a want of it in west.
    w.society().setPressure("east", good, -.3, "players");
    w.society().setPressure("west", good, .3, "players");
    const auto* out = setOut(w, d);
    expect(out, "it sets out");
    const auto trip = out->id;
    const double sourceP = out->sourceP.at(good);
    expect(sourceP < -.1, "its source's pressure as it set out kept: " + std::to_string(sourceP));
    expect(std::abs(w.society().pressure("east", good) - sourceP / 2) < .01, "the glut in east halved as it loads: " +
                                                                               std::to_string(w.society().pressure("east", good)));
    const auto driver = d.driver;
    const auto* job = w.society().jobOf(driver);
    expect(job, "its carter holds a post");
    run(w, 30);
    const auto* carter = w.entity(driver);
    expect(carter && carter->activity.rfind("on the West run", 0) == 0, "the carter is on the West run: " + (carter ? carter->activity : ""));
    expect(w.drivers().count(driver) && w.drivers().at(driver) == trip, "driving it");
    const auto* wagon = w.entity("road:" + trip);
    expect(wagon && wagon->description.find(carter->name) != std::string::npos, "the wagon shows who drives it: " +
                                                                                  (wagon ? wagon->description : ""));
    expect(wagon->description.find("guard") != std::string::npos && wagon->description.find('(') == std::string::npos,
           "and its guard (T4), its goods in plain words: " + wagon->description);
    // West's shelf of the good sold out before it comes.
    std::string shopKeeper;
    for (const auto& keeper : {"wg", "wm"})
        for (const auto& ware : w.society().wares(keeper))
            if (items::baseOf(ware) == good && shopKeeper.empty())
                shopKeeper = keeper;
    const double westBefore = w.society().pressure("west", good);
    std::string ware;
    if (!shopKeeper.empty())
    {
        for (const auto& x : w.society().wares(shopKeeper))
            if (items::baseOf(x) == good)
                ware = x;
        w.society().consume(w.society().tillOf(shopKeeper), ware, stock(w, shopKeeper, ware), "test: sold out");
    }
    std::vector<WorldEvent> seen;
    expect(until(w, "caravan arrives", trip, 1500, &seen), "it arrives in west");
    const double westAfter = w.society().pressure("west", good);
    const double expected = westBefore - .2 * (westBefore - sourceP);
    expect(std::abs(westAfter - expected) < .03 && westAfter < westBefore, "the gap closed by a fifth as it arrives: " +
                                                                            std::to_string(westBefore) + " to " + std::to_string(westAfter));
    if (!shopKeeper.empty())
        expect(stock(w, shopKeeper, ware) >= w.society().shelfTarget(w.society().tillOf(shopKeeper), ware),
               "the shelf of it in west refilled at once");
    const auto& news = w.society().board().news;
    expect(news.count("west") && news.at("west").from == "east" && news.at("west").words.find("cheap in East") != std::string::npos,
           "it brings the news of east: " + (news.count("west") ? news.at("west").words : std::string("none")));
    const auto plain = World::goodWords(good);
    expect(news.at("west").words == plain + (World::pluralGood(plain) ? " are" : " is") + " cheap in East",
           "in plain words: " + news.at("west").words);
    // A wagon that brings a good never makes it dearer: a second arrival with west already cheaper than east moves nothing.
    // Home again, its carter back to his post.
    expect(until(w, "caravan home", trip, 1500), "it comes home");
    run(w, 5);
    expect(!w.drivers().count(driver), "and its carter is free again");
    expect(w.society().realBalanced(), "real money is balanced");
}

// An arrival never raises a cargo good's P (7.4): with the far end cheaper than the source, nothing moves.
void anArrivalNeverRaises()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    const auto* sent = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{"nails", 10}});
    expect(sent && sent->manifest.size() == 1, "a wagon with nails");
    const auto trip = sent->id;
    w.society().setPressure("east", "nails", .2, "players");
    w.society().setPressure("west", "nails", -.2, "players");
    // (Its sourceP was read as it set out: 0, east's then.)
    expect(until(w, "caravan arrives", trip, 1500), "it arrives");
    expect(w.society().pressure("west", "nails") <= -.19, "west's cheap nails not made dearer by the wagon");
}

// Ada goes out to the camp in the middle of the road and fights it to the end: true when she breaks it.
bool adaBreaksTheCamp(World& w)
{
    auto* ada = w.entity("player-ada");
    ada->strength = 90;
    ada->dexterity = 90;
    w.roads().camps[0].strength = 6;
    ada->cellId = id(3);
    ada->position = {8.5, 8.5};
    run(w, 2);
    ada->cellId = id(4);
    const auto* leader = w.entity("road:camp_mid:0");
    expect(leader, "the camp's leader is on the road");
    ada->position = {std::max(.6, leader->position.x - 1), leader->position.y};
    const auto fight = w.attack("player-ada", leader->id);
    expect(fight.ok, "a fight begins: " + fight.message);
    bool cleared = false;
    for (int t = 0; t < 6000 && !cleared; ++t)
    {
        auto* me = w.entity("player-ada");
        me->stamina = std::max(me->stamina, 60.0);
        me->hurt = 0;
        test::playTurn(w, "player-ada");
        w.tick(.25);
        cleared = !w.roads().camps[0].active;
    }
    return cleared;
}

// A raid (8.1, 8.6): the price rises at the raid at the destination, its goods are at the camp for whoever clears it,
// a bounty within the cap, the carter home robbed or hurt and never dead; then a player breaks the camp and finds the
// cargo.
void aRaid()
{
    auto f = strip();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    // A strong, starving camp in the middle of the road, and wagons until one is robbed (the odds are the camp's).
    auto spec = w.society().authored().economy;
    spec.rules["campStrengthMost"] = 20;
    w.society().adoptEconomy(spec);
    const std::string good = "nails";
    const double before = w.society().pressure("west", good);
    std::string trip, driver;
    std::vector<WorldEvent> seen;
    for (int attempt = 0; attempt < 10 && trip.empty(); ++attempt)
    {
        w.roads().caravans.clear();
        w.roads().camps = {{"camp_mid", id(4), 20, 100, -100, true}};
        const auto* out = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{good, 10}});
        expect(out && !out->driver.empty(), "a wagon sets out, a carter driving");
        const auto id = out->id;
        const auto who = out->driver;
        w.roads().caravans.back().guards = 0;
        bool passed = false;
        for (int t = 0; t < 1500 && trip.empty() && !passed; ++t)
        {
            w.tick(1);
            for (auto& e : w.takeEvents())
            {
                seen.push_back(e);
                if (e.kind == "raid" && e.actor == "camp_mid")
                    trip = id, driver = who;
                passed |= e.kind == "caravan passes" && e.actor == id;
            }
        }
    }
    expect(!trip.empty(), "the bandits fall on a wagon");
    const market::Departure d{"", "wagon", "east", "west", driver};
    const auto* c = caravan(w, trip);
    expect(c && c->status == "raided" && c->manifest.empty(), "robbed, it turns for home empty");
    expect(std::abs(w.society().pressure("west", good) - before - .15) < .02, "its goods dearer in west at the raid");
    const auto* hoard = w.society().account("bandits:camp_mid");
    expect(hoard && hoard->cash == 0, "no coins into the camp");
    std::int64_t worth = 0;
    int pieces = 0;
    for (const auto& [item, n] : hoard->stock)
        worth += n * Society::catalogPrice(item), pieces += n;
    expect(pieces > 0 && worth <= 40, "its cargo at the camp, at most 40p of it: " + std::to_string(worth));
    bool bounty = false;
    for (const auto& k : w.roads().contracts)
        if (k.kind == "bounty" && k.target == "camp_mid")
            bounty = k.town == "west" && k.reward > 0 && k.reward <= w.society().rule("bountyMost");
    expect(bounty, "west puts a price on the camp, within the cap");
    bool hurtOrRobbed = false;
    for (const auto& e : seen)
        hurtOrRobbed |= (e.kind == "carter hurt" || e.kind == "carter robbed") && e.actor == d.driver;
    const auto* carter = w.entity(d.driver);
    expect(hurtOrRobbed && carter && !carter->dead, "its carter robbed or hurt, never killed");
    expect(w.roads().camps[0].strength <= w.society().rule("campStrengthMost"), "the camp's strength within its cap");
    // The raid is a trouble for west's Times this week.
    nextMorning(w);
    expect(w.society().board().timesShift.count("west") && w.society().board().timesShift.at("west") <= -10, "west's Times shaken by the raid");
    // Ada breaks the camp and finds the cargo.
    expect(adaBreaksTheCamp(w), "she breaks the camp");
    int found = 0;
    if (const auto* mine = w.society().account("player-ada"))
        for (const auto& [item, n] : mine->stock)
            if (item == good)
                found += n;
    expect(found > 0, "and finds the wagon's cargo among their things");
    expect(w.society().realBalanced(), "real money is balanced");
}

// The cargo found at a broken camp is a reward at the door (9.3): staged goods to a player, tallied to her holder by their
// catalogue worth with the bounty, and held to the day's rewards cap with it; what is over the cap goes back. What she is
// told, and the event's coins, are what she was paid, not what was posted (third repair).
void foundCargoIsAReward()
{
    const std::string good = "nails";
    const auto each = Society::catalogPrice(good);
    const std::string dayPaid = "you have had all the rewards one day pays";
    // Her day's rewards: all before her; three pieces short of the cap; room for all the cargo and 7p of the bounty.
    enum { Free, NearTheCap, PartOfTheBounty };
    for (const int day : {Free, NearTheCap, PartOfTheBounty})
    {
        const std::string when = day == NearTheCap ? " (near the cap)" : day == PartOfTheBounty ? " (room for part of the bounty)" : "";
        auto f = strip();
        auto w = load(f);
        w.addPlayer("player-ada", "Ada");
        w.tick(.6);
        auto& s = w.society();
        const auto holder = s.holderOf("player-ada");
        w.roads().caravans.clear();
        w.roads().camps = {{"camp_mid", id(4), 6, 100, -100, true}};
        // What a raid leaves: its cargo at the camp, and west's price on the camp from its treasury.
        const int pieces = int(std::min<std::int64_t>(10, 40 / std::max<std::int64_t>(1, each)));
        expect(each > 0 && pieces >= 4 && s.openAccount("bandits:camp_mid") && s.create("bandits:camp_mid", good, pieces, "raided cargo"),
               "a raided wagon's cargo at the camp" + when);
        const auto reward = w.postContract("bounty", s.treasuryOf("west"), "west", "camp_mid", 20, 20, "the bandits robbing the road").reward;
        expect(reward == 20, "a price on the camp" + when);
        const auto cap = s.statusTable().cap("rewardsADay");
        const std::int64_t room = day == NearTheCap ? 3 * each : pieces * each + 7;
        if (day != Free)
            expect(s.shift("treasury", "player-ada", "", 0, cap - room, "faction mission reward") && s.faucetRoom("player-ada", "rewardsADay") == room,
                   "her day's other rewards leave her so much room" + when);
        const auto had = s.faucetToday(holder, "reward");
        const auto purse = s.cash("player-ada");
        const auto held = [&] {
            const auto* mine = s.account("player-ada");
            return mine ? Society::stock(*mine, good) : 0;
        };
        const int before = held();
        w.takeNotices();
        w.takeEvents();
        expect(adaBreaksTheCamp(w), "she breaks the camp" + when);
        const int found = held() - before;
        const auto got = s.faucetToday(holder, "reward") - had;
        const auto paid = s.cash("player-ada") - purse;
        std::string told;
        for (const auto& [who, words] : w.takeNotices())
            if (who == "player-ada")
                told += words + "\n";
        std::int64_t said = -1;
        for (const auto& e : w.takeEvents())
            if (e.kind == "contract done" && e.detail.rfind("bounty:", 0) == 0)
                said = e.coins;
        const auto tells = [&](const std::string& words) { return told.find(words) != std::string::npos; };
        if (day == Free)
        {
            expect(found == pieces && paid == reward, "she finds all the cargo, and the bounty is hers: " + std::to_string(found) + ", " + std::to_string(paid));
            expect(got == reward + pieces * each,
                   "the cargo counted with the bounty in her day's rewards, by its catalogue worth: " + std::to_string(got) + " of " +
                       std::to_string(reward + pieces * each));
            expect(tells(" The bounty on them, 20 pennies, is yours.") && !tells(dayPaid) && said == 20,
                   "told the bounty is hers, the event its 20: " + std::to_string(said) + "; " + told);
        }
        else if (day == NearTheCap)
        {
            expect(found == 3 && paid == 0, "she finds only what the day's cap has room for, and no bounty past it: " + std::to_string(found) +
                                                ", " + std::to_string(paid));
            expect(got == 3 * each && s.faucetToday(holder, "reward") == cap && s.faucetRoom("player-ada", "rewardsADay") == 0,
                   "her day's rewards at the cap, the cargo in them: " + std::to_string(s.faucetToday(holder, "reward")));
            expect(tells(" The rest of it goes back to its carters.") &&
                       tells(" The bounty on them goes back to those who posted it: " + dayPaid + ".") && !tells("is yours") && said == 0,
                   "told the bounty goes back, not that it is hers; the event says nothing paid: " + std::to_string(said) + "; " + told);
        }
        else
        {
            expect(found == pieces && paid == 7, "she finds all the cargo, and the 7p of the bounty the day has room for: " +
                                                     std::to_string(found) + ", " + std::to_string(paid));
            expect(got == pieces * each + 7 && s.faucetToday(holder, "reward") == cap, "her day's rewards at the cap" + when);
            expect(tells(" Of the bounty on them, 20 pennies, you are paid 7 pennies: " + dayPaid + ", and the rest goes back.") &&
                       !tells("is yours") && said == 7,
                   "told what she is paid of it, the event its 7: " + std::to_string(said) + "; " + told);
        }
        expect(!s.account("bandits:camp_mid") && !s.account("contract:k" + std::to_string(w.roads().nextId - 1)), "nothing left at the camp, nor in the bounty's pot" + when);
        expect(s.realBalanced(), "real money is balanced" + when);
    }
}

// More kinds of thing than she can hold (a save holds 64 kinds: MaxGoodsKinds): the cargo at a camp she breaks is not
// hers, and it isn't left at a camp that is gone either: it goes back, and the camp's account closes (third repair).
void aFullPackAtABrokenCamp()
{
    auto f = strip();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    auto& s = w.society();
    w.roads().caravans.clear();
    w.roads().camps = {{"camp_mid", id(4), 6, 100, -100, true}};
    expect(s.openAccount("bandits:camp_mid") && s.create("bandits:camp_mid", "nails", 4, "raided cargo"), "a raided wagon's nails at the camp");
    auto light = items::allGoods();                 // (The lightest kinds: she still has to fight.)
    std::stable_sort(light.begin(), light.end(), [](const items::Item& a, const items::Item& b) { return a.weight < b.weight; });
    for (const auto& item : light)
        if (item.id != "nails" && s.account("player-ada")->stock.size() < MaxGoodsKinds)
            s.create("player-ada", item.id, 1, "test pack");
    const auto kinds = s.account("player-ada")->stock.size();
    expect(kinds == MaxGoodsKinds && !s.account("player-ada")->stock.count("nails"), "her pack holds every kind it can, and no nails: " + std::to_string(kinds));
    s.openRealBalance();                            // (The test's own goods: the gate measures play from here.)
    w.takeNotices();
    expect(adaBreaksTheCamp(w), "she breaks the camp");
    std::string told;
    for (const auto& [who, words] : w.takeNotices())
        if (who == "player-ada")
            told += words + "\n";
    const auto* mine = s.account("player-ada");
    expect(mine && Society::stock(*mine, "nails") == 0 && mine->stock.size() == kinds, "no nails in a pack that has no room for another kind");
    expect(!s.account("bandits:camp_mid"), "and none left at the camp: its account is closed");
    expect(told.find(" The cargo they took from the wagons is here too, and goes back to its carters.") != std::string::npos,
           "she is told it goes back: " + told);
    expect(s.realBalanced(), "real money is balanced");
}

// An escort's pay is a reward at the door as well (9.3), and its words and its event are what was paid (third repair).
void anEscortsPayWithinTheDay()
{
    const std::string dayPaid = "you have had all the rewards one day pays";
    enum { Free, Part, None };
    for (const int day : {Free, Part, None})
    {
        const std::string when = day == Part ? " (room for 5p)" : day == None ? " (no room)" : "";
        auto f = strip();
        auto w = load(f);
        w.addPlayer("player-ada", "Ada");
        w.tick(.6);
        expect(w.setTimeOfDay(8).ok, "a morning");
        auto& s = w.society();
        w.roads().camps.clear();
        w.roads().caravans.clear();
        const auto escort =
            w.postContract("escort", s.treasuryOf("west"), "west", "west", 12, 7, "guarding the next wagon from East to West", "east").id;
        auto* ada = w.entity("player-ada");
        ada->cellId = id(8);
        ada->position = {8.5, 8.5};
        expect(w.takeContract("player-ada", escort).ok, "Ada takes the escort at west's board" + when);
        ada->cellId = id(1);
        ada->position = {8.5, 10.5};
        const auto cap = s.statusTable().cap("rewardsADay");
        const std::int64_t room = day == Part ? 5 : 0;
        if (day != Free)
            expect(s.shift("treasury", "player-ada", "", 0, cap - room, "faction mission reward") && s.faucetRoom("player-ada", "rewardsADay") == room,
                   "her day's other rewards leave her so much room" + when);
        const auto purse = s.cash("player-ada");
        const auto* sent = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{"nails", 5}});
        expect(sent && sent->escorts == std::vector<std::string>{"player-ada"}, "the wagon counts her among its escorts" + when);
        const auto trip = sent->id;
        w.takeNotices();
        w.takeEvents();
        bool arrived = false;
        std::int64_t said = -1;
        for (int t = 0; t < 1500 && !arrived; ++t)
        {
            if (const auto* wagon = w.entity("road:" + trip))
            {
                auto* p = w.entity("player-ada");
                p->cellId = wagon->cellId;
                p->position = {std::max(.6, wagon->position.x - 1), wagon->position.y};
            }
            w.tick(1);
            for (const auto& e : w.takeEvents())
            {
                if (e.kind == "contract done" && e.detail.rfind("escort:", 0) == 0)
                    said = e.coins;
                arrived = arrived || (e.kind == "caravan arrives" && e.actor == trip);
            }
        }
        expect(arrived, "it reaches west with her" + when);
        std::string told;
        for (const auto& [who, words] : w.takeNotices())
            if (who == "player-ada")
                told += words + "\n";
        const auto paid = s.cash("player-ada") - purse;
        const auto tells = [&](const std::string& words) { return told.find(words) != std::string::npos; };
        const std::string got = std::to_string(paid) + ", the event " + std::to_string(said) + "; " + told;
        if (day == Free)
            expect(paid == 12 && said == 12 && tells("safely. You are paid 12 pennies for guarding it.") && !tells(dayPaid), "paid in full, and told so: " + got);
        else if (day == Part)
            expect(paid == 5 && said == 5 &&
                       tells("safely. You are paid 5 pennies of the 12 pennies for guarding it: " + dayPaid + ", and the rest goes back."),
                   "paid what the day has room for, and told that: " + got);
        else
            expect(paid == 0 && said == 0 && tells("safely. The pay for guarding it goes back: " + dayPaid + ".") && !tells("You are paid"),
                   "paid nothing past the day's cap, and not told she was: " + got);
        expect(!s.account("contract:" + escort), "the escort's pot is closed" + when);
        expect(s.realBalanced(), "real money is balanced" + when);
    }
}

// An escort from a save before escorts named their road (no `from`): it finds its road and keeps it (third repair). Taken
// at the board where a wagon to its town sets out, it is that road's; held by a player who stands in another market, it
// isn't taken along by this market's wagon, and is its own market's when that wagon goes; held by one who waits in the
// town itself or out on the road, it goes with the first wagon there, as it always did (which tells them where it
// waits); held by one who is away from the world, it is kept for them.
void anOlderEscortFindsItsRoad()
{
    const auto older = [](World& w) {
        return w.postContract("escort", w.society().treasuryOf("west"), "west", "west", 12, 7, "guarding the next wagon to West").id;
    };
    const auto of = [](const World& w, const std::string& k) {
        for (const auto& c : w.roads().contracts)
            if (c.id == k)
                return c;
        return Contract{};
    };
    const auto holds = [](World& w, const std::string& k, const std::string& who) {   // (As a save from before holds it.)
        for (auto& c : w.roads().contracts)
            if (c.id == k)
                c.status = "taken", c.taker = who;
    };
    const auto stands = [](World& w, const std::string& who, int cell) {
        auto* e = w.entity(who);
        expect(e != nullptr, who + " is in the world");
        e->cellId = id(cell);
        e->position = {8.5, 8.5};
    };
    const auto send = [](World& w, const char* from) {
        const auto* c = w.sendCaravan(*w.roadMarket(from), *w.roadMarket("west"), {{"nails", 5}});
        expect(c != nullptr, std::string("a wagon sets out from ") + from);
        return c->escorts;
    };
    const std::vector<std::string> nobody;
    const auto fresh = [](Fixture& f) {
        auto w = load(f);
        w.tick(.6);
        w.roads().camps.clear();
        w.roads().caravans.clear();
        return w;
    };
    {
        auto f = strip("EE..WW..NN");               // (West between east and north.)
        auto w = fresh(f);
        w.addPlayer("player-ada", "Ada");
        const auto k = older(w);
        expect(of(w, k).from.empty(), "an older save's escort names no road");
        w.society().stagedBoard().committed.push_back(market::Departure{"wolder", "wagon", "east", "west", ""});
        stands(w, "player-ada", 1);
        expect(w.takeContract("player-ada", k).ok && of(w, k).from == "east", "taken at east's board, it is east's road from then: " + of(w, k).from);
        expect(send(w, "north") == nobody, "north's wagon, out first, leaves without her");
        expect(send(w, "east") == std::vector<std::string>{"player-ada"}, "east's wagon waits for her");
    }
    {
        auto f = strip("EE..WW..NN");
        auto w = fresh(f);
        w.addPlayer("player-bo", "Bo");
        const auto k = older(w);
        holds(w, k, "player-bo");
        stands(w, "player-bo", 9);                  // (In north's market.)
        expect(send(w, "east") == nobody && of(w, k).status == "taken" && of(w, k).from.empty(),
               "east's wagon doesn't take along one who stands in north's market");
        expect(send(w, "north") == std::vector<std::string>{"player-bo"} && of(w, k).from == "north",
               "north's wagon waits for him, and the work is north's road from then: " + of(w, k).from);
        expect(send(w, "east") == nobody, "and east's next wagon leaves him be");
    }
    {
        auto f = strip("EE..WW..NN");
        auto w = fresh(f);
        w.addPlayer("player-cy", "Cy");
        const auto k = older(w);
        holds(w, k, "player-cy");
        stands(w, "player-cy", 5);                  // (In west's own market, where the wagons are going.)
        expect(send(w, "north") == std::vector<std::string>{"player-cy"} && of(w, k).from == "north",
               "one who waits in the town itself goes with the first wagon to it, as before");
    }
    {
        auto f = strip("EE..WW..NN");
        auto w = fresh(f);
        const auto k = older(w);
        holds(w, k, "player-dee");                  // (Nobody in the world: away.)
        expect(send(w, "east") == nobody && send(w, "north") == nobody && of(w, k).status == "taken" && of(w, k).taker == "player-dee" &&
                   of(w, k).from.empty(),
               "held by one who is away, it is kept for them: no wagon takes it");
    }
}

// Late, with a cause (8.1, 8.6): a seeded mishap holds the wagon; a day past due it is posted overdue, and its goods grow
// dearer at its end by 0.05 a day, at most 0.2.
void aLateWagon()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    const auto* sent = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{"nails", 10}});
    expect(sent, "a wagon");
    const auto trip = sent->id;
    w.roads().caravans.back().delayAt = 2;      // (A mishap on its second cell.)
    std::vector<WorldEvent> seen;
    expect(until(w, "wagon held up", trip, 1500, &seen), "it is held up on the road");
    const auto* c = caravan(w, trip);
    expect(c && !c->lateCause.empty(), "with a cause people talk about: " + (c ? c->lateCause : ""));
    const auto cause = c->lateCause;
    const double before = w.society().pressure("west", "nails");
    bool overdue = false;
    std::string words;
    for (int day = 0; day < 3 && !overdue; ++day)
    {
        nextMorning(w);
        for (const auto& e : w.takeEvents())
            if (e.kind == "wagon overdue" && e.actor == trip)
                overdue = true, words = e.detail;
    }
    expect(overdue && words.find(cause) != std::string::npos && words.rfind("Overdue", 0) == 0, "posted overdue, and why: " + words);
    const double after = w.society().pressure("west", "nails");
    expect(after > before + .04, "its goods dearer at its end: " + std::to_string(before) + " to " + std::to_string(after));
    for (int day = 0; day < 6; ++day)
    {
        nextMorning(w);
        if (const auto* now = caravan(w, trip); !now || now->status != "travelling")
            break;
    }
    expect(w.society().pressure("west", "nails") <= before + .2 + .01, "never more than 0.2 for it");
}

// Imports (D27, T14): a good west's shops set out that it doesn't make comes only on wagons; between them its shelf comes
// from the back room, a fifth less each morning, then goes thin; an arrival refills it. A market no wagon reaches refills
// everything each morning.
void importsThinBetweenWagons()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    // West's general store: what it sets out that east brings and west doesn't make.
    std::string ware;
    for (const auto& x : w.society().wares("wg"))
        if (w.society().imported("west", items::baseOf(x)))
            ware = x;
    expect(!ware.empty(), "west's general store has goods only the wagons bring");
    const auto till = w.society().tillOf("wg");
    const int target = w.society().shelfTarget(till, ware);
    // Sold out every day: the back room sets out what it has, then less, then nothing.
    std::vector<int> shelves;
    for (int day = 0; day < 8; ++day)
    {
        w.society().consume(till, ware, stock(w, "wg", ware), "test: sold out");
        nextMorning(w);
        expect(w.setTimeOfDay(11).ok, "to the late morning");
        run(w, 30);
        shelves.push_back(stock(w, "wg", ware));
        w.roads().caravans.clear();             // (No wagon comes in this while.)
        w.society().stagedBoard().committed.clear();
    }
    expect(shelves.front() > 0 && shelves.back() < target, "the shelf thins between wagons: first " + std::to_string(shelves.front()) +
                                                            ", last " + std::to_string(shelves.back()) + " of " + std::to_string(target));
    expect(std::is_sorted(shelves.rbegin(), shelves.rend()) || shelves.back() <= shelves.front(), "never fuller without a wagon");
    // A wagon brings it: the shelf at its target at once.
    const auto* sent = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{items::baseOf(ware), 12}});
    expect(sent, "a wagon with it");
    const auto trip = sent->id;
    expect(until(w, "caravan arrives", trip, 1500), "it arrives");
    expect(stock(w, "wg", ware) >= target, "and the shelf is full again");
    // A market no wagon reaches (no carters anywhere): everything refills each morning.
    auto lone = strip("EE.....WW", "", false);
    auto v = load(lone);
    v.tick(.6);
    expect(!v.society().imported("west", items::baseOf(ware)), "with no carters, nothing is only the wagons'");
    const auto vtill = v.society().tillOf("wg");
    v.society().consume(vtill, ware, stock(v, "wg", ware), "test: sold out");
    nextMorning(v);
    expect(v.setTimeOfDay(11).ok, "to the late morning");
    run(v, 30);
    expect(stock(v, "wg", ware) >= v.society().shelfTarget(vtill, ware), "and the shelf refills the next morning");
}

// Farm carts (8.2): a staffed farm's cart every morning, its carter a real hand at a real post, heavier on Marketday; on
// arrival its goods pulled halfway toward the harvest's baseline, only down, and the market's stalls stocked from it on
// Marketday. No staffed farm, no carts.
void farmCarts()
{
    // The keeper's label as the farm list has it (communities.json, farm_people.json): "farms Longmead".
    named = {{3, "fs_grain_long_granary"}};
    auto probe = strip("EE.G...WW");
    auto p = load(probe);
    std::string label;
    for (const auto& farm : p.society().farms())
        if (farm.def->id == "grain_farm_1")
            label = p.society().farmLabel(farm, 0);
    expect(!label.empty(), "the grain farm's keeper's label: " + label);
    // Without anyone at its posts: no cart.
    p.tick(.6);
    for (int day = 0; day < 2; ++day)
        nextMorning(p);
    for (const auto& d : p.society().board().committed)
        expect(d.kind != "cart", "no staffed farm, no carts");
    {
        std::map<std::string, std::string> facts;
        p.talkTown("e1", facts);
        expect(!facts.count("world.harvest"), "and no harvest talk where no farm's cart comes");
    }
    auto f = strip("EE.G...WW", resident("fk", "Bran Long", "civilian", label, 0, 9.5, 3, 9.5));
    auto w = load(f);
    w.tick(.6);
    expect(w.cartEnds().size() >= 1, "the farm's cart has its end");
    const auto end = std::find_if(w.cartEnds().begin(), w.cartEnds().end(), [](const auto& e) { return e.farmId == "grain_farm_1"; });
    expect(end != w.cartEnds().end() && end->market == "east", "its cart goes to its nearest market, east");
    // A good harvest (the DM's override): a cart every morning.
    w.society().stagedBoard().harvestOverride[end->area] = {2, w.calendarDays() + 60};
    expect(w.harvestOf(end->area) == 2, "a good harvest");
    w.society().setPressure("east", "flour", .2, "players");
    nextMorning(w);
    const market::Departure* cart = nullptr;
    for (const auto& d : w.society().board().committed)
        if (d.kind == "cart" && d.from == "grain_farm_1")
            cart = &d;
    expect(cart && cart->to == "east" && !cart->manifest.empty(), "the morning's cart is rolled");
    {
        // The harvest people talk of is the one that loads the carts (T11: `world.harvest`).
        std::map<std::string, std::string> facts;
        w.talkTown("e1", facts);
        expect(facts["world.harvest"] == "good" && cart->size == 3, "east hears of a good harvest, its cart a heavy one: " +
                                                                        facts["world.harvest"]);
    }
    for (const auto& [good, n] : cart->manifest)
        expect(w.society().tradePoints().find("east")->sold.count(good) || w.society().tradePoints().find("east")->bought.count(good),
               "carrying goods east's shops stock or buy: " + good);
    const auto d = *cart;
    const auto* out = setOut(w, d);
    expect(out && out->kind == "cart" && out->driver == "fk", "it sets out, driven by the farm's keeper");
    const auto trip = out->id;
    bool taking = false;
    std::vector<WorldEvent> came;
    for (int t = 0; t < 60 && !taking; ++t)
    {
        w.tick(1);
        for (auto& e : w.takeEvents())
            came.push_back(e);
        const auto* keeper = w.entity("fk");
        taking = keeper && keeper->activity.rfind("taking the cart to East", 0) == 0;
    }
    expect(taking, "who takes the cart to East, away from the farm: " + w.entity("fk")->activity);
    bool arrived = false;
    for (const auto& e : came)
        arrived |= e.kind == "cart arrives" && e.actor == trip;
    expect(arrived || until(w, "cart arrives", trip, 1500), "it comes in");
    bool flour = false;
    for (const auto& [good, n] : d.manifest)
        flour |= good == "flour";
    if (flour)
        expect(std::abs(w.society().pressure("east", "flour") - (.2 - .15) / 2) < .03,
               "flour pulled halfway toward a good harvest's baseline: " + std::to_string(w.society().pressure("east", "flour")));
    run(w, 5);
    expect(!caravan(w, trip) && !w.drivers().count("fk"), "one way: the cart is done and its keeper free");
    // Marketday: heavier, and the stalls stocked a little over from it.
    int guard = 0;
    while (calendar::weekdayOf(w.calendarDays() + 1) != calendar::Marketday && guard++ < 8)
        nextMorning(w);
    nextMorning(w);
    const market::Departure* marketCart = nullptr;
    for (const auto& x : w.society().board().committed)
        if (x.kind == "cart")
            marketCart = &x;
    expect(marketCart && marketCart->manifest.front().second > d.manifest.front().second, "heavier on Marketday");
    const auto md = *marketCart;
    const auto* mout = setOut(w, md);
    expect(mout, "the Marketday cart sets out");
    std::vector<WorldEvent> seen;
    expect(until(w, "cart arrives", mout->id, 1500, &seen), "and comes in");
    bool stalls = false;
    for (const auto& e : seen)
        stalls |= e.kind == "cart stocks the stalls";
    expect(stalls, "stocking the Marketday stalls");
    std::string ware;
    for (const auto& x : w.society().wares("em"))
        if (items::baseOf(x) == md.manifest.front().first)
            ware = x;
    if (!ware.empty())
        expect(stock(w, "em", ware) > w.society().shelfTarget(w.society().tillOf("em"), ware),
               "the stall's shelf of it over its everyday target");
    named.clear();
}

// An endpoint deleted while a wagon travels (16.5): a clean drop on load, its letters reopened at the origin (or back to
// the writer with the origin gone too), its escorts' pay back; and a contract whose market is gone refunds its poster.
void anEndpointDeleted()
{
    auto f = strip();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    w.roads().camps.clear();
    auto* ada = w.entity("player-ada");
    ada->cellId = id(8);
    ada->position = {8.5, 8.5};
    const std::string escort = w.postContract("escort", "treasury", "west", "west", 12, 7, "guarding the next wagon to west").id;
    expect(w.takeContract("player-ada", escort).ok, "Ada takes the escort");
    auto& letter = w.postContract("courier", "em", "west", "wm", 5, 30, "a letter from Ember to Wren");
    const std::string letterId = letter.id;
    letter.created -= 8;
    ada->cellId = id(1);
    ada->position = {8.5, 10.5};
    const auto* sent = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{"nails", 10}});
    expect(sent && sent->letters == std::vector<std::string>{letterId}, "a wagon with the letter");
    run(w, 20);
    const auto saved = w.save();
    const auto escortPay = w.society().cash("contract:" + escort);
    const auto treasury = w.society().cash("treasury");
    // West's merchants gone (a host's edit): west is no market.
    auto edited = f;
    std::string kept;
    std::istringstream lines(edited.manifest);
    for (std::string line; std::getline(lines, line);)
        if (line.find("\"wm\"") == std::string::npos && line.find("\"wg\"") == std::string::npos)
            kept += line + "\n";
    edited.manifest = kept;
    auto v = load(edited);
    const auto restored = v.restore(saved);
    expect(restored.ok, "the save loads into the edited world: " + restored.message);
    expect(!v.roadMarket("west") && v.roadMarket("east"), "west is no market now");
    expect(v.roads().caravans.empty() && !v.entity("road:" + sent->id), "the wagon is dropped on load");
    for (const auto& k : v.roads().contracts)
    {
        if (k.id == letterId)
            expect(k.status == "open" && k.taker.empty() && v.society().cash("contract:" + k.id) == 5,
                   "its letter open again at its origin, its 5p in escrow: " + k.status);
        if (k.id == escort)
            expect(k.status == "expired" && !v.society().account("contract:" + k.id), "the escort to a market that's gone ended");
    }
    expect(v.society().realBalanced(), "real money is balanced");
    (void)escortPay;
    (void)treasury;
    // With both ends gone, the letter goes back to its writer.
    auto bare = f;
    std::string keptBoth;
    std::istringstream all(bare.manifest);
    for (std::string line; std::getline(all, line);)
    {
        // Every merchant gone (no market anywhere), Ember, the letter's writer, still here, out of work.
        if (line.find("\"em\"") != std::string::npos)
            line.replace(line.find("\"merchant\" \"keeping the stall\""), std::string("\"merchant\" \"keeping the stall\"").size(),
                         "\"civilian\" \"working\"");
        if (line.find("\"merchant\"") == std::string::npos)
            keptBoth += line + "\n";
    }
    bare.manifest = keptBoth;
    auto u = load(bare);
    const auto writer = saved.society.accounts.count("em") ? saved.society.accounts.at("em").cash : 0;
    expect(u.restore(saved).ok, "the save loads into a world with no markets left");
    expect(u.roads().caravans.empty(), "the wagon dropped");
    expect(u.roadMarkets().empty(), "no market left");
    bool refunded = false;
    for (const auto& k : u.roads().contracts)
        if (k.id == letterId)
            refunded = k.status == "expired" && !u.society().account("contract:" + k.id);
    expect(refunded, "its letter refunded to its writer");
    (void)writer;
    expect(u.society().realBalanced(), "real money is balanced");
}

// No port, no ships (Phase 5 builds them): nothing on the Board is a ship's; and the committed departures are bounded.
void noPortNoShips()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    for (int day = 0; day < 6; ++day)
        nextMorning(w);
    for (const auto& d : w.society().board().committed)
        expect(d.kind == "wagon" || d.kind == "cart", "no ships without a port");
    expect(w.society().board().committed.size() <= market::CommittedMost, "the committed departures bounded");
    // Saved and read back the same.
    const auto again = market::decode(market::encode(w.society().board()));
    expect(again.committed.size() == w.society().board().committed.size() && again.rolledThrough == w.society().board().rolledThrough &&
               again.nextDeparture == w.society().board().nextDeparture,
           "the committed departures round-trip");
    for (std::size_t i = 0; i < again.committed.size(); ++i)
        expect(again.committed[i].manifest == w.society().board().committed[i].manifest && again.committed[i].day == w.society().board().committed[i].day,
               "each with its manifest, in order, and its day");
}
// No carter to be had at its hour (8.1: only an announced cause changes a posted wagon): set back a day, said why, and
// given up after a few days.
void noCarterSetsItBack()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    const auto d = postedWagon(w);
    for (const auto* who : {"c1", "c2"})
        w.entity(who)->dead = true;
    std::vector<WorldEvent> seen;
    while (std::floor(w.calendarDays()) < double(d.day))
        nextMorning(w);
    expect(w.setTimeOfDay(std::min(23.5, d.hour + .05)).ok, "to its hour");
    for (int t = 0; t < 8; ++t)
        w.tick(.5);
    for (auto& e : w.takeEvents())
        seen.push_back(e);
    bool delayed = false;
    for (const auto& e : seen)
        delayed |= e.kind == "wagon delayed" && e.actor == d.id && e.detail.find("Set back: no carter to be had.") != std::string::npos;
    expect(delayed, "set back, with its cause on the notice");
    const market::Departure* again = nullptr;
    for (const auto& x : w.society().board().committed)
        if (x.id == d.id)
            again = &x;
    expect(again && again->day == d.day + 1 && again->cause == "no carter to be had", "to the next day, its cause kept");
    bool cancelled = false;
    for (int day = 0; day < 8 && !cancelled; ++day)
    {
        nextMorning(w);
        expect(w.setTimeOfDay(12).ok, "to midday");
        for (int t = 0; t < 4; ++t)
            w.tick(.5);
        for (const auto& e : w.takeEvents())
            cancelled |= e.kind == "wagon cancelled" && e.actor == d.id;
    }
    expect(cancelled, "and given up after a few days");
    for (const auto& c : w.roads().caravans)
        expect(c.departure != d.id, "it never set out without a carter");
}

// Bandits (D14): camps gather mildly more in hard times, never past the cap in strength, and a bounty never past its cap.
void banditsScaleMildly()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    double odds[3];
    int i = 0;
    for (const int slider : {-100, 0, 100})
    {
        w.society().setProsperity(slider, true);
        odds[i++] = w.campGatherOdds();
    }
    expect(odds[0] > odds[1] && odds[1] > odds[2] && odds[0] <= .34 * 1.25 * 1.1 + 1e-9 && odds[2] >= .34 * .8 - 1e-9,
           "camps gather mildly more in hard times, fewer in a boom: " + std::to_string(odds[0]) + " " + std::to_string(odds[2]));
    // A road's far market in hard times of its own (raids, late wagons: its Times, 5.3) gathers camps a little more, still
    // within the cap.
    w.society().setProsperity(0, true);
    const double realm = w.campGatherOdds();
    w.society().stagedBoard().timesShift["west"] = -30;
    const double hard = w.campGatherOdds("west");
    expect(hard > realm && hard <= .34 * 1.25 * 1.1 + 1e-9, "the far market's own hard times count, mildly: " + std::to_string(realm) +
                                                             " to " + std::to_string(hard));
    w.roads().camps = {{"camp_big", id(4), 20, 50, -100, true}};
    nextMorning(w);
    expect(w.roads().camps[0].strength <= w.society().rule("campStrengthMost"), "a camp's strength capped by the world's rule");
}

// The carters' notices on the boards (8.1, 8.6), through the Game: pinned at both ends when posted, by "the carters", with
// the calendar's weekday names; taken down when the wagon comes in.
void noticesPostAndComeDown()
{
    // The strip as a world on disk (cells, seams, the manifest), for a game to start from.
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("ratw-s2stage-notices-" + std::to_string(::getpid()));
    fs::remove_all(root);
    fs::create_directories(root / "cells");
    fs::create_directories(root / "seams");
    auto f = strip();
    for (const auto& [cellId, text] : f.cells)
    {
        std::ofstream(root / "cells" / (cellId + ".cell")) << text;
        std::ofstream(root / "seams" / cellId) << f.seams[cellId];
    }
    std::ofstream(root / "world.ratw") << f.manifest;
    game::Options o;
    o.forkSnapshots = false;
    o.tiesOptional = true;
    o.worldExport = root.string();
    game::Game g(o);
    std::string problem;
    expect(g.start(problem), "a game starts on the strip: " + problem);
    auto& w = g.world();
    w.tick(.6);
    expect(w.roadMarket("east") && w.roadMarket("west"), "with its two markets");
    w.tick(.6);
    w.roads().camps.clear();
    const auto d = postedWagon(w);
    const auto weekday = calendar::weekdayName(calendar::weekdayOf(double(d.day) + d.hour / 24));
    const auto* atEast = g.documents().find("wagon-" + d.id + "-from");
    const auto* atWest = g.documents().find("wagon-" + d.id + "-to");
    expect(atEast && atEast->board == "board:east" && atEast->kind == "notice" && atEast->sign == "the carters" &&
               atEast->text.find(weekday) != std::string::npos && atEast->expires > w.calendarDays(),
           "pinned on east's board by the carters, naming its day: " + (atEast ? atEast->text : std::string("none")));
    expect(atWest && atWest->board == "board:west" && atWest->text.rfind("Wagon from East, due " + weekday, 0) == 0,
           "and on west's: " + (atWest ? atWest->text : std::string("none")));
    w.roads().camps.clear();
    const auto* out = setOut(w, d);
    expect(out, "it sets out");
    const auto trip = out->id;
    expect(until(w, "caravan arrives", trip, 1500), "it comes in");
    expect(!g.documents().find("wagon-" + d.id + "-from") && !g.documents().find("wagon-" + d.id + "-to"), "and its notices come down");
    // A wagon that will never come (a host closed its road while it travelled): lost, its notices down too, its escort's
    // work open again for the next wagon.
    w.roads().caravans.clear();
    w.society().stagedBoard().committed.clear();
    w.roads().camps.clear();
    const auto d2 = postedWagon(w);
    const auto* out2 = setOut(w, d2);
    expect(out2 && (g.documents().find("wagon-" + d2.id + "-from") || g.documents().find("wagon-" + d2.id + "-to")),
           "a second wagon out, its notices up");
    const auto trip2 = out2->id;
    w.addPlayer("player-ada", "Ada");
    const auto escort = w.postContract("escort", w.society().treasuryOf("west"), "west", "west", 12, 7, "guarding the next wagon to West").id;
    for (auto& k : w.roads().contracts)
        if (k.id == escort)
            k.status = "taken", k.taker = "player-ada";
    for (auto& c : w.roads().caravans)
        if (c.departure == d2.id)
        {
            c.escorts = {"player-ada"};
            c.endCell = "no_such_place";            // (Its road gone: no way on.)
            c.waitUntil = 0;
        }
    bool lost = false;
    for (int t = 0; t < 120 && !lost; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
            lost = lost || (e.kind == "caravan lost" && e.item == d2.id);
    }
    expect(lost, "lost on the road, naming its departure");
    expect(!g.documents().find("wagon-" + d2.id + "-from") && !g.documents().find("wagon-" + d2.id + "-to"), "its notices come down");
    for (const auto& k : w.roads().contracts)
        if (k.id == escort)
            expect(k.status == "open" && k.taker.empty(), "and Ada's escort open again for the next wagon: " + k.status);
    expect(!w.drivers().count(d2.driver) || w.drivers().at(d2.driver) != trip2, "its carter free");
    fs::remove_all(root);
}

// The source's player-sold glut goes out first (8.1, T3): a good its shops only buy (ore at a smithy, berries at a stall),
// dumped there by players, is on the next wagon to a market that sets it out or buys it, and halved as it loads.
void aGlutRidesTheNextWagon()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    std::string good;
    {
        const auto* east = w.society().tradePoints().find("east");
        const auto* west = w.society().tradePoints().find("west");
        expect(east && west, "two markets");
        for (const auto& g : east->bought)
            if (good.empty() && !east->sold.count(g) && !east->made.count(g) && !east->crafted.count(g) &&
                (west->sold.count(g) || west->bought.count(g)))
                good = g;
    }
    expect(!good.empty(), "a good east's shops buy but don't make or sell, that west wants");
    w.society().setPressure("east", good, -.4, "dump");
    const auto d = postedWagon(w);
    expect(std::any_of(d.manifest.begin(), d.manifest.end(), [&](const auto& m) { return m.first == good; }),
           "the next wagon out carries the glut of " + good);
    expect(d.manifest.size() <= 5, "still at most five goods");
    const auto* out = setOut(w, d);
    expect(out && out->sourceP.count(good) && out->sourceP.at(good) < -.1, "it sets out with the glut as it was");
    expect(std::abs(w.society().pressure("east", good) - out->sourceP.at(good) / 2) < .02,
           "the glut halved as it loads: " + std::to_string(w.society().pressure("east", good)));
}

// Every import comes in rotation (T14, D27): with all of west's imports bare (players buying out the shelves and back
// rooms each day), each wagon takes those brought longest ago, so none waits more than a couple of wagons, and the
// imports from one source are no more than its wagons bring in rotation.
void everyImportComesInRotation()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    std::set<std::string> imports;
    for (const auto* keeper : {"wg", "wm"})
        for (const auto& ware : w.society().wares(keeper))
            if (w.society().imported("west", items::baseOf(ware)))
                imports.insert(items::baseOf(ware));
    expect(!imports.empty() && imports.size() <= 10, "west's imports from east, at most a rotation's: " + std::to_string(imports.size()));
    const auto drain = [&] {
        for (const auto* keeper : {"wg", "wm"})
        {
            const auto till = w.society().tillOf(keeper);
            for (const auto& ware : w.society().wares(keeper))
                if (const auto base = items::baseOf(ware); w.society().imported("west", base))
                {
                    w.society().consume(till, ware, stock(w, keeper, ware), "test: bought out");
                    w.society().stagedBoard().backRoom[till][base] = 0;
                }
        }
    };
    std::map<std::string, int> waited;
    int longest = 0, wagons = 0;
    for (; wagons < 10; ++wagons)
    {
        drain();
        const auto d = postedWagon(w);
        std::set<std::string> on;
        for (const auto& [g, n] : d.manifest)
            on.insert(g);
        // (It comes in: the test's wagons never walk, their goods refill as an arrival's do.)
        for (const auto& g : on)
            w.society().arrivalRefill("west", g);
        w.society().stagedBoard().committed.clear();
        for (const auto& g : imports)
        {
            waited[g] = on.count(g) ? 0 : waited[g] + 1;
            longest = std::max(longest, waited[g]);
        }
    }
    expect(longest <= 2, "every import is on one of every three wagons at least: the longest wait " + std::to_string(longest) +
                             " wagons, of " + std::to_string(imports.size()) + " imports");
    // When each came is kept with the Board, and read back.
    const auto again = market::decode(market::encode(w.society().board()));
    expect(again.broughtIn.count("west") && again.broughtIn.at("west") == w.society().board().broughtIn.at("west"),
           "when each import last came round-trips");
}

// The DM's rules that stop the wagons (caravanRate 0, wagonsOnRoad 0) or the carts (cartsOnRoad 0): nothing then
// brings the imports, so they refill each morning (D27, 5.1), never bare for good.
void rulesThatStopTheRoads()
{
    for (const auto* rule : {"caravanRate", "wagonsOnRoad"})
    {
        auto f = strip();
        auto w = load(f);
        w.tick(.6);
        std::string ware;
        for (const auto& x : w.society().wares("wg"))
            if (w.society().imported("west", items::baseOf(x)))
                ware = x;
        expect(!ware.empty(), std::string(rule) + ": west waits for wagons for some goods");
        auto spec = w.society().authored().economy;
        spec.rules[rule] = 0;
        w.society().adoptEconomy(spec);
        const auto till = w.society().tillOf("wg");
        for (int day = 0; day < 3; ++day)
        {
            w.society().consume(till, ware, stock(w, "wg", ware), "test: sold out");
            w.society().stagedBoard().backRoom[till][items::baseOf(ware)] = 0;
            nextMorning(w);
            expect(w.setTimeOfDay(11).ok, "to the late morning");
            run(w, 30);
            expect(!w.society().imported("west", items::baseOf(ware)), std::string(rule) + " 0: nothing west waits for a wagon for");
            expect(stock(w, "wg", ware) >= w.society().shelfTarget(till, ware),
                   std::string(rule) + " 0: the shelf refills the next morning, day " + std::to_string(day));
        }
        spec.rules[rule] = rule == std::string("caravanRate") ? 100 : 16;
        w.society().adoptEconomy(spec);
        nextMorning(w);
        expect(w.society().imported("west", items::baseOf(ware)), std::string(rule) + " back: the wagons bring it again");
    }
    // The carts: a good only a farm's cart brings east is east's own each morning while cartsOnRoad is 0.
    named = {{3, "fs_grain_long_granary"}};
    auto probe = strip("EE.G...WW");
    auto p = load(probe);
    std::string label;
    for (const auto& farm : p.society().farms())
        if (farm.def->id == "grain_farm_1")
            label = p.society().farmLabel(farm, 0);
    auto f = strip("EE.G...WW", resident("fk", "Bran Long", "civilian", label, 0, 9.5, 3, 9.5));
    auto w = load(f);
    w.tick(.6);
    nextMorning(w);                                 // (Its keeper in his post.)
    std::string carted;
    for (const auto& farm : w.society().farms())
        if (farm.def->id == "grain_farm_1")
            for (const auto& g : farm.def->yields)
                if (carted.empty() && w.society().imported("east", g))
                    carted = g;
    expect(!carted.empty(), "east waits for the grain farm's cart for some good");
    auto spec = w.society().authored().economy;
    spec.rules["cartsOnRoad"] = 0;
    w.society().adoptEconomy(spec);
    nextMorning(w);
    expect(!w.society().imported("east", carted), "cartsOnRoad 0: " + carted + " refills each morning");
    for (const auto& d : w.society().board().committed)
        expect(d.kind != "cart", "and no cart comes");
    named.clear();
}

// A market plain for law (D7) still has its board (a world of two law towns, east and west, and the hamlet north between):
// a player at north's board reads the carters' notice of the wagon coming to it; the carters' pins never take one a
// player needs; the escort is taken at the board where the wagon sets out, and the far end's notice says so.
void aPlainMarketsBoard()
{
    auto f = strip("EE..NN..WW");
    const auto root = onDisk(f, "plain");
    game::Game g(optionsOn(root));
    std::string problem;
    expect(g.start(problem), "a game starts on the strip: " + problem);
    auto& w = g.world();
    w.tick(.6);
    w.tick(.6);
    expect(w.roadMarket("north") && w.lawTown(id(5)).empty() && w.lawTown(id(1)) == "east" && w.lawTown(id(8)) == "west",
           "two law towns, and north a market plain for law");
    w.roads().camps.clear();
    market::Departure d;
    bool found = false;
    for (int day = 0; day < 40 && !found; ++day)
    {
        for (const auto& x : w.society().board().committed)
            if (!found && x.kind == "wagon" && x.from == "east" && x.to == "north")
                d = x, found = true;
        if (!found)
            nextMorning(w);
    }
    expect(found, "a wagon from east to north is posted");
    const auto* pinned = g.documents().find("wagon-" + d.id + "-to");
    expect(pinned && pinned->board == "board:north", "its notice pinned on north's board");
    Client ash;
    enterAt(g, ash, 71, "ash", "north");
    board(g, ash, {{"verb", "read"}});
    const auto* view = ash.last("board");
    bool read = false;
    for (const auto& n : view ? view->array("notices") : std::vector<json::Value>{})
        read = read || n.string("text").rfind("Wagon from East, due ", 0) == 0;
    expect(read, "a player at north's board reads it: " + (view ? json::dump(*view) : ash.said()));
    // The carters' notices fill the board past its thirty pins; a player still pins hers.
    for (int i = 0; i < 32; ++i)
        w.recordEvent({"wagon posted", "wfill" + std::to_string(i), "north", id(1), 0, 0, "east", 1, 0,
                       "Wagon to North, Dawnday: nails.\nWagon from East, due Dawnday: nails."});
    expect(g.documents().onBoard("board:north").size() >= 32, "the carters' notices fill north's board");
    const auto said = board(g, ash, {{"verb", "post"}, {"kind", "other"}, {"text", "Seeking company on the road west."}});
    expect(said.find("You pin your notice") != std::string::npos, "and a player still pins hers: " + said);
    // The escort: asked for by north, taken at east's board while a wagon from there is posted, and the far end's notice
    // sends a reader there.
    w.postContract("escort", w.society().treasuryOf("north"), "north", "north", 12, 7, "guarding the next wagon to North");
    Client bo;
    enterAt(g, bo, 72, "bo", "east");
    bool atSource = false;
    for (const auto* k : w.contractsNear(bo.entityId))
        atSource = atSource || (k->kind == "escort" && k->town == "north");
    bool committed = false;
    for (const auto& x : w.society().board().committed)
        committed = committed || (x.kind == "wagon" && x.from == "east" && x.to == "north");
    expect(!committed || atSource, "the escort is taken at east's board, where the wagon sets out");
    auto far = d;
    const auto atEnd = w.departureNotice(far, true), atStart = w.departureNotice(far, false);
    expect(atEnd.find("Escort wanted: ask at the board in East.") != std::string::npos && atStart.find("Escort wanted.") != std::string::npos,
           "the far end's notice says where the escort is taken: " + atEnd);
    std::filesystem::remove_all(root);
}

// The words on the notices, the news and the talk (8.1): plain goods ("nails", never "nails (bag of 50)"), "nails are" and
// "cheese is", "the general store".
void plainWords()
{
    expect(World::goodWords("nails") == "nails" && World::goodWords("cheese") == "cheese" && World::goodWords("herbs") == "cooking herbs",
           "goods in plain words: " + World::goodWords("nails") + ", " + World::goodWords("cheese"));
    expect(World::pluralGood("nails") && World::pluralGood("eggs") && !World::pluralGood("cheese") && !World::pluralGood("canvas") &&
               !World::pluralGood("citrus") && !World::pluralGood("glass") && !World::pluralGood("leather cap with ear slits"),
           "which take \"are\"");
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    for (int i = 0; i < 6; ++i)
    {
        const auto d = postedWagon(w);
        for (const bool atEnd : {false, true})
        {
            const auto text = w.departureNotice(d, atEnd);
            expect(text.find('(') == std::string::npos && text.find("goods store") == std::string::npos, "a notice in plain words: " + text);
            for (const auto& [g, n] : d.manifest)
                expect(text.find(World::goodWords(g)) != std::string::npos, "naming each good: " + text);
        }
        w.society().stagedBoard().committed.clear();
        nextMorning(w);
    }
}

// Overdue with no mishap and no storm on its way: nobody knows why ("No word of it yet"), never "held up by no word of it".
void overdueWithNoWord()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    const auto* sent = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{"nails", 10}});
    expect(sent, "a wagon");
    const auto trip = sent->id;
    w.roads().caravans.back().due = w.calendarDays() + .01;   // (Due at once; nothing on the road holds it, nobody sees it.)
    std::string words;
    for (int day = 0; day < 3 && words.empty(); ++day)
    {
        nextMorning(w);
        for (const auto& e : w.takeEvents())
            if (e.kind == "wagon overdue" && e.actor == trip)
                words = e.detail;
    }
    expect(words == "Overdue: the wagon from East. No word of it yet.", "posted overdue with no cause known: " + words);
}

// Farm carts that don't come (8.2): none for want of room on the roads (cartsOnRoad) is said of no harvest and bumps
// nothing; one nobody at the farm is free to take is missed with its cause; and two farms' missed carts of a good share
// one bump at their market, +0.05 in all, not +0.05 each.
void cartsThatDontCome()
{
    named = {{3, "fs_grain_long_granary"}, {4, "fs_grain_lark_granary"}};
    auto probe = strip("EE.GH...WW");
    auto p = load(probe);
    std::string one, two;
    for (const auto& farm : p.society().farms())
    {
        if (farm.def->id == "grain_farm_1")
            one = p.society().farmLabel(farm, 0);
        if (farm.def->id == "grain_farm_2")
            two = p.society().farmLabel(farm, 0);
    }
    expect(!one.empty() && !two.empty(), "the two grain farms' keepers' labels");
    auto f = strip("EE.GH...WW", resident("fk", "Bran Long", "civilian", one, 0, 9.5, 3, 9.5) +
                                     resident("fk2", "Ada Lark", "civilian", two, 0, 10.5, 4, 9.5));
    auto w = load(f);
    w.tick(.6);
    std::set<std::string> areas;
    int toEast = 0;
    for (const auto& e : w.cartEnds())
        if (e.market == "east")
            ++toEast, areas.insert(e.area);
    expect(toEast == 2, "both farms' carts go to east");
    for (const auto& area : areas)
        w.society().stagedBoard().harvestOverride[area] = {2, w.calendarDays() + 60};   // (Good: every cart rolled comes.)
    std::set<std::string> goods;
    for (const auto& farm : w.society().farms())
        if (farm.def->id == "grain_farm_1")
            for (const auto& g : farm.def->yields)
                for (const auto& other : w.society().farms())
                    if (other.def->id == "grain_farm_2" &&
                        std::find(other.def->yields.begin(), other.def->yields.end(), g) != other.def->yields.end() &&
                        (w.society().tradePoints().find("east")->sold.count(g) || w.society().tradePoints().find("east")->bought.count(g)))
                        goods.insert(g);
    expect(!goods.empty(), "goods both farms send east");
    // No room on the roads: no cart, no bump, nothing said of the harvest.
    auto spec = w.society().authored().economy;
    spec.rules["cartsOnRoad"] = 0;
    w.society().adoptEconomy(spec);
    for (const auto& g : goods)
        w.society().setPressure("east", g, 0, "players");
    nextMorning(w);
    for (const auto& d : w.society().board().committed)
        expect(d.kind != "cart", "no room on the roads: no cart");
    for (const auto& g : goods)
        expect(w.society().pressure("east", g) < .01, "and no bump for " + g + ": " + std::to_string(w.society().pressure("east", g)));
    // Room again, but nobody at either farm to take the cart: each missed, with its cause; one shared bump.
    spec.rules["cartsOnRoad"] = 48;
    w.society().adoptEconomy(spec);
    w.entity("fk")->dead = true;
    w.entity("fk2")->dead = true;
    nextMorning(w);
    int rolled = 0;
    for (const auto& d : w.society().board().committed)
        rolled += d.kind == "cart";
    expect(rolled == 2, "both carts rolled: " + std::to_string(rolled));
    std::map<std::string, double> before;
    for (const auto& g : goods)
        before[g] = w.society().pressure("east", g);
    w.takeEvents();
    expect(w.setTimeOfDay(8).ok, "to the morning, past the carts' hours");
    for (int t = 0; t < 4; ++t)
        w.tick(.5);
    int missed = 0;
    for (const auto& e : w.takeEvents())
        missed += e.kind == "cart missed" && e.detail.rfind("no hand to spare at ", 0) == 0;
    expect(missed == 2, "each cart missed, nobody free to take it: " + std::to_string(missed));
    for (const auto& g : goods)
    {
        const double bump = w.society().pressure("east", g) - before[g];
        expect(bump > .04 && bump < .056, "one shared bump of 0.05 for " + g + ", not one a farm: " + std::to_string(bump));
    }
    named.clear();
}

// ---- Crashes between checkpoints (the journal). Each case plays to its crash on a file save (a checkpoint, then
// something the world's tick does on the roads, then a player's trade journalled, then the game gone without saving),
// and is checked after the restart: the Board, the accounts and the roads are journalled together, so the replay has
// each thing once. The same cases go through Postgres (Tests/trade_show_pg.py: --write-crashes, --check-crashes).
struct CrashCase
{
    const char* name;
    std::function<json::Value(game::Game&)> before;                 // To the crash: what was so then.
    std::function<void(game::Game&, const json::Value&)> after;     // After the restart.
    // At the crash itself (optional): what its window's seconds may have moved since `before` (a robbed wagon home, a
    // price eased by the clock).
    std::function<void(game::Game&, json::Value&)> atCrash = {};
};

int bountiesOn(const World& w, const std::string& camp)
{
    int n = 0;
    for (const auto& k : w.roads().contracts)
        n += k.kind == "bounty" && k.target == camp && (k.status == "open" || k.status == "taken");
    return n;
}
int lootAt(const World& w, const std::string& camp)
{
    int n = 0;
    if (const auto* hoard = w.society().account("bandits:" + camp))
        for (const auto& [item, pieces] : hoard->stock)
            n += pieces;
    return n;
}
void strongCamps(World& w)
{
    auto spec = w.society().authored().economy;
    spec.rules["campStrengthMost"] = 20;
    w.society().adoptEconomy(spec);
}
std::string slurp(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    expect(bool(in), "reads " + file.string());
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
// A price's pressure either side of a crash: it eases by the clock (market::decayed), and the clock restarts on the last
// record's moment, a few ticks short of the crash's.
bool samePrice(double a, double b) { return std::abs(a - b) < 1e-4; }

const std::vector<CrashCase>& crashCases()
{
    static const std::vector<CrashCase> cases{
        // A wagon set out after the checkpoint: on the road exactly once, its departure gone from the Board.
        {"setsout",
         [](game::Game& g) {
             auto& w = g.world();
             w.tick(.6);
             w.tick(.6);
             w.roads().camps.clear();
             const auto d = postedWagon(w);
             while (std::floor(w.calendarDays()) < double(d.day))
                 nextMorning(w);
             g.save();
             g.settle();                            // (The checkpoint, on its day: the wagon posted, not yet out.)
             expect(setOut(w, d), "it sets out after the checkpoint");
             auto was = json::Value::object();
             was.add("departure", d.id);
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             const auto departure = was.string("departure");
             int onRoad = 0;
             for (const auto& c : g.world().roads().caravans)
                 onRoad += c.departure == departure;
             bool posted = false;
             for (const auto& x : g.world().society().board().committed)
                 posted = posted || x.id == departure;
             expect(onRoad == 1 && !posted, "the wagon on the road once, its departure gone from the Board: " + std::to_string(onRoad) +
                                                (posted ? ", still posted" : ""));
         }},
        // A wagon came in after the checkpoint (its goods cheaper at its end, 8.6): it doesn't come in twice.
        {"arrive",
         [](game::Game& g) {
             auto& w = g.world();
             w.tick(.6);
             w.tick(.6);
             w.roads().camps.clear();
             const auto* sent = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{"nails", 10}});
             expect(sent, "a wagon with nails");
             const auto trip = sent->id;
             w.society().setPressure("west", "nails", .25, "players");
             const double dear = w.society().pressure("west", "nails");
             g.save();
             g.settle();                            // (The checkpoint: the wagon on the road.)
             expect(until(w, "caravan arrives", trip, 1500), "it arrives after the checkpoint");
             const double p = w.society().pressure("west", "nails");
             expect(p < dear - .03, "its nails cheaper in west as it comes in: " + std::to_string(dear) + " to " + std::to_string(p));
             auto was = json::Value::object();
             was.add("trip", trip);
             was.add("p", p);
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             auto& w = g.world();
             const auto trip = was.string("trip");
             const auto* c = caravan(w, trip);
             expect(!c || c->status != "travelling", "after the crash the wagon that came in isn't on its way again");
             expect(samePrice(w.society().pressure("west", "nails"), was.number("atCrash")),
                    "west's price as the arrival left it: " + std::to_string(w.society().pressure("west", "nails")) + " for " +
                        std::to_string(was.number("atCrash")));
             expect(!until(w, "caravan arrives", trip, 1500), "and it doesn't come in a second time");
         }},
        // A wagon robbed after the checkpoint (+0.15 at its end, its cargo at the camp, a bounty): robbed once.
        {"raid",
         [](game::Game& g) {
             auto& w = g.world();
             w.tick(.6);
             w.tick(.6);
             strongCamps(w);
             std::string trip;
             double before = 0;
             for (int attempt = 0; attempt < 10 && trip.empty(); ++attempt)
             {
                 w.roads().caravans.clear();
                 w.roads().camps = {{"camp_mid", id(4), 20, 100, -100, true}};
                 const auto* out = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{"nails", 10}});
                 expect(out, "a wagon sets out");
                 const auto mine = out->id;
                 w.roads().caravans.back().guards = 0;
                 before = w.society().pressure("west", "nails");
                 g.save();
                 g.settle();                        // (The checkpoint: the wagon on the road, the camp ahead of it.)
                 bool passed = false;
                 for (int t = 0; t < 1500 && trip.empty() && !passed; ++t)
                 {
                     w.tick(1);
                     for (const auto& e : w.takeEvents())
                     {
                         if (e.kind == "raid" && e.actor == "camp_mid")
                             trip = mine;
                         passed |= e.kind == "caravan passes" && e.actor == mine;
                     }
                 }
             }
             const auto* c = trip.empty() ? nullptr : caravan(w, trip);
             expect(c && c->status == "raided", "the bandits fall on a wagon after the checkpoint");
             const double p = w.society().pressure("west", "nails");
             expect(std::abs(p - before - .15) < .02, "its nails dearer in west at the raid");
             expect(bountiesOn(w, "camp_mid") == 1 && lootAt(w, "camp_mid") > 0, "one bounty on the camp, the cargo in it");
             auto was = json::Value::object();
             was.add("trip", trip);
             was.add("p", p);
             was.add("strength", w.roads().camps[0].strength);
             was.add("loot", lootAt(w, "camp_mid"));
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             auto& w = g.world();
             strongCamps(w);
             const auto trip = was.string("trip");
             const auto* c = caravan(w, trip);
             if (was.boolean("home"))               // (It got home in its window's seconds, before the crash.)
                 expect(!c, "after the crash the robbed wagon that was home isn't on the road again: " + (c ? c->status : std::string()));
             else
                 expect(c && c->status == "raided" && c->manifest.empty(),
                        "after the crash the robbed wagon is still robbed, not on its way again" + (c ? ": " + c->status : std::string()));
             expect(w.roads().camps.size() == 1 && near(w.roads().camps[0].strength, was.number("strength")),
                    "the camp as strong as the raid left it");
             expect(bountiesOn(w, "camp_mid") == 1, "one bounty on the camp: " + std::to_string(bountiesOn(w, "camp_mid")));
             expect(samePrice(w.society().pressure("west", "nails"), was.number("atCrash")),
                    "west's price as the raid left it: " + std::to_string(w.society().pressure("west", "nails")) + " for " +
                        std::to_string(was.number("atCrash")));
             expect(lootAt(w, "camp_mid") == int(was.number("loot")), "the cargo at the camp, once");
             // On home: nothing falls on it again.
             bool again = false;
             for (int t = 0; t < 1500 && caravan(w, trip); ++t)
             {
                 w.tick(1);
                 for (const auto& e : w.takeEvents())
                     again |= e.kind == "raid" && e.actor == "camp_mid";
             }
             expect(!again && w.society().pressure("west", "nails") <= was.number("atCrash") + 1e-4 && bountiesOn(w, "camp_mid") == 1 &&
                        lootAt(w, "camp_mid") == int(was.number("loot")),
                    "and it is robbed once: no second raid, rise, bounty or cargo");
         },
         [](game::Game& g, json::Value& was) {
             auto& w = g.world();
             expect(w.roads().camps.size() == 1, "the camp is there at the crash");
             was.set("strength", w.roads().camps[0].strength);
             was.set("loot", lootAt(w, "camp_mid"));
             was.set("home", caravan(w, was.string("trip")) == nullptr);
         }},
        // A wagon posted overdue the morning after the checkpoint (+0.05 at its end): that morning's rise comes once.
        {"late",
         [](game::Game& g) {
             auto& w = g.world();
             w.tick(.6);
             w.tick(.6);
             w.roads().camps.clear();
             const auto* sent = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{"nails", 10}});
             expect(sent, "a wagon with nails");
             const auto trip = sent->id;
             w.roads().caravans.back().due = w.calendarDays() + .01;   // (Due at once, as overdueWithNoWord's.)
             g.save();
             g.settle();                            // (The checkpoint, the day before it is overdue.)
             bool overdue = false;
             for (int day = 0; day < 3 && !overdue; ++day)
             {
                 nextMorning(w);
                 for (const auto& e : w.takeEvents())
                     overdue |= e.kind == "wagon overdue" && e.actor == trip;
             }
             const auto* c = caravan(w, trip);
             expect(overdue && c && near(c->latePushed, .05), "posted overdue a morning after the checkpoint, its nails 0.05 dearer");
             auto was = json::Value::object();
             was.add("trip", trip);
             was.add("day", std::floor(w.calendarDays()));
             was.add("p", w.society().pressure("west", "nails"));
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             auto& w = g.world();
             const auto trip = was.string("trip");
             const auto late = [&](double pushed, double p, const std::string& what) {
                 const auto* c = caravan(w, trip);
                 expect(c && c->overdue && near(c->latePushed, pushed), what + ": the wagon " +
                                                                            (c ? std::to_string(c->latePushed) : std::string("gone")));
                 if (p >= 0)
                     expect(samePrice(w.society().pressure("west", "nails"), p), what + ": west's price " +
                                                                              std::to_string(w.society().pressure("west", "nails")) +
                                                                              " for " + std::to_string(p));
             };
             late(.05, was.number("atCrash"), "after the crash it is as late as it was");
             // The clock is the record's, not the checkpoint's of a day or two before: that morning doesn't come again.
             expect(std::floor(w.calendarDays()) == was.number("day"), "the clock restarts on the trade's day, not the checkpoint's");
             run(w, 5);
             late(.05, was.number("p"), "the same morning brings no second rise");
             nextMorning(w);
             late(.1, -1, "the day after brings the next one");
         },
         [](game::Game& g, json::Value& was) { was.set("p", g.world().society().pressure("west", "nails")); }},
        // A robbed camp's bounty, posted from west's treasury before the checkpoint (staged coins in a real pot: the pot's
        // escrow source), settled the morning after it when the camp starves out. The pot and its source are both gone
        // after the crash (a source added before a checkpoint and taken away after it used to stay: third repair).
        {"scatter",
         [](game::Game& g) {
             auto& w = g.world();
             w.tick(.6);
             w.tick(.6);
             strongCamps(w);
             bool robbed = false;
             for (int attempt = 0; attempt < 10 && !robbed; ++attempt)
             {
                 w.roads().caravans.clear();
                 w.roads().camps = {{"camp_mid", id(4), 20, 100, -100, true}};
                 const auto* out = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{"nails", 10}});
                 expect(out, "a wagon sets out");
                 const auto mine = out->id;
                 w.roads().caravans.back().guards = 0;
                 bool passed = false;
                 for (int t = 0; t < 1500 && !robbed && !passed; ++t)
                 {
                     w.tick(1);
                     for (const auto& e : w.takeEvents())
                     {
                         robbed = robbed || (e.kind == "raid" && e.actor == "camp_mid");
                         passed |= e.kind == "caravan passes" && e.actor == mine;
                     }
                 }
             }
             std::string pot;
             for (const auto& k : w.roads().contracts)
                 if (k.kind == "bounty" && k.target == "camp_mid" && k.status == "open")
                     pot = "contract:" + k.id;
             const auto& sources = w.society().state().staged.escrowSource;
             expect(robbed && !pot.empty() && w.society().account(pot) && sources.count(pot) && sources.at(pot) > 0,
                    "a wagon robbed, a bounty on the camp, its pot's coins from the staged side");
             w.roads().camps[0].strength = .9;
             w.roads().camps[0].hunger = 100;
             g.save();
             g.settle();                            // (The checkpoint: the bounty and its pot in it.)
             nextMorning(w);
             expect(!w.roads().camps[0].active && bountiesOn(w, "camp_mid") == 0 && !w.society().account(pot) && !sources.count(pot),
                    "the camp starves out the morning after the checkpoint, its bounty settled and its pot closed");
             auto was = json::Value::object();
             was.add("pot", pot);
             return was;
         },
         [](game::Game& g, const json::Value& was) {
             auto& w = g.world();
             const auto pot = was.string("pot");
             expect(bountiesOn(w, "camp_mid") == 0 && !w.society().account(pot), "after the crash no bounty on the camp that is gone, nor its pot");
             expect(!w.society().state().staged.escrowSource.count(pot), "and no staged source kept for a pot that is gone");
         }},
    };
    return cases;
}

// Each case twice: with its trade straight after the checkpoint, and ("<case>_window") with a snapshot between them that
// is taken and does not land, so the store still holds the checkpoint before it when the game goes (third repair: the
// journal's records after a snapshot's capture used to be changes from the snapshot, which the store didn't hold).
struct CrashRun
{
    const CrashCase* c;
    bool window;
    std::string name;
};
const std::vector<CrashRun>& crashRuns()
{
    static const std::vector<CrashRun> runs = [] {
        std::vector<CrashRun> all;
        for (const bool window : {false, true})
            for (const auto& c : crashCases())
                all.push_back({&c, window, std::string(c.name) + (window ? "_window" : "")});
        return all;
    }();
    return runs;
}

std::string crashSave(const std::string& name)
{
    return (std::filesystem::temp_directory_path() / ("ratw-s2stage-crash-" + name + "-" + std::to_string(::getpid()) + ".json")).string();
}
void cleanSave(const std::string& path)
{
    for (const auto* end : {"", ".journal", ".art", ".lock", ".reports", ".snapshot"})
        std::filesystem::remove_all(path + end);
}

// The window between a snapshot's capture and its landing, held open: the tick's snapshot is really forked, and its
// child cannot write its file, so the save stays the checkpoint before it.
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

// A purchase at a shop: coins for goods, and the Board's price moved by it (an economy record that changes the Board).
// Before the shops open (their keepers asleep) the Board is moved as a purchase moves it instead, for the next trade's
// record to carry.
void buysAtAShop(game::Game& g, Client& c, const std::string& name)
{
    auto& w = g.world();
    const auto cash = [&] {
        const auto* a = w.society().account(c.entityId);
        return a ? a->cash : 0;
    };
    const auto before = cash();
    std::string said;
    for (const char* keeper : {"em", "es", "wg", "wm"})
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
    if (cash() == before && said.find("sleeping") != std::string::npos)
    {
        const double p = w.society().pressure("east", "meal");
        w.society().setPressure("east", "meal", p + .02, "players");
        expect(std::abs(w.society().pressure("east", "meal") - p - .02) < 1e-6, name + ": the shops shut, the Board moved as a purchase moves it");
        return;
    }
    expect(cash() < before, name + ": a purchase at a shop: " + said);
}

// A bounty open on a board, taken there (a roads record; its pot is an account opened before the snapshot). False when
// no bounty is open.
bool takesABounty(game::Game& g, Client& c, const std::string& name)
{
    auto& w = g.world();
    std::vector<std::pair<std::string, std::string>> open;
    for (const auto& k : w.roads().contracts)
        if (k.kind == "bounty" && k.status == "open")
            open.push_back({k.id, k.town});
    for (const auto& [k, town] : open)
    {
        const auto* spot = w.boardSpot(town);
        auto* me = w.entity(c.entityId);
        expect(spot && me, name + ": " + town + " has a board");
        me->cellId = spot->cell;
        me->position = {spot->x + .5, spot->y};
        c.events.clear();
        g.command(&c, cmd({{"type", "action"}, {"action", "take " + k}}));
        g.settle();
        expect(c.said().find("You take on") != std::string::npos, name + ": the bounty taken at " + town + "'s board: " + c.said());
        return true;
    }
    return false;
}

// What holds after any crash: work still open or taken has its reward in its pot, and no staged source is kept for a pot
// that is gone.
void soundAfterACrash(const World& w, const std::string& name)
{
    for (const auto& k : w.roads().contracts)
        if ((k.status == "open" || k.status == "taken") && k.reward > 0)
        {
            const auto* pot = w.society().account("contract:" + k.id);
            expect(pot && pot->cash == k.reward, name + ": the " + k.kind + " " + k.id + " (" + k.status + ") has its reward in its pot: " +
                                                     (pot ? std::to_string(pot->cash) : std::string("no pot")) + " of " + std::to_string(k.reward));
        }
    for (const auto& [pot, coins] : w.society().state().staged.escrowSource)
        expect(w.society().account(pot) != nullptr, name + ": a staged source kept for a pot that is gone: " + pot);
}

// To the crash: the case's own play, then (a window run) a snapshot that doesn't land, a purchase at a shop and a bounty
// taken where one is open, then a player's trade (a penny for a notice: an economy record), then the game goes without
// saving.
json::Value toTheCrash(const CrashRun& r, const std::filesystem::path& root, const std::string& path)
{
    const auto& c = *r.c;
    std::string log;
    auto o = optionsOn(root, path);
    o.forkSnapshots = r.window;
    game::Game g(o);
    g.log = [&log](const char*, const std::string& text) { log += text + "\n"; };
    std::string problem;
    expect(g.start(problem), r.name + ": a game starts on the strip with a file save: " + problem);
    // (A window run's player is in before the case's checkpoint: nothing is journalled between what the world's tick does
    // after it and the snapshot that doesn't land.)
    Client ash;
    if (r.window)
        enterAt(g, ash, 81, "ash", "east");
    auto was = c.before(g);
    if (!r.window)
        enterAt(g, ash, 81, "ash", "east");
    if (r.window)
    {
        aSnapshotDoesNotLand(g, path, log, r.name);
        buysAtAShop(g, ash, r.name);
        const bool took = takesABounty(g, ash, r.name);
        expect(took || std::string(c.name) != "raid", r.name + ": the bounty on the camp is there to take");
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
    if (c.atCrash)
        c.atCrash(g, was);
    was.add("atCrash", g.world().society().pressure("west", "nails"));   // (West's nails as the last record has them.)
    return was;
}

void afterTheCrash(const CrashRun& r, const std::filesystem::path& root, const std::string& path, const json::Value& was)
{
    game::Game g(optionsOn(root, path));
    std::string problem;
    expect(g.start(problem), r.name + ": starts again after the crash: " + problem);
    soundAfterACrash(g.world(), r.name);
    r.c->after(g, was);
    soundAfterACrash(g.world(), r.name);
}

void departuresSurviveACrash()
{
    for (const auto& r : crashRuns())
    {
        auto f = strip();
        const auto root = onDisk(f, "crash-" + r.name);
        const auto path = crashSave(r.name);
        cleanSave(path);
        const auto was = toTheCrash(r, root, path);
        afterTheCrash(r, root, path, was);
        cleanSave(path);
        std::filesystem::remove_all(root);
    }
}

// A snapshot the store has been handed and has not stored yet (a database's, written by its worker; or one it never
// stores): the same window without a fork. A wagon comes in after the last checkpoint that landed; the next snapshot is
// taken and stays in the store's hands; a purchase; the game goes. On what the store holds, the wagon came in once.
struct Held
{
    std::string payload;                            // The last checkpoint that landed.
    std::uint64_t stored = 0;
    int waiting = 0;                                // Snapshots handed over and not stored.
    std::shared_ptr<std::vector<journal::Record>> records = std::make_shared<std::vector<journal::Record>>();
};
struct HeldStore final : game::Store
{
    std::shared_ptr<Held> held;
    std::unique_ptr<journal::Writer> writer;
    explicit HeldStore(std::shared_ptr<Held> h) : held(std::move(h)), writer(journal::memoryWriter(held->records)) {}
    bool database() const override { return false; }
    std::string load() override { return held->payload; }
    bool save(DbStore::Build build, std::uint64_t revision) override
    {
        json::Value document;
        std::string npcStates;
        build(document, npcStates);
        if (document.isObject())
            held->payload = json::dump(document);
        held->stored = revision;
        return true;
    }
    bool saveInBackground(DbStore::Build, std::uint64_t) override
    {
        ++held->waiting;
        return true;
    }
    bool flush() override { return true; }
    void queueEvents(std::vector<WorldEvent>) override {}
    std::vector<std::pair<std::string, std::string>> externalNpcStates() override { return {}; }
    std::string error() const override { return {}; }
    journal::Writer* journal() override { return writer.get(); }
    bool journalAfter(std::uint64_t after, std::vector<journal::Record>& out, std::string&) override
    {
        for (const auto& r : *held->records)
            if (r.seq > after)
                out.push_back(r);
        return true;
    }
    std::uint64_t storedRevision() const override { return held->stored; }
};

void aSnapshotStillInTheStoresHands()
{
    const CrashCase* arrive = nullptr;
    for (const auto& c : crashCases())
        if (std::string(c.name) == "arrive")
            arrive = &c;
    expect(arrive != nullptr, "the arrival's case");
    auto f = strip();
    const auto root = onDisk(f, "held");
    auto held = std::make_shared<Held>();
    json::Value was;
    {
        game::Game g(optionsOn(root));
        g.useStore(std::make_unique<HeldStore>(held));
        std::string problem;
        expect(g.start(problem), "a game starts on a store that holds its snapshots: " + problem);
        Client ash;                                 // (In before the checkpoint: nothing is journalled after the arrival.)
        enterAt(g, ash, 81, "ash", "east");
        g.settle();
        was = arrive->before(g);                    // (Its checkpoint lands: a save() is stored before it returns.)
        const auto landed = held->payload;
        expect(!landed.empty() && held->waiting == 0, "the checkpoint before the arrival is in the store");
        for (int t = 0; t < 45 && held->waiting == 0; ++t)
            g.tick(1);
        expect(held->waiting > 0 && held->payload == landed, "the tick's snapshot is handed to the store, which hasn't stored it");
        buysAtAShop(g, ash, "held");
        g.disconnect(&ash);
        was.add("atCrash", g.world().society().pressure("west", "nails"));
        expect(held->payload == landed && !held->records->empty(), "the game goes: the store holds the older checkpoint and the journal since");
    }
    {
        game::Game g(optionsOn(root));
        g.useStore(std::make_unique<HeldStore>(held));
        std::string problem;
        expect(g.start(problem), "starts again on what the store holds: " + problem);
        soundAfterACrash(g.world(), "held");
        arrive->after(g, was);
    }
    std::filesystem::remove_all(root);
}

// The carters' pinned notices say whether an escort is wanted as things stand (third repair): asked for after the pin
// (a raid on that road), the notices ask as well; the wagon out, nobody can join it and they stop asking.
void pinnedNoticesFollowTheEscort()
{
    auto f = strip();
    const auto root = onDisk(f, "pinned");
    {
        game::Game g(optionsOn(root));
        std::string problem;
        expect(g.start(problem), "a game starts on the strip: " + problem);
        auto& w = g.world();
        // No camp on the road (a spent one kept, so that none is seeded in its place): no escort is asked for.
        const auto quiet = [&] { w.roads().camps = {{"camp_mid", id(4), 0, 0, -100, false}}; };
        quiet();
        w.tick(.6);
        w.tick(.6);
        market::Departure d;
        for (int day = 0; day < 14 && d.id.empty(); ++day)
        {
            quiet();
            if (const auto* posted = firstWagon(w))
                d = *posted;
            else
                nextMorning(w);
        }
        quiet();
        expect(!d.id.empty(), "a wagon is posted within two weeks");
        expect(std::none_of(w.roads().contracts.begin(), w.roads().contracts.end(), [](const Contract& k) { return k.kind == "escort"; }),
               "no camp on the road: no escort asked for");
        const auto pinned = [&](bool atEnd) {
            const auto* doc = g.documents().find("wagon-" + d.id + (atEnd ? "-to" : "-from"));
            return doc ? doc->text : std::string("none");
        };
        const auto asks = [&](bool atEnd) { return pinned(atEnd).find("Escort wanted") != std::string::npos; };
        expect(pinned(false) != "none" && pinned(true) != "none" && !asks(false) && !asks(true), "no camp on its road: its notices ask for no escort: " + pinned(false));
        const auto plain = pinned(false), plainAtEnd = pinned(true);
        const auto escort = w.postContract("escort", w.society().treasuryOf(d.to), d.to, d.to, 12, 7,
                                           "guarding the next wagon from East to West", d.from).id;
        expect(pinned(false).find(" Escort wanted.") != std::string::npos &&
                   pinned(true).find(" Escort wanted: ask at the board in East.") != std::string::npos,
               "an escort asked for after the pin: the notices ask as well: " + pinned(false) + " / " + pinned(true));
        const auto* out = setOut(w, d);
        expect(out != nullptr, "it sets out");
        bool open = false;
        for (const auto& k : w.roads().contracts)
            open = open || (k.id == escort && k.status == "open");
        expect(open && pinned(false) == plain && pinned(true) == plainAtEnd,
               "the wagon out with the work still open: its notices stop asking, the rest as it was: " + pinned(false) + " / " + pinned(true));
    }
    std::filesystem::remove_all(root);
}

// An escort is one road's (second repair): west asks for guards for the wagon from east, a camp on its road. A wagon from
// north to west is no business of theirs: its notices don't ask, north's board doesn't offer the work, north's guard
// doesn't go for it, and when north's wagon is out first it leaves without the one who took the work at east's board,
// who goes with east's (once: not with the wagon after it as well). The road is kept through a save.
void anEscortIsItsRoads()
{
    auto f = strip("EE..WW..NN", resident("ng", "Hale Ward", "guard", "keeping the watch", 8, 5.5, 9, 6.5));   // (West between.)
    const auto root = onDisk(f, "escort");
    const auto path = crashSave("escort");
    cleanSave(path);
    std::string escort, ash;
    const auto status = [&](const World& w) {
        for (const auto& k : w.roads().contracts)
            if (k.id == escort)
                return k.status + (k.taker.empty() ? "" : " by " + k.taker);
        return std::string("gone");
    };
    const auto offered = [&](const World& w, const std::string& who) {
        bool listed = false;
        for (const auto* k : w.contractsNear(who))
            listed = listed || k->id == escort;
        return listed;
    };
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "a game starts on the strip, west between east and north: " + problem);
        auto& w = g.world();
        w.tick(.6);
        w.tick(.6);
        market::Departure d;
        bool found = false;
        for (int day = 0; day < 60 && !found; ++day)
        {
            w.roads().camps = {{"camp_mid", id(2), 3, 0, -100, true}};   // (On the road from east to west.)
            w.roads().caravans.clear();             // (East's two carters kept free for it: no wagon out to north.)
            for (const auto& x : w.society().board().committed)
                if (!found && x.kind == "wagon" && x.from == "east" && x.to == "west")
                    d = x, found = true;
            if (!found)
                nextMorning(w);
        }
        expect(found, "a wagon from east to west is posted, a camp on its road");
        w.roads().camps.clear();
        for (const auto& k : w.roads().contracts)
            if (k.kind == "escort" && k.town == "west" && k.status == "open")
                escort = k.id;
        expect(!escort.empty(), "and west asks for an escort for it");
        const auto pinned = [&](bool atEnd) {
            const auto* doc = g.documents().find("wagon-" + d.id + (atEnd ? "-to" : "-from"));
            return doc ? doc->text : std::string("none");
        };
        expect(pinned(false).find(" Escort wanted.") != std::string::npos &&
                   pinned(true).find(" Escort wanted: ask at the board in East.") != std::string::npos,
               "its pinned notices ask for one: " + pinned(false) + " / " + pinned(true));
        // A wagon from north to west posted as well (by hand: north has no carter of its own).
        auto n = d;
        n.id = "wnorth";
        n.from = "north";
        n.driver.clear();
        w.society().stagedBoard().committed.push_back(n);
        expect(w.departureNotice(d, false).find("Escort wanted.") != std::string::npos &&
                   w.departureNotice(d, true).find("Escort wanted: ask at the board in East.") != std::string::npos,
               "the notices of east's wagon ask for it: " + w.departureNotice(d, false));
        expect(w.departureNotice(n, false).find("Escort wanted") == std::string::npos &&
                   w.departureNotice(n, true).find("Escort wanted") == std::string::npos,
               "the notices of north's wagon don't: " + w.departureNotice(n, false));
        Client bo, ashes;
        enterAt(g, bo, 91, "bo", "north");
        enterAt(g, ashes, 92, "ash", "east");
        ash = ashes.entityId;
        expect(!offered(w, bo.entityId), "north's board doesn't offer the work");
        expect(offered(w, ash), "east's board does");
        // Nobody has taken it in three days: north's guard doesn't go with north's wagon for it.
        for (auto& k : w.roads().contracts)
            if (k.id == escort)
                k.created -= 3;
        const auto* early = w.sendCaravan(*w.roadMarket("north"), *w.roadMarket("west"), {{"nails", 5}});
        expect(early && early->escorts.empty() && status(w) == "open", "north's guard doesn't take the escort of east's road: " + status(w));
        w.roads().caravans.clear();
        for (auto& k : w.roads().contracts)
            if (k.id == escort)
                k.created += 3;
        expect(w.takeContract(ash, escort).ok, "taken at east's board");
        expect(pinned(false) != "none" && pinned(false).find("Escort wanted") == std::string::npos &&
                   pinned(true).find("Escort wanted") == std::string::npos,
               "and the pinned notices stop asking for an escort it has (third repair): " + pinned(false) + " / " + pinned(true));
        g.save();
        g.settle();
        g.disconnect(&bo);
        g.disconnect(&ashes);
    }
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "and after a save and a restart: " + problem);
        auto& w = g.world();
        w.roads().camps.clear();
        expect(status(w) == "taken by " + ash, "the work is still hers: " + status(w));
        const auto* north = w.sendCaravan(*w.roadMarket("north"), *w.roadMarket("west"), {{"nails", 5}});
        expect(north && north->escorts.empty(), "north's wagon, out first, leaves without her");
        const auto* east = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{"nails", 5}});
        expect(east && east->escorts == std::vector<std::string>{ash}, "east's wagon waits for her");
        expect(status(w) == "taken by " + ash, "her work as it was: " + status(w));
        const auto* next = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{"nails", 5}});
        expect(next && next->escorts.empty(), "and the wagon after it doesn't wait for her as well");
    }
    cleanSave(path);
    std::filesystem::remove_all(root);
}

// --write-crashes DIR: each case played to its crash; its checkpoint (DIR/<case>.json), its journal (DIR/<case>.journal,
// a record a line: its number, a tab, its text) and what was so at the crash (DIR/<case>.was.json), for
// Tests/trade_show_pg.py to put through game.save_checkpoint and game.journal in a throwaway database and read back.
// An escort paid on a world tick as its wagon comes in, told so, and then the game gone without saving (the repair after
// Phases 4 and 5 merged): what a world tick told a player is journalled before they are told, so the restart, with her
// away from the world, has her paid and the escort done (it used to put the wagon back on the road, find her gone at its
// arrival and send the pay back).
void anEscortsArrivalPayThroughACrash()
{
    auto f = strip();
    const auto root = onDisk(f, "arrival-pay");
    const auto path = crashSave("arrival-pay");
    cleanSave(path);
    std::string escort, player;
    std::int64_t crashed = -1;
    {
        Client ash;                                 // (Still in the game at the crash: the client outlives it.)
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "a game starts for the arrival's pay: " + problem);
        enterAt(g, ash, 81, "ash", "west");
        g.settle();
        auto& w = g.world();
        auto& s = w.society();
        player = ash.entityId;
        w.tick(.6);
        expect(w.setTimeOfDay(8).ok, "a morning for the arrival's pay");
        w.roads().camps.clear();
        w.roads().caravans.clear();
        escort = w.postContract("escort", s.treasuryOf("west"), "west", "west", 12, 7, "guarding the next wagon from East to West", "east").id;
        expect(w.takeContract(player, escort).ok, "ash takes the escort");
        auto* me = w.entity(player);
        me->cellId = id(1);
        me->position = {8.5, 10.5};
        const auto* sent = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{"nails", 5}});
        expect(sent && sent->escorts == std::vector<std::string>{player}, "the wagon counts ash among its escorts");
        const auto trip = sent->id;
        g.save();
        g.settle();                                 // (The checkpoint: the escort taken, its pot full, the wagon out.)
        const auto purse = s.cash(player);
        bool arrived = false;
        for (int t = 0; t < 1500 && !arrived; ++t)
        {
            if (const auto* wagon = w.entity("road:" + trip))
            {
                auto* p = w.entity(player);
                p->cellId = wagon->cellId;
                p->position = {std::max(.6, wagon->position.x - 1), wagon->position.y};
            }
            w.tick(1);
            for (const auto& e : w.takeEvents())
                arrived = arrived || (e.kind == "caravan arrives" && e.actor == trip);
        }
        expect(arrived, "the wagon reaches west with ash");
        ash.events.clear();
        g.tick(.05);                                // (The game's tick tells her what the world's did.)
        g.settle();
        expect(ash.said().find("You are paid 12 pennies for guarding it.") != std::string::npos, "ash is told she is paid: " + ash.said());
        crashed = s.cash(player);
        expect(crashed == purse + 12, "and she is: " + std::to_string(crashed - purse));
    }                                               // (Gone without saving.)
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "starts again after the arrival's crash: " + problem);
        auto& w = g.world();
        for (int t = 0; t < 1800; ++t)
            w.tick(1);
        std::string status = "settled";
        for (const auto& k : w.roads().contracts)
            if (k.id == escort)
                status = k.status;
        expect(w.society().cash(player) == crashed,
               "the pay she was told of is hers after the crash: " + std::to_string(w.society().cash(player)) + " of " + std::to_string(crashed) +
                   " (the escort " + status + ")");
        expect(status == "done" || status == "settled", "the escort is done, not expired: " + status);
        expect(w.society().realBalanced(), "real money is balanced after the arrival's crash");
    }
    cleanSave(path);
    std::filesystem::remove_all(root);
}

// The boards' notices through a restart and a crash (the repair after Phases 4 and 5 merged; every notice was dropped at
// every load, since a notice is to no one). A player's paid notice is there after a clean restart. A wagon's carters'
// notices, pinned before the checkpoint, are there after a crash too, and say what the Board says by the journal: the
// escort taken after the checkpoint is not asked for again.
void noticesSurviveARestart()
{
    auto f = strip();
    const auto root = onDisk(f, "notices-restart");
    const auto path = crashSave("notices-restart");
    cleanSave(path);
    std::string mine;
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "a game starts for the notices: " + problem);
        Client ash;
        enterAt(g, ash, 81, "ash", "east");
        board(g, ash, {{"verb", "post"}, {"kind", "other"}, {"text", "Lost: a blue scarf, near the well."}});
        g.settle();
        for (const auto& [id, d] : g.documents().all())
            if (d.kind == "notice" && d.author == ash.entityId)
                mine = id;
        expect(!mine.empty(), "ash's notice is up: " + ash.said());
        g.save();
        g.settle();
        g.disconnect(&ash);
    }
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "starts again for the notices: " + problem);
        const auto* d = g.documents().find(mine);
        expect(d && d->text == "Lost: a blue scarf, near the well." && d->board == "board:east", "a player's notice is up after a restart");
    }
    cleanSave(path);
    market::Departure d;
    std::string escort;
    {
        Client ash;                                 // (Still in the game at the crash: the client outlives it.)
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "a game starts for the wagon's notices: " + problem);
        enterAt(g, ash, 82, "ash", "east");
        g.settle();
        auto& w = g.world();
        const auto quiet = [&] { w.roads().camps = {{"camp_mid", id(4), 0, 0, -100, false}}; };
        quiet();
        w.tick(.6);
        w.tick(.6);
        for (int day = 0; day < 14 && d.id.empty(); ++day)
        {
            quiet();
            if (const auto* posted = firstWagon(w))
                d = *posted;
            else
                nextMorning(w);
        }
        quiet();
        expect(!d.id.empty(), "a wagon is posted for the notices");
        escort = w.postContract("escort", w.society().treasuryOf(d.to), d.to, d.to, 12, 7, "guarding the next wagon from East to West", d.from).id;
        const auto* asking = g.documents().find("wagon-" + d.id + "-from");
        expect(asking && asking->text.find("Escort wanted") != std::string::npos, "its notice asks for an escort");
        g.save();
        g.settle();                                 // (The checkpoint: the escort open, the notices asking.)
        const auto* spot = w.boardSpot(d.from);
        auto* me = w.entity(ash.entityId);
        me->cellId = spot->cell;
        me->position = {spot->x + .5, spot->y};
        ash.events.clear();
        g.command(&ash, cmd({{"type", "action"}, {"action", "take " + escort}}));
        g.settle();
        expect(ash.said().find("You take on") != std::string::npos, "ash takes the escort: " + ash.said());
    }                                               // (Gone without saving.)
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "starts again after the notices' crash: " + problem);
        auto& w = g.world();
        bool taken = false;
        for (const auto& k : w.roads().contracts)
            taken = taken || (k.id == escort && k.status == "taken");
        expect(taken, "the escort is still taken after the crash");
        const auto* from = g.documents().find("wagon-" + d.id + "-from");
        const auto* to = g.documents().find("wagon-" + d.id + "-to");
        expect(from && to, "the wagon's notices are up after the crash");
        expect(from->text.find("Escort wanted") == std::string::npos && to->text.find("Escort wanted") == std::string::npos,
               "and do not ask for the escort that is taken: " + from->text + " / " + to->text);
        expect(from->text == w.departureNotice(*firstWagon(w), false), "the notice says what the Board says: " + from->text);
    }
    cleanSave(path);
    std::filesystem::remove_all(root);
}

// ------------------------------------------------------------------ The repair's check (after doc 79's Phases 4 and 5)

// A notice pinned after the checkpoint and the penny it cost go through a crash together, and one taken down stays down
// (the boards are journalled whole with the economy: Game::record). Before, the notice was lost and its penny kept, and
// the one taken down came back.
void aNoticeAndItsPennyThroughACrash()
{
    auto f = strip();
    const auto root = onDisk(f, "notice-penny");
    const auto path = crashSave("notice-penny");
    cleanSave(path);
    std::string early, late;
    std::int64_t cash = 0;
    Client ash;                                     // (In the game at the crash: the client outlives it.)
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "a game starts for the notice's penny: " + problem);
        enterAt(g, ash, 81, "ash", "east");
        board(g, ash, {{"verb", "post"}, {"kind", "other"}, {"text", "Wanted: a lantern, before the checkpoint."}});
        g.settle();
        for (const auto& [id, d] : g.documents().all())
            if (d.kind == "notice" && d.author == ash.entityId)
                early = id;
        g.save();
        g.settle();                                 // (The checkpoint: the first notice up.)
        board(g, ash, {{"verb", "post"}, {"kind", "other"}, {"text", "Found: a blue scarf, after the checkpoint."}});
        g.settle();
        for (const auto& [id, d] : g.documents().all())
            if (d.kind == "notice" && d.author == ash.entityId && id != early)
                late = id;
        board(g, ash, {{"verb", "unpost"}, {"id", early}});
        g.settle();                                 // (Its reply waits for its record.)
        const auto down = ash.said();
        expect(!early.empty() && !late.empty() && !g.documents().find(early) && down.find("take your notice down") != std::string::npos,
               "one notice pinned and one taken down after the checkpoint: " + down);
        cash = g.world().society().cash(ash.entityId);
    }                                               // (Gone without saving.)
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "starts again after the notice's crash: " + problem);
        const auto* d = g.documents().find(late);
        expect(d && d->text == "Found: a blue scarf, after the checkpoint." && d->board == "board:east",
               "the notice pinned after the checkpoint is up after the crash");
        expect(!g.documents().find(early), "the one taken down after the checkpoint stays down");
        expect(g.world().society().cash(ash.entityId) == cash, "the penny it cost paid once: " +
                                                                   std::to_string(g.world().society().cash(ash.entityId)) + " for " +
                                                                   std::to_string(cash));
        expect(g.world().society().realBalanced(), "real money balanced after the notice's crash");
    }
    cleanSave(path);
    std::filesystem::remove_all(root);
}

// A wagon robbed after the checkpoint, the raid in the journal alone: at the start its far end's notice says it was
// robbed, as the raid's event would have made it, and comes down within two days (it went on promising the wagon).
void aRobbedWagonsNoticeAfterACrash()
{
    auto f = strip();
    const auto root = onDisk(f, "robbed-notice");
    const auto path = crashSave("robbed-notice");
    cleanSave(path);
    market::Departure d;
    std::string far;
    Client ash;
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "a game starts for the robbed wagon's notice: " + problem);
        enterAt(g, ash, 83, "ash", "east");
        g.settle();
        auto& w = g.world();
        const auto quiet = [&] { w.roads().camps = {{"camp_mid", id(4), 0, 0, -100, false}}; };
        quiet();
        w.tick(.6);
        w.tick(.6);
        for (int day = 0; day < 14 && d.id.empty(); ++day)
        {
            quiet();
            if (const auto* posted = firstWagon(w))
                d = *posted;
            else
                nextMorning(w);
        }
        quiet();
        expect(!d.id.empty(), "a wagon is posted for the robbed notice");
        expect(setOut(w, d) != nullptr, "it sets out");
        g.tick(.05);
        g.save();
        g.settle();                                 // (The checkpoint: the wagon on the road, its notices up.)
        far = "wagon-" + d.id + "-to";
        expect(g.documents().find(far) != nullptr, "its far end's notice is up");
        for (auto& c : w.roads().caravans)
            if (c.departure == d.id)
            {
                c.status = "raided";               // (The raid, its notice's change not yet made.)
                c.manifest.clear();
            }
        board(g, ash, {{"verb", "post"}, {"kind", "other"}, {"text", "A trade, journalled with the roads."}});
        g.settle();
    }                                               // (Gone without saving.)
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "starts again after the raid's crash: " + problem);
        bool raided = false;
        for (const auto& c : g.world().roads().caravans)
            raided = raided || (c.departure == d.id && c.status == "raided");
        expect(raided, "the wagon is robbed by the journal");
        const auto* n = g.documents().find(far);
        expect(n && n->text.rfind("Robbed on the road: ", 0) == 0, "its far end's notice says it was robbed: " + (n ? n->text : std::string("none")));
        expect(n && n->expires <= g.world().calendarDays() + 2 + 1e-9, "and comes down within two days");
        expect(!g.documents().find("wagon-" + d.id + "-from"), "its near end's is down");
    }
    cleanSave(path);
    std::filesystem::remove_all(root);
}

// A store whose journal commits only when let (a slow database): a notice held for its record, then a later tick's
// notice with nothing moved, reach the player in their order, never the later first (the repair's check: it went out at
// once, ahead of the one still waiting).
struct GatedStore final : game::Store
{
    std::string saved;
    std::shared_ptr<std::atomic<bool>> open = std::make_shared<std::atomic<bool>>(true);
    std::shared_ptr<std::vector<journal::Record>> records = std::make_shared<std::vector<journal::Record>>();
    std::unique_ptr<journal::Writer> writer;
    GatedStore()
    {
        auto gate = open;
        auto into = records;
        writer = std::make_unique<journal::Writer>(
            [gate, into](const std::vector<journal::Record>& batch, std::string& error) {
                if (!*gate)
                {
                    error = "held";
                    return false;                   // (Tried again after a moment, as a database away is.)
                }
                into->insert(into->end(), batch.begin(), batch.end());
                return true;
            },
            nullptr, true);
    }
    bool database() const override { return false; }
    std::string load() override { return saved; }
    bool save(DbStore::Build build, std::uint64_t) override
    {
        json::Value document;
        std::string states;
        build(document, states);
        saved = json::dump(document);
        return true;
    }
    bool saveInBackground(DbStore::Build build, std::uint64_t revision) override { return save(std::move(build), revision); }
    bool flush() override { return true; }
    void queueEvents(std::vector<WorldEvent>) override {}
    std::vector<std::pair<std::string, std::string>> externalNpcStates() override { return {}; }
    std::string error() const override { return {}; }
    journal::Writer* journal() override { return writer.get(); }
    bool journalAfter(std::uint64_t after, std::vector<journal::Record>& out, std::string&) override
    {
        for (const auto& r : *records)
            if (r.seq > after)
                out.push_back(r);
        return true;
    }
};

void aLaterNoticeWaitsItsTurn()
{
    auto f = strip();
    const auto root = onDisk(f, "notice-order");
    game::Game g(optionsOn(root));
    auto store = std::make_unique<GatedStore>();
    auto gate = store->open;
    g.useStore(std::move(store));
    std::string problem;
    expect(g.start(problem), "a game starts on a store that is slow to commit: " + problem);
    Client ash;
    enterAt(g, ash, 84, "ash", "west");
    g.settle();
    auto& w = g.world();
    const auto order = [&] {
        std::string out;
        for (const auto& e : ash.events)
            if (e.string("type") == "system")
                out += e.string("text").rfind("FIRST", 0) == 0 ? "F" : e.string("text").rfind("SECOND", 0) == 0 ? "S" : "";
        return out;
    };
    ash.events.clear();
    *gate = false;                                  // (The database slow from here.)
    w.society().shift(w.society().treasuryOf("west"), ash.entityId, "", 0, 1, "an escort's pay");
    w.noticeFor(ash.entityId, "FIRST: paid on the tick.", true);
    g.tick(.05);
    w.noticeFor(ash.entityId, "SECOND: a later tick's line, nothing moved.");
    g.tick(.05);
    expect(order().empty(), "while the first waits for its record, the later one waits behind it: " + order());
    *gate = true;
    for (int i = 0; i < 200 && order().size() < 2; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));   // (The writer tries again every half second.)
        g.settle();
    }
    expect(order() == "FS", "then both, in their order: " + order());
    std::filesystem::remove_all(root);
}

// A tick's notice journals the economy first only when it speaks of something moved (an escort's pay, a bounty): a fight's
// line and the like are told as they come, with no record (about half a millisecond each on DEV: the repair's check).
void aFightLineIsNotJournalled()
{
    auto f = strip();
    const auto root = onDisk(f, "notice-cost");
    auto held = std::make_shared<Held>();
    game::Game g(optionsOn(root));
    g.useStore(std::make_unique<HeldStore>(held));
    std::string problem;
    expect(g.start(problem), "a game starts for the notices' records: " + problem);
    Client ash;
    enterAt(g, ash, 85, "ash", "west");
    g.settle();
    auto& w = g.world();
    for (int t = 0; t < 4; ++t)
        g.tick(.05);
    g.settle();
    const auto records = [&] { return held->records->size(); };
    auto before = records();
    w.society().shift(w.society().treasuryOf("west"), ash.entityId, "", 0, 1, "a coin found");
    w.noticeFor(ash.entityId, "A blow lands.");
    g.tick(.05);
    g.settle();
    expect(records() == before, "a line that moved nothing writes no record: " + std::to_string(records() - before));
    expect(ash.said().find("A blow lands.") != std::string::npos, "and is told");
    before = records();
    w.noticeFor(ash.entityId, "You are paid 1p for guarding it.", true);
    g.tick(.05);
    g.settle();
    expect(records() == before + 1, "one that speaks of coins moved writes one first: " + std::to_string(records() - before));
    std::filesystem::remove_all(root);
}

// Residents asked for and not yet made (a fresh world's first staffing, made ResidentsAPass a tick) are made before a
// checkpoint: the requests are in none, so a crash after it lost them till the next day's careers pass (a database
// world's arrivals: their rows are placed as they are made). And each is given a tile of its own, the same however many
// a pass makes (206 found in one pass gave some the same tile).
void residentsWaitingAtACheckpoint()
{
    const int asked = int(World::ResidentsAPass) + 4;
    const auto ask = [&](World& w) {
        for (int n = 0; n < asked; ++n)
        {
            ResidentRequest r;
            r.kind = "birth";
            r.templateId = "e1";
            r.name = "Pup " + std::to_string(n);
            r.home = {id(0), 9.5, 4.5};
            r.sex = n % 2 ? "male" : "female";
            w.society().askFor(r);
        }
    };
    const auto tiles = [&](const World& w) {
        std::vector<std::pair<int, int>> out;
        for (const auto& [id, life] : w.society().state().residents)
            if (id.rfind("born_", 0) == 0)
                out.push_back({int(std::floor(life.homeX)), int(std::floor(life.homeY))});
        std::sort(out.begin(), out.end());
        return out;
    };
    auto f = strip();
    auto all = load(f);
    ask(all);
    all.makeRequestedResidents(nullptr, std::size_t(-1));
    auto byOne = load(f);
    ask(byOne);
    while (byOne.society().requestsWaiting() > 0)
        byOne.makeRequestedResidents(nullptr, 1);
    const auto mine = tiles(all);
    expect(int(mine.size()) == asked, "all of them made: " + std::to_string(mine.size()));
    expect(std::adjacent_find(mine.begin(), mine.end()) == mine.end(), "each on a tile of its own");
    expect(mine == tiles(byOne), "the same tiles made one a pass as all at once");
    const auto root = onDisk(f, "waiting-residents");
    const auto path = crashSave("waiting-residents");
    cleanSave(path);
    {
        game::Game g(optionsOn(root, path));
        std::string problem;
        expect(g.start(problem), "a game starts for the waiting residents: " + problem);
        ask(g.world());
        g.save();
        g.settle();
        expect(g.world().society().requestsWaiting() == 0 && int(tiles(g.world()).size()) == asked,
               "a checkpoint makes those still waiting first: " + std::to_string(g.world().society().requestsWaiting()) + " waiting");
    }
    cleanSave(path);
    std::filesystem::remove_all(root);
}

int writeCrashes(const std::filesystem::path& dir)
{
    namespace fs = std::filesystem;
    fs::create_directories(dir);
    for (const auto& r : crashRuns())
    {
        auto f = strip();
        const auto root = onDisk(f, "crash-" + r.name);
        const auto path = crashSave(r.name);
        cleanSave(path);
        const auto was = toTheCrash(r, root, path);
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
    for (const auto& r : crashRuns())
    {
        auto f = strip();
        const auto root = onDisk(f, "crash-" + r.name);
        const auto path = crashSave(r.name);
        cleanSave(path);
        fs::copy_file(back / (r.name + ".json"), path);
        fs::copy_file(back / (r.name + ".journal"), path + ".journal");
        json::Value was;
        std::string error;
        expect(json::parse(slurp(dir / (r.name + ".was.json")), was, error), r.name + ": what was so reads: " + error);
        afterTheCrash(r, root, path, was);
        cleanSave(path);
        fs::remove_all(root);
    }
    std::cout << "trade_show_tests --check-crashes: " << crashRuns().size() << " crashes replayed the same through Postgres (" << checks
              << " checks)\n";
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--write-crashes")
            return writeCrashes(argv[2]);
        if (argc == 4 && std::string(argv[1]) == "--check-crashes")
            return checkCrashes(argv[2], argv[3]);
        wagonsCommittedWhenPosted();
        aWagonsTrip();
        anArrivalNeverRaises();
        aRaid();
        foundCargoIsAReward();
        aFullPackAtABrokenCamp();
        anEscortsPayWithinTheDay();
        anOlderEscortFindsItsRoad();
        aLateWagon();
        importsThinBetweenWagons();
        farmCarts();
        anEndpointDeleted();
        noPortNoShips();
        noCarterSetsItBack();
        banditsScaleMildly();
        noticesPostAndComeDown();
        aGlutRidesTheNextWagon();
        everyImportComesInRotation();
        rulesThatStopTheRoads();
        aPlainMarketsBoard();
        plainWords();
        overdueWithNoWord();
        cartsThatDontCome();
        departuresSurviveACrash();
        aSnapshotStillInTheStoresHands();
        anEscortIsItsRoads();
        pinnedNoticesFollowTheEscort();
        anEscortsArrivalPayThroughACrash();
        noticesSurviveARestart();
        aNoticeAndItsPennyThroughACrash();
        aRobbedWagonsNoticeAfterACrash();
        aLaterNoticeWaitsItsTurn();
        aFightLineIsNotJournalled();
        residentsWaitingAtACheckpoint();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Trade show tests passed: " << checks << " checks.\n";
    return 0;
}
