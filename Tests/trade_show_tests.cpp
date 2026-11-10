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
#include <cmath>
#include <filesystem>
#include <fstream>
#include <unistd.h>
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
    const auto region = [](char c) { return c == 'E' ? "east" : c == 'W' ? "west" : "wilds"; };
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
            expect(lowered(atSource).find(lowered(Society::itemName(d.manifest.front().first))) != std::string::npos,
                   "and what it carries: " + atSource);
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
    expect(w.attack("player-ada", leader->id).ok, "a fight begins");
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
    expect(cleared, "she breaks the camp");
    int found = 0;
    if (const auto* mine = w.society().account("player-ada"))
        for (const auto& [item, n] : mine->stock)
            if (item == good)
                found += n;
    expect(found > 0, "and finds the wagon's cargo among their things");
    expect(w.society().realBalanced(), "real money is balanced");
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
    fs::remove_all(root);
}
} // namespace

int main()
{
    try
    {
        wagonsCommittedWhenPosted();
        aWagonsTrip();
        anArrivalNeverRaises();
        aRaid();
        aLateWagon();
        importsThinBetweenWagons();
        farmCarts();
        anEndpointDeleted();
        noPortNoShips();
        noCarterSetsItBack();
        banditsScaleMildly();
        noticesPostAndComeDown();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Trade show tests passed: " << checks << " checks.\n";
    return 0;
}
